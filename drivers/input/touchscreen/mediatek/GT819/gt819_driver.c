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

#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <asm/uaccess.h>

#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>

#include "tpd_custom_gt819.h"
#include "tpd.h"
#include <cust_eint.h>

#ifndef TPD_NO_GPIO 
#include "cust_gpio_usage.h"
#endif

#define ABS(x)                  ((x<0)?-x:x)


extern struct tpd_device *tpd;

static int tpd_flag = 0;
static int tpd_halt=0;
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
//static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX;
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

static void tpd_eint_interrupt_handler(void);
static int touch_event_handler(void *unused);
static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
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
static unsigned short force[] = {0, 0xAA, I2C_CLIENT_END,I2C_CLIENT_END};
static const unsigned short * const forces[] = { force, NULL };
//static struct i2c_client_address_data addr_data = { .forces = forces,};
static struct i2c_board_info __initdata i2c_tpd={ I2C_BOARD_INFO("mtk-tpd", (0xAA>>1))};
struct i2c_driver tpd_i2c_driver = {                       
    .probe = tpd_i2c_probe,                                   
    .remove = tpd_i2c_remove,                           
    .detect = tpd_i2c_detect,                           
    .driver.name = "mtk-tpd", 
    .id_table = tpd_i2c_id,                             
    .address_list = (const unsigned short*) forces,                        
}; 

static int i2c_read_bytes( struct i2c_client *client, u8 addr, u8 *rxbuf, int len );
static int i2c_write_bytes( struct i2c_client *client, u16 addr, u8 *txbuf, int len );



static u8 cfg_data[] =
{
	0x02,0x04,0x00,0x02,0x58,0x05,0xAD,0x01,
	0x00,0x0F,0x10,0x02,0x48,0x10,0x00,0x00,
	0x00,0x20,0x00,0x10,0x10,0x10,0x00,0x37,
	0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,
	0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,
	0xFF,0xFF,0x00,0x01,0x02,0x03,0x04,0x05,
	0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0xFF,
	0xFF,0xFF,0x00,0x00,0x3C,0x64,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x0B,0x0B,0x0B,0x0B,0x20
};


static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info) {
    strcpy(info->type, "mtk-tpd");
    return 0;
}

static int i2c_read_bytes( struct i2c_client *client, u8 addr, u8 *rxbuf, int len )
{
    u8 retry;
    u16 left = len;
    u16 offset = 0;

    if ( rxbuf == NULL )
        return -1;

    TPD_DEBUG("i2c_read_bytes to device %02X address %04X len %d\n", client->addr, addr, len );

    while ( left > 0 )
    {
        if ( left > MAX_I2C_TRANSFER_SIZE )
        {
            rxbuf[offset] = ( addr+offset ) & 0xFF;
            i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;
            i2c_client->ext_flag = I2C_WR_FLAG | I2C_RS_FLAG;
            
            retry = 0;
            while ( i2c_master_send(i2c_client, &rxbuf[offset], (MAX_I2C_TRANSFER_SIZE << 8 | 1)) < 0 )
            {
                retry++;

                if ( retry == 5 )
                {
                    i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;
                    TPD_DEBUG("I2C read 0x%X length=%d failed\n", addr + offset, MAX_I2C_TRANSFER_SIZE);
                    return -1;
                }
            }
            left -= MAX_I2C_TRANSFER_SIZE;
            offset += MAX_I2C_TRANSFER_SIZE;
        }
        else
        {
            
            rxbuf[offset] = ( addr+offset ) & 0xFF;
            i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;
            i2c_client->ext_flag = I2C_WR_FLAG | I2C_RS_FLAG;
            
            retry = 0;
            while ( i2c_master_send(i2c_client, &rxbuf[offset], (left << 8 | 1)) < 0 )
            {
                retry++;

                if ( retry == 5 )
                {
                    i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;
                    TPD_DEBUG("I2C read 0x%X length=%d failed\n", addr + offset, left);
                    return -1;
                }
            }
            offset += left;
            left = 0;
        }
    }
    i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;
    i2c_client->ext_flag = 0;
    
    return 0;
}

