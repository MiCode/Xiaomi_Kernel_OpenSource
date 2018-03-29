/*******************************************************************************
* mt6575_pwm.h PWM Drvier
*
* Copyright (c) 2010, Media Teck.inc
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public Licence,
* version 2, as publish by the Free Software Foundation.
*
* This program is distributed and in hope it will be useful, but WITHOUT
* ANY WARRNTY; without even the implied warranty of MERCHANTABITLITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
*/

#ifndef __MT_PWM_H__
#define __MT_PWM_H__

#include <linux/types.h>
#include <mach/mt_pwm_hal.h>

struct pwm_easy_config {
	u32 pwm_no;
	u32 duty;
	u32 clk_src;
	u32 clk_div;
	u16 duration;
	u8 pmic_pad;
};

struct pwm_spec_config {
	u32 pwm_no;
	u32 mode;
	u32 clk_div;
	u32 clk_src;
	u8 intr;
	u8 pmic_pad;

	union {
		/* for old mode */
		struct _PWM_OLDMODE_REGS {
			u16 IDLE_VALUE;
			u16 GUARD_VALUE;
			u16 GDURATION;
			u16 WAVE_NUM;
			u16 DATA_WIDTH;
			u16 THRESH;
		} PWM_MODE_OLD_REGS;

		/* for fifo mode */
		struct _PWM_MODE_FIFO_REGS {
			u32 IDLE_VALUE;
			u32 GUARD_VALUE;
			u32 STOP_BITPOS_VALUE;
			u16 HDURATION;
			u16 LDURATION;
			u32 GDURATION;
			u32 SEND_DATA0;
			u32 SEND_DATA1;
			u32 WAVE_NUM;
		} PWM_MODE_FIFO_REGS;

		/* for memory mode */
		struct _PWM_MODE_MEMORY_REGS {
			u32 IDLE_VALUE;
			u32 GUARD_VALUE;
			u32 STOP_BITPOS_VALUE;
			u16 HDURATION;
			u16 LDURATION;
			u16 GDURATION;
			dma_addr_t BUF0_BASE_ADDR;
			u32 BUF0_SIZE;
			u16 WAVE_NUM;
		} PWM_MODE_MEMORY_REGS;

		/* for RANDOM mode */
		struct _PWM_MODE_RANDOM_REGS {
			u16 IDLE_VALUE;
			u16 GUARD_VALUE;
			u32 STOP_BITPOS_VALUE;
			u16 HDURATION;
			u16 LDURATION;
			u16 GDURATION;
			dma_addr_t BUF0_BASE_ADDR;
			u32 BUF0_SIZE;
			dma_addr_t BUF1_BASE_ADDR;
			u32 BUF1_SIZE;
			u16 WAVE_NUM;
			u32 VALID;
		} PWM_MODE_RANDOM_REGS;

		/* for seq mode */
		struct _PWM_MODE_DELAY_REGS {
			/* u32 ENABLE_DELAY_VALUE; */
			u16 PWM3_DELAY_DUR;
			u32 PWM3_DELAY_CLK;	/* 0: block clock source, 1: block/1625 clock source */
			u16 PWM4_DELAY_DUR;
			u32 PWM4_DELAY_CLK;
			u16 PWM5_DELAY_DUR;
			u32 PWM5_DELAY_CLK;
		} PWM_MODE_DELAY_REGS;

	};
};

s32 pwm_set_easy_config(struct pwm_easy_config *conf);
s32 pwm_set_spec_config(struct pwm_spec_config *conf);

void mt_pwm_dump_regs(void);
void mt_pwm_disable(u32 pwm_no, u8 pmic_pad);

/*----------3dLCM support-----------*/
void mt_set_pwm_3dlcm_enable(u8 enable);
/*
 set "pwm_no" inversion of pwm base or not
 */
void mt_set_pwm_3dlcm_inv(u32 pwm_no, u8 inv);
/* void mt_set_pwm_3dlcm_base(u32 pwm_no); */

/* void mt_pwm_26M_clk_enable(u32 enable); */
s32 mt_set_intr_ack(u32 pwm_intr_ack_bit);
s32 mt_set_intr_enable(u32 pwm_intr_enable_bit);
s32 mt_get_intr_status(u32 pwm_intr_status_bit);

#endif
