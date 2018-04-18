/*

**************************************************************************
**                        STMicroelectronics							**
**************************************************************************
**                        marco.cali@st.com								**
**************************************************************************
*                                                                        *
*							HW related data								 *
*                                                                        *
**************************************************************************
**************************************************************************

*/

/*!
* \file ftsHardware.h
* \brief Contains all the definitions and information related to the IC from an hardware point of view
*/

#ifndef FTS_HARDWARE_H
#define FTS_HARDWARE_H


#define DCHIP_ID_0								0x36
#define DCHIP_ID_1								0x39

#define DCHIP_FW_VER_BYTE						2


#define READ_CHUNK								1024
#define WRITE_CHUNK								1024
#define MEMORY_CHUNK							1024


#define I2C_INTERFACE
#ifdef I2C_INTERFACE
#define I2C_SAD									0x49
#else
#define SPI4_WIRE
#define SPI_DELAY_CS							10
#define SPI_CLOCK_FREQ							7000000
#endif

#define IER_ENABLE								0x41
#define IER_DISABLE								0x00


/** @defgroup flash_command Flash Commands
 *	All the commands that works with the flash of the IC
 *	@{
 */
#define FLASH_CMD_UNLOCK						0xF7

#define FLASH_CMD_READ_REGISTER                 0xFA
#define FLASH_CMD_WRITE_REGISTER				0xFA


#define FLASH_UNLOCK_CODE0						0x25
#define FLASH_UNLOCK_CODE1						0x20


#define FLASH_ERASE_START						0x80
#define FLASH_ERASE_CODE1                       0xC0
#define FLASH_DMA_CODE1                         0xC0
#define FLASH_ERASE_UNLOCK_CODE0				0xDE
#define FLASH_ERASE_UNLOCK_CODE1				0x03
#define FLASH_ERASE_CODE0                       0x6A
#define FLASH_DMA_CODE0                      	0x71
#define FLASH_DMA_CONFIG                        0x72
#define FLASH_NUM_PAGE							32

#define FLASH_CX_PAGE_START						28
#define FLASH_CX_PAGE_END						30
#define FLASH_PANEL_PAGE_START					26
#define FLASH_PANEL_PAGE_END					27

/** @} */



#define FLASH_ADDR_CODE							0x00000000
#define FLASH_ADDR_CONFIG						0x00007C00
#define FLASH_ADDR_CX							0x00007000


/** @defgroup fw_file FW file info
 *	All the info related to the fw file
 *	@{
 */

#define FW_HEADER_SIZE							64
#define FW_HEADER_SIGNATURE						0xAA55AA55
#define FW_FTB_VER								0x00000001
#define FW_BYTES_ALLIGN							4
#define FW_BIN_VER_OFFSET						16
#define FW_BIN_CONFIG_ID_OFFSET					20
#define FW_CX_VERSION							(16+4)

/** @} */


#define FIFO_EVENT_SIZE							8
#define FIFO_DEPTH								32

#ifdef I2C_INTERFACE
#define FIFO_CMD_READALL						0x86
#else
#define FIFO_CMD_READALL						0x87
#endif
#define FIFO_CMD_READONE						FIFO_CMD_READALL


#ifdef I2C_INTERFACE
#define FTS_CMD_HW_REG_R						0xFA
#define FTS_CMD_HW_REG_W						0xFA
#define FTS_CMD_FRAMEBUFFER_R					0xA6
#define FTS_CMD_CONFIG_R						0xA8
#define FTS_CMD_CONFIG_W						0xA8
#else
#define FTS_CMD_HW_REG_R						0xFB
#define FTS_CMD_HW_REG_W						0xFA
#define FTS_CMD_FRAMEBUFFER_R					0xA7
#define FTS_CMD_CONFIG_R						0xA9
#define FTS_CMD_CONFIG_W						0xA8
#endif



#ifndef I2C_INTERFACE
#define DUMMY_HW_REG							1
#define DUMMY_FRAMEBUFFER						1
#define DUMMY_CONFIG							1
#define DUMMY_FIFO								1
#else
#define DUMMY_HW_REG							0
#define DUMMY_FRAMEBUFFER						0
#define DUMMY_CONFIG							0
#define DUMMY_FIFO								0
#endif

/** @defgroup hw_adr HW Address
 * @ingroup address
 * Important addresses of hardware registers (and sometimes their important values)
 * @{
 */


#define ADDR_FRAMEBUFFER			(u64)0x0000000000000000
#define ADDR_ERROR_DUMP				(u64)0x000000000000EF80


#define ADDR_SYSTEM_RESET			(u64)0x0000000020000024
#define SYSTEM_RESET_VALUE						0x80


#define ADDR_BOOT_OPTION			(u64)0x0000000020000025


#define ADDR_IER					(u64)0x0000000020000029


#define ADDR_DCHIP_ID				(u64)0x0000000020000000
#define ADDR_DCHIP_FW_VER			(u64)0x0000000020000004


#define ADDR_ICR					(u64)0x000000002000002D
#define SPI4_MASK					0x02


#define ADDR_CRC					(u64)0x0000000020000078
#define CRC_MASK								0x03

#define ADDR_CONFIG_OFFSET			(u64)0x0000000000000000

#define ADDR_GPIO_INPUT				(u64)0x0000000020000030
#define ADDR_GPIO_DIRECTION			(u64)0x0000000020000032
#define ADDR_GPIO_PULLUP			(u64)0x0000000020000034
#define ADDR_GPIO_CONFIG_REG0		(u64)0x000000002000003D
#define ADDR_GPIO_CONFIG_REG2		(u64)0x000000002000003F

#ifdef FTM3_CHIP
#define CX1_WEIGHT					4
#define CX2_WEIGHT					1
#else
#define CX1_WEIGHT					8
#define CX2_WEIGHT					1
#endif

/**@}*/

#endif
