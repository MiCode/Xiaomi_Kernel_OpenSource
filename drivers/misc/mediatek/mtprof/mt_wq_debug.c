#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/mt_wq_debug.h>
#include <linux/xlog.h>

#define WQ_VERBOSE_DEBUG 0

#define WK_REC_NUM 10
#define WK_CBNAME_LEN 32
#define WQ_WORK_ACTIVED 0
#define WQ_WORK_QUEUED 1

struct work_log {
	int req_cpu;
	int addr;
	int activated;
	char cbname[WK_CBNAME_LEN];
};

struct wq_debug {
	int idx;		/* the current array idx to fill in */
	struct work_log wklog[WK_REC_NUM];
};

#ifdef CONFIG_KALLSYMS
extern unsigned long kallsyms_lookup_name(const char *name);
extern int lookup_module_symbol_attrs(unsigned long addr, unsigned long *size,
				      unsigned long *offset, char *modname, char *name);
#endif

DEFINE_PER_CPU(struct wq_debug, wq_debugger);

/* mt_wq_debug_queue_work is invoked while gcwq->lock is held. */
void mt_wq_debug_queue_work(int req_cpu, struct work_struct *work)
{
	struct wq_debug *wqdbg = &__raw_get_cpu_var(wq_debugger);
	int i, len, lastidx, idx = wqdbg->idx, found = 0;
	char func[128];		/* WK_CBNAME_LEN]; */

	/* snprintf(func, WK_CBNAME_LEN, "%pf", work->func); */

	/* if queuing same wk on same cpu, and it hadn't been activated, ignore it due to we have one. */
	if (idx == 0)
		lastidx = WK_REC_NUM - 1;
	else
		lastidx = idx - 1;

	if (wqdbg->wklog[lastidx].addr == (unsigned int)work &&
	    wqdbg->wklog[lastidx].req_cpu == req_cpu &&
	    wqdbg->wklog[lastidx].activated == WQ_WORK_ACTIVED) {
		wqdbg->wklog[lastidx].activated = WQ_WORK_QUEUED;
#if WQ_VERBOSE_DEBUG
		pr_err("ignore work: %x\n", work);
		return;
	} else
		pr_err("queue work idx: %d vs %d, addr: %x, %x, cpu: %d vs %d, %s\n",
		       idx, lastidx,
		       wqdbg->wklog[lastidx].addr,
		       work,
		       wqdbg->wklog[lastidx].req_cpu,
		       req_cpu,
		       wqdbg->wklog[lastidx].activated == WQ_WORK_ACTIVED ? "activated" : "queued");
#else
		return;
	}
#endif


	/* get next activated entry to fill. */
	for (i = idx; i < WK_REC_NUM; i++) {
		if (WQ_WORK_ACTIVED == wqdbg->wklog[i].activated) {
#if WQ_VERBOSE_DEBUG
			if (idx != i)
				pr_err("queue work idx update: %d -> %d\n", idx, i);
#endif
			idx = i;
			found = 1;
			break;
		}
	}
	if (!found) {
		for (i = 0; i < lastidx; i++) {
			if (WQ_WORK_ACTIVED == wqdbg->wklog[i].activated) {
#if WQ_VERBOSE_DEBUG
				if (idx != i)
					pr_err("queue work idx update: %d -> %d\n", idx, i);
#endif
				idx = i;
				break;
			}
		}
	}

	sprintf(func, "%pf", work->func);

#if WQ_VERBOSE_DEBUG
	pr_err("queue work add %d\n", idx);
#endif

	wqdbg->wklog[idx].addr = (unsigned int)work;
	len = strlen(func) > WK_CBNAME_LEN - 1 ? WK_CBNAME_LEN - 1 : strlen(func);
	strncpy(wqdbg->wklog[idx].cbname, func, len);
	wqdbg->wklog[idx].cbname[len] = '\0';
	wqdbg->wklog[idx].req_cpu = req_cpu;
	wqdbg->wklog[idx].activated = WQ_WORK_QUEUED;

	idx++;
	idx %= WK_REC_NUM;

	wqdbg->idx = idx;
}

