/*
 * File: miniISP_ioctl.h
 * Description: miniISP ioctl
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 */

/*
 * This file is part of al6100.
 *
 * al6100 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * al6100 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTIBILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License version 2 for
 * more details.
 *
 * You should have received a copy of the General Public License version 2
 * along with al6100. If not, see https://www.gnu.org/licenses/gpl-2.0.html.
 */

/* /ALTEK_TAG_HwMiniISP>>> */

#ifndef _MINI_ISP_IOCTL_H_
#define _MINI_ISP_IOCTL_H_

#include <linux/types.h>
#include <linux/ioctl.h>         /* _IOW(), _IOR() */

#define SPIBULK_BLOCKSIZE 8192 /* 8k bytes */
#define SPI_CMD_LENGTH 64

#define ISPCMD_LOAD_FW 0x0001
#define ISPCMD_PURE_BYPASS 0x0010
#define ISPCMD_POWER_OFF 0x0100
#define ISPCMD_ENTER_CP_MODE 0x0200
#define ISPCMD_LEAVE_CP_MODE_STANDBY 0x0002
#define ISPCMD_NONE 0x0000


/*Calibration Profile*/
#define ISPCMD_CAMERA_SET_SENSORMODE 0x300A
#define ISPCMD_CAMERA_GET_SENSORMODE 0x300B
#define ISPCMD_CAMERA_SET_OUTPUTFORMAT 0x300D
#define ISPCMD_CAMERA_SET_CP_MODE 0x300E
#define ISPCMD_CAMERA_SET_AE_STATISTICS 0x300F
#define ISPCMD_CAMERA_PREVIEWSTREAMONOFF 0x3010
#define ISPCMD_CAMERA_DUALPDYCALCULATIONWEIGHT 0x3011
#define ISPCMD_LED_POWERCONTROL 0x3012
#define ISPCMD_CAMERA_ACTIVE_AE 0x3013
#define ISPCMD_ISP_AECONTROLONOFF 0x3014
#define ISPCMD_CAMERA_SET_FRAMERATELIMITS 0x3015
#define ISPCMD_CAMERA_SET_PERIODDROPFRAME 0x3016
#define ISPCMD_CAMERA_SET_MAX_EXPOSURE 0x3017
#define ISPCMD_CAMERA_SET_AE_TARGET_MEAN 0x3018
#define ISPCMD_CAMERA_FRAME_SYNC_CONTROL 0x3019
#define ISPCMD_CAMERA_SET_SHOT_MODE 0x301A
#define ISPCMD_CAMERA_LIGHTING_CTRL 0x301B
#define ISPCMD_CAMERA_DEPTH_COMPENSATION 0x301C
#define ISPCMD_CAMERA_TRIGGER_DEPTH_PROCESS_CTRL 0x301D
#define ISPCMD_CAMERA_SET_MIN_EXPOSURE 0x301E
#define ISPCMD_CAMERA_SET_MAX_EXPOSURE_SLOPE 0x301F



/* Bulk Data*/
#define ISPCMD_BULK_WRITE_BASICCODE 0x2002
#define ISPCMD_BULK_WRITE_BOOTCODE 0x2008
#define ISPCMD_BULK_READ_MEMORY 0x2101
#define ISPCMD_BULK_READ_COMLOG 0x2102
#define ISPCMD_BULK_WRITE_CALIBRATION_DATA 0x210B

/*Basic Setting*/
#define ISPCMD_BASIC_SET_DEPTH_3A_INFO 0x10B9
#define ISPCMD_BASIC_SET_DEPTH_AUTO_INTERLEAVE_MODE 0x10BC
#define ISPCMD_BASIC_SET_INTERLEAVE_MODE_DEPTH_TYPE 0x10BD
#define ISPCMD_BASIC_SET_DEPTH_POLISH_LEVEL 0x10BE

