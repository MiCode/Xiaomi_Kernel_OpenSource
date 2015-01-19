/*
 * SiI8620 Linux Driver
 *
 * Copyright (C) 2013-2014 Silicon Image, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/spi/spi.h>
#include <linux/regulator/consumer.h>

#include "si_fw_macros.h"
#include "si_infoframe.h"
#include "si_edid.h"
#include "si_mhl_defs.h"
#include "si_mhl2_edid_3d_api.h"
#include "si_mhl_tx_hw_drv_api.h"
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
#include <linux/input.h>
#include "si_mdt_inputdev.h"
#endif
#include "mhl_linux_tx.h"
#include "mhl_supp.h"
#include "platform.h"
#include "si_mhl_callback_api.h"
#include "si_8620_drv.h"
#include "si_8620_regs.h"

#ifdef SIMG_USE_DTS
#include <linux/of_gpio.h>
#endif


/*
 * Platform resources (I2C port, GPIOs, ...) needed to control the
 * MHL starter kit.  These default values are used to interface with
 * PandaBoard which does not (currently) use device tree.  Device tree
 * capable systems should modify the device tree to declare the platform
 * resources assigned to the MHL starter kit.  If compiled with device tree
 * support (-DSIMG_USE_DTS), this driver will override these default
 * values with those from the device tree.
 */
#define GPIO_MHL_INT			138	/* W_INT */
#define GPIO_BB_RESET			140	/* P_BB_RST# */
#define I2C_ADAPTER				4

#define GPIO_EXP_ADDR				0x40

#define RESET_PULSE_WIDTH			1	/* In ms	*/

/*
 * NOTE: The following GPIO expander register type address
 * offsets are all defined with the address auto-increment
 * bit set (0x80)
 */
#define GPIO_EXP_INPUT_REGS_OFFSET		0x80
#define GPIO_EXP_OUTPUT_REGS_OFFSET		0x88
#define GPIO_EXP_POL_INVERT_REGS_OFFSET		0x90
#define GPIO_EXP_IO_CONFIG_REGS_OFFSET		0x98
#define GPIO_EXP_INTR_MASK_REGS_OFFSET		0xA0

#define GPIO_EXP_BANK_2_OUTPUT_DEFAULT		0xFF
#define GPIO_EXP_BANK_2_3D			(0x01 << 0)
#define GPIO_EXP_BANK_2_PKD_PXL			(0x01 << 1)
#define GPIO_EXP_BANK_2_HDCP_ON			(0x01 << 2)
#define GPIO_EXP_BANK_2_N_TCODE			(0x01 << 3)
#define GPIO_EXP_BANK_2_LED_USB_MODE		(0x01 << 4)
#define GPIO_EXP_BANK_2_SPR_LED2		(0x01 << 5)
#define GPIO_EXP_BANK_2_SPR_LED3		(0x01 << 6)
#define GPIO_EXP_BANK_2_SPR_LED4		(0x01 << 7)

#define GPIO_EXP_BANK_3_OUTPUT_DEFAULT		0x67

#define GPIO_EXP_BANK_3_MHL_TX_RST_B		(0x01 << 0)
#define GPIO_EXP_BANK_3_FW_WAKE_A		(0x01 << 1)
#define GPIO_EXP_BANK_3_CHG_DET			(0x01 << 2)
#define GPIO_EXP_BANK_3_XO3_SINK_VBUS_SENSE	(0x01 << 3)
#define GPIO_EXP_BANK_3_12V_PS_SENSE		(0x01 << 4)
#define GPIO_EXP_BANK_3_EEPROM_WR_EN		(0x01 << 5)
#define GPIO_EXP_BANK_3_TX2MHLRX_PWR_A		(0x01 << 6)
#define GPIO_EXP_BANK_3_M2U_VBUS_CTRL_A		(0x01 << 7)

#define GPIO_EXP_BANK_4_OUTPUT_DEFAULT		0xF0
#define GPIO_EXP_BANK_4_DSW9			(0x01 << 0)
#define GPIO_EXP_BANK_4_DSW10			(0x01 << 1)
#define GPIO_EXP_BANK_4_DSW11			(0x01 << 2)
#define GPIO_EXP_BANK_4_DSW12			(0x01 << 3)
#define GPIO_EXP_BANK_4_USB_SW_CTRL0		(0x01 << 4)
#define GPIO_EXP_BANK_4_USB_SW_CTRL1		(0x01 << 5)
#define GPIO_EXP_BANK_4_LED15_AMBER		(0x01 << 6)
#define GPIO_EXP_BANK_4_LED15_GREEN		(0x01 << 7)

#define GET_FROM_MODULE_PARAM			-1
#define GPIO_ON_EXPANDER			-2

#define REG_PCA_950x_PORT_0_INPUT		0x00
#define REG_PCA_950x_PORT_1_INPUT		0x01
#define REG_PCA_950x_PORT_2_INPUT		0x02
#define REG_PCA_950x_PORT_3_INPUT		0x03
#define REG_PCA_950x_PORT_4_INPUT		0x04

#define REG_PCA_950x_PORT_0_OUTPUT		0x08
#define REG_PCA_950x_PORT_1_OUTPUT		0x09
#define REG_PCA_950x_PORT_2_OUTPUT		0x0A
#define REG_PCA_950x_PORT_3_OUTPUT		0x0B
#define REG_PCA_950x_PORT_4_OUTPUT		0x0C

u8 gpio_exp_bank2_output;
u8 gpio_exp_bank3_output;
u8 gpio_exp_bank4_output;

static char *buildTime = "Built " __DATE__ "-" __TIME__;
static char *buildVersion = "1.03." BUILD_NUM_STRING;

struct semaphore platform_lock;
static uint32_t platform_flags;
bool probe_fail;

static struct spi_device *spi_dev;
#define SPI_BUS_NUM		1
#define	SPI_CHIP_SEL		0
#define SPI_TRANSFER_MODE	SPI_MODE_0
#define SPI_BUS_SPEED		1000000

enum si_spi_opcodes {
	spi_op_disable = 0x04,
	spi_op_enable = 0x06,
	spi_op_clear_status = 0x07,
	spi_op_reg_read = 0x60,
	spi_op_reg_write = 0x61,
	spi_op_ddc_reg_read = 0x62,
	spi_op_emsc_read = 0x80,
	spi_op_emsc_write = 0x81,
	spi_op_slow_cbus_read = 0x90,
	spi_op_slow_cbus_write = 0x91
};

#define MAX_SPI_PAYLOAD_SIZE		LOCAL_BLK_RCV_BUFFER_SIZE
#define MAX_SPI_CMD_SIZE		3
#define EMSC_WRITE_SPI_CMD_SIZE		1
#define EMSC_READ_SPI_CMD_SIZE		1
#define MAX_SPI_DUMMY_XFER_BYTES	20
#define MAX_SPI_XFER_BUFFER_SIZE	(MAX_SPI_CMD_SIZE + \
		MAX_SPI_DUMMY_XFER_BYTES + MAX_SPI_PAYLOAD_SIZE)
#define MAX_SPI_EMSC_BLOCK_SIZE (MAX_SPI_CMD_SIZE + MAX_SPI_PAYLOAD_SIZE)

#define MAX_I2C_PAYLOAD_SIZE		LOCAL_BLK_RCV_BUFFER_SIZE
#define MAX_I2C_CMD_SIZE		0

#define MAX_I2C_EMSC_BLOCK_SIZE (MAX_I2C_CMD_SIZE + MAX_I2C_PAYLOAD_SIZE)

struct spi_xfer_mem {
	u8 *tx_buf;
	u8 *rx_buf;
	/* block commands are asynchronous to normal cbus traffic
	   and CANNOT share a buffer.
	 */
	uint8_t *block_tx_buffers;
	struct spi_transfer spi_xfer[2];
	struct spi_message spi_cmd;
} spi_mem;

struct i2c_xfer_mem {
	uint8_t *block_tx_buffers;
} i2c_mem;

#ifdef MHL_GPIO_EXPANDER
static bool expander_enabled = true;
#else
static bool expander_enabled;
#endif

static int gpio_expander_transfer(u8 offset, u16 count, u8 *values, bool write);

#if defined(SIMG_USE_DTS)
static u32 i2c_adapter_num = I2C_ADAPTER;
#endif
static u32 spi_bus_num = SPI_BUS_NUM;
static struct i2c_adapter *i2c_bus_adapter;

struct i2c_dev_info {
	uint8_t dev_addr;
	struct i2c_client *client;
};

#define I2C_DEV_INFO(addr) \
	{.dev_addr = addr >> 1, .client = NULL}

static struct i2c_dev_info device_addresses[] = {
	I2C_DEV_INFO(SA_TX_PAGE_0),
	I2C_DEV_INFO(SA_TX_PAGE_1),
	I2C_DEV_INFO(SA_TX_PAGE_2),
	I2C_DEV_INFO(SA_TX_PAGE_3),
	I2C_DEV_INFO(SA_TX_PAGE_6),
	I2C_DEV_INFO(SA_TX_CBUS),
	I2C_DEV_INFO(GPIO_EXP_ADDR),
};

int debug_level;
bool debug_reg_dump;
bool input_dev_rap = 1;
bool input_dev_rcp = 1;
bool input_dev_ucp = 1;
#if (INCLUDE_RBP == 1)
bool input_dev_rbp = 1;
#endif
int hdcp_content_type;
bool use_spi = 0;		 /* Default to i2c (0). */
int crystal_khz = 19200; /* SiI8620 SK has 19.2MHz crystal */
int use_heartbeat;

bool wait_for_user_intr;
int tmds_link_speed;
#ifdef FORCE_OCBUS_FOR_ECTS
bool force_ocbus_for_ects;
#endif
int gpio_index = 138;

