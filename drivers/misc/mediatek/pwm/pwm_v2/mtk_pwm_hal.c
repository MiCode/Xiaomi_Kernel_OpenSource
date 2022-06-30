// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>

#include <mt-plat/mtk_pwm_hal_pub.h>
#include <mt-plat/mtk_pwm_hal.h>
#include <mach/mtk_pwm_prv.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

/**********************************
 * Global  data
 ***********************************/
enum {
	PWM_CON,
	PWM_HDURATION,
	PWM_LDURATION,
	PWM_GDURATION,
	PWM_BUF0_BASE_ADDR,
	PWM_BUF0_SIZE,
	PWM_BUF1_BASE_ADDR,
	PWM_BUF1_SIZE,
	PWM_SEND_DATA0,
	PWM_SEND_DATA1,
	PWM_WAVE_NUM,
	PWM_DATA_WIDTH,
	PWM_THRESH,
	PWM_SEND_WAVENUM,
	PWM_VALID,
	PWM_BUF_BASE_ADDR2
} PWM_REG_OFF;


#if IS_ENABLED(CONFIG_OF)
unsigned long PWM_register[PWM_MAX] = {};
struct regmap *pwm_src_regmap;
u32 pwm_clk_src_ctrl;
u32 pwm_bclk_sw_ctrl_offset;
u32 pwm_x_bclk_sw_ctrl_offset[PWM_MAX] = {};
bool pwm_clk_all_on_off;
#endif

/**************************************************************/
enum {
	PWM1_CLK,
	PWM2_CLK,
	PWM3_CLK,
	PWM4_CLK,
	PWM5_CLK,
	PWM6_CLK,
	PWM_HCLK,
	PWM_CLK,
	PWM_CLK_NUM,
};

const char *pwm_clk_name[] = {
	"PWM1-main",
	"PWM2-main",
	"PWM3-main",
	"PWM4-main",
	"PWM5-main",
	"PWM6-main",
	"PWM-HCLK-main",
	"PWM-main"
};

enum {
	PWM_V0, //reserve for 26MHz clk source in pwm domain not INFRA
	PWM_V1, //pwm channel base: 0x10,0x50,0x090,0xd0,0x110,0x150
	PWM_V2, //pwm channel base: 0x80,0xc0,0x100,0x140,0x180,0x1c0
	PWM_V_NUM,
};

struct clk *pwm_clk[PWM_CLK_NUM];
static unsigned int pwm_version = PWM_V0;

void mt_pwm_power_on_hal(u32 pwm_no, bool pmic_pad, unsigned long *power_flag)
{
	int clk_en_ret;
	int i;

	/* Set pwm_main , pwm_hclk_main(for memory and random mode) */
	if (0 == (*power_flag)) {
		if (pwm_clk[PWM_CLK]) {
			pr_info("[PWM][CCF]enable clk PWM_CLK:%p\n",
				pwm_clk[PWM_CLK]);
			clk_en_ret = clk_prepare_enable(pwm_clk[PWM_CLK]);
			if (clk_en_ret) {
				pr_notice("[PWM][CCF]enable clk PWM_CLK failed. ret:%d, clk_pwm_main:%p\n",
				clk_en_ret, pwm_clk[PWM_CLK]);
			} else
				set_bit(PWM_CLK, power_flag);
		}

		if (pwm_clk[PWM_HCLK]) {
			pr_info("[PWM][CCF]enable clk PWM_HCLK: %p\n",
				pwm_clk[PWM_HCLK]);
			clk_en_ret = clk_prepare_enable(pwm_clk[PWM_HCLK]);
			if (clk_en_ret) {
				pr_notice("[PWM][CCF]enable clk PWM_HCLK failed. ret:%d, clk_pwm_hclk_main:%p\n",
				clk_en_ret, pwm_clk[PWM_HCLK]);
			} else
				set_bit(PWM_HCLK, power_flag);
		}
	}
	/* Set pwm_no clk */
	if (pwm_clk_all_on_off) {
		if ((*power_flag) == ((1 << PWM_HCLK) | (1 << PWM_CLK))) {
			/* 1st user come, enable all pwmX clocks */
			for (i = PWM1; i < PWM_MAX; i++) {
				if (pwm_clk[i]) {
					pr_info("[PWM][CCF]all on/off enable clk_pwm%d :%p\n",
						i, pwm_clk[i]);
					clk_en_ret =
						clk_prepare_enable(pwm_clk[i]);
					if (clk_en_ret) {
						pr_notice("[PWM][CCF]all on/off enable clk_pwm_main failed. ret:%d, clk_pwm%d :%p\n",
						clk_en_ret, i, pwm_clk[i]);
					}
				}
			}
		}
		if (pwm_clk[pwm_no])
			set_bit(pwm_no, power_flag);
	} else {
		if (!test_bit(pwm_no, power_flag)) {
			if (pwm_clk[pwm_no]) {
				pr_info("[PWM][CCF]enable clk_pwm%d :%p\n",
					pwm_no, pwm_clk[pwm_no]);
				clk_en_ret =
					clk_prepare_enable(pwm_clk[pwm_no]);
				if (clk_en_ret) {
					pr_notice("[PWM][CCF]enable clk_pwm_main failed. ret:%d, clk_pwm%d :%p\n",
					clk_en_ret, pwm_no, pwm_clk[pwm_no]);
				} else
					set_bit(pwm_no, power_flag);
			}
		}
	}
}

