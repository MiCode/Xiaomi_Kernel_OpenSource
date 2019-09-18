/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <soc/snd_event.h>
#include "bolero-cdc.h"
#include "internal.h"

#define BOLERO_VERSION_1_0 0x0001
#define BOLERO_VERSION_1_1 0x0002
#define BOLERO_VERSION_1_2 0x0003
#define BOLERO_VERSION_ENTRY_SIZE 32
#define BOLERO_CDC_STRING_LEN 80

static struct snd_soc_codec_driver bolero;

/* MCLK_MUX table for all macros */
static u16 bolero_mclk_mux_tbl[MAX_MACRO][MCLK_MUX_MAX] = {
	{TX_MACRO, VA_MACRO},
	{TX_MACRO, RX_MACRO},
	{TX_MACRO, WSA_MACRO},
	{TX_MACRO, VA_MACRO},
};

static void bolero_ahb_write_device(char __iomem *io_base,
				    u16 reg, u8 value)
{
	u32 temp = (u32)(value) & 0x000000FF;

	iowrite32(temp, io_base + reg);
}

static void bolero_ahb_read_device(char __iomem *io_base,
				   u16 reg, u8 *value)
{
	u32 temp;

	temp = ioread32(io_base + reg);
	*value = (u8)temp;
}

static int __bolero_reg_read(struct bolero_priv *priv,
			     u16 macro_id, u16 reg, u8 *val)
{
	int ret = -EINVAL;
	u16 current_mclk_mux_macro;

	mutex_lock(&priv->clk_lock);
	if (!priv->dev_up) {
		dev_dbg_ratelimited(priv->dev,
			"%s: SSR in progress, exit\n", __func__);
		goto err;
	}
	current_mclk_mux_macro =
		priv->current_mclk_mux_macro[macro_id];
	if (!priv->macro_params[current_mclk_mux_macro].mclk_fn) {
		dev_dbg_ratelimited(priv->dev,
			"%s: mclk_fn not init for macro-id:%d, current_mclk_mux_macro:%d\n",
			__func__, macro_id, current_mclk_mux_macro);
		goto err;
	}
	ret = priv->macro_params[current_mclk_mux_macro].mclk_fn(
			priv->macro_params[current_mclk_mux_macro].dev, true);
	if (ret) {
		dev_dbg_ratelimited(priv->dev,
			"%s: clock enable failed for macro-id:%d, current_mclk_mux_macro:%d\n",
			__func__, macro_id, current_mclk_mux_macro);
		goto err;
	}
	bolero_ahb_read_device(
		priv->macro_params[macro_id].io_base, reg, val);
	priv->macro_params[current_mclk_mux_macro].mclk_fn(
			priv->macro_params[current_mclk_mux_macro].dev, false);
err:
	mutex_unlock(&priv->clk_lock);
	return ret;
}

static int __bolero_reg_write(struct bolero_priv *priv,
			      u16 macro_id, u16 reg, u8 val)
{
	int ret = -EINVAL;
	u16 current_mclk_mux_macro;

	mutex_lock(&priv->clk_lock);
	if (!priv->dev_up) {
		dev_dbg_ratelimited(priv->dev,
			"%s: SSR in progress, exit\n", __func__);
		goto err;
	}
	current_mclk_mux_macro =
		priv->current_mclk_mux_macro[macro_id];
	if (!priv->macro_params[current_mclk_mux_macro].mclk_fn) {
		dev_dbg_ratelimited(priv->dev,
			"%s: mclk_fn not init for macro-id:%d, current_mclk_mux_macro:%d\n",
			__func__, macro_id, current_mclk_mux_macro);
		goto err;
	}
	ret = priv->macro_params[current_mclk_mux_macro].mclk_fn(
			priv->macro_params[current_mclk_mux_macro].dev, true);
	if (ret) {
		dev_dbg_ratelimited(priv->dev,
			"%s: clock enable failed for macro-id:%d, current_mclk_mux_macro:%d\n",
			__func__, macro_id, current_mclk_mux_macro);
		goto err;
	}
	bolero_ahb_write_device(
		priv->macro_params[macro_id].io_base, reg, val);
	priv->macro_params[current_mclk_mux_macro].mclk_fn(
			priv->macro_params[current_mclk_mux_macro].dev, false);
err:
	mutex_unlock(&priv->clk_lock);
	return ret;
}

