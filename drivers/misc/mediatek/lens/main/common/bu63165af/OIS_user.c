/* /////////////////////////////////////////////////////////////////////////// */
/* File Name    : OIS_user.c */
/* Function             : User defined function. */
/* These functions depend on user's circumstance. */
/*  */
/* Rule         : Use TAB 4 */
/*  */
/* Copyright(c) Rohm Co.,Ltd. All rights reserved */
/*  */
/***** ROHM Confidential ***************************************************/
#ifndef OIS_USER_C
#define OIS_USER_C
#endif

#include "OIS_head.h"


/* Following Variables that depend on user's environment                        RHM_HT 2013.03.13       add */
OIS_UWORD FOCUS_VAL = 0x0122;

/* <== RHM_HT 2013/07/10        Added new user definition variables */



/* ///////////////////////////////////////////////////////// */
/* VCOSET function */
/* --------------------------------------------------------- */
/* <Function> */
/* To use external clock at CLK/PS, it need to set PLL. */
/* After enabling PLL, more than 30ms wait time is required to change clock source. */
/* So the below sequence has to be used: */
/* Input CLK/PS --> Call VCOSET0 --> Download Program/Coed --> Call VCOSET1 */
/*  */
/* <Input> */
/* none */
/*  */
/* <Output> */
/* none */
/*  */
/* ========================================================= */
void VCOSET0(void)
{
	OIS_UWORD CLK_PS = 23880;	/* Input Frequency [kHz] of CLK/PS terminal (Depend on your system) */
	OIS_UWORD FVCO_1 = 36000;	/* Target Frequency [kHz] */
	/* 27000 for 63163 */
	/* 36000 for 63165 */
	OIS_UWORD FREF = 25;	/* Reference Clock Frequency [kHz] */

	OIS_UWORD DIV_N = CLK_PS / FREF - 1;	/* calc DIV_N */
	OIS_UWORD DIV_M = FVCO_1 / FREF - 1;	/* calc DIV_M */

	I2C_OIS_per_write(0x62, DIV_N);	/* Divider for internal reference clock */
	I2C_OIS_per_write(0x63, DIV_M);	/* Divider for internal PLL clock */
	I2C_OIS_per_write(0x64, 0x4060);	/* Loop Filter */

	I2C_OIS_per_write(0x60, 0x3011);	/* PLL */
	I2C_OIS_per_write(0x65, 0x0080);	/*  */
	I2C_OIS_per_write(0x61, 0x8002);	/* VCOON */
	I2C_OIS_per_write(0x61, 0x8003);	/* Circuit ON */
	I2C_OIS_per_write(0x61, 0x8809);	/* PLL ON */
}

void VCOSET1(void)
{
	I2C_OIS_per_write(0x05, 0x000C);	/* Prepare for PLL clock as master clock */
	I2C_OIS_per_write(0x05, 0x000D);	/* Change to PLL clock */
}

/* ///////////////////////////////////////////////////////// */
/* Write Data to Slave device via I2C master device */
/* --------------------------------------------------------- */
/* <Function> */
/* I2C master send these data to the I2C slave device. */
/* This function relate to your own circuit. */
/*  */
/* <Input> */
/* OIS_UBYTE       slvadr  I2C slave adr */
/* OIS_UBYTE       size    Transfer Size */
/* OIS_UBYTE       *dat    data matrix */
/*  */
/* <Output> */
/* none */
/*  */
/* <Description> */
/* [S][SlaveAdr][W]+[dat[0]]+...+[dat[size-1]][P] */
/*  */
/* ========================================================= */
void WR_I2C(OIS_UBYTE slvadr, OIS_UBYTE size, OIS_UBYTE *dat)
{
	s4AF_WriteReg_BU63165AF(slvadr << 1, dat, size);
}

/* ********************************************************* */
/* Read Data from Slave device via I2C master device */
/* --------------------------------------------------------- */
/* <Function> */
/* I2C master read data from the I2C slave device. */
/* This function relate to your own circuit. */
/*  */
/* <Input> */
/* OIS_UBYTE       slvadr  I2C slave adr */
/* OIS_UBYTE       size    Transfer Size */
/* OIS_UBYTE       *dat    data matrix */
/*  */
/* <Output> */
/* OIS_UWORD       16bit data read from I2C Slave device */
/*  */
/* <Description> */
/* if size == 1 */
/* [S][SlaveAdr][W]+[dat[0]]+         [RS][SlaveAdr][R]+[RD_DAT0]+[RD_DAT1][P] */
/* if size == 2 */
/* [S][SlaveAdr][W]+[dat[0]]+[dat[1]]+[RS][SlaveAdr][R]+[RD_DAT0]+[RD_DAT1][P] */
/*  */
/* ********************************************************* */
OIS_UWORD RD_I2C(OIS_UBYTE slvadr, OIS_UBYTE size, OIS_UBYTE *dat)
{
	OIS_UWORD read_data = 0;
	OIS_UWORD read_data_h = 0;

	if (size == 1) {
		dat[1] = 0;
		s4AF_ReadReg_BU63165AF(slvadr << 1, dat, 2, (u8 *)&read_data, 2);
	} else if (size == 2) {
		s4AF_ReadReg_BU63165AF(slvadr << 1, dat, 2, (u8 *)&read_data, 2);
	}

	read_data_h = read_data >> 8;
	read_data = read_data << 8;
	read_data = read_data | read_data_h;

	return read_data;
}


