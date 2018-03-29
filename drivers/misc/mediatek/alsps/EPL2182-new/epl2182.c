/* drivers/hwmon/mt6516/amit/epl2182.c - EPL2182 ALS/PS driver
 *
 * Author: MingHsien Hsieh <minghsien.hsieh@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>

#include <linux/io.h>
#include "epl2182.h"
#include "upmu_sw.h"
#include "upmu_common.h"
#include <linux/gpio.h>
#include <linux/of_irq.h>

#include <linux/wakelock.h>
#include <linux/sched.h>

#include <alsps.h>
#ifdef CUSTOM_KERNEL_SENSORHUB
#include <SCP_sensorHub.h>
#endif
/******************************************************************************
 * extern functions
*******************************************************************************/

#define COMPATIABLE_NAME "mediatek,epl2182"

/******************************************************************************
 * configuration
*******************************************************************************/

/* TODO: change ps/als integrationtime */
int PS_INTT = 4;
int ALS_INTT = 7;

#define TXBYTES				2
#define RXBYTES				2
#define PACKAGE_SIZE			2
#define I2C_RETRY_COUNT		3

/* TODO: change delay time */
#define PS_DELAY			10
#define ALS_DELAY			40

/* TODO: parameters for lux equation y = ax + b */
#define LUX_PER_COUNT		1100	/* 1100 = 1.1 * 1000 */

#define IPI_WAIT_RSP_TIMEOUT    (HZ/10)	/* 100ms */

static DEFINE_MUTEX(epl2182_mutex);

struct epl_raw_data {
	u8 raw_bytes[PACKAGE_SIZE];
	u16 ps_raw;
	u16 ps_state;
	u16 ps_int_state;
	u16 als_ch0_raw;
	u16 als_ch1_raw;
};


#define EPL2182_DEV_NAME     "EPL2182"


/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               pr_err(APS_TAG"%s\n", __func__)
#define APS_ERR(fmt, args...)    pr_err(APS_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    pr_err(APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    pr_err(APS_TAG fmt, ##args)
#define FTM_CUST_ALSPS "/data/epl2182"

#define POWER_NONE_MACRO MT65XX_POWER_NONE

static struct i2c_client *epl2182_i2c_client;
static struct alsps_hw alsps_cust;
static struct alsps_hw *hw = &alsps_cust;


/*----------------------------------------------------------------------------*/
static const struct i2c_device_id epl2182_i2c_id[] = { {"EPL2182", 0}, {} };

/* static struct i2c_board_info __initdata i2c_EPL2182= { I2C_BOARD_INFO("EPL2182", (0X92>>1))}; */

/*----------------------------------------------------------------------------*/
static int epl2182_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int epl2182_i2c_remove(struct i2c_client *client);
static int epl2182_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);

static int alsps_local_init(void);
static int alsps_remove(void);
/*----------------------------------------------------------------------------*/
static int epl2182_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int epl2182_i2c_resume(struct i2c_client *client);

static irqreturn_t epl2182_eint_func(int irq, void *desc);


static int set_psensor_intr_threshold(uint16_t low_thd, uint16_t high_thd);
static int set_psensor_threshold(struct i2c_client *client);

static struct epl2182_priv *g_epl2182_ptr;
static bool isInterrupt;

static long long int_top_time;
static int int_flag;

/*----------------------------------------------------------------------------*/
enum CMC_TRC {
	CMC_TRC_ALS_DATA = 0x0001,
	CMC_TRC_PS_DATA = 0X0002,
	CMC_TRC_EINT = 0x0004,
	CMC_TRC_IOCTL = 0x0008,
	CMC_TRC_I2C = 0x0010,
	CMC_TRC_CVT_ALS = 0x0020,
	CMC_TRC_CVT_PS = 0x0040,
	CMC_TRC_DEBUG = 0x0800,
};

/*----------------------------------------------------------------------------*/
enum CMC_BIT {
	CMC_BIT_ALS = 1,
	CMC_BIT_PS = 2,
};

/*----------------------------------------------------------------------------*/
struct epl2182_i2c_addr {	/*define a series of i2c slave address */
	u8 write_addr;
	u8 ps_thd;		/*PS INT threshold */
};

/*----------------------------------------------------------------------------*/
struct epl2182_priv {
	struct alsps_hw *hw;
	struct i2c_client *client;
	struct work_struct eint_work;
	struct work_struct data_work;
#ifdef CUSTOM_KERNEL_SENSORHUB
	struct work_struct init_done_work;
#endif
	/*i2c address group */
	struct epl2182_i2c_addr addr;

	int enable_pflag;
	int enable_lflag;
	struct device_node *irq_node;
	int irq;

	/*misc */
	atomic_t trace;
	atomic_t i2c_retry;
	atomic_t als_suspend;
	atomic_t als_debounce;	/*debounce time after enabling als */
	atomic_t als_deb_on;	/*indicates if the debounce is on */
	atomic_t als_deb_end;	/*the jiffies representing the end of debounce */
	atomic_t ps_mask;	/*mask ps: always return far away */
	atomic_t ps_debounce;	/*debounce time after enabling ps */
	atomic_t ps_deb_on;	/*indicates if the debounce is on */
	atomic_t ps_deb_end;	/*the jiffies representing the end of debounce */
	atomic_t ps_suspend;

	/*data */
	u16 als;
	u16 ps;
	u16 lux_per_count;
	bool als_enable;	/*record current als status */
	bool ps_enable;		/*record current ps status */
	ulong enable;		/*record HAL enalbe status */
	ulong pending_intr;	/*pending interrupt */
	/* ulong        first_read;   // record first read ps and als */

	/*data */
	u16 als_level_num;
	u16 als_value_num;
	u32 als_level[C_CUST_ALS_LEVEL - 1];
	u32 als_value[C_CUST_ALS_LEVEL];
	int ps_cali;

	atomic_t ps_thd_val_high;	/*the cmd value can't be read, stored in ram */
	atomic_t ps_thd_val_low;	/*the cmd value can't be read, stored in ram */
};


#ifdef CONFIG_OF
static const struct of_device_id alsps_of_match[] = {
	{.compatible = "mediatek,alsps"},
	{},
};
#endif


/*----------------------------------------------------------------------------*/
static struct i2c_driver epl2182_i2c_driver = {
	.probe = epl2182_i2c_probe,
	.remove = epl2182_i2c_remove,
	.detect = epl2182_i2c_detect,
	.suspend = epl2182_i2c_suspend,
	.resume = epl2182_i2c_resume,
	.id_table = epl2182_i2c_id,
	.driver = {
		   .name = EPL2182_DEV_NAME,
#ifdef CONFIG_OF
			.of_match_table = alsps_of_match,
#endif
		   },
};


static struct epl2182_priv *epl2182_obj;
static struct epl_raw_data gRawData;

static int alsps_init_flag = -1;	/* 0<==>OK -1 <==> fail */

static struct alsps_init_info epl2182_init_info = {
	.name = EPL2182_DEV_NAME,
	.init = alsps_local_init,
	.uninit = alsps_remove,

};

static DECLARE_WAIT_QUEUE_HEAD(wait_rsp_wq);


/* static struct wake_lock als_lock;  Bob.chen add for if ps run, the system forbid to goto sleep mode. */
/*
//====================I2C write operation===============//
//regaddr: ELAN epl2182 Register Address.
//bytecount: How many bytes to be written to epl2182 register via i2c bus.
//txbyte: I2C bus transmit byte(s). Single byte(0X01) transmit only slave address.
//data: setting value.
//
// Example: If you want to write single byte to 0x1D register address, show below
//	      elan_epl2182_I2C_Write(client,0x1D,0x01,0X02,0xff);
//
*/
static int elan_epl2182_I2C_Write(struct i2c_client *client, uint8_t regaddr, uint8_t bytecount,
				  uint8_t txbyte, uint8_t data)
{
	uint8_t buffer[2];
	int ret = 0;
	int retry;

	/* APS_DBG("[ELAN epl2182] %s\n", __func__); */
	mutex_lock(&epl2182_mutex);
	buffer[0] = (regaddr << 3) | bytecount;
	buffer[1] = data;

	for (retry = 0; retry < I2C_RETRY_COUNT; retry++) {
		ret = i2c_master_send(client, buffer, txbyte);
		if (ret >= 0)
			break;

		APS_DBG("epl2182 i2c write error,TXBYTES %d\r\n", ret);
		mdelay(10);
	}

	if (retry >= I2C_RETRY_COUNT) {
		mutex_unlock(&epl2182_mutex);
		APS_DBG(KERN_ERR "[ELAN epl2182 error] %s i2c write retry over %d\n", __func__,
			I2C_RETRY_COUNT);
		return -EINVAL;
	}
	mutex_unlock(&epl2182_mutex);
	return ret;
}