static int bolero_cdc_update_wcd_event(void *handle, u16 event, u32 data)
{
	struct bolero_priv *priv = (struct bolero_priv *)handle;

	if (!priv) {
		pr_err("%s:Invalid bolero priv handle\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case WCD_BOLERO_EVT_RX_MUTE:
		if (priv->macro_params[RX_MACRO].event_handler)
			priv->macro_params[RX_MACRO].event_handler(priv->codec,
				BOLERO_MACRO_EVT_RX_MUTE, data);
		break;
	case WCD_BOLERO_EVT_IMPED_TRUE:
		if (priv->macro_params[RX_MACRO].event_handler)
			priv->macro_params[RX_MACRO].event_handler(priv->codec,
				BOLERO_MACRO_EVT_IMPED_TRUE, data);
		break;
	case WCD_BOLERO_EVT_IMPED_FALSE:
		if (priv->macro_params[RX_MACRO].event_handler)
			priv->macro_params[RX_MACRO].event_handler(priv->codec,
				BOLERO_MACRO_EVT_IMPED_FALSE, data);
		break;
	default:
		dev_err(priv->dev, "%s: Invalid event %d trigger from wcd\n",
			__func__, event);
		return -EINVAL;
	}
	return 0;
}

static int bolero_cdc_register_notifier(void *handle,
					struct notifier_block *nblock,
					bool enable)
{
	struct bolero_priv *priv = (struct bolero_priv *)handle;

	if (!priv) {
		pr_err("%s: bolero priv is null\n", __func__);
		return -EINVAL;
	}
	if (enable)
		return blocking_notifier_chain_register(&priv->notifier,
							nblock);

	return blocking_notifier_chain_unregister(&priv->notifier,
						  nblock);
}

static void bolero_cdc_notifier_call(struct bolero_priv *priv,
				     u32 data)
{
	dev_dbg(priv->dev, "%s: notifier call, data:%d\n", __func__, data);
	blocking_notifier_call_chain(&priv->notifier,
				     data, (void *)priv->wcd_dev);
}

static bool bolero_is_valid_macro_dev(struct device *dev)
{
	if (of_device_is_compatible(dev->parent->of_node, "qcom,bolero-codec"))
		return true;

	return false;
}

static bool bolero_is_valid_codec_dev(struct device *dev)
{
	if (of_device_is_compatible(dev->of_node, "qcom,bolero-codec"))
		return true;

	return false;
}

/**
 * bolero_clear_amic_tx_hold - clears AMIC register on analog codec
 *
 * @dev: bolero device ptr.
 *
 */
void bolero_clear_amic_tx_hold(struct device *dev, u16 adc_n)
{
	struct bolero_priv *priv;
	u16 event;
	u16 amic = 0;

	if (!dev) {
		pr_err("%s: dev is null\n", __func__);
		return;
	}

	if (!bolero_is_valid_codec_dev(dev)) {
		pr_err("%s: invalid codec\n", __func__);
		return;
	}
	priv = dev_get_drvdata(dev);
	if (!priv) {
		dev_err(dev, "%s: priv is null\n", __func__);
		return;
	}
	event = BOLERO_WCD_EVT_TX_CH_HOLD_CLEAR;
	if (adc_n == BOLERO_ADC0)
		amic = 0x1;
	else if (adc_n == BOLERO_ADC2)
		amic = 0x2;
	else if (adc_n == BOLERO_ADC3)
		amic = 0x3;
	else
		return;

	bolero_cdc_notifier_call(priv, (amic << 0x10 | event));
}
EXPORT_SYMBOL(bolero_clear_amic_tx_hold);

/**
 * bolero_get_device_ptr - Get child or macro device ptr
 *
 * @dev: bolero device ptr.
 * @macro_id: ID of macro calling this API.
 *
 * Returns dev ptr on success or NULL on error.
 */
struct device *bolero_get_device_ptr(struct device *dev, u16 macro_id)
{
	struct bolero_priv *priv;

	if (!dev) {
		pr_err("%s: dev is null\n", __func__);
		return NULL;
	}

	if (!bolero_is_valid_codec_dev(dev)) {
		pr_err("%s: invalid codec\n", __func__);
		return NULL;
	}
	priv = dev_get_drvdata(dev);
	if (!priv || (macro_id >= MAX_MACRO)) {
		dev_err(dev, "%s: priv is null or invalid macro\n", __func__);
		return NULL;
	}

	return priv->macro_params[macro_id].dev;
}
EXPORT_SYMBOL(bolero_get_device_ptr);

static int bolero_copy_dais_from_macro(struct bolero_priv *priv)
{
	struct snd_soc_dai_driver *dai_ptr;
	u16 macro_idx;

	/* memcpy into bolero_dais all macro dais */
	if (!priv->bolero_dais)
		priv->bolero_dais = devm_kzalloc(priv->dev,
						priv->num_dais *
						sizeof(
						struct snd_soc_dai_driver),
						GFP_KERNEL);
	if (!priv->bolero_dais)
		return -ENOMEM;

	dai_ptr = priv->bolero_dais;

	for (macro_idx = START_MACRO; macro_idx < MAX_MACRO; macro_idx++) {
		if (priv->macro_params[macro_idx].dai_ptr) {
			memcpy(dai_ptr,
			       priv->macro_params[macro_idx].dai_ptr,
			       priv->macro_params[macro_idx].num_dais *
			       sizeof(struct snd_soc_dai_driver));
			dai_ptr += priv->macro_params[macro_idx].num_dais;
		}
	}
	return 0;
}

/**
 * bolero_register_macro - Registers macro to bolero
 *
 * @dev: macro device ptr.
 * @macro_id: ID of macro calling this API.
 * @ops: macro params to register.
 *
 * Returns 0 on success or -EINVAL on error.
 */
int bolero_register_macro(struct device *dev, u16 macro_id,
			  struct macro_ops *ops)
{
	struct bolero_priv *priv;
	int ret = -EINVAL;

	if (!dev || !ops) {
		pr_err("%s: dev or ops is null\n", __func__);
		return -EINVAL;
	}
	if (!bolero_is_valid_macro_dev(dev)) {
		dev_err(dev, "%s: child device for macro:%d not added yet\n",
			__func__, macro_id);
		return -EINVAL;
	}
	priv = dev_get_drvdata(dev->parent);
	if (!priv || (macro_id >= MAX_MACRO)) {
		dev_err(dev, "%s: priv is null or invalid macro\n", __func__);
		return -EINVAL;
	}

	priv->macro_params[macro_id].init = ops->init;
	priv->macro_params[macro_id].exit = ops->exit;
	priv->macro_params[macro_id].io_base = ops->io_base;
	priv->macro_params[macro_id].num_dais = ops->num_dais;
	priv->macro_params[macro_id].dai_ptr = ops->dai_ptr;
	priv->macro_params[macro_id].mclk_fn = ops->mclk_fn;
	priv->macro_params[macro_id].event_handler = ops->event_handler;
	priv->macro_params[macro_id].dev = dev;
	priv->current_mclk_mux_macro[macro_id] =
				bolero_mclk_mux_tbl[macro_id][MCLK_MUX0];
	if (macro_id == TX_MACRO)
		priv->macro_params[macro_id].reg_wake_irq = ops->reg_wake_irq;

	priv->num_dais += ops->num_dais;
	priv->num_macros_registered++;
	priv->macros_supported[macro_id] = true;

	if (priv->num_macros_registered == priv->num_macros) {
		ret = bolero_copy_dais_from_macro(priv);
		if (ret < 0) {
			dev_err(dev, "%s: copy_dais failed\n", __func__);
			return ret;
		}
		if (priv->macros_supported[TX_MACRO] == false) {
			bolero_mclk_mux_tbl[WSA_MACRO][MCLK_MUX0] = WSA_MACRO;
			priv->current_mclk_mux_macro[WSA_MACRO] = WSA_MACRO;
			bolero_mclk_mux_tbl[VA_MACRO][MCLK_MUX0] = VA_MACRO;
			priv->current_mclk_mux_macro[VA_MACRO] = VA_MACRO;
		}
		ret = snd_soc_register_codec(dev->parent, &bolero,
				priv->bolero_dais, priv->num_dais);
		if (ret < 0) {
			dev_err(dev, "%s: register codec failed\n", __func__);
			return ret;
		}
	}
	return 0;
}
EXPORT_SYMBOL(bolero_register_macro);

/**
 * bolero_unregister_macro - De-Register macro from bolero
 *
 * @dev: macro device ptr.
 * @macro_id: ID of macro calling this API.
 *
 */
void bolero_unregister_macro(struct device *dev, u16 macro_id)
{
	struct bolero_priv *priv;

	if (!dev) {
		pr_err("%s: dev is null\n", __func__);
		return;
	}
	if (!bolero_is_valid_macro_dev(dev)) {
		dev_err(dev, "%s: macro:%d not in valid registered macro-list\n",
			__func__, macro_id);
		return;
	}
	priv = dev_get_drvdata(dev->parent);
	if (!priv || (macro_id >= MAX_MACRO)) {
		dev_err(dev, "%s: priv is null or invalid macro\n", __func__);
		return;
	}

	priv->macro_params[macro_id].init = NULL;
	priv->macro_params[macro_id].num_dais = 0;
	priv->macro_params[macro_id].dai_ptr = NULL;
	priv->macro_params[macro_id].mclk_fn = NULL;
	priv->macro_params[macro_id].event_handler = NULL;
	priv->macro_params[macro_id].dev = NULL;
	if (macro_id == TX_MACRO)
		priv->macro_params[macro_id].reg_wake_irq = NULL;

	priv->num_dais -= priv->macro_params[macro_id].num_dais;
	priv->num_macros_registered--;

	/* UNREGISTER CODEC HERE */
	if (priv->num_macros - 1 == priv->num_macros_registered)
		snd_soc_unregister_codec(dev->parent);
}
EXPORT_SYMBOL(bolero_unregister_macro);

/**
 * bolero_request_clock - request for clock enable/disable
 *
 * @dev: macro device ptr.
 * @macro_id: ID of macro calling this API.
 * @mclk_mux_id: MCLK_MUX ID.
 * @enable: enable or disable clock flag
 *
 * Returns 0 on success or -EINVAL on error.
 */
int bolero_request_clock(struct device *dev, u16 macro_id,
			 enum mclk_mux mclk_mux_id,
			 bool enable)
{
	struct bolero_priv *priv;
	u16 mclk_mux0_macro, mclk_mux1_macro;
	int ret = 0, ret1 = 0;

	if (!dev) {
		pr_err("%s: dev is null\n", __func__);
		return -EINVAL;
	}
	if (!bolero_is_valid_macro_dev(dev)) {
		dev_err(dev, "%s: macro:%d not in valid registered macro-list\n",
			__func__, macro_id);
		return -EINVAL;
	}
	priv = dev_get_drvdata(dev->parent);
	if (!priv || (macro_id >= MAX_MACRO)) {
		dev_err(dev, "%s: priv is null or invalid macro\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&priv->clk_lock);

	mclk_mux0_macro =  bolero_mclk_mux_tbl[macro_id][MCLK_MUX0];
	switch (mclk_mux_id) {
	case MCLK_MUX0:
		ret = priv->macro_params[mclk_mux0_macro].mclk_fn(
			priv->macro_params[mclk_mux0_macro].dev, enable);
		if (ret < 0) {
			dev_err_ratelimited(dev,
				"%s: MCLK_MUX0 %s failed for macro:%d, mclk_mux0_macro:%d\n",
				__func__,
				enable ? "enable" : "disable",
				macro_id, mclk_mux0_macro);
			goto err;
		}
		break;
	case MCLK_MUX1:
		mclk_mux1_macro =  bolero_mclk_mux_tbl[macro_id][MCLK_MUX1];
		ret = priv->macro_params[mclk_mux0_macro].mclk_fn(
			priv->macro_params[mclk_mux0_macro].dev,
			true);
		if (ret < 0) {
			dev_err_ratelimited(dev,
				"%s: MCLK_MUX0 en failed for macro:%d mclk_mux0_macro:%d\n",
				__func__, macro_id, mclk_mux0_macro);
			/*
			 * for disable case, need to proceed still for mclk_mux1
			 * counter to decrement
			 */
			if (enable)
				goto err;
		}
		/*
		 * need different return value as ret variable
		 * is used to track mclk_mux0 enable success or fail
		 */
		ret1 = priv->macro_params[mclk_mux1_macro].mclk_fn(
			priv->macro_params[mclk_mux1_macro].dev, enable);
		if (ret1 < 0)
			dev_err_ratelimited(dev,
				"%s: MCLK_MUX1 %s failed for macro:%d, mclk_mux1_macro:%d\n",
				__func__,
				enable ? "enable" : "disable",
				macro_id, mclk_mux1_macro);
		/* disable mclk_mux0 only if ret is success(0) */
		if (!ret)
			priv->macro_params[mclk_mux0_macro].mclk_fn(
				priv->macro_params[mclk_mux0_macro].dev,
				false);
		if (enable && ret1)
			goto err;
		break;
	case MCLK_MUX_MAX:
	default:
		dev_err(dev, "%s: invalid mclk_mux_id: %d\n",
			__func__, mclk_mux_id);
		ret = -EINVAL;
		goto err;
	}
	if (enable)
		priv->current_mclk_mux_macro[macro_id] =
				bolero_mclk_mux_tbl[macro_id][mclk_mux_id];
	else
		priv->current_mclk_mux_macro[macro_id] =
				bolero_mclk_mux_tbl[macro_id][MCLK_MUX0];
err:
	mutex_unlock(&priv->clk_lock);
	return ret;
}
EXPORT_SYMBOL(bolero_request_clock);

static ssize_t bolero_version_read(struct snd_info_entry *entry,
				   void *file_private_data,
				   struct file *file,
				   char __user *buf, size_t count,
				   loff_t pos)
{
	struct bolero_priv *priv;
	char buffer[BOLERO_VERSION_ENTRY_SIZE];
	int len = 0;

	priv = (struct bolero_priv *) entry->private_data;
	if (!priv) {
		pr_err("%s: bolero priv is null\n", __func__);
		return -EINVAL;
	}

	switch (priv->version) {
	case BOLERO_VERSION_1_0:
		len = snprintf(buffer, sizeof(buffer), "BOLERO_1_0\n");
		break;
	case BOLERO_VERSION_1_1:
		len = snprintf(buffer, sizeof(buffer), "BOLERO_1_1\n");
		break;
	case BOLERO_VERSION_1_2:
		len = snprintf(buffer, sizeof(buffer), "BOLERO_1_2\n");
		break;
	default:
		len = snprintf(buffer, sizeof(buffer), "VER_UNDEFINED\n");
	}

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static int bolero_ssr_enable(struct device *dev, void *data)
{
	struct bolero_priv *priv = data;
	int macro_idx;

	if (priv->initial_boot) {
		priv->initial_boot = false;
		return 0;
	}

	if (priv->macro_params[VA_MACRO].event_handler)
		priv->macro_params[VA_MACRO].event_handler(priv->codec,
			BOLERO_MACRO_EVT_WAIT_VA_CLK_RESET, 0x0);

	/* reset clock to force enable any clock disabled in ssr */
	for (macro_idx = START_MACRO; macro_idx < MAX_MACRO; macro_idx++) {
		if (!priv->macro_params[macro_idx].event_handler)
			continue;
		priv->macro_params[macro_idx].event_handler(priv->codec,
			BOLERO_MACRO_EVT_CLK_RESET, 0x0);
	}
	regcache_cache_only(priv->regmap, false);
	/* call ssr event for supported macros */
	for (macro_idx = START_MACRO; macro_idx < MAX_MACRO; macro_idx++) {
		if (!priv->macro_params[macro_idx].event_handler)
			continue;
		priv->macro_params[macro_idx].event_handler(priv->codec,
			BOLERO_MACRO_EVT_SSR_UP, 0x0);
	}
	mutex_lock(&priv->clk_lock);
	priv->dev_up = true;
	mutex_unlock(&priv->clk_lock);
	bolero_cdc_notifier_call(priv, BOLERO_WCD_EVT_SSR_UP);
	return 0;
}

static void bolero_ssr_disable(struct device *dev, void *data)
{
	struct bolero_priv *priv = data;
	int macro_idx;

	bolero_cdc_notifier_call(priv, BOLERO_WCD_EVT_PA_OFF_PRE_SSR);
	regcache_cache_only(priv->regmap, true);

	mutex_lock(&priv->clk_lock);
	priv->dev_up = false;
	mutex_unlock(&priv->clk_lock);
	/* call ssr event for supported macros */
	for (macro_idx = START_MACRO; macro_idx < MAX_MACRO; macro_idx++) {
		if (!priv->macro_params[macro_idx].event_handler)
			continue;
		priv->macro_params[macro_idx].event_handler(priv->codec,
			BOLERO_MACRO_EVT_SSR_DOWN, 0x0);
	}
	bolero_cdc_notifier_call(priv, BOLERO_WCD_EVT_SSR_DOWN);
}

static struct snd_info_entry_ops bolero_info_ops = {
	.read = bolero_version_read,
};

static const struct snd_event_ops bolero_ssr_ops = {
	.enable = bolero_ssr_enable,
	.disable = bolero_ssr_disable,
};

/*
 * bolero_info_create_codec_entry - creates bolero module
 * @codec_root: The parent directory
 * @codec: Codec instance
 *
 * Creates bolero module and version entry under the given
 * parent directory.
 *
 * Return: 0 on success or negative error code on failure.
 */
int bolero_info_create_codec_entry(struct snd_info_entry *codec_root,
				   struct snd_soc_codec *codec)
{
	struct snd_info_entry *version_entry;
	struct bolero_priv *priv;
	struct snd_soc_card *card;

	if (!codec_root || !codec)
		return -EINVAL;

	priv = snd_soc_codec_get_drvdata(codec);
	if (priv->entry) {
		dev_dbg(priv->dev,
			"%s:bolero module already created\n", __func__);
		return 0;
	}
	card = codec->component.card;
	priv->entry = snd_info_create_subdir(codec_root->module,
					     "bolero", codec_root);
	if (!priv->entry) {
		dev_dbg(codec->dev, "%s: failed to create bolero entry\n",
			__func__);
		return -ENOMEM;
	}
	version_entry = snd_info_create_card_entry(card->snd_card,
						   "version",
						   priv->entry);
	if (!version_entry) {
		dev_err(codec->dev, "%s: failed to create bolero version entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry->private_data = priv;
	version_entry->size = BOLERO_VERSION_ENTRY_SIZE;
	version_entry->content = SNDRV_INFO_CONTENT_DATA;
	version_entry->c.ops = &bolero_info_ops;

	if (snd_info_register(version_entry) < 0) {
		snd_info_free_entry(version_entry);
		return -ENOMEM;
	}
	priv->version_entry = version_entry;

	return 0;
}
EXPORT_SYMBOL(bolero_info_create_codec_entry);

/**
 * bolero_register_wake_irq - Register wake irq of Tx macro
 *
 * @codec: codec ptr.
 * @ipc_wakeup: bool to identify ipc_wakeup to be used or HW interrupt line.
 *
 * Return: 0 on success or negative error code on failure.
 */
int bolero_register_wake_irq(struct snd_soc_codec *codec, u32 ipc_wakeup)
{
	struct bolero_priv *priv = NULL;

	if (!codec)
		return -EINVAL;

	priv = snd_soc_codec_get_drvdata(codec);
	if (!priv)
		return -EINVAL;

	if (!bolero_is_valid_codec_dev(priv->dev)) {
		dev_err(codec->dev, "%s: invalid codec\n", __func__);
		return -EINVAL;
	}

	if (priv->macro_params[TX_MACRO].reg_wake_irq)
		priv->macro_params[TX_MACRO].reg_wake_irq(codec, ipc_wakeup);

	return 0;
}
EXPORT_SYMBOL(bolero_register_wake_irq);

static int bolero_soc_codec_probe(struct snd_soc_codec *codec)
{
	struct bolero_priv *priv = dev_get_drvdata(codec->dev);
	int macro_idx, ret = 0;

	/* call init for supported macros */
	for (macro_idx = START_MACRO; macro_idx < MAX_MACRO; macro_idx++) {
		if (priv->macro_params[macro_idx].init) {
			ret = priv->macro_params[macro_idx].init(codec);
			if (ret < 0) {
				dev_err(codec->dev,
					"%s: init for macro %d failed\n",
					__func__, macro_idx);
				goto err;
			}
		}
	}
	priv->codec = codec;
	/*
	 * In order for the ADIE RTC to differentiate between targets
	 * version info is used.
	 * Assign 1.0 for target with only one macro
	 * Assign 1.1 for target with two macros
	 * Assign 1.2 for target with more than two macros
	 */
	if (priv->num_macros_registered == 1)
		priv->version = BOLERO_VERSION_1_0;
	else if (priv->num_macros_registered == 2)
		priv->version = BOLERO_VERSION_1_1;
	else if (priv->num_macros_registered > 2)
		priv->version = BOLERO_VERSION_1_2;

	ret = snd_event_client_register(priv->dev, &bolero_ssr_ops, priv);
	if (!ret) {
		snd_event_notify(priv->dev, SND_EVENT_UP);
	} else {
		dev_err(codec->dev,
			"%s: Registration with SND event FWK failed ret = %d\n",
			__func__, ret);
		goto err;
	}

	dev_dbg(codec->dev, "%s: bolero soc codec probe success\n", __func__);
err:
	return ret;
}

static int bolero_soc_codec_remove(struct snd_soc_codec *codec)
{
	struct bolero_priv *priv = dev_get_drvdata(codec->dev);
	int macro_idx;

	snd_event_client_deregister(priv->dev);
	/* call exit for supported macros */
	for (macro_idx = START_MACRO; macro_idx < MAX_MACRO; macro_idx++)
		if (priv->macro_params[macro_idx].exit)
			priv->macro_params[macro_idx].exit(codec);

	return 0;
}

static struct regmap *bolero_get_regmap(struct device *dev)
{
	struct bolero_priv *priv = dev_get_drvdata(dev);

	return priv->regmap;
}

static struct snd_soc_codec_driver bolero = {
	.probe = bolero_soc_codec_probe,
	.remove = bolero_soc_codec_remove,
	.get_regmap = bolero_get_regmap,
};

static void bolero_add_child_devices(struct work_struct *work)
{
	struct bolero_priv *priv;
	bool wcd937x_node = false;
	struct platform_device *pdev;
	struct device_node *node;
	int ret = 0, count = 0;
	struct wcd_ctrl_platform_data *platdata = NULL;
	char plat_dev_name[BOLERO_CDC_STRING_LEN] = "";

	priv = container_of(work, struct bolero_priv,
			    bolero_add_child_devices_work);
	if (!priv) {
		pr_err("%s: Memory for bolero priv does not exist\n",
			__func__);
		return;
	}
	if (!priv->dev || !priv->dev->of_node) {
		dev_err(priv->dev, "%s: DT node for bolero does not exist\n",
			__func__);
		return;
	}

	platdata = &priv->plat_data;
	priv->child_count = 0;

	for_each_available_child_of_node(priv->dev->of_node, node) {
		wcd937x_node = false;
		if (strnstr(node->name, "wcd937x", strlen("wcd937x")) != NULL)
			wcd937x_node = true;

		strlcpy(plat_dev_name, node->name,
				(BOLERO_CDC_STRING_LEN - 1));

		pdev = platform_device_alloc(plat_dev_name, -1);
		if (!pdev) {
			dev_err(priv->dev, "%s: pdev memory alloc failed\n",
				__func__);
			ret = -ENOMEM;
			goto err;
		}
		pdev->dev.parent = priv->dev;
		pdev->dev.of_node = node;

		if (wcd937x_node) {
			priv->dev->platform_data = platdata;
			priv->wcd_dev = &pdev->dev;
		}

		ret = platform_device_add(pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: Cannot add platform device\n",
				__func__);
			platform_device_put(pdev);
			goto fail_pdev_add;
		}

		if (priv->child_count < BOLERO_CDC_CHILD_DEVICES_MAX)
			priv->pdev_child_devices[priv->child_count++] = pdev;
		else
			goto err;
	}
	return;
fail_pdev_add:
	for (count = 0; count < priv->child_count; count++)
		platform_device_put(priv->pdev_child_devices[count]);
err:
	return;
}

static int bolero_probe(struct platform_device *pdev)
{
	struct bolero_priv *priv;
	u32 num_macros = 0;
	int ret;
	u32 slew_reg1 = 0, slew_reg2 = 0;
	u32 slew_val1 = 0, slew_val2 = 0;
	char __iomem *slew_io_base1 = NULL, *slew_io_base2 = NULL;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct bolero_priv),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	ret = of_property_read_u32(pdev->dev.of_node, "qcom,num-macros",
				   &num_macros);
	if (ret) {
		dev_err(&pdev->dev,
			"%s:num-macros property not found\n",
			__func__);
		return ret;
	}
	priv->num_macros = num_macros;
	if (priv->num_macros > MAX_MACRO) {
		dev_err(&pdev->dev,
			"%s:num_macros(%d) > MAX_MACRO(%d) than supported\n",
			__func__, priv->num_macros, MAX_MACRO);
		return -EINVAL;
	}
	priv->va_without_decimation = of_property_read_bool(pdev->dev.of_node,
						"qcom,va-without-decimation");
	if (priv->va_without_decimation)
		bolero_reg_access[VA_MACRO] = bolero_va_top_reg_access;

	priv->dev = &pdev->dev;
	priv->dev_up = true;
	priv->initial_boot = true;
	priv->regmap = bolero_regmap_init(priv->dev,
					  &bolero_regmap_config);
	if (IS_ERR_OR_NULL((void *)(priv->regmap))) {
		dev_err(&pdev->dev, "%s:regmap init failed\n", __func__);
		return -EINVAL;
	}
	priv->read_dev = __bolero_reg_read;
	priv->write_dev = __bolero_reg_write;

	priv->plat_data.handle = (void *) priv;
	priv->plat_data.update_wcd_event = bolero_cdc_update_wcd_event;
	priv->plat_data.register_notifier = bolero_cdc_register_notifier;

	dev_set_drvdata(&pdev->dev, priv);
	mutex_init(&priv->io_lock);
	mutex_init(&priv->clk_lock);

	ret = of_property_read_u32(pdev->dev.of_node, "slew_rate_reg1",
				   &slew_reg1);

	ret |= of_property_read_u32(pdev->dev.of_node, "slew_rate_val1",
				   &slew_val1);
	if (!ret) {
		slew_io_base1 = devm_ioremap(&pdev->dev, slew_reg1, 0x4);
		if (!slew_io_base1) {
			dev_err(&pdev->dev, "%s: ioremap failed for slew reg 1\n",
				__func__);
			return -ENOMEM;
		}
		/* update slew rate for tx/rx swr interface */
		iowrite32(slew_val1, slew_io_base1);
	}
	ret = of_property_read_u32(pdev->dev.of_node, "slew_rate_reg2",
				   &slew_reg2);

	ret |= of_property_read_u32(pdev->dev.of_node, "slew_rate_val2",
				   &slew_val2);

	if (!ret) {
		slew_io_base2 = devm_ioremap(&pdev->dev, slew_reg2, 0x4);
		if (!slew_io_base2) {
			dev_err(&pdev->dev, "%s: ioremap failed for slew reg 2\n",
				__func__);
			return -ENOMEM;
		}
		/* update slew rate for tx/rx swr interface */
		iowrite32(slew_val2, slew_io_base2);
	}
	INIT_WORK(&priv->bolero_add_child_devices_work,
		  bolero_add_child_devices);
	schedule_work(&priv->bolero_add_child_devices_work);

	return 0;
}

static int bolero_remove(struct platform_device *pdev)
{
	struct bolero_priv *priv = dev_get_drvdata(&pdev->dev);

	if (!priv)
		return -EINVAL;

	of_platform_depopulate(&pdev->dev);
	mutex_destroy(&priv->io_lock);
	mutex_destroy(&priv->clk_lock);
	return 0;
}

static const struct of_device_id bolero_dt_match[] = {
	{.compatible = "qcom,bolero-codec"},
	{}
};
MODULE_DEVICE_TABLE(of, bolero_dt_match);

static struct platform_driver bolero_drv = {
	.driver = {
		.name = "bolero-codec",
		.owner = THIS_MODULE,
		.of_match_table = bolero_dt_match,
	},
	.probe = bolero_probe,
	.remove = bolero_remove,
};

module_platform_driver(bolero_drv);

MODULE_DESCRIPTION("Bolero driver");
MODULE_LICENSE("GPL v2");
