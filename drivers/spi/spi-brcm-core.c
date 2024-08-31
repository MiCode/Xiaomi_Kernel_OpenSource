//Create by wangyu36@xiaomi.com
#if IS_ENABLED(CONFIG_BRCM_XGBE)
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/compat.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <linux/uaccess.h>
#include <linux/delay.h>
#include "spi_common.h"

static void acd_init_port0(void);
static void acd_init_port3(void);

/*------------------------------ sysfs operation start ---------------------------*/
static int aps_spi_read(unsigned char *wr_buf, int wr_len, unsigned char *rd_buf, int rd_len)
{
	struct spi_ioc_transfer xfer[2];
	int status = 0;
	memset(xfer, 0, sizeof (xfer));

	xfer[0].tx_buf = (unsigned long)wr_buf;
	xfer[0].len = wr_len;
	xfer[0].rx_buf = (unsigned long long)NULL;

	xfer[1].tx_buf = (unsigned long long)NULL;
	xfer[1].len = rd_len;
	xfer[1].rx_buf = (unsigned long)rd_buf;

	status = spidev_message_kern(xfer, 2);

	return status;
}

static int spi_read16(unsigned int addr, unsigned short *rd_val)
{
	unsigned char buf[MAX_BUF_SZ];
	unsigned char val[2];
	int len = 0;
	int i, status;

	//opcode
	buf[len++] = SPI_OPCODE_PHYADDR(g_spi_id) | SPI_OPCODE_RD | SPI_OPCODE_NO_INC | SPI_OPCODE_RD_WAIT_2 | SPI_OPCODE_TX_SZ_16;
	buf[len++] = (addr >> 24) & 0xff;
	buf[len++] = (addr >> 16) & 0xff;
	buf[len++] = (addr >> 8) & 0xff;
	buf[len++] = (addr & 0xff);
	/* wait states as per opcode */
	for (i =0; i < ((buf[0] & SPI_OPCODE_RD_WAIT_MASK) >> SPI_OPCODE_RD_WAIT_SHIFT) * 2; i++)
		buf[len++] = 0x0;

	status = aps_spi_read(&buf[0], len, (unsigned char*)val, sizeof(unsigned short));

	*rd_val = ((unsigned int)val[0] << 8UL) | val[1];
	return status > 0 ? 0 : status;
}

static int aps_spi_write(unsigned char *buf, int len)
{
	struct spi_ioc_transfer xfer[1];
	int status = 0;

	memset(xfer, 0, sizeof (xfer));

	xfer[0].tx_buf = (unsigned long long)buf;
	xfer[0].len = len;
	xfer[0].rx_buf = (unsigned long long)NULL;

	status = spidev_message_kern(xfer, 1);

	return status;
}

static int spi_write16(unsigned int addr, unsigned short data)
{
	unsigned char txbuf[MAX_BUF_SZ];
	int len = 0;
	int status;

	//opcode
	txbuf[len++] = SPI_OPCODE_PHYADDR(g_spi_id) | SPI_OPCODE_WR | SPI_OPCODE_NO_INC | SPI_OPCODE_RD_WAIT_0 | SPI_OPCODE_TX_SZ_16;
	txbuf[len++] = (addr >> 24) & 0xff;
	txbuf[len++] = (addr >> 16) & 0xff;
	txbuf[len++] = (addr >> 8) & 0xff;
	txbuf[len++] = (addr & 0xff);
	txbuf[len++] = (data >> 8) & 0xff;
	txbuf[len++] = (data & 0xff);

	status = aps_spi_write(&txbuf[0], len);
	return status > 0 ? 0 : status;
}

static int spi_write8(unsigned int addr, unsigned char data)
{
    unsigned char buf[MAX_BUF_SZ];
    unsigned int len = 0;
    //opcode
    buf[len++] = SPI_OPCODE_PHYADDR(g_spi_id) | SPI_OPCODE_WR | SPI_OPCODE_NO_INC | SPI_OPCODE_RD_WAIT_0 | SPI_OPCODE_TX_SZ_8;
    buf[len++] = (addr >> 24) & 0xff;
    buf[len++] = (addr >> 16) & 0xff;
    buf[len++] = (addr >> 8) & 0xff;
    buf[len++] = (addr & 0xff);
    buf[len++] = (data & 0xff);
    aps_spi_write(&buf[0], len);
    return 0;
}

