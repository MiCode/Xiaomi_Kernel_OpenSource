#define LOG_TAG         "Plat"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_firmware.h"
#include "cts_sysfs.h"

#ifdef CFG_CTS_FW_LOG_REDIRECT
size_t cts_plat_get_max_fw_log_size(struct cts_platform_data *pdata)
{
	return CTS_FW_LOG_BUF_LEN;
}

u8 *cts_plat_get_fw_log_buf(struct cts_platform_data *pdata, size_t size)
{
	return pdata->fw_log_buf;
}
#endif

size_t cts_plat_get_max_i2c_xfer_size(struct cts_platform_data *pdata)
{
	return CFG_CTS_MAX_I2C_XFER_SIZE;
}

#ifdef CONFIG_CTS_I2C_HOST
u8 *cts_plat_get_i2c_xfer_buf(struct cts_platform_data *pdata, size_t xfer_size)
{
	return pdata->i2c_fifo_buf;
}

int cts_plat_i2c_write(struct cts_platform_data *pdata, u8 i2c_addr,
		       const void *src, size_t len, int retry, int delay)
{
	int ret = 0, retries = 0;

	struct i2c_msg msg = {
		.flags = 0,
		.addr = i2c_addr,
		.buf = (u8 *) src,
		.len = len,
	};

	do {
		ret = i2c_transfer(pdata->i2c_client->adapter, &msg, 1);
		if (ret != 1) {
			if (ret >= 0)
				ret = -EIO;

			if (delay)
				mdelay(delay);
			continue;
		} else
			return 0;
	} while (++retries < retry);

	return ret;
}

int cts_plat_i2c_read(struct cts_platform_data *pdata, u8 i2c_addr,
		      const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
		      int retry, int delay)
{
	int num_msg, ret = 0, retries = 0;

	struct i2c_msg msgs[2] = {
		{
		 .addr = i2c_addr,
		 .flags = 0,
		 .buf = (u8 *) wbuf,
		 .len = wlen },
		{
		 .addr = i2c_addr,
		 .flags = I2C_M_RD,
		 .buf = (u8 *) rbuf,
		 .len = rlen }
	};

	if (wbuf == NULL || wlen == 0)
		num_msg = 1;
	else
		num_msg = 2;

	do {
		ret = i2c_transfer(pdata->i2c_client->adapter,
				   msgs + ARRAY_SIZE(msgs) - num_msg, num_msg);

		if (ret != num_msg) {
			if (ret >= 0)
				ret = -EIO;

			if (delay)
				mdelay(delay);
			continue;
		} else
			return 0;
	} while (++retries < retry);

	return ret;
}

int cts_plat_is_i2c_online(struct cts_platform_data *pdata, u8 i2c_addr)
{
	u8 dummy_bytes[2] = { 0x00, 0x00 };
	int ret;

	ret = cts_plat_i2c_write(pdata, i2c_addr, dummy_bytes,
				 sizeof(dummy_bytes), 5, 2);
	if (ret) {
		cts_err("!!! I2C addr 0x%02x is offline !!!", i2c_addr);
		return false;
	}

	cts_dbg("I2C addr 0x%02x is online", i2c_addr);
	return true;
}
#else
void dump_spi_common(const char *prefix, u8 *data, size_t datalen)
{
	u8 str[1024];
	int offset = 0;
	int i;

	offset += snprintf(str + offset, sizeof(str) - offset, "%s", prefix);
	for (i = 0; i < datalen; i++) {
		offset +=
		    snprintf(str + offset, sizeof(str) - offset, " %02x",
			     data[i]);
	}
	cts_err("%s", str);
}

#ifdef CFG_CTS_MANUAL_CS
int cts_plat_set_cs(struct cts_platform_data *pdata, int val)
{
	if (val)
		gpio_set_value(pdata->cs_gpio, 1);
	else
		gpio_set_value(pdata->cs_gpio, 0);

	return 0;
}
#endif

int cts_spi_send_recv(struct cts_platform_data *pdata, size_t len,
		      u8 *tx_buffer, u8 *rx_buffer)
{
	struct chipone_ts_data *cts_data;
	struct spi_message msg;
	struct spi_transfer cmd = {
		.delay_usecs = 0,
		.speed_hz = pdata->spi_speed * 1000u,
		.tx_buf = tx_buffer,
		.rx_buf = rx_buffer,
		.len = len,
		/* .tx_dma = 0, */
		/* .rx_dma = 0, */
	};
	int ret = 0;

	cts_data =
	    container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

#ifdef CFG_CTS_MANUAL_CS
	cts_plat_set_cs(pdata, 0);
#endif
	spi_message_init(&msg);
	spi_message_add_tail(&cmd, &msg);
	ret = spi_sync(cts_data->spi_client, &msg);
	if (ret)
		cts_err("spi sync failed %d", ret);

	udelay(100);

#ifdef CFG_CTS_MANUAL_CS
	cts_plat_set_cs(pdata, 1);
#endif
	return ret;
}

size_t cts_plat_get_max_spi_xfer_size(struct cts_platform_data *pdata)
{
	return CFG_CTS_MAX_SPI_XFER_SIZE;
}

u8 *cts_plat_get_spi_xfer_buf(struct cts_platform_data *pdata, size_t xfer_size)
{
	return pdata->spi_cache_buf;
}

