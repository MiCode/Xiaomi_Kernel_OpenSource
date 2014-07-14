/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/qpnp/clkdiv.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/slimbus/slimbus.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/liquid_dock.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <sound/q6core.h>
#include <sound/pcm_params.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "../codecs/wcd9xxx-common.h"
#include "../codecs/wcd9320.h"
#include "../codecs/wcd9330.h"

#define DRV_NAME "apq8084-asoc"

#define APQ8084_SPK_ON 1

static int slim0_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int hdmi_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int mi2s_tx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int mi2s_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;

#define SAMPLING_RATE_8KHZ 8000
#define SAMPLING_RATE_16KHZ 16000
#define SAMPLING_RATE_48KHZ 48000
#define SAMPLING_RATE_96KHZ 96000
#define SAMPLING_RATE_192KHZ 192000

static int apq8084_auxpcm_rate = 8000;
#define LO_1_SPK_AMP	0x1
#define LO_3_SPK_AMP	0x2
#define LO_2_SPK_AMP	0x4
#define LO_4_SPK_AMP	0x8

#define LPAIF_OFFSET 0xFE000000
#define LPAIF_PRI_MODE_MUXSEL (LPAIF_OFFSET + 0x34000)
#define LPAIF_SEC_MODE_MUXSEL (LPAIF_OFFSET + 0x35000)
#define LPAIF_TER_MODE_MUXSEL (LPAIF_OFFSET + 0x36000)
#define LPAIF_QUAD_MODE_MUXSEL (LPAIF_OFFSET + 0x37000)

#define I2S_PCM_SEL 1
#define I2S_PCM_SEL_OFFSET 1


#define WCD9XXX_MBHC_DEF_BUTTONS 8
#define WCD9XXX_MBHC_DEF_RLOADS 5
#define TAIKO_EXT_CLK_RATE 9600000

/* It takes about 13ms for Class-D PAs to ramp-up */
#define EXT_CLASS_D_EN_DELAY 13000
#define EXT_CLASS_D_DIS_DELAY 3000
#define EXT_CLASS_D_DELAY_DELTA 2000

/* It takes about 13ms for Class-AB PAs to ramp-up */
#define EXT_CLASS_AB_EN_DELAY 10000
#define EXT_CLASS_AB_DIS_DELAY 1000
#define EXT_CLASS_AB_DELAY_DELTA 1000

#define NUM_OF_AUXPCM_GPIOS 4

static void *adsp_state_notifier;

#define ADSP_STATE_READY_TIMEOUT_MS 3000

struct cpe_load_priv {
	void *cdc_handle;
	struct kobject *cpe_load_kobj;
	struct attribute_group *attr_group;
};

static int cpe_load;

static ssize_t cpe_load_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count);

static struct kobj_attribute cpe_load_attr =
	__ATTR(cpe_load, 0600, NULL, cpe_load_store);

static struct attribute *attrs[] = {
	&cpe_load_attr.attr,
	NULL,
};

static struct attribute_group attr_grp = {
	.attrs = attrs,
};

static struct cpe_load_priv cpe_priv;

static inline int param_is_mask(int p)
{
	return ((p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK));
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
{
	return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static void param_set_mask(struct snd_pcm_hw_params *p, int n, unsigned bit)
{
	if (bit >= SNDRV_MASK_MAX)
		return;
	if (param_is_mask(n)) {
		struct snd_mask *m = param_to_mask(p, n);
		m->bits[0] = 0;
		m->bits[1] = 0;
		m->bits[bit >> 5] |= (1 << (bit & 31));
	}
}

static const char *const auxpcm_rate_text[] = {"8000", "16000"};
static const struct soc_enum apq8084_auxpcm_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, auxpcm_rate_text),
};

void *def_codec_mbhc_cal(void);
static int msm_snd_enable_codec_ext_clk(struct snd_soc_codec *codec,
		int enable, bool dapm);

static struct wcd9xxx_mbhc_config mbhc_cfg = {
	.read_fw_bin = false,
	.calibration = NULL,
	.micbias = MBHC_MICBIAS2,
	.mclk_cb_fn = msm_snd_enable_codec_ext_clk,
	.mclk_rate = TAIKO_EXT_CLK_RATE,
	.gpio = 0,
	.gpio_irq = 0,
	.gpio_level_insert = 1,
	.detect_extn_cable = true,
	.micbias_enable_flags = 1 << MBHC_MICBIAS_ENABLE_THRESHOLD_HEADSET,
	.insert_detect = true,
	.swap_gnd_mic = NULL,
	.cs_enable_flags = (1 << MBHC_CS_ENABLE_POLLING |
			    1 << MBHC_CS_ENABLE_INSERTION |
			    1 << MBHC_CS_ENABLE_REMOVAL),
	.do_recalibration = true,
	.use_vddio_meas = true,
};

struct msm_auxpcm_gpio {
	unsigned gpio_no;
	const char *gpio_name;
};

struct msm_auxpcm_ctrl {
	struct msm_auxpcm_gpio *pin_data;
	u32 cnt;
};

static const struct afe_clk_cfg lpass_default = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_OSR_CLK_DISABLE,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_CLK1_VALID,
	0,
};

static const char *const mi2s_pin_states[] = {"Disable",
					      "quad_mi2s_active"};

/*
 * enum mi2s_pin_state - states for the mi2s pinctrl states
 * Note: these states are similar to the "pinctrl-names"
 * in board/target specific DTSI file.
 */
enum mi2s_pin_state {
	MI2S_STATE_DISABLE = 0,
	MI2S_STATE_QUAD_ON = 1
};

/*
 * struct msm_mi2s_pinctrl_info - manage all the pinctrl information
 *
 * @pinctrl:		TSC pinctrl state holder.
 * @disable:		pinctrl state to disable all the pins.
 * @quad_mi2s_active:	pinctrl state to activate Quaternary MI2S.
 * @curr_state:		the current state of the TLMM pins.
 */
struct msm_mi2s_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *disable;
	struct pinctrl_state *quad_mi2s_active;
	enum mi2s_pin_state curr_mi2s_state;
};

struct apq8084_asoc_mach_data {
	u32 mclk_freq;
	int us_euro_gpio;
	struct msm_auxpcm_ctrl *pri_auxpcm_ctrl;
	struct msm_auxpcm_ctrl *sec_auxpcm_ctrl;
	u32 quad_rx_clk_usrs;
	u32 quad_tx_clk_usrs;
	struct msm_mi2s_pinctrl_info mi2s_pinctrl_info;
};

struct apq8084_asoc_wcd93xx_codec {
	int (*mclk_enable_fn) (struct snd_soc_codec *codec,
			       int mclk_enable, bool dapm);
	void *(*get_afe_config_fn) (struct snd_soc_codec *codec,
				   enum afe_config_type config_type);
	int (*mbhc_hs_detect) (struct snd_soc_codec *codec,
			       struct wcd9xxx_mbhc_config *mbhc_cfg);
	void (*mbhc_hs_detect_exit) (struct snd_soc_codec *codec);
};

static struct apq8084_asoc_wcd93xx_codec apq8084_codec_fn;

#define GPIO_NAME_INDEX 0
#define DT_PARSE_INDEX  1

static char *msm_prim_auxpcm_gpio_name[][2] = {
	{"PRIM_AUXPCM_CLK",       "qcom,prim-auxpcm-gpio-clk"},
	{"PRIM_AUXPCM_SYNC",      "qcom,prim-auxpcm-gpio-sync"},
	{"PRIM_AUXPCM_DIN",       "qcom,prim-auxpcm-gpio-din"},
	{"PRIM_AUXPCM_DOUT",      "qcom,prim-auxpcm-gpio-dout"},
};

static char *msm_sec_auxpcm_gpio_name[][2] = {
	{"SEC_AUXPCM_CLK",       "qcom,sec-auxpcm-gpio-clk"},
	{"SEC_AUXPCM_SYNC",      "qcom,sec-auxpcm-gpio-sync"},
	{"SEC_AUXPCM_DIN",       "qcom,sec-auxpcm-gpio-din"},
	{"SEC_AUXPCM_DOUT",      "qcom,sec-auxpcm-gpio-dout"},
};

void *lpaif_pri_muxsel_virt_addr;
void *lpaif_sec_muxsel_virt_addr;
void *lpaif_quad_muxsel_virt_addr;

struct apq8084_liquid_dock_dev {
	int dock_plug_gpio;
	int dock_plug_irq;
	int dock_plug_det;
	struct snd_soc_dapm_context *dapm;
	struct work_struct irq_work;
};

static struct apq8084_liquid_dock_dev *apq8084_liquid_dock_dev;

/* Shared channel numbers for Slimbus ports that connect APQ to MDM. */
enum {
	SLIM_1_RX_1 = 160, /* BT-SCO TX and USB TX */
	SLIM_1_TX_1 = 161, /* BT-SCO RX and USB RX1 */
	SLIM_1_TX_2 = 162, /* USB RX2 */
	SLIM_3_RX_1 = 167, /* External echo-cancellation ref */
	SLIM_3_RX_2 = 168, /* External echo-cancellation ref */
	SLIM_3_TX_1 = 169, /* HDMI RX */
	SLIM_3_TX_2 = 170, /* HDMI RX */
	SLIM_4_RX_1 = 171, /* In-call music delivery2 */
	SLIM_6_TX_1 = 163, /* In-call recording RX */
	SLIM_6_TX_2 = 164, /* In-call recording RX */
	SLIM_6_RX_1 = 165, /* In-call music delivery TX */
};

enum {
	INCALL_REC_MONO,
	INCALL_REC_STEREO,
};

static struct platform_device *spdev;
static struct regulator *ext_spk_amp_regulator;
static int ext_spk_amp_gpio = -1;
static int ext_ult_spk_amp_gpio = -1;

static int apq8084_spk_control = 1;
static int apq8084_ext_spk_pamp;
static int msm_slim_0_rx_ch = 1;
static int msm_slim_0_tx_ch = 1;

static int msm_hdmi_rx_ch = 2;
static int slim0_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int msm_proxy_rx_ch = 2;
static int hdmi_rx_sample_rate = SAMPLING_RATE_48KHZ;

static int msm_slim_1_rate = SAMPLING_RATE_8KHZ;
static int msm_slim_1_rx_ch = 1;
static int msm_slim_1_tx_ch = 1;
static int msm_slim_3_rx_ch = 1;
static int rec_mode = INCALL_REC_MONO;

static struct mutex cdc_mclk_mutex;
static struct clk *codec_clk;
static int ext_mclk_gpio = -1;
static int clk_users;
static atomic_t prim_auxpcm_rsc_ref;
static atomic_t sec_auxpcm_rsc_ref;
static bool codec_reg_done;
static int apq8084_mi2s_rx_ch = 1;
static int apq8084_mi2s_tx_ch = 1;
static atomic_t quad_mi2s_ref_count;

static const char *const mi2s_tx_ch_text[] = {"One", "Two"};
static const char *const mi2s_rx_ch_text[] = {"One", "Two"};

static int apq8084_liquid_ext_spk_power_amp_init(void)
{
	int ret = 0;

	ext_spk_amp_gpio = of_get_named_gpio(spdev->dev.of_node,
		"qcom,ext-spk-amp-gpio", 0);
	if (ext_spk_amp_gpio >= 0) {
		ret = gpio_request(ext_spk_amp_gpio, "ext_spk_amp_gpio");
		if (ret) {
			pr_err("%s: gpio_request failed for ext_spk_amp_gpio.\n",
				__func__);
			return -EINVAL;
		}
		gpio_direction_output(ext_spk_amp_gpio, 0);

		if (ext_spk_amp_regulator == NULL) {
			ext_spk_amp_regulator = regulator_get(&spdev->dev,
									"qcom,ext-spk-amp");

			if (IS_ERR(ext_spk_amp_regulator)) {
				pr_err("%s: Cannot get regulator %s.\n",
					__func__, "qcom,ext-spk-amp");

				gpio_free(ext_spk_amp_gpio);
				return PTR_ERR(ext_spk_amp_regulator);
			}
		}
	}

	ext_ult_spk_amp_gpio = of_get_named_gpio(spdev->dev.of_node,
		"qcom,ext-ult-spk-amp-gpio", 0);

	if (ext_ult_spk_amp_gpio >= 0) {
		ret = gpio_request(ext_ult_spk_amp_gpio,
						   "ext_ult_spk_amp_gpio");
		if (ret) {
			pr_err("%s: gpio_request failed for ext-ult_spk-amp-gpio.\n",
				__func__);
			return -EINVAL;
		}
		gpio_direction_output(ext_ult_spk_amp_gpio, 0);
	}

	return 0;
}

static void apq8084_liquid_ext_ult_spk_power_amp_enable(u32 on)
{
	if (on) {
		if (regulator_enable(ext_spk_amp_regulator))
			pr_err("%s: enable failed ext_spk_amp_regulator\n",
				__func__);
		gpio_direction_output(ext_ult_spk_amp_gpio, 1);
		/* time takes enable the external power class AB amplifier */
		usleep_range(EXT_CLASS_AB_EN_DELAY,
			     EXT_CLASS_AB_EN_DELAY + EXT_CLASS_AB_DELAY_DELTA);
	} else {
		gpio_direction_output(ext_ult_spk_amp_gpio, 0);
		regulator_disable(ext_spk_amp_regulator);
		/* time takes disable the external power class AB amplifier */
		usleep_range(EXT_CLASS_AB_DIS_DELAY,
			     EXT_CLASS_AB_DIS_DELAY + EXT_CLASS_AB_DELAY_DELTA);
	}

	pr_debug("%s: %s external ultrasound SPKR_DRV PAs.\n", __func__,
			on ? "Enable" : "Disable");
}

static void apq8084_liquid_ext_spk_power_amp_enable(u32 on)
{
	if (on) {
		if (regulator_enable(ext_spk_amp_regulator))
			pr_err("%s: enable failed ext_spk_amp_regulator\n",
				__func__);
		gpio_direction_output(ext_spk_amp_gpio, on);
		/*time takes enable the external power amplifier*/
		usleep_range(EXT_CLASS_D_EN_DELAY,
			     EXT_CLASS_D_EN_DELAY + EXT_CLASS_D_DELAY_DELTA);
	} else {
		gpio_direction_output(ext_spk_amp_gpio, on);
		regulator_disable(ext_spk_amp_regulator);
		/*time takes disable the external power amplifier*/
		usleep_range(EXT_CLASS_D_DIS_DELAY,
			     EXT_CLASS_D_DIS_DELAY + EXT_CLASS_D_DELAY_DELTA);
	}

	pr_debug("%s: %s external speaker PAs.\n", __func__,
			on ? "Enable" : "Disable");
}

