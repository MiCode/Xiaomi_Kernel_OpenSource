/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <linux/mutex.h>
#include "camera_ldo.h"


/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/

static struct i2c_client *camera_ldo_i2c_client;
static DEFINE_MUTEX(camera_ldo_mutex);
/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int camera_ldo_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int camera_ldo_i2c_remove(struct i2c_client *client);
int deviceid = -1;
int deviceid_tmp = -1;
/*****************************************************************************
 * Extern Area
 *****************************************************************************/
void camera_ldo_set_en_ldo(CAMERA_LDO_SELECT ldonum,unsigned int en)
{
	s32 ret=0;
	unsigned int value =0;

	if (NULL == camera_ldo_i2c_client) {
		CAMERA_LDO_PRINT("[camera_ldo] camera_ldo_i2c_client is null!!\n");
		return ;
	}

	mutex_lock(&camera_ldo_mutex);

	if(deviceid == WL2868C_deviceid)
	{
		ret= i2c_smbus_read_byte_data(camera_ldo_i2c_client, CAMERA_LDO_LDO_EN_ADDR_2);
	}
	else
	{
		ret= i2c_smbus_read_byte_data(camera_ldo_i2c_client, CAMERA_LDO_LDO_EN_ADDR);
	}
	if(ret <0)
	{
		CAMERA_LDO_PRINT("[camera_ldo] camera_ldo_set_en_ldo read error!\n");
		mutex_unlock(&camera_ldo_mutex);
		return;
	}

	if(en == 0)
	{
		value = ret & (~(0x01<<ldonum));
	}
	else
	{
		value = ret|(0x01<<ldonum);
	}
	if(deviceid == WL2868C_deviceid)
	{
		i2c_smbus_write_byte_data(camera_ldo_i2c_client,CAMERA_LDO_LDO_EN_ADDR_2,value);
	}
	else
	{
		i2c_smbus_write_byte_data(camera_ldo_i2c_client,CAMERA_LDO_LDO_EN_ADDR,value);
	}
	CAMERA_LDO_PRINT("[camera_ldo] camera_ldo_set_en_ldo enable before:%x after set :%x  \n",ret,value);

	mutex_unlock(&camera_ldo_mutex);

	return;

}
EXPORT_SYMBOL(camera_ldo_set_en_ldo);

//VOUT1=0.496V+LDO1_VOUT [6:0]*0.008.   LDO1/LDO2
//VOUTx=1.504V+LDOx_VOUT [7:0]*0.008.   LDO3~LDO7
void camera_ldo_set_ET5907MV_value(CAMERA_LDO_SELECT ldonum,unsigned int value,unsigned char *regaddr,unsigned int *Ldo_out)
{
	switch(ldonum)
	{
		case CAMERA_LDO_DVDD1:
		case CAMERA_LDO_DVDD2:
			if(value<600)
			{
				CAMERA_LDO_PRINT("[camera_ldo] error vol!!!\n");
				goto exit;
			}
			else
			{
				*Ldo_out = (value-600)/6;
				CAMERA_LDO_PRINT("camera DVDD %d\n",value);
			}
			break;
		case CAMERA_LDO_VDDAF:
		case CAMERA_LDO_VDDIO:
		case CAMERA_LDO_PSENSOR:
		case CAMERA_LDO_AVDD2:
		case CAMERA_LDO_AVDD1:
			if(value<1200)
			{
				CAMERA_LDO_PRINT("[camera_ldo] error vol!!!\n");
				goto exit;

			}
			else
			{
				*Ldo_out = (value-1200)/10;
				CAMERA_LDO_PRINT("camera AVDD %d\n",value);
			}
			break;
		default:
			goto exit;
			break;
	}
	*regaddr = ldonum+CAMERA_LDO_LDO1_OUT_ADDR;

	CAMERA_LDO_PRINT("[camera_ldo] ldo=%d,value=%d,Ldo_out:%d,regaddr=0x%x \n",ldonum,value,*Ldo_out,*regaddr);

exit:
	CAMERA_LDO_PRINT("[camera_ldo] %s exit!!!\n",__FUNCTION__);
}

