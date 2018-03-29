/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author:
 *  Chunfeng Yun <chunfeng.yun@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/usb/phy.h>
#include <linux/usb/xhci_pdriver.h>

#include "xhci.h"
#include "xhci-mtk.h"

/* ip_pw_ctrl0 register */
#define CTRL0_IP_SW_RST	BIT(0)

/* ip_pw_ctrl1 register */
#define CTRL1_IP_HOST_PDN	BIT(0)

/* ip_pw_ctrl2 register */
#define CTRL2_IP_DEV_PDN	BIT(0)

/* ip_pw_sts1 register */
#define STS1_IP_SLEEP_STS	BIT(30)
#define STS1_U3_MAC_RST	BIT(16)
#define STS1_SYS125_RST	BIT(10)
#define STS1_REF_RST		BIT(8)
#define STS1_SYSPLL_STABLE	BIT(0)

/* ip_pw_sts2 register */
#define STS2_U2_MAC_RST	BIT(0)

/* ip_xhci_cap register */
#define CAP_U3_PORT_NUM(p)	((p) & 0xff)
#define CAP_U2_PORT_NUM(p)	(((p) >> 8) & 0xff)

/* u3_ctrl_p register */
#define CTRL_U3_PORT_HOST_SEL	BIT(2)
#define CTRL_U3_PORT_PDN	BIT(1)
#define CTRL_U3_PORT_DIS	BIT(0)

/* u2_ctrl_p register */
#define CTRL_U2_PORT_HOST_SEL	BIT(2)
#define CTRL_U2_PORT_PDN	BIT(1)
#define CTRL_U2_PORT_DIS	BIT(0)

/* u2_phy_pll register */
#define CTRL_U2_FORCE_PLL_STB	BIT(28)

static int check_ip_clk_status(struct xhci_hcd_mtk *mtk)
{
	struct mu3c_ippc_regs __iomem *ippc = mtk->ippc_regs;
	u32 val, check_val;
	int ret;

	check_val = STS1_SYSPLL_STABLE | STS1_REF_RST | STS1_SYS125_RST;
	check_val |= (mtk->num_u3_ports ? STS1_U3_MAC_RST : 0);

	ret = readl_poll_timeout(&ippc->ip_pw_sts1, val,
			  (check_val == (val & check_val)), 100, 20000);
	if (ret) {
		dev_err(mtk->dev, "mac3 clocks are not stable (0x%x)\n", val);
		return ret;
	}

	ret = readl_poll_timeout(&ippc->ip_pw_sts2, val,
			   (val & STS2_U2_MAC_RST), 100, 10000);
	if (ret) {
		dev_err(mtk->dev, "mac2 reset is still active!!!\n");
		return ret;
	}

	return 0;
}

static int xhci_mtk_ports_enable(struct xhci_hcd_mtk *mtk)
{
	struct mu3c_ippc_regs __iomem *ippc = mtk->ippc_regs;
	u32 temp;
	int i;

	/* power on host ip */
	temp = readl(&ippc->ip_pw_ctr1);
	temp &= ~CTRL1_IP_HOST_PDN;
	writel(temp, &ippc->ip_pw_ctr1);

	/* power on and enable all u3 ports */
	for (i = 0; i < mtk->num_u3_ports; i++) {
		temp = readl(&ippc->u3_ctrl_p[i]);
		temp &= ~(CTRL_U3_PORT_PDN | CTRL_U3_PORT_DIS);
		temp |= CTRL_U3_PORT_HOST_SEL;
		writel(temp, &ippc->u3_ctrl_p[i]);
	}

	/* power on and enable all u2 ports */
	for (i = 0; i < mtk->num_u2_ports; i++) {
		temp = readl(&ippc->u2_ctrl_p[i]);
		temp &= ~(CTRL_U2_PORT_PDN | CTRL_U2_PORT_DIS);
		temp |= CTRL_U2_PORT_HOST_SEL;
		writel(temp, &ippc->u2_ctrl_p[i]);
	}

	/*
	 * wait for clocks to be stable, and clock domains reset to
	 * be inactive after power on and enable ports
	 */
	return check_ip_clk_status(mtk);
}

