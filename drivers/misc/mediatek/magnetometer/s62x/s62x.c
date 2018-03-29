/* s62x.c - s62x compass driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <cust_mag.h>
#include "mag.h"

/*----------------------------------------------------------------------------*/

/****** Begin of Customization ****/
/* #define USE_ALTERNATE_ADDRESS      //can change i2c address when the first(0x0c<<1) conflig whit other devices */
/* #define FORCE_KERNEL2X_STYLE        //auto fit android version(cause the different i2c flow) */
/****** End of Customization ******/

#define PLATFORM_DRIVER_NAME    "msensor"
/* The following items are for internal testing only.
    -USE_ALTERNATE_ADDRESS: (--)default (+*)test S
    +USE_ALTERNATE_ADDRESS: (--)default (+-)test S (-+)test A
*/
/* #define _MAGIC_KPT_COMMAND */
/* #define _FORCE_PROBE_ERROR */

/*----------------------------------------------------------------------------*/
#define S62X_I2C_ADDR1          (0x0e)
#define S62X_I2C_ADDR2          (0x1e)

#ifdef USE_ALTERNATE_ADDRESS
#define S62X_I2C_ADDRESS        S62X_I2C_ADDR2
#else
#define S62X_I2C_ADDRESS        S62X_I2C_ADDR1
#endif
#define S62X_DEV_NAME           "s62x"
#define DRIVER_VERSION          "2.0.0"

/*----------------------------------------------------------------------------*/
#define DEBUG                   0
#define S62X_DEBUG              1
#define S62X_DEBUG_MSG          1
#define S62X_DEBUG_FUNC         1
#define S62X_DEBUG_DATA         1
#define S62X_RETRY_COUNT        9
#define S62X_DEFAULT_DELAY      100

#if S62X_DEBUG_MSG
#define SSMDBG(format, ...)			pr_debug("S62X " format "\n", ## __VA_ARGS__)
#else
#define SSMDBG(format, ...)
#endif

#if S62X_DEBUG_FUNC
#define SSMFUNC(func)           pr_debug("S62X " func " is called\n")
#else
#define SSMFUNC(func)
#endif
#define SSMERR(format, ...)			pr_err("S62X " format "\n", ## __VA_ARGS__)

/* Don't change this if you don't know why! (refer comment:ticket:35:2) */
#define PROJECT_ID              "S628"

/*----------------------------------------------------------------------------*/
#define SENSOR_DATA_SIZE	6
#define SENSOR_DATA_COUNT       (SENSOR_DATA_SIZE/sizeof(short))
#define CALIBRATION_DATA_SIZE   12

#define RWBUF_SIZE              96
#define S62X_BUFSIZE            96

#define CONVERT_O		1
#define CONVERT_O_DIV		1
#define CONVERT_M		5
#define CONVERT_M_DIV		28

/*----------------------------------------------------------------------------*/
#define SS_SENSOR_MODE_OFF	0
#define SS_SENSOR_MODE_MEASURE	1

/*----------------------------------------------------------------------------*/
#define S62X_IDX_DEVICE_ID      0x00
#define DEVICE_ID_VALUE         0x21

#define S62X_IDX_DEVICE_INFO    0x01
#define DEVICE_INFO_VALUE       0x10

#define S62X_IDX_STA1           0x02
#define STA1_DRDY               0x01

#define S62X_IDX_X_LSB          0x03
#define S62X_IDX_STA2           0x09

#define S62X_IDX_MODE           0x0A
#define MODE_TRIGGER            0x01

#define S62X_IDX_I2CDIS         0x0F
#define I2CDIS_CIC              0x00
#define I2CDIS_ADC              0x01

#define S62X_IDX_SET_RESET      0x1A
#define SET_RESET_SET           0x01
#define SET_RESET_RESET         0x02

#define S62X_IDX_SET_RESET_CON  0x1C
#define S62X_IDX_VBGSEL         0x1D
#define S62X_IDX_OSC_TRIM       0x1E

#define S62X_IDX_ECO1           0x1F
#ifdef USE_ALTERNATE_ADDRESS
#define ECO1_DEFAULT            0x44
#else
#define ECO1_DEFAULT            0x40
#endif

#define S62X_IDX_ECO2           0x20
#define ECO2_MRAS_NO_RST        0x80

#define S62X_IDX_CHOP           0x21
#define S62X_IDX_BIAS           0x23
#define S62X_IDX_LDO_SEL        0x24
#define S62X_IDX_DATA_POL       0x25
#define S62X_IDX_ADC_RDY_CNT    0x27
#define S62X_IDX_W2             0x28
#define S62X_IDX_PW_VALUE       0x35

#define S62X_IDX_PROBE_OE       0x37
#define PROBE_OE_WATCHDOG       0x10
#define S62X_MUL_X              20
#define S62X_DIV_X              28
#define S62X_OFF_X              2048
#define S62X_MUL_Y              20
#define S62X_DIV_Y              28
#define S62X_OFF_Y              2048
#define S62X_MUL_Z              20
#define S62X_DIV_Z              32
#define S62X_OFF_Z              2048

/*----------------------------------------------------------------------------*/
static struct i2c_client *this_client;

static short last_m_data[SENSOR_DATA_COUNT];
static struct mutex last_m_data_mutex;

static short sensor_data[CALIBRATION_DATA_SIZE];
static struct mutex sensor_data_mutex;
static DECLARE_WAIT_QUEUE_HEAD(open_wq);

static short ssmd_delay = S62X_DEFAULT_DELAY;
static char ssmd_status[RWBUF_SIZE];

static atomic_t open_flag = ATOMIC_INIT(0);
static atomic_t m_flag = ATOMIC_INIT(0);
static atomic_t o_flag = ATOMIC_INIT(0);
static atomic_t m_get_data;
static atomic_t o_get_data;
static atomic_t dev_open_count;
static atomic_t init_phase = ATOMIC_INIT(2);	/* 1 = id check ok, 0 = init ok */

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id s62x_i2c_id[] = { {S62X_DEV_NAME, 0}, {} };

