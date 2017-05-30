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

struct aux_buf {
	u8 *start;      /* buffer start addr */
	u8 *end;	/* buffer end addr */
	u8 *data;       /* data pou32er */
	u32 size;       /* size of buffer */
	u32 len;	/* dara length */
	u8 trans_num;   /* transaction number */
	enum aux_tx_mode tx_mode;
};

struct dp_aux_private {
	struct device *dev;
	struct dp_aux dp_aux;
	struct dp_catalog_aux *catalog;

	struct mutex mutex;
	struct completion comp;

	struct aux_cmd *cmds;
	struct aux_buf txp;
	struct aux_buf rxp;

	u32 aux_error_num;
	bool cmd_busy;

	u8 txbuf[256];
	u8 rxbuf[256];
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

static void dp_aux_buf_init(struct aux_buf *buf, u8 *data, u32 size)
{
	buf->start     = data;
	buf->size      = size;
	buf->data      = buf->start;
	buf->end       = buf->start + buf->size;
	buf->len       = 0;
	buf->trans_num = 0;
	buf->tx_mode   = AUX_NATIVE;
}

static void dp_aux_buf_set(struct dp_aux_private *aux)
{
	init_completion(&aux->comp);
	aux->cmd_busy = false;
	mutex_init(&aux->mutex);

	dp_aux_buf_init(&aux->txp, aux->txbuf, sizeof(aux->txbuf));
	dp_aux_buf_init(&aux->rxp, aux->rxbuf, sizeof(aux->rxbuf));
}

static void dp_aux_buf_reset(struct aux_buf *buf)
{
	buf->data      = buf->start;
	buf->len       = 0;
	buf->trans_num = 0;
	buf->tx_mode   = AUX_NATIVE;

	memset(buf->start, 0x0, 256);
}

static void dp_aux_buf_push(struct aux_buf *buf, u32 len)
{
	buf->data += len;
	buf->len  += len;
}

static u32 dp_aux_buf_trailing(struct aux_buf *buf)
{
	return (u32)(buf->end - buf->data);
}

static u32 dp_aux_add_cmd(struct aux_buf *buf, struct aux_cmd *cmd)
{
	u8 data;
	u8 *bp, *cp;
	u32 i, len;

	if (cmd->ex_mode == AUX_READ)
		len = 4;
	else
		len = cmd->len + 4;

	if (dp_aux_buf_trailing(buf) < len) {
		pr_err("buf trailing error\n");
		return 0;
	}

	/*
	 * cmd fifo only has depth of 144 bytes
	 * limit buf length to 128 bytes here
	 */
	if ((buf->len + len) > 128) {
		pr_err("buf len error\n");
		return 0;
	}

	bp = buf->data;
	data = cmd->addr >> 16;
	data &= 0x0f;  /* 4 addr bits */

	if (cmd->ex_mode == AUX_READ)
		data |=  BIT(4);

	*bp++ = data;
	*bp++ = cmd->addr >> 8;
	*bp++ = cmd->addr;
	*bp++ = cmd->len - 1;

	if (cmd->ex_mode == AUX_WRITE) {
		cp = cmd->buf;

		for (i = 0; i < cmd->len; i++)
			*bp++ = *cp++;
	}

	dp_aux_buf_push(buf, len);

	buf->tx_mode = cmd->tx_mode;

	buf->trans_num++;

	return cmd->len - 1;
}

static u32 dp_aux_cmd_fifo_tx(struct dp_aux_private *aux)
{
	u8 *dp;
	u32 data, len, cnt;
	struct aux_buf *tp = &aux->txp;

	len = tp->len;
	if (len == 0) {
		pr_err("invalid len\n");
		return 0;
	}

	cnt = 0;
	dp = tp->start;

	while (cnt < len) {
		data = *dp;
		data <<= 8;
		data &= 0x00ff00;
		if (cnt == 0)
			data |= BIT(31);

		aux->catalog->data = data;
		aux->catalog->write_data(aux->catalog);

		cnt++;
		dp++;
	}

	data = (tp->trans_num - 1);
	if (tp->tx_mode == AUX_I2C) {
		data |= BIT(8); /* I2C */
		data |= BIT(10); /* NO SEND ADDR */
		data |= BIT(11); /* NO SEND STOP */
	}

	data |= BIT(9); /* GO */
	aux->catalog->data = data;
	aux->catalog->write_trans(aux->catalog);

	return tp->len;
}

static u32 dp_cmd_fifo_rx(struct dp_aux_private *aux, u32 len)
{
	u32 data;
	u8 *dp;
	u32 i;
	struct aux_buf *rp = &aux->rxp;

	data = 0;
	data |= BIT(31); /* INDEX_WRITE */
	data |= BIT(0);  /* read */

	aux->catalog->data = data;
	aux->catalog->write_data(aux->catalog);

	dp = rp->data;

	/* discard first byte */
	data = aux->catalog->read_data(aux->catalog);

	for (i = 0; i < len; i++) {
		data = aux->catalog->read_data(aux->catalog);
		*dp++ = (u8)((data >> 8) & 0xff);
	}

	rp->len = len;
	return len;
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

	if (aux->cmds->tx_mode == AUX_NATIVE)
		dp_aux_native_handler(aux);
	else
		dp_aux_i2c_handler(aux);
}



static int dp_aux_write(struct dp_aux_private *aux)
{
	struct aux_cmd *cm;
	struct aux_buf *tp;
	u32 len, ret, timeout;

	mutex_lock(&aux->mutex);

	tp = &aux->txp;
	dp_aux_buf_reset(tp);

	cm = aux->cmds;
	while (cm) {
		ret = dp_aux_add_cmd(tp, cm);
		if (ret <= 0)
			break;

		if (!cm->next)
			break;
		cm++;
	}

	reinit_completion(&aux->comp);
	aux->cmd_busy = true;

	len = dp_aux_cmd_fifo_tx(aux);

	timeout = wait_for_completion_timeout(&aux->comp, HZ/4);
	if (!timeout)
		pr_err("aux write timeout\n");

	pr_debug("aux status %s\n",
		dp_aux_get_error(aux->aux_error_num));

	if (aux->aux_error_num == DP_AUX_ERR_NONE)
		ret = len;
	else
		ret = aux->aux_error_num;

	aux->cmd_busy = false;
	mutex_unlock(&aux->mutex);
	return  ret;
}

static int dp_aux_read(struct dp_aux_private *aux)
{
	struct aux_cmd *cm;
	struct aux_buf *tp, *rp;
	u32 len, ret, timeout;

	mutex_lock(&aux->mutex);

	tp = &aux->txp;
	rp = &aux->rxp;

	dp_aux_buf_reset(tp);
	dp_aux_buf_reset(rp);

	cm = aux->cmds;
	len = 0;

	while (cm) {
		ret = dp_aux_add_cmd(tp, cm);
		len += cm->len;

		if (ret <= 0)
			break;

		if (!cm->next)
			break;
		cm++;
	}

	reinit_completion(&aux->comp);
	aux->cmd_busy = true;

	dp_aux_cmd_fifo_tx(aux);

	timeout = wait_for_completion_timeout(&aux->comp, HZ/4);
	if (!timeout)
		pr_err("aux read timeout\n");

	pr_debug("aux status %s\n",
		dp_aux_get_error(aux->aux_error_num));

	if (aux->aux_error_num == DP_AUX_ERR_NONE)
		ret = dp_cmd_fifo_rx(aux, len);
	else
		ret = aux->aux_error_num;

	aux->cmds->buf = rp->data;
	aux->cmd_busy = false;

	mutex_unlock(&aux->mutex);

	return ret;
}

static int dp_aux_write_ex(struct dp_aux *dp_aux, u32 addr, u32 len,
				enum aux_tx_mode mode, u8 *buf)
{
	struct aux_cmd cmd = {0};
	struct dp_aux_private *aux;

