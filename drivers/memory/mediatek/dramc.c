// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <soc/mediatek/dramc.h>
#include <linux/bug.h>
#include <linux/regulator/consumer.h>

static struct platform_device *dramc_pdev;
static struct platform_driver dramc_drv;
static struct regulator *reg_vdram2;

static int mr4_v1_init(struct platform_device *pdev,
	struct mr4_dev_t *mr4_dev_ptr)
{
	struct device_node *dramc_node = pdev->dev.of_node;
	int ret;

	mr4_dev_ptr->version = 1;

	ret = of_property_read_u32_array(dramc_node,
		"mr4_rg", (unsigned int *)&(mr4_dev_ptr->mr4_rg), 3);

	return ret;
}

static int fmeter_v0_init(struct platform_device *pdev,
	struct fmeter_dev_t *fmeter_dev_ptr)
{
	struct device_node *dramc_node = pdev->dev.of_node;
	int ret;

	fmeter_dev_ptr->version = 0;

	ret = of_property_read_u32(dramc_node,
		"crystal_freq", &(fmeter_dev_ptr->crystal_freq));
	ret |= of_property_read_u32(dramc_node,
		"shu_of", &(fmeter_dev_ptr->shu_of));
	ret |= of_property_read_u32_array(dramc_node,
		"shu_lv", (unsigned int *)&(fmeter_dev_ptr->shu_lv), 3);
	ret |= of_property_read_u32_array(dramc_node,
		"pll_id", (unsigned int *)&(fmeter_dev_ptr->pll_id), 3);
	ret |= of_property_read_u32_array(dramc_node,
		"sdmpcw", (unsigned int *)(fmeter_dev_ptr->sdmpcw), 6);
	ret |= of_property_read_u32_array(dramc_node,
		"prediv", (unsigned int *)(fmeter_dev_ptr->prediv), 6);
	ret |= of_property_read_u32_array(dramc_node,
		"posdiv", (unsigned int *)(fmeter_dev_ptr->posdiv), 6);
	ret |= of_property_read_u32_array(dramc_node,
		"ckdiv4", (unsigned int *)(fmeter_dev_ptr->ckdiv4), 6);

	return ret;
}


static int fmeter_v1_init(struct platform_device *pdev,
	struct fmeter_dev_t *fmeter_dev_ptr)
{
	struct device_node *dramc_node = pdev->dev.of_node;
	int ret;

	fmeter_dev_ptr->version = 1;

	ret = of_property_read_u32(dramc_node,
		"crystal_freq", &(fmeter_dev_ptr->crystal_freq));
	ret |= of_property_read_u32(dramc_node,
		"shu_of", &(fmeter_dev_ptr->shu_of));
	ret |= of_property_read_u32_array(dramc_node,
		"shu_lv", (unsigned int *)&(fmeter_dev_ptr->shu_lv), 3);
	ret |= of_property_read_u32_array(dramc_node,
		"pll_id", (unsigned int *)&(fmeter_dev_ptr->pll_id), 3);
	ret |= of_property_read_u32_array(dramc_node,
		"pll_md", (unsigned int *)(fmeter_dev_ptr->pll_md), 6);
	ret |= of_property_read_u32_array(dramc_node,
		"sdmpcw", (unsigned int *)(fmeter_dev_ptr->sdmpcw), 6);
	ret |= of_property_read_u32_array(dramc_node,
		"prediv", (unsigned int *)(fmeter_dev_ptr->prediv), 6);
	ret |= of_property_read_u32_array(dramc_node,
		"posdiv", (unsigned int *)(fmeter_dev_ptr->posdiv), 6);
	ret |= of_property_read_u32_array(dramc_node,
		"ckdiv4", (unsigned int *)(fmeter_dev_ptr->ckdiv4), 6);
	ret |= of_property_read_u32_array(dramc_node,
		"cldiv2", (unsigned int *)(fmeter_dev_ptr->cldiv2), 6);
	ret |= of_property_read_u32_array(dramc_node,
		"fbksel", (unsigned int *)(fmeter_dev_ptr->fbksel), 6);
	ret |= of_property_read_u32_array(dramc_node,
		"dqsopen", (unsigned int *)(fmeter_dev_ptr->dqsopen), 6);
	ret |= of_property_read_u32_array(dramc_node,
		"dqopen", (unsigned int *)(fmeter_dev_ptr->dqopen), 6);
	ret |= of_property_read_u32_array(dramc_node,
		"ckdiv4_ca", (unsigned int *)(fmeter_dev_ptr->ckdiv4_ca), 6);

