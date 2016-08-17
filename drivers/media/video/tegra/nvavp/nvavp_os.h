/*
 * drivers/media/video/tegra/nvavp/nvavp_os.h
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef __MEDIA_VIDEO_TEGRA_NVAVP_OS_H
#define __MEDIA_VIDEO_TEGRA_NVAVP_OS_H

#include <linux/types.h>

#include "../../../../video/tegra/nvmap/nvmap.h"

#define NVE2_AVP				(0x0000E276)

struct nv_e276_control {
	u32 reserved00[5];
	u32 dma_start;
	u32 reserved01[2];
	u32 dma_end;
	u32 reserved02[7];
	u32 put;
	u32 reserved03[15];
	u32 get;
	u32 reserved04[9];
	u32 sync_pt_incr_trap_enable;
	u32 watchdog_timeout;
	u32 idle_notify_enable;
	u32 idle_notify_delay;
	u32 idle_clk_enable;
	u32 iram_clk_gating;
	u32 idle;
	u32 outbox_data;
	u32 app_intr_enable;
	u32 app_start_time;
	u32 app_in_iram;
	u32 iram_ucode_addr;
	u32 iram_ucode_size;
	u32 dbg_state[57];
	u32 os_method_data[16];
	u32 app_method_data[128];
};

#define NVE26E_HOST1X_INCR_SYNCPT		(0x00000000)
#define NVE26E_HOST1X_INCR_SYNCPT_COND_OP_DONE	(0x00000001)

#define NVE26E_CH_OPCODE_INCR(Addr, Count) \
	/* op, addr, count */ \
	((1UL << 28) | ((Addr) << 16) | (Count))

#define NVE26E_CH_OPCODE_IMM(addr, value) \
	/* op, addr, count */ \
	((4UL << 28) | ((addr) << 16) | (value))

#define NVE26E_CH_OPCODE_GATHER(off, ins, type, cnt) \
	/* op, offset, insert, type, count */ \
	((6UL << 28) | ((off) << 16) | ((ins) << 15) | ((type) << 14) | cnt)

/* AVP OS methods */
#define NVE276_NOP				(0x00000080)
#define NVE276_SET_APP_TIMEOUT			(0x00000084)
#define NVE276_SET_MICROCODE_A			(0x00000085)
#define NVE276_SET_MICROCODE_B			(0x00000086)
#define NVE276_SET_MICROCODE_C			(0x00000087)

/* Interrupt codes through inbox/outbox data codes (cpu->avp or avp->cpu) */
#define NVE276_OS_INTERRUPT_NOP			(0x00000000) /* wake up avp */
#define NVE276_OS_INTERRUPT_TIMEOUT		(0x00000001)
#define NVE276_OS_INTERRUPT_SEMAPHORE_AWAKEN	(0x00000002)
#define NVE276_OS_INTERRUPT_EXECUTE_AWAKEN	(0x00000004)
#define NVE276_OS_INTERRUPT_DEBUG_STRING	(0x00000008)
#define NVE276_OS_INTERRUPT_DH_KEYEXCHANGE	(0x00000010)
#define NVE276_OS_INTERRUPT_APP_NOTIFY		(0x00000020)
#define NVE276_OS_INTERRUPT_VIDEO_IDLE		(0x00000040)
#define NVE276_OS_INTERRUPT_AUDIO_IDLE		(0x00000080)
#define NVE276_OS_INTERRUPT_SYNCPT_INCR_TRAP	(0x00002000)
#define NVE276_OS_INTERRUPT_AVP_BREAKPOINT	(0x00800000)
#define NVE276_OS_INTERRUPT_AVP_FATAL_ERROR	(0x01000000)

/* Get sync point id from avp->cpu data */
#define NVE276_OS_SYNCPT_INCR_TRAP_GET_SYNCPT(x)	(((x)>>14)&0x1f)

struct nvavp_os_info {
	u32			entry_offset;
	u32			control_offset;
	u32			debug_offset;

	struct nvmap_handle_ref	*handle;
	void			*data;
	u32			size;
	phys_addr_t		phys;
	void			*os_bin;
	phys_addr_t		reset_addr;
};

struct nvavp_ucode_info {
	struct nvmap_handle_ref	*handle;
	void			*data;
	u32			size;
	phys_addr_t		phys;
	void			*ucode_bin;
};

#endif /* __MEDIA_VIDEO_TEGRA_NVAVP_OS_H */
