/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <linux/uaccess.h>
#include <asm/div64.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>
#include <mt-plat/mtk_devinfo.h>

#include "nandx_util.h"
#include "nandx_errno.h"
#include "nandx_info.h"
#include "nandx_bmt.h"
#include "nandx_pmt.h"
#include "nandx_core.h"
#include "nandx_platform.h"
#include "nandx_ops.h"
#include "mtd_ops.h"
#include "wrapper_pmt.h"
#include "mntl_ops.h"

struct nfc_compatible {
	enum IC_VER ic_ver;
	char *ecc_compatible;
	char *top_compatible;
};

static const struct nfc_compatible nfc_compats_mt8127 = {
	.ic_ver = NANDX_MT8127,
	.ecc_compatible = "mediatek,mt8127-nfiecc",
	.top_compatible = NULL,
};

static const struct nfc_compatible nfc_compats_mt8163 = {
	.ic_ver = NANDX_MT8163,
	.ecc_compatible = "mediatek,mt8163-nfiecc",
	.top_compatible = NULL,
};

static const struct nfc_compatible nfc_compats_mt8167 = {
	.ic_ver = NANDX_MT8167,
	.ecc_compatible = "mediatek,mt8167-nfiecc",
	.top_compatible = "mediatek,mt8167-pctl-a-syscfg",
};

static const struct of_device_id ic_of_match[] = {
	{.compatible = "mediatek,mt8127-nfi", .data = &nfc_compats_mt8127},
	{.compatible = "mediatek,mt8163-nfi", .data = &nfc_compats_mt8163},
	{.compatible = "mediatek,mt8167-nfi", .data = &nfc_compats_mt8167},
	{}
};

static void release_platform_data(struct platform_data *pdata)
{
	mem_free(pdata->res);
	mem_free(pdata);
}

static struct platform_data *config_platform_data(struct platform_device
						  *pdev)
{
	u32 nfi_irq, ecc_irq;
	void __iomem *nfi_base, *nfiecc_base, *top_base = NULL;
	struct device *dev;
	struct device_node *nfiecc_node, *top_node;
	const struct of_device_id *of_id;
	struct nfc_compatible *compat;
	struct platform_data *pdata;
	struct nfc_resource *res;

	pdata = mem_alloc(1, sizeof(struct platform_data));
	if (!pdata)
		return NULL;

	res = mem_alloc(1, sizeof(struct nfc_resource));
	if (!res)
		goto freepdata;

	pdata->res = res;

	dev = &pdev->dev;
	nfi_base = of_iomap(dev->of_node, 0);
	nfi_irq = irq_of_parse_and_map(dev->of_node, 0);

	of_id = of_match_node(ic_of_match, pdev->dev.of_node);
	if (!of_id)
		goto freeres;

	compat = (struct nfc_compatible *)of_id->data;
	nfiecc_node = of_find_compatible_node(NULL, NULL,
					      compat->ecc_compatible);
	nfiecc_base = of_iomap(nfiecc_node, 0);
	ecc_irq = irq_of_parse_and_map(nfiecc_node, 0);

	if (compat->top_compatible) {
		top_node = of_find_compatible_node(NULL, NULL,
						   compat->top_compatible);
		top_base = of_iomap(top_node, 0);
	}

	res->ver = (enum IC_VER)(compat->ic_ver);
	res->nfi_regs = (void *)nfi_base;
	res->ecc_regs = (void *)nfiecc_base;
	if (top_base)
		res->top_regs = (void *)top_base;
	res->nfi_irq = nfi_irq;
	res->ecc_irq = ecc_irq;
	res->dev = pdev;

	pdata->freq.freq_async = 133000000;
	pdata->freq.sel_2x_idx = -1;
	pdata->freq.sel_ecc_idx = -1;

	return pdata;

freeres:
	mem_free(res);
freepdata:
	mem_free(pdata);

	return NULL;
}

static struct mtd_info *mtd_info_create(struct platform_device *pdev,
					struct nandx_core *ncore)
{
	struct mtd_info *mtd;

	mtd = mem_alloc(1, sizeof(struct mtd_info));
	if (!mtd)
		return NULL;

