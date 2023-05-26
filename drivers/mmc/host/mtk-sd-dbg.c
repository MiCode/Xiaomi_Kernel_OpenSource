// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <mt-plat/mtk_boot_common.h>
#include <linux/io.h>
#include <linux/scatterlist.h>
#include <linux/gpio.h>
#include <linux/sched/clock.h>
#include <linux/mmc/mmc.h>
#include "mtk-msdc.h"
#include <../core/core.h>
#include <../core/card.h>
#include <../core/mmc_ops.h>
#include "mtk-sd-dbg.h"

struct msdc_host *mtk_msdc_host[] = { NULL, NULL, NULL};
static int enable_msdc_debug = 1;

static unsigned char cmd_buf[256];
#define dbg_max_cnt (40000)
#define MSDC_AEE_BUFFER_SIZE (300 * 1024)
struct dbg_run_host_log {
	unsigned long long time_sec;
	unsigned long long time_usec;
	int type;
	int cmd;
	int arg;
	int cpu;
	unsigned long active_reqs;
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

static struct dbg_run_host_log dbg_run_host_log_dat[dbg_max_cnt];

static struct dbg_dma_cmd_log dbg_dma_cmd_log_dat;
static struct dbg_task_log dbg_task_log_dat[32];
char msdc_aee_buffer[MSDC_AEE_BUFFER_SIZE];
static int dbg_host_cnt;

static unsigned int print_cpu_test = UINT_MAX;

void msdc_debug_set_host(struct mmc_host *mmc)
{
	if (!mmc){
		pr_info("mmc is null\n");
		return;
	}
	if(mmc->index >=0 && mmc->index <= HOST_MAX_NUM)
		mtk_msdc_host[mmc->index] = mmc_priv(mmc);
	else
		pr_info("error:index=%d\n",mmc->index);
}
EXPORT_SYMBOL_GPL(msdc_debug_set_host);

/*
 * type 0: cmd; type 1: rsp; type 3: dma end
 * when type 3: arg 0: no data crc error; arg 1: data crc error
 * @cpu, current CPU ID
 * @reserved, userd for softirq dump "data_active_reqs"
 */
inline void __dbg_add_host_log(struct mmc_host *mmc, int type,
			int cmd, int arg, int cpu, unsigned long reserved)
{
	unsigned long long t, tn;
	unsigned long long nanosec_rem;
#ifdef CONFIG_MTK_EMMC_HW_CQ
	unsigned long flags;
#endif
	static int last_cmd, last_arg, skip;
	int l_skip = 0;
	struct msdc_host *host = mmc_priv(mmc);
	static int tag = -1;

	/* only log msdc0 */
	if (!host || mmc->index != 0)
		return;

	t = cpu_clock(print_cpu_test);
#ifdef CONFIG_MTK_EMMC_HW_CQ
	spin_lock_irqsave(&host->cmd_dump_lock, flags);
#endif

	switch (type) {
	case 0: /* normal - cmd */
		tn = t;
		nanosec_rem = do_div(t, 1000000000)/1000;
		if (cmd == 44) {
			tag = (arg >> 16) & 0x1f;
			dbg_task_log_dat[tag].size = arg & 0xffff;
		} else if (cmd == 45) {
			dbg_task_log_dat[tag].address = arg;
		} else if (cmd == 46 || cmd == 47) {
			dbg_dma_cmd_log_dat.time = tn;
			dbg_dma_cmd_log_dat.cmd = cmd;
			dbg_dma_cmd_log_dat.arg = arg;
		}

		dbg_run_host_log_dat[dbg_host_cnt].time_sec = t;
		dbg_run_host_log_dat[dbg_host_cnt].time_usec = nanosec_rem;
		dbg_run_host_log_dat[dbg_host_cnt].type = type;
		dbg_run_host_log_dat[dbg_host_cnt].cmd = cmd;
		dbg_run_host_log_dat[dbg_host_cnt].arg = arg;
		dbg_run_host_log_dat[dbg_host_cnt].skip = l_skip;
		dbg_host_cnt++;
		if (dbg_host_cnt >= dbg_max_cnt)
			dbg_host_cnt = 0;
		break;
	case 1: /* normal -rsp */
	case 5: /* cqhci - data */
	case 6: /* cqhci - rsp */
	case 60: /* cqhci - dcmd */
	case 61: /* cqhci - dcmd resp */
		nanosec_rem = do_div(t, 1000000000)/1000;
		/*skip log if last cmd rsp are the same*/
		if (last_cmd == cmd &&
			last_arg == arg && cmd == 13) {
			skip++;
			if (dbg_host_cnt == 0)
				dbg_host_cnt = dbg_max_cnt;
			/*remove type = 0, command*/
			dbg_host_cnt--;
			break;
		}
		last_cmd = cmd;
		last_arg = arg;
		l_skip = skip;
		skip = 0;

		dbg_run_host_log_dat[dbg_host_cnt].time_sec = t;
		dbg_run_host_log_dat[dbg_host_cnt].time_usec = nanosec_rem;
		dbg_run_host_log_dat[dbg_host_cnt].type = type;
		dbg_run_host_log_dat[dbg_host_cnt].cmd = cmd;
		dbg_run_host_log_dat[dbg_host_cnt].arg = arg;
		dbg_run_host_log_dat[dbg_host_cnt].skip = l_skip;
		dbg_host_cnt++;
		if (dbg_host_cnt >= dbg_max_cnt)
			dbg_host_cnt = 0;
		break;
	/* add softirq record */
	case MAGIC_CQHCI_DBG_TYPE_SIRQ:
		tn = t;
		nanosec_rem = do_div(t, 1000000000)/1000;

		dbg_run_host_log_dat[dbg_host_cnt].time_sec = t;
		dbg_run_host_log_dat[dbg_host_cnt].time_usec = nanosec_rem;
		dbg_run_host_log_dat[dbg_host_cnt].type = type;
		dbg_run_host_log_dat[dbg_host_cnt].cmd = cmd;
		dbg_run_host_log_dat[dbg_host_cnt].arg = arg;
		dbg_run_host_log_dat[dbg_host_cnt].skip = l_skip;
		dbg_run_host_log_dat[dbg_host_cnt].cpu = cpu;
		dbg_run_host_log_dat[dbg_host_cnt].active_reqs = reserved;

		dbg_host_cnt++;
		if (dbg_host_cnt >= dbg_max_cnt)
			dbg_host_cnt = 0;
		break;
	default:
		break;
	}
#ifdef CONFIG_MTK_EMMC_HW_CQ
	spin_unlock_irqrestore(&host->cmd_dump_lock, flags);
#endif
}

/* all cases which except softirq of IO */
void dbg_add_host_log(struct mmc_host *mmc, int type,
		int cmd, int arg)
{
	if(enable_msdc_debug == 1)
		__dbg_add_host_log(mmc, type, cmd, arg, -1, 0);
}


void mmc_cmd_dump(char **buff, unsigned long *size, struct seq_file *m,
	struct mmc_host *mmc, u32 latest_cnt)
{
	int i, j;
	int tag = -1;
	int is_read, is_rel, is_fprg;
	unsigned long long time_sec, time_usec;
	int type, cmd, arg, skip, cnt, cpu;
	unsigned long active_reqs;
	struct msdc_host *host;
	u32 dump_cnt;
#ifdef CONFIG_MTK_EMMC_HW_CQ
	unsigned long curr_state;
#endif


