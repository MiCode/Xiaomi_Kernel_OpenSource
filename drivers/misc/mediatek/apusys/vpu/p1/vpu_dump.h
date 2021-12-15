// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VPU_DUMP_H__
#define __VPU_DUMP_H__

#include "vpu_cfg.h"
#include "vpu_ioctl.h"
#include "vpu_algo.h"
#include "vpu_cmd.h"

struct vpu_device;

struct vpu_dmp {
	// general info
	int read_cnt; // counter of AEE reads the dump from debugfs.
	char info[VPU_DMP_INFO_SZ];
	uint64_t time;
	struct vpu_request req;

	// device
	int vd_state;
	uint32_t vd_dev_state;
	int vd_pw_boost;

	// registers
	uint32_t r_info[VPU_DMP_REG_CNT_INFO];
	uint32_t r_dbg[VPU_DMP_REG_CNT_DBG];
	uint32_t r_mbox[VPU_DMP_REG_CNT_MBOX];
	uint32_t r_CG_CON;
	uint32_t r_SW_RST;
	uint32_t r_DONE_ST;
	uint32_t r_CTRL;

	// command
	struct vpu_cmd_ctl c_ctl[VPU_MAX_PRIORITY];
	struct __vpu_algo c_alg[VPU_MAX_PRIORITY];
	int c_prio;
	int c_prio_max;
	int c_active;
	uint64_t c_timeout;

	// memory
	uint8_t m_reset[VPU_DMP_RESET_SZ];
	uint8_t m_main[VPU_DMP_MAIN_SZ];
	uint8_t m_kernel[VPU_DMP_KERNEL_SZ];
	uint8_t m_iram[VPU_DMP_IRAM_SZ];
	uint8_t m_work[VPU_DMP_WORK_SZ];
	uint8_t m_reg[VPU_DMP_REG_SZ];
	uint8_t m_imem[VPU_DMP_IMEM_SZ];
	uint8_t m_dmem[VPU_DMP_DMEM_SZ];
#if VPU_XOS
	uint8_t m_pl_algo[VPU_MAX_PRIORITY][VPU_DMP_PRELOAD_SZ];
	uint8_t m_pl_iram[VPU_MAX_PRIORITY][VPU_DMP_IRAM_SZ];
#endif
};

void vpu_dmp_init(struct vpu_device *vd);
void vpu_dmp_exit(struct vpu_device *vd);
int vpu_dmp_create_locked(struct vpu_device *vd, struct vpu_request *req,
	const char *fmt, ...);
void vpu_dmp_free_locked(struct vpu_device *vd);
void vpu_dmp_free_all(void);
void vpu_dmp_seq_core(struct seq_file *s, struct vpu_device *vd);
void vpu_dmp_seq(struct seq_file *s);

#define vpu_dmp_create(vd, req, fmt, args...) do { \
		pr_info("%s: vpu_dmp_create\n", __func__); \
		vpu_cmd_lock_all(vd); \
		if (!vpu_pwr_get_locked_nb(vd)) { \
			if (!vpu_dev_boot(vd)) { \
				vpu_dmp_create_locked(vd, req, fmt, ##args); \
			} \
			vpu_pwr_put_locked_nb(vd); \
		} \
		vpu_cmd_unlock_all(vd); \
	} while (0)

#define vpu_dmp_free(vd) do { \
		pr_info("%s: vpu_dmp_free\n", __func__); \
		vpu_cmd_lock_all(vd); \
		vpu_dmp_free_locked(vd); \
		vpu_cmd_unlock_all(vd); \
	} while (0)

#endif

