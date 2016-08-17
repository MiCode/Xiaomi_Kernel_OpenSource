/*
* Copyright (C) 2012 Invensense, Inc.
* Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

/**
 *  @addtogroup  DRIVERS
 *  @brief       Hardware drivers.
 *
 *  @{
 *      @file    mpu.h
 *      @brief   mpu definition
 */

#ifndef __MPU_H_
#define __MPU_H_

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#elif defined LINUX
#include <sys/ioctl.h>
#include <linux/types.h>
#else
#include "mltypes.h"
#endif

/*********************************************************************/
/* DEBUG INTERFACE
*	Adds SYSFS attributes to read/write MPU registers from ADB
*	shell:
*	dbg_reg	= REGISTER
*		A write to dbg_reg sets the device register to use.
*		A read reads the current value of dbg_reg.
*	dbg_dat = DATA
*		A write to dbg_dat initiates an I2C write transaction
*		to the device register defined by dbg_reg with the
*		data defined by dbg_dat
*		A read to dbg_dat initiates an I2C read transaction
*		to the device register defined by dbg_reg.
*	dbg_i2c_addr = I2C device address
*		If set to 0 (default) the MPU I2C address is used.
*		Other devices can be accessed by setting this to
*		that device's I2C address.  When used after enabling
*		bypass mode, devices behind the MPU can be accessed.
**********************************************************************/
#define	DEBUG_SYSFS_INTERFACE		1

/* Mount maxtices for mount orientation.
 * MTMAT_XXX_CCW_YYY
 *     XXX : mount position. TOP for top and BOT for bottom.
 *     YYY : couter-clockwise rotation angle in degree.
 */
#define MTMAT_TOP_CCW_0		{  1,  0,  0,  0,  1,  0,  0,  0,  1 }
#define MTMAT_TOP_CCW_90	{  0, -1,  0,  1,  0,  0,  0,  0,  1 }
#define MTMAT_TOP_CCW_180	{ -1,  0,  0,  0, -1,  0,  0,  0,  1 }
#define MTMAT_TOP_CCW_270	{  0,  1,  0, -1,  0,  0,  0,  0,  1 }
#define MTMAT_BOT_CCW_0		{ -1,  0,  0,  0,  1,  0,  0,  0, -1 }
#define MTMAT_BOT_CCW_90	{  0, -1,  0, -1,  0,  0,  0,  0, -1 }
#define MTMAT_BOT_CCW_180	{  1,  0,  0,  0, -1,  0,  0,  0, -1 }
#define MTMAT_BOT_CCW_270	{  0,  1,  0,  1,  0,  0,  0,  0, -1 }

/*********************************************************************/
/* Structure and function prototypes				     */
/*********************************************************************/
struct mpu_read_write {
	/* Memory address or register address depending on ioctl */
	__u16 address;
	__u16 length;
	__u8 *data;
};

enum mpuirq_data_type {
	MPUIRQ_DATA_TYPE_MPU_DATA_READY_IRQ,
	MPUIRQ_DATA_TYPE_MPU_FIFO_READY_IRQ,
	MPUIRQ_DATA_TYPE_SLAVE_IRQ,
	MPUIRQ_DATA_TYPE_PM_EVENT,
	MPUIRQ_DATA_TYPE_NUM_TYPES,
};

/* User space PM event notification */
#define MPU_PM_EVENT_SUSPEND_PREPARE (3)
#define MPU_PM_EVENT_POST_SUSPEND    (4)

/**
 * struct mpuirq_data - structure to report what and when
 * @interruptcount	: The number of times this IRQ has occured since open
 * @irqtime		: monotonic time of the IRQ in ns
 * @data_type		: The type of this IRQ enum mpuirq_data_type
 * @data		: Data associated with this IRQ
 */
struct mpuirq_data {
	__u32 interruptcount;
	__s64 irqtime_ns;
	__u32 data_type;
	__s32 data;
};

