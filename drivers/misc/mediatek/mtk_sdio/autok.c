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

#include "autok.h"
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/io.h>

#include <linux/time.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/scatterlist.h>

#define MSDC_FIFO_THD_1K                (1024)
#define TUNE_TX_CNT                     (100)
/*#define TUNE_DATA_TX_ADDR               (0x358000)*/
/* Use negative value to represent address from end of device,
 * 33 blocks used by SGPT at end of device,
 * 32768 blocks used by flashinfo immediate before SGPT
 */
#define TUNE_DATA_TX_ADDR               (-33-32768)
#define CMDQ
#define AUTOK_LATCH_CK_EMMC_TUNE_TIMES  (10) /* 5.0IP eMMC 1KB fifo ZIZE */
#define AUTOK_LATCH_CK_SDIO_TUNE_TIMES  (3)  /* 4.5IP SDIO 128fifo ZIZE */
#define AUTOK_LATCH_CK_SD_TUNE_TIMES    (3)  /* 4.5IP SD 128fifo ZIZE */
#define AUTOK_CMD_TIMES                 (20)
#define AUTOK_TUNING_INACCURACY         (3)  /* scan result may find xxxxooxxx */
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

/* autok platform specific setting */
#define AUTOK_CKGEN_VALUE                       (0)
#define AUTOK_CMD_LATCH_EN_HS400_VALUE          (3)
#define AUTOK_CMD_LATCH_EN_NON_HS400_VALUE      (2)
#define AUTOK_CRC_LATCH_EN_HS400_VALUE          (0)
#define AUTOK_CRC_LATCH_EN_NON_HS400_VALUE      (2)
#define AUTOK_LATCH_CK_VALUE                    (1)
#define AUTOK_CMD_TA_VALUE                      (0)
#define AUTOK_CRC_TA_VALUE                      (0)
#define AUTOK_CRC_MA_VALUE                      (1)
#define AUTOK_BUSY_MA_VALUE                     (1)

enum TUNE_TYPE {
	TUNE_CMD = 0,
	TUNE_DATA,
	TUNE_LATCH_CK,
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
		autok_msdc_retry(MSDC_READ32(MSDC_CFG) & MSDC_CFG_RST, retry, cnt); \
	} while (0)

#define msdc_rxfifocnt() \
	((MSDC_READ32(MSDC_FIFOCS) & MSDC_FIFOCS_RXCNT) >> 0)
#define msdc_txfifocnt() \
	((MSDC_READ32(MSDC_FIFOCS) & MSDC_FIFOCS_TXCNT) >> 16)

#define wait_cond(cond, tmo, left) \
	do { \
		u32 t = tmo; \
		while (1) { \
			if ((cond) || (t == 0)) \
				break; \
			if (t > 0) { \
				ndelay(1); \
				t--; \
			} \
		} \
		left = t; \
	} while (0)


#define msdc_clear_fifo() \
	do { \
		int retry = 5, cnt = 1000; \
		MSDC_SET_BIT32(MSDC_FIFOCS, MSDC_FIFOCS_CLR); \
		/* ensure fifo clear operation be sequential  */ \
		mb(); \
		autok_msdc_retry(MSDC_READ32(MSDC_FIFOCS) & MSDC_FIFOCS_CLR, retry, cnt); \
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

#define BD_MAX_CNT 4	/* Max Allowed Boundary Number */
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

unsigned int do_autok_offline_tune_tx;
unsigned int autok_debug_level = AUTOK_DBG_RES;

const struct AUTOK_PARAM_INFO autok_param_info[] = {
	{{0, 1}, "CMD_EDGE"},
	{{0, 1}, "RDATA_EDGE"},         /* async fifo mode Pad dat edge must fix to 0 */
	{{0, 1}, "RD_FIFO_EDGE"},
	{{0, 1}, "WD_FIFO_EDGE"},

	{{0, 31}, "CMD_RD_D_DLY1"},     /* Cmd Pad Tune Data Phase */
	{{0, 1}, "CMD_RD_D_DLY1_SEL"},
	{{0, 31}, "CMD_RD_D_DLY2"},
	{{0, 1}, "CMD_RD_D_DLY2_SEL"},

	{{0, 31}, "DAT_RD_D_DLY1"},     /* Data Pad Tune Data Phase */
	{{0, 1}, "DAT_RD_D_DLY1_SEL"},
	{{0, 31}, "DAT_RD_D_DLY2"},
	{{0, 1}, "DAT_RD_D_DLY2_SEL"},

	{{0, 7}, "INT_DAT_LATCH_CK"},   /* Latch CK Delay for data read when clock stop */

	{{0, 31}, "EMMC50_DS_Z_DLY1"},	/* eMMC50 Related tuning param */
	{{0, 1}, "EMMC50_DS_Z_DLY1_SEL"},
	{{0, 31}, "EMMC50_DS_Z_DLY2"},
	{{0, 1}, "EMMC50_DS_Z_DLY2_SEL"},
	{{0, 31}, "EMMC50_DS_ZDLY_DLY"},

	/* ================================================= */
	/* Timming Related Mux & Common Setting Config */
	{{0, 1}, "READ_DATA_SMPL_SEL"},         /* all data line path share sample edge */
	{{0, 1}, "WRITE_DATA_SMPL_SEL"},
	{{0, 1}, "DATA_DLYLINE_SEL"},           /* clK tune all data Line share dly */
	{{0, 1}, "MSDC_WCRC_ASYNC_FIFO_SEL"},   /* data tune mode select */
	{{0, 1}, "MSDC_RESP_ASYNC_FIFO_SEL"},   /* data tune mode select */
	/* eMMC50 Function Mux */
	{{0, 1}, "EMMC50_WDATA_MUX_EN"},        /* write path switch to emmc45 */
	{{0, 1}, "EMMC50_CMD_MUX_EN"},          /* response path switch to emmc45 */
	{{0, 1}, "EMMC50_WDATA_EDGE"},
	/* Common Setting Config */
	{{0, 31}, "CKGEN_MSDC_DLY_SEL"},
	{{1, 7}, "CMD_RSP_TA_CNTR"},
	{{1, 7}, "WRDAT_CRCS_TA_CNTR"},
	{{0, 31}, "PAD_CLK_TXDLY"},             /* tx clk dly fix to 0 for HQA res */
};

/**********************************************************
 * AutoK Basic Interface Implenment                       *
 **********************************************************/
#if defined(SDIO_TUNE_WRITE_PATH)
static void mmc_wait_done(struct mmc_request *mrq)
{
	complete(&mrq->completion);
}

static int autok_wait_for_req(struct msdc_host *host, struct mmc_request *mrq)
{
	int ret = E_RESULT_PASS;
	struct mmc_host *mmc = host->mmc;
	struct mmc_command *cmd = mrq->cmd;
	struct mmc_data *data = mrq->data;

	init_completion(&mrq->completion); /* start request */
	mrq->done = mmc_wait_done;
	mrq->cmd->error = 0;
	mrq->cmd->mrq = mrq;
	mrq->cmd->data = data;
	if (data != NULL) {
		mrq->data->error = 0;
		mrq->data->mrq = mrq;
	}
	mmc->ops->request(mmc, mrq);

	wait_for_completion(&mrq->completion); /* wait for request done */

	if (cmd->error) { /*error decode */
		AUTOK_DBGPRINT(AUTOK_DBG_TRACE, "[ERROR]cmd->error : %d\n", cmd->error);
		if (cmd->error == (unsigned int)(-ETIMEDOUT))
			ret |= E_RESULT_CMD_TMO;
		if (cmd->error == (unsigned int)(-EILSEQ))
			ret |= E_RESULT_RSP_CRC;
	}
	if ((data != NULL) && (data->error)) {
		AUTOK_DBGPRINT(AUTOK_DBG_TRACE, "[ERROR]data->error : %d\n", data->error);
		if (data->error == (unsigned int)(-EILSEQ))
			ret |= E_RESULT_DAT_CRC;
		if (data->error == (unsigned int)(-ETIMEDOUT))
			ret |= E_RESULT_DAT_TMO;
	}
	/*
	 * if (ret)
	 *	AUTOK_RAWPRINT("[ERROR]%s error code :0x%x\n", __func__, ret);
	 */

	return ret;

}

static int autok_f0_rw_direct(struct msdc_host *host, unsigned int u4Addr, char *out, char in,
			      bool write)
{
	struct mmc_request mrq = { NULL };
	struct mmc_command cmd = { 0 };
	int ret = E_RESULT_PASS;

	if (u4Addr & ~0x1FFFF)
		return -EINVAL;
	cmd.opcode = SD_IO_RW_DIRECT;
	cmd.arg = write ? 0x80000000 : 0x00000000;
	cmd.arg |= 0 << 28; /* function number = 0 */
	cmd.arg |= (write && out) ? 0x08000000 : 0x00000000; /* read after write flage */
	cmd.arg |= u4Addr << 9;
	cmd.arg |= in;
	cmd.flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_AC;

	cmd.retries = 0;
	cmd.data = NULL;
	mrq.cmd = &cmd;

	ret = autok_wait_for_req(host, &mrq);
	if (out)
		*out = cmd.resp[0] & 0xFF;

	return ret;
}

static int
autok_f0_rw_extended(struct msdc_host *host, unsigned int u4Addr, void *pBuf, unsigned int byte_cnt,
		     bool write, bool opmode, bool bkmode)
{
	struct mmc_request mrq = { NULL };
	struct mmc_command cmd = { 0 };
	struct mmc_data data = { 0 };
	struct scatterlist sg;
	int ret = E_RESULT_PASS;

	if (host == NULL) {
		pr_debug("[%s] [ERR]host = %p\n", __func__, host);
		return -1;
	}
	if (pBuf == NULL)
		pr_err("[%s] [ERR]host = %p, memory malloc failed!!\n", __func__, host);

	/* Setup mrq */
	mrq.cmd = &cmd;
	mrq.data = &data;

	/* Setup cmd */
	cmd.opcode = SD_IO_RW_EXTENDED;
	cmd.arg = 0;
	cmd.arg = write ? 0x80000000 : 0x00000000;
	cmd.arg |= 0 << 28; /* Function num = 0 */
	cmd.arg |= u4Addr << 9;
	cmd.arg |= opmode << 26; /* 0-fix addr 1-inc addr */
	cmd.arg |= bkmode << 27; /* 0- byte 1-block */
	cmd.arg |= byte_cnt;
	cmd.flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_ADTC;
	cmd.data = &data;

	data.flags = write ? MMC_DATA_WRITE : MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	data.blksz = byte_cnt;
	data.blocks = 1;

	sg_init_one(&sg, pBuf, byte_cnt);

	ret = autok_wait_for_req(host, &mrq);

	return ret;
}
#endif

static int autok_send_tune_cmd(struct msdc_host *host, unsigned int opcode, enum TUNE_TYPE tune_type_value)
{
	void __iomem *base = host->base;
	unsigned int value;
	unsigned int rawcmd = 0;
	unsigned int arg = 0;
	unsigned int sts = 0;
	unsigned int wints = 0;
	unsigned int tmo = 0;
	unsigned int left = 0;
	unsigned int fifo_have = 0;
	unsigned int fifo_1k_cnt = 0;
	unsigned int i = 0;
	int ret = E_RESULT_PASS;

	switch (opcode) {
	case MMC_SEND_EXT_CSD:
		rawcmd =  (512 << 16) | (0 << 13) | (1 << 11) | (1 << 7) | (8);
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
	case MMC_READ_SINGLE_BLOCK:
		left = 512;
		rawcmd =  (512 << 16) | (0 << 13) | (1 << 11) | (1 << 7) | (17);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			MSDC_WRITE32(SDC_BLK_NUM, host->tune_latch_ck_cnt);
		else
			MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case MMC_SEND_TUNING_BLOCK:
		left = 64;
		rawcmd =  (64 << 16) | (0 << 13) | (1 << 11) | (1 << 7) | (19);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			MSDC_WRITE32(SDC_BLK_NUM, host->tune_latch_ck_cnt);
		else
			MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case MMC_SEND_TUNING_BLOCK_HS200:
		left = 128;
		rawcmd =  (128 << 16) | (0 << 13) | (1 << 11) | (1 << 7) | (21);
		arg = 0;
		if (tune_type_value == TUNE_LATCH_CK)
			MSDC_WRITE32(SDC_BLK_NUM, host->tune_latch_ck_cnt);
		else
			MSDC_WRITE32(SDC_BLK_NUM, 1);
		break;
	case MMC_WRITE_BLOCK:
		rawcmd =  (512 << 16) | (1 << 13) | (1 << 11) | (1 << 7) | (24);
		if (TUNE_DATA_TX_ADDR >= 0)
			arg = TUNE_DATA_TX_ADDR;
		else
			arg = host->mmc->card->ext_csd.sectors
				+ TUNE_DATA_TX_ADDR;
		break;
	case SD_IO_RW_DIRECT:
		break;
	case SD_IO_RW_EXTENDED:
		break;
	}

	while ((MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY))
		;

	/* clear fifo */
	if ((tune_type_value == TUNE_CMD) || (tune_type_value == TUNE_DATA)) {
		autok_msdc_reset();
		msdc_clear_fifo();
		MSDC_WRITE32(MSDC_INT, 0xffffffff);
	}

	/* start command */
	MSDC_WRITE32(SDC_ARG, arg);
	MSDC_WRITE32(SDC_CMD, rawcmd);

	/* wait interrupt status */
	wints = MSDC_INT_CMDTMO | MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR;
	tmo = 0x3FFFFF;
	wait_cond(((sts = MSDC_READ32(MSDC_INT)) & wints), tmo, tmo);
	if (tmo == 0) {
		ret = E_RESULT_CMD_TMO;
		goto end;
	}

	MSDC_WRITE32(MSDC_INT, (sts & wints));
	if (sts == 0) {
		ret = E_RESULT_CMD_TMO;
		goto end;
	}

	if (sts & MSDC_INT_CMDRDY) {
		if (tune_type_value == TUNE_CMD) {
			ret = E_RESULT_PASS;
			goto end;
		}
	} else if (sts & MSDC_INT_RSPCRCERR) {
		ret = E_RESULT_RSP_CRC;
		goto end;
	} else if (sts & MSDC_INT_CMDTMO) {
		ret = E_RESULT_CMD_TMO;
		goto end;
	}
	if ((tune_type_value != TUNE_LATCH_CK) && (tune_type_value != TUNE_DATA))
		goto skip_tune_latch_ck_and_tune_data;

	while ((MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY)) {
		if (tune_type_value == TUNE_LATCH_CK) {
			fifo_have = msdc_rxfifocnt();
			if ((opcode == MMC_SEND_TUNING_BLOCK_HS200) || (opcode == MMC_READ_SINGLE_BLOCK)
				|| (opcode == MMC_SEND_EXT_CSD)) {
				MSDC_SET_FIELD(MSDC_DBG_SEL, 0xffff << 0, 0x0b);
				MSDC_GET_FIELD(MSDC_DBG_OUT, 0x7ff << 0, fifo_1k_cnt);
				if ((fifo_1k_cnt >= MSDC_FIFO_THD_1K) && (fifo_have >= MSDC_FIFO_SZ)) {
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
				}
			} else if (opcode == MMC_SEND_TUNING_BLOCK) {
				if (fifo_have >= MSDC_FIFO_SZ) {
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
					value = MSDC_READ32(MSDC_RXDATA);
				}
			}
		} else if ((tune_type_value == TUNE_DATA) && (opcode == MMC_WRITE_BLOCK)) {
			for (i = 0; i < 64; i++) {
				MSDC_WRITE32(MSDC_TXDATA, 0x5af00fa5);
				MSDC_WRITE32(MSDC_TXDATA, 0x33cc33cc);
			}

			while ((MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY))
				;
		}
	}

	sts = MSDC_READ32(MSDC_INT);
	wints = MSDC_INT_XFER_COMPL | MSDC_INT_DATCRCERR | MSDC_INT_DATTMO;
	if (sts) {
		/* clear status */
		MSDC_WRITE32(MSDC_INT, (sts & wints));
		if (sts & MSDC_INT_XFER_COMPL)
			ret = E_RESULT_PASS;
		if (MSDC_INT_DATCRCERR & sts)
			ret = E_RESULT_DAT_CRC;
		if (MSDC_INT_DATTMO & sts)
			ret = E_RESULT_DAT_TMO;
	}

skip_tune_latch_ck_and_tune_data:
	while ((MSDC_READ32(SDC_STS) & SDC_STS_SDCBUSY))
		;
	if ((tune_type_value == TUNE_CMD) || (tune_type_value == TUNE_DATA))
		msdc_clear_fifo();

end:
	if (opcode == MMC_STOP_TRANSMISSION) {
		while ((MSDC_READ32(MSDC_PS) & 0x10000) != 0x10000)
			;
	}

	return ret;
}

#if defined(SDIO_TUNE_WRITE_PATH)
static int autok_cmd53_tune_cccr_write(struct msdc_host *host)
{
	int ret = 0;
	unsigned int wDat = 0x5A;
	unsigned int test_cnt = 10;
	int i = 0;

	do {
		i++;

		ret = autok_f0_rw_extended(host, 0xE0, &wDat, 4, 1, 0, 0);
		if (ret != 0) {
			AUTOK_RAWPRINT("[ERROR]CMD53(%d) Write CCCR0 Error: 0x%x\n", i, ret);

			/* abort data transfer don't care response */
			autok_f0_rw_direct(host, 0x6, NULL, 0, 1);
			return ret;
		}
	} while (--test_cnt);

	return ret;
}
#endif

#if 0
static int autok_cmd53_tune_cccr_read(struct msdc_host *host)
{
	int ret = 0;
	unsigned char *rDat = NULL;
	unsigned int test_cnt = 10;
	int i = 0;

	rDat = kmalloc(32, GFP_KERNEL);
	do {
		i++;

		ret = autok_f0_rw_extended(host, SDIO_CCCR_CCCR, rDat, 32, 0, 0, 0);
		if (ret != 0) {
			AUTOK_RAWPRINT("[ERROR]CMD53(%d) Read CCCR0 Error: 0x%x\n", i, ret);
			return ret;
		}
	} while (--test_cnt);
	kfree(rDat);

	return ret;
}
#endif

static int autok_simple_score(char *res_str, unsigned int result)
{
	unsigned int bit = 0;
	unsigned int num = 0;
	unsigned int old = 0;

	if (result == 0) {
		strcpy(res_str, "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO");	/* maybe result is 0 */
		return 32;
	}
	if (result == 0xFFFFFFFF) {
		strcpy(res_str, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
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

	return old;
}

static int autok_simple_score64(char *res_str64, u64 result64)
{
	unsigned int bit = 0;
	unsigned int num = 0;
	unsigned int old = 0;

	if (result64 == 0) {
		/* maybe result is 0 */
		strcpy(res_str64, "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO");
		return 64;
	}
	if (result64 == 0xFFFFFFFFFFFFFFFF) {
		strcpy(res_str64,
		       "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
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

	return old;
}

enum {
	RD_SCAN_NONE,
	RD_SCAN_PAD_BOUND_S,
	RD_SCAN_PAD_BOUND_E,
	RD_SCAN_PAD_MARGIN,
};

static int autok_check_scan_res64(u64 rawdat, struct AUTOK_SCAN_RES *scan_res)
{
	unsigned int bit;
	unsigned int filter = 4;
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
				if ((filter) && ((bit - pBD->Bound_End) <= AUTOK_TUNING_INACCURACY)) {
					AUTOK_DBGPRINT(AUTOK_DBG_TRACE,
					       "[AUTOK]WARN: Try to filter the holes on raw data \r\n");
					RawScanSta = RD_SCAN_PAD_BOUND_S;

					pBD->Bound_width += (bit - pBD->Bound_End);
					pBD->Bound_End = 0;
					filter--;

					/* update full bound info */
					if (pBD->is_fullbound) {
						pBD->is_fullbound = 0;
						scan_res->fbd_cnt -= 1;
					}
				} else {
					/* No filter Check and Get the next boundary information */
					RawScanSta = RD_SCAN_PAD_BOUND_S;
					pBD++;
					pBD->Bound_Start = bit;
					pBD->Bound_width = 1;
					scan_res->bd_cnt += 1;
					if (scan_res->bd_cnt > BD_MAX_CNT) {
						AUTOK_RAWPRINT
						    ("[AUTOK]Error: more than %d Boundary Exist\r\n",
						     BD_MAX_CNT);
						return -1;
					}
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

	return 0;
}

static int autok_calc_bit_cnt(unsigned int result)
{
	unsigned int cnt = 0;

	if (result == 0)
		return 0;
	else if (result == 0xFFFFFFFF)
		return 32;

	do {
		if (result & 0x1)
			cnt++;

		result = result >> 1;
	} while (result);

	return cnt;
}

static int autok_ta_sel(u32 *raw_list, u32 *pmid)
{
	unsigned int i = 0;
	unsigned int raw = 0;
	unsigned int r_start = 0; /* range start */
	unsigned int r_stop = 7; /* range stop */
	unsigned int score = 0;
	unsigned int score_max = 0;
	unsigned int score_max_id = 0;
	unsigned int inaccuracy = TUNING_INACCURACY;

	for (i = 0; i <= 7; i++) {
		score = autok_calc_bit_cnt(raw_list[i] ^ 0xFFFFFFFF);
		if (score > score_max) {
			score_max = score;
			score_max_id = i;
		}
	}
	if (score_max < 5)
		return -1;

	raw = raw_list[score_max_id];
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]The maximum offset is %d\r\n",
		score_max_id);
FIND:
	for (i = 0; i <= 7; i++) {
		if (autok_calc_bit_cnt(raw_list[i] ^ raw) <= inaccuracy) {
			r_start = i;
			break;
		}
	}
	for (i = 7; i >= 0; i--) {
		if (autok_calc_bit_cnt(raw_list[i] ^ raw) <= inaccuracy) {
			r_stop = i;
			break;
		}
	}
	if ((r_start + 2) <= r_stop) {
		/* At least get the TA of which the margin has either left 1T and right 1T */
		*pmid = (r_start + r_stop + 1) / 2;
	} else {
		inaccuracy++;
		if (inaccuracy < 5) {
			AUTOK_DBGPRINT(AUTOK_DBG_RES, "Enlarge the inaccuracy[%d]\r\n", inaccuracy);
			goto FIND;
		}
	}
	if (*pmid) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]Find suitable range[%d %d], TA_sel=%d\r\n",
			       r_start, r_stop, *pmid);
	} else {
		*pmid = score_max_id;
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "Un-expected pattern, pls check!, TA_sel=%d\r\n",
			       *pmid);
	}

	return 0;
}

