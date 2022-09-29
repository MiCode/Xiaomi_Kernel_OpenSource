// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <emi_mpu.h>
#include <linux/arm-smccc.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <mt-plat/aee.h>
#include <soc/mediatek/emi.h>
#include <linux/ratelimit.h>

static const unsigned int bypass_register[] = {0x1d8, 0x3d8, 0x1e4, 0x3e4, 0x1e8, 0x3e8};
static const unsigned int bypass_axi_info[] = {0x6, 0x7f80, 0x2000, 0x7, 0xff01, 0x4001};
static const unsigned int miumpu_dump_info[] = {0, 2, 10, 2};
static const unsigned int miukp_dump_info[] = {0, 15, 2, 15};

static void set_regs(
	struct reg_info_t *reg_list, unsigned int reg_cnt,
	void __iomem *emi_cen_base)
{
	unsigned int i, j;

	for (i = 0; i < reg_cnt; i++)
		for (j = 0; j < reg_list[i].leng; j++)
			writel(reg_list[i].value, emi_cen_base +
				reg_list[i].offset + 4 * j);

	/*
	 * Use the memory barrier to make sure the interrupt signal is
	 * de-asserted (by programming registers) before exiting the
	 * ISR and re-enabling the interrupt.
	 */
	mb();
}

/*
 * mtk_emimpu_iommu_handling_register - register callback for iommu handling
 * @iommu_handling_func:	function point for md handling
 *
 * Return 0 for success, -EINVAL for fail
 */
int mtk_emimpu_iommu_handling_register(emimpu_iommu_handler iommu_handling_func)
{
	struct emi_mpu *mpu_v2;

	mpu_v2 = global_emi_mpu;

	if (!mpu_v2)
		return -EINVAL;

	if (!iommu_handling_func) {
		pr_info("%s: iommu_handling_func is NULL\n", __func__);
		return -EINVAL;
	}

	mpu_v2->iommu_handler = iommu_handling_func;

	pr_info("%s: iommu_handling_func registered!!\n", __func__);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_emimpu_iommu_handling_register);

/*
 * mtk_emimpu_isr_hook_register - register the by-platform hook function for ISR
 * @hook: the by-platform hook function
 *
 * Return 0 for success, -EINVAL for fail
 */
int mtk_emimpu_isr_hook_register(emimpu_isr_hook hook)
{
	struct emi_mpu *mpu_v2;

	mpu_v2 = global_emi_mpu;

	if (!mpu_v2)
		return -EINVAL;

	if (!hook) {
		pr_info("%s: hook is NULL\n", __func__);
		return -EINVAL;
	}

	mpu_v2->by_plat_isr_hook = hook;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_emimpu_isr_hook_register);

static void clear_violation(
	struct emi_mpu *mpu, unsigned int emi_id)
{
	void __iomem *emi_cen_base;
	void __iomem *miu_mpu_base;

	emi_cen_base = mpu->emi_cen_base[emi_id];
	miu_mpu_base = mpu->miu_mpu_base[emi_id];

	set_regs(mpu->clear_reg,
		mpu->clear_reg_cnt, emi_cen_base);

	set_regs(mpu->clear_hp_reg,
		mpu->clear_hp_reg_cnt, emi_cen_base);

	set_regs(mpu->clear_miumpu_reg,
		mpu->clear_miumpu_reg_cnt, miu_mpu_base);
}

static void clear_kp_violation(unsigned int emi_id)
{
	struct arm_smccc_res smc_res;

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_CLEAR_KP,
		emi_id, 0, 0, 0, 0, 0, &smc_res);
}

static void emimpu_vio_dump(struct work_struct *work)
{
	struct emi_mpu *mpu;

	mpu = global_emi_mpu;

	if (!mpu)
		return;

	if (mpu->vio_msg)
		aee_kernel_exception("EMIMPU_V2", mpu->vio_msg);

	mpu->in_msg_dump = 0;
}
static DECLARE_WORK(emimpu_work, emimpu_vio_dump);

