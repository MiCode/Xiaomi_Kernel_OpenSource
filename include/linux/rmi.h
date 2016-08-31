/*
 * Copyright (c) 2011, 2012 Synaptics Incorporated
 * Copyright (c) 2011 Unixphere
 * Copyright (C) 2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifndef _RMI_H
#define _RMI_H
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/list.h>
#include <linux/lockdep.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/debugfs.h>
//#include <linux/earlysuspend.h>

extern struct bus_type rmi_bus_type;

extern struct device_type rmi_function_type;
extern struct device_type rmi_sensor_type;

/* When NV_NOTIFY_OUT_OF_IDLE is set no rmi spi interrupt for 50ms will be
 * considered as idle. On first interrupt after idle miscellaneous input
 * event MSC_ACTIVITY will be sent. This event will serve as early
 * notification for actual input event and will allow cpu frequency governor
 * to boost CPU clk early.
 */
#define NV_NOTIFY_OUT_OF_IDLE	1

/* Permissions for sysfs attributes.  Since the permissions policy will change
 * on a global basis in the future, rather than edit all sysfs attrs everywhere
 * in the driver (and risk screwing that up in the process), we use this handy
 * set of #defines.  That way when we change the policy for sysfs permissions,
 * we only need to change them here.
 */
#define RMI_RO_ATTR S_IRUGO
#define RMI_RW_ATTR (S_IRUGO | S_IWUGO)
#define RMI_WO_ATTR S_IWUGO

enum rmi_attn_polarity {
	RMI_ATTN_ACTIVE_LOW = 0,
	RMI_ATTN_ACTIVE_HIGH = 1
};

/**
 * struct rmi_f11_axis_alignment - target axis alignment
 * @swap_axes: set to TRUE if desired to swap x- and y-axis
 * @flip_x: set to TRUE if desired to flip direction on x-axis
 * @flip_y: set to TRUE if desired to flip direction on y-axis
 * @clip_X_low - reported X coordinates below this setting will be clipped to
 *               the specified value
 * @clip_X_high - reported X coordinates above this setting will be clipped to
 *               the specified value
 * @clip_Y_low - reported Y coordinates below this setting will be clipped to
 *               the specified value
 * @clip_Y_high - reported Y coordinates above this setting will be clipped to
 *               the specified value
 * @offset_X - this value will be added to all reported X coordinates
 * @offset_Y - this value will be added to all reported Y coordinates
 * @rel_report_enabled - if set to true, the relative reporting will be
 *               automatically enabled for this sensor.
 */
struct rmi_f11_2d_axis_alignment {
	u32 swap_axes;
	bool flip_x;
	bool flip_y;
	int clip_X_low;
	int clip_Y_low;
	int clip_X_high;
	int clip_Y_high;
	int offset_X;
	int offset_Y;
	u8 delta_x_threshold;
	u8 delta_y_threshold;
};

/**
 * struct virtualbutton_map - describes rectangular areas of a 2D sensor that
 * will be used by the driver to generate button events.
 *
 * @x - the x position of the low order corner of the rectangle, in RMI4
 * position units.
 * @y - the y position of the low order corner of the rectangle, in RMI4
 * position units.
 * @width - the width of the rectangle, in RMI4 position units.
 * @height - the height of the rectangle, in RMI4 position units.
 * @code - the input subsystem key event code that will be generated when a
 * tap occurs within the rectangle.
 */
struct virtualbutton_map {
	u16 x;
	u16 y;
	u16 width;
	u16 height;
	u16 code;
};

/**
 * struct rmi_f11_virtualbutton_map - provides a list of virtual buttons for
 * a 2D sensor.
 *
 * @buttons - the number of entries in the map.
 * @map - an array of virtual button descriptions.
 */
struct rmi_f11_virtualbutton_map {
	u8 buttons;
	struct virtualbutton_map *map;
};

/** This is used to override any hints an F11 2D sensor might have provided
 * as to what type of sensor it is.
 *
 * @rmi_f11_sensor_default - do not override, determine from F11_2D_QUERY14 if
 * available.
 * @rmi_f11_sensor_touchscreen - treat the sensor as a touchscreen (direct
 * pointing).
 * @rmi_f11_sensor_touchpad - thread the sensor as a touchpad (indirect
 * pointing).
 */
