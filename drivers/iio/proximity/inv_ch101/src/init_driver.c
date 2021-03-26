// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 InvenSense, Inc.
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

#include <linux/i2c.h>

#include "chbsp_init.h"
#include "chirp_bsp.h"
#include "chirp_hal.h"
#include "i2c_hal.h"
#include "soniclib.h"
#include "init_driver.h"
#include "../ch101_client.h"

/* Forward declarations */
static bool check_sensor(int reg, ioport_pin_t prog_pin, ioport_pin_t int_pin);
static void sensor_int_callback(struct ch_group_t *grp_ptr, u8 dev_num);
static void set_ch101_pitch_catch_config(void);
static u8 display_config_info(struct ch_dev_t *dev_ptr);
static u8 handle_data_ready(struct ch_group_t *grp_ptr);
static void trigger_driver(void);
static void show_config(void);


struct chirp_drv {
	bool	driver_active;
	u32	active_devices;
	u32	data_ready_devices;
	u8	ch101_pitch;
	u16	chirp_odr_ms;
};

struct chirp_drv drv_data = { 0 };

/* Task flag word
 *	This variable contains the DATA_READY_FLAG and IQ_READY_FLAG bit flags
 *	that are set in I/O processing routines.  The flags are checked in the
 *	main() loop and, if set, will cause an appropriate handler function to
 *	be called to process sensor data.
 */
static u32 taskflags;

/* Chirp application version */
#define APP_VERSION_MAJOR	1
#define APP_VERSION_MINOR	0

#define CH_PART_NUMBER 101

/* Select sensor firmware to use
 *	The sensor firmware is specified during the call to ch_init(), by
 *	giving the name (address) of the firmware initialization function
 *	that will be called.
 */

/* SHORT RANGE OPTION:
 *	Uncomment the following line to use different sensor f/w optimized for
 *	short range. The short range firmware has 4 times the resolution, but
 *	only 1/4 the maximum range.  If you use this option, you should redefine
 *	the CHIRP_SENSOR_MAX_RANGE_MM symbol, below, to 250mm or less.
 */

//#define	USE_RANGE			/* use range firmware */
//#define	USE_SHORT_RANGE			/* use short-range firmware */
#ifdef USE_RANGE

#ifndef USE_SHORT_RANGE
/* use CH101 GPR OPEN firmware (normal) */
#define	 CHIRP_SENSOR_FW_INIT_FUNC	ch101_gpr_open_init
#else
/* use CH101 GPR SR OPEN firmware (short range) */
#define	 CHIRP_SENSOR_FW_INIT_FUNC	ch101_gpr_sr_open_init
#endif

#else

#define	 CHIRP_SENSOR_FW_INIT_FUNC	ch101_gpr_init

#endif

#define INT_LINE_LATCH_TIME_US		50	/* latch time in microseconds */


static int total_time_ms;

static bool chirp_present[CHIRP_MAX_NUM_SENSORS] = { 0 };
static u8 connected_sensor_array[CHIRP_MAX_NUM_SENSORS] = { 0 };
static int num_connected_device;
static int num_connected_ch101_device;
static int num_connected_ch201_device;

/* Array of interrupts timestamp in ms*/
static u64 chirp_timestamps_ms[CHIRP_MAX_NUM_SENSORS] = { 0 };

/* Array of structs to hold measurement data, one for each possible device */
static struct chirp_data_t chirp_data[CHIRP_MAX_NUM_SENSORS] = { { 0 } };
static int chirp_data_mode[CHIRP_MAX_NUM_SENSORS];

/* Array of ch_dev_t device descriptors, one for each possible device */
static struct ch_dev_t chirp_devices[CHIRP_MAX_NUM_SENSORS] = { { 0 } };

/* Configuration structure for group of sensors */
static struct ch_group_t chirp_group = { 0 };

/* Chirp sensor group pointer */
static struct ch_group_t *sensor_group_ptr;

static struct ch101_buffer *_ch101_buffer;
static struct ch101_client *_ch101_data;

void set_chirp_buffer(struct ch101_buffer *buffer)
{
	_ch101_buffer = buffer;
}

struct ch101_buffer *get_chirp_buffer(void)
{
	return _ch101_buffer;
}

void set_chirp_data(struct ch101_client *data)
{
	_ch101_data = data;

	if (data)
		set_chirp_gpios(data);
}

struct ch101_client *get_chirp_data(void)
{
	return _ch101_data;
}

int find_sensors(void)
{
	int i;
	bool find = false;
	enum ioport_direction dir;
	bool level;

	ioport_set_pin_dir(CHIRP_RST, IOPORT_DIR_OUTPUT); //reset=output
	ioport_set_pin_level(CHIRP_RST, IOPORT_PIN_LEVEL_HIGH);  //reset=H

	printf("%s: max devices: %x\n", __func__,
		(u32)ARRAY_SIZE(chirp_pin_prog));

	/* check sensors*/
	for (i = 0; i < ARRAY_SIZE(chirp_pin_prog); i++) {
		if (chirp_pin_enabled[i])
			chirp_present[i] = check_sensor(chirp_i2c_buses[i],
				chirp_pin_prog[i], chirp_pin_io[i]);
		find |= chirp_present[i];
	}

	dir = ioport_get_pin_dir(CHIRP_RST);
	level = ioport_get_pin_level(CHIRP_RST);
	printf("%s: Reset - dir: %d level: %d\n", __func__, dir, level);

	return find ? 0 : -ENODEV;
}