/*
//====================I2C read operation===============//
*/
static int elan_epl2182_I2C_Read(struct i2c_client *client, uint8_t regaddr, uint8_t bytecount,
				 uint8_t rxbyte, uint8_t *data)
{
	uint8_t buffer[RXBYTES];
	int ret = 0, i = 0;
	int retry;

	/* APS_DBG("[ELAN epl2182] %s\n", __func__); */
	mutex_lock(&epl2182_mutex);
	buffer[0] = (regaddr << 3) | bytecount;

	for (retry = 0; retry < I2C_RETRY_COUNT; retry++) {
		ret = hwmsen_read_block(client, buffer[0], buffer, rxbyte);
		if (ret >= 0)
			break;

		APS_ERR("epl2182 i2c read error,RXBYTES %d\r\n", ret);
		mdelay(10);
	}

	if (retry >= I2C_RETRY_COUNT) {
		APS_ERR(KERN_ERR "[ELAN epl2182 error] %s i2c read retry over %d\n", __func__,
			I2C_RETRY_COUNT);
		mutex_unlock(&epl2182_mutex);
		return -EINVAL;
	}

	for (i = 0; i < PACKAGE_SIZE; i++)
		*data++ = buffer[i];
	mutex_unlock(&epl2182_mutex);

	return ret;
}


static int elan_epl2182_psensor_enable(struct epl2182_priv *epl_data, int enable)
{
	int ret = 0;

	uint8_t regdata;
	uint8_t read_data[2];
	int ps_state;
	struct i2c_client *client = epl_data->client;


	APS_DBG("[ELAN epl2182] %s enable = %d\n", __func__, enable);

	epl_data->enable_pflag = enable;
	ret =
	    elan_epl2182_I2C_Write(client, REG_9, W_SINGLE_BYTE, 0x02,
				   EPL_INT_DISABLE | EPL_DRIVE_120MA);

	if (enable) {
		regdata = EPL_SENSING_2_TIME | EPL_PS_MODE | EPL_L_GAIN;
		regdata = regdata | (isInterrupt ? EPL_C_SENSING_MODE : EPL_S_SENSING_MODE);
		ret = elan_epl2182_I2C_Write(client, REG_0, W_SINGLE_BYTE, 0X02, regdata);

		regdata = PS_INTT << 4 | EPL_PST_1_TIME | EPL_10BIT_ADC;
		ret = elan_epl2182_I2C_Write(client, REG_1, W_SINGLE_BYTE, 0X02, regdata);

		/* set_psensor_intr_threshold(epl_data->hw ->ps_threshold_low,epl_data->hw ->ps_threshold_high); */
		set_psensor_threshold(client);

		ret = elan_epl2182_I2C_Write(client, REG_7, W_SINGLE_BYTE, 0X02, EPL_C_RESET);
		ret = elan_epl2182_I2C_Write(client, REG_7, W_SINGLE_BYTE, 0x02, EPL_C_START_RUN);
		ret = elan_epl2182_I2C_Write(client, REG_9, W_SINGLE_BYTE, 0x02, EPL_INT_ACTIVE_LOW | EPL_DRIVE_120MA);
		msleep(PS_DELAY);
		ret = elan_epl2182_I2C_Read(client, REG_13, R_SINGLE_BYTE, 0x01, read_data);
		ps_state = !((read_data[0] & 0x04) >> 2);
		APS_DBG("epl2182 ps state = %d, gRawData.ps_state = %d, %s\n", ps_state,
		       gRawData.ps_state, __func__);

		int_flag = ps_state;
		schedule_work(&epl_data->data_work);
		gRawData.ps_state = ps_state;	/* update ps state */
		/* APS_LOG("epl2182 gRawData.ps_state = %d, %s\n", gRawData.ps_state, __func__); */
	} else {
		regdata = EPL_SENSING_2_TIME | EPL_PS_MODE | EPL_L_GAIN;
		regdata = regdata | EPL_S_SENSING_MODE;
		ret = elan_epl2182_I2C_Write(client, REG_0, W_SINGLE_BYTE, 0X02, regdata);
		ret = elan_epl2182_I2C_Write(client, REG_9, W_SINGLE_BYTE, 0x02, EPL_INT_DISABLE | EPL_DRIVE_120MA);
	}


	if (ret < 0)
		APS_ERR("[ELAN epl2182 error]%s: ps enable %d fail\n", __func__, ret);
	else
		ret = 0;

	return ret;
}


static int elan_epl2182_lsensor_enable(struct epl2182_priv *epl_data, int enable)
{
	int ret = 0;
	uint8_t regdata;
	struct i2c_client *client = epl_data->client;

	/* APS_LOG("[ELAN epl2182] %s enable = %d\n", __func__, enable); */

	epl_data->enable_lflag = enable;

	if (enable) {
		regdata = EPL_INT_DISABLE;
		ret = elan_epl2182_I2C_Write(client, REG_9, W_SINGLE_BYTE, 0x02, regdata);

		regdata = EPL_S_SENSING_MODE | EPL_SENSING_4_TIME | EPL_ALS_MODE | EPL_AUTO_GAIN;
		ret = elan_epl2182_I2C_Write(client, REG_0, W_SINGLE_BYTE, 0X02, regdata);

		regdata = ALS_INTT << 4 | EPL_PST_1_TIME | EPL_10BIT_ADC;
		ret = elan_epl2182_I2C_Write(client, REG_1, W_SINGLE_BYTE, 0X02, regdata);

		ret = elan_epl2182_I2C_Write(client, REG_10, W_SINGLE_BYTE, 0X02, 0x3e);
		ret = elan_epl2182_I2C_Write(client, REG_11, W_SINGLE_BYTE, 0x02, 0x3e);

		ret = elan_epl2182_I2C_Write(client, REG_7, W_SINGLE_BYTE, 0X02, EPL_C_RESET);
		ret = elan_epl2182_I2C_Write(client, REG_7, W_SINGLE_BYTE, 0x02, EPL_C_START_RUN);
		msleep(ALS_DELAY);
	}


	if (ret < 0)
		APS_ERR("[ELAN epl2182 error]%s: als_enable %d fail\n", __func__, ret);
	else
		ret = 0;

	return ret;
}

