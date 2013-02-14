/* Copyright (c) 2009, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ADSP_AUDIO_MEDIA_FORMAT_H
#define __ADSP_AUDIO_MEDIA_FORMAT_H

/* Supported audio media formats */

/* format block in shmem */
#define ADSP_AUDIO_FORMAT_SHAREDMEMORY	0x01091a78

/* adsp_audio_format_raw_pcm type */
#define ADSP_AUDIO_FORMAT_PCM		0x0103d2fd

/* adsp_audio_format_raw_pcm type */
#define ADSP_AUDIO_FORMAT_DTMF		0x01087725

/* adsp_audio_format_adpcm type */
#define ADSP_AUDIO_FORMAT_ADPCM		0x0103d2ff

/* Yamaha PCM format */
#define ADSP_AUDIO_FORMAT_YADPCM	0x0108dc07

/* ISO/IEC 11172 */
#define ADSP_AUDIO_FORMAT_MP3		0x0103d308

/* ISO/IEC 14496 */
#define ADSP_AUDIO_FORMAT_MPEG4_AAC	0x010422f1

/* AMR-NB audio in FS format */
#define ADSP_AUDIO_FORMAT_AMRNB_FS	0x0105c16c

/* AMR-WB audio in FS format */
#define ADSP_AUDIO_FORMAT_AMRWB_FS	0x0105c16e

/* QCELP 13k, IS733 */
#define ADSP_AUDIO_FORMAT_V13K_FS	0x01080b8a

/* EVRC   8k, IS127 */
#define ADSP_AUDIO_FORMAT_EVRC_FS	0x01080b89

/* EVRC-B   8k, 4GV */
#define ADSP_AUDIO_FORMAT_EVRCB_FS	0x0108f2a3

/* MIDI command stream */
#define ADSP_AUDIO_FORMAT_MIDI		0x0103d300

/* A2DP SBC stream */
#define ADSP_AUDIO_FORMAT_SBC		0x0108c4d8

/* Version 10 Professional */
#define ADSP_AUDIO_FORMAT_WMA_V10PRO	0x0108aa92

/* Version 9 Starndard */
#define ADSP_AUDIO_FORMAT_WMA_V9	0x0108d430

/* AMR WideBand Plus */
#define ADSP_AUDIO_FORMAT_AMR_WB_PLUS   0x0108f3da

/* AC3 Decoder */
#define ADSP_AUDIO_FORMAT_AC3_DECODER   0x0108d5f9

/* Not yet supported audio media formats */

/* ISO/IEC 13818 */
#define ADSP_AUDIO_FORMAT_MPEG2_AAC	0x0103d309

/* 3GPP TS 26.101 Sec 4.0 */
#define ADSP_AUDIO_FORMAT_AMRNB_IF1	0x0103d305

/* 3GPP TS 26.101 Annex A */
#define ADSP_AUDIO_FORMAT_AMRNB_IF2	0x01057b31

/* 3GPP TS 26.201 */
#define ADSP_AUDIO_FORMAT_AMRWB_IF1	0x0103d306

/* 3GPP TS 26.201 */
#define ADSP_AUDIO_FORMAT_AMRWB_IF2	0x0105c16d

/* G.711 */
#define ADSP_AUDIO_FORMAT_G711		0x0106201d

/* QCELP  8k, IS96A */
#define ADSP_AUDIO_FORMAT_V8K_FS	0x01081d29

/* Version 1 codec */
#define ADSP_AUDIO_FORMAT_WMA_V1	0x01055b2b

/* Version 2, 7 & 8 codec */
#define ADSP_AUDIO_FORMAT_WMA_V8	0x01055b2c

/* Version 9 Professional codec */
#define ADSP_AUDIO_FORMAT_WMA_V9PRO	0x01055b2d

/* Version 9 Voice codec */
#define ADSP_AUDIO_FORMAT_WMA_SP1	0x01055b2e

/* Version 9 Lossless codec */
#define ADSP_AUDIO_FORMAT_WMA_LOSSLESS	0x01055b2f

/* Real Media content, low-bitrate */
#define ADSP_AUDIO_FORMAT_RA_SIPR	0x01042a0f

/* Real Media content */
#define ADSP_AUDIO_FORMAT_RA_COOK	0x01042a0e


/* For all of the audio formats, unless specified otherwise, */
/* the following apply: */
/* Format block bits are arranged in bytes and words in little-endian */
/* order, i.e., least-significant bit first and least-significant */
/* byte first. */


/* AAC Format Block. */

/* AAC format block consist of a format identifier followed by */
/* AudioSpecificConfig formatted according to ISO/IEC 14496-3 */