static int i2c_write_bytes( struct i2c_client *client, u16 addr, u8 *txbuf, int len )
{
    u8 buffer[MAX_TRANSACTION_LENGTH];
    u16 left = len;
    u16 offset = 0;
    u8 retry = 0;

    struct i2c_msg msg = 
    {
        .addr = ((client->addr&I2C_MASK_FLAG )|(I2C_ENEXT_FLAG )),
        .ext_flag = i2c_client->ext_flag | I2C_ENEXT_FLAG,
        .flags = 0,
        .buf = buffer
    };


    if ( txbuf == NULL )
        return -1;

    TPD_DEBUG("i2c_write_bytes to device %02X address %04X len %d\n", client->addr, addr, len );

    while ( left > 0 )
    {
        retry = 0;

        buffer[0] = ( addr+offset ) & 0xFF;

        if ( left > MAX_I2C_TRANSFER_SIZE )
        {
            memcpy( &buffer[I2C_DEVICE_ADDRESS_LEN], &txbuf[offset], MAX_I2C_TRANSFER_SIZE );
            msg.len = MAX_TRANSACTION_LENGTH;
            left -= MAX_I2C_TRANSFER_SIZE;
            offset += MAX_I2C_TRANSFER_SIZE;
        }
        else
        {
            memcpy( &buffer[I2C_DEVICE_ADDRESS_LEN], &txbuf[offset], left );
            msg.len = left + I2C_DEVICE_ADDRESS_LEN;
            left = 0;
        }

        TPD_DEBUG("byte left %d offset %d\n", left, offset );

        while ( i2c_transfer( client->adapter, &msg, 1 ) != 1 )
        {
            retry++;

            if ( retry == 5 )
            {
                TPD_DEBUG("I2C write 0x%X%X length=%d failed\n", buffer[0], buffer[1], len);
                return -1;
            }
            else
                 TPD_DEBUG("I2C write retry %d addr 0x%X%X\n", retry, buffer[0], buffer[1]);

        }
    }

    return 0;
}

static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id) {             
    int err = 0;//ret = -1;
    //struct goodix_ts_data *ts;
    //u8 version[17];
    
    #ifdef TPD_NO_GPIO
    u16 temp;
    temp = *(volatile u16 *) TPD_RESET_PIN_ADDR;
    temp = temp | 0x40;
    *(volatile u16 *) TPD_RESET_PIN_ADDR = temp;
    #endif
    i2c_client = client;
    
    printk("MediaTek touch panel i2c probe\n");
    //client->addr = 0x55;
    
    //Power on
    hwPowerOn(TPD_POWER_LDO, VOL_3300, "TP");
    printk("MediaTek touch panel sets hwPowerOn!!!\n");
    //msleep(10);
    
    #ifndef TPD_NO_GPIO 

    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
    msleep(10);  
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
   
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_DISABLE);
    #endif 

    msleep(50);
    
    //Get firmware version
    /*
    #if 0
    memset( &version[0], 0, 17);
    version[16] = '\0';
    err = i2c_read_bytes( i2c_client, TPD_VERSION_INFO_REG, (u8 *)&version[0], 16);

    if ( err )
    {
        printk(TPD_DEVICE " fail to get tpd info %d\n", err );
        return err;
    }
    else
    {
        printk( "Goodix TouchScreen Version:%s\n", (char *)&version[0]);
    }
    #endif
    */
    
    //Load the init table
    // setting resolution, RES_X, RES_Y
    //cfg_data[1] = (TPD_RES_X>>8)&0xff;
    //cfg_data[2] = TPD_RES_X&0xff;
    //cfg_data[3] = (TPD_RES_Y>>8)&0xff;
    //cfg_data[4] = TPD_RES_Y&0xff;
    err = i2c_write_bytes( i2c_client, TPD_CONFIG_REG_BASE, cfg_data, sizeof(cfg_data));

    if (err)
    {
        printk(TPD_DEVICE " fail to write tpd cfg %d\n", err );
        return err;
    }
    msleep(10);
    
    
    //i2c_read_bytes(i2c_client, TPD_TOUCH_INFO_REG_BASE, &version[0], 2);
    //printk(TPD_DEVICE " tpd cfg 0x%x 0x%x 0x%x 0x%x\n", version[0], version[1], version[2], version[3]);
	
    thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
    if (IS_ERR(thread)) { 
        err = PTR_ERR(thread);
        printk(TPD_DEVICE " failed to create kernel thread: %d\n", err);
    }
    
    tpd_load_status = 1;
    
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

