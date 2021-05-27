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
int (*fpsgo_get_fps_fp)(void);
EXPORT_SYMBOL_GPL(fpsgo_get_fps_fp);
void (*fpsgo_get_cmd_fp)(int *cmd, int *value1, int *value2);
EXPORT_SYMBOL_GPL(fpsgo_get_cmd_fp);
void (*fpsgo_notify_nn_job_begin_fp)(unsigned int tid, unsigned long long mid);
void (*fpsgo_notify_nn_job_end_fp)(int pid, int tid, unsigned long long mid,
	int num_step, __s32 *boost, __s32 *device, __u64 *exec_time);
int (*fpsgo_get_nn_priority_fp)(unsigned int pid, unsigned long long mid);
void (*fpsgo_get_nn_ttime_fp)(unsigned int pid, unsigned long long mid,
	int num_step, __u64 *ttime);
void (*fpsgo_notify_swap_buffer_fp)(int pid);
EXPORT_SYMBOL_GPL(fpsgo_notify_swap_buffer_fp);

void (*rsu_getusage_fp)(__s32 *devusage, __u32 *bwusage, __u32 pid);
void (*rsu_getstate_fp)(int *throttled);

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

static void perfctl_notify_fpsgo_nn_begin(
	struct _EARA_NN_PACKAGE *msgKM,
	struct _EARA_NN_PACKAGE *msgUM)
{
	int arr_length;
	__u64 *__target_time = NULL;

	if (!fpsgo_notify_nn_job_begin_fp || !fpsgo_get_nn_ttime_fp)
		return;

	if (msgKM->num_step > MAX_STEP)
		return;

	fpsgo_notify_nn_job_begin_fp(msgKM->tid, msgKM->mid);

	arr_length = msgKM->num_step * MAX_DEVICE;
	__target_time =
		kcalloc(arr_length, sizeof(__u64), GFP_KERNEL);

	if (!__target_time)
		return;

	fpsgo_get_nn_ttime_fp(msgKM->pid,
		msgKM->mid, msgKM->num_step, __target_time);

	if (msgKM->target_time)
		perfctl_copy_to_user(msgKM->target_time,
			__target_time, arr_length * sizeof(__u64));

	kfree(__target_time);
}

static void perfctl_notify_fpsgo_nn_end(
	struct _EARA_NN_PACKAGE *msgKM,
	struct _EARA_NN_PACKAGE *msgUM)
{
	__s32 *boost, *device;
	__u64 *exec_time;
	int size;

	if (!fpsgo_notify_nn_job_end_fp || !fpsgo_get_nn_priority_fp)
		return;

	if (msgKM->num_step > MAX_STEP)
		return;

	size = msgKM->num_step * MAX_DEVICE;

	if (!msgKM->boost || !msgKM->device || !msgKM->exec_time)
		goto out_um_malloc_fail;

	boost = kmalloc_array(size, sizeof(__s32), GFP_KERNEL);
	if (!boost)
		goto out_boost_malloc_fail;
	device = kmalloc_array(size, sizeof(__s32), GFP_KERNEL);
	if (!device)
		goto out_device_malloc_fail;
	exec_time = kmalloc_array(size, sizeof(__u64), GFP_KERNEL);
	if (!exec_time)
		goto out_exec_time_malloc_fail;

	perfctl_copy_from_user(boost,
		msgKM->boost, size * sizeof(__s32));
	perfctl_copy_from_user(device,
		msgKM->device, size * sizeof(__s32));
	perfctl_copy_from_user(exec_time,
		msgKM->exec_time, size * sizeof(__u64));

	fpsgo_notify_nn_job_end_fp(msgKM->pid, msgKM->tid, msgKM->mid,
			msgKM->num_step, boost, device, exec_time);

	msgKM->priority = fpsgo_get_nn_priority_fp(msgKM->pid, msgKM->mid);

	perfctl_copy_to_user(msgUM, msgKM, sizeof(struct _EARA_NN_PACKAGE));
	return;

out_exec_time_malloc_fail:
	kfree(device);
out_device_malloc_fail:
	kfree(boost);
out_boost_malloc_fail:
out_um_malloc_fail:
	fpsgo_notify_nn_job_end_fp(msgKM->pid, msgKM->tid, msgKM->mid,
			msgKM->num_step, NULL, NULL, NULL);
}

/*--------------------DEV OP------------------------*/
static int eara_show(struct seq_file *m, void *v)
{
	return 0;
}

