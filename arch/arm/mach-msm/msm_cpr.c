/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>

#include <mach/irqs.h>

#include "msm_cpr.h"

#define MODULE_NAME "msm-cpr"

/**
 * Convert the Delay time to Timer Count Register
 * e.g if frequency is 19200 kHz and delay required is
 * 20000us, so timer count will be 19200 * 20000 / 1000
 */
#define TIMER_COUNT(freq, delay) ((freq * delay) / 1000)
#define ALL_CPR_IRQ 0x3F

/* Need platform device handle for suspend and resume APIs */
static struct platform_device *cpr_pdev;

static bool enable = 1;
module_param(enable, bool, 0644);
MODULE_PARM_DESC(enable, "CPR Enable");

struct msm_cpr {
	int curr_osc;
	int cpr_mode;
	int prev_mode;
	uint32_t floor;
	uint32_t ceiling;
	bool max_volt_set;
	void __iomem *base;
	unsigned int irq;
	uint32_t cur_Vmin;
	uint32_t cur_Vmax;
	struct mutex cpr_mutex;
	struct regulator *vreg_cx;
	const struct msm_cpr_config *config;
	struct notifier_block freq_transition;
	struct msm_cpr_vp_data *vp;
};

/* Need to maintain state data for suspend and resume APIs */
static struct msm_cpr_reg cpr_save_state;

static inline
void cpr_write_reg(struct msm_cpr *cpr, u32 offset, u32 value)
{
	writel_relaxed(value, cpr->base + offset);
}

static inline u32 cpr_read_reg(struct msm_cpr *cpr, u32 offset)
{
	return readl_relaxed(cpr->base + offset);
}

static
void cpr_modify_reg(struct msm_cpr *cpr, u32 offset, u32 mask, u32 value)
{
	u32 reg_val;

	reg_val = readl_relaxed(cpr->base + offset);
	reg_val &= ~mask;
	reg_val |= value;
	writel_relaxed(reg_val, cpr->base + offset);
}

#ifdef DEBUG
static void cpr_regs_dump_all(struct msm_cpr *cpr)
{
	pr_debug("RBCPR_GCNT_TARGET(%d): 0x%x\n",
		cpr->curr_osc, readl_relaxed(cpr->base +
		RBCPR_GCNT_TARGET(cpr->curr_osc)));
	pr_debug("RBCPR_TIMER_INTERVAL: 0x%x\n",
		readl_relaxed(cpr->base + RBCPR_TIMER_INTERVAL));
	pr_debug("RBIF_TIMER_ADJUST: 0x%x\n",
		readl_relaxed(cpr->base + RBIF_TIMER_ADJUST));
	pr_debug("RBIF_LIMIT: 0x%x\n",
		readl_relaxed(cpr->base + RBIF_LIMIT));
	pr_debug("RBCPR_STEP_QUOT: 0x%x\n",
		readl_relaxed(cpr->base + RBCPR_STEP_QUOT));
	pr_debug("RBIF_SW_VLEVEL: 0x%x\n",
		readl_relaxed(cpr->base + RBIF_SW_VLEVEL));
	pr_debug("RBCPR_DEBUG1: 0x%x\n",
		readl_relaxed(cpr->base + RBCPR_DEBUG1));
	pr_debug("RBCPR_RESULT_0: 0x%x\n",
		readl_relaxed(cpr->base + RBCPR_RESULT_0));
	pr_debug("RBCPR_RESULT_1: 0x%x\n",
		readl_relaxed(cpr->base + RBCPR_RESULT_1));
	pr_debug("RBCPR_QUOT_AVG: 0x%x\n",
		readl_relaxed(cpr->base + RBCPR_QUOT_AVG));
	pr_debug("RBCPR_CTL: 0x%x\n",
		readl_relaxed(cpr->base + RBCPR_CTL));
	pr_debug("RBIF_IRQ_EN(0): 0x%x\n",
		cpr_read_reg(cpr, RBIF_IRQ_EN(cpr->config->irq_line)));
	pr_debug("RBIF_IRQ_STATUS: 0x%x\n",
		cpr_read_reg(cpr, RBIF_IRQ_STATUS));
}
#endif

/* Enable the CPR H/W Block */
static void cpr_enable(struct msm_cpr *cpr)
{
	mutex_lock(&cpr->cpr_mutex);
	cpr_modify_reg(cpr, RBCPR_CTL, LOOP_EN_M, ENABLE_CPR);
	mutex_unlock(&cpr->cpr_mutex);
}

