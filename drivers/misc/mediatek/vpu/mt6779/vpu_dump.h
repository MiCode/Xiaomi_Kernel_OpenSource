/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __VPU_DUMP_H__
#define __VPU_DUMP_H__

#include "vpu_cfg.h"
#include "vpu_drv.h"

struct vpu_device;

struct vpu_dmp {
	// general info
	char info[VPU_DMP_INFO_SZ];
	uint64_t time;
	struct vpu_request req;

	// device
	int vd_state;
	uint32_t vd_dev_state;

	// registers
	uint32_t r_info[VPU_DMP_REG_CNT_INFO];
	uint32_t r_dbg[VPU_DMP_REG_CNT_DBG];
	uint32_t r_CG_CON;
	uint32_t r_SW_RST;
	uint32_t r_DONE_ST;
	uint32_t r_CTRL;

	struct vpu_algo alg;

	int c_prio;
	int c_prio_max;
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

	// iomem
	uint8_t m_ipu_conn[VPU_DMP_SZ];
	uint8_t m_ipu_vcore[VPU_DMP_SZ];
	uint8_t m_infra_cfg[VPU_DMP_SZ];
	uint8_t m_sleep[VPU_DMP_SZ];
	uint8_t m_gals[VPU_DMP_SZ];
	uint8_t m_smi_cmn0[VPU_DMP_SZ];
	uint8_t m_smi_cmn1[VPU_DMP_SZ];
};

#ifdef CONFIG_MTK_VPU_SUPPORT
void vpu_dmp_init(int core);
void vpu_dmp_exit(int core);
int vpu_dmp_create_locked(int core, struct vpu_request *req,
	const char *fmt, ...);
void vpu_dmp_free_locked(int core);
void vpu_dmp_free_all(void);
void vpu_dmp_seq_core(struct seq_file *s, int core);
void vpu_dmp_seq(struct seq_file *s);

#define vpu_dmp_create(core, req, fmt, args...) do { \
		pr_info("%s: vpu_dmp_create: vpu%d\n", __func__, core); \
		vpu_lock(core); \
		if (!vpu_get_power(core, false)) { \
			vpu_dmp_create_locked(core, req, fmt, ##args); \
			vpu_put_power(core, VPT_ENQUE_ON); \
		} else { \
			pr_info("%s: vpu_get_power: failed\n", __func__); \
		} \
		vpu_unlock(core); \
	} while (0)

#define vpu_dmp_free(core) do { \
		pr_info("%s: vpu_dmp_free\n", __func__); \
		vpu_lock(core); \
		vpu_dmp_free_locked(core); \
		vpu_unlock(core); \
	} while (0)

#else
static inline
void vpu_dmp_init(int core)
{
}

static inline
void vpu_dmp_exit(int core)
{
}

static inline
int vpu_dmp_create_locked(int core, struct vpu_request *req,
	const char *fmt, ...)
{
	return 0;
}

static inline
int vpu_dmp_create(int core, struct vpu_request *req,
	const char *fmt, ...)
{
	return 0;
}

static inline
void vpu_dmp_free_locked(int core)
{
}

static inline
void vpu_dmp_free(int core)
{
}

static inline
void vpu_dmp_free_all(void)
{
}

static inline
void vpu_dmp_seq_core(struct seq_file *s, int core)
{
}

static inline
void vpu_dmp_seq(struct seq_file *s)
{
}
#endif

#endif

