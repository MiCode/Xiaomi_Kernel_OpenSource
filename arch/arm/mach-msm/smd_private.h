/* arch/arm/mach-msm/smd_private.h
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2012, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _ARCH_ARM_MACH_MSM_MSM_SMD_PRIVATE_H_
#define _ARCH_ARM_MACH_MSM_MSM_SMD_PRIVATE_H_

#include <linux/types.h>
#include <linux/spinlock.h>
#include <mach/msm_smsm.h>
#include <mach/msm_smd.h>

#define PC_APPS  0
#define PC_MODEM 1

#define VERSION_QDSP6     4
#define VERSION_APPS_SBL  6
#define VERSION_MODEM_SBL 7
#define VERSION_APPS      8
#define VERSION_MODEM     9
#define VERSION_DSPS      10

#define SMD_HEAP_SIZE 512

struct smem_heap_info {
	unsigned initialized;
	unsigned free_offset;
	unsigned heap_remaining;
	unsigned reserved;
};

struct smem_heap_entry {
	unsigned allocated;
	unsigned offset;
	unsigned size;
	unsigned reserved; /* bits 1:0 reserved, bits 31:2 aux smem base addr */
};
#define BASE_ADDR_MASK 0xfffffffc

struct smem_proc_comm {
	unsigned command;
	unsigned status;
	unsigned data1;
	unsigned data2;
};

struct smem_shared {
	struct smem_proc_comm proc_comm[4];
	unsigned version[32];
	struct smem_heap_info heap_info;
	struct smem_heap_entry heap_toc[SMD_HEAP_SIZE];
};

#if defined(CONFIG_MSM_SMD_PKG4)
struct smsm_interrupt_info {
	uint32_t aArm_en_mask;
	uint32_t aArm_interrupts_pending;
	uint32_t aArm_wakeup_reason;
	uint32_t aArm_rpc_prog;
	uint32_t aArm_rpc_proc;
	char aArm_smd_port_name[20];
	uint32_t aArm_gpio_info;
};
#elif defined(CONFIG_MSM_SMD_PKG3)
struct smsm_interrupt_info {
  uint32_t aArm_en_mask;
  uint32_t aArm_interrupts_pending;
  uint32_t aArm_wakeup_reason;
};
#elif !defined(CONFIG_MSM_SMD)
/* Don't trigger the error */
#else
#error No SMD Package Specified; aborting
#endif

#define SZ_DIAG_ERR_MSG 0xC8
#define ID_DIAG_ERR_MSG SMEM_DIAG_ERR_MESSAGE
#define ID_SMD_CHANNELS SMEM_SMD_BASE_ID
#define ID_SHARED_STATE SMEM_SMSM_SHARED_STATE
#define ID_CH_ALLOC_TBL SMEM_CHANNEL_ALLOC_TBL

#define SMD_SS_CLOSED            0x00000000
#define SMD_SS_OPENING           0x00000001
#define SMD_SS_OPENED            0x00000002
#define SMD_SS_FLUSHING          0x00000003
#define SMD_SS_CLOSING           0x00000004
#define SMD_SS_RESET             0x00000005
#define SMD_SS_RESET_OPENING     0x00000006

#define SMD_BUF_SIZE             8192
#define SMD_CHANNELS             64
#define SMD_HEADER_SIZE          20

/* 'type' field of smd_alloc_elm structure
 * has the following breakup
 * bits 0-7   -> channel type
 * bits 8-11  -> xfer type
 * bits 12-31 -> reserved
 */
struct smd_alloc_elm {
	char name[20];
	uint32_t cid;
	uint32_t type;
	uint32_t ref_count;
};

#define SMD_CHANNEL_TYPE(x) ((x) & 0x000000FF)
#define SMD_XFER_TYPE(x)    (((x) & 0x00000F00) >> 8)

struct smd_half_channel {
	unsigned state;
	unsigned char fDSR;
	unsigned char fCTS;
	unsigned char fCD;
	unsigned char fRI;
	unsigned char fHEAD;
	unsigned char fTAIL;
	unsigned char fSTATE;
	unsigned char fBLOCKREADINTR;
	unsigned tail;
	unsigned head;
};

struct smd_half_channel_word_access {
	unsigned state;
	unsigned fDSR;
	unsigned fCTS;
	unsigned fCD;
	unsigned fRI;
	unsigned fHEAD;
	unsigned fTAIL;
	unsigned fSTATE;
	unsigned fBLOCKREADINTR;
	unsigned tail;
	unsigned head;
};

