// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>

#include "inc/mt6370_pmu.h"

#define MT6370_PMU_IRQ_EVT_MAX (128)

struct irq_mapping_tbl {
	const char *name;
	const int id;
};

#define MT6370_PMU_IRQ_MAPPING(_name, _id) { .name = #_name, .id = _id}
static const struct irq_mapping_tbl mt6370_pmu_irq_mapping_tbl[] = {
	MT6370_PMU_IRQ_MAPPING(chg_treg, 4),
	MT6370_PMU_IRQ_MAPPING(chg_aicr, 5),
	MT6370_PMU_IRQ_MAPPING(chg_mivr, 6),
	MT6370_PMU_IRQ_MAPPING(pwr_rdy, 7),
	MT6370_PMU_IRQ_MAPPING(chg_vinovp, 11),
	MT6370_PMU_IRQ_MAPPING(chg_vsysuv, 12),
	MT6370_PMU_IRQ_MAPPING(chg_vsysov, 13),
	MT6370_PMU_IRQ_MAPPING(chg_vbatov, 14),
	MT6370_PMU_IRQ_MAPPING(chg_vbusov, 15),
	MT6370_PMU_IRQ_MAPPING(ts_bat_cold, 20),
	MT6370_PMU_IRQ_MAPPING(ts_bat_cool, 21),
	MT6370_PMU_IRQ_MAPPING(ts_bat_warm, 22),
	MT6370_PMU_IRQ_MAPPING(ts_bat_hot, 23),
	MT6370_PMU_IRQ_MAPPING(chg_tmri, 27),
	MT6370_PMU_IRQ_MAPPING(chg_batabsi, 28),
	MT6370_PMU_IRQ_MAPPING(chg_adpbadi, 29),
	MT6370_PMU_IRQ_MAPPING(chg_rvpi, 30),
	MT6370_PMU_IRQ_MAPPING(otpi, 31),
	MT6370_PMU_IRQ_MAPPING(chg_aiclmeasi, 32),
	MT6370_PMU_IRQ_MAPPING(chg_ichgmeasi, 33),
	MT6370_PMU_IRQ_MAPPING(chgdet_donei, 34),
	MT6370_PMU_IRQ_MAPPING(chg_wdtmri, 35),
	MT6370_PMU_IRQ_MAPPING(ssfinishi, 36),
	MT6370_PMU_IRQ_MAPPING(chg_rechgi, 37),
	MT6370_PMU_IRQ_MAPPING(chg_termi, 38),
	MT6370_PMU_IRQ_MAPPING(chg_ieoci, 39),
	MT6370_PMU_IRQ_MAPPING(attachi, 48),
	MT6370_PMU_IRQ_MAPPING(detachi, 49),
	MT6370_PMU_IRQ_MAPPING(qc30stpdone, 51),
	MT6370_PMU_IRQ_MAPPING(qc_vbusdet_done, 52),
	MT6370_PMU_IRQ_MAPPING(hvdcp_det, 53),
	MT6370_PMU_IRQ_MAPPING(chgdeti, 54),
	MT6370_PMU_IRQ_MAPPING(dcdti, 55),
	MT6370_PMU_IRQ_MAPPING(dirchg_vgoki, 59),
	MT6370_PMU_IRQ_MAPPING(dirchg_wdtmri, 60),
	MT6370_PMU_IRQ_MAPPING(dirchg_uci, 61),
	MT6370_PMU_IRQ_MAPPING(dirchg_oci, 62),
	MT6370_PMU_IRQ_MAPPING(dirchg_ovi, 63),
	MT6370_PMU_IRQ_MAPPING(ovpctrl_swon_evt, 67),
	MT6370_PMU_IRQ_MAPPING(ovpctrl_uvp_d_evt, 68),
	MT6370_PMU_IRQ_MAPPING(ovpctrl_uvp_evt, 69),
	MT6370_PMU_IRQ_MAPPING(ovpctrl_ovp_d_evt, 70),
	MT6370_PMU_IRQ_MAPPING(ovpctrl_ovp_evt, 71),
	MT6370_PMU_IRQ_MAPPING(fled_strbpin, 72),
	MT6370_PMU_IRQ_MAPPING(fled_torpin, 73),
	MT6370_PMU_IRQ_MAPPING(fled_tx, 74),
	MT6370_PMU_IRQ_MAPPING(fled_lvf, 75),
	MT6370_PMU_IRQ_MAPPING(fled2_short, 78),
	MT6370_PMU_IRQ_MAPPING(fled1_short, 79),
	MT6370_PMU_IRQ_MAPPING(fled2_strb, 80),
	MT6370_PMU_IRQ_MAPPING(fled1_strb, 81),
	MT6370_PMU_IRQ_MAPPING(fled2_strb_to, 82),
	MT6370_PMU_IRQ_MAPPING(fled1_strb_to, 83),
	MT6370_PMU_IRQ_MAPPING(fled2_tor, 84),
	MT6370_PMU_IRQ_MAPPING(fled1_tor, 85),
	MT6370_PMU_IRQ_MAPPING(otp, 93),
	MT6370_PMU_IRQ_MAPPING(vdda_ovp, 94),
	MT6370_PMU_IRQ_MAPPING(vdda_uv, 95),
	MT6370_PMU_IRQ_MAPPING(ldo_oc, 103),
	MT6370_PMU_IRQ_MAPPING(isink4_short, 104),
	MT6370_PMU_IRQ_MAPPING(isink3_short, 105),
	MT6370_PMU_IRQ_MAPPING(isink2_short, 106),
	MT6370_PMU_IRQ_MAPPING(isink1_short, 107),
	MT6370_PMU_IRQ_MAPPING(isink4_open, 108),
	MT6370_PMU_IRQ_MAPPING(isink3_open, 109),
	MT6370_PMU_IRQ_MAPPING(isink2_open, 110),
	MT6370_PMU_IRQ_MAPPING(isink1_open, 111),
	MT6370_PMU_IRQ_MAPPING(bled_ocp, 118),
	MT6370_PMU_IRQ_MAPPING(bled_ovp, 119),
	MT6370_PMU_IRQ_MAPPING(dsv_vneg_ocp, 123),
	MT6370_PMU_IRQ_MAPPING(dsv_vpos_ocp, 124),
	MT6370_PMU_IRQ_MAPPING(dsv_bst_ocp, 125),
	MT6370_PMU_IRQ_MAPPING(dsv_vneg_scp, 126),
	MT6370_PMU_IRQ_MAPPING(dsv_vpos_scp, 127),
};

