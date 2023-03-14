/* SPDX-License-Identifier: GPL-2.0 */
/*! \file chirp_bsp.h
 *
 * \brief User-supplied board support package functions to interface Chirp
 * SonicLib to a specific hardware platform.
 *
 * This file defines the I/O interfaces that allow the standard Chirp SonicLib
 * sensor driver functions to manage one or more sensors on a specific hardware
 * platform.  These include functions to initialize and control the various I/O
 * pins connecting the sensor to the host system, the I2C communications
 * interface, timer functions, etc.
 *
 * The board support package developer should not need to modify this file.
 * However, that developer is responsible for implementing these support
 * functions for the desired platform.  Note that some functions are optional,
 * depending on the specific runtime requirements (e.g. is non-blocking I/O
 * required?) or development needs (e.g. is debugging support needed?).
 *
 * \note All functions are marked as REQUIRED, RECOMMENDED, or OPTIONAL in their
 * indvidual descriptions. RECOMMENDED functions are either not used directly by
 * SonicLib (but may be expected by examples and other applications from Chirp)
 * or are only required to support certain operating configurations
 * (e.g. individual device triggering).
 *
 *  #### Organization
 *  The file organization for a BSP is intentionally very flexible, so that you
 *  may efficiently use existing code that supports your hardware or otherwise
 *  use your own organizing preferences.
 *
 *  By convention, the functions that implement the BSP interfaces defined in
 *  chirp_bsp.h are placed in a source file called chbsp_VVV_BBB.c, where "VVV"
 *  is the board vendor name, and "BBB" is the board name.  For example, the BSP
 *  for a Chirp SmartSonic board has a main file called chbsp_chirp_smartsonic.c
 *
 *  #### Required \b chirp_board_config.h Header File
 *
 *  The board support package must supply a header file called
 *  \b chirp_board_config.h containing definitions of two symbols used
 *  in the SonicLib driver functions.
 *
 *  The following symbols must be defined in \b chirp_board_config.h:
 *  - \a CHIRP_MAX_NUM_SENSORS    = maximum number of Chirp sensors
 *  - \a CHIRP_NUM_I2C_BUSES    = number of I2C bus interfaces
 *
 *  The \b chirp_board_config.h file must be in the C pre-processor include path
 *  when you build your application with SonicLib.
 *
 *
 *  #### Callback and Notification Functions
 *
 *  In some cases, the BSP is required to call a function to notify SonicLib or
 *  the application that an event has occurred:
 *
 *  - The BSP's handler routine that detects that a sensor interrupt has
 *    occurred (typically on a GPIO line) must call the application's callback
 *    routine whose address was stored in the \b io_int_callback field in the
 *    ch_group_t group descriptor. The BSP function must pass the device number
 *    of the sensor which interrupted as a parameter.
 *
 *  - If non-blocking I/O is used, the BSP's handler functions which process the
 *    completion of an I/O operation must notify SonicLib that the I/O has
 *    completed by calling the \a ch_io_notify() function. The group pointer and
 *    I2C bus number must be passed as parameters to identify which I/O channel
 *    has finished.
 *
 *
 *  #### Implementation Hints
 *
 * Most of the required functions take a pointer to a ch_dev_t device descriptor
 * structure as a handle to identify the sensor being controlled.  The ch_dev_t
 * structure contains various fields with configuration and operating state
 * information for the device.  In general, these data field values may be
 * obtained using various \a ch_get_XXX() functions provided by the SonicLib
 * driver API, so it should not be necessary to access fields directly.
 *
 * Some functions take a pointer to a ch_group_t (sensor group descriptor)
 * structure as a parameter but must operate on individual sensors.  These
 * functions can be implemented using the \a ch_get_dev_ptr() function to access
 * the ch_dev_t structure describing each individual sensor in the group, based
 * on its device number (I/O index value).  The total number of possible
 * sensor devices in a group may be obtained by using the \a ch_get_num_ports()
 * function.
 *
 * Similarly, each sensor's ch_dev_t structure contains a \a dev_num field that
 * may be used to manage the pin assignments for the various sensors, by using
 * it as an index into individual arrays which list the pins assigned to the
 * PROG, INT, and RESET_N lines.  The \a dev_num value for a sensor may be
 * obtained using the \a ch_get_dev_num() function.
 *
 * Often, an action should only be taken on a sensor port if a sensor is present
 * and has been successfully initialized and connected.  The \a
 * ch_sensor_is_connected() function can be used to obtain the connection status
 *
 * These functions are often used together to implement a board support routine,
 * as in the following pseudo-code which shows how to perform an action on all
 * connected devices in a group.  This snippet assumes that a sensor group
 * pointer (struct ch_group_t *) called \a grp_ptr is an input parameter to the
 * function:
 *
 *	struct ch_dev_t *dev_ptr;
 *	u8 dev_num;
 *
 *	for (dev_num = 0; dev_num < ch_get_num_ports(grp_ptr); dev_num++ {
 *		dev_ptr = ch_get_dev_ptr(grp_ptr, dev_num);
 *		if (ch_sensor_is_connected(dev_ptr)) {
 *			 DO WHAT NEEDS DOING FOR THIS CONNECTED DEVICE
 *		}
 *	}
 *
 * The \a dev_num value for each device is specified by the user application as
 * a parameter to \a ch_init().
 *
 * \note Example board support packages that implement the chirp_bsp.h functions
 * are available from Chirp Microsystems for specific platforms.
 * Contact Chirp for more information.
 */

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

