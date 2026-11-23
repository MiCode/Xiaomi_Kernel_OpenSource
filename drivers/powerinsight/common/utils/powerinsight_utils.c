#include "powerinsight_utils.h"

#include <linux/cred.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched/signal.h>

#include "../powerinsight_ioctl.h"

#define IOC_PROC_CMDLINE_GET POWERINSIGHT_UTILS_DIR_IOC(_IOC_WRITE, 1, struct proc_cmdline)
#define IOC_RSS_GET POWERINSIGHT_UTILS_DIR_IOC(_IOC_READ, 2, unsigned long long)
#define IOC_PIDS_GET POWERINSIGHT_UTILS_DIR_IOC(_IOC_READ|_IOC_WRITE, 3, struct uid_name_to_pid)

static int powerinsight_get_proc_cmdline(void __user *argp);
static int powerinsight_get_rss(void __user *argp);
static int powerinsight_get_pids_by_uid_name(void __user *argp);

long powerinsight_ioctl_utils(unsigned int cmd, void __user *argp)
{
    int rc = 0;

    switch (cmd) {
        case IOC_PROC_CMDLINE_GET:
            rc = powerinsight_get_proc_cmdline(argp);
            break;
        case IOC_RSS_GET:
            rc = powerinsight_get_rss(argp);
            break;
        case IOC_PIDS_GET:
            rc = powerinsight_get_pids_by_uid_name(argp);
            break;
        default:
            powerinsight_err("Invalid ioctl command: 0x%x", cmd);
            rc = -ENOTTY;
    }

    return rc;
}

static int powerinsight_get_rss(void __user *argp)
{
	unsigned long rss = get_mm_rss(current->mm);
	unsigned long long size = rss * PAGE_SIZE;

	if (copy_to_user(argp, &size, sizeof(size))) {
		powerinsight_err("Failed to copy_to_user %llu.", size);
		return -EINVAL;
	}

	return 0;
}

/*
 * powerinsight_get_task_cmdline - get task cmdline
 * @struct task_struct *task: The task to get cmdline
 * @char *cmdline: Buffer of cmdline
 * @size_t size: The buffer size, must be equal or bigger than CMDLINE_LEN
 */
int powerinsight_get_cmdline(struct task_struct *task, char *cmdline, size_t size)
{
    int res = 0;
    unsigned int len;
    struct mm_struct *mm;
    unsigned long arg_start, arg_end, env_start, env_end;

    if (!task || !cmdline || size <= 0)
        return -EINVAL;

    mm = get_task_mm(task);
    if (!mm)
        goto out;

    /* 如果参数未初始化则跳过 */
    if (!mm->arg_end)
        goto out_mm;

    /* 锁定 mm->arg_lock 以安全读取参数边界 */
    spin_lock(&mm->arg_lock);
    arg_start = mm->arg_start;
    arg_end = mm->arg_end;
    env_start = mm->env_start;
    env_end = mm->env_end;
    spin_unlock(&mm->arg_lock);

    /* 计算参数长度 */
    len = arg_end - arg_start;
    if (len > size)
        len = size;

    /* 从用户空间读取参数 */
    res = access_process_vm(task, arg_start, cmdline, len, FOLL_FORCE);
    if (res <= 0)
        goto out_mm;

    /* 处理未正确终止的字符串 */
    if (cmdline[res-1] != '\0' && len < size) {
        /* 尝试查找实际字符串长度 */
        len = strnlen(cmdline, res);
        if (len < res) {
            res = len;
        } else {
            /* 检查环境变量作为备选 */
            len = env_end - env_start;
            if (len > size - res)
                len = size - res;
            
            res += access_process_vm(task, env_start, 
				cmdline + res, len, 
                                    FOLL_FORCE);
            res = strnlen(cmdline, res);
        }
    }

out_mm:
    mmput(mm);
out:
    return res;
}

