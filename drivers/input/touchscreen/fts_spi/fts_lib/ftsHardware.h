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

/*DIGITAL CHIP INFO*/
#define DCHIP_ID_0								0x36				/*LSB chip ID for FTM5*/
#define DCHIP_ID_1								0x39				/*MSB chip ID for FTM5*/

#define DCHIP_FW_VER_BYTE						2					/*number of bytes of the fw versions*/

/*CHUNKS*/
#define READ_CHUNK								1024				/*chunk dimension of a single i2c read, max allowed value is 2kB*/
#define WRITE_CHUNK								1024				/*chunk dimension of a single i2c write, max allowed value is 2kB*/
#define MEMORY_CHUNK							1024				/*chunk dimenasion of a single i2c write on mem, max allowed value is 2kB*/

/*PROTOCOL INFO*/
/*#define I2C_INTERFACE		comment if the chip use SPI bus*/
#ifdef I2C_INTERFACE
#define I2C_SAD									0x49				/*slave address of the IC*/
#else
#define SPI4_WIRE													/*comment if the master is SPI3 wires (MOSI and MISO share same line)*/
#define SPI_DELAY_CS							10					/*time in usec to wait before rising the CS*/
#define SPI_CLOCK_FREQ							12000000				/*clock frequency in Hz of the SPI bus*/
#endif

#define IER_ENABLE								0x41				/*value to write in IER_ADDR to enable the interrupts*/
#define IER_DISABLE								0x00				/*value to write in IER_ADDR to disable the interrupts*/

/*FLASH COMMAND*/
/** @defgroup flash_command Flash Commands
 *	All the commands that works with the flash of the IC
 *	@{
 */
#define FLASH_CMD_UNLOCK						0xF7

#define FLASH_CMD_READ_REGISTER                 0xFA
#define FLASH_CMD_WRITE_REGISTER				0xFA

/*FLASH UNLOCK PARAMETER*/
#define FLASH_UNLOCK_CODE0						0x25
#define FLASH_UNLOCK_CODE1						0x20
#define FLASH_UNLOCK_CODE2		0x6B
#define FLASH_UNLOCK_CODE3		0x00

#define FLASH_UVLO_ENABLE_CODE0		0x1B
#define FLASH_UVLO_ENABLE_CODE1		0x66
#define FLASH_AUTOPOWERDOWN_ENABLE_CODE0 0x68
#define FLASH_AUTOPOWERDOWN_ENABLE_CODE1 0x13
/*FLASH ERASE and DMA PARAMETER*/
#define FLASH_ERASE_START						0x80
#define FLASH_ERASE_CODE1                       0xC0
#define FLASH_DMA_CODE1                         0xC0
#define FLASH_ERASE_UNLOCK_CODE0				0xDE
#define FLASH_ERASE_UNLOCK_CODE1				0x03
#define FLASH_ERASE_CODE0                       0x6A
#define FLASH_DMA_CODE0                      	0x71
#define FLASH_DMA_CONFIG                        0x72
#define FLASH_NUM_PAGE							32					/*number of pages in main flash*/

#define FLASH_CX_PAGE_START						28					/*starting page which contain Cx data*/
#define FLASH_CX_PAGE_END						30					/*last page which contain Cx data*/
#define FLASH_PANEL_PAGE_START					26					/*starting page which contain Panel Init data*/
#define FLASH_PANEL_PAGE_END					27					/*last page which contain Panel Init data*/

/** @} */

/*FLASH ADDRESS*/

#define FLASH_ADDR_CODE							0x00000000			/*starting address (words) in the flash of the code in FTI*/
#define FLASH_ADDR_CONFIG						0x00007C00			/*starting address (words) in the flash of the config in FTI*/
#define FLASH_ADDR_CX							0x00007000			/*starting address (words) in the flash of the Init data in FTI*/

/*SIZES FW, CODE, CONFIG, MEMH*/
/** @defgroup fw_file FW file info
 *	All the info related to the fw file
 *	@{
 */

#define FW_HEADER_SIZE							64					/*dimension of the header of the .fts file*/
#define FW_HEADER_SIGNATURE						0xAA55AA55			/*header signature*/
#define FW_FTB_VER								0x00000001			/*.ftb version*/
#define FW_BYTES_ALLIGN							4					/*allignment of the info*/
#define FW_BIN_VER_OFFSET						16					/*offset of the fw version in the .ftb file*/
#define FW_BIN_CONFIG_ID_OFFSET					20					/*offset of the config id in the .ftb file*/
#define FW_CX_VERSION							(16 + 4)				/*offset of CX version in the sec2 of FW file*/

/** @} */

/*FIFO*/
#define FIFO_EVENT_SIZE							8					/*number of bytes of one event*/
#define FIFO_DEPTH								32					/*max number of events that the FIFO can collect before going in overflow in FTM5*/

