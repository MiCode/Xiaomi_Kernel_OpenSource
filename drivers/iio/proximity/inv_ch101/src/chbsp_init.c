// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016-2019, Chirp Microsystems.  All rights reserved.
 *
 * Chirp Microsystems CONFIDENTIAL
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL CHIRP MICROSYSTEMS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "soniclib.h"			// Chirp SonicLib API definitions
#include "chirp_hal.h"
#include "chbsp_init.h"			// header with board-specific defines
#include "chirp_bsp.h"
#include "i2c_hal.h"

u8 chirp_i2c_addrs[] = CHIRP_I2C_ADDRS;
u8 chirp_i2c_buses[] = CHIRP_I2C_BUSES;

/*
 * Here you set the pin masks for each of the prog pins
 */

u32 chirp_pin_enabled[] = { 1, 1, 1, 1, 1, 1};

u32 chirp_pin_prog[] = {
CHIRP0_PROG_0, CHIRP0_PROG_1, CHIRP0_PROG_2,
CHIRP1_PROG_0, CHIRP1_PROG_1, CHIRP1_PROG_2
};

u32 chirp_pin_io[] = {
CHIRP0_INT_0, CHIRP0_INT_1, CHIRP0_INT_2,
CHIRP1_INT_0, CHIRP1_INT_1, CHIRP1_INT_2
};

u32 chirp_pin_io_irq[] = {
CHIRP0_INT_0, CHIRP0_INT_1, CHIRP0_INT_2,
CHIRP1_INT_0, CHIRP1_INT_1, CHIRP1_INT_2
};

/* Callback function pointers */
static ch_timer_callback_t periodic_timer_callback_ptr;
static u16 periodic_timer_interval_ms;

/*!
 * \brief Initialize board hardware
 *
 * \note This function performs all necessary initialization on the board.
 */
void chbsp_board_init(struct ch_group_t *grp_ptr)
{
	/* Initialize group descriptor */
	grp_ptr->num_ports = CHBSP_MAX_DEVICES;
	grp_ptr->num_i2c_buses = CHBSP_NUM_I2C_BUSES;
	grp_ptr->rtc_cal_pulse_ms = CHBSP_RTC_CAL_PULSE_MS;

	ext_int_init();
}

/*!
 * \brief Assert the reset pin
 *
 * This function drives the sensor reset pin low.
 */
void chbsp_reset_assert(void)
{
	ioport_set_pin_level(CHIRP_RST, IOPORT_PIN_LEVEL_LOW); //reset low
}

/*!
 * \brief De-assert the reset pin
 *
 * This function drives the sensor reset pin high.
 */
void chbsp_reset_release(void)
{
	ioport_set_pin_level(CHIRP_RST, IOPORT_PIN_LEVEL_HIGH); //reset high
}


void chbsp_reset_ps_assert(void)
{
	ioport_set_pin_level(CHIRP_RST_PS, IOPORT_PIN_LEVEL_LOW); //reset low
}

/*!
 * \brief De-assert the pulse reset pin
 *
 * This function drives the pulse reset pin high.
 */
void chbsp_reset_ps_release(void)
{
	ioport_set_pin_level(CHIRP_RST_PS, IOPORT_PIN_LEVEL_HIGH); //reset high
}

/*!
 * \brief Assert the PROG pin
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function drives the sensor PROG pin high on the specified port.
 */
void chbsp_program_enable(struct ch_dev_t *dev_ptr)
{
	u8 dev_num = ch_get_dev_num(dev_ptr);

	// select Chirp chip PROG line according to chip number
	ioport_set_pin_level(chirp_pin_prog[dev_num], IOPORT_PIN_LEVEL_HIGH);
}

/*!
 * \brief De-assert the PROG pin
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function drives the sensor PROG pin low on the specified port.
 */
void chbsp_program_disable(struct ch_dev_t *dev_ptr)
{
	u8 dev_num = ch_get_dev_num(dev_ptr);

	// select Chirp chip PROGRAM line according to chip number
	ioport_set_pin_level(chirp_pin_prog[dev_num], IOPORT_PIN_LEVEL_LOW);
}

/*!
 * \brief Configure the Chirp sensor INT pin as an output for one sensor.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function configures the Chirp sensor INT pin as an output
 * (from the perspective of the host system).
 */
void chbsp_set_io_dir_out(struct ch_dev_t *dev_ptr)
{
	u8 dev_num = ch_get_dev_num(dev_ptr);

	if (ch_sensor_is_connected(dev_ptr)) {
		ioport_set_pin_dir(chirp_pin_io[dev_num],
			IOPORT_DIR_OUTPUT);
	}
}

