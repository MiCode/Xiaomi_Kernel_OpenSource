

#ifndef UAPI_IRQ_STATS_H
#define UAPI_IRQ_STATS_H

#include <linux/ioctl.h>

struct xm_irq_stats_settings {
	unsigned int activated;
};

struct irq_stats_irq_summary {
	////int et_type;
	int cpu;
	////unsigned long id;
	unsigned long irq_cnt;
	unsigned long irq_run_total;
	unsigned long max_irq;
	unsigned long max_irq_time;
};

struct irq_stats_irq_detail {
	////int et_type;
	int cpu;
	////unsigned long id;
	unsigned int		irq;
	void *handler;
	unsigned long irq_cnt;
	unsigned long irq_run_total;
};

struct irq_stats_softirq_summary {
	////int et_type;
	int cpu;
	////unsigned long id;
	unsigned long softirq_cnt[XM_NR_SOFTIRQS];
	////unsigned long softirq_cnt_d[XM_NR_SOFTIRQS];
    unsigned long sortirq_run_total[XM_NR_SOFTIRQS];
	////unsigned long sortirq_run_total_d[XM_NR_SOFTIRQS];
};

#endif /* UAPI_IRQ_STATS_H */
