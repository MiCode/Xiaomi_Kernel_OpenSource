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
#include <mach/eint.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <cust_eint.h>
#include <linux/jiffies.h>

#include "tpd.h"
#include "gn_mms144.h"
#include "gn_mms144_bin_isc.h"

#define TP_DEV_NAME "mms144"
#define I2C_RETRY_CNT 5 //Fixed value
#define DOWNLOAD_RETRY_CNT 5 //Fixed value
#define MELFAS_DOWNLOAD 1 //Fixed value

#define PRESS_KEY 1 //Fixed value
#define RELEASE_KEY 0 //Fixed value

#define TS_READ_LEN_ADDR 0x0F //Fixed value
#define TS_READ_START_ADDR 0x10 //Fixed value
#define TS_READ_REGS_LEN 66 //Fixed value
#define TS_WRITE_REGS_LEN 16 //Fixed value

#define TS_MAX_TOUCH 	10 //Model Dependent
#define TS_READ_HW_VER_ADDR 0xF1 //Model Dependent
#define TS_READ_SW_VER_ADDR 0xF5 //Model Dependent

#define MELFAS_HW_REVISON 0x01 //Model Dependent
#define MELFAS_FW_VERSION 0x02 //Model Dependent

#define MELFAS_MAX_TRANSACTION_LENGTH 66
#define MELFAS_MAX_I2C_TRANSFER_SIZE  7
#define MELFAS_I2C_DEVICE_ADDRESS_LEN 1
//#define I2C_MASTER_CLOCK       400
#define MELFAS_I2C_MASTER_CLOCK       100
#define MELFAS_I2C_ADDRESS   0x48

#define REPORT_MT_DOWN(touch_number, x, y, width, strength) \
do {     \
	input_report_abs(tpd->dev, ABS_PRESSURE, strength);  \
    input_report_key(tpd->dev, BTN_TOUCH, 1);   \
	input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, touch_number);\
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);             \
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);             \
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, width);         \
	input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, strength); \
	input_mt_sync(tpd->dev);                                      \
} while (0)

#define REPORT_MT_UP() \
do {   \
    input_report_key(tpd->dev, BTN_TOUCH, 0);  \
    input_mt_sync(tpd->dev);                \
} while (0)


static int melfas_tpd_flag = 0;

static struct muti_touch_info g_Mtouch_info[TS_MAX_TOUCH];
//static int tsp_keycodes[4] = {KEY_MENU, KEY_HOME, KEY_SEARCH, KEY_BACK};

#ifdef TPD_HAVE_BUTTON 
#define TPD_KEY_COUNT	4
#define TPD_KEYS  {  KEY_HOME,KEY_MENU,KEY_BACK, KEY_SEARCH}
#define TPD_KEYS_DIM	{{60,850,120,100},{180,850,120,100},{300,850,120,100},{420,850,120,100}}
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif


static DECLARE_WAIT_QUEUE_HEAD(melfas_waiter);

static struct i2c_client *melfas_i2c_client = NULL;

static const struct i2c_device_id melfas_tpd_id[] = {{TP_DEV_NAME,0},{}};
static struct i2c_board_info __initdata melfas_i2c_tpd={ I2C_BOARD_INFO(TP_DEV_NAME, MELFAS_I2C_ADDRESS)};

static int melfas_tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int melfas_tpd_i2c_remove(struct i2c_client *client);
static int melfas_tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
extern int isc_fw_download(struct i2c_client *client, const u8 *data, size_t len);
static int melfas_i2c_read(struct i2c_client *client, u16 addr, u16 len, u8 *rxbuf);

static void melfas_ts_release_all_finger(void);


extern struct tpd_device *tpd;



static struct i2c_driver melfas_tpd_i2c_driver =
{                       
    .probe = melfas_tpd_i2c_probe,                                   
    .remove = __devexit_p(melfas_tpd_i2c_remove),                           
    .detect = melfas_tpd_i2c_detect,                           
    .driver.name = "mtk-tpd", 
    .id_table = melfas_tpd_id,                             
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend = melfas_ts_suspend, 
    .resume = melfas_ts_resume,
#endif
    
}; 