/* convert raw to lux */
static int epl2182_get_als_value(struct epl2182_priv *obj, u16 als)
{
	int idx;
	int invalid = 0;
	int lux = 0;

	if (als < 15)
		return 0;

	lux = (als * obj->lux_per_count) / 1000;

	for (idx = 0; idx < obj->als_level_num; idx++) {
		if (lux < obj->hw->als_level[idx])
			break;
	}

	if (idx >= obj->als_value_num) {
		APS_ERR("epl2182 exceed range\n");
		idx = obj->als_value_num - 1;
	}

	if (1 == atomic_read(&obj->als_deb_on)) {
		unsigned long endt = atomic_read(&obj->als_deb_end);

		if (time_after(jiffies, endt))
			atomic_set(&obj->als_deb_on, 0);

		if (1 == atomic_read(&obj->als_deb_on))
			invalid = 1;
	}

	if (!invalid) {
#if defined(MTK_AAL_SUPPORT)
		int level_high = obj->hw->als_level[idx];
		int level_low = (idx > 0) ? obj->hw->als_level[idx - 1] : 0;
		int level_diff = level_high - level_low;
		int value_high = obj->hw->als_value[idx];
		int value_low = (idx > 0) ? obj->hw->als_value[idx - 1] : 0;
		int value_diff = value_high - value_low;
		int value = 0;

		if ((level_low >= level_high) || (value_low >= value_high))
			value = value_low;
		else
			value =
			    (level_diff * value_low + (als - level_low) * value_diff +
			     ((level_diff + 1) >> 1)) / level_diff;

		return value;
#endif
		/* APS_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]); */
		return obj->hw->als_value[idx];
	}
	APS_ERR("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);
	return -1;
}


static int set_psensor_intr_threshold(uint16_t low_thd, uint16_t high_thd)
{
	int ret = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA data;
	union EPL2182_CUST_DATA *pCustData;
	int len;

	/* ps_cali would be add back in SCP side. */
	low_thd -= epl2182_obj->ps_cali;
	high_thd -= epl2182_obj->ps_cali;

	data.set_cust_req.sensorType = ID_PROXIMITY;
	data.set_cust_req.action = SENSOR_HUB_SET_CUST;
	pCustData = (union EPL2182_CUST_DATA *) (&data.set_cust_req.custData);

	pCustData->setPSThreshold.action = EPL2182_CUST_ACTION_SET_PS_THRESHODL;
	pCustData->setPSThreshold.threshold[0] = low_thd;
	pCustData->setPSThreshold.threshold[1] = high_thd;
	len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->setPSThreshold);

	ret = SCP_sensorHub_req_send(&data, &len, 1);
#else				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
	struct epl2182_priv *epld = epl2182_obj;
	struct i2c_client *client = epld->client;
	uint8_t high_msb, high_lsb, low_msb, low_lsb;

	/* APS_LOG("epl2182 %s: low_thd = 0x%X, high_thd = 0x%x\n",__func__, low_thd, high_thd); */

	high_msb = (uint8_t) (high_thd >> 8);
	high_lsb = (uint8_t) (high_thd & 0x00ff);
	low_msb = (uint8_t) (low_thd >> 8);
	low_lsb = (uint8_t) (low_thd & 0x00ff);

	elan_epl2182_I2C_Write(client, REG_2, W_SINGLE_BYTE, 0x02, high_lsb);
	elan_epl2182_I2C_Write(client, REG_3, W_SINGLE_BYTE, 0x02, high_msb);
	elan_epl2182_I2C_Write(client, REG_4, W_SINGLE_BYTE, 0x02, low_lsb);
	elan_epl2182_I2C_Write(client, REG_5, W_SINGLE_BYTE, 0x02, low_msb);
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	return ret;
}



/*----------------------------------------------------------------------------*/
/*
static void epl2182_dumpReg(struct i2c_client *client)
{
    APS_LOG("chip id REG 0x00 value = %8x\n", i2c_smbus_read_byte_data(client, 0x00));
    APS_LOG("chip id REG 0x01 value = %8x\n", i2c_smbus_read_byte_data(client, 0x08));
    APS_LOG("chip id REG 0x02 value = %8x\n", i2c_smbus_read_byte_data(client, 0x10));
    APS_LOG("chip id REG 0x03 value = %8x\n", i2c_smbus_read_byte_data(client, 0x18));
    APS_LOG("chip id REG 0x04 value = %8x\n", i2c_smbus_read_byte_data(client, 0x20));
    APS_LOG("chip id REG 0x05 value = %8x\n", i2c_smbus_read_byte_data(client, 0x28));
    APS_LOG("chip id REG 0x06 value = %8x\n", i2c_smbus_read_byte_data(client, 0x30));
    APS_LOG("chip id REG 0x07 value = %8x\n", i2c_smbus_read_byte_data(client, 0x38));
    APS_LOG("chip id REG 0x09 value = %8x\n", i2c_smbus_read_byte_data(client, 0x48));
    APS_LOG("chip id REG 0x0D value = %8x\n", i2c_smbus_read_byte_data(client, 0x68));
    APS_LOG("chip id REG 0x0E value = %8x\n", i2c_smbus_read_byte_data(client, 0x70));
    APS_LOG("chip id REG 0x0F value = %8x\n", i2c_smbus_read_byte_data(client, 0x71));
    APS_LOG("chip id REG 0x10 value = %8x\n", i2c_smbus_read_byte_data(client, 0x80));
    APS_LOG("chip id REG 0x11 value = %8x\n", i2c_smbus_read_byte_data(client, 0x88));
    APS_LOG("chip id REG 0x13 value = %8x\n", i2c_smbus_read_byte_data(client, 0x98));

}
*/


/*----------------------------------------------------------------------------*/
int hw8k_init_device(struct i2c_client *client)
{
	APS_LOG("hw8k_init_device.........\r\n");

	epl2182_i2c_client = client;

	APS_LOG("epl2182 I2C Addr==[0x%x],line=%d\n", epl2182_i2c_client->addr, __LINE__);

	return 0;
}

/*----------------------------------------------------------------------------*/
int epl2182_get_addr(struct alsps_hw *hw, struct epl2182_i2c_addr *addr)
{
	if (!hw || !addr)
		return -EFAULT;
	addr->write_addr = hw->i2c_addr[0];
	return 0;
}


/*----------------------------------------------------------------------------*/
static void epl2182_power(struct alsps_hw *hw, unsigned int on)
{

	static unsigned int power_on;

	APS_FUN();

	if (hw->power_id != POWER_NONE_MACRO) {
		if (power_on == on) {
			APS_LOG("ignore power control: %d\n", on);
		} else if (on) {
			if (!hwPowerOn(hw->power_id, hw->power_vol, "EPL2182"))
				APS_ERR("power on fails!!\n");
		} else {
			if (!hwPowerDown(hw->power_id, "EPL2182"))
				APS_ERR("power off fail!!\n");
		}
	}
	power_on = on;

}

/*----------------------------------------------------------------------------*/

int epl2182_read_als(struct i2c_client *client, u16 *data)
{

	struct epl2182_priv *obj = i2c_get_clientdata(client);
	uint8_t read_data[2];

	if (client == NULL) {
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}

	elan_epl2182_I2C_Read(obj->client, REG_14, R_TWO_BYTE, 0x02, read_data);
	gRawData.als_ch0_raw = (read_data[1] << 8) | read_data[0];
	elan_epl2182_I2C_Read(obj->client, REG_16, R_TWO_BYTE, 0x02, read_data);
	gRawData.als_ch1_raw = (read_data[1] << 8) | read_data[0];
	*data = gRawData.als_ch1_raw;

	/* APS_LOG("epl2182 read als raw data = %d\n", gRawData.als_ch1_raw); */

	return 0;
}


/*----------------------------------------------------------------------------*/
long epl2182_read_ps(struct i2c_client *client, u16 *data)
{
	struct epl2182_priv *obj = i2c_get_clientdata(client);

	uint8_t read_data[2];

	if (client == NULL) {
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}
	/* elan_epl2182_I2C_Read(client,REG_13,R_SINGLE_BYTE,0x01,read_data); */
	/* APS_DBG("epl2182_read_als read REG_13 raw_bytes: 0x%x\n",read_data[0]); */
	/* setting = read_data[0]; */
	/* if((setting&(3<<4))!=0x10) */
	/* { */
	/* APS_ERR("epl2182 read ps data in wrong mode\n"); */
	/* } */

	/* gRawData.ps_state= !((read_data[0]&0x04)>>2); */
	/* APS_LOG("epl2182 ps state = %d, %s\n", gRawData.ps_state, __func__); */

	elan_epl2182_I2C_Read(obj->client, REG_16, R_TWO_BYTE, 0x02, read_data);
	gRawData.ps_raw = (read_data[1] << 8) | read_data[0];



	if (gRawData.ps_raw < obj->ps_cali)
		*data = 0;
	else
		*data = gRawData.ps_raw - obj->ps_cali;

	/* APS_LOG("epl2182 read ps raw data = %d\n", gRawData.ps_raw); */
	/* APS_LOG("epl2182 read ps binary data = %d\n", gRawData.ps_state); */

	return 0;
}