static irqreturn_t emimpu_violation_irq(int irq, void *dev_id)
{
	struct emi_mpu *mpu = (struct emi_mpu *)dev_id;
	struct reg_info_t *dump_reg = mpu->dump_reg;
	struct reg_info_t *miumpu_dump_reg = mpu->miumpu_dump_reg;
	struct reg_info_t *miukp_dump_reg = mpu->miukp_dump_reg;
	void __iomem *emi_cen_base;
	void __iomem *miu_mpu_base;
	void __iomem *miu_kp_base;
	unsigned int emi_id, i;
	ssize_t msg_len;
	int nr_vio, prefetch, vio_dump_idx, vio_dump_pos;
	bool violation, miu_violation, miu_kp_violation, miu_mpu_violation,
		flag_clear_all_violation;
	irqreturn_t irqret;
	const unsigned int hp_mask = 0x600000;
	char md_str[MTK_EMI_MAX_CMD_LEN + 13] = {'\0'};
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 3);

	nr_vio = 0;
	msg_len = 0;
	prefetch = 0;


	for (emi_id = 0; emi_id < mpu->emi_cen_cnt; emi_id++) {
		violation = false;
		miu_violation = false;
		miu_kp_violation = false;
		miu_mpu_violation = false;
		flag_clear_all_violation = false;

		emi_cen_base = mpu->emi_cen_base[emi_id];
		for (i = 0; i < mpu->dump_cnt; i++) {
			dump_reg[i].value = readl(
				emi_cen_base + dump_reg[i].offset);
			if (dump_reg[i].value)
				violation = true;
		}

		if (!violation)
			continue;

		/*
		 * The hardware interrupt to service will be triggered by
		 * two sources: one is the new MIUMPU violation, another is
		 * the legacy EMIMPU violation.
		 * Determine whether this is a MIUMPU violation or a EMIMPU
		 * violation by checking HP_MODE.
		 * If this is a MIUMPU violation, the info to dump is from
		 * another set of registers.
		 */
		miu_violation = (dump_reg[2].value & hp_mask) ? true : false;
		if (miu_violation) {
			miu_mpu_violation = false;
			miu_mpu_base = mpu->miu_mpu_base[emi_id];
			for (i = 0; i < mpu->miumpu_dump_cnt; i++) {
				miumpu_dump_reg[i].value = readl(
				miu_mpu_base + miumpu_dump_reg[i].offset);
			}
			for (i = 0; i < mpu->miumpu_vio_dump_cnt; i++) {
				vio_dump_idx = mpu->miumpu_vio_dump_info[i].vio_dump_idx;
				vio_dump_pos = mpu->miumpu_vio_dump_info[i].vio_dump_pos;
				if (CHECK_BIT(miumpu_dump_reg[vio_dump_idx].value, vio_dump_pos))
					miu_mpu_violation = true;
			}

			miu_kp_violation = false;
			miu_kp_base = mpu->miu_kp_base[emi_id];
			for (i = 0; i < mpu->miukp_dump_cnt; i++) {
				miukp_dump_reg[i].value = readl(
				miu_kp_base + miukp_dump_reg[i].offset);
			}

			for (i = 0; i < mpu->kp_vio_dump_cnt; i++) {
				vio_dump_idx = mpu->kp_vio_dump_info[i].vio_dump_idx;
				vio_dump_pos = mpu->kp_vio_dump_info[i].vio_dump_pos;
				if (CHECK_BIT(miukp_dump_reg[vio_dump_idx].value, vio_dump_pos))
					miu_kp_violation = true;
			}


			if ((!miu_mpu_violation) && (!miu_kp_violation)) {
				flag_clear_all_violation = true;
				if (__ratelimit(&ratelimit))
					pr_info("%s: emi:%d miu_mpu = 0, miu_kp = 0"
					, __func__, emi_id);
				goto clear_violation;
			}
		}

		/*
		 * Have one hook function for one platform for handling
		 * by-platform problems. For example, bypass this violation
		 * since it is a false alarm.
		 * If the violation needs to be bypassed, clear the violation
		 * but do NOT write logs in the vio_msg buffer to avoid generate
		 * a violation report.
		 */
		if (mpu->by_plat_isr_hook) {
			irqret = mpu->by_plat_isr_hook(emi_id,
			(miu_violation) ? miumpu_dump_reg : dump_reg,
			(miu_violation) ? mpu->miumpu_dump_cnt : mpu->dump_cnt);

			if (irqret == IRQ_HANDLED)
				goto clear_violation;
		}

		nr_vio++;

		if (miu_violation) {

			if (miu_mpu_violation) {
				/* MPUT_2ND[28] = CPU-preftech flag*/
				prefetch = (dump_reg[2].value >> 28) & 0x1;
				if (msg_len < MTK_EMI_MAX_CMD_LEN)
					msg_len += scnprintf(mpu->vio_msg + msg_len,
						MTK_EMI_MAX_CMD_LEN - msg_len,
						"\n[MIUMPU]cpu-prefetch:%d\n", prefetch);
				/* Dump MIUMPU violation info */
				if (msg_len < MTK_EMI_MAX_CMD_LEN)
					msg_len += scnprintf(mpu->vio_msg + msg_len,
						MTK_EMI_MAX_CMD_LEN - msg_len,
						"\n[MIUMPU]emiid%d\n", emi_id);
				for (i = 0; i < mpu->miumpu_dump_cnt; i++)
					if (msg_len < MTK_EMI_MAX_CMD_LEN)
						msg_len += scnprintf(mpu->vio_msg + msg_len,
							MTK_EMI_MAX_CMD_LEN - msg_len,
							"[%x]%x;",
							miumpu_dump_reg[i].offset,
							miumpu_dump_reg[i].value);
			} else {
				/* Dump MIUKP violation info */
				if (msg_len < MTK_EMI_MAX_CMD_LEN)
					msg_len += scnprintf(mpu->vio_msg + msg_len,
						MTK_EMI_MAX_CMD_LEN - msg_len,
						"\n[MIUKP]emiid%d\n", emi_id);
				for (i = 0; i < mpu->miukp_dump_cnt; i++)
					if (msg_len < MTK_EMI_MAX_CMD_LEN)
						msg_len += scnprintf(mpu->vio_msg + msg_len,
							MTK_EMI_MAX_CMD_LEN - msg_len,
							"[%x]%x;",
							miukp_dump_reg[i].offset,
							miukp_dump_reg[i].value);
			}

		} else {
			/* Dump EMIMPU violation info */
			if (msg_len < MTK_EMI_MAX_CMD_LEN)
				msg_len += scnprintf(mpu->vio_msg + msg_len,
						MTK_EMI_MAX_CMD_LEN - msg_len,
						"\n[EMIMPU]emiid%d\n", emi_id);
			for (i = 0; i < mpu->dump_cnt; i++)
				if (msg_len < MTK_EMI_MAX_CMD_LEN)
					msg_len += scnprintf(mpu->vio_msg + msg_len,
						MTK_EMI_MAX_CMD_LEN - msg_len,
						"%s(%d),%s(%x),%s(%x);\n",
						"emi", emi_id,
						"off", dump_reg[i].offset,
						"val", dump_reg[i].value);
		}

		/*
		 * Whenever there is an EMI MPU violation, the Modem
		 * software would like to be notified immediately.
		 * This is because the Modem software wants to do
		 * its coredump as earlier as possible for debugging
		 * and analysis.
		 * (Even if the violated master is not Modem, it
		 *  may still need coredump for clarification.)
		 * Have a hook function in the EMI MPU ISR for this
		 * purpose.
		 */
		if (mpu->md_handler) {
			strncpy(md_str, "emi-mpu-v2.c", 13);
			strncat(md_str, mpu->vio_msg, sizeof(md_str) - strlen(md_str) - 1);
			mpu->md_handler(md_str);
		}

		/*
		 * IOMMU IRQ not connected on mt6983, mt6879 and mt6895.
		 * IOMMU uses SRINFO=3 to make SMPU violation when
		 * translation fault. This behavior is security spec
		 * for IOMMU. Throuh having hook function, IOMMU will
		 * be nodified when violoation occurred.
		 */
		if (mpu->iommu_handler) {
			mpu->iommu_handler(emi_id,
				dump_reg, mpu->dump_cnt);
		}

clear_violation:
		clear_violation(mpu, emi_id);
		if (miu_kp_violation || flag_clear_all_violation)
			clear_kp_violation(emi_id);
	}

	if (nr_vio) {
		printk_deferred("%s: %s", __func__, mpu->vio_msg);
		mpu->in_msg_dump = 1;
		schedule_work(&emimpu_work);
	}

	return IRQ_HANDLED;
}

