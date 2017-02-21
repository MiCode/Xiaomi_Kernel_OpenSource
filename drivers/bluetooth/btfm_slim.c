/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <btfm_slim.h>
#include <btfm_slim_wcn3990.h>
#include <linux/bluetooth-power.h>

int btfm_slim_write(struct btfmslim *btfmslim,
		uint16_t reg, int bytes, void *src, uint8_t pgd)
{
	int ret, i;
	struct slim_ele_access msg;
	int slim_write_tries = SLIM_SLAVE_RW_MAX_TRIES;

	BTFMSLIM_DBG("Write to %s", pgd?"PGD":"IFD");
	msg.start_offset = SLIM_SLAVE_REG_OFFSET + reg;
	msg.num_bytes = bytes;
	msg.comp = NULL;

	for ( ; slim_write_tries != 0; slim_write_tries--) {
		mutex_lock(&btfmslim->xfer_lock);
		ret = slim_change_val_element(pgd ? btfmslim->slim_pgd :
			&btfmslim->slim_ifd, &msg, src, bytes);
		mutex_unlock(&btfmslim->xfer_lock);
		if (ret == 0)
			break;
		usleep_range(5000, 5100);
	}

	if (ret) {
		BTFMSLIM_ERR("failed (%d)", ret);
		return ret;
	}

	for (i = 0; i < bytes; i++)
		BTFMSLIM_DBG("Write 0x%02x to reg 0x%x", ((uint8_t *)src)[i],
			reg + i);
	return 0;
}

int btfm_slim_write_pgd(struct btfmslim *btfmslim,
		uint16_t reg, int bytes, void *src)
{
	return btfm_slim_write(btfmslim, reg, bytes, src, PGD);
}

int btfm_slim_write_inf(struct btfmslim *btfmslim,
		uint16_t reg, int bytes, void *src)
{
	return btfm_slim_write(btfmslim, reg, bytes, src, IFD);
}

int btfm_slim_read(struct btfmslim *btfmslim, unsigned short reg,
				int bytes, void *dest, uint8_t pgd)
{
	int ret, i;
	struct slim_ele_access msg;
	int slim_read_tries = SLIM_SLAVE_RW_MAX_TRIES;

	BTFMSLIM_DBG("Read from %s", pgd?"PGD":"IFD");
	msg.start_offset = SLIM_SLAVE_REG_OFFSET + reg;
	msg.num_bytes = bytes;
	msg.comp = NULL;

	for ( ; slim_read_tries != 0; slim_read_tries--) {
		mutex_lock(&btfmslim->xfer_lock);
		ret = slim_request_val_element(pgd ? btfmslim->slim_pgd :
			&btfmslim->slim_ifd, &msg, dest, bytes);
		mutex_unlock(&btfmslim->xfer_lock);
		if (ret == 0)
			break;
		usleep_range(5000, 5100);
	}

	if (ret)
		BTFMSLIM_ERR("failed (%d)", ret);

	for (i = 0; i < bytes; i++)
		BTFMSLIM_DBG("Read 0x%02x from reg 0x%x", ((uint8_t *)dest)[i],
			reg + i);

	return 0;
}

int btfm_slim_read_pgd(struct btfmslim *btfmslim,
		uint16_t reg, int bytes, void *dest)
{
	return btfm_slim_read(btfmslim, reg, bytes, dest, PGD);
}

int btfm_slim_read_inf(struct btfmslim *btfmslim,
		uint16_t reg, int bytes, void *dest)
{
	return btfm_slim_read(btfmslim, reg, bytes, dest, IFD);
}

