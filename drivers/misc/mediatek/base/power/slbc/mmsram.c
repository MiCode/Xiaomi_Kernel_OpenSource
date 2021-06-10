/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <mt-plat/aee.h>
#include <mt-plat/mtk_secure_api.h>
#include "mmsram.h"

#define MMSYSRAM_INTEN0		(0x000)
#define MMSYSRAM_INTEN1		(0x004)
#define MMSYSRAM_INSTA0		(0x010)
#define MMSYSRAM_INSTA1		(0x014)
#define MMSYSRAM_SEC_ADDR0	(0x040)
#define MMSYSRAM_SEC_CTRL0	(0x060)

#define BIT_INSTA0		(0x6FF0E0E0)
#define BIT_INSTA1		(0x000037F8)
#define BIT_SECURE_ON		(0x11111111)

/* First Violation Latch Debug */
#define FVLD_APC0_LATCH_EN	(0x090)
#define FVLD_APC1_LATCH_EN	(0x0C0)
#define FVLD_MPU0_LATCH_EN	(0x110)
#define FVLD_MPU1_LATCH_EN	(0x140)
#define FVLD_APC0_VIO_ADDR	(0x0A4)
#define FVLD_APC0_VIO_ID	(0x0A8)
#define FVLD_APC0_VIO_WR_RD	(0x0AC)
#define FVLD_APC1_VIO_ADDR	(0x0D4)
#define FVLD_APC1_VIO_ID	(0x0D8)
#define FVLD_APC1_VIO_WR_RD	(0x0DC)
#define FVLD_MPU0_VIO_ADDR	(0x124)
#define FVLD_MPU0_VIO_ID	(0x128)
#define FVLD_MPU0_VIO_WR_RD	(0x12C)
#define FVLD_MPU1_VIO_ADDR	(0x154)
#define FVLD_MPU1_VIO_ID	(0x158)
#define FVLD_MPU1_VIO_WR_RD	(0x15C)

#define BIT_APC_LATCH_EN	(0x00000020)
#define BIT_MPU_LATCH_EN	(0x00000FF0)

#define DEC_APC0_VIO_DECERR(x)	(((x) >> 5) & 0x1)
#define DEC_APC0_VIO_WR_RD(x)	(((x) >> 6) & 0x3)
#define DEC_APC1_VIO_DECERR(x)	(((x) >> 13) & 0x1)
#define DEC_APC1_VIO_WR_RD(x)	(((x) >> 14) & 0x3)
#define DEC_MPU0_VIO_DEVAPC(x)	(((x) >> 20) & 0xFF)
#define DEC_MPU0_VIO_WR_RD(x)	(((x) >> 29) & 0x3)
#define DEC_MPU1_VIO_DEVAPC(x)	(((x) >> 3) & 0xFF)
#define DEC_MPU1_VIO_WR_RD(x)	(((x) >> 12) & 0x3)

#define DEC_APC_LATCH_EN(x)	(((x) >> 5) & 0x1)
#define DEC_APC_VIO_ADDR(x)	(x)
#define DEC_APC_VIO_ID(x)	((x) & 0xFFFFF)
#define DEC_APC_VIO_WR_RD(x)	(((x) >> 17) & 0x3)
#define DEC_MPU_LATCH_EN(x)	(((x) >> 4) & 0xFF)
#define DEC_MPU_VIO_ADDR(x)	((x) & 0x1FFFFF)
#define DEC_MPU_VIO_ID(x)	((x) & 0xFFFFF)
#define DEC_MPU_VIO_WR_RD(x)	(((x) >> 17) & 0x3)

#define MAX_CLK_NUM	(8)

struct mmsram_dev {
	void __iomem *ctrl_base;
	void __iomem *sram_paddr;
	void __iomem *sram_vaddr;
	ssize_t sram_size;
	struct clk *clk[MAX_CLK_NUM];
	const char *clk_name[MAX_CLK_NUM];
};

enum smc_mmsram_request {
	MMSRAM_ENABLE_SECURE,
};

static struct work_struct dump_reg_work;

static struct mmsram_dev *mmsram;
static atomic_t clk_ref = ATOMIC_INIT(0);
static bool is_secure_on;
static bool debug_enable;

/* MTCMOS clocks should be defined before CG clocks in DTS */
static int set_clk_enable(bool is_enable)
{
	int ret = 0;

#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	int i, j;

	if (is_enable) {
		for (i = 0; i < MAX_CLK_NUM; i++) {
			if (mmsram->clk[i])
				ret = clk_prepare_enable(mmsram->clk[i]);

			if (ret) {
				pr_notice("mmsram clk(%s) enable fail:%d\n",
					mmsram->clk_name[i], ret);
				for (j = i - 1; j >= 0; j--)
					clk_disable_unprepare(mmsram->clk[j]);
				return ret;
			}
		}

		atomic_inc(&clk_ref);
	} else {
		for (i = MAX_CLK_NUM - 1; i >= 0; i--)
			if (mmsram->clk[i])
				clk_disable_unprepare(mmsram->clk[i]);
		atomic_dec(&clk_ref);
	}
#endif
	if (debug_enable)
		pr_notice("%s:%d\n", __func__, is_enable);
	return ret;
}

