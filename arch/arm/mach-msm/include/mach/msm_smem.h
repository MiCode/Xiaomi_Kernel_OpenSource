/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ARCH_ARM_MACH_MSM_SMEM_H_
#define _ARCH_ARM_MACH_MSM_SMEM_H_

#include <linux/types.h>

enum {
	SMEM_APPS,
	SMEM_MODEM,
	SMEM_Q6,
	SMEM_DSPS,
	SMEM_WCNSS,
	SMEM_MODEM_Q6_FW,
	SMEM_RPM,
	NUM_SMEM_SUBSYSTEMS,
};

#define SMEM_NUM_SMD_STREAM_CHANNELS        64
#define SMEM_NUM_SMD_BLOCK_CHANNELS         64

enum {
	/* fixed items */
	SMEM_PROC_COMM = 0,
	SMEM_HEAP_INFO,
	SMEM_ALLOCATION_TABLE,
	SMEM_VERSION_INFO,
	SMEM_HW_RESET_DETECT,
	SMEM_AARM_WARM_BOOT,
	SMEM_DIAG_ERR_MESSAGE,
	SMEM_SPINLOCK_ARRAY,
	SMEM_MEMORY_BARRIER_LOCATION,
	SMEM_FIXED_ITEM_LAST = SMEM_MEMORY_BARRIER_LOCATION,

	/* dynamic items */
	SMEM_AARM_PARTITION_TABLE,
	SMEM_AARM_BAD_BLOCK_TABLE,
	SMEM_RESERVE_BAD_BLOCKS,
	SMEM_WM_UUID,
	SMEM_CHANNEL_ALLOC_TBL,
	SMEM_SMD_BASE_ID,
	SMEM_SMEM_LOG_IDX = SMEM_SMD_BASE_ID + SMEM_NUM_SMD_STREAM_CHANNELS,
	SMEM_SMEM_LOG_EVENTS,
	SMEM_SMEM_STATIC_LOG_IDX,
	SMEM_SMEM_STATIC_LOG_EVENTS,
	SMEM_SMEM_SLOW_CLOCK_SYNC,
	SMEM_SMEM_SLOW_CLOCK_VALUE,
	SMEM_BIO_LED_BUF,
	SMEM_SMSM_SHARED_STATE,
	SMEM_SMSM_INT_INFO,
	SMEM_SMSM_SLEEP_DELAY,
	SMEM_SMSM_LIMIT_SLEEP,
	SMEM_SLEEP_POWER_COLLAPSE_DISABLED,
	SMEM_KEYPAD_KEYS_PRESSED,
	SMEM_KEYPAD_STATE_UPDATED,
	SMEM_KEYPAD_STATE_IDX,
	SMEM_GPIO_INT,
	SMEM_MDDI_LCD_IDX,
	SMEM_MDDI_HOST_DRIVER_STATE,
	SMEM_MDDI_LCD_DISP_STATE,
	SMEM_LCD_CUR_PANEL,
	SMEM_MARM_BOOT_SEGMENT_INFO,
	SMEM_AARM_BOOT_SEGMENT_INFO,
	SMEM_SLEEP_STATIC,
	SMEM_SCORPION_FREQUENCY,
	SMEM_SMD_PROFILES,
	SMEM_TSSC_BUSY,
	SMEM_HS_SUSPEND_FILTER_INFO,
	SMEM_BATT_INFO,
	SMEM_APPS_BOOT_MODE,
	SMEM_VERSION_FIRST,
	SMEM_VERSION_SMD = SMEM_VERSION_FIRST,
	SMEM_VERSION_LAST = SMEM_VERSION_FIRST + 24,
	SMEM_OSS_RRCASN1_BUF1,
	SMEM_OSS_RRCASN1_BUF2,
	SMEM_ID_VENDOR0,
	SMEM_ID_VENDOR1,
	SMEM_ID_VENDOR2,
	SMEM_HW_SW_BUILD_ID,
	SMEM_SMD_BLOCK_PORT_BASE_ID,
	SMEM_SMD_BLOCK_PORT_PROC0_HEAP = SMEM_SMD_BLOCK_PORT_BASE_ID +
						SMEM_NUM_SMD_BLOCK_CHANNELS,
	SMEM_SMD_BLOCK_PORT_PROC1_HEAP = SMEM_SMD_BLOCK_PORT_PROC0_HEAP +
						SMEM_NUM_SMD_BLOCK_CHANNELS,
	SMEM_I2C_MUTEX = SMEM_SMD_BLOCK_PORT_PROC1_HEAP +
						SMEM_NUM_SMD_BLOCK_CHANNELS,
	SMEM_SCLK_CONVERSION,
	SMEM_SMD_SMSM_INTR_MUX,
	SMEM_SMSM_CPU_INTR_MASK,
	SMEM_APPS_DEM_SLAVE_DATA,
	SMEM_QDSP6_DEM_SLAVE_DATA,
	SMEM_CLKREGIM_BSP,
	SMEM_CLKREGIM_SOURCES,
	SMEM_SMD_FIFO_BASE_ID,
	SMEM_USABLE_RAM_PARTITION_TABLE = SMEM_SMD_FIFO_BASE_ID +
						SMEM_NUM_SMD_STREAM_CHANNELS,
	SMEM_POWER_ON_STATUS_INFO,
	SMEM_DAL_AREA,
	SMEM_SMEM_LOG_POWER_IDX,
	SMEM_SMEM_LOG_POWER_WRAP,
	SMEM_SMEM_LOG_POWER_EVENTS,
	SMEM_ERR_CRASH_LOG,
	SMEM_ERR_F3_TRACE_LOG,
	SMEM_SMD_BRIDGE_ALLOC_TABLE,
	SMEM_SMDLITE_TABLE,
	SMEM_SD_IMG_UPGRADE_STATUS,
	SMEM_SEFS_INFO,
	SMEM_RESET_LOG,
	SMEM_RESET_LOG_SYMBOLS,
	SMEM_MODEM_SW_BUILD_ID,
	SMEM_SMEM_LOG_MPROC_WRAP,
	SMEM_BOOT_INFO_FOR_APPS,
	SMEM_SMSM_SIZE_INFO,
	SMEM_SMD_LOOPBACK_REGISTER,
	SMEM_SSR_REASON_MSS0,
	SMEM_SSR_REASON_WCNSS0,
	SMEM_SSR_REASON_LPASS0,
	SMEM_SSR_REASON_DSPS0,
	SMEM_SSR_REASON_VCODEC0,
	SMEM_SMP2P_APPS_BASE = 427,
	SMEM_SMP2P_MODEM_BASE = SMEM_SMP2P_APPS_BASE + 8,    /* 435 */
	SMEM_SMP2P_AUDIO_BASE = SMEM_SMP2P_MODEM_BASE + 8,   /* 443 */
	SMEM_SMP2P_WIRLESS_BASE = SMEM_SMP2P_AUDIO_BASE + 8, /* 451 */
	SMEM_SMP2P_POWER_BASE = SMEM_SMP2P_WIRLESS_BASE + 8, /* 459 */
	SMEM_FLASH_DEVICE_INFO = SMEM_SMP2P_POWER_BASE + 8,  /* 467 */
	SMEM_BAM_PIPE_MEMORY,     /* 468 */
	SMEM_IMAGE_VERSION_TABLE, /* 469 */
	SMEM_LC_DEBUGGER, /* 470 */
	SMEM_NUM_ITEMS,
};

