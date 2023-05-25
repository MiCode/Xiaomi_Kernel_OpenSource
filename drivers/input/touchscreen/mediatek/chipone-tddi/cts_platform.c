#define LOG_TAG         "Plat"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "linux/spi/spi.h"
//#include "mt_spi.h"
#include "cts_firmware.h"

int tpd_rst_gpio_index = 0;
int tpd_int_gpio_index = 1;

#ifdef CFG_CTS_FW_LOG_REDIRECT
size_t cts_plat_get_max_fw_log_size(struct cts_platform_data *pdata)
{
    return CTS_FW_LOG_BUF_LEN;
}

u8 *cts_plat_get_fw_log_buf(struct cts_platform_data *pdata,
    size_t size)
{
    return pdata->fw_log_buf;
}
#endif

#ifdef CONFIG_CTS_I2C_HOST
size_t cts_plat_get_max_i2c_xfer_size(struct cts_platform_data *pdata)
{
#ifdef TPD_SUPPORT_I2C_DMA
    if (pdata->dma_available) {
        return CFG_CTS_MAX_I2C_XFER_SIZE;
    } else {
        return CFG_CTS_MAX_I2C_FIFO_XFER_SIZE;
    }
#else /* TPD_SUPPORT_I2C_DMA */
    return CFG_CTS_MAX_I2C_XFER_SIZE;
#endif /* TPD_SUPPORT_I2C_DMA */
}

u8 *cts_plat_get_i2c_xfer_buf(struct cts_platform_data *pdata,
    size_t xfer_size)
{
#ifdef TPD_SUPPORT_I2C_DMA
    if (pdata->dma_available && xfer_size > CFG_CTS_MAX_I2C_FIFO_XFER_SIZE) {
        return pdata->i2c_dma_buff_va;
    } else
#endif /* TPD_SUPPORT_I2C_DMA */
        return pdata->i2c_fifo_buf;
}

