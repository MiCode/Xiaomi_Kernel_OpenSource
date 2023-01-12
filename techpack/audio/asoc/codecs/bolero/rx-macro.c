// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <soc/swr-common.h>
#include <soc/swr-wcd.h>

#include <asoc/msm-cdc-pinctrl.h>
#include "bolero-cdc.h"
#include "bolero-cdc-registers.h"
#include "bolero-clk-rsc.h"

#define AUTO_SUSPEND_DELAY  50 /* delay in msec */
#define RX_MACRO_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000 |\
			SNDRV_PCM_RATE_384000)
/* Fractional Rates */
#define RX_MACRO_FRAC_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_88200 |\
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800)

#define RX_MACRO_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE |\
		SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

#define RX_MACRO_ECHO_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_48000)
#define RX_MACRO_ECHO_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE |\
		SNDRV_PCM_FMTBIT_S24_3LE)

#define SAMPLING_RATE_44P1KHZ   44100
#define SAMPLING_RATE_88P2KHZ   88200
#define SAMPLING_RATE_176P4KHZ  176400
#define SAMPLING_RATE_352P8KHZ  352800

#define RX_MACRO_MAX_OFFSET 0x1000

#define RX_MACRO_MAX_DMA_CH_PER_PORT 2
#define RX_SWR_STRING_LEN 80
#define RX_MACRO_CHILD_DEVICES_MAX 3

#define RX_MACRO_INTERP_MUX_NUM_INPUTS 3
#define RX_MACRO_SIDETONE_IIR_COEFF_MAX 5

#define STRING(name) #name
#define RX_MACRO_DAPM_ENUM(name, reg, offset, text) \
static SOC_ENUM_SINGLE_DECL(name##_enum, reg, offset, text); \
static const struct snd_kcontrol_new name##_mux = \
		SOC_DAPM_ENUM(STRING(name), name##_enum)

#define RX_MACRO_DAPM_ENUM_EXT(name, reg, offset, text, getname, putname) \
static SOC_ENUM_SINGLE_DECL(name##_enum, reg, offset, text); \
static const struct snd_kcontrol_new name##_mux = \
		SOC_DAPM_ENUM_EXT(STRING(name), name##_enum, getname, putname)

#define RX_MACRO_DAPM_MUX(name, shift, kctl) \
		SND_SOC_DAPM_MUX(name, SND_SOC_NOPM, shift, 0, &kctl##_mux)

#define RX_MACRO_RX_PATH_OFFSET 0x80
#define RX_MACRO_COMP_OFFSET 0x40

#define MAX_IMPED_PARAMS 6

#define RX_MACRO_EC_MIX_TX0_MASK 0xf0
#define RX_MACRO_EC_MIX_TX1_MASK 0x0f
#define RX_MACRO_EC_MIX_TX2_MASK 0x0f

#define RX_MACRO_GAIN_MAX_VAL 0x28
#define RX_MACRO_GAIN_VAL_UNITY 0x0
/* Define macros to increase PA Gain by half */
#define RX_MACRO_MOD_GAIN (RX_MACRO_GAIN_VAL_UNITY + 6)

#define COMP_MAX_COEFF 25
#define IIR_MIX_CFG_MAX 4
#define IIR_MIX_CFG_OFFSET 10

struct wcd_imped_val {
	u32 imped_val;
	u8 index;
};

static const struct wcd_imped_val imped_index[] = {
	{4, 0},
	{5, 1},
	{6, 2},
	{7, 3},
	{8, 4},
	{9, 5},
	{10, 6},
	{11, 7},
	{12, 8},
	{13, 9},
};

struct comp_coeff_val {
	u8 lsb;
	u8 msb;
};

enum {
	HPH_ULP,
	HPH_LOHIFI,
	HPH_MODE_MAX,
};

static const struct comp_coeff_val
			comp_coeff_table [HPH_MODE_MAX][COMP_MAX_COEFF] = {
	{
		{0x40, 0x00},
		{0x4C, 0x00},
		{0x5A, 0x00},
		{0x6B, 0x00},
		{0x7F, 0x00},
		{0x97, 0x00},
		{0xB3, 0x00},
		{0xD5, 0x00},
		{0xFD, 0x00},
		{0x2D, 0x01},
		{0x66, 0x01},
		{0xA7, 0x01},
		{0xF8, 0x01},
		{0x57, 0x02},
		{0xC7, 0x02},
		{0x4B, 0x03},
		{0xE9, 0x03},
		{0xA3, 0x04},
		{0x7D, 0x05},
		{0x90, 0x06},
		{0xD1, 0x07},
		{0x49, 0x09},
		{0x00, 0x0B},
		{0x01, 0x0D},
		{0x59, 0x0F},
	},
	{
		{0x40, 0x00},
		{0x4C, 0x00},
		{0x5A, 0x00},
		{0x6B, 0x00},
		{0x80, 0x00},
		{0x98, 0x00},
		{0xB4, 0x00},
		{0xD5, 0x00},
		{0xFE, 0x00},
		{0x2E, 0x01},
		{0x66, 0x01},
		{0xA9, 0x01},
		{0xF8, 0x01},
		{0x56, 0x02},
		{0xC4, 0x02},
		{0x4F, 0x03},
		{0xF0, 0x03},
		{0xAE, 0x04},
		{0x8B, 0x05},
		{0x8E, 0x06},
		{0xBC, 0x07},
		{0x56, 0x09},
		{0x0F, 0x0B},
		{0x13, 0x0D},
		{0x6F, 0x0F},
	},
};

struct rx_macro_reg_mask_val {
	u16 reg;
	u8 mask;
	u8 val;
};

static const struct rx_macro_reg_mask_val imped_table[][MAX_IMPED_PARAMS] = {
	{
		{BOLERO_CDC_RX_RX0_RX_VOL_CTL, 0xff, 0xf2},
		{BOLERO_CDC_RX_RX0_RX_VOL_MIX_CTL, 0xff, 0xf2},
		{BOLERO_CDC_RX_RX0_RX_PATH_SEC1, 0x01, 0x00},
		{BOLERO_CDC_RX_RX1_RX_VOL_CTL, 0xff, 0xf2},
		{BOLERO_CDC_RX_RX1_RX_VOL_MIX_CTL, 0xff, 0xf2},
		{BOLERO_CDC_RX_RX1_RX_PATH_SEC1, 0x01, 0x00},
	},
	{
		{BOLERO_CDC_RX_RX0_RX_VOL_CTL, 0xff, 0xf4},
		{BOLERO_CDC_RX_RX0_RX_VOL_MIX_CTL, 0xff, 0xf4},
		{BOLERO_CDC_RX_RX0_RX_PATH_SEC1, 0x01, 0x00},
		{BOLERO_CDC_RX_RX1_RX_VOL_CTL, 0xff, 0xf4},
		{BOLERO_CDC_RX_RX1_RX_VOL_MIX_CTL, 0xff, 0xf4},
		{BOLERO_CDC_RX_RX1_RX_PATH_SEC1, 0x01, 0x00},
	},
	{
		{BOLERO_CDC_RX_RX0_RX_VOL_CTL, 0xff, 0xf7},
		{BOLERO_CDC_RX_RX0_RX_VOL_MIX_CTL, 0xff, 0xf7},
		{BOLERO_CDC_RX_RX0_RX_PATH_SEC1, 0x01, 0x01},
		{BOLERO_CDC_RX_RX1_RX_VOL_CTL, 0xff, 0xf7},
		{BOLERO_CDC_RX_RX1_RX_VOL_MIX_CTL, 0xff, 0xf7},
		{BOLERO_CDC_RX_RX1_RX_PATH_SEC1, 0x01, 0x01},
	},
	{
		{BOLERO_CDC_RX_RX0_RX_VOL_CTL, 0xff, 0xf9},
		{BOLERO_CDC_RX_RX0_RX_VOL_MIX_CTL, 0xff, 0xf9},
		{BOLERO_CDC_RX_RX0_RX_PATH_SEC1, 0x01, 0x00},
		{BOLERO_CDC_RX_RX1_RX_VOL_CTL, 0xff, 0xf9},
		{BOLERO_CDC_RX_RX1_RX_VOL_MIX_CTL, 0xff, 0xf9},
		{BOLERO_CDC_RX_RX1_RX_PATH_SEC1, 0x01, 0x00},
	},
	{
		{BOLERO_CDC_RX_RX0_RX_VOL_CTL, 0xff, 0xfa},
		{BOLERO_CDC_RX_RX0_RX_VOL_MIX_CTL, 0xff, 0xfa},
		{BOLERO_CDC_RX_RX0_RX_PATH_SEC1, 0x01, 0x00},
		{BOLERO_CDC_RX_RX1_RX_VOL_CTL, 0xff, 0xfa},
		{BOLERO_CDC_RX_RX1_RX_VOL_MIX_CTL, 0xff, 0xfa},
		{BOLERO_CDC_RX_RX1_RX_PATH_SEC1, 0x01, 0x00},
	},
	{
		{BOLERO_CDC_RX_RX0_RX_VOL_CTL, 0xff, 0xfb},
		{BOLERO_CDC_RX_RX0_RX_VOL_MIX_CTL, 0xff, 0xfb},
		{BOLERO_CDC_RX_RX0_RX_PATH_SEC1, 0x01, 0x00},
		{BOLERO_CDC_RX_RX1_RX_VOL_CTL, 0xff, 0xfb},
		{BOLERO_CDC_RX_RX1_RX_VOL_MIX_CTL, 0xff, 0xfb},
		{BOLERO_CDC_RX_RX1_RX_PATH_SEC1, 0x01, 0x00},
	},
	{
		{BOLERO_CDC_RX_RX0_RX_VOL_CTL, 0xff, 0xfc},
		{BOLERO_CDC_RX_RX0_RX_VOL_MIX_CTL, 0xff, 0xfc},
		{BOLERO_CDC_RX_RX0_RX_PATH_SEC1, 0x01, 0x00},
		{BOLERO_CDC_RX_RX1_RX_VOL_CTL, 0xff, 0xfc},
		{BOLERO_CDC_RX_RX1_RX_VOL_MIX_CTL, 0xff, 0xfc},
		{BOLERO_CDC_RX_RX1_RX_PATH_SEC1, 0x01, 0x00},
	},
	{
		{BOLERO_CDC_RX_RX0_RX_VOL_CTL, 0xff, 0xfd},
		{BOLERO_CDC_RX_RX0_RX_VOL_MIX_CTL, 0xff, 0xfd},
		{BOLERO_CDC_RX_RX0_RX_PATH_SEC1, 0x01, 0x00},
		{BOLERO_CDC_RX_RX1_RX_VOL_CTL, 0xff, 0xfd},
		{BOLERO_CDC_RX_RX1_RX_VOL_MIX_CTL, 0xff, 0xfd},
		{BOLERO_CDC_RX_RX1_RX_PATH_SEC1, 0x01, 0x00},
	},
	{
		{BOLERO_CDC_RX_RX0_RX_VOL_CTL, 0xff, 0xfd},
		{BOLERO_CDC_RX_RX0_RX_VOL_MIX_CTL, 0xff, 0xfd},
		{BOLERO_CDC_RX_RX0_RX_PATH_SEC1, 0x01, 0x01},
		{BOLERO_CDC_RX_RX1_RX_VOL_CTL, 0xff, 0xfd},
		{BOLERO_CDC_RX_RX1_RX_VOL_MIX_CTL, 0xff, 0xfd},
		{BOLERO_CDC_RX_RX1_RX_PATH_SEC1, 0x01, 0x01},
	},
};

enum {
	INTERP_HPHL,
	INTERP_HPHR,
	INTERP_AUX,
	INTERP_MAX
};

enum {
	RX_MACRO_RX0,
	RX_MACRO_RX1,
	RX_MACRO_RX2,
	RX_MACRO_RX3,
	RX_MACRO_RX4,
	RX_MACRO_RX5,
	RX_MACRO_PORTS_MAX
};

enum {
	RX_MACRO_COMP1, /* HPH_L */
	RX_MACRO_COMP2, /* HPH_R */
	RX_MACRO_COMP_MAX
};

enum {
	RX_MACRO_EC0_MUX = 0,
	RX_MACRO_EC1_MUX,
	RX_MACRO_EC2_MUX,
	RX_MACRO_EC_MUX_MAX,
};

enum {
	INTn_1_INP_SEL_ZERO = 0,
	INTn_1_INP_SEL_DEC0,
	INTn_1_INP_SEL_DEC1,
	INTn_1_INP_SEL_IIR0,
	INTn_1_INP_SEL_IIR1,
	INTn_1_INP_SEL_RX0,
	INTn_1_INP_SEL_RX1,
	INTn_1_INP_SEL_RX2,
	INTn_1_INP_SEL_RX3,
	INTn_1_INP_SEL_RX4,
	INTn_1_INP_SEL_RX5,
};

enum {
	INTn_2_INP_SEL_ZERO = 0,
	INTn_2_INP_SEL_RX0,
	INTn_2_INP_SEL_RX1,
	INTn_2_INP_SEL_RX2,
	INTn_2_INP_SEL_RX3,
	INTn_2_INP_SEL_RX4,
	INTn_2_INP_SEL_RX5,
};

enum {
	INTERP_MAIN_PATH,
	INTERP_MIX_PATH,
};

/* Codec supports 2 IIR filters */
enum {
	IIR0 = 0,
	IIR1,
	IIR_MAX,
};

/* Each IIR has 5 Filter Stages */
enum {
	BAND1 = 0,
	BAND2,
	BAND3,
	BAND4,
	BAND5,
	BAND_MAX,
};

struct rx_macro_idle_detect_config {
	u8 hph_idle_thr;
	u8 hph_idle_detect_en;
};

struct interp_sample_rate {
	int sample_rate;
	int rate_val;
};

static struct interp_sample_rate sr_val_tbl[] = {
	{8000, 0x0}, {16000, 0x1}, {32000, 0x3}, {48000, 0x4}, {96000, 0x5},
	{192000, 0x6}, {384000, 0x7}, {44100, 0x9}, {88200, 0xA},
	{176400, 0xB}, {352800, 0xC},
};

struct rx_macro_bcl_pmic_params {
	u8 id;
	u8 sid;
	u8 ppid;
};

static int rx_macro_core_vote(void *handle, bool enable);
static int rx_macro_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai);
static int rx_macro_get_channel_map(struct snd_soc_dai *dai,
				unsigned int *tx_num, unsigned int *tx_slot,
				unsigned int *rx_num, unsigned int *rx_slot);
static int rx_macro_digital_mute(struct snd_soc_dai *dai, int mute);
static int rx_macro_int_dem_inp_mux_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol);
static int rx_macro_mux_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol);
static int rx_macro_mux_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol);
static int rx_macro_enable_interp_clk(struct snd_soc_component *component,
				      int event, int interp_idx);

/* Hold instance to soundwire platform device */
struct rx_swr_ctrl_data {
	struct platform_device *rx_swr_pdev;
};

struct rx_swr_ctrl_platform_data {
	void *handle; /* holds codec private data */
	int (*read)(void *handle, int reg);
	int (*write)(void *handle, int reg, int val);
	int (*bulk_write)(void *handle, u32 *reg, u32 *val, size_t len);
	int (*clk)(void *handle, bool enable);
	int (*core_vote)(void *handle, bool enable);
	int (*handle_irq)(void *handle,
			  irqreturn_t (*swrm_irq_handler)(int irq,
							  void *data),
			  void *swrm_handle,
			  int action);
};

enum {
	RX_MACRO_AIF_INVALID = 0,
	RX_MACRO_AIF1_PB,
	RX_MACRO_AIF2_PB,
	RX_MACRO_AIF3_PB,
	RX_MACRO_AIF4_PB,
	RX_MACRO_AIF_ECHO,
	RX_MACRO_AIF5_PB,
	RX_MACRO_AIF6_PB,
	RX_MACRO_MAX_DAIS,
};

enum {
	RX_MACRO_AIF1_CAP = 0,
	RX_MACRO_AIF2_CAP,
	RX_MACRO_AIF3_CAP,
	RX_MACRO_MAX_AIF_CAP_DAIS
};
/*
 * @dev: rx macro device pointer
 * @comp_enabled: compander enable mixer value set
 * @prim_int_users: Users of interpolator
 * @rx_mclk_users: RX MCLK users count
 * @vi_feed_value: VI sense mask
 * @swr_clk_lock: to lock swr master clock operations
 * @swr_ctrl_data: SoundWire data structure
 * @swr_plat_data: Soundwire platform data
 * @rx_macro_add_child_devices_work: work for adding child devices
 * @rx_swr_gpio_p: used by pinctrl API
 * @component: codec handle
 */
struct rx_macro_priv {
	struct device *dev;
	int comp_enabled[RX_MACRO_COMP_MAX];
	/* Main path clock users count */
	int main_clk_users[INTERP_MAX];
	int rx_port_value[RX_MACRO_PORTS_MAX];
	u16 prim_int_users[INTERP_MAX];
	int rx_mclk_users;
	int swr_clk_users;
	bool dapm_mclk_enable;
	bool reset_swr;
	int clsh_users;
	int rx_mclk_cnt;
	bool is_native_on;
	bool is_ear_mode_on;
	bool dev_up;
	bool hph_pwr_mode;
	bool hph_hd2_mode;
	struct mutex mclk_lock;
	struct mutex swr_clk_lock;
	struct rx_swr_ctrl_data *swr_ctrl_data;
	struct rx_swr_ctrl_platform_data swr_plat_data;
	struct work_struct rx_macro_add_child_devices_work;
	struct device_node *rx_swr_gpio_p;
	struct snd_soc_component *component;
	unsigned long active_ch_mask[RX_MACRO_MAX_DAIS];
	unsigned long active_ch_cnt[RX_MACRO_MAX_DAIS];
	u16 bit_width[RX_MACRO_MAX_DAIS];
	char __iomem *rx_io_base;
	char __iomem *rx_mclk_mode_muxsel;
	struct rx_macro_idle_detect_config idle_det_cfg;
	u8 sidetone_coeff_array[IIR_MAX][BAND_MAX]
		[RX_MACRO_SIDETONE_IIR_COEFF_MAX * 4];

	struct platform_device *pdev_child_devices
			[RX_MACRO_CHILD_DEVICES_MAX];
	int child_count;
	int is_softclip_on;
	int is_aux_hpf_on;
	int softclip_clk_users;
	struct rx_macro_bcl_pmic_params bcl_pmic_params;
	u16 clk_id;
	u16 default_clk_id;
	int8_t rx0_gain_val;
	int8_t rx1_gain_val;
};

static struct snd_soc_dai_driver rx_macro_dai[];
static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);

static const char * const rx_int_mix_mux_text[] = {
	"ZERO", "RX0", "RX1", "RX2", "RX3", "RX4", "RX5"
};

static const char * const rx_prim_mix_text[] = {
	"ZERO", "DEC0", "DEC1", "IIR0", "IIR1", "RX0", "RX1", "RX2",
	"RX3", "RX4", "RX5"
};

static const char * const rx_sidetone_mix_text[] = {
	"ZERO", "SRC0", "SRC1", "SRC_SUM"
};

static const char * const iir_inp_mux_text[] = {
	"ZERO", "DEC0", "DEC1", "DEC2", "DEC3",
	"DUMMY_1", "DUMMY_2", "DUMMY_3", "DUMMY_4", "DUMMY_5",
	"RX0", "RX1", "RX2", "RX3", "RX4", "RX5"
};

static const char * const rx_int_dem_inp_mux_text[] = {
	"NORMAL_DSM_OUT", "CLSH_DSM_OUT",
};

static const char * const rx_int0_1_interp_mux_text[] = {
	"ZERO", "RX INT0_1 MIX1",
};

static const char * const rx_int1_1_interp_mux_text[] = {
	"ZERO", "RX INT1_1 MIX1",
};

static const char * const rx_int2_1_interp_mux_text[] = {
	"ZERO", "RX INT2_1 MIX1",
};

static const char * const rx_int0_2_interp_mux_text[] = {
	"ZERO", "RX INT0_2 MUX",
};

static const char * const rx_int1_2_interp_mux_text[] = {
	"ZERO", "RX INT1_2 MUX",
};

static const char * const rx_int2_2_interp_mux_text[] = {
	"ZERO", "RX INT2_2 MUX",
};

static const char *const rx_macro_mux_text[] = {
	"ZERO", "AIF1_PB", "AIF2_PB", "AIF3_PB", "AIF4_PB"
};

static const char *const rx_macro_ear_mode_text[] = {"OFF", "ON"};
static const struct soc_enum rx_macro_ear_mode_enum =
	SOC_ENUM_SINGLE_EXT(2, rx_macro_ear_mode_text);

static const char *const rx_macro_hph_hd2_mode_text[] = {"OFF", "ON"};
static const struct soc_enum rx_macro_hph_hd2_mode_enum =
	SOC_ENUM_SINGLE_EXT(2, rx_macro_hph_hd2_mode_text);

static const char *const rx_macro_hph_pwr_mode_text[] = {"ULP", "LOHIFI"};
static const struct soc_enum rx_macro_hph_pwr_mode_enum =
	SOC_ENUM_SINGLE_EXT(2, rx_macro_hph_pwr_mode_text);

static const char * const rx_macro_vbat_bcl_gsm_mode_text[] = {"OFF", "ON"};
static const struct soc_enum rx_macro_vbat_bcl_gsm_mode_enum =
	SOC_ENUM_SINGLE_EXT(2, rx_macro_vbat_bcl_gsm_mode_text);

