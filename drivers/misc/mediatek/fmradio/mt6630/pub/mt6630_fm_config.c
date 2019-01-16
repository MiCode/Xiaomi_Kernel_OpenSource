/* mt6630_fm_config.c
 *
 * (C) Copyright 2011
 * MediaTek <www.MediaTek.com>
 * hongcheng <hongcheng.xia@MediaTek.com>
 *
 * FM Radio Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
/* #include "fm_cust_cfg.h" */
#include "mt6630_fm_cust_cfg.h"
fm_cust_cfg mt6630_fm_config;
/* static fm_s32 fm_index = 0; */

static fm_s32 MT6630fm_cust_config_print(fm_cust_cfg *cfg)
{
	WCN_DBG(FM_NTC | MAIN, "MT6630 rssi_l:\t%d\n", cfg->rx_cfg.long_ana_rssi_th);
	WCN_DBG(FM_NTC | MAIN, "MT6630 rssi_s:\t%d\n", cfg->rx_cfg.short_ana_rssi_th);
	WCN_DBG(FM_NTC | MAIN, "MT6630 pamd_th:\t%d\n", cfg->rx_cfg.pamd_th);
	WCN_DBG(FM_NTC | MAIN, "MT6630 mr_th:\t%d\n", cfg->rx_cfg.mr_th);
	WCN_DBG(FM_NTC | MAIN, "MT6630 atdc_th:\t%d\n", cfg->rx_cfg.atdc_th);
	WCN_DBG(FM_NTC | MAIN, "MT6630 prx_th:\t%d\n", cfg->rx_cfg.prx_th);
	WCN_DBG(FM_NTC | MAIN, "MT6630 atdev_th:\t%d\n", cfg->rx_cfg.atdev_th);
	WCN_DBG(FM_NTC | MAIN, "MT6630 smg_th:\t%d\n", cfg->rx_cfg.smg_th);
	WCN_DBG(FM_NTC | MAIN, "de_emphasis:\t%d\n", cfg->rx_cfg.deemphasis);
	WCN_DBG(FM_NTC | MAIN, "osc_freq:\t%d\n", cfg->rx_cfg.osc_freq);

	WCN_DBG(FM_NTC | MAIN, "aud path[%d]I2S state[%d]mode[%d]rate[%d]\n", cfg->aud_cfg.aud_path,
		cfg->aud_cfg.i2s_info.status, cfg->aud_cfg.i2s_info.mode,
		cfg->aud_cfg.i2s_info.rate);
	return 0;
}

static fm_s32 MT6630cfg_item_handler(fm_s8 *grp, fm_s8 *key, fm_s8 *val, fm_cust_cfg *cfg)
{
	fm_s32 ret = 0;
	struct fm_rx_cust_cfg *rx_cfg = &cfg->rx_cfg;

	if (0 <= (ret = cfg_item_match(key, val, "FM_RX_RSSI_TH_LONG_MT6630", &rx_cfg->long_ana_rssi_th))) {	/* FMR_RSSI_TH_L = 0x0301 */
		return ret;
	} else if (0 <=
		   (ret =
		    cfg_item_match(key, val, "FM_RX_RSSI_TH_SHORT_MT6630",
				   &rx_cfg->short_ana_rssi_th))) {
		return ret;
	} else if (0 <=
		   (ret =
		    cfg_item_match(key, val, "FM_RX_DESENSE_RSSI_MT6630",
				   &rx_cfg->desene_rssi_th))) {
		return ret;
	} else if (0 <= (ret = cfg_item_match(key, val, "FM_RX_PAMD_TH_MT6630", &rx_cfg->pamd_th))) {
		return ret;
	} else if (0 <= (ret = cfg_item_match(key, val, "FM_RX_MR_TH_MT6630", &rx_cfg->mr_th))) {
		return ret;
	} else if (0 <= (ret = cfg_item_match(key, val, "FM_RX_ATDC_TH_MT6630", &rx_cfg->atdc_th))) {
		return ret;
	} else if (0 <= (ret = cfg_item_match(key, val, "FM_RX_PRX_TH_MT6630", &rx_cfg->prx_th))) {
		return ret;
	}
	/*else if (0 <= (ret = cfg_item_match(key, val, "FM_RX_ATDEV_TH_MT6630", &rx_cfg->atdev_th)))
	   {
	   return ret;
	   } */
	else if (0 <= (ret = cfg_item_match(key, val, "FM_RX_SMG_TH_MT6630", &rx_cfg->smg_th))) {
		return ret;
	} else if (0 <=
		   (ret =
		    cfg_item_match(key, val, "FM_RX_DEEMPHASIS_MT6630", &rx_cfg->deemphasis))) {
		return ret;
	} else if (0 <=
		   (ret = cfg_item_match(key, val, "FM_RX_OSC_FREQ_MT6630", &rx_cfg->osc_freq))) {
		return ret;
	} else {
		WCN_DBG(FM_WAR | MAIN, "MT6630 invalid key\n");
		return -1;
	}
}

