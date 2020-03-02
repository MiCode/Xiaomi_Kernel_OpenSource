/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <asm/page.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/compiler.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <mt-plat/mtk_secure_api.h>
#include <mt_emi.h>
#include <mpu_v1.h>
#include <mpu_platform.h>
#include <devmpu.h>

#define LOG_TAG "[DEVMPU]"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) LOG_TAG " " fmt

#define get_bit_field(x, m, o)	((x & (m << o)) >> o)

static void __iomem *DEVMPU_BASE;
static const char *UNKNOWN_MASTER = "unknown";

struct devmpu_vio_stat {

	/* master ID */
	uint16_t id;

	/* master domain */
	uint8_t domain;

	/* is NS transaction (AXI sideband secure bit) */
	bool is_ns;

	/* is write violation */
	bool is_write;

	/* padding */
	uint8_t padding[3];

	/* physical address */
	uint64_t addr;
};

static unsigned int match_id(
	unsigned int axi_id, unsigned int tbl_idx, unsigned int port_id)
{
	if ((axi_id & mst_tbl[tbl_idx].id_mask) == mst_tbl[tbl_idx].id_val) {
		if (port_id == mst_tbl[tbl_idx].port)
			return 1;
	}

	return 0;
}

static const char *id2name(unsigned int axi_id, unsigned int port_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mst_tbl); i++) {
		if (match_id(axi_id, i, port_id))
			return mst_tbl[i].name;
	}

	return (char *)UNKNOWN_MASTER;
}

static int devmpu_vio_get(struct devmpu_vio_stat *vio, bool do_clear)
{
	size_t ret;

	size_t vio_addr;
	size_t vio_info;

	if (unlikely(vio == NULL)) {
		pr_err("%s:%d output pointer is NULL\n",
				__func__, __LINE__);
		return -1;
	}

	ret = mt_secure_call_ret3(MTK_SIP_KERNEL_DEVMPU_VIO_GET,
			do_clear, 0, 0, 0, &vio_addr, &vio_info);
	if (ret == (size_t)(-1)) {
		pr_err("%s:%d failed to get violation, ret=%zd\n",
				__func__, __LINE__, ret);
		return -1;
	}

	vio->addr = vio_addr;
	vio->is_write = (vio_info >> 0) & 0x1;
	vio->is_ns = (vio_info >> 1) & 0x1;
	vio->domain = (vio_info >> 8) & 0xFF;
	vio->id = (vio_info >> 16) & 0xFFFF;

	return 0;
}

int devmpu_print_violation(uint64_t vio_addr, uint32_t vio_id,
		uint32_t vio_domain, uint32_t vio_is_write, bool from_emimpu)
{
	size_t ret;
	struct devmpu_vio_stat vio;

	uint32_t vio_axi_id;
	uint32_t vio_port_id;

	/* overwrite violation info. with the DeviceMPU native one */
	if (!from_emimpu) {
		ret = devmpu_vio_get(&vio, true);
		if (ret) {
			pr_err("%s:%d failed to get DeviceMPU violation\n",
					__func__, __LINE__);
			return -1;
		}

		vio_id = vio.id;
		vio_addr = vio.addr;
		vio_domain = vio.domain;
		vio_is_write = (vio.is_write) ? 1 : 0;
	}

	vio_axi_id = (vio_id >> 3) & 0x1FFF;
	vio_port_id = vio_id & 0x7;

	pr_info("Device MPU violation\n");
	pr_info("current process is \"%s \" (pid: %i)\n",
			current->comm, current->pid);
	pr_info("corrupted address is 0x%llx\n",
			vio_addr);
	pr_info("master ID: 0x%x, AXI ID: 0x%x, port ID: 0x%x\n",
			vio_id, vio_axi_id, vio_port_id);
	pr_info("violation master is %s, from domain 0x%x\n",
			id2name(vio_axi_id, vio_port_id), vio_domain);

	if (__builtin_popcount(vio_is_write) == 1) {
		pr_info("%s violation\n",
				(vio_is_write) ? "write" : "read");
	} else {
		pr_info("strange read/write violation (%u)\n",
				vio_is_write);
	}

	if (!from_emimpu) {
		pr_info("%s transaction\n",
				(vio.is_ns) ? "non-secure" : "secure");
	}

	return 0;
}
EXPORT_SYMBOL(devmpu_print_violation);