static int detect_prog_read(struct ch_dev_t *dev_ptr, u8 reg_addr, u16 *data)
{
	u8 nbytes = CH_PROG_SIZEOF(reg_addr);

	u8 read_data[2];
	u8 message[1] = { 0x7F & reg_addr };

	int ch_err = chdrv_prog_i2c_write(dev_ptr, message, sizeof(message));

	if (!ch_err)
		ch_err = chdrv_prog_i2c_read(dev_ptr, read_data, nbytes);

	if (!ch_err) {
		*data = read_data[0];
		if (nbytes > 1)
			*data |= (((u16)read_data[1]) << 8);
	}

	*data = (0x0a) | ((0x02) << 8);

	return ch_err;
}

static int detect_sensors(struct ch_group_t *grp_ptr)
{
	int ch_err = 0;
	int found = 0;
	u16 prog_stat = UINT16_MAX;
	u8 i = 0;

	printf("%s\n", __func__);

	for (i = 0; i < grp_ptr->num_ports; i++) {
		struct ch_dev_t *dev_ptr = grp_ptr->device[i];

		chbsp_program_enable(dev_ptr);		// assert PROG pin

		found = chdrv_prog_ping(dev_ptr);	// if device found
		printf("found: 0x%02X\n", found);

		ch_err = detect_prog_read(dev_ptr, CH_PROG_REG_STAT,
			&prog_stat);
		if (!ch_err)
			printf("PROG_STAT: 0x%02X\n", prog_stat);

		chbsp_program_disable(dev_ptr);		// de-assert PROG pin
	}
	return ch_err;
}

int is_sensor_connected(int ind)
{
	struct ch_group_t *grp_ptr = &chirp_group;
	struct ch_dev_t *dev_ptr = ch_get_dev_ptr(grp_ptr, ind);

	if (ch_sensor_is_connected(dev_ptr))
		return dev_ptr->op_frequency;
	else
		return 0;
}

static struct ch_group_t *init_group(void)
{
	struct ch_group_t *grp_ptr = &chirp_group;
	u8 chirp_error = 0;
	u8 num_ports = 0;
	u8 dev_num = 0;

/* Initialize board hardware functions
 *	This call to the board support package (BSP) performs all necessary
 *	hardware initialization for the application to run on this board.
 *	This includes setting up memory regions, initialization clocks and
 *	peripherals (including I2C and serial port), and any
 *	processor-specific startup sequences.
 *
 *	The chbsp_board_init() function also initializes fields within the
 *	sensor group descriptor, including number of supported sensors and
 *	the RTC clock calibration pulse length.
 */

	/* Make local copy of group pointer */
	sensor_group_ptr = grp_ptr;

	printf("%s: sensor_group_ptr: %p\n", __func__, sensor_group_ptr);

	chbsp_board_init(grp_ptr);

	printf("CH-%d driver", CH_PART_NUMBER);
	printf("	 Version: %u.%u\n\n",
		APP_VERSION_MAJOR, APP_VERSION_MINOR);
	printf("	 num_ports: %d\n", grp_ptr->num_ports);
	printf("	 num_i2c_buses: %d\n", grp_ptr->num_i2c_buses);

	/* Get the number of (possible) sensor devices on the board
	 *	Set by the BSP during chbsp_board_init()
	 */
	num_ports = ch_get_num_ports(grp_ptr);

/* Initialize sensor descriptors.
 *	This loop initializes each (possible) sensor's ch_dev_t descriptor,
 *	although we don't yet know if a sensor is actually connected.
 *
 *	The call to ch_init() specifies the sensor descriptor, the sensor
 *	group it will be added to, the device number within the group, and
 *	the sensor firmware initialization routine that will be used.
 *	(The sensor firmware selection effectively specifies whether it is
 *	a CH101 or CH201 sensor, as well as the exact feature set.)
 */
	printf("Initializing sensor(s)... ");
	num_connected_device = 0;
	num_connected_ch101_device  = 0;
	num_connected_ch201_device = 0;

	for (dev_num = 0; dev_num < num_ports; dev_num++) {
		if (chirp_pin_enabled[dev_num]) {
			// init struct in array
			struct ch_dev_t *dev_ptr = &chirp_devices[dev_num];

			/* Init device descriptor
			 * Note that this assumes all sensors will use the same
			 * sensor firmware.
			 */
			chirp_error |= ch_init(dev_ptr, grp_ptr, dev_num,
						CHIRP_SENSOR_FW_INIT_FUNC);
			dev_ptr->sensor_connected = chirp_present[dev_num];
			printf("dev_num: %d, sensor_connected: %d\n",
				dev_num, dev_ptr->sensor_connected);

			if (dev_ptr->sensor_connected) {
				connected_sensor_array[num_connected_device++] =
					dev_num;

				if (dev_num < 3)
					num_connected_ch101_device++;
				else
					num_connected_ch201_device++;
			}
		}
	}

	return grp_ptr;
}

void test_detect(void)
{
	struct ch_group_t *grp_ptr = init_group();

	detect_sensors(grp_ptr);

	printf("start group... ");
	ch_group_start(grp_ptr);
	printf("end group... ");
}