void camera_ldo_set_WL2868C_value(CAMERA_LDO_SELECT ldonum,unsigned int value,unsigned char *regaddr,unsigned int *Ldo_out)
{
	switch(ldonum)
	{
		case CAMERA_LDO_DVDD1:
		case CAMERA_LDO_DVDD2:
			if(value<496)
			{
				CAMERA_LDO_PRINT("[camera_ldo] error vol!!!\n");
				goto exit;
			}
			else
			{
				*Ldo_out = (value-496)/8;
				CAMERA_LDO_PRINT("camera DVDD %d\n",value);
			}
			break;
		case CAMERA_LDO_VDDAF:
		case CAMERA_LDO_VDDIO:
		case CAMERA_LDO_PSENSOR:
		case CAMERA_LDO_AVDD2:
		case CAMERA_LDO_AVDD1:
			if(value<1504)
			{
				CAMERA_LDO_PRINT("[camera_ldo] error vol!!!\n");
				goto exit;

			}
			else
			{
				*Ldo_out = (value-1504)/8;
				CAMERA_LDO_PRINT("camera AVDD %d\n",value);
			}
			break;
		default:
			goto exit;
			break;
	}
	*regaddr = ldonum+CAMERA_LDO_LDO1_OUT_ADDR_2;

	CAMERA_LDO_PRINT("[camera_ldo] ldo=%d,value=%d,Ldo_out:%d,regaddr=0x%x \n",ldonum,value,*Ldo_out,*regaddr);

exit:
	CAMERA_LDO_PRINT("[camera_ldo] %s exit!!!\n",__FUNCTION__);
}

void camera_ldo_set_FAN53870_value(CAMERA_LDO_SELECT ldonum,unsigned int value,unsigned char *regaddr,unsigned int *Ldo_out)
{
	switch(ldonum)
	{
		case CAMERA_LDO_DVDD1:
		case CAMERA_LDO_DVDD2:
			if(value<800)
			{
				CAMERA_LDO_PRINT("[camera_ldo] error vol!!!\n");
				goto exit;
			}
			else
			{
				*Ldo_out = (value-800)/8 + 99;
				CAMERA_LDO_PRINT("camera DVDD %d\n",value);
			}
			break;
		case CAMERA_LDO_VDDAF:
		case CAMERA_LDO_VDDIO:
		case CAMERA_LDO_PSENSOR:
		case CAMERA_LDO_AVDD2:
		case CAMERA_LDO_AVDD1:
			if(value<1500)
			{
				CAMERA_LDO_PRINT("[camera_ldo] error vol!!!\n");
				goto exit;

			}
			else
			{
				*Ldo_out = (value-1500)/8 + 16;
				CAMERA_LDO_PRINT("camera AVDD %d\n",value);
			}
			break;
		default:
			goto exit;
			break;
	}
	*regaddr = ldonum+CAMERA_LDO_LDO1_OUT_ADDR;

	CAMERA_LDO_PRINT("[camera_ldo] ldo=%d,value=%d,Ldo_out:%d,regaddr=0x%x \n",ldonum,value,*Ldo_out,*regaddr);

exit:
	CAMERA_LDO_PRINT("[camera_ldo] %s exit!!!\n",__FUNCTION__);
}


