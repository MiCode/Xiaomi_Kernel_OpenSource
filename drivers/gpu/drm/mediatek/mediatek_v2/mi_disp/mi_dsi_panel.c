// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#define pr_fmt(fmt)	"mi-dsi-panel:[%s:%d] " fmt, __func__, __LINE__
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/rtc.h>
#include <video/mipi_display.h>
#include <linux/fs.h>
#include "mi_dsi_panel.h"
#include "../../../../kernel/irq/internals.h"
#include <uapi/drm/mi_disp.h>

#include "mi_disp_feature.h"
#include "mtk_panel_ext.h"
#include "mi_disp_print.h"
#include "mi_panel_ext.h"
#include "mtk_drm_ddp_comp.h"
#include "mi_disp_lhbm.h"

//static struct LCM_param_read_write lcm_param_read_write = {0};
struct LCM_mipi_read_write lcm_mipi_read_write = {0};
struct LCM_led_i2c_read_write lcm_led_i2c_read_write = {0};
//extern struct frame_stat fm_stat;
struct mi_disp_notifier g_notify_data;
struct mi_dsi_panel_cfg *g_mi_cfg;

#define DEFAULT_MAX_BRIGHTNESS_CLONE 8191
#define DEFAULT_MAX_BRIGHTNESS  2047
#define MAX_CMDLINE_PARAM_LEN 64

static char lockdown_info[64] = {0};
extern void mipi_dsi_dcs_write_gce2(struct mtk_dsi *dsi, struct cmdq_pkt *dummy,
					  const void *data, size_t len);

bool is_backlight_set_skip(struct mtk_dsi *dsi, u32 bl_lvl)
{
	if (dsi->mi_cfg.in_fod_calibration ||
		dsi->mi_cfg.feature_val[DISP_FEATURE_HBM_FOD] == FEATURE_ON) {
		DISP_INFO("panel skip set backlight %d due to fod hbm "
				"or fod calibration\n", bl_lvl);
		return true;
	}
	return false;
}

bool dsi_panel_initialized(struct mtk_dsi *dsi)
{
	int ret = 0;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!dsi) {
		pr_err("NULL dsi\n");
		return false;
	}

	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!(panel_ext && panel_ext->funcs)) {
		pr_info("panel_ext func not defined\n");
		return false;
	}

	if (panel_ext->funcs->get_panel_initialized)
		ret = panel_ext->funcs->get_panel_initialized(dsi->panel);
	return ret;
}

static char string_to_hex(const char *str)
{
	char val_l = 0;
	char val_h = 0;

	if (str[0] >= '0' && str[0] <= '9')
		val_h = str[0] - '0';
	else if (str[0] <= 'f' && str[0] >= 'a')
		val_h = 10 + str[0] - 'a';
	else if (str[0] <= 'F' && str[0] >= 'A')
		val_h = 10 + str[0] - 'A';

	if (str[1] >= '0' && str[1] <= '9')
		val_l = str[1]-'0';
	else if (str[1] <= 'f' && str[1] >= 'a')
		val_l = 10 + str[1] - 'a';
	else if (str[1] <= 'F' && str[1] >= 'A')
		val_l = 10 + str[1] - 'A';

	return (val_h << 4) | val_l;
}

static int string_merge_into_buf(const char *str, int len, char *buf)
{
	int buf_size = 0;
	int i = 0;
	const char *p = str;

	while (i < len) {
		if (((p[0] >= '0' && p[0] <= '9') ||
			(p[0] <= 'f' && p[0] >= 'a') ||
			(p[0] <= 'F' && p[0] >= 'A'))
			&& ((i + 1) < len)) {
			buf[buf_size] = string_to_hex(p);
			pr_debug("0x%02x ", buf[buf_size]);
			buf_size++;
			i += 2;
			p += 2;
		} else {
			i++;
			p++;
		}
	}
	return buf_size;
}

ssize_t dsi_panel_write_mipi_reg(char *buf, size_t count)
{
	int retval = 0;
	int dlen = 0;
	unsigned int read_enable = 0;
	unsigned int packet_count = 0;
	unsigned int register_value = 0;
	char *input = NULL;
	char *data = NULL;
	unsigned char pbuf[3] = {0};
	u8 tx[10] = {0};
	unsigned int  i = 0, j = 0;
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	DISP_TIME_INFO("mipi_write_date source: count = %d,buf = %s ", (int)count, buf);

	input = buf;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	retval = kstrtou32(pbuf, 10, &read_enable);
	if (retval)
		goto exit;
	lcm_mipi_read_write.read_enable = !!read_enable;
	input = input + 3;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	packet_count = (unsigned int)string_to_hex(pbuf);
	if (lcm_mipi_read_write.read_enable && !packet_count) {
		retval = -EINVAL;
		goto exit;
	}
	input = input + 3;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	register_value = (unsigned int)string_to_hex(pbuf);
	lcm_mipi_read_write.lcm_setting_table.cmd = register_value;

	if(lcm_mipi_read_write.read_enable) {
		lcm_mipi_read_write.read_count = packet_count;

		cmd_msg->channel = 0;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->type[0] = 0x06;
		tx[0] = lcm_mipi_read_write.lcm_setting_table.cmd;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		cmd_msg->rx_cmd_num = 1;
		cmd_msg->rx_buf[0] = lcm_mipi_read_write.read_buffer;
		memset(cmd_msg->rx_buf[0], 0, lcm_mipi_read_write.read_count);
		cmd_msg->rx_len[0] = lcm_mipi_read_write.read_count;
		retval = mtk_ddic_dsi_read_cmd(cmd_msg);
		if (retval != 0) {
			pr_err("%s error\n", __func__);
		}

		pr_info("read lcm addr:%pad--dlen:%d\n",
			&(*(char *)(cmd_msg->tx_buf[0])), (int)cmd_msg->rx_len[0]);
		for (j = 0; j < cmd_msg->rx_len[0]; j++) {
			pr_info("read lcm addr:%pad--byte:%d,val:%pad 0x%02x\n",
				&(*(char *)(cmd_msg->tx_buf[0])), j,
				&(*(char *)(cmd_msg->rx_buf[0] + j)), *(unsigned char *)(cmd_msg->rx_buf[0] + j));
		}
		goto exit;
	} else {
		lcm_mipi_read_write.lcm_setting_table.count = (unsigned char)packet_count;
		memcpy(lcm_mipi_read_write.lcm_setting_table.para_list, "",64);
		if(count > 8)
		{
			data = kzalloc(count - 6, GFP_KERNEL);
			if (!data) {
				retval = -ENOMEM;
				goto exit;
			}
			data[count-6-1] = '\0';
			//input = input + 3;
			dlen = string_merge_into_buf(input,count -6,data);
			memcpy(lcm_mipi_read_write.lcm_setting_table.para_list, data,dlen);

			cmd_msg->channel = packet_count;
			cmd_msg->flags = 2;
			cmd_msg->tx_cmd_num = 1;
			cmd_msg->type[0] = 0x39;

			if (2 == dlen) {
				cmd_msg->type[0] = 0x15;
			} else if (1 == dlen) {
				cmd_msg->type[0] = 0x05;
			}

			cmd_msg->tx_buf[0] = data;
			cmd_msg->tx_len[0] = dlen;
			for (i = 0; i < (int)cmd_msg->tx_cmd_num; i++) {
				pr_debug("send lcm tx_len[%d]=%d\n",
					i, (int)cmd_msg->tx_len[i]);
				for (j = 0; j < (int)cmd_msg->tx_len[i]; j++) {
					pr_debug(
						"send lcm type[%d]=0x%x, tx_buf[%d]--byte:%d,val:%pad\n",
						i, cmd_msg->type[i], i, j,
						&(*(char *)(cmd_msg->tx_buf[i] + j)));
				}
			}

			mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
		}
	}

	pr_debug("[%s]: mipi_write done!\n", __func__);
	pr_debug("[%s]: write cmd = %d,len = %d\n", __func__,lcm_mipi_read_write.lcm_setting_table.cmd,lcm_mipi_read_write.lcm_setting_table.count);
	pr_debug("[%s]: mipi_write data: ", __func__);
	for(i=0; i<count-3; i++)
	{
		pr_debug("0x%x ", lcm_mipi_read_write.lcm_setting_table.para_list[i]);
	}
	pr_debug("\n ");

	if(count > 8)
	{
		kfree(data);
	}
exit:
	retval = count;
	vfree(cmd_msg);
	return retval;
}