	if (!mmc || !mmc->card)
		return;
	/* only dump msdc0 */
	host = mmc_priv(mmc);
	if (!host || mmc->index != 0)
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
		if (dbg_run_host_log_dat[i].type == 70) {
			cpu = dbg_run_host_log_dat[i].cpu;
			active_reqs = dbg_run_host_log_dat[i].active_reqs;
		} else {
			cpu = -1;
			active_reqs = 0;
		}
		if (cmd == 44 && !type) {
			cnt = arg & 0xffff;
			tag = (arg >> 16) & 0x1f;
			is_read = (arg >> 30) & 0x1;
			is_rel = (arg >> 31) & 0x1;
			is_fprg = (arg >> 24) & 0x1;
			SPREAD_PRINTF(buff, size, m,
		"%03d [%5llu.%06llu]%2d %3d %08x id=%02d %s cnt=%d %d %d\n",
				j, time_sec, time_usec,
				type, cmd, arg, tag,
				is_read ? "R" : "W",
				cnt, is_rel, is_fprg);
		} else if ((cmd == 46 || cmd == 47) && !type) {
			tag = (arg >> 16) & 0x1f;
			SPREAD_PRINTF(buff, size, m,
				"%03d [%5llu.%06llu]%2d %3d %08x id=%02d\n",
				j, time_sec, time_usec,
				type, cmd, arg, tag);
		} else
			SPREAD_PRINTF(buff, size, m,
			"%03d [%5llu.%06llu]%2d %3d %08x(%d) (%d) (0x%08lx) (%d)\n",
				j, time_sec, time_usec,
				type, cmd, arg, arg, skip, active_reqs, cpu);
		i--;
		if (i < 0)
			i = dbg_max_cnt - 1;
	}
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	SPREAD_PRINTF(buff, size, m,
		"areq_cnt:%d, task_id_index %08lx, cq_wait_rdy:%d, cq_rdy_cnt:%d\n",
		atomic_read(&mmc->areq_cnt),
		mmc->task_id_index,
		atomic_read(&mmc->cq_wait_rdy),
		atomic_read(&mmc->cq_rdy_cnt));
#endif
#ifdef CONFIG_MTK_EMMC_HW_CQ
	curr_state = mmc->cmdq_ctx.curr_state;

