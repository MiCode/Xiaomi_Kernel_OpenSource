/* Header file for:
 * Cypress TrueTouch(TM) Standard Product touchscreen drivers.
 * include/linux/cyttsp.h
 *
 * Copyright (C) 2009, 2010 Cypress Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Cypress reserves the right to make changes without further notice
 * to the materials described herein. Cypress does not assume any
 * liability arising out of the application described herein.
 *
 * Contact Cypress Semiconductor at www.cypress.com
 *
 */


#ifndef __CYTTSP_H__
#define __CYTTSP_H__

#include <linux/input.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#include <asm/mach-types.h>

#define CYPRESS_TTSP_NAME	"cyttsp"
#define CY_I2C_NAME		"cyttsp-i2c"
#define CY_SPI_NAME		"cyttsp-spi"

#ifdef CY_DECLARE_GLOBALS
	uint32_t cyttsp_tsdebug;
	module_param_named(tsdebug, cyttsp_tsdebug, uint, 0664);
	uint32_t cyttsp_tsxdebug;
	module_param_named(tsxdebug, cyttsp_tsxdebug, uint, 0664);

	uint32_t cyttsp_disable_touch;
	module_param_named(disable_touch, cyttsp_disable_touch, uint, 0664);
#else
	extern uint32_t cyttsp_tsdebug;
	extern uint32_t cyttsp_tsxdebug;
	extern uint32_t cyttsp_disable_touch;
#endif



/******************************************************************************
 * Global Control, Used to control the behavior of the driver
 */

/* defines for Gen2 (Txx2xx); Gen3 (Txx3xx)
 * use these defines to set cyttsp_platform_data.gen in board config file
 */
#define CY_GEN2		2
#define CY_GEN3		3

/* define for using I2C driver
 */
#define CY_USE_I2C_DRIVER

/* defines for using SPI driver */
/*
#define CY_USE_SPI_DRIVER
 */
#define CY_SPI_DFLT_SPEED_HZ		1000000
#define CY_SPI_MAX_SPEED_HZ		4000000
#define CY_SPI_SPEED_HZ			CY_SPI_DFLT_SPEED_HZ
#define CY_SPI_BITS_PER_WORD		8
#define CY_SPI_DAV			139	/* set correct gpio id */
#define CY_SPI_BUFSIZE			512

/* Voltage and Current ratings */
#define CY_TMA300_VTG_MAX_UV		5500000
#define CY_TMA300_VTG_MIN_UV		1710000
#define CY_TMA300_CURR_24HZ_UA		17500
#define CY_TMA300_SLEEP_CURR_UA		10
#define CY_I2C_VTG_MAX_UV		1800000
#define CY_I2C_VTG_MIN_UV		1800000
#define CY_I2C_CURR_UA			9630
#define CY_I2C_SLEEP_CURR_UA		10


/* define for inclusion of TTSP App Update Load File
 * use this define if update to the TTSP Device is desired
 */
/*
#define CY_INCLUDE_LOAD_FILE
*/

/* define if force new load file for bootloader load */
/*
#define CY_FORCE_FW_UPDATE
*/

/* undef for production use */
/*
#define CY_USE_DEBUG
*/

/* undef for irq use; use this define in the board configuration file */
/*
#define CY_USE_TIMER
 */

/* undef to allow use of extra debug capability */
/*
#define CY_ALLOW_EXTRA_DEBUG
*/

/* undef to remove additional debug prints */
/*
#define CY_USE_EXTRA_DEBUG
*/

/* undef to remove additional debug prints */
/*
#define CY_USE_EXTRA_DEBUG1
 */

/* undef to use operational touch timer jiffies; else use test jiffies */
/*
 */
#define CY_USE_TIMER_DEBUG

/* define to use canned test data */
/*
#define CY_USE_TEST_DATA
 */

/* define if gesture signaling is used
 * and which gesture groups to use
 */
/*
#define CY_USE_GEST
#define CY_USE_GEST_GRP1
#define CY_USE_GEST_GRP2
#define CY_USE_GEST_GRP3
#define CY_USE_GEST_GRP4
 */