void init_driver(void)
{
	u8 chirp_error = 0;
	u8 num_ports = 0;
	u8 dev_num = 0;

	struct ch_group_t *grp_ptr = init_group();

	/* Start all sensors.
	 * The ch_group_start() function will search each port (that was
	 * initialized above) for a sensor. If it finds one, it programs it(with
	 * the firmware specified above during ch_init()) and waits for it to
	 * perform a self-calibration step.  Then, once it has found all the
	 * sensors, ch_group_start() completes a timing reference calibration by
	 * applying a pulse of known length to the sensor's INT line.
	 */
	if (chirp_error == 0) {
		printf("starting group... ");
		chirp_error = ch_group_start(grp_ptr);
	}

	if (chirp_error == 0)
		printf("OK\n");
	else
		printf("starting group FAILED: %d\n", chirp_error);
	printf("\n");

	/* Get and display the initialization results for each connected sensor.
	 *  This loop checks each device number in the sensor group to determine
	 *  if a sensor is actually connected.  If so, it makes a series of
	 *  function calls to get different operating values, including the
	 *  operating frequency, clock calibration values, and firmware version.
	 */
	num_ports = ch_get_num_ports(grp_ptr);

	printf("num_ports: %d\n", num_ports);
	printf("Sensor\tType \t	Freq\t\t RTC Cal \tFirmware\n");

	for (dev_num = 0; dev_num < num_ports; dev_num++) {
		struct ch_dev_t *dev_ptr = ch_get_dev_ptr(grp_ptr, dev_num);

		if (ch_sensor_is_connected(dev_ptr)) {
			printf("num_rx_sample=%d\n",
				ch_get_num_samples(dev_ptr));
			printf("%d\tCH%d\t %u Hz\t%u@%u ms\t%s\n", dev_num,
				ch_get_part_number(dev_ptr),
				ch_get_frequency(dev_ptr),
				ch_get_rtc_cal_result(dev_ptr),
				ch_get_rtc_cal_pulselength(dev_ptr),
				ch_get_fw_version_string(dev_ptr));
		}
	}
	printf("\n");

	// Register callback function to be called when Chirp sensor interrupts
	ch_io_int_callback_set(grp_ptr, sensor_int_callback);
}

void start_driver(int period_ms, int time_ms)
{
	printf("%s\n", __func__);
	total_time_ms = time_ms;
	drv_data.chirp_odr_ms = period_ms;
	trigger_driver();
}

void stop_driver(void)
{
	printf("%s\n", __func__);
	drv_data.driver_active = 0;
}

static void ext_int_handler(u32 gpio_pin)
{
	struct ch_group_t *grp_ptr = &chirp_group;
	ch_io_int_callback_t func_ptr =
		sensor_group_ptr ? sensor_group_ptr->io_int_callback :
		NULL;
	u8 pin_found = 0;
	u8 idx;

//	printf("%s: pin: %d func_ptr: %px\n",
//		__func__, gpio_pin, func_ptr);

	if (sensor_group_ptr == NULL) {
		sensor_group_ptr = grp_ptr;
		func_ptr = sensor_group_ptr->io_int_callback;
		printf("%s: pin: %d func_ptr: %p\n",
			__func__, gpio_pin, func_ptr);
	}

	/* Clear the interrupt */
	// set to low level
	ioport_set_pin_level(gpio_pin, IOPORT_PIN_LEVEL_LOW);
//	// set pin direction as output
//	ioport_set_pin_dir(gpio_pin, IOPORT_DIR_OUTPUT);

	// Drive this INT line down for latching low via TI level shifter
	// (TI TXB0104QPWRQ1)
	chbsp_delay_us(INT_LINE_LATCH_TIME_US);

	// set pin direction as input
	ioport_set_pin_dir(gpio_pin, IOPORT_DIR_INPUT);

	/* Check if this pin is a Chirp sensor, and notify application */
	if (func_ptr != NULL) {
		for (idx = 0; idx < CHBSP_MAX_DEVICES; idx++) {
//			printf("%s: idx: %d chirp_pin_io[idx]: %d\n",
//				__func__, idx, chirp_pin_io[idx]);

			if (gpio_pin == chirp_pin_io[idx]) {
				pin_found = 1;
				break;
			}
		}

		if (pin_found) {
			if (func_ptr != NULL) {
				// Call application callback function - pass I/O
				// index to identify interrupting device
				(*func_ptr)(sensor_group_ptr, idx);
			}
		} else {
			printf("%s: %d pin not found\n", __func__, gpio_pin);
		}
	}
}

void ext_ChirpINT0_handler(int index)
{
	ext_int_handler(chirp_pin_io[index]);
}

