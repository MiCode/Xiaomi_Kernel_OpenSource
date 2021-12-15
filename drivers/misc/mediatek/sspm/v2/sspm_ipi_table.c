// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include "sspm_ipi_table.h"

/*
 * mbox information
 *
 * mbdev  :mbox device
 * irq_num:identity of mbox irq
 * id     :mbox id
 * slot   :how many slots that mbox used, up to 1GB
 * opt    :option for mbox or share memory, 0:mbox, 1:share memory
 * enable :mbox status, 0:disable, 1: enable
 * is64d  :mbox is64d status, 0:32d, 1: 64d
 * base   :mbox base address
 * set_irq_reg  : mbox set irq register
 * clr_irq_reg  : mbox clear irq register
 * init_base_reg: mbox initialize register
 * send_status_reg: mbox send status register
 * recv_status_reg: mbox recv status register
 * mbox lock    : lock of mbox
 */
struct mtk_mbox_info sspm_mbox_table[SSPM_MBOX_TOTAL] = {
	{0, 0, 0, 32, 0, 1, 0, 0, 0, 0, 0, 0, 0, { { { { 0 } } } } },
	{0, 0, 1, 32, 0, 1, 0, 0, 0, 0, 0, 0, 0, { { { { 0 } } } } },
	{0, 0, 2, 32, 0, 1, 0, 0, 0, 0, 0, 0, 0, { { { { 0 } } } } },
	{0, 0, 3, 32, 0, 1, 0, 0, 0, 0, 0, 0, 0, { { { { 0 } } } } },
	{0, 0, 4, 32, 0, 1, 0, 0, 0, 0, 0, 0, 0, { { { { 0 } } } } },
};

/*
 * mbox pin structure, this is for send defination,
 * ipi=endpoint=pin
 * mbox     : (mbox number)mbox number of the pin, up to 16(plt)
 * offset   : (slot)msg offset in share memory, up to 1024*4 KB(plt)
 * send_opt : (opt)send opt, 0:send ,1: send for response(plt)
 * lock     : (lock)polling lock 0:unuse,1:used
 * msg_size : (slot)message size in words, 4 bytes alignment(plt)
 * pin_index  : (bit offset)pin index in the mbox(plt)
 * chan_id    : (u32) ipc channel id(plt)
 * mutex      : (mutex)mutex for remote response
 * completion : (completion)completion for remote response
 * pin_lock   : (spinlock_t)lock of the pin
 */