/*
static unsigned short s62x_force[] = { 0x00, S62X_I2C_ADDRESS, I2C_CLIENT_END, I2C_CLIENT_END };
static const unsigned short *const s62x_forces[] = { s62x_force, NULL };
 */
struct mag_hw mag_cust;
static struct mag_hw *hw = &mag_cust;

/*----------------------------------------------------------------------------*/
static int s62x_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id);
static int s62x_i2c_remove(struct i2c_client *client);
static int s62x_i2c_detect(struct i2c_client *client,
			   struct i2c_board_info *info);
static int s62x_suspend(struct i2c_client *client, pm_message_t msg);
static int s62x_resume(struct i2c_client *client);
static int s62x_local_init(void);
static int s62x_local_remove(void);



/*----------------------------------------------------------------------------*/
enum SS_TRACE {
	SS_FUN_DEBUG = 0x01,
	SS_DATA_DEBUG = 0X02,
	SS_HWM_DEBUG = 0X04,
	SS_CTR_DEBUG = 0X08,
	SS_I2C_DEBUG = 0x10,
};

/*----------------------------------------------------------------------------*/
struct s62x_i2c_data {
	struct i2c_client *client;
	struct mag_hw *hw;
	atomic_t layout;
	atomic_t trace;
	struct hwmsen_convert cvt;
};

#define L2CHIP(x)       ((x)/10)	/* layout to chip id */
#define L2CVTI(x)       ((x)%10)	/* layout to cvt index */

static const struct of_device_id msensor_of_match[] = {
	{ .compatible = "mediatek,msensor", },
	{},
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver s62x_i2c_driver = {
	.driver = {
		   /*.owner = THIS_MODULE, */
		   .name = S62X_DEV_NAME,
			 .of_match_table = msensor_of_match,
		   },
	.probe = s62x_i2c_probe,
	.remove = s62x_i2c_remove,
	.detect = s62x_i2c_detect,
	.suspend	= s62x_suspend,
	.resume	= s62x_resume,
	.id_table = s62x_i2c_id,
};

static struct platform_driver ssm_sensor_driver = {
	.driver = {
		.name = "msensor",
		.of_match_table = msensor_of_match,
	}
};

static struct mag_init_info s62x_init_info = {
	.name = S62X_DEV_NAME,
	.init = s62x_local_init,
	.uninit = s62x_local_remove,
	.platform_diver_addr = &ssm_sensor_driver,
};


/*----------------------------------------------------------------------------*/
static void s62x_power(struct mag_hw *hw, unsigned int on)
{
}

/*----------------------------------------------------------------------------*/
static int I2C_RxData(char *rxData, int length)
{
	uint8_t loop_i;

#if DEBUG
	int i;
	struct i2c_client *client = this_client;
	struct s62x_i2c_data *data = i2c_get_clientdata(client);
	char addr = rxData[0];
#endif

	/* Caller should check parameter validity. */
	if ((rxData == NULL) || (length < 1))
		return -EINVAL;

	for (loop_i = 0; loop_i < S62X_RETRY_COUNT; loop_i++) {
		this_client->addr =
		    (this_client->addr & I2C_MASK_FLAG) | I2C_WR_FLAG;
		if (i2c_master_send
		    (this_client, (const char *)rxData,
		     ((length << 0X08) | 0X01))) {
			break;
		}
		mdelay(20);
	}

	if (loop_i >= S62X_RETRY_COUNT) {
		SSMERR("%s retry over %d\n", __func__, S62X_RETRY_COUNT);
		return -EIO;
	}
#if DEBUG
	if (atomic_read(&data->trace) & SS_I2C_DEBUG) {
		SSMDBG("RxData len=%02x addr=%02x\n data=",
		       length, addr);
		for (i = 0; i < length; i++)
			SSMDBG(" %02x", rxData[i]);
		SSMDBG("\n");
	}
#endif
	return 0;
}

static int I2C_TxData(char *txData, int length)
{
	uint8_t loop_i;

#if DEBUG
	int i;
	struct i2c_client *client = this_client;
	struct s62x_i2c_data *data = i2c_get_clientdata(client);
#endif

	/* Caller should check parameter validity. */
	if ((txData == NULL) || (length < 2))
		return -EINVAL;
	this_client->addr = this_client->addr & I2C_MASK_FLAG;
	for (loop_i = 0; loop_i < S62X_RETRY_COUNT; loop_i++) {
		if (i2c_master_send(this_client, (const char *)txData, length) >
		    0) {
			break;
		}
		mdelay(10);
	}

	if (loop_i >= S62X_RETRY_COUNT) {
		SSMERR("%s retry over %d\n", __func__, S62X_RETRY_COUNT);
		return -EIO;
	}
#if DEBUG
	if (atomic_read(&data->trace) & SS_I2C_DEBUG) {
		SSMDBG("TxData len=%02x addr=%02x\n data=",
		       length, txData[0]);
		for (i = 0; i < (length - 1); i++)
			SSMDBG(" %02x", txData[i + 1]);
		SSMDBG("\n");
	}
#endif
	return 0;
}

static int I2C_TxData2(unsigned char c1, unsigned char c2)
{
	unsigned char data[2];

	data[0] = c1;
	data[1] = c2;

	return I2C_TxData(data, 2);
}

static int ECS_InitDevice(void)
{
	char *err_desc = NULL;

	if (I2C_TxData2(S62X_IDX_BIAS, 0x00) < 0) {
		err_desc = "BIAS";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_VBGSEL, 0x70) < 0) {
		err_desc = "VBGSEL";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_OSC_TRIM, 0x00) < 0) {
		err_desc = "OSC_TRIM";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_ECO1, ECO1_DEFAULT) < 0) {
		err_desc = "ECO1";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_ECO2, 0x04) < 0) {
		err_desc = "ECO2";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_CHOP, 0x05) < 0) {
		err_desc = "CHOP";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_LDO_SEL, 0x13) < 0) {
		err_desc = "LDO_SEL";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_PW_VALUE, 0x5f) < 0) {
		err_desc = "PW_VALUE";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_SET_RESET_CON, 0x80) < 0) {
		err_desc = "SET_RESET_CON";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_DATA_POL, 0x00) < 0) {
		err_desc = "DATA_POL";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_ADC_RDY_CNT, 0x03) < 0) {
		err_desc = "ADC_RDY_CNT";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_W2, 0x20) < 0) {
		err_desc = "W2";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_PROBE_OE, 0x20 | PROBE_OE_WATCHDOG) < 0) {
		err_desc = "PROBE_OE";
		goto end_of_func;
	}

