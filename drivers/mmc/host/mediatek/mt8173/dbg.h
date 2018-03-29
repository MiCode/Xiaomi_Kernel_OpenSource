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

#ifndef __MT_MSDC_DEUBG__
#define __MT_MSDC_DEUBG__
#include "mt_sd.h"
#include<mt-plat/upmu_common.h>

/* ========================== */
extern u32 sdio_pro_enable;

extern void __iomem *gpio_reg_base;
extern void __iomem *infracfg_ao_reg_base;
extern void __iomem *infracfg_reg_base;
extern void __iomem *pericfg_reg_base;
extern void __iomem *emi_reg_base;
extern void __iomem *toprgu_reg_base;
extern void __iomem *apmixed_reg_base1;
extern void __iomem *topckgen_reg_base;

#ifdef CFG_DEV_MSDC0
extern struct msdc_hw msdc0_hw;
#endif
#ifdef CFG_DEV_MSDC1
extern struct msdc_hw msdc1_hw;
#endif

extern void msdc_dump_info(u32 id);
extern struct msdc_host *mtk_msdc_host[];

#ifndef FPGA_PLATFORM
extern void msdc_set_driving(struct msdc_host *host, struct msdc_hw *hw, bool sd_18);
extern void msdc_set_sr(struct msdc_host *host, int clk, int cmd, int dat, int rst, int ds);
extern void msdc_set_smt(struct msdc_host *host, int set_smt);
extern void msdc_set_rdtdsel_dbg(struct msdc_host *host, bool rdsel, u32 value);
extern void msdc_get_rdtdsel_dbg(struct msdc_host *host, bool rdsel, u32 *value);
#endif
extern int ettagent_init(void);
extern void ettagent_exit(void);

extern int mmc_send_ext_csd(struct mmc_card *card, u8 *ext_csd);

#ifdef MSDC_HQA
extern void pmic_config_interface(unsigned int, unsigned int, unsigned int, unsigned int);
#endif

#ifdef ONLINE_TUNING_DVTTEST
extern int mt_msdc_online_tuning_test(struct msdc_host *host, u32 rawcmd, u32 rawarg, u8 rw);
#endif

/* for a type command, e.g. CMD53, 2 blocks */
struct cmd_profile {
	u32 max_tc;		/* Max tick count */
	u32 min_tc;
	u32 tot_tc;		/* total tick count */
	u32 tot_bytes;
	u32 count;		/* the counts of the command */
};

/* dump when total_tc and total_bytes */
struct sdio_profile {
	u32 total_tc;		/* total tick count of CMD52 and CMD53 */
	u32 total_tx_bytes;	/* total bytes of CMD53 Tx */
	u32 total_rx_bytes;	/* total bytes of CMD53 Rx */

	/*CMD52 */
	struct cmd_profile cmd52_tx;
	struct cmd_profile cmd52_rx;

	/*CMD53 in byte unit */
	struct cmd_profile cmd53_tx_byte[512];
	struct cmd_profile cmd53_rx_byte[512];

	/*CMD53 in block unit */
	struct cmd_profile cmd53_tx_blk[100];
	struct cmd_profile cmd53_rx_blk[100];
};

#ifdef MTK_MSDC_ERROR_TUNE_DEBUG
#define MTK_MSDC_ERROR_NONE (0)
#define MTK_MSDC_ERROR_CMD_TMO (0x1)
#define MTK_MSDC_ERROR_CMD_CRC (0x1 << 1)
#define MTK_MSDC_ERROR_DAT_TMO (0x1 << 2)
#define MTK_MSDC_ERROR_DAT_CRC (0x1 << 3)
#define MTK_MSDC_ERROR_ACMD_TMO (0x1 << 4)
#define MTK_MSDC_ERROR_ACMD_CRC (0x1 << 5)

extern unsigned int g_err_tune_dbg_host;
extern unsigned int g_err_tune_dbg_cmd;
extern unsigned int g_err_tune_dbg_arg;
extern unsigned int g_err_tune_dbg_error;
extern unsigned int g_err_tune_dbg_count;
#endif

