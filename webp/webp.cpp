#include "emscripten/emscripten.h"
#include "emscripten/bind.h"
#include "encode.h"
#include "decode.h"


EMSCRIPTEN_BINDINGS(module)
{
	emscripten::value_object<SimpleWebPConfig>("SimpleWebPConfig")
		.field("lossless", &SimpleWebPConfig::lossless)
		.field("quality", &SimpleWebPConfig::quality);

	emscripten::value_object<AnimationEncoderOptions>("AnimationEncoderOptions")
		.field("minimize_size", &AnimationEncoderOptions::minimize_size)
		.field("kmin", &AnimationEncoderOptions::kmin)
		.field("kmax", &AnimationEncoderOptions::kmax)
		.field("quality", &AnimationEncoderOptions::quality)
		.field("lossless", &AnimationEncoderOptions::lossless)
		.field("method", &AnimationEncoderOptions::method)
		.field("loop_count", &AnimationEncoderOptions::loop_count)
		.field("alpha_quality", &AnimationEncoderOptions::alpha_quality)
		.field("allow_mixed", &AnimationEncoderOptions::allow_mixed);

	function("encoder_version", &encoder_version);
	function("encodeRGB", &encodeRGB);
	function("encodeRGBA", &encodeRGBA);
	function("encode", &encode);
	function("encodeAnimation", &encodeAnimation);

	function("decoder_version", &decoder_version);
	function("decodeRGB", &decodeRGB);
	function("decodeRGBA", &decodeRGBA);
	function("decode", &decode);
  function("decodeAnimation", &decodeAnimation);
}