int btfm_slim_enable_ch(struct btfmslim *btfmslim, struct btfmslim_ch *ch,
	uint8_t rxport, uint32_t rates, uint8_t grp, uint8_t nchan)
{
	int ret, i;
	struct slim_ch prop;
	struct btfmslim_ch *chan = ch;
	uint16_t ch_h[2];

	if (!btfmslim || !ch)
		return -EINVAL;

	BTFMSLIM_DBG("port: %d ch: %d", ch->port, ch->ch);

	/* Define the channel with below parameters */
	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = (rates == 48000) ? SLIM_CH_DATAF_NOT_DEFINED
			: SLIM_CH_DATAF_LPCM_AUDIO;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (rates/4000);
	prop.sampleszbits = 16;

	ch_h[0] = ch->ch_hdl;
	ch_h[1] = (grp) ? (ch+1)->ch_hdl : 0;

	ret = slim_define_ch(btfmslim->slim_pgd, &prop, ch_h, nchan, grp,
			&ch->grph);
	if (ret < 0) {
		BTFMSLIM_ERR("slim_define_ch failed ret[%d]", ret);
		goto error;
	}

	for (i = 0; i < nchan; i++, ch++) {
		/* Enable port through registration setting */
		if (btfmslim->vendor_port_en) {
			ret = btfmslim->vendor_port_en(btfmslim, ch->port,
					rxport, 1);
			if (ret < 0) {
				BTFMSLIM_ERR("vendor_port_en failed ret[%d]",
					ret);
				goto error;
			}
		}

		if (rxport) {
			BTFMSLIM_INFO("slim_connect_sink(port: %d, ch: %d)",
				ch->port, ch->ch);
			/* Connect Port with channel given by Machine driver*/
			ret = slim_connect_sink(btfmslim->slim_pgd,
				&ch->port_hdl, 1, ch->ch_hdl);
			if (ret < 0) {
				BTFMSLIM_ERR("slim_connect_sink failed ret[%d]",
					ret);
				goto remove_channel;
			}

		} else {
			BTFMSLIM_INFO("slim_connect_src(port: %d, ch: %d)",
				ch->port, ch->ch);
			/* Connect Port with channel given by Machine driver*/
			ret = slim_connect_src(btfmslim->slim_pgd, ch->port_hdl,
				ch->ch_hdl);
			if (ret < 0) {
				BTFMSLIM_ERR("slim_connect_src failed ret[%d]",
					ret);
				goto remove_channel;
			}
		}
	}

	/* Activate the channel immediately */
	BTFMSLIM_INFO(
		"port: %d, ch: %d, grp: %d, ch->grph: 0x%x, ch_hdl: 0x%x",
		chan->port, chan->ch, grp, chan->grph, chan->ch_hdl);
	ret = slim_control_ch(btfmslim->slim_pgd, (grp ? chan->grph :
		chan->ch_hdl), SLIM_CH_ACTIVATE, true);
	if (ret < 0) {
		BTFMSLIM_ERR("slim_control_ch failed ret[%d]", ret);
		goto remove_channel;
	}

error:
	return ret;

remove_channel:
	/* Remove the channel immediately*/
	ret = slim_control_ch(btfmslim->slim_pgd, (grp ? ch->grph : ch->ch_hdl),
			SLIM_CH_REMOVE, true);
	if (ret < 0)
		BTFMSLIM_ERR("slim_control_ch failed ret[%d]", ret);

	return ret;
}