void tpd_up(int raw_x, int raw_y, int x, int y, int p)
{
    //input_report_abs(tpd->dev, ABS_PRESSURE, 0);
    input_report_key(tpd->dev, BTN_TOUCH, 0);
    //input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
    //input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, 0);
    //input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    //input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(tpd->dev);
    TPD_DEBUG("U[%4d %4d %4d]\n", x, y, 0);
    TPD_EM_PRINT(raw_x, raw_y, x, y, p, 0);
}

static int touch_event_handler(void *unused) {
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
    static u8 buffer[ TPD_POINT_INFO_LEN*MAX_FINGER_NUM ];
    int x, y, size, max_finger_id = 0, finger_num = 0;
    //int finger_num = 0;
    //static u8 id_mask = 0;
    u16 cur_mask;
    int idx, valid_point, lastIdx = 0;
    int ret = 0;
    static int x_history[MAX_FINGER_NUM+1];
    static int y_history[MAX_FINGER_NUM+1];
    
    sched_setscheduler(current, SCHED_RR, &param); 
    do{
        set_current_state(TASK_INTERRUPTIBLE);
        while (tpd_halt) {tpd_flag = 0; msleep(20);}
        wait_event_interruptible(waiter, tpd_flag != 0);
        tpd_flag = 0;
        TPD_DEBUG_SET_TIME;
        set_current_state(TASK_RUNNING); 
        
        
        ret = i2c_read_bytes(i2c_client, TPD_TOUCH_INFO_REG_BASE, &buffer[0], 2);
        if (ret < 0)
        {
            TPD_DEBUG("[mtk-tpd] i2c write communcate error during read status\n");
            continue;
        }
        TPD_DEBUG("[mtk-tpd] STATUS : %x %x\n", buffer[0], buffer[1]);
        cur_mask = ((buffer[1] & 0x03) << 8) | (buffer[0] & 0xff);
        
        if ( tpd == NULL || tpd->dev == NULL )
            continue;
        
        if ( cur_mask )
        {
            idx = 0;
            do
            {
                idx++;
            }while((cur_mask >> idx) > 0);
            
            finger_num = idx;
            max_finger_id = idx - 1;
            ret = i2c_read_bytes( i2c_client, TPD_POINT_INFO_REG_BASE, buffer, (max_finger_id + 1)*TPD_POINT_INFO_LEN);
            if (ret < 0)
            {
                TPD_DEBUG("[mtk-tpd] i2c write communcate error during read point\n");
                continue;
            }
        }
        else
        {
            max_finger_id = 0;
            finger_num = 0;
        }

        for ( idx = 0, valid_point = 0 ; idx < (max_finger_id + 1) ; idx++ )
        {
            u8 *ptr = &buffer[ valid_point*TPD_POINT_INFO_LEN ];

            if (cur_mask & (1 << idx))
            {
                x = ptr[1] + (((int)ptr[0]) << 8);
                y = ptr[3] + (((int)ptr[2]) << 8);
                size = ptr[4];
                
                TPD_DEBUG("[mtk-tpd] position : %x %x\n", x, y);

                tpd_calibrate(&x, &y);
                tpd_down( x, y, x, y, size );

                x_history[idx] = x;
                y_history[idx] = y;
                valid_point++;
                lastIdx = idx;
            }
            else
                TPD_DEBUG("Invalid id %d\n", idx );
        }         
        
        /****Linux kernel 2.6.35. For GB.****/
#if 0       
        if ( cur_mask != id_mask )
        {
            u8 diff = cur_mask^id_mask;
            idx = 0;

            while ( diff )
            {
                if ( ( ( diff & 0x01 ) == 1 ) &&
                     ( ( cur_mask >> idx ) & 0x01 ) == 0 )
                {
                    // check if key release
                    tpd_up( x_history[idx], y_history[idx], x_history[idx], y_history[idx], 0);                    
                }

                diff = ( diff >> 1 );
                idx++;
            }
            id_mask = cur_mask;
        }
        if ( tpd != NULL && tpd->dev != NULL )
        {
            input_sync(tpd->dev);
        }
#endif
        /****Linux kernel from 2.6.35 to 3.0. For ICS.****/
        if ( finger_num )
        {
        	if ( tpd != NULL && tpd->dev != NULL )
            {
            	input_sync(tpd->dev);
        	}
		}
		else
		{
			if ( tpd != NULL && tpd->dev != NULL )
            {
            	//TPD_DEBUG("lastIdx = %d \n", lastIdx);
				//TPD_DEBUG("x_history[lastIdx] = %d, y_history[lastIdx] = %d \n", x_history[lastIdx], y_history[lastIdx]);
            	//TPD_DEBUG("Up[%4d %4d %4d]\n", x_history[lastIdx], y_history[lastIdx], 0);
				//tpd_up(x_history[lastIdx], y_history[lastIdx], x_history[lastIdx], y_history[lastIdx], 0, lastIdx);
				tpd_up(x_history[lastIdx], y_history[lastIdx], x_history[lastIdx], y_history[lastIdx], 0);
				input_sync(tpd->dev);
        	}
		}        
    } while (!kthread_should_stop()); 
    return 0;
}