/*!
 * \brief Configure the Chirp sensor INT pin as an input for one sensor.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function configures the Chirp sensor INT pin as an input
 * (from the perspective of the host system).
 */
void chbsp_set_io_dir_in(struct ch_dev_t *dev_ptr)
{
	u8 dev_num = ch_get_dev_num(dev_ptr);

	if (ch_sensor_is_connected(dev_ptr)) {
		ioport_set_pin_dir(chirp_pin_io[dev_num],
			IOPORT_DIR_INPUT);
	}
}

/* Functions supporting controlling int pins of individual sensors
 * (originally only controllable in a group)
 */
void chbsp_io_set(struct ch_dev_t *dev_ptr)
{
	u8 dev_num = ch_get_dev_num(dev_ptr);

	if (ch_sensor_is_connected(dev_ptr)) {
		ioport_set_pin_level(chirp_pin_io[dev_num],
			IOPORT_PIN_LEVEL_HIGH);
	}
}

void chbsp_io_clear(struct ch_dev_t *dev_ptr)
{
	u8 dev_num = ch_get_dev_num(dev_ptr);

	if (ch_sensor_is_connected(dev_ptr)) {
		ioport_set_pin_level(chirp_pin_io[dev_num],
			IOPORT_PIN_LEVEL_LOW);
	}
}

/*!
 * \brief Configure the Chirp sensor INT pins as outputs for a group of sensors
 *
 * \param grp_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 *
 * This function configures each Chirp sensor's INT pin as an output
 * (from the perspective of the host system).
 */
void chbsp_group_set_io_dir_out(struct ch_group_t *grp_ptr)
{
	u8 dev_num;

	for (dev_num = 0; dev_num < ch_get_num_ports(grp_ptr); dev_num++) {
		struct ch_dev_t *dev_ptr = ch_get_dev_ptr(grp_ptr, dev_num);

		if (ch_sensor_is_connected(dev_ptr)) {
			ioport_set_pin_dir(chirp_pin_io[dev_num],
				IOPORT_DIR_OUTPUT); //output pin
		}
	}
}

/*!
 * \brief Configure the Chirp sensor INT pins as inputs for a group of sensors
 *
 * \param dev_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 *
 * \note This function assumes a bidirectional level shifter is interfacing
 */
void chbsp_group_set_io_dir_in(struct ch_group_t *grp_ptr)
{
	u8 dev_num;

	for (dev_num = 0; dev_num < ch_get_num_ports(grp_ptr); dev_num++) {
		struct ch_dev_t *dev_ptr = ch_get_dev_ptr(grp_ptr, dev_num);

		if (ch_sensor_is_connected(dev_ptr)) {
			ioport_set_pin_dir(chirp_pin_io[dev_num],
				IOPORT_DIR_INPUT); //input pin
		}
	}
}

/*!
 * \brief Initialize the I/O pins.
 *
 * \param dev_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 *
 * Configure reset and program pins as outputs. Assert reset and program.
 * Configure sensor INT pin as input.
 */
void chbsp_group_pin_init(struct ch_group_t *grp_ptr)
{
	u8 dev_num;
	int i;

	for (i = 0; i < ARRAY_SIZE(chirp_pin_prog); i++) {
		if (!chirp_pin_enabled[i])
			continue;
		ioport_set_pin_dir(chirp_pin_prog[i], IOPORT_DIR_OUTPUT);
		ioport_set_pin_level(chirp_pin_prog[i], IOPORT_PIN_LEVEL_LOW);
	}

	ioport_set_pin_dir(CHIRP_RST, IOPORT_DIR_OUTPUT); //reset=output
	chbsp_reset_assert();

	for (dev_num = 0; dev_num < grp_ptr->num_ports; dev_num++) {
		struct ch_dev_t *dev_ptr = ch_get_dev_ptr(grp_ptr, dev_num);

		chbsp_program_enable(dev_ptr);
	}

	/* Initialize IO pins */
	chbsp_group_set_io_dir_in(grp_ptr);
}

/*!
 * \brief Set the INT pins low for a group of sensors.
 *
 * \param dev_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 *
 * This function drives the INT line low for each sensor in the group.
 */
