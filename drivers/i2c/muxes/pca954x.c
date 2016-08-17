/*
 * I2C multiplexer
 *
 * Copyright (c) 2008-2009 Rodolfo Giometti <giometti@linux.it>
 * Copyright (c) 2008-2009 Eurotech S.p.A. <info@eurotech.it>
 *
 * This module supports the PCA954x series of I2C multiplexer/switch chips
 * made by Philips Semiconductors.
 * This includes the:
 *	 PCA9540, PCA9542, PCA9543, PCA9544, PCA9545, PCA9546, PCA9547
 *	 and PCA9548.
 *
 * These chips are all controlled via the I2C bus itself, and all have a
 * single 8-bit register. The upstream "parent" bus fans out to two,
 * four, or eight downstream busses or channels; which of these
 * are selected is determined by the chip type and register contents. A
 * mux can select only one sub-bus at a time; a switch can select any
 * combination simultaneously.
 *
 * Based on:
 *	pca954x.c from Kumar Gala <galak@kernel.crashing.org>
 * Copyright (C) 2006
 *
 * Based on:
 *	pca954x.c from Ken Harrenstien
 * Copyright (C) 2004 Google, Inc. (Ken Harrenstien)
 *
 * Based on:
 *	i2c-virtual_cb.c from Brian Kuschak <bkuschak@yahoo.com>
 * and
 *	pca9540.c from Jean Delvare <khali@linux-fr.org>.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/i2c-mux.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <linux/i2c/pca954x.h>

#define PCA954X_MAX_NCHANS 8

enum pca_type {
	pca_9540,
	pca_9542,
	pca_9543,
	pca_9544,
	pca_9545,
	pca_9546,
	pca_9547,
	pca_9548,
};

struct pca954x {
	enum pca_type type;
	struct i2c_adapter *virt_adaps[PCA954X_MAX_NCHANS];

	u8 last_chan;		/* last register value */
	struct regulator *vcc_reg;
	struct regulator *i2c_reg;
};

struct chip_desc {
	u8 nchans;
	u8 enable;	/* used for muxes only */
	enum muxtype {
		pca954x_ismux = 0,
		pca954x_isswi
	} muxtype;
};

/* Provide specs for the PCA954x types we know about */
static const struct chip_desc chips[] = {
	[pca_9540] = {
		.nchans = 2,
		.enable = 0x4,
		.muxtype = pca954x_ismux,
	},
	[pca_9543] = {
		.nchans = 2,
		.muxtype = pca954x_isswi,
	},
	[pca_9544] = {
		.nchans = 4,
		.enable = 0x4,
		.muxtype = pca954x_ismux,
	},
	[pca_9545] = {
		.nchans = 4,
		.muxtype = pca954x_isswi,
	},
	[pca_9547] = {
		.nchans = 8,
		.enable = 0x8,
		.muxtype = pca954x_ismux,
	},
	[pca_9548] = {
		.nchans = 8,
		.muxtype = pca954x_isswi,
	},
};

