/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_AUTOK_H_
#define _MTK_AUTOK_H_

#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/time.h>

#define AUTOK_VERSION    (0x18110817)
#define MSDC_FIFO_SZ     (128)

#define TUNE_NONE          (0)        /* No need tune */
#define TUNE_CMD_CRC       (0x1 << 0) /* transfer cmd crc */
#define TUNE_DATA_WRITE    (0x1 << 1) /* transfer data crc */
#define TUNE_DATA_READ     (0x1 << 2) /* transfer data crc */
#define TUNE_CMD_TMO       (0x1 << 3) /* transfer cmd tmo */
#define TUNE_AUTOK_PASS    (0x1 << 4) /* autok pass flag */

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
		autok_para_rx.ds_dly3_hs400 = 20; \
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
#define AUTOK_SDC_RX_ENH_EN	(0x1 << 20) /* RW */
#define AUTOK_TOP_SDC_RX_ENHANCE_EN (0x1 << 15) /* RW */

struct msdc_host;
struct mmc_host;

#define E_RES_PASS     (0)
#define E_RES_CMD_TMO  (1<<0)
#define E_RES_RSP_CRC  (1<<1)
#define E_RES_DAT_CRC  (1<<2)
#define E_RES_DAT_TMO  (1<<3)
#define E_RES_W_CRC    (1<<4)
#define E_RES_ERR      (1<<5)
#define E_RES_START    (1<<6)
#define E_RES_PW_SMALL (1<<7)
#define E_RES_KEEP_OLD (1<<8)
#define E_RES_CMP_ERR  (1<<9)
#define E_RES_FATAL_ERR  (1<<10)

#define E_RESULT_MAX

#define MERGE_CMD           (1<<0)
#define MERGE_DAT           (1<<1)
#define MERGE_DS_DAT        (1<<2)
#define MERGE_DS_CMD        (1<<3)
#define MERGE_DEVICE_D_RX   (1<<4)
#define MERGE_DEVICE_C_RX   (1<<5)
#define MERGE_HOST_D_TX     (1<<6)
#define MERGE_HOST_C_TX     (1<<7)
#define MERGE_HOST_CLK_TX   (1<<8)
#define MERGE_HS200_SDR104  (MERGE_CMD | MERGE_DAT)
#define MERGE_HS400         (MERGE_CMD | MERGE_DS_DAT)
#define MERGE_DDR208        (MERGE_CMD \
							| MERGE_DS_DAT \
							| MERGE_DEVICE_D_RX \
							| MERGE_HOST_D_TX)

#ifndef NULL
#define NULL                0
#endif
#ifndef TRUE
#define TRUE                (0 == 0)
#endif
#ifndef FALSE
#define FALSE               (0 != 0)
#endif

#define AUTOK_DBG_OFF                             0
#define AUTOK_DBG_ERROR                           1
#define AUTOK_DBG_RES                             2
#define AUTOK_DBG_WARN                            3
#define AUTOK_DBG_TRACE                           4
#define AUTOK_DBG_LOUD                            5

extern unsigned int autok_debug_level;

#define AUTOK_DBGPRINT(_level, _fmt ...)   \
({                                         \
	if (autok_debug_level >= _level) { \
		pr_info(_fmt);             \
	}                                  \
})

#define AUTOK_RAWPRINT(_fmt ...)           \
({                                         \
	pr_info(_fmt);                     \
})

enum ERROR_TYPE {
	CMD_ERROR = 0,
	DATA_ERROR,
	CRC_STATUS_ERROR,
};

enum TUNE_TX_TYPE {
	TX_CMD = 0,
	TX_DATA,
};

enum AUTOK_PARAM {
	/* command response sample selection
	 * (MSDC_SMPL_RISING, MSDC_SMPL_FALLING)
	 */
	CMD_EDGE,

	/* cmd response async fifo out edge select */
	CMD_FIFO_EDGE,