	/* Fill in remaining MTD driver data */
	mtd->priv = ncore;
	mtd->owner = THIS_MODULE;
	mtd->dev.parent = &pdev->dev;
	mtd->name = "MTK-Nand";
	mtd->writesize = ncore->info->page_size;
	mtd->erasesize = ncore->info->block_size;
	mtd->oobsize = ncore->info->oob_size;
	mtd->size = (u64)ncore->info->block_size * ncore->info->block_num;
	mtd->type = MTD_NANDFLASH;
	mtd->flags = MTD_CAP_NANDFLASH;
	mtd->_erase = nand_erase;
	mtd->_point = NULL;
	mtd->_unpoint = NULL;
	mtd->_read = nand_read;
	mtd->_write = nand_write;
	mtd->_panic_write = nand_write;
	mtd->_read_oob = nand_read_oob;
	mtd->_write_oob = nand_write_oob;
	mtd->_sync = nand_sync;
	mtd->_lock = NULL;
	mtd->_unlock = NULL;
	mtd->_block_isbad = nand_is_bad;
	mtd->_block_markbad = nand_mark_bad;
	mtd->writebufsize = mtd->writesize;

	/* propagate ecc info to mtd_info */
	/* TODO: set NULL TO  */
	mtd->ecc_strength = ncore->info->ecc_strength;
	mtd->ecc_step_size = ncore->info->sector_size;
	/*
	 * Initialize bitflip_threshold to its default prior scan_bbt() call.
	 * scan_bbt() might invoke mtd_read(), thus bitflip_threshold must be
	 * properly set.
	 */
	if (!mtd->bitflip_threshold)
		mtd->bitflip_threshold = mtd->ecc_strength;

	return mtd;
}

static void mtd_info_release(struct mtd_info *mtd)
{
	mem_free(mtd);
}

bool randomizer_is_support(enum IC_VER ver)
{
	u32 idx = 0, enable = 0;

	if (ver == NANDX_MT8167) {
		idx = 0;
		enable = 0x00001000;
	} else if (ver == NANDX_MT8127 || ver == NANDX_MT8163) {
		idx = 26;
		enable = 0x00000004;
	}

	return (get_devinfo_with_index(idx) & enable) ? true : false;
}

static int nand_probe(struct platform_device *pdev)
{
	int ret;
	u32 mode, bmt_block_num, pmt_block_start;
	struct mtd_info *mtd;
	struct platform_data *pdata;
	struct nandx_core *ncore;
	struct nandx_chip_info *info;

	/* get info from dts & config pdata */
	pdata = config_platform_data(pdev);
	if (!pdata)
		return -ENOMEM;

	ret = nandx_platform_init(pdata);
	if (ret < 0)
		goto release_pdata;

	nandx_lock_init();

	mode = MODE_DMA | MODE_ECC | MODE_IRQ | MODE_DDR | MODE_CALIBRATION;
	if (randomizer_is_support(pdata->res->ver))
		mode |= MODE_RANDOMIZE;

	ncore = nandx_core_init(pdata, mode);
	if (ret < 0)
		goto initfail;

	info = ncore->info;
	dump_nand_info(info);
	bmt_block_num = nandx_calculate_bmt_num(info);
	ret = nandx_bmt_init(info, bmt_block_num, true);
	if (ret < 0) {
		pr_err("Error: init bmt failed\n");
		NANDX_ASSERT(0);
		goto bmt_err;
	}

	pmt_block_start = info->block_num - bmt_block_num - PMT_POOL_SIZE;
	ret = nandx_pmt_init(info, pmt_block_start);
	if (ret < 0) {
		pr_err("Error: init pmt failed\n");
		NANDX_ASSERT(0);
		goto pmt_err;
	}

	mtd = mtd_info_create(pdev, ncore);
	if (!mtd) {
		ret = -ENOMEM;
		goto mtd_err;
	}

	nandx_pmt_register(mtd);

	platform_set_drvdata(pdev, mtd);

	/* mntl related init */
	ret = nandx_mntl_data_info_alloc();
	if (ret < 0)
		goto data_init_err;

	ret = nandx_mntl_ops_init();
	if (ret < 0)
		goto ops_init_err;

	ret = init_mntl_module();
	if (ret < 0)
		goto ops_init_err;

	return 0;

ops_init_err:
	nandx_mntl_data_info_free();
data_init_err:
	nandx_pmt_unregister();
	mtd_info_release(mtd);
mtd_err:
	nandx_pmt_exit();
pmt_err:
	nandx_bmt_exit();
bmt_err:
	nandx_core_free();
initfail:
	nandx_platform_power_down(pdata);
release_pdata:
	release_platform_data(pdata);

	pr_debug("%s: probe err %d\n", __func__, ret);
	return ret;
}

