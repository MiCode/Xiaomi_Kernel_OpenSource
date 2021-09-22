/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
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

#endif /* _MSM_KGSL_H */
