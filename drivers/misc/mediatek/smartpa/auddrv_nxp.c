/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program
 * If not, see <http://www.gnu.org/licenses/>.
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_NXP.c
 *
 * Project:
 * --------
 *    Audio smart pa Function
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *----------------------------------------------------------------------------
 *
 *
 *****************************************************************************
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 ****************************************************************************
 */

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 ****************************************************************************
 */
#include "auddrv_nxp.h"
#include <linux/gpio.h>

#define I2C_MASTER_CLOCK (400000)
#define NXPEXTSPK_I2C_DEVNAME "mtksmartpa"

/* #define smart_no_i2c */

/*****************************************************************************
 *           DEFINE AND CONSTANT
 *****************************************************************************
 */

#define RW_BUFFER_LENGTH (256)

/****************************************************************************
 *           V A R I A B L E     D E L A R A T I O N
 ****************************************************************************
 */

/* I2C variable */
static struct i2c_client *new_client;

static void *TfaI2CDMABuf_va;
static dma_addr_t TfaI2CDMABuf_pa;

static int AudDrv_nxpspk_open(struct inode *inode, struct file *fp);
static long AudDrv_nxpspk_ioctl(struct file *fp, unsigned int cmd,
				unsigned long arg);
static ssize_t AudDrv_nxpspk_read(struct file *fp, char __user *data,
				  size_t count, loff_t *offset);
static ssize_t AudDrv_nxpspk_write(struct file *fp, const char __user *data,
				   size_t count, loff_t *offset);

static const struct file_operations AudDrv_nxpspk_fops = {
	.owner = THIS_MODULE,
	.open = AudDrv_nxpspk_open,
	.unlocked_ioctl = AudDrv_nxpspk_ioctl,
	.write = AudDrv_nxpspk_write,
	.read = AudDrv_nxpspk_read,
};

#ifndef smart_no_i2c
/* function declration */
static int NXPExtSpk_i2c_probe(struct i2c_client *client,
			       const struct i2c_device_id *id);
static int NXPExtSpk_i2c_remove(struct i2c_client *client);

static const struct i2c_device_id nxp_smartpa_i2c_id[] = {{"speaker_amp", 0},
							  {} };

MODULE_DEVICE_TABLE(i2c, nxp_smartpa_i2c_id);

static const struct of_device_id nxp_smartpa_of_match[] = {
	{
		.compatible = "mediatek,speaker_amp",
	},
	{} };

/* i2c driver */
static struct i2c_driver NXPExtSpk_i2c_driver = {
	.driver = {

			.name = "speaker_amp",
			.owner = THIS_MODULE,
			.of_match_table = of_match_ptr(nxp_smartpa_of_match),
			.pm = NULL,
		},
	.probe = NXPExtSpk_i2c_probe,
	.remove = NXPExtSpk_i2c_remove,
	.id_table = nxp_smartpa_i2c_id,
};

static int NXPExtSpk_i2c_probe(struct i2c_client *client,
			       const struct i2c_device_id *id)
{
	new_client = client;
	return 0;
}

static int NXPExtSpk_i2c_remove(struct i2c_client *client)
{
	new_client = NULL;
	return 0;
}

static int NXPExtSpk_registerI2C(void)
{
	if (i2c_add_driver(&NXPExtSpk_i2c_driver)) {
		pr_err("fail to add device into i2c");
		return -1;
	}
	return 0;
}
#endif

bool NXPExtSpk_Register(void)
{

#ifndef smart_no_i2c
	NXPExtSpk_registerI2C();
#endif
	return true;
}

/*****************************************************************************
 * FILE OPERATION FUNCTION
 *  AudDrv_nxpspk_ioctl
 *
 * DESCRIPTION
 *  IOCTL Msg handle
 *
 *****************************************************************************
 */
static long AudDrv_nxpspk_ioctl(struct file *fp, unsigned int cmd,
				unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	default: {
		ret = -1;
		break;
	}
	}
	return ret;
}

static int AudDrv_nxpspk_probe(struct platform_device *platform)
{

	TfaI2CDMABuf_va = dma_alloc_coherent(&platform->dev, 4096,
					     &TfaI2CDMABuf_pa, GFP_KERNEL);
	if (TfaI2CDMABuf_va == NULL) {
		dev_err(&platform->dev,
			"AudDrv_nxpspk_probe dma_alloc_coherent error\n");
		return -1;
	}
	NXPExtSpk_Register();
	return 0;
}

static int AudDrv_nxpspk_remove(struct platform_device *platform)
{
	if (TfaI2CDMABuf_va != NULL) {
		dma_free_coherent(&platform->dev, 4096, TfaI2CDMABuf_va,
				  TfaI2CDMABuf_pa);
		TfaI2CDMABuf_va = NULL;
		TfaI2CDMABuf_pa = 0;
	}
	return 0;
}

static int AudDrv_nxpspk_open(struct inode *inode, struct file *fp)
{
	return 0;
}

