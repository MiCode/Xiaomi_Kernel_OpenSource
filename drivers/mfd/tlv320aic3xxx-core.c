/*
 * tlv320aic3xxx-core.c  -- driver for TLV320AIC3XXX
 *
 * Author:      Mukund Navada <navada@ti.com>
 *              Mehar Bajwa <mehar.bajwa@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/gpio.h>

#include <linux/mfd/tlv320aic3xxx-core.h>
#include <linux/mfd/tlv320aic3262-registers.h>

struct aic3262_gpio {
	unsigned int reg;
	u8 mask;
	u8 shift;
};
struct aic3262_gpio aic3262_gpio_control[] = {
	{
	 .reg = AIC3262_GPIO1_IO_CNTL,
	 .mask = AIC3262_GPIO_D6_D2,
	 .shift = AIC3262_GPIO_D2_SHIFT,
	 },
	{
	 .reg = AIC3262_GPIO2_IO_CNTL,
	 .mask = AIC3262_GPIO_D6_D2,
	 .shift = AIC3262_GPIO_D2_SHIFT,
	 },
	{
	 .reg = AIC3262_GPI1_EN,
	 .mask = AIC3262_GPI1_D2_D1,
	 .shift = AIC3262_GPIO_D1_SHIFT,
	 },
	{
	 .reg = AIC3262_GPI2_EN,
	 .mask = AIC3262_GPI2_D5_D4,
	 .shift = AIC3262_GPIO_D4_SHIFT,
	 },
	{
	 .reg = AIC3262_GPO1_OUT_CNTL,
	 .mask = AIC3262_GPO1_D4_D1,
	 .shift = AIC3262_GPIO_D1_SHIFT,
	 },
};

/*Codec read count limit once*/
#define CODEC_BULK_READ_MAX 128
/*Ap read conut limit once*/
#define CODEC_BULK_READ_LIMIT 63

#define CHECK_AIC3xxx_I2C_SHUTDOWN(a) { if (a && a->shutdown_complete) { \
dev_err(a->dev, "error: i2c state is 'shutdown'\n"); return -ENODEV; } }

int set_aic3xxx_book(struct aic3xxx *aic3xxx, int book)
{
	int ret = 0;
	u8 page_buf[] = { 0x0, 0x0 };
	u8 book_buf[] = { 0x7f, 0x0 };

	ret = regmap_write(aic3xxx->regmap, page_buf[0], page_buf[1]);

	if (ret < 0)
		return ret;
	book_buf[1] = book;
	ret = regmap_write(aic3xxx->regmap, book_buf[0], book_buf[1]);

	if (ret < 0)
		return ret;
	aic3xxx->book_no = book;
	aic3xxx->page_no = 0;

	return ret;
}

int set_aic3xxx_page(struct aic3xxx *aic3xxx, int page)
{
	int ret = 0;
	u8 page_buf[] = { 0x0, 0x0 };

	page_buf[1] = page;
	ret = regmap_write(aic3xxx->regmap, page_buf[0], page_buf[1]);

	if (ret < 0)
		return ret;
	aic3xxx->page_no = page;
	return ret;
}
/**
 * aic3xxx_reg_read: Read a single TLV320AIC3262 register.
 *
 * @aic3xxx: Device to read from.
 * @reg: Register to read.
 */
