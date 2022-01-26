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

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/io.h>

#include <linux/time.h>
#include <linux/delay.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio.h>

#include <linux/completion.h>
#include <linux/scatterlist.h>

#include "autok.h"
#include "mtk_sd.h"
#include "autok_cust.h"
#include "msdc_cust.h"
#include "mmc/core/card.h"

/* 100ms */
#define AUTOK_CMD_TIMEOUT            (HZ / 10)
/* 1s x 3 */
#define AUTOK_DAT_TIMEOUT            (HZ * 3)
#define MSDC_FIFO_THD_1K                (1024)
#define TUNE_TX_CNT                     (10)
#define CHECK_QSR                       (0x800D)
#define TUNE_DATA_TX_ADDR               (0x358000)
#define CMDQ
#define AUTOK_CMD_TIMES                 (20)
/* scan result may find xxxxooxxx */
#define AUTOK_TUNING_INACCURACY      (10)
#define AUTOK_MARGIN_THOLD              (5)
#define AUTOK_BD_WIDTH_REF              (3)

#define AUTOK_READ                      0
#define AUTOK_WRITE                     1

#define AUTOK_FINAL_CKGEN_SEL           (0)
#define SCALE_TA_CNTR                   (8)
#define SCALE_CMD_RSP_TA_CNTR           (8)
#define SCALE_WDAT_CRC_TA_CNTR          (8)
#define SCALE_INT_DAT_LATCH_CK_SEL      (8)
#define SCALE_INTERNAL_DLY_CNTR         (32)
#define SCALE_PAD_DAT_DLY_CNTR          (32)

#define TUNING_INACCURACY (2)

enum TUNE_TYPE {
	TUNE_CMD = 0,
	TUNE_DATA,
	TUNE_LATCH_CK,
	TUNE_SDIO_PLUS,
};

#define autok_msdc_retry(expr, retry, cnt) \
	do { \
		int backup = cnt; \
		while (retry) { \
			if (!(expr)) \
				break; \
			if (cnt-- == 0) { \
				retry--; cnt = backup; \
			} \
		} \
	WARN_ON(retry == 0); \
} while (0)

#define autok_msdc_reset() \
	do { \
		int retry = 3, cnt = 1000; \
		MSDC_SET_BIT32(MSDC_CFG, MSDC_CFG_RST); \
		/* ensure reset operation be sequential  */ \
		mb(); \
		autok_msdc_retry(MSDC_READ32(MSDC_CFG) & MSDC_CFG_RST, \
			retry, cnt); \
	} while (0)

#define msdc_rxfifocnt() \
	((MSDC_READ32(MSDC_FIFOCS) & MSDC_FIFOCS_RXCNT) >> 0)
#define msdc_txfifocnt() \
	((MSDC_READ32(MSDC_FIFOCS) & MSDC_FIFOCS_TXCNT) >> 16)

#define wait_cond_tmo(cond, tmo) \
	do { \
		unsigned long timeout = jiffies + tmo; \
		while (1) { \
			if ((cond) || (tmo == 0)) \
				break; \
			if (time_after(jiffies, timeout) && (!cond)) \
				tmo = 0; \
		} \
	} while (0)

#define msdc_clear_fifo() \
	do { \
		int retry = 5, cnt = 1000; \
		MSDC_SET_BIT32(MSDC_FIFOCS, MSDC_FIFOCS_CLR); \
		/* ensure fifo clear operation be sequential  */ \
		mb(); \
		autok_msdc_retry(MSDC_READ32(MSDC_FIFOCS) & MSDC_FIFOCS_CLR, \
			retry, cnt); \
	} while (0)

struct AUTOK_PARAM_RANGE {
	unsigned int start;
	unsigned int end;
};

struct AUTOK_PARAM_INFO {
	struct AUTOK_PARAM_RANGE range;
	char *param_name;
};

struct BOUND_INFO {
	unsigned int Bound_Start;
	unsigned int Bound_End;
	unsigned int Bound_width;
	bool is_fullbound;
};

/* Max Allowed Boundary Number */
#define BD_MAX_CNT 4
struct AUTOK_SCAN_RES {
	/* Bound info record, currently only allow max to 2 bounds exist,
	 * but in extreme case, may have 4 bounds
	 */
	struct BOUND_INFO bd_info[BD_MAX_CNT];
	/* Bound cnt record, must be in rang [0,3] */
	unsigned int bd_cnt;
	/* Full boundary cnt record */
	unsigned int fbd_cnt;
};

struct AUTOK_REF_INFO {
	/* inf[0] - rising edge res, inf[1] - falling edge res */
	struct AUTOK_SCAN_RES scan_info[2];
	/* optimised sample edge select */
	unsigned int opt_edge_sel;
	/* optimised dly cnt sel */
	unsigned int opt_dly_cnt;
	/* 1clk cycle equal how many delay cell cnt, if cycle_cnt is 0,
	 * that is cannot calc cycle_cnt by current Boundary info
	 */
	unsigned int cycle_cnt;
};

struct BOUND_INFO_NEW {
	/* boundary start and end position */
	unsigned char bd_s;
	unsigned char bd_e;
};

/* Max Allowed Boundary Number */
#define BD_MAX_CNT_NEW 32
struct AUTOK_SCAN_RES_NEW {
	/* bd info record, currently only allow max to 32 fail bounds exist */
	struct BOUND_INFO_NEW fail_info[BD_MAX_CNT_NEW];
	struct BOUND_INFO_NEW pass_info[BD_MAX_CNT_NEW];
	/* bd cnt record */
	unsigned char fail_cnt;
	unsigned char pass_cnt;
};
struct AUTOK_REF_INFO_NEW {
	/* inf[0] - rising edge res, inf[1] - falling edge res */
	struct AUTOK_SCAN_RES_NEW scan_info[2];
	/* optimised sample edge select */
	unsigned int opt_edge_sel;
	/* optimised dly cnt sel */
	unsigned int opt_dly_cnt;
	/* 1clk cycle equal how many delay cell cnt, if cycle_cnt is 0,
	 * that is cannot calc cycle_cnt by current Boundary info
	 */
	unsigned int cycle_cnt;
};

enum AUTOK_TX_SCAN_STA_E {
	START_POSITION = 0,
	PASS_POSITION,
	FAIL_POSITION,
};

enum AUTOK_SCAN_WIN {
	CMD_RISE,
	CMD_FALL,
	DAT_RISE,
	DAT_FALL,
	DS_CMD_WIN,
	DS_DATA_WIN,
	D_CMD_RX,
	D_DATA_RX,
	H_CMD_TX,
	H_DATA_TX,
};

enum EXD_RW_FLAG {
	EXT_READ = 0,
	EXT_WRITE,
};

struct autok_host {
	u32 clk_tx;
	u32 fifo_tune;
};

unsigned int autok_debug_level = AUTOK_DBG_RES;

const struct AUTOK_PARAM_INFO autok_param_info[] = {
	{{0, 1}, "CMD_EDGE"},
	{{0, 1}, "CMD_FIFO_EDGE"},
	/* async fifo mode Pad dat edge must fix to 0 */
	{{0, 1}, "RDATA_EDGE"},
	{{0, 1}, "RD_FIFO_EDGE"},
	{{0, 1}, "WD_FIFO_EDGE"},

	/* Cmd Pad Tune Data Phase */
	{{0, 31}, "CMD_RD_D_DLY1"},
	{{0, 1}, "CMD_RD_D_DLY1_SEL"},
	{{0, 31}, "CMD_RD_D_DLY2"},
	{{0, 1}, "CMD_RD_D_DLY2_SEL"},

	/* Data Pad Tune Data Phase */
	{{0, 31}, "DAT_RD_D_DLY1"},
	{{0, 1}, "DAT_RD_D_DLY1_SEL"},
	{{0, 31}, "DAT_RD_D_DLY2"},
	{{0, 1}, "DAT_RD_D_DLY2_SEL"},

	/* Latch CK for data read when clock stop */
	{{0, 7}, "INT_DAT_LATCH_CK"},

	/* eMMC50 Related tuning param */
	{{0, 31}, "EMMC50_DS_Z_DLY1"},
	{{0, 1}, "EMMC50_DS_Z_DLY1_SEL"},
	{{0, 31}, "EMMC50_DS_Z_DLY2"},
	{{0, 1}, "EMMC50_DS_Z_DLY2_SEL"},
	{{0, 31}, "EMMC50_DS_ZDLY_DLY"},
	{{0, 31}, "EMMC50_CMD_TX_DLY"},
	{{0, 31}, "EMMC50_DATA0_TX_DLY"},
	{{0, 31}, "EMMC50_DATA1_TX_DLY"},
	{{0, 31}, "EMMC50_DATA2_TX_DLY"},
	{{0, 31}, "EMMC50_DATA3_TX_DLY"},
	{{0, 31}, "EMMC50_DATA4_TX_DLY"},
	{{0, 31}, "EMMC50_DATA5_TX_DLY"},
	{{0, 31}, "EMMC50_DATA6_TX_DLY"},
	{{0, 31}, "EMMC50_DATA7_TX_DLY"},
	/* tx clk dly fix to 0 for HQA res */
	{{0, 31}, "PAD_CLK_TXDLY_AUTOK"},

	/* ================================================= */
	/* Timming Related Mux & Common Setting Config */
	/* all data line path share sample edge */
	{{0, 1}, "READ_DATA_SMPL_SEL"},
	{{0, 1}, "WRITE_DATA_SMPL_SEL"},
	/* clK tune all data Line share dly */
	{{0, 1}, "DATA_DLYLINE_SEL"},
	/* data tune mode select */
	{{0, 1}, "MSDC_WCRC_ASYNC_FIFO_SEL"},
	/* data tune mode select */
	{{0, 1}, "MSDC_RESP_ASYNC_FIFO_SEL"},
	/* eMMC50 Function Mux */
	/* write path switch to emmc45 */
	{{0, 1}, "EMMC50_WDATA_MUX_EN"},
	/* response path switch to emmc45 */
	{{0, 1}, "EMMC50_CMD_MUX_EN"},
	{{0, 1}, "EMMC50_CMD_RESP_LATCH"},
	{{0, 1}, "EMMC50_WDATA_EDGE"},
	/* Common Setting Config */
	{{0, 31}, "CKGEN_MSDC_DLY_SEL"},
	{{1, 7}, "CMD_RSP_TA_CNTR"},
	{{1, 7}, "WRDAT_CRCS_TA_CNTR"},
	{{0, 1}, "SDC_RX_ENHANCE"},
};

/**********************************************************
 * AutoK Basic Interface Implenment                       *
 **********************************************************/
static int autok_sdio_device_rx_set(struct msdc_host *host,
unsigned int func_num, unsigned int base_addr,
unsigned int *reg_value, unsigned int r_w_dirc,
unsigned int opcode)
{
	void __iomem *base = host->base;
	unsigned int rawcmd = 0;
	unsigned int arg = 0;
	unsigned int sts = 0;
	unsigned int wints = 0;
	unsigned long tmo = 0;
	unsigned long write_tmo = 0;
	int ret = E_RES_PASS;

	switch (opcode) {
	case SD_IO_RW_DIRECT:
		rawcmd = (1 << 7) | (52);
		arg = (r_w_dirc << 31) | (func_num << 28)
			| (base_addr << 9) | (*reg_value)
			| ((r_w_dirc) ? 0x08000000 : 0x00000000);
		MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case SD_IO_RW_EXTENDED:
		rawcmd = (4 << 16) | (r_w_dirc << 13)
			| (1 << 11) | (1 << 7) | (53);
		arg = (r_w_dirc << 31) | (func_num << 28)
			| (base_addr << 9) | (0 << 26) | (0 << 27) | (4);
		MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	}
	tmo = AUTOK_DAT_TIMEOUT;
	wait_cond_tmo(!(MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY), tmo);
	if (tmo == 0) {
		AUTOK_RAWPRINT("[AUTOK]DRS MSDC busy tmo1\n");
		ret = E_RES_FATAL_ERR;
		goto end;
	}

	/* clear fifo */
	autok_msdc_reset();
	msdc_clear_fifo();
	MSDC_WRITE32(MSDC_INT, 0xffffffff);

	/* start command */
	MSDC_WRITE32(SDC_ARG, arg);
	MSDC_WRITE32(SDC_CMD, rawcmd);

	/* wait interrupt status */
	wints = MSDC_INT_CMDTMO | MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR;
	tmo = AUTOK_CMD_TIMEOUT;
	wait_cond_tmo(((sts = MSDC_READ32(MSDC_INT)) & wints), tmo);
	if (tmo == 0) {
		AUTOK_RAWPRINT("[AUTOK]DRS wait int tmo\r\n");
		ret |= E_RES_CMD_TMO;
		goto end;
	}

	MSDC_WRITE32(MSDC_INT, (sts & wints));
	if (sts == 0) {
		ret |= E_RES_CMD_TMO;
		goto end;
	}

	if (sts & MSDC_INT_CMDRDY)
		ret |= E_RES_PASS;
	else if (sts & MSDC_INT_RSPCRCERR) {
		ret |= E_RES_RSP_CRC;
		AUTOK_RAWPRINT("[AUTOK]DRS HW crc\r\n");
		goto end;
	} else if (sts & MSDC_INT_CMDTMO) {
		AUTOK_RAWPRINT("[AUTOK]DRS HW tmo\r\n");
		ret |= E_RES_CMD_TMO;
		goto end;
	}

	tmo = jiffies + AUTOK_DAT_TIMEOUT;
	while ((MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY) && (tmo != 0)) {
		if (time_after(jiffies, tmo))
			tmo = 0;
		if ((r_w_dirc == EXT_WRITE) && (opcode == SD_IO_RW_EXTENDED)) {
			MSDC_WRITE32(MSDC_TXDATA, *reg_value);
			write_tmo = AUTOK_DAT_TIMEOUT;
			wait_cond_tmo(
				!(MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY),
				write_tmo);
			if (write_tmo == 0) {
				AUTOK_RAWPRINT("[AUTOK]DRS MSDC busy tmo2\n");
				ret |= E_RES_FATAL_ERR;
				goto end;
			}
		}
	}
	if ((tmo == 0) && (MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY)) {
		AUTOK_RAWPRINT("[AUTOK]DRS MSDC busy tmo3...\n");
		ret |= E_RES_FATAL_ERR;
		goto end;
	}

	sts = MSDC_READ32(MSDC_INT);
	wints = MSDC_INT_XFER_COMPL | MSDC_INT_DATCRCERR | MSDC_INT_DATTMO;
	if (sts) {
		/* clear status */
		MSDC_WRITE32(MSDC_INT, (sts & wints));
		if (sts & MSDC_INT_XFER_COMPL) {
			if ((r_w_dirc == EXT_READ)
				&& (opcode == SD_IO_RW_EXTENDED)) {
				*reg_value = MSDC_READ32(MSDC_RXDATA);
			}
			ret |= E_RES_PASS;
		}
		if (MSDC_INT_DATCRCERR & sts) {
			ret |= E_RES_DAT_CRC;
			AUTOK_RAWPRINT("[AUTOK]DRS dat crc...\n");
		}
		if (MSDC_INT_DATTMO & sts) {
			ret |= E_RES_DAT_TMO;
			AUTOK_RAWPRINT("[AUTOK]DRS dat tmo...\n");
		}
	}
end:

	return ret;
}

/* define the function to shrink code's column */
static void rx_read(struct msdc_host *host, unsigned int value)
{
	int i = 0;
	void __iomem *base = host->base;

	for (i = 0; i < (MSDC_FIFO_SZ - 64)/4; i++)
		value = MSDC_READ32(MSDC_RXDATA);
}

static int autok_send_tune_cmd(struct msdc_host *host, unsigned int opcode,
	enum TUNE_TYPE tune_type_value, struct autok_host *host_para)
{
	void __iomem *base = host->base;
	unsigned int value;
	unsigned int rawcmd = 0;
	unsigned int arg = 0;
	unsigned int sts = 0;
	unsigned int wints = 0;
	unsigned long tmo = 0;
	unsigned long write_tmo = 0;
	unsigned int left = 0;
	unsigned int fifo_have = 0;
	unsigned int fifo_1k_cnt = 0;
	unsigned int i = 0;
	int ret = E_RES_PASS;
	unsigned int clk_tx_pre = 0;
	u8 bus_width;

	switch (opcode) {
	case MMC_SEND_EXT_CSD:
		rawcmd =  (512 << 16) | (0 << 13)
			| (1 << 11) | (1 << 7) | (8);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			MSDC_WRITE32(SDC_BLK_NUM, host->tune_latch_ck_cnt);
		else
			MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case MMC_STOP_TRANSMISSION:
		rawcmd = (1 << 14)  | (7 << 7) | (12);
		arg = 0;
		break;
	case MMC_SEND_STATUS:
		rawcmd = (1 << 7) | (13);
		arg = (1 << 16);
		break;
	case CHECK_QSR:
		rawcmd = (1 << 7) | (13);
		arg = (1 << 16) | (1 << 15);
		break;
	case MMC_SET_BLOCK_COUNT:
		rawcmd = (1 << 7) | (23);
		arg = 1;
		break;
	case MMC_WRITE_MULTIPLE_BLOCK:
		rawcmd =  (512 << 16) | (1 << 13)
			| (2 << 11) | (1 << 7) | (25);
		arg = TUNE_DATA_TX_ADDR;
		MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case MMC_SWITCH_CQ_EN:
		rawcmd = (7 << 7) | (6);
		arg = (3 << 24) | (15 << 16) | (1 << 8) | (1);
		break;
	case MMC_SWITCH_CQ_DIS:
		rawcmd = (7 << 7) | (6);
		arg = (3 << 24) | (15 << 16) | (0 << 8) | (1);
		break;
	case MMC_QUE_TASK_PARAMS_RD:
		rawcmd = (1 << 7) | (44);
		arg = (1 << 30) | (0 << 16) | (1);
		break;
	case MMC_QUE_TASK_PARAMS_WR:
		rawcmd = (1 << 7) | (44);
		arg = (0 << 30) | (0 << 16) | (1);
		break;
	case MMC_QUE_TASK_ADDR:
		rawcmd = (1 << 7) | (45);
		arg = TUNE_DATA_TX_ADDR;
		break;
	case MMC_EXECUTE_READ_TASK:
		rawcmd = (512 << 16) | (0 << 13)
			| (1 << 11) | (1 << 7) | (46);
		arg = (0 << 16);
		if (tune_type_value == TUNE_LATCH_CK)
			MSDC_WRITE32(SDC_BLK_NUM, host->tune_latch_ck_cnt);
		else
			MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case MMC_EXECUTE_WRITE_TASK:
		rawcmd = (512 << 16) | (1 << 13)
			| (1 << 11) | (1 << 7) | (47);
		arg = (0 << 16);
		MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case MMC_CMDQ_TASK_MGMT:
		rawcmd = (1 << 14) | (7 << 7) | (48);
		arg = (0 << 16) | (2);
		break;
	case MMC_READ_SINGLE_BLOCK:
		left = 512;
		rawcmd = (512 << 16) | (0 << 13)
			| (1 << 11) | (1 << 7) | (17);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			MSDC_WRITE32(SDC_BLK_NUM, host->tune_latch_ck_cnt);
		else
			MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case MMC_SEND_TUNING_BLOCK:
		left = 64;
		rawcmd = (64 << 16) | (0 << 13)
			| (1 << 11) | (1 << 7) | (19);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			MSDC_WRITE32(SDC_BLK_NUM, host->tune_latch_ck_cnt);
		else
			MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case MMC_SEND_TUNING_BLOCK_HS200:
		MSDC_GET_FIELD(SDC_CFG, SDC_CFG_BUSWIDTH, bus_width);
		if (bus_width == 2) {
			left = 128;
			rawcmd = (128 << 16) | (0 << 13)
				| (1 << 11) | (1 << 7) | (21);
			arg = 0;
		} else if (bus_width == 1) {
			left = 64;
			rawcmd = (64 << 16) | (0 << 13)
				| (1 << 11) | (1 << 7) | (21);
			arg = 0;
		}
		if (tune_type_value == TUNE_LATCH_CK)
			MSDC_WRITE32(SDC_BLK_NUM, host->tune_latch_ck_cnt);
		else
			MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case MMC_WRITE_BLOCK:
		/* get clk tx dly, for SD card tune clk tx */
		MSDC_GET_FIELD(MSDC_PAD_TUNE0,
			MSDC_PAD_TUNE0_CLKTXDLY,
			clk_tx_pre);
		rawcmd = (512 << 16) | (1 << 13)
			| (1 << 11) | (1 << 7) | (24);
		arg = TUNE_DATA_TX_ADDR;
		MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case SD_IO_RW_DIRECT:
		rawcmd = (1 << 7) | (52);
		arg = (0x80000000) | (1 << 28)
			| (SDIO_CCCR_ABORT << 9) | (0);
		MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case SD_IO_RW_EXTENDED:
		rawcmd = (4 << 16) | (1 << 13) | (1 << 11) | (1 << 7) | (53);
		arg = (0x80000000) | (1 << 28)
			| (0xB0 << 9) | (0 << 26) | (0 << 27) | (4);
		MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	}

	tmo = AUTOK_DAT_TIMEOUT;
	wait_cond_tmo(!(MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY), tmo);
	if (tmo == 0) {
		AUTOK_RAWPRINT("[AUTOK]MSDC busy tmo1 cmd%d\n", opcode);
		ret |= E_RES_FATAL_ERR;
		goto end;
	}

	/* clear fifo */
	if ((tune_type_value == TUNE_CMD) || (tune_type_value == TUNE_DATA)
		|| (tune_type_value == TUNE_SDIO_PLUS)) {
		if ((tune_type_value == TUNE_CMD)
			&& (host->hw->host_function == MSDC_EMMC))
			MSDC_WRITE32(MSDC_INT,
				MSDC_INT_CMDTMO
				| MSDC_INT_CMDRDY
				| MSDC_INT_RSPCRCERR);
		else {
			autok_msdc_reset();
			msdc_clear_fifo();
			MSDC_WRITE32(MSDC_INT, 0xffffffff);
		}
	}

	/* start command */
	MSDC_WRITE32(SDC_ARG, arg);
	MSDC_WRITE32(SDC_CMD, rawcmd);

	/* wait interrupt status */
	wints = MSDC_INT_CMDTMO | MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR;
	tmo = AUTOK_CMD_TIMEOUT;
	wait_cond_tmo(((sts = MSDC_READ32(MSDC_INT)) & wints), tmo);
	if (tmo == 0) {
		AUTOK_RAWPRINT("[AUTOK]CMD%d wait int tmo\r\n", opcode);
		ret |= E_RES_FATAL_ERR;
		goto end;
	}

	MSDC_WRITE32(MSDC_INT, (sts & wints));
	if (sts == 0) {
		ret |= E_RES_CMD_TMO;
		goto end;
	}

	if (sts & MSDC_INT_CMDRDY) {
		if (tune_type_value == TUNE_CMD) {
			ret |= E_RES_PASS;
			goto end;
		}
	} else if (sts & MSDC_INT_RSPCRCERR) {
		ret |= E_RES_RSP_CRC;
		if (tune_type_value != TUNE_SDIO_PLUS)
			goto end;
	} else if (sts & MSDC_INT_CMDTMO) {
		AUTOK_RAWPRINT("[AUTOK]CMD%d HW tmo\r\n", opcode);
		ret |= E_RES_CMD_TMO;
		if (tune_type_value != TUNE_SDIO_PLUS)
			goto end;
	}

	if ((tune_type_value != TUNE_LATCH_CK)
		&& (tune_type_value != TUNE_DATA)
		&& (tune_type_value != TUNE_SDIO_PLUS))
		goto skip_tune_latch_ck_and_tune_data;
	tmo = jiffies + AUTOK_DAT_TIMEOUT;
	while ((MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY) && (tmo != 0)) {
		if (time_after(jiffies, tmo))
			tmo = 0;
		if (tune_type_value == TUNE_LATCH_CK) {
			fifo_have = msdc_rxfifocnt();
			if ((opcode == MMC_SEND_TUNING_BLOCK_HS200)
				|| (opcode == MMC_READ_SINGLE_BLOCK)
				|| (opcode == MMC_SEND_EXT_CSD)
				|| (opcode == MMC_SEND_TUNING_BLOCK)
				|| (opcode == MMC_EXECUTE_READ_TASK)) {
				MSDC_SET_FIELD(MSDC_DBG_SEL,
					0xffff << 0, 0x0b);
				MSDC_GET_FIELD(MSDC_DBG_OUT,
					0x7ff << 0, fifo_1k_cnt);
				if ((fifo_1k_cnt >= MSDC_FIFO_THD_1K) &&
					(fifo_have >= MSDC_FIFO_SZ) &&
					(host_para->fifo_tune == 1)) {
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
				} else if ((fifo_have >= MSDC_FIFO_SZ) &&
					(host_para->fifo_tune == 0)) {
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
				}
			}
		} else if ((tune_type_value == TUNE_DATA)
		&& ((opcode == MMC_WRITE_BLOCK)
		|| (opcode == MMC_EXECUTE_WRITE_TASK)
		|| (opcode == MMC_WRITE_MULTIPLE_BLOCK))) {
			if (host->hw->host_function == MSDC_SD) {
				MSDC_SET_FIELD(MSDC_PAD_TUNE0,
					MSDC_PAD_TUNE0_CLKTXDLY,
					host_para->clk_tx);
				for (i = 0; i < 63; i++) {
					MSDC_WRITE32(MSDC_TXDATA, 0xa5a5a5a5);
					MSDC_WRITE32(MSDC_TXDATA, 0x1c345678);
				}
				/* restore clk tx brefore half data transfer */
				MSDC_SET_FIELD(MSDC_PAD_TUNE0,
					MSDC_PAD_TUNE0_CLKTXDLY,
					clk_tx_pre);
				MSDC_WRITE32(MSDC_TXDATA, 0xa5a5a5a5);
				MSDC_WRITE32(MSDC_TXDATA, 0x1c345678);
			} else {
				for (i = 0; i < 64; i++) {
					MSDC_WRITE32(MSDC_TXDATA, 0xf0f0f0f0);
					MSDC_WRITE32(MSDC_TXDATA, 0x0f0f0f0f);
				}
			}
			write_tmo = AUTOK_DAT_TIMEOUT;
			wait_cond_tmo(
				!(MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY),
				write_tmo);
			if (write_tmo == 0) {
				AUTOK_RAWPRINT("[AUTOK]MSDC busy tmo2 cmd%d\n",
				    opcode);
				ret |= E_RES_FATAL_ERR;
				goto end;
			}
			if (tune_type_value == TUNE_LATCH_CK) {
				if ((opcode == MMC_SEND_TUNING_BLOCK_HS200)
				|| (opcode == MMC_READ_SINGLE_BLOCK)
				|| (opcode == MMC_SEND_EXT_CSD)) {
					fifo_have = msdc_rxfifocnt();
					MSDC_SET_FIELD(MSDC_DBG_SEL,
						0xffff << 0, 0x0b);
					MSDC_GET_FIELD(MSDC_DBG_OUT,
						0x7ff << 0, fifo_1k_cnt);

					if ((fifo_1k_cnt >= MSDC_FIFO_THD_1K)
					&& (fifo_have > MSDC_FIFO_SZ - 64))
						rx_read(host, value);
				}
			}
		} else if ((tune_type_value == TUNE_SDIO_PLUS)
		&& (opcode == SD_IO_RW_EXTENDED)) {
			MSDC_WRITE32(MSDC_TXDATA, 0x5a5a5a5a);

			write_tmo = AUTOK_DAT_TIMEOUT;
			wait_cond_tmo(
				!(MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY),
				write_tmo);
			if (write_tmo == 0) {
				AUTOK_RAWPRINT("[AUTOK]MSDC busy tmo2 cmd%d\n",
				    opcode);
				ret |= E_RES_FATAL_ERR;
				goto end;
			}
		}
	}
	if ((tmo == 0) && (MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY)) {
		AUTOK_RAWPRINT("[AUTOK]MSDC busy tmo3 cmd%d\n", opcode);
		ret |= E_RES_FATAL_ERR;
		goto end;
	}

	sts = MSDC_READ32(MSDC_INT);
	wints = MSDC_INT_XFER_COMPL | MSDC_INT_DATCRCERR | MSDC_INT_DATTMO;
	if (sts) {
		/* clear status */
		MSDC_WRITE32(MSDC_INT, (sts & wints));
		if (sts & MSDC_INT_XFER_COMPL)
			ret |= E_RES_PASS;
		if (MSDC_INT_DATCRCERR & sts)
			ret |= E_RES_DAT_CRC;
		if (MSDC_INT_DATTMO & sts)
			ret |= E_RES_DAT_TMO;
	}

skip_tune_latch_ck_and_tune_data:
	tmo = AUTOK_DAT_TIMEOUT;
	wait_cond_tmo(!(MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY), tmo);
	if (tmo == 0) {
		AUTOK_RAWPRINT("[AUTOK]MSDC busy tmo4 cmd%d\n", opcode);
		ret |= E_RES_FATAL_ERR;
		goto end;
	}
	if ((tune_type_value == TUNE_CMD) || (tune_type_value == TUNE_DATA))
		msdc_clear_fifo();

end:
	if ((opcode == MMC_STOP_TRANSMISSION)
		|| (opcode == MMC_SWITCH_CQ_EN)
		|| (opcode == MMC_SWITCH_CQ_DIS)
		|| (opcode == MMC_CMDQ_TASK_MGMT)) {
		tmo = AUTOK_DAT_TIMEOUT;
		wait_cond_tmo(
			((MSDC_READ32(MSDC_PS) & 0x10000) == 0x10000),
			tmo);
		if (tmo == 0) {
			AUTOK_RAWPRINT("[AUTOK]DTA0 busy tmo cmd%d\n", opcode);
			ret |= E_RES_FATAL_ERR;
		}
	}

	return ret;
}

