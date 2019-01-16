#define JPEG_DEC_DRIVER

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <generated/autoconf.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/param.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/xlog.h>

#include <asm/io.h>

#include <cmdq_core.h>
#include <jpeg_cmdq.h>
//============================================================

//#include <linux/uaccess.h>
//#include <linux/module.h>
//#include <linux/fs.h>
//#include <linux/platform_device.h>
//#include <linux/cdev.h>
#include <linux/interrupt.h>
//#include <asm/io.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
//#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
//#include <linux/slab.h>
//#include <linux/gfp.h>
#include <linux/aee.h>
#include <linux/timer.h>
#include <linux/disp_assert_layer.h>
//#include <linux/xlog.h>
//#include <linux/fs.h>

//Arch dependent files
//#include <asm/mach/map.h>
//#include <mach/mt6577_pll.h>
#include <mach/mt_irq.h>
#include <mach/mt_clkmgr.h>
#include <mach/irqs.h>

// #include <asm/tcm.h>
#include <asm/cacheflush.h>
//#include <asm/system.h>
//#include <linux/mm.h>
#include <linux/pagemap.h>

#ifndef FPGA_VERSION
  #include <mach/mt_boot.h>
#endif

#include <cmdq_record.h>

#ifndef JPEG_DEV
#include <linux/proc_fs.h>
#endif

#ifdef CONFIG_OF
/* device tree */
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#endif

//==========================================================

#include "jpeg_drv.h"
#include "jpeg_drv_6589_common.h"

//#define USE_SYSRAM
#if 0
#define JPEG_MSG(...)   xlog_printk(ANDROID_LOG_DEBUG, "xlog/jpeg", __VA_ARGS__)
#define JPEG_WRN(...)   xlog_printk(ANDROID_LOG_WARN,  "xlog/jpeg", __VA_ARGS__)
#define JPEG_ERR(...)   xlog_printk(ANDROID_LOG_ERROR, "xlog/jpeg", __VA_ARGS__)
#else
 #define JPEG_MSG printk
 #define JPEG_WRN printk
 #define JPEG_ERR printk
#endif

#define JPEG_DEVNAME "mtk_jpeg"

#define TABLE_SIZE 4096

#define JPEG_DEC_PROCESS 0x1
#define JPEG_ENC_PROCESS 0x2

//#define FPGA_VERSION
#include "jpeg_drv_6589_reg.h"
//--------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------

// global function
extern kal_uint32 _jpeg_enc_int_status;
extern kal_uint32 _jpeg_dec_int_status;
extern kal_uint32 _jpeg_dec_mode ;

#ifdef JPEG_DEV    
// device and driver
static dev_t jpeg_devno;
static struct cdev *jpeg_cdev;
static struct class *jpeg_class = NULL;
#endif

#ifdef CONFIG_OF
static struct JpegDeviceStruct gJpegqDev;
static struct JpegDeviceStruct *gJpegqDevs;
static int nrJpegDevs = 0;

static const struct of_device_id jpeg_of_ids[] = {
    { .compatible = "mediatek,JPGENC", },
    {}
};
#endif

// decoder
static wait_queue_head_t dec_wait_queue;
static spinlock_t jpeg_dec_lock;
static int dec_status = 0;

// encoder
static wait_queue_head_t enc_wait_queue;
static spinlock_t jpeg_enc_lock;
static int enc_status = 0;

//==========================================
//CMDQ

//static cmdqRecStruct jpegCMDQ_handle;
static cmdqRecHandle jpegCMDQ_handle;


//--------------------------------------------------------------------------
// JPEG Common Function
//--------------------------------------------------------------------------
void jpeg_reg_dump(void);

#ifdef FPGA_VERSION

void jpeg_drv_dec_power_on(void)
{  
    JPEG_MSG("JPEG Decoder Power On\n");
}

void jpeg_drv_dec_power_off(void)
{  
    JPEG_MSG("JPEG Decoder Power Off\n");
}

void jpeg_drv_enc_power_on(void)
{  
#ifdef FPGA_VERSION   
    IMG_REG_WRITE( (0), JPEG_EARLY_MM_BASE);  
    JPEG_MSG("JPEG Encoder RESET_MM_BASE!!\n");
#endif
    JPEG_MSG("JPEG Encoder Power On\n");
}

void jpeg_drv_enc_power_off(void)
{  
    JPEG_MSG("JPEG Encoder Power Off\n");
}

#else

static irqreturn_t jpeg_drv_enc_isr(int irq, void *dev_id)
{
    //JPEG_MSG("JPEG Encoder Interrupt\n");

    if(irq == gJpegqDev.encIrqId)
    {
        //mt65xx_irq_mask(MT6575_JPEG_CODEC_IRQ_ID);
#if 0
        if(jpeg_isr_dec_lisr() == 0)
        {
            wake_up_interruptible(&dec_wait_queue);
        }
#endif        
        if(jpeg_isr_enc_lisr() == 0)
        {
            wake_up_interruptible(&enc_wait_queue);
        }        
        //mt65xx_irq_unmask(MT6575_JPEG_CODEC_IRQ_ID);        
    }

    return IRQ_HANDLED;
}

#ifdef JPEG_DEC_DRIVER
static irqreturn_t jpeg_drv_dec_isr(int irq, void *dev_id)
{
    //JPEG_MSG("JPEG Decoder Interrupt\n");
    //jpeg_reg_dump();

    if(irq == gJpegqDev.decIrqId)
    {
        //mt65xx_irq_mask(MT6575_JPEG_CODEC_IRQ_ID);

        if(jpeg_isr_dec_lisr() == 0)
        {
            wake_up_interruptible(&dec_wait_queue);
        }
#if 0        
        if(jpeg_isr_enc_lisr() == 0)
        {
            wake_up_interruptible(&enc_wait_queue);
        }        
#endif        
        //mt65xx_irq_unmask(MT6575_JPEG_CODEC_IRQ_ID);        
    }

    return IRQ_HANDLED;
}
#endif

void jpeg_drv_dec_power_on(void)
{  
    //REG_JPEG_MM_REG_MASK = 0;   
#ifndef FPGA_VERSION    
    enable_clock(MT_CG_DISP0_SMI_COMMON,"JPEG");
    enable_clock(MT_CG_VENC_LARB,"JPEG");
    enable_clock(MT_CG_VENC_JPGDEC,"JPEG");
#endif
   
#if 0
    //MCI
    *(volatile kal_uint32 *)(0xF0208100) |= (0x4|0x8|0x10);
     dsb();
    *(volatile kal_uint32 *)(0xF0208100)  |= (0x11);
     dsb();
#endif  
   
#ifdef FOR_COMPILE   
    BOOL ret;
    ret = enable_clock(MT65XX_PDN_MM_JPEG_DEC,"JPEG");
    NOT_REFERENCED(ret);
#endif
}

