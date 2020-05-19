/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * (C) Modification Copyright 2018 Robert Bosch Kft  All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * Special: Description of the Software:
 *
 * This software module (hereinafter called "Software") and any
 * information on application-sheets (hereinafter called "Information") is
 * provided free of charge for the sole purpose to support your application
 * work. 
 *
 * As such, the Software is merely an experimental software, not tested for
 * safety in the field and only intended for inspiration for further development 
 * and testing. Any usage in a safety-relevant field of use (like automotive,
 * seafaring, spacefaring, industrial plants etc.) was not intended, so there are
 * no precautions for such usage incorporated in the Software.
 * 
 * The Software is specifically designed for the exclusive use for Bosch
 * Sensortec products by personnel who have special experience and training. Do
 * not use this Software if you do not have the proper experience or training.
 * 
 * This Software package is provided as is and without any expressed or
 * implied warranties, including without limitation, the implied warranties of
 * merchantability and fitness for a particular purpose.
 * 
 * Bosch Sensortec and their representatives and agents deny any liability for
 * the functional impairment of this Software in terms of fitness, performance
 * and safety. Bosch Sensortec and their representatives and agents shall not be
 * liable for any direct or indirect damages or injury, except as otherwise
 * stipulated in mandatory applicable law.
 * The Information provided is believed to be accurate and reliable. Bosch
 * Sensortec assumes no responsibility for the consequences of use of such
 * Information nor for any infringement of patents or other rights of third
 * parties which may result from its use.
 * 
 *------------------------------------------------------------------------------
 * The following Product Disclaimer does not apply to the BSX4-HAL-4.1NoFusion Software 
 * which is licensed under the Apache License, Version 2.0 as stated above.  
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Product Disclaimer
 *
 * Common:
 *
 * Assessment of Products Returned from Field
 *
 * Returned products are considered good if they fulfill the specifications / 
 * test data for 0-mileage and field listed in this document.
 *
 * Engineering Samples
 * 
 * Engineering samples are marked with (e) or (E). Samples may vary from the
 * valid technical specifications of the series product contained in this
 * data sheet. Therefore, they are not intended or fit for resale to
 * third parties or for use in end products. Their sole purpose is internal
 * client testing. The testing of an engineering sample may in no way replace
 * the testing of a series product. Bosch assumes no liability for the use
 * of engineering samples. The purchaser shall indemnify Bosch from all claims
 * arising from the use of engineering samples.
 *
 * Intended use
 *
 * Provided that SMI130 is used within the conditions (environment, application,
 * installation, loads) as described in this TCD and the corresponding
 * agreed upon documents, Bosch ensures that the product complies with
 * the agreed properties. Agreements beyond this require
 * the written approval by Bosch. The product is considered fit for the intended
 * use when the product successfully has passed the tests
 * in accordance with the TCD and agreed upon documents.
 *
 * It is the responsibility of the customer to ensure the proper application
 * of the product in the overall system/vehicle.
 *
 * Bosch does not assume any responsibility for changes to the environment
 * of the product that deviate from the TCD and the agreed upon documents 
 * as well as all applications not released by Bosch
  *
 * The resale and/or use of products are at the purchaser’s own risk and 
 * responsibility. The examination and testing of the SMI130 
 * is the sole responsibility of the purchaser.
 *
 * The purchaser shall indemnify Bosch from all third party claims 
 * arising from any product use not covered by the parameters of 
 * this product data sheet or not approved by Bosch and reimburse Bosch 
 * for all costs and damages in connection with such claims.
 *
 * The purchaser must monitor the market for the purchased products,
 * particularly with regard to product safety, and inform Bosch without delay
 * of all security relevant incidents.
 *
 * Application Examples and Hints
 *
 * With respect to any application examples, advice, normal values
 * and/or any information regarding the application of the device,
 * Bosch hereby disclaims any and all warranties and liabilities of any kind,
 * including without limitation warranties of
 * non-infringement of intellectual property rights or copyrights
 * of any third party.
 * The information given in this document shall in no event be regarded 
 * as a guarantee of conditions or characteristics. They are provided
 * for illustrative purposes only and no evaluation regarding infringement
 * of intellectual property rights or copyrights or regarding functionality,
 * performance or error has been made.
 * @filename smi130_gyro_driver.c
 * @date     2015/11/17 13:44
 * @Modification Date 2018/08/28 18:20
 * @id       "836294d"
 * @version  1.5.9
 *
 * @brief    SMI130_GYRO Linux Driver
 */
#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/string.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#endif
#include <linux/math64.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "smi130_gyro.h"
#include "bs_log.h"

/* sensor specific */
#define SENSOR_NAME "smi130_gyro"
#define SMI130_GYRO_ENABLE_INT1 1
#define SENSOR_CHIP_ID_SMI_GYRO (0x0f)
#define CHECK_CHIP_ID_TIME_MAX   1
#define DRIVER_VERSION "0.0.53.0"
#define SMI_GYRO_USE_FIFO          1
#define SMI_GYRO_USE_BASIC_I2C_FUNC     1
#define SMI_GYRO_REG_NAME(name) SMI130_GYRO_##name
#define SMI_GYRO_VAL_NAME(name) SMI130_GYRO_##name
#define SMI_GYRO_CALL_API(name) smi130_gyro_##name
#define MSC_TIME                6

#define SMI_GYRO_I2C_WRITE_DELAY_TIME 1

/* generic */
#define SMI_GYRO_MAX_RETRY_I2C_XFER (2)
#define SMI_GYRO_MAX_RETRY_WAKEUP (5)
#define SMI_GYRO_MAX_RETRY_WAIT_DRDY (100)

#define SMI_GYRO_DELAY_MIN (1)
#define SMI_GYRO_DELAY_DEFAULT (200)

#define SMI_GYRO_VALUE_MAX (32767)
#define SMI_GYRO_VALUE_MIN (-32768)

#define BYTES_PER_LINE (16)

#define SMI_GYRO_SELF_TEST 0

#define SMI_GYRO_SOFT_RESET_VALUE                0xB6

#ifdef SMI_GYRO_USE_FIFO
#define MAX_FIFO_F_LEVEL 100
#define MAX_FIFO_F_BYTES 8
#define SMI130_GYRO_FIFO_DAT_SEL_X                     1
#define SMI130_GYRO_FIFO_DAT_SEL_Y                     2
#define SMI130_GYRO_FIFO_DAT_SEL_Z                     3
#endif

/*!
 * @brief:BMI058 feature
 *  macro definition
*/
#ifdef CONFIG_SENSORS_BMI058
/*! BMI058 X AXIS definition*/
#define BMI058_X_AXIS	SMI130_GYRO_Y_AXIS
/*! BMI058 Y AXIS definition*/
#define BMI058_Y_AXIS	SMI130_GYRO_X_AXIS

#define C_BMI058_One_U8X	1
#define C_BMI058_Two_U8X	2
#endif

/*! Bosch sensor unknown place*/
#define BOSCH_SENSOR_PLACE_UNKNOWN (-1)
/*! Bosch sensor remapping table size P0~P7*/
#define MAX_AXIS_REMAP_TAB_SZ 8


struct bosch_sensor_specific {
	char *name;
	/* 0 to 7 */
	int place;
	int irq;
	int (*irq_gpio_cfg)(void);
};


/*!
 * we use a typedef to hide the detail,
 * because this type might be changed
 */
struct bosch_sensor_axis_remap {
	/* src means which source will be mapped to target x, y, z axis */
	/* if an target OS axis is remapped from (-)x,
	 * src is 0, sign_* is (-)1 */
	/* if an target OS axis is remapped from (-)y,
	 * src is 1, sign_* is (-)1 */
	/* if an target OS axis is remapped from (-)z,
	 * src is 2, sign_* is (-)1 */
	int src_x:3;
	int src_y:3;
	int src_z:3;

	int sign_x:2;
	int sign_y:2;
	int sign_z:2;
};


struct bosch_sensor_data {
	union {
		int16_t v[3];
		struct {
			int16_t x;
			int16_t y;
			int16_t z;
		};
	};
};

#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
#define SMI_GYRO_MAXSAMPLE       4000
#define G_MAX                    23920640
struct smi_gyro_sample {
	int xyz[3];
	unsigned int tsec;
	unsigned long long tnsec;
};
#endif

struct smi_gyro_client_data {
	struct smi130_gyro_t device;
	struct i2c_client *client;
	struct input_dev *input;
	struct delayed_work work;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend_handler;
#endif

	atomic_t delay;
	uint8_t debug_level;
	struct smi130_gyro_data_t value;
	u8 enable:1;
	unsigned int fifo_count;
	unsigned char fifo_datasel;
	uint64_t timestamp;
	uint64_t base_time;
	uint64_t fifo_time;
	uint64_t gyro_count;
	uint64_t time_odr;
	/* controls not only reg, but also workqueue */
	struct mutex mutex_op_mode;
	struct mutex mutex_enable;
	struct bosch_sensor_specific *bosch_pd;
	struct work_struct report_data_work;
	int is_timer_running;
	struct hrtimer timer;
	ktime_t work_delay_kt;
	uint8_t gpio_pin;
	int16_t IRQ;
#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
	bool read_gyro_boot_sample;
	int gyro_bufsample_cnt;
	bool gyro_buffer_smi130_samples;
	bool gyro_enable;
	struct kmem_cache *smi_gyro_cachepool;
	struct smi_gyro_sample *smi130_gyro_samplist[SMI_GYRO_MAXSAMPLE];
	int max_buffer_time;
	struct input_dev *gyrobuf_dev;
	int report_evt_cnt;
	struct mutex gyro_sensor_buff;
#endif
};

