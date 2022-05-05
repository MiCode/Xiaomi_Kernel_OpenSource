// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "cqhci.h"
#include "mtk-mmc.h"
#include "mtk-mmc-dbg.h"
#include "../core/queue.h"
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/tracepoint.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <mt-plat/mrdump.h>

/*  For msdc register dump  */
u16 msdc_offsets[] = {
	MSDC_CFG,
	MSDC_IOCON,
	MSDC_PS,
	MSDC_INT,
	MSDC_INTEN,
	MSDC_FIFOCS,
	SDC_CFG,
	SDC_CMD,
	SDC_ARG,
	SDC_STS,
	SDC_RESP0,
	SDC_RESP1,
	SDC_RESP2,
	SDC_RESP3,
	SDC_BLK_NUM,
	SDC_ADV_CFG0,
	MSDC_NEW_RX_CFG,
	EMMC_IOCON,
	SDC_ACMD_RESP,
	DMA_SA_H4BIT,
	MSDC_DMA_SA,
	MSDC_DMA_CTRL,
	MSDC_DMA_CFG,
	MSDC_DBG_SEL,
	MSDC_DBG_OUT,
	MSDC_PATCH_BIT,
	MSDC_PATCH_BIT1,
	MSDC_PATCH_BIT2,
	MSDC_PAD_TUNE,
	MSDC_PAD_TUNE0,
	MSDC_PAD_TUNE1,
	MSDC_DAT_RDDLY0,
	MSDC_DAT_RDDLY1,
	MSDC_PAD_CTL0,
	PAD_DS_TUNE,
	PAD_CMD_TUNE,
	EMMC50_PAD_DAT01_TUNE,
	EMMC50_PAD_DAT23_TUNE,
	EMMC50_PAD_DAT45_TUNE,
	EMMC50_PAD_DAT67_TUNE,
	EMMC51_CFG0,
	EMMC50_CFG0,
	EMMC50_CFG1,
	EMMC50_CFG3,
	EMMC50_CFG4,
	SDC_FIFO_CFG,
	CQHCI_SETTING,

	0xFFFF /*as mark of end */
};

u16 msdc_offsets_top[] = {
	EMMC_TOP_CONTROL,
	EMMC_TOP_CMD,
	EMMC50_PAD_CTL0,
	EMMC50_PAD_DS_TUNE,
	EMMC50_PAD_DAT0_TUNE,
	EMMC50_PAD_DAT1_TUNE,
	EMMC50_PAD_DAT2_TUNE,
	EMMC50_PAD_DAT3_TUNE,
	EMMC50_PAD_DAT4_TUNE,
	EMMC50_PAD_DAT5_TUNE,
	EMMC50_PAD_DAT6_TUNE,
	EMMC50_PAD_DAT7_TUNE,
	LOOP_TEST_CONTROL,
	MSDC_TOP_NEW_RX_CFG,

	0xFFFF /*as mark of end */
};

/*---------------------------------------------------------------------*/
/* Command dump                                                        */
/*---------------------------------------------------------------------*/
struct mmc_host *mtk_mmc_host[] = {NULL, NULL};
/*#define MTK_MSDC_ERROR_TUNE_DEBUG*/

#define dbg_max_cnt (4000)
#define sd_dbg_max_cnt (500)
#define MMC_AEE_BUFFER_SIZE (300 * 1024)

struct dbg_run_host_log {
	unsigned long long time_sec;
	unsigned long long time_usec;
	int type;
	int cmd;
	int arg;
	int skip;
};

struct dbg_task_log {
	u32 address;
	unsigned long long size;
};
struct dbg_dma_cmd_log {
	unsigned long long time;
	int cmd;
	int arg;
};

static unsigned int dbg_host_cnt;
static unsigned int dbg_sd_cnt;

static struct dbg_run_host_log dbg_run_host_log_dat[dbg_max_cnt];
static struct dbg_run_host_log dbg_run_sd_log_dat[sd_dbg_max_cnt];

static unsigned int print_cpu_test = UINT_MAX;

static spinlock_t cmd_hist_lock;
static bool cmd_hist_init;
static bool cmd_hist_enabled;
static char *mmc_aee_buffer;

/**
 * Data structures to store tracepoints information
 */
struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
};
/***********************************************************************/
static void msdc_dump_clock_sts_core(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	char buffer[512] = {0};
	char *buf_ptr = buffer;
	int n = 0;

	if (host->bulk_clks[0].clk)
		n += scnprintf(&buf_ptr[n], sizeof(buffer) - n,
			"[%s]enable:%d freq:%lu,",
			host->bulk_clks[0].id,
			__clk_is_enabled(host->bulk_clks[0].clk),
			clk_get_rate(host->bulk_clks[0].clk));
	if (host->bulk_clks[1].clk)
		n += scnprintf(&buf_ptr[n], sizeof(buffer) - n,
			"[%s]enable:%d freq:%lu,",
			host->bulk_clks[1].id,
			__clk_is_enabled(host->bulk_clks[1].clk),
			clk_get_rate(host->bulk_clks[1].clk));
	if (host->bulk_clks[2].clk)
		n += scnprintf(&buf_ptr[n], sizeof(buffer) - n,
			"[%s]enable:%d freq:%lu,",
			host->bulk_clks[2].id,
			__clk_is_enabled(host->bulk_clks[2].clk),
			clk_get_rate(host->bulk_clks[2].clk));
	if (host->src_clk)
		n += scnprintf(&buf_ptr[n], sizeof(buffer) - n,
			"[src_clk]enable:%d freq:%lu,",
			__clk_is_enabled(host->src_clk), clk_get_rate(host->src_clk));
	if (host->h_clk)
		n += scnprintf(&buf_ptr[n], sizeof(buffer) - n,
			"[h_clk]enable:%d freq:%lu,",
			__clk_is_enabled(host->h_clk), clk_get_rate(host->h_clk));
	if (host->bus_clk)
		n += scnprintf(&buf_ptr[n], sizeof(buffer) - n,
			"[bus_clk]enable:%d freq:%lu,",
			__clk_is_enabled(host->bus_clk), clk_get_rate(host->bus_clk));
	if (host->src_clk_cg)
		n += scnprintf(&buf_ptr[n], sizeof(buffer) - n,
			"[src_clk_cg]enable:%d freq:%lu\n",
			__clk_is_enabled(host->src_clk_cg), clk_get_rate(host->src_clk_cg));

	if (host->crypto_clk)
		n += scnprintf(&buf_ptr[n], sizeof(buffer) - n,
			"[crypto_clk]enable:%d freq:%lu\n",
			__clk_is_enabled(host->crypto_clk), clk_get_rate(host->crypto_clk));

	if (host->crypto_cg)
		n += scnprintf(&buf_ptr[n], sizeof(buffer) - n,
			"[crypto_cg]enable:%d freq:%lu\n",
			__clk_is_enabled(host->crypto_cg), clk_get_rate(host->crypto_cg));

	SPREAD_PRINTF(buff, size, m, "%s", buffer);
}

void msdc_dump_clock_sts(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	msdc_dump_clock_sts_core(buff, size, m, host);
}

