// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk-mmc-autok.h"
#include "mtk-mmc.h"

#define MSDC_CLKTXDLY		0
#define MSDC_PB0_DEFAULT_VAL		0x403C0007
#define MSDC_PB1_DEFAULT_VAL		0xFFE64309
#define MSDC_PB2_DEFAULT_RESPWAIT	0x3
#define MSDC_PB2_DEFAULT_RESPSTSENSEL	0x1
#define MSDC_PB2_DEFAULT_CRCSTSENSEL	0x1

/* After merge still have over 10 OOOOOOOOOOO window */
#define AUTOK_MERGE_MIN_WIN		10
/* 100ms */
#define AUTOK_CMD_TIMEOUT		(HZ / 10)
/* 1s x 3 */
#define AUTOK_DAT_TIMEOUT		(HZ * 3)
#define MSDC_FIFO_THD_1K		(1024)
#define TUNE_TX_CNT			(10)
#define CHECK_QSR			(0x800D)
#define TUNE_DATA_TX_ADDR		(0x358000)
#define CMDQ
#define AUTOK_CMD_TIMES			(20)
/* scan result may find xxxxooxxx */
#define AUTOK_TUNING_INACCURACY		(10)
#define AUTOK_MARGIN_THOLD		(5)
#define AUTOK_BD_WIDTH_REF		(3)
#define AUTOK_READ			0
#define AUTOK_WRITE			1
#define AUTOK_FINAL_CKGEN_SEL		(0)
#define SCALE_TA_CNTR			(8)
#define SCALE_CMD_RSP_TA_CNTR		(8)
#define SCALE_WDAT_CRC_TA_CNTR		(8)
#define SCALE_INT_DAT_LATCH_CK_SEL	(8)
#define SCALE_INTERNAL_DLY_CNTR		(32)
#define SCALE_PAD_DAT_DLY_CNTR		(32)
#define TUNING_INACCURACY		(2)

enum TUNE_TYPE {
	TUNE_CMD = 0,
	TUNE_DATA,
	TUNE_LATCH_CK,
	TUNE_SDIO_PLUS,
};

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
		sdr_set_bits(host->base + MSDC_FIFOCS, MSDC_FIFOCS_CLR); \
		while (readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_CLR) \
			cpu_relax(); \
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

static void sdr_set_bits(void __iomem *reg, u32 bs)
{
	u32 val = readl(reg);

	val |= bs;
	writel(val, reg);
}

static void sdr_clr_bits(void __iomem *reg, u32 bs)
{
	u32 val = readl(reg);

	val &= ~bs;
	writel(val, reg);
}

static void sdr_set_field(void __iomem *reg, u32 field, u32 val)
{
	unsigned int tv = readl(reg);

	tv &= ~field;
	tv |= ((val) << (ffs((unsigned int)field) - 1));
	writel(tv, reg);
}

static void sdr_get_field(void __iomem *reg, u32 field, u32 *val)
{
	unsigned int tv = readl(reg);

	*val = ((tv & field) >> (ffs((unsigned int)field) - 1));
}

static void msdc_reset_hw(struct msdc_host *host)
{
	u32 val;

	sdr_set_bits(host->base + MSDC_CFG, MSDC_CFG_RST);
	while (readl(host->base + MSDC_CFG) & MSDC_CFG_RST)
		cpu_relax();

	sdr_set_bits(host->base + MSDC_FIFOCS, MSDC_FIFOCS_CLR);
	while (readl(host->base + MSDC_FIFOCS) & MSDC_FIFOCS_CLR)
		cpu_relax();

	val = readl(host->base + MSDC_INT);
	writel(val, host->base + MSDC_INT);
}

/**********************************************************
 * AutoK Basic Interface Implenment                       *
 **********************************************************/

/* define the function to shrink code's column */
static void rx_read(struct msdc_host *host, unsigned int value)
{
	int i = 0;

	for (i = 0; i < (MSDC_FIFO_SZ - 64)/4; i++)
		value = readl(host->base + MSDC_RXDATA);
}

static int autok_send_tune_cmd(struct msdc_host *host, unsigned int opcode,
	enum TUNE_TYPE tune_type_value, struct autok_host *host_para)
{
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
	u32 bus_width = 0;
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	switch (opcode) {
	case MMC_SEND_EXT_CSD:
		rawcmd =  (512 << 16) | (0 << 13)
			| (1 << 11) | (1 << 7) | (8);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			writel(host->tune_latch_ck_cnt, host->base + SDC_BLK_NUM);
		else
			writel(1, host->base + SDC_BLK_NUM);
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
		writel(1, host->base + SDC_BLK_NUM);
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
			writel(host->tune_latch_ck_cnt, host->base + SDC_BLK_NUM);
		else
			writel(1, host->base + SDC_BLK_NUM);
		break;
	case MMC_EXECUTE_WRITE_TASK:
		rawcmd = (512 << 16) | (1 << 13)
			| (1 << 11) | (1 << 7) | (47);
		arg = (0 << 16);
		writel(1, host->base + SDC_BLK_NUM);
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
			writel(host->tune_latch_ck_cnt, host->base + SDC_BLK_NUM);
		else
			writel(1, host->base + SDC_BLK_NUM);
		break;
	case MMC_SEND_TUNING_BLOCK:
		left = 64;
		rawcmd = (64 << 16) | (0 << 13)
			| (1 << 11) | (1 << 7) | (19);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			writel(host->tune_latch_ck_cnt, host->base + SDC_BLK_NUM);
		else
			writel(1, host->base + SDC_BLK_NUM);
		break;
	case MMC_SEND_TUNING_BLOCK_HS200:
		sdr_get_field(host->base + SDC_CFG, SDC_CFG_BUSWIDTH, &bus_width);
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
			writel(host->tune_latch_ck_cnt, host->base + SDC_BLK_NUM);
		else
			writel(1, host->base + SDC_BLK_NUM);
		break;
	case MMC_WRITE_BLOCK:
		/* get clk tx dly, for SD card tune clk tx */
		sdr_get_field(host->base + tune_reg,
			MSDC_PAD_TUNE_CLKTDLY,
			&clk_tx_pre);
		rawcmd = (512 << 16) | (1 << 13)
			| (1 << 11) | (1 << 7) | (24);
		arg = TUNE_DATA_TX_ADDR;
		writel(1, host->base + SDC_BLK_NUM);
		break;
	case SD_IO_RW_DIRECT:
		rawcmd = (1 << 7) | (52);
		arg = (0x80000000) | (1 << 28)
			| (SDIO_CCCR_ABORT << 9) | (0);
		writel(1, host->base + SDC_BLK_NUM);
		break;
	case SD_IO_RW_EXTENDED:
		rawcmd = (4 << 16) | (1 << 13) | (1 << 11) | (1 << 7) | (53);
		arg = (0x80000000) | (1 << 28)
			| (0xB0 << 9) | (0 << 26) | (0 << 27) | (4);
		writel(1, host->base + SDC_BLK_NUM);
		break;
	}

	tmo = AUTOK_DAT_TIMEOUT;
	wait_cond_tmo(!(readl(host->base + SDC_STS) & SDC_STS_SDCBUSY), tmo);
	if (tmo == 0) {
		AUTOK_RAWPRINT("[AUTOK]MSDC busy tmo1 cmd%d\n", opcode);
		ret |= E_RES_FATAL_ERR;
		goto end;
	}

	if ((tune_type_value == TUNE_CMD) || (tune_type_value == TUNE_DATA)
		|| (tune_type_value == TUNE_SDIO_PLUS)) {
		if ((tune_type_value == TUNE_CMD)
			&& (host->id == MSDC_EMMC))
			writel(MSDC_INT_CMDTMO
				| MSDC_INT_CMDRDY
				| MSDC_INT_RSPCRCERR,
				host->base + MSDC_INT);
		else {
			msdc_reset_hw(host);
		}
	}

	/* start command */
	writel(arg, host->base + SDC_ARG);
	writel(rawcmd, host->base + SDC_CMD);

	/* wait interrupt status */
	wints = MSDC_INT_CMDTMO | MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR;
	tmo = AUTOK_CMD_TIMEOUT;
	wait_cond_tmo(((sts = readl(host->base + MSDC_INT)) & wints), tmo);
	if (tmo == 0) {
		AUTOK_RAWPRINT("[AUTOK]CMD%d wait int tmo\r\n", opcode);
		ret |= E_RES_FATAL_ERR;
		goto end;
	}

