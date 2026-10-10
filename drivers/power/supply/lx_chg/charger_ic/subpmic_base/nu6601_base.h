/*
 *  Copyright (C) 2022 Nuvolta ro., Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __LINUX_NU6601_BASE_H
#define __LINUX_NU6601_BASE_H

struct xm_subpmic_device;

#define NU6601_DEVICE_ID        0X61

/* intr regs */
#define NU6601_REG_SW_RST	(0x23)
#define NU6601_REG_DEVICE_ID (0x04)
#define NU6601_REG_INTMNGR_STAT0 (0x0E)
#define NU6601_REG_INTMNGR_STAT1 (0x0F)
#define NU6601_REG_INFRAINT_STAT (0x10)
#define NU6601_REG_ADCINT_STAT (0x40)
#define NU6601_REG_QCINT_STAT (0xB6)
#define NU6601_REG_BC1P2INT_STAT (0xB0)
#define NU6601_REG_UFCSINT0_STAT (0x10)
#define NU6601_REG_UFCSINT1_STAT (0x14)
#define NU6601_REG_UFCSINT2_STAT (0x18)
#define NU6601_REG_LEDINT1_STAT (0x80)
#define NU6601_REG_LEDINT2_STAT (0x84)
#define NU6601_REG_BATTINT_STAT (0x70)
#define NU6601_REG_BOBULOOPINT_STAT (0x1C)
#define NU6601_REG_BOBUINT_STAT (0x18)
#define NU6601_REG_USBINT_STAT (0x14)
#define NU6601_REG_CHGINT_STAT (0x10)

#define NU6601_IRQ_EVT_MAX (128)
struct irq_mapping_tbl {
	const char *name;
	const int id;
};

#define NU6601_IRQ_MAPPING(_name, _id) { .name = #_name, .id = _id}
static const struct irq_mapping_tbl nu6601_irq_mapping_tbl[] = {
	/*0.chg irq 0~7*/
	NU6601_IRQ_MAPPING(usb_det_done, 3),
	NU6601_IRQ_MAPPING(vbus_0v, 4),
	NU6601_IRQ_MAPPING(vbus_gd, 5),
	NU6601_IRQ_MAPPING(chg_fsm, 6),
	NU6601_IRQ_MAPPING(chg_ok, 7),
	/*1.USB irq 8~15*/
	/*2.BOBU irq 16~23*/
	NU6601_IRQ_MAPPING(boost_gd, 20),
	NU6601_IRQ_MAPPING(boost_fail, 21),
	/*3.BOBULOOP irq 24~31*/
	/*4.BATTERY irq 32~39*/
	NU6601_IRQ_MAPPING(vbat_ov, 35),
	/*5.RESERVED irq 40~47*/
	/*6.LED1 irq 48~55*/
	/*7.LED2 IRQ 56~63*/
	NU6601_IRQ_MAPPING(led1_timeout, 56),
	NU6601_IRQ_MAPPING(led2_timeout, 57),
	/*8.INFRA irq 64~71*/
	/*9.ADC irq 72~79*/
	/*10.RESERVED irq 80~87*/
	/*11.QC irq 88~95*/
	NU6601_IRQ_MAPPING(dm_cot_pluse_done, 88),
	NU6601_IRQ_MAPPING(dp_cot_pluse_done, 89),
	NU6601_IRQ_MAPPING(dpdm_2pluse_done, 90),
	NU6601_IRQ_MAPPING(dpdm_3pluse_done, 91),
	NU6601_IRQ_MAPPING(dm_16pluse_done, 92),
	NU6601_IRQ_MAPPING(dp_16pluse_done, 93),
	NU6601_IRQ_MAPPING(hvdcp_det_fail, 94),
	NU6601_IRQ_MAPPING(hvdcp_det_ok, 95),
	/*12.BC1P2 irq 96~103*/
	NU6601_IRQ_MAPPING(dcd_timeout, 99),
	NU6601_IRQ_MAPPING(bc1p2_det_done, 103),
	/*13.UFCS2 irq 104~111*/
	/*14.UFCS1 irq 115~119*/
	/*15.UFCS0 irq 120~127*/
};

struct irq_addr_setting_tbl {
	u8 stat_addr;
	u8 mask_flag;
	u8 id;
};

