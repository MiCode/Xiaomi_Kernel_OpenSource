// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc NAND driver
 *
 * Copyright (C) 2023 Unisoc, Inc.
 * Author: Zhenxiong Lai <zhenxiong.lai@unisoc.com>
 */
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/flashchip.h>
#include <linux/of_device.h>
#ifdef CONFIG_MTD_NAND_SPRD_DEVICE_INFO_FROM_H
#define SPRD_NAND_DEVICE_TBL
#endif
#include "sprd_nfc.h"

static const char * const part_probes[] = {"cmdlinepart", NULL};

static const struct nandc_variant_ops nandc_r6p0_vops = {
	.version = SPRD_NANDC_VERSIONS_R6P0
};

static const struct nandc_variant_ops nandc_r8p0_vops = {
	.version = SPRD_NANDC_VERSIONS_R8P0
};

static struct sprd_nand_param g_nfc_vendor;

static int sprd_nand_get_device(struct mtd_info *mtd)
{
	struct sprd_nfc *host = mtd_to_sprd_nfc(mtd);
	int ret = 0;

	if (host->suspended)
		return -EBUSY;

	ret = host->ops->ctrl_en(host, true);
	if (ret) {
		dev_err(host->dev, "%s ctrl enable failed,%d\n", __func__, ret);
		return -EBUSY;
	}

	return 0;
}

static void sprd_nand_put_device(struct mtd_info *mtd)
{
	struct sprd_nfc *host = mtd_to_sprd_nfc(mtd);

	host->ops->ctrl_en(host, false);
}

static int sprd_nand_mtd_read(struct mtd_info *mtd, loff_t from,
	struct mtd_oob_ops *ops)
{
	struct sprd_nfc *host = mtd_to_sprd_nfc(mtd);
	struct nand_device *nand = mtd_to_nanddev(mtd);
	u32 remain_page_inbuf = 1, ret_num;
	struct mtd_ecc_stats ecc_stats;
	unsigned int max_bitflips = 0;
	struct nand_page_io_req *req;
	struct nand_io_iter iter;
	bool ecc_failed = false;
	bool random = false;
	u32 page;
	struct mtd_part *spl = NULL;
	int ret = 0;

	mutex_lock(&host->lock);
	ret = sprd_nand_get_device(mtd);
	if (ret)
		goto err;

	spl = container_of(mtd->partitions.next, struct mtd_part, node);

	/* Fixme, there is no use of the repeat function to improve performance here */
	nanddev_io_for_each_page(nand, NAND_PAGE_READ, from, ops, &iter) {
		memset(&ecc_stats, 0, sizeof(ecc_stats));
		req = &iter.req;

		page = (req->pos.eraseblock << (host->blkshift - host->pageshift))
				+ req->pos.page;
		if ((page < spl->size / host->param.page_size)
			&& (ops->mode != MTD_OPS_RAW))
			random = true;

		ret = host->ops->read_page(host, page,
				remain_page_inbuf/* repeat */,
				&ret_num, (u32)(!!req->databuf.in), (u32)(!!req->oobbuf.in),
				ops->mode, &ecc_stats,
				random,
				0);

		if (ret < 0 && ret != -EBADMSG)
			break;

		if (req->datalen && req->databuf.in)
			memcpy(req->databuf.in, host->mbuf_v + req->dataoffs, req->datalen);

		if (req->ooblen && req->oobbuf.in) {
			if (req->mode == MTD_OPS_AUTO_OOB)
				mtd_ooblayout_get_databytes(mtd, req->oobbuf.in,
					host->sbuf_v, req->ooboffs, req->ooblen);
			else
				memcpy(req->oobbuf.in, host->sbuf_v + req->ooboffs,
					req->ooblen);
		}

		if (ret == -EBADMSG) {
			ecc_failed = true;
			mtd->ecc_stats.failed += ecc_stats.failed;
		} else {
			mtd->ecc_stats.corrected += ecc_stats.corrected;
			max_bitflips = max_t(unsigned int, max_bitflips,
							ecc_stats.corrected);
		}

		ret = 0;
		ops->retlen += iter.req.datalen;
		ops->oobretlen += iter.req.ooblen;
	}

	sprd_nand_put_device(mtd);

err:
	mutex_unlock(&host->lock);

	if (ecc_failed && !ret)
		ret = -EBADMSG;

	return ret ? ret : max_bitflips;
}

