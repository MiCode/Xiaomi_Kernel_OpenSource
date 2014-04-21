/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/mfd/core.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-slimslave.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/core-resource.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <sound/soc.h>

#define WCD9XXX_REGISTER_START_OFFSET 0x800
#define WCD9XXX_SLIM_RW_MAX_TRIES 3
#define SLIMBUS_PRESENT_TIMEOUT 100

#define MAX_WCD9XXX_DEVICE	4
#define CODEC_DT_MAX_PROP_SIZE   40
#define WCD9XXX_I2C_GSBI_SLAVE_ID "3-000d"
#define WCD9XXX_I2C_TOP_SLAVE_ADDR	0x0d
#define WCD9XXX_ANALOG_I2C_SLAVE_ADDR	0x77
#define WCD9XXX_DIGITAL1_I2C_SLAVE_ADDR	0x66
#define WCD9XXX_DIGITAL2_I2C_SLAVE_ADDR	0x55
#define WCD9XXX_I2C_TOP_LEVEL	0
#define WCD9XXX_I2C_ANALOG	1
#define WCD9XXX_I2C_DIGITAL_1	2
#define WCD9XXX_I2C_DIGITAL_2	3

#define ONDEMAND_REGULATOR true
#define STATIC_REGULATOR (!ONDEMAND_REGULATOR)

/* Number of return values needs to be checked for each
 * registration of Slimbus of I2C bus for each codec
 */
#define NUM_WCD9XXX_REG_RET	9

struct wcd9xxx_i2c {
	struct i2c_client *client;
	struct i2c_msg xfer_msg[2];
	struct mutex xfer_lock;
	int mod_id;
};

struct pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *extncodec_sus;
	struct pinctrl_state *extncodec_act;
};

static struct pinctrl_info pinctrl_info1;
static int extcodec_get_pinctrl(struct device *dev)
{
	struct pinctrl *pinctrl;
	pinctrl = pinctrl_get(dev);
	if (IS_ERR(pinctrl)) {
		pr_err("%s: Unable to get pinctrl handle\n", __func__);
		return -EINVAL;
	}
	pinctrl_info1.pinctrl = pinctrl;
	/* get all the states handles from Device Tree*/
	pinctrl_info1.extncodec_sus = pinctrl_lookup_state(pinctrl, "suspend");
	if (IS_ERR(pinctrl_info1.extncodec_sus)) {
		pr_err("%s: Unable to get pinctrl disable state handle\n",
				__func__);
		return -EINVAL;
	}
	pinctrl_info1.extncodec_act = pinctrl_lookup_state(pinctrl, "active");
	if (IS_ERR(pinctrl_info1.extncodec_act)) {
		pr_err("%s: Unable to get pinctrl disable state handle\n",
				__func__);
		return -EINVAL;
	}
	return 0;
}

static int wcd9xxx_dt_parse_vreg_info(struct device *dev,
				      struct wcd9xxx_regulator *vreg,
				      const char *vreg_name, bool ondemand);
static int wcd9xxx_dt_parse_micbias_info(struct device *dev,
	struct wcd9xxx_micbias_setting *micbias);
static struct wcd9xxx_pdata *wcd9xxx_populate_dt_pdata(struct device *dev);

struct wcd9xxx_i2c wcd9xxx_modules[MAX_WCD9XXX_DEVICE];

static int wcd9xxx_read(struct wcd9xxx *wcd9xxx, unsigned short reg,
		       int bytes, void *dest, bool interface_reg)
{
	int i, ret;

	if (bytes <= 0) {
		dev_err(wcd9xxx->dev, "Invalid byte read length %d\n", bytes);
		return -EINVAL;
	}

	ret = wcd9xxx->read_dev(wcd9xxx, reg, bytes, dest, interface_reg);
	if (ret < 0) {
		dev_err(wcd9xxx->dev, "Codec read failed\n");
		return ret;
	} else {
		for (i = 0; i < bytes; i++)
			dev_dbg(wcd9xxx->dev, "Read 0x%02x from 0x%x\n",
				((u8 *)dest)[i], reg + i);
	}

	return 0;
}
static int __wcd9xxx_reg_read(
	struct wcd9xxx *wcd9xxx,
	unsigned short reg)
{
	u8 val;
	int ret;

	mutex_lock(&wcd9xxx->io_lock);
	ret = wcd9xxx_read(wcd9xxx, reg, 1, &val, false);
	mutex_unlock(&wcd9xxx->io_lock);

	if (ret < 0)
		return ret;
	else
		return val;
}

int wcd9xxx_reg_read(
	struct wcd9xxx_core_resource *core_res,
	unsigned short reg)
{
	struct wcd9xxx *wcd9xxx = (struct wcd9xxx *) core_res->parent;
	return __wcd9xxx_reg_read(wcd9xxx, reg);

}
EXPORT_SYMBOL(wcd9xxx_reg_read);