	writel((sts & wints), host->base + MSDC_INT);
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
	while ((readl(host->base + SDC_STS) & SDC_STS_SDCBUSY) && (tmo != 0)) {
		if (time_after(jiffies, tmo))
			tmo = 0;
		if (tune_type_value == TUNE_LATCH_CK) {
			fifo_have = readl(host->base + MSDC_FIFOCS) &
				MSDC_FIFOCS_RXCNT;
			if ((opcode == MMC_SEND_TUNING_BLOCK_HS200)
				|| (opcode == MMC_READ_SINGLE_BLOCK)
				|| (opcode == MMC_SEND_EXT_CSD)
				|| (opcode == MMC_SEND_TUNING_BLOCK)
				|| (opcode == MMC_EXECUTE_READ_TASK)) {
				sdr_set_field(host->base + MSDC_DBG_SEL,
					0xffff << 0, 0x0b);
				sdr_get_field(host->base + MSDC_DBG_OUT,
					0x7ff << 0, &fifo_1k_cnt);
				if ((fifo_1k_cnt >= MSDC_FIFO_THD_1K) &&
					(fifo_have >= MSDC_FIFO_SZ) &&
					(host_para->fifo_tune == 1)) {
					value = readl(host->base + MSDC_RXDATA);
					value = readl(host->base + MSDC_RXDATA);
					value = readl(host->base + MSDC_RXDATA);
					value = readl(host->base + MSDC_RXDATA);
				} else if ((fifo_have >= MSDC_FIFO_SZ) &&
					(host_para->fifo_tune == 0)) {
					value = readl(host->base + MSDC_RXDATA);
					value = readl(host->base + MSDC_RXDATA);
					value = readl(host->base + MSDC_RXDATA);
					value = readl(host->base + MSDC_RXDATA);
				}
			}
		} else if ((tune_type_value == TUNE_DATA)
		&& ((opcode == MMC_WRITE_BLOCK)
		|| (opcode == MMC_EXECUTE_WRITE_TASK)
		|| (opcode == MMC_WRITE_MULTIPLE_BLOCK))) {
			if (host->id == MSDC_SD || host->id == MSDC_SDIO) {
				sdr_set_field(host->base + tune_reg,
					MSDC_PAD_TUNE_CLKTDLY,
					host_para->clk_tx);
				for (i = 0; i < 63; i++) {
					writel(0xa5a5a5a5, host->base + MSDC_TXDATA);
					writel(0x1c345678, host->base + MSDC_TXDATA);
				}
				/* restore clk tx brefore half data transfer */
				sdr_set_field(host->base + tune_reg,
					MSDC_PAD_TUNE_CLKTDLY,
					clk_tx_pre);
				writel(0xa5a5a5a5, host->base + MSDC_TXDATA);
				writel(0x1c345678, host->base + MSDC_TXDATA);
			} else {
				for (i = 0; i < 64; i++) {
					writel(0xf0f0f0f0, host->base + MSDC_TXDATA);
					writel(0x0f0f0f0f, host->base + MSDC_TXDATA);
				}
			}
			write_tmo = AUTOK_DAT_TIMEOUT;
			wait_cond_tmo(
				!(readl(host->base + SDC_STS) & SDC_STS_SDCBUSY),
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
					fifo_have = readl(host->base + MSDC_FIFOCS) &
						MSDC_FIFOCS_RXCNT;
					sdr_set_field(host->base + MSDC_DBG_SEL,
						0xffff << 0, 0x0b);
					sdr_get_field(host->base + MSDC_DBG_OUT,
						0x7ff << 0, &fifo_1k_cnt);

					if ((fifo_1k_cnt >= MSDC_FIFO_THD_1K)
					&& (fifo_have > MSDC_FIFO_SZ - 64))
						rx_read(host, value);
				}
			}
		} else if ((tune_type_value == TUNE_SDIO_PLUS)
		&& (opcode == SD_IO_RW_EXTENDED)) {
			writel(0x5a5a5a5a, host->base + MSDC_TXDATA);

			write_tmo = AUTOK_DAT_TIMEOUT;
			wait_cond_tmo(
				!(readl(host->base + SDC_STS) & SDC_STS_SDCBUSY),
				write_tmo);
			if (write_tmo == 0) {
				AUTOK_RAWPRINT("[AUTOK]MSDC busy tmo2 cmd%d\n",
				    opcode);
				ret |= E_RES_FATAL_ERR;
				goto end;
			}
		}
	}
	if ((tmo == 0) && (readl(host->base + SDC_STS) & SDC_STS_SDCBUSY)) {
		AUTOK_RAWPRINT("[AUTOK]MSDC busy tmo3 cmd%d\n", opcode);
		ret |= E_RES_FATAL_ERR;
		goto end;
	}

	sts = readl(host->base + MSDC_INT);
	wints = MSDC_INT_XFER_COMPL | MSDC_INT_DATCRCERR | MSDC_INT_DATTMO;
	if (sts) {
		/* clear status */
		writel((sts & wints), host->base + MSDC_INT);
		if (sts & MSDC_INT_XFER_COMPL)
			ret |= E_RES_PASS;
		if (MSDC_INT_DATCRCERR & sts)
			ret |= E_RES_DAT_CRC;
		if (MSDC_INT_DATTMO & sts)
			ret |= E_RES_DAT_TMO;
	}

