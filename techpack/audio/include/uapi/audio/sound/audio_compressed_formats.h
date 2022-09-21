#ifndef __AUDIO_COMPRESSED_FORMATS_H
#define __AUDIO_COMPRESSED_FORMATS_H

#include <linux/types.h>

#define AUDIO_COMP_FORMAT_ALAC 0x1
#define AUDIO_COMP_FORMAT_APE 0x2
#define AUDIO_COMP_FORMAT_APTX 0x3
#define AUDIO_COMP_FORMAT_DSD 0x4
#define AUDIO_COMP_FORMAT_FLAC 0x5
#define AUDIO_COMP_FORMAT_VORBIS 0x6
#define AUDIO_COMP_FORMAT_WMA 0x7
#define AUDIO_COMP_FORMAT_WMA_PRO 0x8

#define SND_COMPRESS_DEC_HDR
struct snd_generic_dec_aac {
	__u16 audio_obj_type;
	__u16 pce_bits_size;
} __attribute__((packed, aligned(4)));

struct snd_generic_dec_flac {
	__u16 sample_size;
	__u16 min_blk_size;
	__u16 max_blk_size;
	__u16 min_frame_size;
	__u16 max_frame_size;
} __attribute__((packed, aligned(4)));

struct snd_generic_dec_alac {
	__u32 frame_length;
	__u8 compatible_version;
	__u8 bit_depth;
	__u8 pb;
	__u8 mb;
	__u8 kb;
	__u8 num_channels;
	__u16 max_run;
	__u32 max_frame_bytes;
	__u32 avg_bit_rate;
	__u32 sample_rate;
	__u32 channel_layout_tag;
} __attribute__((packed, aligned(4)));

struct snd_generic_dec_ape {
	__u16 compatible_version;
	__u16 compression_level;
	__u32 format_flags;
	__u32 blocks_per_frame;
	__u32 final_frame_blocks;
	__u32 total_frames;
	__u16 bits_per_sample;
	__u16 num_channels;
	__u32 sample_rate;
	__u32 seek_table_present;
} __attribute__((packed, aligned(4)));

struct snd_generic_dec_wma {
	__u32 super_block_align;
	__u32 bits_per_sample;
	__u32 channelmask;
	__u32 encodeopt;
	__u32 encodeopt1;
	__u32 encodeopt2;
	__u32 avg_bit_rate;
} __attribute__((packed, aligned(4)));

#define SND_DEC_WMA_EXTENTED_SUPPORT

struct snd_generic_dec_aptx {
	__u32 lap;
	__u32 uap;
	__u32 nap;
} __attribute__((packed, aligned(4)));

struct snd_generic_dec_vorbis {
	__u32 bit_stream_fmt;
} __attribute__((packed, aligned(4)));

/** struct snd_generic_dec_dsd - codec for DSD format
 * @blk_size - dsd channel block size
 */
struct snd_generic_dec_dsd {
	__u32 blk_size;
} __attribute__((packed, aligned(4)));

struct snd_generic_dec_amrwb_plus {
	__u32 bit_stream_fmt;
} __attribute__((packed, aligned(4)));

#endif
