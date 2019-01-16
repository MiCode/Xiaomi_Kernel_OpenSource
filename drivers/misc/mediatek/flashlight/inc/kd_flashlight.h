#ifndef _KD_FLASHLIGHT_H
#define _KD_FLASHLIGHT_H

#include <linux/ioctl.h>

/*************************************************
*
**************************************************/
//In KERNEL mode,SHOULD be sync with mediatype.h
//CHECK before remove or modify
//#undef BOOL
//#define BOOL signed int
#ifndef _MEDIA_TYPES_H
typedef unsigned char   MUINT8;
typedef unsigned short  MUINT16;
typedef unsigned int    MUINT32;
typedef signed char     MINT8;
typedef signed short    MINT16;
typedef signed int      MINT32;
#endif

enum
{
	e_Max_Strobe_Num_Per_Dev=2,
	e_Max_Part_Num_Per_Dev=2,
	e_Max_Sensor_Dev_Num=3,
};

/* cotta-- added for high current solution */
#define KD_STROBE_HIGH_CURRENT_WIDTH 0xFF

/* cotta-- time limit of strobe watch dog timer. unit : ms */
#define FLASH_LIGHT_WDT_TIMEOUT_MS 300



//FIXME: temp. solutoin for main/sub sensor mapping
#define KD_DEFAULT_FLASHLIGHT_INDEX    0
#define KD_CUSTOM_FLASHLIGHT_INDEX    1


#define KD_DEFAULT_FLASHLIGHT_ID    0
#define KD_DUMMY_FLASHLIGHT_ID      1
#define KD_PEAK_FLASHLIGHT_ID       2
#define KD_TORCH_FLASHLIGHT_ID      3
#define KD_CONSTANT_FLASHLIGHT_ID   4


typedef enum
{
    e_CAMERA_NONE_SENSOR=0,
    e_CAMERA_MAIN_SENSOR     = 1,
    e_CAMERA_SUB_SENSOR      = 2,
    e_CAMERA_MAIN_2_SENSOR   = 4,
    //for backward compatible
    e_CAMERA_MAIN_SECOND_SENSOR = 4,
    //DUAL_CAMERA_SUB_2_SENSOR   = 16,
    e_CAMERA_SENSOR_MAX
} eFlashSensorId;

typedef enum
{
    e_FLASH_DRIVER_6332=0,
    e_FLASH_DRIVER_OTHERS=1   //ADD BY LCSH LVXIAOLIANG FOR DUAL FLASH 

} eDrvierNameId;

typedef struct
{
    int (* flashlight_open)(void *pArg);
    int (* flashlight_release)(void *pArg);
    int (* flashlight_ioctl)(unsigned int cmd, unsigned long arg);
} FLASHLIGHT_FUNCTION_STRUCT, *PFLASHLIGHT_FUNCTION_STRUCT;



typedef struct
{
    int sensorDev;
	int strobeId;
    int arg;

}kdStrobeDrvArg;

typedef struct
{
    MUINT32 (* flashlightInit)(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);
} KD_FLASHLIGHT_INIT_FUNCTION_STRUCT, *pKD_FLASHLIGHT_INIT_FUNCTION_STRUCT;

typedef enum {
    FLASHLIGHTDRV_STATE_PREVIEW,
    FLASHLIGHTDRV_STATE_STILL,
}eFlashlightState;

//flash type enum
typedef enum
{
    FLASHLIGHT_NONE = 0,
    FLASHLIGHT_LED_ONOFF,           // LED always on/off
    FLASHLIGHT_LED_CONSTANT,        // CONSTANT type LED
    FLASHLIGHT_LED_PEAK,            // peak strobe type LED
    FLASHLIGHT_LED_TORCH,           // LED turn on when switch FLASH_ON
    FLASHLIGHT_XENON_SCR,           // SCR strobe type Xenon
    FLASHLIGHT_XENON_IGBT           // IGBT strobe type Xenon
}   FLASHLIGHT_TYPE_ENUM;


#define FLASHLIGHT_MAGIC 'S'
//S means "set through a ptr"
//T means "tell by a arg value"
//G means "get by a ptr"
//Q means "get by return a value"
//X means "switch G and S atomically"
//H means "switch T and Q atomically"