skip_tune_latch_ck_and_tune_data:
	tmo = AUTOK_DAT_TIMEOUT;
	wait_cond_tmo(!(readl(host->base + SDC_STS) & SDC_STS_SDCBUSY), tmo);
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
			((readl(host->base + MSDC_PS) & 0x10000) == 0x10000),
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
	unsigned int bit = 0, num = 0, old = 0;

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

	pBdInfo_R = &(pInfo->scan_info[0]);
	pBdInfo_F = &(pInfo->scan_info[1]);
	FBound_Cnt_R = pBdInfo_R->fbd_cnt;
	Bound_Cnt_R = pBdInfo_R->bd_cnt;
	Bound_Cnt_F = pBdInfo_F->bd_cnt;

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
			} else if ((cycle_cnt / 2 - uBD_mid_prev) >
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
			/* error can not find 2 full boundary */
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
				/* when rise edge have only one bd (not full bd)
				 * falling edge shouldn't more than one bd exist
				 * this is a corner case, rise bd may scan miss
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
					if (pBdNext->Bound_Start == 0) {
						/* Current Design Not Allowed
						 * this is a corner case,
						 * boundary may scan error.
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
					if (pBdNext->Bound_End == 63) {
						/* Current Design Not Allowed
						 * this is a corner case,
						 * boundary may scan error.
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
					uDlySel_R =
						pBdPrev->Bound_Start +
						uBD_width / 2 -
						cycle_cnt / 2;
					uMgLost_R = 0;
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
	u32 reg;
	u32 field = 0;
	int use_top_base = 0;
	struct AUTOK_PLAT_TOP_CTRL platform_top_ctrl;
	u32 tune_reg = host->dev_comp->pad_tune_reg;

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

		reg = (u32) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_DSPLSEL);
		break;
	case WRITE_DATA_SMPL_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) WRITE_DATA_SMPL_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}

		reg = (u32) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_W_DSMPL_SEL);
		break;
	case DATA_DLYLINE_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) DATA_DLYLINE_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC_TOP_CONTROL;
			field = (u32) (DATA_K_VALUE_SEL);
		} else {
			reg = (u32) MSDC_IOCON;
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
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC_TOP_CONTROL;
			field = (u32) (PAD_RXDLY_SEL);
		} else {
			reg = (u32) tune_reg;
			field = (u32) (MSDC_PAD_TUNE_RXDLYSEL);
		}
		break;
	case MSDC_WCRC_ASYNC_FIFO_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) WCRC_ASYNC_FIFO_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32) MSDC_PATCH_BIT2;
		field = (u32) (MSDC_PATCH_BIT2_CFGCRCSTS);
		break;
	case MSDC_RESP_ASYNC_FIFO_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) RESP_ASYNC_FIFO_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32) MSDC_PATCH_BIT2;
		field = (u32) (MSDC_PATCH_BIT2_CFGRESP);
		break;
	case CMD_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) CMD_EDGE out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_RSPL);
		break;
	case CMD_FIFO_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) CMD_FIFO_EDGE out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32) EMMC50_CFG0;
		field = (u32) (EMMC50_CFG_CMD_EDGE_SEL);
		break;
	case RDATA_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) RDATA_EDGE out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_DSPLSEL);
		break;
	case RD_FIFO_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) RD_FIFO_EDGE out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32) MSDC_PATCH_BIT;
		field = (u32) (MSDC_PATCH_BIT_RD_DAT_SEL);
		break;
	case WD_FIFO_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) WD_FIFO_EDGE out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32) MSDC_PATCH_BIT2;
		field = (u32) (MSDC_PB2_CFGCRCSTSEDGE);
		break;
	case CMD_RD_D_DLY1:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) CMD_RD_DLY out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC_TOP_CMD;
			field = (u32) (PAD_CMD_RXDLY);
		} else {
			reg = (u32) tune_reg;
			field = (u32) (MSDC_PAD_TUNE_CMDRDLY);
		}
		break;
	case CMD_RD_D_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) CMD_RD_DLY_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC_TOP_CMD;
			field = (u32) (PAD_CMD_RD_RXDLY_SEL);
		} else {
			reg = (u32) tune_reg;
			field = (u32) (MSDC_PAD_TUNE_CMDRRDLYSEL);
		}
		break;
	case CMD_RD_D_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) CMD_RD_DLY2 out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC_TOP_CMD;
			field = (u32) (PAD_CMD_RXDLY2);
		} else {
			reg = (u32) MSDC_PAD_TUNE1;
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
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC_TOP_CMD;
			field = (u32) (PAD_CMD_RD_RXDLY2_SEL);
		} else {
			reg = (u32) MSDC_PAD_TUNE1;
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
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC_TOP_CONTROL;
			field = (u32) (PAD_DAT_RD_RXDLY);
		} else {
			reg = (u32) tune_reg;
			field = (u32) (MSDC_PAD_TUNE_DATRRDLY);
		}
		break;
	case DAT_RD_D_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) DAT_RD_DLY_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC_TOP_CONTROL;
			field = (u32) (PAD_DAT_RD_RXDLY_SEL);
		} else {
			reg = (u32) tune_reg;
			field = (u32) (MSDC_PAD_TUNE_DATRRDLYSEL);
		}
		break;
	case DAT_RD_D_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) DAT_RD_DLY2 out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC_TOP_CONTROL;
			field = (u32) (PAD_DAT_RD_RXDLY2);
		} else {
			reg = (u32) MSDC_PAD_TUNE1;
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
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC_TOP_CONTROL;
			field = (u32) (PAD_DAT_RD_RXDLY2_SEL);
		} else {
			reg = (u32) MSDC_PAD_TUNE1;
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
		reg = (u32) MSDC_PATCH_BIT;
		field = (u32) (MSDC_INT_DAT_LATCH_CK_SEL);
		break;
	case CKGEN_MSDC_DLY_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) CKGEN_MSDC_DLY_SEL out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32) MSDC_PATCH_BIT;
		field = (u32) (MSDC_CKGEN_MSDC_DLY_SEL);
		break;
	case CMD_RSP_TA_CNTR:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug
			    ("[%s](%d) CMD_RSP_TA_CNTR out of range[0~7]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32) MSDC_PATCH_BIT1;
		field = (u32) (MSDC_PATCH_BIT1_CMDTA);
		break;
	case WRDAT_CRCS_TA_CNTR:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug
			    ("[%s](%d) WRDAT_CRCS_TA_CNTR out of range[0~7]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32) MSDC_PATCH_BIT1;
		field = (u32) (MSDC_PATCH_BIT1_WRDAT);
		break;
	case SDC_RX_ENHANCE:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug
			    ("[%s](%d) SDC_RX_ENHANCE out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (((host->id == MSDC_EMMC)
		&& (platform_top_ctrl.msdc0_rx_enhance_top == 1)
		&& host->top_base)
		|| ((host->id == MSDC_SD || host->id == MSDC_SDIO)
		&& (platform_top_ctrl.msdc1_rx_enhance_top == 1)
		&& host->top_base)) {
			use_top_base = 1;
			reg = (u32) EMMC_TOP_CONTROL;
			field = (u32) (AUTOK_TOP_SDC_RX_ENHANCE_EN);
		} else {
			reg = (u32) SDC_ADV_CFG0;
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
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC50_PAD_CTL0;
			field = (u32) (PAD_CLK_TXDLY);
		} else {
			reg = (u32) tune_reg;
			field = (u32) (MSDC_PAD_TUNE_CLKTDLY);
		}
		break;
	case EMMC50_WDATA_MUX_EN:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) EMMC50_WDATA_MUX_EN out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32) EMMC50_CFG0;
		field = (u32) (EMMC50_CFG_CFCSTS_SEL);
		break;
	case EMMC50_CMD_MUX_EN:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) EMMC50_CMD_MUX_EN out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32) EMMC50_CFG0;
		field = (u32) (EMMC50_CFG_CMD_RESP_SEL);
		break;
	case EMMC50_CMD_RESP_LATCH:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) EMMC50_CMD_LATCH out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32) EMMC50_CFG0;
		field = (u32) (EMMC50_CFG_PADCMD_LATCHCK);
		break;
	case EMMC50_WDATA_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) EMMC50_WDATA_EDGE out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32) EMMC50_CFG0;
		field = (u32) (EMMC50_CFG_CRCSTS_EDGE);
		break;
	case EMMC50_DS_Z_DLY1:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DS_Z_DLY1 out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY1);
		} else {
			reg = (u32) PAD_DS_TUNE;
			field = (u32) (PAD_DS_TUNE_DLY1);
		}
		break;
	case EMMC50_DS_Z_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) EMMC50_DS1_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY_SEL);
		} else {
			reg = (u32) PAD_DS_TUNE;
			field = (u32) (PAD_DS_TUNE_DLYSEL);
		}
		break;
	case EMMC50_DS_Z_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DS_Z_DLY2 out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY2);
		} else {
			reg = (u32) PAD_DS_TUNE;
			field = (u32) (PAD_DS_TUNE_DLYSEL);
		}
		break;
	case EMMC50_DS_Z_DLY2_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s](%d) EMMC50_DS2_SEL out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY2_SEL);
		} else {
			reg = (u32) PAD_DS_TUNE;
			field = (u32) (PAD_DS_TUNE_DLY2SEL);
		}
		break;
	case EMMC50_DS_ZDLY_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DS_Z_DLY3 out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC50_PAD_DS_TUNE;
			field = (u32) (PAD_DS_DLY3);
		} else {
			reg = (u32) PAD_DS_TUNE;
			field = (u32) (PAD_DS_TUNE_DLY3);
		}
		break;
	case EMMC50_CMD_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_CMD_TX_DLY out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC_TOP_CMD;
			field = (u32) (PAD_CMD_TX_DLY);
		} else {
			reg = (u32) PAD_CMD_TUNE;
			field = (u32) (MSDC_PAD_CMD_TUNE_TXDLY);
		}
		break;
	case EMMC50_DATA0_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA0_TX out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC50_PAD_DAT0_TUNE;
			field = (u32) (PAD_DAT0_TX_DLY);
		} else {
			reg = (u32) EMMC50_PAD_DAT01_TUNE;
			field = (u32) (MSDC_PAD_DAT0_TXDLY);
		}
		break;
	case EMMC50_DATA1_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA1_TX out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC50_PAD_DAT1_TUNE;
			field = (u32) (PAD_DAT1_TX_DLY);
		} else {
			reg = (u32) EMMC50_PAD_DAT01_TUNE;
			field = (u32) (MSDC_PAD_DAT1_TXDLY);
		}
		break;
	case EMMC50_DATA2_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA2_TX out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC50_PAD_DAT2_TUNE;
			field = (u32) (PAD_DAT2_TX_DLY);
		} else {
			reg = (u32) EMMC50_PAD_DAT23_TUNE;
			field = (u32) (MSDC_PAD_DAT2_TXDLY);
		}
		break;
	case EMMC50_DATA3_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA3_TX out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if (host->top_base) {
			use_top_base = 1;
			reg = (u32) EMMC50_PAD_DAT3_TUNE;
			field = (u32) (PAD_DAT3_TX_DLY);
		} else {
			reg = (u32) EMMC50_PAD_DAT23_TUNE;
			field = (u32) (MSDC_PAD_DAT3_TXDLY);
		}
		break;
	case EMMC50_DATA4_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA4_TX out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if ((host->id == MSDC_EMMC)
			&& (host->top_base)) {
			use_top_base = 1;
			reg = (u32) EMMC50_PAD_DAT4_TUNE;
			field = (u32) (PAD_DAT4_TX_DLY);
		} else {
			reg = (u32) EMMC50_PAD_DAT45_TUNE;
			field = (u32) (MSDC_PAD_DAT4_TXDLY);
		}
		break;
	case EMMC50_DATA5_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA5_TX out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if ((host->id == MSDC_EMMC)
			&& (host->top_base)) {
			use_top_base = 1;
			reg = (u32) EMMC50_PAD_DAT5_TUNE;
			field = (u32) (PAD_DAT5_TX_DLY);
		} else {
			reg = (u32) EMMC50_PAD_DAT45_TUNE;
			field = (u32) (MSDC_PAD_DAT5_TXDLY);
		}
		break;
	case EMMC50_DATA6_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA6_TX out of range[0~31]\n",
			     __func__, *value);
			return -1;
		}
		if ((host->id == MSDC_EMMC)
			&& (host->top_base)) {
			use_top_base = 1;
			reg = (u32) EMMC50_PAD_DAT6_TUNE;
			field = (u32) (PAD_DAT6_TX_DLY);
		} else {
			reg = (u32) EMMC50_PAD_DAT67_TUNE;
			field = (u32) (MSDC_PAD_DAT6_TXDLY);
		}
		break;
	case EMMC50_DATA7_TX_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s](%d) EMMC50_DATA7_TX out of range[0~1]\n",
			     __func__, *value);
			return -1;
		}
		if ((host->id == MSDC_EMMC)
			&& (host->top_base)) {
			use_top_base = 1;
			reg = (u32) EMMC50_PAD_DAT7_TUNE;
			field = (u32) (PAD_DAT7_TX_DLY);
		} else {
			reg = (u32) EMMC50_PAD_DAT67_TUNE;
			field = (u32) (MSDC_PAD_DAT7_TXDLY);
		}
		break;
	default:
		pr_debug
		    ("[%s] Value of [AUTOK_PARAM] is wrong\n", __func__);
		return -1;
	}

	if (rw == AUTOK_READ)
		if (use_top_base)
			sdr_get_field(host->top_base + reg, field, value);
		else
			sdr_get_field(host->base + reg, field, value);
	else if (rw == AUTOK_WRITE) {
		if (use_top_base)
			sdr_set_field(host->top_base + reg, field, *value);
		else
			sdr_set_field(host->base + reg, field, *value);
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
	if (host->id == MSDC_EMMC) {
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

void autok_tuning_parameter_init(struct msdc_host *host, u8 *res)
{
	autok_param_apply(host, res);
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
	struct AUTOK_PLAT_PARA_TX platform_para_tx;
	struct AUTOK_PLAT_PARA_RX platform_para_rx;
	struct AUTOK_PLAT_FUNC platform_para_func;
	struct AUTOK_PLAT_TOP_CTRL platform_top_ctrl;

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

	/* clk tune all data Line share dly */
	autok_write_param(host, DATA_DLYLINE_SEL, 0);

	/* data tune mode select */
	autok_write_param(host, MSDC_DAT_TUNE_SEL, 0);

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
	sdr_set_field(host->base + EMMC50_CFG1, EMMC50_CFG1_DS_CFG, 0);

	/* Common Setting Config */
	autok_write_param(host, CKGEN_MSDC_DLY_SEL,
		platform_para_rx.ckgen_val);
	autok_write_param(host, CMD_RSP_TA_CNTR,
		platform_para_rx.cmd_ta_val);
	autok_write_param(host, WRDAT_CRCS_TA_CNTR,
		platform_para_rx.crc_ta_val);

	sdr_set_field(host->base + MSDC_PATCH_BIT1, MSDC_PB1_GET_BUSY_MA,
		platform_para_rx.busy_ma_val);
	/* DDR50 byte swap issue design fix feature enable */
	if (platform_para_func.ddr50_fix == 1)
		sdr_set_field(host->base + MSDC_PATCH_BIT2, 1 << 19, 1);
	/* multi sync circuit design improve */
	if (platform_para_func.multi_sync == 1) {
		sdr_set_field(host->base + MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_STOP_DLY, 3);
		sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_POPENCNT, 8);
		sdr_set_field(host->base + MSDC_PATCH_BIT1, 0x3 << 19, 3);
		sdr_set_field(host->base + SDC_FIFO_CFG, SDC_FIFO_CFG_WRVALIDSEL, 0);
		sdr_set_field(host->base + SDC_FIFO_CFG, SDC_FIFO_CFG_RDVALIDSEL, 0);
	}

	/* duty bypass that may influence timing */
	if ((host->id == MSDC_EMMC)
		&& (platform_para_func.msdc0_bypass_duty_modify == 1)) {
		if (host->top_base) {
			sdr_set_field(host->top_base + EMMC50_PAD_CTL0, DCC_SEL,
				platform_para_tx.msdc0_duty_bypass);
			sdr_set_field(host->top_base + EMMC50_PAD_CTL0, HL_SEL,
				platform_para_tx.msdc0_hl_duty_sel);
		} else {
			sdr_set_field(host->base + MSDC_PAD_CTL0,
				MSDC_PAD_CTL0_DCCSEL,
				platform_para_tx.msdc0_duty_bypass);
			sdr_set_field(host->base + MSDC_PAD_CTL0,
				MSDC_PAD_CTL0_HLSEL,
				platform_para_tx.msdc0_hl_duty_sel);
		}
	}
	if ((host->id == MSDC_SD || host->id == MSDC_SDIO)
		&& (platform_para_func.msdc1_bypass_duty_modify == 1)) {
		if (host->top_base) {
			sdr_set_field(host->top_base + EMMC50_PAD_CTL0, DCC_SEL,
				platform_para_tx.msdc1_duty_bypass);
			sdr_set_field(host->top_base + EMMC50_PAD_CTL0, HL_SEL,
				platform_para_tx.msdc1_hl_duty_sel);
		}
	}
	return 0;
}
EXPORT_SYMBOL(autok_path_sel);