void mt_pwm_power_off_hal(u32 pwm_no, bool pmic_pad, unsigned long *power_flag)
{
	int i;

	if (pwm_clk_all_on_off) {
		if (test_bit(pwm_no, power_flag)) {
			if (pwm_clk[pwm_no]) {
				pr_info("[PWM][CCF]all on/off disable power_flag clk_pwm%d :%p\n",
					pwm_no, pwm_clk[pwm_no]);
				clear_bit(pwm_no, power_flag);
			}
		}
		if ((*power_flag) == ((1 << PWM_HCLK) | (1 << PWM_CLK))) {
			/* disable all pwmX clocks */
			for (i = PWM1; i < PWM_MAX; i++) {
				if (pwm_clk[i]) {
					pr_info("[PWM][CCF]all on/off disable clk_pwm%d :%p\n",
						i, pwm_clk[i]);
					clk_disable_unprepare(pwm_clk[i]);
				}
			}
		}
	} else {
		if (test_bit(pwm_no, power_flag)) {
			if (pwm_clk[pwm_no]) {
				pr_info("[PWM][CCF]disable clk_pwm%d :%p\n",
					pwm_no, pwm_clk[pwm_no]);
				clk_disable_unprepare(pwm_clk[pwm_no]);
				clear_bit(pwm_no, power_flag);
			}
		}
	}

	if ((*power_flag) == ((1 << PWM_HCLK) | (1 << PWM_CLK))) {
		/* Disable PWM-main, PWM-HCLK-main */
		if (test_bit(PWM_HCLK, power_flag)) {
			if (pwm_clk[PWM_HCLK]) {
				pr_info("[PWM][CCF]disable clk_pwm_hclk :%p\n",
					pwm_clk[PWM_HCLK]);
				clk_disable_unprepare(pwm_clk[PWM_HCLK]);
				clear_bit(PWM_HCLK, power_flag);
			}
		}
		if (test_bit(PWM_CLK, power_flag)) {
			if (pwm_clk[PWM_CLK]) {
				pr_info("[PWM][CCF]disable clk_pwm :%p\n",
					pwm_clk[PWM_CLK]);
				clk_disable_unprepare(pwm_clk[PWM_CLK]);
				clear_bit(PWM_CLK, power_flag);
			}
		}
	}
}

void mt_pwm_init_power_flag(unsigned long *power_flag)
{
	if (pwm_version == PWM_V2) {
		PWM_register[PWM1] = (unsigned long)pwm_base + 0x0080;
		PWM_register[PWM2] = (unsigned long)pwm_base + 0x00c0;
		PWM_register[PWM3] = (unsigned long)pwm_base + 0x0100;
		PWM_register[PWM4] = (unsigned long)pwm_base + 0x0140;
		PWM_register[PWM5] = (unsigned long)pwm_base + 0x0180;
		PWM_register[PWM6] = (unsigned long)pwm_base + 0x01c0;
	} else {
		PWM_register[PWM1] = (unsigned long)pwm_base + 0x0010;
		PWM_register[PWM2] = (unsigned long)pwm_base + 0x0050;
		PWM_register[PWM3] = (unsigned long)pwm_base + 0x0090;
		PWM_register[PWM4] = (unsigned long)pwm_base + 0x00d0;
		PWM_register[PWM5] = (unsigned long)pwm_base + 0x0110;
		PWM_register[PWM6] = (unsigned long)pwm_base + 0x0150;
	}
}

