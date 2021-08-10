#ifndef _AUDIO_EFFECTS_H
#define _AUDIO_EFFECTS_H

#include <linux/types.h>

/** AUDIO EFFECTS **/


/* CONFIG GET/SET */
#define AUDIO_EFFECTS_CONFIG_CACHE			0
#define AUDIO_EFFECTS_CONFIG_SET			1
#define AUDIO_EFFECTS_CONFIG_GET			2

/* CONFIG HEADER */
/*
 * MODULE_ID,
 * DEVICE,
 * NUM_COMMANDS,
 * COMMAND_ID_1,
 * AUDIO_EFFECTS_CONFIG_CACHE/SET/GET,
 * OFFSET_1,
 * LENGTH_1,
 * VALUES_1,
 * ...,
 * ...,
 * COMMAND_ID_2,
 * AUDIO_EFFECTS_CONFIG_CACHE/SET/GET,
 * OFFSET_2,
 * LENGTH_2,
 * VALUES_2,
 * ...,
 * ...,
 * COMMAND_ID_3,
 * ...
 */


/* CONFIG PARAM IDs */
#define VIRTUALIZER_MODULE		0x00001000
#define VIRTUALIZER_ENABLE		0x00001001
#define VIRTUALIZER_STRENGTH		0x00001002
#define VIRTUALIZER_OUT_TYPE		0x00001003
#define VIRTUALIZER_GAIN_ADJUST		0x00001004
#define VIRTUALIZER_ENABLE_PARAM_LEN		1
#define VIRTUALIZER_STRENGTH_PARAM_LEN		1
#define VIRTUALIZER_OUT_TYPE_PARAM_LEN		1
#define VIRTUALIZER_GAIN_ADJUST_PARAM_LEN	1

#define REVERB_MODULE			0x00002000
#define REVERB_ENABLE			0x00002001
#define REVERB_MODE			0x00002002
#define REVERB_PRESET			0x00002003
#define REVERB_WET_MIX			0x00002004
#define REVERB_GAIN_ADJUST		0x00002005
#define REVERB_ROOM_LEVEL		0x00002006
#define REVERB_ROOM_HF_LEVEL		0x00002007
#define REVERB_DECAY_TIME		0x00002008
#define REVERB_DECAY_HF_RATIO		0x00002009
#define REVERB_REFLECTIONS_LEVEL	0x0000200a
#define REVERB_REFLECTIONS_DELAY	0x0000200b
#define REVERB_LEVEL			0x0000200c
#define REVERB_DELAY			0x0000200d
#define REVERB_DIFFUSION		0x0000200e
#define REVERB_DENSITY			0x0000200f
#define REVERB_ENABLE_PARAM_LEN			1
#define REVERB_MODE_PARAM_LEN			1
#define REVERB_PRESET_PARAM_LEN			1
#define REVERB_WET_MIX_PARAM_LEN		1
#define REVERB_GAIN_ADJUST_PARAM_LEN		1
#define REVERB_ROOM_LEVEL_PARAM_LEN		1
#define REVERB_ROOM_HF_LEVEL_PARAM_LEN		1
#define REVERB_DECAY_TIME_PARAM_LEN		1
#define REVERB_DECAY_HF_RATIO_PARAM_LEN		1
#define REVERB_REFLECTIONS_LEVEL_PARAM_LEN	1
#define REVERB_REFLECTIONS_DELAY_PARAM_LEN	1
#define REVERB_LEVEL_PARAM_LEN			1
#define REVERB_DELAY_PARAM_LEN			1
#define REVERB_DIFFUSION_PARAM_LEN		1
#define REVERB_DENSITY_PARAM_LEN		1

#define BASS_BOOST_MODULE		0x00003000
#define BASS_BOOST_ENABLE		0x00003001
#define BASS_BOOST_MODE			0x00003002
#define BASS_BOOST_STRENGTH		0x00003003
#define BASS_BOOST_ENABLE_PARAM_LEN		1
#define BASS_BOOST_MODE_PARAM_LEN		1
#define BASS_BOOST_STRENGTH_PARAM_LEN		1

