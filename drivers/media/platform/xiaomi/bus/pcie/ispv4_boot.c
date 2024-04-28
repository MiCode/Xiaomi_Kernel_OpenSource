/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mfd/core.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include "ispv4_regops.h"
#include "ispv4_boot.h"
#include <linux/delay.h>
#include <linux/component.h>
#include <linux/mfd/ispv4_defs.h>
#include <media/ispv4_defs.h>
#include <linux/debugfs.h>
#include "ispv4_pcie.h"
#include "ispv4_ctrl_ext.h"

extern struct platform_device *ispv4_ctrl_pdev;
static bool pmic_exist = true;
static bool pci_err    = false;
struct dentry *ispv4_debugfs;
EXPORT_SYMBOL_GPL(ispv4_debugfs);

struct ispv4_spi_dev {
	struct device comp_dev;
	struct spi_device *spi_dev;
};

static char *mbox_itf_type;
static char *rproc_itf_type;
static bool unuse_pci = false;

module_param_named(mbox, mbox_itf_type, charp, 0644);
module_param_named(rproc, rproc_itf_type, charp, 0644);
module_param_named(unuse_pci, unuse_pci, bool, 0644);

static struct resource ispv4_mailbox_res[MAILBOX_RES_NUM];
static struct resource ispv4_busmon_res;

static struct mfd_cell ispv4_man_cell = {
	.name = "ispv4-manager-pci",
	.ignore_resource_conflicts = true,
};

static struct mfd_cell ispv4_man_cell_spi = {
	.name = "ispv4-manager-spi",
	.ignore_resource_conflicts = true,
};

// TODO: change mailbox id table
static struct mfd_cell ispv4_mailbox_cell = {
	.name = "xm-ispv4-mbox",
	.num_resources = ARRAY_SIZE(ispv4_mailbox_res),
	.resources = ispv4_mailbox_res,
	.ignore_resource_conflicts = true,
};

static struct mfd_cell ispv4_regops_cell = {
	.name = "xm-ispv4-regops",
	.num_resources = 0,
};

static struct mfd_cell ispv4_busmon_cell = {
	.name = "xm-ispv4-busmon",
	.num_resources = 1,
	.resources = &ispv4_busmon_res,
	.ignore_resource_conflicts = true,
};

static struct mfd_cell ispv4_rproc_spi_cell = {
	.name = "xm-ispv4-rproc-spi",
	.num_resources = 0,
};

/* clang-format off */
static const struct of_device_id ispv4_spi_of_match[] = {
	{
		.compatible = "xiaomi,ispv4_spi",
	},
	{},
};

static const struct spi_device_id ispv4_spi_device_id[] = {
	{ "ispv4_spi", 0 },
	{}
};
/* clang-format on */

static inline void _pci_sel_softreset(bool s)
{
	u32 reg = s ? 1 : 0;
	putreg32(reg, 0xcc001dc);
}

static inline void _pci_soft_preset_reset(void)
{
	/* Reset */
	putreg32(0, 0xcc001e0);
	usleep_range(1000, 1005);
}

static inline void _pci_soft_preset_release(void)
{
	/* Release */
	putreg32(1, 0xcc001e0);
	usleep_range(1000, 1005);
}

void _pci_linkdown_event(void)
{
extern void ispv4_force_prest(void);
extern void ispv4_release_prest(void);
	ispv4_force_prest();
}

void _pci_reset(void)
{
	_pci_sel_softreset(true);
	_pci_soft_preset_reset(); /* Reset */
	_pci_soft_preset_release(); /* Release */
	pr_info("ispv4 pci reset finish\n");
}
EXPORT_SYMBOL_GPL(_pci_reset);

static inline void _pci_enable_dbi(bool s)
{
	uint32_t val;

	if (s)
		pr_info("ispv4 pci enable dbi...\n");
	else
		pr_info("ispv4 pci disable dbi...\n");

	val = getreg32(MISC_CONTROL_1);
	if (s)
		val |= DBI_RO_WR_EN;
	else
		val &= ~DBI_RO_WR_EN;
	putreg32(val, MISC_CONTROL_1);
}

static inline uint32_t ispv4_pcie_iatu_reg_read(struct iatu *iatu, uint32_t reg_addr)
{
	if (!iatu)
		return PTR_ERR(iatu);

	return readl_relaxed(iatu->base + reg_addr);
}

