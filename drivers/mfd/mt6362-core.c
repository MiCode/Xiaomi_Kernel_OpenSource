// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spmi.h>
#include <linux/regmap.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>

#include <dt-bindings/mfd/mt6362.h>

#define MT6362_IRQ_SET			(0x0D)
#define MT6362_REG_TM_PASCODE1		(0x07)
#define MT6362_REG_SPMIM_RCS1		(0x68)
#define MT6362_REG_SPMIM_RCS2		(0x69)
#define MT6362_CHG_IRQ0			(0xD0)
#define MT6362_PD_IRQ			(0xDF)
#define MT6362_IRQ_REGS			(MT6362_PD_IRQ - MT6362_CHG_IRQ0 + 1)
#define MT6362_CHG_MASK0		(0xF0)

#define MT6362_INT_RETRIG		BIT(2)

#define MT6362_REGMAP_IRQ_REG(_irq_evt) \
	REGMAP_IRQ_REG(_irq_evt, (_irq_evt) / 8, BIT((_irq_evt) % 8))

#define MT6362_SPMIMST_RCSCLR
#define MT6362_SPMIMST_STARTADDR	(0x10029000)
#define MT6362_SPMIMST_ENDADDR		(0x100290FF)
#define MT6362_REG_SPMIMST_RCSCLR	(0x28)
#define MT6362_MSK_SPMIMST_RCSCLR	(0xFF00)

struct mt6362_data {
	struct spmi_device *sdev;
	struct device *dev;
	struct regmap *regmap;
	struct regmap_irq_chip_data *irq_data;
	struct regmap_irq_chip irq_chip;
	unsigned int last_access_reg;
	ktime_t last_access_time;
	int irq;
#ifdef MT6362_SPMIMST_RCSCLR
	__iomem void *spmimst_base;
#endif /* MT6362_SPMIMST_RCSCLR */
};

struct init_table {
	u16 addr;
	u8 mask;
	u8 val;
	bool hidden_flag;
};

static const struct init_table mt6362_init_table[] = {
	/* PMIC PART */
	{0X120, 0X77, 0X55, false},
	{0X130, 0X77, 0X55, false},
	{0X140, 0X77, 0X22, false},
	/* BUCK PART */
	{0X21D, 0X77, 0X55, false},
	{0X221, 0X77, 0X55, false},
	{0X223, 0X77, 0X55, false},
	/* LDO PART */
	{0X310, 0X77, 0X11, false},
};

static const struct regmap_irq spmi_regmap_irqs[] = {
	MT6362_REGMAP_IRQ_REG(MT6362_FL_PWR_RDY),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_DETACH),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_RECHG),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_CHG_DONE),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_FL_BK_CHG),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_IEOC),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_CHG_RDY),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_VBUS_GD),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_VBUS_OV),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED2_STRB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED1_STRB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_CHG_BATOV),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_CHG_SYSOV),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_CHG_TOUT),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_CHG_BUSUV),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_CHG_THREG),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_CHG_AICR),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_CHG_MIVR),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_SYS_SHORT),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_SYS_MIN),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_AICC_DONE),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_PE_DONE),
	MT6362_REGMAP_IRQ_REG(MT6362_PP_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FT_DIG_THR),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_WDT),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_OTG_FAULT),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_OTG_BAT_LBP),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_OTG_CC),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_BC12_HVDCP),
	MT6362_REGMAP_IRQ_REG(MT6362_FL_BC12_DN),
	MT6362_REGMAP_IRQ_REG(MT6362_INT_CHRDET_UV),
	MT6362_REGMAP_IRQ_REG(MT6362_INT_CHRDET_OV),
	MT6362_REGMAP_IRQ_REG(MT6362_INT_CHRDET_EXT),
	MT6362_REGMAP_IRQ_REG(MT6362_ADC_DONEI),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED_STRBPIN_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED_TORPIN_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED_TX_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED_LVF_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED_LBP_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED_CHGVINOVP_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED2_SHORT_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED1_SHORT_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED2_STRB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED1_STRB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED2_STRB_TO_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED1_STRB_TO_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED2_TOR_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_FLED1_TOR_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_APWDTRST_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_EN_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_QONB_RST_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_MRSTB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_VDDAOV_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_SYSUV_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_OTP0_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_OTP1_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_OTP2_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_OTP3_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_OTP4_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_OTP5_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_BUCK1_OC_SDN_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_BUCK2_OC_SDN_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_BUCK3_OC_SDN_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_BUCK4_OC_SDN_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_BUCK5_OC_SDN_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_BUCK6_OC_SDN_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_BUCK1_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_BUCK2_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_BUCK3_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_BUCK4_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_BUCK5_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_BUCK6_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_LDO1_OC_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_LDO2_OC_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_LDO3_OC_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_LDO4_OC_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_LDO5_OC_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_LDO6_OC_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_LDO7_OC_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_VDIG18_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_LDO1_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_LDO2_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_LDO3_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_LDO4_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_LDO5_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_LDO6_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_LDO7_PGB_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_USBID_EVT),
	MT6362_REGMAP_IRQ_REG(MT6362_PD_EVT),
};