static int wcd9xxx_write(struct wcd9xxx *wcd9xxx, unsigned short reg,
			int bytes, void *src, bool interface_reg)
{
	int i;

	if (bytes <= 0) {
		pr_err("%s: Error, invalid write length\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < bytes; i++)
		dev_dbg(wcd9xxx->dev, "Write %02x to 0x%x\n", ((u8 *)src)[i],
			reg + i);

	return wcd9xxx->write_dev(wcd9xxx, reg, bytes, src, interface_reg);
}

static int __wcd9xxx_reg_write(
	struct wcd9xxx *wcd9xxx,
	unsigned short reg, u8 val)
{
	int ret;

	mutex_lock(&wcd9xxx->io_lock);
	ret = wcd9xxx_write(wcd9xxx, reg, 1, &val, false);
	mutex_unlock(&wcd9xxx->io_lock);

	return ret;
}

int wcd9xxx_reg_write(
	struct wcd9xxx_core_resource *core_res,
	unsigned short reg, u8 val)
{
	struct wcd9xxx *wcd9xxx = (struct wcd9xxx *) core_res->parent;
	return __wcd9xxx_reg_write(wcd9xxx, reg, val);
}
EXPORT_SYMBOL(wcd9xxx_reg_write);

static u8 wcd9xxx_pgd_la;
static u8 wcd9xxx_inf_la;

int wcd9xxx_interface_reg_read(struct wcd9xxx *wcd9xxx, unsigned short reg)
{
	u8 val;
	int ret;

	mutex_lock(&wcd9xxx->io_lock);
	ret = wcd9xxx_read(wcd9xxx, reg, 1, &val, true);
	mutex_unlock(&wcd9xxx->io_lock);

	if (ret < 0)
		return ret;
	else
		return val;
}
EXPORT_SYMBOL(wcd9xxx_interface_reg_read);

int wcd9xxx_interface_reg_write(struct wcd9xxx *wcd9xxx, unsigned short reg,
		     u8 val)
{
	int ret;

	mutex_lock(&wcd9xxx->io_lock);
	ret = wcd9xxx_write(wcd9xxx, reg, 1, &val, true);
	mutex_unlock(&wcd9xxx->io_lock);

	return ret;
}
EXPORT_SYMBOL(wcd9xxx_interface_reg_write);

static int __wcd9xxx_bulk_read(
	struct wcd9xxx *wcd9xxx,
	unsigned short reg,
	int count, u8 *buf)
{
	int ret;

	mutex_lock(&wcd9xxx->io_lock);
	ret = wcd9xxx_read(wcd9xxx, reg, count, buf, false);
	mutex_unlock(&wcd9xxx->io_lock);

	return ret;
}

int wcd9xxx_bulk_read(
	struct wcd9xxx_core_resource *core_res,
	unsigned short reg,
	int count, u8 *buf)
{
	struct wcd9xxx *wcd9xxx =
			(struct wcd9xxx *) core_res->parent;
	return __wcd9xxx_bulk_read(wcd9xxx, reg, count, buf);
}
EXPORT_SYMBOL(wcd9xxx_bulk_read);

static int __wcd9xxx_bulk_write(struct wcd9xxx *wcd9xxx, unsigned short reg,
		     int count, u8 *buf)
{
	int ret;

	mutex_lock(&wcd9xxx->io_lock);
	ret = wcd9xxx_write(wcd9xxx, reg, count, buf, false);
	mutex_unlock(&wcd9xxx->io_lock);

	return ret;
}

int wcd9xxx_bulk_write(
	struct wcd9xxx_core_resource *core_res,
	unsigned short reg, int count, u8 *buf)
{
	struct wcd9xxx *wcd9xxx =
			(struct wcd9xxx *) core_res->parent;
	return __wcd9xxx_bulk_write(wcd9xxx, reg, count, buf);
}
EXPORT_SYMBOL(wcd9xxx_bulk_write);

static int wcd9xxx_slim_read_device(struct wcd9xxx *wcd9xxx, unsigned short reg,
				int bytes, void *dest, bool interface)
{
	int ret;
	struct slim_ele_access msg;
	int slim_read_tries = WCD9XXX_SLIM_RW_MAX_TRIES;
	msg.start_offset = WCD9XXX_REGISTER_START_OFFSET + reg;
	msg.num_bytes = bytes;
	msg.comp = NULL;

	while (1) {
		mutex_lock(&wcd9xxx->xfer_lock);
		ret = slim_request_val_element(interface ?
			       wcd9xxx->slim_slave : wcd9xxx->slim,
			       &msg, dest, bytes);
		mutex_unlock(&wcd9xxx->xfer_lock);
		if (likely(ret == 0) || (--slim_read_tries == 0))
			break;
		usleep_range(5000, 5100);
	}

	if (ret)
		pr_err("%s: Error, Codec read failed (%d)\n", __func__, ret);

	return ret;
}
/* Interface specifies whether the write is to the interface or general
 * registers.
 */
static int wcd9xxx_slim_write_device(struct wcd9xxx *wcd9xxx,
		unsigned short reg, int bytes, void *src, bool interface)
{
	int ret;
	struct slim_ele_access msg;
	int slim_write_tries = WCD9XXX_SLIM_RW_MAX_TRIES;
	msg.start_offset = WCD9XXX_REGISTER_START_OFFSET + reg;
	msg.num_bytes = bytes;
	msg.comp = NULL;

	while (1) {
		mutex_lock(&wcd9xxx->xfer_lock);
		ret = slim_change_val_element(interface ?
			      wcd9xxx->slim_slave : wcd9xxx->slim,
			      &msg, src, bytes);
		mutex_unlock(&wcd9xxx->xfer_lock);
		if (likely(ret == 0) || (--slim_write_tries == 0))
			break;
		usleep_range(5000, 5100);
	}

	if (ret)
		pr_err("%s: Error, Codec write failed (%d)\n", __func__, ret);

	return ret;
}

static struct mfd_cell tabla1x_devs[] = {
	{
		.name = "tabla1x_codec",
	},
};

static struct mfd_cell tabla_devs[] = {
	{
		.name = "tabla_codec",
	},
};

static struct mfd_cell sitar_devs[] = {
	{
		.name = "sitar_codec",
	},
};

static struct mfd_cell taiko_devs[] = {
	{
		.name = "taiko_codec",
	},
};

static struct mfd_cell tapan_devs[] = {
	{
		.name = "tapan_codec",
	},
};

static struct mfd_cell tomtom_devs[] = {
	{
		.name = "tomtom_codec",
	},
};

static const struct wcd9xxx_codec_type wcd9xxx_codecs[] = {
	{
		TABLA_MAJOR, cpu_to_le16(0x1), tabla1x_devs,
		ARRAY_SIZE(tabla1x_devs), TABLA_NUM_IRQS, -1,
		WCD9XXX_SLIM_SLAVE_ADDR_TYPE_TABLA, 0x03,
	},
	{
		TABLA_MAJOR, cpu_to_le16(0x2), tabla_devs,
		ARRAY_SIZE(tabla_devs), TABLA_NUM_IRQS, -1,
		WCD9XXX_SLIM_SLAVE_ADDR_TYPE_TABLA, 0x03
	},
	{
		/* Siter version 1 has same major chip id with Tabla */
		TABLA_MAJOR, cpu_to_le16(0x0), sitar_devs,
		ARRAY_SIZE(sitar_devs), SITAR_NUM_IRQS, -1,
		WCD9XXX_SLIM_SLAVE_ADDR_TYPE_TABLA, 0x01
	},
	{
		SITAR_MAJOR, cpu_to_le16(0x1), sitar_devs,
		ARRAY_SIZE(sitar_devs), SITAR_NUM_IRQS, -1,
		WCD9XXX_SLIM_SLAVE_ADDR_TYPE_TABLA, 0x01
	},
	{
		SITAR_MAJOR, cpu_to_le16(0x2), sitar_devs,
		ARRAY_SIZE(sitar_devs), SITAR_NUM_IRQS, -1,
		WCD9XXX_SLIM_SLAVE_ADDR_TYPE_TABLA, 0x01
	},
	{
		TAIKO_MAJOR, cpu_to_le16(0x0), taiko_devs,
		ARRAY_SIZE(taiko_devs), TAIKO_NUM_IRQS, 1,
		WCD9XXX_SLIM_SLAVE_ADDR_TYPE_TAIKO, 0x01
	},
	{
		TAIKO_MAJOR, cpu_to_le16(0x1), taiko_devs,
		ARRAY_SIZE(taiko_devs), TAIKO_NUM_IRQS, 2,
		WCD9XXX_SLIM_SLAVE_ADDR_TYPE_TAIKO, 0x01
	},
	{
		TAPAN_MAJOR, cpu_to_le16(0x0), tapan_devs,
		ARRAY_SIZE(tapan_devs), TAPAN_NUM_IRQS, -1,
		WCD9XXX_SLIM_SLAVE_ADDR_TYPE_TAIKO, 0x03
	},
	{
		TAPAN_MAJOR, cpu_to_le16(0x1), tapan_devs,
		ARRAY_SIZE(tapan_devs), TAPAN_NUM_IRQS, -1,
		WCD9XXX_SLIM_SLAVE_ADDR_TYPE_TAIKO, 0x03
	},
	{
		TOMTOM_MAJOR, cpu_to_le16(0x0), tomtom_devs,
		ARRAY_SIZE(tomtom_devs), TOMTOM_NUM_IRQS, -1,
		WCD9XXX_SLIM_SLAVE_ADDR_TYPE_TAIKO, 0x01
	},
};

static void wcd9xxx_bring_up(struct wcd9xxx *wcd9xxx)
{
	struct wcd9xxx_pdata *pdata = wcd9xxx->dev->platform_data;
	enum codec_variant cdc_var;

	if (!pdata) {
		dev_dbg(wcd9xxx->dev, "No platform data to get codec variant, falling back to default\n");
		cdc_var = WCD9XXX;
	} else
		cdc_var = pdata->cdc_variant;

	if (cdc_var == WCD9330) {
		__wcd9xxx_reg_write(wcd9xxx, WCD9330_A_LEAKAGE_CTL, 0x4);
		__wcd9xxx_reg_write(wcd9xxx, WCD9330_A_CDC_CTL, 0);
		/* wait for 5ms after codec reset for it to complete */
		usleep_range(5000, 5100);
		__wcd9xxx_reg_write(wcd9xxx, WCD9330_A_CDC_CTL, 0x1);
		__wcd9xxx_reg_write(wcd9xxx, WCD9330_A_LEAKAGE_CTL, 0x3);
		__wcd9xxx_reg_write(wcd9xxx, WCD9330_A_CDC_CTL, 0x3);
	} else {
		__wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_LEAKAGE_CTL, 0x4);
		__wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_CDC_CTL, 0);
		usleep_range(5000, 5100);
		__wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_CDC_CTL, 3);
		__wcd9xxx_reg_write(wcd9xxx, WCD9XXX_A_LEAKAGE_CTL, 3);
	}
}

static void wcd9xxx_bring_down(struct wcd9xxx *wcd9xxx)
{
	struct wcd9xxx_pdata *pdata = wcd9xxx->dev->platform_data;
	unsigned short reg;

	if (pdata && pdata->cdc_variant == WCD9330)
		reg = WCD9330_A_LEAKAGE_CTL;
	else
		reg = WCD9XXX_A_LEAKAGE_CTL;

	__wcd9xxx_reg_write(wcd9xxx, reg, 0x7);
	__wcd9xxx_reg_write(wcd9xxx, reg, 0x6);
	__wcd9xxx_reg_write(wcd9xxx, reg, 0xe);
	__wcd9xxx_reg_write(wcd9xxx, reg, 0x8);
}

static int wcd9xxx_reset(struct wcd9xxx *wcd9xxx)
{
	int ret;

	if (wcd9xxx->reset_gpio && wcd9xxx->slim_device_bootup
			&& !wcd9xxx->use_pinctrl) {
		ret = gpio_request(wcd9xxx->reset_gpio, "CDC_RESET");
		if (ret) {
			pr_err("%s: Failed to request gpio %d\n", __func__,
				wcd9xxx->reset_gpio);
			wcd9xxx->reset_gpio = 0;
			return ret;
		}
	}
	if (wcd9xxx->reset_gpio) {
		if (wcd9xxx->use_pinctrl) {
			/* Reset the CDC PDM TLMM pins to a default state */
			ret = pinctrl_select_state(pinctrl_info1.pinctrl,
				pinctrl_info1.extncodec_act);
			if (ret != 0) {
				pr_err("%s: Failed to enable gpio pins\n",
						__func__);
				return -EIO;
			}
			gpio_set_value_cansleep(wcd9xxx->reset_gpio, 0);
			msleep(20);
			gpio_set_value_cansleep(wcd9xxx->reset_gpio, 1);
			msleep(20);
			ret = pinctrl_select_state(pinctrl_info1.pinctrl,
					pinctrl_info1.extncodec_sus);
			if (ret != 0) {
				pr_err("%s: Failed to suspend reset pins\n",
						__func__);
				return -EIO;
			}
		} else {
			gpio_direction_output(wcd9xxx->reset_gpio, 0);
			msleep(20);
			gpio_direction_output(wcd9xxx->reset_gpio, 1);
			msleep(20);
		}
	}
	return 0;
}

static void wcd9xxx_free_reset(struct wcd9xxx *wcd9xxx)
{
	if (wcd9xxx->reset_gpio) {
		if (!wcd9xxx->use_pinctrl) {
			gpio_free(wcd9xxx->reset_gpio);
			wcd9xxx->reset_gpio = 0;
		} else
			pinctrl_put(pinctrl_info1.pinctrl);
	}
}

static const struct wcd9xxx_codec_type
*wcd9xxx_check_codec_type(struct wcd9xxx *wcd9xxx, u8 *version)
{
	int i, rc;
	const struct wcd9xxx_codec_type *c, *d = NULL;

	rc = __wcd9xxx_bulk_read(wcd9xxx, WCD9XXX_A_CHIP_ID_BYTE_0,
			       sizeof(wcd9xxx->id_minor),
			       (u8 *)&wcd9xxx->id_minor);
	if (rc < 0)
		goto exit;

	rc = __wcd9xxx_bulk_read(wcd9xxx, WCD9XXX_A_CHIP_ID_BYTE_2,
			       sizeof(wcd9xxx->id_major),
			       (u8 *)&wcd9xxx->id_major);
	if (rc < 0)
		goto exit;
	dev_dbg(wcd9xxx->dev, "%s: wcd9xxx chip id major 0x%x, minor 0x%x\n",
		__func__, wcd9xxx->id_major, wcd9xxx->id_minor);

	for (i = 0, c = &wcd9xxx_codecs[0]; i < ARRAY_SIZE(wcd9xxx_codecs);
	     i++, c++) {
		if (c->id_major == wcd9xxx->id_major) {
			if (c->id_minor == wcd9xxx->id_minor) {
				d = c;
				dev_dbg(wcd9xxx->dev,
					"%s: exact match %s\n", __func__,
					d->dev->name);
				break;
			} else if (!d) {
				d = c;
			} else {
				if ((d->id_minor < c->id_minor) ||
				    (d->id_minor == c->id_minor &&
				     d->version < c->version))
					d = c;
			}
			dev_dbg(wcd9xxx->dev,
				"%s: best match %s, major 0x%x, minor 0x%x\n",
				__func__, d->dev->name, d->id_major,
				d->id_minor);
		}
	}

	if (!d) {
		dev_warn(wcd9xxx->dev,
			 "%s: driver for id major 0x%x, minor 0x%x not found\n",
			 __func__, wcd9xxx->id_major, wcd9xxx->id_minor);
	} else {
		if (d->version > -1) {
			*version = d->version;
		} else {
			rc = __wcd9xxx_reg_read(wcd9xxx,
							WCD9XXX_A_CHIP_VERSION);
			if (rc < 0) {
				d = NULL;
				goto exit;
			}
			*version = (u8)rc & 0x1F;
		}
		dev_info(wcd9xxx->dev,
			 "%s: detected %s, major 0x%x, minor 0x%x, ver 0x%x\n",
			 __func__, d->dev->name, d->id_major, d->id_minor,
			 *version);
	}
exit:
	return d;
}