#ifndef __CHIRP_BSP_H_
#define __CHIRP_BSP_H_

#include "soniclib.h"

#ifdef __GNUC__
#define WEAK __attribute__((weak))
#else
#define WEAK
#endif

/*!
 * \brief Main hardware initialization
 *
 * This function executes the required hardware initialization sequence for the
 * board being used. This includes clock, memory, and processor setup as well as
 * any special handling that is needed. This function is called at the beginning
 * of an application, as the first operation.
 *
 * Along with the actual hardware initialization, this function must also
 * initialize the following fields within the ch_group_t sensor group
 * descriptor:
 *  - \b num_ports = number of ports on the board, usually the same
 *     as \a CHIRP_MAX_DEVICES in \b chirp_board_config.h.
 *  - \b num_i2c_buses = number of ports on the board, usually the same
 *  as \a CHIRP_NUM_I2C_BUSES in \b chirp_board_config.h.
 *  - \b rtc_cal_pulse_ms = length (duration) of the pulse sent on the INT line
 *  to each sensor during calibration of the real-time clock
 *
 * This function is REQUIRED.
 */
void chbsp_board_init(struct ch_group_t *grp_ptr);

/*!
 * \brief Toggle a debug indicator pin
 *
 * \param dbg_pin_num  index value for debug pin to toggle
 *
 * This function should change the state (high/low) of the specified debug
 * indicator pin. The \a dbg_pin_num parameter is an index value that specifies
 * which debug pin should be controlled.
 *
 * This function is OPTIONAL.
 *
 * \note OPTIONAL - Implementing this function is optional and only needed for
 *       debugging support. The indicator pins may be any convenient GPIO
 *       signals on the host system.  They are only used to provide a detectable
 *       indication of the program execution for debugging. If used, the debug
 *       pin(s) must be initialized during \a chbsp_board_init().
 */
void chbsp_debug_toggle(u8 dbg_pin_num);

/*!
 * \brief Turn on a debug indicator pin
 *
 * \param dbg_pin_num  index value for debug pin to turn on
 *
 * This function should drive the specified debug indicator pin high.
 * The \a dbg_pin_num parameter is an index value that specifies which debug pin
 * should be controlled.
 *
 * This function is OPTIONAL.
 *
 * \note OPTIONAL - Implementing this function is optional and only needed for
 *       debugging support. The indicator pins may be any convenient GPIO
 *       signals on the host system.  They are only used to provide a detectable
 *       indication of the program execution for debugging. If used, the debug
 *       pin(s) must be initialized during \a chbsp_board_init().
 */
void chbsp_debug_on(u8 dbg_pin_num);