ssize_t  dsi_panel_read_mipi_reg(char *buf)
{
	int i = 0;
	ssize_t count = 0;

	if (lcm_mipi_read_write.read_enable) {
		for (i = 0; i < lcm_mipi_read_write.read_count; i++) {
			if (i ==  lcm_mipi_read_write.read_count - 1) {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02x\n",
				     lcm_mipi_read_write.read_buffer[i]);
			} else {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02x ",
				     lcm_mipi_read_write.read_buffer[i]);
			}
		}
	}
	return count;
}

static int mi_dsi_panel_get_cmd_pkt_count(const char *data, u32 length, u32 *cnt)
{
	const u32 cmd_set_min_size = 7;
	u32 count = 0;
	u32 packet_length;
	u32 tmp;

	while (length >= cmd_set_min_size) {
		packet_length = cmd_set_min_size;
		tmp = ((data[5] << 8) | (data[6]));
		packet_length += tmp;
		if (packet_length > length) {
			DISP_ERROR("format error\n");
			return -EINVAL;
		}
		length -= packet_length;
		data += packet_length;
		count++;
	}

	*cnt = count;
	return 0;
}

static int mi_dsi_panel_create_cmd_packets(const char *data,
					u32 count,
					struct dsi_cmd_desc *cmd)
{
	int rc = 0;
	int i, j;
	u8 *payload;

	for (i = 0; i < count; i++) {
		u32 size;

		cmd[i].msg.type = data[0];
		cmd[i].last_command = (data[1] == 1);
		cmd[i].msg.channel = data[2];
		cmd[i].msg.flags |= data[3];
		cmd[i].post_wait_ms = data[4];
		cmd[i].msg.tx_len = ((data[5] << 8) | (data[6]));

		size = cmd[i].msg.tx_len * sizeof(u8);

		payload = kzalloc(size, GFP_KERNEL);
		if (!payload) {
			rc = -ENOMEM;
			goto error_free_payloads;
		}

		for (j = 0; j < cmd[i].msg.tx_len; j++)
			payload[j] = data[7 + j];

		cmd[i].msg.tx_buf = payload;
		data += (7 + cmd[i].msg.tx_len);
	}

	return rc;
error_free_payloads:
	for (i = i - 1; i >= 0; i--) {
		cmd--;
		kfree(cmd->msg.tx_buf);
	}

	return rc;
}

ssize_t mi_dsi_panel_enable_gir(struct mtk_dsi *dsi, char *buf)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	dsi->mi_cfg.gir_state = GIR_ON;
	dsi->mi_cfg.feature_val[DISP_FEATURE_GIR] = GIR_ON;
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->panel_set_gir_on)) {
		pr_info("%s panel_set_gir_on func not defined");
		return 0;
	} else {
		panel_ext->funcs->panel_set_gir_on(dsi->panel);
	}
	return 0;
}

ssize_t mi_dsi_panel_disable_gir(struct mtk_dsi *dsi, char *buf)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	dsi->mi_cfg.gir_state = GIR_OFF;
	dsi->mi_cfg.feature_val[DISP_FEATURE_GIR] = GIR_OFF;
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->panel_set_gir_off)) {
		pr_info("%s panel_set_gir_off func not defined");
		return 0;
	} else {
		panel_ext->funcs->panel_set_gir_off(dsi->panel);
	}
	return 0;
}

int mi_dsi_panel_get_gir_status(struct mtk_dsi *dsi)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	if (panel_ext && panel_ext->funcs && panel_ext->funcs->panel_get_gir_status) {
		return panel_ext->funcs->panel_get_gir_status(dsi->panel);
	} else {
		return dsi->mi_cfg.gir_state;
	}
	return -EINVAL;
}

