/*
** $Id: lte_hif_sdio_hal.c,v 1.14.4.1 2011/05/30 mtk02190 Exp $
*/

/*! \file   "lte_hif_sdio_hal.c"
    \brief  This file includes the operation of sdio interface

    Detail description.
*/



 /**
 *  @file           lte_hif_sdio_hal.c
 *  @author         Bryant Lu
 *  @description    HAL interface for mt72xx SDIO HIF 
 *  @date           2011.05.30
 */


#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio.h>

#include <linux/aee.h>

#include "lte_dev_test.h"
#include "eemcs_kal.h"

//#define PADPATER_TO_PSDIOFUNC(_pAdapter) ((struct sdio_func *)(((PHDRV_ADAPTER)_pAdapter)->pBusFunction))
struct sdio_func *lte_sdio_func = NULL;
extern struct wake_lock sdio_wake_lock;

#ifdef USER_BUILD_KERNEL
unsigned int assert_when_msdc_fail = 0;
#else
unsigned int assert_when_msdc_fail = 1;
#endif

//KAL_UINT32 log_sdio_read_time = 0;
//KAL_UINT32 log_sdio_write_time = 0;

#ifdef USER_BUILD_KERNEL

MTLTE_HAL_TO_HIF_CALLBACK lte_sdio_cb_msdc_err = NULL;

int mtlte_hal_register_MSDC_ERR_callback(MTLTE_HAL_TO_HIF_CALLBACK func_ptr) 
{
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;
    
	lte_sdio_cb_msdc_err = func_ptr ;
	
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return KAL_SUCCESS ; 
}
#endif

int sdio_func0_wr(unsigned int u4Register,unsigned char *pValue,  unsigned int Length)
{
    int i = 0 ;
    int err_ret =0;
    
    if(lte_sdio_func==NULL){
        KAL_RAWPRINT(("[ERR] Access LTE SDIO device while there is no device connected!!! \n"));
        return -1;
    }
    
    KAL_ASSERT((pValue!=NULL)) ;	
    
    for (i=0 ; i<Length ; i++ ){
        // only reserved for vendor address is valid , that is u4Register = 0xF0~0xFF
		
		wake_lock(&sdio_wake_lock);
        sdio_claim_host(lte_sdio_func);	
        sdio_f0_writeb(lte_sdio_func,*(pValue+i) ,(u4Register+i), &err_ret) ;
        sdio_release_host(lte_sdio_func);
		wake_unlock(&sdio_wake_lock);
       
        if (err_ret){
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[ERR] function 0 write fail : addr : 0x%x , size : %d, err_ret: 0x%x\n", u4Register, Length, err_ret )) ;
            if(assert_when_msdc_fail) KAL_ASSERT(0);
            else{
#ifdef USER_BUILD_KERNEL
                if(NULL != lte_sdio_cb_msdc_err){
                    KAL_DBGPRINT(KAL, DBG_ERROR, ("[MTLTE][ERR] MSDC access Fail!!  Perform Slient Reset by using WDT reset... \n")) ;
                    lte_sdio_cb_msdc_err(0);
                }
#else
                char error_srt[64] = {0};
                sprintf(error_srt, "\n1.MD maybe already CRASH \n2.MSDC setting maybe not good ");
                aee_kernel_warning("[EEMCS] SDIO access FAIL!! \n  DO NOT access MD anymore!! ", error_srt);
                while(1){KAL_SLEEP_MSEC(1000);}
#endif                
            }
        }else{
	        KAL_DBGPRINT(KAL, DBG_INFO, ("[INFO] function 0 write - addr : 0x%x , value: 0x%x\n", u4Register+i, *(pValue+i))) ;	
	    }
    }
    
    return(err_ret) ? err_ret : KAL_SUCCESS ;
}
EXPORT_SYMBOL(sdio_func0_wr);