void jpeg_drv_dec_power_off(void)
{  
#ifndef FPGA_VERSION   
    disable_clock(MT_CG_VENC_JPGDEC,"JPEG");
    disable_clock(MT_CG_VENC_LARB,"JPEG");
    disable_clock(MT_CG_DISP0_SMI_COMMON,"JPEG");
#endif
   
#ifdef FOR_COMPILE   
    BOOL ret;
    ret = disable_clock(MT65XX_PDN_MM_JPEG_DEC,"JPEG");
    NOT_REFERENCED(ret);
#endif
}
  
void jpeg_drv_enc_power_on(void)
{  
#ifndef FPGA_VERSION    
    //REG_JPEG_MM_REG_MASK  = 0;
   enable_clock(MT_CG_DISP0_SMI_COMMON,"JPEG");
   enable_clock(MT_CG_DISP0_MDP_CROP,"JPEG");
   enable_clock(MT_CG_VENC_LARB,"JPEG");
   enable_clock(MT_CG_VENC_JPGENC,"JPEG");
#endif   

#if 0
    //MCI
    *(volatile kal_uint32 *)(0xF0208100) |= (0x4|0x8|0x10);
     dsb();
    *(volatile kal_uint32 *)(0xF0208100)  |= (0x11);
     dsb();
#endif 
   
#ifdef FOR_COMPILE   
    BOOL ret;
    ret = enable_clock(MT65XX_PDN_MM_JPEG_ENC,"JPEG");
    NOT_REFERENCED(ret);
#endif    
}

void jpeg_drv_enc_power_off(void)
{  
#ifndef FPGA_VERSION    
    disable_clock(MT_CG_VENC_JPGENC,"JPEG");
    disable_clock(MT_CG_VENC_LARB,"JPEG");
    disable_clock(MT_CG_DISP0_MDP_CROP,"JPEG");
    disable_clock(MT_CG_DISP0_SMI_COMMON,"JPEG");
#endif   

#ifdef FOR_COMPILE   
    BOOL ret;
    ret = disable_clock(MT65XX_PDN_MM_JPEG_ENC,"JPEG");
    NOT_REFERENCED(ret);
#endif    
}

#endif


static int jpeg_drv_dec_init(void)
{
    int retValue;
    spin_lock(&jpeg_dec_lock);
    if(dec_status != 0)
    {
        JPEG_WRN("JPEG Decoder is busy\n");
        retValue = -EBUSY;
    }    
    else
    {
        dec_status = 1;
        retValue = 0;    
    }   
    spin_unlock(&jpeg_dec_lock);

    if(retValue == 0)
    {
        jpeg_drv_dec_power_on();

        jpeg_drv_dec_reset();
    }

    return retValue;
}

static void jpeg_drv_dec_deinit(void)
{
    if(dec_status != 0)
    {

        spin_lock(&jpeg_dec_lock);
        dec_status = 0;
        spin_unlock(&jpeg_dec_lock);

        jpeg_drv_dec_reset();

        jpeg_drv_dec_power_off();
    }
}


static int jpeg_drv_enc_init(void)
{
    int retValue;
    
    spin_lock(&jpeg_enc_lock);
    if(enc_status != 0)
    {
        JPEG_WRN("JPEG Encoder is busy\n");
        retValue = -EBUSY;
    }    
    else
    {
        enc_status = 1;
        retValue = 0;    
    }   
    spin_unlock(&jpeg_enc_lock);

    if(retValue == 0)
    {
        jpeg_drv_enc_power_on();
        jpeg_drv_enc_reset();
    }

    return retValue;
}

static void jpeg_drv_enc_deinit(void)
{
    if(enc_status != 0)
    {
        spin_lock(&jpeg_enc_lock);
        enc_status = 0;
        spin_unlock(&jpeg_enc_lock);

        jpeg_drv_enc_reset();
        jpeg_drv_enc_power_off();
    }
}


//--------------------------------------------------------------------------
// JPEG REG DUMP FUNCTION
//--------------------------------------------------------------------------
void jpeg_reg_dump(void)
{
    unsigned int reg_value = 0;
    unsigned int index = 0;

    JPEG_WRN("JPEG REG:\n ********************\n");
    for(index = 0 ; index < 0x168 ; index += 4){
        //reg_value = ioread32(JPEG_DEC_BASE + index); 
        IMG_REG_READ(reg_value, JPEG_DEC_BASE + index);
        JPEG_WRN("+0x%x 0x%x\n", index, reg_value);
    }
}
//--------------------------------------------------------------------------
// JPEG DECODER IOCTRL FUNCTION
//--------------------------------------------------------------------------