int tpd_local_init(void) 
{
    if(i2c_add_driver(&tpd_i2c_driver)!=0) {
      TPD_DMESG("unable to add i2c driver.\n");
      return -1;
    }

    if (0) //if(tpd_load_status == 0)
    {
    	TPD_DMESG("add error touch panel driver.\n");
    	i2c_del_driver(&tpd_i2c_driver);
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
//void tpd_suspend(struct i2c_client *client, pm_message_t message)
static void tpd_suspend( struct early_suspend *h )
{
    int ret = 0;
    //int retry = 0;
    u8 Wrbuf[1] = {1};
    tpd_halt = 1;
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
    TPD_DEBUG("GT819 call suspend\n");
    
    ret = i2c_write_bytes(i2c_client, TPD_POWER_MODE_REG, &Wrbuf[0], 1);

    if(ret < 0)
    {
        TPD_DEBUG("[mtk-tpd] i2c write communcate error during suspend: 0x%x\n", ret);
    }
    
    //Turn off GPIO
    /*
    TPD_DEBUG("Turn off GPIO..\n");
    mt_set_gpio_mode(GPIO70, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO70, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO70, GPIO_OUT_ZERO);
   
    mt_set_gpio_mode(GPIO100, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO100, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO100, GPIO_OUT_ZERO);
    */
}

/* Function to manage power-on resume */
//void tpd_resume(struct i2c_client *client)
static void tpd_resume( struct early_suspend *h )
{   
    TPD_DEBUG("GT819 call resume\n");  
    
    /*
    TPD_DEBUG("Turn on GPIO..\n");
    mt_set_gpio_mode(GPIO70, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO70, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO70, GPIO_OUT_ONE);

	mt_set_gpio_mode(GPIO100, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO100, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO100, GPIO_OUT_ONE);
    */
  
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_EINT_PIN, GPIO_OUT_ZERO);
    msleep(20);
    
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_DISABLE);
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
    tpd_halt = 0;
}

static struct tpd_driver_t tpd_device_driver = {
                .tpd_device_name = "gt819",
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
    printk("MediaTek gt819 touch panel driver init\n");
    i2c_register_board_info(0, &i2c_tpd, 1);
                if(tpd_driver_add(&tpd_device_driver) < 0)
                        TPD_DMESG("add generic driver failed\n");
    return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void) {
    TPD_DMESG("MediaTek gt819 touch panel driver exit\n");
    //input_unregister_device(tpd->dev);
    tpd_driver_remove(&tpd_device_driver);
    
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);
