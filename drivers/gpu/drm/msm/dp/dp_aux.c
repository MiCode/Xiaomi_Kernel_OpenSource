/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include <linux/delay.h>

#include "dp_aux.h"

#define DP_AUX_ENUM_STR(x)		#x

enum {
	DP_AUX_DATA_INDEX_WRITE = BIT(31),
};

struct dp_aux_private {
	struct device *dev;
	struct dp_aux dp_aux;
	struct dp_catalog_aux *catalog;

	struct mutex mutex;
	struct completion comp;

	u32 aux_error_num;
	bool cmd_busy;
	bool native;
	bool read;

	struct drm_dp_aux drm_aux;
};

static char *dp_aux_get_error(u32 aux_error)
{
	switch (aux_error) {
	case DP_AUX_ERR_NONE:
		return DP_AUX_ENUM_STR(DP_AUX_ERR_NONE);
	case DP_AUX_ERR_ADDR:
		return DP_AUX_ENUM_STR(DP_AUX_ERR_ADDR);
	case DP_AUX_ERR_TOUT:
		return DP_AUX_ENUM_STR(DP_AUX_ERR_TOUT);
	case DP_AUX_ERR_NACK:
		return DP_AUX_ENUM_STR(DP_AUX_ERR_NACK);
	case DP_AUX_ERR_DEFER:
		return DP_AUX_ENUM_STR(DP_AUX_ERR_DEFER);
	case DP_AUX_ERR_NACK_DEFER:
		return DP_AUX_ENUM_STR(DP_AUX_ERR_NACK_DEFER);
	default:
		return "unknown";
	}
}

static u32 dp_aux_write(struct dp_aux_private *aux,
		struct drm_dp_aux_msg *msg)
{
	u32 data[4], reg, len;
	u8 *msgdata = msg->buffer;
	int const aux_cmd_fifo_len = 128;
	int i = 0;

	if (aux->read)
		len = 4;
	else
		len = msg->size + 4;

	/*
	 * cmd fifo only has depth of 144 bytes
	 * limit buf length to 128 bytes here
	 */
	if (len > aux_cmd_fifo_len) {
		pr_err("buf len error\n");
		return 0;
	}

	/* Pack cmd and write to HW */
	data[0] = (msg->address >> 16) & 0xf; /* addr[19:16] */
	if (aux->read)
		data[0] |=  BIT(4); /* R/W */

	data[1] = (msg->address >> 8) & 0xff;	/* addr[15:8] */
	data[2] = msg->address & 0xff;		/* addr[7:0] */
	data[3] = (msg->size - 1) & 0xff;	/* len[7:0] */

	for (i = 0; i < len; i++) {
		reg = (i < 4) ? data[i] : msgdata[i - 4];
		reg = ((reg) << 8) & 0x0000ff00; /* index = 0, write */
		if (i == 0)
			reg |= DP_AUX_DATA_INDEX_WRITE;
		aux->catalog->data = reg;
		aux->catalog->write_data(aux->catalog);
	}

	reg = 0; /* Transaction number == 1 */
	if (!aux->native) /* i2c */
		reg |= (BIT(8) | BIT(10) | BIT(11));

	reg |= BIT(9);
	aux->catalog->data = reg;
	aux->catalog->write_trans(aux->catalog);

	return len;
}

static int dp_aux_cmd_fifo_tx(struct dp_aux_private *aux,
		struct drm_dp_aux_msg *msg)
{
	u32 ret = 0, len = 0, timeout;
	int const aux_timeout_ms = HZ/4;

	reinit_completion(&aux->comp);

	len = dp_aux_write(aux, msg);
	if (len == 0) {
		pr_err("DP AUX write failed\n");
		return -EINVAL;
	}

	timeout = wait_for_completion_timeout(&aux->comp, aux_timeout_ms);
	if (!timeout) {
		pr_err("aux write timeout\n");
		return -ETIMEDOUT;
	}