int btfm_slim_disable_ch(struct btfmslim *btfmslim, struct btfmslim_ch *ch,
	uint8_t rxport, uint8_t grp, uint8_t nchan)
{
	int ret, i;

	if (!btfmslim || !ch)
		return -EINVAL;

	BTFMSLIM_INFO("port:%d, grp: %d, ch->grph:0x%x, ch->ch_hdl:0x%x ",
		ch->port, grp, ch->grph, ch->ch_hdl);
	/* Remove the channel immediately*/
	ret = slim_control_ch(btfmslim->slim_pgd, (grp ? ch->grph : ch->ch_hdl),
			SLIM_CH_REMOVE, true);
	if (ret < 0) {
		BTFMSLIM_ERR("slim_control_ch failed ret[%d]", ret);
		ret = slim_disconnect_ports(btfmslim->slim_pgd,
			&ch->port_hdl, 1);
		if (ret < 0) {
			BTFMSLIM_ERR("slim_disconnect_ports failed ret[%d]",
				ret);
			goto error;
		}
	}

	/* Disable port through registration setting */
	for (i = 0; i < nchan; i++, ch++) {
		if (btfmslim->vendor_port_en) {
			ret = btfmslim->vendor_port_en(btfmslim, ch->port,
				rxport, 0);
			if (ret < 0) {
				BTFMSLIM_ERR("vendor_port_en failed ret[%d]",
					ret);
				break;
			}
		}
	}
error:
	return ret;
}
static int btfm_slim_get_logical_addr(struct slim_device *slim)
{
	int ret = 0;
	const unsigned long timeout = jiffies +
			      msecs_to_jiffies(SLIM_SLAVE_PRESENT_TIMEOUT);

	do {
		ret = slim_get_logical_addr(slim, slim->e_addr,
			ARRAY_SIZE(slim->e_addr), &slim->laddr);
		if (!ret)  {
			BTFMSLIM_DBG("Assigned l-addr: 0x%x", slim->laddr);
			break;
		}
		/* Give SLIMBUS time to report present and be ready. */
		usleep_range(1000, 1100);
		BTFMSLIM_DBG("retyring get logical addr");
	} while (time_before(jiffies, timeout));

	return ret;
}

static int btfm_slim_alloc_port(struct btfmslim *btfmslim)
{
	int ret = -EINVAL, i;
	struct btfmslim_ch *rx_chs;
	struct btfmslim_ch *tx_chs;

	if (!btfmslim)
		return ret;

	rx_chs = btfmslim->rx_chs;
	tx_chs = btfmslim->tx_chs;

	if (!rx_chs || !tx_chs)
		return ret;

	BTFMSLIM_DBG("Rx: id\tname\tport\thdl\tch\tch_hdl");
	for (i = 0 ; (rx_chs->port != BTFM_SLIM_PGD_PORT_LAST) &&
		(i < BTFM_SLIM_NUM_CODEC_DAIS); i++, rx_chs++) {

		/* Get Rx port handler from slimbus driver based
		  * on port number
		  */
		ret = slim_get_slaveport(btfmslim->slim_pgd->laddr,
			rx_chs->port, &rx_chs->port_hdl, SLIM_SINK);
		if (ret < 0) {
			BTFMSLIM_ERR("slave port failure port#%d - ret[%d]",
				rx_chs->port, SLIM_SINK);
			return ret;
		}
		BTFMSLIM_DBG("    %d\t%s\t%d\t%x\t%d\t%x", rx_chs->id,
			rx_chs->name, rx_chs->port, rx_chs->port_hdl,
			rx_chs->ch, rx_chs->ch_hdl);
	}

	BTFMSLIM_DBG("Tx: id\tname\tport\thdl\tch\tch_hdl");
	for (i = 0; (tx_chs->port != BTFM_SLIM_PGD_PORT_LAST) &&
		(i < BTFM_SLIM_NUM_CODEC_DAIS); i++, tx_chs++) {

		/* Get Tx port handler from slimbus driver based
		  * on port number
		  */
		ret = slim_get_slaveport(btfmslim->slim_pgd->laddr,
			tx_chs->port, &tx_chs->port_hdl, SLIM_SRC);
		if (ret < 0) {
			BTFMSLIM_ERR("slave port failure port#%d - ret[%d]",
				tx_chs->port, SLIM_SRC);
			return ret;
		}
		BTFMSLIM_DBG("    %d\t%s\t%d\t%x\t%d\t%x", tx_chs->id,
			tx_chs->name, tx_chs->port, tx_chs->port_hdl,
			tx_chs->ch, tx_chs->ch_hdl);
	}
	return ret;
}