enum ext_slave_config_key {
	/* TODO: Remove these first six. */
	MPU_SLAVE_CONFIG_ODR_SUSPEND,
	MPU_SLAVE_CONFIG_ODR_RESUME,
	MPU_SLAVE_CONFIG_FSR_SUSPEND,
	MPU_SLAVE_CONFIG_FSR_RESUME,
	MPU_SLAVE_CONFIG_IRQ_SUSPEND,
	MPU_SLAVE_CONFIG_IRQ_RESUME,
	MPU_SLAVE_CONFIG_ODR,
	MPU_SLAVE_CONFIG_FSR,
	MPU_SLAVE_CONFIG_MOT_THS,
	MPU_SLAVE_CONFIG_NMOT_THS,
	MPU_SLAVE_CONFIG_MOT_DUR,
	MPU_SLAVE_CONFIG_NMOT_DUR,
	MPU_SLAVE_CONFIG_IRQ,
	MPU_SLAVE_WRITE_REGISTERS,
	MPU_SLAVE_READ_REGISTERS,
	MPU_SLAVE_CONFIG_INTERNAL_REFERENCE,
	/* AMI 306 specific config keys */
	MPU_SLAVE_PARAM,
	MPU_SLAVE_WINDOW,
	MPU_SLAVE_READWINPARAMS,
	MPU_SLAVE_SEARCHOFFSET,
	/* MPU3050 and MPU6050 Keys */
	MPU_SLAVE_INT_CONFIG,
	MPU_SLAVE_EXT_SYNC,
	MPU_SLAVE_FULL_SCALE,
	MPU_SLAVE_LPF,
	MPU_SLAVE_CLK_SRC,
	MPU_SLAVE_DIVIDER,
	MPU_SLAVE_DMP_ENABLE,
	MPU_SLAVE_FIFO_ENABLE,
	MPU_SLAVE_DMP_CFG1,
	MPU_SLAVE_DMP_CFG2,
	MPU_SLAVE_TC,
	MPU_SLAVE_GYRO,
	MPU_SLAVE_ADDR,
	MPU_SLAVE_PRODUCT_REVISION,
	MPU_SLAVE_SILICON_REVISION,
	MPU_SLAVE_PRODUCT_ID,
	MPU_SLAVE_GYRO_SENS_TRIM,
	MPU_SLAVE_ACCEL_SENS_TRIM,
	MPU_SLAVE_RAM,
	/* -------------------------- */
	MPU_SLAVE_CONFIG_NUM_CONFIG_KEYS
};

/* For the MPU_SLAVE_CONFIG_IRQ_SUSPEND and MPU_SLAVE_CONFIG_IRQ_RESUME */
enum ext_slave_config_irq_type {
	MPU_SLAVE_IRQ_TYPE_NONE,
	MPU_SLAVE_IRQ_TYPE_MOTION,
	MPU_SLAVE_IRQ_TYPE_DATA_READY,
};

/* Structure for the following IOCTS's
 * MPU_CONFIG_GYRO
 * MPU_CONFIG_ACCEL
 * MPU_CONFIG_COMPASS
 * MPU_CONFIG_PRESSURE
 * MPU_GET_CONFIG_GYRO
 * MPU_GET_CONFIG_ACCEL
 * MPU_GET_CONFIG_COMPASS
 * MPU_GET_CONFIG_PRESSURE
 *
 * @key one of enum ext_slave_config_key
 * @len length of data pointed to by data
 * @apply zero if communication with the chip is not necessary, false otherwise
 *        This flag can be used to select cached data or to refresh cashed data
 *        cache data to be pushed later or push immediately.  If true and the
 *        slave is on the secondary bus the MPU will first enger bypass mode
 *        before calling the slaves .config or .get_config funcion
 * @data pointer to the data to confgure or get
 */
struct ext_slave_config {
	__u8 key;
	__u16 len;
	__u8 apply;
	void *data;
};

enum ext_slave_type {
	EXT_SLAVE_TYPE_GYROSCOPE,
	EXT_SLAVE_TYPE_ACCEL,
	EXT_SLAVE_TYPE_COMPASS,
	EXT_SLAVE_TYPE_PRESSURE,
	/*EXT_SLAVE_TYPE_TEMPERATURE */

	EXT_SLAVE_NUM_TYPES
};
enum secondary_slave_type {
	SECONDARY_SLAVE_TYPE_NONE,
	SECONDARY_SLAVE_TYPE_ACCEL,
	SECONDARY_SLAVE_TYPE_COMPASS,
	SECONDARY_SLAVE_TYPE_PRESSURE,