static int jpeg_dec_ioctl(unsigned int cmd, unsigned long arg, struct file *file)
{
    unsigned int*       pStatus;
    unsigned int        decResult;
    long timeout_jiff;
    JPEG_DEC_DRV_IN dec_params;
    JPEG_DEC_CONFIG_ROW dec_row_params ;
    JPEG_DEC_CONFIG_CMDQ cfg_cmdq_params ;
    
    unsigned int irq_st = 0;
    unsigned int i = 0;
    //unsigned int timeout = 0x1FFFFF;

    JPEG_DEC_DRV_OUT outParams;

    pStatus = (unsigned int*)file->private_data;

    if (NULL == pStatus)
    {
        JPEG_MSG("[JPEGDRV]JPEG Decoder: Private data is null in flush operation. SOME THING WRONG??\n");
        return -EFAULT;
    }
    switch(cmd)
    { 
            // initial and reset JPEG encoder
        case JPEG_DEC_IOCTL_INIT:   /* OT:OK */
            JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Init!!\n");
            if(jpeg_drv_dec_init() == 0)
            {
                *pStatus = JPEG_DEC_PROCESS;
            }
            break;
            
        case JPEG_DEC_IOCTL_RESET:  /* OT:OK */
            JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Reset!!\n");
            jpeg_drv_dec_reset();
            break;
            
        case JPEG_DEC_IOCTL_CONFIG:
            JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Configration!!\n");
            if(*pStatus != JPEG_DEC_PROCESS)
            {
                JPEG_MSG("[JPEGDRV]Permission Denied! This process can not access decoder\n");
                return -EFAULT;
            }
            if(dec_status == 0)
            {
                JPEG_MSG("[JPEGDRV]JPEG Decoder is unlocked!!");
                *pStatus = 0;
                return -EFAULT;
            }
            if(copy_from_user(&dec_params, (void *)arg, sizeof(JPEG_DEC_DRV_IN)))
            {
                JPEG_MSG("[JPEGDRV]JPEG Decoder : Copy from user error\n");
                return -EFAULT;
            }
            //_jpeg_dec_dump_reg_en = dec_params.regDecDumpEn;
            if(dec_params.decodeMode == JPEG_DEC_MODE_MCU_ROW)
               _jpeg_dec_mode = 1;
            else 
               _jpeg_dec_mode = 0;
               
            if (jpeg_drv_dec_set_config_data(&dec_params) < 0)
                return -EFAULT;

            break;
            
            case JPEG_DEC_IOCTL_FLUSH_CMDQ :
              
              JPEG_MSG("[JPEGDRV]enter JPEG BUILD CMDQ !!\n");
              if(*pStatus != JPEG_DEC_PROCESS)
              {
                  JPEG_MSG("[JPEGDRV]Permission Denied! This process can not access decoder\n");
                  return -EFAULT;
              }
              if(dec_status == 0)
              {
                  JPEG_MSG("[JPEGDRV]JPEG Decoder is unlocked!!");
                  *pStatus = 0;
                  return -EFAULT;
              }
              if(copy_from_user(&cfg_cmdq_params, (void *)arg, sizeof(JPEG_DEC_CONFIG_CMDQ)))
              {
                  JPEG_MSG("[JPEGDRV]JPEG Decoder : Copy from user error\n");
                  return -EFAULT;
              }
              JPEG_MSG("[JPEGDRV]JPEG CREATE CMDQ !!\n");
              
              cmdqRecCreate(CMDQ_SCENARIO_JPEG_DEC, &jpegCMDQ_handle) ;
              
              cmdqRecWait(jpegCMDQ_handle, CMDQ_EVENT_JPEG_DEC_EOF) ; 
              
              for( i = 0 ; i < cfg_cmdq_params.goNum ; i++ ){
                JPEG_MSG("[JPEGDRV]JPEG gen CMDQ %x %x %x %x  !!\n", cfg_cmdq_params.pauseMCUidx[i], 
                cfg_cmdq_params.decRowBuf0[i],  cfg_cmdq_params.decRowBuf1[i], cfg_cmdq_params.decRowBuf2[i]);
                cmdqRecWrite(jpegCMDQ_handle,  0x18004170 /*REG_ADDR_JPGDEC_PAUSE_MCU_NUM*/, cfg_cmdq_params.pauseMCUidx[i]-1 , 0xFFFFFFFF) ;    
                cmdqRecWrite(jpegCMDQ_handle,  0x18004140 /*REG_ADDR_JPGDEC_DEST_ADDR0_Y */,    cfg_cmdq_params.decRowBuf0[i] , 0xFFFFFFFF) ;    
                cmdqRecWrite(jpegCMDQ_handle,  0x18004144 /*REG_ADDR_JPGDEC_DEST_ADDR0_U */,    cfg_cmdq_params.decRowBuf1[i] , 0xFFFFFFFF) ;    
                cmdqRecWrite(jpegCMDQ_handle,  0x18004148 /*REG_ADDR_JPGDEC_DEST_ADDR0_V */,    cfg_cmdq_params.decRowBuf2[i] , 0xFFFFFFFF) ;    

                JPEG_MSG("[JPEGDRV]JPEG gen CMDQ go!!\n");
                //trigger 
                cmdqRecWrite(jpegCMDQ_handle, 0x18004274 /*REG_ADDR_JPGDEC_INTERRUPT_STATUS*/ ,    BIT_INQST_MASK_PAUSE , 0xFFFFFFFF) ;    
                
                JPEG_MSG("[JPEGDRV]JPEG gen CMDQ wait!!\n");
                //wait frame done
                cmdqRecWait(jpegCMDQ_handle, CMDQ_EVENT_JPEG_DEC_EOF) ; 
                //cmdqRecPoll(jpegCMDQ_handle, 0x18004274 /*REG_ADDR_JPGDEC_INTERRUPT_STATUS*/ ,    BIT_INQST_MASK_PAUSE , 0x0010) ;    
              }
              
              JPEG_MSG("[JPEGDRV]JPEG flush CMDQ start!!\n");
              cmdqRecFlush(jpegCMDQ_handle) ;
              JPEG_MSG("[JPEGDRV]JPEG flush CMDQ end!!\n");
              
              cmdqRecDestroy(jpegCMDQ_handle) ;    
              JPEG_MSG("[JPEGDRV]JPEG destroy CMDQ end!!\n");
               
              break;
            
            case JPEG_DEC_IOCTL_RESUME:
              if(*pStatus != JPEG_DEC_PROCESS)
              {
                  JPEG_MSG("[JPEGDRV]Permission Denied! This process can not access decoder\n");
                  return -EFAULT;
              }
              if(dec_status == 0)
              {
                  JPEG_MSG("[JPEGDRV]JPEG Decoder is unlocked!!");
                  *pStatus = 0;
                  return -EFAULT;
              }
              if(copy_from_user(&dec_row_params, (void *)arg, sizeof(JPEG_DEC_CONFIG_ROW)))
              {
                  JPEG_MSG("[JPEGDRV]JPEG Decoder : Copy from user error\n");
                  return -EFAULT;
              }

              JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Resume, [%d] %x %x %x !!\n", dec_row_params.pauseMCU -1,dec_row_params.decRowBuf[0], dec_row_params.decRowBuf[1], dec_row_params.decRowBuf[2]);              
              
              jpeg_drv_dec_set_dst_bank0( dec_row_params.decRowBuf[0], dec_row_params.decRowBuf[1], dec_row_params.decRowBuf[2]);
      
              jpeg_drv_dec_set_pause_mcu_idx(dec_row_params.pauseMCU -1) ;
            
              // lock CPU to ensure irq is enabled after trigger HW
              spin_lock(&jpeg_dec_lock);
              jpeg_drv_dec_resume(BIT_INQST_MASK_PAUSE);
              spin_unlock(&jpeg_dec_lock);
            break;
            
        case JPEG_DEC_IOCTL_START:    /* OT:OK */
            JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Start!!\n");
            
            //Debug: printk("0xF0: 0x%08x\n", *(volatile unsigned int*)(JPEG_DEC_BASE + 0xF0));
            
            jpeg_drv_dec_start();
            break;
            
        case JPEG_DEC_IOCTL_WAIT:
            if(*pStatus != JPEG_DEC_PROCESS)
            {
                JPEG_WRN("Permission Denied! This process can not access decoder");
                return -EFAULT;
            }
            if(dec_status == 0)
            {
                JPEG_WRN("Decoder status is available, HOW COULD THIS HAPPEN ??");
                *pStatus = 0;
                return -EFAULT;
            }           
            if(copy_from_user(&outParams, (void *)arg, sizeof(JPEG_DEC_DRV_OUT)))
            {
                JPEG_WRN("JPEG Decoder : Copy from user error\n");
                return -EFAULT;
            }

            //set timeout
            timeout_jiff = outParams.timeout* HZ / 1000;
            //JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Wait Resume Time Jiffies : %ld\n", timeout_jiff);   
#ifdef FPGA_VERSION
//#if 1

            JPEG_MSG("[JPEGDRV]Polling JPEG Status");

            do
            {
                _jpeg_dec_int_status = REG_JPGDEC_INTERRUPT_STATUS;
            } while(_jpeg_dec_int_status == 0);
#else

            //if(outParams.timeout >= 5000){
            //       
            //  JPEG_MSG("Polling JPEG Status");              
            //  do
            //  {
            //      _jpeg_dec_int_status = REG_JPGDEC_INTERRUPT_STATUS;
            //     timeout--;
            //  } while(_jpeg_dec_int_status == 0 && timeout != 0);                              
            //  if(timeout == 0) JPEG_MSG("Polling JPEG Status TIMEOUT!!\n");              
            //}else
            if(jpeg_isr_dec_lisr()<0){
              //JPEG_MSG("wait JPEG irq\n");
              wait_event_interruptible_timeout(dec_wait_queue, _jpeg_dec_int_status, timeout_jiff);              
              JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Enter IRQ Wait Done!!\n");
              //printk("[JPEGDRV]wait JPEG irq done\n");
            }else{
              JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Enter IRQ Wait Already Done!!\n"); 
              //printk("[JPEGDRV]JPEG decoder already done\n"); 
            }
#endif
            
            decResult = jpeg_drv_dec_get_result();

            //jpeg_drv_dec_dump_key_reg();
            
            if(decResult >= 2)
            {
                JPEG_MSG("[JPEGDRV]Decode Result : %d, status %x!\n", decResult, _jpeg_dec_int_status );            
                jpeg_drv_dec_dump_key_reg();  
                //jpeg_drv_dec_dump_reg();
                jpeg_drv_dec_reset();
            }
            irq_st = _jpeg_dec_int_status ;
            decResult = decResult | (irq_st<<8) ;
            _jpeg_dec_int_status = 0;    
            if(copy_to_user(outParams.result, &decResult, sizeof(unsigned int)))
            {
                JPEG_WRN("JPEG Decoder : Copy to user error (result)\n");
                return -EFAULT;            
            }
    
            break;

        case JPEG_DEC_IOCTL_BREAK:
            if (jpeg_drv_dec_break() < 0)
                return -EFAULT;
            break;
            
        case JPEG_DEC_IOCTL_DUMP_REG:
            JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder DUMP REGISTER !!\n");
            jpeg_drv_dec_dump_reg();
            break;

        case JPEG_DEC_IOCTL_DEINIT:
            JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Deinit !!\n");
            // copy input parameters
            if(*pStatus != JPEG_DEC_PROCESS)
            {
                JPEG_ERR("Permission Denied! This process can not access encoder");
                return -EFAULT;
            }

            if(dec_status == 0)
            {
                JPEG_ERR("Encoder status is available, HOW COULD THIS HAPPEN ??");
                *pStatus = 0;
                return -EFAULT;
            }
            jpeg_drv_dec_deinit();
            *pStatus = 0;   
            break;
#ifdef FOR_COMPILE            
        case JPEG_DEC_IOCTL_RW_REG: /* OT:OK */
            jpeg_drv_dec_rw_reg();
            break;
#endif
        default:
            JPEG_ERR("JPEG DEC IOCTL NO THIS COMMAND\n");
            break;
    }
    return 0;
}


