/*
 *  snd_compress_params.h - codec types and parameters for compressed data
 *  streaming interface
 *
 *  Copyright (C) 2011 Intel Corporation
 *  Authors:	Pierre-Louis.Bossart <pierre-louis.bossart@linux.intel.com>
 *              Vinod Koul <vinod.koul@linux.intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The definitions in this file are derived from the OpenMAX AL version 1.1
 * and OpenMAX IL v 1.1.2 header files which contain the copyright notice below.
 *
 * Copyright (c) 2007-2010 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and/or associated documentation files (the
 * "Materials "), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 */


/* AUDIO CODECS SUPPORTED */
#define MAX_NUM_CODECS 32
#define MAX_NUM_CODEC_DESCRIPTORS 32
#define MAX_NUM_RATES 32
#define MAX_NUM_BITRATES 32

/* Codecs are listed linearly to allow for extensibility */
#define SND_AUDIOCODEC_PCM                   ((__u32) 0x00000001)
#define SND_AUDIOCODEC_MP3                   ((__u32) 0x00000002)
#define SND_AUDIOCODEC_AMR                   ((__u32) 0x00000003)
#define SND_AUDIOCODEC_AMRWB                 ((__u32) 0x00000004)
#define SND_AUDIOCODEC_AMRWBPLUS             ((__u32) 0x00000005)
#define SND_AUDIOCODEC_AAC                   ((__u32) 0x00000006)
#define SND_AUDIOCODEC_WMA                   ((__u32) 0x00000007)
#define SND_AUDIOCODEC_REAL                  ((__u32) 0x00000008)
#define SND_AUDIOCODEC_VORBIS                ((__u32) 0x00000009)
#define SND_AUDIOCODEC_FLAC                  ((__u32) 0x0000000A)
#define SND_AUDIOCODEC_IEC61937              ((__u32) 0x0000000B)

/*
 * Profile and modes are listed with bit masks. This allows for a
 * more compact representation of fields that will not evolve
 * (in contrast to the list of codecs)
 */

#define SND_AUDIOPROFILE_PCM                 ((__u32) 0x00000001)

/* MP3 modes are only useful for encoders */
#define SND_AUDIOCHANMODE_MP3_MONO           ((__u32) 0x00000001)
#define SND_AUDIOCHANMODE_MP3_STEREO         ((__u32) 0x00000002)
#define SND_AUDIOCHANMODE_MP3_JOINTSTEREO    ((__u32) 0x00000004)
#define SND_AUDIOCHANMODE_MP3_DUAL           ((__u32) 0x00000008)

#define SND_AUDIOPROFILE_AMR                 ((__u32) 0x00000001)

/* AMR modes are only useful for encoders */
#define SND_AUDIOMODE_AMR_DTX_OFF            ((__u32) 0x00000001)
#define SND_AUDIOMODE_AMR_VAD1               ((__u32) 0x00000002)
#define SND_AUDIOMODE_AMR_VAD2               ((__u32) 0x00000004)

#define SND_AUDIOSTREAMFORMAT_UNDEFINED	     ((__u32) 0x00000000)
#define SND_AUDIOSTREAMFORMAT_CONFORMANCE    ((__u32) 0x00000001)
#define SND_AUDIOSTREAMFORMAT_IF1            ((__u32) 0x00000002)
#define SND_AUDIOSTREAMFORMAT_IF2            ((__u32) 0x00000004)
#define SND_AUDIOSTREAMFORMAT_FSF            ((__u32) 0x00000008)
#define SND_AUDIOSTREAMFORMAT_RTPPAYLOAD     ((__u32) 0x00000010)
#define SND_AUDIOSTREAMFORMAT_ITU            ((__u32) 0x00000020)

#define SND_AUDIOPROFILE_AMRWB               ((__u32) 0x00000001)