static int autok_simple_score(char *res_str, unsigned int result)
{
	unsigned int bit = 0;
	unsigned int num = 0;
	unsigned int old = 0;

	if (result == 0) {
		/* maybe result is 0 */
		strncpy(res_str, "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO", 33);
		return 32;
	}
	if (result == 0xFFFFFFFF) {
		strncpy(res_str, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", 33);
		return 0;
	}

	/* calc continue zero number */
	while (bit < 32) {
		if (result & (1 << bit)) {
			res_str[bit] = 'X';
			bit++;
			if (old < num)
				old = num;
			num = 0;
			continue;
		}
		res_str[bit] = 'O';
		bit++;
		num++;
	}
	if (num > old)
		old = num;

	res_str[32] = '\0';
	return old;
}

static int autok_simple_score64(char *res_str64, u64 result64)
{
	unsigned int bit = 0;
	unsigned int num = 0;
	unsigned int old = 0;

	if (result64 == 0) {
		/* maybe result is 0 */
		strncpy(res_str64,
	"OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO",
			65);
		return 64;
	}
	if (result64 == 0xFFFFFFFFFFFFFFFF) {
		strncpy(res_str64,
	"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
			65);
		return 0;
	}

	/* calc continue zero number */
	while (bit < 64) {
		if (result64 & ((u64) (1LL << bit))) {
			res_str64[bit] = 'X';
			bit++;
			if (old < num)
				old = num;
			num = 0;
			continue;
		}
		res_str64[bit] = 'O';
		bit++;
		num++;
	}
	if (num > old)
		old = num;

	res_str64[64] = '\0';
	return old;
}

enum {
	RD_SCAN_NONE,
	RD_SCAN_PAD_BOUND_S,
	RD_SCAN_PAD_BOUND_E,
	RD_SCAN_PAD_MARGIN,
};

static int autok_check_scan_res64(u64 rawdat,
	struct AUTOK_SCAN_RES *scan_res, unsigned int bd_filter)
{
	unsigned int bit;
	struct BOUND_INFO *pBD = (struct BOUND_INFO *)scan_res->bd_info;
	unsigned int RawScanSta = RD_SCAN_NONE;

	for (bit = 0; bit < 64; bit++) {
		if (rawdat & (1LL << bit)) {
			switch (RawScanSta) {
			case RD_SCAN_NONE:
				RawScanSta = RD_SCAN_PAD_BOUND_S;
				pBD->Bound_Start = 0;
				pBD->Bound_width = 1;
				scan_res->bd_cnt += 1;
				break;
			case RD_SCAN_PAD_MARGIN:
				RawScanSta = RD_SCAN_PAD_BOUND_S;
				pBD->Bound_Start = bit;
				pBD->Bound_width = 1;
				scan_res->bd_cnt += 1;
				break;
			case RD_SCAN_PAD_BOUND_E:
				if ((bit - pBD->Bound_End) <= bd_filter) {
					AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
				"[AUTOK]WARN: Try to filter the hole\r\n");
					RawScanSta = RD_SCAN_PAD_BOUND_S;

					pBD->Bound_width +=
					    (bit - pBD->Bound_End);
					pBD->Bound_End = 0;

					/* update full bound info */
					if (pBD->is_fullbound) {
						pBD->is_fullbound = 0;
						scan_res->fbd_cnt -= 1;
					}
				} else {
					/* No filter Check and
					 * Get the next boundary info
					 */
					RawScanSta = RD_SCAN_PAD_BOUND_S;
					pBD++;
					pBD->Bound_Start = bit;
					pBD->Bound_width = 1;
					scan_res->bd_cnt += 1;
					if (scan_res->bd_cnt > BD_MAX_CNT)
						goto end;
				}
				break;
			case RD_SCAN_PAD_BOUND_S:
				pBD->Bound_width++;
				break;
			default:
				break;
			}
		} else {
			switch (RawScanSta) {
			case RD_SCAN_NONE:
				RawScanSta = RD_SCAN_PAD_MARGIN;
				break;
			case RD_SCAN_PAD_BOUND_S:
				RawScanSta = RD_SCAN_PAD_BOUND_E;
				pBD->Bound_End = bit - 1;
				/* update full bound info */
				if (pBD->Bound_Start > 0) {
					pBD->is_fullbound = 1;
					scan_res->fbd_cnt += 1;
				}
				break;
			case RD_SCAN_PAD_MARGIN:
			case RD_SCAN_PAD_BOUND_E:
			default:
				break;
			}
		}
	}
	if ((pBD->Bound_End == 0) && (pBD->Bound_width != 0))
		pBD->Bound_End = pBD->Bound_Start + pBD->Bound_width - 1;

end:
	if (scan_res->bd_cnt > BD_MAX_CNT)
		AUTOK_RAWPRINT("[AUTOK]WARN: %d bd Exist\r\n", BD_MAX_CNT);
	return 0;
}

static int autok_check_scan_res64_new(u64 rawdat,
	struct AUTOK_SCAN_RES_NEW *scan_res, unsigned int bd_filter)
{
	unsigned int bit;
	int i, j;
	/* bd fail and pass count */
	unsigned char fail = 0;
	unsigned char pass = 0;
	enum AUTOK_TX_SCAN_STA_E RawScanSta = START_POSITION;

	/* check scan window boundary */
	for (bit = 0; bit < 64; bit++) {
		if (rawdat & (1LL << bit)) {
			switch (RawScanSta) {
			case START_POSITION:
				RawScanSta = FAIL_POSITION;
				scan_res->fail_info[fail++].bd_s = bit;
				scan_res->fail_cnt++;
				break;
			case PASS_POSITION:
				RawScanSta = FAIL_POSITION;
				if (bit == 63) {
					scan_res->fail_info[fail++].bd_s = bit;
					scan_res->fail_info[fail - 1].bd_e =
						bit;
				} else
					scan_res->fail_info[fail++].bd_s = bit;
				scan_res->pass_info[pass - 1].bd_e = bit - 1;
				scan_res->fail_cnt++;
				break;
			case FAIL_POSITION:
				RawScanSta = FAIL_POSITION;
				if (bit == 63)
					scan_res->fail_info[fail - 1].bd_e =
						bit;
				break;
			default:
				break;
			}
		} else {
			switch (RawScanSta) {
			case START_POSITION:
				RawScanSta = PASS_POSITION;
				scan_res->pass_info[pass++].bd_s = bit;
				scan_res->pass_cnt++;
				break;
			case PASS_POSITION:
				RawScanSta = PASS_POSITION;
				if (bit == 63)
					scan_res->pass_info[pass - 1].bd_e =
						bit;
				break;
			case FAIL_POSITION:
				RawScanSta = PASS_POSITION;
				if (bit == 63) {
					scan_res->pass_info[pass++].bd_s = bit;
					scan_res->pass_info[pass - 1].bd_e =
						bit;
				} else
					scan_res->pass_info[pass++].bd_s = bit;
				scan_res->fail_info[fail - 1].bd_e = bit - 1;
				scan_res->pass_cnt++;
				break;
			default:
				break;
			}
		}
	}

	for (i = scan_res->fail_cnt; i >= 0; i--) {
		if (i > scan_res->fail_cnt)
			break;
		if ((i >= 1) && ((scan_res->fail_info[i].bd_s
			- scan_res->fail_info[i - 1].bd_e - 1) < bd_filter)) {
			scan_res->fail_info[i - 1].bd_e =
				scan_res->fail_info[i].bd_e;
			scan_res->fail_info[i].bd_s = 0;
			scan_res->fail_info[i].bd_e = 0;
			for (j = i; j < (scan_res->fail_cnt - 1); j++) {
				scan_res->fail_info[j].bd_s =
					scan_res->fail_info[j + 1].bd_s;
				scan_res->fail_info[j].bd_e =
					scan_res->fail_info[j + 1].bd_e;
			}
			/* add check to prevent coverity scan fail */
			if (scan_res->fail_cnt >= 1) {
				scan_res->fail_info[
					scan_res->fail_cnt - 1].bd_s = 0;
				scan_res->fail_info[
					scan_res->fail_cnt - 1].bd_e = 0;
			} else
				WARN_ON(1);
			scan_res->fail_cnt--;
		}
	}

	return 0;
}

static int autok_pad_dly_corner_check(struct AUTOK_REF_INFO *pInfo)
{
	/* scan result @ rising edge */
	struct AUTOK_SCAN_RES *pBdInfo_R = NULL;
	/* scan result @ falling edge */
	struct AUTOK_SCAN_RES *pBdInfo_F = NULL;
	struct AUTOK_SCAN_RES *p_Temp[2] = {NULL};
	unsigned int i, j, k, l;
	unsigned int pass_bd_size[BD_MAX_CNT + 1];
	unsigned int max_pass = 0;
	unsigned int max_size = 0;
	unsigned int bd_max_size = 0;
	unsigned int bd_overlap = 0;
	unsigned int corner_case_flag = 0;

	pBdInfo_R = &(pInfo->scan_info[0]);
	pBdInfo_F = &(pInfo->scan_info[1]);
	/*
	 * for corner case
	 * oooooooooooooooooo rising has no fail bound
	 * oooooooooooooooooo falling has no fail bound
	 */
	if ((pBdInfo_R->bd_cnt == 0) && (pBdInfo_F->bd_cnt == 0)) {
		AUTOK_RAWPRINT("[ATUOK]Warn:can't find bd both edge\r\n");
		pInfo->opt_dly_cnt = 31;
		pInfo->opt_edge_sel = 0;
		return -1;
	}
	/*
	 * for corner case
	 * xxxxxxxxxxxxxxxxxxxx rising only has one boundary,but all fail
	 * oooooooooxxooooooo falling has normal boundary
	 * or
	 * ooooooooooooxooooo rising has normal boundary
	 * xxxxxxxxxxxxxxxxxxxx falling only has one boundary,but all fail
	 */
	if ((pBdInfo_R->bd_cnt == 1) && (pBdInfo_F->bd_cnt == 1)
		&& (pBdInfo_R->bd_info[0].Bound_Start == 0)
		&& (pBdInfo_R->bd_info[0].Bound_End == 63)
		&& (pBdInfo_F->bd_info[0].Bound_Start == 0)
		&& (pBdInfo_F->bd_info[0].Bound_End == 63)) {
		AUTOK_RAWPRINT("[ATUOK]Err:can't find window both edge\r\n");
		return -2;
	}
	/*
	 * for shamoo case
	 * xxxooooooxxxxooooxxx rising has more than 3 boundary
	 * xxxooooooxxxxooooxxx failing has more than 3 boundary
	 */
	if ((pBdInfo_R->bd_cnt >= 3) && (pBdInfo_F->bd_cnt >= 3)) {
		AUTOK_RAWPRINT("[ATUOK]Err:data window shamoo\r\n");
		return -2;
	}
	/*
	 * for corner case
	 * xxxxxxxxxxxxxxxxxxxx rising only has one boundary,but all fail
	 * oooooooooxxooooooo falling has normal boundary
	 * or
	 * ooooooooooooxooooo rising has normal boundary
	 * xxxxxxxxxxxxxxxxxxxx falling only has one boundary,but all fail
	 */
	for (j = 0; j < 2; j++) {
		if (j == 0) {
			p_Temp[0] = pBdInfo_R;
			p_Temp[1] = pBdInfo_F;
		} else {
			p_Temp[0] = pBdInfo_F;
			p_Temp[1] = pBdInfo_R;
		}
		/* check boundary overlap */
		for (k = 0; k < p_Temp[0]->bd_cnt; k++)
			for (l = 0; l < p_Temp[1]->bd_cnt; l++)
				if (((p_Temp[0]->bd_info[k].Bound_Start
				    >= p_Temp[1]->bd_info[l].Bound_Start)
				    && (p_Temp[0]->bd_info[k].Bound_Start
				    <= p_Temp[1]->bd_info[l].Bound_End))
				    || ((p_Temp[0]->bd_info[k].Bound_End
				    <= p_Temp[1]->bd_info[l].Bound_End)
				    && (p_Temp[0]->bd_info[k].Bound_End
				    >= p_Temp[1]->bd_info[l].Bound_Start))
				    || ((p_Temp[1]->bd_info[l].Bound_Start
				    >= p_Temp[0]->bd_info[k].Bound_Start)
				    && (p_Temp[1]->bd_info[l].Bound_Start
				    <= p_Temp[0]->bd_info[k].Bound_End)))
					bd_overlap = 1;
		/*check max boundary size */
		for (k = 0; k < p_Temp[0]->bd_cnt; k++) {
			if ((p_Temp[0]->bd_info[k].Bound_End
				- p_Temp[0]->bd_info[k].Bound_Start)
				>= 20)
				bd_max_size = 1;
		}
		if (((bd_overlap == 1)
			&& (bd_max_size == 1))
			|| ((p_Temp[1]->bd_cnt == 0)
			&& (bd_max_size == 1))) {
			corner_case_flag = 1;
		}
		if (((p_Temp[0]->bd_cnt == 1)
			&& (p_Temp[0]->bd_info[0].Bound_Start == 0)
			&& (p_Temp[0]->bd_info[0].Bound_End == 63))
			|| (corner_case_flag == 1)) {
			if (j == 0)
				pInfo->opt_edge_sel = 1;
			else
				pInfo->opt_edge_sel = 0;
			/* 1T calc fail,need check max pass bd,select mid */
			switch (p_Temp[1]->bd_cnt) {
			case 4:
				pass_bd_size[0] =
				    p_Temp[1]->bd_info[0].Bound_Start - 0;
				pass_bd_size[1] =
				    p_Temp[1]->bd_info[1].Bound_Start
					- p_Temp[1]->bd_info[0].Bound_End;
				pass_bd_size[2] =
				    p_Temp[1]->bd_info[2].Bound_Start
					- p_Temp[1]->bd_info[1].Bound_End;
				pass_bd_size[3] =
				    p_Temp[1]->bd_info[3].Bound_Start
					- p_Temp[1]->bd_info[2].Bound_End;
				pass_bd_size[4] =
				    63 - p_Temp[1]->bd_info[3].Bound_End;
				max_size = pass_bd_size[0];
				max_pass = 0;
				for (i = 0; i < 5; i++) {
					if (pass_bd_size[i] >= max_size) {
						max_size = pass_bd_size[i];
						max_pass = i;
					}
				}
				if (max_pass == 0)
					pInfo->opt_dly_cnt =
					p_Temp[1]->bd_info[0].Bound_Start
					/ 2;
				else if (max_pass == 4)
					pInfo->opt_dly_cnt =
					(63 +
					p_Temp[1]->bd_info[3].Bound_End)
					/ 2;
				else {
					pInfo->opt_dly_cnt =
				    (p_Temp[1]->bd_info[max_pass].Bound_Start
				    +
				    p_Temp[1]->bd_info[max_pass - 1].Bound_End)
				    / 2;
				}
				break;
			case 3:
				pass_bd_size[0] =
				    p_Temp[1]->bd_info[0].Bound_Start - 0;
				pass_bd_size[1] =
				    p_Temp[1]->bd_info[1].Bound_Start
					- p_Temp[1]->bd_info[0].Bound_End;
				pass_bd_size[2] =
				    p_Temp[1]->bd_info[2].Bound_Start
					- p_Temp[1]->bd_info[1].Bound_End;
				pass_bd_size[3] =
				    63 - p_Temp[1]->bd_info[2].Bound_End;
				max_size = pass_bd_size[0];
				max_pass = 0;
				for (i = 0; i < 4; i++) {
					if (pass_bd_size[i] >= max_size) {
						max_size = pass_bd_size[i];
						max_pass = i;
					}
				}
				if (max_pass == 0)
					pInfo->opt_dly_cnt =
				    p_Temp[1]->bd_info[0].Bound_Start / 2;
				else if (max_pass == 3)
					pInfo->opt_dly_cnt =
				    (63 + p_Temp[1]->bd_info[2].Bound_End) / 2;
				else {
					pInfo->opt_dly_cnt =
				    (p_Temp[1]->bd_info[max_pass].Bound_Start
				    +
				    p_Temp[1]->bd_info[max_pass - 1].Bound_End)
				    / 2;
				}
				break;
			case 2:
				pass_bd_size[0] =
				    p_Temp[1]->bd_info[0].Bound_Start - 0;
				pass_bd_size[1] =
				    p_Temp[1]->bd_info[1].Bound_Start
					- p_Temp[1]->bd_info[0].Bound_End;
				pass_bd_size[2] =
				    63 - p_Temp[1]->bd_info[1].Bound_End;
				max_size = pass_bd_size[0];
				max_pass = 0;
				for (i = 0; i < 3; i++) {
					if (pass_bd_size[i] >= max_size) {
						max_size = pass_bd_size[i];
						max_pass = i;
					}
				}
				if (max_pass == 0)
					pInfo->opt_dly_cnt =
					p_Temp[1]->bd_info[0].Bound_Start / 2;
				else if (max_pass == 2)
					pInfo->opt_dly_cnt =
				    (63 + p_Temp[1]->bd_info[1].Bound_End) / 2;
				else {
					pInfo->opt_dly_cnt =
				    (p_Temp[1]->bd_info[max_pass].Bound_Start
				    +
				    p_Temp[1]->bd_info[max_pass - 1].Bound_End)
				    / 2;
				}
				break;
			case 1:
				pass_bd_size[0] =
				    p_Temp[1]->bd_info[0].Bound_Start - 0;
				pass_bd_size[1] =
					63 -
					p_Temp[1]->bd_info[0].Bound_End;
				max_size = pass_bd_size[0];
				max_pass = 0;
				for (i = 0; i < 2; i++) {
					if (pass_bd_size[i] >= max_size) {
						max_size = pass_bd_size[i];
						max_pass = i;
					}
				}
				if (max_pass == 0)
					pInfo->opt_dly_cnt =
					p_Temp[1]->bd_info[0].Bound_Start
					/ 2;
				else if (max_pass == 1)
					pInfo->opt_dly_cnt =
				    (63 +
				    p_Temp[1]->bd_info[0].Bound_End)
				    / 2;
				break;
			case 0:
				pInfo->opt_dly_cnt = 31;
				break;
			default:
				break;
			}
			return -1;
		}
	}
	return 0;
}

