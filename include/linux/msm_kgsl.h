/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef _MSM_KGSL_H
#define _MSM_KGSL_H

/**
 * struct kgsl_gpu_freq_stat - Per GPU freq stat struct
 * @freq: GPU frequency in Hz
 * @active_time: GPU busy time in usecs
 * @idle_time: GPU idle time in usec
 */
struct kgsl_gpu_freq_stat {
	u32 freq;
	u64 active_time;
	u64 idle_time;
};

/**
 * enum kgsl_srcu_events - kgsl srcu notify events for listeners
 * @GPU_GMU_READY - GMU initialized to handle other requests
 * @GPU_GMU_STOP - GMU is not ready to handle other requests
 * @GPU_SSR_BEGIN - GPU/GMU fault detected to start recovery
 * @GPU_SSR_END - GPU/GMU fault recovery end
 * @GPU_SSR_FATAL - LSR context invalidated
 */
enum kgsl_srcu_events {
	GPU_GMU_READY,
	GPU_GMU_STOP,
	GPU_SSR_BEGIN,
	GPU_SSR_END,
	GPU_SSR_FATAL,
};

#if IS_ENABLED(CONFIG_QCOM_KGSL)
/**
 * kgsl_gpu_num_freqs - Get number of available GPU frequencies
 *
 * Return: number of available frequencies on success or negative error
 * on failure
 */
int kgsl_gpu_num_freqs(void);

/**
 * kgsl_gpu_stat - Get per GPU freq stats
 * @stats: Array of struct kgsl_gpu_freq_stat to hold stats
 * @numfreq: Number of entries in @stats
 *
 * This function will populate @stats with per freq stats.
 * Number of entries in @stats array  must be greater or
 * equal to value returned by function kgsl_gpu_num_freqs
 *
 * Return: 0 on success or negative error on failure
 */
int kgsl_gpu_stat(struct kgsl_gpu_freq_stat *stats, u32 numfreq);

/**
 * kgsl_gpu_frame_count - Get number of frames already processed by GPU
 * @pid: pid of the process for which frame count is required
 * @frame_count: pointer to a u64 to store frame count
 *
 * Return: zero on success and number of frames processed corresponding
 * to @pid in @frame_count or negative error on failure
 */
int kgsl_gpu_frame_count(pid_t pid, u64 *frame_count);

/**
 * kgsl_get_stats - Get memory usage of any process or total driver memory
 * @pid: PID of the process
 *
 * Provide the number of bytes of memory that were allocated for process
 * with pid as @pid. If @pid is negative, provide the total memory allocated
 * by the driver in bytes.
 *
 * Return: Total driver memory if @pid is negative, process memory otherwise.
 */
u64 kgsl_get_stats(pid_t pid);

/**
 * kgsl_add_rcu_notifier - Adds a notifier to an SRCU notifier chain.
 * @nb: Notifier block new entry to add in notifier chain
 *
 * Returns zero on success or error on failure.
 */
int kgsl_add_rcu_notifier(struct notifier_block *nb);

/**
 * kgsl_del_rcu_notifier - Remove notifier from an SRCU notifier chain.
 * @nb: Entry to remove from notifier chain
 *
 * Returns zero on success or -ENOENT on failure.
 */
int kgsl_del_rcu_notifier(struct notifier_block *nb);

#else /* !CONFIG_QCOM_KGSL */
static inline int kgsl_gpu_num_freqs(void)
{
	return -ENODEV;
}

static inline int kgsl_gpu_stat(struct kgsl_gpu_freq_stat *stats, u32 numfreq)
{
	return -ENODEV;
}

static inline int kgsl_gpu_frame_count(pid_t pid, u64 *frame_count)
{
	return -ENODEV;
}

static inline u64 kgsl_get_stats(pid_t pid)
{
	return 0;
}

static inline int kgsl_add_rcu_notifier(struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int kgsl_del_rcu_notifier(struct notifier_block *nb)
{
	return -ENOENT;
}
#endif /* CONFIG_QCOM_KGSL */
#endif /* _MSM_KGSL_H */
