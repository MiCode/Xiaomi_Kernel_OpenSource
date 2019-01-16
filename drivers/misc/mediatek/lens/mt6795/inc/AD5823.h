#ifndef _AD5823_H
#define _AD5823_H

#include <linux/ioctl.h>
/* #include "kd_imgsensor.h" */

#define AD5823_MAGIC 'A'
/* IOCTRL(inode * ,file * ,cmd ,arg ) */


/* Structures */
typedef struct {
/* current position */
	u32 u4CurrentPosition;
/* macro position */
	u32 u4MacroPosition;
/* Infiniti position */
	u32 u4InfPosition;
/* Motor Status */
	bool bIsMotorMoving;
/* Motor Open? */
	bool bIsMotorOpen;
/* Support SR? */
	bool bIsSupportSR;
} stAD5823_MotorInfo;

/* Control commnad */
/* S means "set through a ptr" */
/* T means "tell by a arg value" */
/* G means "get by a ptr" */
/* Q means "get by return a value" */
/* X means "switch G and S atomically" */
/* H means "switch T and Q atomically" */
#define AD5823IOC_G_MOTORINFO _IOR(AD5823_MAGIC, 0, stAD5823_MotorInfo)

#define AD5823IOC_T_MOVETO _IOW(AD5823_MAGIC, 1, u32)

#define AD5823IOC_T_SETINFPOS _IOW(AD5823_MAGIC, 2, u32)

#define AD5823IOC_T_SETMACROPOS _IOW(AD5823_MAGIC, 3, u32)

#else
#endif
