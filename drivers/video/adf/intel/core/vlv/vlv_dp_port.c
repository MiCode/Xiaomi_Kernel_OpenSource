/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Created on 15 Sep 2014
 * Author: Sivakumar Thulasimani <sivakumar.thulasimani@intel.com>
 */

#include <linux/i2c.h>
#include <drm/i915_adf.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/vlv_dp_port.h>


#define BARE_ADDRESS_SIZE	3
#define HEADER_SIZE		(BARE_ADDRESS_SIZE + 1)

#define IDLE_ON_MASK		(PP_ON | PP_SEQUENCE_MASK | 0 \
						| PP_SEQUENCE_STATE_MASK)
#define IDLE_ON_VALUE		(PP_ON | PP_SEQUENCE_NONE | 0 \
						| PP_SEQUENCE_STATE_ON_IDLE)

#define IDLE_OFF_MASK		(PP_ON | PP_SEQUENCE_MASK | 0 | 0)
#define IDLE_OFF_VALUE		(0     | PP_SEQUENCE_NONE | 0 | 0)

#define IDLE_CYCLE_MASK		(PP_ON | PP_SEQUENCE_MASK |\
				PP_CYCLE_DELAY_ACTIVE | PP_SEQUENCE_STATE_MASK)
#define IDLE_CYCLE_VALUE	(0     | PP_SEQUENCE_NONE | 0 \
						| PP_SEQUENCE_STATE_OFF_IDLE)
#define MAXIMUM_BRIGHTNESS	100
#define PWM_CTL_DEFAULT		0x1592b00
#define PWM_DUTY_CYCLE		0x1e841e84
#define PLATFORM_MAX_BRIGHTNESS	0x1FFF
#define PWM_ENABLE		(1 << 31)

static u32 wait_panel_status(struct vlv_dp_port *port, u32 mask, u32 value)
{
	u32 pp_stat_reg, pp_ctrl_reg;
	u32 err = 0;

	pp_stat_reg = port->pp_stat_offset;
	pp_ctrl_reg = port->pp_ctl_offset;

	pr_debug("mask %08x value %08x status %08x control %08x\n",
			mask, value,
			REG_READ(pp_stat_reg),
			REG_READ(pp_ctrl_reg));

	if (_wait_for((REG_READ(pp_stat_reg) & mask) == value, 5000, 10)) {
		pr_debug("Panel status timeout: status %08x control %08x\n",
				REG_READ(pp_stat_reg),
				REG_READ(pp_ctrl_reg));
		err = -ETIMEDOUT;
	}

	return err;
}

u8 get_vswing_max(u8 preemp)
{
	switch (preemp) {
	case 0:
		return DP_TRAIN_VOLTAGE_SWING_1200;
	case 1:
		return DP_TRAIN_VOLTAGE_SWING_800;
	case 2:
		return DP_TRAIN_VOLTAGE_SWING_600;
	case 3:
	default:
		return DP_TRAIN_VOLTAGE_SWING_400;
	}
}

u8 get_preemp_max(void)
{
	return DP_TRAIN_PRE_EMPHASIS_9_5 >> DP_TRAIN_PRE_EMPHASIS_SHIFT;
}

void vlv_dp_port_get_max_vswing_preemp(struct vlv_dp_port *port,
	enum vswing_level *max_v, enum preemp_level *max_p)
{
	*max_v = e1_2;
	if (port->is_edp)
		*max_p = e6dB;
	else
		*max_p = e9_5dB;
}

u32 vlv_dp_port_panel_power_seq(struct vlv_dp_port *port, bool enable)
{
	u32 pp = 0;
	u32 err = 0;


	if (enable) {
		wait_panel_status(port, IDLE_CYCLE_MASK, IDLE_CYCLE_VALUE);

		pp = REG_READ(port->pp_ctl_offset);
		pp |= (POWER_TARGET_ON | PANEL_POWER_RESET | (1 << 3));

		REG_WRITE(port->pp_ctl_offset, pp);
		err = wait_panel_status(port, IDLE_ON_MASK, IDLE_ON_VALUE);
	} else {
		pp = REG_READ(port->pp_ctl_offset);
		pp &= ~(POWER_TARGET_ON | PANEL_POWER_RESET);

		REG_WRITE(port->pp_ctl_offset, pp);
		msleep(port->pps_delays.t10);
		err = wait_panel_status(port, IDLE_OFF_MASK, IDLE_OFF_VALUE);
	}

	pr_info("ctl = %8x stat = %8x\n", REG_READ(port->pp_ctl_offset),
					REG_READ(port->pp_stat_offset));
	return err;
}

