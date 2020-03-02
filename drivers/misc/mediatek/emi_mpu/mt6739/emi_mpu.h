/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __MT_EMI_MPU_H
#define __MT_EMI_MPU_H

#define ENABLE_AP_REGION	1

#define EMI_MPUD0_ST            (CEN_EMI_BASE + 0x160)
#define EMI_MPUD_ST(domain)     (EMI_MPUD0_ST + (4*domain)) /* violation status domain */
#define EMI_MPUD0_ST2           (CEN_EMI_BASE + 0x200)     /* violation status domain */
#define EMI_MPUD_ST2(domain)    (EMI_MPUD0_ST2 + (4*domain))/* violation status domain */
#define EMI_MPUS                (CEN_EMI_BASE + 0x01F0)    /* Memory protect unit control registers S */
#define EMI_MPUT                (CEN_EMI_BASE + 0x01F8)    /* Memory protect unit control registers S */
#define EMI_MPUT_2ND            (CEN_EMI_BASE + 0x01FC)    /* Memory protect unit control registers S */

#define EMI_MPU_CTRL                 (0x0)
#define EMI_MPU_DBG                  (0x4)
#define EMI_MPU_SA0                  (0x100)
#define EMI_MPU_EA0                  (0x200)
#define EMI_MPU_SA(region)           (EMI_MPU_SA0 + (region*4))
#define EMI_MPU_EA(region)           (EMI_MPU_EA0 + (region*4))
#define EMI_MPU_APC0                 (0x300)
#define EMI_MPU_APC(region, domain)  (EMI_MPU_APC0 + (region*4) + ((domain/8)*0x100))
#define EMI_MPU_CTRL_D0              (0x800)
#define EMI_MPU_CTRL_D(domain)       (EMI_MPU_CTRL_D0 + (domain*4))
#define EMI_RG_MASK_D0               (0x900)
#define EMI_RG_MASK_D(domain)        (EMI_RG_MASK_D0 + (domain*4))

#define NO_PROTECTION   0
#define SEC_RW          1
#define SEC_RW_NSEC_R   2
#define SEC_RW_NSEC_W   3
#define SEC_R_NSEC_R    4
#define FORBIDDEN       5
#define SEC_R_NSEC_RW   6

#define EN_MPU_STR "ON"
#define DIS_MPU_STR "OFF"

#define MAX_CHANNELS  (1)
#define MAX_RANKS     (2)

#define EMI_MPU_REGION_NUMBER	(24)
#define EMI_MPU_DOMAIN_NUMBER	(8)

enum {
	AXI_VIO_ID = 0,
	AXI_ADR_CHK_EN = 16,
	AXI_LOCK_CHK_EN = 17,
	AXI_NON_ALIGN_CHK_EN = 18,
	AXI_NON_ALIGN_CHK_MST = 20,
	AXI_VIO_CLR = 24,
	AXI_VIO_WR = 27,
	AXI_ADR_VIO = 28,
	AXI_LOCK_ISSUE = 29,
	AXI_NON_ALIGN_ISSUE = 30
};

enum {
	MASTER_APMCU = 0,
	MASTER_MM = 1,
	MASTER_INFRA = 2,
	MASTER_MDMCU = 3,
	MASTER_MDDMA = 4,
	MASTER_MFG = 5,
	MASTER_ALL = 6
};

/* Basic DRAM setting */
struct basic_dram_setting {
	/* Number of channels */
	unsigned channel_nr;
	/* Per-channel information */
	struct {
		/* Per-rank information */
		struct {
			/* Does this rank exist */
			bool valid_rank;
			/* Rank size - (in Gb)*/
			unsigned rank_size;
			/* Number of segments */
			unsigned segment_nr;
		} rank[MAX_RANKS];
	} channel[MAX_CHANNELS];
};

#define LOCK	1
#define UNLOCK	0

#define SET_ACCESS_PERMISSON(lock, d7, d6, d5, d4, d3, d2, d1, d0) \
((((unsigned int) d7) << 21) | (((unsigned int) d6) << 18) | (((unsigned int) d5) << 15) | \
(((unsigned int) d4)  << 12) | (((unsigned int) d3) <<  9) | (((unsigned int) d2) <<  6) | \
(((unsigned int) d1)  <<  3) | ((unsigned int) d0) | ((unsigned long long) lock << 26))

extern int emi_mpu_set_region_protection(unsigned long long start_addr,
	unsigned long long end_addr, int region, unsigned int access_permission);
#if defined(CONFIG_MTKPASR)
extern void acquire_dram_setting(struct basic_dram_setting *pasrdpd);
#endif
extern unsigned int mt_emi_mpu_irq_get(void);
extern void mt_emi_reg_base_set(void *base);
extern void *mt_emi_reg_base_get(void);
extern int emi_mpu_get_violation_port(void);
extern phys_addr_t get_max_DRAM_size(void);
extern unsigned int mt_emi_reg_read(unsigned int offset);
extern void mt_emi_reg_write(unsigned int data, unsigned int offset);

#endif  /* !__MT_EMI_MPU_H */
