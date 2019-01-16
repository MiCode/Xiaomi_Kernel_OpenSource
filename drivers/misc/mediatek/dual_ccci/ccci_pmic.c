/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccci_pmic.c
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   MT65XX CCCI PMIC Driver
 *
 ****************************************************************************/


#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <asm/dma-mapping.h>

#include <ccci.h>
#include <ccci_pmic.h>


typedef enum
{
    VSIM_1_3V = 0,
    VSIM_1_5V,
    VSIM_1_8V,
    VSIM_2_5V,
    VSIM_2_8V,
    VSIM_3_0V,
    VSIM_3_3V,
    VSIM_1_2V
}vsim_sel_enum;


#define  CONFIG_CCCI_PMIC_DEBUG_MSG
#define  CCCI_PMIC_DEVNAME  "ccci_pmic"
#define  CCCI_PMIC_QSIZE     (10)

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
static spinlock_t          pmic_spinlock = SPIN_LOCK_UNLOCKED;
#else
static DEFINE_SPINLOCK(pmic_spinlock);
#endif

static shared_mem_pmic_t  *ccci_pmic_shared_mem;
static int                 ccci_pmic_shared_mem_phys_addr;

static dev_t               pmic_dev_num;
static struct cdev         pmic_cdev;
static struct kfifo        pmic_fifo;

static void ccci_pmic_read(struct work_struct *ws);
static DECLARE_WORK       (ccci_pmic_read_work, ccci_pmic_read);

int g_VSIM_1 = 2;
int g_VSIM_2 = 2;

