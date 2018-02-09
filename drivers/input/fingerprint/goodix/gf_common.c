#include "gf_common.h"
#include "gf_spi.h"

#if defined(USE_SPI_BUS)
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#elif defined(USE_PLATFORM_BUS)
#include <linux/platform_device.h>
#endif



/******************** Function Definitions *************************/

#ifdef SPI_ASYNC
static void gf_spi_complete(void *arg)
{
	complete(arg);
}
#endif

int gf_spi_read_bytes(struct gf_dev *gf_dev, unsigned short addr, unsigned short data_len,
		unsigned char *rx_buf)
{
#ifdef SPI_ASYNC
	DECLARE_COMPLETION_ONSTACK(write_done);
#endif
	struct spi_message msg;
	struct spi_transfer *xfer;
	int ret = 0;

	xfer = kzalloc(sizeof(*xfer)*2, GFP_KERNEL);
	if (xfer == NULL) {
		pr_err("%s, No memory for command.\n", __func__);
		return -ENOMEM;
	}

	/*send gf command to device.*/
	spi_message_init(&msg);
	rx_buf[0] = GF_W;
	rx_buf[1] = (unsigned char)((addr >> 8)&0xFF);
	rx_buf[2] = (unsigned char)(addr & 0xFF);
	xfer[0].tx_buf = rx_buf;
	xfer[0].len = 3;
	spi_message_add_tail(&xfer[0], &msg);

	/*if wanted to read data from gf.
	 *Should write Read command to device
	 *before read any data from device.
	 */
	spi_sync(gf_dev->spi, &msg);
	spi_message_init(&msg);
	memset(rx_buf, 0xff, data_len + 4);
	rx_buf[3] = GF_R;
	xfer[1].tx_buf = &rx_buf[3];
	xfer[1].rx_buf = &rx_buf[3];
	xfer[1].len = data_len + 1;

	spi_message_add_tail(&xfer[1], &msg);

#ifdef SPI_ASYNC
	msg.complete = gf_spi_complete;
	msg.context = &write_done;

	spin_lock_irq(&gf_dev->spi_lock);
	ret = spi_async(gf_dev->spi, &msg);
	spin_unlock_irq(&gf_dev->spi_lock);
	if (ret == 0) {
		wait_for_completion(&write_done);
		if (msg.status == 0)
			ret = msg.actual_length - 1;
	}
#else
	ret = spi_sync(gf_dev->spi, &msg);
	if (ret == 0) {
		ret = msg.actual_length - 1;
	}
#endif
	kfree(xfer);
	if (xfer != NULL)
		xfer = NULL;

	return 0;
}

int gf_spi_write_bytes(struct gf_dev *gf_dev, unsigned short addr, unsigned short data_len,
		unsigned char *tx_buf)
{
#ifdef SPI_ASYNC
	DECLARE_COMPLETION_ONSTACK(read_done);
#endif
	struct spi_message msg;
	struct spi_transfer *xfer;
	int ret = 0;

	xfer = kzalloc(sizeof(*xfer), GFP_KERNEL);
	if (xfer == NULL) {
		pr_err("%s, No memory for command.\n", __func__);
		return -ENOMEM;
	}

	/*send gf command to device.*/
	spi_message_init(&msg);
	tx_buf[0] = GF_W;
	tx_buf[1] = (unsigned char)((addr >> 8)&0xFF);
	tx_buf[2] = (unsigned char)(addr & 0xFF);
	xfer[0].tx_buf = tx_buf;
	xfer[0].len = data_len + 3;
	spi_message_add_tail(xfer, &msg);
#ifdef SPI_ASYNC
	msg.complete = gf_spi_complete;
	msg.context = &read_done;

	spin_lock_irq(&gf_dev->spi_lock);
	ret = spi_async(gf_dev->spi, &msg);
	spin_unlock_irq(&gf_dev->spi_lock);
	if (ret == 0) {
		wait_for_completion(&read_done);
		if (msg.status == 0)
			ret = msg.actual_length - GF_WDATA_OFFSET;
	}
#else
	ret = spi_sync(gf_dev->spi, &msg);
	if (ret == 0) {
		ret = msg.actual_length - GF_WDATA_OFFSET;
	}
#endif
	kfree(xfer);
	if (xfer != NULL)
		xfer = NULL;



	return 0;
}

int gf_spi_read_word(struct gf_dev *gf_dev, unsigned short addr, unsigned short *value)
{
	int status = 0;
	u8 *buf = NULL;
	mutex_lock(&gf_dev->buf_lock);
	status = gf_spi_read_bytes(gf_dev, addr, 2, gf_dev->gBuffer);
	buf = gf_dev->gBuffer + GF_RDATA_OFFSET;
	*value = ((unsigned short)(buf[0]<<8)) | buf[1];
	mutex_unlock(&gf_dev->buf_lock);
	return status;
}

int gf_spi_write_word(struct gf_dev *gf_dev, unsigned short addr, unsigned short value)
{
	int status = 0;
	mutex_lock(&gf_dev->buf_lock);
	gf_dev->gBuffer[GF_WDATA_OFFSET] = 0x00;
	gf_dev->gBuffer[GF_WDATA_OFFSET+1] = 0x01;
	gf_dev->gBuffer[GF_WDATA_OFFSET+2] = (u8)(value>>8);
	gf_dev->gBuffer[GF_WDATA_OFFSET+3] = (u8)(value & 0x00ff);
	status = gf_spi_write_bytes(gf_dev, addr, 4, gf_dev->gBuffer);
	mutex_unlock(&gf_dev->buf_lock);

	return status;
}

