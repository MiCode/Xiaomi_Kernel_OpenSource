/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/hardware/cp14.h>

static unsigned int etm_read_reg(uint32_t reg)
{
	switch (reg) {
	case 0x0:
		return etm_read(ETMCR);
	case 0x1:
		return etm_read(ETMCCR);
	case 0x2:
		return etm_read(ETMTRIGGER);
	case 0x4:
		return etm_read(ETMSR);
	case 0x5:
		return etm_read(ETMSCR);
	case 0x6:
		return etm_read(ETMTSSCR);
	case 0x8:
		return etm_read(ETMTEEVR);
	case 0x9:
		return etm_read(ETMTECR1);
	case 0xB:
		return etm_read(ETMFFLR);
	case 0x10:
		return etm_read(ETMACVR0);
	case 0x11:
		return etm_read(ETMACVR1);
	case 0x12:
		return etm_read(ETMACVR2);
	case 0x13:
		return etm_read(ETMACVR3);
	case 0x14:
		return etm_read(ETMACVR4);
	case 0x15:
		return etm_read(ETMACVR5);
	case 0x16:
		return etm_read(ETMACVR6);
	case 0x17:
		return etm_read(ETMACVR7);
	case 0x18:
		return etm_read(ETMACVR8);
	case 0x19:
		return etm_read(ETMACVR9);
	case 0x1A:
		return etm_read(ETMACVR10);
	case 0x1B:
		return etm_read(ETMACVR11);
	case 0x1C:
		return etm_read(ETMACVR12);
	case 0x1D:
		return etm_read(ETMACVR13);
	case 0x1E:
		return etm_read(ETMACVR14);
	case 0x1F:
		return etm_read(ETMACVR15);
	case 0x20:
		return etm_read(ETMACTR0);
	case 0x21:
		return etm_read(ETMACTR1);
	case 0x22:
		return etm_read(ETMACTR2);
	case 0x23:
		return etm_read(ETMACTR3);
	case 0x24:
		return etm_read(ETMACTR4);
	case 0x25:
		return etm_read(ETMACTR5);
	case 0x26:
		return etm_read(ETMACTR6);
	case 0x27:
		return etm_read(ETMACTR7);
	case 0x28:
		return etm_read(ETMACTR8);
	case 0x29:
		return etm_read(ETMACTR9);
	case 0x2A:
		return etm_read(ETMACTR10);
	case 0x2B:
		return etm_read(ETMACTR11);
	case 0x2C:
		return etm_read(ETMACTR12);
	case 0x2D:
		return etm_read(ETMACTR13);
	case 0x2E:
		return etm_read(ETMACTR14);
	case 0x2F:
		return etm_read(ETMACTR15);
	case 0x50:
		return etm_read(ETMCNTRLDVR0);
	case 0x51:
		return etm_read(ETMCNTRLDVR1);
	case 0x52:
		return etm_read(ETMCNTRLDVR2);
	case 0x53:
		return etm_read(ETMCNTRLDVR3);
	case 0x54:
		return etm_read(ETMCNTENR0);
	case 0x55:
		return etm_read(ETMCNTENR1);
	case 0x56:
		return etm_read(ETMCNTENR2);
	case 0x57:
		return etm_read(ETMCNTENR3);
	case 0x58:
		return etm_read(ETMCNTRLDEVR0);
	case 0x59:
		return etm_read(ETMCNTRLDEVR1);
	case 0x5A:
		return etm_read(ETMCNTRLDEVR2);
	case 0x5B:
		return etm_read(ETMCNTRLDEVR3);
	case 0x5C:
		return etm_read(ETMCNTVR0);
	case 0x5D:
		return etm_read(ETMCNTVR1);
	case 0x5E:
		return etm_read(ETMCNTVR2);
	case 0x5F:
		return etm_read(ETMCNTVR3);
	case 0x60:
		return etm_read(ETMSQ12EVR);
	case 0x61:
		return etm_read(ETMSQ21EVR);
	case 0x62:
		return etm_read(ETMSQ23EVR);
	case 0x63:
		return etm_read(ETMSQ31EVR);
	case 0x64:
		return etm_read(ETMSQ32EVR);
	case 0x65:
		return etm_read(ETMSQ13EVR);
	case 0x67:
		return etm_read(ETMSQR);
	case 0x68:
		return etm_read(ETMEXTOUTEVR0);
	case 0x69:
		return etm_read(ETMEXTOUTEVR1);
	case 0x6A:
		return etm_read(ETMEXTOUTEVR2);
	case 0x6B:
		return etm_read(ETMEXTOUTEVR3);
	case 0x6C:
		return etm_read(ETMCIDCVR0);
	case 0x6D:
		return etm_read(ETMCIDCVR1);
	case 0x6E:
		return etm_read(ETMCIDCVR2);
	case 0x6F:
		return etm_read(ETMCIDCMR);
	case 0x70:
		return etm_read(ETMIMPSPEC0);
	case 0x71:
		return etm_read(ETMIMPSPEC1);
	case 0x72:
		return etm_read(ETMIMPSPEC2);
	case 0x73:
		return etm_read(ETMIMPSPEC3);
	case 0x74:
		return etm_read(ETMIMPSPEC4);
	case 0x75:
		return etm_read(ETMIMPSPEC5);
	case 0x76:
		return etm_read(ETMIMPSPEC6);
	case 0x77:
		return etm_read(ETMIMPSPEC7);
	case 0x78:
		return etm_read(ETMSYNCFR);
	case 0x79:
		return etm_read(ETMIDR);
	case 0x7A:
		return etm_read(ETMCCER);
	case 0x7B:
		return etm_read(ETMEXTINSELR);
	case 0x7C:
		return etm_read(ETMTESSEICR);
	case 0x7D:
		return etm_read(ETMEIBCR);
	case 0x7E:
		return etm_read(ETMTSEVR);
	case 0x7F:
		return etm_read(ETMAUXCR);
	case 0x80:
		return etm_read(ETMTRACEIDR);
	case 0x90:
		return etm_read(ETMVMIDCVR);
	case 0xC1:
		return etm_read(ETMOSLSR);
	case 0xC2:
		return etm_read(ETMOSSRR);
	case 0xC4:
		return etm_read(ETMPDCR);
	case 0xC5:
		return etm_read(ETMPDSR);
	default:
		WARN(1, "invalid CP14 access to ETM reg: %lx",
							(unsigned long)reg);
		return 0;
	}
}

