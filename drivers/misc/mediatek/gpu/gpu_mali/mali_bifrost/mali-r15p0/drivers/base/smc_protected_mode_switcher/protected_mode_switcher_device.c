/*
 *
 * (C) COPYRIGHT 2017 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/compiler.h>

#include <linux/protected_mode_switcher.h>

/*
 * Protected Mode Switch
 */

#define SMC_FAST_CALL  (1 << 31)
#define SMC_64         (1 << 30)
#define SMC_OEN_OFFSET 24
#define SMC_OEN_SIP    (2 << SMC_OEN_OFFSET)

struct smc_protected_mode_device {
	u16 smc_fid_enable;
	u16 smc_fid_disable;
	struct device *dev;
};

asmlinkage u64 __invoke_protected_mode_switch_smc(u64, u64, u64, u64);

static u64 invoke_smc(u32 oen, u16 function_number, bool smc64,
		u64 arg0, u64 arg1, u64 arg2)
{
	u32 fid = 0;

	fid |= SMC_FAST_CALL; /* Bit 31: Fast call */
	if (smc64)
		fid |= SMC_64; /* Bit 30: 1=SMC64, 0=SMC32 */
	fid |= oen; /* Bit 29:24: OEN */
	/* Bit 23:16: Must be zero for fast calls */
	fid |= (function_number); /* Bit 15:0: function number */

	return __invoke_protected_mode_switch_smc(fid, arg0, arg1, arg2);
}

static int protected_mode_enable(struct protected_mode_device *protected_dev)
{
	struct smc_protected_mode_device *sdev = protected_dev->data;

	if (!sdev)
		/* Not supported */
		return -EINVAL;

	return invoke_smc(SMC_OEN_SIP,
			sdev->smc_fid_enable, false,
			0, 0, 0);

}

static int protected_mode_disable(struct protected_mode_device *protected_dev)
{
	struct smc_protected_mode_device *sdev = protected_dev->data;

	if (!sdev)
		/* Not supported */
		return -EINVAL;

	return invoke_smc(SMC_OEN_SIP,
			sdev->smc_fid_disable, false,
			0, 0, 0);
}


static int protected_mode_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct protected_mode_device *protected_dev;
	struct smc_protected_mode_device *sdev;
	u32 tmp = 0;

	protected_dev = devm_kzalloc(&pdev->dev, sizeof(*protected_dev),
			GFP_KERNEL);
	if (!protected_dev)
		return -ENOMEM;

	sdev = devm_kzalloc(&pdev->dev, sizeof(*sdev), GFP_KERNEL);
	if (!sdev) {
		devm_kfree(&pdev->dev, protected_dev);
		return -ENOMEM;
	}

	protected_dev->data = sdev;
	protected_dev->ops.protected_mode_enable = protected_mode_enable;
	protected_dev->ops.protected_mode_disable = protected_mode_disable;
	sdev->dev = dev;

	if (!of_property_read_u32(dev->of_node, "arm,smc,protected_enable",
			&tmp))
		sdev->smc_fid_enable = tmp;

	if (!of_property_read_u32(dev->of_node, "arm,smc,protected_disable",
			&tmp))
		sdev->smc_fid_disable = tmp;

	/* Check older property names, for compatibility with outdated DTBs */
	if (!of_property_read_u32(dev->of_node, "arm,smc,secure_enable", &tmp))
		sdev->smc_fid_enable = tmp;

	if (!of_property_read_u32(dev->of_node, "arm,smc,secure_disable", &tmp))
		sdev->smc_fid_disable = tmp;

	platform_set_drvdata(pdev, protected_dev);

	dev_info(&pdev->dev, "Protected mode switcher %s loaded\n", pdev->name);
	dev_info(&pdev->dev, "SMC enable: 0x%x\n", sdev->smc_fid_enable);
	dev_info(&pdev->dev, "SMC disable: 0x%x\n", sdev->smc_fid_disable);

	return 0;
}

static int protected_mode_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Protected mode switcher %s removed\n",
			pdev->name);

	return 0;
}

static const struct of_device_id protected_mode_dt_ids[] = {
	{ .compatible = "arm,smc-protected-mode-switcher" },
	{ .compatible = "arm,secure-mode-switcher" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, protected_mode_dt_ids);

static struct platform_driver protected_mode_driver = {
	.probe = protected_mode_probe,
	.remove = protected_mode_remove,
	.driver = {
		.name = "smc-protected-mode-switcher",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(protected_mode_dt_ids),
	}
};

module_platform_driver(protected_mode_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ARM Ltd.");
MODULE_VERSION("1.0");