int cts_plat_spi_write(struct cts_platform_data *pdata, u8 dev_addr,
		       const void *src, size_t len, int retry, int delay)
{
	int ret = 0, retries = 0;
	u16 crc;
	size_t data_len;

	if (len > CFG_CTS_MAX_SPI_XFER_SIZE) {
		cts_err("write too much data:wlen=%zu\n", len);
		return -EIO;
	}

	if (pdata->cts_dev->rtdata.program_mode) {
		pdata->spi_tx_buf[0] = dev_addr;
		memcpy(&pdata->spi_tx_buf[1], src, len);

		do {
			ret =
			    cts_spi_send_recv(pdata, len + 1, pdata->spi_tx_buf,
					      pdata->spi_rx_buf);
			if (ret) {
				cts_err("SPI write failed %d", ret);
				if (delay)
					mdelay(delay);
			} else
				return 0;
		} while (++retries < retry);
	} else {
		data_len = len - 2;
		pdata->spi_tx_buf[0] = dev_addr;
		pdata->spi_tx_buf[1] = *((u8 *) src + 1);
		pdata->spi_tx_buf[2] = *((u8 *) src);
		put_unaligned_le16(data_len, &pdata->spi_tx_buf[3]);
		crc = (u16) cts_crc32(pdata->spi_tx_buf, 5);
		put_unaligned_le16(crc, &pdata->spi_tx_buf[5]);
		memcpy(&pdata->spi_tx_buf[7], (char *)src + 2, data_len);
		crc = (u16) cts_crc32((char *)src + 2, data_len);
		put_unaligned_le16(crc, &pdata->spi_tx_buf[7 + data_len]);
		do {
			ret =
			    cts_spi_send_recv(pdata, len + 7, pdata->spi_tx_buf,
					      pdata->spi_rx_buf);
			udelay(10 * data_len);
			if (ret) {
				cts_err("SPI write failed %d", ret);
				if (delay)
					mdelay(delay);
			} else
				return 0;
		} while (++retries < retry);
	}
	return ret;
}

int cts_plat_spi_read(struct cts_platform_data *pdata, u8 dev_addr,
		      const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
		      int retry, int delay)
{
	int ret = 0, retries = 0;
	u16 crc;

	if (wlen > CFG_CTS_MAX_SPI_XFER_SIZE
	    || rlen > CFG_CTS_MAX_SPI_XFER_SIZE) {
		cts_err("write/read too much data:wlen=%zd, rlen=%zd", wlen,
			rlen);
		return -EIO;
	}

	if (pdata->cts_dev->rtdata.program_mode) {
		pdata->spi_tx_buf[0] = dev_addr | 0x01;
		memcpy(&pdata->spi_tx_buf[1], wbuf, wlen);
		do {
			ret = cts_spi_send_recv(pdata, rlen + 5,
						pdata->spi_tx_buf,
						pdata->spi_rx_buf);
			if (ret) {
				cts_err("SPI read failed %d", ret);
				if (delay)
					mdelay(delay);
				continue;
			}
			memcpy(rbuf, pdata->spi_rx_buf + 5, rlen);
			return 0;
		} while (++retries < retry);
	} else {
		do {
			if (wlen != 0) {
				pdata->spi_tx_buf[0] = dev_addr | 0x01;
				pdata->spi_tx_buf[1] = wbuf[1];
				pdata->spi_tx_buf[2] = wbuf[0];
				put_unaligned_le16(rlen, &pdata->spi_tx_buf[3]);
				crc = (u16) cts_crc32(pdata->spi_tx_buf, 5);
				put_unaligned_le16(crc, &pdata->spi_tx_buf[5]);
				ret = cts_spi_send_recv(pdata, 7,
							pdata->spi_tx_buf,
							pdata->spi_rx_buf);
				if (ret) {
					cts_err("SPI read failed %d", ret);
					if (delay)
						mdelay(delay);
					continue;
				}
			}
			memset(pdata->spi_tx_buf, 0, 7);
			pdata->spi_tx_buf[0] = dev_addr | 0x01;
			udelay(100);
			ret = cts_spi_send_recv(pdata, rlen + 2,
						pdata->spi_tx_buf,
						pdata->spi_rx_buf);
			if (ret) {
				cts_err("SPI read failed %d", ret);
				if (delay)
					mdelay(delay);
				continue;
			}
			memcpy(rbuf, pdata->spi_rx_buf, rlen);
			crc = (u16) cts_crc32(pdata->spi_rx_buf, rlen);
			if (get_unaligned_le16(&pdata->spi_rx_buf[rlen]) != crc) {
				cts_err("SPI RX CRC error: rx_crc %04x != %04x",
					get_unaligned_le16(&pdata->spi_rx_buf[rlen]),
					crc);
				continue;
			}
			return 0;
		} while (++retries < retry);
	}
	if (retries >= retry)
		cts_err("SPI read too much retry");

	return -EIO;
}