	SECONDARY_SLAVE_TYPE_TYPES
};

enum ext_slave_id {
	ID_INVALID = 0,
	GYRO_ID_MPU3050,
	GYRO_ID_MPU6050A2,
	GYRO_ID_MPU6050B1,
	GYRO_ID_MPU6050B1_NO_ACCEL,
	GYRO_ID_ITG3500,

	ACCEL_ID_LIS331,
	ACCEL_ID_LSM303DLX,
	ACCEL_ID_LIS3DH,
	ACCEL_ID_KXSD9,
	ACCEL_ID_KXTF9,
	ACCEL_ID_BMA150,
	ACCEL_ID_BMA222,
	ACCEL_ID_BMA250,
	ACCEL_ID_ADXL34X,
	ACCEL_ID_MMA8450,
	ACCEL_ID_MMA845X,
	ACCEL_ID_MPU6050,

	COMPASS_ID_AK8963,
	COMPASS_ID_AK8975,
	COMPASS_ID_AK8972,
	COMPASS_ID_AMI30X,
	COMPASS_ID_AMI306,
	COMPASS_ID_YAS529,
	COMPASS_ID_YAS530,
	COMPASS_ID_HMC5883,
	COMPASS_ID_LSM303DLH,
	COMPASS_ID_LSM303DLM,
	COMPASS_ID_MMC314X,
	COMPASS_ID_HSCDTD002B,
	COMPASS_ID_HSCDTD004A,

	PRESSURE_ID_BMA085,
};

#define INV_PROD_KEY(ver, rev) (ver * 100 + rev)

enum ext_slave_endian {
	EXT_SLAVE_BIG_ENDIAN,
	EXT_SLAVE_LITTLE_ENDIAN,
	EXT_SLAVE_FS8_BIG_ENDIAN,
	EXT_SLAVE_FS16_BIG_ENDIAN,
};

enum ext_slave_bus {
	EXT_SLAVE_BUS_INVALID = -1,
	EXT_SLAVE_BUS_PRIMARY = 0,
	EXT_SLAVE_BUS_SECONDARY = 1
};

/**
 *  struct ext_slave_platform_data - Platform data for mpu3050 and mpu6050
 *  slave devices
 *
 *  @type: the type of slave device based on the enum ext_slave_type
 *         definitions.
 *  @irq: the irq number attached to the slave if any.
 *  @adapt_num: the I2C adapter number.
 *  @bus: the bus the slave is attached to: enum ext_slave_bus
 *  @address: the I2C slave address of the slave device.
 *  @orientation: the mounting matrix of the device relative to MPU.
 *  @irq_data: private data for the slave irq handler
 *  @private_data: additional data, user customizable.  Not touched by the MPU
 *                 driver.
 *
 * The orientation matricies are 3x3 rotation matricies
 * that are applied to the data to rotate from the mounting orientation to the
 * platform orientation.  The values must be one of 0, 1, or -1 and each row and
 * column should have exactly 1 non-zero value.
 */
struct ext_slave_platform_data {
	__u8 type;
	__u32 irq;
	__u32 adapt_num;
	__u32 bus;
	__u8 address;
	__s8 orientation[9];
	void *irq_data;
	void *private_data;
};

struct fix_pnt_range {
	__s32 mantissa;
	__s32 fraction;
};

static inline long range_fixedpoint_to_long_mg(struct fix_pnt_range rng)
{
	return (long)(rng.mantissa * 1000 + rng.fraction / 10);
}

struct ext_slave_read_trigger {
	__u8 reg;
	__u8 value;
};

