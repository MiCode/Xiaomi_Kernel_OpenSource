#ifndef _MT_PMIC_UPMU_SW_H_
#define _MT_PMIC_UPMU_SW_H_

#include <mach/pmic_mt6331_6332_sw.h>

//==============================================================================
// Low battery level define
//==============================================================================
typedef enum LOW_BATTERY_LEVEL_TAG 
{
    LOW_BATTERY_LEVEL_0 = 0,
    LOW_BATTERY_LEVEL_1 = 1,
    LOW_BATTERY_LEVEL_2 = 2
} LOW_BATTERY_LEVEL;

typedef enum LOW_BATTERY_PRIO_TAG 
{
    LOW_BATTERY_PRIO_CPU_B      = 0,
    LOW_BATTERY_PRIO_CPU_L      = 1,
    LOW_BATTERY_PRIO_GPU        = 2,    
    LOW_BATTERY_PRIO_MD         = 3,
    LOW_BATTERY_PRIO_MD5        = 4,
    LOW_BATTERY_PRIO_FLASHLIGHT = 5,
    LOW_BATTERY_PRIO_VIDEO      = 6,
    LOW_BATTERY_PRIO_WIFI       = 7,
    LOW_BATTERY_PRIO_BACKLIGHT  = 8
} LOW_BATTERY_PRIO;

extern void (*low_battery_callback)(LOW_BATTERY_LEVEL);
extern void register_low_battery_notify( void (*low_battery_callback)(LOW_BATTERY_LEVEL), LOW_BATTERY_PRIO prio_val );


//==============================================================================
// Battery OC level define
//==============================================================================
typedef enum BATTERY_OC_LEVEL_TAG 
{
    BATTERY_OC_LEVEL_0 = 0,
    BATTERY_OC_LEVEL_1 = 1    
} BATTERY_OC_LEVEL;

typedef enum BATTERY_OC_PRIO_TAG 
{
    BATTERY_OC_PRIO_CPU_B      = 0,
    BATTERY_OC_PRIO_CPU_L      = 1,
    BATTERY_OC_PRIO_GPU        = 2
} BATTERY_OC_PRIO;

extern void (*battery_oc_callback)(BATTERY_OC_LEVEL);
extern void register_battery_oc_notify( void (*battery_oc_callback)(BATTERY_OC_LEVEL), BATTERY_OC_PRIO prio_val );


//==============================================================================
// Battery percent define
//==============================================================================
typedef enum BATTERY_PERCENT_LEVEL_TAG 
{
    BATTERY_PERCENT_LEVEL_0 = 0,
    BATTERY_PERCENT_LEVEL_1 = 1
} BATTERY_PERCENT_LEVEL;

typedef enum BATTERY_PERCENT_PRIO_TAG 
{
    BATTERY_PERCENT_PRIO_CPU_B      = 0,
    BATTERY_PERCENT_PRIO_CPU_L      = 1,
    BATTERY_PERCENT_PRIO_GPU        = 2,    
    BATTERY_PERCENT_PRIO_MD         = 3,
    BATTERY_PERCENT_PRIO_MD5        = 4,
    BATTERY_PERCENT_PRIO_FLASHLIGHT = 5,
    BATTERY_PERCENT_PRIO_VIDEO      = 6,
    BATTERY_PERCENT_PRIO_WIFI       = 7,
    BATTERY_PERCENT_PRIO_BACKLIGHT  = 8
} BATTERY_PERCENT_PRIO;

extern void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL);
extern void register_battery_percent_notify( void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL), BATTERY_PERCENT_PRIO prio_val );

#endif // _MT_PMIC_UPMU_SW_H_