/* AMRWB modes are only useful for encoders */
#define SND_AUDIOMODE_AMRWB_DTX_OFF          ((__u32) 0x00000001)
#define SND_AUDIOMODE_AMRWB_VAD1             ((__u32) 0x00000002)
#define SND_AUDIOMODE_AMRWB_VAD2             ((__u32) 0x00000004)

#define SND_AUDIOPROFILE_AMRWBPLUS           ((__u32) 0x00000001)

#define SND_AUDIOPROFILE_AAC                 ((__u32) 0x00000001)

/* AAC modes are required for encoders and decoders */
#define SND_AUDIOMODE_AAC_MAIN               ((__u32) 0x00000001)
#define SND_AUDIOMODE_AAC_LC                 ((__u32) 0x00000002)
#define SND_AUDIOMODE_AAC_SSR                ((__u32) 0x00000004)
#define SND_AUDIOMODE_AAC_LTP                ((__u32) 0x00000008)
#define SND_AUDIOMODE_AAC_HE                 ((__u32) 0x00000010)
#define SND_AUDIOMODE_AAC_SCALABLE           ((__u32) 0x00000020)
#define SND_AUDIOMODE_AAC_ERLC               ((__u32) 0x00000040)
#define SND_AUDIOMODE_AAC_LD                 ((__u32) 0x00000080)
#define SND_AUDIOMODE_AAC_HE_PS              ((__u32) 0x00000100)
#define SND_AUDIOMODE_AAC_HE_MPS             ((__u32) 0x00000200)

/* AAC formats are required for encoders and decoders */
#define SND_AUDIOSTREAMFORMAT_MP2ADTS        ((__u32) 0x00000001)
#define SND_AUDIOSTREAMFORMAT_MP4ADTS        ((__u32) 0x00000002)
#define SND_AUDIOSTREAMFORMAT_MP4LOAS        ((__u32) 0x00000004)
#define SND_AUDIOSTREAMFORMAT_MP4LATM        ((__u32) 0x00000008)
#define SND_AUDIOSTREAMFORMAT_ADIF           ((__u32) 0x00000010)
#define SND_AUDIOSTREAMFORMAT_MP4FF          ((__u32) 0x00000020)
#define SND_AUDIOSTREAMFORMAT_RAW            ((__u32) 0x00000040)

#define SND_AUDIOPROFILE_WMA7                ((__u32) 0x00000001)
#define SND_AUDIOPROFILE_WMA8                ((__u32) 0x00000002)
#define SND_AUDIOPROFILE_WMA9                ((__u32) 0x00000004)
#define SND_AUDIOPROFILE_WMA10               ((__u32) 0x00000008)

#define SND_AUDIOMODE_WMA_LEVEL1             ((__u32) 0x00000001)
#define SND_AUDIOMODE_WMA_LEVEL2             ((__u32) 0x00000002)
#define SND_AUDIOMODE_WMA_LEVEL3             ((__u32) 0x00000004)
#define SND_AUDIOMODE_WMA_LEVEL4             ((__u32) 0x00000008)
#define SND_AUDIOMODE_WMAPRO_LEVELM0         ((__u32) 0x00000010)
#define SND_AUDIOMODE_WMAPRO_LEVELM1         ((__u32) 0x00000020)
#define SND_AUDIOMODE_WMAPRO_LEVELM2         ((__u32) 0x00000040)
#define SND_AUDIOMODE_WMAPRO_LEVELM3         ((__u32) 0x00000080)

#define SND_AUDIOSTREAMFORMAT_WMA_ASF        ((__u32) 0x00000001)
/*
 * Some implementations strip the ASF header and only send ASF packets
 * to the DSP
 */
#define SND_AUDIOSTREAMFORMAT_WMA_NOASF_HDR  ((__u32) 0x00000002)

#define SND_AUDIOPROFILE_REALAUDIO           ((__u32) 0x00000001)

