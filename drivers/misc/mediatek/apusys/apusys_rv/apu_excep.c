// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/iommu.h>
#include <linux/delay.h>
#include <mt-plat/aee.h>

#include "apu.h"
#include "apu_excep.h"
#include "apu_config.h"
#include "apusys_secure.h"
#include "hw_logger.h"
#include "apu_regdump.h"

static const uint32_t TaskContext[] = {
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // GPR
	0x0, // PC
	0x0, // SP
	0x300, // CSR
	0x341, // CSR
	0x301, // CSR
	0x315, // CSR
	0x7c0, // CSR
	0xb00, // CSR
	0xc01, // CSR
	0xb02, // CSR
	0xb80, // CSR
	0xc81, // CSR
	0xb82, // CSR
	0xbc0, // CSR
	0xbd0, // CSR
	0xf11, // CSR
	0xf12, // CSR
	0xf13, // CSR
	0xf14, // CSR
	0xfc1, // CSR
	0xfc2, // CSR
	0xfc3, // CSR
	0xfc4, // CSR
	0x5c0, // CSR
	0x5d0, // CSR
	0x5d1, // CSR
	0x5d2, // CSR
	0x5d3, // CSR
	0x5d4, // CSR
	0x5d5, // CSR
	0x5d6, // CSR
	0x5d7, // CSR
	0x5d8, // CSR
	0x5d9, // CSR
	0x5da, // CSR
	0x5db, // CSR
	0x5dc, // CSR
	0x5dd, // CSR
	0x5de, // CSR
	0x5df, // CSR
	0x5e0, // CSR
	0x5e1, // CSR
	0x5e2, // CSR
	0x5e3, // CSR
	0x5e4, // CSR
	0x5e5, // CSR
	0x5e6, // CSR
	0x5e7, // CSR
	0x5e8, // CSR
	0x5e9, // CSR
	0x5ea, // CSR
	0x5eb, // CSR
	0x5ec, // CSR
	0x5ed, // CSR
	0x5ee, // CSR
	0x5ef, // CSR
	0x5f0, // CSR
	0x5f1, // CSR
	0x5f2, // CSR
	0x5f3, // CSR
	0x5f4, // CSR
	0x5f5, // CSR
	0x5f6, // CSR
	0x5f7, // CSR
	0x302, // CSR
	0x303, // CSR
	0x304, // CSR
	0x344, // CSR
	0x9c0, // CSR
	0x9dc, // CSR
	0x9dd, // CSR
	0x9de, // CSR
	0x9df, // CSR
	0x9e0, // CSR
	0x9e1, // CSR
	0x9e2, // CSR
	0x9e3, // CSR
	0x9e4, // CSR
	0x9e5, // CSR
	0x9e6, // CSR
	0x9e7, // CSR
	0x9e8, // CSR
	0x9e9, // CSR
	0x9ea, // CSR
	0x9eb, // CSR
	0x9ec, // CSR
	0x9ed, // CSR
	0x9ee, // CSR
	0x9ef, // CSR
	0x9f0, // CSR
	0x9f1, // CSR
	0x9f2, // CSR
	0x9f3, // CSR
	0x9f4, // CSR
	0x9f5, // CSR
	0x9f6, // CSR
	0x9f7, // CSR
	0x9f8, // CSR
	0x9f9, // CSR
	0x9fa, // CSR
	0x9fb, // CSR
	0x9fc, // CSR
	0x9fd, // CSR
	0x9fe, // CSR
	0x9ff, // CSR
	0x7d0, // CSR
	0x7d1, // CSR
	0x7d2, // CSR
	0x7d3, // CSR
	0x7d4, // CSR
	0x7d8, // CSR
	0x7d9, // CSR
	0x7da, // CSR
	0x7db, // CSR
	0x340, // CSR
	0x342, // CSR
	0x343, // CSR
	0x7c5, // CSR
	0x7c6, // CSR
};

enum apusys_assert_module {
	assert_apusys_rv = 0,
	assert_apusys_power,
	assert_apusys_middleware,
	assert_apusys_edma,
	assert_apusys_mdla,
	assert_apusys_mvpu,
	assert_apusys_reviser,
	assert_apusys_devapc,
	assert_apusys_mnoc,
	assert_apusys_qos,

	assert_module_max,
};

static const char * const apusys_assert_module_name[assert_module_max] = {
	"APUSYS_RV",
	"APUSYS_POWER",
	"APUSYS_MIDDLEWARE",
	"APUSYS_EDMA",
	"APUSYS_MDLA",
	"APUSYS_MVPU",
	"APUSYS_REVISER",
	"APUSYS_DEVAPC",
	"APUSYS_MNOC",
	"APUSYS_QOS",
};

struct apu_coredump_work_struct {
	struct mtk_apu *apu;
	struct work_struct work;
};