static int autok_pad_dly_sel(struct AUTOK_REF_INFO *pInfo)
{
	/* scan result @ rising edge */
	struct AUTOK_SCAN_RES *pBdInfo_R = NULL;
	/* scan result @ falling edge */
	struct AUTOK_SCAN_RES *pBdInfo_F = NULL;
	/* Save the first boundary info for calc optimised dly count */
	struct BOUND_INFO *pBdPrev = NULL;
	/* Save the second boundary info for calc optimised dly count */
	struct BOUND_INFO *pBdNext = NULL;
	struct BOUND_INFO *pBdTmp = NULL;
	/* Full Boundary count */
	unsigned int FBound_Cnt_R = 0;
	unsigned int Bound_Cnt_R = 0;
	unsigned int Bound_Cnt_F = 0;
	unsigned int cycle_cnt = 64;
	int uBD_mid_prev = 0;
	int uBD_mid_next = 0;
	int uBD_width = 3;
	int uDlySel_F = 0;
	int uDlySel_R = 0;
	/* for falling edge margin compress */
	int uMgLost_F = 0;
	/* for rising edge margin compress */
	int uMgLost_R = 0;
	unsigned int ret = 0;
	unsigned int i;
	int corner_res = 0;

	pBdInfo_R = &(pInfo->scan_info[0]);
	pBdInfo_F = &(pInfo->scan_info[1]);
	FBound_Cnt_R = pBdInfo_R->fbd_cnt;
	Bound_Cnt_R = pBdInfo_R->bd_cnt;
	Bound_Cnt_F = pBdInfo_F->bd_cnt;

	corner_res = autok_pad_dly_corner_check(pInfo);
	if (corner_res == -1)
		return 0;
	else if (corner_res == -2)
		return -2;

	switch (FBound_Cnt_R) {
	case 4:	/* SSSS Corner may cover 2~3T */
	case 3:
		AUTOK_RAWPRINT("[ATUOK]Warn:Many Full bd cnt:%d\r\n",
			FBound_Cnt_R);
	case 2:	/* mode_1 : 2 full boudary */
		for (i = 0; i < BD_MAX_CNT; i++) {
			if (pBdInfo_R->bd_info[i].is_fullbound) {
				if (pBdPrev == NULL) {
					pBdPrev = &(pBdInfo_R->bd_info[i]);
				} else {
					pBdNext = &(pBdInfo_R->bd_info[i]);
					break;
				}
			}
		}

		if (pBdPrev && pBdNext) {
			uBD_mid_prev =
				(pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
			uBD_mid_next =
				(pBdNext->Bound_Start + pBdNext->Bound_End) / 2;
			/* while in 2 full bound case, bd_width calc */
			uBD_width =
			    (pBdPrev->Bound_width +
			    pBdNext->Bound_width) / 2;
			cycle_cnt = uBD_mid_next - uBD_mid_prev;
			/* delay count sel at rising edge */
			if (uBD_mid_prev >= cycle_cnt / 2) {
				uDlySel_R = uBD_mid_prev - cycle_cnt / 2;
				uMgLost_R = 0;
			} else if ((cycle_cnt / 2 -
			    uBD_mid_prev) >
			    AUTOK_MARGIN_THOLD) {
				uDlySel_R = uBD_mid_prev + cycle_cnt / 2;
				uMgLost_R = 0;
			} else {
				uDlySel_R = 0;
				uMgLost_R = cycle_cnt / 2 - uBD_mid_prev;
			}
			/* delay count sel at falling edge */
			pBdTmp = &(pBdInfo_R->bd_info[0]);
			if (pBdTmp->is_fullbound) {
				/* ooooxxxooooooxxxooo */
				uDlySel_F = uBD_mid_prev;
				uMgLost_F = 0;
			} else {
				/* xooooooxxxoooooooxxxoo */
				if (pBdTmp->Bound_End > uBD_width / 2) {
					uDlySel_F =
					    (pBdTmp->Bound_End) -
					    (uBD_width / 2);
					uMgLost_F = 0;
				} else {
					uDlySel_F = 0;
					uMgLost_F =
					    (uBD_width / 2) -
					    (pBdTmp->Bound_End);
				}
			}
		} else {
			/* error can not find 2 foull boary */
			AUTOK_RAWPRINT("[AUTOK]can not find 2 full bd @Mode1");
			return -1;
		}
		break;

	case 1:	/* rising edge find one full boundary */
		if (Bound_Cnt_R > 1) {
			/* mode_2: 1 full boundary and boundary count > 1 */
			pBdPrev = &(pBdInfo_R->bd_info[0]);
			pBdNext = &(pBdInfo_R->bd_info[1]);

			if (pBdPrev->is_fullbound)
				uBD_width = pBdPrev->Bound_width;
			else
				uBD_width = pBdNext->Bound_width;

			if ((pBdPrev->is_fullbound)
				|| (pBdNext->is_fullbound)) {
				if (pBdPrev->Bound_Start > 0)
					cycle_cnt =
					    pBdNext->Bound_Start -
					    pBdPrev->Bound_Start;
				else
					cycle_cnt =
					    pBdNext->Bound_End -
					    pBdPrev->Bound_End;

				/* delay count sel@rising & falling edge */
				if (pBdPrev->is_fullbound) {
					uBD_mid_prev =
					    (pBdPrev->Bound_Start +
					    pBdPrev->Bound_End) / 2;
					uDlySel_F = uBD_mid_prev;
					uMgLost_F = 0;
					if (uBD_mid_prev >= cycle_cnt / 2) {
						uDlySel_R =
						    uBD_mid_prev -
						    cycle_cnt / 2;
						uMgLost_R = 0;
					} else if ((cycle_cnt / 2 -
						    uBD_mid_prev) >
						   AUTOK_MARGIN_THOLD) {
						uDlySel_R =
						    uBD_mid_prev +
						    cycle_cnt / 2;
						uMgLost_R = 0;
					} else {
						uDlySel_R = 0;
						uMgLost_R =
						    cycle_cnt / 2 -
						    uBD_mid_prev;
					}
				} else {
					/* first boundary not full boudary */
					uBD_mid_next =
					    (pBdNext->Bound_Start +
					    pBdNext->Bound_End) / 2;
					uDlySel_R =
					    uBD_mid_next - cycle_cnt / 2;
					uMgLost_R = 0;
					if (pBdPrev->Bound_End >
						uBD_width / 2) {
						uDlySel_F =
						    (pBdPrev->Bound_End) -
						    (uBD_width / 2);
						uMgLost_F = 0;
					} else {
						uDlySel_F = 0;
						uMgLost_F =
						    (uBD_width / 2) -
						    (pBdPrev->Bound_End);
					}
				}
			} else {
				/* full bound must in first 2 boundary */
				return -1;
			}
		} else if (Bound_Cnt_F > 0) {
			/* mode_3: 1 full boundary and
			 * only one boundary exist @rising edge
			 * this boundary is full bound
			 */
			pBdPrev = &(pBdInfo_R->bd_info[0]);
			pBdNext = &(pBdInfo_F->bd_info[0]);
			uBD_mid_prev =
			    (pBdPrev->Bound_Start +
			    pBdPrev->Bound_End) / 2;
			uBD_width = pBdPrev->Bound_width;

			if (pBdNext->Bound_Start == 0) {
				cycle_cnt =
				    (pBdPrev->Bound_End -
				    pBdNext->Bound_End) * 2;
			} else if (pBdNext->Bound_End == 63) {
				cycle_cnt =
				    (pBdNext->Bound_Start -
				    pBdPrev->Bound_Start) * 2;
			} else {
				uBD_mid_next =
				    (pBdNext->Bound_Start +
				    pBdNext->Bound_End) / 2;

				if (uBD_mid_next > uBD_mid_prev)
					cycle_cnt =
					    (uBD_mid_next -
					    uBD_mid_prev) * 2;
				else
					cycle_cnt =
					    (uBD_mid_prev -
					    uBD_mid_next) * 2;
			}

			uDlySel_F = uBD_mid_prev;
			uMgLost_F = 0;

			if (uBD_mid_prev >= cycle_cnt / 2) {
				/* case 1 */
				uDlySel_R = uBD_mid_prev - cycle_cnt / 2;
				uMgLost_R = 0;
			} else if (cycle_cnt / 2 -
			    uBD_mid_prev <=
			    AUTOK_MARGIN_THOLD) {
				/* case 2 */
				uDlySel_R = 0;
				uMgLost_R =
				    cycle_cnt / 2 -
				    uBD_mid_prev;
			} else if (cycle_cnt / 2
			    + uBD_mid_prev <= 63) {
				/* case 3 */
				uDlySel_R = cycle_cnt / 2 + uBD_mid_prev;
				uMgLost_R = 0;
			} else if (32 -
			    uBD_mid_prev <=
			    AUTOK_MARGIN_THOLD) {
				/* case 4 */
				uDlySel_R = 0;
				uMgLost_R =
				    cycle_cnt / 2 -
				    uBD_mid_prev;
			} else {
				/* case 5 */
				uDlySel_R = 63;
				uMgLost_R =
				    uBD_mid_prev +
				    cycle_cnt / 2 - 63;
			}
		} else {
			/* mode_4: falling edge no boundary found
			 * & rising edge only one full boundary exist
			 * this boundary is full bound
			 */
			pBdPrev = &(pBdInfo_R->bd_info[0]);
			uBD_mid_prev =
			    (pBdPrev->Bound_Start +
			    pBdPrev->Bound_End) / 2;
			uBD_width = pBdPrev->Bound_width;

			if (pBdPrev->Bound_End >
			    (64 - pBdPrev->Bound_Start))
				cycle_cnt = 2 *
				    (pBdPrev->Bound_End + 1);
			else
				cycle_cnt = 2 *
				    (64 - pBdPrev->Bound_Start);

			uDlySel_R = 0xFF;
			/* Margin enough donot care margin lost */
			uMgLost_R = 0xFF;
			uDlySel_F = uBD_mid_prev;
			/* Margin enough donot care margin lost */
			uMgLost_F = 0xFF;

			AUTOK_RAWPRINT("[AUTOK]Warn: 1T > %d\r\n",
				cycle_cnt);
		}
		break;

	case 0:	/* rising edge cannot find full boudary */
		if (Bound_Cnt_R == 2) {
			pBdPrev = &(pBdInfo_R->bd_info[0]);
			/* this boundary is full bound */
			pBdNext = &(pBdInfo_F->bd_info[0]);

			if (pBdNext->is_fullbound) {
				/* mode_5: rising_edge 2 boundary
				 * (not full bound),
				 * falling edge 1 full boundary
				 */
				uBD_width = pBdNext->Bound_width;
				cycle_cnt = 2 *
				    (pBdNext->Bound_End -
				    pBdPrev->Bound_End);
				uBD_mid_next =
				    (pBdNext->Bound_Start +
				    pBdNext->Bound_End) / 2;
				uDlySel_R = uBD_mid_next;
				uMgLost_R = 0;
				if (pBdPrev->Bound_End >= uBD_width / 2) {
					uDlySel_F =
					    pBdPrev->Bound_End - uBD_width / 2;
					uMgLost_F = 0;
				} else {
					uDlySel_F = 0;
					uMgLost_F =
					    uBD_width / 2 -
					    pBdPrev->Bound_End;
				}
			} else {
				/* for falling edge there must be
				 * one full boundary between two
				 * bounary_mid at rising
				 * this is a corner case,
				 * falling boundary may  scan miss.
				 * xoooooooooooooooox
				 * oooooooooooooooooo
				 * or
				 * xoooooooooooooooox
				 * xxoooooooooooooooo
				 * or
				 * xoooooooooooooooox
				 * ooooooooooooooooox
				 */
				pInfo->cycle_cnt =
				    pBdInfo_R->bd_info[1].Bound_End
				- pBdInfo_R->bd_info[0].Bound_Start;
				if (Bound_Cnt_F == 0) {
					pInfo->opt_edge_sel = 1;
					pInfo->opt_dly_cnt = 0;
				} else {
					pInfo->opt_edge_sel = 0;
					pInfo->opt_dly_cnt =
					    (pBdInfo_R->bd_info[1].Bound_End
					    + pBdInfo_R->bd_info[0].Bound_Start)
					    / 2;
				}
				return ret;
			}
		} else if (Bound_Cnt_R == 1) {
			if (Bound_Cnt_F > 1) {
				/* when riss edge have only one bd (not full bd)
				 * falling edge shouldn't more than 1Bound exist
				 * this is a corner case, rise bd may  scan miss
				 * xooooooooooooooooo
				 * oooxooooooooxooooo
				 */
				pInfo->cycle_cnt =
			    (pBdInfo_F->bd_info[1].Bound_End
				+ pBdInfo_F->bd_info[1].Bound_Start) / 2
				- (pBdInfo_F->bd_info[0].Bound_End
				+ pBdInfo_F->bd_info[0].Bound_Start) / 2;
				pInfo->opt_edge_sel = 1;
				pInfo->opt_dly_cnt =
			    ((pBdInfo_F->bd_info[1].Bound_End
				+ pBdInfo_F->bd_info[1].Bound_Start) / 2
				+ (pBdInfo_F->bd_info[0].Bound_End
				+ pBdInfo_F->bd_info[0].Bound_Start) / 2) / 2;
				return ret;
			} else if (Bound_Cnt_F == 1) {
				/* mode_6: rise edge only 1 bd (not full bd)
				 * & fall edge have only 1 bound too
				 */
				pBdPrev = &(pBdInfo_R->bd_info[0]);
				pBdNext = &(pBdInfo_F->bd_info[0]);
				if (pBdNext->is_fullbound) {
					uBD_width =
					    pBdNext->Bound_width;
				} else {
					if (pBdNext->Bound_width >
						pBdPrev->Bound_width)
						uBD_width =
						    (pBdNext->Bound_width + 1);
					else
						uBD_width =
						    (pBdPrev->Bound_width + 1);

					if (uBD_width < AUTOK_BD_WIDTH_REF)
						uBD_width = AUTOK_BD_WIDTH_REF;
				} /* Boundary width calc done */

				if (pBdPrev->Bound_Start == 0) {
					/* Current Desing Not Allowed */
					if (pBdNext->Bound_Start == 0) {
						/* Current Desing Not Allowed
						 * this is a corner case,
						 * boundary may  scan error.
						 * xooooooooooooooooo
						 * xooooooooooooooooo
						 */
						pInfo->cycle_cnt = 2 *
					    (64 -
					    (pBdInfo_R->bd_info[0].Bound_End +
					    pBdInfo_R->bd_info[0].Bound_Start)
					    / 2);
						pInfo->opt_edge_sel = 0;
						pInfo->opt_dly_cnt = 31;
						return ret;
					}

					cycle_cnt =
					    (pBdNext->Bound_Start -
					    pBdPrev->Bound_End +
					     uBD_width) * 2;
				} else if (pBdPrev->Bound_End == 63) {
					/* Current Desing Not Allowed */
					if (pBdNext->Bound_End == 63) {
						/* Current Desing Not Allowed
						 * this is a corner case,
						 * boundary may  scan error.
						 * ooooooooooooooooox
						 * ooooooooooooooooox
						 */
						pInfo->cycle_cnt =
					    pBdInfo_R->bd_info[0].Bound_End +
					    pBdInfo_R->bd_info[0].Bound_Start;
						pInfo->opt_edge_sel = 0;
						pInfo->opt_dly_cnt = 31;
						return ret;
					}

					cycle_cnt =
					    (pBdPrev->Bound_Start -
					    pBdNext->Bound_End +
					     uBD_width) * 2;
				} /* cycle count calc done */

				/* calc optimise delay count */
				if (pBdPrev->Bound_Start == 0) {
					/* falling edge sel */
					if (pBdPrev->Bound_End >=
						uBD_width / 2) {
						uDlySel_F =
							pBdPrev->Bound_End -
							uBD_width / 2;
						uMgLost_F = 0;
					} else {
						uDlySel_F = 0;
						uMgLost_F =
							uBD_width / 2 -
							pBdPrev->Bound_End;
					}

					/* rising edge sel */
					if (pBdPrev->Bound_End -
						uBD_width / 2 +
						cycle_cnt / 2 > 63) {
						uDlySel_R = 63;
						uMgLost_R =
						    pBdPrev->Bound_End -
						    uBD_width / 2 +
						    cycle_cnt / 2 - 63;
					} else {
						uDlySel_R =
						    pBdPrev->Bound_End -
						    uBD_width / 2 +
						    cycle_cnt / 2;
						uMgLost_R = 0;
					}
				} else if (pBdPrev->Bound_End == 63) {
					/* falling edge sel */
					if (pBdPrev->Bound_Start +
						uBD_width / 2 < 63) {
						uDlySel_F =
						    pBdPrev->Bound_Start +
						    uBD_width / 2;
						uMgLost_F = 0;
					} else {
						uDlySel_F = 63;
						uMgLost_F =
						    pBdPrev->Bound_Start +
						    uBD_width / 2 - 63;
					}

					/* rising edge sel */
					if (pBdPrev->Bound_Start +
						uBD_width / 2 -
						cycle_cnt / 2 < 0) {
						uDlySel_R = 0;
						uMgLost_R =
						    cycle_cnt / 2 -
						    (pBdPrev->Bound_Start +
							uBD_width / 2);
					} else {
						uDlySel_R =
						    pBdPrev->Bound_Start +
						    uBD_width / 2 -
						    cycle_cnt / 2;
						uMgLost_R = 0;
					}
				} else {
					return -1;
				}
			} else if (Bound_Cnt_F == 0) {
				/* mode_7: rising edge only one
				 * bound (not full), falling no boundary
				 */
				cycle_cnt = 128;
				pBdPrev = &(pBdInfo_R->bd_info[0]);
				if (pBdPrev->Bound_Start == 0) {
					uDlySel_F = 0;
					uDlySel_R = 63;
				} else if (pBdPrev->Bound_End == 63) {
					uDlySel_F = 63;
					uDlySel_R = 0xFF;
				} else {
					return -1;
				}
				uMgLost_F = 0xFF;
				uMgLost_R = 0xFF;

				AUTOK_RAWPRINT("[AUTOK]Warn: 1T > %d\r\n",
				    cycle_cnt);
			}
		} else if (Bound_Cnt_R == 0) {
			/* Rising Edge No Boundary found */
			if (Bound_Cnt_F > 1) {
				/* falling edge not allowed two
				 * boundary Exist for this case
				 * this is a corner case,
				 *rising boundary may  scan miss.
				 * oooooooooooooooooo
				 * oooxooooooooxooooo
				 */
				pInfo->cycle_cnt =
				    (pBdInfo_F->bd_info[1].Bound_End
				+ pBdInfo_F->bd_info[1].Bound_Start) / 2
				- (pBdInfo_F->bd_info[0].Bound_End
				+ pBdInfo_F->bd_info[0].Bound_Start) / 2;
				pInfo->opt_edge_sel = 0;
				pInfo->opt_dly_cnt =
				    (pBdInfo_F->bd_info[0].Bound_End
				+ pBdInfo_F->bd_info[0].Bound_Start) / 2;
				return ret;
			} else if (Bound_Cnt_F > 0) {
				/* mode_8: falling edge
				 * have one Boundary exist
				 */
				pBdPrev = &(pBdInfo_F->bd_info[0]);

				/* this boundary is full bound */
				if (pBdPrev->is_fullbound) {
					uBD_mid_prev =
					    (pBdPrev->Bound_Start +
					    pBdPrev->Bound_End) / 2;

					if (pBdPrev->Bound_End >
						    (64 - pBdPrev->Bound_Start))
						cycle_cnt =
						    2 *
						    (pBdPrev->Bound_End + 1);
					else
						cycle_cnt =
						    2 *
						    (64 - pBdPrev->Bound_Start);

					uDlySel_R = uBD_mid_prev;
					uMgLost_R = 0xFF;
					uDlySel_F = 0xFF;
					uMgLost_F = 0xFF;
				} else {
					cycle_cnt = 128;

					uDlySel_R =
					    (pBdPrev->Bound_Start ==
					    0) ? 0 : 63;
					uMgLost_R = 0xFF;
					uDlySel_F = 0xFF;
					uMgLost_F = 0xFF;
				}

				AUTOK_RAWPRINT("[AUTOK]Warning: 1T > %d\r\n",
							cycle_cnt);
			} else {
				/* falling edge no boundary
				 * exist no need tuning
				 */
				cycle_cnt = 128;
				uDlySel_F = 0;
				uMgLost_F = 0xFF;
				uDlySel_R = 0;
				uMgLost_R = 0xFF;
				AUTOK_RAWPRINT("[AUTOK]Warn: 1T > %d\r\n",
						    cycle_cnt);
			}
		} else {
			/* Error if bound_cnt > 3 there must
			 *be at least one full boundary exist
			 */
			return -1;
		}
		break;

	default:
		/* warn if bd count>4 (current hw design,
		 *this case cannot happen)
		 */
		return -1;
	}

	/* Select Optimised Sample edge & delay count (the small one) */
	pInfo->cycle_cnt = cycle_cnt;
	if (uDlySel_R <= uDlySel_F) {
		pInfo->opt_edge_sel = 0;
		pInfo->opt_dly_cnt = uDlySel_R;
	} else {
		pInfo->opt_edge_sel = 1;
		pInfo->opt_dly_cnt = uDlySel_F;

	}
	AUTOK_RAWPRINT("[AUTOK]Analysis Result: 1T = %d\r\n", cycle_cnt);
	return ret;
}

#if SINGLE_EDGE_ONLINE_TUNE
static int
autok_pad_dly_sel_single_edge(struct AUTOK_SCAN_RES *pInfo,
	unsigned int cycle_cnt_ref, unsigned int *pDlySel)
{
	/* Save the first boundary info for calc optimised dly count */
	struct BOUND_INFO *pBdPrev = NULL;
	/* Save the second boundary
	 * info for calc optimised dly count
	 */
	struct BOUND_INFO *pBdNext = NULL;
	unsigned int Bound_Cnt = 0;
	unsigned int uBD_mid_prev = 0;
	int uDlySel = 0;
	int uMgLost = 0;
	unsigned int ret = 0;

	Bound_Cnt = pInfo->bd_cnt;
	if (Bound_Cnt > 1) {
		pBdPrev = &(pInfo->bd_info[0]);
		pBdNext = &(pInfo->bd_info[1]);
		if (!(pBdPrev->is_fullbound)) {
			/* mode_1: at least 2 Bound and Boud0_Start == 0 */
			uDlySel = (pBdPrev->Bound_End +
					    pBdNext->Bound_Start) / 2;
			uMgLost = (uDlySel > 31) ? (uDlySel - 31) : 0;
			uDlySel = (uDlySel > 31) ? 31 : uDlySel;

		} else {
			/* mode_2: at least 2 Bound
			 * found and Bound0_Start != 0
			 */
			uBD_mid_prev = (pBdPrev->Bound_Start +
						    pBdPrev->Bound_End) / 2;
			if (uBD_mid_prev >= cycle_cnt_ref / 2) {
				uDlySel = uBD_mid_prev - cycle_cnt_ref / 2;
				uMgLost = 0;
			} else if (cycle_cnt_ref / 2 - uBD_mid_prev <
					    AUTOK_MARGIN_THOLD) {
				uDlySel = 0;
				uMgLost = cycle_cnt_ref / 2 - uBD_mid_prev;
			} else {
				uDlySel = (pBdPrev->Bound_End +
						    pBdNext->Bound_Start) / 2;
				if ((uDlySel > 31)
					&& (uDlySel - 31 <
						AUTOK_MARGIN_THOLD)) {
					uDlySel = 31;
					uMgLost = uDlySel - 31;
				} else {
					/* uDlySel = uDlySel; */
					uMgLost = 0;
				}
			}
		}
	} else if (Bound_Cnt > 0) {
		/* only one bound fond */
		pBdPrev = &(pInfo->bd_info[0]);
		if (pBdPrev->is_fullbound) {
			/* mode_3: Bound_S != 0 */
			uBD_mid_prev = (pBdPrev->Bound_Start +
			    pBdPrev->Bound_End) / 2;
			if (uBD_mid_prev >= cycle_cnt_ref / 2) {
				uDlySel = uBD_mid_prev - cycle_cnt_ref / 2;
				uMgLost = 0;
			} else if (cycle_cnt_ref / 2 - uBD_mid_prev <
					    AUTOK_MARGIN_THOLD) {
				uDlySel = 0;
				uMgLost = cycle_cnt_ref / 2 - uBD_mid_prev;
			} else if ((uBD_mid_prev > 31 - AUTOK_MARGIN_THOLD)
				   || (pBdPrev->Bound_Start >= 16)) {
				uDlySel = 0;
				uMgLost = cycle_cnt_ref / 2 - uBD_mid_prev;
			} else if (uBD_mid_prev + cycle_cnt_ref / 2 <= 63) {
				/* Left Margin not enough must
				 * need to select the right side
				 */
				uDlySel = uBD_mid_prev + cycle_cnt_ref / 2;
				uMgLost = 0;
			} else {
				uDlySel = 63;
				uMgLost = uBD_mid_prev + cycle_cnt_ref / 2 - 63;
			}
		} else if (pBdPrev->Bound_Start == 0) {
			/* mode_4 : Only one Boud and
			 * Boud_S = 0  (Currently 1T nearly equal 64 )
			 */

			/* May not exactly by for
			 * Cycle_Cnt enough can don't care
			 */
			uBD_mid_prev = (pBdPrev->Bound_Start +
			    pBdPrev->Bound_End) / 2;
			if (pBdPrev->Bound_Start + cycle_cnt_ref / 2 >= 31) {
				uDlySel = 31;
				uMgLost = uBD_mid_prev + cycle_cnt_ref / 2 - 31;
			} else {
				uDlySel = uBD_mid_prev + cycle_cnt_ref / 2;
				uMgLost = 0;
			}
		} else {
			/* mode_5: Only one Boud and Boud_E = 64
			 * May not exactly by for
			 * Cycle_Cnt enough can don't care
			 */
			uBD_mid_prev = (pBdPrev->Bound_Start +
			    pBdPrev->Bound_End) / 2;
			if (pBdPrev->Bound_Start < cycle_cnt_ref / 2) {
				uDlySel = 0;
				uMgLost = cycle_cnt_ref / 2 - uBD_mid_prev;
			} else if (uBD_mid_prev - cycle_cnt_ref / 2 > 31) {
				uDlySel = 31;
				uMgLost = uBD_mid_prev - cycle_cnt_ref / 2 - 31;
			} else {
				uDlySel = uBD_mid_prev - cycle_cnt_ref / 2;
				uMgLost = 0;
			}
		}
	} else {
		/*mode_6: no bound foud */
		uDlySel = 31;
		uMgLost = 0xFF;
	}
	*pDlySel = uDlySel;
	if (uDlySel > 31) {
		AUTOK_RAWPRINT
		    ("[AUTOK]Warn Dly %d>31 easily effected by Voltage\r\n",
		     uDlySel);
	}

	return ret;
}
#endif

static int autok_ds_dly_sel(struct AUTOK_SCAN_RES_NEW *pInfo,
	unsigned int *pDlySel)
{
	int uDlySel = 0;
	unsigned int max_pass_win;
	unsigned char max_pass_win_position;
	unsigned char i;

	max_pass_win_position = 0;
	max_pass_win = pInfo->pass_info[0].bd_e
		- pInfo->pass_info[0].bd_s;

	for (i = 0; i < pInfo->pass_cnt; i++)
		if ((pInfo->pass_info[i].bd_e
			- pInfo->pass_info[i].bd_s) > max_pass_win) {
			max_pass_win = pInfo->pass_info[i].bd_e
				- pInfo->pass_info[i].bd_s;
			max_pass_win_position = i;
		}
	uDlySel =
		(pInfo->pass_info[max_pass_win_position].bd_s
		+ pInfo->pass_info[max_pass_win_position].bd_e) / 2;
	*pDlySel = uDlySel;

	return max_pass_win;
}

/*************************************************************************
 * FUNCTION
 *  autok_adjust_param
 *
 * DESCRIPTION
 *  This function for auto-K, adjust msdc parameter
 *
 * PARAMETERS
 *    host: msdc host manipulator pointer
 *    param: enum of msdc parameter
 *    value: value of msdc parameter
 *    rw: AUTOK_READ/AUTOK_WRITE
 *
 * RETURN VALUES
 *    error code: 0 success,
 *               -1 parameter input error
 *               -2 read/write fail
 *               -3 else error
 *************************************************************************/
static int autok_adjust_param(struct msdc_host *host,
	enum AUTOK_PARAM param, u32 *value, int rw)
{
	void __iomem *base = host->base;
#if !defined(FPGA_PLATFORM)
	void __iomem *base_top = host->base_top;
#endif
	u32 *reg;
	u32 field = 0;
	struct AUTOK_PLAT_TOP_CTRL platform_top_ctrl;

	memset(&platform_top_ctrl, 0, sizeof(struct AUTOK_PLAT_TOP_CTRL));
	get_platform_top_ctrl(platform_top_ctrl);

	switch (param) {
	case READ_DATA_SMPL_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) READ_DATA_SMPL_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}

		reg = (u32 *) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_R_D_SMPL_SEL);
		break;
	case WRITE_DATA_SMPL_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) WRITE_DATA_SMPL_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}

		reg = (u32 *) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_W_D_SMPL_SEL);
		break;
	case DATA_DLYLINE_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) DATA_DLYLINE_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CONTROL;
			field = (u32) (DATA_K_VALUE_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_IOCON;
			field = (u32) (MSDC_IOCON_DDLSEL);
		}
		break;
	case MSDC_DAT_TUNE_SEL:	/* 0-Dat tune 1-CLk tune ; */
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) DAT_TUNE_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CONTROL;
			field = (u32) (PAD_RXDLY_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE0;
			field = (u32) (MSDC_PAD_TUNE0_RXDLYSEL);
		}
		break;
	case MSDC_WCRC_ASYNC_FIFO_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) WCRC_ASYNC_FIFO_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT2;
		field = (u32) (MSDC_PB2_CFGCRCSTS);
		break;
	case MSDC_RESP_ASYNC_FIFO_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) RESP_ASYNC_FIFO_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT2;
		field = (u32) (MSDC_PB2_CFGRESP);
		break;
	case CMD_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) CMD_EDGE out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_RSPL);
		break;
	case CMD_FIFO_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) CMD_FIFO_EDGE out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_CFG0;
		field = (u32) (MSDC_EMMC50_CFG_CMD_EDGE_SEL);
		break;
	case RDATA_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) RDATA_EDGE out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_R_D_SMPL);
		break;
	case RD_FIFO_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) RD_FIFO_EDGE out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT0;
		field = (u32) (MSDC_PB0_RD_DAT_SEL);
		break;
	case WD_FIFO_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) WD_FIFO_EDGE out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT2;
		field = (u32) (MSDC_PB2_CFGCRCSTSEDGE);
		break;
	case CMD_RD_D_DLY1:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) CMD_RD_DLY out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CMD;
			field = (u32) (PAD_CMD_RXDLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE0;
			field = (u32) (MSDC_PAD_TUNE0_CMDRDLY);
		}
		break;
	case CMD_RD_D_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) CMD_RD_DLY_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CMD;
			field = (u32) (PAD_CMD_RD_RXDLY_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE0;
			field = (u32) (MSDC_PAD_TUNE0_CMDRRDLYSEL);
		}
		break;
	case CMD_RD_D_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) CMD_RD_DLY2 out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CMD;
			field = (u32) (PAD_CMD_RXDLY2);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE1;
			field = (u32) (MSDC_PAD_TUNE1_CMDRDLY2);
		}
		break;
	case CMD_RD_D_DLY2_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) CMD_RD_DLY2_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CMD;
			field = (u32) (PAD_CMD_RD_RXDLY2_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE1;
			field = (u32) (MSDC_PAD_TUNE1_CMDRRDLY2SEL);
		}
		break;
	case DAT_RD_D_DLY1:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) DAT_RD_DLY out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CONTROL;
			field = (u32) (PAD_DAT_RD_RXDLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE0;
			field = (u32) (MSDC_PAD_TUNE0_DATRRDLY);
		}
		break;
	case DAT_RD_D_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) DAT_RD_DLY_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CONTROL;
			field = (u32) (PAD_DAT_RD_RXDLY_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE0;
			field = (u32) (MSDC_PAD_TUNE0_DATRRDLYSEL);
		}
		break;
	case DAT_RD_D_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) DAT_RD_DLY2 out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CONTROL;
			field = (u32) (PAD_DAT_RD_RXDLY2);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE1;
			field = (u32) (MSDC_PAD_TUNE1_DATRRDLY2);
		}
		break;
	case DAT_RD_D_DLY2_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) DAT_RD_DLY2_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CONTROL;
			field = (u32) (PAD_DAT_RD_RXDLY2_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE1;
			field = (u32) (MSDC_PAD_TUNE1_DATRRDLY2SEL);
		}
		break;
	case INT_DAT_LATCH_CK:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug
			    ("[%s](%d) INT_DAT_LATCH_CK out of range[0~7]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT0;
		field = (u32) (MSDC_PB0_INT_DAT_LATCH_CK_SEL);
		break;
	case CKGEN_MSDC_DLY_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) CKGEN_MSDC_DLY_SEL out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT0;
		field = (u32) (MSDC_PB0_CKGEN_MSDC_DLY_SEL);
		break;
	case CMD_RSP_TA_CNTR:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug
			    ("[%s](%d) CMD_RSP_TA_CNTR out of range[0~7]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT1;
		field = (u32) (MSDC_PB1_CMD_RSP_TA_CNTR);
		break;
	case WRDAT_CRCS_TA_CNTR:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug
			    ("[%s](%d) WRDAT_CRCS_TA_CNTR out of range[0~7]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT1;
		field = (u32) (MSDC_PB1_WRDAT_CRCS_TA_CNTR);
		break;
	case SDC_RX_ENHANCE:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug
			    ("[%s](%d) SDC_RX_ENHANCE out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (((host->hw->host_function == MSDC_EMMC)
		&& (platform_top_ctrl.msdc0_rx_enhance_top == 1)
		&& host->base_top)
		|| ((host->hw->host_function == MSDC_SD)
		&& (platform_top_ctrl.msdc1_rx_enhance_top == 1)
		&& host->base_top)
		|| ((host->hw->host_function == MSDC_SDIO)
		&& (platform_top_ctrl.msdc2_rx_enhance_top == 1)
		&& host->base_top)) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CONTROL;
			field = (u32) (AUTOK_TOP_SDC_RX_ENHANCE_EN);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) SDC_ADV_CFG0;
			field = (u32) (AUTOK_SDC_RX_ENH_EN);
		}
		break;
	case PAD_CLK_TXDLY_AUTOK:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) PAD_CLK_TXDLY out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_CTL0;
			field = (u32) (PAD_CLK_TXDLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) MSDC_PAD_TUNE0;
			field = (u32) (MSDC_PAD_TUNE0_CLKTXDLY);
		}
		break;
	case EMMC50_WDATA_MUX_EN:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) EMMC50_WDATA_MUX_EN out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_CFG0;
		field = (u32) (MSDC_EMMC50_CFG_CRC_STS_SEL);
		break;
	case EMMC50_CMD_MUX_EN:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) EMMC50_CMD_MUX_EN out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_CFG0;
		field = (u32) (MSDC_EMMC50_CFG_CMD_RESP_SEL);
		break;
	case EMMC50_CMD_RESP_LATCH:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) EMMC50_CMD_LATCH out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_CFG0;
		field = (u32) (MSDC_EMMC50_CFG_PADCMD_LATCHCK);
		break;
	case EMMC50_WDATA_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) EMMC50_WDATA_EDGE out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_CFG0;
		field = (u32) (MSDC_EMMC50_CFG_CRC_STS_EDGE);
		break;
	case EMMC50_DS_Z_DLY1:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DS_Z_DLY1 out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY1);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DS_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY1);
		}
		break;
	case EMMC50_DS_Z_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) EMMC50_DS1_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DS_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLYSEL);
		}
		break;
	case EMMC50_DS_Z_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DS_Z_DLY2 out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY2);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DS_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY2);
		}
		break;
	case EMMC50_DS_Z_DLY2_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) EMMC50_DS2_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY2_SEL);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DS_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY2SEL);
		}
		break;
	case EMMC50_DS_ZDLY_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DS_Z_DLY3 out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY3);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DS_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY3);
		}
		break;
	case EMMC50_CMD_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_CMD_TX_DLY out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) EMMC_TOP_CMD;
			field = (u32) (PAD_CMD_TX_DLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_CMD_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_CMD_TUNE_TXDLY);
		}
		break;
	case EMMC50_DATA0_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA0_TX out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DAT0_TUNE;
			field = (u32) (PAD_DAT0_TX_DLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DAT01_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DAT0_TXDLY);
		}
		break;
	case EMMC50_DATA1_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA1_TX out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DAT1_TUNE;
			field = (u32) (PAD_DAT1_TX_DLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DAT01_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DAT1_TXDLY);
		}
		break;
	case EMMC50_DATA2_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA2_TX out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DAT2_TUNE;
			field = (u32) (PAD_DAT2_TX_DLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DAT23_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DAT2_TXDLY);
		}
		break;
	case EMMC50_DATA3_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA3_TX out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->base_top) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DAT3_TUNE;
			field = (u32) (PAD_DAT3_TX_DLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DAT23_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DAT3_TXDLY);
		}
		break;
	case EMMC50_DATA4_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA4_TX out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if ((host->hw->host_function == MSDC_EMMC)
			&& (host->base_top)) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DAT4_TUNE;
			field = (u32) (PAD_DAT4_TX_DLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DAT45_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DAT4_TXDLY);
		}
		break;
	case EMMC50_DATA5_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA5_TX out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if ((host->hw->host_function == MSDC_EMMC)
			&& (host->base_top)) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DAT5_TUNE;
			field = (u32) (PAD_DAT5_TX_DLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DAT45_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DAT5_TXDLY);
		}
		break;
	case EMMC50_DATA6_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA6_TX out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if ((host->hw->host_function == MSDC_EMMC)
			&& (host->base_top)) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DAT6_TUNE;
			field = (u32) (PAD_DAT6_TX_DLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DAT67_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DAT6_TXDLY);
		}
		break;
	case EMMC50_DATA7_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA7_TX out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if ((host->hw->host_function == MSDC_EMMC)
			&& (host->base_top)) {
#if !defined(FPGA_PLATFORM)
			reg = (u32 *) TOP_EMMC50_PAD_DAT7_TUNE;
			field = (u32) (PAD_DAT7_TX_DLY);
#else
			return 0;
#endif
		} else {
			reg = (u32 *) EMMC50_PAD_DAT67_TUNE;
			field = (u32) (MSDC_EMMC50_PAD_DAT7_TXDLY);
		}
		break;
	default:
		pr_debug
		    ("[%s] Value of [AUTOK_PARAM] is wrong\n", __func__);
		return -1;
	}

	if (rw == AUTOK_READ)
		MSDC_GET_FIELD(reg, field, *value);
	else if (rw == AUTOK_WRITE) {
		MSDC_SET_FIELD(reg, field, *value);

		if (param == CKGEN_MSDC_DLY_SEL)
			mdelay(1);
	} else {
		pr_debug("[%s] Value of [int rw] is wrong\n", __func__);
		return -1;
	}

	return 0;
}

static int autok_param_update(enum AUTOK_PARAM param_id,
	unsigned int result, u8 *autok_tune_res)
{
	if (param_id < TUNING_PARAM_COUNT) {
		if ((result > autok_param_info[param_id].range.end) ||
		    (result < autok_param_info[param_id].range.start)) {
			AUTOK_RAWPRINT
			    ("[AUTOK]param outof range : %d not in [%d,%d]\r\n",
				       result,
					autok_param_info[param_id].range.start,
				       autok_param_info[param_id].range.end);
			return -1;
		}
		autok_tune_res[param_id] = (u8) result;
		return 0;
	}
	AUTOK_RAWPRINT("[AUTOK]param not found\r\n");

	return -1;
}

static int autok_param_apply(struct msdc_host *host, u8 *autok_tune_res)
{
	unsigned int i = 0;
	unsigned int value = 0;

	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		value = (u8) autok_tune_res[i];
		autok_adjust_param(host, i, &value, AUTOK_WRITE);
	}

	return 0;
}

static int autok_result_dump(struct msdc_host *host, u8 *autok_tune_res)
{
	AUTOK_RAWPRINT
	    ("[AUTOK]CMD [EDGE:%d CMD_FIFO_EDGE:%d DLY1:%d DLY2:%d]\r\n",
		autok_tune_res[0], autok_tune_res[1],
		autok_tune_res[5], autok_tune_res[7]);
	AUTOK_RAWPRINT
	    ("[AUTOK]DAT [RDAT_EDGE:%d RD_FIFO_EDGE:%d WD_FIFO_EDGE:%d]\r\n",
		autok_tune_res[2], autok_tune_res[3],
		autok_tune_res[4]);
	AUTOK_RAWPRINT
	    ("[AUTOK]DAT [LATCH_CK:%d DLY1:%d DLY2:%d]\r\n",
		autok_tune_res[13], autok_tune_res[9],
		autok_tune_res[11]);
	AUTOK_RAWPRINT
	    ("[AUTOK]DS  [DLY1:%d DLY2:%d DLY3:%d]\r\n",
		autok_tune_res[14], autok_tune_res[16],
		autok_tune_res[18]);
	AUTOK_RAWPRINT
	    ("[AUTOK]CLK TX  [%d]\r\n", autok_tune_res[28]);
	AUTOK_RAWPRINT
	    ("[AUTOK]CMD TX  [%d]\r\n", autok_tune_res[19]);
	if (host->hw->host_function == MSDC_EMMC) {
		AUTOK_RAWPRINT("[AUTOK]DAT TX  [D0:%d D1:%d D2:%d D3:%d]\r\n",
			autok_tune_res[20], autok_tune_res[21],
			autok_tune_res[22], autok_tune_res[23]);
		AUTOK_RAWPRINT("[AUTOK]DAT TX  [D4:%d D5:%d D6:%d D7:%d]\r\n",
			autok_tune_res[24], autok_tune_res[25],
			autok_tune_res[26], autok_tune_res[27]);
	} else {
		AUTOK_RAWPRINT("[AUTOK]DAT TX  [D0:%d D1:%d D2:%d D3:%d]\r\n",
			autok_tune_res[20], autok_tune_res[21],
			autok_tune_res[22], autok_tune_res[23]);
	}

	return 0;
}

#if AUTOK_PARAM_DUMP_ENABLE
static int autok_register_dump(struct msdc_host *host)
{
	unsigned int i = 0;
	unsigned int value = 0;
	u8 autok_tune_res[TUNING_PARAM_COUNT];

	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		autok_adjust_param(host, i, &value, AUTOK_READ);
		autok_tune_res[i] = value;
	}
	AUTOK_RAWPRINT
	    ("[AUTOK]CMD [EDGE:%d CMD_FIFO_EDGE:%d DLY1:%d DLY2:%d]\r\n",
		autok_tune_res[0], autok_tune_res[1],
		autok_tune_res[5], autok_tune_res[7]);
	AUTOK_RAWPRINT
	    ("[AUTOK]DAT [RDAT_EDGE:%d RD_FIFO_EDGE:%d WD_FIFO_EDGE:%d]\r\n",
		autok_tune_res[2], autok_tune_res[3],
		autok_tune_res[4]);
	AUTOK_RAWPRINT
	    ("[AUTOK]DAT [LATCH_CK:%d DLY1:%d DLY2:%d]\r\n",
		autok_tune_res[13], autok_tune_res[9],
		autok_tune_res[11]);
	AUTOK_RAWPRINT
	    ("[AUTOK]DS  [DLY1:%d DLY2:%d DLY3:%d]\r\n",
		autok_tune_res[14], autok_tune_res[16],
		autok_tune_res[18]);
	AUTOK_RAWPRINT("[AUTOK]CLK TX  [%d]\r\n", autok_tune_res[28]);
	AUTOK_RAWPRINT("[AUTOK]CMD TX  [%d]\r\n", autok_tune_res[19]);
	if (host->hw->host_function == MSDC_EMMC) {
		AUTOK_RAWPRINT("[AUTOK]DAT TX  [D0:%d D1:%d D2:%d D3:%d]\r\n",
			autok_tune_res[20], autok_tune_res[21],
			autok_tune_res[22], autok_tune_res[23]);
		AUTOK_RAWPRINT("[AUTOK]DAT TX  [D4:%d D5:%d D6:%d D7:%d]\r\n",
			autok_tune_res[24], autok_tune_res[25],
			autok_tune_res[26], autok_tune_res[27]);
	} else {
		AUTOK_RAWPRINT("[AUTOK]DAT TX  [D0:%d D1:%d D2:%d D3:%d]\r\n",
			autok_tune_res[20], autok_tune_res[21],
			autok_tune_res[22], autok_tune_res[23]);
	}

	return 0;
}
#endif

void autok_tuning_parameter_init(struct msdc_host *host, u8 *res)
{
	unsigned int ret = 0;
	/* void __iomem *base = host->base; */

	/* MSDC_SET_FIELD(MSDC_PATCH_BIT2, 7<<29, 2); */
	/* MSDC_SET_FIELD(MSDC_PATCH_BIT2, 7<<16, 4); */

	ret = autok_param_apply(host, res);
}

/*******************************************************
 * Function: autok_adjust_paddly                       *
 * Param : value - delay cnt from 0 to 63              *
 *         pad_sel - 0 for cmd pad and 1 for data pad  *
 *******************************************************/
#define CMD_PAD_RDLY 0
#define DAT_PAD_RDLY 1
#define DS_PAD_RDLY 2
static void autok_adjust_paddly(struct msdc_host *host,
	unsigned int *value, unsigned int pad_sel)
{
	unsigned int uCfgL = 0;
	unsigned int uCfgLSel = 0;
	unsigned int uCfgH = 0;
	unsigned int uCfgHSel = 0;
	unsigned int dly_cnt = *value;

	uCfgL = (dly_cnt > 31) ? (31) : dly_cnt;
	uCfgH = (dly_cnt > 31) ? (dly_cnt - 32) : 0;

	uCfgLSel = (uCfgL > 0) ? 1 : 0;
	uCfgHSel = (uCfgH > 0) ? 1 : 0;
	switch (pad_sel) {
	case CMD_PAD_RDLY:
		autok_adjust_param(host, CMD_RD_D_DLY1,
			&uCfgL, AUTOK_WRITE);
		autok_adjust_param(host, CMD_RD_D_DLY2,
			&uCfgH, AUTOK_WRITE);

		autok_adjust_param(host, CMD_RD_D_DLY1_SEL,
			&uCfgLSel, AUTOK_WRITE);
		autok_adjust_param(host, CMD_RD_D_DLY2_SEL,
			&uCfgHSel, AUTOK_WRITE);
		break;
	case DAT_PAD_RDLY:
		autok_adjust_param(host, DAT_RD_D_DLY1,
			&uCfgL, AUTOK_WRITE);
		autok_adjust_param(host, DAT_RD_D_DLY2,
			&uCfgH, AUTOK_WRITE);

		autok_adjust_param(host, DAT_RD_D_DLY1_SEL,
			&uCfgLSel, AUTOK_WRITE);
		autok_adjust_param(host, DAT_RD_D_DLY2_SEL,
			&uCfgHSel, AUTOK_WRITE);
		break;
	case DS_PAD_RDLY:
		autok_adjust_param(host, EMMC50_DS_Z_DLY1,
			&uCfgL, AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DS_Z_DLY2,
			&uCfgH, AUTOK_WRITE);

		autok_adjust_param(host, EMMC50_DS_Z_DLY1_SEL,
			&uCfgLSel, AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DS_Z_DLY2_SEL,
			&uCfgHSel, AUTOK_WRITE);
		break;
	}
}