ssize_t mi_dsi_panel_write_mipi_reg(char *buf)
{
	int rc = 0, read_length = 0, read_buffer_position = 0;
	u32 packet_count = 0;
	char *token, *input_copy, *input_dup = NULL;
	const char *delim = " ";
	char *buffer = NULL;
	u32 buf_size = 0;
	u32 tmp_data = 0;
	int i = 0, j = 0;
	char tx[10] = {0};
	char temp_read_buffer[10] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	struct dsi_cmd_desc *cmds = NULL;

	DISP_INFO("input buffer:{%s}\n", buf);

	input_copy = kstrdup(buf, GFP_KERNEL);
	if (!input_copy) {
		rc = -ENOMEM;
		goto exit;
	}

	input_dup = input_copy;
	/* removes leading and trailing whitespace from input_copy */
	input_copy = strim(input_copy);

	/* Split a string into token */
	token = strsep(&input_copy, delim);
	if (token) {
		rc = kstrtoint(token, 10, &tmp_data);
		if (rc) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit_free0;
		}
		lcm_mipi_read_write.read_enable = !!tmp_data;
	}

	/* Removes leading whitespace from input_copy */
	if (input_copy)
		input_copy = skip_spaces(input_copy);
	else
		goto exit_free0;

	token = strsep(&input_copy, delim);
	if (token) {
		rc = kstrtoint(token, 10, &tmp_data);
		if (rc) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit_free0;
		}
		if (tmp_data > sizeof(lcm_mipi_read_write.read_buffer)) {
			DISP_ERROR("read size exceeding the limit %d\n",
					sizeof(lcm_mipi_read_write.read_buffer));
			goto exit_free0;
		}
		lcm_mipi_read_write.read_count = tmp_data;
	}

	/* Removes leading whitespace from input_copy */
	if (input_copy)
		input_copy = skip_spaces(input_copy);
	else
		goto exit_free0;

	buffer = kzalloc(strlen(input_copy), GFP_KERNEL);
	if (!buffer) {
		rc = -ENOMEM;
		goto exit_free0;
	}

	token = strsep(&input_copy, delim);
	while (token) {
		rc = kstrtoint(token, 16, &tmp_data);
		if (rc) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit_free1;
		}
		DISP_DEBUG("buffer[%d] = 0x%02x\n", buf_size, tmp_data);
		buffer[buf_size++] = (tmp_data & 0xff);
		/* Removes leading whitespace from input_copy */
		if (input_copy) {
			input_copy = skip_spaces(input_copy);
			token = strsep(&input_copy, delim);
		} else {
			token = NULL;
		}
	}

	if (lcm_mipi_read_write.read_enable) {
		cmd_msg->channel = 0;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->type[0] = 0x06;
		tx[0] = buffer[7];
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		cmd_msg->rx_cmd_num = 1;

		if (lcm_mipi_read_write.read_count > 10) {
			read_length = lcm_mipi_read_write.read_count;
			read_buffer_position = 0;
			while (read_length > 0) {
				cmd_msg->rx_buf[0] = temp_read_buffer;
				memset(temp_read_buffer, 0, sizeof(temp_read_buffer));
				cmd_msg->rx_len[0] = read_length > 10 ? 10 : read_length;
				rc = mtk_ddic_dsi_read_cmd(cmd_msg);
				if (rc != 0) {
					pr_err("%s error\n", __func__);
				}
				memcpy(&lcm_mipi_read_write.read_buffer[read_buffer_position], temp_read_buffer, cmd_msg->rx_len[0]);
				read_buffer_position += cmd_msg->rx_len[0];
				read_length -= 10;
			}
		} else {
			cmd_msg->rx_buf[0] = lcm_mipi_read_write.read_buffer;
			memset(cmd_msg->rx_buf[0], 0, lcm_mipi_read_write.read_count);
			cmd_msg->rx_len[0] = lcm_mipi_read_write.read_count;
			rc = mtk_ddic_dsi_read_cmd(cmd_msg);
			if (rc != 0) {
				pr_err("%s error\n", __func__);
			}
			pr_info("read lcm addr:%pad--dlen:%d\n",
				&(*(char *)(cmd_msg->tx_buf[0])), (int)cmd_msg->rx_len[0]);
			for (j = 0; j < cmd_msg->rx_len[0]; j++) {
				pr_info("read lcm addr:%pad--byte:%d,val:%pad\n",
					&(*(char *)(cmd_msg->tx_buf[0])), j,
					&(*(char *)(cmd_msg->rx_buf[0] + j)));
			}
		}
		goto exit;
	} else {
		rc = mi_dsi_panel_get_cmd_pkt_count(buffer, buf_size, &packet_count);
			if (!packet_count) {
				DISP_ERROR("get pkt count failed!\n");
				goto exit_free1;
		}

		cmds = kzalloc(sizeof(struct dsi_cmd_desc) * packet_count, GFP_KERNEL);
		if (!cmds) {
			rc = -ENOMEM;
			goto exit_free0;
		}

		rc = mi_dsi_panel_create_cmd_packets(buffer, packet_count, cmds);
		if (rc) {
			DISP_ERROR("panel failed to create cmd packets, rc=%d\n", rc);
			goto exit_free2;
		}

		cmd_msg->channel = 0;
		cmd_msg->flags = MIPI_DSI_MSG_USE_LPM;
		for (i = 0; i < packet_count; i++) {
			cmd_msg->type[j] = cmds[i].msg.type;
			cmd_msg->tx_len[j] = cmds[i].msg.tx_len;
			cmd_msg->tx_buf[j] = cmds[i].msg.tx_buf;
			if (cmds[i].last_command) {
				cmd_msg->tx_cmd_num = j + 1;
				mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
				j = 0;
			} else
				j++;
		}

		for (i = 0; i < packet_count; i++) {
			pr_info("send lcm tx_len[%d]=%d\n",
				i, cmds[i].msg.tx_len);
			for (j = 0; j < cmds[i].msg.tx_len; j++) {
				pr_info(
					"send lcm type[%d]=0x%x, tx_buf[%d]--byte:%d,val:%x\n",
					i, cmds[i].msg.type, i, j,
					((char *)(cmds[i].msg.tx_buf))[j]);
			}
		}
	}

	rc = 0;

	for (i = 0; i < packet_count; i++) {
		kfree(cmds[i].msg.tx_buf);
	}

exit_free2:
	kfree(cmds);
exit_free1:
	kfree(buffer);
exit_free0:
	kfree(input_dup);
exit:
	vfree(cmd_msg);
	return rc;
}

ssize_t  mi_dsi_panel_read_mipi_reg(char *buf)
{
	int i = 0;
	ssize_t count = 0;
	DISP_INFO("lcm_mipi_read_write.read_count\n",lcm_mipi_read_write.read_count);
	if(buf == NULL) {
		DISP_INFO("buf == NULL\n");
		return 0;
	}

	if (lcm_mipi_read_write.read_enable) {
		for (i = 0; i < lcm_mipi_read_write.read_count; i++) {
			if (i == lcm_mipi_read_write.read_count - 1) {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02X\n",
				     lcm_mipi_read_write.read_buffer[i]);
			} else {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02X,",
				     lcm_mipi_read_write.read_buffer[i]);
			}
		}
	}
	return count;
}

ssize_t  mi_dsi_panel_read_mipi_reg_cp(char *exitbuf, char *enterbuf, int read_buf_pos)
{
	int i = 0;
	int j = 0;
	ssize_t count = 0;
	if(exitbuf == NULL || enterbuf == NULL) {
		DISP_INFO("buf == NULL\n");
		return 0;
	}

	DISP_INFO("lcm_mipi_read_write.read_count = %d\n",lcm_mipi_read_write.read_count);
	if (lcm_mipi_read_write.read_enable) {
		for (i = read_buf_pos; i < read_buf_pos + lcm_mipi_read_write.read_count; i++) {
			exitbuf[i] = lcm_mipi_read_write.read_buffer[j];
			enterbuf[i] = exitbuf[i];
			DISP_DEBUG(" exitbuf[%d] %x", i, exitbuf[i]);
			count++;
			j++;
		}

	}

	return count;
}

void  mi_dsi_panel_rewrite_enterDClut(char *exitDClut, char *enterDClut, int count)
{
	int i = 0, j = 0;
	DISP_DEBUG("mi_dsi_panel_rewrite_enterDClut +\n");
	if(exitDClut == NULL || enterDClut == NULL) {
		DISP_ERROR("buf == NULL\n");
		return;
	}

	DISP_DEBUG("mi_dsi_panel_rewrite_enterDClut count = %d\n", count);
	for (i = 0; i < count/5; i++) {
		for (j = i * 5; j < (i * 5 + 3) ; j++) {
			enterDClut[j] = exitDClut[i * 5 + 3];
		}
	}

	for (i = 0; i < count; i++)
		DISP_DEBUG(" enterDClut[%d] %x", i, enterDClut[i]);

	return;
}

static int mi_get_dc_lut(char* page4buf, char*offsetbuf, char *read_exitDClut, char* exitDClut, char* enterDClut, int read_buf_pos)
{
	ssize_t write_count = 0;
	ssize_t read_count = 0;

	DISP_DEBUG("+\n");
	write_count = dsi_panel_write_mipi_reg(page4buf, strlen(page4buf) + 1);
	if(!write_count) {
		DISP_ERROR("DC LUT switch to page4 failed\n");
		goto end;
	}
	write_count = dsi_panel_write_mipi_reg(offsetbuf, strlen(offsetbuf) + 1);
	if(!write_count) {
		DISP_ERROR("DC LUT switch to offsetbuf failed\n");
		goto end;
	}
	write_count = dsi_panel_write_mipi_reg(read_exitDClut, strlen(read_exitDClut) + 1);
	if(!write_count) {
		DISP_ERROR("DC LUT switch to read_exitDClut failed\n");
		goto end;
	}
	read_count = mi_dsi_panel_read_mipi_reg_cp(exitDClut, enterDClut, read_buf_pos);
	DISP_DEBUG("read_count = %d\n", read_count);
	if(!read_count) {
		DISP_ERROR("DC LUT read exitDClut failed\n");
		goto end;
	}
end:
	DISP_DEBUG("-\n");
	return read_count;
}