u32 vlv_dp_port_pwm_seq(struct vlv_dp_port *port, bool enable)
{
	u32 val = 0;

	if (enable) {
		val = PWM_CTL_DEFAULT | PWM_ENABLE;
		REG_WRITE(port->pwm_ctl_offset, val);
		val = port->duty_cycle_delay = PWM_DUTY_CYCLE;
		REG_WRITE(port->pwm_duty_cycle_offset, val);
		mdelay(5);
	} else {

		val = REG_READ(port->pwm_duty_cycle_offset);
		port->duty_cycle_delay = val;

		/* clear the duty cycle */
		val &= ~BACKLIGHT_DUTY_CYCLE_MASK;
		REG_WRITE(port->pwm_duty_cycle_offset, val);

		mdelay(5);
		val = REG_READ(port->pwm_ctl_offset);
		val &= ~BIT31;
		REG_WRITE(port->pwm_ctl_offset, val);
	}
	return 0;
}

u32 vlv_dp_port_backlight_seq(struct vlv_dp_port *port, bool enable)
{
	u32 pp = 0;

	/* FIXME: implement register locking */
	if (enable) {
		vlv_dp_port_pwm_seq(port, enable);

		msleep(port->pps_delays.t8);
		pp = REG_READ(port->pp_ctl_offset);
		pp |= EDP_BLC_ENABLE;
		REG_WRITE(port->pp_ctl_offset, pp);
	} else {
		pp = REG_READ(port->pp_ctl_offset);
		pp &= ~EDP_BLC_ENABLE;
		REG_WRITE(port->pp_ctl_offset, pp);
		vlv_dp_port_pwm_seq(port, enable);
	}

	return 0;
}

u32 vlv_dp_port_enable(struct vlv_dp_port *port, u32 flags,
		union encoder_params *params)
{
	u32 reg_val = 0;
	u8 lane_count = params->dp.lane_count;

	reg_val = REG_READ(port->offset);
	reg_val |= DP_VOLTAGE_0_4 | DP_PRE_EMPHASIS_0;
	reg_val |= DP_PORT_WIDTH(lane_count);

	/* FIXME: set audio if supported */
	if (flags & DRM_MODE_FLAG_PHSYNC)
		reg_val |= DP_SYNC_HS_HIGH;

	if (flags & DRM_MODE_FLAG_PVSYNC)
		reg_val |= DP_SYNC_VS_HIGH;

	reg_val &= ~(DP_LINK_TRAIN_MASK);
	reg_val |= DP_LINK_TRAIN_PAT_IDLE;

	/* FIXME: find a way to check this based on dpcd */
	reg_val |= DP_ENHANCED_FRAMING;

	/*
	 * bit varies between VLV/CHV hence using local var
	 * that was set during init itself
	 */
	reg_val |= port->pipe_select_val;

	REG_WRITE(port->offset, reg_val);

	reg_val |= DP_PORT_EN;

	REG_WRITE(port->offset, reg_val);
	return 0;
}

u32 vlv_dp_port_disable(struct vlv_dp_port *port)
{
	u32 reg_val = 0;

	reg_val = REG_READ(port->offset);

	/* set link to idle */
	reg_val &= ~DP_LINK_TRAIN_MASK_CHV;
	reg_val |= DP_LINK_TRAIN_PAT_IDLE;
	REG_WRITE(port->offset, reg_val);
	mdelay(17);

	/* disable port */
	reg_val &= ~DP_AUDIO_OUTPUT_ENABLE;
	reg_val &= ~DP_PORT_EN;
	REG_WRITE(port->offset, reg_val);

	reg_val = REG_READ(port->hist_guard_offset);
	reg_val &= ~(1 << 31);
	REG_WRITE(port->hist_guard_offset, reg_val);

	reg_val = REG_READ(port->hist_ctl_offset);
	reg_val &= ~(1 << 31);
	REG_WRITE(port->hist_ctl_offset, reg_val);

	/* perform lane reset frm chv_post_disable_dp */
	return 0;
}