static void autok_paddly_update(unsigned int pad_sel,
	unsigned int dly_cnt, u8 *autok_tune_res)
{
	unsigned int uCfgL = 0;
	unsigned int uCfgLSel = 0;
	unsigned int uCfgH = 0;
	unsigned int uCfgHSel = 0;

	uCfgL = (dly_cnt > 31) ? (31) : dly_cnt;
	uCfgH = (dly_cnt > 31) ? (dly_cnt - 32) : 0;

	uCfgLSel = (uCfgL > 0) ? 1 : 0;
	uCfgHSel = (uCfgH > 0) ? 1 : 0;
	switch (pad_sel) {
	case CMD_PAD_RDLY:
		autok_param_update(CMD_RD_D_DLY1,
			uCfgL, autok_tune_res);
		autok_param_update(CMD_RD_D_DLY2,
			uCfgH, autok_tune_res);

		autok_param_update(CMD_RD_D_DLY1_SEL,
			uCfgLSel, autok_tune_res);
		autok_param_update(CMD_RD_D_DLY2_SEL,
			uCfgHSel, autok_tune_res);
		break;
	case DAT_PAD_RDLY:
		autok_param_update(DAT_RD_D_DLY1,
			uCfgL, autok_tune_res);
		autok_param_update(DAT_RD_D_DLY2,
			uCfgH, autok_tune_res);

		autok_param_update(DAT_RD_D_DLY1_SEL,
			uCfgLSel, autok_tune_res);
		autok_param_update(DAT_RD_D_DLY2_SEL,
			uCfgHSel, autok_tune_res);
		break;
	case DS_PAD_RDLY:
		autok_param_update(EMMC50_DS_Z_DLY1,
			uCfgL, autok_tune_res);
		autok_param_update(EMMC50_DS_Z_DLY2,
			uCfgH, autok_tune_res);

		autok_param_update(EMMC50_DS_Z_DLY1_SEL,
			uCfgLSel, autok_tune_res);
		autok_param_update(EMMC50_DS_Z_DLY2_SEL,
			uCfgHSel, autok_tune_res);
		break;
	}
}

static void autok_window_apply(enum AUTOK_SCAN_WIN scan_win,
	u64 sacn_window, unsigned char *autok_tune_res)
{
	switch (scan_win) {
	case CMD_RISE:
		autok_tune_res[CMD_SCAN_R0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[CMD_SCAN_R1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[CMD_SCAN_R2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[CMD_SCAN_R3] = (sacn_window >> 24) & 0xff;
		autok_tune_res[CMD_SCAN_R4] = (sacn_window >> 32) & 0xff;
		autok_tune_res[CMD_SCAN_R5] = (sacn_window >> 40) & 0xff;
		autok_tune_res[CMD_SCAN_R6] = (sacn_window >> 48) & 0xff;
		autok_tune_res[CMD_SCAN_R7] = (sacn_window >> 56) & 0xff;
		break;
	case CMD_FALL:
		autok_tune_res[CMD_SCAN_F0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[CMD_SCAN_F1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[CMD_SCAN_F2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[CMD_SCAN_F3] = (sacn_window >> 24) & 0xff;
		autok_tune_res[CMD_SCAN_F4] = (sacn_window >> 32) & 0xff;
		autok_tune_res[CMD_SCAN_F5] = (sacn_window >> 40) & 0xff;
		autok_tune_res[CMD_SCAN_F6] = (sacn_window >> 48) & 0xff;
		autok_tune_res[CMD_SCAN_F7] = (sacn_window >> 56) & 0xff;
		break;
	case DAT_RISE:
		autok_tune_res[DAT_SCAN_R0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[DAT_SCAN_R1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[DAT_SCAN_R2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[DAT_SCAN_R3] = (sacn_window >> 24) & 0xff;
		autok_tune_res[DAT_SCAN_R4] = (sacn_window >> 32) & 0xff;
		autok_tune_res[DAT_SCAN_R5] = (sacn_window >> 40) & 0xff;
		autok_tune_res[DAT_SCAN_R6] = (sacn_window >> 48) & 0xff;
		autok_tune_res[DAT_SCAN_R7] = (sacn_window >> 56) & 0xff;
		break;
	case DAT_FALL:
		autok_tune_res[DAT_SCAN_F0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[DAT_SCAN_F1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[DAT_SCAN_F2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[DAT_SCAN_F3] = (sacn_window >> 24) & 0xff;
		autok_tune_res[DAT_SCAN_F4] = (sacn_window >> 32) & 0xff;
		autok_tune_res[DAT_SCAN_F5] = (sacn_window >> 40) & 0xff;
		autok_tune_res[DAT_SCAN_F6] = (sacn_window >> 48) & 0xff;
		autok_tune_res[DAT_SCAN_F7] = (sacn_window >> 56) & 0xff;
		break;
	case DS_CMD_WIN:
		autok_tune_res[DS_CMD_SCAN_0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[DS_CMD_SCAN_1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[DS_CMD_SCAN_2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[DS_CMD_SCAN_3] = (sacn_window >> 24) & 0xff;
		autok_tune_res[DS_CMD_SCAN_4] = (sacn_window >> 32) & 0xff;
		autok_tune_res[DS_CMD_SCAN_5] = (sacn_window >> 40) & 0xff;
		autok_tune_res[DS_CMD_SCAN_6] = (sacn_window >> 48) & 0xff;
		autok_tune_res[DS_CMD_SCAN_7] = (sacn_window >> 56) & 0xff;
		break;
	case DS_DATA_WIN:
		autok_tune_res[DS_DAT_SCAN_0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[DS_DAT_SCAN_1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[DS_DAT_SCAN_2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[DS_DAT_SCAN_3] = (sacn_window >> 24) & 0xff;
		autok_tune_res[DS_DAT_SCAN_4] = (sacn_window >> 32) & 0xff;
		autok_tune_res[DS_DAT_SCAN_5] = (sacn_window >> 40) & 0xff;
		autok_tune_res[DS_DAT_SCAN_6] = (sacn_window >> 48) & 0xff;
		autok_tune_res[DS_DAT_SCAN_7] = (sacn_window >> 56) & 0xff;
		break;
	case D_CMD_RX:
		autok_tune_res[D_CMD_SCAN_0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[D_CMD_SCAN_1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[D_CMD_SCAN_2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[D_CMD_SCAN_3] = (sacn_window >> 24) & 0xff;
		break;
	case D_DATA_RX:
		autok_tune_res[D_DATA_SCAN_0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[D_DATA_SCAN_1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[D_DATA_SCAN_2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[D_DATA_SCAN_3] = (sacn_window >> 24) & 0xff;
		break;
	case H_CMD_TX:
		autok_tune_res[H_CMD_SCAN_0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[H_CMD_SCAN_1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[H_CMD_SCAN_2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[H_CMD_SCAN_3] = (sacn_window >> 24) & 0xff;
		break;
	case H_DATA_TX:
		autok_tune_res[H_DATA_SCAN_0] = (sacn_window >> 0) & 0xff;
		autok_tune_res[H_DATA_SCAN_1] = (sacn_window >> 8) & 0xff;
		autok_tune_res[H_DATA_SCAN_2] = (sacn_window >> 16) & 0xff;
		autok_tune_res[H_DATA_SCAN_3] = (sacn_window >> 24) & 0xff;
		break;
	}
}

static void msdc_autok_version_apply(unsigned char *autok_tune_res)
{
	autok_tune_res[AUTOK_VER0] = (AUTOK_VERSION >> 0) & 0xff;
	autok_tune_res[AUTOK_VER1] = (AUTOK_VERSION >> 8) & 0xff;
	autok_tune_res[AUTOK_VER2] = (AUTOK_VERSION >> 16) & 0xff;
	autok_tune_res[AUTOK_VER3] = (AUTOK_VERSION >> 24) & 0xff;
}

/*******************************************************
 * Exectue tuning IF Implenment                        *
 *******************************************************/
static int autok_write_param(struct msdc_host *host,
	enum AUTOK_PARAM param, u32 value)
{
	autok_adjust_param(host, param, &value, AUTOK_WRITE);

	return 0;
}

int autok_path_sel(struct msdc_host *host)
{
	void __iomem *base = host->base;
	void __iomem *base_top = NULL;
	struct AUTOK_PLAT_PARA_TX platform_para_tx;
	struct AUTOK_PLAT_PARA_RX platform_para_rx;
	struct AUTOK_PLAT_FUNC platform_para_func;
	struct AUTOK_PLAT_TOP_CTRL platform_top_ctrl;
#if !defined(FPGA_PLATFORM)
	base_top = host->base_top;
#endif

	memset(&platform_para_tx, 0, sizeof(struct AUTOK_PLAT_PARA_TX));
	memset(&platform_para_rx, 0, sizeof(struct AUTOK_PLAT_PARA_RX));
	memset(&platform_para_func, 0, sizeof(struct AUTOK_PLAT_FUNC));
	memset(&platform_top_ctrl, 0, sizeof(struct AUTOK_PLAT_TOP_CTRL));
	get_platform_para_tx(platform_para_tx);
	get_platform_para_rx(platform_para_rx);
	get_platform_func(platform_para_func);
	get_platform_top_ctrl(platform_top_ctrl);

	autok_write_param(host, READ_DATA_SMPL_SEL, 0);
	autok_write_param(host, WRITE_DATA_SMPL_SEL, 0);

	/* clK tune all data Line share dly */
	autok_write_param(host, DATA_DLYLINE_SEL, 0);

	/* data tune mode select */
#if CHIP_DENALI_3_DAT_TUNE
	autok_write_param(host, MSDC_DAT_TUNE_SEL, 1);
#else
	autok_write_param(host, MSDC_DAT_TUNE_SEL, 0);
#endif
	autok_write_param(host, MSDC_WCRC_ASYNC_FIFO_SEL, 1);
	autok_write_param(host, MSDC_RESP_ASYNC_FIFO_SEL, 0);

	/* eMMC50 Function Mux */
	/* write path switch to emmc45 */
	autok_write_param(host, EMMC50_WDATA_MUX_EN, 0);

	/* response path switch to emmc45 */
	autok_write_param(host, EMMC50_CMD_MUX_EN, 0);
	/* response use DS latch */
	autok_write_param(host, EMMC50_CMD_RESP_LATCH, 0);
	autok_write_param(host, EMMC50_WDATA_EDGE, 0);
	MSDC_SET_FIELD(EMMC50_CFG1, MSDC_EMMC50_CFG1_DSCFG, 0);

	/* Common Setting Config */
	autok_write_param(host, CKGEN_MSDC_DLY_SEL,
		platform_para_rx.ckgen_val);
	autok_write_param(host, CMD_RSP_TA_CNTR,
		platform_para_rx.cmd_ta_val);
	autok_write_param(host, WRDAT_CRCS_TA_CNTR,
		platform_para_rx.crc_ta_val);

	MSDC_SET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_GET_BUSY_MA,
		platform_para_rx.busy_ma_val);
	/* DDR50 byte swap issue design fix feature enable */
	if (platform_para_func.ddr50_fix == 1)
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, 1 << 19, 1);
	/* multi sync circuit design improve */
	if (platform_para_func.multi_sync == 1) {
		MSDC_SET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_STOP_DLY_SEL, 3);
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_POPENCNT, 8);
		MSDC_SET_FIELD(MSDC_PATCH_BIT1, 0x3 << 19, 3);
		MSDC_SET_FIELD(SDC_FIFO_CFG, SDC_FIFO_CFG_WR_VALID_SEL, 0);
		MSDC_SET_FIELD(SDC_FIFO_CFG, SDC_FIFO_CFG_RD_VALID_SEL, 0);
	}

	/* duty bypass that may influence timing */
	if ((host->hw->host_function == MSDC_EMMC)
		&& (platform_para_func.msdc0_bypass_duty_modify == 1)) {
		if (host->base_top) {
			MSDC_SET_FIELD(TOP_EMMC50_PAD_CTL0, DCC_SEL,
				platform_para_tx.msdc0_duty_bypass);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_CTL0, HL_SEL,
				platform_para_tx.msdc0_hl_duty_sel);
		} else {
			MSDC_SET_FIELD(EMMC50_PAD_CTL0,
				MSDC_EMMC50_PAD_CTL0_DCCSEL,
				platform_para_tx.msdc0_duty_bypass);
			MSDC_SET_FIELD(EMMC50_PAD_CTL0,
				MSDC_EMMC50_PAD_CTL0_HLSEL,
				platform_para_tx.msdc0_hl_duty_sel);
		}
	}
	if ((host->hw->host_function == MSDC_SD)
		&& (platform_para_func.msdc1_bypass_duty_modify == 1)) {
		if (host->base_top) {
			MSDC_SET_FIELD(TOP_EMMC50_PAD_CTL0, DCC_SEL,
				platform_para_tx.msdc1_duty_bypass);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_CTL0, HL_SEL,
				platform_para_tx.msdc1_hl_duty_sel);
		}
	}
	if ((host->hw->host_function == MSDC_SDIO)
		&& (platform_para_func.msdc2_bypass_duty_modify == 1)) {
		if (host->base_top) {
			MSDC_SET_FIELD(TOP_EMMC50_PAD_CTL0, DCC_SEL,
				platform_para_tx.msdc2_duty_bypass);
			MSDC_SET_FIELD(TOP_EMMC50_PAD_CTL0, HL_SEL,
				platform_para_tx.msdc2_hl_duty_sel);
		} else {
			MSDC_SET_FIELD(EMMC50_PAD_CTL0,
				MSDC_EMMC50_PAD_CTL0_DCCSEL,
				platform_para_tx.msdc2_duty_bypass);
			MSDC_SET_FIELD(EMMC50_PAD_CTL0,
				MSDC_EMMC50_PAD_CTL0_HLSEL,
				platform_para_tx.msdc2_hl_duty_sel);
		}
	}

	return 0;
}
EXPORT_SYMBOL(autok_path_sel);

int autok_init_ddr208(struct msdc_host *host)
{
	void __iomem *base = host->base;
	struct AUTOK_PLAT_PARA_RX platform_para_rx;
	struct AUTOK_PLAT_FUNC platform_para_func;

	memset(&platform_para_rx, 0, sizeof(struct AUTOK_PLAT_PARA_RX));
	memset(&platform_para_func, 0, sizeof(struct AUTOK_PLAT_FUNC));
	get_platform_para_rx(platform_para_rx);
	get_platform_func(platform_para_func);

	/* driver may miss data tune path setting in the interim */
	autok_path_sel(host);

	/* if any specific config need modify add here */
	if (platform_para_func.rx_enhance) {
		autok_write_param(host, SDC_RX_ENHANCE, 1);
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, 0);
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL, 0);
	} else {
		/* LATCH_TA_EN Config for WCRC Path non_HS400 */
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL,
		    platform_para_rx.latch_en_crc_ddr208);
		/* LATCH_TA_EN Config for CMD Path non_HS400 */
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL,
		    platform_para_rx.latch_en_cmd_ddr208);
	}
	/* response path switch to emmc50 */
#if SDIO_PLUS_CMD_TUNE
	autok_write_param(host, EMMC50_CMD_MUX_EN, 0);
	autok_write_param(host, EMMC50_CMD_RESP_LATCH, 0);
#else
	autok_write_param(host, EMMC50_CMD_MUX_EN, 1);
	autok_write_param(host, EMMC50_CMD_RESP_LATCH, 1);
#endif
	MSDC_SET_FIELD(EMMC50_CFG1, MSDC_EMMC50_CFG1_DSCFG, 1);
	/* write path switch to emmc50 */
	autok_write_param(host, EMMC50_WDATA_MUX_EN, 1);
	/* Specifical for HS400 Path Sel */
	autok_write_param(host, MSDC_WCRC_ASYNC_FIFO_SEL, 0);
	if (platform_para_func.multi_sync == 0) {
		if (platform_para_func.new_path_ddr208 == 1) {
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_WR_VALID_SEL, 0);
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_RD_VALID_SEL, 0);
		} else if (platform_para_func.new_path_ddr208 == 0) {
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_WR_VALID_SEL, 1);
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_RD_VALID_SEL, 1);
		}
	}
	MSDC_SET_FIELD(EMMC50_CFG0, MSDC_EMMC50_CFG_READ_DAT_CNT,
		platform_para_rx.read_dat_cnt_ddr208);
	MSDC_SET_FIELD(EMMC50_CFG0, MSDC_EMMC50_CFG_END_BIT_CHK_CNT,
		platform_para_rx.end_bit_chk_cnt_ddr208);
	MSDC_SET_FIELD(EMMC50_CFG1, MSDC_EMMC50_CFG1_CKSWITCH_CNT,
		platform_para_rx.latchck_switch_cnt_ddr208);

	return 0;
}
EXPORT_SYMBOL(autok_init_ddr208);

int autok_init_sdr104(struct msdc_host *host)
{
	void __iomem *base = host->base;
	struct AUTOK_PLAT_PARA_RX platform_para_rx;
	struct AUTOK_PLAT_FUNC platform_para_func;

	memset(&platform_para_rx, 0, sizeof(struct AUTOK_PLAT_PARA_RX));
	memset(&platform_para_func, 0, sizeof(struct AUTOK_PLAT_FUNC));
	get_platform_para_rx(platform_para_rx);
	get_platform_func(platform_para_func);

	/* driver may miss data tune path setting in the interim */
	autok_path_sel(host);

	/* if any specific config need modify add here */
	if (platform_para_func.rx_enhance) {
		autok_write_param(host, SDC_RX_ENHANCE, 1);
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, 0);
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL, 0);
	} else {
		if (host->sclk <= 100000000) {
			/* LATCH_TA_EN Config for WCRC Path HS FS mode */
			MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL,
				platform_para_rx.latch_en_crc_hs);
			/* LATCH_TA_EN Config for CMD Path HS FS mode */
			MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL,
				platform_para_rx.latch_en_cmd_hs);
		} else if (host->hw->host_function == MSDC_SD) {
			/* LATCH_TA_EN Config for WCRC Path SDR104 mode */
			MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL,
				platform_para_rx.latch_en_crc_sd_sdr104);
			/* LATCH_TA_EN Config for CMD Path SDR104 mode */
			MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL,
				platform_para_rx.latch_en_cmd_sd_sdr104);
		} else if (host->hw->host_function == MSDC_SDIO) {
			/* LATCH_TA_EN Config for WCRC Path SDR104 mode */
			MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL,
				platform_para_rx.latch_en_crc_sdio_sdr104);
			/* LATCH_TA_EN Config for CMD Path SDR104 mode */
			MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL,
				platform_para_rx.latch_en_cmd_sdio_sdr104);
		}
	}
	/* enable dvfs feature */
	/* if (host->hw->host_function == MSDC_SDIO) */
	/*	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_EN, 1); */
	if (platform_para_func.multi_sync == 0) {
		if (platform_para_func.new_path_sdr104 == 1) {
			MSDC_SET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_STOP_DLY_SEL,
			    platform_para_rx.new_stop_sdr104);
			MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_POPENCNT,
			    platform_para_rx.new_water_sdr104);
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_WR_VALID_SEL, 0);
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_RD_VALID_SEL, 0);
		} else if (platform_para_func.new_path_sdr104 == 0) {
			/* use default setting */
			MSDC_SET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_STOP_DLY_SEL,
			    platform_para_rx.old_stop_sdr104);
			MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_POPENCNT,
			    platform_para_rx.old_water_sdr104);
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_WR_VALID_SEL, 1);
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_RD_VALID_SEL, 1);
		}
	}

	return 0;
}
EXPORT_SYMBOL(autok_init_sdr104);

int autok_init_hs200(struct msdc_host *host)
{
	void __iomem *base = host->base;
	struct AUTOK_PLAT_PARA_RX platform_para_rx;
	struct AUTOK_PLAT_FUNC platform_para_func;

	memset(&platform_para_rx, 0, sizeof(struct AUTOK_PLAT_PARA_RX));
	memset(&platform_para_func, 0, sizeof(struct AUTOK_PLAT_FUNC));
	get_platform_para_rx(platform_para_rx);
	get_platform_func(platform_para_func);

	/* driver may miss data tune path setting in the interim */
	autok_path_sel(host);

	/* if any specific config need modify add here */
	if (platform_para_func.rx_enhance) {
		autok_write_param(host, SDC_RX_ENHANCE, 1);
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, 0);
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL, 0);
	} else {
		/* LATCH_TA_EN Config for WCRC Path non_HS400 */
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL,
		    platform_para_rx.latch_en_crc_hs200);
		/* LATCH_TA_EN Config for CMD Path non_HS400 */
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL,
		    platform_para_rx.latch_en_cmd_hs200);
	}
	if (platform_para_func.multi_sync == 0) {
		if (platform_para_func.new_path_hs200 == 1) {
			MSDC_SET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_STOP_DLY_SEL,
			    platform_para_rx.new_stop_hs200);
			MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_POPENCNT,
			    platform_para_rx.new_water_hs200);
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_WR_VALID_SEL, 0);
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_RD_VALID_SEL, 0);
		} else if (platform_para_func.new_path_hs200 == 0) {
			/* use default setting */
			MSDC_SET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_STOP_DLY_SEL,
			    platform_para_rx.old_stop_hs200);
			MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_POPENCNT,
			    platform_para_rx.old_water_hs200);
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_WR_VALID_SEL, 1);
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_RD_VALID_SEL, 1);
		}
	}

	return 0;
}
EXPORT_SYMBOL(autok_init_hs200);

int autok_init_hs400(struct msdc_host *host)
{
	void __iomem *base = host->base;
	struct AUTOK_PLAT_PARA_RX platform_para_rx;
	struct AUTOK_PLAT_FUNC platform_para_func;

	memset(&platform_para_rx, 0, sizeof(struct AUTOK_PLAT_PARA_RX));
	memset(&platform_para_func, 0, sizeof(struct AUTOK_PLAT_FUNC));
	get_platform_para_rx(platform_para_rx);
	get_platform_func(platform_para_func);

	/* driver may miss data tune path setting in the interim */
	autok_path_sel(host);

	/* if any specific config need modify add here */
	if (platform_para_func.rx_enhance) {
		autok_write_param(host, SDC_RX_ENHANCE, 1);
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, 0);
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL, 0);
	} else {
		/* LATCH_TA_EN Config for WCRC Path HS400 */
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL,
		    platform_para_rx.latch_en_crc_hs400);
		/* LATCH_TA_EN Config for CMD Path HS400 */
		MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL,
		    platform_para_rx.latch_en_cmd_hs400);
	}
	/* write path switch to emmc50 */
	autok_write_param(host, EMMC50_WDATA_MUX_EN, 1);
	/* Specifical for HS400 Path Sel */
	autok_write_param(host, MSDC_WCRC_ASYNC_FIFO_SEL, 0);
	if (platform_para_func.multi_sync == 0) {
		if (platform_para_func.new_path_hs400 == 1) {
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_WR_VALID_SEL, 0);
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_RD_VALID_SEL, 0);
		} else if (platform_para_func.new_path_hs400 == 0) {
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_WR_VALID_SEL, 1);
			MSDC_SET_FIELD(SDC_FIFO_CFG,
				SDC_FIFO_CFG_RD_VALID_SEL, 1);
		}
	}
	MSDC_SET_FIELD(EMMC50_CFG0, MSDC_EMMC50_CFG_READ_DAT_CNT,
		platform_para_rx.read_dat_cnt_hs400);
	MSDC_SET_FIELD(EMMC50_CFG0, MSDC_EMMC50_CFG_END_BIT_CHK_CNT,
		platform_para_rx.end_bit_chk_cnt_hs400);
	MSDC_SET_FIELD(EMMC50_CFG1, MSDC_EMMC50_CFG1_CKSWITCH_CNT,
		platform_para_rx.latchck_switch_cnt_hs400);

	return 0;
}
EXPORT_SYMBOL(autok_init_hs400);

int execute_online_tuning_hs400(struct msdc_host *host, u8 *res)
{
	void __iomem *base = host->base;
	unsigned int ret = 0;
	int err = 0;
	unsigned int response;
	unsigned int uCmdEdge = 0;
	unsigned int uDatEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k, cycle_value;
	struct AUTOK_REF_INFO *pBdInfo;
	struct AUTOK_REF_INFO_NEW *pInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[TUNING_PARA_SCAN_COUNT];
	unsigned int opcode = MMC_SEND_STATUS;
	unsigned int uDatDly = 0;
	struct autok_host autok_host_para;
	struct AUTOK_PLAT_PARA_RX platform_para_rx;
	struct AUTOK_PLAT_PARA_TX platform_para_tx;

	pBdInfo = kmalloc(sizeof(struct AUTOK_REF_INFO), GFP_ATOMIC);
	if (!pBdInfo) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] mem alloc fail\r\n");
		return -1;
	}
	pInfo = kmalloc(sizeof(struct AUTOK_REF_INFO_NEW), GFP_ATOMIC);
	if (!pInfo) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] mem alloc fail\r\n");
		kfree(pBdInfo);
		return -1;
	}

	memset(&autok_host_para, 0, sizeof(struct autok_host));
	memset(&platform_para_rx, 0, sizeof(struct AUTOK_PLAT_PARA_RX));
	memset(&platform_para_tx, 0, sizeof(struct AUTOK_PLAT_PARA_TX));
	get_platform_para_rx(platform_para_rx);
	get_platform_para_tx(platform_para_tx);

	autok_init_hs400(host);
	memset((void *)p_autok_tune_res, 0,
		sizeof(p_autok_tune_res) / sizeof(u8));

	/* restore TX value */
	autok_param_update(PAD_CLK_TXDLY_AUTOK,
		platform_para_tx.msdc0_hs400_clktx, p_autok_tune_res);
	autok_param_update(EMMC50_CMD_TX_DLY,
		platform_para_tx.msdc0_hs400_cmdtx, p_autok_tune_res);
	autok_param_update(EMMC50_DATA0_TX_DLY,
		platform_para_tx.msdc0_hs400_dat0tx, p_autok_tune_res);
	autok_param_update(EMMC50_DATA1_TX_DLY,
		platform_para_tx.msdc0_hs400_dat1tx, p_autok_tune_res);
	autok_param_update(EMMC50_DATA2_TX_DLY,
		platform_para_tx.msdc0_hs400_dat2tx, p_autok_tune_res);
	autok_param_update(EMMC50_DATA3_TX_DLY,
		platform_para_tx.msdc0_hs400_dat3tx, p_autok_tune_res);
	autok_param_update(EMMC50_DATA4_TX_DLY,
		platform_para_tx.msdc0_hs400_dat4tx, p_autok_tune_res);
	autok_param_update(EMMC50_DATA5_TX_DLY,
		platform_para_tx.msdc0_hs400_dat5tx, p_autok_tune_res);
	autok_param_update(EMMC50_DATA6_TX_DLY,
		platform_para_tx.msdc0_hs400_dat6tx, p_autok_tune_res);
	autok_param_update(EMMC50_DATA7_TX_DLY,
		platform_para_tx.msdc0_hs400_dat7tx, p_autok_tune_res);
	/* Step1 : Tuning Cmd Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(pBdInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uCmdEdge = 0;
	do {
		autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode,
					    TUNE_CMD, &autok_host_para);
				if ((ret & (E_RES_CMD_TMO
				    | E_RES_RSP_CRC)) != 0) {
					RawData64 |= (u64)(1LL << j);
					break;
				} else if ((ret & E_RES_FATAL_ERR) != 0)
					goto fail;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD %d \t %d \t %s\r\n",
			uCmdEdge, score, tune_result_str64);
		if (uCmdEdge)
			autok_window_apply(CMD_FALL,
			    RawData64, p_autok_tune_res);
		else
			autok_window_apply(CMD_RISE,
			    RawData64, p_autok_tune_res);
		if (autok_check_scan_res64(RawData64,
			    &pBdInfo->scan_info[uCmdEdge],
			    AUTOK_TUNING_INACCURACY) != 0) {
			goto fail;
		}

		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	err = autok_pad_dly_sel(pBdInfo);
	if (err == -2) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]======Analysis Failed!!======\r\n");
		goto fail;
	}
	autok_param_update(CMD_EDGE,
		pBdInfo->opt_edge_sel, p_autok_tune_res);
	autok_paddly_update(CMD_PAD_RDLY,
		pBdInfo->opt_dly_cnt, p_autok_tune_res);

#ifdef SUPPORT_NEW_TX_NEW_RX
	AUTOK_RAWPRINT("[AUTOK] new_rx autok starting... ...\r\n");
	autok_tuning_parameter_init(host, p_autok_tune_res);
	/* check device status */
	ret = autok_send_tune_cmd(host, 13, TUNE_CMD, &autok_host_para);
	if (ret == E_RES_PASS) {
		response = MSDC_READ32(SDC_RESP0);
		AUTOK_RAWPRINT("[AUTOK]dev status 0x%08x\r\n", response);
	} else
		AUTOK_RAWPRINT("[AUTOK]CMD err while check dev status\r\n");

#ifdef CMDQ
	opcode = MMC_SEND_EXT_CSD; // can also use MMC_READ_SINGLE_BLOCK
#else
	opcode = MMC_READ_SINGLE_BLOCK;
#endif

//	opcode = MMC_SEND_TUNING_BLOCK_HS200;
	memset(pBdInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uDatEdge = 0;
	do {
		autok_adjust_param(host, RD_FIFO_EDGE, &uDatEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode,
					TUNE_DATA, &autok_host_para);
				   // device cant receive cmd21.(cmd13 check device status)
				   // device is idle  (cmd1 ok ?)  -> cause by power.
				   // tx drving is not best (dts)
				   // dump host info.
				if ((ret & (E_RES_CMD_TMO | E_RES_RSP_CRC)) != 0) {
					AUTOK_RAWPRINT
						 ("[AUTOK]Err CMD Fail@RD\r\n");
					goto fail;
				} else if ((ret & (E_RES_DAT_CRC |
					E_RES_DAT_TMO)) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				} else if ((ret & E_RES_FATAL_ERR) != 0)
					goto fail;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DAT %d \t %d \t %s\r\n",
		uDatEdge, score, tune_result_str64);
		if (uDatEdge)
			autok_window_apply(DAT_FALL,
					RawData64, p_autok_tune_res);
		else
			autok_window_apply(DAT_RISE,
					RawData64, p_autok_tune_res);
		if (autok_check_scan_res64(RawData64,
			&pBdInfo->scan_info[uDatEdge],
			AUTOK_TUNING_INACCURACY) != 0) {
			host->autok_error = -1;
			goto fail;
		}
		uDatEdge ^= 0x1;
	} while (uDatEdge);

	err = autok_pad_dly_sel(pBdInfo);
	if (err == -2) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			   "[AUTOK][Error]======Analysis Failed!!======\r\n");
		goto fail;
	}
	autok_param_update(RD_FIFO_EDGE, pBdInfo->opt_edge_sel,
		p_autok_tune_res);
	autok_paddly_update(DAT_PAD_RDLY, pBdInfo->opt_dly_cnt,
		p_autok_tune_res);
	autok_param_update(WD_FIFO_EDGE, pBdInfo->opt_edge_sel,
		p_autok_tune_res);
