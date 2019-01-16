#ifndef _BU64745GWZAF_H
#define _BU64745GWZAF_H

#include <linux/ioctl.h>
//#include "kd_imgsensor.h"

#define BU64745GWZAF_MAGIC 'A'
//IOCTRL(inode * ,file * ,cmd ,arg )


//Structures
typedef struct {
//current position
u32 u4CurrentPosition;
//macro position
u32 u4MacroPosition;
//Infiniti position
u32 u4InfPosition;
//Motor Status
bool          bIsMotorMoving;
//Motor Open?
bool          bIsMotorOpen;
//Support SR?
bool          bIsSupportSR;
} stBU64745GWZAF_MotorInfo;

//Control commnad
//S means "set through a ptr"
//T means "tell by a arg value"
//G means "get by a ptr"
//Q means "get by return a value"
//X means "switch G and S atomically"
//H means "switch T and Q atomically"
#define BU64745GWZAFIOC_G_MOTORINFO _IOR(BU64745GWZAF_MAGIC,0,stBU64745GWZAF_MotorInfo)

#define BU64745GWZAFIOC_T_MOVETO _IOW(BU64745GWZAF_MAGIC,1,u32)

#define BU64745GWZAFIOC_T_SETINFPOS _IOW(BU64745GWZAF_MAGIC,2,u32)
#define BU64745GWZAFIOC_T_SETMACROPOS _IOW(BU64745GWZAF_MAGIC,3,u32)

typedef        short int                    CL_BOOL;

typedef        char                        CL_BYTE;
typedef        short int                    CL_WORD;
typedef        long int                    CL_LONG;
typedef        unsigned char                CL_UBYTE;
typedef        unsigned short int            CL_UWORD;
typedef        unsigned long int            CL_ULONG;

typedef        volatile char                CL_vBYTE;
typedef        volatile short int            CL_vWORD;
typedef        volatile long int            CL_vLONG;
typedef        volatile unsigned char        CL_vUBYTE;
typedef        volatile unsigned short int    CL_vUWORD;
typedef        volatile unsigned long int    CL_vULONG;

#define        _SLV_FBAF_        0xE8

CL_UWORD    gl_AMPGAIN ;
CL_UWORD    gl_PREOFST ;
CL_UWORD    gl_POSTOFS ;
CL_UWORD    gl_HOFS    ;
CL_UWORD    gl_KgHG    ;
CL_UWORD    gl_KgLPG   ;
CL_UWORD    gl_Kf0A    ;
CL_UWORD    gl_CUROFS  ;

CL_UWORD    POS_TARGET ;


#else
#endif