static const struct of_device_id emimpu_of_ids[] = {
	{.compatible = "mediatek,mt6983-emimpu",},
	{}
};
MODULE_DEVICE_TABLE(of, emimpu_of_ids);

static int emimpu_probe(struct platform_device *pdev)
{
	struct device_node *emimpu_node = pdev->dev.of_node;
	struct device_node *emicen_node =
		of_parse_phandle(emimpu_node, "mediatek,emi-reg", 0);
	struct device_node *miukp_node =
		of_parse_phandle(emimpu_node, "mediatek,miukp-reg", 0);
	struct device_node *miumpu_node =
		of_parse_phandle(emimpu_node, "mediatek,miumpu-reg", 0);
	struct emi_mpu *mpu;
	int ret, size, i, axi_set_num;
	struct resource *res;
	unsigned int *dump_list, *miukp_dump_list, *miumpu_dump_list, *miumpu_bypass_list;

	dev_info(&pdev->dev, "driver probed\n");

	if (!emicen_node) {
		dev_err(&pdev->dev, "No emi-reg\n");
		return -ENXIO;
	}

	if (!miukp_node) {
		dev_err(&pdev->dev, "No miukp-reg\n");
		return -ENXIO;
	}

	if (!miumpu_node) {
		dev_err(&pdev->dev, "No miumpu-reg\n");
		return -ENXIO;
	}

	mpu = devm_kzalloc(&pdev->dev,
		sizeof(struct emi_mpu), GFP_KERNEL);
	if (!mpu)
		return -ENOMEM;
//dump emi start
	size = of_property_count_elems_of_size(emimpu_node,
		"dump", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No dump\n");
		return -ENXIO;
	}
	dump_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!dump_list)
		return -ENOMEM;

	size >>= 2;
	mpu->dump_cnt = size;
	ret = of_property_read_u32_array(emimpu_node, "dump",
		dump_list, size);
	if (ret) {
		dev_err(&pdev->dev, "No emi dump\n");
		return -ENXIO;
	}
	mpu->dump_reg = devm_kmalloc(&pdev->dev,
		size * sizeof(struct reg_info_t), GFP_KERNEL);
	if (!(mpu->dump_reg))
		return -ENOMEM;

	for (i = 0; i < mpu->dump_cnt; i++) {
		mpu->dump_reg[i].offset = dump_list[i];
		mpu->dump_reg[i].value = 0;
		mpu->dump_reg[i].leng = 0;
	}