static void apq8084_liquid_route_aud_dock_dev(void)
{
	struct apq8084_liquid_dock_dev *dock_dev = apq8084_liquid_dock_dev;
	struct snd_soc_dapm_context *dapm = dock_dev->dapm;

	mutex_lock(&dapm->codec->mutex);

	/* Turn off external amp to turn off liquid spkr */
	if ((apq8084_ext_spk_pamp & LO_1_SPK_AMP) &&
		(apq8084_ext_spk_pamp & LO_3_SPK_AMP) &&
		(apq8084_ext_spk_pamp & LO_2_SPK_AMP) &&
		(apq8084_ext_spk_pamp & LO_4_SPK_AMP))
		apq8084_liquid_ext_spk_power_amp_enable(0);

	mutex_unlock(&dapm->codec->mutex);
}

static void apq8084_liquid_docking_irq_work(struct work_struct *work)
{
	struct apq8084_liquid_dock_dev *dock_dev =
		container_of(work, struct apq8084_liquid_dock_dev, irq_work);
	struct snd_soc_dapm_context *dapm = dock_dev->dapm;

	mutex_lock(&dapm->codec->mutex);
	dock_dev->dock_plug_det =
		gpio_get_value(dock_dev->dock_plug_gpio);

	if (0 == dock_dev->dock_plug_det) {
		if ((apq8084_ext_spk_pamp & LO_1_SPK_AMP) &&
			(apq8084_ext_spk_pamp & LO_3_SPK_AMP) &&
			(apq8084_ext_spk_pamp & LO_2_SPK_AMP) &&
			(apq8084_ext_spk_pamp & LO_4_SPK_AMP))
			apq8084_liquid_ext_spk_power_amp_enable(1);
	} else {
		if ((apq8084_ext_spk_pamp & LO_1_SPK_AMP) &&
			(apq8084_ext_spk_pamp & LO_3_SPK_AMP) &&
			(apq8084_ext_spk_pamp & LO_2_SPK_AMP) &&
			(apq8084_ext_spk_pamp & LO_4_SPK_AMP))
			apq8084_liquid_ext_spk_power_amp_enable(0);
	}
	mutex_unlock(&dapm->codec->mutex);
}

static irqreturn_t apq8084_liquid_docking_irq_handler(int irq, void *dev)
{
	struct apq8084_liquid_dock_dev *dock_dev = dev;

	/* switch speakers should not run in interrupt context */
	schedule_work(&dock_dev->irq_work);
	return IRQ_HANDLED;
}

static int apq8084_liquid_dock_notify_handler(struct notifier_block *this,
					unsigned long dock_event,
					void *unused)
{
	int err = 0;

	/* plug in docking speaker+plug in device OR unplug one of them */
	u32 dock_plug_irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
					IRQF_SHARED;

	if (dock_event) {
		err = gpio_request(apq8084_liquid_dock_dev->dock_plug_gpio,
					   "dock-plug-det-irq");
		if (err) {
			pr_err("%s: fail request dock-plug-det-irq err = %d\n",
				__func__, err);
			goto exit;
		}

		apq8084_liquid_dock_dev->dock_plug_det =
			gpio_get_value(apq8084_liquid_dock_dev->dock_plug_gpio);
		if (apq8084_liquid_dock_dev->dock_plug_det)
			apq8084_liquid_route_aud_dock_dev();
		apq8084_liquid_dock_dev->dock_plug_irq =
			gpio_to_irq(apq8084_liquid_dock_dev->dock_plug_gpio);

		err = request_irq(apq8084_liquid_dock_dev->dock_plug_irq,
				  apq8084_liquid_docking_irq_handler,
				  dock_plug_irq_flags,
				  "liquid_dock_plug_irq",
				  apq8084_liquid_dock_dev);
		if (err < 0) {
			pr_err("%s: Request Irq Failed err = %d\n",
				__func__, err);
			goto out;
		}

		INIT_WORK(
			&apq8084_liquid_dock_dev->irq_work,
			apq8084_liquid_docking_irq_work);
	} else {
		if (apq8084_liquid_dock_dev->dock_plug_gpio)
			gpio_free(apq8084_liquid_dock_dev->dock_plug_gpio);

		if (apq8084_liquid_dock_dev->dock_plug_irq)
			free_irq(apq8084_liquid_dock_dev->dock_plug_irq,
				 apq8084_liquid_dock_dev);
	}
	return NOTIFY_OK;

out:
	gpio_free(apq8084_liquid_dock_dev->dock_plug_gpio);
exit:
	return NOTIFY_DONE;
}

static struct notifier_block apq8084_liquid_docking_notifier = {
	.notifier_call  = apq8084_liquid_dock_notify_handler,
};

static int apq8084_liquid_init_docking(struct snd_soc_dapm_context *dapm)
{
	int ret = 0;
	int dock_plug_gpio = 0;

	dock_plug_gpio = of_get_named_gpio(spdev->dev.of_node,
					   "qcom,dock-plug-det-irq", 0);

	if (dock_plug_gpio >= 0) {
		apq8084_liquid_dock_dev =
		 kzalloc(sizeof(*apq8084_liquid_dock_dev), GFP_KERNEL);
		if (!apq8084_liquid_dock_dev) {
			pr_err("apq8084_liquid_dock_dev alloc fail.\n");
			ret = -ENOMEM;
			goto exit;
		}

		apq8084_liquid_dock_dev->dock_plug_gpio = dock_plug_gpio;
		apq8084_liquid_dock_dev->dapm = dapm;

		register_liquid_dock_notify(&apq8084_liquid_docking_notifier);
	}
exit:
	return ret;
}

static int apq8084_liquid_ext_spk_power_amp_on(u32 spk)
{
	int rc = 0;

	if (spk & (LO_1_SPK_AMP | LO_3_SPK_AMP | LO_2_SPK_AMP | LO_4_SPK_AMP)) {
		pr_debug("%s: External speakers are already on. spk = 0x%x\n",
			__func__, spk);

		apq8084_ext_spk_pamp |= spk;
		if ((apq8084_ext_spk_pamp & LO_1_SPK_AMP) &&
		    (apq8084_ext_spk_pamp & LO_3_SPK_AMP) &&
		    (apq8084_ext_spk_pamp & LO_2_SPK_AMP) &&
		    (apq8084_ext_spk_pamp & LO_4_SPK_AMP))
			if (ext_spk_amp_gpio >= 0 &&
			    apq8084_liquid_dock_dev &&
			    apq8084_liquid_dock_dev->dock_plug_det == 0)
				apq8084_liquid_ext_spk_power_amp_enable(1);
	} else  {
		pr_err("%s: Invalid external speaker ampl. spk = 0x%x\n",
			__func__, spk);
		rc = -EINVAL;
	}
	return rc;
}

static void apq8084_ext_spk_power_amp_on(u32 spk)
{
	if (gpio_is_valid(ext_spk_amp_gpio))
		apq8084_liquid_ext_spk_power_amp_on(spk);
}

static void apq8084_liquid_ext_spk_power_amp_off(u32 spk)
{
	if (spk & (LO_1_SPK_AMP |
		   LO_3_SPK_AMP |
		   LO_2_SPK_AMP |
		   LO_4_SPK_AMP)) {

		pr_debug("%s Left and right speakers case spk = 0x%08x",
			__func__, spk);
		apq8084_ext_spk_pamp &= ~spk;
		if (!apq8084_ext_spk_pamp) {
			if (ext_spk_amp_gpio >= 0 &&
				apq8084_liquid_dock_dev != NULL &&
				apq8084_liquid_dock_dev->dock_plug_det == 0)
				apq8084_liquid_ext_spk_power_amp_enable(0);
		}
	} else  {
		pr_err("%s: ERROR : Invalid Ext Spk Ampl. spk = 0x%08x\n",
			__func__, spk);
	}
}

static void apq8084_ext_spk_power_amp_off(u32 spk)
{
	if (gpio_is_valid(ext_spk_amp_gpio))
		apq8084_liquid_ext_spk_power_amp_off(spk);
}

static void apq8084_ext_control(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	mutex_lock(&dapm->codec->mutex);
	pr_debug("%s: apq8084_spk_control = %d", __func__, apq8084_spk_control);
	if (apq8084_spk_control == APQ8084_SPK_ON) {
		snd_soc_dapm_enable_pin(dapm, "Lineout_1 amp");
		snd_soc_dapm_enable_pin(dapm, "Lineout_3 amp");
		snd_soc_dapm_enable_pin(dapm, "Lineout_2 amp");
		snd_soc_dapm_enable_pin(dapm, "Lineout_4 amp");
	} else {
		snd_soc_dapm_disable_pin(dapm, "Lineout_1 amp");
		snd_soc_dapm_disable_pin(dapm, "Lineout_3 amp");
		snd_soc_dapm_disable_pin(dapm, "Lineout_2 amp");
		snd_soc_dapm_disable_pin(dapm, "Lineout_4 amp");
	}

	mutex_unlock(&dapm->codec->mutex);
	snd_soc_dapm_sync(dapm);
}

static int apq8084_get_spk(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: apq8084_spk_control = %d", __func__, apq8084_spk_control);
	ucontrol->value.integer.value[0] = apq8084_spk_control;
	return 0;
}

static int apq8084_set_spk(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	pr_debug("%s()\n", __func__);
	if (apq8084_spk_control == ucontrol->value.integer.value[0])
		return 0;

	apq8084_spk_control = ucontrol->value.integer.value[0];
	apq8084_ext_control(codec);
	return 1;
}

static int msm_ext_spkramp_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *k, int event)
{
	int ret = 0;

	pr_debug("%s()\n", __func__);
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (!strcmp(w->name, "Lineout_1 amp"))
			apq8084_ext_spk_power_amp_on(LO_1_SPK_AMP);
		else if (!strcmp(w->name, "Lineout_3 amp"))
			apq8084_ext_spk_power_amp_on(LO_3_SPK_AMP);
		else if (!strcmp(w->name, "Lineout_2 amp"))
			apq8084_ext_spk_power_amp_on(LO_2_SPK_AMP);
		else if  (!strcmp(w->name, "Lineout_4 amp"))
			apq8084_ext_spk_power_amp_on(LO_4_SPK_AMP);
		else {
			pr_err("%s() Invalid Speaker Widget = %s\n",
					__func__, w->name);
			ret = -EINVAL;
		}
	} else {
		if (!strcmp(w->name, "Lineout_1 amp"))
			apq8084_ext_spk_power_amp_off(LO_1_SPK_AMP);
		else if (!strcmp(w->name, "Lineout_3 amp"))
			apq8084_ext_spk_power_amp_off(LO_3_SPK_AMP);
		else if (!strcmp(w->name, "Lineout_2 amp"))
			apq8084_ext_spk_power_amp_off(LO_2_SPK_AMP);
		else if  (!strcmp(w->name, "Lineout_4 amp"))
			apq8084_ext_spk_power_amp_off(LO_4_SPK_AMP);
		else {
			pr_err("%s() Invalid Speaker Widget = %s\n",
					__func__, w->name);
			ret = -EINVAL;
		}
	}
	return ret;
}

static int msm_ext_spkramp_ultrasound_event(struct snd_soc_dapm_widget *w,
					    struct snd_kcontrol *k, int event)
{
	pr_debug("%s()\n", __func__);
	if (!strcmp(w->name, "SPK_ultrasound amp")) {
		if (!gpio_is_valid(ext_ult_spk_amp_gpio)) {
			pr_err("%s: ext_ult_spk_amp_gpio isn't configured\n",
				__func__);
			return -EINVAL;
		}

		if (SND_SOC_DAPM_EVENT_ON(event))
			apq8084_liquid_ext_ult_spk_power_amp_enable(1);
		else
			apq8084_liquid_ext_ult_spk_power_amp_enable(0);
	} else {
			pr_err("%s() Invalid Speaker Widget = %s\n",
					__func__, w->name);
			return -EINVAL;
	}
	return 0;
}

static int apq8084_ext_mclk_gpio_init(void)
{
	int ret = 0;
	ext_mclk_gpio = of_get_named_gpio(spdev->dev.of_node,
					  "qcom,ext-mclk-gpio", 0);

	pr_debug("%s: ext_mclk_gpio %d", __func__, ext_mclk_gpio);
	if (ext_mclk_gpio >= 0) {
		ret = gpio_request(ext_mclk_gpio, "ext_mclk_gpio");
		if (ret) {
			pr_err("%s: gpio_request failed for ext_mclk_gpio.\n",
				__func__);

			ext_mclk_gpio = -1;
			return -EINVAL;
		}
		gpio_direction_output(ext_mclk_gpio, 0);
	}
	return 0;
}

static int msm_snd_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm)
{
	int ret = 0;

	pr_debug("%s: enable = %d clk_users = %d\n",
			__func__, enable, clk_users);

	if (!apq8084_codec_fn.mclk_enable_fn) {
		dev_err(codec->dev, "%s: codec mclk enable fn is not init'ed\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&cdc_mclk_mutex);
	if (enable) {
		if (ext_mclk_gpio >= 0) {
			clk_users++;
			if (clk_users != 1)
				goto exit;

			gpio_direction_output(ext_mclk_gpio, 1);
			apq8084_codec_fn.mclk_enable_fn(codec, 1, dapm);
		} else if (codec_clk) {
			clk_users++;
			if (clk_users != 1)
				goto exit;

			clk_prepare_enable(codec_clk);
			apq8084_codec_fn.mclk_enable_fn(codec, 1, dapm);
		} else {
			dev_err(codec->dev, "%s: did not get Codec MCLK\n",
				__func__);
			ret = -EINVAL;
			goto exit;
		}
	} else {
		if (clk_users > 0) {
			clk_users--;
			if (clk_users == 0) {
				apq8084_codec_fn.mclk_enable_fn(codec, 0, dapm);
				if (ext_mclk_gpio >= 0)
					gpio_direction_output(ext_mclk_gpio, 0);
				else if (codec_clk)
					clk_disable_unprepare(codec_clk);
			}
		} else {
			pr_err("%s: Error releasing Codec MCLK\n", __func__);
			ret = -EINVAL;
			goto exit;
		}
	}
exit:
	mutex_unlock(&cdc_mclk_mutex);
	return ret;
}

static int apq8084_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return msm_snd_enable_codec_ext_clk(w->codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return msm_snd_enable_codec_ext_clk(w->codec, 0, true);
	}
	return 0;
}

static const struct snd_soc_dapm_widget apq8084_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	apq8084_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("Lineout_1 amp", msm_ext_spkramp_event),
	SND_SOC_DAPM_SPK("Lineout_3 amp", msm_ext_spkramp_event),

	SND_SOC_DAPM_SPK("Lineout_2 amp", msm_ext_spkramp_event),
	SND_SOC_DAPM_SPK("Lineout_4 amp", msm_ext_spkramp_event),
	SND_SOC_DAPM_SPK("SPK_ultrasound amp",
					 msm_ext_spkramp_ultrasound_event),

	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCRight Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCLeft Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Analog Mic4", NULL),
	SND_SOC_DAPM_MIC("Analog Mic6", NULL),
	SND_SOC_DAPM_MIC("Analog Mic7", NULL),

	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_MIC("Digital Mic3", NULL),
	SND_SOC_DAPM_MIC("Digital Mic4", NULL),
	SND_SOC_DAPM_MIC("Digital Mic5", NULL),
	SND_SOC_DAPM_MIC("Digital Mic6", NULL),
};