void chbsp_group_io_clear(struct ch_group_t *grp_ptr)
{
	u8 dev_num;

	for (dev_num = 0; dev_num < ch_get_num_ports(grp_ptr); dev_num++) {
		struct ch_dev_t *dev_ptr = ch_get_dev_ptr(grp_ptr, dev_num);

		if (ch_sensor_is_connected(dev_ptr)) {
			ioport_set_pin_level(chirp_pin_io[dev_num],
				IOPORT_PIN_LEVEL_LOW);
		}
	}
}

/*!
 * \brief Set the INT pins high for a group of sensors.
 *
 * \param dev_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 *
 * This function drives the INT line high for each sensor in the group.
 */
void chbsp_group_io_set(struct ch_group_t *grp_ptr)
{
	u8 dev_num;

	for (dev_num = 0; dev_num < ch_get_num_ports(grp_ptr); dev_num++) {
		struct ch_dev_t *dev_ptr = ch_get_dev_ptr(grp_ptr, dev_num);

		if (ch_sensor_is_connected(dev_ptr)) {
			ioport_set_pin_level(chirp_pin_io[dev_num],
				IOPORT_PIN_LEVEL_HIGH);
		}
	}
}

/*!
 * \brief Disable interrupts for a group of sensors
 *
 * \param dev_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 *
 * For each sensor in the group, this function disables the host interrupt
 * associated with the Chirp sensor device's INT line.
 */
void chbsp_group_io_interrupt_enable(struct ch_group_t *grp_ptr)
{
	u8 dev_num;

	for (dev_num = 0; dev_num < ch_get_num_ports(grp_ptr); dev_num++) {
		struct ch_dev_t *dev_ptr = ch_get_dev_ptr(grp_ptr, dev_num);

		chbsp_io_interrupt_enable(dev_ptr);
	}
}

/*!
 * \brief Enable the interrupt for one sensor
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function enables the host interrupt associated with the Chirp sensor
 * device's INT line.
 */
void chbsp_io_interrupt_enable(struct ch_dev_t *dev_ptr)
{
	u8 dev_num = ch_get_dev_num(dev_ptr);

	if (ch_sensor_is_connected(dev_ptr)) {
		os_clear_interrupt(chirp_pin_io_irq[dev_num]);
		os_enable_interrupt(chirp_pin_io_irq[dev_num]);
	}
}

/*!
 * \brief Disable interrupts for a group of sensors
 *
 * \param dev_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 *
 * For each sensor in the group, this function disables the host interrupt
 * associated with the Chirp sensor device's INT line.
 */
void chbsp_group_io_interrupt_disable(struct ch_group_t *grp_ptr)
{
	u8 dev_num;

	for (dev_num = 0; dev_num < ch_get_num_ports(grp_ptr); dev_num++) {
		struct ch_dev_t *dev_ptr = ch_get_dev_ptr(grp_ptr, dev_num);

		chbsp_io_interrupt_disable(dev_ptr);
	}
}

/*!
 * \brief Disable the interrupt for one sensor
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function disables the host interrupt associated with the Chirp sensor
 * device's INT line.
 */
void chbsp_io_interrupt_disable(struct ch_dev_t *dev_ptr)
{
	u8 dev_num = ch_get_dev_num(dev_ptr);

	if (dev_ptr->sensor_connected)
		os_disable_interrupt(chirp_pin_io_irq[dev_num]);
}

/*!
 * \brief Set callback routine for Chirp sensor I/O interrupt
 *
 * \param callback_func_ptr	pointer to application function to be called
 *				when interrupt occurs
 *
 * This function sets up the specified callback routine to be called whenever
 * the interrupt associated with the sensor's INT line occurs.
 * The callback routine address in stored in a pointer variable that will later
 * be accessed from within the interrupt handler to call the function.
 *
 * The callback function will be called at interrupt level from the interrupt
 * service routine.
 */
void chbsp_io_callback_set(ch_io_int_callback_t callback_func_ptr)
{
}

/*!
 * \brief Delay for specified number of microseconds
 *
 * \param us	number of microseconds to delay before returning
 *
 * This function waits for the specified number of microseconds before
 * returning to the caller.
 */
void chbsp_delay_us(u32 us)
{
	os_delay_us(us);
}

/*!
 * \brief Delay for specified number of milliseconds.
 *
 * \param ms	number of milliseconds to delay before returning
 *
 * This function waits for the specified number of milliseconds before
 * returning to the caller.
 */
void chbsp_delay_ms(u32 ms)
{
	os_delay_ms(ms);
}

/*!
 * \brief Initialize the host's I2C hardware.
 *
 * \return 0 if successful, 1 on error
 *
 * This function performs general I2C initialization on the host system.
 */
