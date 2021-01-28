// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <asm/page.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/device.h>
#include <linux/compiler.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <memory/mediatek/emi.h>
#include <devmpu.h>
#include <devmpu_emi.h>

#define LOG_TAG "[DEVMPU]"

#define DEVMPU_MAX_TAG_LEN 15
#define DUMP_TAG  "dump_devmpu"
#define CHECK_TAG "violate_devmpu"

#define DEVMPU_MAX_CTX 3
#define DEVMPU_DEFAULT_CTX 0

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) LOG_TAG " " fmt

#define switchValue(x) (((x << 24) & 0xff000000) | \
			((x << 8) & 0x00ff0000)  | \
			((x >> 8) & 0x0000ff00)  | \
			((x >> 24) & 0x000000ff))

struct devmpu_context {

	/* HW register mapped base */
	void __iomem *reg_base;

	/* DRAM (PA) space protected */
	uint64_t prot_base;
	uint64_t prot_size;

	/* page granularity */
	uint32_t page_size;

	/* virtual irq number */
	uint32_t virq;

	/* max ctx */
	uint32_t max_ctx;
} devmpu_ctx_ary[DEVMPU_MAX_CTX];

struct devmpu_vio_stat {

	/* master ID */
	uint16_t id;

	/* master domain */
	uint8_t domain;

	/* is NS transaction (AxPROT[1]) */
	bool is_ns;

	/* is write violation */
	bool is_write;

	/* padding */
	uint8_t padding[3];

	/* physical address */
	uint64_t addr;
};

static int devmpu_vio_get(struct devmpu_vio_stat *vio, bool do_clear)
{
	struct arm_smccc_res res;

	size_t vio_addr;
	size_t vio_info;

	if (unlikely(vio == NULL)) {
		pr_err("%s:%d output pointer is NULL\n",
				__func__, __LINE__);
		return -1;
	}


	arm_smccc_smc(MTK_SIP_KERNEL_DEVMPU_VIO_GET,
			do_clear, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0) {
		pr_err("%s:%d failed to get violation, ret=0x%lx\n",
				__func__, __LINE__, res.a0);
		return -1;
	}

	vio_addr = res.a1;
	vio_info = res.a2;

	vio->addr = vio_addr;
	vio->is_write = (vio_info >> 0) & 0x1;
	vio->is_ns = (vio_info >> 1) & 0x1;
	vio->domain = (vio_info >> 8) & 0xFF;
	vio->id = (vio_info >> 16) & 0xFFFF;

	return 0;
}

void devmpu_vio_clear(unsigned int emi_id)
{
	struct arm_smccc_res smc_res;

	/* clear hyp violation status */
	arm_smccc_smc(MTK_SIP_KERNEL_DEVMPU_VIO_CLR,
			emi_id, 0, 0, 0, 0, 0, 0, &smc_res);

	if (smc_res.a0) {
		pr_err("%s:%d failed to clear violation, ret=0x%lx\n",
				__func__, __LINE__, smc_res.a0);
	}
}

static int devmpu_rw_perm_get(uint64_t pa, size_t *rd_perm, size_t *wr_perm)
{
	struct arm_smccc_res res;
	struct devmpu_context *devmpu_ctx;

	devmpu_ctx = &devmpu_ctx_ary[DEVMPU_DEFAULT_CTX];

	if (unlikely(
			pa < devmpu_ctx->prot_base
		||	pa >= devmpu_ctx->prot_base + devmpu_ctx->prot_size)) {
		pr_err("%s:%d invalid DRAM physical address, pa=0x%llx\n",
				__func__, __LINE__, pa);
		return -1;
	}

	if (unlikely(rd_perm == NULL || wr_perm == NULL)) {
		pr_err("%s:%d output pointer is NULL\n",
				__func__, __LINE__);
		return -1;
	}

	arm_smccc_smc(MTK_SIP_KERNEL_DEVMPU_PERM_GET,
			pa, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0) {
		pr_err("%s:%d failed to get permission, ret=0x%lx\n",
				__func__, __LINE__, res.a0);
		return -1;
	}

	*rd_perm = (size_t)res.a1;
	*wr_perm = (size_t)res.a2;

	return 0;
}