static const char *const spk_function[] = {"Off", "On"};
static const char *const slim0_rx_ch_text[] = {"One", "Two"};
static const char *const slim0_tx_ch_text[] = {"One", "Two", "Three", "Four",
						"Five", "Six", "Seven",
						"Eight"};
static char const *hdmi_rx_ch_text[] = {"Two", "Three", "Four", "Five",
					"Six", "Seven", "Eight"};
static char const *rx_bit_format_text[] = {"S16_LE", "S24_LE"};
static char const *slim0_rx_sample_rate_text[] = {"KHZ_48", "KHZ_96",
						  "KHZ_192"};
static const char *const proxy_rx_ch_text[] = {"One", "Two", "Three", "Four",
					      "Five", "Six", "Seven", "Eight"};

static char const *hdmi_rx_sample_rate_text[] = {"KHZ_48", "KHZ_96",
						 "KHZ_192"};

static const char * const slim1_tx_ch_text[] = {"One", "Two"};
static const char * const slim3_rx_ch_text[] = {"One", "Two"};
static const char *const slim1_rate_text[] = {"8000", "16000", "48000"};

static int slim0_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (slim0_rx_sample_rate) {
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 2;
		break;

	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 1;
		break;

	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: slim0_rx_sample_rate = %d\n", __func__,
				slim0_rx_sample_rate);
	return 0;
}

static int slim0_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ucontrol value = %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	switch (ucontrol->value.integer.value[0]) {
	case 2:
		slim0_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		slim0_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		slim0_rx_sample_rate = SAMPLING_RATE_48KHZ;
	}

	pr_debug("%s: slim0_rx_sample_rate = %d\n", __func__,
			slim0_rx_sample_rate);
	return 0;
}

static int slim0_rx_bit_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (slim0_rx_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: slim0_rx_bit_format = %d, ucontrol value = %ld\n",
			 __func__, slim0_rx_bit_format,
			ucontrol->value.integer.value[0]);
	return 0;
}

static int slim0_rx_bit_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		slim0_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		slim0_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return 0;
}

static int msm_slim_0_rx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_0_rx_ch  = %d\n", __func__,
		 msm_slim_0_rx_ch);
	ucontrol->value.integer.value[0] = msm_slim_0_rx_ch - 1;
	return 0;
}

static int msm_slim_0_rx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_0_rx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_slim_0_rx_ch = %d\n", __func__,
		 msm_slim_0_rx_ch);
	return 1;
}

static int msm_slim_0_tx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_0_tx_ch  = %d\n", __func__,
		 msm_slim_0_tx_ch);
	ucontrol->value.integer.value[0] = msm_slim_0_tx_ch - 1;
	return 0;
}

static int msm_slim_0_tx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_0_tx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_slim_0_tx_ch = %d\n", __func__, msm_slim_0_tx_ch);
	return 1;
}

static int msm_slim_1_tx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_1_tx_ch  = %d\n", __func__,
		 msm_slim_1_tx_ch);

	ucontrol->value.integer.value[0] = msm_slim_1_tx_ch - 1;
	return 0;
}

static int msm_slim_1_tx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_1_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_slim_1_tx_ch = %d\n", __func__,
		 msm_slim_1_tx_ch);
	return 1;
}

static int msm_slim_1_rate_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_1_rate  = %d", __func__,
		 msm_slim_1_rate);

	ucontrol->value.integer.value[0] = msm_slim_1_rate;
	return 0;
}

static int msm_slim_1_rate_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 2:
		msm_slim_1_rate = SAMPLING_RATE_48KHZ;
		break;
	case 1:
		msm_slim_1_rate = SAMPLING_RATE_16KHZ;
		break;
	case 0:
	default:
		msm_slim_1_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	pr_debug("%s: msm_slim_1_rate = %d\n", __func__,
		 msm_slim_1_rate);
	return 0;
}

static int msm_slim_3_rx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_3_rx_ch  = %d\n", __func__,
		 msm_slim_3_rx_ch);

	ucontrol->value.integer.value[0] = msm_slim_3_rx_ch - 1;
	return 0;
}

static int msm_slim_3_rx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_3_rx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_slim_3_rx_ch = %d\n", __func__,
		 msm_slim_3_rx_ch);
	return 1;
}

static int hdmi_rx_bit_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (hdmi_rx_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: hdmi_rx_bit_format = %d, ucontrol value = %ld\n",
			 __func__, hdmi_rx_bit_format,
			ucontrol->value.integer.value[0]);
	return 0;
}

static int hdmi_rx_bit_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		hdmi_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		hdmi_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: hdmi_rx_bit_format = %d, ucontrol value = %ld\n",
			 __func__, hdmi_rx_bit_format,
			ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_hdmi_rx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_hdmi_rx_ch  = %d\n", __func__,
			msm_hdmi_rx_ch);
	ucontrol->value.integer.value[0] = msm_hdmi_rx_ch - 2;
	return 0;
}

static int msm_hdmi_rx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	msm_hdmi_rx_ch = ucontrol->value.integer.value[0] + 2;
	if (msm_hdmi_rx_ch > 8) {
		pr_err("%s: channels exceeded 8.Limiting to max channels-8\n",
			__func__);
		msm_hdmi_rx_ch = 8;
	}
	pr_debug("%s: msm_hdmi_rx_ch = %d\n", __func__, msm_hdmi_rx_ch);
	return 1;
}

static int hdmi_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (hdmi_rx_sample_rate) {
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 2;
		break;

	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 1;
		break;

	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 0;
		break;
	}
	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: hdmi_rx_sample_rate = %d\n", __func__,
				hdmi_rx_sample_rate);
	return 0;
}

static int hdmi_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ucontrol value = %ld\n", __func__,
			ucontrol->value.integer.value[0]);
	switch (ucontrol->value.integer.value[0]) {
	case 2:
		hdmi_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		hdmi_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		hdmi_rx_sample_rate = SAMPLING_RATE_48KHZ;
	}
	pr_debug("%s: hdmi_rx_sample_rate = %d\n", __func__,
			hdmi_rx_sample_rate);
	return 0;
}

static int apq8084_auxpcm_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = apq8084_auxpcm_rate;
	return 0;
}

static int apq8084_auxpcm_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		apq8084_auxpcm_rate = 8000;
		break;
	case 1:
		apq8084_auxpcm_rate = 16000;
		break;
	default:
		apq8084_auxpcm_rate = 8000;
		break;
	}
	return 0;
}

static int msm_proxy_rx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_proxy_rx_ch = %d\n", __func__,
						msm_proxy_rx_ch);
	ucontrol->value.integer.value[0] = msm_proxy_rx_ch - 1;
	return 0;
}

static int msm_proxy_rx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_proxy_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_proxy_rx_ch = %d\n", __func__,
						msm_proxy_rx_ch);
	return 1;
}

static int msm_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = apq8084_auxpcm_rate;
	channels->min = channels->max = 1;
	return 0;
}

static int msm_proxy_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					   struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: msm_proxy_rx_ch =%d\n", __func__, msm_proxy_rx_ch);
	if (channels->max < 2)
		channels->min = channels->max = 2;
	channels->min = channels->max = msm_proxy_rx_ch;
	rate->min = rate->max = 48000;
	return 0;
}

static int msm_proxy_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					   struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	rate->min = rate->max = 48000;
	return 0;
}

static int apq8084_hdmi_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					   struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s channels->min %u channels->max %u ()\n", __func__,
			channels->min, channels->max);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				hdmi_rx_bit_format);
	if (channels->max < 2)
		channels->min = channels->max = 2;
	rate->min = rate->max = hdmi_rx_sample_rate;
	channels->min = channels->max = msm_hdmi_rx_ch;
	return 0;
}

static int msm_aux_pcm_get_gpios(struct msm_auxpcm_ctrl *auxpcm_ctrl)
{
	struct msm_auxpcm_gpio *pin_data = NULL;
	int ret = 0;
	int i;
	int j;

	pin_data = auxpcm_ctrl->pin_data;
	for (i = 0; i < auxpcm_ctrl->cnt; i++, pin_data++) {
		ret = gpio_request(pin_data->gpio_no,
				pin_data->gpio_name);
		pr_debug("%s: gpio = %d, gpio name = %s\n"
			"ret = %d\n", __func__,
			pin_data->gpio_no,
			pin_data->gpio_name,
			ret);
		if (ret) {
			pr_err("%s: Failed to request gpio %d\n",
				__func__, pin_data->gpio_no);
			/* Release all GPIOs on failure */
			for (j = i; j >= 0; j--)
				gpio_free(pin_data->gpio_no);
			return ret;
		}
	}
	return 0;
}

static int msm_aux_pcm_free_gpios(struct msm_auxpcm_ctrl *auxpcm_ctrl)
{
	struct msm_auxpcm_gpio *pin_data = NULL;
	int i;
	int ret = 0;

	if (auxpcm_ctrl == NULL || auxpcm_ctrl->pin_data == NULL) {
		pr_err("%s: Ctrl pointers are NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	pin_data = auxpcm_ctrl->pin_data;
	for (i = 0; i < auxpcm_ctrl->cnt; i++, pin_data++) {
		gpio_free(pin_data->gpio_no);
		pr_debug("%s: gpio = %d, gpio_name = %s\n",
			__func__, pin_data->gpio_no,
			pin_data->gpio_name);
	}
err:
	return ret;
}

static int msm_prim_auxpcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_auxpcm_ctrl *auxpcm_ctrl = NULL;
	int ret = 0;

	pr_debug("%s(): substream = %s, prim_auxpcm_rsc_ref counter = %d\n",
		__func__, substream->name, atomic_read(&prim_auxpcm_rsc_ref));
	auxpcm_ctrl = pdata->pri_auxpcm_ctrl;
	if (auxpcm_ctrl == NULL || auxpcm_ctrl->pin_data == NULL) {
		pr_err("%s: Ctrl pointers are NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	if (atomic_inc_return(&prim_auxpcm_rsc_ref) == 1) {
		if (lpaif_pri_muxsel_virt_addr != NULL)
			iowrite32(I2S_PCM_SEL << I2S_PCM_SEL_OFFSET,
				lpaif_pri_muxsel_virt_addr);
		else
			pr_err("%s lpaif_pri_muxsel_virt_addr is NULL\n",
				 __func__);
		ret = msm_aux_pcm_get_gpios(auxpcm_ctrl);
	}
	if (ret < 0) {
		pr_err("%s: Aux PCM GPIO request failed\n", __func__);
		return -EINVAL;
	}
err:
	return ret;
}

static void msm_prim_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_auxpcm_ctrl *auxpcm_ctrl = NULL;

	pr_debug("%s(): substream = %s, prim_auxpcm_rsc_ref counter = %d\n",
		__func__, substream->name, atomic_read(&prim_auxpcm_rsc_ref));
	auxpcm_ctrl = pdata->pri_auxpcm_ctrl;
	if (atomic_dec_return(&prim_auxpcm_rsc_ref) == 0)
		msm_aux_pcm_free_gpios(auxpcm_ctrl);
}

static struct snd_soc_ops msm_pri_auxpcm_be_ops = {
	.startup = msm_prim_auxpcm_startup,
	.shutdown = msm_prim_auxpcm_shutdown,
};

static int msm_sec_auxpcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_auxpcm_ctrl *auxpcm_ctrl = NULL;
	int ret = 0;

	pr_debug("%s(): substream = %s, sec_auxpcm_rsc_ref counter = %d\n",
		__func__, substream->name, atomic_read(&sec_auxpcm_rsc_ref));
	auxpcm_ctrl = pdata->sec_auxpcm_ctrl;
	if (auxpcm_ctrl == NULL || auxpcm_ctrl->pin_data == NULL) {
		pr_err("%s: Ctrl pointers are NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	if (atomic_inc_return(&sec_auxpcm_rsc_ref) == 1) {
		if (lpaif_sec_muxsel_virt_addr != NULL)
			iowrite32(I2S_PCM_SEL << I2S_PCM_SEL_OFFSET,
				lpaif_sec_muxsel_virt_addr);
		else
			pr_err("%s lpaif_sec_muxsel_virt_addr is NULL\n",
				 __func__);
		ret = msm_aux_pcm_get_gpios(auxpcm_ctrl);
	}
	if (ret < 0) {
		pr_err("%s: Aux PCM GPIO request failed\n", __func__);
		return -EINVAL;
	}
err:
	return ret;
}

static void msm_sec_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_auxpcm_ctrl *auxpcm_ctrl = NULL;

	pr_debug("%s(): substream = %s, sec_auxpcm_rsc_ref counter = %d\n",
		__func__, substream->name, atomic_read(&sec_auxpcm_rsc_ref));
	auxpcm_ctrl = pdata->sec_auxpcm_ctrl;
	if (atomic_dec_return(&sec_auxpcm_rsc_ref) == 0)
		msm_aux_pcm_free_gpios(auxpcm_ctrl);
}

static struct snd_soc_ops msm_sec_auxpcm_be_ops = {
	.startup = msm_sec_auxpcm_startup,
	.shutdown = msm_sec_auxpcm_shutdown,
};

static int msm_quad_mi2s_set_pinctrl(struct apq8084_asoc_mach_data *pdata)
{
	struct msm_mi2s_pinctrl_info *pinctrl_info = &pdata->mi2s_pinctrl_info;
	int ret = 0;

	pr_debug("%s: curr_mi2s_state = %s\n", __func__,
		 mi2s_pin_states[pinctrl_info->curr_mi2s_state]);
	/* Enable Quaternary MI2S TLMM pins and set to appropriate state */
	switch (pinctrl_info->curr_mi2s_state) {
	case MI2S_STATE_DISABLE:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
					   pinctrl_info->quad_mi2s_active);
		if (ret) {
			pr_err("%s: pinctrl_select_state failed with %d\n",
				__func__, ret);
			ret = -EIO;
			goto err;
		}
		pinctrl_info->curr_mi2s_state = MI2S_STATE_QUAD_ON;
		break;
	case MI2S_STATE_QUAD_ON:
		pr_err("%s: MI2S TLMM pins already set\n", __func__);
		break;
	default:
		pr_err("%s: MI2S TLMM pin state is invalid\n", __func__);
		return -EINVAL;
	}

err:
	return ret;
}