/* Disable the CPR H/W Block */
static void cpr_disable(struct msm_cpr *cpr)
{
	mutex_lock(&cpr->cpr_mutex);
	cpr_modify_reg(cpr, RBCPR_CTL, LOOP_EN_M, DISABLE_CPR);
	mutex_unlock(&cpr->cpr_mutex);
}

static int32_t cpr_poll_result(struct msm_cpr *cpr)
{
	uint32_t val = 0;
	int8_t rc = 0;

	rc = readl_poll_timeout(cpr->base + RBCPR_RESULT_0, val, ~val & BUSY_M,
				10, 1000);
	if (rc)
		pr_info("%s: RBCPR_RESULT_0 read error: %d\n",
			 __func__, rc);
	return rc;
}

static int32_t cpr_poll_result_done(struct msm_cpr *cpr)
{
	uint32_t val = 0;
	int8_t rc = 0;

	rc = readl_poll_timeout(cpr->base + RBIF_IRQ_STATUS, val, val & 0x1,
				10, 1000);
	if (rc)
		pr_info("%s: RBCPR_IRQ_STATUS read error: %d\n",
			 __func__, rc);
	return rc;
}

static void
cpr_2pt_kv_analysis(struct msm_cpr *cpr, struct msm_cpr_mode *chip_data)
{
	int32_t level_uV = 0, rc;
	uint32_t quot1, quot2;

	/**
	 * 2 Point KV Analysis to calculate Step Quot
	 * STEP_QUOT is number of QUOT units per PMIC step
	 * STEP_QUOT = (quot1 - quot2) / 4
	 *
	 * The step quot is calculated once for every mode and stored for
	 * later use.
	 */
	if (chip_data->step_quot != ~0)
		goto out_2pt_kv;

	/**
	 * Using the value from chip_data->tgt_volt_offset
	 * calculate the new PMIC adjusted voltages and set
	 * the PMIC to provide this value.
	 *
	 * Assuming default voltage is the highest value of safe boot up
	 * voltage, offset is always subtracted from it.
	 *
	 */
	level_uV = chip_data->turbo_Vmax -
		(chip_data->tgt_volt_offset * cpr->vp->step_size);
	pr_debug("tgt_volt_uV = %d\n", level_uV);

	/* Call the PMIC specific routine to set the voltage */
	rc = regulator_set_voltage(cpr->vreg_cx, level_uV, level_uV);
	if (rc) {
		pr_err("%s: Initial voltage set at %duV failed. %d\n",
			__func__, level_uV, rc);
		return;
	}
	rc = regulator_enable(cpr->vreg_cx);
	if (rc) {
		pr_err("failed to enable %s, rc=%d\n", "vdd_cx", rc);
		return;
	}

	/* First CPR measurement at a higher voltage to get QUOT1 */

	/* Enable the Software mode of operation */
	cpr_modify_reg(cpr, RBCPR_CTL, HW_TO_PMIC_EN_M, SW_MODE);

	/* Enable the cpr measurement */
	cpr_modify_reg(cpr, RBCPR_CTL, LOOP_EN_M, ENABLE_CPR);

	/* IRQ is already disabled */
	rc = cpr_poll_result_done(cpr);
	if (rc) {
		pr_err("%s: Quot1: Exiting due to INT_DONE poll timeout\n",
			__func__);
		return;
	}

	rc = cpr_poll_result(cpr);
	if (rc) {
		pr_err("%s: Quot1: Exiting due to BUSY poll timeout\n",
			__func__);
		return;
	}

	quot1 = (cpr_read_reg(cpr, RBCPR_DEBUG1) & QUOT_SLOW_M) >> 12;

	/* Take second CPR measurement at a lower voltage to get QUOT2 */
	level_uV -= 4 * cpr->vp->step_size;
	pr_debug("tgt_volt_uV = %d\n", level_uV);

	cpr_modify_reg(cpr, RBCPR_CTL, LOOP_EN_M, DISABLE_CPR);
	/* Call the PMIC specific routine to set the voltage */
	rc = regulator_set_voltage(cpr->vreg_cx, level_uV, level_uV);
	if (rc) {
		pr_err("%s: Voltage set at %duV failed. %d\n",
			__func__, level_uV, rc);
		return;
	}

	cpr_modify_reg(cpr, RBCPR_CTL, HW_TO_PMIC_EN_M, SW_MODE);
	cpr_modify_reg(cpr, RBCPR_CTL, LOOP_EN_M, ENABLE_CPR);

	/* cpr_write_reg(cpr, RBIF_CONT_NACK_CMD, 0x1); */
	rc = cpr_poll_result_done(cpr);
	if (rc) {
		pr_err("%s: Quot2: Exiting due to INT_DONE poll timeout\n",
			__func__);
		goto err_poll_result_done;
	}
	/* IRQ is already disabled */
	rc = cpr_poll_result(cpr);
	if (rc) {
		pr_err("%s: Quot2: Exiting due to BUSY poll timeout\n",
			__func__);
		goto err_poll_result;
	}
	quot2 = (cpr_read_reg(cpr, RBCPR_DEBUG1) & QUOT_SLOW_M) >> 12;
	chip_data->step_quot = (quot1 - quot2) / 4;
	pr_info("%s: Calculated Step Quot is %d\n",
			__func__, chip_data->step_quot);
	/* Disable the cpr */
	cpr_modify_reg(cpr, RBCPR_CTL, LOOP_EN_M, DISABLE_CPR);

out_2pt_kv:
	/* Program the step quot */
	cpr_write_reg(cpr, RBCPR_STEP_QUOT, (chip_data->step_quot & 0xFF));
	return;
err_poll_result:
err_poll_result_done:
	regulator_disable(cpr->vreg_cx);
}