/* ========================== */
typedef enum {
	SD_TOOL_ZONE = 0,
	SD_TOOL_DMA_SIZE  = 1,
	SD_TOOL_PM_ENABLE = 2,
	SD_TOOL_SDIO_PROFILE = 3,
	SD_TOOL_CLK_SRC_SELECT = 4,
	SD_TOOL_REG_ACCESS = 5,
	SD_TOOL_SET_DRIVING = 6,
	SD_TOOL_DESENSE = 7,
	RW_BIT_BY_BIT_COMPARE = 8,
	SMP_TEST_ON_ONE_HOST = 9,
	SMP_TEST_ON_ALL_HOST = 10,
	SD_TOOL_MSDC_HOST_MODE = 11,
	SD_TOOL_DMA_STATUS = 12,
	SD_TOOL_ENABLE_SLEW_RATE = 13,
	SD_TOOL_ENABLE_SMT = 14,
	MMC_PERF_DEBUG = 15,
	MMC_PERF_DEBUG_PRINT = 16,
	SD_TOOL_SET_RDTDSEL = 17,
	MMC_REGISTER_READ = 18,
	MMC_REGISTER_WRITE = 19,
	MSDC_READ_WRITE = 20,
	MMC_ERROR_TUNE = 21,
	MMC_EDC_EMMC_CACHE = 22,
	MMC_DUMP_GPD = 23,
	MMC_ETT_TUNE = 24,
	MMC_CRC_STRESS = 25,
	ENABLE_AXI_MODULE = 26,
} msdc_dbg;


typedef struct {
	unsigned char clk_drv;
	unsigned char cmd_drv;
	unsigned char dat_drv;
	unsigned char rst_drv;
	unsigned char ds_drv;
} drv_mod;

extern u32 dma_size[HOST_MAX_NUM];
extern struct msdc_host *mtk_msdc_host[HOST_MAX_NUM];	/* for fpga early porting */
extern unsigned char msdc_clock_src[HOST_MAX_NUM];
extern drv_mod msdc_drv_mode[HOST_MAX_NUM];
extern u32 msdc_host_mode[HOST_MAX_NUM];	/*SD/eMMC mode (HS/DDR/UHS) */
extern u32 msdc_host_mode2[HOST_MAX_NUM];
extern int g_dma_debug[HOST_MAX_NUM];


#ifdef MSDC_DMA_ADDR_DEBUG
extern struct dma_addr msdc_latest_dma_address[MAX_BD_PER_GPD];
#endif
extern struct dma_addr *msdc_get_dma_address(int host_id);
extern int msdc_get_dma_status(int host_id);
extern int emmc_multi_rw_compare(int host_num, uint address, int count);
extern int msdc_tune_write(struct msdc_host *host);
extern int msdc_tune_read(struct msdc_host *host);
extern int msdc_tune_cmdrsp(struct msdc_host *host);
extern int emmc_hs400_tune_rw(struct msdc_host *host);
extern void msdc_dump_gpd_bd(int id);
extern void msdc_dump_register(struct msdc_host *host);

extern int g_ett_tune;
extern int g_ett_hs400_tune;
extern int g_ett_cmd_tune;
extern int g_ett_read_tune;
extern int g_ett_write_tune;
extern int g_reset_tune;

extern u32 sdio_enable_tune;
extern u32 sdio_iocon_dspl;
extern u32 sdio_iocon_w_dspl;
extern u32 sdio_iocon_rspl;

extern u32 sdio_pad_tune_rrdly;
extern u32 sdio_pad_tune_rdly;
extern u32 sdio_pad_tune_wrdly;
extern u32 sdio_dat_rd_dly0_0;
extern u32 sdio_dat_rd_dly0_1;
extern u32 sdio_dat_rd_dly0_2;
extern u32 sdio_dat_rd_dly0_3;
extern u32 sdio_dat_rd_dly1_0;
extern u32 sdio_dat_rd_dly1_1;
extern u32 sdio_dat_rd_dly1_2;
extern u32 sdio_dat_rd_dly1_3;
extern u32 sdio_clk_drv;
extern u32 sdio_cmd_drv;
extern u32 sdio_data_drv;
extern u32 sdio_tune_flag;

int msdc_debug_proc_init(void);

extern void GPT_GetCounter64(u32 *cntL32, u32 *cntH32);
u32 msdc_time_calc(u32 old_L32, u32 old_H32, u32 new_L32, u32 new_H32);
void msdc_performance(u32 opcode, u32 sizes, u32 bRx, u32 ticks);

#endif