static const struct snd_kcontrol_new rx_int2_1_vbat_mix_switch[] = {
	SOC_DAPM_SINGLE("RX AUX VBAT Enable", SND_SOC_NOPM, 0, 1, 0)
};

static const char * const hph_idle_detect_text[] = {"OFF", "ON"};

static SOC_ENUM_SINGLE_EXT_DECL(hph_idle_detect_enum, hph_idle_detect_text);

RX_MACRO_DAPM_ENUM(rx_int0_2, BOLERO_CDC_RX_INP_MUX_RX_INT0_CFG1, 0,
		rx_int_mix_mux_text);
RX_MACRO_DAPM_ENUM(rx_int1_2, BOLERO_CDC_RX_INP_MUX_RX_INT1_CFG1, 0,
		rx_int_mix_mux_text);
RX_MACRO_DAPM_ENUM(rx_int2_2, BOLERO_CDC_RX_INP_MUX_RX_INT2_CFG1, 0,
		rx_int_mix_mux_text);


RX_MACRO_DAPM_ENUM(rx_int0_1_mix_inp0, BOLERO_CDC_RX_INP_MUX_RX_INT0_CFG0, 0,
		rx_prim_mix_text);
RX_MACRO_DAPM_ENUM(rx_int0_1_mix_inp1, BOLERO_CDC_RX_INP_MUX_RX_INT0_CFG0, 4,
		rx_prim_mix_text);
RX_MACRO_DAPM_ENUM(rx_int0_1_mix_inp2, BOLERO_CDC_RX_INP_MUX_RX_INT0_CFG1, 4,
		rx_prim_mix_text);
RX_MACRO_DAPM_ENUM(rx_int1_1_mix_inp0, BOLERO_CDC_RX_INP_MUX_RX_INT1_CFG0, 0,
		rx_prim_mix_text);
RX_MACRO_DAPM_ENUM(rx_int1_1_mix_inp1, BOLERO_CDC_RX_INP_MUX_RX_INT1_CFG0, 4,
		rx_prim_mix_text);
RX_MACRO_DAPM_ENUM(rx_int1_1_mix_inp2, BOLERO_CDC_RX_INP_MUX_RX_INT1_CFG1, 4,
		rx_prim_mix_text);
RX_MACRO_DAPM_ENUM(rx_int2_1_mix_inp0, BOLERO_CDC_RX_INP_MUX_RX_INT2_CFG0, 0,
		rx_prim_mix_text);
RX_MACRO_DAPM_ENUM(rx_int2_1_mix_inp1, BOLERO_CDC_RX_INP_MUX_RX_INT2_CFG0, 4,
		rx_prim_mix_text);
RX_MACRO_DAPM_ENUM(rx_int2_1_mix_inp2, BOLERO_CDC_RX_INP_MUX_RX_INT2_CFG1, 4,
		rx_prim_mix_text);

RX_MACRO_DAPM_ENUM(rx_int0_mix2_inp, BOLERO_CDC_RX_INP_MUX_SIDETONE_SRC_CFG0, 2,
		rx_sidetone_mix_text);
RX_MACRO_DAPM_ENUM(rx_int1_mix2_inp, BOLERO_CDC_RX_INP_MUX_SIDETONE_SRC_CFG0, 4,
		rx_sidetone_mix_text);
RX_MACRO_DAPM_ENUM(rx_int2_mix2_inp, BOLERO_CDC_RX_INP_MUX_SIDETONE_SRC_CFG0, 6,
		rx_sidetone_mix_text);

RX_MACRO_DAPM_ENUM(iir0_inp0, BOLERO_CDC_RX_IIR_INP_MUX_IIR0_MIX_CFG0, 0,
	iir_inp_mux_text);
RX_MACRO_DAPM_ENUM(iir0_inp1, BOLERO_CDC_RX_IIR_INP_MUX_IIR0_MIX_CFG1, 0,
	iir_inp_mux_text);
RX_MACRO_DAPM_ENUM(iir0_inp2, BOLERO_CDC_RX_IIR_INP_MUX_IIR0_MIX_CFG2, 0,
	iir_inp_mux_text);
RX_MACRO_DAPM_ENUM(iir0_inp3, BOLERO_CDC_RX_IIR_INP_MUX_IIR0_MIX_CFG3, 0,
	iir_inp_mux_text);
RX_MACRO_DAPM_ENUM(iir1_inp0, BOLERO_CDC_RX_IIR_INP_MUX_IIR1_MIX_CFG0, 0,
	iir_inp_mux_text);
RX_MACRO_DAPM_ENUM(iir1_inp1, BOLERO_CDC_RX_IIR_INP_MUX_IIR1_MIX_CFG1, 0,
	iir_inp_mux_text);
RX_MACRO_DAPM_ENUM(iir1_inp2, BOLERO_CDC_RX_IIR_INP_MUX_IIR1_MIX_CFG2, 0,
	iir_inp_mux_text);
RX_MACRO_DAPM_ENUM(iir1_inp3, BOLERO_CDC_RX_IIR_INP_MUX_IIR1_MIX_CFG3, 0,
	iir_inp_mux_text);

RX_MACRO_DAPM_ENUM(rx_int0_1_interp, SND_SOC_NOPM, 0,
	rx_int0_1_interp_mux_text);
RX_MACRO_DAPM_ENUM(rx_int1_1_interp, SND_SOC_NOPM, 0,
	rx_int1_1_interp_mux_text);
RX_MACRO_DAPM_ENUM(rx_int2_1_interp, SND_SOC_NOPM, 0,
	rx_int2_1_interp_mux_text);

RX_MACRO_DAPM_ENUM(rx_int0_2_interp, SND_SOC_NOPM, 0,
	rx_int0_2_interp_mux_text);
RX_MACRO_DAPM_ENUM(rx_int1_2_interp, SND_SOC_NOPM, 0,
	rx_int1_2_interp_mux_text);
RX_MACRO_DAPM_ENUM(rx_int2_2_interp, SND_SOC_NOPM, 0,
	rx_int2_2_interp_mux_text);

RX_MACRO_DAPM_ENUM_EXT(rx_int0_dem_inp, BOLERO_CDC_RX_RX0_RX_PATH_CFG1, 0,
	rx_int_dem_inp_mux_text, snd_soc_dapm_get_enum_double,
	rx_macro_int_dem_inp_mux_put);
RX_MACRO_DAPM_ENUM_EXT(rx_int1_dem_inp, BOLERO_CDC_RX_RX1_RX_PATH_CFG1, 0,
	rx_int_dem_inp_mux_text, snd_soc_dapm_get_enum_double,
	rx_macro_int_dem_inp_mux_put);

RX_MACRO_DAPM_ENUM_EXT(rx_macro_rx0, SND_SOC_NOPM, 0, rx_macro_mux_text,
	rx_macro_mux_get, rx_macro_mux_put);
RX_MACRO_DAPM_ENUM_EXT(rx_macro_rx1, SND_SOC_NOPM, 0, rx_macro_mux_text,
	rx_macro_mux_get, rx_macro_mux_put);
RX_MACRO_DAPM_ENUM_EXT(rx_macro_rx2, SND_SOC_NOPM, 0, rx_macro_mux_text,
	rx_macro_mux_get, rx_macro_mux_put);
RX_MACRO_DAPM_ENUM_EXT(rx_macro_rx3, SND_SOC_NOPM, 0, rx_macro_mux_text,
	rx_macro_mux_get, rx_macro_mux_put);
RX_MACRO_DAPM_ENUM_EXT(rx_macro_rx4, SND_SOC_NOPM, 0, rx_macro_mux_text,
	rx_macro_mux_get, rx_macro_mux_put);
RX_MACRO_DAPM_ENUM_EXT(rx_macro_rx5, SND_SOC_NOPM, 0, rx_macro_mux_text,
	rx_macro_mux_get, rx_macro_mux_put);

static const char * const rx_echo_mux_text[] = {
	"ZERO", "RX_MIX0", "RX_MIX1", "RX_MIX2"
};

static const struct soc_enum rx_mix_tx2_mux_enum =
	SOC_ENUM_SINGLE(BOLERO_CDC_RX_INP_MUX_RX_MIX_CFG5, 0, 4,
			rx_echo_mux_text);

static const struct snd_kcontrol_new rx_mix_tx2_mux =
	SOC_DAPM_ENUM("RX MIX TX2_MUX Mux", rx_mix_tx2_mux_enum);

static const struct soc_enum rx_mix_tx1_mux_enum =
	SOC_ENUM_SINGLE(BOLERO_CDC_RX_INP_MUX_RX_MIX_CFG4, 0, 4,
			rx_echo_mux_text);

static const struct snd_kcontrol_new rx_mix_tx1_mux =
	SOC_DAPM_ENUM("RX MIX TX1_MUX Mux", rx_mix_tx1_mux_enum);

static const struct soc_enum rx_mix_tx0_mux_enum =
	SOC_ENUM_SINGLE(BOLERO_CDC_RX_INP_MUX_RX_MIX_CFG4, 4, 4,
			rx_echo_mux_text);

static const struct snd_kcontrol_new rx_mix_tx0_mux =
	SOC_DAPM_ENUM("RX MIX TX0_MUX Mux", rx_mix_tx0_mux_enum);

static struct snd_soc_dai_ops rx_macro_dai_ops = {
	.hw_params = rx_macro_hw_params,
	.get_channel_map = rx_macro_get_channel_map,
	.digital_mute = rx_macro_digital_mute,
};