end_of_func:

	if (err_desc) {
		SSMERR("S62X_IDX_%s failed\n", err_desc);
		return -EFAULT;
	}
#if DEBUG
	{
		struct i2c_client *client = this_client;
		struct s62x_i2c_data *data = i2c_get_clientdata(client);

		if ((atomic_read(&data->trace) & 0xF000) == 0x9000)
			return -EFAULT;
		atomic_set(&init_phase, 0);
	}
#endif
	return 0;
}

static int ECS_SetMode_Off(void)
{
	char *err_desc = NULL;

	if (I2C_TxData2(S62X_IDX_MODE, 0x00) < 0) {
		err_desc = "MODE";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_ECO2, 0x04) < 0) {
		err_desc = "ECO2";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_I2CDIS, I2CDIS_CIC) < 0) {
		err_desc = "ECO2";
		goto end_of_func;
	}

end_of_func:

	if (err_desc) {
		SSMERR("S62X_IDX_%s failed\n", err_desc);
		return -EFAULT;
	}

	return 0;
}

static int ECS_SetMode_Measure(void)
{
	char *err_desc = NULL;

	if (I2C_TxData2(S62X_IDX_I2CDIS, I2CDIS_ADC) < 0) {
		err_desc = "I2CDIS";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_ECO2, 0x04 | ECO2_MRAS_NO_RST) < 0) {
		err_desc = "ECO2";
		goto end_of_func;
	}

	if (I2C_TxData2(S62X_IDX_MODE, MODE_TRIGGER) < 0) {
		err_desc = "MODE";
		goto end_of_func;
	}

end_of_func:

	if (err_desc) {
		SSMERR("S62X_IDX_%s failed\n", err_desc);
		return -EFAULT;
	}

	return 0;
}

static int ECS_SetMode(char mode)
{
	int ret;

	switch (mode) {

	case SS_SENSOR_MODE_OFF:
		ret = ECS_SetMode_Off();
		break;

	case SS_SENSOR_MODE_MEASURE:
		ret = ECS_SetMode_Measure();
		break;

	default:
		SSMDBG("%s: Unknown mode(%d)", __func__, mode);
		return -EINVAL;
	}

	return ret;
}

static int ECS_CheckDevice(void)
{
	char id1, id2;
	int err;

	id1 = S62X_IDX_DEVICE_ID;
	err = I2C_RxData(&id1, 1);
	if (err < 0)
		return err;

	id2 = S62X_IDX_DEVICE_INFO;
	err = I2C_RxData(&id2, 1);
	if (err < 0)
		return err;
#if DEBUG
	{
		struct i2c_client *client = this_client;
		struct s62x_i2c_data *data = i2c_get_clientdata(client);

		if ((atomic_read(&data->trace) & 0xF000) == 0x8000)
			id1 = 0x00, id2 = 0x01;
		else
			atomic_set(&init_phase, 1);
	}
#endif
	if (id1 != DEVICE_ID_VALUE || id2 != DEVICE_INFO_VALUE) {
		SSMERR("incorrect id %02X:%02X\n", id1, id2);
		return -EFAULT;
	}

	return 0;
}

static int ECS_SaveData(short *buf)
{
#if DEBUG
	struct i2c_client *client = this_client;
	struct s62x_i2c_data *data = i2c_get_clientdata(client);
#endif

	mutex_lock(&sensor_data_mutex);
	memcpy(sensor_data, buf, sizeof(sensor_data));
	mutex_unlock(&sensor_data_mutex);

#if DEBUG
	if (atomic_read(&data->trace) & SS_HWM_DEBUG) {
		SSMDBG
			("Get daemon data: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d!",
			sensor_data[0], sensor_data[1], sensor_data[2],
			sensor_data[3], sensor_data[4], sensor_data[5],
			sensor_data[6], sensor_data[7], sensor_data[8],
			sensor_data[9], sensor_data[10], sensor_data[11]);
	}
#endif
	return 0;
}

#define V(c1, c2) (int)(((((unsigned)(c1))<<8)|((unsigned)(c2)))&0x0fff)

static int ECS_GetData(short *mag)
{
	int loop_i, ret;
	struct i2c_client *client = this_client;
	struct s62x_i2c_data *data = i2c_get_clientdata(client);
	short v;
	char buf[6];

	for (loop_i = 0; loop_i < S62X_RETRY_COUNT; loop_i++) {
		buf[0] = S62X_IDX_STA1;
		if (I2C_RxData(buf, 1)) {
			SSMERR("S62X_IDX_STA1 %s fail\n", __func__);
			return -1;
		}

		if (buf[0] == STA1_DRDY)
			break;
		msleep(20);
	}

	if (loop_i >= S62X_RETRY_COUNT) {
		SSMERR("%s retry over\n", __func__);
		return -1;
	}

	buf[0] = S62X_IDX_X_LSB;
	ret = I2C_RxData(buf, sizeof(buf));
	if (ret < 0) {
		SSMERR("S62X_IDX_X_LSB %s fail\n", __func__);
		return -1;
	}

	v = V(buf[1], buf[0]), mag[data->cvt.map[0]] =
	    (data->cvt.sign[0] > 0) ? v : (4095 - v);
	v = V(buf[5], buf[4]), mag[data->cvt.map[1]] =
	    (data->cvt.sign[1] > 0) ? v : (4095 - v);
	v = V(buf[3], buf[2]), mag[data->cvt.map[2]] =
	    (data->cvt.sign[2] > 0) ? v : (4095 - v);

	/* for debug only */
	mutex_lock(&last_m_data_mutex);
	memcpy(last_m_data, mag, sizeof(last_m_data));
	mutex_unlock(&last_m_data_mutex);

#if DEBUG
	if (atomic_read(&data->trace) & SS_DATA_DEBUG)
		SSMDBG("Get device data: (%d,%d,%d)", mag[0], mag[1], mag[2]);
#endif

	return 0;
}