int cts_plat_spi_read_delay_idle(struct cts_platform_data *pdata, u8 dev_addr,
				 const u8 *wbuf, size_t wlen, void *rbuf,
				 size_t rlen, int retry, int delay, int idle)
{
	int ret = 0, retries = 0;
	u16 crc;

	if (wlen > CFG_CTS_MAX_SPI_XFER_SIZE ||
	    rlen > CFG_CTS_MAX_SPI_XFER_SIZE) {
		cts_err("write/read too much data:wlen=%zu, rlen=%zu", wlen,
			rlen);
		return -E2BIG;
	}

	if (pdata->cts_dev->rtdata.program_mode) {
		pdata->spi_tx_buf[0] = dev_addr | 0x01;
		memcpy(&pdata->spi_tx_buf[1], wbuf, wlen);
		do {
			ret = cts_spi_send_recv(pdata, rlen + 5,
						pdata->spi_tx_buf,
						pdata->spi_rx_buf);
			if (ret) {
				cts_err("SPI read failed %d", ret);
				if (delay)
					mdelay(delay);
				continue;
			}
			memcpy(rbuf, pdata->spi_rx_buf + 5, rlen);
			return 0;
		} while (++retries < retry);
	} else {
		do {
			if (wlen != 0) {
				pdata->spi_tx_buf[0] = dev_addr | 0x01;
				pdata->spi_tx_buf[1] = wbuf[1];
				pdata->spi_tx_buf[2] = wbuf[0];
				put_unaligned_le16(rlen, &pdata->spi_tx_buf[3]);
				crc = (u16) cts_crc32(pdata->spi_tx_buf, 5);
				put_unaligned_le16(crc, &pdata->spi_tx_buf[5]);
				ret =
				    cts_spi_send_recv(pdata, 7,
						      pdata->spi_tx_buf,
						      pdata->spi_rx_buf);
				if (ret) {
					cts_err("SPI read failed %d", ret);
					if (delay)
						mdelay(delay);
					continue;
				}
			}
			memset(pdata->spi_tx_buf, 0, 7);
			pdata->spi_tx_buf[0] = dev_addr | 0x01;
			udelay(idle);
			ret = cts_spi_send_recv(pdata, rlen + 2,
						pdata->spi_tx_buf,
						pdata->spi_rx_buf);
			if (ret) {
				if (delay)
					mdelay(delay);
				continue;
			}
			memcpy(rbuf, pdata->spi_rx_buf, rlen);
			crc = (u16) cts_crc32(pdata->spi_rx_buf, rlen);
			if (get_unaligned_le16(&pdata->spi_rx_buf[rlen]) != crc)
				continue;
			return 0;
		} while (++retries < retry);
	}
	if (retries >= retry)
		cts_err("cts_plat_spi_read error");

	return -EIO;
}

int cts_plat_is_normal_mode(struct cts_platform_data *pdata)
{
	struct chipone_ts_data *cts_data;
	u16 fwid;
	int ret;
	/* u8 tx_buf[4] = { 0 }; */
	/* u32 addr; */

	cts_set_normal_addr(pdata->cts_dev);
	cts_data =
	    container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
	/*
	 * addr = CTS_DEVICE_FW_REG_CHIP_TYPE;
	 * put_unaligned_be16(addr, tx_buf);
	 * ret = cts_plat_spi_read(pdata, CTS_DEV_NORMAL_MODE_SPIADDR,
	 *     tx_buf, 2,
	 *     &fwid, 2, 3, 10);
	 * fwid = be16_to_cpu(fwid);
	 */
	ret = pdata->cts_dev->ops->get_fw_id(pdata->cts_dev, &fwid);

	if (ret || !cts_is_fwid_valid(fwid))
		return false;

	return true;
}
#endif

static void cts_plat_handle_irq(struct cts_platform_data *pdata)
{
	int ret;

	cts_dbg("Handle IRQ");

	cts_lock_device(pdata->cts_dev);
	ret = cts_irq_handler(pdata->cts_dev);
	if (ret)
		cts_err("Device handle IRQ failed %d", ret);
	cts_unlock_device(pdata->cts_dev);
}

static irqreturn_t cts_plat_irq_handler(int irq, void *dev_id)
{
	struct cts_platform_data *pdata;
#ifndef CONFIG_GENERIC_HARDIRQS
	struct chipone_ts_data *cts_data;
#endif /* CONFIG_GENERIC_HARDIRQS */

	cts_dbg("IRQ handler");

	pdata = (struct cts_platform_data *)dev_id;
	if (pdata == NULL) {
		cts_err("IRQ handler with NULL dev_id");
		return IRQ_NONE;
	}
#ifdef CONFIG_GENERIC_HARDIRQS
	cts_plat_handle_irq(pdata);
#else /* CONFIG_GENERIC_HARDIRQS */
	cts_data =
	    container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

	if (queue_work(cts_data->workqueue, &pdata->ts_irq_work)) {
		cts_dbg("IRQ queue work");
		cts_plat_disable_irq(pdata);
	} else
		cts_warn
		    ("IRQ handler queue work failed as already on the queue");
#endif /* CONFIG_GENERIC_HARDIRQS */

	return IRQ_HANDLED;
}

#ifndef CONFIG_GENERIC_HARDIRQS
static void cts_plat_touch_dev_irq_work(struct work_struct *work)
{
	struct cts_platform_data *pdata =
	    container_of(work, struct cts_platform_data, ts_irq_work);

	cts_dbg("IRQ work");

	cts_plat_handle_irq(pdata);

	cts_plat_enable_irq(pdata);
}
#endif /* CONFIG_GENERIC_HARDIRQS */