void msdc_dump_ldo_sts(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	u32 id = host->id;
	struct mmc_host *mmc = mmc_from_priv(host);

	switch (id) {
	/*
	 * PMIC only provide regulator APIs with mutex protection.
	 * Therefore, can not dump msdc ldo status in IRQ context.
	 * Enable dump msdc ldo if you make sure the dump context
	 * is correct.
	 */
	case MSDC_EMMC:
		SPREAD_PRINTF(buff, size, m,
			" VEMC_EN=0x%x, VEMC_VOL=%duV\n",
			regulator_is_enabled(mmc->supply.vmmc),
			regulator_get_voltage(mmc->supply.vmmc));
		break;
	case MSDC_SD:
		SPREAD_PRINTF(buff, size, m,
		" VMCH_EN=0x%x, VMCH_VOL=%duV\n",
			regulator_is_enabled(mmc->supply.vmmc),
			regulator_get_voltage(mmc->supply.vmmc));
		SPREAD_PRINTF(buff, size, m,
		" VMC_EN=0x%x, VMC_VOL=%duV\n",
			regulator_is_enabled(mmc->supply.vqmmc),
			regulator_get_voltage(mmc->supply.vqmmc));
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(msdc_dump_ldo_sts);

void msdc_dump_register_core(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	u32 id = host->id;
	u32 msg_size = 0;
	u32 val;
	u16 offset, i;
	char buffer[PRINTF_REGISTER_BUFFER_SIZE + 1];
	char *buffer_cur_ptr = buffer;

	memset(buffer, 0, PRINTF_REGISTER_BUFFER_SIZE);
	SPREAD_PRINTF(buff, size, m, "MSDC%d normal register\n", id);
	for (i = 0; msdc_offsets[i] != (u16)0xFFFF; i++) {
		offset = msdc_offsets[i];
		val = readl(host->base + offset);
		MSDC_REG_PRINT(offset, val, ONE_REGISTER_STRING_SIZE, msg_size,
			PRINTF_REGISTER_BUFFER_SIZE, buffer, buffer_cur_ptr, m);
	}
	SPREAD_PRINTF(buff, size, m, "%s\n", buffer);

	if (!host->top_base)
		return;

	MSDC_RST_REG_PRINT_BUF(msg_size,
		PRINTF_REGISTER_BUFFER_SIZE, buffer, buffer_cur_ptr);

	SPREAD_PRINTF(buff, size, m, "MSDC%d top register\n", id);

	for (i = 0;  msdc_offsets_top[i] != (u16)0xFFFF; i++) {
		offset = msdc_offsets_top[i];
		val = readl(host->top_base + offset);
		MSDC_REG_PRINT(offset, val, ONE_REGISTER_STRING_SIZE, msg_size,
			PRINTF_REGISTER_BUFFER_SIZE, buffer, buffer_cur_ptr, m);
	}
	SPREAD_PRINTF(buff, size, m, "%s\n", buffer);
}

void msdc_dump_register(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	msdc_dump_register_core(buff, size, m, host);
}

void msdc_dump_dbg_register(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	u32 msg_size = 0;
	u16 i;
	char buffer[PRINTF_REGISTER_BUFFER_SIZE + 1];
	char *buffer_cur_ptr = buffer;

	memset(buffer, 0, PRINTF_REGISTER_BUFFER_SIZE);
	SPREAD_PRINTF(buff, size, m, "MSDC debug register [set:out]\n");
	for (i = 0; i < MSDC_DEBUG_REGISTER_COUNT + 1; i++) {
		msg_size += ONE_REGISTER_STRING_SIZE;
		if (msg_size >= PRINTF_REGISTER_BUFFER_SIZE) {
			SPREAD_PRINTF(buff, size, m, "%s", buffer);
			memset(buffer, 0, PRINTF_REGISTER_BUFFER_SIZE);
			msg_size = ONE_REGISTER_STRING_SIZE;
			buffer_cur_ptr = buffer;
		}
		writel(i, host->base + MSDC_DBG_SEL);
		snprintf(buffer_cur_ptr, ONE_REGISTER_STRING_SIZE + 1,
			"[%.3hx:%.8x]", i, readl(host->base + MSDC_DBG_OUT));
		buffer_cur_ptr += ONE_REGISTER_STRING_SIZE;
	}
	SPREAD_PRINTF(buff, size, m, "%s\n", buffer);

	writel(0x27, host->base + MSDC_DBG_SEL);
	msg_size = 0;
	memset(buffer, 0, PRINTF_REGISTER_BUFFER_SIZE);
	buffer_cur_ptr = buffer;
	SPREAD_PRINTF(buff, size, m, "MSDC debug 0x224 register [set:out]\n");
	for (i = 0; i < 12; i++) {
		msg_size += ONE_REGISTER_STRING_SIZE;
		if (msg_size >= PRINTF_REGISTER_BUFFER_SIZE) {
			SPREAD_PRINTF(buff, size, m, "%s", buffer);
			memset(buffer, 0, PRINTF_REGISTER_BUFFER_SIZE);
			msg_size = ONE_REGISTER_STRING_SIZE;
			buffer_cur_ptr = buffer;
		}
		writel(i, host->base + EMMC50_CFG4);
		snprintf(buffer_cur_ptr, ONE_REGISTER_STRING_SIZE + 1,
			"[%.3hx:%.8x]", i, readl(host->base + MSDC_DBG_OUT));
		buffer_cur_ptr += ONE_REGISTER_STRING_SIZE;
	}
	SPREAD_PRINTF(buff, size, m, "%s\n", buffer);

	writel(0, host->base + MSDC_DBG_SEL);
}

void msdc_dump_autok(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	int i, j;
	int bit_pos, byte_pos, start;
	char buf[65];

	SPREAD_PRINTF(buff, size, m, "[AUTOK]VER : 0x%02x%02x%02x%02x\r\n",
		host->autok_res[0][AUTOK_VER3],
		host->autok_res[0][AUTOK_VER2],
		host->autok_res[0][AUTOK_VER1],
		host->autok_res[0][AUTOK_VER0]);

	for (i = AUTOK_VCORE_LEVEL1; i >= AUTOK_VCORE_LEVEL0; i--) {
		start = CMD_SCAN_R0;
		for (j = 0; j < 64; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]CMD Rising \t: %s\r\n", buf);

		start = CMD_SCAN_F0;
		for (j = 0; j < 64; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]CMD Falling \t: %s\r\n", buf);

		start = DAT_SCAN_R0;
		for (j = 0; j < 64; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]DAT Rising \t: %s\r\n", buf);

		start = DAT_SCAN_F0;
		for (j = 0; j < 64; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]DAT Falling \t: %s\r\n", buf);

		/* cmd response use ds pin, but window is
		 * different with data pin, because cmd response is SDR.
		 */
		start = DS_CMD_SCAN_0;
		for (j = 0; j < 64; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]DS CMD Window \t: %s\r\n", buf);

		start = DS_DAT_SCAN_0;
		for (j = 0; j < 64; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]DS DAT Window \t: %s\r\n", buf);

		start = D_DATA_SCAN_0;
		for (j = 0; j < 32; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]Device Data RX \t: %s\r\n", buf);

		start = H_DATA_SCAN_0;
		for (j = 0; j < 32; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]Host   Data TX \t: %s\r\n", buf);

		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]CMD [EDGE:%d CMD_FIFO_EDGE:%d DLY1:%d DLY2:%d]\r\n",
			host->autok_res[i][0], host->autok_res[i][1],
			host->autok_res[i][5], host->autok_res[i][7]);
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]DAT [RDAT_EDGE:%d RD_FIFO_EDGE:%d WD_FIFO_EDGE:%d]\r\n",
			host->autok_res[i][2], host->autok_res[i][3],
			host->autok_res[i][4]);
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]DAT [LATCH_CK:%d DLY1:%d DLY2:%d]\r\n",
			host->autok_res[i][13], host->autok_res[i][9],
			host->autok_res[i][11]);
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]DS  [DLY1:%d DLY2:%d DLY3:%d]\r\n",
			host->autok_res[i][14], host->autok_res[i][16],
			host->autok_res[i][18]);
		SPREAD_PRINTF(buff, size, m, "[AUTOK]DAT [TX SEL:%d]\r\n",
			host->autok_res[i][20]);
	}
}

void msdc_dump_info(char **buff, unsigned long *size, struct seq_file *m,
	struct msdc_host *host)
{
	if (host == NULL) {
		SPREAD_PRINTF(buff, size, m, "msdc host null\n");
		return;
	}