static int autok_pad_dly_sel(struct AUTOK_REF_INFO *pInfo)
{
	struct AUTOK_SCAN_RES *pBdInfo_R = NULL; /* scan result @ rising edge */
	struct AUTOK_SCAN_RES *pBdInfo_F = NULL; /* scan result @ falling edge */
	struct BOUND_INFO *pBdPrev = NULL; /* Save the first boundary info for calc optimised dly count */
	struct BOUND_INFO *pBdNext = NULL; /* Save the second boundary info for calc optimised dly count */
	struct BOUND_INFO *pBdTmp = NULL;
	unsigned int FBound_Cnt_R = 0;	/* Full Boundary count */
	unsigned int Bound_Cnt_R = 0;
	unsigned int Bound_Cnt_F = 0;
	unsigned int cycle_cnt = 64;
	int uBD_mid_prev = 0;
	int uBD_mid_next = 0;
	int uBD_width = 3;
	int uDlySel_F = 0;
	int uDlySel_R = 0;
	int uMgLost_F = 0; /* for falling edge margin compress */
	int uMgLost_R = 0; /* for rising edge margin compress */
	unsigned int i;
	unsigned int ret = 0;

	pBdInfo_R = &(pInfo->scan_info[0]);
	pBdInfo_F = &(pInfo->scan_info[1]);
	FBound_Cnt_R = pBdInfo_R->fbd_cnt;
	Bound_Cnt_R = pBdInfo_R->bd_cnt;
	Bound_Cnt_F = pBdInfo_F->bd_cnt;

	switch (FBound_Cnt_R) {
	case 4:	/* SSSS Corner may cover 2~3T */
	case 3:
		AUTOK_RAWPRINT("[ATUOK]Warning: Too Many Full boundary count:%d\r\n", FBound_Cnt_R);
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
			uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
			uBD_mid_next = (pBdNext->Bound_Start + pBdNext->Bound_End) / 2;
			/* while in 2 full bound case, bd_width calc */
			uBD_width = (pBdPrev->Bound_width + pBdNext->Bound_width) / 2;
			cycle_cnt = uBD_mid_next - uBD_mid_prev;
			/* delay count sel at rising edge */
			if (uBD_mid_prev >= cycle_cnt / 2) {
				uDlySel_R = uBD_mid_prev - cycle_cnt / 2;
				uMgLost_R = 0;
			} else if ((cycle_cnt / 2 - uBD_mid_prev) > AUTOK_MARGIN_THOLD) {
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
					uDlySel_F = (pBdTmp->Bound_End) - (uBD_width / 2);
					uMgLost_F = 0;
				} else {
					uDlySel_F = 0;
					uMgLost_F = (uBD_width / 2) - (pBdTmp->Bound_End);
				}
			}
		} else {
			/* error can not find 2 foull boary */
			AUTOK_RAWPRINT("[AUTOK]error can not find 2 foull boudary @ Mode_1");
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

			if ((pBdPrev->is_fullbound) || (pBdNext->is_fullbound)) {
				if (pBdPrev->Bound_Start > 0)
					cycle_cnt = pBdNext->Bound_Start - pBdPrev->Bound_Start;
				else
					cycle_cnt = pBdNext->Bound_End - pBdPrev->Bound_End;

				/* delay count sel@rising & falling edge */
				if (pBdPrev->is_fullbound) {
					uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
					uDlySel_F = uBD_mid_prev;
					uMgLost_F = 0;
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
				} else {
					/* first boundary not full boudary */
					uBD_mid_next = (pBdNext->Bound_Start + pBdNext->Bound_End) / 2;
					uDlySel_R = uBD_mid_next - cycle_cnt / 2;
					uMgLost_R = 0;
					if (pBdPrev->Bound_End > uBD_width / 2) {
						uDlySel_F = (pBdPrev->Bound_End) - (uBD_width / 2);
						uMgLost_F = 0;
					} else {
						uDlySel_F = 0;
						uMgLost_F = (uBD_width / 2) - (pBdPrev->Bound_End);
					}
				}
			} else {
				return -1; /* full bound must in first 2 boundary */
			}
		} else if (Bound_Cnt_F > 0) {
			/* mode_3: 1 full boundary and only one boundary exist @rising edge */
			pBdPrev = &(pBdInfo_R->bd_info[0]); /* this boundary is full bound */
			pBdNext = &(pBdInfo_F->bd_info[0]);
			uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
			uBD_width = pBdPrev->Bound_width;

			if (pBdNext->Bound_Start == 0) {
				cycle_cnt = (pBdPrev->Bound_End - pBdNext->Bound_End) * 2;
			} else if (pBdNext->Bound_End == 63) {
				cycle_cnt = (pBdNext->Bound_Start - pBdPrev->Bound_Start) * 2;
			} else {
				uBD_mid_next = (pBdNext->Bound_Start + pBdNext->Bound_End) / 2;

				if (uBD_mid_next > uBD_mid_prev)
					cycle_cnt = (uBD_mid_next - uBD_mid_prev) * 2;
				else
					cycle_cnt = (uBD_mid_prev - uBD_mid_next) * 2;
			}

			uDlySel_F = uBD_mid_prev;
			uMgLost_F = 0;

			if (uBD_mid_prev >= cycle_cnt / 2) { /* case 1 */
				uDlySel_R = uBD_mid_prev - cycle_cnt / 2;
				uMgLost_R = 0;
			} else if (cycle_cnt / 2 - uBD_mid_prev <= AUTOK_MARGIN_THOLD) { /* case 2 */
				uDlySel_R = 0;
				uMgLost_R = cycle_cnt / 2 - uBD_mid_prev;
			} else if (cycle_cnt / 2 + uBD_mid_prev <= 63) { /* case 3 */
				uDlySel_R = cycle_cnt / 2 + uBD_mid_prev;
				uMgLost_R = 0;
			} else if (32 - uBD_mid_prev <= AUTOK_MARGIN_THOLD) { /* case 4 */
				uDlySel_R = 0;
				uMgLost_R = cycle_cnt / 2 - uBD_mid_prev;
			} else { /* case 5 */
				uDlySel_R = 63;
				uMgLost_R = uBD_mid_prev + cycle_cnt / 2 - 63;
			}
		} else {
			/* mode_4: falling edge no boundary found & rising edge only one full boundary exist */
			pBdPrev = &(pBdInfo_R->bd_info[0]);	/* this boundary is full bound */
			uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
			uBD_width = pBdPrev->Bound_width;

			if (pBdPrev->Bound_End > (64 - pBdPrev->Bound_Start))
				cycle_cnt = 2 * (pBdPrev->Bound_End + 1);
			else
				cycle_cnt = 2 * (64 - pBdPrev->Bound_Start);

			uDlySel_R = (uBD_mid_prev >= 32) ? 0 : 63;
			uMgLost_R = 0xFF; /* Margin enough donot care margin lost */
			uDlySel_F = uBD_mid_prev;
			uMgLost_F = 0xFF; /* Margin enough donot care margin lost */

			AUTOK_RAWPRINT("[AUTOK]Warning: 1T > %d\r\n", cycle_cnt);
		}
		break;

	case 0:	/* rising edge cannot find full boudary */
		if (Bound_Cnt_R == 2) {
			pBdPrev = &(pBdInfo_R->bd_info[0]);
			pBdNext = &(pBdInfo_F->bd_info[0]); /* this boundary is full bound */

			if (pBdNext->is_fullbound) {
				/* mode_5: rising_edge 2 boundary (not full bound), falling edge 1 full boundary */
				uBD_width = pBdNext->Bound_width;
				cycle_cnt = 2 * (pBdNext->Bound_End - pBdPrev->Bound_End);
				uBD_mid_next = (pBdNext->Bound_Start + pBdNext->Bound_End) / 2;
				uDlySel_R = uBD_mid_next;
				uMgLost_R = 0;
				if (pBdPrev->Bound_End >= uBD_width / 2) {
					uDlySel_F = pBdPrev->Bound_End - uBD_width / 2;
					uMgLost_F = 0;
				} else {
					uDlySel_F = 0;
					uMgLost_F = uBD_width / 2 - pBdPrev->Bound_End;
				}
			} else {
				/* for falling edge there must be one full boundary between two bounary_mid at rising */
				return -1;
			}
		} else if (Bound_Cnt_R == 1) {
			if (Bound_Cnt_F > 1) {
				/* when rising_edge have only one boundary (not full bound),
				 * falling edge should not more than 1Bound exist
				 */
				return -1;
			} else if (Bound_Cnt_F == 1) {
				/* mode_6: rising edge only 1 boundary (not full Bound)
				 * & falling edge have only 1 bound too
				 */
				pBdPrev = &(pBdInfo_R->bd_info[0]);
				pBdNext = &(pBdInfo_F->bd_info[0]);
				if (pBdNext->is_fullbound) {
					uBD_width = pBdNext->Bound_width;
				} else {
					if (pBdNext->Bound_width > pBdPrev->Bound_width)
						uBD_width = (pBdNext->Bound_width + 1);
					else
						uBD_width = (pBdPrev->Bound_width + 1);

					if (uBD_width < AUTOK_BD_WIDTH_REF)
						uBD_width = AUTOK_BD_WIDTH_REF;
				} /* Boundary width calc done */

				if (pBdPrev->Bound_Start == 0) {
					/* Current Desing Not Allowed */
					if (pBdNext->Bound_Start == 0)
						return -1;

					cycle_cnt =
					    (pBdNext->Bound_Start - pBdPrev->Bound_End +
					     uBD_width) * 2;
				} else if (pBdPrev->Bound_End == 63) {
					/* Current Desing Not Allowed */
					if (pBdNext->Bound_End == 63)
						return -1;

					cycle_cnt =
					    (pBdPrev->Bound_Start - pBdNext->Bound_End +
					     uBD_width) * 2;
				} /* cycle count calc done */

				/* calc optimise delay count */
				if (pBdPrev->Bound_Start == 0) {
					/* falling edge sel */
					if (pBdPrev->Bound_End >= uBD_width / 2) {
						uDlySel_F = pBdPrev->Bound_End - uBD_width / 2;
						uMgLost_F = 0;
					} else {
						uDlySel_F = 0;
						uMgLost_F = uBD_width / 2 - pBdPrev->Bound_End;
					}

					/* rising edge sel */
					if (pBdPrev->Bound_End - uBD_width / 2 + cycle_cnt / 2 > 63) {
						uDlySel_R = 63;
						uMgLost_R =
						    pBdPrev->Bound_End - uBD_width / 2 +
						    cycle_cnt / 2 - 63;
					} else {
						uDlySel_R =
						    pBdPrev->Bound_End - uBD_width / 2 +
						    cycle_cnt / 2;
						uMgLost_R = 0;
					}
				} else if (pBdPrev->Bound_End == 63) {
					/* falling edge sel */
					if (pBdPrev->Bound_Start + uBD_width / 2 < 63) {
						uDlySel_F = pBdPrev->Bound_Start + uBD_width / 2;
						uMgLost_F = 0;
					} else {
						uDlySel_F = 63;
						uMgLost_F =
						    pBdPrev->Bound_Start + uBD_width / 2 - 63;
					}

					/* rising edge sel */
					if (pBdPrev->Bound_Start + uBD_width / 2 - cycle_cnt / 2 < 0) {
						uDlySel_R = 0;
						uMgLost_R =
						    cycle_cnt / 2 - (pBdPrev->Bound_Start +
								     uBD_width / 2);
					} else {
						uDlySel_R =
						    pBdPrev->Bound_Start + uBD_width / 2 -
						    cycle_cnt / 2;
						uMgLost_R = 0;
					}
				} else {
					return -1;
				}
			} else if (Bound_Cnt_F == 0) {
				/* mode_7: rising edge only one bound (not full), falling no boundary */
				cycle_cnt = 128;
				pBdPrev = &(pBdInfo_R->bd_info[0]);
				if (pBdPrev->Bound_Start == 0) {
					uDlySel_F = 0;
					uDlySel_R = 63;
				} else if (pBdPrev->Bound_End == 63) {
					uDlySel_F = 63;
					uDlySel_R = 0;
				} else {
					return -1;
				}
				uMgLost_F = 0xFF;
				uMgLost_R = 0xFF;

				AUTOK_RAWPRINT("[AUTOK]Warning: 1T > %d\r\n", cycle_cnt);
			}
		} else if (Bound_Cnt_R == 0) { /* Rising Edge No Boundary found */
			if (Bound_Cnt_F > 1) {
				/* falling edge not allowed two boundary Exist for this case */
				return -1;
			} else if (Bound_Cnt_F > 0) {
				/* mode_8: falling edge have one Boundary exist */
				pBdPrev = &(pBdInfo_F->bd_info[0]);

				/* this boundary is full bound */
				if (pBdPrev->is_fullbound) {
					uBD_mid_prev =
					    (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;

					if (pBdPrev->Bound_End > (64 - pBdPrev->Bound_Start))
						cycle_cnt = 2 * (pBdPrev->Bound_End + 1);
					else
						cycle_cnt = 2 * (64 - pBdPrev->Bound_Start);

					uDlySel_R = uBD_mid_prev;
					uMgLost_R = 0xFF;
					uDlySel_F = (uBD_mid_prev >= 32) ? 0 : 63;
					uMgLost_F = 0xFF;
				} else {
					cycle_cnt = 128;

					uDlySel_R = (pBdPrev->Bound_Start == 0) ? 0 : 63;
					uMgLost_R = 0xFF;
					uDlySel_F = (pBdPrev->Bound_Start == 0) ? 63 : 0;
					uMgLost_F = 0xFF;
				}

				AUTOK_RAWPRINT("[AUTOK]Warning: 1T > %d\r\n", cycle_cnt);
			} else {
				/* falling edge no boundary exist no need tuning */
				cycle_cnt = 128;
				uDlySel_F = 0;
				uMgLost_F = 0xFF;
				uDlySel_R = 0;
				uMgLost_R = 0xFF;
				AUTOK_RAWPRINT("[AUTOK]Warning: 1T > %d\r\n", cycle_cnt);
			}
		} else {
			/* Error if bound_cnt > 3 there must be at least one full boundary exist */
			return -1;
		}
		break;

	default:
		/* warning if boundary count > 4 (from current hw design, this case cannot happen) */
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
	AUTOK_RAWPRINT("[AUTOK]Analysis Result: 1T = %d\r\n ", cycle_cnt);
	return ret;
}