int autok_init_sdr104(struct msdc_host *host)
{
	struct AUTOK_PLAT_PARA_RX platform_para_rx;
	struct AUTOK_PLAT_FUNC platform_para_func;
	struct mmc_host *mmc = mmc_from_priv(host);
	
	memset(&platform_para_rx, 0, sizeof(struct AUTOK_PLAT_PARA_RX));
	memset(&platform_para_func, 0, sizeof(struct AUTOK_PLAT_FUNC));
	get_platform_para_rx(platform_para_rx);
	get_platform_func(platform_para_func);

	/* driver may miss data tune path setting in the interim */
	autok_path_sel(host);

	/* if any specific config need modify add here */
	if (platform_para_func.rx_enhance) {
		autok_write_param(host, SDC_RX_ENHANCE, 1);
		sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, 0);
		sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_RESPSTSENSEL, 0);
	} else {
		if (mmc->actual_clock <= 100000000) {
			/* LATCH_TA_EN Config for WCRC Path HS FS mode */
			sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL,
				platform_para_rx.latch_en_crc_hs);
			/* LATCH_TA_EN Config for CMD Path HS FS mode */
			sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_RESPSTSENSEL,
				platform_para_rx.latch_en_cmd_hs);
		} else if (host->id == MSDC_SD || host->id == MSDC_SDIO) {
			/* LATCH_TA_EN Config for WCRC Path SDR104 mode */
			sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL,
				platform_para_rx.latch_en_crc_sd_sdr104);
			/* LATCH_TA_EN Config for CMD Path SDR104 mode */
			sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_RESPSTSENSEL,
				platform_para_rx.latch_en_cmd_sd_sdr104);
		}
	}
	/* enable dvfs feature */
	/* if (host->id == MSDC_SDIO) */
	/*	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_DVFS_EN, 1); */
	if (platform_para_func.multi_sync == 0) {
		if (platform_para_func.new_path_sdr104 == 1) {
			sdr_set_field(host->base + MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_STOP_DLY,
			    platform_para_rx.new_stop_sdr104);
			sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_POPENCNT,
			    platform_para_rx.new_water_sdr104);
			sdr_set_field(host->base + SDC_FIFO_CFG,
				SDC_FIFO_CFG_WRVALIDSEL, 0);
			sdr_set_field(host->base + SDC_FIFO_CFG,
				SDC_FIFO_CFG_RDVALIDSEL, 0);
		} else if (platform_para_func.new_path_sdr104 == 0) {
			/* use default setting */
			sdr_set_field(host->base + MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_STOP_DLY,
			    platform_para_rx.old_stop_sdr104);
			sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_POPENCNT,
			    platform_para_rx.old_water_sdr104);
			sdr_set_field(host->base + SDC_FIFO_CFG,
				SDC_FIFO_CFG_WRVALIDSEL, 1);
			sdr_set_field(host->base + SDC_FIFO_CFG,
				SDC_FIFO_CFG_RDVALIDSEL, 1);
		}
	}

	return 0;
}
EXPORT_SYMBOL(autok_init_sdr104);

int autok_init_hs200(struct msdc_host *host)
{
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
		sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, 0);
		sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_RESPSTSENSEL, 0);
	} else {
		/* LATCH_TA_EN Config for WCRC Path non_HS400 */
		sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL,
		    platform_para_rx.latch_en_crc_hs200);
		/* LATCH_TA_EN Config for CMD Path non_HS400 */
		sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_RESPSTSENSEL,
		    platform_para_rx.latch_en_cmd_hs200);
	}
	if (platform_para_func.multi_sync == 0) {
		if (platform_para_func.new_path_hs200 == 1) {
			sdr_set_field(host->base + MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_STOP_DLY,
			    platform_para_rx.new_stop_hs200);
			sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_POPENCNT,
			    platform_para_rx.new_water_hs200);
			sdr_set_field(host->base + SDC_FIFO_CFG,
				SDC_FIFO_CFG_WRVALIDSEL, 0);
			sdr_set_field(host->base + SDC_FIFO_CFG,
				SDC_FIFO_CFG_RDVALIDSEL, 0);
		} else if (platform_para_func.new_path_hs200 == 0) {
			/* use default setting */
			sdr_set_field(host->base + MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_STOP_DLY,
			    platform_para_rx.old_stop_hs200);
			sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_POPENCNT,
			    platform_para_rx.old_water_hs200);
			sdr_set_field(host->base + SDC_FIFO_CFG,
				SDC_FIFO_CFG_WRVALIDSEL, 1);
			sdr_set_field(host->base + SDC_FIFO_CFG,
				SDC_FIFO_CFG_RDVALIDSEL, 1);
		}
	}

	return 0;
}
EXPORT_SYMBOL(autok_init_hs200);

int autok_init_hs400(struct msdc_host *host)
{
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
		sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, 0);
		sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_RESPSTSENSEL, 0);
	} else {
		/* LATCH_TA_EN Config for WCRC Path HS400 */
		sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL,
		    platform_para_rx.latch_en_crc_hs400);
		/* LATCH_TA_EN Config for CMD Path HS400 */
		sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_RESPSTSENSEL,
		    platform_para_rx.latch_en_cmd_hs400);
	}
	/* write path switch to emmc50 */
	autok_write_param(host, EMMC50_WDATA_MUX_EN, 1);
	/* Specifical for HS400 Path Sel */
	autok_write_param(host, MSDC_WCRC_ASYNC_FIFO_SEL, 0);
	if (platform_para_func.multi_sync == 0) {
		if (platform_para_func.new_path_hs400 == 1) {
			sdr_set_field(host->base + SDC_FIFO_CFG,
				SDC_FIFO_CFG_WRVALIDSEL, 0);
			sdr_set_field(host->base + SDC_FIFO_CFG,
				SDC_FIFO_CFG_RDVALIDSEL, 0);
		} else if (platform_para_func.new_path_hs400 == 0) {
			sdr_set_field(host->base + SDC_FIFO_CFG,
				SDC_FIFO_CFG_WRVALIDSEL, 1);
			sdr_set_field(host->base + SDC_FIFO_CFG,
				SDC_FIFO_CFG_RDVALIDSEL, 1);
		}
	}
	sdr_set_field(host->base + EMMC50_CFG0, EMMC50_CFG_READ_DAT_CNT,
		platform_para_rx.read_dat_cnt_hs400);
	sdr_set_field(host->base + EMMC50_CFG0, EMMC50_CFG_END_BIT_CHK_CNT,
		platform_para_rx.end_bit_chk_cnt_hs400);
	sdr_set_field(host->base + EMMC50_CFG1, EMMC50_CFG1_CKSWITCH_CNT,
		platform_para_rx.latchck_switch_cnt_hs400);

	return 0;
}
EXPORT_SYMBOL(autok_init_hs400);