static struct snd_soc_dai_driver rx_macro_dai[] = {
	{
		.name = "rx_macro_rx1",
		.id = RX_MACRO_AIF1_PB,
		.playback = {
			.stream_name = "RX_MACRO_AIF1 Playback",
			.rates = RX_MACRO_RATES | RX_MACRO_FRAC_RATES,
			.formats = RX_MACRO_FORMATS,
			.rate_max = 384000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &rx_macro_dai_ops,
	},
	{
		.name = "rx_macro_rx2",
		.id = RX_MACRO_AIF2_PB,
		.playback = {
			.stream_name = "RX_MACRO_AIF2 Playback",
			.rates = RX_MACRO_RATES | RX_MACRO_FRAC_RATES,
			.formats = RX_MACRO_FORMATS,
			.rate_max = 384000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &rx_macro_dai_ops,
	},
	{
		.name = "rx_macro_rx3",
		.id = RX_MACRO_AIF3_PB,
		.playback = {
			.stream_name = "RX_MACRO_AIF3 Playback",
			.rates = RX_MACRO_RATES | RX_MACRO_FRAC_RATES,
			.formats = RX_MACRO_FORMATS,
			.rate_max = 384000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &rx_macro_dai_ops,
	},
	{
		.name = "rx_macro_rx4",
		.id = RX_MACRO_AIF4_PB,
		.playback = {
			.stream_name = "RX_MACRO_AIF4 Playback",
			.rates = RX_MACRO_RATES | RX_MACRO_FRAC_RATES,
			.formats = RX_MACRO_FORMATS,
			.rate_max = 384000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &rx_macro_dai_ops,
	},
	{
		.name = "rx_macro_echo",
		.id = RX_MACRO_AIF_ECHO,
		.capture = {
			.stream_name = "RX_AIF_ECHO Capture",
			.rates = RX_MACRO_ECHO_RATES,
			.formats = RX_MACRO_ECHO_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 3,
		},
		.ops = &rx_macro_dai_ops,
	},
	{
		.name = "rx_macro_rx5",
		.id = RX_MACRO_AIF5_PB,
		.playback = {
			.stream_name = "RX_MACRO_AIF5 Playback",
			.rates = RX_MACRO_RATES | RX_MACRO_FRAC_RATES,
			.formats = RX_MACRO_FORMATS,
			.rate_max = 384000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &rx_macro_dai_ops,
	},
	{
		.name = "rx_macro_rx6",
		.id = RX_MACRO_AIF6_PB,
		.playback = {
			.stream_name = "RX_MACRO_AIF6 Playback",
			.rates = RX_MACRO_RATES | RX_MACRO_FRAC_RATES,
			.formats = RX_MACRO_FORMATS,
			.rate_max = 384000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &rx_macro_dai_ops,
	},
};

static int get_impedance_index(int imped)
{
	int i = 0;

	if (imped < imped_index[i].imped_val) {
		pr_debug("%s, detected impedance is less than %d Ohm\n",
			__func__, imped_index[i].imped_val);
		i = 0;
		goto ret;
	}
	if (imped >= imped_index[ARRAY_SIZE(imped_index) - 1].imped_val) {
		pr_debug("%s, detected impedance is greater than %d Ohm\n",
			__func__,
			imped_index[ARRAY_SIZE(imped_index) - 1].imped_val);
		i = ARRAY_SIZE(imped_index) - 1;
		goto ret;
	}
	for (i = 0; i < ARRAY_SIZE(imped_index) - 1; i++) {
		if (imped >= imped_index[i].imped_val &&
			imped < imped_index[i + 1].imped_val)
			break;
	}
ret:
	pr_debug("%s: selected impedance index = %d\n",
			__func__, imped_index[i].index);
	return imped_index[i].index;
}

/*
 * rx_macro_wcd_clsh_imped_config -
 * This function updates HPHL and HPHR gain settings
 * according to the impedance value.
 *
 * @component: codec pointer handle
 * @imped: impedance value of HPHL/R
 * @reset: bool variable to reset registers when teardown
 */
static void rx_macro_wcd_clsh_imped_config(struct snd_soc_component *component,
					   int imped, bool reset)
{
	int i;
	int index = 0;
	int table_size;

	static const struct rx_macro_reg_mask_val
				(*imped_table_ptr)[MAX_IMPED_PARAMS];

	table_size = ARRAY_SIZE(imped_table);
	imped_table_ptr = imped_table;
	/* reset = 1, which means request is to reset the register values */
	if (reset) {
		for (i = 0; i < MAX_IMPED_PARAMS; i++)
			snd_soc_component_update_bits(component,
				imped_table_ptr[index][i].reg,
				imped_table_ptr[index][i].mask, 0);
		return;
	}
	index = get_impedance_index(imped);
	if (index >= (ARRAY_SIZE(imped_index) - 1)) {
		pr_debug("%s, impedance not in range = %d\n", __func__, imped);
		return;
	}
	if (index >= table_size) {
		pr_debug("%s, impedance index not in range = %d\n", __func__,
			index);
		return;
	}
	for (i = 0; i < MAX_IMPED_PARAMS; i++)
		snd_soc_component_update_bits(component,
				imped_table_ptr[index][i].reg,
				imped_table_ptr[index][i].mask,
				imped_table_ptr[index][i].val);
}

static bool rx_macro_get_data(struct snd_soc_component *component,
			       struct device **rx_dev,
			       struct rx_macro_priv **rx_priv,
			       const char *func_name)
{
	*rx_dev = bolero_get_device_ptr(component->dev, RX_MACRO);

	if (!(*rx_dev)) {
		dev_err(component->dev,
			"%s: null device for macro!\n", func_name);
		return false;
	}

	*rx_priv = dev_get_drvdata((*rx_dev));
	if (!(*rx_priv)) {
		dev_err(component->dev,
			"%s: priv is null for macro!\n", func_name);
		return false;
	}

	if (!(*rx_priv)->component) {
		dev_err(component->dev,
			"%s: rx_priv component is not initialized!\n", func_name);
		return false;
	}

	return true;
}

static int rx_macro_set_port_map(struct snd_soc_component *component,
				u32 usecase, u32 size, void *data)
{
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;
	struct swrm_port_config port_cfg;
	int ret = 0;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	memset(&port_cfg, 0, sizeof(port_cfg));
	port_cfg.uc = usecase;
	port_cfg.size = size;
	port_cfg.params = data;

	if (rx_priv->swr_ctrl_data)
		ret = swrm_wcd_notify(
			rx_priv->swr_ctrl_data[0].rx_swr_pdev,
			SWR_SET_PORT_MAP, &port_cfg);

	return ret;
}

static int rx_macro_int_dem_inp_mux_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(widget->dapm);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val = 0;
	unsigned short look_ahead_dly_reg =
				BOLERO_CDC_RX_RX0_RX_PATH_CFG0;

	val = ucontrol->value.enumerated.item[0];
	if (val >= e->items)
		return -EINVAL;

	dev_dbg(component->dev, "%s: wname: %s, val: 0x%x\n", __func__,
		widget->name, val);

	if (e->reg == BOLERO_CDC_RX_RX0_RX_PATH_CFG1)
		look_ahead_dly_reg = BOLERO_CDC_RX_RX0_RX_PATH_CFG0;
	else if (e->reg == BOLERO_CDC_RX_RX1_RX_PATH_CFG1)
		look_ahead_dly_reg = BOLERO_CDC_RX_RX1_RX_PATH_CFG0;

	/* Set Look Ahead Delay */
	snd_soc_component_update_bits(component, look_ahead_dly_reg,
			    0x08, (val ? 0x08 : 0x00));
	/* Set DEM INP Select */
	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

static int rx_macro_set_prim_interpolator_rate(struct snd_soc_dai *dai,
					    u8 rate_reg_val,
					    u32 sample_rate)
{
	u8 int_1_mix1_inp = 0;
	u32 j = 0, k = 0, port = 0;
	u16 int_mux_cfg0 = 0, int_mux_cfg1 = 0, iir_mux_cfg = 0;
	u32 iir_mux_cfg_val = 0;
	u16 int_fs_reg = 0;
	u8 int_mux_cfg0_val = 0, int_mux_cfg1_val = 0;
	u8 inp0_sel = 0, inp1_sel = 0, inp2_sel = 0;
	struct snd_soc_component *component = dai->component;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	for_each_set_bit(port, &rx_priv->active_ch_mask[dai->id],
			 RX_MACRO_PORTS_MAX) {
		int_1_mix1_inp = port;
		if ((int_1_mix1_inp < RX_MACRO_RX0) ||
			(int_1_mix1_inp > RX_MACRO_PORTS_MAX)) {
			pr_err("%s: Invalid RX port, Dai ID is %d\n",
				__func__, dai->id);
			return -EINVAL;
		}

		int_mux_cfg0 = BOLERO_CDC_RX_INP_MUX_RX_INT0_CFG0;

		/*
		 * Loop through all interpolator MUX inputs and find out
		 * to which interpolator input, the rx port
		 * is connected
		 */
		for (j = 0; j < INTERP_MAX; j++) {
			int_mux_cfg1 = int_mux_cfg0 + 4;

			int_mux_cfg0_val = snd_soc_component_read32(
						component, int_mux_cfg0);
			int_mux_cfg1_val = snd_soc_component_read32(
						component, int_mux_cfg1);
			inp0_sel = int_mux_cfg0_val & 0x0F;
			inp1_sel = (int_mux_cfg0_val >> 4) & 0x0F;
			inp2_sel = (int_mux_cfg1_val >> 4) & 0x0F;
			if ((inp0_sel == int_1_mix1_inp + INTn_1_INP_SEL_RX0) ||
			    (inp1_sel == int_1_mix1_inp + INTn_1_INP_SEL_RX0) ||
			    (inp2_sel == int_1_mix1_inp + INTn_1_INP_SEL_RX0)) {
				int_fs_reg = BOLERO_CDC_RX_RX0_RX_PATH_CTL +
					     0x80 * j;
				pr_debug("%s: AIF_PB DAI(%d) connected to INT%u_1\n",
					  __func__, dai->id, j);
				pr_debug("%s: set INT%u_1 sample rate to %u, rate_reg=%d\n",
					__func__, j, sample_rate, rate_reg_val);
				/* sample_rate is in Hz */
				snd_soc_component_update_bits(component,
						int_fs_reg,
						0x0F, rate_reg_val);
			} else if ((inp0_sel == INTn_1_INP_SEL_IIR0) ||
				  (inp1_sel == INTn_1_INP_SEL_IIR0) ||
				  (inp2_sel == INTn_1_INP_SEL_IIR0)) {
				for (k = 0; k < IIR_MIX_CFG_MAX; k++) {
					iir_mux_cfg =
					BOLERO_CDC_RX_IIR_INP_MUX_IIR0_MIX_CFG0
					+ 4 * k;
					iir_mux_cfg_val =
					snd_soc_component_read32(component,
						iir_mux_cfg) & 0x1F;

					if (iir_mux_cfg_val == int_1_mix1_inp
					    + IIR_MIX_CFG_OFFSET){
						int_fs_reg =
						BOLERO_CDC_RX_RX0_RX_PATH_CTL +
						0x80 * j;
						pr_debug("%s: AIF_PB DAI(%d) connected to INT%u_1 via IIR0\n",
							 __func__, dai->id, j);
						pr_debug("%s: set INT%u_1 sample rate to %u\n",
							 __func__, j, sample_rate);
						/* sample_rate is in Hz */
						snd_soc_component_update_bits(component,
							int_fs_reg,
							0x0F, rate_reg_val);
					}
				}
			} else if ((inp0_sel == INTn_1_INP_SEL_IIR1) ||
				  (inp1_sel == INTn_1_INP_SEL_IIR1) ||
				   (inp2_sel == INTn_1_INP_SEL_IIR1)) {
				for (k = 0; k < IIR_MIX_CFG_MAX; k++) {
					iir_mux_cfg =
					BOLERO_CDC_RX_IIR_INP_MUX_IIR1_MIX_CFG0
					+ 4 * k;
					iir_mux_cfg_val =
					snd_soc_component_read32(
					component, iir_mux_cfg) & 0x1F;

					if (iir_mux_cfg_val == int_1_mix1_inp
					    + IIR_MIX_CFG_OFFSET){
						int_fs_reg =
						BOLERO_CDC_RX_RX0_RX_PATH_CTL +
						0x80 * j;
						pr_debug("%s: AIF_PB DAI(%d) connected to INT%u_1 via IIR1\n",
							 __func__, dai->id, j);
						pr_debug("%s: set INT%u_1 sample rate to %u\n",
							 __func__, j, sample_rate);
						/* sample_rate is in Hz */
						snd_soc_component_update_bits(
							component, int_fs_reg,
							0x0F, rate_reg_val);
			    		}
				}
			}
			int_mux_cfg0 += 8;
		}
	}

	return 0;
}

static int rx_macro_set_mix_interpolator_rate(struct snd_soc_dai *dai,
					u8 rate_reg_val,
					u32 sample_rate)
{
	u8 int_2_inp = 0;
	u32 j = 0, port = 0;
	u16 int_mux_cfg1 = 0, int_fs_reg = 0;
	u8 int_mux_cfg1_val = 0;
	struct snd_soc_component *component = dai->component;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	for_each_set_bit(port, &rx_priv->active_ch_mask[dai->id],
			 RX_MACRO_PORTS_MAX) {
		int_2_inp = port;
		if ((int_2_inp < RX_MACRO_RX0) ||
			(int_2_inp > RX_MACRO_PORTS_MAX)) {
			pr_err("%s: Invalid RX port, Dai ID is %d\n",
				__func__, dai->id);
			return -EINVAL;
		}

		int_mux_cfg1 = BOLERO_CDC_RX_INP_MUX_RX_INT0_CFG1;
		for (j = 0; j < INTERP_MAX; j++) {
			int_mux_cfg1_val = snd_soc_component_read32(
						component, int_mux_cfg1) &
						0x0F;
			if (int_mux_cfg1_val == int_2_inp +
							INTn_2_INP_SEL_RX0) {
				int_fs_reg = BOLERO_CDC_RX_RX0_RX_PATH_MIX_CTL +
						0x80 * j;
				pr_debug("%s: AIF_PB DAI(%d) connected to INT%u_2\n",
					  __func__, dai->id, j);
				pr_debug("%s: set INT%u_2 sample rate to %u\n",
					__func__, j, sample_rate);
				snd_soc_component_update_bits(
						component, int_fs_reg,
						0x0F, rate_reg_val);
			}
			int_mux_cfg1 += 8;
		}
	}
	return 0;
}

static bool rx_macro_is_fractional_sample_rate(u32 sample_rate)
{
	switch (sample_rate) {
	case SAMPLING_RATE_44P1KHZ:
	case SAMPLING_RATE_88P2KHZ:
	case SAMPLING_RATE_176P4KHZ:
	case SAMPLING_RATE_352P8KHZ:
		return true;
	default:
		return false;
	}
	return false;
}

static int rx_macro_set_interpolator_rate(struct snd_soc_dai *dai,
					  u32 sample_rate)
{
	struct snd_soc_component *component = dai->component;
	int rate_val = 0;
	int i = 0, ret = 0;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;


	for (i = 0; i < ARRAY_SIZE(sr_val_tbl); i++) {
		if (sample_rate == sr_val_tbl[i].sample_rate) {
			rate_val = sr_val_tbl[i].rate_val;
			if (rx_macro_is_fractional_sample_rate(sample_rate))
				rx_priv->is_native_on = true;
			else
				rx_priv->is_native_on = false;
			break;
		}
	}
	if ((i == ARRAY_SIZE(sr_val_tbl)) || (rate_val < 0)) {
		dev_err(component->dev, "%s: Unsupported sample rate: %d\n",
			__func__, sample_rate);
		return -EINVAL;
	}

	ret = rx_macro_set_prim_interpolator_rate(dai, (u8)rate_val, sample_rate);
	if (ret)
		return ret;
	ret = rx_macro_set_mix_interpolator_rate(dai, (u8)rate_val, sample_rate);
	if (ret)
		return ret;

	return ret;
}

static int rx_macro_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	int ret = 0;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	dev_dbg(component->dev,
		"%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n", __func__,
		 dai->name, dai->id, params_rate(params),
		 params_channels(params));

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		ret = rx_macro_set_interpolator_rate(dai, params_rate(params));
		if (ret) {
			pr_err("%s: cannot set sample rate: %u\n",
				__func__, params_rate(params));
			return ret;
		}
		rx_priv->bit_width[dai->id] = params_width(params);
		break;
	case SNDRV_PCM_STREAM_CAPTURE:
	default:
		break;
	}
	return 0;
}

static int rx_macro_get_channel_map(struct snd_soc_dai *dai,
				unsigned int *tx_num, unsigned int *tx_slot,
				unsigned int *rx_num, unsigned int *rx_slot)
{
	struct snd_soc_component *component = dai->component;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;
	unsigned int temp = 0, ch_mask = 0;
	u16 val = 0, mask = 0, cnt = 0, i = 0;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	switch (dai->id) {
	case RX_MACRO_AIF1_PB:
	case RX_MACRO_AIF2_PB:
	case RX_MACRO_AIF3_PB:
	case RX_MACRO_AIF4_PB:
		for_each_set_bit(temp, &rx_priv->active_ch_mask[dai->id],
			 RX_MACRO_PORTS_MAX) {
			ch_mask |= (1 << temp);
			if (++i == RX_MACRO_MAX_DMA_CH_PER_PORT)
				break;
		}
		/*
		 * CDC_DMA_RX_0 port drives RX0/RX1 -- ch_mask 0x1/0x2/0x3
		 * CDC_DMA_RX_1 port drives RX2/RX3 -- ch_mask 0x1/0x2/0x3
		 * CDC_DMA_RX_2 port drives RX4     -- ch_mask 0x1
		 * CDC_DMA_RX_3 port drives RX5     -- ch_mask 0x1
		 * AIFn can pair to any CDC_DMA_RX_n port.
		 * In general, below convention is used::
		 * CDC_DMA_RX_0(AIF1)/CDC_DMA_RX_1(AIF2)/
		 * CDC_DMA_RX_2(AIF3)/CDC_DMA_RX_3(AIF4)
		 * Above is reflected in machine driver BE dailink
		 */
		if (ch_mask & 0x0C)
			ch_mask = ch_mask >> 2;
		if ((ch_mask & 0x10) || (ch_mask & 0x20))
			ch_mask = 0x1;
		*rx_slot = ch_mask;
		*rx_num = rx_priv->active_ch_cnt[dai->id];
		dev_err(rx_priv->dev,
			"%s: dai->id:%d(%s) ch_mask:0x%x active_ch_cnt:%d active_mask: 0x%lx\n",
			__func__, dai->id, dai->name, *rx_slot, *rx_num, rx_priv->active_ch_mask[dai->id]);
		break;
	case RX_MACRO_AIF5_PB:
		*rx_slot = 0x1;
		*rx_num = 0x01;
		dev_dbg(rx_priv->dev,
			"%s: dai->id:%d, ch_mask:0x%x, active_ch_cnt:%d\n",
			__func__, dai->id, *rx_slot, *rx_num);
		break;
	case RX_MACRO_AIF6_PB:
		*rx_slot = 0x1;
		*rx_num = 0x01;
		dev_dbg(rx_priv->dev,
			"%s: dai->id:%d, ch_mask:0x%x, active_ch_cnt:%d\n",
			__func__, dai->id, *rx_slot, *rx_num);
		break;
	case RX_MACRO_AIF_ECHO:
		val = snd_soc_component_read32(component,
			BOLERO_CDC_RX_INP_MUX_RX_MIX_CFG4);
		if (val & RX_MACRO_EC_MIX_TX0_MASK) {
			mask |= 0x1;
			cnt++;
		}
		if (val & RX_MACRO_EC_MIX_TX1_MASK) {
			mask |= 0x2;
			cnt++;
		}
		val = snd_soc_component_read32(component,
			BOLERO_CDC_RX_INP_MUX_RX_MIX_CFG5);
		if (val & RX_MACRO_EC_MIX_TX2_MASK) {
			mask |= 0x4;
			cnt++;
		}
		*tx_slot = mask;
		*tx_num = cnt;
		break;
	default:
		dev_err(rx_dev, "%s: Invalid AIF\n", __func__);
		break;
	}
	return 0;
}

static int rx_macro_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_component *component = dai->component;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;
	uint16_t j = 0, reg = 0, mix_reg = 0, dsm_reg = 0;
	u16 int_mux_cfg0 = 0, int_mux_cfg1 = 0;
	u8 int_mux_cfg0_val = 0, int_mux_cfg1_val = 0;

	if (mute)
		return 0;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	switch (dai->id) {
	case RX_MACRO_AIF1_PB:
	case RX_MACRO_AIF2_PB:
	case RX_MACRO_AIF3_PB:
	case RX_MACRO_AIF4_PB:
	for (j = 0; j < INTERP_MAX; j++) {
		reg = BOLERO_CDC_RX_RX0_RX_PATH_CTL +
				(j * RX_MACRO_RX_PATH_OFFSET);
		mix_reg = BOLERO_CDC_RX_RX0_RX_PATH_MIX_CTL +
				(j * RX_MACRO_RX_PATH_OFFSET);
		dsm_reg = BOLERO_CDC_RX_RX0_RX_PATH_DSM_CTL +
				(j * RX_MACRO_RX_PATH_OFFSET);
		if (j == INTERP_AUX)
			dsm_reg = BOLERO_CDC_RX_RX2_RX_PATH_DSM_CTL;
		int_mux_cfg0 = BOLERO_CDC_RX_INP_MUX_RX_INT0_CFG0 + j * 8;
		int_mux_cfg1 = int_mux_cfg0 + 4;
		int_mux_cfg0_val = snd_soc_component_read32(component,
							int_mux_cfg0);
		int_mux_cfg1_val = snd_soc_component_read32(component,
							int_mux_cfg1);
		if (snd_soc_component_read32(component, dsm_reg) & 0x01) {
			if (int_mux_cfg0_val || (int_mux_cfg1_val & 0xF0))
				snd_soc_component_update_bits(component,
							reg, 0x20, 0x20);
			if (int_mux_cfg1_val & 0x0F) {
				snd_soc_component_update_bits(component,
							reg, 0x20, 0x20);
				snd_soc_component_update_bits(component,
							mix_reg, 0x20, 0x20);
			}
		}
	}
		break;
	default:
		break;
	}
	return 0;
}

static int rx_macro_mclk_enable(struct rx_macro_priv *rx_priv,
				 bool mclk_enable, bool dapm)
{
	struct regmap *regmap = dev_get_regmap(rx_priv->dev->parent, NULL);
	int ret = 0;

	if (regmap == NULL) {
		dev_err(rx_priv->dev, "%s: regmap is NULL\n", __func__);
		return -EINVAL;
	}

	dev_dbg(rx_priv->dev, "%s: mclk_enable = %u, dapm = %d clk_users= %d\n",
		__func__, mclk_enable, dapm, rx_priv->rx_mclk_users);

	mutex_lock(&rx_priv->mclk_lock);
	if (mclk_enable) {
		if (rx_priv->rx_mclk_users == 0) {
			if (rx_priv->is_native_on)
				rx_priv->clk_id = RX_CORE_CLK;
			rx_macro_core_vote(rx_priv, true);
			ret = bolero_clk_rsc_request_clock(rx_priv->dev,
							   rx_priv->default_clk_id,
							   rx_priv->clk_id,
							   true);
			rx_macro_core_vote(rx_priv, false);
			if (ret < 0) {
				dev_err(rx_priv->dev,
					"%s: rx request clock enable failed\n",
					__func__);
				goto exit;
			}
			bolero_clk_rsc_fs_gen_request(rx_priv->dev,
							true);
			regcache_mark_dirty(regmap);
			regcache_sync_region(regmap,
					RX_START_OFFSET,
					RX_MAX_OFFSET);
			regmap_update_bits(regmap,
				BOLERO_CDC_RX_CLK_RST_CTRL_MCLK_CONTROL,
				0x01, 0x01);
			regmap_update_bits(regmap,
				BOLERO_CDC_RX_CLK_RST_CTRL_MCLK_CONTROL,
				0x02, 0x02);
			regmap_update_bits(regmap,
				BOLERO_CDC_RX_CLK_RST_CTRL_FS_CNT_CONTROL,
				0x02, 0x00);
			regmap_update_bits(regmap,
				BOLERO_CDC_RX_CLK_RST_CTRL_FS_CNT_CONTROL,
				0x01, 0x01);
		}
		rx_priv->rx_mclk_users++;
	} else {
		if (rx_priv->rx_mclk_users <= 0) {
			dev_err(rx_priv->dev, "%s: clock already disabled\n",
				__func__);
			rx_priv->rx_mclk_users = 0;
			goto exit;
		}
		rx_priv->rx_mclk_users--;
		if (rx_priv->rx_mclk_users == 0) {
			regmap_update_bits(regmap,
				BOLERO_CDC_RX_CLK_RST_CTRL_FS_CNT_CONTROL,
				0x01, 0x00);
			regmap_update_bits(regmap,
				BOLERO_CDC_RX_CLK_RST_CTRL_FS_CNT_CONTROL,
				0x02, 0x02);
			regmap_update_bits(regmap,
				BOLERO_CDC_RX_CLK_RST_CTRL_MCLK_CONTROL,
				0x02, 0x00);
			regmap_update_bits(regmap,
				BOLERO_CDC_RX_CLK_RST_CTRL_MCLK_CONTROL,
				0x01, 0x00);
			bolero_clk_rsc_fs_gen_request(rx_priv->dev,
			   false);
			rx_macro_core_vote(rx_priv, true);
			bolero_clk_rsc_request_clock(rx_priv->dev,
						 rx_priv->default_clk_id,
						 rx_priv->clk_id,
						 false);
			rx_macro_core_vote(rx_priv, false);
			rx_priv->clk_id = rx_priv->default_clk_id;
		}
	}
exit:
	trace_printk("%s: mclk_enable = %u, dapm = %d clk_users= %d\n",
		__func__, mclk_enable, dapm, rx_priv->rx_mclk_users);
	mutex_unlock(&rx_priv->mclk_lock);
	return ret;
}

static int rx_macro_mclk_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	int ret = 0;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;
	int mclk_freq = MCLK_FREQ;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	dev_dbg(rx_dev, "%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (rx_priv->is_native_on)
			mclk_freq = MCLK_FREQ_NATIVE;
		if (rx_priv->swr_ctrl_data)
			swrm_wcd_notify(
				rx_priv->swr_ctrl_data[0].rx_swr_pdev,
				SWR_CLK_FREQ, &mclk_freq);
		ret = rx_macro_mclk_enable(rx_priv, 1, true);
		if (ret)
			rx_priv->dapm_mclk_enable = false;
		else
			rx_priv->dapm_mclk_enable = true;
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (rx_priv->dapm_mclk_enable)
			ret = rx_macro_mclk_enable(rx_priv, 0, true);
		break;
	default:
		dev_err(rx_priv->dev,
			"%s: invalid DAPM event %d\n", __func__, event);
		ret = -EINVAL;
	}
	return ret;
}

static int rx_macro_event_handler(struct snd_soc_component *component,
				  u16 event, u32 data)
{
	u16 reg = 0, reg_mix = 0, rx_idx = 0, mute = 0x0, val = 0;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;
	int ret = 0;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	switch (event) {
	case BOLERO_MACRO_EVT_RX_MUTE:
		rx_idx = data >> 0x10;
		mute = data & 0xffff;
		val = mute ? 0x10 : 0x00;
		reg = BOLERO_CDC_RX_RX0_RX_PATH_CTL + (rx_idx *
					RX_MACRO_RX_PATH_OFFSET);
		reg_mix = BOLERO_CDC_RX_RX0_RX_PATH_MIX_CTL + (rx_idx *
					RX_MACRO_RX_PATH_OFFSET);
		snd_soc_component_update_bits(component, reg,
				0x10, val);
		snd_soc_component_update_bits(component, reg_mix,
				0x10, val);
		break;
	case BOLERO_MACRO_EVT_RX_COMPANDER_SOFT_RST:
		rx_idx = data >> 0x10;
		if (rx_idx == INTERP_AUX)
			goto done;
		reg = BOLERO_CDC_RX_COMPANDER0_CTL0 +
				(rx_idx * RX_MACRO_COMP_OFFSET);
		snd_soc_component_write(component, reg,
				snd_soc_component_read32(component, reg));
		break;
	case BOLERO_MACRO_EVT_IMPED_TRUE:
		rx_macro_wcd_clsh_imped_config(component, data, true);
		break;
	case BOLERO_MACRO_EVT_IMPED_FALSE:
		rx_macro_wcd_clsh_imped_config(component, data, false);
		break;
	case BOLERO_MACRO_EVT_SSR_DOWN:
		trace_printk("%s, enter SSR down\n", __func__);
		rx_priv->dev_up = false;
		if (rx_priv->swr_ctrl_data) {
			swrm_wcd_notify(
				rx_priv->swr_ctrl_data[0].rx_swr_pdev,
				SWR_DEVICE_SSR_DOWN, NULL);
		}
		if ((!pm_runtime_enabled(rx_dev) ||
		     !pm_runtime_suspended(rx_dev))) {
			ret = bolero_runtime_suspend(rx_dev);
			if (!ret) {
				pm_runtime_disable(rx_dev);
				pm_runtime_set_suspended(rx_dev);
				pm_runtime_enable(rx_dev);
			}
		}
		break;
	case BOLERO_MACRO_EVT_PRE_SSR_UP:
		rx_macro_core_vote(rx_priv, true);
		/* enable&disable RX_CORE_CLK to reset GFMUX reg */
		ret = bolero_clk_rsc_request_clock(rx_priv->dev,
						rx_priv->default_clk_id,
						RX_CORE_CLK, true);
		if (ret < 0) {
			dev_err_ratelimited(rx_priv->dev,
				"%s, failed to enable clk, ret:%d\n",
				__func__, ret);
		} else {
			bolero_clk_rsc_request_clock(rx_priv->dev,
						rx_priv->default_clk_id,
						RX_CORE_CLK, false);
		}
		rx_macro_core_vote(rx_priv, false);
		break;
	case BOLERO_MACRO_EVT_SSR_UP:
		trace_printk("%s, enter SSR up\n", __func__);
		rx_priv->dev_up = true;
		/* reset swr after ssr/pdr */
		rx_priv->reset_swr = true;

		if (rx_priv->swr_ctrl_data)
			swrm_wcd_notify(
				rx_priv->swr_ctrl_data[0].rx_swr_pdev,
				SWR_DEVICE_SSR_UP, NULL);
		break;
	case BOLERO_MACRO_EVT_CLK_RESET:
		bolero_rsc_clk_reset(rx_dev, RX_CORE_CLK);
		break;
	case BOLERO_MACRO_EVT_RX_PA_GAIN_UPDATE:
		rx_priv->rx0_gain_val = snd_soc_component_read32(component,
					BOLERO_CDC_RX_RX0_RX_VOL_CTL);
		rx_priv->rx1_gain_val = snd_soc_component_read32(component,
					BOLERO_CDC_RX_RX1_RX_VOL_CTL);
		if (data) {
			/* Reduce gain by half only if its greater than -6DB */
			if ((rx_priv->rx0_gain_val >= RX_MACRO_GAIN_VAL_UNITY)
			&& (rx_priv->rx0_gain_val <= RX_MACRO_GAIN_MAX_VAL))
				snd_soc_component_update_bits(component,
					BOLERO_CDC_RX_RX0_RX_VOL_CTL, 0xFF,
					(rx_priv->rx0_gain_val -
					 RX_MACRO_MOD_GAIN));
			if ((rx_priv->rx1_gain_val >= RX_MACRO_GAIN_VAL_UNITY)
			&& (rx_priv->rx1_gain_val <= RX_MACRO_GAIN_MAX_VAL))
				snd_soc_component_update_bits(component,
					BOLERO_CDC_RX_RX1_RX_VOL_CTL, 0xFF,
					(rx_priv->rx1_gain_val -
					 RX_MACRO_MOD_GAIN));
		}
		else {
			/* Reset gain value to default */
			if ((rx_priv->rx0_gain_val >=
			    (RX_MACRO_GAIN_VAL_UNITY - RX_MACRO_MOD_GAIN)) &&
			    (rx_priv->rx0_gain_val <= (RX_MACRO_GAIN_MAX_VAL -
			    RX_MACRO_MOD_GAIN)))
				snd_soc_component_update_bits(component,
					BOLERO_CDC_RX_RX0_RX_VOL_CTL, 0xFF,
					(rx_priv->rx0_gain_val +
					 RX_MACRO_MOD_GAIN));
			if ((rx_priv->rx1_gain_val >=
			    (RX_MACRO_GAIN_VAL_UNITY - RX_MACRO_MOD_GAIN)) &&
			    (rx_priv->rx1_gain_val <= (RX_MACRO_GAIN_MAX_VAL -
			    RX_MACRO_MOD_GAIN)))
				snd_soc_component_update_bits(component,
					BOLERO_CDC_RX_RX1_RX_VOL_CTL, 0xFF,
					(rx_priv->rx1_gain_val +
					 RX_MACRO_MOD_GAIN));
		}
		break;
	case BOLERO_MACRO_EVT_HPHL_HD2_ENABLE:
		/* Enable hd2 config for hphl*/
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_RX0_RX_PATH_CFG0, 0x04, data);
		break;
	case BOLERO_MACRO_EVT_HPHR_HD2_ENABLE:
		/* Enable hd2 config for hphr*/
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_RX1_RX_PATH_CFG0, 0x04, data);
		break;
	}
done:
	return ret;
}