#define VAR_CHAR_NUM_MAX      20

#if defined (GN_MTK_BSP)
static ssize_t show_update_pro(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
#if defined(TPD_UPDATE_FW)
    char ver[20];
    TPD_DMESG(" show_update_fw_pro\n");
	
	melfas_update_show_reg(ver);
	snprintf(buf, VAR_CHAR_NUM_MAX, "%s", ver);
#endif
	return 0;

}
static ssize_t store_update_pro(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    //char strbuf[10]="OK\n";
	
    TPD_DMESG(" store_update_fw_pro\n");
    TPD_DMESG(" store_update_fw_pro--%s----\n",buf);
    if(!strcmp(buf , "update"))
    {
#if defined(TPD_UPDATE_FW)
		melfas_tpd_update_fw();
#endif
    }
//	snprintf(buf, VAR_CHAR_NUM_MAX, "%s", strbuf);
    return 0;        

}

static struct kobj_attribute update_firmware_attribute = {
	.attr = {.name = "update", .mode = 0664},
	.show = show_update_pro,
	.store = store_update_pro,
};
static int tpd_create_attr(void) 
{
	int ret;
	struct kobject *kobject_ts;
	kobject_ts = kobject_create_and_add("touch_screen", NULL);
	if (!kobject_ts)
	{
		printk("create kobjetct error!\n");
		return -1;
	}
	
	ret = sysfs_create_file(kobject_ts, &update_firmware_attribute.attr);
	if (ret) {
		kobject_put(kobject_ts);
		printk("create file error\n");
		return -1;
	}
	return 0;	

}
#endif




static void esd_rest_tp(void)
{
	printk("==========tp have inter esd =============\n");
	mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ZERO);
	msleep(100);
   mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);	
	
}

static int melfas_touch_event_handler(void *unused)
{
	uint8_t buf[TS_READ_REGS_LEN] = { 0 };
	int i, read_num, fingerID, Touch_Type = 0, touchState = 0;//, keyID = 0;
	//uint8_t buf_esd[2]={0};
	
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD }; 
	

    sched_setscheduler(current, SCHED_RR, &param); 

    do
    {
        set_current_state(TASK_INTERRUPTIBLE);
		
        wait_event_interruptible(melfas_waiter, melfas_tpd_flag != 0);
		
        melfas_tpd_flag = 0;
        set_current_state(TASK_RUNNING); 

		melfas_i2c_read(melfas_i2c_client, TS_READ_LEN_ADDR, 1, buf);
		read_num = buf[0];
		if(read_num)
		{
			melfas_i2c_read(melfas_i2c_client, TS_READ_START_ADDR, read_num, buf);
        	//printk("esd 0x10 register = %2X\n",buf[0]);
          	if(0x0F == buf[0])
           	{
            	   esd_rest_tp();
             	continue;
		 		}

			for (i = 0; i < read_num; i = i + 6)
            {
                Touch_Type = (buf[i] >> 5) & 0x03;

                /* touch type is panel */
                if (Touch_Type == TOUCH_SCREEN)
                {
                    fingerID = (buf[i] & 0x0F) - 1;
                    touchState = (buf[i] & 0x80);

                    g_Mtouch_info[fingerID].posX = (uint16_t)(buf[i + 1] & 0x0F) << 8 | buf[i + 2];
                    g_Mtouch_info[fingerID].posY = (uint16_t)(buf[i + 1] & 0xF0) << 4 | buf[i + 3];
                    g_Mtouch_info[fingerID].area = buf[i + 4];

                    if (touchState)
                        g_Mtouch_info[fingerID].pressure = buf[i + 5];
                    else
                        g_Mtouch_info[fingerID].pressure = 0;
                }
            }
			for (i = 0; i < TS_MAX_TOUCH; i++)
            {
                  if (g_Mtouch_info[i].pressure == -1)
                    continue;


				  if(g_Mtouch_info[i].pressure == 0)
				  {
					  // release event    
				  	REPORT_MT_UP();  
				  }
				  else
				  {
					REPORT_MT_DOWN(i, g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].area, g_Mtouch_info[i].pressure);
				  }
                 TPD_DEBUG("[TSP] %s: Touch ID: %d, State : %d, x: %d, y: %d, z: %d w: %d\n", __FUNCTION__,
                        i, (g_Mtouch_info[i].pressure > 0), g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].pressure, g_Mtouch_info[i].area);


                if (g_Mtouch_info[i].pressure == 0)
                    g_Mtouch_info[i].pressure = -1;
            }
			
			if ( tpd != NULL && tpd->dev != NULL )
            	input_sync(tpd->dev);
            
		}
		//else
		//{
		//	REPORT_MT_UP();
		//}
		
    } while ( !kthread_should_stop() ); 

    return 0;
}