#define SND_AUDIOMODE_REALAUDIO_G2           ((__u32) 0x00000001)
#define SND_AUDIOMODE_REALAUDIO_8            ((__u32) 0x00000002)
#define SND_AUDIOMODE_REALAUDIO_10           ((__u32) 0x00000004)
#define SND_AUDIOMODE_REALAUDIO_SURROUND     ((__u32) 0x00000008)

#define SND_AUDIOPROFILE_VORBIS              ((__u32) 0x00000001)

#define SND_AUDIOMODE_VORBIS                 ((__u32) 0x00000001)

#define SND_AUDIOPROFILE_FLAC                ((__u32) 0x00000001)

/*
 * Define quality levels for FLAC encoders, from LEVEL0 (fast)
 * to LEVEL8 (best)
 */
#define SND_AUDIOMODE_FLAC_LEVEL0            ((__u32) 0x00000001)
#define SND_AUDIOMODE_FLAC_LEVEL1            ((__u32) 0x00000002)
#define SND_AUDIOMODE_FLAC_LEVEL2            ((__u32) 0x00000004)
#define SND_AUDIOMODE_FLAC_LEVEL3            ((__u32) 0x00000008)
#define SND_AUDIOMODE_FLAC_LEVEL4            ((__u32) 0x00000010)
#define SND_AUDIOMODE_FLAC_LEVEL5            ((__u32) 0x00000020)
#define SND_AUDIOMODE_FLAC_LEVEL6            ((__u32) 0x00000040)
#define SND_AUDIOMODE_FLAC_LEVEL7            ((__u32) 0x00000080)
#define SND_AUDIOMODE_FLAC_LEVEL8            ((__u32) 0x00000100)

#define SND_AUDIOSTREAMFORMAT_FLAC           ((__u32) 0x00000001)
#define SND_AUDIOSTREAMFORMAT_FLAC_OGG       ((__u32) 0x00000002)

/* IEC61937 payloads without CUVP and preambles */
#define SND_AUDIOPROFILE_IEC61937            ((__u32) 0x00000001)
/* IEC61937 with S/PDIF preambles+CUVP bits in 32-bit containers */
#define SND_AUDIOPROFILE_IEC61937_SPDIF      ((__u32) 0x00000002)

/*
 * IEC modes are mandatory for decoders. Format autodetection
 *  will only happen on the DSP side with mode 0. The PCM mode should
 *  not be used, the PCM codec should be used instead
 */
#define SND_AUDIOMODE_IEC_REF_STREAM_HEADER  ((__u32) 0x00000000)
#define SND_AUDIOMODE_IEC_LPCM		     ((__u32) 0x00000001)
#define SND_AUDIOMODE_IEC_AC3		     ((__u32) 0x00000002)
#define SND_AUDIOMODE_IEC_MPEG1		     ((__u32) 0x00000004)
#define SND_AUDIOMODE_IEC_MP3		     ((__u32) 0x00000008)
#define SND_AUDIOMODE_IEC_MPEG2		     ((__u32) 0x00000010)
#define SND_AUDIOMODE_IEC_AACLC		     ((__u32) 0x00000020)
#define SND_AUDIOMODE_IEC_DTS		     ((__u32) 0x00000040)
#define SND_AUDIOMODE_IEC_ATRAC		     ((__u32) 0x00000080)
#define SND_AUDIOMODE_IEC_SACD		     ((__u32) 0x00000100)
#define SND_AUDIOMODE_IEC_EAC3		     ((__u32) 0x00000200)
#define SND_AUDIOMODE_IEC_DTS_HD	     ((__u32) 0x00000400)
#define SND_AUDIOMODE_IEC_MLP		     ((__u32) 0x00000800)
#define SND_AUDIOMODE_IEC_DST		     ((__u32) 0x00001000)
#define SND_AUDIOMODE_IEC_WMAPRO	     ((__u32) 0x00002000)
#define SND_AUDIOMODE_IEC_REF_CXT            ((__u32) 0x00004000)
#define SND_AUDIOMODE_IEC_HE_AAC	     ((__u32) 0x00008000)
#define SND_AUDIOMODE_IEC_HE_AAC2	     ((__u32) 0x00010000)
#define SND_AUDIOMODE_IEC_MPEG_SURROUND	     ((__u32) 0x00020000)

