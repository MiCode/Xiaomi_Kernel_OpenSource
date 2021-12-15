// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include "msm_vidc_debug.h"
#include "hfi_common.h"

#define VIDC_CPU_BASE_OFFS_AR50_LT		0x000A0000
#define VIDEO_GCC_BASE_OFFS_AR50_LT		0x00000000
#define VIDEO_CC_BASE_OFFS_AR50_LT		0x00100000

#define VIDC_CPU_CS_BASE_OFFS_AR50_LT		(VIDC_CPU_BASE_OFFS_AR50_LT)
#define VIDC_CPU_IC_BASE_OFFS_AR50_LT		(VIDC_CPU_BASE_OFFS_AR50_LT)

#define VIDC_CPU_CS_A2HSOFTINTCLR_AR50_LT	(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x1C)
#define VIDC_CPU_CS_VMIMSG_AR50_LTi		(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x34)
#define VIDC_CPU_CS_VMIMSGAG0_AR50_LT		(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x38)
#define VIDC_CPU_CS_VMIMSGAG1_AR50_LT		(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x3C)
#define VIDC_CPU_CS_VMIMSGAG2_AR50_LT		(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x40)
#define VIDC_CPU_CS_VMIMSGAG3_AR50_LT		(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x44)
#define VIDC_CPU_CS_SCIACMD_AR50_LT		(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x48)

/* HFI_CTRL_STATUS */
#define VIDC_CPU_CS_SCIACMDARG0_AR50_LT		(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x4C)
#define VIDC_CPU_CS_SCIACMDARG0_BMSK_AR50_LT	0xff
#define VIDC_CPU_CS_SCIACMDARG0_SHFT_AR50_LT	0x0
#define VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_ERROR_STATUS_BMSK_AR50_LT	0xfe
#define VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_ERROR_STATUS_SHFT_AR50_LT	0x1
#define VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_STATUS_BMSK_AR50_LT	0x1
#define VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_STATUS_SHFT_AR50_LT	0x0
#define VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_PC_READY_AR50_LT           	0x100
#define VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_IDLE_MSG_BMSK_AR50_LT     0x40000000

/* HFI_QTBL_INFO */
#define VIDC_CPU_CS_SCIACMDARG1_AR50_LT		(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x50)

/* HFI_QTBL_ADDR */
#define VIDC_CPU_CS_SCIACMDARG2_AR50_LT		(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x54)

/* HFI_VERSION_INFO */
#define VIDC_CPU_CS_SCIACMDARG3_AR50_LT		(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x58)

/* VIDC_SFR_ADDR */
#define VIDC_CPU_CS_SCIBCMD_AR50_LT		(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x5C)

/* VIDC_MMAP_ADDR */
#define VIDC_CPU_CS_SCIBCMDARG0_AR50_LT		(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x60)

/* VIDC_UC_REGION_ADDR */
#define VIDC_CPU_CS_SCIBARG1_AR50_LT		(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x64)

/* VIDC_UC_REGION_ADDR */
#define VIDC_CPU_CS_SCIBARG2_AR50_LT		(VIDC_CPU_CS_BASE_OFFS_AR50_LT + 0x68)

#define VIDC_CPU_IC_SOFTINT_EN_AR50_LT	(VIDC_CPU_IC_BASE_OFFS_AR50_LT + 0x148)
#define VIDC_CPU_IC_SOFTINT_AR50_LT		(VIDC_CPU_IC_BASE_OFFS_AR50_LT + 0x150)
#define VIDC_CPU_IC_SOFTINT_H2A_BMSK_AR50_LT	0x8000
#define VIDC_CPU_IC_SOFTINT_H2A_SHFT_AR50_LT	0x1

/*
 * --------------------------------------------------------------------------
 * MODULE: vidc_wrapper
 * --------------------------------------------------------------------------
 */
#define VIDC_WRAPPER_BASE_OFFS_AR50_LT		0x000B0000

#define VIDC_WRAPPER_HW_VERSION_AR50_LT		(VIDC_WRAPPER_BASE_OFFS_AR50_LT + 0x00)
#define VIDC_WRAPPER_HW_VERSION_MAJOR_VERSION_MASK_AR50_LT  0x78000000
#define VIDC_WRAPPER_HW_VERSION_MAJOR_VERSION_SHIFT_AR50_LT 28
#define VIDC_WRAPPER_HW_VERSION_MINOR_VERSION_MASK_AR50_LT  0xFFF0000
#define VIDC_WRAPPER_HW_VERSION_MINOR_VERSION_SHIFT_AR50_LT 16
#define VIDC_WRAPPER_HW_VERSION_STEP_VERSION_MASK_AR50_LT   0xFFFF

#define VIDC_WRAPPER_CLOCK_CONFIG_AR50_LT	(VIDC_WRAPPER_BASE_OFFS_AR50_LT + 0x04)