static bool check_sensor(int reg, ioport_pin_t prog_pin, ioport_pin_t int_pin)
{
	u8 sig_bytes[2];
	bool good;
	const char *ch;
	unsigned char r;
	bool level;

	sig_bytes[0] = 0;
	sig_bytes[1] = 0;

	/* check sensor */
	ioport_set_pin_dir(prog_pin, IOPORT_DIR_OUTPUT);
	ioport_set_pin_level(prog_pin, IOPORT_PIN_LEVEL_HIGH);

	r = CH_I2C_ADDR_PROG;

	printf("%s: prog_pin: %d\n", __func__, prog_pin);

	if (reg == 0)
		i2c_master_read_register0(r, 0x00, 2, sig_bytes);
	else if (reg == 1)
		i2c_master_read_register1(r, 0x00, 2, sig_bytes);
	else if (reg == 2)
		i2c_master_read_register2(r, 0x00, 2, sig_bytes);

	good = ((sig_bytes[0] == CH_SIG_BYTE_0) &&
		(sig_bytes[1] == CH_SIG_BYTE_1));

	printf("%s: addr: %x sig: %02x %02x\n",
		__func__, r, sig_bytes[0], sig_bytes[1]);

	printf("%s: good: %d\n", __func__, good);

	ch = "";
	if (prog_pin == CHIRP0_PROG_0)
		ch = "CH-0";
	else if (prog_pin == CHIRP0_PROG_1)
		ch = "CH-1";
	else if (prog_pin == CHIRP0_PROG_2)
		ch = "CH-2";
	else if (prog_pin == CHIRP1_PROG_0)
		ch = "CH-3";
	else if (prog_pin == CHIRP1_PROG_1)
		ch = "CH-4";
	else if (prog_pin == CHIRP1_PROG_2)
		ch = "CH-5";
	else if (prog_pin == CHIRP2_PROG_0)
		ch = "CH-6";
	else if (prog_pin == CHIRP2_PROG_1)
		ch = "CH-7";
	else if (prog_pin == CHIRP2_PROG_2)
		ch = "CH-8";

	printf("Chirp sensor I2C_%d %s %02X %s found\n", reg, ch, prog_pin,
		good ? "" : " not");

	ioport_set_pin_level(prog_pin, IOPORT_PIN_LEVEL_LOW);

	/* check int pin */
	ioport_set_pin_dir(int_pin, IOPORT_DIR_OUTPUT);
	ioport_set_pin_level(int_pin, IOPORT_PIN_LEVEL_HIGH);

	chbsp_delay_us(10); // Pulse needs to be a minimum of 800ns long

	level = ioport_get_pin_level(int_pin);
	if (!level) {
		printf("Chirp sensor I2C_%d %s INT %d error\n",
			reg, ch, int_pin);
		good = false;

	}
	ioport_set_pin_level(int_pin, IOPORT_PIN_LEVEL_LOW);

	return good;
}

static void set_ch101_pitch_catch_config(void)
{
	u8 num_ports;
	u8 dev_num;
	struct ch_group_t *grp_ptr;
	u8 last_ch101_pitch;
	int count;
	u8 ret_val;
	bool excluded = false;

	grp_ptr = &chirp_group;
	num_ports = ch_get_num_ports(grp_ptr);
	last_ch101_pitch = drv_data.ch101_pitch;

	//printf("%s: last_ch101_pitch: %d\n", __func__, last_ch101_pitch);

	count = 0;
	if (num_connected_ch101_device) {
		do {
			drv_data.ch101_pitch = (drv_data.ch101_pitch + 1)
				% num_connected_ch101_device;
		} while (excluded == true && ++count < CHIRP_MAX_NUM_SENSORS);

		if (count >= CHIRP_MAX_NUM_SENSORS)
			return;

		for (dev_num = 0; dev_num < 3; dev_num++) {
			// init struct in array
			enum ch_mode_t mode;
			struct ch_dev_t *dev_ptr = &chirp_devices[dev_num];

			if (dev_num ==
				connected_sensor_array[drv_data.ch101_pitch])
				mode = CH_MODE_TRIGGERED_TX_RX;
			else if (dev_num ==
				connected_sensor_array[last_ch101_pitch])
				mode = CH_MODE_TRIGGERED_RX_ONLY;
			else
				continue;

			ret_val = ch_set_mode(dev_ptr, mode);
			if (!ret_val)
				dev_ptr->mode = mode;
		}
	}

	if (num_connected_ch201_device) {
		for (dev_num = 3; dev_num < num_ports; dev_num++) {
			// init struct in array
			enum ch_mode_t mode;
			struct ch_dev_t *dev_ptr = &chirp_devices[dev_num];

			mode = CH_MODE_TRIGGERED_TX_RX;
			//printf("%s: mode: %02x\n", __func__, mode);

			ret_val = ch_set_mode(dev_ptr, mode);
			if (!ret_val)
				dev_ptr->mode = mode;
		}
	}
	show_config();
}

/*
 * display_config_info() - display the configuration values for a sensor
 *
 * This function displays the current configuration settings for an individual
 * sensor.  The operating mode, maximum range, and static target rejection
 * range (if used) are displayed.
 *
 * For CH201 sensors only, the multiple detection threshold values are also
 * displayed.
 */
static u8 display_config_info(struct ch_dev_t *dev_ptr)
{
	struct ch_config_t read_config;
	struct ch_thresh_t *tr;
	u8 chirp_error;
	int i = 0;

	u8 dev_num = ch_get_dev_num(dev_ptr);

	/* Read configuration values for the device into ch_config_t structure*/
	chirp_error = ch_get_config(dev_ptr, &read_config);

	if (!chirp_error) {
		char *mode_string;

		switch (read_config.mode) {
		case CH_MODE_IDLE:
			mode_string = "IDLE";
			break;
		case CH_MODE_FREERUN:
			mode_string = "FREERUN";
			break;
		case CH_MODE_TRIGGERED_TX_RX:
			mode_string = "TRIGGERED_TX_RX";
			break;
		case CH_MODE_TRIGGERED_RX_ONLY:
			mode_string = "TRIGGERED_RX_ONLY";
			break;
		default:
			mode_string = "UNKNOWN";
		}

		/* Display sensor number, mode and max range */
		printf("Sensor %d:\tmax_range=%dmm \tmode=%s  ", dev_num,
			read_config.max_range, mode_string);

		/* Display static target rejection range, if used */
		if (read_config.static_range != 0) {
			printf("static_range=%d samples",
				read_config.static_range);
		}

		/* Display detection thresholds (only supported on CH201) */
		if (ch_get_part_number(dev_ptr) == CH201_PART_NUMBER) {
			struct ch_thresholds_t read_thresholds;

			/* Get threshold values in structure */
			chirp_error = ch_get_thresholds(dev_ptr,
				&read_thresholds);

			if (!chirp_error) {
				printf("\n  Detection thresholds:\n");
				for (i = 0; i < CH_NUM_THRESHOLDS; i++) {
					tr = &read_thresholds.threshold[i];
					printf(" %d\tstart: %2d\tlevel: %d\n",
						i,
						tr->start_sample,
						tr->level);
				}
			} else {
				printf(" Device %d: Error ch_get_thresholds()",
					dev_num);
			}
		}
		printf("\n");

	} else {
		printf(" Device %d: Error during ch_get_config()\n", dev_num);
	}

	return chirp_error;
}