ssize_t mi_dsi_panel_read_and_update_dc_param(struct mtk_dsi *dsi)
{
	char page4buf[24] = {0};
	char offsetbuf[12] = {0};
	char read_exitDClut60[9] = {0};
	char read_exitDClut120[12] = {0};
	char exitDClut60[75] = {0};
	char enterDClut60[75] = {0};
	char exitDClut120[75] = {0};
	char enterDClut120[75] = {0};
	char read_buf[3] = {0};
	int count = 0;
	struct mtk_panel_ext *panel_ext = NULL;
	struct mtk_ddp_comp *comp =  NULL;

	int offset_count = 0;
	int read_length = 75;
	int len = 0;
	int read_buf_pos = 0;
	int read_offset = 0;

	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	DISP_INFO("+\n");

	strcpy(page4buf, "00 05 F0 55 AA 52 08 04");
	strcpy(offsetbuf, "00 01 6F 0A");
	strcpy(read_exitDClut60, "01 0A D2");
	strcpy(read_exitDClut120, "01 0A D4");

	while (read_length > 0) {
		len = read_length > 10 ? 10 : read_length;
		DISP_DEBUG("read_length = %d len = %d read_buf_pos = %d\n", read_length, len, read_buf_pos);
		offset_count = snprintf(offsetbuf + strlen(offsetbuf) - 2, 3, "%02x", read_offset);
		DISP_DEBUG("offsetbuf = %s strlen(offsetbuf) = %d offset_count = %d\n", offsetbuf, strlen(offsetbuf), offset_count);
		offset_count = snprintf(read_buf, 3, "%02x", len);
		DISP_DEBUG("read_buf = %s strlen(read_buf) = %d offset_count = %d len = %d\n", read_buf, strlen(read_buf), offset_count, len);
		read_exitDClut60[3] = read_buf[0];
		read_exitDClut60[4] = read_buf[1];
		read_exitDClut120[3] = read_buf[0];
		read_exitDClut120[4] = read_buf[1];
		DISP_DEBUG("read_exitDClut60 = %s strlen(read_exitDClut60) = %d offset_count = %d len = %d\n", read_exitDClut60, strlen(read_exitDClut60), offset_count, len);
		DISP_DEBUG("read_exitDClut120 = %s strlen(read_exitDClut120) = %d offset_count = %d len = %d\n", read_exitDClut120, strlen(read_exitDClut120), offset_count, len);
		count = mi_get_dc_lut(page4buf, offsetbuf, read_exitDClut60, exitDClut60, enterDClut60, read_buf_pos);
		if(count < 0){
			DISP_ERROR("mi_get_dc_lut 60hz count = %d\n", count);
			goto end;
		}
		count = mi_get_dc_lut(page4buf, offsetbuf, read_exitDClut120, exitDClut120, enterDClut120, read_buf_pos);
		if(count < 0){
			DISP_ERROR("mi_get_dc_lut 120hz count = %d\n", count);
			goto end;
		}
		read_buf_pos += len;
		read_length -= 10;
		read_offset += 10;
	}
	DISP_DEBUG("mi_dsi_panel_read_and_update_dc_param start read_buf_pos = %d 60hz\n", read_buf_pos);
	mi_dsi_panel_rewrite_enterDClut(exitDClut60, enterDClut60, read_buf_pos);
	DISP_DEBUG("mi_dsi_panel_read_and_update_dc_param start read_buf_pos = %d 120hz\n", read_buf_pos);
	mi_dsi_panel_rewrite_enterDClut(exitDClut120, enterDClut120, read_buf_pos);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->panel_set_dc_lut_params)) {
		pr_info("%s panel_set_dc_lut_params func not defined");
		goto end;
	} else {
		panel_ext->funcs->panel_set_dc_lut_params(dsi->panel, exitDClut60, enterDClut60, exitDClut120, enterDClut120, read_buf_pos);
	}
end:
	DISP_INFO("-\n");
	return 0;
}

int mi_dsi_panel_write_dsi_cmd(struct dsi_cmd_rw_ctl *ctl)
{
	int rc = 0;
	u32 packet_count = 0;
	int i = 0, j = 0;
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	struct dsi_cmd_desc *cmds = NULL;

	if (!ctl->tx_len || !ctl->tx_ptr) {
		DISP_ERROR("panel invalid params\n");
		rc = -EINVAL;
		goto exit;
	}

	rc = mi_dsi_panel_get_cmd_pkt_count(ctl->tx_ptr, ctl->tx_len, &packet_count);
		if (!packet_count) {
			DISP_ERROR("get pkt count failed!\n");
			goto exit;
	}

	cmds = kzalloc(sizeof(struct dsi_cmd_desc) * packet_count, GFP_KERNEL);
	if (!cmds) {
		rc = -ENOMEM;
		goto exit;
	}

	rc = mi_dsi_panel_create_cmd_packets(ctl->tx_ptr, packet_count, cmds);
	if (rc) {
		DISP_ERROR("panel failed to create cmd packets, rc=%d\n", rc);
		goto exit_free1;
	}

	cmd_msg->channel = 0;
	if (ctl->tx_state == MI_DSI_CMD_LP_STATE) {
		cmd_msg->flags = MIPI_DSI_MSG_USE_LPM;
	} else if (ctl->tx_state == MI_DSI_CMD_HS_STATE) {
		cmd_msg->flags = 0;
	} else {
		DISP_ERROR("panel command state unrecognized-%d\n", ctl->tx_state);
		goto exit_free2;
	}
	for (i = 0; i < packet_count; i++) {
		cmd_msg->type[j] = cmds[i].msg.type;
		cmd_msg->tx_len[j] = cmds[i].msg.tx_len;
		cmd_msg->tx_buf[j] = cmds[i].msg.tx_buf;
		if (cmds[i].last_command) {
			cmd_msg->tx_cmd_num = j + 1;
			mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
			j = 0;
		} else
			j++;
		}

	for (i = 0; i < packet_count; i++) {
		pr_debug("send lcm tx_len[%d]=%d\n",
			i, cmds[i].msg.tx_len);
		for (j = 0; j < cmds[i].msg.tx_len; j++) {
			pr_debug(
				"send lcm type[%d]=0x%x, tx_buf[%d]--byte:%d,val:%pad\n",
				i, cmds[i].msg.type, i, j,
				((char *)(cmds[i].msg.tx_buf))[j]);
		}
	}

exit_free2:
	if (ctl->tx_len && ctl->tx_ptr) {
		for (i = 0; i < packet_count; i++) {
			kfree(cmds[i].msg.tx_buf);
		}
	}
exit_free1:
	if (ctl->tx_len && ctl->tx_ptr) {
		kfree(cmds);
	}
exit:
	vfree(cmd_msg);
	return rc;
}

static int dsi_panel_get_lockdown_from_cmdline(unsigned char *plockdowninfo)
{
	int ret = -1;
	int i = 0;

	if (g_mi_cfg->lockdowninfo_read.lockdowninfo_read_done) {
		for(i = 0; i < 8; i++) {
			pr_info("panel lockdown 0x%x",  g_mi_cfg->lockdowninfo_read.lockdowninfo[i]);
			plockdowninfo[i] = g_mi_cfg->lockdowninfo_read.lockdowninfo[i];
		}
		ret = 0;
	} else if (sscanf(lockdown_info, "0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx",
		&plockdowninfo[0], &plockdowninfo[1], &plockdowninfo[2], &plockdowninfo[3],
		&plockdowninfo[4], &plockdowninfo[5], &plockdowninfo[6], &plockdowninfo[7]) != 9) {
		pr_err("failed to parse lockdown info from cmdline !\n");
	} else {
		pr_info("lockdown info from cmdline = 0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,"
			"0x%02hhx,0x%02hhx,0x%02hhx",
			plockdowninfo[0], plockdowninfo[1], plockdowninfo[2], plockdowninfo[3],
			plockdowninfo[4], plockdowninfo[5], plockdowninfo[6], plockdowninfo[7]);
		if (plockdowninfo[1] == 0x44 || plockdowninfo[1] == 0x36) {
			ret = 0;
		} else {
			ret = -1;
		}
	}
	return ret;
}

