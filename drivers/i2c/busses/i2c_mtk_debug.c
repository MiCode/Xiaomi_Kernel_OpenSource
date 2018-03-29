/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/delay.h>
#include "i2c-mtk.h"
/* filer out error messages     */

static char data_buffer[256 * 4];

static inline void i2c_writew_d(struct mt_i2c *i2c, u8 offset, u16 value)
{
	writew(value, (void *)((i2c->base) + (offset)));
}

static inline u16 i2c_readw_d(struct mt_i2c *i2c, u8 offset)
{
	return readw((void const *)((i2c->base) + (offset)));
}

int mt_i2c_test(int id, int addr)
{
	int ret = 0;
	/* ret = i2c_trans_data(id, addr, buffer,,buffer, 1, 1, 0); */
	return ret;
}
EXPORT_SYMBOL(mt_i2c_test);

int mt_i2c_test_multi_wr(int id, int addr)
{
	int ret;
	struct i2c_msg msg[12];
	struct i2c_adapter *adap;
	char buf0[3] = {0x55, 0x00, 0x01};
	char buf1[3] = {0x55, 0x01, 0x02};
	char buf2[3] = {0x55, 0x02, 0x03};
	char buf3[3] = {0x55, 0x03, 0x04};
	char buf4[2] = {0x55, 0x00};
	char buf5[2] = {0xff, 0xff};
	char buf6[2] = {0x55, 0x01};
	char buf7[2] = {0xff, 0xff};
	char buf8[2] = {0x55, 0x02};
	char buf9[2] = {0xff, 0xff};
	char buf10[2] = {0x55, 0x03};
	char buf11[2] = {0xff, 0xff};

	adap = i2c_get_adapter(id);
	if (!adap)
		return -1;

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = 3;
	msg[0].buf = buf0;
	msg[1].addr = addr;
	msg[1].flags = 0;
	msg[1].len = 3;
	msg[1].buf = buf1;
	msg[2].addr = addr;
	msg[2].flags = 0;
	msg[2].len = 3;
	msg[2].buf = buf2;
	msg[3].addr = addr;
	msg[3].flags = 0;
	msg[3].len = 3;
	msg[3].buf = buf3;
	msg[4].addr = addr;
	msg[4].flags = 0;
	msg[4].len = 2;
	msg[4].buf = buf4;
	msg[5].addr = addr;
	msg[5].flags = I2C_M_RD;
	msg[5].len = 1;
	msg[5].buf = buf5;
	msg[6].addr = addr;
	msg[6].flags = 0;
	msg[6].len = 2;
	msg[6].buf = buf6;
	msg[7].addr = addr;
	msg[7].flags = I2C_M_RD;
	msg[7].len = 1;
	msg[7].buf = buf7;
	msg[8].addr = addr;
	msg[8].flags = 0;
	msg[8].len = 2;
	msg[8].buf = buf8;
	msg[9].addr = addr;
	msg[9].flags = I2C_M_RD;
	msg[9].len = 1;
	msg[9].buf = buf9;
	msg[10].addr = addr;
	msg[10].flags = 0;
	msg[10].len = 2;
	msg[10].buf = buf10;
	msg[11].addr = addr;
	msg[11].flags = I2C_M_RD;
	msg[11].len = 1;
	msg[11].buf = buf11;
	hw_trig_i2c_enable(adap);
	ret = hw_trig_i2c_transfer(adap, msg, 4);
	hw_trig_i2c_disable(adap);
	pr_err("camera  0x5500 : %x 0x5501 : %x 0x5502 : %x 0x5503 : %x .\n",
		buf5[0], buf7[0], buf9[0], buf11[0]);
	return ret;
}

int mt_i2c_test_wrrd(int id, int addr, int wr_len, int rd_len, char *wr_buf, char *rd_buf)
{

	int ret;
	struct i2c_msg msg[2];
	struct i2c_adapter *adap;

	adap = i2c_get_adapter(id);
	if (!adap)
		return -1;

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = wr_len;
	msg[0].buf = wr_buf;
	/*for(i = 0; i < wr_len; i++)
	   {
	   printk("cxd wr_len = %d i2c_trans_data-%d = 0x%x\n",wr_len, i, wr_buf[i]);
	   } */
	msg[1].addr = addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = rd_len;
	msg[1].buf = rd_buf;

	ret = i2c_transfer(adap, &msg[0], 2);
	/*printk("cxd i2c_trans_ret = %d\n", ret);
	   for(ii = 0; ii<rd_len; ii++)
	   {
	   printk("cxd i2c_trans_data-%d = 0x%x\n", ii, rd_buf[ii]);
	   } */
	return ret;

}