static int rx_macro_find_playback_dai_id_for_port(int port_id,
						  struct rx_macro_priv *rx_priv)
{
	int i = 0;

	for (i = RX_MACRO_AIF1_PB; i < RX_MACRO_MAX_DAIS; i++) {
		if (test_bit(port_id, &rx_priv->active_ch_mask[i]))
			return i;
	}

	return -EINVAL;
}

static int rx_macro_set_idle_detect_thr(struct snd_soc_component *component,
					struct rx_macro_priv *rx_priv,
					int interp, int path_type)
{
	int port_id[4] = { 0, 0, 0, 0 };
	int *port_ptr = NULL;
	int num_ports = 0;
	int bit_width = 0, i = 0;
	int mux_reg = 0, mux_reg_val = 0;
	int dai_id = 0, idle_thr = 0;

	if ((interp != INTERP_HPHL) && (interp != INTERP_HPHR))
		return 0;

	if (!rx_priv->idle_det_cfg.hph_idle_detect_en)
		return 0;

	port_ptr = &port_id[0];
	num_ports = 0;

	/*
	 * Read interpolator MUX input registers and find
	 * which cdc_dma port is connected and store the port
	 * numbers in port_id array.
	 */
	if (path_type == INTERP_MIX_PATH) {
		mux_reg = BOLERO_CDC_RX_INP_MUX_RX_INT0_CFG1 +
						2 * interp;
		mux_reg_val = snd_soc_component_read32(component, mux_reg) &
				0x0f;

		if ((mux_reg_val >= INTn_2_INP_SEL_RX0) &&
		   (mux_reg_val <= INTn_2_INP_SEL_RX5)) {
			*port_ptr++ = mux_reg_val - 1;
			num_ports++;
		}
	}

	if (path_type == INTERP_MAIN_PATH) {
		mux_reg = BOLERO_CDC_RX_INP_MUX_RX_INT1_CFG0 +
			  2 * (interp - 1);
		mux_reg_val = snd_soc_component_read32(component, mux_reg) &
				0x0f;
		i = RX_MACRO_INTERP_MUX_NUM_INPUTS;

		while (i) {
			if ((mux_reg_val >= INTn_1_INP_SEL_RX0) &&
			    (mux_reg_val <= INTn_1_INP_SEL_RX5)) {
				*port_ptr++ = mux_reg_val -
					INTn_1_INP_SEL_RX0;
				num_ports++;
			}
			mux_reg_val =
				(snd_soc_component_read32(component, mux_reg) &
					0xf0) >> 4;
			mux_reg += 1;
			i--;
		}
	}

	dev_dbg(component->dev, "%s: num_ports: %d, ports[%d %d %d %d]\n",
		__func__, num_ports, port_id[0], port_id[1],
		port_id[2], port_id[3]);

	i = 0;
	while (num_ports) {
		dai_id = rx_macro_find_playback_dai_id_for_port(port_id[i++],
								rx_priv);

		if ((dai_id >= 0) && (dai_id < RX_MACRO_MAX_DAIS)) {
			dev_dbg(component->dev, "%s: dai_id: %d bit_width: %d\n",
				__func__, dai_id,
				rx_priv->bit_width[dai_id]);

			if (rx_priv->bit_width[dai_id] > bit_width)
				bit_width = rx_priv->bit_width[dai_id];
		}
		num_ports--;
	}

	switch (bit_width) {
	case 16:
		idle_thr = 0xff; /* F16 */
		break;
	case 24:
	case 32:
		idle_thr = 0x03; /* F22 */
		break;
	default:
		idle_thr = 0x00;
		break;
	}

	dev_dbg(component->dev, "%s: (new) idle_thr: %d, (cur) idle_thr: %d\n",
		__func__, idle_thr, rx_priv->idle_det_cfg.hph_idle_thr);

	if ((rx_priv->idle_det_cfg.hph_idle_thr == 0) ||
	    (idle_thr < rx_priv->idle_det_cfg.hph_idle_thr)) {
		snd_soc_component_write(component,
			BOLERO_CDC_RX_IDLE_DETECT_CFG3, idle_thr);
		rx_priv->idle_det_cfg.hph_idle_thr = idle_thr;
	}

	return 0;
}

static int rx_macro_enable_mix_path(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	u16 gain_reg = 0, mix_reg = 0;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	if (w->shift >= INTERP_MAX) {
		dev_err(component->dev, "%s: Invalid Interpolator value %d for name %s\n",
			__func__, w->shift, w->name);
		return -EINVAL;
	}

	gain_reg = BOLERO_CDC_RX_RX0_RX_VOL_MIX_CTL +
				(w->shift * RX_MACRO_RX_PATH_OFFSET);
	mix_reg = BOLERO_CDC_RX_RX0_RX_PATH_MIX_CTL +
				(w->shift * RX_MACRO_RX_PATH_OFFSET);

	dev_dbg(component->dev, "%s %d %s\n", __func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		rx_macro_set_idle_detect_thr(component, rx_priv, w->shift,
					INTERP_MIX_PATH);
		rx_macro_enable_interp_clk(component, event, w->shift);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_component_write(component, gain_reg,
			snd_soc_component_read32(component, gain_reg));
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Clk Disable */
		snd_soc_component_update_bits(component, mix_reg, 0x20, 0x00);
		rx_macro_enable_interp_clk(component, event, w->shift);
		/* Reset enable and disable */
		snd_soc_component_update_bits(component, mix_reg, 0x40, 0x40);
		snd_soc_component_update_bits(component, mix_reg, 0x40, 0x00);
		break;
	}

	return 0;
}

static bool rx_macro_adie_lb(struct snd_soc_component *component,
			     int interp_idx)
{
	u16 int_mux_cfg0 = 0, int_mux_cfg1 = 0;
	u8 int_mux_cfg0_val = 0, int_mux_cfg1_val = 0;
	u8 int_n_inp0 = 0, int_n_inp1 = 0, int_n_inp2 = 0;

	int_mux_cfg0 = BOLERO_CDC_RX_INP_MUX_RX_INT0_CFG0 + interp_idx * 8;
	int_mux_cfg1 = int_mux_cfg0 + 4;
	int_mux_cfg0_val = snd_soc_component_read32(component, int_mux_cfg0);
	int_mux_cfg1_val = snd_soc_component_read32(component, int_mux_cfg1);

	int_n_inp0 = int_mux_cfg0_val & 0x0F;
	if (int_n_inp0 == INTn_1_INP_SEL_DEC0 ||
		int_n_inp0 == INTn_1_INP_SEL_DEC1 ||
		int_n_inp0 == INTn_1_INP_SEL_IIR0 ||
		int_n_inp0 == INTn_1_INP_SEL_IIR1)
		return true;

	int_n_inp1 = int_mux_cfg0_val >> 4;
	if (int_n_inp1 == INTn_1_INP_SEL_DEC0 ||
		int_n_inp1 == INTn_1_INP_SEL_DEC1 ||
		int_n_inp1 == INTn_1_INP_SEL_IIR0 ||
		int_n_inp1 == INTn_1_INP_SEL_IIR1)
		return true;

	int_n_inp2 = int_mux_cfg1_val >> 4;
	if (int_n_inp2 == INTn_1_INP_SEL_DEC0 ||
		int_n_inp2 == INTn_1_INP_SEL_DEC1 ||
		int_n_inp2 == INTn_1_INP_SEL_IIR0 ||
		int_n_inp2 == INTn_1_INP_SEL_IIR1)
		return true;

	return false;
}

static int rx_macro_enable_main_path(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	u16 gain_reg = 0;
	u16 reg = 0;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	dev_dbg(component->dev, "%s %d %s\n", __func__, event, w->name);

	if (w->shift >= INTERP_MAX) {
		dev_err(component->dev, "%s: Invalid Interpolator value %d for name %s\n",
			__func__, w->shift, w->name);
		return -EINVAL;
	}

	reg = BOLERO_CDC_RX_RX0_RX_PATH_CTL + (w->shift *
						RX_MACRO_RX_PATH_OFFSET);
	gain_reg = BOLERO_CDC_RX_RX0_RX_VOL_CTL + (w->shift *
						RX_MACRO_RX_PATH_OFFSET);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		rx_macro_set_idle_detect_thr(component, rx_priv, w->shift,
						INTERP_MAIN_PATH);
		rx_macro_enable_interp_clk(component, event, w->shift);
		if (rx_macro_adie_lb(component, w->shift))
			snd_soc_component_update_bits(component,
						reg, 0x20, 0x20);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_component_write(component, gain_reg,
			snd_soc_component_read32(component, gain_reg));
		break;
	case SND_SOC_DAPM_POST_PMD:
		rx_macro_enable_interp_clk(component, event, w->shift);
		break;
	}

	return 0;
}

static int rx_macro_config_compander(struct snd_soc_component *component,
				struct rx_macro_priv *rx_priv,
				int interp_n, int event)
{
	int comp = 0;
	u16 comp_ctl0_reg = 0, rx_path_cfg0_reg = 0, rx_path_cfg3_reg = 0;
	u16 rx0_path_ctl_reg = 0;
	u8 pcm_rate = 0, val = 0;

	/* AUX does not have compander */
	if (interp_n == INTERP_AUX)
		return 0;

	rx_path_cfg3_reg = BOLERO_CDC_RX_RX0_RX_PATH_CFG3 +
					(comp * RX_MACRO_RX_PATH_OFFSET);
	rx0_path_ctl_reg = BOLERO_CDC_RX_RX0_RX_PATH_CTL +
					(comp * RX_MACRO_RX_PATH_OFFSET);
	pcm_rate = (snd_soc_component_read32(component, rx0_path_ctl_reg)
						& 0x0F);

	dev_err(component->dev, "%s: pcm_rate %d\n",
		__func__, pcm_rate);

	if (pcm_rate < 0x06)
		val = 0x03;
	else if (pcm_rate < 0x08)
		val = 0x01;
	else if (pcm_rate < 0x0B)
		val = 0x02;
	else
		val = 0x00;
	snd_soc_component_update_bits(component, rx_path_cfg3_reg,
					0x03, val);

	comp = interp_n;
	dev_dbg(component->dev, "%s: event %d compander %d, enabled %d\n",
		__func__, event, comp + 1, rx_priv->comp_enabled[comp]);

	rx_path_cfg3_reg = BOLERO_CDC_RX_RX0_RX_PATH_CFG3 +
					(comp * RX_MACRO_RX_PATH_OFFSET);
	rx0_path_ctl_reg = BOLERO_CDC_RX_RX0_RX_PATH_CTL +
					(comp * RX_MACRO_RX_PATH_OFFSET);
	pcm_rate = (snd_soc_component_read32(component, rx0_path_ctl_reg)
						& 0x0F);
	if (pcm_rate < 0x06)
		val = 0x03;
	else if (pcm_rate < 0x08)
		val = 0x01;
	else if (pcm_rate < 0x0B)
		val = 0x02;
	else
		val = 0x00;

	if (SND_SOC_DAPM_EVENT_ON(event))
		snd_soc_component_update_bits(component, rx_path_cfg3_reg,
					0x03, val);
	if (SND_SOC_DAPM_EVENT_OFF(event))
		snd_soc_component_update_bits(component, rx_path_cfg3_reg,
					0x03, 0x03);
	if (!rx_priv->comp_enabled[comp])
		return 0;

	comp_ctl0_reg = BOLERO_CDC_RX_COMPANDER0_CTL0 +
					(comp * RX_MACRO_COMP_OFFSET);
	rx_path_cfg0_reg = BOLERO_CDC_RX_RX0_RX_PATH_CFG0 +
					(comp * RX_MACRO_RX_PATH_OFFSET);
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		/* Enable Compander Clock */
		snd_soc_component_update_bits(component, comp_ctl0_reg,
					0x01, 0x01);
		snd_soc_component_update_bits(component, comp_ctl0_reg,
					0x02, 0x02);
		snd_soc_component_update_bits(component, comp_ctl0_reg,
					0x02, 0x00);
		snd_soc_component_update_bits(component, rx_path_cfg0_reg,
					0x02, 0x02);
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_component_update_bits(component, comp_ctl0_reg,
					0x04, 0x04);
		snd_soc_component_update_bits(component, rx_path_cfg0_reg,
					0x02, 0x00);
		snd_soc_component_update_bits(component, comp_ctl0_reg,
					0x01, 0x00);
		snd_soc_component_update_bits(component, comp_ctl0_reg,
					0x04, 0x00);
	}

	return 0;
}

static int rx_macro_load_compander_coeff(struct snd_soc_component *component,
					 struct rx_macro_priv *rx_priv,
					 int interp_n, int event)
{
	int comp = 0;
	u16 comp_coeff_lsb_reg = 0, comp_coeff_msb_reg = 0;
	int i = 0;
	int hph_pwr_mode = HPH_LOHIFI;

	if (!rx_priv->comp_enabled[comp])
		return 0;

	if (interp_n == INTERP_HPHL) {
		comp_coeff_lsb_reg = BOLERO_CDC_RX_TOP_HPHL_COMP_WR_LSB;
		comp_coeff_msb_reg = BOLERO_CDC_RX_TOP_HPHL_COMP_WR_MSB;
	} else if (interp_n == INTERP_HPHR) {
		comp_coeff_lsb_reg = BOLERO_CDC_RX_TOP_HPHR_COMP_WR_LSB;
		comp_coeff_msb_reg = BOLERO_CDC_RX_TOP_HPHR_COMP_WR_MSB;
	} else {
		/* compander coefficients are loaded only for hph path */
		return 0;
	}

	comp = interp_n;
	hph_pwr_mode = rx_priv->hph_pwr_mode;
	dev_dbg(component->dev, "%s: event %d compander %d, enabled %d\n",
		__func__, event, comp + 1, rx_priv->comp_enabled[comp]);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		/* Load Compander Coeff */
		for (i = 0; i < COMP_MAX_COEFF; i++) {
			snd_soc_component_write(component, comp_coeff_lsb_reg,
					comp_coeff_table[hph_pwr_mode][i].lsb);
			snd_soc_component_write(component, comp_coeff_msb_reg,
					comp_coeff_table[hph_pwr_mode][i].msb);
		}
	}

	return 0;
}

static void rx_macro_enable_softclip_clk(struct snd_soc_component *component,
					 struct rx_macro_priv *rx_priv,
					 bool enable)
{
	if (enable) {
		if (rx_priv->softclip_clk_users == 0)
			snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_SOFTCLIP_CRC,
				0x01, 0x01);
		rx_priv->softclip_clk_users++;
	} else {
		rx_priv->softclip_clk_users--;
		if (rx_priv->softclip_clk_users == 0)
			snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_SOFTCLIP_CRC,
				0x01, 0x00);
	}
}

static int rx_macro_config_softclip(struct snd_soc_component *component,
				struct rx_macro_priv *rx_priv,
				int event)
{
	dev_dbg(component->dev, "%s: event %d, enabled %d\n",
		__func__, event, rx_priv->is_softclip_on);

	if (!rx_priv->is_softclip_on)
		return 0;

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		/* Enable Softclip clock */
		rx_macro_enable_softclip_clk(component, rx_priv, true);
		/* Enable Softclip control */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_SOFTCLIP_SOFTCLIP_CTRL, 0x01, 0x01);
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_SOFTCLIP_SOFTCLIP_CTRL, 0x01, 0x00);
		rx_macro_enable_softclip_clk(component, rx_priv, false);
	}

	return 0;
}

static int rx_macro_config_aux_hpf(struct snd_soc_component *component,
				struct rx_macro_priv *rx_priv,
				int event)
{
	dev_dbg(component->dev, "%s: event %d, enabled %d\n",
		__func__, event, rx_priv->is_aux_hpf_on);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		/* Update Aux HPF control */
		if (!rx_priv->is_aux_hpf_on)
			snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_RX2_RX_PATH_CFG1, 0x04, 0x00);
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		/* Reset to default (HPF=ON) */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_RX2_RX_PATH_CFG1, 0x04, 0x04);
	}

	return 0;
}


static inline void
rx_macro_enable_clsh_block(struct rx_macro_priv *rx_priv, bool enable)
{
	if ((enable && ++rx_priv->clsh_users == 1) ||
	    (!enable && --rx_priv->clsh_users == 0))
		snd_soc_component_update_bits(rx_priv->component,
				BOLERO_CDC_RX_CLSH_CRC, 0x01,
				(u8) enable);
	if (rx_priv->clsh_users < 0)
		rx_priv->clsh_users = 0;
	dev_dbg(rx_priv->dev, "%s: clsh_users %d, enable %d", __func__,
		rx_priv->clsh_users, enable);
}

static int rx_macro_config_classh(struct snd_soc_component *component,
				struct rx_macro_priv *rx_priv,
				int interp_n, int event)
{
	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		rx_macro_enable_clsh_block(rx_priv, false);
		return 0;
	}

	if (!SND_SOC_DAPM_EVENT_ON(event))
		return 0;

	rx_macro_enable_clsh_block(rx_priv, true);
	if (interp_n == INTERP_HPHL ||
		interp_n == INTERP_HPHR) {
		/*
		 * These K1 values depend on the Headphone Impedance
		 * For now it is assumed to be 16 ohm
		 */
		snd_soc_component_update_bits(component,
					BOLERO_CDC_RX_CLSH_K1_LSB,
					0xFF, 0xC0);
		snd_soc_component_update_bits(component,
					BOLERO_CDC_RX_CLSH_K1_MSB,
					0x0F, 0x00);
	}
	switch (interp_n) {
	case INTERP_HPHL:
		if (rx_priv->is_ear_mode_on)
			snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_CLSH_HPH_V_PA,
				0x3F, 0x39);
		else
			snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_CLSH_HPH_V_PA,
				0x3F, 0x1C);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_CLSH_DECAY_CTRL,
				0x07, 0x00);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_RX0_RX_PATH_CFG0,
				0x40, 0x40);
		break;
	case INTERP_HPHR:
		if (rx_priv->is_ear_mode_on)
			snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_CLSH_HPH_V_PA,
				0x3F, 0x39);
		else
			snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_CLSH_HPH_V_PA,
				0x3F, 0x1C);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_CLSH_DECAY_CTRL,
				0x07, 0x00);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_RX1_RX_PATH_CFG0,
				0x40, 0x40);
		break;
	case INTERP_AUX:
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_RX2_RX_PATH_CFG0,
				0x08, 0x08);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_RX2_RX_PATH_CFG0,
				0x10, 0x10);
		break;
	}

	return 0;
}

static void rx_macro_hd2_control(struct snd_soc_component *component,
				 u16 interp_idx, int event)
{
	u16 hd2_scale_reg = 0;
	u16 hd2_enable_reg = 0;

	switch (interp_idx) {
	case INTERP_HPHL:
		hd2_scale_reg = BOLERO_CDC_RX_RX0_RX_PATH_SEC3;
		hd2_enable_reg = BOLERO_CDC_RX_RX0_RX_PATH_CFG0;
		break;
	case INTERP_HPHR:
		hd2_scale_reg = BOLERO_CDC_RX_RX1_RX_PATH_SEC3;
		hd2_enable_reg = BOLERO_CDC_RX_RX1_RX_PATH_CFG0;
		break;
	}

	if (hd2_enable_reg && SND_SOC_DAPM_EVENT_ON(event)) {
		snd_soc_component_update_bits(component, hd2_scale_reg,
				0x3C, 0x14);
		snd_soc_component_update_bits(component, hd2_enable_reg,
				0x04, 0x04);
	}

	if (hd2_enable_reg && SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_component_update_bits(component, hd2_enable_reg,
				0x04, 0x00);
		snd_soc_component_update_bits(component, hd2_scale_reg,
				0x3C, 0x00);
	}
}

static int rx_macro_hph_idle_detect_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct rx_macro_priv *rx_priv = NULL;
	struct device *rx_dev = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] =
		rx_priv->idle_det_cfg.hph_idle_detect_en;

	return 0;
}

static int rx_macro_hph_idle_detect_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct rx_macro_priv *rx_priv = NULL;
	struct device *rx_dev = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	rx_priv->idle_det_cfg.hph_idle_detect_en =
		ucontrol->value.integer.value[0];

	return 0;
}

static int rx_macro_get_compander(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] = rx_priv->comp_enabled[comp];
	return 0;
}

static int rx_macro_set_compander(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	dev_dbg(component->dev, "%s: Compander %d enable current %d, new %d\n",
		__func__, comp + 1, rx_priv->comp_enabled[comp], value);
	rx_priv->comp_enabled[comp] = value;

	return 0;
}

static int rx_macro_mux_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(widget->dapm);
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] =
			rx_priv->rx_port_value[widget->shift];
	return 0;
}

