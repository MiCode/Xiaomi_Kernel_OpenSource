// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 */

#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/core.h>
#include <linux/mfd/sc27xx-pmic.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/pm_wakeirq.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>
#include <uapi/linux/usb/charger.h>
#include <linux/seq_buf.h>
#include <../drivers/unisoc_platform/sysdump/unisoc_sysdump.h>
#include <linux/wakeup_reason.h>

#define SPRD_PMIC_INT_MASK_STATUS	0x0
#define SPRD_PMIC_INT_RAW_STATUS	0x4
#define SPRD_PMIC_INT_EN		0x8

#define SPRD_SC2720_IRQ_BASE		0xc0
#define SPRD_SC2720_IRQ_NUMS		9
#define SPRD_SC2720_SLAVE_ID            0x0
#define SPRD_SC2721_IRQ_BASE		0xc0
#define SPRD_SC2721_IRQ_NUMS		11
#define SPRD_SC2721_SLAVE_ID            0x0
#define SPRD_SC2730_IRQ_BASE		0x80
#define SPRD_SC2730_IRQ_NUMS		10
#define SPRD_SC2730_SLAVE_ID            0x0
#define SPRD_SC2730_CHG_DET		0x1b9c
#define SPRD_SC2731_IRQ_BASE		0x140
#define SPRD_SC2731_IRQ_NUMS		16
#define SPRD_SC2731_SLAVE_ID            0x0
#define SPRD_SC2731_CHG_DET		0xedc
#define SPRD_UMP9620_IRQ_BASE           0x80
#define SPRD_UMP9620_IRQ_NUMS           11
#define SPRD_UMP9620_SLAVE_ID           0x0
#define SPRD_UMP9621_SLAVE_ID           0x8000
#define SPRD_UMP9622_SLAVE_ID           0xc000
#define SPRD_UIP8520_IRQ_BASE           0x80
#define SPRD_UIP8520_IRQ_NUMS           12
#define SPRD_UIP8520_SLAVE_ID           0x14000

#define SPRD_PMIC_EIC_IE		0x18
#define SPRD_PMIC_EIC_RIS		0x1c
#define SPRD_PMIC_EIC_MIS		0x20
#define SPRD_SC2731_EIC_BASE		0x300
#define SPRD_SC2730_EIC_BASE		0x280
#define SPRD_SC2721_EIC_BASE		0x280
#define SPRD_SC2720_EIC_BASE		0x280
#define SPRD_UMP9620_EIC_BASE		0x280
#define SPRD_UIP8520_EIC_BASE		0x280

/* PMIC charger detection definition */
#define SPRD_PMIC_CHG_DET_DELAY_US	200000
#define SPRD_PMIC_CHG_DET_TIMEOUT	2000000
#define SPRD_PMIC_CHG_DET_DONE		BIT(11)
#define SPRD_PMIC_SDP_TYPE		BIT(7)
#define SPRD_PMIC_DCP_TYPE		BIT(6)
#define SPRD_PMIC_CDP_TYPE		BIT(5)
#define SPRD_PMIC_CHG_TYPE_MASK		GENMASK(7, 5)

#define SPRD_PRINT_BUF_LEN              8192
#define PMICINT_printf(m, x...)			\
	do {                                              \
		if (!m)                                   \
			pr_debug("%s", x);                      \
		else if (seq_buf_printf(m, x)) {         \
			seq_buf_clear(m);                 \
			seq_buf_printf(m, x);             \
		}                                         \
} while (0)

struct sprd_pmic {
	struct regmap *regmap;
	struct device *dev;
	struct regmap_irq *irqs;
	struct regmap_irq_chip irq_chip;
	struct regmap_irq_chip_data *irq_data;
	const struct sprd_pmic_data *pdata;
	int irq;
};

struct sprd_pmic_data {
	u32 slave_id;
	u32 irq_base;
	u32 eic_base;
	u32 num_irqs;
	u32 charger_det;
};

