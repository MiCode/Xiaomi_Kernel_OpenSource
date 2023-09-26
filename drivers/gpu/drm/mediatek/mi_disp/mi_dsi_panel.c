/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"mi-dsi-panel:[%s:%d] " fmt, __func__, __LINE__
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/rtc.h>
#include <video/mipi_display.h>

//#include "dsi_panel.h"
//#include "dsi_display.h"
//#include "dsi_ctrl_hw.h"
//#include "dsi_parser.h"
#include "../../../../kernel/irq/internals.h"
#include "mi_disp_feature_id.h"
#include "mi_dsi_panel.h"
//#include "mi_dsi_panel_count.h"
//#include "xiaomi_frame_stat.h"
#include "mi_disp_feature.h"
#include "mtk_panel_ext.h"
#include "mi_disp_print.h"
//#include "mtk_drm_ddp_comp.h"
#include "mi_disp_calib.h"
static struct LCM_param_read_write lcm_param_read_write = {0};
struct LCM_mipi_read_write lcm_mipi_read_write = {0};
struct LCM_led_i2c_read_write lcm_led_i2c_read_write = {0};
//extern struct frame_stat fm_stat;
struct mi_disp_notifier g_notify_data;
int ishasCalibed = 0;

#define DEFAULT_MAX_BRIGHTNESS_CLONE 8191
#define DEFAULT_MAX_BRIGHTNESS  2047

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

#if defined (CONFIG_DRM_PANEL_K16_38_0C_0A_DSC_VDO) || defined (CONFIG_DRM_PANEL_K16_38_0E_0B_DSC_VDO)
bool is_backlight_set_block(struct mtk_dsi *dsi, u32 bl_lvl)
{
	if (!dsi->mi_cfg.bl_enable) {
		DISP_INFO("panel block set backlight %d due to bl_enable %d\n", bl_lvl,
				dsi->mi_cfg.bl_enable);
		return true;
	}
	return false;
}
#endif

bool dsi_panel_initialized(struct drm_panel *panel)
{
	return panel->panel_initialized;
}

void display_utc_time_marker(char *annotation)
{
	struct timespec ts;
	struct rtc_time tm;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	pr_info("%s: %d-%02d-%02d %02d:%02d:%02d.%09lu UTC\n",
		annotation, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
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
	pr_info("[%s]: mipi_write_date source: count = %d,buf = %s ", __func__, (int)count, buf);

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

			mtk_ddic_dsi_send_cmd(cmd_msg, true);
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

static int ch_check(char ch)
{
	int rc = -1;
	if ((ch >= '0' && ch <= '9')
		|| (ch >= 'A' && ch <= 'F')
		|| (ch >= 'a' && ch <= 'f'))
		rc = 0;

	return rc;
}

static int mi_read_calib_wp_info(void)
{
	int calib_status = 0;
	struct file *fp = NULL;
	mm_segment_t fs;
	loff_t pos = 0;
	int i = 0;
	int WP_NUM = 20;
	char ch[20] = {0};
	char wp_info[20] = {0};
	size_t wpinfo_size = 64;

	mi_panel_wpinfo_read(wp_info, wpinfo_size);

	fs = get_fs();
	set_fs(KERNEL_DS);

	pr_info("mi_read_calib_wp_info 0:calib_status = %d \n", calib_status);
	fp = filp_open("/mnt/vendor/persist/display/screenColorCal_panelInfo.txt", O_RDWR, 0644);
	if (IS_ERR_OR_NULL(fp)) {
		pr_err(" filp_open(/mnt/vendor/persist/display/screenColorCal_panelInfo.txt) falied\n");
		calib_status = 2;
		goto error;
	} else {
		pr_info("filp_open(/mnt/vendor/persist/display/screenColorCal_panelInfo.txt) success\n");
	}
	while (vfs_read(fp, &ch[i], 1, &pos) > 0) {
		pr_info("mi_read_calib_wp_info: pos = %ld ch[%d] = %c %x wp_info[%d] = %c %x\n", pos, i, ch[i], ch[i], i ,wp_info[i], wp_info[i]);
		if (ch_check(ch[i])) {
			continue;
			pr_info("mi_read_calib_wp_info: ch_check failed ch[%d] = %c %x\n", i, ch[i], ch[i]);
		}
		if(ch[i] == wp_info[i])
			pr_info("mi_read_calib_wp_info:ch[%d] == wp_info[%d] == %c\n", i, i, ch[i]);
		else {
			pr_info("mi_read_calib_wp_info:error ch[%d](%c) != wp_info[%d](%c) \n", i, ch[i], i, wp_info[i]);
			calib_status = 2;
		}
		if (i > WP_NUM-1)
			break;
		i++;
	}

	if (fp) {
		filp_close(fp, NULL);
		fp = NULL;
	}

error:
	set_fs(fs);
	return calib_status;
}

ssize_t mi_dsi_panel_enable_calib(struct mtk_dsi *dsi, char *buf)
{
	struct mtk_panel_ext *panel_ext = NULL;
	struct mtk_ddp_comp *comp =  NULL;
	int calib_status1 = 0;
	int calib_status2 = 0;
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	mutex_lock(&dsi->dsi_lock);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->panel_set_gir_on_read_B8reg)) {
		pr_info("%s panel_set_gir_on_read_B8reg func not defined");
		goto done;
	} else {
		panel_ext->funcs->panel_set_gir_on_read_B8reg();
	}
	calib_status1 = mi_read_calib_wp_info();
	calib_status2 = mi_read_initcode();
	ishasCalibed = calib_status1 + calib_status2;
	pr_info("mi_read_initcode end calib_status1 = %d calib_status2 = %d ishasCalibed = %d", calib_status1, calib_status2, ishasCalibed);

	mi_dsi_panel_read_and_update_dc_param(dsi);
	done:
		mutex_unlock(&dsi->dsi_lock);
		return 0;
}