#ifdef I2C_INTERFACE
#define FIFO_CMD_READALL						0x86				/*command to read all the events in the FIFO*/
#else
#define FIFO_CMD_READALL						0x87				/*command to read all the events in the FIFO*/
#endif
#define FIFO_CMD_READONE						FIFO_CMD_READALL	/*commad to read one event from FIFO*/

/*OP CODES FOR MEMORY (based on protocol)*/
#ifdef I2C_INTERFACE
#define FTS_CMD_HW_REG_R						0xFA				/*command to read an hw register if FTI*/
#define FTS_CMD_HW_REG_W						0xFA				/*command to write an hw register if FTI*/
#define FTS_CMD_FRAMEBUFFER_R					0xA6				/*command to read the framebuffer if FTI*/
#define FTS_CMD_CONFIG_R						0xA8				/*command to read the config memory if FTI*/
#define FTS_CMD_CONFIG_W						0xA8				/*command to write the config memory if FTI*/
#else
#define FTS_CMD_HW_REG_R						0xFB				/*command to read an hw register if FTI*/
#define FTS_CMD_HW_REG_W						0xFA				/*command to write an hw register if FTI*/
#define FTS_CMD_FRAMEBUFFER_R					0xA7				/*command to read the framebuffer if FTI*/
#define FTS_CMD_CONFIG_R						0xA9				/*command to read the config memory if FTI*/
#define FTS_CMD_CONFIG_W						0xA8				/*command to write the config memory if FTI*/
#endif

/*DUMMY BYTES DATA*/

#ifndef I2C_INTERFACE
#define DUMMY_HW_REG							1					/*1 if the first byte read from HW register is dummy*/
#define DUMMY_FRAMEBUFFER						1					/*1 if the first byte read from Frame buffer is dummy*/
#define DUMMY_CONFIG							1					/*1 if the first byte read from Config Memory is dummy*/
#define DUMMY_FIFO								1					/*1 if the first byte read from FIFO is dummy*/
#else
#define DUMMY_HW_REG							0					/*1 if the first byte read from HW register is dummy*/
#define DUMMY_FRAMEBUFFER						0					/*1 if the first byte read from Frame buffer is dummy*/
#define DUMMY_CONFIG							0					/*1 if the first byte read from Config Memory is dummy*/
#define DUMMY_FIFO								0					/*1 if the first byte read from FIFO is dummy*/
#endif

/** @defgroup hw_adr HW Address
 * @ingroup address
 * Important addresses of hardware registers (and sometimes their important values)
 * @{
 */

/*IMPORTANT HW ADDRESSES (u64)*/
#define ADDR_FRAMEBUFFER			((u64)0x0000000000000000)			/*frame buffer address in memory*/
#define ADDR_ERROR_DUMP				((u64)0x000000000000EF80)			/*start address dump error log*/

/*SYSTEM RESET INFO*/
#define ADDR_SYSTEM_RESET			((u64)0x0000000020000024)			/*address of System control register in FTI*/
#define SYSTEM_RESET_VALUE						0x80				/*value to write in SYSTEM_RESET_ADDRESS to perform a system reset in FTM5*/

/*REMAP REGISTER*/
#define ADDR_BOOT_OPTION			((u64)0x0000000020000025)			/*address of Boot option register in SONIA*/

/*INTERRUPT INFO*/
#define ADDR_IER					((u64)0x0000000020000029)			/*address of the Interrupt enable register in FTMI*/

/*Chip ID/Fw Version*/
#define ADDR_DCHIP_ID				((u64)0x0000000020000000)			/*chip id address for FTI*/
#define ADDR_DCHIP_FW_VER			((u64)0x0000000020000004)			/*fw version address for FTI*/

/*INTERFACE REGISTER*/
#define ADDR_ICR					((u64)0x000000002000002D)			/*address of Device control register to set the comunication protocol (SPI/I2C)*/
#define SPI4_MASK					0x02							/*bit to set spi4*/

/*CRC ADDR*/
#define ADDR_CRC					((u64)0x0000000020000078)			/*address of the CRC control register in FTI*/
#define CRC_MASK								0x03				/*bitmask which reveal if there is a CRC error in the flash*/

#define ADDR_CONFIG_OFFSET			((u64)0x0000000000000000)			/*config address in memory if FTI*/

#define ADDR_GPIO_INPUT				((u64)0x0000000020000030)			/*address of GPIO input register*/
#define ADDR_GPIO_DIRECTION			((u64)0x0000000020000032)			/*address of GPIO direction register*/
#define ADDR_GPIO_PULLUP			((u64)0x0000000020000034)			/*address of GPIO pullup register*/
#define ADDR_GPIO_CONFIG_REG0		((u64)0x000000002000003D)			/*address of GPIO config register*/
#define ADDR_GPIO_CONFIG_REG2		((u64)0x000000002000003F)			/*address of GPIO config register*/


/**@}*/

#endif