/**
 *  struct ext_slave_descr - Description of the slave device for programming.
 *
 *  @suspend:	function pointer to put the device in suspended state
 *  @resume:	function pointer to put the device in running state
 *  @read:	function that reads the device data
 *  @init:	function used to preallocate memory used by the driver
 *  @exit:	function used to free memory allocated for the driver
 *  @config:	function used to configure the device
 *  @get_config:function used to get the device's configuration
 *
 *  @name:	text name of the device
 *  @type:	device type. enum ext_slave_type
 *  @id:	enum ext_slave_id
 *  @read_reg:	starting register address to retrieve data.
 *  @read_len:	length in bytes of the sensor data.  Typically  6.
 *  @endian:	byte order of the data. enum ext_slave_endian
 *  @range:	full scale range of the slave ouput: struct fix_pnt_range
 *  @trigger:	If reading data first requires writing a register this is the
 *		data to write.
 *
 *  Defines the functions and information about the slave the mpu3050 and
 *  mpu6050 needs to use the slave device.
 */
struct ext_slave_descr {
	int (*init) (void *mlsl_handle,
		     struct ext_slave_descr *slave,
		     struct ext_slave_platform_data *pdata);
	int (*exit) (void *mlsl_handle,
		     struct ext_slave_descr *slave,
		     struct ext_slave_platform_data *pdata);
	int (*suspend) (void *mlsl_handle,
			struct ext_slave_descr *slave,
			struct ext_slave_platform_data *pdata);
	int (*resume) (void *mlsl_handle,
		       struct ext_slave_descr *slave,
		       struct ext_slave_platform_data *pdata);
	int (*read) (void *mlsl_handle,
		     struct ext_slave_descr *slave,
		     struct ext_slave_platform_data *pdata,
		     __u8 *data);
	int (*config) (void *mlsl_handle,
		       struct ext_slave_descr *slave,
		       struct ext_slave_platform_data *pdata,
		       struct ext_slave_config *config);
	int (*get_config) (void *mlsl_handle,
			   struct ext_slave_descr *slave,
			   struct ext_slave_platform_data *pdata,
			   struct ext_slave_config *config);

	char *name;
	__u8 type;
	__u8 id;
	__u8 read_reg;
	__u8 read_len;
	__u8 endian;
	struct fix_pnt_range range;
	struct ext_slave_read_trigger *trigger;
};


#define NVI_CONFIG_BOOT_AUTO		(0) /* auto detect connection to MPU */
#define NVI_CONFIG_BOOT_MPU		(1) /* connected to MPU */
#define NVI_CONFIG_BOOT_HOST		(2) /* connected to host */
#define NVI_CONFIG_BOOT_MASK		(0x03)

/**
 * struct mpu_platform_data - Platform data for the mpu driver
 * @int_config:		Bits [7:3] of the int config register.
 * @level_shifter:	0: VLogic, 1: VDD
 * @orientation:	Orientation matrix of the gyroscope
 * @sec_slave_type:     secondary slave device type, can be compass, accel, etc
 * @sec_slave_id:       id of the secondary slave device
 * @secondary_i2c_address: secondary device's i2c address
 * @secondary_orientation: secondary device's orientation matrix
 * @config: the selection determines the device behavior.
 *          Select from the NVI_CONFIG_BOOT_ defines.
 *
 * Contains platform specific information on how to configure the MPU3050 to
 * work on this platform.  The orientation matricies are 3x3 rotation matricies
 * that are applied to the data to rotate from the mounting orientation to the
 * platform orientation.  The values must be one of 0, 1, or -1 and each row and
 * column should have exactly 1 non-zero value.
 */
struct mpu_platform_data {
	__u8 int_config;
	__u8 level_shifter;
	__s8 orientation[9];
	enum secondary_slave_type sec_slave_type;
	enum ext_slave_id sec_slave_id;
	__u16 secondary_i2c_addr;
	__u8 secondary_read_reg;
	__s8 secondary_orientation[9];
	__u8 key[16];
	__u8 config;
};