static int xhci_mtk_ports_disable(struct xhci_hcd_mtk *mtk)
{
	struct mu3c_ippc_regs __iomem *ippc = mtk->ippc_regs;
	u32 temp;
	int ret;
	int i;

	/* power down all u3 ports */
	for (i = 0; i < mtk->num_u3_ports; i++) {
		temp = readl(&ippc->u3_ctrl_p[i]);
		temp |= CTRL_U3_PORT_PDN;
		writel(temp, &ippc->u3_ctrl_p[i]);
	}

	/* power down all u2 ports */
	for (i = 0; i < mtk->num_u2_ports; i++) {
		temp = readl(&ippc->u2_ctrl_p[i]);
		temp |= CTRL_U2_PORT_PDN;
		writel(temp, &ippc->u2_ctrl_p[i]);
	}

	/* power down host ip */
	temp = readl(&ippc->ip_pw_ctr1);
	temp |= CTRL1_IP_HOST_PDN;
	writel(temp, &ippc->ip_pw_ctr1);

	ret = readl_poll_timeout(&ippc->ip_pw_sts1, temp,
			  (temp & STS1_IP_SLEEP_STS), 100, 100000);
	if (ret) {
		dev_err(mtk->dev, "ip sleep failed!!!\n");
		return ret;
	}
	return 0;
}

static void xhci_mtk_ports_config(struct xhci_hcd_mtk *mtk)
{
	struct mu3c_ippc_regs __iomem *ippc = mtk->ippc_regs;
	u32 temp;

	/* reset whole ip */
	temp = readl(&ippc->ip_pw_ctr0);
	temp |= CTRL0_IP_SW_RST;
	writel(temp, &ippc->ip_pw_ctr0);
	udelay(1);
	temp = readl(&ippc->ip_pw_ctr0);
	temp &= ~CTRL0_IP_SW_RST;
	writel(temp, &ippc->ip_pw_ctr0);

	/*
	 * device ip is default power-on in fact
	 * power down device ip, otherwise ip-sleep will fail
	 */
	temp = readl(&ippc->ip_pw_ctr2);
	temp |= CTRL2_IP_DEV_PDN;
	writel(temp, &ippc->ip_pw_ctr2);

	temp = readl(&ippc->ip_xhci_cap);
	mtk->num_u3_ports = CAP_U3_PORT_NUM(temp);
	mtk->num_u2_ports = CAP_U2_PORT_NUM(temp);
	dev_dbg(mtk->dev, "%s u2p:%d, u3p:%d\n", __func__,
			mtk->num_u2_ports, mtk->num_u3_ports);

	xhci_mtk_ports_enable(mtk);
}

static int xhci_mtk_clks_enable(struct xhci_hcd_mtk *mtk)
{
	int ret;

	ret = clk_prepare_enable(mtk->sys_clk);
	if (ret) {
		dev_err(mtk->dev, "failed to enable sys_clk\n");
		return ret;
	}
	ret = clk_prepare_enable(mtk->ethif);
	if (ret) {
		dev_err(mtk->dev, "failed to enable ethif\n");
		clk_disable_unprepare(mtk->sys_clk);
		return ret;
	}
	return 0;
}

static void xhci_mtk_clks_disable(struct xhci_hcd_mtk *mtk)
{
	clk_disable_unprepare(mtk->ethif);
	clk_disable_unprepare(mtk->sys_clk);
}

static int xhci_mtk_setup(struct usb_hcd *hcd);
static const struct xhci_driver_overrides xhci_mtk_overrides __initconst = {
	.extra_priv_size = sizeof(struct xhci_hcd),
	.reset = xhci_mtk_setup,
};

static struct hc_driver __read_mostly xhci_mtk_hc_driver;

static int xhci_mtk_phy_init(struct xhci_hcd_mtk *mtk)
{
	int i;
	int ret;

	for (i = 0; i < mtk->num_phys; i++) {
		ret = phy_init(mtk->phys[i]);
		if (ret)
			goto exit_phy;
	}
	return 0;

exit_phy:
	for (; i > 0; i--)
		phy_exit(mtk->phys[i - 1]);

	return ret;
}

static int xhci_mtk_phy_exit(struct xhci_hcd_mtk *mtk)
{
	int i;

	for (i = 0; i < mtk->num_phys; i++)
		phy_exit(mtk->phys[i]);

	return 0;
}

static int xhci_mtk_phy_power_on(struct xhci_hcd_mtk *mtk)
{
	int i;
	int ret;

	for (i = 0; i < mtk->num_phys; i++) {
		ret = phy_power_on(mtk->phys[i]);
		if (ret)
			goto power_off_phy;
	}
	return 0;

power_off_phy:
	for (; i > 0; i--)
		phy_power_off(mtk->phys[i - 1]);

	return ret;
}

static void xhci_mtk_phy_power_off(struct xhci_hcd_mtk *mtk)
{
	unsigned int i;

	for (i = 0; i < mtk->num_phys; i++)
		phy_power_off(mtk->phys[i]);
}