/* <FIXME: multichannel encoders aren't supported for now. Would need
   an additional definition of channel arrangement> */

/* VBR/CBR definitions */
#define SND_RATECONTROLMODE_CONSTANTBITRATE  ((__u32) 0x00000001)
#define SND_RATECONTROLMODE_VARIABLEBITRATE  ((__u32) 0x00000002)

/* Encoder options */

struct wmaEncoderOptions {
	__u32 super_block_align; /* WMA Type-specific data */
};


/**
 * struct vorbisEncoderOptions
 * @quality: Sets encoding quality to n, between -1 (low) and 10 (high).
 * In the default mode of operation, the quality level is 3.
 * Normal quality range is 0 - 10.
 * @managed: Boolean. Set  bitrate  management  mode. This turns off the
 * normal VBR encoding, but allows hard or soft bitrate constraints to be
 * enforced by the encoder. This mode can be slower, and may also be
 * lower quality. It is primarily useful for streaming.
 * @maxBitrate: enabled only is managed is TRUE
 * @minBitrate: enabled only is managed is TRUE
 * @downmix: Boolean. Downmix input from stereo to mono (has no effect on
 * non-stereo streams). Useful for lower-bitrate encoding.
 *
 * These options were extracted from the OpenMAX IL spec and gstreamer vorbisenc
 * properties
 *
 * For best quality users should specify VBR mode and set quality levels.
 */

struct vorbisEncoderOptions {
	int quality;
	__u32 managed;
	__u32 maxBitrate;
	__u32 minBirate;
	__u32 downmix;
};


/**
 * struct realEncoderOptions
 * @coupling_quant_bits: is the number of coupling quantization bits in the stream
 * @coupling_start_region: is the coupling start region in the stream
 * @num_regions: is the number of regions value
 *
 * These options were extracted from the OpenMAX IL spec
 */

struct realEncoderOptions {
	__u32 coupling_quant_bits;
	__u32 coupling_start_region;
	__u32 num_regions;
};

/**
 * struct flacEncoderOptions
 * @serialNumber: valid only for OGG formats, needs to be set by application
 * @replayGain: Add ReplayGain tags
 *
 * These options were extracted from the FLAC online documentation
 * at http://flac.sourceforge.net/documentation_tools_flac.html
 *
 * To make the API simpler, it is assumed that the user will select quality
 * profiles. Additional options that affect encoding quality and speed can
 * be added at a later stage if need be.
 *
 * By default the Subset format is used by encoders.
 *
 * TAGS such as pictures, etc, cannot be handled by an offloaded encoder and are
 * not supported in this API.
 */

struct flacEncoderOptions {
	__u32 serialNumber;
	__u32 replayGain;
};

struct genericEncoderOptions {
	__u32 encoderBandwidth;
	int reserved[15];
};

union AudioCodecOptions {
	struct wmaEncoderOptions wmaSpecificOptions;
	struct vorbisEncoderOptions vorbisSpecificOptions;
	struct realEncoderOptions realSpecificOptions;
	struct flacEncoderOptions flacEncoderOptions;
	struct genericEncoderOptions genericOptions;
};