#define VIDC_WRAPPER_INTR_STATUS_AR50_LT	(VIDC_WRAPPER_BASE_OFFS_AR50_LT + 0x0C)
#define VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK_AR50_LT	0x10
#define VIDC_WRAPPER_INTR_STATUS_A2HWD_SHFT_AR50_LT	0x4
#define VIDC_WRAPPER_INTR_STATUS_A2H_BMSK_AR50_LT	0x4
#define VIDC_WRAPPER_INTR_STATUS_A2H_SHFT_AR50_LT	0x2

#define VIDC_WRAPPER_INTR_MASK_AR50_LT		(VIDC_WRAPPER_BASE_OFFS_AR50_LT + 0x10)
#define VIDC_WRAPPER_INTR_MASK_A2HWD_BMSK_AR50_LT	0x10
#define VIDC_WRAPPER_INTR_MASK_A2HWD_SHFT_AR50_LT	0x4
#define VIDC_WRAPPER_INTR_MASK_A2HVCODEC_BMSK_AR50_LT	0x8
#define VIDC_WRAPPER_INTR_MASK_A2HCPU_BMSK_AR50_LT	0x4
#define VIDC_WRAPPER_INTR_MASK_A2HCPU_SHFT_AR50_LT	0x2

#define VIDC_WRAPPER_INTR_CLEAR_A2HWD_BMSK_AR50_LT	0x10
#define VIDC_WRAPPER_INTR_CLEAR_A2HWD_SHFT_AR50_LT	0x4
#define VIDC_WRAPPER_INTR_CLEAR_A2H_BMSK_AR50_LT	0x4
#define VIDC_WRAPPER_INTR_CLEAR_A2H_SHFT_AR50_LT	0x2

/*
 * --------------------------------------------------------------------------
 * MODULE: tz_wrapper
 * --------------------------------------------------------------------------
 */
#define VIDC_WRAPPER_TZ_BASE_OFFS	0x000C0000
#define VIDC_WRAPPER_TZ_CPU_CLOCK_CONFIG	(VIDC_WRAPPER_TZ_BASE_OFFS)
#define VIDC_WRAPPER_TZ_CPU_STATUS	(VIDC_WRAPPER_TZ_BASE_OFFS + 0x10)

#define VIDC_CTRL_INIT_AR50_LT			VIDC_CPU_CS_SCIACMD_AR50_LT

#define VIDC_CTRL_STATUS_AR50_LT		VIDC_CPU_CS_SCIACMDARG0_AR50_LT
#define VIDC_CTRL_ERROR_STATUS__M_AR50_LT \
		VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_ERROR_STATUS_BMSK_AR50_LT
#define VIDC_CTRL_INIT_IDLE_MSG_BMSK_AR50_LT \
		VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_IDLE_MSG_BMSK_AR50_LT
#define VIDC_CTRL_STATUS_PC_READY_AR50_LT \
		VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_PC_READY_AR50_LT

#define VIDC_QTBL_INFO_AR50_LT			VIDC_CPU_CS_SCIACMDARG1_AR50_LT
#define VIDC_QTBL_ADDR_AR50_LT			VIDC_CPU_CS_SCIACMDARG2_AR50_LT
#define VIDC_VERSION_INFO_AR50_LT		VIDC_CPU_CS_SCIACMDARG3_AR50_LT

#define VIDC_SFR_ADDR_AR50_LT			VIDC_CPU_CS_SCIBCMD_AR50_LT
#define VIDC_MMAP_ADDR_AR50_LT			VIDC_CPU_CS_SCIBCMDARG0_AR50_LT
#define VIDC_UC_REGION_ADDR_AR50_LT		VIDC_CPU_CS_SCIBARG1_AR50_LT
#define VIDC_UC_REGION_SIZE_AR50_LT		VIDC_CPU_CS_SCIBARG2_AR50_LT

void __interrupt_init_ar50_lt(struct venus_hfi_device *device, u32 sid)
{
	__write_register(device, VIDC_WRAPPER_INTR_MASK_AR50_LT,
		VIDC_WRAPPER_INTR_MASK_A2HVCODEC_BMSK_AR50_LT, sid);
}

void __setup_ucregion_memory_map_ar50_lt(struct venus_hfi_device *device, u32 sid)
{
	__write_register(device, VIDC_UC_REGION_ADDR_AR50_LT,
			(u32)device->iface_q_table.align_device_addr, sid);
	__write_register(device, VIDC_UC_REGION_SIZE_AR50_LT, SHARED_QSIZE, sid);
	__write_register(device, VIDC_QTBL_ADDR_AR50_LT,
			(u32)device->iface_q_table.align_device_addr, sid);
	__write_register(device, VIDC_QTBL_INFO_AR50_LT, 0x01, sid);
	if (device->sfr.align_device_addr)
		__write_register(device, VIDC_SFR_ADDR_AR50_LT,
				(u32)device->sfr.align_device_addr, sid);
	if (device->qdss.align_device_addr)
		__write_register(device, VIDC_MMAP_ADDR_AR50_LT,
				(u32)device->qdss.align_device_addr, sid);
}