s32 mt_pwm_sel_pmic_hal(u32 pwm_no)
{
	pr_debug("mt_pwm_sel_pmic\n");
	return -EINVALID;
}

s32 mt_pwm_sel_ap_hal(u32 pwm_no)
{
	pr_debug("mt_pwm_sel_ap\n");
	return -EINVALID;
}

void mt_set_pwm_enable_hal(u32 pwm_no)
{
	SETREG32(PWM_ENABLE, 1 << pwm_no);
}

void mt_set_pwm_disable_hal(u32 pwm_no)
{
	CLRREG32(PWM_ENABLE, 1 << pwm_no);
}

void mt_set_pwm_enable_seqmode_hal(void)
{
}

void mt_set_pwm_disable_seqmode_hal(void)
{
}

s32 mt_set_pwm_test_sel_hal(u32 val)
{
	return 0;
}

void mt_set_pwm_clk_hal(u32 pwm_no, u32 clksrc, u32 div)
{
	unsigned long reg_con;

	reg_con = PWM_register[pwm_no] + 4 * PWM_CON;
	MASKREG32(reg_con, PWM_CON_CLKDIV_MASK, div);
	if ((clksrc & 0x80000000) != 0) {
		clksrc &= ~(0x80000000);
		if (clksrc == CLK_BLOCK_BY_1625_OR_32K) {/* old mode: 32k clk*/
			SETREG32(reg_con, 1 << PWM_CON_CLKSEL_OLD_OFFSET);
			SETREG32(reg_con, 1 << PWM_CON_CLKSEL_OFFSET);
			pr_info("%s: PWM old clock set 32K!\n", __func__);
		} else {
			CLRREG32(reg_con, 1 << PWM_CON_CLKSEL_OLD_OFFSET);
			SETREG32(reg_con, 1 << PWM_CON_CLKSEL_OFFSET);
			pr_info("%s: PWM old clock set 26M/1625!\n", __func__);
		}
	} else {
		CLRREG32(reg_con, 1 << PWM_CON_CLKSEL_OLD_OFFSET);
		if (clksrc == CLK_BLOCK) {
			CLRREG32(reg_con, 1 << PWM_CON_CLKSEL_OFFSET);
			pr_info("%s: PWM new clock set 26M!\n", __func__);
		} else if (clksrc == CLK_BLOCK_BY_1625_OR_32K) {
			SETREG32(reg_con, 1 << PWM_CON_CLKSEL_OFFSET);
			pr_info("%s: PWM new clock set 26M/1625!\n", __func__);
		} else
			pr_info("clksrc(%u) set err\n", clksrc);
	}
}

s32 mt_get_pwm_clk_hal(u32 pwm_no)
{
	s32 clk, clksrc, clkdiv;
	unsigned long reg_con, reg_val;

	reg_con = PWM_register[pwm_no] + 4 * PWM_CON;

	reg_val = INREG32(reg_con);

	if (((reg_val & PWM_CON_CLKSEL_MASK) >> PWM_CON_CLKSEL_OFFSET) == 1)
		if (((reg_val & PWM_CON_OLD_MODE_MASK) >>
				PWM_CON_OLD_MODE_OFFSET) == 1)
			clksrc = 32 * 1024;
		else
			clksrc = BLOCK_CLK;
	else
		clksrc = BLOCK_CLK / 1625;

	clkdiv = 2 << (reg_val & PWM_CON_CLKDIV_MASK);
	if (clkdiv <= 0) {
		pr_debug("clkdiv less zero, not valid\n");
		return -ERROR;
	}

	clk = clksrc / clkdiv;
	pr_debug("CLK is :%d\n", clk);
	return clk;
}