ssize_t mi_dsi_panel_get_calib_status(char *buf){
	int count = sprintf(buf, "%d", ishasCalibed);
	pr_info("mi_dsi_panel_get_calib_status:%s count = %d\n", buf, count);
	return count;
}

ssize_t mi_dsi_panel_enable_gir(struct mtk_dsi *dsi, char *buf)
{
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	if (!dsi) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
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
#if !defined(CONFIG_DRM_PANEL_K16_38_0C_0A_DSC_VDO) && !defined(CONFIG_DRM_PANEL_K16_38_0E_0B_DSC_VDO)
		cmd_msg->flags = MIPI_DSI_MSG_USE_LPM;
#else
		if (buffer[3] == 0x00)
			cmd_msg->flags = MIPI_DSI_MSG_USE_LPM;
		else
			cmd_msg->flags = 0;
#endif
		for (i = 0; i < packet_count; i++) {
			cmd_msg->type[j] = cmds[i].msg.type;
			cmd_msg->tx_len[j] = cmds[i].msg.tx_len;
			cmd_msg->tx_buf[j] = cmds[i].msg.tx_buf;
			if (cmds[i].last_command) {
				cmd_msg->tx_cmd_num = j + 1;
				mtk_ddic_dsi_send_cmd(cmd_msg, true);
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
			mtk_ddic_dsi_send_cmd(cmd_msg, true);
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
	char lockdown_str[40] = {'\0'};
	char *match = (char *) strnstr(saved_command_line,
				"mtk_drm.panel_opt=",
				strlen(saved_command_line));

	if (match && plockdowninfo) {
		memcpy(lockdown_str, (match + strlen("panel_lockdown=")),
			sizeof(lockdown_str) - 1);
		if (sscanf(lockdown_str, "0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx",
				&plockdowninfo[0], &plockdowninfo[1], &plockdowninfo[2], &plockdowninfo[3],
				&plockdowninfo[4], &plockdowninfo[5], &plockdowninfo[6], &plockdowninfo[7])
					!= 8) {
			pr_err("failed to parse lockdown info from cmdline !\n");
		} else {
			pr_info("lockdown info from cmdline = 0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,"
					"0x%02hhx,0x%02hhx,0x%02hhx",
					plockdowninfo[0], plockdowninfo[1], plockdowninfo[2], plockdowninfo[3],
					plockdowninfo[4], plockdowninfo[5], plockdowninfo[6], plockdowninfo[7]);
			ret = 0;
		}
	}
	return ret;
}

int get_lockdown_info_for_nvt(unsigned char* p_lockdown_info) {
	int ret = 0;
	int i = 0;

	/* CMD2 Page1 is selected */
	char select_page_cmd[] = {0xFF, 0x21};
	u8 tx[10] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg;

	if (!p_lockdown_info)
		return -EINVAL;

	if (!dsi_panel_get_lockdown_from_cmdline(p_lockdown_info))
		return 0;

	cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (!cmd_msg)
		goto NOMEM;

	cmd_msg->channel = 1;
	cmd_msg->flags = 2;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x15;
	cmd_msg->tx_buf[0] = select_page_cmd;
	cmd_msg->tx_len[0] = 2;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE2;
	}

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	/* Read the first 8 byte of CMD2 page1 0xF1 reg */
	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = 0x06;
	tx[0] = 0xF1;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;

	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_buf[0] = kzalloc(8 * sizeof(unsigned char), GFP_KERNEL);
	if (!cmd_msg->rx_buf[0]) {
		pr_err("%s: memory allocation failed\n", __func__);
		ret = -ENOMEM;
		goto DONE2;
	}

	cmd_msg->rx_len[0] = 8;
	ret = mtk_ddic_dsi_read_cmd(cmd_msg);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE1;
	}

	for (i = 0; i < 8; i++) {
		pr_info("read lcm addr:0x%x--byte:%d,val:0x%02hhx\n",
				*(unsigned char *)(cmd_msg->tx_buf[0]), i,
				*(unsigned char *)(cmd_msg->rx_buf[0] + i));
		p_lockdown_info[i] = *(unsigned char *)(cmd_msg->rx_buf[0] + i);
	}

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

ssize_t  led_i2c_reg_write(struct drm_connector *connector, char *buf, unsigned long  count)
{
	ssize_t retval = -EINVAL;
	unsigned int read_enable = 0;
	unsigned int packet_count = 0;
	char register_addr = 0;
	char *input = NULL;
	unsigned char pbuf[3] = {0};

	struct mtk_dsi *dsi = NULL;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	pr_info("%s +\n", __func__);
	dsi = (struct mtk_dsi *)to_mtk_dsi(connector);
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
							LM36273_REG_READ, lcm_led_i2c_read_write.read_count);
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
			retval = panel_ext->funcs->led_i2c_reg_op(lcm_led_i2c_read_write.buffer, LM36273_REG_WRITE, 0);
	}

	return retval;
}

