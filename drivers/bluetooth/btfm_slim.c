// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
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
#include <linux/fs.h>
#include <linux/btpower.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include "btfm_slim.h"
#include "btfm_slim_slave.h"
#define DELAY_FOR_PORT_OPEN_MS (200)
#define SLIM_MANF_ID_QCOM	0x217
#define SLIM_PROD_CODE		0x221
#define BT_CMD_SLIM_TEST		0xbfac

struct class *btfm_slim_class;
static int btfm_slim_major;

struct btfmslim *btfm_slim_drv_data;

static int btfm_num_ports_open;

int btfm_slim_write(struct btfmslim *btfmslim,
		uint16_t reg, uint8_t reg_val, uint8_t pgd)
{
	int ret = -1;
	uint32_t reg_addr;
	int slim_write_tries = SLIM_SLAVE_RW_MAX_TRIES;

	BTFMSLIM_INFO("Write to %s", pgd?"PGD":"IFD");
	reg_addr = SLIM_SLAVE_REG_OFFSET + reg;

	for ( ; slim_write_tries != 0; slim_write_tries--) {
		mutex_lock(&btfmslim->xfer_lock);
		ret = slim_writeb(pgd ? btfmslim->slim_pgd :
			&btfmslim->slim_ifd, reg_addr, reg_val);
		mutex_unlock(&btfmslim->xfer_lock);
		if (ret) {
			BTFMSLIM_DBG("retrying to Write 0x%02x to reg 0x%x ret %d",
					 reg_val, reg_addr, ret);
		} else {
			BTFMSLIM_DBG("Written 0x%02x to reg 0x%x ret %d", reg_val, reg_addr, ret);
			break;
		}

		usleep_range(5000, 5100);
	}
	if (ret) {
		BTFMSLIM_DBG("retrying to Write 0x%02x to reg 0x%x ret %d",
				reg_val, reg_addr, ret);
	}
	return ret;
}

int btfm_slim_read(struct btfmslim *btfmslim, uint32_t reg, uint8_t pgd)
{
	int ret = -1;
	int slim_read_tries = SLIM_SLAVE_RW_MAX_TRIES;
	uint32_t reg_addr;
	BTFMSLIM_DBG("Read from %s", pgd?"PGD":"IFD");
	reg_addr = SLIM_SLAVE_REG_OFFSET + reg;

	for ( ; slim_read_tries != 0; slim_read_tries--) {
		mutex_lock(&btfmslim->xfer_lock);

		ret = slim_readb(pgd ? btfmslim->slim_pgd :
				&btfmslim->slim_ifd, reg_addr);
		BTFMSLIM_DBG("Read 0x%02x from reg 0x%x", ret, reg_addr);
		mutex_unlock(&btfmslim->xfer_lock);
		if (ret > 0)
			break;
		usleep_range(5000, 5100);
	}

	return ret;
}

