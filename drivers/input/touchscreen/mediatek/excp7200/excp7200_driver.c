#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif 
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/dma-mapping.h>

#include "tpd_custom_excp7200.h"
#include "tpd.h"
#include <cust_eint.h>

#ifndef TPD_NO_GPIO 
#include "cust_gpio_usage.h"
#endif

#define ABS(x)                  ((x<0)?-x:x)
#define MAX_I2C_LEN		10


extern struct tpd_device *tpd;

static int tpd_flag = 0;
static int tpd_halt=0;
static u8 *I2CDMABuf_va = NULL;
static u32 I2CDMABuf_pa = NULL;
static struct task_struct *thread = NULL;
static DECLARE_WAIT_QUEUE_HEAD(waiter);

#ifdef TPD_HAVE_BUTTON 
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT]   = TPD_WARP_END;
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX;
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

static void tpd_eint_interrupt_handler(void);
static int touch_event_handler(void *unused);
static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int tpd_i2c_remove(struct i2c_client *client);
#if 0
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);
#endif
static struct i2c_client *i2c_client = NULL;
static const struct i2c_device_id tpd_i2c_id[] = {{"mtk-tpd",0},{}};
static unsigned short force[] = {0, 0x08, I2C_CLIENT_END,I2C_CLIENT_END};
static const unsigned short * const forces[] = { force, NULL };
static struct i2c_client_address_data addr_data = { .forces = forces,};
struct i2c_driver tpd_i2c_driver = {                       
    .probe = tpd_i2c_probe,                                   
    .remove = tpd_i2c_remove,                           
    .detect = tpd_i2c_detect,                           
    .driver.name = "mtk-tpd", 
    .id_table = tpd_i2c_id,                             
    .address_data = &addr_data,                        
}; 

static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {
    strcpy(info->type, "mtk-tpd");
    return 0;
}

static int tpd_i2c_write(struct i2c_client *client, const uint8_t *buf, int len)
{
	int i = 0;
	for(i = 0 ; i < len; i++)
	{
		I2CDMABuf_va[i] = buf[i];
	}

	if(len < 8)
	{
                client->addr = client->addr & I2C_MASK_FLAG | I2C_ENEXT_FLAG;
		return i2c_master_send(client, buf, len);
	}
	else
	{
                client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
		return i2c_master_send(client, I2CDMABuf_pa, len);
	}    
}

static int tpd_i2c_read(struct i2c_client *client, uint8_t *buf, int len)
{
    int i = 0, ret = 0;
    
    if(len < 8)
    {
        client->addr = client->addr & I2C_MASK_FLAG | I2C_ENEXT_FLAG;
        return i2c_master_recv(client, buf, len);
    }
    else
    {
        client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
        ret = i2c_master_recv(client, I2CDMABuf_pa, len);
    
        if(ret < 0)
        {
            return ret;
        }
    
        for(i = 0; i < len; i++)
        {
            buf[i] = I2CDMABuf_va[i];
        }
    }
    return ret;
}

static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id) {             
    int err = 0, ret = -1;
    u8 cmdbuf[MAX_I2C_LEN]={0x03, 0x03, 0x0A, 0x01, 0x41, 0, 0, 0, 0, 0};
    
    
    #ifdef TPD_NO_GPIO
    u16 temp;
    temp = *(volatile u16 *) TPD_RESET_PIN_ADDR;
    temp = temp | 0x40;
    *(volatile u16 *) TPD_RESET_PIN_ADDR = temp;
    #endif
    i2c_client = client;
    
    printk("MediaTek touch panel i2c probe\n");
    
    #ifndef TPD_NO_GPIO 

    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
    msleep(10);  
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
   
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
    #endif 

    msleep(50);
    
    I2CDMABuf_va = (u8 *)dma_alloc_coherent(NULL, 4096, &I2CDMABuf_pa, GFP_KERNEL);
    if(!I2CDMABuf_va)
    {
	printk("Allocate Touch DMA I2C Buffer failed!\n");
	return -1;
    }

    i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
    ret = tpd_i2c_write(i2c_client, cmdbuf, 10);
    i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;
    if (ret != sizeof(cmdbuf))
    {
        TPD_DEBUG("[mtk-tpd] i2c write communcate error: 0x%x\n", ret);
        return -1;
    }
	
    thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
    if (IS_ERR(thread)) { 
        err = PTR_ERR(thread);
        TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", err);
    }
    
    //mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
    //mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
    mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, tpd_eint_interrupt_handler, 1);
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    
    printk("MediaTek touch panel i2c probe success\n");
    
    return 0;
}