int devmpu_print_violation(uint64_t vio_addr, uint32_t vio_id,
		uint32_t vio_domain, uint32_t vio_rw, bool from_emimpu)
{
	size_t ret;
	struct devmpu_context *devmpu_ctx;
	struct devmpu_vio_stat vio = {
		.id = 0,
		.domain = 0,
		.is_ns = false,
		.is_write = false,
		.addr = 0x0ULL
	};

	uint32_t vio_axi_id;
	uint32_t vio_port_id;
	uint32_t page;
	size_t rd_perm;
	size_t wr_perm;

	devmpu_ctx = &devmpu_ctx_ary[DEVMPU_DEFAULT_CTX];

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

		/*
		 * use 0b01/0b10 to specify write/read violation
		 * to be consistent with EMI MPU violation handling
		 */
		vio_rw = (vio.is_write) ? 1 : 2;

#ifdef CONFIG_MTK_ENABLE_GENIEZONE
		if (vio_rw == 2 && vio_domain == 0)
			return 0;
#endif
	}

	vio_addr += devmpu_ctx->prot_base;

	vio_axi_id = (vio_id >> 3) & 0x1FFF;
	vio_port_id = vio_id & 0x7;

	pr_info("Device MPU violation\n");
	pr_info("current process is \"%s \" (pid: %i)\n",
			current->comm, current->pid);
	pr_info("corrupted address is 0x%llx\n",
			vio_addr);
	pr_info("master ID: 0x%x, AXI ID: 0x%x, port ID: 0x%x\n",
			vio_id, vio_axi_id, vio_port_id);
	pr_info("violation domain 0x%x\n",
			vio_domain);

	if (vio_rw == 1)
		pr_info("write violation\n");
	else if (vio_rw == 2)
		pr_info("read violation\n");
	else
		pr_info("strange read/write violation (%u)\n", vio_rw);

	if (!devmpu_rw_perm_get(vio_addr, &rd_perm, &wr_perm)) {
		page = vio_addr - devmpu_ctx->prot_base;
		page /= devmpu_ctx->page_size;
		pr_info("Page#%x RD/WR : %08zx/%08zx (%lld)\n",
			page,
			switchValue(rd_perm),
			switchValue(wr_perm),
			(vio_addr / devmpu_ctx->page_size) % 4);
	}

	if (!from_emimpu) {
		pr_info("%s transaction\n",
				(vio.is_ns) ? "non-secure" : "secure");
	}

	return 0;
}
EXPORT_SYMBOL(devmpu_print_violation);

/* sysfs */
static int devmpu_dump_perm(void)
{
	struct devmpu_context *devmpu_ctx;

	uint32_t i;

	uint64_t pa, pa_dump;
	uint32_t pages;

	size_t rd_perm;
	size_t wr_perm;

	uint8_t rd_perm_bmp[16];
	uint8_t wr_perm_bmp[16];

	devmpu_ctx = &devmpu_ctx_ary[DEVMPU_DEFAULT_CTX];

	pa = devmpu_ctx->prot_base;
	pages = devmpu_ctx->prot_size / devmpu_ctx->page_size;

	pr_info("Page# (bus-addr)  :  RD/WR permissions\n");

	for (i = 0; i < pages; ++i) {
		if (i && i % 16 == 0) {
			pa_dump = (uint64_t)(i - 16) * devmpu_ctx->page_size;
			pa_dump += devmpu_ctx->prot_base;
			pr_info("%04x (%02x_%08x):  %08x/%08x %08x/%08x %08x/%08x %08x/%08x\n",
				i - 16,
				(uint32_t)(pa_dump >> 32),
				(uint32_t)(pa_dump & 0xffffffff),
				*((uint32_t *)rd_perm_bmp),
				*((uint32_t *)wr_perm_bmp),
				*((uint32_t *)rd_perm_bmp+1),
				*((uint32_t *)wr_perm_bmp+1),
				*((uint32_t *)rd_perm_bmp+2),
				*((uint32_t *)wr_perm_bmp+2),
				*((uint32_t *)rd_perm_bmp+3),
				*((uint32_t *)wr_perm_bmp+3));
		}

		if (devmpu_rw_perm_get(pa, &rd_perm, &wr_perm)) {
			pr_err("%s:%d failed to get permission\n",
					__func__, __LINE__);
			return -EINVAL;
		}

		rd_perm_bmp[i % 16] = (uint8_t)rd_perm;
		wr_perm_bmp[i % 16] = (uint8_t)wr_perm;

		pa += devmpu_ctx->page_size;
	}

	return 0;
}