/**
 * struct nvi_mpu_port: Allows an external driver to use the MPU
 *    auxiliary I2C master by providing the details for the I2C
 *    polling of a device connected to the MPU.
 * - addr: The 6:0 I2C address of the device connected to
 *      the MPU.
 *      7:7 = 0 if the port is to do write transactions.
 *          = 1 if the port is to do read transactions.
 * - reg: The device register the I2C transaction will
 *      use.
 * - ctrl: The number of consecutive registers to read in
 *      3:0. If the port is to do write transactions then this
 *      value must be 1.  See MPU documentation for the other
 *      bits in I2C_SLVx_CTRL that can be applied by this byte.
 * - data_out: The data byte written if the port is configured
 *      to do writes (addr 7:7 = 0).
 * - delay_ms: The polling delay time between I2C transactions
 *      in ms.  Note that the MPU HW only supports one delay
 *      time so the longest delay of all the MPU ports enabled
 *      is used.
 * - delay_us: The delay at which the read data is reported.
 * - shutdown_bypass: set if a connection to the host is needed
 *      when the system is shutdown.  The MPU API will be
 *      disabled as part of its shutdown but it will enable the
 *      bypass if this is true.
 * - *handler: The pointer to the function called when the data
 *      is available.  This can be NULL if the port is
 *      configured for write transactions.
 *      The function is called with the following parameters:
 *      - *data: The pointer to the data to read.
 *      - length: The number of bytes to be read (same value as
 *           length above).
 *      - timestamp: The timestamp of when the data was polled.
 *      - *pointer: A generic pointer defined next below.  Note
 *           that this can be NULL if this will be a write I2C
 *           transaction.
 * - *ext_driver: A generic pointer that can be used by the
 *      external driver.  Note that this is specifically for the
 *      external driver and not used by the MPU.
 */
struct nvi_mpu_port {
	u8 addr;
	u8 reg;
	u8 ctrl;
	u8 data_out;
	unsigned int delay_ms;
	unsigned long delay_us;
	bool shutdown_bypass;
	void (*handler)(u8 *data, unsigned int len,
			long long timestamp, void *ext_driver);
	void *ext_driver;
};

/**
 * Expected use of the nvi_mpu_ routines are as follows:
 * - nvi_mpu_dev_valid: Use to validate whether a device is
 *      connected to the MPU.
 * - nvi_mpu_port_alloc: Request a connection to the device.  If
 *      successful, a port number will be returned to identify
 *      the connection.  The port number is then used for all
 *      further communication with the connection.
 * - nvi_mpu_port_free: Use to close the port connection.
 * - nvi_mpu_enable: Use to enable/disable a port.
 *      The enable and FIFO enable is disabled by default so
 *      this will be required after a port is assigned.
 * - nvi_mpu_delay_us: Use to set the sampling rate in
 *      microseconds.  The fastest rate of all the enabled MPU
 *      devices will be used that does not exceed the
 *      nvi_mpu_delay_ms setting of an enabled device.
 * - nvi_mpu_delay_ms: Use to change the port polling delay at
 *      runtime. There is only one HW delay so the delay used
 *      will be the longest delay of all the enabled ports.
 *      This is separate from the sampling rate
 *      (nvi_mpu_delay_us).  See function notes below.
 * - nvi_mpu_data_out: Use to change the data written at runtime
 *      for ports that are configured as I2C write transactions.
 * - nvi_mpu_bypass request/release: Use to connect/disconnect
 *      the MPU host from the device.  When bypass is enabled,
 *      the connection from the device to the MPU will then be
 *      connected to the host (that the MPU is connected to).
 *      This is a global connection switch affecting all ports
 *      so a mechanism is in place of whether the request is
 *      honored or not.  See the function notes for
 *      nvi_mpu_bypass_request.
 */

/**
 * Use to validate a device connected to the MPU I2C master.
 * The function works by doing a single byte read or write to
 * the device and detecting a NACK.  Typically, the call would
 * be set up to read a byte ID of the device.
 * @param struct nvi_mpu_port *nmp
 *           Only the following is needed in nmp:
 *           - addr
 *           - reg
 *           - ctrl
 *           - data_out if a write transaction
 * @param *val: pointer for read data.  Can be NULL if write.
 * @return int error
 *            Possible return value or errors are:
 *            - 0: device is connected to MPU.
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -ENODEV: The device is not connected to the MPU.
 *            - -EINVAL: Problem with input parameters.
 *            - -EIO: The device is connected but responded with
 *                 a NACK.
 */
int nvi_mpu_dev_valid(struct nvi_mpu_port *nmp, u8 *data);