static const struct i2c_device_id pca954x_id[] = {
	{ "pca9540", pca_9540 },
	{ "pca9542", pca_9540 },
	{ "pca9543", pca_9543 },
	{ "pca9544", pca_9544 },
	{ "pca9545", pca_9545 },
	{ "pca9546", pca_9545 },
	{ "pca9547", pca_9547 },
	{ "pca9548", pca_9548 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pca954x_id);

/* Write to mux register. Don't use i2c_transfer()/i2c_smbus_xfer()
   for this as they will try to lock adapter a second time */
static int pca954x_reg_write(struct i2c_adapter *adap,
			     struct i2c_client *client, u8 val)
{
	int ret = -ENODEV;
	struct pca954x *data = i2c_get_clientdata(client);

	/* Increase ref count for pca954x vcc */
	if (data->vcc_reg) {
		ret = regulator_enable(data->vcc_reg);
		if (ret) {
			dev_err(&client->dev, "%s: failed to enable vcc\n",
				__func__);
			goto vcc_regulator_failed;
		}
	}
	/* Increase ref count for pca954x vcc_i2c */
	if (data->i2c_reg) {
		ret = regulator_enable(data->i2c_reg);
		if (ret) {
			dev_err(&client->dev, "%s: failed to enable vcc_i2c\n",
				__func__);
			goto i2c_regulator_failed;
		}
	}

	if (adap->algo->master_xfer) {
		struct i2c_msg msg;
		char buf[1];

		msg.addr = client->addr;
		msg.flags = 0;
		msg.len = 1;
		buf[0] = val;
		msg.buf = buf;
		ret = adap->algo->master_xfer(adap, &msg, 1);
	} else {
		union i2c_smbus_data data;
		ret = adap->algo->smbus_xfer(adap, client->addr,
					     client->flags,
					     I2C_SMBUS_WRITE,
					     val, I2C_SMBUS_BYTE, &data);
	}

	/* Decrease ref count for pca954x vcc_i2c */
	if (data->i2c_reg)
		regulator_disable(data->i2c_reg);

i2c_regulator_failed:
	/* Decrease ref count for pca954x vcc */
	if (data->vcc_reg)
		regulator_disable(data->vcc_reg);
vcc_regulator_failed:
	return ret;
}

static int pca954x_select_chan(struct i2c_adapter *adap,
			       void *client, u32 chan)
{
	struct pca954x *data = i2c_get_clientdata(client);
	const struct chip_desc *chip = &chips[data->type];
	u8 regval;
	int ret = 0;

	/* we make switches look like muxes, not sure how to be smarter */
	if (chip->muxtype == pca954x_ismux)
		regval = chan | chip->enable;
	else
		regval = 1 << chan;

	/* Only select the channel if its different from the last channel */
	if (data->last_chan != regval) {
		ret = pca954x_reg_write(adap, client, regval);
		data->last_chan = regval;
	}

	return ret;
}

static int pca954x_deselect_mux(struct i2c_adapter *adap,
				void *client, u32 chan)
{
	struct pca954x *data = i2c_get_clientdata(client);

	/* Deselect active channel */
	data->last_chan = 0;
	return pca954x_reg_write(adap, client, data->last_chan);
}

/*
 * I2C init/probing/exit functions
 */
static int pca954x_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adap = to_i2c_adapter(client->dev.parent);
	struct pca954x_platform_data *pdata = client->dev.platform_data;
	int num, force;
	struct pca954x *data;
	int ret = -ENODEV;

	if (!i2c_check_functionality(adap, I2C_FUNC_SMBUS_BYTE))
		goto err;

	data = kzalloc(sizeof(struct pca954x), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}

	i2c_set_clientdata(client, data);

	/* Get regulator pointer for pca954x vcc */
	data->vcc_reg = regulator_get(&client->dev, "vcc");
	if (PTR_ERR(data->vcc_reg) == -EPROBE_DEFER)
		data->vcc_reg = NULL;
	else if (IS_ERR(data->vcc_reg)) {
		dev_err(&client->dev, "%s: failed to get vcc\n",
			__func__);
		ret = PTR_ERR(data->vcc_reg);
		goto exit_free;
	}
	/* Get regulator pointer for pca954x vcc_i2c */
	data->i2c_reg = regulator_get(&client->dev, "vcc_i2c");
	if (PTR_ERR(data->i2c_reg) == -EPROBE_DEFER)
		data->i2c_reg = NULL;
	else if (IS_ERR(data->i2c_reg)) {
		dev_err(&client->dev, "%s: failed to get vcc_i2c\n",
			__func__);
		ret = PTR_ERR(data->i2c_reg);
		regulator_put(data->vcc_reg);
		goto exit_free;
	}

	/* Increase ref count for pca954x vcc */
	if (data->vcc_reg) {
		pr_info("%s: enable vcc\n", __func__);
		ret = regulator_enable(data->vcc_reg);
		if (ret) {
			dev_err(&client->dev, "%s: failed to enable vcc\n",
				__func__);
			goto exit_regulator_put;
		}
	}
	/* Increase ref count for pca954x vcc_i2c */
	if (data->i2c_reg) {
		pr_info("%s: enable vcc_i2c\n", __func__);
		ret = regulator_enable(data->i2c_reg);
		if (ret) {
			dev_err(&client->dev, "%s: failed to enable vcc_i2c\n",
				__func__);
			goto exit_vcc_regulator_disable;
		}
	}

	/*
	 * Power-On Reset takes time.
	 * I2C is ready after Power-On Reset.
	 */
	msleep(1);

	/* Write the mux register at addr to verify
	 * that the mux is in fact present. This also
	 * initializes the mux to disconnected state.
	 */
	if (i2c_smbus_write_byte(client, 0) < 0) {
		dev_warn(&client->dev, "probe failed\n");
		goto exit_regulator_disable;
	}

	/* Decrease ref count for pca954x vcc */
	if (data->vcc_reg)
		regulator_disable(data->vcc_reg);
	/* Decrease ref count for pca954x vcc_i2c */
	if (data->i2c_reg)
		regulator_disable(data->i2c_reg);

	data->type = id->driver_data;
	data->last_chan = 0;		   /* force the first selection */

	/* Now create an adapter for each channel */
	for (num = 0; num < chips[data->type].nchans; num++) {
		force = 0;			  /* dynamic adap number */
		if (pdata) {
			if (num < pdata->num_modes)
				/* force static number */
				force = pdata->modes[num].adap_id;
			else
				/* discard unconfigured channels */
				break;
		}

		data->virt_adaps[num] =
			i2c_add_mux_adapter(adap, client,
				force, num, pca954x_select_chan,
				(pdata && pdata->modes[num].deselect_on_exit)
					? pca954x_deselect_mux : NULL);

		if (data->virt_adaps[num] == NULL) {
			ret = -ENODEV;
			dev_err(&client->dev,
				"failed to register multiplexed adapter"
				" %d as bus %d\n", num, force);
			goto virt_reg_failed;
		}
	}

	dev_info(&client->dev,
		 "registered %d multiplexed busses for I2C %s %s\n",
		 num, chips[data->type].muxtype == pca954x_ismux
				? "mux" : "switch", client->name);

	return 0;

virt_reg_failed:
	for (num--; num >= 0; num--)
		i2c_del_mux_adapter(data->virt_adaps[num]);
exit_regulator_disable:
	if (data->i2c_reg)
		regulator_disable(data->i2c_reg);
exit_vcc_regulator_disable:
	if (data->vcc_reg)
		regulator_disable(data->vcc_reg);
exit_regulator_put:
	regulator_put(data->i2c_reg);
	regulator_put(data->vcc_reg);
exit_free:
	kfree(data);
err:
	return ret;
}

static int pca954x_remove(struct i2c_client *client)
{
	struct pca954x *data = i2c_get_clientdata(client);
	const struct chip_desc *chip = &chips[data->type];
	int i, err;

	for (i = 0; i < chip->nchans; ++i)
		if (data->virt_adaps[i]) {
			err = i2c_del_mux_adapter(data->virt_adaps[i]);
			if (err)
				return err;
			data->virt_adaps[i] = NULL;
		}

	regulator_put(data->i2c_reg);
	regulator_put(data->vcc_reg);

	kfree(data);
	return 0;
}

static struct i2c_driver pca954x_driver = {
	.driver		= {
		.name	= "pca954x",
		.owner	= THIS_MODULE,
	},
	.probe		= pca954x_probe,
	.remove		= pca954x_remove,
	.id_table	= pca954x_id,
};

module_i2c_driver(pca954x_driver);

MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("PCA954x I2C mux/switch driver");
MODULE_LICENSE("GPL v2");