static void melfas_i2c_tpd_eint_interrupt_handler(void)
{ 
    TPD_DEBUG_PRINT_INT;
    melfas_tpd_flag=1;
    wake_up_interruptible(&melfas_waiter);
} 

int melfas_i2c_write_bytes( struct i2c_client *client, u16 addr, int len, u8 *txbuf )
{
    u8 buffer[MELFAS_MAX_TRANSACTION_LENGTH]={0};
    u16 left = len;
    u8 offset = 0;
    u8 retry = 0;

    struct i2c_msg msg = 
    {
        .addr = ((client->addr&I2C_MASK_FLAG )|(I2C_ENEXT_FLAG )),
        .flags = 0,
        .buf = buffer,
        .timing = MELFAS_I2C_MASTER_CLOCK,
    };


    if ( txbuf == NULL )
        return -1;

    TPD_DEBUG("i2c_write_bytes to device %02X address %04X len %d\n", client->addr, addr, len );

    while ( left > 0 )
    {
        retry = 0;

        buffer[0] = (u8)addr+offset;

        if ( left > MELFAS_MAX_I2C_TRANSFER_SIZE )
        {
            memcpy( &buffer[MELFAS_I2C_DEVICE_ADDRESS_LEN], &txbuf[offset], MELFAS_MAX_I2C_TRANSFER_SIZE );
            msg.len = MELFAS_MAX_TRANSACTION_LENGTH;
            left -= MELFAS_MAX_I2C_TRANSFER_SIZE;
            offset += MELFAS_MAX_I2C_TRANSFER_SIZE;
        }
        else
        {
            memcpy( &buffer[MELFAS_I2C_DEVICE_ADDRESS_LEN], &txbuf[offset], left );
            msg.len = left + MELFAS_I2C_DEVICE_ADDRESS_LEN;
            left = 0;
        }

        TPD_DEBUG("byte left %d offset %d\n", left, offset );

        while ( i2c_transfer( client->adapter, &msg, 1 ) != 1 )
        {
            retry++;

            if ( retry == I2C_RETRY_CNT )
            {
                TPD_DEBUG("I2C write 0x%X%X length=%d failed\n", buffer[0], buffer[1], len);
                TPD_DMESG("I2C write 0x%X%X length=%d failed\n", buffer[0], buffer[1], len);
                return -1;
            }
            else
                 TPD_DEBUG("I2C write retry %d addr 0x%X%X\n", retry, buffer[0], buffer[1]);

        }
    }

    return 0;
}

