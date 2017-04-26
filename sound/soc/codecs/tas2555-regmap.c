/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2016 XiaoMi, Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along with
** this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
** Street, Fifth Floor, Boston, MA 02110-1301, USA.
**
** File:
**     tas2555-regmap.c
**
** Description:
**     I2C driver with regmap for Texas Instruments TAS2555 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#define DEBUG
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include "tas2555.h"
#include "tas2555-core.h"
#include "tas2555-codec.h"

#ifdef CONFIG_TAS2555_MISC
#include "tas2555-misc.h"
#endif

#define ENABLE_GPIO_RESET

#define ENABLE_TILOAD
#ifdef ENABLE_TILOAD
#include "tiload.h"
#endif

static void tas2555_change_book_page(struct tas2555_priv *pTAS2555, int nBook,
	int nPage)
{
	if ((pTAS2555->mnCurrentBook == nBook)
		&& pTAS2555->mnCurrentPage == nPage){
		return;
	}

	if (pTAS2555->mnCurrentBook != nBook) {
		regmap_write(pTAS2555->mpRegmap, TAS2555_BOOKCTL_PAGE, 0);
		pTAS2555->mnCurrentPage = 0;
		regmap_write(pTAS2555->mpRegmap, TAS2555_BOOKCTL_REG, nBook);
		pTAS2555->mnCurrentBook = nBook;
		if (nPage != 0) {
			regmap_write(pTAS2555->mpRegmap, TAS2555_BOOKCTL_PAGE, nPage);
			pTAS2555->mnCurrentPage = nPage;
		}
	} else if (pTAS2555->mnCurrentPage != nPage) {
		regmap_write(pTAS2555->mpRegmap, TAS2555_BOOKCTL_PAGE, nPage);
		pTAS2555->mnCurrentPage = nPage;
	}
}

static int tas2555_dev_read(struct tas2555_priv *pTAS2555,
	unsigned int nRegister, unsigned int *pValue)
{
	int ret = 0;

	mutex_lock(&pTAS2555->dev_lock);

	if (pTAS2555->mbTILoadActive) {
		if (!(nRegister & 0x80000000)) {
			mutex_unlock(&pTAS2555->dev_lock);
			return 0;
		}
		nRegister &= ~0x80000000;
	}

	tas2555_change_book_page(pTAS2555, TAS2555_BOOK_ID(nRegister),
		TAS2555_PAGE_ID(nRegister));
	ret = regmap_read(pTAS2555->mpRegmap, TAS2555_PAGE_REG(nRegister), pValue);

	mutex_unlock(&pTAS2555->dev_lock);
	return ret;
}

static int tas2555_dev_write(struct tas2555_priv *pTAS2555,
	unsigned int nRegister, unsigned int nValue)
{
	int ret = 0;

	mutex_lock(&pTAS2555->dev_lock);
	if ((nRegister == 0xAFFEAFFE) && (nValue == 0xBABEBABE)) {
		pTAS2555->mbTILoadActive = true;
		mutex_unlock(&pTAS2555->dev_lock);
		return 0;
	}

	if ((nRegister == 0xBABEBABE) && (nValue == 0xAFFEAFFE)) {
		pTAS2555->mbTILoadActive = false;
		mutex_unlock(&pTAS2555->dev_lock);
		return 0;
	}

	if (pTAS2555->mbTILoadActive) {
		if (!(nRegister & 0x80000000)) {
			mutex_unlock(&pTAS2555->dev_lock);
			return 0;
		}
		nRegister &= ~0x80000000;
	}

	tas2555_change_book_page(pTAS2555, TAS2555_BOOK_ID(nRegister),
		TAS2555_PAGE_ID(nRegister));
	ret = regmap_write(pTAS2555->mpRegmap, TAS2555_PAGE_REG(nRegister),
		nValue);
	mutex_unlock(&pTAS2555->dev_lock);

	return ret;
}

static int tas2555_dev_bulk_read(struct tas2555_priv *pTAS2555,
	unsigned int nRegister, u8 *pData, unsigned int nLength)
{
	int ret = 0;

	mutex_lock(&pTAS2555->dev_lock);
	if (pTAS2555->mbTILoadActive) {
		if (!(nRegister & 0x80000000)) {
			mutex_unlock(&pTAS2555->dev_lock);
			return 0;
		}
		nRegister &= ~0x80000000;
	}

	tas2555_change_book_page(pTAS2555, TAS2555_BOOK_ID(nRegister),
		TAS2555_PAGE_ID(nRegister));

	ret = regmap_bulk_read(pTAS2555->mpRegmap, TAS2555_PAGE_REG(nRegister),
		pData, nLength);
	mutex_unlock(&pTAS2555->dev_lock);

	return ret;
}

