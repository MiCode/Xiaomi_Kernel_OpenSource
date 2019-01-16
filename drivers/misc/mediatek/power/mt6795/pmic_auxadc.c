#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/earlysuspend.h>
#include <linux/seq_file.h>

#include <asm/uaccess.h>
#include <linux/string.h>
#include <mach/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mach/mt_pm_ldo.h>
#include <mach/eint.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_gpio.h>
#include <mach/mtk_rtc.h>
#include <mach/mt_spm_mtcmos.h>

#include <mach/battery_common.h>
#include <linux/time.h>
#include <mach/pmic_mt6331_6332_sw.h>
#include <cust_pmic.h>
#include <cust_battery_meter.h>
//==============================================================================
// Extern
//==============================================================================
extern int Enable_BATDRV_LOG;
extern int g_R_BAT_SENSE;
extern int g_R_I_SENSE;
extern int g_R_CHARGER_1;
extern int g_R_CHARGER_2;
extern int g_bat_init_flag;


//==============================================================================
// PMIC-AUXADC related define
//==============================================================================
#define VOLTAGE_FULL_RANGE     	3200
#define ADC_PRECISE         	4096 	// 12 bits
#define ADC_PRECISE_CH7     	32768 	// 15 bits

//==============================================================================
// PMIC-AUXADC global variable
//==============================================================================
kal_int32 count_time_out=100;
struct wake_lock pmicAuxadc_irq_lock;
static DEFINE_SPINLOCK(pmic_adc_lock);
static DEFINE_MUTEX(pmic_auxadc_mutex);


//==============================================================================
// PMIC-AUXADC related API
//==============================================================================
void pmic_auxadc_init(void)
{
    wake_lock_init(&pmicAuxadc_irq_lock, WAKE_LOCK_SUSPEND, "pmicAuxadc irq wakelock");
    
	// for batses, isense
	if(mt6332_upmu_get_swcid()==PMIC6332_E1_CID_CODE) {
		mt6332_upmu_set_rg_adcin_batsns_en(1);
		mt6332_upmu_set_rg_adcin_cs_en(1);
	}
		
	mt6332_upmu_set_rg_vbif28_on_ctrl(1);
	mt6332_upmu_set_rg_vbif28_en(1);
			
	//for tses_31 & 32
	mt6331_upmu_set_rg_vbuf_en(1);
	mt6332_upmu_set_rg_vbuf_en(1);
	
	// set average smaple number = 16
	mt6331_upmu_set_auxadc_avg_num_sel(0);
	mt6332_upmu_set_auxadc_avg_num_sel(0);
	mt6331_upmu_set_auxadc_avg_num_small(3);
	mt6332_upmu_set_auxadc_avg_num_small(3);
	
	xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "****[pmic_auxadc_init] DONE\n");
}

kal_uint32  pmic_is_auxadc_ready(kal_int32 channel_num, upmu_adc_chip_list_enum chip_num, upmu_adc_user_list_enum user_num)
{
#if 1	
	kal_uint32 ret=0;
	kal_uint32 int_status_val_0=0;
	unsigned long flags;
	
	spin_lock_irqsave(&pmic_adc_lock, flags);
	if ( chip_num == MT6331_CHIP ) {
		if ( channel_num == 8 && user_num == GPS ) {
			ret=pmic_read_interface_nolock(MT6331_AUXADC_ADC14,(&int_status_val_0),0x8000,0x0);
		} else if ( channel_num == 7 && user_num == MD ) {
			ret=pmic_read_interface_nolock(MT6331_AUXADC_ADC15,(&int_status_val_0),0x8000,0x0);
		} else if ( channel_num == 7 && user_num == AP ) {
			ret=pmic_read_interface_nolock(MT6331_AUXADC_ADC16,(&int_status_val_0),0x8000,0x0);
		} else if ( channel_num == 4 && user_num == MD ) {
			ret=pmic_read_interface_nolock(MT6331_AUXADC_ADC17,(&int_status_val_0),0x8000,0x0);
		} else {
			ret=pmic_read_interface_nolock(MT6331_AUXADC_ADC0 + (channel_num * 2),(&int_status_val_0),0x8000,0x0);
		}
	} else if( chip_num == MT6332_CHIP ) {
		if ( channel_num == 4 && user_num == MD ) {
			ret=pmic_read_interface_nolock(MT6332_AUXADC_ADC17,(&int_status_val_0),0x8000,0x0);
		} else if ( channel_num == 7 && user_num == AP ) {
			ret=pmic_read_interface_nolock(MT6332_AUXADC_ADC16,(&int_status_val_0),0x8000,0x0);
		} else {
			ret=pmic_read_interface_nolock(MT6332_AUXADC_ADC0 + (channel_num * 2),(&int_status_val_0),0x8000,0x0);
		}
	}
	spin_unlock_irqrestore(&pmic_adc_lock, flags);
	
	return int_status_val_0 >> 15;
#else
    return 0;
#endif    
}

