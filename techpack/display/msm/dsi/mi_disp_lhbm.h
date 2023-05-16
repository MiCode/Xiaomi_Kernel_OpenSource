#ifndef _MI_DISP_LHBM_H_
#define _MI_DISP_LHBM_H_

#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <uapi/linux/sched/types.h>
#include "dsi_display.h"
#include "sde_connector.h"
#include "dsi_panel_mi.h"
#include "dsi_mi_feature.h"

#define LHBM_TAG "[mi-lhbm]"

enum disp_display_type {
	MI_DISP_PRIMARY = 0,
	MI_DISP_SECONDARY,
	MI_DISP_MAX,
};

struct disp_work {
	struct kthread_work work;
	struct kthread_delayed_work delayed_work;
	struct disp_display *dd_ptr;
	wait_queue_head_t *wq;
	void *data;
};

struct disp_thread {
	struct task_struct *thread;
	struct kthread_worker worker;
	struct disp_display *dd_ptr;
};

struct disp_display {
	void *display;
	int intf_type;
	struct mutex mutex_lock;
};

struct disp_lhbm {
	struct disp_display d_display[MI_DISP_MAX];
	struct disp_thread fod_thread;
	//wait_queue_head_t fod_pending_wq;
};

struct fod_work_data {
	struct dsi_display *display;
	int fod_btn;
	bool from_touch;
};

enum {
	FOD_WORK_INIT = 0,
	FOD_WORK_DOING = 1,
	FOD_WORK_DONE = 2,
};

enum mi_panel_op_code {
	MI_FOD_HBM_ON = 0,
	MI_FOD_HBM_OFF,
	MI_FOD_AOD_TO_NORMAL,
	MI_FOD_NORMAL_TO_AOD,
};

enum local_lhbm_target_brightness {
	LOCAL_LHBM_TARGET_BRIGHTNESS_NONE,
	LOCAL_LHBM_TARGET_BRIGHTNESS_WHITE_1000NIT,
	LOCAL_LHBM_TARGET_BRIGHTNESS_WHITE_110NIT,
	LOCAL_LHBM_TARGET_BRIGHTNESS_GREEN_500NIT,
	LOCAL_LHBM_TARGET_BRIGHTNESS_MAX
};

int mi_disp_set_fod_queue_work(u32 fod_btn, bool from_touch);
#endif