static int tas2555_dev_bulk_write(struct tas2555_priv *pTAS2555,
	unsigned int nRegister, u8 *pData, unsigned int nLength)
{
	int ret = 0;

	mutex_lock(&pTAS2555->dev_lock);
	if (pTAS2555->mbTILoadActive) {
		if (!(nRegister & 0x80000000)) {
			mutex_unlock(&pTAS2555->dev_lock);
			return 0;
		}
		nRegister &= ~0x80000000;
	}

	tas2555_change_book_page(pTAS2555, TAS2555_BOOK_ID(nRegister),
		TAS2555_PAGE_ID(nRegister));
	ret = regmap_bulk_write(pTAS2555->mpRegmap, TAS2555_PAGE_REG(nRegister),
		pData, nLength);
	mutex_unlock(&pTAS2555->dev_lock);

	return ret;
}

static int tas2555_dev_update_bits(struct tas2555_priv *pTAS2555,
	unsigned int nRegister, unsigned int nMask, unsigned int nValue)
{
	int ret = 0;

	mutex_lock(&pTAS2555->dev_lock);

	if (pTAS2555->mbTILoadActive) {
		if (!(nRegister & 0x80000000)) {
			mutex_unlock(&pTAS2555->dev_lock);
			return 0;
		}
		nRegister &= ~0x80000000;
	}

	tas2555_change_book_page(pTAS2555, TAS2555_BOOK_ID(nRegister),
		TAS2555_PAGE_ID(nRegister));

	ret = regmap_update_bits(pTAS2555->mpRegmap, TAS2555_PAGE_REG(nRegister), nMask, nValue);

	mutex_unlock(&pTAS2555->dev_lock);
	return ret;
}

static bool tas2555_volatile(struct device *pDev, unsigned int nRegister)
{
	return true;
}

static bool tas2555_writeable(struct device *pDev, unsigned int nRegister)
{
	return true;
}

static const struct regmap_config tas2555_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.writeable_reg = tas2555_writeable,
	.volatile_reg = tas2555_volatile,
	.cache_type = REGCACHE_NONE,
	.max_register = 128,
};