ssize_t  led_i2c_reg_read(struct drm_connector *connector, char *buf)
{
	int i = 0;
	ssize_t count = 0;

	struct mtk_dsi *dsi = NULL;
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	pr_info("%s +\n", __func__);
	dsi = (struct mtk_dsi *)to_mtk_dsi(connector);
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

int dsi_panel_get_wpinfo_from_cmdline(char *buf, size_t size)
{
	u8 read_data[8];
	char wp_and_maxlum_str[40] = {'\0'};
	char *match = (char *) strnstr(saved_command_line,
				"WpAndMaxlum:",
				strlen(saved_command_line));
	if (match) {
		memcpy(wp_and_maxlum_str, (match + strlen("WpAndMaxlum:")),
			sizeof(wp_and_maxlum_str) - 1);
		if (sscanf(wp_and_maxlum_str, "0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx",
				&read_data[0], &read_data[1], &read_data[2], &read_data[3], &read_data[4], &read_data[5], &read_data[6], &read_data[7]) != 8) {
			pr_err("failed to parse wp and lum info from cmdline !\n");
			return 0;
		} else {
			int count = snprintf(buf, size, "%02x%02x%02x%02x%02x%02x%02x%02x\n",
				read_data[0], read_data[1], read_data[2], read_data[3],
				read_data[4], read_data[5], read_data[6], read_data[7]);
			pr_info("read maxlum & wp: %hhu, %hhu, %hhu, %hhu, %hhu, %hhu, %hhu, %hhu\n",
				read_data[0], read_data[1], read_data[2], read_data[3],
				read_data[4], read_data[5], read_data[6], read_data[7]);
			return count;
		}
	}
	pr_err("failed to match wp and lum info from cmdline !\n");
	return 0;
}

int mi_panel_wpinfo_read(char *buf, size_t size)
{
	int ret = 0;
	u8 tx[10] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg;
	int count = 0;

	count = dsi_panel_get_wpinfo_from_cmdline(buf, size);
	if (count)
		return count;

	cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (!cmd_msg)
		goto NOMEM;

	/* Read the first 8 byte of CMD2 page1 0xF1 reg */
	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = 0x06; //DSI_DCS_READ_NO_PARAM
	tx[0] = 0xA3;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;

	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_len[0] = 8;
	cmd_msg->rx_buf[0] = kzalloc(cmd_msg->rx_len[0] * sizeof(unsigned char), GFP_KERNEL);
	if (!cmd_msg->rx_buf[0]) {
		pr_err("%s: memory allocation failed\n", __func__);
		count = 0;
		goto DONE2;
	}

	ret = mtk_ddic_dsi_read_cmd(cmd_msg);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE1;
	}
	count = snprintf(buf, size, "%02x%02x%02x%02x%02x%02x%02x%02x\n",
		*(unsigned char *)(cmd_msg->rx_buf[0]), *(unsigned char *)(cmd_msg->rx_buf[0] + 1),
		*(unsigned char *)(cmd_msg->rx_buf[0] + 2), *(unsigned char *)(cmd_msg->rx_buf[0] + 3),
		*(unsigned char *)(cmd_msg->rx_buf[0] + 4), *(unsigned char *)(cmd_msg->rx_buf[0] + 5),
		*(unsigned char *)(cmd_msg->rx_buf[0] + 6), *(unsigned char *)(cmd_msg->rx_buf[0] + 7));
	pr_info("read maxlum & wp:%d buf:%s %hhu, %hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu\n", count, buf,
		*(unsigned char *)(cmd_msg->rx_buf[0]), *(unsigned char *)(cmd_msg->rx_buf[0] + 1),
		*(unsigned char *)(cmd_msg->rx_buf[0] + 2), *(unsigned char *)(cmd_msg->rx_buf[0] + 3),
		*(unsigned char *)(cmd_msg->rx_buf[0] + 4), *(unsigned char *)(cmd_msg->rx_buf[0] + 5),
		*(unsigned char *)(cmd_msg->rx_buf[0] + 6), *(unsigned char *)(cmd_msg->rx_buf[0] + 7));

DONE1:
	kfree(cmd_msg->rx_buf[0]);
DONE2:
	vfree(cmd_msg);

	pr_debug("%s end -\n", __func__);
	return count;

NOMEM:
	pr_err("%s: memory allocation failed\n", __func__);
	return 0;
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

	pr_info("%s +, doze_brightness = %d\n", __func__, doze_brightness);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	mutex_lock(&dsi->dsi_lock);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->set_doze_brightness)) {
		pr_info("%s set_doze_brightness func not defined", __func__);
		ret = 0;
	} else {
		if (!panel_ext->funcs->set_doze_brightness(dsi->panel, doze_brightness))
			dsi->mi_cfg.feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] = doze_brightness;
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