static int spi_write32(unsigned int addr, unsigned int data)
{
	unsigned char buf[MAX_BUF_SZ];
	unsigned int len = 0;
	//opcode
	buf[len++] = SPI_OPCODE_PHYADDR(g_spi_id) | SPI_OPCODE_WR | SPI_OPCODE_NO_INC | SPI_OPCODE_RD_WAIT_0 | SPI_OPCODE_TX_SZ_32;
	buf[len++] = (addr >> 24) & 0xff;
	buf[len++] = (addr >> 16) & 0xff;
	buf[len++] = (addr >> 8) & 0xff;
	buf[len++] = (addr & 0xff);
	buf[len++] = (data >> 24) & 0xff;
	buf[len++] = (data >> 16) & 0xff;
	buf[len++] = (data >> 8) & 0xff;
	buf[len++] = (data & 0xff);
	aps_spi_write(&buf[0], len);
	return 0;
}

static int spi_read32(unsigned int addr, unsigned int *rd_val)
{
	unsigned char txbuf[MAX_BUF_SZ];
	unsigned char val[4];
	int len = 0;
	int i, ret = -1;

	//opcode
	txbuf[len++] = SPI_OPCODE_PHYADDR(g_spi_id) | SPI_OPCODE_RD | SPI_OPCODE_NO_INC | SPI_OPCODE_RD_WAIT_2 | SPI_OPCODE_TX_SZ_32;
	txbuf[len++] = (addr >> 24) & 0xff;
	txbuf[len++] = (addr >> 16) & 0xff;
	txbuf[len++] = (addr >> 8) & 0xff;
	txbuf[len++] = (addr & 0xff);
	/* wait states as per opcode */
	for (i = 0; i < ((txbuf[0] & SPI_OPCODE_RD_WAIT_MASK) >> SPI_OPCODE_RD_WAIT_SHIFT) * 2; i++)
	txbuf[len++] = 0x0;
	ret = aps_spi_read(&txbuf[0], len, (unsigned char*)val, sizeof(unsigned int));
	if (ret) {
	printk("aps_spi_read failed ret=%d\n", ret);
	}
	*rd_val = ((unsigned int)val[0] << 24UL)
			| ((unsigned int)val[1] << 16UL)
			| ((unsigned int)val[2] << 8UL)
			| val[3];
	return ret;
}

static int spi_write64(unsigned int addr, unsigned long data)
{
	unsigned char buf[MAX_BUF_SZ];
	unsigned int len = 0;
	//opcode
	buf[len++] = SPI_OPCODE_PHYADDR(g_spi_id) | SPI_OPCODE_WR | SPI_OPCODE_NO_INC | SPI_OPCODE_RD_WAIT_0 | SPI_OPCODE_TX_SZ_64;
	buf[len++] = (addr >> 24) & 0xff;
	buf[len++] = (addr >> 16) & 0xff;
	buf[len++] = (addr >> 8) & 0xff;
	buf[len++] = (addr & 0xff);
	buf[len++] = (data >> 56) & 0xff;
	buf[len++] = (data >> 48) & 0xff;
	buf[len++] = (data >> 40) & 0xff;
	buf[len++] = (data >> 32) & 0xff;
	buf[len++] = (data >> 24) & 0xff;
	buf[len++] = (data >> 16) & 0xff;
	buf[len++] = (data >> 8) & 0xff;
	buf[len++] = (data & 0xff);
	aps_spi_write(&buf[0], len);
	return 0;
}

