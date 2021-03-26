/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _BLOCK_BLK_PM_H_
#define _BLOCK_BLK_PM_H_

#include <linux/pm_runtime.h>

#ifdef CONFIG_PM
static inline void blk_pm_request_resume(struct request_queue *q)
{
	if (q->dev && (q->rpm_status == RPM_SUSPENDED ||
		       q->rpm_status == RPM_SUSPENDING))
		pm_request_resume(q->dev);
}

static inline void blk_pm_mark_last_busy(struct request *rq)
{
	if (rq->q->dev && !(rq->rq_flags & RQF_PM))
		pm_runtime_mark_last_busy(rq->q->dev);
}
#else
static inline void blk_pm_request_resume(struct request_queue *q)
{
}

static inline void blk_pm_mark_last_busy(struct request *rq)
{
}
#endif

#endif /* _BLOCK_BLK_PM_H_ */