static int jpeg_enc_ioctl(unsigned int cmd, unsigned long arg, struct file *file)
{
    int retValue;
    //unsigned int decResult;
    
    long timeout_jiff;
    unsigned int file_size, enc_result_code;
    //unsigned int _jpeg_enc_int_status;
    unsigned int jpeg_enc_wait_timeout = 0;
    unsigned int cycle_count;
    unsigned int ret ;
    
    unsigned int *pStatus;
    
    //JpegDrvEncParam cfgEnc;
    JPEG_ENC_DRV_IN cfgEnc;
    
    JPEG_ENC_DRV_OUT enc_result;

    pStatus = (unsigned int*)file->private_data;

    if(NULL == pStatus)
    {
        JPEG_WRN("Private data is null in flush operation. HOW COULD THIS HAPPEN ??\n");
        return -EFAULT;
    }
    
    switch(cmd)
    {       
        case JPEG_ENC_IOCTL_RW_REG:
            //jpeg_drv_enc_rw_reg();
            break;

        // initial and reset JPEG encoder
        case JPEG_ENC_IOCTL_INIT: 
            JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Init!!\n");
            
            retValue = jpeg_drv_enc_init();

            if(retValue == 0)
            {
                *pStatus = JPEG_ENC_PROCESS;
            }
            
            return retValue;   
     
            break;
            
        case JPEG_ENC_IOCTL_WARM_RESET:
            JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Warm Reset\n");
            enc_result_code = jpeg_drv_enc_warm_reset();
            if (0 == enc_result_code)
            {
                return -EFAULT;
            }                
            break;
            
        // configure the register
        case JPEG_ENC_IOCTL_CONFIG:
            JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Configure Hardware\n");
            if(*pStatus != JPEG_ENC_PROCESS)
            {
                JPEG_WRN("Permission Denied! This process can not access encoder");
                return -EFAULT;
            }
            
            if(enc_status == 0)
            {
                JPEG_WRN("Encoder status is available, HOW COULD THIS HAPPEN ??");
                *pStatus = 0;
                return -EFAULT;
            }
            
            // copy input parameters
            if(copy_from_user(&cfgEnc, (void *)arg, sizeof(JPEG_ENC_DRV_IN)))
            {
                JPEG_MSG("[JPEGDRV]JPEG Encoder : Copy from user error\n");
                return -EFAULT;
            }
            
            // 0. reset 
            jpeg_drv_enc_reset();
            
            // 1. set src config
            //memset(&src_cfg, 0, sizeof(JpegDrvEncSrcCfg));
            
            //src_cfg.luma_addr = cfgEnc.srcBufferAddr;
            //if (cfgEnc.encFormat == NV12 || cfgEnc.encFormat == NV21)
            //{
            //    unsigned int srcChromaAddr = cfgEnc.srcChromaAddr;                
            //    srcChromaAddr = TO_CEIL(srcChromaAddr, 128);    //((srcChromaAddr+127)&~127);
            //    src_cfg.chroma_addr = srcChromaAddr;
            //}
            //
            //src_cfg.width = cfgEnc.encWidth;
            //src_cfg.height = cfgEnc.encHeight;            
            //src_cfg.yuv_format = cfgEnc.encFormat;
            
            // 1. set src config            
            JPEG_MSG("[JPEGDRV]SRC_IMG: %x %x, DU:%x, fmt:%x!!\n", cfgEnc.encWidth, cfgEnc.encHeight, cfgEnc.totalEncDU, cfgEnc.encFormat);
            
            ret = jpeg_drv_enc_set_src_image(cfgEnc.encWidth, cfgEnc.encHeight, cfgEnc.encFormat, cfgEnc.totalEncDU) ;
            if(ret == 0){
                JPEG_MSG("[JPEGDRV]JPEG Encoder set srouce image failed\n");
                return -EFAULT;
            }
            
            // 2. set src buffer info
            JPEG_MSG("[JPEGDRV]SRC_BUF: addr %x, %x, stride %x, %x!!\n", cfgEnc.srcBufferAddr, cfgEnc.srcChromaAddr ,cfgEnc.imgStride, cfgEnc.memStride);
            
            ret = jpeg_drv_enc_set_src_buf(cfgEnc.encFormat, cfgEnc.imgStride, cfgEnc.memStride, cfgEnc.srcBufferAddr, cfgEnc.srcChromaAddr);
            if(ret == 0){
                JPEG_MSG("[JPEGDRV]JPEG Encoder set srouce buffer failed\n");
                return -EFAULT;
            }            
            
            //if (0 == jpeg_drv_enc_src_cfg(src_cfg))
            //{
            //    JPEG_MSG("JPEG Encoder src cfg failed\n");
            //    return -EFAULT;
            //}
            
            // 3. set dst buffer info
            JPEG_MSG("[JPEGDRV]DST_BUF: addr:%x, size:%x, ofs:%x, mask:%x!!\n",cfgEnc.dstBufferAddr, cfgEnc.dstBufferSize, cfgEnc.dstBufAddrOffset, cfgEnc.dstBufAddrOffsetMask);            
            
            ret = jpeg_drv_enc_set_dst_buff(cfgEnc.dstBufferAddr, cfgEnc.dstBufferSize, cfgEnc.dstBufAddrOffset, cfgEnc.dstBufAddrOffsetMask);
            if (ret == 0 ){
                JPEG_MSG("[JPEGDRV]JPEG Encoder set dst buffer failed\n");
                return -EFAULT;
            }                                             
            //memset(&dst_cfg, 0, sizeof(JpegDrvEncDstCfg));
            //
            //dst_cfg.dst_addr = cfgEnc.dstBufferAddr;
            //dst_cfg.dst_size = cfgEnc.dstBufferSize;
            //dst_cfg.exif_en = cfgEnc.enableEXIF;
            //
            //JPEG_MSG("go L:%d, %x, %d, %d !!", __LINE__, dst_cfg.dst_addr, dst_cfg.dst_size, dst_cfg.file_format);
            //
            //if (0 == jpeg_drv_enc_dst_buff(dst_cfg))
            //    return -EFAULT;
          
            // 4 .set ctrl config
            JPEG_MSG("[JPEGDRV]ENC_CFG: exif:%d, q:%d, DRI:%d !!\n", cfgEnc.enableEXIF, cfgEnc.encQuality, cfgEnc.restartInterval);

            jpeg_drv_enc_ctrl_cfg(cfgEnc.enableEXIF, cfgEnc.encQuality, cfgEnc.restartInterval);

            //memset(&ctrl_cfg, 0, sizeof(JpegDrvEncCtrlCfg));
            //
            //ctrl_cfg.quality = cfgEnc.encQuality;
            //ctrl_cfg.gmc_disable = cfgEnc.disableGMC;
            //ctrl_cfg.restart_interval = cfgEnc.restartInterval;
            //
            break;

        case JPEG_ENC_IOCTL_START:            
            JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Start!!\n");
            if(*pStatus != JPEG_ENC_PROCESS)
            {
                JPEG_WRN("Permission Denied! This process can not access encoder");
                return -EFAULT;
            }
            if(enc_status == 0)
            {
                JPEG_WRN("Encoder status is available, HOW COULD THIS HAPPEN ??");
                *pStatus = 0;
                return -EFAULT;
            }
            jpeg_drv_enc_start();            
            break;

        case JPEG_ENC_IOCTL_WAIT:
            JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Wait!!\n");            
            if(*pStatus != JPEG_ENC_PROCESS)
            {
                JPEG_WRN("Permission Denied! This process can not access encoder");
                return -EFAULT;
            }
            if(enc_status == 0)
            {
                JPEG_WRN("Encoder status is available, HOW COULD THIS HAPPEN ??");
                *pStatus = 0;
                return -EFAULT;
            }            
            if(copy_from_user(&enc_result, (void *)arg, sizeof(JPEG_ENC_DRV_OUT)))
            {
                JPEG_WRN("JPEG Encoder : Copy from user error\n");
                return -EFAULT;
            }

            //TODO:    ENC_DONE in REG_JPEG_ENC_INTERRUPT_STATUS need to set to 0 after read.
            jpeg_enc_wait_timeout = 0xFFFFFF;

#ifdef FPGA_VERSION

            do{
                _jpeg_enc_int_status = REG_JPEG_ENC_INTERRUPT_STATUS;
                jpeg_enc_wait_timeout--;
            }while(_jpeg_enc_int_status == 0 && jpeg_enc_wait_timeout > 0);

            if (jpeg_enc_wait_timeout == 0)
                JPEG_MSG("JPEG Encoder timeout\n");

            ret = jpeg_drv_enc_get_result(&file_size);

            //JPEG_MSG("Result : %d, Size : %u, addres : 0x%x\n", ret, file_size, ioread32(JPG_CODEC_BASE + 0x120));
            JPEG_MSG("Result : %d, Size : %u, addres : 0x%x\n", ret, file_size, ioread32(JPEG_ENC_BASE + 0x120));

            if(_jpeg_enc_int_status != 1)
            {
                jpeg_drv_enc_dump_reg();
            }
                
#else
            
              //set timeout
              timeout_jiff = enc_result.timeout* HZ / 1000;
              JPEG_MSG("[JPEGDRV]JPEG Encoder Time Jiffies : %ld\n", timeout_jiff);   
            
            if(jpeg_isr_enc_lisr()<0)
            {
              wait_event_interruptible_timeout(enc_wait_queue, _jpeg_enc_int_status, timeout_jiff);
              JPEG_MSG("[JPEGDRV]JPEG Encoder Wait done !!\n");   
            }
            else
            {
              JPEG_MSG("[JPEGDRV]JPEG Encoder already done !!\n");  
            }

            ret = jpeg_drv_enc_get_result(&file_size);

            JPEG_MSG("[JPEGDRV]Result : %d, Size : %u!!\n", ret, file_size);
            if(ret != 0)
            {
                jpeg_drv_enc_dump_reg();

                jpeg_drv_enc_warm_reset();
            }

#endif

            cycle_count = jpeg_drv_enc_get_cycle_count();
            
            if(copy_to_user(enc_result.fileSize, &file_size, sizeof(unsigned int)))
            {
                JPEG_MSG("[JPEGDRV]JPEG Encoder : Copy to user error (file size)\n");
                return -EFAULT;
            }
            if(copy_to_user(enc_result.result, &ret, sizeof(unsigned int)))
            {
                JPEG_MSG("[JPEGDRV]JPEG Encoder : Copy to user error (status)\n");
                return -EFAULT;            
            }
            if(copy_to_user(enc_result.cycleCount, &cycle_count, sizeof(unsigned int)))
            {
                JPEG_MSG("[JPEGDRV]JPEG Encoder : Copy to user error (cycle)\n");
                return -EFAULT;            
            }            
            break;
            
        case JPEG_ENC_IOCTL_DEINIT:
            JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Deinit!!\n");
            // copy input parameters
            if(*pStatus != JPEG_ENC_PROCESS)
            {
                JPEG_WRN("Permission Denied! This process can not access encoder");
                return -EFAULT;
            }

            if(enc_status == 0)
            {
                JPEG_WRN("Encoder status is available, HOW COULD THIS HAPPEN ??");
                *pStatus = 0;
                return -EFAULT;
            }
            jpeg_drv_enc_deinit();
            *pStatus = 0;   
            break;

        case JPEG_ENC_IOCTL_DUMP_REG:
            jpeg_drv_enc_dump_reg();
            break;

        default:
            JPEG_MSG("[JPEGDRV]JPEG ENC IOCTL NO THIS COMMAND\n");
                
        }
    return 0;
}