int aic3xxx_reg_read(struct aic3xxx *aic3xxx, unsigned int reg)
{
	unsigned int val;
	int ret;
	union aic3xxx_reg_union *aic_reg = (union aic3xxx_reg_union *) &reg;
	u8 book, page, offset;

	page = aic_reg->aic3xxx_register.page;
	book = aic_reg->aic3xxx_register.book;
	offset = aic_reg->aic3xxx_register.offset;

	mutex_lock(&aic3xxx->io_lock);
	CHECK_AIC3xxx_I2C_SHUTDOWN(aic3xxx)
	if (aic3xxx->book_no != book) {
		ret = set_aic3xxx_book(aic3xxx, book);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}

	if (aic3xxx->page_no != page) {
		ret = set_aic3xxx_page(aic3xxx, page);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	ret = regmap_read(aic3xxx->regmap, offset, &val);
	mutex_unlock(&aic3xxx->io_lock);

	if (ret < 0)
		return ret;
	else
		return val;
}
EXPORT_SYMBOL_GPL(aic3xxx_reg_read);

/**
 * aic3xxx_bulk_read: Read multiple TLV320AIC3262 registers
 *
 * @aic3xxx: Device to read from
 * @reg: First register
 * @count: Number of registers
 * @buf: Buffer to fill.  The data will be returned big endian.
 */
int aic3xxx_bulk_read(struct aic3xxx *aic3xxx, unsigned int reg,
		      int count, u8 *buf)
{
	int ret = 0;
	int count_temp = count;
	union aic3xxx_reg_union *aic_reg = (union aic3xxx_reg_union *) &reg;
	u8 book, page, offset;

	if (count > CODEC_BULK_READ_MAX)
		return -1;

	page = aic_reg->aic3xxx_register.page;
	book = aic_reg->aic3xxx_register.book;
	offset = aic_reg->aic3xxx_register.offset;

	mutex_lock(&aic3xxx->io_lock);
	CHECK_AIC3xxx_I2C_SHUTDOWN(aic3xxx)
	if (aic3xxx->book_no != book) {
		ret = set_aic3xxx_book(aic3xxx, book);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}

	if (aic3xxx->page_no != page) {
		ret = set_aic3xxx_page(aic3xxx, page);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}

	while (count_temp) {
		if (count_temp > CODEC_BULK_READ_LIMIT) {
			ret = regmap_bulk_read(aic3xxx->regmap, offset,
			buf, CODEC_BULK_READ_LIMIT);
			offset += CODEC_BULK_READ_LIMIT;
			buf += CODEC_BULK_READ_LIMIT;
			count_temp -= CODEC_BULK_READ_LIMIT;
		} else {
			ret = regmap_bulk_read(aic3xxx->regmap, offset,
			buf, count_temp);
			offset += count_temp;
			buf += count_temp;
			count_temp -= count_temp;
		}
	}

	mutex_unlock(&aic3xxx->io_lock);
		return ret;
}
EXPORT_SYMBOL_GPL(aic3xxx_bulk_read);

/**
 * aic3xxx_reg_write: Write a single TLV320AIC3262 register.
 *
 * @aic3xxx: Device to write to.
 * @reg: Register to write to.
 * @val: Value to write.
 */
int aic3xxx_reg_write(struct aic3xxx *aic3xxx, unsigned int reg,
		      unsigned char val)
{
	union aic3xxx_reg_union *aic_reg = (union aic3xxx_reg_union *) &reg;
	int ret = 0;
	u8 page, book, offset;

	page = aic_reg->aic3xxx_register.page;
	book = aic_reg->aic3xxx_register.book;
	offset = aic_reg->aic3xxx_register.offset;

	mutex_lock(&aic3xxx->io_lock);
	CHECK_AIC3xxx_I2C_SHUTDOWN(aic3xxx)
	if (book != aic3xxx->book_no) {
		ret = set_aic3xxx_book(aic3xxx, book);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	if (page != aic3xxx->page_no) {
		ret = set_aic3xxx_page(aic3xxx, page);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	ret = regmap_write(aic3xxx->regmap, offset, val);
	mutex_unlock(&aic3xxx->io_lock);
	return ret;

}
EXPORT_SYMBOL_GPL(aic3xxx_reg_write);

/**
 * aic3xxx_bulk_write: Write multiple TLV320AIC3262 registers
 *
 * @aic3xxx: Device to write to
 * @reg: First register
 * @count: Number of registers
 * @buf: Buffer to write from.  Data must be big-endian formatted.
 */
int aic3xxx_bulk_write(struct aic3xxx *aic3xxx, unsigned int reg,
		       int count, const u8 *buf)
{
	union aic3xxx_reg_union *aic_reg = (union aic3xxx_reg_union *) &reg;
	int ret = 0;
	u8 page, book, offset;

	page = aic_reg->aic3xxx_register.page;
	book = aic_reg->aic3xxx_register.book;
	offset = aic_reg->aic3xxx_register.offset;

	mutex_lock(&aic3xxx->io_lock);
	CHECK_AIC3xxx_I2C_SHUTDOWN(aic3xxx)
	if (book != aic3xxx->book_no) {
		ret = set_aic3xxx_book(aic3xxx, book);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	if (page != aic3xxx->page_no) {
		ret = set_aic3xxx_page(aic3xxx, page);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	ret = regmap_raw_write(aic3xxx->regmap, offset, buf, count);
	mutex_unlock(&aic3xxx->io_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(aic3xxx_bulk_write);

/**
 * aic3xxx_set_bits: Set the value of a bitfield in a TLV320AIC3262 register
 *
 * @aic3xxx: Device to write to.
 * @reg: Register to write to.
 * @mask: Mask of bits to set.
 * @val: Value to set (unshifted)
 */
int aic3xxx_set_bits(struct aic3xxx *aic3xxx, unsigned int reg,
		     unsigned char mask, unsigned char val)
{
	union aic3xxx_reg_union *aic_reg = (union aic3xxx_reg_union *) &reg;
	int ret = 0;
	u8 page, book, offset;

	page = aic_reg->aic3xxx_register.page;
	book = aic_reg->aic3xxx_register.book;
	offset = aic_reg->aic3xxx_register.offset;

	mutex_lock(&aic3xxx->io_lock);
	CHECK_AIC3xxx_I2C_SHUTDOWN(aic3xxx)
	if (book != aic3xxx->book_no) {
		ret = set_aic3xxx_book(aic3xxx, book);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	if (page != aic3xxx->page_no) {
		ret = set_aic3xxx_page(aic3xxx, page);
		if (ret < 0) {
			mutex_unlock(&aic3xxx->io_lock);
			return ret;
		}
	}
	ret = regmap_update_bits(aic3xxx->regmap, offset, mask, val);
	mutex_unlock(&aic3xxx->io_lock);
	return ret;

}
EXPORT_SYMBOL_GPL(aic3xxx_set_bits);

/**
 * aic3xxx_wait_bits: wait for a value of a bitfield in a TLV320AIC3262 register
 *
 * @aic3xxx: Device to write to.
 * @reg: Register to write to.
 * @mask: Mask of bits to set.
 * @val: Value to set (unshifted)
 * @mdelay: mdelay value in each iteration in milliseconds
 * @count: iteration count for timeout
 */
int aic3xxx_wait_bits(struct aic3xxx *aic3xxx, unsigned int reg,
		      unsigned char mask, unsigned char val, int sleep,
		      int counter)
{
	int status;
	int timeout = sleep * counter;

	status = aic3xxx_reg_read(aic3xxx, reg);
	while (((status & mask) != val) && counter) {
		mdelay(sleep);
		status = aic3xxx_reg_read(aic3xxx, reg);
		counter--;
	};
	if (!counter)
		dev_err(aic3xxx->dev,
			"wait_bits timedout (%d millisecs). lastval 0x%x val 0x%x\n",
			timeout, status, val);
	return counter;
}
EXPORT_SYMBOL_GPL(aic3xxx_wait_bits);

static struct mfd_cell aic3262_devs[] = {
	{
	 .name = "tlv320aic3262-codec",
	},
	{
	.name = "tlv320aic3262-gpio",
	},
};

static struct mfd_cell aic3285_devs[] = {
	{ .name = "tlv320aic3285-codec" },
	{ .name = "tlv320aic3285-extcon" },
	{ .name = "tlv320aic3285-gpio" },
};

/**
 * Instantiate the generic non-control parts of the device.
 */
int aic3xxx_device_init(struct aic3xxx *aic3xxx, int irq)
{
	struct aic3xxx_pdata *pdata = aic3xxx->dev->platform_data;
	const char *devname;
	int ret, i;
	u8 resetVal = 1;

	dev_info(aic3xxx->dev, "aic3xxx_device_init beginning\n");

	mutex_init(&aic3xxx->io_lock);
	dev_set_drvdata(aic3xxx->dev, aic3xxx);

	if (!pdata)
		return -EINVAL;

	/*GPIO reset for TLV320AIC3262 codec */
	if (pdata->gpio_reset) {
		ret = gpio_request(pdata->gpio_reset, "aic3xxx-reset-pin");
		if (ret != 0) {
			dev_err(aic3xxx->dev, "not able to acquire gpio\n");
			goto err_return;
		}
		gpio_direction_output(pdata->gpio_reset, 1);
		mdelay(5);
		gpio_direction_output(pdata->gpio_reset, 0);
		mdelay(5);
		gpio_direction_output(pdata->gpio_reset, 1);
		mdelay(5);
	}

	/* run the codec through software reset */
	ret = aic3xxx_reg_write(aic3xxx, AIC3262_RESET_REG, resetVal);
	if (ret < 0) {
		dev_err(aic3xxx->dev, "Could not write to AIC3262 register\n");
		goto err_return;
	}

	mdelay(10);

	ret = aic3xxx_reg_read(aic3xxx, AIC3262_DEVICE_ID);
	if (ret < 0) {
		dev_err(aic3xxx->dev, "Failed to read ID register\n");
		goto err_return;
	}

	switch (ret) {
	case 3:
		devname = "TLV320AIC3262";
		if (aic3xxx->type != TLV320AIC3262)
			dev_warn(aic3xxx->dev, "Device registered as type %d\n",
				 aic3xxx->type);
		aic3xxx->type = TLV320AIC3262;
		break;
	case 4:
		devname = "TLV320AIC3285";
		if (aic3xxx->type != TLV320AIC3285)
			dev_warn(aic3xxx->dev, "Device registered as type %d\n",
				 aic3xxx->type);
		aic3xxx->type = TLV320AIC3285;
	default:
		dev_err(aic3xxx->dev, "Device is not a TLV320AIC3262 type=%d",
			ret);
		ret = -EINVAL;
		goto err_return;
	}

	dev_info(aic3xxx->dev, "%s revision %c\n", devname, 'D' + ret);

	/*If naudint is gpio convert it to irq number */
	if (pdata->gpio_irq == 1) {
		aic3xxx->irq = gpio_to_irq(pdata->naudint_irq);
		gpio_request(pdata->naudint_irq, "aic3xxx-gpio-irq");
		gpio_direction_input(pdata->naudint_irq);
	} else
		aic3xxx->irq = pdata->naudint_irq;

	aic3xxx->irq_base = pdata->irq_base;
	for (i = 0; i < AIC3262_NUM_GPIO; i++) {
		if (pdata->gpio[i].used) {
			if (pdata->gpio[i].in) {
				aic3xxx_set_bits(aic3xxx,
						 aic3262_gpio_control[i].reg,
						 aic3262_gpio_control[i].mask,
						 0x1 << aic3262_gpio_control[i].
						 shift);
				if (pdata->gpio[i].in_reg) {
					aic3xxx_set_bits(aic3xxx,
							 pdata->gpio[i].in_reg,
							 pdata->gpio[i].
							 in_reg_bitmask,
							 pdata->gpio[i].
							 value << pdata->
							 gpio[i].in_reg_shift);
				}
			} else {
				aic3xxx_set_bits(aic3xxx,
						 aic3262_gpio_control[i].reg,
						 aic3262_gpio_control[i].mask,
						 pdata->gpio[i].
						 value <<
						 aic3262_gpio_control[i].shift);
			}
		} else
			aic3xxx_set_bits(aic3xxx, aic3262_gpio_control[i].reg,
					 aic3262_gpio_control[i].mask, 0x0);
	}
	aic3xxx->suspended = true;

	/* codec interrupt */
	if (aic3xxx->irq) {
		ret = aic3xxx_irq_init(aic3xxx);
		if (ret < 0)
			goto err_irq;
	}
	switch (aic3xxx->type) {
	case TLV320AIC3262:
		ret = mfd_add_devices(aic3xxx->dev, -1,
			      aic3262_devs, ARRAY_SIZE(aic3262_devs), NULL, 0);
		break;

	case TLV320AIC3285:
		ret = mfd_add_devices(aic3xxx->dev, -1,
			      aic3285_devs, ARRAY_SIZE(aic3285_devs), NULL, 0);
		break;
	case TLV320AIC3266:
		break;

	default:
		dev_err(aic3xxx->dev, "unable to recognize codec\n");
	}
	if (ret != 0) {
		dev_err(aic3xxx->dev, "Failed to add children: %d\n", ret);
		goto err_mfd;
	}
	dev_info(aic3xxx->dev, "aic3xxx_device_init added mfd devices\n");

	return 0;

err_mfd:

	aic3xxx_irq_exit(aic3xxx);
err_irq:

	if (pdata && pdata->gpio_irq)
		gpio_free(pdata->naudint_irq);
err_return:

	if (pdata && pdata->gpio_reset)
		gpio_free(pdata->gpio_reset);

	return ret;
}
EXPORT_SYMBOL_GPL(aic3xxx_device_init);

void aic3xxx_device_exit(struct aic3xxx *aic3xxx)
{
	struct aic3xxx_pdata *pdata = aic3xxx->dev->platform_data;

	pm_runtime_disable(aic3xxx->dev);
	mfd_remove_devices(aic3xxx->dev);
	aic3xxx_irq_exit(aic3xxx);

	if (pdata && pdata->gpio_irq)
		gpio_free(pdata->naudint_irq);
	if (pdata && pdata->gpio_reset)
		gpio_free(pdata->gpio_reset);

}
EXPORT_SYMBOL_GPL(aic3xxx_device_exit);

MODULE_AUTHOR("Mukund Navada <navada@ti.comm>");
MODULE_AUTHOR("Mehar Bajwa <mehar.bajwa@ti.com>");
MODULE_DESCRIPTION("Core support for the TLV320AIC3XXX audio CODEC");
MODULE_LICENSE("GPL");
