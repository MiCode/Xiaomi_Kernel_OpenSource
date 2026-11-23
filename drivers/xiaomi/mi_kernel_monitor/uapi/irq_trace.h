

#ifndef UAPI_IRQ_TRACE_H
#define UAPI_IRQ_TRACE_H

#include <linux/ioctl.h>

struct xm_irq_trace_settings {
	unsigned int activated;
	int threshold_irq, threshold_sirq, threshold_timer;
};

struct IRQ_TRACE_DETAIL {
	////struct xm_timespec tv;
	int cpu;
	int source;
	void *func;
	unsigned long time;
    u64 timestamp;
};

#endif /* UAPI_IRQ_TRACE_H */