static int wcd9xxx_num_irq_regs(const struct wcd9xxx *wcd9xxx)
{
	return (wcd9xxx->codec_type->num_irqs / 8) +
		((wcd9xxx->codec_type->num_irqs % 8) ? 1 : 0);
}

/*
 * Interrupt table for v1 corresponds to newer version
 * codecs (wcd9304 and wcd9310)
 */
static const struct intr_data intr_tbl_v1[] = {
	{WCD9XXX_IRQ_SLIMBUS, false},
	{WCD9XXX_IRQ_MBHC_INSERTION, true},
	{WCD9XXX_IRQ_MBHC_POTENTIAL, true},
	{WCD9XXX_IRQ_MBHC_RELEASE, true},
	{WCD9XXX_IRQ_MBHC_PRESS, true},
	{WCD9XXX_IRQ_MBHC_SHORT_TERM, true},
	{WCD9XXX_IRQ_MBHC_REMOVAL, true},
	{WCD9XXX_IRQ_BG_PRECHARGE, false},
	{WCD9XXX_IRQ_PA1_STARTUP, false},
	{WCD9XXX_IRQ_PA2_STARTUP, false},
	{WCD9XXX_IRQ_PA3_STARTUP, false},
	{WCD9XXX_IRQ_PA4_STARTUP, false},
	{WCD9XXX_IRQ_PA5_STARTUP, false},
	{WCD9XXX_IRQ_MICBIAS1_PRECHARGE, false},
	{WCD9XXX_IRQ_MICBIAS2_PRECHARGE, false},
	{WCD9XXX_IRQ_MICBIAS3_PRECHARGE, false},
	{WCD9XXX_IRQ_HPH_PA_OCPL_FAULT, false},
	{WCD9XXX_IRQ_HPH_PA_OCPR_FAULT, false},
	{WCD9XXX_IRQ_EAR_PA_OCPL_FAULT, false},
	{WCD9XXX_IRQ_HPH_L_PA_STARTUP, false},
	{WCD9XXX_IRQ_HPH_R_PA_STARTUP, false},
	{WCD9320_IRQ_EAR_PA_STARTUP, false},
	{WCD9XXX_IRQ_RESERVED_0, false},
	{WCD9XXX_IRQ_RESERVED_1, false},
};

/*
 * Interrupt table for v2 corresponds to newer version
 * codecs (wcd9320 and wcd9306)
 */
static const struct intr_data intr_tbl_v2[] = {
	{WCD9XXX_IRQ_SLIMBUS, false},
	{WCD9XXX_IRQ_MBHC_INSERTION, true},
	{WCD9XXX_IRQ_MBHC_POTENTIAL, true},
	{WCD9XXX_IRQ_MBHC_RELEASE, true},
	{WCD9XXX_IRQ_MBHC_PRESS, true},
	{WCD9XXX_IRQ_MBHC_SHORT_TERM, true},
	{WCD9XXX_IRQ_MBHC_REMOVAL, true},
	{WCD9320_IRQ_MBHC_JACK_SWITCH, true},
	{WCD9306_IRQ_MBHC_JACK_SWITCH, true},
	{WCD9XXX_IRQ_BG_PRECHARGE, false},
	{WCD9XXX_IRQ_PA1_STARTUP, false},
	{WCD9XXX_IRQ_PA2_STARTUP, false},
	{WCD9XXX_IRQ_PA3_STARTUP, false},
	{WCD9XXX_IRQ_PA4_STARTUP, false},
	{WCD9306_IRQ_HPH_PA_OCPR_FAULT, false},
	{WCD9XXX_IRQ_PA5_STARTUP, false},
	{WCD9XXX_IRQ_MICBIAS1_PRECHARGE, false},
	{WCD9306_IRQ_HPH_PA_OCPL_FAULT, false},
	{WCD9XXX_IRQ_MICBIAS2_PRECHARGE, false},
	{WCD9XXX_IRQ_MICBIAS3_PRECHARGE, false},
	{WCD9XXX_IRQ_HPH_PA_OCPL_FAULT, false},
	{WCD9XXX_IRQ_HPH_PA_OCPR_FAULT, false},
	{WCD9XXX_IRQ_EAR_PA_OCPL_FAULT, false},
	{WCD9XXX_IRQ_HPH_L_PA_STARTUP, false},
	{WCD9XXX_IRQ_HPH_R_PA_STARTUP, false},
	{WCD9XXX_IRQ_RESERVED_0, false},
	{WCD9XXX_IRQ_RESERVED_1, false},
	{WCD9XXX_IRQ_MAD_AUDIO, false},
	{WCD9XXX_IRQ_MAD_BEACON, false},
	{WCD9XXX_IRQ_MAD_ULTRASOUND, false},
	{WCD9XXX_IRQ_SPEAKER_CLIPPING, false},
	{WCD9XXX_IRQ_VBAT_MONITOR_ATTACK, false},
	{WCD9XXX_IRQ_VBAT_MONITOR_RELEASE, false},
	{WCD9XXX_IRQ_RESERVED_2, false},
};

/*
 * Interrupt table for v3 corresponds to newer version
 * codecs (wcd9330)
 */
static const struct intr_data intr_tbl_v3[] = {
	{WCD9XXX_IRQ_SLIMBUS, false},
	{WCD9XXX_IRQ_MBHC_INSERTION, true},
	{WCD9XXX_IRQ_MBHC_POTENTIAL, true},
	{WCD9XXX_IRQ_MBHC_RELEASE, true},
	{WCD9XXX_IRQ_MBHC_PRESS, true},
	{WCD9XXX_IRQ_MBHC_SHORT_TERM, true},
	{WCD9XXX_IRQ_MBHC_REMOVAL, true},
	{WCD9330_IRQ_MBHC_JACK_SWITCH, true},
	{WCD9XXX_IRQ_BG_PRECHARGE, false},
	{WCD9XXX_IRQ_PA1_STARTUP, false},
	{WCD9XXX_IRQ_PA2_STARTUP, false},
	{WCD9XXX_IRQ_PA3_STARTUP, false},
	{WCD9XXX_IRQ_PA4_STARTUP, false},
	{WCD9XXX_IRQ_PA5_STARTUP, false},
	{WCD9XXX_IRQ_MICBIAS1_PRECHARGE, false},
	{WCD9XXX_IRQ_MICBIAS2_PRECHARGE, false},
	{WCD9XXX_IRQ_MICBIAS3_PRECHARGE, false},
	{WCD9XXX_IRQ_HPH_PA_OCPL_FAULT, false},
	{WCD9XXX_IRQ_HPH_PA_OCPR_FAULT, false},
	{WCD9XXX_IRQ_EAR_PA_OCPL_FAULT, false},
	{WCD9XXX_IRQ_HPH_L_PA_STARTUP, false},
	{WCD9XXX_IRQ_HPH_R_PA_STARTUP, false},
	{WCD9320_IRQ_EAR_PA_STARTUP, false},
	{WCD9330_IRQ_SVASS_ERR_EXCEPTION, false},
	{WCD9330_IRQ_SVASS_ENGINE, true},
	{WCD9330_IRQ_MAD_AUDIO, false},
	{WCD9330_IRQ_MAD_BEACON, false},
	{WCD9330_IRQ_MAD_ULTRASOUND, false},
	{WCD9330_IRQ_SPEAKER1_CLIPPING, false},
	{WCD9330_IRQ_SPEAKER2_CLIPPING, false},
	{WCD9330_IRQ_VBAT_MONITOR_ATTACK, false},
	{WCD9330_IRQ_VBAT_MONITOR_RELEASE, false},
};

static int wcd9xxx_device_init(struct wcd9xxx *wcd9xxx)
{
	int ret = 0;
	u8 version;
	const struct wcd9xxx_codec_type *found;
	struct wcd9xxx_core_resource *core_res = &wcd9xxx->core_res;

	mutex_init(&wcd9xxx->io_lock);
	mutex_init(&wcd9xxx->xfer_lock);

	dev_set_drvdata(wcd9xxx->dev, wcd9xxx);
	wcd9xxx_bring_up(wcd9xxx);

	found = wcd9xxx_check_codec_type(wcd9xxx, &version);
	if (!found) {
		ret = -ENODEV;
		goto err;
	} else {
		wcd9xxx->codec_type = found;
		wcd9xxx->version = version;
	}

	core_res->parent = wcd9xxx;
	core_res->dev = wcd9xxx->dev;

	if (wcd9xxx->codec_type->id_major == TABLA_MAJOR
		|| wcd9xxx->codec_type->id_major == SITAR_MAJOR) {
		core_res->intr_table = intr_tbl_v1;
		core_res->intr_table_size = ARRAY_SIZE(intr_tbl_v1);
	} else if (wcd9xxx->codec_type->id_major == TOMTOM_MAJOR) {
		core_res->intr_table = intr_tbl_v3;
		core_res->intr_table_size = ARRAY_SIZE(intr_tbl_v3);
	} else {
		core_res->intr_table = intr_tbl_v2;
		core_res->intr_table_size = ARRAY_SIZE(intr_tbl_v2);
	}

	wcd9xxx_core_res_init(&wcd9xxx->core_res,
				wcd9xxx->codec_type->num_irqs,
				wcd9xxx_num_irq_regs(wcd9xxx),
				wcd9xxx_reg_read, wcd9xxx_reg_write,
				wcd9xxx_bulk_read, wcd9xxx_bulk_write);

	if (wcd9xxx_core_irq_init(&wcd9xxx->core_res))
		goto err;

	ret = mfd_add_devices(wcd9xxx->dev, -1, found->dev, found->size,
			      NULL, 0, NULL);
	if (ret != 0) {
		dev_err(wcd9xxx->dev, "Failed to add children: %d\n", ret);
		goto err_irq;
	}

	ret = device_init_wakeup(wcd9xxx->dev, true);
	if (ret) {
		dev_err(wcd9xxx->dev, "Device wakeup init failed: %d\n", ret);
		goto err_irq;
	}

	return ret;
err_irq:
	wcd9xxx_irq_exit(&wcd9xxx->core_res);
err:
	wcd9xxx_bring_down(wcd9xxx);
	wcd9xxx_core_res_deinit(&wcd9xxx->core_res);
	mutex_destroy(&wcd9xxx->io_lock);
	mutex_destroy(&wcd9xxx->xfer_lock);
	return ret;
}