int chbsp_i2c_init(void)
{
	i2c_master_init();
	return 0;
}

int chbsp_i2c_deinit(void)
{
	return 0;
}

/*!
 * \brief Return I2C information for a sensor port on the board.
 *
 * \param dev_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 * \param dev_num	device number within sensor group
 * \param info_ptr	pointer to structure to be filled with I2C config values
 *
 * \return 0 if successful, 1 if error
 *
 * This function returns I2C values in the ch_i2c_info_t structure specified by
 * \a info_ptr.
 * The structure includes three fields.
 *  - The \a address field contains the I2C address for the sensor.
 *  - The \a bus_num field contains the I2C bus number (index).
 *  - The \a drv_flags field contains various bit flags through which the BSP
 *	can inform
 *  SonicLib driver functions to perform specific actions during
 *	I2C I/O operations.
 */
u8 chbsp_i2c_get_info(struct ch_group_t *grp_ptr, u8 io_index,
	struct ch_i2c_info_t *info_ptr)
{
	u8 ret_val = 1;

	if (io_index < CHBSP_MAX_DEVICES) {
		info_ptr->address = chirp_i2c_addrs[io_index];
		info_ptr->bus_num = chirp_i2c_buses[io_index];
		// no special I2C handling by SonicLib driver is needed
		info_ptr->drv_flags = 0;

		ret_val = 0;
	}

	return ret_val;
}

/*!
 * \brief Write bytes to an I2C slave.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 * \param data		data to be transmitted
 * \param num_bytes	length of data to be transmitted
 *
 * \return 0 if successful, 1 on error or NACK
 *
 * This function writes one or more bytes of data to an I2C slave device.
 * The I2C interface must have already been initialized using chbsp_i2c_init().
 */
int chbsp_i2c_write(struct ch_dev_t *dev_ptr, u8 *data, u16 num_bytes)
{
	int bytes = 0;

	if (dev_ptr->i2c_bus_index == 0) {
		bytes = i2c_master_write_register0_sync(dev_ptr->i2c_address,
			num_bytes, data); //I2C bus 0
	} else if (dev_ptr->i2c_bus_index == 1) {
		bytes = i2c_master_write_register1_sync(dev_ptr->i2c_address,
			num_bytes, data); //I2C bus 1
	} else if (dev_ptr->i2c_bus_index == 2) {
		bytes = i2c_master_write_register2_sync(dev_ptr->i2c_address,
			num_bytes, data); //I2C bus 2
	}

	return (bytes != num_bytes);
}

/*!
 * \brief Write bytes to an I2C slave using memory addressing.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 * \param mem_addr	internal memory or register address within device
 * \param data		data to be transmitted
 * \param num_bytes	length of data to be transmitted
 *
 * \return 0 if successful, 1 on error or NACK
 *
 * This function writes one or more bytes of data to an I2C slave device using
 * an internal memory or register address.  The remote device will write
 * \a num_bytes bytes of data starting at internal memory/register address
 * \a mem_addr.
 * The I2C interface must have already been initialized using chbsp_i2c_init().
 */
int chbsp_i2c_mem_write(struct ch_dev_t *dev_ptr, u16 mem_addr, u8 *data,
	u16 num_bytes)
{
	int error = 0;

	if (dev_ptr->i2c_bus_index == 0) {
		// I2C bus 0
		error = i2c_master_write_register0(dev_ptr->i2c_address,
			(unsigned char)mem_addr, num_bytes, data);
	} else if (dev_ptr->i2c_bus_index == 1) {
		// I2C bus 1
		error = i2c_master_write_register1(dev_ptr->i2c_address,
			(unsigned char)mem_addr, num_bytes, data);
	} else if (dev_ptr->i2c_bus_index == 2) {
		// I2C bus 2
		error = i2c_master_write_register2(dev_ptr->i2c_address,
			(unsigned char)mem_addr, num_bytes, data);
	}
	return error;
}

/*!
 * \brief Write bytes to an I2C slave, non-blocking.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 * \param data		pointer to the start of data to be transmitted
 * \param num_bytes	length of data to be transmitted
 *
 * \return 0 if successful, 1 on error or NACK
 *
 * This function initiates a non-blocking write of the specified number
 * of bytes to an I2C slave device.
 *
 * The I2C interface must have already been initialized using chbsp_i2c_init().
 */
int chbsp_i2c_write_nb(struct ch_dev_t *dev_ptr, u8 *data, u16 num_bytes)
{
	// XXX not implemented
	return 1;
}