module_param(debug_reg_dump, bool, S_IRUGO);
module_param(debug_level, int, S_IRUGO);

module_param(input_dev_rap, bool, S_IRUGO);
module_param(input_dev_rcp, bool, S_IRUGO);
module_param(input_dev_ucp, bool, S_IRUGO);
#if (INCLUDE_RBP == 1)
module_param(input_dev_rbp, bool, S_IRUGO);
#endif
module_param(hdcp_content_type, int, S_IRUGO);
module_param(use_spi, bool, S_IRUGO);
module_param(crystal_khz, int, S_IRUGO);
module_param(use_heartbeat, int, S_IRUGO);
module_param(wait_for_user_intr, bool, S_IRUGO);
module_param(tmds_link_speed, int, S_IRUGO);
#ifdef	FORCE_OCBUS_FOR_ECTS
module_param(force_ocbus_for_ects, bool, S_IRUGO);
#endif

module_param_named(debug_msgs, debug_level, int, S_IRUGO);

struct platform_signals_list platform_signals[] = {
	{.name = "TX_HW_RESET",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_3_OUTPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_3_MHL_TX_RST_B,
	 .gpio_bank_value = &gpio_exp_bank3_output,
	 .param = NULL},
	{.name = "TX_FW_WAKE",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_3_OUTPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_3_FW_WAKE_A,
	 .gpio_bank_value = &gpio_exp_bank3_output,
	 .param = NULL},
	{.name = "CHG_DET",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_3_OUTPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_3_CHG_DET,
	 .gpio_bank_value = &gpio_exp_bank3_output,
	 .param = NULL},
	{.name = "XO3_SINK_VBUS_SENSE",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_3_INPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_3_XO3_SINK_VBUS_SENSE,
	 .gpio_bank_value = &gpio_exp_bank3_output,
	 .param = NULL},
	{.name = "TWELVE_VOLT_PS_SENSE",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_3_OUTPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_3_12V_PS_SENSE,
	 .gpio_bank_value = &gpio_exp_bank3_output,
	 .param = NULL},
	{.name = "EEPROM_WR_EN",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_3_OUTPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_3_EEPROM_WR_EN,
	 .gpio_bank_value = &gpio_exp_bank3_output,
	 .param = NULL},
	{.name = "TX2MHLRX_PWR",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_3_OUTPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_3_TX2MHLRX_PWR_A,
	 .gpio_bank_value = &gpio_exp_bank3_output,
	 .param = NULL},
	{.name = "M2U_VBUS_CTRL",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_3_OUTPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_3_M2U_VBUS_CTRL_A,
	 .gpio_bank_value = &gpio_exp_bank3_output,
	 .param = NULL},
	{.name = "LED_3D",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_2_OUTPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_2_3D,
	 .gpio_bank_value = &gpio_exp_bank2_output,
	 .param = NULL},
	{.name = "LED_PACKED_PIXEL",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_2_OUTPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_2_PKD_PXL,
	 .gpio_bank_value = &gpio_exp_bank2_output,
	 .param = NULL},
	{.name = "LED_HDCP",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_2_OUTPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_2_HDCP_ON,
	 .gpio_bank_value = &gpio_exp_bank2_output,
	 .param = NULL},
	{.name = "LED_USB_MODE",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_2_OUTPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_2_LED_USB_MODE,
	 .gpio_bank_value = &gpio_exp_bank2_output,
	 .param = NULL},
	{.name = "LED_SPARE_2",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_2_OUTPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_2_SPR_LED2,
	 .gpio_bank_value = &gpio_exp_bank2_output,
	 .param = NULL},
	{.name = "LED_SPARE_3",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_2_OUTPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_2_SPR_LED3,
	 .gpio_bank_value = &gpio_exp_bank2_output,
	 .param = NULL},
	{.name = "LED_SPARE_4",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_2_OUTPUT},
	 .gpio_mask_PCA950x = GPIO_EXP_BANK_2_SPR_LED4,
	 .gpio_bank_value = &gpio_exp_bank2_output,
	 .param = NULL},
	{.name = "X02_USB_SW_CTRL",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_4_OUTPUT},
	 .gpio_mask_PCA950x = (GPIO_EXP_BANK_4_USB_SW_CTRL0 |
			       GPIO_EXP_BANK_4_USB_SW_CTRL1),
	 .gpio_bank_value = &gpio_exp_bank4_output,
	 .param = NULL},
	{.name = "X02_USB_SW_CTRL0",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_4_OUTPUT},
	 .gpio_mask_PCA950x = (GPIO_EXP_BANK_4_USB_SW_CTRL0),
	 .gpio_bank_value = &gpio_exp_bank4_output,
	 .param = NULL},
	{.name = "X02_USB_SW_CTRL1",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_4_OUTPUT},
	 .gpio_mask_PCA950x = (GPIO_EXP_BANK_4_USB_SW_CTRL1),
	 .gpio_bank_value = &gpio_exp_bank4_output,
	 .param = NULL},
	{.name = "X02_LED15_AMBER",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_4_OUTPUT},
	 .gpio_mask_PCA950x = (GPIO_EXP_BANK_4_LED15_AMBER),
	 .gpio_bank_value = &gpio_exp_bank4_output,
	 .param = NULL},
	{.name = "X02_LED15_GREEN",
	 .gpio_number = GPIO_ON_EXPANDER,
	 .gpio_reg_PCA950x = {GPIO_EXP_ADDR, REG_PCA_950x_PORT_4_OUTPUT},
	 .gpio_mask_PCA950x = (GPIO_EXP_BANK_4_LED15_GREEN),
	 .gpio_bank_value = &gpio_exp_bank4_output,
	 .param = NULL}
};

#define MHL_INT_INDEX		0
#define MHL_RESET_INDEX		1
static struct gpio starter_kit_control_gpios[] = {
	/*
	 * GPIO signals needed for the starter kit board.
	 */
	{GPIO_MHL_INT, GPIOF_IN, "MHL_intr"},
	{GPIO_BB_RESET, GPIOF_OUT_INIT_HIGH, "MHL_reset"},
};

static inline int platform_read_i2c_block(struct i2c_adapter *i2c_bus, u8 page,
	u8 offset, u16 count, u8 *values)
{
	struct i2c_msg msg[2];

	msg[0].flags = 0;
	msg[0].addr = page >> 1;
	msg[0].buf = &offset;
	msg[0].len = 1;

	msg[1].flags = I2C_M_RD;
	msg[1].addr = page >> 1;
	msg[1].buf = values;
	msg[1].len = count;

	return i2c_transfer(i2c_bus, msg, 2);
}