static int tas2555_i2c_probe(struct i2c_client *pClient,
	const struct i2c_device_id *pID)
{
	struct tas2555_priv *pTAS2555;
	const char *fw_name;
	unsigned int n;
	int spkr_id;
	int nResult;

	dev_info(&pClient->dev, "%s enter\n", __FUNCTION__);

	pTAS2555 = devm_kzalloc(&pClient->dev, sizeof(struct tas2555_priv), GFP_KERNEL);
	if (!pTAS2555)
		return -ENOMEM;

	pTAS2555->dev = &pClient->dev;
	i2c_set_clientdata(pClient, pTAS2555);
	dev_set_drvdata(&pClient->dev, pTAS2555);

#ifdef ENABLE_GPIO_RESET
	pTAS2555->reset_gpio =
		of_get_named_gpio(pClient->dev.of_node, "ti,reset-gpio", 0);
	dev_info(&pClient->dev, "reset gpio is %d\n", pTAS2555->reset_gpio);
	if (gpio_is_valid(pTAS2555->reset_gpio)) {
		devm_gpio_request_one(&pClient->dev, pTAS2555->reset_gpio,
			GPIOF_OUT_INIT_LOW, "TAS2555_RST");
		msleep(10);
		gpio_set_value_cansleep(pTAS2555->reset_gpio, 1);
		udelay(1000);
	}
#endif

	pTAS2555->spkr_id_gpio = of_get_named_gpio(pClient->dev.of_node, "spkr-id-gpio", 0);
	if (pTAS2555->spkr_id_gpio < 0) {
		dev_info(&pClient->dev, "property %s not detected in node %s",
			"spkr-id-gpio", pClient->dev.of_node->full_name);
		spkr_id = -1;
	} else {
		spkr_id = tas2555_get_speaker_id(pTAS2555->spkr_id_gpio);
		dev_dbg(&pClient->dev, "speaker id is %d\n", spkr_id);
	}

	pTAS2555->pinctrl = devm_pinctrl_get(&pClient->dev);
	if (IS_ERR_OR_NULL(pTAS2555->pinctrl)) {
		dev_info(&pClient->dev, "%s: Unable to get pinctrl handle\n", __func__);
		pTAS2555->pinctrl = NULL;
	} else {
		tas2555_set_pinctrl(pTAS2555, false);

		pTAS2555->mclk = clk_get(&pClient->dev, "tas2555-mclk");
		if (IS_ERR(pTAS2555->mclk)) {
			dev_info(&pClient->dev, "mclk can't be found\n");
			pTAS2555->mclk = NULL;
		}
	}

	pTAS2555->mpRegmap = devm_regmap_init_i2c(pClient, &tas2555_i2c_regmap);
	if (IS_ERR(pTAS2555->mpRegmap)) {
		nResult = PTR_ERR(pTAS2555->mpRegmap);
		dev_err(&pClient->dev, "Failed to allocate register map: %d\n",
			nResult);
		return nResult;
	}

	pTAS2555->read = tas2555_dev_read;
	pTAS2555->write = tas2555_dev_write;
	pTAS2555->bulk_read = tas2555_dev_bulk_read;
	pTAS2555->bulk_write = tas2555_dev_bulk_write;
	pTAS2555->update_bits = tas2555_dev_update_bits;
	pTAS2555->set_mode = tas2555_set_mode;
	pTAS2555->set_calibration = tas2555_set_calibration;

	mutex_init(&pTAS2555->dev_lock);

	/* Reset the chip */
	nResult = tas2555_dev_write(pTAS2555, TAS2555_SW_RESET_REG, 0x01);
	if (nResult < 0) {
		dev_err(&pClient->dev, "I2C communication ERROR: %d\n",
			nResult);
		return nResult;
	}

	udelay(1000);

	pTAS2555->mpFirmware =
		devm_kzalloc(&pClient->dev, sizeof(TFirmware),
		GFP_KERNEL);
	if (!pTAS2555->mpFirmware)
		return -ENOMEM;

	pTAS2555->mpCalFirmware =
		devm_kzalloc(&pClient->dev, sizeof(TFirmware),
		GFP_KERNEL);
	if (!pTAS2555->mpCalFirmware)
		return -ENOMEM;

	pTAS2555->mnCurrentPage = 0;
	pTAS2555->mnCurrentBook = 0;

	nResult = tas2555_dev_read(pTAS2555, TAS2555_REV_PGID_REG, &n);
	dev_info(&pClient->dev, "TAS2555 PGID: 0x%02x\n", n);

	tas2555_load_default(pTAS2555);

	pTAS2555->mbTILoadActive = false;

	mutex_init(&pTAS2555->codec_lock);
	tas2555_register_codec(pTAS2555);

#ifdef CONFIG_TAS2555_MISC
	mutex_init(&pTAS2555->file_lock);
	tas2555_register_misc(pTAS2555);
#endif

#ifdef ENABLE_TILOAD
	tiload_driver_init(pTAS2555);
#endif

	fw_name = (spkr_id == 3) ? TAS2555_FW_NAME_GOER : TAS2555_FW_NAME_AAC;
	dev_info(&pClient->dev, "loading firmware: %s\n", fw_name);
	nResult = request_firmware_nowait(THIS_MODULE, 1, fw_name,
		pTAS2555->dev, GFP_KERNEL, pTAS2555, tas2555_fw_ready);

	return nResult;
}

static int tas2555_i2c_remove(struct i2c_client *pClient)
{
	struct tas2555_priv *pTAS2555 = i2c_get_clientdata(pClient);

	dev_info(pTAS2555->dev, "%s\n", __FUNCTION__);

	tas2555_deregister_codec(pTAS2555);
	mutex_destroy(&pTAS2555->codec_lock);

#ifdef CONFIG_TAS2555_MISC
	tas2555_deregister_misc(pTAS2555);
	mutex_destroy(&pTAS2555->file_lock);
#endif

	return 0;
}

static const struct i2c_device_id tas2555_i2c_id[] = {
	{"tas2555", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, tas2555_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id tas2555_of_match[] = {
	{.compatible = "ti,tas2555"},
	{},
};

MODULE_DEVICE_TABLE(of, tas2555_of_match);
#endif

static struct i2c_driver tas2555_i2c_driver = {
	.driver = {
		.name = "tas2555",
		.owner = THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(tas2555_of_match),
#endif
	},
	.probe = tas2555_i2c_probe,
	.remove = tas2555_i2c_remove,
	.id_table = tas2555_i2c_id,
};

module_i2c_driver(tas2555_i2c_driver);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2555 I2C Smart Amplifier driver");
MODULE_LICENSE("GPLv2");