static fm_s32 MT6630fm_cust_config_default(fm_cust_cfg *cfg)
{
	FMR_ASSERT(cfg);

	cfg->rx_cfg.long_ana_rssi_th = FM_RX_RSSI_TH_LONG_MT6630;
	cfg->rx_cfg.short_ana_rssi_th = FM_RX_RSSI_TH_SHORT_MT6630;
	cfg->rx_cfg.desene_rssi_th = FM_RX_DESENSE_RSSI_MT6630;
	cfg->rx_cfg.pamd_th = FM_RX_PAMD_TH_MT6630;
	cfg->rx_cfg.mr_th = FM_RX_MR_TH_MT6630;
	cfg->rx_cfg.atdc_th = FM_RX_ATDC_TH_MT6630;
	cfg->rx_cfg.prx_th = FM_RX_PRX_TH_MT6630;
	cfg->rx_cfg.smg_th = FM_RX_SMG_TH_MT6630;
	cfg->rx_cfg.deemphasis = FM_RX_DEEMPHASIS_MT6630;
	cfg->rx_cfg.osc_freq = FM_RX_OSC_FREQ_MT6630;

	cfg->tx_cfg.pamd_th = FM_TX_PAMD_TH_MT6630;
	cfg->tx_cfg.mr_th = FM_TX_MR_TH_MT6630;
	cfg->tx_cfg.smg_th = FM_TX_SMG_TH_MT6630;

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

	return 0;
}

static fm_s32 MT6630fm_cust_config_file(const fm_s8 *filename, fm_cust_cfg *cfg)
{
	fm_s32 ret = 0;
	fm_s8 *buf = NULL;
	fm_s32 file_len = 0;

	if (!(buf = fm_zalloc(4096))) {
		WCN_DBG(FM_ALT | MAIN, "-ENOMEM\n");
		return -ENOMEM;
	}
/* fm_index = 0; */

	file_len = fm_file_read(filename, buf, 4096, 0);

	if (file_len <= 0) {
		ret = -1;
		goto out;
	}

	ret = cfg_parser(buf, MT6630cfg_item_handler, cfg);

 out:

	if (buf) {
		fm_free(buf);
	}

	return ret;
}

#define MT6630_FM_CUST_CFG_PATH "etc/fmr/mt6630_fm_cust.cfg"
fm_s32 MT6630fm_cust_config_setup(const fm_s8 *filepath)
{
	fm_s32 ret = 0;
	fm_s8 *filep = NULL;
	fm_s8 file_path[51] = { 0 };

	MT6630fm_cust_config_default(&mt6630_fm_config);
	WCN_DBG(FM_NTC | MAIN, "MT6630 FM default config\n");
	MT6630fm_cust_config_print(&mt6630_fm_config);

	if (!filepath) {
		filep = MT6630_FM_CUST_CFG_PATH;
	} else {
		memcpy(file_path, filepath, (strlen(filepath) > 50) ? 50 : strlen(filepath));
		filep = file_path;
		trim_path(&filep);
	}

	ret = MT6630fm_cust_config_file(filep, &mt6630_fm_config);
	WCN_DBG(FM_NTC | MAIN, "MT6630 FM cust config\n");
	MT6630fm_cust_config_print(&mt6630_fm_config);

	return ret;
}