static inline
void cpr_irq_clr_and_ack(struct msm_cpr *cpr, uint32_t mask)
{
	/* Clear the interrupt */
	cpr_write_reg(cpr, RBIF_IRQ_CLEAR, ALL_CPR_IRQ);
	/* Acknowledge the Recommendation */
	cpr_write_reg(cpr, RBIF_CONT_ACK_CMD, 0x1);
}

static inline
void cpr_irq_clr_and_nack(struct msm_cpr *cpr, uint32_t mask)
{
	cpr_write_reg(cpr, RBIF_IRQ_CLEAR, ALL_CPR_IRQ);
	cpr_write_reg(cpr, RBIF_CONT_NACK_CMD, 0x1);
}

static void cpr_irq_set(struct msm_cpr *cpr, uint32_t irq, bool enable)
{
	uint32_t irq_enabled;

	irq_enabled = cpr_read_reg(cpr, RBIF_IRQ_EN(cpr->config->irq_line));
	if (enable == 1)
		irq_enabled |= irq;
	else
		irq_enabled &= ~irq;
	cpr_modify_reg(cpr, RBIF_IRQ_EN(cpr->config->irq_line),
			INT_MASK, irq_enabled);
}

static void
cpr_up_event_handler(struct msm_cpr *cpr, uint32_t new_volt)
{
	int rc, set_volt_uV;
	struct msm_cpr_mode *chip_data;

	chip_data = &cpr->config->cpr_mode_data[cpr->cpr_mode];

	/**
	 * FIXME: Need to handle a potential race condition between
	 * freq switch handler and CPR interrupt handler here
	 */
	/* Set New PMIC voltage */
	set_volt_uV = (new_volt < cpr->cur_Vmax ? new_volt
				: cpr->cur_Vmax);
	rc = regulator_set_voltage(cpr->vreg_cx, set_volt_uV,
					set_volt_uV);
	if (rc) {
		pr_err("%s: Voltage set at %duV failed. %d\n",
			__func__, set_volt_uV, rc);
		cpr_irq_clr_and_nack(cpr, BIT(4) | BIT(0));
		return;
	}
	pr_info("(railway_voltage: %d uV)\n", set_volt_uV);

	cpr->max_volt_set = (set_volt_uV == cpr->cur_Vmax) ? 1 : 0;

	/* Clear all the interrupts */
	cpr_write_reg(cpr, RBIF_IRQ_CLEAR, ALL_CPR_IRQ);

	/* Disable Auto ACK for Down interrupts */
	cpr_modify_reg(cpr, RBCPR_CTL, SW_AUTO_CONT_NACK_DN_EN_M, 0);

	/* Enable down interrupts to App as it might have got disabled if CPR
	 * hit Vmin earlier. Voltage set is above Vmin now.
	 */
	cpr_irq_set(cpr, DOWN_INT, 1);

	/* Acknowledge the Recommendation */
	cpr_write_reg(cpr, RBIF_CONT_ACK_CMD, 0x1);
}