	if (host->tuning_in_progress == true)
		return;

	msdc_dump_register(buff, size, m, host);

	if (!buff)
		mdelay(10);

	msdc_dump_clock_sts(buff, size, m, host);

	msdc_dump_ldo_sts(buff, size, m, host);

	/* prevent bad sdcard, print too much log */
	if (host->id != MSDC_SD)
		msdc_dump_autok(buff, size, m, host);

	if (!buff)
		mdelay(10);

	msdc_dump_dbg_register(buff, size, m, host);
}
EXPORT_SYMBOL(msdc_dump_info);

static inline u8 *dbg_get_desc(struct cqhci_host *cq_host, u8 tag)
{
	return cq_host->desc_base + (tag * cq_host->slot_sz);
}

static void cqhci_prep_task_desc_dbg(struct mmc_request *mrq,
					u64 *data, bool intr)
{
	u32 req_flags = mrq->data->flags;

	*data = CQHCI_VALID(1) |
		CQHCI_END(1) |
		CQHCI_INT(intr) |
		CQHCI_ACT(0x5) |
		CQHCI_FORCED_PROG(!!(req_flags & MMC_DATA_FORCED_PRG)) |
		CQHCI_DATA_TAG(!!(req_flags & MMC_DATA_DAT_TAG)) |
		CQHCI_DATA_DIR(!!(req_flags & MMC_DATA_READ)) |
		CQHCI_PRIORITY(!!(req_flags & MMC_DATA_PRIO)) |
		CQHCI_QBAR(!!(req_flags & MMC_DATA_QBR)) |
		CQHCI_REL_WRITE(!!(req_flags & MMC_DATA_REL_WR)) |
		CQHCI_BLK_COUNT(mrq->data->blocks) |
		CQHCI_BLK_ADDR((u64)mrq->data->blk_addr);

}

static bool is_dcmd_request(struct mmc_request *mrq)
{
	struct mmc_queue_req *mqrq;
	struct request *req;
	struct mmc_queue *mq;
	struct mmc_host *host;

	/* skip non-cqe cmd */
	if (PTR_ERR(mrq->completion.wait.task_list.next)
		&& PTR_ERR(mrq->completion.wait.task_list.prev))
		return false;

	mqrq = container_of(mrq, struct mmc_queue_req, brq.mrq);
	req = blk_mq_rq_from_pdu(mqrq);

	if (!req || !req->q || !req->q->queuedata)
		return false;

	mq = (struct mmc_queue *)(req->q->queuedata);

	if (IS_ERR_OR_NULL(mq->card) || !mq->card->host)
		return false;

	host = mq->card->host;

	if (mq->use_cqe && !host->hsq_enabled) {
		if (req_op(req) == REQ_OP_FLUSH)
			return (host->caps2 & MMC_CAP2_CQE_DCMD) ? true : false;
	}
	return false;
}

static void __emmc_store_buf_start(void *__data, struct mmc_host *mmc,
	struct mmc_request *mrq)
{
	unsigned long long t, tn;
	unsigned long long nanosec_rem = 0;
	static int last_cmd, last_arg, skip;
	int l_skip = 0;
	u64 *task_desc = NULL;
	u64 data;
	struct cqhci_host *cq_host = NULL;
	unsigned long flags;
	struct msdc_host *host = NULL;

	if (!cmd_hist_enabled)
		return;

	if (!mmc)
		return;

	host = mmc_priv(mmc);
	cq_host = mmc->cqe_private;
	t = cpu_clock(print_cpu_test);
	tn = t;
	nanosec_rem = do_div(t, 1000000000)/1000;

	spin_lock_irqsave(&host->log_lock, flags);