#ifdef CONFIG_CTS_OF
static int cts_plat_parse_dt(struct cts_platform_data *pdata,
			     struct device_node *dev_node)
{
	int ret = 0;

	cts_info("Parse device tree");

	pdata->int_gpio =
	    of_get_named_gpio(dev_node, CFG_CTS_OF_INT_GPIO_NAME, 0);
	if (!gpio_is_valid(pdata->int_gpio)) {
		cts_err("Parse INT GPIO from dt failed %d", pdata->int_gpio);
		pdata->int_gpio = -1;
	}
	cts_info("  %-12s: %d", "int gpio", pdata->int_gpio);

	pdata->irq = gpio_to_irq(pdata->int_gpio);
	if (pdata->irq < 0) {
		cts_err("Parse irq failed %d", ret);
		return pdata->irq;
	}
	cts_info("  %-12s: %d", "irq num", pdata->irq);

#ifdef CFG_CTS_HAS_RESET_PIN
	pdata->rst_gpio =
	    of_get_named_gpio(dev_node, CFG_CTS_OF_RST_GPIO_NAME, 0);
	if (!gpio_is_valid(pdata->rst_gpio)) {
		cts_err("Parse RST GPIO from dt failed %d", pdata->rst_gpio);
		pdata->rst_gpio = -1;
	}
	cts_info("  %-12s: %d", "rst gpio", pdata->rst_gpio);
#endif /* CFG_CTS_HAS_RESET_PIN */

#ifdef CFG_CTS_MANUAL_CS
	pdata->cs_gpio =
	    of_get_named_gpio(dev_node, CFG_CTS_OF_CS_GPIO_NAME, 0);
	if (!gpio_is_valid(pdata->cs_gpio)) {
		cts_err("Parse CS GPIO from dt failed %d", pdata->cs_gpio);
		pdata->cs_gpio = -1;
	}
	cts_info("  %-12s: %d", "cs gpio", pdata->cs_gpio);
#endif

	ret = of_property_read_u32(dev_node, CFG_CTS_OF_X_RESOLUTION_NAME,
				   &pdata->res_x);
	if (ret)
		cts_warn("Parse X resolution from dt failed %d", ret);

	cts_info("  %-12s: %d", "X resolution", pdata->res_x);

	ret = of_property_read_u32(dev_node, CFG_CTS_OF_Y_RESOLUTION_NAME,
				   &pdata->res_y);
	if (ret)
		cts_warn("Parse Y resolution from dt failed %d", ret);

	cts_info("  %-12s: %d", "Y resolution", pdata->res_y);

	if (of_property_read_u32
	    (dev_node, "chipone,def-build-id", &pdata->build_id)) {
		pdata->build_id = 0;
		cts_info("chipone,build_id undefined.\n");
	} else
		cts_info("chipone,build_id=0x%04X\n", pdata->build_id);

	if (of_property_read_u32
	    (dev_node, "chipone,def-config-id", &pdata->config_id)) {
		pdata->config_id = 0;
		cts_info("chipone,config_id undefined.\n");
	} else
		cts_info("chipone,config_id=0x%04X\n", pdata->config_id);

#ifdef CFG_CTS_FW_UPDATE_SYS
	ret = of_property_read_string(dev_node, CFG_CTS_OF_PANEL_SUPPLIER,
				      &pdata->panel_supplier);
	if (ret) {
		pdata->panel_supplier = NULL;
		cts_warn("read panel supplier failed, ret=%d\n", ret);
	} else
		cts_info("panel supplier=%s", (char *)pdata->panel_supplier);
#endif

	return 0;
}
#endif /* CONFIG_CTS_OF */

#ifdef CFG_CTS_FORCE_UP
static void cts_plat_touch_event_timeout_work(struct work_struct *work)
{
	struct cts_platform_data *pdata = container_of
	    (work, struct cts_platform_data, touch_event_timeout_work.work);

	cts_warn("Touch event timeout work");

	cts_plat_release_all_touch(pdata);
}
#endif

int cts_plat_spi_setup(struct cts_platform_data *pdata)
{
	int ret;

	pdata->spi_client->chip_select = 0;
	pdata->spi_client->mode = SPI_MODE_0;
	pdata->spi_client->bits_per_word = 8;

	ret = spi_setup(pdata->spi_client);
	if (ret)
		cts_err("spi_setup err!");
	return 0;
}

#ifdef CONFIG_CTS_I2C_HOST
int cts_init_platform_data(struct cts_platform_data *pdata,
	struct i2c_client *i2c_client)
#else
int cts_init_platform_data(struct cts_platform_data *pdata,
	struct spi_device *spi)