int btfm_slim_enable_ch(struct btfmslim *btfmslim, struct btfmslim_ch *ch,
	uint8_t rxport, uint32_t rates, uint8_t nchan)
{
	int ret, i;
	struct btfmslim_ch *chan = ch;
	int chipset_ver;

	if (!btfmslim || !ch)
		return -EINVAL;

	BTFMSLIM_DBG("port: %d ch: %d", ch->port, ch->ch);

	chan->dai.sruntime = slim_stream_allocate(btfmslim->slim_pgd, "BTFM_SLIM");
	if (chan->dai.sruntime == NULL) {
		BTFMSLIM_ERR("slim_stream_allocate failed");
		return -EINVAL;
	}
	chan->dai.sconfig.bps = btfmslim->bps;
	chan->dai.sconfig.direction = btfmslim->direction;
	chan->dai.sconfig.rate = rates;
	chan->dai.sconfig.ch_count = nchan;
	chan->dai.sconfig.chs = kcalloc(nchan, sizeof(unsigned int), GFP_KERNEL);
	if (!chan->dai.sconfig.chs)
		return -ENOMEM;

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
		chan->dai.sconfig.chs[i] = ch->ch;
		chan->dai.sconfig.port_mask |= BIT(ch->port);
	}

	/* Activate the channel immediately */
	BTFMSLIM_INFO("port: %d, ch: %d", chan->port, chan->ch);
	chipset_ver = btpower_get_chipset_version();
	BTFMSLIM_INFO("chipset soc version:%x", chipset_ver);

	/* for feedback channel, PCM bit should not be set */
	if (btfm_feedback_ch_setting) {
		BTFMSLIM_DBG("port open for feedback ch, not setting PCM bit");
		//prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
		/* reset so that next port open sets the data format properly */
		btfm_feedback_ch_setting = 0;
	}

	ret = slim_stream_prepare(chan->dai.sruntime, &chan->dai.sconfig);
	if (ret) {
		BTFMSLIM_ERR("slim_stream_prepare failed = %d", ret);
		goto error;
	}

	ret = slim_stream_enable(chan->dai.sruntime);
	if (ret) {
		BTFMSLIM_ERR("slim_stream_enable failed = %d", ret);
		goto error;
	}

	if (ret == 0)
		btfm_num_ports_open++;

	BTFMSLIM_INFO("btfm_num_ports_open: %d", btfm_num_ports_open);
	return ret;
error:
	BTFMSLIM_INFO("error %d while opening port, btfm_num_ports_open: %d",
			ret, btfm_num_ports_open);
	kfree(chan->dai.sconfig.chs);
	chan->dai.sconfig.chs = NULL;
	return ret;
}

int btfm_slim_disable_ch(struct btfmslim *btfmslim, struct btfmslim_ch *ch,
			uint8_t rxport, uint8_t nchan)
{
	int ret, i;
	int chipset_ver = 0;

	if (!btfmslim || !ch)
		return -EINVAL;

	BTFMSLIM_INFO("port:%d ", ch->port);
	if (ch->dai.sruntime == NULL) {
		BTFMSLIM_ERR("Channel not enabled yet. returning");
		return -EINVAL;
	}

	if (rxport && (btfmslim->sample_rate == 44100 ||
		btfmslim->sample_rate == 88200)) {
		BTFMSLIM_INFO("disconnecting the ports, removing the channel");
		/* disconnect the ports of the stream */
		ret = slim_stream_unprepare_disconnect_port(ch->dai.sruntime,
				true, false);
		if (ret != 0)
			BTFMSLIM_ERR("slim_stream_unprepare failed %d", ret);
	}

	ret = slim_stream_disable(ch->dai.sruntime);
	if (ret != 0) {
		BTFMSLIM_ERR("slim_stream_disable failed returned val = %d", ret);
		if ((btfmslim->sample_rate != 44100) && (btfmslim->sample_rate != 88200)) {
			/* disconnect the ports of the stream */
			ret = slim_stream_unprepare_disconnect_port(ch->dai.sruntime,
					true, false);
			if (ret != 0)
				BTFMSLIM_ERR("slim_stream_unprepare failed %d", ret);
		}
	}

	/* free the ports allocated to the stream */
	ret = slim_stream_unprepare_disconnect_port(ch->dai.sruntime, false, true);
	if (ret != 0)
		BTFMSLIM_ERR("slim_stream_unprepare failed returned val = %d", ret);

	/* Disable port through registration setting */
	for (i = 0; i < nchan; i++, ch++) {
		if (btfmslim->vendor_port_en) {
			ret = btfmslim->vendor_port_en(btfmslim, ch->port,
				rxport, 0);
			if (ret < 0) {
				BTFMSLIM_ERR("vendor_port_en failed [%d]", ret);
				break;
			}
		}
	}
	ch->dai.sconfig.port_mask = 0;
	if (ch->dai.sconfig.chs != NULL) {
		kfree(ch->dai.sconfig.chs);
		BTFMSLIM_INFO("setting ch->dai.sconfig.chs to NULL");
		ch->dai.sconfig.chs = NULL;
	} else
		BTFMSLIM_ERR("ch->dai.sconfig.chs is already NULL");

	if (btfm_num_ports_open > 0)
		btfm_num_ports_open--;

	ch->dai.sruntime = NULL;

	BTFMSLIM_INFO("btfm_num_ports_open: %d", btfm_num_ports_open);

	chipset_ver = btpower_get_chipset_version();

	if (btfm_num_ports_open == 0 && (chipset_ver == QCA_HSP_SOC_ID_0200 ||
		chipset_ver == QCA_HSP_SOC_ID_0210 ||
		chipset_ver == QCA_HSP_SOC_ID_1201 ||
		chipset_ver == QCA_HSP_SOC_ID_1211 ||
		chipset_ver == QCA_APACHE_SOC_ID_0100 ||
		chipset_ver == QCA_APACHE_SOC_ID_0110 ||
		chipset_ver == QCA_APACHE_SOC_ID_0120 ||
		chipset_ver == QCA_APACHE_SOC_ID_0121)) {
		BTFMSLIM_INFO("SB reset needed after all ports disabled, sleeping");
		msleep(DELAY_FOR_PORT_OPEN_MS);
	}

	return ret;
}