	/* read data sample selection
	 * (MSDC_SMPL_RISING, MSDC_SMPL_FALLING)
	 */
	RDATA_EDGE,

	/* read data async fifo out edge select */
	RD_FIFO_EDGE,

	/* write data crc status async fifo out edge select */
	WD_FIFO_EDGE,

	/* [Data Tune]CMD Pad RX Delay Line1 Control.
	 * This register is used to
	 * fine-tune CMD pad macro respose latch timing.
	 * Total 32 stages[Data Tune]
	 */
	CMD_RD_D_DLY1,

	/* [Data Tune]CMD Pad RX Delay Line1 Sel-> delay cell1 enable */
	CMD_RD_D_DLY1_SEL,

	/* [Data Tune]CMD Pad RX Delay Line2 Control. This register is used to
	 * fine-tune CMD pad macro respose latch timing.
	 * Total 32 stages[Data Tune]
	 */
	CMD_RD_D_DLY2,

	/* [Data Tune]CMD Pad RX Delay Line1 Sel-> delay cell2 enable */
	CMD_RD_D_DLY2_SEL,

	/* [Data Tune]DAT Pad RX Delay Line1 Control (for MSDC RD),
	 * Total 32 stages [Data Tune]
	 */
	DAT_RD_D_DLY1,

	/* [Data Tune]DAT Pad RX Delay Line1 Sel-> delay cell1 enable */
	DAT_RD_D_DLY1_SEL,

	/* [Data Tune]DAT Pad RX Delay Line2 Control (for MSDC RD),
	 * Total 32 stages [Data Tune]
	 */
	DAT_RD_D_DLY2,

	/* [Data Tune]DAT Pad RX Delay Line2 Sel-> delay cell2 enable */
	DAT_RD_D_DLY2_SEL,

	/* Internal MSDC clock phase selection. Total 8 stages,
	 * each stage can delay 1 clock period of msdc_src_ck
	 */
	INT_DAT_LATCH_CK,

	/* DS Pad Z clk delay count, range: 0~63, Z dly1(0~31)+Z dly2(0~31) */
	EMMC50_DS_Z_DLY1,

	/* DS Pad Z clk del sel: [dly2_sel:dly1_sel] -> [0,1]:
	 * dly1 enable [1,2]:dl2 & dly1 enable ,else :no dly enable
	 */
	EMMC50_DS_Z_DLY1_SEL,

	/* DS Pad Z clk delay count, range: 0~63, Z dly1(0~31)+Z dly2(0~31) */
	EMMC50_DS_Z_DLY2,

	/* DS Pad Z clk del sel: [dly2_sel:dly1_sel] -> [0,1]:
	 * dly1 enable [1,2]:dl2 & dly1 enable ,else :no dly enable,
	 */
	EMMC50_DS_Z_DLY2_SEL,

	/* DS Pad Z_DLY clk delay count, range: 0~31 */
	EMMC50_DS_ZDLY_DLY,

	/* eMMC50 CMD TX dly */
	EMMC50_CMD_TX_DLY,

	/* eMMC50 DATA TX dly */
	EMMC50_DATA0_TX_DLY,
	EMMC50_DATA1_TX_DLY,
	EMMC50_DATA2_TX_DLY,
	EMMC50_DATA3_TX_DLY,
	EMMC50_DATA4_TX_DLY,
	EMMC50_DATA5_TX_DLY,
	EMMC50_DATA6_TX_DLY,
	EMMC50_DATA7_TX_DLY,

	/* CLK Pad TX Delay Control. This register is used to
	 * add delay to CLK phase. Total 32 stages
	 */
	PAD_CLK_TXDLY_AUTOK,
	TUNING_PARAM_COUNT,

	/* CMD scan result */
	CMD_SCAN_R0,
	CMD_SCAN_R1,
	CMD_SCAN_R2,
	CMD_SCAN_R3,
	CMD_SCAN_R4,
	CMD_SCAN_R5,
	CMD_SCAN_R6,
	CMD_SCAN_R7,