static int rx_macro_mux_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(widget->dapm);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_dapm_update *update = NULL;
	u32 rx_port_value = ucontrol->value.integer.value[0];
	u32 aif_rst = 0;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	aif_rst = rx_priv->rx_port_value[widget->shift];
	if (!rx_port_value) {
		if (aif_rst == 0) {
			dev_err(rx_dev, "%s:AIF reset already\n", __func__);
			return 0;
		}
		if (aif_rst > RX_MACRO_AIF4_PB) {
			dev_err(rx_dev, "%s: Invalid AIF reset\n", __func__);
			return 0;
		}
	}
	rx_priv->rx_port_value[widget->shift] = rx_port_value;

	dev_err(rx_dev, "%s: name:%s mux input:%d mux output:%d aif_rst: %d\n",
		__func__, widget->name, rx_port_value, widget->shift, aif_rst);

	switch (rx_port_value) {
	case 0:
		if (rx_priv->active_ch_cnt[aif_rst]) {
			clear_bit(widget->shift,
				&rx_priv->active_ch_mask[aif_rst]);
			rx_priv->active_ch_cnt[aif_rst]--;
		}
		break;
	case 1:
	case 2:
	case 3:
	case 4:
		set_bit(widget->shift,
			&rx_priv->active_ch_mask[rx_port_value]);
		rx_priv->active_ch_cnt[rx_port_value]++;
		break;
	default:
		dev_err(component->dev,
			"%s:Invalid AIF_ID for RX_MACRO MUX %d\n",
			__func__, rx_port_value);
		goto err;
	}

	snd_soc_dapm_mux_update_power(widget->dapm, kcontrol,
					rx_port_value, e, update);
	return 0;
err:
	return -EINVAL;
}

static int rx_macro_get_ear_mode(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] = rx_priv->is_ear_mode_on;
	return 0;
}

static int rx_macro_put_ear_mode(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	rx_priv->is_ear_mode_on =
			(!ucontrol->value.integer.value[0] ? false : true);
	return 0;
}

static int rx_macro_get_hph_hd2_mode(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] = rx_priv->hph_hd2_mode;
	return 0;
}

static int rx_macro_put_hph_hd2_mode(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	rx_priv->hph_hd2_mode = ucontrol->value.integer.value[0];
	return 0;
}

static int rx_macro_get_hph_pwr_mode(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] = rx_priv->hph_pwr_mode;
	return 0;
}

static int rx_macro_put_hph_pwr_mode(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	rx_priv->hph_pwr_mode = ucontrol->value.integer.value[0];
	return 0;
}

static int rx_macro_vbat_bcl_gsm_mode_func_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);

	ucontrol->value.integer.value[0] =
		((snd_soc_component_read32(
			component, BOLERO_CDC_RX_BCL_VBAT_CFG) & 0x04) ?
		  1 : 0);

	dev_dbg(component->dev, "%s: value: %lu\n", __func__,
		ucontrol->value.integer.value[0]);

	return 0;
}

static int rx_macro_vbat_bcl_gsm_mode_func_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);

	dev_dbg(component->dev, "%s: value: %lu\n", __func__,
		ucontrol->value.integer.value[0]);

	/* Set Vbat register configuration for GSM mode bit based on value */
	if (ucontrol->value.integer.value[0])
		snd_soc_component_update_bits(component,
					BOLERO_CDC_RX_BCL_VBAT_CFG,
					0x04, 0x04);
	else
		snd_soc_component_update_bits(component,
					BOLERO_CDC_RX_BCL_VBAT_CFG,
					0x04, 0x00);

	return 0;
}

static int rx_macro_soft_clip_enable_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] = rx_priv->is_softclip_on;

	dev_dbg(component->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int rx_macro_soft_clip_enable_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	rx_priv->is_softclip_on = ucontrol->value.integer.value[0];

	dev_dbg(component->dev, "%s: soft clip enable = %d\n", __func__,
		rx_priv->is_softclip_on);

	return 0;
}

static int rx_macro_aux_hpf_mode_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] = rx_priv->is_aux_hpf_on;

	dev_dbg(component->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int rx_macro_aux_hpf_mode_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	rx_priv->is_aux_hpf_on = ucontrol->value.integer.value[0];

	dev_dbg(component->dev, "%s: aux hpf enable = %d\n", __func__,
		rx_priv->is_aux_hpf_on);

	return 0;
}


static int rx_macro_enable_vbat(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol,
				 int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	dev_dbg(component->dev, "%s %s %d\n", __func__, w->name, event);
	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Enable clock for VBAT block */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_PATH_CTL, 0x10, 0x10);
		/* Enable VBAT block */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_CFG, 0x01, 0x01);
		/* Update interpolator with 384K path */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_RX2_RX_PATH_CFG1, 0x80, 0x80);
		/* Update DSM FS rate */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_RX2_RX_PATH_SEC7, 0x02, 0x02);
		/* Use attenuation mode */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_CFG, 0x02, 0x00);
		/* BCL block needs softclip clock to be enabled */
		rx_macro_enable_softclip_clk(component, rx_priv, true);
		/* Enable VBAT at channel level */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_RX2_RX_PATH_CFG1, 0x02, 0x02);
		/* Set the ATTK1 gain */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD1,
			0xFF, 0xFF);
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD2,
			0xFF, 0x03);
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD3,
			0xFF, 0x00);
		/* Set the ATTK2 gain */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD4,
			0xFF, 0xFF);
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD5,
			0xFF, 0x03);
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD6,
			0xFF, 0x00);
		/* Set the ATTK3 gain */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD7,
			0xFF, 0xFF);
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD8,
			0xFF, 0x03);
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD9,
			0xFF, 0x00);
		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_RX2_RX_PATH_CFG1,
				0x80, 0x00);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_RX2_RX_PATH_SEC7,
				0x02, 0x00);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_BCL_VBAT_CFG,
				0x02, 0x02);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_RX2_RX_PATH_CFG1,
				0x02, 0x00);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD1,
				0xFF, 0x00);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD2,
				0xFF, 0x00);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD3,
				0xFF, 0x00);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD4,
				0xFF, 0x00);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD5,
				0xFF, 0x00);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD6,
				0xFF, 0x00);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD7,
				0xFF, 0x00);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD8,
				0xFF, 0x00);
		snd_soc_component_update_bits(component,
				BOLERO_CDC_RX_BCL_VBAT_BCL_GAIN_UPD9,
				0xFF, 0x00);
		rx_macro_enable_softclip_clk(component, rx_priv, false);
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_CFG, 0x01, 0x00);
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_PATH_CTL, 0x10, 0x00);
		break;
	default:
		dev_err(rx_dev, "%s: Invalid event %d\n", __func__, event);
		break;
	}
	return 0;
}

static void rx_macro_idle_detect_control(struct snd_soc_component *component,
					 struct rx_macro_priv *rx_priv,
					 int interp, int event)
{
	int reg = 0, mask = 0, val = 0;

	if (!rx_priv->idle_det_cfg.hph_idle_detect_en)
		return;

	if (interp == INTERP_HPHL) {
		reg = BOLERO_CDC_RX_IDLE_DETECT_PATH_CTL;
		mask = 0x01;
		val = 0x01;
	}
	if (interp == INTERP_HPHR) {
		reg = BOLERO_CDC_RX_IDLE_DETECT_PATH_CTL;
		mask = 0x02;
		val = 0x02;
	}

	if (reg && SND_SOC_DAPM_EVENT_ON(event))
		snd_soc_component_update_bits(component, reg, mask, val);

	if (reg && SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_component_update_bits(component, reg, mask, 0x00);
		rx_priv->idle_det_cfg.hph_idle_thr = 0;
		snd_soc_component_write(component,
				BOLERO_CDC_RX_IDLE_DETECT_CFG3, 0x0);
	}
}

static void rx_macro_hphdelay_lutbypass(struct snd_soc_component *component,
					struct rx_macro_priv *rx_priv,
					u16 interp_idx, int event)
{
	u16 hph_lut_bypass_reg = 0;
	u16 hph_comp_ctrl7 = 0;

	switch (interp_idx) {
	case INTERP_HPHL:
		hph_lut_bypass_reg = BOLERO_CDC_RX_TOP_HPHL_COMP_LUT;
		hph_comp_ctrl7 = BOLERO_CDC_RX_COMPANDER0_CTL7;
		break;
	case INTERP_HPHR:
		hph_lut_bypass_reg = BOLERO_CDC_RX_TOP_HPHR_COMP_LUT;
		hph_comp_ctrl7 = BOLERO_CDC_RX_COMPANDER1_CTL7;
		break;
	default:
		break;
	}

	if (hph_lut_bypass_reg && SND_SOC_DAPM_EVENT_ON(event)) {
		if (interp_idx == INTERP_HPHL) {
			if (rx_priv->is_ear_mode_on)
				snd_soc_component_update_bits(component,
					BOLERO_CDC_RX_RX0_RX_PATH_CFG1,
					0x02, 0x02);
			else
				snd_soc_component_update_bits(component,
					hph_lut_bypass_reg,
					0x80, 0x80);
		} else {
			snd_soc_component_update_bits(component,
					hph_lut_bypass_reg,
					0x80, 0x80);
		}
		if (rx_priv->hph_pwr_mode)
			snd_soc_component_update_bits(component,
					hph_comp_ctrl7,
					0x20, 0x00);
	}

	if (hph_lut_bypass_reg && SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_component_update_bits(component,
					BOLERO_CDC_RX_RX0_RX_PATH_CFG1,
					0x02, 0x00);
		snd_soc_component_update_bits(component, hph_lut_bypass_reg,
					0x80, 0x00);
		snd_soc_component_update_bits(component, hph_comp_ctrl7,
					0x20, 0x20);
	}
}

static int rx_macro_enable_interp_clk(struct snd_soc_component *component,
				      int event, int interp_idx)
{
	u16 main_reg = 0, dsm_reg = 0, rx_cfg2_reg = 0;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	main_reg = BOLERO_CDC_RX_RX0_RX_PATH_CTL +
			(interp_idx * RX_MACRO_RX_PATH_OFFSET);
	dsm_reg = BOLERO_CDC_RX_RX0_RX_PATH_DSM_CTL +
			(interp_idx * RX_MACRO_RX_PATH_OFFSET);
	if (interp_idx == INTERP_AUX)
		dsm_reg = BOLERO_CDC_RX_RX2_RX_PATH_DSM_CTL;
	rx_cfg2_reg = BOLERO_CDC_RX_RX0_RX_PATH_CFG2 +
			(interp_idx * RX_MACRO_RX_PATH_OFFSET);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (rx_priv->main_clk_users[interp_idx] == 0) {
			/* Main path PGA mute enable */
			snd_soc_component_update_bits(component, main_reg,
					0x10, 0x10);
			snd_soc_component_update_bits(component, dsm_reg,
					0x01, 0x01);
			snd_soc_component_update_bits(component, rx_cfg2_reg,
					0x03, 0x03);
			rx_macro_load_compander_coeff(component, rx_priv,
						      interp_idx, event);
			rx_macro_idle_detect_control(component, rx_priv,
					interp_idx, event);
			if (rx_priv->hph_hd2_mode)
				rx_macro_hd2_control(
					component, interp_idx, event);
			rx_macro_hphdelay_lutbypass(component, rx_priv,
						    interp_idx, event);
			rx_macro_config_compander(component, rx_priv,
						interp_idx, event);
			if (interp_idx == INTERP_AUX) {
				rx_macro_config_softclip(component, rx_priv,
							event);
				rx_macro_config_aux_hpf(component, rx_priv,
							event);
			}
			rx_macro_config_classh(component, rx_priv,
						interp_idx, event);
		}
		rx_priv->main_clk_users[interp_idx]++;
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		rx_priv->main_clk_users[interp_idx]--;
		if (rx_priv->main_clk_users[interp_idx] <= 0) {
			rx_priv->main_clk_users[interp_idx] = 0;
			/* Main path PGA mute enable */
			snd_soc_component_update_bits(component, main_reg,
					0x10, 0x10);
			/* Clk Disable */
			snd_soc_component_update_bits(component, dsm_reg,
						0x01, 0x00);
			snd_soc_component_update_bits(component, main_reg,
						0x20, 0x00);
			/* Reset enable and disable */
			snd_soc_component_update_bits(component, main_reg,
						0x40, 0x40);
			snd_soc_component_update_bits(component, main_reg,
						0x40, 0x00);
			/* Reset rate to 48K*/
			dev_dbg(component->dev, "%s: reset rate to 48k\n", __func__);
			snd_soc_component_update_bits(component, main_reg,
						0x0F, 0x04);
			snd_soc_component_update_bits(component, rx_cfg2_reg,
						0x03, 0x00);
			rx_macro_config_classh(component, rx_priv,
						interp_idx, event);
			rx_macro_config_compander(component, rx_priv,
						interp_idx, event);
			if (interp_idx ==  INTERP_AUX) {
				rx_macro_config_softclip(component, rx_priv,
							event);
				rx_macro_config_aux_hpf(component, rx_priv,
				event);
			}
			rx_macro_hphdelay_lutbypass(component, rx_priv,
						interp_idx, event);
			if (rx_priv->hph_hd2_mode)
				rx_macro_hd2_control(component, interp_idx,
						event);
			rx_macro_idle_detect_control(component, rx_priv,
					interp_idx, event);
		}
	}

	dev_dbg(component->dev, "%s event %d main_clk_users %d\n",
		__func__,  event, rx_priv->main_clk_users[interp_idx]);

	return rx_priv->main_clk_users[interp_idx];
}

static int rx_macro_enable_rx_path_clk(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	u16 sidetone_reg = 0, fs_reg = 0;

	dev_dbg(component->dev, "%s %d %d\n", __func__, event, w->shift);
	sidetone_reg = BOLERO_CDC_RX_RX0_RX_PATH_CFG1 +
			RX_MACRO_RX_PATH_OFFSET * (w->shift);
	fs_reg = BOLERO_CDC_RX_RX0_RX_PATH_CTL +
			RX_MACRO_RX_PATH_OFFSET * (w->shift);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		rx_macro_enable_interp_clk(component, event, w->shift);
		snd_soc_component_update_bits(component, sidetone_reg,
					0x10, 0x10);
		snd_soc_component_update_bits(component, fs_reg,
					0x20, 0x20);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component, sidetone_reg,
					0x10, 0x00);
		rx_macro_enable_interp_clk(component, event, w->shift);
		break;
	default:
		break;
	};
	return 0;
}

static void rx_macro_restore_iir_coeff(struct rx_macro_priv *rx_priv, int iir_idx,
				int band_idx)
{
	u16 reg_add = 0, coeff_idx = 0, idx = 0;
	struct regmap *regmap = dev_get_regmap(rx_priv->dev->parent, NULL);

	if (regmap == NULL) {
		dev_err(rx_priv->dev, "%s: regmap is NULL\n", __func__);
		return;
	}

	regmap_write(regmap,
		(BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B1_CTL + 0x80 * iir_idx),
		(band_idx * BAND_MAX * sizeof(uint32_t)) & 0x7F);

	reg_add = BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B2_CTL + 0x80 * iir_idx;

	/* 5 coefficients per band and 4 writes per coefficient */
	for (coeff_idx = 0; coeff_idx < RX_MACRO_SIDETONE_IIR_COEFF_MAX;
		coeff_idx++) {
		/* Four 8 bit values(one 32 bit) per coefficient */
		regmap_write(regmap, reg_add,
		rx_priv->sidetone_coeff_array[iir_idx][band_idx][idx++]);
		regmap_write(regmap, reg_add,
		rx_priv->sidetone_coeff_array[iir_idx][band_idx][idx++]);
		regmap_write(regmap, reg_add,
		rx_priv->sidetone_coeff_array[iir_idx][band_idx][idx++]);
		regmap_write(regmap, reg_add,
		rx_priv->sidetone_coeff_array[iir_idx][band_idx][idx++]);
	}
}

static int rx_macro_iir_enable_audio_mixer_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	/* IIR filter band registers are at integer multiples of 0x80 */
	u16 iir_reg = BOLERO_CDC_RX_SIDETONE_IIR0_IIR_CTL + 0x80 * iir_idx;

	ucontrol->value.integer.value[0] = (
				snd_soc_component_read32(component, iir_reg) &
				(1 << band_idx)) != 0;

	dev_dbg(component->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0]);
	return 0;
}

static int rx_macro_iir_enable_audio_mixer_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	bool iir_band_en_status = 0;
	int value = ucontrol->value.integer.value[0];
	u16 iir_reg = BOLERO_CDC_RX_SIDETONE_IIR0_IIR_CTL + 0x80 * iir_idx;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	rx_macro_restore_iir_coeff(rx_priv, iir_idx, band_idx);

	/* Mask first 5 bits, 6-8 are reserved */
	snd_soc_component_update_bits(component, iir_reg, (1 << band_idx),
			    (value << band_idx));

	iir_band_en_status = ((snd_soc_component_read32(component, iir_reg) &
			      (1 << band_idx)) != 0);
	dev_dbg(component->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx, iir_band_en_status);
	return 0;
}

static uint32_t get_iir_band_coeff(struct snd_soc_component *component,
				   int iir_idx, int band_idx,
				   int coeff_idx)
{
	uint32_t value = 0;

	/* Address does not automatically update if reading */
	snd_soc_component_write(component,
		(BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B1_CTL + 0x80 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t)) & 0x7F);

	value |= snd_soc_component_read32(component,
		(BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B2_CTL + 0x80 * iir_idx));

	snd_soc_component_write(component,
		(BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B1_CTL + 0x80 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 1) & 0x7F);

	value |= (snd_soc_component_read32(component,
			       (BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B2_CTL +
				0x80 * iir_idx)) << 8);

	snd_soc_component_write(component,
		(BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B1_CTL + 0x80 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 2) & 0x7F);

	value |= (snd_soc_component_read32(component,
			       (BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B2_CTL +
				0x80 * iir_idx)) << 16);

	snd_soc_component_write(component,
		(BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B1_CTL + 0x80 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 3) & 0x7F);

	/* Mask bits top 2 bits since they are reserved */
	value |= ((snd_soc_component_read32(component,
				(BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B2_CTL +
				 0x80 * iir_idx)) & 0x3F) << 24);

	return value;
}

static int rx_macro_iir_band_audio_mixer_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		get_iir_band_coeff(component, iir_idx, band_idx, 0);
	ucontrol->value.integer.value[1] =
		get_iir_band_coeff(component, iir_idx, band_idx, 1);
	ucontrol->value.integer.value[2] =
		get_iir_band_coeff(component, iir_idx, band_idx, 2);
	ucontrol->value.integer.value[3] =
		get_iir_band_coeff(component, iir_idx, band_idx, 3);
	ucontrol->value.integer.value[4] =
		get_iir_band_coeff(component, iir_idx, band_idx, 4);

	dev_dbg(component->dev, "%s: IIR #%d band #%d b0 = 0x%x\n"
		"%s: IIR #%d band #%d b1 = 0x%x\n"
		"%s: IIR #%d band #%d b2 = 0x%x\n"
		"%s: IIR #%d band #%d a1 = 0x%x\n"
		"%s: IIR #%d band #%d a2 = 0x%x\n",
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[1],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[2],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[3],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[4]);
	return 0;
}

static void set_iir_band_coeff(struct snd_soc_component *component,
			       int iir_idx, int band_idx,
			       uint32_t value)
{
	snd_soc_component_write(component,
		(BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B2_CTL + 0x80 * iir_idx),
		(value & 0xFF));

	snd_soc_component_write(component,
		(BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B2_CTL + 0x80 * iir_idx),
		(value >> 8) & 0xFF);

	snd_soc_component_write(component,
		(BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B2_CTL + 0x80 * iir_idx),
		(value >> 16) & 0xFF);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_component_write(component,
		(BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B2_CTL + 0x80 * iir_idx),
		(value >> 24) & 0x3F);
}

static int rx_macro_iir_band_audio_mixer_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	int coeff_idx, idx = 0;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	/*
	 * Mask top bit it is reserved
	 * Updates addr automatically for each B2 write
	 */
	snd_soc_component_write(component,
		(BOLERO_CDC_RX_SIDETONE_IIR0_IIR_COEF_B1_CTL + 0x80 * iir_idx),
		(band_idx * BAND_MAX * sizeof(uint32_t)) & 0x7F);

	/* Store the coefficients in sidetone coeff array */
	for (coeff_idx = 0; coeff_idx < RX_MACRO_SIDETONE_IIR_COEFF_MAX;
		coeff_idx++) {
		uint32_t value = ucontrol->value.integer.value[coeff_idx];

		set_iir_band_coeff(component, iir_idx, band_idx, value);

		/* Four 8 bit values(one 32 bit) per coefficient */
		rx_priv->sidetone_coeff_array[iir_idx][band_idx][idx++] =
								(value & 0xFF);
		rx_priv->sidetone_coeff_array[iir_idx][band_idx][idx++] =
							 (value >> 8) & 0xFF;
		rx_priv->sidetone_coeff_array[iir_idx][band_idx][idx++] =
							 (value >> 16) & 0xFF;
		rx_priv->sidetone_coeff_array[iir_idx][band_idx][idx++] =
							 (value >> 24) & 0xFF;
	}

	pr_debug("%s: IIR #%d band #%d b0 = 0x%x\n"
		"%s: IIR #%d band #%d b1 = 0x%x\n"
		"%s: IIR #%d band #%d b2 = 0x%x\n"
		"%s: IIR #%d band #%d a1 = 0x%x\n"
		"%s: IIR #%d band #%d a2 = 0x%x\n",
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(component, iir_idx, band_idx, 0),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(component, iir_idx, band_idx, 1),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(component, iir_idx, band_idx, 2),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(component, iir_idx, band_idx, 3),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(component, iir_idx, band_idx, 4));
	return 0;
}

