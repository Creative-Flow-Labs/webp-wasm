export interface WebPConfig {
	lossless: number
	quality: number
}

export interface WebPAnimationFrame {
	data: Uint8Array
	duration: number
}

export interface DecodedWebPAnimationFrame extends WebPAnimationFrame {
  width: number
  height: number
}

export interface WebPDecodedImageData {
  width: number
  height: number
  data: Uint8Array
}

export interface AnimationEncoderOptions {
	minimizeSize?: boolean    // Enable frame diff optimization (slower but smaller)
	kmin?: number             // Min keyframe distance (0 = default)
	kmax?: number             // Max keyframe distance (0 = default)
	quality?: number          // 0-100 (default: 80)
	lossless?: boolean        // Lossless encoding (default: false)
	method?: number           // 0-6 compression method (default: 4)
	loopCount?: number        // 0 = infinite, N = loop N times (default: 0)
}

export type Nullable<T> = T | null