//--------------------------------------------------------------------------
//
//--------------------------------------------------------------------------

#ifdef CONFIG_COMPAT

static int compat_get_jpeg_dec_ioctl_wait_data(
            compat_JPEG_DEC_DRV_OUT __user *data32,
            JPEG_DEC_DRV_OUT __user *data)
{
    compat_long_t timeout;
    compat_uptr_t result;
    int err;

    err = get_user(timeout, &data32->timeout);
    err |= put_user(timeout, &data->timeout);
    err |= get_user(result, &data32->result);
    err |= put_user(compat_ptr(result), &data->result);
    return err;
}

static int compat_put_jpeg_dec_ioctl_wait_data(
            compat_JPEG_DEC_DRV_OUT __user *data32,
            JPEG_DEC_DRV_OUT __user *data)
{
    compat_long_t timeout;
    //compat_uptr_t result;
    int err;

    err = get_user(timeout, &data->timeout);
    err |= put_user(timeout, &data32->timeout);
    //err |= get_user(result, &data->result);
    //err |= put_user(result, &data32->result);
    return err;
}

static int compat_get_jpeg_dec_ioctl_chksum_data(
            compat_JpegDrvDecResult __user *data32,
            JpegDrvDecResult __user *data)
{
    compat_uptr_t pChksum;
    int err;

    err = get_user(pChksum, &data32->pChksum);
    err |= put_user(compat_ptr(pChksum), &data->pChksum);
    return err;
}