static int sprd_nand_mtd_write(struct mtd_info *mtd, loff_t to,
	struct mtd_oob_ops *ops)
{
	struct sprd_nfc *host = mtd_to_sprd_nfc(mtd);
	struct nand_device *nand = mtd_to_nanddev(mtd);
	u32 remain_page_inbuf = 1, ret_num;
	struct nand_page_io_req *req;
	struct nand_io_iter iter;
	bool random = false;
	u32 page;
	struct mtd_part *spl = NULL;
	int ret = 0;

	mutex_lock(&host->lock);
	ret = sprd_nand_get_device(mtd);
	if (ret)
		goto err;

	spl = container_of(mtd->partitions.next, struct mtd_part, node);

	nanddev_io_for_each_page(nand, NAND_PAGE_WRITE, to, ops, &iter) {
		req = &iter.req;

		if (req->datalen) {
			memset(host->mbuf_v, 0xFF, nanddev_page_size(nand));
			memcpy(host->mbuf_v + req->dataoffs, req->databuf.out,
					req->datalen);
		}

		if (req->ooblen) {
			memset(host->sbuf_v, 0xFF, nanddev_per_page_oobsize(nand));
			if (req->mode == MTD_OPS_AUTO_OOB)
				mtd_ooblayout_set_databytes(mtd, req->oobbuf.out, host->sbuf_v,
					req->ooboffs, req->ooblen);
			else
				memcpy(host->sbuf_v + req->ooboffs, req->oobbuf.out,
					req->ooblen);
		}

		page = (req->pos.eraseblock << (host->blkshift - host->pageshift))
		       + req->pos.page;
		if ((page < spl->size / host->param.page_size)
				&& (ops->mode != MTD_OPS_RAW))
			random = true;

		ret = host->ops->write_page(host,
				(req->pos.eraseblock << (host->blkshift - host->pageshift))
				+ req->pos.page,
				remain_page_inbuf, &ret_num,
				(u32)(!!req->databuf.out), (u32)(!!req->oobbuf.out),
				ops->mode, random);

		if (ret)
			break;

		ops->retlen += iter.req.datalen;
		ops->oobretlen += iter.req.ooblen;
	}

	sprd_nand_put_device(mtd);

err:
	mutex_unlock(&host->lock);

	return ret;
}

static int sprd_nand_mtd_block_isbad(struct mtd_info *mtd, loff_t offs)
{
	struct nand_device *nand = mtd_to_nanddev(mtd);
	struct sprd_nfc *host = mtd_to_sprd_nfc(mtd);
	struct nand_pos pos;
	int ret = 0;

	nanddev_offs_to_pos(nand, offs, &pos);
	mutex_lock(&host->lock);
	ret = sprd_nand_get_device(mtd);
	if (ret)
		goto err;

	ret = nanddev_isbad(nand, &pos);
	sprd_nand_put_device(mtd);

err:
	mutex_unlock(&host->lock);

	return ret;
}

static int sprd_nand_mtd_block_markbad(struct mtd_info *mtd, loff_t offs)
{
	struct nand_device *nand = mtd_to_nanddev(mtd);
	struct sprd_nfc *host = mtd_to_sprd_nfc(mtd);
	struct nand_pos pos;
	int ret = 0;

	nanddev_offs_to_pos(nand, offs, &pos);
	mutex_lock(&host->lock);
	ret = sprd_nand_get_device(mtd);
	if (ret)
		goto err;

	ret = nanddev_markbad(nand, &pos);
	sprd_nand_put_device(mtd);

err:
	mutex_unlock(&host->lock);

	return ret;
}

static int sprd_nand_mtd_erase(struct mtd_info *mtd, struct erase_info *einfo)
{
	struct sprd_nfc *host = mtd_to_sprd_nfc(mtd);
	int ret = 0;

	mutex_lock(&host->lock);
	ret = sprd_nand_get_device(mtd);
	if (ret)
		goto err;

	ret = nanddev_mtd_erase(mtd, einfo);
	sprd_nand_put_device(mtd);

err:
	mutex_unlock(&host->lock);

	return ret;
}