#else
	/* DLY3 keep default value 20 */
	p_autok_tune_res[EMMC50_DS_ZDLY_DLY] = platform_para_rx.ds_dly3_hs400;
	cycle_value = pBdInfo->cycle_cnt;
	/* Step2 : Tuning DS Clk Path-ZCLK only tune DLY1 */
#ifdef CMDQ
	opcode = MMC_SEND_EXT_CSD; /* can also use MMC_READ_SINGLE_BLOCK */
#else
	opcode = MMC_READ_SINGLE_BLOCK;
#endif
	autok_tuning_parameter_init(host, p_autok_tune_res);
	/* check device status */
	ret = autok_send_tune_cmd(host, MMC_SEND_STATUS, TUNE_CMD,
		&autok_host_para);
	if (ret == E_RES_PASS) {
		response = MSDC_READ32(SDC_RESP0);
		AUTOK_RAWPRINT("[AUTOK]device status 0x%08x\r\n", response);
	} else
		AUTOK_RAWPRINT("[AUTOK]CMD err while check device status\r\n");
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	/* check QSR status when CQ on */
	if (host->mmc->card && mmc_card_cmdq(host->mmc->card)) {
		ret = autok_send_tune_cmd(host, CHECK_QSR,
			TUNE_CMD, &autok_host_para);
		if (ret == E_RES_PASS) {
			response = MSDC_READ32(SDC_RESP0);
			AUTOK_RAWPRINT("[AUTOK]QSR 0x%08x\r\n", response);
		} else
			AUTOK_RAWPRINT("[AUTOK]CMD error while check QSR\r\n");
	} else
		AUTOK_RAWPRINT("[AUTOK]CQ not enabled\r\n");
#endif
	/* tune data pad delay , find data pad boundary */
	for (j = 0; j < 32; j++) {
		autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
		AUTOK_RAWPRINT("[AUTOK]%d:DMA STATUS:%d\r\n",
				__LINE__, atomic_read(&host->dma_status));
		if (MSDC_READ32(MSDC_DMA_CFG) & 0x01) {
			msdc_dump_info(NULL, 0, NULL, host->id);
			/* Trigger KE when dma is active */
			(void)0;
		}
		for (k = 0; k < AUTOK_CMD_TIMES / 4; k++) {
			ret = autok_send_tune_cmd(host, opcode, TUNE_DATA,
			    &autok_host_para);
			if ((ret & (E_RES_CMD_TMO | E_RES_RSP_CRC)) != 0) {
				AUTOK_RAWPRINT("[AUTOK]Err CMD Fail@RD\r\n");
				goto fail;
			} else if ((ret & (E_RES_DAT_CRC | E_RES_DAT_TMO)) != 0)
				break;
			else if ((ret & E_RES_FATAL_ERR) != 0) {
				goto fail;
			}
		}
		if ((ret & (E_RES_DAT_CRC | E_RES_DAT_TMO)) != 0) {
			p_autok_tune_res[DAT_RD_D_DLY1] = j;
			if (j)
				p_autok_tune_res[DAT_RD_D_DLY1_SEL] = 1;
			break;
		}
	}
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(pInfo, 0, sizeof(struct AUTOK_REF_INFO_NEW));
	RawData64 = 0LL;
	/* tune DS delay , base on data pad boundary */
	for (j = 0; j < 32; j++) {
		autok_adjust_paddly(host, &j, DS_PAD_RDLY);
		AUTOK_RAWPRINT("[AUTOK]%d:DMA STATUS:%d\r\n",
				__LINE__, atomic_read(&host->dma_status));
		if (MSDC_READ32(MSDC_DMA_CFG) & 0x01) {
			msdc_dump_info(NULL, 0, NULL, host->id);
			/* Trigger KE when dma is active */
			(void)0;
		}
		for (k = 0; k < AUTOK_CMD_TIMES / 4; k++) {
			ret = autok_send_tune_cmd(host, opcode, TUNE_DATA,
			    &autok_host_para);
			if ((ret & (E_RES_CMD_TMO
			    | E_RES_RSP_CRC)) != 0) {
				AUTOK_RAWPRINT("[AUTOK]Err CMD Fail@RD\r\n");
				goto fail;
			} else if ((ret & (E_RES_DAT_CRC | E_RES_DAT_TMO)) != 0) {
				RawData64 |= (u64) (1LL << j);
				break;
			} else if ((ret & E_RES_FATAL_ERR) != 0)
				goto fail;
		}
	}
	RawData64 |= 0xffffffff00000000;
	score = autok_simple_score64(tune_result_str64, RawData64);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DLY1/2 %d \t %d \t %s\r\n",
		uCmdEdge, score, tune_result_str64);
	autok_window_apply(DS_DATA_WIN, RawData64, p_autok_tune_res);
	if (autok_check_scan_res64_new(RawData64, &pInfo->scan_info[0], 0) != 0)
		goto fail;

	autok_ds_dly_sel(&pInfo->scan_info[0], &uDatDly);
	autok_paddly_update(DS_PAD_RDLY, uDatDly, p_autok_tune_res);
#endif
	autok_tuning_parameter_init(host, p_autok_tune_res);
	autok_result_dump(host, p_autok_tune_res);
#if AUTOK_PARAM_DUMP_ENABLE
	autok_register_dump(host);
#endif
	msdc_autok_version_apply(p_autok_tune_res);
	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
		       sizeof(p_autok_tune_res) / sizeof(u8));
	}

	kfree(pBdInfo);
	kfree(pInfo);
	return 0;
fail:
	kfree(pBdInfo);
	kfree(pInfo);
	return -1;
}

int execute_cmd_online_tuning(struct msdc_host *host, u8 *res)
{
	void __iomem *base = host->base;
#if !defined(FPGA_PLATFORM)
	void __iomem *base_top = host->base_top;
#endif
	unsigned int ret = 0;
	int err = 0;
	unsigned int uCmdEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k; /* cycle_value */
	struct AUTOK_REF_INFO *pBdInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[5];
	unsigned int opcode = MMC_SEND_STATUS;
	struct autok_host autok_host_para;

	pBdInfo = kmalloc(sizeof(struct AUTOK_REF_INFO), GFP_ATOMIC);
	if (!pBdInfo) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] mem alloc fail\r\n");
		return -1;
	}
	memset(&autok_host_para, 0, sizeof(struct autok_host));
	memset((void *)p_autok_tune_res, 0,
		sizeof(p_autok_tune_res) / sizeof(u8));

	/* Tuning Cmd Path */
	memset(pBdInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uCmdEdge = 0;
	do {
		autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode,
					    TUNE_CMD, &autok_host_para);
				if ((ret & (E_RES_CMD_TMO
				    | E_RES_RSP_CRC)) != 0) {
					RawData64 |= (u64)(1LL << j);
					break;
				} else if ((ret & E_RES_FATAL_ERR) != 0)
					goto fail;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD %d \t %d \t %s\r\n",
			uCmdEdge, score, tune_result_str64);
		if (autok_check_scan_res64(RawData64,
			    &pBdInfo->scan_info[uCmdEdge],
			    AUTOK_TUNING_INACCURACY) != 0)
			goto fail;

		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	err = autok_pad_dly_sel(pBdInfo);
	if (err == -2) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
				"[AUTOK][Error]======Analysis Failed!!======\r\n");
		goto fail;
	}
	autok_adjust_param(host, CMD_EDGE,
		&pBdInfo->opt_edge_sel, AUTOK_WRITE);
	autok_adjust_paddly(host,
		&pBdInfo->opt_dly_cnt, CMD_PAD_RDLY);
	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_RSPL,
	    p_autok_tune_res[0]);
	if (host->hw->host_function == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
		MSDC_GET_FIELD(EMMC_TOP_CMD,
		    PAD_CMD_RXDLY,
		    p_autok_tune_res[1]);
		MSDC_GET_FIELD(EMMC_TOP_CMD,
		    PAD_CMD_RD_RXDLY_SEL,
		    p_autok_tune_res[2]);
		MSDC_GET_FIELD(EMMC_TOP_CMD,
		    PAD_CMD_RXDLY2,
		    p_autok_tune_res[3]);
		MSDC_GET_FIELD(EMMC_TOP_CMD,
		    PAD_CMD_RD_RXDLY2_SEL,
		    p_autok_tune_res[4]);
#else
		kfree(pBdInfo);
		return 0;
#endif
		} else {
			if (host->base_top) {
#if !defined(FPGA_PLATFORM)
				MSDC_GET_FIELD(EMMC_TOP_CMD,
					PAD_CMD_RXDLY,
					p_autok_tune_res[1]);
				MSDC_GET_FIELD(EMMC_TOP_CMD,
					PAD_CMD_RD_RXDLY_SEL,
					p_autok_tune_res[2]);
				MSDC_GET_FIELD(EMMC_TOP_CMD,
					PAD_CMD_RXDLY2,
					p_autok_tune_res[3]);
				MSDC_GET_FIELD(EMMC_TOP_CMD,
					PAD_CMD_RD_RXDLY2_SEL,
					p_autok_tune_res[4]);
#endif
			} else {
				MSDC_GET_FIELD(MSDC_PAD_TUNE0,
					MSDC_PAD_TUNE0_CMDRDLY,
					p_autok_tune_res[1]);
				MSDC_GET_FIELD(MSDC_PAD_TUNE0,
					MSDC_PAD_TUNE0_CMDRRDLYSEL,
					p_autok_tune_res[2]);
				MSDC_GET_FIELD(MSDC_PAD_TUNE1,
					MSDC_PAD_TUNE1_CMDRDLY2,
					p_autok_tune_res[3]);
				MSDC_GET_FIELD(MSDC_PAD_TUNE1,
					MSDC_PAD_TUNE1_CMDRRDLY2SEL,
					p_autok_tune_res[4]);
			}
	}

	AUTOK_RAWPRINT("[AUTOK]CMD [EDGE:%d DLY1:%d DLY2:%d]\r\n",
		p_autok_tune_res[0], p_autok_tune_res[1], p_autok_tune_res[3]);

	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
			sizeof(p_autok_tune_res) / sizeof(u8));
	}

	kfree(pBdInfo);
	return 0;
fail:
	kfree(pBdInfo);
	return -1;
}

/* online tuning for latch ck */
int autok_tune_latch_ck(struct msdc_host *host, unsigned int opcode,
	unsigned int latch_ck)
{
	unsigned int ret = 0;
	unsigned int j, k;
	void __iomem *base = host->base;
	unsigned int tune_time;
	struct autok_host autok_host_para;
	struct AUTOK_PLAT_PARA_MISC platform_para_misc;
	struct AUTOK_PLAT_FUNC platform_para_func;

	memset(&autok_host_para, 0, sizeof(struct autok_host));
	memset(&platform_para_misc, 0, sizeof(struct AUTOK_PLAT_PARA_MISC));
	memset(&platform_para_func, 0, sizeof(struct AUTOK_PLAT_FUNC));
	get_platform_para_misc(platform_para_misc);
	get_platform_func(platform_para_func);

	if (platform_para_func.fifo_1k == 1)
		autok_host_para.fifo_tune = 1;
	else
		autok_host_para.fifo_tune = 0;
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	switch (host->hw->host_function) {
	case MSDC_EMMC:
		tune_time = platform_para_misc.latch_ck_emmc_times;
		break;
	case MSDC_SD:
		tune_time = platform_para_misc.latch_ck_sd_times;
		break;
	case MSDC_SDIO:
		tune_time = platform_para_misc.latch_ck_sdio_times;
		break;
	default:
		tune_time = platform_para_misc.latch_ck_sdio_times;
		break;
	}
	for (j = latch_ck; j < 8; j += (host->hclk / host->sclk)) {
		host->tune_latch_ck_cnt = 0;
		msdc_clear_fifo();
		MSDC_SET_FIELD(MSDC_PATCH_BIT0,
		    MSDC_PB0_INT_DAT_LATCH_CK_SEL, j);
		for (k = 0; k < tune_time; k++) {
			if (opcode == MMC_SEND_TUNING_BLOCK_HS200) {
				switch (k) {
				case 0:
					host->tune_latch_ck_cnt = 1;
					break;
				default:
					host->tune_latch_ck_cnt = k;
					break;
				}
			} else if (opcode == MMC_SEND_TUNING_BLOCK) {
				switch (k) {
				case 0:
				case 1:
				case 2:
					host->tune_latch_ck_cnt = 1;
					break;
				default:
					host->tune_latch_ck_cnt = k - 1;
					break;
				}
			} else if (opcode == MMC_SEND_EXT_CSD) {
				host->tune_latch_ck_cnt = k + 1;
			} else
				host->tune_latch_ck_cnt++;
			ret = autok_send_tune_cmd(host, opcode, TUNE_LATCH_CK,
			    &autok_host_para);
			if ((ret & (E_RES_CMD_TMO
				    | E_RES_RSP_CRC)) != 0) {
				AUTOK_RAWPRINT("[AUTOK]CMD Fail @ LATCHCK\r\n");
				return 0;
			} else if ((ret & (E_RES_DAT_CRC
					    | E_RES_DAT_TMO)) != 0) {
				AUTOK_RAWPRINT("[AUTOK]LATCH_CK %d fail\r\n",
						    j);
				break;
			}
		}
		if (ret == 0) {
			MSDC_SET_FIELD(MSDC_PATCH_BIT0,
			    MSDC_PB0_INT_DAT_LATCH_CK_SEL, j);
			break;
		}
	}
	host->tune_latch_ck_cnt = 0;

	return (j >= 8) ? 7 : j;
}

/* online tuning for eMMC4.5(hs200) */
int execute_online_tuning_hs200(struct msdc_host *host, u8 *res)
{
	void __iomem *base = host->base;
	unsigned int ret = 0;
	int err = 0;
	unsigned int response;
	unsigned int uCmdEdge = 0;
	unsigned int uDatEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k;
	struct AUTOK_REF_INFO *pBdInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[TUNING_PARA_SCAN_COUNT];
	unsigned int opcode = MMC_SEND_STATUS;
	struct autok_host autok_host_para;
	struct AUTOK_PLAT_FUNC platform_para_func;
	struct AUTOK_PLAT_PARA_TX platform_para_tx;

	pBdInfo = kmalloc(sizeof(struct AUTOK_REF_INFO), GFP_ATOMIC);
	if (!pBdInfo) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] mem alloc fail\r\n");
		return -1;
	}
	memset(&autok_host_para, 0, sizeof(struct autok_host));
	memset(&platform_para_func, 0, sizeof(struct AUTOK_PLAT_FUNC));
	memset(&platform_para_tx, 0, sizeof(struct AUTOK_PLAT_PARA_TX));
	get_platform_func(platform_para_func);
	get_platform_para_tx(platform_para_tx);

	autok_init_hs200(host);
	memset((void *)p_autok_tune_res, 0,
	    sizeof(p_autok_tune_res) / sizeof(u8));

	/* restore TX value */
	autok_param_update(PAD_CLK_TXDLY_AUTOK,
		platform_para_tx.msdc0_clktx, p_autok_tune_res);

	/* Step1 : Tuning Cmd Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(pBdInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uCmdEdge = 0;
	do {
		autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES; k++) {
				ret = autok_send_tune_cmd(host, opcode,
					    TUNE_CMD, &autok_host_para);
				if ((ret & (E_RES_CMD_TMO
				    | E_RES_RSP_CRC)) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				} else if ((ret & E_RES_FATAL_ERR) != 0)
					goto fail;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD %d \t %d \t %s\r\n",
			uCmdEdge, score, tune_result_str64);
		if (uCmdEdge)
			autok_window_apply(CMD_FALL,
			    RawData64, p_autok_tune_res);
		else
			autok_window_apply(CMD_RISE,
			    RawData64, p_autok_tune_res);
		if (autok_check_scan_res64(RawData64,
			    &pBdInfo->scan_info[uCmdEdge],
			    AUTOK_TUNING_INACCURACY) != 0) {
			host->autok_error = -1;
			goto fail;
		}

		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	err = autok_pad_dly_sel(pBdInfo);
	if (err == -2) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]======Analysis Failed!!======\r\n");
		goto fail;
	}
	autok_param_update(CMD_EDGE, pBdInfo->opt_edge_sel,
		p_autok_tune_res);
	autok_paddly_update(CMD_PAD_RDLY, pBdInfo->opt_dly_cnt,
		p_autok_tune_res);

	/* Step2 Tuning Data Path (Only Rising Edge Used) */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	/* check device status */
	ret = autok_send_tune_cmd(host, 13, TUNE_CMD, &autok_host_para);
	if (ret == E_RES_PASS) {
		response = MSDC_READ32(SDC_RESP0);
		AUTOK_RAWPRINT("[AUTOK]dev status 0x%08x\r\n", response);
	} else
		AUTOK_RAWPRINT("[AUTOK]CMD err while check dev status\r\n");
	opcode = MMC_SEND_TUNING_BLOCK_HS200;
	memset(pBdInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uDatEdge = 0;
	do {
		autok_adjust_param(host, RD_FIFO_EDGE, &uDatEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode,
					    TUNE_DATA, &autok_host_para);
				if ((ret & (E_RES_CMD_TMO
				    | E_RES_RSP_CRC)) != 0) {
					AUTOK_RAWPRINT
						("[AUTOK]Err CMD Fail@RD\r\n");
					goto fail;
				} else if ((ret & (E_RES_DAT_CRC
						    | E_RES_DAT_TMO)) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				} else if ((ret & E_RES_FATAL_ERR) != 0)
					goto fail;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DAT %d \t %d \t %s\r\n",
			uDatEdge, score, tune_result_str64);
		if (uDatEdge)
			autok_window_apply(DAT_FALL,
			    RawData64, p_autok_tune_res);
		else
			autok_window_apply(DAT_RISE,
			    RawData64, p_autok_tune_res);
		if (autok_check_scan_res64(RawData64,
			    &pBdInfo->scan_info[uDatEdge],
			    AUTOK_TUNING_INACCURACY) != 0) {
			host->autok_error = -1;
			goto fail;
		}

		uDatEdge ^= 0x1;
	} while (uDatEdge);

	err = autok_pad_dly_sel(pBdInfo);
	if (err == -2) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]======Analysis Failed!!======\r\n");
		goto fail;
	}
	autok_param_update(RD_FIFO_EDGE, pBdInfo->opt_edge_sel,
		p_autok_tune_res);
	autok_paddly_update(DAT_PAD_RDLY, pBdInfo->opt_dly_cnt,
		p_autok_tune_res);
	autok_param_update(WD_FIFO_EDGE, pBdInfo->opt_edge_sel,
		p_autok_tune_res);

	autok_tuning_parameter_init(host, p_autok_tune_res);

	/* Step3 : Tuning LATCH CK  */
	if (platform_para_func.new_path_hs200 == 0) {
		opcode = MMC_SEND_TUNING_BLOCK_HS200;
		p_autok_tune_res[INT_DAT_LATCH_CK] =
		    autok_tune_latch_ck(host, opcode,
		    p_autok_tune_res[INT_DAT_LATCH_CK]);
	}

	autok_result_dump(host, p_autok_tune_res);

#if AUTOK_PARAM_DUMP_ENABLE
	autok_register_dump(host);
#endif
	msdc_autok_version_apply(p_autok_tune_res);
	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
		       sizeof(p_autok_tune_res) / sizeof(u8));
	}

	kfree(pBdInfo);
	return 0;
fail:
	kfree(pBdInfo);
	return err;
}

/* online tuning for SDIO3.0 plus */
int execute_online_tuning_sdio30_plus(struct msdc_host *host, u8 *res)
{
#if (SDIO_PLUS_CMD_TUNE | DS_DLY3_SCAN)
	void __iomem *base = host->base;
#endif
	unsigned int ret = 0;
	unsigned int uCmdEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k;
#if DS_DLY3_SCAN
	unsigned int i;
#endif
	unsigned int opcode = MMC_SEND_TUNING_BLOCK;
#if SDIO_PLUS_CMD_TUNE
	int err = 0;
	struct AUTOK_REF_INFO *pBdInfo;
#endif
	struct AUTOK_REF_INFO_NEW *pInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[TUNING_PARA_SCAN_COUNT];
	unsigned int uDatDly = 0;
#if !SDIO_PLUS_CMD_TUNE
	u64 RawCmd64 = 0LL;
	unsigned int cmd_bd_position = 0;
	unsigned int dat_bd_position = 0;
	unsigned int cmd_bd_find = 0;
	unsigned int dat_bd_find = 0;
#endif
	struct autok_host autok_host_para;
	struct AUTOK_PLAT_PARA_RX platform_para_rx;
	struct AUTOK_PLAT_PARA_TX platform_para_tx;

#if SDIO_PLUS_CMD_TUNE
	pBdInfo = kmalloc(sizeof(struct AUTOK_REF_INFO), GFP_ATOMIC);
	if (!pBdInfo) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] mem alloc fail\r\n");
		return -1;
	}
#endif
	pInfo = kmalloc(sizeof(struct AUTOK_REF_INFO_NEW), GFP_ATOMIC);
	if (!pInfo) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] mem alloc fail\r\n");
#ifdef SDIO_PLUS_CMD_TUNE
		kfree(pBdInfo);
#endif
		return -1;
	}

	memset(&autok_host_para, 0, sizeof(struct autok_host));
	memset(&platform_para_rx, 0, sizeof(struct AUTOK_PLAT_PARA_RX));
	memset(&platform_para_tx, 0, sizeof(struct AUTOK_PLAT_PARA_TX));
	get_platform_para_rx(platform_para_rx);
	get_platform_para_tx(platform_para_tx);

	autok_init_ddr208(host);
	memset((void *)p_autok_tune_res, 0,
	    sizeof(p_autok_tune_res) / sizeof(u8));

	/* restore TX value */
	autok_param_update(PAD_CLK_TXDLY_AUTOK,
		platform_para_tx.sdio30_plus_clktx, p_autok_tune_res);
	autok_param_update(EMMC50_CMD_TX_DLY,
		platform_para_tx.sdio30_plus_cmdtx, p_autok_tune_res);
	autok_param_update(EMMC50_DATA0_TX_DLY,
		platform_para_tx.sdio30_plus_dat0tx, p_autok_tune_res);
	autok_param_update(EMMC50_DATA1_TX_DLY,
		platform_para_tx.sdio30_plus_dat1tx, p_autok_tune_res);
	autok_param_update(EMMC50_DATA2_TX_DLY,
		platform_para_tx.sdio30_plus_dat2tx, p_autok_tune_res);
	autok_param_update(EMMC50_DATA3_TX_DLY,
		platform_para_tx.sdio30_plus_dat3tx, p_autok_tune_res);
	/* Step1 : Tuning Cmd Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);

#if SDIO_PLUS_CMD_TUNE
	memset(pBdInfo, 0, sizeof(struct AUTOK_REF_INFO));
	autok_write_param(host, EMMC50_CMD_MUX_EN, 0);
	autok_write_param(host, EMMC50_CMD_RESP_LATCH, 0);
	uCmdEdge = 0;
	do {
		autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode,
					    TUNE_CMD, &autok_host_para);
				if ((ret & E_RES_RSP_CRC) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				} else if ((ret & E_RES_CMD_TMO) != 0) {
					autok_msdc_reset();
					msdc_clear_fifo();
					MSDC_WRITE32(MSDC_INT, 0xffffffff);
					RawData64 |= (u64) (1LL << j);
					break;
				} else if ((ret & E_RES_FATAL_ERR) != 0)
					goto fail;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD %d \t %d \t %s\r\n",
			uCmdEdge, score, tune_result_str64);
		if (uCmdEdge)
			autok_window_apply(CMD_FALL,
			    RawData64, p_autok_tune_res);
		else
			autok_window_apply(CMD_RISE,
			    RawData64, p_autok_tune_res);
		if (autok_check_scan_res64(RawData64,
			    &pBdInfo->scan_info[uCmdEdge],
			    AUTOK_TUNING_INACCURACY) != 0)
			goto fail;
		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	err = autok_pad_dly_sel(pBdInfo);
	if (err == -2) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]======Analysis Failed!!======\r\n");
		goto fail;
	}
	autok_param_update(CMD_EDGE, pBdInfo->opt_edge_sel,
		p_autok_tune_res);
	autok_paddly_update(CMD_PAD_RDLY, pBdInfo->opt_dly_cnt,
		p_autok_tune_res);

	/* DLY3 keep default value 20 */
	p_autok_tune_res[EMMC50_DS_ZDLY_DLY] = platform_para_rx.ds_dly3_ddr208;
	/* Step2 : Tuning DS Clk Path-ZCLK only tune DLY1 */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	/* tune data pad delay , find data pad boundary */
	for (j = 0; j < 32; j++) {
		autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
		for (k = 0; k < AUTOK_CMD_TIMES / 4; k++) {
			ret = autok_send_tune_cmd(host, opcode,
				TUNE_DATA, &autok_host_para);
			if ((ret & (E_RES_CMD_TMO | E_RES_RSP_CRC)) != 0) {
				AUTOK_DBGPRINT(AUTOK_DBG_RES,
				    "[AUTOK]Err CMD Fail@RD\r\n");
				goto fail;
			} else if ((ret & (E_RES_DAT_CRC | E_RES_DAT_TMO)) != 0)
				break;
			else if ((ret & E_RES_FATAL_ERR) != 0)
				goto fail;
		}
		if ((ret & (E_RES_DAT_CRC | E_RES_DAT_TMO)) != 0) {
			p_autok_tune_res[DAT_RD_D_DLY1] = j;
			if (j)
				p_autok_tune_res[DAT_RD_D_DLY1_SEL] = 1;
			break;
		}
	}
	autok_tuning_parameter_init(host, p_autok_tune_res);
#if DS_DLY3_SCAN
	for (i = 0; i < 32; i++) {
		MSDC_SET_FIELD(EMMC50_PAD_DS_TUNE,
		    MSDC_EMMC50_PAD_DS_TUNE_DLY3, i);
#endif
	memset(pInfo, 0, sizeof(struct AUTOK_REF_INFO_NEW));
	RawData64 = 0LL;
	/* tune DS delay , base on data pad boundary */
	for (j = 0; j < 32; j++) {
		autok_adjust_paddly(host, &j, DS_PAD_RDLY);
		for (k = 0; k < AUTOK_CMD_TIMES / 4; k++) {
			ret = autok_send_tune_cmd(host, opcode, TUNE_SDIO_PLUS,
			    &autok_host_para);
			if ((ret & (E_RES_CMD_TMO
			    | E_RES_RSP_CRC)) != 0) {
				AUTOK_RAWPRINT
				    ("[AUTOK]Err CMD Fail@RD\r\n");
				goto fail;
			} else if ((ret & (E_RES_DAT_CRC
					    | E_RES_DAT_TMO)) != 0) {
				RawData64 |= (u64) (1LL << j);
				break;
			} else if ((ret & E_RES_FATAL_ERR) != 0)
				goto fail;
		}
	}
	RawData64 |= 0xffffffff00000000;
	score = autok_simple_score64(tune_result_str64, RawData64);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DLY1/2 %d \t %d \t %s\r\n",
		uCmdEdge, score, tune_result_str64);
	autok_window_apply(DS_DATA_WIN, RawData64, p_autok_tune_res);
	if (autok_check_scan_res64_new(RawData64,
		&pInfo->scan_info[0], 0) != 0)
		goto fail;

#if DS_DLY3_SCAN
	}
#endif
	autok_ds_dly_sel(&pInfo->scan_info[0], &uDatDly);
	autok_paddly_update(DS_PAD_RDLY, uDatDly, p_autok_tune_res);
#else
	/* DLY3 keep default value 20 */
	p_autok_tune_res[EMMC50_DS_ZDLY_DLY] = platform_para_rx.ds_dly3_ddr208;
	/* Step2 : Tuning DS Clk Path-ZCLK only tune DLY1 */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	/* tune data pad delay , find cmd/data pad boundary */
	for (j = 0; j < 32; j++) {
		if (cmd_bd_find == 0)
			autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
		if (dat_bd_find == 0)
			autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
		for (k = 0; k < AUTOK_CMD_TIMES / 4; k++) {
			ret = autok_send_tune_cmd(host, opcode, TUNE_SDIO_PLUS,
			    &autok_host_para);
			if ((ret & (E_RES_CMD_TMO | E_RES_RSP_CRC)) != 0) {
				if (cmd_bd_find == 0) {
					cmd_bd_find = 1;
					cmd_bd_position = j;
				}
			} else if ((ret & E_RES_FATAL_ERR) != 0)
				goto fail;

			if ((ret & (E_RES_DAT_CRC | E_RES_DAT_TMO)) != 0) {
				if (dat_bd_find == 0) {
					dat_bd_find = 1;
					dat_bd_position = j;
				}
			} else if ((ret & E_RES_FATAL_ERR) != 0)
				goto fail;
		}
		if ((cmd_bd_find == 1) && (dat_bd_find == 1)) {
			p_autok_tune_res[CMD_RD_D_DLY1] = cmd_bd_position;
			p_autok_tune_res[DAT_RD_D_DLY1] = dat_bd_position;
			if (cmd_bd_position)
				p_autok_tune_res[CMD_RD_D_DLY1_SEL] = 1;
			if (dat_bd_position)
				p_autok_tune_res[DAT_RD_D_DLY1_SEL] = 1;
			break;
		}
	}
	autok_tuning_parameter_init(host, p_autok_tune_res);
#if DS_DLY3_SCAN
	for (i = 0; i < 32; i++) {
		MSDC_SET_FIELD(EMMC50_PAD_DS_TUNE,
		    MSDC_EMMC50_PAD_DS_TUNE_DLY3, i);
#endif
	memset(pInfo, 0, sizeof(struct AUTOK_REF_INFO_NEW));
	RawData64 = 0LL;
	/* tune DS delay , base on cmd/data pad boundary */
	for (j = 0; j < 32; j++) {
		autok_adjust_paddly(host, &j, DS_PAD_RDLY);
		for (k = 0; k < AUTOK_CMD_TIMES / 4; k++) {
			ret = autok_send_tune_cmd(host, opcode, TUNE_SDIO_PLUS,
			    &autok_host_para);
			if ((ret & (E_RES_CMD_TMO | E_RES_RSP_CRC)) != 0)
				RawCmd64 |= (u64) (1LL << j);
			else if ((ret & E_RES_FATAL_ERR) != 0)
				goto fail;

			if ((ret & (E_RES_DAT_CRC | E_RES_DAT_TMO)) != 0)
				RawData64 |= (u64) (1LL << j);
			else if ((ret & E_RES_FATAL_ERR) != 0)
				goto fail;
		}
	}
	RawCmd64 |= 0xffffffff00000000;
	score = autok_simple_score64(tune_result_str64, RawCmd64);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DLY1/2 CMD %d \t %d \t %s\r\n",
		uCmdEdge, score, tune_result_str64);
	RawData64 |= 0xffffffff00000000;
	score = autok_simple_score64(tune_result_str64, RawData64);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DLY1/2 DAT %d \t %d \t %s\r\n",
		uCmdEdge, score, tune_result_str64);
	/* check cmd and data DS merge window */
	score = autok_simple_score64(tune_result_str64, RawCmd64 | RawData64);
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
	    "[AUTOK]DLY1/2 CMD & DAT MERGE %d \t %d \t %s\r\n",
		uCmdEdge, score, tune_result_str64);
	if (score <= 5)
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
		    "[AUTOK][WARN]CMD & DAT MERGE SIZE is %d\r\n",
		    score);

	autok_window_apply(DS_CMD_WIN, RawCmd64, p_autok_tune_res);
	if (autok_check_scan_res64_new(RawCmd64,
		&pInfo->scan_info[0], 0) != 0)
		goto fail;
	autok_window_apply(DS_DATA_WIN, RawData64, p_autok_tune_res);
	if (autok_check_scan_res64_new(RawData64,
		&pInfo->scan_info[0], 0) != 0)
		goto fail;