static int melfas_i2c_read(struct i2c_client *client, u16 addr, u16 len, u8 *rxbuf)
{
    u8 buffer[MELFAS_I2C_DEVICE_ADDRESS_LEN]={0};
    u8 retry;
    u16 left = len;
    u8 offset = 0;

    struct i2c_msg msg[2] =
    {
        {
            .addr = ((client->addr&I2C_MASK_FLAG )|(I2C_ENEXT_FLAG )),
            .flags = 0,
            .buf = buffer,
            .len = MELFAS_I2C_DEVICE_ADDRESS_LEN,
            .timing = MELFAS_I2C_MASTER_CLOCK
        },
        {
            .addr = ((client->addr&I2C_MASK_FLAG )|(I2C_ENEXT_FLAG )),
            .flags = I2C_M_RD,
            .timing = MELFAS_I2C_MASTER_CLOCK
        },
    };

    if ( rxbuf == NULL )
        return -1;

    TPD_DEBUG("i2c_read_bytes to device %02X address %04X len %d\n", client->addr, addr, len );

    while ( left > 0 )
    {
        buffer[0] = (u8)addr+offset;

        msg[1].buf = &rxbuf[offset];

        if ( left > MELFAS_MAX_TRANSACTION_LENGTH )
        {
            msg[1].len = MELFAS_MAX_TRANSACTION_LENGTH;
            left -= MELFAS_MAX_TRANSACTION_LENGTH;
            offset += MELFAS_MAX_TRANSACTION_LENGTH;
        }
        else
        {
            msg[1].len = left;
            left = 0;
        }

        retry = 0;

        while ( i2c_transfer( client->adapter, &msg[0], 2 ) != 2 )
        {
            retry++;

            if ( retry == I2C_RETRY_CNT )
            {
                TPD_DEBUG("I2C read 0x%X length=%d failed\n", addr + offset, len);
                TPD_DMESG("I2C read 0x%X length=%d failed\n", addr + offset, len);
                return -1;
            }
        }
    }

    return 0;
}

static int melfas_check_firmware(struct i2c_client *client, u8 *val)
{
    int ret = 0;
    //uint8_t i = 0;

   
    ret = melfas_i2c_read(client, TS_READ_HW_VER_ADDR, 1, &val[0]);
    ret = melfas_i2c_read(client, TS_READ_SW_VER_ADDR, 1, &val[1]);

    if (ret >= 0)
    {
        TPD_DMESG("[melfas_tpd]: HW Revision[0x%02x] SW Version[0x%02x] \n", val[0], val[1]);
    }

    if (ret < 0)
    {
        TPD_DMESG("[melfas_tpd] %s,%d: i2c read fail[%d] \n", __FUNCTION__, __LINE__, ret);
    }
	
	return ret;
}

static int melfas_firmware_update(struct i2c_client *client)
{
    int ret = 0;
    uint8_t fw_ver[2] = {0, };
	
    ret = melfas_check_firmware(client, fw_ver);
    if (ret < 0)
        TPD_DMESG("[melfas_tpd] check_firmware fail! [%d]", ret);
    else
    {
#if 0//MELFAS_DOWNLOAD
        if (fw_ver[1] < MELFAS_FW_VERSION)
        {
            int ver;

            TPD_DMESG("[melfas_tpd] %s: \n", __func__);
            ret = isc_fw_download(client, MELFAS_binary, MELFAS_binary_nLength);
            if (ret < 0)
            {
                //ret = isp_fw_download(MELFAS_MMS100_Initial_binary, MELFAS_MMS100_Initial_nLength);
                if (ret != 0)
                {
                    TPD_DMESG("[melfas_tpd] error updating firmware to version 0x%02x \n", MELFAS_FW_VERSION);
                    ret = -1;
                }
                else
                {
                    ret = isc_fw_download(client, MELFAS_binary, MELFAS_binary_nLength);
                }
            }
        }
#endif
    }

    return ret;
}
static int melfas_tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{             
    int err = 0;
	struct task_struct *thread = NULL;
	
	TPD_DMESG("[melfas_tpd] %s\n", __func__);
	
	//power on
    mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);

    msleep(200);  
    
    melfas_i2c_client = client;
	
	melfas_firmware_update(client);
	
#if defined (GN_MTK_BSP)
		err = tpd_create_attr();
		if(err)
		{
			TPD_DMESG(" tpd create attribute err = %d\n", err);
		}