static ssize_t show_config(struct device *dev, struct device_attribute *attr, char *buff)
{
	int len = strlen(data_buffer);

	memcpy(buff, data_buffer, len);
	pr_alert("Return Value:%s\n\n", data_buffer);
	return len;
}

static int pows(int x, int y)
{
	int result = 1;

	while (y--)
		result *= x;
	return result;
}

int string2hex(const char *buffer, int cnt)
{
	int c = 0;
	char t = 0;
	int count = cnt;

	while (count--) {
		t = *(buffer + cnt - count - 1);
		if (t >= 'A' && t <= 'F')
			c += ((t - 'A') + 10) * pows(16, count);
		else if (t >= '0' && t <= '9')
			c += (t - '0') * pows(16, count);
		else
			c = -1;
	}
	return c;
}

char *get_hexbuffer(char *data_buffer, char *hex_buffer)
{
	char *ptr = data_buffer;
	int index = 0;

	while (*ptr && *++ptr) {
		*(hex_buffer + index++) = string2hex(ptr - 1, 2);
		ptr++;
	}
	*(hex_buffer + index) = 0;
	return hex_buffer;
}

int i2c_trans_data(int bus_id, int address, char *buf_wr, char *buf_rd, int operation, int len_wr,
		   int len_rd)
{
	int ret;

	struct i2c_msg msg;
	struct i2c_adapter *adap;

	adap = i2c_get_adapter(bus_id);
	if (!adap)
		return -1;

	msg.addr = address;
	if (operation == 2) {
		msg.flags = I2C_M_RD;
		msg.len = len_rd;
		msg.buf = (char *)buf_rd;
	} else {
		msg.flags = 0;
		msg.len = len_wr;
		msg.buf = (char *)buf_wr;
	}
	ret = i2c_transfer(adap, &msg, 1);
	/*if(ret > 0) {
	   for(i = 0; i<msg.len; i++)
	   {
	   printk("cxd i2c_trans_data-%d = 0x%x\n", i, msg.buf[i]);
	   }
	   } */
	i2c_put_adapter(adap);
	return (ret == 1) ? msg.len : ret;
}

/* extern mt_i2c ; */
static int i2c_test_reg(int bus_id, int val)
{
	int ret = 0;
	struct i2c_adapter *adap;
	struct mt_i2c *i2c;

	adap = i2c_get_adapter(bus_id);
	if (!adap)
		return -1;
	i2c = container_of(adap, struct mt_i2c, adap);
	/* printk("I2C%d base address %8x\n", bus_id, (unsigned int)(i2c->base)); */
	/* write i2c writable register with 0 */
	i2c_writew_d(i2c, OFFSET_SLAVE_ADDR, val);
	i2c_writew_d(i2c, OFFSET_INTR_MASK, val);
	i2c_writew_d(i2c, OFFSET_INTR_STAT, val);
	i2c_writew_d(i2c, OFFSET_CONTROL, val);
	i2c_writew_d(i2c, OFFSET_TRANSFER_LEN, val);
	i2c_writew_d(i2c, OFFSET_TRANSAC_LEN, val);
	i2c_writew_d(i2c, OFFSET_DELAY_LEN, val);
	i2c_writew_d(i2c, OFFSET_TIMING, val);
	i2c_writew_d(i2c, OFFSET_EXT_CONF, val);
	i2c_writew_d(i2c, OFFSET_IO_CONFIG, val);
	i2c_writew_d(i2c, OFFSET_HS, val);
	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	   transmitted, else error code. */
	i2c_put_adapter(adap);
	return ret;
}

static int i2c_soft_reset(int bus_id)
{
	int ret = 0;
	struct i2c_adapter *adap;
	struct mt_i2c *i2c;

	adap = i2c_get_adapter(bus_id);
	if (!adap)
		return -1;
	i2c = container_of(adap, struct mt_i2c, adap);
	/* printk("I2C%d base address %8x\n", bus_id, (unsigned int)(i2c->base)); */
	/* write i2c writable register with 0 */
	i2c_writew_d(i2c, OFFSET_SOFTRESET, 1);
	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	   transmitted, else error code. */
	i2c_put_adapter(adap);
	return ret;
}