#if defined (CONFIG_DRM_PANEL_K16_38_0C_0A_DSC_VDO) || defined (CONFIG_DRM_PANEL_K16_38_0E_0B_DSC_VDO)
	if (is_backlight_set_block(dsi, brightness)) {
		ret = wait_for_completion_timeout(&dsi->bl_wait_completion, msecs_to_jiffies(200));
		dsi->mi_cfg.bl_wait_frame = true;
		DISP_INFO("wait_for_completion_timeout return %d\n", ret);
	}
#endif

	pr_debug("%s +, brightness = %d\n", __func__, brightness);
	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

#if defined(CONFIG_DRM_PANEL_K10_42_02_0A_DSC_CMD)
	if ((2 >= dsi->mi_cfg.last_bl_level) && (DEFAULT_MAX_BRIGHTNESS < brightness)) {
		while(DEFAULT_MAX_BRIGHTNESS != dsi->mi_cfg.last_bl_level) {
			usleep_range(10 * 1000, 10 * 1000 + 10);
		}
	}
#endif

	mutex_lock(&dsi->dsi_lock);

	if (!(panel_ext && panel_ext->funcs && panel_ext->funcs->setbacklight_control)) {
		pr_info("%s setbacklight_control func not defined", __func__);
		ret = 0;
	} else {
		if (!panel_ext->funcs->setbacklight_control(dsi->panel, brightness)) {
			if (panel_ext->funcs->panel_pwm_demura_gain_update &&
				dsi->mi_cfg.feature_val[DISP_FEATURE_DC] == FEATURE_OFF) {
				if (brightness >= PANEL_PWM_DEMURA_BACKLIGHT_THRESHOLD
					&& dsi->mi_cfg.last_bl_level < PANEL_PWM_DEMURA_BACKLIGHT_THRESHOLD)
					panel_ext->funcs->panel_pwm_demura_gain_update(dsi->panel, 1);
				else if (brightness < PANEL_PWM_DEMURA_BACKLIGHT_THRESHOLD
					&& (dsi->mi_cfg.last_bl_level >= PANEL_PWM_DEMURA_BACKLIGHT_THRESHOLD
					|| dsi->mi_cfg.last_bl_level == 0))
					panel_ext->funcs->panel_pwm_demura_gain_update(dsi->panel, 0);
			}
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

static int dsi_panel_get_wp_or_laxlum_from_cmdline(u8 read_type)
{
	int ret = -1;
	u8 read_data[3];
	char wp_and_maxlum_str[16] = {'\0'};
	char *match = (char *) strnstr(saved_command_line,
				"WpAndMaxlum:",
				strlen(saved_command_line));

	if (match) {
		memcpy(wp_and_maxlum_str, (match + strlen("WpAndMaxlum:")),
			sizeof(wp_and_maxlum_str) - 1);
		if (sscanf(wp_and_maxlum_str, "0x%02hhx,0x%02hhx,0x%02hhx",
				&read_data[0], &read_data[1], &read_data[2]) != 3) {
			pr_err("failed to parse wp and lum info from cmdline !\n");
		} else {
			if (read_type == 0) {
				lcm_param_read_write.read_count = 2;
				lcm_param_read_write.read_buffer[0] = read_data[0];
				lcm_param_read_write.read_buffer[1] = read_data[1];
				pr_info("read wp: %hhu,%hhu\n", read_data[0], read_data[1]);
			} else {
				lcm_param_read_write.read_count = 1;
				lcm_param_read_write.read_buffer[0] = read_data[2];
				pr_info("read maxlum: %hhu\n", read_data[2]);
			}
			ret = 0;
		}
	}
	return ret;
}

static ssize_t panel_max_luminance_read(void)
{
	int ret = 0;
	/* CMD2 Page1 is selected */
	char select_page_cmd[] = {0xFF, 0x21};
	u8 tx[10] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg;

	if (!dsi_panel_get_wp_or_laxlum_from_cmdline(1))
		return 0;

	cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (!cmd_msg)
		goto NOMEM;

	cmd_msg->channel = 1;
	cmd_msg->flags = 2;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x15;
	cmd_msg->tx_buf[0] = select_page_cmd;
	cmd_msg->tx_len[0] = 2;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE2;
	}

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	/* Read the first 8 byte of CMD2 page1 0xF1 reg */
	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = 0x06; //DSI_DCS_READ_NO_PARAM
	tx[0] = 0xF2;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;

	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_buf[0] = kzalloc(3 * sizeof(unsigned char), GFP_KERNEL);
	if (!cmd_msg->rx_buf[0]) {
		pr_err("%s: memory allocation failed\n", __func__);
		ret = -ENOMEM;
		goto DONE2;
	}

	cmd_msg->rx_len[0] = 3;
	ret = mtk_ddic_dsi_read_cmd(cmd_msg);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE1;
	}

	lcm_param_read_write.read_count = 1;
	lcm_param_read_write.read_buffer[0] = *(unsigned char *)(cmd_msg->rx_buf[0] + 2);
	pr_info("read panel_max_luminance: %hhu\n", lcm_param_read_write.read_buffer[0]);

DONE1:
	kfree(cmd_msg->rx_buf[0]);
DONE2:
	vfree(cmd_msg);

	pr_debug("%s end -\n", __func__);
	return ret;

NOMEM:
	pr_err("%s: memory allocation failed\n", __func__);
	return -ENOMEM;
}