static int
autok_pad_dly_sel_single_edge(struct AUTOK_SCAN_RES *pInfo, unsigned int cycle_cnt_ref,
			      unsigned int *pDlySel)
{
	struct BOUND_INFO *pBdPrev = NULL; /* Save the first boundary info for calc optimised dly count */
	struct BOUND_INFO *pBdNext = NULL; /* Save the second boundary info for calc optimised dly count */
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
			uDlySel = (pBdPrev->Bound_End + pBdNext->Bound_Start) / 2;
			uMgLost = (uDlySel > 31) ? (uDlySel - 31) : 0;
			uDlySel = (uDlySel > 31) ? 31 : uDlySel;

		} else {
			/* mode_2: at least 2 Bound found and Bound0_Start != 0 */
			uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
			if (uBD_mid_prev >= cycle_cnt_ref / 2) {
				uDlySel = uBD_mid_prev - cycle_cnt_ref / 2;
				uMgLost = 0;
			} else if (cycle_cnt_ref / 2 - uBD_mid_prev < AUTOK_MARGIN_THOLD) {
				uDlySel = 0;
				uMgLost = cycle_cnt_ref / 2 - uBD_mid_prev;
			} else {
				uDlySel = (pBdPrev->Bound_End + pBdNext->Bound_Start) / 2;
				if ((uDlySel > 31) && (uDlySel - 31 < AUTOK_MARGIN_THOLD)) {
					uDlySel = 31;
					uMgLost = uDlySel - 31;
				} else {
					uDlySel = uDlySel;
					uMgLost = 0;
				}
			}
		}
	} else if (Bound_Cnt > 0) {
		/* only one bound fond */
		pBdPrev = &(pInfo->bd_info[0]);
		if (pBdPrev->is_fullbound) {
			/* mode_3: Bound_S != 0 */
			uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
			if (uBD_mid_prev >= cycle_cnt_ref / 2) {
				uDlySel = uBD_mid_prev - cycle_cnt_ref / 2;
				uMgLost = 0;
			} else if (cycle_cnt_ref / 2 - uBD_mid_prev < AUTOK_MARGIN_THOLD) {
				uDlySel = 0;
				uMgLost = cycle_cnt_ref / 2 - uBD_mid_prev;
			} else if ((uBD_mid_prev > 31 - AUTOK_MARGIN_THOLD)
				   || (pBdPrev->Bound_Start >= 16)) {
				uDlySel = 0;
				uMgLost = cycle_cnt_ref / 2 - uBD_mid_prev;
			} else if (uBD_mid_prev + cycle_cnt_ref / 2 <= 63) {
				/* Left Margin not enough must need to select the right side */
				uDlySel = uBD_mid_prev + cycle_cnt_ref / 2;
				uMgLost = 0;
			} else {
				uDlySel = 63;
				uMgLost = uBD_mid_prev + cycle_cnt_ref / 2 - 63;
			}
		} else if (pBdPrev->Bound_Start == 0) {
			/* mode_4 : Only one Boud and Boud_S = 0  (Currently 1T nearly equal 64 ) */

			/* May not exactly by for Cycle_Cnt enough can don't care */
			uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
			if (pBdPrev->Bound_Start + cycle_cnt_ref / 2 >= 31) {
				uDlySel = 31;
				uMgLost = uBD_mid_prev + cycle_cnt_ref / 2 - 31;
			} else {
				uDlySel = uBD_mid_prev + cycle_cnt_ref / 2;
				uMgLost = 0;
			}
		} else {
			/* mode_5: Only one Boud and Boud_E = 64 */

			/* May not exactly by for Cycle_Cnt enough can don't care */
			uBD_mid_prev = (pBdPrev->Bound_Start + pBdPrev->Bound_End) / 2;
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
	} else { /*mode_6: no bound foud */
		uDlySel = 31;
		uMgLost = 0xFF;
	}
	*pDlySel = uDlySel;
	if (uDlySel > 31) {
		AUTOK_RAWPRINT
		    ("[AUTOK]Warning Dly Sel %d > 31 easily effected by Voltage Swing\r\n",
		     uDlySel);
	}

	return ret;
}

static int autok_ds_dly_sel(struct AUTOK_SCAN_RES *pInfo, unsigned int cycle_value, unsigned int *pDlySel)
{
	unsigned int ret = 0;
	int uDlySel = 0;
	unsigned int Bound_Cnt = pInfo->bd_cnt;

	if (pInfo->fbd_cnt > 0) {
		/* no more than 2 boundary exist */
		AUTOK_RAWPRINT("[AUTOK]Error: Scan DS Not allow Full boundary Occurs!\r\n");
		return -1;
	}

	if (Bound_Cnt > 1) {
		/* bound count == 2 */
		uDlySel = (pInfo->bd_info[0].Bound_End + pInfo->bd_info[1].Bound_Start) / 2;
	} else if (Bound_Cnt > 0) {
		/* bound count == 1 */
		if (pInfo->bd_info[0].Bound_Start == 0) {
			if (pInfo->bd_info[0].Bound_End > 31) {
				uDlySel = (pInfo->bd_info[0].Bound_End + 64) / 2;
				AUTOK_RAWPRINT("[AUTOK]Warning: DS Delay not in range 0~31!\r\n");
			} else {
				uDlySel = (pInfo->bd_info[0].Bound_End + 31) / 2;
			}
		} else {
			/* uDlySel = pInfo->bd_info[0].Bound_Start / 2; */
			uDlySel = (((pInfo->bd_info[0].Bound_Start * 5000) / cycle_value)
				- (1250 - 400)) * cycle_value / 5000;
		}
	} else {
		/* bound count == 0 */
		uDlySel = 16;
	}
	*pDlySel = uDlySel;

	return ret;
}