enum rmi_f11_sensor_type {
	rmi_f11_sensor_default = 0,
	rmi_f11_sensor_touchscreen,
	rmi_f11_sensor_touchpad
};

/**
 * struct rmi_f11_sensor_data - overrides defaults for a single F11 2D sensor.
 * @axis_align - provides axis alignment overrides (see above).
 * @virtual_buttons - describes areas of the touch sensor that will be treated
 *                    as buttons.
 * @type_a - all modern RMI F11 firmwares implement Multifinger Type B
 * protocol.  Set this to true to force MF Type A behavior, in case you find
 * an older sensor.
 * @sensor_type - Forces the driver to treat the sensor as an indirect
 * pointing device (touchpad) rather than a direct pointing device
 * (touchscreen).  This is useful when F11_2D_QUERY14 register is not
 * available.
 */
struct rmi_f11_sensor_data {
	struct rmi_f11_2d_axis_alignment axis_align;
	struct rmi_f11_virtualbutton_map virtual_buttons;
	bool type_a;
	enum rmi_f11_sensor_type sensor_type;
};

/**
 * struct rmi_f01_power - override default power management settings.
 *
 */
enum rmi_f01_nosleep {
	RMI_F01_NOSLEEP_DEFAULT = 0,
	RMI_F01_NOSLEEP_OFF = 1,
	RMI_F01_NOSLEEP_ON = 2
};

/**
 * struct rmi_f01_power_management -When non-zero, these values will be written
 * to the touch sensor to override the default firmware settigns.  For a
 * detailed explanation of what each field does, see the corresponding
 * documention in the RMI4 specification.
 *
 * @nosleep - specifies whether the device is permitted to sleep or doze (that
 * is, enter a temporary low power state) when no fingers are touching the
 * sensor.
 * @wakeup_threshold - controls the capacitance threshold at which the touch
 * sensor will decide to wake up from that low power state.
 * @doze_holdoff - controls how long the touch sensor waits after the last
 * finger lifts before entering the doze state, in units of 100ms.
 * @doze_interval - controls the interval between checks for finger presence
 * when the touch sensor is in doze mode, in units of 10ms.
 */
struct rmi_f01_power_management {
	enum rmi_f01_nosleep nosleep;
	u8 wakeup_threshold;
	u8 doze_holdoff;
	u8 doze_interval;
};

/**
 * struct rmi_button_map - used to specify the initial input subsystem key
 * event codes to be generated by buttons (or button like entities) on the
 * touch sensor.
 * @nbuttons - length of the button map.
 * @map - the key event codes for the corresponding buttons on the touch
 * sensor.
 */
struct rmi_button_map {
	u8 nbuttons;
	u8 *map;
};

struct rmi_f30_gpioled_map {
	u8 ngpioleds;
	u8 *map;
};

/**
 * struct rmi_device_platform_data_spi - provides parameters used in SPI
 * communications.  All Synaptics SPI products support a standard SPI
 * interface; some also support what is called SPI V2 mode, depending on
 * firmware and/or ASIC limitations.  In V2 mode, the touch sensor can
 * support shorter delays during certain operations, and these are specified
 * separately from the standard mode delays.
 *
 * @block_delay - for standard SPI transactions consisting of both a read and
 * write operation, the delay (in microseconds) between the read and write
 * operations.
 * @split_read_block_delay_us - for V2 SPI transactions consisting of both a
 * read and write operation, the delay (in microseconds) between the read and
 * write operations.
 * @read_delay_us - the delay between each byte of a read operation in normal
 * SPI mode.
 * @write_delay_us - the delay between each byte of a write operation in normal
 * SPI mode.
 * @split_read_byte_delay_us - the delay between each byte of a read operation
 * in V2 mode.
 * @pre_delay_us - the delay before the start of a SPI transaction.  This is
 * typically useful in conjunction with custom chip select assertions (see
 * below).
 * @post_delay_us - the delay after the completion of an SPI transaction.  This
 * is typically useful in conjunction with custom chip select assertions (see
 * below).
 * @cs_assert - For systems where the SPI subsystem does not control the CS/SSB
 * line, or where such control is broken, you can provide a custom routine to
 * handle a GPIO as CS/SSB.  This routine will be called at the beginning and
 * end of each SPI transaction.  The RMI SPI implementation will wait
 * pre_delay_us after this routine returns before starting the SPI transfer;
 * and post_delay_us after completion of the SPI transfer(s) before calling it
 * with assert==FALSE.
 */