static inline void ispv4_pcie_iatu_reg_write(struct iatu *iatu, uint32_t reg_addr, uint32_t val)
{
	if (!iatu)
		return;

	writel_relaxed(val, iatu->base + reg_addr);
}

__maybe_unused int _pci_config_iatu_fast(struct ispv4_data *priv)
{
	uint32_t val;
	struct iatu *iatu;

	iatu = devm_kzalloc(&priv->pci->dev, sizeof(struct iatu),
				   GFP_KERNEL);
	if (!iatu)
		return -ENOMEM;
	iatu->base = priv->base_bar[3] + 0x4000;

	//pr_info("ispv4 pci config iatu...\n");
	//REGION=0 DIR=IN
	ispv4_pcie_iatu_reg_write(iatu, PCIE_IATU_LWR_TARGET_ADDR(0, 1),0);
	ispv4_pcie_iatu_reg_write(iatu, PCIE_IATU_UPPER_TARGET_ADDR(0, 1), 0);
	ispv4_pcie_iatu_reg_write(iatu, PCIE_IATU_REGION_CTRL_1(0, 1), 0);
	val = IATU_REGION_EN;
	val |= IATU_BAR_MARCH_MODE;
	// IRAM use BAR0
	val |= (0 << IATU_BAR_NUM_SHIFT) & IATU_BAR_NUM_MASK;
	ispv4_pcie_iatu_reg_write(iatu, PCIE_IATU_REGION_CTRL_2(0, 1), val);

	// REGION=2 DIR=IN
	ispv4_pcie_iatu_reg_write(iatu, PCIE_IATU_LWR_TARGET_ADDR(1, 1),0x80000000);
	ispv4_pcie_iatu_reg_write(iatu, PCIE_IATU_UPPER_TARGET_ADDR(1, 1), 0);
	ispv4_pcie_iatu_reg_write(iatu, PCIE_IATU_REGION_CTRL_1(1, 1), 0);
	val = IATU_REGION_EN;
	val |= IATU_BAR_MARCH_MODE;
	// DDR use BAR2
	val |= (2 << IATU_BAR_NUM_SHIFT) & IATU_BAR_NUM_MASK;
	ispv4_pcie_iatu_reg_write(iatu, PCIE_IATU_REGION_CTRL_2(1, 1), val);

	// REGION=3 DIR=IN
	ispv4_pcie_iatu_reg_write(iatu, PCIE_IATU_LWR_TARGET_ADDR(2, 1),0x00000000);
	ispv4_pcie_iatu_reg_write(iatu, PCIE_IATU_UPPER_TARGET_ADDR(2, 1), 0);
	ispv4_pcie_iatu_reg_write(iatu, PCIE_IATU_REGION_CTRL_1(2, 1), 0);
	val = IATU_REGION_EN;
	val |= IATU_BAR_MARCH_MODE;
	val |= (4 << IATU_BAR_NUM_SHIFT) & IATU_BAR_NUM_MASK;
	ispv4_pcie_iatu_reg_write(iatu, PCIE_IATU_REGION_CTRL_2(2, 1), val);

	// // REGION=4 DIR=IN
	// TO INTC
	// putreg32(0x0D420000, IATU_LWR_TARGET_ADDR(3, 1));
	// putreg32(0, IATU_UPPER_TARGET_ADDR(3, 1));
	// putreg32(0, IATU_REGION_CTRL_1(3, 1));
	// val = IATU_REGION_EN;
	// val |= IATU_BAR_MARCH_MODE;
	// val |= (1 << IATU_BAR_NUM_SHIFT) & IATU_BAR_NUM_MASK;
	// putreg32(val, IATU_REGION_CTRL_2(3, 1));

	pr_info("ispv4 pci config iatu finish\n");
	return 0;
}

