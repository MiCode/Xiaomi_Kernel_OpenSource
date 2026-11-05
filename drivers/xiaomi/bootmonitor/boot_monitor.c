#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "boot_monitor.h"
#include "../boottime/boottime.h"
#include "boot_fail.h"

static unsigned long log_count_2;
static struct log_t **bootprof_2;
static struct task_struct *monitor_main = NULL;
static int count =0;
static int ringnums = 0;
static char prof_node[RPOC_ENTRY_LINE] = {0};
static struct proc_dir_entry *bootmonitor_entry;
struct bootmonitor_context boot_cxt;
int bm_event_counts;
int first_boot = 0;
char *bm_write_buffer = NULL;

char bm_boot_mode[10] = {0};
module_param_string(bootmode, bm_boot_mode, 10, 0644);

//#define MAX_CMDLINE_PARAM_LEN 256
char build_fingerprint[MAX_CMDLINE_PARAM_LEN] = {0};
module_param_string(fingerprint, build_fingerprint, MAX_CMDLINE_PARAM_LEN,0644);

static int blackbox = 0;
module_param(blackbox, int, 0600);
MODULE_PARM_DESC(blackbox,
		"blackbox (default 0)");

struct type_map bm_boot_events[] = {
	{5,  8,   "early-init"},
	{3,  8,   "late-init"},
	{5,  8,   "late-fs"},
	{12, 20,  "zygote-start"},
	{8,  24,  "zygote_preload_end"},
	{3,  12,  "start_android"},
	{6,  12,  "pms_start"},
	{18, 24,  "pms_ready"},
	{30, 40,  "ams_ready"},
	{8,  24,  "boot_completed"},
};

static void exit_boot_monitor(struct bootmonitor_context *cxt) {
	MTN_PRINT_START();
	/*add bdev == NULL ,because bdev no init*/
	if (IS_ERR(bdev) || bdev == NULL) {
		MTN_PRINT_INFO("%s-%d:bdev no init\n", __func__, __LINE__);
	} else {
		sync_blockdev(bdev);
		mutex_destroy(&write_bm_mutex);
		invalidate_mapping_pages(bdev->bd_inode->i_mapping, 0, -1);
		blkdev_put(bdev, NULL);
		MTN_PRINT_INFO("%s-%d:invalidate_mapping_pages end\n", __func__, __LINE__);
	}
	if (bm_write_buffer) {
		vfree(bm_write_buffer);
	}
	if (cxt->oops_buf) {
		vfree(cxt->oops_buf);
	}
	/*release netlink*/
	//bm_netlink_exit();
	platform_driver_unregister(&boot_monitor_driver);
	monitor_main = NULL;
	MTN_PRINT_END();
	return;
}

static bool find_event(int nums) {
	struct log_t *p;
	char *occurrence = NULL;
	char *firstboot = NULL;
	int i;
	spin_lock(&bootprof_lock);
	log_count_2 = get_log_count();
	bootprof_2 = get_bootprof_pointer();
	for (i = ringnums; i < log_count_2; i++) {
        count ++;
		p = &bootprof_2[i / LOGS_PER_BUF][i % LOGS_PER_BUF];
		if (!p->comm_event)
			continue;
		/*first boot*/
		firstboot = strstr(p->comm_event + TASK_COMM_LEN, FIRST_BOOT);
		if (firstboot) {
			MTN_PRINT_INFO("%s-%d:first boot\n", __func__, __LINE__);
			first_boot = 1;
		}
		/*p->comm_event + TASK_COMM_LEN = event*/
        occurrence = strstr(p->comm_event + TASK_COMM_LEN, bm_boot_events[nums].name);
        if (!occurrence) {
            continue;
        } else {
            MTN_PRINT_INFO("%s done\n", bm_boot_events[nums].name);
			spin_unlock(&bootprof_lock);
			ringnums = i;
            return true;
        }
	}
	ringnums = i;
	spin_unlock(&bootprof_lock);
	return false;
}

static int write_log_to_dev(struct bootmonitor_context *cxt, char * buffer, int event) {
	int err;

	err = -1;
	if (cxt->oops_buf == NULL || buffer == NULL) {
		MTN_PRINT_ERR("%s-%d:cxt->oops_buf or buffer is NULL\n", __func__, __LINE__);
		return err;
	}
	/*erase blackbox-bootfail-log-partition*/
	err = partition_bm_write(PARTITION_KMSG_OFFSET + HEADER_SIZE_4K, PARTITION_KMSG_SIZE - HEADER_SIZE_4K, buffer);
	if (err != 0) {
		MTN_PRINT_ERR("%s-%d:partition_bm_write error\n", __func__, __LINE__);
		return err;
	}
	err = partition_bm_write(PARTITION_PMSG_OFFSET, PARTITION_PMSG_SIZE, cxt->oops_buf);
	if (err != 0) {
		MTN_PRINT_ERR("%s-%d:partition_bm_write error\n", __func__, __LINE__);
		return err;
	}
	/*get kernel log and logcat*/
	monitor_get_kmsg(buffer);
	monitor_get_pmsg(cxt);
	/*init and update header*/
	err = write_blackbox_header(cxt, buffer, event);
	if (err != 0) {
		MTN_PRINT_ERR("%s-%d:write_blackbox_header error\n", __func__, __LINE__);
		return err;
	}
	/*record log in blackbox-bootfail-partition*/
	err = partition_bm_write(PARTITION_KMSG_OFFSET + HEADER_SIZE_4K, PARTITION_KMSG_SIZE - HEADER_SIZE_4K, buffer);
	if (err != 0) {
		MTN_PRINT_ERR("%s-%d:partition_bm_write error\n", __func__, __LINE__);
		return err;
	}
	err = partition_bm_write(PARTITION_PMSG_OFFSET, PARTITION_PMSG_SIZE, cxt->oops_buf);
	if (err != 0) {
		MTN_PRINT_ERR("%s-%d:partition_bm_write error\n", __func__, __LINE__);
		return err;
	}
	return err;
}