static int sprd_nand_mtd_block_isreserved(struct mtd_info *mtd, loff_t offs)
{
	struct nand_device *nand = mtd_to_nanddev(mtd);
	struct sprd_nfc *host = mtd_to_sprd_nfc(mtd);
	struct nand_pos pos;
	int ret = 0;

	nanddev_offs_to_pos(nand, offs, &pos);
	mutex_lock(&host->lock);
	ret = sprd_nand_get_device(mtd);
	if (ret)
		goto err;

	ret = nanddev_isreserved(nand, &pos);
	sprd_nand_put_device(mtd);

err:
	mutex_unlock(&host->lock);

	return ret;
}

static int sprd_nand_mtd_suspend(struct mtd_info *mtd)
{
	struct sprd_nfc *host = mtd_to_sprd_nfc(mtd);

	mutex_lock(&host->lock);
	if (!host->suspended) {
		host->ops->ctrl_suspend(host);
		host->suspended = 1;
	}
	mutex_unlock(&host->lock);

	return 0;
}

static void sprd_nand_mtd_resume(struct mtd_info *mtd)
{
	struct sprd_nfc *host = mtd_to_sprd_nfc(mtd);
	int ret;

	mutex_lock(&host->lock);
	ret = host->ops->ctrl_en(host, true);
	if (ret) {
		dev_err(host->dev, "%s ctrl enable failed,%d\n", __func__, ret);
		mutex_unlock(&host->lock);
		return;
	}

	if (!host->suspended)
		dev_dbg(host->dev, "warning ctrl is not in suspend state\n");
	else {
		host->ops->ctrl_resume(host);
		host->suspended = 0;
	}

	host->ops->ctrl_en(host, false);
	mutex_unlock(&host->lock);
}

static int sprd_nand_erase(struct nand_device *nand, const struct nand_pos *pos)
{
	struct sprd_nfc *host = nanddev_to_sprd_nfc(nand);
	unsigned int row = nanddev_pos_to_row(nand, pos);

	return host->ops->erase(host, row);
}

static int sprd_nand_markbad(struct nand_device *nand, const struct nand_pos *pos)
{
	struct sprd_nfc *host = nanddev_to_sprd_nfc(nand);
	unsigned int row = nanddev_pos_to_row(nand, pos);
	bool random = false;
	unsigned int offs = 0;
	struct mtd_part *spl = NULL;

	spl = container_of(host->nand.mtd.partitions.next, struct mtd_part, node);
	offs = nanddev_pos_to_offs(nand, pos);

	if (spl->offset >= offs && offs < (spl->offset + spl->size))
		random = true;

	return host->ops->block_mark_bad(host, row, random);
}

static bool sprd_nand_isbad(struct nand_device *nand, const struct nand_pos *pos)
{
	struct sprd_nfc *host = nanddev_to_sprd_nfc(nand);
	unsigned int row = nanddev_pos_to_row(nand, pos);
	bool random = false;
	unsigned int offs = 0;
	struct mtd_part *spl = NULL;

	spl = container_of(host->nand.mtd.partitions.next, struct mtd_part, node);
	offs = nanddev_pos_to_offs(nand, pos);

	if (spl->offset >= offs && offs < (spl->offset + spl->size))
		random = true;

	return !!host->ops->block_is_bad(host, row, random);
}

static const struct nand_ops sprd_nand_ops = {
	.erase = sprd_nand_erase,
	.markbad = sprd_nand_markbad,
	.isbad = sprd_nand_isbad,
};

static int sprd_nand_ooblayout_free(struct mtd_info *mtd, int section,
	struct mtd_oob_region *oob_region)
{
	struct sprd_nfc *host = mtd_to_sprd_nfc(mtd);

	if (section)
		return -EINVAL;

	oob_region->length = host->param.info_size << (host->pageshift - host->sectshift);
	oob_region->offset = 2;

	return 0;
}

static int sprd_nand_ooblayout_ecc(struct mtd_info *mtd, int section,
	struct mtd_oob_region *oob_region)
{
	struct sprd_nfc *host = mtd_to_sprd_nfc(mtd);

	if (section)
		return -EINVAL;

	oob_region->length = host->param.ecc_size << (host->pageshift - host->sectshift);
	oob_region->offset = mtd->oobsize - oob_region->length;

	return 0;
}

static const struct mtd_ooblayout_ops sprd_nand_ooblayout_ops = {
	.free = sprd_nand_ooblayout_free,
	.ecc = sprd_nand_ooblayout_ecc,
};