int btfm_slim_hw_init(struct btfmslim *btfmslim)
{
	int ret;

	BTFMSLIM_DBG("");
	if (!btfmslim)
		return -EINVAL;

	if (btfmslim->enabled) {
		BTFMSLIM_DBG("Already enabled");
		return 0;
	}
	mutex_lock(&btfmslim->io_lock);

	/* Assign Logical Address for PGD (Ported Generic Device)
	  * enumeration address
	  */
	ret = btfm_slim_get_logical_addr(btfmslim->slim_pgd);
	if (ret) {
		BTFMSLIM_ERR("failed to get slimbus %s logical address: %d",
		       btfmslim->slim_pgd->name, ret);
		goto error;
	}

	/* Assign Logical Address for Ported Generic Device
	  * enumeration address
	  */
	ret = btfm_slim_get_logical_addr(&btfmslim->slim_ifd);
	if (ret) {
		BTFMSLIM_ERR("failed to get slimbus %s logical address: %d",
		       btfmslim->slim_ifd.name, ret);
		goto error;
	}

	/* Allocate ports with logical address to get port handler from
	  * slimbus driver
	  */
	ret = btfm_slim_alloc_port(btfmslim);
	if (ret)
		goto error;

	/* Start vendor specific initialization and get port information */
	if (btfmslim->vendor_init)
		ret = btfmslim->vendor_init(btfmslim);

	/* Only when all registers read/write successfully, it set to
	  * enabled status
	  */
	btfmslim->enabled = 1;
error:
	mutex_unlock(&btfmslim->io_lock);
	return ret;
}


int btfm_slim_hw_deinit(struct btfmslim *btfmslim)
{
	int ret = 0;

	if (!btfmslim)
		return -EINVAL;

	if (!btfmslim->enabled) {
		BTFMSLIM_DBG("Already disabled");
		return 0;
	}
	mutex_lock(&btfmslim->io_lock);
	btfmslim->enabled = 0;
	mutex_unlock(&btfmslim->io_lock);
	return ret;
}

static int btfm_slim_get_dt_info(struct btfmslim *btfmslim)
{
	int ret = 0;
	struct slim_device *slim = btfmslim->slim_pgd;
	struct slim_device *slim_ifd = &btfmslim->slim_ifd;
	struct property *prop;

	if (!slim || !slim_ifd)
		return -EINVAL;

	if (slim->dev.of_node) {
		BTFMSLIM_DBG("Platform data from device tree (%s)",
			slim->name);
		ret = of_property_read_string(slim->dev.of_node,
			"qcom,btfm-slim-ifd", &slim_ifd->name);
		if (ret) {
			BTFMSLIM_ERR("Looking up %s property in node %s failed",
				"qcom,btfm-slim-ifd",
				 slim->dev.of_node->full_name);
			return -ENODEV;
		}
		BTFMSLIM_DBG("qcom,btfm-slim-ifd (%s)", slim_ifd->name);

		prop = of_find_property(slim->dev.of_node,
				"qcom,btfm-slim-ifd-elemental-addr", NULL);
		if (!prop) {
			BTFMSLIM_ERR("Looking up %s property in node %s failed",
				"qcom,btfm-slim-ifd-elemental-addr",
				slim->dev.of_node->full_name);
			return -ENODEV;
		} else if (prop->length != 6) {
			BTFMSLIM_ERR(
				"invalid codec slim ifd addr. addr length= %d",
				prop->length);
			return -ENODEV;
		}
		memcpy(slim_ifd->e_addr, prop->value, 6);
		BTFMSLIM_DBG(
			"PGD Enum Addr: %.02x:%.02x:%.02x:%.02x:%.02x: %.02x",
			slim->e_addr[0], slim->e_addr[1], slim->e_addr[2],
			slim->e_addr[3], slim->e_addr[4], slim->e_addr[5]);
		BTFMSLIM_DBG(
			"IFD Enum Addr: %.02x:%.02x:%.02x:%.02x:%.02x: %.02x",
			slim_ifd->e_addr[0], slim_ifd->e_addr[1],
			slim_ifd->e_addr[2], slim_ifd->e_addr[3],
			slim_ifd->e_addr[4], slim_ifd->e_addr[5]);
	} else {
		BTFMSLIM_ERR("Platform data is not valid");
	}

	return ret;
}