static int devmpu_check_violation(void)
{
	struct devmpu_context *devmpu_ctx;
	struct device_node *dn;
	void __iomem *reg_base;
	uint32_t prop_ary[4];
	uint32_t prop_addr;
	uint32_t prop_size;
	uint64_t prop_value;

	devmpu_ctx = &devmpu_ctx_ary[DEVMPU_DEFAULT_CTX];

	/* Get property from dts */
	dn = of_find_compatible_node(NULL, NULL, "mediatek,atf-reserved");
	if (!dn) {
		pr_err("%s:%d failed to get device node\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	if (of_property_read_u32_array(dn, "reg", prop_ary, 4)) {
		pr_err("%s:%d failed to get property\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	prop_addr = prop_ary[1];
	prop_size = prop_ary[3];

	pr_info("Address 0x%x Size 0x%x\n", prop_addr, prop_size);

	/* Check if 2MB addr/size alignment */
	if (prop_addr % devmpu_ctx->page_size) {
		pr_err("%s:%d address is not 2MB alignment 0x%x\n",
				__func__, __LINE__, prop_addr);
		return -EINVAL;
	}

	if (prop_size % devmpu_ctx->page_size) {
		pr_err("%s:%d size is not 2MB alignment 0x%x\n",
				__func__, __LINE__, prop_size);
		return -EINVAL;
	}

	pr_info("Check 2MB address/size alignment correctly\n");

	/* Trigger DevMPU violation */
	if (prop_addr && prop_size) {
		pr_info("Check if DevMPU violation is at 0x%x\n", prop_addr);
		reg_base = ioremap((phys_addr_t)prop_addr, prop_size);
		pr_info("Read from %p\n", reg_base);
		prop_value = *(uint64_t *)reg_base;
		pr_info("value 0x%llx\n", prop_value);
		pr_info("Write to %p\n", reg_base);
		*(uint64_t *)reg_base = prop_value;
	}

	return 0;
}

static ssize_t devmpu_config_show(struct device_driver *driver, char *buf)
{
	return 0;
}

static ssize_t devmpu_config_store(struct device_driver *driver,
	const char *buf, size_t count)
{
	int ret = 0;

	if (strlen(buf) > DEVMPU_MAX_TAG_LEN) {
		pr_notice("%s: command overflow\n", __func__);
		return -EINVAL;
	}

	if (!strncmp(buf, DUMP_TAG, strlen(DUMP_TAG))) {
		ret = devmpu_dump_perm();
	}	else if (!strncmp(buf, CHECK_TAG, strlen(CHECK_TAG))) {
		ret = devmpu_check_violation();
	} else {
		pr_notice("%s Invalid argument!!\n", __func__);
		return -EINVAL;
	}

	if (ret) {
		pr_notice("%s verificaiton fail!!\n", __func__);
		return -EINVAL;
	}

	return count;
}
static DRIVER_ATTR_RW(devmpu_config);

static irqreturn_t devmpu_irq_handler(int irq, void *dev_id)
{
	devmpu_print_violation(0, 0, 0, 0, false);
	return IRQ_HANDLED;
}

/* driver registration */
static int devmpu_probe(struct platform_device *pdev)
{
	int rc;

	void __iomem *reg_base;
	static uint32_t probe_cnt;
	struct devmpu_context *devmpu_ctx = NULL;
	uint64_t prot_base;
	uint64_t prot_size;
	uint32_t page_size;
	uint32_t virq;

	struct device_node *dn = pdev->dev.of_node;
	struct resource *res = NULL;

	pr_info("Device MPU probe: %s\n", pdev->name);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("%s:%d failed to get resource\n",
				__func__, __LINE__);
		return -ENOENT;
	}

	reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(reg_base)) {
		pr_err("%s:%d unable to map DEVMPU_BASE\n",
				__func__, __LINE__);
		return -ENOENT;
	}

	if (of_property_read_u64(dn, "prot-base", &prot_base)) {
		pr_err("%s:%d failed to get protected region base\n",
				__func__, __LINE__);
		return -ENOENT;
	}

	if (of_property_read_u64(dn, "prot-size", &prot_size)) {
		pr_err("%s:%d failed to get protected region size\n",
				__func__, __LINE__);
		return -ENOENT;
	}

	if (of_property_read_u32(dn, "page-size", &page_size)) {
		pr_err("%s:%d failed to get protected region granularity\n",
				__func__, __LINE__);
		return -ENOENT;
	}

	virq = irq_of_parse_and_map(dn, 0);
	rc = request_irq(virq, (irq_handler_t)devmpu_irq_handler,
			IRQF_TRIGGER_NONE, "devmpu", NULL);
	if (rc) {
		pr_err("%s:%d failed to request irq, rc=%d\n",
				__func__, __LINE__, rc);
		return -EPERM;
	}

	if ((probe_cnt + 1) > DEVMPU_MAX_CTX) {
		pr_err("%s:%d failed to create context\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	devmpu_ctx = &devmpu_ctx_ary[probe_cnt];

	devmpu_ctx->reg_base = reg_base;
	devmpu_ctx->prot_base = prot_base;
	devmpu_ctx->prot_size = prot_size;
	devmpu_ctx->page_size = page_size;
	devmpu_ctx->virq = virq;
	devmpu_ctx->max_ctx = (probe_cnt + 1);

	pr_info("Create context [%d]\n", probe_cnt);
	pr_info("reg_base=0x%pK\n", devmpu_ctx->reg_base);
	pr_info("prot_base=0x%llx\n", devmpu_ctx->prot_base);
	pr_info("prot_size=0x%llx\n", devmpu_ctx->prot_size);
	pr_info("page_size=0x%x\n", devmpu_ctx->page_size);
	pr_info("virq=0x%x\n", devmpu_ctx->virq);

	probe_cnt++;

#ifdef CONFIG_MTK_DEVMPU_EMI
	rc = devmpu_regist_emi();
	if (rc) {
		pr_err("%s:%d failed to request EMI callback, rc=%d\n",
				__func__, __LINE__, rc);
		return -EINVAL;
	}
#endif

	return 0;
}

static const struct of_device_id devmpu_of_match[] = {
	{ .compatible = "mediatek,device_mpu_low" },
	{ .compatible = "mediatek,device_mpu_sub" },
	{ .compatible = "mediatek,device_mpu_acp" },
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
				__func__, __LINE__, ret);
	}

#if !defined(USER_BUILD_KERNEL)
	ret = driver_create_file(&devmpu_drv.driver,
			&driver_attr_devmpu_config);
	if (ret) {
		pr_err("%s:%d failed to create driver sysfs file, ret=%d\n",
				__func__, __LINE__, ret);
	}
#endif

	return ret;
}

static void __exit devmpu_exit(void)
{
#if !defined(USER_BUILD_KERNEL)
	driver_remove_file(&devmpu_drv.driver,
			&driver_attr_devmpu_config);
#endif

	platform_driver_unregister(&devmpu_drv);
}

postcore_initcall(devmpu_init);
module_exit(devmpu_exit);

MODULE_DESCRIPTION("Mediatek Device MPU Driver");
MODULE_AUTHOR("Calvin Liao <calvin.liao@mediatek.com>");
MODULE_LICENSE("GPL");
