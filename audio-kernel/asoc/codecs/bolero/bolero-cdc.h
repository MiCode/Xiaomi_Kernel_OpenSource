/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef BOLERO_CDC_H
#define BOLERO_CDC_H

#include <sound/soc.h>
#include <linux/regmap.h>

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
	BOLERO_MACRO_EVT_RX_MUTE = 1, /* for RX mute/unmute */
	BOLERO_MACRO_EVT_IMPED_TRUE, /* for imped true */
	BOLERO_MACRO_EVT_IMPED_FALSE, /* for imped false */
	BOLERO_MACRO_EVT_SSR_DOWN,
	BOLERO_MACRO_EVT_SSR_UP,
	BOLERO_MACRO_EVT_WAIT_VA_CLK_RESET,
	BOLERO_MACRO_EVT_REG_WAKE_IRQ
};

struct macro_ops {
	int (*init)(struct snd_soc_codec *codec);
	int (*exit)(struct snd_soc_codec *codec);
	u16 num_dais;
	struct device *dev;
	struct snd_soc_dai_driver *dai_ptr;
	int (*mclk_fn)(struct device *dev, bool enable);
	int (*event_handler)(struct snd_soc_codec *codec, u16 event,
			     u32 data);
	int (*reg_wake_irq)(struct snd_soc_codec *codec, u32 data);
	char __iomem *io_base;
};

#if IS_ENABLED(CONFIG_SND_SOC_BOLERO)
int bolero_register_macro(struct device *dev, u16 macro_id,
			  struct macro_ops *ops);
void bolero_unregister_macro(struct device *dev, u16 macro_id);
struct device *bolero_get_device_ptr(struct device *dev, u16 macro_id);
int bolero_request_clock(struct device *dev, u16 macro_id,
			 enum mclk_mux mclk_mux_id,
			 bool enable);
int bolero_info_create_codec_entry(
		struct snd_info_entry *codec_root,
		struct snd_soc_codec *codec);
int bolero_register_wake_irq(struct snd_soc_codec *codec, u32 data);
void bolero_clear_amic_tx_hold(struct device *dev, u16 adc_n);
#else
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

static inline int bolero_request_clock(struct device *dev, u16 macro_id,
				       enum mclk_mux mclk_mux_id,
				       bool enable)
{
	return 0;
}

static int bolero_info_create_codec_entry(
		struct snd_info_entry *codec_root,
		struct snd_soc_codec *codec)
{
	return 0;
}

static inline void bolero_clear_amic_tx_hold(struct device *dev, u16 adc_n)
{
}

static inline int bolero_register_wake_irq(struct snd_soc_codec *codec,
					   u32 data)
{
	return 0;
}
#endif /* CONFIG_SND_SOC_BOLERO */
#endif /* BOLERO_CDC_H */
