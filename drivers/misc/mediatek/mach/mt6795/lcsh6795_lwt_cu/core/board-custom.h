#ifndef __ARCH_ARM_MACH_MT6575_CUSTOM_BOARD_H
#define __ARCH_ARM_MACH_MT6575_CUSTOM_BOARD_H

#include <generated/autoconf.h>

/*=======================================================================*/
/* MT6575 SD                                                             */
/*=======================================================================*/
#define CFG_DEV_MSDC0
#define CFG_DEV_MSDC1
//#define CFG_DEV_MSDC2
#define CFG_DEV_MSDC3
//#define CFG_DEV_MSDC4
#if defined(CONFIG_MTK_COMBO) || defined(CONFIG_MTK_COMBO_MODULE)
/*
SDIO slot index number used by connectivity combo chip:
0: invalid (used by memory card)
1: MSDC1
2: MSDC2
*/
#define CONFIG_MTK_WCN_CMB_SDIO_SLOT  (3) /* MSDC0 for temp use */
#else
#undef CONFIG_MTK_WCN_CMB_SDIO_SLOT
#endif

#if 0 /* FIXME. */
/*=======================================================================*/
/* MT6575 UART                                                           */
/*=======================================================================*/
#define CFG_DEV_UART1
#define CFG_DEV_UART2
#define CFG_DEV_UART3
#define CFG_DEV_UART4

#define CFG_UART_PORTS          (4)

/*=======================================================================*/
/* MT6575 I2C                                                            */
/*=======================================================================*/
#define CFG_DEV_I2C
//#define CFG_I2C_HIGH_SPEED_MODE
//#define CFG_I2C_DMA_MODE

/*=======================================================================*/
/* MT6575 ADB                                                            */
/*=======================================================================*/
#define ADB_SERIAL "E1K"

#endif

/*=======================================================================*/
/* MT6575 NAND FLASH                                                     */
/*=======================================================================*/
#if 0
#define RAMDOM_READ 1<<0
#define CACHE_READ  1<<1
/*******************************************************************************
 * NFI & ECC Configuration 
 *******************************************************************************/
typedef struct
{
    u16 id;			//deviceid+menuid
    u8  addr_cycle;
    u8  iowidth;
    u16 totalsize;	
    u16 blocksize;
    u16 pagesize;
    u32 timmingsetting;
    char devciename[14];
    u32 advancedmode;   //
}flashdev_info,*pflashdev_info;

static const flashdev_info g_FlashTable[]={
    //micro
    {0xAA2C,  5,  8,  256,	128,  2048,  0x01113,  "MT29F2G08ABD",	0},
    {0xB12C,  4,  16, 128,	128,  2048,  0x01113,  "MT29F1G16ABC",	0},
    {0xBA2C,  5,  16, 256,	128,  2048,  0x01113,  "MT29F2G16ABD",	0}, 
    {0xAC2C,  5,  8,  512,	128,  2048,  0x01113,  "MT29F4G08ABC",	0},
    {0xBC2C,  5,  16, 512,	128,  2048,  0x44333,  "MT29F4G16ABD",	0},
    //samsung 
    {0xBAEC,  5,  16, 256,	128,  2048,  0x01123,  "K522H1GACE",	0},
    {0xBCEC,  5,  16, 512,	128,  2048,  0x01123,  "K524G2GACB",	0},
    {0xDAEC,  5,  8,  256,	128,  2048,  0x33222,  "K9F2G08U0A",	RAMDOM_READ},
    {0xF1EC,  4,  8,  128,	128,  2048,  0x01123,  "K9F1G08U0A",	RAMDOM_READ},
    {0xAAEC,  5,  8,  256,	128,  2048,  0x01123,  "K9F2G08R0A",	0},
    //hynix
    {0xD3AD,  5,  8,  1024, 256,  2048,  0x44333,  "HY27UT088G2A",	0},
    {0xA1AD,  4,  8,  128,	128,  2048,  0x01123,  "H8BCSOPJOMCP",	0},
    {0xBCAD,  5,  16, 512,	128,  2048,  0x01123,  "H8BCSOUNOMCR",	0},
    {0xBAAD,  5,  16, 256,	128,  2048,  0x01123,  "H8BCSOSNOMCR",	0},
    //toshiba
    {0x9598,  5,  16, 816,	128,  2048,  0x00113,  "TY9C000000CMG", 0},
    {0x9498,  5,  16, 375,	128,  2048,  0x00113,  "TY9C000000CMG", 0},
    {0xC198,  4,  16, 128,	128,  2048,  0x44333,  "TC58NWGOS8C",	0},
    {0xBA98,  5,  16, 256,	128,  2048,  0x02113,  "TC58NYG1S8C",	0},
    //st-micro
    {0xBA20,  5,  16, 256,	128,  2048,  0x01123,  "ND02CGR4B2DI6", 0},

    // elpida
    {0xBC20,  5,  16, 512,  128,  2048,  0x01123,  "04GR4B2DDI6",   0},
    {0x0000,  0,  0,  0,	0,	  0,	 0, 	   "xxxxxxxxxxxxx", 0}
};
#endif
	
	
#define NFI_DEFAULT_ACCESS_TIMING        (0x44333)

//uboot only support 1 cs
#define NFI_CS_NUM                  (2)
#define NFI_DEFAULT_CS				(0)

#define USE_AHB_MODE                	(1)

#define PLATFORM_EVB                (1)

#endif /* __ARCH_ARM_MACH_MT6575_CUSTOM_BOARD_H */