	if (!dp_aux || !len) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	cmd.ex_mode = AUX_WRITE;
	cmd.tx_mode = mode;
	cmd.addr    = addr;
	cmd.len     = len;
	cmd.buf     = buf;

	aux->cmds = &cmd;

	return dp_aux_write(aux);
}

static int dp_aux_read_ex(struct dp_aux *dp_aux, u32 addr, u32 len,
				enum aux_tx_mode mode, u8 **buf)
{
	int rc = 0;
	struct aux_cmd cmd = {0};
	struct dp_aux_private *aux;

	if (!dp_aux || !len) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	cmd.ex_mode = AUX_READ;
	cmd.tx_mode = mode;
	cmd.addr    = addr;
	cmd.len     = len;

	aux->cmds = &cmd;

	rc = dp_aux_read(aux);
	if (rc <= 0) {
		rc = -EINVAL;
		goto end;
	}

	*buf = cmd.buf;
end:
	return rc;
}

static int dp_aux_process(struct dp_aux *dp_aux, struct aux_cmd *cmds)
{
	struct dp_aux_private *aux;

	if (!dp_aux || !cmds) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	aux->cmds = cmds;

	if (cmds->ex_mode == AUX_READ)
		return dp_aux_read(aux);
	else
		return dp_aux_write(aux);
}

static bool dp_aux_ready(struct dp_aux *dp_aux)
{
	u8 data = 0;
	int count, ret;
	struct dp_aux_private *aux;

	if (!dp_aux) {
		pr_err("invalid input\n");
		goto error;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	for (count = 5; count; count--) {
		ret = dp_aux_write_ex(dp_aux, 0x50, 1, AUX_I2C, &data);
		if (ret >= 0)
			break;

		msleep(100);
	}

	if (count <= 0) {
		pr_err("aux chan NOT ready\n");
		goto error;
	}

	return true;
error:
	return false;
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

	aux->dev = dev;

	dp_aux_buf_set(aux);

	aux->catalog = catalog;

	dp_aux = &aux->dp_aux;

	dp_aux->process = dp_aux_process;
	dp_aux->read    = dp_aux_read_ex;
	dp_aux->write   = dp_aux_write_ex;
	dp_aux->ready   = dp_aux_ready;
	dp_aux->isr     = dp_aux_isr;
	dp_aux->init    = dp_aux_init;
	dp_aux->deinit  = dp_aux_deinit;

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