static ssize_t panel_xy_coordinate_read(void)
{
	int ret = 0;
	/* CMD2 Page1 is selected */
	char select_page_cmd[] = {0xFF, 0x21};
	u8 tx[10] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg;

	if (!dsi_panel_get_wp_or_laxlum_from_cmdline(0))
		return 0;

	cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (!cmd_msg)
		goto NOMEM;

	cmd_msg->channel = 1;
	cmd_msg->flags = 2;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x15;
	cmd_msg->tx_buf[0] = select_page_cmd;
	cmd_msg->tx_len[0] = 2;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE2;
	}

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	/* Read the first 8 byte of CMD2 page1 0xF1 reg */
	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = 0x06; //DSI_DCS_READ_NO_PARAM
	tx[0] = 0xF2;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;

	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_buf[0] = kzalloc(3 * sizeof(unsigned char), GFP_KERNEL);
	if (!cmd_msg->rx_buf[0]) {
		pr_err("%s: memory allocation failed\n", __func__);
		ret = -ENOMEM;
		goto DONE2;
	}

	cmd_msg->rx_len[0] = 3;
	ret = mtk_ddic_dsi_read_cmd(cmd_msg);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE1;
	}

	lcm_param_read_write.read_count = 2;
	lcm_param_read_write.read_buffer[0] = *(unsigned char *)(cmd_msg->rx_buf[0]);
	lcm_param_read_write.read_buffer[1] = *(unsigned char *)(cmd_msg->rx_buf[0]+1);
	pr_info("read panel_xy_coordinate: %hhu, %hhu\n",
				lcm_param_read_write.read_buffer[0],
				lcm_param_read_write.read_buffer[1]);

DONE1:
	kfree(cmd_msg->rx_buf[0]);
DONE2:
	vfree(cmd_msg);

	pr_debug("%s end -\n", __func__);
	return ret;

NOMEM:
	pr_err("%s: memory allocation failed\n", __func__);
	return -ENOMEM;
}