static void show_config(void)
{
	struct ch_group_t *grp_ptr = &chirp_group;
	u8 num_ports = ch_get_num_ports(grp_ptr);
	u8 dev_num = 0;

	printf("Sensors configuration\n");
	for (dev_num = 0; dev_num < num_ports; dev_num++) {
		struct ch_dev_t *dev_ptr = ch_get_dev_ptr(grp_ptr, dev_num);

		if (ch_sensor_is_connected(dev_ptr)) {
			/* Read back and display config settings */
			display_config_info(dev_ptr);
		}
	}
}

void config_driver(void)
{
	int num_samples;
	struct ch_group_t *grp_ptr = &chirp_group;
	u8 chirp_error = 0;
	u8 num_ports = 0;
	u8 dev_num = 0;

	num_ports = ch_get_num_ports(grp_ptr);

	printf("Configuring sensors...\n");
	for (dev_num = 0; dev_num < num_ports; dev_num++) {
		struct ch_config_t dev_config;
		struct ch_dev_t *dev_ptr = ch_get_dev_ptr(grp_ptr, dev_num);

		if (ch_sensor_is_connected(dev_ptr)) {
			/* Select sensor mode
			 * All connected sensors are placed in hardware
			 * triggered mode. The first connected (lowest numbered)
			 * sensor will transmit and receive, all others will
			 * only receive.
			 */

			// add to active device bit mask
			drv_data.active_devices |= (1 << dev_num);
			dev_config.mode = CH_MODE_TRIGGERED_RX_ONLY;

			/* Init config structure with default values */
			dev_config.max_range = CHIRP_SENSOR_MAX_RANGE_MM;
			dev_config.static_range = CHIRP_SENSOR_STATIC_RANGE;
			dev_config.sample_interval =
				CHIRP_SENSOR_SAMPLE_INTERVAL;

			/* Set detection thresholds (CH201 only) */
			dev_config.thresh_ptr = 0;

			/* Apply sensor configuration */
			chirp_error = ch_set_config(dev_ptr, &dev_config);
			if (chirp_error)
				printf("Device %d: Error ch_set_config()\n",
					dev_num);

			num_samples = ch_get_num_samples(dev_ptr);
			printf("ch101: get sample=%d\n", num_samples);
			if (num_samples > MAX_RX_SAMPLES) {
				ch_set_num_samples(dev_ptr, MAX_RX_SAMPLES);
				dev_ptr->num_rx_samples = MAX_RX_SAMPLES;
			}

			/* Enable sensor interrupt if using free-running mode
			 *	Note that interrupt is automatically enabled if
			 *	using triggered modes.
			 */
			if (!chirp_error &&
				dev_config.mode == CH_MODE_FREERUN)
				chbsp_io_interrupt_enable(dev_ptr);

			/* Turn on an LED to indicate device connected */
			if (!chirp_error)
				chbsp_led_on(dev_num);
		}
	}

	set_ch101_pitch_catch_config();
}

void set_complete(void)
{
	struct ch101_client *data = get_chirp_data();

	if (data->cbk->data_complete)
		data->cbk->data_complete(data);
}

static void trigger_driver(void)
{
	u64 cur_time;
	struct ch_group_t *grp_ptr = &chirp_group;

	u64 last_chirp_time_ms = 0;
	u64 start_time = os_timestamp_ms();

	printf("%s: start\n", __func__);

	taskflags = 0;
	drv_data.driver_active = 1;
	while (drv_data.driver_active) {
		if (taskflags == 0) {
			// 1 ms - put processor in low-power sleep mode
			chbsp_proc_sleep(1);
			cur_time = os_timestamp_ms();
			// 100 ms (10 Hz)
			if (cur_time - last_chirp_time_ms >=
				drv_data.chirp_odr_ms) {
				ch_group_trigger(grp_ptr);
				last_chirp_time_ms = os_timestamp_ms();
			}
		}

		/* Check for sensor data-ready interrupt(s) */
		if (taskflags & DATA_READY_FLAG) {
			printf("%s: taskflags: %02x\n", __func__, taskflags);

			// All sensors have interrupted - handle sensor data
			// clear flag
			taskflags &= ~DATA_READY_FLAG;

			// Disable interrupt unless in free-running mode
			// It will automatically be re-enabled during the next
			// trigger
			chbsp_group_io_interrupt_disable(grp_ptr);

			// read and display measurement
			handle_data_ready(grp_ptr);
			drv_data.data_ready_devices = 0;

			set_ch101_pitch_catch_config();
		}

		cur_time = os_timestamp_ms();
		if (cur_time - start_time >= total_time_ms)
			stop_driver();
	}
	printf("%s: stop\n", __func__);
}

