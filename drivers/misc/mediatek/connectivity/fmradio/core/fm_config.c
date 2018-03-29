/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/string.h>
#include <linux/slab.h>

#include "fm_typedef.h"
#include "fm_rds.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_stdlib.h"
#include "fm_patch.h"
#include "fm_config.h"

fm_cust_cfg fm_config;
static fm_u16 g_fm_chipid;
static enum fm_cfg_chip_type g_fm_chip_type = FM_CHIP_TYPE_MAX;

#define FM_CUST_CFG_PATH "fm_cust.cfg"

fm_s32 to_upper_n(fm_s8 *str, fm_s32 len)
{
	fm_s32 i = 0;

	for (i = 0; i < len; i++) {
		if (('a' <= str[i]) && (str[i] <= 'z'))
			str[i] = str[i] - ('a' - 'A');
	}

	return 0;
}

fm_s32 check_hex_str(fm_s8 *str, fm_s32 len)
{
	fm_s32 i = 0;

	for (i = 0; i < len; i++) {
		if ((('a' <= str[i]) && (str[i] <= 'z')) || (('A' <= str[i]) && (str[i] <= 'Z'))
		    || (('0' <= str[i]) && (str[i] <= '9'))) {
			;
		} else {
			return -1;
		}
	}

	return 0;
}

fm_s32 check_dec_str(fm_s8 *str, fm_s32 len)
{
	fm_s32 i = 0;

	for (i = 0; i < len; i++) {
		if (('0' <= str[i]) && (str[i] <= '9'))
			;
		else
			return -1;
	}

	return 0;
}

fm_s32 ascii_to_hex(fm_s8 *in_ascii, fm_u16 *out_hex)
{
	fm_s32 len = (fm_s32) strlen(in_ascii);
	int i = 0;
	fm_u16 tmp;

	len = (len > 4) ? 4 : len;

	if (check_hex_str(in_ascii, len))
		return -1;

	to_upper_n(in_ascii, len);
	*out_hex = 0;

	for (i = 0; i < len; i++) {
		if (in_ascii[len - i - 1] < 'A') {
			tmp = in_ascii[len - i - 1];
			*out_hex |= ((tmp - '0') << (4 * i));
		} else {
			tmp = in_ascii[len - i - 1];
			*out_hex |= ((tmp - 'A' + 10) << (4 * i));
		}
	}

	return 0;
}

fm_s32 ascii_to_dec(fm_s8 *in_ascii, fm_s32 *out_dec)
{
	fm_s32 len = (fm_s32) strlen(in_ascii);
	int i = 0;
	int flag;
	int multi = 1;

	len = (len > 10) ? 10 : len;

	if (in_ascii[0] == '-') {
		flag = -1;
		in_ascii += 1;
		len -= 1;
	} else {
		flag = 1;
	}

	if (check_dec_str(in_ascii, len))
		return -1;

	*out_dec = 0;
	multi = 1;

	for (i = 0; i < len; i++) {
		*out_dec += ((in_ascii[len - i - 1] - '0') * multi);
		multi *= 10;
	}

	*out_dec *= flag;
	return 0;
}

fm_s32 trim_string(fm_s8 **start)
{
	fm_s8 *end = *start;

	/* Advance to non-space character */
	while (*(*start) == ' ')
		(*start)++;

	/* Move to end of string */
	while (*end != '\0')
		(end)++;

	/* Backup to non-space character */
	do {
		end--;
	} while ((end >= *start) && (*end == ' '));

	/* Terminate string after last non-space character */
	*(++end) = '\0';
	return end - *start;
}

fm_s32 trim_path(fm_s8 **start)
{
	fm_s8 *end = *start;

	while (*(*start) == ' ')
		(*start)++;

	while (*end != '\0')
		(end)++;

	do {
		end--;
	} while ((end >= *start) && ((*end == ' ') || (*end == '\n') || (*end == '\r')));

	*(++end) = '\0';
	return end - *start;
}

