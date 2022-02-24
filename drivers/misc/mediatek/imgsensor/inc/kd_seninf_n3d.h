/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _KD_SENINF_N3D_H_
#define _KD_SENINF_N3D_H_

struct sensor_info {
	unsigned int sensor_id;
	unsigned int sensor_idx;
	unsigned int cammux_id;

	unsigned int fl_active_delay;
	unsigned int def_fl_lc;          // default framelength_lc
	unsigned int max_fl_lc;          // for framelength boundary check
	unsigned int def_shutter_lc;     // default shutter_lc
};

struct n3d_perframe {
	unsigned int sensor_id;
	unsigned int sensor_idx;

	/* bellow items can be query from "subdrv_ctx" */
	unsigned int min_fl_lc;          // also means max frame rate
	unsigned int shutter_lc;
	unsigned int margin_lc;
	unsigned int flicker_en;
	unsigned int out_fl_lc;

	/* for on-the-fly mode change */
	unsigned int pclk;               // write_shutter(), set_max_framerate()
	unsigned int linelength;         // write_shutter(), set_max_framerate()
	/* lineTimeInNs ~= 10^9 * (linelength/pclk) */
	unsigned int lineTimeInNs;
};

struct KD_REGISTER_SENSOR {
	struct sensor_info sensor;
};

struct KD_N3D_AE_INFO {
	struct n3d_perframe ae_info;
};

struct KD_N3D_PERFRAME {
	struct n3d_perframe per1;
	struct n3d_perframe per2;
};

#define SENINF_N3D_MAGIC 'i'
 /* IOCTRL(inode * ,file * ,cmd ,arg ) */
 /* S means "set through a ptr" */
 /* T means "tell by a arg value" */
 /* G means "get by a ptr" */
 /* Q means "get by return a value" */
 /* X means "switch G and S atomically" */
 /* H means "switch T and Q atomically" */

#define KDSENINFN3DIOC_X_REGISTER_SENSOR             \
		_IOWR(SENINF_N3D_MAGIC, 0, struct KD_REGISTER_SENSOR)

#define KDSENINFN3DIOC_X_UNREGISTER_SENSOR           \
		_IOWR(SENINF_N3D_MAGIC, 5, struct KD_REGISTER_SENSOR)

#define KDSENINFN3DIOC_X_START_SYNC                  \
		_IO(SENINF_N3D_MAGIC, 10)

#define KDSENINFN3DIOC_X_STOP_SYNC                   \
		_IO(SENINF_N3D_MAGIC, 15)

#define KDSENINFN3DIOC_X_UPDATE_AE_INFO              \
		_IOWR(SENINF_N3D_MAGIC, 20, struct KD_N3D_AE_INFO)

#define KDSENINFN3DIOC_X_PERFRAME_CTRL               \
		_IOWR(SENINF_N3D_MAGIC, 25, struct KD_N3D_PERFRAME)

#endif