static inline int platform_write_i2c_block(struct i2c_adapter *i2c_bus, u8 page,
	u8 offset, u16 count, u8 *values)
{
	struct i2c_msg msg;
	u8 *buffer;
	int ret;

	buffer = kmalloc(count + 1, GFP_KERNEL);
	if (!buffer) {
		printk(KERN_ERR "%s:%d buffer allocation failed\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	buffer[0] = offset;
	memmove(&buffer[1], values, count);

	msg.flags = 0;
	msg.addr = page >> 1;
	msg.buf = buffer;
	msg.len = count + 1;

	ret = i2c_transfer(i2c_bus, &msg, 1);

	kfree(buffer);

	if (ret != 1) {
		printk(KERN_ERR "%s:%d I2c write failed 0x%02x:0x%02x\n",
			__func__, __LINE__, page, offset);
		ret = -EIO;
	} else {
		ret = 0;
	}

	return ret;
}

static int platform_read_i2c_reg(struct i2c_adapter *bus_adapter_i2c, u8 page,
	u8 offset)
{
	int ret;
	u8 byte_read;

	ret = platform_read_i2c_block(bus_adapter_i2c, page, offset, 1,
		&byte_read);
	MHL_TX_DBG_INFO("\tGI2C_R %2x:%2x = %2x\n", page, offset, ret);
	if (ret != 2) {
		printk(KERN_ERR "%s:%d I2c read failed, 0x%02x:0x%02x\n",
			__func__, __LINE__, page, offset);
		ret = -EIO;
	} else {
		ret = 0;
	}
	return ret ? ret : byte_read;
}

static int platform_write_i2c_reg(struct i2c_adapter *bus_adapter_i2c, u8 page,
	u8 offset, u8 value)
{
	MHL_TX_DBG_INFO("\tGI2C_W %2x:%2x <- %2x\n", page, offset, value);
	return platform_write_i2c_block(bus_adapter_i2c, page, offset, 1,
					&value);
}

uint32_t platform_get_flags(void)
{
	return platform_flags;
}

static void toggle_reset_n(void)
{
	MHL_TX_DBG_INFO("Toggle MHL_RST_B/RESET_N pin. Resets 8620 only.\n");
	gpio_exp_bank3_output &= ~GPIO_EXP_BANK_3_MHL_TX_RST_B;
	gpio_expander_transfer(GPIO_EXP_OUTPUT_REGS_OFFSET + 3,
			       1, &gpio_exp_bank3_output, true);

	/* Without this, we see a 500ns reset pulse. Enforce 1ms.*/
	msleep(RESET_PULSE_WIDTH);

	gpio_exp_bank3_output |= GPIO_EXP_BANK_3_MHL_TX_RST_B;
	gpio_expander_transfer(GPIO_EXP_OUTPUT_REGS_OFFSET + 3,
			       1, &gpio_exp_bank3_output, true);

}

static void toggle_BB_RST(int reset_period)
{
	MHL_TX_DBG_INFO("Toggle BB_RST# pin. Resets GPIO expander AND 8620\n");
	gpio_set_value(starter_kit_control_gpios[MHL_RESET_INDEX].gpio, 0);
	msleep(reset_period);
	gpio_set_value(starter_kit_control_gpios[MHL_RESET_INDEX].gpio, 1);

}

int is_interrupt_asserted(void)
{
	return gpio_get_value(starter_kit_control_gpios[MHL_INT_INDEX].gpio)
			? 0 : 1;
}

int get_config(void *dev_context, int config_idx)
{
	int pin_state = 0;

	if (config_idx < ARRAY_SIZE(platform_signals)) {
		switch (platform_signals[config_idx].gpio_number) {
		case GET_FROM_MODULE_PARAM:
			pin_state = *(platform_signals[config_idx].param);
			break;
		case GPIO_ON_EXPANDER:
			pin_state =
			    (platform_read_i2c_reg
			     (i2c_bus_adapter,
			      platform_signals[config_idx].gpio_reg_PCA950x.
			      slave_addr,
			      platform_signals[config_idx].gpio_reg_PCA950x.
			      offset)
			     & platform_signals[config_idx].
			     gpio_mask_PCA950x) ? 1 : 0;
			break;
		default:
			pin_state =
				gpio_get_value(platform_signals[config_idx].
				gpio_number);
			break;
		}
	}
	return pin_state;
}

void set_pin_impl(int pin_idx, int value,
		  const char *function_name, int line_num)
{
	uint8_t bank_value;

	if (pin_idx < ARRAY_SIZE(platform_signals)) {

		MHL_TX_DBG_INFO("set_pin(%s,%d)\n",
			platform_signals[pin_idx].name, value);
		switch (platform_signals[pin_idx].gpio_number) {
		case GET_FROM_MODULE_PARAM:
			break;
		case GPIO_ON_EXPANDER:
			if (expander_enabled) {
				bank_value = *(platform_signals[pin_idx]
							.gpio_bank_value);
				if (value)
					bank_value |= platform_signals[pin_idx].
						gpio_mask_PCA950x;
				else
					bank_value &=
						~platform_signals[pin_idx].
							gpio_mask_PCA950x;

				*(platform_signals[pin_idx].gpio_bank_value) =
					bank_value;
				platform_write_i2c_reg(i2c_bus_adapter,
					       platform_signals[pin_idx].
					       gpio_reg_PCA950x.slave_addr,
					       platform_signals[pin_idx].
					       gpio_reg_PCA950x.offset,
					       bank_value);
			} else {
				bank_value = 0;
			}
			break;
		default:
			gpio_set_value(platform_signals[pin_idx].gpio_number,
				value);
			break;
		}
	}
}

void platform_mhl_tx_hw_reset(uint32_t reset_period, uint32_t reset_delay)
{
	/* then reset the chip */
	if (expander_enabled)
		toggle_reset_n();
	else
		toggle_BB_RST(100);

	if (reset_delay)
		msleep(reset_delay);

	if (use_spi) {
		u8 cmd = spi_op_enable;
		spi_write(spi_dev, &cmd, 1);
	}
}

void mhl_tx_vbus_control(enum vbus_power_state power_state)
{
	struct device *parent_dev;
	struct mhl_dev_context *dev_context;

	if (use_spi)
		parent_dev = &spi_dev->dev;
	else
		parent_dev = &device_addresses[0].client->dev;

	dev_context = dev_get_drvdata(parent_dev);

	switch (power_state) {
	case VBUS_OFF:
		set_pin(M2U_VBUS_CTRL, 0);
		set_pin(TX2MHLRX_PWR, 1);
		break;

	case VBUS_ON:
		set_pin(TX2MHLRX_PWR, 0);
		set_pin(M2U_VBUS_CTRL, 1);
		break;

	default:
		dev_err(dev_context->mhl_dev,
			"%s: Invalid power state %d received!\n",
			__func__, power_state);
		break;
	}
}

void mhl_tx_vbus_current_ctl(uint16_t max_current_in_milliamps)
{
	/*
		Starter kit does not have a PMIC.
		Implement VBUS input current limit control here.
	*/

}

int si_device_dbg_i2c_reg_xfer(void *dev_context, u8 page, u8 offset,
			       u16 count, bool rw_flag, u8 *buffer)
{
	u16 address = (page << 8) | offset;

	if (rw_flag == DEBUG_I2C_WRITE)
		return mhl_tx_write_reg_block(dev_context, address, count,
					      buffer);
	else
		return mhl_tx_read_reg_block(dev_context, address, count,
					     buffer);
}

#define MAX_DEBUG_MSG_SIZE	1024

#if defined(DEBUG)

/*
 * Return a pointer to the file name part of the
 * passed path spec string.
 */
char *find_file_name(const char *path_spec)
{
	char *pc;

	for (pc = (char *)&path_spec[strlen(path_spec)];
		pc != path_spec; --pc) {
		if ('\\' == *pc) {
			++pc;
			break;
		}
		if ('/' == *pc) {
			++pc;
			break;
		}
	}
	return pc;
}

void print_formatted_debug_msg(char *file_spec, const char *func_name,
			       int line_num, char *fmt, ...)
{
	uint8_t *msg = NULL;
	uint8_t *msg_offset;
	char *file_spec_sep = NULL;
	int remaining_msg_len = MAX_DEBUG_MSG_SIZE;
	int len;
	va_list ap;

	if (fmt == NULL)
		return;

	msg = kmalloc(remaining_msg_len, GFP_KERNEL);
	if (msg == NULL)
		return;

	msg_offset = msg;

	len = scnprintf(msg_offset, remaining_msg_len, "mhl: ");
	msg_offset += len;
	remaining_msg_len -= len;

	/* Only print the file name, not the path */
	if (file_spec != NULL)
		file_spec = find_file_name(file_spec);

	if (file_spec != NULL) {
		if (func_name != NULL)
			file_spec_sep = "->";
		else if (line_num != -1)
			file_spec_sep = ":";
	}

	if (file_spec) {
		len = scnprintf(msg_offset, remaining_msg_len, "%s", file_spec);
		msg_offset += len;
		remaining_msg_len -= len;
	}

	if (file_spec_sep) {
		len =
		    scnprintf(msg_offset, remaining_msg_len, "%s",
			      file_spec_sep);
		msg_offset += len;
		remaining_msg_len -= len;
	}

	if (func_name) {
		len = scnprintf(msg_offset, remaining_msg_len, "%s", func_name);
		msg_offset += len;
		remaining_msg_len -= len;
	}

	if (line_num != -1) {
		if ((file_spec != NULL) || (func_name != NULL))
			len =
			    scnprintf(msg_offset, remaining_msg_len, ":%d ",
				      line_num);
		else
			len =
			    scnprintf(msg_offset, remaining_msg_len, "%d ",
				      line_num);

		msg_offset += len;
		remaining_msg_len -= len;
	}

	va_start(ap, fmt);
	len = vscnprintf(msg_offset, remaining_msg_len, fmt, ap);
	va_end(ap);

	printk(msg);

	kfree(msg);
}

void dump_transfer(enum tx_interface_types if_type,
		   u8 page, u8 offset, u16 count, u8 *values, bool write)
{
	if (debug_reg_dump != 0) {
		int buf_size = 64;
		u16 idx;
		int buf_offset;
		char *buf;
		char *if_type_msg;

		switch (if_type) {
		case TX_INTERFACE_TYPE_I2C:
			if_type_msg = "I2C";
			break;
		case TX_INTERFACE_TYPE_SPI:
			if_type_msg = "SPI";
			break;
		default:
			return;
		};

		if (count > 1) {
			/* 3 chars per byte displayed */
			buf_size += count * 3;
			/* plus per display row overhead */
			buf_size += ((count / 16) + 1) * 8;
		}

		buf = kmalloc(buf_size, GFP_KERNEL);
		if (!buf)
			return;

		if (count == 1) {

			scnprintf(buf, buf_size, "   %s %02X.%02X %s %02X\n",
				  if_type_msg,
				  page, offset, write ? "W" : "R", values[0]);
		} else {
			idx = 0;
			buf_offset =
			    scnprintf(buf, buf_size, "%s %02X.%02X %s(%d)",
				      if_type_msg, page, offset,
				      write ? "W" : "R", count);

			for (idx = 0; idx < count; idx++) {
				if (0 == (idx & 0x0F))
					buf_offset +=
					    scnprintf(&buf[buf_offset],
						      buf_size - buf_offset,
						      "\n%04X: ", idx);

				buf_offset += scnprintf(&buf[buf_offset],
							buf_size - buf_offset,
							"%02X ", values[idx]);
			}
			buf_offset +=
			    scnprintf(&buf[buf_offset], buf_size - buf_offset,
				      "\n");
		}

		print_formatted_debug_msg(NULL, NULL, -1, buf);
		kfree(buf);
	}
}
#endif /* #if defined(DEBUG) */

static struct mhl_drv_info drv_info = {
	.drv_context_size = sizeof(struct drv_hw_context),
	.mhl_device_initialize = si_mhl_tx_chip_initialize,
	.mhl_device_isr = si_mhl_tx_drv_device_isr,
	.mhl_device_dbg_i2c_reg_xfer = si_device_dbg_i2c_reg_xfer,
	.mhl_device_get_aksv = si_mhl_tx_drv_get_aksv
};

int mhl_tx_write_reg_block_i2c(void *drv_context, u8 page, u8 offset,
			       u16 count, u8 *values)
{
	DUMP_I2C_TRANSFER(page, offset, count, values, true);

	return platform_write_i2c_block(i2c_bus_adapter, page, offset, count,
					values);
}

int mhl_tx_write_reg_i2c(void *drv_context, u8 page, u8 offset, u8 value)
{
	return mhl_tx_write_reg_block_i2c(drv_context, page, offset, 1, &value);
}

int mhl_tx_read_reg_block_i2c(void *drv_context, u8 page, u8 offset,
	u16 count, u8 *values)
{
	int ret;

	if (count == 0) {
		MHL_TX_DBG_ERR("Tried to read 0 bytes\n");
		return -EINVAL;
	}

	ret = platform_read_i2c_block(i2c_bus_adapter, page, offset, count,
		values);
	if (ret != 2) {
		MHL_TX_DBG_ERR("I2c read failed, 0x%02x:0x%02x\n", page,
			offset);
		ret = -EIO;
	} else {
		ret = 0;
		DUMP_I2C_TRANSFER(page, offset, count, values, false);
	}

	return ret;
}

int mhl_tx_read_reg_i2c(void *drv_context, u8 page, u8 offset)
{
	u8 byte_read;
	int status;

	status = mhl_tx_read_reg_block_i2c(drv_context, page, offset,
					   1, &byte_read);

	return status ? status : byte_read;
}

static int i2c_addr_to_spi_cmd(void *drv_context, bool write, u8 *page,
			       u8 *opcode, u8 *dummy_bytes)
{
	if (write) {
		*opcode = spi_op_reg_write;
		*dummy_bytes = 0;
	} else {
		*opcode = spi_op_reg_read;
		*dummy_bytes = 5;
	}

	switch (*page) {
	case SA_TX_PAGE_0:
		*page = 0;
		break;
	case SA_TX_PAGE_1:
		*page = 1;
		break;
	case SA_TX_PAGE_2:
		*page = 2;
		break;
	case SA_TX_PAGE_3:
		*page = 3;
		break;
	case SA_TX_PAGE_4:
		*page = 4;
		break;
	case SA_TX_CBUS:
		*page = 5;
		break;
	case SA_TX_PAGE_6:
		*page = 6;
		break;
	case SA_TX_PAGE_7:
		*page = 7;
		break;
	case SA_TX_PAGE_8:
		*page = 8;
		break;
	default:
		MHL_TX_DBG_ERR("Called with unknown page 0x%02x\n", *page);
		return -EINVAL;
	}
	return 0;
}

inline uint8_t reg_page(uint16_t address)
{
	return (uint8_t)((address >> 8) & 0x00FF);
}

inline uint8_t reg_offset(uint16_t address)
{
	return (uint8_t)(address & 0x00FF);
}

static int mhl_tx_write_reg_block_spi(void *drv_context, u8 page, u8 offset,
	u16 count, u8 *values)
{
	u8 opcode;
	u8 dummy_bytes;
	u16 length = count + 3;
	int ret;

	DUMP_SPI_TRANSFER(page, offset, count, values, true);

	ret = i2c_addr_to_spi_cmd(drv_context, true, &page, &opcode,
				  &dummy_bytes);
	if (ret != 0)
		return ret;

	length = 3 + count + dummy_bytes;

	if (length > MAX_SPI_XFER_BUFFER_SIZE) {
		MHL_TX_DBG_ERR("Transfer count (%d) is too large!\n", count);
		return -EINVAL;
	}

	spi_mem.tx_buf[0] = opcode;
	spi_mem.tx_buf[1] = page;
	spi_mem.tx_buf[2] = offset;
	if (dummy_bytes)
		memset(&spi_mem.tx_buf[3], 0, dummy_bytes);

	memmove(&spi_mem.tx_buf[dummy_bytes + 3], values, count);

	ret = spi_write(spi_dev, spi_mem.tx_buf, length);

	if (ret != 0) {
		MHL_TX_DBG_ERR("SPI write block failed, "
			       "page: 0x%02x, register: 0x%02x\n",
			       page, offset);
		ret = -EIO;
	} else {
		ret = 0;
	}

	return ret;
}

static int mhl_tx_write_reg_spi(void *drv_context, u8 page, u8 offset, u8 value)
{
	return mhl_tx_write_reg_block_spi(drv_context, page, offset, 1, &value);
}

static int mhl_tx_read_reg_block_spi(void *drv_context, u8 page, u8 offset,
	u16 count, u8 *values)
{
	u8 page_num = page;
	u8 opcode;
	u8 dummy_bytes;
	u16 length;
	int ret = 0;

	if (count > MAX_SPI_PAYLOAD_SIZE) {
		MHL_TX_DBG_ERR("Requested transfer count (%d) is too large\n",
			count);
		return -EINVAL;
	}

	ret = i2c_addr_to_spi_cmd(drv_context, false, &page_num, &opcode,
		&dummy_bytes);
	if (ret != 0)
		return ret;

	if ((reg_page(REG_DDC_DATA) == page) &&
		(reg_offset(REG_DDC_DATA) == offset)) {
		dummy_bytes = 11;
		opcode = spi_op_ddc_reg_read;
	}

	length = 3 + count + dummy_bytes;
	if (length > MAX_SPI_XFER_BUFFER_SIZE) {
		MHL_TX_DBG_ERR("Requested transfer total (%d) is too large\n",
			length);
		return -EINVAL;
	}

	spi_message_init(&spi_mem.spi_cmd);
	memset(&spi_mem.spi_xfer, 0, sizeof(spi_mem.spi_xfer));
	spi_mem.tx_buf[0] = opcode;
	spi_mem.tx_buf[1] = page_num;
	spi_mem.tx_buf[2] = offset;

	spi_mem.spi_xfer[0].tx_buf = spi_mem.tx_buf;
	spi_mem.spi_xfer[0].len = 3 + dummy_bytes;
#ifdef USE_SPIOPTIMIZE
	memset(&spi_mem.tx_buf[3], 0, dummy_bytes + count);
	spi_mem.spi_xfer[0].len += count;

	spi_mem.spi_xfer[0].rx_buf = spi_mem.rx_buf;
	spi_message_add_tail(&spi_mem.spi_xfer[0], &spi_mem.spi_cmd);
	ret = spi_sync(spi_dev, &spi_mem.spi_cmd);
#else
	spi_message_add_tail(&spi_mem.spi_xfer[0], &spi_mem.spi_cmd);

	spi_mem.spi_xfer[1].rx_buf = spi_mem.rx_buf;
	spi_mem.spi_xfer[1].len = count;
	spi_mem.spi_xfer[1].cs_change = 1;
	spi_message_add_tail(&spi_mem.spi_xfer[1], &spi_mem.spi_cmd);

	ret = spi_sync(spi_dev, &spi_mem.spi_cmd);
#endif

	if (ret != 0) {
		MHL_TX_DBG_ERR("SPI read block failed, "
			"page: 0x%02x, register: 0x%02x\n",
			page, offset);
	} else {
#ifdef USE_SPIOPTIMIZE
		memcpy(values, &spi_mem.rx_buf[3 + dummy_bytes], count);
#else
		memcpy(values, spi_mem.rx_buf, count);
#endif
		DUMP_SPI_TRANSFER(page, offset, count, values, false);
	}

	return ret;
}

static int mhl_tx_read_reg_block_spi_emsc(void *drv_context, u16 count,
	u8 *values)
{
	u8 dummy_bytes = 1;
	u16 length;
	int ret;

	if (count > MAX_SPI_PAYLOAD_SIZE) {
		MHL_TX_DBG_ERR("Requested transfer count (%d) is too large\n",
			count);
		return -EINVAL;
	}

	length = EMSC_READ_SPI_CMD_SIZE + dummy_bytes + count;
	if (length > MAX_SPI_XFER_BUFFER_SIZE) {
		MHL_TX_DBG_ERR("Requested transfer total (%d) is too large\n",
			length);
		return -EINVAL;
	}

	spi_message_init(&spi_mem.spi_cmd);
	memset(&spi_mem.spi_xfer, 0, sizeof(spi_mem.spi_xfer));
	spi_mem.tx_buf[0] = spi_op_emsc_read;

	spi_mem.spi_xfer[0].tx_buf = spi_mem.tx_buf;
	spi_mem.spi_xfer[0].len = EMSC_READ_SPI_CMD_SIZE + dummy_bytes;

#ifdef USE_SPIOPTIMIZE
	memset(&spi_mem.tx_buf[EMSC_READ_SPI_CMD_SIZE], 0, dummy_bytes + count);
	spi_mem.spi_xfer[0].rx_buf = spi_mem.rx_buf;
	spi_mem.spi_xfer[0].len += count;
	spi_message_add_tail(&spi_mem.spi_xfer[0], &spi_mem.spi_cmd);
#else
	spi_message_add_tail(&spi_mem.spi_xfer[0], &spi_mem.spi_cmd);

	spi_mem.spi_xfer[1].rx_buf = spi_mem.rx_buf;
	spi_mem.spi_xfer[1].len = count;
	spi_mem.spi_xfer[1].cs_change = 1;
	spi_message_add_tail(&spi_mem.spi_xfer[1], &spi_mem.spi_cmd);
#endif
	ret = spi_sync(spi_dev, &spi_mem.spi_cmd);

	if (ret != 0) {
		MHL_TX_DBG_ERR("SPI eMSC read block failed ");
	} else {
#ifdef USE_SPIOPTIMIZE
		memcpy(values,
			&spi_mem.rx_buf[EMSC_READ_SPI_CMD_SIZE + dummy_bytes],
			count);
#else
		memcpy(values, spi_mem.rx_buf, count);
#endif
		/* DUMP_SPI_TRANSFER(page, offset, count, values, false); */
	}

	return ret;
}

int mhl_tx_read_spi_emsc(void *drv_context, u16 count, u8 *values)
{
	u8 *dptr;
	u16 length, block;
	int ret = 0;

	/*
	 * Don't read more than 128 bytes at a time to reduce
	 * possible problems with SPI hardware.
	 */
	dptr = values;
	length = count;
	while (length > 0) {
		block = (length < 128) ? length : 128;
		ret = mhl_tx_read_reg_block_spi_emsc(drv_context, block, dptr);
		if (ret != 0)
			break;
		length -= block;
		dptr += block;
	}

	return ret;
}

static int mhl_tx_read_reg_spi(void *drv_context, u8 page, u8 offset)
{
	u8 byte_read = 0;
	int status;

	status = mhl_tx_read_reg_block_spi(drv_context, page, offset, 1,
		&byte_read);

	return status ? status : byte_read;
}

void mhl_tx_clear_emsc_read_err(void *drv_context)
{

	if (use_spi) {
		spi_mem.tx_buf[0] = spi_op_clear_status;
		spi_mem.tx_buf[1] = 0x02;
		spi_write(spi_dev, spi_mem.tx_buf, 2);
	}
}

int mhl_tx_write_reg_block(void *drv_context, u16 address, u16 count,
	u8 *values)
{
	u8 page = (u8)(address >> 8);
	u8 offset = (u8)address;

	if (use_spi)
		return mhl_tx_write_reg_block_spi(drv_context, page, offset,
			count, values);
	else
		return mhl_tx_write_reg_block_i2c(drv_context, page, offset,
			count, values);
}

void si_mhl_tx_platform_get_block_buffer_info(struct block_buffer_info_t
	*block_buffer_info)
{
	if (use_spi) {
		block_buffer_info->buffer = spi_mem.block_tx_buffers;
		block_buffer_info->req_size = MAX_SPI_EMSC_BLOCK_SIZE;
		block_buffer_info->payload_offset = EMSC_WRITE_SPI_CMD_SIZE;
	} else {
		block_buffer_info->buffer = i2c_mem.block_tx_buffers;
		block_buffer_info->req_size = MAX_I2C_EMSC_BLOCK_SIZE;
		block_buffer_info->payload_offset = MAX_I2C_CMD_SIZE;
	}
}

int mhl_tx_write_block_spi_emsc(void *drv_context, struct block_req *req)
{
	u16 length;
	int ret;

	/* DUMP_SPI_TRANSFER(page, offset, req->count, req->payload->as_bytes,
	 * true);
	 */

	/* dummy bytes will always be zero */
	length = EMSC_WRITE_SPI_CMD_SIZE + req->count;

	if (length > MAX_SPI_EMSC_BLOCK_SIZE) {
		MHL_TX_DBG_ERR("Transfer count (%d) is too large!\n",
			req->count);
		return -EINVAL;
	}

	req->platform_header[0] = spi_op_emsc_write;

	ret = spi_write(spi_dev, req->platform_header, length);

	if (ret != 0) {
		MHL_TX_DBG_ERR("SPI write block failed\n");
		ret = -EIO;
	} else {
		ret = 0;
	}

	return ret;
}

int mhl_tx_write_reg(void *drv_context, u16 address, u8 value)
{
	u8 page = (u8)(address >> 8);
	u8 offset = (u8)address;

	if (use_spi)
		return mhl_tx_write_reg_spi(drv_context, page, offset, value);
	else
		return mhl_tx_write_reg_i2c(drv_context, page, offset, value);
}

int mhl_tx_read_reg_block(void *drv_context, u16 address, u16 count, u8 *values)
{
	u8 page = (u8)(address >> 8);
	u8 offset = (u8)address;

	if (use_spi)
		return mhl_tx_read_reg_block_spi(drv_context, page, offset,
						 count, values);
	else
		return mhl_tx_read_reg_block_i2c(drv_context, page, offset,
						 count, values);
}

int mhl_tx_read_reg(void *drv_context, u16 address)
{
	u8 page = (u8)(address >> 8);
	u8 offset = (u8)address;

	if (use_spi)
		return mhl_tx_read_reg_spi(drv_context, page, offset);
	else
		return mhl_tx_read_reg_i2c(drv_context, page, offset);
}

int mhl_tx_modify_reg(void *drv_context, u16 address, u8 mask, u8 value)
{
	int reg_value;
	int write_status;

	reg_value = mhl_tx_read_reg(drv_context, address);
	if (reg_value < 0)
		return reg_value;

	reg_value &= ~mask;
	reg_value |= mask & value;

	write_status = mhl_tx_write_reg(drv_context, address, reg_value);

	if (write_status < 0)
		return write_status;
	else
		return reg_value;
}

/*
 * Return a value indicating how upstream HPD is
 * implemented on this platform.
 */
enum hpd_control_mode platform_get_hpd_control_mode(void)
{
	return HPD_CTRL_PUSH_PULL;
}

static int gpio_expander_transfer(u8 offset, u16 count, u8 *values, bool write)
{
	struct i2c_msg msg[2];
	u8 buf[8];
	int msg_count;
	int ret;

	if ((count + 1) > ARRAY_SIZE(buf))
		return -1;

	if (write) {
		buf[0] = offset;
		memmove(&buf[1], values, count);

		msg[0].flags = 0;
		msg[0].addr = GPIO_EXP_ADDR >> 1;
		msg[0].buf = buf;
		msg[0].len = count + 1;

		msg_count = 1;

	} else {

		msg[0].flags = 0;
		msg[0].addr = GPIO_EXP_ADDR >> 1;
		msg[0].buf = &offset;
		msg[0].len = 1;

		msg[1].flags = I2C_M_RD;
		msg[1].addr = GPIO_EXP_ADDR >> 1;
		msg[1].buf = values;
		msg[1].len = count;
		msg_count = 2;
	}

	ret = i2c_transfer(i2c_bus_adapter, msg, msg_count);
	if (ret != msg_count) {
		MHL_TX_DBG_ERR("I2c %s failed ret:%d, page: 0x%02x, "
			       "register: 0x%02x count:0x%x\n",
			       write ? "write" : "read", ret, GPIO_EXP_ADDR,
			       offset, count);
		ret = -EIO;
	} else {
		ret = 0;
		/*DUMP_I2C_TRANSFER(GPIO_EXP_ADDR, offset, count, values,
		 * write);
		 */
	}

	return ret;
}

static int gpio_expander_init(void)
{
	u8 gpio_exp_mask_init[] = { GPIO_EXP_INTR_MASK_REGS_OFFSET,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	u8 gpio_exp_pol_invert_init[] = { GPIO_EXP_POL_INVERT_REGS_OFFSET,
		0x00, 0x00, 0x00, 0x00, 0x00
	};
	u8 gpio_exp_output_init[] = { GPIO_EXP_OUTPUT_REGS_OFFSET, 0x00, 0x00,
		GPIO_EXP_BANK_2_OUTPUT_DEFAULT,
		GPIO_EXP_BANK_3_OUTPUT_DEFAULT,
		GPIO_EXP_BANK_4_OUTPUT_DEFAULT
	};
	u8 gpio_exp_io_config_init[] = { GPIO_EXP_IO_CONFIG_REGS_OFFSET,
		0xFF, 0xFF, 0x00, 0x18, 0x0F
	};
	int ret;

	/* First reset GPIO Expander (and 8620) */
	toggle_BB_RST(10);

	ret = gpio_expander_transfer(gpio_exp_mask_init[0],
		ARRAY_SIZE(gpio_exp_mask_init) - 1,
		&gpio_exp_mask_init[1], true);
	if (ret != 0) {
		pr_err("%s():%d gpio_expander_transfer failed, error code %d\n",
		       __func__, __LINE__, ret);
		return ret;
	}

	ret = gpio_expander_transfer(gpio_exp_pol_invert_init[0],
		ARRAY_SIZE(gpio_exp_pol_invert_init) - 1,
		&gpio_exp_pol_invert_init[1], true);
	if (ret != 0) {
		pr_err("%s():%d gpio_expander_transfer failed, error code %d\n",
			__func__, __LINE__, ret);
		return ret;
	}
	ret = gpio_expander_transfer(gpio_exp_output_init[0],
				     ARRAY_SIZE(gpio_exp_output_init) - 1,
				     &gpio_exp_output_init[1], true);
	gpio_exp_bank2_output = GPIO_EXP_BANK_2_OUTPUT_DEFAULT;
	gpio_exp_bank3_output = GPIO_EXP_BANK_3_OUTPUT_DEFAULT;
	gpio_exp_bank4_output = GPIO_EXP_BANK_4_OUTPUT_DEFAULT;

	if (ret != 0) {
		pr_err("%s():%d gpio_expander_transfer failed, error code %d\n",
		       __func__, __LINE__, ret);
		return ret;
	}
	ret = gpio_expander_transfer(gpio_exp_io_config_init[0],
				     ARRAY_SIZE(gpio_exp_io_config_init) - 1,
				     &gpio_exp_io_config_init[1], true);
	if (ret != 0) {
		pr_err("%s():%d gpio_expander_transfer failed, error code %d\n",
		       __func__, __LINE__, ret);
	}
	return ret;
}

#if (LINUX_KERNEL_VER >= 308)
static int starter_kit_init(void)
#else
static int __devinit starter_kit_init(void)
#endif
{
	int ret;

	/* Acquire the GPIO pins needed to control the starter kit. */
	ret = gpio_request_array(starter_kit_control_gpios,
		ARRAY_SIZE(starter_kit_control_gpios));
	if (ret < 0) {
		pr_err("%s(): gpio_request_array failed, error code %d\n",
			__func__, ret);
	} else {
		if (expander_enabled)
			ret = gpio_expander_init();
		else
			toggle_BB_RST(100);
		if (ret != 0) {
			gpio_free_array(starter_kit_control_gpios,
				ARRAY_SIZE(starter_kit_control_gpios));
		}
	}
	return ret;
}

#if defined(SIMG_USE_DTS)
static int si_8620_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int value;
	int pmi_gpio9;
	int pmi_gpio10;
	int pm_gpio2;
	int fw_wake;
	int rc = 0;
	struct pinctrl *pinctrl;
	struct pinctrl_state *active_state;
	struct regulator *ldo;

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		MHL_TX_DBG_ERR("%s: failed to get pinctrl\n", __func__);
		rc = -ENODEV;
		goto dt_exit;
	}

	active_state = pinctrl_lookup_state(pinctrl, "mhl_active");
	if (IS_ERR_OR_NULL(active_state)) {
		MHL_TX_DBG_ERR("%s: failed to find mhl_active state\n",
								__func__);
		rc = -ENODEV;
		goto dt_exit;
	}

	if (pinctrl_select_state(pinctrl, active_state)) {
		MHL_TX_DBG_ERR("%s: failed to select mhl active state\n",
								__func__);
	}

	ldo = devm_regulator_get(dev, "ldo25");
	if (IS_ERR_OR_NULL(ldo)) {
		MHL_TX_DBG_ERR("%s: Unable to get regulator\n", __func__);
		rc = -ENODEV;
		goto dt_exit;
	}

	rc = regulator_set_voltage(ldo, 1000000, 1000000);
	if (rc) {
		MHL_TX_DBG_ERR("%s: Unable to set regulator voltage\n",
								__func__);
		goto dt_exit;
	}

	rc = regulator_enable(ldo);
	if (rc) {
		MHL_TX_DBG_ERR("%s: Regulator enable failed\n", __func__);
		goto dt_exit;
	}

	pm_gpio2 = of_get_named_gpio(np, "sil,pwr-gpio", 0);
	pmi_gpio9 = of_get_named_gpio(np, "sil,pmi9", 0);
	pmi_gpio10 = of_get_named_gpio(np, "sil,pmi10", 0);
	fw_wake = of_get_named_gpio(np, "sil,fw-wake", 0);

	if (gpio_is_valid(pmi_gpio9)) {
		rc = gpio_request(pmi_gpio9, "mhl_pmi9");
		if (rc) {
			MHL_TX_DBG_ERR("%s:power_gpio=[%d] req failed:\n",
				__func__, pmi_gpio9);
			goto dt_exit;
		}
		gpio_direction_output(pmi_gpio9, 0);
		gpio_set_value(pmi_gpio9, 1);
	} else {
		MHL_TX_DBG_ERR("%s:power_gpio=[%d] invalid\n",
				__func__, pmi_gpio9);
		rc = pmi_gpio9;
		goto dt_exit;
	}

	if (gpio_is_valid(pmi_gpio10)) {
		rc = gpio_request(pmi_gpio10, "mhl_pmi10");
		if (rc) {
			MHL_TX_DBG_ERR("%s:power_gpio=[%d] req failed:\n",
				__func__, pmi_gpio10);
			goto dt_exit;
		}
		gpio_direction_output(pmi_gpio10, 0);
		gpio_set_value(pmi_gpio10, 1);
	} else {
		MHL_TX_DBG_ERR("%s:power_gpio=[%d] invalid\n",
				__func__, pmi_gpio10);
		rc = pmi_gpio10;
		goto dt_exit;
	}

	if (gpio_is_valid(pm_gpio2)) {
		rc = gpio_request(pm_gpio2, "mhl_pm2");
		if (rc) {
			MHL_TX_DBG_ERR("%s:power_gpio=[%d] req failed:\n",
				__func__, pm_gpio2);
			goto dt_exit;
		}
		gpio_direction_output(pm_gpio2, 0);
		gpio_set_value(pm_gpio2, 0);
	} else {
		MHL_TX_DBG_ERR("%s:power_gpio=[%d] invalid\n",
				__func__, pm_gpio2);
		rc = pm_gpio2;
		goto dt_exit;
	}

	if (gpio_is_valid(fw_wake)) {
		rc = gpio_request(fw_wake, "mhl_fw");
		if (rc) {
			MHL_TX_DBG_ERR("%s:wake_gpio=[%d] req failed:\n",
				__func__, fw_wake);
			return rc;
		}
		gpio_direction_output(fw_wake, 0);
		gpio_set_value(fw_wake, 1);
		platform_signals[TX_FW_WAKE].gpio_number = fw_wake;
	} else {
		MHL_TX_DBG_ERR("%s:wake_gpio=[%d] invalid\n",
				__func__, fw_wake);
		rc = fw_wake;
		goto dt_exit;
	}

	value = of_get_named_gpio_flags(np, "sil,reset-gpio", 0, NULL);
	if (value >= 0) {
		starter_kit_control_gpios[MHL_RESET_INDEX].gpio = value;
		gpio_set_value(starter_kit_control_gpios[MHL_RESET_INDEX].gpio,
									1);
	}

	value = of_get_named_gpio_flags(np, "sil,irq-gpio", 0, NULL);
	if (value >= 0)
		starter_kit_control_gpios[MHL_INT_INDEX].gpio = value;

	/*
	 * Need this for I/O expander in case we're using SPI as
	 * the register I/O.
	 */
	if (!of_property_read_u32(np, "sil,i2c-port", &value))
		i2c_adapter_num = value;

	MHL_TX_DBG_INFO("Resources assigned to driver...\n");
	MHL_TX_DBG_INFO("    Reset GPIO = %d\n",
			starter_kit_control_gpios[MHL_RESET_INDEX].gpio);
	MHL_TX_DBG_INFO("    Interrupt GPIO = %d\n",
			starter_kit_control_gpios[MHL_INT_INDEX].gpio);
	MHL_TX_DBG_INFO("    I2C adapter = %d\n", i2c_adapter_num);
	if (use_spi)
		MHL_TX_DBG_ERR("    SPI adapter = %d\n", spi_bus_num);

dt_exit:
	return rc;
}
#endif

#if (LINUX_KERNEL_VER >= 308)
static int si_8620_mhl_tx_i2c_probe(struct i2c_client *client,
#else
static int __devinit si_8620_mhl_tx_i2c_probe(struct i2c_client *client,
#endif
	const struct i2c_device_id *id)
{
	int ret;

	pr_info("%s(), i2c_device_id = %p\n", __func__, id);

#if defined(SIMG_USE_DTS)
	/*
	 * Modify default driver platform parameters with those
	 * specified in the device tree.
	 */
	if (client->dev.of_node) {
		ret = si_8620_parse_dt(&client->dev);
		if (ret)
			goto done;
	}
#endif
	i2c_bus_adapter = to_i2c_adapter(client->dev.parent);
	device_addresses[0].client = client;

	if (!i2c_bus_adapter ||
		!i2c_check_functionality(i2c_bus_adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		MHL_TX_DBG_ERR("[ERROR] i2c function check failed\n");
		ret = -EIO;
		goto done;
	}
	i2c_mem.block_tx_buffers = kmalloc(MAX_I2C_EMSC_BLOCK_SIZE *
		NUM_BLOCK_QUEUE_REQUESTS, GFP_KERNEL);
	if (NULL == i2c_mem.block_tx_buffers) {
		ret = -ENOMEM;
		goto done;
	}

	ret = starter_kit_init();
	if (ret >= 0) {
		drv_info.irq = gpio_to_irq(
			starter_kit_control_gpios[MHL_INT_INDEX].gpio);
		ret = mhl_tx_init(&drv_info, &client->dev);
		if (ret) {
			MHL_TX_DBG_ERR("mhl_tx_init failed, error code %d\n",
				ret);
			gpio_free_array(starter_kit_control_gpios,
				ARRAY_SIZE(starter_kit_control_gpios));
		}
	}
done:
	if (ret != 0) {
		kfree(i2c_mem.block_tx_buffers);
		probe_fail = true;
	}
	return ret;
}

#if (LINUX_KERNEL_VER >= 308)
static int si_8620_mhl_tx_remove(struct i2c_client *client)
#else
static int __devexit si_8620_mhl_tx_remove(struct i2c_client *client)
#endif
{
	if (!use_spi)
		kfree(i2c_mem.block_tx_buffers);

	gpio_free_array(starter_kit_control_gpios,
		ARRAY_SIZE(starter_kit_control_gpios));
	return 0;
}

int si_8620_pm_suspend(struct device *dev)
{
	int status = -EINVAL;

	if (dev == 0)
		goto done;

	status = down_interruptible(&platform_lock);
	if (status)
		goto done;

	status = mhl_handle_power_change_request(dev, false);
	/*
	 * Set MHL/USB switch to USB
	 * NOTE: Switch control is implemented differently on each
	 * version of the starter kit.
	 */
	set_pin(X02_USB_SW_CTRL, 0);

	up(&platform_lock);
done:
	return status;
}

int si_8620_pm_resume(struct device *dev)
{
	int status = -EINVAL;

	if (dev == 0)
		goto done;

	status = down_interruptible(&platform_lock);
	if (status)
		goto done;

	set_pin(X02_USB_SW_CTRL, 1);
	status = mhl_handle_power_change_request(dev, true);

	up(&platform_lock);
done:
	return status;
}

int si_8620_power_control(bool power_up)
{
	struct device *dev = NULL;
	int status;

	if (use_spi)
		dev = &spi_dev->dev;
	else
		dev = &device_addresses[0].client->dev;

	if (power_up)
		status = si_8620_pm_resume(dev);
	else
		status = si_8620_pm_suspend(dev);
	return status;
}
EXPORT_SYMBOL_GPL(si_8620_power_control);

int si_8620_get_hpd_status(int *hpd_status)
{
	struct device *dev = NULL;
	int status = 0;
	struct mhl_dev_context *dev_context;

	if (use_spi)
		dev = &spi_dev->dev;
	else
		dev = &device_addresses[0].client->dev;

	dev_context = dev_get_drvdata(dev);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("%scouldn't acquire mutex%s\n",
			ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
		return  -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("%sshutting down%s\n",
			ANSI_ESC_YELLOW_TEXT, ANSI_ESC_RESET_TEXT);
		status = -ENODEV;
	} else {
		*hpd_status = si_mhl_tx_drv_get_hpd_status(dev_context);
		MHL_TX_DBG_INFO("%HPD status: %s%d%s\n",
			ANSI_ESC_YELLOW_TEXT,
			*hpd_status,
			ANSI_ESC_RESET_TEXT);
	}
	up(&dev_context->isr_lock);
	return status;
}
EXPORT_SYMBOL_GPL(si_8620_get_hpd_status);

int si_8620_get_hdcp2_status(uint32_t *hdcp2_status)
{
	struct device *dev = NULL;
	int status = 0;
	struct mhl_dev_context *dev_context;

	if (use_spi)
		dev = &spi_dev->dev;
	else
		dev = &device_addresses[0].client->dev;

	dev_context = dev_get_drvdata(dev);

	if (down_interruptible(&dev_context->isr_lock)) {
		MHL_TX_DBG_ERR("%scouldn't acquire mutex%s\n",
			ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
		return  -ERESTARTSYS;
	}

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		MHL_TX_DBG_ERR("%sshutting down%s\n",
			ANSI_ESC_YELLOW_TEXT, ANSI_ESC_RESET_TEXT);
		status = -ENODEV;
	} else {
		*hdcp2_status = si_mhl_tx_drv_get_hdcp2_status(dev_context);
		MHL_TX_DBG_INFO("%HDCP2 status: %s0x%8x%s\n",
			ANSI_ESC_YELLOW_TEXT,
			*hdcp2_status,
			ANSI_ESC_RESET_TEXT);
	}
	up(&dev_context->isr_lock);
	return status;
}
EXPORT_SYMBOL_GPL(si_8620_get_hdcp2_status);

static const struct i2c_device_id si_8620_mhl_tx_id[] = {
	{MHL_DEVICE_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, si_8620_mhl_tx_id);

static const struct dev_pm_ops si_8620_tx_pm_ops = {
	.runtime_suspend = si_8620_pm_suspend,
	.runtime_resume = si_8620_pm_resume,
};

#ifdef SIMG_USE_DTS
static struct of_device_id si_8620_of_match_table[] = {
	{
		.compatible = "sil,sii-8620",
	},
	{}
};
#endif

static struct i2c_driver si_8620_mhl_tx_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = MHL_DRIVER_NAME,
#ifdef SIMG_USE_DTS
		   .of_match_table = si_8620_of_match_table,
#endif
		   .pm = &si_8620_tx_pm_ops,
		   },
	.id_table = si_8620_mhl_tx_id,
	.probe = si_8620_mhl_tx_i2c_probe,

#if (LINUX_KERNEL_VER >= 308)
	.remove = si_8620_mhl_tx_remove,
#else
	.remove = __devexit_p(si_8620_mhl_tx_remove),
#endif
	.command = NULL,
};

#ifndef SIMG_USE_DTS
static struct i2c_board_info __initdata si_8620_i2c_boardinfo[] = {
	{
	 I2C_BOARD_INFO(MHL_DEVICE_NAME, (SA_TX_PAGE_0 >> 1)),
	 .flags = I2C_CLIENT_WAKE,
	 .irq = -1
	 }
};
#endif

#if (LINUX_KERNEL_VER >= 308)
static int si_8620_mhl_tx_spi_probe(struct spi_device *spi)
#else
static int __devinit si_8620_mhl_tx_spi_probe(struct spi_device *spi)
#endif
{
	int ret;

	pr_info("%s(), spi = %p\n", __func__, spi);
	spi->bits_per_word = 8;
	spi_dev = spi;
	spi_bus_num = spi->master->bus_num;

#if defined(SIMG_USE_DTS)
	/*
	 * Modify default driver platform parameters with those
	 * specified in the device tree.
	 */
	if (spi_dev->dev.of_node) {
		ret = si_8620_parse_dt(&spi_dev->dev);
		if (ret)
			goto failed;
	}

	/*
	 * Because we're using SPI for register access, we need to
	 * obtain separate i2c access for the I/O expander, so we
	 * use the I2C adapter number we got from the device tree.
	 */
	i2c_bus_adapter = i2c_get_adapter(i2c_adapter_num);
	if (i2c_bus_adapter == NULL) {
		pr_err("%s() failed to get i2c adapter %d\n", __func__,
			i2c_adapter_num);
		ret = -EFAULT;
		goto failed;
	}

	if (!i2c_check_functionality(i2c_bus_adapter,
		I2C_FUNC_SMBUS_BYTE_DATA)) {
		MHL_TX_DBG_ERR("[ERROR] i2c function check failed\n");
		ret = -EIO;
		goto failed;
	}
#endif

	spi_mem.tx_buf = kmalloc(MAX_SPI_XFER_BUFFER_SIZE, GFP_KERNEL);
	spi_mem.rx_buf = kmalloc(MAX_SPI_XFER_BUFFER_SIZE, GFP_KERNEL);
	spi_mem.block_tx_buffers = kmalloc(MAX_SPI_EMSC_BLOCK_SIZE *
		NUM_BLOCK_QUEUE_REQUESTS, GFP_KERNEL);
	if (!spi_mem.tx_buf || !spi_mem.rx_buf || !spi_mem.block_tx_buffers) {
		ret = -ENOMEM;
		goto mem_cleanup;
	}
	ret = starter_kit_init();
	if (ret >= 0) {
		drv_info.irq = gpio_to_irq(
			starter_kit_control_gpios[MHL_INT_INDEX].gpio);
		ret = mhl_tx_init(&drv_info, &spi_dev->dev);
		if (ret) {
			pr_err("%s(): mhl_tx_init failed, error code %d\n",
				__func__, ret);
			gpio_free_array(starter_kit_control_gpios,
					ARRAY_SIZE(starter_kit_control_gpios));
			goto mem_cleanup;
		}
		goto done;
	}

mem_cleanup:
	kfree(spi_mem.tx_buf);
	spi_mem.tx_buf = NULL;

	kfree(spi_mem.rx_buf);
	spi_mem.rx_buf = NULL;

	kfree(spi_mem.block_tx_buffers);
	spi_mem.block_tx_buffers = NULL;

#if defined(SIMG_USE_DTS)
failed:
#endif
	probe_fail = true;

done:
	return ret;
}

#if (LINUX_KERNEL_VER >= 308)
static int si_8620_mhl_spi_remove(struct spi_device *spi_dev)
#else
static int __devexit si_8620_mhl_spi_remove(struct spi_device *spi_dev)
#endif
{
	pr_info("%s() called\n", __func__);

	gpio_free_array(starter_kit_control_gpios,
			ARRAY_SIZE(starter_kit_control_gpios));
	kfree(spi_mem.tx_buf);
	kfree(spi_mem.rx_buf);
	kfree(spi_mem.block_tx_buffers);
	return 0;
}

static struct spi_driver si_86x0_mhl_tx_spi_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = MHL_DRIVER_NAME,
#ifdef SIMG_USE_DTS
		.of_match_table = si_8620_of_match_table,
#endif
		.pm = &si_8620_tx_pm_ops,
		},
	.probe = si_8620_mhl_tx_spi_probe,
#if (LINUX_KERNEL_VER >= 308)
	.remove = si_8620_mhl_spi_remove,
#else
	.remove = __devexit_p(si_8620_mhl_spi_remove),
#endif
};