static void set_reg_secure(bool is_on)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_MMSRAM_CONTROL, MMSRAM_ENABLE_SECURE,
			is_on ? 1 : 0, 0, 0, 0, 0, 0, &res);
}

static s32 before_reg_rw(void)
{
	if (set_clk_enable(true)) {
		pr_notice("%s: enable clk fail\n", __func__);
		return -EINVAL;
	}
	set_reg_secure(false);
	return 0;
}

static void after_reg_rw(void)
{
	set_reg_secure(true);
	set_clk_enable(false);
}


void mmsram_set_secure(bool secure_on)
{
	if (secure_on == is_secure_on)
		return;
	is_secure_on = secure_on;
	if (before_reg_rw()) {
		pr_notice("%s: error before reg rw\n", __func__);
		return;
	}
	if (secure_on)
		writel(BIT_SECURE_ON, mmsram->ctrl_base + MMSYSRAM_SEC_CTRL0);
	else
		writel(0, mmsram->ctrl_base + MMSYSRAM_SEC_CTRL0);
	after_reg_rw();
}

static void init_mmsram_reg(void)
{
	if (before_reg_rw()) {
		pr_notice("%s: error before reg rw\n", __func__);
		return;
	}
	writel((0x1 << 24) | 0x160000,
		mmsram->ctrl_base + MMSYSRAM_SEC_ADDR0);

	writel(0x0, mmsram->ctrl_base + MMSYSRAM_INSTA0);
	writel(0x0, mmsram->ctrl_base + MMSYSRAM_INSTA1);

	writel(BIT_APC_LATCH_EN, mmsram->ctrl_base + FVLD_APC0_LATCH_EN);
	writel(BIT_APC_LATCH_EN, mmsram->ctrl_base + FVLD_APC1_LATCH_EN);
	writel(BIT_MPU_LATCH_EN, mmsram->ctrl_base + FVLD_MPU0_LATCH_EN);
	writel(BIT_MPU_LATCH_EN, mmsram->ctrl_base + FVLD_MPU1_LATCH_EN);
	writel(BIT_INSTA0, mmsram->ctrl_base + MMSYSRAM_INTEN0);
	writel(BIT_INSTA1, mmsram->ctrl_base + MMSYSRAM_INTEN1);

	after_reg_rw();
}

int mmsram_power_on(void)
{
	int ret = 0;

	set_clk_enable(true);
	init_mmsram_reg();
	if (debug_enable)
		pr_notice("mmsram power on\n");
	return ret;
}
EXPORT_SYMBOL_GPL(mmsram_power_on);

void mmsram_power_off(void)
{
	set_clk_enable(false);
	if (debug_enable)
		pr_notice("mmsram power off\n");
}
EXPORT_SYMBOL_GPL(mmsram_power_off);

int enable_mmsram(void)
{
	pr_notice("enable mmsram\n");

	return 0;
}
EXPORT_SYMBOL_GPL(enable_mmsram);

void disable_mmsram(void)
{
	pr_notice("disable mmsram\n");
}
EXPORT_SYMBOL_GPL(disable_mmsram);

void mmsram_get_info(struct mmsram_data *data)
{
	data->paddr = mmsram->sram_paddr;
	data->vaddr = mmsram->sram_vaddr;
	data->size = mmsram->sram_size;
	pr_notice("%s: pa:%#x va:%#x size:%#lx\n",
		__func__, data->paddr, data->vaddr,
		data->size);
}
EXPORT_SYMBOL_GPL(mmsram_get_info);

