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

#if !defined(PLATFORM_H)
#define PLATFORM_H

#define DEVICE_ID_8620 0x8620

#define DEBUG_I2C_WRITE			1
#define DEBUG_I2C_READ			0
#define MAX_DEBUG_TRANSFER_SIZE		32

enum dbg_msg_level {
	DBG_MSG_LEVEL_ERR,
	DBG_MSG_LEVEL_WARN,
	DBG_MSG_LEVEL_INFO,
	DBG_MSG_LEVEL_GPIO,
	DBG_MSG_LEVEL_EDID_INFO,
	DBG_MSG_LEVEL_COMM_INFO
};

enum tx_interface_types {
	TX_INTERFACE_TYPE_I2C,
	TX_INTERFACE_TYPE_SPI
};

#if defined(DEBUG)

void print_formatted_debug_msg(char *file_spec, const char *func_name,
			       int line_num, char *fmt, ...);

void dump_transfer(enum tx_interface_types if_type,
		   u8 page, u8 offset, u16 count, u8 *values, bool write);

#define MHL_TX_PROXY_DBG_PRINT(level, function, iLine, ...) \
{				\
	if ((level) <= debug_level) \
		print_formatted_debug_msg(NULL, function, iLine, __VA_ARGS__); \
}

#define MHL_TX_GENERIC_DBG_PRINT(level, ...) \
{				\
	if ((level) <= debug_level) \
		print_formatted_debug_msg(NULL, __func__, __LINE__, \
		__VA_ARGS__); \
}

#define MHL_TX_COMM_INFO(...) \
	MHL_TX_GENERIC_DBG_PRINT(DBG_MSG_LEVEL_COMM_INFO, __VA_ARGS__)

#define MHL_TX_EDID_INFO(...) \
	MHL_TX_GENERIC_DBG_PRINT(DBG_MSG_LEVEL_EDID_INFO, __VA_ARGS__)

#define MHL_TX_DBG_GPIO(...) \
	MHL_TX_GENERIC_DBG_PRINT(DBG_MSG_LEVEL_GPIO, __VA_ARGS__)

#define MHL_TX_DBG_INFO(...) \
	MHL_TX_GENERIC_DBG_PRINT(DBG_MSG_LEVEL_INFO, __VA_ARGS__)

#define MHL_TX_DBG_WARN(...) \
	MHL_TX_GENERIC_DBG_PRINT(DBG_MSG_LEVEL_WARN, __VA_ARGS__)

#define MHL_TX_DBG_ERR(...)			\
	MHL_TX_GENERIC_DBG_PRINT(DBG_MSG_LEVEL_ERR, __VA_ARGS__)

#define DUMP_I2C_TRANSFER(page, offset, count, values, write_flag)	\
	dump_transfer(TX_INTERFACE_TYPE_I2C, page, offset,		\
	count, values, write_flag);

#define DUMP_SPI_TRANSFER(page, offset, count, values, write_flag)	\
	dump_transfer(TX_INTERFACE_TYPE_SPI, page, offset,		\
	count, values, write_flag);
#else

#define MHL_TX_PROXY_DBG_PRINT(level, function, iLine, ...)
#define MHL_TX_GENERIC_DBG_PRINT(level, ...)
#define MHL_TX_COMM_INFO(fmt, ...)
#define MHL_TX_EDID_INFO(fmt, ...)
#define MHL_TX_DBG_GPIO(fmt, ...)
#define MHL_TX_DBG_INFO(fmt, ...)
#define MHL_TX_DBG_WARN(fmt, ...)
#define MHL_TX_DBG_ERR(fmt, ...)

#define DUMP_I2C_TRANSFER(page, offset, count, values, write_flag)
#define DUMP_SPI_TRANSFER(page, offset, count, values, write_flag)

#endif

void push_debug_level(int new_verbosity);
void pop_debug_level(void);

enum vbus_power_state {
	VBUS_OFF,
	VBUS_ON
};

enum hpd_control_mode {
	HPD_CTRL_MODE_ERROR = -1,
	HPD_CTRL_OPEN_DRAIN,
	HPD_CTRL_PUSH_PULL
};

enum hpd_control_mode platform_get_hpd_control_mode(void);
void platform_mhl_tx_hw_reset(uint32_t reset_period, uint32_t reset_delay);
void mhl_tx_vbus_control(enum vbus_power_state power_state);
void mhl_tx_vbus_current_ctl(uint16_t max_current_in_milliamps);

struct platform_reg_pair {
	uint8_t slave_addr;
	uint8_t offset;
};

struct platform_signals_list {
	char *name;
	int16_t gpio_number;
	struct platform_reg_pair gpio_reg_PCA950x;
	uint8_t gpio_mask_PCA950x;
	uint8_t *gpio_bank_value;
	bool *param;
};
struct block_buffer_info_t {
	uint8_t *buffer;
	uint8_t payload_offset;
	size_t req_size;
};
void si_mhl_tx_platform_get_block_buffer_info(
	struct block_buffer_info_t *block_buffer_info);