//FLASHLIGHTIOC_T_ENABLE : Tell FLASHLIGHT to turn ON/OFF
#define FLASHLIGHTIOC_T_ENABLE _IOW(FLASHLIGHT_MAGIC,5, int  )

//set flashlight power level 0~31(weak~strong)
#define FLASHLIGHTIOC_T_LEVEL _IOW(FLASHLIGHT_MAGIC,10, int)

//set flashlight time us
#define FLASHLIGHTIOC_T_FLASHTIME _IOW(FLASHLIGHT_MAGIC,15, int)

//set flashlight state
#define FLASHLIGHTIOC_T_STATE _IOW(FLASHLIGHT_MAGIC,20, int)

//get flash type
#define FLASHLIGHTIOC_G_FLASHTYPE _IOR(FLASHLIGHT_MAGIC,25, int)

//set flashlight driver
#define FLASHLIGHTIOC_X_SET_DRIVER _IOWR(FLASHLIGHT_MAGIC,30,int)

/* cotta-- set capture delay of sensor */
#define FLASHLIGHTIOC_T_DELAY _IOW(FLASHLIGHT_MAGIC, 35, int)


#define FLASH_IOC_SET_TIME_OUT_TIME_MS  _IOR(FLASHLIGHT_MAGIC, 100, int)
#define FLASH_IOC_SET_STEP 		        _IOR(FLASHLIGHT_MAGIC, 105, int)
#define FLASH_IOC_SET_DUTY				_IOR(FLASHLIGHT_MAGIC, 110, int)
#define FLASH_IOC_SET_ONOFF           	_IOR(FLASHLIGHT_MAGIC, 115, int)
#define FLASH_IOC_UNINIT           	_IOR(FLASHLIGHT_MAGIC, 120, int)

#define FLASH_IOC_PRE_ON           	_IOR(FLASHLIGHT_MAGIC, 125, int)
#define FLASH_IOC_GET_PRE_ON_TIME_MS           	_IOR(FLASHLIGHT_MAGIC, 130, int)
#define FLASH_IOC_GET_PRE_ON_TIME_MS_DUTY           	_IOR(FLASHLIGHT_MAGIC, 131, int)

#define FLASH_IOC_SET_REG_ADR  _IOR(FLASHLIGHT_MAGIC, 135, int)
#define FLASH_IOC_SET_REG_VAL  _IOR(FLASHLIGHT_MAGIC, 140, int)
#define FLASH_IOC_SET_REG  _IOR(FLASHLIGHT_MAGIC, 145, int)
#define FLASH_IOC_GET_REG  _IOR(FLASHLIGHT_MAGIC, 150, int)


#define FLASH_IOC_GET_MAIN_PART_ID           	_IOR(FLASHLIGHT_MAGIC, 155, int)
#define FLASH_IOC_GET_SUB_PART_ID           	_IOR(FLASHLIGHT_MAGIC, 160, int)
#define FLASH_IOC_GET_MAIN2_PART_ID           	_IOR(FLASHLIGHT_MAGIC, 165, int)
#define FLASH_IOC_GET_PART_ID           	_IOR(FLASHLIGHT_MAGIC, 166, int)


#define FLASH_IOC_HAS_LOW_POWER_DETECT _IOR(FLASHLIGHT_MAGIC, 170, int)
#define FLASH_IOC_LOW_POWER_DETECT_START _IOR(FLASHLIGHT_MAGIC, 175, int)
#define FLASH_IOC_LOW_POWER_DETECT_END _IOR(FLASHLIGHT_MAGIC, 180, int)
#define FLASH_IOC_IS_LOW_POWER _IOR(FLASHLIGHT_MAGIC, 182, int)


#define FLASH_IOC_GET_ERR _IOR(FLASHLIGHT_MAGIC, 185, int)
#define FLASH_IOC_GET_PROTOCOL_VERSION _IOR(FLASHLIGHT_MAGIC, 190, int) //0: old, 1: 95

#define FLASH_IOC_IS_CHARGER_IN _IOR(FLASHLIGHT_MAGIC, 195, int)
#define FLASH_IOC_IS_OTG_USE _IOR(FLASHLIGHT_MAGIC, 200, int)
#define FLASH_IOC_GET_FLASH_DRIVER_NAME_ID _IOR(FLASHLIGHT_MAGIC, 205, int)



typedef struct
{
    int sensorDev;
    int arg;

}StrobeDrvArg;




#endif