/* mt_wq_debug_activate_work is invoked while gcwq->lock is held. */
void mt_wq_debug_activate_work(struct work_struct *work)
{
	struct wq_debug *wqdbg = &__raw_get_cpu_var(wq_debugger);
	int i, n_start = wqdbg->idx, n_end;

	/* in general, it's the last one we push to debugger zone */
	if (n_start == 0) {
		n_start = WK_REC_NUM - 1;
		n_end = 0;
	} else {
		n_end = n_start;
		n_start -= 1;
	}

	/* dirty work... */
	for (i = n_start; i >= 0; i--) {
#if WQ_VERBOSE_DEBUG
		pr_err("activate work %d addr:%x vs %x\n", i, wqdbg->wklog[i].addr, work);
#endif
		if (wqdbg->wklog[i].addr == (unsigned int)work)
			wqdbg->wklog[i].activated = WQ_WORK_ACTIVED;
		return;
	}
	for (i = WK_REC_NUM - 1; i >= n_end; i--) {
#if WQ_VERBOSE_DEBUG
		pr_err("activate work %d addr:%x vs %x\n", i, wqdbg->wklog[i].addr, work);
#endif
		if (wqdbg->wklog[i].addr == (unsigned int)work)
			wqdbg->wklog[i].activated = WQ_WORK_ACTIVED;
		return;
	}
	pr_err("activate work addr not found!!! %x\n", (unsigned int)work);
}

int mt_dump_wq_debugger(void)
{
#define WQ_LOG_TAG "wq_debug"
	int dumpRemain = 0, dumpFrom, i, cpu;
	int address = 0, ret = -1;
	char symName[128];

	for_each_possible_cpu(cpu) {
		dumpFrom = per_cpu(wq_debugger, cpu).idx;
		if (dumpFrom > 0)
			dumpRemain = dumpFrom;

#if WQ_VERBOSE_DEBUG
		pr_err("wq debugger: From:%d, Remain:%d\n", dumpFrom, dumpRemain);
#endif
		for (i = dumpFrom; i < WK_REC_NUM; i++) {
#ifdef CONFIG_KALLSYMS
			address = kallsyms_lookup_name(per_cpu(wq_debugger, cpu).wklog[i].cbname);
			ret = lookup_module_symbol_attrs(address, NULL, NULL, &symName[0], NULL);
#endif
			pr_err
			    ("wq:cpu:%d,idx:%d,req_cpu:%d,wkaddr:%x,cb:%s,cbaddr:%x,module:%s,%s\n",
			     cpu, i, per_cpu(wq_debugger, cpu).wklog[i].req_cpu,
			     per_cpu(wq_debugger, cpu).wklog[i].addr, per_cpu(wq_debugger,
									      cpu).wklog[i].cbname,
			     address, ret != 0 ? "null" : symName, per_cpu(wq_debugger,
									   cpu).wklog[i].
			     activated == WQ_WORK_ACTIVED ? "activated" : "queued");
		}
		for (i = 0; i < dumpRemain; i++) {
#ifdef CONFIG_KALLSYMS
			address = kallsyms_lookup_name(per_cpu(wq_debugger, cpu).wklog[i].cbname);
			ret = lookup_module_symbol_attrs(address, NULL, NULL, &symName[0], NULL);
#endif
			pr_err
			    ("wq:cpu:%d,idx:%d,req_cpu:%d,wkaddr:%x,cb:%s,cbaddr:%x,module:%s,%s\n",
			     cpu, i, per_cpu(wq_debugger, cpu).wklog[i].req_cpu,
			     per_cpu(wq_debugger, cpu).wklog[i].addr, per_cpu(wq_debugger,
									      cpu).wklog[i].cbname,
			     address, ret != 0 ? "null" : symName, per_cpu(wq_debugger,
									   cpu).wklog[i].
			     activated == WQ_WORK_ACTIVED ? "activated" : "queued");
		}
	}
	return 0;
}
EXPORT_SYMBOL(mt_dump_wq_debugger);

void mttrace_workqueue_execute_work(struct work_struct *work)
{
	if (unlikely(wq_tracing & WQ_DUMP_EXECUTE_WORK))
		pr_err("execute work=%p\n", (void *)work);
}
EXPORT_SYMBOL(mttrace_workqueue_execute_work);

void mttrace_workqueue_execute_end(struct work_struct *work)
{
	if (unlikely(wq_tracing & WQ_DUMP_EXECUTE_WORK))
		pr_err("execute end work=%p\n", (void *)work);
}
EXPORT_SYMBOL(mttrace_workqueue_execute_end);

void mttrace_workqueue_activate_work(struct work_struct *work)
{
	if (unlikely(wq_tracing & WQ_DUMP_ACTIVE_WORK))
		pr_err("activate work=%p\n", (void *)work);

	if (likely(wq_debugger_enable))
		mt_wq_debug_activate_work(work);
}
EXPORT_SYMBOL(mttrace_workqueue_activate_work);

void mttrace_workqueue_queue_work(unsigned int req_cpu, struct work_struct *work)
{
	if (unlikely(wq_tracing & WQ_DUMP_QUEUE_WORK))
		pr_err("queue work=%p function=%pf req_cpu=%u\n",
		       (void *)work, (void *)work->func, req_cpu);

	if (likely(wq_debugger_enable))
		mt_wq_debug_queue_work(req_cpu, work);
}
EXPORT_SYMBOL(mttrace_workqueue_queue_work);
