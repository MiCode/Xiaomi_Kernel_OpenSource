#ifndef AUDDRV_NXPSPK_H
#define AUDDRV_NXPSPK_H

#include <mach/mt_typedefs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/xlog.h>
#include <mach/irqs.h>
#include <mach/mt_irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <mach/mt_reg_base.h>
#include <asm/div64.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/i2c.h>
#include <mach/mt_boot.h>
#include <mach/eint.h>

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/


/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/
static char const *const kAudioNXPSpkName = "/dev/nxpspk";


typedef struct
{
    unsigned char data0;
    unsigned char data1;
    unsigned char data2;
    unsigned char data3;
} Aud_Buffer_Control;

//below is control message
#define AUD_NXP_IOC_MAGIC 'C'

#define SET_NXP_REG         _IOWR(AUD_NXP_IOC_MAGIC, 0x00, Aud_Buffer_Control*)
#define GET_NXP_REG         _IOWR(AUD_NXP_IOC_MAGIC, 0x01, Aud_Buffer_Control*)

/* Pre-defined definition */
#define NXP_DEBUG_ON
#define NXP_DEBUG_ARRAY_ON
#define NXP_DEBUG_FUNC_ON


//Log define
#define NXP_INFO(fmt,arg...)           printk("<<-NXP-INFO->> "fmt"\n",##arg)
#define NXP_ERROR(fmt,arg...)          printk("<<-NXP-ERROR->> "fmt"\n",##arg)
#define NXP_DEBUG(fmt,arg...)          do{\
                                         if(NXP_DEBUG_ON)\
                                         printk("<<-NXP-DEBUG->> [%d]"fmt"\n",__LINE__, ##arg);\
                                       }while(0)
#define NXP_DEBUG_ARRAY(array, num)    do{\
                                         s32 i;\
                                         u8* a = array;\
                                         if(NXP_DEBUG_ARRAY_ON)\
                                         {\
                                            printk("<<-NXP-DEBUG-ARRAY->>\n");\
                                            for (i = 0; i < (num); i++)\
                                            {\
                                                printk("%02x   ", (a)[i]);\
                                                if ((i + 1 ) %10 == 0)\
                                                {\
                                                    printk("\n");\
                                                }\
                                            }\
                                            printk("\n");\
                                        }\
                                       }while(0)
#define NXP_DEBUG_FUNC()               do{\
                                         if(NXP_DEBUG_FUNC_ON)\
                                         printk("<<-NXP-FUNC->> Func:%s@Line:%d\n",__func__,__LINE__);\
                                       }while(0)
#define NXP_SWAP(x, y)                 do{\
                                         typeof(x) z = x;\
                                         x = y;\
                                         y = z;\
                                       }while (0)

//****************************PART4:UPDATE define*******************************
#define TFA9890_DEVICEID   0x0080

//Error no
#define ERROR_NO_FILE           2   //ENOENT
#define ERROR_FILE_READ         23  //ENFILE
#define ERROR_FILE_TYPE         21  //EISDIR
#define ERROR_GPIO_REQUEST      4   //EINTR
#define ERROR_I2C_TRANSFER      5   //EIO
#define ERROR_NO_RESPONSE       16  //EBUSY
#define ERROR_TIMEOUT           110 //ETIMEDOUT

#endif


