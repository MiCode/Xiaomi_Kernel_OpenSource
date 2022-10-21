// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/soc/qcom/fsa4480-i2c.h>
#include <linux/usb/usbpd.h>
#include <linux/delay.h>

#include "dp_aux.h"
#include "dp_debug.h"

#define DP_AUX_ENUM_STR(x)		#x

enum {
	DP_AUX_DATA_INDEX_WRITE = BIT(31),
};

struct dp_aux_private {
	struct device *dev;
	struct dp_aux dp_aux;
	struct dp_catalog_aux *catalog;
	struct dp_aux_cfg *cfg;
	struct device_node *aux_switch_node;
	struct mutex mutex;
	struct completion comp;
	struct drm_dp_aux drm_aux;

	bool cmd_busy;
	bool native;
	bool read;
	bool no_send_addr;
	bool no_send_stop;
	bool enabled;

	u32 offset;
	u32 segment;
	u32 aux_error_num;
	u32 retry_cnt;

	atomic_t aborted;

	u8 *dpcd;
	u8 *edid;
};

#ifdef CONFIG_DYNAMIC_DEBUG
static void dp_aux_hex_dump(struct drm_dp_aux *drm_aux,
		struct drm_dp_aux_msg *msg)
{
	char prefix[64];
	int i, linelen, remaining = msg->size;
	const int rowsize = 16;
	u8 linebuf[64];
	struct dp_aux_private *aux = container_of(drm_aux,
		struct dp_aux_private, drm_aux);

	snprintf(prefix, sizeof(prefix), "%s %s %4xh(%2zu): ",
		aux->native ? "NAT" : "I2C",
		aux->read ? "RD" : "WR",
		msg->address, msg->size);

	for (i = 0; i < msg->size; i += rowsize) {
		linelen = min(remaining, rowsize);
		remaining -= rowsize;

		hex_dump_to_buffer(msg->buffer + i, linelen, rowsize, 1,
			linebuf, sizeof(linebuf), false);

		DP_DEBUG("%s%s\n", prefix, linebuf);
	}
}
#else
static void dp_aux_hex_dump(struct drm_dp_aux *drm_aux,
		struct drm_dp_aux_msg *msg)
{
}
#endif

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
		DP_ERR("buf len error\n");
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

	aux->catalog->clear_trans(aux->catalog, false);
	aux->catalog->clear_hw_interrupts(aux->catalog);

	reg = 0; /* Transaction number == 1 */
	if (!aux->native) { /* i2c */
		reg |= BIT(8);

		if (aux->no_send_addr)
			reg |= BIT(10);

		if (aux->no_send_stop)
			reg |= BIT(11);
	}

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
		DP_ERR("DP AUX write failed\n");
		return -EINVAL;
	}

	timeout = wait_for_completion_timeout(&aux->comp, aux_timeout_ms);
	if (!timeout) {
		DP_ERR("aux %s timeout\n", (aux->read ? "read" : "write"));
		return -ETIMEDOUT;
	}

	if (aux->aux_error_num == DP_AUX_ERR_NONE) {
		ret = len;
	} else {
		pr_err_ratelimited("aux err: %s\n",
			dp_aux_get_error(aux->aux_error_num));

		ret = -EINVAL;
	}

	return ret;
}

static void dp_aux_cmd_fifo_rx(struct dp_aux_private *aux,
		struct drm_dp_aux_msg *msg)
{
	u32 data;
	u8 *dp;
	u32 i, actual_i;
	u32 len = msg->size;

	aux->catalog->clear_trans(aux->catalog, true);

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

		actual_i = (data >> 16) & 0xFF;
		if (i != actual_i)
			DP_WARN("Index mismatch: expected %d, found %d\n",
				i, actual_i);
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
	if (isr & DP_INTR_AUX_ERROR) {
		aux->aux_error_num = DP_AUX_ERR_PHY;
		aux->catalog->clear_hw_interrupts(aux->catalog);
	}

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
		if (isr & DP_INTR_AUX_ERROR) {
			aux->aux_error_num = DP_AUX_ERR_PHY;
			aux->catalog->clear_hw_interrupts(aux->catalog);
		}
	}

	complete(&aux->comp);
}