	if (!(mrq->cmd) && cq_host && cq_host->desc_base) { /* CQE */
		task_desc = (__le64 __force *)dbg_get_desc(cq_host, mrq->tag);
		cqhci_prep_task_desc_dbg(mrq, &data, 1);
		*task_desc = cpu_to_le64(data);

		dbg_run_host_log_dat[dbg_host_cnt].time_sec = t;
		dbg_run_host_log_dat[dbg_host_cnt].time_usec = nanosec_rem;
		dbg_run_host_log_dat[dbg_host_cnt].type = 5;
		dbg_run_host_log_dat[dbg_host_cnt].cmd = MAGIC_CQHCI_DBG_NUM_L + mrq->tag;
		dbg_run_host_log_dat[dbg_host_cnt].arg = lower_32_bits(*task_desc);
		dbg_run_host_log_dat[dbg_host_cnt].skip = l_skip;
		dbg_host_cnt++;
		if (dbg_host_cnt >= dbg_max_cnt)
			dbg_host_cnt = 0;
		dbg_run_host_log_dat[dbg_host_cnt].time_sec = t;
		dbg_run_host_log_dat[dbg_host_cnt].time_usec = nanosec_rem;
		dbg_run_host_log_dat[dbg_host_cnt].type = 5;
		dbg_run_host_log_dat[dbg_host_cnt].cmd = MAGIC_CQHCI_DBG_NUM_U + mrq->tag;
		dbg_run_host_log_dat[dbg_host_cnt].arg = upper_32_bits(*task_desc);
		dbg_run_host_log_dat[dbg_host_cnt].skip = l_skip;
		dbg_host_cnt++;
	} else if (mrq->cmd) { /* non-CQE */
		/* skip log if last cmd rsp are the same */
		if (last_cmd == mrq->cmd->opcode &&
			last_arg == mrq->cmd->arg && mrq->cmd->opcode == 13) {
			skip++;
			if (dbg_host_cnt == 0)
				dbg_host_cnt = dbg_max_cnt;
			/* remove type = 0, command */
			dbg_host_cnt--;
			spin_unlock_irqrestore(&host->log_lock, flags);
			return;
		}
		last_cmd = mrq->cmd->opcode;
		last_arg = mrq->cmd->arg;
		l_skip = skip;
		skip = 0;

		if (is_dcmd_request(mrq)) {
			dbg_run_host_log_dat[dbg_host_cnt].time_sec = t;
			dbg_run_host_log_dat[dbg_host_cnt].time_usec = nanosec_rem;
			dbg_run_host_log_dat[dbg_host_cnt].type = 60;
			dbg_run_host_log_dat[dbg_host_cnt].cmd = mrq->cmd->opcode;
			dbg_run_host_log_dat[dbg_host_cnt].arg = mrq->cmd->arg;
			dbg_run_host_log_dat[dbg_host_cnt].skip = l_skip;
			dbg_host_cnt++;
		} else {
			dbg_run_host_log_dat[dbg_host_cnt].time_sec = t;
			dbg_run_host_log_dat[dbg_host_cnt].time_usec = nanosec_rem;
			dbg_run_host_log_dat[dbg_host_cnt].type = 0;
			dbg_run_host_log_dat[dbg_host_cnt].cmd = mrq->cmd->opcode;
			dbg_run_host_log_dat[dbg_host_cnt].arg = mrq->cmd->arg;
			dbg_run_host_log_dat[dbg_host_cnt].skip = l_skip;
			dbg_host_cnt++;
		}
	}

	if (dbg_host_cnt >= dbg_max_cnt)
		dbg_host_cnt = 0;

	spin_unlock_irqrestore(&host->log_lock, flags);
}

static void __emmc_store_buf_end(void *__data, struct mmc_host *mmc,
	struct mmc_request *mrq)
{
	unsigned long long t;
	unsigned long long nanosec_rem = 0;
	static int last_cmd, last_arg, skip;
	int l_skip = 0;
	struct cqhci_host *cq_host = NULL;
	unsigned long flags;
	struct msdc_host *host = NULL;

	if (!cmd_hist_enabled)
		return;

	if (!mmc)
		return;

	host = mmc_priv(mmc);
	cq_host = mmc->cqe_private;
	t = cpu_clock(print_cpu_test);

	nanosec_rem = do_div(t, 1000000000)/1000;

	spin_lock_irqsave(&host->log_lock, flags);

	if (!(mrq->cmd)) { /* CQE */
		dbg_run_host_log_dat[dbg_host_cnt].time_sec = t;
		dbg_run_host_log_dat[dbg_host_cnt].time_usec = nanosec_rem;
		dbg_run_host_log_dat[dbg_host_cnt].type = 5;
		dbg_run_host_log_dat[dbg_host_cnt].cmd = MAGIC_CQHCI_DBG_NUM_RI + mrq->tag;
		dbg_run_host_log_dat[dbg_host_cnt].arg = cqhci_readl(cq_host, CQHCI_CRA);
		dbg_run_host_log_dat[dbg_host_cnt].skip = l_skip;
		dbg_host_cnt++;
	} else if (mrq->cmd) { /* non-CQE */
		/* skip log if last cmd rsp are the same */
		if (last_cmd == mrq->cmd->opcode &&
			last_arg == mrq->cmd->arg && mrq->cmd->opcode == 13) {
			skip++;
			if (dbg_host_cnt == 0)
				dbg_host_cnt = dbg_max_cnt;
			/* remove type = 0, command */
			dbg_host_cnt--;
			spin_unlock_irqrestore(&host->log_lock, flags);
			return;
		}
		last_cmd = mrq->cmd->opcode;
		last_arg = mrq->cmd->resp[0];
		l_skip = skip;
		skip = 0;

		if (is_dcmd_request(mrq)) {
			dbg_run_host_log_dat[dbg_host_cnt].time_sec = t;
			dbg_run_host_log_dat[dbg_host_cnt].time_usec = nanosec_rem;
			dbg_run_host_log_dat[dbg_host_cnt].type = 61;
			dbg_run_host_log_dat[dbg_host_cnt].cmd = mrq->cmd->opcode;
			dbg_run_host_log_dat[dbg_host_cnt].arg = cqhci_readl(cq_host, CQHCI_CRDCT);
			dbg_run_host_log_dat[dbg_host_cnt].skip = l_skip;
			dbg_host_cnt++;
		} else {
			dbg_run_host_log_dat[dbg_host_cnt].time_sec = t;
			dbg_run_host_log_dat[dbg_host_cnt].time_usec = nanosec_rem;
			dbg_run_host_log_dat[dbg_host_cnt].type = 1;
			dbg_run_host_log_dat[dbg_host_cnt].cmd = mrq->cmd->opcode;
			dbg_run_host_log_dat[dbg_host_cnt].arg = mrq->cmd->resp[0];
			dbg_run_host_log_dat[dbg_host_cnt].skip = l_skip;
			dbg_host_cnt++;
		}
	}
	if (dbg_host_cnt >= dbg_max_cnt)
		dbg_host_cnt = 0;

