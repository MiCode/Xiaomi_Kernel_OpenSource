/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define MAX_FREQ_STEP 6

enum clk_id {
	CLK_MM,
	CLK_CAM,
	CLK_IMG,
	CLK_VENC,
	CLK_VDEC,
	CLK_IPE,
	CLK_DPE,
	CLK_CCU,
	CLK_MAX_NUM,
};

#if IS_ENABLED(CONFIG_MTK_MMDVFS)
s32 mmdvfs_wrapper_set_freq(u32 clk_id, u32 freq);
s32 mmdvfs_wrapper_get_freq_steps(u32 clk_id, u64 *freq_steps, u32 *step_size);
u64 mmdvfs_qos_get_freq(u32 clk_id);
#else
static inline s32 mmdvfs_wrapper_set_freq(u32 clk_id, u32 freq) { return 0; }
static inline s32 mmdvfs_wrapper_get_freq_steps(
	u32 clk_id, u64 *freq_steps, u32 *step_size)
	{ *step_size = 0; return 0; }
static inline u64 mmdvfs_qos_get_freq(u32 clk_id) { return 0; }
#endif