#endif
{
	struct input_dev *input_dev;
	int ret = 0;

	cts_info("cts_init_platform_data Init");

#ifdef CONFIG_CTS_OF
	{
		struct device *dev;

#ifdef CONFIG_CTS_I2C_HOST
		dev = &i2c_client->dev;
#else
		dev = &spi->dev;
#endif /* CONFIG_CTS_I2C_HOST */
		ret = cts_plat_parse_dt(pdata, dev->of_node);
		if (ret) {
			cts_err("Parse dt failed %d", ret);
			return ret;
		}
	}
#endif /* CONFIG_CTS_OF */

#ifdef CONFIG_CTS_I2C_HOST
	pdata->i2c_client = i2c_client;
	pdata->i2c_client->irq = pdata->irq;
#else
	pdata->spi_client = spi;
	pdata->spi_client->irq = pdata->irq;
#endif /* CONFIG_CTS_I2C_HOST */

	mutex_init(&pdata->dev_lock);
	spin_lock_init(&pdata->irq_lock);

	input_dev = input_allocate_device();
	if (input_dev == NULL) {
		cts_err("Failed to allocate input device.");
		return -ENOMEM;
	}

       /** - Init input device */
	input_dev->name = CFG_CTS_DEVICE_NAME;
	input_dev->name = CFG_CTS_DEVICE_NAME;
#ifdef CONFIG_CTS_I2C_HOST
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &pdata->i2c_client->dev;
#else
	input_dev->id.bustype = BUS_SPI;
	input_dev->dev.parent = &pdata->spi_client->dev;
#endif /* CONFIG_CTS_I2C_HOST */
	input_dev->evbit[0] = BIT_MASK(EV_SYN) |
	    BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
#ifdef CFG_CTS_PALM_DETECT
	set_bit(CFG_CTS_PALM_EVENT, input_dev->keybit);
#endif

#ifdef CFG_CTS_SWAP_XY
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->res_y, 0,
			     0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->res_x, 0,
			     0);
#else /* CFG_CTS_SWAP_XY */
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->res_x, 0,
			     0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->res_y, 0,
			     0);
#endif /* CFG_CTS_SWAP_XY */

	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0,
			     CFG_CTS_MAX_TOUCH_NUM * 2, 0, 0);

	input_set_capability(input_dev, EV_KEY, BTN_TOUCH);

#ifdef CONFIG_CTS_SLOTPROTOCOL
	input_mt_init_slots(input_dev, CFG_CTS_MAX_TOUCH_NUM, 0);
#endif /* CONFIG_CTS_SLOTPROTOCOL */
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	__set_bit(EV_ABS, input_dev->evbit);
	input_set_drvdata(input_dev, pdata);
	ret = input_register_device(input_dev);
	if (ret) {
		cts_err("Failed to register input device");
		return ret;
	}

	pdata->ts_input_dev = input_dev;

#if !defined(CONFIG_GENERIC_HARDIRQS)
	INIT_WORK(&pdata->ts_irq_work, cts_plat_touch_dev_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */

#ifdef CONFIG_CTS_VIRTUALKEY
	{
		u8 vkey_keymap[CFG_CTS_NUM_VKEY] = CFG_CTS_VKEY_KEYCODES;

		memcpy(pdata->vkey_keycodes, vkey_keymap, sizeof(vkey_keymap));
		pdata->vkey_num = CFG_CTS_NUM_VKEY;
	}
#endif /* CONFIG_CTS_VIRTUALKEY */

#ifdef CFG_CTS_GESTURE
	{
		u8 gesture_keymap[CFG_CTS_NUM_GESTURE][2] =
		    CFG_CTS_GESTURE_KEYMAP;

		memcpy(pdata->gesture_keymap, gesture_keymap,
		       sizeof(gesture_keymap));
		pdata->gesture_num = CFG_CTS_NUM_GESTURE;
	}
#endif /* CFG_CTS_GESTURE */

#ifdef CFG_CTS_FORCE_UP
	INIT_DELAYED_WORK(&pdata->touch_event_timeout_work,
			  cts_plat_touch_event_timeout_work);
#endif

#ifndef CONFIG_CTS_I2C_HOST
	pdata->spi_speed = CFG_CTS_SPI_SPEED_KHZ;
	cts_plat_spi_setup(pdata);
#endif
	return 0;
}

int cts_deinit_platform_data(struct cts_platform_data *pdata)
{
	cts_info("De-Init platform_data");
	input_unregister_device(pdata->ts_input_dev);
	return 0;
}

int cts_plat_request_resource(struct cts_platform_data *pdata)
{
	int ret;

	cts_info("Request resource");

	ret = gpio_request_one(pdata->int_gpio, GPIOF_IN,
			       CFG_CTS_DEVICE_NAME "-int");
	if (ret) {
		cts_err("Request INT gpio (%d) failed %d", pdata->int_gpio,
			ret);
		goto err_out;
	}
#ifdef CFG_CTS_HAS_RESET_PIN
	ret = gpio_request_one(pdata->rst_gpio, GPIOF_OUT_INIT_HIGH,
			       CFG_CTS_DEVICE_NAME "-rst");
	if (ret) {
		cts_err("Request RST gpio (%d) failed %d", pdata->rst_gpio,
			ret);
		goto err_free_int;
	}
#endif /* CFG_CTS_HAS_RESET_PIN */

#ifdef CFG_CTS_MANUAL_CS
	ret = gpio_request_one(pdata->cs_gpio, GPIOF_OUT_INIT_HIGH,
			       CFG_CTS_DEVICE_NAME "-cs");
	if (ret) {
		cts_err("Request CS gpio (%d) failed %d", pdata->cs_gpio, ret);
		goto err_request_cs_gpio;
	}
#endif

	return 0;

#ifdef CFG_CTS_MANUAL_CS
err_request_cs_gpio:
#endif

#ifdef CONFIG_CTS_REGULATOR
err_free_rst:
#endif /* CONFIG_CTS_REGULATOR */
#ifdef CFG_CTS_HAS_RESET_PIN
	gpio_free(pdata->rst_gpio);
err_free_int:
#endif /* CFG_CTS_HAS_RESET_PIN */
	gpio_free(pdata->int_gpio);
err_out:
	return ret;
}