#ifndef SIMG_USE_DTS
static struct spi_board_info __initdata si_86x0_spi_board_info = {
	.modalias = MHL_DRIVER_NAME,
	.max_speed_hz = SPI_BUS_SPEED,
	.bus_num = SPI_BUS_NUM,
	.chip_select = SPI_CHIP_SEL,
	.mode = SPI_TRANSFER_MODE,
	.irq = -1
};

static int __init add_spi_device_to_bus(void)
{
	struct spi_master *spi_master;
	int status = 0;

	pr_info("add_spi_device_to_bus called\n");

	spi_master = spi_busnum_to_master(SPI_BUS_NUM);
	if (spi_master == NULL) {
		pr_err("spi_busnum_to_master(%d) returned NULL\n", SPI_BUS_NUM);
		return -1;
	}

	spi_dev = spi_new_device(spi_master, &si_86x0_spi_board_info);
	if (spi_dev == NULL || probe_fail) {
		pr_err("spi_new_device() failed\n");
		if (spi_dev)
			spi_unregister_device(spi_dev);
		status = -1;
		goto exit;
	}

exit:
	put_device(&spi_master->dev);
	return status;
}
#endif

static int __init spi_init(void)
{
	int status;

	status = spi_register_driver(&si_86x0_mhl_tx_spi_driver);
	if (status < 0) {
		pr_err("[ERROR] %s():%d failed !\n", __func__, __LINE__);
		goto done;
	}

#ifndef SIMG_USE_DTS
	status = add_spi_device_to_bus();
	if (status < 0)
		MHL_TX_DBG_ERR("Failed to add MHL transmitter as SPI device\n");
#endif

	if (probe_fail || status < 0) {
		spi_unregister_driver(&si_86x0_mhl_tx_spi_driver);
		status = -ENODEV;
	}
done:
	return status;
}