bool vlv_dp_port_is_screen_connected(struct vlv_dp_port *port)
{
	bool ret = false;
	u32 bit = 0;
	u32 val = REG_READ(PORT_HOTPLUG_STAT);

	switch (port->port_id) {
	case PORT_B:
		bit = PORTB_HOTPLUG_LIVE_STATUS_VLV;
		break;
	case PORT_C:
		bit = PORTC_HOTPLUG_LIVE_STATUS_VLV;
		break;
	case PORT_D:
		bit = PORTD_HOTPLUG_LIVE_STATUS_VLV;
		break;
	default:
		break;
	}

	if (val & bit)
		ret = true;

	return ret;

}

static u32 vlv_dp_port_aux_wait_done(struct vlv_dp_port *port, bool has_aux_irq)
{
	u32 ch_ctl = port->aux_ctl_offset;
	u32 status = 0;
	bool done = true;

#define c (((status = REG_READ(ch_ctl)) & DP_AUX_CH_CTL_SEND_BUSY) == 0)
	if (has_aux_irq)
		;
	/*
	 * FIXME: check if this required
	 * done = wait_event_timeout(dev_priv->gmbus_wait_queue, c,
	 *				msecs_to_jiffies_timeout(10));
	 */
	else
		done = wait_for_atomic(c, 10) == 0;
	if (!done)
		pr_err("dp aux hw did not signal timeout (has irq: %i)!\n",
			has_aux_irq);
#undef c

	return status;
}

