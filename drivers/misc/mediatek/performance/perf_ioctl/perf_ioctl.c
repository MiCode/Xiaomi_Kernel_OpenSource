// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "perf_ioctl.h"


#define TAG "PERF_IOCTL"

#define MAX_STEP 10

void (*fpsgo_notify_qudeq_fp)(int qudeq,
		unsigned int startend,
		int pid, unsigned long long identifier);
EXPORT_SYMBOL_GPL(fpsgo_notify_qudeq_fp);
void (*fpsgo_notify_connect_fp)(int pid,
		int connectedAPI, unsigned long long identifier);
EXPORT_SYMBOL_GPL(fpsgo_notify_connect_fp);
void (*fpsgo_notify_bqid_fp)(int pid, unsigned long long bufID,
		int queue_SF, unsigned long long identifier, int create);
EXPORT_SYMBOL_GPL(fpsgo_notify_bqid_fp);
void (*fpsgo_notify_vsync_fp)(void);
EXPORT_SYMBOL_GPL(fpsgo_notify_vsync_fp);
void (*fpsgo_get_fps_fp)(int *pid, int *fps);
EXPORT_SYMBOL_GPL(fpsgo_get_fps_fp);
void (*fpsgo_get_cmd_fp)(int *cmd, int *value1, int *value2);
EXPORT_SYMBOL_GPL(fpsgo_get_cmd_fp);
void (*gbe_get_cmd_fp)(int *cmd, int *value1, int *value2);
EXPORT_SYMBOL_GPL(gbe_get_cmd_fp);
int (*fpsgo_get_fstb_active_fp)(long long time_diff);
EXPORT_SYMBOL_GPL(fpsgo_get_fstb_active_fp);
int (*fpsgo_wait_fstb_active_fp)(void);
EXPORT_SYMBOL_GPL(fpsgo_wait_fstb_active_fp);
void (*fpsgo_notify_swap_buffer_fp)(int pid);
EXPORT_SYMBOL_GPL(fpsgo_notify_swap_buffer_fp);

void (*fpsgo_notify_sbe_rescue_fp)(int pid, int start, int enhance, unsigned long long frameID);
EXPORT_SYMBOL_GPL(fpsgo_notify_sbe_rescue_fp);

int (*usrtch_ioctl_fp)(unsigned long arg);
EXPORT_SYMBOL(usrtch_ioctl_fp);

struct proc_dir_entry *perfmgr_root;
EXPORT_SYMBOL(perfmgr_root);

static unsigned long perfctl_copy_from_user(void *pvTo,
		const void __user *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static unsigned long perfctl_copy_to_user(void __user *pvTo,
		const void *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvTo, ulBytes))
		return __copy_to_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

/*--------------------SYNC------------------------*/
static int eas_show(struct seq_file *m, void *v)
{
	return 0;
}

static int eas_open(struct inode *inode, struct file *file)
{
	return single_open(file, eas_show, inode->i_private);
}