static void
cpr_dn_event_handler(struct msm_cpr *cpr, uint32_t new_volt)
{
	int rc, set_volt_uV;
	struct msm_cpr_mode *chip_data;

	chip_data = &cpr->config->cpr_mode_data[cpr->cpr_mode];

	/**
	 * FIXME: Need to handle a potential race condition between
	 * freq switch handler and CPR interrupt handler here
	 */
	/* Set New PMIC volt */
	set_volt_uV = (new_volt > cpr->cur_Vmin ? new_volt
				: cpr->cur_Vmin);
	rc = regulator_set_voltage(cpr->vreg_cx, set_volt_uV,
					set_volt_uV);
	if (rc) {
		pr_err("%s: Voltage at %duV failed %d\n",
			__func__, set_volt_uV, rc);
		cpr_irq_clr_and_nack(cpr, BIT(2) | BIT(0));
		return;
	}
	pr_info("(railway_voltage: %d uV)\n", set_volt_uV);

	cpr->max_volt_set = 0;

	/* Clear all the interrupts */
	cpr_write_reg(cpr, RBIF_IRQ_CLEAR, ALL_CPR_IRQ);

	if (new_volt <= cpr->cur_Vmin) {
		/*
		 * Disable down interrupt to App after we hit Vmin
		 * It shall be enabled after we service an up interrupt
		 *
		 * A race condition between freq switch handler and CPR
		 * interrupt handler is possible. So, do not disable
		 * interrupt if a freq switch already caused a mode
		 * change since we need this interrupt in the new mode.
		 */
		if (cpr->cpr_mode == cpr->prev_mode) {
			/* Enable Auto ACK for CPR Down Flags
			 * while DOWN_INT to App is disabled */
			cpr_modify_reg(cpr, RBCPR_CTL,
					SW_AUTO_CONT_NACK_DN_EN_M,
					SW_AUTO_CONT_NACK_DN_EN);
			cpr_irq_set(cpr, DOWN_INT, 0);
			pr_debug("%s: DOWN_INT disabled\n", __func__);
		}
	}
	/* Acknowledge the Recommendation */
	cpr_write_reg(cpr, RBIF_CONT_ACK_CMD, 0x1);
}

static void cpr_set_vdd(struct msm_cpr *cpr, enum cpr_action action)
{
	uint32_t curr_volt, new_volt, error_step;
	struct msm_cpr_mode *chip_data;

	chip_data = &cpr->config->cpr_mode_data[cpr->cpr_mode];
	error_step = cpr_read_reg(cpr, RBCPR_RESULT_0) >> 2;
	error_step &= 0xF;

	curr_volt = regulator_get_voltage(cpr->vreg_cx);

	if (action == UP) {
		/* Clear IRQ, ACK and return if Vdd already at Vmax */
		if (cpr->max_volt_set == 1) {
			cpr_write_reg(cpr, RBIF_IRQ_CLEAR, ALL_CPR_IRQ);
			cpr_write_reg(cpr, RBIF_CONT_NACK_CMD, 0x1);
			return;
		}

		/**
		 * Using up margin in the comparison helps avoid having to
		 * change up threshold values in chip register.
		 */
		if (error_step < (cpr->config->up_threshold +
					cpr->config->up_margin)) {
			pr_debug("UP_INT error step too small to set\n");
			cpr_irq_clr_and_nack(cpr, BIT(4) | BIT(0));
			return;
		}

		/* Calculte new PMIC voltage */
		new_volt = curr_volt + (error_step * cpr->vp->step_size);
		pr_debug("UP_INT: new_volt: %d\n", new_volt);
		pr_info("(UP Voltage recommended by CPR: %d uV)\n", new_volt);
		cpr_up_event_handler(cpr, new_volt);

	} else if (action == DOWN) {
		/**
		 * Using down margin in the comparison helps avoid having to
		 * change down threshold values in chip register.
		 */
		if (error_step < (cpr->config->dn_threshold +
					cpr->config->dn_margin)) {
			pr_debug("DOWN_INT error_step too small to set\n");
			cpr_irq_clr_and_nack(cpr, BIT(2) | BIT(0));
			return;
		}

		/* Calculte new PMIC voltage */
		new_volt = curr_volt - (error_step * cpr->vp->step_size);
		pr_debug("DOWN_INT: new_volt: %d\n", new_volt);
		pr_info("(DN Voltage recommended by CPR: %d uV)\n", new_volt);
		cpr_dn_event_handler(cpr, new_volt);
	}
}