/*----------------------------------------------------------------------------*/
#ifdef CUSTOM_KERNEL_SENSORHUB
static void alsps_init_done_work(struct work_struct *work)
{
	struct epl2182_priv *obj = epl2182_obj;
	union EPL2182_CUST_DATA *p_cust_data;
	SCP_SENSOR_HUB_DATA data;
	int max_cust_data_size_per_packet;
	int i;
	uint sizeOfCustData;
	uint len;
	char *p = (char *)obj->hw;

	APS_FUN();

	p_cust_data = (union EPL2182_CUST_DATA *) data.set_cust_req.custData;
	sizeOfCustData = sizeof(*(obj->hw));
	max_cust_data_size_per_packet =
	    sizeof(data.set_cust_req.custData) - offsetof(struct EPL2182_SET_CUST, data);

	for (i = 0; sizeOfCustData > 0; i++) {

		data.set_cust_req.sensorType = ID_PROXIMITY;
		data.set_cust_req.action = SENSOR_HUB_SET_CUST;
		p_cust_data->setCust.action = EPL2182_CUST_ACTION_SET_CUST;
		p_cust_data->setCust.part = i;
		if (sizeOfCustData > max_cust_data_size_per_packet)
			len = max_cust_data_size_per_packet;
		else
			len = sizeOfCustData;

		memcpy(p_cust_data->setCust.data, p, len);
		sizeOfCustData -= len;
		p += len;

		len +=
		    offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + offsetof(struct EPL2182_SET_CUST,
									       data);
		SCP_sensorHub_req_send(&data, &len, 1);
	}

	data.set_cust_req.sensorType = ID_PROXIMITY;
	data.set_cust_req.action = SENSOR_HUB_SET_CUST;
	p_cust_data->setEintInfo.action = EPL2182_CUST_ACTION_SET_EINT_INFO;
	p_cust_data->setEintInfo.gpio_mode = GPIO_ALS_EINT_PIN_M_EINT;
	p_cust_data->setEintInfo.gpio_pin = GPIO_ALS_EINT_PIN;
	p_cust_data->setEintInfo.eint_num = CUST_EINT_ALS_NUM;
	p_cust_data->setEintInfo.eint_is_deb_en = CUST_EINT_ALS_DEBOUNCE_EN;
	p_cust_data->setEintInfo.eint_type = CUST_EINT_ALS_TYPE;
	len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(p_cust_data->setEintInfo);
	SCP_sensorHub_req_send(&data, &len, 1);
}
#endif
/*----------------------------------------------------------------------------*/
#ifndef CUSTOM_KERNEL_SENSORHUB

static irqreturn_t epl2182_eint_func(int irq, void *desc)
{
	struct epl2182_priv *obj = g_epl2182_ptr;

	int_top_time = sched_clock();

	if (!obj)
		return IRQ_HANDLED;
#ifndef FPGA_EARLY_PORTING
	disable_irq_nosync(epl2182_obj->irq);
#endif				/* #ifndef FPGA_EARLY_PORTING */
	schedule_work(&obj->eint_work);

	return IRQ_HANDLED;

}

#else
static int alsps_irq_handler(void *data, uint len)
{
	struct epl2182_priv *obj = epl2182_obj;
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P) data;

	if (!obj)
		return -1;

	APS_ERR("len = %d, type = %d, sction = %d, event = %d, data = %d\n", len,
		rsp->rsp.sensorType, rsp->rsp.action, rsp->rsp.errCode, rsp->notify_rsp.data[1]);

	switch (rsp->rsp.action) {
	case SENSOR_HUB_NOTIFY:
		switch (rsp->notify_rsp.event) {
		case SCP_INIT_DONE:
			schedule_work(&obj->init_done_work);
			break;
		case SCP_NOTIFY:
			if (EPL2182_NOTIFY_PROXIMITY_CHANGE == rsp->notify_rsp.data[0]) {
				gRawData.ps_state = rsp->notify_rsp.data[1];
				schedule_work(&obj->eint_work);
			} else if (EPL2182_NOTIFY_PROXIMITY_NOT_CHANGE == rsp->notify_rsp.data[0]) {
				gRawData.ps_state = rsp->notify_rsp.data[1];
				schedule_work(&obj->data_work);
			} else {
				APS_ERR("Unknown notify\n");
			}
			break;
		default:
			APS_ERR("Error sensor hub notify\n");
			break;
		}
		break;
	default:
		APS_ERR("Error sensor hub action\n");
		break;
	}

	return 0;
}
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

/*----------------------------------------------------------------------------*/
static void epl2182_eint_work(struct work_struct *work)
{
#ifdef CUSTOM_KERNEL_SENSORHUB
	int res = 0;

	res = ps_report_interrupt_data(gRawData.ps_state);
	if (res != 0)
		APS_ERR("epl2182_eint_work err: %d\n", res);
#else				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
	struct epl2182_priv *epld = g_epl2182_ptr;
	int err;
	uint8_t read_data[2];
	int flag;

	if (epld->enable_pflag == 0)
		goto exit;

	APS_ERR("epl2182 int top half time = %lld\n", int_top_time);

	elan_epl2182_I2C_Read(epld->client, REG_16, R_TWO_BYTE, 0x02, read_data);
	gRawData.ps_raw = (read_data[1] << 8) | read_data[0];
	APS_LOG("epl2182 ps raw_data = %d\n", gRawData.ps_raw);

	elan_epl2182_I2C_Read(epld->client, REG_13, R_SINGLE_BYTE, 0x01, read_data);
	flag = !((read_data[0] & 0x04) >> 2);
	if (flag != gRawData.ps_state) {
		APS_LOG("epl2182 eint work gRawData.ps_state = %d, flag = %d, %s\n",
			gRawData.ps_state, flag, __func__);

		gRawData.ps_state = flag;	/* update ps state */

		/* let up layer to know */
		err = ps_report_interrupt_data(flag);
		if (err)
			APS_ERR("epl2182 call ps_report_interrupt_data fail = %d\n", err);
		/* APS_LOG("epl2182 xxxxx eint work\n"); */

	} else {
		APS_LOG("epl2182 eint data won't update");
	}

exit:
	elan_epl2182_I2C_Write(epld->client, REG_7, W_SINGLE_BYTE, 0x02, EPL_DATA_UNLOCK);
	if (test_bit(CMC_BIT_ALS, &epld->enable)) {
		/* APS_DBG("als enable eint mask ps!\n"); */
#ifndef FPGA_EARLY_PORTING
		disable_irq(epl2182_obj->irq);
#endif				/* #ifndef FPGA_EARLY_PORTING */
	} else {
		/* APS_DBG("als disable eint unmask ps!\n"); */
#ifndef FPGA_EARLY_PORTING
		enable_irq(epl2182_obj->irq);
#endif				/* #ifndef FPGA_EARLY_PORTING */
	}
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
}



/*----------------------------------------------------------------------------*/
int epl2182_setup_eint(struct i2c_client *client)
{
#ifdef CUSTOM_KERNEL_SENSORHUB
	int err = 0;

	err = SCP_sensorHub_rsp_registration(ID_PROXIMITY, alsps_irq_handler);


#else				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
	struct epl2182_priv *obj = i2c_get_clientdata(client);
	int ret;
	u32 ints[2] = { 0, 0 };
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_cfg;
	struct platform_device *alsps_pdev = get_alsps_platformdev();


	APS_LOG("epl2182_setup_eint\n");


	g_epl2182_ptr = obj;

	/*configure to GPIO function, external interrupt */

#ifndef FPGA_EARLY_PORTING
/* gpio setting */
	pinctrl = devm_pinctrl_get(&alsps_pdev->dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		APS_ERR("Cannot find alsps pinctrl!\n");
	}
	pins_default = pinctrl_lookup_state(pinctrl, "pin_default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		APS_ERR("Cannot find alsps pinctrl default!\n");
	}

	pins_cfg = pinctrl_lookup_state(pinctrl, "pin_cfg");
	if (IS_ERR(pins_cfg)) {
		ret = PTR_ERR(pins_cfg);
		APS_ERR("Cannot find alsps pinctrl pin_cfg!\n");

	}
	pinctrl_select_state(pinctrl, pins_cfg);

	if (epl2182_obj->irq_node) {
		of_property_read_u32_array(epl2182_obj->irq_node, "debounce", ints,
					   ARRAY_SIZE(ints));
		gpio_request(ints[0], "p-sensor");
		gpio_set_debounce(ints[0], ints[1]);
		APS_LOG("ints[0] = %d, ints[1] = %d!!\n", ints[0], ints[1]);

		epl2182_obj->irq = irq_of_parse_and_map(epl2182_obj->irq_node, 0);
		APS_LOG("cm36652_obj->irq = %d\n", epl2182_obj->irq);
		if (!epl2182_obj->irq) {
			APS_ERR("irq_of_parse_and_map fail!!\n");
			return -EINVAL;
		}
		if (request_irq
		    (epl2182_obj->irq, epl2182_eint_func, IRQF_TRIGGER_NONE, "ALS-eint", NULL)) {
			APS_ERR("IRQ LINE NOT AVAILABLE!!\n");
			return -EINVAL;
		}
		enable_irq(epl2182_obj->irq);
	} else {
		APS_ERR("null irq node!!\n");
		return -EINVAL;
	}

	enable_irq(epl2182_obj->irq);
#endif				/* #ifndef FPGA_EARLY_PORTING */

#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	return 0;

}