	SPREAD_PRINTF(buff, size, m,
		"active_reqs : 0x%lx\n",
		mmc->cmdq_ctx.active_reqs);
	SPREAD_PRINTF(buff, size, m,
		"curr_state  : 0x%lx\n",
		curr_state);
	SPREAD_PRINTF(buff, size, m,
		"%s %s %s %s %s\n",
		curr_state & (1 << CMDQ_STATE_ERR) ?
			"ERR":"",
		curr_state & (1 << CMDQ_STATE_DCMD_ACTIVE) ?
			"DCMD_ACTIVE":"",
		curr_state & (1 << CMDQ_STATE_HALT) ?
			"HALT":"",
		curr_state & (1 << CMDQ_STATE_CQ_DISABLE) ?
			"CQ_DISABLE":"",
		curr_state & (1 << CMDQ_STATE_REQ_TIMED_OUT) ?
			"REQ_TIMED_OUT":"");
	SPREAD_PRINTF(buff, size, m,
		"part_curr  : %d\n",
		mmc->card->part_curr);
#endif
	SPREAD_PRINTF(buff, size, m,
		"claimed(%d), claim_cnt(%d), claimer pid(%d), comm %s\n",
		mmc->claimed, mmc->claim_cnt,
		mmc->claimer && mmc->claimer->task ?
			mmc->claimer->task->pid : 0,
		mmc->claimer && mmc->claimer->task ?
			mmc->claimer->task->comm : "NULL");
}

void msdc_dump_host_state(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	void __iomem *base = host->base;

	SPREAD_PRINTF(buff, size, m,
		"enable_msdc_debug : %d\n", enable_msdc_debug);
	/* add log description*/
	SPREAD_PRINTF(buff, size, m,
		"column 1   : log number(Reverse order);\n");
	SPREAD_PRINTF(buff, size, m,
		"column 2   : kernel time\n");
	SPREAD_PRINTF(buff, size, m,
		"column 3   : type(0-cmd, 1-resp, 5-cqhci cmd, 60-cqhci dcmd doorbell,");
	SPREAD_PRINTF(buff, size, m,
		"61-cqhci dcmd complete(irq in), 70-cqhci softirq in);\n");
	SPREAD_PRINTF(buff, size, m,
		"column 4&5 : cmd index&arg(1XX-task XX's task descriptor low 32bit, ");
	SPREAD_PRINTF(buff, size, m,
		"2XX-task XX's task descriptor high 32bit, ");
	SPREAD_PRINTF(buff, size, m,
		"5XX-task XX's task completion(irq in), ");
	SPREAD_PRINTF(buff, size, m,
		"others index-command index(non 70 type) or cmd/data error(70 type)) ");
	SPREAD_PRINTF(buff, size, m,
		"others arg-command arg(non 70 type) or cmdq_req->tag(70 type));\n");
	SPREAD_PRINTF(buff, size, m,
		"column 6   : repeat count(The role of problem analysis is low);\n");
	SPREAD_PRINTF(buff, size, m,
		"column 7   : record data_active_reqs;\n");
	SPREAD_PRINTF(buff, size, m,
		"column 8   : only record softirq's running CPU id(only for 70 type);\n");
}