ssize_t dsi_panel_set_disp_param(struct drm_connector* connector, u32 cmd)
{
	struct mtk_dsi *dsi = container_of(connector, struct mtk_dsi, conn);
	struct mtk_panel_ext *panel_ext = dsi->ext;
	enum DISPPARAM_MODE cmd_temp;
	u32 fod_backlight = 0, params = 0;
	static u32 backlight_by_brightness = 0x40;
	#if 0
	if (!(comp->mtk_crtc->enabled)) {
		pr_info("Sleep State, return\n");
		return -EINVAL;
	}
	#endif

	pr_info("%s-%d:dsi = %p, cmd = 0x%x \n",__func__, __LINE__, dsi, cmd);
	mutex_lock(&dsi->dsi_lock);

	if (DISPPARAM_FOD_BACKLIGHT == (cmd & 0x0F00000)) {
		cmd_temp = DISPPARAM_FOD_BACKLIGHT;
	}

	cmd_temp = cmd & 0x0F;
	switch (cmd_temp) {
		case DISPPARAM_WHITEPOINT_XY:
		{
			pr_info("read xy coordinate\n");
			panel_xy_coordinate_read();
			break;
		}
		case DISPPARAM_MAX_LUMINANCE_READ:
		{
			pr_info("read max luminance nit value");
			panel_max_luminance_read();
			break;
		}
		default:
			break;
	}

	cmd_temp = cmd & 0x0F00;
	switch (cmd_temp) {
		case DISPPARAM_DIMMING_OFF:
		{
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_elvss_control))
				break;
			panel_ext->funcs->panel_elvss_control(dsi->panel, false);
			break;
		}

		case DISPPARAM_DIMMING:
		{
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_elvss_control))
				break;
			panel_ext->funcs->panel_elvss_control(dsi->panel, true);
			break;
		}
		default:
			break;
	}

	cmd_temp = cmd & 0x0F0000;
	switch (cmd_temp) {
		case DISPPARAM_DC_ON:
		{
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_set_dc))
				break;
			panel_ext->funcs->panel_set_dc(dsi->panel, true);
			dsi->dc_flag = true;
			pr_info("dc on\n");
			break;
		}

		case DISPPARAM_DC_OFF:
		{
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_set_dc))
				break;
			panel_ext->funcs->panel_set_dc(dsi->panel, false);
			dsi->dc_flag = false;
			pr_info("dc off\n");
			break;
		}

		case DISPPARAM_HBM_ON:
		{
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->hbm_fod_control))
				break;
			dsi->normal_hbm_flag = true;
			panel_ext->funcs->hbm_fod_control(dsi->panel, true);
			break;
		}

		case DISPPARAM_LCD_HBM_L1_ON:
		case DISPPARAM_LCD_HBM_L2_ON:
		case DISPPARAM_LCD_HBM_L3_ON:
		case DISPPARAM_LCD_HBM_OFF:
		{
			if (panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->normal_hbm_control) {
				panel_ext->funcs->normal_hbm_control(dsi->panel, cmd_temp);
				dsi->normal_hbm_flag = cmd_temp != DISPPARAM_LCD_HBM_OFF;
			}
			break;
		}

		case DISPPARAM_HBM_FOD_ON:
		{
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->hbm_fod_control))
				break;
			pr_info("fod_backlight_flag on\n");
			dsi->fod_backlight_flag = true;
			dsi->fod_hbm_flag = true;
			panel_ext->funcs->hbm_fod_control(dsi->panel, true);
			break;
		}

		case DISPPARAM_HBM_OFF:
		{
			dsi->normal_hbm_flag = false;
			if (dsi->fod_hbm_flag)
				break;
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->hbm_fod_control))
				break;
			dsi->normal_hbm_flag = false;
			panel_ext->funcs->hbm_fod_control(dsi->panel, false);
			break;
		}

		case DISPPARAM_HBM_FOD_OFF:
		{
			pr_info("fod_backlight_flag off\n");
			dsi->fod_backlight_flag = false;
			dsi->fod_hbm_flag = false;
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->hbm_fod_control))
				break;

			panel_ext->funcs->hbm_fod_control(dsi->panel, false);
			break;
		}
		default:
			break;
	}

	cmd_temp = cmd & 0x0F00000;
	switch (cmd_temp) {
		case DISPPARAM_SRGB:
		{
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_set_crc_srgb))
				break;
			pr_info("CRC srgb");
			panel_ext->funcs->panel_set_crc_srgb(dsi->panel);
			break;
		}

		case DISPPARAM_P3:
		{
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_set_crc_p3))
				break;
			pr_info("CRC p3");
			panel_ext->funcs->panel_set_crc_p3(dsi->panel);
			break;
		}

		case DISPPARAM_CRC_OFF:
		{
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_set_crc_off))
				break;
			pr_info("CRC off");
			panel_ext->funcs->panel_set_crc_off(dsi->panel);

			break;
		}

		case DISPPARAM_CRC_P3_D65:
		{
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_set_crc_p3_d65))
				break;
			pr_info("CRC p3  d65");
			panel_ext->funcs->panel_set_crc_p3_d65(dsi->panel);

			break;
		}

		case DISPPARAM_FOD_BACKLIGHT:
		{
			fod_backlight = cmd & 0x1FFF;
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->setbacklight_control))
				break;
			if (0x1000 == fod_backlight) {
				params = backlight_by_brightness;
			}
			else {
				params = (fod_backlight & 0x7ff) ;
			}
			pr_info("fod backlight = 0x%x \n", params);
			panel_ext->funcs->setbacklight_control(dsi->panel, params);

			break;
		}
		default:
			break;
	}

	cmd_temp = cmd & 0x0F000000;
	switch (cmd_temp) {
		case DISPPARAM_FOD_BACKLIGHT_ON:
		{
			pr_info("fod_backlight_flag on\n");
			dsi->fod_backlight_flag = true;
			break;
		}

		case DISPPARAM_FOD_BACKLIGHT_OFF:
		{
			pr_info("fod_backlight_flag false\n");
			dsi->fod_backlight_flag = false;
			break;
		}

		case DISPPARAM_BACKLIGHT_SET:
		{
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->setbacklight_control))
				break;

			backlight_by_brightness = params = (cmd & 0xfff);
			if (!backlight_by_brightness) {
				dsi->normal_hbm_flag = false;
			}
			pr_info("fod_backlight_flag = %d(%d), backlight = %d \n",
				dsi->fod_backlight_flag, dsi->normal_hbm_flag, params);
			panel_ext->funcs->setbacklight_control(dsi->panel, params);

			break;
		}

		case DISPPARAM_PANEL_ID_GET:
		{
			if (!(panel_ext && panel_ext->funcs &&
			      panel_ext->funcs->panel_id_get))
				break;

			panel_ext->funcs->panel_id_get(dsi->panel);
			break;
		}
		default:
			break;
	}

	mutex_unlock(&dsi->dsi_lock);
	return 0;
}