/*!
 * \brief Write bytes to an I2C slave using memory addressing, non-blocking.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 * \param mem_addr	internal memory or register address within device
 * \param data		pointer to the start of data to be transmitted
 * \param num_bytes	length of data to be transmitted
 *
 * \return 0 if successful, 1 on error or NACK
 *
 * This function initiates a non-blocking write of the specified number of bytes
 * to an I2C slave device, using an internal memory or register address.
 * The remote device will write \a num_bytes bytes of data starting at internal
 * memory/register address \a mem_addr.
 *
 * The I2C interface must have already been initialized using chbsp_i2c_init().
 */
int chbsp_i2c_mem_write_nb(struct ch_dev_t *dev_ptr, u16 mem_addr, u8 *data,
	u16 num_bytes)
{
	// XXX not implemented
	return 1;
}

/*!
 * \brief Read bytes from an I2C slave.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 * \param data		pointer to receive data buffer
 * \param num_bytes	number of bytes to read
 *
 * \return 0 if successful, 1 on error or NACK
 *
 * This function reads the specified number of bytes from an I2C slave device.
 * The I2C interface must have already been initialized using chbsp_i2c_init().
 */
int chbsp_i2c_read(struct ch_dev_t *dev_ptr, u8 *data, u16 num_bytes)
{
	int bytes = 0;
	u8 i2c_addr = ch_get_i2c_address(dev_ptr);
	u8 bus_num = ch_get_i2c_bus(dev_ptr);

	if (bus_num == 0) {
		// I2C bus 0
		bytes = i2c_master_read_register0_sync(i2c_addr, num_bytes,
			data);
	} else if (bus_num == 1) {
		// I2C bus 1
		bytes = i2c_master_read_register1_sync(i2c_addr, num_bytes,
			data);
	} else if (bus_num == 2) {
		// I2C bus 2
		bytes = i2c_master_read_register2_sync(i2c_addr, num_bytes,
			data);
	}
	return (bytes != num_bytes);
}

/*!
 * \brief Read bytes from an I2C slave using memory addressing.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 * \param mem_addr	internal memory or register address within device
 * \param data		pointer to receive data buffer
 * \param num_bytes	number of bytes to read
 *
 * \return 0 if successful, 1 on error or NACK
 *
 * This function reads the specified number of bytes from an I2C slave device,
 * using an internal memory or register address.  The remote device will return
 * \a num_bytes bytes starting at internal memory/register address \a mem_addr.
 *
 * The I2C interface must have already been initialized using chbsp_i2c_init().
 */
int chbsp_i2c_mem_read(struct ch_dev_t *dev_ptr, u16 mem_addr, u8 *data,
	u16 num_bytes)
{
	int error = 1;		// default is error return
	u8 i2c_addr = ch_get_i2c_address(dev_ptr);
	u8 bus_num = ch_get_i2c_bus(dev_ptr);

	if (bus_num == 0) {
		// I2C bus 0
		error = i2c_master_read_register0(i2c_addr,
			(unsigned char)mem_addr, num_bytes, data);
	} else if (bus_num == 1) {
		// I2C bus 1
		error = i2c_master_read_register1(i2c_addr,
			(unsigned char)mem_addr, num_bytes, data);
	} else if (bus_num == 2) {
		// I2C bus 2
		error = i2c_master_read_register2(i2c_addr,
			(unsigned char)mem_addr, num_bytes, data);
	}
	return error;
}

/*!
 * \brief Read bytes from an I2C slave, non-blocking.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 * \param data		pointer to receive data buffer
 * \param num_bytes	number of bytes to read
 *
 * \return 0 if successful, 1 on error or NACK
 *
 * This function initiates a non-blocking read of the specified number of bytes
 * from an I2C slave.
 *
 * The I2C interface must have already been initialized using chbsp_i2c_init().
 */
int chbsp_i2c_read_nb(struct ch_dev_t *dev_ptr, u8 *data, u16 num_bytes)
{
	int error = 0;

	error = chbsp_i2c_read(dev_ptr, data, num_bytes);
	return error;
}

/*!
 * \brief Read bytes from an I2C slave using memory addressing, non-blocking.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 * \param mem_addr	internal memory or register address within device
 * \param data		pointer to receive data buffer
 * \param num_bytes	number of bytes to read
 *
 * \return 0 if successful, 1 on error or NACK
 *
 * This function initiates a non-blocking read of the specified number of bytes
 * from an I2C slave.
 *
 * The I2C interface must have already been initialized using chbsp_i2c_init().
 */