static irqreturn_t cpr_irq0_handler(int irq, void *dev_id)
{
	struct msm_cpr *cpr = dev_id;
	uint32_t reg_val, ctl_reg;

	reg_val = cpr_read_reg(cpr, RBIF_IRQ_STATUS);
	ctl_reg = cpr_read_reg(cpr, RBCPR_CTL);

	/* Following sequence of handling is as per each IRQ's priority */
	if (reg_val & BIT(4)) {
		pr_debug(" CPR:IRQ %d occured for UP Flag\n", irq);
		cpr_set_vdd(cpr, UP);

	} else if ((reg_val & BIT(2)) && !(ctl_reg & SW_AUTO_CONT_NACK_DN_EN)) {
		pr_debug(" CPR:IRQ %d occured for Down Flag\n", irq);
		cpr_set_vdd(cpr, DOWN);

	} else if (reg_val & BIT(1)) {
		pr_debug(" CPR:IRQ %d occured for Min Flag\n", irq);
		cpr_irq_clr_and_nack(cpr, BIT(1) | BIT(0));

	} else if (reg_val & BIT(5)) {
		pr_debug(" CPR:IRQ %d occured for MAX Flag\n", irq);
		cpr_irq_clr_and_nack(cpr, BIT(5) | BIT(0));

	} else if (reg_val & BIT(3)) {
		/* SW_AUTO_CONT_ACK_EN is enabled */
		pr_debug(" CPR:IRQ %d occured for Mid Flag\n", irq);
	}
	return IRQ_HANDLED;
}

static void cpr_config(struct msm_cpr *cpr)
{
	uint32_t delay_count, cnt = 0, rc, tmp_uV;
	struct msm_cpr_mode *chip_data;

	chip_data = &cpr->config->cpr_mode_data[cpr->cpr_mode];

	/* Program the SW vlevel */
	cpr_modify_reg(cpr, RBIF_SW_VLEVEL, SW_VLEVEL_M,
			cpr->config->sw_vlevel);

	/* Set the floor and ceiling values */
	cpr->floor =  cpr->config->floor;
	cpr->ceiling = cpr->config->ceiling;

	/* Program the Ceiling & Floor values */
	cpr_modify_reg(cpr, RBIF_LIMIT, (CEILING_M | FLOOR_M),
					((cpr->ceiling << 6) | cpr->floor));

	/* Program the Up and Down Threshold values */
	cpr_modify_reg(cpr, RBCPR_CTL, UP_THRESHOLD_M | DN_THRESHOLD_M,
			cpr->config->up_threshold << 24 |
			cpr->config->dn_threshold << 28);

	cpr->curr_osc = chip_data->ring_osc;
	chip_data->ring_osc_data[cpr->curr_osc].quot =
		cpr->config->max_quot;

	/**
	 * Program the gate count and target values
	 * for all the ring oscilators
	 */
	while (cnt < NUM_OSC) {
		cpr_modify_reg(cpr, RBCPR_GCNT_TARGET(cnt),
				(GCNT_M | TARGET_M),
				(chip_data->ring_osc_data[cnt].gcnt << 12 |
				chip_data->ring_osc_data[cnt].quot));
		pr_debug("RBCPR_GCNT_TARGET(%d): = 0x%x\n", cnt,
			readl_relaxed(cpr->base + RBCPR_GCNT_TARGET(cnt)));
		cnt++;
	}

	/* Configure the step quot */
	cpr_2pt_kv_analysis(cpr, chip_data);

	/**
	 * Call the PMIC specific routine to set the voltage
	 * Set with an extra step since it helps as per
	 * characterization data.
	 */
	chip_data->calibrated_uV +=  cpr->vp->step_size;
	tmp_uV = chip_data->calibrated_uV;
	rc = regulator_set_voltage(cpr->vreg_cx, tmp_uV, tmp_uV);
	if (rc)
		pr_err("%s: Voltage set failed %d\n", __func__, rc);

	/*
	 * Program the Timer Register for delay between CPR measurements
	 * This is required to allow the device sufficient time for idle
	 * power collapse.
	 */
	delay_count = TIMER_COUNT(cpr->config->ref_clk_khz,
					cpr->config->delay_us);
	cpr_write_reg(cpr, RBCPR_TIMER_INTERVAL, delay_count);

	/* Enable the Timer */
	cpr_modify_reg(cpr, RBCPR_CTL, TIMER_M, ENABLE_TIMER);

	/* Enable Auto ACK for Mid interrupts */
	cpr_modify_reg(cpr, RBCPR_CTL, SW_AUTO_CONT_ACK_EN_M,
			SW_AUTO_CONT_ACK_EN);
}

