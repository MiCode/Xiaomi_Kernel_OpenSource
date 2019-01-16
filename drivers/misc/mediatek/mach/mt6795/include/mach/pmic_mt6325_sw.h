#ifndef _MT6325_PMIC_SW_H_
#define _MT6325_PMIC_SW_H_

#include <mach/mt_typedefs.h>

//==============================================================================
// The CHIP INFO
//==============================================================================
#define PMIC6325_E1_CID_CODE    0x2510
#define PMIC6325_E2_CID_CODE    0x2520
#define PMIC6325_E3_CID_CODE    0x2530

//==============================================================================
// The AUXADC INFO
//==============================================================================
#define AUXADC_CHANNEL_MASK	0x1f
#define AUXADC_CHANNEL_SHIFT	0
#define AUXADC_CHIP_MASK	0x03
#define AUXADC_CHIP_SHIFT	5
#define AUXADC_USER_MASK	0x0f
#define AUXADC_USER_SHIFT	8

typedef enum {
	AP = 0,
	MD,
	GPS,
	ADC_USER_MAX	
} upmu_adc_user_list_enum;
typedef enum {
	MT6325_CHIP = 0,
	MT6311_CHIP,
	ADC_CHIP_MAX
} upmu_adc_chip_list_enum;
//==============================================================================
// PMIC Exported Function
//==============================================================================
extern U32 pmic_read_interface (U32 RegNum, U32 *val, U32 MASK, U32 SHIFT);
extern U32 pmic_config_interface (U32 RegNum, U32 val, U32 MASK, U32 SHIFT);
extern U32 pmic_read_interface_nolock (U32 RegNum, U32 *val, U32 MASK, U32 SHIFT);
extern U32 pmic_config_interface_nolock (U32 RegNum, U32 val, U32 MASK, U32 SHIFT);
extern void pmic_lock(void);
extern void pmic_unlock(void);

#endif // _MT6325_PMIC_SW_H_