#define EQ_MODULE			0x00004000
#define EQ_ENABLE			0x00004001
#define EQ_CONFIG			0x00004002
#define EQ_NUM_BANDS			0x00004003
#define EQ_BAND_LEVELS			0x00004004
#define EQ_BAND_LEVEL_RANGE		0x00004005
#define EQ_BAND_FREQS			0x00004006
#define EQ_SINGLE_BAND_FREQ_RANGE	0x00004007
#define EQ_SINGLE_BAND_FREQ		0x00004008
#define EQ_BAND_INDEX			0x00004009
#define EQ_PRESET_ID			0x0000400a
#define EQ_NUM_PRESETS			0x0000400b
#define EQ_PRESET_NAME			0x0000400c
#define EQ_ENABLE_PARAM_LEN			1
#define EQ_CONFIG_PARAM_LEN			3
#define EQ_CONFIG_PER_BAND_PARAM_LEN		5
#define EQ_NUM_BANDS_PARAM_LEN			1
#define EQ_BAND_LEVELS_PARAM_LEN		13
#define EQ_BAND_LEVEL_RANGE_PARAM_LEN		2
#define EQ_BAND_FREQS_PARAM_LEN			13
#define EQ_SINGLE_BAND_FREQ_RANGE_PARAM_LEN	2
#define EQ_SINGLE_BAND_FREQ_PARAM_LEN		1
#define EQ_BAND_INDEX_PARAM_LEN			1
#define EQ_PRESET_ID_PARAM_LEN			1
#define EQ_NUM_PRESETS_PARAM_LEN		1
#define EQ_PRESET_NAME_PARAM_LEN		32

#define EQ_TYPE_NONE	0
#define EQ_BASS_BOOST	1
#define EQ_BASS_CUT	2
#define EQ_TREBLE_BOOST	3
#define EQ_TREBLE_CUT	4
#define EQ_BAND_BOOST	5
#define EQ_BAND_CUT	6

#define SOFT_VOLUME_MODULE		0x00006000
#define SOFT_VOLUME_ENABLE		0x00006001
#define SOFT_VOLUME_GAIN_2CH		0x00006002
#define SOFT_VOLUME_GAIN_MASTER		0x00006003
#define SOFT_VOLUME_ENABLE_PARAM_LEN		1
#define SOFT_VOLUME_GAIN_2CH_PARAM_LEN		2
#define SOFT_VOLUME_GAIN_MASTER_PARAM_LEN	1

#define SOFT_VOLUME2_MODULE		0x00007000
#define SOFT_VOLUME2_ENABLE		0x00007001
#define SOFT_VOLUME2_GAIN_2CH		0x00007002
#define SOFT_VOLUME2_GAIN_MASTER	0x00007003
#define SOFT_VOLUME2_ENABLE_PARAM_LEN		SOFT_VOLUME_ENABLE_PARAM_LEN
#define SOFT_VOLUME2_GAIN_2CH_PARAM_LEN		SOFT_VOLUME_GAIN_2CH_PARAM_LEN
#define SOFT_VOLUME2_GAIN_MASTER_PARAM_LEN	\
					SOFT_VOLUME_GAIN_MASTER_PARAM_LEN

#define PBE_CONF_MODULE_ID	0x00010C2A
#define PBE_CONF_PARAM_ID	0x00010C49

#define PBE_MODULE		0x00008000
#define PBE_ENABLE		0x00008001
#define PBE_CONFIG		0x00008002
#define PBE_ENABLE_PARAM_LEN		1
#define PBE_CONFIG_PARAM_LEN		28

/* Command Payload length and size for Non-IID commands */
#define COMMAND_PAYLOAD_LEN	3
#define COMMAND_PAYLOAD_SZ	(COMMAND_PAYLOAD_LEN * sizeof(__u32))
/* Command Payload length and size for IID commands */
#define COMMAND_IID_PAYLOAD_LEN	4
#define COMMAND_IID_PAYLOAD_SZ	(COMMAND_IID_PAYLOAD_LEN * sizeof(__u32))
#define MAX_INBAND_PARAM_SZ	4096
#define Q27_UNITY		(1 << 27)
#define Q8_UNITY		(1 << 8)
#define CUSTOM_OPENSL_PRESET	18

#define VIRTUALIZER_ENABLE_PARAM_SZ	\
			(VIRTUALIZER_ENABLE_PARAM_LEN*sizeof(__u32))
#define VIRTUALIZER_STRENGTH_PARAM_SZ	\
			(VIRTUALIZER_STRENGTH_PARAM_LEN*sizeof(__u32))
#define VIRTUALIZER_OUT_TYPE_PARAM_SZ	\
			(VIRTUALIZER_OUT_TYPE_PARAM_LEN*sizeof(__u32))
#define VIRTUALIZER_GAIN_ADJUST_PARAM_SZ	\
			(VIRTUALIZER_GAIN_ADJUST_PARAM_LEN*sizeof(__u32))
struct virtualizer_params {
	__u32 device;
	__u32 enable_flag;
	__u32 strength;
	__u32 out_type;
	__s32 gain_adjust;
};