	pr_debug("aux status %s\n",
		dp_aux_get_error(aux->aux_error_num));

	if (aux->aux_error_num == DP_AUX_ERR_NONE)
		ret = len;
	else
		ret = -EINVAL;

	return ret;
}

static void dp_aux_cmd_fifo_rx(struct dp_aux_private *aux,
		struct drm_dp_aux_msg *msg)
{
	u32 data;
	u8 *dp;
	u32 i;
	u32 len = msg->size;

	data = 0;
	data |= DP_AUX_DATA_INDEX_WRITE; /* INDEX_WRITE */
	data |= BIT(0);  /* read */

	aux->catalog->data = data;
	aux->catalog->write_data(aux->catalog);

	dp = msg->buffer;

	/* discard first byte */
	data = aux->catalog->read_data(aux->catalog);

	for (i = 0; i < len; i++) {
		data = aux->catalog->read_data(aux->catalog);
		*dp++ = (u8)((data >> 8) & 0xff);
	}
}

static void dp_aux_native_handler(struct dp_aux_private *aux)
{
	u32 isr = aux->catalog->isr;

	if (isr & DP_INTR_AUX_I2C_DONE)
		aux->aux_error_num = DP_AUX_ERR_NONE;
	else if (isr & DP_INTR_WRONG_ADDR)
		aux->aux_error_num = DP_AUX_ERR_ADDR;
	else if (isr & DP_INTR_TIMEOUT)
		aux->aux_error_num = DP_AUX_ERR_TOUT;
	if (isr & DP_INTR_NACK_DEFER)
		aux->aux_error_num = DP_AUX_ERR_NACK;

	complete(&aux->comp);
}

static void dp_aux_i2c_handler(struct dp_aux_private *aux)
{
	u32 isr = aux->catalog->isr;

	if (isr & DP_INTR_AUX_I2C_DONE) {
		if (isr & (DP_INTR_I2C_NACK | DP_INTR_I2C_DEFER))
			aux->aux_error_num = DP_AUX_ERR_NACK;
		else
			aux->aux_error_num = DP_AUX_ERR_NONE;
	} else {
		if (isr & DP_INTR_WRONG_ADDR)
			aux->aux_error_num = DP_AUX_ERR_ADDR;
		else if (isr & DP_INTR_TIMEOUT)
			aux->aux_error_num = DP_AUX_ERR_TOUT;
		if (isr & DP_INTR_NACK_DEFER)
			aux->aux_error_num = DP_AUX_ERR_NACK_DEFER;
		if (isr & DP_INTR_I2C_NACK)
			aux->aux_error_num = DP_AUX_ERR_NACK;
		if (isr & DP_INTR_I2C_DEFER)
			aux->aux_error_num = DP_AUX_ERR_DEFER;
	}

	complete(&aux->comp);
}