#if DS_DLY3_SCAN
	}
#endif
	if (autok_ds_dly_sel(&pInfo->scan_info[0], &uDatDly) == 0) {
		autok_paddly_update(DS_PAD_RDLY, uDatDly, p_autok_tune_res);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			"[AUTOK][Error]======Analysis Failed!!======\r\n");
	}
#endif

	autok_tuning_parameter_init(host, p_autok_tune_res);
	autok_result_dump(host, p_autok_tune_res);
#if AUTOK_PARAM_DUMP_ENABLE
	autok_register_dump(host);
#endif
	msdc_autok_version_apply(p_autok_tune_res);
	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
			   sizeof(p_autok_tune_res) / sizeof(u8));
	}
	host->autok_error = 0;
#if SDIO_PLUS_CMD_TUNE
	kfree(pBdInfo);
#endif
	kfree(pInfo);
	return 0;
fail:
#if SDIO_PLUS_CMD_TUNE
	kfree(pBdInfo);
#endif
	kfree(pInfo);
	return -1;
}

/* online tuning for SDIO/SD */
int execute_online_tuning(struct msdc_host *host, u8 *res)
{
	void __iomem *base = host->base;
	unsigned int ret = 0;
	int err = 0;
	unsigned int uCmdEdge = 0;
	unsigned int uDatEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k;
	unsigned int opcode = MMC_SEND_TUNING_BLOCK;
	struct AUTOK_REF_INFO *pBdInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[TUNING_PARA_SCAN_COUNT];
	struct autok_host autok_host_para;
	struct AUTOK_PLAT_FUNC platform_para_func;
	struct AUTOK_PLAT_PARA_TX platform_para_tx;

	pBdInfo = kmalloc(sizeof(struct AUTOK_REF_INFO), GFP_ATOMIC);
	if (!pBdInfo) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] mem alloc fail\r\n");
		return -1;
	}
	memset(&autok_host_para, 0, sizeof(struct autok_host));
	memset(&platform_para_func, 0, sizeof(struct AUTOK_PLAT_FUNC));
	memset(&platform_para_tx, 0, sizeof(struct AUTOK_PLAT_PARA_TX));
	get_platform_func(platform_para_func);
	get_platform_para_tx(platform_para_tx);

	autok_init_sdr104(host);
	memset((void *)p_autok_tune_res, 0,
	    sizeof(p_autok_tune_res) / sizeof(u8));

	/* restore TX value */
	if (host->hw->host_function == MSDC_SDIO) {
		autok_param_update(PAD_CLK_TXDLY_AUTOK,
			platform_para_tx.msdc2_clktx, p_autok_tune_res);
	} else if (host->hw->host_function == MSDC_SD) {
		if (host->sclk <= 100000000) {
			autok_param_update(PAD_CLK_TXDLY_AUTOK,
				platform_para_tx.msdc1_clktx, p_autok_tune_res);
		} else {
			autok_param_update(PAD_CLK_TXDLY_AUTOK,
			    platform_para_tx.msdc1_sdr104_clktx,
			    p_autok_tune_res);
		}
	}
	/* Step1 : Tuning Cmd Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(pBdInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uCmdEdge = 0;
	do {
		autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host,
				    opcode, TUNE_CMD,
				    &autok_host_para);
				if ((ret & E_RES_RSP_CRC) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				} else if ((ret & E_RES_CMD_TMO) != 0) {
					autok_msdc_reset();
					msdc_clear_fifo();
					MSDC_WRITE32(MSDC_INT, 0xffffffff);
					RawData64 |= (u64) (1LL << j);
					break;
				} else if ((ret & E_RES_FATAL_ERR) != 0)
					goto fail;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD %d \t %d \t %s\r\n",
			uCmdEdge, score, tune_result_str64);
		if (uCmdEdge)
			autok_window_apply(CMD_FALL, RawData64,
				p_autok_tune_res);
		else
			autok_window_apply(CMD_RISE, RawData64,
				p_autok_tune_res);
		if (autok_check_scan_res64(RawData64,
			    &pBdInfo->scan_info[uCmdEdge],
			    AUTOK_TUNING_INACCURACY) != 0) {
			host->autok_error = -1;
			goto fail;
		}

		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	err = autok_pad_dly_sel(pBdInfo);
	if (err == -2) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]======Analysis Failed!!======\r\n");
		goto fail;
	}
	autok_param_update(CMD_EDGE, pBdInfo->opt_edge_sel,
		p_autok_tune_res);
	autok_paddly_update(CMD_PAD_RDLY, pBdInfo->opt_dly_cnt,
		p_autok_tune_res);

	/* Step2 : Tuning Data Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(pBdInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uDatEdge = 0;
	do {
		autok_adjust_param(host, RD_FIFO_EDGE, &uDatEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode,
					    TUNE_DATA, &autok_host_para);
				if ((ret & (E_RES_CMD_TMO
						    | E_RES_RSP_CRC)) != 0) {
					AUTOK_RAWPRINT
					    ("[AUTOK]Err CMD Fail@RD\r\n");
					host->autok_error = -1;
					goto fail;
				} else if ((ret & (E_RES_DAT_CRC
						    | E_RES_DAT_TMO)) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				} else if ((ret & E_RES_FATAL_ERR) != 0)
					goto fail;
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DAT %d \t %d \t %s\r\n",
			uDatEdge, score, tune_result_str64);
		if (uDatEdge)
			autok_window_apply(DAT_FALL,
			    RawData64, p_autok_tune_res);
		else
			autok_window_apply(DAT_RISE,
			    RawData64, p_autok_tune_res);
		if (autok_check_scan_res64(RawData64,
			    &pBdInfo->scan_info[uDatEdge],
			    AUTOK_TUNING_INACCURACY) != 0) {
			host->autok_error = -1;
			goto fail;
		}

		uDatEdge ^= 0x1;
	} while (uDatEdge);

	err = autok_pad_dly_sel(pBdInfo);
	if (err == -2) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]======Analysis Failed!!======\r\n");
		goto fail;
	}
	autok_param_update(RD_FIFO_EDGE, pBdInfo->opt_edge_sel,
		p_autok_tune_res);
	autok_paddly_update(DAT_PAD_RDLY, pBdInfo->opt_dly_cnt,
		p_autok_tune_res);
	autok_param_update(WD_FIFO_EDGE, pBdInfo->opt_edge_sel,
		p_autok_tune_res);

	autok_tuning_parameter_init(host, p_autok_tune_res);

	/* Step3 : Tuning LATCH CK */
	if (platform_para_func.new_path_sdr104 == 0) {
		opcode = MMC_SEND_TUNING_BLOCK;
		p_autok_tune_res[INT_DAT_LATCH_CK] =
		    autok_tune_latch_ck(host, opcode,
			p_autok_tune_res[INT_DAT_LATCH_CK]);
	}

	autok_result_dump(host, p_autok_tune_res);
#if AUTOK_PARAM_DUMP_ENABLE
	autok_register_dump(host);
#endif
	msdc_autok_version_apply(p_autok_tune_res);
	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
		       sizeof(p_autok_tune_res) / sizeof(u8));
	}
	host->autok_error = 0;

	kfree(pBdInfo);
	return 0;
fail:
	kfree(pBdInfo);
	return -1;
}

void autok_msdc_tx_setting(struct msdc_host *host, struct mmc_ios *ios)
{
	void __iomem *base = host->base;
	struct AUTOK_PLAT_PARA_TX platform_para_tx;
	unsigned int value;
	unsigned int clk_mode;

	memset(&platform_para_tx, 0, sizeof(struct AUTOK_PLAT_PARA_TX));
	get_platform_para_tx(platform_para_tx);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, clk_mode);
	if (host->hw->host_function == MSDC_EMMC) {
		if (ios->timing == MMC_TIMING_MMC_HS400) {
			MSDC_SET_FIELD(EMMC50_CFG0,
				MSDC_EMMC50_CFG_TXSKEW_SEL,
				platform_para_tx.msdc0_hs400_txskew);
			value = platform_para_tx.msdc0_hs400_clktx;
			autok_adjust_param(host, PAD_CLK_TXDLY_AUTOK,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_hs400_cmdtx;
			autok_adjust_param(host, EMMC50_CMD_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_hs400_dat0tx;
			autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_hs400_dat1tx;
			autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_hs400_dat2tx;
			autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_hs400_dat3tx;
			autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_hs400_dat4tx;
			autok_adjust_param(host, EMMC50_DATA4_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_hs400_dat5tx;
			autok_adjust_param(host, EMMC50_DATA5_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_hs400_dat6tx;
			autok_adjust_param(host, EMMC50_DATA6_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_hs400_dat7tx;
			autok_adjust_param(host, EMMC50_DATA7_TX_DLY,
			    &value, AUTOK_WRITE);
		} else if (ios->timing == MMC_TIMING_MMC_HS200) {
			MSDC_SET_FIELD(EMMC50_CFG0,
				MSDC_EMMC50_CFG_TXSKEW_SEL,
				platform_para_tx.msdc0_hs400_txskew);
		} else {
			if (ios->timing == MMC_TIMING_MMC_DDR52) {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_DDR50CKD,
					platform_para_tx.msdc0_ddr_ckd);
			} else {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_DDR50CKD, 0);
			}
			value = platform_para_tx.msdc0_clktx;
			autok_adjust_param(host, PAD_CLK_TXDLY_AUTOK,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_cmdtx;
			autok_adjust_param(host, EMMC50_CMD_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_dat0tx;
			autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_dat1tx;
			autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_dat2tx;
			autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_dat3tx;
			autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_dat4tx;
			autok_adjust_param(host, EMMC50_DATA4_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_dat5tx;
			autok_adjust_param(host, EMMC50_DATA5_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_dat6tx;
			autok_adjust_param(host, EMMC50_DATA6_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.msdc0_dat7tx;
			autok_adjust_param(host, EMMC50_DATA7_TX_DLY,
			    &value, AUTOK_WRITE);
		}
	} else if (host->hw->host_function == MSDC_SD) {
		MSDC_SET_FIELD(MSDC_IOCON,
			MSDC_IOCON_DDR50CKD, platform_para_tx.msdc1_ddr_ckd);
		if (ios->timing == MMC_TIMING_UHS_SDR104) {
			value = platform_para_tx.msdc1_sdr104_clktx;
			autok_adjust_param(host, PAD_CLK_TXDLY_AUTOK,
			    &value, AUTOK_WRITE);
		} else {
			value = platform_para_tx.msdc1_clktx;
			autok_adjust_param(host, PAD_CLK_TXDLY_AUTOK,
			    &value, AUTOK_WRITE);
		}
	} else if (host->hw->host_function == MSDC_SDIO) {
		if ((ios->timing == MMC_TIMING_UHS_DDR50)
			&& (host->sclk > 200000000)) {
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_DDR50CKD,
				platform_para_tx.msdc2_ddr_ckd);
		} else {
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_DDR50CKD, 0);
		}
		if (clk_mode == 3) {
			value = platform_para_tx.sdio30_plus_clktx;
			autok_adjust_param(host, PAD_CLK_TXDLY_AUTOK,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.sdio30_plus_cmdtx;
			autok_adjust_param(host, EMMC50_CMD_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.sdio30_plus_dat0tx;
			autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.sdio30_plus_dat1tx;
			autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.sdio30_plus_dat2tx;
			autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
			    &value, AUTOK_WRITE);
			value = platform_para_tx.sdio30_plus_dat3tx;
			autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
			    &value, AUTOK_WRITE);
		} else {
			value = platform_para_tx.msdc2_clktx;
			autok_adjust_param(host, PAD_CLK_TXDLY_AUTOK,
			    &value, AUTOK_WRITE);
		}
	}
}
EXPORT_SYMBOL(autok_msdc_tx_setting);

void autok_low_speed_switch_edge(struct msdc_host *host,
	struct mmc_ios *ios, enum ERROR_TYPE error_type)
{
	void __iomem *base = host->base;
	unsigned int orig_resp_edge, orig_crc_fifo_edge;
	unsigned int cur_resp_edge, cur_crc_fifo_edge;
	unsigned int orig_read_edge, orig_read_fifo_edge;
	unsigned int cur_read_edge, cur_read_fifo_edge;

	AUTOK_RAWPRINT("[AUTOK][low speed switch edge]======start======\r\n");
	if (host->hw->host_function == MSDC_EMMC) {
		switch (error_type) {
		case CMD_ERROR:
			MSDC_GET_FIELD(MSDC_IOCON,
			    MSDC_IOCON_RSPL,
			    orig_resp_edge);
			MSDC_SET_FIELD(MSDC_IOCON,
			    MSDC_IOCON_RSPL,
			    orig_resp_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_IOCON,
			    MSDC_IOCON_RSPL,
			    cur_resp_edge);
			AUTOK_RAWPRINT("[AUTOK][CMD err]edge %d->%d\r\n"
				, orig_resp_edge, cur_resp_edge);
			break;
		case DATA_ERROR:
#ifdef PORT0_PB0_RD_DAT_SEL_VALID
			if (ios->timing == MMC_TIMING_MMC_DDR52) {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL_SEL, 0);
				MSDC_GET_FIELD(MSDC_IOCON,
				    MSDC_IOCON_R_D_SMPL,
				    orig_read_edge);
				MSDC_SET_FIELD(MSDC_IOCON,
				    MSDC_IOCON_R_D_SMPL,
				    orig_read_edge ^ 0x1);
				MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, 0);
				MSDC_GET_FIELD(MSDC_IOCON,
				    MSDC_IOCON_R_D_SMPL,
				    cur_read_edge);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
				    MSDC_PB0_RD_DAT_SEL,
				    cur_read_fifo_edge);
				AUTOK_RAWPRINT("[AUTOK][RD err]fifo_edge = %d",
					cur_read_fifo_edge);
				AUTOK_RAWPRINT("edge %d->%d\r\n",
					orig_read_edge, cur_read_edge);
			} else {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL_SEL, 0);
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, 0);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
				    MSDC_PB0_RD_DAT_SEL,
				    orig_read_fifo_edge);
				MSDC_SET_FIELD(MSDC_PATCH_BIT0,
				    MSDC_PB0_RD_DAT_SEL,
				    orig_read_fifo_edge ^ 0x1);
				MSDC_GET_FIELD(MSDC_IOCON,
				    MSDC_IOCON_R_D_SMPL,
				    cur_read_edge);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
				    MSDC_PB0_RD_DAT_SEL,
				    cur_read_fifo_edge);
				AUTOK_RAWPRINT("[AUTOK][RD err]edge = %d",
				    cur_read_edge);
				AUTOK_RAWPRINT("fifo_edge %d->%d\r\n",
				    orig_read_fifo_edge,
				    cur_read_fifo_edge);
			}
#else
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL_SEL, 0);
			MSDC_GET_FIELD(MSDC_IOCON,
			    MSDC_IOCON_R_D_SMPL,
			    orig_read_edge);
			MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, 0);
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL, orig_read_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL, cur_read_edge);
			AUTOK_RAWPRINT("[AUTOK][RD err]edge %d->%d\r\n",
				orig_read_edge, cur_read_edge);
#endif
			break;
		case CRC_STATUS_ERROR:
			MSDC_GET_FIELD(MSDC_PATCH_BIT2,
			    MSDC_PB2_CFGCRCSTSEDGE,
			    orig_crc_fifo_edge);
			MSDC_SET_FIELD(MSDC_PATCH_BIT2,
			    MSDC_PB2_CFGCRCSTSEDGE,
			    orig_crc_fifo_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE,
				cur_crc_fifo_edge);
			AUTOK_RAWPRINT("[AUTOK][WR err]edge %d->%d\r\n",
			    orig_crc_fifo_edge, cur_crc_fifo_edge);
			break;
		}
	} else if (host->hw->host_function == MSDC_SD) {
		switch (error_type) {
		case CMD_ERROR:
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL,
				orig_resp_edge);
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL,
				orig_resp_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL,
				cur_resp_edge);
			AUTOK_RAWPRINT("[AUTOK][CMD err]edge %d->%d\r\n",
				orig_resp_edge, cur_resp_edge);
			break;
		case DATA_ERROR:
#ifdef PORT1_PB0_RD_DAT_SEL_VALID
			if (ios->timing == MMC_TIMING_UHS_DDR50) {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL_SEL, 0);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL,
					orig_read_edge);
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL,
					orig_read_edge ^ 0x1);
				MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, 0);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL,
					cur_read_edge);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL,
					cur_read_fifo_edge);
				AUTOK_RAWPRINT("[AUTOK][RD err]fifo_edge = %d",
					cur_read_fifo_edge);
				AUTOK_RAWPRINT("edge %d->%d\r\n",
					orig_read_edge, cur_read_edge);
			} else {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL_SEL, 0);
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, 0);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL,
					orig_read_fifo_edge);
				MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL,
					orig_read_fifo_edge ^ 0x1);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL,
					cur_read_edge);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL,
					cur_read_fifo_edge);
				AUTOK_RAWPRINT("[AUTOK][RD err]edge = %d",
				    cur_read_edge);
				AUTOK_RAWPRINT("fifo_edge %d->%d\r\n",
				    orig_read_fifo_edge, cur_read_fifo_edge);
			}
#else
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL_SEL, 0);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL,
				orig_read_edge);
			MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, 0);
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL,
				orig_read_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL,
				cur_read_edge);
			AUTOK_RAWPRINT("[AUTOK][RD err]edge %d->%d\r\n"
				, orig_read_edge, cur_read_edge);
#endif
			break;
		case CRC_STATUS_ERROR:
			MSDC_GET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE,
				orig_crc_fifo_edge);
			MSDC_SET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE,
				orig_crc_fifo_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE,
				cur_crc_fifo_edge);
			AUTOK_RAWPRINT("[AUTOK][WR err]edge %d->%d\r\n"
				, orig_crc_fifo_edge, cur_crc_fifo_edge);
			break;
		}
	} else if (host->hw->host_function == MSDC_SDIO) {
		switch (error_type) {
		case CMD_ERROR:
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL,
				orig_resp_edge);
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL,
				orig_resp_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_RSPL,
				cur_resp_edge);
			AUTOK_RAWPRINT("[AUTOK][CMD err]edge %d->%d\r\n",
			    orig_resp_edge, cur_resp_edge);
			break;
		case DATA_ERROR:
#ifdef PORT3_PB0_RD_DAT_SEL_VALID
			if (ios->timing == MMC_TIMING_UHS_DDR50) {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL_SEL, 0);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL,
					orig_read_edge);
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL,
					orig_read_edge ^ 0x1);
				MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, 0);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL,
					cur_read_edge);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL,
					cur_read_fifo_edge);
				AUTOK_RAWPRINT("[AUTOK][RD err]fifo_edge = %d",
					cur_read_fifo_edge);
				AUTOK_RAWPRINT("edge %d->%d\r\n",
					orig_read_edge, cur_read_edge);
			} else {
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL_SEL, 0);
				MSDC_SET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL, 0);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL,
					orig_read_fifo_edge);
				MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL,
					orig_read_fifo_edge ^ 0x1);
				MSDC_GET_FIELD(MSDC_IOCON,
					MSDC_IOCON_R_D_SMPL,
					cur_read_edge);
				MSDC_GET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL,
					cur_read_fifo_edge);
				AUTOK_RAWPRINT("[AUTOK][RD err]edge = %d",
				    cur_read_edge);
				AUTOK_RAWPRINT("fifo_edge %d->%d\r\n",
				    orig_read_fifo_edge, cur_read_fifo_edge);
			}
#else
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL_SEL, 0);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL,
				orig_read_edge);
			MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, 0);
			MSDC_SET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL,
				orig_read_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_IOCON,
				MSDC_IOCON_R_D_SMPL,
				cur_read_edge);
			AUTOK_RAWPRINT("[AUTOK][RD err]edge %d->%d\r\n",
				orig_read_edge, cur_read_edge);
#endif
			break;
		case CRC_STATUS_ERROR:
			MSDC_GET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE,
				orig_crc_fifo_edge);
			MSDC_SET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE,
				orig_crc_fifo_edge ^ 0x1);
			MSDC_GET_FIELD(MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE,
				cur_crc_fifo_edge);
			AUTOK_RAWPRINT("[AUTOK][WR err]edge %d->%d\r\n",
				orig_crc_fifo_edge, cur_crc_fifo_edge);
			break;
		}
	}
	AUTOK_RAWPRINT("[AUTOK][low speed switch edge]======end======\r\n");
}
EXPORT_SYMBOL(autok_low_speed_switch_edge);

void autok_msdc_device_rx_set(struct msdc_host *host,
	unsigned int cmd_tx, unsigned int data_p_tx, unsigned int data_n_tx)
{
	void __iomem *base = host->base;
	unsigned int ret = E_RES_PASS;
	unsigned int base_addr = 0;
	unsigned int func_num = 0;
	unsigned int reg_value = 0;
	unsigned int r_w_dirc = 0;
	unsigned int i;

	base_addr = 0x11c;
	func_num = 0x1;
	if (cmd_tx == 0)
		reg_value = 0;
	else
		reg_value = (1 << 7) + cmd_tx;
	r_w_dirc = EXT_WRITE;
	ret = autok_sdio_device_rx_set(host, func_num, base_addr,
		&reg_value, r_w_dirc, SD_IO_RW_DIRECT);
	if (ret != E_RES_PASS)
		AUTOK_RAWPRINT("[AUTOK]DRS reg 0x%x set fail\r\n",
			base_addr);

	for (i = 0; i < 4; i++) {
		base_addr = 0x124 + i;
		func_num = 0x1;
		if (data_p_tx == 0)
			reg_value = 0;
		else
			reg_value = data_p_tx + (1 << 7);
		r_w_dirc = EXT_WRITE;
		ret = autok_sdio_device_rx_set(host, func_num, base_addr,
			&reg_value, r_w_dirc, SD_IO_RW_DIRECT);
		if (ret != E_RES_PASS)
			AUTOK_RAWPRINT("[AUTOK]DRS reg 0x%x set fail\r\n",
				base_addr);
	}

	for (i = 0; i < 4; i++) {
		base_addr = 0x128 + i;
		func_num = 0x1;
		if (data_n_tx == 0)
			reg_value = 0;
		else
			reg_value = data_n_tx + (1 << 7);
		r_w_dirc = EXT_WRITE;
		ret = autok_sdio_device_rx_set(host, func_num, base_addr,
			&reg_value, r_w_dirc, SD_IO_RW_DIRECT);
		if (ret != E_RES_PASS)
			AUTOK_RAWPRINT("[AUTOK]DRS reg 0x%x set fail\r\n",
				base_addr);
	}
	/* read back setting check */
	base_addr = 0x11c;
	func_num = 0x1;
	r_w_dirc = EXT_READ;
	ret = autok_sdio_device_rx_set(host, func_num, base_addr,
		&cmd_tx, r_w_dirc, SD_IO_RW_DIRECT);
	cmd_tx = MSDC_READ32(SDC_RESP0) & 0xFF;
	if (ret != E_RES_PASS)
		AUTOK_RAWPRINT("[AUTOK]DRS reg 0x%x read fail\r\n",
			base_addr);
	for (i = 0; i < 4; i++) {
		base_addr = 0x124 + i;
		func_num = 0x1;
		r_w_dirc = EXT_READ;
		ret = autok_sdio_device_rx_set(host, func_num, base_addr,
			&data_p_tx, r_w_dirc, SD_IO_RW_DIRECT);
		if (ret != E_RES_PASS)
			AUTOK_RAWPRINT("[AUTOK]DRS reg 0x%x set fail\r\n",
				base_addr);
		else
			data_p_tx = (data_p_tx << (i * 8))
				| (MSDC_READ32(SDC_RESP0) & 0xFF);
	}

	for (i = 0; i < 4; i++) {
		base_addr = 0x128 + i;
		func_num = 0x1;
		r_w_dirc = EXT_READ;
		ret = autok_sdio_device_rx_set(host, func_num, base_addr,
			&data_n_tx, r_w_dirc, SD_IO_RW_DIRECT);
		if (ret != E_RES_PASS)
			AUTOK_RAWPRINT("[AUTOK]DRS reg 0x%x set fail\r\n",
				base_addr);
		else
			data_n_tx = (data_n_tx << (i * 8))
				| (MSDC_READ32(SDC_RESP0) & 0xFF);
	}
}

void autok_msdc_device_rx_get(struct msdc_host *host, unsigned int *cmd_tx,
	unsigned int *data_p_tx, unsigned int *data_n_tx)
{
	void __iomem *base = host->base;
	unsigned int ret = E_RES_PASS;
	unsigned int base_addr = 0;
	unsigned int func_num = 0;
	unsigned int r_w_dirc = 0;
	unsigned int i;

	/* read back setting check */
	base_addr = 0x11c;
	func_num = 0x1;
	r_w_dirc = EXT_READ;
	ret = autok_sdio_device_rx_set(host, func_num, base_addr,
		cmd_tx, r_w_dirc, SD_IO_RW_DIRECT);
	if (ret != E_RES_PASS)
		AUTOK_RAWPRINT("[AUTOK]DRS reg 0x%x read fail\r\n",
			base_addr);
	else
		*cmd_tx = MSDC_READ32(SDC_RESP0) & 0xFF;
	for (i = 0; i < 4; i++) {
		base_addr = 0x124 + i;
		func_num = 0x1;
		r_w_dirc = EXT_READ;
		ret = autok_sdio_device_rx_set(host, func_num, base_addr,
			data_p_tx, r_w_dirc, SD_IO_RW_DIRECT);
		if (ret != E_RES_PASS)
			AUTOK_RAWPRINT("[AUTOK]DRS reg 0x%x set fail\r\n",
				base_addr);
		else
			*data_p_tx = (*data_p_tx << (i * 8))
				| (MSDC_READ32(SDC_RESP0) & 0xFF);
	}

	for (i = 0; i < 4; i++) {
		base_addr = 0x128 + i;
		func_num = 0x1;
		r_w_dirc = EXT_READ;
		ret = autok_sdio_device_rx_set(host, func_num, base_addr,
			data_n_tx, r_w_dirc, SD_IO_RW_DIRECT);
		if (ret != E_RES_PASS)
			AUTOK_RAWPRINT("[AUTOK]DRS reg 0x%x set fail\r\n",
				base_addr);
		else
			*data_n_tx = (*data_n_tx << (i * 8))
				| (MSDC_READ32(SDC_RESP0) & 0xFF);
	}
}


int autok_offline_tuning_device_RX(struct msdc_host *host, u8 *res)
{
	int ret = 0;
#if AUTOK_OFFLINE_DAT_D_RX_ENABLE
	unsigned int dat_rx_sel = 0;
#endif
#if AUTOK_OFFLINE_CMD_D_RX_ENABLE
	unsigned int cmd_rx_sel = 0;
#endif

#if (AUTOK_OFFLINE_CMD_D_RX_ENABLE || AUTOK_OFFLINE_DAT_D_RX_ENABLE)
	void __iomem *base = host->base;
	unsigned int tune_rx_value;
	unsigned char tune_cnt;
	unsigned char i;
	unsigned char tune_crc_cnt[32];
	unsigned char tune_pass_cnt[32];
	unsigned char tune_tmo_cnt[32];
	char tune_result[33];
	unsigned int cmd_tx = 0;
	unsigned int dat_tx[4] = {0};
	unsigned int cmd_init_tx;
	unsigned int dat_init_tx[4] = {0};

	unsigned int check_cnt = 0;
	unsigned int iorx = 0;
	unsigned int base_addr = 0;
	unsigned int func_num = 0;
	unsigned int reg_value = 0;
	unsigned int r_w_dirc = 0;
	unsigned int cmd_rx = 0;
	unsigned int data_p_rx = 0;
	unsigned int data_n_rx = 0;
	u64 Rx64 = 0LL;
	struct autok_host autok_host_para;
	struct AUTOK_REF_INFO_NEW *pInfo;

	pInfo = kmalloc(sizeof(struct AUTOK_REF_INFO_NEW), GFP_ATOMIC);
	if (!pInfo) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] mem alloc fail\r\n");
		return -1;
	}
	memset(&autok_host_para, 0, sizeof(struct autok_host));
	memset(pInfo, 0, sizeof(struct AUTOK_REF_INFO_NEW));
	Rx64 = 0LL;

	AUTOK_RAWPRINT("[AUTOK]SDIO device function enable\r\n");
	/* read previous device setting */
	base_addr = 0x02;
	func_num = 0x0;
	reg_value = 0;
	r_w_dirc = EXT_READ;
	ret = autok_sdio_device_rx_set(host, func_num, base_addr,
		&reg_value, r_w_dirc, SD_IO_RW_DIRECT);
	if (reg_value & 0x02)
		goto tune_device_rx;
	/* function has not enabled, enable device function1 */
	base_addr = 0x02;
	func_num = 0x0;
	reg_value |= 0x02;
	r_w_dirc = EXT_WRITE;
	ret = autok_sdio_device_rx_set(host, func_num, base_addr,
		&reg_value, r_w_dirc, SD_IO_RW_DIRECT);
	if (ret != E_RES_PASS)
		AUTOK_RAWPRINT("[AUTOK]IOEx reg 0x%x set fail\r\n", base_addr);
	AUTOK_RAWPRINT("[AUTOK]SDIO device function enable ready check\r\n");
	while ((!(iorx & 0x02)) && (check_cnt < 10)) {
		check_cnt++;
		base_addr = 0x03;
		func_num = 0x0;
		reg_value = 0x00;
		r_w_dirc = EXT_READ;
		ret = autok_sdio_device_rx_set(host, func_num, base_addr,
			&reg_value, r_w_dirc, SD_IO_RW_DIRECT);
		if (ret != E_RES_PASS)
			AUTOK_RAWPRINT("[AUTOK]IOEx reg 0x%x set fail\r\n",
			    base_addr);
		iorx = MSDC_READ32(SDC_RESP0) & 0xff;
		AUTOK_RAWPRINT("[AUTOK]iorx 0x%x\r\n", iorx);
	}
tune_device_rx:
	autok_adjust_param(host, EMMC50_CMD_TX_DLY,
		&cmd_tx, AUTOK_READ);
	autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
		&dat_tx[0], AUTOK_READ);
	autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
		&dat_tx[1], AUTOK_READ);
	autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
		&dat_tx[2], AUTOK_READ);
	autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
		&dat_tx[3], AUTOK_READ);
	cmd_init_tx = 0;
	dat_init_tx[0] = 0;
	dat_init_tx[1] = 0;
	dat_init_tx[2] = 0;
	dat_init_tx[3] = 0;
	autok_adjust_param(host, EMMC50_CMD_TX_DLY,
		&cmd_init_tx, AUTOK_WRITE);
	autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
		&dat_init_tx[0], AUTOK_WRITE);
	autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
		&dat_init_tx[1], AUTOK_WRITE);
	autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
		&dat_init_tx[2], AUTOK_WRITE);
	autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
		&dat_init_tx[3], AUTOK_WRITE);
	/* store tx setting */
	autok_msdc_device_rx_get(host, &cmd_rx, &data_p_rx, &data_n_rx);
	AUTOK_RAWPRINT("[AUTOK]pre SDIO cmd rx %x data rx = %x %x\r\n",
		cmd_rx, data_p_rx, data_n_rx);