s32 mt_set_pwm_con_datasrc_hal(u32 pwm_no, u32 val)
{
	unsigned long reg_con;

	reg_con = PWM_register[pwm_no] + 4 * PWM_CON;
	if (val == PWM_FIFO)
		CLRREG32(reg_con, 1 << PWM_CON_SRCSEL_OFFSET);
	else if (val == MEMORY)
		SETREG32(reg_con, 1 << PWM_CON_SRCSEL_OFFSET);
	else
		return 1;
	return 0;
}

s32 mt_set_pwm_con_mode_hal(u32 pwm_no, u32 val)
{
	unsigned long reg_con;

	reg_con = PWM_register[pwm_no] + 4 * PWM_CON;
	if (val == PERIOD)
		CLRREG32(reg_con, 1 << PWM_CON_MODE_OFFSET);
	else if (val == RAND)
		SETREG32(reg_con, 1 << PWM_CON_MODE_OFFSET);
	else
		return 1;
	return 0;
}

s32 mt_set_pwm_con_idleval_hal(u32 pwm_no, uint16_t val)
{
	unsigned long reg_con;

	reg_con = PWM_register[pwm_no] + 4 * PWM_CON;
	if (val == IDLE_TRUE)
		SETREG32(reg_con, 1 << PWM_CON_IDLE_VALUE_OFFSET);
	else if (val == IDLE_FALSE)
		CLRREG32(reg_con, 1 << PWM_CON_IDLE_VALUE_OFFSET);
	else
		return 1;
	return 0;
}

s32 mt_set_pwm_con_guardval_hal(u32 pwm_no, uint16_t val)
{
	unsigned long reg_con;

	reg_con = PWM_register[pwm_no] + 4 * PWM_CON;
	if (val == GUARD_TRUE)
		SETREG32(reg_con, 1 << PWM_CON_GUARD_VALUE_OFFSET);
	else if (val == GUARD_FALSE)
		CLRREG32(reg_con, 1 << PWM_CON_GUARD_VALUE_OFFSET);
	else
		return 1;
	return 0;
}

void mt_set_pwm_con_stpbit_hal(u32 pwm_no, u32 stpbit, u32 srcsel)
{
	unsigned long reg_con;

	reg_con = PWM_register[pwm_no] + 4 * PWM_CON;
	if (srcsel == PWM_FIFO)
		MASKREG32(reg_con, PWM_CON_STOP_BITS_MASK,
			stpbit << PWM_CON_STOP_BITS_OFFSET);
	if (srcsel == MEMORY)
		MASKREG32(reg_con,
			PWM_CON_STOP_BITS_MASK &
			(0x1f << PWM_CON_STOP_BITS_OFFSET),
			stpbit << PWM_CON_STOP_BITS_OFFSET);
}

s32 mt_set_pwm_con_oldmode_hal(u32 pwm_no, u32 val)
{
	unsigned long reg_con;

	reg_con = PWM_register[pwm_no] + 4 * PWM_CON;
	if (val == OLDMODE_DISABLE)
		CLRREG32(reg_con, 1 << PWM_CON_OLD_MODE_OFFSET);
	else if (val == OLDMODE_ENABLE)
		SETREG32(reg_con, 1 << PWM_CON_OLD_MODE_OFFSET);
	else
		return 1;
	return 0;
}

void mt_set_pwm_HiDur_hal(u32 pwm_no, uint16_t DurVal)
{				/* only low 16 bits are valid */
	unsigned long reg_HiDur;

	reg_HiDur = PWM_register[pwm_no] + 4 * PWM_HDURATION;
	OUTREG32(reg_HiDur, DurVal);
}

void mt_set_pwm_LowDur_hal(u32 pwm_no, uint16_t DurVal)
{
	unsigned long reg_LowDur;

	reg_LowDur = PWM_register[pwm_no] + 4 * PWM_LDURATION;
	OUTREG32(reg_LowDur, DurVal);
}

void mt_set_pwm_GuardDur_hal(u32 pwm_no, uint16_t DurVal)
{
	unsigned long reg_GuardDur;

	reg_GuardDur = PWM_register[pwm_no] + 4 * PWM_GDURATION;
	OUTREG32(reg_GuardDur, DurVal);
}