/*!
 * \brief Turn off a debug indicator pin
 *
 * \param dbg_pin_num  index value for debug pin to turn off
 *
 * This function should drive the the specified debug indicator pin low.
 * The \a dbg_pin_num parameter is an index value that specifies which debug pin
 * should be controlled.
 *
 * This function is OPTIONAL.
 *
 * \note OPTIONAL - Implementing this function is optional and only needed for
 *       debugging support. The indicator pins may be any convenient GPIO
 *       signals on the host system.  They are only used to provide a detectable
 *       indication of the program execution for debugging. If used, the debug
 *       pin(s) must be initialized during \a chbsp_board_init().
 */
void chbsp_debug_off(u8 dbg_pin_num);

/*!
 * \brief Assert the reset pin for all sensors.
 *
 * This function should drive the Chirp sensor reset pin low (assert RESET_N)
 * on all sensors.
 *
 * This function is REQUIRED.
 */
void chbsp_reset_assert(void);

/*!
 * \brief Deassert the reset pin for all sensors.
 *
 * This function should drive the Chirp sensor reset pin high (or open drain if
 * there is a pull-up) on all sensors.
 *
 * This function is REQUIRED.
 */
void chbsp_reset_release(void);

/*!
 * \brief Assert the reset pulse pin
 *
 * This function drives the reset pulse pin low.
 */
void chbsp_reset_ps_assert(void);

/*!
 * \brief De-assert the pulse reset pin
 *
 * This function drives the pulse reset pin high.
 */
void chbsp_reset_ps_release(void);

/*!
 * \brief Assert the PROG pin
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function should drive the Chirp sensor PROG pin high for the specified
 * device.  It is used by the driver to initiate I2C communication with a
 * specific Chirp sensor device before a unique I2C address is assigned to the
 * device or when the programming interface is used.
 *
 * When the PROG pin is asserted, the device will respond to the standard
 * programming I2C address (0x45).
 *
 * This function is REQUIRED.
 */
void chbsp_program_enable(struct ch_dev_t *dev_ptr);

/*!
 * \brief Deassert the PROG pin
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function should drive the Chirp sensor PROG pin low for the specified
 * device.
 *
 * This function is REQUIRED.
 */
void chbsp_program_disable(struct ch_dev_t *dev_ptr);

/*!
 * \brief Configure the Chirp sensor INT pin as an output for a group of sensors
 *
 * \param grp_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 *
 * This function should configure each Chirp sensor's INT pin as an output
 * (from the perspective of the host system).
 *
 * This function is REQUIRED.
 */
void chbsp_group_set_io_dir_out(struct ch_group_t *grp_ptr);

/*!
 * \brief Configure the Chirp sensor INT pin as an output for one sensor.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function should configure the Chirp sensor INT pin as an output
 * (from the perspective of the host system).
 *
 * This function is RECOMMENDED.
 *
 * \note RECOMMENDED - Implementing this function is optional and is only
 * required if individual sensor hardware triggering is used. It is not required
 * if the sensors are operated only in free-running mode, or if they are always
 * triggered as a group, using \a ch_group_trigger(). However, implementing this
 * function is required if sensors will be triggered individually, using
 * \a ch_trigger().
 */
void chbsp_set_io_dir_out(struct ch_dev_t *dev_ptr);

/*!
 * \brief Configure the Chirp sensor INT pins as inputs for a group of sensors
 *
 * \param grp_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 *
 * This function should configure each Chirp sensor's INT pin as an input
 * (from the perspective of the host system).
 *
 * This function is REQUIRED.
 */
void chbsp_group_set_io_dir_in(struct ch_group_t *grp_ptr);

/*!
 * \brief Configure the Chirp sensor INT pin as an input for one sensor.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function should configure the Chirp sensor INT pin as an input
 * (from the perspective of the host system).
 *
 * This function is RECOMMENDED.
 *
 * \note RECOMMENDED - Implementing this function is optional and is only
 * required if individual sensor hardware triggering is used. It is not required
 * if the sensors are operated only in free-running mode, or if they are always
 * triggered as a group, using \a ch_group_trigger(). However, implementing this
 * function is required if sensors will be triggered individually, using
 * \a ch_trigger().
 */
void chbsp_set_io_dir_in(struct ch_dev_t *dev_ptr);