static void acd_init_port0(void)
{
	unsigned int addr;
	unsigned short data;

	spidev_open_kern();

	addr = 0x49030000 + 0x1E04;
	data = 0x0001;
	spi_write16(addr, data);

	addr = 0x49030000 + 0x2E0E;
	data = 0x0202;
	spi_write16(addr, data);     // ACD_EXPC7

	addr = 0x49030000 + 0x2E10;
	data = 0x7F50;
	spi_write16(addr, data);     // ACD_EXPC8

	addr = 0x49030000 + 0x2E12;
	data = 0x2C22;
	spi_write16(addr, data);     // ACD_EXPC9

	addr = 0x49030000 + 0x2E14;
	data = 0x5252;
	spi_write16(addr, data);     // ACD_EXPCA

	addr = 0x49030000 + 0x2E16;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPCB

	addr = 0x49030000 + 0x2E18;
	data = 0x0014;
	spi_write16(addr, data);     // ACD_EXPCC

	addr = 0x49030000 + 0x2E1C;
	data = 0x1CA3;
	spi_write16(addr, data);     // ACD_EXPCE

	addr = 0x49030000 + 0x2E1E;
	data = 0x0206;
	spi_write16(addr, data);     // ACD_EXPCF

	addr = 0x49030000 + 0x2E20;
	data = 0x0010;
	spi_write16(addr, data);     // ACD_EXPE0

	addr = 0x49030000 + 0x2E22;
	data = 0x0D0D;
	spi_write16(addr, data);     // ACD_EXPE1

	addr = 0x49030000 + 0x2E24;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE2

	addr = 0x49030000 + 0x2E26;
	data = 0x7700;
	spi_write16(addr, data);     // ACD_EXPE3

	addr = 0x49030000 + 0x2E28;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE4

	addr = 0x49030000 + 0x2E2E;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE7

	addr = 0x49030000 + 0x2E3E;
	data = 0x409F;
	spi_write16(addr, data);     // ACD_EXPEF

	addr = 0x49030000 + 0x2E1A;
	data = 0x1129;
	spi_write16(addr, data);     // ACD_EXPCD

	addr = 0x49030000 + 0x2E1A;
	data = 0x0129;
	spi_write16(addr, data);     // ACD_EXPCD

	addr = 0x49030000 + 0x2E20;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE0

	addr = 0x49030000 + 0x2E22;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE1

	addr = 0x49030000 + 0x2E24;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE2

	addr = 0x49030000 + 0x2E26;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE3

	addr = 0x49030000 + 0x2E28;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE4

	addr = 0x49030000 + 0x2E2E;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE7

	addr = 0x49030000 + 0x2E3E;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPEF

	addr = 0x49030000 + 0x2E20;
	data = 0x3619;
	spi_write16(addr, data);     // ACD_EXPE0

	addr = 0x49030000 + 0x2E22;
	data = 0x343A;
	spi_write16(addr, data);     // ACD_EXPE1

	addr = 0x49030000 + 0x2E24;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE2

	addr = 0x49030000 + 0x2E26;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE3

	addr = 0x49030000 + 0x2E28;
	data = 0x8000;
	spi_write16(addr, data);     // ACD_EXPE4

	addr = 0x49030000 + 0x2E2A;
	data = 0x000E;
	spi_write16(addr, data);     // ACD_EXPE5

	addr = 0x49030000 + 0x2E2E;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE7

	addr = 0x49030000 + 0x2E32;
	data = 0x0400;
	spi_write16(addr, data);     // ACD_EXPE9

	addr = 0x49030000 + 0x2E3A;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPED

	addr = 0x49030000 + 0x2E3E;
	data = 0xA2BF;
	spi_write16(addr, data);     // ACD_EXPEF

	addr = 0x49030000 + 0x2E1A;
	data = 0x1129;
	spi_write16(addr, data);     // ACD_EXPCD

	addr = 0x49030000 + 0x2E1A;
	data = 0x0129;
	spi_write16(addr, data);     // ACD_EXPCD

	addr = 0x49030000 + 0x2E20;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE0

	addr = 0x49030000 + 0x2E22;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE1

	addr = 0x49030000 + 0x2E24;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE2

	addr = 0x49030000 + 0x2E26;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE3

	addr = 0x49030000 + 0x2E28;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE4

	addr = 0x49030000 + 0x2E2A;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE5

	addr = 0x49030000 + 0x2E2E;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE7

	addr = 0x49030000 + 0x2E30;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE8

	addr = 0x49030000 + 0x2E32;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPE9

	addr = 0x49030000 + 0x2E3A;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPED

	addr = 0x49030000 + 0x2E3E;
	data = 0x0000;
	spi_write16(addr, data);     // ACD_EXPEF

	spidev_release_kern();
}

