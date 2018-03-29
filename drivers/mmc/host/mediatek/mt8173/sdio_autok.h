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

#ifndef MT6582_AUTOK_H
#define MT6582_AUTOK_H

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <mt-plat/mt_boot_common.h>

#include "mt_sd.h"
#include <mt_vcore_dvfs.h>

#define AUTOK_READ 0
#define AUTOK_WRITE 1

#define PROC_BUF_SIZE 512

/*************************************************************************
*           AutoK Implementation
*************************************************************************/
/* #define AUTOK_DEBUG */
#define USE_KERNEL_THREAD
/* #define CHANGE_SCHED_POLICY */
/* #define SCHED_POLICY_INFO */
#define PMIC_MT6323

/*TODO: just remove those def and include the correct one header*/
#define SDIO_IP_WTMDR	(0x00B0)
#define SDIO_IP_WTMCR	(0x00B4)
#define SDIO_IP_WTMDPCR0	(0x00B8)
#define SDIO_IP_WTMDPCR1	(0x00BC)
#define SDIO_IP_WPLRCR	(0x00D4)
#define TEST_MODE_STATUS        (0x100)

struct sdio_autok_params {
	u32 cmd_edge;
	u32 rdata_edge;
	u32 wdata_edge;
	u32 clk_drv;
	u32 cmd_drv;
	u32 dat_drv;
	u32 dat0_rd_dly;
	u32 dat1_rd_dly;
	u32 dat2_rd_dly;
	u32 dat3_rd_dly;
	u32 dat_wrd_dly;
	u32 cmd_resp_rd_dly;
	u32 cmd_rd_dly;
	u32 int_dat_latch_ck;
	u32 ckgen_msdc_dly_sel;
	u32 cmd_rsp_ta_cntr;
	u32 wrdat_crcs_ta_cntr;
	u32 pad_clk_txdly;
};

typedef struct {
	unsigned int sel;
} S_AUTOK_DATA;

typedef union {
	unsigned int version;
	unsigned int freq;
	S_AUTOK_DATA data;
} U_AUTOK_INTERFACE_DATA;

#ifdef USE_KERNEL_THREAD
struct sdio_autok_thread_data {
	struct msdc_host *host;
	struct autok_predata *p_autok_predata;
	char stage;
	struct autok_progress *p_autok_progress;
	u8 *is_autok_done;
	struct completion *autok_completion;
	char *log;
};
#else				/* USE_KERNEL_THREAD */
struct sdio_autok_workqueue_data {
	struct delayed_work autok_delayed_work;
	struct msdc_host *host;
	struct autok_predata *p_autok_predata;
	char stage;
};
#endif				/* USE_KERNEL_THREAD */

struct log_mmap_info {
	char *data;		/* the data */
	int reference;		/* how many times it is mmapped */
	int size;
};

typedef enum {
	 /*CMD*/ E_MSDC_PAD_TUNE_CMDRRDLY = 0,
	E_MSDC_CMD_RSP_TA_CNTR,
	E_MSDC_IOCON_RSPL,
	E_MSDC_CKGEN_MSDC_DLY_SEL,
	E_MSDC_PAD_TUNE_CMDRDLY,
	 /*READ*/ E_MSDC_INT_DAT_LATCH_CK_SEL,
#if 1
	E_MSDC_IOCON_RDSPL,
	E_MSDC_PAD_TUNE_DATRRDLY,
#else
	E_IOCON_RD0SPL,
	E_IOCON_RD1SPL,
	E_IOCON_RD2SPL,
	E_IOCON_RD3SPL,
	E_DAT_RDDLY0_D0,
	E_DAT_RDDLY0_D1,
	E_DAT_RDDLY0_D2,
	E_DAT_RDDLY0_D3,
#endif
	 /*WRITE*/ E_MSDC_WRDAT_CRCS_TA_CNTR,
	E_MSDC_IOCON_WDSPL,
	E_MSDC_PAD_TUNE_DATWRDLY,

	E_MSDC_PAD_DLY_PERIOD,
	E_MSDC_CMD_INT_MARGIN,

	E_MSDC_F_TINY_MARGIN,

	E_AUTOK_VERSION,
	E_AUTOK_FREQ,
	E_AUTOK_PARM_MAX
} E_AUTOK_PARAM;