/* Active distance in pixels for a gesture to be reported
 * if set to 0, then all gesture movements are reported
 */
#define CY_ACT_DIST_DFLT	8
#define CY_ACT_DIST			CY_ACT_DIST_DFLT

/* define if MT signals are desired */
/*
*/
#define CY_USE_MT_SIGNALS

/* define if MT tracking id signals are used */
/*
#define CY_USE_MT_TRACK_ID
 */

/* define if ST signals are required */
/*
*/
#define CY_USE_ST_SIGNALS

/* define to send handshake to device */
/*
*/
#define CY_USE_HNDSHK

/* define if log all raw motion signals to a sysfs file */
/*
#define CY_LOG_TO_FILE
*/


/* End of the Global Control section
 ******************************************************************************
 */
#define CY_DIFF(m, n)		((m) != (n))

#ifdef CY_LOG_TO_FILE
	#define cyttsp_openlog()	/* use sysfs */
#else
	#define cyttsp_openlog()
#endif /* CY_LOG_TO_FILE */

/* see kernel.h for pr_xxx def'ns */
#define cyttsp_info(f, a...)		pr_info("%s:" f,  __func__ , ## a)
#define cyttsp_error(f, a...)		pr_err("%s:" f,  __func__ , ## a)
#define cyttsp_alert(f, a...)		pr_alert("%s:" f,  __func__ , ## a)

#ifdef CY_USE_DEBUG
	#define cyttsp_debug(f, a...)	pr_alert("%s:" f,  __func__ , ## a)
