/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Jani Nikula <jani.nikula@intel.com>
 */
/* FIXME: remove unnecessary */
#include <linux/export.h>
#include <intel_adf_device.h>
#include <video/mipi_display.h>
#include <core/common/dsi/dsi_config.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/vlv/vlv_dc_regs.h>

#include <drm/drmP.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dsi_port.h>

/*
 * XXX: MIPI_DATA_ADDRESS, MIPI_DATA_LENGTH, MIPI_COMMAND_LENGTH, and
 * MIPI_COMMAND_ADDRESS registers.
 *
 * Apparently these registers provide a MIPI adapter level way to send (lots of)
 * commands and data to the receiver, without having to write the commands and
 * data to MIPI_{HS,LP}_GEN_{CTRL,DATA} registers word by word.
 *
 * Presumably for anything other than MIPI_DCS_WRITE_MEMORY_START and
 * MIPI_DCS_WRITE_MEMORY_CONTINUE (which are used to update the external
 * framebuffer in command mode displays) these are just an optimization that can
 * come later.
 *
 * For memory writes, these should probably be used for performance.
 */

static void print_stat(struct vlv_dsi_port *port)
{
	u32 val;

	val = REG_READ(port->intr_stat_offset);
		/* FIXME: hardcoded to pipe A for now */
#define STAT_BIT(val, bit) (val) & (bit) ? " " #bit : ""
	pr_info("MIPI_INTR_STAT(0) = %08x\n"
	"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
		val,
		STAT_BIT(val, TEARING_EFFECT),
		STAT_BIT(val, SPL_PKT_SENT_INTERRUPT),
		STAT_BIT(val, GEN_READ_DATA_AVAIL),
		STAT_BIT(val, LP_GENERIC_WR_FIFO_FULL),
		STAT_BIT(val, HS_GENERIC_WR_FIFO_FULL),
		STAT_BIT(val, RX_PROT_VIOLATION),
		STAT_BIT(val, RX_INVALID_TX_LENGTH),
		STAT_BIT(val, ACK_WITH_NO_ERROR),
		STAT_BIT(val, TURN_AROUND_ACK_TIMEOUT),
		STAT_BIT(val, LP_RX_TIMEOUT),
		STAT_BIT(val, HS_TX_TIMEOUT),
		STAT_BIT(val, DPI_FIFO_UNDERRUN),
		STAT_BIT(val, LOW_CONTENTION),
		STAT_BIT(val, HIGH_CONTENTION),
		STAT_BIT(val, TXDSI_VC_ID_INVALID),
		STAT_BIT(val, TXDSI_DATA_TYPE_NOT_RECOGNISED),
		STAT_BIT(val, TXCHECKSUM_ERROR),
		STAT_BIT(val, TXECC_MULTIBIT_ERROR),
		STAT_BIT(val, TXECC_SINGLE_BIT_ERROR),
		STAT_BIT(val, TXFALSE_CONTROL_ERROR),
		STAT_BIT(val, RXDSI_VC_ID_INVALID),
		STAT_BIT(val, RXDSI_DATA_TYPE_NOT_REGOGNISED),
		STAT_BIT(val, RXCHECKSUM_ERROR),
		STAT_BIT(val, RXECC_MULTIBIT_ERROR),
		STAT_BIT(val, RXECC_SINGLE_BIT_ERROR),
		STAT_BIT(val, RXFALSE_CONTROL_ERROR),
		STAT_BIT(val, RXHS_RECEIVE_TIMEOUT_ERROR),
		STAT_BIT(val, RX_LP_TX_SYNC_ERROR),
		STAT_BIT(val, RXEXCAPE_MODE_ENTRY_ERROR),
		STAT_BIT(val, RXEOT_SYNC_ERROR),
		STAT_BIT(val, RXSOT_SYNC_ERROR),
		STAT_BIT(val, RXSOT_ERROR));
#undef STAT_BIT
}

enum dsi_type {
	DSI_DCS,
	DSI_GENERIC,
};

/* enable or disable command mode hs transmissions */
void vlv_dsi_port_cmd_hs_mode_enable(struct vlv_dsi_port *port, bool enable)
{
	u32 temp;
	u32 mask = DBI_FIFO_EMPTY;

	if (wait_for((REG_READ(port->fifo_stat_offset) & mask) == mask, 50))
		pr_err("Timeout waiting for DBI FIFO empty\n");

	temp = REG_READ(port->hs_ls_dbi_enable_offset);
	temp &= DBI_HS_LP_MODE_MASK;
	REG_WRITE(port->hs_ls_dbi_enable_offset,
		enable ? DBI_HS_MODE : DBI_LP_MODE);

	port->hs_mode = enable;
}