void cts_plat_free_resource(struct cts_platform_data *pdata)
{
	cts_info("Free resource");

	if (gpio_is_valid(pdata->int_gpio))
		gpio_free(pdata->int_gpio);

#ifdef CFG_CTS_HAS_RESET_PIN
	if (gpio_is_valid(pdata->rst_gpio))
		gpio_free(pdata->rst_gpio);

#endif /* CFG_CTS_HAS_RESET_PIN */
#ifdef CFG_CTS_MANUAL_CS
	if (gpio_is_valid(pdata->cs_gpio))
		gpio_free(pdata->cs_gpio);

#endif
}

int cts_plat_request_irq(struct cts_platform_data *pdata)
{
	int ret;

	cts_info("Request IRQ");

#ifdef CONFIG_GENERIC_HARDIRQS
	ret = request_threaded_irq(pdata->irq, NULL,
				   cts_plat_irq_handler,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   CFG_CTS_DRIVER_NAME, pdata);
#else /* CONFIG_GENERIC_HARDIRQS */
	ret = request_irq(pdata->irq,
			  cts_plat_irq_handler,
			  IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			  CFG_CTS_DRIVER_NAME, pdata);
#endif /* CONFIG_GENERIC_HARDIRQS */
	if (ret) {
		cts_err("Request IRQ failed %d", ret);
		return ret;
	}

	cts_plat_disable_irq(pdata);

	return 0;
}

void cts_plat_free_irq(struct cts_platform_data *pdata)
{
	free_irq(pdata->irq, pdata);
}

int cts_plat_enable_irq(struct cts_platform_data *pdata)
{
	unsigned long irqflags;

	cts_dbg("Enable IRQ");

	if (pdata->irq > 0) {
		spin_lock_irqsave(&pdata->irq_lock, irqflags);
		if (pdata->irq_is_disable) {	/* && !cts_is_device_suspended(pdata->chip)) */
			cts_dbg("Real enable IRQ");
			enable_irq(pdata->irq);
			pdata->irq_is_disable = false;
		}
		spin_unlock_irqrestore(&pdata->irq_lock, irqflags);

		return 0;
	}

	return -ENODEV;
}

int cts_plat_disable_irq(struct cts_platform_data *pdata)
{
	unsigned long irqflags;

	cts_dbg("Disable IRQ");

	if (pdata->irq > 0) {
		spin_lock_irqsave(&pdata->irq_lock, irqflags);
		if (!pdata->irq_is_disable) {
			cts_dbg("Real disable IRQ");
			disable_irq_nosync(pdata->irq);
			pdata->irq_is_disable = true;
		}
		spin_unlock_irqrestore(&pdata->irq_lock, irqflags);

		return 0;
	}

	return -ENODEV;
}

#ifdef CFG_CTS_HAS_RESET_PIN
int cts_plat_reset_device(struct cts_platform_data *pdata)
{
	/* !!!can not be modified */
	/* !!!can not be modified */
	/* !!!can not be modified */
	cts_info("Reset device");

	gpio_set_value(pdata->rst_gpio, 1);
	mdelay(1);
	gpio_set_value(pdata->rst_gpio, 0);
	mdelay(10);
	gpio_set_value(pdata->rst_gpio, 1);
	mdelay(40);

	return 0;
}

int cts_plat_set_reset(struct cts_platform_data *pdata, int val)
{
	cts_info("Set Reset to %s", val ? "HIGH" : "LOW");
	if (val)
		gpio_set_value(pdata->rst_gpio, 1);
	else
		gpio_set_value(pdata->rst_gpio, 0);

	return 0;
}
#endif /* CFG_CTS_HAS_RESET_PIN */

int cts_plat_get_int_pin(struct cts_platform_data *pdata)
{
	return gpio_get_value(pdata->int_gpio);
}

int cts_plat_power_up_device(struct cts_platform_data *pdata)
{
	cts_info("Power up device");

	return 0;
}

int cts_plat_power_down_device(struct cts_platform_data *pdata)
{
	cts_info("Power down device");

	return 0;
}

int cts_plat_init_touch_device(struct cts_platform_data *pdata)
{
	cts_info("Init touch device");

	return 0;
}