struct mtk_mbox_pin_send sspm_mbox_pin_send[] = {
/* the following will use mbox 0 */
	{0, IPIS_C_PPM_OUT_OFFSET, 1, 0, IPIS_C_PPM_OUT_SIZE,
	 IPIS_C_PPM_OUT_OFFSET, IPIS_C_PPM,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
	{0, IPIS_C_QOS_OUT_OFFSET, 1, 0, IPIS_C_QOS_OUT_SIZE,
	 IPIS_C_QOS_OUT_OFFSET, IPIS_C_QOS,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
	{0, IPIS_C_PMIC_OUT_OFFSET, 1, 0, IPIS_C_PMIC_OUT_SIZE,
	 IPIS_C_PMIC_OUT_OFFSET, IPIS_C_PMIC,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
	{0, IPIS_C_MET_OUT_OFFSET, 1, 0, IPIS_C_MET_OUT_SIZE,
	 IPIS_C_MET_OUT_OFFSET, IPIS_C_MET,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
	{0, IPIS_C_THERMAL_OUT_OFFSET, 1, 0, IPIS_C_THERMAL_OUT_SIZE,
	 IPIS_C_THERMAL_OUT_OFFSET, IPIS_C_THERMAL,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
	{0, IPIS_C_GPU_DVFS_OUT_OFFSET, 1, 0, IPIS_C_GPU_DVFS_OUT_SIZE,
	 IPIS_C_GPU_DVFS_OUT_OFFSET, IPIS_C_GPU_DVFS,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
	{0, IPIS_C_GPU_PM_OUT_OFFSET, 1, 0, IPIS_C_GPU_PM_OUT_SIZE,
	 IPIS_C_GPU_PM_OUT_OFFSET, IPIS_C_GPU_PM,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
/* the following will use mbox 1 */
	{1, IPIS_C_PLATFORM_OUT_OFFSET, 1, 0, IPIS_C_PLATFORM_OUT_SIZE,
	 IPIS_C_PLATFORM_OUT_OFFSET, IPIS_C_PLATFORM,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
	{1, IPIS_C_SMI_OUT_OFFSET, 1, 0, IPIS_C_SMI_OUT_SIZE,
	 IPIS_C_SMI_OUT_OFFSET, IPIS_C_SMI,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
	{1, IPIS_C_CM_OUT_OFFSET, 1, 0, IPIS_C_CM_OUT_SIZE,
	 IPIS_C_CM_OUT_OFFSET, IPIS_C_CM,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
	{1, IPIS_C_SLBC_OUT_OFFSET, 1, 0, IPIS_C_SLBC_OUT_SIZE,
	 IPIS_C_SLBC_OUT_OFFSET, IPIS_C_SLBC,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
	{1, IPIS_C_SPM_SUSPEND_OUT_OFFSET, 1, 0, IPIS_C_SPM_SUSPEND_OUT_SIZE,
	 IPIS_C_SPM_SUSPEND_OUT_OFFSET, IPIS_C_SPM_SUSPEND,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
	{1, IPIR_C_MET_OUT_OFFSET, 1, 0, IPIR_C_MET_OUT_SIZE,
	 IPIR_C_MET_OUT_OFFSET, IPIR_C_MET,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
	{1, IPIR_C_GPU_DVFS_OUT_OFFSET, 1, 0, IPIR_C_GPU_DVFS_OUT_SIZE,
	 IPIR_C_GPU_DVFS_OUT_OFFSET, IPIR_C_GPU_DVFS,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
	{1, IPIR_C_PLATFORM_OUT_OFFSET, 1, 0, IPIR_C_PLATFORM_OUT_SIZE,
	 IPIR_C_PLATFORM_OUT_OFFSET, IPIR_C_PLATFORM,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
	{1, IPIR_C_SLBC_OUT_OFFSET, 1, 0, IPIR_C_SLBC_OUT_SIZE,
	 IPIR_C_SLBC_OUT_OFFSET, IPIR_C_SLBC,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
/* the following will use mbox 4 */
	{4, IPIS_C_FHCTL_OUT_OFFSET, 1, 0, IPIS_C_FHCTL_OUT_SIZE,
	 IPIS_C_FHCTL_OUT_OFFSET, IPIS_C_FHCTL,
	 { { 0 } }, { 0 }, { { { { 0 } } } } },
};

/*
 * mbox pin structure, this is for receive defination,
 * ipi=endpoint=pin
 * mbox     : (mbox number)mbox number of the pin, up to 16(plt)
 * offset   : (slot)msg offset in share memory, up to 1024*4 KB(plt)
 * recv_opt : (opt)recv option,  0:receive ,1: response(plt)
 * lock     : (lock)polling lock 0:unuse,1:used
 * buf_full_opt : (opt)buffer option 0:drop, 1:assert, 2:overwrite(plt)
 * cb_ctx_opt : (opt)callback option 0:isr context, 1:process context(plt)
 * msg_size   : (slot)msg used slots in the mbox, 4 bytes alignment(plt)
 * pin_index  : (bit offset)pin index in the mbox(plt)
 * chan_id : (u32) ipc channel id(plt)
 * notify     : (completion)notify process
 * mbox_pin_cb: (cb)cb function
 * pin_buf : (void*)buffer point
 * prdata  : (void*)private data
 * pin_lock: (spinlock_t)lock of the pin
 */
struct mtk_mbox_pin_recv sspm_mbox_pin_recv[] = {
/* the following will use mbox 2 */
	{2, IPIR_I_QOS_IN_OFFSET, 0, 0, 1, 1,
	 IPIR_I_QOS_IN_SIZE, IPIR_I_QOS_IN_OFFSET,
	 IPIR_I_QOS, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIR_C_MET_IN_OFFSET, 0, 0, 1, 1,
	 IPIR_C_MET_IN_SIZE, IPIR_C_MET_IN_OFFSET,
	 IPIR_C_MET, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIR_C_GPU_DVFS_IN_OFFSET, 0, 0, 1, 1,
	 IPIR_C_GPU_DVFS_IN_SIZE, IPIR_C_GPU_DVFS_IN_OFFSET,
	 IPIR_C_GPU_DVFS, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIR_C_PLATFORM_IN_OFFSET, 0, 0, 1, 1,
	 IPIR_C_PLATFORM_IN_SIZE, IPIR_C_PLATFORM_IN_OFFSET,
	 IPIR_C_PLATFORM, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIR_C_SLBC_IN_OFFSET, 0, 0, 1, 1,
	 IPIR_C_SLBC_IN_SIZE, IPIR_C_SLBC_IN_OFFSET,
	 IPIR_C_SLBC, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIS_C_PPM_IN_OFFSET, 1, 0, 1, 0,
	 IPIS_C_PPM_IN_SIZE, IPIS_C_PPM_IN_OFFSET,
	 IPIS_C_PPM, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIS_C_QOS_IN_OFFSET, 1, 0, 1, 0,
	 IPIS_C_QOS_IN_SIZE, IPIS_C_QOS_IN_OFFSET,
	 IPIS_C_QOS, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIS_C_PMIC_IN_OFFSET, 1, 0, 1, 0,
	 IPIS_C_PMIC_IN_SIZE, IPIS_C_PMIC_IN_OFFSET,
	 IPIS_C_PMIC, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIS_C_MET_IN_OFFSET, 1, 0, 1, 0,
	 IPIS_C_MET_IN_SIZE, IPIS_C_MET_IN_OFFSET,
	 IPIS_C_MET, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIS_C_THERMAL_IN_OFFSET, 1, 0, 1, 0,
	 IPIS_C_THERMAL_IN_SIZE, IPIS_C_THERMAL_IN_OFFSET,
	 IPIS_C_THERMAL, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIS_C_GPU_DVFS_IN_OFFSET, 1, 0, 1, 0,
	 IPIS_C_GPU_DVFS_IN_SIZE, IPIS_C_GPU_DVFS_IN_OFFSET,
	 IPIS_C_GPU_DVFS, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIS_C_GPU_PM_IN_OFFSET, 1, 0, 1, 0,
	 IPIS_C_GPU_PM_IN_SIZE, IPIS_C_GPU_PM_IN_OFFSET,
	 IPIS_C_GPU_PM, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIS_C_PLATFORM_IN_OFFSET, 1, 0, 1, 0,
	 IPIS_C_PLATFORM_IN_SIZE, IPIS_C_PLATFORM_IN_OFFSET,
	 IPIS_C_PLATFORM, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIS_C_SMI_IN_OFFSET, 1, 0, 1, 0,
	 IPIS_C_SMI_IN_SIZE, IPIS_C_SMI_IN_OFFSET,
	 IPIS_C_SMI, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIS_C_CM_IN_OFFSET, 1, 0, 1, 0,
	 IPIS_C_CM_IN_SIZE, IPIS_C_CM_IN_OFFSET,
	 IPIS_C_CM, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIS_C_SLBC_IN_OFFSET, 1, 0, 1, 0,
	 IPIS_C_SLBC_IN_SIZE, IPIS_C_SLBC_IN_OFFSET,
	 IPIS_C_SLBC, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
	{2, IPIS_C_SPM_SUSPEND_IN_OFFSET, 1, 0, 1, 0,
	 IPIS_C_SPM_SUSPEND_IN_SIZE, IPIS_C_SPM_SUSPEND_IN_OFFSET,
	 IPIS_C_SPM_SUSPEND, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
/* the following will use mbox 4 */
	{4, IPIS_C_FHCTL_IN_OFFSET, 1, 0, 1, 0,
	 IPIS_C_FHCTL_IN_SIZE, IPIS_C_FHCTL_IN_OFFSET,
	 IPIS_C_FHCTL, { 0 }, 0, 0, 0, { { { { 0 } } } },
	 { 0, 0, 0, 0, 0, 0 } },
};

#define SSPM_TOTAL_SEND_PIN	(sizeof(sspm_mbox_pin_send) \
				 / sizeof(struct mtk_mbox_pin_send))
#define SSPM_TOTAL_RECV_PIN	(sizeof(sspm_mbox_pin_recv) \
				 / sizeof(struct mtk_mbox_pin_recv))

struct mtk_mbox_device sspm_mboxdev = {
	.name = "sspm_mboxdev",
	.pin_recv_table = &sspm_mbox_pin_recv[0],
	.pin_send_table = &sspm_mbox_pin_send[0],
	.info_table = &sspm_mbox_table[0],
	.count = SSPM_MBOX_TOTAL,
	.recv_count = SSPM_TOTAL_RECV_PIN,
	.send_count = SSPM_TOTAL_SEND_PIN,
};

struct mtk_ipi_device sspm_ipidev = {
	.name = "sspm_ipidev",
	.id = IPI_DEV_SSPM,
	.mbdev = &sspm_mboxdev,
	.timeout_handler = sspm_ipi_timeout_cb,
};