static const uint8_t mt6370_irqr_filter[16] = {
	0xF1, 0xF8, 0xF0, 0xF8, 0xFF, 0xE3, 0xFB, 0xF8,
	0xF8, 0xCF, 0x3F, 0xE0, 0x80, 0xFF, 0xC0, 0xF8,
};

static const uint8_t mt6370_irqf_filter[16] = {
	0xF1, 0xF8, 0xF0, 0x80, 0x00, 0x00, 0x00, 0x00,
	0xF8, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static uint8_t mt6370_pmu_curr_irqmask[16];

static void mt6370_pmu_irq_bus_lock(struct irq_data *data)
{
	struct mt6370_pmu_chip *chip = data->chip_data;
	int ret = 0;

	ret = mt6370_pmu_reg_block_read(chip, MT6370_PMU_CHGMASK1, 16,
					mt6370_pmu_curr_irqmask);
	if (ret < 0)
		dev_err(chip->dev, "%s: read irq mask fail\n", __func__);
}

static void mt6370_pmu_irq_bus_sync_unlock(struct irq_data *data)
{
	struct mt6370_pmu_chip *chip = data->chip_data;
	int ret = 0;

	ret = mt6370_pmu_reg_block_write(chip, MT6370_PMU_CHGMASK1, 16,
					mt6370_pmu_curr_irqmask);
	if (ret < 0)
		dev_err(chip->dev, "%s: write irq mask fail\n", __func__);
}

static void mt6370_pmu_irq_disable(struct irq_data *data)
{
	struct mt6370_pmu_chip *chip = data->chip_data;

	dev_dbg(chip->dev, "%s: hwirq = %d, %s\n", __func__, (int)data->hwirq,
		mt6370_pmu_get_hwirq_name(chip, (int)data->hwirq));
	mt6370_pmu_curr_irqmask[data->hwirq / 8] |= (1 << (data->hwirq % 8));
}

static void mt6370_pmu_irq_enable(struct irq_data *data)
{
	struct mt6370_pmu_chip *chip = data->chip_data;

	dev_dbg(chip->dev, "%s: hwirq = %d, %s\n", __func__, (int)data->hwirq,
		mt6370_pmu_get_hwirq_name(chip, (int)data->hwirq));
	mt6370_pmu_curr_irqmask[data->hwirq / 8] &= ~(1 << (data->hwirq % 8));
}

static struct irq_chip mt6370_pmu_irq_chip = {
	.name = "mt6370_pmu_irq",
	.irq_bus_lock = mt6370_pmu_irq_bus_lock,
	.irq_bus_sync_unlock = mt6370_pmu_irq_bus_sync_unlock,
	.irq_enable = mt6370_pmu_irq_enable,
	.irq_disable = mt6370_pmu_irq_disable,
};

static int mt6370_pmu_irq_domain_map(struct irq_domain *d, unsigned int virq,
					irq_hw_number_t hw)
{
	struct mt6370_pmu_chip *chip = d->host_data;

	if (hw >= MT6370_PMU_IRQ_EVT_MAX)
		return -EINVAL;
	irq_set_chip_data(virq, chip);
	irq_set_chip_and_handler(virq, &mt6370_pmu_irq_chip, handle_simple_irq);
	irq_set_nested_thread(virq, true);
	irq_set_parent(virq, chip->irq);
	irq_set_noprobe(virq);
	return 0;
}

static void mt6370_pmu_irq_domain_unmap(struct irq_domain *d, unsigned int virq)
{
	irq_set_chip_and_handler(virq, NULL, NULL);
	irq_set_chip_data(virq, NULL);
}

static const struct irq_domain_ops mt6370_pmu_irq_domain_ops = {
	.map = mt6370_pmu_irq_domain_map,
	.unmap = mt6370_pmu_irq_domain_unmap,
	.xlate = irq_domain_xlate_onetwocell,
};

static irqreturn_t mt6370_pmu_irq_handler(int irq, void *priv)
{
	struct mt6370_pmu_chip *chip = (struct mt6370_pmu_chip *)priv;
	u8 irq_ind = 0, data[16] = { 0 }, mask[16] = {
	0};
	u8 stat_chg[16] = { 0 }, stat_old[16] = {
	0}, stat_new[16] = {
	0};
	u8 valid_chg[16] = { 0 };
	int i = 0, j = 0, ret = 0;

	pr_info_ratelimited("%s\n", __func__);
	pm_runtime_get_sync(chip->dev);
	ret = mt6370_pmu_reg_write(chip, MT6370_PMU_REG_IRQMASK, 0xfe);
	if (ret < 0) {
		dev_err(chip->dev, "mask irq indicators fail\n");
		goto out_irq_handler;
	}
	ret = mt6370_pmu_reg_read(chip, MT6370_PMU_REG_IRQIND);
	if (ret < 0) {
		dev_err(chip->dev, "read irq indicator fail\n");
		goto out_irq_handler;
	}
	irq_ind = ret;

	/* read stat before reading irq evt */
	ret = mt6370_pmu_reg_block_read(chip, MT6370_PMU_REG_CHGSTAT1, 16,
					stat_old);
	if (ret < 0) {
		dev_err(chip->dev, "read prev irq stat fail\n");
		goto out_irq_handler;
	}
	/* workaround for irq, divided irq event into upper and lower */
	ret = mt6370_pmu_reg_block_read(chip, MT6370_PMU_REG_CHGIRQ1, 5, data);
	if (ret < 0) {
		dev_err(chip->dev, "read upper irq event fail\n");
		goto out_irq_handler;
	}
	ret = mt6370_pmu_reg_block_read(chip, MT6370_PMU_REG_QCIRQ, 10,
					data + 6);
	if (ret < 0) {
		dev_err(chip->dev, "read lower irq event fail\n");
		goto out_irq_handler;
	}

	/* read stat after reading irq evt */
	ret = mt6370_pmu_reg_block_read(chip, MT6370_PMU_REG_CHGSTAT1, 16,
					stat_new);
	if (ret < 0) {
		dev_err(chip->dev, "read post irq stat fail\n");
		goto out_irq_handler;
	}
	ret = mt6370_pmu_reg_block_read(chip, MT6370_PMU_CHGMASK1, 16, mask);
	if (ret < 0) {
		dev_err(chip->dev, "read irq mask fail\n");
		goto out_irq_handler;
	}
	ret = mt6370_pmu_reg_write(chip, MT6370_PMU_REG_IRQMASK, 0x00);
	if (ret < 0) {
		dev_err(chip->dev, "unmask irq indicators fail\n");
		goto out_irq_handler;
	}
	for (i = 0; i < 16; i++) {
		stat_chg[i] = stat_old[i] ^ stat_new[i];
		valid_chg[i] = (stat_new[i] & mt6370_irqr_filter[i]) |
		    (~stat_new[i] & mt6370_irqf_filter[i]);
		data[i] |= (stat_chg[i] & valid_chg[i]);
		data[i] &= ~mask[i];
		if (!data[i])
			continue;
		for (j = 0; j < 8; j++) {
			if (!(data[i] & (1 << j)))
				continue;
			ret = irq_find_mapping(chip->irq_domain, i * 8 + j);
			if (ret) {
				dev_info_ratelimited(chip->dev,
				"%s: handler irq_domain = (%d, %d)\n",
				__func__, i, j);
				handle_nested_irq(ret);
			} else
				dev_err(chip->dev, "unmapped %d %d\n", i, j);
		}
	}
out_irq_handler:
	pm_runtime_put(chip->dev);
	return IRQ_HANDLED;
}

static int mt6370_pmu_irq_init(struct mt6370_pmu_chip *chip)
{
	u8 data[16] = { 0 };
	int ret = 0;

	/* mask all */
	ret = mt6370_pmu_reg_write(chip, MT6370_PMU_REG_IRQMASK, 0xfe);
	if (ret < 0)
		return ret;
	memset(data, 0xff, 16);
	ret = mt6370_pmu_reg_block_write(chip, MT6370_PMU_CHGMASK1, 16, data);
	if (ret < 0)
		return ret;
	ret = mt6370_pmu_reg_block_read(chip, MT6370_PMU_REG_CHGIRQ1, 16, data);
	if (ret < 0)
		return ret;
	/* re-enable all, but the all part irq event masks are enabled */
	ret = mt6370_pmu_reg_write(chip, MT6370_PMU_REG_IRQMASK, 0x00);
	if (ret < 0)
		return ret;
	return 0;
}

void mt6370_pmu_irq_suspend(struct mt6370_pmu_chip *chip)
{
	if (device_may_wakeup(chip->dev))
		enable_irq_wake(chip->irq);
}
EXPORT_SYMBOL(mt6370_pmu_irq_suspend);

void mt6370_pmu_irq_resume(struct mt6370_pmu_chip *chip)
{
	if (device_may_wakeup(chip->dev))
		disable_irq_wake(chip->irq);
}
EXPORT_SYMBOL(mt6370_pmu_irq_resume);

int mt6370_pmu_get_virq_number(struct mt6370_pmu_chip *chip, const char *name)
{
	int i;

	if (!name) {
		dev_err(chip->dev, "%s: null name\n", __func__);
		return -EINVAL;
	}
	for (i = 0; i < ARRAY_SIZE(mt6370_pmu_irq_mapping_tbl); i++) {
		if (!strcmp(mt6370_pmu_irq_mapping_tbl[i].name, name)) {
			return irq_create_mapping(chip->irq_domain,
					mt6370_pmu_irq_mapping_tbl[i].id);
		}
	}
	return -EINVAL;
}
EXPORT_SYMBOL(mt6370_pmu_get_virq_number);

const char *mt6370_pmu_get_hwirq_name(struct mt6370_pmu_chip *chip, int hwirq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt6370_pmu_irq_mapping_tbl); i++) {
		if (mt6370_pmu_irq_mapping_tbl[i].id == hwirq)
			return mt6370_pmu_irq_mapping_tbl[i].name;
	}
	return "not found";
}
EXPORT_SYMBOL(mt6370_pmu_get_hwirq_name);