extern int mtk_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
			    int num, u32 ext_flag, u32 timing);

static int nxp_i2c_master_send(const struct i2c_client *client, const char *buf,
			       int count)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;

	unsigned int extflag = 0;

	msg.flags = client->flags & I2C_M_TEN;
	msg.addr = client->addr;
	msg.len = count;
	msg.buf = (char *)buf;
	ret = mtk_i2c_transfer(adap, &msg, 1, extflag, I2C_MASTER_CLOCK);

	/*
	 * If everything went ok (i.e. 1 msg transmitted), return #bytes
	 * transmitted, else error code.
	 */
	return (ret == 1) ? count : ret;
}

static int nxp_i2c_master_recv(const struct i2c_client *client, char *buf,
			       int count)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;
	int ret;
	unsigned int extflag = 0;

	msg.len = count;
	msg.flags = client->flags & I2C_M_TEN;
	msg.flags |= I2C_M_RD;
	msg.buf = (char *)buf;
	msg.addr = client->addr;

	ret = mtk_i2c_transfer(adap, &msg, 1, extflag, I2C_MASTER_CLOCK);

	/*
	 * If everything went ok (i.e. 1 msg received), return #bytes received,
	 * else error code.
	 */
	return (ret == 1) ? count : ret;
}

static ssize_t AudDrv_nxpspk_write(struct file *fp, const char __user *data,
				   size_t count, loff_t *offset)
{
	int i = 0;
	int ret;
	char *tmp;
	char *TfaI2CDMABuf = (char *)TfaI2CDMABuf_va;

	tmp = kmalloc(count, GFP_KERNEL);
	if (tmp == NULL)
		return -ENOMEM;
	if (copy_from_user(tmp, data, count)) {
		kfree(tmp);
		return -EFAULT;
	}

	for (i = 0; i < count; i++)
		TfaI2CDMABuf[i] = tmp[i];

	if (count <= 8)
		ret = nxp_i2c_master_send(new_client, tmp, count);
	else
		ret = nxp_i2c_master_send(new_client, (char *)TfaI2CDMABuf,
					  count);

	kfree(tmp);
	return ret;
}

static ssize_t AudDrv_nxpspk_read(struct file *fp, char __user *data,
				  size_t count, loff_t *offset)
{
	int i = 0;
	char *tmp;
	char *TfaI2CDMABuf = (char *)TfaI2CDMABuf_va;
	int ret = 0;

	if (count > 8192)
		count = 8192;

	tmp = kmalloc(count, GFP_KERNEL);
	if (tmp == NULL)
		return -ENOMEM;

	if (count <= 8)
		ret = nxp_i2c_master_recv(new_client, tmp, count);
	else {
		ret = nxp_i2c_master_recv(new_client, (char *)TfaI2CDMABuf,
					  count);
		for (i = 0; i < count; i++)
			tmp[i] = TfaI2CDMABuf[i];
	}

	/* ret = i2c_master_recv(new_client, tmp, count); */

	if (ret >= 0)
		ret = copy_to_user(data, tmp, count) ? (-EFAULT) : ret;
	kfree(tmp);

	return ret;
}

/**************************************************************************
 * STRUCT
 *  File Operations and misc device
 *
 **************************************************************************/

static struct miscdevice AudDrv_nxpspk_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "smartpa_i2c",
	.fops = &AudDrv_nxpspk_fops,
};

/***************************************************************************
 * FUNCTION
 *  AudDrv_nxpspk_mod_init / AudDrv_nxpspk_mod_exit
 *
 * DESCRIPTION
 *  Module init and de-init (only be called when system boot up)
 *
 **************************************************************************/

#ifdef CONFIG_OF
static const struct of_device_id mtk_smart_pa_of_ids[] = {
	{
		.compatible = "mediatek,mtksmartpa",
	},
	{} };
#endif

static struct platform_driver AudDrv_nxpspk = {
	.probe = AudDrv_nxpspk_probe,
	.remove = AudDrv_nxpspk_remove,
	.driver = {

			.name = NXPEXTSPK_I2C_DEVNAME,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mtk_smart_pa_of_ids,
#endif
		},
};

static int __init AudDrv_nxpspk_mod_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&AudDrv_nxpspk);
	if (ret) {
		pr_err("AudDrv Fail:%d - Register DRIVER\n", ret);
		return ret;
	}

	/* register MISC device */
	ret = misc_register(&AudDrv_nxpspk_device);
	if (ret) {
		pr_err("AudDrv_nxpspk_mod_init misc_register Fail:%d\n",
			ret);
		return ret;
	}

	return 0;
}

static void __exit AudDrv_nxpspk_mod_exit(void)
{
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mtksmartpa");
MODULE_AUTHOR("MediaTek Inc");

module_init(AudDrv_nxpspk_mod_init);
module_exit(AudDrv_nxpspk_mod_exit);