#endif
	
    thread = kthread_run(melfas_touch_event_handler, 0, TPD_DEVICE);

    if (IS_ERR(thread))
    { 
        err = PTR_ERR(thread);
        TPD_DMESG(TPD_DEVICE "[melfas_tpd] failed to create kernel thread: %d\n", err);
    }
 
    // set INT mode
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
	
    msleep(50);

    //mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
    //mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
    mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, tpd_eint_interrupt_handler, 1);
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    tpd_load_status = 1;
    
    return 0;

}
static int melfas_tpd_i2c_remove(struct i2c_client *client)
{
    return 0;
}
static int melfas_tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	TPD_DMESG("[melfas_tpd] %s\n", __func__);
    strcpy(info->type, "mtk-tpd");
    return 0;
}
static int melfas_tpd_local_init(void) 
{

	TPD_DMESG("[melfas_tpd] end %s, %d\n", __FUNCTION__, __LINE__);  
    if(i2c_add_driver(&melfas_tpd_i2c_driver)!=0)
    {
        TPD_DMESG("[melfas_tpd] unable to add i2c driver.\n");
        return -1;
    }
    if(tpd_load_status == 0)
    {
    	TPD_DMESG("[melfas_tpd] add error touch panel driver.\n");
    	i2c_del_driver(&melfas_tpd_i2c_driver);
    	return -1;
    }
  	#ifdef TPD_HAVE_BUTTON     
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);
	#endif   

   
    tpd_type_cap = 1;

    return 0;
}


#ifdef SLOT_TYPE
static void melfas_ts_release_all_finger(void)
{
	int i;
	TPD_DMESG("[melfas_tpd] %s\n", __func__);
	for (i = 0; i < TS_MAX_TOUCH; i++)
	{
		input_mt_slot(tpd->dev, i);
		input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, false);
	}
	input_sync(tpd->dev);
}
#else	
static void melfas_ts_release_all_finger(void)
{
	int i;
    TPD_DMESG("[melfas_tpd] %s\n", __func__);

	for(i=0; i<TS_MAX_TOUCH; i++)
	{
		if(-1 == g_Mtouch_info[i].pressure)
			continue;

		if(g_Mtouch_info[i].pressure == 0)
			input_mt_sync(tpd->dev);

		if(0 == g_Mtouch_info[i].pressure)
			g_Mtouch_info[i].pressure = -1;
	}
	input_sync(tpd->dev);
}
#endif


#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_tpd_early_suspend(struct early_suspend *h)
{
    TPD_DMESG("[melfas_tpd] %s\n", __func__);

    melfas_ts_release_all_finger();

	//irq mask
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	//power down
    mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ZERO);
}

static void melfas_tpd_late_resume(struct early_suspend *h)
{
    //int ret;
    //struct melfas_ts_data *ts = i2c_get_clientdata(client);

    TPD_DMESG("[melfas_tpd] %s\n", __func__);

    //power on
    mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
    msleep(200);
   	//irq unmask
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
}
#endif

static struct tpd_driver_t melfas_tpd_device_driver =
{
    .tpd_device_name = "melfas_mms144",
    .tpd_local_init = melfas_tpd_local_init,
#ifdef CONFIG_HAS_EARLYSUSPEND    
    .suspend = melfas_tpd_early_suspend,
    .resume = melfas_tpd_late_resume,
#endif
#ifdef TPD_HAVE_BUTTON
    .tpd_have_button = 1,
#else
    .tpd_have_button = 0,
#endif		

    
};

/* called when loaded into kernel */
static int __init melfas_tpd_driver_init(void)
{
    TPD_DMESG("[melfas_tpd] %s\n", __func__);
	i2c_register_board_info(0, &melfas_i2c_tpd, 1);
    if ( tpd_driver_add(&melfas_tpd_device_driver) < 0)
        TPD_DMESG("[melfas_tpd] add generic driver failed\n");

    return 0;
}

/* should never be called */
static void __exit melfas_tpd_driver_exit(void)
{
    TPD_DMESG("[melfas_tpd] %s\n", __func__);
    tpd_driver_remove(&melfas_tpd_device_driver);
}


module_init(melfas_tpd_driver_init);
module_exit(melfas_tpd_driver_exit);