	spin_unlock_irqrestore(&host->log_lock, flags);
}

static void __sd_store_buf_start(void *__data, struct mmc_host *mmc,
	struct mmc_request *mrq)
{
	unsigned long long t, tn;
	unsigned long long nanosec_rem;
	unsigned long flags;
	struct msdc_host *host = NULL;

	if (!cmd_hist_enabled)
		return;

	if (!mmc)
		return;

	host = mmc_priv(mmc);
	t = cpu_clock(print_cpu_test);
	tn = t;
	nanosec_rem = do_div(t, 1000000000)/1000;

	spin_lock_irqsave(&host->log_lock, flags);

	dbg_run_sd_log_dat[dbg_sd_cnt].time_sec = t;
	dbg_run_sd_log_dat[dbg_sd_cnt].time_usec = nanosec_rem;
	dbg_run_sd_log_dat[dbg_sd_cnt].type = 0;
	dbg_run_sd_log_dat[dbg_sd_cnt].cmd = mrq->cmd->opcode;
	dbg_run_sd_log_dat[dbg_sd_cnt].arg = mrq->cmd->arg;
	dbg_run_sd_log_dat[dbg_sd_cnt].skip = 0;
	dbg_sd_cnt++;

	if (dbg_sd_cnt >= sd_dbg_max_cnt)
		dbg_sd_cnt = 0;

	spin_unlock_irqrestore(&host->log_lock, flags);
}

static void __sd_store_buf_end(void *__data, struct mmc_host *mmc,
	struct mmc_request *mrq)
{
	unsigned long long t, tn;
	unsigned long long nanosec_rem;
	static int last_cmd, last_arg, skip;
	int l_skip = 0;
	unsigned long flags;
	struct msdc_host *host = NULL;

	if (!cmd_hist_enabled)
		return;

	if (!mmc)
		return;

	host = mmc_priv(mmc);
	t = cpu_clock(print_cpu_test);
	tn = t;
	nanosec_rem = do_div(t, 1000000000)/1000;

	/* skip log if last cmd rsp are the same */
	if (last_cmd == mrq->cmd->opcode &&
		last_arg == mrq->cmd->arg && mrq->cmd->opcode == 13) {
		skip++;
		spin_lock_irqsave(&host->log_lock, flags);
		if (dbg_sd_cnt == 0)
			dbg_sd_cnt = sd_dbg_max_cnt;
		/* remove type = 0, command */
		dbg_sd_cnt--;
		spin_unlock_irqrestore(&host->log_lock, flags);
		return;
	}
	last_cmd = mrq->cmd->opcode;
	last_arg = mrq->cmd->arg;
	l_skip = skip;
	skip = 0;

	spin_lock_irqsave(&host->log_lock, flags);

	dbg_run_sd_log_dat[dbg_sd_cnt].time_sec = t;
	dbg_run_sd_log_dat[dbg_sd_cnt].time_usec = nanosec_rem;
	dbg_run_sd_log_dat[dbg_sd_cnt].type = 1;
	dbg_run_sd_log_dat[dbg_sd_cnt].cmd = mrq->cmd->opcode;
	dbg_run_sd_log_dat[dbg_sd_cnt].arg = mrq->cmd->resp[0];
	dbg_run_sd_log_dat[dbg_sd_cnt].skip = l_skip;
	dbg_sd_cnt++;

	if (dbg_sd_cnt >= sd_dbg_max_cnt)
		dbg_sd_cnt = 0;

	spin_unlock_irqrestore(&host->log_lock, flags);
}

/* all cases which except softirq of IO */
static void record_mmc_send_command(void *__data,
	struct mmc_host *mmc, struct mmc_request *mrq)
{
	if (!(mmc->caps2 & MMC_CAP2_NO_MMC))
		__emmc_store_buf_start(__data, mmc, mrq);
	else if (!(mmc->caps2 & MMC_CAP2_NO_SD))
		__sd_store_buf_start(__data, mmc, mrq);
}

static void record_mmc_receive_command(void *__data,
	struct mmc_host *mmc, struct mmc_request *mrq)
{
	if (!(mmc->caps2 & MMC_CAP2_NO_MMC))
		__emmc_store_buf_end(__data, mmc, mrq);
	else if (!(mmc->caps2 & MMC_CAP2_NO_SD))
		__sd_store_buf_end(__data, mmc, mrq);
}

void mmc_cmd_dump(char **buff, unsigned long *size, struct seq_file *m,
	struct mmc_host *mmc, u32 latest_cnt)
{
	int i, j;
	unsigned long long time_sec, time_usec;
	int type, cmd, arg, skip;
	u32 dump_cnt;

	if (!mmc || !mmc->card)
		return;

	dump_cnt = min_t(u32, latest_cnt, dbg_max_cnt);

	i = dbg_host_cnt - 1;
	if (i < 0)
		i = dbg_max_cnt - 1;