#define MAX_AUTOK_DAT_NUM       (E_AUTOK_PARM_MAX)
#define E_AUTOK_DLY_PARAM_MAX   (E_MSDC_PAD_TUNE_DATWRDLY+1)

#define LTE_MODEM_FUNC (1)
#define CMD_52         (52)
#define CMD_53         (53)

#define REQ_CMD_EIO    (0x1 << 0)
#define REQ_CMD_TMO    (0x1 << 1)
#define REQ_DAT_ERR    (0x1 << 2)

#define MSDC_READ      (0)
#define MSDC_WRITE     (1)
#define LOG_SIZE (PAGE_SIZE*8)


enum AUTOK_PARAM {
	CMD_EDGE,		/* command response sample selection (MSDC_SMPL_RISING, MSDC_SMPL_FALLING) */
	RDATA_EDGE,		/* read data sample selection (MSDC_SMPL_RISING, MSDC_SMPL_FALLING) */
	WDATA_EDGE,		/* write data sample selection (MSDC_SMPL_RISING, MSDC_SMPL_FALLING) */
	CLK_DRV,		/* clock driving */
	CMD_DRV,		/* command driving */
	DAT_DRV,		/* data driving */
	DAT0_RD_DLY,		/* DAT0 Pad RX Delay Line Control (for MSDC RD), Total 32 stages */
	DAT1_RD_DLY,		/* DAT1 Pad RX Delay Line Control (for MSDC RD), Total 32 stages */
	DAT2_RD_DLY,		/* DAT2 Pad RX Delay Line Control (for MSDC RD), Total 32 stages */
	DAT3_RD_DLY,		/* DAT3 Pad RX Delay Line Control (for MSDC RD), Total 32 stages */
	DAT_WRD_DLY,		/* Write Data Status Internal Delay Line Control.                */
				/* This register is used to fine-tune write status phase latched */
				/* by MSDC internal clock. Total 32 stages			 */
	DAT_RD_DLY,		/* Rx  Delay Line Control. Total 32 stages			 */
	CMD_RESP_RD_DLY,	/* CMD Response Internal Delay Line Control. This register is    */
				/* used to fine-tune response phase  latched by MSDC internal    */
				/* clock. Total 32 stages					 */
	CMD_RD_DLY,		/* CMD Pad RX Delay Line Control. This register is used to       */
				/* fine-tune CMD pad macro respose latch timing. Total 32 stages */
	DATA_DLYLINE_SEL,	/* Data line delay line fine tune selection. 1'b0: All data line */
				/* share one delay selection value indicated by                  */
				/* PAD_TUNE.PAD_DAT_RD_RXDLY. 1'b1: Each data line has its own   */
				/* delay selection value indicated by Data line (x):             */
				/* DAT_RD_DLY(x).DAT0_RD_DLY					 */
	READ_DATA_SMPL_SEL,	/* Data line rising/falling latch  fine tune selection in read   */
				/* transaction. 1'b0: All data line share one value indicated by */
				/* MSDC_IOCON.R_D_SMPL. 1'b1: Each data line has its own         */
				/* selection value indicated by Data line (x):                   */
				/* MSDC_IOCON.R_D(x)_SMPL					 */
	WRITE_DATA_SMPL_SEL,	/* Data line rising/falling latch  fine tune selection in write  */
				/* transaction. 1'b0: All data line share one value indicated by */
				/* MSDC_IOCON.W_D_SMPL. 1'b1: Each data line has its own         */
				/* selection value indicated by Data line (x):                   */
				/* MSDC_IOCON.W_D(x)_SMPL					 */
	INT_DAT_LATCH_CK,	/* Internal MSDC clock phase selection. Total 8 stages, each     */
				/* stage can delay 1 clock period of msdc_src_ck		 */
	CKGEN_MSDC_DLY_SEL,	/* CKBUF in CKGEN Delay Selection. Total 32 stages */
	CMD_RSP_TA_CNTR,	/* CMD response turn around period. The turn around cycle =      */
				/* CMD_RSP_TA_CNTR + 2, Only for USH104 mode, this register      */
				/* should be set to 0 in non-UHS104 mode			 */
	WRDAT_CRCS_TA_CNTR,	/* Write data and CRC status turn around period. The turn around */
				/* cycle = WRDAT_CRCS_TA_CNTR + 2, Only for USH104 mode,  this   */
				/* register should be set to 0 in non-UHS104 mode		 */
	PAD_CLK_TXDLY,		/* CLK Pad TX Delay Control. This register is used to add delay  */
				/* to CLK phase. Total 32 stages				 */
	TOTAL_PARAM_COUNT
};