static void dump_reg_func(struct work_struct *work)
{
	u32 interrupt_monitor0, interrupt_monitor1;
	void __iomem *ctrl_base = mmsram->ctrl_base;

	if (before_reg_rw()) {
		pr_notice("%s: error before reg rw\n", __func__);
		return;
	}

	/* Print debug log */
	interrupt_monitor0 = readl(ctrl_base + MMSYSRAM_INSTA0);
	interrupt_monitor1 = readl(ctrl_base + MMSYSRAM_INSTA1);
	pr_notice("apc0_vio_decerr:%#x apc0_vio_wr_rd:%#x\n",
		DEC_APC0_VIO_DECERR(interrupt_monitor0),
		DEC_APC0_VIO_WR_RD(interrupt_monitor0));
	pr_notice("apc1_vio_decerr:%#x apc1_vio_wr_rd:%#x\n",
		DEC_APC1_VIO_DECERR(interrupt_monitor0),
		DEC_APC1_VIO_WR_RD(interrupt_monitor0));
	pr_notice("mpu0_vio_devapc:%#x mpu0_vio_wr_rd:%#x\n",
		DEC_MPU0_VIO_DEVAPC(interrupt_monitor0),
		DEC_MPU0_VIO_WR_RD(interrupt_monitor0));
	pr_notice("mpu1_vio_devapc:%#x mpu1_vio_wr_rd:%#x\n",
		DEC_MPU1_VIO_DEVAPC(interrupt_monitor1),
		DEC_MPU1_VIO_WR_RD(interrupt_monitor1));
	pr_notice("apc0_latch_en:%#x apc0_vio_addr:%#x\n",
		DEC_APC_LATCH_EN(readl(ctrl_base + FVLD_APC0_LATCH_EN)),
		DEC_APC_VIO_ADDR(readl(ctrl_base + FVLD_APC0_VIO_ADDR)));
	pr_notice("apc0_vio_id:%#x apc0_vio_wr_rd:%#x\n",
		DEC_APC_VIO_ID(readl(ctrl_base + FVLD_APC0_VIO_ID)),
		DEC_APC_VIO_WR_RD(readl(ctrl_base + FVLD_APC0_VIO_WR_RD)));

	pr_notice("apc1_latch_en:%#x apc1_vio_addr:%#x\n",
		DEC_APC_LATCH_EN(readl(ctrl_base + FVLD_APC1_LATCH_EN)),
		DEC_APC_VIO_ADDR(readl(ctrl_base + FVLD_APC1_VIO_ADDR)));
	pr_notice("apc1_vio_id:%#x apc1_vio_wr_rd:%#x\n",
		DEC_APC_VIO_ID(readl(ctrl_base + FVLD_APC1_VIO_ID)),
		DEC_APC_VIO_WR_RD(readl(ctrl_base + FVLD_APC1_VIO_WR_RD)));

	pr_notice("mpu0_latch_en:%#x mpu0_vio_addr:%#x\n",
		DEC_MPU_LATCH_EN(readl(ctrl_base + FVLD_MPU0_LATCH_EN)),
		DEC_MPU_VIO_ADDR(readl(ctrl_base + FVLD_MPU0_VIO_ADDR)));

	pr_notice("mpu0_vio_id:%#x mpu0_vio_wr_rd:%#x\n",
		DEC_MPU_VIO_ID(readl(ctrl_base + FVLD_MPU0_VIO_ID)),
		DEC_MPU_VIO_WR_RD(readl(ctrl_base + FVLD_MPU0_VIO_WR_RD)));
	pr_notice("mpu1_latch_en:%#x mpu1_vio_addr:%#x\n",
		DEC_MPU_LATCH_EN(readl(ctrl_base + FVLD_MPU1_LATCH_EN)),
		DEC_MPU_VIO_ADDR(readl(ctrl_base + FVLD_MPU1_VIO_ADDR)));
	pr_notice("mpu1_vio_id:%#x mpu1_vio_wr_rd:%#x\n",
		DEC_MPU_VIO_ID(readl(ctrl_base + FVLD_MPU1_VIO_ID)),
		DEC_MPU_VIO_WR_RD(readl(ctrl_base + FVLD_MPU1_VIO_WR_RD)));

	writel(0x0, ctrl_base + MMSYSRAM_INSTA0);
	writel(0x0, ctrl_base + MMSYSRAM_INSTA1);

	after_reg_rw();

	aee_kernel_warning("MMSRAM", "MMSRAM Violation.");
}

static irqreturn_t mmsram_irq_handler(int irq, void *data)
{
	pr_notice("handle mmsram irq!\n");
	schedule_work(&dump_reg_work);
	return IRQ_HANDLED;
}