static int msm_quad_mi2s_reset_pinctrl(struct apq8084_asoc_mach_data *pdata)
{
	struct msm_mi2s_pinctrl_info *pinctrl_info = &pdata->mi2s_pinctrl_info;
	int ret = 0;

	pr_debug("%s: curr_mi2s_state = %s\n", __func__,
		 mi2s_pin_states[pinctrl_info->curr_mi2s_state]);
	/* Reset Quaternary MI2S TLMM pins and set to appropriate state */
	switch (pinctrl_info->curr_mi2s_state) {
	case MI2S_STATE_QUAD_ON:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
					   pinctrl_info->disable);
		if (ret) {
			pr_err("%s: pinctrl_select_state failed with %d\n",
				__func__, ret);
			ret = -EIO;
			goto err;
		}
		pinctrl_info->curr_mi2s_state = MI2S_STATE_DISABLE;
		break;
	case MI2S_STATE_DISABLE:
		pr_err("%s: MI2S TLMM pins already disabled\n", __func__);
		break;
	default:
		pr_err("%s: MI2S TLMM pin state is invalid\n", __func__);
		return -EINVAL;
	}

err:
	return ret;
}

static int apq8084_quad_mi2s_clk_ctl(struct snd_soc_pcm_runtime *rtd,
				bool enable,
				struct snd_pcm_substream *substream)
{
	struct snd_soc_card *card = rtd->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct afe_clk_cfg *lpass_clk = NULL;
	int ret = 0;
	u32 afe_port_id;

	if (pdata == NULL) {
		pr_err("%s:platform data is null\n", __func__);
		return -EINVAL;
	}
	lpass_clk = kzalloc(sizeof(struct afe_clk_cfg), GFP_KERNEL);
	if (lpass_clk == NULL) {
		pr_err("%s:Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}
	memcpy(lpass_clk, &lpass_default, sizeof(struct afe_clk_cfg));
	pr_debug("%s: lpass clock enable = %d\n", __func__, enable);
	if (enable) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			afe_port_id = AFE_PORT_ID_QUATERNARY_MI2S_RX;
			if (pdata->quad_rx_clk_usrs == 0) {
				lpass_clk->clk_val1 =
					    Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
				lpass_clk->clk_set_mode =
						Q6AFE_LPASS_MODE_CLK1_VALID;
			}
			ret = afe_set_lpass_clock(afe_port_id, lpass_clk);
			if (ret < 0) {
				pr_err("%s:afe_set_lpass_clock failed with %d\n",
					__func__, ret);
				goto err;
			} else {
				pdata->quad_rx_clk_usrs++;
			}
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			afe_port_id = AFE_PORT_ID_QUATERNARY_MI2S_TX;
			if (pdata->quad_tx_clk_usrs == 0) {
				lpass_clk->clk_val1 =
					    Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
				lpass_clk->clk_set_mode =
						Q6AFE_LPASS_MODE_CLK1_VALID;
			}
			ret = afe_set_lpass_clock(afe_port_id, lpass_clk);
			if (ret < 0) {
				pr_err("%s:afe_set_lpass_clock failed with %d\n",
					__func__, ret);
				goto err;
			} else {
				pdata->quad_tx_clk_usrs++;
			}
		}
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			afe_port_id = AFE_PORT_ID_QUATERNARY_MI2S_RX;
			if (pdata->quad_rx_clk_usrs > 0)
				pdata->quad_rx_clk_usrs--;
			if (pdata->quad_rx_clk_usrs == 0) {
				lpass_clk->clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_DISABLE;
				lpass_clk->clk_set_mode =
						Q6AFE_LPASS_MODE_CLK1_VALID;
			}
			ret = afe_set_lpass_clock(afe_port_id, lpass_clk);
			if (ret < 0) {
				pr_err("%s:afe_set_lpass_clock failed with %d\n",
					__func__, ret);
				goto err;
			}
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			afe_port_id = AFE_PORT_ID_QUATERNARY_MI2S_TX;
			if (pdata->quad_tx_clk_usrs > 0)
				pdata->quad_tx_clk_usrs--;
			if (pdata->quad_tx_clk_usrs == 0) {
				lpass_clk->clk_val1 =
						Q6AFE_LPASS_IBIT_CLK_DISABLE;
				lpass_clk->clk_set_mode =
						Q6AFE_LPASS_MODE_CLK1_VALID;
			}
			ret = afe_set_lpass_clock(afe_port_id, lpass_clk);
			if (ret < 0) {
				pr_err("%s:afe_set_lpass_clock failed with %d\n",
					__func__, ret);
				goto err;
			}
		}
	}
	pr_debug("%s: clk 1 = 0x%x clk2 = 0x%x mode = 0x%x\n",
			 __func__, lpass_clk->clk_val1,
			lpass_clk->clk_val2,
			lpass_clk->clk_set_mode);
	ret = 0;
err:
	kfree(lpass_clk);
	return ret;
}

static int msm_quad_mi2s_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret = 0;
	uint32_t pcm_sel_reg;

	pr_debug("%s(): substream = %s, quad_mi2s_ref_count = %d\n",
		 __func__, substream->name, atomic_read(&quad_mi2s_ref_count));

	if (pdata == NULL || lpaif_quad_muxsel_virt_addr == NULL) {
		pr_err("%s: Invalid parameters for Quad MI2S\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	if (atomic_inc_return(&quad_mi2s_ref_count) == 1) {
		pcm_sel_reg = ioread32(lpaif_quad_muxsel_virt_addr);
		if (pcm_sel_reg & (I2S_PCM_SEL << I2S_PCM_SEL_OFFSET)) {
			iowrite32(pcm_sel_reg &
				~(I2S_PCM_SEL << I2S_PCM_SEL_OFFSET),
				  lpaif_quad_muxsel_virt_addr);
		}

		ret = msm_quad_mi2s_set_pinctrl(pdata);
		if (ret) {
			pr_err("%s: MI2S TLMM pinctrl set failed with %d\n",
				__func__, ret);
			return ret;
		}

		ret = apq8084_quad_mi2s_clk_ctl(rtd, true, substream);
		if (ret) {
			pr_err("%s: Setting clk control failed with %d\n",
				__func__, ret);
			return ret;
		}
		/* This sets the CONFIG PARAMETER WS_SRC.
		 * SND_SOC_DAIFMT_CBS_CFS means internal clock/master mode.
		 * SND_SOC_DAIFMT_CBM_CFM means external clock/slave mode.
		 */
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret)
			pr_err("%s: set fmt cpu dai failed with %d\n",
				__func__, ret);
	}
err:
	return ret;
}

static void msm_quad_mi2s_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret;

	pr_debug("%s(): substream = %s, quad_mi2s_ref_count = %d\n",
		 __func__, substream->name, atomic_read(&quad_mi2s_ref_count));

	if (pdata == NULL) {
		pr_err("%s: Invalid platform data\n", __func__);
		return;
	}

	if (atomic_dec_return(&quad_mi2s_ref_count) == 0) {
		ret = msm_quad_mi2s_reset_pinctrl(pdata);
		if (ret)
			pr_err("%s Reset pinctrl failed with %d\n",
				__func__, ret);
		ret = apq8084_quad_mi2s_clk_ctl(rtd, false, substream);
		if (ret)
			pr_err("%s Clock disable failed with %d\n",
				__func__, ret);
	}
}

static struct snd_soc_ops apq8084_quad_mi2s_be_ops = {
	.startup = msm_quad_mi2s_startup,
	.shutdown = msm_quad_mi2s_shutdown,
};

static int apq8084_mi2s_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				mi2s_tx_bit_format);
	rate->min = rate->max = SAMPLING_RATE_48KHZ;
	channels->min = channels->max = apq8084_mi2s_tx_ch;
	pr_debug("%s: format = %d rate = %d, channels = %d\n",
			__func__, params_format(params), params_rate(params),
			apq8084_mi2s_tx_ch);
	return 0;
}

static int apq8084_mi2s_tx_ch_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: apq8084_i2s_tx_ch  = %d\n", __func__,
		 apq8084_mi2s_tx_ch);
	ucontrol->value.integer.value[0] = apq8084_mi2s_tx_ch - 1;
	return 0;
}

static int apq8084_mi2s_tx_ch_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	apq8084_mi2s_tx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: apq8084_i2s_tx_ch = %d\n", __func__,
		 apq8084_mi2s_tx_ch);
	return 1;
}
static int apq8084_mi2s_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				mi2s_rx_bit_format);
	rate->min = rate->max = SAMPLING_RATE_48KHZ;
	channels->min = channels->max = apq8084_mi2s_rx_ch;
	pr_debug("%s: format = %d rate = %d, channels = %d\n",
			__func__, params_format(params), params_rate(params),
			apq8084_mi2s_rx_ch);
	return 0;
}

static int apq8084_mi2s_rx_ch_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: apq8084_i2s_rx_ch  = %d\n", __func__,
		 apq8084_mi2s_rx_ch);
	ucontrol->value.integer.value[0] = apq8084_mi2s_rx_ch - 1;
	return 0;
}

static int apq8084_mi2s_rx_ch_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	apq8084_mi2s_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: apq8084_i2s_rx_ch = %d\n", __func__,
		 apq8084_mi2s_tx_ch);
	return 1;
}

static int msm_slim_0_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				   slim0_rx_bit_format);
	rate->min = rate->max = slim0_rx_sample_rate;
	channels->min = channels->max = msm_slim_0_rx_ch;
	pr_debug("%s: format = %d, rate = %d, channels = %d\n",
		__func__, params_format(params),
		params_rate(params), msm_slim_0_rx_ch);
	return 0;
}

static int msm_slim_0_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm_slim_0_tx_ch;
	return 0;
}

static int msm_slim_4_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;
	return 0;
}

static int msm_slim_5_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	int rc;
	void *config;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_interval *rate =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s enter\n", __func__);
	if (!apq8084_codec_fn.get_afe_config_fn) {
		pr_err("%s: codec get afe config not init'ed\n", __func__);
		return -EINVAL;
	}
	rate->min = rate->max = 16000;
	channels->min = channels->max = 1;
	config = apq8084_codec_fn.get_afe_config_fn(codec,
					AFE_SLIMBUS_SLAVE_PORT_CONFIG);
	rc = afe_set_config(AFE_SLIMBUS_SLAVE_PORT_CONFIG, config,
			    SLIMBUS_5_TX);
	if (rc) {
		pr_err("%s: Failed to set slimbus slave port config %d\n",
		       __func__, rc);
		return rc;
	}
	return 0;
}

static int msm_slim_1_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msm_slim_1_rate;
	channels->min = channels->max = msm_slim_1_rx_ch;
	return 0;
}

static int msm_slim_1_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msm_slim_1_rate;
	channels->min = channels->max = msm_slim_1_tx_ch;
	return 0;
}

static int msm_slim_3_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm_slim_3_rx_ch;
	return 0;
}

static int msm_slim_3_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;
	return 0;
}

static int msm_slim_4_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = 48000;
	channels->min = channels->max = 1;

	pr_debug("%s() channels->min %u channels->max %u\n", __func__,
		 channels->min, channels->max);
	return 0;
}

static int msm_slim_6_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);

	rate->min = rate->max = 48000;
	if (rec_mode == INCALL_REC_STEREO)
		channels->min = channels->max = 2;
	else
		channels->min = channels->max = 1;

	pr_debug("%s channels->min %u channels->max %u ()\n", __func__,
		 channels->min, channels->max);
	return 0;
}

static int msm_slim_6_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = 48000;
	channels->min = channels->max = 1;

	pr_debug("%s channels->min %u channels->max %u ()\n", __func__,
		 channels->min, channels->max);
	return 0;
}

static int msm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				  struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	return 0;
}

static int msm_incall_rec_mode_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = rec_mode;
	return 0;
}

static int msm_incall_rec_mode_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	rec_mode = ucontrol->value.integer.value[0];
	pr_debug("%s: rec_mode:%d\n", __func__, rec_mode);
	return 0;
}


