import type {
  WebPConfig,
  Nullable,
  AnimationEncoderOptions,
  WebPDecodedImageData,
  DecodedWebPAnimationFrame,
} from './types'
// @ts-ignore
import Module from './webp-wasm'

// default webp config
const defaultWebpConfig: WebPConfig = {
  lossless: 0,
  quality: 100,
}

export const encoderVersion = async (): Promise<string> => {
  const module = await Module()
  return module.encoder_version()
}

export const encodeRGB = async (
  rgb: Uint8Array,
  width: number,
  height: number,
  quality?: number
): Promise<Nullable<Uint8Array>> => {
  const module = await Module()
  quality = typeof quality !== 'number' ? 100 : Math.min(100, Math.max(0, quality))
  return module.encodeRGB(rgb, width, height, quality)
}

export const encodeRGBA = async (
  rgba: Uint8Array,
  width: number,
  height: number,
  quality?: number
): Promise<Nullable<Uint8Array>> => {
  const module = await Module()
  quality = typeof quality !== 'number' ? 100 : Math.min(100, Math.max(0, quality))
  return module.encodeRGBA(rgba, width, height, quality)
}

export const encode = async (
  data: Uint8Array,
  width: number,
  height: number,
  hasAlpha: boolean,
  config: Partial<WebPConfig>
): Promise<Nullable<Uint8Array>> => {
  const module = await Module()
  const webpConfig = {
    ...defaultWebpConfig,
    ...config,
  }
  webpConfig.lossless = Math.min(1, Math.max(0, webpConfig.lossless))
  webpConfig.quality = Math.min(100, Math.max(0, webpConfig.quality))
  return module.encode(data, width, height, hasAlpha, webpConfig)
}

// Streaming encoder API (replaces batch encodeAnimation)
// Returns an encoder handle (integer) for use with other streaming functions

export const createStreamingEncoder = async (
  width: number,
  height: number,
  hasAlpha: boolean,
  options?: AnimationEncoderOptions
): Promise<number> => {
  const module = await Module()

  // Convert JS options to C++ struct format with defaults
  const opts = {
    minimize_size: options?.minimizeSize ? 1 : 0,
    kmin: options?.kmin ?? 0,
    kmax: options?.kmax ?? 0,
    quality: options?.quality ?? 80,
    lossless: options?.lossless ? 1 : 0,
    method: options?.method ?? 4,
    loop_count: options?.loopCount ?? 0,
    alpha_quality: options?.alphaQuality ?? 100,
    allow_mixed: options?.allowMixed ? 1 : 0,
  }

  const handle = module.createStreamingEncoder(width, height, hasAlpha, opts)
  if (handle === 0) {
    throw new Error('Failed to create WebP streaming encoder')
  }
  return handle
}

export const addFrameToEncoder = async (
  handle: number,
  rgbaData: Uint8Array,
  durationMs: number
): Promise<boolean> => {
  const module = await Module()
  // Pass Uint8Array directly - Emscripten auto-converts to std::string
  const result = module.addFrameToEncoder(handle, rgbaData, durationMs)
  return result === 1
}

export const finalizeEncoder = async (
  handle: number
): Promise<Nullable<Uint8Array>> => {
  const module = await Module()
  return module.finalizeEncoder(handle)
}

export const deleteEncoder = async (
  handle: number
): Promise<void> => {
  const module = await Module()
  module.deleteEncoder(handle)
}

export const decoderVersion = async (): Promise<string> => {
  const module = await Module()
  return module.decoder_version()
}

export const decodeRGB = async (data: Uint8Array): Promise<Nullable<WebPDecodedImageData>> => {
  const module = await Module()
  return module.decodeRGB(data)
}

export const decodeRGBA = async (data: Uint8Array): Promise<Nullable<WebPDecodedImageData>> => {
  const module = await Module()
  return module.decodeRGBA(data)
}

// TODO:
// export const decode = async (data: Uint8Array, hasAlpha: boolean) => {
// 	const module = await Module()
// 	return module.decode(data, hasAlpha)
// }

export const decodeAnimation = async (
  data: Uint8Array,
  hasAlpha: boolean
): Promise<Nullable<DecodedWebPAnimationFrame[]>> => {
  const module = await Module()
  return module.decodeAnimation(data, hasAlpha)
}