/*!
 * \brief Initialize the set of I/O pins for a group of sensors.
 *
 * \param grp_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 *
 * This function initializes the set of I/O pins used for a group of sensors.
 * It should perform any clock initialization and other setup required to use
 * the ports/pins. It should then configure the RESET_N and PROG pins as outputs
 * (from the perspective of the host system), and assert both RESET_N and PROG.
 * It should also configure the INT pin as an input.
 *
 * This function is REQUIRED.
 */
void chbsp_group_pin_init(struct ch_group_t *grp_ptr);

/*!
 * \brief Set the INT pins low for a group of sensors.
 *
 * \param grp_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 *
 * This function should drive the INT line low for each sensor in the group.
 *
 * This function is REQUIRED.
 */
void chbsp_group_io_clear(struct ch_group_t *grp_ptr);

/*!
 * \brief Set the INT pin low for one sensor.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function should drive the INT line low for the specified sensor.
 *
 * This function is OPTIONAL.
 *
 * \note OPTIONAL - Implementing this function is optional and only needed for
 * individual sensor hardware triggering using \a ch_trigger().  It is not
 * required if sensors are only triggered as a group using \a ch_group_trigger()
 */
void chbsp_io_clear(struct ch_dev_t *dev_ptr);

/*!
 * \brief Set the INT pins high for a group of sensors.
 *
 * \param grp_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 *
 * This function should drive the INT line high for each sensor in the group.
 *
 * This function is REQUIRED.
 */
void chbsp_group_io_set(struct ch_group_t *grp_ptr);

/*!
 * \brief Set the INT pin high for one sensor.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function should drive the INT line high for the specified sensor.
 *
 * This function is OPTIONAL.
 *
 * \note OPTIONAL - Implementing this function is optional and only needed for
 * individual sensor hardware triggering using \a ch_trigger().  It is not
 * required if sensors are only triggered as a group using \a ch_group_trigger()
 */
void chbsp_io_set(struct ch_dev_t *dev_ptr);

/*!
 * \brief Enable interrupts for a group of sensors.
 *
 * \param grp_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 *
 * For each sensor in the group, this function should enable the host interrupt
 * associated with the Chirp sensor device's INT line.
 *
 * This function is REQUIRED.
 */
void chbsp_group_io_interrupt_enable(struct ch_group_t *grp_ptr);

/*!
 * \brief Enable the interrupt for one sensor
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function should enable the host interrupt associated with the Chirp
 * sensor device's INT line.
 *
 * This function is REQUIRED.
 */
void chbsp_io_interrupt_enable(struct ch_dev_t *dev_ptr);

/*!
 * \brief Disable interrupts for a group of sensors
 *
 * \param grp_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 *
 * For each sensor in the group, this function should disable the host interrupt
 * associated with the Chirp sensor device's INT line.
 * *
 * This function is REQUIRED.
 */
void chbsp_group_io_interrupt_disable(struct ch_group_t *grp_ptr);

/*!
 * \brief Disable the interrupt for one sensor
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function should disable the host interrupt associated with the Chirp
 * sensor device's INT line.
 *
 * This function is REQUIRED.
 */
void chbsp_io_interrupt_disable(struct ch_dev_t *dev_ptr);

/*!
 * \brief Set callback routine for Chirp sensor I/O interrupt
 *
 * \param callback_func_ptr	pointer to application function to be called
 *				when interrupt occurs
 *
 * This function sets up the specified callback routine to be called whenever
 * the interrupt associated with the sensor's INT line occurs.
 * The implementation will typically save the callback routine address in a
 * pointer variable that can later be accessed from within the interrupt handler
 * to call the function.
 *
 * The callback function will generally be called at interrupt level from the
 * interrupt service routine.
 *
 * This function is REQUIRED.
 */
void chbsp_io_callback_set(ch_io_int_callback_t callback_func_ptr);

/*!
 * \brief Delay for specified number of microseconds
 *
 * \param us	number of microseconds to delay before returning
 *
 * This function should wait for the specified number of microseconds before
 * returning to the caller.
 *
 * This function is REQUIRED.
 */
void chbsp_delay_us(uint32_t us);