//dump emi end
//dump miu kp start
	size = of_property_count_elems_of_size(miukp_node,
		"dump", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No miu kp dump\n");
		return -ENXIO;
	}
	miukp_dump_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!miukp_dump_list)
		return -ENOMEM;
	size >>= 2;
	mpu->miukp_dump_cnt = size;
	ret = of_property_read_u32_array(miukp_node, "dump",
		miukp_dump_list, size);
	if (ret) {
		dev_err(&pdev->dev, "No  miu kp dump\n");
		return -ENXIO;
	}
	mpu->miukp_dump_reg = devm_kmalloc(&pdev->dev,
		size * sizeof(struct reg_info_t), GFP_KERNEL);
	if (!(mpu->miukp_dump_reg))
		return -ENOMEM;

	for (i = 0; i < mpu->miukp_dump_cnt; i++) {
		mpu->miukp_dump_reg[i].offset = miukp_dump_list[i];
		mpu->miukp_dump_reg[i].value = 0;
		mpu->miukp_dump_reg[i].leng = 0;
	}
//dump miu kp end
//dump miu mpu start
	size = of_property_count_elems_of_size(miumpu_node,
		"dump", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No miu mpu dump\n");
		return -ENXIO;
	}
	miumpu_dump_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!miumpu_dump_list)
		return -ENOMEM;

	size >>= 2;
	mpu->miumpu_dump_cnt = size;
	ret = of_property_read_u32_array(miumpu_node, "dump",
		miumpu_dump_list, size);
	if (ret) {
		dev_err(&pdev->dev, "No miu mpu dump\n");
		return -ENXIO;
	}
	mpu->miumpu_dump_reg = devm_kmalloc(&pdev->dev,
		size * sizeof(struct reg_info_t), GFP_KERNEL);
	if (!(mpu->miumpu_dump_reg))
		return -ENOMEM;

	for (i = 0; i < mpu->miumpu_dump_cnt; i++) {
		mpu->miumpu_dump_reg[i].offset = miumpu_dump_list[i];
		mpu->miumpu_dump_reg[i].value = 0;
		mpu->miumpu_dump_reg[i].leng = 0;
	}