static int nand_remove(struct platform_device *pdev)
{
	struct mtd_info *mtd;
	struct platform_data *pdata;
	struct nandx_core *ncore;
	bool high_speed_en, ecc_clk_en;

	mtd = platform_get_drvdata(pdev);
	ncore = (struct nandx_core *)mtd->priv;
	pdata = ncore->pdata;

	mtd_device_unregister(mtd);
	high_speed_en = pdata->freq.sel_2x_idx >= 0;
	ecc_clk_en = pdata->freq.sel_ecc_idx >= 0;
	nandx_platform_unprepare_clock(ncore->pdata, high_speed_en,
				       ecc_clk_en);
	nandx_platform_power_down(pdata);
	release_platform_data(pdata);
	mtd_info_release(mtd);

	return 0;
}

static int nand_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	struct mtd_info *mtd;
	struct platform_data *pdata;
	struct nandx_core *ncore;
	bool high_speed_en, ecc_clk_en;

	pr_info("%s\n", __func__);
	nandx_get_device(FL_PM_SUSPENDED);

	mtd = platform_get_drvdata(pdev);
	ncore = (struct nandx_core *)mtd->priv;
	pdata = ncore->pdata;

	high_speed_en = pdata->freq.sel_2x_idx >= 0;
	ecc_clk_en = pdata->freq.sel_ecc_idx >= 0;
	nandx_platform_enable_clock(ncore->pdata, high_speed_en, ecc_clk_en);
	ret = nandx_core_suspend();

	nandx_platform_disable_clock(ncore->pdata, high_speed_en, ecc_clk_en);
	nandx_platform_unprepare_clock(ncore->pdata, high_speed_en,
				       ecc_clk_en);

	nandx_platform_power_down(pdata);

	return ret;
}

static int nand_resume(struct platform_device *pdev)
{
	int ret;
	struct mtd_info *mtd;
	struct platform_data *pdata;
	struct nandx_core *ncore;
	struct nandx_lock *nlock = get_nandx_lock();
	bool high_speed_en, ecc_clk_en;

	pr_info("%s\n", __func__);
	mtd = platform_get_drvdata(pdev);
	ncore = (struct nandx_core *)mtd->priv;
	pdata = ncore->pdata;

	nandx_platform_power_on(pdata);

	nandx_platform_prepare_clock(ncore->pdata, false, false);
	nandx_platform_enable_clock(ncore->pdata, false, false);
	ret = nandx_core_resume();
	nandx_platform_disable_clock(ncore->pdata, false, false);
	nandx_platform_unprepare_clock(ncore->pdata, false, false);

	high_speed_en = pdata->freq.sel_2x_idx >= 0;
	ecc_clk_en = pdata->freq.sel_ecc_idx >= 0;
	nandx_platform_prepare_clock(ncore->pdata, high_speed_en, ecc_clk_en);

	if (nlock->state == FL_PM_SUSPENDED)
		nandx_release_device();

	return ret;
}

static struct platform_driver nand_driver = {
	.probe = nand_probe,
	.remove = nand_remove,
	.suspend = nand_suspend,
	.resume = nand_resume,
	.driver = {
		   .name = "mtk-nand",
		   .owner = THIS_MODULE,
		   .of_match_table = ic_of_match,
		   },
};

#define PROCNAND	"driver/nand"

static int nand_proc_show(struct seq_file *m, void *v)
{
	struct nandx_core *ncore = get_nandx_core();
	struct nandx_chip_info *info = ncore->info;
	u64 size = (u64)info->block_size * info->block_num;

	seq_puts(m, "Size");
	seq_printf(m, "0x%llx\n", size);
	seq_printf(m, "0x%x\n", info->page_size);
	seq_printf(m, "0x%x\n", info->oob_size);
	seq_puts(m, "\n");

	return 0;
}

static int nand_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, nand_proc_show, inode->i_private);
}

static const struct file_operations nand_fops = {
	.open = nand_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init nand_init(void)
{
	proc_create(PROCNAND, 0664, NULL, &nand_fops);

	return platform_driver_register(&nand_driver);
}

static void __exit nand_exit(void)
{
	platform_driver_unregister(&nand_driver);
}
module_init(nand_init);
module_exit(nand_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK Nand Flash Controller Driver");
MODULE_AUTHOR("MediaTek");
