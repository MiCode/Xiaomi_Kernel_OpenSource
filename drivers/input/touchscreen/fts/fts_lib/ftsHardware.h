/*

**************************************************************************
**                        STMicroelectronics		                **
**************************************************************************
**                        marco.cali@st.com				**
**************************************************************************
*                                                                        *
*			HW related data					 *
*                                                                        *
**************************************************************************
**************************************************************************

*/




#define DUMMY_HW_REG				1
#define DUMMY_FRAMEBUFFER			1
#define DUMMY_MEMORY				1

#ifdef FTM3_CHIP
#define DCHIP_ID_0					0x39
#define DCHIP_ID_1					0x6C
#else
#define DCHIP_ID_0					0x36
#define DCHIP_ID_1					0x70
#endif

#ifdef FTM3_CHIP
#define DCHIP_ID_ADDR				0x0007
#define DCHIP_FW_VER_ADDR			0x000A
#else
#define DCHIP_ID_ADDR				0x0004
#define DCHIP_FW_VER_ADDR			0x000B
#endif

#define DCHIP_FW_VER_BYTE			2

#define READ_CHUNK					(2 * 1024)
#define WRITE_CHUNK					(2 * 1024)
#define MEMORY_CHUNK				(2 * 1024)

#ifdef FTM3_CHIP
#define I2C_SAD						0x49
#else
#define I2C_SAD						0x48
#endif

#define I2C_INTERFACE
#define ICR_ADDR					0x0024
#define ICR_SPI_VALUE				0x02

#ifdef FTM3_CHIP
#define SYSTEM_RESET_ADDRESS		0x0023
#define SYSTEM_RESET_VALUE			0x01
#else
#define SYSTEM_RESET_ADDRESS		0x0028
#define SYSTEM_RESET_VALUE			0x80
#endif



#ifdef FTM3_CHIP
#define IER_ADDR					0x001C
#else
#define IER_ADDR					0x002C
#endif

#define IER_ENABLE					0x41
#define IER_DISABLE					0x00


#define FLASH_CMD_UNLOCK			0xF7


#ifdef FTM3_CHIP
#define FLASH_CMD_WRITE_LOWER_64                0xF0
#define FLASH_CMD_WRITE_UPPER_64                0xF1
#define FLASH_CMD_BURN				0xF2
#define FLASH_CMD_ERASE				0xF3
#define FLASH_CMD_READSTATUS                    0xF4
#else
#define FLASH_CMD_WRITE_64K                     0xF8
#define FLASH_CMD_READ_REGISTER                 0xF9
#define FLASH_CMD_WRITE_REGISTER		0xFA
#endif

#define FLASH_UNLOCK_CODE0			0x74
#define FLASH_UNLOCK_CODE1			0x45

#ifndef FTM3_CHIP
#define FLASH_ERASE_UNLOCK_CODE0			0x72
#define FLASH_ERASE_UNLOCK_CODE1			0x03
#define FLASH_ERASE_UNLOCK_CODE2			0x02
#define FLASH_ERASE_CODE0                           0x02
#define FLASH_ERASE_CODE1                           0xC0
#define FLASH_DMA_CODE0                           0x05
#define FLASH_DMA_CODE1                           0xC0
#define FLASH_DMA_CONFIG                        0x06
#define FLASH_ERASE_START			0x80
#define FLASH_NUM_PAGE				64
#define FLASH_CX_PAGE_START			61
#define FLASH_CX_PAGE_END			62
#endif


#ifdef FTM3_CHIP
#define FLASH_ADDR_SWITCH_CMD                   0x00010000
#define FLASH_ADDR_CODE				0x00000000
#define FLASH_ADDR_CONFIG			0x0001E800
#define FLASH_ADDR_CX				0x0001F000
#else
#define ADDR_WARM_BOOT                          0x001E
#define WARM_BOOT_VALUE                          0x38
#define FLASH_ADDR_CODE				0x00000000
#define FLASH_ADDR_CONFIG			0x0000FC00
#endif


#ifdef FTM3_CHIP
#define ADDR_CRC_BYTE0				0x00
#define ADDR_CRC_BYTE1				0x86
#define CRC_MASK				0x02
#else
#define ADDR_CRC_BYTE0				0x00
#define ADDR_CRC_BYTE1				0x74
#define CRC_MASK				0x03
#endif

#ifdef FTM3_CHIP
#define FW_HEADER_SIZE					32
#define FW_SIZE						((int)(128 * 1024))
#define FW_CODE_SIZE				((int)(122 * 1024))
#define FW_CONFIG_SIZE				((int)(2 * 1024))
#define FW_CX_SIZE					((int)(FW_SIZE - FW_CODE_SIZE - FW_CONFIG_SIZE))
#define FW_VER_MEMH_BYTE1			193
#define FW_VER_MEMH_BYTE0			192
#define FW_OFF_CONFID_MEMH_BYTE1	2
#define FW_OFF_CONFID_MEMH_BYTE0	1
#define FW_BIN_VER_OFFSET					4
#define FW_BIN_CONFIG_VER_OFFSET				(FW_HEADER_SIZE + FW_CODE_SIZE + 1)
#else
#define FW_HEADER_SIZE					64
#define FW_HEADER_SIGNATURE				0xAA55AA55
#define FW_FTB_VER					0x00000001
#define FW_BYTES_ALLIGN					4
#define FW_BIN_VER_OFFSET					16
#define FW_BIN_CONFIG_VER_OFFSET				20
#endif

#define FIFO_EVENT_SIZE				8
#ifdef FTM3_CHIP
#define FIFO_DEPTH					32
#else
#define FIFO_DEPTH					64
#endif

#define FIFO_CMD_READONE			0x85
#define FIFO_CMD_READALL			0x86
#define FIFO_CMD_LAST				0x87
#define FIFO_CMD_FLUSH				0xA1

#ifdef FTM3_CHIP
#define CX1_WEIGHT					4
#define CX2_WEIGHT					1
#else
#define CX1_WEIGHT					15
#define CX2_WEIGHT					2
#endif

#define FTS_CMD_HW_REG_R			0xB6
#define FTS_CMD_HW_REG_W			0xB6
#define FTS_CMD_FRAMEBUFFER_R		0xD0
#define FTS_CMD_FRAMEBUFFER_W		0xD0