static int monitor_main_thread_body(void *data) {
	bool ret = false;
	int k;
	int err;
	int counts;
	int boottime;
	char bootwarn[RPOC_ENTRY_LINE] = "warning:";
	char booterror[RPOC_ENTRY_LINE] = "error:";
	struct bootmonitor_context *cxt;

	cxt = &boot_cxt;
	boottime = DEFAULT_TIMEOUT - 1;
	MTN_PRINT_START();
	while (!kthread_should_stop()) {
		for(bm_event_counts = 0; bm_event_counts < countof(bm_boot_events); bm_event_counts ++) {
			counts = bm_boot_events[bm_event_counts].warntime + 1;
			MTN_PRINT_INFO("counts = bm_boot_events[%d].warntime = %d\n", bm_event_counts, bm_boot_events[bm_event_counts].warntime);
			for(k = 0; k < counts; k ++) {
				set_current_state(TASK_INTERRUPTIBLE);
				ret = find_event(bm_event_counts);
				/*wait 1s*/
				if(!ret && k != (counts - 1)) {
					schedule_timeout(msecs_to_jiffies(1000));
					boottime ++;
					continue;
				} else if (!ret && k == bm_boot_events[bm_event_counts].warntime) {
					strncat(bootwarn, bm_boot_events[bm_event_counts].name, sizeof(bm_boot_events[bm_event_counts].name));
					//bm_sendnlmsg(bootwarn);
					/*reset bootwarn*/
					memset(bootwarn, 0, sizeof(bootwarn));
					snprintf(bootwarn, sizeof(bootwarn), "warning:");
					/*set counts to error time*/
					counts = bm_boot_events[bm_event_counts].errortimes + 1;
					MTN_PRINT_INFO("counts = bm_boot_events[%d].errortimes = %d\n", bm_event_counts, bm_boot_events[bm_event_counts].errortimes);
					/*if first boot, ams ready time to long*/
					if (first_boot == 1 && bm_event_counts == AMS_READY) {
						counts = AMS_READY_TIME;
						MTN_PRINT_INFO("first_boot:counts = %d\n", counts);
					}
					schedule_timeout(msecs_to_jiffies(1000));
					boottime ++;
					continue;
				} else if (boottime == DEFAULT_BOOT_TIME) {
					MTN_PRINT_INFO("%s timeout:boottime is %d line=%d\n", __func__, boottime,  __LINE__);
					break;
				} else {
					break;
				}
			}
			MTN_PRINT_INFO("%s-%d:set task to running\n", __func__,  __LINE__);
			set_current_state(TASK_RUNNING);
			if((k == bm_boot_events[bm_event_counts].errortimes || boottime == DEFAULT_BOOT_TIME) && !ret) {
				strncat(booterror, bm_boot_events[bm_event_counts].name, sizeof(bm_boot_events[bm_event_counts].name));
				strncpy(prof_node, booterror, sizeof(booterror));
				//bm_sendnlmsg(booterror);
				if(!get_bm_devices()) {
					err = write_log_to_dev(cxt, bm_write_buffer, bm_event_counts);
				}
				MTN_PRINT_INFO("error write end\n");
				exit_boot_monitor(cxt);
				return 0;
			}
		}
		strncpy(prof_node, BOOT_OK, sizeof(BOOT_OK));
		exit_boot_monitor(cxt);
		MTN_PRINT_END();
		return 0;
	}
	MTN_PRINT_END();
	return 0;
}

static ssize_t
boot_monitor_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[BOOT_MONITOR_SIZE];
	size_t copy_size = cnt;

	if (cnt >= sizeof(buf))
		copy_size = BOOT_MONITOR_SIZE - 1;

	if (copy_from_user(&buf, ubuf, copy_size))
		return -EFAULT;

	buf[copy_size] = 0;
	memset(prof_node, 0, sizeof(prof_node));
	strncpy(prof_node, buf, sizeof(buf));

	return cnt;
}