static u32 get_aux_send_ctl(bool has_aux_irq, int send_bytes,
		uint32_t aux_clock_divider)
{
	u32 precharge, timeout;

	precharge = 5;
	timeout = DP_AUX_CH_CTL_TIME_OUT_400us;

	return DP_AUX_CH_CTL_SEND_BUSY |
		DP_AUX_CH_CTL_DONE |
		(has_aux_irq ? DP_AUX_CH_CTL_INTERRUPT : 0) |
		DP_AUX_CH_CTL_TIME_OUT_ERROR |
		timeout |
		DP_AUX_CH_CTL_RECEIVE_ERROR |
		(send_bytes << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
		(precharge << DP_AUX_CH_CTL_PRECHARGE_2US_SHIFT) |
		(aux_clock_divider << DP_AUX_CH_CTL_BIT_CLOCK_2X_SHIFT);
}

static u32 get_aux_clock_divider(u32 index)
{
	return index ? 0 : 100;
}

static u32 pack_aux(uint8_t *src, int src_bytes)
{
	int	i;
	uint32_t v = 0;

	if (src_bytes > 4)
		src_bytes = 4;
	for (i = 0; i < src_bytes; i++)
		v |= ((uint32_t) src[i]) << ((3-i) * 8);
	return v;
}

static void unpack_aux(uint32_t src, uint8_t *dst, int dst_bytes)
{
	int i;
	if (dst_bytes > 4)
		dst_bytes = 4;
	for (i = 0; i < dst_bytes; i++)
		dst[i] = src >> ((3-i) * 8);
}

static u32 vlv_dp_port_aux_ch(struct vlv_dp_port *port,
		u8 *send, u32 send_bytes,
		u8 *recv, u32 recv_size)
{
	int try, clock = 0;
	u32 ch_data, ch_ctl;
	u32 aux_clock_divider;
	u32 status = 0, recv_bytes = 0;
	u32 ret = 0, i;
	/* FIXME: check if this is needed */
	bool has_aux_irq = false;
	u32 val = 0;

	ch_ctl = port->aux_ctl_offset;
	ch_data = port->aux_ctl_offset + 4;

	/* Try to wait for any previous AUX channel activity */
	for (try = 0; try < 3; try++) {
		status = REG_READ(ch_ctl);
		if ((status & DP_AUX_CH_CTL_SEND_BUSY) == 0)
			break;
		mdelay(1);
	}

	if (try == 3) {
		pr_err("%s:aux_ch not started status 0x%08x\n",
			 __func__, REG_READ(ch_ctl));
		ret = -EBUSY;
		goto aux_out;
	}

	/* Only 5 data registers! */
	if (WARN_ON(send_bytes > 20 || recv_size > 20)) {
		ret = -E2BIG;
		goto aux_out;
	}

	while ((aux_clock_divider = get_aux_clock_divider(clock++))) {
		u32 send_ctl = get_aux_send_ctl(has_aux_irq, send_bytes,
							aux_clock_divider);

		/* Must try at least 3 times according to DP spec */
		for (try = 0; try < 5; try++) {
			/* Load the send data into the aux data registers */
			for (i = 0; i < send_bytes; i += 4) {
				val = pack_aux(send + i, send_bytes - i);
				REG_WRITE(ch_data + i, val);
			}

			/* Send the command and wait for it to complete */
			REG_WRITE(ch_ctl, send_ctl);

			/* FIXME:!!!!!!!!!!!!! check if has_irq helps here  */
			status = vlv_dp_port_aux_wait_done(port, false);

			status = REG_READ(ch_ctl);
			val =  status |
				DP_AUX_CH_CTL_DONE |
				DP_AUX_CH_CTL_TIME_OUT_ERROR |
				DP_AUX_CH_CTL_RECEIVE_ERROR;

			/* Clear done status and any errors */
			REG_WRITE(ch_ctl, val);

			if (status & (DP_AUX_CH_CTL_TIME_OUT_ERROR |
					DP_AUX_CH_CTL_RECEIVE_ERROR))
				continue;
			if (status & DP_AUX_CH_CTL_DONE)
				break;
		}
		if (status & DP_AUX_CH_CTL_DONE)
			break;
	}

	if ((status & DP_AUX_CH_CTL_DONE) == 0) {
		pr_warn("dp_port_aux_ch not done status 0x%08x\n", status);
		ret = -EBUSY;
		goto aux_out;
	}

	/*
	 * Check for timeout or receive error.
	 * Timeouts occur when the sink is not connected
	 */
	if (status & DP_AUX_CH_CTL_RECEIVE_ERROR) {
		pr_warn("dp_port_aux_ch receive error status 0x%08x\n", status);
		ret = -EIO;
		goto aux_out;
	}

	/*
	 * Timeouts occur when the device isn't connected, so they're
	 * "normal" -- don't fill the kernel log with these
	 */
	if (status & DP_AUX_CH_CTL_TIME_OUT_ERROR) {
		pr_warn("dp_port_aux_ch timeout status 0x%08x\n", status);
		ret = -ETIMEDOUT;
		goto aux_out;
	}

	/* Unload any bytes sent back from the other side */
	recv_bytes = ((status & DP_AUX_CH_CTL_MESSAGE_SIZE_MASK) >>
		      DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT);
	if (recv_bytes > recv_size)
		recv_bytes = recv_size;

	for (i = 0; i < recv_bytes; i += 4)
		unpack_aux(REG_READ(ch_data + i),
			   recv + i, recv_bytes - i);

	ret = recv_bytes;

aux_out:
	return ret;
}

u32 vlv_dp_port_aux_transfer(struct vlv_dp_port *port,
		struct dp_aux_msg *msg)
{
	uint8_t txbuf[20], rxbuf[20];
	size_t txsize, rxsize;
	u32 ret;

	mutex_lock(&port->hw_mutex);

	txbuf[0] = msg->request << 4;
	txbuf[1] = msg->address >> 8;
	txbuf[2] = msg->address & 0xff;
	txbuf[3] = msg->size - 1;

	switch (msg->request & ~DP_AUX_I2C_MOT) {
	case DP_AUX_NATIVE_WRITE:
	case DP_AUX_I2C_WRITE:
		txsize = msg->size ? HEADER_SIZE + msg->size :
					BARE_ADDRESS_SIZE;
		rxsize = 1;

		if (WARN_ON(txsize > 20)) {
			ret = -E2BIG;
			goto aux_tx_exit;
		}

		memcpy(txbuf + HEADER_SIZE, msg->buffer, msg->size);
		ret = vlv_dp_port_aux_ch(port, txbuf, txsize, rxbuf, rxsize);
		if ((ret > 0) && (ret < 20)) {
			msg->reply = rxbuf[0] >> 4;

			/* Return payload size. */
			ret = msg->size;
		}
		break;

	case DP_AUX_NATIVE_READ:
	case DP_AUX_I2C_READ:
		txsize = msg->size ? HEADER_SIZE : BARE_ADDRESS_SIZE;
		rxsize = msg->size + 1;

		if (WARN_ON(rxsize > 20)) {
			ret = -E2BIG;
			goto aux_tx_exit;
		}
		ret = vlv_dp_port_aux_ch(port, txbuf, txsize, rxbuf, rxsize);
		if ((ret > 0) && (ret < 20)) {
			msg->reply = rxbuf[0] >> 4;
			/*
			 * Assume happy day, and copy the data. The caller is
			 * expected to check msg->reply before touching it.
			 *
			 * Return payload size.
			 */
			ret--;
			memcpy(msg->buffer, rxbuf + 1, ret);
		}
		break;

	default:
		ret = -EINVAL;
		break;
	}

aux_tx_exit:
	mutex_unlock(&port->hw_mutex);
	return ret;

}

u32 vlv_dp_port_set_signal_levels(struct vlv_dp_port *port,
	struct link_params *params, u32 *deemp, u32 *margin)
{
	/* FIXME: implement this */
	BUG();
	return -EINVAL;
}

void vlv_dp_port_get_adjust_train(struct vlv_dp_port *port,
	struct link_params *params)
{
	u8 v = 0, this_v = 0, p = 0, this_p = 0;
	u32 link_status;
	u8 temp;
	struct dp_aux_msg msg = {0};
	int lane;
	u8 preemph_max;
	u8 voltage_max;

	/* read status 206 & 207 */
	msg.address = DP_ADJUST_REQUEST_LANE0_1;
	msg.request = DP_AUX_NATIVE_READ;
	msg.buffer = (u8 *) &link_status;
	msg.size = 2;

	vlv_dp_port_aux_transfer(port, &msg);
	for (lane = 0; lane < params->lane_count; lane++) {
		temp = (u8) (0xF & (link_status >> (lane * 4)));
		this_v = (temp & DP_TRAIN_VOLTAGE_SWING_MASK);
		this_p = ((temp & 0xC) >> 2);

		if (this_v > v)
			v = this_v;
		if (this_p > p)
			p = this_p;
	}

	params->vswing = v;
	params->preemp = p;

	preemph_max = get_preemp_max();
	if (p >= preemph_max)
		params->preemp = preemph_max;

	voltage_max = get_vswing_max(p);
	if (v >= voltage_max)
		params->vswing = voltage_max;
}

u32 vlv_dp_port_set_link_pattern(struct vlv_dp_port *port,
		u8 train_pattern)
{
	u32 val = 0;

	val = REG_READ(port->offset);

	if (train_pattern & DP_LINK_SCRAMBLING_DISABLE)
		val |= DP_TP_CTL_SCRAMBLE_DISABLE;
	else
		val &= ~DP_TP_CTL_SCRAMBLE_DISABLE;


	val &= ~DP_LINK_TRAIN_MASK_CHV;

	switch (train_pattern & DP_TRAINING_PATTERN_MASK) {
	case DP_TRAINING_PATTERN_DISABLE:
		val |= DP_LINK_TRAIN_OFF;
		break;
	case DP_TRAINING_PATTERN_1:
		val |= DP_LINK_TRAIN_PAT_1;
		break;
	case DP_TRAINING_PATTERN_2:
		val |= DP_LINK_TRAIN_PAT_2;
		break;
	/* This case may never hit !!!! */
	case DP_PORT_IDLE_PATTERN_SET:
		val |= DP_LINK_TRAIN_PAT_IDLE;
		break;
	case DP_TRAINING_PATTERN_3:
		val |= DP_LINK_TRAIN_PAT_3_CHV;
		break;
	}

	if (train_pattern & DP_PORT_IDLE_PATTERN_SET)
		val |= DP_LINK_TRAIN_PAT_IDLE;

	REG_WRITE(port->offset, val);
	REG_READ(port->offset);

	if (train_pattern & DP_PORT_IDLE_PATTERN_SET)
		mdelay(1);

	return 0;
}

static u32 vlv_dp_port_i2c_functionality(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL |
		I2C_FUNC_SMBUS_READ_BLOCK_DATA |
		I2C_FUNC_SMBUS_BLOCK_PROC_CALL |
		I2C_FUNC_10BIT_ADDR;
}

/*
 * Transfer a single I2C-over-AUX message and handle various error conditions,
 * retrying the transaction as appropriate.  It is assumed that the
 * vlv_dp_port_aux_transfer function does not modify anything in the msg
 * other than the reply field.
 */
static int vlv_dp_port_i2c_do_msg(struct vlv_dp_port *port,
		struct dp_aux_msg *msg)
{
	unsigned int retry;
	int err;

	/*
	 * DP1.2 sections 2.7.7.1.5.6.1 and 2.7.7.1.6.6.1: A DP Source device
	 * is required to retry at least seven times upon receiving AUX_DEFER
	 * before giving up the AUX transaction.
	 */
	for (retry = 0; retry < 7; retry++) {
		err = vlv_dp_port_aux_transfer(port, msg);
		if (err < 0) {
			if (err == -EBUSY)
				continue;

			pr_debug("transaction failed: %d\n", err);
			return err;
		}


		switch (msg->reply & DP_AUX_NATIVE_REPLY_MASK) {
		case DP_AUX_NATIVE_REPLY_ACK:
			/*
			 * For I2C-over-AUX transactions this isn't enough, we
			 * need to check for the I2C ACK reply.
			 */
			break;

		case DP_AUX_NATIVE_REPLY_NACK:
			pr_debug("native nack\n");
			return -EREMOTEIO;

		case DP_AUX_NATIVE_REPLY_DEFER:
			pr_debug("native defer");

			/*
			 * We could check for I2C bit rate capabilities and if
			 * available adjust this interval. We could also be
			 * more careful with DP-to-legacy adapters where a
			 * long legacy cable may force very low I2C bit rates.
			 *
			 * For now just defer for long enough to hopefully be
			 * safe for all use-cases.
			 */
			usleep_range(500, 600);
			continue;

		default:
			pr_err("invalid native reply %#04x\n", msg->reply);
			return -EREMOTEIO;
		}

		switch (msg->reply & DP_AUX_I2C_REPLY_MASK) {
		case DP_AUX_I2C_REPLY_ACK:
			/*
			 * Both native ACK and I2C ACK replies received. We
			 * can assume the transfer was successful.
			 */
			if (err < msg->size)
				return -EPROTO;
			return 0;

		case DP_AUX_I2C_REPLY_NACK:
			pr_debug("I2C nack\n");
			return -EREMOTEIO;

		case DP_AUX_I2C_REPLY_DEFER:
			pr_debug("I2C defer\n");
			usleep_range(400, 500);
			continue;

		default:
			pr_err("invalid I2C reply %#04x\n", msg->reply);
			return -EREMOTEIO;
		}
	}

	pr_debug("too many retries, giving up\n");
	return -EREMOTEIO;
}

struct i2c_adapter *vlv_dp_port_get_i2c_adapter(struct vlv_dp_port *port)
{
	return &port->ddc;
}

static int vlv_dp_port_i2c_xfer(struct i2c_adapter *adapter,
		struct i2c_msg *msgs, int num)
{
	struct vlv_dp_port *port = adapter->algo_data;
	unsigned int i, j;
	struct dp_aux_msg msg;
	int err = 0;

	memset(&msg, 0, sizeof(msg));

	for (i = 0; i < num; i++) {
		msg.address = msgs[i].addr;
		msg.request = (msgs[i].flags & I2C_M_RD) ?
				DP_AUX_I2C_READ : DP_AUX_I2C_WRITE;
		msg.request |= DP_AUX_I2C_MOT;

		/*
		 * Send a bare address packet to start the transaction.
		 * Zero sized messages specify an address only (bare
		 * address) transaction.
		 */
		msg.buffer = NULL;
		msg.size = 0;
		err = vlv_dp_port_i2c_do_msg(port, &msg);
		if (err < 0)
			break;

		/*
		 * Many hardware implementations support FIFOs larger than a
		 * single byte, but it has been empirically determined that
		 * transferring data in larger chunks can actually lead to
		 * decreased performance. Therefore each message is simply
		 * transferred byte-by-byte.
		 */
		for (j = 0; j < msgs[i].len; j++) {
			msg.buffer = msgs[i].buf + j;
			msg.size = 1;
			err = vlv_dp_port_i2c_do_msg(port, &msg);
			if (err < 0)
				break;
		}
		if (err < 0)
			break;
	}
	if (err >= 0)
		err = num;

	/*
	 * Send a bare address packet to close out the transaction.
	 * Zero sized messages specify an address only (bare
	 * address) transaction.
	 */
	msg.request &= ~DP_AUX_I2C_MOT;
	msg.buffer = NULL;
	msg.size = 0;
	(void)vlv_dp_port_i2c_do_msg(port, &msg);

	return err;
}

static const struct i2c_algorithm i2c_algo = {
	.functionality = vlv_dp_port_i2c_functionality,
	.master_xfer = vlv_dp_port_i2c_xfer,
};

u32 vlv_dp_port_i2c_register(struct vlv_dp_port *port, struct device *dev)
{
	mutex_init(&port->hw_mutex);

	port->ddc.algo = &i2c_algo;
	port->ddc.algo_data = port;
	port->ddc.retries = 3;
	port->ddc.class = I2C_CLASS_DDC;
	port->ddc.owner = THIS_MODULE;
	port->ddc.dev.parent = dev;
	port->ddc.dev.of_node = dev->of_node;
	strlcpy(port->ddc.name, port->name, sizeof(port->ddc.name));

	return i2c_add_adapter(&port->ddc);
}

u32 vlv_dp_port_vdd_seq(struct vlv_dp_port *port, bool enable)
{
	u32 val = 0;
	val = REG_READ(port->pp_ctl_offset);

	if (enable == (val & (1 << 3)))
		return 0;

	if (enable) {
		val |= (1 << 3);
		REG_WRITE(port->pp_ctl_offset, val);
		msleep(200);
	} else {
		val &= ~(1 << 3);
		REG_WRITE(port->pp_ctl_offset, val);
	}

	return 0;
}

u32 vlv_dp_port_load_panel_delays(struct vlv_dp_port *port)
{
	u16 *ptr = NULL;
	u32 val = 0;
	if (!port->is_edp)
		return 0;

	ptr = intel_get_vbt_pps_delays();
	if (!ptr) {
		pr_err("unable to load pps delays from vbt, using spec default\n");

		port->pps_delays.t1_t3 = 210 * 10;
		port->pps_delays.t8 = 50 * 10;
		port->pps_delays.t9 = 50 * 10;
		port->pps_delays.t10 = 500 * 10;
		port->pps_delays.t11_t12 = (510 + 100) * 10;

	} else {
		pr_err("PPS delays: %d %d %d %d %d\n", *ptr, *(ptr+1), *(ptr+2),
				*(ptr+3), *(ptr+4));

		port->pps_delays.t1_t3 = *ptr;
		port->pps_delays.t8 = *(ptr+1);
		port->pps_delays.t9 = *(ptr+2);
		port->pps_delays.t10 = *(ptr+3);
		port->pps_delays.t11_t12 = *(ptr+4);
	}

	val = (port->port_id << 30);
	val |= ((port->pps_delays.t1_t3 << 16) | (port->pps_delays.t8));
	REG_WRITE(port->pp_on_delay_offset, val);

	val = ((port->pps_delays.t9 << 16) | (port->pps_delays.t10));
	REG_WRITE(port->pp_off_delay_offset, val);

	val = (port->pps_delays.t11_t12 / 1000) + 1;

	/* 200Mhz so directly program it */
	val = ((0x270F << 8) | (val));
	REG_WRITE(port->pp_divisor_offset, val);
	return 0;
}

void vlv_dp_port_destroy(struct vlv_dp_port *port)
{
	mutex_destroy(&port->hw_mutex);
	i2c_del_adapter(&port->ddc);
}

bool vlv_dp_port_init(struct vlv_dp_port *port, enum port port_id,
		enum pipe pipe_id, enum intel_pipe_type type,
		struct device *dev)
{
	bool ret = false;
	port->port_id = port_id;
	switch (port_id) {
	case PORT_B:
		port->offset = VLV_DISPLAY_BASE + DP_B;
		port->aux_ctl_offset = VLV_DISPLAY_BASE + DPB_AUX_CH_CTL;
		port->name = "DPDDC-B";
		break;
	case PORT_C:
		port->offset = VLV_DISPLAY_BASE + DP_C;
		port->aux_ctl_offset = VLV_DISPLAY_BASE + DPC_AUX_CH_CTL;
		port->name = "DPDDC-C";
		break;
	case PORT_D:
		if (type == INTEL_PIPE_EDP) {
			pr_err("%s: EDP not supported on port D\n", __func__);
			return false;
		}
		port->offset = VLV_DISPLAY_BASE + DP_D;
		port->aux_ctl_offset = VLV_DISPLAY_BASE + DPD_AUX_CH_CTL;
		port->name = "DPDDC-D";
		break;
	case PORT_A:
	default:
		/* PORT A not supported for DP/eDP in VLV/CHV */
		pr_err("%s: invalid port id passed\n", __func__);
		return false;
	}

	if (type == INTEL_PIPE_EDP)
		port->is_edp = true;

	port->pp_ctl_offset = VLV_PIPE_PP_CONTROL(pipe_id);
	port->pp_stat_offset = VLV_PIPE_PP_STATUS(pipe_id);
	port->pp_on_delay_offset = VLV_PIPE_PP_ON_DELAYS(pipe_id);
	port->pp_off_delay_offset = VLV_PIPE_PP_OFF_DELAYS(pipe_id);
	port->pp_divisor_offset = VLV_PIPE_PP_DIVISOR(pipe_id);
	port->pwm_ctl_offset = VLV_BLC_PWM_CTL2(pipe_id);
	port->pwm_duty_cycle_offset = VLV_BLC_PWM_CTL(pipe_id);
	port->hist_guard_offset = VLV_BLC_HIST_GUARD(pipe_id);
	port->hist_ctl_offset = VLV_BLC_HIST_CTL(pipe_id);

	port->duty_cycle_delay = 0;

	ret = vlv_dp_port_load_panel_delays(port);
	vlv_dp_port_i2c_register(port, dev);

	/* enable vdd in case it is not already on for edid read */
	if (port->is_edp)
		vlv_dp_port_vdd_seq(port, true);

	if (IS_CHERRYVIEW())
		port->pipe_select_val = DP_PIPE_SELECT_CHV(pipe_id);
	else
		port->pipe_select_val = (pipe_id ? DP_PIPE_MASK : 0);

	pr_info("%s:%d port_id %d pipe %d\n", __func__, __LINE__,
			port_id, pipe_id);

	return true;
}

u32 vlv_dp_port_set_brightness(struct vlv_dp_port *port, int level)
{
	u32 val;

	if (!port->is_edp)
		return 0;

	val = REG_READ(port->pwm_duty_cycle_offset);
	level = (level * PLATFORM_MAX_BRIGHTNESS) / MAXIMUM_BRIGHTNESS;
	val = val & ~BACKLIGHT_DUTY_CYCLE_MASK;
	REG_WRITE(port->pwm_duty_cycle_offset, val | level);

	return 1;
}

u32 vlv_dp_port_get_brightness(struct vlv_dp_port *port)
{
	u32 val;
	val = REG_READ(port->pwm_duty_cycle_offset) & BACKLIGHT_DUTY_CYCLE_MASK;

	if (!port->is_edp)
		return 0;

	val = (val/PLATFORM_MAX_BRIGHTNESS * MAXIMUM_BRIGHTNESS);

	return val;
}