	CMD_SCAN_F0,
	CMD_SCAN_F1,
	CMD_SCAN_F2,
	CMD_SCAN_F3,
	CMD_SCAN_F4,
	CMD_SCAN_F5,
	CMD_SCAN_F6,
	CMD_SCAN_F7,

	/* DATA scan result */
	DAT_SCAN_R0,
	DAT_SCAN_R1,
	DAT_SCAN_R2,
	DAT_SCAN_R3,
	DAT_SCAN_R4,
	DAT_SCAN_R5,
	DAT_SCAN_R6,
	DAT_SCAN_R7,

	DAT_SCAN_F0,
	DAT_SCAN_F1,
	DAT_SCAN_F2,
	DAT_SCAN_F3,
	DAT_SCAN_F4,
	DAT_SCAN_F5,
	DAT_SCAN_F6,
	DAT_SCAN_F7,

	/* DS CMD scan result */
	DS_CMD_SCAN_0,
	DS_CMD_SCAN_1,
	DS_CMD_SCAN_2,
	DS_CMD_SCAN_3,
	DS_CMD_SCAN_4,
	DS_CMD_SCAN_5,
	DS_CMD_SCAN_6,
	DS_CMD_SCAN_7,

	/* DS DAT scan result */
	DS_DAT_SCAN_0,
	DS_DAT_SCAN_1,
	DS_DAT_SCAN_2,
	DS_DAT_SCAN_3,
	DS_DAT_SCAN_4,
	DS_DAT_SCAN_5,
	DS_DAT_SCAN_6,
	DS_DAT_SCAN_7,

	/* Device CMD RX result */
	D_CMD_SCAN_0,
	D_CMD_SCAN_1,
	D_CMD_SCAN_2,
	D_CMD_SCAN_3,

	/* Device DATA RX result */
	D_DATA_SCAN_0,
	D_DATA_SCAN_1,
	D_DATA_SCAN_2,
	D_DATA_SCAN_3,

	/* Host CMD TX result */
	H_CMD_SCAN_0,
	H_CMD_SCAN_1,
	H_CMD_SCAN_2,
	H_CMD_SCAN_3,

	/* Host DATA TX result */
	H_DATA_SCAN_0,
	H_DATA_SCAN_1,
	H_DATA_SCAN_2,
	H_DATA_SCAN_3,

	/* AUTOK version */
	AUTOK_VER0,
	AUTOK_VER1,
	AUTOK_VER2,
	AUTOK_VER3,

	CMD_MAX_WIN,
	DAT_MAX_WIN,
	DS_MAX_WIN,
	DEV_D_RX_MAX_WIN,
	DEV_C_RX_MAX_WIN,
	H_D_TX_MAX_WIN,
	H_C_TX_MAX_WIN,
	H_CLK_TX_MAX_WIN,

	TUNING_PARA_SCAN_COUNT,

	/* Data line rising/falling latch
	 * fine tune selection in read transaction.
	 * 1'b0: All data line share one value indicated
	 * by MSDC_IOCON.R_D_SMPL.
	 * 1'b1: Each data line has its own  selection value indicated by
	 * Data line (x): MSDC_IOCON.R_D(x)_SMPL
	 */
	READ_DATA_SMPL_SEL,

	/* Data line rising/falling latch
	 * fine tune selection in write transaction.
	 * 1'b0: All data line share one value indicated by
	 * MSDC_IOCON.W_D_SMPL.
	 * 1'b1: Each data line has its own selection value
	 * indicated by Data line (x): MSDC_IOCON.W_D(x)_SMPL
	 */
	WRITE_DATA_SMPL_SEL,

	/* Data line delay line fine tune selection.
	 *1'b0: All data line share one delay
	 * selection value indicated by PAD_TUNE.PAD_DAT_RD_RXDLY.
	 * 1'b1: Each data line has its
	 * own delay selection value indicated by
	 * Data line (x): DAT_RD_DLY(x).DAT0_RD_DLY
	 */
	DATA_DLYLINE_SEL,