struct smd_half_channel_access {
	void (*set_state)(volatile void *half_channel, unsigned data);
	unsigned (*get_state)(volatile void *half_channel);
	void (*set_fDSR)(volatile void *half_channel, unsigned char data);
	unsigned (*get_fDSR)(volatile void *half_channel);
	void (*set_fCTS)(volatile void *half_channel, unsigned char data);
	unsigned (*get_fCTS)(volatile void *half_channel);
	void (*set_fCD)(volatile void *half_channel, unsigned char data);
	unsigned (*get_fCD)(volatile void *half_channel);
	void (*set_fRI)(volatile void *half_channel, unsigned char data);
	unsigned (*get_fRI)(volatile void *half_channel);
	void (*set_fHEAD)(volatile void *half_channel, unsigned char data);
	unsigned (*get_fHEAD)(volatile void *half_channel);
	void (*set_fTAIL)(volatile void *half_channel, unsigned char data);
	unsigned (*get_fTAIL)(volatile void *half_channel);
	void (*set_fSTATE)(volatile void *half_channel, unsigned char data);
	unsigned (*get_fSTATE)(volatile void *half_channel);
	void (*set_fBLOCKREADINTR)(volatile void *half_channel,
					unsigned char data);
	unsigned (*get_fBLOCKREADINTR)(volatile void *half_channel);
	void (*set_tail)(volatile void *half_channel, unsigned data);
	unsigned (*get_tail)(volatile void *half_channel);
	void (*set_head)(volatile void *half_channel, unsigned data);
	unsigned (*get_head)(volatile void *half_channel);
};

int is_word_access_ch(unsigned ch_type);

struct smd_half_channel_access *get_half_ch_funcs(unsigned ch_type);

struct smem_ram_ptn {
	char name[16];
	unsigned start;
	unsigned size;

	/* RAM Partition attribute: READ_ONLY, READWRITE etc.  */
	unsigned attr;

	/* RAM Partition category: EBI0, EBI1, IRAM, IMEM */
	unsigned category;

	/* RAM Partition domain: APPS, MODEM, APPS & MODEM (SHARED) etc. */
	unsigned domain;

	/* RAM Partition type: system, bootloader, appsboot, apps etc. */
	unsigned type;

	/* reserved for future expansion without changing version number */
	unsigned reserved2, reserved3, reserved4, reserved5;
} __attribute__ ((__packed__));


struct smem_ram_ptable {
	#define _SMEM_RAM_PTABLE_MAGIC_1 0x9DA5E0A8
	#define _SMEM_RAM_PTABLE_MAGIC_2 0xAF9EC4E2
	unsigned magic[2];
	unsigned version;
	unsigned reserved1;
	unsigned len;
	struct smem_ram_ptn parts[32];
	unsigned buf;
} __attribute__ ((__packed__));

/* SMEM RAM Partition */
enum {
	DEFAULT_ATTRB = ~0x0,
	READ_ONLY = 0x0,
	READWRITE,
};

enum {
	DEFAULT_CATEGORY = ~0x0,
	SMI = 0x0,
	EBI1,
	EBI2,
	QDSP6,
	IRAM,
	IMEM,
	EBI0_CS0,
	EBI0_CS1,
	EBI1_CS0,
	EBI1_CS1,
	SDRAM = 0xE,
};

enum {
	DEFAULT_DOMAIN = 0x0,
	APPS_DOMAIN,
	MODEM_DOMAIN,
	SHARED_DOMAIN,
};

enum {
	SYS_MEMORY = 1,        /* system memory*/
	BOOT_REGION_MEMORY1,   /* boot loader memory 1*/
	BOOT_REGION_MEMORY2,   /* boot loader memory 2,reserved*/
	APPSBL_MEMORY,         /* apps boot loader memory*/
	APPS_MEMORY,           /* apps  usage memory*/
};

extern spinlock_t smem_lock;


void smd_diag(void);

struct interrupt_stat {
	uint32_t smd_in_count;
	uint32_t smd_out_hardcode_count;
	uint32_t smd_out_config_count;

	uint32_t smsm_in_count;
	uint32_t smsm_out_hardcode_count;
	uint32_t smsm_out_config_count;
};
extern struct interrupt_stat interrupt_stats[NUM_SMD_SUBSYSTEMS];

#endif
