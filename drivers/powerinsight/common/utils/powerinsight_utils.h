#ifndef POWERINSIGHT_UTILS_H
#define POWERINSIGHT_UTILS_H

#include <linux/sched.h>
#include <linux/uaccess.h>

#include <linux/printk.h>

#define POWERINSIGHT_TAG				"POWERINSIGHT"
#define powerinsight_debug(fmt, ...)	pr_debug("[" POWERINSIGHT_TAG "][D] %s: " fmt "\n", __func__, ##__VA_ARGS__)
#define powerinsight_info(fmt, ...)	pr_info("[" POWERINSIGHT_TAG "][I] %s: " fmt "\n", __func__, ##__VA_ARGS__)
#define powerinsight_err(fmt, ...)		pr_err("[" POWERINSIGHT_TAG "][E] %s: " fmt "\n", __func__, ##__VA_ARGS__)
#define powerinsight_warn(fmt, ...)	pr_warn("[" POWERINSIGHT_TAG "][W] %s: " fmt "\n", __func__, ##__VA_ARGS__)

// keep CMDLINE_LEN as 128 bytes
#define CMDLINE_LEN				128
#define MAX_PID_NUM				5
#define MAX_ALLOC_MEM_LENGTH	0x100000

struct dev_transmit_t {
	int length;
	char data[0];
} __packed;

struct proc_cmdline {
	uid_t uid;
	pid_t pid;
	char comm[TASK_COMM_LEN];
	char cmdline[CMDLINE_LEN];
} __packed;

struct uid_name_to_pid {
	uid_t uid;
	pid_t pid[MAX_PID_NUM];
	char name[CMDLINE_LEN];
} __packed;

long powerinsight_ioctl_utils(unsigned int cmd, void __user *argp);
int powerinsight_get_cmdline(struct task_struct *task, char *cmdline, size_t size);
int powerinsight_get_cmdline_by_uid_pid(uid_t uid, pid_t pid, char *buffer, size_t size);
struct dev_transmit_t *powerinsight_alloc_transmit(size_t data_size);
size_t powerinsight_get_transmit_size(struct dev_transmit_t *transmit);
void powerinsight_free_transmit(void *transmit);

static inline int get_enable_value(void __user *argp, int *enable)
{
	uint8_t value;

	if (copy_from_user(&value, argp, sizeof(uint8_t)))
		return -EFAULT;

	if (value != 1 && value != 0) {
		powerinsight_err("Invalid enable value: %u", value);
		return -EFAULT;
	}
	*enable = (value == 1);

	return 0;
}

static inline int get_timestamp_value(void __user *argp, long long *timestamp)
{
	if (copy_from_user(timestamp, argp, sizeof(long long)))
		return -EFAULT;

	return 0;
}

#endif // POWERINSIGHT_UTILS_H