#endif
#if AUTOK_OFFLINE_CMD_D_RX_ENABLE
	/*  Tuning Cmd TX */
	AUTOK_RAWPRINT("[AUTOK][tune device cmd RX]======start======\r\n");
	/* Step1 : Tuning Cmd TX */
	for (tune_rx_value = 0; tune_rx_value < 32; tune_rx_value++) {
		tune_tmo_cnt[tune_rx_value] = 0;
		tune_crc_cnt[tune_rx_value] = 0;
		tune_pass_cnt[tune_rx_value] = 0;
		autok_msdc_device_rx_set(host, tune_rx_value,
			data_p_rx & 0x1f, data_n_rx & 0x1f);
		for (tune_cnt = 0; tune_cnt < TUNE_TX_CNT; tune_cnt++) {
			ret = autok_send_tune_cmd(host, MMC_SEND_TUNING_BLOCK,
				TUNE_SDIO_PLUS, &autok_host_para);
			if ((ret & E_RES_CMD_TMO) != 0) {
				tune_tmo_cnt[tune_rx_value]++;
				Rx64 |= (u64) (1LL << tune_rx_value);
			} else if ((ret&(E_RES_RSP_CRC)) != 0) {
				tune_crc_cnt[tune_rx_value]++;
				Rx64 |= (u64) (1LL << tune_rx_value);
			} else if ((ret & (E_RES_PASS)) == 0)
				tune_pass_cnt[tune_rx_value]++;
		}
	}

	/* print result */
	for (i = 0; i < 32; i++) {
		if ((tune_tmo_cnt[i] != 0) || (tune_crc_cnt[i] != 0))
			tune_result[i] = 'X';
		else if (tune_pass_cnt[i] == TUNE_TX_CNT)
			tune_result[i] = 'O';
	}
	tune_result[32] = '\0';
	AUTOK_RAWPRINT("[AUTOK]tune_dev_cmd_RX 0-31 %s\r\n", tune_result);
	/* select a best cmd rx setting, default setting may can not work */
	Rx64 |= 0xffffffff00000000;
	autok_check_scan_res64_new(Rx64, &pInfo->scan_info[0], 0);
	autok_ds_dly_sel(&pInfo->scan_info[0], &cmd_rx_sel);
	AUTOK_RAWPRINT("[AUTOK]tune dev cmd RX sel:%d\r\n", cmd_rx_sel);
	autok_msdc_device_rx_set(host, cmd_rx_sel, 0, 0);
	autok_adjust_param(host, EMMC50_CMD_TX_DLY, &cmd_tx, AUTOK_WRITE);
	AUTOK_RAWPRINT("[AUTOK][tune dev cmd RX]======end======\r\n");
	if (res != NULL)
		autok_window_apply(D_CMD_RX, Rx64, res);
#endif
#if AUTOK_OFFLINE_DAT_D_RX_ENABLE
	AUTOK_RAWPRINT("[AUTOK][tune dev data RX]======start======\r\n");
	memset(pInfo, 0, sizeof(struct AUTOK_REF_INFO_NEW));
	Rx64 = 0LL;
	/*  Tuning Data TX */
	for (tune_rx_value = 0; tune_rx_value < 32; tune_rx_value++) {
		tune_tmo_cnt[tune_rx_value] = 0;
		tune_crc_cnt[tune_rx_value] = 0;
		tune_pass_cnt[tune_rx_value] = 0;

		autok_msdc_device_rx_set(host, cmd_rx & 0x1f,
			tune_rx_value, tune_rx_value);
		for (tune_cnt = 0; tune_cnt < TUNE_TX_CNT; tune_cnt++) {
			/* send cmd53 write data */
			ret = autok_send_tune_cmd(host,
			    SD_IO_RW_EXTENDED, TUNE_SDIO_PLUS,
			    &autok_host_para);
			if ((ret & (E_RES_RSP_CRC | E_RES_CMD_TMO)) != 0) {
				AUTOK_RAWPRINT("[AUTOK]tune dat RX cmd%d err\n",
					MMC_WRITE_BLOCK);
				AUTOK_RAWPRINT("[AUTOK]tune dat RX fail\n");
				goto end;
			}
			if ((ret & E_RES_DAT_TMO) != 0) {
				tune_tmo_cnt[tune_rx_value]++;
				Rx64 |= (u64) (1LL << tune_rx_value);
				/* send CMD52 abort command */
				autok_send_tune_cmd(host,
				    SD_IO_RW_DIRECT,
				    TUNE_CMD, &autok_host_para);
			} else if ((ret & (E_RES_DAT_CRC)) != 0) {
				tune_crc_cnt[tune_rx_value]++;
				Rx64 |= (u64) (1LL << tune_rx_value);
				/* send CMD52 abort command */
				autok_send_tune_cmd(host,
				    SD_IO_RW_DIRECT,
				    TUNE_CMD, &autok_host_para);
			} else if ((ret & (E_RES_PASS)) == 0)
				tune_pass_cnt[tune_rx_value]++;
		}
	}

	/* print result */
	for (i = 0; i < 32; i++) {
		if ((tune_tmo_cnt[i] != 0) || (tune_crc_cnt[i] != 0))
			tune_result[i] = 'X';
		else if (tune_pass_cnt[i] == TUNE_TX_CNT)
			tune_result[i] = 'O';
	}
	tune_result[32] = '\0';
	AUTOK_RAWPRINT("[AUTOK]device DAT RX 0 - 31      %s\r\n", tune_result);
	/* select a best dat rx setting */
	Rx64 |= 0xffffffff00000000;
	autok_check_scan_res64_new(Rx64, &pInfo->scan_info[0], 0);
	autok_ds_dly_sel(&pInfo->scan_info[0], &dat_rx_sel);
	AUTOK_RAWPRINT("[AUTOK]device DAT RX dly:%d\r\n", dat_rx_sel);
	/* restore data rx setting */
	autok_msdc_device_rx_set(host, cmd_rx & 0x1f,
		data_p_rx & 0x1f, data_n_rx & 0x1f);
	autok_adjust_param(host, EMMC50_CMD_TX_DLY,
		&cmd_tx, AUTOK_WRITE);
	autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
		&dat_tx[0], AUTOK_WRITE);
	autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
		&dat_tx[1], AUTOK_WRITE);
	autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
		&dat_tx[2], AUTOK_WRITE);
	autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
		&dat_tx[3], AUTOK_WRITE);
	AUTOK_RAWPRINT("[AUTOK][tune device data RX]======end======\r\n");
	if (res != NULL)
		autok_window_apply(D_DATA_RX, Rx64, res);
end:
#endif
	kfree(pInfo);
	return ret;
}

int autok_offline_tuning_clk_TX(struct msdc_host *host, unsigned int opcode)
{
	int ret = 0;
	void __iomem *base = host->base;
	unsigned int response;
	unsigned int data_pin_status = 0xff;
	unsigned int tune_tx_value;
	unsigned char tune_cnt;
	unsigned char i;
	unsigned char tune_crc_cnt[32];
	unsigned char tune_pass_cnt[32];
	unsigned char tune_tmo_cnt[32];
	char tune_result[33];
	unsigned int clk_tx;
	struct autok_host autok_host_para;

	memset(&autok_host_para, 0, sizeof(struct autok_host));
	AUTOK_RAWPRINT("[AUTOK][tune clk TX]=========start========\r\n");
	/* store tx setting */
	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CLKTXDLY, clk_tx);

	/* Step1 : Tuning Clk TX */
	for (tune_tx_value = 0; tune_tx_value < 32; tune_tx_value++) {
		tune_tmo_cnt[tune_tx_value] = 0;
		tune_crc_cnt[tune_tx_value] = 0;
		tune_pass_cnt[tune_tx_value] = 0;
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CLKTXDLY,
			tune_tx_value);
		for (tune_cnt = 0; tune_cnt < TUNE_TX_CNT; tune_cnt++) {
			ret = autok_send_tune_cmd(host,
				    MMC_SEND_TUNING_BLOCK,
				    TUNE_CMD, &autok_host_para);
			if ((ret & E_RES_CMD_TMO) != 0) {
				autok_msdc_reset();
				msdc_clear_fifo();
				MSDC_WRITE32(MSDC_INT, 0xffffffff);
				tune_tmo_cnt[tune_tx_value]++;
			} else if ((ret&(E_RES_RSP_CRC)) != 0)
				tune_crc_cnt[tune_tx_value]++;
			else if ((ret & (E_RES_PASS)) == 0)
				tune_pass_cnt[tune_tx_value]++;
		}
	}

	/* print result */
	for (i = 0; i < 32; i++) {
		if (tune_tmo_cnt[i] != 0)
			tune_result[i] = 'X';
		else if (tune_crc_cnt[i] != 0)
			tune_result[i] = 'R';
		else if (tune_pass_cnt[i] == TUNE_TX_CNT)
			tune_result[i] = 'O';
	}
	tune_result[32] = '\0';
	AUTOK_RAWPRINT("[AUTOK]tune_clk_TX 0 - 31   %s\r\n", tune_result);
	AUTOK_RAWPRINT("[AUTOK][tune clk TX]=========end========\r\n");

	/* restore clk tx setting */
	MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CLKTXDLY, clk_tx);
	AUTOK_RAWPRINT("[AUTOK][tune clk TX @write data]======start======\r\n");

	/* Step2 : Tuning clk TX @ write data */
	for (tune_tx_value = 0; tune_tx_value < 32; tune_tx_value++) {
		tune_tmo_cnt[tune_tx_value] = 0;
		tune_crc_cnt[tune_tx_value] = 0;
		tune_pass_cnt[tune_tx_value] = 0;
		autok_host_para.clk_tx = tune_tx_value;
		for (tune_cnt = 0; tune_cnt < TUNE_TX_CNT / 2; tune_cnt++) {
			/* check device status */
			if (opcode == MMC_WRITE_BLOCK) {
				MSDC_GET_FIELD(MSDC_PS,
				    MSDC_PS_DAT,
				    data_pin_status);
				if (!(data_pin_status & 0x1)) {
					autok_send_tune_cmd(host,
					    MMC_STOP_TRANSMISSION,
					    TUNE_CMD,
					    &autok_host_para);
					while (!(data_pin_status & 0x1))
						MSDC_GET_FIELD(MSDC_PS,
						    MSDC_PS_DAT,
						    data_pin_status);
				}
			}
			/* send cmd24 write one block data */
			ret = autok_send_tune_cmd(host, opcode, TUNE_DATA,
			    &autok_host_para);
			response = MSDC_READ32(SDC_RESP0);
			if ((ret & (E_RES_RSP_CRC | E_RES_CMD_TMO)) != 0) {
				autok_msdc_reset();
				msdc_clear_fifo();
				MSDC_WRITE32(MSDC_INT, 0xffffffff);
				AUTOK_RAWPRINT("[AUTOK]tune clk TX cmd%d err\n",
					MMC_WRITE_BLOCK);
				AUTOK_RAWPRINT("[AUTOK]tune clk TX fail\n");
				goto end;
			}
			if ((ret & E_RES_DAT_TMO) != 0)
				tune_tmo_cnt[tune_tx_value]++;
			else if ((ret & (E_RES_DAT_CRC)) != 0)
				tune_crc_cnt[tune_tx_value]++;
			else if ((ret & (E_RES_PASS)) == 0)
				tune_pass_cnt[tune_tx_value]++;
		}
	}

	/* print result */
	for (i = 0; i < 32; i++) {
		if ((tune_tmo_cnt[i] != 0) || (tune_crc_cnt[i] != 0))
			tune_result[i] = 'X';
		else if (tune_pass_cnt[i] == (TUNE_TX_CNT / 2))
			tune_result[i] = 'O';
	}
	tune_result[32] = '\0';
	AUTOK_RAWPRINT("[AUTOK]tune_clk_TX 0 - 31  %s\r\n", tune_result);

end:
	/* restore clk tx setting */
	MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CLKTXDLY, clk_tx);

	AUTOK_RAWPRINT("[AUTOK][tune clk TX @write data]======end======\r\n");
	return ret;
}

int autok_emmc_tune_tx(struct msdc_host *host, unsigned int opcode)
{
	int ret = 0;
	void __iomem *base = host->base;
	unsigned int response;
	struct autok_host autok_host_para;

	memset(&autok_host_para, 0, sizeof(struct autok_host));
	/* check device status */
	response = 0;
	while (((response >> 9) & 0xF) != 4) {
		ret = autok_send_tune_cmd(host, MMC_SEND_STATUS, TUNE_CMD,
			&autok_host_para);
		if ((ret & (E_RES_RSP_CRC | E_RES_CMD_TMO)) != 0) {
			AUTOK_RAWPRINT("[AUTOK]tune data TX cmd13 err\r\n");
			AUTOK_RAWPRINT("[AUTOK]tune data TX fail\r\n");
			return -1;
		}
		response = MSDC_READ32(SDC_RESP0);
		if ((((response >> 9) & 0xF) == 5)
			|| (((response >> 9) & 0xF) == 6))
			ret = autok_send_tune_cmd(host,
			    MMC_STOP_TRANSMISSION, TUNE_CMD,
			    &autok_host_para);
	}

	/* send cmd24/cmd23-cmd25 write one block data */
	if (opcode == MMC_WRITE_MULTIPLE_BLOCK) {
		ret = autok_send_tune_cmd(host, MMC_SET_BLOCK_COUNT, TUNE_CMD,
			&autok_host_para);
		if ((ret & (E_RES_RSP_CRC | E_RES_CMD_TMO)) != 0) {
			AUTOK_RAWPRINT("[AUTOK]tune data TX cmd23 err\r\n");
			AUTOK_RAWPRINT("[AUTOK]tune data TX fail\r\n");
			return -1;
		}
	}
	return 0;
}

static int autok_tune_data_tx(struct msdc_host *host,
	struct autok_host *host_para,
	unsigned int opcode,
	unsigned int tx_value, u64 *rx64,
	unsigned char *crc_cnt, unsigned char *tmo_cnt,
	unsigned char *pass_cnt)
{
	int ret = 0;
	void __iomem *base = host->base;
	unsigned int response;
	unsigned char tune_cnt;

	for (tune_cnt = 0; tune_cnt < TUNE_TX_CNT; tune_cnt++) {
		if (host->hw->host_function == MSDC_EMMC) {
			if (autok_emmc_tune_tx(host, opcode) == -1)
				return -1;

			ret = autok_send_tune_cmd(host, opcode, TUNE_DATA,
				host_para);
			response = MSDC_READ32(SDC_RESP0);
		} else {
			/* send cmd53 write data */
			opcode = SD_IO_RW_EXTENDED;
			ret = autok_send_tune_cmd(host, opcode, TUNE_SDIO_PLUS,
				host_para);
		}
		if ((ret & (E_RES_RSP_CRC | E_RES_CMD_TMO)) != 0) {
			AUTOK_RAWPRINT("[AUTOK]tune data TX cmd%d err\n",
				opcode);
			AUTOK_RAWPRINT("[AUTOK]tune data TX fail\n");
			return -1;
		}
		if ((ret & E_RES_DAT_TMO) != 0) {
			(*tmo_cnt)++;
				*rx64 |= (u64) (1LL << tx_value);
			if ((host->hw->host_function == MSDC_EMMC)
				&& ((opcode == MMC_WRITE_MULTIPLE_BLOCK)
				|| (opcode == MMC_EXECUTE_WRITE_TASK))) {
				autok_send_tune_cmd(host,
				    MMC_STOP_TRANSMISSION,
				    TUNE_CMD, host_para);
			}
			/* send CMD52 abort command */
			if (host->hw->host_function == MSDC_SDIO)
				autok_send_tune_cmd(host,
				    SD_IO_RW_DIRECT,
				    TUNE_CMD, host_para);
		} else if ((ret & (E_RES_DAT_CRC)) != 0) {
			(*crc_cnt)++;
			*rx64 |= (u64) (1LL << tx_value);
			if ((host->hw->host_function == MSDC_EMMC)
				&& ((opcode == MMC_WRITE_MULTIPLE_BLOCK)
				|| (opcode == MMC_EXECUTE_WRITE_TASK))) {
				autok_send_tune_cmd(host,
				    MMC_STOP_TRANSMISSION,
				    TUNE_CMD, host_para);
			}
			/* send CMD52 abort command */
			if (host->hw->host_function == MSDC_SDIO)
				autok_send_tune_cmd(host,
				    SD_IO_RW_DIRECT,
				    TUNE_CMD, host_para);
		} else if ((ret & (E_RES_PASS)) == 0)
			(*pass_cnt)++;
	}
	return 0;
}

int autok_offline_tuning_TX(struct msdc_host *host, u8 *res)
{
	int ret = 0;
#if (AUTOK_OFFLINE_CMD_H_TX_ENABLE || AUTOK_OFFLINE_DAT_H_TX_ENABLE)
	void __iomem *base = host->base;
	unsigned int tune_tx_value;
	unsigned char i;
	unsigned char tune_crc_cnt[32];
	unsigned char tune_pass_cnt[32];
	unsigned char tune_tmo_cnt[32];
	char tune_result[33];
	unsigned int cmd_tx;
	unsigned int dat_tx[8] = {0};
	u64 Rx64 = 0LL;

	unsigned int check_cnt = 0;
	unsigned int iorx = 0;
	unsigned int base_addr = 0;
	unsigned int func_num = 0;
	unsigned int reg_value = 0;
	unsigned int r_w_dirc = 0;
	unsigned int cmd_rx = 0;
	unsigned int data_p_rx = 0;
	unsigned int data_n_rx = 0;
	struct AUTOK_REF_INFO_NEW *pInfo;
#endif
#if AUTOK_OFFLINE_CMD_H_TX_ENABLE
	unsigned char tune_cnt;
	unsigned int cmd_tx_sel = 0;
#endif
#if AUTOK_OFFLINE_DAT_H_TX_ENABLE
	unsigned int dat_tx_sel = 0;
	unsigned int dat_tx_separ_sel[8] = {0};
	unsigned int separate_tune_start = 0;
	unsigned int separate_tune_cnt = 1;
	unsigned int j;
	unsigned int opcode = MMC_WRITE_BLOCK;
	u64 Rx64_separate[8] = {0LL};
	unsigned char crc_cnt, tmo_cnt, pass_cnt;
	struct autok_host autok_host_para;
	struct AUTOK_PLAT_PARA_MISC platform_para_misc;

	pInfo = kmalloc(sizeof(struct AUTOK_REF_INFO_NEW), GFP_ATOMIC);
	if (!pInfo) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] mem alloc fail\r\n");
		return -1;
	}
	memset(&autok_host_para, 0, sizeof(struct autok_host));
	memset(&platform_para_misc, 0, sizeof(struct AUTOK_PLAT_PARA_MISC));
	get_platform_para_misc(platform_para_misc);
#endif

#if (AUTOK_OFFLINE_CMD_H_TX_ENABLE || AUTOK_OFFLINE_DAT_H_TX_ENABLE)
	if (host->hw->host_function == MSDC_SDIO) {
		/* read previous device setting */
		base_addr = 0x02;
		func_num = 0x0;
		reg_value = 0;
		r_w_dirc = EXT_READ;
		ret = autok_sdio_device_rx_set(host, func_num, base_addr,
			&reg_value, r_w_dirc, SD_IO_RW_DIRECT);
		if (reg_value & 0x02)
			goto tune_host_tx;
		/* function has not enabled, enable device function1 */
		AUTOK_RAWPRINT("[AUTOK]SDIO dev func enable\r\n");
		base_addr = 0x02;
		func_num = 0x0;
		reg_value |= 0x02;
		r_w_dirc = EXT_WRITE;
		ret = autok_sdio_device_rx_set(host, func_num, base_addr,
			&reg_value, r_w_dirc, SD_IO_RW_DIRECT);
		if (ret != E_RES_PASS)
			AUTOK_RAWPRINT("[AUTOK]set reg 0x%x fail\r\n",
			    base_addr);
		AUTOK_RAWPRINT("[AUTOK]dev func enable ready check\r\n");
		while ((!(iorx & 0x02)) && (check_cnt < 10)) {
			check_cnt++;
			base_addr = 0x03;
			func_num = 0x0;
			reg_value = 0x00;
			r_w_dirc = EXT_READ;
			ret = autok_sdio_device_rx_set(host,
			    func_num, base_addr,
				&reg_value, r_w_dirc, SD_IO_RW_DIRECT);
			if (ret != E_RES_PASS)
				AUTOK_RAWPRINT("[AUTOK]set reg 0x%x fail\r\n",
					base_addr);
			iorx = MSDC_READ32(SDC_RESP0) & 0xff;
			AUTOK_RAWPRINT("[AUTOK]iorx 0x%x\r\n", iorx);
		}
tune_host_tx:
		/* store tx setting */
		autok_msdc_device_rx_get(host, &cmd_rx,
			&data_p_rx, &data_n_rx);
	}
	/* store tx setting */
	if (host->hw->host_function == MSDC_EMMC) {
		autok_adjust_param(host, EMMC50_CMD_TX_DLY,
			&cmd_tx, AUTOK_READ);
		autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
			&dat_tx[0], AUTOK_READ);
		autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
			&dat_tx[1], AUTOK_READ);
		autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
			&dat_tx[2], AUTOK_READ);
		autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
			&dat_tx[3], AUTOK_READ);
		autok_adjust_param(host, EMMC50_DATA4_TX_DLY,
			&dat_tx[4], AUTOK_READ);
		autok_adjust_param(host, EMMC50_DATA5_TX_DLY,
			&dat_tx[5], AUTOK_READ);
		autok_adjust_param(host, EMMC50_DATA6_TX_DLY,
			&dat_tx[6], AUTOK_READ);
		autok_adjust_param(host, EMMC50_DATA7_TX_DLY,
			&dat_tx[7], AUTOK_READ);
	} else {
		autok_adjust_param(host, EMMC50_CMD_TX_DLY,
			&cmd_tx, AUTOK_READ);
		autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
			&dat_tx[0], AUTOK_READ);
		autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
			&dat_tx[1], AUTOK_READ);
		autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
			&dat_tx[2], AUTOK_READ);
		autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
			&dat_tx[3], AUTOK_READ);
	}
#endif
#if AUTOK_OFFLINE_CMD_H_TX_ENABLE
	AUTOK_RAWPRINT("[AUTOK][tune cmd TX]======start======\r\n");
	autok_msdc_device_rx_set(host, 0, 0, 0);
	AUTOK_RAWPRINT("[AUTOK][device RX set]CMD:0 D0-3:0 D4-7:0\r\n");
	memset(pInfo, 0, sizeof(struct AUTOK_REF_INFO_NEW));
	/* Step1 : Tuning Cmd TX */
	for (tune_tx_value = 0; tune_tx_value < 32; tune_tx_value++) {
		tune_tmo_cnt[tune_tx_value] = 0;
		tune_crc_cnt[tune_tx_value] = 0;
		tune_pass_cnt[tune_tx_value] = 0;
		autok_adjust_param(host, EMMC50_CMD_TX_DLY,
			&tune_tx_value, AUTOK_WRITE);
		for (tune_cnt = 0; tune_cnt < TUNE_TX_CNT; tune_cnt++) {
			if (host->hw->host_function == MSDC_EMMC)
				ret = autok_send_tune_cmd(host, MMC_SEND_STATUS,
					TUNE_CMD, &autok_host_para);
			else
				ret = autok_send_tune_cmd(host,
				    MMC_SEND_TUNING_BLOCK,
					TUNE_SDIO_PLUS, &autok_host_para);
			if ((ret & E_RES_CMD_TMO) != 0) {
				tune_tmo_cnt[tune_tx_value]++;
				Rx64 |= (u64) (1LL << tune_tx_value);
			} else if ((ret&(E_RES_RSP_CRC)) != 0) {
				tune_crc_cnt[tune_tx_value]++;
				Rx64 |= (u64) (1LL << tune_tx_value);
			} else if ((ret & (E_RES_PASS)) == 0)
				tune_pass_cnt[tune_tx_value]++;
		}
	}

	/* print result */
	for (i = 0; i < 32; i++) {
		if ((tune_tmo_cnt[i] != 0) || (tune_crc_cnt[i] != 0))
			tune_result[i] = 'X';
		else if (tune_pass_cnt[i] == TUNE_TX_CNT)
			tune_result[i] = 'O';
	}
	tune_result[32] = '\0';
	AUTOK_RAWPRINT("[AUTOK]tune_cmd_TX 0 - 31 %s\r\n", tune_result);
	Rx64 |= 0xffffffff00000000;
	autok_check_scan_res64_new(Rx64, &pInfo->scan_info[0], 0);
	autok_ds_dly_sel(&pInfo->scan_info[0], &cmd_tx_sel);
	AUTOK_RAWPRINT("[AUTOK]tune host cmd TX sel:%d\r\n", cmd_tx_sel);
	autok_adjust_param(host, EMMC50_CMD_TX_DLY,
		&cmd_tx_sel, AUTOK_WRITE);
	AUTOK_RAWPRINT("[AUTOK][tune cmd TX]======end======\r\n");
	if (res != NULL) {
		autok_window_apply(H_CMD_TX, Rx64, res);
		autok_param_update(EMMC50_CMD_TX_DLY, cmd_tx_sel, res);
	}

	/* restore cmd tx setting */
	autok_adjust_param(host, EMMC50_CMD_TX_DLY,
		&cmd_tx, AUTOK_WRITE);
#endif
#if AUTOK_OFFLINE_DAT_H_TX_ENABLE
	AUTOK_RAWPRINT("[AUTOK][tune data TX]======start======\r\n");
separate_tune_dat_tx:
	AUTOK_RAWPRINT("[AUTOK][separate tune data TX]======start======\r\n");
	memset(pInfo, 0, sizeof(struct AUTOK_REF_INFO_NEW));
	if (host->hw->host_function == MSDC_SDIO) {
		/* restore device cmd rx setting */
		autok_msdc_device_rx_set(host, cmd_rx & 0x1f, 0, 0);
		AUTOK_RAWPRINT("[AUTOK][dev RX set]CMD:%d D0-3:%d D4-7:%d\r\n",
			cmd_rx, 0, 0);
	}
	/* Step2 : Tuning Data TX */
	Rx64 = 0LL;
	Rx64_separate[0] = 0LL;
	Rx64_separate[1] = 0LL;
	Rx64_separate[2] = 0LL;
	Rx64_separate[3] = 0LL;
	Rx64_separate[4] = 0LL;
	Rx64_separate[5] = 0LL;
	Rx64_separate[6] = 0LL;
	Rx64_separate[7] = 0LL;
#if 0
	if (host->cmdq_en == 1) {
		opcode = MMC_EXECUTE_WRITE_TASK;
		autok_send_tune_cmd(host, MMC_SWITCH_CQ_EN,
			TUNE_CMD, &autok_host_para);
	}