/*!
 * \brief Delay for specified number of milliseconds.
 *
 * \param ms	number of milliseconds to delay before returning
 *
 * This function should wait for the specified number of milliseconds before
 * returning to the caller.
 *
 * This function is REQUIRED.
 *
 * \note This function is used during the \a ch_group_start() function to
 * control the length of the calibration pulse sent to the Chirp sensor device
 * during the real-time clock (RTC) calibration, based on the pulse length
 * specified in the board support package.  The accuracy of this pulse timing
 * will directly affect the accuracy of the range values calculated by the
 * sensor.
 */
void chbsp_delay_ms(uint32_t ms);

/*!
 * \brief Return a free-running counter value in milliseconds.
 *
 * \return a 32-bit free-running counter value in milliseconds
 *
 * This function should use a running timer to provide an updated timestamp,
 * in milliseconds, when called.
 *
 * If \a CHDRV_DEBUG is defined, this function is used by the SonicLib driver
 * to calculate elapsed times for various operations.
 *
 * This function is OPTIONAL.
 *
 * \note OPTIONAL - Implementing this function is optional and only needed for
 * debugging support.
 */
uint32_t chbsp_timestamp_ms(void);

/*!
 * \brief Initialize the host's I2C hardware.
 *
 * \return 0 if successful, 1 on error
 *
 * This function should perform general I2C initialization on the host system.
 * This includes both hardware initialization and setting up any necessary
 * software structures.  Upon successful return from this routine, the system
 * should be ready to perform I/O operations such as \a chbsp_i2c_read() and
 * \a chbsp_i2c_write().
 *
 * This function is REQUIRED.
 */
int chbsp_i2c_init(void);

/*!
 * \brief De-initialize the host's I2C hardware.
 *
 * \return 0 if successful, 1 on error
 *
 * This function de-initializes the I2C hardware and control structures on the
 * host system.
 *
 * This function is OPTIONAL.
 *
 * \note OPTIONAL - Implementing this function is optional and only needed for
 * debugging support.
 */
int chbsp_i2c_deinit(void);

/*!
 * \brief Return I2C information for a sensor port on the board.
 *
 * \param grp_ptr	pointer to the ch_group_t config structure for
 *			a group of sensors
 * \param dev_num	device number within sensor group
 * \param info_ptr	pointer to structure to be filled with I2C config values
 *
 * \return 0 if successful, 1 if error
 *
 * This function is called by SonicLib functions to obtain I2C operating
 * parameters for a specific device on the board.
 *
 * This function returns I2C values in the ch_i2c_info_t structure specified by
 * \a info_ptr. The structure includes three fields.
 *  - The \a address field contains the I2C address for the sensor.
 *  - The \a bus_num field contains the I2C bus number (index).
 *  - The \a drv_flags field contains various bit flags through which the BSP
 *  can inform SonicLib driver functions to perform specific actions during I2C
 *  I/O operations.  The possible flags include:
 *  - - \a I2C_DRV_FLAG_RESET_AFTER_NB - the I2C interface needs to be reset
 *  after non-blocking transfers
 *  - - \a I2C_DRV_FLAG_USE_PROG_NB - use high-speed programming interface for
 *  non-blocking transfers
 *
 * This function is REQUIRED.
 */
u8 chbsp_i2c_get_info(struct ch_group_t *grp_ptr, u8 dev_num,
	struct ch_i2c_info_t *info_ptr);

/*!
 * \brief Write bytes to an I2C slave.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 * \param data			data to be transmitted
 * \param num_bytes	length of data to be transmitted
 *
 * \return 0 if successful, 1 on error or NACK
 *
 * This function should write one or more bytes of data to an I2C slave device.
 * The I2C interface will have already been initialized using
 * \a chbsp_i2c_init().
 *
 * \note Implementations of this function should use the \a ch_get_i2c_address()
 * function to obtain the device I2C address.
 */
int chbsp_i2c_write(struct ch_dev_t *dev_ptr, u8 *data, u16 num_bytes);

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
 * This function should write one or more bytes of data to an I2C slave device
 * using an internal memory or register address.  The remote device will write
 * \a num_bytes bytes of data starting at internal memory/register address
 * \a mem_addr. The I2C interface will have already been initialized using
 * \a chbsp_i2c_init().
 *
 * This function is REQUIRED.
 *
 * \note Implementations of this function should use the \a ch_get_i2c_address()
 * function to obtain the device I2C address.
 */