int execute_online_tuning_hs400(struct msdc_host *host, u8 *res)
{
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

	/* DLY3 keep default value */
	if (host->hs400_ds_delay)
		p_autok_tune_res[EMMC50_DS_ZDLY_DLY] =
			host->hs400_ds_delay & 0xff;
	else
		p_autok_tune_res[EMMC50_DS_ZDLY_DLY] =
			platform_para_rx.ds_dly3_hs400;
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
		response = readl(host->base + SDC_RESP0);
		AUTOK_RAWPRINT("[AUTOK]device status 0x%08x\r\n", response);
	} else
		AUTOK_RAWPRINT("[AUTOK]CMD err while check device status\r\n");

	if (support_new_rx(host->dev_comp->new_rx_ver)) {
		if (host->top_base)
			sdr_set_field(host->top_base + EMMC50_PAD_DS_TUNE,
				(PAD_DS_DLY2_SEL | PAD_DS_DLY_SEL), 0);
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
	} else {
		/* tune data pad delay , find data pad boundary */
		for (j = 0; j < 32; j++) {
			autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 4; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_DATA,
				    &autok_host_para);
				if ((ret & (E_RES_CMD_TMO | E_RES_RSP_CRC)) != 0) {
					AUTOK_RAWPRINT
					    ("[AUTOK]Err CMD Fail@RD\r\n");
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
			for (k = 0; k < AUTOK_CMD_TIMES / 4; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_DATA,
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
		if (autok_check_scan_res64_new(RawData64, &pInfo->scan_info[0], 0) != 0)
			goto fail;
		autok_ds_dly_sel(&pInfo->scan_info[0], &uDatDly);
		autok_paddly_update(DS_PAD_RDLY, uDatDly, p_autok_tune_res);
	}
	autok_tuning_parameter_init(host, p_autok_tune_res);
	autok_result_dump(host, p_autok_tune_res);

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
	unsigned int ret = 0;
	int err = 0;
	unsigned int uCmdEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k; /* cycle_value */
	struct AUTOK_REF_INFO *pBdInfo;
	char tune_result_str64[65];
	u32 p_autok_tune_res[5];
	unsigned int opcode = MMC_SEND_STATUS;
	struct autok_host autok_host_para;
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	pBdInfo = kmalloc(sizeof(struct AUTOK_REF_INFO), GFP_ATOMIC);
	if (!pBdInfo) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] mem alloc fail\r\n");
		return -1;
	}
	memset(&autok_host_para, 0, sizeof(struct autok_host));
	memset((void *)p_autok_tune_res, 0,
		sizeof(p_autok_tune_res) / sizeof(u32));

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
	sdr_get_field(host->base + MSDC_IOCON, MSDC_IOCON_RSPL,
	    &p_autok_tune_res[0]);
	if (host->id == MSDC_EMMC) {
#if !defined(FPGA_PLATFORM)
		sdr_get_field(host->base + EMMC_TOP_CMD,
		    PAD_CMD_RXDLY,
		    &p_autok_tune_res[1]);
		sdr_get_field(host->base + EMMC_TOP_CMD,
		    PAD_CMD_RD_RXDLY_SEL,
		    &p_autok_tune_res[2]);
		sdr_get_field(host->base + EMMC_TOP_CMD,
		    PAD_CMD_RXDLY2,
		    &p_autok_tune_res[3]);
		sdr_get_field(host->base + EMMC_TOP_CMD,
		    PAD_CMD_RD_RXDLY2_SEL,
		    &p_autok_tune_res[4]);
#else
		kfree(pBdInfo);
		return 0;
#endif
	} else {
		sdr_get_field(host->base + tune_reg,
			MSDC_PAD_TUNE_CMDRDLY,
			&p_autok_tune_res[1]);
		sdr_get_field(host->base + tune_reg,
			MSDC_PAD_TUNE_CMDRRDLYSEL,
			&p_autok_tune_res[2]);
		sdr_get_field(host->base + MSDC_PAD_TUNE1,
			MSDC_PAD_TUNE1_CMDRDLY2,
			&p_autok_tune_res[3]);
		sdr_get_field(host->base + MSDC_PAD_TUNE1,
			MSDC_PAD_TUNE1_CMDRRDLY2SEL,
			&p_autok_tune_res[4]);
	}

	AUTOK_RAWPRINT("[AUTOK]CMD [EDGE:%d DLY1:%d DLY2:%d]\r\n",
		p_autok_tune_res[0], p_autok_tune_res[1], p_autok_tune_res[3]);

	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
			sizeof(p_autok_tune_res) / sizeof(u32));
	}

	kfree(pBdInfo);
	return 0;
fail:
	kfree(pBdInfo);
	return -1;
}