static int xhci_mtk_ldos_enable(struct xhci_hcd_mtk *mtk)
{
	int ret;

	ret = regulator_enable(mtk->vbus);
	if (ret) {
		dev_err(mtk->dev, "failed to enable vbus\n");
		return ret;
	}

	ret = regulator_enable(mtk->vusb33);
	if (ret) {
		dev_err(mtk->dev, "failed to enable vusb33\n");
		regulator_disable(mtk->vbus);
		return ret;
	}
	return 0;
}

static void xhci_mtk_ldos_disable(struct xhci_hcd_mtk *mtk)
{
	regulator_disable(mtk->vbus);
	regulator_disable(mtk->vusb33);
}

static void xhci_mtk_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	/*
	 * As of now platform drivers don't provide MSI support so we ensure
	 * here that the generic code does not try to make a pci_dev from our
	 * dev struct in order to setup MSI
	 */
	xhci->quirks |= XHCI_PLAT;
	xhci->quirks |= XHCI_MTK_HOST;
	/*
	 * MTK host controller gives a spurious successful event after a
	 * short transfer. Ignore it.
	 */
	xhci->quirks |= XHCI_SPURIOUS_SUCCESS;
}

/* called during probe() after chip reset completes */
static int xhci_mtk_setup(struct usb_hcd *hcd)
{
	struct xhci_hcd *xhci;
	int ret;

	ret = xhci_gen_setup(hcd, xhci_mtk_quirks);
	if (ret)
		return ret;

	if (!usb_hcd_is_primary_hcd(hcd))
		return 0;

	xhci = hcd_to_xhci(hcd);
	return xhci_mtk_sch_init(xhci);
}


static int xhci_mtk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct usb_xhci_pdata *pdata = dev_get_platdata(dev);
	struct xhci_hcd_mtk *mtk;
	const struct hc_driver *driver;
	struct xhci_hcd *xhci;
	struct resource *res;
	struct usb_hcd *hcd;
	struct phy *phy;
	int phy_num;
	int ret = -ENODEV;
	int irq;

	if (usb_disabled())
		return -ENODEV;

	driver = &xhci_mtk_hc_driver;
	mtk = devm_kzalloc(dev, sizeof(*mtk), GFP_KERNEL);
	if (!mtk)
		return -ENOMEM;

	mtk->dev = dev;
	mtk->vbus = devm_regulator_get(dev, "vbus");
	if (IS_ERR(mtk->vbus)) {
		dev_err(dev, "fail to get vbus\n");
		return PTR_ERR(mtk->vbus);
	}

	mtk->vusb33 = devm_regulator_get(dev, "vusb33");
	if (IS_ERR(mtk->vusb33)) {
		dev_err(dev, "fail to get vusb33\n");
		return PTR_ERR(mtk->vusb33);
	}

	mtk->sys_clk = devm_clk_get(dev, "sys_ck");
	if (IS_ERR(mtk->sys_clk)) {
		dev_err(dev, "fail to get sys_ck\n");
		return PTR_ERR(mtk->sys_clk);
	}

	mtk->ethif = devm_clk_get(dev, "ethif");
	if (IS_ERR(mtk->ethif)) {
		dev_err(dev, "fail to get ethif\n");
		return PTR_ERR(mtk->ethif);
	}

	mtk->num_phys = of_count_phandle_with_args(node,
			"phys", "#phy-cells");
	if (mtk->num_phys > 0) {
		mtk->phys = devm_kcalloc(dev, mtk->num_phys,
					sizeof(*mtk->phys), GFP_KERNEL);
		if (!mtk->phys)
			return -ENOMEM;
	} else {
		mtk->num_phys = 0;
	}
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	ret = xhci_mtk_ldos_enable(mtk);
	if (ret)
		goto disable_pm;

	ret = xhci_mtk_clks_enable(mtk);
	if (ret)
		goto disable_ldos;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		goto disable_clk;

	/* Initialize dma_mask and coherent_dma_mask to 32-bits */
	ret = dma_set_coherent_mask(dev, DMA_BIT_MASK(32));
	if (ret)
		goto disable_clk;

	if (!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;
	else
		dma_set_mask(dev, DMA_BIT_MASK(32));

	hcd = usb_create_hcd(driver, dev, dev_name(dev));
	if (!hcd) {
		ret = -ENOMEM;
		goto disable_clk;
	}

	/*
	 * USB 2.0 roothub is stored in the platform_device.
	 * Swap it with mtk HCD.
	 */
	mtk->hcd = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, mtk);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hcd->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(hcd->regs)) {
		ret = PTR_ERR(hcd->regs);
		goto put_usb2_hcd;
	}
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	mtk->ippc_regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(mtk->ippc_regs)) {
		ret = PTR_ERR(mtk->ippc_regs);
		goto put_usb2_hcd;
	}

	for (phy_num = 0; phy_num < mtk->num_phys; phy_num++) {
		phy = devm_of_phy_get_by_index(dev, node, phy_num);
		if (IS_ERR(phy)) {
			ret = PTR_ERR(phy);
			goto put_usb2_hcd;
		}
		mtk->phys[phy_num] = phy;
	}

	xhci_mtk_ports_config(mtk);
	ret = xhci_mtk_phy_init(mtk);
	if (ret)
		goto put_usb2_hcd;

	ret = xhci_mtk_phy_power_on(mtk);
	if (ret)
		goto exit_phys;

	device_init_wakeup(dev, 1);

	xhci = hcd_to_xhci(hcd);
	xhci->main_hcd = hcd;
	xhci->shared_hcd = usb_create_shared_hcd(driver, dev,
			dev_name(dev), hcd);
	if (!xhci->shared_hcd) {
		ret = -ENOMEM;
		goto power_off_phys;
	}

	if ((node && of_property_read_bool(node, "usb3-lpm-capable")) ||
			(pdata && pdata->usb3_lpm_capable))
		xhci->quirks |= XHCI_LPM_SUPPORT;

	if (HCC_MAX_PSA(xhci->hcc_params) >= 4)
		xhci->shared_hcd->can_do_streams = 1;

	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret)
		goto put_usb3_hcd;

	ret = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
	if (ret)
		goto dealloc_usb2_hcd;

	return 0;