__maybe_unused static void _pci_config_iatu(void)
{
	uint32_t val;

	pr_info("ispv4 pci config iatu...\n");

	// REGION=0 DIR=IN
	putreg32(0, IATU_LWR_TARGET_ADDR(0, 1));
	putreg32(0, IATU_UPPER_TARGET_ADDR(0, 1));
	putreg32(0, IATU_REGION_CTRL_1(0, 1));
	val = IATU_REGION_EN;
	val |= IATU_BAR_MARCH_MODE;
	// IRAM use BAR0
	val |= (0 << IATU_BAR_NUM_SHIFT) & IATU_BAR_NUM_MASK;
	putreg32(val, IATU_REGION_CTRL_2(0, 1));

	// REGION=2 DIR=IN
	putreg32(0x80000000, IATU_LWR_TARGET_ADDR(1, 1));
	putreg32(0, IATU_UPPER_TARGET_ADDR(1, 1));
	putreg32(0, IATU_REGION_CTRL_1(1, 1));
	val = IATU_REGION_EN;
	val |= IATU_BAR_MARCH_MODE;
	// DDR use BAR2
	val |= (2 << IATU_BAR_NUM_SHIFT) & IATU_BAR_NUM_MASK;
	putreg32(val, IATU_REGION_CTRL_2(1, 1));

	// // REGION=3 DIR=IN
	putreg32(0x00000000, IATU_LWR_TARGET_ADDR(2, 1));
	putreg32(0, IATU_UPPER_TARGET_ADDR(2, 1));
	putreg32(0, IATU_REGION_CTRL_1(2, 1));
	val = IATU_REGION_EN;
	val |= IATU_BAR_MARCH_MODE;
	val |= (4 << IATU_BAR_NUM_SHIFT) & IATU_BAR_NUM_MASK;
	putreg32(val, IATU_REGION_CTRL_2(2, 1));

	// // REGION=4 DIR=IN
	// TO INTC
	// putreg32(0x0D420000, IATU_LWR_TARGET_ADDR(3, 1));
	// putreg32(0, IATU_UPPER_TARGET_ADDR(3, 1));
	// putreg32(0, IATU_REGION_CTRL_1(3, 1));
	// val = IATU_REGION_EN;
	// val |= IATU_BAR_MARCH_MODE;
	// val |= (1 << IATU_BAR_NUM_SHIFT) & IATU_BAR_NUM_MASK;
	// putreg32(val, IATU_REGION_CTRL_2(3, 1));

	pr_info("ispv4 pci config iatu finish\n");
}

static void _pci_config_bar(void)
{
	uint32_t bar_addr;
	uint32_t bar_mask_addr;
	uint32_t bar_flags;
	//pr_info("ispv4 pci config bar...\n");
	bar_addr = BAR_REG(0);
	bar_mask_addr = BAR_MASK_REG(0);
	bar_flags = PCI_BAR_FLAG_MEM64;
	putreg32(0xfffffff, bar_mask_addr);
	putreg32(bar_flags, bar_addr);
	bar_addr = BAR_REG(2);
	bar_mask_addr = BAR_MASK_REG(2);
	bar_flags = PCI_BAR_FLAG_MEM64 | PCI_BAR_FLAG_PREFETCH;
	putreg32(0x3ffffff, bar_mask_addr);
	putreg32(bar_flags, bar_addr);
	bar_addr = BAR_REG(4);
	bar_mask_addr = BAR_MASK_REG(4);
	bar_flags = PCI_BAR_FLAG_MEM;
	putreg32(0x3ffffff, bar_mask_addr);
	putreg32(bar_flags, bar_addr);
	pr_info("ispv4 pci config bar finish\n");
}

static void _pci_config_pm(void)
{
	uint32_t val;

	//pr_info("ispv4 pci config pm...\n");
	val = getreg32(0x0cc0001c);
	// Fix l2/l3 PME reply delay.
	val |= BIT(4);
	// Enable l1ss
	val &= ~BIT(0);
	putreg32(val, 0x0cc0001c);
	val = getreg32(0x0cc000e0);
	// Remove reference clk
	val |= BIT(0);
	putreg32(val, 0x0cc000e0);
	// Fix 100ms auto exit l1ss bug.
	val = getreg32(0x0cd00890);
	val &= ~BIT(0);
	putreg32(val, 0x0cd00890);
	// Change l1 entry latency to 32us
	val = getreg32(0x0cd0070c);
	val &= ~L1_ENTRY_LAGENCY;
	val |= FIELD_PREP(L1_ENTRY_LAGENCY, 5);
	putreg32(val, 0x0cd0070c);
	pr_info("ispv4 pci config pm finish\n");
}