static long eas_ioctl_impl(struct file *filp,
		unsigned int cmd, unsigned long arg, void *pKM)
{
	ssize_t ret = 0;
	void __user *ubuf = (struct _CORE_CTL_PACKAGE *)arg;
	struct _CORE_CTL_PACKAGE msgKM = {0};
	bool bval;
#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
	unsigned int sync;
	unsigned int val;
	struct cpumask *cpumask_ptr;
#endif
#if IS_ENABLED(CONFIG_MTK_CPUQOS_V3)
	void __user *ubuf_cpuqos = (struct _CPUQOS_V3_PACKAGE *)arg;
	struct _CPUQOS_V3_PACKAGE msgKM_cpuqos = {0};
#endif

	switch (cmd) {
#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
	case EAS_SYNC_SET:
		if (perfctl_copy_from_user(&sync, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_wake_sync(sync);
		break;
	case EAS_SYNC_GET:
		sync = get_wake_sync();
		if (perfctl_copy_to_user((void *)arg, &sync, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_PERTASK_LS_SET:
		if (perfctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_uclamp_min_ls(val);
		break;
	case EAS_PERTASK_LS_GET:
		val = get_uclamp_min_ls();
		if (perfctl_copy_to_user((void *)arg, &val, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_ACTIVE_MASK_GET:
		val = __cpu_active_mask.bits[0];
		if (perfctl_copy_to_user((void *)arg, &val, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_NEWLY_IDLE_BALANCE_INTERVAL_SET:
		if (perfctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_newly_idle_balance_interval_us(val);
		break;
	case EAS_NEWLY_IDLE_BALANCE_INTERVAL_GET:
		val = get_newly_idle_balance_interval_us();
		if (perfctl_copy_to_user((void *)arg, &val, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_GET_THERMAL_HEADROOM_INTERVAL_SET:
		if (perfctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_get_thermal_headroom_interval_tick(val);
		break;
	case EAS_GET_THERMAL_HEADROOM_INTERVAL_GET:
		val = get_thermal_headroom_interval_tick();
		if (perfctl_copy_to_user((void *)arg, &val, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_SET_SYSTEM_MASK:
		if (perfctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_system_cpumask_int(val);
		break;
	case EAS_GET_SYSTEM_MASK:
		cpumask_ptr = get_system_cpumask();
		val = cpumask_ptr->bits[0];
		if (perfctl_copy_to_user((void *)arg, &val, sizeof(unsigned int)))
			return -1;
		break;
#else
	case EAS_SYNC_SET:
	case EAS_SYNC_GET:
	case EAS_PERTASK_LS_SET:
	case EAS_PERTASK_LS_GET:
	case EAS_ACTIVE_MASK_GET:
	case EAS_NEWLY_IDLE_BALANCE_INTERVAL_SET:
	case EAS_NEWLY_IDLE_BALANCE_INTERVAL_GET:
	case EAS_GET_THERMAL_HEADROOM_INTERVAL_SET:
	case EAS_GET_THERMAL_HEADROOM_INTERVAL_GET:
	case EAS_SET_SYSTEM_MASK:
	case EAS_GET_SYSTEM_MASK:
		break;
#endif
	case CORE_CTL_FORCE_PAUSE_CPU:
		if (perfctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;

		bval = !!msgKM.is_pause;
#if IS_ENABLED(CONFIG_MTK_CORE_CTL)
		ret = core_ctl_force_pause_cpu(msgKM.cpu, bval);
#elif IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
		if (bval)
			ret = sched_pause_cpu(msgKM.cpu);
		else
			ret = sched_resume_cpu(msgKM.cpu);
#else
		return -1;
#endif
		break;
#if IS_ENABLED(CONFIG_MTK_CORE_CTL)
	case CORE_CTL_SET_OFFLINE_THROTTLE_MS:
		if (perfctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		ret = core_ctl_set_offline_throttle_ms(msgKM.cid, msgKM.throttle_ms);
		break;
	case CORE_CTL_SET_LIMIT_CPUS:
		if (perfctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		ret = core_ctl_set_limit_cpus(msgKM.cid, msgKM.min, msgKM.max);
		break;
	case CORE_CTL_SET_NOT_PREFERRED:
		if (perfctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;

		ret = core_ctl_set_not_preferred(msgKM.not_preferred_cpus);
		break;
	case CORE_CTL_SET_BOOST:
		if (perfctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;

		bval = !!msgKM.boost;
		ret = core_ctl_set_boost(bval);
		break;
	case CORE_CTL_SET_UP_THRES:
		if (perfctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		ret = core_ctl_set_up_thres(msgKM.cid, msgKM.thres);
		break;
	case CORE_CTL_ENABLE_POLICY:
		if (perfctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;

		val = msgKM.enable_policy;
		ret = core_ctl_enable_policy(val);
		break;
#else
	case CORE_CTL_SET_OFFLINE_THROTTLE_MS:
	case CORE_CTL_SET_LIMIT_CPUS:
	case CORE_CTL_SET_NOT_PREFERRED:
	case CORE_CTL_SET_BOOST:
	case CORE_CTL_SET_UP_THRES:
	case CORE_CTL_ENABLE_POLICY:
		break;
#endif
#if IS_ENABLED(CONFIG_MTK_CPUQOS_V3)
	case CPUQOS_V3_SET_CPUQOS_MODE:
		if (perfctl_copy_from_user(&msgKM_cpuqos, ubuf_cpuqos,
				sizeof(struct _CPUQOS_V3_PACKAGE)))
			return -1;

		set_cpuqos_mode(msgKM_cpuqos.mode);
		break;
	case CPUQOS_V3_SET_CT_TASK:
		if (perfctl_copy_from_user(&msgKM_cpuqos, ubuf_cpuqos,
				sizeof(struct _CPUQOS_V3_PACKAGE)))
			return -1;

		ret = set_ct_task(msgKM_cpuqos.pid, msgKM_cpuqos.set_task);
		break;
	case CPUQOS_V3_SET_CT_GROUP:
		if (perfctl_copy_from_user(&msgKM_cpuqos, ubuf_cpuqos,
				sizeof(struct _CPUQOS_V3_PACKAGE)))
			return -1;

		ret = set_ct_group(msgKM_cpuqos.group_id, msgKM_cpuqos.set_group);
		break;
#else
	case CPUQOS_V3_SET_CPUQOS_MODE:
	case CPUQOS_V3_SET_CT_TASK:
	case CPUQOS_V3_SET_CT_GROUP:
		break;
#endif
	default:
		pr_debug(TAG "%s %d: unknown cmd %x\n",
			__FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static long eas_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	return eas_ioctl_impl(filp, cmd, arg, NULL);
}

static const struct proc_ops eas_Fops = {
	.proc_ioctl = eas_ioctl,
	.proc_compat_ioctl = eas_ioctl,
	.proc_open = eas_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/*--------------------XGFFRAME------------------------*/
int (*xgff_frame_startend_fp)(unsigned int startend,
		unsigned int tid,
		unsigned long long queueid,
		unsigned long long frameid,
		unsigned long long *cputime,
		unsigned int *area,
		unsigned int *pdeplistsize,
		unsigned int *pdeplist);
EXPORT_SYMBOL(xgff_frame_startend_fp);
void (*xgff_frame_getdeplist_maxsize_fp)(
		unsigned int *pdeplistsize);
EXPORT_SYMBOL(xgff_frame_getdeplist_maxsize_fp);
void (*xgff_frame_min_cap_fp)(unsigned int min_cap);
EXPORT_SYMBOL(xgff_frame_min_cap_fp);

static int xgff_show(struct seq_file *m, void *v)
{
	return 0;
}

static int xgff_open(struct inode *inode, struct file *file)
{
	return single_open(file, xgff_show, inode->i_private);
}

static long xgff_ioctl_impl(struct file *filp,
		unsigned int cmd, unsigned long arg, void *pKM)
{
	ssize_t ret = 0;
	struct _XGFFRAME_PACKAGE *msgKM = NULL,
		*msgUM = (struct _XGFFRAME_PACKAGE *)arg;
	struct _XGFFRAME_PACKAGE smsgKM;

	__u32 *vpdeplist = NULL;
	unsigned int maxsize_deplist = 0;

	msgKM = (struct _XGFFRAME_PACKAGE *)pKM;
	if (!msgKM) {
		msgKM = &smsgKM;
		if (perfctl_copy_from_user(msgKM, msgUM,
				sizeof(struct _XGFFRAME_PACKAGE))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}
	}

	switch (cmd) {
	case XGFFRAME_START:
		if (!xgff_frame_startend_fp || !xgff_frame_getdeplist_maxsize_fp) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}

		xgff_frame_getdeplist_maxsize_fp(&maxsize_deplist);
		vpdeplist = kcalloc(msgKM->deplist_size, sizeof(__u32), GFP_KERNEL);
		if (!vpdeplist) {
			ret = -ENOMEM;
			goto ret_ioctl;
		}
		perfctl_copy_from_user(vpdeplist,
			msgKM->deplist, msgKM->deplist_size * sizeof(__s32));
		if (msgKM->deplist_size > maxsize_deplist)
			msgKM->deplist_size = maxsize_deplist;
		ret = xgff_frame_startend_fp(1, msgKM->tid, msgKM->queueid,
			msgKM->frameid, NULL, NULL, &msgKM->deplist_size, vpdeplist);

		perfctl_copy_to_user(msgUM, msgKM, sizeof(struct _XGFFRAME_PACKAGE));

		kfree(vpdeplist);
		break;
	case XGFFRAME_END:
		if (!xgff_frame_startend_fp || !xgff_frame_getdeplist_maxsize_fp) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}

		xgff_frame_getdeplist_maxsize_fp(&maxsize_deplist);
		vpdeplist = kcalloc(msgKM->deplist_size, sizeof(__u32), GFP_KERNEL);

		if (!vpdeplist) {
			ret = -ENOMEM;
			goto ret_ioctl;
		}
		perfctl_copy_from_user(vpdeplist,
			msgKM->deplist, msgKM->deplist_size * sizeof(__s32));
		if (msgKM->deplist_size > maxsize_deplist)
			msgKM->deplist_size = maxsize_deplist;

		ret = xgff_frame_startend_fp(0, msgKM->tid, msgKM->queueid,
			msgKM->frameid, &msgKM->cputime, &msgKM->area,
			&msgKM->deplist_size, vpdeplist);

		perfctl_copy_to_user(msgUM, msgKM, sizeof(struct _XGFFRAME_PACKAGE));


		kfree(vpdeplist);

		break;

	case XGFFRAME_MIN_CAP:
		if (!xgff_frame_min_cap_fp) {
			ret = -EAGAIN;
			goto ret_ioctl;
		}
		xgff_frame_min_cap_fp((unsigned int)msgKM->min_cap);
		break;

	default:
		pr_debug(TAG "%s %d: unknown cmd %x\n",
			__FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static long xgff_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	return xgff_ioctl_impl(filp, cmd, arg, NULL);
}

static long xgff_compat_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	int ret = -EFAULT;
	struct _XGFFRAME_PACKAGE_32 {
		__u32 tid;
		__u64 queueid;
		__u64 frameid;

		__u64 cputime;
		__u32 area;
		__u32 deplist_size;

		union {
			__u32 *deplist;
			__u64 p_dummy_deplist;
		};
	};

	struct _XGFFRAME_PACKAGE sEaraPackageKM64;
	struct _XGFFRAME_PACKAGE_32 sEaraPackageKM32;
	struct _XGFFRAME_PACKAGE_32 *psEaraPackageKM32 = &sEaraPackageKM32;
	struct _XGFFRAME_PACKAGE_32 *psEaraPackageUM32 =
		(struct _XGFFRAME_PACKAGE_32 *)arg;

	if (perfctl_copy_from_user(psEaraPackageKM32,
			psEaraPackageUM32, sizeof(struct _XGFFRAME_PACKAGE_32)))
		goto unlock_and_return;

	sEaraPackageKM64 = *((struct _XGFFRAME_PACKAGE *)psEaraPackageKM32);
	sEaraPackageKM64.deplist =
		(void *)((size_t) psEaraPackageKM32->deplist);

	ret = xgff_ioctl_impl(filp, cmd, arg, &sEaraPackageKM64);

unlock_and_return:
	return ret;
}

static const struct proc_ops xgff_Fops = {
	.proc_ioctl = xgff_ioctl,
	.proc_compat_ioctl = xgff_compat_ioctl,
	.proc_open = xgff_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/*--------------------INIT------------------------*/

static int device_show(struct seq_file *m, void *v)
{
	return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
	return single_open(file, device_show, inode->i_private);
}

static long device_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;
	int pwr_cmd = -1, value1 = -1, value2 = -1, pwr_pid = -1, pwr_fps = -1;
	struct _FPSGO_PACKAGE *msgKM = NULL,
			*msgUM = (struct _FPSGO_PACKAGE *)arg;
	struct _FPSGO_PACKAGE smsgKM;

	msgKM = &smsgKM;

	if (perfctl_copy_from_user(msgKM, msgUM,
				sizeof(struct _FPSGO_PACKAGE))) {
		ret = -EFAULT;
		goto ret_ioctl;
	}

	switch (cmd) {
#if defined(CONFIG_MTK_FPSGO_V3)
	case FPSGO_QUEUE:
		if (fpsgo_notify_qudeq_fp)
			fpsgo_notify_qudeq_fp(1,
					msgKM->start, msgKM->tid,
					msgKM->identifier);
		break;
	case FPSGO_DEQUEUE:
		if (fpsgo_notify_qudeq_fp)
			fpsgo_notify_qudeq_fp(0,
					msgKM->start, msgKM->tid,
					msgKM->identifier);
		break;
	case FPSGO_QUEUE_CONNECT:
		if (fpsgo_notify_connect_fp)
			fpsgo_notify_connect_fp(msgKM->tid,
					msgKM->connectedAPI, msgKM->identifier);
		break;
	case FPSGO_BQID:
		if (fpsgo_notify_bqid_fp)
			fpsgo_notify_bqid_fp(msgKM->tid, msgKM->bufID,
				msgKM->queue_SF, msgKM->identifier,
				msgKM->start);
		break;
	case FPSGO_TOUCH:
		if (usrtch_ioctl_fp)
			usrtch_ioctl_fp(msgKM->frame_time);
		break;
	case FPSGO_VSYNC:
		if (fpsgo_notify_vsync_fp)
			fpsgo_notify_vsync_fp();
		break;
	case FPSGO_SWAP_BUFFER:
		if (fpsgo_notify_swap_buffer_fp)
			fpsgo_notify_swap_buffer_fp(msgKM->tid);
		break;
	case FPSGO_GET_FPS:
		if (fpsgo_get_fps_fp) {
			fpsgo_get_fps_fp(&pwr_pid, &pwr_fps);
			msgKM->tid = pwr_pid;
			msgKM->value1 = pwr_fps;
		} else
			ret = -1;
		perfctl_copy_to_user(msgUM, msgKM,
				sizeof(struct _FPSGO_PACKAGE));
		break;
	case FPSGO_GET_CMD:
		if (fpsgo_get_cmd_fp) {
			fpsgo_get_cmd_fp(&pwr_cmd, &value1, &value2);
			msgKM->cmd = pwr_cmd;
			msgKM->value1 = value1;
			msgKM->value2 = value2;
		} else
			ret = -1;
		perfctl_copy_to_user(msgUM, msgKM,
				sizeof(struct _FPSGO_PACKAGE));
		break;
	case FPSGO_GBE_GET_CMD:
		if (gbe_get_cmd_fp) {
			gbe_get_cmd_fp(&pwr_cmd, &value1, &value2);
			msgKM->cmd = pwr_cmd;
			msgKM->value1 = value1;
			msgKM->value2 = value2;
		} else
			ret = -1;
		perfctl_copy_to_user(msgUM, msgKM,
				sizeof(struct _FPSGO_PACKAGE));
		break;
	case FPSGO_GET_FSTB_ACTIVE:
		if (fpsgo_get_fstb_active_fp)
			msgKM->active = fpsgo_get_fstb_active_fp(msgKM->time_diff);
		else
			ret = 0;
		perfctl_copy_to_user(msgUM, msgKM,
				sizeof(struct _FPSGO_PACKAGE));
		break;
	case FPSGO_WAIT_FSTB_ACTIVE:
		if (fpsgo_wait_fstb_active_fp)
			fpsgo_wait_fstb_active_fp();
		break;
	case FPSGO_SBE_RESCUE:
		if (fpsgo_notify_sbe_rescue_fp)
			fpsgo_notify_sbe_rescue_fp(msgKM->tid, msgKM->start, msgKM->value2,
						msgKM->frame_id);
		break;

#else
	case FPSGO_TOUCH:
		 [[fallthrough]];
	case FPSGO_QUEUE:
		 [[fallthrough]];
	case FPSGO_DEQUEUE:
		 [[fallthrough]];
	case FPSGO_QUEUE_CONNECT:
		 [[fallthrough]];
	case FPSGO_VSYNC:
		 [[fallthrough]];
	case FPSGO_BQID:
		 [[fallthrough]];
	case FPSGO_SWAP_BUFFER:
		 [[fallthrough]];
	case FPSGO_GET_FPS:
		 [[fallthrough]];
	case FPSGO_GET_CMD:
		 [[fallthrough]];
	case FPSGO_GBE_GET_CMD:
		 [[fallthrough]];
	case FPSGO_GET_FSTB_ACTIVE:
		[[fallthrough]];
	case FPSGO_WAIT_FSTB_ACTIVE:
		[[fallthrough]];
	case FPSGO_SBE_RESCUE:
		break;
#endif

	default:
		pr_debug(TAG "%s %d: unknown cmd %x\n",
			__FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static const struct proc_ops Fops = {
	.proc_compat_ioctl = device_ioctl,
	.proc_ioctl = device_ioctl,
	.proc_open = device_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/*--------------------INIT------------------------*/
static void __exit exit_perfctl(void) {}
//static int __init init_perfctl(struct proc_dir_entry *parent)
static int __init init_perfctl(void)
{
	struct proc_dir_entry *pe, *parent;
	int ret_val = 0;


	pr_debug(TAG"Start to init perf_ioctl driver\n");

	parent = proc_mkdir("perfmgr", NULL);
	perfmgr_root = parent;

	pe = proc_create("perf_ioctl", 0664, parent, &Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		ret_val = -ENOMEM;
		goto out_wq;
	}

	pe = proc_create("eas_ioctl", 0664, parent, &eas_Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		ret_val = -ENOMEM;
		goto out_wq;
	}

	pe = proc_create("xgff_ioctl", 0664, parent, &xgff_Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		ret_val = -ENOMEM;
		goto out_wq;
	}

	pr_debug(TAG"init perf_ioctl driver done\n");

	return 0;

out_wq:
	return ret_val;
}

module_init(init_perfctl);
module_exit(exit_perfctl);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek FPSGO perf_ioctl");
MODULE_AUTHOR("MediaTek Inc.");