int chbsp_i2c_mem_write(struct ch_dev_t *dev_ptr, u16 mem_addr, u8 *data,
	u16 num_bytes);

/*!
 * \brief Write bytes to an I2C slave, non-blocking.
 * \param data		pointer to the start of data to be transmitted
 * \param num_bytes	length of data to be transmitted
 *
 * \return 0 if successful, 1 on error or NACK
 *
 * This function should initiate a non-blocking write of the specified number of
 * bytes to an I2C slave device.
 *
 * The I2C interface must have already been initialized using
 * \a chbsp_i2c_init().
 *
 * This function is OPTIONAL.
 *
 * \note OPTIONAL - Implementing this function is optional and only needed if
 * non-blocking writes on the I2C bus are required. It is not called by SonicLib
 * functions.
 *
 * \note Implementations of this function should use the \a ch_get_i2c_address()
 * function to obtain the device I2C address.
 */
int chbsp_i2c_write_nb(struct ch_dev_t *dev_ptr, u8 *data, u16 num_bytes);

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
 * This function should initiate a non-blocking write of the specified number of
 * bytes to an I2C slave device, using an internal memory or register address.
 * The remote device will write \a num_bytes bytes of data starting at internal
 * memory/register address \a mem_addr.
 *
 * The I2C interface must have already been initialized using
 * \a chbsp_i2c_init().
 *
 * This function is OPTIONAL.
 *
 * \note OPTIONAL - Implementing this function is optional and only needed if
 * non-blocking writes on the I2C bus are required.  It is not called by
 * SonicLib functions.
 *
 * \note Implementations of this function should use the \a ch_get_i2c_address()
 * function to obtain the device I2C address.
 */
int chbsp_i2c_mem_write_nb(struct ch_dev_t *dev_ptr, u16 mem_addr, u8 *data,
	u16 num_bytes);

/*!
 * \brief Read bytes from an I2C slave.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 * \param data			pointer to receive data buffer
 * \param num_bytes	number of bytes to read
 *
 * \return 0 if successful, 1 on error or NACK
 *
 * This function should read the specified number of bytes from an I2C slave
 * device.
 *
 * The I2C interface must have already been initialized using
 * \a chbsp_i2c_init().
 *
 * This function is REQUIRED.
 *
 * \note Implementations of this function should use the \a ch_get_i2c_address()
 * function to obtain the device I2C address.
 */
int chbsp_i2c_read(struct ch_dev_t *dev_ptr, u8 *data, u16 num_bytes);

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
 * This function should read the specified number of bytes from an I2C slave
 * device, using an internal memory or register address.  The remote device will
 * return \a num_bytes bytes starting at internal memory/register address
 * \a mem_addr.
 *
 * The I2C interface must have already been initialized using
 * \a chbsp_i2c_init().
 *
 * This function is normally implemented such that the device address and memory
 * address are written to the I2C bus, then a repeated-start marker is sent, and
 * the data is read.  Most micro-controller I/O libraries support such an access
 * sequence.  (If this is not possible, the write and read phases may be
 * separated into two discrete operations, without the repeated-start.  However,
 * this may be a problem if more than one bus master is present, because the bus
 * is not held between the two phases.)
 *
 * This function is REQUIRED.
 *
 * \note Implementations of this function should use the \a ch_get_i2c_address()
 * function to obtain the device I2C address.
 */
int chbsp_i2c_mem_read(struct ch_dev_t *dev_ptr, u16 mem_addr, u8 *data,
	u16 num_bytes);

/*!
 * \brief Read bytes from an I2C slave, non-blocking.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 * \param data		pointer to receive data buffer
 * \param num_bytes	number of bytes to read
 *
 * \return 0 if successful, 1 on error or NACK
 *
 * This function should initiate a non-blocking read of the specified number of
 * bytes from an I2C slave.
 *
 * The I2C interface must have already been initialized using
 * \a chbsp_i2c_init().
 *
 * This function is OPTIONAL.
 *
 * \note OPTIONAL - Implementing this function is optional and only needed if
 * non-blocking I/Q readout is required.
 *
 * \note Implementations of this function should use the \a ch_get_i2c_address()
 * function to obtain the device I2C address.
 */
