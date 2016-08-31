/*
 * tlv320aic325x-core.c  -- driver for TLV320AIC3XXX
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

#include <linux/mfd/tlv320aic325x-core.h>
#include <linux/mfd/tlv320aic325x-registers.h>
#include <linux/mfd/tlv320aic3256-registers.h>


/**
 * set_aic325x_book: change book which we have to write/read to.
 *
 * @aic325x: Device to write/read to.
 * @book: Book to write/read to.
 */
int set_aic325x_book(struct aic325x *aic325x, int book)
{
	int ret = 0;
	u8 page_buf[] = { 0x0, 0x0 };
	u8 book_buf[] = { 0x7f, 0x0 };

	ret = regmap_write(aic325x->regmap, page_buf[0], page_buf[1]);

	if (ret < 0)
		return ret;
	book_buf[1] = book;
	ret = regmap_write(aic325x->regmap, book_buf[0], book_buf[1]);

	if (ret < 0)
		return ret;
	aic325x->book_no = book;
	aic325x->page_no = 0;

	return ret;
}

/**
 * set_aic325x_page: change page which we have to write/read to.
 *
 * @aic325x: Device to write/read to.
 * @page: Book to write/read to.
 */
int set_aic325x_page(struct aic325x *aic325x, int page)
{
	int ret = 0;
	u8 page_buf[] = { 0x0, 0x0 };

	page_buf[1] = page;
	ret = regmap_write(aic325x->regmap, page_buf[0], page_buf[1]);

	if (ret < 0)
		return ret;
	aic325x->page_no = page;
	return ret;
}
/**
 * aic325x_reg_read: Read a single TLV320AIC3xxx register.
 *
 * @aic325x: Device to read from.
 * @reg: Register to read.
 */
int aic325x_reg_read(struct aic325x *aic325x, unsigned int reg)
{
	unsigned int val;
	int ret;
	union aic325x_reg_union *aic_reg = (union aic325x_reg_union *) &reg;
	u8 book, page, offset;

	page = aic_reg->aic325x_register.page;
	book = aic_reg->aic325x_register.book;
	offset = aic_reg->aic325x_register.offset;

	mutex_lock(&aic325x->io_lock);
	if (aic325x->book_no != book) {
		ret = set_aic325x_book(aic325x, book);
		if (ret < 0) {
			mutex_unlock(&aic325x->io_lock);
			return ret;
		}
	}

	if (aic325x->page_no != page) {
		ret = set_aic325x_page(aic325x, page);
		if (ret < 0) {
			mutex_unlock(&aic325x->io_lock);
			return ret;
		}
	}
	ret = regmap_read(aic325x->regmap, offset, &val);
	mutex_unlock(&aic325x->io_lock);

	if (ret < 0)
		return ret;
	else
		return val;
}
EXPORT_SYMBOL_GPL(aic325x_reg_read);

/**
 * aic325x_bulk_read: Read multiple TLV320AIC3XXX registers
 *
 * @aic325x: Device to read from
 * @reg: First register
 * @count: Number of registers
 * @buf: Buffer to fill.  The data will be returned big endian.
 */
int aic325x_bulk_read(struct aic325x *aic325x, unsigned int reg,
		      int count, u8 *buf)
{
	int ret;
	union aic325x_reg_union *aic_reg = (union aic325x_reg_union *) &reg;
	u8 book, page, offset;

	page = aic_reg->aic325x_register.page;
	book = aic_reg->aic325x_register.book;
	offset = aic_reg->aic325x_register.offset;

	mutex_lock(&aic325x->io_lock);
	if (aic325x->book_no != book) {
		ret = set_aic325x_book(aic325x, book);
		if (ret < 0) {
			mutex_unlock(&aic325x->io_lock);
			return ret;
		}
	}

	if (aic325x->page_no != page) {
		ret = set_aic325x_page(aic325x, page);
		if (ret < 0) {
			mutex_unlock(&aic325x->io_lock);
			return ret;
		}
	}
	ret = regmap_bulk_read(aic325x->regmap, offset, buf, count);
	mutex_unlock(&aic325x->io_lock);
		return ret;
}
EXPORT_SYMBOL_GPL(aic325x_bulk_read);

/**
 * aic325x_reg_write: Write a single TLV320AIC3XXX register.
 *
 * @aic325x: Device to write to.
 * @reg: Register to write to.
 * @val: Value to write.
 */