static struct i2c_client *smi_gyro_client;
/* i2c operation for API */
static int smi_gyro_i2c_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len);
static int smi_gyro_i2c_write(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len);

static void smi_gyro_dump_reg(struct i2c_client *client);
static int smi_gyro_check_chip_id(struct i2c_client *client);

#ifdef CONFIG_HAS_EARLYSUSPEND
static int smi_gyro_post_resume(struct i2c_client *client);
static int smi_gyro_pre_suspend(struct i2c_client *client);
static void smi_gyro_early_suspend(struct early_suspend *handler);
static void smi_gyro_late_resume(struct early_suspend *handler);
#endif

static void smi130_gyro_delay(SMI130_GYRO_U16 msec)
{
	if (msec <= 20)
		usleep_range(msec * 1000, msec * 1000);
	else
		msleep(msec);
}

/*!
* SMI130_GYRO sensor remapping function
* need to give some parameter in BSP files first.
*/
static const struct bosch_sensor_axis_remap
	bosch_axis_remap_tab_dft[MAX_AXIS_REMAP_TAB_SZ] = {
	/* src_x src_y src_z  sign_x  sign_y  sign_z */
	{  0,	 1,    2,	  1,	  1,	  1 }, /* P0 */
	{  1,	 0,    2,	  1,	 -1,	  1 }, /* P1 */
	{  0,	 1,    2,	 -1,	 -1,	  1 }, /* P2 */
	{  1,	 0,    2,	 -1,	  1,	  1 }, /* P3 */

	{  0,	 1,    2,	 -1,	  1,	 -1 }, /* P4 */
	{  1,	 0,    2,	 -1,	 -1,	 -1 }, /* P5 */
	{  0,	 1,    2,	  1,	 -1,	 -1 }, /* P6 */
	{  1,	 0,    2,	  1,	  1,	 -1 }, /* P7 */
};

static void bosch_remap_sensor_data(struct bosch_sensor_data *data,
			const struct bosch_sensor_axis_remap *remap)
{
	struct bosch_sensor_data tmp;

	tmp.x = data->v[remap->src_x] * remap->sign_x;
	tmp.y = data->v[remap->src_y] * remap->sign_y;
	tmp.z = data->v[remap->src_z] * remap->sign_z;

	memcpy(data, &tmp, sizeof(*data));
}

static void bosch_remap_sensor_data_dft_tab(struct bosch_sensor_data *data,
			int place)
{
/* sensor with place 0 needs not to be remapped */
	if ((place <= 0) || (place >= MAX_AXIS_REMAP_TAB_SZ))
		return;
	bosch_remap_sensor_data(data, &bosch_axis_remap_tab_dft[place]);
}

static void smi130_gyro_remap_sensor_data(struct smi130_gyro_data_t *val,
		struct smi_gyro_client_data *client_data)
{
	struct bosch_sensor_data bsd;
	int place;

	if ((NULL == client_data->bosch_pd) || (BOSCH_SENSOR_PLACE_UNKNOWN
			 == client_data->bosch_pd->place))
		place = BOSCH_SENSOR_PLACE_UNKNOWN;
	else
		place = client_data->bosch_pd->place;

#ifdef CONFIG_SENSORS_BMI058
/*x,y need to be invesed becase of HW Register for BMI058*/
	bsd.y = val->datax;
	bsd.x = val->datay;
	bsd.z = val->dataz;
#else
	bsd.x = val->datax;
	bsd.y = val->datay;
	bsd.z = val->dataz;
#endif

	bosch_remap_sensor_data_dft_tab(&bsd, place);

	val->datax = bsd.x;
	val->datay = bsd.y;
	val->dataz = bsd.z;

}

static int smi_gyro_check_chip_id(struct i2c_client *client)
{
	int err = -1;
	u8 chip_id = 0;
	u8 read_count = 0;

	while (read_count++ < CHECK_CHIP_ID_TIME_MAX) {
		smi_gyro_i2c_read(client, SMI_GYRO_REG_NAME(CHIP_ID_ADDR), &chip_id, 1);
		PINFO("read chip id result: %#x", chip_id);

		if ((chip_id & 0xff) != SENSOR_CHIP_ID_SMI_GYRO) {
			smi130_gyro_delay(1);
		} else {
			err = 0;
			break;
		}
	}
	return err;
}

static void smi_gyro_dump_reg(struct i2c_client *client)
{
	int i;
	u8 dbg_buf[64] = {0};
	u8 dbg_buf_str[64 * 3 + 1] = "";

	for (i = 0; i < BYTES_PER_LINE; i++) {
		dbg_buf[i] = i;
		snprintf(dbg_buf_str + i * 3, 16, "%02x%c",
				dbg_buf[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	dev_dbg(&client->dev, "%s\n", dbg_buf_str);

	smi_gyro_i2c_read(client, SMI_GYRO_REG_NAME(CHIP_ID_ADDR), dbg_buf, 64);
	for (i = 0; i < 64; i++) {
		snprintf(dbg_buf_str + i * 3, 16, "%02x%c",
				dbg_buf[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	dev_dbg(&client->dev, "%s\n", dbg_buf_str);
}

/*i2c read routine for API*/
static int smi_gyro_i2c_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
#if !defined SMI_GYRO_USE_BASIC_I2C_FUNC
	s32 dummy;
	if (NULL == client)
		return -ENODEV;

	while (0 != len--) {
#ifdef SMI_GYRO_SMBUS
		dummy = i2c_smbus_read_byte_data(client, reg_addr);
		if (dummy < 0) {
			dev_err(&client->dev, "i2c bus read error");
			return -EIO;
		}
		*data = (u8)(dummy & 0xff);
#else
		dummy = i2c_master_send(client, (char *)&reg_addr, 1);
		if (dummy < 0)
			return -EIO;

		dummy = i2c_master_recv(client, (char *)data, 1);
		if (dummy < 0)
			return -EIO;
#endif
		reg_addr++;
		data++;
	}
	return 0;
#else
	int retry;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg_addr,
		},

		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	for (retry = 0; retry < SMI_GYRO_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			smi130_gyro_delay(SMI_GYRO_I2C_WRITE_DELAY_TIME);
	}

	if (SMI_GYRO_MAX_RETRY_I2C_XFER <= retry) {
		dev_err(&client->dev, "I2C xfer error");
		return -EIO;
	}

	return 0;
#endif
}

#ifdef SMI_GYRO_USE_FIFO
static int smi_gyro_i2c_burst_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u16 len)
{
	int retry;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg_addr,
		},

		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	for (retry = 0; retry < SMI_GYRO_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			smi130_gyro_delay(SMI_GYRO_I2C_WRITE_DELAY_TIME);
	}

	if (SMI_GYRO_MAX_RETRY_I2C_XFER <= retry) {
		dev_err(&client->dev, "I2C xfer error");
		return -EIO;
	}

	return 0;
}
#endif

/*i2c write routine for */
static int smi_gyro_i2c_write(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
#if !defined SMI_GYRO_USE_BASIC_I2C_FUNC
	s32 dummy;

#ifndef SMI_GYRO_SMBUS
	u8 buffer[2];
#endif

	if (NULL == client)
		return -ENODEV;

	while (0 != len--) {
#ifdef SMI_GYRO_SMBUS
		dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
#else
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(client, (char *)buffer, 2);
#endif
		reg_addr++;
		data++;
		if (dummy < 0) {
			dev_err(&client->dev, "error writing i2c bus");
			return -EIO;
		}

	}
	return 0;
#else
	u8 buffer[2];
	int retry;
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 2,
		 .buf = buffer,
		 },
	};

	while (0 != len--) {
		buffer[0] = reg_addr;
		buffer[1] = *data;
		for (retry = 0; retry < SMI_GYRO_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0) {
				break;
			} else {
				smi130_gyro_delay(SMI_GYRO_I2C_WRITE_DELAY_TIME);
			}
		}
		if (SMI_GYRO_MAX_RETRY_I2C_XFER <= retry) {
			dev_err(&client->dev, "I2C xfer error");
			return -EIO;
		}
		reg_addr++;
		data++;
	}

	return 0;
#endif
}

static int smi_gyro_i2c_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	int err;
	err = smi_gyro_i2c_read(smi_gyro_client, reg_addr, data, len);
	return err;
}

static int smi_gyro_i2c_write_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	int err;
	err = smi_gyro_i2c_write(smi_gyro_client, reg_addr, data, len);
	return err;
}