fm_s32 cfg_parser(fm_s8 *buffer, CFG_HANDLER handler, fm_cust_cfg *cfg)
{
	fm_s32 ret = 0;
	fm_s8 *p = buffer;
	fm_s8 *group_start = NULL;
	fm_s8 *key_start = NULL;
	fm_s8 *value_start = NULL;

	enum fm_cfg_parser_state state = FM_CFG_STAT_NONE;

	if (p == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	for (p = buffer; *p != '\0'; p++) {
		switch (state) {
		case FM_CFG_STAT_NONE:{
				if (*p == '[') {
					/* if we get char '[' in none state, it means a new group name start */
					state = FM_CFG_STAT_GROUP;
					group_start = p + 1;
				} else if (*p == COMMENT_CHAR) {
					/* if we get char '#' in none state, it means a new comment start */
					state = FM_CFG_STAT_COMMENT;
				} else if (!isspace(*p) && (*p != '\n') && (*p != '\r')) {
					/* if we get an nonspace char in none state, it means a new key start */
					state = FM_CFG_STAT_KEY;
					key_start = p;
				}

				break;
			}
		case FM_CFG_STAT_GROUP:{
				if (*p == ']') {
					/* if we get char ']' in group state, it means a group name complete */
					*p = '\0';
					/* FIX_ME */
					/* record group name */
					state = FM_CFG_STAT_NONE;
					trim_string(&group_start);
					/* WCN_DBG(FM_NTC|MAIN, "g=%s\n", group_start); */
				}

				break;
			}
		case FM_CFG_STAT_COMMENT:{
				if (*p == '\n') {
					/* if we get char '\n' in comment state, it means new line start */
					state = FM_CFG_STAT_NONE;
					group_start = p + 1;
				}

				break;
			}
		case FM_CFG_STAT_KEY:{
				if (*p == DELIMIT_CHAR) {
					/* if we get char '=' in key state, it means a key name complete */
					*p = '\0';
					/* FIX_ME */
					/* record key name */
					state = FM_CFG_STAT_VALUE;
					value_start = p + 1;
					trim_string(&key_start);
					/* WCN_DBG(FM_NTC|MAIN, "k=%s\n", key_start); */
				}

				break;
			}
		case FM_CFG_STAT_VALUE:{
				if (*p == '\n' || *p == '\r') {
					/* if we get char '\n' or '\r' in value state, it means a value complete */
					*p = '\0';
					/* record value */
					trim_string(&value_start);
					/* WCN_DBG(FM_NTC|MAIN, "v=%s\n", value_start); */

					if (handler)
						ret = handler(group_start, key_start, value_start, cfg);

					state = FM_CFG_STAT_NONE;
				}

				break;
			}
		default:
			break;
		}
	}

	return ret;
}

fm_s32 cfg_item_match(fm_s8 *src_key, fm_s8 *src_val, fm_s8 *dst_key, fm_s32 *dst_val)
{
	fm_s32 ret = 0;
	fm_u16 tmp_hex;
	fm_s32 tmp_dec;

	/* WCN_DBG(FM_NTC|MAIN,"src_key=%s,src_val=%s\n", src_key,src_val); */
	/* WCN_DBG(FM_NTC|MAIN,"dst_key=%s\n", dst_key); */
	if (strcmp(src_key, dst_key) == 0) {
		if (strncmp(src_val, "0x", strlen("0x")) == 0) {
			src_val += strlen("0x");
			/* WCN_DBG(FM_NTC|MAIN,"%s\n", src_val); */
			ret = ascii_to_hex(src_val, &tmp_hex);

			if (!ret) {
				*dst_val = tmp_hex;
				/* WCN_DBG(FM_NTC|MAIN, "%s 0x%04x\n", dst_key, tmp_hex); */
				return 0;
			}
			/* WCN_DBG(FM_ERR | MAIN, "%s format error\n", dst_key); */
			return 1;
		}

		ret = ascii_to_dec(src_val, &tmp_dec);

		if (!ret /*&& ((0 <= tmp_dec) && (tmp_dec <= 0xFFFF)) */) {
			*dst_val = tmp_dec;
			/* WCN_DBG(FM_NTC|MAIN, "%s %d\n", dst_key, tmp_dec); */
			return 0;
		}
		/* WCN_DBG(FM_ERR | MAIN, "%s format error\n", dst_key); */
		return 1;
	}
	/* else */
	/* { */
	/* WCN_DBG(FM_ERR | MAIN, "src_key!=dst_key\n"); */
	/* } */

	return -1;
}

static fm_s32 cfg_item_handler(fm_s8 *grp, fm_s8 *key, fm_s8 *val, fm_cust_cfg *cfg)
{
	fm_s32 ret = 0;
	struct fm_rx_cust_cfg *rx_cfg = &cfg->rx_cfg;
	struct fm_tx_cust_cfg *tx_cfg = &cfg->tx_cfg;


	ret = cfg_item_match(key, val, "FM_RX_RSSI_TH_LONG", &rx_cfg->long_ana_rssi_th);
	if (0 <= ret)
		return ret;

	ret = cfg_item_match(key, val, "FM_RX_RSSI_TH_SHORT", &rx_cfg->short_ana_rssi_th);
	if (0 <= ret)
		return ret;

	ret = cfg_item_match(key, val, "FM_RX_DESENSE_RSSI", &rx_cfg->desene_rssi_th);
	if (0 <= ret)
		return ret;

	ret = cfg_item_match(key, val, "FM_RX_SMG_TH", &rx_cfg->smg_th);
	if (0 <= ret)
		return ret;

	ret = cfg_item_match(key, val, "FM_RX_DEEMPHASIS", &rx_cfg->deemphasis);
	if (0 <= ret)
		return ret;

	ret = cfg_item_match(key, val, "FM_RX_OSC_FREQ", &rx_cfg->osc_freq);
	if (0 <= ret)
		return ret;

	ret = cfg_item_match(key, val, "FM_RX_PAMD_TH", &rx_cfg->pamd_th);
	if (0 <= ret)
		return ret;

	ret = cfg_item_match(key, val, "FM_RX_MR_TH", &rx_cfg->mr_th);
	if (0 <= ret)
		return ret;

	ret = cfg_item_match(key, val, "FM_RX_ATDC_TH", &rx_cfg->atdc_th);
	if (0 <= ret)
		return ret;

	ret = cfg_item_match(key, val, "FM_RX_PRX_TH", &rx_cfg->prx_th);
	if (0 <= ret)
		return ret;

	ret = cfg_item_match(key, val, "FM_TX_PAMD_TH", &tx_cfg->pamd_th);
	if (0 <= ret)
		return ret;

	ret = cfg_item_match(key, val, "FM_TX_MR_TH", &tx_cfg->mr_th);
	if (0 <= ret)
		return ret;

	ret = cfg_item_match(key, val, "FM_TX_SMG_TH", &tx_cfg->smg_th);
	if (0 <= ret)
		return ret;


	WCN_DBG(FM_WAR | MAIN, "invalid key\n");
	return -1;
}

static fm_s32 fm_cust_config_default(fm_cust_cfg *cfg)
{
	if (cfg == NULL) {
		WCN_DBG(FM_ERR | MAIN, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/*RX Threshold config*/
	cfg->rx_cfg.long_ana_rssi_th = FM_RX_RSSI_TH_LONG;
	cfg->rx_cfg.short_ana_rssi_th = FM_RX_RSSI_TH_SHORT;
	cfg->rx_cfg.desene_rssi_th = FM_RX_DESENSE_RSSI;
	cfg->rx_cfg.pamd_th = FM_RX_PAMD_TH;
	cfg->rx_cfg.mr_th = FM_RX_MR_TH;
	cfg->rx_cfg.atdc_th = FM_RX_ATDC_TH;
	cfg->rx_cfg.prx_th = FM_RX_PRX_TH;
	cfg->rx_cfg.smg_th = FM_RX_SMG_TH;
	cfg->rx_cfg.deemphasis = FM_RX_DEEMPHASIS;
	cfg->rx_cfg.osc_freq = FM_RX_OSC_FREQ;

	/*TX Threshold config*/
	cfg->tx_cfg.pamd_th = FM_TX_PAMD_TH;
	cfg->tx_cfg.mr_th = FM_TX_MR_TH;
	cfg->tx_cfg.smg_th = FM_TX_SMG_TH;

	/*Audio path config*/
	if (g_fm_chip_type == FM_COMBO_CHIP) {
		/* combo chip config MT6630,MT6632 */
#ifdef CONFIG_MTK_MERGE_INTERFACE_SUPPORT
		cfg->aud_cfg.aud_path = FM_AUD_MRGIF;
		cfg->aud_cfg.i2s_info.status = FM_I2S_OFF;
		cfg->aud_cfg.i2s_info.mode = FM_I2S_SLAVE;
		cfg->aud_cfg.i2s_info.rate = FM_I2S_44K;
		cfg->aud_cfg.i2s_pad = FM_I2S_PAD_IO;
#elif defined FM_DIGITAL_INPUT
		cfg->aud_cfg.aud_path = FM_AUD_I2S;
		cfg->aud_cfg.i2s_info.status = FM_I2S_OFF;
		cfg->aud_cfg.i2s_info.mode = FM_I2S_SLAVE;
		cfg->aud_cfg.i2s_info.rate = FM_I2S_44K;
		cfg->aud_cfg.i2s_pad = FM_I2S_PAD_IO;
#elif defined FM_ANALOG_INPUT
		cfg->aud_cfg.aud_path = FM_AUD_ANALOG;
		cfg->aud_cfg.i2s_info.status = FM_I2S_STATE_ERR;
		cfg->aud_cfg.i2s_info.mode = FM_I2S_MODE_ERR;
		cfg->aud_cfg.i2s_info.rate = FM_I2S_SR_ERR;
		cfg->aud_cfg.i2s_pad = FM_I2S_PAD_ERR;
#else
		cfg->aud_cfg.aud_path = FM_AUD_ERR;
		cfg->aud_cfg.i2s_info.status = FM_I2S_STATE_ERR;
		cfg->aud_cfg.i2s_info.mode = FM_I2S_MODE_ERR;
		cfg->aud_cfg.i2s_info.rate = FM_I2S_SR_ERR;
		cfg->aud_cfg.i2s_pad = FM_I2S_PAD_ERR;
#endif
	} else if ((g_fm_chip_type == FM_AD_DIE_CHIP) || (g_fm_chip_type == FM_SOC_CHIP)) {
		/* MT6627 MT6580 MT6631 ?*/
		cfg->aud_cfg.aud_path = FM_AUD_I2S;
		cfg->aud_cfg.i2s_info.status = FM_I2S_OFF;
		cfg->aud_cfg.i2s_info.mode = FM_I2S_MASTER;
		cfg->aud_cfg.i2s_info.rate = FM_I2S_32K;
		cfg->aud_cfg.i2s_pad = FM_I2S_PAD_CONN;
	} else {
		WCN_DBG(FM_ALT | MAIN, "Invalid chip type %d\n", g_fm_chip_type);
		cfg->aud_cfg.aud_path = FM_AUD_ERR;
		cfg->aud_cfg.i2s_info.status = FM_I2S_STATE_ERR;
		cfg->aud_cfg.i2s_info.mode = FM_I2S_MODE_ERR;
		cfg->aud_cfg.i2s_info.rate = FM_I2S_SR_ERR;
		cfg->aud_cfg.i2s_pad = FM_I2S_PAD_ERR;
	}
	return 0;
}

static fm_s32 fm_cust_config_file(const fm_s8 *filename, fm_cust_cfg *cfg)
{
	fm_s32 ret = 0;
	fm_s8 *buf = NULL;
	fm_s32 file_len = 0;

	buf = fm_zalloc(4096);
	if (!buf) {
		WCN_DBG(FM_ALT | MAIN, "-ENOMEM\n");
		return -ENOMEM;
	}

	file_len = fm_file_read(filename, buf, 4096, 0);

	if (file_len <= 0) {
		WCN_DBG(FM_ALT | MAIN, "fail to read config file = %s\n", filename);
		ret = -1;
		goto out;
	}

	ret = cfg_parser(buf, cfg_item_handler, cfg);

out:
	if (buf)
		fm_free(buf);

	return ret;
}

static fm_s32 fm_cust_config_print(fm_cust_cfg *cfg)
{
	WCN_DBG(FM_NTC | MAIN, "0x%x configs:\n", g_fm_chipid);
	WCN_DBG(FM_NTC | MAIN, "RX->rssi_l:\t%d\n", cfg->rx_cfg.long_ana_rssi_th);
	WCN_DBG(FM_NTC | MAIN, "RX->rssi_s:\t%d\n", cfg->rx_cfg.short_ana_rssi_th);
	WCN_DBG(FM_NTC | MAIN, "RX->pamd_th:\t%d\n", cfg->rx_cfg.pamd_th);
	WCN_DBG(FM_NTC | MAIN, "RX->mr_th:\t%d\n", cfg->rx_cfg.mr_th);
	WCN_DBG(FM_NTC | MAIN, "RX->atdc_th:\t%d\n", cfg->rx_cfg.atdc_th);
	WCN_DBG(FM_NTC | MAIN, "RX->prx_th:\t%d\n", cfg->rx_cfg.prx_th);
	WCN_DBG(FM_NTC | MAIN, "RX->smg_th:\t%d\n", cfg->rx_cfg.smg_th);
	WCN_DBG(FM_NTC | MAIN, "RX->de_emphasis:\t%d\n", cfg->rx_cfg.deemphasis);
	WCN_DBG(FM_NTC | MAIN, "RX->osc_freq:\t%d\n", cfg->rx_cfg.osc_freq);
	WCN_DBG(FM_NTC | MAIN, "RX->desense_rssi_th:\t%d\n", cfg->rx_cfg.desene_rssi_th);

	WCN_DBG(FM_NTC | MAIN, "TX->scan_hole_low:\t%d\n", cfg->tx_cfg.scan_hole_low);
	WCN_DBG(FM_NTC | MAIN, "TX->scan_hole_high:\t%d\n", cfg->tx_cfg.scan_hole_high);
	WCN_DBG(FM_NTC | MAIN, "TX->power_level:\t%d\n", cfg->tx_cfg.power_level);

	WCN_DBG(FM_NTC | MAIN, "aud path[%d]I2S state[%d]mode[%d]rate[%d]pad[%d]\n",
		cfg->aud_cfg.aud_path,
		cfg->aud_cfg.i2s_info.status,
		cfg->aud_cfg.i2s_info.mode,
		cfg->aud_cfg.i2s_info.rate,
		cfg->aud_cfg.i2s_pad);
	return 0;
}

fm_s32 fm_cust_config_setup(const fm_s8 *filepath)
{
	fm_s32 ret = 0;
	fm_s8 *filep = NULL;
	fm_s8 file_path[51] = { 0 };

	fm_cust_config_default(&fm_config);
	WCN_DBG(FM_NTC | MAIN, "FM default config\n");
	fm_cust_config_print(&fm_config);

	if (!filepath) {
		filep = FM_CUST_CFG_PATH;
	} else {
		memcpy(file_path, filepath, (strlen(filepath) > 50) ? 50 : strlen(filepath));
		filep = file_path;
		trim_path(&filep);
	}

	ret = fm_cust_config_file(filep, &fm_config);
	WCN_DBG(FM_NTC | MAIN, "FM cust config\n");
	fm_cust_config_print(&fm_config);
	return ret;
}

fm_u16 fm_cust_config_fetch(enum fm_cust_cfg_op op_code)
{
	fm_u16 tmp = 0;

	switch (op_code) {
	/* For FM RX */
	case FM_CFG_RX_RSSI_TH_LONG:
		tmp = fm_config.rx_cfg.long_ana_rssi_th;
		break;
	case FM_CFG_RX_RSSI_TH_SHORT:
		tmp = fm_config.rx_cfg.short_ana_rssi_th;
		break;
	case FM_CFG_RX_DESENSE_RSSI_TH:
		tmp = fm_config.rx_cfg.desene_rssi_th;
		break;
	case FM_CFG_RX_PAMD_TH:
		tmp = fm_config.rx_cfg.pamd_th;
		break;
	case FM_CFG_RX_MR_TH:
		tmp = fm_config.rx_cfg.mr_th;
		break;
	case FM_CFG_RX_ATDC_TH:
		tmp = fm_config.rx_cfg.atdc_th;
		break;
	case FM_CFG_RX_PRX_TH:
		tmp = fm_config.rx_cfg.prx_th;
		break;
	case FM_CFG_RX_ATDEV_TH:
		tmp = fm_config.rx_cfg.atdev_th;
		break;
	case FM_CFG_RX_CQI_TH:
		tmp = fm_config.rx_cfg.cqi_th;
		break;
	case FM_CFG_RX_SMG_TH:
		tmp = fm_config.rx_cfg.smg_th;
		break;
	case FM_CFG_RX_DEEMPHASIS:
		tmp = fm_config.rx_cfg.deemphasis;
		break;
	case FM_CFG_RX_OSC_FREQ:
		tmp = fm_config.rx_cfg.osc_freq;
		break;

	case FM_CFG_TX_SCAN_HOLE_LOW:
		tmp = fm_config.tx_cfg.scan_hole_low;
		break;
	case FM_CFG_TX_SCAN_HOLE_HIGH:
		tmp = fm_config.tx_cfg.scan_hole_high;
		break;
	case FM_CFG_TX_PWR_LEVEL:
		tmp = fm_config.tx_cfg.power_level;
		break;
	case FM_CFG_TX_PAMD_TH:
		tmp = fm_config.tx_cfg.pamd_th;
		break;
	case FM_CFG_TX_DEEMPHASIS:
		tmp = fm_config.tx_cfg.mr_th;
		break;
	case FM_CFG_TX_SMG_TH:
		tmp = fm_config.tx_cfg.smg_th;
		break;
	default:
		break;
	}

	WCN_DBG(FM_DBG | MAIN, "cust cfg %d: 0x%04x\n", op_code, tmp);
	return tmp;
}
fm_u16 fm_cust_config_chip(fm_u16 chipid, enum fm_cfg_chip_type type)
{
	g_fm_chipid = chipid;
	g_fm_chip_type = type;

	return 0;
}