int mt6370_pmu_irq_register(struct mt6370_pmu_chip *chip)
{
	struct mt6370_pmu_platform_data *pdata = dev_get_platdata(chip->dev);
	int ret = 0;

	ret = mt6370_pmu_irq_init(chip);
	if (ret < 0)
		return ret;
	ret = gpio_request_one(pdata->intr_gpio, GPIOF_IN,
				"mt6370_pmu_irq_gpio");
	if (ret < 0) {
		dev_err(chip->dev, "%s: gpio request fail\n", __func__);
		return ret;
	}
	ret = gpio_to_irq(pdata->intr_gpio);
	if (ret < 0) {
		dev_err(chip->dev, "%s: irq mapping fail\n", __func__);
		goto out_pmu_irq;
	}
	chip->irq_domain = irq_domain_add_linear(chip->dev->of_node,
						 MT6370_PMU_IRQ_EVT_MAX,
						 &mt6370_pmu_irq_domain_ops,
						 chip);
	if (!chip->irq_domain) {
		dev_err(chip->dev, "irq domain register fail\n");
		ret = -EINVAL;
		goto out_pmu_irq;
	}
	chip->irq = ret;
	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
					mt6370_pmu_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"mt6370_pmu_irq", chip);
	if (ret < 0)
		goto out_pmu_irq;
	device_init_wakeup(chip->dev, true);
	return 0;
out_pmu_irq:
	gpio_free(pdata->intr_gpio);
	return ret;
}
EXPORT_SYMBOL(mt6370_pmu_irq_register);

void mt6370_pmu_irq_unregister(struct mt6370_pmu_chip *chip)
{
	struct mt6370_pmu_platform_data *pdata = dev_get_platdata(chip->dev);

	device_init_wakeup(chip->dev, false);
	devm_free_irq(chip->dev, chip->irq, chip);
	irq_domain_remove(chip->irq_domain);
	gpio_free(pdata->intr_gpio);
}
EXPORT_SYMBOL(mt6370_pmu_irq_unregister);