static int compat_put_jpeg_dec_ioctl_chksum_data(
            compat_JpegDrvDecResult __user *data32,
            JpegDrvDecResult __user *data)
{
    //compat_uptr_t pChksum;
    //int err;

    //err = get_user(pChksum, &data->pChksum);
    //err |= put_user(pChksum, &data32->pChksum);
    return 0;
}

static int compat_get_jpeg_enc_ioctl_wait_data(
            compat_JPEG_ENC_DRV_OUT __user *data32,
            JPEG_ENC_DRV_OUT __user *data)
{
    compat_long_t timeout;
    compat_uptr_t fileSize;
    compat_uptr_t result;
    compat_uptr_t cycleCount;
    int err;

    err = get_user(timeout, &data32->timeout);
    err |= put_user(timeout, &data->timeout);
    err |= get_user(fileSize, &data32->fileSize);
    err |= put_user(compat_ptr(fileSize), &data->fileSize);
    err |= get_user(result, &data32->result);
    err |= put_user(compat_ptr(result), &data->result);
    err |= get_user(cycleCount, &data32->cycleCount);
    err |= put_user(compat_ptr(cycleCount), &data->cycleCount);
    return err;
}

static int compat_put_jpeg_enc_ioctl_wait_data(
            compat_JPEG_ENC_DRV_OUT __user *data32,
            JPEG_ENC_DRV_OUT __user *data)
{
    compat_long_t timeout;
    //compat_uptr_t fileSize;
    //compat_uptr_t result;
    //compat_uptr_t cycleCount;
    int err;

    err = get_user(timeout, &data->timeout);
    err |= put_user(timeout, &data32->timeout);
    //err |= get_user(fileSize, &data->fileSize);
    //err |= put_user(fileSize, &data32->fileSize);
    //err |= get_user(result, &data->result);
    //err |= put_user(result, &data32->result);
    //err |= get_user(cycleCount, &data->cycleCount);
    //err |= put_user(cycleCount, &data32->cycleCount);
    return err;
}

static long compat_jpeg_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long ret;

    if (!filp->f_op || !filp->f_op->unlocked_ioctl)
        return -ENOTTY;

    switch (cmd) 
    {
        case COMPAT_JPEG_DEC_IOCTL_WAIT:
        {
            compat_JPEG_DEC_DRV_OUT __user *data32;
            JPEG_DEC_DRV_OUT __user *data;
            int err;
    
            data32 = compat_ptr(arg);
            data = compat_alloc_user_space(sizeof(*data));
            if (data == NULL)
                return -EFAULT;
    
            err = compat_get_jpeg_dec_ioctl_wait_data(data32, data);
            if (err)
                return err;
            ret = filp->f_op->unlocked_ioctl(filp, JPEG_DEC_IOCTL_WAIT,(unsigned long)data);
            err = compat_put_jpeg_dec_ioctl_wait_data(data32, data);
            return ret ? ret : err;
        }
        case COMPAT_JPEG_DEC_IOCTL_CHKSUM:
        {
            compat_JpegDrvDecResult __user *data32;
            JpegDrvDecResult __user *data;
            int err;
    
            data32 = compat_ptr(arg);
            data = compat_alloc_user_space(sizeof(*data));
            if (data == NULL)
                return -EFAULT;
    
            err = compat_get_jpeg_dec_ioctl_chksum_data(data32, data);
            if (err)
                return err;
            ret = filp->f_op->unlocked_ioctl(filp, JPEG_DEC_IOCTL_CHKSUM,(unsigned long)data);
            err = compat_put_jpeg_dec_ioctl_chksum_data(data32, data);
            return ret ? ret : err;
        }
        case COMPAT_JPEG_ENC_IOCTL_WAIT:
        {
            compat_JPEG_ENC_DRV_OUT __user *data32;
            JPEG_ENC_DRV_OUT __user *data;
            int err;
    
            data32 = compat_ptr(arg);
            data = compat_alloc_user_space(sizeof(*data));
            if (data == NULL)
                return -EFAULT;
    
            err = compat_get_jpeg_enc_ioctl_wait_data(data32, data);
            if (err)
                return err;
            ret = filp->f_op->unlocked_ioctl(filp, JPEG_ENC_IOCTL_WAIT,(unsigned long)data);
            err = compat_put_jpeg_enc_ioctl_wait_data(data32, data);
            return ret ? ret : err;
        }
        case JPEG_DEC_IOCTL_INIT:
        case JPEG_DEC_IOCTL_START:
        case JPEG_DEC_IOCTL_DEINIT:
        case JPEG_DEC_IOCTL_DUMP_REG:
        case JPEG_ENC_IOCTL_INIT: 
        case JPEG_ENC_IOCTL_DEINIT:
        case JPEG_ENC_IOCTL_START:
            return filp->f_op->unlocked_ioctl(filp, cmd, arg);
            
        case JPEG_DEC_IOCTL_CONFIG:
        case JPEG_DEC_IOCTL_RESUME:
        case JPEG_DEC_IOCTL_FLUSH_CMDQ:
        case JPEG_ENC_IOCTL_CONFIG:
            return filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
            
        default:
            return -ENOIOCTLCMD;    
    }
}
#endif