int chbsp_i2c_read_nb(struct ch_dev_t *dev_ptr, u8 *data, u16 num_bytes);

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
 * This function should initiate a non-blocking read of the specified number of
 * bytes from an I2C slave. The I2C interface must have already been initialized
 * using \a chbsp_i2c_init().
 *
 * This function is OPTIONAL.
 *
 * \note OPTIONAL - Implementing this function is optional and only needed if
 * non-blocking I/Q readout is required.
 *
 * \note Implementations of this function should use the \a ch_get_i2c_address()
 * function to obtain the device I2C address.
 */
int chbsp_i2c_mem_read_nb(struct ch_dev_t *dev_ptr, u16 mem_addr, u8 *data,
	u16 num_bytes);

/*!
 * \brief Interrupt handler callout for external devices sharing the I2C bus.
 *
 * \param trans	pointer to Chirp sensor I2C transaction control structure
 *
 * This function is called when a non-blocking I2C operation completes on an
 * "external" (non Chirp sensor) device. The \a chbsp_external_i2c_queue()
 * function should be called to add such a transaction to the I2C queue.
 *
 * \note OPTIONAL - Implementing this function is optional and only needed if
 * devices other than the Chirp sensor(s) are operating on the same I2C bus and
 * sharing the Chirp driver non-blocking I2C I/O mechanism.
 */
void chbsp_external_i2c_irq_handler(struct chdrv_i2c_transaction_t *trans);

/*!
 * \brief Reset I2C bus associated with device.
 *
 * \param dev_ptr	pointer to the ch_dev_t config structure for a sensor
 *
 * This function should perform a reset of the I2C interface for the specified
 * device.
 */
//!
void chbsp_i2c_reset(struct ch_dev_t *dev_ptr);

/*!
 * \brief Initialize periodic timer.
 *
 * \param interval_ms		timer interval, in milliseconds
 * \param callback_func_ptr	address of routine to be called every time the
 *				imer expires
 *
 * \return 0 if successful, 1 if error
 *
 * This function initializes a periodic timer on the board.  The timer should be
 * programmed to generate an interrupt after every \a interval_ms milliseconds.
 *
 * The \a callback_func_ptr parameter specifies a callback routine that will be
 * called when the timer expires (and interrupt occurs).  The timer interrupt
 * handler function within the board support package should call this function.
 *
 * The period timer is often used to trigger sensor measurement cycles by having
 * the application's callback function call \a ch_trigger() or
 * \a ch_group_trigger().
 *
 * This function is RECOMMENDED.
 *
 * \note RECOMMENDED - This and other periodic timer functions are not called by
 * SonicLib functions, so are not required.  However, they are used in examples
 * and other applications from Chirp.
 */
u8 chbsp_periodic_timer_init(u16 interval_ms,
	ch_timer_callback_t callback_func_ptr);

/*!
 * \brief Enable periodic timer interrupt.
 *
 * This function enables the interrupt associated with the periodic timer
 * initialized by \a chbsp_periodic_timer_init().
 *
 * This function is RECOMMENDED.
 *
 * \note RECOMMENDED - This and other periodic timer functions are not called by
 * SonicLib functions, so are not required.  However, they are used in examples
 * and other applications from Chirp.
 */
void chbsp_periodic_timer_irq_enable(void);

/*!
 * \brief Disable periodic timer interrupt.
 *
 * This function enables the interrupt associated with the periodic timer
 * initialized by \a chbsp_periodic_timer_init().
 *
 * This function is RECOMMENDED.
 *
 * \note RECOMMENDED - This and other periodic timer functions are not called by
 * SonicLib functions, so are not required.  However, they are used in examples
 * and other applications from Chirp.
 */
void chbsp_periodic_timer_irq_disable(void);

/*!
 * \brief Start periodic timer.
 *
 * \return 0 if successful, 1 if error
 *
 * This function starts the periodic timer initialized by
 * \a chbsp_periodic_timer_init().
 *
 * This function is RECOMMENDED.
 *
 * \note RECOMMENDED - This and other periodic timer functions are not called by
 * SonicLib functions, so are not required.  However, they are used in examples
 * and other applications from Chirp.
 */
u8 chbsp_periodic_timer_start(void);