	for (j = 0; j < dump_cnt; j++) {
		time_sec = dbg_run_host_log_dat[i].time_sec;
		time_usec = dbg_run_host_log_dat[i].time_usec;
		type = dbg_run_host_log_dat[i].type;
		cmd = dbg_run_host_log_dat[i].cmd;
		arg = dbg_run_host_log_dat[i].arg;
		skip = dbg_run_host_log_dat[i].skip;

		SPREAD_PRINTF(buff, size, m,
		"%03d [%5llu.%06llu]%2d %3d %08x (%d)\n",
			j, time_sec, time_usec,
			type, cmd, arg, skip);
		i--;
		if (i < 0)
			i = dbg_max_cnt - 1;
	}

	SPREAD_PRINTF(buff, size, m,
		"eMMC claimed(%d), claim_cnt(%d), claimer pid(%d), comm %s\n",
		mmc->claimed, mmc->claim_cnt,
		mmc->claimer && mmc->claimer->task ?
			mmc->claimer->task->pid : 0,
		mmc->claimer && mmc->claimer->task ?
			mmc->claimer->task->comm : "NULL");
}

void sd_cmd_dump(char **buff, unsigned long *size, struct seq_file *m,
	struct mmc_host *mmc, u32 latest_cnt)
{
	int i, j;
	unsigned long long time_sec, time_usec;
	int type, cmd, arg, skip;
	u32 dump_cnt;

	if (!mmc)
		return;

	dump_cnt = min_t(u32, latest_cnt, sd_dbg_max_cnt);

	i = dbg_sd_cnt - 1;
	if (i < 0)
		i = sd_dbg_max_cnt - 1;

	for (j = 0; j < dump_cnt; j++) {
		time_sec = dbg_run_sd_log_dat[i].time_sec;
		time_usec = dbg_run_sd_log_dat[i].time_usec;
		type = dbg_run_sd_log_dat[i].type;
		cmd = dbg_run_sd_log_dat[i].cmd;
		arg = dbg_run_sd_log_dat[i].arg;
		skip = dbg_run_sd_log_dat[i].skip;

		SPREAD_PRINTF(buff, size, m,
		"%03d [%5llu.%06llu]%2d %3d %08x (%d)\n",
			j, time_sec, time_usec,
			type, cmd, arg, skip);
		i--;
		if (i < 0)
			i = sd_dbg_max_cnt - 1;
	}

	SPREAD_PRINTF(buff, size, m,
		"SD claimed(%d), claim_cnt(%d), claimer pid(%d), comm %s\n",
		mmc->claimed, mmc->claim_cnt,
		mmc->claimer && mmc->claimer->task ? mmc->claimer->task->pid : 0,
		mmc->claimer && mmc->claimer->task ? mmc->claimer->task->comm : "NULL");
}


void msdc_dump_host_state(char **buff, unsigned long *size,
	struct seq_file *m)
{
	/* add log description*/
	SPREAD_PRINTF(buff, size, m,
		"column 1   : log number(Reverse order);\n");
	SPREAD_PRINTF(buff, size, m,
		"column 2   : kernel time\n");
	SPREAD_PRINTF(buff, size, m,
		"column 3   : type(0-cmd, 1-resp, 5-cqhci cmd, 60-cqhci dcmd doorbell,");
	SPREAD_PRINTF(buff, size, m,
		"61-cqhci dcmd complete(irq in));\n");
	SPREAD_PRINTF(buff, size, m,
		"column 4&5 : cmd index&arg(1XX-task XX's task descriptor low 32bit, ");
	SPREAD_PRINTF(buff, size, m,
		"2XX-task XX's task descriptor high 32bit, ");
	SPREAD_PRINTF(buff, size, m,
		"5XX-task XX's task completion(irq in), ");
	SPREAD_PRINTF(buff, size, m,
		"others index-command index) ");
	SPREAD_PRINTF(buff, size, m,
		"others arg-command arg);\n");
}

static int cmd_hist_on(void)
{
	unsigned long flags;

	spin_lock_irqsave(&cmd_hist_lock, flags);
	cmd_hist_enabled = true;
	spin_unlock_irqrestore(&cmd_hist_lock, flags);

	return 0;
}

static int cmd_hist_off(void)
{
	unsigned long flags;

	spin_lock_irqsave(&cmd_hist_lock, flags);
	cmd_hist_enabled = false;
	spin_unlock_irqrestore(&cmd_hist_lock, flags);

	return 0;
}

static struct tracepoints_table interests[] = {
	{.name = "mmc_request_start", .func = record_mmc_send_command},
	{.name = "mmc_request_done", .func = record_mmc_receive_command},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(interests) / sizeof(struct tracepoints_table); \
	i++)

/**
 * Find the struct tracepoint* associated with a given tracepoint
 * name.
 */
static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(interests[i].name, tp->name) == 0)
			interests[i].tp = tp;
	}
}

#ifndef USER_BUILD_KERNEL
#define PROC_PERM		0660
#else
#define PROC_PERM		0440
#endif