struct rmi_device_platform_data_spi {
	int block_delay_us;
	int split_read_block_delay_us;
	int read_delay_us;
	int write_delay_us;
	int split_read_byte_delay_us;
	int pre_delay_us;
	int post_delay_us;

	void *cs_assert_data;
	int (*cs_assert) (const void *cs_assert_data, const bool assert);
};

/**
 * struct rmi_device_platform_data - system specific configuration info.
 *
 * @sensor_name - this is used for various diagnostic messages.
 *
 * @firmware_name - if specified will override default firmware name,
 * for reflashing.
 *
 * @attn_gpio - the index of a GPIO that will be used to provide the ATTN
 * interrupt from the touch sensor.
 * @attn_polarity - indicates whether ATTN is active high or low.
 * @level_triggered - by default, the driver uses edge triggered interrupts.
 * However, this can cause problems with suspend/resume on some platforms.  In
 * that case, set this to 1 to use level triggered interrupts.
 * @gpio_config - a routine that will be called when the driver is loaded to
 * perform any platform specific GPIO configuration, and when it is unloaded
 * for GPIO de-configuration.  This is typically used to configure the ATTN
 * GPIO and the I2C or SPI pins, if necessary.
 * @gpio_data - platform specific data to be passed to the GPIO configuration
 * function.
 *
 * @poll_interval_ms - the time in milliseconds between reads of the interrupt
 * status register.  This is ignored if attn_gpio is non-zero.
 *
 * @reset_delay_ms - after issuing a reset command to the touch sensor, the
 * driver waits a few milliseconds to give the firmware a chance to
 * to re-initialize.  You can override the default wait period here.
 *
 * @spi_data - override default settings for SPI delays and SSB management (see
 * above).
 *
 * @f11_sensor_data - an array of platform data for individual F11 2D sensors.
 * @f11_sensor_count - the length of f11_sensor_data array.  Extra entries will
 * be ignored; if there are too few entries, all settings for the additional
 * sensors will be defaulted.
 * @f11_rezero_wait - if non-zero, this is how may milliseconds the F11 2D
 * sensor(s) will wait before being be rezeroed on exit from suspend.  If
 * this value is zero, the F11 2D sensor(s) will not be rezeroed on resume.
 * @pre_suspend - this will be called before any other suspend operations are
 * done.
 * @power_management - overrides default touch sensor doze mode settings (see
 * above)
 * @f19_button_map - provide initial input subsystem key mappings for F19.
 * @f1a_button_map - provide initial input subsystem key mappings for F1A.
 * @gpioled_map - provides initial settings for GPIOs and LEDs controlled by
 * F30.
 * @f41_button_map - provide initial input subsystem key mappings for F41.
 * @f54_direct_touch_report_size - the size of the report used for direct
 * touch.
 *
 * @post_suspend - this will be called after all suspend operations are
 * completed.  This is the ONLY safe place to power off an RMI sensor
 * during the suspend process.
 * @pre_resume - this is called before any other resume operations.  If you
 * powered off the RMI4 sensor in post_suspend(), then you MUST power it back
 * here, and you MUST wait an appropriate time for the ASIC to come up
 * (100ms to 200ms, depending on the sensor) before returning.
 * @pm_data - this will be passed to the various (pre|post)_(suspend/resume)
 * functions.
 */
struct rmi_device_platform_data {
	char *sensor_name;	/* Used for diagnostics. */

	int attn_gpio;
	enum rmi_attn_polarity attn_polarity;
	bool level_triggered;
	void *gpio_data;
	int (*gpio_config)(void *gpio_data, bool configure);

	int poll_interval_ms;

