/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef BOLERO_CDC_H
#define BOLERO_CDC_H

#include <sound/soc.h>
#include <linux/regmap.h>

#define BOLERO_VERSION_1_0 0x0001
#define BOLERO_VERSION_1_1 0x0002
#define BOLERO_VERSION_1_2 0x0003
#define BOLERO_VERSION_2_0 0x0004
#define BOLERO_VERSION_2_1 0x0005

enum {
	START_MACRO,
	TX_MACRO = START_MACRO,
	RX_MACRO,
	WSA_MACRO,
	VA_MACRO,
	MAX_MACRO
};

enum mclk_mux {
	MCLK_MUX0,
	MCLK_MUX1,
	MCLK_MUX_MAX
};

enum {
	BOLERO_ADC0 = 1,
	BOLERO_ADC1,
	BOLERO_ADC2,
	BOLERO_ADC3,
	BOLERO_ADC_MAX
};

enum {
	CLK_SRC_TX_RCG = 0,
	CLK_SRC_VA_RCG,
};

enum {
	BOLERO_MACRO_EVT_RX_MUTE = 1, /* for RX mute/unmute */
	BOLERO_MACRO_EVT_IMPED_TRUE, /* for imped true */
	BOLERO_MACRO_EVT_IMPED_FALSE, /* for imped false */
	BOLERO_MACRO_EVT_SSR_DOWN,
	BOLERO_MACRO_EVT_SSR_UP,
	BOLERO_MACRO_EVT_WAIT_VA_CLK_RESET,
	BOLERO_MACRO_EVT_CLK_RESET,
	BOLERO_MACRO_EVT_REG_WAKE_IRQ,
	BOLERO_MACRO_EVT_RX_COMPANDER_SOFT_RST,
	BOLERO_MACRO_EVT_BCS_CLK_OFF,
	BOLERO_MACRO_EVT_SSR_GFMUX_UP,
	BOLERO_MACRO_EVT_PRE_SSR_UP,
	BOLERO_MACRO_EVT_RX_PA_GAIN_UPDATE,
	BOLERO_MACRO_EVT_HPHL_HD2_ENABLE, /* Enable HD2 cfg for HPHL */
	BOLERO_MACRO_EVT_HPHR_HD2_ENABLE, /* Enable HD2 cfg for HPHR */
};

enum {
	DMIC_TX = 0,
	DMIC_VA = 1,

};

struct macro_ops {
	int (*init)(struct snd_soc_component *component);
	int (*exit)(struct snd_soc_component *component);
	u16 num_dais;
	struct device *dev;
	struct snd_soc_dai_driver *dai_ptr;
	int (*mclk_fn)(struct device *dev, bool enable);
	int (*event_handler)(struct snd_soc_component *component, u16 event,
			     u32 data);
	int (*reg_wake_irq)(struct snd_soc_component *component, u32 data);
	int (*set_port_map)(struct snd_soc_component *component, u32 uc,
			    u32 size, void *data);
	int (*clk_div_get)(struct snd_soc_component *component);
	int (*clk_switch)(struct snd_soc_component *component, int clk_src);
	int (*reg_evt_listener)(struct snd_soc_component *component, bool en);
	int (*clk_enable)(struct snd_soc_component *c, bool en);
	char __iomem *io_base;
	u16 clk_id_req;
	u16 default_clk_id;
};

typedef int (*rsc_clk_cb_t)(struct device *dev, u16 event);

#if IS_ENABLED(CONFIG_SND_SOC_BOLERO)
int bolero_register_res_clk(struct device *dev, rsc_clk_cb_t cb);
void bolero_unregister_res_clk(struct device *dev);
bool bolero_is_va_macro_registered(struct device *dev);
int bolero_register_macro(struct device *dev, u16 macro_id,
			  struct macro_ops *ops);
void bolero_unregister_macro(struct device *dev, u16 macro_id);
struct device *bolero_get_device_ptr(struct device *dev, u16 macro_id);
struct device *bolero_get_rsc_clk_device_ptr(struct device *dev);
int bolero_info_create_codec_entry(
		struct snd_info_entry *codec_root,
		struct snd_soc_component *component);
int bolero_register_wake_irq(struct snd_soc_component *component, u32 data);
void bolero_clear_amic_tx_hold(struct device *dev, u16 adc_n);
int bolero_runtime_resume(struct device *dev);
int bolero_runtime_suspend(struct device *dev);
int bolero_set_port_map(struct snd_soc_component *component, u32 size, void *data);
int bolero_tx_clk_switch(struct snd_soc_component *component, int clk_src);
int bolero_register_event_listener(struct snd_soc_component *component,
				   bool enable);
void bolero_wsa_pa_on(struct device *dev, bool adie_lb);
bool bolero_check_core_votes(struct device *dev);
int bolero_tx_mclk_enable(struct snd_soc_component *c, bool enable);
int bolero_get_version(struct device *dev);
int bolero_dmic_clk_enable(struct snd_soc_component *component,
			   u32 dmic, u32 tx_mode, bool enable);
void bolero_tx_macro_mute_hs(void);
#else

static inline void bolero_tx_macro_mute_hs(void)
{
}
static inline int bolero_register_res_clk(struct device *dev, rsc_clk_cb_t cb)
{
	return 0;
}
static inline void bolero_unregister_res_clk(struct device *dev)
{
}

static bool bolero_is_va_macro_registered(struct device *dev)
{
	return false;
}

static inline int bolero_register_macro(struct device *dev,
					u16 macro_id,
					struct macro_ops *ops)
{
	return 0;
}

static inline void bolero_unregister_macro(struct device *dev, u16 macro_id)
{
}

static inline struct device *bolero_get_device_ptr(struct device *dev,
						   u16 macro_id)
{
	return NULL;
}

static int bolero_info_create_codec_entry(
		struct snd_info_entry *codec_root,
		struct snd_soc_component *component)
{
	return 0;
}

static inline void bolero_clear_amic_tx_hold(struct device *dev, u16 adc_n)
{
}

static inline int bolero_register_wake_irq(struct snd_soc_component *component,
					   u32 data)
{
	return 0;
}

static inline int bolero_runtime_resume(struct device *dev)
{
	return 0;
}

static int bolero_runtime_suspend(struct device *dev)
{
	return 0;
}

static inline int bolero_set_port_map(struct snd_soc_component *component,
				u32 size, void *data)
{
	return 0;
}

static inline int bolero_tx_clk_switch(struct snd_soc_component *component,
					int clk_src)
{
	return 0;
}

static inline int bolero_register_event_listener(
					struct snd_soc_component *component,
					bool enable)
{
	return 0;
}

static void bolero_wsa_pa_on(struct device *dev, bool adie_lb)
{
}

static inline bool bolero_check_core_votes(struct device *dev)
{
	return false;
}

static int bolero_get_version(struct device *dev)
{
	return 0;
}

static int bolero_dmic_clk_enable(struct snd_soc_component *component,
			   u32 dmic, u32 tx_mode, bool enable)
{
	return 0;
}
static int bolero_tx_mclk_enable(struct snd_soc_component *c, bool enable)
{
	return 0;
}
#endif /* CONFIG_SND_SOC_BOLERO */
#endif /* BOLERO_CDC_H */
