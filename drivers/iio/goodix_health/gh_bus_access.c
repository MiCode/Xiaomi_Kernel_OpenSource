/* Goodix's health driver
 *
 * 2010 - 2021 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software FounSUPPORT_REE_SPIdation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include "gh_common.h"
u8 *g_tx_buf = NULL;
u8 *g_rx_buf = NULL;
#define TANSFER_MAX_LEN (1024*1024+10)

#if (GH_SUPPORT_BUS_SPI == GH_SUPPORT_BUS)
void gh_spi_setup_conf(struct gh_device *gh_dev, u32 speed)
{
	u32 max_speed_hz;

	if (speed >= 9)
        max_speed_hz = 25000000;
	else if (speed >= 8)
		max_speed_hz = 19200000;
	else if (speed >= 6)
		max_speed_hz = 9600000;
	else if (speed >= 4)
		max_speed_hz = 4800000;
	else
		max_speed_hz = 960000;

	gh_dev->spi->mode = SPI_MODE_0;
	gh_dev->spi->max_speed_hz = max_speed_hz;
	gh_dev->spi->bits_per_word = 8;

	if (spi_setup(gh_dev->spi))
		gh_debug(ERR_LOG, "%s, failed to setup spi conf\n", __func__);
}

static int gh_spi_transfer_raw(struct gh_device *gh_dev, u8 *tx_buf, u8 *rx_buf, u32 len)
{
	struct spi_message msg;
	struct spi_transfer xfer;

	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(struct spi_transfer));

	xfer.tx_buf = tx_buf;
	xfer.rx_buf = rx_buf;
	xfer.len = len;
	spi_message_add_tail(&xfer, &msg);
	spi_sync(gh_dev->spi, &msg);

	return 0;
}

int gh_ioctl_transfer_raw_cmd(struct gh_device *gh_dev, unsigned long arg,unsigned int bufsiz)
{
	struct gh_ioc_transfer_raw ioc_xraw;
	int retval = 0;

	do {
		uint32_t len;

		if (copy_from_user(&ioc_xraw, (struct gh_ioc_transfer_raw *)arg, sizeof(struct gh_ioc_transfer_raw))) {
			gh_debug(ERR_LOG, "%s: Failed to copy gh_ioc_transfer_raw from user to kernel\n", __func__);
			retval = -EFAULT;
			break;
		}

		if (ioc_xraw.read_buf == NULL || ioc_xraw.write_buf == NULL) {
			gh_debug(ERR_LOG, "%s: read buf and write buf can not equal to NULL simultaneously.\n", __func__);
			retval = -EINVAL;
			break;
		}

		pr_err("@@len: 0x%x\n",ioc_xraw.len);

		if ((ioc_xraw.len > TANSFER_MAX_LEN) || (ioc_xraw.len == 0)) {
			pr_err("%s: request transfer length larger than maximum buffer\n", __func__);
			retval = -EINVAL;
			break;
		}

		/* change speed and set transfer mode */
		gh_spi_setup_conf(gh_dev, ioc_xraw.high_time);

		len = ioc_xraw.len;

		if (copy_from_user(g_tx_buf, ioc_xraw.write_buf, ioc_xraw.len)) {
			gh_debug(ERR_LOG, "Failed to copy gh_ioc_transfer from user to kernel\n");
			retval = -EFAULT;
			break;
		}

		gh_spi_transfer_raw(gh_dev, g_tx_buf, g_rx_buf, len);

		if (copy_to_user(ioc_xraw.read_buf, g_rx_buf, ioc_xraw.len)) {
			gh_debug(ERR_LOG, "Failed to copy gh_ioc_transfer_raw from kernel to user\n");
			retval = -EFAULT;
		}
	} while(0);

	return retval;
 }

int gh_init_transfer_buffer()
{
	int retval = 0;
	int len = TANSFER_MAX_LEN;
	g_tx_buf = kzalloc(len, GFP_KERNEL);
	if (NULL == g_tx_buf) {
		gh_debug(ERR_LOG, "%s: failed to allocate raw tx buffer\n", __func__);
		retval = -EMSGSIZE;
	}

	g_rx_buf = kzalloc(len, GFP_KERNEL);
	if (NULL == g_rx_buf) {
		kfree(g_tx_buf);
		gh_debug(ERR_LOG, "%s: failed to allocate raw rx buffer\n", __func__);
		retval = -EMSGSIZE;
	}

	return retval;
}

int gh_free_transfer_buffer()
{
	kfree(g_tx_buf);
	kfree(g_rx_buf);
	return 0;
}

#elif (GH_SUPPORT_BUS_I2C == GH_SUPPORT_BUS)

int gh_ioctl_transfer_raw_cmd(struct gh_device *gh_dev, unsigned long arg, unsigned int bufsiz)
{
	struct gh_i2c_rdwr_ioctl_data rdwr_arg;
	struct i2c_msg *rdwr_pa;
	u8 __user **data_ptrs;
	int i, res;

	if (copy_from_user(&rdwr_arg,
			(struct gh_i2c_rdwr_ioctl_data __user *)arg,
			sizeof(rdwr_arg)))
		return -EFAULT;

	/* Put an arbitrary limit on the number of messages that can
	 * be sent at once */
	if (rdwr_arg.nmsgs > 2)
		return -EINVAL;

	rdwr_pa = memdup_user(rdwr_arg.msgs,
				rdwr_arg.nmsgs * sizeof(struct i2c_msg));
	if (IS_ERR(rdwr_pa))
		return PTR_ERR(rdwr_pa);

	data_ptrs = kmalloc(rdwr_arg.nmsgs * sizeof(u8 __user *), GFP_KERNEL);
	if (data_ptrs == NULL) {
		kfree(rdwr_pa);
		return -ENOMEM;
	}

	res = 0;
	for (i = 0; i < rdwr_arg.nmsgs; i++) {
		/* Limit the size of the message to a sane amount */
		if (rdwr_pa[i].len > 8192) {
			res = -EINVAL;
			break;
		}

		data_ptrs[i] = (u8 __user *)rdwr_pa[i].buf;
		rdwr_pa[i].buf = memdup_user(data_ptrs[i], rdwr_pa[i].len);
		if (IS_ERR(rdwr_pa[i].buf)) {
			res = PTR_ERR(rdwr_pa[i].buf);
			break;
		}
		rdwr_pa[i].addr = gh_dev->client->addr;
	}
	if (res < 0) {
		int j;
		for (j = 0; j < i; ++j)
			kfree(rdwr_pa[j].buf);
		kfree(data_ptrs);
		kfree(rdwr_pa);
		return res;
	}

	res = i2c_transfer(gh_dev->client->adapter, rdwr_pa, rdwr_arg.nmsgs);
	while (i-- > 0) {
		if (res >= 0 && (rdwr_pa[i].flags & I2C_M_RD)) {
			if (copy_to_user(data_ptrs[i], rdwr_pa[i].buf,
					rdwr_pa[i].len))
				res = -EFAULT;
		}
		kfree(rdwr_pa[i].buf);
	}
	kfree(data_ptrs);
	kfree(rdwr_pa);
	return res;	
}

#endif