static void wcd9xxx_device_exit(struct wcd9xxx *wcd9xxx)
{
	device_init_wakeup(wcd9xxx->dev, false);
	wcd9xxx_irq_exit(&wcd9xxx->core_res);
	wcd9xxx_bring_down(wcd9xxx);
	wcd9xxx_free_reset(wcd9xxx);
	wcd9xxx_core_res_deinit(&wcd9xxx->core_res);
	mutex_destroy(&wcd9xxx->io_lock);
	mutex_destroy(&wcd9xxx->xfer_lock);
	if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		slim_remove_device(wcd9xxx->slim_slave);
	kfree(wcd9xxx);
}


#ifdef CONFIG_DEBUG_FS
struct wcd9xxx *debugCodec;

static struct dentry *debugfs_wcd9xxx_dent;
static struct dentry *debugfs_peek;
static struct dentry *debugfs_poke;

static unsigned char read_data;

static int codec_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static int get_parameters(char *buf, long int *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else
			return -EINVAL;
	}
	return 0;
}

static ssize_t codec_debug_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char lbuf[8];

	snprintf(lbuf, sizeof(lbuf), "0x%x\n", read_data);
	return simple_read_from_buffer(ubuf, count, ppos, lbuf,
		strnlen(lbuf, 7));
}


static ssize_t codec_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char *access_str = filp->private_data;
	char lbuf[32];
	int rc;
	long int param[5];

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';

	if (!strncmp(access_str, "poke", 6)) {
		/* write */
		rc = get_parameters(lbuf, param, 2);
		if ((param[0] <= 0x3FF) && (param[1] <= 0xFF) &&
			(rc == 0))
			wcd9xxx_interface_reg_write(debugCodec, param[0],
				param[1]);
		else
			rc = -EINVAL;
	} else if (!strncmp(access_str, "peek", 6)) {
		/* read */
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] <= 0x3FF) && (rc == 0))
			read_data = wcd9xxx_interface_reg_read(debugCodec,
				param[0]);
		else
			rc = -EINVAL;
	}

	if (rc == 0)
		rc = cnt;
	else
		pr_err("%s: rc = %d\n", __func__, rc);

	return rc;
}

static const struct file_operations codec_debug_ops = {
	.open = codec_debug_open,
	.write = codec_debug_write,
	.read = codec_debug_read
};
#endif

static int wcd9xxx_init_supplies(struct wcd9xxx *wcd9xxx,
				 struct wcd9xxx_pdata *pdata)
{
	int ret;
	int i;
	wcd9xxx->supplies = kzalloc(sizeof(struct regulator_bulk_data) *
				   ARRAY_SIZE(pdata->regulator),
				   GFP_KERNEL);
	if (!wcd9xxx->supplies) {
		ret = -ENOMEM;
		goto err;
	}

	wcd9xxx->num_of_supplies = 0;

	if (ARRAY_SIZE(pdata->regulator) > WCD9XXX_MAX_REGULATOR) {
		pr_err("%s: Array Size out of bound\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(pdata->regulator); i++) {
		if (pdata->regulator[i].name) {
			wcd9xxx->supplies[i].supply = pdata->regulator[i].name;
			wcd9xxx->num_of_supplies++;
		}
	}

	ret = regulator_bulk_get(wcd9xxx->dev, wcd9xxx->num_of_supplies,
				 wcd9xxx->supplies);
	if (ret != 0) {
		dev_err(wcd9xxx->dev, "Failed to get supplies: err = %d\n",
							ret);
		goto err_supplies;
	}

	for (i = 0; i < wcd9xxx->num_of_supplies; i++) {
		if (regulator_count_voltages(wcd9xxx->supplies[i].consumer) <=
		    0)
			continue;
		ret = regulator_set_voltage(wcd9xxx->supplies[i].consumer,
					    pdata->regulator[i].min_uV,
					    pdata->regulator[i].max_uV);
		if (ret) {
			pr_err("%s: Setting regulator voltage failed for regulator %s err = %d\n",
				__func__,
				wcd9xxx->supplies[i].supply, ret);
			goto err_get;
		}

		ret = regulator_set_optimum_mode(wcd9xxx->supplies[i].consumer,
						pdata->regulator[i].optimum_uA);
		if (ret < 0) {
			pr_err("%s: Setting regulator optimum mode failed for regulator %s err = %d\n",
				__func__,
				wcd9xxx->supplies[i].supply, ret);
			goto err_get;
		} else {
			ret = 0;
		}
	}

	return ret;

err_get:
	regulator_bulk_free(wcd9xxx->num_of_supplies, wcd9xxx->supplies);
err_supplies:
	kfree(wcd9xxx->supplies);
err:
	return ret;
}

static int wcd9xxx_enable_static_supplies(struct wcd9xxx *wcd9xxx,
					  struct wcd9xxx_pdata *pdata)
{
	int i;
	int ret = 0;

	for (i = 0; i < wcd9xxx->num_of_supplies; i++) {
		if (pdata->regulator[i].ondemand)
			continue;
		ret = regulator_enable(wcd9xxx->supplies[i].consumer);
		if (ret) {
			pr_err("%s: Failed to enable %s\n", __func__,
			       wcd9xxx->supplies[i].supply);
			break;
		} else {
			pr_debug("%s: Enabled regulator %s\n", __func__,
				 wcd9xxx->supplies[i].supply);
		}
	}

	while (ret && --i)
		if (!pdata->regulator[i].ondemand)
			regulator_disable(wcd9xxx->supplies[i].consumer);

	return ret;
}

static void wcd9xxx_disable_supplies(struct wcd9xxx *wcd9xxx,
				     struct wcd9xxx_pdata *pdata)
{
	int i;
	int rc;

	for (i = 0; i < wcd9xxx->num_of_supplies; i++) {
		if (pdata->regulator[i].ondemand)
			continue;
		rc = regulator_disable(wcd9xxx->supplies[i].consumer);
		if (rc) {
			pr_err("%s: Failed to disable %s\n", __func__,
			       wcd9xxx->supplies[i].supply);
		} else {
			pr_debug("%s: Disabled regulator %s\n", __func__,
				 wcd9xxx->supplies[i].supply);
		}
	}
	for (i = 0; i < wcd9xxx->num_of_supplies; i++) {
		if (regulator_count_voltages(wcd9xxx->supplies[i].consumer) <=
		    0)
			continue;
		regulator_set_voltage(wcd9xxx->supplies[i].consumer, 0,
				      pdata->regulator[i].max_uV);
		regulator_set_optimum_mode(wcd9xxx->supplies[i].consumer, 0);
	}
	regulator_bulk_free(wcd9xxx->num_of_supplies, wcd9xxx->supplies);
	kfree(wcd9xxx->supplies);
}

struct wcd9xxx_i2c *get_i2c_wcd9xxx_device_info(u16 reg)
{
	u16 mask = 0x0f00;
	int value = 0;
	struct wcd9xxx_i2c *wcd9xxx = NULL;
	value = ((reg & mask) >> 8) & 0x000f;
	switch (value) {
	case 0:
		wcd9xxx = &wcd9xxx_modules[0];
		break;
	case 1:
		wcd9xxx = &wcd9xxx_modules[1];
		break;
	case 2:
		wcd9xxx = &wcd9xxx_modules[2];
		break;
	case 3:
		wcd9xxx = &wcd9xxx_modules[3];
		break;
	default:
		break;
	}
	return wcd9xxx;
}

int wcd9xxx_i2c_write_device(u16 reg, u8 *value,
				u32 bytes)
{

	struct i2c_msg *msg;
	int ret = 0;
	u8 reg_addr = 0;
	u8 data[bytes + 1];
	struct wcd9xxx_i2c *wcd9xxx;

	wcd9xxx = get_i2c_wcd9xxx_device_info(reg);
	if (wcd9xxx == NULL || wcd9xxx->client == NULL) {
		pr_err("failed to get device info\n");
		return -ENODEV;
	}
	reg_addr = (u8)reg;
	msg = &wcd9xxx->xfer_msg[0];
	msg->addr = wcd9xxx->client->addr;
	msg->len = bytes + 1;
	msg->flags = 0;
	data[0] = reg;
	data[1] = *value;
	msg->buf = data;
	ret = i2c_transfer(wcd9xxx->client->adapter, wcd9xxx->xfer_msg, 1);
	/* Try again if the write fails */
	if (ret != 1) {
		ret = i2c_transfer(wcd9xxx->client->adapter,
						wcd9xxx->xfer_msg, 1);
		if (ret != 1) {
			pr_err("failed to write the device\n");
			return ret;
		}
	}
	pr_debug("write sucess register = %x val = %x\n", reg, data[1]);
	return 0;
}


int wcd9xxx_i2c_read_device(unsigned short reg,
				  int bytes, unsigned char *dest)
{
	struct i2c_msg *msg;
	int ret = 0;
	u8 reg_addr = 0;
	struct wcd9xxx_i2c *wcd9xxx;
	u8 i = 0;

	wcd9xxx = get_i2c_wcd9xxx_device_info(reg);
	if (wcd9xxx == NULL || wcd9xxx->client == NULL) {
		pr_err("failed to get device info\n");
		return -ENODEV;
	}
	for (i = 0; i < bytes; i++) {
		reg_addr = (u8)reg++;
		msg = &wcd9xxx->xfer_msg[0];
		msg->addr = wcd9xxx->client->addr;
		msg->len = 1;
		msg->flags = 0;
		msg->buf = &reg_addr;

		msg = &wcd9xxx->xfer_msg[1];
		msg->addr = wcd9xxx->client->addr;
		msg->len = 1;
		msg->flags = I2C_M_RD;
		msg->buf = dest++;
		ret = i2c_transfer(wcd9xxx->client->adapter,
				wcd9xxx->xfer_msg, 2);

		/* Try again if read fails first time */
		if (ret != 2) {
			ret = i2c_transfer(wcd9xxx->client->adapter,
							wcd9xxx->xfer_msg, 2);
			if (ret != 2) {
				pr_err("failed to read wcd9xxx register\n");
				return ret;
			}
		}
	}
	return 0;
}

int wcd9xxx_i2c_read(struct wcd9xxx *wcd9xxx, unsigned short reg,
			int bytes, void *dest, bool interface_reg)
{
	return wcd9xxx_i2c_read_device(reg, bytes, dest);
}

int wcd9xxx_i2c_write(struct wcd9xxx *wcd9xxx, unsigned short reg,
			 int bytes, void *src, bool interface_reg)
{
	return wcd9xxx_i2c_write_device(reg, src, bytes);
}

static int wcd9xxx_i2c_get_client_index(struct i2c_client *client,
					int *wcd9xx_index)
{
	int ret = 0;
	switch (client->addr) {
	case WCD9XXX_I2C_TOP_SLAVE_ADDR:
		*wcd9xx_index = WCD9XXX_I2C_TOP_LEVEL;
	break;
	case WCD9XXX_ANALOG_I2C_SLAVE_ADDR:
		*wcd9xx_index = WCD9XXX_I2C_ANALOG;
	break;
	case WCD9XXX_DIGITAL1_I2C_SLAVE_ADDR:
		*wcd9xx_index = WCD9XXX_I2C_DIGITAL_1;
	break;
	case WCD9XXX_DIGITAL2_I2C_SLAVE_ADDR:
		*wcd9xx_index = WCD9XXX_I2C_DIGITAL_2;
	break;
	default:
		ret = -EINVAL;
	break;
	}
	return ret;
}

static int wcd9xxx_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct wcd9xxx *wcd9xxx = NULL;
	struct wcd9xxx_pdata *pdata = NULL;
	int val = 0;
	int ret = 0;
	int wcd9xx_index = 0;
	struct device *dev;
	int intf_type;