static const struct soc_enum msm_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, slim0_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(8, slim0_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(7, hdmi_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(3, slim0_rx_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(8, proxy_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(3, hdmi_rx_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(2, slim1_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(3, slim1_rate_text),
	SOC_ENUM_SINGLE_EXT(3, slim3_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, mi2s_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, mi2s_tx_ch_text),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("Speaker Function", msm_snd_enum[0], apq8084_get_spk,
			apq8084_set_spk),
	SOC_ENUM_EXT("SLIM_0_RX Channels", msm_snd_enum[1],
			msm_slim_0_rx_ch_get, msm_slim_0_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_TX Channels", msm_snd_enum[2],
			msm_slim_0_tx_ch_get, msm_slim_0_tx_ch_put),
	SOC_ENUM_EXT("AUX PCM SampleRate", apq8084_auxpcm_enum[0],
			apq8084_auxpcm_rate_get, apq8084_auxpcm_rate_put),
	SOC_ENUM_EXT("HDMI_RX Channels", msm_snd_enum[3],
			msm_hdmi_rx_ch_get, msm_hdmi_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_RX Format", msm_snd_enum[4],
			slim0_rx_bit_format_get, slim0_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_0_RX SampleRate", msm_snd_enum[5],
			slim0_rx_sample_rate_get, slim0_rx_sample_rate_put),
	SOC_ENUM_EXT("HDMI_RX Bit Format", msm_snd_enum[4],
			hdmi_rx_bit_format_get, hdmi_rx_bit_format_put),
	SOC_ENUM_EXT("PROXY_RX Channels", msm_snd_enum[6],
			msm_proxy_rx_ch_get, msm_proxy_rx_ch_put),
	SOC_ENUM_EXT("HDMI_RX SampleRate", msm_snd_enum[7],
			hdmi_rx_sample_rate_get, hdmi_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_1_TX Channels", msm_snd_enum[8],
			msm_slim_1_tx_ch_get, msm_slim_1_tx_ch_put),
	SOC_ENUM_EXT("SLIM_1 SampleRate", msm_snd_enum[9],
			msm_slim_1_rate_get, msm_slim_1_rate_put),
	SOC_ENUM_EXT("SLIM_3_RX Channels", msm_snd_enum[10],
			msm_slim_3_rx_ch_get, msm_slim_3_rx_ch_put),
	SOC_SINGLE_EXT("Incall Rec Mode", SND_SOC_NOPM, 0, 1, 0,
		msm_incall_rec_mode_get, msm_incall_rec_mode_put),
	SOC_ENUM_EXT("QUAT_MI2S_RX Channels", msm_snd_enum[11],
			apq8084_mi2s_rx_ch_get, apq8084_mi2s_rx_ch_put),
	SOC_ENUM_EXT("QUAT_MI2S_TX Channels", msm_snd_enum[12],
			apq8084_mi2s_tx_ch_get, apq8084_mi2s_tx_ch_put),
};

static bool apq8084_swap_gnd_mic(struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->card;
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int value = gpio_get_value_cansleep(pdata->us_euro_gpio);

	pr_debug("%s: swap select switch %d to %d\n", __func__, value, !value);
	gpio_set_value_cansleep(pdata->us_euro_gpio, !value);
	return true;
}

static int msm_afe_set_config(struct snd_soc_codec *codec)
{
	int rc;
	void *config_data;

	pr_debug("%s: enter\n", __func__);

	if (!apq8084_codec_fn.get_afe_config_fn) {
		dev_err(codec->dev, "%s: codec get afe config not init'ed\n",
			__func__);
		return -EINVAL;
	}

	config_data = apq8084_codec_fn.get_afe_config_fn(codec,
						AFE_CDC_REGISTERS_CONFIG);
	rc = afe_set_config(AFE_CDC_REGISTERS_CONFIG, config_data, 0);
	if (rc) {
		pr_err("%s: Failed to set codec registers config %d\n",
		       __func__, rc);
		return rc;
	}
	config_data = apq8084_codec_fn.get_afe_config_fn(codec,
						AFE_SLIMBUS_SLAVE_CONFIG);
	rc = afe_set_config(AFE_SLIMBUS_SLAVE_CONFIG, config_data, 0);
	if (rc) {
		pr_err("%s: Failed to set slimbus slave config %d\n", __func__,
		       rc);
		return rc;
	}
	return 0;
}

static void msm_afe_clear_config(void)
{
	afe_clear_config(AFE_CDC_REGISTERS_CONFIG);
	afe_clear_config(AFE_SLIMBUS_SLAVE_CONFIG);
}

static int  apq8084_adsp_state_callback(struct notifier_block *nb,
					unsigned long value, void *priv)
{
	if (value == SUBSYS_BEFORE_SHUTDOWN) {
		pr_debug("%s: ADSP is about to shutdown. Clearing AFE config\n",
			 __func__);
		msm_afe_clear_config();
	} else if (value == SUBSYS_AFTER_POWERUP) {
		pr_debug("%s: ADSP is up\n", __func__);
	}
	return NOTIFY_OK;
}

static struct notifier_block adsp_state_notifier_block = {
	.notifier_call = apq8084_adsp_state_callback,
	.priority = -INT_MAX,
};

static int apq8084_wcd93xx_codec_up(struct snd_soc_codec *codec)
{
	int err;
	unsigned long timeout;
	int adsp_ready = 0;

	timeout = jiffies +
		msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);

	do {
		if (!q6core_is_adsp_ready()) {
			pr_err("%s: ADSP Audio isn't ready\n", __func__);
		} else {
			pr_debug("%s: ADSP Audio is ready\n", __func__);
			adsp_ready = 1;
			break;
		}
	} while (time_after(timeout, jiffies));

	if (!adsp_ready) {
		pr_err("%s: timed out waiting for ADSP Audio\n", __func__);
		return -ETIMEDOUT;
	}

	err = msm_afe_set_config(codec);
	if (err)
		pr_err("%s: Failed to set AFE config. err %d\n",
				__func__, err);
	return err;
}

static int apq8084_codec_event_cb(struct snd_soc_codec *codec,
				  enum wcd9xxx_codec_event codec_event)
{
	switch (codec_event) {
	case WCD9XXX_CODEC_EVENT_CODEC_UP:
		return apq8084_wcd93xx_codec_up(codec);
		break;
	default:
		pr_err("%s: UnSupported codec event %d\n",
				__func__, codec_event);
		return -EINVAL;
	}
}

static int msm_snd_get_ext_clk_cnt(void)
{
	return clk_users;
}

static int apq8084_tomtom_cpe_enable(struct snd_soc_codec *codec)
{
	int ret = 0;

	ret = tomtom_enable_cpe(codec);
	if (IS_ERR_VALUE(ret))
		pr_err("%s: CPE enable failed, err (0x%x)\n",
			__func__, ret);
	return ret;
}

static ssize_t cpe_load_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int ret = 0;

	if (cpe_load) {
		pr_err("%s: CPE already loaded\n",
			__func__);
		return count;
	}

	sscanf(buf, "%du", &cpe_load);

	if (cpe_load) {
		ret = apq8084_tomtom_cpe_enable(cpe_priv.cdc_handle);
		if (IS_ERR_VALUE(ret))
			cpe_load = 0;
		else
			pr_info("%s: CPE enabled for tomtom_codec\n",
				__func__);
	}

	return count;
}

static int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	void *config_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	bool cdc_type = 0;

	/* Codec SLIMBUS configuration
	 * RX1, RX2, RX3, RX4, RX5, RX6, RX7, RX8, RX9, RX10, RX11, RX12, RX13
	 * TX1, TX2, TX3, TX4, TX5, TX6, TX7, TX8, TX9, TX10, TX11, TX12, TX13
	 * TX14, TX15, TX16
	 */
	unsigned int rx_ch[TOMTOM_RX_MAX] = {144, 145, 146, 147, 148, 149, 150,
					    151, 152, 153, 154, 155, 156};
	unsigned int tx_ch[TOMTOM_TX_MAX]  = {128, 129, 130, 131, 132, 133,
					     134, 135, 136, 137, 138, 139,
					     140, 141, 142, 143};

	pr_info("%s(), dev_name%s\n", __func__, dev_name(cpu_dai->dev));
	if (!strcmp(dev_name(codec_dai->dev), "tomtom_codec"))
		cdc_type = 1; /* TOMTOM codec */
	rtd->pmdown_time = 0;
	err = snd_soc_add_codec_controls(codec, msm_snd_controls,
					 ARRAY_SIZE(msm_snd_controls));
	if (err < 0)
		return err;

	err = apq8084_liquid_ext_spk_power_amp_init();
	if (err) {
		pr_err("%s: LiQUID 8084 CLASS_D PAs init failed (%d)\n",
			__func__, err);
		return err;
	}

	err = apq8084_liquid_init_docking(dapm);
	if (err) {
		pr_err("%s: LiQUID 8084 init Docking stat IRQ failed (%d)\n",
			   __func__, err);
		return err;
	}

	snd_soc_dapm_new_controls(dapm, apq8084_dapm_widgets,
				ARRAY_SIZE(apq8084_dapm_widgets));
	snd_soc_dapm_enable_pin(dapm, "Lineout_1 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_3 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_2 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_4 amp");

	snd_soc_dapm_ignore_suspend(dapm, "Lineout_1 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_3 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_2 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_4 amp");
	snd_soc_dapm_ignore_suspend(dapm, "SPK_ultrasound amp");
	snd_soc_dapm_ignore_suspend(dapm, "Handset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "ANCRight Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "ANCLeft Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic2");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic3");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic4");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic5");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic6");

	snd_soc_dapm_ignore_suspend(dapm, "MADINPUT");
	snd_soc_dapm_ignore_suspend(dapm, "EAR");
	snd_soc_dapm_ignore_suspend(dapm, "HEADPHONE");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT1");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT2");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT3");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT4");
	snd_soc_dapm_ignore_suspend(dapm, "SPK_OUT");
	snd_soc_dapm_ignore_suspend(dapm, "ANC HEADPHONE");
	snd_soc_dapm_ignore_suspend(dapm, "ANC EAR");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC5");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC6");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC5");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC6");

	snd_soc_dapm_sync(dapm);

	err = apq8084_ext_mclk_gpio_init();
	if (err) {
		pr_err("%s: apq8084 mclk_gpio init failed (%d)\n",
			__func__, err);
	}
	if (ext_mclk_gpio < 0)
		codec_clk = clk_get(cpu_dai->dev, "osr_clk");

	snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
				    tx_ch, ARRAY_SIZE(rx_ch), rx_ch);

	if (cdc_type) {
		apq8084_codec_fn.mclk_enable_fn = tomtom_mclk_enable;
		apq8084_codec_fn.get_afe_config_fn = tomtom_get_afe_config;
		apq8084_codec_fn.mbhc_hs_detect = tomtom_hs_detect;
		apq8084_codec_fn.mbhc_hs_detect_exit = tomtom_hs_detect_exit;
	} else {
		apq8084_codec_fn.mclk_enable_fn = taiko_mclk_enable;
		apq8084_codec_fn.get_afe_config_fn = taiko_get_afe_config;
		apq8084_codec_fn.mbhc_hs_detect = taiko_hs_detect;
		apq8084_codec_fn.mbhc_hs_detect_exit = taiko_hs_detect_exit;
	}

	err = msm_afe_set_config(codec);
	if (err) {
		pr_err("%s: Failed to set AFE config %d\n", __func__, err);
		goto out;
	}

	config_data = apq8084_codec_fn.get_afe_config_fn(codec,
							 AFE_AANC_VERSION);
	err = afe_set_config(AFE_AANC_VERSION, config_data, 0);
	if (err) {
		pr_err("%s: Failed to set aanc version %d\n",
			__func__, err);
		goto out;
	}
	config_data = apq8084_codec_fn.get_afe_config_fn(codec,
				AFE_CDC_CLIP_REGISTERS_CONFIG);
	if (config_data) {
		err = afe_set_config(AFE_CDC_CLIP_REGISTERS_CONFIG,
					config_data, 0);
		if (err) {
			pr_err("%s: Failed to set clip registers %d\n",
				__func__, err);
			return err;
		}
	}
	config_data = apq8084_codec_fn.get_afe_config_fn(codec,
							 AFE_CLIP_BANK_SEL);
	if (config_data) {
		err = afe_set_config(AFE_CLIP_BANK_SEL, config_data, 0);
		if (err) {
			pr_err("%s: Failed to set AFE bank selection %d\n",
				__func__, err);
			return err;
		}
	}
	/* start mbhc */
	mbhc_cfg.calibration = def_codec_mbhc_cal();
	if (mbhc_cfg.calibration) {
		err = apq8084_codec_fn.mbhc_hs_detect(codec,
						      &mbhc_cfg);
		if (err)
			goto out;
	} else {
		err = -ENOMEM;
		goto out;
	}
	adsp_state_notifier =
	    subsys_notif_register_notifier("adsp",
					   &adsp_state_notifier_block);
	if (!adsp_state_notifier) {
		pr_err("%s: Failed to register adsp state notifier\n",
		       __func__);
		err = -EFAULT;
		apq8084_codec_fn.mbhc_hs_detect_exit(codec);
		goto out;
	}

	if (cdc_type) {
		tomtom_event_register(apq8084_codec_event_cb, rtd->codec);
		tomtom_register_ext_clk_cb(msm_snd_enable_codec_ext_clk,
					   msm_snd_get_ext_clk_cnt,
					   rtd->codec);
		cpe_priv.cdc_handle = codec;
		cpe_priv.attr_group = &attr_grp;
		cpe_priv.cpe_load_kobj = kobject_create_and_add("snd_apq8084",
						       kernel_kobj);
		if (!cpe_priv.cpe_load_kobj) {
			pr_err("%s: cpe_load: sysfs create_add failed\n",
				__func__);
		} else if (sysfs_create_group(cpe_priv.cpe_load_kobj,
					      cpe_priv.attr_group)) {
			pr_err("%s: sysfs_create_group failed\n", __func__);
			kobject_del(cpe_priv.cpe_load_kobj);
		}

		err = msm_snd_enable_codec_ext_clk(rtd->codec, 1, false);
		if (IS_ERR_VALUE(err)) {
			pr_err("%s: Failed to enable mclk, err = 0x%x\n",
				__func__, err);
			goto out;
		}
		tomtom_enable_qfuse_sensing(rtd->codec);
		err = msm_snd_enable_codec_ext_clk(rtd->codec, 0, false);
		if (IS_ERR_VALUE(err)) {
			pr_err("%s: Failed to disable mclk, err = 0x%x\n",
				__func__, err);
			goto out;
		}
	} else
		taiko_event_register(apq8084_codec_event_cb, rtd->codec);

	codec_reg_done = true;
	return 0;
out:
	if (ext_mclk_gpio < 0)
		clk_put(codec_clk);
	return err;
}

static int msm_stubrx_init(struct snd_soc_pcm_runtime *rtd)
{
	rtd->pmdown_time = 0;
	return 0;
}

static int apq8084_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	pr_debug("%s(): dai_link_str_name = %s cpu_dai = %s codec_dai = %s\n",
		  __func__, rtd->dai_link->stream_name,
		  rtd->dai_link->cpu_dai_name,
		  rtd->dai_link->codec_dai_name);
	return 0;
}

void *def_codec_mbhc_cal(void)
{
	void *codec_cal;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;
	u8 *n_ready, *n_cic, *gain;

	codec_cal = kzalloc(WCD9XXX_MBHC_CAL_SIZE(WCD9XXX_MBHC_DEF_BUTTONS,
						WCD9XXX_MBHC_DEF_RLOADS),
			    GFP_KERNEL);
	if (!codec_cal) {
		pr_err("%s: out of memory\n", __func__);
		return NULL;
	}

#define S(X, Y) ((WCD9XXX_MBHC_CAL_GENERAL_PTR(codec_cal)->X) = (Y))
	S(t_ldoh, 100);
	S(t_bg_fast_settle, 100);
	S(t_shutdown_plug_rem, 255);
	S(mbhc_nsa, 4);
	S(mbhc_navg, 4);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_PLUG_DET_PTR(codec_cal)->X) = (Y))
	S(mic_current, TOMTOM_PID_MIC_5_UA);
	S(hph_current, TOMTOM_PID_MIC_5_UA);
	S(t_mic_pid, 100);
	S(t_ins_complete, 250);
	S(t_ins_retry, 200);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(codec_cal)->X) = (Y))
	S(v_no_mic, 30);
	S(v_hs_max, 2400);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_BTN_DET_PTR(codec_cal)->X) = (Y))
	S(c[0], 62);
	S(c[1], 124);
	S(nc, 1);
	S(n_meas, 3);
	S(mbhc_nsc, 11);
	S(n_btn_meas, 1);
	S(n_btn_con, 2);
	S(num_btn, WCD9XXX_MBHC_DEF_BUTTONS);
	S(v_btn_press_delta_sta, 100);
	S(v_btn_press_delta_cic, 50);
#undef S
	btn_cfg = WCD9XXX_MBHC_CAL_BTN_DET_PTR(codec_cal);
	btn_low = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_V_BTN_LOW);
	btn_high = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg,
					       MBHC_BTN_DET_V_BTN_HIGH);
	btn_low[0] = -50;
	btn_high[0] = 20;
	btn_low[1] = 21;
	btn_high[1] = 61;
	btn_low[2] = 62;
	btn_high[2] = 104;
	btn_low[3] = 105;
	btn_high[3] = 148;
	btn_low[4] = 149;
	btn_high[4] = 189;
	btn_low[5] = 190;
	btn_high[5] = 228;
	btn_low[6] = 229;
	btn_high[6] = 269;
	btn_low[7] = 270;
	btn_high[7] = 500;
	n_ready = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_N_READY);
	n_ready[0] = 80;
	n_ready[1] = 68;
	n_cic = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_N_CIC);
	n_cic[0] = 60;
	n_cic[1] = 47;
	gain = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_GAIN);
	gain[0] = 11;
	gain[1] = 9;

	return codec_cal;
}