#ifdef CONFIG_MTD_NAND_SPRD_DEVICE_INFO_FROM_H
static struct sprd_nand_maker *sprd_nfc_find_maker(u8 idmaker)
{
	struct sprd_nand_maker *pmaker = maker_table;

	while (pmaker->idmaker != 0) {
		if (pmaker->idmaker == idmaker)
			return pmaker;
		pmaker++;
	}

	return NULL;
}

static struct sprd_nand_device *
sprd_nfc_find_device(struct sprd_nand_maker *pmaker, u8 id_device)
{
	struct sprd_nand_device *pdevice = pmaker->p_devtab;

	while (pdevice->id_device != 0) {
		if (pdevice->id_device == id_device)
			return pdevice;
		pdevice++;
	}

	return NULL;
}

static void sprd_nand_set_memorg(struct sprd_nfc *host, struct sprd_nand_param *param)
{
	struct nand_memory_organization *memorg;

	memorg = nanddev_get_memorg(&host->nand);
	memorg->bits_per_cell = 1;
	memorg->pagesize =  param->page_size;
	memorg->pages_per_eraseblock = param->blk_size/param->page_size;
	memorg->eraseblocks_per_lun = param->blk_num;
	memorg->planes_per_lun = 1;
	memorg->luns_per_target = 1;
	memorg->ntargets = host->csnum;
	memorg->oobsize = param->s_oob.oob_size;
}
#endif

static void sprd_nfc_print_info(struct sprd_nand_param *p)
{
#ifdef CONFIG_MTD_NAND_SPRD_DEVICE_INFO_FROM_H
	struct sprd_nand_maker *pmaker = sprd_nfc_find_maker(p->idmaker);
	struct sprd_nand_device *pdevice = sprd_nfc_find_device(pmaker, p->id_device);

	if (!pdevice)
		pr_info("idmaker (%02x) is in conflict with other\n", p->idmaker);
	else
		pr_info("nand device is %s:%s\n", pmaker->p_name, pdevice->p_name);
#endif
	pr_info("nand device block size is %d\n", p->blk_size);
	pr_info("nand device page size is %d\n", p->page_size);
	pr_info("nand device spare size is %d\n", p->nspare_size);
	pr_info("nand device eccbits is %d\n", p->s_oob.ecc_bits);
}

struct sprd_nand_param *sprd_get_nand_param(struct sprd_nfc *host)
{
#ifdef CONFIG_MTD_NAND_SPRD_DEVICE_INFO_FROM_H
	struct sprd_nand_param *param = sprd_nand_param_table;

	for (; param->idmaker != 0; param++) {
		if (!memcmp(param->id, host->param.id, 5)) {
			sprd_nfc_print_info(param);
			sprd_nand_set_memorg(host, param);

			return param;
		}
	}

	pr_err("Nand params unconfig, please check it. Halt on booting!!!\n");
	WARN_ON(true);

	return NULL;
#else
	struct sprd_nand_param *param = &g_nfc_vendor;

	sprd_nfc_print_info(param);

	return param;
#endif
}