#define NU6601_IRQ_SETTING(_addr, _flag, _id) { .stat_addr = _addr, .mask_flag = _flag, .id = _id}
static struct irq_addr_setting_tbl nu6601_irq_addr_setting_tbl[] = {
	NU6601_IRQ_SETTING(NU6601_REG_CHGINT_STAT, 0x00, 3),
	NU6601_IRQ_SETTING(NU6601_REG_USBINT_STAT, 0xff, 3),
	NU6601_IRQ_SETTING(NU6601_REG_BOBUINT_STAT, 0x03, 3),
	NU6601_IRQ_SETTING(NU6601_REG_BOBULOOPINT_STAT, 0xff, 3),
	NU6601_IRQ_SETTING(NU6601_REG_BATTINT_STAT, 0x10, 3),
	NU6601_IRQ_SETTING(0, 0, 0), //Reserverd
	NU6601_IRQ_SETTING(NU6601_REG_LEDINT1_STAT, 0x00, 3),
	NU6601_IRQ_SETTING(NU6601_REG_LEDINT2_STAT, 0x00, 3),

	NU6601_IRQ_SETTING(NU6601_REG_INFRAINT_STAT, 0x00, 2),
	NU6601_IRQ_SETTING(NU6601_REG_ADCINT_STAT, 0xff, 2),
	NU6601_IRQ_SETTING(0, 0, 0), //Reserverd
	NU6601_IRQ_SETTING(NU6601_REG_QCINT_STAT, 0x00, 4),
	NU6601_IRQ_SETTING(NU6601_REG_BC1P2INT_STAT, 0x00, 4),
	NU6601_IRQ_SETTING(NU6601_REG_UFCSINT2_STAT, 0xff, 4),
	NU6601_IRQ_SETTING(NU6601_REG_UFCSINT1_STAT, 0xff, 4),
	NU6601_IRQ_SETTING(NU6601_REG_UFCSINT0_STAT, 0xff, 4),
};

static inline void nu6601_irq_bus_lock(struct irq_data *data)
{
	struct xm_subpmic_device *subpmic_dev = data->chip_data;
	int i, ret = 0;
	u8 addr, mask, chipid = 0;

	for (i = 0; i < ARRAY_SIZE(nu6601_irq_addr_setting_tbl); i++) {
		addr = nu6601_irq_addr_setting_tbl[i].stat_addr + 2;
		if (addr == 2)
			continue;
		chipid = nu6601_irq_addr_setting_tbl[i].id;
		ret = regmap_bulk_read(subpmic_dev->rmap, addr | (chipid << 8), &mask, 1);
		if (ret < 0)
			dev_err(subpmic_dev->dev, "%s: read irq mask fail chipid %d addr %x\n", __func__, chipid, addr);

		nu6601_irq_addr_setting_tbl[i].mask_flag = mask;
	}
}

static inline void nu6601_irq_bus_unlock(struct irq_data *data)
{
	struct xm_subpmic_device *subpmic_dev = data->chip_data;
	int i, ret = 0;
	u8 addr, mask, chipid = 0;

	for (i = 0; i < ARRAY_SIZE(nu6601_irq_addr_setting_tbl); i++) {
		addr = nu6601_irq_addr_setting_tbl[i].stat_addr + 2;
		if (addr == 2)
			continue;
		chipid = nu6601_irq_addr_setting_tbl[i].id;
		mask = nu6601_irq_addr_setting_tbl[i].mask_flag;
		ret = regmap_bulk_write(subpmic_dev->rmap, addr | (chipid << 8), &mask, 1);
		if (ret < 0)
			dev_err(subpmic_dev->dev, "%s: write irq mask fail\n", __func__);
	}
}

static inline int nu6601_irq_init(struct xm_subpmic_device *subpmic_dev)
{
	int i, ret = 0;
	u8 addr, mask, val, chipid = 0;

	/* reset chip */
	mask = 0x2f;
	ret = regmap_bulk_write(subpmic_dev->rmap, NU6601_REG_SW_RST | (2 << 8), &mask, 1);
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(nu6601_irq_addr_setting_tbl); i++) {
		addr = nu6601_irq_addr_setting_tbl[i].stat_addr;
		if (!addr)
			continue;
		chipid = nu6601_irq_addr_setting_tbl[i].id;
		mask = nu6601_irq_addr_setting_tbl[i].mask_flag;

		ret = regmap_bulk_write(subpmic_dev->rmap, (addr + 2) | (chipid << 8), &mask, 1);
		if (ret < 0)
			return ret;

		//clear all INT_FLAG
		ret = regmap_bulk_read(subpmic_dev->rmap, (addr + 1) | (chipid << 8), &val, 1);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static inline const  char *nu6601_get_hwirq_name(struct xm_subpmic_device *subpmic_dev, int hwirq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(nu6601_irq_mapping_tbl); i++) {
		if (nu6601_irq_mapping_tbl[i].id == hwirq)
			return nu6601_irq_mapping_tbl[i].name;
	}
	return "not found";
}