	int reset_delay_ms;

	struct rmi_device_platform_data_spi spi_data;

	/* function handler pdata */
	struct rmi_f11_sensor_data *f11_sensor_data;
	u8 f11_sensor_count;
	u16 f11_rezero_wait;
	struct rmi_f01_power_management power_management;
	struct rmi_button_map *f19_button_map;
	struct rmi_button_map *f1a_button_map;
	struct rmi_f30_gpioled_map *gpioled_map;
	struct rmi_button_map *f41_button_map;
	int f54_direct_touch_report_size;

#ifdef CONFIG_RMI4_FWLIB
	char *firmware_name;
#endif

#ifdef	CONFIG_PM
	void *pm_data;
	int (*pre_suspend) (const void *pm_data);
	int (*post_suspend) (const void *pm_data);
	int (*pre_resume) (const void *pm_data);
	int (*post_resume) (const void *pm_data);
#endif
};

/**
 * struct rmi_function_descriptor - RMI function base addresses
 *
 * @query_base_addr: The RMI Query base address
 * @command_base_addr: The RMI Command base address
 * @control_base_addr: The RMI Control base address
 * @data_base_addr: The RMI Data base address
 * @interrupt_source_count: The number of irqs this RMI function needs
 * @function_number: The RMI function number
 *
 * This struct is used when iterating the Page Description Table. The addresses
 * are 16-bit values to include the current page address.
 *
 */
struct rmi_function_descriptor {
	u16 query_base_addr;
	u16 command_base_addr;
	u16 control_base_addr;
	u16 data_base_addr;
	u8 interrupt_source_count;
	u8 function_number;
	u8 function_version;
};

struct rmi_function_dev;
struct rmi_device;

/**
 * struct rmi_function_driver - driver routines for a particular RMI function.
 *
 * @func: The RMI function number
 * @probe: Called when the handler is successfully matched to a function device.
 * @reset: Called when a reset of the touch sensor is detected.  The routine
 * should perform any out-of-the-ordinary reset handling that might be
 * necessary.  Restoring of touch sensor configuration registers should be
 * handled in the config() callback, below.
 * @config: Called when the function container is first initialized, and
 * after a reset is detected.  This routine should write any necessary
 * configuration settings to the device.
 * @attention: Called when the IRQ(s) for the function are set by the touch
 * sensor.
 * @suspend: Should perform any required operations to suspend the particular
 * function.
 * @resume: Should perform any required operations to resume the particular
 * function.
 *
 * All callbacks are expected to return 0 on success, error code on failure.
 */
struct rmi_function_driver {
	struct device_driver driver;

	u8 func;
	int (*probe)(struct rmi_function_dev *fc);
	int (*remove)(struct rmi_function_dev *fc);
	int (*config)(struct rmi_function_dev *fc);
	int (*reset)(struct rmi_function_dev *fc);
	int (*attention)(struct rmi_function_dev *fc,
				unsigned long *irq_bits);
#ifdef CONFIG_PM
	int (*suspend)(struct rmi_function_dev *fc);
	int (*resume)(struct rmi_function_dev *fc);
#if defined(CONFIG_HAS_EARLYSUSPEND)
	int (*early_suspend)(struct rmi_function_dev *fc);
	int (*late_resume)(struct rmi_function_dev *fc);
#endif
#endif
#ifdef NV_NOTIFY_OUT_OF_IDLE
	int (*out_of_idle)(struct rmi_function_dev *fc);
#endif
};

#define to_rmi_function_driver(d) \
		container_of(d, struct rmi_function_driver, driver);

/**
 * struct rmi_function_dev - represents an a particular RMI4 function on a given
 * RMI4 sensor.
 *
 * @fd: The function descriptor of the RMI function
 * @rmi_dev: Pointer to the RMI device associated with this function device
 * @dev: The device associated with this particular function.
 *
 * @num_of_irqs: The number of irqs needed by this function
 * @irq_pos: The position in the irq bitfield this function holds
 * @irq_mask: For convience, can be used to mask IRQ bits off during ATTN
 * interrupt handling.
 * @data: Private data pointer
 *
 * @list: Used to create a list of function devices.
 * @debugfs_root: used during debugging
 *
 */