__maybe_unused static int ispv4_pcie_phy_update(u32 *phydata, u32 len)
{
	int ret;
	int i;

	if (len > PCIE_PHY_SRAM_SIZE / sizeof(int)) {
		pr_err("ispv4 fw len(%d (sizeof(in))) is larger than sram size\n",
		       len);
		return -1;
	}

	_pci_soft_preset_reset(); /* Reset */

	/*app_itssm_enable is ctrled by sw*/
	clear_set_reg32(1, ITSSM_SW_CTRL_EN_MASK, PCIE_CTRL_REG);
	/*pcie phy is init mode*/
	clear_set_reg32(0, PHY_MISC_CFG_MASK, PCIE_PHY_REG);
	/*cr para clk into 88M*/
	clear_set_reg32(1, SLV_TOP_CLK_CORE_PHY_PCIE_CON_MASK, TOP_CLK_CORE);
	/*pipe_lane perst_n is ctrled by sw*/
	clear_set_reg32(PHY_RESET_OVERD_EN_VAL, PHY_RESET_OVERD_EN_MASK,
			PCIE_PHY_REG0);
	/*pipe_lane perst_n is set*/
	clear_set_reg32(PHY_RESET_OVERD_EN_VAL, PHY_RESET_OVERD_EN_MASK,
			PCIE_PHY_REG1);
	/*polling sram already loaded fw*/
	ret = polling_reg32(PHY_MISC_RPT_VAL, PHY_MISC_RPT_MASK, PCIE_PHY_REG2);
	if (ret)
		return -1;

	/*write phy fw data to interal sram*/
	for (i = 0; i < len; i++) {
		putreg32(*phydata++, PCIE_PHY_SRAM + 4 * (i++));
	}
	/*sram load finish*/
	clear_set_reg32(1, PHY_MISC_CFG2_MASK, PCIE_PHY_REG3);

	_pci_soft_preset_release(); /* Release */

	/*waitinf for phy initing*/
	ret = polling_reg32(PCIE_PHY_STATUS_VAL, PCIE_PHY_STATUS_MASK,
			    PCIE_CTRL_REG1);
	if (ret)
		return -1;
	/*enable pcie linkup*/
	clear_set_reg32(1, SSI_GENERGEL_CORE_CTRL_MASK, PCIE_CTRL_REG2);

	/*Attention: any need to dealy to wait for linkup or irq*/

	return 0;
}

int ispv4_boot_preconfig_pci(void)
{
	_pci_reset();
	_pci_config_pm();
	return 0;
}

int ispv4_boot_config_pci(void)
{
	_pci_enable_dbi(true);

	pr_info("ispv4 boot set interrupt line = 0\n");
	putreg32(0, (PF0_TYPE0_HDR_OFF + 0x3C));

	_pci_config_bar();
	// _pci_config_iatu();
	_pci_enable_dbi(false);

	return 0;
}

int ispv4_resume_config_pci(void)
{
	_pci_config_pm();

	pr_info("ispv4 boot set interrupt line = 0\n");
	putreg32(0, (PF0_TYPE0_HDR_OFF + 0x3C));

	_pci_config_bar();
	// _pci_config_iatu();
	return 0;
}

static int repops_probe_test(void)
{
	int tmp = 0;
	return ispv4_regops_read(0, &tmp);
}

static int spi_speed(void *priv, u32 speed)
{
	struct spi_device *spi = priv;
	spi->max_speed_hz = speed;
	spi_setup(spi);
	return 0;
}

static void spi_gettick(void *priv)
{
	struct spi_device *spi = priv;
	uint32_t tmp = 0;
	ispv4_regops_read(0x8FFFFFFC, &tmp);
	dev_info(&spi->dev, "ispv4 tick: 0x%x!!\n", tmp);
}

static int ispv4_comp_bind(struct device *comp, struct device *master,
			   void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	struct ispv4_spi_dev *sdev;

	sdev = container_of(comp, typeof(*sdev), comp_dev);
	priv->v4l2_spi.spi = sdev;
	priv->v4l2_spi.spidev = sdev->spi_dev;
	priv->v4l2_spi.spi_speed = spi_speed;
	priv->v4l2_spi.spi_gettick = spi_gettick;
	priv->v4l2_spi.avalid = true;
	dev_info(comp, "avalid!!\n");
	return 0;
}