static void etm_write_reg(uint32_t val, uint32_t reg)
{
	switch (reg) {
	case 0x0:
		etm_write(val, ETMCR);
		return;
	case 0x2:
		etm_write(val, ETMTRIGGER);
		return;
	case 0x4:
		etm_write(val, ETMSR);
		return;
	case 0x6:
		etm_write(val, ETMTSSCR);
		return;
	case 0x8:
		etm_write(val, ETMTEEVR);
		return;
	case 0x9:
		etm_write(val, ETMTECR1);
		return;
	case 0xB:
		etm_write(val, ETMFFLR);
		return;
	case 0x10:
		etm_write(val, ETMACVR0);
		return;
	case 0x11:
		etm_write(val, ETMACVR1);
		return;
	case 0x12:
		etm_write(val, ETMACVR2);
		return;
	case 0x13:
		etm_write(val, ETMACVR3);
		return;
	case 0x14:
		etm_write(val, ETMACVR4);
		return;
	case 0x15:
		etm_write(val, ETMACVR5);
		return;
	case 0x16:
		etm_write(val, ETMACVR6);
		return;
	case 0x17:
		etm_write(val, ETMACVR7);
		return;
	case 0x18:
		etm_write(val, ETMACVR8);
		return;
	case 0x19:
		etm_write(val, ETMACVR9);
		return;
	case 0x1A:
		etm_write(val, ETMACVR10);
		return;
	case 0x1B:
		etm_write(val, ETMACVR11);
		return;
	case 0x1C:
		etm_write(val, ETMACVR12);
		return;
	case 0x1D:
		etm_write(val, ETMACVR13);
		return;
	case 0x1E:
		etm_write(val, ETMACVR14);
		return;
	case 0x1F:
		etm_write(val, ETMACVR15);
		return;
	case 0x20:
		etm_write(val, ETMACTR0);
		return;
	case 0x21:
		etm_write(val, ETMACTR1);
		return;
	case 0x22:
		etm_write(val, ETMACTR2);
		return;
	case 0x23:
		etm_write(val, ETMACTR3);
		return;
	case 0x24:
		etm_write(val, ETMACTR4);
		return;
	case 0x25:
		etm_write(val, ETMACTR5);
		return;
	case 0x26:
		etm_write(val, ETMACTR6);
		return;
	case 0x27:
		etm_write(val, ETMACTR7);
		return;
	case 0x28:
		etm_write(val, ETMACTR8);
		return;
	case 0x29:
		etm_write(val, ETMACTR9);
		return;
	case 0x2A:
		etm_write(val, ETMACTR10);
		return;
	case 0x2B:
		etm_write(val, ETMACTR11);
		return;
	case 0x2C:
		etm_write(val, ETMACTR12);
		return;
	case 0x2D:
		etm_write(val, ETMACTR13);
		return;
	case 0x2E:
		etm_write(val, ETMACTR14);
		return;
	case 0x2F:
		etm_write(val, ETMACTR15);
		return;
	case 0x50:
		etm_write(val, ETMCNTRLDVR0);
		return;
	case 0x51:
		etm_write(val, ETMCNTRLDVR1);
		return;
	case 0x52:
		etm_write(val, ETMCNTRLDVR2);
		return;
	case 0x53:
		etm_write(val, ETMCNTRLDVR3);
		return;
	case 0x54:
		etm_write(val, ETMCNTENR0);
		return;
	case 0x55:
		etm_write(val, ETMCNTENR1);
		return;
	case 0x56:
		etm_write(val, ETMCNTENR2);
		return;
	case 0x57:
		etm_write(val, ETMCNTENR3);
		return;
	case 0x58:
		etm_write(val, ETMCNTRLDEVR0);
		return;
	case 0x59:
		etm_write(val, ETMCNTRLDEVR1);
		return;
	case 0x5A:
		etm_write(val, ETMCNTRLDEVR2);
		return;
	case 0x5B:
		etm_write(val, ETMCNTRLDEVR3);
		return;
	case 0x5C:
		etm_write(val, ETMCNTVR0);
		return;
	case 0x5D:
		etm_write(val, ETMCNTVR1);
		return;
	case 0x5E:
		etm_write(val, ETMCNTVR2);
		return;
	case 0x5F:
		etm_write(val, ETMCNTVR3);
		return;
	case 0x60:
		etm_write(val, ETMSQ12EVR);
		return;
	case 0x61:
		etm_write(val, ETMSQ21EVR);
		return;
	case 0x62:
		etm_write(val, ETMSQ23EVR);
		return;
	case 0x63:
		etm_write(val, ETMSQ31EVR);
		return;
	case 0x64:
		etm_write(val, ETMSQ32EVR);
		return;
	case 0x65:
		etm_write(val, ETMSQ13EVR);
		return;
	case 0x67:
		etm_write(val, ETMSQR);
		return;
	case 0x68:
		etm_write(val, ETMEXTOUTEVR0);
		return;
	case 0x69:
		etm_write(val, ETMEXTOUTEVR1);
		return;
	case 0x6A:
		etm_write(val, ETMEXTOUTEVR2);
		return;
	case 0x6B:
		etm_write(val, ETMEXTOUTEVR3);
		return;
	case 0x6C:
		etm_write(val, ETMCIDCVR0);
		return;
	case 0x6D:
		etm_write(val, ETMCIDCVR1);
		return;
	case 0x6E:
		etm_write(val, ETMCIDCVR2);
		return;
	case 0x6F:
		etm_write(val, ETMCIDCMR);
		return;
	case 0x70:
		etm_write(val, ETMIMPSPEC0);
		return;
	case 0x71:
		etm_write(val, ETMIMPSPEC1);
		return;
	case 0x72:
		etm_write(val, ETMIMPSPEC2);
		return;
	case 0x73:
		etm_write(val, ETMIMPSPEC3);
		return;
	case 0x74:
		etm_write(val, ETMIMPSPEC4);
		return;
	case 0x75:
		etm_write(val, ETMIMPSPEC5);
		return;
	case 0x76:
		etm_write(val, ETMIMPSPEC6);
		return;
	case 0x77:
		etm_write(val, ETMIMPSPEC7);
		return;
	case 0x78:
		etm_write(val, ETMSYNCFR);
		return;
	case 0x7B:
		etm_write(val, ETMEXTINSELR);
		return;
	case 0x7C:
		etm_write(val, ETMTESSEICR);
		return;
	case 0x7D:
		etm_write(val, ETMEIBCR);
		return;
	case 0x7E:
		etm_write(val, ETMTSEVR);
		return;
	case 0x7F:
		etm_write(val, ETMAUXCR);
		return;
	case 0x80:
		etm_write(val, ETMTRACEIDR);
		return;
	case 0x90:
		etm_write(val, ETMVMIDCVR);
		return;
	case 0xC0:
		etm_write(val, ETMOSLAR);
		return;
	case 0xC2:
		etm_write(val, ETMOSSRR);
		return;
	case 0xC4:
		etm_write(val, ETMPDCR);
		return;
	case 0xC5:
		etm_write(val, ETMPDSR);
		return;
	default:
		WARN(1, "invalid CP14 access to ETM reg: %lx",
							(unsigned long)reg);
		return;
	}
}

static inline uint32_t offset_to_reg_num(uint32_t off)
{
	return off >> 2;
}

unsigned int etm_readl_cp14(uint32_t off)
{
	uint32_t reg = offset_to_reg_num(off);
	return etm_read_reg(reg);
}

void etm_writel_cp14(uint32_t val, uint32_t off)
{
	uint32_t reg = offset_to_reg_num(off);
	etm_write_reg(val, reg);
}