static int __init i2c_init(void)
{
#ifndef SIMG_USE_DTS
	struct i2c_client *client;
	int idx;
#endif
	int ret = -ENODEV;

#ifndef SIMG_USE_DTS
	/* "Hotplug" the MHL transmitter device onto the 2nd I2C bus */
	i2c_bus_adapter = i2c_get_adapter(I2C_ADAPTER);
	if (i2c_bus_adapter == NULL)
		goto done;

	for (idx = 0; idx < ARRAY_SIZE(device_addresses); idx++) {
		if (idx == 0) {
			client =
			    i2c_new_device(i2c_bus_adapter,
					   &si_8620_i2c_boardinfo[idx]);
			device_addresses[idx].client = client;
		} else {
			device_addresses[idx].client =
			    i2c_new_dummy(i2c_bus_adapter,
					  device_addresses[idx].dev_addr);
		}
		if (device_addresses[idx].client == NULL) {
			MHL_TX_DBG_INFO("returning %d\n", ret);
			goto err_exit;
		}
	}
#endif
	ret = i2c_add_driver(&si_8620_mhl_tx_i2c_driver);
	if (ret < 0 || probe_fail) {
		if (ret == 0)
			i2c_del_driver(&si_8620_mhl_tx_i2c_driver);
		MHL_TX_DBG_INFO("failed !\n\nCHECK POWER AND CONNECTION "
						"TO CP8620 Starter Kit.\n\n");
		goto err_exit;
	}

	goto done;

err_exit:
#ifndef SIMG_USE_DTS
	for (idx = 0; idx < ARRAY_SIZE(device_addresses); idx++) {
		if (device_addresses[idx].client != NULL)
			i2c_unregister_device(device_addresses[idx].client);
	}
#endif

done:
	if (probe_fail)
		ret = -ENODEV;

	MHL_TX_DBG_INFO("returning %d\n", ret);
	return ret;
}