kal_uint32  pmic_get_adc_output(kal_int32 channel_num, upmu_adc_chip_list_enum chip_num, upmu_adc_user_list_enum user_num)
{
#if 1	
	kal_uint32 ret=0;
	kal_uint32 int_status_val_0=0;
	unsigned long flags;
	
	spin_lock_irqsave(&pmic_adc_lock, flags);
	if ( chip_num == MT6331_CHIP ) {
		if ( channel_num == 8 && user_num == GPS ) {
			ret=pmic_read_interface_nolock(MT6331_AUXADC_ADC14,(&int_status_val_0),0x7FFF,0x0);
		} else if ( channel_num == 7 && user_num == MD ) {
			ret=pmic_read_interface_nolock(MT6331_AUXADC_ADC15,(&int_status_val_0),0x7FFF,0x0);
		} else if ( channel_num == 7 && user_num == AP ) {
			ret=pmic_read_interface_nolock(MT6331_AUXADC_ADC16,(&int_status_val_0),0x7FFF,0x0);
		} else if ( channel_num == 4 && user_num == MD ) {
			ret=pmic_read_interface_nolock(MT6331_AUXADC_ADC17,(&int_status_val_0),0x7FFF,0x0);
		} else {
			ret=pmic_read_interface_nolock(MT6331_AUXADC_ADC0 + (channel_num * 2),(&int_status_val_0),0x0FFF,0x0);
		}
	} else if ( chip_num == MT6332_CHIP ) {
		if ( channel_num == 4 && user_num == MD ) {
			ret=pmic_read_interface_nolock(MT6332_AUXADC_ADC17,(&int_status_val_0),0x0FFF,0x0);
		} else if ( channel_num == 7  && user_num == AP ) {
			ret=pmic_read_interface_nolock(MT6332_AUXADC_ADC16,(&int_status_val_0),0x7FFF,0x0);
		} else {
			ret=pmic_read_interface_nolock(MT6332_AUXADC_ADC0 + (channel_num * 2),(&int_status_val_0),0x0FFF,0x0);
		}
	}
	spin_unlock_irqrestore(&pmic_adc_lock, flags);
	return int_status_val_0;
#else
    return 0;
#endif    
}

static kal_uint32 PMIC_IMM_RequestAuxadcChannel(kal_int32 channel_num, upmu_adc_chip_list_enum chip_num, upmu_adc_user_list_enum user_num, int mode)
{
#if 1
	unsigned long flags;
	kal_uint32 ret = 0;
	
	if (user_num >= ADC_USER_MAX || chip_num >= ADC_CHIP_MAX)
		return 0;
		
	if ( chip_num == MT6331_CHIP ) {
		if (user_num == AP) {
			spin_lock_irqsave(&pmic_adc_lock, flags);
			if (mode == 0) {
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6331_AUXADC_RQST0_CLR), 0x1, 0x1, channel_num);
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6331_AUXADC_RQST0_CLR), 0x0, 0x1, channel_num);
			} else if (mode == 1) {
		        		ret=pmic_config_interface_nolock( (kal_uint32)(MT6331_AUXADC_RQST0_SET), 0x1, 0x1, channel_num);
			} else if (mode == 2){
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6331_AUXADC_RQST0), 0x1, 0x1, channel_num);	
			}
			spin_unlock_irqrestore(&pmic_adc_lock, flags);
		} else if ( (user_num == MD && ( channel_num == 4 || channel_num == 7 )) || (user_num == GPS && channel_num == 8) ) {
			spin_lock_irqsave(&pmic_adc_lock, flags);
			if (mode == 0) {
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6331_AUXADC_RQST1_CLR), 0x1, 0x1, channel_num);
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6331_AUXADC_RQST1_CLR), 0x0, 0x1, channel_num);
			} else if (mode == 1) {
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6331_AUXADC_RQST1_SET), 0x1, 0x1, channel_num);
			} else if (mode == 2){
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6331_AUXADC_RQST1), 0x1, 0x1, channel_num);
		        }
			spin_unlock_irqrestore(&pmic_adc_lock, flags);
		} else {
			return 0;	
		}
	} else if ( chip_num == MT6332_CHIP ) {
		if (user_num == AP) {
			spin_lock_irqsave(&pmic_adc_lock, flags);
			if (mode == 0) {
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6332_AUXADC_RQST0_CLR), 0x1, 0x1, channel_num);
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6332_AUXADC_RQST0_CLR), 0x0, 0x1, channel_num);
			} else if (mode == 1) {
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6332_AUXADC_RQST0_SET), 0x1, 0x1, channel_num);
			} else if (mode == 2){
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6332_AUXADC_RQST0), 0x1, 0x1, channel_num);		
			}
			spin_unlock_irqrestore(&pmic_adc_lock, flags);
		} else if ( user_num == MD &&  channel_num == 4 ) {
			spin_lock_irqsave(&pmic_adc_lock, flags);
			if (mode == 0) {
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6332_AUXADC_RQST1_CLR), 0x1, 0x1, channel_num);
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6332_AUXADC_RQST1_CLR), 0x0, 0x1, channel_num);
			} else if (mode == 1) {
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6332_AUXADC_RQST1_SET), 0x1, 0x1, channel_num);
			} else if (mode == 2){
				ret=pmic_config_interface_nolock( (kal_uint32)(MT6332_AUXADC_RQST1), 0x1, 0x1, channel_num);	
			}
			spin_unlock_irqrestore(&pmic_adc_lock, flags);
		} else {
			return 0;	
		}
	}
	return ret;
