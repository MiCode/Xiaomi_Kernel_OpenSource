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

/* global pointer for exported functions */
struct emi_mpu *global_emi_mpu;
EXPORT_SYMBOL_GPL(global_emi_mpu);

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

static void clear_violation(
	struct emi_mpu *mpu, unsigned int emi_id)
{
	void __iomem *emi_cen_base;

	emi_cen_base = mpu->emi_cen_base[emi_id];

	set_regs(mpu->clear_reg,
		mpu->clear_reg_cnt, emi_cen_base);

	if (mpu->post_clear)
		mpu->post_clear(emi_id);
}

static void emimpu_vio_dump(struct work_struct *work)
{
	struct emi_mpu *mpu;
	struct emimpu_dbg_cb *curr_dbg_cb;

	mpu = global_emi_mpu;
	if (!mpu)
		return;

	for (curr_dbg_cb = mpu->dbg_cb_list; curr_dbg_cb;
		curr_dbg_cb = curr_dbg_cb->next_dbg_cb)
		curr_dbg_cb->func();

	if (mpu->vio_msg)
		aee_kernel_exception("EMIMPU", mpu->vio_msg);

	mpu->in_msg_dump = 0;
}
static DECLARE_WORK(emimpu_work, emimpu_vio_dump);

static irqreturn_t emimpu_violation_irq(int irq, void *dev_id)
{
	struct emi_mpu *mpu = (struct emi_mpu *)dev_id;
	struct reg_info_t *dump_reg = mpu->dump_reg;
	void __iomem *emi_cen_base;
	unsigned int emi_id, i;
	ssize_t msg_len;
	int n, nr_vio;
	bool violation;
	char md_str[MTK_EMI_MAX_CMD_LEN + 10] = {'\0'};

	if (mpu->in_msg_dump)
		goto ignore_violation;

	n = snprintf(mpu->vio_msg, MTK_EMI_MAX_CMD_LEN, "violation\n");
	msg_len = (n < 0) ? 0 : (ssize_t)n;

	nr_vio = 0;
	for (emi_id = 0; emi_id < mpu->emi_cen_cnt; emi_id++) {
		violation = false;
		emi_cen_base = mpu->emi_cen_base[emi_id];

		for (i = 0; i < mpu->dump_cnt; i++) {
			dump_reg[i].value = readl(
				emi_cen_base + dump_reg[i].offset);

			if (msg_len < MTK_EMI_MAX_CMD_LEN) {
				n = snprintf(mpu->vio_msg + msg_len,
					MTK_EMI_MAX_CMD_LEN - msg_len,
					"%s(%d),%s(%x),%s(%x);\n",
					"emi", emi_id,
					"off", dump_reg[i].offset,
					"val", dump_reg[i].value);
				msg_len += (n < 0) ? 0 : (ssize_t)n;
			}

			if (dump_reg[i].value)
				violation = true;
		}

		if (!violation)
			continue;

		/*
		 * The DEVAPC module used the EMI MPU interrupt on some
		 * old smart-phone SoC. For these SoC, the DEVAPC driver
		 * will register a handler for processing its interrupt.
		 * If the handler has processed DEVAPC interrupt (and
		 * returns IRQ_HANDLED), just skip dumping and exit.
		 */
		if (mpu->pre_handler)
			if (mpu->pre_handler(emi_id, dump_reg,
					mpu->dump_cnt) == IRQ_HANDLED) {
				clear_violation(mpu, emi_id);
				mtk_clear_md_violation();
				continue;
			}

		nr_vio++;

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
			strncpy(md_str, "emi-mpu.c", 10);
			strncat(md_str, mpu->vio_msg, sizeof(md_str) - strlen(md_str) - 1);
			mpu->md_handler(md_str);
		}
	}

	if (nr_vio) {
		pr_info("%s: %s", __func__, mpu->vio_msg);
		mpu->in_msg_dump = 1;
		schedule_work(&emimpu_work);
	}

ignore_violation:
	for (emi_id = 0; emi_id < mpu->emi_cen_cnt; emi_id++)
		clear_violation(mpu, emi_id);

	return IRQ_HANDLED;
}

int emimpu_ap_region_init(void)
{
	struct emi_mpu *mpu;

	mpu = global_emi_mpu;
	if (!mpu)
		return 0;

	if (!(mpu->ap_rg_info))
		return 0;

	pr_info("%s: enable AP region\n", __func__);

	mtk_emimpu_set_protection(mpu->ap_rg_info);
	mtk_emimpu_free_region(mpu->ap_rg_info);

	kfree(mpu->ap_rg_info);
	mpu->ap_rg_info = NULL;

	return 0;
}
EXPORT_SYMBOL(emimpu_ap_region_init);