static void dp_aux_isr(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		DP_ERR("invalid input\n");
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

static void dp_aux_reconfig(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		DP_ERR("invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	aux->catalog->update_aux_cfg(aux->catalog,
			aux->cfg, PHY_AUX_CFG1);
	aux->catalog->reset(aux->catalog);
}

static void dp_aux_abort_transaction(struct dp_aux *dp_aux, bool abort)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		DP_ERR("invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	atomic_set(&aux->aborted, abort);
}

static void dp_aux_update_offset_and_segment(struct dp_aux_private *aux,
		struct drm_dp_aux_msg *input_msg)
{
	u32 const edid_address = 0x50;
	u32 const segment_address = 0x30;
	bool i2c_read = input_msg->request &
		(DP_AUX_I2C_READ & DP_AUX_NATIVE_READ);
	u8 *data = NULL;

	if (aux->native || i2c_read || ((input_msg->address != edid_address) &&
		(input_msg->address != segment_address)))
		return;


	data = input_msg->buffer;
	if (input_msg->address == segment_address)
		aux->segment = *data;
	else
		aux->offset = *data;
}

/**
 * dp_aux_transfer_helper() - helper function for EDID read transactions
 *
 * @aux: DP AUX private structure
 * @input_msg: input message from DRM upstream APIs
 * @send_seg: send the seg to sink
 *
 * return: void
 *
 * This helper function is used to fix EDID reads for non-compliant
 * sinks that do not handle the i2c middle-of-transaction flag correctly.
 */
static void dp_aux_transfer_helper(struct dp_aux_private *aux,
		struct drm_dp_aux_msg *input_msg, bool send_seg)
{
	struct drm_dp_aux_msg helper_msg;
	u32 const message_size = 0x10;
	u32 const segment_address = 0x30;
	u32 const edid_block_length = 0x80;
	bool i2c_mot = input_msg->request & DP_AUX_I2C_MOT;
	bool i2c_read = input_msg->request &
		(DP_AUX_I2C_READ & DP_AUX_NATIVE_READ);

	if (!i2c_mot || !i2c_read || (input_msg->size == 0))
		return;

	/*
	 * Sending the segment value and EDID offset will be performed
	 * from the DRM upstream EDID driver for each block. Avoid
	 * duplicate AUX transactions related to this while reading the
	 * first 16 bytes of each block.
	 */
	if (!(aux->offset % edid_block_length) || !send_seg)
		goto end;

	aux->read = false;
	aux->cmd_busy = true;
	aux->no_send_addr = true;
	aux->no_send_stop = true;

	/*
	 * Send the segment address for i2c reads for segment > 0 and for which
	 * the middle-of-transaction flag is set. This is required to support
	 * EDID reads of more than 2 blocks as the segment address is reset to 0
	 * since we are overriding the middle-of-transaction flag for read
	 * transactions.
	 */
	if (aux->segment) {
		memset(&helper_msg, 0, sizeof(helper_msg));
		helper_msg.address = segment_address;
		helper_msg.buffer = &aux->segment;
		helper_msg.size = 1;
		dp_aux_cmd_fifo_tx(aux, &helper_msg);
	}

	/*
	 * Send the offset address for every i2c read in which the
	 * middle-of-transaction flag is set. This will ensure that the sink
	 * will update its read pointer and return the correct portion of the
	 * EDID buffer in the subsequent i2c read trasntion triggered in the
	 * native AUX transfer function.
	 */
	memset(&helper_msg, 0, sizeof(helper_msg));
	helper_msg.address = input_msg->address;
	helper_msg.buffer = &aux->offset;
	helper_msg.size = 1;
	dp_aux_cmd_fifo_tx(aux, &helper_msg);
end:
	aux->offset += message_size;
	if (aux->offset == 0x80 || aux->offset == 0x100)
		aux->segment = 0x0; /* reset segment at end of block */
}

static int dp_aux_transfer_ready(struct dp_aux_private *aux,
		struct drm_dp_aux_msg *msg, bool send_seg)
{
	int ret = 0;
	int const aux_cmd_native_max = 16;
	int const aux_cmd_i2c_max = 128;

	if (atomic_read(&aux->aborted)) {
		ret = -ETIMEDOUT;
		goto error;
	}

	aux->native = msg->request & (DP_AUX_NATIVE_WRITE & DP_AUX_NATIVE_READ);

	/* Ignore address only message */
	if ((msg->size == 0) || (msg->buffer == NULL)) {
		msg->reply = aux->native ?
			DP_AUX_NATIVE_REPLY_ACK : DP_AUX_I2C_REPLY_ACK;
		goto error;
	}

	/* msg sanity check */
	if ((aux->native && (msg->size > aux_cmd_native_max)) ||
		(msg->size > aux_cmd_i2c_max)) {
		DP_ERR("%s: invalid msg: size(%zu), request(%x)\n",
			__func__, msg->size, msg->request);
		ret = -EINVAL;
		goto error;
	}

	dp_aux_update_offset_and_segment(aux, msg);

	dp_aux_transfer_helper(aux, msg, send_seg);

	aux->read = msg->request & (DP_AUX_I2C_READ & DP_AUX_NATIVE_READ);

	if (aux->read) {
		aux->no_send_addr = true;
		aux->no_send_stop = false;
	} else {
		aux->no_send_addr = true;
		aux->no_send_stop = true;
	}

	aux->cmd_busy = true;
error:
	return ret;
}

static ssize_t dp_aux_transfer_debug(struct drm_dp_aux *drm_aux,
		struct drm_dp_aux_msg *msg)
{
	u32 timeout;
	ssize_t ret;
	struct dp_aux_private *aux = container_of(drm_aux,
		struct dp_aux_private, drm_aux);

	mutex_lock(&aux->mutex);

	ret = dp_aux_transfer_ready(aux, msg, false);
	if (ret)
		goto end;

	aux->aux_error_num = DP_AUX_ERR_NONE;

	if (!aux->dpcd || !aux->edid) {
		DP_ERR("invalid aux/dpcd structure\n");
		goto end;
	}

	if ((msg->address + msg->size) > SZ_4K) {
		DP_DEBUG("invalid dpcd access: addr=0x%x, size=0x%lx\n",
				msg->address, msg->size);
		goto address_error;
	}

	if (aux->native) {
		mutex_lock(aux->dp_aux.access_lock);
		aux->dp_aux.reg = msg->address;
		aux->dp_aux.read = aux->read;
		aux->dp_aux.size = msg->size;

		if (!aux->read)
			memcpy(aux->dpcd + msg->address,
				msg->buffer, msg->size);

		reinit_completion(&aux->comp);
		mutex_unlock(aux->dp_aux.access_lock);

		timeout = wait_for_completion_timeout(&aux->comp, HZ * 2);
		if (!timeout) {
			DP_ERR("%s timeout: 0x%x\n",
				aux->read ? "read" : "write",
				msg->address);
			atomic_set(&aux->aborted, 1);
			ret = -ETIMEDOUT;
			goto end;
		}

		mutex_lock(aux->dp_aux.access_lock);
		if (aux->read)
			memcpy(msg->buffer, aux->dpcd + msg->address,
				msg->size);
		mutex_unlock(aux->dp_aux.access_lock);

		aux->aux_error_num = DP_AUX_ERR_NONE;
	} else {
		if (aux->read && msg->address == 0x50) {
			memcpy(msg->buffer,
				aux->edid + aux->offset - 16,
				msg->size);
		}
	}

	if (aux->aux_error_num == DP_AUX_ERR_NONE) {
		dp_aux_hex_dump(drm_aux, msg);

		if (!aux->read)
			memset(msg->buffer, 0, msg->size);

		msg->reply = aux->native ?
			DP_AUX_NATIVE_REPLY_ACK : DP_AUX_I2C_REPLY_ACK;
	} else {
		/* Reply defer to retry */
		msg->reply = aux->native ?
			DP_AUX_NATIVE_REPLY_DEFER : DP_AUX_I2C_REPLY_DEFER;
	}

	ret = msg->size;
	goto end;

address_error:
	memset(msg->buffer, 0, msg->size);
	ret = msg->size;
end:
	if (ret == -ETIMEDOUT)
		aux->dp_aux.state |= DP_STATE_AUX_TIMEOUT;
	aux->dp_aux.reg = 0xFFFF;
	aux->dp_aux.read = true;
	aux->dp_aux.size = 0;

	mutex_unlock(&aux->mutex);
	return ret;
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
	int const retry_count = 5;
	struct dp_aux_private *aux = container_of(drm_aux,
		struct dp_aux_private, drm_aux);

	mutex_lock(&aux->mutex);

	ret = dp_aux_transfer_ready(aux, msg, true);
	if (ret)
		goto unlock_exit;

	if (!aux->cmd_busy) {
		ret = msg->size;
		goto unlock_exit;
	}

	ret = dp_aux_cmd_fifo_tx(aux, msg);
	if ((ret < 0) && !atomic_read(&aux->aborted)) {
		aux->retry_cnt++;
		if (!(aux->retry_cnt % retry_count))
			aux->catalog->update_aux_cfg(aux->catalog,
				aux->cfg, PHY_AUX_CFG1);
		aux->catalog->reset(aux->catalog);
		goto unlock_exit;
	} else if (ret < 0) {
		goto unlock_exit;
	}

	if (aux->aux_error_num == DP_AUX_ERR_NONE) {
		if (aux->read)
			dp_aux_cmd_fifo_rx(aux, msg);

		dp_aux_hex_dump(drm_aux, msg);

		msg->reply = aux->native ?
			DP_AUX_NATIVE_REPLY_ACK : DP_AUX_I2C_REPLY_ACK;
	} else {
		/* Reply defer to retry */
		msg->reply = aux->native ?
			DP_AUX_NATIVE_REPLY_DEFER : DP_AUX_I2C_REPLY_DEFER;
	}

	/* Return requested size for success or retry */
	ret = msg->size;
	aux->retry_cnt = 0;

unlock_exit:
	aux->cmd_busy = false;
	mutex_unlock(&aux->mutex);
	return ret;
}

static void dp_aux_reset_phy_config_indices(struct dp_aux_cfg *aux_cfg)
{
	int i = 0;

	for (i = 0; i < PHY_AUX_CFG_MAX; i++)
		aux_cfg[i].current_index = 0;
}

static void dp_aux_init(struct dp_aux *dp_aux, struct dp_aux_cfg *aux_cfg)
{
	struct dp_aux_private *aux;

	if (!dp_aux || !aux_cfg) {
		DP_ERR("invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	if (aux->enabled)
		return;

	dp_aux_reset_phy_config_indices(aux_cfg);
	aux->catalog->setup(aux->catalog, aux_cfg);
	aux->catalog->reset(aux->catalog);
	aux->catalog->enable(aux->catalog, true);
	atomic_set(&aux->aborted, 0);
	aux->retry_cnt = 0;
	aux->enabled = true;
}

static void dp_aux_deinit(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		DP_ERR("invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	if (!aux->enabled)
		return;

	atomic_set(&aux->aborted, 1);
	aux->catalog->enable(aux->catalog, false);
	aux->enabled = false;
}

static int dp_aux_register(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;
	int ret = 0;

	if (!dp_aux) {
		DP_ERR("invalid input\n");
		ret = -EINVAL;
		goto exit;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	aux->drm_aux.name = "sde_dp_aux";
	aux->drm_aux.dev = aux->dev;
	aux->drm_aux.transfer = dp_aux_transfer;
	ret = drm_dp_aux_register(&aux->drm_aux);
	if (ret) {
		DP_ERR("%s: failed to register drm aux: %d\n", __func__, ret);
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
		DP_ERR("invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);
	drm_dp_aux_unregister(&aux->drm_aux);
}

static void dp_aux_dpcd_updated(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		DP_ERR("invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	/* make sure wait has started */
	usleep_range(20, 30);
	complete(&aux->comp);
}

static void dp_aux_set_sim_mode(struct dp_aux *dp_aux, bool en,
		u8 *edid, u8 *dpcd)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		DP_ERR("invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	mutex_lock(&aux->mutex);

	aux->edid = edid;
	aux->dpcd = dpcd;

	if (en) {
		atomic_set(&aux->aborted, 0);
		aux->drm_aux.transfer = dp_aux_transfer_debug;
	} else {
		aux->drm_aux.transfer = dp_aux_transfer;
	}

	mutex_unlock(&aux->mutex);
}

static int dp_aux_configure_aux_switch(struct dp_aux *dp_aux,
		bool enable, int orientation)
{
	struct dp_aux_private *aux;
	int rc = 0;
	enum fsa_function event = FSA_USBC_DISPLAYPORT_DISCONNECTED;

	if (!dp_aux) {
		DP_ERR("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	if (!aux->aux_switch_node) {
		DP_DEBUG("undefined fsa4480 handle\n");
		rc = -EINVAL;
		goto end;
	}

	if (enable) {
		switch (orientation) {
		case ORIENTATION_CC1:
			event = FSA_USBC_ORIENTATION_CC1;
			break;
		case ORIENTATION_CC2:
			event = FSA_USBC_ORIENTATION_CC2;
			break;
		default:
			DP_ERR("invalid orientation\n");
			rc = -EINVAL;
			goto end;
		}
	}

	DP_DEBUG("enable=%d, orientation=%d, event=%d\n",
			enable, orientation, event);

	rc = fsa4480_switch_event(aux->aux_switch_node, event);
	if (rc)
		DP_ERR("failed to configure fsa4480 i2c device (%d)\n", rc);
end:
	return rc;
}

struct dp_aux *dp_aux_get(struct device *dev, struct dp_catalog_aux *catalog,
		struct dp_parser *parser, struct device_node *aux_switch)
{
	int rc = 0;
	struct dp_aux_private *aux;
	struct dp_aux *dp_aux;

	if (!catalog || !parser ||
			(!parser->no_aux_switch &&
				!aux_switch &&
				!parser->gpio_aux_switch)) {
		DP_ERR("invalid input\n");
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
	aux->cfg = parser->aux_cfg;
	aux->aux_switch_node = aux_switch;
	dp_aux = &aux->dp_aux;
	aux->retry_cnt = 0;
	aux->dp_aux.reg = 0xFFFF;

	dp_aux->isr     = dp_aux_isr;
	dp_aux->init    = dp_aux_init;
	dp_aux->deinit  = dp_aux_deinit;
	dp_aux->drm_aux_register = dp_aux_register;
	dp_aux->drm_aux_deregister = dp_aux_deregister;
	dp_aux->reconfig = dp_aux_reconfig;
	dp_aux->abort = dp_aux_abort_transaction;
	dp_aux->dpcd_updated = dp_aux_dpcd_updated;
	dp_aux->set_sim_mode = dp_aux_set_sim_mode;
	dp_aux->aux_switch = dp_aux_configure_aux_switch;

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

	mutex_destroy(&aux->mutex);

	devm_kfree(aux->dev, aux);
}