static int sprd_nand_parse_dt(struct platform_device *pdev, struct sprd_nfc *host)
{
	u32 cell_type;
	int plane_sel_bit;
	u32 supt_cache;
	u32 ecc_param[3];
	u32 time_param[3];
	struct nand_memory_organization *memorg;
	struct ext_param ext_info;
	int nlun;
	int ret;

	struct device *dev = &pdev->dev;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->ioaddr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(host->ioaddr))
		return -EINVAL;

	host->randomizer = of_property_read_bool(pdev->dev.of_node, "sprd,random-mode");

	host->nandc_2x_clk = devm_clk_get(dev, "nandc_2x_clk");
	if (IS_ERR(host->nandc_2x_clk))
		return PTR_ERR(host->nandc_2x_clk);

	host->nandc_ecc_clk = devm_clk_get(dev, "nandc_ecc_clk");
	if (IS_ERR(host->nandc_ecc_clk))
		return PTR_ERR(host->nandc_ecc_clk);

	host->nandc_2xwork_clk = devm_clk_get(dev, "nandc_2xwork_clk");
	if (IS_ERR(host->nandc_2xwork_clk))
		return PTR_ERR(host->nandc_2xwork_clk);

	host->nandc_eccwork_clk = devm_clk_get(dev, "nandc_eccwork_clk");
	if (IS_ERR(host->nandc_eccwork_clk))
		return PTR_ERR(host->nandc_eccwork_clk);

	host->nandc_ahb_enable = devm_clk_get(dev, "nandc_ahb_enable");
	if (IS_ERR(host->nandc_ahb_enable))
		return PTR_ERR(host->nandc_ahb_enable);

	if (host->version == SPRD_NANDC_VERSIONS_R6P0) {
		host->nandc_ecc_clk_eb = devm_clk_get(dev, "nandc_ecc_enable");
		if (IS_ERR(host->nandc_ecc_clk_eb))
			return PTR_ERR(host->nandc_ecc_clk_eb);

		host->nandc_26m_clk_eb = devm_clk_get(dev, "nandc_26m_enable");
		if (IS_ERR(host->nandc_26m_clk_eb))
			return PTR_ERR(host->nandc_26m_clk_eb);

		host->nandc_1x_clk_eb = devm_clk_get(dev, "nandc_1x_enable");
		if (IS_ERR(host->nandc_1x_clk_eb))
			return PTR_ERR(host->nandc_1x_clk_eb);

		host->nandc_2x_clk_eb = devm_clk_get(dev, "nandc_2x_enable");
		if (IS_ERR(host->nandc_2x_clk_eb))
			return PTR_ERR(host->nandc_2x_clk_eb);
	}

	memorg = nanddev_get_memorg(&host->nand);
	ret = of_property_read_u32_array(pdev->dev.of_node, "memorg", (u32 *)memorg,
			sizeof(struct nand_memory_organization) / sizeof(memorg->bits_per_cell));
	if (ret)
		return ret;

	ret = of_property_read_u32_array(pdev->dev.of_node, "ecc", (u32 *)&ecc_param,
					ARRAY_SIZE(ecc_param));
	if (ret)
		return ret;

	ret = of_property_read_u32_array(pdev->dev.of_node, "param", (u32 *)&ext_info,
					sizeof(struct ext_param) / sizeof(ext_info.nsect_size));
	if (ret)
		return ret;

	ret = of_property_read_u32_array(pdev->dev.of_node, "timing", (u32 *)&time_param,
					ARRAY_SIZE(time_param));
	if (ret) {
		dev_warn(&pdev->dev, "set default, get timing failed: %d\n", ret);
		time_param[0] = 10;
		time_param[1] = 10;
		time_param[2] = 15;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "cell_type", &cell_type);
	if (ret) {
		dev_warn(&pdev->dev, "set default, get cell_type failed: %d\n", ret);
		cell_type = 1;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "supt_cache_func", &supt_cache);
	if (ret) {
		dev_warn(&pdev->dev, "set default, get supt_cache_func failed: %d\n", ret);
		supt_cache = 0;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "plane_sel_bit", &plane_sel_bit);
	if (ret) {
		dev_warn(&pdev->dev, "set default, get plane_sel_bit failed: %d\n", ret);
		plane_sel_bit = -1;
	}

	host->param.supt_cache_func = supt_cache;
	host->param.cell_type = cell_type;
	host->param.plane_sel_bit = plane_sel_bit;

	memset(&g_nfc_vendor, 0, sizeof(g_nfc_vendor));

	nlun = memorg->luns_per_target * memorg->ntargets;
	g_nfc_vendor.blk_num = memorg->eraseblocks_per_lun * nlun;
	g_nfc_vendor.blk_size = memorg->pagesize * memorg->pages_per_eraseblock;
	g_nfc_vendor.page_size = memorg->pagesize;
	g_nfc_vendor.nsect_size = ext_info.nsect_size;
	g_nfc_vendor.nspare_size = memorg->oobsize;
	g_nfc_vendor.nbus_width = ext_info.nbus_width;
	g_nfc_vendor.ncycles = ext_info.ncycle;

	g_nfc_vendor.s_oob.oob_size = memorg->oobsize;
	g_nfc_vendor.s_oob.ecc_bits = ecc_param[0];
	g_nfc_vendor.s_oob.ecc_pos = ecc_param[1];
	g_nfc_vendor.s_oob.ecc_size = (14 * (g_nfc_vendor.s_oob.ecc_bits) + 7) / 8;
	g_nfc_vendor.s_oob.info_pos = ext_info.info_pos;
	g_nfc_vendor.s_oob.info_size = ext_info.info_size;

	g_nfc_vendor.s_timing.ace_ns = time_param[0];
	g_nfc_vendor.s_timing.rwh_ns = time_param[1];
	g_nfc_vendor.s_timing.rwl_ns = time_param[2];

	return 0;
}