static int msm_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch[SLIM_MAX_RX_PORTS], tx_ch[SLIM_MAX_TX_PORTS];
	unsigned int rx_ch_cnt = 0, tx_ch_cnt = 0;
	unsigned int user_set_tx_ch = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("%s: rx_0_ch=%d\n", __func__, msm_slim_0_rx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
					&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n", __func__);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
						  msm_slim_0_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map\n", __func__);
			goto end;
		}
	} else {

		pr_debug("%s: %s_tx_dai_id_%d_ch=%d\n", __func__,
			 codec_dai->name, codec_dai->id, user_set_tx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
					 &tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n", __func__);
			goto end;
		}
		/* For <codec>_tx1 case */
		if (codec_dai->id == 1)
			user_set_tx_ch = msm_slim_0_tx_ch;
		/* For <codec>_tx2 case */
		else if (codec_dai->id == 3)
			user_set_tx_ch = params_channels(params);
		else if (codec_dai->id == 5)
			/* DAI 5 is used for external EC reference from codec.
			 * Since Rx is fed as reference for EC, the config of
			 * this DAI is based on that of the Rx path.
			 */
			user_set_tx_ch = msm_slim_0_rx_ch;
		else
			user_set_tx_ch = tx_ch_cnt;

		pr_debug("%s: msm_slim_0_tx_ch(%d)user_set_tx_ch(%d)tx_ch_cnt(%d)\n",
			 __func__, msm_slim_0_tx_ch, user_set_tx_ch, tx_ch_cnt);

		ret = snd_soc_dai_set_channel_map(cpu_dai,
						  user_set_tx_ch, tx_ch, 0 , 0);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map\n", __func__);
			goto end;
		}
	}
end:
	return ret;
}

static void apq8084_snd_shudown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	pr_debug("%s(): dai_link_str_name = %s cpu_dai = %s codec_dai = %s\n",
		  __func__, rtd->dai_link->stream_name,
		  rtd->dai_link->cpu_dai_name, rtd->dai_link->codec_dai_name);
}

static struct snd_soc_ops apq8084_be_ops = {
	.startup = apq8084_snd_startup,
	.hw_params = msm_snd_hw_params,
	.shutdown = apq8084_snd_shudown,
};

static int apq8084_slimbus_1_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct slim_controller *slim = slim_busnum_to_ctrl(1);

	pr_debug("%s(): dai_link_str_name = %s cpu_dai = %s codec_dai = %s\n",
		  __func__, rtd->dai_link->stream_name,
		  rtd->dai_link->cpu_dai_name,
		  rtd->dai_link->codec_dai_name);

	if (slim != NULL)
		pm_runtime_get_sync(slim->dev.parent);

	return 0;
}

static void apq8084_slimbus_1_shudown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct slim_controller *slim = slim_busnum_to_ctrl(1);

	pr_debug("%s(): dai_link_str_name = %s cpu_dai = %s codec_dai = %s\n",
		  __func__, rtd->dai_link->stream_name,
		  rtd->dai_link->cpu_dai_name, rtd->dai_link->codec_dai_name);

	if (slim != NULL) {
		pm_runtime_mark_last_busy(slim->dev.parent);
		pm_runtime_put(slim->dev.parent);
	}
}

static int apq8084_slimbus_2_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch[SLIM_MAX_RX_PORTS], tx_ch[SLIM_MAX_TX_PORTS];
	unsigned int rx_ch_cnt = 0, tx_ch_cnt = 0;
	unsigned int num_tx_ch = 0;
	unsigned int num_rx_ch = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		num_rx_ch =  params_channels(params);
		pr_debug("%s: %s rx_dai_id = %d  num_ch = %d\n", __func__,
			codec_dai->name, codec_dai->id, num_rx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n", __func__);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
				num_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map\n", __func__);
			goto end;
		}
	} else {
		num_tx_ch =  params_channels(params);
		pr_debug("%s: %s  tx_dai_id = %d  num_ch = %d\n", __func__,
			codec_dai->name, codec_dai->id, num_tx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n", __func__);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai,
				num_tx_ch, tx_ch, 0 , 0);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map\n", __func__);
			goto end;
		}
	}
end:
	return ret;
}

static int apq8084_slimbus_1_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch = SLIM_1_RX_1, tx_ch[2] = {SLIM_1_TX_1, SLIM_1_TX_2};

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("%s: APQ USB TX -> SLIMBUS_1_RX -> MDM TX shared ch %d\n",
			 __func__, rx_ch);

		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0, 1, &rx_ch);
		if (ret < 0) {
			pr_err("%s: Erorr %d setting SLIMBUS_1 RX channel map\n",
				__func__, ret);
			goto end;
		}
	} else {
		pr_debug("%s: MDM RX ->SLIMBUS_1_TX ->APQ USB Rx shared ch %d %d\n",
			  __func__, tx_ch[0], tx_ch[1]);

		ret = snd_soc_dai_set_channel_map(cpu_dai, msm_slim_1_tx_ch,
						  tx_ch, 0, 0);
		if (ret < 0) {
			pr_err("%s: Erorr %d setting SLIMBUS_1 TX channel map\n",
				__func__, ret);
			goto end;
		}
	}

end:
	return ret;
}

static int apq8084_slimbus_3_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch[2] = {SLIM_3_RX_1, SLIM_3_RX_2};
	unsigned int tx_ch[2] = {SLIM_3_TX_1, SLIM_3_TX_2};

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("%s: SLIMBUS_3_RX_ch %d, sch %d %d\n",
			 __func__, msm_slim_3_rx_ch, rx_ch[0], rx_ch[1]);

		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
						  msm_slim_3_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: Erorr %d setting SLIMBUS_3 RX channel map\n",
				__func__, ret);
			goto end;
		}
	} else {
		pr_debug("%s: MDM RX -> SLIMBUS_3_TX -> APQ HDMI ch: %d, %d\n",
			 __func__, tx_ch[0], tx_ch[1]);

		ret = snd_soc_dai_set_channel_map(cpu_dai, 2, tx_ch, 0, 0);
		if (ret < 0) {
			pr_err("%s: Erorr %d setting SLIMBUS_3 TX channel map\n",
				__func__, ret);
			goto end;
		}
	}

end:
	return ret;
}

static int apq8084_slimbus_4_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch = SLIM_4_RX_1;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("%s: SLIMBUS_4_RX -> MDM TX shared ch %d\n",
			 __func__, rx_ch);

		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0, 1, &rx_ch);
		if (ret < 0) {
			pr_err("%s: Erorr %d setting SLIM_4 RX channel map\n",
				__func__, ret);
		}
	}
	return ret;
}

static int apq8084_slimbus_6_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch = SLIM_6_RX_1, tx_ch[2];

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("%s: SLIMBUS_6_RX -> MDM TX shared ch %d\n",
			 __func__, rx_ch);

		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0, 1, &rx_ch);
		if (ret < 0) {
			pr_err("%s: Erorr %d setting SLIM_6 RX channel map\n",
				__func__, ret);
		}
	} else {
		if (rec_mode == INCALL_REC_STEREO) {
			tx_ch[0] = SLIM_6_TX_1;
			tx_ch[1] = SLIM_6_TX_2;
			ret = snd_soc_dai_set_channel_map(cpu_dai, 2,
							  tx_ch, 0, 0);
		} else {
			tx_ch[0] = SLIM_6_TX_1;
			ret = snd_soc_dai_set_channel_map(cpu_dai, 1,
							  tx_ch, 0, 0);
		}
		pr_debug("%s: Incall Record shared tx_ch[0]:%d, tx_ch[1]:%d\n",
			__func__, tx_ch[0], tx_ch[1]);

		if (ret < 0) {
			pr_err("%s: Erorr %d setting SLIM_6 TX channel map\n",
				__func__, ret);

		}
	}
	return ret;
}

static struct snd_soc_ops apq8084_slimbus_1_be_ops = {
	.startup = apq8084_slimbus_1_startup,
	.hw_params = apq8084_slimbus_1_hw_params,
	.shutdown = apq8084_slimbus_1_shudown,
};

static struct snd_soc_ops apq8084_slimbus_2_be_ops = {
	.startup = apq8084_snd_startup,
	.hw_params = apq8084_slimbus_2_hw_params,
	.shutdown = apq8084_snd_shudown,
};

static struct snd_soc_ops apq8084_slimbus_3_be_ops = {
	.startup = apq8084_snd_startup,
	.hw_params = apq8084_slimbus_3_hw_params,
	.shutdown = apq8084_snd_shudown,
};

static struct snd_soc_ops apq8084_slimbus_4_be_ops = {
	.startup = apq8084_snd_startup,
	.hw_params = apq8084_slimbus_4_hw_params,
	.shutdown = apq8084_snd_shudown,
};