/** struct SndAudioCodecDescriptor - description of codec capabilities
 * @maxChannels: maximum number of audio channels
 * @minBitsPerSample: Minimum bits per sample of PCM data <FIXME: needed?>
 * @maxBitsPerSample: Maximum bits per sample of PCM data <FIXME: needed?>
 * @minSampleRate: Minimum sampling rate supported, unit is Hz
 * @maxSampleRate: Minimum sampling rate supported, unit is Hz
 * @isFreqRangeContinuous: TRUE if the device supports a continuous range of
 *                         sampling rates between minSampleRate and maxSampleRate;
 *                         otherwise FALSE <FIXME: needed?>
 * @SampleRatesSupported: Indexed array containing supported sampling rates in Hz
 * @numSampleRatesSupported: Size of the pSamplesRatesSupported array
 * @minBitRate: Minimum bitrate in bits per second
 * @maxBitRate: Max bitrate in bits per second
 * @isBitrateRangeContinuous: TRUE if the device supports a continuous range of
 *		      bitrates between minBitRate and maxBitRate; otherwise FALSE
 * @BitratesSupported: Indexed array containing supported bit rates
 * @numBitratesSupported: Size of the pBiratesSupported array
 * @rateControlSupported: value is specified by SND_RATECONTROLMODE defines.
 * @profileSetting: Profile supported. See SND_AUDIOPROFILE defines.
 * @modeSetting: Mode supported. See SND_AUDIOMODE defines
 * @streamFormat: Format supported. See SND_AUDIOSTREAMFORMAT defines
 * @reserved: reserved for future use
 *
 * This structure provides a scalar value for profile, mode and stream format fields.
 * If an implementation supports multiple combinations, they will be listed as codecs
 * with different IDs, for example there would be 2 decoders for AAC-RAW and AAC-ADTS.
 * This entails some redundancy but makes it easier to avoid invalid configurations.
 *
 */

struct SndAudioCodecDescriptor {
	__u32 maxChannels;
	__u32 minBitsPerSample;
	__u32 maxBitsPerSample;
	__u32 minSampleRate;
	__u32 maxSampleRate;
	__u32 isFreqRangeContinuous;
	__u32 sampleRatesSupported[MAX_NUM_RATES];
	__u32 numSampleRatesSupported;
	__u32 minBitRate;
	__u32 maxBitRate;
	__u32 isBitrateRangeContinuous;
	__u32 bitratesSupported[MAX_NUM_BITRATES];
	__u32 numBitratesSupported;
	__u32 rateControlSupported;
	__u32 profileSetting;
	__u32 modeSetting;
	__u32 streamFormat;
	__u32 reserved[16];
};

/** struct SndAudioCodecSettings -
 * @codecId: Identifies the supported audio encoder/decoder. See SND_AUDIOCODEC	macros.
 * @channelsIn: Number of input audio channels
 * @channelsOut: Number of output channels. In case of contradiction between this field and the
 *		channelMode field, the channelMode field overrides
 * @sampleRate: Audio sample rate of input data
 * @bitRate: Bitrate of encoded data. May be ignored by decoders
 * @bitsPerSample: <FIXME: Needed? DSP implementations can handle their own format>
 * @rateControl: Encoding rate control. See SND_RATECONTROLMODE defines.
 *               Encoders may rely on profiles for quality levels.
 *		 May be ignored by decoders.
 * @profileSetting: Mandatory for encoders, can be mandatory for specific decoders as well.
 *		See SND_AUDIOPROFILE defines
 * @levelSetting: Supported level (Only used by WMA at the moment)
 * @channelMode: Channel mode for encoder. See SND_AUDIOCHANMODE defines
 * @streamFormat: Format of encoded bistream. Mandatory when defined. See SND_AUDIOSTREAMFORMAT
 *		defines
 * @blockAlignment: Block alignment in bytes of an audio sample. Only required for PCM or IEC formats
 * @options: encoder-specific settings
 * @reserved: reserved for future use
 */

struct SndAudioCodecSettings {
	__u32 codecId;
	__u32 channelsIn;
	__u32 channelsOut;
	__u32 sampleRate;
	__u32 bitRate;
	__u32 bitsPerSample;
	__u32 rateControl;
	__u32 profileSetting;
	__u32 levelSetting;
	__u32 channelMode;
	__u32 streamFormat;
	__u32 blockAlignment;
	union AudioCodecOptions options;
	__u32 reserved[3];
};