static int mmsram_probe(struct platform_device *pdev)
{
	struct resource *res;
	int irq, err, clk_num, i;

	mmsram = devm_kzalloc(&pdev->dev, sizeof(*mmsram), GFP_KERNEL);
	if (!mmsram)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_notice(&pdev->dev, "could not get resource for ctrl\n");
		return -EINVAL;
	}

	mmsram->ctrl_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mmsram->ctrl_base)) {
		dev_notice(&pdev->dev,
			"could not ioremap resource for ctrl\n");
		return PTR_ERR(mmsram->ctrl_base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_notice(&pdev->dev, "could not get resource for memory\n");
		return -EINVAL;
	}
	mmsram->sram_paddr = (void *)res->start;
	mmsram->sram_size = resource_size(res);
	mmsram->sram_vaddr =  (void __iomem *) devm_memremap(&pdev->dev,
				res->start, mmsram->sram_size, MEMREMAP_WT);
	if (IS_ERR(mmsram->sram_vaddr)) {
		dev_notice(&pdev->dev,
			"could not ioremap resource for memory\n");
		return PTR_ERR(mmsram->sram_vaddr);
	}

	dev_notice(&pdev->dev, "probe va=%p pa=%p size=%#lx\n",
		mmsram->sram_vaddr, mmsram->sram_paddr,
		mmsram->sram_size);

	if (!IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)) {
		clk_num = of_property_read_string_array(pdev->dev.of_node,
			"clock-names", &mmsram->clk_name[0], MAX_CLK_NUM);
		for (i = 0; i < clk_num; i++) {
			mmsram->clk[i] = devm_clk_get(&pdev->dev,
						mmsram->clk_name[i]);
			if (IS_ERR(mmsram->clk[i])) {
				dev_notice(&pdev->dev,
					"could not get mmsram clk(%s) info\n",
					mmsram->clk_name[i]);
				return PTR_ERR(mmsram->clk[i]);
			}
		}
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_notice(&pdev->dev, "failed to get irq (%d)\n", irq);
		return -EINVAL;
	}
	err = devm_request_irq(&pdev->dev, irq, mmsram_irq_handler, IRQF_SHARED,
			       "mtk_mmsram", mmsram);
	if (err) {
		dev_notice(&pdev->dev,
			"failed to register ISR %d (%d)", irq, err);
		return err;
	}

	INIT_WORK(&dump_reg_work, dump_reg_func);

	return 0;
}

#define RESULT_STR_LEN 8
int test_mmsram;
struct mmsram_data *data;
int set_test_mmsram(const char *val, const struct kernel_param *kp)
{
	int result;
	u32 test_case, offset, value;
	const char *test_str = "12345678";
	char result_str[RESULT_STR_LEN + 1] = {0};

	result = sscanf(val, "%d %i %i", &test_case, &offset, &value);
	if (result != 3) {
		pr_notice("invalid input: %s, result(%d)\n", val, result);
		return -EINVAL;
	}
	pr_notice("%s (test_case, offset, value): (%d,%#x,%#x)\n",
		__func__, test_case, offset, value);

	switch (test_case) {
	case 0: /* Initialize */
		data = kzalloc(sizeof(*data), GFP_KERNEL);
		mmsram_get_info(data);
		enable_mmsram();
		mmsram_power_on();
		break;
	case 1: /* Uninitialize */
		mmsram_power_off();
		disable_mmsram();
		kfree(data);
		break;
	case 2: /* Write value to offset */
		writel(value, data->vaddr + offset);
		value = readl(data->vaddr + offset);
		pr_notice("write %#x success\n", value);
		break;
	case 3: /* Read value from offset */
		value = readl(data->vaddr + offset);
		pr_notice("read %#x success\n", value);
		break;
	case 4: /* Write test string to offset */
		memcpy_toio(data->vaddr + offset, test_str, RESULT_STR_LEN);
		pr_notice("write str:%s success\n", test_str);
		break;
	case 5: /* Write test string from offset */
		memcpy_fromio(result_str, data->vaddr, RESULT_STR_LEN);
		result_str[RESULT_STR_LEN] = '\0';
		pr_notice("read str:%s success\n", result_str);
		break;
	default:
		pr_notice("wrong input test_case:%d\n", test_case);
	}

	return 0;
}
static struct kernel_param_ops test_mmsram_ops = {
	.set = set_test_mmsram,
	.get = param_get_int,
};
module_param_cb(test_mmsram, &test_mmsram_ops, &test_mmsram, 0644);
MODULE_PARM_DESC(test_mmsram, "test mmsram");

static struct kernel_param_ops debug_enable_ops = {
	.set = param_set_bool,
	.get = param_get_bool,
};
module_param_cb(
	debug_enable, &debug_enable_ops, &debug_enable, 0644);
MODULE_PARM_DESC(debug_enable, "enable or disable mmsram debug log");

static const struct of_device_id of_mmsram_match_tbl[] = {
	{
		.compatible = "mediatek,mmsram",
	},
	{}
};

static struct platform_driver mmsram_drv = {
	.probe = mmsram_probe,
	.driver = {
		.name = "mtk-mmsram",
		.owner = THIS_MODULE,
		.of_match_table = of_mmsram_match_tbl,
	},
};
static int __init mtk_mmsram_init(void)
{
	s32 status;

	status = platform_driver_register(&mmsram_drv);
	if (status) {
		pr_notice("Failed to register mmsram driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}
static void __exit mtk_mmsram_exit(void)
{
	platform_driver_unregister(&mmsram_drv);
}
module_init(mtk_mmsram_init);
module_exit(mtk_mmsram_exit);
MODULE_DESCRIPTION("MTK MMSRAM driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");