#define NUM_OSL_REVERB_PRESETS_SUPPORTED	6
#define REVERB_ENABLE_PARAM_SZ		\
			(REVERB_ENABLE_PARAM_LEN*sizeof(__u32))
#define REVERB_MODE_PARAM_SZ		\
			(REVERB_MODE_PARAM_LEN*sizeof(__u32))
#define REVERB_PRESET_PARAM_SZ		\
			(REVERB_PRESET_PARAM_LEN*sizeof(__u32))
#define REVERB_WET_MIX_PARAM_SZ		\
			(REVERB_WET_MIX_PARAM_LEN*sizeof(__u32))
#define REVERB_GAIN_ADJUST_PARAM_SZ	\
			(REVERB_GAIN_ADJUST_PARAM_LEN*sizeof(__u32))
#define REVERB_ROOM_LEVEL_PARAM_SZ	\
			(REVERB_ROOM_LEVEL_PARAM_LEN*sizeof(__u32))
#define REVERB_ROOM_HF_LEVEL_PARAM_SZ	\
			(REVERB_ROOM_HF_LEVEL_PARAM_LEN*sizeof(__u32))
#define REVERB_DECAY_TIME_PARAM_SZ	\
			(REVERB_DECAY_TIME_PARAM_LEN*sizeof(__u32))
#define REVERB_DECAY_HF_RATIO_PARAM_SZ	\
			(REVERB_DECAY_HF_RATIO_PARAM_LEN*sizeof(__u32))
#define REVERB_REFLECTIONS_LEVEL_PARAM_SZ	\
			(REVERB_REFLECTIONS_LEVEL_PARAM_LEN*sizeof(__u32))
#define REVERB_REFLECTIONS_DELAY_PARAM_SZ	\
			(REVERB_REFLECTIONS_DELAY_PARAM_LEN*sizeof(__u32))
#define REVERB_LEVEL_PARAM_SZ		\
			(REVERB_LEVEL_PARAM_LEN*sizeof(__u32))
#define REVERB_DELAY_PARAM_SZ		\
			(REVERB_DELAY_PARAM_LEN*sizeof(__u32))
#define REVERB_DIFFUSION_PARAM_SZ	\
			(REVERB_DIFFUSION_PARAM_LEN*sizeof(__u32))
#define REVERB_DENSITY_PARAM_SZ		\
			(REVERB_DENSITY_PARAM_LEN*sizeof(__u32))
struct reverb_params {
	__u32 device;
	__u32 enable_flag;
	__u32 mode;
	__u32 preset;
	__u32 wet_mix;
	__s32  gain_adjust;
	__s32  room_level;
	__s32  room_hf_level;
	__u32 decay_time;
	__u32 decay_hf_ratio;
	__s32  reflections_level;
	__u32 reflections_delay;
	__s32  level;
	__u32 delay;
	__u32 diffusion;
	__u32 density;
};

#define BASS_BOOST_ENABLE_PARAM_SZ	\
			(BASS_BOOST_ENABLE_PARAM_LEN*sizeof(__u32))
#define BASS_BOOST_MODE_PARAM_SZ	\
			(BASS_BOOST_MODE_PARAM_LEN*sizeof(__u32))
#define BASS_BOOST_STRENGTH_PARAM_SZ	\
			(BASS_BOOST_STRENGTH_PARAM_LEN*sizeof(__u32))
struct bass_boost_params {
	__u32 device;
	__u32 enable_flag;
	__u32 mode;
	__u32 strength;
};


#define MAX_EQ_BANDS 12
#define MAX_OSL_EQ_BANDS 5
#define EQ_ENABLE_PARAM_SZ			\
			(EQ_ENABLE_PARAM_LEN*sizeof(__u32))
#define EQ_CONFIG_PARAM_SZ			\
			(EQ_CONFIG_PARAM_LEN*sizeof(__u32))
#define EQ_CONFIG_PER_BAND_PARAM_SZ		\
			(EQ_CONFIG_PER_BAND_PARAM_LEN*sizeof(__u32))
#define EQ_CONFIG_PARAM_MAX_LEN			(EQ_CONFIG_PARAM_LEN+\
			MAX_EQ_BANDS*EQ_CONFIG_PER_BAND_PARAM_LEN)
#define EQ_CONFIG_PARAM_MAX_SZ			\
			(EQ_CONFIG_PARAM_MAX_LEN*sizeof(__u32))
#define EQ_NUM_BANDS_PARAM_SZ			\
			(EQ_NUM_BANDS_PARAM_LEN*sizeof(__u32))
#define EQ_BAND_LEVELS_PARAM_SZ			\
			(EQ_BAND_LEVELS_PARAM_LEN*sizeof(__u32))