/*************************************************************************
 * FUNCTION
 *  msdc_autok_adjust_param
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
static int msdc_autok_adjust_param(struct msdc_host *host, enum AUTOK_PARAM param, u32 *value,
				   int rw)
{
	void __iomem *base = host->base;
	u32 *reg;
	u32 field = 0;

	switch (param) {
	case READ_DATA_SMPL_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for READ_DATA_SMPL_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}

		reg = (u32 *) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_R_D_SMPL_SEL);
		break;
	case WRITE_DATA_SMPL_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for WRITE_DATA_SMPL_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}

		reg = (u32 *) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_W_D_SMPL_SEL);
		break;
	case DATA_DLYLINE_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for DATA_DLYLINE_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}

		reg = (u32 *) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_DDLSEL);
		break;
	case MSDC_DAT_TUNE_SEL:	/* 0-Dat tune 1-CLk tune ; */
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for DATA_DLYLINE_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PAD_TUNE0;
		field = (u32) (MSDC_PAD_TUNE0_RXDLYSEL);
		break;
	case MSDC_WCRC_ASYNC_FIFO_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for DATA_DLYLINE_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT2;
		field = (u32) (MSDC_PB2_CFGCRCSTS);
		break;
	case MSDC_RESP_ASYNC_FIFO_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for DATA_DLYLINE_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT2;
		field = (u32) (MSDC_PB2_CFGRESP);
		break;
	case CMD_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_EDGE is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_RSPL);
		break;
	case RDATA_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for RDATA_EDGE is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_IOCON;
		field = (u32) (MSDC_IOCON_R_D_SMPL);
		break;
	case RD_FIFO_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for RDATA_EDGE is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT0;
		field = (u32) (MSDC_PB0_RD_DAT_SEL);
		break;
	case WD_FIFO_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for WDATA_EDGE is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT2;
		field = (u32) (MSDC_PB2_CFGCRCSTSEDGE);
		break;
	case CMD_RD_D_DLY1:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_RD_DLY is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PAD_TUNE0;
		field = (u32) (MSDC_PAD_TUNE0_CMDRDLY);
		break;
	case CMD_RD_D_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_RD_DLY is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PAD_TUNE0;
		field = (u32) (MSDC_PAD_TUNE0_CMDRRDLYSEL);
		break;
	case CMD_RD_D_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_RD_DLY is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PAD_TUNE1;
		field = (u32) (MSDC_PAD_TUNE1_CMDRDLY2);
		break;
	case CMD_RD_D_DLY2_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_RD_DLY is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PAD_TUNE1;
		field = (u32) (MSDC_PAD_TUNE1_CMDRRDLY2SEL);
		break;
	case DAT_RD_D_DLY1:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for DAT0_RD_DLY is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PAD_TUNE0;
		field = (u32) (MSDC_PAD_TUNE0_DATRRDLY);
		break;
	case DAT_RD_D_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_RD_DLY is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PAD_TUNE0;
		field = (u32) (MSDC_PAD_TUNE0_DATRRDLYSEL);
		break;
	case DAT_RD_D_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for DAT1_RD_DLY is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PAD_TUNE1;
		field = (u32) (MSDC_PAD_TUNE1_DATRRDLY2);
		break;
	case DAT_RD_D_DLY2_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_RD_DLY is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PAD_TUNE1;
		field = (u32) (MSDC_PAD_TUNE1_DATRRDLY2SEL);
		break;
	case INT_DAT_LATCH_CK:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug
			    ("[%s] Input value(%d) for INT_DAT_LATCH_CK is out of range, it should be [0~7]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT0;
		field = (u32) (MSDC_PB0_INT_DAT_LATCH_CK_SEL);
		break;
	case CKGEN_MSDC_DLY_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for CKGEN_MSDC_DLY_SEL is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT0;
		field = (u32) (MSDC_PB0_CKGEN_MSDC_DLY_SEL);
		break;
	case CMD_RSP_TA_CNTR:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug
			    ("[%s] Input value(%d) for CMD_RSP_TA_CNTR is out of range, it should be [0~7]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT1;
		field = (u32) (MSDC_PB1_CMD_RSP_TA_CNTR);
		break;
	case WRDAT_CRCS_TA_CNTR:
		if ((rw == AUTOK_WRITE) && (*value > 7)) {
			pr_debug
			    ("[%s] Input value(%d) for WRDAT_CRCS_TA_CNTR is out of range, it should be [0~7]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PATCH_BIT1;
		field = (u32) (MSDC_PB1_WRDAT_CRCS_TA_CNTR);
		break;
	case PAD_CLK_TXDLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for PAD_CLK_TXDLY is out of range, it should be [0~31]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) MSDC_PAD_TUNE0;
		field = (u32) (MSDC_PAD_TUNE0_CLKTXDLY);
		break;
	case EMMC50_WDATA_MUX_EN:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_WDATA_MUX_EN is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_CFG0;
		field = (u32) (MSDC_EMMC50_CFG_CRC_STS_SEL);
		break;
	case EMMC50_CMD_MUX_EN:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_CMD_MUX_EN is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_CFG0;
		field = (u32) (MSDC_EMMC50_CFG_CMD_RESP_SEL);
		break;
	case EMMC50_WDATA_EDGE:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_WDATA_EDGE is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_CFG0;
		field = (u32) (MSDC_EMMC50_CFG_CRC_STS_EDGE);
		break;
	case EMMC50_DS_Z_DLY1:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_DS_Z_DLY1 is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_PAD_DS_TUNE;
		field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY1);
		break;
	case EMMC50_DS_Z_DLY1_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_DS_Z_DLY1_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_PAD_DS_TUNE;
		field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLYSEL);
		break;
	case EMMC50_DS_Z_DLY2:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_DS_Z_DLY2 is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_PAD_DS_TUNE;
		field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY2);
		break;
	case EMMC50_DS_Z_DLY2_SEL:
		if ((rw == AUTOK_WRITE) && (*value > 1)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_DS_Z_DLY1_SEL is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_PAD_DS_TUNE;
		field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY2SEL);
		break;
	case EMMC50_DS_ZDLY_DLY:
		if ((rw == AUTOK_WRITE) && (*value > 31)) {
			pr_debug
			    ("[%s] Input value(%d) for EMMC50_DS_Z_DLY2 is out of range, it should be [0~1]\n",
			     __func__, *value);
			return -1;
		}
		reg = (u32 *) EMMC50_PAD_DS_TUNE;
		field = (u32) (MSDC_EMMC50_PAD_DS_TUNE_DLY3);
		break;
	default:
		pr_debug("[%s] Value of [enum AUTOK_PARAM param] is wrong\n", __func__);
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

static int autok_param_update(enum AUTOK_PARAM param_id, unsigned int result, u8 *autok_tune_res)
{
	if (param_id < TUNING_PARAM_COUNT) {
		if ((result > autok_param_info[param_id].range.end) ||
		    (result < autok_param_info[param_id].range.start)) {
			AUTOK_RAWPRINT("[AUTOK]param outof range : %d not in [%d,%d]\r\n",
				       result, autok_param_info[param_id].range.start,
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
		msdc_autok_adjust_param(host, i, &value, AUTOK_WRITE);
	}

	return 0;
}

static int autok_result_dump(struct msdc_host *host, u8 *autok_tune_res)
{
	AUTOK_RAWPRINT("[AUTOK]CMD [EDGE:%d DLY1:%d DLY2:%d ]\r\n",
		autok_tune_res[0], autok_tune_res[4], autok_tune_res[6]);
	AUTOK_RAWPRINT("[AUTOK]DAT [RDAT_EDGE:%d RD_FIFO_EDGE:%d WD_FIFO_EDGE:%d]\r\n",
		autok_tune_res[1], autok_tune_res[2], autok_tune_res[3]);
	AUTOK_RAWPRINT("[AUTOK]DAT [LATCH_CK:%d DLY1:%d DLY2:%d ]\r\n",
		autok_tune_res[12], autok_tune_res[8], autok_tune_res[10]);
	AUTOK_RAWPRINT("[AUTOK]DS  [DLY1:%d DLY2:%d DLY3:%d]\r\n",
		autok_tune_res[13], autok_tune_res[15], autok_tune_res[17]);

	return 0;
}

#if AUTOK_PARAM_DUMP_ENABLE
static int autok_register_dump(struct msdc_host *host)
{
	unsigned int i = 0;
	unsigned int value = 0;

	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		msdc_autok_adjust_param(host, i, &value, AUTOK_READ);
		AUTOK_RAWPRINT("[AUTOK]reg field %s:%d\r\n", autok_param_info[i].param_name, value);
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
 * Function: msdc_autok_adjust_paddly                  *
 * Param : value - delay cnt from 0 to 63              *
 *         pad_sel - 0 for cmd pad and 1 for data pad  *
 *******************************************************/
#define CMD_PAD_RDLY 0
#define DAT_PAD_RDLY 1
#define DS_PAD_RDLY 2
static void msdc_autok_adjust_paddly(struct msdc_host *host, unsigned int *value,
				     unsigned int pad_sel)
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
		msdc_autok_adjust_param(host, CMD_RD_D_DLY1, &uCfgL, AUTOK_WRITE);
		msdc_autok_adjust_param(host, CMD_RD_D_DLY2, &uCfgH, AUTOK_WRITE);

		msdc_autok_adjust_param(host, CMD_RD_D_DLY1_SEL, &uCfgLSel, AUTOK_WRITE);
		msdc_autok_adjust_param(host, CMD_RD_D_DLY2_SEL, &uCfgHSel, AUTOK_WRITE);
		break;
	case DAT_PAD_RDLY:
		msdc_autok_adjust_param(host, DAT_RD_D_DLY1, &uCfgL, AUTOK_WRITE);
		msdc_autok_adjust_param(host, DAT_RD_D_DLY2, &uCfgH, AUTOK_WRITE);

		msdc_autok_adjust_param(host, DAT_RD_D_DLY1_SEL, &uCfgLSel, AUTOK_WRITE);
		msdc_autok_adjust_param(host, DAT_RD_D_DLY2_SEL, &uCfgHSel, AUTOK_WRITE);
		break;
	case DS_PAD_RDLY:
		msdc_autok_adjust_param(host, EMMC50_DS_Z_DLY1, &uCfgL, AUTOK_WRITE);
		msdc_autok_adjust_param(host, EMMC50_DS_Z_DLY2, &uCfgH, AUTOK_WRITE);

		msdc_autok_adjust_param(host, EMMC50_DS_Z_DLY1_SEL, &uCfgLSel, AUTOK_WRITE);
		msdc_autok_adjust_param(host, EMMC50_DS_Z_DLY2_SEL, &uCfgHSel, AUTOK_WRITE);
		break;
	}
}

static void autok_paddly_update(unsigned int pad_sel, unsigned int dly_cnt, u8 *autok_tune_res)
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
		autok_param_update(CMD_RD_D_DLY1, uCfgL, autok_tune_res);
		autok_param_update(CMD_RD_D_DLY2, uCfgH, autok_tune_res);

		autok_param_update(CMD_RD_D_DLY1_SEL, uCfgLSel, autok_tune_res);
		autok_param_update(CMD_RD_D_DLY2_SEL, uCfgHSel, autok_tune_res);
		break;
	case DAT_PAD_RDLY:
		autok_param_update(DAT_RD_D_DLY1, uCfgL, autok_tune_res);
		autok_param_update(DAT_RD_D_DLY2, uCfgH, autok_tune_res);

		autok_param_update(DAT_RD_D_DLY1_SEL, uCfgLSel, autok_tune_res);
		autok_param_update(DAT_RD_D_DLY2_SEL, uCfgHSel, autok_tune_res);
		break;
	case DS_PAD_RDLY:
		autok_param_update(EMMC50_DS_Z_DLY1, uCfgL, autok_tune_res);
		autok_param_update(EMMC50_DS_Z_DLY2, uCfgH, autok_tune_res);

		autok_param_update(EMMC50_DS_Z_DLY1_SEL, uCfgLSel, autok_tune_res);
		autok_param_update(EMMC50_DS_Z_DLY2_SEL, uCfgHSel, autok_tune_res);
		break;
	}
}

/*******************************************************
 * Exectue tuning IF Implenment                        *
 *******************************************************/
static int autok_write_param(struct msdc_host *host, enum AUTOK_PARAM param, u32 value)
{
	msdc_autok_adjust_param(host, param, &value, AUTOK_WRITE);

	return 0;
}

int autok_path_sel(struct msdc_host *host)
{
	void __iomem *base = host->base;

	autok_write_param(host, READ_DATA_SMPL_SEL, 0);
	autok_write_param(host, WRITE_DATA_SMPL_SEL, 0);

	/* clK tune all data Line share dly */
	autok_write_param(host, DATA_DLYLINE_SEL, 0);

	/* data tune mode select */
#if defined(CHIP_DENALI_3_DAT_TUNE)
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
	autok_write_param(host, EMMC50_WDATA_EDGE, 0);

	/* Common Setting Config */
	autok_write_param(host, CKGEN_MSDC_DLY_SEL, AUTOK_CKGEN_VALUE);
	autok_write_param(host, CMD_RSP_TA_CNTR, AUTOK_CMD_TA_VALUE);
	autok_write_param(host, WRDAT_CRCS_TA_CNTR, AUTOK_CRC_TA_VALUE);

	MSDC_SET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_GET_BUSY_MA, AUTOK_BUSY_MA_VALUE);
	MSDC_SET_FIELD(MSDC_PATCH_BIT1, MSDC_PB1_GET_CRC_MA, AUTOK_CRC_MA_VALUE);

	return 0;
}
EXPORT_SYMBOL(autok_path_sel);

int autok_init_sdr104(struct msdc_host *host)
{
	void __iomem *base = host->base;

	/* driver may miss data tune path setting in the interim */
	autok_path_sel(host);

	/* if any specific config need modify add here */
	/* LATCH_TA_EN Config for WCRC Path non_HS400 */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, AUTOK_CRC_LATCH_EN_NON_HS400_VALUE);
	/* LATCH_TA_EN Config for CMD Path non_HS400 */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL, AUTOK_CMD_LATCH_EN_NON_HS400_VALUE);

	/* enable dvfs feature */
	if (host->hw->host_function == MSDC_SDIO)
		MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_EN, 1);

	return 0;
}
EXPORT_SYMBOL(autok_init_sdr104);

int autok_init_hs200(struct msdc_host *host)
{
	void __iomem *base = host->base;

	/* driver may miss data tune path setting in the interim */
	autok_path_sel(host);

	/* if any specific config need modify add here */
	/* LATCH_TA_EN Config for WCRC Path non_HS400 */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, AUTOK_CRC_LATCH_EN_NON_HS400_VALUE);
	/* LATCH_TA_EN Config for CMD Path non_HS400 */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL, AUTOK_CMD_LATCH_EN_NON_HS400_VALUE);

	return 0;
}
EXPORT_SYMBOL(autok_init_hs200);

int autok_init_hs400(struct msdc_host *host)
{
	void __iomem *base = host->base;
	/* driver may miss data tune path setting in the interim */
	autok_path_sel(host);

	/* if any specific config need modify add here */
	/* LATCH_TA_EN Config for WCRC Path HS400 */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL, AUTOK_CMD_LATCH_EN_HS400_VALUE);
	/* LATCH_TA_EN Config for CMD Path HS400 */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, AUTOK_CRC_LATCH_EN_HS400_VALUE);
	/* write path switch to emmc50 */
	autok_write_param(host, EMMC50_WDATA_MUX_EN, 1);
	/* Specifical for HS400 Path Sel */
	autok_write_param(host, MSDC_WCRC_ASYNC_FIFO_SEL, 0);

	return 0;
}
EXPORT_SYMBOL(autok_init_hs400);