static int
cpr_freq_transition(struct notifier_block *nb, unsigned long val,
				void *data)
{
	struct msm_cpr *cpr = container_of(nb, struct msm_cpr, freq_transition);
	struct cpufreq_freqs *freqs = data;
	uint32_t quot, new_freq, ctl_reg;

	switch (val) {
	case CPUFREQ_PRECHANGE:
		pr_debug("pre freq change notification to cpr\n");

		/* Disable Measurement to stop generation of CPR IRQs */
		cpr_disable(cpr);
		/* Disable routing of IRQ to App */
		cpr_irq_set(cpr, INT_MASK & ~MID_INT, 0);
		disable_irq(cpr->irq);
		cpr_write_reg(cpr, RBIF_IRQ_CLEAR, ALL_CPR_IRQ);
		pr_debug("RBCPR_CTL: 0x%x\n",
			readl_relaxed(cpr->base + RBCPR_CTL));
		pr_debug("RBIF_IRQ_STATUS: 0x%x\n",
			cpr_read_reg(cpr, RBIF_IRQ_STATUS));
		pr_debug("RBIF_IRQ_EN(0): 0x%x\n",
			cpr_read_reg(cpr, RBIF_IRQ_EN(cpr->config->irq_line)));

		cpr->prev_mode = cpr->cpr_mode;
		break;

	case CPUFREQ_POSTCHANGE:
		pr_debug("post freq change notification to cpr\n");
		ctl_reg = cpr_read_reg(cpr, RBCPR_CTL);
		/**
		 * As per chip characterization data, use max nominal freq
		 * to calculate quot for all lower frequencies too
		 */
		if (freqs->new > cpr->config->max_nom_freq) {
			new_freq = freqs->new;
			cpr->cur_Vmin = cpr->config->cpr_mode_data[1].turbo_Vmin;
			cpr->cur_Vmax = cpr->config->cpr_mode_data[1].turbo_Vmax;
		} else {
			new_freq = cpr->config->max_nom_freq;
			cpr->cur_Vmin = cpr->config->cpr_mode_data[1].nom_Vmin;
			cpr->cur_Vmax = cpr->config->cpr_mode_data[1].nom_Vmax;
		}

		/* Configure CPR for the new frequency */
		quot = cpr->config->get_quot(cpr->config->max_quot,
						cpr->config->max_freq / 1000,
						new_freq / 1000);
		cpr_modify_reg(cpr, RBCPR_GCNT_TARGET(cpr->curr_osc), TARGET_M,
				quot);
		pr_debug("RBCPR_GCNT_TARGET(%d): = 0x%x\n", cpr->curr_osc,
			readl_relaxed(cpr->base +
					RBCPR_GCNT_TARGET(cpr->curr_osc)));
		pr_debug("%s: new_freq: %d, set_freq: %d, quot: %d\n", __func__,
			freqs->new, new_freq, quot);

		enable_irq(cpr->irq);
		/**
		 * Enable all interrupts. One of them could be in a disabled
		 * state if vdd had hit Vmax / Vmin earlier
		 */
		cpr_irq_set(cpr, INT_MASK & ~MID_INT, 1);

		/**
		 * Clear the auto NACK down bit if enabled in the freq.
		 * transition phase.
		 */
		if (ctl_reg & SW_AUTO_CONT_NACK_DN_EN)
			cpr_modify_reg(cpr, RBCPR_CTL,
				 SW_AUTO_CONT_NACK_DN_EN_M, 0);
		pr_debug("RBIF_IRQ_EN(0): 0x%x\n",
			cpr_read_reg(cpr, RBIF_IRQ_EN(cpr->config->irq_line)));
		pr_debug("RBCPR_CTL: 0x%x\n",
			readl_relaxed(cpr->base + RBCPR_CTL));
		pr_debug("RBIF_IRQ_STATUS: 0x%x\n",
			cpr_read_reg(cpr, RBIF_IRQ_STATUS));
		cpr_enable(cpr);
		break;
	default:
		break;
	}
	return 0;
}