struct rmi_function_dev {

	struct rmi_function_descriptor fd;
	struct rmi_device *rmi_dev;
	struct device dev;
	int num_of_irqs;
	int irq_pos;
	unsigned long *irq_mask;
	void *data;
	struct list_head list;

	struct dentry *debugfs_root;
};

#define to_rmi_function_dev(d) \
		container_of(d, struct rmi_function_dev, dev);


int __must_check __rmi_register_function_driver(struct rmi_function_driver *,
						 struct module *, const char *);
#define rmi_register_function_driver(handler) \
	__rmi_register_function_driver(handler, THIS_MODULE, KBUILD_MODNAME)

void rmi_unregister_function_driver(struct rmi_function_driver *);

/**
 * struct rmi_driver - driver for an RMI4 sensor on the RMI bus.
 *
 * @driver: Device driver model driver
 * @irq_handler: Callback for handling irqs
 * @reset_handler: Called when a reset is detected.
 * @get_func_irq_mask: Callback for calculating interrupt mask
 * @store_irq_mask: Callback for storing and replacing interrupt mask
 * @restore_irq_mask: Callback for restoring previously stored interrupt mask
 * @store_productid: Callback for cache product id from function 01
 * @data: Private data pointer
 *
 */
struct rmi_driver {
	struct device_driver driver;

	int (*irq_handler)(struct rmi_device *rmi_dev, int irq);
	int (*reset_handler)(struct rmi_device *rmi_dev);
	int (*store_irq_mask)(struct rmi_device *rmi_dev,
				unsigned long *new_interupts);
	int (*restore_irq_mask)(struct rmi_device *rmi_dev);
	int (*store_productid)(struct rmi_device *rmi_dev);
	int (*set_input_params)(struct rmi_device *rmi_dev,
			struct input_dev *input);
	int (*remove)(struct rmi_device *rmi_dev);
	void *data;
};

#define to_rmi_driver(d) \
	container_of(d, struct rmi_driver, driver);

/** struct rmi_phys_info - diagnostic information about the RMI physical
 * device, used in the phys debugfs file.
 *
 * @proto String indicating the protocol being used.
 * @tx_count Number of transmit operations.
 * @tx_bytes Number of bytes transmitted.
 * @tx_errs  Number of errors encountered during transmit operations.
 * @rx_count Number of receive operations.
 * @rx_bytes Number of bytes received.
 * @rx_errs  Number of errors encountered during receive operations.
 * @att_count Number of times ATTN assertions have been handled.
 */
struct rmi_phys_info {
	char *proto;
	long tx_count;
	long tx_bytes;
	long tx_errs;
	long rx_count;
	long rx_bytes;
	long rx_errs;
};

/**
 * struct rmi_phys_device - represent an RMI physical device
 *
 * @dev: Pointer to the communication device, e.g. i2c or spi
 * @rmi_dev: Pointer to the RMI device
 * @write_block: Writing a block of data to the specified address
 * @read_block: Read a block of data from the specified address.
 * @irq_thread: if not NULL, the sensor driver will use this instead of the
 * default irq_thread implementation.
 * @hard_irq: if not NULL, the sensor driver will use this for the hard IRQ
 * handling
 * @data: Private data pointer
 *
 * The RMI physical device implements the glue between different communication
 * buses such as I2C and SPI.
 *
 */
struct rmi_phys_device {
	struct device *dev;
	struct rmi_device *rmi_dev;

	int (*write_block)(struct rmi_phys_device *phys, u16 addr,
			   const void *buf, const int len);
	int (*read_block)(struct rmi_phys_device *phys, u16 addr,
			  void *buf, const int len);

	int (*enable_device) (struct rmi_phys_device *phys);
	void (*disable_device) (struct rmi_phys_device *phys);

	irqreturn_t (*irq_thread)(int irq, void *p);
	irqreturn_t (*hard_irq)(int irq, void *p);

	void *data;

	struct rmi_phys_info info;
};

