#include <string.h>
#include <vector>
#include "version.h"
#include "encode.h"

using namespace emscripten;
thread_local val Uint8Array = val::global("Uint8Array");

// Global registry for streaming encoders (opaque handle pattern)
static std::map<int, std::unique_ptr<StreamingEncoderState>> g_encoders;
static int g_nextHandle = 1;

val encoder_version()
{
	return get_version(WebPGetEncoderVersion());
}

val encodeRGB(std::string rgb, int width, int height, int quality_factor) {
	uint8_t* output;
	size_t size = WebPEncodeRGB((uint8_t*)rgb.c_str(), width, height, 3 * width, quality_factor, &output);
	if (size == 0 || output == nullptr) {
		return val::null();
	}
	val result = Uint8Array.new_(typed_memory_view(size, output));
	WebPFree(output);
	return result;
}

val encodeRGBA(std::string rgba, int width, int height, int quality_factor) {
	uint8_t* output;
	size_t size = WebPEncodeRGBA((uint8_t*)rgba.c_str(), width, height, 4 * width, quality_factor, &output);
	if (size == 0 || output == nullptr) {
		return val::null();
	}
	val result = Uint8Array.new_(typed_memory_view(size, output));
	WebPFree(output);
	return result;
}

val encode(std::string data, int width, int height, bool has_alpha, SimpleWebPConfig config)
{
	WebPConfig webp_config;
	WebPConfigInit(&webp_config);

	webp_config.quality = config.quality;
	webp_config.lossless = config.lossless;

	WebPMemoryWriter wrt;
	WebPMemoryWriterInit(&wrt);

	WebPPicture pic;
	if (!WebPPictureInit(&pic))
	{
		return val::null();
	}
	// only support argb!!!
	pic.use_argb = 1;
	pic.width = width;
	pic.height = height;
	if (!WebPPictureAlloc(&pic))
	{
		WebPPictureFree(&pic);
		return val::null();
	}
	pic.writer = WebPMemoryWrite;
	pic.custom_ptr = &wrt;
	int import_success = has_alpha
		? WebPPictureImportRGBA(&pic, (uint8_t*)data.c_str(), width * 4)
		: WebPPictureImportRGB(&pic, (uint8_t*)data.c_str(), width * 3);
	if (!import_success)
	{
		WebPPictureFree(&pic);
		WebPMemoryWriterClear(&wrt);
		return val::null();
	}
	int success = WebPEncode(&webp_config, &pic);
	if (!success)
	{
		WebPPictureFree(&pic);
		WebPMemoryWriterClear(&wrt);
		return val::null();
	}
	val encoded_data = Uint8Array.new_(typed_memory_view(wrt.size, wrt.mem));
	WebPPictureFree(&pic);
	WebPMemoryWriterClear(&wrt);
	return encoded_data;
}

// Streaming encoder API implementation

int createStreamingEncoder(int width, int height, bool has_alpha, AnimationEncoderOptions options)
{
	auto state = std::make_unique<StreamingEncoderState>();
	state->width = width;
	state->height = height;
	state->has_alpha = has_alpha;
	state->timestamp_ms = 0;
	state->loop_count = options.loop_count;

	// Init encoder options
	WebPAnimEncoderOptions enc_options;
	WebPAnimEncoderOptionsInit(&enc_options);
	enc_options.minimize_size = options.minimize_size;
	enc_options.allow_mixed = options.allow_mixed;
	if (options.kmin > 0) enc_options.kmin = options.kmin;
	if (options.kmax > 0) enc_options.kmax = options.kmax;

	// Init per-frame config (stored for reuse)
	WebPConfigInit(&state->config);
	state->config.quality = options.quality;
	state->config.lossless = options.lossless;
	state->config.method = options.method;
	state->config.alpha_quality = options.alpha_quality;

	// Validate config to ensure it's valid
	if (!WebPValidateConfig(&state->config)) {
		return 0;  // Invalid config
	}

	state->encoder = WebPAnimEncoderNew(width, height, &enc_options);
	if (!state->encoder) {
		return 0;  // 0 = invalid handle
	}

	int handle = g_nextHandle++;
	g_encoders[handle] = std::move(state);
	return handle;
}