int execute_online_tuning_hs400(struct msdc_host *host, u8 *res)
{
	unsigned int ret = 0;
	unsigned int uCmdEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k, cycle_value;
	struct AUTOK_REF_INFO uCmdDatInfo;
	struct AUTOK_SCAN_RES *pBdInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[TUNING_PARAM_COUNT];
	unsigned int opcode = MMC_SEND_STATUS;
#if HS400_DSCLK_NEED_TUNING
	u32 RawData = 0;
#endif
	unsigned int uDatDly = 0;

	autok_init_hs400(host);
	memset((void *)p_autok_tune_res, 0, sizeof(p_autok_tune_res) / sizeof(u8));

	/* Step1 : Tuning Cmd Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uCmdEdge = 0;
	do {
		pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[uCmdEdge]);
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_CMD);
				if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
					RawData64 |= (u64)(1LL << j);
					break;
				}
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD %d \t %d \t %s\r\n", uCmdEdge, score,
			       tune_result_str64);
		if (autok_check_scan_res64(RawData64, pBdInfo) != 0)
			return -1;

		#if 0
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK]Edge:%d \t BoundaryCnt:%d \t FullBoundaryCnt:%d \t\r\n",
			       uCmdEdge, pBdInfo->bd_cnt, pBdInfo->fbd_cnt);

		for (i = 0; i < BD_MAX_CNT; i++) {
			AUTOK_DBGPRINT(AUTOK_DBG_RES,
				       "[AUTOK]BoundInf[%d]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n", i,
				       pBdInfo->bd_info[i].Bound_Start, pBdInfo->bd_info[i].Bound_End,
				       pBdInfo->bd_info[i].Bound_width, pBdInfo->bd_info[i].is_fullbound);
		}
		#endif

		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	if (autok_pad_dly_sel(&uCmdDatInfo) == 0) {
		autok_param_update(CMD_EDGE, uCmdDatInfo.opt_edge_sel, p_autok_tune_res);
		autok_paddly_update(CMD_PAD_RDLY, uCmdDatInfo.opt_dly_cnt, p_autok_tune_res);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}

	p_autok_tune_res[EMMC50_DS_ZDLY_DLY] = (uCmdDatInfo.cycle_cnt / 2) > 31 ? 31 : (uCmdDatInfo.cycle_cnt / 2);
	cycle_value = uCmdDatInfo.cycle_cnt;
	/* Step2 : Tuning DS Clk Path-ZCLK only tune DLY1 */
#ifdef CMDQ
	opcode = MMC_SEND_EXT_CSD;
#else
	opcode = MMC_READ_SINGLE_BLOCK;
#endif
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));
	pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[0]);
	RawData64 = 0LL;
	for (j = 0; j < 32; j++) {
		msdc_autok_adjust_paddly(host, &j, DS_PAD_RDLY);
		for (k = 0; k < AUTOK_CMD_TIMES / 4; k++) {
			ret = autok_send_tune_cmd(host, opcode, TUNE_DATA);
			if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
				AUTOK_RAWPRINT
				    ("[AUTOK]Error Autok CMD Failed while tune DS Delay\r\n");
				return -1;
			} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
				RawData64 |= (u64) (1LL << j);
				break;
			}
		}
	}
	RawData64 |= 0xffffffff00000000;
	score = autok_simple_score64(tune_result_str64, RawData64);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DLY1/2 %d \t %d \t %s\r\n", uCmdEdge, score,
		       tune_result_str64);
	if (autok_check_scan_res64(RawData64, pBdInfo) != 0)
		return -1;

	#if 0
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
		       "[AUTOK]Edge:%d \t BoundaryCnt:%d \t FullBoundaryCnt:%d \t\r\n", uCmdEdge,
		       pBdInfo->bd_cnt, pBdInfo->fbd_cnt);

	for (i = 0; i < BD_MAX_CNT; i++) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK]BoundInf[%d]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n", i,
			       pBdInfo->bd_info[i].Bound_Start, pBdInfo->bd_info[i].Bound_End,
			       pBdInfo->bd_info[i].Bound_width, pBdInfo->bd_info[i].is_fullbound);
	}
	#endif

	if (autok_ds_dly_sel(pBdInfo, cycle_value, &uDatDly) == 0) {
		autok_paddly_update(DS_PAD_RDLY, uDatDly, p_autok_tune_res);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}

#if HS400_DSCLK_NEED_TUNING
	/* Step3 : Tuning DS Clk Path-ZDLY */
	p_autok_tune_res[EMMC50_DS_Z_DLY1] = 8;
	p_autok_tune_res[EMMC50_DS_Z_DLY1_SEL] = 1;
	autok_tuning_parameter_init(host, p_autok_tune_res);
	AUTOK_RAWPRINT("[AUTOK]Step3.Scan DS(ZDLY) Clk Pad Delay\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DS_ZDLY_DLY DS_ZCLK_DLY1~2 \r\n");
	pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[1]);
	RawData = 0;
	for (j = 0; j < 32; j++) {
		msdc_autok_adjust_param(host, EMMC50_DS_ZDLY_DLY, &j, AUTOK_WRITE);
		for (k = 0; k < AUTOK_CMD_TIMES / 4; k++) {
			ret = autok_send_tune_cmd(host, opcode, TUNE_DATA);
			if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
				AUTOK_RAWPRINT
				    ("[AUTOK]Error Autok CMD Failed while tune DS(ZDLY) Delay\r\n");
				return -1;
			} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
				RawData |= (u64) (1 << j);
				break;
			}
		}
	}
	score = autok_simple_score(tune_result_str64, RawData);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] %d \t %d \t %s\r\n", uCmdEdge, score,
		       tune_result_str64);
	if (autok_check_scan_res64(RawData, pBdInfo) != 0)
		return -1;

	AUTOK_DBGPRINT(AUTOK_DBG_RES,
		       "[AUTOK]Edge:%d \t BoundaryCnt:%d \t FullBoundaryCnt:%d \t\r\n", uCmdEdge,
		       pBdInfo->bd_cnt, pBdInfo->fbd_cnt);

	for (i = 0; i < BD_MAX_CNT; i++) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK]BoundInf[%d]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n", i,
			       pBdInfo->bd_info[i].Bound_Start, pBdInfo->bd_info[i].Bound_End,
			       pBdInfo->bd_info[i].Bound_width, pBdInfo->bd_info[i].is_fullbound);
	}

	if (autok_ds_dly_sel(pBdInfo, cycle_value, &uDatDly) == 0) {
		autok_param_update(EMMC50_DS_ZDLY_DLY, uDatDly, p_autok_tune_res);
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK]================Analysis Result[HS400]================\r\n");
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]Sample ZDelay Cnt Sel:%d\r\n", uDatDly);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}
#endif

	autok_tuning_parameter_init(host, p_autok_tune_res);
	autok_result_dump(host, p_autok_tune_res);
#if AUTOK_PARAM_DUMP_ENABLE
	autok_register_dump(host);
#endif
	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
		       sizeof(p_autok_tune_res) / sizeof(u8));
	}

	return 0;
}

int execute_cmd_online_tuning(struct msdc_host *host, u8 *res)
{
	void __iomem *base = host->base;
	unsigned int ret = 0;
	unsigned int uCmdEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int i, j, k;
	struct AUTOK_REF_INFO uCmdDatInfo;
	struct AUTOK_SCAN_RES *pBdInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[5];
	unsigned int opcode = MMC_SEND_STATUS;

	memset((void *)p_autok_tune_res, 0, sizeof(p_autok_tune_res) / sizeof(u8));

	/* Tuning Cmd Path */
	AUTOK_RAWPRINT("[AUTOK]Optimised CMD Pad Data Delay Sel\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD_EDGE \t CMD_RD_R_DLY1~2 \r\n");
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uCmdEdge = 0;
	do {
		pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[uCmdEdge]);
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_CMD);
				if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
					RawData64 |= (u64)(1LL << j);
					break;
				}
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK] %d \t %d \t %s\r\n", uCmdEdge, score,
				tune_result_str64);
		if (autok_check_scan_res64(RawData64, pBdInfo) != 0)
			return -1;

		AUTOK_DBGPRINT(AUTOK_DBG_RES,
				"[AUTOK]Edge:%d \t BoundaryCnt:%d \t FullBoundaryCnt:%d \t\r\n",
				uCmdEdge, pBdInfo->bd_cnt, pBdInfo->fbd_cnt);

		for (i = 0; i < BD_MAX_CNT; i++) {
			AUTOK_DBGPRINT(AUTOK_DBG_RES,
					"[AUTOK]BoundInf[%d]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n", i,
					pBdInfo->bd_info[i].Bound_Start, pBdInfo->bd_info[i].Bound_End,
					pBdInfo->bd_info[i].Bound_width, pBdInfo->bd_info[i].is_fullbound);
		}

		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	if (autok_pad_dly_sel(&uCmdDatInfo) == 0) {
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdDatInfo.opt_edge_sel, AUTOK_WRITE);
		msdc_autok_adjust_paddly(host, &uCmdDatInfo.opt_dly_cnt, CMD_PAD_RDLY);
		MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_RSPL, p_autok_tune_res[0]);
		MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, p_autok_tune_res[1]);
		MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRRDLYSEL, p_autok_tune_res[2]);
		MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRDLY2, p_autok_tune_res[3]);
		MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRRDLY2SEL, p_autok_tune_res[4]);
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
				"[AUTOK]================Analysis Result================\r\n");
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]Sample Edge Sel: %d, Delay Cnt Sel:%d\r\n",
				uCmdDatInfo.opt_edge_sel, uCmdDatInfo.opt_dly_cnt);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
				"[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}

	AUTOK_RAWPRINT("[AUTOK]param CMD_EDGE:%d\r\n", p_autok_tune_res[0]);
	AUTOK_RAWPRINT("[AUTOK]param CMD_RD_D_DLY1:%d\r\n", p_autok_tune_res[1]);
	AUTOK_RAWPRINT("[AUTOK]param CMD_RD_D_DLY1_SEL:%d\r\n", p_autok_tune_res[2]);
	AUTOK_RAWPRINT("[AUTOK]param CMD_RD_D_DLY2:%d\r\n", p_autok_tune_res[3]);
	AUTOK_RAWPRINT("[AUTOK]param CMD_RD_D_DLY2_SEL:%d\r\n", p_autok_tune_res[4]);

	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
			sizeof(p_autok_tune_res) / sizeof(u8));
	}
	AUTOK_RAWPRINT("[AUTOK]CMD Online Tune Alg Complete\r\n");

	return 0;
}

/* online tuning for latch ck */
int autok_execute_tuning_latch_ck(struct msdc_host *host, unsigned int opcode,
	unsigned int latch_ck_initail_value)
{
	unsigned int ret = 0;
	unsigned int j, k;
	void __iomem *base = host->base;
	unsigned int tune_time;

	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	switch (host->hw->host_function) {
	case MSDC_EMMC:
		tune_time = AUTOK_LATCH_CK_EMMC_TUNE_TIMES;
		break;
	case MSDC_SD:
		tune_time = AUTOK_LATCH_CK_SD_TUNE_TIMES;
		break;
	case MSDC_SDIO:
		tune_time = AUTOK_LATCH_CK_SDIO_TUNE_TIMES;
		break;
	default:
		tune_time = AUTOK_LATCH_CK_SDIO_TUNE_TIMES;
		break;
	}
	for (j = latch_ck_initail_value; j < 8; j += (host->hclk / host->sclk)) {
		host->tune_latch_ck_cnt = 0;
		msdc_clear_fifo();
		MSDC_SET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL, j);
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
			ret = autok_send_tune_cmd(host, opcode, TUNE_LATCH_CK);
			if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
				AUTOK_RAWPRINT("[AUTOK]Error Autok CMD Failed while tune LATCH CK\r\n");
				break;
			} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
				AUTOK_RAWPRINT("[AUTOK]Error Autok  tune LATCH_CK error %d\r\n", j);
				break;
			}
		}
		if (ret == 0) {
			MSDC_SET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL, j);
			break;
		}
	}
	host->tune_latch_ck_cnt = 0;
	return j;
}

/* online tuning for eMMC4.5(hs200) */
int execute_online_tuning_hs200(struct msdc_host *host, u8 *res)
{
	unsigned int ret = 0;
	unsigned int uCmdEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k, cycle_value;
	struct AUTOK_REF_INFO uCmdDatInfo;
	struct AUTOK_SCAN_RES *pBdInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[TUNING_PARAM_COUNT];
	unsigned int opcode = MMC_SEND_STATUS;
	unsigned int uDatDly = 0;

	autok_init_hs200(host);
	memset((void *)p_autok_tune_res, 0, sizeof(p_autok_tune_res) / sizeof(u8));
	p_autok_tune_res[INT_DAT_LATCH_CK] = 1;

	/* Step1 : Tuning Cmd Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uCmdEdge = 0;
	do {
		pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[uCmdEdge]);
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_CMD);
				if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				}
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD %d \t %d \t %s\r\n", uCmdEdge, score,
			       tune_result_str64);
		if (autok_check_scan_res64(RawData64, pBdInfo) != 0) {
			host->autok_error = -1;
			return -1;
		}

		#if 0
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK]Edge:%d \t BoundaryCnt:%d \t FullBoundaryCnt:%d \t\r\n",
			       uCmdEdge, pBdInfo->bd_cnt, pBdInfo->fbd_cnt);

		for (i = 0; i < BD_MAX_CNT; i++) {
			AUTOK_DBGPRINT(AUTOK_DBG_RES,
				       "[AUTOK]BoundInf[%d]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n", i,
				       pBdInfo->bd_info[i].Bound_Start, pBdInfo->bd_info[i].Bound_End,
				       pBdInfo->bd_info[i].Bound_width, pBdInfo->bd_info[i].is_fullbound);
		}
		#endif

		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	if (autok_pad_dly_sel(&uCmdDatInfo) == 0) {
		autok_param_update(CMD_EDGE, uCmdDatInfo.opt_edge_sel, p_autok_tune_res);
		autok_paddly_update(CMD_PAD_RDLY, uCmdDatInfo.opt_dly_cnt, p_autok_tune_res);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}
	cycle_value = uCmdDatInfo.cycle_cnt;

	/* Step2 Tuning Data Path (Only Rising Edge Used) */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	opcode = MMC_SEND_TUNING_BLOCK_HS200;
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));
	pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[0]);
	RawData64 = 0LL;
	for (j = 0; j < 64; j++) {
		msdc_autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
		for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
			ret = autok_send_tune_cmd(host, opcode, TUNE_DATA);
			if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
				AUTOK_RAWPRINT("[AUTOK]Error Autok CMD Failed while tune Read\r\n");
				return -1;
			} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
				RawData64 |= (u64) (1LL << j);
				break;
			}
		}
	}
	score = autok_simple_score64(tune_result_str64, RawData64);
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DAT %d \t %d \t %s\r\n", 0, score,
		       tune_result_str64);
	if (autok_check_scan_res64(RawData64, pBdInfo) != 0) {
		return -1;
	};
	#if 0
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
		       "[AUTOK]Edge:%d \t BoundaryCnt:%d \t FullBoundaryCnt:%d \t\r\n", 0,
		       pBdInfo->bd_cnt, pBdInfo->fbd_cnt);

	for (i = 0; i < BD_MAX_CNT; i++) {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK]BoundInf[%d]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n", i,
			       pBdInfo->bd_info[i].Bound_Start, pBdInfo->bd_info[i].Bound_End,
			       pBdInfo->bd_info[i].Bound_width, pBdInfo->bd_info[i].is_fullbound);
	}
	#endif

	if (autok_pad_dly_sel_single_edge(pBdInfo, cycle_value, &uDatDly) == 0)
		autok_paddly_update(DAT_PAD_RDLY, uDatDly, p_autok_tune_res);

	autok_tuning_parameter_init(host, p_autok_tune_res);

	/* Step3 : Tuning LATCH CK  */
	opcode = MMC_SEND_TUNING_BLOCK_HS200;
	p_autok_tune_res[INT_DAT_LATCH_CK] = autok_execute_tuning_latch_ck(host, opcode,
		p_autok_tune_res[INT_DAT_LATCH_CK]);

	autok_result_dump(host, p_autok_tune_res);

#if AUTOK_PARAM_DUMP_ENABLE
	autok_register_dump(host);
#endif
	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
		       sizeof(p_autok_tune_res) / sizeof(u8));
	}

	return 0;
}