static int eara_open(struct inode *inode, struct file *file)
{
	return single_open(file, eara_show, inode->i_private);
}

static long eara_ioctl_impl(struct file *filp,
		unsigned int cmd, unsigned long arg, void *pKM)
{
	ssize_t ret = 0;
	struct _EARA_NN_PACKAGE *msgKM = NULL,
		*msgUM = (struct _EARA_NN_PACKAGE *)arg;
	struct _EARA_NN_PACKAGE smsgKM;

	msgKM = (struct _EARA_NN_PACKAGE *)pKM;
	if (!msgKM) {
		msgKM = &smsgKM;
		if (perfctl_copy_from_user(msgKM, msgUM,
				sizeof(struct _EARA_NN_PACKAGE))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}
	}

	switch (cmd) {
	case EARA_NN_BEGIN:
		perfctl_notify_fpsgo_nn_begin(msgKM, msgUM);
		break;
	case EARA_NN_END:
		perfctl_notify_fpsgo_nn_end(msgKM, msgUM);
		break;
	case EARA_GETUSAGE:
		msgKM->bw_usage = 0;

		if (rsu_getusage_fp)
			rsu_getusage_fp(&msgKM->dev_usage, &msgKM->bw_usage,
					msgKM->pid);
		else
			msgKM->dev_usage = 0;

		perfctl_copy_to_user(msgUM, msgKM,
				sizeof(struct _EARA_NN_PACKAGE));

		break;
	case EARA_GETSTATE:
		if (rsu_getstate_fp)
			rsu_getstate_fp(&msgKM->thrm_throttled);
		else
			msgKM->thrm_throttled = -1;

		perfctl_copy_to_user(msgUM, msgKM,
				sizeof(struct _EARA_NN_PACKAGE));

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

static long eara_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	return eara_ioctl_impl(filp, cmd, arg, NULL);
}

static long eara_compat_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	int ret = -EFAULT;
	struct _EARA_NN_PACKAGE_32 {
		__u32 pid;
		__u32 tid;
		__u64 mid;
		__s32 errorno;
		__s32 priority;
		__s32 num_step;

		__s32 dev_usage;
		__u32 bw_usage;
		__s32 thrm_throttled;

		union {
			__u32 device;
			__u64 p_dummy_device;
		};
		union {
			__u32 boost;
			__u64 p_dummy_boost;
		};
		union {
			__u32 exec_time;
			__u64 p_dummy_exec_time;
		};
		union {
			__u32 target_time;
			__u64 p_dummy_target_time;
		};
	};
	struct _EARA_NN_PACKAGE sEaraPackageKM64;
	struct _EARA_NN_PACKAGE_32 sEaraPackageKM32;
	struct _EARA_NN_PACKAGE_32 *psEaraPackageKM32 = &sEaraPackageKM32;
	struct _EARA_NN_PACKAGE_32 *psEaraPackageUM32 =
		(struct _EARA_NN_PACKAGE_32 *)arg;

	if (perfctl_copy_from_user(psEaraPackageKM32,
			psEaraPackageUM32, sizeof(struct _EARA_NN_PACKAGE_32)))
		goto unlock_and_return;

	sEaraPackageKM64 = *((struct _EARA_NN_PACKAGE *)psEaraPackageKM32);
	sEaraPackageKM64.device =
		(void *)((size_t) psEaraPackageKM32->device);
	sEaraPackageKM64.boost =
		(void *)((size_t) psEaraPackageKM32->boost);
	sEaraPackageKM64.exec_time =
		(void *)((size_t) psEaraPackageKM32->exec_time);
	sEaraPackageKM64.target_time =
		(void *)((size_t) psEaraPackageKM32->target_time);

	ret = eara_ioctl_impl(filp, cmd, arg, &sEaraPackageKM64);

unlock_and_return:
	return ret;
}