int get_lockdown_info_for_nvt(unsigned char* p_lockdown_info) {
	int ret = 0;
	int i = 0;

	/* CMD2 Page1 is selected */
	char select_page_cmd[] = {0xFF, 0x22};
	u8 tx[10] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg;

	if (!p_lockdown_info)
		return -EINVAL;

	if (!dsi_panel_get_lockdown_from_cmdline(p_lockdown_info))
		return 0;

	pr_info("panel vendor:0x%02hhx\n", p_lockdown_info[1]);

	cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (!cmd_msg)
		goto NOMEM;

	cmd_msg->channel = 1;
	cmd_msg->flags = 2;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x15;
	cmd_msg->tx_buf[0] = select_page_cmd;
	cmd_msg->tx_len[0] = 2;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE2;
	}

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	/* Read the first 1 byte of CMD2 page2 0x01 ~ 0x08 reg */
	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = 0x06;

	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_len[0] = 1;
	cmd_msg->rx_buf[0] = kzalloc(1 * sizeof(unsigned char), GFP_KERNEL);
	if (!cmd_msg->rx_buf[0]) {
		pr_err("%s: memory allocation failed\n", __func__);
		ret = -ENOMEM;
		goto DONE2;
	}

	for (i = 0; i < 8; i++) {
		if (p_lockdown_info[1] == 0x36)
		    tx[0] = 0x01 + i;
		else
		    tx[0] = 0x00 + i;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->tx_len[0] = 1;

		ret = mtk_ddic_dsi_read_cmd(cmd_msg);
		if (ret != 0) {
			pr_err("%s error\n", __func__);
			goto DONE1;
		}

		pr_info("read lcm addr:0x%x--byte:%d,val:0x%02hhx\n",
				*(unsigned char *)(cmd_msg->tx_buf[0]), i,
				*(unsigned char *)(cmd_msg->rx_buf[0]));
		p_lockdown_info[i] = *(unsigned char *)(cmd_msg->rx_buf[0]);
		g_mi_cfg->lockdowninfo_read.lockdowninfo[i] = *(unsigned char *)(cmd_msg->rx_buf[0]);
	}
	g_mi_cfg->lockdowninfo_read.lockdowninfo_read_done = true;

DONE1:
	kfree(cmd_msg->rx_buf[0]);
DONE2:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
	return ret;

NOMEM:
	pr_err("%s: memory allocation failed\n", __func__);
	return -ENOMEM;
}
EXPORT_SYMBOL(get_lockdown_info_for_nvt);

ssize_t  led_i2c_reg_write(struct mtk_dsi *dsi, char *buf, unsigned long  count)
{
	ssize_t retval = -EINVAL;
	unsigned int read_enable = 0;
	unsigned int packet_count = 0;
	char register_addr = 0;
	char *input = NULL;
	unsigned char pbuf[3] = {0};

	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	pr_info("%s +\n", __func__);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	pr_info("[%s], count  = %ld, buf = %s ", __func__, count, buf);

	if (count < 9 || buf == NULL) {
		/* 01 01 01      -- read 0x01 register, len:1*/
		/* 00 01 08 17 -- write 0x17 to 0x08 register,*/
		pr_info("[%s], command is invalid, count  = %ld,buf = %s ", __func__, count, buf);
		return retval;
	}

	input = buf;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	retval = kstrtou32(pbuf, 10, &read_enable);
	if (retval)
		return retval;
	lcm_led_i2c_read_write.read_enable = !!read_enable;
	input = input + 3;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	packet_count = (unsigned int)string_to_hex(pbuf);
	if (lcm_led_i2c_read_write.read_enable && !packet_count) {
		retval = -EINVAL;
		return retval;
	}
	input = input + 3;
	memcpy(pbuf, input, 2);
	pbuf[2] = '\0';
	register_addr = string_to_hex(pbuf);
	if (lcm_led_i2c_read_write.read_enable) {
		lcm_led_i2c_read_write.read_count = packet_count;
		memset(lcm_led_i2c_read_write.buffer, 0, sizeof(lcm_led_i2c_read_write.buffer));
		lcm_led_i2c_read_write.buffer[0] = (unsigned char)register_addr;
		if (panel_ext->funcs->led_i2c_reg_op)
			retval = panel_ext->funcs->led_i2c_reg_op(lcm_led_i2c_read_write.buffer,
							KTZ8866_REG_READ, lcm_led_i2c_read_write.read_count);
	} else {
		if (count < 12)
			return retval;

		memset(lcm_led_i2c_read_write.buffer, 0, sizeof(lcm_led_i2c_read_write.buffer));
		lcm_led_i2c_read_write.buffer[0] = (unsigned char)register_addr;
		input = input + 3;
		memcpy(pbuf, input, 2);
		pbuf[2] = '\0';
		lcm_led_i2c_read_write.buffer[1] = (unsigned char)string_to_hex(pbuf);

		if (panel_ext->funcs->led_i2c_reg_op)
			retval = panel_ext->funcs->led_i2c_reg_op(lcm_led_i2c_read_write.buffer, KTZ8866_REG_WRITE, 0);
	}

	return retval;
}

ssize_t  led_i2c_reg_read(struct mtk_dsi *dsi, char *buf)
{
	int i = 0;
	ssize_t count = 0;

	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	pr_info("%s +\n", __func__);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (lcm_led_i2c_read_write.read_enable) {
		for (i = 0; i < lcm_led_i2c_read_write.read_count; i++) {
			if (i ==  lcm_led_i2c_read_write.read_count - 1) {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02x\n",
				     lcm_led_i2c_read_write.buffer[i]);
			} else {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02x ",
				     lcm_led_i2c_read_write.buffer[i]);
			}
		}
	}
	return count;
}

int mi_dsi_panel_get_panel_info(struct mtk_dsi *dsi,
			char *buf)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	pr_info("%s +\n", __func__);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->get_panel_info)) {
		pr_info("%s get_panel_info func not defined", __func__);
		return 0;
	} else {
		return panel_ext->funcs->get_panel_info(dsi->panel, buf);
	}
}

int mi_dsi_panel_get_wp_info(struct mtk_dsi *dsi, char *buf, size_t size)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	int count = 0;

	pr_info("%s: +\n", __func__);

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		goto err;
	}

	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	if (panel_ext && panel_ext->funcs && panel_ext->funcs->get_wp_info) {
		count = panel_ext->funcs->get_wp_info(dsi->panel, buf, size);
	}

err:
	pr_info("%s: -\n", __func__);
	return count;
}

int mi_dsi_panel_get_fps(struct mtk_dsi *dsi,
			u32 *fps)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	pr_debug("%s +\n", __func__);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->get_panel_dynamic_fps)) {
		pr_info("%s get_panel_dynamic_fps func not defined", __func__);
		return 0;
	} else {
		return panel_ext->funcs->get_panel_dynamic_fps(dsi->panel, fps);
	}
}

int mi_dsi_panel_get_max_brightness_clone(struct mtk_dsi *dsi,
			u32 *max_brightness_clone)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	pr_info("%s +\n", __func__);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->get_panel_max_brightness_clone)) {
		pr_info("%s get_panel_max_brightness_clone func not defined", __func__);
		return -1;
	} else {
		return panel_ext->funcs->get_panel_max_brightness_clone(dsi->panel, max_brightness_clone);
	}
}


int mi_dsi_panel_get_thermal_dimming_support(struct mtk_dsi *dsi, bool *enabled)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	pr_info("%s +\n", __func__);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->get_panel_thermal_dimming_enable)) {
		pr_info("%s get_panel_thermal_dimming_enable func not defined", __func__);
		return -1;
	} else {
		return panel_ext->funcs->get_panel_thermal_dimming_enable(dsi->panel, enabled);
	}
}