static void acd_init_port3(void)
{
	unsigned int addr;
	unsigned short data;

	spidev_open_kern();

	addr = 0x49CF254E;
	data = 0xA01A;
	spi_write16(addr, data);

	addr = 0x49CF2550;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2552;
	data = 0x00EF;
	spi_write16(addr, data);

	addr = 0x49CF2558;
	data = 0x0200;
	spi_write16(addr, data);

	addr = 0x49CF255C;
	data = 0x4000;
	spi_write16(addr, data);

	addr = 0x49CF255E;
	data = 0x3000;
	spi_write16(addr, data);

	addr = 0x49CF2560;
	data = 0x0015;
	spi_write16(addr, data);

	addr = 0x49CF2562;
	data = 0x0D0D;
	spi_write16(addr, data);

	addr = 0x49CF2564;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2566;
	data = 0x7700;
	spi_write16(addr, data);

	addr = 0x49CF2568;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF256E;
	data = 0x00A0;
	spi_write16(addr, data);

	addr = 0x49CF257E;
	data = 0x409F;
	spi_write16(addr, data);

	addr = 0x49CF255A;
	data = 0x1000;
	spi_write16(addr, data);

	addr = 0x49CF255A;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2560;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2562;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2564;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2566;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2568;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF256E;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF257E;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2560;
	data = 0x3600;
	spi_write16(addr, data);

	addr = 0x49CF2562;
	data = 0x343A;
	spi_write16(addr, data);

	addr = 0x49CF2564;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2566;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2568;
	data = 0x8000;
	spi_write16(addr, data);

	addr = 0x49CF256A;
	data = 0x000E;
	spi_write16(addr, data);

	addr = 0x49CF256E;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2572;
	data = 0x0400;
	spi_write16(addr, data);

	addr = 0x49CF257A;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF257E;
	data = 0xA3BF;
	spi_write16(addr, data);

	addr = 0x49CF255A;
	data = 0x1000;
	spi_write16(addr, data);

	addr = 0x49CF255A;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2560;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2562;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2564;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2566;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2568;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF256A;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF256E;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2570;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2572;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF257A;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF257E;
	data = 0x0000;
	spi_write16(addr, data);

	addr = 0x49CF2614;
	data = 0x2000;
	spi_write16(addr, data);

	addr = 0x49CF2600;
	data = 0x8001;
	spi_write16(addr, data);

	addr = 0x49CF2602;
	data = 0x9428;
	spi_write16(addr, data);

	spidev_release_kern();
}

static ssize_t phy_bcm89272_port0_dut_status_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data, data;
	unsigned int read_addr = 0x49032E02, addr = 0x49032E00;
	int status, link_status = 0;

	acd_init_port0();
	spidev_open_kern();

	// write 0x49032E00
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 10);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}
	data = 0;
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 15);
	SET_BIT_ENABLE(data, 9);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	// read 0x49032E02
	msleep(1000);
	status = spi_read16(read_addr, (&read_data));
	if (status) {
		goto err_done;
	}

	// reset switch
	data = 0xFFFF;
	status = spi_write16(0x4A820024, data);
	link_status = (read_data >> 12) & 0x0000000F;

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "0x%x\n", link_status);
}
static DRIVER_ATTR_RO(phy_bcm89272_port0_dut_status);

static ssize_t phy_bcm89272_port3_dut_status_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data, data;
	unsigned int read_addr = 0x49CF2542, addr = 0x49CF2540;
	int status, link_status = 0;

	acd_init_port3();
	spidev_open_kern();

	// write 0x49CF2540
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 6);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}
	data = 0;
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 13);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 13);
	SET_BIT_ENABLE(data, 15);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 10);
	SET_BIT_ENABLE(data, 13);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 10);
	SET_BIT_ENABLE(data, 13);
	SET_BIT_ENABLE(data, 15);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_ENABLE(data, 10);
	SET_BIT_ENABLE(data, 15);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	// read 0x49CF2542
	msleep(1000);
	status = spi_read16(read_addr, (&read_data));
	if (status) {
		goto err_done;
	}

	// reset switch
	data = 0xFFFF;
	status = spi_write16(0x4A820024, data);
	link_status = (read_data >> 12) & 0xF;

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "0x%x\n", link_status);
}
static DRIVER_ATTR_RO(phy_bcm89272_port3_dut_status);

static ssize_t phy_bcm89272_port0_link_status_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x4B000100;
	int status, link_status = 2;

	spidev_open_kern();

	// read 0x4B000100
	status = spi_read16(read_addr, (&read_data));
	if (status) {
		goto err_done;
	}
	link_status = GET_BIT(read_data, 0);

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "%d\n", link_status);
}
static DRIVER_ATTR_RO(phy_bcm89272_port0_link_status);