void tpd_eint_interrupt_handler(void) { 
    TPD_DEBUG_PRINT_INT; tpd_flag=1; wake_up_interruptible(&waiter);
} 

static int tpd_i2c_remove(struct i2c_client *client) 
{
    if(I2CDMABuf_va)
    {
	dma_free_coherent(NULL, 4096, I2CDMABuf_va, I2CDMABuf_pa);
	I2CDMABuf_va = NULL;
	I2CDMABuf_pa = 0;
    }
    return 0;
}

void tpd_down(int raw_x, int raw_y, int x, int y, int p) {
    input_report_abs(tpd->dev, ABS_PRESSURE, 128);
    input_report_key(tpd->dev, BTN_TOUCH, 1);
    input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 128);
    input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, 128);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(tpd->dev);
    TPD_DEBUG("D[%4d %4d %4d]\n", x, y, p);
    TPD_EM_PRINT(raw_x, raw_y, x, y, p, 1);
}

void tpd_up(int raw_x, int raw_y, int x, int y, int p) {
    input_report_abs(tpd->dev, ABS_PRESSURE, 0);
    input_report_key(tpd->dev, BTN_TOUCH, 0);
    input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
    input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, 0);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(tpd->dev);
    TPD_DEBUG("U[%4d %4d %4d]\n", x, y, 0);
    TPD_EM_PRINT(raw_x, raw_y, x, y, p, 0);
}

static int touch_event_handler(void *unused) {
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD }; 
    static int x1, y1, raw_x1, raw_y1;
    int temp_x1, temp_y1, temp_raw_x1, temp_raw_y1, temp_touch_dir, temp_contact_id;
    int report_id, touch_valid, contact_id, touch_dir, touch_need_sync;
    char buffer[10];
    int ret = -1;
    
    sched_setscheduler(current, SCHED_RR, &param); 
    do {
        set_current_state(TASK_INTERRUPTIBLE);
        while (tpd_halt) {tpd_flag = 0; msleep(20);}
        wait_event_interruptible(waiter, tpd_flag != 0);
        tpd_flag = 0;
        TPD_DEBUG_SET_TIME;
        set_current_state(TASK_RUNNING); 
        TPD_DEBUG("[mtk-tpd] touch interrupt\n");
        touch_need_sync = 0;
        temp_x1 = 0xFFFFFFFF;
        temp_y1 = 0xFFFFFFFF;
        temp_raw_x1 = 0xFFFFFFFF;
        temp_raw_y1 = 0xFFFFFFFF;
        temp_touch_dir = 0xFFFFFFFF;
        temp_contact_id = 0xFFFFFFFF;
        do
        {
            i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
            ret = tpd_i2c_read(i2c_client, buffer, 10);
            if (ret != sizeof(buffer))
            {
                TPD_DEBUG("[mtk-tpd] i2c read communcate error: 0x%x\n", ret);
                continue;
            }
            i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;
            report_id = buffer[0];
            if (0x4 == report_id)
            {
            	touch_valid = buffer[1]>>7;
            	if (touch_valid)
            	{
            	    contact_id = (buffer[1]&0x7C)>>2;
            	    touch_dir = buffer[1]&0x1;
            	    raw_x1 = x1 = ((buffer[3] << 8) | buffer[2]);
                    raw_y1 = y1 = ((buffer[5] << 8) | buffer[4]);
                    TPD_DEBUG("[mtk-tpd]:id:%d, raw_x1:%d, raw_y1:%d\n", contact_id, raw_x1, raw_y1);
                    tpd_calibrate(&x1, &y1);
                    
                    if (((temp_x1 != x1) || (temp_y1 != y1) || (temp_contact_id != contact_id))
                        && (0xFFFFFFFF != temp_x1) && (0xFFFFFFFF != temp_y1))
                    {
                    	touch_need_sync = 1;
                        if (temp_touch_dir)
                        {
                    	    tpd_down(temp_raw_y1, temp_raw_x1, temp_y1, temp_x1, 1);
                        }
                        else
                        {
                    	    tpd_up(temp_raw_y1, temp_raw_x1, temp_y1, temp_x1, 0);
                        }
                    }
                    temp_x1 = x1;
                    temp_y1 = y1;
                    temp_raw_x1 = raw_x1;
                    temp_raw_y1 = raw_y1;
                    temp_touch_dir = touch_dir;
                    temp_contact_id = contact_id;
            	}
            	else
            	{
            	    TPD_DEBUG("[mtk-tpd]:Touch Invalid\n");
            	}
            }
            else
            {
                TPD_DEBUG("[mtk-tpd]:Invalid report ID:%d\n", buffer);
            }
        }while(!mt_get_gpio_in(GPIO_CTP_EINT_PIN));
        
        if ((0xFFFFFFFF != temp_x1) && (0xFFFFFFFF != temp_y1))
        {
            touch_need_sync = 1;
            if (temp_touch_dir)
            {
                tpd_down(temp_raw_y1, temp_raw_x1, temp_y1, temp_x1, 1);
            }
            else
            {
                tpd_up(temp_raw_y1, temp_raw_x1, temp_y1, temp_x1, 0);
            }
        }
        if (touch_need_sync)
            input_sync(tpd->dev);
    } while (!kthread_should_stop()); 
    return 0;
}