void cts_plat_deinit_touch_device(struct cts_platform_data *pdata)
{
	cts_info("De-init touch device");

#ifndef CONFIG_GENERIC_HARDIRQS
	if (work_pending(&pdata->ts_irq_work))
		cancel_work_sync(&pdata->ts_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */
}

#ifdef CFG_CTS_PALM_DETECT
void cts_report_palm_event(struct cts_platform_data *pdata)
{
	input_report_key(pdata->ts_input_dev, CFG_CTS_PALM_EVENT, 1);
	input_sync(pdata->ts_input_dev);
	msleep(100);
	input_report_key(pdata->ts_input_dev, CFG_CTS_PALM_EVENT, 0);
	input_sync(pdata->ts_input_dev);
}
#endif

int cts_plat_process_touch_msg(struct cts_platform_data *pdata,
			       struct cts_device_touch_msg *msgs, int num)
{
	struct input_dev *input_dev = pdata->ts_input_dev;
	int i;
	int contact = 0;
#ifdef CONFIG_CTS_SLOTPROTOCOL
	static unsigned char finger_last[CFG_CTS_MAX_TOUCH_NUM] = { 0 };
	unsigned char finger_current[CFG_CTS_MAX_TOUCH_NUM] = { 0 };
#endif

	cts_dbg("Process touch %d msgs", num);
	if (num == 0 || num > CFG_CTS_MAX_TOUCH_NUM)
		return 0;

	for (i = 0; i < num; i++) {
		u16 x, y;

		x = le16_to_cpu(msgs[i].x);
		y = le16_to_cpu(msgs[i].y);

#ifdef CFG_CTS_SWAP_XY
		swap(x, y);
#endif /* CFG_CTS_SWAP_XY */
#ifdef CFG_CTS_WRAP_X
		x = wrap(pdata->res_x, x);
#endif /* CFG_CTS_WRAP_X */
#ifdef CFG_CTS_WRAP_Y
		y = wrap(pdata->res_y, y);
#endif /* CFG_CTS_WRAP_Y */
		cts_dbg("  Process touch msg[%d]: id[%u] ev=%u x=%u y=%u p=%u",
			i, msgs[i].id, msgs[i].event, x, y, msgs[i].pressure);
		if (msgs[i].event == CTS_DEVICE_TOUCH_EVENT_DOWN
		    || msgs[i].event == CTS_DEVICE_TOUCH_EVENT_MOVE
		    || msgs[i].event == CTS_DEVICE_TOUCH_EVENT_STAY) {
			if (msgs[i].id < CFG_CTS_MAX_TOUCH_NUM)
				finger_current[msgs[i].id] = 1;
		}
#ifdef CONFIG_CTS_SLOTPROTOCOL
		/* input_mt_slot(input_dev, msgs[i].id); */
		switch (msgs[i].event) {
		case CTS_DEVICE_TOUCH_EVENT_DOWN:
		case CTS_DEVICE_TOUCH_EVENT_MOVE:
		case CTS_DEVICE_TOUCH_EVENT_STAY:
			contact++;
			input_mt_slot(input_dev, msgs[i].id);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,
						   true);
			input_report_abs(input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
			input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
					 msgs[i].pressure);
			input_report_abs(input_dev, ABS_MT_PRESSURE,
					 msgs[i].pressure);
			break;

		case CTS_DEVICE_TOUCH_EVENT_UP:
			/* input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false); */
			break;

		default:
			cts_warn
			    ("Process touch msg with unknwon event %u id %u",
			     msgs[i].event, msgs[i].id);
			break;
		}
#else /* CONFIG_CTS_SLOTPROTOCOL */
    /**
     * If the driver reports one of BTN_TOUCH or ABS_PRESSURE
     * in addition to the ABS_MT events, the last SYN_MT_REPORT event
     * may be omitted. Otherwise, the last SYN_REPORT will be dropped
     * by the input core, resulting in no zero-contact event
     * reaching userland.
     */
		switch (msgs[i].event) {
		case CTS_DEVICE_TOUCH_EVENT_DOWN:
		case CTS_DEVICE_TOUCH_EVENT_MOVE:
		case CTS_DEVICE_TOUCH_EVENT_STAY:
			contact++;
			input_report_abs(input_dev, ABS_MT_PRESSURE,
					 msgs[i].pressure);
			input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
					 msgs[i].pressure);
			input_report_key(input_dev, BTN_TOUCH, 1);
			input_report_abs(input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
			input_mt_sync(input_dev);
			break;

		case CTS_DEVICE_TOUCH_EVENT_UP:
			break;
		default:
			cts_warn
			    ("Process touch msg with unknwon event %u id %u",
			     msgs[i].event, msgs[i].id);
			break;
		}
#endif /* CONFIG_CTS_SLOTPROTOCOL */
	}

#ifdef CONFIG_CTS_SLOTPROTOCOL
	for (i = 0; i < CFG_CTS_MAX_TOUCH_NUM; i++) {
		if (finger_last[i] != 0 && finger_current[i] == 0) {
			input_mt_slot(input_dev, i);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,
						   false);
		}
		finger_last[i] = finger_current[i];
	}
	input_report_key(input_dev, BTN_TOUCH, contact > 0);
#else
	if (contact == 0) {
		input_report_key(input_dev, BTN_TOUCH, 0);
		input_mt_sync(input_dev);
	}
#endif
	input_sync(input_dev);
#ifdef CFG_CTS_FORCE_UP
	if (contact) {
		struct chipone_ts_data *cts_data;

		cts_data =
		    container_of(pdata->cts_dev, struct chipone_ts_data,
				 cts_dev);

		if (delayed_work_pending(&pdata->touch_event_timeout_work)) {
			mod_delayed_work(cts_data->workqueue,
					 &pdata->touch_event_timeout_work,
					 msecs_to_jiffies(100));
		} else {
			queue_delayed_work(cts_data->workqueue,
					   &pdata->touch_event_timeout_work,
					   msecs_to_jiffies(100));
		}
	} else {
		cancel_delayed_work_sync(&pdata->touch_event_timeout_work);
	}
#endif
	return 0;
}