#else
	return 0;
#endif   
}

int PMIC_IMM_GetChannelNumber(upmu_adc_chl_list_enum dwChannel)
{
	kal_int32 channel_num;
	channel_num = (dwChannel & (AUXADC_CHANNEL_MASK << AUXADC_CHANNEL_SHIFT)) >> AUXADC_CHANNEL_SHIFT ;
	
	return channel_num;	
}

upmu_adc_chip_list_enum PMIC_IMM_GetChipNumber(upmu_adc_chl_list_enum dwChannel)
{
	upmu_adc_chip_list_enum chip_num;
	chip_num = (upmu_adc_chip_list_enum)(dwChannel & (AUXADC_CHIP_MASK << AUXADC_CHIP_SHIFT)) >> AUXADC_CHIP_SHIFT ;
	
	return chip_num;	
}

upmu_adc_user_list_enum PMIC_IMM_GetUserNumber(upmu_adc_chl_list_enum dwChannel)
{
	upmu_adc_user_list_enum user_num;
	user_num = (upmu_adc_user_list_enum)(dwChannel & (AUXADC_USER_MASK << AUXADC_USER_SHIFT)) >> AUXADC_USER_SHIFT ;
	
	return user_num;		
}

int PMIC_IMM_get_adc_channel_num(char *adc_name, int len)
{
	if (strcmp(adc_name, "ADC_FDD_Rf_Params_Dynamic_Custom") == 0) {
		return ADC_ADCVIN0_AP;
	}
	
	return -1;
}