static void ispv4_comp_unbind(struct device *comp, struct device *master,
			      void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	priv->v4l2_spi.avalid = false;
}

__maybe_unused static const struct component_ops comp_ops = {
	.bind = ispv4_comp_bind,
	.unbind = ispv4_comp_unbind
};

#define SPI_TEST_MAGIC 0xdefaabbc
#define SPI_DEFER_MAGIC 0x1

static int ispv4_spi_add_man(struct spi_device *spi, bool with_pci)
{
	int ret;
	struct device *dev = &spi->dev;
	struct mfd_cell *c = with_pci ? &ispv4_man_cell : &ispv4_man_cell_spi;

	ret = mfd_add_devices(dev, PLATFORM_DEVID_NONE, c, 1, NULL, 0, NULL);
	if (ret != 0) {
		dev_err(dev, "ispv4 boot add cam-dev failed (%d)!\n", ret);
	}
	return ret;
}

static int ispv4_spi_init(struct spi_device *spi, struct ispv4_spi_dev **sdevp)
{
	int ret = 0;
	struct ispv4_spi_dev *sdev;
	struct device *dev = &spi->dev;
	ispv4_debugfs = debugfs_create_dir("xm_ispv4", NULL);

	spi->max_speed_hz = 1000000;
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;

	sdev = kzalloc(sizeof(*sdev), GFP_KERNEL);
	if (sdev == NULL)
		return -ENOMEM;

	*sdevp = sdev;
	sdev->spi_dev = spi;
	spi_set_drvdata(spi, sdev);
	device_initialize(&sdev->comp_dev);
	dev_set_name(&sdev->comp_dev, "ispv4_boot");

	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(dev, "spi_setup failed (%d)!\n", ret);
		goto out;
	}

	ret = mfd_add_devices(dev, PLATFORM_DEVID_NONE, &ispv4_regops_cell, 1,
			      NULL, 0, NULL);
	if (ret != 0) {
		dev_err(dev, "ispv4 boot add regops failed (%d)!\n", ret);
		goto regops_failed;
	}

	return 0;

regops_failed:
	/* NOTE: must not remove cam-dev. Ensure camera is ok */
out:
	return ret;
}

static int ispv4_chip_boot(void)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_MIISP_CHIP)
	ret = ispv4_power_on_pmic(ispv4_ctrl_pdev);
	if (ret != 0) {
		pmic_exist = false;
		dev_err(&ispv4_ctrl_pdev->dev, "ispv4_power_on_cpu with pmic fail, ret = %d\n", ret);
		return ret;
	}
	ret = ispv4_power_on_chip(ispv4_ctrl_pdev);
	if (ret != 0) {
		dev_err(&ispv4_ctrl_pdev->dev, "ispv4_power_on_cpu with cpu fail, ret = %d\n", ret);
		return ret;
	}
	dev_info(&ispv4_ctrl_pdev->dev, "ispv4 power on pmic and release cpu done!\n");
#else
	ispv4_fpga_reset(ispv4_ctrl_pdev);
#endif

	return ret;
}

#define PROBE_DO_ONCE(func, ...)                                                                 \
	({                                                                                       \
		int __ret = 0;                                                                   \
		do {                                                                             \
			static int __done = 0;                                                   \
			if (__done == 0) {                                                       \
				__ret = func(__VA_ARGS__);                                       \
				if (__ret != 0) {                                                \
					pr_err("ispv4 boot: %s ret err %d, goto success_exit\n", \
					       #func, __ret);                                    \
					goto success_exit;                                       \
				}                                                                \
				__done = 1;                                                      \
			} else {                                                                 \
				pr_crit("%s has done, skip...\n", #func);                        \
			}                                                                        \
		} while (0);                                                                     \
		__ret;                                                                           \
	})

/* Hack from qcom */
extern int ispv4_disable_wakeup(void);

static ssize_t ispv4_exist_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
        return sysfs_emit(buf, "%d\n", pmic_exist);
}

static ssize_t ispv4_err_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
        return sysfs_emit(buf, "%d\n", pci_err);
}

static ssize_t ispv4_err_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,size_t count)
{
	int val;
	int ret;

	ret = kstrtoint(buf, count, &val);
	if(ret < 0)
		return ret;

	if(val > 0)
		pci_err = true;
	else
		pci_err = false;

    return count;
}