/**
 * Request a port.
 * @param struct nvi_mpu_port *nmp
 *           - addr: device I2C address 6:0.
 *                7:7 = 0 if the port is to do writes.
 *                7:7 = 1 if the port is to do reads.
 *           - reg: the starting register to write or read.
 *           - ctrl: number of bytes to read.  Use 1 if port
 *                is configured to do writes.
 *           - data_out: only valid if port is configured to do
 *                writes.
 *           - delay: polling delay
 *           - handler: function to call when data is read. This
 *                should be NULL if the port is configured to do
 *                writes.
 *           - ext_driver: this pointer is passed in handler for
 *                use by external driver.  This should be NULL
 *                if the port is configured for writes.
 * @return int error/port id
 *            if return >= 0 then this is the port ID.  The ID
 *            will have a value of 0 to 3 (HW has 4 ports).
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -ENODEV: A port is not available.  The only way
 *                 to resolve this error is for a port to be
 *                 freed.
 *            - -EINVAL: Problem with input parameters.
 */
int nvi_mpu_port_alloc(struct nvi_mpu_port *nmp);

/**
 * Remove a port.
 * @param port
 * @return int error
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -EINVAL: Problem with input parameters.
 */
int nvi_mpu_port_free(int port);

/**
 * Enable/disable a port.
 * @param port
 * @param enable
 * @return int error
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -EINVAL: Problem with input parameters.
 */
int nvi_mpu_enable(int port, bool enable, bool fifo_enable);

/**
 * Use to change the ports sampling delay in microseconds. The
 * hardware only supports one sampling rate so the shortest time
 * is used among all enabled ports, accelerometer, and gyro. If
 * the requested rate is longer than the actual rate and the
 * port is configured for reads, the data will be reported at
 * the requested rate skipping the data polled at the faster
 * rate.  Setting this to zero causes other enabled devices to
 * determine the sampling rate.  If there are no other enabled
 * devices, then the MPU default rate is used.
 * @param port
 * @param delay_us
 * @return int error
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -EINVAL: Problem with input parameters.
 */
int nvi_mpu_delay_us(int port, unsigned long delay_us);

/**
 * Use to change the ports polling delay in milliseconds.
 * A delay value of 0 disables the delay for that port.  The
 * hardware only supports one delay value so the largest request
 * of all the enabled ports is used. The polling delay is in
 * addition to the sampling delay (nvi_mpu_delay_us).  This is
 * typically used to guarantee a delay after an I2C write to a
 * device to allow the device to process the request and be read
 * by another port before another write at the sampling delay.
 *
 * @param port
 * @param delay_ms
 * @return int error/delay used or 0 if request is to disable
 *            and is successful.
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -EINVAL: Problem with input parameters.
 */
int nvi_mpu_delay_ms(int port, u8 delay_ms);

/**
 * Use to change the data written to the sensor.
 * @param port
 * @param data_out is the new data to be written
 * @return int error
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -EINVAL: Problem with input parameters.
 */
int nvi_mpu_data_out(int port, u8 data_out);

/**
 * Enable/disable the MPU bypass mode.  When enabled, the MPU
 * will connect its auxiliary I2C ports to the host.  This is
 * typically used to initialize a device that requires more I2C
 * transactions than the automated port polling can offer.
 * EVERY nvi_mpu_bypass_request call must be balanced with a
 * nvi_mpu_bypass_release call!
 * A bypass request does not need a following ~enable call.  The
 * release call will automatically handle the correct bypass
 * enable setting. The request locks the bypass setting if
 * successful.  The release unlocks and restores the setting if
 * need be.  Although odd, the purpose of the request call with
 * the enable cleared to false is to allow an external driver to
 * access its device that would normally conflict with a device
 * behind the MPU.  Note that this call must not be a permanent
 * solution, i.e. delayed or no release call.
 * When the MPU is in a shutdown state the return error will be
 * -EPERM and bypass will be enabled to allow access from the
 * host to the devices connected to the MPU for their own
 * shutdown needs.
 * @param enable
 * @return int error: calls that return with an error must not
 *            be balanced with a release call.
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 */
int nvi_mpu_bypass_request(bool enable);

/**
 * See the nvi_mpu_bypass_request notes.
 * @return int 0: Always returns 0.  The call return should be
 *         void but for backward compatibility it returns 0.
 */
int nvi_mpu_bypass_release(void);

#endif	/* __MPU_H_ */