/*
 * Since different PMICs of SC27xx series can have different interrupt
 * base address and irq number, we should save irq number and irq base
 * in the device data structure.
 */
static const struct sprd_pmic_data sc2720_data = {
	.slave_id = SPRD_SC2720_SLAVE_ID,
	.irq_base = SPRD_SC2720_IRQ_BASE,
	.eic_base = SPRD_SC2720_EIC_BASE,
	.num_irqs = SPRD_SC2720_IRQ_NUMS,
};

static const struct sprd_pmic_data sc2721_data = {
	.slave_id = SPRD_SC2721_SLAVE_ID,
	.irq_base = SPRD_SC2721_IRQ_BASE,
	.eic_base = SPRD_SC2721_EIC_BASE,
	.num_irqs = SPRD_SC2721_IRQ_NUMS,
};

static const struct sprd_pmic_data sc2730_data = {
	.slave_id = SPRD_SC2730_SLAVE_ID,
	.irq_base = SPRD_SC2730_IRQ_BASE,
	.eic_base = SPRD_SC2730_EIC_BASE,
	.num_irqs = SPRD_SC2730_IRQ_NUMS,
	.charger_det = SPRD_SC2730_CHG_DET,
};

static const struct sprd_pmic_data sc2731_data = {
	.slave_id = SPRD_SC2731_SLAVE_ID,
	.irq_base = SPRD_SC2731_IRQ_BASE,
	.eic_base = SPRD_SC2731_EIC_BASE,
	.num_irqs = SPRD_SC2731_IRQ_NUMS,
	.charger_det = SPRD_SC2731_CHG_DET,
};

static const struct sprd_pmic_data ump9620_data = {
	.slave_id = SPRD_UMP9620_SLAVE_ID,
	.irq_base = SPRD_UMP9620_IRQ_BASE,
	.eic_base = SPRD_UMP9620_EIC_BASE,
	.num_irqs = SPRD_UMP9620_IRQ_NUMS,
};

static const struct sprd_pmic_data uip8520_data = {
	.slave_id = SPRD_UIP8520_SLAVE_ID,
	.irq_base = SPRD_UIP8520_IRQ_BASE,
	.eic_base = SPRD_UIP8520_EIC_BASE,
	.num_irqs = SPRD_UIP8520_IRQ_NUMS,
};

static const struct sprd_pmic_data ump9621_data = {
	.slave_id = SPRD_UMP9621_SLAVE_ID,
};

static const struct sprd_pmic_data ump9622_data = {
	.slave_id = SPRD_UMP9622_SLAVE_ID,
};

enum usb_charger_type sprd_pmic_detect_charger_type(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct sprd_pmic *ddata = spi_get_drvdata(spi);
	const struct sprd_pmic_data *pdata = ddata->pdata;
	enum usb_charger_type type;
	u32 val;
	int ret;

	ret = regmap_read_poll_timeout(ddata->regmap, pdata->charger_det, val,
				       (val & SPRD_PMIC_CHG_DET_DONE),
				       SPRD_PMIC_CHG_DET_DELAY_US,
				       SPRD_PMIC_CHG_DET_TIMEOUT);
	if (ret) {
		dev_err(&spi->dev, "failed to detect charger type\n");
		return UNKNOWN_TYPE;
	}

	switch (val & SPRD_PMIC_CHG_TYPE_MASK) {
	case SPRD_PMIC_CDP_TYPE:
		type = CDP_TYPE;
		break;
	case SPRD_PMIC_DCP_TYPE:
		type = DCP_TYPE;
		break;
	case SPRD_PMIC_SDP_TYPE:
		type = SDP_TYPE;
		break;
	default:
		type = UNKNOWN_TYPE;
		break;
	}

	return type;
}
EXPORT_SYMBOL_GPL(sprd_pmic_detect_charger_type);