#define EQ_BAND_LEVEL_RANGE_PARAM_SZ		\
			(EQ_BAND_LEVEL_RANGE_PARAM_LEN*sizeof(__u32))
#define EQ_BAND_FREQS_PARAM_SZ			\
			(EQ_BAND_FREQS_PARAM_LEN*sizeof(__u32))
#define EQ_SINGLE_BAND_FREQ_RANGE_PARAM_SZ	\
			(EQ_SINGLE_BAND_FREQ_RANGE_PARAM_LEN*sizeof(__u32))
#define EQ_SINGLE_BAND_FREQ_PARAM_SZ		\
			(EQ_SINGLE_BAND_FREQ_PARAM_LEN*sizeof(__u32))
#define EQ_BAND_INDEX_PARAM_SZ			\
			(EQ_BAND_INDEX_PARAM_LEN*sizeof(__u32))
#define EQ_PRESET_ID_PARAM_SZ			\
			(EQ_PRESET_ID_PARAM_LEN*sizeof(__u32))
#define EQ_NUM_PRESETS_PARAM_SZ			\
			(EQ_NUM_PRESETS_PARAM_LEN*sizeof(__u8))
struct eq_config_t {
	__s32 eq_pregain;
	__s32 preset_id;
	__u32 num_bands;
};
struct eq_per_band_config_t {
	__s32 band_idx;
	__u32 filter_type;
	__u32 freq_millihertz;
	__s32  gain_millibels;
	__u32 quality_factor;
};
struct eq_per_band_freq_range_t {
	__u32 band_index;
	__u32 min_freq_millihertz;
	__u32 max_freq_millihertz;
};

struct eq_params {
	__u32 device;
	__u32 enable_flag;
	struct eq_config_t config;
	struct eq_per_band_config_t per_band_cfg[MAX_EQ_BANDS];
	struct eq_per_band_freq_range_t per_band_freq_range[MAX_EQ_BANDS];
	__u32 band_index;
	__u32 freq_millihertz;
};

#define PBE_ENABLE_PARAM_SZ	\
			(PBE_ENABLE_PARAM_LEN*sizeof(__u32))
#define PBE_CONFIG_PARAM_SZ	\
			(PBE_CONFIG_PARAM_LEN*sizeof(__u16))
struct pbe_config_t {
	__s16  real_bass_mix;
	__s16  bass_color_control;
	__u16 main_chain_delay;
	__u16 xover_filter_order;
	__u16 bandpass_filter_order;
	__s16  drc_delay;
	__u16 rms_tav;
	__s16 exp_threshold;
	__u16 exp_slope;
	__s16 comp_threshold;
	__u16 comp_slope;
	__u16 makeup_gain;
	__u32 comp_attack;
	__u32 comp_release;
	__u32 exp_attack;
	__u32 exp_release;
	__s16 limiter_bass_threshold;
	__s16 limiter_high_threshold;
	__s16 limiter_bass_makeup_gain;
	__s16 limiter_high_makeup_gain;
	__s16 limiter_bass_gc;
	__s16 limiter_high_gc;
	__s16  limiter_delay;
	__u16 reserved;
	/* place holder for filter coeffs to be followed */
	__s32 p1LowPassCoeffs[5*2];
	__s32 p1HighPassCoeffs[5*2];
	__s32 p1BandPassCoeffs[5*3];
	__s32 p1BassShelfCoeffs[5];
	__s32 p1TrebleShelfCoeffs[5];
} __packed;

struct pbe_params {
	__u32 device;
	__u32 enable_flag;
	__u32 cfg_len;
	struct pbe_config_t config;
};

#define SOFT_VOLUME_ENABLE_PARAM_SZ		\
			(SOFT_VOLUME_ENABLE_PARAM_LEN*sizeof(__u32))
#define SOFT_VOLUME_GAIN_MASTER_PARAM_SZ	\
			(SOFT_VOLUME_GAIN_MASTER_PARAM_LEN*sizeof(__u32))
#define SOFT_VOLUME_GAIN_2CH_PARAM_SZ		\
			(SOFT_VOLUME_GAIN_2CH_PARAM_LEN*sizeof(__u16))
struct soft_volume_params {
	__u32 device;
	__u32 enable_flag;
	__u32 master_gain;
	__u32 left_gain;
	__u32 right_gain;
};

struct msm_nt_eff_all_config {
	struct bass_boost_params bass_boost;
	struct pbe_params pbe;
	struct virtualizer_params virtualizer;
	struct reverb_params reverb;
	struct eq_params equalizer;
	struct soft_volume_params saplus_vol;
	struct soft_volume_params topo_switch_vol;
};

#endif /*_MSM_AUDIO_EFFECTS_H*/
