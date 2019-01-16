#ifndef _CUST_PMIC_H_
#define _CUST_PMIC_H_

#define LOW_POWER_LIMIT_LEVEL_1 15

//Define for disable low battery protect feature, default no define for enable low battery protect.
//#define DISABLE_LOW_BATTERY_PROTECT

//Define for disable battery OC protect feature, default no define for enable low battery protect.
//#define DISABLE_BATTERY_OC_PROTECT

//Define for disable battery 15% protect feature, default no define for enable low battery protect.
//#define DISABLE_BATTERY_PERCENT_PROTECT


/* ADC Channel Number */
typedef enum {
	//MT6331
	ADC_TSENSE_31_AP = 	0x004,
	ADC_VACCDET_AP,
	ADC_VISMPS_1_AP,
	ADC_ADCVIN0_AP,
	ADC_HP_AP = 		0x009,
        
	//MT6332
	ADC_BATSNS_AP = 	0x010,
	ADC_ISENSE_AP,
	ADC_VBIF_AP,
	ADC_BATON_AP,
	ADC_TSENSE_32_AP,
	ADC_VCHRIN_AP,
	ADC_VISMPS_2_AP,
	ADC_VUSB_AP,
	ADC_M3_REF_AP,   
	ADC_SPK_ISENSE_AP,
	ADC_SPK_THR_V_AP,
	ADC_SPK_THR_I_AP,

	ADC_VADAPTOR_AP =	0x027,        
	ADC_TSENSE_31_MD = 	0x104,
	ADC_ADCVIN0_MD = 	0x107,
	ADC_TSENSE_32_MD =	0x114,
	ADC_ADCVIN0_GPS = 	0x208
} upmu_adc_chl_list_enum;

typedef enum {
	MT6331_CHIP = 0,
	MT6332_CHIP,
	ADC_CHIP_MAX
} upmu_adc_chip_list_enum;

#endif /* _CUST_PMIC_H_ */ 