/*
 * mtk_emimpu_init_region - init rg_info's apc data with default forbidden
 * @rg_info:	the target region for init
 * @rg_num:	the region id for the rg_info
 *
 * Returns 0 on success, -EINVAL if rg_info or rg_num is invalid,
 * -ENODEV if the emi-mpu driver is not probed successfully,
 * -ENOMEM if out of memory
 */
int mtk_emimpu_init_region(
	struct emimpu_region_t *rg_info, unsigned int rg_num)
{
	struct emi_mpu *mpu;
	unsigned int size;
	unsigned int i;

	if (rg_info)
		memset(rg_info, 0, sizeof(struct emimpu_region_t));
	else
		return -EINVAL;

	mpu = global_emi_mpu;
	if (!mpu)
		return -ENODEV;

	if (rg_num >= mpu->region_cnt) {
		pr_info("%s: fail, out-of-range region\n", __func__);
		return -EINVAL;
	}

	size = sizeof(unsigned int) * mpu->domain_cnt;
	rg_info->apc = kmalloc(size, GFP_KERNEL);
	if (!(rg_info->apc))
		return -ENOMEM;
	for (i = 0; i < mpu->domain_cnt; i++)
		rg_info->apc[i] = MTK_EMIMPU_FORBIDDEN;

	rg_info->rg_num = rg_num;

	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_init_region);

/*
 * mtk_emi_mpu_free_region - free the apc data in rg_info
 * @rg_info:	the target region for free
 *
 * Returns 0 on success, -EINVAL if rg_info is invalid
 */
int mtk_emimpu_free_region(struct emimpu_region_t *rg_info)
{
	if (rg_info && rg_info->apc) {
		kfree(rg_info->apc);
		return 0;
	} else
		return -EINVAL;
}
EXPORT_SYMBOL(mtk_emimpu_free_region);

/*
 * mtk_emimpu_set_addr - set the address space
 * @rg_info:	the target region for address setting
 * @start:	the start address
 * @end:	the end address
 *
 * Returns 0 on success, -EINVAL if rg_info is invalid
 */
int mtk_emimpu_set_addr(struct emimpu_region_t *rg_info,
	unsigned long long start, unsigned long long end)
{
	if (rg_info) {
		rg_info->start = start;
		rg_info->end = end;
		return 0;
	} else
		return -EINVAL;
}
EXPORT_SYMBOL(mtk_emimpu_set_addr);

/*
 * mtk_emimpu_set_apc - set access permission for target domain
 * @rg_info:	the target region for apc setting
 * @d_num:	the target domain id
 * @apc:	the access permission setting
 *
 * Returns 0 on success, -EINVAL if rg_info or d_num is invalid,
 * -ENODEV if the emi-mpu driver is not probed successfully
 */
int mtk_emimpu_set_apc(struct emimpu_region_t *rg_info,
	unsigned int d_num, unsigned int apc)
{
	struct emi_mpu *mpu;

	if (!rg_info)
		return -EINVAL;

	mpu = global_emi_mpu;
	if (!mpu)
		return -ENODEV;

	if (d_num >= mpu->domain_cnt) {
		pr_info("%s: fail, out-of-range domain\n", __func__);
		return -EINVAL;
	}

	rg_info->apc[d_num] = apc & 0x7;

	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_set_apc);

/*
 * mtk_emimpu_lock_region - set lock for target region
 * @rg_info:	the target region for lock
 * @lock:	enable/disable lock
 *
 * Returns 0 on success, -EINVAL if rg_info is invalid
 */
int mtk_emimpu_lock_region(struct emimpu_region_t *rg_info, bool lock)
{
	if (rg_info) {
		rg_info->lock = lock;
		return 0;
	} else
		return -EINVAL;
}
EXPORT_SYMBOL(mtk_emimpu_lock_region);

/*
 * mtk_emimpu_set_protection - set emimpu protect into device
 * @rg_info:	the target region information
 *
 * Returns 0 on success, -EINVAL if rg_info is invalid,
 * -ENODEV if the emi-mpu driver is not probed successfully,
 * -EPERM if the SMC call returned failure
 */