int mi_dsi_panel_set_doze_brightness(struct mtk_dsi *dsi,
			int doze_brightness)
{
	int ret = 0;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	DISP_TIME_INFO("doze_brightness = %d\n", doze_brightness);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	mutex_lock(&dsi->dsi_lock);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->set_doze_brightness)) {
		pr_info("%s set_doze_brightness func not defined", __func__);
		ret = 0;
	} else {
#ifdef CONFIG_MI_DISP_FOD_SYNC
		if (panel_ext && panel_ext->params && panel_ext->params->aod_delay_enable) {
			if (doze_brightness && !(dsi->mi_cfg.aod_wait_frame)) {
				ret = wait_for_completion_timeout(&dsi->aod_wait_completion, msecs_to_jiffies(200));
				pr_info("aod wait_for_completion_timeout return %d\n", ret);
				dsi->mi_cfg.aod_wait_frame = true;
			}
		}
#endif

		if (!panel_ext->funcs->set_doze_brightness(dsi->panel, doze_brightness)) {
			mi_disp_feature_event_notify_by_type(mi_get_disp_id("primary"), MI_DISP_EVENT_DOZE, sizeof(doze_brightness), doze_brightness);
			dsi->mi_cfg.feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] = doze_brightness;
		}
	}

	mutex_unlock(&dsi->dsi_lock);
	return ret;
}

int mi_dsi_panel_get_doze_brightness(struct mtk_dsi *dsi,
			u32 *doze_brightness)
{
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	pr_info("%s +\n", __func__);
	mutex_lock(&dsi->dsi_lock);
	*doze_brightness = dsi->mi_cfg.feature_val[DISP_FEATURE_DOZE_BRIGHTNESS];
	mutex_unlock(&dsi->dsi_lock);
	return 0;
}

int mi_dsi_panel_set_brightness(struct mtk_dsi *dsi,
			int brightness)
{
	int ret = 0;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	DISP_TIME_INFO("brightness = %d\n", brightness);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	mutex_lock(&dsi->dsi_lock);
	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->setbacklight_control)) {
		pr_info("%s set_backlight_control func not defined");
		ret = 0;
	} else {
		if (!panel_ext->funcs->setbacklight_control(dsi->panel, brightness)) {
			mi_disp_feature_event_notify_by_type(mi_get_disp_id("primary"), MI_DISP_EVENT_DOZE, sizeof(brightness), brightness);
			dsi->mi_cfg.last_bl_level = brightness;
		}
	}

	mutex_unlock(&dsi->dsi_lock);
	return ret;
}

int mi_dsi_panel_get_brightness(struct mtk_dsi *dsi,
			u32 *brightness)
{
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	pr_info("%s +\n", __func__);
	mutex_lock(&dsi->dsi_lock);
	*brightness = dsi->mi_cfg.last_bl_level;
	mutex_unlock(&dsi->dsi_lock);
	return 0;
}