static const struct of_device_id sprd_nand_of_match[] = {
	{.compatible = "sprd,nandc-r6p0", .data = &nandc_r6p0_vops},
	{.compatible = "sprd,nandc-r8p0", .data = &nandc_r8p0_vops},
	{ }
};

static int sprd_nand_drv_probe(struct platform_device *pdev)
{
	const struct of_device_id *nand_of_id;
	struct nandc_variant_ops *vops;
	u32 intf_type;
	struct sprd_nand_param *param;
	struct mtd_info *mtd;
	struct sprd_nfc *host;
	u32 id0, id1;
	int ret;

	nand_of_id = of_match_node(sprd_nand_of_match, pdev->dev.of_node);
	vops = (struct nandc_variant_ops *)nand_of_id->data;

	ret = of_property_read_u32(pdev->dev.of_node, "intf_type", &intf_type);
	if (ret) {
		dev_warn(&pdev->dev, "set default, get intf-type failed: %d\n", ret);
		intf_type = 0;
	}

	host = sprd_nfc_alloc_host(&pdev->dev, intf_type);
	if (IS_ERR(host)) {
		ret = PTR_ERR(host);
		dev_warn(&pdev->dev, "cannot allocate memory for sprd_nfc\n");
		goto err;
	}

	host->version = vops->version;
	host->irq = platform_get_irq(pdev, 0);
	if (host->irq < 0) {
		ret = -EINVAL;
		dev_warn(&pdev->dev, "get irq failed\n");
		goto err_free_host;
	}

	ret = sprd_nand_parse_dt(pdev, host);
	if (ret) {
		dev_warn(&pdev->dev, "parse dt failed: %d\n", ret);
		goto err_free_host;
	}

	ret = host->ops->probe_device(host, &id0, &id1);
	if (ret)
		goto err_free_host;

	param = sprd_get_nand_param(host);
	if (!param)
		goto err_free_host;

	ret = sprd_nfc_setup_host(host, param, 1);
	if (ret)
		goto err_free_host;

	platform_set_drvdata(pdev, host);

	mutex_init(&host->lock);

	ret = nanddev_init(&host->nand, &sprd_nand_ops, THIS_MODULE);
	if (ret)
		goto err_free_host;

	mtd = &host->nand.mtd;
	mtd->dev.parent = host->dev;
	mtd->priv = host;
	mtd->oobavail = host->param.info_size << (host->pageshift - host->sectshift);

	mtd->bitflip_threshold = host->risk_threshold;
	mtd->name = "sprd-nand";

	mtd_set_ooblayout(mtd, &sprd_nand_ooblayout_ops);
	mtd->ecc_strength = host->param.ecc_mode;

	mtd->_erase = sprd_nand_mtd_erase;
	mtd->_read_oob = sprd_nand_mtd_read;
	mtd->_write_oob = sprd_nand_mtd_write;
	mtd->_block_isbad = sprd_nand_mtd_block_isbad;
	mtd->_block_markbad = sprd_nand_mtd_block_markbad;
	mtd->_block_isreserved = sprd_nand_mtd_block_isreserved;
	mtd->_suspend = sprd_nand_mtd_suspend;
	mtd->_resume = sprd_nand_mtd_resume;

	ret = mtd_device_parse_register(mtd, part_probes, 0, 0, 0);
	if (ret)
		goto err_nanddev_cleanup;

	return 0;

err_nanddev_cleanup:
	nanddev_cleanup(&host->nand);
err_free_host:
	sprd_nfc_free_host(host);
err:
	dev_err(&pdev->dev, "sprd-nand probe failed: %d\n", ret);

	return ret;
}

static int sprd_nand_drv_remove(struct platform_device *pdev)
{
	struct sprd_nfc *host = platform_get_drvdata(pdev);

	sprd_nfc_free_host(host);

	return 0;
}

MODULE_DEVICE_TABLE(of, sprd_nand_of_match);
static struct platform_driver sprd_nand_driver = {
	.probe = sprd_nand_drv_probe,
	.remove = sprd_nand_drv_remove,
	.driver = {
		.name = "sprd-nand",
		.of_match_table = sprd_nand_of_match,
	},
};
module_platform_driver(sprd_nand_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SPRD MTD NAND driver");