static struct apu_coredump_work_struct apu_coredump_work;

static void apu_do_tcmdump(struct mtk_apu *apu)
{
	struct apu_coredump *coredump =
		(struct apu_coredump *) apu->coredump_buf;

	memcpy(coredump->tcmdump, (char *) apu->md32_tcm, TCM_SIZE);
}

static void apu_do_ramdump(struct mtk_apu *apu)
{
	struct apu_coredump *coredump =
		(struct apu_coredump *) apu->coredump_buf;

	/* first 128kB is only for bootstrap */
	memcpy(coredump->ramdump,
		(char *) apu->code_buf + DRAM_DUMP_OFFSET, DRAM_DUMP_SIZE);
}

static void dbg_apb_dw(struct mtk_apu *apu, uint32_t dbg_reg, uint32_t val)
{
	unsigned long flags;

	spin_lock_irqsave(&apu->reg_lock, flags);
	iowrite32(dbg_reg, apu->md32_debug_apb + DBG_INSTR);
	iowrite32(0x1, apu->md32_debug_apb + DBG_INSTR_WR);
	iowrite32(val, apu->md32_debug_apb + DBG_WDATA);
	iowrite32(0x1, apu->md32_debug_apb + DBG_WDATA_WR);
	spin_unlock_irqrestore(&apu->reg_lock, flags);
}

static void dbg_apb_iw(struct mtk_apu *apu, uint32_t dbg_cmd)
{
	unsigned long flags;

	spin_lock_irqsave(&apu->reg_lock, flags);
	iowrite32(dbg_cmd, apu->md32_debug_apb + DBG_INSTR);
	iowrite32(0x1, apu->md32_debug_apb + DBG_INSTR_WR);
	spin_unlock_irqrestore(&apu->reg_lock, flags);
}

static uint32_t dbg_apb_dr(struct mtk_apu *apu, uint32_t dbg_reg)
{
	unsigned long flags;
	uint32_t ret;

	spin_lock_irqsave(&apu->reg_lock, flags);
	iowrite32(dbg_reg, apu->md32_debug_apb + DBG_INSTR);
	iowrite32(0x1, apu->md32_debug_apb + DBG_INSTR_WR);
	ret = ioread32(apu->md32_debug_apb + DBG_RDATA);
	spin_unlock_irqrestore(&apu->reg_lock, flags);

	return ret;
}

static uint32_t dbg_read_csr(struct mtk_apu *apu, uint32_t csr_id)
{
	dbg_apb_dw(apu, DBG_INSTR_REG_INSTR, 0x2473 | (csr_id << 20));
	dbg_apb_iw(apu, DBG_EXECUTE_INSTR);
	dbg_apb_dw(apu, DBG_INSTR_REG_INSTR, 0x7DF41073);
	dbg_apb_iw(apu, DBG_EXECUTE_INSTR);

	return dbg_apb_dr(apu, DBG_DATA_REG_INSTR);
}

static uint32_t apusys_rv_smc_call(struct device *dev, uint32_t smc_id,
	uint32_t a2)
{
	struct arm_smccc_res res;

	dev_info(dev, "%s: smc call %d\n",
			__func__, smc_id);

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id,
				a2, 0, 0, 0, 0, 0, &res);
	if (smc_id == MTK_APUSYS_KERNEL_OP_APUSYS_RV_DBG_APB_ATTACH)
		dev_info(dev, "%s: smc call return(0x%x)\n",
			__func__, res.a0);
	else if (((int) res.a0) < 0)
		dev_info(dev, "%s: smc call %d return error(%d)\n",
			__func__, smc_id, res.a0);

	return res.a0;
}