	return ret;
}

static ssize_t mr_show(struct device_driver *driver, char *buf)
{
	struct dramc_dev_t *dramc_dev_ptr =
		(struct dramc_dev_t *)platform_get_drvdata(dramc_pdev);
	struct mr_info_t *mr_info_ptr = dramc_dev_ptr->mr_info_ptr;
	unsigned int i;
	ssize_t ret;

	for (ret = 0, i = 0; i < dramc_dev_ptr->mr_cnt; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "mr%d: 0x%x\n",
			mr_info_ptr[i].mr_index, mr_info_ptr[i].mr_value);
		if (ret >= PAGE_SIZE)
			return strlen(buf);
	}

	return strlen(buf);
}

static ssize_t mr4_show(struct device_driver *driver, char *buf)
{
	struct dramc_dev_t *dramc_dev_ptr =
		(struct dramc_dev_t *)platform_get_drvdata(dramc_pdev);
	unsigned int i;
	ssize_t ret;

	for (ret = 0, i = 0; i < dramc_dev_ptr->ch_cnt; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"mr4: ch%d 0x%x\n", i, mtk_dramc_get_mr4(i));
		if (ret >= PAGE_SIZE)
			return strlen(buf);
	}

	return strlen(buf);
}

static ssize_t dram_data_rate_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "DRAM data rate = %d\n",
		mtk_dramc_get_data_rate());
}

__weak int mtk_dramc_binning_test(void)
{
	return 0;
}

static ssize_t binning_test_show(struct device_driver *driver, char *buf)
{
	int ret;

	ret = mtk_dramc_binning_test();
	if (!ret)
		return snprintf(buf, PAGE_SIZE, "unsupport mem test\n");
	else if (ret > 0)
		return snprintf(buf, PAGE_SIZE, "mem test all pass\n");
	else
		return snprintf(buf, PAGE_SIZE, "mem test failed %d\n", ret);
}

static DRIVER_ATTR_RO(mr);
static DRIVER_ATTR_RO(mr4);
static DRIVER_ATTR_RO(dram_data_rate);
static DRIVER_ATTR_RO(binning_test);