dealloc_usb2_hcd:
	xhci_mtk_sch_exit(xhci);
	usb_remove_hcd(hcd);

put_usb3_hcd:
	usb_put_hcd(xhci->shared_hcd);

power_off_phys:
	xhci_mtk_phy_power_off(mtk);
	device_init_wakeup(dev, 0);

exit_phys:
	xhci_mtk_phy_exit(mtk);

put_usb2_hcd:
	usb_put_hcd(hcd);

disable_clk:
	xhci_mtk_clks_disable(mtk);

disable_ldos:
	xhci_mtk_ldos_disable(mtk);

disable_pm:
	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);
	return ret;
}

static int xhci_mtk_remove(struct platform_device *dev)
{
	struct xhci_hcd_mtk *mtk = platform_get_drvdata(dev);
	struct usb_hcd	*hcd = mtk->hcd;
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);

	usb_remove_hcd(xhci->shared_hcd);
	xhci_mtk_phy_power_off(mtk);
	xhci_mtk_phy_exit(mtk);
	device_init_wakeup(&dev->dev, 0);

	usb_remove_hcd(hcd);
	usb_put_hcd(xhci->shared_hcd);
	usb_put_hcd(hcd);
	xhci_mtk_sch_exit(xhci);
	xhci_mtk_clks_disable(mtk);
	xhci_mtk_ldos_disable(mtk);
	pm_runtime_put_sync(&dev->dev);
	pm_runtime_disable(&dev->dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int xhci_mtk_suspend(struct device *dev)
{
	struct xhci_hcd_mtk *mtk = dev_get_drvdata(dev);

	xhci_mtk_ports_disable(mtk);
	xhci_mtk_phy_power_off(mtk);
	xhci_mtk_clks_disable(mtk);
	return 0;
}

static int xhci_mtk_resume(struct device *dev)
{
	struct xhci_hcd_mtk *mtk = dev_get_drvdata(dev);

	xhci_mtk_clks_enable(mtk);
	xhci_mtk_phy_power_on(mtk);
	xhci_mtk_ports_enable(mtk);
	return 0;
}

static const struct dev_pm_ops xhci_mtk_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(xhci_mtk_suspend, xhci_mtk_resume)
};
#define DEV_PM_OPS	(&xhci_mtk_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_OF
static const struct of_device_id mtk_xhci_of_match[] = {
	{ .compatible = "mediatek,mt2701-xhci"},
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_xhci_of_match);
#endif

static struct platform_driver mtk_xhci_driver = {
	.probe	= xhci_mtk_probe,
	.remove	= xhci_mtk_remove,
	.driver	= {
		.name = "xhci-mtk",
		.pm = DEV_PM_OPS,
		.of_match_table = of_match_ptr(mtk_xhci_of_match),
	},
};

static int __init xhci_mtk_init(void)
{
	xhci_init_driver(&xhci_mtk_hc_driver, &xhci_mtk_overrides);
	return platform_driver_register(&mtk_xhci_driver);
}
module_init(xhci_mtk_init);

static void __exit xhci_mtk_exit(void)
{
	platform_driver_unregister(&mtk_xhci_driver);
}
module_exit(xhci_mtk_exit);

MODULE_DESCRIPTION("MediaTek xHCI Host Controller Driver");
MODULE_LICENSE("GPL v2");