void single_shot_driver(void)
{
	struct ch101_buffer *buffer = get_chirp_buffer();
	struct ch_group_t *grp_ptr = &chirp_group;
	u8 dev_num = 0;
	u8 count = 3;
	bool data_ready = false;

	/* Register callback function to be called when sensor interrupts */
	ch_io_int_callback_set(grp_ptr, sensor_int_callback);

	printf("%s: begin\n", __func__);

	taskflags = 0;
	ch_group_trigger(grp_ptr);

	printf("%s: while\n", __func__);

	data_ready = false;

	while (--count > 0) {
		chbsp_proc_sleep(10); // 10 ms - put processor in sleep mode
		printf("%s: count: %d, taskflags: %02x\n",
			__func__, count, taskflags);

		/* Check for sensor data-ready interrupt(s) */
		if ((taskflags & DATA_READY_FLAG) || (count == 1)) {
			// All sensors have interrupted - handle sensor data
			// clear flag
			taskflags &= ~DATA_READY_FLAG;

			// Disable interrupt unless in free-running mode
			// It will automatically be re-enabled during the next
			// trigger
			chbsp_group_io_interrupt_disable(grp_ptr);

			// read and display measurement
			handle_data_ready(grp_ptr);

			data_ready = true;
			break;
		}
	}

	set_ch101_pitch_catch_config();

	if (data_ready) {
		for (dev_num = 0; dev_num < ch_get_num_ports(grp_ptr);
			dev_num++) {
			struct ch_dev_t *dev_ptr =
				ch_get_dev_ptr(grp_ptr, dev_num);

			if (ch_sensor_is_connected(dev_ptr)) {
				buffer->distance[dev_num] =
					chirp_data[dev_num].range / 32; //mm
				buffer->amplitude[dev_num] =
					chirp_data[dev_num].amplitude;
				buffer->nb_samples[dev_num] =
					chirp_data[dev_num].num_samples;
				buffer->mode[dev_num] =
					chirp_data_mode[dev_num];
				memcpy(buffer->iq_data[dev_num],
					chirp_data[dev_num].iq_data,
					chirp_data[dev_num].num_samples *
					sizeof(struct ch101_iq_data));

				printf("%s: %d: %d %d, %d\n", __func__,
					dev_num,
					buffer->distance[dev_num],
					buffer->amplitude[dev_num],
					buffer->mode[dev_num]);
			}
		}

	} else {
		printf("%s: No data\n", __func__);
		memset(buffer, 0, sizeof(*buffer));
	}

	set_complete();
	drv_data.data_ready_devices = 0;

	printf("%s: end\n\n", __func__);
}

/*
 * sensor_int_callback() - sensor interrupt callback routine
 *
 * This function is called by the board support package's interrupt handler for
 * the sensor's INT line every time that the sensor interrupts.  The device
 * number parameter, dev_num, is used to identify the interrupting device
 * within the sensor group.  (Generally the device number is same as the port
 * number used in the BSP to manage I/O pins, etc.)
 *
 * This callback function is registered by the call to ch_io_int_callback_set()
 * in main().
 */
static void sensor_int_callback(struct ch_group_t *grp_ptr, u8 dev_num)
{
	// time of interrupt in ms
	chirp_timestamps_ms[dev_num] = os_timestamp_ms();

	// add to data-ready bit mask
	drv_data.data_ready_devices |= (1 << dev_num);

	printf("%s: dev: %d data_ready_devices: %02x active_devices: %02x\n",
		__func__,
		dev_num, drv_data.data_ready_devices,
		drv_data.active_devices);

	if (drv_data.data_ready_devices == drv_data.active_devices) {

		/* All active sensors have interrupted
		 * after performing a measurement
		 */
		drv_data.data_ready_devices = 0;

		/* Set data-ready flag - it will be checked in main() loop */
		taskflags |= DATA_READY_FLAG;
	}
}

/*
 * handle_data_ready() - get data from all sensors
 *
 * This routine is called from the main() loop after all sensors have
 * interrupted. It shows how to read the sensor data once a measurement is
 * complete.  This routine always reads out the range and amplitude, and
 * optionally performs either a blocking or non-blocking read of the raw I/Q
 * data.	See the comments in hello_chirp.h for information about the
 * I/Q readout build options.
 *
 * If a blocking I/Q read is requested, this function will read the data from
 * the sensor into the application's "chirp_data" structure for this device
 * before returning.
 *
 * Optionally, if a I/Q blocking read is requested and the OUTPUT_IQ_DATA_CSV
 * build symbol is defined, this function will output the full I/Q data as a
 * series of comma-separated value pairs (Q, I), each on a separate line.  This
 * may be a useful step toward making the data available in an external
 * application for analysis (e.g. by copying the CSV values into a spreadsheet
 * program).
 *
 * If a non-blocking I/Q is read is initiated, a callback routine will be called
 * when the operation is complete.  The callback routine must have been
 * registered using the ch_io_complete_callback_set function.
 */