static int dramc_probe(struct platform_device *pdev)
{
	struct device_node *dramc_node = pdev->dev.of_node;
	struct dramc_dev_t *dramc_dev_ptr;
	unsigned int vdram2_enable;
	unsigned int mr4_version;
	unsigned int fmeter_version;
	struct resource *res;
	unsigned int i, size, retval;
	int ret;

	pr_info("%s: module probe.\n", __func__);
	dramc_pdev = pdev;
	dramc_dev_ptr = devm_kmalloc(&pdev->dev,
		sizeof(struct dramc_dev_t), GFP_KERNEL);

	if (!dramc_dev_ptr)
		return -ENOMEM;
	ret = of_property_read_u32(dramc_node,
		"dram_type", &(dramc_dev_ptr->dram_type));
	if (ret) {
		pr_info("%s: get dram_type fail\n", __func__);
		return -EINVAL;
	}
	ret = of_property_read_u32(dramc_node,
		"support_ch_cnt", &(dramc_dev_ptr->support_ch_cnt));
	if (ret) {
		pr_info("%s: get support_ch_cnt fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(dramc_node,
		"ch_cnt", &(dramc_dev_ptr->ch_cnt));
	if (ret) {
		pr_info("%s: get ch_cnt fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(dramc_node,
		"rk_cnt", &(dramc_dev_ptr->rk_cnt));
	if (ret) {
		pr_info("%s: get rk_cnt fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(dramc_node,
		"mr_cnt", &(dramc_dev_ptr->mr_cnt));
	if (ret) {
		pr_info("%s: get mr_cnt fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(dramc_node,
		"freq_cnt", &(dramc_dev_ptr->freq_cnt));
	if (ret) {
		pr_info("%s: get freq_cnt fail\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(dramc_node, "mr4_version", &mr4_version);
	if (ret)
		pr_info("%s: not support mr4\n", __func__);
	else if (mr4_version == 1) {
		dramc_dev_ptr->mr4_dev_ptr = devm_kmalloc(&pdev->dev,
				sizeof(struct mr4_dev_t), GFP_KERNEL);
		if (!(dramc_dev_ptr->mr4_dev_ptr))
			return -ENOMEM;
		ret = mr4_v1_init(pdev,
			(struct mr4_dev_t *)(dramc_dev_ptr->mr4_dev_ptr));
		if (ret) {
			pr_info("%s: mr4_v1_init fail\n", __func__);
			return -EINVAL;
		}
	} else
		dramc_dev_ptr->mr4_dev_ptr = NULL;

	pr_info("%s: %s(%d),%s(%d),%s(%d),%s(%d),%s(%d),%s(%d),%s(%s)\n",
		__func__,
		"dram_type", dramc_dev_ptr->dram_type,
		"support_ch_cnt", dramc_dev_ptr->support_ch_cnt,
		"ch_cnt", dramc_dev_ptr->ch_cnt,
		"rk_cnt", dramc_dev_ptr->rk_cnt,
		"mr_cnt", dramc_dev_ptr->mr_cnt,
		"freq_cnt", dramc_dev_ptr->freq_cnt,
		"mr4", (dramc_dev_ptr->mr4_dev_ptr) ? "true" : "false");

	/*for vdram2 regulator*/
	ret = of_property_read_u32(
		dramc_node, "vdram2_enable", &vdram2_enable);
	if (ret)
		pr_info("%s: no need enable vdram2\n", __func__);
	else if (vdram2_enable && (TYPE_LPDDR4X == dramc_dev_ptr->dram_type)) {
		reg_vdram2 = regulator_get(&pdev->dev, "vdram2");
		if (!IS_ERR(reg_vdram2)) {
			retval = regulator_enable(reg_vdram2);
			if (retval < 0) {
				pr_info("regulator_enable vdram2 failed: %d\n", retval);
				return -EINVAL;
			}
		} else {
			pr_info("regulator_get vdram2 failed\n");
			reg_vdram2 = NULL;
			return -EINVAL;
		}
		pr_info("regulator_enable vdram2 success: %d\n", retval);
	}

	size = sizeof(unsigned int) * dramc_dev_ptr->rk_cnt;
	dramc_dev_ptr->rk_size = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!(dramc_dev_ptr->rk_size))
		return -ENOMEM;
	ret = of_property_read_u32_array(dramc_node,
		"rk_size", dramc_dev_ptr->rk_size, dramc_dev_ptr->rk_cnt);
	if (ret) {
		pr_info("%s: get rk_size fail\n", __func__);
		return -EINVAL;
	}

	size = sizeof(struct mr_info_t) * dramc_dev_ptr->mr_cnt;
	dramc_dev_ptr->mr_info_ptr = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(dramc_dev_ptr->mr_info_ptr))
		return -ENOMEM;
	ret = of_property_read_u32_array(dramc_node, "mr",
		(unsigned int *)dramc_dev_ptr->mr_info_ptr, size >> 2);
	if (ret) {
		pr_info("%s: get mr_info fail\n", __func__);
		return -EINVAL;
	}
	for (i = 0; i < dramc_dev_ptr->mr_cnt; i++)
		pr_info("%s: mr%d(%x)\n", __func__,
			dramc_dev_ptr->mr_info_ptr[i].mr_index,
			dramc_dev_ptr->mr_info_ptr[i].mr_value);

	size = sizeof(unsigned int) * dramc_dev_ptr->freq_cnt;
	dramc_dev_ptr->freq_step = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!(dramc_dev_ptr->freq_step))
		return -ENOMEM;
	ret = of_property_read_u32_array(dramc_node, "freq_step",
		dramc_dev_ptr->freq_step, dramc_dev_ptr->freq_cnt);
	if (ret) {
		pr_info("%s: get freq_step fail\n", __func__);
		return -EINVAL;
	}

	dramc_dev_ptr->sleep_base = of_iomap(dramc_node,
		dramc_dev_ptr->support_ch_cnt * 4);
	if (IS_ERR(dramc_dev_ptr->sleep_base)) {
		pr_info("%s: unable to map sleep base\n", __func__);
		return -EINVAL;
	}

	size = sizeof(phys_addr_t) * dramc_dev_ptr->support_ch_cnt;
	dramc_dev_ptr->dramc_chn_base_ao = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(dramc_dev_ptr->dramc_chn_base_ao))
		return -ENOMEM;
	dramc_dev_ptr->dramc_chn_base_nao = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(dramc_dev_ptr->dramc_chn_base_nao))
		return -ENOMEM;
	dramc_dev_ptr->ddrphy_chn_base_ao = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(dramc_dev_ptr->ddrphy_chn_base_ao))
		return -ENOMEM;
	dramc_dev_ptr->ddrphy_chn_base_nao = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(dramc_dev_ptr->ddrphy_chn_base_nao))
		return -ENOMEM;

	for (i = 0; i < dramc_dev_ptr->support_ch_cnt; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		dramc_dev_ptr->dramc_chn_base_ao[i] =
			devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(dramc_dev_ptr->dramc_chn_base_ao[i])) {
			pr_info("%s: unable to map ch%d DRAMC AO base\n",
				__func__, i);
			return -EINVAL;
		}

		res = platform_get_resource(pdev, IORESOURCE_MEM,
			i + dramc_dev_ptr->support_ch_cnt);
		dramc_dev_ptr->dramc_chn_base_nao[i] =
			devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(dramc_dev_ptr->dramc_chn_base_nao[i])) {
			pr_info("%s: unable to map ch%d DRAMC NAO base\n",
				__func__, i);
			return -EINVAL;
		}

		res = platform_get_resource(pdev, IORESOURCE_MEM,
			i + dramc_dev_ptr->support_ch_cnt * 2);
		dramc_dev_ptr->ddrphy_chn_base_ao[i] =
			devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(dramc_dev_ptr->ddrphy_chn_base_ao[i])) {
			pr_info("%s: unable to map ch%d DDRPHY AO base\n",
				__func__, i);
			return -EINVAL;
		}

		res = platform_get_resource(pdev, IORESOURCE_MEM,
			i + dramc_dev_ptr->support_ch_cnt * 3);
		dramc_dev_ptr->ddrphy_chn_base_nao[i] =
			devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(dramc_dev_ptr->ddrphy_chn_base_nao[i])) {
			pr_info("%s: unable to map ch%d DDRPHY NAO base\n",
				__func__, i);
			return -EINVAL;
		}
	}

	ret = of_property_read_u32(
		dramc_node, "fmeter_version", &fmeter_version);
	if (ret) {
		pr_info("%s: get fmeter_version fail\n", __func__);
		return -EINVAL;
	}
	pr_info("%s: fmeter_version(%d)\n", __func__, fmeter_version);

	dramc_dev_ptr->fmeter_dev_ptr = devm_kmalloc(&pdev->dev,
		sizeof(struct fmeter_dev_t), GFP_KERNEL);
	if (!(dramc_dev_ptr->fmeter_dev_ptr)) {
		pr_info("%s: memory  alloc fail\n", __func__);
		return -ENOMEM;
	}
	switch (fmeter_version) {
	case 0:
		ret = fmeter_v0_init(pdev, dramc_dev_ptr->fmeter_dev_ptr);
		if (ret) {
			pr_err("%s: fmeter_v0_init fail\n", __func__);
			return -EINVAL;
		}
		break;
	case 1:
		ret = fmeter_v1_init(pdev, dramc_dev_ptr->fmeter_dev_ptr);
		if (ret) {
			pr_info("%s: fmeter_v1_init fail\n", __func__);
			return -EINVAL;
		}
		break;
	default:
		devm_kfree(&pdev->dev, dramc_dev_ptr->fmeter_dev_ptr);
		dramc_dev_ptr->fmeter_dev_ptr = NULL;
	}

	ret = driver_create_file(
		pdev->dev.driver, &driver_attr_binning_test);
	if (ret) {
		pr_info("%s: fail to create binning_test sysfs\n", __func__);
		return ret;
	}

	ret = driver_create_file(
		pdev->dev.driver, &driver_attr_dram_data_rate);
	if (ret) {
		pr_info("%s: fail to create dram_data_rate sysfs\n", __func__);
		return ret;
	}

	ret = driver_create_file(
		pdev->dev.driver, &driver_attr_mr);
	if (ret) {
		pr_info("%s: fail to create mr sysfs\n", __func__);
		return ret;
	}

	if (dramc_dev_ptr->mr4_dev_ptr) {
		ret = driver_create_file(
			pdev->dev.driver, &driver_attr_mr4);
		if (ret) {
			pr_info("%s: fail to create mr4 sysfs\n", __func__);
			return ret;
		}
	}

	platform_set_drvdata(pdev, dramc_dev_ptr);
	pr_info("%s: DRAM data type = %d\n", __func__,
		mtk_dramc_get_ddr_type());

	pr_info("%s: DRAM data rate = %d\n", __func__,
		mtk_dramc_get_data_rate());
	return ret;
}