static int __init si_8620_init(void)
{
	int ret;

	pr_info("mhl: Starting SiI%d Driver v%s\n", MHL_PRODUCT_NUM,
		buildVersion);
	pr_info("mhl: %s\n", buildTime);
#if (INCLUDE_HID == 1)
	pr_info("mhl: Supports MHL3 HID\n");
#endif

	sema_init(&platform_lock, 1);

	platform_flags &= ~PLATFORM_FLAG_HEARTBEAT_MASK;
	switch (use_heartbeat) {
	case 0:
		/* don't do anything with heatbeat */
		break;
	case 1:
		platform_flags |= PLATFORM_VALUE_ISSUE_HEARTBEAT;
		break;
	case 2:
		platform_flags |= PLATFORM_VALUE_DISCONN_HEARTBEAT;
		break;
	default:
		MHL_TX_DBG_ERR("%sinvalid use_heartbeat parameter%s\n",
			ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
	}

	if (tmds_link_speed == 15)
		platform_flags |= PLATFORM_FLAG_1_5GBPS;
	else if (tmds_link_speed == 3)
		platform_flags |= PLATFORM_FLAG_3GBPS;
	else if (tmds_link_speed == 6)
		platform_flags |= PLATFORM_FLAG_6GBPS;

	/*
	 * NOTE: Even if the user selects to communicate with the MHL
	 * transmitter via SPI we still need I2C to communicate with
	 * other devices on the starter kit board.
	 */
#ifndef SIMG_USE_DTS
	i2c_bus_adapter = i2c_get_adapter(I2C_ADAPTER);
	if (i2c_bus_adapter == NULL) {
		pr_err("%s() failed to get i2c adapter %d\n", __func__,
			(int)I2C_ADAPTER);
		return -EFAULT;
	}
#endif
	if (use_spi)
		ret = spi_init();
	else
		ret = i2c_init();

	return ret;
}

static void __exit si_8620_exit(void)
{
#ifndef SIMG_USE_DTS
	int idx;
#endif

	pr_info("si_8620_exit called\n");

	si_8620_power_control(false);

	if (use_spi) {
		mhl_tx_remove(&spi_dev->dev);
		spi_unregister_driver(&si_86x0_mhl_tx_spi_driver);
#ifndef SIMG_USE_DTS
		spi_unregister_device(spi_dev);
#endif
	} else {
		if (device_addresses[0].client != NULL) {
			mhl_tx_remove(&device_addresses[0].client->dev);
			MHL_TX_DBG_INFO("client removed\n");
		}
		i2c_del_driver(&si_8620_mhl_tx_i2c_driver);
		MHL_TX_DBG_INFO("i2c driver deleted from context\n");

#ifndef SIMG_USE_DTS
		for (idx = 0; idx < ARRAY_SIZE(device_addresses); idx++) {
			MHL_TX_DBG_INFO("\n");
			if (device_addresses[idx].client != NULL) {
				MHL_TX_DBG_INFO("unregistering device:%p\n",
						device_addresses[idx].client);
				i2c_unregister_device(device_addresses[idx].
						      client);
			}
		}
#endif
	}
	MHL_TX_DBG_ERR("driver unloaded.\n");
}

static int debug_level_stack[15];
static unsigned int debug_level_stack_ptr;

void push_debug_level(int new_verbosity)
{
	if (debug_level_stack_ptr < ARRAY_SIZE(debug_level_stack)) {
		/* stack is initially empty */
		debug_level_stack[debug_level_stack_ptr++] = debug_level;
		debug_level = new_verbosity;
	} else {
		MHL_TX_DBG_ERR("%sdebug_level_stack overflowed%s\n",
			       ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
	}
}

void pop_debug_level(void)
{
	if (debug_level_stack_ptr > 0) {
		if (debug_level_stack_ptr > ARRAY_SIZE(debug_level_stack)) {
			MHL_TX_DBG_ERR("%sdebug_level_stack overflowed%s\n",
				       ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
		} else {
			debug_level =
			    debug_level_stack[--debug_level_stack_ptr];
		}
	}
}

int si_8620_register_callbacks(struct si_mhl_callback_api_t *p_callbacks)
{
	struct device *dev = NULL;
	int status = 0;
	struct mhl_dev_context *dev_context;
	struct drv_hw_context *hw_context;

	if (use_spi)
		dev = &spi_dev->dev;
	else
		dev = &device_addresses[0].client->dev;

	dev_context = dev_get_drvdata(dev);
	hw_context = (struct drv_hw_context *)&dev_context->drv_context;

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		if (NULL != p_callbacks) {
			if (p_callbacks->context)
				hw_context->
					callbacks.context =
					p_callbacks->context;
			if (p_callbacks->display_timing_enum_begin)
				hw_context->
					callbacks.display_timing_enum_begin =
					p_callbacks->display_timing_enum_begin;
			if (p_callbacks->display_timing_enum_item)
				hw_context->
					callbacks.display_timing_enum_item =
					p_callbacks->display_timing_enum_item;
			if (p_callbacks->display_timing_enum_end)
				hw_context->
					callbacks.display_timing_enum_end =
					p_callbacks->display_timing_enum_end;
			if (p_callbacks->hpd_driven_low)
				hw_context->
					callbacks.hpd_driven_low =
					p_callbacks->hpd_driven_low;
			if (p_callbacks->hpd_driven_high)
				hw_context->
					callbacks.hpd_driven_high =
					p_callbacks->hpd_driven_high;
		}
	}

	up(&dev_context->isr_lock);
	return status;
}
EXPORT_SYMBOL(si_8620_register_callbacks);

int si_8620_info_frame_change(enum hpd_high_callback_status mode_parm,
	union avif_or_cea_861_dtd_u *p_avif_or_dtd,
	size_t avif_or_dtd_max_length, union vsif_mhl3_or_hdmi_u *p_vsif,
	size_t vsif_max_length)
{
	struct device *dev = NULL;
	int status;
	struct mhl_dev_context *dev_context;
	struct drv_hw_context *hw_context;

	if (use_spi)
		dev = &spi_dev->dev;
	else
		dev = &device_addresses[0].client->dev;

	dev_context = dev_get_drvdata(dev);
	hw_context = (struct drv_hw_context *)&dev_context->drv_context;

	if (down_interruptible(&dev_context->isr_lock))
		return -ERESTARTSYS;

	if (dev_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {
		size_t xfer_size;

		memset(&hw_context->vsif_mhl3_or_hdmi_from_callback, 0,
			sizeof(hw_context->vsif_mhl3_or_hdmi_from_callback));
		memset(&hw_context->avif_or_dtd_from_callback, 0,
			sizeof(hw_context->avif_or_dtd_from_callback));

		if (sizeof(hw_context->vsif_mhl3_or_hdmi_from_callback) <
			vsif_max_length) {
			xfer_size = sizeof(
				hw_context->vsif_mhl3_or_hdmi_from_callback);
		} else {
			xfer_size = vsif_max_length;
		}
		memcpy(&hw_context->vsif_mhl3_or_hdmi_from_callback, p_vsif,
			xfer_size);

		if (sizeof(hw_context->avif_or_dtd_from_callback) <
			avif_or_dtd_max_length) {
			xfer_size = sizeof(
				hw_context->avif_or_dtd_from_callback);
		} else {
			xfer_size = avif_or_dtd_max_length;
		}
		memcpy(&hw_context->avif_or_dtd_from_callback, p_avif_or_dtd,
			xfer_size);

		status = si_mhl_tx_drv_set_display_mode(dev_context, mode_parm);
	}

	up(&dev_context->isr_lock);
	return status;
}
EXPORT_SYMBOL(si_8620_info_frame_change);

module_init(si_8620_init);
module_exit(si_8620_exit);

MODULE_DESCRIPTION("Silicon Image MHL Transmitter driver");
MODULE_AUTHOR("Silicon Image <http://www.siliconimage.com>");
MODULE_LICENSE("GPL");