static void smi_gyro_work_func(struct work_struct *work)
{
	struct smi_gyro_client_data *client_data =
		container_of((struct delayed_work *)work,
			struct smi_gyro_client_data, work);

	unsigned long delay =
		msecs_to_jiffies(atomic_read(&client_data->delay));
	struct smi130_gyro_data_t gyro_data;

	SMI_GYRO_CALL_API(get_dataXYZ)(&gyro_data);
	/*remapping for SMI130_GYRO sensor*/
	smi130_gyro_remap_sensor_data(&gyro_data, client_data);

	input_report_abs(client_data->input, ABS_X, gyro_data.datax);
	input_report_abs(client_data->input, ABS_Y, gyro_data.datay);
	input_report_abs(client_data->input, ABS_Z, gyro_data.dataz);
	input_sync(client_data->input);

	schedule_delayed_work(&client_data->work, delay);
}

static struct workqueue_struct *reportdata_wq;

uint64_t smi130_gyro_get_alarm_timestamp(void)
{
	uint64_t ts_ap;
	struct timespec tmp_time;
	get_monotonic_boottime(&tmp_time);
	ts_ap = (uint64_t)tmp_time.tv_sec * 1000000000 + tmp_time.tv_nsec;
	return ts_ap;
}
#define ABS(x) ((x) > 0 ? (x) : -(x))

static void smi130_gyro_work_func(struct work_struct *work)
{
	struct	smi_gyro_client_data *smi130_gyro =
		container_of(work,
				struct smi_gyro_client_data, report_data_work);
	int i;
	struct smi130_gyro_data_t gyro_lsb;
	unsigned char fifo_framecount;
	signed char fifo_data_out[MAX_FIFO_F_LEVEL * MAX_FIFO_F_BYTES] = {0};
	unsigned char f_len = 0;
	uint64_t del;
	uint64_t time_internal;
	struct timespec ts;
	int64_t drift_time = 0;
	static uint64_t time_odr;
	static uint32_t data_cnt;
	static uint32_t pre_data_cnt;
	static int64_t sample_drift_offset;
	if (smi130_gyro->fifo_datasel)
		/*Select one axis data output for every fifo frame*/
		f_len = 2;
	else
		/*Select X Y Z axis data output for every fifo frame*/
		f_len = 6;
	if (SMI_GYRO_CALL_API(get_fifo_framecount)(&fifo_framecount) < 0) {
		PERR("bm160_get_fifo_framecount err\n");
		return;
	}
	if (fifo_framecount == 0)
		return;
	if (fifo_framecount > MAX_FIFO_F_LEVEL)
			fifo_framecount = MAX_FIFO_F_LEVEL;
	if (smi_gyro_i2c_burst_read(smi130_gyro->client, SMI130_GYRO_FIFO_DATA_ADDR,
			fifo_data_out, fifo_framecount * f_len) < 0) {
			PERR("smi130_gyro read fifo err\n");
			return;
	}
	smi130_gyro->fifo_time = smi130_gyro_get_alarm_timestamp();
	if (smi130_gyro->gyro_count == 0)
		smi130_gyro->base_time = smi130_gyro->timestamp =
		smi130_gyro->fifo_time - (fifo_framecount-1) * smi130_gyro->time_odr;

	smi130_gyro->gyro_count += fifo_framecount;
	del = smi130_gyro->fifo_time - smi130_gyro->base_time;
	time_internal = div64_u64(del, smi130_gyro->gyro_count);
	data_cnt++;
	if (data_cnt == 1)
		time_odr = smi130_gyro->time_odr;
	if (time_internal > time_odr) {
		if (time_internal - time_odr > div64_u64 (time_odr, 200))
			time_internal = time_odr + div64_u64(time_odr, 200);
	} else {
		if (time_odr - time_internal > div64_u64(time_odr, 200))
			time_internal = time_odr - div64_u64(time_odr, 200);
	}

	/* Select X Y Z axis data output for every frame */
	for (i = 0; i < fifo_framecount; i++) {
		if (smi130_gyro->debug_level & 0x01)
			printk(KERN_INFO "smi_gyro time =%llu fifo_time = %llu time_internal = %llu smi_gyro->count= %llu count = %d",
		smi130_gyro->timestamp, smi130_gyro->fifo_time,
		time_internal, smi130_gyro->gyro_count, fifo_framecount);
		ts = ns_to_timespec(smi130_gyro->timestamp);
		gyro_lsb.datax =
		((unsigned char)fifo_data_out[i * f_len + 1] << 8
				| (unsigned char)fifo_data_out[i * f_len + 0]);
		gyro_lsb.datay =
		((unsigned char)fifo_data_out[i * f_len + 3] << 8
				| (unsigned char)fifo_data_out[i * f_len + 2]);
		gyro_lsb.dataz =
		((unsigned char)fifo_data_out[i * f_len + 5] << 8
				| (unsigned char)fifo_data_out[i * f_len + 4]);
		smi130_gyro_remap_sensor_data(&gyro_lsb, smi130_gyro);
		input_event(smi130_gyro->input, EV_MSC, MSC_TIME,
		ts.tv_sec);
		input_event(smi130_gyro->input, EV_MSC, MSC_TIME,
		ts.tv_nsec);
		input_event(smi130_gyro->input, EV_MSC,
			MSC_GESTURE, gyro_lsb.datax);
		input_event(smi130_gyro->input, EV_MSC,
			MSC_RAW, gyro_lsb.datay);
		input_event(smi130_gyro->input, EV_MSC,
			MSC_SCAN, gyro_lsb.dataz);
		input_sync(smi130_gyro->input);
		smi130_gyro->timestamp += time_internal - sample_drift_offset;
	}
	drift_time = smi130_gyro->timestamp - smi130_gyro->fifo_time;
	if (data_cnt % 20 == 0) {
		if (ABS(drift_time) > div64_u64(time_odr, 5)) {
			sample_drift_offset =
		div64_s64(drift_time, smi130_gyro->gyro_count - pre_data_cnt);
			pre_data_cnt = smi130_gyro->gyro_count;
			time_odr = time_internal;
		}
	}
}


static enum hrtimer_restart reportdata_timer_fun(
	struct hrtimer *hrtimer)
{
	struct smi_gyro_client_data *client_data =
		container_of(hrtimer, struct smi_gyro_client_data, timer);
	int32_t delay = 0;
	delay = 10;
	queue_work(reportdata_wq, &(client_data->report_data_work));
	client_data->work_delay_kt = ns_to_ktime(delay*1000000);
	hrtimer_forward(hrtimer, ktime_get(), client_data->work_delay_kt);

	return HRTIMER_RESTART;
}

static ssize_t smi_gyro_show_enable_timer(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);

	return snprintf(buf, 16, "%d\n", client_data->is_timer_running);
}

static ssize_t smi_gyro_store_enable_timer(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (data) {
		if (0 == client_data->is_timer_running) {
			hrtimer_start(&client_data->timer,
			ns_to_ktime(10000000),
			HRTIMER_MODE_REL);
		client_data->is_timer_running = 1;
		client_data->base_time = 0;
		client_data->timestamp = 0;
		client_data->gyro_count = 0;
	}
	} else {
		if (1 == client_data->is_timer_running) {
			hrtimer_cancel(&client_data->timer);
			client_data->is_timer_running = 0;
			client_data->base_time = 0;
			client_data->timestamp = 0;
			client_data->gyro_count = 0;
	}
	}
	return count;
}

static ssize_t smi130_gyro_show_debug_level(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);
	err = snprintf(buf, 8, "%d\n", client_data->debug_level);
	return err;
}
static ssize_t smi130_gyro_store_debug_level(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int32_t ret = 0;
	unsigned long data;
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);
	ret = kstrtoul(buf, 16, &data);
	if (ret)
		return ret;
	client_data->debug_level = (uint8_t)data;
	return count;
}

static int smi_gyro_set_soft_reset(struct i2c_client *client)
{
	int err = 0;
	unsigned char data = SMI_GYRO_SOFT_RESET_VALUE;
	err = smi_gyro_i2c_write(client, SMI130_GYRO_BGW_SOFTRESET_ADDR, &data, 1);
	return err;
}

static ssize_t smi_gyro_show_chip_id(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 16, "%d\n", SENSOR_CHIP_ID_SMI_GYRO);
}

#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
static inline int smi130_check_gyro_early_buff_enable_flag(
		struct smi_gyro_client_data *client_data)
{
	if (client_data->gyro_buffer_smi130_samples == true)
		return 1;
	else
		return 0;
}
static void smi130_check_gyro_enable_flag(
		struct smi_gyro_client_data *client_data, unsigned long data)
{
	if (data == SMI130_GYRO_MODE_NORMAL)
		client_data->gyro_enable = true;
	else
		client_data->gyro_enable = false;
}
#else
static inline int smi130_check_gyro_early_buff_enable_flag(
		struct smi_gyro_client_data *client_data)
{
	return 0;
}
static void smi130_check_gyro_enable_flag(
		struct smi_gyro_client_data *client_data, unsigned long data)
{
}
#endif

static ssize_t smi_gyro_show_op_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);
	u8 op_mode = 0xff;

	mutex_lock(&client_data->mutex_op_mode);
	SMI_GYRO_CALL_API(get_mode)(&op_mode);
	mutex_unlock(&client_data->mutex_op_mode);

	ret = snprintf(buf, 16, "%d\n", op_mode);

	return ret;
}