static int btfm_slim_alloc_port(struct btfmslim *btfmslim)
{
	int ret = -EINVAL, i;
	int  chipset_ver;
	struct btfmslim_ch *rx_chs;
	struct btfmslim_ch *tx_chs;

	if (!btfmslim)
		return ret;

	chipset_ver = btpower_get_chipset_version();
	BTFMSLIM_INFO("chipset soc version:%x", chipset_ver);

	rx_chs = btfmslim->rx_chs;
	tx_chs = btfmslim->tx_chs;
	if ((chipset_ver >=  QCA_CHEROKEE_SOC_ID_0310) &&
		(chipset_ver <=  QCA_CHEROKEE_SOC_ID_0320_UMC)) {
		for (i = 0; (tx_chs->port != BTFM_SLIM_PGD_PORT_LAST) &&
		(i < BTFM_SLIM_NUM_CODEC_DAIS); i++, tx_chs++) {
			if (tx_chs->port == SLAVE_SB_PGD_PORT_TX1_FM)
				tx_chs->port = CHRKVER3_SB_PGD_PORT_TX1_FM;
			else if (tx_chs->port == SLAVE_SB_PGD_PORT_TX2_FM)
				tx_chs->port = CHRKVER3_SB_PGD_PORT_TX2_FM;
			BTFMSLIM_INFO("Tx port:%d", tx_chs->port);
		}
		tx_chs = btfmslim->tx_chs;
	}
	if (!rx_chs || !tx_chs)
		return ret;

	return 0;
}

