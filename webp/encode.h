#include "src/webp/encode.h"
#include "src/webp/mux.h"
#include "emscripten/emscripten.h"
#include "emscripten/val.h"
#include <map>
#include <memory>

struct SimpleWebPConfig
{
	int lossless;
  float quality;
};

struct AnimationEncoderOptions
{
    int minimize_size;  // 0 or 1 — enables frame diff optimization
    int kmin;           // min keyframe distance (0 = use libwebp default)
    int kmax;           // max keyframe distance (0 = use libwebp default)
    float quality;      // 0-100
    int lossless;       // 0 = lossy, 1 = lossless
    int method;         // 0-6 (0=fast, 6=slowest/best compression)
    int loop_count;     // 0 = infinite, N = loop N times
    int alpha_quality;  // 0-100 (0=smallest, 100=lossless). Default: 100
    int allow_mixed;    // 0 or 1 — allow mixing lossy+lossless per frame
};

// Streaming encoder state - stored in global registry
struct StreamingEncoderState
{
    WebPAnimEncoder* encoder;
    WebPConfig config;
    int width;
    int height;
    bool has_alpha;
    int timestamp_ms;
    int loop_count;
    int allow_mixed;
};

emscripten::val encoder_version();

emscripten::val encodeRGB(std::string rgb, int width, int height, int quality_factor);

emscripten::val encodeRGBA(std::string rgba, int width, int height, int quality_factor);

emscripten::val encode(std::string data, int width, int height, bool use_alpha, SimpleWebPConfig config);

// Streaming encoder API (replaces batch encodeAnimation)
int createStreamingEncoder(int width, int height, bool has_alpha, AnimationEncoderOptions options);
int addFrameToEncoder(int handle, std::string rgba_data, int duration_ms);
emscripten::val finalizeEncoder(int handle);
void deleteEncoder(int handle);