static ssize_t smi_gyro_store_op_mode(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);

	long op_mode;


	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;

	smi130_check_gyro_enable_flag(client_data, op_mode);

	err = smi130_check_gyro_early_buff_enable_flag(client_data);
	if (err)
		return count;

	mutex_lock(&client_data->mutex_op_mode);

	err = SMI_GYRO_CALL_API(set_mode)(op_mode);

	mutex_unlock(&client_data->mutex_op_mode);

	if (err)
		return err;
	else
		return count;
}



static ssize_t smi_gyro_show_value(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);
	int count;

	struct smi130_gyro_data_t value_data;
	SMI_GYRO_CALL_API(get_dataXYZ)(&value_data);
	/*SMI130_GYRO sensor raw data remapping*/
	smi130_gyro_remap_sensor_data(&value_data, client_data);

	count = snprintf(buf, 96, "%hd %hd %hd\n",
				value_data.datax,
				value_data.datay,
				value_data.dataz);

	return count;
}

static ssize_t smi_gyro_show_range(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char range = 0;
	SMI_GYRO_CALL_API(get_range_reg)(&range);
	err = snprintf(buf, 16, "%d\n", range);
	return err;
}

static ssize_t smi_gyro_store_range(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long range;
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);

	err = smi130_check_gyro_early_buff_enable_flag(client_data);
	if (err)
		return count;

	err = kstrtoul(buf, 10, &range);
	if (err)
		return err;
	SMI_GYRO_CALL_API(set_range_reg)(range);
	return count;
}

/*
decimation    odr     filter bandwidth     bits
20	100HZ		32HZ		7
10	200Hz		64HZ		6
20	100HZ		12HZ		5
10	200hz		23HZ		4
5	400HZ		47HZ		3
2	1000HZ		116HZ		2
0	2000HZ		230HZ		1
0	2000HZ		Unfiltered(523HZ)	0
*/

static const uint64_t odr_map[8] = {
500000, 500000, 1000000, 2500000, 5000000, 10000000, 5000000, 10000000};

static ssize_t smi_gyro_show_bandwidth(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char bandwidth = 0;
	SMI_GYRO_CALL_API(get_bw)(&bandwidth);
	err = snprintf(buf, 16, "%d\n", bandwidth);
	return err;
}

static ssize_t smi_gyro_store_bandwidth(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);
	unsigned long bandwidth;
	u8 op_mode = 0xff;

	err = smi130_check_gyro_early_buff_enable_flag(client_data);
	if (err)
		return count;

	err = kstrtoul(buf, 10, &bandwidth);
	if (err)
		return err;
	/*
	set bandwidth only in the op_mode=0
	*/
	err = SMI_GYRO_CALL_API(get_mode)(&op_mode);
	if (op_mode == 0) {
		err += SMI_GYRO_CALL_API(set_bw)(bandwidth);
	} else {
		err += SMI_GYRO_CALL_API(set_mode)(0);
		err += SMI_GYRO_CALL_API(set_bw)(bandwidth);
		smi130_gyro_delay(1);
		err += SMI_GYRO_CALL_API(set_mode)(2);
		smi130_gyro_delay(3);
	}

	if (err)
		PERR("set failed");
	client_data->time_odr = odr_map[bandwidth];
	client_data->base_time = 0;
	client_data->gyro_count = 0;
	return count;
}


static ssize_t smi_gyro_show_enable(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);
	int err;

	mutex_lock(&client_data->mutex_enable);
	err = snprintf(buf, 16, "%d\n", client_data->enable);
	mutex_unlock(&client_data->mutex_enable);
	return err;
}

static ssize_t smi_gyro_store_enable(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	data = data ? 1 : 0;
	mutex_lock(&client_data->mutex_enable);
	if (data != client_data->enable) {
		if (data) {
			schedule_delayed_work(
					&client_data->work,
					msecs_to_jiffies(atomic_read(
							&client_data->delay)));
		} else {
			cancel_delayed_work_sync(&client_data->work);
		}

		client_data->enable = data;
	}
	mutex_unlock(&client_data->mutex_enable);

	return count;
}

static ssize_t smi_gyro_show_delay(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);

	return snprintf(buf, 16, "%d\n", atomic_read(&client_data->delay));

}

static ssize_t smi_gyro_store_delay(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	if (data == 0) {
		err = -EINVAL;
		return err;
	}

	if (data < SMI_GYRO_DELAY_MIN)
		data = SMI_GYRO_DELAY_MIN;

	atomic_set(&client_data->delay, data);

	return count;
}


static ssize_t smi_gyro_store_fastoffset_en(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long fastoffset_en;
	err = kstrtoul(buf, 10, &fastoffset_en);
	if (err)
		return err;
	if (fastoffset_en) {

#ifdef CONFIG_SENSORS_BMI058
		SMI_GYRO_CALL_API(set_fast_offset_en_ch)(BMI058_X_AXIS, 1);
		SMI_GYRO_CALL_API(set_fast_offset_en_ch)(BMI058_Y_AXIS, 1);
#else
		SMI_GYRO_CALL_API(set_fast_offset_en_ch)(SMI130_GYRO_X_AXIS, 1);
		SMI_GYRO_CALL_API(set_fast_offset_en_ch)(SMI130_GYRO_Y_AXIS, 1);
#endif

		SMI_GYRO_CALL_API(set_fast_offset_en_ch)(SMI130_GYRO_Z_AXIS, 1);
		SMI_GYRO_CALL_API(enable_fast_offset)();
	}
	return count;
}

static ssize_t smi_gyro_store_slowoffset_en(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long slowoffset_en;
	err = kstrtoul(buf, 10, &slowoffset_en);
	if (err)
		return err;
	if (slowoffset_en) {
		SMI_GYRO_CALL_API(set_slow_offset_th)(3);
		SMI_GYRO_CALL_API(set_slow_offset_dur)(0);
#ifdef CONFIG_SENSORS_BMI058
		SMI_GYRO_CALL_API(set_slow_offset_en_ch)(BMI058_X_AXIS, 1);
		SMI_GYRO_CALL_API(set_slow_offset_en_ch)(BMI058_Y_AXIS, 1);
#else
		SMI_GYRO_CALL_API(set_slow_offset_en_ch)(SMI130_GYRO_X_AXIS, 1);
		SMI_GYRO_CALL_API(set_slow_offset_en_ch)(SMI130_GYRO_Y_AXIS, 1);
#endif
		SMI_GYRO_CALL_API(set_slow_offset_en_ch)(SMI130_GYRO_Z_AXIS, 1);
	} else {
#ifdef CONFIG_SENSORS_BMI058
	SMI_GYRO_CALL_API(set_slow_offset_en_ch)(BMI058_X_AXIS, 0);
	SMI_GYRO_CALL_API(set_slow_offset_en_ch)(BMI058_Y_AXIS, 0);
#else
	SMI_GYRO_CALL_API(set_slow_offset_en_ch)(SMI130_GYRO_X_AXIS, 0);
	SMI_GYRO_CALL_API(set_slow_offset_en_ch)(SMI130_GYRO_Y_AXIS, 0);
#endif
	SMI_GYRO_CALL_API(set_slow_offset_en_ch)(SMI130_GYRO_Z_AXIS, 0);
	}

	return count;
}

static ssize_t smi_gyro_show_selftest(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char selftest;
	SMI_GYRO_CALL_API(selftest)(&selftest);
	err = snprintf(buf, 16, "%d\n", selftest);
	return err;
}

static ssize_t smi_gyro_show_sleepdur(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char sleepdur;
	SMI_GYRO_CALL_API(get_sleepdur)(&sleepdur);
	err = snprintf(buf, 16, "%d\n", sleepdur);
	return err;
}

static ssize_t smi_gyro_store_sleepdur(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long sleepdur;
	err = kstrtoul(buf, 10, &sleepdur);
	if (err)
		return err;
	SMI_GYRO_CALL_API(set_sleepdur)(sleepdur);
	return count;
}

static ssize_t smi_gyro_show_autosleepdur(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char autosleepdur;
	SMI_GYRO_CALL_API(get_autosleepdur)(&autosleepdur);
	err = snprintf(buf, 16, "%d\n", autosleepdur);
	return err;
}

static ssize_t smi_gyro_store_autosleepdur(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long autosleepdur;
	unsigned char bandwidth;
	err = kstrtoul(buf, 10, &autosleepdur);
	if (err)
		return err;
	SMI_GYRO_CALL_API(get_bw)(&bandwidth);
	SMI_GYRO_CALL_API(set_autosleepdur)(autosleepdur, bandwidth);
	return count;
}

static ssize_t smi_gyro_show_place(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);
	int place = BOSCH_SENSOR_PLACE_UNKNOWN;

	if (NULL != client_data->bosch_pd)
		place = client_data->bosch_pd->place;

	return snprintf(buf, 16, "%d\n", place);
}


#ifdef SMI_GYRO_DEBUG
static ssize_t smi_gyro_store_softreset(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long softreset;
	err = kstrtoul(buf, 10, &softreset);
	if (err)
		return err;
	SMI_GYRO_CALL_API(set_soft_reset)();
	return count;
}