int mtk_emimpu_set_protection(struct emimpu_region_t *rg_info)
{
	struct emi_mpu *mpu;
	unsigned int start, end;
	unsigned int group_apc;
	unsigned int d_group;
	struct arm_smccc_res smc_res;
	int i, j;

	if (!rg_info)
		return -EINVAL;

	if (!(rg_info->apc)) {
		pr_info("%s: fail, protect without init\n", __func__);
		return -EINVAL;
	}

	mpu = global_emi_mpu;
	if (!mpu)
		return -ENODEV;

	d_group = mpu->domain_cnt / 8;

	start = (unsigned int)
		(rg_info->start >> (mpu->addr_align)) |
		(rg_info->rg_num << 24);

	for (i = d_group - 1; i >= 0; i--) {
		end = (unsigned int)
			(rg_info->end >> (mpu->addr_align)) |
			(i << 24);

		for (group_apc = 0, j = 0; j < 8; j++)
			group_apc |=
				((rg_info->apc[i * 8 + j]) & 0x7) << (3 * j);
		if ((i == 0) && rg_info->lock)
			group_apc |= 0x80000000;

		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_SET,
			start, end, group_apc, 0, 0, 0, &smc_res);
		if (smc_res.a0) {
			pr_info("%s:%d failed to set region permission, ret=0x%lx\n",
				__func__, __LINE__, smc_res.a0);
			return -EPERM;
		}

	}

	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_set_protection);

/*
 * mtk_emimpu_clear_protection - clear emimpu protection
 * @rg_info:	the target region information
 *
 * Returns 0 on success, -EINVAL if rg_info is invalid,
 * -ENODEV if the emi-mpu driver is not probed successfully,
 * -EPERM if the SMC call returned failure
 */
int mtk_emimpu_clear_protection(struct emimpu_region_t *rg_info)
{
	struct emi_mpu *mpu;
	struct arm_smccc_res smc_res;

	if (!rg_info)
		return -EINVAL;

	mpu = global_emi_mpu;
	if (!mpu)
		return -ENODEV;

	if (rg_info->rg_num > mpu->region_cnt) {
		pr_info("%s: region %u overflow\n", __func__, rg_info->rg_num);
		return -EINVAL;
	}

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_CLEAR,
		rg_info->rg_num, 0, 0, 0, 0, 0, &smc_res);
	if (smc_res.a0) {
		pr_info("%s:%d failed to clear region permission, ret=0x%lx\n",
			__func__, __LINE__, smc_res.a0);
		return -EPERM;
	}
	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_clear_protection);

/*
 * mtk_emimpu_prehandle_register - register callback for irq prehandler
 * @bypass_func:	function point for prehandler
 *
 * Return 0 for success, -EINVAL for fail
 */
int mtk_emimpu_prehandle_register(emimpu_pre_handler bypass_func)
{
	struct emi_mpu *mpu;

	mpu = global_emi_mpu;
	if (!mpu)
		return -EINVAL;

	if (!bypass_func) {
		pr_info("%s: bypass_func is NULL\n", __func__);
		return -EINVAL;
	}

	mpu->pre_handler = bypass_func;

	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_prehandle_register);

/*
 * mtk_emimpu_postclear_register - register callback for clear posthandler
 * @clear_func:	function point for posthandler
 *
 * Return 0 for success, -EINVAL for fail
 */
int mtk_emimpu_postclear_register(emimpu_post_clear clear_func)
{
	struct emi_mpu *mpu;

	mpu = global_emi_mpu;
	if (!mpu)
		return -EINVAL;

	if (!clear_func) {
		pr_info("%s: clear_func is NULL\n", __func__);
		return -EINVAL;
	}

	mpu->post_clear = clear_func;

	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_postclear_register);

/*
 * mtk_emimpu_md_handling_register - register callback for md handling
 * @md_handling_func:	function point for md handling
 *
 * Return 0 for success, -EINVAL for fail
 */
int mtk_emimpu_md_handling_register(emimpu_md_handler md_handling_func)
{
	struct emi_mpu *mpu;

	mpu = global_emi_mpu;
	if (!mpu)
		return -EINVAL;

	if (!md_handling_func) {
		pr_info("%s: md_handling_func is NULL\n", __func__);
		return -EINVAL;
	}

	mpu->md_handler = md_handling_func;

	pr_info("%s: md_handling_func registered!!\n", __func__);

	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_md_handling_register);

/*
 * mtk_clear_md_violation - clear irq for md violation
 *
 * No return
 */