int sdio_func0_rd(  unsigned int u4Register,unsigned char *pValue,  unsigned int Length)
{
    int i = 0 ;
    int err_ret =0;
    
    if(lte_sdio_func==NULL){
        KAL_RAWPRINT(("[ERR] Access LTE SDIO device while there is no device connected!!! \n"));
        return -1;
    }
    
    KAL_ASSERT((pValue!=NULL)) ;
    
    for (i=0 ; i<Length ; i++ ){
       
		wake_lock(&sdio_wake_lock);
        sdio_claim_host(lte_sdio_func);	
        *(pValue+i) = sdio_f0_readb(lte_sdio_func, (u4Register+i), &err_ret) ;
        sdio_release_host(lte_sdio_func);
		wake_unlock(&sdio_wake_lock);
     
        if (err_ret){
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[ERR]function 0 read fail : addr : 0x%x , size : %d, err_ret: 0x%x\n", u4Register, Length, err_ret)) ;
            if(assert_when_msdc_fail) KAL_ASSERT(0);
            else{
#ifdef USER_BUILD_KERNEL
                if(NULL != lte_sdio_cb_msdc_err){
                    KAL_DBGPRINT(KAL, DBG_ERROR, ("[MTLTE][ERR] MSDC access Fail!!  Perform Slient Reset by using WDT reset... \n")) ;
                    lte_sdio_cb_msdc_err(0);
                }
#else                
                char error_srt[64] = {0};
                sprintf(error_srt, "\n1.MD maybe already CRASH \n2.MSDC setting maybe not good ");
                aee_kernel_warning("[EEMCS] SDIO access FAIL!! \n  DO NOT access MD anymore!! ", error_srt);
                while(1){KAL_SLEEP_MSEC(1000);}
#endif
            }
        }else{
	        KAL_DBGPRINT(KAL, DBG_INFO, ("[INFO] function 0 read - addr : 0x%x , value: 0x%x\n", u4Register+i, *(pValue+i))) ;	
	    }
    }
    
    return(err_ret) ? err_ret : KAL_SUCCESS ;	
}
EXPORT_SYMBOL(sdio_func0_rd);

void dump_gpio(void)
{
	KAL_DBGPRINT(KAL, DBG_ERROR, ("GPIO_LTE_POWER_PIN: mode=%d, in=%d, out=%d, dir=%d\n", mt_get_gpio_mode(GPIO_LTE_POWER_PIN), 
				mt_get_gpio_in(GPIO_LTE_POWER_PIN),
				mt_get_gpio_out(GPIO_LTE_POWER_PIN),
				mt_get_gpio_dir(GPIO_LTE_POWER_PIN)));

	KAL_DBGPRINT(KAL, DBG_ERROR, ("GPIO_LTE_RESET_PIN: mode=%d, in=%d, out=%d, dir=%d\n", mt_get_gpio_mode(GPIO_LTE_RESET_PIN), 
				mt_get_gpio_in(GPIO_LTE_RESET_PIN),
				mt_get_gpio_out(GPIO_LTE_RESET_PIN),
				mt_get_gpio_dir(GPIO_LTE_RESET_PIN)));
}

int sdio_func1_wr(unsigned int u4Register,void *pBuffer,  unsigned int Length)
{
    int ret=0, i;	
    
    if(lte_sdio_func==NULL){
        KAL_RAWPRINT(("[ERR] Access LTE SDIO device while there is no device connected!!! \n"));
        return -1;
    }
    
    KAL_ASSERT((pBuffer!=NULL)) ;

	wake_lock(&sdio_wake_lock);
    sdio_claim_host(lte_sdio_func);	
    ret = sdio_writesb(lte_sdio_func, u4Register, pBuffer, Length);
    sdio_release_host(lte_sdio_func);
	wake_unlock(&sdio_wake_lock);
   
    if (ret){
		dump_gpio();

        KAL_DBGPRINT(KAL, DBG_ERROR, ("[ERR] function 1 write fail : addr : 0x%x , size : %d, err_ret: 0x%x\n", u4Register, Length, ret)) ;
        KAL_DBGPRINT(KAL, DBG_ERROR, ("Error packet content = ")) ;
        for(i=0; i<Length; i++){
            KAL_DBGPRINT(KAL, DBG_ERROR, ("%x ", *(unsigned char *)(pBuffer+i) )) ;
            
            if(i>32){
                KAL_DBGPRINT(KAL, DBG_ERROR,("packet is big, only print 32byte... " ));
                break;
            }
        }
        KAL_DBGPRINT(KAL, DBG_ERROR, (" \n ")) ;
        if(assert_when_msdc_fail) KAL_ASSERT(0);
        else{
#ifdef USER_BUILD_KERNEL
                if(NULL != lte_sdio_cb_msdc_err){
                    KAL_DBGPRINT(KAL, DBG_ERROR, ("[MTLTE][ERR] MSDC access Fail!!  Perform Slient Reset by using WDT reset... \n")) ;
                    lte_sdio_cb_msdc_err(0);
                }
#else

                char error_srt[64] = {0};
                sprintf(error_srt, "\n1.MD maybe already CRASH \n2.MSDC setting maybe not good ");
                aee_kernel_warning("[EEMCS] SDIO access FAIL!! \n  DO NOT access MD anymore!! ", error_srt);
                while(1){KAL_SLEEP_MSEC(1000);}
#endif                
            }
        
    }else{
	    KAL_DBGPRINT(KAL, DBG_INFO, ("[INFO] function 1 write - addr : 0x%x , Length: %d, value: 0x%08x\n", u4Register, Length, *((unsigned int*)pBuffer))) ;	
	}
    
    return(ret) ? ret : KAL_SUCCESS ;	
}

