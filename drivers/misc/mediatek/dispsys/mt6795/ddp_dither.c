#include "cmdq_record.h"

#include "ddp_reg.h"
#include "ddp_dither.h"

#include "ddp_path.h"

#define DITHER_REG(reg_base, index) ((reg_base) + (index) * 4)

void disp_dither_init(disp_dither_id_t id, unsigned int dither_bpp, void *cmdq)
{
	unsigned long reg_base = 0;
	unsigned int enable;

	if (id == DISP_DITHER0) {
		reg_base = DISP_REG_OD_DITHER_0;
	} else if (id == DISP_DITHER1) {
		reg_base = DISP_REG_GAMMA_DITHER_0;
	} else {
		printk(KERN_ERR "[DITHER] disp_dither_init: invalid dither hardware ID = %d\n", id);
		return;
	}

	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 5), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 6), 0x00003004, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 7), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 8), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 9), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 10), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 11), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 12), 0x00000011, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 13), 0x00000000, ~0);
	DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 14), 0x00000000, ~0);

	enable = 0x1;
	if (dither_bpp == 16) {	/* 565 */
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 15), 0x50500001, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 16), 0x50504040, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 0), 0x00000001, ~0);
	} else if (dither_bpp == 18) {	/* 666 */
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 15), 0x40400001, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 16), 0x40404040, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 0), 0x00000001, ~0);
	} else if (dither_bpp == 24) {	/* 888 */
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 15), 0x20200001, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 16), 0x20202020, ~0);
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 0), 0x00000001, ~0);
	} else if (dither_bpp > 24) {
		printk("[DITHER] High depth LCM (bpp = %d), no dither\n", dither_bpp);
		enable = 1;
	} else {
		printk("[DITHER] invalid dither bpp = %d\n", dither_bpp);
		/* Bypass dither */
		DISP_REG_MASK(cmdq, DITHER_REG(reg_base, 0), 0x00000000, ~0);
		enable = 0;
	}

	if (id == DISP_DITHER1) {
		DISP_REG_MASK(cmdq, DISP_REG_GAMMA_CFG, enable << 2, 1 << 2);
	} else {
		DISP_REG_MASK(cmdq, DISP_REG_OD_EN, enable, 0x1);
		DISP_REG_MASK(cmdq, DISP_REG_OD_CFG, enable << 2, 1 << 2);
	}
}