int tpd_local_init(void) 
{
     if(i2c_add_driver(&tpd_i2c_driver)!=0) {
      TPD_DMESG("unable to add i2c driver.\n");
      return -1;
    }
#ifdef TPD_HAVE_BUTTON     
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif   
  
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))    
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT*4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT*4);
#endif 

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_def_calmat_local, 8*4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local, 8*4);	
#endif  
		TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);  
		tpd_type_cap = 1;
    return 0;
}

/* Function to manage low power suspend */
void tpd_suspend(struct i2c_client *client, pm_message_t message)
{
    int ret = -1;
    u8 cmdbuf[MAX_I2C_LEN]={0x03, 0x06, 0x0A, 0x03, 0x36, 0x3F, 0x02, 0, 0, 0};
    tpd_halt = 1;
    
    
    i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
    ret = tpd_i2c_write(i2c_client, cmdbuf, 10);
    i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;
    if (ret != sizeof(cmdbuf))
    {
        TPD_DEBUG("[mtk-tpd] i2c write communcate error in suspend: 0x%x\n", ret);
    }
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
    
    TPD_DEBUG("[mtk-tpd] touch suspend\n");
}

/* Function to manage power-on resume */
void tpd_resume(struct i2c_client *client)
{
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_EINT_PIN, GPIO_OUT_ZERO);
    msleep(10); 
    mt_set_gpio_out(GPIO_CTP_EINT_PIN, GPIO_OUT_ONE);
    
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);

    
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
    tpd_halt = 0;
    TPD_DEBUG("[mtk-tpd] touch resume\n");
}

static struct tpd_driver_t tpd_device_driver = {
		.tpd_device_name = "excp7200",
		.tpd_local_init = tpd_local_init,
		.suspend = tpd_suspend,
		.resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
		.tpd_have_button = 1,
#else
		.tpd_have_button = 0,
#endif		
};
/* called when loaded into kernel */
static int __init tpd_driver_init(void) {
    printk("MediaTek excp7200 touch panel driver init\n");
		if(tpd_driver_add(&tpd_device_driver) < 0)
			TPD_DMESG("add generic driver failed\n");
    return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void) {
    TPD_DMESG("MediaTek excp7200 touch panel driver exit\n");
    //input_unregister_device(tpd->dev);
    tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