static int ECS_GetOpenStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0));
	return atomic_read(&open_flag);
}

static int ECS_GetCloseStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) <= 0));
	return atomic_read(&open_flag);
}

/*----------------------------------------------------------------------------*/
static int ECS_ReadChipInfo(char *buf, int bufsize)
{
	static const char * const supported_chip[] = { "S628", "S628A", "S625A" };
	int phase = atomic_read(&init_phase);
	struct i2c_client *client = this_client;
	struct s62x_i2c_data *data = i2c_get_clientdata(client);
	unsigned chip = L2CHIP(atomic_read(&data->layout));
	int ret;

	if (!buf)
		return -1;

	if (!this_client) {
		*buf = 0;
		return -2;
	}

	if (phase != 0) {
		*buf = 0;
		return -3;
	}

	if (chip < sizeof(supported_chip) / sizeof(char *))
		ret = sprintf(buf, supported_chip[chip]);
	else
		ret = sprintf(buf, "?");

	return ret;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_daemon_value(struct device_driver *ddri, char *buf)
{
	char strbuf[S62X_BUFSIZE];

	sprintf(strbuf, "s62xd");
	return sprintf(buf, "%s", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[S62X_BUFSIZE];

	ECS_ReadChipInfo(strbuf, S62X_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	short mdata[SENSOR_DATA_COUNT];

	mutex_lock(&last_m_data_mutex);
	memcpy(mdata, last_m_data, sizeof(mdata));
	mutex_unlock(&last_m_data_mutex);

	return sprintf(buf, "%d %d %d\n", mdata[0], mdata[1], mdata[2]);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_posturedata_value(struct device_driver *ddri, char *buf)
{
	short tmp[3];

	tmp[0] = sensor_data[0] * CONVERT_O / CONVERT_O_DIV;
	tmp[1] = sensor_data[1] * CONVERT_O / CONVERT_O_DIV;
	tmp[2] = sensor_data[2] * CONVERT_O / CONVERT_O_DIV;

	return sprintf(buf, "%d, %d, %d\n", tmp[0], tmp[1], tmp[2]);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = this_client;
	struct s62x_i2c_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		       data->hw->direction, atomic_read(&data->layout),
		       data->cvt.sign[0], data->cvt.sign[1], data->cvt.sign[2],
		       data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);
}

/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, const char *buf,
				  size_t count)
{
	struct i2c_client *client = this_client;
	struct s62x_i2c_data *data = i2c_get_clientdata(client);
	int layout = 0;
	int ret;

	ret = kstrtoint(buf, 0, &layout);
	if (ret < 0)
		SSMERR("input err\n");
	if (layout == 1) {
		atomic_set(&data->layout, layout);
		if (!hwmsen_get_convert(L2CVTI(layout), &data->cvt)) {
			SSMERR("HWMSEN_GET_CONVERT function error!\n");
		} else
		    if (!hwmsen_get_convert
			(L2CVTI(data->hw->direction), &data->cvt)) {
			SSMERR("invalid layout: %d, restore to %d\n", layout,
			       data->hw->direction);
		} else {
			SSMERR("invalid layout: (%d, %d)\n", layout,
			       data->hw->direction);
			hwmsen_get_convert(0, &data->cvt);
		}
	} else {
		SSMERR("invalid format = '%s'\n", buf);
	}

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = this_client;
	struct s62x_i2c_data *data = i2c_get_clientdata(client);
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "VERS: %s (%s)\n",
		     DRIVER_VERSION, ssmd_status);

	if (data->hw) {
		len +=
		    snprintf(buf + len, PAGE_SIZE - len,
			     "CUST: %d %d (%d %d)\n", data->hw->i2c_num,
			     data->hw->direction, data->hw->power_id,
			     data->hw->power_vol);
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	}

	switch (atomic_read(&init_phase)) {

	case 2:
		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "IDCK: %d\n",
			     ECS_CheckDevice());
		break;

	case 1:
		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "INIT: %d\n",
			     ECS_InitDevice());
		break;

	case 0:
		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "OPEN: %d (%d/%d)\n",
			     atomic_read(&dev_open_count),
			     atomic_read(&m_get_data),
			     atomic_read(&o_get_data));
		break;
	}

	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct s62x_i2c_data *obj = i2c_get_clientdata(this_client);

	if (NULL == obj) {
		SSMERR("data is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf,
				 size_t count)
{
	struct s62x_i2c_data *obj = i2c_get_clientdata(this_client);
	int trace;

	if (NULL == obj) {
		SSMERR("data is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace))
		atomic_set(&obj->trace, trace);
	else
		SSMERR("invalid content: '%s', length = %d\n", buf, count);

	return count;
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(daemon, S_IRUGO, show_daemon_value, NULL);
static DRIVER_ATTR(chipinfo, S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(posturedata, S_IRUGO, show_posturedata_value, NULL);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(layout, S_IRUGO | S_IWUSR, show_layout_value,
		   store_layout_value);
static DRIVER_ATTR(trace, S_IRUGO | S_IWUSR, show_trace_value,
		   store_trace_value);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *s62x_attr_list[] = {
	&driver_attr_daemon,
	&driver_attr_chipinfo,
	&driver_attr_sensordata,
	&driver_attr_posturedata,
	&driver_attr_status,
	&driver_attr_layout,
	&driver_attr_trace,
};

/*----------------------------------------------------------------------------*/
static int s62x_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(s62x_attr_list) / sizeof(s62x_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, s62x_attr_list[idx]);
		if (err != 0) {
			SSMERR("driver_create_file (%s) = %d\n",
			       s62x_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int s62x_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(s62x_attr_list) / sizeof(s62x_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, s62x_attr_list[idx]);

	return err;
}

/*----------------------------------------------------------------------------*/
static int s62x_open(struct inode *inode, struct file *file)
{
	int ret = -1;
#if DEBUG
	struct s62x_i2c_data *obj = i2c_get_clientdata(this_client);

	if (atomic_read(&obj->trace) & SS_CTR_DEBUG)
		SSMDBG("open device node");
#endif

	atomic_inc(&dev_open_count);
	ret = nonseekable_open(inode, file);

	return ret;
}

/*----------------------------------------------------------------------------*/
static int s62x_release(struct inode *inode, struct file *file)
{
#if DEBUG
	struct s62x_i2c_data *obj = i2c_get_clientdata(this_client);

	if (atomic_read(&obj->trace) & SS_CTR_DEBUG)
		SSMDBG("release device node");
#endif
	atomic_dec(&dev_open_count);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int factory_mode(void)
{
	/* for factory mode (without open from the Daemon and successfully initialized) */
	return (atomic_read(&dev_open_count) == 1
		&& atomic_read(&init_phase) == 0);
}

/*----------------------------------------------------------------------------*/
static long s62x_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	/* NOTE: In this function the size of "char" should be 1-byte. */
	short mdata[SENSOR_DATA_COUNT];
	char rwbuf[RWBUF_SIZE];
	char buff[S62X_BUFSIZE];
	char mode;
	short value[12];
	uint32_t enable;
	struct hwm_sensor_data *hwm_data;
	short delay;
	int status;
	int ret = -1;

	switch (cmd) {
	case ECS_IOCTL_WRITE:
		if (argp == NULL) {
			SSMDBG("invalid argument.");
			return -EINVAL;
		}
		if (copy_from_user(rwbuf, argp, sizeof(rwbuf))) {
			SSMDBG("copy_from_user failed.");
			return -EFAULT;
		}

		if (rwbuf[0] == 0) {
			memset(ssmd_status, 0, sizeof(ssmd_status));
			strncpy(ssmd_status, &rwbuf[1],
				sizeof(ssmd_status) - 1);
			break;
		}

		if ((rwbuf[0] < 2) || (rwbuf[0] > (RWBUF_SIZE - 1))) {
			SSMDBG("invalid argument.");
			return -EINVAL;
		}
		ret = I2C_TxData(&rwbuf[1], rwbuf[0]);
		if (ret < 0)
			return ret;
		break;

	case ECS_IOCTL_READ:
		if (argp == NULL) {
			SSMDBG("invalid argument.");
			return -EINVAL;
		}

		if (copy_from_user(rwbuf, argp, sizeof(rwbuf))) {
			SSMDBG("copy_from_user failed.");
			return -EFAULT;
		}

		if ((rwbuf[0] < 1) || (rwbuf[0] > (RWBUF_SIZE - 1))) {
			SSMDBG("invalid argument.");
			return -EINVAL;
		}
		ret = I2C_RxData(&rwbuf[1], rwbuf[0]);
		if (ret < 0)
			return ret;
		if (copy_to_user(argp, rwbuf, rwbuf[0] + 1)) {
			SSMDBG("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECS_IOCTL_SET_MODE:
		if (argp == NULL) {
			SSMDBG("invalid argument.");
			return -EINVAL;
		}
		if (copy_from_user(&mode, argp, sizeof(mode))) {
			SSMDBG("copy_from_user failed.");
			return -EFAULT;
		}
		ret = ECS_SetMode(mode);
		if (ret < 0)
			return ret;
		break;

	case MSENSOR_IOCTL_SET_MODE:
		SSMDBG("arg = %d\n", (char)arg);
		ret = ECS_SetMode((char)arg);
		if (ret < 0)
			return ret;
		break;

	case ECS_IOCTL_GETDATA:
		ret = ECS_GetData(mdata);
		if (ret < 0)
			return ret;
		if (copy_to_user(argp, mdata, sizeof(mdata))) {
			SSMDBG("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECS_IOCTL_SET_YPR:
		if (argp == NULL) {
			SSMDBG("invalid argument.");
			return -EINVAL;
		}
		if (copy_from_user(value, argp, sizeof(value))) {
			SSMDBG("copy_from_user failed.");
			return -EFAULT;
		}
		ECS_SaveData(value);
		break;

	case ECS_IOCTL_GET_OPEN_STATUS:
		status = ECS_GetOpenStatus();
		SSMDBG("ECS_GetOpenStatus returned (%d)", status);
		if (copy_to_user(argp, &status, sizeof(status))) {
			SSMDBG("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECS_IOCTL_GET_CLOSE_STATUS:
		status = ECS_GetCloseStatus();
		SSMDBG("ECS_GetCloseStatus returned (%d)", status);
		if (copy_to_user(argp, &status, sizeof(status))) {
			SSMDBG("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECS_IOCTL_GET_DELAY:
		delay = ssmd_delay;
		if (copy_to_user(argp, &delay, sizeof(delay))) {
			SSMDBG("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECS_IOCTL_GET_PROJECT_NAME:
		if (argp == NULL) {
			SSMERR("IO parameter pointer is NULL!\n");
			break;
		}

		sprintf(buff, PROJECT_ID);
		status =
		    ECS_ReadChipInfo(buff + sizeof(PROJECT_ID),
				     S62X_BUFSIZE - sizeof(PROJECT_ID));
		status =
		    status <
		    0 ? sizeof(PROJECT_ID) : sizeof(PROJECT_ID) + status + 1;
		if (copy_to_user(argp, buff, status))
			return -EFAULT;
		break;

	case MSENSOR_IOCTL_READ_CHIPINFO:
		if (argp == NULL) {
			SSMERR("IO parameter pointer is NULL!\n");
			break;
		}

		ECS_ReadChipInfo(buff, S62X_BUFSIZE);
		if (copy_to_user(argp, buff, strlen(buff) + 1))
			return -EFAULT;
		break;

	case MSENSOR_IOCTL_READ_SENSORDATA:
		if (argp == NULL) {
			SSMERR("IO parameter pointer is NULL!\n");
			break;
		}

		if (factory_mode()) {
			ret = ECS_GetData(mdata);
			if (ret < 0)
				return -EFAULT;
		}

		mutex_lock(&last_m_data_mutex);
		memcpy(mdata, last_m_data, sizeof(mdata));
		mutex_unlock(&last_m_data_mutex);
		mdata[0] = mdata[0] * S62X_MUL_X / S62X_DIV_X;
		mdata[1] = mdata[1] * S62X_MUL_Y / S62X_DIV_Y;
		mdata[2] = mdata[2] * S62X_MUL_Z / S62X_DIV_Z;
		sprintf(buff, "%x %x %x", (int)mdata[0], (int)mdata[1],
			(int)mdata[2]);
		if (copy_to_user(argp, buff, strlen(buff) + 1))
			return -EFAULT;
		break;

	case MSENSOR_IOCTL_SENSOR_ENABLE:
		if (argp == NULL) {
			SSMERR("IO parameter pointer is NULL!\n");
			break;
		}

		if (copy_from_user(&enable, argp, sizeof(enable))) {
			SSMDBG("copy_from_user failed.");
			return -EFAULT;
		}
		SSMDBG("MSENSOR_IOCTL_SENSOR_ENABLE enable=%d", enable);
		if (enable == 1) {
			atomic_set(&o_flag, 1);
			atomic_set(&open_flag, 1);
		} else {
			atomic_set(&o_get_data, 0);
			atomic_set(&o_flag, 0);
			if (atomic_read(&m_flag) == 0)
				atomic_set(&open_flag, 0);
		}
		wake_up(&open_wq);

		if (factory_mode()) {
			int mode =
			    enable ? SS_SENSOR_MODE_MEASURE :
			    SS_SENSOR_MODE_OFF;

			SSMDBG
			    ("MSENSOR_IOCTL_SENSOR_ENABLE SetMode(%d)\n",
			     mode);
			ECS_SetMode(mode);
		}
		break;

	case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
		if (argp == NULL) {
			SSMERR("IO parameter pointer is NULL!\n");
			break;
		}

		hwm_data = (struct hwm_sensor_data *) buff;
		mutex_lock(&sensor_data_mutex);
		hwm_data->values[0] = sensor_data[0] * CONVERT_O;
		hwm_data->values[1] = sensor_data[1] * CONVERT_O;
		hwm_data->values[2] = sensor_data[2] * CONVERT_O;
		hwm_data->status = sensor_data[4];
		hwm_data->value_divide = CONVERT_O_DIV;
		mutex_unlock(&sensor_data_mutex);

		sprintf(buff, "%x %x %x %x %x", hwm_data->values[0],
			hwm_data->values[1], hwm_data->values[2],
			hwm_data->status, hwm_data->value_divide);
		if (copy_to_user(argp, buff, strlen(buff) + 1))
			return -EFAULT;
		break;

	default:
		SSMERR("%s not supported = 0x%x\n", __func__, cmd);
		return -ENOIOCTLCMD;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static const struct file_operations s62x_fops = {
	.owner = THIS_MODULE,
	.open = s62x_open,
	.release = s62x_release,
	.unlocked_ioctl = s62x_ioctl,
};

/*----------------------------------------------------------------------------*/
static struct miscdevice s62x_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msensor",
	.fops = &s62x_fops,
};

/*----------------------------------------------------------------------------*/
int s62x_operate(void *self, uint32_t command, void *buff_in, int size_in,
		 void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *msensor_data;

#if DEBUG
	struct i2c_client *client = this_client;
	struct s62x_i2c_data *data = i2c_get_clientdata(client);
#endif

#if DEBUG
	if (atomic_read(&data->trace) & SS_FUN_DEBUG)
		SSMFUNC("s62x_operate");
#endif

	switch (command) {

	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			SSMERR("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (value <= 20)
				ssmd_delay = 20;
			ssmd_delay = value;
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			SSMERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {

			value = *(int *)buff_in;

			if (value == 1) {
				atomic_set(&m_flag, 1);
				atomic_set(&open_flag, 1);
			} else {
				atomic_set(&m_get_data, 0);
				atomic_set(&m_flag, 0);
				if (atomic_read(&o_flag) == 0)
					atomic_set(&open_flag, 0);
			}
			wake_up(&open_wq);
		}
		break;

	case SENSOR_GET_DATA:
		atomic_inc(&m_get_data);
		if ((buff_out == NULL) || (size_out < sizeof(struct hwm_sensor_data))) {
			SSMERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			msensor_data = (struct hwm_sensor_data *) buff_out;
			mutex_lock(&sensor_data_mutex);

			msensor_data->values[0] = sensor_data[9] * CONVERT_M;
			msensor_data->values[1] = sensor_data[10] * CONVERT_M;
			msensor_data->values[2] = sensor_data[11] * CONVERT_M;
			msensor_data->status = sensor_data[4];
			msensor_data->value_divide = CONVERT_M_DIV;

			mutex_unlock(&sensor_data_mutex);
		#if DEBUG
			if (atomic_read(&data->trace) & SS_HWM_DEBUG) {
					SSMDBG
					    ("Hwm get m-sensor data: %d, %d, %d. divide %d, status %d!",
					     msensor_data->values[0],
					     msensor_data->values[1],
					     msensor_data->values[2],
					     msensor_data->value_divide,
					     msensor_data->status);
			}
		#endif
		}
		break;

	default:
		SSMERR("msensor operate function no this parameter %d!\n",
		       command);
		err = -1;
		break;
	}

	return err;
}

/*----------------------------------------------------------------------------*/
int s62x_orientation_operate(void *self, uint32_t command, void *buff_in,
			     int size_in, void *buff_out, int size_out,
			     int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *osensor_data;
#if DEBUG
	struct i2c_client *client = this_client;
	struct s62x_i2c_data *data = i2c_get_clientdata(client);
#endif

#if DEBUG
	if (atomic_read(&data->trace) & SS_FUN_DEBUG)
		SSMFUNC("s62x_orientation_operate");
#endif

	switch (command) {

	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			SSMERR("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (value <= 20)
				ssmd_delay = 20;
			ssmd_delay = value;
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			SSMERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {

			value = *(int *)buff_in;

			if (value == 1) {
				atomic_set(&o_flag, 1);
				atomic_set(&open_flag, 1);
			} else {
				atomic_set(&o_get_data, 0);
				atomic_set(&o_flag, 0);
				if (atomic_read(&m_flag) == 0)
					atomic_set(&open_flag, 0);
			}
			wake_up(&open_wq);
		}
		break;

	case SENSOR_GET_DATA:
		atomic_inc(&o_get_data);
		if ((buff_out == NULL) || (size_out < sizeof(struct hwm_sensor_data))) {
			SSMERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			osensor_data = (struct hwm_sensor_data *) buff_out;
			mutex_lock(&sensor_data_mutex);

			osensor_data->values[0] = sensor_data[0] * CONVERT_O;
			osensor_data->values[1] = sensor_data[1] * CONVERT_O;
			osensor_data->values[2] = sensor_data[2] * CONVERT_O;
			osensor_data->status = sensor_data[4];
			osensor_data->value_divide = CONVERT_O_DIV;

			mutex_unlock(&sensor_data_mutex);
		#if DEBUG
			if (atomic_read(&data->trace) & SS_HWM_DEBUG) {
				SSMDBG
				    ("Hwm get o-sensor data: %d, %d, %d. divide %d, status %d!",
				     osensor_data->values[0],
				     osensor_data->values[1],
				     osensor_data->values[2],
				     osensor_data->value_divide,
				     osensor_data->status);
			}
		#endif
		}
		break;
	default:
		SSMERR("gsensor operate function no this parameter %d!\n",
		       command);
		err = -1;
		break;
	}

	return err;
}

/*----------------------------------------------------------------------------*/
static int s62x_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct s62x_i2c_data *obj = i2c_get_clientdata(client);

	if (msg.event == PM_EVENT_SUSPEND)
		s62x_power(obj->hw, 0);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int s62x_resume(struct i2c_client *client)
{
	struct s62x_i2c_data *obj = i2c_get_clientdata(client);

	s62x_power(obj->hw, 1);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int s62x_i2c_detect(struct i2c_client *client,
			   struct i2c_board_info *info)
{
	strcpy(info->type, S62X_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
#ifdef _MAGIC_KPT_COMMAND
static int magic_kpt_command(const struct i2c_client *client)
{
	char cmd1[] = { 0x0A, 0x00 };
	char cmd2[] = { 0x0F, 0x1B };
	struct i2c_msg msg[] = {
		{
			.addr = S62X_I2C_ADDR1,
			.flags = 0,
			.len = sizeof(cmd1),
			.buf = cmd1,
			}
		,
		{
			.addr = S62X_I2C_ADDR1,
			.flags = 0,
			.len = sizeof(cmd2),
			.buf = cmd2,
		}
		,
	};

	return i2c_transfer(client->adapter, msg, 2);
}
#endif

/*----------------------------------------------------------------------------*/
#ifdef USE_ALTERNATE_ADDRESS
static int s62x_change_address(const struct i2c_client *client)
{
	uint8_t loop_i;
	char cmd[] = { S62X_IDX_ECO1, ECO1_DEFAULT };
	struct i2c_msg msg[] = {
		{
			.addr = S62X_I2C_ADDR1,
			.flags = 0,
			.len = sizeof(cmd),
			.buf = cmd,
			}
		,
	};

	for (loop_i = 0; loop_i < 3; loop_i++) {
		if (i2c_transfer(client->adapter, msg, 1) > 0)
			return 0;
		mdelay(10);
	}

	/* SSMERR("change address retry over %d\n", loop_i); */
	return -EIO;
}
#endif
static int s62x_m_enable(int en)
{
	int value = 0;
	int err = 0;

	value = en;
	if (value == 1) {
		atomic_set(&m_flag, 1);
		atomic_set(&open_flag, 1);

		err = ECS_SetMode(SS_SENSOR_MODE_MEASURE);
		if (err < 0) {
			SSMDBG("%s:ECS_SetMode on Error.\n", __func__);
			return err;
		}
	} else {
		atomic_set(&m_flag, 0);
		if (atomic_read(&o_flag) == 0) {
			atomic_set(&open_flag, 0);
			err = ECS_SetMode(SS_SENSOR_MODE_OFF);
			if (err < 0) {
				SSMDBG("%s:ECS_SetMode off Error.\n", __func__);
				return err;
			}
		}
	}
	wake_up(&open_wq);
	return err;
}

static int s62x_m_set_delay(u64 ns)
{
	int value = 0;

	value = (int)ns/1000/1000;
	if (value <= 20)
		ssmd_delay = 20;
	else
		ssmd_delay = value;

	return 0;
}
static int s62x_m_open_report_data(int open)
{
	return 0;
}

static int s62x_m_get_data(int *x , int *y, int *z, int *status)
{
	mutex_lock(&sensor_data_mutex);
	*x = sensor_data[9] * CONVERT_M;
	*y = sensor_data[10] * CONVERT_M;
	*z = sensor_data[11] * CONVERT_M;
	*status = sensor_data[4];
	mutex_unlock(&sensor_data_mutex);
	return 0;
}


static int s62x_o_enable(int en)
{
	int value = 0;
	int err = 0;

	value = en;
	if (value == 1) {
		atomic_set(&o_flag, 1);
		atomic_set(&open_flag, 1);
		err = ECS_SetMode(SS_SENSOR_MODE_MEASURE);
		if (err < 0) {
			SSMERR("%s:ECS_SetMode on Error.\n", __func__);
			return err;
		}
	} else {
		atomic_set(&o_get_data, 0);
		atomic_set(&o_flag, 0);
		if (atomic_read(&m_flag) == 0)
			atomic_set(&open_flag, 0);
		err = ECS_SetMode(SS_SENSOR_MODE_OFF);
		if (err < 0) {
			SSMERR("%s:ECS_SetMode off Error.\n", __func__);
			return err;
		}
	}
	wake_up(&open_wq);

	return err;
}

static int s62x_o_set_delay(u64 ns)
{
	int value = 0;

	value = (int)ns/1000/1000;
	if (value <= 20)
		ssmd_delay = 20;
	else
		ssmd_delay = value;
	return 0;
}
static int s62x_o_open_report_data(int open)
{
	return 0;
}

static int s62x_o_get_data(int *x , int *y, int *z, int *status)
{
	mutex_lock(&sensor_data_mutex);

	*x = sensor_data[0] * CONVERT_O;
	*y = sensor_data[1] * CONVERT_O;
	*z = sensor_data[2] * CONVERT_O;
	*status = sensor_data[4];
	mutex_unlock(&sensor_data_mutex);
	return 0;
}


/*----------------------------------------------------------------------------*/
static int s62x_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct s62x_i2c_data *data;
	int err = 0;
	struct mag_control_path ctl = {0};
	struct mag_data_path mag_data = {0};

	data = kmalloc(sizeof(struct s62x_i2c_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	memset(data, 0, sizeof(struct s62x_i2c_data));
	data->hw = hw;

	if (hwmsen_get_convert(L2CVTI(data->hw->direction), &data->cvt)) {
		SSMERR("invalid direction: %d\n", data->hw->direction);
		goto exit_init_failed;
	}

	atomic_set(&data->layout, data->hw->direction);
	atomic_set(&data->trace, 0x0000);

	mutex_init(&last_m_data_mutex);
	mutex_init(&sensor_data_mutex);

	init_waitqueue_head(&open_wq);

	data->client = client;
	new_client = data->client;
	i2c_set_clientdata(new_client, data);

	this_client = new_client;

#ifdef _MAGIC_KPT_COMMAND
	magic_kpt_command(client);
#endif

#ifdef USE_ALTERNATE_ADDRESS
	err = s62x_change_address(client);
	SSMERR("address change %s\n", err == 0 ? "OK" : "NG");
#endif

#ifdef _FORCE_PROBE_ERROR
	SSMERR("force probe error\n");
	return -1;
#endif

	/* Check connection */
	if (ECS_CheckDevice() != 0) {
		SSMERR("check device connect error\n");
	} else {
		atomic_set(&init_phase, 1);
		if (ECS_InitDevice() != 0)
			SSMERR("init device error\n");
		else
			atomic_set(&init_phase, 0);
	}

	/* Register sysfs attribute */
	err = s62x_create_attr(&(s62x_init_info.platform_diver_addr->driver));
	if (err != 0) {
		SSMERR("create attribute err = %d\n", err);
		goto exit_sysfs_create_group_failed;
	}
#ifdef ENABLE_DUALSTACK_MODE
	if (atomic_read(&init_phase) == 2) {
		SSMERR("dual stack mode exit\n");
		goto exit_init_failed;
	}
#endif

	err = misc_register(&s62x_device);
	if (err != 0) {
		SSMERR("device register failed\n");
		goto exit_misc_device_register_failed;
	}

	ctl.is_use_common_factory = false;
	ctl.m_enable = s62x_m_enable;
	ctl.m_set_delay  = s62x_m_set_delay;
	ctl.m_open_report_data = s62x_m_open_report_data;
	ctl.o_enable = s62x_o_enable;
	ctl.o_set_delay  = s62x_o_set_delay;
	ctl.o_open_report_data = s62x_o_open_report_data;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = data->hw->is_batch_supported;

	err = mag_register_control_path(&ctl);
	if (err) {
		SSMDBG("register mag control path err\n");
		goto exit_kfree;
	}

	mag_data.div_m = CONVERT_M_DIV;
	mag_data.div_o = CONVERT_O_DIV;
	mag_data.get_data_o = s62x_o_get_data;
	mag_data.get_data_m = s62x_m_get_data;

	err = mag_register_data_path(&mag_data);
	if (err) {
		SSMDBG("register data control path err\n");
		goto exit_kfree;
	}

	SSMDBG("%s: %s", __func__, atomic_read(&init_phase) ? "NG" : "OK");
	return 0;

exit_sysfs_create_group_failed:
exit_init_failed:
exit_misc_device_register_failed:
exit_kfree:
	kfree(data);

exit:
	SSMERR("%s: err = %d\n", __func__, err);
	return err;
}

/*----------------------------------------------------------------------------*/
static int s62x_i2c_remove(struct i2c_client *client)
{
	int err;

	err = s62x_delete_attr(&(s62x_init_info.platform_diver_addr->driver));
	if (err != 0)
		SSMERR("delete_attr fail: %d\n", err);

	this_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	misc_deregister(&s62x_device);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int s62x_local_init(void)
{
	s62x_power(hw, 1);
	atomic_set(&dev_open_count, 0);
	if (i2c_add_driver(&s62x_i2c_driver)) {
		SSMERR("add driver error\n");
		return -1;
	}
	return 0;
}

/*----------------------------------------------------------------------------*/
static int s62x_local_remove(void)
{
	s62x_power(hw, 0);
	atomic_set(&dev_open_count, 0);
	i2c_del_driver(&s62x_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init s62x_init(void)
{
	const char *name = "mediatek,s62x";

	hw = get_mag_dts_func(name, hw);
	if (!hw)
		SSMERR("get dts info fail\n");

	mag_driver_add(&s62x_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit s62x_exit(void)
{
}

/*----------------------------------------------------------------------------*/
module_init(s62x_init);
module_exit(s62x_exit);

MODULE_DESCRIPTION("S62X Compass Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