int aic325x_reg_write(struct aic325x *aic325x, unsigned int reg,
		      unsigned char val)
{
	union aic325x_reg_union *aic_reg = (union aic325x_reg_union *) &reg;
	int ret = 0;
	u8 page, book, offset;

	page = aic_reg->aic325x_register.page;
	book = aic_reg->aic325x_register.book;
	offset = aic_reg->aic325x_register.offset;

	mutex_lock(&aic325x->io_lock);
	if (book != aic325x->book_no) {
		ret = set_aic325x_book(aic325x, book);
		if (ret < 0) {
			mutex_unlock(&aic325x->io_lock);
			return ret;
		}
	}
	if (page != aic325x->page_no) {
		ret = set_aic325x_page(aic325x, page);
		if (ret < 0) {
			mutex_unlock(&aic325x->io_lock);
			return ret;
		}
	}
	ret = regmap_write(aic325x->regmap, offset, val);
	mutex_unlock(&aic325x->io_lock);
	return ret;

}
EXPORT_SYMBOL_GPL(aic325x_reg_write);

/**
 * aic325x_bulk_write: Write multiple TLV320AIC3XXX registers
 *
 * @aic325x: Device to write to
 * @reg: First register
 * @count: Number of registers
 * @buf: Buffer to write from.  Data must be big-endian formatted.
 */