#else
	#define cyttsp_debug(f, a...)	{if (cyttsp_tsdebug) \
					pr_alert("%s:" f,  __func__ , ## a); }
#endif /* CY_USE_DEBUG */

#ifdef CY_ALLOW_EXTRA_DEBUG
#ifdef CY_USE_EXTRA_DEBUG
	#define cyttsp_xdebug(f, a...)	pr_alert("%s:" f,  __func__ , ## a)
#else
	#define cyttsp_xdebug(f, a...)	{if (cyttsp_tsxdebug) \
					pr_alert("%s:" f,  __func__ , ## a); }
#endif /* CY_USE_EXTRA_DEBUG */

#ifdef CY_USE_EXTRA_DEBUG1
	#define cyttsp_xdebug1(f, a...)	pr_alert("%s:" f,  __func__ , ## a)
#else
	#define cyttsp_xdebug1(f, a...)
#endif /* CY_USE_EXTRA_DEBUG1 */
#else
	#define cyttsp_xdebug(f, a...)
	#define cyttsp_xdebug1(f, a...)
#endif /* CY_ALLOW_EXTRA_DEBUG */

#ifdef CY_USE_TIMER_DEBUG
	#define	TOUCHSCREEN_TIMEOUT	(msecs_to_jiffies(1000))
#else
	#define	TOUCHSCREEN_TIMEOUT	(msecs_to_jiffies(28))
#endif

/* reduce extra signals in MT only build
 * be careful not to lose backward compatibility for pre-MT apps
 */
#ifdef CY_USE_ST_SIGNALS
	#define CY_USE_ST	1
#else
	#define CY_USE_ST	0
#endif /* CY_USE_ST_SIGNALS */

/* rely on kernel input.h to define Multi-Touch capability */
/* if input.h defines the Multi-Touch signals, then use MT */
#if defined(ABS_MT_TOUCH_MAJOR) && defined(CY_USE_MT_SIGNALS)
	#define CY_USE_MT	1
	#define CY_MT_SYNC(input)	input_mt_sync(input)
#else
	#define CY_USE_MT	0
	#define CY_MT_SYNC(input)
	/* the following includes are provided to ensure a compile;
	 * the code that compiles with these defines will not be executed if
	 * the CY_USE_MT is properly used in the platform structure init
	 */
	#ifndef ABS_MT_TOUCH_MAJOR
	#define ABS_MT_TOUCH_MAJOR	0x30	/* touching ellipse */
	#define ABS_MT_TOUCH_MINOR	0x31	/* (omit if circular) */
	#define ABS_MT_WIDTH_MAJOR	0x32	/* approaching ellipse */
	#define ABS_MT_WIDTH_MINOR	0x33	/* (omit if circular) */
	#define ABS_MT_ORIENTATION	0x34	/* Ellipse orientation */
	#define ABS_MT_POSITION_X	0x35	/* Center X ellipse position */
	#define ABS_MT_POSITION_Y	0x36	/* Center Y ellipse position */
	#define ABS_MT_TOOL_TYPE	0x37	/* Type of touching device */
	#define ABS_MT_BLOB_ID		0x38	/* Group set of pkts as blob */
	#endif /* ABS_MT_TOUCH_MAJOR */
#endif /* ABS_MT_TOUCH_MAJOR and CY_USE_MT_SIGNALS */
#if defined(ABS_MT_TRACKING_ID)  && defined(CY_USE_MT_TRACK_ID)
	#define CY_USE_TRACKING_ID	1
#else
	#define CY_USE_TRACKING_ID	0
/* define only if not defined already by system;
 * value based on linux kernel 2.6.30.10
 */
#ifndef ABS_MT_TRACKING_ID
	#define ABS_MT_TRACKING_ID	(ABS_MT_BLOB_ID+1)
#endif
#endif /* ABS_MT_TRACKING_ID */

#define CY_USE_DEEP_SLEEP_SEL		0x80
#define CY_USE_LOW_POWER_SEL		0x01

#ifdef CY_USE_TEST_DATA
	#define cyttsp_testdat(ray1, ray2, sizeofray) \
		{ \
			int i; \
			u8 *up1 = (u8 *)ray1; \
			u8 *up2 = (u8 *)ray2; \
			for (i = 0; i < sizeofray; i++) { \
				up1[i] = up2[i]; \
			} \
		}
#else
	#define cyttsp_testdat(xy, test_xy, sizeofray)
#endif /* CY_USE_TEST_DATA */

/* helper macros */
#define GET_NUM_TOUCHES(x)		((x) & 0x0F)
#define GET_TOUCH1_ID(x)		(((x) & 0xF0) >> 4)
#define GET_TOUCH2_ID(x)		((x) & 0x0F)
#define GET_TOUCH3_ID(x)		(((x) & 0xF0) >> 4)
#define GET_TOUCH4_ID(x)		((x) & 0x0F)
#define IS_LARGE_AREA(x)		(((x) & 0x10) >> 4)
#define FLIP_DATA_FLAG			0x01
#define REVERSE_X_FLAG			0x02
#define REVERSE_Y_FLAG			0x04
#define FLIP_DATA(flags)		((flags) & FLIP_DATA_FLAG)
#define REVERSE_X(flags)		((flags) & REVERSE_X_FLAG)
#define REVERSE_Y(flags)		((flags) & REVERSE_Y_FLAG)
#define FLIP_XY(x, y)			{ \
						u16 tmp; \
						tmp = (x); \
						(x) = (y); \
						(y) = tmp; \
					}
#define INVERT_X(x, xmax)		((xmax) - (x))
#define INVERT_Y(y, maxy)		((maxy) - (y))
#define SET_HSTMODE(reg, mode)		((reg) & (mode))
#define GET_HSTMODE(reg)		((reg & 0x70) >> 4)
#define GET_BOOTLOADERMODE(reg)		((reg & 0x10) >> 4)

/* constant definitions */
/* maximum number of concurrent ST track IDs */
#define CY_NUM_ST_TCH_ID		2

/* maximum number of concurrent MT track IDs */
#define CY_NUM_MT_TCH_ID		4

/* maximum number of track IDs */
#define CY_NUM_TRK_ID			16

#define CY_NTCH				0	/* no touch (lift off) */
#define CY_TCH				1	/* active touch (touchdown) */
#define CY_ST_FNGR1_IDX			0
#define CY_ST_FNGR2_IDX			1
#define CY_MT_TCH1_IDX			0
#define CY_MT_TCH2_IDX			1
#define CY_MT_TCH3_IDX			2
#define CY_MT_TCH4_IDX			3
#define CY_XPOS				0
#define CY_YPOS				1
#define CY_IGNR_TCH			(-1)
#define CY_SMALL_TOOL_WIDTH		10
#define CY_LARGE_TOOL_WIDTH		255
#define CY_REG_BASE			0x00
#define CY_REG_GEST_SET			0x1E
#define CY_REG_ACT_INTRVL		0x1D
#define CY_REG_TCH_TMOUT		(CY_REG_ACT_INTRVL+1)
#define CY_REG_LP_INTRVL		(CY_REG_TCH_TMOUT+1)
#define CY_SOFT_RESET			((1 << 0))
#define CY_DEEP_SLEEP			((1 << 1))
#define CY_LOW_POWER			((1 << 2))
#define CY_MAXZ				255
#define CY_OK				0
#define CY_INIT				1
#define	CY_DLY_DFLT			10	/* ms */
#define CY_DLY_SYSINFO			20	/* ms */
#define CY_DLY_BL			300
#define CY_DLY_DNLOAD			100	/* ms */
#define CY_NUM_RETRY			4	/* max num touch data read */

/* handshake bit in the hst_mode reg */
#define CY_HNDSHK_BIT			0x80
#ifdef CY_USE_HNDSHK
	#define CY_SEND_HNDSHK		1
#else
	#define CY_SEND_HNDSHK		0
#endif

/* Bootloader File 0 offset */
#define CY_BL_FILE0			0x00

/* Bootloader command directive */
#define CY_BL_CMD			0xFF

/* Bootloader Initiate Bootload */
#define CY_BL_INIT_LOAD			0x38

/* Bootloader Write a Block */
#define CY_BL_WRITE_BLK			0x39

/* Bootloader Terminate Bootload */
#define CY_BL_TERMINATE			0x3B

/* Bootloader Exit and Verify Checksum command */
#define CY_BL_EXIT			0xA5

/* Bootloader default keys */
#define CY_BL_KEY0			0x00
#define CY_BL_KEY1			0x01
#define CY_BL_KEY2			0x02
#define CY_BL_KEY3			0x03
#define CY_BL_KEY4			0x04
#define CY_BL_KEY5			0x05
#define CY_BL_KEY6			0x06
#define CY_BL_KEY7			0x07

/* Active Power state scanning/processing refresh interval */
#define CY_ACT_INTRVL_DFLT		0x00

/* touch timeout for the Active power */
#define CY_TCH_TMOUT_DFLT		0xFF

/* Low Power state scanning/processing refresh interval */
#define CY_LP_INTRVL_DFLT		0x0A

#define CY_IDLE_STATE		0
#define CY_ACTIVE_STATE		1
#define CY_LOW_PWR_STATE		2
#define CY_SLEEP_STATE		3

/* device mode bits */
#define CY_OP_MODE		0x00
#define CY_SYSINFO_MODE		0x10

/* power mode select bits */
#define CY_SOFT_RESET_MODE		0x01	/* return to Bootloader mode */
#define CY_DEEP_SLEEP_MODE		0x02
#define CY_LOW_PWR_MODE		0x04

#define CY_NUM_KEY			8

#ifdef CY_USE_GEST
	#define CY_USE_GESTURES	1
#else
	#define CY_USE_GESTURES	0
#endif /* CY_USE_GESTURE_SIGNALS */

#ifdef CY_USE_GEST_GRP1
	#define CY_GEST_GRP1	0x10
#else
	#define CY_GEST_GRP1	0x00
#endif	/* CY_USE_GEST_GRP1 */
#ifdef CY_USE_GEST_GRP2
	#define CY_GEST_GRP2	0x20
#else
	#define CY_GEST_GRP2	0x00
#endif	/* CY_USE_GEST_GRP2 */
#ifdef CY_USE_GEST_GRP3
	#define CY_GEST_GRP3	0x40
#else
	#define CY_GEST_GRP3	0x00
#endif	/* CY_USE_GEST_GRP3 */
#ifdef CY_USE_GEST_GRP4
	#define CY_GEST_GRP4	0x80
#else
	#define CY_GEST_GRP4	0x00
#endif	/* CY_USE_GEST_GRP4 */

struct cyttsp_regulator {
	const char *name;
	u32	max_uV;
	u32	min_uV;
	u32	hpm_load_uA;
	u32	lpm_load_uA;
};

struct cyttsp_platform_data {
	u32 panel_maxx;
	u32 panel_maxy;
	u32 disp_resx;
	u32 disp_resy;
	u32 disp_minx;
	u32 disp_miny;
	u32 disp_maxx;
	u32 disp_maxy;
	u8 correct_fw_ver;
	u32 flags;
	u8 gen;
	u8 use_st;
	u8 use_mt;
	u8 use_hndshk;
	u8 use_trk_id;
	u8 use_sleep;
	u8 use_gestures;
	u8 gest_set;
	u8 act_intrvl;
	u8 tch_tmout;
	u8 lp_intrvl;
	u8 power_state;
	bool wakeup;
	int sleep_gpio;
	int resout_gpio;
	int irq_gpio;
	struct cyttsp_regulator *regulator_info;
	u8 num_regulators;
	const char *fw_fname;
	bool disable_ghost_det;
#ifdef CY_USE_I2C_DRIVER
	s32 (*init)(struct i2c_client *client);
	s32 (*resume)(struct i2c_client *client);
	s32 (*suspend)(struct i2c_client *client);
#endif
#ifdef CY_USE_SPI_DRIVER
	s32 (*init)(struct spi_device *spi);
	s32 (*resume)(struct spi_device *spi);
#endif
};

/* TrueTouch Standard Product Gen3 (Txx3xx) interface definition */
struct cyttsp_gen3_xydata_t {
	u8 hst_mode;
	u8 tt_mode;
	u8 tt_stat;
	u16 x1 __attribute__ ((packed));
	u16 y1 __attribute__ ((packed));
	u8 z1;
	u8 touch12_id;
	u16 x2 __attribute__ ((packed));
	u16 y2 __attribute__ ((packed));
	u8 z2;
	u8 gest_cnt;
	u8 gest_id;
	u16 x3 __attribute__ ((packed));
	u16 y3 __attribute__ ((packed));
	u8 z3;
	u8 touch34_id;
	u16 x4 __attribute__ ((packed));
	u16 y4 __attribute__ ((packed));
	u8 z4;
	u8 tt_undef[3];
	u8 gest_set;
	u8 tt_reserved;
};

/* TrueTouch Standard Product Gen2 (Txx2xx) interface definition */
#define CY_GEN2_NOTOUCH		0x03	/* Both touches removed */
#define CY_GEN2_GHOST		0x02	/* ghost */
#define CY_GEN2_2TOUCH		0x03	/* 2 touch; no ghost */
#define CY_GEN2_1TOUCH		0x01	/* 1 touch only */
#define CY_GEN2_TOUCH2		0x01	/* 1st touch removed;
						 * 2nd touch remains */
struct cyttsp_gen2_xydata_t {
	u8 hst_mode;
	u8 tt_mode;
	u8 tt_stat;
	u16 x1 __attribute__ ((packed));
	u16 y1 __attribute__ ((packed));
	u8 z1;
	u8 evnt_idx;
	u16 x2 __attribute__ ((packed));
	u16 y2 __attribute__ ((packed));
	u8 tt_undef1;
	u8 gest_cnt;
	u8 gest_id;
	u8 tt_undef[14];
	u8 gest_set;
	u8 tt_reserved;
};

/* TTSP System Information interface definition */
struct cyttsp_sysinfo_data_t {
	u8 hst_mode;
	u8 mfg_cmd;
	u8 mfg_stat;
	u8 cid[3];
	u8 tt_undef1;
	u8 uid[8];
	u8 bl_verh;
	u8 bl_verl;
	u8 tts_verh;
	u8 tts_verl;
	u8 app_idh;
	u8 app_idl;
	u8 app_verh;
	u8 app_verl;
	u8 tt_undef[6];
	u8 act_intrvl;
	u8 tch_tmout;
	u8 lp_intrvl;
};

/* TTSP Bootloader Register Map interface definition */
#define CY_BL_CHKSUM_OK		0x01
struct cyttsp_bootloader_data_t {
	u8 bl_file;
	u8 bl_status;
	u8 bl_error;
	u8 blver_hi;
	u8 blver_lo;
	u8 bld_blver_hi;
	u8 bld_blver_lo;
	u8 ttspver_hi;
	u8 ttspver_lo;
	u8 appid_hi;
	u8 appid_lo;
	u8 appver_hi;
	u8 appver_lo;
	u8 cid_0;
	u8 cid_1;
	u8 cid_2;
};

#define cyttsp_wake_data_t		cyttsp_gen3_xydata_t
#ifdef CY_DECLARE_GLOBALS
	#ifdef CY_INCLUDE_LOAD_FILE
		/* this file declares:
		 * firmware download block array (cyttsp_fw[]),
		 * the number of command block records (cyttsp_fw_records),
		 * and the version variables
		 */
		#include "cyttsp_fw.h"		/* imports cyttsp_fw[] array */
		#define cyttsp_app_load()	1
		#ifdef CY_FORCE_FW_UPDATE
			#define cyttsp_force_fw_load()	1
		#else
			#define cyttsp_force_fw_load()	0
		#endif

	#else
		/* the following declarations are to allow
		 * some debugging capability
		 */
		unsigned char cyttsp_fw_tts_verh = 0x00;
		unsigned char cyttsp_fw_tts_verl = 0x01;
		unsigned char cyttsp_fw_app_idh = 0x02;
		unsigned char cyttsp_fw_app_idl = 0x03;
		unsigned char cyttsp_fw_app_verh = 0x04;
		unsigned char cyttsp_fw_app_verl = 0x05;
		unsigned char cyttsp_fw_cid_0 = 0x06;
		unsigned char cyttsp_fw_cid_1 = 0x07;
		unsigned char cyttsp_fw_cid_2 = 0x08;
		#define cyttsp_app_load()	0
		#define cyttsp_force_fw_load()	0
	#endif
	#define cyttsp_tts_verh()	cyttsp_fw_tts_verh
	#define cyttsp_tts_verl()	cyttsp_fw_tts_verl
	#define cyttsp_app_idh()	cyttsp_fw_app_idh
	#define cyttsp_app_idl()	cyttsp_fw_app_idl
	#define cyttsp_app_verh()	cyttsp_fw_app_verh
	#define cyttsp_app_verl()	cyttsp_fw_app_verl
	#define cyttsp_cid_0()		cyttsp_fw_cid_0
	#define cyttsp_cid_1()		cyttsp_fw_cid_1
	#define cyttsp_cid_2()		cyttsp_fw_cid_2
	#ifdef CY_USE_TEST_DATA
		static struct cyttsp_gen2_xydata_t tt_gen2_testray[] = {
		{0x00}, {0x00}, {0x04},
		{0x4000}, {0x8000}, {0x80},
		{0x03},
		{0x2000}, {0x1000}, {0x00},
		{0x00},
		{0x00},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00},
		{0x00}
		};

		static struct cyttsp_gen3_xydata_t tt_gen3_testray[] = {
		{0x00}, {0x00}, {0x04},
		{0x4000}, {0x8000}, {0x80},
		{0x12},
		{0x2000}, {0x1000}, {0xA0},
		{0x00}, {0x00},
		{0x8000}, {0x4000}, {0xB0},
		{0x34},
		{0x4000}, {0x1000}, {0xC0},
		{0x00, 0x00, 0x00},
		{0x00},
		{0x00}
		};
	#endif /* CY_USE_TEST_DATA */

#else
		extern u8 g_appload_ray[];
#endif

#endif /* __CYTTSP_H__ */