void mtk_clear_md_violation(void)
{
	struct emi_mpu *mpu;
	void __iomem *emi_cen_base;
	unsigned int emi_id;
	struct arm_smccc_res smc_res;

	mpu = global_emi_mpu;
	if (!mpu)
		return;
	if (mpu->version == EMIMPUVER2) {
		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_CLEAR_MD,
			0, 0, 0, 0, 0, 0, &smc_res);
		if (smc_res.a0) {
			pr_info("%s:%d failed to clear md violation, ret=0x%lx\n",
				__func__, __LINE__, smc_res.a0);
			return;
		}
	} else {
		for (emi_id = 0; emi_id < mpu->emi_cen_cnt; emi_id++) {
			emi_cen_base = mpu->emi_cen_base[emi_id];

			set_regs(mpu->clear_md_reg,
				mpu->clear_md_reg_cnt, emi_cen_base);
		}
	}

	pr_info("%s:version %d\n", __func__, mpu->version);
}
EXPORT_SYMBOL(mtk_clear_md_violation);

/*
 * mtk_emimpu_debugdump_register - register callback for debug info dump
 * @debug_func:	function point for debug info dump
 *
 * Return 0 for success, -EINVAL for fail
 */
int mtk_emimpu_debugdump_register(emimpu_debug_dump debug_func)
{
	struct emimpu_dbg_cb *targ_dbg_cb;
	struct emimpu_dbg_cb *curr_dbg_cb;
	struct emi_mpu *mpu;

	mpu = global_emi_mpu;
	if (!mpu)
		return -EINVAL;

	if (!debug_func) {
		pr_info("%s: debug_func is NULL\n", __func__);
		return -EINVAL;
	}

	targ_dbg_cb = kmalloc(sizeof(struct emimpu_dbg_cb), GFP_KERNEL);
	if (!targ_dbg_cb)
		return -ENOMEM;

	targ_dbg_cb->func = debug_func;
	targ_dbg_cb->next_dbg_cb = NULL;

	if (!(mpu->dbg_cb_list)) {
		mpu->dbg_cb_list = targ_dbg_cb;
		return 0;
	}

	for (curr_dbg_cb = mpu->dbg_cb_list; curr_dbg_cb;
		curr_dbg_cb = curr_dbg_cb->next_dbg_cb) {
		if (!(curr_dbg_cb->next_dbg_cb)) {
			curr_dbg_cb->next_dbg_cb = targ_dbg_cb;
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_debugdump_register);

static const struct of_device_id emimpu_of_ids[] = {
	{.compatible = "mediatek,common-emimpu",},
	{}
};
MODULE_DEVICE_TABLE(of, emimpu_of_ids);

static int emimpu_probe(struct platform_device *pdev)
{
	struct device_node *emimpu_node = pdev->dev.of_node;
	struct device_node *emicen_node =
		of_parse_phandle(emimpu_node, "mediatek,emi-reg", 0);
	struct emi_mpu *mpu;
	int ret, size, i;
	struct emimpu_region_t *rg_info;
	struct arm_smccc_res smc_res;
	struct resource *res;
	unsigned int *dump_list;
	unsigned int *ap_apc;
	unsigned int ap_region;
	unsigned int slverr;

	dev_info(&pdev->dev, "driver probed\n");

	if (!emicen_node) {
		dev_err(&pdev->dev, "No emi-reg\n");
		return -ENXIO;
	}

	mpu = devm_kzalloc(&pdev->dev,
		sizeof(struct emi_mpu), GFP_KERNEL);
	if (!mpu)
		return -ENOMEM;

	ret = of_property_read_u32(emimpu_node,
		"region_cnt", &(mpu->region_cnt));
	if (ret) {
		dev_err(&pdev->dev, "No region_cnt\n");
		return -ENXIO;
	}

	ret = of_property_read_u32(emimpu_node,
		"domain_cnt", &(mpu->domain_cnt));
	if (ret) {
		dev_err(&pdev->dev, "No domain_cnt\n");
		return -ENXIO;
	}

	ret = of_property_read_u32(emimpu_node,
		"addr_align", &(mpu->addr_align));
	if (ret) {
		dev_err(&pdev->dev, "No addr_align\n");
		return -ENXIO;
	}

	ret = of_property_read_u64(emimpu_node,
		"dram_start", &(mpu->dram_start));
	if (ret) {
		dev_err(&pdev->dev, "No dram_start\n");
		return -ENXIO;
	}

	ret = of_property_read_u64(emimpu_node,
		"dram_end", &(mpu->dram_end));
	if (ret) {
		dev_err(&pdev->dev, "No dram_end fail\n");
		return -ENXIO;
	}

	ret = of_property_read_u32(emimpu_node,
		"ctrl_intf", &(mpu->ctrl_intf));
	if (ret) {
		dev_err(&pdev->dev, "No ctrl_intf\n");
		return -ENXIO;
	}

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
		dev_err(&pdev->dev, "No dump\n");
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
	}

	mpu->vio_msg = devm_kmalloc(&pdev->dev,
		MTK_EMI_MAX_CMD_LEN, GFP_KERNEL);
	if (!(mpu->vio_msg))
		return -ENOMEM;

	global_emi_mpu = mpu;
	platform_set_drvdata(pdev, mpu);

	ret = of_property_read_u32(emimpu_node, "ap_region", &ap_region);
	if (ret) {
		mpu->ap_rg_info = NULL;
		dev_info(&pdev->dev, "No ap_region\n");
	} else {
		/* initialize the AP region */

		size = sizeof(unsigned int) * mpu->domain_cnt;
		ap_apc = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
		if (!ap_apc)
			return -ENOMEM;
		ret = of_property_read_u32_array(emimpu_node, "ap_apc",
			(unsigned int *)ap_apc, mpu->domain_cnt);
		if (ret) {
			dev_err(&pdev->dev, "No ap_apc\n");
			return -ENXIO;
		}

		mpu->ap_rg_info = kmalloc(sizeof(struct emimpu_region_t),
					GFP_KERNEL);
		if (!(mpu->ap_rg_info))
			return -ENOMEM;

		rg_info = mpu->ap_rg_info;

		mtk_emimpu_init_region(rg_info, ap_region);

		mtk_emimpu_set_addr(rg_info,
			mpu->dram_start, mpu->dram_end);

		for (i = 0; i < mpu->domain_cnt; i++)
			if (ap_apc[i] != MTK_EMIMPU_FORBIDDEN)
				mtk_emimpu_set_apc(rg_info, i, ap_apc[i]);

		mtk_emimpu_lock_region(rg_info, MTK_EMIMPU_LOCK);
	}

	mpu->irq = irq_of_parse_and_map(emimpu_node, 0);
	if (mpu->irq == 0) {
		dev_err(&pdev->dev, "Failed to get irq resource\n");
		ret = -ENXIO;
		goto free_ap_rg_info;
	}
	ret = request_irq(mpu->irq, (irq_handler_t)emimpu_violation_irq,
		IRQF_TRIGGER_NONE, "emimpu", mpu);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq");
		ret = -EINVAL;
		goto free_ap_rg_info;
	}

	ret = of_property_read_u32(emimpu_node, "slverr", &slverr);
	if (!ret && slverr)
		for (i = 0; i < mpu->domain_cnt; i++) {
			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_SLVERR,
				i, 0, 0, 0, 0, 0, &smc_res);
			if (smc_res.a0) {
				dev_err(&pdev->dev, "Failed to set MPU domain%d Slave Error, ret=0x%lx\n",
					i, smc_res.a0);
				ret = -EINVAL;
				goto free_ap_rg_info;
			}
		}

	mpu->version = EMIMPUVER1;

	devm_kfree(&pdev->dev, ap_apc);
	devm_kfree(&pdev->dev, dump_list);

	dev_info(&pdev->dev, "%s(%d),%s(%d),%s(%d),%s(%llx),%s(%llx),%s(%d)\n",
		"region_cnt", mpu->region_cnt,
		"domain_cnt", mpu->domain_cnt,
		"addr_align", mpu->addr_align,
		"dram_start", mpu->dram_start,
		"dram_end", mpu->dram_end,
		"ctrl_intf", mpu->ctrl_intf);

	return 0;

free_ap_rg_info:
	kfree(mpu->ap_rg_info);

	return ret;
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
		.name = "emimpu_driver",
		.owner = THIS_MODULE,
		.of_match_table = emimpu_of_ids,
	},
};

static __init int emimpu_init(void)
{
	int ret;

	pr_info("emimpu was loaded\n");

	ret = platform_driver_register(&emimpu_driver);
	if (ret) {
		pr_err("emimpu: failed to register driver\n");
		return ret;
	}

	return 0;
}

module_init(emimpu_init);

MODULE_DESCRIPTION("MediaTek EMI MPU Driver");
MODULE_LICENSE("GPL v2");
