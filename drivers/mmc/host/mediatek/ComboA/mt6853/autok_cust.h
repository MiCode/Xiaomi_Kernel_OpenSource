/* Copyright (C) 2018 MediaTek Inc.
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

#ifndef _AUTOK_CUST_H_
#define _AUTOK_CUST_H_


#define AUTOK_VERSION                   (0x18110817)

struct AUTOK_PLAT_PARA_TX {
	unsigned int chip_hw_ver;

	u8 msdc0_hs400_clktx;
	u8 msdc0_hs400_cmdtx;
	u8 msdc0_hs400_dat0tx;
	u8 msdc0_hs400_dat1tx;
	u8 msdc0_hs400_dat2tx;
	u8 msdc0_hs400_dat3tx;
	u8 msdc0_hs400_dat4tx;
	u8 msdc0_hs400_dat5tx;
	u8 msdc0_hs400_dat6tx;
	u8 msdc0_hs400_dat7tx;
	u8 msdc0_hs400_txskew;
	u8 msdc0_ddr_ckd;
	u8 msdc1_ddr_ckd;
	u8 msdc2_ddr_ckd;

	u8 msdc0_clktx;
	u8 msdc0_cmdtx;
	u8 msdc0_dat0tx;
	u8 msdc0_dat1tx;
	u8 msdc0_dat2tx;
	u8 msdc0_dat3tx;
	u8 msdc0_dat4tx;
	u8 msdc0_dat5tx;
	u8 msdc0_dat6tx;
	u8 msdc0_dat7tx;
	u8 msdc0_txskew;

	u8 msdc1_clktx;
	u8 msdc1_sdr104_clktx;

	u8 msdc2_clktx;

	u8 sdio30_plus_clktx;
	u8 sdio30_plus_cmdtx;
	u8 sdio30_plus_dat0tx;
	u8 sdio30_plus_dat1tx;
	u8 sdio30_plus_dat2tx;
	u8 sdio30_plus_dat3tx;

	u8 msdc0_duty_bypass;
	u8 msdc0_hl_duty_sel;
	u8 msdc1_duty_bypass;
	u8 msdc1_hl_duty_sel;
	u8 msdc2_duty_bypass;
	u8 msdc2_hl_duty_sel;
};

struct AUTOK_PLAT_PARA_RX {
	unsigned int chip_hw_ver;

	u8 ckgen_val;
	u8 latch_en_cmd_hs400;
	u8 latch_en_crc_hs400;
	u8 latch_en_cmd_hs200;
	u8 latch_en_crc_hs200;
	u8 latch_en_cmd_ddr208;
	u8 latch_en_crc_ddr208;
	u8 latch_en_cmd_sd_sdr104;
	u8 latch_en_crc_sd_sdr104;
	u8 latch_en_cmd_sdio_sdr104;
	u8 latch_en_crc_sdio_sdr104;
	u8 latch_en_cmd_hs;
	u8 latch_en_crc_hs;
	u8 cmd_ta_val;
	u8 crc_ta_val;
	u8 busy_ma_val;

	u8 new_water_hs400;
	u8 new_water_hs200;
	u8 new_water_ddr208;
	u8 new_water_sdr104;
	u8 new_water_hs;

	u8 new_stop_hs400;
	u8 new_stop_hs200;
	u8 new_stop_ddr208;
	u8 new_stop_sdr104;
	u8 new_stop_hs;

	u8 old_water_hs400;
	u8 old_water_hs200;
	u8 old_water_ddr208;
	u8 old_water_sdr104;
	u8 old_water_hs;

	u8 old_stop_hs400;
	u8 old_stop_hs200;
	u8 old_stop_ddr208;
	u8 old_stop_sdr104;
	u8 old_stop_hs;

	u8 read_dat_cnt_hs400;
	u8 read_dat_cnt_ddr208;

	u8 end_bit_chk_cnt_hs400;
	u8 end_bit_chk_cnt_ddr208;

	u8 latchck_switch_cnt_hs400;
	u8 latchck_switch_cnt_ddr208;

	u8 ds_dly3_hs400;
	u8 ds_dly3_ddr208;
};

struct AUTOK_PLAT_PARA_MISC {
	unsigned int chip_hw_ver;

	u8 latch_ck_emmc_times;
	u8 latch_ck_sdio_times;
	u8 latch_ck_sd_times;
	u8 emmc_data_tx_tune;
	u8 data_tx_separate_tune;
};

struct AUTOK_PLAT_TOP_CTRL {
	u8 msdc0_rx_enhance_top;
	u8 msdc1_rx_enhance_top;
	u8 msdc2_rx_enhance_top;
};

struct AUTOK_PLAT_FUNC {
	unsigned int chip_hw_ver;

	u8 new_path_hs400;
	u8 new_path_hs200;
	u8 new_path_ddr208;
	u8 new_path_sdr104;
	u8 new_path_hs;
	u8 multi_sync;
	u8 rx_enhance;
	u8 r1b_check;
	u8 ddr50_fix;
	u8 fifo_1k;
	u8 latch_enhance;
	u8 msdc0_bypass_duty_modify;
	u8 msdc1_bypass_duty_modify;
	u8 msdc2_bypass_duty_modify;
};

#define get_platform_para_tx(autok_para_tx) \
	do { \
		autok_para_tx.msdc0_hs400_clktx = 0; \
		autok_para_tx.msdc0_hs400_cmdtx = 0; \
		autok_para_tx.msdc0_hs400_dat0tx = 0; \
		autok_para_tx.msdc0_hs400_dat1tx = 0; \
		autok_para_tx.msdc0_hs400_dat2tx = 0; \
		autok_para_tx.msdc0_hs400_dat3tx = 0; \
		autok_para_tx.msdc0_hs400_dat4tx = 0; \
		autok_para_tx.msdc0_hs400_dat5tx = 0; \
		autok_para_tx.msdc0_hs400_dat6tx = 0; \
		autok_para_tx.msdc0_hs400_dat7tx = 0; \
		autok_para_tx.msdc0_hs400_txskew = 0; \
		autok_para_tx.msdc0_ddr_ckd = 1; \
		autok_para_tx.msdc1_ddr_ckd = 0; \
		autok_para_tx.msdc2_ddr_ckd = 1; \
		autok_para_tx.msdc0_clktx = 0; \
		autok_para_tx.msdc0_cmdtx = 0; \
		autok_para_tx.msdc0_dat0tx = 0; \
		autok_para_tx.msdc0_dat1tx = 0; \
		autok_para_tx.msdc0_dat2tx = 0; \
		autok_para_tx.msdc0_dat3tx = 0; \
		autok_para_tx.msdc0_dat4tx = 0; \
		autok_para_tx.msdc0_dat5tx = 0; \
		autok_para_tx.msdc0_dat6tx = 0; \
		autok_para_tx.msdc0_dat7tx = 0; \
		autok_para_tx.msdc0_txskew = 0; \
		autok_para_tx.msdc1_clktx = 0; \
		autok_para_tx.msdc1_sdr104_clktx = 0; \
		autok_para_tx.msdc2_clktx = 0; \
		autok_para_tx.sdio30_plus_clktx = 0; \
		autok_para_tx.sdio30_plus_cmdtx = 0; \
		autok_para_tx.sdio30_plus_dat0tx = 0; \
		autok_para_tx.sdio30_plus_dat1tx = 0; \
		autok_para_tx.sdio30_plus_dat2tx = 0; \
		autok_para_tx.sdio30_plus_dat3tx = 0; \
		autok_para_tx.msdc0_duty_bypass = 0; \
		autok_para_tx.msdc0_hl_duty_sel = 0; \
		autok_para_tx.msdc1_duty_bypass = 0; \
		autok_para_tx.msdc1_hl_duty_sel = 0; \
		autok_para_tx.msdc2_duty_bypass = 0; \
		autok_para_tx.msdc2_hl_duty_sel = 0; \
	} while (0)

#define get_platform_para_rx(autok_para_rx) \
	do { \
		autok_para_rx.ckgen_val = 0; \
		autok_para_rx.latch_en_cmd_hs400 = 3; \
		autok_para_rx.latch_en_crc_hs400 = 3; \
		autok_para_rx.latch_en_cmd_hs200 = 2; \
		autok_para_rx.latch_en_crc_hs200 = 2; \
		autok_para_rx.latch_en_cmd_ddr208 = 4; \
		autok_para_rx.latch_en_crc_ddr208 = 4; \
		autok_para_rx.latch_en_cmd_sd_sdr104 = 1; \
		autok_para_rx.latch_en_crc_sd_sdr104 = 1; \
		autok_para_rx.latch_en_cmd_sdio_sdr104 = 2; \
		autok_para_rx.latch_en_crc_sdio_sdr104 = 2; \
		autok_para_rx.latch_en_cmd_hs = 1; \
		autok_para_rx.latch_en_crc_hs = 1; \
		autok_para_rx.cmd_ta_val = 0; \
		autok_para_rx.crc_ta_val = 0; \
		autok_para_rx.busy_ma_val = 1; \
		autok_para_rx.new_water_hs400 = 8; \
		autok_para_rx.new_stop_hs400 = 3; \
		autok_para_rx.new_water_hs200 = 0; \
		autok_para_rx.new_stop_hs200 = 6; \
		autok_para_rx.new_water_ddr208 = 8; \
		autok_para_rx.new_stop_ddr208 = 3; \
		autok_para_rx.new_water_sdr104 = 0; \
		autok_para_rx.new_stop_sdr104 = 6; \
		autok_para_rx.new_water_hs = 8; \
		autok_para_rx.new_stop_hs = 3; \
		autok_para_rx.old_water_hs400 = 8; \
		autok_para_rx.old_stop_hs400 = 3; \
		autok_para_rx.old_water_hs200 = 0; \
		autok_para_rx.old_stop_hs200 = 6; \
		autok_para_rx.old_water_ddr208 = 8; \
		autok_para_rx.old_stop_ddr208 = 3; \
		autok_para_rx.old_water_sdr104 = 0; \
		autok_para_rx.old_stop_sdr104 = 6; \
		autok_para_rx.old_water_hs = 8; \
		autok_para_rx.old_stop_hs = 3; \
		autok_para_rx.read_dat_cnt_hs400 = 7; \
		autok_para_rx.read_dat_cnt_ddr208 = 0; \
		autok_para_rx.end_bit_chk_cnt_hs400 = 14; \
		autok_para_rx.end_bit_chk_cnt_ddr208 = 0; \
		autok_para_rx.latchck_switch_cnt_hs400 = 6; \
		autok_para_rx.latchck_switch_cnt_ddr208 = 0; \
		autok_para_rx.ds_dly3_hs400 = 0; \
		autok_para_rx.ds_dly3_ddr208 = 0; \
	} while (0)

#define get_platform_para_misc(autok_para_misc) \
	do { \
		autok_para_misc.latch_ck_emmc_times = 10; \
		autok_para_misc.latch_ck_sdio_times = 20; \
		autok_para_misc.latch_ck_sd_times = 20; \
		autok_para_misc.emmc_data_tx_tune = 1; \
		autok_para_misc.data_tx_separate_tune = 0; \
	} while (0)

#define get_platform_top_ctrl(autok_top_ctrl) \
	do { \
		autok_top_ctrl.msdc0_rx_enhance_top = 1; \
		autok_top_ctrl.msdc1_rx_enhance_top = 1; \
		autok_top_ctrl.msdc2_rx_enhance_top = 0; \
	} while (0)
/*
 * emmc_data_tx_tune:0 use cmd24;1 use cmd23+cmd25;2:use cmdq cmd
 */