void camera_ldo_set_ldo_value(CAMERA_LDO_SELECT ldonum,unsigned int value)
{
	unsigned int  Ldo_out =0;
	unsigned char regaddr =0;
	s32 ret = 0;
	CAMERA_LDO_PRINT("[camera_ldo] %s enter!!!\n",__FUNCTION__);

	if (NULL == camera_ldo_i2c_client) {
		CAMERA_LDO_PRINT("[camera_ldo] camera_ldo_i2c_client is null!!\n");
		return ;
	}
	if(ldonum >= CAMERA_LDO_MAX)
	{
		CAMERA_LDO_PRINT("[camera_ldo] error ldonum not support!!!\n");
		return;
	}
	if(deviceid == ET5907MV_deviceid)
	{
		//一供
		camera_ldo_set_ET5907MV_value(ldonum,value,&regaddr,&Ldo_out);
		CAMERA_LDO_PRINT("ET5907MV set done\n");
	}
	else if(deviceid == WL2868C_deviceid)
	{
		//二供
		camera_ldo_set_WL2868C_value(ldonum,value,&regaddr,&Ldo_out);
		CAMERA_LDO_PRINT("WL2868C set done\n");
	}
	else if(deviceid == FAN53870_deviceid)
	{
		//三供
		camera_ldo_set_FAN53870_value(ldonum,value,&regaddr,&Ldo_out);
		CAMERA_LDO_PRINT("FAN53870 set done\n");
	}

	mutex_lock(&camera_ldo_mutex);

	i2c_smbus_write_byte_data(camera_ldo_i2c_client,regaddr,Ldo_out);
	ret = i2c_smbus_read_byte_data(camera_ldo_i2c_client,regaddr);

	mutex_unlock(&camera_ldo_mutex);
	CAMERA_LDO_PRINT("[camera_ldo] after write ret=0x%x\n",ret);
}
EXPORT_SYMBOL(camera_ldo_set_ldo_value);

/*****************************************************************************
 * Data Structure
 ********************************************************/
static const struct of_device_id i2c_of_match[] = {
	{ .compatible = "mediatek,i2c_camera_ldo", },
	{ .compatible = "mediatek,i2c_camera_ldo2", },
	{},
};

static const struct i2c_device_id camera_ldo_i2c_id[] = {
    {"camera_ldo_I2C", 0},
    {},
};
static struct i2c_driver camera_ldo_i2c_driver = {
/************************************************************
Attention:
Althouh i2c_bus do not use .id_table to match, but it must be defined,
otherwise the probe function will not be executed!
************************************************************/
	.id_table = camera_ldo_i2c_id,
	.probe = camera_ldo_i2c_probe,
	.remove = camera_ldo_i2c_remove,
	.driver = {
		.name = "camera_ldo_I2C",
		.of_match_table = i2c_of_match,
	},
};

static int camera_ldo_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	if (NULL == client) {
		CAMERA_LDO_PRINT("[camera_ldo] i2c_client is NULL\n");
		return -1;
	}
	deviceid_tmp = i2c_smbus_read_byte_data(client, CAMERA_DEVICEID_2_ADDR);
	CAMERA_LDO_PRINT("i2c read deviceid = 0x%x\n",deviceid_tmp);
	if(deviceid_tmp < 0)
	{
		CAMERA_LDO_PRINT("i2c failed to read\n");
		return -1;
	}
	deviceid = deviceid_tmp;
	camera_ldo_i2c_client = client;

	camera_ldo_set_ldo_value(CAMERA_LDO_PSENSOR,3300); //this is p sensor power
	camera_ldo_set_en_ldo(CAMERA_LDO_PSENSOR,1);
	CAMERA_LDO_PRINT("%s p sensor 3.3v power on success \n",__func__);
	CAMERA_LDO_PRINT("[camera_ldo]camera_ldo_i2c_probe success addr = 0x%x\n", client->addr);
	return 0;
}

static int camera_ldo_i2c_remove(struct i2c_client *client)
{
	camera_ldo_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

static int __init camera_ldo_init(void)
{
	i2c_add_driver(&camera_ldo_i2c_driver);
    return 0;
}

static void __exit camera_ldo_exit(void)
{
	i2c_del_driver(&camera_ldo_i2c_driver);
}

module_init(camera_ldo_init);
module_exit(camera_ldo_exit);

MODULE_AUTHOR("AmyGuo <guohuiqing@huaqin.com>");
MODULE_DESCRIPTION("MTK EXT CAMERA LDO Driver");
MODULE_LICENSE("GPL");