static ssize_t phy_bcm89272_port0_sqi_status_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data, data;
	unsigned int read_addr = 0x490300A4, addr = 0x49032016;
	unsigned int sqi_status = 0;
	int status = 0;

	spidev_open_kern();

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	SET_BIT_DISABLE(data, 13);
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto err_done;
	}
	sqi_status = (read_data >> 1) & 0x7;

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "%d\n", sqi_status);
}
static DRIVER_ATTR_RO(phy_bcm89272_port0_sqi_status);

static ssize_t phy_bcm89272_port0_work_mode_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x49021068, mode = 2;
	int status;

	spidev_open_kern();

	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto err_done;
	}

	mode = GET_BIT(read_data, 14);

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "%d\n", mode);
}

static ssize_t phy_bcm89272_port0_work_mode_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data;
	unsigned int addr = 0x49021068;
	int status;

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}

	if (buf[0] == '1') {
		SET_BIT_ENABLE(data, 14);
	} else if (buf[0] == '0') {
		SET_BIT_DISABLE(data, 14);
	}
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}
static DRIVER_ATTR_RW(phy_bcm89272_port0_work_mode);

static ssize_t phy_bcm89272_port3_link_status_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x4B000100;
	int status, link_status = 2;

	spidev_open_kern();

	// read 0x4B000100
	status = spi_read16(read_addr, (&read_data));
	if (status) {
		goto err_done;
	}
	link_status = GET_BIT(read_data, 3);

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "%d\n", link_status);
}
static DRIVER_ATTR_RO(phy_bcm89272_port3_link_status);

static ssize_t phy_bcm89272_port3_sqi_status_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data, data;
	unsigned int read_addr = 0x49CF22E8, addr = 0x49CF2050;
	unsigned int sqi_status = 0;
	int status = 0;

	spidev_open_kern();

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}
	data = 0x0C30; // enable DSP clock
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto err_done;
	}
	sqi_status = (read_data >> 1) & 0x7;

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "%d\n", sqi_status);
}
static DRIVER_ATTR_RO(phy_bcm89272_port3_sqi_status);

static ssize_t phy_bcm89272_port3_work_mode_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x49C21068, mode = 2;
	int status;

	spidev_open_kern();

	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto err_done;
	}

	mode = GET_BIT(read_data, 14);

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "%d\n", mode);
}

static ssize_t phy_bcm89272_port3_work_mode_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data;
	unsigned int addr = 0x49C21068;
	int status;

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}

	if (buf[0] == '1') {
		SET_BIT_ENABLE(data, 14);
	} else if (buf[0] == '0') {
		SET_BIT_DISABLE(data, 14);
	}
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}
static DRIVER_ATTR_RW(phy_bcm89272_port3_work_mode);\

static ssize_t phy_bcm89272_PN_polarity_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x49021202, mode = 2;
	int status;

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto err_done;
	}

	mode = GET_BIT(read_data, 2);

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "%d\n", mode);
}
static DRIVER_ATTR_RO(phy_bcm89272_PN_polarity);

static void test_tvco_for_1G(struct device_driver *drv)
{
	unsigned short data;
	unsigned int addr;
	int status;

	spidev_open_kern();

	// WDT_WdogControl: Disable Watchdog first
	addr = 0x40145008;
	data = 0;
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	// AFE_DIG_PLL_TEST
	addr = 0x49038060;
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}

	SET_BIT_ENABLE(data, 0);
	SET_BIT_ENABLE(data, 1);

	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

	// PMA_PMD_IEEE_CONTROL_REG1
	addr = 0x49020000;
	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}

	SET_BIT_ENABLE(data, 1);
	SET_BIT_ENABLE(data, 6);

	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return;
}

static void test_tvco(struct device_driver *drv, int port)
{
	unsigned short data;
	unsigned int addr = 0x4A4F2022; // BRPHY0_GPHY_CORE_SHD1C_01_ Px
	int status;

	spidev_open_kern();

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}

	switch (port) {
	case 1:
		data = 0x415;
		break;
	case 2:
		data = 0x425;
		break;
	case 3:
		data = 0x435;
		break;
	case 4:
		data = 0x445;
		break;
	case 5:
		data = 0x405;
		break;

	default:
		goto err_done;
	}

	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return;
}