//dump miu mpu end
//dump miumpu vio info start
	size = of_property_count_elems_of_size(miumpu_node,
		"vio-info", sizeof(char));
	if (size <= 0) {
		size = sizeof(miumpu_dump_info);
		mpu->miumpu_vio_dump_cnt = size/sizeof(struct vio_dump_info_t);
		mpu->miumpu_vio_dump_info = devm_kmalloc(&pdev->dev,
			size, GFP_KERNEL);
		if (!(mpu->miumpu_vio_dump_info))
			return -ENOMEM;
		memcpy(mpu->miumpu_vio_dump_info, miumpu_dump_info, size);
	} else {
		mpu->miumpu_vio_dump_cnt = size/sizeof(struct vio_dump_info_t);
		mpu->miumpu_vio_dump_info = devm_kmalloc(&pdev->dev,
			size, GFP_KERNEL);
		if (!(mpu->miumpu_vio_dump_info))
			return -ENOMEM;
		size >>= 2;
		ret = of_property_read_u32_array(miumpu_node, "vio-info",
			(unsigned int *)(mpu->miumpu_vio_dump_info), size);
		if (ret)
			return -ENXIO;
	}
//dump miumpu vio info end
//dump kp vio info start
	size = of_property_count_elems_of_size(miukp_node,
		"vio-info", sizeof(char));
	if (size <= 0) {
		size = sizeof(miukp_dump_info);
		mpu->kp_vio_dump_cnt = size/sizeof(struct vio_dump_info_t);
		mpu->kp_vio_dump_info = devm_kmalloc(&pdev->dev,
			size, GFP_KERNEL);
		if (!(mpu->kp_vio_dump_info))
			return -ENOMEM;
		memcpy(mpu->kp_vio_dump_info, miukp_dump_info, size);
	} else {
		mpu->kp_vio_dump_cnt = size/sizeof(struct vio_dump_info_t);
		mpu->kp_vio_dump_info = devm_kmalloc(&pdev->dev,
			size, GFP_KERNEL);
		if (!(mpu->kp_vio_dump_info))
			return -ENOMEM;
		size >>= 2;
		ret = of_property_read_u32_array(miukp_node, "vio-info",
			(unsigned int *)(mpu->kp_vio_dump_info), size);
		if (ret)
			return -ENXIO;
	}
//dump kp vio info end

//clear start
	size = of_property_count_elems_of_size(emimpu_node,
		"clear", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No clear\n");
		return  -ENXIO;
	}
	mpu->clear_reg = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(mpu->clear_reg))
		return -ENOMEM;

	mpu->clear_reg_cnt = size / sizeof(struct reg_info_t);
	size >>= 2;
	ret = of_property_read_u32_array(emimpu_node, "clear",
		(unsigned int *)(mpu->clear_reg), size);
	if (ret) {
		dev_err(&pdev->dev, "No clear\n");
		return -ENXIO;
	}
//clear end

//clear md start
	size = of_property_count_elems_of_size(emimpu_node,
		"clear_md", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No clear_md\n");
		return -ENXIO;
	}
	mpu->clear_md_reg = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(mpu->clear_md_reg))
		return -ENOMEM;

	mpu->clear_md_reg_cnt = size / sizeof(struct reg_info_t);
	size >>= 2;
	ret = of_property_read_u32_array(emimpu_node, "clear_md",
		(unsigned int *)(mpu->clear_md_reg), size);
	if (ret) {
		dev_err(&pdev->dev, "No clear_md\n");
		return -ENXIO;
	}
//clear md end
//clear hp start
	size = of_property_count_elems_of_size(emimpu_node,
		"clear_hp", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No clear_hp\n");
		return -ENXIO;
	}
	mpu->clear_hp_reg = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(mpu->clear_hp_reg))
		return -ENOMEM;

	mpu->clear_hp_reg_cnt = size / sizeof(struct reg_info_t);
	size >>= 2;
	ret = of_property_read_u32_array(emimpu_node, "clear_hp",
		(unsigned int *)(mpu->clear_hp_reg), size);
	if (ret) {
		dev_err(&pdev->dev, "No clear_hp\n");
		return -ENXIO;
	}