	intf_type = wcd9xxx_get_intf_type();

	pr_debug("%s: interface status %d\n", __func__, intf_type);
	if (intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		dev_dbg(&client->dev, "%s:Codec is detected in slimbus mode\n",
			__func__);
		return -ENODEV;
	} else if (intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
		ret = wcd9xxx_i2c_get_client_index(client, &wcd9xx_index);
		if (ret != 0)
			dev_err(&client->dev, "%s: I2C set codec I2C\n"
				"client failed\n", __func__);
		else {
			dev_err(&client->dev, "%s:probe for other slaves\n"
				"devices of codec I2C slave Addr = %x\n",
				__func__, client->addr);
			wcd9xxx_modules[wcd9xx_index].client = client;
		}
		return ret;
	} else if (intf_type == WCD9XXX_INTERFACE_TYPE_PROBING) {
		dev = &client->dev;
		if (client->dev.of_node) {
			dev_dbg(&client->dev, "%s:Platform data\n"
				"from device tree\n", __func__);
			pdata = wcd9xxx_populate_dt_pdata(&client->dev);
			client->dev.platform_data = pdata;
		} else {
			dev_dbg(&client->dev, "%s:Platform data from\n"
				"board file\n", __func__);
			pdata = client->dev.platform_data;
		}
		wcd9xxx = kzalloc(sizeof(struct wcd9xxx), GFP_KERNEL);
		if (wcd9xxx == NULL) {
			pr_err("%s: error, allocation failed\n", __func__);
			ret = -ENOMEM;
			goto fail;
		}

		if (!pdata) {
			dev_dbg(&client->dev, "no platform data?\n");
			ret = -EINVAL;
			goto fail;
		}
		pdata = wcd9xxx_populate_dt_pdata(&client->dev);
		ret = extcodec_get_pinctrl(&client->dev);
		if (ret < 0)
			wcd9xxx->use_pinctrl = false;
		else
			wcd9xxx->use_pinctrl = true;
		dev_dbg(&client->dev, " wcd9xxx->use_pinctrl = %d\n",wcd9xxx->use_pinctrl);

		if (i2c_check_functionality(client->adapter,
					    I2C_FUNC_I2C) == 0) {
			dev_dbg(&client->dev, "can't talk I2C?\n");
			ret = -EIO;
			goto fail;
		}
		dev_set_drvdata(&client->dev, wcd9xxx);
		wcd9xxx->dev = &client->dev;
		wcd9xxx->reset_gpio = pdata->reset_gpio;
		wcd9xxx->slim_device_bootup = true;
		if (client->dev.of_node)
			wcd9xxx->mclk_rate = pdata->mclk_rate;

		ret = wcd9xxx_init_supplies(wcd9xxx, pdata);
		if (ret) {
			pr_err("%s: Fail to enable Codec supplies\n",
			       __func__);
			goto err_codec;
		}

		ret = wcd9xxx_enable_static_supplies(wcd9xxx, pdata);
		if (ret) {
			pr_err("%s: Fail to enable Codec pre-reset supplies\n",
			       __func__);
			goto err_codec;
		}
		usleep_range(5, 10);

		ret = wcd9xxx_reset(wcd9xxx);
		if (ret) {
			pr_err("%s: Resetting Codec failed\n", __func__);
			goto err_supplies;
		}

		ret = wcd9xxx_i2c_get_client_index(client, &wcd9xx_index);
		if (ret != 0) {
			pr_err("%s:Set codec I2C client failed\n", __func__);
			goto err_supplies;
		}

		wcd9xxx_modules[wcd9xx_index].client = client;
		wcd9xxx->read_dev = wcd9xxx_i2c_read;
		wcd9xxx->write_dev = wcd9xxx_i2c_write;
		if (!wcd9xxx->dev->of_node)
			wcd9xxx_initialize_irq(&wcd9xxx->core_res,
					pdata->irq, pdata->irq_base);

		ret = wcd9xxx_device_init(wcd9xxx);
		if (ret) {
			pr_err("%s: error, initializing device failed\n",
			       __func__);
			goto err_device_init;
		}

		ret = wcd9xxx_read(wcd9xxx, WCD9XXX_A_CHIP_STATUS, 1, &val, 0);
		if (ret < 0)
			pr_err("%s: failed to read the wcd9xxx status (%d)\n",
			       __func__, ret);
		if (val != wcd9xxx->codec_type->i2c_chip_status)
			pr_err("%s: unknown chip status 0x%x\n", __func__, val);

		wcd9xxx_set_intf_type(WCD9XXX_INTERFACE_TYPE_I2C);

		return ret;
	} else
		pr_err("%s: I2C probe in wrong state\n", __func__);


err_device_init:
	wcd9xxx_free_reset(wcd9xxx);
err_supplies:
	wcd9xxx_disable_supplies(wcd9xxx, pdata);
err_codec:
	kfree(wcd9xxx);
fail:
	return ret;
}

static int wcd9xxx_i2c_remove(struct i2c_client *client)
{
	struct wcd9xxx *wcd9xxx;
	struct wcd9xxx_pdata *pdata = client->dev.platform_data;
	pr_debug("exit\n");
	wcd9xxx = dev_get_drvdata(&client->dev);
	wcd9xxx_disable_supplies(wcd9xxx, pdata);
	wcd9xxx_device_exit(wcd9xxx);
	return 0;
}

static int wcd9xxx_dt_parse_vreg_info(struct device *dev,
				      struct wcd9xxx_regulator *vreg,
				      const char *vreg_name,
				      bool ondemand)
{
	int len, ret = 0;
	const __be32 *prop;
	char prop_name[CODEC_DT_MAX_PROP_SIZE];
	struct device_node *regnode = NULL;
	u32 prop_val;

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE, "%s-supply",
		vreg_name);
	regnode = of_parse_phandle(dev->of_node, prop_name, 0);

	if (!regnode) {
		dev_err(dev, "Looking up %s property in node %s failed",
				prop_name, dev->of_node->full_name);
		return -ENODEV;
	}
	vreg->name = vreg_name;
	vreg->ondemand = ondemand;

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
		"qcom,%s-voltage", vreg_name);
	prop = of_get_property(dev->of_node, prop_name, &len);

	if (!prop || (len != (2 * sizeof(__be32)))) {
		dev_err(dev, "%s %s property\n",
				prop ? "invalid format" : "no", prop_name);
		return -EINVAL;
	} else {
		vreg->min_uV = be32_to_cpup(&prop[0]);
		vreg->max_uV = be32_to_cpup(&prop[1]);
	}

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
			"qcom,%s-current", vreg_name);

	ret = of_property_read_u32(dev->of_node, prop_name, &prop_val);
	if (ret) {
		dev_err(dev, "Looking up %s property in node %s failed",
				prop_name, dev->of_node->full_name);
		return -EFAULT;
	}
	vreg->optimum_uA = prop_val;

	dev_info(dev, "%s: vol=[%d %d]uV, curr=[%d]uA, ond %d\n", vreg->name,
		vreg->min_uV, vreg->max_uV, vreg->optimum_uA, vreg->ondemand);
	return 0;
}

static int wcd9xxx_read_of_property_u32(struct device *dev,
	const char *name, u32 *val)
{
	int ret = 0;
	ret = of_property_read_u32(dev->of_node, name, val);
	if (ret)
		dev_err(dev, "Looking up %s property in node %s failed",
				name, dev->of_node->full_name);
	return ret;
}