/* online tuning for SDIO/SD */
int execute_online_tuning(struct msdc_host *host, u8 *res)
{
	unsigned int ret = 0;
	unsigned int uCmdEdge = 0;
	unsigned int uDatEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int j, k;
	unsigned int opcode = MMC_SEND_TUNING_BLOCK;
	struct AUTOK_REF_INFO uCmdDatInfo;
	struct AUTOK_SCAN_RES *pBdInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[TUNING_PARAM_COUNT];

	autok_init_sdr104(host);
	memset((void *)p_autok_tune_res, 0, sizeof(p_autok_tune_res) / sizeof(u8));

	/* Step1 : Tuning Cmd Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uCmdEdge = 0;
	do {
		pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[uCmdEdge]);
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_CMD);
				if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				}
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD %d \t %d \t %s\r\n", uCmdEdge, score,
			       tune_result_str64);
		if (autok_check_scan_res64(RawData64, pBdInfo) != 0) {
			host->autok_error = -1;
			return -1;
		}
		#if 0
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK]Edge:%d \t BoundaryCnt:%d \t FullBoundaryCnt:%d \t\r\n",
			       uCmdEdge, pBdInfo->bd_cnt, pBdInfo->fbd_cnt);

		for (i = 0; i < BD_MAX_CNT; i++) {
			AUTOK_DBGPRINT(AUTOK_DBG_RES,
				       "[AUTOK]BoundInf[%d]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n", i,
				       pBdInfo->bd_info[i].Bound_Start, pBdInfo->bd_info[i].Bound_End,
				       pBdInfo->bd_info[i].Bound_width, pBdInfo->bd_info[i].is_fullbound);
		}
		#endif

		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	if (autok_pad_dly_sel(&uCmdDatInfo) == 0) {
		autok_param_update(CMD_EDGE, uCmdDatInfo.opt_edge_sel, p_autok_tune_res);
		autok_paddly_update(CMD_PAD_RDLY, uCmdDatInfo.opt_dly_cnt, p_autok_tune_res);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}

	/* Step2 : Tuning Data Path */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));

	uDatEdge = 0;
	do {
		pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[uDatEdge]);
		msdc_autok_adjust_param(host, RD_FIFO_EDGE, &uDatEdge, AUTOK_WRITE);
		RawData64 = 0LL;
		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
			for (k = 0; k < AUTOK_CMD_TIMES / 2; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_DATA);
				if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
					AUTOK_RAWPRINT
					    ("[AUTOK]Error Autok CMD Failed while tune Read\r\n");
					host->autok_error = -1;
					return -1;
				} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				}
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DAT %d \t %d \t %s\r\n", uDatEdge, score,
			       tune_result_str64);
		if (autok_check_scan_res64(RawData64, pBdInfo) != 0) {
			host->autok_error = -1;
			return -1;
		}
		#if 0
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK]Edge:%d \t BoundaryCnt:%d \t FullBoundaryCnt:%d \t\r\n",
			       uDatEdge, pBdInfo->bd_cnt, pBdInfo->fbd_cnt);

		for (i = 0; i < BD_MAX_CNT; i++) {
			AUTOK_DBGPRINT(AUTOK_DBG_RES,
				       "[AUTOK]BoundInf[%d]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n", i,
				       pBdInfo->bd_info[i].Bound_Start, pBdInfo->bd_info[i].Bound_End,
				       pBdInfo->bd_info[i].Bound_width, pBdInfo->bd_info[i].is_fullbound);
		}
		#endif

		uDatEdge ^= 0x1;
	} while (uDatEdge);

	if (autok_pad_dly_sel(&uCmdDatInfo) == 0) {
		autok_param_update(RD_FIFO_EDGE, uCmdDatInfo.opt_edge_sel, p_autok_tune_res);
		autok_paddly_update(DAT_PAD_RDLY, uCmdDatInfo.opt_dly_cnt, p_autok_tune_res);
		autok_param_update(WD_FIFO_EDGE, uCmdDatInfo.opt_edge_sel, p_autok_tune_res);
	} else {
		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK][Error]=============Analysis Failed!!=======================\r\n");
	}

	autok_tuning_parameter_init(host, p_autok_tune_res);

	/* Step3 : Tuning LATCH CK */
	p_autok_tune_res[INT_DAT_LATCH_CK] = autok_execute_tuning_latch_ck(host, opcode,
		p_autok_tune_res[INT_DAT_LATCH_CK]);

	autok_result_dump(host, p_autok_tune_res);
#if AUTOK_PARAM_DUMP_ENABLE
	autok_register_dump(host);
#endif
	if (res != NULL) {
		memcpy((void *)res, (void *)p_autok_tune_res,
		       sizeof(p_autok_tune_res) / sizeof(u8));
	}
	host->autok_error = 0;

	return 0;
}

int execute_online_tuning_stress(struct msdc_host *host)
{
	unsigned int ret = 0;
	unsigned int uCmdEdge = 0;
	unsigned int uDatEdge = 0;
	u64 RawData64 = 0LL;
	unsigned int score = 0;
	unsigned int i, j, k;
	struct AUTOK_REF_INFO uCmdDatInfo;
	struct AUTOK_SCAN_RES *pBdInfo;
	char tune_result_str64[65];
	u8 p_autok_tune_res[TUNING_PARAM_COUNT];

	autok_init_sdr104(host);
	memset((void *)p_autok_tune_res, 0, sizeof(p_autok_tune_res) / sizeof(u8));

	/* Step1. CMD Path Optimised Delay Count */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	AUTOK_RAWPRINT("[AUTOK]Step1.Optimised CMD Pad Data Delay Sel\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD_EDGE \t CMD_RD_R_DLY1~2 \r\n");
	for (i = 0; i < 32; i++) {
		msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &i, AUTOK_WRITE);
		memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));
		uCmdEdge = 0;
		do {
			pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[uCmdEdge]);
			msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
			RawData64 = 0LL;
			for (j = 0; j < 64; j++) {
				msdc_autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
				for (k = 0; k < AUTOK_CMD_TIMES; k++) {
					ret = autok_send_tune_cmd(host, MMC_SEND_TUNING_BLOCK, TUNE_CMD);
					if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
						RawData64 |= (u64) (1LL << j);
						break;
					}
				}
			}
			score = autok_simple_score64(tune_result_str64, RawData64);
			AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK][%d] %d \t %d \t %s\r\n", i, uCmdEdge,
				       score, tune_result_str64);
			/*get Boundary info */
			if (autok_check_scan_res64(RawData64, pBdInfo) != 0) {
				return -1;
			};

			AUTOK_DBGPRINT(AUTOK_DBG_RES,
				       "[AUTOK]Edge:%d \t BoundaryCnt:%d \t FullBoundaryCnt:%d \t\r\n",
				       uCmdEdge, pBdInfo->bd_cnt, pBdInfo->fbd_cnt);
			AUTOK_DBGPRINT(AUTOK_DBG_RES,
				       "[AUTOK]BoundInf[0]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n",
				       pBdInfo->bd_info[0].Bound_Start,
				       pBdInfo->bd_info[0].Bound_End,
				       pBdInfo->bd_info[0].Bound_width,
				       pBdInfo->bd_info[0].is_fullbound);
			AUTOK_DBGPRINT(AUTOK_DBG_RES,
				       "[AUTOK]BoundInf[1]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n",
				       pBdInfo->bd_info[1].Bound_Start,
				       pBdInfo->bd_info[1].Bound_End,
				       pBdInfo->bd_info[1].Bound_width,
				       pBdInfo->bd_info[1].is_fullbound);
			AUTOK_DBGPRINT(AUTOK_DBG_RES,
				       "[AUTOK]BoundInf[2]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n",
				       pBdInfo->bd_info[2].Bound_Start,
				       pBdInfo->bd_info[2].Bound_End,
				       pBdInfo->bd_info[2].Bound_width,
				       pBdInfo->bd_info[2].is_fullbound);

			uCmdEdge ^= 0x1;
		} while (uCmdEdge);

		autok_pad_dly_sel(&uCmdDatInfo);

		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK]================Analysis Result[SD(IO)]===============\r\n");
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]Sample Edge Sel: %d, Delay Cnt Sel:%d\r\n",
			       uCmdDatInfo.opt_edge_sel, uCmdDatInfo.opt_dly_cnt);
	}

	/* Step2. DATA Path Optimised Delay Count */
	autok_tuning_parameter_init(host, p_autok_tune_res);
	AUTOK_RAWPRINT("[AUTOK]Step2.Optimised CMD Pad Data Delay Sel\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD_EDGE \t CMD_RD_R_DLY1~2 \r\n");
	uCmdEdge = 0;
	for (i = 0; i < 32; i++) {
		msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &i, AUTOK_WRITE);
		memset(&uCmdDatInfo, 0, sizeof(struct AUTOK_REF_INFO));
		uDatEdge = 0;
		do {
			pBdInfo = (struct AUTOK_SCAN_RES *)&(uCmdDatInfo.scan_info[uDatEdge]);
			msdc_autok_adjust_param(host, RD_FIFO_EDGE, &uDatEdge, AUTOK_WRITE);
			RawData64 = 0LL;
			for (j = 0; j < 64; j++) {
				msdc_autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
rd_retry:
				for (k = 0; k < AUTOK_CMD_TIMES; k++) {
					ret = autok_send_tune_cmd(host, MMC_SEND_TUNING_BLOCK, TUNE_DATA);
					if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
						uCmdEdge ^= 0x1;
						msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge,
									AUTOK_WRITE);
						goto rd_retry;
					} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
						RawData64 |= (u64) (1LL << j);
						break;
					}
				}
			}
			score = autok_simple_score64(tune_result_str64, RawData64);
			AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK][%d] %d \t %d \t %s\r\n", i, uDatEdge,
				       score, tune_result_str64);
			/*get Boundary info */
			if (autok_check_scan_res64(RawData64, pBdInfo) != 0) {
				return -1;
			};

			AUTOK_DBGPRINT(AUTOK_DBG_RES,
				       "[AUTOK]Edge:%d \t BoundaryCnt:%d \t FullBoundaryCnt:%d \t\r\n",
				       uDatEdge, pBdInfo->bd_cnt, pBdInfo->fbd_cnt);
			AUTOK_DBGPRINT(AUTOK_DBG_RES,
				       "[AUTOK]BoundInf[0]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n",
				       pBdInfo->bd_info[0].Bound_Start,
				       pBdInfo->bd_info[0].Bound_End,
				       pBdInfo->bd_info[0].Bound_width,
				       pBdInfo->bd_info[0].is_fullbound);
			AUTOK_DBGPRINT(AUTOK_DBG_RES,
				       "[AUTOK]BoundInf[1]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n",
				       pBdInfo->bd_info[1].Bound_Start,
				       pBdInfo->bd_info[1].Bound_End,
				       pBdInfo->bd_info[1].Bound_width,
				       pBdInfo->bd_info[1].is_fullbound);
			AUTOK_DBGPRINT(AUTOK_DBG_RES,
				       "[AUTOK]BoundInf[2]: S:%d \t E:%d \t W:%d \t FullBound:%d\r\n",
				       pBdInfo->bd_info[2].Bound_Start,
				       pBdInfo->bd_info[2].Bound_End,
				       pBdInfo->bd_info[2].Bound_width,
				       pBdInfo->bd_info[2].is_fullbound);

			uDatEdge ^= 0x1;
		} while (uDatEdge);

		autok_pad_dly_sel(&uCmdDatInfo);

		AUTOK_DBGPRINT(AUTOK_DBG_RES,
			       "[AUTOK]================Analysis Result[SD(IO)]===============\r\n");
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]Sample Edge Sel: %d, Delay Cnt Sel:%d\r\n",
			       uCmdDatInfo.opt_edge_sel, uCmdDatInfo.opt_dly_cnt);
	}

	i = 0;
	msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &i, AUTOK_WRITE);

	AUTOK_RAWPRINT("[AUTOK]Online Tune Alg Complete\r\n");

	return 0;
}

/********************************************************************
 * Offline tune for SDIO/eMMC/SD                                    *
 * Common Interface                                                 *
 ********************************************************************/
int execute_offline_tuning_hs400(struct msdc_host *host)
{
	unsigned int ret = 0;

	unsigned int i, j, k;
	unsigned int uCkgenSel = 0;
	unsigned int uCmdEdge = 0;
	unsigned int res_cmd_ta = 1;

	u64 RawData64 = 0LL;
	unsigned int score = 0;
	char tune_result_str32[33];
	char tune_result_str64[65];
	u32 g_ta_raw[SCALE_TA_CNTR];
	u32 g_ta_score[SCALE_TA_CNTR];
	u8 p_autok_tune_res[TUNING_PARAM_COUNT];

	unsigned int opcode = MMC_READ_SINGLE_BLOCK;

	autok_init_hs400(host);
	memset((void *)p_autok_tune_res, 0, sizeof(p_autok_tune_res) / sizeof(u8));
	p_autok_tune_res[EMMC50_DS_Z_DLY1] = 7;
	p_autok_tune_res[EMMC50_DS_Z_DLY1_SEL] = 1;
	p_autok_tune_res[EMMC50_DS_ZDLY_DLY] = 24;


	/************************************************************
	 *                                                          *
	 *  Step 1 : Caculate CMD_RSP_TA                            *
	 *                                                          *
	 ************************************************************/
	autok_tuning_parameter_init(host, p_autok_tune_res);
	AUTOK_RAWPRINT("[AUTOK]Step1.Scan CMD_RSP_TA Base on Cmd Pad Delay\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD_EDGE \t CMD_RSP_TA_CNTR \t PAD_CMD_D_DLY \r\n");

	uCmdEdge = 0;
	do {
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		for (i = 0; i < 8; i++) {
			g_ta_raw[i] = 0;
			msdc_autok_adjust_param(host, CMD_RSP_TA_CNTR, &i, AUTOK_WRITE);
			for (j = 0; j < 32; j++) {
				msdc_autok_adjust_param(host, CMD_RD_D_DLY1, &j, AUTOK_WRITE);

				for (k = 0; k < AUTOK_CMD_TIMES; k++) {
					ret = autok_send_tune_cmd(host, opcode, TUNE_CMD);

					if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
						g_ta_raw[i] |= (1 << j);
						break;
					}
				}
			}
			g_ta_score[i] = autok_simple_score(tune_result_str32, g_ta_raw[i]);
			AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]%d \t %d\t %d \t %s\r\n", uCmdEdge, i,
				       g_ta_score[i], tune_result_str32);
		}

		if (autok_ta_sel(g_ta_raw, &res_cmd_ta) == 0) {
			/* autok_param_update(CMD_RSP_TA_CNTR, res_cmd_ta, p_autok_tune_res); */
			msdc_autok_adjust_param(host, CMD_RSP_TA_CNTR, &res_cmd_ta, AUTOK_WRITE);
			AUTOK_RAWPRINT("[AUTOK]CMD_TA Sel:%d\r\n", res_cmd_ta);
			break;
		}
		AUTOK_RAWPRINT
		    ("[AUTOK]Internal Boundary Occours,need switch cmd edge for rescan!\r\n");
			uCmdEdge ^= 1;	/* TA select Fail need switch cmd edge for rescan */
	} while (uCmdEdge);

	/************************************************************
	 *                                                          *
	 *  Step 2 : Scan CMD Pad Delay Base on CMD_EDGE & CKGEN    *
	 *                                                          *
	 ************************************************************/
	autok_tuning_parameter_init(host, p_autok_tune_res);
	AUTOK_RAWPRINT("[AUTOK]Step2.Scan CMD Pad Data Delay\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD_EDGE \t CKGEN \t CMD_RD_R_DLY1~2 \r\n");
	uCmdEdge = 0;

	do {
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);

		for (i = 0; i < 32; i++) {
			RawData64 = 0LL;
			msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &i, AUTOK_WRITE);

			for (j = 0; j < 64; j++) {
				msdc_autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
				for (k = 0; k < AUTOK_CMD_TIMES; k++) {
					ret = autok_send_tune_cmd(host, opcode, TUNE_CMD);
					if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
						RawData64 |= (u64) (1LL << j);
						break;
					}
				}
			}
			score = autok_simple_score64(tune_result_str64, RawData64);
			AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]%d \t %d \t %d \t %s\r\n",
				/*(u32)((RawData64>>32)&0xFFFFFFFF),(u32)(RawData64&0xFFFFFFFF), */
				uCmdEdge, i, score, tune_result_str64);
		}

		uCmdEdge ^= 0x1;
	} while (uCmdEdge);

	uCkgenSel = 0;
	msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &uCkgenSel, AUTOK_WRITE);
	/************************************************************
	 *                                                          *
	 *  Step 3 : Scan Dat Pad Delay Base on Data_EDGE & CKGEN   *
	 *                                                          *
	 ************************************************************/
	opcode = MMC_SEND_TUNING_BLOCK_HS200;
	autok_tuning_parameter_init(host, p_autok_tune_res);
	AUTOK_RAWPRINT("[AUTOK]Step3.Scan DS Clk Pad Delay\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DS_ZDLY_DLY DS_ZCLK_DLY1~2 \r\n");
	for (i = 0; i < 32; i++) {
		msdc_autok_adjust_param(host, EMMC50_DS_ZDLY_DLY, &i, AUTOK_WRITE);
		uCmdEdge = 0;
		RawData64 = 0LL;

		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, DS_PAD_RDLY);