static int btfm_slim_probe(struct slim_device *slim)
{
	int ret = 0;
	struct btfmslim *btfm_slim;

	BTFMSLIM_DBG("");
	if (!slim->ctrl)
		return -EINVAL;

	/* Allocation btfmslim data pointer */
	btfm_slim = kzalloc(sizeof(struct btfmslim), GFP_KERNEL);
	if (btfm_slim == NULL) {
		BTFMSLIM_ERR("error, allocation failed");
		return -ENOMEM;
	}
	/* BTFM Slimbus driver control data configuration */
	btfm_slim->slim_pgd = slim;

	/* Assign vendor specific function */
	btfm_slim->rx_chs = SLIM_SLAVE_RXPORT;
	btfm_slim->tx_chs = SLIM_SLAVE_TXPORT;
	btfm_slim->vendor_init = SLIM_SLAVE_INIT;
	btfm_slim->vendor_port_en = SLIM_SLAVE_PORT_EN;

	/* Created Mutex for slimbus data transfer */
	mutex_init(&btfm_slim->io_lock);
	mutex_init(&btfm_slim->xfer_lock);

	/* Get Device tree node for Interface Device enumeration address */
	ret = btfm_slim_get_dt_info(btfm_slim);
	if (ret)
		goto dealloc;

	/* Add Interface Device for slimbus driver */
	ret = slim_add_device(btfm_slim->slim_pgd->ctrl, &btfm_slim->slim_ifd);
	if (ret) {
		BTFMSLIM_ERR("error, adding SLIMBUS device failed");
		goto dealloc;
	}

	/* Platform driver data allocation */
	slim->dev.platform_data = btfm_slim;

	/* Driver specific data allocation */
	btfm_slim->dev = &slim->dev;
	ret = btfm_slim_register_codec(&slim->dev);
	ret = bt_register_slimdev(&slim->dev);
	return ret;

dealloc:
	mutex_destroy(&btfm_slim->io_lock);
	mutex_destroy(&btfm_slim->xfer_lock);
	kfree(btfm_slim);
	return ret;
}
static int btfm_slim_remove(struct slim_device *slim)
{
	struct btfmslim *btfm_slim = slim->dev.platform_data;

	BTFMSLIM_DBG("");
	mutex_destroy(&btfm_slim->io_lock);
	mutex_destroy(&btfm_slim->xfer_lock);
	snd_soc_unregister_codec(&slim->dev);

	BTFMSLIM_DBG("slim_remove_device() - btfm_slim->slim_ifd");
	slim_remove_device(&btfm_slim->slim_ifd);

	BTFMSLIM_DBG("slim_remove_device() - btfm_slim->slim_pgd");
	slim_remove_device(slim);

	kfree(btfm_slim);
	return 0;
}

static const struct slim_device_id btfm_slim_id[] = {
	{SLIM_SLAVE_COMPATIBLE_STR, 0},
	{}
};

static struct slim_driver btfm_slim_driver = {
	.driver = {
		.name = "btfmslim-driver",
		.owner = THIS_MODULE,
	},
	.probe = btfm_slim_probe,
	.remove = btfm_slim_remove,
	.id_table = btfm_slim_id
};

static int __init btfm_slim_init(void)
{
	int ret;

	BTFMSLIM_DBG("");
	ret = slim_driver_register(&btfm_slim_driver);
	if (ret)
		BTFMSLIM_ERR("Failed to register slimbus driver: %d", ret);
	return ret;
}

static void __exit btfm_slim_exit(void)
{
	BTFMSLIM_DBG("");
	slim_driver_unregister(&btfm_slim_driver);
}

module_init(btfm_slim_init);
module_exit(btfm_slim_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("BTFM Slimbus Slave driver");