static struct snd_soc_ops apq8084_slimbus_6_be_ops = {
	.startup = apq8084_snd_startup,
	.hw_params = apq8084_slimbus_6_hw_params,
	.shutdown = apq8084_snd_shudown,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link apq8084_common_dai_links[] = {
	/* FrontEnd DAI Links */
	{
		.name = "APQ8084 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{
		.name = "APQ8084 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name   = "MultiMedia2",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{
		.name = "Circuit-Switch Voice",
		.stream_name = "CS-Voice",
		.cpu_dai_name   = "CS-VOICE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
	},
	{
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name	= "VoIP",
		.platform_name  = "msm-voip-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_VOIP,
	},
	{
		.name = "APQ8084 LPA",
		.stream_name = "LPA",
		.cpu_dai_name	= "MultiMedia3",
		.platform_name  = "msm-pcm-lpa",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* Hostless PCM purpose */
	{
		.name = "SLIMBUS_0 Hostless",
		.stream_name = "SLIMBUS_0 Hostless",
		.cpu_dai_name = "SLIMBUS0_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		/* this dai link has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Quaternary MI2S TX Hostless",
		.stream_name = "Quaternary MI2S_TX Hostless Capture",
		.cpu_dai_name = "QUAT_MI2S_TX_HOSTLESS",
		.platform_name	= "msm-pcm-hostless",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "MSM AFE-PCM RX",
		.stream_name = "AFE-PROXY RX",
		.cpu_dai_name = "msm-dai-q6-dev.241",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
	},
	{
		.name = "MSM AFE-PCM TX",
		.stream_name = "AFE-PROXY TX",
		.cpu_dai_name = "msm-dai-q6-dev.240",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
	},
	{
		.name = "APQ8084 Compress1",
		.stream_name = "Compress1",
		.cpu_dai_name	= "MultiMedia4",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE
			| ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA4,
	},
	{
		.name = "AUXPCM Hostless",
		.stream_name = "AUXPCM Hostless",
		.cpu_dai_name   = "AUXPCM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "SLIMBUS_1 Hostless",
		.stream_name = "SLIMBUS_1 Hostless",
		.cpu_dai_name = "SLIMBUS1_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		/* this dai link has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "SLIMBUS_3 Hostless",
		.stream_name = "SLIMBUS_3 Hostless",
		.cpu_dai_name = "SLIMBUS3_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		/* this dai link has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "SLIMBUS_4 Hostless",
		.stream_name = "SLIMBUS_4 Hostless",
		.cpu_dai_name = "SLIMBUS4_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		/* this dai link has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "VoLTE",
		.stream_name = "VoLTE",
		.cpu_dai_name   = "VoLTE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOLTE,
	},
	{
		.name = "APQ8084 LowLatency",
		.stream_name = "MultiMedia5",
		.cpu_dai_name   = "MultiMedia5",
		.platform_name  = "msm-pcm-dsp.1",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA5,
	},
	/* LSM FE */
	{
		.name = "Listen 1 Audio Service",
		.stream_name = "Listen 1 Audio Service",
		.cpu_dai_name = "LSM1",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
			     SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM1,
	},
	/* Multiple Tunnel instances */
	{
		.name = "APQ8084 Compress2",
		.stream_name = "Compress2",
		.cpu_dai_name	= "MultiMedia7",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE
			| ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA7,
	},
	{
		.name = "APQ8084 Compress3",
		.stream_name = "Compress3",
		.cpu_dai_name	= "MultiMedia10",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE
			| ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA10,
	},
	{
		.name = "APQ8084 Compr8",
		.stream_name = "COMPR8",
		.cpu_dai_name	= "MultiMedia8",
		.platform_name  = "msm-compr-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE
			| ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA8,
	},
	/* Voice Stub */
	{
		.name = "Voice Stub",
		.stream_name = "Voice Stub",
		.cpu_dai_name = "VOICE_STUB",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
		 SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "VoLTE Stub",
		.stream_name = "VoLTE Stub",
		.cpu_dai_name   = "VOLTE_STUB",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	/* HDMI Hostless */
	{
		.name = "HDMI_RX_HOSTLESS",
		.stream_name = "HDMI_RX_HOSTLESS",
		.cpu_dai_name = "HDMI_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "INT_HFP_BT Hostless",
		.stream_name = "INT_HFP_BT Hostless",
		.cpu_dai_name   = "INT_HFP_BT_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "APQ8084 HFP TX",
		.stream_name = "MultiMedia6",
		.cpu_dai_name = "MultiMedia6",
		.platform_name  = "msm-pcm-loopback",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA6,
	},
	{
		.name = "Voice2 Stub",
		.stream_name = "Voice2 Stub",
		.cpu_dai_name = "VOICE2_STUB",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		/* this dainlink has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Listen 2 Audio Service",
		.stream_name = "Listen 2 Audio Service",
		.cpu_dai_name = "LSM2",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
			     SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM2,
	},
	{
		.name = "Listen 3 Audio Service",
		.stream_name = "Listen 3 Audio Service",
		.cpu_dai_name = "LSM3",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
			     SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM3,
	},
	{
		.name = "Listen 4 Audio Service",
		.stream_name = "Listen 4 Audio Service",
		.cpu_dai_name = "LSM4",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
			     SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM4,
	},
	{
		.name = "Listen 5 Audio Service",
		.stream_name = "Listen 5 Audio Service",
		.cpu_dai_name = "LSM5",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
			     SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM5,
	},
	{
		.name = "Listen 6 Audio Service",
		.stream_name = "Listen 6 Audio Service",
		.cpu_dai_name = "LSM6",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
			     SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM6,
	},
	{
		.name = "Listen 7 Audio Service",
		.stream_name = "Listen 7 Audio Service",
		.cpu_dai_name = "LSM7",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
			     SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM7,
	},
	{
		.name = "Listen 8 Audio Service",
		.stream_name = "Listen 8 Audio Service",
		.cpu_dai_name = "LSM8",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
			     SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM8,
	},
	{
		.name = "QCHAT",
		.stream_name = "QCHAT",
		.cpu_dai_name   = "QCHAT",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_QCHAT,
	},
	/* Multiple Offload instances */
	{
		.name = "APQ8084 Compress4",
		.stream_name = "Compress4",
		.cpu_dai_name	= "MultiMedia11",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE
			| ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA11,
	},
	{
		.name = "APQ8084 Compress5",
		.stream_name = "Compress5",
		.cpu_dai_name	= "MultiMedia12",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE
			| ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA12,
	},
	{
		.name = "APQ8084 Compress6",
		.stream_name = "Compress6",
		.cpu_dai_name	= "MultiMedia13",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE
			| ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA13,
	},
	{
		.name = "APQ8084 Compress7",
		.stream_name = "Compress7",
		.cpu_dai_name	= "MultiMedia14",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE
			| ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA14,
	},
	{
		.name = "APQ8084 Compress8",
		.stream_name = "Compress8",
		.cpu_dai_name	= "MultiMedia15",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE
			| ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA15,
	},
	{
		.name = "APQ8084 Compress9",
		.stream_name = "Compress9",
		.cpu_dai_name	= "MultiMedia16",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE
			| ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dai link has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA16,
	},
	{
		.name = "APQ8084 Media9",
		.stream_name = "MultiMedia9",
		.cpu_dai_name   = "MultiMedia9",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA9,
	},
};

static struct snd_soc_dai_link apq8084_tomtom_fe_dai_links[] = {
	{
		.name = LPASS_BE_SLIMBUS_4_TX,
		.stream_name = "Slimbus4 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16393",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "tomtom_codec",
		.codec_dai_name	= "tomtom_vifeedback",
		.be_id = MSM_BACKEND_DAI_SLIMBUS_4_TX,
		.be_hw_params_fixup = msm_slim_4_tx_be_hw_params_fixup,
		.ops = &apq8084_be_ops,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
	},
	/* Ultrasound RX Back End DAI Link */
	{
		.name = "SLIMBUS_2 Hostless Playback",
		.stream_name = "SLIMBUS_2 Hostless Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16388",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "tomtom_codec",
		.codec_dai_name = "tomtom_rx2",
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ops = &apq8084_slimbus_2_be_ops,
	},
	/* Ultrasound TX Back End DAI Link */
	{
		.name = "SLIMBUS_2 Hostless Capture",
		.stream_name = "SLIMBUS_2 Hostless Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16389",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "tomtom_codec",
		.codec_dai_name = "tomtom_tx2",
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ops = &apq8084_slimbus_2_be_ops,
	},
};

static struct snd_soc_dai_link apq8084_taiko_fe_dai_links[] = {
	{
		.name = LPASS_BE_SLIMBUS_4_TX,
		.stream_name = "Slimbus4 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16393",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "taiko_codec",
		.codec_dai_name	= "taiko_vifeedback",
		.be_id = MSM_BACKEND_DAI_SLIMBUS_4_TX,
		.be_hw_params_fixup = msm_slim_4_tx_be_hw_params_fixup,
		.ops = &apq8084_be_ops,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
	},
	/* Ultrasound RX Back End DAI Link */
	{
		.name = "SLIMBUS_2 Hostless Playback",
		.stream_name = "SLIMBUS_2 Hostless Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16388",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "taiko_codec",
		.codec_dai_name = "taiko_rx2",
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ops = &apq8084_slimbus_2_be_ops,
	},
	/* Ultrasound TX Back End DAI Link */
	{
		.name = "SLIMBUS_2 Hostless Capture",
		.stream_name = "SLIMBUS_2 Hostless Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16389",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "taiko_codec",
		.codec_dai_name = "taiko_tx2",
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ops = &apq8084_slimbus_2_be_ops,
	},
};

static struct snd_soc_dai_link apq8084_common_be_dai_links[] = {
	/* Backend AFE DAI Links */
	{
		.name = LPASS_BE_AFE_PCM_RX,
		.stream_name = "AFE Playback",
		.cpu_dai_name = "msm-dai-q6-dev.224",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_RX,
		.be_hw_params_fixup = msm_proxy_rx_be_hw_params_fixup,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_AFE_PCM_TX,
		.stream_name = "AFE Capture",
		.cpu_dai_name = "msm-dai-q6-dev.225",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_TX,
		.be_hw_params_fixup = msm_proxy_tx_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Primary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_AUXPCM_RX,
		.stream_name = "AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_pri_auxpcm_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		/* this dai link has playback support */
	},
	{
		.name = LPASS_BE_AUXPCM_TX,
		.stream_name = "AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_pri_auxpcm_be_ops,
		.ignore_suspend = 1,
	},
	/* Secondary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_SEC_AUXPCM_RX,
		.stream_name = "Sec AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_SEC_AUXPCM_RX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_sec_auxpcm_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		/* this dai link has playback support */
	},
	{
		.name = LPASS_BE_SEC_AUXPCM_TX,
		.stream_name = "Sec AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_SEC_AUXPCM_TX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_sec_auxpcm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_1_RX,
		.stream_name = "Slimbus1 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16386",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-rx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_1_RX,
		.be_hw_params_fixup = msm_slim_1_rx_be_hw_params_fixup,
		.ops = &apq8084_slimbus_1_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_1_TX,
		.stream_name = "Slimbus1 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16387",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-tx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_1_TX,
		.be_hw_params_fixup = msm_slim_1_tx_be_hw_params_fixup,
		.ops = &apq8084_slimbus_1_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_3_RX,
		.stream_name = "Slimbus3 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16390",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-rx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_3_RX,
		.be_hw_params_fixup = msm_slim_3_rx_be_hw_params_fixup,
		.ops = &apq8084_slimbus_3_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_3_TX,
		.stream_name = "Slimbus3 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16391",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-tx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_3_TX,
		.be_hw_params_fixup = msm_slim_3_tx_be_hw_params_fixup,
		.ops = &apq8084_slimbus_3_be_ops,
		.ignore_suspend = 1,
	},
	/* Incall Record Uplink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_TX,
		.stream_name = "Voice Uplink Capture",
		.cpu_dai_name = "msm-dai-q6-dev.32772",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Record Downlink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_RX,
		.stream_name = "Voice Downlink Capture",
		.cpu_dai_name = "msm-dai-q6-dev.32771",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Music BACK END DAI Link */
	{
		.name = LPASS_BE_VOICE_PLAYBACK_TX,
		.stream_name = "Voice Farend Playback",
		.cpu_dai_name = "msm-dai-q6-dev.32773",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_6_RX,
		.stream_name = "Slimbus6 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16396",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_6_RX,
		.be_hw_params_fixup = msm_slim_6_rx_be_hw_params_fixup,
		.ops = &apq8084_slimbus_6_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_6_TX,
		.stream_name = "Slimbus6 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16397",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_6_TX,
		.be_hw_params_fixup = msm_slim_6_tx_be_hw_params_fixup,
		.ops = &apq8084_slimbus_6_be_ops,
		.ignore_suspend = 1,
	},
	/* MI2S Backend DAI Links */
	{
		.name = LPASS_BE_QUAT_MI2S_RX,
		.stream_name = "Quaternary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
		.be_hw_params_fixup = &apq8084_mi2s_rx_be_hw_params_fixup,
		.ops = &apq8084_quad_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_TX,
		.stream_name = "Quaternary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		.be_hw_params_fixup = &apq8084_mi2s_tx_be_hw_params_fixup,
		.ops = &apq8084_quad_mi2s_be_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link apq8084_tomtom_be_dai_links[] = {
	/* Slimbus Backend DAI Links */
	{
		.name = LPASS_BE_SLIMBUS_0_RX,
		.stream_name = "Slimbus Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16384",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tomtom_codec",
		.codec_dai_name	= "tomtom_rx1",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.init = &msm_audrx_init,
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.ops = &apq8084_be_ops,
		.ignore_pmdown_time = 1,
		/* this dai link has playback support */
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_0_TX,
		.stream_name = "Slimbus Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16385",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tomtom_codec",
		.codec_dai_name	= "tomtom_tx1",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
		.be_hw_params_fixup = msm_slim_0_tx_be_hw_params_fixup,
		.ops = &apq8084_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_4_RX,
		.stream_name = "Slimbus4 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16392",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-rx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_4_RX,
		.be_hw_params_fixup = msm_slim_4_rx_be_hw_params_fixup,
		.ops = &apq8084_slimbus_4_be_ops,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	/* MAD BE */
	{
		.name = LPASS_BE_SLIMBUS_5_TX,
		.stream_name = "Slimbus5 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16395",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tomtom_codec",
		.codec_dai_name = "tomtom_mad1",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_5_TX,
		.be_hw_params_fixup = msm_slim_5_tx_be_hw_params_fixup,
		.ops = &apq8084_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_STUB_RX,
		.stream_name = "Stub Playback",
		.cpu_dai_name = "msm-dai-stub-dev.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tomtom_codec",
		.codec_dai_name = "tomtom_rx2",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_EXTPROC_RX,
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.init = &msm_stubrx_init,
		.ops = &apq8084_be_ops,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_STUB_TX,
		.stream_name = "Stub Capture",
		.cpu_dai_name = "msm-dai-stub-dev.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tomtom_codec",
		.codec_dai_name = "tomtom_tx1",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_EXTPROC_TX,
		.be_hw_params_fixup = msm_slim_0_tx_be_hw_params_fixup,
		.ops = &apq8084_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_STUB_1_TX,
		.stream_name = "Stub1 Capture",
		.cpu_dai_name = "msm-dai-stub-dev.3",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "tomtom_codec",
		.codec_dai_name	= "tomtom_tx3",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_EXTPROC_EC_TX,
		/* This BE is used for external EC reference from codec. Since
		 * Rx is fed as reference for EC, the config of this DAI is
		 * based on that of the Rx path.
		 */
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.ops = &apq8084_be_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link apq8084_taiko_be_dai_links[] = {
	/* Slimbus Backend DAI Links */
	{
		.name = LPASS_BE_SLIMBUS_0_RX,
		.stream_name = "Slimbus Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16384",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name	= "taiko_rx1",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE
			| ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.init = &msm_audrx_init,
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.ops = &apq8084_be_ops,
		.ignore_pmdown_time = 1,
		/* this dai link has playback support */
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_0_TX,
		.stream_name = "Slimbus Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16385",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name	= "taiko_tx1",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE
			| ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
		.be_hw_params_fixup = msm_slim_0_tx_be_hw_params_fixup,
		.ops = &apq8084_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_4_RX,
		.stream_name = "Slimbus4 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16392",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-rx",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_4_RX,
		.be_hw_params_fixup = msm_slim_4_rx_be_hw_params_fixup,
		.ops = &apq8084_slimbus_4_be_ops,
		/* this dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	/* MAD BE */
	{
		.name = LPASS_BE_SLIMBUS_5_TX,
		.stream_name = "Slimbus5 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16395",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name = "taiko_mad1",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_5_TX,
		.be_hw_params_fixup = msm_slim_5_tx_be_hw_params_fixup,
		.ops = &apq8084_be_ops,
	},
	{
		.name = LPASS_BE_STUB_RX,
		.stream_name = "Stub Playback",
		.cpu_dai_name = "msm-dai-stub-dev.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name = "taiko_rx2",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_EXTPROC_RX,
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.init = &msm_stubrx_init,
		.ops = &apq8084_be_ops,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_STUB_TX,
		.stream_name = "Stub Capture",
		.cpu_dai_name = "msm-dai-stub-dev.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "taiko_codec",
		.codec_dai_name = "taiko_tx1",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_EXTPROC_TX,
		.be_hw_params_fixup = msm_slim_0_tx_be_hw_params_fixup,
		.ops = &apq8084_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_STUB_1_TX,
		.stream_name = "Stub1 Capture",
		.cpu_dai_name = "msm-dai-stub-dev.3",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "taiko_codec",
		.codec_dai_name	= "taiko_tx3",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_EXTPROC_EC_TX,
		/* This BE is used for external EC reference from codec. Since
		 * Rx is fed as reference for EC, the config of this DAI is
		 * based on that of the Rx path.
		 */
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.ops = &apq8084_be_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link apq8084_hdmi_dai_link[] = {
	/* HDMI BACK END DAI Link */
	{
		.name = LPASS_BE_HDMI,
		.stream_name = "HDMI Playback",
		.cpu_dai_name = "msm-dai-q6-hdmi.8",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-hdmi-audio-codec-rx",
		.codec_dai_name = "msm_hdmi_audio_codec_rx_dai",
		.no_pcm = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.be_id = MSM_BACKEND_DAI_HDMI_RX,
		.be_hw_params_fixup = apq8084_hdmi_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link apq8084_cpe_lsm_dailink[] = {
	/* CPE LSM FE */
	{
		.name = "CPE Listen service",
		.stream_name = "CPE Listen Audio Service",
		.cpu_dai_name = "CPE_LSM_NOHOST",
		.platform_name = "msm-cpe-lsm",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "tomtom_mad1",
		.codec_name = "tomtom_codec",
	},
};

static struct snd_soc_dai_link apq8084_taiko_dai_links[
		 ARRAY_SIZE(apq8084_common_dai_links) +
		 ARRAY_SIZE(apq8084_taiko_fe_dai_links) +
		 ARRAY_SIZE(apq8084_common_be_dai_links) +
		 ARRAY_SIZE(apq8084_taiko_be_dai_links) +
		 ARRAY_SIZE(apq8084_hdmi_dai_link)];

static struct snd_soc_dai_link apq8084_tomtom_dai_links[
		 ARRAY_SIZE(apq8084_common_dai_links) +
		 ARRAY_SIZE(apq8084_tomtom_fe_dai_links) +
		 ARRAY_SIZE(apq8084_common_be_dai_links) +
		 ARRAY_SIZE(apq8084_tomtom_be_dai_links) +
		 ARRAY_SIZE(apq8084_hdmi_dai_link) +
		 ARRAY_SIZE(apq8084_cpe_lsm_dailink)];

struct snd_soc_card snd_soc_card_tomtom_apq8084 = {
	.name		= "apq8084-tomtom-snd-card",
};

struct snd_soc_card snd_soc_card_taiko_apq8084 = {
	.name		= "apq8084-taiko-snd-card",
};

static int apq8084_dtparse_auxpcm(struct platform_device *pdev,
				  struct msm_auxpcm_ctrl **auxpcm_ctrl,
				  char *msm_auxpcm_gpio_name[][2])
{
	int ret = 0;
	int i = 0;
	struct msm_auxpcm_gpio *pin_data = NULL;
	struct msm_auxpcm_ctrl *ctrl;
	unsigned int gpio_no[NUM_OF_AUXPCM_GPIOS];
	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;
	int auxpcm_cnt = 0;

	pin_data = devm_kzalloc(&pdev->dev, (ARRAY_SIZE(gpio_no) *
				sizeof(struct msm_auxpcm_gpio)),
				GFP_KERNEL);
	if (!pin_data) {
		dev_err(&pdev->dev, "No memory for gpio\n");
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(gpio_no); i++) {
		gpio_no[i] = of_get_named_gpio_flags(pdev->dev.of_node,
				msm_auxpcm_gpio_name[i][DT_PARSE_INDEX],
				0, &flags);

		if (gpio_no[i] > 0) {
			pin_data[i].gpio_name =
			     msm_auxpcm_gpio_name[auxpcm_cnt][GPIO_NAME_INDEX];
			pin_data[i].gpio_no = gpio_no[i];
			dev_dbg(&pdev->dev, "%s:GPIO gpio[%s] =\n"
				"0x%x\n", __func__,
				pin_data[i].gpio_name,
				pin_data[i].gpio_no);
			auxpcm_cnt++;
		} else {
			dev_err(&pdev->dev, "%s:Invalid AUXPCM GPIO[%s]= %x\n",
				 __func__,
				msm_auxpcm_gpio_name[i][GPIO_NAME_INDEX],
				gpio_no[i]);
			ret = -ENODEV;
			goto err;
		}
	}
	ctrl = devm_kzalloc(&pdev->dev,
				sizeof(struct msm_auxpcm_ctrl), GFP_KERNEL);
	if (!ctrl) {
		dev_err(&pdev->dev, "No memory for gpio\n");
		ret = -ENOMEM;
		goto err;
	}

	ctrl->pin_data = pin_data;
	ctrl->cnt = auxpcm_cnt;
	*auxpcm_ctrl = ctrl;
	return ret;

err:
	if (pin_data)
		devm_kfree(&pdev->dev, pin_data);
	return ret;
}

/**
 * msm_mi2s_get_pinctrl() - Get the MI2S pinctrl definitions.
 *
 * @pdev:	A pointer to the Audio platform device.
 *
 * Get the pinctrl states' handles from the device tree. The function doesn't
 * enforce wrong pinctrl definitions, i.e. it's the client's responsibility to
 * define all the necessary states for the board being used.
 *
 * Return 0 on success, error value otherwise.
 */
static int msm_mi2s_get_pinctrl(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_mi2s_pinctrl_info *pinctrl_info = &pdata->mi2s_pinctrl_info;
	struct pinctrl *pinctrl;
	int ret;

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		pr_err("%s: Unable to get pinctrl handle\n", __func__);
		return -EINVAL;
	}
	pinctrl_info->pinctrl = pinctrl;

	/* get all the states handles from Device Tree*/
	pinctrl_info->disable = pinctrl_lookup_state(pinctrl,
						"pmx-quad-mi2s-sleep");
	if (IS_ERR(pinctrl_info->disable)) {
		pr_err("%s: could not get disable pinstate\n", __func__);
		goto err;
	}

	pinctrl_info->quad_mi2s_active = pinctrl_lookup_state(pinctrl,
						"pmx-quad-mi2s-active");
	if (IS_ERR(pinctrl_info->quad_mi2s_active)) {
		pr_err("%s: could not get quad_mi2s_active pinstate\n",
			__func__);
		goto err;
	}

	/* Reset the MI2S TLMM pins to a default state */
	ret = pinctrl_select_state(pinctrl_info->pinctrl,
					pinctrl_info->disable);
	if (ret != 0) {
		pr_err("%s: Disable MI2S TLMM pins failed with %d\n",
			__func__, ret);
		return -EIO;
	}
	pinctrl_info->curr_mi2s_state = MI2S_STATE_DISABLE;
	return 0;

err:
	devm_pinctrl_put(pinctrl);
	pinctrl_info->pinctrl = NULL;
	return -EINVAL;
}

static int apq8084_prepare_us_euro(struct snd_soc_card *card)
{
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int ret;

	if (pdata->us_euro_gpio >= 0) {
		dev_dbg(card->dev, "%s : us_euro gpio request %d", __func__,
			pdata->us_euro_gpio);
		ret = gpio_request(pdata->us_euro_gpio,
				   "WCD93XX_CODEC_US_EURO");
		if (ret) {
			dev_err(card->dev,
				"%s: Failed to request Codec US/EURO gpio %d error %d\n",
				__func__, pdata->us_euro_gpio, ret);
			return ret;
		}
	}
	return 0;
}

static const struct of_device_id apq8084_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,apq8084-audio-taiko", .data = "taiko_codec"},
	{ .compatible = "qcom,apq8084-audio-tomtom", .data = "tomtom_codec"},
	{},
};

static struct snd_soc_card *populate_snd_card_dailinks(struct device *dev)
{
	struct snd_soc_card *card = NULL;
	struct snd_soc_dai_link *dailink;
	int len_1, len_2, len_3, len_4;
	const struct of_device_id *match;

	match = of_match_node(apq8084_asoc_machine_of_match, dev->of_node);
	if (!match) {
		dev_err(dev, "%s: No DT match found for sound card\n",
			__func__);
		return NULL;
	}

	if (!strcmp(match->data, "tomtom_codec")) {
		card = &snd_soc_card_tomtom_apq8084;
		len_1 = ARRAY_SIZE(apq8084_common_dai_links);
		len_2 = len_1 + ARRAY_SIZE(apq8084_tomtom_fe_dai_links);
		len_3 = len_2 + ARRAY_SIZE(apq8084_common_be_dai_links);

		memcpy(apq8084_tomtom_dai_links,
		       apq8084_common_dai_links,
		       sizeof(apq8084_common_dai_links));
		memcpy(apq8084_tomtom_dai_links + len_1,
		       apq8084_tomtom_fe_dai_links,
		       sizeof(apq8084_tomtom_fe_dai_links));
		memcpy(apq8084_tomtom_dai_links + len_2,
		       apq8084_common_be_dai_links,
		       sizeof(apq8084_common_be_dai_links));
		memcpy(apq8084_tomtom_dai_links + len_3,
		       apq8084_tomtom_be_dai_links,
		       sizeof(apq8084_tomtom_be_dai_links));

		dailink = apq8084_tomtom_dai_links;
		len_4 = len_3 + ARRAY_SIZE(apq8084_tomtom_be_dai_links);

	} else {
		card = &snd_soc_card_taiko_apq8084;
		len_1 = ARRAY_SIZE(apq8084_common_dai_links);
		len_2 = len_1 + ARRAY_SIZE(apq8084_taiko_fe_dai_links);
		len_3 = len_2 + ARRAY_SIZE(apq8084_common_be_dai_links);

		memcpy(apq8084_taiko_dai_links,
		       apq8084_common_dai_links,
		       sizeof(apq8084_common_dai_links));
		memcpy(apq8084_taiko_dai_links + len_1,
		       apq8084_taiko_fe_dai_links,
		       sizeof(apq8084_taiko_fe_dai_links));
		memcpy(apq8084_taiko_dai_links + len_2,
		       apq8084_common_be_dai_links,
		       sizeof(apq8084_common_be_dai_links));
		memcpy(apq8084_taiko_dai_links + len_3,
		       apq8084_taiko_be_dai_links,
		       sizeof(apq8084_taiko_be_dai_links));

		dailink = apq8084_taiko_dai_links;
		len_4 = len_3 + ARRAY_SIZE(apq8084_taiko_be_dai_links);
	}

	if (of_property_read_bool(dev->of_node, "qcom,hdmi-audio-rx")) {
		dev_dbg(dev, "%s(): hdmi audio support present\n",
				__func__);
		memcpy(dailink + len_4, apq8084_hdmi_dai_link,
			sizeof(apq8084_hdmi_dai_link));
		len_4 += ARRAY_SIZE(apq8084_hdmi_dai_link);
	} else {
		dev_dbg(dev, "%s(): No hdmi audio support\n", __func__);
	}

	if (!strcmp(match->data, "tomtom_codec")) {
		memcpy(dailink + len_4, apq8084_cpe_lsm_dailink,
		       sizeof(apq8084_cpe_lsm_dailink));
		len_4 += ARRAY_SIZE(apq8084_cpe_lsm_dailink);
	}

	card->dai_link = dailink;
	card->num_links = len_4;

	return card;
}

static int apq8084_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct apq8084_asoc_mach_data *pdata;
	int ret;
	const char *auxpcm_pri_gpio_set = NULL;
	const struct of_device_id *match;
	char *mclk_freq_prop_name;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}
	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct apq8084_asoc_mach_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Can't allocate apq8084_asoc_mach_data\n");
		return -ENOMEM;
	}

	card = populate_snd_card_dailinks(&pdev->dev);
	if (!card) {
		dev_err(&pdev->dev, "%s: Card uninitialized\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);
	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret)
		goto err;

	ret = snd_soc_of_parse_audio_routing(card,
			"qcom,audio-routing");
	if (ret)
		goto err;

	match = of_match_node(apq8084_asoc_machine_of_match,
			      pdev->dev.of_node);
	if (!match) {
		dev_err(&pdev->dev, "%s: No DT match found for sound card\n",
			__func__);
		ret = -EINVAL;
		goto err;
	}
	if (!strcmp(match->data, "tomtom_codec"))
		mclk_freq_prop_name = "qcom,tomtom-mclk-clk-freq";
	else
		mclk_freq_prop_name = "qcom,taiko-mclk-clk-freq";

	ret = of_property_read_u32(pdev->dev.of_node,
			mclk_freq_prop_name, &pdata->mclk_freq);
	if (ret) {
		dev_err(&pdev->dev, "Looking up %s property in node %s failed",
			mclk_freq_prop_name,
			pdev->dev.of_node->full_name);
		goto err;
	}

	if (pdata->mclk_freq != 9600000) {
		dev_err(&pdev->dev, "unsupported codec mclk freq %u\n",
			pdata->mclk_freq);
		ret = -EINVAL;
		goto err;
	}

	mutex_init(&cdc_mclk_mutex);
	atomic_set(&prim_auxpcm_rsc_ref, 0);
	atomic_set(&sec_auxpcm_rsc_ref, 0);
	spdev = pdev;

	ret = snd_soc_register_card(card);
	if (ret == -EPROBE_DEFER) {
		if (codec_reg_done)
			ret = -EINVAL;
		goto err;
	} else if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}

	/* Parse Primary AUXPCM info from DT */
	ret = apq8084_dtparse_auxpcm(pdev, &pdata->pri_auxpcm_ctrl,
					msm_prim_auxpcm_gpio_name);
	if (ret) {
		dev_err(&pdev->dev,
		"%s: Primary Auxpcm pin data parse failed\n", __func__);
		goto err;
	}

	/* Parse Secondary AUXPCM info from DT */
	ret = apq8084_dtparse_auxpcm(pdev, &pdata->sec_auxpcm_ctrl,
					msm_sec_auxpcm_gpio_name);
	if (ret) {
		dev_err(&pdev->dev,
		"%s: Secondary Auxpcm pin data parse failed\n", __func__);
		goto err;
	}

	/* Parse pinctrl info for MI2S ports, if defined */
	ret = msm_mi2s_get_pinctrl(pdev);
	if (!ret) {
		pr_debug("%s: MI2S pinctrl parsing successful\n", __func__);
		lpaif_quad_muxsel_virt_addr =
				ioremap(LPAIF_QUAD_MODE_MUXSEL, 4);
		if (lpaif_quad_muxsel_virt_addr == NULL) {
			pr_err("%s Quad MI2S mux virt addr is null\n",
				__func__);
			ret = -EINVAL;
			goto err;
		}
	} else {
		dev_info(&pdev->dev,
			"%s: Parsing pinctrl failed with %d. Cannot use MI2S Ports\n",
			__func__, ret);
	}

	pdata->us_euro_gpio = of_get_named_gpio(pdev->dev.of_node,
				"qcom,us-euro-gpios", 0);
	if (pdata->us_euro_gpio < 0) {
		dev_info(&pdev->dev, "property %s not detected in node %s",
			"qcom,us-euro-gpios",
			pdev->dev.of_node->full_name);
	} else {
		dev_dbg(&pdev->dev, "%s detected %d",
			"qcom,us-euro-gpios", pdata->us_euro_gpio);
		mbhc_cfg.swap_gnd_mic = apq8084_swap_gnd_mic;
	}

	ret = apq8084_prepare_us_euro(card);
	if (ret)
		dev_err(&pdev->dev, "apq8084_prepare_us_euro failed (%d)\n",
			ret);

	ret = of_property_read_string(pdev->dev.of_node,
			"qcom,prim-auxpcm-gpio-set", &auxpcm_pri_gpio_set);
	if (ret) {
		dev_err(&pdev->dev, "Looking up %s property in node %s failed",
			"qcom,prim-auxpcm-gpio-set",
			pdev->dev.of_node->full_name);
		goto err;
	}
	if (!strcmp(auxpcm_pri_gpio_set, "prim-gpio-prim")) {
		lpaif_pri_muxsel_virt_addr = ioremap(LPAIF_PRI_MODE_MUXSEL, 4);
	} else if (!strcmp(auxpcm_pri_gpio_set, "prim-gpio-tert")) {
		lpaif_pri_muxsel_virt_addr = ioremap(LPAIF_TER_MODE_MUXSEL, 4);
	} else {
		dev_err(&pdev->dev, "Invalid value %s for AUXPCM GPIO set\n",
			auxpcm_pri_gpio_set);
		ret = -EINVAL;
		goto err;
	}
	if (lpaif_pri_muxsel_virt_addr == NULL) {
		pr_err("%s Pri muxsel virt addr is null\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	lpaif_sec_muxsel_virt_addr = ioremap(LPAIF_SEC_MODE_MUXSEL, 4);
	if (lpaif_sec_muxsel_virt_addr == NULL) {
		pr_err("%s Sec muxsel virt addr is null\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	return 0;

err:
	if (pdata->mi2s_pinctrl_info.pinctrl) {
		dev_dbg(&pdev->dev, "%s: freeing MI2S pinctrl\n", __func__);
		devm_pinctrl_put(pdata->mi2s_pinctrl_info.pinctrl);
	}
	if (pdata->us_euro_gpio > 0) {
		dev_dbg(&pdev->dev, "%s free us_euro gpio %d\n",
			__func__, pdata->us_euro_gpio);
		gpio_free(pdata->us_euro_gpio);
		pdata->us_euro_gpio = 0;
	}
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int apq8084_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct apq8084_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	if (ext_spk_amp_regulator)
		regulator_put(ext_spk_amp_regulator);

	if (gpio_is_valid(ext_ult_spk_amp_gpio))
		gpio_free(ext_ult_spk_amp_gpio);

	gpio_free(pdata->us_euro_gpio);
	if (gpio_is_valid(ext_spk_amp_gpio))
		gpio_free(ext_spk_amp_gpio);

	unregister_liquid_dock_notify(&apq8084_liquid_docking_notifier);
	if (apq8084_liquid_dock_dev != NULL) {
		kfree(apq8084_liquid_dock_dev);
		apq8084_liquid_dock_dev = NULL;
	}

	if (gpio_is_valid(ext_mclk_gpio))
		gpio_free(ext_mclk_gpio);

	iounmap(lpaif_pri_muxsel_virt_addr);
	iounmap(lpaif_sec_muxsel_virt_addr);
	snd_soc_unregister_card(card);
	return 0;
}

static struct platform_driver apq8084_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = apq8084_asoc_machine_of_match,
	},
	.probe = apq8084_asoc_machine_probe,
	.remove = apq8084_asoc_machine_remove,
};
module_platform_driver(apq8084_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, apq8084_asoc_machine_of_match);