void mt_set_pwm_send_data0_hal(u32 pwm_no, u32 data)
{
	unsigned long reg_data0;

	reg_data0 = PWM_register[pwm_no] + 4 * PWM_SEND_DATA0;
	OUTREG32(reg_data0, data);
}

void mt_set_pwm_send_data1_hal(u32 pwm_no, u32 data)
{
	unsigned long reg_data1;

	reg_data1 = PWM_register[pwm_no] + 4 * PWM_SEND_DATA1;
	OUTREG32(reg_data1, data);
}

void mt_set_pwm_wave_num_hal(u32 pwm_no, uint16_t num)
{
	unsigned long reg_wave_num;

	reg_wave_num = PWM_register[pwm_no] + 4 * PWM_WAVE_NUM;
	OUTREG32(reg_wave_num, num);
}

void mt_set_pwm_data_width_hal(u32 pwm_no, uint16_t width)
{
	unsigned long reg_data_width;

	reg_data_width = PWM_register[pwm_no] + 4 * PWM_DATA_WIDTH;
	OUTREG32(reg_data_width, width);
}

void mt_set_pwm_thresh_hal(u32 pwm_no, uint16_t thresh)
{
	unsigned long reg_thresh;

	reg_thresh = PWM_register[pwm_no] + 4 * PWM_THRESH;
	OUTREG32(reg_thresh, thresh);
}

s32 mt_get_pwm_send_wavenum_hal(u32 pwm_no)
{
	unsigned long reg_send_wavenum = 0;

	reg_send_wavenum = PWM_register[pwm_no] + 4 * PWM_SEND_WAVENUM;
	return INREG32(reg_send_wavenum);
}

void mt_set_intr_enable_hal(u32 pwm_intr_enable_bit)
{
	if (pwm_version == PWM_V2)
		SETREG32(PWM_INT_ENABLE_V2, 1 << (pwm_intr_enable_bit / 2));
	else
		SETREG32(PWM_INT_ENABLE, 1 << pwm_intr_enable_bit);
}

s32 mt_get_intr_status_hal(u32 pwm_intr_status_bit)
{
	unsigned long int_status;

	if (pwm_version == PWM_V2) {
		int_status = INREG32(PWM_INT_STATUS_V2);
		int_status = (int_status >> (pwm_intr_status_bit / 2)) & 0x01;
	} else {
		int_status = INREG32(PWM_INT_STATUS);
		int_status = (int_status >> pwm_intr_status_bit) & 0x01;
	}
	return int_status;
}

void mt_set_intr_ack_hal(u32 pwm_intr_ack_bit)
{
	if (pwm_version == PWM_V2)
		SETREG32(PWM_INT_ACK_V2, 1 << (pwm_intr_ack_bit / 2));
	else
		SETREG32(PWM_INT_ACK, 1 << pwm_intr_ack_bit);
}

void mt_set_pwm_buf0_addr_hal(u32 pwm_no, dma_addr_t addr)
{
	unsigned long reg_buff0_addr, reg_buff0_addr2;
	unsigned int upper_32_addr;
	unsigned int lower_32_addr;

	reg_buff0_addr = PWM_register[pwm_no] +	4 * PWM_BUF0_BASE_ADDR;
	reg_buff0_addr2 = PWM_register[pwm_no] + 4 * PWM_BUF_BASE_ADDR2;

	lower_32_addr = lower_32_bits(addr);
	OUTREG32_DMA(reg_buff0_addr, lower_32_addr);
	CLRREG32(reg_buff0_addr2, 0xF);

	if (addr > 0xFFFFFFFF) {
		upper_32_addr = upper_32_bits(addr);
		SETREG32(reg_buff0_addr2, upper_32_addr);
	}
}

void mt_set_pwm_buf0_size_hal(u32 pwm_no, uint16_t size)
{
	unsigned long reg_buff0_size;

	reg_buff0_size = PWM_register[pwm_no] + 4 * PWM_BUF0_SIZE;
	OUTREG32(reg_buff0_size, size);
}