int aic325x_bulk_write(struct aic325x *aic325x, unsigned int reg,
		       int count, const u8 *buf)
{
	union aic325x_reg_union *aic_reg = (union aic325x_reg_union *) &reg;
	int ret = 0;
	u8 page, book, offset;

	page = aic_reg->aic325x_register.page;
	book = aic_reg->aic325x_register.book;
	offset = aic_reg->aic325x_register.offset;

	mutex_lock(&aic325x->io_lock);
	if (book != aic325x->book_no) {
		ret = set_aic325x_book(aic325x, book);
		if (ret < 0) {
			mutex_unlock(&aic325x->io_lock);
			return ret;
		}
	}
	if (page != aic325x->page_no) {
		ret = set_aic325x_page(aic325x, page);
		if (ret < 0) {
			mutex_unlock(&aic325x->io_lock);
			return ret;
		}
	}
	ret = regmap_raw_write(aic325x->regmap, offset, buf, count);
	mutex_unlock(&aic325x->io_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(aic325x_bulk_write);

/**
 * aic325x_set_bits: Set the value of a bitfield in a TLV320AIC3XXX register
 *
 * @aic325x: Device to write to.
 * @reg: Register to write to.
 * @mask: Mask of bits to set.
 * @val: Value to set (unshifted)
 */
int aic325x_set_bits(struct aic325x *aic325x, unsigned int reg,
		     unsigned char mask, unsigned char val)
{
	union aic325x_reg_union *aic_reg = (union aic325x_reg_union *) &reg;
	int ret = 0;
	u8 page, book, offset;

	page = aic_reg->aic325x_register.page;
	book = aic_reg->aic325x_register.book;
	offset = aic_reg->aic325x_register.offset;

	mutex_lock(&aic325x->io_lock);
	if (book != aic325x->book_no) {
		ret = set_aic325x_book(aic325x, book);
		if (ret < 0) {
			mutex_unlock(&aic325x->io_lock);
			return ret;
		}
	}
	if (page != aic325x->page_no) {
		ret = set_aic325x_page(aic325x, page);
		if (ret < 0) {
			mutex_unlock(&aic325x->io_lock);
			return ret;
		}
	}
	ret = regmap_update_bits(aic325x->regmap, offset, mask, val);
	mutex_unlock(&aic325x->io_lock);
	return ret;

}
EXPORT_SYMBOL_GPL(aic325x_set_bits);

/**
 * aic325x_wait_bits: wait for a value of a bitfield in a TLV320AIC3XXX register
 *
 * @aic325x: Device to write to.
 * @reg: Register to write to.
 * @mask: Mask of bits to set.
 * @val: Value to set (unshifted)
 * @sleep: delay value in each iteration in micro seconds
 * @count: iteration count for timeout
 */
int aic325x_wait_bits(struct aic325x *aic325x, unsigned int reg,
		      unsigned char mask, unsigned char val, int sleep,
		      int counter)
{
	int status;
	int timeout = sleep * counter;

	status = aic325x_reg_read(aic325x, reg);
	while (((status & mask) != val) && counter) {
		usleep_range(sleep, sleep + 500);
		status = aic325x_reg_read(aic325x, reg);
		counter--;
	};
	if (!counter)
		dev_err(aic325x->dev,
			"wait_bits timedout (%d millisecs). lastval 0x%x\n",
			timeout, status);
	return counter;
}
EXPORT_SYMBOL_GPL(aic325x_wait_bits);

static struct mfd_cell aic3262_devs[] = {
	{
	.name = "tlv320aic3262-codec",
	},
	{
	.name = "tlv320aic3262-gpio",
	},
};
static struct mfd_cell aic3256_devs[] = {
	{
		.name = "tlv320aic325x-codec",
	},
	{
		.name = "tlv320aic3256-gpio",
	},
};

/**
 * Instantiate the generic non-control parts of the device.
 */
int aic325x_device_init(struct aic325x *aic325x)
{
	const char *devname;
	int ret, i;
	u8 reset = 1;

	dev_info(aic325x->dev, "aic325x_device_init beginning\n");
	mutex_init(&aic325x->io_lock);
	dev_set_drvdata(aic325x->dev, aic325x);

	if (dev_get_platdata(aic325x->dev))
		memcpy(&aic325x->pdata, dev_get_platdata(aic325x->dev),
			sizeof(aic325x->pdata));

	/* GPIO reset for TLV320AIC3xxx codec */
	if (aic325x->pdata.gpio_reset) {
		ret = gpio_request_one(aic325x->pdata.gpio_reset,
					GPIOF_DIR_OUT | GPIOF_INIT_LOW,
					"aic325x-reset-pin");
		if (ret != 0) {
			dev_err(aic325x->dev, "not able to acquire gpio\n");
			goto err_return;
		}
	}

	/* run the codec through software reset */
	ret = aic325x_reg_write(aic325x, AIC3XXX_RESET, reset);
	if (ret < 0) {
		dev_err(aic325x->dev, "Could not write to AIC3XXX register\n");
		goto err_return;
	}

	usleep_range(10000, 10500);

	ret = aic325x_reg_read(aic325x, AIC3XXX_DEVICE_ID);
	if (ret < 0) {
		dev_err(aic325x->dev, "Failed to read ID register\n");
		goto err_return;
	}
	devname = "TLV320AIC3256";
	aic325x->type = TLV320AIC3256;

	/*If naudint is gpio convert it to irq number */
	if (aic325x->pdata.gpio_irq == 1) {
		aic325x->irq = gpio_to_irq(aic325x->pdata.naudint_irq);
		gpio_request(aic325x->pdata.naudint_irq, "aic325x-gpio-irq");
		gpio_direction_input(aic325x->pdata.naudint_irq);
	} else {
		aic325x->irq = aic325x->pdata.naudint_irq;
	}

	for (i = 0; i < aic325x->pdata.num_gpios; i++) {
		aic325x_reg_write(aic325x, aic325x->pdata.gpio_defaults[i].reg,
			aic325x->pdata.gpio_defaults[i].value);
	}

	if (aic325x->irq) {
		ret = aic325x_irq_init(aic325x);
		if (ret < 0)
			goto err_irq;
	}


	dev_info(aic325x->dev, "%s revision %c\n", devname, 'D' + ret);

	switch (aic325x->type) {
	case TLV320AIC3266:
	case TLV320AIC3262:
		ret = mfd_add_devices(aic325x->dev, -1, aic3262_devs,
			      ARRAY_SIZE(aic3262_devs), NULL, 0, NULL);
		break;
	case TLV320AIC3256:
		ret = mfd_add_devices(aic325x->dev, -1, aic3256_devs,
		              ARRAY_SIZE(aic3256_devs), NULL, 0, NULL);
		break;
	default:
		dev_err(aic325x->dev, "unable to recognize codec\n");
		break;
	}
	if (ret != 0) {
		dev_err(aic325x->dev, "Failed to add children: %d\n", ret);
		goto err_mfd;
	}
	dev_info(aic325x->dev, "aic325x_device_init added mfd devices\n");

	return 0;

err_mfd:

	aic325x_irq_exit(aic325x);
err_irq:

	if (aic325x->pdata.gpio_irq)
		gpio_free(aic325x->pdata.naudint_irq);
err_return:

	if (aic325x->pdata.gpio_reset)
		gpio_free(aic325x->pdata.gpio_reset);

	return ret;
}
EXPORT_SYMBOL_GPL(aic325x_device_init);

void aic325x_device_exit(struct aic325x *aic325x)
{

	mfd_remove_devices(aic325x->dev);
	aic325x_irq_exit(aic325x);

	if (aic325x->pdata.gpio_irq)
		gpio_free(aic325x->pdata.naudint_irq);
	if (aic325x->pdata.gpio_reset)
		gpio_free(aic325x->pdata.gpio_reset);

}
EXPORT_SYMBOL_GPL(aic325x_device_exit);

MODULE_AUTHOR("Mukund Navada <navada@ti.comm>");
MODULE_AUTHOR("Mehar Bajwa <mehar.bajwa@ti.com>");
MODULE_DESCRIPTION("Core support for the TLV320AIC3XXX audio CODEC");
MODULE_LICENSE("GPL");