static void ccci_pmic_read(struct work_struct *ws)
{
    unsigned long exec_time_1=0, exec_time_2=0;
    unsigned long arg;
    unsigned long flag;
    // mt6326_check_power();
    
    /*
     *    0. Start Timer
     */
    // exec_time_1 = sampletrigger(0, 0, 0);
    exec_time_1 = 0;
                
    //#ifdef CONFIG_CCCI_PMIC_DEBUG_MSG
    //printk("************* ccci_pmic_callback : exec_time_1 = %ld ms\r\n", exec_time_1);
    CCCI_PMIC_MSG("ccci_pmic_callback : exec_time_1 = %ld ms\r\n", exec_time_1);
    //#endif
    
    /*
     *    1. Parsing CCCI Message Format which received from MD site
     */
    spin_lock_irqsave(&pmic_spinlock,flag);
    if (kfifo_out(&pmic_fifo, (unsigned char *) &arg, sizeof(unsigned long)) != sizeof(unsigned long))
    {
    spin_unlock_irqrestore(&pmic_spinlock,flag);
        CCCI_MSG("<pmic>Unable to get new request from fifo\n");
        return;
    }
    spin_unlock_irqrestore(&pmic_spinlock,flag);
    ccci_pmic_shared_mem->ccci_msg.pmic6326_op         =  arg & 0x000000FF;        
    ccci_pmic_shared_mem->ccci_msg.pmic6326_type     = (arg & 0x0000FF00) >> 8; 
    ccci_pmic_shared_mem->ccci_msg.pmic6326_param1    = (arg & 0x00FF0000) >> 16;
    ccci_pmic_shared_mem->ccci_msg.pmic6326_param2    = (arg & 0xFF000000) >> 24;
    
    /*
     *    2. Execute the operation (API) that MD site called
     */        
    switch(ccci_pmic_shared_mem->ccci_msg.pmic6326_op) 
    {
        case PMIC6326_VSIM_ENABLE :
        //#ifdef CONFIG_CCCI_PMIC_DEBUG_MSG
        //printk("ccci_pmic_callback : PMIC6326_VSIM_ENABLE\n");
        CCCI_PMIC_MSG("PMIC6326_VSIM_ENABLE\n");
        //#endif
            //pmic_vsim_enable(ccci_pmic_shared_mem->ccci_msg.pmic6326_param1);
            
            break;

        case PMIC6326_VSIM_SET_AND_ENABLE :
        //#ifdef CONFIG_CCCI_PMIC_DEBUG_MSG
        //printk("ccci_pmic_callback : PMIC6326_VSIM_SET_AND_ENABLE\n");
        CCCI_PMIC_MSG("PMIC6326_VSIM_SET_AND_ENABLE\n");
        //#endif
            //pmic_vsim_cal(0);
            //pmic_vsim_sel(ccci_pmic_shared_mem->ccci_msg.pmic6326_param1);
            //pmic_vsim_enable(KAL_TRUE);

            break;

    case PMIC6236_LOCK :
        //#ifdef CONFIG_CCCI_PMIC_DEBUG_MSG
        //printk("ccci_pmic_callback : PMIC6236_LOCK\n");
        CCCI_PMIC_MSG("PMIC6236_LOCK\n");
        //#endif
        break;

    case PMIC6326_UNLOCK :
        //#ifdef CONFIG_CCCI_PMIC_DEBUG_MSG
        //printk("ccci_pmic_callback : PMIC6326_UNLOCK\n");
        CCCI_PMIC_MSG("CONFIG_CCCI_PMIC_DEBUG_MSG\n");
        //#endif
        break;

    case PMIC6326_VSIM2_ENABLE :
        //#ifdef CONFIG_CCCI_PMIC_DEBUG_MSG
        //printk("ccci_pmic_callback : PMIC6326_VSIM2_ENABLE\n");
        CCCI_PMIC_MSG("PMIC6326_VSIM2_ENABLE\n");
        //#endif
            //pmic_vsim2_enable(ccci_pmic_shared_mem->ccci_msg.pmic6326_param1);

            break;    

    case PMIC6326_VSIM2_SET_AND_ENABLE :
        //#ifdef CONFIG_CCCI_PMIC_DEBUG_MSG
        //printk("ccci_pmic_callback : PMIC6326_VSIM2_SET_AND_ENABLE\n");
        CCCI_PMIC_MSG("PMIC6326_VSIM2_SET_AND_ENABLE\n");
        //#endif
        //pmic_vsim2_sel(ccci_pmic_shared_mem->ccci_msg.pmic6326_param1);
        //pmic_vsim2_enable(KAL_TRUE);

            break;        

    default :
        //#ifdef CONFIG_CCCI_PMIC_DEBUG_MSG
        //printk("ccci_pmic_callback : ERROR op !\n");
        CCCI_PMIC_MSG("Error op\n");
        //#endif
    }

    
    // exec_time_2 = sampletrigger(0, 0, 1);
    exec_time_2 = 10000;
    
    //#ifdef CONFIG_CCCI_PMIC_DEBUG_MSG
    //printk("************* ccci_pmic_callback : exec_time_2 = %ld ms\r\n", exec_time_2);
    CCCI_PMIC_MSG("exec_time_2 = %ld ms\r\n", exec_time_2);
    //#endif
    
    exec_time_2 = (exec_time_2 - exec_time_1)/1000;
    //#ifdef CONFIG_CCCI_PMIC_DEBUG_MSG
    //printk("************* ccci_pmic_callback : exec_time = %ld ms\r\n", exec_time_2);
    CCCI_PMIC_MSG("exec_time = %ld ms\r\n", exec_time_2);
    //#endif
    
    /*
     *    3. AP site write Message to share memory "ccci_pmic_shared_mem"
     */        
    ccci_pmic_shared_mem->ccci_msg.pmic6326_type = (kal_uint8)PMIC6326_RES;
    ccci_pmic_shared_mem->ccci_msg_info.pmic6326_exec_time = exec_time_2;
    
    ccci_pmic_shared_mem->ccci_msg = ccci_pmic_shared_mem->ccci_msg;
    ccci_pmic_shared_mem->ccci_msg_info = ccci_pmic_shared_mem->ccci_msg_info;        
    
    /*
     *    4. Inform AP CCCI driver 
     */
    //ccci_write_mailbox(CCCI_PMIC_TX, 0);  
}

    
static void ccci_pmic_callback(CCCI_BUFF_T *buff, void *private_data)
{
    unsigned long flag;
    //#ifdef CONFIG_CCCI_PMIC_DEBUG_MSG
    //printk("CCCI_PMIC: ccci_pmic_callback\n");
    CCCI_PMIC_MSG("callback++\n");
    //#endif
   
    spin_lock_irqsave(&pmic_spinlock,flag);
    if (buff->channel == CCCI_PMIC_RX)        
    {
        if (kfifo_in(&pmic_fifo, (unsigned char *) &buff->data[1], sizeof(unsigned long)) != sizeof(unsigned long))
        {
        spin_unlock_irqrestore(&pmic_spinlock,flag);
            //CCCI_LOGE("CCCI_PMIC: Unable to put new request into fifo\n");
            CCCI_PMIC_MSG("callback--0\n");
            return;
        }
        spin_unlock_irqrestore(&pmic_spinlock,flag);
        schedule_work(&ccci_pmic_read_work);
    }
    CCCI_PMIC_MSG("callback--1\n");
}