int addFrameToEncoder(int handle, std::string rgba_data, int duration_ms)
{
	auto it = g_encoders.find(handle);
	if (it == g_encoders.end() || !it->second->encoder) {
		return 0;  // Invalid handle or encoder
	}

	StreamingEncoderState* state = it->second.get();

	// Validate data size
	int channels = state->has_alpha ? 4 : 3;
	size_t expected_size = (size_t)state->width * state->height * channels;

	if (rgba_data.size() != expected_size) {
		return 0;
	}

	WebPPicture pic;
	if (!WebPPictureInit(&pic)) {
		return 0;
	}

	pic.use_argb = 1;
	pic.width = state->width;
	pic.height = state->height;

	// Allocate picture memory like the working encode() function does
	if (!WebPPictureAlloc(&pic)) {
		WebPPictureFree(&pic);
		return 0;
	}

	int stride = channels * state->width;
	// Use .c_str() like the working encode() function
	int success = state->has_alpha
		? WebPPictureImportRGBA(&pic, (uint8_t*)rgba_data.c_str(), stride)
		: WebPPictureImportRGB(&pic, (uint8_t*)rgba_data.c_str(), stride);

	if (!success) {
		WebPPictureFree(&pic);
		return 0;
	}

	success = WebPAnimEncoderAdd(state->encoder, &pic, state->timestamp_ms, &state->config);

	state->timestamp_ms += duration_ms;

	WebPPictureFree(&pic);
	return success;
}

val finalizeEncoder(int handle)
{
	auto it = g_encoders.find(handle);
	if (it == g_encoders.end() || !it->second->encoder) {
		return val::null();
	}

	StreamingEncoderState* state = it->second.get();

	// Signal end with null frame
	WebPAnimEncoderAdd(state->encoder, NULL, state->timestamp_ms, NULL);

	WebPData webp_data;
	WebPDataInit(&webp_data);
	if (!WebPAnimEncoderAssemble(state->encoder, &webp_data)) {
		// Cleanup encoder
		WebPAnimEncoderDelete(state->encoder);
		g_encoders.erase(it);
		return val::null();
	}

	// Cleanup the anim encoder (no longer needed)
	WebPAnimEncoderDelete(state->encoder);
	state->encoder = nullptr;

	// Apply loop count via WebPMux
	WebPMux* mux = WebPMuxCreate(&webp_data, 1);
	if (mux) {
		WebPMuxAnimParams params;
		WebPMuxGetAnimationParams(mux, &params);
		params.loop_count = state->loop_count;
		WebPMuxSetAnimationParams(mux, &params);

		WebPData output;
		WebPDataInit(&output);
		WebPMuxAssemble(mux, &output);
		WebPMuxDelete(mux);
		WebPDataClear(&webp_data);

		val encoded_data = Uint8Array.new_(typed_memory_view(output.size, output.bytes));

		// Auto-cleanup: remove from registry after finalize
		g_encoders.erase(it);

		// Note: WebPDataClear(&output) would invalidate the typed_memory_view
		// The JS side must copy the data before it can be cleared
		// Emscripten's Uint8Array.new_() creates a copy, so we're safe
		WebPDataClear(&output);

		return encoded_data;
	}

	val encoded_data = Uint8Array.new_(typed_memory_view(webp_data.size, webp_data.bytes));

	// Auto-cleanup: remove from registry after finalize
	g_encoders.erase(it);

	WebPDataClear(&webp_data);

	return encoded_data;
}

void deleteEncoder(int handle)
{
	auto it = g_encoders.find(handle);
	if (it != g_encoders.end()) {
		if (it->second->encoder) {
			WebPAnimEncoderDelete(it->second->encoder);
		}
		g_encoders.erase(it);
	}
}