int cts_plat_i2c_write(struct cts_platform_data *pdata, u8 i2c_addr,
        const void *src, size_t len, int retry, int delay)
{
    int ret = 0, retries = 0;

#ifdef TPD_SUPPORT_I2C_DMA
    struct i2c_msg msg = {
        .addr   = i2c_addr,
        .flags  = !I2C_M_RD,
        .len    = len,
        .timing = 300,
    };

    if (pdata->dma_available && len > CFG_CTS_MAX_I2C_FIFO_XFER_SIZE) {
        msg.ext_flag = (pdata->i2c_client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
        msg.buf = (u8 *)pdata->i2c_dma_buff_pa;
        memcpy(pdata->i2c_dma_buff_va, src, len);
    } else {
        msg.buf = (u8 *)src;
    }
    msg.len  = len;
#else
    struct i2c_msg msg = {
        .addr  = i2c_addr,
        .flags = !I2C_M_RD,
        .len   = len,
        .buf   = (u8 *)src,
    };
#endif /* TPD_SUPPORT_I2C_DMA */

    do {
        ret = i2c_transfer(pdata->i2c_client->adapter, &msg, 1);
        if (ret != 1) {
            if (ret >= 0) {
                ret = -EIO;
            }

            if (delay) {
                mdelay(delay);
            }
            continue;
        } else {
            return 0;
        }
    } while (++retries < retry);

    return ret;
}

int cts_plat_i2c_read(struct cts_platform_data *pdata, u8 i2c_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
        int retry, int delay)
{
    int num_msg, ret = 0, retries = 0;

#ifdef TPD_SUPPORT_I2C_DMA
    struct i2c_msg msgs[2] = {
        {
            .addr   = i2c_addr,
            .flags  = !I2C_M_RD,
            .len    = wlen,
            .buf    = (u8 *)wbuf,
            .timing = 300,
        },
        {
            .addr     = i2c_addr,
            .flags    = I2C_M_RD,
            .len      = rlen,
            .timing   = 300,
        },
    };

    if (pdata->dma_available && rlen > CFG_CTS_MAX_I2C_FIFO_XFER_SIZE) {
        msgs[1].ext_flag = (pdata->i2c_client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
        msgs[1].buf      = (u8 *)pdata->i2c_dma_buff_pa;
    } else {
        msgs[1].buf = (u8 *)rbuf;
    }
#else /* TPD_SUPPORT_I2C_DMA */
    struct i2c_msg msgs[2] = {
        {
            .addr  = i2c_addr,
            .flags = !I2C_M_RD,
            .buf   = (u8 *)wbuf,
            .len   = wlen
        },
        {
            .addr  = i2c_addr,
            .flags = I2C_M_RD,
            .buf   = (u8 *)rbuf,
            .len   = rlen
        }
    };
#endif /* TPD_SUPPORT_I2C_DMA */

    if (wbuf == NULL || wlen == 0) {
        num_msg = 1;
    } else {
        num_msg = 2;
    }

    do {
        ret = i2c_transfer(pdata->i2c_client->adapter,
                msgs + ARRAY_SIZE(msgs) - num_msg, num_msg);

        if (ret != num_msg) {
            if (ret >= 0) {
                ret = -EIO;
            }

            if (delay) {
                mdelay(delay);
            }
            continue;
        } else {
#ifdef TPD_SUPPORT_I2C_DMA
    if (pdata->dma_available && rlen > CFG_CTS_MAX_I2C_FIFO_XFER_SIZE) {
            memcpy(rbuf, pdata->i2c_dma_buff_va, rlen);
    }
#endif /* TPD_SUPPORT_I2C_DMA */

            return 0;
        }
    } while (++retries < retry);

    return ret;
}

int cts_plat_is_i2c_online(struct cts_platform_data *pdata, u8 i2c_addr)
{
    u8 dummy_bytes[2] = {0x00, 0x00};
    int ret;

    ret = cts_plat_i2c_write(pdata, i2c_addr, dummy_bytes, sizeof(dummy_bytes), 5, 2);
    if (ret) {
        cts_err("!!! I2C addr 0x%02x is offline !!!", i2c_addr);
        return false;
    } else {
        cts_dbg("I2C addr 0x%02x is online", i2c_addr);
        return true;
    }
}
#else

#ifdef CFG_MTK_LEGEND_PLATFORM
struct mt_chip_conf cts_spi_conf_mt65xx = {
    .setuptime = 15,
    .holdtime = 15,
    .high_time = 21, //for mt6582, 104000khz/(4+4) = 130000khz
    .low_time = 21,
    .cs_idletime = 20,
    .ulthgh_thrsh = 0,

    .cpol = 0,
    .cpha = 0,

    .rx_mlsb = 1,
    .tx_mlsb = 1,

    .tx_endian = 0,
    .rx_endian = 0,

    .com_mod = FIFO_TRANSFER,
    .pause = 1,
    .finish_intr = 1,
    .deassert = 0,
    .ulthigh = 0,
    .tckdly = 0,
};

typedef enum {
    SPEED_500KHZ = 500,
    SPEED_1MHZ = 1000,
    SPEED_2MHZ = 2000,
    SPEED_3MHZ = 3000,
    SPEED_4MHZ = 4000,
    SPEED_6MHZ = 6000,
    SPEED_8MHZ = 8000,
    SPEED_KEEP,
    SPEED_UNSUPPORTED
} SPI_SPEED;

void cts_plat_spi_set_mode(struct spi_device *spi, SPI_SPEED speed, int flag)
{
    struct mt_chip_conf *mcc = &cts_spi_conf_mt65xx;
    if (flag == 0) {
        mcc->com_mod = FIFO_TRANSFER;
    } else {
        mcc->com_mod = DMA_TRANSFER;
    }

    switch (speed) {
    case SPEED_500KHZ:
        mcc->high_time = 120;
        mcc->low_time = 120;
        break;
    case SPEED_1MHZ:
        mcc->high_time = 60;
        mcc->low_time = 60;
        break;
    case SPEED_2MHZ:
        mcc->high_time = 30;
        mcc->low_time = 30;
        break;
    case SPEED_3MHZ:
        mcc->high_time = 20;
        mcc->low_time = 20;
        break;
    case SPEED_4MHZ:
        mcc->high_time = 15;
        mcc->low_time = 15;
        break;
    case SPEED_6MHZ:
        mcc->high_time = 10;
        mcc->low_time = 10;
        break;
    case SPEED_8MHZ:
        mcc->high_time = 8;
        mcc->low_time = 8;
        break;
    case SPEED_KEEP:
    case SPEED_UNSUPPORTED:
        break;
    }
    if (spi_setup(spi) < 0) {
        cts_err("Failed to set spi");
    }
}

int cts_plat_spi_setup(struct cts_platform_data *pdata)
{
    pdata->spi_client->mode = SPI_MODE_0;
    pdata->spi_client->bits_per_word = 8;
//  pdata->spi_client->chip_select = 0;
    pdata->spi_client->controller_data = (void *)&cts_spi_conf_mt65xx;
    spi_setup(pdata->spi_client);
    cts_plat_spi_set_mode(pdata->spi_client, pdata->spi_speed, 0);
    return 0;
}
#else
int cts_plat_spi_setup(struct cts_platform_data *pdata)
{
    pdata->spi_client->mode = SPI_MODE_0;
    pdata->spi_client->bits_per_word = 8;
    if(pdata->spi_client->max_speed_hz > 6000000)
    {
        pdata->spi_client->max_speed_hz = 6000000;
    }
    spi_setup(pdata->spi_client);
    return 0;
}
#endif


#ifdef CFG_CTS_MANUAL_CS
int cts_plat_set_cs(struct cts_platform_data *pdata, u8 val)
{
    if (val) {
        pinctrl_select_state(pdata->pinctrl1, pdata->spi_cs_high);
    }
    else {
        pinctrl_select_state(pdata->pinctrl1, pdata->spi_cs_low);
    }
    return 0;
}
#endif

int cts_spi_send_recv(struct cts_platform_data *pdata, size_t len , u8 *tx_buffer, u8 *rx_buffer)
{
	struct chipone_ts_data *cts_data;
    struct spi_message msg;
    struct spi_transfer cmd = {
        .cs_change = 0,
        .delay_usecs = 0,
        .speed_hz = pdata->spi_speed * 1000,
        .tx_buf = tx_buffer,
        .rx_buf = rx_buffer,
        .len    = len,
        //.tx_dma = 0,
        //.rx_dma = 0,
        .bits_per_word = 8,
    };
    int ret = 0 ;
	cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(pdata, 0);
#endif
    spi_message_init(&msg);
    spi_message_add_tail(&cmd,  &msg);
    ret = spi_sync(cts_data->spi_client, &msg);
    if (ret) {
        cts_err("spi_sync failed.\n");
    }
#ifdef CFG_CTS_MANUAL_CS
    cts_plat_set_cs(pdata, 1);
#endif
    return ret;
}

size_t cts_plat_get_max_spi_xfer_size(struct cts_platform_data *pdata)
{
    return CFG_CTS_MAX_SPI_XFER_SIZE;
}

u8 *cts_plat_get_spi_xfer_buf(struct cts_platform_data *pdata,
    size_t xfer_size)
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
		cts_err("write too much data:wlen=%zd\n", len);
		return -EIO;
	}

    if (pdata->cts_dev->rtdata.program_mode) {
    	pdata->spi_tx_buf[0] = dev_addr;
    	memcpy(&pdata->spi_tx_buf[1], src, len);

        do {
            ret = cts_spi_send_recv(pdata, len + 1, pdata->spi_tx_buf, pdata->spi_rx_buf);
            if (ret) {
                if (delay) {
                    mdelay(delay);
                }
            } else {
                return 0;
            }
        } while (++retries < retry);
    }
    else {
        data_len = len - 2;
    	pdata->spi_tx_buf[0] = dev_addr;
        pdata->spi_tx_buf[1] = *((u8 *)src + 1);
        pdata->spi_tx_buf[2] = *((u8 *)src);
        put_unaligned_le16(data_len, &pdata->spi_tx_buf[3]);
        crc = (u16)cts_crc32(pdata->spi_tx_buf, 5);
    	put_unaligned_le16(crc, &pdata->spi_tx_buf[5]);
    	memcpy(&pdata->spi_tx_buf[7], (char *)src + 2, data_len);
    	crc = (u16)cts_crc32((char *)src + 2, data_len);
    	put_unaligned_le16(crc, &pdata->spi_tx_buf[7+data_len]);
        do {
            ret = cts_spi_send_recv(pdata, len + 7, pdata->spi_tx_buf, pdata->spi_rx_buf);
			udelay(10 * data_len);
            if (ret) {
                if (delay) {
                    mdelay(delay);
                }
            } else {
                return 0;
            }
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

	if (wlen > CFG_CTS_MAX_SPI_XFER_SIZE || rlen > CFG_CTS_MAX_SPI_XFER_SIZE) {
		cts_err("write/read too much data:wlen=%zd, rlen=%zd", wlen, rlen);
		return -EIO;
	}

	if (pdata->cts_dev->rtdata.program_mode)
	{
		pdata->spi_tx_buf[0] = dev_addr | 0x01;
		memcpy(&pdata->spi_tx_buf[1], wbuf, wlen);
		do {
			ret = cts_spi_send_recv(pdata, rlen + 5, pdata->spi_tx_buf, pdata->spi_rx_buf);
			if (ret) {
				if (delay) {
					mdelay(delay);
				}
				continue;
			}
			memcpy(rbuf, pdata->spi_rx_buf+5, rlen);
			return 0;
		} while(++retries < retry);
	}
	else {
		do {
			if (wlen != 0) {
            	pdata->spi_tx_buf[0] = dev_addr | 0x01;
                pdata->spi_tx_buf[1] = wbuf[1];
                pdata->spi_tx_buf[2] = wbuf[0];
                put_unaligned_le16(rlen, &pdata->spi_tx_buf[3]);
                crc = (u16)cts_crc32(pdata->spi_tx_buf, 5);
                put_unaligned_le16(crc, &pdata->spi_tx_buf[5]);
		        ret = cts_spi_send_recv(pdata, 7, pdata->spi_tx_buf, pdata->spi_rx_buf);
				if (ret) {
					if (delay) {
						mdelay(delay);
					}
					continue;
				}
			}
			memset(pdata->spi_tx_buf, 0, 7);
			pdata->spi_tx_buf[0] = dev_addr | 0x01;
			udelay(100);
 			ret = cts_spi_send_recv(pdata, rlen + 2, pdata->spi_tx_buf, pdata->spi_rx_buf);
			if (ret) {
				if (delay) {
					mdelay(delay);
				}
				continue;
			}
			memcpy(rbuf, pdata->spi_rx_buf, rlen);
			crc = (u16)cts_crc32(pdata->spi_rx_buf, rlen);
			if (get_unaligned_le16(&pdata->spi_rx_buf[rlen]) != crc) {
			    continue;
			}
			return 0;
		} while (++retries < retry);
	}
	if (retries >= retry) {
		cts_err("cts_plat_spi_read error");
	}

    return -ENODEV;
}

int cts_plat_spi_read_delay_idle(struct cts_platform_data *pdata, u8 dev_addr,
        const u8 *wbuf, size_t wlen, void *rbuf, size_t rlen,
        int retry, int delay, int idle)
{
    int ret = 0, retries = 0;
    u16 crc;

	if (wlen > CFG_CTS_MAX_SPI_XFER_SIZE || rlen > CFG_CTS_MAX_SPI_XFER_SIZE) {
		cts_err("write/read too much data:wlen=%zd, rlen=%zd", wlen, rlen);
		return -EIO;
	}

	if (pdata->cts_dev->rtdata.program_mode)
	{
		pdata->spi_tx_buf[0] = dev_addr | 0x01;
		memcpy(&pdata->spi_tx_buf[1], wbuf, wlen);
		do {
			ret = cts_spi_send_recv(pdata, rlen + 5, pdata->spi_tx_buf, pdata->spi_rx_buf);
			if (ret) {
				if (delay) {
					mdelay(delay);
				}
				continue;
			}
			memcpy(rbuf, pdata->spi_rx_buf+5, rlen);
			return 0;
		} while(++retries < retry);
	}
	else {
		do {
			if (wlen != 0) {
            	pdata->spi_tx_buf[0] = dev_addr | 0x01;
                pdata->spi_tx_buf[1] = wbuf[1];
                pdata->spi_tx_buf[2] = wbuf[0];
                put_unaligned_le16(rlen, &pdata->spi_tx_buf[3]);
                crc = (u16)cts_crc32(pdata->spi_tx_buf, 5);
                put_unaligned_le16(crc, &pdata->spi_tx_buf[5]);
		        ret = cts_spi_send_recv(pdata, 7, pdata->spi_tx_buf, pdata->spi_rx_buf);
				if (ret) {
					if (delay) {
						mdelay(delay);
					}
					continue;
				}
			}
			memset(pdata->spi_tx_buf, 0, 7);
			pdata->spi_tx_buf[0] = dev_addr | 0x01;
			udelay(idle);
 			ret = cts_spi_send_recv(pdata, rlen + 2, pdata->spi_tx_buf, pdata->spi_rx_buf);
			if (ret) {
				if (delay) {
					mdelay(delay);
				}
				continue;
			}
			memcpy(rbuf, pdata->spi_rx_buf, rlen);
			crc = (u16)cts_crc32(pdata->spi_rx_buf, rlen);
			if (get_unaligned_le16(&pdata->spi_rx_buf[rlen]) != crc) {
			    continue;
			}
			return 0;
		} while (++retries < retry);
	}
	if (retries >= retry) {
		cts_err("cts_plat_spi_read error");
	}

    return -ENODEV;
}

int cts_plat_is_normal_mode(struct cts_platform_data *pdata)
{
	struct chipone_ts_data *cts_data;
	u8 tx_buf[4] = {0};
	u16 fwid;
	u32 addr;
	int ret;

	pdata->cts_dev->rtdata.addr_width = 2;
	pdata->cts_dev->rtdata.program_mode = false;
	cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
	addr = CTS_DEVICE_FW_REG_CHIP_TYPE;
    put_unaligned_be16(addr, tx_buf);
	ret = cts_plat_spi_read(pdata, CTS_DEV_NORMAL_MODE_SPIADDR, tx_buf, 2, &fwid, 2, 3, 10);
    fwid = be16_to_cpu(fwid);
    if (ret || !cts_is_fwid_valid(fwid)) {
        return false;
    }

    return true;
}
#endif

static void cts_plat_handle_irq(struct cts_platform_data *pdata)
{
    int ret;

    cts_dbg("Handle IRQ");

    cts_lock_device(pdata->cts_dev);
    ret = cts_irq_handler(pdata->cts_dev);
    if (ret) {
        cts_err("Device handle IRQ failed %d", ret);
    }
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
    cts_data = container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);

    cts_plat_disable_irq(pdata);

    queue_work(cts_data->workqueue, &pdata->ts_irq_work);
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

#ifdef CFG_CTS_FORCE_UP
/*
static void cts_plat_touch_event_timeout(unsigned long arg)
{
    cts_warn("Touch event timeout");

    cts_plat_release_all_touch((struct cts_platform_data *)arg);
}*/
#endif

#ifndef CONFIG_CTS_I2C_HOST
static int cts_plat_init_dts(struct cts_platform_data *pdata,
                                    struct spi_device *spi)
{
#ifdef CFG_CTS_MANUAL_CS
    struct device_node *node;

    spi->dev.of_node = of_find_compatible_node(NULL, NULL, "chipone-tddi");
    pdata->pinctrl1 = devm_pinctrl_get(&spi->dev);
    node = of_find_compatible_node(NULL, NULL, "chipone-tddi");
    if (node) {
        pdata->spi_cs_low = pinctrl_lookup_state(pdata->pinctrl1, "spi_cs_low");
        if (IS_ERR(pdata->spi_cs_low)) {
            cts_err("Cannot find pinctrl spi cs high!\n");
            return -1;
        }
        pdata->spi_cs_high = pinctrl_lookup_state(pdata->pinctrl1, "spi_cs_high");
        if (IS_ERR(pdata->spi_cs_high)) {
            return -1;
        }
        return 0;
    }
    return -1;
#else
    return 0;
#endif
}
#endif

#ifdef CONFIG_CTS_OF
static int cts_plat_parse_dt(struct cts_platform_data *pdata,
        struct device_node *dev_node)
{
    int ret;

    cts_info("Parse device tree");

	ret = of_property_read_u32(dev_node, CFG_CTS_OF_X_RESOLUTION_NAME,
            &pdata->res_x);
    if (ret) {
        cts_warn("Parse X resolution from dt failed %d", ret);
        //return ret;
    }
    cts_info("  %-12s: %d", "X resolution", pdata->res_x);

    ret = of_property_read_u32(dev_node, CFG_CTS_OF_Y_RESOLUTION_NAME,
            &pdata->res_y);
    if (ret) {
        cts_warn("Parse Y resolution from dt failed %d", ret);
        //return ret;
    }
    cts_info("  %-12s: %d", "Y resolution", pdata->res_y);

    return 0;
}
#endif /* CONFIG_CTS_OF */

#ifdef CONFIG_CTS_I2C_HOST
int cts_init_platform_data(struct cts_platform_data *pdata,
        struct i2c_client *i2c_client)
#else
int cts_init_platform_data(struct cts_platform_data *pdata,
        struct spi_device *spi)

#endif
{
    struct device_node *node = NULL;
	u32 ints[2] = { 0, 0 };
    int ret = 0;
	struct input_dev *input_dev;

    cts_info("Init");
	
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
#else
	pdata->spi_client = spi;
#endif
    //pdata->ts_input_dev = tpd->dev;

    rt_mutex_init(&pdata->dev_lock);

#if !defined(CONFIG_GENERIC_HARDIRQS)
    /* Init work for bottom half of interrupt */
    INIT_WORK(&pdata->ts_irq_work, cts_plat_touch_dev_irq_work);
#endif /* CONFIG_GENERIC_HARDIRQS */

    if ((node = of_find_matching_node(node, touch_of_match)) == NULL) {
        cts_err("Find touch eint node failed");
        return -ENODATA;
    }
    if (of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints)) == 0) {
		gpio_set_debounce(ints[0], ints[1]);
	} else {
		cts_info("Debounce time not found");
	}
    pdata->irq = irq_of_parse_and_map(node, 0);
    if (pdata->irq == 0) {
        cts_err("Parse irq in dts failed");
        return -ENODEV;
    }

#ifdef CONFIG_CTS_I2C_HOST
    pdata->i2c_client = i2c_client;
    pdata->i2c_client->irq = pdata->irq;
#else
	pdata->spi_client = spi;
	pdata->spi_client->irq = pdata->irq;
#endif /* CONFIG_CTS_I2C_HOST */

    spin_lock_init(&pdata->irq_lock);

#ifdef CONFIG_CTS_VIRTUALKEY
    pdata->vkey_num = tpd_dts_data.tpd_keycnt;
#endif /* CONFIG_CTS_VIRTUALKEY */

#ifdef CFG_CTS_GESTURE
    {
        u8 gesture_keymap[CFG_CTS_NUM_GESTURE][2] = CFG_CTS_GESTURE_KEYMAP;
        memcpy(pdata->gesture_keymap, gesture_keymap, sizeof(gesture_keymap));
        pdata->gesture_num = CFG_CTS_NUM_GESTURE;
    }
#endif /* CFG_CTS_GESTURE */

#ifdef TPD_SUPPORT_I2C_DMA
    tpd->dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
    pdata->i2c_dma_buff_va = (u8 *)dma_alloc_coherent(&tpd->dev->dev,
            CFG_CTS_MAX_I2C_XFER_SIZE, &pdata->i2c_dma_buff_pa, GFP_KERNEL);
    if (pdata->i2c_dma_buff_va == NULL) {
        cts_err("Allocate I2C DMA Buffer failed!");
        //return -ENOMEM;
    } else {
        pdata->dma_available = true;
    }
#endif /* TPD_SUPPORT_I2C_DMA */

#ifdef CFG_CTS_FORCE_UP
   // setup_timer(&pdata->touch_event_timeout_timer,
   //     cts_plat_touch_event_timeout, (unsigned long)pdata);
#endif

#ifndef CONFIG_CTS_I2C_HOST
    ret = cts_plat_init_dts(pdata, spi);
    pdata->spi_speed = CFG_CTS_SPI_SPEED_KHZ;
    cts_plat_spi_setup(pdata);
#endif

	cts_info("Init input device");

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
	input_dev->evbit[0] =	BIT_MASK(EV_SYN) |
							BIT_MASK(EV_KEY) |
							BIT_MASK(EV_ABS);

#ifdef CFG_CTS_SWAP_XY
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			0, pdata->res_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			0, pdata->res_x, 0, 0);
#else /* CFG_CTS_SWAP_XY */
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			0, pdata->res_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			0, pdata->res_y, 0, 0);
#endif /* CFG_CTS_SWAP_XY */

	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID,0, CFG_CTS_MAX_TOUCH_NUM, 0, 0);
	
	input_set_capability(input_dev, EV_KEY, BTN_TOUCH);