#endif
	if (separate_tune_start == 1) {
		if (host->hw->host_function == MSDC_EMMC)
			separate_tune_cnt = 8;
		else
			separate_tune_cnt = 4;
	}
	if (host->hw->host_function == MSDC_EMMC) {
		if (platform_para_misc.emmc_data_tx_tune == 1)
			opcode = MMC_WRITE_MULTIPLE_BLOCK;
		else if (platform_para_misc.emmc_data_tx_tune == 0)
			opcode = MMC_WRITE_BLOCK;
	}
	for (j = 0; j < separate_tune_cnt; j++) {
		Rx64 = 0LL;
		for (tune_tx_value = 0; tune_tx_value < 32; tune_tx_value++) {
			tmo_cnt = 0;
			crc_cnt = 0;
			pass_cnt = 0;
			if ((host->hw->host_function == MSDC_EMMC)
				&& (separate_tune_start != 0)) {
				switch (j) {
				case 0:
				autok_adjust_param(host,
					EMMC50_DATA0_TX_DLY,
						&tune_tx_value, AUTOK_WRITE);
					break;
				case 1:
				autok_adjust_param(host,
					EMMC50_DATA1_TX_DLY,
						&tune_tx_value, AUTOK_WRITE);
					break;
				case 2:
				autok_adjust_param(host,
					EMMC50_DATA2_TX_DLY,
						&tune_tx_value, AUTOK_WRITE);
					break;
				case 3:
				autok_adjust_param(host,
					EMMC50_DATA3_TX_DLY,
						&tune_tx_value, AUTOK_WRITE);
					break;
				case 4:
				autok_adjust_param(host,
					EMMC50_DATA4_TX_DLY,
						&tune_tx_value, AUTOK_WRITE);
					break;
				case 5:
				autok_adjust_param(host,
					EMMC50_DATA5_TX_DLY,
						&tune_tx_value, AUTOK_WRITE);
					break;
				case 6:
				autok_adjust_param(host,
					EMMC50_DATA6_TX_DLY,
						&tune_tx_value, AUTOK_WRITE);
					break;
				case 7:
				autok_adjust_param(host,
					EMMC50_DATA7_TX_DLY,
						&tune_tx_value, AUTOK_WRITE);
					break;
				default:
					break;
				}
			} else if ((host->hw->host_function == MSDC_EMMC)
				&& (separate_tune_start == 0)) {
				autok_adjust_param(host,
					EMMC50_DATA0_TX_DLY,
				    &tune_tx_value, AUTOK_WRITE);
				autok_adjust_param(host,
					EMMC50_DATA1_TX_DLY,
				    &tune_tx_value, AUTOK_WRITE);
				autok_adjust_param(host,
					EMMC50_DATA2_TX_DLY,
				    &tune_tx_value, AUTOK_WRITE);
				autok_adjust_param(host,
					EMMC50_DATA3_TX_DLY,
				    &tune_tx_value, AUTOK_WRITE);
				autok_adjust_param(host,
					EMMC50_DATA4_TX_DLY,
				    &tune_tx_value, AUTOK_WRITE);
				autok_adjust_param(host,
					EMMC50_DATA5_TX_DLY,
				    &tune_tx_value, AUTOK_WRITE);
				autok_adjust_param(host,
					EMMC50_DATA6_TX_DLY,
				    &tune_tx_value, AUTOK_WRITE);
				autok_adjust_param(host,
					EMMC50_DATA7_TX_DLY,
				    &tune_tx_value, AUTOK_WRITE);
			} else if ((host->hw->host_function != MSDC_EMMC)
			&& (separate_tune_start == 1)) {
				switch (j) {
				case 0:
				autok_adjust_param(host,
					EMMC50_DATA0_TX_DLY,
						&tune_tx_value, AUTOK_WRITE);
					break;
				case 1:
				autok_adjust_param(host,
					EMMC50_DATA1_TX_DLY,
						&tune_tx_value, AUTOK_WRITE);
					break;
				case 2:
				autok_adjust_param(host,
					EMMC50_DATA2_TX_DLY,
						&tune_tx_value, AUTOK_WRITE);
					break;
				case 3:
				autok_adjust_param(host,
					EMMC50_DATA3_TX_DLY,
						&tune_tx_value, AUTOK_WRITE);
					break;
				default:
					break;
				}
			} else if ((host->hw->host_function != MSDC_EMMC)
			&& (separate_tune_start == 0)) {
				autok_adjust_param(host,
					EMMC50_DATA0_TX_DLY,
					&tune_tx_value, AUTOK_WRITE);
				autok_adjust_param(host,
					EMMC50_DATA1_TX_DLY,
					&tune_tx_value, AUTOK_WRITE);
				autok_adjust_param(host,
					EMMC50_DATA2_TX_DLY,
					&tune_tx_value, AUTOK_WRITE);
				autok_adjust_param(host,
					EMMC50_DATA3_TX_DLY,
					&tune_tx_value, AUTOK_WRITE);
				}
				if (autok_tune_data_tx(host, &autok_host_para,
				    opcode, tune_tx_value,
				    &Rx64, &crc_cnt,
				    &tmo_cnt,
				    &pass_cnt) != 0)
					goto end;
				if (separate_tune_start)
					Rx64_separate[j] = Rx64;
				tune_crc_cnt[tune_tx_value] = crc_cnt;
				tune_tmo_cnt[tune_tx_value] = tmo_cnt;
				tune_pass_cnt[tune_tx_value] = pass_cnt;
#if 0
				AUTOK_RAWPRINT(
				    "[AUTOK]tune_data_TX data_tx_value = %d\n",
					tune_tx_value);
				AUTOK_RAWPRINT(
			"[AUTOK]tmo_cnt = %d, crc_cnt = %d, pass_cnt = %d\n",
				    tune_tmo_cnt[tune_tx_value],
				    tune_crc_cnt[tune_tx_value],
					tune_pass_cnt[tune_tx_value]);
#endif
		}
		/* print result */
		for (i = 0; i < 32; i++) {
			if ((tune_tmo_cnt[i] != 0) || (tune_crc_cnt[i] != 0))
				tune_result[i] = 'X';
			else if (tune_pass_cnt[i] == TUNE_TX_CNT)
				tune_result[i] = 'O';
		}
		tune_result[32] = '\0';
		if (separate_tune_start == 0)
			AUTOK_RAWPRINT("[AUTOK]DAT TX 0 - 31      %s\r\n",
			    tune_result);
		else
			AUTOK_RAWPRINT("[AUTOK]DAT TX(%d) 0 - 31      %s\r\n",
			    j, tune_result);
		if (separate_tune_start == 1) {
			switch (j) {
			case 0:
				autok_adjust_param(host,
					EMMC50_DATA0_TX_DLY,
					&dat_tx_sel, AUTOK_WRITE);
				break;
			case 1:
				autok_adjust_param(host,
					EMMC50_DATA1_TX_DLY,
					&dat_tx_sel, AUTOK_WRITE);
				break;
			case 2:
				autok_adjust_param(host,
					EMMC50_DATA2_TX_DLY,
					&dat_tx_sel, AUTOK_WRITE);
				break;
			case 3:
				autok_adjust_param(host,
					EMMC50_DATA3_TX_DLY,
					&dat_tx_sel, AUTOK_WRITE);
				break;
			case 4:
				autok_adjust_param(host,
					EMMC50_DATA4_TX_DLY,
					&dat_tx_sel, AUTOK_WRITE);
				break;
			case 5:
				autok_adjust_param(host,
					EMMC50_DATA5_TX_DLY,
					&dat_tx_sel, AUTOK_WRITE);
				break;
			case 6:
				autok_adjust_param(host,
					EMMC50_DATA6_TX_DLY,
					&dat_tx_sel, AUTOK_WRITE);
				break;
			case 7:
				autok_adjust_param(host,
					EMMC50_DATA7_TX_DLY,
					&dat_tx_sel, AUTOK_WRITE);
				break;
			default:
				break;
			}
		}
	}

	/* restore data tx setting */
	if (host->hw->host_function == MSDC_EMMC) {
		autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
			&dat_tx[0], AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
			&dat_tx[1], AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
			&dat_tx[2], AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
			&dat_tx[3], AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DATA4_TX_DLY,
			&dat_tx[4], AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DATA5_TX_DLY,
			&dat_tx[5], AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DATA6_TX_DLY,
			&dat_tx[6], AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DATA7_TX_DLY,
			&dat_tx[7], AUTOK_WRITE);
	} else {
		autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
			&dat_tx[0], AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
			&dat_tx[1], AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
			&dat_tx[2], AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
			&dat_tx[3], AUTOK_WRITE);
	}
	if (separate_tune_start == 1) {
		if (host->hw->host_function == MSDC_EMMC)
			separate_tune_cnt = 8;
		else
			separate_tune_cnt = 4;
		for (j = 0; j < separate_tune_cnt; j++) {
			memset(pInfo, 0, sizeof(struct AUTOK_REF_INFO_NEW));
			Rx64_separate[j] |= 0xffffffff00000000;
			autok_check_scan_res64_new(Rx64_separate[j],
				&pInfo->scan_info[0], 0);
			autok_ds_dly_sel(&pInfo->scan_info[0],
				&dat_tx_separ_sel[j]);
			AUTOK_RAWPRINT(
			    "[AUTOK]separate tune host data%d TX sel:%d\r\n",
			    j, dat_tx_separ_sel[j]);
		}
	} else {
		Rx64 |= 0xffffffff00000000;
		autok_check_scan_res64_new(Rx64, &pInfo->scan_info[0], 0);
		autok_ds_dly_sel(&pInfo->scan_info[0], &dat_tx_sel);
		AUTOK_RAWPRINT("[AUTOK]tune host data TX sel:%d\r\n",
			dat_tx_sel);
	}
	if (host->hw->host_function == MSDC_SDIO) {
		if (separate_tune_start == 1) {
			autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
			    &dat_tx_separ_sel[0], AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
			    &dat_tx_separ_sel[1], AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
			    &dat_tx_separ_sel[2], AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
			    &dat_tx_separ_sel[3], AUTOK_WRITE);
		} else {
			autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
			    &dat_tx_sel, AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
			    &dat_tx_sel, AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
			    &dat_tx_sel, AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
			    &dat_tx_sel, AUTOK_WRITE);
		}
	} else {
		if (separate_tune_start == 1) {
			autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
			    &dat_tx_separ_sel[0], AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
			    &dat_tx_separ_sel[1], AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
			    &dat_tx_separ_sel[2], AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
			    &dat_tx_separ_sel[3], AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA4_TX_DLY,
			    &dat_tx_separ_sel[4], AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA5_TX_DLY,
			    &dat_tx_separ_sel[5], AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA6_TX_DLY,
			    &dat_tx_separ_sel[6], AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA7_TX_DLY,
			    &dat_tx_separ_sel[7], AUTOK_WRITE);
		} else {
			autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
			    &dat_tx_sel, AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
			    &dat_tx_sel, AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
			    &dat_tx_sel, AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
			    &dat_tx_sel, AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA4_TX_DLY,
			    &dat_tx_sel, AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA5_TX_DLY,
			    &dat_tx_sel, AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA6_TX_DLY,
			    &dat_tx_sel, AUTOK_WRITE);
			autok_adjust_param(host, EMMC50_DATA7_TX_DLY,
			    &dat_tx_sel, AUTOK_WRITE);
		}
	}
	AUTOK_RAWPRINT("[AUTOK][tune data TX]=========end========\r\n");
	if (res != NULL) {
		autok_window_apply(H_DATA_TX, Rx64, res);
		if (host->hw->host_function == MSDC_SDIO) {
			if (separate_tune_start == 1) {
				autok_param_update(EMMC50_DATA0_TX_DLY,
					dat_tx_separ_sel[0], res);
				autok_param_update(EMMC50_DATA1_TX_DLY,
					dat_tx_separ_sel[1], res);
				autok_param_update(EMMC50_DATA2_TX_DLY,
					dat_tx_separ_sel[2], res);
				autok_param_update(EMMC50_DATA3_TX_DLY,
					dat_tx_separ_sel[3], res);
			} else {
				autok_param_update(EMMC50_DATA0_TX_DLY,
					dat_tx_sel, res);
				autok_param_update(EMMC50_DATA1_TX_DLY,
					dat_tx_sel, res);
				autok_param_update(EMMC50_DATA2_TX_DLY,
					dat_tx_sel, res);
				autok_param_update(EMMC50_DATA3_TX_DLY,
					dat_tx_sel, res);
			}
		} else {
			if (separate_tune_start == 1) {
				autok_param_update(EMMC50_DATA0_TX_DLY,
					dat_tx_separ_sel[0], res);
				autok_param_update(EMMC50_DATA1_TX_DLY,
					dat_tx_separ_sel[1], res);
				autok_param_update(EMMC50_DATA2_TX_DLY,
					dat_tx_separ_sel[2], res);
				autok_param_update(EMMC50_DATA3_TX_DLY,
					dat_tx_separ_sel[3], res);
				autok_param_update(EMMC50_DATA4_TX_DLY,
					dat_tx_separ_sel[4], res);
				autok_param_update(EMMC50_DATA5_TX_DLY,
					dat_tx_separ_sel[5], res);
				autok_param_update(EMMC50_DATA6_TX_DLY,
					dat_tx_separ_sel[6], res);
				autok_param_update(EMMC50_DATA7_TX_DLY,
					dat_tx_separ_sel[7], res);
			} else {
				autok_param_update(EMMC50_DATA0_TX_DLY,
					dat_tx_sel, res);
				autok_param_update(EMMC50_DATA1_TX_DLY,
					dat_tx_sel, res);
				autok_param_update(EMMC50_DATA2_TX_DLY,
					dat_tx_sel, res);
				autok_param_update(EMMC50_DATA3_TX_DLY,
					dat_tx_sel, res);
				autok_param_update(EMMC50_DATA4_TX_DLY,
					dat_tx_sel, res);
				autok_param_update(EMMC50_DATA5_TX_DLY,
					dat_tx_sel, res);
				autok_param_update(EMMC50_DATA6_TX_DLY,
					dat_tx_sel, res);
				autok_param_update(EMMC50_DATA7_TX_DLY,
					dat_tx_sel, res);
			}
		}
	}
	if (host->hw->host_function == MSDC_SDIO) {
		/* restore data tx setting */
		autok_msdc_device_rx_set(host, cmd_rx & 0x1f,
			data_p_rx & 0x1f, data_n_rx & 0x1f);
		AUTOK_RAWPRINT("[AUTOK][dev RX set]CMD:%d D0-3:%d D4-7:%d\r\n",
			cmd_rx, data_p_rx, data_n_rx);
	}
	if (separate_tune_start == 1)
		separate_tune_start = 0;
	else {
		if (platform_para_misc.data_tx_separate_tune == 1) {
			separate_tune_start = 1;
			goto separate_tune_dat_tx;
		}
	}
#if 0
	if (host->cmdq_en == 1)
		autok_send_tune_cmd(host, MMC_SWITCH_CQ_DIS,
			TUNE_CMD, &autok_host_para);
#endif
end:
#endif
	kfree(pInfo);
	return ret;
}

int autok_sdio30_plus_tuning(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	void __iomem *base = host->base;
	u8 autok_tune_res[TUNING_PARAM_COUNT];
	unsigned int i = 0;
	unsigned int value = 0;
	unsigned int dvfs_en = 0;
	unsigned int dvfs_hw = 0;
	unsigned int dtoc = 0;

	do_gettimeofday(&tm_s);

	int_en = MSDC_READ32(MSDC_INTEN);
	MSDC_WRITE32(MSDC_INTEN, 0);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_EN, dvfs_en);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_HW, dvfs_hw);
	MSDC_GET_FIELD(SDC_CFG, SDC_CFG_DTOC, dtoc);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, 1);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_EN, 0);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_HW, 0);
	MSDC_SET_FIELD(SDC_CFG, SDC_CFG_DTOC, 3);

	/* store pre autok parameter */
	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		autok_adjust_param(host, i, &value, AUTOK_READ);
		autok_tune_res[i] = value;
	}

	ret = execute_online_tuning_sdio30_plus(host, res);
	if (ret != 0) {
		AUTOK_RAWPRINT("[AUTOK] ======Autok Failed======\r\n");
		AUTOK_RAWPRINT("[AUTOK] ======restore pre paras======\r\n");
		/* restore pre autok parameter */
		for (i = 0; i < TUNING_PARAM_COUNT; i++) {
			value = (u8) autok_tune_res[i];
			autok_adjust_param(host, i, &value, AUTOK_WRITE);
		}
	}
#if AUTOK_SDIO_OFFLINE_TUNE_TX_ENABLE
	autok_offline_tuning_TX(host, res);
#endif
#if AUTOK_OFFLINE_TUNE_DEVICE_RX_ENABLE
	autok_offline_tuning_device_RX(host, res);
#endif

	autok_msdc_reset();
	msdc_clear_fifo();
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_EN, dvfs_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_HW, dvfs_hw);
	MSDC_SET_FIELD(SDC_CFG, SDC_CFG_DTOC, dtoc);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000
		+ (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK]======Cost:%d ms======\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(autok_sdio30_plus_tuning);

int autok_execute_tuning(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	void __iomem *base = host->base;
	u8 autok_tune_res[TUNING_PARAM_COUNT];
	unsigned int i = 0;
	unsigned int value = 0;
	unsigned int dvfs_en = 0;
	unsigned int dvfs_hw = 0;
	unsigned int dtoc = 0;

	do_gettimeofday(&tm_s);

	int_en = MSDC_READ32(MSDC_INTEN);
	MSDC_WRITE32(MSDC_INTEN, 0);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_EN, dvfs_en);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_HW, dvfs_hw);
	MSDC_GET_FIELD(SDC_CFG, SDC_CFG_DTOC, dtoc);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, 1);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_EN, 0);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_HW, 0);
	MSDC_SET_FIELD(SDC_CFG, SDC_CFG_DTOC, 3);

	/* store pre autok parameter */
	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		autok_adjust_param(host, i, &value, AUTOK_READ);
		autok_tune_res[i] = value;
	}

	ret = execute_online_tuning(host, res);
	if (ret != 0) {
		AUTOK_RAWPRINT("[AUTOK] ======Autok Failed======\r\n");
		AUTOK_RAWPRINT("[AUTOK] ======restore pre paras======\r\n");
		/* restore pre autok parameter */
		for (i = 0; i < TUNING_PARAM_COUNT; i++) {
			value = (u8) autok_tune_res[i];
			autok_adjust_param(host, i, &value, AUTOK_WRITE);
		}
	}
#if AUTOK_SD_CARD_OFFLINE_TUNE_TX_ENABLE
	if (host->hw->host_function == MSDC_SD)
		autok_offline_tuning_clk_TX(host, MMC_WRITE_BLOCK);
#endif
	autok_msdc_reset();
	msdc_clear_fifo();
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_EN, dvfs_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_HW, dvfs_hw);
	MSDC_SET_FIELD(SDC_CFG, SDC_CFG_DTOC, dtoc);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000
		+ (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK]======Cost:%d ms======\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(autok_execute_tuning);

int hs400_execute_tuning(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	void __iomem *base = host->base;
	u8 autok_tune_res[TUNING_PARAM_COUNT];
	unsigned int i = 0;
	unsigned int value = 0;

	do_gettimeofday(&tm_s);
	int_en = MSDC_READ32(MSDC_INTEN);
	MSDC_WRITE32(MSDC_INTEN, 0);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, 1);

	/* store pre autok parameter */
	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		autok_adjust_param(host, i, &value, AUTOK_READ);
		autok_tune_res[i] = value;
	}

	ret = execute_online_tuning_hs400(host, res);
	if (ret != 0) {
		AUTOK_RAWPRINT("[AUTOK] ======HS400 Failed======\r\n");
		AUTOK_RAWPRINT("[AUTOK] ======restore pre paras======\r\n");
		/* restore pre autok parameter */
		for (i = 0; i < TUNING_PARAM_COUNT; i++) {
			value = (u8) autok_tune_res[i];
			autok_adjust_param(host, i, &value, AUTOK_WRITE);
		}
	}
#if AUTOK_EMMC_OFFLINE_TUNE_TX_ENABLE
	autok_offline_tuning_TX(host, res);
#endif

	autok_msdc_reset();
	msdc_clear_fifo();
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000
		+ (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK][HS400]======Cost:%d ms======\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(hs400_execute_tuning);

int hs400_execute_tuning_cmd(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	void __iomem *base = host->base;

	do_gettimeofday(&tm_s);
	int_en = MSDC_READ32(MSDC_INTEN);
	MSDC_WRITE32(MSDC_INTEN, 0);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, 1);

	autok_init_hs400(host);
	ret = execute_cmd_online_tuning(host, res);
	if (ret != 0)
		AUTOK_RAWPRINT("[AUTOK cmd] ======HS400 Failed======\r\n");

	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000
		+ (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK][HS400 cmd]======Cost:%d ms======\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(hs400_execute_tuning_cmd);

int hs200_execute_tuning(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	void __iomem *base = host->base;
	u8 autok_tune_res[TUNING_PARAM_COUNT];
	unsigned int i = 0;
	unsigned int value = 0;
	unsigned int dtoc = 0;
	struct AUTOK_PLAT_FUNC platform_para_func;
	unsigned int ckgen;

	memset(&platform_para_func, 0, sizeof(struct AUTOK_PLAT_FUNC));
	get_platform_func(platform_para_func);
	do_gettimeofday(&tm_s);
	int_en = MSDC_READ32(MSDC_INTEN);
	MSDC_WRITE32(MSDC_INTEN, 0);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_GET_FIELD(SDC_CFG, SDC_CFG_DTOC, dtoc);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, 1);
	MSDC_SET_FIELD(SDC_CFG, SDC_CFG_DTOC, 3);

	/* store pre autok parameter */
	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		autok_adjust_param(host, i, &value, AUTOK_READ);
		autok_tune_res[i] = value;
	}

	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	if (platform_para_func.latch_enhance == 1) {
		ckgen = 0;
		autok_write_param(host, CKGEN_MSDC_DLY_SEL, ckgen);
	}
	ret = execute_online_tuning_hs200(host, res);
	if (platform_para_func.latch_enhance == 1) {
		if (ret == -2) {
			ckgen += 1;
			autok_write_param(host, CKGEN_MSDC_DLY_SEL, ckgen);
			ret = execute_online_tuning_hs200(host, res);
		}
	}
	if (ret != 0) {
		AUTOK_RAWPRINT("[AUTOK] ======Autok HS200 Failed======\r\n");
		AUTOK_RAWPRINT("[AUTOK]======restore pre paras======\r\n");
		/* restore pre autok parameter */
		for (i = 0; i < TUNING_PARAM_COUNT; i++) {
			value = (u8) autok_tune_res[i];
			autok_adjust_param(host, i, &value, AUTOK_WRITE);
		}
	}

	autok_msdc_reset();
	msdc_clear_fifo();
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_SET_FIELD(SDC_CFG, SDC_CFG_DTOC, dtoc);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000
		+ (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK][HS200]======Cost:%d ms======\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(hs200_execute_tuning);

int hs200_execute_tuning_cmd(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	void __iomem *base = host->base;

	do_gettimeofday(&tm_s);
	int_en = MSDC_READ32(MSDC_INTEN);
	MSDC_WRITE32(MSDC_INTEN, 0);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, 1);

	autok_init_hs200(host);
	ret = execute_cmd_online_tuning(host, res);
	if (ret != 0)
		AUTOK_RAWPRINT("[AUTOK cmd] ======HS200 Failed======\r\n");

	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000
		+ (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK][HS200 cmd]======Cost:%d ms======\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(hs200_execute_tuning_cmd);

int autok_vcore_merge_sel(struct msdc_host *host, unsigned int merge_cap)
{
	void __iomem *base = host->base;
	unsigned int ret = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int uCmdEdge = 0;
	unsigned int uDatEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int RawData = 0;
	unsigned int j, k;
	struct AUTOK_REF_INFO_NEW *pInfo;
	unsigned int max_win[2];
	unsigned int dly_sel[2];
	char tune_result_str64[65];
	char tune_result_str[33];
	unsigned int score = 0;
	unsigned int data_dly = 0;
	unsigned int clk_mode = 0;

	do_gettimeofday(&tm_s);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, clk_mode);

	pInfo = kmalloc(sizeof(struct AUTOK_REF_INFO_NEW), GFP_ATOMIC);
	if (!pInfo) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] mem alloc fail\r\n");
		return -1;
	}
	/* Init set window 0xFF as infinite */
	for (j = CMD_MAX_WIN; j <= H_CLK_TX_MAX_WIN; j++)
		host->autok_res[AUTOK_VCORE_MERGE][j] = 0xFF;

	/* Step1 :  Cmd Path */
	if (!(merge_cap & MERGE_CMD))
		goto data_merge;
	memset(pInfo, 0, sizeof(struct AUTOK_REF_INFO_NEW));

	uCmdEdge = 0;
	do {
		RawData64 = 0LL;
		for (j = 0; j < (AUTOK_VCORE_NUM); j++) {
			for (k = 0; k < 8; k++) {
				if (uCmdEdge)
					RawData64 |=
				    (((u64)host->autok_res[j][CMD_SCAN_F0 + k])
				    << (8 * k));
				else
					RawData64 |=
				    (((u64)host->autok_res[j][CMD_SCAN_R0 + k])
				    << (8 * k));
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			"[AUTOK]CMD %d \t %d \t %s merge\r\n",
			uCmdEdge, score, tune_result_str64);
		if (autok_check_scan_res64_new(RawData64,
			&pInfo->scan_info[uCmdEdge], 0) != 0)
			goto fail;
		max_win[uCmdEdge] =
			autok_ds_dly_sel(&pInfo->scan_info[uCmdEdge],
			&dly_sel[uCmdEdge]);
		if (uCmdEdge)
			autok_window_apply(CMD_FALL, RawData64,
			    host->autok_res[AUTOK_VCORE_MERGE]);
		else
			autok_window_apply(CMD_RISE, RawData64,
			    host->autok_res[AUTOK_VCORE_MERGE]);
		uCmdEdge ^= 0x1;
	} while (uCmdEdge);
	if (max_win[0] >= max_win[1]) {
		pInfo->opt_edge_sel = 0;
		pInfo->opt_dly_cnt = dly_sel[0];
	} else {
		pInfo->opt_edge_sel = 1;
		pInfo->opt_dly_cnt = dly_sel[1];
	}
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
		"[AUTOK]cmd edge = %d cmd dly = %d max win = %d\r\n",
		pInfo->opt_edge_sel,
		pInfo->opt_dly_cnt,
		max_win[pInfo->opt_edge_sel]);
	host->autok_res[AUTOK_VCORE_MERGE][CMD_MAX_WIN] =
		max_win[pInfo->opt_edge_sel];
	autok_param_update(CMD_EDGE, pInfo->opt_edge_sel,
		host->autok_res[AUTOK_VCORE_MERGE]);
	autok_paddly_update(CMD_PAD_RDLY, pInfo->opt_dly_cnt,
		host->autok_res[AUTOK_VCORE_MERGE]);
	autok_adjust_param(host, CMD_EDGE,
		&pInfo->opt_edge_sel, AUTOK_WRITE);
	autok_adjust_paddly(host, &pInfo->opt_dly_cnt, CMD_PAD_RDLY);
	/* Step2 :  Dat Path */
data_merge:
	if (clk_mode == 3) {
		data_dly = 0;
		for (j = 0; j < AUTOK_VCORE_NUM; j++)
			data_dly += host->autok_res[j][DAT_RD_D_DLY1];
		data_dly = data_dly / AUTOK_VCORE_NUM;
		autok_paddly_update(DAT_PAD_RDLY, data_dly,
			host->autok_res[AUTOK_VCORE_MERGE]);
		autok_adjust_paddly(host, &data_dly, DAT_PAD_RDLY);
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			"[AUTOK]dat dly = %d\r\n", data_dly);
		goto ds_merge;
	}
	if (!(merge_cap & MERGE_DAT))
		goto ds_merge;
	memset(pInfo, 0, sizeof(struct AUTOK_REF_INFO_NEW));
	uDatEdge = 0;
	do {
		RawData64 = 0LL;
			for (j = 0; j < AUTOK_VCORE_NUM; j++) {
				for (k = 0; k < 8; k++) {
					if (uDatEdge)
						RawData64 |=
				(((u64)host->autok_res[j][DAT_SCAN_F0 + k])
					    << (8 * k));
					else
						RawData64 |=
				(((u64)host->autok_res[j][DAT_SCAN_R0 + k])
					<< (8 * k));
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			"[AUTOK]DAT %d \t %d \t %s merge\r\n",
		    uDatEdge, score, tune_result_str64);
		if (autok_check_scan_res64_new(RawData64,
			&pInfo->scan_info[uDatEdge], 0) != 0)
			goto fail;
		max_win[uDatEdge] =
			autok_ds_dly_sel(&pInfo->scan_info[uDatEdge],
			&dly_sel[uDatEdge]);
		if (uDatEdge)
			autok_window_apply(DAT_FALL, RawData64,
			    host->autok_res[AUTOK_VCORE_MERGE]);
			else
				autok_window_apply(DAT_RISE, RawData64,
				    host->autok_res[AUTOK_VCORE_MERGE]);
		uDatEdge ^= 0x1;
	} while (uDatEdge);
	if (max_win[0] >= max_win[1]) {
		pInfo->opt_edge_sel = 0;
		pInfo->opt_dly_cnt = dly_sel[0];
	} else {
		pInfo->opt_edge_sel = 1;
		pInfo->opt_dly_cnt = dly_sel[1];
	}
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
		"[AUTOK]dat edge = %d dat dly = %d max win = %d\r\n",
	    pInfo->opt_edge_sel, pInfo->opt_dly_cnt,
	    max_win[pInfo->opt_edge_sel]);
	host->autok_res[AUTOK_VCORE_MERGE][DAT_MAX_WIN]
		= max_win[pInfo->opt_edge_sel];
	autok_param_update(RD_FIFO_EDGE, pInfo->opt_edge_sel,
	    host->autok_res[AUTOK_VCORE_MERGE]);
	autok_paddly_update(DAT_PAD_RDLY, pInfo->opt_dly_cnt,
	    host->autok_res[AUTOK_VCORE_MERGE]);
	autok_param_update(WD_FIFO_EDGE, pInfo->opt_edge_sel,
	    host->autok_res[AUTOK_VCORE_MERGE]);
	autok_adjust_param(host, RD_FIFO_EDGE,
	    &pInfo->opt_edge_sel, AUTOK_WRITE);
	autok_adjust_param(host, WD_FIFO_EDGE,
	    &pInfo->opt_edge_sel, AUTOK_WRITE);
	autok_adjust_paddly(host,
		&pInfo->opt_dly_cnt, DAT_PAD_RDLY);
	/* Step3 :  DS Path */
ds_merge:
	if (!(merge_cap & MERGE_DS_DAT))
		goto device_data_rx_merge;

	host->autok_res[AUTOK_VCORE_MERGE][EMMC50_DS_ZDLY_DLY] =
		host->autok_res[AUTOK_VCORE_LEVEL0][EMMC50_DS_ZDLY_DLY];

	RawData64 = 0LL;
	memset(pInfo, 0, sizeof(struct AUTOK_REF_INFO_NEW));
	for (j = 0; j < AUTOK_VCORE_NUM; j++) {
		for (k = 0; k < 8; k++)
			RawData64 |=
				(((u64)host->autok_res[j][DS_DAT_SCAN_0 + k])
				<< (8 * k));
	}
	score = autok_simple_score64(tune_result_str64, RawData64);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DLY1/2 \t %d \t %s merge\r\n",
		score, tune_result_str64);

	autok_window_apply(DS_DATA_WIN, RawData64,
		host->autok_res[AUTOK_VCORE_MERGE]);
	if (autok_check_scan_res64_new(RawData64,
		&pInfo->scan_info[0], 0) != 0)
		goto fail;

	max_win[0] = autok_ds_dly_sel(&pInfo->scan_info[0], &data_dly);
	host->autok_res[AUTOK_VCORE_MERGE][DS_MAX_WIN] = max_win[0];
	autok_paddly_update(DS_PAD_RDLY, data_dly,
		host->autok_res[AUTOK_VCORE_MERGE]);
	autok_adjust_paddly(host, &data_dly, DS_PAD_RDLY);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DS dly = %d\r\n", data_dly);
	/* Step4 :  Device Dat RX */
device_data_rx_merge:
	if (!(merge_cap & MERGE_DEVICE_D_RX))
		goto host_data_tx_merge;
	RawData64 = 0LL;
	RawData = 0;
	memset(pInfo, 0, sizeof(struct AUTOK_REF_INFO_NEW));
	for (j = 0; j < AUTOK_VCORE_NUM; j++) {
		for (k = 0; k < 4; k++)
			RawData |= ((host->autok_res[j][D_DATA_SCAN_0 + k])
				<< (8 * k));
	}
	score = autok_simple_score(tune_result_str, RawData);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]dev DAT RX \t %d \t %s merge\r\n",
		score, tune_result_str);
	autok_window_apply(D_DATA_RX, RawData,
		host->autok_res[AUTOK_VCORE_MERGE]);
	RawData64 = ((u64)RawData) | 0xffffffff00000000;
	if (autok_check_scan_res64_new(RawData64,
		&pInfo->scan_info[0], 0) != 0)
		goto fail;

	max_win[0] = autok_ds_dly_sel(&pInfo->scan_info[0], &data_dly);
	host->autok_res[AUTOK_VCORE_MERGE][DEV_D_RX_MAX_WIN] = max_win[0];
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]dev DAT RX dly = %d\r\n",
		data_dly);
	/* Step5 :  Dat TX */
host_data_tx_merge:
	if (!(merge_cap & MERGE_HOST_D_TX))
		goto end;
	RawData64 = 0LL;
	RawData = 0;
	memset(pInfo, 0, sizeof(struct AUTOK_REF_INFO_NEW));
	for (j = 0; j < AUTOK_VCORE_NUM; j++) {
		for (k = 0; k < 4; k++)
			RawData |= ((host->autok_res[j][H_DATA_SCAN_0 + k])
				<< (8 * k));
	}
	score = autok_simple_score(tune_result_str, RawData);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DAT TX \t %d \t %s merge\r\n",
		score, tune_result_str);
	autok_window_apply(H_DATA_TX, RawData,
		host->autok_res[AUTOK_VCORE_MERGE]);
	RawData64 = ((u64)RawData) | 0xffffffff00000000;
	if (autok_check_scan_res64_new(RawData64,
		&pInfo->scan_info[0], 0) != 0)
		goto fail;

	max_win[0] = autok_ds_dly_sel(&pInfo->scan_info[0], &data_dly);
	host->autok_res[AUTOK_VCORE_MERGE][H_D_TX_MAX_WIN] = max_win[0];
	autok_param_update(EMMC50_DATA0_TX_DLY, data_dly,
		host->autok_res[AUTOK_VCORE_MERGE]);
	autok_param_update(EMMC50_DATA1_TX_DLY, data_dly,
		host->autok_res[AUTOK_VCORE_MERGE]);
	autok_param_update(EMMC50_DATA2_TX_DLY, data_dly,
		host->autok_res[AUTOK_VCORE_MERGE]);
	autok_param_update(EMMC50_DATA3_TX_DLY, data_dly,
		host->autok_res[AUTOK_VCORE_MERGE]);
	if (host->hw->host_function == MSDC_EMMC) {
		autok_param_update(EMMC50_DATA4_TX_DLY, data_dly,
			host->autok_res[AUTOK_VCORE_MERGE]);
		autok_param_update(EMMC50_DATA5_TX_DLY, data_dly,
			host->autok_res[AUTOK_VCORE_MERGE]);
		autok_param_update(EMMC50_DATA6_TX_DLY, data_dly,
			host->autok_res[AUTOK_VCORE_MERGE]);
		autok_param_update(EMMC50_DATA7_TX_DLY, data_dly,
			host->autok_res[AUTOK_VCORE_MERGE]);
	}
	autok_adjust_param(host, EMMC50_DATA0_TX_DLY,
		&data_dly, AUTOK_WRITE);
	autok_adjust_param(host, EMMC50_DATA1_TX_DLY,
		&data_dly, AUTOK_WRITE);
	autok_adjust_param(host, EMMC50_DATA2_TX_DLY,
		&data_dly, AUTOK_WRITE);
	autok_adjust_param(host, EMMC50_DATA3_TX_DLY,
		&data_dly, AUTOK_WRITE);
	if (host->hw->host_function == MSDC_EMMC) {
		autok_adjust_param(host, EMMC50_DATA4_TX_DLY,
			&data_dly, AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DATA5_TX_DLY,
			&data_dly, AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DATA6_TX_DLY,
			&data_dly, AUTOK_WRITE);
		autok_adjust_param(host, EMMC50_DATA7_TX_DLY,
			&data_dly, AUTOK_WRITE);
	}
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]dat tx = %d\r\n", data_dly);

end:
	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000
		+ (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK][merge]======Time Cost:%d ms======\r\n", tm_val);

	kfree(pInfo);
	return ret;
fail:
	kfree(pInfo);
	return -1;
}
EXPORT_SYMBOL(autok_vcore_merge_sel);

