#ifndef _MT6331_6332_PMIC_SW_H_
#define _MT6331_6332_PMIC_SW_H_

#include <mach/mt_typedefs.h>

//==============================================================================
// The CHIP INFO
//==============================================================================
#define PMIC6331_E1_CID_CODE    0x3110
#define PMIC6331_E2_CID_CODE    0x3120
#define PMIC6331_E3_CID_CODE    0x3130

#define PMIC6332_E1_CID_CODE    0x3210
#define PMIC6332_E2_CID_CODE    0x3220
#define PMIC6332_E3_CID_CODE    0x3230

//==============================================================================
// The AUXADC INFO
//==============================================================================
#define AUXADC_CHANNEL_MASK	0x0f
#define AUXADC_CHANNEL_SHIFT	0
#define AUXADC_CHIP_MASK	0x0f
#define AUXADC_CHIP_SHIFT	4
#define AUXADC_USER_MASK	0x0f
#define AUXADC_USER_SHIFT	8
#define CLEAR_REQ		0
#define SET_REQ			1	
#define ONLY_REQ		2

typedef enum {
	AP = 0,
	MD,
	GPS,
	ADC_USER_MAX	
} upmu_adc_user_list_enum;
//==============================================================================
// PMIC Exported Function
//==============================================================================
extern U32 pmic_read_interface (U32 RegNum, U32 *val, U32 MASK, U32 SHIFT);
extern U32 pmic_config_interface (U32 RegNum, U32 val, U32 MASK, U32 SHIFT);
extern U32 pmic_read_interface_nolock (U32 RegNum, U32 *val, U32 MASK, U32 SHIFT);
extern U32 pmic_config_interface_nolock (U32 RegNum, U32 val, U32 MASK, U32 SHIFT);
extern void pmic_lock(void);
extern void pmic_unlock(void);

#endif // _MT6331_6332_PMIC_SW_H_