static void apu_coredump_work_func(struct work_struct *p_work)
{
	unsigned long flags;
	int i, j;
	struct apu_coredump_work_struct *apu_coredump_work =
		container_of(p_work, struct apu_coredump_work_struct, work);
	struct mtk_apu *apu = apu_coredump_work->apu;
	struct device *dev = apu->dev;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	struct apu_coredump *coredump =
		(struct apu_coredump *) apu->coredump_buf;
	uint32_t pc, lr, sp;
	uint32_t reg_dump[REG_SIZE/sizeof(uint32_t)];
	uint32_t tbuf_dump[TBUF_SIZE/sizeof(uint32_t)];
	uint32_t tbuf_cur_ptr;
	uint32_t status, data, timeout;
	uint32_t val;
	uint32_t *ptr;

	/* bypass AP coredump flow if wdt timeout
	 * is triggered by uP exception
	 */
	if (apu->conf_buf->ramdump_type == 0x1) {
		if (apu->conf_buf->ramdump_module >= assert_module_max) {
			dev_info(dev, "%s: ramdump_module(%u) >= assert_module_max(%u)\n",
				__func__, apu->conf_buf->ramdump_module, assert_module_max);
			apu->conf_buf->ramdump_module = 0;
		}
		if ((apu->platdata->flags & F_SECURE_COREDUMP)) {
			apusys_rv_smc_call(dev,
				MTK_APUSYS_KERNEL_OP_APUSYS_RV_COREDUMP_SHADOW_COPY, 0);
			/* gating md32 cg for cache dump */
			apusys_rv_smc_call(dev,
				MTK_APUSYS_KERNEL_OP_APUSYS_RV_CG_GATING, 0);
			apusys_rv_smc_call(dev,
				MTK_APUSYS_KERNEL_OP_APUSYS_RV_CACHEDUMP, 0);
			apusys_rv_smc_call(dev,
				MTK_APUSYS_KERNEL_OP_APUSYS_RV_CLEAR_WDT_ISR, 0);
		}

		apu_regdump();
		apusys_rv_aee_warn(apusys_assert_module_name[apu->conf_buf->ramdump_module],
			"APUSYS_RV_EXCEPTION");
		dev_info(dev, "%s: done\n", __func__);
		return;
	}

	if ((apu->platdata->flags & F_SECURE_COREDUMP)) {
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_TCMDUMP, 0);

		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_RAMDUMP, 0);

		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_TBUFDUMP, 0);

		/* ungate md32 cg for debug apb connection */
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_CG_UNGATING, 0);
		status = apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_DBG_APB_ATTACH, 0);

		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_REGDUMP, (status & 0x1));

		/* gating md32 cg for cache dump */
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_CG_GATING, 0);
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_CACHEDUMP, 0);

		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_CLEAR_WDT_ISR, 0);
	} else {
		pc = ioread32(apu->md32_sysctrl + MON_PC);
		lr = ioread32(apu->md32_sysctrl + MON_LR);
		sp = ioread32(apu->md32_sysctrl + MON_SP);
		reg_dump[0] = REG_SIZE; // reg dump size
		reg_dump[1] = lr;
		reg_dump[2] = sp;
		reg_dump[32] = pc;
		reg_dump[33] = sp;

		apu_do_tcmdump(apu);
		apu_do_ramdump(apu);

		tbuf_cur_ptr = (ioread32(apu->md32_sysctrl + MD32_STATUS) >> 16 & 0x7);
		for (i = 0; i < 8; i++) {
			spin_lock_irqsave(&apu->reg_lock, flags);
			iowrite32(tbuf_cur_ptr, apu->md32_sysctrl + TBUF_DBG_SEL);
			spin_unlock_irqrestore(&apu->reg_lock, flags);
			if (tbuf_cur_ptr > 0)
				tbuf_cur_ptr--;
			else
				tbuf_cur_ptr = 7;
			for (j = 0; j < 4; j++) {
				tbuf_dump[i*4 + j] =
					ioread32(apu->md32_sysctrl +
						TBUF_DBG_DAT3 - j*4);
			}
		}
		memcpy(coredump->tbufdump, tbuf_dump, sizeof(tbuf_dump));

		spin_lock_irqsave(&apu->reg_lock, flags);

		/* ungate md32 cg for debug apb connection */
		if (!hw_ops->cg_ungating) {
			spin_unlock_irqrestore(&apu->reg_lock, flags);
			WARN_ON(1);
			return;
		}
		hw_ops->cg_ungating(apu);

		/* set DBG_EN */
		iowrite32(0x1, apu->md32_debug_apb + DBG_EN);
		/* set DBG MODE */
		iowrite32(0x0, apu->md32_debug_apb + DBG_MODE);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		/* ATTACH */
		dbg_apb_iw(apu, DBG_ATTACH_INSTR);
		/* REQUEST */
		dbg_apb_iw(apu, DBG_REQUEST_INSTR);
		/* Read DBG STATUS Register */
		status = dbg_apb_dr(apu, DBG_STATUS_REG_INSTR);

		dev_info(dev, "%s: status = 0x%x\n", __func__, status);

		timeout = 0;
		/* Check if RV33 go into DEBUG mode */
		while ((status & 0x1) != 0x1) {
			udelay(10);
			/* Read DBG STATUS Register */
			status = dbg_apb_dr(apu, DBG_STATUS_REG_INSTR);
			if (timeout++ > 100) {
				dev_info(dev, "%s: timeout\n", __func__);
				break;
			}
		}

		if ((status & 0x1) == 0x1) {
			reg_dump[0] = REG_SIZE; // reg dump size
			/* Read GPRs */
			for (i = 1; i < 32; i++) {
				val = 0x7DF01073 | ((uint32_t) i << 15);
				dbg_apb_dw(apu, DBG_INSTR_REG_INSTR, val);
				dbg_apb_iw(apu, DBG_EXECUTE_INSTR);
				reg_dump[i] = dbg_apb_dr(apu, DBG_DATA_REG_INSTR);
			}

			/* Read CSRs */
			for (i = 34; i < REG_SIZE/sizeof(uint32_t); i++)
				reg_dump[i] = dbg_read_csr(apu, TaskContext[i]);
		}

		if ((status & 0x1) == 0x1) { /* Read Cache Data */
			dbg_apb_dw(apu, DBG_ADDR_REG_INSTR, DRAM_DUMP_OFFSET);
			ptr = (uint32_t *) coredump->ramdump;
			/* read one word per loop */
			for (i = 0; i < DRAM_DUMP_SIZE/sizeof(uint32_t); i++) {
				dbg_apb_iw(apu, DBG_READ_DM);
				data = dbg_apb_dr(apu, DBG_DATA_REG_INSTR);
				ptr[i] = data;
			}
		}

		if (!hw_ops->cg_gating) {
			WARN_ON(1);
			return;
		}
		/* freeze md32 by gating cg */
		hw_ops->cg_gating(apu);

		if (!hw_ops->rv_cachedump) {
			WARN_ON(1);
			return;
		}
		hw_ops->rv_cachedump(apu);

		memcpy(coredump->regdump, reg_dump, sizeof(reg_dump));

		dsb(SY); /* may take lots of time */
	}

	apu_regdump();
	apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_TIMEOUT");
	dev_info(dev, "%s: done\n", __func__);
}

