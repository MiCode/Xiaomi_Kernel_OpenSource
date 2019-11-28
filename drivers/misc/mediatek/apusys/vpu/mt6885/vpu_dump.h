/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __VPU_DUMP_H__
#define __VPU_DUMP_H__

#include "vpu_cmn.h"

#define VPU_DMP_RESET_SZ  0x400
#define VPU_DMP_MAIN_SZ   0x40000  // 256 KB
#define VPU_DMP_KERNEL_SZ 0x20000  // 128 KB
#define VPU_DMP_IRAM_SZ   0x10000  // 64 KB
#define VPU_DMP_WORK_SZ   0x2000   // 8 KB
#define VPU_DMP_REG_SZ    0x1000   // 4 KB
#define VPU_DMP_IMEM_SZ   0x30000  // 192 KB
#define VPU_DMP_DMEM_SZ   0x40000  // 256 KB

#define VPU_DMP_REG_CNT_INFO 32
#define VPU_DMP_REG_CNT_DBG 8

#define VPU_DMP_INFO_SZ 128

struct vpu_dmp {
	// general info
	char info[VPU_DMP_INFO_SZ];
	uint64_t time;
	struct vpu_request req;

	// device
	int vd_state;
	struct vpu_algo vd_algo_curr;

	// registers
	uint32_t r_info[VPU_DMP_REG_CNT_INFO];
	uint32_t r_dbg[VPU_DMP_REG_CNT_DBG];
	uint32_t r_CG_CON;
	uint32_t r_SW_RST;
	uint32_t r_DONE_ST;
	uint32_t r_CTRL;

	// memory
	uint8_t m_reset[VPU_DMP_RESET_SZ];
	uint8_t m_main[VPU_DMP_MAIN_SZ];
	uint8_t m_kernel[VPU_DMP_KERNEL_SZ];
	uint8_t m_iram[VPU_DMP_IRAM_SZ];
	uint8_t m_work[VPU_DMP_WORK_SZ];
	uint8_t m_reg[VPU_DMP_REG_SZ];
	uint8_t m_imem[VPU_DMP_IMEM_SZ];
	uint8_t m_dmem[VPU_DMP_DMEM_SZ];
};

#ifdef CONFIG_MTK_APUSYS_VPU_DEBUG
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
		mutex_lock(&vd->lock); \
		mutex_lock(&vd->cmd_lock); \
		if (!vpu_pwr_get_locked_nb(vd)) { \
			if (!vpu_dev_boot(vd)) { \
				vpu_dmp_create_locked(vd, req, fmt, ##args); \
			} \
			vpu_pwr_put_locked(vd); \
		} \
		mutex_unlock(&vd->cmd_lock); \
		mutex_unlock(&vd->lock); \
	} while (0)

#define vpu_dmp_free(vd) do { \
		pr_info("%s: vpu_dmp_free\n", __func__); \
		mutex_lock(&vd->lock); \
		mutex_lock(&vd->cmd_lock); \
		vpu_dmp_free_locked(vd); \
		mutex_unlock(&vd->cmd_lock); \
		mutex_unlock(&vd->lock); \
	} while (0)

#else
static inline
void vpu_dmp_init(struct vpu_device *vd)
{
}

static inline
void vpu_dmp_exit(struct vpu_device *vd)
{
}

static inline
int vpu_dmp_create_locked(struct vpu_device *vd, struct vpu_request *req,
	const char *fmt, ...)
{
	return 0;
}

static inline
int vpu_dmp_create(struct vpu_device *vd, struct vpu_request *req,
	const char *fmt, ...)
{
	return 0;
}

static inline
void vpu_dmp_free_locked(struct vpu_device *vd)
{
}

static inline
void vpu_dmp_free(struct vpu_device *vd)
{
}

static inline
void vpu_dmp_free_all(void)
{
}

static inline
void vpu_dmp_seq_core(struct seq_file *s, struct vpu_device *vd)
{
}

static inline
void vpu_dmp_seq(struct seq_file *s)
{
}
#endif

#endif

