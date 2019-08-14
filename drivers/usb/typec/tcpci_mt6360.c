// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/usb/tcpm.h>
#include <linux/regmap.h>
#include "tcpci.h"

#include <linux/mfd/mt6360.h>

#define MT6360_TCPC_VID		(0x29cf)
#define MT6360_TCPC_PID		(0x6360)

#define MT6360_REG_VCONNCTRL1	(0x8C)
#define MT6360_REG_MODECTRL2	(0x8F)
#define MT6360_REG_SWRESET	(0xA0)
#define MT6360_REG_DEBCTRL1	(0xA1)
#define MT6360_REG_DRPCTRL1	(0xA2)
#define MT6360_REG_DRPCTRL2	(0xA3)
#define MT6360_REG_I2CTORST	(0xBF)
#define MT6360_REG_RXCTRL2	(0xCF)
#define MT6360_REG_CTDCTRL2	(0xEC)

/* MT6360_REG_VCONNCTRL1 */
#define MT6360_VCONNCL_ENABLE	BIT(0)
/* MT6360_REG_RXCTRL2 */
#define MT6360_OPEN40M_ENABLE	BIT(7)
/* MT6360_REG_CTDCTRL2 */
#define MT6360_RPONESHOT_ENABLE	BIT(6)

struct mt6360_tcpc_info {
	struct tcpci_data data;
	struct tcpci *tcpci;
	struct i2c_client *i2c;
	struct device *dev;
	int irq;
	struct device_connection dev_conn;
};

static inline int mt6360_tcpc_read16(
		       struct mt6360_tcpc_info *mti, unsigned int reg, u16 *val)
{
	return regmap_raw_read(mti->data.regmap, reg, val, sizeof(*val));
}

static inline int mt6360_tcpc_write16(
		       struct mt6360_tcpc_info *mti, unsigned int reg, u16 val)
{
	return regmap_raw_write(mti->data.regmap, reg, &val, sizeof(val));
}

static const struct regmap_config mt6360_tcpc_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF, /* 0x80 .. 0xFF are vendor defined */
};

static int mt6360_tcpc_init(struct tcpci *tcpci, struct tcpci_data *data)
{
	struct mt6360_tcpc_info *mti = (void *)data;
	int ret;

	dev_dbg(mti->dev, "%s\n", __func__);
	ret = regmap_write(data->regmap, MT6360_REG_SWRESET, 0x01);
	if (ret < 0)
		return ret;
	/* after reset command, wait 1~2ms to wait IC action */
	usleep_range(1000, 2000);
	/* write all alert to masked */
	ret = mt6360_tcpc_write16(mti, TCPC_ALERT_MASK, 0);
	if (ret < 0)
		return ret;
	/* config I2C timeout reset enable , and timeout to 200ms */
	ret = regmap_write(data->regmap, MT6360_REG_I2CTORST, 0x8f);
	if (ret < 0)
		return ret;
	/* config CC Detect Debounce : 26.7*val us */
	ret = regmap_write(data->regmap, MT6360_REG_DEBCTRL1, 0x10);
	if (ret < 0)
		return ret;
	/* DRP Toggle Cycle : 51.2 + 6.4*val ms */
	ret = regmap_write(data->regmap, MT6360_REG_DRPCTRL1, 4);
	if (ret < 0)
		return ret;
	/* DRP Duyt Ctrl : dcSRC: /1024 */
	ret = mt6360_tcpc_write16(mti, MT6360_REG_DRPCTRL2, 330);
	if (ret < 0)
		return ret;
	/* Enable VCONN Current Limit function */
	ret = regmap_update_bits(data->regmap, MT6360_REG_VCONNCTRL1,
				 MT6360_VCONNCL_ENABLE, MT6360_VCONNCL_ENABLE);
	if (ret < 0)
		return ret;
	/* Enable cc open 40ms when pmic send vsysuv signal */
	ret = regmap_update_bits(data->regmap, MT6360_REG_RXCTRL2,
				 MT6360_OPEN40M_ENABLE, MT6360_OPEN40M_ENABLE);
	if (ret < 0)
		return ret;
	/* Enable Rpdet oneshot detection */
	ret = regmap_update_bits(data->regmap,
				 MT6360_REG_CTDCTRL2, MT6360_RPONESHOT_ENABLE,
				 MT6360_RPONESHOT_ENABLE);
	if (ret < 0)
		return ret;
	/* Set shipping mode off, AUTOIDLE on */
	return regmap_write(data->regmap, MT6360_REG_MODECTRL2, 0x7A);
}

static irqreturn_t mt6360_irq(int irq, void *dev_id)
{
	struct mt6360_tcpc_info *mti = dev_id;

	return tcpci_irq(mti->tcpci);
}