static int wcd9xxx_dt_parse_micbias_info(struct device *dev,
	struct wcd9xxx_micbias_setting *micbias)
{
	u32 prop_val;

	if (!(wcd9xxx_read_of_property_u32(dev, "qcom,cdc-micbias-ldoh-v",
				&prop_val)))
		micbias->ldoh_v  =  (u8)prop_val;

	wcd9xxx_read_of_property_u32(dev, "qcom,cdc-micbias-cfilt1-mv",
				&micbias->cfilt1_mv);

	wcd9xxx_read_of_property_u32(dev, "qcom,cdc-micbias-cfilt2-mv",
				&micbias->cfilt2_mv);

	wcd9xxx_read_of_property_u32(dev, "qcom,cdc-micbias-cfilt3-mv",
				&micbias->cfilt3_mv);

	/* Read micbias values for codec. Does not matter even if a few
	 * micbias values are not defined in the Device Tree. Codec will
	 * anyway not use those values
	 */
	if (!(wcd9xxx_read_of_property_u32(dev, "qcom,cdc-micbias1-cfilt-sel",
				&prop_val)))
		micbias->bias1_cfilt_sel = (u8)prop_val;

	if (!(wcd9xxx_read_of_property_u32(dev, "qcom,cdc-micbias2-cfilt-sel",
				&prop_val)))
		micbias->bias2_cfilt_sel = (u8)prop_val;

	if (!(wcd9xxx_read_of_property_u32(dev, "qcom,cdc-micbias3-cfilt-sel",
				&prop_val)))
		micbias->bias3_cfilt_sel = (u8)prop_val;

	if (!(wcd9xxx_read_of_property_u32(dev, "qcom,cdc-micbias4-cfilt-sel",
				&prop_val)))
		micbias->bias4_cfilt_sel = (u8)prop_val;

	/* micbias external cap */
	micbias->bias1_cap_mode =
	    (of_property_read_bool(dev->of_node, "qcom,cdc-micbias1-ext-cap") ?
	     MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);
	micbias->bias2_cap_mode =
	    (of_property_read_bool(dev->of_node, "qcom,cdc-micbias2-ext-cap") ?
	     MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);
	micbias->bias3_cap_mode =
	    (of_property_read_bool(dev->of_node, "qcom,cdc-micbias3-ext-cap") ?
	     MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);
	micbias->bias4_cap_mode =
	    (of_property_read_bool(dev->of_node, "qcom,cdc-micbias4-ext-cap") ?
	     MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);

	micbias->bias2_is_headset_only =
	    of_property_read_bool(dev->of_node,
				  "qcom,cdc-micbias2-headset-only");

	dev_dbg(dev, "ldoh_v  %u cfilt1_mv %u cfilt2_mv %u cfilt3_mv %u",
		(u32)micbias->ldoh_v, (u32)micbias->cfilt1_mv,
		(u32)micbias->cfilt2_mv, (u32)micbias->cfilt3_mv);

	dev_dbg(dev, "bias1_cfilt_sel %u bias2_cfilt_sel %u\n",
		(u32)micbias->bias1_cfilt_sel, (u32)micbias->bias2_cfilt_sel);

	dev_dbg(dev, "bias3_cfilt_sel %u bias4_cfilt_sel %u\n",
		(u32)micbias->bias3_cfilt_sel, (u32)micbias->bias4_cfilt_sel);

	dev_dbg(dev, "bias1_ext_cap %d bias2_ext_cap %d\n",
		micbias->bias1_cap_mode, micbias->bias2_cap_mode);
	dev_dbg(dev, "bias3_ext_cap %d bias4_ext_cap %d\n",
		micbias->bias3_cap_mode, micbias->bias4_cap_mode);

	dev_dbg(dev, "bias2_is_headset_only %d\n",
		micbias->bias2_is_headset_only);
	return 0;
}

static int wcd9xxx_dt_parse_slim_interface_dev_info(struct device *dev,
						struct slim_device *slim_ifd)
{
	int ret = 0;
	struct property *prop;

	ret = of_property_read_string(dev->of_node, "qcom,cdc-slim-ifd",
				      &slim_ifd->name);
	if (ret) {
		dev_err(dev, "Looking up %s property in node %s failed",
			"qcom,cdc-slim-ifd-dev", dev->of_node->full_name);
		return -ENODEV;
	}
	prop = of_find_property(dev->of_node,
			"qcom,cdc-slim-ifd-elemental-addr", NULL);
	if (!prop) {
		dev_err(dev, "Looking up %s property in node %s failed",
			"qcom,cdc-slim-ifd-elemental-addr",
			dev->of_node->full_name);
		return -ENODEV;
	} else if (prop->length != 6) {
		dev_err(dev, "invalid codec slim ifd addr. addr length = %d\n",
			      prop->length);
		return -ENODEV;
	}
	memcpy(slim_ifd->e_addr, prop->value, 6);

	return 0;
}

static int wcd9xxx_process_supplies(struct device *dev,
		struct wcd9xxx_pdata *pdata, const char *supply_list,
		int supply_cnt, bool is_ondemand, int index)
{
	int idx, ret = 0;
	const char *name;

	if (supply_cnt == 0) {
		dev_dbg(dev, "%s: no supplies defined for %s\n", __func__,
				supply_list);
		return 0;
	}

	for (idx = 0; idx < supply_cnt; idx++) {
		ret = of_property_read_string_index(dev->of_node,
						    supply_list, idx,
						    &name);
		if (ret) {
			dev_err(dev, "%s: of read string %s idx %d error %d\n",
				__func__, supply_list, idx, ret);
			goto err;
		}

		dev_dbg(dev, "%s: Found cdc supply %s as part of %s\n",
				__func__, name, supply_list);
		ret = wcd9xxx_dt_parse_vreg_info(dev,
					&pdata->regulator[index + idx],
					name, is_ondemand);
		if (ret)
			goto err;
	}

	return 0;

err:
	return ret;

}

static struct wcd9xxx_pdata *wcd9xxx_populate_dt_pdata(struct device *dev)
{
	struct wcd9xxx_pdata *pdata;
	int ret, static_cnt, ond_cnt, cp_supplies_cnt;
	u32 mclk_rate = 0;
	u32 dmic_sample_rate = 0;
	const char *static_prop_name = "qcom,cdc-static-supplies";
	const char *ond_prop_name = "qcom,cdc-on-demand-supplies";
	const char *cp_supplies_name = "qcom,cdc-cp-supplies";
	const char *cdc_name;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		return NULL;
	}

	static_cnt = of_property_count_strings(dev->of_node, static_prop_name);
	if (IS_ERR_VALUE(static_cnt)) {
		dev_err(dev, "%s: Failed to get static supplies %d\n", __func__,
			static_cnt);
		goto err;
	}

	/* On-demand supply list is an optional property */
	ond_cnt = of_property_count_strings(dev->of_node, ond_prop_name);
	if (IS_ERR_VALUE(ond_cnt))
		ond_cnt = 0;

	/* cp-supplies list is an optional property */
	cp_supplies_cnt = of_property_count_strings(dev->of_node,
							cp_supplies_name);
	if (IS_ERR_VALUE(cp_supplies_cnt))
		cp_supplies_cnt = 0;

	BUG_ON(static_cnt <= 0 || ond_cnt < 0 || cp_supplies_cnt < 0);
	if ((static_cnt + ond_cnt + cp_supplies_cnt)
			> ARRAY_SIZE(pdata->regulator)) {
		dev_err(dev, "%s: Num of supplies %u > max supported %zu\n",
			__func__, static_cnt, ARRAY_SIZE(pdata->regulator));
		goto err;
	}

	ret = wcd9xxx_process_supplies(dev, pdata, static_prop_name,
				static_cnt, STATIC_REGULATOR, 0);
	if (ret)
		goto err;

	ret = wcd9xxx_process_supplies(dev, pdata, ond_prop_name,
				ond_cnt, ONDEMAND_REGULATOR, static_cnt);
	if (ret)
		goto err;

	ret = wcd9xxx_process_supplies(dev, pdata, cp_supplies_name,
				cp_supplies_cnt, ONDEMAND_REGULATOR,
				static_cnt + ond_cnt);
	if (ret)
		goto err;

	ret = wcd9xxx_dt_parse_micbias_info(dev, &pdata->micbias);
	if (ret)
		goto err;

	pdata->reset_gpio = of_get_named_gpio(dev->of_node,
				"qcom,cdc-reset-gpio", 0);
	if (pdata->reset_gpio < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
			"qcom, cdc-reset-gpio", dev->of_node->full_name,
			pdata->reset_gpio);
		goto err;
	}
	dev_dbg(dev, "%s: reset gpio %d", __func__, pdata->reset_gpio);
	ret = of_property_read_u32(dev->of_node,
				   "qcom,cdc-mclk-clk-rate",
				   &mclk_rate);
	if (ret) {
		dev_err(dev, "Looking up %s property in\n"
			"node %s failed",
			"qcom,cdc-mclk-clk-rate",
			dev->of_node->full_name);
		devm_kfree(dev, pdata);
		ret = -EINVAL;
		goto err;
	}
	pdata->mclk_rate = mclk_rate;

	ret = of_property_read_u32(dev->of_node,
				"qcom,cdc-dmic-sample-rate",
				&dmic_sample_rate);
	if (ret) {
		dev_err(dev, "Looking up %s property in node %s failed",
			"qcom,cdc-dmic-sample-rate",
			dev->of_node->full_name);
		dmic_sample_rate = WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED;
	}
	if (pdata->mclk_rate == WCD9XXX_MCLK_CLK_9P6HZ) {
		if ((dmic_sample_rate != WCD9XXX_DMIC_SAMPLE_RATE_2P4MHZ) &&
		    (dmic_sample_rate != WCD9XXX_DMIC_SAMPLE_RATE_3P2MHZ) &&
		    (dmic_sample_rate != WCD9XXX_DMIC_SAMPLE_RATE_4P8MHZ) &&
		    (dmic_sample_rate != WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED)) {
			dev_err(dev, "Invalid dmic rate %d for mclk %d\n",
				dmic_sample_rate, pdata->mclk_rate);
			ret = -EINVAL;
			goto err;
		}
	} else if (pdata->mclk_rate == WCD9XXX_MCLK_CLK_12P288MHZ) {
		if ((dmic_sample_rate != WCD9XXX_DMIC_SAMPLE_RATE_3P072MHZ) &&
		    (dmic_sample_rate != WCD9XXX_DMIC_SAMPLE_RATE_4P096MHZ) &&
		    (dmic_sample_rate != WCD9XXX_DMIC_SAMPLE_RATE_6P144MHZ) &&
		    (dmic_sample_rate != WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED)) {
			dev_err(dev, "Invalid dmic rate %d for mclk %d\n",
				dmic_sample_rate, pdata->mclk_rate);
			ret = -EINVAL;
			goto err;
		}
	}
	pdata->dmic_sample_rate = dmic_sample_rate;

	ret = of_property_read_string(dev->of_node,
				"qcom,cdc-variant",
				&cdc_name);
	if (ret) {
		dev_dbg(dev, "Property %s not found in node %s\n",
				"qcom,cdc-variant",
				dev->of_node->full_name);
		pdata->cdc_variant = WCD9XXX;
	} else {
		if (!strcmp(cdc_name, "WCD9330"))
			pdata->cdc_variant = WCD9330;
		else
			pdata->cdc_variant = WCD9XXX;
	}

	return pdata;
