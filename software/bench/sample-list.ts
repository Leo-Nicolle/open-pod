// The bench samples (see samples.ts, which builds them). Kept free of Node
// imports: the browser runner (docs) uses it too.
export type Sample = {
  /** File stem on the card; also the track title the runner looks for. */
  name: string;
  /** ffmpeg output options for the source file. */
  args: string[];
  sourceExt: string;
  /** Extension once converted for the card. */
  cardExt: string;
};

export const SAMPLES: Sample[] = [
  { name: "01 mp3 64k mono 22k", args: ["-ac", "1", "-ar", "22050", "-c:a", "libmp3lame", "-b:a", "64k"], sourceExt: "mp3", cardExt: "mp3" },
  { name: "02 mp3 128k", args: ["-ar", "44100", "-c:a", "libmp3lame", "-b:a", "128k"], sourceExt: "mp3", cardExt: "mp3" },
  { name: "03 mp3 320k", args: ["-ar", "44100", "-c:a", "libmp3lame", "-b:a", "320k"], sourceExt: "mp3", cardExt: "mp3" },
  { name: "04 mp3 V0 VBR", args: ["-ar", "44100", "-c:a", "libmp3lame", "-q:a", "0"], sourceExt: "mp3", cardExt: "mp3" },
  { name: "05 flac 16-44", args: ["-ar", "44100", "-sample_fmt", "s16", "-c:a", "flac"], sourceExt: "flac", cardExt: "flac" },
  { name: "06 wav 16-44", args: ["-ar", "44100", "-c:a", "pcm_s16le"], sourceExt: "wav", cardExt: "flac" },
  { name: "07 flac 24-48", args: ["-ar", "48000", "-sample_fmt", "s32", "-bits_per_raw_sample", "24", "-c:a", "flac"], sourceExt: "flac", cardExt: "flac" },
  { name: "08 flac 24-96", args: ["-ar", "96000", "-sample_fmt", "s32", "-bits_per_raw_sample", "24", "-c:a", "flac"], sourceExt: "flac", cardExt: "flac" },
];

/**
 * Loops in the background (#repeat 1) while the other scenarios run, so
 * browsing and search are measured with the audio feeder busy, as in real
 * use: CD-quality FLAC, the most common lossless stream.
 */
export const BACKGROUND_SAMPLE = "05 flac 16-44";

/** The heaviest stream the card carries (24-bit FLAC is copied as is). */
export const HEAVIEST_SAMPLE = "07 flac 24-48";