static u8 handle_data_ready(struct ch_group_t *grp_ptr)
{
	u8 dev_num;
	int error;
	int num_samples = 0;
	u16 start_sample = 0;
	u8 ret_val = 0;

	/* Read and display data from each connected sensor
	 *	This loop will write the sensor data to this application's
	 *	"chirp_data" array.
	 *	Each sensor has a separate chirp_data_t structure in that
	 *	array, so the device number is used as an index.
	 */

	for (dev_num = 0; dev_num < ch_get_num_ports(grp_ptr); dev_num++) {
		struct ch_dev_t *dev_ptr = ch_get_dev_ptr(grp_ptr, dev_num);

		if (ch_sensor_is_connected(dev_ptr)) {
	/* Get measurement results from each connected sensor
	 *	For sensor in transmit/receive mode, report one-way
	 *	echo distance. For sensor(s) in receive-only mode,
	 *	report direct one-way distance from transmitting
	 *	sensor
	 */

			printf("%s: dev_num: %d\n", __func__, dev_num);

			if (ch_get_mode(dev_ptr) == CH_MODE_TRIGGERED_RX_ONLY) {
				chirp_data_mode[dev_num] =
					CH_MODE_TRIGGERED_RX_ONLY;
				error = ch_get_range(dev_ptr,
					CH_RANGE_DIRECT,
					&chirp_data[dev_num].range);
			} else {
				chirp_data_mode[dev_num] =
					CH_MODE_TRIGGERED_TX_RX;
				error = ch_get_range(dev_ptr,
					CH_RANGE_ECHO_ONE_WAY,
					&chirp_data[dev_num].range);
			}

			printf("%s: range: %x\n",
				__func__, chirp_data[dev_num].range);

			if (chirp_data[dev_num].range == CH_NO_TARGET) {
				/* No target object was detected - no range
				 * value, no updated amplitude
				 */
				chirp_data[dev_num].amplitude = 0;
				printf("Port %d: no target found",
					dev_num);
				/* Check if no target condition is due to i2c
				 * read error
				 */
				if (error) {
					printf("Read error detected");
					continue;
				}
			} else {
				/* Target object was successfully detected
				 * (range available). Get the new amplitude
				 * value - it's only updated if range
				 * was successfully measured.
				 */
				error = ch_get_amplitude(dev_ptr,
					&chirp_data[dev_num].amplitude);

				printf("Port %d:  Range: %d  Amplitude: %6u  ",
					dev_num, chirp_data[dev_num].range / 32,
					chirp_data[dev_num].amplitude);
				if (error) {
					printf("Read error detected");
					continue;
				}
			}
			printf("\n\r");

			/* Get number of active samples in this measurement */
			num_samples = ch_get_num_samples(dev_ptr);
			if (num_samples > MAX_NB_SAMPLES)
				num_samples = MAX_NB_SAMPLES;
			chirp_data[dev_num].num_samples = num_samples;

			/* Read full IQ data from device into buffer or queue
			 * read request, based on build-time options
			 */

#ifdef READ_IQ_DATA_BLOCKING
			/* Reading I/Q data in normal, blocking mode */
			if (dev_num < 3) {
				error = ch_get_iq_data(dev_ptr,
				chirp_data[dev_num].iq_data, start_sample,
				num_samples, CH_IO_MODE_BLOCK);

				if (!error) {
					printf("%4d IQ samples", num_samples);
//	{
//		struct ch_iq_sample_t *p = chirp_data[dev_num].iq_data;
//		int i;
//		for (i = 0; i < num_samples; i++, p++)
//			printf("	  %4d IQ: %4d %4d", i, p->i, p->q);
//	}
				} else {
					printf("Error reading %d IQ samples",
					num_samples);
				}
				printf("\n\r");
			}

#elif defined(READ_IQ_DATA_NONBLOCK)
			/* Reading I/Q data in non-blocking mode -
			 * queue a read operation
			 */
			printf("queuing %d IQ samples..", num_samples);

			error = ch_get_iq_data(dev_ptr,
				chirp_data[dev_num].iq_data,
				start_sample, num_samples,
				CH_IO_MODE_NONBLOCK);

			if (!error) {
				// record a pending non-blocking read
				num_queued++;
				printf("OK");
			} else {
				printf("**ERROR**");
			}
#endif  // IQ_DATA_NONBLOCK

			printf("\n\r");
		}
	}

	return ret_val;
}

#define TEST_WRITE_READ 0

#if TEST_WRITE_READ

#define FINALTEST_MEM_TEST_DATA 1
#define FINALTEST_MEM_TEST_PROG 1
#define FINALTEST_MAX_MEM_ERRORS 1

/*!< CH-101 data transfer starting address register address. */
#define CH101_PROG_ADDR	 0x05
/*!< CH-101 data transfer size register address. */
#define CH101_PROG_CNT	  0x07

//! Memory write/read test values
#define FINALTEST_MEM_NUM_PATTERNS				(2)
#define FINALTEST_MEM_TEST_CHAR_1				 (0x5A)
#define FINALTEST_MEM_TEST_CHAR_2				 (0xA5)
#define FINALTEST_PROG_MEM_ADDR					(0xF800)
#define FINALTEST_PROG_MEM_SIZE					128 //2048
#define FINALTEST_DATA_MEM_ADDR					(0x0200)
#define FINALTEST_DATA_MEM_SIZE					128 //2048
#define FINALTEST_MEM_READ_TRANSFER_SIZE		(256)

static uint16_t finaltest_mem_pattern(struct ch_dev_t *self, uint16_t addr,
	uint16_t num_bytes, uint8_t pattern_char, uint16_t *offset_ptr);