/*System Cmd*/
#define ISPCMD_SYSTEM_GET_STATUSOFLASTEXECUTEDCOMMAND 0x0015
#define ISPCMD_SYSTEM_GET_ERRORCODE 0x0016
#define ISPCMD_SYSTEM_SET_ISPREGISTER 0x0100
#define ISPCMD_SYSTEM_GET_ISPREGISTER 0x0101
/*#define ISPCMD_SYSTEM_SET_DEBUGCMD 0x0104*/
#define ISPCMD_SYSTEM_SET_COMLOGLEVEL 0x0109
#define ISPCMD_SYSTEM_GET_CHIPTESTREPORT 0x010A

/*Operarion Code*/
#define ISPCMD_MINIISPOPEN 0x4000



/* ALTEK_AL6100_KERNEL >>> */
#define IOC_ISP_CTRL_FLOW_KERNEL_MAGIC 'D'

#define IOCTL_ISP_RUN_TASK_INIT \
	_IOWR(IOC_ISP_CTRL_FLOW_KERNEL_MAGIC, BASE_MINIISP_CONTROL, struct miniISP_cmd_config)

#define IOCTL_ISP_RUN_TASK_START \
	_IOWR(IOC_ISP_CTRL_FLOW_KERNEL_MAGIC, BASE_MINIISP_CONTROL + 1, struct miniISP_cmd_config)

#define IOCTL_ISP_RUN_TASK_STOP \
	_IOWR(IOC_ISP_CTRL_FLOW_KERNEL_MAGIC, BASE_MINIISP_CONTROL + 2, struct miniISP_cmd_config)

#define IOCTL_ISP_RUN_TASK_DEINIT \
	_IOWR(IOC_ISP_CTRL_FLOW_KERNEL_MAGIC, BASE_MINIISP_CONTROL + 3, struct miniISP_cmd_config)

#define IOCTL_ISP_RUN_TASK_QUERY \
	_IOWR(IOC_ISP_CTRL_FLOW_KERNEL_MAGIC, BASE_MINIISP_CONTROL + 4, struct miniISP_cmd_config)

struct miniISP_chi_param {
	/* uint8_t board_power_always_on; //1: power is always on */
	uint8_t scid;
	uint8_t irflood_mode;
	uint8_t merge_mode;
} __attribute__ ((packed));


/* ALTEK_AL6100_KERNEL <<< */




/* TODO: Need to solve the kernel panic >>> */
struct miniISP_cmd_config {
	uint16_t opcode;
	uint32_t size;
	uint64_t param;
} __attribute__ ((packed));

#define BASE_MINIISP_CONTROL 100

#define IOC_ISP_CTRL_FLOW_MAGIC 'C'

#define IOCTL_ISP_LOAD_FW \
	_IOWR(IOC_ISP_CTRL_FLOW_MAGIC, BASE_MINIISP_CONTROL, struct miniISP_cmd_config)

#define IOCTL_ISP_PURE_BYPASS \
	_IOWR(IOC_ISP_CTRL_FLOW_MAGIC, BASE_MINIISP_CONTROL + 1, struct miniISP_cmd_config)

#define IOCTL_ISP_POWER_OFF \
	_IOWR(IOC_ISP_CTRL_FLOW_MAGIC, BASE_MINIISP_CONTROL + 2, struct miniISP_cmd_config)

#define IOCTL_ISP_ENTER_CP_MODE \
		_IOWR(IOC_ISP_CTRL_FLOW_MAGIC, BASE_MINIISP_CONTROL + 3, struct miniISP_cmd_config)

#define IOCTL_ISP_LEAVE_CP_MODE \
		_IOWR(IOC_ISP_CTRL_FLOW_MAGIC, BASE_MINIISP_CONTROL + 4, struct miniISP_cmd_config)

#define IOCTL_ISP_CTRL_CMD \
	_IOWR(IOC_ISP_CTRL_FLOW_MAGIC, BASE_MINIISP_CONTROL + 5, struct miniISP_cmd_config)

#define IOCTL_ISP_POWER_ON \
	_IOWR(IOC_ISP_CTRL_FLOW_MAGIC, BASE_MINIISP_CONTROL + 6, struct miniISP_cmd_config)

#define IOCTL_ISP_DEINIT \
	_IOWR(IOC_ISP_CTRL_FLOW_MAGIC, BASE_MINIISP_CONTROL + 7, struct miniISP_cmd_config)

/* /ALTEK_TAG_HwMiniISP<<< */
#endif