#if 0
extern KAL_UINT32 abnormal_int_count;
#endif

EXPORT_SYMBOL(sdio_func1_wr);

int sdio_func1_rd(unsigned int u4Register,void *pBuffer,  unsigned int Length)
{
    int ret=0 ;

    if(lte_sdio_func==NULL){
        KAL_RAWPRINT(("[ERR] Access LTE SDIO device while there is no device connected!!! \n"));
        return -1;
    }
    
    KAL_ASSERT((pBuffer!=NULL)) ;

	wake_lock(&sdio_wake_lock);
    sdio_claim_host(lte_sdio_func);	
    ret = sdio_readsb(lte_sdio_func, pBuffer, u4Register, Length);
    sdio_release_host(lte_sdio_func);
	wake_unlock(&sdio_wake_lock);
    
    if (ret){
		dump_gpio();

        KAL_DBGPRINT(KAL, DBG_ERROR, ("[ERR] function 1 read fail : addr : 0x%x , size : %d, err_ret: 0x%x\n", u4Register, Length, ret)) ;		
        if(assert_when_msdc_fail) KAL_ASSERT(0);
        else{
#ifdef USER_BUILD_KERNEL
                if(NULL != lte_sdio_cb_msdc_err){
                    KAL_DBGPRINT(KAL, DBG_ERROR, ("[MTLTE][ERR] MSDC access Fail!!  Perform Slient Reset by using WDT reset... \n")) ;
                    lte_sdio_cb_msdc_err(0);
                }
#else            
                char error_srt[64] = {0};
                sprintf(error_srt, "\n1.MD maybe already CRASH \n2.MSDC setting maybe not good ");
                aee_kernel_warning("[EEMCS] SDIO access FAIL!! \n  DO NOT access MD anymore!! ", error_srt);
                while(1){KAL_SLEEP_MSEC(1000);}
#endif
            }
    }else{
	    KAL_DBGPRINT(KAL, DBG_INFO, ("[INFO] function 1 read - addr : 0x%x , Length: %d, value: 0x%08x\n", u4Register, Length, *((unsigned int *)pBuffer))) ;	
	}

#if 0
    int i = 0;
    unsigned int *print_p;
    print_p = (unsigned int *)pBuffer;

    if(abnormal_int_count > 0){
        KAL_DBGPRINT(KAL, DBG_ERROR, ("[ERR] Read when abnormal occurs : addr : 0x%x , size : %d \n", u4Register, Length)) ;

        for(i=0; i<Length; i+=16){
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[ERR] 0x%x, 0x%x, 0x%x, 0x%x  \n", *(print_p+(i>>2)), *(print_p+(i>>2)+1), *(print_p+(i>>2)+2), *(print_p+(i>>2)+3))) ;
        }
    }
#endif

    return(ret) ? ret : KAL_SUCCESS ;
}
EXPORT_SYMBOL(sdio_func1_rd);