static irqreturn_t apu_wdt_isr(int irq, void *private_data)
{
	unsigned long flags;
	struct mtk_apu *apu = (struct mtk_apu *) private_data;
	struct device *dev = apu->dev;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	uint32_t val;

	if ((apu->platdata->flags & F_SECURE_COREDUMP)) {
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_CG_GATING, 0);

		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_DISABLE_WDT_ISR, 0);
	} else {
		val = ioread32(apu->apu_wdt);
		if (val != 0x1) {
			dev_info(dev, "%s: skip abnormal isr call(status = 0x%x)\n",
				__func__, val);
			return IRQ_HANDLED;
		}
		spin_lock_irqsave(&apu->reg_lock, flags);
		/* freeze md32 by turn off cg */
		if (!hw_ops->cg_gating) {
			spin_unlock_irqrestore(&apu->reg_lock, flags);
			WARN_ON(1);
			return -EINVAL;
		}
		hw_ops->cg_gating(apu);
		/* disable apu wdt */
		iowrite32(ioread32(apu->apu_wdt + 4) &
			(~(0x1U << 31)), apu->apu_wdt + 4);
		/* clear wdt interrupt */
		iowrite32(0x1, apu->apu_wdt);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
	}

	disable_irq_nosync(apu->wdt_irq_number);
	dev_info(dev, "%s: disable wdt_irq(%d)\n", __func__,
		apu->wdt_irq_number);

	dev_info(dev, "%s\n", __func__);

	schedule_work(&(apu_coredump_work.work));

	return IRQ_HANDLED;
}

static int apu_wdt_irq_register(struct platform_device *pdev,
	struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret = 0;

	apu->wdt_irq_number = platform_get_irq_byname(pdev, "apu_wdt");
	dev_info(dev, "%s: wdt_irq_number = %d\n", __func__, apu->wdt_irq_number);

	ret = devm_request_irq(&pdev->dev, apu->wdt_irq_number, apu_wdt_isr,
			irq_get_trigger_type(apu->wdt_irq_number),
			"apusys_wdt", apu);
	if (ret < 0)
		dev_info(dev, "%s: devm_request_irq Failed to request irq %d: %d\n",
				__func__, apu->wdt_irq_number, ret);

	return ret;
}

int apu_excep_init(struct platform_device *pdev, struct mtk_apu *apu)
{
	int ret = 0;

	INIT_WORK(&(apu_coredump_work.work), &apu_coredump_work_func);
	apu_coredump_work.apu = apu;
	ret = apu_wdt_irq_register(pdev, apu);
	if (ret < 0)
		return ret;

	return ret;
}

void apu_excep_remove(struct platform_device *pdev, struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	unsigned long flags;

	if ((apu->platdata->flags & F_SECURE_COREDUMP)) {
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_DISABLE_WDT_ISR, 0);
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_CLEAR_WDT_ISR, 0);
	} else {
		spin_lock_irqsave(&apu->reg_lock, flags);
		/* disable apu wdt */
		iowrite32(ioread32(apu->apu_wdt + 4) &
			(~(0x1U << 31)), apu->apu_wdt + 4);
		/* clear wdt interrupt */
		iowrite32(0x1, apu->apu_wdt);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
	}

	disable_irq(apu->wdt_irq_number);
	dev_info(dev, "%s: disable wdt_irq(%d)\n", __func__,
		apu->wdt_irq_number);

	cancel_work_sync(&(apu_coredump_work.work));
}