static void test_for_1G(struct device_driver *drv, int test_mode)
{
	// 1G port only
	unsigned short data;
	int status;

	if (test_mode == TestMode_TVCO) {
		return test_tvco_for_1G(drv);
	}

	spidev_open_kern();

	// DSP_TOP_PHSHFT_CONTROL
	data = 0x40;
	status = spi_write16(0x49030A14, data);
	if (status) {
		goto err_done;
	}

	// AUTONEG_IEEE_AUTONEG_BASET1_AN_CONTROL
	status = spi_write16(0x490E0400, 0);
	if (status) {
		goto err_done;
	}

	// LINK_SYNC_CONTROL_A
	data = 0x3;
	status = spi_write16(0x49031E04, data);
	if (status) {
		goto err_done;
	}

	// PMA_PMD_IEEE_BASET1_PMA_PMD_CONTROL
	if (test_mode == TestMode_IB) {
		data = 0x8001;
	} else {
		data = 0xc001;
	}
	status = spi_write16(0x49021068, data);
	if (status) {
		goto err_done;
	}

	if (test_mode != TestMode_IB) {
		// PMA_PMD_IEEE_BASE1000T1_TEST_MODE_CONTROL
		status = spi_read16(0x49021208, &data);
		if (status) {
			goto err_done;
		}

		switch(test_mode) {
		case TestMode_2:
			data = 0x4000;
			break;
		case TestMode_4:
			data = 0x8000;
			break;
		case TestMode_5:
			data = 0xA000;
			break;
		case TestMode_6:
			data = 0xC000;
			break;
		default:
			goto err_done;
		}

		status = spi_write16(0x49021208, data);
		if (status) {
			goto err_done;
		}
	}

err_done:
	spidev_release_kern();
	return;
}

static void set_test_mode_for_port(struct device_driver *drv, int port_num, const char* test_mode)
{
	unsigned short data;
	unsigned int addr1, addr2;
	int test_mode_num = 0, status;

	if (test_mode == NULL) {
		return;
	}
	test_mode_num = test_mode[0] - '0';
	if (test_mode_num <= 0 || test_mode_num > 6) {
		if (strncmp(test_mode, "IB", 2) == 0) {
			test_mode_num = TestMode_IB;
		} else if (strncmp(test_mode, "TVCO", 4) == 0) {
			test_mode_num = TestMode_TVCO;
		}
	}

	if (test_mode_num <= 0 || test_mode_num > TestMode_TVCO) {
		return;
	}

	if (port_num > 0 && test_mode_num == TestMode_TVCO) {
		return test_tvco(drv, port_num);
	}

	switch(port_num) {
	case 0:
		return test_for_1G(drv, test_mode_num);
	case 1:
		addr1 = 0x494E0400;
		addr2 = 0x4942106C;
		break;
	case 2:
		addr1 = 0x498E0400;
		addr2 = 0x4982106C;
		break;
	case 3:
		addr1 = 0x49CE0400;
		addr2 = 0x49C2106C;
		break;
	case 4:
		addr1 = 0x4A0E0400;
		addr2 = 0x4A02106C;
		break;
	case 5:
		addr1 = 0x4A4E0400;
		addr2 = 0x4A42106C;
		break;
	default:
		return;
	}

	spidev_open_kern();

	status = spi_write16(addr1, 0);
	if (status) {
		goto err_done;
	}

	switch(test_mode_num) {
	case 1:
		data = 0x2000;
		break;
	case 2:
		data = 0x4000;
		break;
	case 4:
		data = 0x8000;
		break;
	case 5:
		data = 0xA000;
		break;
	default:
		goto err_done;
	}

	status = spi_write16(addr2, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return;
}

static ssize_t phy_bcm89272_port0_set_test_mode_store(struct device_driver *drv, const char *buf, size_t count)
{
	set_test_mode_for_port(drv, 0, buf);
	return count;
}
static DRIVER_ATTR_WO(phy_bcm89272_port0_set_test_mode);

static ssize_t phy_bcm89272_port3_set_test_mode_store(struct device_driver *drv, const char *buf, size_t count)
{
	set_test_mode_for_port(drv, 3, buf);
	return count;
}
static DRIVER_ATTR_WO(phy_bcm89272_port3_set_test_mode);

static ssize_t phy_bcm89272_port0_get_normal_test_mode_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x49021208, test_mode = 0;
	int status;

	spidev_open_kern();

	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto err_done;
	}

	test_mode = read_data >> 13;
	if (test_mode) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "%d\n", test_mode);
}
static DRIVER_ATTR_RO(phy_bcm89272_port0_get_normal_test_mode);