err:
	devm_kfree(dev, pdata);
	return NULL;
}

static int wcd9xxx_slim_get_laddr(struct slim_device *sb,
				  const u8 *e_addr, u8 e_len, u8 *laddr)
{
	int ret;
	const unsigned long timeout = jiffies +
				      msecs_to_jiffies(SLIMBUS_PRESENT_TIMEOUT);

	do {
		ret = slim_get_logical_addr(sb, e_addr, e_len, laddr);
		if (!ret)
			break;
		/* Give SLIMBUS time to report present and be ready. */
		usleep_range(1000, 1100);
		pr_debug_ratelimited("%s: retyring get logical addr\n",
				     __func__);
	} while time_before(jiffies, timeout);

	return ret;
}

static int wcd9xxx_slim_probe(struct slim_device *slim)
{
	struct wcd9xxx *wcd9xxx;
	struct wcd9xxx_pdata *pdata;
	int ret = 0;
	int intf_type;

	intf_type = wcd9xxx_get_intf_type();

	if (intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
		dev_dbg(&slim->dev, "%s:Codec is detected in I2C mode\n",
			__func__);
		return -ENODEV;
	}
	if (slim->dev.of_node) {
		dev_info(&slim->dev, "Platform data from device tree\n");
		pdata = wcd9xxx_populate_dt_pdata(&slim->dev);
		ret = wcd9xxx_dt_parse_slim_interface_dev_info(&slim->dev,
				&pdata->slimbus_slave_device);
		if (ret) {
			dev_err(&slim->dev, "Error, parsing slim interface\n");
			devm_kfree(&slim->dev, pdata);
			ret = -EINVAL;
			goto err;
		}
		slim->dev.platform_data = pdata;

	} else {
		dev_info(&slim->dev, "Platform data from board file\n");
		pdata = slim->dev.platform_data;
	}

	if (!pdata) {
		dev_err(&slim->dev, "Error, no platform data\n");
		ret = -EINVAL;
		goto err;
	}

	wcd9xxx = kzalloc(sizeof(struct wcd9xxx), GFP_KERNEL);
	if (wcd9xxx == NULL) {
		pr_err("%s: error, allocation failed\n", __func__);
		ret = -ENOMEM;
		goto err;
	}
	if (!slim->ctrl) {
		pr_err("Error, no SLIMBUS control data\n");
		ret = -EINVAL;
		goto err_codec;
	}
	wcd9xxx->slim = slim;
	slim_set_clientdata(slim, wcd9xxx);
	wcd9xxx->reset_gpio = pdata->reset_gpio;
	wcd9xxx->dev = &slim->dev;
	wcd9xxx->mclk_rate = pdata->mclk_rate;
	wcd9xxx->slim_device_bootup = true;

	ret = wcd9xxx_init_supplies(wcd9xxx, pdata);
	if (ret) {
		pr_err("%s: Fail to init Codec supplies %d\n", __func__, ret);
		goto err_codec;
	}
	ret = wcd9xxx_enable_static_supplies(wcd9xxx, pdata);
	if (ret) {
		pr_err("%s: Fail to enable Codec pre-reset supplies\n",
		       __func__);
		goto err_codec;
	}
	usleep_range(5, 10);

	ret = wcd9xxx_reset(wcd9xxx);
	if (ret) {
		pr_err("%s: Resetting Codec failed\n", __func__);
		goto err_supplies;
	}

	ret = wcd9xxx_slim_get_laddr(wcd9xxx->slim, wcd9xxx->slim->e_addr,
				     ARRAY_SIZE(wcd9xxx->slim->e_addr),
				     &wcd9xxx->slim->laddr);
	if (ret) {
		pr_err("%s: failed to get slimbus %s logical address: %d\n",
		       __func__, wcd9xxx->slim->name, ret);
		goto err_reset;
	}
	wcd9xxx->read_dev = wcd9xxx_slim_read_device;
	wcd9xxx->write_dev = wcd9xxx_slim_write_device;
	wcd9xxx_pgd_la = wcd9xxx->slim->laddr;
	wcd9xxx->slim_slave = &pdata->slimbus_slave_device;
	if (!wcd9xxx->dev->of_node)
		wcd9xxx_initialize_irq(&wcd9xxx->core_res,
					pdata->irq, pdata->irq_base);

	ret = slim_add_device(slim->ctrl, wcd9xxx->slim_slave);
	if (ret) {
		pr_err("%s: error, adding SLIMBUS device failed\n", __func__);
		goto err_reset;
	}

	ret = wcd9xxx_slim_get_laddr(wcd9xxx->slim_slave,
				     wcd9xxx->slim_slave->e_addr,
				     ARRAY_SIZE(wcd9xxx->slim_slave->e_addr),
				     &wcd9xxx->slim_slave->laddr);
	if (ret) {
		pr_err("%s: failed to get slimbus %s logical address: %d\n",
		       __func__, wcd9xxx->slim->name, ret);
		goto err_slim_add;
	}
	wcd9xxx_inf_la = wcd9xxx->slim_slave->laddr;
	wcd9xxx_set_intf_type(WCD9XXX_INTERFACE_TYPE_SLIMBUS);

	ret = wcd9xxx_device_init(wcd9xxx);
	if (ret) {
		pr_err("%s: error, initializing device failed\n", __func__);
		goto err_slim_add;
	}
#ifdef CONFIG_DEBUG_FS
	debugCodec = wcd9xxx;

	debugfs_wcd9xxx_dent = debugfs_create_dir
		("wcd9310_slimbus_interface_device", 0);
	if (!IS_ERR(debugfs_wcd9xxx_dent)) {
		debugfs_peek = debugfs_create_file("peek",
		S_IFREG | S_IRUGO, debugfs_wcd9xxx_dent,
		(void *) "peek", &codec_debug_ops);

		debugfs_poke = debugfs_create_file("poke",
		S_IFREG | S_IRUGO, debugfs_wcd9xxx_dent,
		(void *) "poke", &codec_debug_ops);
	}
#endif

	return ret;

err_slim_add:
	slim_remove_device(wcd9xxx->slim_slave);
err_reset:
	wcd9xxx_free_reset(wcd9xxx);
err_supplies:
	wcd9xxx_disable_supplies(wcd9xxx, pdata);
err_codec:
	kfree(wcd9xxx);
err:
	return ret;
}
static int wcd9xxx_slim_remove(struct slim_device *pdev)
{
	struct wcd9xxx *wcd9xxx;
	struct wcd9xxx_pdata *pdata = pdev->dev.platform_data;

#ifdef CONFIG_DEBUG_FS
	debugfs_remove(debugfs_peek);
	debugfs_remove(debugfs_poke);
	debugfs_remove(debugfs_wcd9xxx_dent);
#endif
	wcd9xxx = slim_get_devicedata(pdev);
	wcd9xxx_deinit_slimslave(wcd9xxx);
	slim_remove_device(wcd9xxx->slim_slave);
	wcd9xxx_disable_supplies(wcd9xxx, pdata);
	wcd9xxx_device_exit(wcd9xxx);
	return 0;
}

static int wcd9xxx_device_up(struct wcd9xxx *wcd9xxx)
{
	int ret = 0;
	struct wcd9xxx_core_resource *wcd9xxx_res = &wcd9xxx->core_res;

	if (wcd9xxx->slim_device_bootup) {
		wcd9xxx->slim_device_bootup = false;
		return 0;
	}

	dev_info(wcd9xxx->dev, "%s: codec bring up\n", __func__);
	wcd9xxx_bring_up(wcd9xxx);
	ret = wcd9xxx_irq_init(wcd9xxx_res);
	if (ret) {
		pr_err("%s: wcd9xx_irq_init failed : %d\n", __func__, ret);
	} else {
		if (wcd9xxx->post_reset)
			ret = wcd9xxx->post_reset(wcd9xxx);
	}
	return ret;
}

static int wcd9xxx_slim_device_reset(struct slim_device *sldev)
{
	int ret;
	struct wcd9xxx *wcd9xxx = slim_get_devicedata(sldev);
	if (!wcd9xxx) {
		pr_err("%s: wcd9xxx is NULL\n", __func__);
		return -EINVAL;
	}

	dev_info(wcd9xxx->dev, "%s: device reset\n", __func__);
	if (wcd9xxx->slim_device_bootup)
		return 0;
	ret = wcd9xxx_reset(wcd9xxx);
	if (ret)
		dev_err(wcd9xxx->dev, "%s: Resetting Codec failed\n", __func__);

	return ret;
}