static int vlv_dsi_port_cmd_vc_send_short(struct vlv_dsi_port *port,
		int channel, u8 data_type, u16 data)
{
	u32 ctrl_reg;
	u32 ctrl;
	u32 mask;

	pr_info("channel %d, data_type %u, data %04x\n",
			channel, data_type, data);

	if (port->hs_mode) {
		ctrl_reg = port->hs_gen_ctrl_offset;
		mask = HS_CTRL_FIFO_FULL;
	} else {
		ctrl_reg = port->lp_gen_ctrl_offset;
		mask = LP_CTRL_FIFO_FULL;
	}

	if (wait_for((REG_READ(port->fifo_stat_offset) & mask) == 0, 50)) {
		pr_err("Timeout waiting for HS/LP CTRL FIFO !full\n");
		print_stat(port);
	}

	/*
	 * Note: This function is also used for long packets, with length passed
	 * as data, since SHORT_PACKET_PARAM_SHIFT ==
	 * LONG_PACKET_WORD_COUNT_SHIFT.
	 */
	ctrl = data << SHORT_PACKET_PARAM_SHIFT |
		channel << VIRTUAL_CHANNEL_SHIFT |
		data_type << DATA_TYPE_SHIFT;

	REG_WRITE(ctrl_reg, ctrl);

	return 0;
}

static int vlv_dsi_port_cmd_vc_send_long(struct vlv_dsi_port *port,
			int channel, u8 data_type, const u8 *data, int len)
{
	u32 data_reg;
	int i, j, n;
	u32 mask;

	pr_info("channel %d, data_type %u, len %04x\n",
			channel, data_type, len);

	if (port->hs_mode) {
		data_reg = port->hs_gen_data_offset;
		mask = HS_DATA_FIFO_FULL;
	} else {
		data_reg = port->lp_gen_data_offset;
		mask = LP_DATA_FIFO_FULL;
	}

	if (wait_for((REG_READ(port->fifo_stat_offset) & mask) == 0, 50))
		pr_err("Timeout waiting for HS/LP DATA FIFO !full\n");

	for (i = 0; i < len; i += n) {
		u32 val = 0;
		n = min_t(int, len - i, 4);

		for (j = 0; j < n; j++)
			val |= *data++ << 8 * j;

		REG_WRITE(data_reg, val);
		/*
		 * XXX: check for data fifo full, once that is set, write 4
		 * dwords, then wait for not set, then continue.
		 */
	}

	return vlv_dsi_port_cmd_vc_send_short(port, channel, data_type, len);
}

static int vlv_dsi_port_cmd_vc_write_common(struct vlv_dsi_port *port,
		int channel, const u8 *data, int len, enum dsi_type type)
{
	int ret;

	if (len == 0) {
		BUG_ON(type == DSI_GENERIC);
		ret = vlv_dsi_port_cmd_vc_send_short(port, channel,
				MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM, 0);
	} else if (len == 1) {
		ret = vlv_dsi_port_cmd_vc_send_short(port, channel,
					type == DSI_GENERIC ?
					MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM :
					MIPI_DSI_DCS_SHORT_WRITE, data[0]);
	} else if (len == 2) {
		ret = vlv_dsi_port_cmd_vc_send_short(port, channel,
					type == DSI_GENERIC ?
					MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM :
					MIPI_DSI_DCS_SHORT_WRITE_PARAM,
					(data[1] << 8) | data[0]);
	} else {
		ret = vlv_dsi_port_cmd_vc_send_long(port, channel,
					type == DSI_GENERIC ?
					MIPI_DSI_GENERIC_LONG_WRITE :
					MIPI_DSI_DCS_LONG_WRITE, data, len);
	}

	return ret;
}

int vlv_dsi_port_cmd_vc_dcs_write(struct vlv_dsi_port *port, int channel,
		const u8 *data, int len)
{
	return vlv_dsi_port_cmd_vc_write_common(port, channel, data, len,
		DSI_DCS);
}

int vlv_dsi_port_cmd_vc_generic_write(struct vlv_dsi_port *port, int channel,
		const u8 *data, int len)
{
	return vlv_dsi_port_cmd_vc_write_common(port, channel, data, len,
		DSI_GENERIC);
}