int sdio_func1_wr_cmd52(unsigned int u4Register,unsigned char *pValue,  unsigned int Length)
{
    int i = 0 ;
    int err_ret ;
    
    if(lte_sdio_func==NULL){
        KAL_RAWPRINT(("[ERR] Access LTE SDIO device while there is no device connected!!! \n"));
        return -1;
    }
    
    KAL_ASSERT((pValue!=NULL)) ;
    
    for (i=0 ; i<Length ; i++ ){
        // only reserved for vendor address is valid , that is u4Register = 0xF0~0xFF
        sdio_claim_host(lte_sdio_func);	
        sdio_writeb(lte_sdio_func,*(pValue+i) ,(u4Register+i), &err_ret) ;
        sdio_release_host(lte_sdio_func);
        if (err_ret){
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[ERR] function 1 write by cmd52 fail : addr : 0x%x , size : %d, err_ret: 0x%x\n", u4Register, Length, err_ret )) ;	
        }else{
	        KAL_DBGPRINT(KAL, DBG_INFO, ("[INFO] function 1 write by cmd52 - addr : 0x%x , value: 0x%x\n", u4Register+i, *(pValue+i))) ;	
	    }
    }
    
    return(err_ret) ? err_ret : KAL_SUCCESS ;
}
EXPORT_SYMBOL(sdio_func1_wr_cmd52);


int sdio_func1_rd_cmd52(  unsigned int u4Register,unsigned char *pValue,  unsigned int Length)
{
    int i = 0 ;
    int err_ret ;
    
    if(lte_sdio_func==NULL){
        KAL_RAWPRINT(("[ERR] Access LTE SDIO device while there is no device connected!!! \n"));
        return -1;
    }
    
    KAL_ASSERT((pValue!=NULL)) ;
    
    for (i=0 ; i<Length ; i++ ){
        sdio_claim_host(lte_sdio_func);	
        *(pValue+i) = sdio_readb(lte_sdio_func, (u4Register+i), &err_ret) ;
        sdio_release_host(lte_sdio_func);
        if (err_ret){
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[ERR]function 1 read by cmd52 fail : addr : 0x%x , size : %d, err_ret: 0x%x\n", u4Register, Length, err_ret)) ;
        }else{
	        KAL_DBGPRINT(KAL, DBG_INFO, ("[INFO] function 1 read by cmd52 - addr : 0x%x , value: 0x%x\n", u4Register+i, *(pValue+i))) ;	
	    }
    }
    
    return(err_ret) ? err_ret : KAL_SUCCESS ;	
}
EXPORT_SYMBOL(sdio_func1_rd_cmd52);




int sdio_property_set(HDRV_SDBUS_PROPERTY PropFunc, unsigned char *pData,  unsigned int size)
{
    unsigned char set_value ;  
    unsigned short blocksize ;
    int ret = 0;
    
    KAL_DBGPRINT(KAL, DBG_TRACE,("[FNC] =======> %s \n", KAL_FUNC_NAME)) ;

    if(lte_sdio_func==NULL){
        KAL_RAWPRINT(("[ERR] Access LTE SDIO device while there is no device connected!!! \n"));
        return -1;
    }
    
    KAL_ASSERT((pData!=NULL)) ;
    	
    if (PropFunc == HDRV_SDBUS_BUS_WIDTH) {
    	if ((*pData != HDRV_SDBUS_BUSWIDTH_1BIT) && (*pData != HDRV_SDBUS_BUSWIDTH_4BIT)){
    		KAL_DBGPRINT(KAL, DBG_WARN, ("[WARN] the Bus width set should be only 1 bit or 4 bit\n")) ;		
			return KAL_FAIL ;
    	}
    	ret = sdio_func0_rd(SDIO_FN0_CCCR_BICR, &set_value, sizeof(unsigned char)) ;
	    if(ret){
	        KAL_DBGPRINT(KAL, DBG_ERROR, ("[ERR] function 0 read CCR BICR fail : addr : 0x%x , err_ret: 0x%x\n", SDIO_FN0_CCCR_BICR, ret)) ;	        
	    }

		if (*pData == HDRV_SDBUS_BUSWIDTH_4BIT){
			set_value |= HDRV_SDBUS_BUSWIDTH_4BIT ;
		}else{
			set_value &= (~HDRV_SDBUS_BUSWIDTH_BITMASK) ;
		}
	    
	    ret = sdio_func0_wr(SDIO_FN0_CCCR_BICR, &set_value, sizeof(unsigned char)) ;
        if(ret){
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[ERR] function 0 set bus width fail : addr : 0x%x , err_ret: 0x%x\n", SDIO_FN0_CCCR_BICR, ret) ) ;
        }
	    KAL_DBGPRINT(KAL, DBG_INFO , ("[INFO] set bus width to %d\n", *pData )) ;														
    } else if (PropFunc == HDRV_SDBUS_FUNCTION_BLOCK_LENGTH) {
        blocksize = *((unsigned short*)pData) ;
        KAL_DBGPRINT(KAL, DBG_INFO , ("[INFO] set block size to %d, param size is %d\n", blocksize , size)) ;
     
        sdio_claim_host(lte_sdio_func);	
        ret = sdio_set_block_size(lte_sdio_func, blocksize);
        sdio_release_host(lte_sdio_func);
       
        if (ret){
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[ERR] function 0 set block size fail : err_ret: 0x%x\n", ret )) ;
        }
    }	
    
    KAL_DBGPRINT(KAL, DBG_TRACE,("[FNC] <======= %s \n", KAL_FUNC_NAME)) ;
    
    return(ret) ? ret : KAL_SUCCESS ;
}
EXPORT_SYMBOL(sdio_property_set);