/*!
 * \brief Stop periodic timer.
 *
 * \return 0 if successful, 1 if error
 *
 * This function stops the periodic timer initialized by
 * \a chbsp_periodic_timer_init().
 *
 * This function is RECOMMENDED.
 *
 * \note RECOMMENDED - This and other periodic timer functions are not called by
 * SonicLib functions, so are not required.  However, they are used in examples
 * and other applications from Chirp.
 */
u8 chbsp_periodic_timer_stop(void);

/*!
 * \brief Periodic timer handler.
 *
 * \return 0 if successful, 1 if error
 *
 * This function handles the expiration of the periodic timer, re-arms it and
 * any associated interrupts for the next interval, and calls the callback
 * routine that was registered using \a chbsp_periodic_timer_init().
 *
 * This function is RECOMMENDED.
 *
 * \note RECOMMENDED - This and other periodic timer functions are not called by
 * SonicLib functions, so are not required.  However, they are used in examples
 * and other applications from Chirp.
 */
void chbsp_periodic_timer_handler(void);

/*!
 * \brief Put the processor into low-power sleep state.
 *
 * This function puts the host processor (MCU) into a low-power sleep mode, to
 * conserve energy.  The sleep state should be selected such that interrupts
 * associated with the I2C, external GPIO pins, and the periodic timer (if used)
 * are able to wake up the device.
 *
 * This function is RECOMMENDED.
 *
 * \note RECOMMENDED - This and other periodic timer functions are not called by
 * SonicLib functions, so are not required.  However, they are used in examples
 * and other applications from Chirp.
 */
void chbsp_proc_sleep(u16 ms);

/*!
 * \brief Turn on an LED on the board.
 *
 * This function turns on an LED on the board.  The implementation of this
 * function is flexible to allow for different arrangements of LEDs.
 *
 * The \a dev_num parameter contains the device number of a specific sensor.
 * The device number may be used to select which LED should be turned on
 * (e.g. if it is located next to an individual sensor).  If the board does not
 * have LEDs associated with individual sensors, the BSP may implement this
 * routine to combine all device numbers to control a single LED, or any other
 * organization that makes sense for the board.
 *
 * This function is RECOMMENDED.
 *
 * \note RECOMMENDED - This and other periodic timer functions are not called by
 * SonicLib functions, so are not required.  However, they are used in examples
 * and other applications from Chirp.
 */
void chbsp_led_on(u8 dev_num);

/*!
 * \brief Turn off an LED on the board.
 *
 * This function turns off an LED on the board.  The implementation of this
 * function is flexible to allow for different arrangements of LEDs.
 *
 * The \a dev_num parameter contains the device number of a specific sensor.
 * The device number may be used to select which LED should be turned on
 * (e.g. if it is located next to an individual sensor).  If the board does not
 * have LEDs associated with individual sensors, the BSP may implement this
 * routine to combine all device numbers to control a single LED, or any other
 * organization that makes sense for the board.
 *
 * This function is RECOMMENDED.
 *
 * \note RECOMMENDED - This and other periodic timer functions are not called by
 * SonicLib functions, so are not required.  However, they are used in examples
 * and other applications from Chirp.
 */
void chbsp_led_off(u8 dev_num);

/*!
 * \brief Output a text string via serial interface
 *
 * \param str	pointer to a string of characters to be output
 *
 * This function should print debug information to the console. The user can
 * implement this function to print characters over a serial port, for example.
 *
 * In the Chirp SonicLib sensor driver, debug message output may be enabled by
 * defining \a CHDRV_DEBUG and/or \a CHDRV_DEBUG_VERBOSE.
 *
 * \note OPTIONAL - Implementing this function is optional and only needed for
 * debugging support.
 */
void chbsp_print_str(char *str);

void chbsp_printf(const char *fmt, ...);

/*!
 * \brief Critical section enter/leave.
 *
 * This functions called to prevent execution break in multi task environment.
 *
 * This function is RECOMMENDED.
 *
 * \note RECOMMENDED - This functions required in in multi task environment.
 */
void chbsp_critical_section_enter(void);
void chbsp_critical_section_leave(void);

#endif  /* __CHIRP_BSP_H_ */