/*----------------------------------------------------------------------------*/
static int epl2182_init_client(struct i2c_client *client)
{

	struct epl2182_priv *obj = i2c_get_clientdata(client);
	int err = 0;

	APS_LOG("epl2182 [Agold spl] I2C Addr==[0x%x],line=%d\n", epl2182_i2c_client->addr,
		__LINE__);

	/*  interrupt mode */


	APS_FUN();

	if (obj->hw->polling_mode_ps == 0) {
#ifndef FPGA_EARLY_PORTING
		disable_irq(epl2182_obj->irq);
#endif				/* #ifndef FPGA_EARLY_PORTING */
		err = epl2182_setup_eint(client);
		if (err) {
			APS_ERR("setup eint: %d\n", err);
			return err;
		}
		APS_LOG("epl2182 interrupt setup\n");
	}

	err = hw8k_init_device(client);
	if (err != 0) {
		APS_ERR("init dev: %d\n", err);
		return err;
	}

	return err;
}

static void epl2182_check_ps_data(struct work_struct *work)
{

#ifdef CUSTOM_KERNEL_SENSORHUB
	int res = 0;

	res = ps_report_interrupt_data(gRawData.ps_state);
	if (res != 0) {
		APS_ERR("epl2182_eint_work err: %d\n", res);
		return res;
	}
#else

	int flag;
	uint8_t read_data[2];
	int err = 0;
	struct epl2182_priv *epld = epl2182_obj;

	elan_epl2182_I2C_Read(epld->client, REG_13, R_SINGLE_BYTE, 0x01, read_data);
	flag = !((read_data[0] & 0x04) >> 2);
	if (flag != int_flag) {
		APS_ERR("epl2182 call hwmsen_get_interrupt_data fail = %d\n", err);
		goto exit;
	} else {
		/* let up layer to know */
		APS_LOG("epl2182 int_flag state = %d, %s\n", int_flag, __func__);
		err = ps_report_interrupt_data(int_flag);
		if (err != 0) {
			APS_ERR("epl2182 call ps_report_interrupt_data fail = %d\n", err);
			goto exit;
		}
	}
exit:
	return;
#endif
}

/*----------------------------------------------------------------------------*/
static ssize_t epl2182_show_reg(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = epl2182_obj->client;
	ssize_t len = 0;

	if (!epl2182_obj) {
		APS_ERR("epl2182_obj is null!!\n");
		return 0;
	}

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x00 value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x00));
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x01 value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x08));
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x02 value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x10));
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x03 value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x18));
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x04 value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x20));
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x05 value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x28));
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x06 value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x30));
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x07 value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x38));
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x09 value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x48));
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x0D value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x68));
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x0E value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x70));
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x0F value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x71));
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x10 value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x80));
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x11 value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x88));
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "chip id REG 0x13 value = %8x\n",
		     i2c_smbus_read_byte_data(client, 0x98));

	return len;

}

/*----------------------------------------------------------------------------*/
static ssize_t epl2182_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct epl2182_priv *epld = epl2182_obj;
	uint8_t read_data[2];

	if (!epl2182_obj) {
		APS_ERR("epl2182_obj is null!!\n");
		return 0;
	}
	elan_epl2182_I2C_Write(epld->client, REG_7, W_SINGLE_BYTE, 0x02, EPL_DATA_LOCK);

	elan_epl2182_I2C_Read(epld->client, REG_16, R_TWO_BYTE, 0x02, read_data);
	gRawData.ps_raw = (read_data[1] << 8) | read_data[0];
	APS_LOG("ch1 raw_data = %d\n", gRawData.ps_raw);

	elan_epl2182_I2C_Write(epld->client, REG_7, W_SINGLE_BYTE, 0x02, EPL_DATA_UNLOCK);
	len += snprintf(buf + len, PAGE_SIZE - len, "ch1 raw is %d\n", gRawData.ps_raw);
	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl2182_store_als_int_time(struct device_driver *ddri, const char *buf, size_t count)
{
	if (!epl2182_obj) {
		APS_ERR("epl2182_obj is null!!\n");
		return 0;
	}
	if (kstrtoint(buf, 10, &ALS_INTT))
		APS_LOG("als int time is %d\n", ALS_INTT);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl2182_store_ps_int_time(struct device_driver *ddri, const char *buf, size_t count)
{
	if (!epl2182_obj) {
		APS_ERR("epl2182_obj is null!!\n");
		return 0;
	}

	if (kstrtoint(buf, 10, &PS_INTT))
		APS_LOG("ps int time is %d\n", PS_INTT);
	return count;
}


/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(status, S_IWUSR | S_IRUGO, epl2182_show_status, NULL);
static DRIVER_ATTR(reg, S_IWUSR | S_IRUGO, epl2182_show_reg, NULL);
static DRIVER_ATTR(als_int_time, S_IWUSR | S_IRUGO, NULL, epl2182_store_als_int_time);
static DRIVER_ATTR(ps_int_time, S_IWUSR | S_IRUGO, NULL, epl2182_store_ps_int_time);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *epl2182_attr_list[] = {
	&driver_attr_status,
	&driver_attr_reg,
	&driver_attr_als_int_time,
	&driver_attr_ps_int_time,
};

/*----------------------------------------------------------------------------*/
static int epl2182_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(epl2182_attr_list) / sizeof(epl2182_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, epl2182_attr_list[idx]);
		if (err) {
			APS_ERR("driver_create_file (%s) = %d\n", epl2182_attr_list[idx]->attr.name,
				err);
			break;
		}
	}
	return err;
}



/*----------------------------------------------------------------------------*/
static int epl2182_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(epl2182_attr_list) / sizeof(epl2182_attr_list[0]));

	if (!driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, epl2182_attr_list[idx]);

	return err;
}