/* ********************************************************* */
/* Write Factory Adjusted data to the non-volatile memory */
/* --------------------------------------------------------- */
/* <Function> */
/* Factory adjusted data are sotred somewhere */
/* non-volatile memory. */
/*  */
/* <Input> */
/* _FACT_ADJ       Factory Adjusted data */
/*  */
/* <Output> */
/* none */
/*  */
/* <Description> */
/* You have to port your own system. */
/*  */
/* ********************************************************* */
void store_FADJ_MEM_to_non_volatile_memory(_FACT_ADJ param)
{

	    /*      Write to the non-vollatile memory such as EEPROM or internal of the CMOS sensor... */
}

/* ********************************************************* */
/* Read Factory Adjusted data from the non-volatile memory */
/* --------------------------------------------------------- */
/* <Function> */
/* Factory adjusted data are sotred somewhere */
/* non-volatile memory.  I2C master has to read these */
/* data and store the data to the OIS controller. */
/*  */
/* <Input> */
/* none */
/*  */
/* <Output> */
/* _FACT_ADJ       Factory Adjusted data */
/*  */
/* <Description> */
/* You have to port your own system. */
/*  */
/* ********************************************************* */
_FACT_ADJ get_FADJ_MEM_from_non_volatile_memory(void)
{
	u16 ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x0763, &ReadData);
	FADJ_MEM.gl_CURDAT = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x0765, &ReadData);
	FADJ_MEM.gl_HALOFS_X = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x0767, &ReadData);
	FADJ_MEM.gl_HALOFS_Y = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x0769, &ReadData);
	FADJ_MEM.gl_HX_OFS = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x076B, &ReadData);
	FADJ_MEM.gl_HY_OFS = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x076D, &ReadData);
	FADJ_MEM.gl_PSTXOF = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x076F, &ReadData);
	FADJ_MEM.gl_PSTYOF = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x0771, &ReadData);
	FADJ_MEM.gl_GX_OFS = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x0773, &ReadData);
	FADJ_MEM.gl_GY_OFS = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x0775, &ReadData);
	FADJ_MEM.gl_KgxHG = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x0777, &ReadData);
	FADJ_MEM.gl_KgyHG = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x0779, &ReadData);
	FADJ_MEM.gl_KGXG = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x077B, &ReadData);
	FADJ_MEM.gl_KGYG = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x077D, &ReadData);
	FADJ_MEM.gl_SFTHAL_X = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x077F, &ReadData);
	FADJ_MEM.gl_SFTHAL_Y = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x0781, &ReadData);
	FADJ_MEM.gl_TMP_X_ = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x0783, &ReadData);
	FADJ_MEM.gl_TMP_Y_ = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x0785, &ReadData);
	FADJ_MEM.gl_KgxH0 = (OIS_UWORD) ReadData;

	s4EEPROM_ReadReg_BU63165AF(0x0787, &ReadData);
	FADJ_MEM.gl_KgyH0 = (OIS_UWORD) ReadData;

	return FADJ_MEM;	/* Note: This return data is for DEBUG. */
}

/* ********************************************************* */
/* Wait */
/* --------------------------------------------------------- */
/* <Function> */
/*  */
/* <Input> */
/* OIS_ULONG       time    on the micro second time scale */
/*  */
/* <Output> */
/* none */
/*  */
/* <Description> */
/*  */
/* ********************************************************* */
void Wait_usec(OIS_ULONG time)
{
	/* Please write your source code here. */
}

#ifdef	DEBUG_FADJ
/* ********************************************************* */
/* Printf for DEBUG */
/* --------------------------------------------------------- */
/* <Function> */
/*  */
/* <Input> */
/* const char *format, ... */
/* Same as printf */
/*  */
/* <Output> */
/* none */
/*  */
/* <Description> */
/*  */
/* ********************************************************* */
int debug_print(const char *format, ...)
{
	return 0;
}

#endif