void mt_pwm_dump_regs_hal(void)
{
	int i = 0;
	unsigned long reg_val = 0;

	pr_info("=========> [PWM DUMP RG START] <=========\n ");
	for (i = PWM1; i < PWM_MAX; i++) {
		reg_val = INREG32(PWM_register[i] + 4 * PWM_CON);
		pr_info("[PWM%d_CON]: 0x%lx\n", i + 1, reg_val);
		reg_val = INREG32(PWM_register[i] + 4 * PWM_HDURATION);
		pr_info("[PWM%d_HDURATION]: 0x%lx\n", i + 1, reg_val);
		reg_val = INREG32(PWM_register[i] + 4 * PWM_LDURATION);
		pr_info("[PWM%d_LDURATION]: 0x%lx\n", i + 1, reg_val);
		reg_val = INREG32(PWM_register[i] + 4 * PWM_GDURATION);
		pr_info("[PWM%d_GDURATION]: 0x%lx\n", i + 1, reg_val);

		reg_val = INREG32(PWM_register[i] + 4 * PWM_BUF0_BASE_ADDR);
		pr_info("[PWM%d_BUF0_BASE_ADDR]: 0x%lx\n", i, reg_val);
		reg_val = INREG32(PWM_register[i] + 4 * PWM_BUF0_SIZE);
		pr_info("[PWM%d_BUF0_SIZE]: 0x%lx\n", i, reg_val);
		reg_val = INREG32(PWM_register[i] + 4 * PWM_BUF1_BASE_ADDR);
		pr_info("[PWM%d_BUF1_BASE_ADDR]: 0x%lx\n", i, reg_val);
		reg_val = INREG32(PWM_register[i] + 4 * PWM_BUF1_SIZE);
		pr_info("[PWM%d_BUF1_SIZE]: 0x%lx\n", i + 1, reg_val);

		reg_val = INREG32(PWM_register[i] + 4 * PWM_SEND_DATA0);
		pr_info("[PWM%d_SEND_DATA0]: 0x%lx]\n", i + 1, reg_val);
		reg_val = INREG32(PWM_register[i] + 4 * PWM_SEND_DATA1);
		pr_info("[PWM%d_PWM_SEND_DATA1]: 0x%lx\n", i + 1, reg_val);
		reg_val = INREG32(PWM_register[i] + 4 * PWM_WAVE_NUM);
		pr_info("[PWM%d_WAVE_NUM]: 0x%lx\n", i + 1, reg_val);
		reg_val = INREG32(PWM_register[i] + 4 * PWM_DATA_WIDTH);
		pr_info("[PWM%d_WIDTH]: 0x%lx\n", i + 1, reg_val);

		reg_val = INREG32(PWM_register[i] + 4 * PWM_THRESH);
		pr_info("[PWM%d_THRESH]: 0x%lx\n", i + 1, reg_val);
		reg_val = INREG32(PWM_register[i] + 4 * PWM_SEND_WAVENUM);
		pr_info("[PWM%d_SEND_WAVENUM]: 0x%lx\n\r", i + 1, reg_val);
		reg_val = INREG32(PWM_register[i] + 4 * PWM_BUF_BASE_ADDR2);
		pr_info("[PWM%d_BUF_BASE_ADDR2]: 0x%lx\n\r", i + 1, reg_val);
	}

	reg_val = INREG32(PWM_ENABLE);
	pr_info("[PWM_ENABLE]: 0x%lx\n ", reg_val);
	if (pwm_version == PWM_V2)
		reg_val = INREG32(PWM_CK_26M_SEL_V2);
	else
		reg_val = INREG32(PWM_CK_26M_SEL);
	pr_info("[PWM_26M_SEL]: 0x%lx\n ", reg_val);
	/*pr_info("peri pdn0 clock: 0x%x\n", INREG32(INFRA_PDN_STA0));*/
	if (pwm_version == PWM_V2)
		reg_val = INREG32(PWM_INT_ENABLE_V2);
	else
		reg_val = INREG32(PWM_INT_ENABLE);
	pr_info("[PWM_INT_ENABLE]:0x%lx\n ", reg_val);
	if (pwm_version == PWM_V2)
		reg_val = INREG32(PWM_INT_STATUS_V2);
	else
		reg_val = INREG32(PWM_INT_STATUS);
	pr_info("[PWM_INT_STATUS]: 0x%lx\n ", reg_val);
	if (pwm_version == PWM_V2)
		reg_val = INREG32(PWM_EN_STATUS_V2);
	else
		reg_val = INREG32(PWM_EN_STATUS);
	pr_info("[PWM_EN_STATUS]: 0x%lx\n ", reg_val);
	pr_info("=========> [PWM DUMP RG END] <=========\n ");

}