//#ifdef CONFIG_CTS_SLOTPROTOCOL
//	input_mt_init_slots(input_dev, CFG_CTS_MAX_TOUCH_NUM, 0);
//#endif /* CONFIG_CTS_SLOTPROTOCOL */
//	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
//	__set_bit(EV_ABS, input_dev->evbit);
	input_set_drvdata(input_dev, pdata);
	ret = input_register_device(input_dev);
	if (ret) {
		cts_err("Failed to register input device");
		return ret;
	}

	pdata->ts_input_dev = input_dev;

    return ret;
}

int cts_plat_request_resource(struct cts_platform_data *pdata)
{
    cts_info("Request resource");

    tpd_gpio_as_int(tpd_int_gpio_index);
    tpd_gpio_output(tpd_rst_gpio_index, 1);

    return 0;
}

int cts_plat_request_irq(struct cts_platform_data *pdata)
{
    int ret;
	struct cts_device *cts_dev = pdata->cts_dev;
	struct cts_device_fwdata *fwdata = &cts_dev->fwdata;
	u8 int_mode;

    cts_info("Request IRQ");
	int_mode = fwdata->int_mode;
	cts_info("Get fw int mode:%d, request %s IRQ", int_mode,
		int_mode==0?"falling" : "rising");
#ifdef CONFIG_GENERIC_HARDIRQS
	if (int_mode) {
	    ret = request_threaded_irq(pdata->irq, NULL,
			cts_plat_irq_handler, IRQF_TRIGGER_RISING | IRQF_ONESHOT,
	            CFG_CTS_DRIVER_NAME, pdata);
	}
	else {
	    ret = request_threaded_irq(pdata->irq, NULL,
			cts_plat_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	            CFG_CTS_DRIVER_NAME, pdata);
	}
#else /* CONFIG_GENERIC_HARDIRQS */
	if (int_mode) {
	    ret = request_irq(pdata->irq,
	        cts_plat_irq_handler, IRQF_TRIGGER_RISING | IRQF_ONESHOT,
	            CFG_CTS_DRIVER_NAME, pdata);
	}
	else {
	    ret = request_irq(pdata->irq,
	        cts_plat_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	            CFG_CTS_DRIVER_NAME, pdata);
	}
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

void cts_plat_free_resource(struct cts_platform_data *pdata)
{
    cts_info("Free resource");

    /**
     * Note:
     *    If resource request without managed, should free all resource
     *    requested in cts_plat_request_resource().
     */

#ifdef TPD_SUPPORT_I2C_DMA
    if (pdata->i2c_dma_buff_va) {
        dma_free_coherent(&tpd->dev->dev, CFG_CTS_MAX_I2C_XFER_SIZE,
            pdata->i2c_dma_buff_va, pdata->i2c_dma_buff_pa);
        pdata->i2c_dma_buff_va = NULL;
        pdata->i2c_dma_buff_pa = 0;
    }
#endif /* TPD_SUPPORT_I2C_DMA */
}

int cts_plat_enable_irq(struct cts_platform_data *pdata)
{
    unsigned long irqflags;

    cts_dbg("Enable IRQ");

    if (pdata->irq > 0) {
        spin_lock_irqsave(&pdata->irq_lock, irqflags);
        if (pdata->irq_is_disable)/* && !cts_is_device_suspended(pdata->chip)) */{
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
    cts_info("Reset device");

    tpd_gpio_output(tpd_rst_gpio_index, 0);
    mdelay(1);
    tpd_gpio_output(tpd_rst_gpio_index, 1);
    mdelay(50);

    return 0;
}

int cts_plat_set_reset(struct cts_platform_data *pdata, int val)
{
    cts_info("Set Reset");
    if (val) {
        tpd_gpio_output(tpd_rst_gpio_index, 1);
    }
    else {
        tpd_gpio_output(tpd_rst_gpio_index, 0);
    }
    return 0;
}
#endif /* CFG_CTS_HAS_RESET_PIN */

int cts_plat_get_int_pin(struct cts_platform_data *pdata)
{
    /* MTK platform can not get INT pin value */
    return -ENOTSUPP;
}

int cts_plat_init_touch_device(struct cts_platform_data *pdata)
{
    cts_info("Init touch device");

    return input_mt_init_slots(pdata->ts_input_dev,
        tpd_dts_data.touch_max_num, INPUT_MT_DIRECT);

    return 0;
}

void cts_plat_deinit_touch_device(struct cts_platform_data *pdata)
{
    cts_info("De-init touch device");

    if (pdata->ts_input_dev) {
#ifndef CONFIG_GENERIC_HARDIRQS
        if (work_pending(&pdata->ts_irq_work)) {
            cancel_work_sync(&pdata->ts_irq_work);
        }
#endif /* CONFIG_GENERIC_HARDIRQS */
    }
}

static int tpd_history_x, tpd_history_y;
int cts_plat_process_touch_msg(struct cts_platform_data *pdata,
            struct cts_device_touch_msg *msgs, int num)
{
    struct input_dev *input_dev = pdata->ts_input_dev;
    int i;
    int contact = 0;
#ifdef CONFIG_CTS_SLOTPROTOCOL
    static unsigned char finger_last[CFG_CTS_MAX_TOUCH_NUM] = {0};
    unsigned char finger_current[CFG_CTS_MAX_TOUCH_NUM] = {0};
#endif

    cts_dbg("Process touch %d msgs", num);
    if (num ==0 || num > CFG_CTS_MAX_TOUCH_NUM) {
        return 0;
    }
    for (i = 0; i < num; i++) {
        u16 x, y;

        x = le16_to_cpu(msgs[i].x);
        y = le16_to_cpu(msgs[i].y);

#ifdef CFG_CTS_SWAP_XY
        swap(x,y);
#endif /* CFG_CTS_SWAP_XY */
#ifdef CFG_CTS_WRAP_X
        x = wrap(TPD_RES_X,x);
#endif /* CFG_CTS_WRAP_X */
#ifdef CFG_CTS_WRAP_Y
        y = wrap(TPD_RES_Y,y);
#endif /* CFG_CTS_WRAP_Y */

        cts_dbg("  Process touch msg[%d]: id[%u] ev=%u x=%u y=%u p=%u",
            i, msgs[i].id, msgs[i].event, x, y, msgs[i].pressure);

#ifdef CONFIG_CTS_SLOTPROTOCOL
	if (msgs[i].event == CTS_DEVICE_TOUCH_EVENT_DOWN
		|| msgs[i].event == CTS_DEVICE_TOUCH_EVENT_MOVE
		|| msgs[i].event == CTS_DEVICE_TOUCH_EVENT_STAY) {
		if (msgs[i].id < CFG_CTS_MAX_TOUCH_NUM) {
			finger_current[msgs[i].id] = 1;
		}
        	input_mt_slot(input_dev, msgs[i].id);
	}
        switch (msgs[i].event) {
            case CTS_DEVICE_TOUCH_EVENT_DOWN:
                TPD_DEBUG_SET_TIME;
                TPD_EM_PRINT(x, y, x, y, msgs[i].id, 1);
                tpd_history_x = x;
                tpd_history_y = y;
#ifdef CONFIG_MTK_BOOT
                if (tpd_dts_data.use_tpd_button) {
                    if (FACTORY_BOOT == get_boot_mode() ||
                        RECOVERY_BOOT == get_boot_mode())
                        tpd_button(x, y, 1);
                }
#endif /* CONFIG_MTK_BOOT */
            case CTS_DEVICE_TOUCH_EVENT_MOVE:
            case CTS_DEVICE_TOUCH_EVENT_STAY:
                contact++;
                input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
                input_report_abs(input_dev, ABS_MT_POSITION_X, x);
                input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
                input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, msgs[i].pressure);
                input_report_abs(input_dev, ABS_MT_PRESSURE, msgs[i].pressure);
                break;

            case CTS_DEVICE_TOUCH_EVENT_UP:
                TPD_DEBUG_SET_TIME;
                TPD_EM_PRINT(tpd_history_x, tpd_history_y, tpd_history_x, tpd_history_y, msgs[i].id, 0);
                tpd_history_x = 0;
                tpd_history_y = 0;
#ifdef CONFIG_MTK_BOOT
                if (tpd_dts_data.use_tpd_button) {
                    if (FACTORY_BOOT == get_boot_mode() ||
                        RECOVERY_BOOT == get_boot_mode())
                        tpd_button(0, 0, 0);
                }
#endif /* CONFIG_MTK_BOOT */
                //input_report_key(input_dev, BTN_TOUCH, 0);
                //input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
                break;

            default:
                cts_warn("Process touch msg with unknwon event %u id %u",
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
  		        input_report_abs(input_dev, ABS_MT_PRESSURE, msgs[i].pressure);
		        input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, msgs[i].pressure);
                input_report_key(input_dev, BTN_TOUCH, 1);
                input_report_abs(input_dev, ABS_MT_POSITION_X, x);
                input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
                input_mt_sync(input_dev);
                break;

            case CTS_DEVICE_TOUCH_EVENT_UP:
                break;
            default:
                cts_warn("Process touch msg with unknwon event %u id %u",
                    msgs[i].event, msgs[i].id);
                break;
        }
#endif /* CONFIG_CTS_SLOTPROTOCOL */
    }
#ifdef CONFIG_CTS_SLOTPROTOCOL
	for (i = 0; i < CFG_CTS_MAX_TOUCH_NUM; i++) {
		if (finger_last[i] != 0 && finger_current[i] == 0) {
			input_mt_slot(input_dev, i);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
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
        mod_timer(&pdata->touch_event_timeout_timer, jiffies + msecs_to_jiffies(100));
    } else {
        del_timer(&pdata->touch_event_timeout_timer);
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
#ifdef CFG_CTS_FORCE_UP
    del_timer(&pdata->touch_event_timeout_timer);
#endif
    return 0;
}

#ifdef CONFIG_CTS_VIRTUALKEY
int cts_plat_init_vkey_device(struct cts_platform_data *pdata)
{
    pdata->vkey_state = 0;

    cts_info("Init Vkey");

    if (tpd_dts_data.use_tpd_button) {
        cts_info("Init vkey");

        pdata->vkey_state = 0;
        tpd_button_setting(tpd_dts_data.tpd_key_num, tpd_dts_data.tpd_key_local,
                           tpd_dts_data.tpd_key_dim_local);
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
    u8  event;
    int i;

    event = pdata->vkey_state ^ vkey_state;

    cts_dbg("Process vkey state=0x%02x, event=0x%02x", vkey_state, event);

    for (i = 0; i < pdata->vkey_num; i++) {
        if (event & BIT(i)) {
            tpd_button(x, y, vkey_state & BIT(i));

            /* MTK fobidon more than one key pressed in the same time */
            break;
        }
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
            tpd_button(x, y, 0);
        }
    }

    pdata->vkey_state = 0;

    return 0;
}
#endif /* CONFIG_CTS_VIRTUALKEY */

#ifdef CFG_CTS_GESTURE
int cts_plat_enable_irq_wake(struct cts_platform_data *pdata)
{
    cts_info("Enable IRQ wake");

    if (pdata->irq > 0) {
        if (!pdata->irq_wake_enabled) {
            pdata->irq_wake_enabled = true;
            return enable_irq_wake(pdata->irq);
        }

        cts_warn("Enable irq wake while already disabled");
        return -EINVAL;
    }

    cts_warn("Enable irq wake while irq invalid %d", pdata->irq);
    return -ENODEV;
}

int cts_plat_disable_irq_wake(struct cts_platform_data *pdata)
{
    cts_info("Disable IRQ wake");

    if (pdata->irq > 0) {
        if (pdata->irq_wake_enabled) {
            pdata->irq_wake_enabled = false;
            return disable_irq_wake(pdata->irq);
        }

        cts_warn("Disable irq wake while already disabled");
        return -EINVAL;
    }

    cts_warn("Disable irq wake while irq invalid %d", pdata->irq);
    return -ENODEV;
}

int cts_plat_init_gesture(struct cts_platform_data *pdata)
{
    int i;

    cts_info("Init gesture");

    // TODO: If system will issure enable/disable command, comment following line.
    //cts_enable_gesture_wakeup(pdata->cts_dev);

    for (i = 0; i < pdata->gesture_num; i ++) {
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