#define get_platform_func(autok_para_func) \
	do { \
		autok_para_func.new_path_hs400 = 1; \
		autok_para_func.new_path_hs200 = 1; \
		autok_para_func.new_path_ddr208 = 1; \
		autok_para_func.new_path_sdr104 = 1; \
		autok_para_func.new_path_hs = 1; \
		autok_para_func.multi_sync = 1; \
		autok_para_func.rx_enhance = 1; \
		autok_para_func.r1b_check = 1; \
		autok_para_func.ddr50_fix = 1; \
		autok_para_func.fifo_1k = 1; \
		autok_para_func.latch_enhance = 1; \
		autok_para_func.msdc0_bypass_duty_modify = 1; \
		autok_para_func.msdc1_bypass_duty_modify = 1; \
		autok_para_func.msdc2_bypass_duty_modify = 0; \
	} while (0)

#define PORT0_PB0_RD_DAT_SEL_VALID
#define PORT1_PB0_RD_DAT_SEL_VALID
#define PORT3_PB0_RD_DAT_SEL_VALID
#define MMC_QUE_TASK_PARAMS_RD 441
#define MMC_QUE_TASK_PARAMS_WR 440
#define MMC_SWITCH_CQ_EN 601
#define MMC_SWITCH_CQ_DIS 600