void pwm_debug_store_hal(void)
{
	/* dump clock status */
	/*pr_debug("peri pdn0 clock: 0x%x\n", INREG32(INFRA_PDN_STA0));*/
}

void pwm_debug_show_hal(void)
{
	mt_pwm_dump_regs_hal();
}

/*----------3dLCM support-----------*/
/*
 *base pwm2, select pwm3&4&5 same as pwm2 or inversion of pwm2
 */
void mt_set_pwm_3dlcm_enable_hal(u8 enable)
{
	if (pwm_version == PWM_V2)
		SETREG32(PWM_3DLCM_V2, 1 << PWM_3DLCM_ENABLE_OFFSET);
}

/*
 *set "pwm_no" inversion of pwm base or not
 */
void mt_set_pwm_3dlcm_inv_hal(u32 pwm_no, u8 inv)
{
	if (pwm_version == PWM_V2) {
		unsigned long reg_con;

		reg_con = PWM_register[pwm_no] + 4 * PWM_CON;

		/*set "pwm_no" as auxiliary first */
		SETREG32(reg_con, 1 << (19));

		if (inv)
			SETREG32(reg_con, 1 << (17));
		else
			CLRREG32(reg_con, 1 << (17));
	}
}

void mt_set_pwm_3dlcm_base_hal(u32 pwm_no)
{
	if (pwm_version == PWM_V2) {
		unsigned long reg_con;

		reg_con = PWM_register[pwm_no] + 4 * PWM_CON;

		CLRREG32(reg_con, 1 << (18));
		SETREG32(reg_con, 1 << (18));
	}
}

void mt_pwm_26M_clk_enable_hal(u32 enable)
{
	unsigned long reg_con;

	/* select 66M or 26M */
	if (pwm_version == PWM_V2)
		reg_con = (unsigned long)PWM_CK_26M_SEL_V2;
	else
		reg_con = (unsigned long)PWM_CK_26M_SEL;
	if (enable)
		SETREG32(reg_con, 1 << PWM_CK_26M_SEL_OFFSET);
	else
		CLRREG32(reg_con, 1 << PWM_CK_26M_SEL_OFFSET);

}

void mt_pwm_clk_sel_hal(u32 pwm_no, u32 clk_src)
{
	int pwm_x_offset = 0;
	if (pwm_no > PWM_MAX)
		pr_info("PWM: invalid pwm_no\n");

	switch (pwm_no) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		pwm_x_offset = pwm_x_bclk_sw_ctrl_offset[pwm_no];
		break;
	default:
		pr_info("PWM: invalid pwm_x_offset\n");
	}

	switch (clk_src) {
	/* 32K */
	case 0x00:
		regmap_update_bits(pwm_src_regmap, pwm_clk_src_ctrl,
			0x3 << pwm_x_offset | 0x3 << pwm_bclk_sw_ctrl_offset,
			0x0 << pwm_x_offset | 0x0 << pwm_bclk_sw_ctrl_offset);
		break;
	/* 26M */
	case 0x01:
		regmap_update_bits(pwm_src_regmap, pwm_clk_src_ctrl,
			0x3 << pwm_x_offset | 0x3 << pwm_bclk_sw_ctrl_offset,
			0x1 << pwm_x_offset | 0x1 << pwm_bclk_sw_ctrl_offset);
		break;
	/* 78M not recommend */
	case 0x2:
		regmap_update_bits(pwm_src_regmap, pwm_clk_src_ctrl,
			0x3 << pwm_x_offset | 0x3 << pwm_bclk_sw_ctrl_offset,
			0x2 << pwm_x_offset | 0x2 << pwm_bclk_sw_ctrl_offset);
		break;
	/* 66M, topckgen default */
	case 0x3:
		regmap_update_bits(pwm_src_regmap, pwm_clk_src_ctrl,
			0x3 << pwm_x_offset | 0x3 << pwm_bclk_sw_ctrl_offset,
			0x3 << pwm_x_offset | 0x3 << pwm_bclk_sw_ctrl_offset);
		break;
	default:
		pr_info("PWM: invalid clk_src\n");
	}
}
EXPORT_SYMBOL(mt_pwm_clk_sel_hal);