rd_retry0:
			for (k = 0; k < AUTOK_CMD_TIMES; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_DATA);
				if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
					uCmdEdge ^= 0x1;
					msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge,
								AUTOK_WRITE);
					goto rd_retry0;
				} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				}
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK][%d] %d \t %d \t %s\r\n", i, uCmdEdge, score,
			       tune_result_str64);
	}

	/* CMD 17 Single block read */
	opcode = MMC_READ_SINGLE_BLOCK;
	autok_tuning_parameter_init(host, p_autok_tune_res);
	AUTOK_RAWPRINT("[AUTOK]Step3.Scan DS Clk Pad Delay\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]DS_ZDLY_DLY DS_ZCLK_DLY1~2 \r\n");

	for (i = 0; i < 32; i++) {
		msdc_autok_adjust_param(host, EMMC50_DS_ZDLY_DLY, &i, AUTOK_WRITE);
		uCmdEdge = 0;
		RawData64 = 0LL;

		for (j = 0; j < 64; j++) {
			msdc_autok_adjust_paddly(host, &j, DS_PAD_RDLY);
rd_retry1:
			for (k = 0; k < AUTOK_CMD_TIMES; k++) {
				ret = autok_send_tune_cmd(host, opcode, TUNE_DATA);
				if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
					uCmdEdge ^= 0x1;
					msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge,
								AUTOK_WRITE);
					goto rd_retry1;
				} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
					RawData64 |= (u64) (1LL << j);
					break;
				}
			}
		}
		score = autok_simple_score64(tune_result_str64, RawData64);
		AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK][%d] %d \t %d \t %s\r\n", i, uCmdEdge, score,
			       tune_result_str64);
	}

	autok_tuning_parameter_init(host, p_autok_tune_res);

	return ret;
}

int execute_offline_tuning(struct msdc_host *host)
{
	/* AUTOK_CODE_RELEASE */
	unsigned int ret = 0;
	unsigned int i, j, k;
	unsigned int uCkgenSel = 0;
	unsigned int uCmdEdge = 0;
	unsigned int uDatEdge = 0;
	unsigned int res_cmd_ta = 1;
#if defined(SDIO_TUNE_WRITE_PATH)
	unsigned int uWCrcEdge = 0;
	unsigned int res_wcrc_ta = 1;
#endif

	u64 RawData64 = 0LL;
	unsigned int score = 0;
	char tune_result_str32[33];
	char tune_result_str64[65];
	u32 g_ta_raw[SCALE_TA_CNTR];
	u32 g_ta_score[SCALE_TA_CNTR];
	u8 p_autok_tune_res[TUNING_PARAM_COUNT];
	unsigned int opcode = MMC_SEND_TUNING_BLOCK;

	memset((void *)p_autok_tune_res, 0, sizeof(p_autok_tune_res) / sizeof(u8));
	if (host->hw->host_function == MSDC_SDIO) {
		opcode = MMC_SEND_TUNING_BLOCK;
		autok_init_sdr104(host);
	} else if (host->hw->host_function == MSDC_EMMC) {
		opcode = MMC_SEND_TUNING_BLOCK_HS200;
		autok_init_hs200(host);
	}

	/************************************************************
	 *                                                          *
	 *  Step 1 : Caculate CMD_RSP_TA                            *
	 *                                                          *
	 ************************************************************/
	autok_tuning_parameter_init(host, p_autok_tune_res);
	AUTOK_RAWPRINT("[AUTOK]Step1.Scan CMD_RSP_TA Base on Cmd Pad Delay\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD_EDGE \t CMD_RSP_TA_CNTR \t PAD_CMD_D_DLY \r\n");

	uCmdEdge = 0;
	do {
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
		for (i = 0; i < 8; i++) {
			g_ta_raw[i] = 0;
			msdc_autok_adjust_param(host, CMD_RSP_TA_CNTR, &i, AUTOK_WRITE);
			for (j = 0; j < 32; j++) {
				msdc_autok_adjust_param(host, CMD_RD_D_DLY1, &j, AUTOK_WRITE);

				for (k = 0; k < AUTOK_CMD_TIMES; k++) {
					ret = autok_send_tune_cmd(host, opcode, TUNE_CMD);

					if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
						g_ta_raw[i] |= (1 << j);
						break;
					}
				}
			}
			g_ta_score[i] = autok_simple_score(tune_result_str32, g_ta_raw[i]);
			AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]%d \t %d\t %d \t %s\r\n", uCmdEdge, i,
				g_ta_score[i], tune_result_str32);
		}

		if (autok_ta_sel(g_ta_raw, &res_cmd_ta) == 0) {
			msdc_autok_adjust_param(host, CMD_RSP_TA_CNTR, &res_cmd_ta, AUTOK_WRITE);
			AUTOK_RAWPRINT("[AUTOK]CMD_TA Sel:%d\r\n", res_cmd_ta);
			break;
		}
		AUTOK_RAWPRINT
		    ("[AUTOK]Internal Boundary Occours,need switch cmd edge for rescan!\r\n");
			uCmdEdge ^= 1;	/* TA select Fail need switch cmd edge for rescan */
	} while (uCmdEdge);

	/************************************************************
	 *                                                          *
	 *  Step 2 : Scan CMD Pad Delay Base on CMD_EDGE & CKGEN    *
	 *                                                          *
	 ************************************************************/
	autok_tuning_parameter_init(host, p_autok_tune_res);
	AUTOK_RAWPRINT("[AUTOK]Step2.Scan CMD Pad Data Delay\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD_EDGE \t CKGEN \t CMD_RD_R_DLY1~2 \r\n");
	uCmdEdge = 0;

	do {
		msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);

		for (i = 0; i < 32; i++) {
			RawData64 = 0LL;
			msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &i, AUTOK_WRITE);

			for (j = 0; j < 64; j++) {
				msdc_autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
				for (k = 0; k < AUTOK_CMD_TIMES; k++) {
					ret = autok_send_tune_cmd(host, opcode, TUNE_CMD);
					if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
						RawData64 |= (u64)(1LL << j);
						break;
					}
				}
			}
			score = autok_simple_score64(tune_result_str64, RawData64);
			AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]%d \t %d \t %d \t %s\r\n",
			/*(u32)((RawData64>>32)&0xFFFFFFFF),(u32)(RawData64&0xFFFFFFFF), */
			uCmdEdge, i, score, tune_result_str64);
		}

		uCmdEdge ^= 0x1;
	} while (uCmdEdge);


	/************************************************************
	 *                                                          *
	 *  Step 3 : Scan Dat Pad Delay Base on Data_EDGE & CKGEN   *
	 *                                                          *
	 ************************************************************/
	autok_tuning_parameter_init(host, p_autok_tune_res);
	AUTOK_RAWPRINT("[AUTOK]Step3.Scan Dat Pad Delay\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
		       "[AUTOK]CMD_EDGE \t DAT_EDGE \t CKGEN \t DAT_RD_R_DLY1~2 \r\n");
	uCmdEdge = 0;

	do {
		msdc_autok_adjust_param(host, RD_FIFO_EDGE, &uDatEdge, AUTOK_WRITE);
		for (i = 0; i < 32; i++) {
			RawData64 = 0LL;
			msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &i, AUTOK_WRITE);

			for (j = 0; j < 64; j++) {
				msdc_autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
rd_retry:
				for (k = 0; k < AUTOK_CMD_TIMES; k++) {
					ret = autok_send_tune_cmd(host, opcode, TUNE_DATA);
					if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
						uCmdEdge ^= 0x1;
						msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge,
									AUTOK_WRITE);
						goto rd_retry;
					} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
						RawData64 |= (u64) (1LL << j);
						break;
					}
				}
			}
			score = autok_simple_score64(tune_result_str64, RawData64);
			AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK][%d] \t %d \t %d \t %d \t %s\r\n",
				       uCmdEdge, uDatEdge, i, score, tune_result_str64);
		}

		uDatEdge ^= 0x1;
	} while (uDatEdge);

#if defined(SDIO_TUNE_WRITE_PATH)
	/************************************************************
	 *                                                          *
	 *  Step 4: Scan Write Pad Delay Base on WCRC_EDGE & CKGEN  *
	 *                                                          *
	 ************************************************************/
	autok_tuning_parameter_init(host, p_autok_tune_res);
	AUTOK_RAWPRINT("[AUTOK]Step4.Scan Data Pad Delay on Write Path\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
		       "[AUTOK]CMD_EDGE \t WCRC_EDGE \t CKGEN \t DAT_RD_R_DLY1~2 \r\n");
	uCmdEdge = 0;

	do {
		msdc_autok_adjust_param(host, WD_FIFO_EDGE, &uWCrcEdge, AUTOK_WRITE);
		for (i = 0; i < 32; i++) {
			RawData64 = 0LL;
			msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &i, AUTOK_WRITE);

			for (j = 0; j < 64; j++) {
				msdc_autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
wd_retry:
				for (k = 0; k < 2; k++) {
					ret = autok_cmd53_tune_cccr_write(host);
					if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
						uCmdEdge ^= 0x1;
						msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge,
									AUTOK_WRITE);
						goto wd_retry;
					} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
						RawData64 |= (u64) (1LL << j);
						break;
					}
				}
			}
			score = autok_simple_score64(tune_result_str64, RawData64);
			AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK][%d] \t %d \t %d \t %d \t %s\r\n",
				       uCmdEdge, uWCrcEdge, i, score, tune_result_str64);
		}

		uWCrcEdge ^= 0x1;
	} while (uWCrcEdge);

	uCkgenSel = 0;
	msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &uCkgenSel, AUTOK_WRITE);
	/************************************************************
	 *                                                          *
	 *  Step 5 : Scan Write TA Base on Pad Delay                *
	 *                                                          *
	 ************************************************************/
	autok_tuning_parameter_init(host, p_autok_tune_res);
	AUTOK_RAWPRINT("[AUTOK]Step5.Scan WCRC_TA Base on Data Pad Delay\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
		       "[AUTOK]WCRC_EDGE \t WRDAT_CRCS_TA_CNTR \t DAT_RD_R_DLY1~2 \r\n");
	uCmdEdge = 0;
	uWCrcEdge = 0;

	do {
		msdc_autok_adjust_param(host, WD_FIFO_EDGE, &uWCrcEdge, AUTOK_WRITE);
		for (i = 0; i < 8; i++) {
			g_ta_raw[i] = 0;
			msdc_autok_adjust_param(host, WRDAT_CRCS_TA_CNTR, &i, AUTOK_WRITE);
			for (j = 0; j < 32; j++) {
				msdc_autok_adjust_paddly(host, &j, DAT_PAD_RDLY);


retry:
				for (k = 0; k < AUTOK_CMD_TIMES; k++) {
					ret = autok_cmd53_tune_cccr_write(host);
					if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
						uCmdEdge ^= 0x1;
						msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge,
									AUTOK_WRITE);
						goto retry;
					} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
						g_ta_raw[i] |= (1 << j);
						break;
					}
				}
			}
			g_ta_score[i] = autok_simple_score(tune_result_str32, g_ta_raw[i]);
			AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK][%d] \t %d \t %d\t %d \t %s\r\n",
				       uCmdEdge, uWCrcEdge, i, g_ta_score[i], tune_result_str32);
		}

		if (autok_ta_sel(g_ta_raw, &res_wcrc_ta) == 0) {
			msdc_autok_adjust_param(host, WRDAT_CRCS_TA_CNTR, &res_wcrc_ta, AUTOK_WRITE);
			AUTOK_RAWPRINT("[AUTOK]CMD_TA Sel:%d\r\n", res_wcrc_ta);
			break;
		}
		AUTOK_RAWPRINT
		    ("[AUTOK]Internal Boundary Occours,need switch cmd edge for rescan!\r\n");
			uWCrcEdge ^= 1;	/* TA select Fail need switch cmd edge for rescan */
	} while (uWCrcEdge);