struct autok_progress {
	u32 host_id;
	u32 done;
	u32 fail;
};

struct autok_predata {
	u8 vol_count;
	u8 param_count;
	unsigned int *vol_list;

	U_AUTOK_INTERFACE_DATA **ai_data;
};


int msdc_autok_read(struct msdc_host *host, unsigned int u4Addr, unsigned int u4Func, void *pBuffer,
		    unsigned int u4Len, unsigned int u4Cmd);
int msdc_autok_write(struct msdc_host *host, unsigned int u4Addr, unsigned int u4Func,
		     void *pBuffer, unsigned int u4Len, unsigned int u4Cmd);
int msdc_autok_adjust_param(struct msdc_host *host, enum AUTOK_PARAM param, u32 *value, int rw);
int msdc_autok_stg1_cal(struct msdc_host *host, unsigned int offset_restore,
			struct autok_predata *p_single_autok);
/* int msdc_autok_stg1_data_get(void **ppData, int *pLen); */
int msdc_autok_stg2_cal(struct msdc_host *host, struct autok_predata *p_autok_data,
			unsigned int vcore_uv_off);
int msdc_autok_apply_param(struct msdc_host *host, unsigned int vcore_uv_off);
int msdc_autok_get_suggetst_vcore(unsigned int **suggest_vol_tbl);
extern char *reset_autok_cursor(int voltage);
/* extern void clear_autok_buf(); */
bool is_vcore_ss_corner(void);


/*****************************************************************************
 *                         Functions Declearation                            *
 *****************************************************************************/

extern unsigned int sdio_get_rings(unsigned int *io_ring, unsigned int *core_ring);
extern unsigned int msdc_do_command(struct msdc_host *host,
				    struct mmc_command *cmd, int tune, unsigned long timeout);
extern int msdc_pio_read(struct msdc_host *host, struct mmc_data *data);
extern int msdc_pio_write(struct msdc_host *host, struct mmc_data *data);

extern struct msdc_host *mtk_msdc_host[];

/* Auto-K Thread function */
extern volatile int sdio_autok_processed;

extern unsigned int autok_get_current_vcore_offset(void);
#ifdef CONFIG_SDIOAUTOK_SUPPORT
extern unsigned int g_autok_vcore_sel[];
#endif

extern void mmc_set_clock(struct mmc_host *host, unsigned int hz);
extern void msdc_ungate_clock(struct msdc_host *host);
extern void msdc_gate_clock(struct msdc_host *host, int delay);
int send_autok_uevent(char *text, struct msdc_host *host);

/* static DEFINE_SPINLOCK(autok_lock); */
/*extern char *log_info;*/
/*extern int total_msg_size;*/
extern void msdc_sdio_set_long_timing_delay_by_freq(struct msdc_host *host, u32 clock);

/* CALLBACK for device wait */

#endif				/* end of MT6582_AUTOK_H */