int chbsp_i2c_mem_read_nb(struct ch_dev_t *dev_ptr, u16 mem_addr, u8 *data,
	u16 num_bytes)
{
	int bytes = 0;
	u8 i2c_addr = ch_get_i2c_address(dev_ptr);
	u8 bus_num = ch_get_i2c_bus(dev_ptr);

	if (bus_num == 0) {
		// I2C bus 0
		bytes = i2c_master_read_register0_nb(i2c_addr, num_bytes, data);
	} else if (bus_num == 1) {
		// I2C bus 1
		bytes = i2c_master_read_register1_nb(i2c_addr, num_bytes, data);
	} else if (bus_num == 2) {
		// I2C bus 2
		bytes = i2c_master_read_register2_nb(i2c_addr, num_bytes, data);
	}
	return (bytes != num_bytes);
}

/*!
 * \brief Reset I2C bus associated with device.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function performs a reset of the I2C interface for the specified device.
 */
void chbsp_i2c_reset(struct ch_dev_t *dev_ptr)
{
	u8 bus_num = ch_get_i2c_bus(dev_ptr);

	if (bus_num == 0)			// I2C bus 0
		i2c_master_initialize0();
	else if (bus_num == 1)			// I2C bus 1
		i2c_master_initialize1();
	else if (bus_num == 2)			// I2C bus 2
		i2c_master_initialize2();
}

/*!
 * \brief Initialize periodic timer.
 *
 * \param interval_ms		timer interval, in milliseconds
 * \param callback_func_ptr	address of routine to be called every time the
 *				timer expires
 *
 * \return 0 if successful, 1 if error
 *
 * This function initializes a periodic timer on the board.  The timer is
 * programmed to generate an interrupt after every \a interval_ms milliseconds.
 *
 * The \a callback_func_ptr parameter specifies a callback routine that will be
 * called when the timer expires (and interrupt occurs).
 * The \a chbsp_periodic_timer_handler function will call this function.
 */
u8 chbsp_periodic_timer_init(u16 interval_ms,
	ch_timer_callback_t callback_func_ptr)
{
	/* Save timer interval and callback function */
	periodic_timer_interval_ms = interval_ms;
	periodic_timer_callback_ptr = callback_func_ptr;

	return 0;
}

/*!
 * \brief Enable periodic timer interrupt.
 *
 * This function enables the interrupt associated with the periodic timer
 * initialized by \a chbsp_periodic_timer_init().
 */
void chbsp_periodic_timer_irq_enable(void)
{
}

/*!
 * \brief Disable periodic timer interrupt.
 *
 * This function enables the interrupt associated with the periodic timer
 * initialized by \a chbsp_periodic_timer_init().
 */
void chbsp_periodic_timer_irq_disable(void)
{
}

/*!
 * \brief Start periodic timer.
 *
 * \return 0 if successful, 1 if error
 *
 * This function starts the periodic timer initialized by
 * \a chbsp_periodic_timer_init().
 */
u8 chbsp_periodic_timer_start(void)
{
	return 0;
}

/*!
 * \brief Periodic timer handler.
 *
 * \return 0 if successful, 1 if error
 *
 * This function handles the expiration of the periodic timer, re-arms it and
 * any associated interrupts for the next interval, and calls the callback
 * routine that was registered using \a chbsp_periodic_timer_init().
 */
void chbsp_periodic_timer_handler(void)
{
	ch_timer_callback_t func_ptr = periodic_timer_callback_ptr;

	chbsp_periodic_timer_start();

	if (func_ptr != NULL)
		(*func_ptr)();	// call application timer callback routine

	chbsp_periodic_timer_irq_enable();
}

/*!
 * \brief Turn on an LED on the board.
 *
 * This function turns on an LED on the board.
 *
 * The \a dev_num parameter contains the device number of a specific sensor.
 * This routine will turn on the LED on the Chirp sensor daughterboard that
 * is next to the specified sensor.
 */
void chbsp_led_on(u8 led_num)
{
}

/*!
 * \brief Turn off an LED on the board.
 *
 * This function turns off an LED on the board.
 *
 * The \a dev_num parameter contains the device number of a specific sensor.
 * This routine will turn off the LED on the Chirp sensor daughterboard that
 * is next to the specified sensor.
 */
void chbsp_led_off(u8 led_num)
{
}

u32 chbsp_timestamp_ms(void)
{
	return (u32)os_timestamp_ms();
}

void chbsp_print_str(char *str)
{
	os_print_str(str);
}