static void dp_aux_isr(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		pr_err("invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	aux->catalog->get_irq(aux->catalog, aux->cmd_busy);

	if (!aux->cmd_busy)
		return;

	if (aux->native)
		dp_aux_native_handler(aux);
	else
		dp_aux_i2c_handler(aux);
}

/*
 * This function does the real job to process an AUX transaction.
 * It will call aux_reset() function to reset the AUX channel,
 * if the waiting is timeout.
 */
static ssize_t dp_aux_transfer(struct drm_dp_aux *drm_aux,
		struct drm_dp_aux_msg *msg)
{
	ssize_t ret;
	int const aux_cmd_native_max = 16;
	int const aux_cmd_i2c_max = 128;
	struct dp_aux_private *aux = container_of(drm_aux,
		struct dp_aux_private, drm_aux);

	mutex_lock(&aux->mutex);

	aux->native = msg->request & (DP_AUX_NATIVE_WRITE & DP_AUX_NATIVE_READ);
	aux->read = msg->request & (DP_AUX_I2C_READ & DP_AUX_NATIVE_READ);
	aux->cmd_busy = true;

	/* Ignore address only message */
	if ((msg->size == 0) || (msg->buffer == NULL)) {
		msg->reply = aux->native ?
			DP_AUX_NATIVE_REPLY_ACK : DP_AUX_I2C_REPLY_ACK;
		ret = msg->size;
		goto unlock_exit;
	}

	/* msg sanity check */
	if ((aux->native && (msg->size > aux_cmd_native_max)) ||
		(msg->size > aux_cmd_i2c_max)) {
		pr_err("%s: invalid msg: size(%zu), request(%x)\n",
			__func__, msg->size, msg->request);
		ret = -EINVAL;
		goto unlock_exit;
	}

	ret = dp_aux_cmd_fifo_tx(aux, msg);
	if (ret < 0) {
		aux->catalog->reset(aux->catalog); /* reset aux */
		goto unlock_exit;
	}

	if (aux->aux_error_num == DP_AUX_ERR_NONE) {
		if (aux->read)
			dp_aux_cmd_fifo_rx(aux, msg);

		msg->reply = aux->native ?
			DP_AUX_NATIVE_REPLY_ACK : DP_AUX_I2C_REPLY_ACK;
	} else {
		/* Reply defer to retry */
		msg->reply = aux->native ?
			DP_AUX_NATIVE_REPLY_DEFER : DP_AUX_I2C_REPLY_DEFER;
	}

	/* Return requested size for success or retry */
	ret = msg->size;

unlock_exit:
	aux->cmd_busy = false;
	mutex_unlock(&aux->mutex);
	return ret;
}

static void dp_aux_init(struct dp_aux *dp_aux, u32 *aux_cfg)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		pr_err("invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	aux->catalog->reset(aux->catalog);
	aux->catalog->enable(aux->catalog, true);
	aux->catalog->setup(aux->catalog, aux_cfg);
}

static void dp_aux_deinit(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		pr_err("invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	aux->catalog->enable(aux->catalog, false);
}

static int dp_aux_register(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;
	int ret = 0;

	if (!dp_aux) {
		pr_err("invalid input\n");
		ret = -EINVAL;
		goto exit;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	aux->drm_aux.name = "sde_dp_aux";
	aux->drm_aux.dev = aux->dev;
	aux->drm_aux.transfer = dp_aux_transfer;
	ret = drm_dp_aux_register(&aux->drm_aux);
	if (ret) {
		pr_err("%s: failed to register drm aux: %d\n", __func__, ret);
		goto exit;
	}
	dp_aux->drm_aux = &aux->drm_aux;
exit:
	return ret;
}

static void dp_aux_deregister(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		pr_err("invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);
	drm_dp_aux_unregister(&aux->drm_aux);
}

struct dp_aux *dp_aux_get(struct device *dev, struct dp_catalog_aux *catalog)
{
	int rc = 0;
	struct dp_aux_private *aux;
	struct dp_aux *dp_aux;

	if (!catalog) {
		pr_err("invalid input\n");
		rc = -ENODEV;
		goto error;
	}

	aux = devm_kzalloc(dev, sizeof(*aux), GFP_KERNEL);
	if (!aux) {
		rc = -ENOMEM;
		goto error;
	}

	init_completion(&aux->comp);
	aux->cmd_busy = false;
	mutex_init(&aux->mutex);

	aux->dev = dev;
	aux->catalog = catalog;
	dp_aux = &aux->dp_aux;

	dp_aux->isr     = dp_aux_isr;
	dp_aux->init    = dp_aux_init;
	dp_aux->deinit  = dp_aux_deinit;
	dp_aux->drm_aux_register = dp_aux_register;
	dp_aux->drm_aux_deregister = dp_aux_deregister;

	return dp_aux;
error:
	return ERR_PTR(rc);
}

void dp_aux_put(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;

	if (!dp_aux)
		return;

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	devm_kfree(aux->dev, aux);
}