int mi_dsi_panel_set_disp_param(struct mtk_dsi *dsi, struct disp_feature_ctl *ctl)
{
	int rc = 0;
	bool need_lock = false;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	struct drm_panel *panel = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	struct mtk_drm_private *private = NULL;

	if (!dsi || !ctl) {
		pr_err("NULL dsi or ctl\n");
		return 0;
	}

	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	panel = dsi->panel;

	DISP_TIME_INFO("panel feature: %s, value: %d\n",
		get_disp_feature_id_name(ctl->feature_id), ctl->feature_val);

	if (!(panel_ext && panel_ext->funcs)) {
		pr_info("panel_ext func not defined\n");
		return 0;
	}

	if (!dsi_panel_initialized(dsi)) {
		DISP_ERROR("Panel not initialized!\n");
		return 0;
	}

	mi_cfg = &dsi->mi_cfg;

	if (dsi->encoder.crtc && dsi->encoder.crtc->dev && dsi->encoder.crtc->dev->dev_private)
		private = dsi->encoder.crtc->dev->dev_private;

	mutex_lock(&dsi->dsi_lock);
	switch (ctl->feature_id) {
	case DISP_FEATURE_DIMMING:
		if (!panel_ext->funcs->panel_elvss_control)
			break;
		if (ctl->feature_val == FEATURE_ON)
			panel_ext->funcs->panel_elvss_control(dsi->panel, true);
		else
			panel_ext->funcs->panel_elvss_control(dsi->panel, false);
		mi_cfg->feature_val[DISP_FEATURE_DIMMING] = ctl->feature_val;
		break;
	case DISP_FEATURE_HBM:
		mi_cfg->feature_val[DISP_FEATURE_HBM] = ctl->feature_val;
		mi_disp_feature_event_notify_by_type(mi_get_disp_id("primary"), MI_DISP_EVENT_HBM, sizeof(ctl->feature_val), ctl->feature_val);
		if (panel_ext->funcs->normal_hbm_control) {
			if (ctl->feature_val == FEATURE_ON) {
				panel_ext->funcs->normal_hbm_control(dsi->panel, 1);
				dsi->mi_cfg.normal_hbm_flag = true;
			} else {
				panel_ext->funcs->normal_hbm_control(dsi->panel, 0);
				dsi->mi_cfg.normal_hbm_flag = false;
			}
		}
		break;
	case DISP_FEATURE_HBM_FOD:
		mi_cfg->feature_val[DISP_FEATURE_HBM_FOD] = ctl->feature_val;
		if (panel_ext->funcs->hbm_fod_control) {
			if (ctl->feature_val == FEATURE_ON) {
				panel_ext->funcs->hbm_fod_control(dsi->panel, true);
			} else {
				panel_ext->funcs->hbm_fod_control(dsi->panel, false);
			}
		}
		break;
	case DISP_FEATURE_LOCAL_HBM:
		mi_cfg->feature_val[DISP_FEATURE_LOCAL_HBM] = ctl->feature_val;
		if (panel_ext->funcs->set_lhbm_fod)
			panel_ext->funcs->set_lhbm_fod(dsi, ctl->feature_val);
		break;
	case DISP_FEATURE_BRIGHTNESS:
		if (!panel_ext->funcs->setbacklight_control) {
			pr_info("%s setbacklight_control func not defined", __func__);
		} else {
			if (!panel_ext->funcs->setbacklight_control(dsi->panel, ctl->feature_val))
				dsi->mi_cfg.last_bl_level = ctl->feature_val;
		}
		break;
	case DISP_FEATURE_DOZE_BRIGHTNESS:
		DISP_INFO("DOZE BRIGHTNESS:%d\n", ctl->feature_val);
		if (!panel_ext->funcs->set_doze_brightness
			|| !panel_ext->funcs->doze_disable
			|| !panel_ext->funcs->doze_enable) {
			DISP_ERROR("doze brightness func not defined\n");
			break;
		}
		if (is_support_doze_brightness(ctl->feature_val)) {
			if (private)
				mutex_lock(&private->commit.lock);
			mtk_drm_idlemgr_kick(__func__, dsi->encoder.crtc, 0);
			if (ctl->feature_val == DOZE_TO_NORMAL && mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] != DOZE_TO_NORMAL) {
				panel_ext->funcs->doze_disable(dsi->panel, dsi, mipi_dsi_dcs_write_gce2, NULL);
			} else if (ctl->feature_val != DOZE_TO_NORMAL && mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] == DOZE_TO_NORMAL) {
				panel_ext->funcs->doze_enable(dsi->panel, dsi, mipi_dsi_dcs_write_gce2, NULL);
			}
			if (private)
				mutex_unlock(&private->commit.lock);
			panel_ext->funcs->set_doze_brightness(dsi->panel, ctl->feature_val);
			mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] = ctl->feature_val;
		} else
			DISP_ERROR("invaild doze brightness%d\n", ctl->feature_val);
		break;
	case DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS:
		if (ctl->feature_val == -1) {
		    DISP_INFO("FOD calibration brightness restore last_bl_level=%d\n",
			mi_cfg->last_bl_level);
		if (panel_ext->funcs->setbacklight_control)
			panel_ext->funcs->setbacklight_control(dsi->panel, mi_cfg->last_bl_level);
		    mi_cfg->in_fod_calibration = false;
		} else {
		    if (ctl->feature_val >= 0 &&
			ctl->feature_val <= 2047) {
			mi_cfg->in_fod_calibration = true;
					if (panel_ext->funcs->panel_elvss_control)
						panel_ext->funcs->panel_elvss_control(dsi->panel, false);
					if (panel_ext->funcs->setbacklight_control)
						panel_ext->funcs->setbacklight_control(dsi->panel, ctl->feature_val);
			mi_cfg->dimming_state = STATE_NONE;
		    } else {
			mi_cfg->in_fod_calibration = false;
			DISP_ERROR("FOD calibration invalid brightness level:%d\n",
				ctl->feature_val);
		    }
		}
		mi_cfg->feature_val[DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS] = ctl->feature_val;
		break;
	case DISP_FEATURE_FOD_CALIBRATION_HBM:
		if (ctl->feature_val == -1) {
			DISP_INFO("FOD calibration HBM restore last_bl_level=%d\n",
			    mi_cfg->last_bl_level);
		if (panel_ext->funcs->hbm_fod_control)
			panel_ext->funcs->hbm_fod_control(dsi->panel, false);
		mi_cfg->dimming_state = STATE_DIM_RESTORE;
		mi_cfg->in_fod_calibration = false;
		} else {
			mi_cfg->in_fod_calibration = true;
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
			if (panel_ext->funcs->hbm_fod_control)
				panel_ext->funcs->hbm_fod_control(dsi->panel, true);
		}
		mi_cfg->feature_val[DISP_FEATURE_FOD_CALIBRATION_HBM] = ctl->feature_val;
		break;
	case DISP_FEATURE_DC:
		if (!panel_ext->funcs->panel_set_dc)
			break;
		DISP_INFO("DC mode state:%d\n", ctl->feature_val);
		if (ctl->feature_val == FEATURE_ON){
			panel_ext->funcs->panel_set_dc(dsi->panel, true);
			need_lock = true;
#if IS_ENABLED(CONFIG_DRM_PANEL_L11_38_0A_0A_DSC_CMD) || IS_ENABLED(CONFIG_DRM_PANEL_L11A_38_0A_0A_DSC_CMD) || IS_ENABLED(CONFIG_DRM_PANEL_L2M_38_0A_0A_DSC_CMD)
#ifdef CONFIG_MI_DISP_SILKY_BRIGHTNESS_CRC
			mtk_ddp_comp_io_cmd(comp, NULL, MI_RESTORE_CRC_LEVEL, &need_lock);
			mtk_ddp_comp_io_cmd(comp, NULL, SET_DC_SYNC_TE_MODE_ON, NULL);
#endif
#endif
		} else {
			panel_ext->funcs->panel_set_dc(dsi->panel, false);
#if IS_ENABLED(CONFIG_DRM_PANEL_L11_38_0A_0A_DSC_CMD) || IS_ENABLED(CONFIG_DRM_PANEL_L11A_38_0A_0A_DSC_CMD) || IS_ENABLED(CONFIG_DRM_PANEL_L2M_38_0A_0A_DSC_CMD)
#ifdef CONFIG_MI_DISP_SILKY_BRIGHTNESS_CRC
			mtk_ddp_comp_io_cmd(comp, NULL, SET_DC_SYNC_TE_MODE_OFF, NULL);
#endif
#endif
		}
		mi_cfg->feature_val[DISP_FEATURE_DC] = ctl->feature_val;
		mi_disp_feature_event_notify_by_type(mi_get_disp_id("primary"), MI_DISP_EVENT_DC, sizeof(ctl->feature_val), ctl->feature_val);
		break;
	case DISP_FEATURE_CRC:
		DISP_INFO("CRC:%d\n", ctl->feature_val);
		if (mi_cfg->crc_state == ctl->feature_val) {
			DISP_INFO("CRC is the same, return\n");
			break;
		}

		if (mi_cfg->feature_val[DISP_FEATURE_BIST_MODE] == FEATURE_ON) {
			DISP_INFO("skip crc set due to Bist mode on\n");
			break;
		}
		switch (ctl->feature_val) {
		case CRC_SRGB:
		{
			if (!panel_ext->funcs->panel_set_crc_srgb)
				break;
			if(mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] != DOZE_TO_NORMAL){
				DISP_INFO("Has enter doze, crc return\n");
				break;
			}
			pr_info("CRC srgb");
			panel_ext->funcs->panel_set_crc_srgb(dsi->panel);
			break;
		}
		case CRC_P3:
		{
			if (!panel_ext->funcs->panel_set_crc_p3)
				break;
			pr_info("CRC p3");
			panel_ext->funcs->panel_set_crc_p3(dsi->panel);
			break;
		}
		case CRC_P3_D65:
		{
			if (!panel_ext->funcs->panel_set_crc_p3_d65)
				break;
			pr_info("CRC p3 d65");
			panel_ext->funcs->panel_set_crc_p3_d65(dsi->panel);
			break;
		}
		case CRC_P3_FLAT:
		{
			if (!panel_ext->funcs->panel_set_crc_p3_flat)
				break;
			pr_info("CRC p3 flat");
			panel_ext->funcs->panel_set_crc_p3_flat(dsi->panel);
			break;
		}
		case CRC_OFF:
		{
			if (!panel_ext->funcs->panel_set_crc_off)
				break;
			pr_info("CRC off");
			panel_ext->funcs->panel_set_crc_off(dsi->panel);
			break;
		}
		default:
			break;
		}
		mi_cfg->crc_state = ctl->feature_val;
		DISP_INFO(" CRC: end mi_cfg->crc_state = %d\n", mi_cfg->crc_state);
		break;
	case DISP_FEATURE_FLAT_MODE:
	case DISP_FEATURE_GIR:
		DISP_INFO("GIR:%d\n", ctl->feature_val);
		if (mi_cfg->gir_state == ctl->feature_val) {
			DISP_INFO("GIR is the same, return\n");
			break;
		}
		if(mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] != DOZE_TO_NORMAL){
			DISP_INFO("Has enter doze, gir return\n");
			break;
		}
		switch (ctl->feature_val) {
		case GIR_ON:
		{
			if (!panel_ext->funcs->panel_set_gir_on)
				break;
			pr_info("GIR on");
			panel_ext->funcs->panel_set_gir_on(dsi->panel);
			break;
		}
		case GIR_OFF:
		{
			if (!panel_ext->funcs->panel_set_gir_off)
				break;
			pr_info("GIR off");
			panel_ext->funcs->panel_set_gir_off(dsi->panel);
			break;
		}
		default:
			break;
		}
		mi_cfg->gir_state = ctl->feature_val;
		mi_cfg->feature_val[DISP_FEATURE_GIR] = ctl->feature_val;
		mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE] = ctl->feature_val;
		break;
	case DISP_FEATURE_FP_STATUS:
		DISP_INFO("DISP_FEATURE_FP_STATUS=%d\n", ctl->feature_val);