#ifdef CONFIG_MSM_SMD
void *smem_alloc(unsigned id, unsigned size);
void *smem_alloc2(unsigned id, unsigned size_in);
void *smem_get_entry(unsigned id, unsigned *size);
void *smem_find(unsigned id, unsigned size);

/**
 * smem_get_entry_no_rlock - Get existing item without using remote spinlock
 *
 * @id:       ID of SMEM item
 * @size_out: Pointer to size variable for storing the result
 * @returns:  Pointer to SMEM item or NULL if it doesn't exist
 *
 * This function does not lock the remote spinlock and should only be used in
 * failure-recover cases such as retrieving the subsystem failure reason during
 * subsystem restart.
 */
void *smem_get_entry_no_rlock(unsigned id, unsigned *size_out);

/**
 * smem_virt_to_phys() - Convert SMEM address to physical address.
 *
 * @smem_address: Virtual address returned by smem_alloc()/smem_alloc2()
 * @returns: Physical address (or NULL if there is a failure)
 *
 * This function should only be used if an SMEM item needs to be handed
 * off to a DMA engine.
 */
phys_addr_t smem_virt_to_phys(void *smem_address);

#else
static inline void *smem_alloc(unsigned id, unsigned size)
{
	return NULL;
}
static inline void *smem_alloc2(unsigned id, unsigned size_in)
{
	return NULL;
}
static inline void *smem_get_entry(unsigned id, unsigned *size)
{
	return NULL;
}
static inline void *smem_find(unsigned id, unsigned size)
{
	return NULL;
}
void *smem_get_entry_no_rlock(unsigned id, unsigned *size_out)
{
	return NULL;
}
static inline phys_addr_t smem_virt_to_phys(void *smem_address)
{
	return (phys_addr_t) NULL;
}
#endif /* CONFIG_MSM_SMD  */
#endif /* _ARCH_ARM_MACH_MSM_SMEM_H_ */