static int rx_macro_set_iir_gain(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);

	dev_dbg(component->dev, "%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU: /* fall through */
	case SND_SOC_DAPM_PRE_PMD:
		if (strnstr(w->name, "IIR0", sizeof("IIR0"))) {
			snd_soc_component_write(component,
				BOLERO_CDC_RX_SIDETONE_IIR0_IIR_GAIN_B1_CTL,
			snd_soc_component_read32(component,
				BOLERO_CDC_RX_SIDETONE_IIR0_IIR_GAIN_B1_CTL));
			snd_soc_component_write(component,
				BOLERO_CDC_RX_SIDETONE_IIR0_IIR_GAIN_B2_CTL,
			snd_soc_component_read32(component,
				BOLERO_CDC_RX_SIDETONE_IIR0_IIR_GAIN_B2_CTL));
			snd_soc_component_write(component,
				BOLERO_CDC_RX_SIDETONE_IIR0_IIR_GAIN_B3_CTL,
			snd_soc_component_read32(component,
				BOLERO_CDC_RX_SIDETONE_IIR0_IIR_GAIN_B3_CTL));
			snd_soc_component_write(component,
				BOLERO_CDC_RX_SIDETONE_IIR0_IIR_GAIN_B4_CTL,
			snd_soc_component_read32(component,
				BOLERO_CDC_RX_SIDETONE_IIR0_IIR_GAIN_B4_CTL));
		} else {
			snd_soc_component_write(component,
				BOLERO_CDC_RX_SIDETONE_IIR1_IIR_GAIN_B1_CTL,
			snd_soc_component_read32(component,
				BOLERO_CDC_RX_SIDETONE_IIR1_IIR_GAIN_B1_CTL));
			snd_soc_component_write(component,
				BOLERO_CDC_RX_SIDETONE_IIR1_IIR_GAIN_B2_CTL,
			snd_soc_component_read32(component,
				BOLERO_CDC_RX_SIDETONE_IIR1_IIR_GAIN_B2_CTL));
			snd_soc_component_write(component,
				BOLERO_CDC_RX_SIDETONE_IIR1_IIR_GAIN_B3_CTL,
			snd_soc_component_read32(component,
				BOLERO_CDC_RX_SIDETONE_IIR1_IIR_GAIN_B3_CTL));
			snd_soc_component_write(component,
				BOLERO_CDC_RX_SIDETONE_IIR1_IIR_GAIN_B4_CTL,
			snd_soc_component_read32(component,
				BOLERO_CDC_RX_SIDETONE_IIR1_IIR_GAIN_B4_CTL));
		}
		break;
	}
	return 0;
}