static ssize_t phy_bcm89272_port3_get_normal_test_mode_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x49C2106C, test_mode = 0;
	int status;

	spidev_open_kern();

	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto err_done;
	}

	test_mode = read_data >> 13;
	if (test_mode) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "%d\n", test_mode);
}
static DRIVER_ATTR_RO(phy_bcm89272_port3_get_normal_test_mode);

static ssize_t phy_bcm89272_poer0_get_tvco_test_mode_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x49038060, is_tvco = 0;
	int status;

	spidev_open_kern();

	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto err_done;
	}

	if ((read_data & 0x3) == 0x3) {
		is_tvco = 1;
	}

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "%d\n", is_tvco);
}
static DRIVER_ATTR_RO(phy_bcm89272_poer0_get_tvco_test_mode);

static ssize_t phy_bcm89272_port3_get_tvco_test_mode_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x4A4F2022, is_tvco = 0;
	int status;

	spidev_open_kern();

	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto err_done;
	}

	if ((read_data & 0x35) == 0x35) {
		is_tvco = 1;
	}

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "%d\n", is_tvco);
}
static DRIVER_ATTR_RO(phy_bcm89272_port3_get_tvco_test_mode);

static ssize_t phy_bcm89272_port0_get_ib_test_mode_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr;
	unsigned int is_ib = 0;
	int status;

	spidev_open_kern();

	read_addr = 0x49030A14;
	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto done;
	}
	if (GET_BIT(read_data, 6) != 1) {
		goto done;
	}

	read_addr = 0x490E0400;
	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto done;
	}
	if (GET_BIT(read_data, 12) != 0) {
		goto done;
	}

	read_addr = 0x49031E04;
	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto done;
	}
	if (GET_BIT(read_data, 0) != 1 || GET_BIT(read_data, 1) != 1) {
		goto done;
	}

	read_addr = 0x49021068;
	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto done;
	}
	if (GET_BIT(read_data, 14) != 0) {
		goto done;
	}

	is_ib = 1;

done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "%d\n", is_ib);
}
static DRIVER_ATTR_RO(phy_bcm89272_port0_get_ib_test_mode);

/********************************************************************************************************/

static ssize_t phy_bcm89272_loopback_state_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x49060000, state = 2;
	int status;

	spidev_open_kern();

	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto err_done;
	}

	state = GET_BIT(read_data, 14);

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "%d\n", state);
}

static ssize_t phy_bcm89272_loopback_state_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data;
	unsigned int addr = 0x49060000;
	int status;

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}

	if (buf[0] == '1') {
		SET_BIT_ENABLE(data, 14);
	} else if (buf[0] == '0') {
		SET_BIT_DISABLE(data, 14);
	}
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}
static DRIVER_ATTR_RW(phy_bcm89272_loopback_state);

static ssize_t phy_bcm89272_external_loopback_state_show(struct device_driver *drv, char *buf)
{
	unsigned short read_data;
	unsigned int read_addr = 0x49032650, state = 2;
	int status;

	spidev_open_kern();

	status = spi_read16(read_addr, &read_data);
	if (status) {
		goto err_done;
	}

	state = GET_BIT(read_data, 15);

err_done:
	spidev_release_kern();
	return scnprintf(buf, PAGE_SIZE, "%d\n", state);
}

static ssize_t phy_bcm89272_external_loopback_state_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data;
	unsigned int addr = 0x49032650;
	int status;

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	status = spi_read16(addr, &data);
	if (status) {
		goto err_done;
	}

	if (buf[0] == '1') {
		SET_BIT_ENABLE(data, 15);
	} else if (buf[0] == '0') {
		SET_BIT_DISABLE(data, 15);
	}
	status = spi_write16(addr, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}
static DRIVER_ATTR_RW(phy_bcm89272_external_loopback_state);

static ssize_t phy_bcm89272_enable_port3_5_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short data = 0;
	unsigned int addr = 0x4b000003;
	int status;

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	status = spi_write8(addr, data);
	if (status) {
		goto err_done;
	}

	addr = 0x4b000005;
	status = spi_write8(addr, data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
	return count;
}
static DRIVER_ATTR_WO(phy_bcm89272_enable_port3_5);

static void create_default_vlan_for_port(struct device_driver *drv, unsigned short port, unsigned short vlan, unsigned short pri)
{
	unsigned int data = 0;
	unsigned int base_addr = 0x4b003410;
	int status;

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	data += pri << 13;
	data += vlan;
	status = spi_write16(base_addr + (2*port), data);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
}

