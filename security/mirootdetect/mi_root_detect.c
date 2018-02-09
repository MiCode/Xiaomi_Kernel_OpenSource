#define DEBUG

#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/kallsyms.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/utsname.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/genhd.h>
#include <linux/kobject.h>
#include <linux/module.h>

#include <linux/mi_root_detect.h>

#define ROOT_COUNT_LIMIT	20
#define ROOT_DETECT_NETLINK	28
#define NETLINK_PORT		28

struct root_record {
	char process_name[TASK_COMM_LEN];
	char parent_name[TASK_COMM_LEN];
	int is_pgl;
	int uid;
	int euid;
	int sid;
	int cap;
};

struct work_struct notify_work;

/*
 * CAP_KILL 5
 */
static struct root_record kill_process_table[] = {
	{"system_server", "main", 0, 1000, 1000, 0},
	{"", "", 0, 0, 0, 0},
};

/*
 * CAP_MODULE 16
 */
static struct root_record sys_module_table[] = {
	{"tc", "netmgrd", 0, 0, 0, 0},
	{"sdcard", "vold", 0, 0, 0, 0},
	{"system_server", "main", 0, 1000, 1000, 0},
	{"", "", 0, 0, 0, 0},
};

/*
 * CAP_ADMIN 21
 */
static struct root_record sys_admin_table[] = {
	{"init", "init", 0, 0, 0, 0},
	{"main", "init", 1, 0, 0, 0},
	{"rmt_storage", "init", 1, 0, 0, 0},
	{"main", "main", 0, 0, 0, 0},
	{"qseecomd", "init", 1, 0, 0, 0},
	{"qseecomd", "qseecomd", 0, 1000, 1000, 0},
	{"vold", "init", 1, 0, 0, 0},
	{"sdcard", "vold", 0, 0, 0, 0},
	{"vold", "vold", 0, 0, 0, 0},
	{"", "", 0, 0, 0, 0},
};

/*
 * CAP_PTRACE 19
 */
static struct root_record sys_ptrace_table[] = {
	{"system_server", "main", 0, 1000, 1000, 0},
	{"ps", "mcd", 0, 0, 0, 0},
	{"vold", "init", 1, 0, 0, 0},
	{"", "", 0, 0, 0, 0},
};

/*
 * CAP_SETCAP 8
 */
static struct root_record setpcap_table[] = {
	{"dpmd", "dpmd", 0, 0, 0, 0},
	{"cnd", "init", 1, 0, 0, 0},
	{"init", "init", 0, 0, 0, 0},
	{"", "", 0, 0, 0, 0},
};

/*
 * SET UID -1
 */
static struct root_record set_uid_table[] = {
	{"main", "init", 1, 0, 9999, 0},
	{"dpmd", "dpmd", 0, 0, 0, 0},
	{"", "", 0, 0, 0, 0},
};

static struct kobject *mirootdetect_kobj;
/* if current is root */
static int currentStatus;
/* how many root info has saved */
static int recordCounter;
static struct root_record currentRootInfo;
/* root info */
static struct root_record historyRootInfo[ROOT_COUNT_LIMIT];

/*
 * inspect if the process is illegal
 * legal -> 0
 * illegal -> 1
 */
static int inspect_root_capability(struct root_record *table,
				struct task_struct *task)
{
	int i = 0;
	int flag;
	kuid_t tmp_uid, tmp_euid;
	/* skip Binder */
	if (!strncmp(current->group_leader->comm, "Binder", 5))
		return 0;

	for (i = 0; table[i].process_name[0]; i++) {
		if (!strncmp(task->group_leader->comm,
			table[i].process_name, TASK_COMM_LEN) &&
			!strncmp(task->real_parent->group_leader->comm,
			table[i].parent_name, TASK_COMM_LEN)) {

			if (!strncmp(table[i].parent_name, "init", 5)) {
				if (task->real_parent->group_leader->pid != 1)
					return 1;
			}
			flag = task_pgrp_vnr(task) == task->tgid ? 1 : 0;
			tmp_uid = task_uid(task);
			tmp_euid = task_euid(task);
			if (flag == table[i].is_pgl &&
				tmp_uid.val == table[i].uid &&
				tmp_euid.val == table[i].euid) {

				if (task_session_vnr(task) == table[i].sid)
					return 0;
			}
		}
	}
	return 1;
}

/*
 * inspect if the process is illegal
 * legal -> 0
 * illegal -> 1
 */
int inspect_illegal_root_capability(int cap)
{
	int result;

	/* the current pointer refers to the user process currently executing */
	if (current->mm == NULL)
		return 0;
	/* processID */
	if (current->pid == 1)
		return 0;
	if (cap == CAP_KILL)
		result = inspect_root_capability(kill_process_table, current);
	else if (cap == CAP_SYS_MODULE)
		result = inspect_root_capability(sys_module_table, current);
	else if (cap == CAP_SYS_ADMIN)
		result = inspect_root_capability(sys_admin_table, current);
	else if (cap == CAP_SYS_PTRACE)
		result = inspect_root_capability(sys_ptrace_table, current);
	else if (cap == CAP_SETPCAP)
		result = inspect_root_capability(setpcap_table, current);
	else if (cap == -1)
		result = inspect_root_capability(set_uid_table, current);
	else
		result = 0;
	return result;
}