static int btfm_slim_get_logical_addr(struct slim_device *slim)
{
	int ret = 0;
	const unsigned long timeout = jiffies +
			      msecs_to_jiffies(SLIM_SLAVE_PRESENT_TIMEOUT);
	BTFMSLIM_INFO("");

	do {

		ret = slim_get_logical_addr(slim);
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

int btfm_slim_hw_init(struct btfmslim *btfmslim)
{
	int ret;
	int chipset_ver;
	struct slim_device *slim;
	struct slim_device *slim_ifd;

	BTFMSLIM_DBG("");
	if (!btfmslim)
		return -EINVAL;

	if (btfmslim->enabled) {
		BTFMSLIM_DBG("Already enabled");
		return 0;
	}

	slim = btfmslim->slim_pgd;
	slim_ifd = &btfmslim->slim_ifd;

	mutex_lock(&btfmslim->io_lock);
	BTFMSLIM_INFO(
		"PGD Enum Addr: mfr id:%.02x prod code:%.02x dev ind:%.02x ins:%.02x",
		slim->e_addr.manf_id, slim->e_addr.prod_code,
		slim->e_addr.dev_index, slim->e_addr.instance);


	chipset_ver = btpower_get_chipset_version();
	BTFMSLIM_INFO("chipset soc version:%x", chipset_ver);

	if (chipset_ver == QCA_HSP_SOC_ID_0100 ||
		chipset_ver == QCA_HSP_SOC_ID_0110 ||
		chipset_ver == QCA_HSP_SOC_ID_0200 ||
		chipset_ver == QCA_HSP_SOC_ID_0210 ||
		chipset_ver == QCA_HSP_SOC_ID_1201 ||
		chipset_ver == QCA_HSP_SOC_ID_1211) {
		BTFMSLIM_INFO("chipset is hastings prime, overwriting EA");
		slim->is_laddr_valid = false;
		slim->e_addr.manf_id = SLIM_MANF_ID_QCOM;
		slim->e_addr.prod_code = SLIM_PROD_CODE;
		slim->e_addr.dev_index = 0x01;
		slim->e_addr.instance = 0x0;
		/* we are doing this to indicate that this is not a child node
		 * (doesn't have call back functions). Needed only for querying
		 * logical address.
		 */
		slim_ifd->dev.driver = NULL;
		slim_ifd->ctrl = btfmslim->slim_pgd->ctrl; //slimbus controller structure.
		slim_ifd->is_laddr_valid = false;
		slim_ifd->e_addr.manf_id = SLIM_MANF_ID_QCOM;
		slim_ifd->e_addr.prod_code = SLIM_PROD_CODE;
		slim_ifd->e_addr.dev_index = 0x0;
		slim_ifd->e_addr.instance = 0x0;
		slim_ifd->laddr = 0x0;
	} else if (chipset_ver == QCA_MOSELLE_SOC_ID_0100 ||
		chipset_ver == QCA_MOSELLE_SOC_ID_0110 ||
		chipset_ver == QCA_MOSELLE_SOC_ID_0120) {
		BTFMSLIM_INFO("chipset is Moselle, overwriting EA");
		slim->is_laddr_valid = false;
		slim->e_addr.manf_id = SLIM_MANF_ID_QCOM;
		slim->e_addr.prod_code = 0x222;
		slim->e_addr.dev_index = 0x01;
		slim->e_addr.instance = 0x0;
		/* we are doing this to indicate that this is not a child node
		 * (doesn't have call back functions). Needed only for querying
		 * logical address.
		 */
		slim_ifd->dev.driver = NULL;
		slim_ifd->ctrl = btfmslim->slim_pgd->ctrl; //slimbus controller structure.
		slim_ifd->is_laddr_valid = false;
		slim_ifd->e_addr.manf_id = SLIM_MANF_ID_QCOM;
		slim_ifd->e_addr.prod_code = 0x222;
		slim_ifd->e_addr.dev_index = 0x0;
		slim_ifd->e_addr.instance = 0x0;
		slim_ifd->laddr = 0x0;
	} else if (chipset_ver == QCA_HAMILTON_SOC_ID_0100 ||
		chipset_ver ==  QCA_HAMILTON_SOC_ID_0101 ||
		chipset_ver ==  QCA_HAMILTON_SOC_ID_0200) {
		BTFMSLIM_INFO("chipset is Hamliton, overwriting EA");
		slim->is_laddr_valid = false;
		slim->e_addr.manf_id = SLIM_MANF_ID_QCOM;
		slim->e_addr.prod_code = 0x220;
		slim->e_addr.dev_index = 0x01;
		slim->e_addr.instance = 0x0;
		/* we are doing this to indicate that this is not a child node
		 * (doesn't have call back functions). Needed only for querying
		 * logical address.
		 */
		slim_ifd->dev.driver = NULL;
		slim_ifd->ctrl = btfmslim->slim_pgd->ctrl; //slimbus controller structure.
		slim_ifd->is_laddr_valid = false;
		slim_ifd->e_addr.manf_id = SLIM_MANF_ID_QCOM;
		slim_ifd->e_addr.prod_code = 0x220;
		slim_ifd->e_addr.dev_index = 0x0;
		slim_ifd->e_addr.instance = 0x0;
		slim_ifd->laddr = 0x0;
	} else if (chipset_ver == QCA_CHEROKEE_SOC_ID_0200 ||
		chipset_ver ==  QCA_CHEROKEE_SOC_ID_0201  ||
		chipset_ver ==  QCA_CHEROKEE_SOC_ID_0210  ||
		chipset_ver ==  QCA_CHEROKEE_SOC_ID_0211  ||
		chipset_ver ==  QCA_CHEROKEE_SOC_ID_0310  ||
		chipset_ver ==  QCA_CHEROKEE_SOC_ID_0320  ||
		chipset_ver ==  QCA_CHEROKEE_SOC_ID_0320_UMC  ||
		chipset_ver ==  QCA_APACHE_SOC_ID_0100  ||
		chipset_ver ==  QCA_APACHE_SOC_ID_0110  ||
		chipset_ver ==  QCA_APACHE_SOC_ID_0120 ||
		chipset_ver ==  QCA_APACHE_SOC_ID_0121 ||
		chipset_ver ==  QCA_COMANCHE_SOC_ID_0101 ||
		chipset_ver ==  QCA_COMANCHE_SOC_ID_0110 ||
		chipset_ver ==  QCA_COMANCHE_SOC_ID_0120 ||
		chipset_ver ==  QCA_COMANCHE_SOC_ID_0130 ||
		chipset_ver ==  QCA_COMANCHE_SOC_ID_4130 ||
		chipset_ver ==  QCA_COMANCHE_SOC_ID_5120 ||
		chipset_ver ==  QCA_COMANCHE_SOC_ID_5130) {
		BTFMSLIM_INFO("chipset is Chk/Apache/CMC, overwriting EA");
		slim->is_laddr_valid = false;
		slim->e_addr.manf_id = SLIM_MANF_ID_QCOM;
		slim->e_addr.prod_code = 0x220;
		slim->e_addr.dev_index = 0x01;
		slim->e_addr.instance = 0x0;
		/* we are doing this to indicate that this is not a child node
		 * (doesn't have call back functions). Needed only for querying
		 * logical address.
		 */
		slim_ifd->dev.driver = NULL;
		slim_ifd->ctrl = btfmslim->slim_pgd->ctrl; //slimbus controller structure.
		slim_ifd->is_laddr_valid = false;
		slim_ifd->e_addr.manf_id = SLIM_MANF_ID_QCOM;
		slim_ifd->e_addr.prod_code = 0x220;
		slim_ifd->e_addr.dev_index = 0x0;
		slim_ifd->e_addr.instance = 0x0;
		slim_ifd->laddr = 0x0;
	}
	BTFMSLIM_INFO(
		"PGD Enum Addr: manu id:%.02x prod code:%.02x dev idx:%.02x instance:%.02x",
		slim->e_addr.manf_id, slim->e_addr.prod_code,
		slim->e_addr.dev_index, slim->e_addr.instance);

	BTFMSLIM_INFO(
		"IFD Enum Addr: manu id:%.02x prod code:%.02x dev idx:%.02x instance:%.02x",
		slim_ifd->e_addr.manf_id, slim_ifd->e_addr.prod_code,
		slim_ifd->e_addr.dev_index, slim_ifd->e_addr.instance);

	/* Assign Logical Address for PGD (Ported Generic Device)
	 * enumeration address
	 */
	ret = btfm_slim_get_logical_addr(btfmslim->slim_pgd);
	if (ret) {
		BTFMSLIM_ERR("failed to get slimbus logical address: %d", ret);
		goto error;
	}

	/* Assign Logical Address for Ported Generic Device
	 * enumeration address
	 */
	ret = btfm_slim_get_logical_addr(&btfmslim->slim_ifd);
	if (ret) {
		BTFMSLIM_ERR("failed to get slimbus logical address: %d", ret);
		goto error;
	}

	ret = btfm_slim_alloc_port(btfmslim);
	if (ret != 0)
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

	BTFMSLIM_INFO("");
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

static int btfm_slim_status(struct slim_device *sdev,
				enum slim_device_status status)
{
	struct device *dev = &sdev->dev;
	struct btfmslim *btfm_slim;
	int ret = 0;
	btfm_slim = dev_get_drvdata(dev);
	ret = btfm_slim_register_codec(btfm_slim);
	if (ret)
		BTFMSLIM_ERR("error, registering slimbus codec failed");

	return ret;
}

static long btfm_slim_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case BT_CMD_SLIM_TEST:
		BTFMSLIM_INFO("cmd BT_CMD_SLIM_TEST, call btfm_slim_hw_init");
		ret = btfm_slim_hw_init(btfm_slim_drv_data);
		break;
	}
	return ret;
}

static const struct file_operations bt_dev_fops = {
	.unlocked_ioctl = btfm_slim_ioctl,
	.compat_ioctl = btfm_slim_ioctl,
};

static int btfm_slim_probe(struct slim_device *slim)
{
	int ret = 0;
	struct btfmslim *btfm_slim;

	pr_info("%s: name = %s\n", __func__, dev_name(&slim->dev));
	/*this as true during the probe then slimbus won't check for logical address*/
	slim->is_laddr_valid = true;
	dev_set_name(&slim->dev, "%s", "btfmslim_slave");
	pr_info("%s: name = %s\n", __func__, dev_name(&slim->dev));

	BTFMSLIM_DBG("");
	BTFMSLIM_ERR("is_laddr_valid is true");
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
	dev_set_drvdata(&slim->dev, btfm_slim);

	/* Driver specific data allocation */
	btfm_slim->dev = &slim->dev;
	ret = btpower_register_slimdev(&slim->dev);
	if (ret < 0) {
		btfm_slim_unregister_codec(&slim->dev);
		ret = -EPROBE_DEFER;
		goto dealloc;
	}

	btfm_slim_drv_data = btfm_slim;
	btfm_slim_major = register_chrdev(0, "btfm_slim", &bt_dev_fops);
	if (btfm_slim_major < 0) {
		BTFMSLIM_ERR("%s: failed to allocate char dev\n", __func__);
		ret = -1;
		goto register_err;
	}

	btfm_slim_class = class_create(THIS_MODULE, "btfmslim-dev");
	if (IS_ERR(btfm_slim_class)) {
		BTFMSLIM_ERR("%s: coudn't create class\n", __func__);
		ret = -1;
		goto class_err;
	}

	if (device_create(btfm_slim_class, NULL, MKDEV(btfm_slim_major, 0),
		NULL, "btfmslim") == NULL) {
		BTFMSLIM_ERR("%s: failed to allocate char dev\n", __func__);
		ret = -1;
		goto device_err;
	}
	return ret;

device_err:
	class_destroy(btfm_slim_class);
class_err:
	unregister_chrdev(btfm_slim_major, "btfm_slim");
register_err:
	btfm_slim_unregister_codec(&slim->dev);
dealloc:
	mutex_destroy(&btfm_slim->io_lock);
	mutex_destroy(&btfm_slim->xfer_lock);
	kfree(btfm_slim);
	return ret;
}

static void btfm_slim_remove(struct slim_device *slim)
{
	struct device *dev = &slim->dev;
	struct btfmslim *btfm_slim = dev_get_drvdata(dev);
	BTFMSLIM_DBG("");
	mutex_destroy(&btfm_slim->io_lock);
	mutex_destroy(&btfm_slim->xfer_lock);
	snd_soc_unregister_component(&slim->dev);
	kfree(btfm_slim);
}

static const struct slim_device_id btfm_slim_id[] = {
	{
	.manf_id = SLIM_MANF_ID_QCOM,
	.prod_code = SLIM_PROD_CODE,
	.dev_index = 0x1,
	.instance = 0x0,
	},
	{
	.manf_id = SLIM_MANF_ID_QCOM,
	.prod_code = 0x220,
	.dev_index = 0x1,
	.instance = 0x0,
	}
};

MODULE_DEVICE_TABLE(slim, btfm_slim_id);

static struct slim_driver btfm_slim_driver = {
	.driver = {
		.name = "btfmslim-driver",
		.owner = THIS_MODULE,
	},
	.probe = btfm_slim_probe,
	.device_status = btfm_slim_status,
	.remove = btfm_slim_remove,
	.id_table = btfm_slim_id
};

module_slim_driver(btfm_slim_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("BTFM Slimbus Slave driver");