static void msdc_proc_dump(struct seq_file *m, u32 id)
{
	struct msdc_host *host = mtk_msdc_host[id];

	if (host == NULL) {
		pr_info("====== Null msdc%d, dump skipped ======\n", id);
		return;
	}

	msdc_dump_host_state(NULL, NULL, m, host);
	mmc_cmd_dump(NULL, NULL, m, host->mmc, dbg_max_cnt);
}

void get_msdc_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	struct msdc_host *host = mtk_msdc_host[0];
	unsigned long free_size = MSDC_AEE_BUFFER_SIZE;
	char *buff;

	if (host == NULL) {
		pr_info("====== Null msdc, dump skipped ======\n");
		return;
	}

	buff = msdc_aee_buffer;
	msdc_dump_host_state(&buff, &free_size, NULL, host);
	mmc_cmd_dump(&buff, &free_size, NULL, host->mmc, dbg_max_cnt);
	/* retrun start location */
	*vaddr = (unsigned long)msdc_aee_buffer;
	*size = MSDC_AEE_BUFFER_SIZE - free_size;
}
EXPORT_SYMBOL(get_msdc_aee_buffer);

int g_count;
/* ========== driver proc interface =========== */
static int msdc_debug_proc_show(struct seq_file *m, void *v)
{
	int cmd = -1;
	int sscanf_num;
	int p1, p2, p3, p4, p5, p6, p7, p8;
	int id, zone;
	int mode;
	int thread_num, compare_count, multi_address;
	void __iomem *base = NULL;
	ulong data_for_wr;
	unsigned int offset = 0;
	unsigned int reg_value;
	int spd_mode = MMC_TIMING_LEGACY;
	struct msdc_host *host = NULL;
#ifdef MSDC_DMA_ADDR_DEBUG
	struct dma_addr *dma_address, *p_dma_address;
#endif
	int dma_status;
#ifdef MTK_MMC_SDIO_DEBUG
	u8 *res;
	int vcore;
#endif

		/* default dump info for aee */
		seq_puts(m, "==== msdc debug info for aee ====\n");
		msdc_proc_dump(m, 0);

	return 0;
}

static ssize_t msdc_debug_proc_write(struct file *file, const char *buf,
	size_t count, loff_t *data)
{
	int ret;

	if (count == 0)
		return -1;
	if (count > 255)
		count = 255;
	g_count = count;
	ret = copy_from_user(cmd_buf, buf, count);
	if (ret < 0)
		return -1;

	switch (cmd_buf[0]) {
	case 0x31:
		enable_msdc_debug = 1;
		break;
	default :
		enable_msdc_debug = 0;
		break;
	}
	pr_info("enable_msdc_debug=%d\n", enable_msdc_debug);

	return count;
}

static int msdc_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, msdc_debug_proc_show, inode->i_private);
}

static const struct file_operations msdc_proc_fops = {
	.open = msdc_proc_open,
	.write = msdc_debug_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


#ifndef USER_BUILD_KERNEL
#define PROC_PERM		0660
#else
#define PROC_PERM		0440
#endif
int msdc_debug_proc_init(void)
{
	kuid_t uid;
	kgid_t gid;
	struct proc_dir_entry *prEntry;

#ifdef USER_BUILD_KERNEL
	enable_msdc_debug = 0;
#else
	enable_msdc_debug = 1;
#endif

	uid = make_kuid(&init_user_ns, 0);
	gid = make_kgid(&init_user_ns, 1001);

	prEntry = proc_create("msdc_debug", PROC_PERM, NULL, &msdc_proc_fops);

	if (prEntry)
		proc_set_user(prEntry, uid, gid);
	else
		pr_info("[%s]: failed to create /proc/msdc_debug\n",
			__func__);

	return 0;
}
EXPORT_SYMBOL_GPL(msdc_debug_proc_init);