#ifdef MT6362_SPMIMST_RCSCLR
static inline void mt6362_clear_spmimst_rcs(struct mt6362_data *data)
{
	writel(MT6362_MSK_SPMIMST_RCSCLR,
	       (data->spmimst_base + MT6362_REG_SPMIMST_RCSCLR));
}
#endif /* MT6362_SPMIMST_RCSCLR */

static int mt6362_handle_post_irq(void *irq_drv_data)
{
	struct mt6362_data *data = irq_drv_data;
	struct regmap *regmap = data->regmap;

#ifdef MT6362_SPMIMST_RCSCLR
	mt6362_clear_spmimst_rcs(data);
#endif /* MT6362_SPMIMST_RCSCLR */
	return regmap_update_bits(regmap, MT6362_IRQ_SET, MT6362_INT_RETRIG,
				  MT6362_INT_RETRIG);
}

static const struct regmap_irq_chip spmi_regmap_irq_chip = {
	.name = "mt6362_spmi",
	.irqs = spmi_regmap_irqs,
	.num_irqs = ARRAY_SIZE(spmi_regmap_irqs),
	.num_regs = MT6362_IRQ_REGS,
	.status_base = MT6362_CHG_IRQ0,
	.mask_base = MT6362_CHG_MASK0,
	.ack_base = MT6362_CHG_IRQ0,
	.init_ack_masked = true,
	.use_ack = true,
	.handle_post_irq = mt6362_handle_post_irq,
};

static int mt6362_spmi_reg_read(void *context,
				unsigned int reg, unsigned int *val)
{
	struct mt6362_data *data = context;
	ktime_t current_time, avail_access_time;
	u8 regval;
	int ret;

	if (reg == data->last_access_reg) {
		avail_access_time = ktime_add_us(data->last_access_time, 3);
		current_time = ktime_get();
		if (ktime_before(current_time, avail_access_time)) {
			/* used for usec time ceil */
			avail_access_time = ktime_add_ns(avail_access_time,
							     NSEC_PER_USEC - 1);
			udelay(ktime_us_delta(avail_access_time, current_time));
		}
	}
	ret = spmi_ext_register_readl(data->sdev, reg, &regval, 1);
	if (ret < 0)
		return ret;
	*val = regval;
	return 0;
}

static int mt6362_spmi_reg_write(void *context,
				 unsigned int reg, unsigned int val)
{
	struct mt6362_data *data = context;
	int ret;

	ret = spmi_ext_register_writel(data->sdev, reg, (u8 *)&val, 1);
	if (ret)
		return ret;
	data->last_access_reg = reg;
	data->last_access_time = ktime_get();
	return 0;
}

static const struct regmap_config spmi_regmap_config = {
	.reg_bits	= 16,
	.val_bits	= 8,
	.max_register	= 0x7ff,
	.fast_io	= true,
	.use_single_rw  = true,
	.reg_read	= mt6362_spmi_reg_read,
	.reg_write	= mt6362_spmi_reg_write,
};

#ifdef CONFIG_FPGA_EARLY_PORTING
static int mt6362_spmi_rcs_init(struct mt6362_data *data)
{
	struct regmap *regmap = data->regmap;
	int ret;

	dev_info(dev, "%s\n", __func__);
	/* rcs_enable[7], rcs_a[6], rcs_cmd[5:4], rcs_id[3:0] */
	ret = regmap_write(regmap, MT6362_REG_SPMIM_RCS1, 0x91);
	if (ret < 0)
		return ret;
	/* rcs_addr */
	return regmap_write(regmap, MT6362_REG_SPMIM_RCS2, 0x09);
}
#endif /* CONFIG_FPGA_EARLY_PORTING */

static int mt6362_enable_hidden_mode(struct mt6362_data *data, bool en)
{
	return regmap_write(data->regmap,
			    MT6362_REG_TM_PASCODE1, en ? 0x69 : 0);
}