static int i2c_ext_conf_test(int bus_id, int val)
{
	int ret = 0;
	struct i2c_adapter *adap;
	struct mt_i2c *i2c;

	adap = i2c_get_adapter(bus_id);
	if (!adap)
		return -1;
	i2c = container_of(adap, struct mt_i2c, adap);
	/* printk("I2C%d base address %8x\n", bus_id, (unsigned int)(i2c->base)); */
	/* write i2c writable register with 0 */
	i2c_writew_d(i2c, OFFSET_EXT_CONF, val);
	/* printk("EXT_CONF 0x%x", i2c_readw_d(i2c, OFFSET_EXT_CONF)); */
	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	   transmitted, else error code. */
	i2c_put_adapter(adap);
	return ret;
}

static void hex2string(unsigned char *in, unsigned char *out, int length)
{
	unsigned char *ptr = in;
	unsigned char *ptrout = out;
	unsigned char t;

	while (length--) {
		t = (*ptr & 0xF0) >> 4;
		if (t < 10)
			*ptrout = t + '0';
		else
			*ptrout = t + 'A' - 10;

		ptrout++;

		t = (*ptr & 0x0F);
		if (t < 10)
			*ptrout = t + '0';
		else
			*ptrout = t + 'A' - 10;

		ptr++;
		ptrout++;
	}
	*ptrout = 0;
}

static ssize_t set_config(struct device *dev, struct device_attribute *attr, const char *buf,
			  size_t count)
{
	int bus_id;
	int address;
	int operation;
	int wr_number = 0;
	int rd_number = 0;

	int length = 0;
	void *vir_addr_wr = NULL;
	void *vir_addr_rd = NULL;
	/* int status; */
	int ret = 0;
	int scanf_ret = 0;
	unsigned char tmpbuffer[128];

	pr_alert("%s\n", buf);
	scanf_ret = sscanf
	    (buf, "%d %x %d %d %d %1023s", &bus_id, &address, &operation, &wr_number, &rd_number,
	     data_buffer);
	if (scanf_ret) {
		pr_alert("bus_id:%d,address:%x,operation:0x%x\n",
		       bus_id, address, operation);
		if ((address != 0) && (operation <= 2)) {
			length = strlen(data_buffer);

			if (operation == 1) {
				if ((length >> 1) != wr_number)
					pr_alert("Error length of data number = %d,length = %d\n",
					       wr_number, length >> 1);
				vir_addr_wr = kzalloc(wr_number, GFP_KERNEL);
				if (vir_addr_wr == NULL) {

					pr_err("alloc virtual memory failed\n");
					goto err;
				}
				get_hexbuffer(data_buffer, vir_addr_wr);
				pr_alert("data_buffer:%s\n", data_buffer);


			}
			if (operation == 2) {
				vir_addr_rd = kzalloc(rd_number, GFP_KERNEL);
				if (vir_addr_rd == NULL) {

					pr_err("alloc virtual memory failed\n");
					goto err;
				}
			}
			if (operation == 0) {
				vir_addr_wr = kzalloc(wr_number, GFP_KERNEL);
				if (vir_addr_wr == NULL) {

					pr_err("alloc virtual memory failed\n");
					goto err;
				}
				vir_addr_rd = kzalloc(rd_number, GFP_KERNEL);
				if (vir_addr_rd == NULL) {
					kfree(vir_addr_wr);
					pr_err("alloc virtual memory failed\n");
					goto err;
				}
				get_hexbuffer(data_buffer, vir_addr_wr);
				pr_alert("data_buffer:%s\n", data_buffer);
			}



			if (operation == 0) {	/* 0:WRRD 1:WR 2:RD */
				ret =
				    mt_i2c_test_wrrd(bus_id, address, wr_number, rd_number,
						     vir_addr_wr, vir_addr_rd);
			} else {
				ret =
				    i2c_trans_data(bus_id, address, vir_addr_wr, vir_addr_rd,
						   operation, wr_number, rd_number);
			}
			/* dealing */

			if (ret >= 0) {
				if (operation == 2) {
					hex2string(vir_addr_rd, tmpbuffer, rd_number);
					snprintf(data_buffer, sizeof(data_buffer), "1 %s", tmpbuffer);
					pr_alert("Actual return Value:%d %s\n", ret, data_buffer);
				} else if (operation == 0) {
					hex2string(vir_addr_rd, tmpbuffer, rd_number);
					snprintf(data_buffer, sizeof(data_buffer), "1 %s", tmpbuffer);
					pr_alert("Actual return Value:%d %s\n", ret, data_buffer);
				} else {
					snprintf(data_buffer, sizeof(data_buffer), "1 %s", "00");
					pr_alert("Actual return Value:%d %s\n", ret, data_buffer);
				}

			} else if (ret < 0) {
				if (ret == -EINVAL)
					snprintf(data_buffer, sizeof(data_buffer), "0 %s", "Invalid Parameter");
				else if (ret == -ETIMEDOUT)
					snprintf(data_buffer, sizeof(data_buffer), "0 %s", "Transfer Timeout");
				else if (ret == -EREMOTEIO)
					snprintf(data_buffer, sizeof(data_buffer), "0 %s", "Ack Error");
				else
					snprintf(data_buffer, sizeof(data_buffer), "0 %s", "unknown error");
				pr_alert("Actual return Value:%d %p\n", ret, data_buffer);
			}
			kfree(vir_addr_rd);
			kfree(vir_addr_wr);

		} else {
			struct i2c_adapter *adap = i2c_get_adapter(bus_id);
			if (adap) {
				struct mt_i2c *i2c = i2c_get_adapdata(adap);

				if (operation == 3) {
					i2c_dump_info(i2c);
				} else if (operation == 4) {
					i2c_test_reg(bus_id, 0);
					i2c_dump_info(i2c);
					i2c_test_reg(bus_id, 0xFFFFFFFF);
					i2c_dump_info(i2c);
				} else if (operation == 5) {
					i2c_ext_conf_test(bus_id, address);
				} else if (operation == 9) {
					i2c_soft_reset(bus_id);
					i2c_dump_info(i2c);
				} else if (operation == 6) {
					mt_i2c_test_multi_wr(bus_id, address);
					if (bus_id == 0) {
						/* I2C0 PINMUX2 power on */
						/* hwPowerOn(MT65XX_POWER_LDO_VMC1,VOL_DEFAULT,"i2c_pinmux"); */
						/* hwPowerOn(MT65XX_POWER_LDO_VMCH1,VOL_DEFAULT,"i2c_pinmux"); */
					}

				} else if (operation == 7) {
					mt_i2c_test(1, 0x50);
				} else {
					dev_err(dev, "i2c debug system: Parameter invalid!\n");
				}
			} else {
				/*adap invalid */
				dev_err(dev, "i2c debug system: get adap fail!\n");
			}
		}
	} else {
		/*parameter invalid */
		dev_err(dev, "i2c debug system: Parameter invalid!\n");
	}

	return count;
 err:
	pr_err("analyze failed\n");
	return -1;
}

