/* /////////////////////////////////////////////////////////////////////////// */
/* File Name    : OIS_main.c */
/* Function             : Main control function runnning on ISP. */
/* ( But Just for example ) */
/* Rule         : Use TAB 4 */
/*  */
/* Copyright(c) Rohm Co.,Ltd. All rights reserved */
/*  */
/***** ROHM Confidential ***************************************************/
#ifndef OIS_MAIN_C
#define OIS_MAIN_C
#endif

/* #define OIS_DEBUG */
#ifdef OIS_DEBUG
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#endif

/* #include <stdio.h> */
#include "OIS_head.h"

#ifdef OIS_DEBUG
#define OIS_DRVNAME "BU63165AF_OIS"
#define LOG_INF(format, args...) pr_info(OIS_DRVNAME " [%s] " format, __func__, ##args)
#endif

/* GLOBAL variable ( Upper Level Host Set this Global variables ) */
/* ////////////////////////////////////////////////////////////////////////////// */
OIS_UWORD BOOT_MODE = _FACTORY_;

#define	AF_REQ			0x8000
#define	SCENE_REQ_ON	0x4000
#define	SCENE_REQ_OFF	0x2000
#define	POWERDOWN		0x1000
#define	INITIAL_VAL		0x0000

OIS_UWORD OIS_SCENE = _SCENE_D_A_Y_1;
OIS_UWORD OIS_REQUEST = INITIAL_VAL;	/* OIS control register. */

/* ==> RHM_HT 2013.03.04        Change type (OIS_UWORD -> double) */
double OIS_PIXEL[2];		/* Just Only use for factory adjustment. */
/* <== RHM_HT 2013.03.04 */
ADJ_STS OIS_MAIN_STS = ADJ_ERR;

static _FACT_ADJ fadj;

void setOISMode(int Disable)
{
	if (Disable == 1)
		func_SET_SCENE_PARAM_for_NewGYRO_Fil(_SCENE_SPORT_3, 0, 0, 0, &fadj);
	else
		func_SET_SCENE_PARAM_for_NewGYRO_Fil(_SCENE_SPORT_3, 1, 0, 0, &fadj);
}

int setVCMPos(unsigned short DAC_Val)
{
	I2C_OIS_F0123_wr_(0x90, 0x00, DAC_Val);	/* AF Control */

	return 0;
}

void OIS_Standby(void)
{
	I2C_OIS_F0123_wr_(0x90, 0x00, 0x0200);
	I2C_OIS_mem_write(0x7F, 0x0080);

	I2C_OIS_per_write(0x72, 0x0000);
	I2C_OIS_per_write(0x73, 0x0000);

	I2C_OIS_per_write(0x18, 0x000F);
	I2C_OIS_per_write(0x1B, 0x0B02);
	I2C_OIS_per_write(0x1C, 0x0B02);

	I2C_OIS_per_write(0x3D, 0x0000);
	I2C_OIS_per_write(0x22, 0x0300);
	I2C_OIS_per_write(0x59, 0x0000);
}


/* MAIN OIS */
/* ////////////////////////////////////////////////////////////////////////////// */
void Main_OIS(void)
{
	/* ------------------------------------------------------ */
	/* Get Factory adjusted data */
	/* ------------------------------------------------------ */
	fadj = get_FADJ_MEM_from_non_volatile_memory();	/* Initialize by Factory adjusted value. */

	#ifdef OIS_DEBUG
	LOG_INF("gl_CURDAT = 0x%04X\n", fadj.gl_CURDAT);
	LOG_INF("gl_HALOFS_X = 0x%04X\n", fadj.gl_HALOFS_X);
	LOG_INF("gl_HALOFS_Y = 0x%04X\n", fadj.gl_HALOFS_Y);
	LOG_INF("gl_PSTXOF = 0x%04X\n", fadj.gl_PSTXOF);
	LOG_INF("gl_PSTYOF = 0x%04X\n", fadj.gl_PSTYOF);
	LOG_INF("gl_HX_OFS = 0x%04X\n", fadj.gl_HX_OFS);
	LOG_INF("gl_HY_OFS = 0x%04X\n", fadj.gl_HY_OFS);
	LOG_INF("gl_GX_OFS = 0x%04X\n", fadj.gl_GX_OFS);
	LOG_INF("gl_GY_OFS = 0x%04X\n", fadj.gl_GY_OFS);
	LOG_INF("gl_KgxHG  = 0x%04X\n", fadj.gl_KgxHG);
	LOG_INF("gl_KgyHG  = 0x%04X\n", fadj.gl_KgyHG);
	LOG_INF("gl_KGXG   = 0x%04X\n", fadj.gl_KGXG);
	LOG_INF("gl_KGYG   = 0x%04X\n", fadj.gl_KGYG);
	LOG_INF("gl_SFTHAL_X = 0x%04X\n", fadj.gl_SFTHAL_X);
	LOG_INF("gl_SFTHAL_Y = 0x%04X\n", fadj.gl_SFTHAL_Y);
	LOG_INF("gl_TMP_X_ = 0x%04X\n", fadj.gl_TMP_X_);
	LOG_INF("gl_TMP_Y_ = 0x%04X\n", fadj.gl_TMP_Y_);
	LOG_INF("gl_KgxH0 = 0x%04X\n", fadj.gl_KgxH0);
	LOG_INF("gl_KgyH0 = 0x%04X\n", fadj.gl_KgyH0);
	#endif

	/* ------------------------------------------------------ */
	/* Enable Source Power and Input external clock to CLK/PS pin. */
	/* ------------------------------------------------------ */
	/* Please write your source code here. */

	/* ------------------------------------------------------ */
	/* PLL setting to use external CLK */
	/* ------------------------------------------------------ */
	VCOSET0();

	/* ------------------------------------------------------ */
	/* Download Program and Coefficient */
	/* ------------------------------------------------------ */
	OIS_MAIN_STS = func_PROGRAM_DOWNLOAD();	/* Program Download */
	if (OIS_MAIN_STS <= ADJ_ERR)
		return;
	func_COEF_DOWNLOAD(0);	/* Download Coefficient */

	/* ------------------------------------------------------ */
	/* Change Clock to external pin CLK_PS */
	/* ------------------------------------------------------ */
	VCOSET1();

	/* ------------------------------------------------------ */
	/* Issue DSP start command. */
	/* ------------------------------------------------------ */
	I2C_OIS_spcl_cmnd(1, _cmd_8C_EI);	/* DSP calculation START */

	/* ------------------------------------------------------ */
	/* Set calibration data */
	/* ------------------------------------------------------ */
	SET_FADJ_PARAM(&fadj);

	/* ------------------------------------------------------ */
	/* Set scene parameter for OIS */
	/* ------------------------------------------------------ */
	func_SET_SCENE_PARAM_for_NewGYRO_Fil(_SCENE_SPORT_3, 1, 0, 0, &fadj);
}