/* SPRD PMIC INT MT SUPPORT */
static char *sprd_pmicint_buf;
static struct seq_buf *sprd_pmicint_seq_buf;
static char pmicint_temp_buf[256];

static void sprd_pmic_irq_log_reason(struct sprd_pmic *ddata, u32 mask_status)
{
	u32 i = 0;
	int nested_irq = 0;

	for (i = 0; i < ddata->irq_chip.num_irqs; i++) {
		if (mask_status & BIT(i)) {
			nested_irq = irq_find_mapping(regmap_irq_get_domain(ddata->irq_data), i);
			log_threaded_irq_wakeup_reason(nested_irq, ddata->irq);
		}
	}
}

static int sprd_pmic_handle_pre_irq(void *irq_drv_data)
{
	struct sprd_pmic *ddata = irq_drv_data;
	u32 mask_status = 0, raw_status = 0;
	u32 eic_mis_status = 0, eic_ris_status = 0;
	int j;

	if (regmap_read(ddata->regmap, ddata->pdata->irq_base + SPRD_PMIC_INT_MASK_STATUS,
			&mask_status) < 0) {
		pr_err("failed to get pmicint mask\n");
	}
	sprd_pmic_irq_log_reason(ddata, mask_status);

	if (regmap_read(ddata->regmap, ddata->pdata->irq_base + SPRD_PMIC_INT_RAW_STATUS,
			&raw_status) < 0) {
		pr_err("failed to get pmicint raw\n");
	}

	if (regmap_read(ddata->regmap, ddata->pdata->eic_base + SPRD_PMIC_EIC_MIS,
			&eic_mis_status) < 0) {
		pr_err("failed to get eic_mis_status\n");
	}

	if (regmap_read(ddata->regmap, ddata->pdata->eic_base + SPRD_PMIC_EIC_RIS,
			&eic_ris_status) < 0) {
		pr_err("failed to get eic_ris_status\n");
	}

	j = snprintf(pmicint_temp_buf, sizeof(pmicint_temp_buf),
			"INT-mask=%d raw=%d * EIC-mis=%d ris=%d\n", mask_status, raw_status,
			eic_mis_status, eic_ris_status);

	return 0;
}

static int sprd_pmic_handle_post_irq(void *irq_drv_data)
{
	PMICINT_printf(sprd_pmicint_seq_buf, pmicint_temp_buf);
	return 0;
}
/* SPRD PMIC INT MT SUPPORT */

static int sprd_pmic_spi_write(void *context, const void *data, size_t count)
{
	struct device *dev = context;
	struct spi_device *spi = to_spi_device(dev);
	const struct sprd_pmic_data *pdata;
	int ret;
	u32 mdata[2];

	/* The pmic only supports operation of 16bit data and 16bit addr*/
	if (count > 8) {
		dev_err(&spi->dev, "data count exceed!\n");
		return -EINVAL;
	}
	pdata = ((struct sprd_pmic *)spi_get_drvdata(spi))->pdata;

	mdata[0] = *((u32 *)data);
	mdata[1] = *((u32 *)data + 1);
	*mdata += pdata->slave_id;
	ret = spi_write(spi, (const void *)mdata, count);

	if (ret)
		pr_err("pmic mfd write failed!\n");

	return ret;
}

static int sprd_pmic_spi_read(void *context,
			      const void *reg, size_t reg_size,
			      void *val, size_t val_size)
{
	struct device *dev = context;
	struct spi_device *spi = to_spi_device(dev);
	const struct sprd_pmic_data *pdata;
	u32 rx_buf[2] = { 0 };
	int ret;

	/* Now we only support one PMIC register to read every time. */
	if (reg_size != sizeof(u32) || val_size != sizeof(u32))
		return -EINVAL;

	pdata = ((struct sprd_pmic *)spi_get_drvdata(spi))->pdata;
	/* Copy address to read from into first element of SPI buffer. */
	memcpy(rx_buf, reg, sizeof(u32));
	rx_buf[0] += pdata->slave_id;
	ret = spi_read(spi, rx_buf, 1);
	if (ret < 0) {
		pr_err("pmic mfd read failed!\n");
		return ret;
	}

	memcpy(val, rx_buf, val_size);
	return 0;
}