//static int jpeg_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
static long jpeg_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch(cmd)
    {
        case JPEG_DEC_IOCTL_INIT:
        case JPEG_DEC_IOCTL_CONFIG:
        case JPEG_DEC_IOCTL_START:
        case JPEG_DEC_IOCTL_RESUME:
        case JPEG_DEC_IOCTL_WAIT:
        case JPEG_DEC_IOCTL_DEINIT:
        case JPEG_DEC_IOCTL_DUMP_REG:
        case JPEG_DEC_IOCTL_FLUSH_CMDQ:
            return jpeg_dec_ioctl(cmd, arg, file);

        case JPEG_ENC_IOCTL_INIT: 
        case JPEG_ENC_IOCTL_CONFIG:
        case JPEG_ENC_IOCTL_WAIT:
        case JPEG_ENC_IOCTL_DEINIT:
        case JPEG_ENC_IOCTL_START:
            return jpeg_enc_ioctl(cmd, arg, file);
            
        default :
            break; 
    }
    
    return -EINVAL;
}

static int jpeg_open(struct inode *inode, struct file *file)
{
    unsigned int *pStatus;
    //Allocate and initialize private data
    file->private_data = kmalloc(sizeof(unsigned int) , GFP_ATOMIC);

    if(NULL == file->private_data)
    {
        JPEG_WRN("Not enough entry for JPEG open operation\n");
        return -ENOMEM;
    }

    pStatus = (unsigned int *)file->private_data;
    *pStatus = 0;
    
    return 0;
}

static ssize_t jpeg_read(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
    JPEG_MSG("jpeg driver read\n");
    return 0;
}

static int jpeg_release(struct inode *inode, struct file *file)
{
    if(NULL != file->private_data)
    {
        kfree(file->private_data);
        file->private_data = NULL;
    }
    return 0;
}

static int jpeg_flush(struct file * a_pstFile , fl_owner_t a_id)
{
    unsigned int *pStatus;

    pStatus = (unsigned int*)a_pstFile->private_data;

    if(NULL == pStatus)
    {
        JPEG_WRN("Private data is null in flush operation. HOW COULD THIS HAPPEN ??\n");
        return -EFAULT;
    }

    if (*pStatus == JPEG_ENC_PROCESS)
    {
        if(enc_status != 0) 
        {
            JPEG_WRN("Error! Enable error handling for jpeg encoder");
            jpeg_drv_enc_deinit();
        }
    }
#ifdef JPEG_DEC_DRIVER    
    else if (*pStatus == JPEG_DEC_PROCESS)
    {
        if(dec_status != 0) 
        {
            JPEG_WRN("Error! Enable error handling for jpeg decoder");
            jpeg_drv_dec_deinit();
        }
    }
#endif        

    return 0;
}

/* Kernel interface */
static struct file_operations jpeg_fops = {
	.owner		= THIS_MODULE,
	//.ioctl		= jpeg_ioctl,
	.unlocked_ioctl = jpeg_unlocked_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = compat_jpeg_ioctl,
#endif
	.open		= jpeg_open,
	.release	= jpeg_release,
	.flush		= jpeg_flush,
	.read       = jpeg_read,
};


const long jpeg_dev_get_encoder_base_VA(void)
{
    return gJpegqDev.encRegBaseVA;
}


const long jpeg_dev_get_decoder_base_VA(void)
{
    return gJpegqDev.decRegBaseVA;
}


static int jpeg_probe(struct platform_device *pdev)
{

#ifdef CONFIG_OF

    int new_count;
    struct JpegDeviceStruct* jpegDev;
    struct device_node *node = NULL;

    new_count = nrJpegDevs + 1;
    gJpegqDevs = krealloc(gJpegqDevs, 
        sizeof(struct JpegDeviceStruct) * new_count, GFP_KERNEL);
    if (!gJpegqDevs) {
        dev_err(&pdev->dev, "Unable to allocate cam_isp_devs\n");
        return -ENOMEM;
    }

    jpegDev = &(gJpegqDevs[nrJpegDevs]);
    jpegDev->pDev = &pdev->dev;

    memset(&gJpegqDev, 0x0, sizeof(JpegDeviceStruct));

    node = of_find_compatible_node(NULL, NULL, "mediatek,JPGENC");
    jpegDev->encRegBaseVA = (unsigned long)of_iomap(node, 0);
    jpegDev->encIrqId = irq_of_parse_and_map(node, 0);

    node = of_find_compatible_node(NULL, NULL, "mediatek,JPGDEC");
    jpegDev->decRegBaseVA = (unsigned long)of_iomap(node, 0);
    jpegDev->decIrqId = irq_of_parse_and_map(node, 0);

    gJpegqDev = *jpegDev;

#else

    gJpegqDev.encRegBaseVA = (0L | 0xF7003000);
    gJpegqDev.decRegBaseVA = (0L | 0xF7004000);
    gJpegqDev.encIrqId = JPGENC_IRQ_BIT_ID;
    gJpegqDev.decIrqId = JPGDEC_IRQ_BIT_ID;

    gJpegqDev.pDev = &pdev->dev;

#endif

#ifdef JPEG_DEV    
	int ret;
    struct class_device *class_dev = NULL;
    
    JPEG_MSG("-------------jpeg driver probe-------\n");
	ret = alloc_chrdev_region(&jpeg_devno, 0, 1, JPEG_DEVNAME);

	if(ret)
	{
	    JPEG_ERR("Error: Can't Get Major number for JPEG Device\n");
	}
	else
	{
	    JPEG_MSG("Get JPEG Device Major number (%d)\n", jpeg_devno);
    }

	jpeg_cdev = cdev_alloc();
    jpeg_cdev->owner = THIS_MODULE;
	jpeg_cdev->ops = &jpeg_fops;

	ret = cdev_add(jpeg_cdev, jpeg_devno, 1);

    jpeg_class = class_create(THIS_MODULE, JPEG_DEVNAME);
    class_dev = (struct class_device *)device_create(jpeg_class, NULL, jpeg_devno, NULL, JPEG_DEVNAME);
#else
    
    proc_create("mtk_jpeg", 0, NULL, &jpeg_fops);

#endif

    spin_lock_init(&jpeg_dec_lock);
    spin_lock_init(&jpeg_enc_lock);

    // initial codec, register codec ISR
    dec_status = 0;
    enc_status = 0;
    _jpeg_dec_int_status = 0;
    _jpeg_enc_int_status = 0;
    _jpeg_dec_mode = 0;

#ifndef FPGA_VERSION

#ifdef JPEG_DEC_DRIVER
    init_waitqueue_head(&dec_wait_queue);
#endif    
    init_waitqueue_head(&enc_wait_queue);  
    
    //mt6575_irq_set_sens(MT6575_JPEG_CODEC_IRQ_ID, MT65xx_LEVEL_SENSITIVE);
    //mt6575_irq_set_polarity(MT6575_JPEG_CODEC_IRQ_ID, MT65xx_POLARITY_LOW);
    //mt6575_irq_unmask(MT6575_JPEG_CODEC_IRQ_ID);
    JPEG_MSG("request JPEG Encoder IRQ \n");
    enable_irq(gJpegqDev.encIrqId);
    if(request_irq(gJpegqDev.encIrqId, jpeg_drv_enc_isr, IRQF_TRIGGER_LOW, "jpeg_enc_driver" , NULL))
    //if(request_irq(JPGENC_IRQ_BIT_ID, jpeg_drv_enc_isr, /*IRQF_TRIGGER_RISING*/ IRQF_TRIGGER_HIGH, "jpeg_enc_driver" , NULL))
    //if(request_irq(JPGENC_IRQ_BIT_ID, jpeg_drv_enc_isr, IRQF_TRIGGER_RISING , "jpeg_enc_driver" , NULL))
    {
        JPEG_ERR("JPEG ENC Driver request irq failed\n");
    }

#ifdef JPEG_DEC_DRIVER    
    enable_irq(gJpegqDev.decIrqId);
    JPEG_MSG("request JPEG Decoder IRQ \n");
    //if(request_irq(JPGDEC_IRQ_BIT_ID, jpeg_drv_dec_isr, IRQF_TRIGGER_LOW, "jpeg_dec_driver" , NULL))
    //if(request_irq(JPGDEC_IRQ_BIT_ID, jpeg_drv_dec_isr, /*IRQF_TRIGGER_RISING*/ IRQF_TRIGGER_HIGH, "jpeg_dec_driver" , NULL))
    //if(request_irq(JPGDEC_IRQ_BIT_ID, jpeg_drv_dec_isr, IRQF_TRIGGER_RISING , "jpeg_dec_driver" , NULL))
    if(request_irq(gJpegqDev.decIrqId, jpeg_drv_dec_isr, IRQF_TRIGGER_FALLING , "jpeg_dec_driver" , NULL))
    {
        JPEG_ERR("JPEG DEC Driver request irq failed\n");
    }    
#endif    
    
#endif
	JPEG_MSG("JPEG Probe Done\n");

#ifdef JPEG_DEV    
	NOT_REFERENCED(class_dev);
#endif
	return 0;
}