#ifdef CONFIG_MI_DISP_LHBM
		if (ctl->tx_len == 1) {
			mi_cfg->fod_low_brightness_allow = ctl->tx_ptr[0];
		}
		DISP_INFO("DISP_FEATURE_FP_STATUS=%s, fod_low_brightness_allow=%d\n",
			get_fingerprint_status_name(ctl->feature_val),
			mi_cfg->fod_low_brightness_allow);
		mi_disp_lhbm_fod_set_fingerprint_status(dsi, ctl->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] = ctl->feature_val;
		if (ctl->feature_val == ENROLL_STOP ||
			ctl->feature_val == AUTH_STOP ||
			ctl->feature_val == HEART_RATE_STOP) {
			if (mi_disp_lhbm_fod_enabled(dsi)) {
				mi_disp_set_fod_queue_work(FOD_EVENT_UP, false);
			}
		}
#endif
		break;
	case DISP_FEATURE_LCD_HBM:
		if(is_support_lcd_hbm_level(ctl->feature_val)) {
			DISP_INFO("DISP_FEATURE_LCD_HBM=%d\n", ctl->feature_val);
			if (panel_ext->funcs->normal_hbm_control) {
				panel_ext->funcs->normal_hbm_control(dsi->panel, ctl->feature_val);
				dsi->mi_cfg.normal_hbm_flag = ctl->feature_val == FEATURE_ON ?
					true:false;
			}
			mi_cfg->feature_val[DISP_FEATURE_LCD_HBM] = ctl->feature_val;
		} else
			DISP_ERROR("invaild lcd hbm level %d\n", ctl->feature_val);
		break;
	case DISP_FEATURE_BIST_MODE:
		DISP_INFO("DISP_FEATURE_BIST_MODE = %d\n", ctl->feature_val);
		if (panel_ext->funcs->panel_set_bist_enable) {
			panel_ext->funcs->panel_set_bist_enable(dsi->panel, ctl->feature_val);
			mi_cfg->feature_val[DISP_FEATURE_BIST_MODE] = ctl->feature_val;
		}
		break;
	case DISP_FEATURE_BIST_MODE_COLOR:
		if (ctl->tx_ptr && panel_ext->funcs->panel_set_bist_color) {
			if (ctl->tx_ptr[0] >= 0 && ctl->tx_ptr[0] <= 255
				&& ctl->tx_ptr[1] >= 0 && ctl->tx_ptr[1] <= 255
				&& ctl->tx_ptr[2] >= 0 && ctl->tx_ptr[2] <= 255) {
				panel_ext->funcs->panel_set_bist_color(dsi->panel, ctl->tx_ptr);
				DISP_INFO("set bist color to R:%d G:%d B:%d\n",
					ctl->tx_ptr[0], ctl->tx_ptr[1], ctl->tx_ptr[2]);
			}
		}
		break;
	case DISP_FEATURE_ROUND_MODE:
		DISP_INFO("DISP_FEATURE_ROUND_MODE = %d\n", ctl->feature_val);
		if (panel_ext->funcs->panel_set_round_enable) {
			panel_ext->funcs->panel_set_round_enable(dsi->panel, ctl->feature_val);
		}
		break;
	case DISP_FEATURE_SPR_RENDER:
		DISP_INFO("SPR:%d\n", ctl->feature_val);
		if (mi_cfg->feature_val[DISP_FEATURE_SPR_RENDER] == ctl->feature_val) {
			DISP_INFO("SPR is the same, return\n");
			break;
		}
		if (panel_ext->funcs->set_spr_status) {
			panel_ext->funcs->set_spr_status(dsi->panel, ctl->feature_val);
		}
		mi_cfg->feature_val[DISP_FEATURE_SPR_RENDER] = ctl->feature_val;
		break;
	default:
		DISP_ERROR("invalid feature id\n");
		break;
	}

	mutex_unlock(&dsi->dsi_lock);
	return rc;
}

ssize_t mi_dsi_panel_get_disp_param(struct mtk_dsi *dsi,
			char *buf, size_t size)
{
	struct mi_dsi_panel_cfg *mi_cfg;
	ssize_t count = 0;
	int i = 0;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EAGAIN;
	}

	mi_cfg = &dsi->mi_cfg;

	count = snprintf(buf, size, "%040s: feature value\n", "feature name[feature id]");

	for (i = DISP_FEATURE_DIMMING; i < DISP_FEATURE_MAX; i++) {
		count += snprintf(buf + count, size - count, "%036s[%02d]: %d\n",
				     get_disp_feature_id_name(i), i, mi_cfg->feature_val[i]);
	}

	return count;
}

void mi_dsi_panel_mi_cfg_state_update(struct mtk_dsi *dsi, int power_state)
{
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return;
	}

	mi_cfg = &dsi->mi_cfg;

	DISP_INFO("power state:%d\n", power_state);
	switch (power_state) {
	case MI_DISP_POWER_POWERDOWN:
		mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] = DOZE_TO_NORMAL;
		mi_cfg->feature_val[DISP_FEATURE_HBM] = FEATURE_OFF;
		mi_cfg->feature_val[DISP_FEATURE_DC] = FEATURE_OFF;
		mi_cfg->feature_val[DISP_FEATURE_DIMMING] = FEATURE_OFF;
		break;
	case MI_DISP_POWER_ON:
	case MI_DISP_POWER_LP1:
	case MI_DISP_POWER_LP2:
	default:
		break;
	}

	return;
}

bool is_aod_and_panel_initialized(struct mtk_dsi *dsi)
{
	if (!dsi) {
		pr_err("%s NULL dsi\n", __func__);
		return false;
	}

	if (dsi->output_en && dsi->doze_enabled)
		return true;
	return false;
}

void mi_disp_cfg_init(struct mtk_dsi *dsi)
{
	int ret = 0;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!dsi) {
		pr_err("%s NULL dsi\n", __func__);
		return;
	}
	mutex_init(&dsi->dsi_lock);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!(panel_ext && panel_ext->funcs)) {
		pr_info("panel_ext func not defined\n");
		return;
	}

	mi_cfg = &dsi->mi_cfg;
	g_mi_cfg = &dsi->mi_cfg;

#ifdef CONFIG_MI_DISP_FOD_SYNC
	mi_cfg->bl_enable = true;
	mi_cfg->bl_wait_frame = false;
	mi_cfg->aod_wait_frame = true;
	init_completion(&dsi->bl_wait_completion);
	init_completion(&dsi->aod_wait_completion);
#endif

	ret = mi_dsi_panel_get_thermal_dimming_support(dsi, &dsi->mi_cfg.thermal_dimming_enabled);
	DISP_INFO("thermal dimming enabled: %d\n", dsi->mi_cfg.thermal_dimming_enabled);

	ret = mi_dsi_panel_get_max_brightness_clone(dsi, &dsi->mi_cfg.max_brightness_clone);
	if (!ret) {
		dsi->mi_cfg.thermal_max_brightness_clone = dsi->mi_cfg.max_brightness_clone;
		DISP_INFO("max brightness clone:%d\n", dsi->mi_cfg.max_brightness_clone);
	} else {
		dsi->mi_cfg.max_brightness_clone = DEFAULT_MAX_BRIGHTNESS_CLONE;
		dsi->mi_cfg.thermal_max_brightness_clone = DEFAULT_MAX_BRIGHTNESS_CLONE;
		DISP_INFO("default max brightness clone:%d\n", dsi->mi_cfg.max_brightness_clone);
	}
	dsi->mi_cfg.real_brightness_clone = -1;

	return;
}

module_param_string(panel_opt, lockdown_info, MAX_CMDLINE_PARAM_LEN, 0600);
MODULE_PARM_DESC(panel_opt, "mediatek_drm.panel_opt=<panel_opt> while <panel_opt> is 'panel_opt' ");