#ifdef CONFIG_PM
static int msm_cpr_resume(struct device *dev)
{
	struct msm_cpr *cpr = dev_get_drvdata(dev);
	int osc_num = cpr->config->cpr_mode_data->ring_osc;

	cpr->config->clk_enable();

	cpr_write_reg(cpr, RBCPR_TIMER_INTERVAL,
		cpr_save_state.rbif_timer_interval);
	cpr_write_reg(cpr, RBIF_IRQ_EN(cpr->config->irq_line),
		cpr_save_state.rbif_int_en);
	cpr_write_reg(cpr, RBIF_LIMIT,
		cpr_save_state.rbif_limit);
	cpr_write_reg(cpr, RBIF_TIMER_ADJUST,
		cpr_save_state.rbif_timer_adjust);
	cpr_write_reg(cpr, RBCPR_GCNT_TARGET(osc_num),
		cpr_save_state.rbcpr_gcnt_target);
	cpr_write_reg(cpr, RBCPR_STEP_QUOT,
		cpr_save_state.rbcpr_step_quot);
	cpr_write_reg(cpr, RBIF_SW_VLEVEL,
		cpr_save_state.rbif_sw_level);
	cpr_write_reg(cpr, RBCPR_CTL,
		cpr_save_state.rbcpr_ctl);

	enable_irq(cpr->irq);
	cpr_enable(cpr);

	return 0;
}

static int msm_cpr_suspend(struct device *dev)

{
	struct msm_cpr *cpr = dev_get_drvdata(dev);
	int osc_num = cpr->config->cpr_mode_data->ring_osc;

	/* Disable CPR measurement before IRQ to avoid pending interrupts */
	cpr_disable(cpr);
	disable_irq(cpr->irq);

	cpr_save_state.rbif_timer_interval =
		cpr_read_reg(cpr, RBCPR_TIMER_INTERVAL);
	cpr_save_state.rbif_int_en =
		cpr_read_reg(cpr, RBIF_IRQ_EN(cpr->config->irq_line));
	cpr_save_state.rbif_limit =
		cpr_read_reg(cpr, RBIF_LIMIT);
	cpr_save_state.rbif_timer_adjust =
		cpr_read_reg(cpr, RBIF_TIMER_ADJUST);
	cpr_save_state.rbcpr_gcnt_target =
		cpr_read_reg(cpr, RBCPR_GCNT_TARGET(osc_num));
	cpr_save_state.rbcpr_step_quot =
		cpr_read_reg(cpr, RBCPR_STEP_QUOT);
	cpr_save_state.rbif_sw_level =
		cpr_read_reg(cpr, RBIF_SW_VLEVEL);
	cpr_save_state.rbcpr_ctl =
		cpr_read_reg(cpr, RBCPR_CTL);

	return 0;
}

void msm_cpr_pm_resume(void)
{
	msm_cpr_resume(&cpr_pdev->dev);
}
EXPORT_SYMBOL(msm_cpr_pm_resume);

void msm_cpr_pm_suspend(void)
{
	msm_cpr_suspend(&cpr_pdev->dev);
}
EXPORT_SYMBOL(msm_cpr_pm_suspend);
#endif

void msm_cpr_disable(void)
{
	struct msm_cpr *cpr = platform_get_drvdata(cpr_pdev);
	cpr_disable(cpr);
}
EXPORT_SYMBOL(msm_cpr_disable);

void msm_cpr_enable(void)
{
	struct msm_cpr *cpr = platform_get_drvdata(cpr_pdev);
	cpr_enable(cpr);
}
EXPORT_SYMBOL(msm_cpr_enable);