static const struct snd_kcontrol_new rx_macro_snd_controls[] = {
	SOC_SINGLE_S8_TLV("RX_RX0 Digital Volume",
			  BOLERO_CDC_RX_RX0_RX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX_RX1 Digital Volume",
			  BOLERO_CDC_RX_RX1_RX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX_RX2 Digital Volume",
			  BOLERO_CDC_RX_RX2_RX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX_RX0 Mix Digital Volume",
			  BOLERO_CDC_RX_RX0_RX_VOL_MIX_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX_RX1 Mix Digital Volume",
			  BOLERO_CDC_RX_RX1_RX_VOL_MIX_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX_RX2 Mix Digital Volume",
			  BOLERO_CDC_RX_RX2_RX_VOL_MIX_CTL,
			  -84, 40, digital_gain),

	SOC_SINGLE_EXT("RX_COMP1 Switch", SND_SOC_NOPM, RX_MACRO_COMP1, 1, 0,
		rx_macro_get_compander, rx_macro_set_compander),
	SOC_SINGLE_EXT("RX_COMP2 Switch", SND_SOC_NOPM, RX_MACRO_COMP2, 1, 0,
		rx_macro_get_compander, rx_macro_set_compander),

	SOC_ENUM_EXT("HPH Idle Detect", hph_idle_detect_enum,
		rx_macro_hph_idle_detect_get, rx_macro_hph_idle_detect_put),

	SOC_ENUM_EXT("RX_EAR Mode", rx_macro_ear_mode_enum,
		rx_macro_get_ear_mode, rx_macro_put_ear_mode),

	SOC_ENUM_EXT("RX_HPH HD2 Mode", rx_macro_hph_hd2_mode_enum,
		rx_macro_get_hph_hd2_mode, rx_macro_put_hph_hd2_mode),

	SOC_ENUM_EXT("RX_HPH_PWR_MODE", rx_macro_hph_pwr_mode_enum,
		rx_macro_get_hph_pwr_mode, rx_macro_put_hph_pwr_mode),

	SOC_ENUM_EXT("RX_GSM mode Enable", rx_macro_vbat_bcl_gsm_mode_enum,
			rx_macro_vbat_bcl_gsm_mode_func_get,
			rx_macro_vbat_bcl_gsm_mode_func_put),
	SOC_SINGLE_EXT("RX_Softclip Enable", SND_SOC_NOPM, 0, 1, 0,
		     rx_macro_soft_clip_enable_get,
		     rx_macro_soft_clip_enable_put),
	SOC_SINGLE_EXT("AUX_HPF Enable", SND_SOC_NOPM, 0, 1, 0,
			rx_macro_aux_hpf_mode_get,
			rx_macro_aux_hpf_mode_put),

	SOC_SINGLE_S8_TLV("IIR0 INP0 Volume",
		BOLERO_CDC_RX_SIDETONE_IIR0_IIR_GAIN_B1_CTL, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("IIR0 INP1 Volume",
		BOLERO_CDC_RX_SIDETONE_IIR0_IIR_GAIN_B2_CTL, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("IIR0 INP2 Volume",
		BOLERO_CDC_RX_SIDETONE_IIR0_IIR_GAIN_B3_CTL, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("IIR0 INP3 Volume",
		BOLERO_CDC_RX_SIDETONE_IIR0_IIR_GAIN_B4_CTL, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP0 Volume",
		BOLERO_CDC_RX_SIDETONE_IIR1_IIR_GAIN_B1_CTL, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP1 Volume",
		BOLERO_CDC_RX_SIDETONE_IIR1_IIR_GAIN_B2_CTL, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP2 Volume",
		BOLERO_CDC_RX_SIDETONE_IIR1_IIR_GAIN_B3_CTL, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP3 Volume",
		BOLERO_CDC_RX_SIDETONE_IIR1_IIR_GAIN_B4_CTL, -84, 40,
		digital_gain),

	SOC_SINGLE_EXT("IIR0 Enable Band1", IIR0, BAND1, 1, 0,
		rx_macro_iir_enable_audio_mixer_get,
		rx_macro_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR0 Enable Band2", IIR0, BAND2, 1, 0,
		rx_macro_iir_enable_audio_mixer_get,
		rx_macro_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR0 Enable Band3", IIR0, BAND3, 1, 0,
		rx_macro_iir_enable_audio_mixer_get,
		rx_macro_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR0 Enable Band4", IIR0, BAND4, 1, 0,
		rx_macro_iir_enable_audio_mixer_get,
		rx_macro_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR0 Enable Band5", IIR0, BAND5, 1, 0,
		rx_macro_iir_enable_audio_mixer_get,
		rx_macro_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR1 Enable Band1", IIR1, BAND1, 1, 0,
		rx_macro_iir_enable_audio_mixer_get,
		rx_macro_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR1 Enable Band2", IIR1, BAND2, 1, 0,
		rx_macro_iir_enable_audio_mixer_get,
		rx_macro_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR1 Enable Band3", IIR1, BAND3, 1, 0,
		rx_macro_iir_enable_audio_mixer_get,
		rx_macro_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR1 Enable Band4", IIR1, BAND4, 1, 0,
		rx_macro_iir_enable_audio_mixer_get,
		rx_macro_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("IIR1 Enable Band5", IIR1, BAND5, 1, 0,
		rx_macro_iir_enable_audio_mixer_get,
		rx_macro_iir_enable_audio_mixer_put),

	SOC_SINGLE_MULTI_EXT("IIR0 Band1", IIR0, BAND1, 255, 0, 5,
		rx_macro_iir_band_audio_mixer_get,
		rx_macro_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR0 Band2", IIR0, BAND2, 255, 0, 5,
		rx_macro_iir_band_audio_mixer_get,
		rx_macro_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR0 Band3", IIR0, BAND3, 255, 0, 5,
		rx_macro_iir_band_audio_mixer_get,
		rx_macro_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR0 Band4", IIR0, BAND4, 255, 0, 5,
		rx_macro_iir_band_audio_mixer_get,
		rx_macro_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR0 Band5", IIR0, BAND5, 255, 0, 5,
		rx_macro_iir_band_audio_mixer_get,
		rx_macro_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR1 Band1", IIR1, BAND1, 255, 0, 5,
		rx_macro_iir_band_audio_mixer_get,
		rx_macro_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR1 Band2", IIR1, BAND2, 255, 0, 5,
		rx_macro_iir_band_audio_mixer_get,
		rx_macro_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR1 Band3", IIR1, BAND3, 255, 0, 5,
		rx_macro_iir_band_audio_mixer_get,
		rx_macro_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR1 Band4", IIR1, BAND4, 255, 0, 5,
		rx_macro_iir_band_audio_mixer_get,
		rx_macro_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("IIR1 Band5", IIR1, BAND5, 255, 0, 5,
		rx_macro_iir_band_audio_mixer_get,
		rx_macro_iir_band_audio_mixer_put),
};

static int rx_macro_enable_echo(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;
	u16 val = 0, ec_hq_reg = 0;
	int ec_tx = 0;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	dev_dbg(rx_dev, "%s %d %s\n", __func__, event, w->name);

	val = snd_soc_component_read32(component,
			BOLERO_CDC_RX_INP_MUX_RX_MIX_CFG4);
	if (!(strcmp(w->name, "RX MIX TX0 MUX")))
		ec_tx = ((val & 0xf0) >> 0x4) - 1;
	else if (!(strcmp(w->name, "RX MIX TX1 MUX")))
		ec_tx = (val & 0x0f) - 1;

	val = snd_soc_component_read32(component,
			BOLERO_CDC_RX_INP_MUX_RX_MIX_CFG5);
	if (!(strcmp(w->name, "RX MIX TX2 MUX")))
		ec_tx = (val & 0x0f) - 1;

	if (ec_tx < 0 || (ec_tx >= RX_MACRO_EC_MUX_MAX)) {
		dev_err(rx_dev, "%s: EC mix control not set correctly\n",
			__func__);
		return -EINVAL;
	}
	ec_hq_reg = BOLERO_CDC_RX_EC_REF_HQ0_EC_REF_HQ_PATH_CTL +
			    0x40 * ec_tx;
	snd_soc_component_update_bits(component, ec_hq_reg, 0x01, 0x01);
	ec_hq_reg = BOLERO_CDC_RX_EC_REF_HQ0_EC_REF_HQ_CFG0 +
				0x40 * ec_tx;
	/* default set to 48k */
	snd_soc_component_update_bits(component, ec_hq_reg, 0x1E, 0x08);

	return 0;
}

static const struct snd_soc_dapm_widget rx_macro_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("RX AIF1 PB", "RX_MACRO_AIF1 Playback", 0,
		SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("RX AIF2 PB", "RX_MACRO_AIF2 Playback", 0,
		SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("RX AIF3 PB", "RX_MACRO_AIF3 Playback", 0,
		SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("RX AIF4 PB", "RX_MACRO_AIF4 Playback", 0,
		SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_OUT("RX AIF_ECHO", "RX_AIF_ECHO Capture", 0,
		SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("RX AIF5 PB", "RX_MACRO_AIF5 Playback", 0,
		SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("RX AIF6 PB", "RX_MACRO_AIF6 Playback", 0,
		SND_SOC_NOPM, 0, 0),

	RX_MACRO_DAPM_MUX("RX_MACRO RX0 MUX", RX_MACRO_RX0, rx_macro_rx0),
	RX_MACRO_DAPM_MUX("RX_MACRO RX1 MUX", RX_MACRO_RX1, rx_macro_rx1),
	RX_MACRO_DAPM_MUX("RX_MACRO RX2 MUX", RX_MACRO_RX2, rx_macro_rx2),
	RX_MACRO_DAPM_MUX("RX_MACRO RX3 MUX", RX_MACRO_RX3, rx_macro_rx3),
	RX_MACRO_DAPM_MUX("RX_MACRO RX4 MUX", RX_MACRO_RX4, rx_macro_rx4),
	RX_MACRO_DAPM_MUX("RX_MACRO RX5 MUX", RX_MACRO_RX5, rx_macro_rx5),

	SND_SOC_DAPM_MIXER("RX_RX0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX_RX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX_RX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX_RX3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX_RX4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX_RX5", SND_SOC_NOPM, 0, 0, NULL, 0),

	RX_MACRO_DAPM_MUX("IIR0 INP0 MUX", 0, iir0_inp0),
	RX_MACRO_DAPM_MUX("IIR0 INP1 MUX", 0, iir0_inp1),
	RX_MACRO_DAPM_MUX("IIR0 INP2 MUX", 0, iir0_inp2),
	RX_MACRO_DAPM_MUX("IIR0 INP3 MUX", 0, iir0_inp3),
	RX_MACRO_DAPM_MUX("IIR1 INP0 MUX", 0, iir1_inp0),
	RX_MACRO_DAPM_MUX("IIR1 INP1 MUX", 0, iir1_inp1),
	RX_MACRO_DAPM_MUX("IIR1 INP2 MUX", 0, iir1_inp2),
	RX_MACRO_DAPM_MUX("IIR1 INP3 MUX", 0, iir1_inp3),

	SND_SOC_DAPM_MUX_E("RX MIX TX0 MUX", SND_SOC_NOPM,
			   RX_MACRO_EC0_MUX, 0,
			   &rx_mix_tx0_mux, rx_macro_enable_echo,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX MIX TX1 MUX", SND_SOC_NOPM,
			   RX_MACRO_EC1_MUX, 0,
			   &rx_mix_tx1_mux, rx_macro_enable_echo,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX MIX TX2 MUX", SND_SOC_NOPM,
			   RX_MACRO_EC2_MUX, 0,
			   &rx_mix_tx2_mux, rx_macro_enable_echo,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("IIR0", BOLERO_CDC_RX_SIDETONE_IIR0_IIR_PATH_CTL,
		4, 0, NULL, 0, rx_macro_set_iir_gain,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("IIR1", BOLERO_CDC_RX_SIDETONE_IIR1_IIR_PATH_CTL,
		4, 0, NULL, 0, rx_macro_set_iir_gain,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER("SRC0", BOLERO_CDC_RX_SIDETONE_SRC0_ST_SRC_PATH_CTL,
		4, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SRC1", BOLERO_CDC_RX_SIDETONE_SRC1_ST_SRC_PATH_CTL,
		4, 0, NULL, 0),

	RX_MACRO_DAPM_MUX("RX INT0 DEM MUX", 0, rx_int0_dem_inp),
	RX_MACRO_DAPM_MUX("RX INT1 DEM MUX", 0, rx_int1_dem_inp),

	SND_SOC_DAPM_MUX_E("RX INT0_2 MUX", SND_SOC_NOPM, INTERP_HPHL, 0,
		&rx_int0_2_mux, rx_macro_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT1_2 MUX", SND_SOC_NOPM, INTERP_HPHR, 0,
		&rx_int1_2_mux, rx_macro_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT2_2 MUX", SND_SOC_NOPM, INTERP_AUX, 0,
		&rx_int2_2_mux, rx_macro_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	RX_MACRO_DAPM_MUX("RX INT0_1 MIX1 INP0", 0, rx_int0_1_mix_inp0),
	RX_MACRO_DAPM_MUX("RX INT0_1 MIX1 INP1", 0, rx_int0_1_mix_inp1),
	RX_MACRO_DAPM_MUX("RX INT0_1 MIX1 INP2", 0, rx_int0_1_mix_inp2),
	RX_MACRO_DAPM_MUX("RX INT1_1 MIX1 INP0", 0, rx_int1_1_mix_inp0),
	RX_MACRO_DAPM_MUX("RX INT1_1 MIX1 INP1", 0, rx_int1_1_mix_inp1),
	RX_MACRO_DAPM_MUX("RX INT1_1 MIX1 INP2", 0, rx_int1_1_mix_inp2),
	RX_MACRO_DAPM_MUX("RX INT2_1 MIX1 INP0", 0, rx_int2_1_mix_inp0),
	RX_MACRO_DAPM_MUX("RX INT2_1 MIX1 INP1", 0, rx_int2_1_mix_inp1),
	RX_MACRO_DAPM_MUX("RX INT2_1 MIX1 INP2", 0, rx_int2_1_mix_inp2),

	SND_SOC_DAPM_MUX_E("RX INT0_1 INTERP", SND_SOC_NOPM, INTERP_HPHL, 0,
		&rx_int0_1_interp_mux, rx_macro_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT1_1 INTERP", SND_SOC_NOPM, INTERP_HPHR, 0,
		&rx_int1_1_interp_mux, rx_macro_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT2_1 INTERP", SND_SOC_NOPM, INTERP_AUX, 0,
		&rx_int2_1_interp_mux, rx_macro_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	RX_MACRO_DAPM_MUX("RX INT0_2 INTERP", 0, rx_int0_2_interp),
	RX_MACRO_DAPM_MUX("RX INT1_2 INTERP", 0, rx_int1_2_interp),
	RX_MACRO_DAPM_MUX("RX INT2_2 INTERP", 0, rx_int2_2_interp),

	SND_SOC_DAPM_MIXER("RX INT0_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT0 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT1_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT1 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT2_1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT2 SEC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX_E("RX INT0 MIX2 INP", SND_SOC_NOPM, INTERP_HPHL,
		0, &rx_int0_mix2_inp_mux, rx_macro_enable_rx_path_clk,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT1 MIX2 INP", SND_SOC_NOPM, INTERP_HPHR,
		0, &rx_int1_mix2_inp_mux, rx_macro_enable_rx_path_clk,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX INT2 MIX2 INP", SND_SOC_NOPM, INTERP_AUX,
		0, &rx_int2_mix2_inp_mux, rx_macro_enable_rx_path_clk,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("RX INT2_1 VBAT", SND_SOC_NOPM,
		0, 0, rx_int2_1_vbat_mix_switch,
		ARRAY_SIZE(rx_int2_1_vbat_mix_switch),
		rx_macro_enable_vbat,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("RX INT0 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT1 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX INT2 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("HPHL_OUT"),
	SND_SOC_DAPM_OUTPUT("HPHR_OUT"),
	SND_SOC_DAPM_OUTPUT("AUX_OUT"),
	SND_SOC_DAPM_OUTPUT("PCM_OUT"),

	SND_SOC_DAPM_INPUT("RX_TX DEC0_INP"),
	SND_SOC_DAPM_INPUT("RX_TX DEC1_INP"),
	SND_SOC_DAPM_INPUT("RX_TX DEC2_INP"),
	SND_SOC_DAPM_INPUT("RX_TX DEC3_INP"),

	SND_SOC_DAPM_SUPPLY_S("RX_MCLK", 0, SND_SOC_NOPM, 0, 0,
	rx_macro_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route rx_audio_map[] = {
	{"RX AIF1 PB", NULL, "RX_MCLK"},
	{"RX AIF2 PB", NULL, "RX_MCLK"},
	{"RX AIF3 PB", NULL, "RX_MCLK"},
	{"RX AIF4 PB", NULL, "RX_MCLK"},

	{"RX AIF6 PB", NULL, "RX_MCLK"},
	{"PCM_OUT", NULL, "RX AIF6 PB"},

	{"RX_MACRO RX0 MUX", "AIF1_PB", "RX AIF1 PB"},
	{"RX_MACRO RX1 MUX", "AIF1_PB", "RX AIF1 PB"},
	{"RX_MACRO RX2 MUX", "AIF1_PB", "RX AIF1 PB"},
	{"RX_MACRO RX3 MUX", "AIF1_PB", "RX AIF1 PB"},
	{"RX_MACRO RX4 MUX", "AIF1_PB", "RX AIF1 PB"},
	{"RX_MACRO RX5 MUX", "AIF1_PB", "RX AIF1 PB"},

	{"RX_MACRO RX0 MUX", "AIF2_PB", "RX AIF2 PB"},
	{"RX_MACRO RX1 MUX", "AIF2_PB", "RX AIF2 PB"},
	{"RX_MACRO RX2 MUX", "AIF2_PB", "RX AIF2 PB"},
	{"RX_MACRO RX3 MUX", "AIF2_PB", "RX AIF2 PB"},
	{"RX_MACRO RX4 MUX", "AIF2_PB", "RX AIF2 PB"},
	{"RX_MACRO RX5 MUX", "AIF2_PB", "RX AIF2 PB"},

	{"RX_MACRO RX0 MUX", "AIF3_PB", "RX AIF3 PB"},
	{"RX_MACRO RX1 MUX", "AIF3_PB", "RX AIF3 PB"},
	{"RX_MACRO RX2 MUX", "AIF3_PB", "RX AIF3 PB"},
	{"RX_MACRO RX3 MUX", "AIF3_PB", "RX AIF3 PB"},
	{"RX_MACRO RX4 MUX", "AIF3_PB", "RX AIF3 PB"},
	{"RX_MACRO RX5 MUX", "AIF3_PB", "RX AIF3 PB"},

	{"RX_MACRO RX0 MUX", "AIF4_PB", "RX AIF4 PB"},
	{"RX_MACRO RX1 MUX", "AIF4_PB", "RX AIF4 PB"},
	{"RX_MACRO RX2 MUX", "AIF4_PB", "RX AIF4 PB"},
	{"RX_MACRO RX3 MUX", "AIF4_PB", "RX AIF4 PB"},
	{"RX_MACRO RX4 MUX", "AIF4_PB", "RX AIF4 PB"},
	{"RX_MACRO RX5 MUX", "AIF4_PB", "RX AIF4 PB"},

	{"RX_RX0", NULL, "RX_MACRO RX0 MUX"},
	{"RX_RX1", NULL, "RX_MACRO RX1 MUX"},
	{"RX_RX2", NULL, "RX_MACRO RX2 MUX"},
	{"RX_RX3", NULL, "RX_MACRO RX3 MUX"},
	{"RX_RX4", NULL, "RX_MACRO RX4 MUX"},
	{"RX_RX5", NULL, "RX_MACRO RX5 MUX"},

	{"RX INT0_1 MIX1 INP0", "RX0", "RX_RX0"},
	{"RX INT0_1 MIX1 INP0", "RX1", "RX_RX1"},
	{"RX INT0_1 MIX1 INP0", "RX2", "RX_RX2"},
	{"RX INT0_1 MIX1 INP0", "RX3", "RX_RX3"},
	{"RX INT0_1 MIX1 INP0", "RX4", "RX_RX4"},
	{"RX INT0_1 MIX1 INP0", "RX5", "RX_RX5"},
	{"RX INT0_1 MIX1 INP0", "IIR0", "IIR0"},
	{"RX INT0_1 MIX1 INP0", "IIR1", "IIR1"},
	{"RX INT0_1 MIX1 INP0", "DEC0", "RX_TX DEC0_INP"},
	{"RX INT0_1 MIX1 INP0", "DEC1", "RX_TX DEC1_INP"},
	{"RX INT0_1 MIX1 INP1", "RX0", "RX_RX0"},
	{"RX INT0_1 MIX1 INP1", "RX1", "RX_RX1"},
	{"RX INT0_1 MIX1 INP1", "RX2", "RX_RX2"},
	{"RX INT0_1 MIX1 INP1", "RX3", "RX_RX3"},
	{"RX INT0_1 MIX1 INP1", "RX4", "RX_RX4"},
	{"RX INT0_1 MIX1 INP1", "RX5", "RX_RX5"},
	{"RX INT0_1 MIX1 INP1", "IIR0", "IIR0"},
	{"RX INT0_1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX INT0_1 MIX1 INP1", "DEC0", "RX_TX DEC0_INP"},
	{"RX INT0_1 MIX1 INP1", "DEC1", "RX_TX DEC1_INP"},
	{"RX INT0_1 MIX1 INP2", "RX0", "RX_RX0"},
	{"RX INT0_1 MIX1 INP2", "RX1", "RX_RX1"},
	{"RX INT0_1 MIX1 INP2", "RX2", "RX_RX2"},
	{"RX INT0_1 MIX1 INP2", "RX3", "RX_RX3"},
	{"RX INT0_1 MIX1 INP2", "RX4", "RX_RX4"},
	{"RX INT0_1 MIX1 INP2", "RX5", "RX_RX5"},
	{"RX INT0_1 MIX1 INP2", "IIR0", "IIR0"},
	{"RX INT0_1 MIX1 INP2", "IIR1", "IIR1"},
	{"RX INT0_1 MIX1 INP2", "DEC0", "RX_TX DEC0_INP"},
	{"RX INT0_1 MIX1 INP2", "DEC1", "RX_TX DEC1_INP"},

	{"RX INT1_1 MIX1 INP0", "RX0", "RX_RX0"},
	{"RX INT1_1 MIX1 INP0", "RX1", "RX_RX1"},
	{"RX INT1_1 MIX1 INP0", "RX2", "RX_RX2"},
	{"RX INT1_1 MIX1 INP0", "RX3", "RX_RX3"},
	{"RX INT1_1 MIX1 INP0", "RX4", "RX_RX4"},
	{"RX INT1_1 MIX1 INP0", "RX5", "RX_RX5"},
	{"RX INT1_1 MIX1 INP0", "IIR0", "IIR0"},
	{"RX INT1_1 MIX1 INP0", "IIR1", "IIR1"},
	{"RX INT1_1 MIX1 INP0", "DEC0", "RX_TX DEC0_INP"},
	{"RX INT1_1 MIX1 INP0", "DEC1", "RX_TX DEC1_INP"},
	{"RX INT1_1 MIX1 INP1", "RX0", "RX_RX0"},
	{"RX INT1_1 MIX1 INP1", "RX1", "RX_RX1"},
	{"RX INT1_1 MIX1 INP1", "RX2", "RX_RX2"},
	{"RX INT1_1 MIX1 INP1", "RX3", "RX_RX3"},
	{"RX INT1_1 MIX1 INP1", "RX4", "RX_RX4"},
	{"RX INT1_1 MIX1 INP1", "RX5", "RX_RX5"},
	{"RX INT1_1 MIX1 INP1", "IIR0", "IIR0"},
	{"RX INT1_1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX INT1_1 MIX1 INP1", "DEC0", "RX_TX DEC0_INP"},
	{"RX INT1_1 MIX1 INP1", "DEC1", "RX_TX DEC1_INP"},
	{"RX INT1_1 MIX1 INP2", "RX0", "RX_RX0"},
	{"RX INT1_1 MIX1 INP2", "RX1", "RX_RX1"},
	{"RX INT1_1 MIX1 INP2", "RX2", "RX_RX2"},
	{"RX INT1_1 MIX1 INP2", "RX3", "RX_RX3"},
	{"RX INT1_1 MIX1 INP2", "RX4", "RX_RX4"},
	{"RX INT1_1 MIX1 INP2", "RX5", "RX_RX5"},
	{"RX INT1_1 MIX1 INP2", "IIR0", "IIR0"},
	{"RX INT1_1 MIX1 INP2", "IIR1", "IIR1"},
	{"RX INT1_1 MIX1 INP2", "DEC0", "RX_TX DEC0_INP"},
	{"RX INT1_1 MIX1 INP2", "DEC1", "RX_TX DEC1_INP"},

	{"RX INT2_1 MIX1 INP0", "RX0", "RX_RX0"},
	{"RX INT2_1 MIX1 INP0", "RX1", "RX_RX1"},
	{"RX INT2_1 MIX1 INP0", "RX2", "RX_RX2"},
	{"RX INT2_1 MIX1 INP0", "RX3", "RX_RX3"},
	{"RX INT2_1 MIX1 INP0", "RX4", "RX_RX4"},
	{"RX INT2_1 MIX1 INP0", "RX5", "RX_RX5"},
	{"RX INT2_1 MIX1 INP0", "IIR0", "IIR0"},
	{"RX INT2_1 MIX1 INP0", "IIR1", "IIR1"},
	{"RX INT2_1 MIX1 INP0", "DEC0", "RX_TX DEC0_INP"},
	{"RX INT2_1 MIX1 INP0", "DEC1", "RX_TX DEC1_INP"},
	{"RX INT2_1 MIX1 INP1", "RX0", "RX_RX0"},
	{"RX INT2_1 MIX1 INP1", "RX1", "RX_RX1"},
	{"RX INT2_1 MIX1 INP1", "RX2", "RX_RX2"},
	{"RX INT2_1 MIX1 INP1", "RX3", "RX_RX3"},
	{"RX INT2_1 MIX1 INP1", "RX4", "RX_RX4"},
	{"RX INT2_1 MIX1 INP1", "RX5", "RX_RX5"},
	{"RX INT2_1 MIX1 INP1", "IIR0", "IIR0"},
	{"RX INT2_1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX INT2_1 MIX1 INP1", "DEC0", "RX_TX DEC0_INP"},
	{"RX INT2_1 MIX1 INP1", "DEC1", "RX_TX DEC1_INP"},
	{"RX INT2_1 MIX1 INP2", "RX0", "RX_RX0"},
	{"RX INT2_1 MIX1 INP2", "RX1", "RX_RX1"},
	{"RX INT2_1 MIX1 INP2", "RX2", "RX_RX2"},
	{"RX INT2_1 MIX1 INP2", "RX3", "RX_RX3"},
	{"RX INT2_1 MIX1 INP2", "RX4", "RX_RX4"},
	{"RX INT2_1 MIX1 INP2", "RX5", "RX_RX5"},
	{"RX INT2_1 MIX1 INP2", "IIR0", "IIR0"},
	{"RX INT2_1 MIX1 INP2", "IIR1", "IIR1"},
	{"RX INT2_1 MIX1 INP2", "DEC0", "RX_TX DEC0_INP"},
	{"RX INT2_1 MIX1 INP2", "DEC1", "RX_TX DEC1_INP"},

	{"RX INT0_1 MIX1", NULL, "RX INT0_1 MIX1 INP0"},
	{"RX INT0_1 MIX1", NULL, "RX INT0_1 MIX1 INP1"},
	{"RX INT0_1 MIX1", NULL, "RX INT0_1 MIX1 INP2"},
	{"RX INT1_1 MIX1", NULL, "RX INT1_1 MIX1 INP0"},
	{"RX INT1_1 MIX1", NULL, "RX INT1_1 MIX1 INP1"},
	{"RX INT1_1 MIX1", NULL, "RX INT1_1 MIX1 INP2"},
	{"RX INT2_1 MIX1", NULL, "RX INT2_1 MIX1 INP0"},
	{"RX INT2_1 MIX1", NULL, "RX INT2_1 MIX1 INP1"},
	{"RX INT2_1 MIX1", NULL, "RX INT2_1 MIX1 INP2"},

	{"RX MIX TX0 MUX", "RX_MIX0", "RX INT0 SEC MIX"},
	{"RX MIX TX0 MUX", "RX_MIX1", "RX INT1 SEC MIX"},
	{"RX MIX TX0 MUX", "RX_MIX2", "RX INT2 SEC MIX"},
	{"RX MIX TX1 MUX", "RX_MIX0", "RX INT0 SEC MIX"},
	{"RX MIX TX1 MUX", "RX_MIX1", "RX INT1 SEC MIX"},
	{"RX MIX TX1 MUX", "RX_MIX2", "RX INT2 SEC MIX"},
	{"RX MIX TX2 MUX", "RX_MIX0", "RX INT0 SEC MIX"},
	{"RX MIX TX2 MUX", "RX_MIX1", "RX INT1 SEC MIX"},
	{"RX MIX TX2 MUX", "RX_MIX2", "RX INT2 SEC MIX"},
	{"RX AIF_ECHO", NULL, "RX MIX TX0 MUX"},
	{"RX AIF_ECHO", NULL, "RX MIX TX1 MUX"},
	{"RX AIF_ECHO", NULL, "RX MIX TX2 MUX"},
	{"RX AIF_ECHO", NULL, "RX_MCLK"},

	/* Mixing path INT0 */
	{"RX INT0_2 MUX", "RX0", "RX_RX0"},
	{"RX INT0_2 MUX", "RX1", "RX_RX1"},
	{"RX INT0_2 MUX", "RX2", "RX_RX2"},
	{"RX INT0_2 MUX", "RX3", "RX_RX3"},
	{"RX INT0_2 MUX", "RX4", "RX_RX4"},
	{"RX INT0_2 MUX", "RX5", "RX_RX5"},
	{"RX INT0_2 INTERP", NULL, "RX INT0_2 MUX"},
	{"RX INT0 SEC MIX", NULL, "RX INT0_2 INTERP"},

	/* Mixing path INT1 */
	{"RX INT1_2 MUX", "RX0", "RX_RX0"},
	{"RX INT1_2 MUX", "RX1", "RX_RX1"},
	{"RX INT1_2 MUX", "RX2", "RX_RX2"},
	{"RX INT1_2 MUX", "RX3", "RX_RX3"},
	{"RX INT1_2 MUX", "RX4", "RX_RX4"},
	{"RX INT1_2 MUX", "RX5", "RX_RX5"},
	{"RX INT1_2 INTERP", NULL, "RX INT1_2 MUX"},
	{"RX INT1 SEC MIX", NULL, "RX INT1_2 INTERP"},

	/* Mixing path INT2 */
	{"RX INT2_2 MUX", "RX0", "RX_RX0"},
	{"RX INT2_2 MUX", "RX1", "RX_RX1"},
	{"RX INT2_2 MUX", "RX2", "RX_RX2"},
	{"RX INT2_2 MUX", "RX3", "RX_RX3"},
	{"RX INT2_2 MUX", "RX4", "RX_RX4"},
	{"RX INT2_2 MUX", "RX5", "RX_RX5"},
	{"RX INT2_2 INTERP", NULL, "RX INT2_2 MUX"},
	{"RX INT2 SEC MIX", NULL, "RX INT2_2 INTERP"},

	{"RX INT0_1 INTERP", NULL, "RX INT0_1 MIX1"},
	{"RX INT0 SEC MIX", NULL, "RX INT0_1 INTERP"},
	{"RX INT0 MIX2", NULL, "RX INT0 SEC MIX"},
	{"RX INT0 MIX2", NULL, "RX INT0 MIX2 INP"},
	{"RX INT0 DEM MUX", "CLSH_DSM_OUT", "RX INT0 MIX2"},
	{"HPHL_OUT", NULL, "RX INT0 DEM MUX"},
	{"HPHL_OUT", NULL, "RX_MCLK"},

	{"RX INT1_1 INTERP", NULL, "RX INT1_1 MIX1"},
	{"RX INT1 SEC MIX", NULL, "RX INT1_1 INTERP"},
	{"RX INT1 MIX2", NULL, "RX INT1 SEC MIX"},
	{"RX INT1 MIX2", NULL, "RX INT1 MIX2 INP"},
	{"RX INT1 DEM MUX", "CLSH_DSM_OUT", "RX INT1 MIX2"},
	{"HPHR_OUT", NULL, "RX INT1 DEM MUX"},
	{"HPHR_OUT", NULL, "RX_MCLK"},

	{"RX INT2_1 INTERP", NULL, "RX INT2_1 MIX1"},

	{"RX INT2_1 VBAT", "RX AUX VBAT Enable", "RX INT2_1 INTERP"},
	{"RX INT2 SEC MIX", NULL, "RX INT2_1 VBAT"},

	{"RX INT2 SEC MIX", NULL, "RX INT2_1 INTERP"},
	{"RX INT2 MIX2", NULL, "RX INT2 SEC MIX"},
	{"RX INT2 MIX2", NULL, "RX INT2 MIX2 INP"},
	{"AUX_OUT", NULL, "RX INT2 MIX2"},
	{"AUX_OUT", NULL, "RX_MCLK"},

	{"IIR0", NULL, "RX_MCLK"},
	{"IIR0", NULL, "IIR0 INP0 MUX"},
	{"IIR0 INP0 MUX", "DEC0", "RX_TX DEC0_INP"},
	{"IIR0 INP0 MUX", "DEC1", "RX_TX DEC1_INP"},
	{"IIR0 INP0 MUX", "DEC2", "RX_TX DEC2_INP"},
	{"IIR0 INP0 MUX", "DEC3", "RX_TX DEC3_INP"},
	{"IIR0 INP0 MUX", "RX0", "RX_RX0"},
	{"IIR0 INP0 MUX", "RX1", "RX_RX1"},
	{"IIR0 INP0 MUX", "RX2", "RX_RX2"},
	{"IIR0 INP0 MUX", "RX3", "RX_RX3"},
	{"IIR0 INP0 MUX", "RX4", "RX_RX4"},
	{"IIR0 INP0 MUX", "RX5", "RX_RX5"},
	{"IIR0", NULL, "IIR0 INP1 MUX"},
	{"IIR0 INP1 MUX", "DEC0", "RX_TX DEC0_INP"},
	{"IIR0 INP1 MUX", "DEC1", "RX_TX DEC1_INP"},
	{"IIR0 INP1 MUX", "DEC2", "RX_TX DEC2_INP"},
	{"IIR0 INP1 MUX", "DEC3", "RX_TX DEC3_INP"},
	{"IIR0 INP1 MUX", "RX0", "RX_RX0"},
	{"IIR0 INP1 MUX", "RX1", "RX_RX1"},
	{"IIR0 INP1 MUX", "RX2", "RX_RX2"},
	{"IIR0 INP1 MUX", "RX3", "RX_RX3"},
	{"IIR0 INP1 MUX", "RX4", "RX_RX4"},
	{"IIR0 INP1 MUX", "RX5", "RX_RX5"},
	{"IIR0", NULL, "IIR0 INP2 MUX"},
	{"IIR0 INP2 MUX", "DEC0", "RX_TX DEC0_INP"},
	{"IIR0 INP2 MUX", "DEC1", "RX_TX DEC1_INP"},
	{"IIR0 INP2 MUX", "DEC2", "RX_TX DEC2_INP"},
	{"IIR0 INP2 MUX", "DEC3", "RX_TX DEC3_INP"},
	{"IIR0 INP2 MUX", "RX0", "RX_RX0"},
	{"IIR0 INP2 MUX", "RX1", "RX_RX1"},
	{"IIR0 INP2 MUX", "RX2", "RX_RX2"},
	{"IIR0 INP2 MUX", "RX3", "RX_RX3"},
	{"IIR0 INP2 MUX", "RX4", "RX_RX4"},
	{"IIR0 INP2 MUX", "RX5", "RX_RX5"},
	{"IIR0", NULL, "IIR0 INP3 MUX"},
	{"IIR0 INP3 MUX", "DEC0", "RX_TX DEC0_INP"},
	{"IIR0 INP3 MUX", "DEC1", "RX_TX DEC1_INP"},
	{"IIR0 INP3 MUX", "DEC2", "RX_TX DEC2_INP"},
	{"IIR0 INP3 MUX", "DEC3", "RX_TX DEC3_INP"},
	{"IIR0 INP3 MUX", "RX0", "RX_RX0"},
	{"IIR0 INP3 MUX", "RX1", "RX_RX1"},
	{"IIR0 INP3 MUX", "RX2", "RX_RX2"},
	{"IIR0 INP3 MUX", "RX3", "RX_RX3"},
	{"IIR0 INP3 MUX", "RX4", "RX_RX4"},
	{"IIR0 INP3 MUX", "RX5", "RX_RX5"},

	{"IIR1", NULL, "RX_MCLK"},
	{"IIR1", NULL, "IIR1 INP0 MUX"},
	{"IIR1 INP0 MUX", "DEC0", "RX_TX DEC0_INP"},
	{"IIR1 INP0 MUX", "DEC1", "RX_TX DEC1_INP"},
	{"IIR1 INP0 MUX", "DEC2", "RX_TX DEC2_INP"},
	{"IIR1 INP0 MUX", "DEC3", "RX_TX DEC3_INP"},
	{"IIR1 INP0 MUX", "RX0", "RX_RX0"},
	{"IIR1 INP0 MUX", "RX1", "RX_RX1"},
	{"IIR1 INP0 MUX", "RX2", "RX_RX2"},
	{"IIR1 INP0 MUX", "RX3", "RX_RX3"},
	{"IIR1 INP0 MUX", "RX4", "RX_RX4"},
	{"IIR1 INP0 MUX", "RX5", "RX_RX5"},
	{"IIR1", NULL, "IIR1 INP1 MUX"},
	{"IIR1 INP1 MUX", "DEC0", "RX_TX DEC0_INP"},
	{"IIR1 INP1 MUX", "DEC1", "RX_TX DEC1_INP"},
	{"IIR1 INP1 MUX", "DEC2", "RX_TX DEC2_INP"},
	{"IIR1 INP1 MUX", "DEC3", "RX_TX DEC3_INP"},
	{"IIR1 INP1 MUX", "RX0", "RX_RX0"},
	{"IIR1 INP1 MUX", "RX1", "RX_RX1"},
	{"IIR1 INP1 MUX", "RX2", "RX_RX2"},
	{"IIR1 INP1 MUX", "RX3", "RX_RX3"},
	{"IIR1 INP1 MUX", "RX4", "RX_RX4"},
	{"IIR1 INP1 MUX", "RX5", "RX_RX5"},
	{"IIR1", NULL, "IIR1 INP2 MUX"},
	{"IIR1 INP2 MUX", "DEC0", "RX_TX DEC0_INP"},
	{"IIR1 INP2 MUX", "DEC1", "RX_TX DEC1_INP"},
	{"IIR1 INP2 MUX", "DEC2", "RX_TX DEC2_INP"},
	{"IIR1 INP2 MUX", "DEC3", "RX_TX DEC3_INP"},
	{"IIR1 INP2 MUX", "RX0", "RX_RX0"},
	{"IIR1 INP2 MUX", "RX1", "RX_RX1"},
	{"IIR1 INP2 MUX", "RX2", "RX_RX2"},
	{"IIR1 INP2 MUX", "RX3", "RX_RX3"},
	{"IIR1 INP2 MUX", "RX4", "RX_RX4"},
	{"IIR1 INP2 MUX", "RX5", "RX_RX5"},
	{"IIR1", NULL, "IIR1 INP3 MUX"},
	{"IIR1 INP3 MUX", "DEC0", "RX_TX DEC0_INP"},
	{"IIR1 INP3 MUX", "DEC1", "RX_TX DEC1_INP"},
	{"IIR1 INP3 MUX", "DEC2", "RX_TX DEC2_INP"},
	{"IIR1 INP3 MUX", "DEC3", "RX_TX DEC3_INP"},
	{"IIR1 INP3 MUX", "RX0", "RX_RX0"},
	{"IIR1 INP3 MUX", "RX1", "RX_RX1"},
	{"IIR1 INP3 MUX", "RX2", "RX_RX2"},
	{"IIR1 INP3 MUX", "RX3", "RX_RX3"},
	{"IIR1 INP3 MUX", "RX4", "RX_RX4"},
	{"IIR1 INP3 MUX", "RX5", "RX_RX5"},

	{"SRC0", NULL, "IIR0"},
	{"SRC1", NULL, "IIR1"},
	{"RX INT0 MIX2 INP", "SRC0", "SRC0"},
	{"RX INT0 MIX2 INP", "SRC1", "SRC1"},
	{"RX INT1 MIX2 INP", "SRC0", "SRC0"},
	{"RX INT1 MIX2 INP", "SRC1", "SRC1"},
	{"RX INT2 MIX2 INP", "SRC0", "SRC0"},
	{"RX INT2 MIX2 INP", "SRC1", "SRC1"},
};

static int rx_macro_core_vote(void *handle, bool enable)
{
	int rc = 0;
	struct rx_macro_priv *rx_priv = (struct rx_macro_priv *) handle;

	if (rx_priv == NULL) {
		pr_err("%s: rx priv data is NULL\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		pm_runtime_get_sync(rx_priv->dev);
		if (bolero_check_core_votes(rx_priv->dev))
			rc = 0;
		else
			rc = -ENOTSYNC;
	} else {
		pm_runtime_put_autosuspend(rx_priv->dev);
		pm_runtime_mark_last_busy(rx_priv->dev);
	}
	return rc;
}

static int rx_swrm_clock(void *handle, bool enable)
{
	struct rx_macro_priv *rx_priv = (struct rx_macro_priv *) handle;
	struct regmap *regmap = dev_get_regmap(rx_priv->dev->parent, NULL);
	int ret = 0;

	if (regmap == NULL) {
		dev_err(rx_priv->dev, "%s: regmap is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&rx_priv->swr_clk_lock);

	trace_printk("%s: swrm clock %s\n",
			__func__, (enable ? "enable" : "disable"));
	dev_dbg(rx_priv->dev, "%s: swrm clock %s\n",
		__func__, (enable ? "enable" : "disable"));
	if (enable) {
		pm_runtime_get_sync(rx_priv->dev);
		if (rx_priv->swr_clk_users == 0) {
			ret = msm_cdc_pinctrl_select_active_state(
						rx_priv->rx_swr_gpio_p);
			if (ret < 0) {
				dev_err(rx_priv->dev,
					"%s: rx swr pinctrl enable failed\n",
					__func__);
				pm_runtime_mark_last_busy(rx_priv->dev);
				pm_runtime_put_autosuspend(rx_priv->dev);
				goto exit;
			}
			ret = rx_macro_mclk_enable(rx_priv, 1, true);
			if (ret < 0) {
				msm_cdc_pinctrl_select_sleep_state(
						rx_priv->rx_swr_gpio_p);
				dev_err(rx_priv->dev,
					"%s: rx request clock enable failed\n",
					__func__);
				pm_runtime_mark_last_busy(rx_priv->dev);
				pm_runtime_put_autosuspend(rx_priv->dev);
				goto exit;
			}
			if (rx_priv->reset_swr)
				regmap_update_bits(regmap,
					BOLERO_CDC_RX_CLK_RST_CTRL_SWR_CONTROL,
					0x02, 0x02);
			regmap_update_bits(regmap,
				BOLERO_CDC_RX_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x01);
			if (rx_priv->reset_swr)
				regmap_update_bits(regmap,
					BOLERO_CDC_RX_CLK_RST_CTRL_SWR_CONTROL,
					0x02, 0x00);
			rx_priv->reset_swr = false;
		}
		pm_runtime_mark_last_busy(rx_priv->dev);
		pm_runtime_put_autosuspend(rx_priv->dev);
		rx_priv->swr_clk_users++;
	} else {
		if (rx_priv->swr_clk_users <= 0) {
			dev_err(rx_priv->dev,
				"%s: rx swrm clock users already reset\n",
				__func__);
			rx_priv->swr_clk_users = 0;
			goto exit;
		}
		rx_priv->swr_clk_users--;
		if (rx_priv->swr_clk_users == 0) {
			regmap_update_bits(regmap,
				BOLERO_CDC_RX_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x00);
			rx_macro_mclk_enable(rx_priv, 0, true);
			ret = msm_cdc_pinctrl_select_sleep_state(
						rx_priv->rx_swr_gpio_p);
			if (ret < 0) {
				dev_err(rx_priv->dev,
					"%s: rx swr pinctrl disable failed\n",
					__func__);
				goto exit;
			}
		}
	}
	trace_printk("%s: swrm clock users %d\n",
		__func__, rx_priv->swr_clk_users);
	dev_dbg(rx_priv->dev, "%s: swrm clock users %d\n",
		__func__, rx_priv->swr_clk_users);
exit:
	mutex_unlock(&rx_priv->swr_clk_lock);
	return ret;
}

static const struct rx_macro_reg_mask_val rx_macro_reg_init[] = {
	{BOLERO_CDC_RX_RX0_RX_PATH_SEC7, 0x07, 0x02},
	{BOLERO_CDC_RX_RX1_RX_PATH_SEC7, 0x07, 0x02},
	{BOLERO_CDC_RX_RX2_RX_PATH_SEC7, 0x07, 0x02},
	{BOLERO_CDC_RX_RX0_RX_PATH_CFG3, 0x03, 0x02},
	{BOLERO_CDC_RX_RX1_RX_PATH_CFG3, 0x03, 0x02},
	{BOLERO_CDC_RX_RX2_RX_PATH_CFG3, 0x03, 0x02},
};

static void rx_macro_init_bcl_pmic_reg(struct snd_soc_component *component)
{
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!component) {
		pr_err("%s: NULL component pointer!\n", __func__);
		return;
	}

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return;

	switch (rx_priv->bcl_pmic_params.id) {
	case 0:
		/* Enable ID0 to listen to respective PMIC group interrupts */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_DECODE_CTL1, 0x02, 0x02);
		/* Update MC_SID0 */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_DECODE_CFG1, 0x0F,
			rx_priv->bcl_pmic_params.sid);
		/* Update MC_PPID0 */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_DECODE_CFG2, 0xFF,
			rx_priv->bcl_pmic_params.ppid);
		break;
	case 1:
		/* Enable ID1 to listen to respective PMIC group interrupts */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_DECODE_CTL1, 0x01, 0x01);
		/* Update MC_SID1 */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_DECODE_CFG3, 0x0F,
			rx_priv->bcl_pmic_params.sid);
		/* Update MC_PPID1 */
		snd_soc_component_update_bits(component,
			BOLERO_CDC_RX_BCL_VBAT_DECODE_CFG1, 0xFF,
			rx_priv->bcl_pmic_params.ppid);
		break;
	default:
		dev_err(rx_dev, "%s: PMIC ID is invalid %d\n",
		       __func__, rx_priv->bcl_pmic_params.id);
		break;
	}
}

static int rx_macro_init(struct snd_soc_component *component)
{
	struct snd_soc_dapm_context *dapm =
				snd_soc_component_get_dapm(component);
	int ret = 0;
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;
	int i;

	rx_dev = bolero_get_device_ptr(component->dev, RX_MACRO);
	if (!rx_dev) {
		dev_err(component->dev,
			"%s: null device for macro!\n", __func__);
		return -EINVAL;
	}
	rx_priv = dev_get_drvdata(rx_dev);
	if (!rx_priv) {
		dev_err(component->dev,
			"%s: priv is null for macro!\n", __func__);
		return -EINVAL;
	}

	ret = snd_soc_dapm_new_controls(dapm, rx_macro_dapm_widgets,
					ARRAY_SIZE(rx_macro_dapm_widgets));
	if (ret < 0) {
		dev_err(rx_dev, "%s: failed to add controls\n", __func__);
		return ret;
	}
	ret = snd_soc_dapm_add_routes(dapm, rx_audio_map,
					ARRAY_SIZE(rx_audio_map));
	if (ret < 0) {
		dev_err(rx_dev, "%s: failed to add routes\n", __func__);
		return ret;
	}
	ret = snd_soc_dapm_new_widgets(dapm->card);
	if (ret < 0) {
		dev_err(rx_dev, "%s: failed to add widgets\n", __func__);
		return ret;
	}
	ret = snd_soc_add_component_controls(component, rx_macro_snd_controls,
				   ARRAY_SIZE(rx_macro_snd_controls));
	if (ret < 0) {
		dev_err(rx_dev, "%s: failed to add snd_ctls\n", __func__);
		return ret;
	}
	rx_priv->dev_up = true;
	rx_priv->rx0_gain_val = 0;
	rx_priv->rx1_gain_val = 0;
	snd_soc_dapm_ignore_suspend(dapm, "RX_MACRO_AIF1 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "RX_MACRO_AIF2 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "RX_MACRO_AIF3 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "RX_MACRO_AIF4 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "RX_MACRO_AIF5 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "RX_MACRO_AIF6 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "HPHL_OUT");
	snd_soc_dapm_ignore_suspend(dapm, "HPHR_OUT");
	snd_soc_dapm_ignore_suspend(dapm, "AUX_OUT");
	snd_soc_dapm_ignore_suspend(dapm, "PCM_OUT");
	snd_soc_dapm_ignore_suspend(dapm, "RX_TX DEC0_INP");
	snd_soc_dapm_ignore_suspend(dapm, "RX_TX DEC1_INP");
	snd_soc_dapm_ignore_suspend(dapm, "RX_TX DEC2_INP");
	snd_soc_dapm_ignore_suspend(dapm, "RX_TX DEC3_INP");
	snd_soc_dapm_sync(dapm);

	for (i = 0; i < ARRAY_SIZE(rx_macro_reg_init); i++)
		snd_soc_component_update_bits(component,
				rx_macro_reg_init[i].reg,
				rx_macro_reg_init[i].mask,
				rx_macro_reg_init[i].val);

	rx_priv->component = component;
	rx_macro_init_bcl_pmic_reg(component);

	return 0;
}

static int rx_macro_deinit(struct snd_soc_component *component)
{
	struct device *rx_dev = NULL;
	struct rx_macro_priv *rx_priv = NULL;

	if (!rx_macro_get_data(component, &rx_dev, &rx_priv, __func__))
		return -EINVAL;

	rx_priv->component = NULL;

	return 0;
}

static void rx_macro_add_child_devices(struct work_struct *work)
{
	struct rx_macro_priv *rx_priv = NULL;
	struct platform_device *pdev = NULL;
	struct device_node *node = NULL;
	struct rx_swr_ctrl_data *swr_ctrl_data = NULL, *temp = NULL;
	int ret = 0;
	u16 count = 0, ctrl_num = 0;
	struct rx_swr_ctrl_platform_data *platdata = NULL;
	char plat_dev_name[RX_SWR_STRING_LEN] = "";
	bool rx_swr_master_node = false;

	rx_priv = container_of(work, struct rx_macro_priv,
			     rx_macro_add_child_devices_work);
	if (!rx_priv) {
		pr_err("%s: Memory for rx_priv does not exist\n",
			__func__);
		return;
	}

	if (!rx_priv->dev) {
		pr_err("%s: RX device does not exist\n", __func__);
		return;
	}

	if(!rx_priv->dev->of_node) {
		dev_err(rx_priv->dev,
			"%s: DT node for RX dev does not exist\n", __func__);
		return;
	}

	platdata = &rx_priv->swr_plat_data;
	rx_priv->child_count = 0;

	for_each_available_child_of_node(rx_priv->dev->of_node, node) {
		rx_swr_master_node = false;
		if (strnstr(node->name, "rx_swr_master",
				strlen("rx_swr_master")) != NULL)
			rx_swr_master_node = true;

		if(rx_swr_master_node)
			strlcpy(plat_dev_name, "rx_swr_ctrl",
				(RX_SWR_STRING_LEN - 1));
		else
			strlcpy(plat_dev_name, node->name,
				(RX_SWR_STRING_LEN - 1));

		pdev = platform_device_alloc(plat_dev_name, -1);
		if (!pdev) {
			dev_err(rx_priv->dev, "%s: pdev memory alloc failed\n",
				__func__);
			ret = -ENOMEM;
			goto err;
		}
		pdev->dev.parent = rx_priv->dev;
		pdev->dev.of_node = node;

		if (rx_swr_master_node) {
			ret = platform_device_add_data(pdev, platdata,
						       sizeof(*platdata));
			if (ret) {
				dev_err(&pdev->dev,
					"%s: cannot add plat data ctrl:%d\n",
					__func__, ctrl_num);
				goto fail_pdev_add;
			}

			temp = krealloc(swr_ctrl_data,
					(ctrl_num + 1) * sizeof(
					struct rx_swr_ctrl_data),
					GFP_KERNEL);
			if (!temp) {
				ret = -ENOMEM;
				goto fail_pdev_add;
			}
			swr_ctrl_data = temp;
			swr_ctrl_data[ctrl_num].rx_swr_pdev = pdev;
			ctrl_num++;
			dev_dbg(&pdev->dev,
				"%s: Adding soundwire ctrl device(s)\n",
				__func__);
			rx_priv->swr_ctrl_data = swr_ctrl_data;
		}

		ret = platform_device_add(pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: Cannot add platform device\n",
				__func__);
			goto fail_pdev_add;
		}

		if (rx_priv->child_count < RX_MACRO_CHILD_DEVICES_MAX)
			rx_priv->pdev_child_devices[
					rx_priv->child_count++] = pdev;
		else
			goto err;
	}
	return;
fail_pdev_add:
	for (count = 0; count < rx_priv->child_count; count++)
		platform_device_put(rx_priv->pdev_child_devices[count]);
err:
	return;
}

static void rx_macro_init_ops(struct macro_ops *ops, char __iomem *rx_io_base)
{
	memset(ops, 0, sizeof(struct macro_ops));
	ops->init = rx_macro_init;
	ops->exit = rx_macro_deinit;
	ops->io_base = rx_io_base;
	ops->dai_ptr = rx_macro_dai;
	ops->num_dais = ARRAY_SIZE(rx_macro_dai);
	ops->event_handler = rx_macro_event_handler;
	ops->set_port_map = rx_macro_set_port_map;
}

static int rx_macro_probe(struct platform_device *pdev)
{
	struct macro_ops ops = {0};
	struct rx_macro_priv *rx_priv = NULL;
	u32 rx_base_addr = 0, muxsel = 0;
	char __iomem *rx_io_base = NULL, *muxsel_io = NULL;
	int ret = 0;
	u8 bcl_pmic_params[3];
	u32 default_clk_id = 0;
	u32 is_used_rx_swr_gpio = 1;
	const char *is_used_rx_swr_gpio_dt = "qcom,is-used-swr-gpio";

	if (!bolero_is_va_macro_registered(&pdev->dev)) {
		dev_err(&pdev->dev,
			"%s: va-macro not registered yet, defer\n", __func__);
		return -EPROBE_DEFER;
	}

	rx_priv = devm_kzalloc(&pdev->dev, sizeof(struct rx_macro_priv),
			    GFP_KERNEL);
	if (!rx_priv)
		return -ENOMEM;

	rx_priv->dev = &pdev->dev;
	ret = of_property_read_u32(pdev->dev.of_node, "reg",
				   &rx_base_addr);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "reg");
		return ret;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "qcom,rx_mclk_mode_muxsel",
				   &muxsel);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "reg");
		return ret;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "qcom,default-clk-id",
				   &default_clk_id);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "qcom,default-clk-id");
		default_clk_id = RX_CORE_CLK;
	}
	if (of_find_property(pdev->dev.of_node, is_used_rx_swr_gpio_dt,
			     NULL)) {
		ret = of_property_read_u32(pdev->dev.of_node,
					   is_used_rx_swr_gpio_dt,
					   &is_used_rx_swr_gpio);
		if (ret) {
			dev_err(&pdev->dev, "%s: error reading %s in dt\n",
				__func__, is_used_rx_swr_gpio_dt);
			is_used_rx_swr_gpio = 1;
		}
	}
	rx_priv->rx_swr_gpio_p = of_parse_phandle(pdev->dev.of_node,
					"qcom,rx-swr-gpios", 0);
	if (!rx_priv->rx_swr_gpio_p && is_used_rx_swr_gpio) {
		dev_err(&pdev->dev, "%s: swr_gpios handle not provided!\n",
			__func__);
		return -EINVAL;
	}
	if (msm_cdc_pinctrl_get_state(rx_priv->rx_swr_gpio_p) < 0 &&
		is_used_rx_swr_gpio) {
		dev_err(&pdev->dev, "%s: failed to get swr pin state\n",
			__func__);
		return -EPROBE_DEFER;
	}
	msm_cdc_pinctrl_set_wakeup_capable(
				rx_priv->rx_swr_gpio_p, false);

	rx_io_base = devm_ioremap(&pdev->dev, rx_base_addr,
				  RX_MACRO_MAX_OFFSET);
	if (!rx_io_base) {
		dev_err(&pdev->dev, "%s: ioremap failed\n", __func__);
		return -ENOMEM;
	}
	rx_priv->rx_io_base = rx_io_base;
	muxsel_io = devm_ioremap(&pdev->dev, muxsel, 0x4);
	if (!muxsel_io) {
		dev_err(&pdev->dev, "%s: ioremap failed for muxsel\n",
			__func__);
		return -ENOMEM;
	}
	rx_priv->rx_mclk_mode_muxsel = muxsel_io;
	rx_priv->reset_swr = true;
	INIT_WORK(&rx_priv->rx_macro_add_child_devices_work,
		  rx_macro_add_child_devices);
	rx_priv->swr_plat_data.handle = (void *) rx_priv;
	rx_priv->swr_plat_data.read = NULL;
	rx_priv->swr_plat_data.write = NULL;
	rx_priv->swr_plat_data.bulk_write = NULL;
	rx_priv->swr_plat_data.clk = rx_swrm_clock;
	rx_priv->swr_plat_data.core_vote = rx_macro_core_vote;
	rx_priv->swr_plat_data.handle_irq = NULL;

	ret = of_property_read_u8_array(pdev->dev.of_node,
				"qcom,rx-bcl-pmic-params", bcl_pmic_params,
				sizeof(bcl_pmic_params));
	if (ret) {
		dev_dbg(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "qcom,rx-bcl-pmic-params");
	} else {
		rx_priv->bcl_pmic_params.id = bcl_pmic_params[0];
		rx_priv->bcl_pmic_params.sid = bcl_pmic_params[1];
		rx_priv->bcl_pmic_params.ppid = bcl_pmic_params[2];
	}
	rx_priv->clk_id = default_clk_id;
	rx_priv->default_clk_id  = default_clk_id;
	ops.clk_id_req = rx_priv->clk_id;
	ops.default_clk_id = default_clk_id;

	rx_priv->is_aux_hpf_on = 1;

	dev_set_drvdata(&pdev->dev, rx_priv);
	mutex_init(&rx_priv->mclk_lock);
	mutex_init(&rx_priv->swr_clk_lock);
	rx_macro_init_ops(&ops, rx_io_base);

	ret = bolero_register_macro(&pdev->dev, RX_MACRO, &ops);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: register macro failed\n", __func__);
		goto err_reg_macro;
	}
	pm_runtime_set_autosuspend_delay(&pdev->dev, AUTO_SUSPEND_DELAY);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, true);
	pm_runtime_enable(&pdev->dev);
	schedule_work(&rx_priv->rx_macro_add_child_devices_work);

	return 0;

err_reg_macro:
	mutex_destroy(&rx_priv->mclk_lock);
	mutex_destroy(&rx_priv->swr_clk_lock);
	return ret;
}

static int rx_macro_remove(struct platform_device *pdev)
{
	struct rx_macro_priv *rx_priv = NULL;
	u16 count = 0;

	rx_priv = dev_get_drvdata(&pdev->dev);

	if (!rx_priv)
		return -EINVAL;

	for (count = 0; count < rx_priv->child_count &&
		count < RX_MACRO_CHILD_DEVICES_MAX; count++)
		platform_device_unregister(rx_priv->pdev_child_devices[count]);

	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	bolero_unregister_macro(&pdev->dev, RX_MACRO);
	mutex_destroy(&rx_priv->mclk_lock);
	mutex_destroy(&rx_priv->swr_clk_lock);
	kfree(rx_priv->swr_ctrl_data);
	return 0;
}

static const struct of_device_id rx_macro_dt_match[] = {
	{.compatible = "qcom,rx-macro"},
	{}
};

static const struct dev_pm_ops bolero_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		pm_runtime_force_suspend,
		pm_runtime_force_resume
	)
	SET_RUNTIME_PM_OPS(
		bolero_runtime_suspend,
		bolero_runtime_resume,
		NULL
	)
};

static struct platform_driver rx_macro_driver = {
	.driver = {
		.name = "rx_macro",
		.owner = THIS_MODULE,
		.pm = &bolero_dev_pm_ops,
		.of_match_table = rx_macro_dt_match,
		.suppress_bind_attrs = true,
	},
	.probe = rx_macro_probe,
	.remove = rx_macro_remove,
};

module_platform_driver(rx_macro_driver);

MODULE_DESCRIPTION("RX macro driver");
MODULE_LICENSE("GPL v2");