static int ccci_pmic_start(void)
{
    int size;
    #if 0
    unsigned int i=0;
    unsigned long exec_time_1=0, exec_time_2=0;
    
    /* Test Timer*/
    exec_time_1 = sampletrigger(0, 0, 0);
    printk("************* ccci_pmic_callback : exec_time_1 = %ld ms\r\n", exec_time_1);

    for(i=0;i<1000;i++)
    {
        printk("************* ");
    }

    exec_time_2 = sampletrigger(0, 0, 1);
    printk("************* ccci_pmic_callback : exec_time_2 = %ld ms\r\n", exec_time_2);
        
    exec_time_2 = (exec_time_2 - exec_time_1)/1000;
    printk("************* ccci_pmic_callback : exec_time = %ld ms\r\n", exec_time_2);
    #endif
    
    //#ifdef CONFIG_CCCI_PMIC_DEBUG_MSG
    //printk("CCCI_PMIC: ccci_pmic_start ****************************\n");
    CCCI_PMIC_MSG("ccci_pmic_start\n");
    //#endif

     if (0!= kfifo_alloc(&pmic_fifo,sizeof(unsigned long) * CCCI_PMIC_QSIZE, GFP_KERNEL))
     {
        //CCCI_LOGE("CCCI_PMIC: Unable to create fifo\n");
        CCCI_MSG("<pmic> Unable to create fifo\n");     
        return -EFAULT;
     }
 
    //ASSERT(ccci_pmic_setup((int*)&ccci_pmic_shared_mem, &ccci_pmic_shared_mem_phys_addr, &size) == CCCI_SUCCESS);    
    //printk("CCCI PMIC %x:%x:%d\n", (unsigned int)ccci_pmic_shared_mem, (unsigned int)ccci_pmic_shared_mem_phys_addr, size);
    CCCI_PMIC_MSG("%x:%x:%d\n", (unsigned int)ccci_pmic_shared_mem, (unsigned int)ccci_pmic_shared_mem_phys_addr, size);
    //ASSERT(ccci_register(CCCI_PMIC_RX, ccci_pmic_callback, NULL) == CCCI_SUCCESS);
    //ASSERT(ccci_register(CCCI_PMIC_TX, ccci_pmic_callback, NULL) == CCCI_SUCCESS);
    
    return 0;
}

static void ccci_pmic_stop(void)
{
    //#ifdef CONFIG_CCCI_PMIC_DEBUG_MSG
    //printk("CCCI_PMIC: ccci_pmic_stop ****************************\n");
    CCCI_PMIC_MSG("pmic_stop\n");
    //#endif
    ccci_pmic_shared_mem = NULL;
    ccci_pmic_shared_mem_phys_addr = 0;
    //ASSERT(ccci_unregister(CCCI_PMIC_RX) == CCCI_SUCCESS);
    //ASSERT(ccci_unregister(CCCI_PMIC_TX) == CCCI_SUCCESS);

    kfifo_free(&pmic_fifo);
}

static struct file_operations pmic_fops = 
{
    .owner   = THIS_MODULE,
    //.ioctl   = ccci_pmic_ioctl,
    //.open    = ccci_pmic_open,   
    //.release = ccci_pmic_release,
};

 int __init ccci_pmic_init(void)
{
    int ret;
    
    if (alloc_chrdev_region(&pmic_dev_num, 0, 1, CCCI_PMIC_DEVNAME))
    {
        //printk(KERN_ERR "CCCI_PMIC: Device major number allocation failed\n");
        CCCI_MSG("<pmic>Device major number allocation failed\n");
        return -EAGAIN;
    }

    cdev_init(&pmic_cdev, &pmic_fops);
    pmic_cdev.owner = THIS_MODULE;
    ret = cdev_add(&pmic_cdev, pmic_dev_num, 1);
    if (ret)
    {
        //printk(KERN_ERR "CCCI_PMIC: Char device add failed\n");
        CCCI_MSG("<pmic>Char device add failed\n");
        return ret;
    }

    ret = ccci_pmic_start();
    if (ret)
    {
        //printk(KERN_ERR "CCCI_PMIC: Unable to initialize environment\n");
        CCCI_MSG("<pmic>Unable to initialize environment\n");
        return ret;
    }

    //printk("CCCI_PMIC: Init complete, device major number = %d\n", MAJOR(pmic_dev_num));
    CCCI_PMIC_MSG("Init complete, device major number = %d\n", MAJOR(pmic_dev_num));
    
    return 0;
}

 void __exit ccci_pmic_exit(void)
{
    ccci_pmic_stop();
    
    cdev_del(&pmic_cdev);
    unregister_chrdev_region(pmic_dev_num, 1);
}
