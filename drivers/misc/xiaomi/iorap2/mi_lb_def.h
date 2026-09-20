/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2024-2024 XRing Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MI_LB_DEF_H_
#define _MI_LB_DEF_H_

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/printk.h>

extern int g_lb_msg_level;
extern int g_app_uid;

#define MI_LB_ERR(msg, ...) \
                do { if (g_lb_msg_level >= 0) \
                        pr_notice("[LB E]:%s %d: "msg, __func__, __LINE__, ## __VA_ARGS__); \
                } while (0)

#define MI_LB_WARN(msg, ...) \
                do { if (g_lb_msg_level >= 1) \
                        pr_notice("[LB W]:%s %d: "msg, __func__, __LINE__, ## __VA_ARGS__); \
                } while (0)

#define MI_LB_INFO(msg, ...) \
                do { if (g_lb_msg_level >= 2) \
                        pr_notice("[LB I]:%s %d: "msg, __func__, __LINE__, ## __VA_ARGS__); \
                } while (0)

#define MI_LB_DBG(msg, ...) \
                do { if (g_lb_msg_level >= 3) \
                        pr_notice("[LB D]:%s %d: "msg, __func__, __LINE__, ## __VA_ARGS__); \
                } while (0)

//#define MI_LB_PREREAD 1

#define LB_DEV_NAME 		"iorap_dev"
#define MAX_COLLECT_FILES	2048
//worst case
#define RECORD_SIZE		(1024 * 128UL)
#define RECORD_BUFFER_SIZE	(sizeof(struct collect_info) * RECORD_SIZE)

#define PREREAD_FILE_SIZE (2048 * 1024 * 1024UL)

#define PREREAD_THREAD_CNT 1

#define MAX_APPS_LIST 100
#define MAX_COLLECT_PIDS 5

#define LB_MAGIC 'L'

#define LB_COLLECT   	_IOWR(LB_MAGIC, 0x1, struct lb_owner)
#define LB_PREREAD   	_IOWR(LB_MAGIC, 0x2, struct lb_owner)
#define LB_CLEAR     	_IOWR(LB_MAGIC, 0x3, struct lb_owner)
#define LB_FLUSH     	_IOWR(LB_MAGIC, 0x4, struct lb_owner)
#define LB_STOP      	_IOWR(LB_MAGIC, 0x5, struct lb_owner)
#define LB_ABORT      	_IOWR(LB_MAGIC, 0x6, struct lb_owner)
#define LB_GET_PREREAD_INFO_SIZE   	_IOWR(LB_MAGIC, 0x7, struct lb_owner)
#define LB_GET_PREREAD_INFO   		_IOWR(LB_MAGIC, 0x8, struct lb_owner)
#define LB_GET_APP_NAMES			_IOWR(LB_MAGIC, 0x9, struct lb_owner)

#define LB_PTE_SNAPSHOT	_IOWR(LB_MAGIC, 0x10, struct lb_owner)
#define LB_PTE_PREREAD	_IOWR(LB_MAGIC, 0x11, struct lb_owner)
#define LB_PTE_STOP	_IOWR(LB_MAGIC, 0x12, struct lb_owner)
#define LB_PTE_INTERRUPT	_IOWR(LB_MAGIC, 0x13, struct lb_owner)


enum APP_LAUNCH_CMD {
	APP_LAUNCH_NONE = 0,
	APP_COLLECT_START,
	APP_PREREAD_START,
	APP_LAUNCH_FINISH,
	APP_LAUNCH_ABORT,
	APP_LAUNCH_MAX
};

struct lb_owner {
        unsigned int uid;
        int pid;
        char package_name[256];
};

struct lb_app_list {
	unsigned int app_nums;
	struct lb_owner owner[MAX_APPS_LIST];
};

typedef struct lb_app_preread_file_info {
	struct timespec64 i_mtime;
	unsigned long i_ino;
	loff_t	i_size;
	char file_path[256];
	unsigned int bitmap_longs;
	unsigned long bitmaps[0];
}app_preread_file_info;

struct lb_app_preread_info {
	struct lb_owner owner;
	unsigned int files;
	unsigned long file_info_buf_size;
	app_preread_file_info preread_file_info[0];
};

struct lb_app_preread_info_size {
	struct lb_owner owner;
	unsigned long info_size;
};

struct collect_info {
	struct file *file;
	pgoff_t start;
	unsigned len; //in 4k
};

struct file_info {
	struct list_head list;
	unsigned int i_count;
	unsigned long i_ino;
	dev_t	s_dev;
	char *path;
	struct timespec64 i_mtime;
	unsigned long collect_num;
	loff_t	i_size;
	bool is_moved;
	bool is_collected;
	unsigned int mm_size;
	unsigned int  bitmap_longs;
	unsigned long *bitmaps;

#ifdef MI_LB_PREREAD
	struct file *pre_filp;
#endif
	//2G 2048
	// 1M
};

struct package_info {
	struct list_head app_list;
	//link all files of one package
	struct list_head file_infos;
	struct lb_owner owner;
	struct mutex package_lock;
	unsigned int files;
	unsigned int total_startup_bytes;
	unsigned int app_startup_bytes;
	int hit_rate;
	unsigned int mm_size;
	bool is_cleared;
	atomic_t users;
};

struct active_package_info {
	struct package_info *current_package;
	unsigned int pids_num;
	int pids[MAX_COLLECT_PIDS];
};

struct collected_cache_file  {
	unsigned long i_ino;
	dev_t	s_dev;
	struct file *file;
};

struct lb_preread {
	struct task_struct *preread_thread;
	wait_queue_head_t preread_wait;
};

struct lb_manager {
	spinlock_t lock;
	struct mutex package_list_lock;
	struct task_struct *compiler_thread;
	char file_path[PATH_MAX];
	//link all package
	struct list_head package_infos;
	//file info during collecting
	struct collected_cache_file tmp_file_infos[MAX_COLLECT_FILES];
	unsigned short collected_num;
	char *record_buf;
	unsigned int cur;
	wait_queue_head_t compiler_wait;
	struct mutex compiler_lock;
#ifdef MI_LB_PREREAD
	struct lb_preread preread[PREREAD_THREAD_CNT];
#endif
	struct delayed_work compiler_work;
	unsigned long total_memory;
	unsigned int app_num;
	unsigned long start_jiffies;
};

struct lb_data {
        int chr_major;
        struct class *chr_class;
        struct device *chr_dev;
        bool initialized;
};

#endif /* _MI_LB_DEF_H_ */