static uint16_t finaltest_mem_test(struct ch_dev_t *self)
{
	uint8_t pattern_num;
	uint16_t ret_val = 0;
	uint16_t err_count = 0;
	uint16_t err_offset = 0;
	uint8_t pattern_chars[FINALTEST_MEM_NUM_PATTERNS] = {
			FINALTEST_MEM_TEST_CHAR_1, FINALTEST_MEM_TEST_CHAR_2
		};

#if (FINALTEST_MEM_TEST_DATA)
	 printf("%s: FINALTEST_MEM_TEST_DATA\n", __func__);

	for (pattern_num = 0; pattern_num <
		FINALTEST_MEM_NUM_PATTERNS; pattern_num++)
		err_count += finaltest_mem_pattern(self,
		FINALTEST_DATA_MEM_ADDR,
		FINALTEST_DATA_MEM_SIZE,
		pattern_chars[pattern_num], &err_offset);
#endif

#if (FINALTEST_MEM_TEST_PROG)
	 printf("%s: FINALTEST_MEM_TEST_PROG\n", __func__);

	for (pattern_num = 0; pattern_num <
		FINALTEST_MEM_NUM_PATTERNS; pattern_num++) {

		err_count += finaltest_mem_pattern(self,
		FINALTEST_PROG_MEM_ADDR, FINALTEST_PROG_MEM_SIZE,
			pattern_chars[pattern_num], &err_offset);
	}
#endif

	if (err_count > FINALTEST_MAX_MEM_ERRORS)
		ret_val = 1;		  // report error status

	return ret_val;
}


static uint16_t finaltest_mem_pattern(struct ch_dev_t *self, uint16_t addr,
	uint16_t num_bytes, uint8_t pattern_char, uint16_t *offset_ptr)
{
	uint16_t err_count = 0;
	uint16_t err_offset = 0;
	uint8_t tx_buf[FINALTEST_PROG_MEM_SIZE];// note: assumes that prog &
			// data memory regions are same size, or PROG is larger
	uint8_t rx_buf[FINALTEST_PROG_MEM_SIZE];
	int i;
	int ch_err = 0;
	int num_transfers =
		(num_bytes + (FINALTEST_MEM_READ_TRANSFER_SIZE - 1)) /
				FINALTEST_MEM_READ_TRANSFER_SIZE;
	int bytes_left = num_bytes;		 // remaining bytes to read

	chbsp_program_enable(self);		// assert PROG pin

	// Fill buffer with pattern to write
	for (i = 0; i < num_bytes; i++)
		tx_buf[i] = pattern_char;

	// Write data to device
	ch_err = chdrv_prog_mem_write(self, addr, tx_buf, num_bytes);

	printf("%s: chdrv_prog_mem_write: ch_err=%d", __func__, ch_err);

	// Read back data
	if (!ch_err) {
		for (i = 0; i < num_transfers; i++) {
			int bytes_to_read;

			// read burst command
			uint8_t message[] = { (0x80 | CH_PROG_REG_CTL), 0x09 };

			if (bytes_left > FINALTEST_MEM_READ_TRANSFER_SIZE)
				bytes_to_read =
					FINALTEST_MEM_READ_TRANSFER_SIZE;
			else
				bytes_to_read = bytes_left;

			chdrv_prog_write(self, CH101_PROG_ADDR,
			(addr + (i * FINALTEST_MEM_READ_TRANSFER_SIZE)));
			chdrv_prog_write(self, CH101_PROG_CNT,
				 (FINALTEST_MEM_READ_TRANSFER_SIZE - 1));

			ch_err = chdrv_prog_i2c_write(self,
				message, sizeof(message));
			printf("%s: chdrv_prog_i2c_write: %d",
				__func__, ch_err);

			ch_err |= chdrv_prog_i2c_read(self, &(rx_buf[i *
			FINALTEST_MEM_READ_TRANSFER_SIZE]), bytes_to_read);
			printf("%s: chdrv_prog_i2c_read: %d", __func__, ch_err);

			bytes_left -= bytes_to_read;
		}
	}

	// Check read data
	if (!ch_err) {
		for (i = 0; i < num_bytes; i++) {
			if (rx_buf[i] != tx_buf[i]) {
				err_count++;
				if (err_offset == 0)
					err_offset = i;
			}
		}
	}

	if ((err_count != 0) && (offset_ptr != NULL))
		*offset_ptr = err_offset;

	//wdt_reset();
	// reset watchdog to avoid timeout (data write/read takes approx 100ms)

	printf("%s: err_count: %d", __func__, err_count);

	chbsp_program_disable(self);		// de-assert PROG pin

	return err_count;
}

void test_write_read(void)
{
	struct ch_group_t *grp_ptr = init_group();
	u8 num_ports = ch_get_num_ports(grp_ptr);
	u8 dev_num = 0;

	printf("%s\n", __func__);

	ioport_set_pin_dir(CHIRP_RST, IOPORT_DIR_OUTPUT);
	ioport_set_pin_level(CHIRP_RST, IOPORT_PIN_LEVEL_HIGH);

	/* check sensor */
	ioport_set_pin_dir(CHIRP0_PROG_0, IOPORT_DIR_OUTPUT);
	ioport_set_pin_level(CHIRP0_PROG_0, IOPORT_PIN_LEVEL_HIGH);

	for (dev_num = 0; dev_num < num_ports; dev_num++) {
		struct ch_dev_t *dev_ptr = ch_get_dev_ptr(grp_ptr, dev_num);

		printf("%s: ch_sensor_is_connected: %d\n", __func__,
			ch_sensor_is_connected(dev_ptr));
		if (ch_sensor_is_connected(dev_ptr)) {
			printf("%s: finaltest_mem_test\n", __func__);
			finaltest_mem_test(dev_ptr);
		}
	}

	ioport_set_pin_level(CHIRP0_PROG_0, IOPORT_PIN_LEVEL_LOW);
}

#else

void test_write_read(void)
{
}

#endif