/*
 * reg define
 */
#define AUTOK_SDC_RX_ENH_EN	(0x1  << 20) /* RW */
#define AUTOK_TOP_SDC_RX_ENHANCE_EN (0x1 << 15) /* RW */

/**********************************************************
 * Feature  Control Defination                            *
 **********************************************************/
#define AUTOK_EMMC_OFFLINE_TUNE_TX_ENABLE       0
#define AUTOK_SD_CARD_OFFLINE_TUNE_TX_ENABLE    0
#define AUTOK_SDIO_OFFLINE_TUNE_TX_ENABLE       1
#define AUTOK_OFFLINE_CMD_H_TX_ENABLE           0
#define AUTOK_OFFLINE_DAT_H_TX_ENABLE           1
#define AUTOK_OFFLINE_CMD_D_RX_ENABLE           0
#define AUTOK_OFFLINE_DAT_D_RX_ENABLE           1
#define AUTOK_OFFLINE_TUNE_DEVICE_RX_ENABLE     1
#define AUTOK_PARAM_DUMP_ENABLE                 0
#define SINGLE_EDGE_ONLINE_TUNE                 0
#define SDIO_PLUS_CMD_TUNE                      1
#define STOP_CLK_NEW_PATH                       0
#define DS_DLY3_SCAN                            0
#define CHIP_DENALI_3_DAT_TUNE                  0

#endif /* _AUTOK_CUST_H_ */