// Batch animation encoding API - processes all frames in one call
// This is more reliable as it avoids cross-call state management
val encodeAnimation(int width, int height, bool has_alpha, val frames, AnimationEncoderOptions options)
{
	int frame_count = frames["length"].as<int>();
	if (frame_count == 0) {
		return val::null();
	}

	// Init encoder options
	WebPAnimEncoderOptions enc_options;
	WebPAnimEncoderOptionsInit(&enc_options);
	enc_options.minimize_size = options.minimize_size;
	enc_options.allow_mixed = options.allow_mixed;
	if (options.kmin > 0) enc_options.kmin = options.kmin;
	if (options.kmax > 0) enc_options.kmax = options.kmax;

	// Create encoder
	WebPAnimEncoder* encoder = WebPAnimEncoderNew(width, height, &enc_options);
	if (!encoder) {
		return val::null();
	}

	// Init per-frame config
	WebPConfig config;
	WebPConfigInit(&config);
	config.quality = options.quality;
	config.lossless = options.lossless;
	config.method = options.method;
	config.alpha_quality = options.alpha_quality;

	if (!WebPValidateConfig(&config)) {
		WebPAnimEncoderDelete(encoder);
		return val::null();
	}

	int channels = has_alpha ? 4 : 3;
	int stride = channels * width;
	size_t expected_size = (size_t)width * height * channels;
	int timestamp_ms = 0;

	// Process all frames
	for (int i = 0; i < frame_count; i++) {
		val frame = frames[i];
		val frame_data = frame["data"];
		int duration_ms = frame["duration"].as<int>();

		// Get frame data as string (Emscripten converts Uint8Array to std::string)
		std::string data = frame_data.as<std::string>();

		if (data.size() != expected_size) {
			WebPAnimEncoderDelete(encoder);
			return val::null();
		}

		WebPPicture pic;
		if (!WebPPictureInit(&pic)) {
			WebPAnimEncoderDelete(encoder);
			return val::null();
		}

		pic.use_argb = 1;
		pic.width = width;
		pic.height = height;

		if (!WebPPictureAlloc(&pic)) {
			WebPPictureFree(&pic);
			WebPAnimEncoderDelete(encoder);
			return val::null();
		}

		// Use .c_str() like the working encode() function
		int import_success = has_alpha
			? WebPPictureImportRGBA(&pic, (uint8_t*)data.c_str(), stride)
			: WebPPictureImportRGB(&pic, (uint8_t*)data.c_str(), stride);

		if (!import_success) {
			WebPPictureFree(&pic);
			WebPAnimEncoderDelete(encoder);
			return val::null();
		}

		int add_success = WebPAnimEncoderAdd(encoder, &pic, timestamp_ms, &config);
		timestamp_ms += duration_ms;
		WebPPictureFree(&pic);

		if (!add_success) {
			WebPAnimEncoderDelete(encoder);
			return val::null();
		}
	}

	// Signal end with null frame
	WebPAnimEncoderAdd(encoder, NULL, timestamp_ms, NULL);

	// Assemble final WebP
	WebPData webp_data;
	WebPDataInit(&webp_data);
	if (!WebPAnimEncoderAssemble(encoder, &webp_data)) {
		WebPAnimEncoderDelete(encoder);
		return val::null();
	}

	WebPAnimEncoderDelete(encoder);

	// Apply loop count via WebPMux
	WebPMux* mux = WebPMuxCreate(&webp_data, 1);
	if (mux) {
		WebPMuxAnimParams params;
		WebPMuxGetAnimationParams(mux, &params);
		params.loop_count = options.loop_count;
		WebPMuxSetAnimationParams(mux, &params);

		WebPData output;
		WebPDataInit(&output);
		WebPMuxAssemble(mux, &output);
		WebPMuxDelete(mux);
		WebPDataClear(&webp_data);

		val encoded_data = Uint8Array.new_(typed_memory_view(output.size, output.bytes));
		WebPDataClear(&output);
		return encoded_data;
	}

	val encoded_data = Uint8Array.new_(typed_memory_view(webp_data.size, webp_data.bytes));
	WebPDataClear(&webp_data);
	return encoded_data;
}