/*
 * mtk_dramc_get_steps_freq - get the freq of target DVFS step
 * @step:	the step index of DVFS
 *
 * Returns the DRAM freq
 */
int mtk_dramc_get_steps_freq(unsigned int step)
{
	struct dramc_dev_t *dramc_dev_ptr;

	if (!dramc_pdev)
		return -1;

	dramc_dev_ptr =
		(struct dramc_dev_t *)platform_get_drvdata(dramc_pdev);

	if (step < dramc_dev_ptr->freq_cnt)
		return dramc_dev_ptr->freq_step[step];

	return -1;
}
EXPORT_SYMBOL(mtk_dramc_get_steps_freq);

static unsigned int decode_freq_v0(unsigned int vco_freq)
{
	switch (vco_freq) {
	case 3588:
		return 3600;
	case 3198:
		return 3200;
	case 2392:
		return 2400;
	case 1859:
		return 1866;
	case 1599:
		return 1600;
	case 1534:
		return 1534;
	case 1196:
		return 1200;
	}

	return vco_freq;
}

static unsigned int decode_freq(unsigned int vco_freq)
{
	switch (vco_freq) {
	case 6370:
		return 6400;
	case 5486:
		return 5500;
	case 4264:
		return 4266;
	case 3718:
	case 3588:
		return 3733;
	case 3068:
		return 3200;
	case 2652:
	case 2664:
		return 2667;
	case 2366:
		return 2400;
	case 2132:
		return 2133;
	case 1859:
	case 1794:
		return 1866;
	case 1534:
		return 1600;
	case 1144:
	case 1196:
		return 1200;
	case 754:
	case 799:
		return 800;
	case 396:
		return 400;
	}

	return vco_freq;
}