static int __devinit msm_cpr_probe(struct platform_device *pdev)
{
	int res, irqn, irq_enabled;
	struct msm_cpr *cpr;
	const struct msm_cpr_config *pdata = pdev->dev.platform_data;
	void __iomem *base;
	struct resource *mem;
	struct msm_cpr_mode *chip_data;

	if (!enable)
		return -EPERM;

	if (!pdata) {
		pr_err("CPR: Platform data is not available\n");
		return -EIO;
	}

	cpr = devm_kzalloc(&pdev->dev, sizeof(struct msm_cpr), GFP_KERNEL);
	if (!cpr)
		return -ENOMEM;

	/* Initialize platform_data */
	cpr->config = pdata;

	/* Set initial Vmin,Vmax equal to turbo */
	cpr->cur_Vmin = cpr->config->cpr_mode_data[1].turbo_Vmin;
	cpr->cur_Vmax = cpr->config->cpr_mode_data[1].turbo_Vmax;

	cpr_pdev = pdev;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem || !mem->start) {
		pr_err("CPR: get resource failed\n");
		res = -ENXIO;
		goto out;
	}

	base = ioremap_nocache(mem->start, resource_size(mem));
	if (!base) {
		pr_err("CPR: ioremap failed\n");
		res = -ENOMEM;
		goto out;
	}

	if (cpr->config->irq_line < 0) {
		pr_err("CPR: Invalid IRQ line specified\n");
		res = -ENXIO;
		goto err_ioremap;
	}
	irqn = platform_get_irq(pdev, cpr->config->irq_line);
	if (irqn < 0) {
		pr_err("CPR: Unable to get irq\n");
		res = -ENXIO;
		goto err_ioremap;
	}

	cpr->irq = irqn;

	cpr->base = base;

	cpr->vp = pdata->vp_data;

	mutex_init(&cpr->cpr_mutex);

	/* Initialize the Voltage domain for CPR */
	cpr->vreg_cx = regulator_get(&pdev->dev, "vddx_cx");
	if (IS_ERR(cpr->vreg_cx)) {
		res = PTR_ERR(cpr->vreg_cx);
		pr_err("could not get regulator: %d\n", res);
		goto err_reg_get;
	}

	/* Assume current mode is TURBO Mode */
	cpr->cpr_mode = TURBO_MODE;
	cpr->prev_mode = TURBO_MODE;

	/* Initial configuration of CPR */
	cpr_config(cpr);

	platform_set_drvdata(pdev, cpr);

	chip_data = &cpr->config->cpr_mode_data[cpr->cpr_mode];
	pr_info("CPR Platform Data (upside_steps: %d) (downside_steps: %d) ",
		cpr->config->up_threshold, cpr->config->dn_threshold);
	pr_info("(nominal_voltage: %duV) (turbo_voltage: %duV)\n",
		cpr->config->cpr_mode_data[NORMAL_MODE].calibrated_uV,
		cpr->config->cpr_mode_data[TURBO_MODE].calibrated_uV);
	pr_info("(Current corner: TURBO) (gcnt_target: %d) (quot: %d)\n",
		chip_data->ring_osc_data[chip_data->ring_osc].gcnt,
		chip_data->ring_osc_data[chip_data->ring_osc].quot);

	/* Initialze the Debugfs Entry for cpr */
	res = msm_cpr_debug_init(cpr->base);
	if (res) {
		pr_err("CPR: Debugfs Creation Failed\n");
		goto err_ioremap;
	}

	/* Register the interrupt handler for IRQ 0 */
	res = request_threaded_irq(irqn, NULL, cpr_irq0_handler,
			IRQF_TRIGGER_RISING, "msm-cpr-irq0", cpr);
	if (res) {
		pr_err("CPR: request irq failed for IRQ %d\n", irqn);
		goto err_ioremap;
	}

	/**
	 * Enable the requested interrupt lines.
	 * Do not enable MID_INT since we shall use
	 * SW_AUTO_CONT_ACK_EN bit.
	 */
	irq_enabled = INT_MASK & ~MID_INT;
	cpr_modify_reg(cpr, RBIF_IRQ_EN(cpr->config->irq_line),
			INT_MASK, irq_enabled);

	/* Enable the cpr */
	cpr_modify_reg(cpr, RBCPR_CTL, LOOP_EN_M, ENABLE_CPR);

	cpr->freq_transition.notifier_call = cpr_freq_transition;
	cpufreq_register_notifier(&cpr->freq_transition,
					CPUFREQ_TRANSITION_NOTIFIER);

	return res;

err_reg_get:
	free_irq(irqn, cpr);
err_ioremap:
	iounmap(base);
out:
	return res;
}

static int __devexit msm_cpr_remove(struct platform_device *pdev)
{
	struct msm_cpr *cpr = platform_get_drvdata(pdev);

	cpufreq_unregister_notifier(&cpr->freq_transition,
					CPUFREQ_TRANSITION_NOTIFIER);

	regulator_disable(cpr->vreg_cx);
	regulator_put(cpr->vreg_cx);
	free_irq(cpr->irq, cpr);
	iounmap(cpr->base);
	mutex_destroy(&cpr->cpr_mutex);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct dev_pm_ops msm_cpr_dev_pm_ops = {
	.suspend = msm_cpr_suspend,
	.resume = msm_cpr_resume,
};

static struct platform_driver msm_cpr_driver = {
	.probe = msm_cpr_probe,
	.remove = __devexit_p(msm_cpr_remove),
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &msm_cpr_dev_pm_ops,
#endif
	},
};

static int __init msm_init_cpr(void)
{
	return platform_driver_register(&msm_cpr_driver);
}

module_init(msm_init_cpr);

static void __exit msm_exit_cpr(void)
{
	platform_driver_unregister(&msm_cpr_driver);
}

module_exit(msm_exit_cpr);

MODULE_DESCRIPTION("MSM CPR Driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
