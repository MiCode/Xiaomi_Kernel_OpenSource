/* SPDX-License-Identifier: GPL-2.0 */
//
//Copyright (C) 2016-2024 Unisoc (Shanghai) Technologies Co., Ltd

#ifndef __SPRD_PNP_COMM_H__
#define __SPRD_PNP_COMM_H__

#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/wait.h>

#define ENABLE_PNP_GPU_INFO        (0)

#define EVENT_MAX                  (128)
#define CPU_FREQ_MAX               (100)
#define CPU_IDLE_MAX               (100)
#define CPU_MAX                    (8)

#if ENABLE_PNP_GPU_INFO
#define GPU_FREQ_MAX               (50)
#endif //ENABLE_PNP_GPU_INFO

extern int sprd_pnp_log_force;

#define SPRD_PNP_ERR(fmt, ...)  pr_err("[%s] "pr_fmt(fmt), "SPRD_PNP", ##__VA_ARGS__)
#define SPRD_PNP_WARN(fmt, ...) pr_warn("[%s]"pr_fmt(fmt), "SPRD_PNP", ##__VA_ARGS__)
#define SPRD_PNP_INFO(fmt, ...)                                         \
	do {                                                                \
		if (!sprd_pnp_log_force)                                        \
			pr_debug("[%s] "pr_fmt(fmt), "SPRD_PNP", ##__VA_ARGS__);    \
		else                                                            \
			pr_info("[%s] "pr_fmt(fmt), "SPRD_PNP", ##__VA_ARGS__);     \
	} while (0)                                                         \

struct cpu_info_buffer {
	int len;
	char event[EVENT_MAX];
};

struct per_cpu_item {
	bool non_empty;
	unsigned int cpu;
	int w_pos;
	int r_pos;
	struct cpu_info_buffer *page;
};

struct cpu_freq_info {
	bool debug_en;
	int regis_cnt;
	int overflow;
	struct per_cpu_item *item;
	struct cpu_info_buffer *buf;
	char *cpu_freq_buf;
};

struct cpu_idle_page {
	int w_pos;
	int r_pos;
	struct cpu_info_buffer *page;
};

struct cpu_idle_info {
	bool debug_en;
	int regis_cnt;
	int overflow;
	struct cpu_idle_page *ctrl;
	char *cpu_idle_buf;
};

#if ENABLE_PNP_GPU_INFO
struct gpu_info_buffer {
	int len;
	char event[EVENT_MAX];
};

struct gpu_freq_page {
	int w_pos;
	int r_pos;
	struct gpu_info_buffer *page;
};

struct gpu_freq_info {
	bool debug_en;
	int regis_cnt;
	int overflow;
	struct gpu_freq_page *ctrl;
	char *gpu_freq_buf;
};
#endif //ENABLE_PNP_GPU_INFO

struct unipnp_event {
	char name[32];
	int value;
};

struct sprd_pnp_platform_info {
	struct device *dev;
	struct task_struct *wait_thread_task;
	wait_queue_head_t notify_wq;
	wait_queue_head_t poll_wq;
	int wake_up_condition;
	struct unipnp_event event;
	struct cpu_freq_info cpu_freq_data;
	struct cpu_idle_info cpu_idle_data;
	//struct gpu_freq_info gpu_freq_data;
};

extern ssize_t common_en_read(bool *debug_en, struct file *file,
		char __user *in_buf, size_t count, loff_t *ppos);
extern ssize_t common_en_write(bool *debug_en, struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos);

#endif /* __SPRD_PNP_COMM_H__ */