//==============================================================================
// PMIC-AUXADC 
//==============================================================================
int PMIC_IMM_GetOneChannelValue(upmu_adc_chl_list_enum dwChannel, int deCount, int trimd)
{
#if 1
	kal_int32 ret_data;    
	kal_int32 count=0;
	kal_int32 u4Sample_times = 0;
	kal_int32 u4channel=0;    
	kal_int32 adc_result_temp=0;
	kal_int32 r_val_temp=0;   
	kal_int32 adc_result=0;   
	kal_int32 channel_num;
	upmu_adc_chip_list_enum chip_num;
	upmu_adc_user_list_enum user_num;

	/*
		MT6331
		0 : NA
		1 : NA
		2 : NA 
		3 : NA
		4 : TSENSE_PMIC_31
		5 : VACCDET
		6 : VISMPS_1
		7 : AUXADCVIN0
		8 : NA    
		9 : HP
		11-15: Shared
		
		MT6332
		0 : BATSNS
		1 : ISENSE
		2 : VBIF 
		3 : BATON
		4 : TSENSE_PMIC_32
		5 : VCHRIN
		6 : VISMPS_2
		7 : VUSB/ VADAPTOR
		8 : M3_REF    
		9 : SPK_ISENSE
		10: SPK_THR_V
		11: SPK_THR_I
		12-15: shared 
	*/
	if(mt6332_upmu_get_swcid()>=PMIC6332_E2_CID_CODE) {
		wake_lock(&pmicAuxadc_irq_lock);
	}
	
	mutex_lock(&pmic_auxadc_mutex);
	
	channel_num 	= PMIC_IMM_GetChannelNumber(dwChannel);
	chip_num 	= PMIC_IMM_GetChipNumber(dwChannel);
	user_num 	= PMIC_IMM_GetUserNumber(dwChannel);
	
	if (channel_num == 7 && chip_num == MT6332_CHIP) {	
		mt6332_upmu_set_rg_chrwdt_en(0);
		if(mt6332_upmu_get_swcid()==PMIC6332_E1_CID_CODE) {
			mt6332_upmu_set_rg_chrwdt_en(0);
			pmic_config_interface( (kal_uint32)(MT6332_CHR_CON14), 0x1, MT6332_PMIC_RG_AUXADC_USB_DET_MASK, MT6332_PMIC_RG_AUXADC_USB_DET_SHIFT);
			pmic_config_interface( (kal_uint32)(MT6332_CHR_CON14), 0x0, MT6332_PMIC_RG_AUXADC_DCIN_DET_MASK, MT6332_PMIC_RG_AUXADC_DCIN_DET_SHIFT);
		} else if (mt6332_upmu_get_swcid()>=PMIC6332_E2_CID_CODE) {
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[PMIC_IMM_GetOneChannelValue] usb E2 \n");
			pmic_config_interface( (kal_uint32)(MT6332_CHR_CON22), 0x1, MT6332_PMIC_RG_AUXADC_USB_DET_MASK, MT6332_PMIC_RG_AUXADC_USB_DET_SHIFT);
			pmic_config_interface( (kal_uint32)(MT6332_CHR_CON22), 0x0, MT6332_PMIC_RG_AUXADC_DCIN_DET_MASK, MT6332_PMIC_RG_AUXADC_DCIN_DET_SHIFT);
		}
	} else  if ( (channel_num == 7) && ((dwChannel & (0x20)) != 0) ){
		chip_num = 1;
		if(mt6332_upmu_get_swcid()==PMIC6332_E1_CID_CODE) {
			mt6332_upmu_set_rg_chrwdt_en(0);
			pmic_config_interface( (kal_uint32)(MT6332_CHR_CON14), 0x0, MT6332_PMIC_RG_AUXADC_USB_DET_MASK, MT6332_PMIC_RG_AUXADC_USB_DET_SHIFT);
			pmic_config_interface( (kal_uint32)(MT6332_CHR_CON14), 0x1, MT6332_PMIC_RG_AUXADC_DCIN_DET_MASK, MT6332_PMIC_RG_AUXADC_DCIN_DET_SHIFT);
		}  else if (mt6332_upmu_get_swcid()>=PMIC6332_E2_CID_CODE) {
			
			pmic_config_interface( (kal_uint32)(MT6332_CHR_CON22), 0x0, MT6332_PMIC_RG_AUXADC_USB_DET_MASK, MT6332_PMIC_RG_AUXADC_USB_DET_SHIFT);
			pmic_config_interface( (kal_uint32)(MT6332_CHR_CON22), 0x1, MT6332_PMIC_RG_AUXADC_DCIN_DET_MASK, MT6332_PMIC_RG_AUXADC_DCIN_DET_SHIFT);
		}
	}

	if (user_num == GPS && chip_num == MT6331_CHIP) {
		mt6331_upmu_set_auxadc_ck_aon(1);
		mt6331_upmu_set_auxadc_ck_aon_md(0);
		mt6331_upmu_set_auxadc_ck_aon_gps(0);
		mt6331_upmu_set_auxadc_data_reuse_sel(0);
	} else {
		mt6331_upmu_set_auxadc_ck_aon(0);
		mt6331_upmu_set_auxadc_ck_aon_md(0);
		mt6331_upmu_set_auxadc_ck_aon_gps(0);
		mt6331_upmu_set_auxadc_data_reuse_sel(3);	
	}
	do
	{
		count=0;
		ret_data=0;


#if 1		
		PMIC_IMM_RequestAuxadcChannel(channel_num, chip_num, user_num, CLEAR_REQ);		// clear
		PMIC_IMM_RequestAuxadcChannel(channel_num, chip_num, user_num, SET_REQ);		// set
#else
		PMIC_IMM_RequestAuxadcChannel(channel_num, chip_num, user_num, ONLY_REQ);		// request only
#endif
		udelay(1);
	        
	        
	
	        switch(dwChannel){         
	            case ADC_TSENSE_31_AP:	        
	            case ADC_VACCDET_AP:    
	            case ADC_VISMPS_1_AP:                 
	            case ADC_ADCVIN0_AP:    
	            case ADC_HP_AP:    
	            case ADC_BATSNS_AP:    
	            case ADC_ISENSE_AP:    
	            case ADC_VBIF_AP:    	                
	            case ADC_BATON_AP:                   
	            case ADC_TSENSE_32_AP:    
	            case ADC_VCHRIN_AP:  
	            case ADC_VISMPS_2_AP:
		    case ADC_VUSB_AP:  
		    case ADC_M3_REF_AP:
		    case ADC_SPK_ISENSE_AP:    
		    case ADC_SPK_THR_V_AP:  
		    case ADC_SPK_THR_I_AP: 
		    case ADC_VADAPTOR_AP: 
		    case ADC_TSENSE_31_MD:
		    case ADC_ADCVIN0_MD:
		    case ADC_ADCVIN0_GPS:
		    case ADC_TSENSE_32_MD:
	                while( pmic_is_auxadc_ready(channel_num, chip_num, user_num) != 1 )
	                {
		            udelay(1);
		            if( (count++) > count_time_out)
		            {
				xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[IMM_GetOneChannelValue_PMIC] (%d) Time out!\n", dwChannel);
				break;
		            }            
	                }
	                ret_data = pmic_get_adc_output(channel_num, chip_num, user_num);                
	                break; 
		    default:
		        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUXADC] Invalid channel value(%d,%d)\n", dwChannel, trimd);
		        if(mt6332_upmu_get_swcid()>=PMIC6332_E2_CID_CODE) {
		        	wake_unlock(&pmicAuxadc_irq_lock);
			}
		        return -1;
		}
		
		PMIC_IMM_RequestAuxadcChannel(channel_num, chip_num, user_num, CLEAR_REQ);		// clear
		
	        u4channel += ret_data;
	
	        u4Sample_times++;
	
	            //debug
		if (Enable_BATDRV_LOG == 2) {
			xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUXADC] u4channel[%d]=%d.\n", 
				dwChannel, ret_data);
		}
	        
	}while (u4Sample_times < deCount);
	mutex_unlock(&pmic_auxadc_mutex);
	/* Value averaging  */ 
	adc_result_temp = u4channel / deCount;
	
	switch(dwChannel){         
	case ADC_TSENSE_31_AP:                
	    r_val_temp = 1;           
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;
	case ADC_VACCDET_AP:    
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;
	case ADC_VISMPS_1_AP:    
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;
	case ADC_ADCVIN0_AP:    
		xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUXADC]ADC_ADCVIN0_AP\n"); 
		adc_result = adc_result_temp;
		break;
	case ADC_HP_AP:    
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;
	case ADC_BATSNS_AP:    
	    r_val_temp = 2;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;
	case ADC_ISENSE_AP:    
	    r_val_temp = 2;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;
	case ADC_VBIF_AP:    
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;    
	case ADC_BATON_AP:    
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;                
	case ADC_TSENSE_32_AP:  
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;    
	case ADC_VCHRIN_AP:  
	    r_val_temp = 10;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;  
	case ADC_VISMPS_2_AP:  
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;
	case ADC_VUSB_AP: 
	    r_val_temp = 10;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE_CH7;
	    break;    
	case ADC_M3_REF_AP:
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;  
	case ADC_SPK_ISENSE_AP:    
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;
	case ADC_SPK_THR_V_AP:  
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;
	case ADC_SPK_THR_I_AP:  
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;
	case ADC_VADAPTOR_AP:
	    r_val_temp = 10;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE_CH7;
	    break;  
	case ADC_TSENSE_31_MD:  
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;  
	case ADC_ADCVIN0_MD:
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE_CH7;
	    break;  
	case ADC_ADCVIN0_GPS:
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE_CH7;
	    break;  
	case ADC_TSENSE_32_MD:        
	    r_val_temp = 1;
	    adc_result = (adc_result_temp*r_val_temp*VOLTAGE_FULL_RANGE)/ADC_PRECISE;
	    break;  
	default:
	    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUXADC] Invalid channel value(%d,%d)\n", dwChannel, trimd);
	    if(mt6332_upmu_get_swcid()>=PMIC6332_E2_CID_CODE) {
	    	wake_unlock(&pmicAuxadc_irq_lock);
	    }
	    return -1;
	}
	if (Enable_BATDRV_LOG == 2) {
		xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[AUXADC] adc_result_temp=%d, adc_result=%d, r_val_temp=%d.\n", 
			adc_result_temp, adc_result, r_val_temp);
	}
	
	if (channel_num == 7 && chip_num == MT6332_CHIP) {
		if(mt6332_upmu_get_swcid()==PMIC6332_E1_CID_CODE) {
			mt6332_upmu_set_rg_chrwdt_en(1);
		}
	}

	if(mt6332_upmu_get_swcid()>=PMIC6332_E2_CID_CODE) {
		wake_unlock(&pmicAuxadc_irq_lock);
	}
	return adc_result;
#else
	return 0;
#endif   
}


