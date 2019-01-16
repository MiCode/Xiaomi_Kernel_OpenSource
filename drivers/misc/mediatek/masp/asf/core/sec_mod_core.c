/******************************************************************************
 *  INCLUDE LIBRARY
 ******************************************************************************/

/******************************************************************************
 *  INCLUDE LINUX HEADER
 ******************************************************************************/
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/ioctl.h>

/******************************************************************************
 *  INCLUDE LIBRARY
 ******************************************************************************/
#include <mach/mt_sec_hal.h>
#include "sec_boot_lib.h"
#include "masp_version.h"
#include "sec_ioctl.h"
#include "sec_osal_light.h"
#include "sec_nvram.h"

#define MOD                         "ASF"
#define HEVC_BLK_LEN                20480

#define CI_BLK_SIZE                 16
#define CI_BLK_ALIGN(len) ( ((len)+CI_BLK_SIZE-1) & ~(CI_BLK_SIZE-1) )

/**************************************************************************
 *  GLOBAL VARIABLES
 **************************************************************************/
typedef struct
{
    unsigned char buf[HEVC_BLK_LEN];
    unsigned int len;
} HEVC_BLK;
HEVC_BLK hevc_blk;


/**************************************************************************
 *  EXTERNAL VARIABLE
 **************************************************************************/
extern MtdPart                      mtd_part_map[];
extern bool                         bMsg;
extern struct semaphore             hacc_sem;

/**************************************************************************
 *  EXTERNAL FUNCTION
 **************************************************************************/
extern int sec_get_random_id(unsigned int *rid);
extern void sec_update_lks(unsigned char tr, unsigned char dn, unsigned char fb_ulk);
extern void sec_core_init (void);

static uint lks = 2;//if sec is not enabled, this param will not be updated
module_param(lks, uint, S_IRUSR/*|S_IWUSR|S_IWGRP*/|S_IRGRP|S_IROTH); /* r--r--r-- */
MODULE_PARM_DESC(lks, "A device lks parameter under sysfs (0=NL, 1=L, 2=NA)");

void sec_update_lks(unsigned char tr, unsigned char dn, unsigned char fb_ulk)
{
//SMSG(true,"[%s] zhaoyang sec_update_lks fb_ulk=%d sec_schip_enabled()=%d sec_boot_enabled()=%d\n",MOD,fb_ulk,sec_schip_enabled(),sec_boot_enabled());
printk( "[%s] zhaoyang sec_update_lks fb_ulk=%d sec_schip_enabled()=%d sec_boot_enabled()=%d\n",MOD,fb_ulk,sec_schip_enabled(),sec_boot_enabled());
//SMSG(true,"[%s] zhaoyang sec_update_lks tr=%d dn=%d\n",MOD,tr,dn);
printk( "[%s] zhaoyang sec_update_lks tr=%d dn=%d\n",MOD,tr,dn);
    if(fb_ulk)//FB
    {
        lks = 0;
    }
    else if(sec_schip_enabled())//SC
    {
        lks = 1;
    }
    else if(!sec_boot_enabled())//NSC
    {
        lks = 0;
    }
    else if(0 == tr && 2 == dn)//SWSEC
    {
        lks = 0;
    }
    else//SWSEC
    {
        lks = 1;
    }
}

//extern void osal_msleep(unsigned int msec);

/**************************************************************************
 *  SEC DRIVER IOCTL
 **************************************************************************/ 