void mt_pwm_platform_init(struct platform_device *pdev)
{
	int ret = 0;

	pwm_src_regmap = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
		"pwmsrcclk");

	if (IS_ERR(pwm_src_regmap)) {
		dev_err(&pdev->dev, "Cannot find pwm src controller: %ld\n",
		PTR_ERR(pwm_src_regmap));
	}

	if (of_property_read_bool(pdev->dev.of_node, "mediatek,pwm-clk-all-on-off")) {
		pwm_clk_all_on_off = true;
		pr_info("find node mediatek,pwm-clk-all-on-off: %d\n",
			pwm_clk_all_on_off);
	}

	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,pwm-version",
			&pwm_version);
	if (ret == 0)
		pr_info("find node mediatek,pwm-version: 0x%x\n",
			pwm_version);
	else
		pr_info("default pwm_version: 0x%x\n",
			pwm_version);

	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,pwm-topclk-ctl-reg",
			&pwm_clk_src_ctrl);
	if (ret == 0)
		pr_info("find node mediatek,pwm-topclk-ctl-reg: 0x%x\n",
			pwm_clk_src_ctrl);

	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,pwm-bclk-sw-ctrl-offset",
			&pwm_bclk_sw_ctrl_offset);
	if (ret == 0)
		pr_info("find node mediatek,pwm-bclk-sw-ctrl-offfset: 0x%x\n",
			pwm_bclk_sw_ctrl_offset);

	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,pwm1-bclk-sw-ctrl-offset",
			&pwm_x_bclk_sw_ctrl_offset[0]);
	if (ret == 0)
		pr_info("find node mediatek,pwm1-bclk-sw-ctrl-offfset: 0x%x\n",
			pwm_x_bclk_sw_ctrl_offset[0]);

	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,pwm2-bclk-sw-ctrl-offset",
			&pwm_x_bclk_sw_ctrl_offset[1]);
	if (ret == 0)
		pr_info("find node mediatek,pwm2-bclk-sw-ctrl-offfset: 0x%x\n",
			pwm_x_bclk_sw_ctrl_offset[1]);

	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,pwm3-bclk-sw-ctrl-offset",
			&pwm_x_bclk_sw_ctrl_offset[2]);
	if (ret == 0)
		pr_info("find node mediatek,pwm3-bclk-sw-ctrl-offfset: 0x%x\n",
			pwm_x_bclk_sw_ctrl_offset[2]);

	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,pwm4-bclk-sw-ctrl-offset",
			&pwm_x_bclk_sw_ctrl_offset[3]);
	if (ret == 0)
		pr_info("find node mediatek,pwm4-bclk-sw-ctrl-offfset: 0x%x\n",
			pwm_x_bclk_sw_ctrl_offset[3]);

	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,pwm5-bclk-sw-ctrl-offset",
			&pwm_x_bclk_sw_ctrl_offset[4]);
	if (ret == 0)
		pr_info("find node mediatek,pwm5-bclk-sw-ctrl-offfset: 0x%x\n",
			pwm_x_bclk_sw_ctrl_offset[4]);

	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,pwm6-bclk-sw-ctrl-offset",
			&pwm_x_bclk_sw_ctrl_offset[5]);
	if (ret == 0)
		pr_info("find node mediatek,pwm6-bclk-sw-ctrl-offfset: 0x%x\n",
			pwm_x_bclk_sw_ctrl_offset[5]);
}

int mt_get_pwm_clk_src(struct platform_device *pdev)
{
	int i = 0;

	for (i = PWM1_CLK; i < PWM_CLK_NUM; i++) {
		pwm_clk[i] = devm_clk_get(&pdev->dev, pwm_clk_name[i]);
		if (IS_ERR(pwm_clk[i])) {
			pwm_clk[i] = NULL;
			pr_info("cannot get %s clock\n", pwm_clk_name[i]);
		} else {
			pr_info("[PWM] get %s clock, %p\n",
				pwm_clk_name[i], pwm_clk[i]);
		}
	}
	return 0;
}

unsigned int mt_get_pwm_version(void)
{
	return pwm_version;
}