//clear hp end
//clear miukp start
	size = of_property_count_elems_of_size(miukp_node,
		"clear", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No clear miu kp\n");
		return -ENXIO;
	}
	mpu->clear_miukp_reg = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(mpu->clear_miukp_reg))
		return -ENOMEM;

	mpu->clear_miukp_reg_cnt = size / sizeof(struct reg_info_t);
	size >>= 2;
	ret = of_property_read_u32_array(miukp_node, "clear",
		(unsigned int *)(mpu->clear_miukp_reg), size);
	if (ret) {
		dev_err(&pdev->dev, "No clear miu kp\n");
		return -ENXIO;
	}
//clear miukp end
//clear miumpu start
	size = of_property_count_elems_of_size(miumpu_node,
		"clear", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No clear miu mpu\n");
		return -ENXIO;
	}
	mpu->clear_miumpu_reg = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(mpu->clear_miumpu_reg))
		return -ENOMEM;

	mpu->clear_miumpu_reg_cnt = size / sizeof(struct reg_info_t);
	size >>= 2;
	ret = of_property_read_u32_array(miumpu_node, "clear",
		(unsigned int *)(mpu->clear_miumpu_reg), size);
	if (ret) {
		dev_err(&pdev->dev, "No clear miu mpu\n");
		return -ENXIO;
	}
//clear miumpu end
//bypass miumpu start
	size = of_property_count_elems_of_size(miumpu_node,
		"bypass", sizeof(char));
	if (size <= 0) {
		/* for previous platform */
		size = sizeof(bypass_register) / sizeof(unsigned int);
		mpu->bypass_miu_reg_num = size;
		mpu->bypass_miu_reg = devm_kmalloc(&pdev->dev,
			size * sizeof(unsigned int), GFP_KERNEL);
		if (!(mpu->bypass_miu_reg))
			return -ENOMEM;

		for (i = 0; i < mpu->bypass_miu_reg_num; i++)
			mpu->bypass_miu_reg[i] = bypass_register[i];
	} else {
		miumpu_bypass_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
		if (!miumpu_bypass_list)
			return -ENOMEM;

		size /= sizeof(unsigned int);
		mpu->bypass_miu_reg_num = size;
		ret = of_property_read_u32_array(miumpu_node, "bypass",
			miumpu_bypass_list, size);
		if (ret) {
			pr_info("No bypass miu mpu\n");
			return -ENXIO;
		}
		mpu->bypass_miu_reg = devm_kmalloc(&pdev->dev,
			size * sizeof(unsigned int), GFP_KERNEL);
		if (!(mpu->bypass_miu_reg))
			return -ENOMEM;

		for (i = 0; i < mpu->bypass_miu_reg_num; i++)
			mpu->bypass_miu_reg[i] = miumpu_bypass_list[i];
	}
//bypass miumpu end
//bypass_axi miumpu start
	size = of_property_count_elems_of_size(miumpu_node,
		"bypass-axi", sizeof(char));
	if (size <= 0) {
		/* for previous platform */
		size = sizeof(bypass_axi_info) / sizeof(unsigned int);
		axi_set_num = AXI_SET_NUM(size);
		mpu->bypass_axi_num = axi_set_num;
		mpu->bypass_axi = devm_kmalloc(&pdev->dev,
			axi_set_num * sizeof(struct bypass_aix_info_t), GFP_KERNEL);
		if (!(mpu->bypass_axi))
			return -ENOMEM;

		for (i = 0; i < mpu->bypass_axi_num; i++) {
			mpu->bypass_axi[i].port = bypass_axi_info[(i*3)+0];
			mpu->bypass_axi[i].axi_mask = bypass_axi_info[(i*3)+1];
			mpu->bypass_axi[i].axi_value = bypass_axi_info[(i*3)+2];
		}
	} else {
		miumpu_bypass_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
		if (!miumpu_bypass_list)
			return -ENOMEM;

		size /= sizeof(unsigned int);
		axi_set_num = AXI_SET_NUM(size);
		mpu->bypass_axi_num = axi_set_num;
		ret = of_property_read_u32_array(miumpu_node, "bypass-axi",
			miumpu_bypass_list, size);
		if (ret) {
			pr_info("No bypass miu mpu\n");
			return -ENXIO;
		}
		mpu->bypass_axi = devm_kmalloc(&pdev->dev,
			axi_set_num * sizeof(struct bypass_aix_info_t), GFP_KERNEL);
		if (!(mpu->bypass_axi))
			return -ENOMEM;

		for (i = 0; i < mpu->bypass_axi_num; i++) {
			mpu->bypass_axi[i].port = miumpu_bypass_list[(i*3)+0];
			mpu->bypass_axi[i].axi_mask = miumpu_bypass_list[(i*3)+1];
			mpu->bypass_axi[i].axi_value = miumpu_bypass_list[(i*3)+2];
		}
	}