long sec_core_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int err                     = 0;
    int ret                     = 0;
    unsigned int cipher_len     = 0;    
    unsigned int rid[4];
    unsigned char part_name[16];    
    META_CONTEXT meta_ctx;
    int status                  = 0;

    /* ---------------------------------- */
    /* IOCTL                              */
    /* ---------------------------------- */

    if (_IOC_TYPE(cmd) != SEC_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > SEC_IOC_MAXNR)
        return -ENOTTY;
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    if (_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    if (err) return -EFAULT;
    
    switch (cmd) {

        /* ---------------------------------- */
        /* get random id                      */
        /* ---------------------------------- */
        case SEC_GET_RANDOM_ID:
            SMSG(bMsg,"[%s] CMD - SEC_GET_RANDOM_ID\n",MOD);
            sec_get_random_id(&rid[0]);
            ret = osal_copy_to_user((void __user *)arg, (void *)&rid[0], sizeof(unsigned int) * 4);
            break;            

        /* ---------------------------------- */
        /* init boot info                     */
        /* ---------------------------------- */
        case SEC_BOOT_INIT:
            SMSG(bMsg,"[%s] CMD - SEC_BOOT_INIT\n",MOD);
            ret = masp_boot_init();
            sec_core_init();
            ret = osal_copy_to_user((void __user *)arg, (void *)&ret, sizeof(int));            
            break;


        /* ---------------------------------- */
        /* check if secure boot is enbaled    */
        /* ---------------------------------- */
        case SEC_BOOT_IS_ENABLED:
            SMSG(bMsg,"[%s] CMD - SEC_BOOT_IS_ENABLED\n",MOD);
            ret = sec_boot_enabled();
            ret = osal_copy_to_user((void __user *)arg, (void *)&ret, sizeof(int));            
            break;

        /* ---------------------------------- */
        /* encrypt sec cfg                    */
        /* ---------------------------------- */
        case SEC_SECCFG_ENCRYPT:
            SMSG(bMsg,"[%s] CMD - SEC_SECCFG_ENCRYPT\n",MOD);   
            if(copy_from_user((void *)&seccfg, (void __user *)arg, sizeof(SECCFG_U)))
            {
                return -EFAULT;
            }

            /* specify encrpytion length */
            SMSG(true,"[%s] SECCFG v%d\n",MOD,get_seccfg_ver());            
            if (SEC_CFG_END_PATTERN == seccfg.v1.end_pattern)
            {
                if((SECCFG_V1 != get_seccfg_ver()) && (SECCFG_V1_2 != get_seccfg_ver()))
                {
                    SMSG(true,"[%s] mismatch seccfg version v%d\n",MOD,get_seccfg_ver());
                    SEC_ASSERT(0);
                }
                
                cipher_len = get_seccfg_cipher_len();
                sec_update_lks(seccfg.v1.sw_sec_lock_try, seccfg.v1.sw_sec_lock_done, seccfg.v1.attr == ATTR_DISABLE_IMG_CHECK);
                masp_hal_sp_hacc_enc((unsigned char*)&seccfg.v1.image_info,cipher_len,rom_info.m_SEC_CTRL.m_seccfg_ac_en,HACC_USER1,FALSE);
            }
            else if (SEC_CFG_END_PATTERN == seccfg.v3.end_pattern)
            {
                if(SECCFG_V3 != get_seccfg_ver())
                {
                    SMSG(true,"[%s] mismatch seccfg version v%d\n",MOD,get_seccfg_ver());
                    SEC_ASSERT(0);
                }
                
                cipher_len = get_seccfg_cipher_len();
//zhaoyang
//SMSG(true,"[%s] zhaoyang start SEC_SECCFG_ENCRYPT\n",MOD,);
printk( "[%s] zhaoyang start SEC_SECCFG_ENCRYPT\n",MOD);
                sec_update_lks(seccfg.v3.sw_sec_lock_try, seccfg.v3.sw_sec_lock_done, seccfg.v3.seccfg_attr == ATTR_DISABLE_IMG_CHECK);
//zhaoyang
//SMSG(true,"[%s] zhaoyang end SEC_SECCFG_ENCRYPT\n",MOD,);
printk( "[%s] zhaoyang end SEC_SECCFG_ENCRYPT\n",MOD);
                masp_hal_sp_hacc_enc((unsigned char*)&seccfg.v3.image_info,cipher_len,rom_info.m_SEC_CTRL.m_seccfg_ac_en,HACC_USER1,FALSE);
            }
            else
            {
                SMSG(true,"[%s] wrong seccfg version v%d\n",MOD,seccfg.v3.seccfg_ver)
                SEC_ASSERT(0);
            }

            ret = osal_copy_to_user((void __user *)arg, (void *)&seccfg, sizeof(SECCFG_U));
            break;

        /* ---------------------------------- */
        /* decrypt sec cfg                    */
        /* ---------------------------------- */
        case SEC_SECCFG_DECRYPT:
            SMSG(bMsg,"[%s] CMD - SEC_SECCFG_DECRYPT\n",MOD);   
            if(copy_from_user((void *)&seccfg, (void __user *)arg, sizeof(SECCFG_U)))
            {
                return -EFAULT;
            }

            /* specify decrpytion length */
            if (SEC_CFG_END_PATTERN == seccfg.v1.end_pattern)
            {
                /* seccfg version should be corrected by caller */
                set_seccfg_ver(SECCFG_V1); 
                cipher_len = get_seccfg_cipher_len();
                masp_hal_sp_hacc_dec((unsigned char*)&seccfg.v1.image_info,cipher_len,rom_info.m_SEC_CTRL.m_seccfg_ac_en,HACC_USER1,FALSE);
                sec_update_lks(seccfg.v1.sw_sec_lock_try, seccfg.v1.sw_sec_lock_done, seccfg.v1.attr == ATTR_DISABLE_IMG_CHECK);
            }
            else if (SEC_CFG_END_PATTERN == seccfg.v3.end_pattern)
            {
                /* seccfg version should be corrected by caller */
                set_seccfg_ver(SECCFG_V3);
                cipher_len = get_seccfg_cipher_len();
                masp_hal_sp_hacc_dec((unsigned char*)&seccfg.v3.image_info,cipher_len,rom_info.m_SEC_CTRL.m_seccfg_ac_en,HACC_USER1,FALSE);
//zhaoyang
//SMSG(true,"[%s] zhaoyang start SEC_SECCFG_DECRYPT\n",MOD,);
printk( "[%s] zhaoyang start SEC_SECCFG_DECRYPT\n",MOD);


                sec_update_lks(seccfg.v3.sw_sec_lock_try, seccfg.v3.sw_sec_lock_done, seccfg.v3.seccfg_attr == ATTR_DISABLE_IMG_CHECK);
//zhaoyang
//SMSG(true,"[%s] zhaoyang end SEC_SECCFG_DECRYPT\n",MOD,);
printk( "[%s] zhaoyang end SEC_SECCFG_DECRYPT\n",MOD);
            }
            else
            {
                SMSG(true,"[%s] wrong seccfg version v%d\n",MOD,seccfg.v3.seccfg_ver)
                SEC_ASSERT(0);
            }            

            SMSG(bMsg,"[%s] SECCFG v%d\n",MOD,get_seccfg_ver());  
            
            ret = osal_copy_to_user((void __user *)arg, (void *)&seccfg, sizeof(SECCFG_U));
            break;

        /* ---------------------------------- */
        /* NVRAM HW encryption                */
        /* ---------------------------------- */
        case SEC_NVRAM_HW_ENCRYPT:
            SMSG(bMsg,"[%s] CMD - SEC_NVRAM_HW_ENCRYPT\n",MOD);   
            if(osal_copy_from_user((void *)&meta_ctx, (void __user *)arg, sizeof(meta_ctx)))
            {
                return -EFAULT;
            }

            /* TODO : double check if META register is correct ? */
            masp_hal_sp_hacc_enc((unsigned char*)&(meta_ctx.data),NVRAM_CIPHER_LEN,TRUE,HACC_USER2,FALSE);
            meta_ctx.ret = SEC_OK;
            
            ret = osal_copy_to_user((void __user *)arg, (void *)&meta_ctx, sizeof(meta_ctx));
            break;

        /* ---------------------------------- */
        /* NVRAM HW decryption                */
        /* ---------------------------------- */
        case SEC_NVRAM_HW_DECRYPT:
            SMSG(bMsg,"[%s] CMD - SEC_NVRAM_HW_DECRYPT\n",MOD);   
            if(osal_copy_from_user((void *)&meta_ctx, (void __user *)arg, sizeof(meta_ctx)))
            {
                return -EFAULT;
            }

            masp_hal_sp_hacc_dec((unsigned char*)&(meta_ctx.data),NVRAM_CIPHER_LEN,TRUE,HACC_USER2,FALSE);
            meta_ctx.ret = SEC_OK;
            ret = osal_copy_to_user((void __user *)arg, (void *)&meta_ctx, sizeof(meta_ctx));
            break;

        /* ---------------------------------- */
        /* HEVC EOP                           */
        /* ---------------------------------- */
        case SEC_HEVC_EOP:
            SMSG(TRUE,"[%s] CMD - SEC_HEVC_EOP\n",MOD);   
            if(osal_copy_from_user((void *)(&hevc_blk), (void __user *)arg, sizeof(HEVC_BLK)))
            {
                return -EFAULT;
            }
            
            if ((hevc_blk.len % CI_BLK_SIZE) == 0)
            {
                cipher_len = hevc_blk.len;
            }
            else if ((hevc_blk.len % CI_BLK_SIZE) > 0)
            {
                cipher_len = CI_BLK_ALIGN(hevc_blk.len)-CI_BLK_SIZE;
                if (cipher_len == 0 ){
                    SMSG(TRUE,"[%s] less than one ci_blk, no need to do eop",MOD);   
                    break; 
                }
            }
            masp_hal_sp_hacc_enc((unsigned char*)(&hevc_blk.buf),cipher_len,TRUE,HACC_USER4,FALSE);

            ret = osal_copy_to_user((void __user *)arg, (void *)(&hevc_blk), sizeof(HEVC_BLK));
            break;

        /* ---------------------------------- */
        /* HEVC DOP                           */
        /* ---------------------------------- */
        case SEC_HEVC_DOP:
            SMSG(TRUE,"[%s] CMD - SEC_HEVC_DOP\n",MOD);   
            if(osal_copy_from_user((void *)(&hevc_blk), (void __user *)arg, sizeof(HEVC_BLK)))
            {
                return -EFAULT;
            }

            if ((hevc_blk.len % CI_BLK_SIZE) == 0)
            {
                cipher_len = hevc_blk.len;
            }
            else if ((hevc_blk.len % CI_BLK_SIZE) > 0)
            {
                cipher_len = CI_BLK_ALIGN(hevc_blk.len)-CI_BLK_SIZE;
                if (cipher_len == 0 ){
                    SMSG(TRUE,"[%s] less than one ci_blk, no need to do dop",MOD);   
                    break; 
                }
            }

            masp_hal_sp_hacc_dec((unsigned char*)(&hevc_blk.buf),cipher_len,TRUE,HACC_USER4,FALSE);
            
            ret = osal_copy_to_user((void __user *)arg, (void *)(&hevc_blk), sizeof(HEVC_BLK));
            break;

        /* ---------------------------------- */
        /* check if secure usbdl is enbaled   */
        /* ---------------------------------- */
        case SEC_USBDL_IS_ENABLED:
            SMSG(bMsg,"[%s] CMD - SEC_USBDL_IS_ENABLED\n",MOD);
            ret = sec_usbdl_enabled();
            ret = osal_copy_to_user((void __user *)arg, (void *)&ret, sizeof(int));            
            break;

        /* ---------------------------------- */
        /* configure HACC HW (include SW KEY)  */
        /* ---------------------------------- */
        case SEC_HACC_CONFIG:
            SMSG(bMsg,"[%s] CMD - SEC_HACC_CONFIG\n",MOD);
            ret = sec_boot_hacc_init();
            ret = osal_copy_to_user((void __user *)arg, (void *)&ret, sizeof(int));            
            break;

        /* ---------------------------------- */
        /* enable HACC HW clock                */
        /* ---------------------------------- */
        case SEC_HACC_ENABLE_CLK:
            SMSG(bMsg,"[%s] CMD - SEC_HACC_ENABLE_CLK\n",MOD);
            ret = osal_copy_to_user((void __user *)arg, (void *)&ret, sizeof(int));            
            break;

        /* ---------------------------------- */
        /* lock hacc function                  */
        /* ---------------------------------- */
        case SEC_HACC_LOCK:
        
            SMSG(bMsg,"[%s] CMD - SEC_HACC_LOCK\n",MOD);   
            SMSG(bMsg,"[%s] lock\n",MOD);

            /* If the semaphore is successfully acquired, this function returns 0.*/                
            ret = osal_hacc_lock();

            if(ret)
            {
                SMSG(true,"[%s] ERESTARTSYS\n",MOD);
                return -ERESTARTSYS;
            }

            return ret;
            
        /* ---------------------------------- */
        /* unlock hacc function                */
        /* ---------------------------------- */
        case SEC_HACC_UNLOCK:

            SMSG(bMsg,"[%s] CMD - SEC_HACC_UNLOCK\n",MOD);   
            SMSG(bMsg,"[%s] unlock\n",MOD);

            osal_hacc_unlock();
            
            break;

        /* ---------------------------------- */
        /* check if secure boot check enabled */
        /* ---------------------------------- */
        case SEC_BOOT_PART_CHECK_ENABLE:
            SMSG(bMsg,"[%s] CMD -SEC_BOOT_PART_CHECK_ENABLE\n",MOD);
            if(copy_from_user((void *)part_name, (void __user *)arg, sizeof(part_name)))
            {
                return -EFAULT;
            }
            ret = sec_boot_check_part_enabled (part_name);
            SMSG(bMsg,"[%s] result '0x%x'\n",MOD,ret);
            return ret;

        /* ---------------------------------- */
        /* notify mark incomplete             */
        /* ---------------------------------- */
        case SEC_BOOT_NOTIFY_MARK_STATUS:
            SMSG(true,"[%s] mark status\n",MOD);
            /* may do some post process here ... */
            break;

        /* ---------------------------------- */
        /* notify check pass                  */
        /* ---------------------------------- */
        case SEC_BOOT_NOTIFY_PASS:
            SMSG(true,"[%s] sbchk pass\n",MOD);
            SMSG(true,"[%s] sbchk pass\n",MOD);
            SMSG(true,"[%s] sbchk pass\n",MOD);
            SMSG(true,"[%s] sbchk pass\n",MOD);            
            SMSG(true,"[%s] sbchk pass\n",MOD);
            /* may do some post process here ... */
            break;

        /* ---------------------------------- */
        /* notify check fail                  */
        /* ---------------------------------- */
        case SEC_BOOT_NOTIFY_FAIL:
            if(osal_copy_from_user((void *)part_name, (void __user *)arg, sizeof(part_name)))
            {
                return -EFAULT;
            }

            SMSG(true,"[%s] sbchk fail '%s'\n",MOD,part_name);
            SMSG(true,"[%s] sbchk fail '%s'\n",MOD,part_name);
            SMSG(true,"[%s] sbchk fail '%s'\n",MOD,part_name);
            SMSG(true,"[%s] sbchk fail '%s'\n",MOD,part_name);
            SMSG(true,"[%s] sbchk fail '%s'\n",MOD,part_name);
            osal_msleep(3000);
            /* punishment ... */    
            SEC_ASSERT(0);
            break;

        /* ---------------------------------- */
        /* notify recovery mode done          */
        /* ---------------------------------- */
        case SEC_BOOT_NOTIFY_RMSDUP_DONE:
            SMSG(true,"[%s] recovery mode done\n",MOD);
            /* may do some post process here ... */
            break;

        /* ---------------------------------- */
        /* read rom info                      */
        /* ---------------------------------- */
        case SEC_READ_ROM_INFO:
            SMSG(bMsg,"[%s] read rom info\n",MOD);            
            ret = osal_copy_to_user((void __user *)arg, (void *)&rom_info, sizeof(AND_ROMINFO_T));
            break;     
            
        
        /* ---------------------------------- */
        /* notify check status          */
        /* ---------------------------------- */
        case SEC_BOOT_NOTIFY_STATUS:     
            ret = osal_copy_from_user((void *)&status, (void __user *)arg, sizeof(int)); 
            SMSG(true,"[%s] sbchk status : '0x%x' \n",MOD,status);
            break;
    }

    return 0;
}

/**************************************************************************
 *  SEC DRIVER INIT
 **************************************************************************/ 
void sec_core_init (void)
{
    SMSG(true,"[%s] version '%s%s', enter.\n",MOD,BUILD_TIME,BUILD_BRANCH);
    
    /* ---------------------------------- */
    /* disable key init in kerne module   */
    /* ---------------------------------- */
    sec_info.bKeyInitDis = TRUE;    
}

/**************************************************************************
 *  SEC DRIVER EXIT
 **************************************************************************/ 
void sec_core_exit (void)
{
    SMSG(true,"[%s] version '%s%s', exit.\n",MOD,BUILD_TIME,BUILD_BRANCH);
}