static int wcd9xxx_slim_device_up(struct slim_device *sldev)
{
	struct wcd9xxx *wcd9xxx = slim_get_devicedata(sldev);
	if (!wcd9xxx) {
		pr_err("%s: wcd9xxx is NULL\n", __func__);
		return -EINVAL;
	}
	dev_info(wcd9xxx->dev, "%s: slim device up\n", __func__);
	return wcd9xxx_device_up(wcd9xxx);
}

static int wcd9xxx_slim_device_down(struct slim_device *sldev)
{
	struct wcd9xxx *wcd9xxx = slim_get_devicedata(sldev);

	if (!wcd9xxx) {
		pr_err("%s: wcd9xxx is NULL\n", __func__);
		return -EINVAL;
	}
	wcd9xxx_irq_exit(&wcd9xxx->core_res);
	if (wcd9xxx->dev_down)
		wcd9xxx->dev_down(wcd9xxx);
	dev_dbg(wcd9xxx->dev, "%s: device down\n", __func__);
	return 0;
}

static int wcd9xxx_slim_resume(struct slim_device *sldev)
{
	struct wcd9xxx *wcd9xxx = slim_get_devicedata(sldev);
	return wcd9xxx_core_res_resume(&wcd9xxx->core_res);
}

static int wcd9xxx_i2c_resume(struct i2c_client *i2cdev)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(&i2cdev->dev);
	if (wcd9xxx)
		return wcd9xxx_core_res_resume(&wcd9xxx->core_res);
	else
		return 0;
}

static int wcd9xxx_slim_suspend(struct slim_device *sldev, pm_message_t pmesg)
{
	struct wcd9xxx *wcd9xxx = slim_get_devicedata(sldev);
	return wcd9xxx_core_res_suspend(&wcd9xxx->core_res, pmesg);
}

static int wcd9xxx_i2c_suspend(struct i2c_client *i2cdev, pm_message_t pmesg)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(&i2cdev->dev);
	if (wcd9xxx)
		return wcd9xxx_core_res_suspend(&wcd9xxx->core_res, pmesg);
	else
		return 0;
}

static const struct slim_device_id sitar_slimtest_id[] = {
	{"sitar-slim", 0},
	{}
};
static struct slim_driver sitar_slim_driver = {
	.driver = {
		.name = "sitar-slim",
		.owner = THIS_MODULE,
	},
	.probe = wcd9xxx_slim_probe,
	.remove = wcd9xxx_slim_remove,
	.id_table = sitar_slimtest_id,
	.resume = wcd9xxx_slim_resume,
	.suspend = wcd9xxx_slim_suspend,
};

static const struct slim_device_id sitar1p1_slimtest_id[] = {
	{"sitar1p1-slim", 0},
	{}
};
static struct slim_driver sitar1p1_slim_driver = {
	.driver = {
		.name = "sitar1p1-slim",
		.owner = THIS_MODULE,
	},
	.probe = wcd9xxx_slim_probe,
	.remove = wcd9xxx_slim_remove,
	.id_table = sitar1p1_slimtest_id,
	.resume = wcd9xxx_slim_resume,
	.suspend = wcd9xxx_slim_suspend,
};

static const struct slim_device_id slimtest_id[] = {
	{"tabla-slim", 0},
	{}
};

static struct slim_driver tabla_slim_driver = {
	.driver = {
		.name = "tabla-slim",
		.owner = THIS_MODULE,
	},
	.probe = wcd9xxx_slim_probe,
	.remove = wcd9xxx_slim_remove,
	.id_table = slimtest_id,
	.resume = wcd9xxx_slim_resume,
	.suspend = wcd9xxx_slim_suspend,
};

static const struct slim_device_id slimtest2x_id[] = {
	{"tabla2x-slim", 0},
	{}
};

static struct slim_driver tabla2x_slim_driver = {
	.driver = {
		.name = "tabla2x-slim",
		.owner = THIS_MODULE,
	},
	.probe = wcd9xxx_slim_probe,
	.remove = wcd9xxx_slim_remove,
	.id_table = slimtest2x_id,
	.resume = wcd9xxx_slim_resume,
	.suspend = wcd9xxx_slim_suspend,
};

static const struct slim_device_id taiko_slimtest_id[] = {
	{"taiko-slim-pgd", 0},
	{}
};

static struct slim_driver taiko_slim_driver = {
	.driver = {
		.name = "taiko-slim",
		.owner = THIS_MODULE,
	},
	.probe = wcd9xxx_slim_probe,
	.remove = wcd9xxx_slim_remove,
	.id_table = taiko_slimtest_id,
	.resume = wcd9xxx_slim_resume,
	.suspend = wcd9xxx_slim_suspend,
	.device_up = wcd9xxx_slim_device_up,
	.reset_device = wcd9xxx_slim_device_reset,
	.device_down = wcd9xxx_slim_device_down,
};

static const struct slim_device_id tapan_slimtest_id[] = {
	{"tapan-slim-pgd", 0},
	{}
};

static struct slim_driver tapan_slim_driver = {
	.driver = {
		.name = "tapan-slim",
		.owner = THIS_MODULE,
	},
	.probe = wcd9xxx_slim_probe,
	.remove = wcd9xxx_slim_remove,
	.id_table = tapan_slimtest_id,
	.resume = wcd9xxx_slim_resume,
	.suspend = wcd9xxx_slim_suspend,
	.device_up = wcd9xxx_slim_device_up,
	.reset_device = wcd9xxx_slim_device_reset,
	.device_down = wcd9xxx_slim_device_down,
};

static const struct slim_device_id tomtom_slimtest_id[] = {
	{"tomtom-slim-pgd", 0},
	{}
};

static struct slim_driver tomtom_slim_driver = {
	.driver = {
		.name = "tomtom-slim",
		.owner = THIS_MODULE,
	},
	.probe = wcd9xxx_slim_probe,
	.remove = wcd9xxx_slim_remove,
	.id_table = tomtom_slimtest_id,
	.resume = wcd9xxx_slim_resume,
	.suspend = wcd9xxx_slim_suspend,
	.device_up = wcd9xxx_slim_device_up,
	.reset_device = wcd9xxx_slim_device_reset,
	.device_down = wcd9xxx_slim_device_down,
};

static struct i2c_device_id wcd9xxx_id_table[] = {
	{"wcd9xxx-i2c", WCD9XXX_I2C_TOP_LEVEL},
	{"wcd9xxx-i2c", WCD9XXX_I2C_ANALOG},
	{"wcd9xxx-i2c", WCD9XXX_I2C_DIGITAL_1},
	{"wcd9xxx-i2c", WCD9XXX_I2C_DIGITAL_2},
	{}
};

static struct i2c_device_id tabla_id_table[] = {
	{"tabla top level", WCD9XXX_I2C_TOP_LEVEL},
	{"tabla analog", WCD9XXX_I2C_ANALOG},
	{"tabla digital1", WCD9XXX_I2C_DIGITAL_1},
	{"tabla digital2", WCD9XXX_I2C_DIGITAL_2},
	{}
};
MODULE_DEVICE_TABLE(i2c, tabla_id_table);

static struct i2c_driver tabla_i2c_driver = {
	.driver                 = {
		.owner          =       THIS_MODULE,
		.name           =       "tabla-i2c-core",
	},
	.id_table               =       tabla_id_table,
	.probe                  =       wcd9xxx_i2c_probe,
	.remove                 =       wcd9xxx_i2c_remove,
	.resume	= wcd9xxx_i2c_resume,
	.suspend = wcd9xxx_i2c_suspend,
};

static struct i2c_driver wcd9xxx_i2c_driver = {
	.driver                 = {
		.owner          =       THIS_MODULE,
		.name           =       "wcd9xxx-i2c-core",
	},
	.id_table               =       wcd9xxx_id_table,
	.probe                  =       wcd9xxx_i2c_probe,
	.remove                 =       wcd9xxx_i2c_remove,
	.resume	= wcd9xxx_i2c_resume,
	.suspend = wcd9xxx_i2c_suspend,
};


static int __init wcd9xxx_init(void)
{
	int ret[NUM_WCD9XXX_REG_RET];
	int i = 0;

	wcd9xxx_set_intf_type(WCD9XXX_INTERFACE_TYPE_PROBING);

	ret[0] = slim_driver_register(&tabla_slim_driver);
	if (ret[0])
		pr_err("Failed to register tabla SB driver: %d\n", ret[0]);

	ret[1] = slim_driver_register(&tabla2x_slim_driver);
	if (ret[1])
		pr_err("Failed to register tabla2x SB driver: %d\n", ret[1]);

	ret[2] = i2c_add_driver(&tabla_i2c_driver);
	if (ret[2])
		pr_err("failed to add the tabla2x I2C driver: %d\n", ret[2]);

	ret[3] = slim_driver_register(&sitar_slim_driver);
	if (ret[3])
		pr_err("Failed to register sitar SB driver: %d\n", ret[3]);

	ret[4] = slim_driver_register(&sitar1p1_slim_driver);
	if (ret[4])
		pr_err("Failed to register sitar SB driver: %d\n", ret[4]);

	ret[5] = slim_driver_register(&taiko_slim_driver);
	if (ret[5])
		pr_err("Failed to register taiko SB driver: %d\n", ret[5]);

	ret[6] = i2c_add_driver(&wcd9xxx_i2c_driver);
	if (ret[6])
		pr_err("failed to add the wcd9xxx I2C driver: %d\n", ret[6]);

	ret[7] = slim_driver_register(&tapan_slim_driver);
	if (ret[7])
		pr_err("Failed to register tapan SB driver: %d\n", ret[7]);

	ret[8] = slim_driver_register(&tomtom_slim_driver);
	if (ret[8])
		pr_err("Failed to register tomtom SB driver: %d\n", ret[8]);

	for (i = 0; i < NUM_WCD9XXX_REG_RET; i++) {
		if (ret[i])
			return ret[i];
	}
	return 0;
}
module_init(wcd9xxx_init);

static void __exit wcd9xxx_exit(void)
{
	wcd9xxx_set_intf_type(WCD9XXX_INTERFACE_TYPE_PROBING);
}
module_exit(wcd9xxx_exit);

MODULE_DESCRIPTION("Codec core driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