#endif
	/* Debug Info for reference */
	/************************************************************
	 *                                                          *
	 *  Step [1] : Scan CMD Pad Delay Base on CMD_EDGE & CKGEN  *
	 *                                                          *
	 ************************************************************/
	autok_tuning_parameter_init(host, p_autok_tune_res);
	AUTOK_RAWPRINT("[AUTOK]Step[1].Scan CMD Pad Data Delay\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]CMD_EDGE \t CKGEN \t CMD_RD_R_DLY1~2 \r\n");
	uCmdEdge = 0;

	for (i = 0; i < 32; i++) {
		msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &i, AUTOK_WRITE);

		do {
			msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge, AUTOK_WRITE);
			RawData64 = 0LL;

			for (j = 0; j < 64; j++) {
				msdc_autok_adjust_paddly(host, &j, CMD_PAD_RDLY);
				for (k = 0; k < AUTOK_CMD_TIMES; k++) {
					ret = autok_send_tune_cmd(host, opcode, TUNE_CMD);
					if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
						RawData64 |= (u64) (1LL << j);
						break;
					}
				}
			}
			score = autok_simple_score64(tune_result_str64, RawData64);
			AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK]%d \t %d \t %d \t %s\r\n",
				/*(u32)((RawData64>>32)&0xFFFFFFFF),(u32)(RawData64&0xFFFFFFFF), */
				uCmdEdge, i, score, tune_result_str64);
			uCmdEdge ^= 0x1;

		} while (uCmdEdge);
	}

	/************************************************************
	 *                                                          *
	 *  Step [2] : Scan Dat Pad Delay Base on Data_EDGE & CKGEN *
	 *                                                          *
	 ************************************************************/
	autok_tuning_parameter_init(host, p_autok_tune_res);
	AUTOK_RAWPRINT("[AUTOK]Step[2].Scan Dat Pad Delay\r\n");
	AUTOK_DBGPRINT(AUTOK_DBG_RES,
		       "[AUTOK]CMD_EDGE \t DAT_EDGE \t CKGEN \t DAT_RD_R_DLY1~2 \r\n");
	uCmdEdge = 0;
	uDatEdge = 0;
	for (i = 0; i < 32; i++) {
		msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &i, AUTOK_WRITE);

		do {
			msdc_autok_adjust_param(host, RD_FIFO_EDGE, &uDatEdge, AUTOK_WRITE);
			RawData64 = 0LL;

			for (j = 0; j < 64; j++) {
				msdc_autok_adjust_paddly(host, &j, DAT_PAD_RDLY);
rd_retry1:
				for (k = 0; k < AUTOK_CMD_TIMES; k++) {
					ret = autok_send_tune_cmd(host, opcode, TUNE_DATA);
					if ((ret & (E_RESULT_CMD_TMO | E_RESULT_RSP_CRC)) != 0) {
						uCmdEdge ^= 0x1;
						msdc_autok_adjust_param(host, CMD_EDGE, &uCmdEdge,
									AUTOK_WRITE);
						goto rd_retry1;
					} else if ((ret & (E_RESULT_DAT_CRC | E_RESULT_DAT_TMO)) != 0) {
						RawData64 |= (u64) (1LL << j);
						break;
					}
				}
			}
			score = autok_simple_score64(tune_result_str64, RawData64);
			AUTOK_DBGPRINT(AUTOK_DBG_RES, "[AUTOK][%d] \t %d \t %d \t %d \t %s\r\n",
				       uCmdEdge, uDatEdge, i, score, tune_result_str64);

			uDatEdge ^= 0x1;
		} while (uDatEdge);

	}

	uCkgenSel = 0;
	msdc_autok_adjust_param(host, CKGEN_MSDC_DLY_SEL, &uCkgenSel, AUTOK_WRITE);

	AUTOK_RAWPRINT("[AUTOK]Offline Tune Alg Complete\r\n");

	return 0;
}

#if AUTOK_OFFLINE_TUNE_TX_ENABLE
int autok_offline_tuning_TX(struct msdc_host *host)
{
	int ret = 0;
	void __iomem *base = host->base;
	unsigned int response;
	unsigned int tune_tx_value;
	unsigned char tune_cnt;
	unsigned char i;
	unsigned char tune_crc_cnt[32];
	unsigned char tune_pass_cnt[32];
	unsigned char tune_tmo_cnt[32];
	char tune_result[33];
	unsigned int cmd_tx;
	unsigned int dat0_tx;
	unsigned int dat1_tx;
	unsigned int dat2_tx;
	unsigned int dat3_tx;
	unsigned int dat4_tx;
	unsigned int dat5_tx;
	unsigned int dat6_tx;
	unsigned int dat7_tx;

	AUTOK_RAWPRINT("[AUTOK][tune cmd TX]=========start========\r\n");
	/* store tx setting */
	MSDC_GET_FIELD(EMMC50_PAD_CMD_TUNE, MSDC_EMMC50_PAD_CMD_TUNE_TXDLY, cmd_tx);
	MSDC_GET_FIELD(EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT0_TXDLY, dat0_tx);
	MSDC_GET_FIELD(EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT1_TXDLY, dat1_tx);
	MSDC_GET_FIELD(EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT2_TXDLY, dat2_tx);
	MSDC_GET_FIELD(EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT3_TXDLY, dat3_tx);
	MSDC_GET_FIELD(EMMC50_PAD_DAT45_TUNE, MSDC_EMMC50_PAD_DAT4_TXDLY, dat4_tx);
	MSDC_GET_FIELD(EMMC50_PAD_DAT45_TUNE, MSDC_EMMC50_PAD_DAT5_TXDLY, dat5_tx);
	MSDC_GET_FIELD(EMMC50_PAD_DAT67_TUNE, MSDC_EMMC50_PAD_DAT6_TXDLY, dat6_tx);
	MSDC_GET_FIELD(EMMC50_PAD_DAT67_TUNE, MSDC_EMMC50_PAD_DAT7_TXDLY, dat7_tx);

	/* Step1 : Tuning Cmd TX */
	for (tune_tx_value = 0; tune_tx_value < 32; tune_tx_value++) {
		tune_tmo_cnt[tune_tx_value] = 0;
		tune_crc_cnt[tune_tx_value] = 0;
		tune_pass_cnt[tune_tx_value] = 0;
		MSDC_SET_FIELD(EMMC50_PAD_CMD_TUNE, MSDC_EMMC50_PAD_CMD_TUNE_TXDLY, tune_tx_value);
		for (tune_cnt = 0; tune_cnt < TUNE_TX_CNT; tune_cnt++) {
			ret = autok_send_tune_cmd(host, MMC_SEND_STATUS, TUNE_CMD);
			if ((ret & E_RESULT_CMD_TMO) != 0)
				tune_tmo_cnt[tune_tx_value]++;
			else if ((ret&(E_RESULT_RSP_CRC)) != 0)
				tune_crc_cnt[tune_tx_value]++;
			else if ((ret & (E_RESULT_PASS)) == 0)
				tune_pass_cnt[tune_tx_value]++;
		}

		/* AUTOK_RAWPRINT("[AUTOK]tune_cmd_TX cmd_tx_value = %d tmo_cnt = %d crc_cnt = %d pass_cnt = %d\n",
		 * tune_tx_value, tune_tmo_cnt[tune_tx_value],tune_crc_cnt[tune_tx_value],
		 * tune_pass_cnt[tune_tx_value]);
		 */
	}

	/* print result */
	for (i = 0; i < 32; i++) {
		if ((tune_tmo_cnt[i] != 0) || (tune_crc_cnt[i] != 0))
			tune_result[i] = 'X';
		else if (tune_pass_cnt[i] == TUNE_TX_CNT)
			tune_result[i] = 'O';
	}
	tune_result[32] = '\0';
	AUTOK_RAWPRINT("[AUTOK]tune_cmd_TX 0 - 31      %s\r\n", tune_result);
	AUTOK_RAWPRINT("[AUTOK][tune cmd TX]=========end========\r\n");

	/* restore cmd tx setting */
	MSDC_SET_FIELD(EMMC50_PAD_CMD_TUNE, MSDC_EMMC50_PAD_CMD_TUNE_TXDLY, cmd_tx);
	AUTOK_RAWPRINT("[AUTOK][tune data TX]=========start========\r\n");

	/* Step2 : Tuning Data TX */
	for (tune_tx_value = 0; tune_tx_value < 32; tune_tx_value++) {
		tune_tmo_cnt[tune_tx_value] = 0;
		tune_crc_cnt[tune_tx_value] = 0;
		tune_pass_cnt[tune_tx_value] = 0;
		MSDC_SET_FIELD(EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT0_TXDLY, tune_tx_value);
		MSDC_SET_FIELD(EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT1_TXDLY, tune_tx_value);
		MSDC_SET_FIELD(EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT2_TXDLY, tune_tx_value);
		MSDC_SET_FIELD(EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT3_TXDLY, tune_tx_value);
		MSDC_SET_FIELD(EMMC50_PAD_DAT45_TUNE, MSDC_EMMC50_PAD_DAT4_TXDLY, tune_tx_value);
		MSDC_SET_FIELD(EMMC50_PAD_DAT45_TUNE, MSDC_EMMC50_PAD_DAT5_TXDLY, tune_tx_value);
		MSDC_SET_FIELD(EMMC50_PAD_DAT67_TUNE, MSDC_EMMC50_PAD_DAT6_TXDLY, tune_tx_value);
		MSDC_SET_FIELD(EMMC50_PAD_DAT67_TUNE, MSDC_EMMC50_PAD_DAT7_TXDLY, tune_tx_value);

		for (tune_cnt = 0; tune_cnt < TUNE_TX_CNT; tune_cnt++) {
			/* check device status */
			response = 0;
			while (((response >> 9) & 0xF) != 4) {
				ret = autok_send_tune_cmd(host, MMC_SEND_STATUS, TUNE_CMD);
				if ((ret & (E_RESULT_RSP_CRC | E_RESULT_CMD_TMO)) != 0) {
					AUTOK_RAWPRINT("[AUTOK]------while tune data TX cmd13 occur error------\r\n");
					AUTOK_RAWPRINT("[AUTOK]------tune data TX fail------\r\n");
					goto end;
				}
				response = MSDC_READ32(SDC_RESP0);
				if ((((response >> 9) & 0xF) == 5) || (((response >> 9) & 0xF) == 6))
					ret = autok_send_tune_cmd(host, MMC_STOP_TRANSMISSION, TUNE_CMD);
			}

			/* send cmd24 write one block data */
			ret = autok_send_tune_cmd(host, MMC_WRITE_BLOCK, TUNE_DATA);
			response = MSDC_READ32(SDC_RESP0);
			if ((ret & (E_RESULT_RSP_CRC | E_RESULT_CMD_TMO)) != 0) {
				AUTOK_RAWPRINT("[AUTOK]------while tune data TX cmd%d occur error------\n",
					MMC_WRITE_BLOCK);
				AUTOK_RAWPRINT("[AUTOK]------tune data TX fail------\n");
				goto end;
			}
			if ((ret & E_RESULT_DAT_TMO) != 0)
				tune_tmo_cnt[tune_tx_value]++;
			else if ((ret & (E_RESULT_DAT_CRC)) != 0)
				tune_crc_cnt[tune_tx_value]++;
			else if ((ret & (E_RESULT_PASS)) == 0)
				tune_pass_cnt[tune_tx_value]++;
		}

		/* AUTOK_RAWPRINT("[AUTOK]tune_data_TX data_tx_value = %d tmo_cnt = %d crc_cnt = %d pass_cnt = %d\n",
		 * tune_tx_value, tune_tmo_cnt[tune_tx_value],tune_crc_cnt[tune_tx_value],
		 * tune_pass_cnt[tune_tx_value]);
		 */
	}

	/* print result */
	for (i = 0; i < 32; i++) {
		if ((tune_tmo_cnt[i] != 0) || (tune_crc_cnt[i] != 0))
			tune_result[i] = 'X';
		else if (tune_pass_cnt[i] == TUNE_TX_CNT)
			tune_result[i] = 'O';
	}
	tune_result[32] = '\0';
	AUTOK_RAWPRINT("[AUTOK]tune_data_TX 0 - 31      %s\r\n", tune_result);

	/* restore data tx setting */
	MSDC_SET_FIELD(EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT0_TXDLY, dat0_tx);
	MSDC_SET_FIELD(EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT1_TXDLY, dat1_tx);
	MSDC_SET_FIELD(EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT2_TXDLY, dat2_tx);
	MSDC_SET_FIELD(EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT3_TXDLY, dat3_tx);
	MSDC_SET_FIELD(EMMC50_PAD_DAT45_TUNE, MSDC_EMMC50_PAD_DAT4_TXDLY, dat4_tx);
	MSDC_SET_FIELD(EMMC50_PAD_DAT45_TUNE, MSDC_EMMC50_PAD_DAT5_TXDLY, dat5_tx);
	MSDC_SET_FIELD(EMMC50_PAD_DAT67_TUNE, MSDC_EMMC50_PAD_DAT6_TXDLY, dat6_tx);
	MSDC_SET_FIELD(EMMC50_PAD_DAT67_TUNE, MSDC_EMMC50_PAD_DAT7_TXDLY, dat7_tx);

	AUTOK_RAWPRINT("[AUTOK][tune data TX]=========end========\r\n");

end:
	return ret;
}
#endif

int autok_execute_tuning(struct msdc_host *host, u8 *res)
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

#if AUTOK_OFFLINE_TUNE_ENABLE
	if (execute_offline_tuning(host) != 0)
		AUTOK_RAWPRINT("[AUTOK] ========Error: Autok OFFLINE Failed========");
#endif
	if (execute_online_tuning(host, res) != 0)
		AUTOK_RAWPRINT("[AUTOK] ========Error: Autok Failed========");

	autok_msdc_reset();
	msdc_clear_fifo();
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000 + (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK]=========Time Cost:%d ms========\r\n", tm_val);

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

	do_gettimeofday(&tm_s);
	int_en = MSDC_READ32(MSDC_INTEN);
	MSDC_WRITE32(MSDC_INTEN, 0);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, 1);

#if HS400_OFFLINE_TUNE_ENABLE
	if (execute_offline_tuning_hs400(host) != 0)
		AUTOK_RAWPRINT("[AUTOK] ========Error: Autok HS400 OFFLINE Failed========");
#endif
	if (execute_online_tuning_hs400(host, res) != 0)
		AUTOK_RAWPRINT("[AUTOK] ========Error: Autok HS400 Failed========");
#if AUTOK_OFFLINE_TUNE_TX_ENABLE
	if (do_autok_offline_tune_tx)
		autok_offline_tuning_TX(host);
#endif

	autok_msdc_reset();
	msdc_clear_fifo();
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000 + (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK][HS400]=========Time Cost:%d ms========\r\n", tm_val);

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
	if (execute_cmd_online_tuning(host, res) != 0)
		AUTOK_RAWPRINT("[AUTOK only for cmd] ========Error: Autok HS400 Failed========");

	autok_msdc_reset();
	msdc_clear_fifo();
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000 + (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK][HS400 only for cmd]=========Time Cost:%d ms========\r\n", tm_val);

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

	do_gettimeofday(&tm_s);
	int_en = MSDC_READ32(MSDC_INTEN);
	MSDC_WRITE32(MSDC_INTEN, 0);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, 1);

#if HS200_OFFLINE_TUNE_ENABLE
	if (execute_offline_tuning(host) != 0)
		AUTOK_RAWPRINT("[AUTOK] ========Error: Autok OFFLINE HS200 Failed========");
#endif
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	if (execute_online_tuning_hs200(host, res) != 0)
		AUTOK_RAWPRINT("[AUTOK] ========Error: Autok HS200 Failed========");

	autok_msdc_reset();
	msdc_clear_fifo();
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000 + (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK][HS200]=========Time Cost:%d ms========\r\n", tm_val);

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
	if (execute_cmd_online_tuning(host, res) != 0)
		AUTOK_RAWPRINT("[AUTOK only for cmd] ========Error: Autok HS200 Failed========");

	autok_msdc_reset();
	msdc_clear_fifo();
	MSDC_WRITE32(MSDC_INT, 0xffffffff);
	MSDC_WRITE32(MSDC_INTEN, int_en);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKPDN, clk_pwdn);

	do_gettimeofday(&tm_e);
	tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000 + (tm_e.tv_usec - tm_s.tv_usec) / 1000;
	AUTOK_RAWPRINT("[AUTOK][HS200 only for cmd]=========Time Cost:%d ms========\r\n", tm_val);

	return ret;
}
EXPORT_SYMBOL(hs200_execute_tuning_cmd);