void endian_exchange(int len, unsigned char *buf)
{
	int i;
	u8 buf_tmp;
	for (i = 0; i < len/2; i++) {
		buf_tmp = buf[2*i+1];
		buf[2*i+1] = buf[2*i] ;
		buf[2*i] = buf_tmp;
	}
}

int gf_spi_read_data(struct gf_dev *gf_dev, unsigned short addr, int len, unsigned char *value)
{
	int status;

	mutex_lock(&gf_dev->buf_lock);
	status = gf_spi_read_bytes(gf_dev, addr, len, gf_dev->gBuffer);
	memcpy(value, gf_dev->gBuffer+GF_RDATA_OFFSET, len);
	mutex_unlock(&gf_dev->buf_lock);

	return status;
}

int gf_spi_read_data_bigendian(struct gf_dev *gf_dev, unsigned short addr, int len, unsigned char *value)
{
	int status;

	mutex_lock(&gf_dev->buf_lock);
	status = gf_spi_read_bytes(gf_dev, addr, len, gf_dev->gBuffer);
	memcpy(value, gf_dev->gBuffer+GF_RDATA_OFFSET, len);
	mutex_unlock(&gf_dev->buf_lock);

	endian_exchange(len, value);
	return status;
}

int gf_spi_write_data(struct gf_dev *gf_dev, unsigned short addr, int len, unsigned char *value)
{
	int status = 0;
	unsigned short addr_len = 0;
	unsigned char *buf = NULL;

	if (len > 1024 * 10) {
		pr_err("%s length is large.\n", __func__);
		return -EPERM;
	}

	addr_len = len / 2;

	buf = kzalloc(len + 2, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("%s, No memory for gBuffer.\n", __func__);
		return -ENOMEM;
	}

	buf[0] = (unsigned char) ((addr_len & 0xFF00) >> 8);
	buf[1] = (unsigned char) (addr_len & 0x00FF);
	memcpy(buf+2, value, len);
	endian_exchange(len, buf+2);

	mutex_lock(&gf_dev->buf_lock);
	memcpy(gf_dev->gBuffer+GF_WDATA_OFFSET, buf, len+2);
	kfree(buf);

	status = gf_spi_write_bytes(gf_dev, addr, len+2, gf_dev->gBuffer);
	mutex_unlock(&gf_dev->buf_lock);

	return status;
}

/***
 * Yfpan Change gf_spi_send_cmd()interface for Milan HV Series.
 * (Now add for MilanE HV)
 ***/
int gf_spi_send_cmd(struct gf_dev *gf_dev, unsigned char *cmd, int len)
{
	struct spi_message msg;
	struct spi_transfer *xfer;
	int ret;

	/***
	 * Add for HV Enable and Voltage value
	 **/
	unsigned short cmd_15v_enable = 0x08D0;
	unsigned short cmd_14v_enable = 0x18D0;
	unsigned short cmd_13v_enable = 0x28D0;
	unsigned short cmd_12v_enable = 0x38D0;
	unsigned short cmd_11v_enable = 0x48D0;
	unsigned short cmd_10v_enable = 0x58D0;
	unsigned short cmd_9v_enable  = 0x68D0;
	unsigned short cmd_8v_enable  = 0x78D0;
	unsigned short cmd_7v_enable  = 0x88D0;
	unsigned short cmd_hv_disable = 0x00D0;

	spi_message_init(&msg);
	xfer = kzalloc(sizeof(*xfer), GFP_KERNEL);

	if (xfer == NULL) {
		pr_err("%s, No memory for command.\n", __func__);
		return -ENOMEM;
	}

	switch (*cmd) {
	case 0x08:
		pr_info("%s Enable HV and set voltage to 15v.", __func__);
		xfer->tx_buf = &cmd_15v_enable;
		xfer->len    = 2;
		break;
	case 0x18:
		pr_info("%s Enable HV and set voltage to 14v.", __func__);
		xfer->tx_buf = &cmd_14v_enable;
		xfer->len    = 2;
		break;
	case 0x28:
		pr_info("%s Enable HV and set voltage to 13v.", __func__);
		xfer->tx_buf = &cmd_13v_enable;
		xfer->len    = 2;
		break;
	case 0x38:
		pr_info("%s Enable HV and set voltage to 12v.", __func__);
		xfer->tx_buf = &cmd_12v_enable;
		xfer->len    = 2;
		break;
	case 0x48:
		pr_info("%s Enable HV and set voltage to 11v.", __func__);
		xfer->tx_buf = &cmd_11v_enable;
		xfer->len    = 2;
		break;
	case 0x58:
		pr_info("%s Enable HV and set voltage to 10v.", __func__);
		xfer->tx_buf = &cmd_10v_enable;
		xfer->len    = 2;
		break;
	case 0x68:
		pr_info("%s Enable HV and set voltage to 9v.", __func__);
		xfer->tx_buf = &cmd_9v_enable;
		xfer->len    = 2;
		break;
	case 0x78:
		pr_info("%s Enable HV and set voltage to 8v.", __func__);
		xfer->tx_buf = &cmd_8v_enable;
		xfer->len    = 2;
		break;
	case 0x88:
	case 0x98:
	case 0xa8:
	case 0xb8:
	case 0xd8:
		pr_info("%s Enable HV and set voltage to 7v.", __func__);
		xfer->tx_buf = &cmd_7v_enable;
		xfer->len    = 2;
		break;
	case 0x00:
		pr_info("%s Disable HV.", __func__);
		xfer->tx_buf = &cmd_hv_disable;
		xfer->len    = 2;
		break;
	default:
		xfer->tx_buf = cmd;
		xfer->len = len;
		break;
	}

	spi_message_add_tail(xfer, &msg);
	ret = spi_sync(gf_dev->spi, &msg);

	kfree(xfer);
	if (xfer != NULL)
		xfer = NULL;

	return ret;
}