static unsigned int fmeter_v0(struct dramc_dev_t *dramc_dev_ptr)
{
	struct fmeter_dev_t *fmeter_dev_ptr =
		(struct fmeter_dev_t *)dramc_dev_ptr->fmeter_dev_ptr;
	unsigned int shu_lv_val;
	unsigned int pll_id_val;
	unsigned int sdmpcw_val;
	unsigned int prediv_val;
	unsigned int posdiv_val;
	unsigned int ckdiv4_val;
	unsigned int offset;
	unsigned int vco_freq;

	shu_lv_val = (readl(dramc_dev_ptr->dramc_chn_base_ao[0] +
		fmeter_dev_ptr->shu_lv.offset) &
		fmeter_dev_ptr->shu_lv.mask) >>
		fmeter_dev_ptr->shu_lv.shift;

	pll_id_val = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] +
		fmeter_dev_ptr->pll_id.offset) &
		fmeter_dev_ptr->pll_id.mask) >>
		fmeter_dev_ptr->pll_id.shift;

	offset = fmeter_dev_ptr->sdmpcw[pll_id_val].offset +
		fmeter_dev_ptr->shu_of * shu_lv_val;
	sdmpcw_val = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] + offset) &
		fmeter_dev_ptr->sdmpcw[pll_id_val].mask) >>
		fmeter_dev_ptr->sdmpcw[pll_id_val].shift;

	offset = fmeter_dev_ptr->prediv[pll_id_val].offset +
		fmeter_dev_ptr->shu_of * shu_lv_val;
	prediv_val = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] + offset) &
		fmeter_dev_ptr->prediv[pll_id_val].mask) >>
		fmeter_dev_ptr->prediv[pll_id_val].shift;

	offset = fmeter_dev_ptr->posdiv[pll_id_val].offset +
		fmeter_dev_ptr->shu_of * shu_lv_val;
	posdiv_val = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] + offset) &
		fmeter_dev_ptr->posdiv[pll_id_val].mask) >>
		fmeter_dev_ptr->posdiv[pll_id_val].shift;

	offset = fmeter_dev_ptr->ckdiv4[pll_id_val].offset +
		fmeter_dev_ptr->shu_of * shu_lv_val;
	ckdiv4_val = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] + offset) &
		fmeter_dev_ptr->ckdiv4[pll_id_val].mask) >>
		fmeter_dev_ptr->ckdiv4[pll_id_val].shift;

	vco_freq = ((fmeter_dev_ptr->crystal_freq >> prediv_val) * (sdmpcw_val >> 8))
		>> posdiv_val >> ckdiv4_val;

	return decode_freq_v0(vco_freq);
}

