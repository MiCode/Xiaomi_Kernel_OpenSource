/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _LINUX_BINDER_INTERNAL_H
#define _LINUX_BINDER_INTERNAL_H

#include <linux/export.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/refcount.h>
#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/uidgid.h>

#ifdef CONFIG_MTK_ENG_BUILD
#define BINDER_WATCHDOG		"v0.1"
#endif
#define BINDER_USER_TRACKING	1

#ifdef BINDER_USER_TRACKING
#include <linux/rtc.h>
#include <linux/time.h>
#endif

#ifdef BINDER_WATCHDOG
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/kthread.h>

/*****************************************************************************/
/* Payload layout of addService():                                           */
/*                                                                           */
/* Parcel header | IServiceManager.descriptor | Parcel header | Service name */
/* (Please refer ServiceManagerNative.java:addService())                     */
/* IServiceManager.descriptor is 'android.os.IServiceManager' interleaved    */
/* with character '\0'.                                                      */
/* that is, 'a', '\0', 'n', '\0', 'd', '\0', 'r', '\0', 'o', ...             */
/*                                                                           */
/* so the offset of Service name                                             */
/* = Parcel header x2 + strlen(android.os.IServiceManager) x2                */
/* = 8x2 + 26x2 = 68                                                         */
/*****************************************************************************/
#define MAX_SERVICE_NAME_LEN    32
#define MAGIC_SERVICE_NAME_OFFSET       68
#define MAX_ENG_TRANS_LOG_BUFF_LEN      10240

enum wait_on_reason {
	WAIT_ON_NONE = 0U,
	WAIT_ON_READ = 1U,
	WAIT_ON_EXEC = 2U,
	WAIT_ON_REPLY_READ = 3U,
};
#endif

struct binder_context {
	struct binder_node *binder_context_mgr_node;
	struct mutex context_mgr_node_lock;
	kuid_t binder_context_mgr_uid;
	const char *name;
};

/**
 * struct binder_device - information about a binder device node
 * @hlist:          list of binder devices (only used for devices requested via
 *                  CONFIG_ANDROID_BINDER_DEVICES)
 * @miscdev:        information about a binder character device node
 * @context:        binder context information
 * @binderfs_inode: This is the inode of the root dentry of the super block
 *                  belonging to a binderfs mount.
 */
struct binder_device {
	struct hlist_node hlist;
	struct miscdevice miscdev;
	struct binder_context context;
	struct inode *binderfs_inode;
	refcount_t ref;
};

/**
 * binderfs_mount_opts - mount options for binderfs
 * @max: maximum number of allocatable binderfs binder devices
 * @stats_mode: enable binder stats in binderfs.
 */
struct binderfs_mount_opts {
	int max;
	int stats_mode;
};

/**
 * binderfs_info - information about a binderfs mount
 * @ipc_ns:         The ipc namespace the binderfs mount belongs to.
 * @control_dentry: This records the dentry of this binderfs mount
 *                  binder-control device.
 * @root_uid:       uid that needs to be used when a new binder device is
 *                  created.
 * @root_gid:       gid that needs to be used when a new binder device is
 *                  created.
 * @mount_opts:     The mount options in use.
 * @device_count:   The current number of allocated binder devices.
 * @proc_log_dir:   Pointer to the directory dentry containing process-specific
 *                  logs.
 */
struct binderfs_info {
	struct ipc_namespace *ipc_ns;
	struct dentry *control_dentry;
	kuid_t root_uid;
	kgid_t root_gid;
	struct binderfs_mount_opts mount_opts;
	int device_count;
	struct dentry *proc_log_dir;
};

extern const struct file_operations binder_fops;

extern char *binder_devices_param;

#ifdef CONFIG_ANDROID_BINDERFS
extern bool is_binderfs_device(const struct inode *inode);
extern struct dentry *binderfs_create_file(struct dentry *dir, const char *name,
					   const struct file_operations *fops,
					   void *data);
extern void binderfs_remove_file(struct dentry *dentry);
#else
static inline bool is_binderfs_device(const struct inode *inode)
{
	return false;
}
static inline struct dentry *binderfs_create_file(struct dentry *dir,
					   const char *name,
					   const struct file_operations *fops,
					   void *data)
{
	return NULL;
}
static inline void binderfs_remove_file(struct dentry *dentry) {}
#endif

#ifdef CONFIG_ANDROID_BINDERFS
extern int __init init_binderfs(void);
#else
static inline int __init init_binderfs(void)
{
	return 0;
}
#endif

int binder_stats_show(struct seq_file *m, void *unused);
DEFINE_SHOW_ATTRIBUTE(binder_stats);

int binder_state_show(struct seq_file *m, void *unused);
DEFINE_SHOW_ATTRIBUTE(binder_state);

int binder_transactions_show(struct seq_file *m, void *unused);
DEFINE_SHOW_ATTRIBUTE(binder_transactions);

int binder_transaction_log_show(struct seq_file *m, void *unused);
DEFINE_SHOW_ATTRIBUTE(binder_transaction_log);

struct binder_transaction_log_entry {
	int debug_id;
	int debug_id_done;
	int call_type;
	int from_proc;
	int from_thread;
	int target_handle;
	int to_proc;
	int to_thread;
	int to_node;
	int data_size;
	int offsets_size;
	int return_error_line;
	uint32_t return_error;
	uint32_t return_error_param;
	const char *context_name;
#ifdef BINDER_WATCHDOG
	unsigned int code;
	char service[MAX_SERVICE_NAME_LEN];
	int fd;
	struct timespec readstamp;
	struct timespec endstamp;
	unsigned int cur;
#endif
#ifdef BINDER_USER_TRACKING
	struct timespec timestamp;
	struct timeval tv;
#endif
};
#ifdef BINDER_WATCHDOG
struct binder_timeout_log_entry {
	enum wait_on_reason r;
	pid_t from_proc;
	pid_t from_thrd;
	pid_t to_proc;
	pid_t to_thrd;
	unsigned int over_sec;
	struct timespec ts;
	struct timeval tv;
	unsigned int code;
	char service[MAX_SERVICE_NAME_LEN];
	int debug_id;
};
#endif

struct binder_transaction_log {
	atomic_t cur;
	bool full;
#ifdef BINDER_WATCHDOG
	unsigned int size;
	struct binder_transaction_log_entry *entry;
#else
	struct binder_transaction_log_entry entry[32];
#endif
};

extern struct binder_transaction_log binder_transaction_log;
extern struct binder_transaction_log binder_transaction_log_failed;
#endif /* _LINUX_BINDER_INTERNAL_H */