	/* [Data Tune]CMD & DATA Pin tune Data Selection[Data Tune Sel] */
	MSDC_DAT_TUNE_SEL,

	/* [Async_FIFO Mode Sel For Write Path] */
	MSDC_WCRC_ASYNC_FIFO_SEL,

	/* [Async_FIFO Mode Sel For CMD Path] */
	MSDC_RESP_ASYNC_FIFO_SEL,

	/* Write Path Mux for emmc50 function & emmc45 function ,
	 * Only emmc50 design valid,[1-eMMC50, 0-eMMC45]
	 */
	EMMC50_WDATA_MUX_EN,

	/* CMD Path Mux for emmc50 function & emmc45 function ,
	 * Only emmc50 design valid,[1-eMMC50, 0-eMMC45]
	 */
	EMMC50_CMD_MUX_EN,

	/* CMD response DS latch or internal clk latch */
	EMMC50_CMD_RESP_LATCH,

	/* write data crc status async fifo output edge select */
	EMMC50_WDATA_EDGE,

	/* CKBUF in CKGEN Delay Selection. Total 32 stages */
	CKGEN_MSDC_DLY_SEL,

	/* CMD response turn around period.
	 *The turn around cycle = CMD_RSP_TA_CNTR + 2,
	 * Only for USH104 mode, this register should be
	 * set to 0 in non-UHS104 mode
	 */
	CMD_RSP_TA_CNTR,

	/* Write data and CRC status turn around period.
	 * The turn around cycle = WRDAT_CRCS_TA_CNTR + 2,
	 * Only for USH104 mode,  this register should be set to 0
	 * in non-UHS104 mode
	 */
	WRDAT_CRCS_TA_CNTR,

	SDC_RX_ENHANCE,

	TOTAL_PARAM_COUNT
};

enum AUTOK_VCORE {
	AUTOK_VCORE_LEVEL0 = 0,
	AUTOK_VCORE_LEVEL1,
	AUTOK_VCORE_LEVEL2,
	AUTOK_VCORE_LEVEL3,
	AUTOK_VCORE_MERGE,
	AUTOK_VCORE_NUM = AUTOK_VCORE_MERGE
};

/**********************************************************
 * Function Declaration                                   *
 **********************************************************/
extern int autok_path_sel(struct msdc_host *host);
extern int autok_init_sdr104(struct msdc_host *host);
extern int autok_init_hs200(struct msdc_host *host);
extern int autok_init_hs400(struct msdc_host *host);
extern void autok_msdc_tx_setting(struct msdc_host *host, struct mmc_ios *ios);
extern void autok_low_speed_switch_edge(struct msdc_host *host,
	struct mmc_ios *ios, enum ERROR_TYPE error_type);
extern void autok_tuning_parameter_init(struct msdc_host *host, u8 *res);
extern int autok_execute_tuning(struct msdc_host *host, u8 *res);
extern int hs200_execute_tuning(struct msdc_host *host, u8 *res);
extern int hs200_execute_tuning_cmd(struct msdc_host *host, u8 *res);
extern int hs400_execute_tuning(struct msdc_host *host, u8 *res);
extern int hs400_execute_tuning_cmd(struct msdc_host *host, u8 *res);
extern int autok_vcore_merge_sel(struct msdc_host *host,
	unsigned int merge_cap);
extern int emmc_execute_dvfs_autok(struct msdc_host *host, u32 opcode);
extern int sd_execute_dvfs_autok(struct msdc_host *host, u32 opcode);
extern int emmc_execute_autok(struct msdc_host *host, u32 opcode);
extern int sd_execute_autok(struct msdc_host *host, u32 opcode);
extern void msdc_init_tune_path(struct msdc_host *host, unsigned char timing);
extern void msdc_init_tune_setting(struct msdc_host *host);

#endif  /* _MTK_AUTOK_H_ */