static DEVICE_ATTR_RO(ispv4_exist);
static DEVICE_ATTR_RW(ispv4_err);

static struct attribute *ispv4_attrs[] = {
	&dev_attr_ispv4_exist.attr,
	&dev_attr_ispv4_err.attr,
	NULL,
};

static const struct attribute_group ispv4_attr_group = {
        .attrs = ispv4_attrs,
};

static int ispv4_sysfs_register(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	int ret = 0;

	struct kobject *ispv4_kobj = &spi->dev.kobj;
	ret = sysfs_create_link(kernel_kobj, ispv4_kobj, "ispv4");
	if (ret) {
	        return ret;
	}
	ret = sysfs_create_group(ispv4_kobj, &ispv4_attr_group);
	if (ret) {
	        return ret;
	}
	dev_info(dev, "%s, create attribute success", __func__);

	return 0;
}

static void ispv4_sysfs_unregister(struct spi_device *spi)
{
	struct kobject *ispv4_kobj = &spi->dev.kobj;

	sysfs_remove_group(ispv4_kobj, &ispv4_attr_group);
	sysfs_remove_link(kernel_kobj, "ispv4");
}

static int ispv4_spi_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	int ret = 0;
	int boot_flag = 0;
	static struct ispv4_spi_dev *sdev = NULL;
	bool with_pci = false;
	struct ispv4_ctrl_data *ctrl_priv;

	pr_info("Into %s\n", __FUNCTION__);

	PROBE_DO_ONCE(ispv4_spi_init, spi, &sdev);

	if (ispv4_ctrl_pdev == NULL)
		return -EPROBE_DEFER;

	PROBE_DO_ONCE(ispv4_chip_boot);

	spi_set_drvdata(spi, sdev);

	ret = repops_probe_test();
	if (ret == -ENODEV) {
		goto defer_exit;
	} else if (ret != 0) {
		dev_err(dev, "booting time regops try failed %d.\n", ret);
		/*
		 * SPI transfer met error.
		 * We need keep qcom-camera-dev, so exit success.
		 * Should not continue, because next ops need spi.
		 */
		goto success_exit;
	}

	if (mbox_itf_type && strncmp("spi", mbox_itf_type, 5) == 0) {
		boot_flag |= BOOT_MB_BY_SPI;
	} else {
		boot_flag |= BOOT_MB_BY_PCI;
		dev_info(dev, "ispv4 boot add mbox will using pci!\n");
	}

	if (rproc_itf_type && strncmp("spi", rproc_itf_type, 5) == 0) {
		boot_flag |= BOOT_RP_BY_SPI;
	} else {
		boot_flag |= BOOT_RP_BY_PCI;
		dev_info(dev, "ispv4 boot add rproc will using pci!\n");
	}

#if IS_ENABLED(CONFIG_MIISP_CHIP)
	/* Chip change to a high rate. */
	PROBE_DO_ONCE(ispv4_power_on_sequence_preconfig, ispv4_ctrl_pdev);
	dev_info(dev, "ispv4 power on preconfig done!\n");
#endif

	if (!unuse_pci) {
		PROBE_DO_ONCE(ispv4_disable_wakeup);
		PROBE_DO_ONCE(ispv4_boot_preconfig_pci);
	}

	if (!unuse_pci) {
		ret = ispv4_pci_init(boot_flag);
		if (ret == -EPROBE_DEFER) {
			dev_err(dev, "pci rc not ready!\n");
			goto defer_exit;
		}
		else if(ret != 0){
			pci_err = true;
			dev_err(dev, "pci not ready , ret %d\n",ret);
		}
		else{
			with_pci = true;
		}
	} else
		boot_flag = BOOT_MB_BY_SPI | BOOT_RP_BY_SPI;

		/* NOTE: will not probe-defer, so pci_init need not do once */

#if defined(PCI_FAILED_USE_SPI)
	if (!unuse_pci && ret != 0) {
		dev_err(dev, "ispv4 boot pci failed!\n");
		boot_flag = BOOT_MB_BY_SPI | BOOT_RP_BY_SPI;
	}