static ssize_t smi_gyro_show_dumpreg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;
	u8 reg[0x40];
	int i;
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);

	for (i = 0; i < 0x40; i++) {
		smi_gyro_i2c_read(client_data->client, i, reg+i, 1);

		count += snprintf(&buf[count], 48, "0x%x: 0x%x\n", i, reg[i]);
	}
	return count;
}
#endif

#ifdef SMI_GYRO_USE_FIFO
static ssize_t smi_gyro_show_fifo_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char fifo_mode;
	SMI_GYRO_CALL_API(get_fifo_mode)(&fifo_mode);
	err = snprintf(buf, 16, "%d\n", fifo_mode);
	return err;
}

static ssize_t smi_gyro_store_fifo_mode(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long fifo_mode;
	err = kstrtoul(buf, 10, &fifo_mode);
	if (err)
		return err;
	SMI_GYRO_CALL_API(set_fifo_mode)(fifo_mode);
	return count;
}

static ssize_t smi_gyro_show_fifo_framecount(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char fifo_framecount;
	SMI_GYRO_CALL_API(get_fifo_framecount)(&fifo_framecount);
	err = snprintf(buf, 32, "%d\n", fifo_framecount);
	return err;
}

static ssize_t smi_gyro_store_fifo_framecount(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	client_data->fifo_count = (unsigned int) data;

	return count;
}

static ssize_t smi_gyro_show_fifo_overrun(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char fifo_overrun;
	SMI_GYRO_CALL_API(get_fifo_overrun)(&fifo_overrun);
	err = snprintf(buf, 16, "%d\n", fifo_overrun);
	return err;
}

static ssize_t smi_gyro_show_fifo_data_frame(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char f_len = 0;
	unsigned char fifo_framecount;
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);

	if (client_data->fifo_datasel)
		/*Select one axis data output for every fifo frame*/
		f_len = 2;
	else
		/*Select X Y Z axis data output for every fifo frame*/
		f_len = 6;

	if (SMI_GYRO_CALL_API(get_fifo_framecount)(&fifo_framecount) < 0) {
		PERR("bm160_get_fifo_framecount err\n");
		return -EINVAL;
	}
	if (fifo_framecount == 0)
		return 0;

	smi_gyro_i2c_burst_read(client_data->client, SMI130_GYRO_FIFO_DATA_ADDR,
			buf, fifo_framecount * f_len);
	return fifo_framecount * f_len;
}

/*!
 * @brief show fifo_data_sel axis definition(Android definition, not sensor HW reg).
 * 0--> x, y, z axis fifo data for every frame
 * 1--> only x axis fifo data for every frame
 * 2--> only y axis fifo data for every frame
 * 3--> only z axis fifo data for every frame
 */
static ssize_t smi_gyro_show_fifo_data_sel(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char fifo_data_sel;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi_gyro_client_data *client_data = i2c_get_clientdata(client);
	signed char place = BOSCH_SENSOR_PLACE_UNKNOWN;

	SMI_GYRO_CALL_API(get_fifo_data_sel)(&fifo_data_sel);

	/*remapping fifo_dat_sel if define virtual place in BSP files*/
	if ((NULL != client_data->bosch_pd) &&
		(BOSCH_SENSOR_PLACE_UNKNOWN != client_data->bosch_pd->place)) {
		place = client_data->bosch_pd->place;
		/* sensor with place 0 needs not to be remapped */
		if ((place > 0) && (place < MAX_AXIS_REMAP_TAB_SZ)) {
			if (SMI130_GYRO_FIFO_DAT_SEL_X == fifo_data_sel)
				/* SMI130_GYRO_FIFO_DAT_SEL_X: 1, Y:2, Z:3;
				*bosch_axis_remap_tab_dft[i].src_x:0, y:1, z:2
				*so we need to +1*/
				fifo_data_sel =
					bosch_axis_remap_tab_dft[place].src_x + 1;

			else if (SMI130_GYRO_FIFO_DAT_SEL_Y == fifo_data_sel)
				fifo_data_sel =
					bosch_axis_remap_tab_dft[place].src_y + 1;
		}

	}

	err = snprintf(buf, 16, "%d\n", fifo_data_sel);
	return err;
}

/*!
 * @brief store fifo_data_sel axis definition(Android definition, not sensor HW reg).
 * 0--> x, y, z axis fifo data for every frame
 * 1--> only x axis fifo data for every frame
 * 2--> only y axis fifo data for every frame
 * 3--> only z axis fifo data for every frame
 */
static ssize_t smi_gyro_store_fifo_data_sel(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)

{
	int err;
	unsigned long fifo_data_sel;

	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);
	signed char place;

	err = kstrtoul(buf, 10, &fifo_data_sel);
	if (err)
		return err;

	/*save fifo_data_sel(android axis definition)*/
	client_data->fifo_datasel = (unsigned char) fifo_data_sel;

	/*remaping fifo_dat_sel if define virtual place*/
	if ((NULL != client_data->bosch_pd) &&
		(BOSCH_SENSOR_PLACE_UNKNOWN != client_data->bosch_pd->place)) {
		place = client_data->bosch_pd->place;
		/* sensor with place 0 needs not to be remapped */
		if ((place > 0) && (place < MAX_AXIS_REMAP_TAB_SZ)) {
			/*Need X Y axis revesal sensor place: P1, P3, P5, P7 */
			/* SMI130_GYRO_FIFO_DAT_SEL_X: 1, Y:2, Z:3;
			  * but bosch_axis_remap_tab_dft[i].src_x:0, y:1, z:2
			  * so we need to +1*/
			if (SMI130_GYRO_FIFO_DAT_SEL_X == fifo_data_sel)
				fifo_data_sel =
					bosch_axis_remap_tab_dft[place].src_x + 1;

			else if (SMI130_GYRO_FIFO_DAT_SEL_Y == fifo_data_sel)
				fifo_data_sel =
					bosch_axis_remap_tab_dft[place].src_y + 1;
		}
	}

	if (SMI_GYRO_CALL_API(set_fifo_data_sel)(fifo_data_sel) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi_gyro_show_fifo_tag(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char fifo_tag;
	SMI_GYRO_CALL_API(get_fifo_tag)(&fifo_tag);
	err = snprintf(buf, 16, "%d\n", fifo_tag);
	return err;
}

static ssize_t smi_gyro_store_fifo_tag(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)

{
	int err;
	unsigned long fifo_tag;
	err = kstrtoul(buf, 10, &fifo_tag);
	if (err)
		return err;
	SMI_GYRO_CALL_API(set_fifo_tag)(fifo_tag);
	return count;
}
#endif

static ssize_t smi130_gyro_driver_version_show(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);
	int ret;

	if (client_data == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}

	ret = snprintf(buf, 128, "Driver version: %s\n",
			DRIVER_VERSION);
	return ret;
}

#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
static int smi_gyro_read_bootsampl(struct smi_gyro_client_data *client_data,
		unsigned long enable_read)
{
	int i = 0;

	client_data->gyro_buffer_smi130_samples = false;
	if (enable_read) {
		for (i = 0; i < client_data->gyro_bufsample_cnt; i++) {
			if (client_data->debug_level & 0x08)
				PINFO("gyro=%d,x=%d,y=%d,z=%d,sec=%d,ns=%lld\n",
				i, client_data->smi130_gyro_samplist[i]->xyz[0],
				client_data->smi130_gyro_samplist[i]->xyz[1],
				client_data->smi130_gyro_samplist[i]->xyz[2],
				client_data->smi130_gyro_samplist[i]->tsec,
				client_data->smi130_gyro_samplist[i]->tnsec);
			input_report_abs(client_data->gyrobuf_dev, ABS_X,
				client_data->smi130_gyro_samplist[i]->xyz[0]);
			input_report_abs(client_data->gyrobuf_dev, ABS_Y,
				client_data->smi130_gyro_samplist[i]->xyz[1]);
			input_report_abs(client_data->gyrobuf_dev, ABS_Z,
				client_data->smi130_gyro_samplist[i]->xyz[2]);
			input_report_abs(client_data->gyrobuf_dev, ABS_RX,
				client_data->smi130_gyro_samplist[i]->tsec);
			input_report_abs(client_data->gyrobuf_dev, ABS_RY,
				client_data->smi130_gyro_samplist[i]->tnsec);
			input_sync(client_data->gyrobuf_dev);
		}
	} else {
		/* clean up */
		if (client_data->gyro_bufsample_cnt != 0) {
			for (i = 0; i < SMI_GYRO_MAXSAMPLE; i++)
				kmem_cache_free(client_data->smi_gyro_cachepool,
					client_data->smi130_gyro_samplist[i]);
			kmem_cache_destroy(client_data->smi_gyro_cachepool);
			client_data->gyro_bufsample_cnt = 0;
		}

	}
	/*SYN_CONFIG indicates end of data*/
	input_event(client_data->gyrobuf_dev, EV_SYN, SYN_CONFIG, 0xFFFFFFFF);
	input_sync(client_data->gyrobuf_dev);
	if (client_data->debug_level & 0x08)
		PINFO("End of gyro samples bufsample_cnt=%d\n",
				client_data->gyro_bufsample_cnt);
	return 0;
}
static ssize_t read_gyro_boot_sample_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);

	return snprintf(buf, 16, "%u\n",
			client_data->read_gyro_boot_sample);
}
static ssize_t read_gyro_boot_sample_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct smi_gyro_client_data *client_data = input_get_drvdata(input);
	unsigned long enable = 0;

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;
	if (enable > 1) {
		PERR("Invalid value of input, input=%ld\n", enable);
		return -EINVAL;
	}
	mutex_lock(&client_data->gyro_sensor_buff);
	err = smi_gyro_read_bootsampl(client_data, enable);
	mutex_unlock(&client_data->gyro_sensor_buff);
	if (err)
		return err;
	client_data->read_gyro_boot_sample = enable;

	return count;
}
#endif