static struct regmap_bus sprd_pmic_regmap = {
	.write = sprd_pmic_spi_write,
	.read = sprd_pmic_spi_read,
	.reg_format_endian_default = REGMAP_ENDIAN_NATIVE,
	.val_format_endian_default = REGMAP_ENDIAN_NATIVE,
};

static const struct regmap_config sprd_pmic_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = 0xffff,
};

static void sprd_pmic_minidump_init(struct spi_device *spi)
{
	int ret;

	sprd_pmicint_buf = devm_kzalloc(&spi->dev, SPRD_PRINT_BUF_LEN, GFP_KERNEL);
	if (IS_ERR_OR_NULL(sprd_pmicint_buf))
		goto error_sprd_pmicint_buf;

	sprd_pmicint_seq_buf = devm_kzalloc(&spi->dev, sizeof(*sprd_pmicint_seq_buf), GFP_KERNEL);
	if (IS_ERR_OR_NULL(sprd_pmicint_seq_buf))
		goto error_sprd_pmicint_seq_buf;

	ret = minidump_save_extend_information("sprd-pmic-spi",
					       __pa((unsigned long)(sprd_pmicint_buf)),
					       __pa((unsigned long)(sprd_pmicint_buf) +
						    SPRD_PRINT_BUF_LEN));

	if (ret)
		goto error_sprd_pmicint_minidump;

	seq_buf_init(sprd_pmicint_seq_buf, sprd_pmicint_buf, SPRD_PRINT_BUF_LEN);
	return;

error_sprd_pmicint_minidump:
	devm_kfree(&spi->dev, sprd_pmicint_seq_buf);
error_sprd_pmicint_seq_buf:
	sprd_pmicint_seq_buf = NULL;
	devm_kfree(&spi->dev, sprd_pmicint_buf);
error_sprd_pmicint_buf:
	sprd_pmicint_buf = NULL;
	dev_err(&spi->dev, "alloc sprd-pmic-spi fail\n");
}