/*
 * compare if record is exist
 * equal -> 0
 * not equal -> 1
 */
static int record_compare(struct root_record a, struct root_record b)
{
	return (!strncmp(a.process_name, b.process_name, TASK_COMM_LEN)
			&& !strncmp(a.parent_name, b.parent_name, TASK_COMM_LEN)
			&& a.is_pgl == b.is_pgl
			&& a.uid == b.uid
			&& a.euid == b.euid
			&& a.sid == b.sid
			&& a.cap == b.cap)?0:1;
}

/*
 * save record
 */
void record_illegal_root(int cap)
{
	kuid_t tmp_uid, tmp_euid;
	int temp;
	/* check the limit */
	if (recordCounter >= ROOT_COUNT_LIMIT)
		return;
	/* get current info */
	tmp_uid = task_uid(current);
	tmp_euid = task_euid(current);
	/* set current info */
	strlcpy(currentRootInfo.process_name,
		current->group_leader->comm,
		TASK_COMM_LEN);
	strlcpy(currentRootInfo.parent_name,
		current->real_parent->group_leader->comm,
		TASK_COMM_LEN);
	currentRootInfo.is_pgl = (task_pgrp_vnr(current) ==
				current->tgid ? 1 : 0);
	currentRootInfo.uid = tmp_uid.val;
	currentRootInfo.euid = tmp_euid.val;
	currentRootInfo.sid = task_session_vnr(current);
	currentRootInfo.cap = cap;
	/* check if this record exists */
	for (temp = 0; temp < recordCounter; temp++) {
		if (record_compare(currentRootInfo,
				historyRootInfo[temp]) == 0)
			return;
	}
	/* save this record */
	strlcpy(historyRootInfo[recordCounter].process_name,
				currentRootInfo.process_name,
				TASK_COMM_LEN);
	strlcpy(historyRootInfo[recordCounter].parent_name,
				currentRootInfo.parent_name,
				TASK_COMM_LEN);
	historyRootInfo[recordCounter].is_pgl = currentRootInfo.is_pgl;
	historyRootInfo[recordCounter].uid = currentRootInfo.uid;
	historyRootInfo[recordCounter].euid = currentRootInfo.euid;
	historyRootInfo[recordCounter].sid = currentRootInfo.sid;
	historyRootInfo[recordCounter].cap = currentRootInfo.cap;
	/* increase counter and change rootstatus*/
	currentStatus = 1;
	recordCounter++;
	schedule_work(&notify_work);
	pr_debug("[miRootDetect]append new record:%s|%s, Current record counter = %d\n",
		currentRootInfo.process_name,
		currentRootInfo.parent_name,
		recordCounter);
}

static void do_notify(struct work_struct *work)
{
	pr_debug("[miRootDetect]send notify");
	sysfs_notify(mirootdetect_kobj, NULL, "rootdetectstatus");
}

static ssize_t get_rootdetectstatus(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;

	pr_debug("[miRootDetect]get root status = %d, record counter = %d\n",
		currentStatus,
		recordCounter);
	/* copy current info */
	memcpy(buf, &currentStatus, sizeof(int));
	memcpy(buf + sizeof(int), &recordCounter, sizeof(int));
	memcpy(buf + sizeof(int) * 2,
		historyRootInfo,
		sizeof(struct root_record) * ROOT_COUNT_LIMIT);
	/* set return size */
	ret = sizeof(int) * 2 + sizeof(struct root_record) * ROOT_COUNT_LIMIT;
	return ret;
}

static DEVICE_ATTR(rootdetectstatus, 0444, get_rootdetectstatus, NULL);

static struct attribute *root_detect_status_fs_attrs[] = {
	&dev_attr_rootdetectstatus.attr,
	NULL,
};

static struct attribute_group root_detect_status_attrs_group = {
	.attrs = (struct attribute **)root_detect_status_fs_attrs,
};

static int __init mi_root_detect_init(void)
{
	int ret;

	mirootdetect_kobj = kobject_create_and_add("rootdetect", NULL);
	if (!mirootdetect_kobj) {
		pr_debug_once("[miRootDetect]init error! create kobject failed\n");
		return -ENOMEM;
	}
	ret = sysfs_create_group(mirootdetect_kobj,
				&root_detect_status_attrs_group);
	if (ret) {
		pr_debug_once("[miRootDetect]init error! create sysfs failed\n");
		kobject_put(mirootdetect_kobj);
		return ret;
	}
	/* Init control parameter */
	currentStatus = 0;
	recordCounter = 0;
	memset(&currentRootInfo, 0x0, sizeof(struct root_record));
	memset(historyRootInfo,
		0x0,
		sizeof(struct root_record) * ROOT_COUNT_LIMIT);
	pr_debug_once("[miRootDetect]init success!\n");

	INIT_WORK(&notify_work, do_notify);
	return ret;
}
static void __exit mi_root_detect_exit(void)
{
	sysfs_remove_group(mirootdetect_kobj, &root_detect_status_attrs_group);
}

module_init(mi_root_detect_init);
module_exit(mi_root_detect_exit);