/**
 * struct rmi_device - represents an RMI4 sensor device on the RMI bus.
 *
 * @dev: The device created for the RMI bus
 * @number: Unique number for the device on the bus.
 * @driver: Pointer to associated driver
 * @phys: Pointer to the physical interface
 * @early_suspend_handler: Pointers to early_suspend, if
 * configured.
 * @debugfs_root: base for this particular sensor device.
 *
 */
struct rmi_device {
	struct device dev;
	int number;

	struct rmi_driver *driver;
	struct rmi_phys_device *phys;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend_handler;
#endif

	struct dentry *debugfs_root;
	int    interrupt_restore_block_flag;
};

#define to_rmi_device(d) container_of(d, struct rmi_device, dev);
#define to_rmi_platform_data(d) ((d)->phys->dev->platform_data);

/**
 * rmi_read - read a single byte
 * @d: Pointer to an RMI device
 * @addr: The address to read from
 * @buf: The read buffer
 *
 * Reads a byte of data using the underlaying physical protocol in to buf. It
 * returns zero or a negative error code.
 */
static inline int rmi_read(struct rmi_device *d, u16 addr, void *buf)
{
	return d->phys->read_block(d->phys, addr, buf, 1);
}

/**
 * rmi_read_block - read a block of bytes
 * @d: Pointer to an RMI device
 * @addr: The start address to read from
 * @buf: The read buffer
 * @len: Length of the read buffer
 *
 * Reads a block of byte data using the underlaying physical protocol in to buf.
 * It returns the amount of bytes read or a negative error code.
 */
static inline int rmi_read_block(struct rmi_device *d, u16 addr, void *buf,
				 const int len)
{
	return d->phys->read_block(d->phys, addr, buf, len);
}

/**
 * rmi_write - write a single byte
 * @d: Pointer to an RMI device
 * @addr: The address to write to
 * @data: The data to write
 *
 * Writes a byte from buf using the underlaying physical protocol. It
 * returns zero or a negative error code.
 */
static inline int rmi_write(struct rmi_device *d, u16 addr, const u8 data)
{
	return d->phys->write_block(d->phys, addr, &data, 1);
}

/**
 * rmi_write_block - write a block of bytes
 * @d: Pointer to an RMI device
 * @addr: The start address to write to
 * @buf: The write buffer
 * @len: Length of the write buffer
 *
 * Writes a block of byte data from buf using the underlaying physical protocol.
 * It returns the amount of bytes written or a negative error code.
 */
static inline int rmi_write_block(struct rmi_device *d, u16 addr,
				  const void *buf, const int len)
{
	return d->phys->write_block(d->phys, addr, buf, len);
}

int rmi_register_phys_device(struct rmi_phys_device *phys);
void rmi_unregister_phys_device(struct rmi_phys_device *phys);
int rmi_for_each_dev(void *data, int (*func)(struct device *dev, void *data));

/**
 * module_rmi_function_driver() - Helper macro for registering a function driver
 * @__rmi_driver: rmi_function_driver struct
 *
 * Helper macro for RMI4 function drivers which do not do anything special in
 * module init/exit. This eliminates a lot of boilerplate. Each module
 * may only use this macro once, and calling it replaces module_init()
 * and module_exit().
 */
#define module_rmi_function_driver(__rmi_driver)	\
	module_driver(__rmi_driver,			\
		      rmi_register_function_driver,	\
		      rmi_unregister_function_driver)

/**
 * Helper fn to convert a byte array representing a 16 bit value in the RMI
 * endian-ness to a 16-bit value in the native processor's specific endianness.
 * We don't use ntohs/htons here because, well, we're not dealing with
 * a pair of 16 bit values. Casting dest to u16* wouldn't work, because
 * that would imply knowing the byte order of u16 in the first place.  The
 * same applies for using shifts and masks.
 */
static inline u16 batohs(u8 *src)
{
	return src[1] << 8 | src[0];
}
/**
 * Helper function to convert a 16 bit value (in host processor endianess) to
 * a byte array in the RMI endianess for u16s.  See above comment for
 * why we dont us htons or something like that.
 */
static inline void hstoba(u8 *dest, u16 src)
{
	dest[0] = src & 0xFF;
	dest[1] =  src >> 8;
}

#endif