//bypass_axi miumpu end
//reg base start
	mpu->emi_cen_cnt = of_property_count_elems_of_size(
			emicen_node, "reg", sizeof(unsigned int) * 4);
	if (mpu->emi_cen_cnt <= 0) {
		dev_err(&pdev->dev, "No reg\n");
		return -ENXIO;
	}

	mpu->emi_cen_base = devm_kmalloc_array(&pdev->dev,
		mpu->emi_cen_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(mpu->emi_cen_base))
		return -ENOMEM;

	mpu->emi_mpu_base = devm_kmalloc_array(&pdev->dev,
		mpu->emi_cen_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(mpu->emi_mpu_base))
		return -ENOMEM;

	mpu->miu_kp_base = devm_kmalloc_array(&pdev->dev,
		mpu->emi_cen_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(mpu->miu_kp_base))
		return -ENOMEM;

	mpu->miu_mpu_base = devm_kmalloc_array(&pdev->dev,
		mpu->emi_cen_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(mpu->miu_mpu_base))
		return -ENOMEM;

	for (i = 0; i < mpu->emi_cen_cnt; i++) {
		mpu->emi_cen_base[i] = of_iomap(emicen_node, i);
		if (IS_ERR(mpu->emi_cen_base[i])) {
			dev_err(&pdev->dev, "Failed to map EMI%d CEN base\n",
				i);
			return -EIO;
		}

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		mpu->emi_mpu_base[i] =
			devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(mpu->emi_mpu_base[i])) {
			dev_err(&pdev->dev, "Failed to map EMI%d MPU base\n",
				i);
			return -EIO;
		}

		mpu->miu_kp_base[i] = of_iomap(miukp_node, i);
		if (IS_ERR(mpu->miu_kp_base[i])) {
			dev_err(&pdev->dev, "Failed to map MIU%d kernel protection base\n",
				i);
			return -EIO;
		}

		mpu->miu_mpu_base[i] = of_iomap(miumpu_node, i);
		if (IS_ERR(mpu->miu_mpu_base[i])) {
			dev_err(&pdev->dev, "Failed to map MIU%d secure range base\n",
				i);
			return -EIO;
		}
	}
//reg base end
	mpu->vio_msg = devm_kmalloc(&pdev->dev,
		MTK_EMI_MAX_CMD_LEN, GFP_KERNEL);
	if (!(mpu->vio_msg))
		return -ENOMEM;

	global_emi_mpu = mpu;
	platform_set_drvdata(pdev, mpu);

	mpu->irq = irq_of_parse_and_map(emimpu_node, 0);
	if (mpu->irq == 0) {
		dev_err(&pdev->dev, "Failed to get irq resource\n");
		return -ENXIO;
	}
	ret = request_irq(mpu->irq, (irq_handler_t)emimpu_violation_irq,
		IRQF_TRIGGER_NONE, "emimpu", mpu);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq");
		return -EINVAL;
	}

	mpu->version = EMIMPUVER2;

	devm_kfree(&pdev->dev, dump_list);
	devm_kfree(&pdev->dev, miukp_dump_list);
	devm_kfree(&pdev->dev, miumpu_dump_list);

	return 0;
}

static int emimpu_remove(struct platform_device *pdev)
{
	struct emi_mpu *mpu = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "driver removed\n");

	free_irq(mpu->irq, mpu);

	flush_work(&emimpu_work);

	global_emi_mpu = NULL;

	return 0;
}

static struct platform_driver emimpu_driver = {
	.probe = emimpu_probe,
	.remove = emimpu_remove,
	.driver = {
		.name = "emimpu_v2_driver",
		.owner = THIS_MODULE,
		.of_match_table = emimpu_of_ids,
	},
};

static __init int emimpu_init(void)
{
	int ret;

	pr_info("emimpu_v2 was loaded\n");

	ret = platform_driver_register(&emimpu_driver);
	if (ret) {
		pr_info("emimpu_v2: failed to register driver\n");
		return ret;
	}

	return 0;
}

module_init(emimpu_init);

MODULE_DESCRIPTION("MediaTek EMI MPU V2 Driver");
MODULE_LICENSE("GPL v2");
