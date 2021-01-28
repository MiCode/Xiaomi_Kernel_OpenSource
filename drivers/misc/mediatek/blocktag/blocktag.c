// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define DEBUG 1

#include <linux/debugfs.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/cpumask.h>
#include <linux/sched/cputime.h>
#include <linux/tick.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/vmalloc.h>
#include <linux/memblock.h>
#include <linux/blk_types.h>
#include <linux/module.h>
#include <linux/vmstat.h>
#include <linux/types.h>

#define BLOCKIO_MIN_VER	"3.09"

#ifdef CONFIG_MTK_USE_RESERVED_EXT_MEM
#include <linux/exm_driver.h>
#endif

#include <mt-plat/mtk_blocktag.h>

/*
 * snprintf may return a value of size or "more" to indicate
 * that the output was truncated, thus be careful of "more"
 * case.
 */
#define SPREAD_PRINTF(buff, size, evt, fmt, args...) \
do { \
	if (buff && size && *(size)) { \
		unsigned long var = snprintf(*(buff), *(size), fmt, ##args); \
		if (var > 0) { \
			if (var > *(size)) \
				var = *(size); \
			*(size) -= var; \
			*(buff) += var; \
		} \
	} \
	if (evt) \
		seq_printf(evt, fmt, ##args); \
	if (!buff && !evt) { \
		pr_info(fmt, ##args); \
	} \
} while (0)

#define mtk_btag_pidlog_index(p) \
	((unsigned long)(__page_to_pfn(p)) - \
	(memblock_start_of_DRAM() >> PAGE_SHIFT))

#define mtk_btag_pidlog_max_entry() \
	(mtk_btag_system_dram_size >> PAGE_SHIFT)

#define mtk_btag_pidlog_entry(idx) \
	(((struct page_pid_logger *)mtk_btag_pagelogger) + idx)

/* max dump size is 300KB whitch can be adjusted */
#define BLOCKIO_AEE_BUFFER_SIZE (300 * 1024)
char blockio_aee_buffer[BLOCKIO_AEE_BUFFER_SIZE];

/* procfs dentries */
struct proc_dir_entry *btag_proc_root;

static int mtk_btag_init_procfs(void);

/* mini context for major embedded storage only */
#define MICTX_PROC_CMD_BUF_SIZE (1)
static struct mtk_btag_mictx_struct *mtk_btag_mictx;
static bool mtk_btag_mictx_ready;
static bool mtk_btag_mictx_debug;

/* blocktag */
static DEFINE_MUTEX(mtk_btag_list_lock);
static LIST_HEAD(mtk_btag_list);

static struct mtk_blocktag *mtk_btag_find(const char *name)
{
	struct mtk_blocktag *btag, *n;

	list_for_each_entry_safe(btag, n, &mtk_btag_list, list) {
		if (!strncmp(btag->name, name, BLOCKTAG_NAME_LEN-1))
			return btag;
	}
	return NULL;
}

static struct mtk_blocktag *mtk_btag_find_locked(const char *name)
{
	struct mtk_blocktag *btag;

	mutex_lock(&mtk_btag_list_lock);
	btag = mtk_btag_find(name);
	mutex_unlock(&mtk_btag_list_lock);
	return btag;
}

/* pid logger: page loger*/
unsigned long long mtk_btag_system_dram_size;
struct page_pid_logger *mtk_btag_pagelogger;

static size_t mtk_btag_seq_pidlog_usedmem(char **buff, unsigned long *size,
	struct seq_file *seq)
{
	size_t size_l = 0;

	if (!IS_ERR_OR_NULL(mtk_btag_pagelogger)) {
		size_l = (sizeof(struct page_pid_logger)
			* (mtk_btag_system_dram_size >> PAGE_SHIFT));
		SPREAD_PRINTF(buff, size, seq,
		"page pid logger buffer: %llu entries * %zu = %zu bytes\n",
			(mtk_btag_system_dram_size >> PAGE_SHIFT),
			sizeof(struct page_pid_logger),
			size_l);
	}
	return size_l;
}

#define biolog_fmt "wl:%d%%,%lld,%lld,%d.vm:%lld,%lld,%lld,%lld,%lld,%lld." \
	"cpu:%llu,%llu,%llu,%llu,%llu,%llu,%llu.pid:%d,"
#define biolog_fmt_wt "wt:%d,%d,%lld."
#define biolog_fmt_rt "rt:%d,%d,%lld."
#define pidlog_fmt "{%05d:%05d:%08d:%05d:%08d}"

void mtk_btag_pidlog_insert(struct mtk_btag_pidlogger *pidlog, pid_t pid,
	__u32 len, int write)
{
	int i;
	struct mtk_btag_pidlogger_entry *pe;
	struct mtk_btag_pidlogger_entry_rw *prw;

	for (i = 0; i < BLOCKTAG_PIDLOG_ENTRIES; i++) {
		pe = &pidlog->info[i];
		if ((pe->pid == pid) || (pe->pid == 0)) {
			pe->pid = pid;
			prw = (write) ? &pe->w : &pe->r;
			prw->count++;
			prw->length += len;
			break;
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_btag_pidlog_insert);

void mtk_mq_btag_pidlog_insert(struct mtk_btag_pidlogger *pidlog, pid_t pid,
	__u32 len, int write, bool ext_sd)
{
	int i;
	struct mtk_btag_pidlogger_entry *pe;
	struct mtk_btag_pidlogger_entry_rw *prw;

	for (i = 0; i < BLOCKTAG_PIDLOG_ENTRIES; i++) {
		pe = &pidlog->info[i];
		if (!strncmp(pe->comm, "kworker", strlen("kworker")) ||
			(pe->pid == pid) || (pe->pid == 0)) {
			pe->pid = pid;
			BUILD_BUG_ON(sizeof(pe->comm) != TASK_COMM_LEN);
			strncpy(pe->comm, current->comm, sizeof(pe->comm));
			prw = (write) ? &pe->w : &pe->r;
			prw->count++;
			prw->length += len;
			break;
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_mq_btag_pidlog_insert);

static void mtk_btag_pidlog_add(struct request_queue *q, struct bio *bio,
	unsigned short pid, __u32 len)
{
	int write = bio_data_dir(bio);
	int major = bio->bi_disk ? MAJOR(bio_dev(bio)) : 0;

	if (pid != 0xFFFF && major) {
#ifdef CONFIG_MTK_UFS_BLOCK_IO_LOG
		if (major == SCSI_DISK0_MAJOR || major == BLOCK_EXT_MAJOR) {
			mtk_btag_pidlog_add_ufs(q, pid, len, write);
			return;
		}
#endif
#ifdef CONFIG_MMC_BLOCK_IO_LOG
		if (major == MMC_BLOCK_MAJOR) {
			mtk_btag_pidlog_add_mmc(q, pid, len, write);
			return;
		}
#endif
	}
}

/*
 * pidlog: hook function for __blk_bios_map_sg()
 * rw: 0=read, 1=write
 */
void mtk_btag_pidlog_map_sg(struct request_queue *q, struct bio *bio,
	struct bio_vec *bvec)
{
	struct page_pid_logger *ppl, tmp;
	unsigned long idx;

	if (!mtk_btag_pagelogger || !bio || !bvec)
		return;

	idx = mtk_btag_pidlog_index(bvec->bv_page);
	ppl = mtk_btag_pidlog_entry(idx);

	tmp.pid = ppl->pid;
	ppl->pid = 0xFFFF;

	mtk_btag_pidlog_add(q, bio, tmp.pid, bvec->bv_len);
}
EXPORT_SYMBOL_GPL(mtk_btag_pidlog_map_sg);

static void _mtk_btag_pidlog_set_pid(struct page *p, int mode)
{
	struct page_pid_logger *ppl;
	unsigned long idx;

	idx = mtk_btag_pidlog_index(p);
	ppl = mtk_btag_pidlog_entry(idx);

	if (idx >= mtk_btag_pidlog_max_entry())
		return;

	/* we do lockless operation here to favor performance */

	if (mode == PIDLOG_MODE_BLK_SUBMIT_BIO) {
		/*
		 * do not overwrite the real owner set by
		 * mm or file system layer
		 */
		if (ppl->pid == 0xFFFF)
			ppl->pid = current->pid;
	} else {
		/* the latest owner will be counted */
		ppl->pid = current->pid;
	}
}

void mtk_btag_pidlog_copy_pid(struct page *src, struct page *dst)
{
	struct page_pid_logger *ppl_src, *ppl_dst;
	unsigned long idx_src, idx_dst;

	idx_src = mtk_btag_pidlog_index(src);

	if (idx_src >= mtk_btag_pidlog_max_entry())
		return;

	idx_dst = mtk_btag_pidlog_index(dst);

	if (idx_dst >= mtk_btag_pidlog_max_entry())
		return;

	ppl_src = mtk_btag_pidlog_entry(idx_src);
	ppl_dst = mtk_btag_pidlog_entry(idx_dst);
	ppl_dst->pid = ppl_src->pid;
}

/* pidlog: hook function for submit_bio() */
void mtk_btag_pidlog_submit_bio(struct bio *bio)
{
	struct bio_vec bvec;
	struct bvec_iter iter;

	if (!mtk_btag_pagelogger)
		return;

	bio_for_each_segment(bvec, bio, iter) {
		if (bvec.bv_page)
			_mtk_btag_pidlog_set_pid(bvec.bv_page,
				PIDLOG_MODE_BLK_SUBMIT_BIO);
	}
}
EXPORT_SYMBOL_GPL(mtk_btag_pidlog_submit_bio);

void mtk_btag_pidlog_set_pid(struct page *p)
{
	if (!mtk_btag_pagelogger || !p)
		return;

	_mtk_btag_pidlog_set_pid(p, PIDLOG_MODE_MM_FS);
}
EXPORT_SYMBOL_GPL(mtk_btag_pidlog_set_pid);

/* evaluate vmstat trace from global_node_page_state() */
void mtk_btag_vmstat_eval(struct mtk_btag_vmstat *vm)
{
	int cpu;
	struct vm_event_state *this;

	vm->file_pages = ((global_node_page_state(NR_FILE_PAGES))
		<< (PAGE_SHIFT - 10));
	vm->file_dirty = ((global_node_page_state(NR_FILE_DIRTY))
		<< (PAGE_SHIFT - 10));
	vm->dirtied = ((global_node_page_state(NR_DIRTIED))
		<< (PAGE_SHIFT - 10));
	vm->writeback = ((global_node_page_state(NR_WRITEBACK))
		<< (PAGE_SHIFT - 10));
	vm->written = ((global_node_page_state(NR_WRITTEN))
		<< (PAGE_SHIFT - 10));

	/* file map fault */
	vm->fmflt = 0;

	for_each_online_cpu(cpu) {
		this = &per_cpu(vm_event_states, cpu);
		vm->fmflt += this->event[PGMAJFAULT];
	}

}
EXPORT_SYMBOL_GPL(mtk_btag_vmstat_eval);

void mtk_btag_mictx_dump(void)
{
	struct mtk_btag_mictx_iostat_struct iostat;
	int ret;

	ret = mtk_btag_mictx_get_data(&iostat);

	if (ret) {
		pr_info("[BLOCK_TAG] Mictx: Get data failed %d\n", ret);
		return;
	}

	pr_info("[BLOCK_TAG] Mictx: %llu|%u|%u|%u|%u|%u|%u|%u|%u|%u|%u\n",
		iostat.duration, iostat.q_depth, iostat.wl,
		iostat.tp_req_r, iostat.tp_req_w,
		iostat.tp_all_r, iostat.tp_all_w,
		iostat.reqcnt_r, iostat.reqcnt_w,
		iostat.reqsize_r, iostat.reqsize_w);
}
/* evaluate pidlog trace from context */
void mtk_btag_pidlog_eval(struct mtk_btag_pidlogger *pl,
	struct mtk_btag_pidlogger *ctx_pl)
{
	int i;

	for (i = 0; i < BLOCKTAG_PIDLOG_ENTRIES; i++) {
		if (ctx_pl->info[i].pid == 0)
			break;
	}

	if (i != 0) {
		int size = i * sizeof(struct mtk_btag_pidlogger_entry);

		memcpy(&pl->info[0], &ctx_pl->info[0], size);
		memset(&ctx_pl->info[0], 0, size);
	}

	if (mtk_btag_mictx_debug)
		mtk_btag_mictx_dump();
}
EXPORT_SYMBOL_GPL(mtk_btag_pidlog_eval);

static __u64 mtk_btag_cpu_idle_time(int cpu)
{
	u64 idle, idle_usecs = -1ULL;

	if (cpu_online(cpu))
		idle_usecs = get_cpu_idle_time_us(cpu, NULL);

	if (idle_usecs == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	else
		idle = idle_usecs * NSEC_PER_USEC;

	return idle;
}

static __u64 mtk_btag_cpu_iowait_time(int cpu)
{
	__u64 iowait, iowait_usecs = -1ULL;

	if (cpu_online(cpu))
		iowait_usecs = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_usecs == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	else
		iowait = iowait_usecs * NSEC_PER_USEC;

	return iowait;
}

/* evaluate cpu trace from kcpustat_cpu() */
void mtk_btag_cpu_eval(struct mtk_btag_cpu *cpu)
{
	int i;
	__u64 user, nice, system, idle, iowait, irq, softirq;

	user = nice = system = idle = iowait = irq = softirq = 0;

	for_each_possible_cpu(i) {
		user += kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice += kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system += kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle += mtk_btag_cpu_idle_time(i);
		iowait += mtk_btag_cpu_iowait_time(i);
		irq += kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq += kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
	}

	cpu->user = nsec_to_clock_t(user);
	cpu->nice = nsec_to_clock_t(nice);
	cpu->system = nsec_to_clock_t(system);
	cpu->idle = nsec_to_clock_t(idle);
	cpu->iowait = nsec_to_clock_t(iowait);
	cpu->irq = nsec_to_clock_t(irq);
	cpu->softirq = nsec_to_clock_t(softirq);
}
EXPORT_SYMBOL_GPL(mtk_btag_cpu_eval);

static __u32 mtk_btag_eval_tp_speed(__u32 bytes, __u64 duration)
{
	__u32 speed_kbs = 0;

	if (!bytes || !duration)
		return 0;

	/* convert ns to ms */
	do_div(duration, 1000000);

	if (duration) {
		/* bytes/ms */
		speed_kbs = bytes / (__u32)duration;

		/* KB/s */
		speed_kbs = (speed_kbs * 1000) >> 10;
	}

	return speed_kbs;
}

static void mtk_btag_throughput_rw_eval(struct mtk_btag_throughput_rw *rw)
{
	__u64 usage;

	usage = rw->usage;

	do_div(usage, 1000000); /* convert ns to ms */

	if (usage && rw->size) {
		rw->speed = (rw->size) / (__u32)usage;  /* bytes/ms */
		rw->speed = (rw->speed*1000) >> 10; /* KB/s */
		rw->usage = usage;
	} else {
		rw->speed = 0;
		rw->size = 0;
		rw->usage = 0;
	}
}
/* calculate throughput */
void mtk_btag_throughput_eval(struct mtk_btag_throughput *tp)
{
	mtk_btag_throughput_rw_eval(&tp->r);
	mtk_btag_throughput_rw_eval(&tp->w);
}
EXPORT_SYMBOL_GPL(mtk_btag_throughput_eval);

/* print trace to kerne log */
static void mtk_btag_klog_entry(char **ptr, int *len, struct mtk_btag_trace *tr)
{
	int i, n;

#define boundary_check() { *len -= n; *ptr += n; }

	if (tr->throughput.r.usage) {
		n = snprintf(*ptr, *len, biolog_fmt_rt,
			tr->throughput.r.speed,
			tr->throughput.r.size,
			tr->throughput.r.usage);
		boundary_check();
		if (*len < 0)
			return;
	}

	if (tr->throughput.w.usage) {
		n = snprintf(*ptr, *len, biolog_fmt_wt,
			tr->throughput.w.speed,
			tr->throughput.w.size,
			tr->throughput.w.usage);
		boundary_check();
		if (*len < 0)
			return;
	}
	//if (!tr->throughput.w.usage || !tr->throughput.r.usage)
	n = snprintf(*ptr, *len, biolog_fmt,
		tr->workload.percent,
		tr->workload.usage,
		tr->workload.period,
		tr->workload.count,
		tr->vmstat.file_pages,
		tr->vmstat.file_dirty,
		tr->vmstat.dirtied,
		tr->vmstat.writeback,
		tr->vmstat.written,
		tr->vmstat.fmflt,
		tr->cpu.user,
		tr->cpu.nice,
		tr->cpu.system,
		tr->cpu.idle,
		tr->cpu.iowait,
		tr->cpu.irq,
		tr->cpu.softirq,
		tr->pid);
	boundary_check();
	if (*len < 0)
		return;

	for (i = 0; i < BLOCKTAG_PIDLOG_ENTRIES; i++) {
		struct mtk_btag_pidlogger_entry *pe;

		pe = &tr->pidlog.info[i];

		if (pe->pid == 0)
			break;

		n = snprintf(*ptr, *len, pidlog_fmt,
			pe->pid,
			pe->w.count,
			pe->w.length,
			pe->r.count,
			pe->r.length);
		boundary_check();
		if (*len < 0)
			return;
	}
}

void mtk_btag_klog(struct mtk_blocktag *btag, struct mtk_btag_trace *tr)
{
	int len;
	char *ptr;
	unsigned long flags;

	if (!btag || !btag->klog_enable || !tr)
		return;

	len = BLOCKTAG_PRINT_LEN-1;
	ptr = &btag->prbuf.buf[0];

	spin_lock_irqsave(&btag->prbuf.lock, flags);
	mtk_btag_klog_entry(&ptr, &len, tr);
	spin_unlock_irqrestore(&btag->prbuf.lock, flags);
}
EXPORT_SYMBOL_GPL(mtk_btag_klog);

static int mtk_btag_pr_time(char *out, int size, const char *str, __u64 t)
{
	uint32_t nsec;
	int ret;

	nsec = do_div(t, 1000000000);
	ret = snprintf(out, size, ",%s=[%lu.%06lu]", str, (unsigned long)t,
		(unsigned long)nsec/1000);
	return ret;
}

static const char *mtk_btag_pr_speed(char *out, int size, __u64 usage,
	__u32 bytes)
{
	__u32 speed;

	if (!usage || !bytes)
		return "";

	do_div(usage, 1000); /* convert ns to us */
	speed = 1000 * bytes / (__u32)usage;  /* bytes/ms */
	speed = (speed*1000) >> 10; /* KB/s */

	snprintf(out, size, ",%u KB/s", speed);
	return out;
}

void mtk_btag_task_timetag(char *buf, unsigned int len, unsigned int stage,
	unsigned int max, const char *name[], uint64_t *t, __u32 bytes)
{
	__u64 busy_time = 0;
	int i;
	int ret;

	if (!buf || !len)
		return;

	for (i = 0; i <= stage; i++) {
		ret = mtk_btag_pr_time(buf, len, name[i], t[i]);
		buf += ret;
		len -= ret;
	}

	if (stage == max-1) {
		busy_time = t[stage] - t[0];
		ret = mtk_btag_pr_time(buf, len, "busy", busy_time);
		buf += ret;
		len -= ret;
		mtk_btag_pr_speed(buf, len, busy_time, bytes);
	}
}
EXPORT_SYMBOL_GPL(mtk_btag_task_timetag);


void mtk_btag_seq_time(char **buff, unsigned long *size,
	struct seq_file *seq, uint64_t time)
{
	uint32_t nsec;

	nsec = do_div(time, 1000000000);
	SPREAD_PRINTF(buff, size, seq, "[%5lu.%06lu]", (unsigned long)time,
		(unsigned long)nsec/1000);
}
EXPORT_SYMBOL_GPL(mtk_btag_seq_time);

static void mtk_btag_seq_trace(char **buff, unsigned long *size,
	struct seq_file *seq, const char *name, struct mtk_btag_trace *tr)
{
	int i;

	if (tr->time <= 0)
		return;

	mtk_btag_seq_time(buff, size, seq, tr->time);
	SPREAD_PRINTF(buff, size, seq, "%s.q:%d.", name, tr->qid);

	if (tr->throughput.r.usage)
		SPREAD_PRINTF(buff, size, seq, biolog_fmt_rt,
			tr->throughput.r.speed,
			tr->throughput.r.size,
			tr->throughput.r.usage);
	if (tr->throughput.w.usage)
		SPREAD_PRINTF(buff, size, seq, biolog_fmt_wt,
			tr->throughput.w.speed,
			tr->throughput.w.size,
			tr->throughput.w.usage);

	SPREAD_PRINTF(buff, size, seq, biolog_fmt,
		tr->workload.percent,
		tr->workload.usage,
		tr->workload.period,
		tr->workload.count,
		tr->vmstat.file_pages,
		tr->vmstat.file_dirty,
		tr->vmstat.dirtied,
		tr->vmstat.writeback,
		tr->vmstat.written,
		tr->vmstat.fmflt,
		tr->cpu.user,
		tr->cpu.nice,
		tr->cpu.system,
		tr->cpu.idle,
		tr->cpu.iowait,
		tr->cpu.irq,
		tr->cpu.softirq,
		tr->pid);

	for (i = 0; i < BLOCKTAG_PIDLOG_ENTRIES; i++) {
		struct mtk_btag_pidlogger_entry *pe;

		pe = &tr->pidlog.info[i];

		if (pe->pid == 0)
			break;

		SPREAD_PRINTF(buff, size, seq, pidlog_fmt,
			pe->pid,
			pe->w.count,
			pe->w.length,
			pe->r.count,
			pe->r.length);
	}
	SPREAD_PRINTF(buff, size, seq, ".\n");
}

/* get current trace in debugfs ring buffer */
struct mtk_btag_trace *mtk_btag_curr_trace(struct mtk_btag_ringtrace *rt)
{
	if (rt)
		return &rt->trace[rt->index];
	else
		return NULL;
}
EXPORT_SYMBOL_GPL(mtk_btag_curr_trace);

/* step to next trace in debugfs ring buffer */
struct mtk_btag_trace *mtk_btag_next_trace(struct mtk_btag_ringtrace *rt)
{
	rt->index++;
	if (rt->index >= rt->max)
		rt->index = 0;

	return mtk_btag_curr_trace(rt);
}
EXPORT_SYMBOL_GPL(mtk_btag_next_trace);

/* clear debugfs ring buffer */
static void mtk_btag_clear_trace(struct mtk_btag_ringtrace *rt)
{
	unsigned long flags;

	spin_lock_irqsave(&rt->lock, flags);
	memset(rt->trace, 0, (sizeof(struct mtk_btag_trace) * rt->max));
	rt->index = 0;
	spin_unlock_irqrestore(&rt->lock, flags);
}

static void mtk_btag_seq_debug_show_ringtrace(char **buff, unsigned long *size,
	struct seq_file *seq, struct mtk_blocktag *btag)
{
	struct mtk_btag_ringtrace *rt = BTAG_RT(btag);
	unsigned long flags;
	int i, end;

	if (!rt)
		return;

	if (rt->index >= rt->max || rt->index < 0)
		rt->index = 0;

	SPREAD_PRINTF(buff, size, seq, "<%s: blocktag trace %s>\n",
		btag->name, BLOCKIO_MIN_VER);

	spin_lock_irqsave(&rt->lock, flags);
	end = (rt->index > 0) ? rt->index-1 : rt->max-1;
	for (i = rt->index;;) {
		mtk_btag_seq_trace(buff, size, seq, btag->name, &rt->trace[i]);
		if (i == end)
			break;
		i = (i >= rt->max-1) ? 0 : i+1;
	};
	spin_unlock_irqrestore(&rt->lock, flags);
}


static size_t mtk_btag_seq_sub_show_usedmem(char **buff, unsigned long *size,
	struct seq_file *seq, struct mtk_blocktag *btag)
{
	size_t used_mem = 0;
	size_t size_l;

	SPREAD_PRINTF(buff, size, seq, "<%s: memory usage>\n", btag->name);
	SPREAD_PRINTF(buff, size, seq, "%s blocktag: %zu bytes\n", btag->name,
		sizeof(struct mtk_blocktag));
	used_mem += sizeof(struct mtk_blocktag);

	if (BTAG_RT(btag)) {
		size_l = (sizeof(struct mtk_btag_trace) * BTAG_RT(btag)->max);
		SPREAD_PRINTF(buff, size, seq,
		"%s debug ring buffer: %d traces * %zu = %zu bytes\n",
			btag->name,
			BTAG_RT(btag)->max,
			sizeof(struct mtk_btag_trace),
			size_l);
		used_mem += size_l;
	}

	if (BTAG_CTX(btag)) {
		size_l = btag->ctx.size * btag->ctx.count;
		SPREAD_PRINTF(buff, size, seq,
			"%s queue context: %d contexts * %d = %zu bytes\n",
			btag->name,
			btag->ctx.count,
			btag->ctx.size,
			size_l);
		used_mem += size_l;
	}

	SPREAD_PRINTF(buff, size, seq, "%s aee buffer: %d bytes\n", btag->name,
			BLOCKIO_AEE_BUFFER_SIZE);
	used_mem += BLOCKIO_AEE_BUFFER_SIZE;

	SPREAD_PRINTF(buff, size, seq,
		"%s sub-total: %zu KB\n", btag->name, used_mem >> 10);
	return used_mem;
}

/* clear all ringtraces */
static ssize_t mtk_btag_main_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	struct mtk_blocktag *btag, *n;

	mutex_lock(&mtk_btag_list_lock);
	list_for_each_entry_safe(btag, n, &mtk_btag_list, list)
		mtk_btag_clear_trace(&btag->rt);
	mutex_unlock(&mtk_btag_list_lock);
	return count;
}

/* clear ringtrace */
static ssize_t mtk_btag_sub_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	struct seq_file *seq = file->private_data;
	struct mtk_blocktag *btag;

	if (seq && seq->private) {
		btag = seq->private;
		mtk_btag_clear_trace(&btag->rt);
	}
	return count;
}

/* seq file operations */
static void *mtk_btag_seq_debug_start(struct seq_file *seq, loff_t *pos)
{
	unsigned int idx;

	if (*pos < 0 || *pos >= 1)
		return NULL;

	idx = *pos + 1;
	return (void *) ((unsigned long) idx);
}

static void *mtk_btag_seq_debug_next(struct seq_file *seq, void *v, loff_t *pos)
{
	unsigned int idx;

	++*pos;
	if (*pos < 0 || *pos >= 1)
		return NULL;

	idx = *pos + 1;
	return (void *) ((unsigned long) idx);
}

static void mtk_btag_seq_debug_stop(struct seq_file *seq, void *v)
{
}

static int mtk_btag_seq_sub_show(struct seq_file *seq, void *v)
{
	struct mtk_blocktag *btag = seq->private;

	if (btag) {
		mtk_btag_seq_debug_show_ringtrace(NULL, NULL, seq, btag);
		if (btag->seq_show) {
			seq_printf(seq, "<%s: context info>\n", btag->name);
			btag->seq_show(NULL, NULL, seq);
		}
		mtk_btag_seq_sub_show_usedmem(NULL, NULL, seq, btag);
	}
	return 0;
}

static const struct seq_operations mtk_btag_seq_sub_ops = {
	.start  = mtk_btag_seq_debug_start,
	.next   = mtk_btag_seq_debug_next,
	.stop   = mtk_btag_seq_debug_stop,
	.show   = mtk_btag_seq_sub_show,
};

static int mtk_btag_sub_open(struct inode *inode, struct file *file)
{
	int rc;

	rc = seq_open(file, &mtk_btag_seq_sub_ops);

	if (!rc) {
		struct seq_file *m = file->private_data;
		struct dentry *entry = container_of(inode->i_dentry.first,
			struct dentry, d_u.d_alias);

		if (entry && entry->d_parent) {
			pr_notice("[BLOCK_TAG] %s: %s/%s\n", __func__,
				entry->d_parent->d_name.name,
				entry->d_name.name);
			m->private =
		mtk_btag_find_locked(entry->d_parent->d_name.name);
		}
	}
	return rc;
}

static const struct file_operations mtk_btag_sub_fops = {
	.owner		= THIS_MODULE,
	.open		= mtk_btag_sub_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
	.write		= mtk_btag_sub_write,
};

static ssize_t mtk_btag_mictx_sub_write(struct file *file,
	const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	int ret;
	char cmd[MICTX_PROC_CMD_BUF_SIZE];

	if (count == 0)
		goto err;
	else if (count > MICTX_PROC_CMD_BUF_SIZE)
		count = MICTX_PROC_CMD_BUF_SIZE;

	ret = copy_from_user(cmd, ubuf, count);

	if (ret < 0)
		goto err;

	if (cmd[0] == '1')
		mtk_btag_mictx_enable(1);
	else if (cmd[0] == '2')
		mtk_btag_mictx_enable(0);
	else if (cmd[0] == '3')
		mtk_btag_mictx_debug = 1;
	else if (cmd[0] == '4')
		mtk_btag_mictx_debug = 0;
	else {
		pr_info("[pidmap] invalid arg: 0x%x\n", cmd[0]);
		goto err;
	}

	return count;

err:
	return -1;
}
static int mtk_btag_mctx_sub_show(struct seq_file *s, void *data)
{
	seq_puts(s, "<MTK Blocktag Mini Context>\n");
	seq_puts(s, "Status:\n");
	seq_printf(s, "  Ready: %d\n", mtk_btag_mictx_ready);
	seq_printf(s, "  Mictx Instance: %p\n", mtk_btag_mictx);
	seq_puts(s, "Commands:\n");
	seq_puts(s, "  Enable Mini Context : echo 1 > blocktag_mictx\n");
	seq_puts(s, "  Disable Mini Context: echo 2 > blocktag_mictx\n");
	seq_puts(s, "  Enable Self-Test    : echo 3 > blocktag_mictx\n");
	seq_puts(s, "  Disable Self-Test   : echo 4 > blocktag_mictx\n");
	return 0;
}

static int mtk_btag_mictx_sub_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_btag_mctx_sub_show, inode->i_private);
}

static const struct file_operations mtk_btag_mictx_sub_fops = {
	.owner		= THIS_MODULE,
	.open		= mtk_btag_mictx_sub_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
	.write		= mtk_btag_mictx_sub_write,
};

struct mtk_blocktag *mtk_btag_alloc(const char *name,
	unsigned int ringtrace_count, size_t ctx_size, unsigned int ctx_count,
	mtk_btag_seq_f seq_show)
{
	struct mtk_blocktag *btag;

	if (!name || !ringtrace_count || !ctx_size || !ctx_count)
		return NULL;

	btag = mtk_btag_find(name);
	if (btag) {
		pr_notice("[BLOCK_TAG] %s: blocktag %s already exists.\n",
			__func__, name);
		return NULL;
	}

	btag = kmalloc(sizeof(struct mtk_blocktag), GFP_NOFS);
	if (!btag)
		return NULL;

	memset(btag, 0, sizeof(struct mtk_blocktag));
	btag->seq_show = seq_show;
	btag->used_mem = sizeof(struct mtk_blocktag) +
		(sizeof(struct mtk_btag_trace) * ringtrace_count) +
		(ctx_count * ctx_size);

	/* ringtrace */
	btag->rt.index = 0;
	btag->rt.max = ringtrace_count;
	spin_lock_init(&btag->rt.lock);
	btag->rt.trace = kmalloc_array(ringtrace_count,
		sizeof(struct mtk_btag_trace), GFP_NOFS);
	if (!btag->rt.trace) {
		kfree(btag);
		return NULL;
	}
	memset(btag->rt.trace, 0,
		(sizeof(struct mtk_btag_trace) * ringtrace_count));
	strncpy(btag->name, name, BLOCKTAG_NAME_LEN-1);
	spin_lock_init(&btag->rt.lock);

	/* context */
	btag->ctx.count = ctx_count;
	btag->ctx.size = ctx_size;
	btag->ctx.priv = kmalloc_array(ctx_count, ctx_size, GFP_NOFS);
	if (!btag->ctx.priv) {
		kfree(btag->rt.trace);
		kfree(btag);
		return NULL;
	}
	memset(btag->ctx.priv, 0, ctx_size * ctx_count);

	/* procfs dentries */
	mtk_btag_init_procfs();
	btag->dentry.droot = proc_mkdir(name, btag_proc_root);

	if (IS_ERR(btag->dentry.droot))
		goto out;

	btag->dentry.dlog = proc_create("blockio", S_IFREG | 0444,
		btag->dentry.droot, &mtk_btag_sub_fops);

	if (IS_ERR(btag->dentry.dlog))
		goto out;

	btag->dentry.dlog_mictx = proc_create("blockio_mictx",
		S_IFREG | 0444,
		btag->dentry.droot, &mtk_btag_mictx_sub_fops);

	if (IS_ERR(btag->dentry.dlog_mictx))
		goto out;

out:
	spin_lock_init(&btag->prbuf.lock);
	list_add(&btag->list, &mtk_btag_list);

	return btag;
}
EXPORT_SYMBOL_GPL(mtk_btag_alloc);

void mtk_btag_free(struct mtk_blocktag *btag)
{
	if (!btag)
		return;

	list_del(&btag->list);
	kfree(btag->ctx.priv);
	kfree(btag->rt.trace);
	kfree(btag);
}
EXPORT_SYMBOL_GPL(mtk_btag_free);

static int __init mtk_btag_early_memory_info(void)
{
	phys_addr_t start, end;

	start = memblock_start_of_DRAM();
	end = memblock_end_of_DRAM();
	mtk_btag_system_dram_size = (unsigned long long)(end - start);
	pr_debug("[BLOCK_TAG] DRAM: %pa - %pa, size: 0x%llx\n", &start,
		&end, (unsigned long long)(end - start));
	return 0;
}
fs_initcall(mtk_btag_early_memory_info);

static void mtk_btag_pidlogger_init(void)
{
	unsigned long count = mtk_btag_system_dram_size >> PAGE_SHIFT;
	unsigned long size = count * sizeof(struct page_pid_logger);

	if (mtk_btag_pagelogger)
		goto init;

#ifdef CONFIG_MTK_USE_RESERVED_EXT_MEM
	mtk_btag_pagelogger = extmem_malloc_page_align(size);
#else
	mtk_btag_pagelogger = vmalloc(size);
#endif

init:
	if (mtk_btag_pagelogger)
		memset(mtk_btag_pagelogger, -1, size);
	else
		pr_info(
		"[BLOCK_TAG] blockio: fail to allocate mtk_btag_pagelogger\n");
}

static void mtk_btag_seq_main_info(char **buff, unsigned long *size,
	struct seq_file *seq)
{
	size_t used_mem = 0;
	struct mtk_blocktag *btag, *n;

	SPREAD_PRINTF(buff, size, seq, "[Trace]\n");
	mutex_lock(&mtk_btag_list_lock);
	list_for_each_entry_safe(btag, n, &mtk_btag_list, list)
		mtk_btag_seq_debug_show_ringtrace(buff, size, seq, btag);

	SPREAD_PRINTF(buff, size, seq, "[Info]\n");
	list_for_each_entry_safe(btag, n, &mtk_btag_list, list)
		if (btag->seq_show) {
			SPREAD_PRINTF(buff, size, seq, "<%s: context info>\n",
					btag->name);
			btag->seq_show(buff, size, seq);
		}

	SPREAD_PRINTF(buff, size, seq, "[Memory Usage]\n");
	list_for_each_entry_safe(btag, n, &mtk_btag_list, list)
		used_mem += mtk_btag_seq_sub_show_usedmem(buff, size,
				seq, btag);
	mutex_unlock(&mtk_btag_list_lock);

	SPREAD_PRINTF(buff, size, seq, "<blocktag core>\n");
	used_mem += mtk_btag_seq_pidlog_usedmem(buff, size, seq);

	SPREAD_PRINTF(buff, size, seq, "--------------------------------\n");
	SPREAD_PRINTF(buff, size, seq, "Total: %zu KB\n", used_mem >> 10);
}

static int mtk_btag_seq_main_show(struct seq_file *seq, void *v)
{
	mtk_btag_seq_main_info(NULL, NULL, seq);
	return 0;
}

void get_blockio_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	unsigned long free_size = BLOCKIO_AEE_BUFFER_SIZE;
	char *buff;

	buff = blockio_aee_buffer;
	mtk_btag_seq_main_info(&buff, &free_size, NULL);
	/* retrun start location */
	*vaddr = (unsigned long)blockio_aee_buffer;
	*size = BLOCKIO_AEE_BUFFER_SIZE - free_size;
}
EXPORT_SYMBOL(get_blockio_aee_buffer);

static const struct seq_operations mtk_btag_seq_main_ops = {
	.start  = mtk_btag_seq_debug_start,
	.next   = mtk_btag_seq_debug_next,
	.stop   = mtk_btag_seq_debug_stop,
	.show   = mtk_btag_seq_main_show,
};

static int mtk_btag_main_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &mtk_btag_seq_main_ops);
}

static const struct file_operations mtk_btag_main_fops = {
	.owner		= THIS_MODULE,
	.open		= mtk_btag_main_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
	.write		= mtk_btag_main_write,
};

static int mtk_btag_init_procfs(void)
{
	struct proc_dir_entry *proc_entry;
	kuid_t uid;
	kgid_t gid;

	if (btag_proc_root)
		return 0;

	uid = make_kuid(&init_user_ns, 0);
	gid = make_kgid(&init_user_ns, 1001);

	btag_proc_root = proc_mkdir("blocktag", NULL);

	proc_entry = proc_create("blockio", S_IFREG | 0444, btag_proc_root,
			      &mtk_btag_main_fops);

	if (proc_entry)
		proc_set_user(proc_entry, uid, gid);
	else
		pr_info("[BLOCK_TAG} %s: failed to initialize procfs", __func__);

	return 0;
}


static inline
struct mtk_btag_mictx_struct *mtk_btag_mictx_get_ctx(void)
{
	if (mtk_btag_mictx_ready)
		return mtk_btag_mictx;
	else
		return NULL;
}

void mtk_btag_mictx_eval_tp(
	unsigned int write, __u64 usage, __u32 size)
{
	struct mtk_btag_mictx_struct *ctx;
	struct mtk_btag_throughput_rw *tprw;
	unsigned long flags;
	__u64 cur_time = sched_clock();
	__u64 req_begin_time;

	ctx = mtk_btag_mictx_get_ctx();
	if (!ctx)
		return;

	tprw = (write) ? &ctx->tp.w : &ctx->tp.r;
	spin_lock_irqsave(&ctx->lock, flags);
	tprw->size += size;
	tprw->usage += usage;

	ctx->tp_max_time = cur_time;
	req_begin_time = cur_time - usage;

	if (!ctx->tp_min_time)
		ctx->tp_min_time = req_begin_time;
	else {
		if (req_begin_time < ctx->tp_min_time)
			ctx->tp_min_time = req_begin_time;
	}
	spin_unlock_irqrestore(&ctx->lock, flags);
}

void mtk_btag_mictx_eval_req(
	unsigned int write, __u32 cnt, __u32 size)
{
	struct mtk_btag_mictx_struct *ctx;
	struct mtk_btag_req_rw *reqrw;
	unsigned long flags;

	ctx = mtk_btag_mictx_get_ctx();
	if (!ctx)
		return;

	reqrw = (write) ? &ctx->req.w : &ctx->req.r;
	spin_lock_irqsave(&ctx->lock, flags);
	reqrw->count += cnt;
	reqrw->size += size;
	spin_unlock_irqrestore(&ctx->lock, flags);
}

void mtk_btag_mictx_update_ctx(
	__u32 q_depth)
{
	struct mtk_btag_mictx_struct *ctx;
	unsigned long flags;

	ctx = mtk_btag_mictx_get_ctx();
	if (!ctx)
		return;

	spin_lock_irqsave(&ctx->lock, flags);
	ctx->q_depth = q_depth;

	if (!ctx->q_depth) {
		ctx->idle_begin = sched_clock();
	} else {
		if (ctx->idle_begin) {
			ctx->idle_total +=
				(sched_clock() - ctx->idle_begin);
			ctx->idle_begin = 0;
		}
	}
	spin_unlock_irqrestore(&ctx->lock, flags);
}

static void mtk_btag_mictx_reset(
	struct mtk_btag_mictx_struct *ctx,
	__u64 window_begin)
{
	if (!window_begin)
		window_begin = sched_clock();
	ctx->window_begin = window_begin;

	if (!ctx->q_depth)
		ctx->idle_begin = ctx->window_begin;
	else
		ctx->idle_begin = 0;

	ctx->idle_total = 0;
	ctx->tp_min_time = ctx->tp_max_time = 0;
	memset(&ctx->tp, 0, sizeof(struct mtk_btag_throughput));
	memset(&ctx->req, 0, sizeof(struct mtk_btag_req));
}

int mtk_btag_mictx_get_data(
	struct mtk_btag_mictx_iostat_struct *iostat)
{
	struct mtk_btag_mictx_struct *ctx;
	__u64 time_cur, dur, tp_dur;
	unsigned long flags;

	ctx = mtk_btag_mictx_get_ctx();
	if (!ctx || !iostat)
		return -1;

	spin_lock_irqsave(&ctx->lock, flags);

	time_cur = sched_clock();
	dur = time_cur - ctx->window_begin;

	/* fill-in duration */
	iostat->duration = dur;

	/* calculate throughput (per-request) */
	iostat->tp_req_r = mtk_btag_eval_tp_speed(
		ctx->tp.r.size, ctx->tp.r.usage);
	iostat->tp_req_w = mtk_btag_eval_tp_speed(
		ctx->tp.w.size, ctx->tp.w.usage);

	/* calculate throughput (overlapped, not 100% precise) */
	tp_dur = ctx->tp_max_time - ctx->tp_min_time;
	iostat->tp_all_r = mtk_btag_eval_tp_speed(
		ctx->tp.r.size, tp_dur);
	iostat->tp_all_w = mtk_btag_eval_tp_speed(
		ctx->tp.w.size, tp_dur);

	/* provide request count and size */
	iostat->reqcnt_r = ctx->req.r.count;
	iostat->reqsize_r = ctx->req.r.size;
	iostat->reqcnt_w = ctx->req.w.count;
	iostat->reqsize_w = ctx->req.w.size;

	/* calculate workload */
	if (ctx->idle_begin)
		ctx->idle_total += (time_cur - ctx->idle_begin);

	iostat->wl = 100 -
		((__u32)((ctx->idle_total >> 10) * 100) / (__u32)(dur >> 10));

	/* fill-in cmdq depth */
	iostat->q_depth = ctx->q_depth;

	/* everything was provided, now we can reset the ctx */
	mtk_btag_mictx_reset(ctx, time_cur);

	spin_unlock_irqrestore(&ctx->lock, flags);

	return 0;
}

void mtk_btag_mictx_enable(int enable)
{
	if (enable && mtk_btag_mictx)
		return;

	if (enable) {
		mtk_btag_mictx =
			kzalloc(sizeof(struct mtk_btag_mictx_struct), GFP_NOFS);
		if (!mtk_btag_mictx) {
			pr_info("[BLOCK_TAG] mtk_btag_mictx allocation fail, disabled.\n");
			return;
		}

		spin_lock_init(&mtk_btag_mictx->lock);
		mtk_btag_mictx_reset(mtk_btag_mictx, 0);
		mtk_btag_mictx_ready = 1;

	} else {
		if (!mtk_btag_mictx)
			return;

		mtk_btag_mictx_ready = 0;
		kfree(mtk_btag_mictx);
		mtk_btag_mictx = NULL;
	}
}

static int __init mtk_btag_init(void)
{
	mtk_btag_pidlogger_init();
	mtk_btag_init_procfs();
	return 0;
}

static void __exit mtk_btag_exit(void)
{
	proc_remove(btag_proc_root);
}

module_init(mtk_btag_init);
module_exit(mtk_btag_exit);

MODULE_AUTHOR("Perry Hsu <perry.hsu@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Storage Block Tag Trace");