ssize_t dsi_panel_get_disp_param(char *buf)
{
	int i = 0;
	ssize_t count = 0;

	for (i = 0; i < lcm_param_read_write.read_count; i++) {
		count += snprintf(buf + count, PAGE_SIZE - count, "p%d=%hhu",
				i, lcm_param_read_write.read_buffer[i]);
	}
	return count;
}

int mi_dsi_panel_set_disp_param(struct mtk_dsi *dsi, struct disp_feature_ctl *ctl)
{
	int rc = 0;
#if defined (CONFIG_DRM_PANEL_K16_38_0C_0A_DSC_VDO) || defined (CONFIG_DRM_PANEL_K16_38_0E_0B_DSC_VDO)
	int notifier_event = 0;
#endif
	struct mtk_ddp_comp *comp =  NULL;
	struct mtk_panel_ext *panel_ext = NULL;
	struct drm_panel *panel = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	struct mtk_drm_private *private = NULL;
	struct disp_event event;

	if (!dsi || !ctl) {
		pr_err("NULL dsi or ctl\n");
		return 0;
	}

	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);
	panel = dsi->panel;

	DISP_UTC_INFO("panel feature: %s, value: %d\n",
		get_disp_feature_id_name(ctl->feature_id), ctl->feature_val);

	if (!(panel_ext && panel_ext->funcs)) {
		pr_info("panel_ext func not defined\n");
		return 0;
	}

	if (!panel->panel_initialized) {
		DISP_ERROR("Panel not initialized!\n");
		goto exit;
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
		if (!panel_ext->funcs->hbm_fod_control)
			break;
		if (ctl->feature_val == FEATURE_ON) {
			panel_ext->funcs->hbm_fod_control(dsi->panel, true);
		} else {
			if (mi_cfg->feature_val[DISP_FEATURE_HBM_FOD] == FEATURE_ON)
				break;
			panel_ext->funcs->hbm_fod_control(dsi->panel, false);
		}
		mi_cfg->feature_val[DISP_FEATURE_HBM] = ctl->feature_val;
		break;
	case DISP_FEATURE_BRIGHTNESS:
		if (!panel_ext->funcs->setbacklight_control) {
			pr_info("%s set_doze_brightness func not defined", __func__);
		} else {
			if (!panel_ext->funcs->setbacklight_control(dsi->panel, ctl->feature_val))
				dsi->mi_cfg.last_bl_level = ctl->feature_val;
		}
		break;
	case DISP_FEATURE_DOZE_BRIGHTNESS:
#if !defined(CONFIG_DRM_PANEL_K16_38_0C_0A_DSC_VDO) && !defined(CONFIG_DRM_PANEL_K16_38_0E_0B_DSC_VDO)
		if (!panel_ext->funcs->set_doze_brightness
		|| !panel_ext->funcs->doze_disable
		|| !panel_ext->funcs->doze_enable) {
#else
		if (!panel_ext->funcs->set_doze_brightness) {
#endif
			DISP_ERROR("doze brightness func not defined\n");
			break;
		}
		if (is_support_doze_brightness(ctl->feature_val)) {
			if (private)
				mutex_lock(&private->commit.lock);
			mtk_drm_idlemgr_kick(__func__, dsi->encoder.crtc, 0);
			if (ctl->feature_val == DOZE_TO_NORMAL) {
				panel_ext->funcs->doze_disable(dsi->panel, dsi, mipi_dsi_dcs_write_gce2, NULL);
			} else {
				panel_ext->funcs->doze_enable(dsi->panel, dsi, mipi_dsi_dcs_write_gce2, NULL);
			}
			if (private)
				mutex_unlock(&private->commit.lock);
			panel_ext->funcs->set_doze_brightness(dsi->panel, ctl->feature_val);
			mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] = ctl->feature_val;
		} else
			DISP_ERROR("invaild doze brightness%d\n", ctl->feature_val);
		break;
	case DISP_FEATURE_DC:
		if (!panel_ext->funcs->panel_set_dc)
			break;
		DISP_INFO("DC mode state:%d\n", ctl->feature_val);
		if (ctl->feature_val == FEATURE_ON){
			panel_ext->funcs->panel_set_dc(dsi->panel, true);
		} else {
			panel_ext->funcs->panel_set_dc(dsi->panel, false);
		}

		mi_cfg->feature_val[DISP_FEATURE_DC] = ctl->feature_val;

		event.disp_id = MI_DISP_PRIMARY;
		event.type = MI_DISP_EVENT_DC_STATUS;
		event.length = sizeof(mi_cfg->feature_val[DISP_FEATURE_DC]);
		mi_disp_feature_event_notify(&event, (u8 *)&(mi_cfg->feature_val[DISP_FEATURE_DC]));
		break;
	case DISP_FEATURE_CRC:
		DISP_INFO("CRC:%d\n", ctl->feature_val);
		if (mi_cfg->crc_state == ctl->feature_val) {
			DISP_INFO("CRC is the same, return\n");
			break;
		}
#if !defined(CONFIG_DRM_PANEL_K16_38_0C_0A_DSC_VDO) && !defined(CONFIG_DRM_PANEL_K16_38_0E_0B_DSC_VDO)
		if (mi_cfg->feature_val[DISP_FEATURE_BIST_MODE] == FEATURE_ON) {
			DISP_INFO("skip crc set due to Bist mode on\n");
			break;
		}
#endif
		switch (ctl->feature_val) {
		case CRC_SRGB:
		{
			if (!panel_ext->funcs->panel_set_crc_srgb)
				break;
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
		mi_cfg->feature_val[DISP_FEATURE_CRC] = ctl->feature_val;
		DISP_INFO(" CRC: end mi_cfg->crc_state = %d\n", mi_cfg->crc_state);
		break;
	case DISP_FEATURE_GIR:
		DISP_INFO("GIR:%d\n", ctl->feature_val);
		if (mi_cfg->gir_state == ctl->feature_val) {
			DISP_INFO("GIR is the same, return\n");
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
		break;
	case DISP_FEATURE_FP_STATUS:
		DISP_INFO("DISP_FEATURE_FP_STATUS=%d\n", ctl->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] = ctl->feature_val;
		break;
	case DISP_FEATURE_LCD_HBM:
		if(is_support_lcd_hbm_level(ctl->feature_val)) {
			DISP_INFO("DISP_FEATURE_LCD_HBM=%d\n", ctl->feature_val);
			if (panel_ext->funcs->normal_hbm_control) {
				panel_ext->funcs->normal_hbm_control(dsi->panel, ctl->feature_val);
			}
			mi_cfg->feature_val[DISP_FEATURE_LCD_HBM] = ctl->feature_val;
		} else
			DISP_ERROR("invaild lcd hbm level %d\n", ctl->feature_val);
		break;
#if !defined(CONFIG_DRM_PANEL_K16_38_0C_0A_DSC_VDO) && !defined(CONFIG_DRM_PANEL_K16_38_0E_0B_DSC_VDO)
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
#else
	case DISP_FEATURE_DOZE_STATE:
		if (is_support_doze_brightness(ctl->feature_val)) {
			DISP_INFO("DISP_FEATURE_DOZE_STATE=%d\n", ctl->feature_val);
			if (ctl->feature_val == MI_DISP_DPMS_ON) {
				notifier_event = MI_DISP_DPMS_EVENT;
			} else {
				notifier_event = MI_DISP_DPMS_EARLY_EVENT;
				mi_cfg->bl_enable = false;
				mi_cfg->bl_wait_frame = false;
				reinit_completion(&dsi->bl_wait_completion);
			}
			g_notify_data.data = &ctl->feature_val;
			g_notify_data.disp_id = MI_DISPLAY_PRIMARY;
			mi_disp_notifier_call_chain(notifier_event, &g_notify_data);
			mi_cfg->feature_val[DISP_FEATURE_DOZE_STATE] = ctl->feature_val;
		} else
			DISP_ERROR("invaild doze state %d\n", ctl->feature_val);
		break;
#endif
	default:
		DISP_ERROR("invalid feature id\n");
		break;
	}

exit:
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

	count = snprintf(buf, size, "%040s: feature vaule\n", "feature name[feature id]");

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

	comp = &dsi->ddp_comp;
	panel_ext = mtk_dsi_get_panel_ext(comp);

	if (!(panel_ext && panel_ext->funcs)) {
		pr_info("panel_ext func not defined\n");
		return;
	}

	mi_cfg = &dsi->mi_cfg;

	ret = mi_dsi_panel_get_max_brightness_clone(dsi, &dsi->mi_cfg.max_brightness_clone);
	if (!ret) {
		dsi->mi_cfg.thermal_max_brightness_clone = dsi->mi_cfg.max_brightness_clone;
		DISP_INFO("max brightness clone:%d\n", dsi->mi_cfg.max_brightness_clone);
	} else {
		dsi->mi_cfg.max_brightness_clone = DEFAULT_MAX_BRIGHTNESS_CLONE;
		dsi->mi_cfg.thermal_max_brightness_clone = DEFAULT_MAX_BRIGHTNESS_CLONE;
		DISP_INFO("default max brightness clone:%d\n", dsi->mi_cfg.max_brightness_clone);
	}

	return;
}