int mhl_tx_write_block_spi_emsc(void *drv_context, struct block_req *req);
int mhl_tx_read_spi_emsc(void *drv_context, u16 count, u8 *values);
void mhl_tx_clear_emsc_read_err(void *drv_context);

int mhl_tx_write_reg(void *drv_context, u16 address, u8 value);
int mhl_tx_read_reg(void *drv_context, u16 address);
int mhl_tx_write_reg_block(void *drv_context, u16 address, u16 count,
	u8 *values);
int mhl_tx_read_reg_block(void *drv_context, u16 address, u16 count,
	u8 *values);
int mhl_tx_modify_reg(void *drv_context, u16 address, u8 mask, u8 value);

#ifdef DEBUG
int si_8620_power_control(bool power_up);
#endif

uint32_t platform_get_flags(void);
#define PLATFORM_FLAG_HEARTBEAT_MASK		0x00000030
#define PLATFORM_VALUE_ISSUE_HEARTBEAT		0x00000010
#define PLATFORM_VALUE_DISCONN_HEARTBEAT	0x00000020
#define PLATFORM_FLAG_LINK_SPEED		0x000000C0
#define PLATFORM_FLAG_6GBPS			0x000000C0
#define PLATFORM_FLAG_3GBPS			0x00000080
#define PLATFORM_FLAG_1_5GBPS			0x00000040

int is_interrupt_asserted(void);
int get_config(void *dev_context, int config_idx);

#define GPIO_LED_ON	0
#define GPIO_LED_OFF	1
void set_pin_impl(/*void *dev_context,*/ int pin_idx, int value,
		  const char *function_name, int line_num);
#define set_pin(/*dev_context,*/ pin_idx, value) \
	set_pin_impl(/*dev_context,*/ pin_idx, value, __func__, __LINE__)

extern bool source_vbus_on;
extern bool bpp_on_wb;
extern bool use_spi;
extern bool wait_for_user_intr;
extern int debug_level;
extern bool debug_reg_dump;
extern bool force_ocbus_for_ects;
extern int crystal_khz;
extern int gpio_index;
extern int hdcp_content_type;
extern bool input_dev_rap;
#if (INCLUDE_RBP == 1)
extern bool input_dev_rbp;
#endif
extern bool input_dev_rcp;
extern bool input_dev_ucp;

/* Starter kit board signal control index definitions */
#define TX_HW_RESET			0
#define TX_FW_WAKE			1
#define CHG_DET				2
#define XO3_SINK_VBUS_SENSE		3
#define TWELVE_VOLT_PS_SENSE		4
#define EEPROM_WR_EN			5
#define TX2MHLRX_PWR			6
#define M2U_VBUS_CTRL			7
#define LED_3D				8
#define LED_PACKED_PIXEL		9
#define LED_HDCP			10
#define LED_USB_MODE			11
#define LED_SPARE_2			12
#define LED_SPARE_3			13
#define LED_SPARE_4			14
#define X02_USB_SW_CTRL			15
#define X02_USB_SW_CTRL0		16
#define X02_USB_SW_CTRL1		17
#define X02_USB_LED15_AMBER		18
#define X02_USB_LED15_GREEN		19

#ifdef ANSI_COLORS
#define ANSI_ESC_RESET_TEXT "\x1b[0m"
#define ANSI_ESC_YELLOW_BG "\x1b[43m"
#define ANSI_ESC_WHITE_BG "\x1b[47m"
#define ANSI_ESC_RED_TEXT "\x1b[31m"
#define ANSI_ESC_YELLOW_TEXT "\x1b[33m"
#define ANSI_ESC_GREEN_TEXT "\x1b[32m"
#define ANSI_ESC_BLACK_TEXT "\1b[30m"
#define ANSI_ESC_WHITE_TEXT "\x1b[37m\x1b[1m"
#define ANSI_ESC_MAGENTA_TEXT "\x1b[35m"
#define ANSI_ESC_CYAN_TEXT "\x1b[36m"
#else
#define ANSI_ESC_RESET_TEXT ""
#define ANSI_ESC_WHITE_BG ""
#define ANSI_ESC_RED_TEXT "\n\n"
#define ANSI_ESC_YELLOW_TEXT "\n\n"
#define ANSI_ESC_GREEN_TEXT "\n\n"
#define ANSI_ESC_BLACK_TEXT ""
#define ANSI_ESC_WHITE_TEXT ""
#define ANSI_ESC_MAGENTA_TEXT ""
#define ANSI_ESC_CYAN_TEXT ""
#endif
#endif /* if !defined(PLATFORM_H) */