/******************************************************************************
 * Function Configuration
******************************************************************************/
static int epl2182_open(struct inode *inode, struct file *file)
{
	file->private_data = epl2182_i2c_client;

	APS_FUN();

	if (!file->private_data) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int epl2182_release(struct inode *inode, struct file *file)
{
	APS_FUN();
	file->private_data = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/

static int set_psensor_threshold(struct i2c_client *client)
{
	struct epl2182_priv *obj = i2c_get_clientdata(client);

	int res = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA data;
	union EPL2182_CUST_DATA *pCustData;
	int len;
	int32_t ps_thd_val_low, ps_thd_val_high;

	ps_thd_val_low = atomic_read(&obj->ps_thd_val_low);
	ps_thd_val_high = atomic_read(&obj->ps_thd_val_high);

	ps_thd_val_low -= obj->ps_cali;
	ps_thd_val_high -= obj->ps_cali;

	data.set_cust_req.sensorType = ID_PROXIMITY;
	data.set_cust_req.action = SENSOR_HUB_SET_CUST;
	pCustData = (union EPL2182_CUST_DATA *) (&data.set_cust_req.custData);

	pCustData->setPSThreshold.action = EPL2182_CUST_ACTION_SET_PS_THRESHODL;
	pCustData->setPSThreshold.threshold[0] = ps_thd_val_low;
	pCustData->setPSThreshold.threshold[1] = ps_thd_val_high;
	len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->setPSThreshold);

	res = SCP_sensorHub_req_send(&data, &len, 1);
#else
	int databuf[2];

	APS_ERR("set_psensor_threshold function high: 0x%x, low:0x%x\n",
		atomic_read(&obj->ps_thd_val_high), atomic_read(&obj->ps_thd_val_low));
	databuf[0] = atomic_read(&obj->ps_thd_val_low);
	databuf[1] = atomic_read(&obj->ps_thd_val_high);	/* threshold value need to confirm */

	res = set_psensor_intr_threshold(databuf[0], databuf[1]);
#endif
	return res;
}

/*----------------------------------------------------------------------------*/
static long epl2182_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct epl2182_priv *obj = i2c_get_clientdata(client);
	int err = 0;
	void __user *ptr = (void __user *)arg;
	int dat;
	uint32_t enable;
	int ps_result;
	int ps_cali;
	int threshold[2];
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA data;
	union EPL2182_CUST_DATA *pCustData;
	int len;

	data.set_cust_req.sensorType = ID_PROXIMITY;
	data.set_cust_req.action = SENSOR_HUB_SET_CUST;
	pCustData = (union EPL2182_CUST_DATA *) (&data.set_cust_req.custData);
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	switch (cmd) {
	case ALSPS_SET_PS_MODE:
		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}

		if (enable) {
			if (isInterrupt) {
				err = elan_epl2182_psensor_enable(obj, 1);
				if (err != 0) {
					APS_ERR("enable ps fail: %d\n", err);
					return -1;
				}
			}
			set_bit(CMC_BIT_PS, &obj->enable);
		} else {
			if (isInterrupt) {
				err = elan_epl2182_psensor_enable(obj, 0);
				if (err != 0) {
					APS_ERR("disable ps fail: %d\n", err);
					return -1;
				}
			}
			clear_bit(CMC_BIT_PS, &obj->enable);
		}
		break;


	case ALSPS_GET_PS_MODE:
		enable = test_bit(CMC_BIT_PS, &obj->enable);
		if (copy_to_user(ptr, &enable, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		break;


	case ALSPS_GET_PS_DATA:
		err = elan_epl2182_psensor_enable(obj, 1);
		if (err != 0) {
			APS_ERR("enable ps fail: %d\n", err);
			return -1;
		}
		epl2182_read_ps(obj->client, &obj->ps);
		dat = gRawData.ps_state;

		APS_LOG("ioctl ps state value = %d\n", dat);

		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;


	case ALSPS_GET_PS_RAW_DATA:
		err = elan_epl2182_psensor_enable(obj, 1);
		if (err != 0) {
			APS_ERR("enable ps fail: %d\n", err);
			return -1;
		}
		epl2182_read_ps(obj->client, &obj->ps);
		dat = obj->ps;

		APS_LOG("ioctl ps raw value = %d\n", dat);
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;


	case ALSPS_SET_ALS_MODE:
		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		if (enable)
			set_bit(CMC_BIT_ALS, &obj->enable);
		else
			clear_bit(CMC_BIT_ALS, &obj->enable);
		break;



	case ALSPS_GET_ALS_MODE:
		enable = test_bit(CMC_BIT_ALS, &obj->enable);
		if (copy_to_user(ptr, &enable, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		break;



	case ALSPS_GET_ALS_DATA:
		err = elan_epl2182_lsensor_enable(obj, 1);
		if (err != 0) {
			APS_ERR("disable als fail: %d\n", err);
			return -1;
		}

		epl2182_read_als(obj->client, &obj->als);
		dat = epl2182_get_als_value(obj, obj->als);
		APS_LOG("ioctl get als data = %d\n", dat);


		if (obj->enable_pflag && isInterrupt) {
			err = elan_epl2182_psensor_enable(obj, 1);
			if (err != 0) {
				APS_ERR("disable ps fail: %d\n", err);
				return -1;
			}
		}

		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;


	case ALSPS_GET_ALS_RAW_DATA:
		err = elan_epl2182_lsensor_enable(obj, 1);
		if (err != 0) {
			APS_ERR("disable als fail: %d\n", err);
			return -1;
		}

		epl2182_read_als(obj->client, &obj->als);
		dat = obj->als;
		APS_DBG("ioctl get als raw data = %d\n", dat);


		if (obj->enable_pflag && isInterrupt) {
			err = elan_epl2182_psensor_enable(obj, 1);
			if (err != 0) {
				APS_ERR("disable ps fail: %d\n", err);
				return -1;
			}
		}
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;
	case ALSPS_GET_PS_TEST_RESULT:
		err = epl2182_read_ps(obj->client, &obj->ps);
		if (err)
			goto err_out;
		if (obj->ps > atomic_read(&obj->ps_thd_val_high))
			ps_result = 0;
		else
			ps_result = 1;

		if (copy_to_user(ptr, &ps_result, sizeof(ps_result))) {
			err = -EFAULT;
			goto err_out;
		}
		break;


	case ALSPS_IOCTL_CLR_CALI:
		if (copy_from_user(&dat, ptr, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		if (dat == 0)
			obj->ps_cali = 0;

#ifdef CUSTOM_KERNEL_SENSORHUB
		pCustData->clearCali.action = EPL2182_CUST_ACTION_CLR_CALI;
		len =
		    offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->clearCali);

		err = SCP_sensorHub_req_send(&data, &len, 1);
#endif

		break;

	case ALSPS_IOCTL_GET_CALI:
		ps_cali = obj->ps_cali;
		APS_ERR("%s set ps_calix%x\n", __func__, obj->ps_cali);
		if (copy_to_user(ptr, &ps_cali, sizeof(ps_cali))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_IOCTL_SET_CALI:
		if (copy_from_user(&ps_cali, ptr, sizeof(ps_cali))) {
			err = -EFAULT;
			goto err_out;
		}

		obj->ps_cali = ps_cali;

#ifdef CUSTOM_KERNEL_SENSORHUB
		pCustData->setCali.action = EPL2182_CUST_ACTION_SET_CALI;
		pCustData->setCali.cali = ps_cali;
		len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->setCali);

		err = SCP_sensorHub_req_send(&data, &len, 1);
#endif

		APS_ERR("%s set ps_calix%x\n", __func__, obj->ps_cali);
		break;

	case ALSPS_SET_PS_THRESHOLD:
		if (copy_from_user(threshold, ptr, sizeof(threshold))) {
			err = -EFAULT;
			goto err_out;
		}
		APS_ERR("%s set threshold high: 0x%x, low: 0x%x\n", __func__, threshold[0],
			threshold[1]);
		atomic_set(&obj->ps_thd_val_high, (threshold[0] + obj->ps_cali));
		atomic_set(&obj->ps_thd_val_low, (threshold[1] + obj->ps_cali));	/* need to confirm */

		set_psensor_threshold(obj->client);

		break;

	case ALSPS_GET_PS_THRESHOLD_HIGH:
		APS_ERR("%s get threshold high before cali: 0x%x\n", __func__,
			atomic_read(&obj->ps_thd_val_high));
		threshold[0] = atomic_read(&obj->ps_thd_val_high) - obj->ps_cali;
		APS_ERR("%s set ps_calix%x\n", __func__, obj->ps_cali);
		APS_ERR("%s get threshold high: 0x%x\n", __func__, threshold[0]);
		if (copy_to_user(ptr, &threshold[0], sizeof(threshold[0]))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_PS_THRESHOLD_LOW:
		APS_ERR("%s get threshold low before cali: 0x%x\n", __func__,
			atomic_read(&obj->ps_thd_val_low));
		threshold[0] = atomic_read(&obj->ps_thd_val_low) - obj->ps_cali;
		APS_ERR("%s set ps_calix%x\n", __func__, obj->ps_cali);
		APS_ERR("%s get threshold low: 0x%x\n", __func__, threshold[0]);
		if (copy_to_user(ptr, &threshold[0], sizeof(threshold[0]))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	default:
		APS_ERR("%s not supported = 0x%04x", __func__, cmd);
		err = -ENOIOCTLCMD;
		break;
	}

err_out:
	return err;
}

#ifdef CONFIG_COMPAT
static long epl2182_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long err = 0;

	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_ALSPS_SET_PS_MODE:
		err = file->f_op->unlocked_ioctl(file, ALSPS_SET_PS_MODE, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_MODE:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_MODE, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_DATA:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_DATA, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_RAW_DATA:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_RAW_DATA, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_SET_ALS_MODE:
		err = file->f_op->unlocked_ioctl(file, ALSPS_SET_ALS_MODE, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_ALS_MODE:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_ALS_MODE, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_ALS_DATA:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_ALS_DATA, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_ALS_RAW_DATA:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_ALS_RAW_DATA, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_TEST_RESULT:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_TEST_RESULT, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_IOCTL_CLR_CALI:
		err = file->f_op->unlocked_ioctl(file, ALSPS_IOCTL_CLR_CALI, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_IOCTL_GET_CALI:
		err = file->f_op->unlocked_ioctl(file, ALSPS_IOCTL_GET_CALI, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_IOCTL_SET_CALI:
		err = file->f_op->unlocked_ioctl(file, ALSPS_IOCTL_SET_CALI, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_SET_PS_THRESHOLD:
		err = file->f_op->unlocked_ioctl(file, ALSPS_SET_PS_THRESHOLD, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_THRESHOLD_HIGH:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_THRESHOLD_HIGH, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_THRESHOLD_LOW:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_THRESHOLD_LOW, (unsigned long)arg32);
		break;
	default:
		APS_ERR("%s not supported = 0x%04x", __func__, cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}
#endif

/*----------------------------------------------------------------------------*/
static const struct file_operations epl2182_fops = {
	.owner = THIS_MODULE,
	.open = epl2182_open,
	.release = epl2182_release,
	.unlocked_ioctl = epl2182_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = epl2182_compat_ioctl,
#endif
};


/*----------------------------------------------------------------------------*/
static struct miscdevice epl2182_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &epl2182_fops,
};


/*----------------------------------------------------------------------------*/
static int epl2182_i2c_suspend(struct i2c_client *client, pm_message_t msg)
{
	/* struct epl2182_priv *obj = i2c_get_clientdata(client); */
	int err = 0;

	APS_FUN();
	return err;

}



/*----------------------------------------------------------------------------*/
static int epl2182_i2c_resume(struct i2c_client *client)
{
	/* struct epl2182_priv *obj = i2c_get_clientdata(client); */
	int err = 0;

	APS_FUN();
	return err;
}


/*--------------------------------------------------------------------------------*/
static int als_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/*--------------------------------------------------------------------------------*/
/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */
static int als_enable_nodata(int en)
{
	int res = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#endif

	APS_LOG("epl2182_obj als enable value = %d\n", en);

#ifdef CUSTOM_KERNEL_SENSORHUB
	req.activate_req.sensorType = ID_LIGHT;
	req.activate_req.action = SENSOR_HUB_ACTIVATE;
	req.activate_req.enable = en;
	len = sizeof(req.activate_req);
	res = SCP_sensorHub_req_send(&req, &len, 1);
#else
	if (!epl2182_obj) {
		APS_ERR("epl2182_obj is null!!\n");
		return -1;
	}
	APS_LOG("epl2182_obj als enable value = %d\n", en);

	if (en) {
		set_bit(CMC_BIT_ALS, &epl2182_obj->enable);
#ifndef FPGA_EARLY_PORTING
		disable_irq(epl2182_obj->irq);
#endif				/* #ifndef FPGA_EARLY_PORTING */
		/* APS_DBG("enable als mask ps!\n"); */
	} else {
#ifndef FPGA_EARLY_PORTING
		enable_irq(epl2182_obj->irq);
#endif				/* #ifndef FPGA_EARLY_PORTING */
		/* APS_DBG("disable als unmask ps!\n"); */
		clear_bit(CMC_BIT_ALS, &epl2182_obj->enable);
		if (epl2182_obj->enable_pflag && isInterrupt) {
			res = elan_epl2182_psensor_enable(epl2182_obj, 1);
			if (res != 0) {
				APS_ERR("enable ps fail: %d\n", res);
				return -1;
			}
		}
	}
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	if (res) {
		APS_ERR("als_enable_nodata is failed!!\n");
		return -1;
	}
	return 0;
}

/*--------------------------------------------------------------------------------*/
static int als_set_delay(u64 ns)
{
	return 0;
}

/*--------------------------------------------------------------------------------*/
static int als_get_data(int *value, int *status)
{
	int err = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#else
	/* struct epl2182_priv *obj =NULL; */
#endif

#ifdef CUSTOM_KERNEL_SENSORHUB
	req.get_data_req.sensorType = ID_LIGHT;
	req.get_data_req.action = SENSOR_HUB_GET_DATA;
	len = sizeof(req.get_data_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err) {
		APS_ERR("SCP_sensorHub_req_send fail!\n");
	} else {
		*value = req.get_data_rsp.int16_Data[0];
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}
	if (atomic_read(&epl2182_obj->trace) & CMC_TRC_PS_DATA)
		APS_LOG("value = %d\n", *value);
#else
	if (!epl2182_obj) {
		APS_ERR("epl2182_obj is null!!\n");
		return -1;
	}

	if (0 == atomic_read(&epl2182_obj->als_suspend)) {
		err = elan_epl2182_lsensor_enable(epl2182_obj, 1);
		if (err != 0) {
			APS_ERR("enable als fail: %d\n", err);
			return -1;
		}
		epl2182_read_als(epl2182_obj->client, &epl2182_obj->als);

		*value = epl2182_get_als_value(epl2182_obj, epl2182_obj->als);
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
		/* APS_LOG("get als data->values[0] = %d\n", sensor_data->values[0]); */
	} else {
		APS_LOG("epl2182 sensor in suspend!\n");
		return -1;
	}

	if (epl2182_obj->enable_pflag && isInterrupt) {
		err = elan_epl2182_psensor_enable(epl2182_obj, 1);
		if (err != 0) {
			APS_ERR("enable ps fail: %d\n", err);
			return -1;
		}
	}
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	return err;
}

/*--------------------------------------------------------------------------------*/
/* if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL */
static int ps_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/*--------------------------------------------------------------------------------*/
/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */
static int ps_enable_nodata(int en)
{
	int res = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#endif

	APS_LOG("epl2182_obj als enable value = %d\n", en);

#ifdef CUSTOM_KERNEL_SENSORHUB
	req.activate_req.sensorType = ID_PROXIMITY;
	req.activate_req.action = SENSOR_HUB_ACTIVATE;
	req.activate_req.enable = en;
	len = sizeof(req.activate_req);
	res = SCP_sensorHub_req_send(&req, &len, 1);
#else
	if (!epl2182_obj) {
		APS_ERR("epl2182_obj is null!!\n");
		return -1;
	}
	APS_LOG("epl2182_obj als enable value = %d\n", en);

	if (en) {
		if (isInterrupt) {
			res = elan_epl2182_psensor_enable(epl2182_obj, 1);
			if (res != 0) {
				APS_ERR("enable ps fail: %d\n", res);
				return -1;
			}
		}
		set_bit(CMC_BIT_PS, &epl2182_obj->enable);
	} else {
		if (isInterrupt) {
			res = elan_epl2182_psensor_enable(epl2182_obj, 0);
			if (res != 0) {
				APS_ERR("disable ps fail: %d\n", res);
				return -1;
			}
		}
		clear_bit(CMC_BIT_PS, &epl2182_obj->enable);
	}
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	if (res) {
		APS_ERR("als_enable_nodata is failed!!\n");
		return -1;
	}

	return 0;

}

/*--------------------------------------------------------------------------------*/
static int ps_set_delay(u64 ns)
{
	return 0;
}

/*--------------------------------------------------------------------------------*/
static int ps_get_data(int *value, int *status)
{
	int err = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#endif

#ifdef CUSTOM_KERNEL_SENSORHUB
	req.get_data_req.sensorType = ID_PROXIMITY;
	req.get_data_req.action = SENSOR_HUB_GET_DATA;
	len = sizeof(req.get_data_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err) {
		APS_ERR("SCP_sensorHub_req_send fail!\n");
	} else {
		*value = req.get_data_rsp.int16_Data[0];
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
		APS_LOG("get_data_rsp.int16_Data[0]= 0x%x\n", req.get_data_rsp.int16_Data[0]);
	}

	if (atomic_read(&epl2182_obj->trace) & CMC_TRC_PS_DATA)
		APS_LOG("value = %d\n", *value);
#else

	if (!epl2182_obj) {
		APS_ERR("epl2182_obj is null!!\n");
		return -1;
	}

	err = elan_epl2182_psensor_enable(epl2182_obj, 1);
	if (err != 0) {
		APS_ERR("enable ps fail: %d\n", err);
		return -1;
	}

	epl2182_read_ps(epl2182_obj->client, &epl2182_obj->ps);

	*value = gRawData.ps_state;
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	return err;
}

/*----------------------------------------------------------------------------*/

static int epl2182_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, EPL2182_DEV_NAME);
	return 0;
}


/*----------------------------------------------------------------------------*/
static int epl2182_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct epl2182_priv *obj;
	struct als_control_path als_ctl = { 0 };
	struct als_data_path als_data = { 0 };
	struct ps_control_path ps_ctl = { 0 };
	struct ps_data_path ps_data = { 0 };
	int err = 0;

	APS_FUN();

	/* epl2182_dumpReg(client); */
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(*obj));

	epl2182_obj = obj;
	obj->hw = hw;

	epl2182_get_addr(obj->hw, &obj->addr);

	APS_ERR("addr is 0x%x!\n", obj->addr.write_addr);

	epl2182_obj->als_level_num =
	    sizeof(epl2182_obj->hw->als_level) / sizeof(epl2182_obj->hw->als_level[0]);
	epl2182_obj->als_value_num =
	    sizeof(epl2182_obj->hw->als_value) / sizeof(epl2182_obj->hw->als_value[0]);
	BUG_ON(sizeof(epl2182_obj->als_level) != sizeof(epl2182_obj->hw->als_level));
	memcpy(epl2182_obj->als_level, epl2182_obj->hw->als_level, sizeof(epl2182_obj->als_level));
	BUG_ON(sizeof(epl2182_obj->als_value) != sizeof(epl2182_obj->hw->als_value));
	memcpy(epl2182_obj->als_value, epl2182_obj->hw->als_value, sizeof(epl2182_obj->als_value));

	INIT_WORK(&obj->eint_work, epl2182_eint_work);
#ifdef CUSTOM_KERNEL_SENSORHUB
	INIT_WORK(&obj->init_done_work, alsps_init_done_work);
#endif
	INIT_WORK(&obj->data_work, epl2182_check_ps_data);

	init_waitqueue_head(&wait_rsp_wq);

	obj->client = client;
#ifdef FPGA_EARLY_PORTING
	obj->client->timing = 100;
#else
	obj->client->timing = 400;
#endif

	i2c_set_clientdata(client, obj);

	atomic_set(&obj->als_debounce, 2000);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 1000);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->trace, 0x00);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->ps_thd_val_high, obj->hw->ps_threshold_high);
	atomic_set(&obj->ps_thd_val_low, obj->hw->ps_threshold_low);
	obj->irq_node = of_find_compatible_node(NULL, NULL, "mediatek, als-eint");

	obj->ps_cali = 0;
	obj->ps_enable = 0;
	obj->als_enable = 0;
	obj->lux_per_count = LUX_PER_COUNT;
	obj->enable = 0;
	obj->pending_intr = 0;

	gRawData.ps_state = -1;

	atomic_set(&obj->i2c_retry, 3);

	epl2182_i2c_client = client;

	elan_epl2182_I2C_Write(client, REG_0, W_SINGLE_BYTE, 0x02, EPL_S_SENSING_MODE);
	elan_epl2182_I2C_Write(client, REG_9, W_SINGLE_BYTE, 0x02, EPL_INT_DISABLE);

	err = epl2182_init_client(client);
	if (err)
		goto exit_init_failed;

	APS_ERR("epl2182_init_client OK!\n");

	err = misc_register(&epl2182_device);
	if (err) {
		APS_ERR("epl2182_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	err = epl2182_create_attr(&epl2182_init_info.platform_diver_addr->driver);
	if (err) {
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	if (obj->hw->polling_mode_ps != 1)
		isInterrupt = true;

	als_ctl.open_report_data = als_open_report_data;
	als_ctl.enable_nodata = als_enable_nodata;
	als_ctl.set_delay = als_set_delay;
	als_ctl.is_report_input_direct = false;
#ifdef CUSTOM_KERNEL_SENSORHUB
	als_ctl.is_support_batch = obj->hw->is_batch_supported_als;
#else
	als_ctl.is_support_batch = false;
#endif

	err = als_register_control_path(&als_ctl);
	if (err) {
		APS_ERR("register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	als_data.get_data = als_get_data;
	als_data.vender_div = 100;
	err = als_register_data_path(&als_data);
	if (err) {
		APS_ERR("tregister fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}


	ps_ctl.open_report_data = ps_open_report_data;
	ps_ctl.enable_nodata = ps_enable_nodata;
	ps_ctl.set_delay = ps_set_delay;
	ps_ctl.is_report_input_direct = false;
#ifdef CUSTOM_KERNEL_SENSORHUB
	ps_ctl.is_support_batch = obj->hw->is_batch_supported_ps;
#else
	ps_ctl.is_support_batch = false;
#endif

	err = ps_register_control_path(&ps_ctl);
	if (err) {
		APS_ERR("register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	ps_data.get_data = ps_get_data;
	ps_data.vender_div = 100;
	err = ps_register_data_path(&ps_data);
	if (err) {
		APS_ERR("tregister fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}


	err = batch_register_support_info(ID_LIGHT, als_ctl.is_support_batch, 1, 0);
	if (err) {
		APS_ERR("register light batch support err = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	err = batch_register_support_info(ID_PROXIMITY, ps_ctl.is_support_batch, 1, 0);
	if (err) {
		APS_ERR("register proximity batch support err = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	alsps_init_flag = 0;
	APS_LOG("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
exit_sensor_obj_attach_fail:
	misc_deregister(&epl2182_device);
exit_misc_device_register_failed:
exit_init_failed:
	kfree(obj);
exit:
	epl2182_i2c_client = NULL;
	APS_ERR("%s: err = %d\n", __func__, err);
	alsps_init_flag = -1;
	return err;



}



/*----------------------------------------------------------------------------*/
static int epl2182_i2c_remove(struct i2c_client *client)
{
	int err;

	err = epl2182_delete_attr(&epl2182_init_info.platform_diver_addr->driver);
	if (err)
		APS_ERR("epl2182_delete_attr fail: %d\n", err);

	err = misc_deregister(&epl2182_device);
	if (err)
		APS_ERR("misc_deregister fail: %d\n", err);

	epl2182_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}



/*----------------------------------------------------------------------------*/
static int alsps_local_init(void)
{

	APS_FUN();

	epl2182_power(hw, 1);

	if (i2c_add_driver(&epl2182_i2c_driver)) {
		APS_ERR("add driver error\n");
		return -1;
	}

	APS_ERR("add driver epl2182_i2c_driver ok!\n");

	if (-1 == alsps_init_flag)
		return -1;
	/* printk("fwq loccal init---\n"); */
	return 0;
}

/*----------------------------------------------------------------------------*/
static int alsps_remove(void)
{
	APS_FUN();
	epl2182_power(hw, 0);

	APS_ERR("EPL2182 remove\n");
	i2c_del_driver(&epl2182_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init epl2182_init(void)
{
	hw = get_alsps_dts_func(COMPATIABLE_NAME, hw);
	APS_LOG("%s: i2c_number=%d\n", __func__, hw->i2c_num);
	alsps_driver_add(&epl2182_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit epl2182_exit(void)
{
	APS_FUN();
}

/*----------------------------------------------------------------------------*/
module_init(epl2182_init);
module_exit(epl2182_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("yucong.xiong@mediatek.com");
MODULE_DESCRIPTION("EPL2182 ALSPS driver");
MODULE_LICENSE("GPL");