/* The following AAC format identifiers are supported */
#define ADSP_AUDIO_AAC_ADTS		0x010619cf
#define ADSP_AUDIO_AAC_MPEG4_ADTS	0x010619d0
#define ADSP_AUDIO_AAC_LOAS		0x010619d1
#define ADSP_AUDIO_AAC_ADIF		0x010619d2
#define ADSP_AUDIO_AAC_RAW		0x010619d3
#define ADSP_AUDIO_AAC_FRAMED_RAW	0x0108c1fb

struct adsp_audio_no_payload_format {
	/* Media Format Code (must always be first element) */
	u32 format;
	/* no payload for this format type */
} __attribute__ ((packed));

/* Maxmum number of bytes allowed in a format block */
#define ADSP_AUDIO_FORMAT_DATA_MAX 16

/* For convenience, to be used as a standard format block */
/* for various media types that don't need a unique format block */
/* ie. PCM, DTMF, etc. */
struct adsp_audio_standard_format {
	/* Media Format Code (must always be first element) */
	u32 format;

	/* payload */
	u16 channels;
	u16 bits_per_sample;
	u32 sampling_rate;
	u8 is_signed;
	u8 is_interleaved;
} __attribute__ ((packed));

/* ADPCM format block */
struct adsp_audio_adpcm_format {
	/* Media Format Code (must always be first element) */
	u32 format;

	/* payload */
	u16 channels;
	u16 bits_per_sample;
	u32 sampling_rate;
	u8 is_signed;
	u8 is_interleaved;
	u32 block_size;
} __attribute__ ((packed));

/* MIDI format block */
struct adsp_audio_midi_format {
	/* Media Format Code (must always be first element) */
	u32 format;

	/* payload */
	u32 sampling_rate;
	u16 channels;
	u16 mode;
} __attribute__ ((packed));

#define ADSP_AUDIO_COMPANDING_ALAW	0x10619cd
#define ADSP_AUDIO_COMPANDING_MLAW	0x10619ce

/* G711 format block */
struct adsp_audio_g711_format {
	/* Media Format Code (must always be first element) */
	u32 format;

	/* payload */
	u32 companding;
} __attribute__ ((packed));


struct adsp_audio_wma_pro_format {
	/* Media Format Code (must always be first element) */
	u32 format;

	/* payload */
	u16 format_tag;
	u16 channels;
	u32 samples_per_sec;
	u32 avg_bytes_per_sec;
	u16 block_align;
	u16 valid_bits_per_sample;
	u32 channel_mask;
	u16 encode_opt;
	u16 advanced_encode_opt;
	u32 advanced_encode_opt2;
	u32 drc_peak_reference;
	u32 drc_peak_target;
	u32 drc_average_reference;
	u32 drc_average_target;
} __attribute__ ((packed));

struct adsp_audio_amrwb_plus_format {
	/* Media Format Code (must always be first element) */
	u32		format;

	/* payload */
	u32		size;
	u32		version;
	u32		channels;
	u32		amr_band_mode;
	u32		amr_dtx_mode;
	u32		amr_frame_format;
	u32		amr_isf_index;
} __attribute__ ((packed));

/* Binary Byte Stream Format */
/* Binary format type that defines a byte stream, */
/* can be used to specify any format (ie. AAC) */
struct adsp_audio_binary_format {
	/* Media Format Code (must always be first element) */
	u32 format;

	/* payload */
	/* number of bytes set in byte stream */
	u32 num_bytes;
	/* Byte stream binary data */
	u8 data[ADSP_AUDIO_FORMAT_DATA_MAX];
} __attribute__ ((packed));

struct adsp_audio_shared_memory_format {
	/* Media Format Code (must always be first element) */
	u32		format;

	/* Number of bytes in shared memory */
	u32		len;
	/* Phyisical address to data in shared memory */
	u32		address;
} __attribute__ ((packed));


/* Union of all format types */
union adsp_audio_format {
	/* Basic format block with no payload */
	struct adsp_audio_no_payload_format	no_payload;
	/* Generic format block PCM, DTMF */
	struct adsp_audio_standard_format	standard;
	/* ADPCM format block */
	struct adsp_audio_adpcm_format		adpcm;
	/* MIDI format block */
	struct adsp_audio_midi_format		midi;
	/* G711 format block */
	struct adsp_audio_g711_format		g711;
	/* WmaPro format block */
	struct adsp_audio_wma_pro_format	wma_pro;
	/* WmaPro format block */
	struct adsp_audio_amrwb_plus_format	amrwb_plus;
	/* binary (byte stream) format block, used for AAC */
	struct adsp_audio_binary_format		binary;
	/* format block in shared memory */
	struct adsp_audio_shared_memory_format	shared_mem;
};

#endif