int cts_plat_release_all_touch(struct cts_platform_data *pdata)
{
	struct input_dev *input_dev = pdata->ts_input_dev;

#if defined(CONFIG_CTS_SLOTPROTOCOL)
	int id;
#endif /* CONFIG_CTS_SLOTPROTOCOL */

	cts_info("Release all touch");

#ifdef CONFIG_CTS_SLOTPROTOCOL
	for (id = 0; id < CFG_CTS_MAX_TOUCH_NUM; id++) {
		input_mt_slot(input_dev, id);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
	}
	input_report_key(input_dev, BTN_TOUCH, 0);
#else
	input_report_key(input_dev, BTN_TOUCH, 0);
	input_mt_sync(input_dev);
#endif /* CONFIG_CTS_SLOTPROTOCOL */
	input_sync(input_dev);

	return 0;
}

#ifdef CONFIG_CTS_VIRTUALKEY
int cts_plat_init_vkey_device(struct cts_platform_data *pdata)
{
	int i;

	cts_info("Init VKey");

	pdata->vkey_state = 0;

	for (i = 0; i < pdata->vkey_num; i++) {
		input_set_capability(pdata->ts_input_dev,
				     EV_KEY, pdata->vkey_keycodes[i]);
	}

	return 0;
}

void cts_plat_deinit_vkey_device(struct cts_platform_data *pdata)
{
	cts_info("De-init VKey");

	pdata->vkey_state = 0;
}

int cts_plat_process_vkey(struct cts_platform_data *pdata, u8 vkey_state)
{
	u8 event;
	int i;

	event = pdata->vkey_state ^ vkey_state;

	cts_dbg("Process vkey state=0x%02x, event=0x%02x", vkey_state, event);

	for (i = 0; i < pdata->vkey_num; i++) {
		input_report_key(pdata->ts_input_dev,
				 pdata->vkey_keycodes[i],
				 vkey_state & BIT(i) ? 1 : 0);
	}

	pdata->vkey_state = vkey_state;

	return 0;
}

int cts_plat_release_all_vkey(struct cts_platform_data *pdata)
{
	int i;

	cts_info("Release all vkeys");

	for (i = 0; i < pdata->vkey_num; i++) {
		if (pdata->vkey_state & BIT(i)) {
			input_report_key(pdata->ts_input_dev,
					 pdata->vkey_keycodes[i], 0);
		}
	}

	pdata->vkey_state = 0;

	return 0;
}
#endif /* CONFIG_CTS_VIRTUALKEY */

#ifdef CFG_CTS_GESTURE
int cts_plat_enable_irq_wake(struct cts_platform_data *pdata)
{
	int ret;

	cts_info("Enable IRQ wake");

	if (pdata->irq > 0) {
		ret = enable_irq_wake(pdata->irq);
		if (ret < 0) {
			cts_err("Enable irq wake failed");
			return -EINVAL;
		}
		pdata->irq_wake_enabled = true;
		return 0;
	}

	cts_warn("Enable irq wake while irq invalid %d", pdata->irq);
	return -ENODEV;
}

int cts_plat_disable_irq_wake(struct cts_platform_data *pdata)
{
	int ret;

	cts_info("Disable IRQ wake");

	if (pdata->irq > 0) {
		ret = disable_irq_wake(pdata->irq);
		if (ret < 0) {
			cts_warn("Disable irq wake while already disabled");
			return -EINVAL;
		}
		pdata->irq_wake_enabled = false;
		return 0;
	}

	cts_warn("Disable irq wake while irq invalid %d", pdata->irq);
	return -ENODEV;
}

int cts_plat_init_gesture(struct cts_platform_data *pdata)
{
	int i;

	cts_info("Init gesture");

	/* TODO: If system will issure enable/disable command, comment following line. */
	/* cts_enable_gesture_wakeup(pdata->cts_dev); */

	for (i = 0; i < pdata->gesture_num; i++) {
		input_set_capability(pdata->ts_input_dev, EV_KEY,
				     pdata->gesture_keymap[i][1]);
	}

	return 0;
}

void cts_plat_deinit_gesture(struct cts_platform_data *pdata)
{
	cts_info("De-init gesture");
}

int cts_plat_process_gesture_info(struct cts_platform_data *pdata,
				  struct cts_device_gesture_info *gesture_info)
{
	int i;

	cts_info("Process gesture, id=0x%02x", gesture_info->gesture_id);

#if defined(CFG_CTS_GESTURE_REPORT_KEY)
	for (i = 0; i < CFG_CTS_NUM_GESTURE; i++) {
		if (gesture_info->gesture_id == pdata->gesture_keymap[i][0]) {
			cts_info("Report key[%u]", pdata->gesture_keymap[i][1]);
			input_report_key(pdata->ts_input_dev,
					 pdata->gesture_keymap[i][1], 1);
			input_sync(pdata->ts_input_dev);

			input_report_key(pdata->ts_input_dev,
					 pdata->gesture_keymap[i][1], 0);
			input_sync(pdata->ts_input_dev);

			return 0;
		}
	}
#endif /* CFG_CTS_GESTURE_REPORT_KEY */

	cts_warn("Process unrecognized gesture id=%u",
		 gesture_info->gesture_id);

	return -EINVAL;
}

#endif /* CFG_CTS_GESTURE */