static int mt6360_check_tcpc_id(struct mt6360_tcpc_info *mti)
{
	u16 vid, pid;
	int ret;

	ret = mt6360_tcpc_read16(mti, TCPC_VENDOR_ID, &vid);
	if (ret < 0)
		return ret;
	ret = mt6360_tcpc_read16(mti, TCPC_PRODUCT_ID, &pid);
	if (ret < 0)
		return ret;
	if (vid != MT6360_TCPC_VID || pid != MT6360_TCPC_PID) {
		dev_err(mti->dev, "[vid:pid] -> [0x%04x:0x%04x]\n", vid, pid);
		return -ENODEV;
	}
	return 0;
}

static int mt6360_tcpc_probe(struct platform_device *pdev)
{
	struct mt6360_pmu_info *pmu_info = dev_get_drvdata(pdev->dev.parent);
	struct mt6360_tcpc_info *mti;
	const char *dev_conn_end;
	int ret;

	mti = devm_kzalloc(&pdev->dev, sizeof(*mti), GFP_KERNEL);
	if (!mti)
		return -ENOMEM;
	mti->i2c = pmu_info->i2c[MT6360_SLAVE_TCPC];
	mti->dev = &pdev->dev;
	platform_set_drvdata(pdev, mti);

	/* get alert irqno */
	mti->irq = platform_get_irq_byname(pdev, "PD_IRQB");
	if (mti->irq < 0)
		return mti->irq;
	/* regmap register */
	mti->data.regmap = devm_regmap_init_i2c(mti->i2c,
					   &mt6360_tcpc_regmap_config);
	if (IS_ERR(mti->data.regmap)) {
		dev_err(&pdev->dev, "Failed to initialize regmap\n");
		return PTR_ERR(mti->data.regmap);
	}
	/* tcpc id check */
	ret = mt6360_check_tcpc_id(mti);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to check TCPC id\n");
		return ret;
	}
	/* get dev-conn-end for usb role switch */
	ret = of_property_read_string(pdev->dev.of_node, "dev-conn-end",
					   &dev_conn_end);
	if (!ret) {
		mti->dev_conn.endpoint[0] = dev_conn_end;
		mti->dev_conn.endpoint[1] = dev_name(mti->dev);
		mti->dev_conn.id = "usb-role-switch";
		device_connection_add(&mti->dev_conn);
	}
	/* register tcpc port */
	mti->data.init = mt6360_tcpc_init;
	mti->tcpci = tcpci_register_port(&pdev->dev, &mti->data);
	if (IS_ERR_OR_NULL(mti->tcpci)) {
		dev_err(&pdev->dev, "Failed to register tcpci port\n");
		ret = PTR_ERR(mti->tcpci);
		goto port_err;
	}
	/* after all are registered, reguest irq for alert pin */
	ret = devm_request_threaded_irq(mti->dev, mti->irq, NULL, mt6360_irq,
					IRQF_ONESHOT | IRQF_TRIGGER_LOW,
					dev_name(mti->dev), mti);
	if (ret < 0) {
		dev_err(mti->dev, "Failed to request irq\n");
		goto irq_err;
	}
	device_init_wakeup(mti->dev, true);
	dev_info(&pdev->dev, "Successfully probed\n");
	return 0;

irq_err:
	tcpci_unregister_port(mti->tcpci);
port_err:
	if (mti->dev_conn.id)
		device_connection_remove(&mti->dev_conn);
	return ret;
}

static int mt6360_tcpc_remove(struct platform_device *pdev)
{
	struct mt6360_tcpc_info *mti = platform_get_drvdata(pdev);

	tcpci_unregister_port(mti->tcpci);
	if (mti->dev_conn.id)
		device_connection_remove(&mti->dev_conn);
	return 0;
}

static int __maybe_unused mt6360_tcpc_suspend(struct device *dev)
{
	struct mt6360_tcpc_info *mti = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(mti->irq);
	disable_irq(mti->irq);
	return 0;
}

static int __maybe_unused mt6360_tcpc_resume(struct device *dev)
{
	struct mt6360_tcpc_info *mti = dev_get_drvdata(dev);

	enable_irq(mti->irq);
	if (device_may_wakeup(dev))
		disable_irq_wake(mti->irq);
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6360_tcpc_pm_ops,
			 mt6360_tcpc_suspend, mt6360_tcpc_resume);

static const struct platform_device_id mt6360_tcpc_id[] = {
	{ "mt6360_tcpc", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, mt6360_tcpc_id);

static const struct of_device_id mt6360_tcpc_of_id[] = {
	{ .compatible = "mediatek,mt6360_tcpc", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6360_tcpc_of_id);

static struct platform_driver mt6360_tcpc_driver = {
	.driver = {
		.name = "mt6360_tcpc",
		.owner = THIS_MODULE,
		.pm = &mt6360_tcpc_pm_ops,
		.of_match_table = of_match_ptr(mt6360_tcpc_of_id),
	},
	.probe = mt6360_tcpc_probe,
	.remove = mt6360_tcpc_remove,
	.id_table = mt6360_tcpc_id,
};
module_platform_driver(mt6360_tcpc_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 USB Type-C Port Controller Interface Driver");
MODULE_VERSION("1.0.0");
MODULE_LICENSE("GPL");
