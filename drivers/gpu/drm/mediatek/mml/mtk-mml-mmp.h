/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_MML_MMP_H__
#define __MTK_MML_MMP_H__

#if IS_ENABLED(CONFIG_MMPROFILE)
#include <mmprofile.h>
#include <mmprofile_function.h>

#define mmp_data2_fence(c, s)	((c & 0xff) << 24 | s & 0xffffff)

#define mml_mmp(event, flag, v1, v2) \
	mmprofile_log_ex(mml_mmp_get_event()->event, flag, v1, v2)

struct mml_mmp_events_t {
	mmp_event mml;
	mmp_event submit;
	mmp_event config;
	mmp_event buf_map;
	mmp_event fence;
	mmp_event fence_timeout;
	mmp_event flush;
	mmp_event submit_cb;
	mmp_event stop_racing;
	mmp_event irq_loop;
	mmp_event irq_err;
	mmp_event irq_done;
	mmp_event irq_stop;
	mmp_event fence_sig;
	mmp_event exec;
};

void mml_mmp_init(void);
struct mml_mmp_events_t *mml_mmp_get_event(void);
#else

#define mml_mmp(args...)

static inline void mml_mmp_init(void)
{
}
#endif

#endif	/* __MTK_MML_MMP_H__ */