static unsigned int fmeter_v1(struct dramc_dev_t *dramc_dev_ptr)
{
	struct fmeter_dev_t *fmeter_dev_ptr =
		(struct fmeter_dev_t *)dramc_dev_ptr->fmeter_dev_ptr;
	unsigned int shu_lv_val;
	unsigned int pll_id_val;
	unsigned int pll_md_val;
	unsigned int sdmpcw_val;
	unsigned int prediv_val;
	unsigned int posdiv_val;
	unsigned int ckdiv4_val;
	unsigned int cldiv2_val;
	unsigned int offset;
	unsigned int vco_freq;
	unsigned int fbksel;
	unsigned int dqsopen;
	unsigned int dqopen;
	unsigned int ckdiv4_ca_val;

	shu_lv_val = (readl(dramc_dev_ptr->ddrphy_chn_base_nao[0] +
		fmeter_dev_ptr->shu_lv.offset) &
		fmeter_dev_ptr->shu_lv.mask) >>
		fmeter_dev_ptr->shu_lv.shift;

	pll_id_val = (readl(dramc_dev_ptr->ddrphy_chn_base_nao[0] +
		fmeter_dev_ptr->pll_id.offset) &
		fmeter_dev_ptr->pll_id.mask) >>
		fmeter_dev_ptr->pll_id.shift;

	offset = fmeter_dev_ptr->pll_md[pll_id_val].offset +
		fmeter_dev_ptr->shu_of * shu_lv_val;
	pll_md_val = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] + offset) &
		fmeter_dev_ptr->pll_md[pll_id_val].mask) >>
		fmeter_dev_ptr->pll_md[pll_id_val].shift;

	offset = fmeter_dev_ptr->sdmpcw[pll_id_val].offset +
		fmeter_dev_ptr->shu_of * shu_lv_val;
	sdmpcw_val = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] + offset) &
		fmeter_dev_ptr->sdmpcw[pll_id_val].mask) >>
		fmeter_dev_ptr->sdmpcw[pll_id_val].shift;

	offset = fmeter_dev_ptr->prediv[pll_id_val].offset +
		fmeter_dev_ptr->shu_of * shu_lv_val;
	prediv_val = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] + offset) &
		fmeter_dev_ptr->prediv[pll_id_val].mask) >>
		fmeter_dev_ptr->prediv[pll_id_val].shift;

	offset = fmeter_dev_ptr->posdiv[pll_id_val].offset +
		fmeter_dev_ptr->shu_of * shu_lv_val;
	posdiv_val = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] + offset) &
		fmeter_dev_ptr->posdiv[pll_id_val].mask) >>
		fmeter_dev_ptr->posdiv[pll_id_val].shift;

	offset = fmeter_dev_ptr->ckdiv4[pll_id_val].offset +
		fmeter_dev_ptr->shu_of * shu_lv_val;
	ckdiv4_val = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] + offset) &
		fmeter_dev_ptr->ckdiv4[pll_id_val].mask) >>
		fmeter_dev_ptr->ckdiv4[pll_id_val].shift;

	offset = fmeter_dev_ptr->cldiv2[pll_id_val].offset +
		fmeter_dev_ptr->shu_of * shu_lv_val;
	cldiv2_val = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] + offset) &
		fmeter_dev_ptr->cldiv2[pll_id_val].mask) >>
		fmeter_dev_ptr->cldiv2[pll_id_val].shift;

	offset = fmeter_dev_ptr->fbksel[pll_id_val].offset +
		fmeter_dev_ptr->shu_of * shu_lv_val;
	fbksel = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] + offset) &
		fmeter_dev_ptr->fbksel[pll_id_val].mask) >>
		fmeter_dev_ptr->fbksel[pll_id_val].shift;

	offset = fmeter_dev_ptr->dqsopen[pll_id_val].offset +
		fmeter_dev_ptr->shu_of * shu_lv_val;
	dqsopen = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] + offset) &
		fmeter_dev_ptr->dqsopen[pll_id_val].mask) >>
		fmeter_dev_ptr->dqsopen[pll_id_val].shift;

	offset = fmeter_dev_ptr->dqopen[pll_id_val].offset +
		fmeter_dev_ptr->shu_of * shu_lv_val;
	dqopen = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] + offset) &
		fmeter_dev_ptr->dqopen[pll_id_val].mask) >>
		fmeter_dev_ptr->dqopen[pll_id_val].shift;

	offset = fmeter_dev_ptr->ckdiv4_ca[pll_id_val].offset +
		fmeter_dev_ptr->shu_of * shu_lv_val;
	ckdiv4_ca_val = (readl(dramc_dev_ptr->ddrphy_chn_base_ao[0] + offset) &
		fmeter_dev_ptr->ckdiv4_ca[pll_id_val].mask) >>
		fmeter_dev_ptr->ckdiv4_ca[pll_id_val].shift;

	vco_freq = ((fmeter_dev_ptr->crystal_freq >> prediv_val) *
		(sdmpcw_val >> 7)) >> posdiv_val >> 1 >> ckdiv4_val >>
		pll_md_val >> cldiv2_val << fbksel;

	if ((dqsopen == 1 || dqopen == 1) && (ckdiv4_ca_val == 1))
		vco_freq >>= 2;
	else if ((dqsopen == 1 || dqopen == 1) && (ckdiv4_ca_val == 0))
		vco_freq >>= 1;

	return decode_freq(vco_freq);
}