/* sysfs */
static int devmpu_rw_perm_get(uint64_t pa, size_t *rd_perm, size_t *wr_perm)
{
	size_t ret;

	if (unlikely(
			pa < DEVMPU_DRAM_BASE
		||	pa >= DEVMPU_DRAM_BASE + DEVMPU_DRAM_SIZE)) {
		pr_err("%s:%d invalid DRAM physical address, pa=0x%llx\n",
				__func__, __LINE__, pa);
		return -1;
	}

	if (unlikely(rd_perm == NULL || wr_perm == NULL)) {
		pr_err("%s:%d output pointer is NULL\n",
				__func__, __LINE__);
		return -1;
	}

	ret = mt_secure_call_ret3(MTK_SIP_KERNEL_DEVMPU_PERM_GET,
			pa, 0, 0, 0, rd_perm, wr_perm);
	if (ret == (size_t)(-1)) {
		pr_err("%s:%d failed to get permission, SMC ret=%zd\n",
				__func__, __LINE__, ret);
		return -1;
	}

	return 0;
}

static ssize_t devmpu_show(struct device_driver *driver, char *buf)
{
	ssize_t ret = 0;

	uint32_t i;
	uint64_t pa;

	size_t rd_perm = 0xffffffff;
	size_t wr_perm = 0xffffffff;

	uint8_t rd_perm_bmp[16];
	uint8_t wr_perm_bmp[16];

	pr_info("Page#  RD/WR permissions\n");

	for (i = 0; i < DEVMPU_PAGE_NUM; ++i) {

		if (i && i % 16 == 0) {
			pr_info("%04x:  %08x/%08x %08x/%08x %08x/%08x %08x/%08x\n",
				i - 16,
				*((uint32_t *)rd_perm_bmp),
				*((uint32_t *)wr_perm_bmp),
				*((uint32_t *)rd_perm_bmp+1),
				*((uint32_t *)wr_perm_bmp+1),
				*((uint32_t *)rd_perm_bmp+2),
				*((uint32_t *)wr_perm_bmp+2),
				*((uint32_t *)rd_perm_bmp+3),
				*((uint32_t *)wr_perm_bmp+3));
		}

		pa = DEVMPU_DRAM_BASE + (i * DEVMPU_PAGE_SIZE);
		if (devmpu_rw_perm_get(pa, &rd_perm, &wr_perm)) {
			pr_err("%s:%d failed to get permission\n",
					__func__, __LINE__);
			return -1;
		}

		rd_perm_bmp[i % 16] = (uint8_t)rd_perm;
		wr_perm_bmp[i % 16] = (uint8_t)wr_perm;
	}

	return ret;
}

static ssize_t devmpu_store(struct device_driver *driver,
		const char *buf, size_t count)
{
	return count;
}
DRIVER_ATTR(devmpu_config, 0444, devmpu_show, devmpu_store);

/* driver registration */
static int devmpu_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;

	pr_info("%s:%d module probe\n", __func__, __LINE__);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	DEVMPU_BASE = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(DEVMPU_BASE)) {
		pr_err("%s:%d unable to map DEVMPU_BASE\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	return ret;
}

static const struct of_device_id devmpu_of_match[] = {
	{ .compatible = "mediatek,infra_device_mpu" },
	{},
};

static struct platform_driver devmpu_drv = {
	.probe = devmpu_probe,
	.driver = {
		.name = "devmpu",
		.owner = THIS_MODULE,
		.of_match_table = devmpu_of_match,
	},
};

static int __init devmpu_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&devmpu_drv);
	if (ret) {
		pr_err("%s:%d failed to register devmpu driver, ret=%d\n",
				__func__, __LINE__);
	}

#if !defined(USER_BUILD_KERNEL)
	ret = driver_create_file(&devmpu_drv.driver,
			&driver_attr_devmpu_config);
	if (ret) {
		pr_err("%s:%d failed to create driver sysfs file, ret=%d\n",
				__func__, __LINE__);
	}
#endif

	return ret;
}

static void __exit devmpu_exit(void)
{
}

postcore_initcall(devmpu_init);
module_exit(devmpu_exit);
