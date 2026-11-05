#ifndef __BOOT_MONITOR_H__
#define __BOOT_MONITOR_H__

#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/sched/clock.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/kmsg_dump.h>
#include <linux/timekeeping.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/pagemap.h>
#include <linux/backing-dev.h>
#include <linux/mount.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/major.h>
#include <linux/writeback.h>
#include <linux/export.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/stat.h>

#define DEFAULT_TIMEOUT	                  5
#define DEFAULT_BOOT_TIME                 45

#define MTN_PRINT_ERR(args...)            pr_err("<monitor fail>"args);
#define MTN_PRINT_INFO(args...)           pr_info("<monitor info>"args);
#define MTN_PRINT_START(args...)          pr_info(">>>enter  %s: line: %d.\n", __func__, __LINE__);
#define MTN_PRINT_END(args...)            pr_info("<<<exit  %s: line %d.\n", __func__, __LINE__);
#define countof(arr)                      (sizeof(arr) / sizeof(arr[0]))

#define BOOT_MONITOR_HEADER_SIZE          512
#define BOOT_MONITOR_SIZE                 32

#define LOG_MONITOR_HEADER_MAGIC          0xA99BD480DD25E1F0
#define BOOT_MONITOR_VERSION              0x1000
#define PARTITION_KMSG_OFFSET             0x9C00000 // 154 M + 2 M
#define PARTITION_KMSG_SIZE               0x100000  //1 M
#define PARTITION_PMSG_OFFSET             0x9D00000 // 154 M + 2 M + 1 M
#define PARTITION_PMSG_SIZE               0x200000  //2 M
#define PARTITION_LOG_OFFSET              0x200000

#define NETLINK_TEST                      31
#define MAX_MSGSIZE                       128

#define AMS_READY                         8
#define AMS_READY_TIME                    80

#define RPOC_ENTRY_LINE                   64
#define MAX_CMDLINE_PARAM_LEN             128

#define BOOT_MODE_NORMAL                  "normal"
#define BOOT_OK                           "boot-ok"
#define BEFORE_VERSION                    "no record for fisrt bootfail init"
#define FIRST_BOOT                    	  "mFirstBoot"

extern unsigned long get_log_count(void);
extern struct log_t **get_bootprof_pointer(void);
extern void bm_netlink_exit(void);
extern void bm_sendnlmsg(char *message);
extern void monitor_get_kmsg(char *buffer);
extern int get_bm_devices(void);
extern int _partition_bm_read(struct block_device *dev, loff_t from, size_t len, void *buf);
extern int partition_bm_write(loff_t to, size_t len, const void *buf);
extern int bm_netlink_init(void);

extern spinlock_t bootprof_lock;
extern unsigned long boottime_ok;
extern int bm_event_counts;
extern int first_boot;
extern char bm_boot_mode[10];
extern char *bm_write_buffer;
extern struct block_device *bdev;
extern struct mutex write_bm_mutex;
extern struct platform_driver boot_monitor_driver;

extern char build_fingerprint[MAX_CMDLINE_PARAM_LEN];
extern int blackbox_version;
extern int os_version;

struct boot_platform_data {
	unsigned long	mem_size;
	phys_addr_t	    mem_address;
	unsigned long	console_size;
	unsigned long	pmsg_size;
};

struct bootmonitor_context {
	struct boot_platform_data monitor_data;
	void *oops_buf;
};

extern int write_blackbox_header(struct bootmonitor_context *cxt, char *buffer, int event);
extern void monitor_get_pmsg(struct bootmonitor_context *cxt);

struct type_map {
	int warntime;
	int errortimes;
	char name[40];
};

extern struct bootmonitor_context boot_cxt;
extern struct type_map bm_boot_events[];

struct pmsg_buffer_hdr {
	uint32_t    sig;
	atomic_t    start;
	atomic_t    size;
	uint8_t     data[0];
};

#endif