static int __powerinsight_get_proc_cmdline(uid_t uid, pid_t pid, const char *comm, char *buffer, size_t size)
{
	int ret = 0;
	struct task_struct *task = NULL;
	bool cmp_comm = false, cmp_leader = false, cmp_cmdline = false;
	char cmdline[CMDLINE_LEN] = {0};

	if (comm == NULL || buffer == NULL || size < CMDLINE_LEN) {
		powerinsight_err("Invalid parameter");
		return -EINVAL;
	}

	rcu_read_lock();
	task = find_task_by_vpid(pid);
	if (task)
		get_task_struct(task);
	rcu_read_unlock();
	if (!task)
		return -EFAULT;

	if (uid != from_kuid_munged(current_user_ns(), task_uid(task))) {
		powerinsight_err("Mismatched uid");
		ret = -EFAULT;
		goto exit;
	}
	ret = powerinsight_get_cmdline(task, cmdline, CMDLINE_LEN);
	if (ret == 0) {
		// compare the command
		if (strlen(comm) > 0) {
			cmp_comm = strncmp(comm, task->comm, TASK_COMM_LEN - 1) == 0;
			if (task->group_leader != NULL)
				cmp_leader = strncmp(comm, task->group_leader->comm, TASK_COMM_LEN - 1) == 0;
			cmp_cmdline = strstr(cmdline, comm) != NULL;
			if (!cmp_comm && !cmp_leader && !cmp_cmdline) {
				ret = -EFAULT;
				goto exit;
			}
		}
		strncpy(buffer, cmdline, size - 1);
	}

exit:
	put_task_struct(task);

	return ret;
}

int powerinsight_get_cmdline_by_uid_pid(uid_t uid, pid_t pid, char *buffer, size_t size)
{
	char comm[TASK_COMM_LEN] = {0};

	return __powerinsight_get_proc_cmdline(uid, pid, comm, buffer, size);
}

static int powerinsight_get_proc_cmdline(void __user *argp)
{
	struct proc_cmdline proc_cmd;

	if (copy_from_user(&proc_cmd, argp, sizeof(struct proc_cmdline)))
		return -EFAULT;

	proc_cmd.comm[TASK_COMM_LEN - 1] = '\0';
	proc_cmd.cmdline[CMDLINE_LEN - 1] = '\0';
	__powerinsight_get_proc_cmdline(proc_cmd.uid, proc_cmd.pid, proc_cmd.comm, proc_cmd.cmdline, CMDLINE_LEN);
	if (copy_to_user(argp, &proc_cmd, sizeof(struct proc_cmdline)))
		return -EFAULT;

	return 0;
}

static int powerinsight_get_pids_by_uid_name(void __user *argp)
{
    uid_t uid;
    int count = 0;
    struct uid_name_to_pid data;
    struct task_struct *process, *task;

    if (copy_from_user(&data, argp, sizeof(struct uid_name_to_pid)))
        return -EFAULT;

    data.name[CMDLINE_LEN - 1] = '\0';
    if (!strlen(data.name)) {
        powerinsight_err("Invalid parameter");
        return -EINVAL;
    }

    rcu_read_lock();
    for_each_process_thread(process, task) {
        if (count >= MAX_PID_NUM)
            break;
        uid = from_kuid_munged(current_user_ns(), task_uid(task));
        if (uid == data.uid && strstr(data.name, task->comm)) {
            data.pid[count] = task->pid;
            count++;
        }
    }
    rcu_read_unlock();

    if (copy_to_user(argp, &data, sizeof(struct uid_name_to_pid)))
        return -EFAULT;

    return 0;
}

struct dev_transmit_t *powerinsight_alloc_transmit(size_t data_size)
{
	int total_size;
	struct dev_transmit_t *transmit = NULL;

	if (data_size > MAX_ALLOC_MEM_LENGTH) {
		powerinsight_err("Invalid length %d", (int)data_size);
		return NULL;
	}
	total_size = sizeof(struct dev_transmit_t) + data_size;
	transmit = kzalloc(total_size, GFP_KERNEL);
	if (transmit == NULL)
		return NULL;
	transmit->length = data_size;

	return transmit;
}

size_t powerinsight_get_transmit_size(struct dev_transmit_t *transmit)
{
	if (transmit == NULL)
		return 0;

	return sizeof(struct dev_transmit_t) + transmit->length;
}

void powerinsight_free_transmit(void *transmit)
{
	kfree(transmit);
	transmit = NULL;
}

