// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Xiaomi, Inc.
 */

#include "zisp_rproc_utils.h"

static int zisp_rproc_ipi_thread(void *data)
{
	struct zisp_rproc_ipi *ipi;
	struct rproc *rproc;
	int vqid;

	ipi = data;
	rproc = ipi->rproc;
	while (!kthread_should_stop()) {
		atomic_set(&ipi->recv_wakeup, 0);
		for (vqid = 0; vqid < rproc->max_notifyid; vqid++)
			rproc_vq_interrupt(rproc, vqid);
		set_current_state(TASK_INTERRUPTIBLE);
		if (!atomic_read(&ipi->recv_wakeup))
			schedule();
	}

	ipi->recv_thread = NULL;

	return 0;
}

static irqreturn_t zisp_rproc_ipi_callback(int irq, void *data)
{
	struct zisp_rproc_ipi *ipi;

	ipi = data;
	atomic_set(&ipi->recv_wakeup, 1);
	if (ipi->recv_thread)
		wake_up_process(ipi->recv_thread);

	return IRQ_HANDLED;
}

int zisp_rproc_ipi_setup(struct zisp_rproc_ipi *ipi, struct rproc *rproc)
{
	int ret;
	struct sched_param param = {
		.sched_priority = 3 * MAX_USER_RT_PRIO / 4,
	};

	ipi->rproc = rproc;
	ipi->recv_thread = kthread_create(zisp_rproc_ipi_thread, ipi,
					  dev_name(&rproc->dev));
	ipi->rx_callback = zisp_rproc_ipi_callback;
	if (IS_ERR(ipi->recv_thread)) {
		ret = PTR_ERR(ipi->recv_thread);
		dev_err(&rproc->dev, "kthread_create failed: %d\n", ret);
		return ret;
	}
	sched_setscheduler(ipi->recv_thread, SCHED_FIFO, &param);
	return 0;
}

void zisp_rproc_ipi_teardown(struct zisp_rproc_ipi *ipi)
{
	kthread_stop(ipi->recv_thread);
}