static ssize_t mmc_debug_proc_write(struct file *file, const char *buf,
				 size_t count, loff_t *data)
{
	unsigned long op = MMCDBG_UNKNOWN;
	char cmd_buf[16];

	if (count == 0 || count > 15)
		return -EINVAL;

	if (copy_from_user(cmd_buf, buf, count))
		return -EINVAL;

	cmd_buf[count] = '\0';
	if (kstrtoul(cmd_buf, 15, &op))
		return -EINVAL;

	if (op == MMCDBG_CMD_LIST_ENABLE) {
		cmd_hist_on();
		pr_info("mmc_mtk_dbg: cmd history on\n");
	} else if (op == MMCDBG_CMD_LIST_DISABLE) {
		cmd_hist_off();
		pr_info("mmc_mtk_dbg: cmd history off\n");
	} else
		return -EINVAL;

	return count;
}

static int mmc_debug_proc_show(struct seq_file *m, void *v)
{
	msdc_dump_host_state(NULL, NULL, m);
	mmc_cmd_dump(NULL, NULL, m, mtk_mmc_host[0], dbg_max_cnt);
	sd_cmd_dump(NULL, NULL, m, mtk_mmc_host[1], sd_dbg_max_cnt);

	return 0;
}

static int mmc_debug_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_debug_proc_show, inode->i_private);
}

static const struct proc_ops mmc_debug_proc_fops = {
	.proc_open = mmc_debug_proc_open,
	.proc_write = mmc_debug_proc_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

int mmc_debug_init_procfs(void)
{
	struct proc_dir_entry *prEntry;
	kuid_t uid;
	kgid_t gid;

	uid = make_kuid(&init_user_ns, 0);
	gid = make_kgid(&init_user_ns, 1001);

	/* Create "mmc_debug" node */
	prEntry = proc_create("mmc_debug", PROC_PERM, NULL,
			      &mmc_debug_proc_fops);

	if (!prEntry)
		return -ENOENT;

	proc_set_user(prEntry, uid, gid);

	return 0;
}

static void cmd_hist_cleanup(void)
{
	memset(dbg_run_host_log_dat, 0, sizeof(dbg_run_host_log_dat));
}

static void mmc_dbg_cleanup(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
						    interests[i].func,
						    NULL);
		}
	}

	cmd_hist_cleanup();
}

int mmc_dbg_register(struct mmc_host *mmc)
{
	int i, ret;
	bool is_sd;

	if (!(mmc->caps2 & MMC_CAP2_NO_MMC)) {
		mtk_mmc_host[0] = mmc;
		is_sd = false;
	} else if (!(mmc->caps2 & MMC_CAP2_NO_SD)) {
		mtk_mmc_host[1] = mmc;
		is_sd = true;
	} else /* SDIO no debug */
		return -EINVAL;

	/* avoid init repeatedly */
	if (cmd_hist_init == true)
		return 0;

	/*
	 * Ignore any failure of AEE buffer allocation to still allow
	 * command history dump in procfs.
	 */
	mmc_aee_buffer = kzalloc(MMC_AEE_BUFFER_SIZE, GFP_NOFS);

	/* Blocktag */
	ret = mmc_mtk_biolog_init(mmc);
	if (ret)
		return ret;

	spin_lock_init(&cmd_hist_lock);

	/* Install the tracepoints */
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (interests[i].tp == NULL) {
			pr_info("Error: %s not found\n",
				interests[i].name);
			/* Unload previously loaded */
			mmc_dbg_cleanup();
			return -EINVAL;
		}

		tracepoint_probe_register(interests[i].tp,
					  interests[i].func,
					  NULL);
		interests[i].init = true;
	}

	/* Create control nodes in procfs */
	ret = mmc_debug_init_procfs();

	cmd_hist_init = true;
	cmd_hist_enabled = true;

	return ret;
}
EXPORT_SYMBOL_GPL(mmc_dbg_register);

void mmc_mtk_dbg_get_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	unsigned long free_size = MMC_AEE_BUFFER_SIZE;
	char *buff;

	if (!mmc_aee_buffer) {
		pr_info("failed to dump MMC: null AEE buffer");
		return;
	}

	buff = mmc_aee_buffer;
	msdc_dump_host_state(&buff, &free_size, NULL);
	mmc_cmd_dump(&buff, &free_size, NULL, mtk_mmc_host[0], dbg_max_cnt);
	sd_cmd_dump(&buff, &free_size, NULL, mtk_mmc_host[1], sd_dbg_max_cnt);

	/* return start location */
	*vaddr = (unsigned long)mmc_aee_buffer;
	*size = MMC_AEE_BUFFER_SIZE - free_size;
}
EXPORT_SYMBOL_GPL(mmc_mtk_dbg_get_aee_buffer);

static int __init mmc_mtk_dbg_init(void)
{
	dbg_host_cnt = 0;
	dbg_sd_cnt = 0;
	cmd_hist_init = false;
	cmd_hist_enabled = false;

	mrdump_set_extra_dump(AEE_EXTRA_FILE_MMC, mmc_mtk_dbg_get_aee_buffer);
	return 0;
}

static void __exit mmc_mtk_dbg_exit(void)
{
	mrdump_set_extra_dump(AEE_EXTRA_FILE_MMC, NULL);
	return;
}

module_init(mmc_mtk_dbg_init);
module_exit(mmc_mtk_dbg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek SD/MMC Debug Driver");