static DEVICE_ATTR(chip_id, S_IRUSR,
		smi_gyro_show_chip_id, NULL);
#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
static DEVICE_ATTR(read_gyro_boot_sample, 0644,
		read_gyro_boot_sample_show, read_gyro_boot_sample_store);
#endif
static DEVICE_ATTR(op_mode, S_IRUGO | S_IWUSR,
		smi_gyro_show_op_mode, smi_gyro_store_op_mode);
static DEVICE_ATTR(value, S_IRUSR,
		smi_gyro_show_value, NULL);
static DEVICE_ATTR(range, S_IRUGO | S_IWUSR,
		smi_gyro_show_range, smi_gyro_store_range);
static DEVICE_ATTR(bandwidth, S_IRUGO | S_IWUSR,
		smi_gyro_show_bandwidth, smi_gyro_store_bandwidth);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR,
		smi_gyro_show_enable, smi_gyro_store_enable);
static DEVICE_ATTR(delay, S_IRUGO | S_IWUSR,
		smi_gyro_show_delay, smi_gyro_store_delay);
static DEVICE_ATTR(fastoffset_en, S_IWUSR,
		NULL, smi_gyro_store_fastoffset_en);
static DEVICE_ATTR(slowoffset_en, S_IWUSR,
		NULL, smi_gyro_store_slowoffset_en);
static DEVICE_ATTR(selftest, S_IRUGO,
		smi_gyro_show_selftest, NULL);
static DEVICE_ATTR(sleepdur, S_IRUGO | S_IWUSR,
		smi_gyro_show_sleepdur, smi_gyro_store_sleepdur);
static DEVICE_ATTR(autosleepdur, S_IRUGO | S_IWUSR,
		smi_gyro_show_autosleepdur, smi_gyro_store_autosleepdur);
static DEVICE_ATTR(place, S_IRUSR,
		smi_gyro_show_place, NULL);
static DEVICE_ATTR(enable_timer, S_IRUGO | S_IWUSR,
		smi_gyro_show_enable_timer, smi_gyro_store_enable_timer);
static DEVICE_ATTR(debug_level, S_IRUGO | S_IWUSR,
		smi130_gyro_show_debug_level, smi130_gyro_store_debug_level);
static DEVICE_ATTR(driver_version, S_IRUSR,
		smi130_gyro_driver_version_show, NULL);
#ifdef SMI_GYRO_DEBUG
static DEVICE_ATTR(softreset, S_IWUSR,
		NULL, smi_gyro_store_softreset);
static DEVICE_ATTR(regdump, S_IRUSR,
		smi_gyro_show_dumpreg, NULL);
#endif
#ifdef SMI_GYRO_USE_FIFO
static DEVICE_ATTR(fifo_mode, S_IRUGO | S_IWUSR,
		smi_gyro_show_fifo_mode, smi_gyro_store_fifo_mode);
static DEVICE_ATTR(fifo_framecount, S_IRUGO | S_IWUSR,
		smi_gyro_show_fifo_framecount, smi_gyro_store_fifo_framecount);
static DEVICE_ATTR(fifo_overrun, S_IRUGO,
		smi_gyro_show_fifo_overrun, NULL);
static DEVICE_ATTR(fifo_data_frame, S_IRUSR,
		smi_gyro_show_fifo_data_frame, NULL);
static DEVICE_ATTR(fifo_data_sel, S_IRUGO | S_IWUSR,
		smi_gyro_show_fifo_data_sel, smi_gyro_store_fifo_data_sel);
static DEVICE_ATTR(fifo_tag, S_IRUGO | S_IWUSR,
		smi_gyro_show_fifo_tag, smi_gyro_store_fifo_tag);
#endif

static struct attribute *smi_gyro_attributes[] = {
	&dev_attr_chip_id.attr,
#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
	&dev_attr_read_gyro_boot_sample.attr,
#endif
	&dev_attr_op_mode.attr,
	&dev_attr_value.attr,
	&dev_attr_range.attr,
	&dev_attr_bandwidth.attr,
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_fastoffset_en.attr,
	&dev_attr_slowoffset_en.attr,
	&dev_attr_selftest.attr,
	&dev_attr_sleepdur.attr,
	&dev_attr_autosleepdur.attr,
	&dev_attr_place.attr,
	&dev_attr_enable_timer.attr,
	&dev_attr_debug_level.attr,
	&dev_attr_driver_version.attr,
#ifdef SMI_GYRO_DEBUG
	&dev_attr_softreset.attr,
	&dev_attr_regdump.attr,
#endif
#ifdef SMI_GYRO_USE_FIFO
	&dev_attr_fifo_mode.attr,
	&dev_attr_fifo_framecount.attr,
	&dev_attr_fifo_overrun.attr,
	&dev_attr_fifo_data_frame.attr,
	&dev_attr_fifo_data_sel.attr,
	&dev_attr_fifo_tag.attr,
#endif
	NULL
};

static struct attribute_group smi_gyro_attribute_group = {
	.attrs = smi_gyro_attributes
};


static int smi_gyro_input_init(struct smi_gyro_client_data *client_data)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = SENSOR_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_X, SMI_GYRO_VALUE_MIN, SMI_GYRO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, SMI_GYRO_VALUE_MIN, SMI_GYRO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, ABS_Z, SMI_GYRO_VALUE_MIN, SMI_GYRO_VALUE_MAX, 0, 0);
	input_set_capability(dev, EV_MSC, MSC_GESTURE);
	input_set_capability(dev, EV_MSC, MSC_RAW);
	input_set_capability(dev, EV_MSC, MSC_SCAN);
	input_set_capability(dev, EV_MSC, MSC_TIME);
	input_set_drvdata(dev, client_data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	client_data->input = dev;

	return 0;
}

static void smi_gyro_input_destroy(struct smi_gyro_client_data *client_data)
{
	struct input_dev *dev = client_data->input;

	input_unregister_device(dev);
	input_free_device(dev);
}
#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
static void store_gyro_boot_sample(struct smi_gyro_client_data *client_data,
			int x, int y, int z, struct timespec ts)
{
	if (false == client_data->gyro_buffer_smi130_samples)
		return;
	mutex_lock(&client_data->gyro_sensor_buff);
	if (ts.tv_sec <  client_data->max_buffer_time) {
		if (client_data->gyro_bufsample_cnt < SMI_GYRO_MAXSAMPLE) {
			client_data->smi130_gyro_samplist[client_data
				->gyro_bufsample_cnt]->xyz[0] = x;
			client_data->smi130_gyro_samplist[client_data
				->gyro_bufsample_cnt]->xyz[1] = y;
			client_data->smi130_gyro_samplist[client_data
				->gyro_bufsample_cnt]->xyz[2] = z;
			client_data->smi130_gyro_samplist[client_data
				->gyro_bufsample_cnt]->tsec = ts.tv_sec;
			client_data->smi130_gyro_samplist[client_data
				->gyro_bufsample_cnt]->tnsec = ts.tv_nsec;
			client_data->gyro_bufsample_cnt++;
		}
	} else {
		PINFO("End of GYRO buffering %d",
				client_data->gyro_bufsample_cnt);
		client_data->gyro_buffer_smi130_samples = false;
		if (client_data->gyro_enable == false) {
			smi130_gyro_set_mode(SMI130_GYRO_MODE_SUSPEND);
			smi130_gyro_delay(5);
		}
	}
	mutex_unlock(&client_data->gyro_sensor_buff);
}
#else
static void store_gyro_boot_sample(struct smi_gyro_client_data *client_data,
		int x, int y, int z, struct timespec ts)
{
}
#endif