static int sprd_pmic_probe(struct spi_device *spi)
{
	struct sprd_pmic *ddata;
	const struct sprd_pmic_data *pdata;
	int ret, i;

	pdata = of_device_get_match_data(&spi->dev);
	if (!pdata) {
		dev_err(&spi->dev, "No matching driver data found\n");
		return -EINVAL;
	}

	ddata = devm_kzalloc(&spi->dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;

	ddata->regmap = devm_regmap_init(&spi->dev, &sprd_pmic_regmap,
					 &spi->dev, &sprd_pmic_config);
	if (IS_ERR(ddata->regmap)) {
		ret = PTR_ERR(ddata->regmap);
		dev_err(&spi->dev, "Failed to allocate register map %d\n", ret);
		return ret;
	}

	spi_set_drvdata(spi, ddata);
	ddata->dev = &spi->dev;
	ddata->pdata = pdata;

	if (pdata->irq_base)
		sprd_pmic_minidump_init(spi);

	if (spi->irq) {
		ddata->irq = spi->irq;

		ddata->irq_chip.name = dev_name(&spi->dev);
		ddata->irq_chip.status_base =
			pdata->irq_base + SPRD_PMIC_INT_MASK_STATUS;
		ddata->irq_chip.mask_base = pdata->irq_base + SPRD_PMIC_INT_EN;
		ddata->irq_chip.ack_base = 0;
		ddata->irq_chip.num_regs = 1;
		ddata->irq_chip.num_irqs = pdata->num_irqs;
		ddata->irq_chip.mask_invert = true;
		ddata->irq_chip.handle_pre_irq = sprd_pmic_handle_pre_irq;
		ddata->irq_chip.handle_post_irq = sprd_pmic_handle_post_irq;
		ddata->irq_chip.irq_drv_data = ddata;

		ddata->irqs = devm_kcalloc(&spi->dev, pdata->num_irqs, sizeof(struct regmap_irq), GFP_KERNEL);
		if (!ddata->irqs)
			return -ENOMEM;

		ddata->irq_chip.irqs = ddata->irqs;
		for (i = 0; i < pdata->num_irqs; i++)
			ddata->irqs[i].mask = BIT(i);

		ret = devm_regmap_add_irq_chip(&spi->dev, ddata->regmap, ddata->irq, IRQF_ONESHOT, 0,
					       &ddata->irq_chip, &ddata->irq_data);
		if (ret) {
			dev_err(&spi->dev, "Failed to add PMIC irq chip %d\n", ret);
			return ret;
		}
	}

	ret = devm_of_platform_populate(&spi->dev);
	if (ret) {
		dev_err(&spi->dev, "Failed to populate sub-devices %d\n", ret);
		return ret;
	}

	if (ddata->irq) {
		device_init_wakeup(&spi->dev, true);
		dev_pm_set_wake_irq(&spi->dev, ddata->irq);
	}

	return 0;
}

static int sprd_pmic_remove(struct spi_device *spi)
{
	if (device_may_wakeup(&spi->dev)) {
		dev_pm_clear_wake_irq(&spi->dev);
		device_init_wakeup(&spi->dev, false);
	}
	return 0;
}

static const struct of_device_id sprd_pmic_match[] = {
	{ .compatible = "sprd,sc2720", .data = &sc2720_data },
	{ .compatible = "sprd,sc2721", .data = &sc2721_data },
	{ .compatible = "sprd,sc2731", .data = &sc2731_data },
	{ .compatible = "sprd,sc2730", .data = &sc2730_data },
	{ .compatible = "sprd,ump9620", .data = &ump9620_data },
	{ .compatible = "sprd,ump9621", .data = &ump9621_data },
	{ .compatible = "sprd,ump9622", .data = &ump9622_data },
	{ .compatible = "sprd,uip8520", .data = &uip8520_data },
	{},
};
MODULE_DEVICE_TABLE(of, sprd_pmic_match);

static const struct spi_device_id sprd_pmic_spi_ids[] = {
	{ .name = "sc2720", .driver_data = (unsigned long)&sc2720_data },
	{ .name = "sc2721", .driver_data = (unsigned long)&sc2721_data },
	{ .name = "sc2731", .driver_data = (unsigned long)&sc2731_data },
	{ .name = "sc2730", .driver_data = (unsigned long)&sc2730_data },
	{ .name = "ump9620", .driver_data = (unsigned long)&ump9620_data },
	{ .name = "ump9621", .driver_data = (unsigned long)&ump9621_data },
	{ .name = "ump9622", .driver_data = (unsigned long)&ump9622_data },
	{ .name = "uip8520", .driver_data = (unsigned long)&uip8520_data },
	{},
};
MODULE_DEVICE_TABLE(spi, sprd_pmic_spi_ids);

static struct spi_driver sprd_pmic_driver = {
	.driver = {
		.name = "sc27xx-pmic",
		.of_match_table = sprd_pmic_match,
	},
	.probe = sprd_pmic_probe,
	.remove = sprd_pmic_remove,
	.id_table = sprd_pmic_spi_ids,
};

static int __init sprd_pmic_init(void)
{
	return spi_register_driver(&sprd_pmic_driver);
}
subsys_initcall(sprd_pmic_init);

static void __exit sprd_pmic_exit(void)
{
	spi_unregister_driver(&sprd_pmic_driver);
}
module_exit(sprd_pmic_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum SC27xx PMICs driver");
MODULE_AUTHOR("Baolin Wang <baolin.wang@spreadtrum.com>");

