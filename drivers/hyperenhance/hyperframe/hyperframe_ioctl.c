// MIUI ADD: Performance_FramePredictBoost
#include <linux/workqueue.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include "hyperframe.h"

void (*hyperframe_notify_doframe_fp)(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time);
void (*hyperframe_notify_recordview_fp)(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time);
void (*hyperframe_notify_drawframes_fp)(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time);
void (*hyperframe_notify_dequeue_fp)(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time);
void (*hyperframe_notify_queue_fp)(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time);
void (*hyperframe_notify_gpu_fp)(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time);
void (*hyperframe_notify_vsync_fp)(unsigned long long time);
void (*hyperframe_notify_id_fp)(unsigned int pid, unsigned int tid, unsigned int is_focus);

static struct proc_dir_entry *hyperframe_dir;

static long hyperframe_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;
	struct _FRAME_PACKAGE *msgKM = NULL,
			*msgUM = (struct _FRAME_PACKAGE *)arg;
	struct _FRAME_PACKAGE smsgKM;

	msgKM = &smsgKM;
	if (__copy_from_user(msgKM, msgUM, sizeof(struct _FRAME_PACKAGE))) {
		ret = -EFAULT;
		return -EINVAL;
	}

	switch (cmd) {
	case HYPER_FRAME_DOFRAME:
		if (hyperframe_notify_doframe_fp)
			hyperframe_notify_doframe_fp(msgKM->pid, msgKM->vsync_id, msgKM->start, msgKM->time_stamp);
		break;
	case HYPER_FRAME_RECORDVIEW:
		if (hyperframe_notify_recordview_fp)
			hyperframe_notify_recordview_fp(msgKM->pid, msgKM->vsync_id, msgKM->start, msgKM->time_stamp);
		break;
	case HYPER_FRAME_DRAWFRAMES:
		if (hyperframe_notify_drawframes_fp) {
			hyperframe_notify_drawframes_fp(msgKM->pid, msgKM->vsync_id, msgKM->start, msgKM->time_stamp);
		}
		break;
	case HYPER_FRAME_DEQUEUE:
		// if (hyperframe_notify_dequeue_fp) {
		// 	hyperframe_notify_dequeue_fp(msgKM->pid);
		// }
		break;
	case HYPER_FRAME_QUEUE:
		// if (hyperframe_notify_queue_fp) {
		// 	hyperframe_notify_queue_fp(msgKM->pid);
		// }
		break;
	case HYPER_FRAME_GPU:
		// if (hyperframe_notify_gpu_fp) {
		// 	hyperframe_notify_gpu_fp(msgKM->pid);
		// }
		break;
	case HYPER_FRAME_VSYNC:
		if (hyperframe_notify_vsync_fp)
			hyperframe_notify_vsync_fp(msgKM->time_stamp);
		break;
	case HYPER_FRAME_ID:
		if (hyperframe_notify_id_fp)
			hyperframe_notify_id_fp(msgKM->pid, msgKM->tid, msgKM->is_focus);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct proc_ops Fops = {
	.proc_ioctl = hyperframe_ioctl,
	.proc_compat_ioctl = hyperframe_ioctl,
	// .proc_open = single_open,
	// .proc_read = seq_read,
	// .proc_lseek = seq_lseek,
	// .proc_release = single_release,
};

int framectl_init(void)
{
	hyperframe_dir = proc_mkdir(HYPERFRAME_DIR, NULL);
	if (!hyperframe_dir) {
		pr_err("Error creating /proc/%s\n", HYPERFRAME_DIR);
		return -ENOMEM;
	}

	if (!proc_create(HYPERFRAME_IOCTL, 0666, hyperframe_dir, &Fops)) {
		pr_err("Error creating /proc/%s/%s\n", HYPERFRAME_DIR, HYPERFRAME_IOCTL);
		remove_proc_entry(HYPERFRAME_DIR, NULL);
		return -ENOMEM;
	}
	pr_info("hyperframe module loaded\n");

	return 0;
}

int framectl_exit(void)
{
	if (hyperframe_dir == NULL) {
		pr_err("framectl_exit hyperframe_dir NULL\n");
	} else {
		remove_proc_entry(HYPERFRAME_IOCTL, hyperframe_dir);
		remove_proc_entry(HYPERFRAME_DIR, NULL);
	}

	pr_info("hyperframe module unloaded\n");
	return 0;
}
// END Performance_FramePredictBoost