#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
static int smi130_gyro_early_buff_init(struct smi_gyro_client_data *client_data)
{
	int i = 0, err = 0;

	client_data->gyro_bufsample_cnt = 0;
	client_data->report_evt_cnt = 5;
	client_data->max_buffer_time = 40;

	client_data->smi_gyro_cachepool = kmem_cache_create("gyro_sensor_sample"
			, sizeof(struct smi_gyro_sample), 0,
			SLAB_HWCACHE_ALIGN, NULL);
	if (!client_data->smi_gyro_cachepool) {
		PERR("smi_gyro_cachepool cache create failed\n");
		err = -ENOMEM;
		return 0;
	}

	for (i = 0; i < SMI_GYRO_MAXSAMPLE; i++) {
		client_data->smi130_gyro_samplist[i] =
			kmem_cache_alloc(client_data->smi_gyro_cachepool,
					GFP_KERNEL);
		if (!client_data->smi130_gyro_samplist[i]) {
			err = -ENOMEM;
			goto clean_exit1;
		}
	}


	client_data->gyrobuf_dev = input_allocate_device();
	if (!client_data->gyrobuf_dev) {
		err = -ENOMEM;
		PERR("input device allocation failed\n");
		goto clean_exit1;
	}
	client_data->gyrobuf_dev->name = "smi130_gyrobuf";
	client_data->gyrobuf_dev->id.bustype = BUS_I2C;
	input_set_events_per_packet(client_data->gyrobuf_dev,
			client_data->report_evt_cnt * SMI_GYRO_MAXSAMPLE);
	set_bit(EV_ABS, client_data->gyrobuf_dev->evbit);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_X,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_Y,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_Z,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_RX,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_RY,
			-G_MAX, G_MAX, 0, 0);
	err = input_register_device(client_data->gyrobuf_dev);
	if (err) {
		PERR("unable to register input device %s\n",
				client_data->gyrobuf_dev->name);
		goto clean_exit2;
	}

	client_data->gyro_buffer_smi130_samples = true;
	client_data->gyro_enable = false;

	mutex_init(&client_data->gyro_sensor_buff);
	smi130_gyro_set_mode(SMI130_GYRO_MODE_NORMAL);
	smi130_gyro_delay(5);

	smi130_gyro_set_bw(5);
	smi130_gyro_delay(5);

	smi130_gyro_set_range_reg(4);
	smi130_gyro_delay(5);

	smi130_gyro_set_mode(SMI130_GYRO_MODE_NORMAL);
	smi130_gyro_delay(5);

	smi130_gyro_set_range_reg(4);
	smi130_gyro_delay(5);

	smi130_gyro_set_data_en(SMI130_GYRO_ENABLE);

	return 1;

clean_exit2:
	input_free_device(client_data->gyrobuf_dev);
clean_exit1:
	for (i = 0; i < SMI_GYRO_MAXSAMPLE; i++)
		kmem_cache_free(client_data->smi_gyro_cachepool,
				client_data->smi130_gyro_samplist[i]);
	kmem_cache_destroy(client_data->smi_gyro_cachepool);
	return 0;
}

static void smi130_gyro_input_cleanup(struct smi_gyro_client_data *client_data)
{
	int i = 0;

	input_unregister_device(client_data->gyrobuf_dev);
	input_free_device(client_data->gyrobuf_dev);
	for (i = 0; i < SMI_GYRO_MAXSAMPLE; i++)
		kmem_cache_free(client_data->smi_gyro_cachepool,
				client_data->smi130_gyro_samplist[i]);
	kmem_cache_destroy(client_data->smi_gyro_cachepool);
}

static int smi130_enable_int1(void)
{
	return smi130_gyro_set_data_en(SMI130_GYRO_DISABLE);
}
#else
static int smi130_gyro_early_buff_init(struct smi_gyro_client_data *client_data)
{
	return 1;
}
static void smi130_gyro_input_cleanup(struct smi_gyro_client_data *client_data)
{
}
static int smi130_enable_int1(void)
{
	return smi130_gyro_set_data_en(SMI130_GYRO_ENABLE);
}
#endif


#if defined(SMI130_GYRO_ENABLE_INT1) || defined(SMI130_GYRO_ENABLE_INT2)
static irqreturn_t smi130_gyro_irq_work_func(int irq, void *handle)
{
	struct smi_gyro_client_data *client_data = handle;
	struct smi130_gyro_data_t gyro_data;
	struct timespec ts;
	ts = ns_to_timespec(client_data->timestamp);

	SMI_GYRO_CALL_API(get_dataXYZ)(&gyro_data);
	/*remapping for SMI130_GYRO sensor*/
	smi130_gyro_remap_sensor_data(&gyro_data, client_data);
	input_event(client_data->input, EV_MSC, MSC_TIME,
		ts.tv_sec);
	input_event(client_data->input, EV_MSC, MSC_TIME,
		ts.tv_nsec);
	input_event(client_data->input, EV_MSC,
		MSC_GESTURE, gyro_data.datax);
	input_event(client_data->input, EV_MSC,
		MSC_RAW, gyro_data.datay);
	input_event(client_data->input, EV_MSC,
		MSC_SCAN, gyro_data.dataz);
	input_sync(client_data->input);
	store_gyro_boot_sample(client_data, gyro_data.datax,
			gyro_data.datay, gyro_data.dataz, ts);
	return IRQ_HANDLED;
}

static irqreturn_t smi_gyro_irq_handler(int irq, void *handle)
{
	struct smi_gyro_client_data *client_data = handle;
	client_data->timestamp= smi130_gyro_get_alarm_timestamp();
	return IRQ_WAKE_THREAD;
}
#endif
static int smi_gyro_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	struct smi_gyro_client_data *client_data = NULL;
	PINFO("function entrance");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		PERR("i2c_check_functionality error!");
		err = -EIO;
		goto exit_err_clean;
	}

	if (NULL == smi_gyro_client) {
		smi_gyro_client = client;
	} else {
		PERR("this driver does not support multiple clients");
		err = -EINVAL;
		goto exit_err_clean;
	}

	/* check chip id */
	err = smi_gyro_check_chip_id(client);
	if (!err) {
		PINFO("Bosch Sensortec Device %s detected", SENSOR_NAME);
	} else {
		PERR("Bosch Sensortec Device not found, chip id mismatch");
		err = -1;
		goto exit_err_clean;
	}

	/* do soft reset */
	smi130_gyro_delay(5);
	err = smi_gyro_set_soft_reset(client);
	if (err < 0) {
		PERR("erro soft reset!\n");
		err = -EINVAL;
		goto exit_err_clean;
	}
	smi130_gyro_delay(30);


	client_data = kzalloc(sizeof(struct smi_gyro_client_data), GFP_KERNEL);
	if (NULL == client_data) {
		PERR("no memory available");
		err = -ENOMEM;
		goto exit_err_clean;
	}

	i2c_set_clientdata(client, client_data);
	client_data->client = client;

	mutex_init(&client_data->mutex_op_mode);
	mutex_init(&client_data->mutex_enable);

	/* input device init */
	err = smi_gyro_input_init(client_data);
	if (err < 0)
		goto exit_err_clean;

	/* sysfs node creation */
	err = sysfs_create_group(&client_data->input->dev.kobj,
			&smi_gyro_attribute_group);

	if (err < 0)
		goto exit_err_sysfs;

	if (NULL != client->dev.platform_data) {
		client_data->bosch_pd = kzalloc(sizeof(*client_data->bosch_pd),
				GFP_KERNEL);

		if (NULL != client_data->bosch_pd) {
			memcpy(client_data->bosch_pd, client->dev.platform_data,
					sizeof(*client_data->bosch_pd));
			PINFO("%s sensor driver set place: p%d",
					SENSOR_NAME,
					client_data->bosch_pd->place);
		}
	}

	/* workqueue init */
	INIT_DELAYED_WORK(&client_data->work, smi_gyro_work_func);
	atomic_set(&client_data->delay, SMI_GYRO_DELAY_DEFAULT);

	/* h/w init */
	client_data->device.bus_read = smi_gyro_i2c_read_wrapper;
	client_data->device.bus_write = smi_gyro_i2c_write_wrapper;
	client_data->device.delay_msec = smi130_gyro_delay;
	SMI_GYRO_CALL_API(init)(&client_data->device);

	smi_gyro_dump_reg(client);

	client_data->enable = 0;
	client_data->fifo_datasel = 0;
	client_data->fifo_count = 0;

	/*workqueue init*/
	INIT_WORK(&client_data->report_data_work,
	smi130_gyro_work_func);
	reportdata_wq = create_singlethread_workqueue("smi130_gyro_wq");
	if (NULL == reportdata_wq)
		PERR("fail to create the reportdta_wq %d", -ENOMEM);
	hrtimer_init(&client_data->timer, CLOCK_MONOTONIC,
		HRTIMER_MODE_REL);
	client_data->timer.function = reportdata_timer_fun;
	client_data->work_delay_kt = ns_to_ktime(10000000);
	client_data->is_timer_running = 0;
	client_data->time_odr = 500000;
#ifdef SMI130_GYRO_ENABLE_INT1
	err = SMI_GYRO_CALL_API(set_mode)(SMI130_GYRO_MODE_NORMAL);
	smi130_gyro_delay(5);
	/*config the interrupt and map the interrupt*/
	/*high level trigger*/
	err += smi130_gyro_set_int_lvl(SMI130_GYRO_INT1_DATA, 1);
	smi130_gyro_delay(5);
	err += smi130_gyro_set_int_od(SMI130_GYRO_INT1, 0);
	smi130_gyro_delay(5);
	err += smi130_gyro_set_int_data(SMI130_GYRO_INT1_DATA, SMI130_GYRO_ENABLE);
	smi130_gyro_delay(5);
	err += smi130_enable_int1();
	smi130_gyro_delay(5);
	/*default odr is 100HZ*/
	err += SMI_GYRO_CALL_API(set_bw)(7);
	smi130_gyro_delay(5);
	if (err)
		PERR("config sensor data ready interrupt failed");
