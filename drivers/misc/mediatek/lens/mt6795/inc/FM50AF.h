#ifndef _FM50AF_H
#define _FM50AF_H

#include <linux/ioctl.h>
/* #include "kd_imgsensor.h" */

#define FM50AF_MAGIC 'A'
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
} stFM50AF_MotorInfo;

/* Control commnad */
/* S means "set through a ptr" */
/* T means "tell by a arg value" */
/* G means "get by a ptr" */
/* Q means "get by return a value" */
/* X means "switch G and S atomically" */
/* H means "switch T and Q atomically" */
#define FM50AFIOC_G_MOTORINFO _IOR(FM50AF_MAGIC, 0, stFM50AF_MotorInfo)

#define FM50AFIOC_T_MOVETO _IOW(FM50AF_MAGIC, 1, u32)

#define FM50AFIOC_T_SETINFPOS _IOW(FM50AF_MAGIC, 2, u32)

#define FM50AFIOC_T_SETMACROPOS _IOW(FM50AF_MAGIC, 3, u32)

#else
#endif