/*
 * mtk_dramc_get_data_rate - calculate DRAM data rate
 *
 * Returns DRAM data rate (MB/s)
 */
unsigned int mtk_dramc_get_data_rate(void)
{
	struct dramc_dev_t *dramc_dev_ptr;
	struct fmeter_dev_t *fmeter_dev_ptr;

	if (!dramc_pdev)
		return 0;

	dramc_dev_ptr =
		(struct dramc_dev_t *)platform_get_drvdata(dramc_pdev);

	fmeter_dev_ptr = (struct fmeter_dev_t *)dramc_dev_ptr->fmeter_dev_ptr;
	if (!fmeter_dev_ptr)
		return 0;

	switch (fmeter_dev_ptr->version) {
	case 0:
		return fmeter_v0(dramc_dev_ptr);
	case 1:
		return fmeter_v1(dramc_dev_ptr);
	default:
		return 0;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_dramc_get_data_rate);

static unsigned int mr4_v1(struct dramc_dev_t *dramc_dev_ptr, unsigned int ch)
{
	struct mr4_dev_t *mr4_dev_ptr =
		(struct mr4_dev_t *)dramc_dev_ptr->mr4_dev_ptr;

	return (readl(dramc_dev_ptr->dramc_chn_base_nao[ch] +
		mr4_dev_ptr->mr4_rg.offset) & mr4_dev_ptr->mr4_rg.mask) >>
		mr4_dev_ptr->mr4_rg.shift;
}

/*
 * mtk_dramc_get_mr4 - get the DRAM MR4 value of specific DRAM channel
 * @ch:	the channel index
 *
 * Returns the MR4 value
 */
unsigned int mtk_dramc_get_mr4(unsigned int ch)
{
	struct dramc_dev_t *dramc_dev_ptr;
	struct mr4_dev_t *mr4_dev_ptr;

	if (!dramc_pdev)
		return 0;

	dramc_dev_ptr =
		(struct dramc_dev_t *)platform_get_drvdata(dramc_pdev);

	mr4_dev_ptr = (struct mr4_dev_t *)dramc_dev_ptr->mr4_dev_ptr;
	if (!mr4_dev_ptr)
		return 0;

	if (ch >= dramc_dev_ptr->ch_cnt)
		return 0;

	if (mr4_dev_ptr->version == 1)
		return mr4_v1(dramc_dev_ptr, ch);

	return 0;
}
EXPORT_SYMBOL(mtk_dramc_get_mr4);

/*
 * mtk_dramc_get_ddr_type - get DRAM type
 *
 * Returns the DRAM type
 */
unsigned int mtk_dramc_get_ddr_type(void)
{
	struct dramc_dev_t *dramc_dev_ptr;

	if (!dramc_pdev)
		return 0;
	dramc_dev_ptr =
		(struct dramc_dev_t *)platform_get_drvdata(dramc_pdev);

	return dramc_dev_ptr->dram_type;
}
EXPORT_SYMBOL(mtk_dramc_get_ddr_type);

static int dramc_remove(struct platform_device *pdev)
{
	dramc_pdev = NULL;

	return 0;
}

static const struct of_device_id dramc_of_ids[] = {
	{.compatible = "mediatek,common-dramc",},
	{}
};

static struct platform_driver dramc_drv = {
	.probe = dramc_probe,
	.remove = dramc_remove,
	.driver = {
		.name = "dramc_drv",
		.owner = THIS_MODULE,
		.of_match_table = dramc_of_ids,
	},
};

static int __init dramc_drv_init(void)
{
	int ret;

	ret = platform_driver_register(&dramc_drv);
	if (ret) {
		pr_info("%s: init fail, ret 0x%x\n", __func__, ret);
		return ret;
	}

	return ret;
}
#if IS_BUILTIN(CONFIG_MTK_DRAMC)
subsys_initcall(dramc_drv_init);
#else
module_init(dramc_drv_init);
#endif


MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("MediaTek DRAMC Driver");
MODULE_LICENSE("GPL v2");