#endif
#ifdef SMI130_GYRO_ENABLE_INT2
	err = SMI_GYRO_CALL_API(set_mode)(SMI130_GYRO_MODE_NORMAL);
	/*config the interrupt and map the interrupt*/
	/*high level trigger*/
	err += smi130_gyro_set_int_lvl(SMI130_GYRO_INT2_DATA, 1);
	smi130_gyro_delay(3);
	err += smi130_gyro_set_int_od(SMI130_GYRO_INT2, 0);
	smi130_gyro_delay(5);
	err += smi130_gyro_set_int_data(SMI130_GYRO_INT2_DATA, SMI130_GYRO_ENABLE);
	smi130_gyro_delay(3);
	err += smi130_gyro_set_data_en(SMI130_GYRO_ENABLE);
	/*default odr is 100HZ*/
	err += SMI_GYRO_CALL_API(set_bw)(7);
	smi130_gyro_delay(5);
	if (err)
		PERR("config sensor data ready interrupt failed");
#endif
	err += SMI_GYRO_CALL_API(set_mode)(
		SMI_GYRO_VAL_NAME(MODE_SUSPEND));
	if (err < 0)
		goto exit_err_sysfs;
#ifdef CONFIG_HAS_EARLYSUSPEND
	client_data->early_suspend_handler.suspend = smi_gyro_early_suspend;
	client_data->early_suspend_handler.resume = smi_gyro_late_resume;
	register_early_suspend(&client_data->early_suspend_handler);
#endif
#if defined(SMI130_GYRO_ENABLE_INT1) || defined(SMI130_GYRO_ENABLE_INT2)
	client_data->gpio_pin = of_get_named_gpio_flags(
		client->dev.of_node,
		"smi130_gyro,gpio_irq", 0, NULL);
	PDEBUG("smi130_gyro qpio number:%d\n", client_data->gpio_pin);
	err = gpio_request_one(client_data->gpio_pin,
				GPIOF_IN, "bm160_interrupt");
	if (err < 0) {
		PDEBUG("requestgpio  failed\n");
		client_data->gpio_pin = 0;
	}
	if (client_data->gpio_pin != 0) {
		err = gpio_direction_input(client_data->gpio_pin);
		if (err < 0) {
			PDEBUG("request failed\n");
		}
		client_data->IRQ = gpio_to_irq(client_data->gpio_pin);
		err = request_threaded_irq(client_data->IRQ,
				smi_gyro_irq_handler, smi130_gyro_irq_work_func,
				IRQF_TRIGGER_RISING, SENSOR_NAME, client_data);
		if (err < 0)
			PDEBUG("request handle failed\n");
	}
#endif

	err = smi130_gyro_early_buff_init(client_data);
	if (!err)
		return err;

	PINFO("sensor %s probed successfully", SENSOR_NAME);

	dev_dbg(&client->dev,
		"i2c_client: %p client_data: %p i2c_device: %p input: %p",
		client, client_data, &client->dev, client_data->input);

	return 0;

exit_err_sysfs:
	if (err)
		smi_gyro_input_destroy(client_data);

exit_err_clean:
	if (err) {
		if (client_data != NULL) {
			kfree(client_data);
			client_data = NULL;
		}

		smi_gyro_client = NULL;
	}

	return err;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static int smi_gyro_pre_suspend(struct i2c_client *client)
{
	int err = 0;
	struct smi_gyro_client_data *client_data =
		(struct smi_gyro_client_data *)i2c_get_clientdata(client);
	PINFO("function entrance");

	mutex_lock(&client_data->mutex_enable);
	if (client_data->enable) {
		cancel_delayed_work_sync(&client_data->work);
		PINFO("cancel work");
	}
	mutex_unlock(&client_data->mutex_enable);
	if (client_data->is_timer_running) {
		hrtimer_cancel(&client_data->timer);
		client_data->base_time = 0;
		client_data->timestamp = 0;
		client_data->fifo_time = 0;
		client_data->gyro_count = 0;
	}
	return err;
}

static int smi_gyro_post_resume(struct i2c_client *client)
{
	int err = 0;
	struct smi_gyro_client_data *client_data =
		(struct smi_gyro_client_data *)i2c_get_clientdata(client);

	PINFO("function entrance");
	mutex_lock(&client_data->mutex_enable);
	if (client_data->enable) {
		schedule_delayed_work(&client_data->work,
				msecs_to_jiffies(
					atomic_read(&client_data->delay)));
	}
	mutex_unlock(&client_data->mutex_enable);
	if (client_data->is_timer_running) {
		hrtimer_start(&client_data->timer,
					ns_to_ktime(client_data->time_odr),
			HRTIMER_MODE_REL);
		client_data->base_time = 0;
		client_data->timestamp = 0;
		client_data->is_timer_running = 1;
		client_data->gyro_count = 0;
	}
	return err;
}

static void smi_gyro_early_suspend(struct early_suspend *handler)
{
	int err = 0;
	struct smi_gyro_client_data *client_data =
		(struct smi_gyro_client_data *)container_of(handler,
			struct smi_gyro_client_data, early_suspend_handler);
	struct i2c_client *client = client_data->client;

	PINFO("function entrance");

	mutex_lock(&client_data->mutex_op_mode);
	if (client_data->enable) {
		err = smi_gyro_pre_suspend(client);
		err = SMI_GYRO_CALL_API(set_mode)(
				SMI_GYRO_VAL_NAME(MODE_SUSPEND));
	}
	mutex_unlock(&client_data->mutex_op_mode);
}

static void smi_gyro_late_resume(struct early_suspend *handler)
{

	int err = 0;
	struct smi_gyro_client_data *client_data =
		(struct smi_gyro_client_data *)container_of(handler,
			struct smi_gyro_client_data, early_suspend_handler);
	struct i2c_client *client = client_data->client;

	PINFO("function entrance");

	mutex_lock(&client_data->mutex_op_mode);

	if (client_data->enable)
		err = SMI_GYRO_CALL_API(set_mode)(SMI_GYRO_VAL_NAME(MODE_NORMAL));

	/* post resume operation */
	smi_gyro_post_resume(client);

	mutex_unlock(&client_data->mutex_op_mode);
}
#endif

void smi_gyro_shutdown(struct i2c_client *client)
{
	struct smi_gyro_client_data *client_data =
		(struct smi_gyro_client_data *)i2c_get_clientdata(client);

	mutex_lock(&client_data->mutex_op_mode);
	SMI_GYRO_CALL_API(set_mode)(
		SMI_GYRO_VAL_NAME(MODE_DEEPSUSPEND));
	mutex_unlock(&client_data->mutex_op_mode);
}

static int smi_gyro_remove(struct i2c_client *client)
{
	int err = 0;
	u8 op_mode;

	struct smi_gyro_client_data *client_data =
		(struct smi_gyro_client_data *)i2c_get_clientdata(client);

	if (NULL != client_data) {
#ifdef CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&client_data->early_suspend_handler);
#endif
		smi130_gyro_input_cleanup(client_data);
		mutex_lock(&client_data->mutex_op_mode);
		SMI_GYRO_CALL_API(get_mode)(&op_mode);
		if (SMI_GYRO_VAL_NAME(MODE_NORMAL) == op_mode) {
			cancel_delayed_work_sync(&client_data->work);
			PINFO("cancel work");
		}
		mutex_unlock(&client_data->mutex_op_mode);

		err = SMI_GYRO_CALL_API(set_mode)(
				SMI_GYRO_VAL_NAME(MODE_SUSPEND));
		smi130_gyro_delay(SMI_GYRO_I2C_WRITE_DELAY_TIME);

		sysfs_remove_group(&client_data->input->dev.kobj,
				&smi_gyro_attribute_group);
		smi_gyro_input_destroy(client_data);
		kfree(client_data);
		smi_gyro_client = NULL;
	}

	return err;
}

static const struct i2c_device_id smi_gyro_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, smi_gyro_id);
static const struct of_device_id smi130_gyro_of_match[] = {
	{ .compatible = "smi130_gyro", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, smi130_gyro_of_match);

static struct i2c_driver smi_gyro_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
		.of_match_table = smi130_gyro_of_match,
	},
	.class = I2C_CLASS_HWMON,
	.id_table = smi_gyro_id,
	.probe = smi_gyro_probe,
	.remove = smi_gyro_remove,
	.shutdown = smi_gyro_shutdown,
};

static int __init SMI_GYRO_init(void)
{
	return i2c_add_driver(&smi_gyro_driver);
}

static void __exit SMI_GYRO_exit(void)
{
	i2c_del_driver(&smi_gyro_driver);
}

MODULE_AUTHOR("contact@bosch-sensortec.com>");
MODULE_DESCRIPTION("SMI_GYRO GYROSCOPE SENSOR DRIVER");
MODULE_LICENSE("GPL v2");

module_init(SMI_GYRO_init);
module_exit(SMI_GYRO_exit);