void __power_off_ar50_lt(struct venus_hfi_device *device)
{
	if (!device->power_enabled)
		return;

	if (!(device->intr_status & VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK_AR50_LT))
		disable_irq_nosync(device->hal_data->irq);
	device->intr_status = 0;

	__disable_unprepare_clks(device);
	if (__disable_regulators(device))
		d_vpr_e("Failed to disable regulators\n");

	if (__unvote_buses(device, DEFAULT_SID))
		d_vpr_e("Failed to unvote for buses\n");
	device->power_enabled = false;
}

int __prepare_pc_ar50_lt(struct venus_hfi_device *device)
{
	int rc = 0;
	u32 wfi_status = 0, idle_status = 0, pc_ready = 0;
	u32 ctrl_status = 0;
	u32 count = 0, max_tries = 10;

	ctrl_status = __read_register(device, VIDC_CTRL_STATUS_AR50_LT, DEFAULT_SID);
	pc_ready = ctrl_status & VIDC_CTRL_STATUS_PC_READY_AR50_LT;
	idle_status = ctrl_status & BIT(30);

	if (pc_ready) {
		d_vpr_l("Already in pc_ready state\n");
		return 0;
	}

	wfi_status = BIT(0) & __read_register(device,
			VIDC_WRAPPER_TZ_CPU_STATUS, DEFAULT_SID);
	if (!wfi_status || !idle_status) {
		d_vpr_e("Skipping PC, wfi status not set\n");
		goto skip_power_off;
	}

	rc = __prepare_pc(device);
	if (rc) {
		d_vpr_e("Failed __prepare_pc %d\n", rc);
		goto skip_power_off;
	}

	while (count < max_tries) {
		wfi_status = BIT(0) & __read_register(device,
				VIDC_WRAPPER_TZ_CPU_STATUS, DEFAULT_SID);
		ctrl_status = __read_register(device,
				VIDC_CTRL_STATUS_AR50_LT, DEFAULT_SID);
		pc_ready = ctrl_status & VIDC_CTRL_STATUS_PC_READY_AR50_LT;
		if (wfi_status && pc_ready)
			break;
		usleep_range(150, 250);
		count++;
	}

	if (count == max_tries) {
		d_vpr_e("Skip PC. Core is not in right state\n");
		goto skip_power_off;
	}

	return rc;

skip_power_off:
	d_vpr_e("Skip PC, wfi=%#x, idle=%#x, pcr=%#x, ctrl=%#x)\n",
		wfi_status, idle_status, pc_ready, ctrl_status);
	return -EAGAIN;
}

void __raise_interrupt_ar50_lt(struct venus_hfi_device *device, u32 sid)
{
	__write_register(device, VIDC_CPU_IC_SOFTINT_AR50_LT,
		VIDC_CPU_IC_SOFTINT_H2A_SHFT_AR50_LT, sid);
}

void __core_clear_interrupt_ar50_lt(struct venus_hfi_device *device)
{
	u32 intr_status = 0, mask = 0;

	if (!device) {
		d_vpr_e("%s: NULL device\n", __func__);
		return;
	}

	intr_status = __read_register(device, VIDC_WRAPPER_INTR_STATUS_AR50_LT, DEFAULT_SID);
	mask = (VIDC_WRAPPER_INTR_STATUS_A2H_BMSK_AR50_LT |
		VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK_AR50_LT |
		VIDC_CTRL_INIT_IDLE_MSG_BMSK_AR50_LT);

	if (intr_status & mask) {
		device->intr_status |= intr_status;
		device->reg_count++;
		d_vpr_l(
			"INTERRUPT for device: %pK: times: %d interrupt_status: %d\n",
			device, device->reg_count, intr_status);
	} else {
		device->spur_count++;
	}

	__write_register(device, VIDC_CPU_CS_A2HSOFTINTCLR_AR50_LT, 1, DEFAULT_SID);
}

int __boot_firmware_ar50_lt(struct venus_hfi_device *device, u32 sid)
{
	int rc = 0;
	u32 ctrl_init_val = 0, ctrl_status = 0, count = 0, max_tries = 1000;

	ctrl_init_val = BIT(0);

	__write_register(device, VIDC_CTRL_INIT_AR50_LT, ctrl_init_val, sid);
	while (!ctrl_status && count < max_tries) {
		ctrl_status = __read_register(device, VIDC_CTRL_STATUS_AR50_LT, sid);
		if ((ctrl_status & VIDC_CTRL_ERROR_STATUS__M_AR50_LT) == 0x4) {
			s_vpr_e(sid, "invalid setting for UC_REGION\n");
			break;
		}
		usleep_range(50, 100);
		count++;
	}

	if (count >= max_tries) {
		s_vpr_e(sid, "Error booting up vidc firmware\n");
		rc = -ETIME;
	}

	/* Enable interrupt before sending commands to venus */
	__write_register(device, VIDC_CPU_IC_SOFTINT_EN_AR50_LT, 0x1, sid);
	return rc;
}