int sdio_property_get(HDRV_SDBUS_PROPERTY PropFunc, unsigned char *pData,  unsigned int size)
{		
    unsigned char	regVal8 = 0 ;
    unsigned char blksize[2] ;
    
    KAL_DBGPRINT(KAL, DBG_TRACE,("[FNC] =======> %s \n", KAL_FUNC_NAME)) ;
    
    if(lte_sdio_func==NULL){
        KAL_RAWPRINT(("[ERR] Access LTE SDIO device while there is no device connected!!! \n"));
        return -1;
    }
    
    KAL_ASSERT((pData!=NULL)) ;
    
    if (PropFunc == HDRV_SDBUS_BUS_WIDTH) {		
        sdio_func0_rd(SDIO_FN0_CCCR_BICR, &regVal8, sizeof(unsigned char)) ;
        *(unsigned char *)pData= (regVal8==0)? 1 : 4 ;
    } else if (PropFunc == HDRV_SDBUS_FUNCTION_BLOCK_LENGTH) {
        sdio_func0_rd(SDIO_FN0_CCCR_IOBSF1R, &blksize[0], sizeof(KAL_UINT16)) ;
        *pData = blksize[0] ;
        *(pData+1) = blksize[1] ;
        KAL_DBGPRINT(KAL, DBG_INFO , ("[INFO] get block size to %d, param size is %d\n", *((unsigned short *)blksize) , size)) ;
    }
    
    KAL_DBGPRINT(KAL, DBG_TRACE,("[FNC] <======= %s \n", KAL_FUNC_NAME)) ;
    
    return KAL_SUCCESS ;
}
EXPORT_SYMBOL(sdio_property_get);

int sdio_open_device(struct sdio_func *sdiofunc)
{
    int ret=0 ;
    KAL_DBGPRINT(KAL, DBG_TRACE,("[FNC] =======> %s \n", KAL_FUNC_NAME)) ;

    lte_sdio_func = sdiofunc ;
    
    /*	enable sdio function */
    sdio_claim_host(lte_sdio_func);
    ret = sdio_enable_func(lte_sdio_func);
    sdio_release_host(lte_sdio_func);   


    if (ret) {
        KAL_RAWPRINT(("[ERR] sdio_enable_func failed!\n")) ;
        return ret;
    }else{
        KAL_RAWPRINT(("[INFO] sdio_enable_func OK!\n")) ;
    }

    KAL_DBGPRINT(KAL, DBG_TRACE,("[FNC] <======= %s \n", KAL_FUNC_NAME)) ;
    return KAL_SUCCESS ;
    
}
EXPORT_SYMBOL(sdio_open_device);

int sdio_close_device(struct sdio_func *sdiofunc)
{		    
    int ret=0 ;
    
    KAL_DBGPRINT(KAL, DBG_TRACE,("[FNC] =======> %s \n", KAL_FUNC_NAME)) ;

    KAL_ASSERT(lte_sdio_func == sdiofunc) ;

    /*	disable sdio function */	    
    sdio_claim_host(lte_sdio_func);
    ret = sdio_disable_func(lte_sdio_func);
    sdio_release_host(lte_sdio_func);

    if (ret) {
        KAL_RAWPRINT(("[ERR] sdio_disable_func failed!\n")) ;
        return ret;
    }else{
        KAL_RAWPRINT(("[INFO] sdio_disable_func OK!\n")) ;
    }
	lte_sdio_func = NULL ;
	
    KAL_DBGPRINT(KAL, DBG_TRACE,("[FNC] <======= %s \n", KAL_FUNC_NAME)) ;
    return KAL_SUCCESS ;
}
EXPORT_SYMBOL(sdio_close_device);