static int vlv_dsi_port_cmd_vc_dcs_send_read_request(struct vlv_dsi_port *port,
		int channel, u8 dcs_cmd)
{
	return vlv_dsi_port_cmd_vc_send_short(port, channel, MIPI_DSI_DCS_READ,
				dcs_cmd);
}

static int vlv_dsi_port_cmd_vc_generic_send_read_request(
		struct vlv_dsi_port *port,
		int channel, u8 *reqdata, int reqlen)
{
	u16 data;
	u8 data_type;

	switch (reqlen) {
	case 0:
		data_type = MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM;
		data = 0;
		break;
	case 1:
		data_type = MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM;
		data = reqdata[0];
		break;
	case 2:
		data_type = MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM;
		data = (reqdata[1] << 8) | reqdata[0];
		break;
	default:
		BUG();
	}

	return vlv_dsi_port_cmd_vc_send_short(port, channel, data_type, data);
}

static int vlv_dsi_port_cmd_read_data_return(struct vlv_dsi_port *port,
		u8 *buf, int buflen)
{
	int i, len = 0;
	u32 data_reg, val;

	if (port->hs_mode)
		data_reg = port->hs_gen_data_offset;
	else
		data_reg = port->lp_gen_data_offset;

	while (len < buflen) {
		val = REG_READ(data_reg);
		for (i = 0; i < 4 && len < buflen; i++, len++)
			buf[len] = val >> 8 * i;
	}

	return len;
}

int vlv_dsi_port_cmd_vc_dcs_read(struct vlv_dsi_port *port, int channel,
		u8 dcs_cmd, u8 *buf, int buflen)
{
	u32 mask;
	int ret;

	/*
	 * XXX: should issue multiple read requests and reads if request is
	 * longer than MIPI_MAX_RETURN_PKT_SIZE
	 */

	REG_WRITE(port->intr_stat_offset, GEN_READ_DATA_AVAIL);

	ret = vlv_dsi_port_cmd_vc_dcs_send_read_request(port, channel, dcs_cmd);
	if (ret)
		return ret;

	mask = GEN_READ_DATA_AVAIL;
	if (wait_for((REG_READ(port->intr_stat_offset) & mask) == mask, 50))
		DRM_ERROR("Timeout waiting for read data.\n");

	ret = vlv_dsi_port_cmd_read_data_return(port, buf, buflen);
	if (ret < 0)
		return ret;

	if (ret != buflen)
		return -EIO;

	return 0;
}

int vlv_dsi_port_cmd_vc_generic_read(struct vlv_dsi_port *port, int channel,
		u8 *reqdata, int reqlen, u8 *buf, int buflen)
{
	u32 mask;
	int ret;

	/*
	 * XXX: should issue multiple read requests and reads if request is
	 * longer than MIPI_MAX_RETURN_PKT_SIZE
	 */

	REG_WRITE(port->intr_stat_offset, GEN_READ_DATA_AVAIL);

	ret = vlv_dsi_port_cmd_vc_generic_send_read_request(port, channel,
				reqdata, reqlen);
	if (ret)
		return ret;

	mask = GEN_READ_DATA_AVAIL;
	if (wait_for((REG_READ(port->intr_stat_offset) & mask) == mask, 50))
		pr_err("Timeout waiting for read data.\n");

	ret = vlv_dsi_port_cmd_read_data_return(port, buf, buflen);
	if (ret < 0)
		return ret;

	if (ret != buflen)
		return -EIO;

	return 0;
}

/*
 * send a video mode command
 *
 * XXX: commands with data in MIPI_DPI_DATA?
 */
int vlv_dsi_port_cmd_dpi_send_cmd(struct vlv_dsi_port *port, u32 cmd, bool hs)
{
	u32 mask;

	/* XXX: pipe, hs */
	if (hs)
		cmd &= ~DPI_LP_MODE;
	else
		cmd |= DPI_LP_MODE;

	/* clear bit */
	REG_WRITE(port->intr_stat_offset, SPL_PKT_SENT_INTERRUPT);

	/* XXX: old code skips write if control unchanged */
	if (cmd == REG_READ(port->dpi_ctrl_offset))
		pr_err("Same special packet %02x twice in a row.\n", cmd);

	REG_WRITE(port->dpi_ctrl_offset, cmd);

	mask = SPL_PKT_SENT_INTERRUPT;
	if (wait_for((REG_READ(port->intr_stat_offset) & mask) == mask, 100))
		pr_err("Video mode command 0x%08x send failed.\n", cmd);

	return 0;
}