/* online tuning for eMMC4.5(hs200) */
int execute_online_tuning_hs200(struct msdc_host *host, u8 *res)
{
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
	ret = autok_send_tune_cmd(host, opcode, TUNE_CMD, &autok_host_para);
	if (ret == E_RES_PASS) {
		response = readl(host->base + SDC_RESP0);
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

	autok_result_dump(host, p_autok_tune_res);

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

/* online tuning for SD */
int execute_online_tuning(struct msdc_host *host, u8 *res)
{
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
	struct mmc_host *mmc = mmc_from_priv(host);

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
	if (host->id == MSDC_SD || host->id == MSDC_SDIO) {
		if (mmc->actual_clock <= 100000000) {
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
					msdc_reset_hw(host);
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

	autok_result_dump(host, p_autok_tune_res);

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
	struct AUTOK_PLAT_PARA_TX platform_para_tx;
	unsigned int value;
	unsigned int clk_mode;

	memset(&platform_para_tx, 0, sizeof(struct AUTOK_PLAT_PARA_TX));
	get_platform_para_tx(platform_para_tx);
	if (host->dev_comp->clk_div_bits == 8)
		sdr_get_field(host->base + MSDC_CFG,
			MSDC_CFG_CKMOD, &clk_mode);
	else
		sdr_get_field(host->base + MSDC_CFG,
			MSDC_CFG_CKMOD_EXTRA, &clk_mode);
	if (host->id == MSDC_EMMC) {
		if (ios->timing == MMC_TIMING_MMC_HS400) {
			sdr_set_field(host->base + EMMC50_CFG0,
				EMMC50_CFG_TXSKEW_SEL,
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
			sdr_set_field(host->base + EMMC50_CFG0,
				EMMC50_CFG_TXSKEW_SEL,
				platform_para_tx.msdc0_hs400_txskew);
		} else {
			if (ios->timing == MMC_TIMING_MMC_DDR52) {
				sdr_set_field(host->base + MSDC_IOCON,
					MSDC_IOCON_DDR50CKD,
					platform_para_tx.msdc0_ddr_ckd);
			} else {
				sdr_set_field(host->base + MSDC_IOCON,
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
	} else if (host->id == MSDC_SD || host->id == MSDC_SDIO) {
		sdr_set_field(host->base + MSDC_IOCON,
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
	}
}
EXPORT_SYMBOL(autok_msdc_tx_setting);

void autok_low_speed_switch_edge(struct msdc_host *host,
	struct mmc_ios *ios, enum ERROR_TYPE error_type)
{
	unsigned int orig_resp_edge, orig_crc_fifo_edge;
	unsigned int cur_resp_edge, cur_crc_fifo_edge;
	unsigned int orig_read_edge, orig_read_fifo_edge;
	unsigned int cur_read_edge, cur_read_fifo_edge;
	unsigned int orig_read_data_sample = 0, cur_read_data_sample = 0;

	AUTOK_RAWPRINT("[AUTOK][low speed switch edge]======start======\r\n");
	if (host->id == MSDC_EMMC) {
		switch (error_type) {
		case CMD_ERROR:
			sdr_get_field(host->base + MSDC_IOCON,
			    MSDC_IOCON_RSPL,
			    &orig_resp_edge);
			sdr_set_field(host->base + MSDC_IOCON,
			    MSDC_IOCON_RSPL,
			    orig_resp_edge ^ 0x1);
			sdr_get_field(host->base + MSDC_IOCON,
			    MSDC_IOCON_RSPL,
			    &cur_resp_edge);
			AUTOK_RAWPRINT("[AUTOK][CMD err]edge %d->%d\r\n"
				, orig_resp_edge, cur_resp_edge);
			break;
		case DATA_ERROR:
#ifdef PORT0_PB0_RD_DAT_SEL_VALID
			if (ios->timing == MMC_TIMING_MMC_DDR52) {
				sdr_set_field(host->base + MSDC_IOCON,
					MSDC_IOCON_DSPLSEL, 0);
				sdr_get_field(host->base + MSDC_IOCON,
				    MSDC_IOCON_DSPLSEL,
				    &orig_read_edge);
				sdr_set_field(host->base + MSDC_IOCON,
				    MSDC_IOCON_DSPLSEL,
				    orig_read_edge ^ 0x1);
				sdr_set_field(host->base + MSDC_PATCH_BIT,
					MSDC_PATCH_BIT_RD_DAT_SEL, 0);
				sdr_get_field(host->base + MSDC_IOCON,
				    MSDC_IOCON_DSPLSEL,
				    &cur_read_edge);
				sdr_get_field(host->base + MSDC_PATCH_BIT,
				    MSDC_PATCH_BIT_RD_DAT_SEL,
				    &cur_read_fifo_edge);
				if (support_new_tx(host->dev_comp->new_tx_ver)) {
					sdr_get_field(host->base + MSDC_IOCON,
					    MSDC_IOCON_DSPL,
					    &orig_read_data_sample);
					sdr_set_field(host->base + MSDC_IOCON,
					    MSDC_IOCON_DSPL,
					    orig_read_data_sample ^ 0x1);
					sdr_get_field(host->base + MSDC_IOCON,
					    MSDC_IOCON_DSPL,
					    &cur_read_data_sample);
				}
				AUTOK_RAWPRINT("[AUTOK][RD err]fifo_edge = %d",
					cur_read_fifo_edge);
				AUTOK_RAWPRINT("edge %d->%d, read_data_sample %d->%d\r\n",
					orig_read_edge, cur_read_edge,
					orig_read_data_sample, cur_read_data_sample);
			} else {
				sdr_set_field(host->base + MSDC_IOCON,
					MSDC_IOCON_DSPLSEL, 0);
				sdr_set_field(host->base + MSDC_IOCON,
					MSDC_IOCON_DSPLSEL, 0);
				sdr_get_field(host->base + MSDC_PATCH_BIT,
				    MSDC_PATCH_BIT_RD_DAT_SEL,
				    &orig_read_fifo_edge);
				sdr_set_field(host->base + MSDC_PATCH_BIT,
				    MSDC_PATCH_BIT_RD_DAT_SEL,
				    orig_read_fifo_edge ^ 0x1);
				sdr_get_field(host->base + MSDC_IOCON,
				    MSDC_IOCON_DSPLSEL,
				    &cur_read_edge);
				sdr_get_field(host->base + MSDC_PATCH_BIT,
				    MSDC_PATCH_BIT_RD_DAT_SEL,
				    &cur_read_fifo_edge);
				AUTOK_RAWPRINT("[AUTOK][RD err]edge = %d",
				    cur_read_edge);
				AUTOK_RAWPRINT("fifo_edge %d->%d\r\n",
				    orig_read_fifo_edge,
				    cur_read_fifo_edge);
			}
#else
			sdr_set_field(host->base + MSDC_IOCON,
				MSDC_IOCON_DSPLSEL, 0);
			sdr_get_field(host->base + MSDC_IOCON,
			    MSDC_IOCON_DSPLSEL,
			    &orig_read_edge);
			sdr_set_field(host->base + MSDC_PATCH_BIT,
					MSDC_PATCH_BIT_RD_DAT_SEL, 0);
			sdr_set_field(host->base + MSDC_IOCON,
				MSDC_IOCON_DSPLSEL, orig_read_edge ^ 0x1);
			sdr_get_field(host->base + MSDC_IOCON,
				MSDC_IOCON_DSPLSEL, &cur_read_edge);
			AUTOK_RAWPRINT("[AUTOK][RD err]edge %d->%d\r\n",
				orig_read_edge, cur_read_edge);
#endif
			break;
		case CRC_STATUS_ERROR:
			sdr_get_field(host->base + MSDC_PATCH_BIT2,
			    MSDC_PB2_CFGCRCSTSEDGE,
			    &orig_crc_fifo_edge);
			sdr_set_field(host->base + MSDC_PATCH_BIT2,
			    MSDC_PB2_CFGCRCSTSEDGE,
			    orig_crc_fifo_edge ^ 0x1);
			sdr_get_field(host->base + MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE,
				&cur_crc_fifo_edge);
			AUTOK_RAWPRINT("[AUTOK][WR err]edge %d->%d\r\n",
			    orig_crc_fifo_edge, cur_crc_fifo_edge);
			break;
		}
	} else if (host->id == MSDC_SD || host->id == MSDC_SDIO) {
		switch (error_type) {
		case CMD_ERROR:
			sdr_get_field(host->base + MSDC_IOCON,
				MSDC_IOCON_RSPL,
				&orig_resp_edge);
			sdr_set_field(host->base + MSDC_IOCON,
				MSDC_IOCON_RSPL,
				orig_resp_edge ^ 0x1);
			sdr_get_field(host->base + MSDC_IOCON,
				MSDC_IOCON_RSPL,
				&cur_resp_edge);
			AUTOK_RAWPRINT("[AUTOK][CMD err]edge %d->%d\r\n",
				orig_resp_edge, cur_resp_edge);
			break;
		case DATA_ERROR:
#ifdef PORT1_PB0_RD_DAT_SEL_VALID
			if (ios->timing == MMC_TIMING_UHS_DDR50) {
				sdr_set_field(host->base + MSDC_IOCON,
					MSDC_IOCON_DSPLSEL, 0);
				sdr_get_field(host->base + MSDC_IOCON,
					MSDC_IOCON_DSPLSEL,
					&orig_read_edge);
				sdr_set_field(host->base + MSDC_IOCON,
					MSDC_IOCON_DSPLSEL,
					orig_read_edge ^ 0x1);
				sdr_set_field(host->base + MSDC_PATCH_BIT,
					MSDC_PATCH_BIT_RD_DAT_SEL, 0);
				sdr_get_field(host->base + MSDC_IOCON,
					MSDC_IOCON_DSPLSEL,
					&cur_read_edge);
				sdr_get_field(host->base + MSDC_PATCH_BIT,
					MSDC_PATCH_BIT_RD_DAT_SEL,
					&cur_read_fifo_edge);
				if (support_new_tx(host->dev_comp->new_tx_ver)) {
					sdr_get_field(host->base + MSDC_IOCON,
					    MSDC_IOCON_DSPL,
					    &orig_read_data_sample);
					sdr_set_field(host->base + MSDC_IOCON,
					    MSDC_IOCON_DSPL,
					    orig_read_data_sample ^ 0x1);
					sdr_get_field(host->base + MSDC_IOCON,
					    MSDC_IOCON_DSPL,
					    &cur_read_data_sample);
				}
				AUTOK_RAWPRINT("[AUTOK][RD err]fifo_edge = %d",
					cur_read_fifo_edge);
				AUTOK_RAWPRINT("edge %d->%d, read_data_sample %d->%d\r\n",
					orig_read_edge, cur_read_edge,
					orig_read_data_sample, cur_read_data_sample);
			} else {
				sdr_set_field(host->base + MSDC_IOCON,
					MSDC_IOCON_DSPLSEL, 0);
				sdr_set_field(host->base + MSDC_IOCON,
					MSDC_IOCON_DSPLSEL, 0);
				sdr_get_field(host->base + MSDC_PATCH_BIT,
					MSDC_PATCH_BIT_RD_DAT_SEL,
					&orig_read_fifo_edge);
				sdr_set_field(host->base + MSDC_PATCH_BIT,
					MSDC_PATCH_BIT_RD_DAT_SEL,
					orig_read_fifo_edge ^ 0x1);
				sdr_get_field(host->base + MSDC_IOCON,
					MSDC_IOCON_DSPLSEL,
					&cur_read_edge);
				sdr_get_field(host->base + MSDC_PATCH_BIT,
					MSDC_PATCH_BIT_RD_DAT_SEL,
					&cur_read_fifo_edge);
				AUTOK_RAWPRINT("[AUTOK][RD err]edge = %d",
				    cur_read_edge);
				AUTOK_RAWPRINT("fifo_edge %d->%d\r\n",
				    orig_read_fifo_edge, cur_read_fifo_edge);
			}
#else
			sdr_set_field(host->base + MSDC_IOCON,
				MSDC_IOCON_DSPLSEL, 0);
			sdr_get_field(host->base + MSDC_IOCON,
				MSDC_IOCON_DSPLSEL,
				&orig_read_edge);
			sdr_set_field(host->base + MSDC_PATCH_BIT,
					MSDC_PATCH_BIT_RD_DAT_SEL, 0);
			sdr_set_field(host->base + MSDC_IOCON,
				MSDC_IOCON_DSPLSEL,
				orig_read_edge ^ 0x1);
			sdr_get_field(host->base + MSDC_IOCON,
				MSDC_IOCON_DSPLSEL,
				&cur_read_edge);
			AUTOK_RAWPRINT("[AUTOK][RD err]edge %d->%d\r\n"
				, orig_read_edge, cur_read_edge);
#endif
			break;
		case CRC_STATUS_ERROR:
			sdr_get_field(host->base + MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE,
				&orig_crc_fifo_edge);
			sdr_set_field(host->base + MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE,
				orig_crc_fifo_edge ^ 0x1);
			sdr_get_field(host->base + MSDC_PATCH_BIT2,
				MSDC_PB2_CFGCRCSTSEDGE,
				&cur_crc_fifo_edge);
			AUTOK_RAWPRINT("[AUTOK][WR err]edge %d->%d\r\n",
				orig_crc_fifo_edge, cur_crc_fifo_edge);
			break;
		}
	}

	AUTOK_RAWPRINT("[AUTOK][low speed switch edge]======end======\r\n");
}
EXPORT_SYMBOL(autok_low_speed_switch_edge);

int autok_execute_tuning(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timespec64 tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	u8 autok_tune_res[TUNING_PARAM_COUNT];
	unsigned int i = 0;
	unsigned int value = 0;
	unsigned int dvfs_en = 0;
	unsigned int dvfs_hw = 0;
	unsigned int dtoc = 0;

	ktime_get_ts64(&tm_s);

	int_en = readl(host->base + MSDC_INTEN);
	writel(0, host->base + MSDC_INTEN);
	sdr_get_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, &clk_pwdn);
	sdr_get_field(host->base + MSDC_CFG, MSDC_CFG_DVFS_EN, &dvfs_en);
	sdr_get_field(host->base + MSDC_CFG, MSDC_CFG_DVFS_HW, &dvfs_hw);
	sdr_get_field(host->base + SDC_CFG, SDC_CFG_DTOC, &dtoc);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, 1);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_DVFS_EN, 0);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_DVFS_HW, 0);
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC, 3);

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

	msdc_reset_hw(host);
	writel(int_en, host->base + MSDC_INTEN);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_DVFS_EN, dvfs_en);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_DVFS_HW, dvfs_hw);
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC, dtoc);

	ktime_get_ts64(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000
		+ (tm_e.tv_nsec - tm_s.tv_nsec) / 1000000;
	AUTOK_RAWPRINT("[AUTOK]======Cost:%d ms======\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(autok_execute_tuning);

int hs400_execute_tuning(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timespec64 tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	u8 autok_tune_res[TUNING_PARAM_COUNT];
	unsigned int i = 0;
	unsigned int value = 0;

	ktime_get_ts64(&tm_s);
	int_en = readl(host->base + MSDC_INTEN);
	writel(0, host->base + MSDC_INTEN);
	sdr_get_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, &clk_pwdn);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, 1);

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

	msdc_reset_hw(host);
	writel(int_en, host->base + MSDC_INTEN);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	ktime_get_ts64(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000
		+ (tm_e.tv_nsec - tm_s.tv_nsec) / 1000000;
	AUTOK_RAWPRINT("[AUTOK][HS400]======Cost:%d ms======\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(hs400_execute_tuning);

int hs400_execute_tuning_cmd(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timespec64 tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;

	ktime_get_ts64(&tm_s);
	int_en = readl(host->base + MSDC_INTEN);
	writel(0, host->base + MSDC_INTEN);
	sdr_get_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, &clk_pwdn);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, 1);

	autok_init_hs400(host);
	ret = execute_cmd_online_tuning(host, res);
	if (ret != 0)
		AUTOK_RAWPRINT("[AUTOK cmd] ======HS400 Failed======\r\n");

	writel(int_en, host->base + MSDC_INTEN);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	ktime_get_ts64(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000
		+ (tm_e.tv_nsec - tm_s.tv_nsec) / 1000000;
	AUTOK_RAWPRINT("[AUTOK][HS400 cmd]======Cost:%d ms======\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(hs400_execute_tuning_cmd);

int hs200_execute_tuning(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timespec64 tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;
	u8 autok_tune_res[TUNING_PARAM_COUNT];
	unsigned int i = 0;
	unsigned int value = 0;
	unsigned int dtoc = 0;
	struct AUTOK_PLAT_FUNC platform_para_func;
	unsigned int ckgen;

	memset(&platform_para_func, 0, sizeof(struct AUTOK_PLAT_FUNC));
	get_platform_func(platform_para_func);
	ktime_get_ts64(&tm_s);
	int_en = readl(host->base + MSDC_INTEN);
	writel(0, host->base + MSDC_INTEN);
	sdr_get_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, &clk_pwdn);
	sdr_get_field(host->base + SDC_CFG, SDC_CFG_DTOC, &dtoc);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, 1);
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC, 3);

	/* store pre autok parameter */
	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		autok_adjust_param(host, i, &value, AUTOK_READ);
		autok_tune_res[i] = value;
	}

	writel(0xffffffff, host->base + MSDC_INT);
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

	msdc_reset_hw(host);
	writel(int_en, host->base + MSDC_INTEN);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	sdr_set_field(host->base + SDC_CFG, SDC_CFG_DTOC, dtoc);

	ktime_get_ts64(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000
		+ (tm_e.tv_nsec - tm_s.tv_nsec) / 1000000;
	AUTOK_RAWPRINT("[AUTOK][HS200]======Cost:%d ms======\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(hs200_execute_tuning);

int hs200_execute_tuning_cmd(struct msdc_host *host, u8 *res)
{
	int ret = 0;
	struct timespec64 tm_s, tm_e;
	unsigned int tm_val = 0;
	unsigned int clk_pwdn = 0;
	unsigned int int_en = 0;

	ktime_get_ts64(&tm_s);
	int_en = readl(host->base + MSDC_INTEN);
	writel(0, host->base + MSDC_INTEN);
	sdr_get_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, &clk_pwdn);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, 1);

	autok_init_hs200(host);
	ret = execute_cmd_online_tuning(host, res);
	if (ret != 0)
		AUTOK_RAWPRINT("[AUTOK cmd] ======HS200 Failed======\r\n");

	writel(int_en, host->base + MSDC_INTEN);
	sdr_set_field(host->base + MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	ktime_get_ts64(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000
		+ (tm_e.tv_nsec - tm_s.tv_nsec) / 1000000;
	AUTOK_RAWPRINT("[AUTOK][HS200 cmd]======Cost:%d ms======\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(hs200_execute_tuning_cmd);

int autok_vcore_merge_sel(struct msdc_host *host, unsigned int merge_cap)
{
	unsigned int ret = 0;
	struct timespec64 tm_s, tm_e;
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

	ktime_get_ts64(&tm_s);
	if (host->dev_comp->clk_div_bits == 8)
		sdr_get_field(host->base + MSDC_CFG,
			MSDC_CFG_CKMOD, &clk_mode);
	else
		sdr_get_field(host->base + MSDC_CFG,
			MSDC_CFG_CKMOD_EXTRA, &clk_mode);

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
	if (host->id == MSDC_EMMC) {
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
	if (host->id == MSDC_EMMC) {
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
	ktime_get_ts64(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000
		+ (tm_e.tv_nsec - tm_s.tv_nsec) / 1000000;
	AUTOK_RAWPRINT("[AUTOK][merge]======Time Cost:%d ms======\r\n", tm_val);

	kfree(pInfo);
	return ret;
fail:
	kfree(pInfo);
	return -1;
}
EXPORT_SYMBOL(autok_vcore_merge_sel);

int sd_execute_dvfs_autok(struct msdc_host *host, u32 opcode)
{
	int ret = 0;
	int vcore = AUTOK_VCORE_MERGE;
	u8 *res;
	struct mmc_host *mmc = mmc_from_priv(host);

	res = host->autok_res[vcore];

	if (mmc->ios.timing == MMC_TIMING_UHS_SDR104 ||
	    mmc->ios.timing == MMC_TIMING_UHS_SDR50) {
		if (host->is_autok_done == 0) {
			pr_notice("[AUTOK]SDcard autok\n");
			ret = autok_execute_tuning(host, res);
			memcpy(host->autok_res[AUTOK_VCORE_LEVEL0],
					host->autok_res[AUTOK_VCORE_MERGE],
					TUNING_PARA_SCAN_COUNT);
			host->is_autok_done = 1;
		} else {
			autok_init_sdr104(host);
			autok_tuning_parameter_init(host, res);
		}
	}

	return ret;
}
EXPORT_SYMBOL(sd_execute_dvfs_autok);

int emmc_execute_dvfs_autok(struct msdc_host *host, u32 opcode)
{
	int ret = 0;
	int vcore = AUTOK_VCORE_MERGE;
	u8 *res;
	struct mmc_host *mmc = mmc_from_priv(host);

	res = host->autok_res[vcore];

	if (mmc->ios.timing == MMC_TIMING_MMC_HS200
		&& !host->is_autok_done) {
		if (opcode == MMC_SEND_STATUS) {
			pr_notice("[AUTOK]eMMC HS200 Tune CMD only\n");
			ret = hs200_execute_tuning_cmd(host, res);
		} else {
			pr_notice("[AUTOK]eMMC HS200 Tune\n");
			ret = hs200_execute_tuning(host, res);
		}
	} else if (mmc->ios.timing == MMC_TIMING_MMC_HS400) {
		if (opcode == MMC_SEND_STATUS) {
			pr_notice("[AUTOK]eMMC HS400 Tune CMD only\n");
			ret = hs400_execute_tuning_cmd(host, res);
		} else {
			pr_notice("[AUTOK]eMMC HS400 Tune\n");
			ret = hs400_execute_tuning(host, res);
		}
	}

	return ret;
}
EXPORT_SYMBOL(emmc_execute_dvfs_autok);

int emmc_execute_autok(struct msdc_host *host, u32 opcode)
{
	int ret = 0;
#if !defined(FPGA_PLATFORM)
	int merge_result, merge_mode, merge_window, need_merge = 0;
	int i;
	int autok_err_type = -1;
	struct mmc_host *mmc = mmc_from_priv(host);

	if (mmc->ios.timing == MMC_TIMING_MMC_HS400 ||
		mmc->ios.timing == MMC_TIMING_MMC_HS200) {
		ret = emmc_execute_dvfs_autok(host, opcode);
		if (ret)
			goto exit;
		if (mmc->ios.timing == MMC_TIMING_MMC_HS400) {
			if (host->is_autok_done) {
				memcpy(host->autok_res[AUTOK_VCORE_LEVEL1],
						host->autok_res[AUTOK_VCORE_MERGE],
						TUNING_PARA_SCAN_COUNT);
				need_merge = 1;
			} else
				memcpy(host->autok_res[AUTOK_VCORE_LEVEL0],
						host->autok_res[AUTOK_VCORE_MERGE],
						TUNING_PARA_SCAN_COUNT);
			host->is_autok_done = 1;
		}
	} else {
		need_merge = 0;
		if (host->need_tune & TUNE_CMD_CRC)
			autok_err_type = CMD_ERROR;
		else if (host->need_tune & TUNE_DATA_WRITE)
			autok_err_type = CRC_STATUS_ERROR;
		else if (host->need_tune & TUNE_DATA_READ)
			autok_err_type = DATA_ERROR;
		if (autok_err_type != -1)
			autok_low_speed_switch_edge(host, &mmc->ios, autok_err_type);
		host->need_tune = TUNE_NONE;
	}

	if (opcode == MMC_SEND_STATUS)
		goto exit;

	if (need_merge) {
		if (mmc->ios.timing == MMC_TIMING_MMC_HS400)
			merge_mode = MERGE_HS400;
		else
			merge_mode = MERGE_HS200_SDR104;

		merge_result = autok_vcore_merge_sel(host, merge_mode);
		for (i = CMD_MAX_WIN; i <= H_CLK_TX_MAX_WIN; i++) {
			merge_window = host->autok_res[AUTOK_VCORE_MERGE][i];
			if (merge_window < AUTOK_MERGE_MIN_WIN) {
				merge_result = -2;
				pr_info("[AUTOK]%s:merge_window[%d] less than %d\n",
					__func__, i, AUTOK_MERGE_MIN_WIN);
			}
			if (merge_window != 0xFF)
				pr_info("[AUTOK]merge_value = %d\n", merge_window);
		}
		if (merge_result == 0) {
			autok_tuning_parameter_init(host,
				host->autok_res[AUTOK_VCORE_MERGE]);
			pr_info("[AUTOK]No need change para when dvfs\n");
		} else if (merge_result < 0) {
			autok_tuning_parameter_init(host,
				host->autok_res[AUTOK_VCORE_LEVEL1]);
			pr_info("[AUTOK]merge_result:%d,restore legacy window\n", merge_result);
		}
	}

exit:

#endif

	return ret;
}
EXPORT_SYMBOL(emmc_execute_autok);

int sd_execute_autok(struct msdc_host *host, u32 opcode)
{
	int merge_result, merge_mode, merge_window, need_merge = 0;
	int i, ret = 0;
	u8 *res = host->autok_res[AUTOK_VCORE_MERGE];
	int autok_err_type = -1;
	struct mmc_host *mmc = mmc_from_priv(host);

	if (mmc->ios.timing == MMC_TIMING_UHS_SDR104 ||
		mmc->ios.timing == MMC_TIMING_UHS_SDR50) {
		pr_notice("[AUTOK]SD/SDIO card autok\n");
		ret = autok_execute_tuning(host, res);
		if (ret)
			goto exit;
		if (mmc->ios.timing == MMC_TIMING_UHS_SDR104) {
			if (host->is_autok_done) {
				memcpy(host->autok_res[AUTOK_VCORE_LEVEL1],
						host->autok_res[AUTOK_VCORE_MERGE],
						TUNING_PARA_SCAN_COUNT);
				need_merge = 1;
			} else
				memcpy(host->autok_res[AUTOK_VCORE_LEVEL0],
						host->autok_res[AUTOK_VCORE_MERGE],
						TUNING_PARA_SCAN_COUNT);
			host->is_autok_done = 1;
		}
	} else if (mmc->ios.timing == MMC_TIMING_MMC_HS200) {
		/* Distinguish mmc by timing */
		if (opcode == MMC_SEND_STATUS) {
			pr_notice("[AUTOK]MMC HS200 Tune CMD only\n");
			ret = hs200_execute_tuning_cmd(host, res);
			need_merge = 0;
		} else {
			pr_notice("[AUTOK]MMC HS200 Tune\n");
			ret = hs200_execute_tuning(host, res);
			if (host->is_autok_done) {
				memcpy(host->autok_res[AUTOK_VCORE_LEVEL1],
						host->autok_res[AUTOK_VCORE_MERGE],
						TUNING_PARA_SCAN_COUNT);
				need_merge = 1;
			} else
				memcpy(host->autok_res[AUTOK_VCORE_LEVEL0],
						host->autok_res[AUTOK_VCORE_MERGE],
						TUNING_PARA_SCAN_COUNT);
			host->is_autok_done = 1;
		}
	} else {
		need_merge = 0;
		if (host->need_tune & TUNE_CMD_CRC)
			autok_err_type = CMD_ERROR;
		else if (host->need_tune & TUNE_DATA_WRITE)
			autok_err_type = CRC_STATUS_ERROR;
		else if (host->need_tune & TUNE_DATA_READ)
			autok_err_type = DATA_ERROR;
		if (autok_err_type != -1)
			autok_low_speed_switch_edge(host, &mmc->ios, autok_err_type);
		host->need_tune = TUNE_NONE;
	}
	if (need_merge) {
		merge_mode = MERGE_HS200_SDR104;
		merge_result = autok_vcore_merge_sel(host, merge_mode);
		for (i = CMD_MAX_WIN; i <= H_CLK_TX_MAX_WIN; i++) {
			merge_window = host->autok_res[AUTOK_VCORE_MERGE][i];
			if (merge_window < AUTOK_MERGE_MIN_WIN) {
				merge_result = -2;
				pr_info("[AUTOK]%s:merge_window[%d] less than %d\n",
					__func__, i, AUTOK_MERGE_MIN_WIN);
			}
			if (merge_window != 0xFF)
				pr_info("[AUTOK]merge_value = %d\n", merge_window);
		}

		if (merge_result == 0) {
			autok_tuning_parameter_init(host,
				host->autok_res[AUTOK_VCORE_MERGE]);
			pr_info("[AUTOK]No need change para when dvfs\n");
		} else if (merge_result < 0) {
			autok_tuning_parameter_init(host,
				host->autok_res[AUTOK_VCORE_LEVEL1]);
		}
	}

exit:
	return ret;
}
EXPORT_SYMBOL(sd_execute_autok);

void msdc_init_tune_path(struct msdc_host *host, unsigned char timing)
{
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	writel(0x00000000, host->base + tune_reg);

	if (host->top_base) {
		/* FIX ME: toggle these fields according to timing */
		/* FIX ME: maybe unnecessary if autok can take care */
		sdr_clr_bits(host->top_base + EMMC_TOP_CONTROL, DATA_K_VALUE_SEL);
		sdr_clr_bits(host->top_base + EMMC_TOP_CONTROL, DELAY_EN);
		sdr_clr_bits(host->top_base + EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY);
		sdr_clr_bits(host->top_base + EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY_SEL);
		sdr_clr_bits(host->top_base + EMMC_TOP_CONTROL, PAD_RXDLY_SEL);
		sdr_clr_bits(host->top_base + EMMC_TOP_CMD, PAD_CMD_RXDLY);
		sdr_clr_bits(host->top_base + EMMC_TOP_CMD, PAD_CMD_RD_RXDLY_SEL);
		sdr_clr_bits(host->top_base + EMMC50_PAD_CTL0, PAD_CLK_TXDLY);
	}

	sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_DDLSEL);
	sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPLSEL);
	sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_DSPL);
	if (timing == MMC_TIMING_MMC_HS400) {
		sdr_clr_bits(host->base + tune_reg, MSDC_PAD_TUNE_DATRRDLYSEL);
		sdr_clr_bits(host->base + MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL);
		if (host->top_base) {
			/* FIX ME: maybe unnecessary if autok can take care */
			sdr_clr_bits(host->top_base + EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY_SEL);
			sdr_clr_bits(host->top_base + EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY2_SEL);
		}
	} else {
		sdr_set_bits(host->base + tune_reg, MSDC_PAD_TUNE_DATRRDLYSEL);
		sdr_clr_bits(host->base + MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL);
		if (host->top_base) {
			/* FIX ME: maybe unnecessary if autok can take care */
			sdr_set_bits(host->top_base + EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY_SEL);
			sdr_clr_bits(host->top_base + EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY2_SEL);
		}
	}

	if (timing == MMC_TIMING_MMC_HS400)
		sdr_clr_bits(host->base + MSDC_PATCH_BIT2, MSDC_PATCH_BIT2_CFGCRCSTS);
	else
		sdr_set_bits(host->base + MSDC_PATCH_BIT2, MSDC_PATCH_BIT2_CFGCRCSTS);

	sdr_clr_bits(host->base + MSDC_IOCON, MSDC_IOCON_W_DSMPL_SEL);

	sdr_clr_bits(host->base + MSDC_PATCH_BIT2, MSDC_PATCH_BIT2_CFGRESP);
	sdr_set_bits(host->base + tune_reg, MSDC_PAD_TUNE_CMDRRDLYSEL);
	sdr_clr_bits(host->base + MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRRDLY2SEL);

	if ((timing == MMC_TIMING_MMC_HS400) && (host->top_base)) {
		/* FIX ME: maybe unnecessary if autok can take care */
		sdr_set_bits(host->top_base + EMMC_TOP_CMD, PAD_CMD_RD_RXDLY_SEL);
		sdr_clr_bits(host->top_base + EMMC_TOP_CMD, PAD_CMD_RD_RXDLY2_SEL);
	}

	sdr_clr_bits(host->base + EMMC50_CFG0, EMMC50_CFG_CMD_RESP_SEL);

	autok_path_sel(host);
}
EXPORT_SYMBOL(msdc_init_tune_path);

void msdc_init_tune_setting(struct msdc_host *host)
{
	u32 val;
	u32 tune_reg = host->dev_comp->pad_tune_reg;

	/* FIX ME: check if always convered by autok */
	sdr_set_field(host->base + tune_reg, MSDC_PAD_TUNE_CLKTDLY,
		MSDC_CLKTXDLY);
	if (host->top_base) {
		sdr_set_field(host->top_base + EMMC50_PAD_CTL0, PAD_CLK_TXDLY,
			MSDC_CLKTXDLY);
		sdr_set_field(host->top_base + EMMC_TOP_CONTROL,
			(PAD_DAT_RD_RXDLY2 | PAD_DAT_RD_RXDLY), 0);
		sdr_set_field(host->top_base + EMMC_TOP_CMD,
			(PAD_CMD_RXDLY2 | PAD_CMD_RXDLY), 0);
		sdr_set_field(host->top_base + EMMC50_PAD_DS_TUNE,
			(PAD_DS_DLY3 | PAD_DS_DLY2 | PAD_DS_DLY1), 0);
	}

	/* Reserve MSDC_IOCON_DDR50CKD bit, clear all other bits */
	val = readl(host->base + MSDC_IOCON) & MSDC_IOCON_DDR50CKD;
	writel(val, host->base + MSDC_IOCON);

	writel(0x00000000, host->base + MSDC_DAT_RDDLY0);
	writel(0x00000000, host->base + MSDC_DAT_RDDLY1);

	writel(MSDC_PB0_DEFAULT_VAL, host->base + MSDC_PATCH_BIT);
	writel(MSDC_PB1_DEFAULT_VAL, host->base + MSDC_PATCH_BIT1);

	/* Fix HS400 mode */
	sdr_clr_bits(host->base + EMMC50_CFG0, EMMC50_CFG_TXSKEW_SEL);
	sdr_set_bits(host->base + MSDC_PATCH_BIT1, MSDC_PB1_DDR_CMD_FIX_SEL);

	/* DDR50 mode */
	sdr_set_bits(host->base + MSDC_PATCH_BIT2, MSDC_PB2_DDR50SEL);

	/* 64T + 48T cmd <-> resp */
	sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_RESPWAIT,
		MSDC_PB2_DEFAULT_RESPWAIT);
	sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_RESPSTSENSEL,
		MSDC_PB2_DEFAULT_RESPSTSENSEL);
	sdr_set_field(host->base + MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL,
		MSDC_PB2_DEFAULT_CRCSTSENSEL);

	autok_path_sel(host);
}
EXPORT_SYMBOL(msdc_init_tune_setting);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek SD/MMC Autok Driver");