static DEVICE_ATTR(ut, 0660, show_config, set_config);

static int i2c_common_probe(struct platform_device *pdev)
{
	int ret = 0;
	/* your code here£¬your should save client in your own way */
	pr_alert("i2c_common device probe\n");
	ret = device_create_file(&pdev->dev, &dev_attr_ut);
	pr_alert("i2c_common device probe ret = %d\n", ret);
	return ret;
}

static int i2c_common_remove(struct platform_device *pdev)
{
	int ret = 0;
	/* your code here */
	device_remove_file(&pdev->dev, &dev_attr_ut);
	return ret;
}

static struct platform_driver i2c_common_driver = {
	.driver = {
		   .name = "mt-iicd",
		   .owner = THIS_MODULE,
		   },

	.probe = i2c_common_probe,
	.remove = i2c_common_remove,
};


/* platform device */
static struct platform_device i2c_common_device = {
	.name = "mt-iicd",
};

static int __init xxx_init(void)
{
	int err;

	pr_alert("i2c_common device init\n");
	err = platform_device_register(&i2c_common_device);
	if (err)
		return err;

	err = platform_driver_register(&i2c_common_driver);
	if (err)
		platform_device_unregister(&i2c_common_device);

	return err;
}

static void __exit xxx_exit(void)
{
	platform_driver_unregister(&i2c_common_driver);
	platform_device_unregister(&i2c_common_device);
}
module_init(xxx_init);
module_exit(xxx_exit);
/* module_platform_driver(i2c_common_driver); */


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek I2C Bus Driver Test Driver");
MODULE_AUTHOR("Ranran Lu");