static const struct proc_ops eara_Fops = {
	.proc_ioctl = eara_ioctl,
	.proc_compat_ioctl = eara_compat_ioctl,
	.proc_open = eara_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/*--------------------SYNC------------------------*/
static int eas_show(struct seq_file *m, void *v)
{
	return 0;
}

static int eas_open(struct inode *inode, struct file *file)
{
	return single_open(file, eas_show, inode->i_private);
}

extern void set_wake_sync(unsigned int sync);
extern unsigned int get_wake_sync(void);
extern void set_uclamp_min_ls(unsigned int val);
extern unsigned int get_uclamp_min_ls(void);
#if IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
extern int sched_pause_cpu(int val);
extern int sched_resume_cpu(int val);
#endif

static long eas_ioctl_impl(struct file *filp,
		unsigned int cmd, unsigned long arg, void *pKM)
{
	ssize_t ret = 0;
	unsigned int sync;
	unsigned int val;
	unsigned int cpu;
	bool is_pause;
	char powerhal_str[20];
	char *ubuf = (char *)arg;
#if IS_ENABLED(CONFIG_MTK_CORE_CTL)
	unsigned int cid, min, max, thres, throttle_ms;
	bool enable, boost;
#endif

	switch (cmd) {
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
	case CORE_CTL_FORCE_RESUME_CPU:
	case CORE_CTL_FORCE_PAUSE_CPU:
		if (perfctl_copy_from_user(powerhal_str, ubuf, sizeof(char *)))
			return -1;
		if (sscanf(powerhal_str, "%u %u\n", &cpu, &is_pause) != 2)
			return -1;
#if IS_ENABLED(CONFIG_MTK_CORE_CTL)
		ret = core_ctl_force_pause_cpu(cpu, is_pause);
#elif IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
		if (is_pause)
			ret = sched_pause_cpu(cpu);
		else
			ret = sched_resume_cpu(cpu);
#else
		return -1;
#endif
		break;
#if IS_ENABLED(CONFIG_MTK_CORE_CTL)
	case CORE_CTL_SET_OFFLINE_THROTTLE_MS:
		if (perfctl_copy_from_user(powerhal_str, ubuf, sizeof(char *)))
			return -1;
		if (sscanf(powerhal_str, "%u %u\n", &cid, &throttle_ms) != 2)
			return -1;
		ret = core_ctl_set_offline_throttle_ms(cid, throttle_ms);
		break;
	case CORE_CTL_SET_LIMIT_CPUS:
		if (perfctl_copy_from_user(powerhal_str, ubuf, sizeof(char *)))
			return -1;
		if (sscanf(powerhal_str, "%u %u %u\n", &cid, &min, &max) != 3)
			return -1;
		ret = core_ctl_set_limit_cpus(cid, min, max);
		break;
	case CORE_CTL_SET_NOT_PREFERRED:
		if (perfctl_copy_from_user(powerhal_str, ubuf, sizeof(char *)))
			return -1;
		if (sscanf(powerhal_str, "%u %u %u\n", &cid, &cpu, &enable) != 3)
			return -1;
		ret = core_ctl_set_not_preferred(cid, cpu, enable);
		break;
	case CORE_CTL_SET_BOOST:
		if (perfctl_copy_from_user(powerhal_str, ubuf, sizeof(char *)))
			return -1;
		if (sscanf(powerhal_str, "%u\n", &boost) != 1)
			return -1;
		ret = core_ctl_set_boost(boost);
		break;
	case CORE_CTL_SET_UP_THRES:
		if (perfctl_copy_from_user(powerhal_str, ubuf, sizeof(char *)))
			return -1;
		if (sscanf(powerhal_str, "%u %u\n", &cid, &thres) != 2)
			return -1;
		ret = core_ctl_set_up_thres(cid, thres);
		break;
	case CORE_CTL_ENABLE_POLICY:
		if (perfctl_copy_from_user(powerhal_str, ubuf, sizeof(char *)))
			return -1;
		if (sscanf(powerhal_str, "%u\n", &enable) != 1)
			return -1;
		ret = core_ctl_enable_policy(enable);
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
	int pwr_cmd = -1, value1 = -1, value2 = -1;
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
		if (fpsgo_get_fps_fp)
			msgKM->fps = fpsgo_get_fps_fp();
		else
			ret = -1;
		perfctl_copy_to_user(msgUM, msgKM,
				sizeof(struct _FPSGO_PACKAGE));
		break;
	case FPSGO_GET_CMD:
		if (fpsgo_get_fps_fp) {
			fpsgo_get_cmd_fp(&pwr_cmd, &value1, &value2);
			msgKM->cmd = pwr_cmd;
			msgKM->value1 = value1;
			msgKM->value2 = value2;
		}
		else
			ret = -1;
		perfctl_copy_to_user(msgUM, msgKM,
				sizeof(struct _FPSGO_PACKAGE));
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

	pe = proc_create("eara_ioctl", 0664, parent, &eara_Fops);
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
