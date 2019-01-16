#ifndef MT6575_NAND_DEVICE
#define MT6575_NAND_DEVICE

#include "mt6575_nand.h"

static const flashdev_info g_FlashTable[]={
	//micro
	{0xAA2C,  5,  8,  256,  128,  2048,  0x01113,  "MT29F2G08ABD",  0},
	{0xB12C,  4,  16, 128,  128,  2048,  0x01113,  "MT29F1G16ABC",  0},
	{0xBA2C,  5,  16, 256,  128,  2048,  0x01113,  "MT29F2G16ABD",  0},	
	{0xAC2C,  5,  8,  512,	128,  2048,  0x01113,  "MT29F4G08ABC",	0},
    {0xA12C,  4,  8,  128,  128,  2048,  0x01113,  "MT29F1G08ABB",	0},
    {0xBC2C,  5,  16, 512,  128,  2048,  0x21044333,  "MT29C4G96MAZAPCJA_5IT",	0},
	//samsung 
	{0xBAEC,  5,  16, 256,  128,  2048,  0x01123,  "K522H1GACE",    0},
	//{0xBCEC,  5,  16, 512,  128,  2048,  0x01123,  "K524G2GACB",    0}, //RAMDOM_READ},
	{0xBCEC,  5,  16, 512,  256,  4096,  0x21044333,  "KA100O015E-BJTT",    0}, //RAMDOM_READ},
	{0xDAEC,  5,  8,  256,  128,  2048,  0x33222,  "K9F2G08U0A",    RAMDOM_READ},
	{0xF1EC,  4,  8,  128,  128,  2048,  0x01123,  "K9F1G08U0A",    RAMDOM_READ},
	{0xAAEC,  5,  8,  256,  128,  2048,  0x01123,  "K9F2G08R0A",    0},
    //hynix
	{0xD3AD,  5,  8,  1024, 256,  2048,  0x44333,  "HY27UT088G2A",  0},
	{0xA1AD,  4,  8,  128,  128,  2048,  0x01123,  "H8BCSOPJOMCP",  0},
	//{0xBCAD,  5,  16, 512,  128,  2048,  0x01123,  "H8BCSOUNOMCR",  0},
	{0xBCAD,  5,  16, 512,  128,  2048,  0x10801011, /*0x21044333,*/  "H9DA4GH4JJAMCR_4EM",  0},
	{0xBAAD,  5,  16, 256,  128,  2048,  0x01123,  "H8BCSOSNOMCR",  0},
	//toshiba
	{0x9598,  5,  16, 816,  128,  2048,  0x00113,  "TY9C000000CMG", 0},
	{0x9498,  5,  16, 375,  128,  2048,  0x00113,  "TY9C000000CMG", 0},
    {0xBC98,  5,  16, 512, 128,  2048,  0x02113,  "TY58NYG2S8E",	0},
	{0xC198,  4,  16, 128,  128,  2048,  0x44333,  "TC58NWGOS8C",   0},
	{0xBA98,  5,  16, 256,  128,  2048,  0x02113,  "TC58NYG1S8C",   0},
	//st-micro
	{0xBA20,  5,  16, 256,  128,  2048,  0x01123,  "ND02CGR4B2DI6", 0},

	// elpida
	{0xBC20,  5,  16, 512,  128,  2048,  0x01123,  "04GR4B2DDI6",   0},
	{0x0000,  0,  0,  0,	0,	  0,	 0, 	   "xxxxxxxxxxxxx", 0}
};

#endif