static void add_vlan_for_port(struct device_driver *drv, unsigned short port, unsigned short vlan)
{
	unsigned int data;
	int status;

	status = spidev_open_kern();
	if (status) {
		goto err_done;
	}

	// read vlan port mask
	status = spi_write16(0x4b000581, vlan);
	if (status) {
		goto err_done;
	}

	status = spi_write32(0x4b000580, 0x0);
	if (status) {
		goto err_done;
	}

	status = spi_write32(0x4b000580, 0x81);
	if (status) {
		goto err_done;
	}
	msleep(300);
	status = spi_read32(0x4b000583, &data);
	SET_BIT_ENABLE(data, port);

	// add port to vlan
	status = spi_write8(0x4b003400, 0xe3);
	if (status) {
		goto err_done;
	}

	status = spi_write32(0x4b000583, data);
	if (status) {
		goto err_done;
	}

	status = spi_write16(0x4b000581, vlan);
	if (status) {
		goto err_done;
	}

	status = spi_write32(0x4b000580, 0x0);
	if (status) {
		goto err_done;
	}

	status = spi_write32(0x4b000580, 0x80);
	if (status) {
		goto err_done;
	}

err_done:
	spidev_release_kern();
}

static void add_static_arl_for_port(struct device_driver *drv, unsigned short port)
{
	spidev_open_kern();

	if (port == 4) {
		spi_write64(0x4b000502, 0x020400000022);
		spi_write16(0x4b000508, 0x21);
		spi_write64(0x4b000510, 0x21020400000022);
		spi_write32(0x4b000518, 0x18004);
		spi_write8(0x4b000500, 0x0);
		spi_write8(0x4b000500, 0x80);
		msleep(300);
		spi_write64(0x4b000502, 0x020400000022);
		spi_write16(0x4b000508, 0x2D);
		spi_write64(0x4b000510, 0x2D020400000022);
		spi_write32(0x4b000518, 0x18004);
		spi_write8(0x4b000500, 0x0);
		spi_write8(0x4b000500, 0x80);
		msleep(300);
		spi_write64(0x4b000502, 0x01005E7F0001);
		spi_write16(0x4b000508, 0x2D);
		spi_write64(0x4b000510, 0x2D01005E7F0001);
		spi_write32(0x4b000518, 0x18151);
		spi_write8(0x4b000500, 0x0);
		spi_write8(0x4b000500, 0x80);
	}

	spidev_release_kern();
}

static ssize_t phy_bcm89272_config_port_vlan_membership_store(struct device_driver *drv, const char *buf, size_t count)
{
	unsigned short port = buf[0] - '0';
	if (port < 0 || port > 8) {
		return count;
	}
	switch(port) {
		case 3:
			create_default_vlan_for_port(drv, port, VLAN_33, VLAN_PRI_0);
			add_vlan_for_port(drv, port, VLAN_33);
			break;
		case 4:
			add_vlan_for_port(drv, port, VLAN_33);
			add_vlan_for_port(drv, port, VLAN_45);
			add_static_arl_for_port(drv, port);
			break;
		case 5:
			create_default_vlan_for_port(drv, port, VLAN_33, VLAN_PRI_0);
			add_vlan_for_port(drv, port, VLAN_33);
			break;
		default:
			add_static_arl_for_port(drv, port);
			break;
	}

	return count;
}
static DRIVER_ATTR_WO(phy_bcm89272_config_port_vlan_membership);

int diagnosis_sysfs_init(struct spi_driver spidev_driver) {
	int status;
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_port0_link_status);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_port0_sqi_status);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_port0_work_mode);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_port3_link_status);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_port3_sqi_status);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_port3_work_mode);
		if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_port0_set_test_mode);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_port3_set_test_mode);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_port0_dut_status);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_port3_dut_status);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_PN_polarity);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_loopback_state);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_external_loopback_state);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_port0_get_normal_test_mode);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_port3_get_normal_test_mode);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_poer0_get_tvco_test_mode);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_port3_get_tvco_test_mode);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_port0_get_ib_test_mode);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_enable_port3_5);
	if (status < 0) {
		goto err_done;
	}
	status = driver_create_file(&(spidev_driver.driver), &driver_attr_phy_bcm89272_config_port_vlan_membership);
err_done:
	if (status < 0) {
		status = -ENOENT;
	}
	return status;
}
/*---------------------------- sysfs operation end ------------------------------*/
#endif