#else
	if (!unuse_pci && ret != 0) {
		dev_err(dev, "ispv4 boot pci meet error %d. return!\n", ret);
		goto success_exit;
	}
#endif

	ctrl_priv = platform_get_drvdata(ispv4_ctrl_pdev);

	if ((boot_flag & BOOT_MB_BY_SPI) != 0) {
		dev_info(dev, "ispv4 boot add mbox by spi!\n");
		ispv4_mailbox_res[MAILBOX_REG].name = "spi";
		ispv4_mailbox_res[MAILBOX_REG].flags = IORESOURCE_MEM;
		ispv4_mailbox_res[MAILBOX_IRQ].start =
			ctrl_priv->irq_info.gpio_irq[ISPV4_MBOX_IRQ];
		ispv4_mailbox_res[MAILBOX_IRQ].end =
			ctrl_priv->irq_info.gpio_irq[ISPV4_MBOX_IRQ];
		ispv4_mailbox_res[MAILBOX_IRQ].flags = IORESOURCE_IRQ;

		ret = mfd_add_devices(dev, PLATFORM_DEVID_NONE,
				      &ispv4_mailbox_cell, 1, NULL, 0, NULL);
		if (ret != 0)
			dev_err(dev, "ispv4 boot add mbox failed (%d)!\n", ret);
	}

	if ((boot_flag & BOOT_RP_BY_SPI) != 0) {
		dev_info(dev, "ispv4 boot add rproc by spi!\n");
		ret = mfd_add_devices(dev, PLATFORM_DEVID_NONE,
				      &ispv4_rproc_spi_cell, 1, NULL, 0, NULL);
		if (ret != 0)
			dev_err(dev, "ispv4 boot add rproc failed (%d)!\n",
				ret);
	}

	ispv4_busmon_res.start = ctrl_priv->irq_info.gpio_irq[ISPV4_BUSMON_IRQ];
	ispv4_busmon_res.end = ctrl_priv->irq_info.gpio_irq[ISPV4_BUSMON_IRQ];
	ispv4_busmon_res.flags = IORESOURCE_IRQ;

	ret = mfd_add_devices(dev, PLATFORM_DEVID_NONE, &ispv4_busmon_cell, 1,
			      NULL, 0, NULL);
	if (ret != 0) {
		dev_err(dev, "ispv4 boot add busmon failed (%d)!\n", ret);
	}

	pr_err("comp add %s! sdev = %x, comp_name = %s\n", __FUNCTION__, sdev,
	       dev_name(&sdev->comp_dev));
	ret = component_add(&sdev->comp_dev, &comp_ops);
	if (ret != 0) {
		dev_err(dev, "register ispv4_boot component failed %d.\n", ret);
		goto success_exit;
	}

success_exit:
	(void)ispv4_sysfs_register(spi);

	(void)ispv4_spi_add_man(spi, with_pci);
	if (ret != 0)
		dev_err(dev, "Probe met error %d, probe not finish.\n", ret);
	return 0;
defer_exit:
	return -EPROBE_DEFER;
}

static void ispv4_spi_remove(struct spi_device *spi)
{
	struct ispv4_spi_dev *sdev;
	sdev = spi_get_drvdata(spi);

	pr_alert("ispv4 remove components %s\n", dev_name(&sdev->comp_dev));
	debugfs_remove(ispv4_debugfs);
	ispv4_debugfs = NULL;
	component_del(&sdev->comp_dev, &comp_ops);
	ispv4_pci_exit();
	mfd_remove_devices(&spi->dev);
	ispv4_sysfs_unregister(spi);
	kfree(sdev);
}

static struct spi_driver ispv4_spi_drv = {
	.driver = {
		.name = "ispv4_spi",
		.owner = THIS_MODULE,
		// .probe_type = PROBE_FORCE_SYNCHRONOUS,
		.of_match_table = ispv4_spi_of_match,
	},
	.probe = ispv4_spi_probe,
	.remove = ispv4_spi_remove,
	.id_table = ispv4_spi_device_id,
};

int __init ispv4_boot_init(void)
{
	return spi_register_driver(&ispv4_spi_drv);
}

void __exit ispv4_boot_exit(void)
{
	spi_unregister_driver(&ispv4_spi_drv);
}

module_init(ispv4_boot_init);
module_exit(ispv4_boot_exit);