static int mt6362_init_setting(struct mt6362_data *data)
{
	int i, ret;

	/* initial setting */
	for (i = 0; i < ARRAY_SIZE(mt6362_init_table); i++) {
		if (mt6362_init_table[i].hidden_flag) {
			ret = mt6362_enable_hidden_mode(data, true);
			if (ret < 0)
				return ret;
		}
		ret = regmap_update_bits(data->regmap,
					 mt6362_init_table[i].addr,
					 mt6362_init_table[i].mask,
					 mt6362_init_table[i].val);
		if (mt6362_init_table[i].hidden_flag)
			mt6362_enable_hidden_mode(data, false);
		if (ret < 0)
			return ret;
	}
	return ret;
}

#ifdef MT6362_SPMIMST_RCSCLR
static struct resource spmimst_resource = {
	.start = MT6362_SPMIMST_STARTADDR,
	.end = MT6362_SPMIMST_ENDADDR,
	.flags = IORESOURCE_MEM,
	.name = "spmimst",
};
#endif /* MT6362_SPMIMST_RCSCLR */

static int mt6362_probe(struct spmi_device *sdev)
{
	struct mt6362_data *data;
	struct regmap *regmap;
	struct device_node *np = sdev->dev.of_node;
	int rv;

	data = devm_kzalloc(&sdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->sdev = sdev;
	data->dev = &sdev->dev;
	data->last_access_reg = U32_MAX;
	spmi_device_set_drvdata(sdev, data);

	regmap = devm_regmap_init(&sdev->dev, NULL, data, &spmi_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);
	data->regmap = regmap;

#ifdef CONFIG_FPGA_EARLY_PORTING
	rv = mt6362_spmi_rcs_init(data);
	if (rv < 0) {
		dev_err(&sdev->dev, "%s: spmi rcs init failed\n", __func__);
		return rv;
	}
#endif /* CONFIG_FPGA_EARLY_PORTING */

	rv = mt6362_init_setting(data);
	if (rv) {
		dev_err(&sdev->dev, "Failed to set initial setting(%d)\n", rv);
		return rv;
	}

#ifdef MT6362_SPMIMST_RCSCLR
	data->spmimst_base = devm_ioremap(&sdev->dev, spmimst_resource.start,
					  resource_size(&spmimst_resource));
	if (!data->spmimst_base) {
		dev_notice(&sdev->dev,
			   "Failed to ioremap spmi master address\n");
		return -EINVAL;
	}
	mt6362_clear_spmimst_rcs(data);
#endif /* MT6362_SPMIMST_RCSCLR */

	data->irq = of_irq_get(np, 0);
	if (data->irq < 0) {
		dev_err(&sdev->dev, "Failed to get irq(%d)\n", data->irq);
		return data->irq;
	}

	memcpy(&data->irq_chip, &spmi_regmap_irq_chip, sizeof(data->irq_chip));
	data->irq_chip.irq_drv_data = data;
	rv = devm_regmap_add_irq_chip(&sdev->dev, regmap, data->irq,
				      IRQF_ONESHOT, 0, &data->irq_chip,
				      &data->irq_data);
	if (rv) {
		dev_err(&sdev->dev, "Failed to add irq chip(%d)\n", rv);
		return rv;
	}

	rv = devm_of_platform_populate(&sdev->dev);
	if (rv) {
		dev_err(&sdev->dev, "Failed to populate children(%d)\n", rv);
		return rv;
	}

	device_init_wakeup(&sdev->dev, true);

	return 0;
}

static int __maybe_unused mt6362_suspend(struct device *dev)
{
	struct mt6362_data *data = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(data->irq);

	disable_irq(data->irq);

	dev_info(dev, "%s: done\n", __func__);
	return 0;
}

static int __maybe_unused mt6362_resume(struct device *dev)
{
	struct mt6362_data *data = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(data->irq);

	enable_irq(data->irq);

	dev_info(dev, "%s: done\n", __func__);
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6362_pm_ops, mt6362_suspend, mt6362_resume);

static const struct of_device_id __maybe_unused mt6362_ofid_tbls[] = {
	{ .compatible = "mediatek,mt6362", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt6362_of_id);

static struct spmi_driver mt6362_driver = {
	.driver = {
		.name = "mt6362",
		.of_match_table = of_match_ptr(mt6362_ofid_tbls),
		.pm = &mt6362_pm_ops,
	},
	.probe = mt6362_probe,
};
module_spmi_driver(mt6362_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6362 SPMI PMIC Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