static int boot_monitor_show(struct seq_file *m, void *v)
{
	if (!m) {
		MTN_PRINT_ERR("seq_file is Null.\n");
		return 0;
	}
	seq_printf(m, "%s\n", prof_node);

	return 0;
}

static int boot_monitor_open(struct inode *inode, struct file *file)
{
	return single_open(file, boot_monitor_show, inode->i_private);
}

static const struct proc_ops boot_monitor_fops = {
	.proc_open = boot_monitor_open,
	.proc_write = boot_monitor_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init boot_monitor_init(void)
{
	int result;
	struct bootmonitor_context *cxt;

	cxt = &boot_cxt;
	/*boottime init ready?*/
	if (boottime_ok != 1) {
		MTN_PRINT_ERR("%s-%d:boottime no start\n", __func__, __LINE__);
		return 0;
	}
	/*check blackbox partition version_change, if blackbox is 1000, the size is 154M, no bootfail partition*/
	if (blackbox != BLACKBOX_VERSION) {
		MTN_PRINT_ERR("%s-%d:blackbox  is %d, BLACKBOX_VERSION is %d, no bootfail partition\n", __func__, __LINE__, blackbox, BLACKBOX_VERSION);
		return 0;
	}
	/*boot boot mode is normal?*/
    result = strcmp(BOOT_MODE_NORMAL, bm_boot_mode);
    if (result != 0) {
        MTN_PRINT_ERR("%s-%d:boot BOOT_MODE_NORMAL is %s,no need boot monitor\n", __func__, __LINE__, bm_boot_mode);
        return 0;
    }
	/*buufer for kmsg and pmsg*/
	bm_write_buffer = vzalloc(PARTITION_KMSG_SIZE - HEADER_SIZE_4K);
	if (!bm_write_buffer) {
		MTN_PRINT_ERR("%s-%d:bm_write_buffer vzalloc fail\n", __func__, __LINE__);
		return 0;
	}
	cxt->oops_buf = vzalloc(PARTITION_PMSG_SIZE);
	if (!cxt->oops_buf) {
		MTN_PRINT_ERR("%s-%d:cxt->oops_buf vzalloc fail\n", __func__, __LINE__);
		if (bm_write_buffer)
			vfree(bm_write_buffer);
		return 0;
	}

	/*init netlink*/
	//bm_netlink_init();

	/*get platform_driver from device tree*/
	platform_driver_register(&boot_monitor_driver);

	/*creat /proc/boot_monitor */
	bootmonitor_entry = proc_create("boot_monitor", 0664, NULL, &boot_monitor_fops);
	if (!bootmonitor_entry) {
		if (bm_write_buffer)
			vfree(bm_write_buffer);
		if (cxt->oops_buf)
			vfree(cxt->oops_buf);
		MTN_PRINT_ERR("%s-%d:fail to create file node\n", __func__, __LINE__);
		return 0;
	}

	monitor_main = kthread_run(monitor_main_thread_body, NULL, "monitor_main");
	if (!monitor_main) {
		if (bm_write_buffer)
			vfree(bm_write_buffer);
		if (cxt->oops_buf)
			vfree(cxt->oops_buf);
		if (bootmonitor_entry)
			proc_remove(bootmonitor_entry);
		MTN_PRINT_ERR("%s-%d:create thread monitor_main\n", __func__, __LINE__);
		return 0;
	}
	MTN_PRINT_END();
	return 0;
}

static void __exit boot_monitor_exit(void)
{
	struct bootmonitor_context *cxt;

	cxt = &boot_cxt;
	/*add bdev == NULL ,because bdev no init*/
	if (IS_ERR(bdev) || bdev == NULL) {
		MTN_PRINT_ERR("%s-%d:bdev no\n", __func__, __LINE__);
	} else {
		sync_blockdev(bdev);
		invalidate_mapping_pages(bdev->bd_inode->i_mapping, 0, -1);
		blkdev_put(bdev, NULL);
		MTN_PRINT_ERR("invalidate_mapping_pages end\n");
	}
	mutex_destroy(&write_bm_mutex);
	if (bm_write_buffer) {
		MTN_PRINT_INFO("%s-%d:bm_write_buffer is %p\n", __func__, __LINE__, bm_write_buffer);
		vfree(bm_write_buffer);
	}
	if (cxt->oops_buf) {
		MTN_PRINT_INFO("%s-%d:cxt->oops_buf is %p\n", __func__, __LINE__, cxt->oops_buf);
		vfree(cxt->oops_buf);
	}
	if (bootmonitor_entry){
		MTN_PRINT_INFO("%s-%d:bootmonitor_entry release\n", __func__, __LINE__);
		proc_remove(bootmonitor_entry);
	}
	if (monitor_main != NULL) {
		MTN_PRINT_INFO("kthread_stop monitor_main\n");
		platform_driver_unregister(&boot_monitor_driver);
		kthread_stop(monitor_main);
	}
	MTN_PRINT_INFO("exit monitor_main\n");
}

module_init(boot_monitor_init);
module_exit(boot_monitor_exit);
MODULE_LICENSE("GPL v2");