static int jpeg_remove(struct platform_device *pdev)
{
	JPEG_MSG("JPEG Codec remove\n");
	//unregister_chrdev(JPEGDEC_MAJOR, JPEGDEC_DEVNAME);
#ifndef FPGA_VERSION
	free_irq(JPGENC_IRQ_BIT_ID, NULL);
#ifdef JPEG_DEC_DRIVER	
	free_irq(JPGDEC_IRQ_BIT_ID, NULL);
#endif
	
#endif
	JPEG_MSG("Done\n");
	return 0;
}

static void jpeg_shutdown(struct platform_device *pdev)
{
	JPEG_MSG("JPEG Codec shutdown\n");
	/* Nothing yet */
}

/* PM suspend */
static int jpeg_suspend(struct platform_device *pdev, pm_message_t mesg)
{
#ifdef JPEG_DEC_DRIVER   
    jpeg_drv_dec_deinit();
#endif    
    jpeg_drv_enc_deinit();
    return 0;
}

/* PM resume */
static int jpeg_resume(struct platform_device *pdev)
{
    return 0;
}


static int jpeg_pm_suspend(struct device *pDevice)
{
    struct platform_device *pdev = to_platform_device(pDevice);
    BUG_ON(pdev == NULL);

    return jpeg_suspend(pdev, PMSG_SUSPEND);
}

static int jpeg_pm_resume(struct device *pDevice)
{
    struct platform_device *pdev = to_platform_device(pDevice);
    BUG_ON(pdev == NULL);

    return jpeg_resume(pdev);
}

static int jpeg_pm_restore_noirq(struct device *pDevice)
{
    return 0;
}


static struct dev_pm_ops jpeg_pm_ops = {
    .suspend       = jpeg_pm_suspend,
    .resume        = jpeg_pm_resume,
    .freeze        = NULL,
    .thaw          = NULL,
    .poweroff      = NULL,
    .restore       = NULL,
    .restore_noirq = jpeg_pm_restore_noirq,
};

static struct platform_driver jpeg_driver = {
	.probe		= jpeg_probe,
	.remove		= jpeg_remove,
	.shutdown	= jpeg_shutdown,
	.suspend	= jpeg_suspend,
	.resume		= jpeg_resume,
	.driver     = {
	              .name = JPEG_DEVNAME,
                .pm = &jpeg_pm_ops,
#ifdef CONFIG_OF
                .of_match_table = jpeg_of_ids,
#endif
	},
};

static void jpeg_device_release(struct device *dev)
{
	// Nothing to release? 
}

static u64 jpegdec_dmamask = ~(u32)0;

static struct platform_device jpeg_device = {
	.name	 = JPEG_DEVNAME,
	.id      = 0,
	.dev     = {
		.release = jpeg_device_release,
		.dma_mask = &jpegdec_dmamask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources = 0,
};

static int __init jpeg_init(void)
{
    int ret;

    JPEG_MSG("JPEG Codec initialize\n");
	
    #if 0
    JPEG_MSG("Register the JPEG Codec device\n");
	if(platform_device_register(&jpeg_device))
	{
        JPEG_ERR("failed to register jpeg codec device\n");
        ret = -ENODEV;
        return ret;
	}
    #endif

    JPEG_MSG("Register the JPEG Codec driver\n");    
    if(platform_driver_register(&jpeg_driver))
    {
        JPEG_ERR("failed to register jpeg codec driver\n");
        platform_device_unregister(&jpeg_device);
        ret = -ENODEV;
        return ret;
    }
    cmdqCoreRegisterCB(CMDQ_GROUP_JPEG,
                       cmdqJpegClockOn,
                       cmdqJpegDumpInfo,
                       cmdqJpegResetEng,
                       cmdqJpegClockOff);
    return 0;
}

static void __exit jpeg_exit(void)
{
#ifdef JPEG_DEV    
    cdev_del(jpeg_cdev);
    unregister_chrdev_region(jpeg_devno, 1);
    device_destroy(jpeg_class, jpeg_devno);
    class_destroy(jpeg_class);
#else
    remove_proc_entry("mtk_jpeg", NULL);
#endif
    cmdqCoreRegisterCB(CMDQ_GROUP_JPEG,
                       NULL,
                       NULL,
                       NULL,
                       NULL);
                       	
    //JPEG_MSG("Unregistering driver\n");
    platform_driver_unregister(&jpeg_driver);
    platform_device_unregister(&jpeg_device);


    JPEG_MSG("Done\n");
}

module_init(jpeg_init);
module_exit(jpeg_exit);
MODULE_AUTHOR("Hao-Ting Huang <otis.huang@mediatek.com>");
MODULE_DESCRIPTION("MT6582 JPEG Codec Driver");
MODULE_LICENSE("GPL");