static inline void nu6601_irq_disable(struct irq_data *data)
{
	struct xm_subpmic_device *subpmic_dev = data->chip_data;

	dev_dbg(subpmic_dev->dev, "%s: hwirq = %d, %s\n", __func__, (int)data->hwirq,
		nu6601_get_hwirq_name(subpmic_dev, (int)data->hwirq));
	nu6601_irq_addr_setting_tbl[data->hwirq / 8].mask_flag |= (1 << (data->hwirq % 8));
}

static inline void nu6601_irq_enable(struct irq_data *data)
{
	struct xm_subpmic_device *subpmic_dev = data->chip_data;

	dev_dbg(subpmic_dev->dev, "%s: hwirq = %d, %s\n", __func__, (int)data->hwirq,
		nu6601_get_hwirq_name(subpmic_dev, (int)data->hwirq));
	nu6601_irq_addr_setting_tbl[data->hwirq / 8].mask_flag &= ~(1 << (data->hwirq % 8));
}

static inline irqreturn_t nu6601_irq_handler(int irq, void *priv)
{
	struct xm_subpmic_device *subpmic_dev = (struct xm_subpmic_device *)priv;
	u16 irq_stat = 0;
	u8 sub_stat = 0;
	int i = 0, j = 0, ret = 0;
	u8 addr, chipid = 0;
	// keep wakeup
    pm_wakeup_event(subpmic_dev->dev, 500);

	/*read irq stat*/
	ret = regmap_bulk_read(subpmic_dev->rmap, NU6601_REG_INTMNGR_STAT0  | (2 << 8), (u8 *)&irq_stat, 2);
	if (ret < 0) {
		dev_err(subpmic_dev->dev, "read irq mngr fail\n");
		goto out_irq_handler;
	}
	if (irq_stat == 0)
		goto out_irq_handler;

	if (irq_stat & (1 << 12)) {
		i = 12;
		addr = nu6601_irq_addr_setting_tbl[i].stat_addr;
		chipid = nu6601_irq_addr_setting_tbl[i].id;

		/*read sub stat*/
		ret = regmap_bulk_read(subpmic_dev->rmap, (addr + 1) | (chipid << 8), &sub_stat, 1);
		if (ret < 0) {
			dev_err(subpmic_dev->dev, "read irq substa chipd %d 0x%x fail \n", chipid, addr+1);
			goto out_irq_handler;
		}

		for (j = 0; j < 8; j++) {
			if (!(sub_stat & (1 << j)))
				continue;
			ret = irq_find_mapping(subpmic_dev->domain, i * 8 + j);
			if (ret)
				handle_nested_irq(ret);
			else
				dev_err(subpmic_dev->dev, "unmapped %d %d\n", i, j);
		}
	}
		
	for (i = 0; i < ARRAY_SIZE(nu6601_irq_addr_setting_tbl); i++) {
		if (i == 12)
			continue;
		if (!(irq_stat & (1 << i)))
			continue;

		addr = nu6601_irq_addr_setting_tbl[i].stat_addr;
		chipid = nu6601_irq_addr_setting_tbl[i].id;


		/*read sub stat*/
		ret = regmap_bulk_read(subpmic_dev->rmap, (addr + 1) | (chipid << 8), &sub_stat, 1);
		if (ret < 0) {
			dev_err(subpmic_dev->dev, "read irq substa chipd %d 0x%x fail \n", chipid, addr+1);
			goto out_irq_handler;
		}

		for (j = 0; j < 8; j++) {
			if (!(sub_stat & (1 << j)))
				continue;
			ret = irq_find_mapping(subpmic_dev->domain, i * 8 + j);
			if (ret) {
			//	dev_info_ratelimited(subpmic_dev->dev,
			//			"%s: handler irq_domain = (%d, %d)\n",
			//			__func__, i, j);
				handle_nested_irq(ret);
			} else
				dev_err(subpmic_dev->dev, "unmapped %d %d\n", i, j);
		}
	}

out_irq_handler:
	return IRQ_HANDLED;
}

static inline int nu6601_check_chip(struct xm_subpmic_device *subpmic_dev)
{
    int ret;
    u8 did = 0;
    ret = regmap_bulk_read(subpmic_dev->rmap, NU6601_REG_DEVICE_ID | (2 << 8), &did, 1);
    if (ret < 0 || did != SC6601_DEVICE_ID) {
  //      return -ENODEV;
    }
	dev_err(subpmic_dev->dev, "nu6601 device id %d\n", did);
    return 0;
}

#endif /* #ifndef __LINUX_NU6601_BASE_H */
