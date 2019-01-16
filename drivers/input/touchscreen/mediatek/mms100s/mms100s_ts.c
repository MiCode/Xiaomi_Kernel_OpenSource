#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
//#include <linux/gpio.h>
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
#include "tpd_custom_mms100s.h"

#include <linux/spinlock.h>
#include <mach/mt_wdt.h>
#include <mach/mt_gpt.h>
#include <mach/mt_reg_base.h>

//#include "mms100_ISP_download.h"
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <cust_eint.h>

#include <linux/wakelock.h>
#include "mms100s_ts.h"


//If test in MTK EVB, define as below: //liming
//#define MTK_EVB_TEST
#ifdef MTK_EVB_TEST
#undef CUST_EINT_TOUCH_PANEL_NUM
#define CUST_EINT_TOUCH_PANEL_NUM    1

#undef GPIO_CTP_EINT_PIN 
#define GPIO_CTP_EINT_PIN GPIO129

#undef GPIO_CTP_EINT_PIN_M_EINT
#define GPIO_CTP_EINT_PIN_M_EINT 2

#endif
/* Event types */
#define MMS_LOG_EVENT		0xD
#define MMS_NOTIFY_EVENT	0xE
#define MMS_ERROR_EVENT		0xF
#define MMS_TOUCH_SCREEN_EVENT	0x20

#define MAX_FINGER_NUM		10
#define FINGER_EVENT_SZ		8

#define TP_DEV_NAME "mms134"
#define I2C_RETRY_CNT 5 //Fixed value
#define DOWNLOAD_RETRY_CNT 5 //Fixed value
#define MELFAS_DOWNLOAD 1 //Fixed value
#define MELFAS_CE_PIN_CONTROL 0//anderson

#define PRESS_KEY 1 //Fixed value
#define RELEASE_KEY 0 //Fixed value

#define TS_READ_LEN_ADDR 0x0F //Fixed value
#define TS_READ_START_ADDR 0x10 //Fixed value
#define TS_READ_REGS_LEN 66 //Fixed value
#define TS_WRITE_REGS_LEN 16 //Fixed value

#define TS_MAX_TOUCH 	10 //Model Dependent

#define TS_READ_HW_VER_ADDR 0xC1 //Model Dependent
#define TS_READ_SW_VER_ADDR 0xC0 //Model Dependent
#define MELFAS_FW_VERSION   0xC3 //Model Dependent
#define MELFAS_CHIP_FW_VERSION   0xC4 //Model Dependent

#ifdef MTK_USE_DOME_KEY
//vee5ds_open_br   : 4 key
//vee5nfc_open_eu  : 2 key(domekey)
//vee5ss_tcl_mx     : 2 key(domekey)
//vee5ss_viv_br     : 2 key(domekey)
#define MELFAS_CURRENT_FW_VERSION 2
#else
#define MELFAS_CURRENT_FW_VERSION 4
#endif

#define MELFAS_HW_REVISON 0x01 //Model Dependent

#define MELFAS_MAX_TRANSACTION_LENGTH 66
#define MELFAS_MAX_I2C_TRANSFER_SIZE  8
#define MELFAS_I2C_DEVICE_ADDRESS_LEN 1
//#define I2C_MASTER_CLOCK       400
#define MELFAS_I2C_MASTER_CLOCK       300
#define MELFAS_I2C_ADDRESS   0x48


#define GPT_IRQEN       (APMCU_GPTIMER_BASE + 0x0000)

static DEFINE_MUTEX(i2c_access);


#define REPORT_MT_DOWN(touch_number, x, y, width, strength) \
do {     \
	input_report_abs(tpd->dev, ABS_PRESSURE, strength);  \
    input_report_key(tpd->dev, BTN_TOUCH, 1);   \
	input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, touch_number);\
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);             \
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);             \
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, width);         \
	input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, strength); \
	input_mt_sync(tpd->dev); \
	TPD_EM_PRINT(x, y, x, y, touch_number, 1);\
} while (0)

#define REPORT_MT_UP(touch_number, x, y, width, strength) \
do {     \
	input_report_abs(tpd->dev, ABS_PRESSURE, strength);  \
    input_report_key(tpd->dev, BTN_TOUCH, 0);   \
	input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, touch_number);\
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);             \
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);             \
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, width);         \
	input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, strength); \
	input_mt_sync(tpd->dev);\
	TPD_EM_PRINT(x, y, x, y, touch_number, 0);\
} while (0)


static int melfas_tpd_flag = 0;
unsigned char  touch_fw_version = 0;

#define TPD_HAVE_BUTTON 

enum {
	None = 0,
	TOUCH_SCREEN,
	TOUCH_KEY
};

struct muti_touch_info {
	int strength;
//	int width
	int area;
	int posX;
	int posY;
	int status;
	int pressure;
};

static struct muti_touch_info g_Mtouch_info[TS_MAX_TOUCH];

/*MTKLM_CHANGE_S : 2012-10-25 lm@mtk.com Changed feature
vee5ds_open_br   : 4 key
vee5nfc_open_eu  : 2 key
vee5ss_tcl_mx     : 2 key
vee5ss_viv_br     : 2 key
*/
//#define TPD_KEY_COUNT	3
#ifdef TPD_HAVE_BUTTON
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

/*MTKLM_CHANGE_E : 2012-10-25 lm@mtk.com Changed feature*/

/*MTKLM_CHANGE_S : 2012-10-16 lm@mtk.com ghost finger pattern detection*/
struct delayed_work ghost_monitor_work;
static int ghost_touch_cnt = 0;
static int ghost_x = 1000;
static int ghost_y = 1000;
/*MTKLM_CHANGE_E : 2012-10-16 lm@mtk.com ghost finger pattern detection*/


static int is_key_pressed = 0;
static int pressed_keycode = 0;
static int is_touch_pressed = 0;
#define CANCEL_KEY 0xff


static DECLARE_WAIT_QUEUE_HEAD(melfas_waiter);

struct i2c_client *melfas_i2c_client = NULL;

static const struct i2c_device_id melfas_tpd_id[] = {{TP_DEV_NAME,0},{}};
static struct i2c_board_info __initdata melfas_i2c_tpd={ I2C_BOARD_INFO(TP_DEV_NAME, MELFAS_I2C_ADDRESS)};

static int melfas_tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int melfas_tpd_i2c_remove(struct i2c_client *client);
static int melfas_tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);

extern int mms100_ISC_download_binary_data(void);
int melfas_i2c_read(struct i2c_client *client, u16 addr, u16 len, u8 *rxbuf);

static void melfas_ts_release_all_finger(void);
//static int melfas_firmware_update(struct i2c_client *client);
static int melfas_ts_fw_load(struct i2c_client *client);

int melfas_i2c_read(struct i2c_client *client, u16 addr, u16 len, u8 *rxbuf);


extern struct tpd_device *tpd;
static struct mms_ts_info *info = NULL;


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

#define GN_MTK_BSP

#if defined (GN_MTK_BSP)
static ssize_t show_update_pro(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
//   char ver[20];
//	snprintf(buf, VAR_CHAR_NUM_MAX, "%s", ver);
	return 0;
}
static ssize_t store_update_pro(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
  return 0; //liming
	flush_delayed_work_sync(&ghost_monitor_work);
	melfas_ts_fw_load(melfas_i2c_client);
    return count;        
}

#if 1
static DEVICE_ATTR(update, 0664, show_update_pro, store_update_pro);
#else
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
#endif

void tpd_hw_enable(void)
{
	TPD_DMESG("[tpd_hw_enable]\n");
    hwPowerOn(MT6323_POWER_LDO_VGP1, VOL_3000, "TP");//TOUCH  LDO
    msleep(25);
	#if MELFAS_CE_PIN_CONTROL//anderson
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	#endif
	msleep(30);
}

void tpd_hw_disable(void)
{
	TPD_DMESG("[tpd_hw_disable]\n");
	#if MELFAS_CE_PIN_CONTROL
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
    msleep(25);
	#endif
	hwPowerDown(MT6323_POWER_LDO_VGP1, "TP");//TOUCH  LDO
}

static void esd_rest_tp(void)
{
	printk("==========tp have inter esd =============\n");
    melfas_ts_release_all_finger();
	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	tpd_hw_disable();
	msleep(100);
	tpd_hw_enable();
	msleep(50);
	mms_config_start(info);
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
}


static int melfas_touch_event_handler(void *unused)
{
	uint8_t buf[TS_READ_REGS_LEN] = { 0 };
	int i, j, read_num, fingerID, Touch_Type = 0, touchState = 0;
	int keyID = 0, reportID = 0;
	int ret;

	int press_count = 0;
	int is_touch_mix = 0;

    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD }; 

    TPD_DMESG("[melfas_tpd]  melfas_touch_event_handler  %s\n", __func__);
    sched_setscheduler(current, SCHED_RR, &param); 

    do
    {
        set_current_state(TASK_INTERRUPTIBLE);
		
        wait_event_interruptible(melfas_waiter, melfas_tpd_flag != 0);
		TPD_DEBUG_SET_TIME;
        melfas_tpd_flag = 0;
        set_current_state(TASK_RUNNING); 

		mutex_lock(&i2c_access);
		
		//i2c_smbus_read_i2c_block_data(melfas_i2c_client, TS_READ_LEN_ADDR, 1, buf);
		melfas_i2c_read(melfas_i2c_client, TS_READ_LEN_ADDR, 1, buf);
		read_num = buf[0];
        TPD_DMESG("[melfas_tpd]  read_num  %d\n", read_num);
        printk ("[melfas_tpd ] read_num is %d\n", read_num);
		if(read_num)
		{
		  uint8_t *ptr_buf = buf;
		  int cnt = read_num;
		  int i = 0;
		  u8 reg = 0x10;
		  i2c_master_send(melfas_i2c_client, &reg, 1);

			for (i = 0 ; i < cnt ;  i += MMS_READ_BYTE){
			  if( i <= cnt - MMS_READ_BYTE){
				i2c_master_recv(melfas_i2c_client, ptr_buf, MMS_READ_BYTE);
				ptr_buf += MMS_READ_BYTE;
			  }else{
				i2c_master_recv(melfas_i2c_client, ptr_buf, cnt-i);
				ptr_buf += cnt -i;
			  }		
			}

			// printk("melfas:i2c 0-7  : %02x %02x %02x %02x    %02x %02x %02x %02x\n", buf[0],buf[1],buf[2],buf[3],  buf[4],buf[5],buf[6],buf[7]);   //liming need remove
			// printk("melfas:i2c 8-15 : %02x %02x %02x %02x    %02x %02x %02x %02x\n", buf[8],buf[9],buf[10],buf[11],  buf[12],buf[13],buf[14],buf[15]);//liming need remove
			// printk("melfas:i2c 16-23: %02x %02x %02x %02x    %02x %02x %02x %02x\n", buf[16],buf[17],buf[18],buf[19],  buf[20],buf[21],buf[22],buf[23]);//liming need remove

			if (0x0C == buf[0] || 0x0D == buf[0] || 0x0E == buf[0])
			{
				goto exit_work_func;
			}

          	if (0x0F == buf[0])
           	{
            	esd_rest_tp();
             	goto exit_work_func;
		 	}

			for (i = 0; i < read_num; i = i + MMS_READ_BYTE)
            {
                Touch_Type = (buf[i] >> 5) & 0x03;
				fingerID = (buf[i] & 0x0F) - 1;
				touchState = ((buf[i] & 0x80)==0x80);
				reportID = (buf[i] & 0x0F);
				keyID = reportID;

				//printk("melfas: Touch_Type:0x%x fingerID:0x%x touchState:0x%x reportID:0x%x keyID:0x%x\n", Touch_Type, fingerID, touchState, reportID, keyID);   //liming need remove

				if (fingerID >= TS_MAX_TOUCH)
					continue;

                /* touch type is panel */
				if (Touch_Type == TOUCH_SCREEN)
                {
                    int posX, posY;
                    
                    posX = (uint16_t)(buf[i + 1] & 0x0F) << 8 | buf[i + 2];
                    posY = g_Mtouch_info[fingerID].posY = (uint16_t)(buf[i + 1] & 0xF0) << 4 | buf[i + 3];

                    g_Mtouch_info[fingerID].posX = TPD_WARP_X(MMS_MAX_WIDTH, posX);
                    g_Mtouch_info[fingerID].posY = TPD_WARP_X(MMS_MAX_HEIGHT, posY);                   
                    g_Mtouch_info[fingerID].area = buf[i + 4];
					g_Mtouch_info[fingerID].status = touchState;


					//printk("melfas: x:%d\n", g_Mtouch_info[fingerID].posX);   //liming need remove
					//printk("melfas: y:%d\n", g_Mtouch_info[fingerID].posY);   //liming need remove

                    if (touchState)
						 g_Mtouch_info[fingerID].pressure = 10;
                        //g_Mtouch_info[fingerID].pressure = buf[i + 5];
                    else {
                        g_Mtouch_info[fingerID].pressure = 0;
						
						/*MTKLM_CHANGE_S : 2012-10-16 lm@mtk.com ghost finger pattern detection*/
						if(ghost_touch_cnt == 0)
						{
							ghost_x = g_Mtouch_info[fingerID].posX;
							ghost_y = g_Mtouch_info[fingerID].posY;
							ghost_touch_cnt++;
						}
						else
						{
							if(ghost_x + 40 >= g_Mtouch_info[fingerID].posX && ghost_x - 40 <= g_Mtouch_info[fingerID].posX)
								if(ghost_y + 40 >= g_Mtouch_info[fingerID].posY && ghost_y - 40 <= g_Mtouch_info[fingerID].posY)
									ghost_touch_cnt++;
						}
						/*MTKLM_CHANGE_E : 2012-10-16 lm@mtk.com ghost finger pattern detection*/
                   }
					is_touch_mix = 1;
                }
				else if (Touch_Type == TOUCH_KEY)
				{
					// virtual key with vibration response
					is_touch_mix = 1;
					g_Mtouch_info[0].status = touchState; 
					g_Mtouch_info[0].area = 1;
					g_Mtouch_info[0].pressure = touchState ? 10 : 0;

					//printk("melfas: keyID:%d\n", keyID);   //liming need remove

					if (keyID == 0x1)
					{
						g_Mtouch_info[0].posX = tpd_keys_dim_local[0][0];
						g_Mtouch_info[0].posY = tpd_keys_dim_local[0][1];
						is_key_pressed = PRESS_KEY;
					}
					else if (keyID == 0x2)
					{
						g_Mtouch_info[0].posX = tpd_keys_dim_local[1][0];
						g_Mtouch_info[0].posY = tpd_keys_dim_local[1][1];
						is_key_pressed = PRESS_KEY;
					}else if(keyID == 0x3){
					    g_Mtouch_info[0].posX = tpd_keys_dim_local[2][0];
						g_Mtouch_info[0].posY = tpd_keys_dim_local[2][1];
						is_key_pressed = PRESS_KEY;
					}
					else
					{
						is_touch_mix = 0;
						printk("  Error keyID is %d!!!!!!!!!!\n", keyID);
					}
					
					for (j = 1; j < TS_MAX_TOUCH; j++)
					{
						if (g_Mtouch_info[j].status == -1 && g_Mtouch_info[j].pressure == -1)
							continue;
						g_Mtouch_info[j].status = -1;
						g_Mtouch_info[j].pressure = -1;
						is_touch_pressed = 1;
					}
					if (is_touch_pressed)
					{
						input_report_key(tpd->dev, BTN_TOUCH, 0);
						input_mt_sync(tpd->dev);
						input_sync(tpd->dev);
						is_touch_pressed = 0;
					}

				}
            }

			press_count = 0;
			if(is_touch_mix){		
				for (i = 0; i < TS_MAX_TOUCH; i++)
				{
				  //printk("melfas: touch:%d presure:%d status:%d\n", i,g_Mtouch_info[i].pressure,g_Mtouch_info[i].status);   //liming need remove

					if (g_Mtouch_info[i].pressure == -1)
						continue;

					if (g_Mtouch_info[i].status == 0){
						//is_touch_pressed = 0;
						g_Mtouch_info[i].status = -1;
						continue;
					}
					if(g_Mtouch_info[i].status == 1){
					  if(g_Mtouch_info[i].pressure == 0){
							REPORT_MT_UP(i, g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].area, g_Mtouch_info[i].pressure);
					  }
					  else if(g_Mtouch_info[i].pressure == 10){
							REPORT_MT_DOWN(i, g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].area, g_Mtouch_info[i].pressure);
					  }
						else ;

						press_count++;
					}
					if (g_Mtouch_info[i].pressure == 0)
						g_Mtouch_info[i].pressure = -1;
				}
				
				if(press_count == 0) 
                {
                    input_report_key(tpd->dev, BTN_TOUCH, 0);
					input_mt_sync(tpd->dev);
                }
			}
			is_touch_mix = 0;
			input_sync(tpd->dev);
		}
exit_work_func:	
		mutex_unlock(&i2c_access);
    } while ( !kthread_should_stop() ); 

    return 0;
}

static void melfas_i2c_tpd_eint_interrupt_handler(void)
{ 
    TPD_DEBUG_PRINT_INT;

	//printk("liming melfas_i2c_tpd_eint_interrupt_handler\n"); //liming need remove

	#ifdef MTK_EVB_TEST
	printk("liming melfas_i2c_tpd_eint_interrupt_handler\n");
	#endif 

    melfas_tpd_flag=1;
    wake_up_interruptible(&melfas_waiter);
} 

int melfas_i2c_write_bytes( struct i2c_client *client, u16 addr, int len, u8 *txbuf )
{
    u8 buffer[MELFAS_MAX_TRANSACTION_LENGTH]={0};
    u16 left = len;
    u8 offset = 0;
    u8 retry = 0;

    TPD_DEBUG("[melfas_tpd]    melfas_i2c_write_data");
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

int melfas_i2c_write_data(struct i2c_client *client, int len, u8 *txbuf)
{
    u16 ret = 0;
    TPD_DEBUG("[melfas_tpd]    melfas_i2c_write_data");
	client->addr = client->addr&I2C_MASK_FLAG;

    if ( txbuf == NULL )
        return -1;

	ret = i2c_master_send(client, txbuf, len);

	if (ret != len)
		return -1;

    return 0;
}

int melfas_i2c_read_data(struct i2c_client *client, int len, u8 *rxbuf)
{
    u16 ret = 0;

    TPD_DEBUG("[melfas_tpd]    melfas_i2c_read_data");
	client->addr = client->addr&I2C_MASK_FLAG;

    if ( rxbuf == NULL )
        return -1;

	ret = i2c_master_recv(client, rxbuf, len);
TPD_DEBUG("[melfas_tpd]    melfas_i2c_read_data %d",len);
	if (ret != len)
		return -1;

    return 0;
}

int melfas_i2c_read(struct i2c_client *client, u16 addr, u16 len, u8 *rxbuf)
{
    u8 buffer[MELFAS_I2C_DEVICE_ADDRESS_LEN]={0};
    u16 left = len;
    u8 offset = 0;

    TPD_DEBUG("[melfas_tpd]    melfas_i2c_read");
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

    TPD_DEBUG("[melfas_tpd] i2c_read_bytes to device %02X address %04X len %d\n", client->addr, addr, len );

    while ( left > 0 )
    {
        buffer[0] = (u8)addr+offset;

        msg[1].buf = &rxbuf[offset];

        if ( left > MELFAS_MAX_I2C_TRANSFER_SIZE )
        {
            msg[1].len = MELFAS_MAX_I2C_TRANSFER_SIZE;
            left -= MELFAS_MAX_I2C_TRANSFER_SIZE;
            offset += MELFAS_MAX_I2C_TRANSFER_SIZE;
        }
        else
        {
            msg[1].len = left;
            left = 0;
        }

        if( i2c_transfer( client->adapter, &msg[0], 2 ) != 2 )
            {
                TPD_DMESG("[melfas_tpd] I2C read 0x%X length=%d failed\n", addr + offset, len);
                return -1;
            }

        }

    return 0;
}

static DEFINE_SPINLOCK(touch_reg_operation_spinlock);

static unsigned short g_wdt_original_data;

/* This function will disable watch dog */
void mtk_touch_wdt_disable(void)
{
	/* unsigned short tmp; */

	/* spin_lock(&touch_reg_operation_spinlock); */

	/* tmp = DRV_Reg16(MTK_WDT_MODE); */
	/* g_wdt_original_data = tmp; */
	/* tmp |= MTK_WDT_MODE_KEY; */
	/* tmp &= ~MTK_WDT_MODE_ENABLE; */

	/* DRV_WriteReg16(MTK_WDT_MODE,tmp); */

	/* spin_unlock(&touch_reg_operation_spinlock); */
}

void mtk_touch_wdt_restart(void)
        {
/* 	unsigned short tmp; */

/*     // Reset WatchDogTimer's counting value to time out value */
/*     // ie., keepalive() */

/* //    DRV_WriteReg16(MTK_WDT_RESTART, MTK_WDT_RESTART_KEY); */

/* 	spin_lock(&touch_reg_operation_spinlock); */

/* 	DRV_WriteReg16(MTK_WDT_MODE,g_wdt_original_data); */
/* 	tmp = DRV_Reg16(MTK_WDT_MODE); */

/* 	spin_unlock(&touch_reg_operation_spinlock); */
	
        }

/* static DEFINE_SPINLOCK(touch_gpt_lock); */
/* static unsigned long touch_gpt_flags; */

/* const UINT32 touch_gpt_mask[GPT_TOTAL_COUNT+1] = { */
/*     0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0 */
/* }; */

/* void TOUCH_GPT_DisableIRQ(GPT_NUM numGPT) */
/*             { */
/*     if (numGPT == GPT1 || numGPT == GPT2) */
/*         return; */

/*     spin_lock_irqsave(&touch_gpt_lock, touch_gpt_flags); */
    
/*     DRV_ClrReg32(GPT_IRQEN, touch_gpt_mask[numGPT]); */

/*     spin_unlock_irqrestore(&touch_gpt_lock, touch_gpt_flags); */
/* } */

/* void TOUCH_GPT_EnableIRQ(GPT_NUM numGPT) */
/* { */
/*     if (numGPT == GPT1 || numGPT == GPT2) */
/*         return; */
 
/*     spin_lock_irqsave(&touch_gpt_lock, touch_gpt_flags); */

/*     DRV_SetReg32(GPT_IRQEN, touch_gpt_mask[numGPT]); */

/*     spin_unlock_irqrestore(&touch_gpt_lock, touch_gpt_flags); */
/* } */


static int melfas_ts_fw_load(struct i2c_client *client)
{
    int ret = 0;
    
    printk("[TSP] %s: \n", __func__);

	// Tempraly remove
   	/* mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); */
	/* mtk_touch_wdt_disable(); */
	/* TOUCH_GPT_DisableIRQ(GPT5); */

	/* ret = mms100_ISC_download_binary_data(); */
	/* if (ret) */
	/* 	printk("<MELFAS> SET Download ISP Fail\n"); */
	
	/* TOUCH_GPT_EnableIRQ(GPT5); */
	/* mtk_touch_wdt_restart(); */
	/* mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */

	/* tpd_hw_disable(); */
	/* msleep(100); */
	/* tpd_hw_enable(); */
	/* msleep(100); */

    return ret;
}

#if 0//anderson
static int melfas_check_firmware(struct i2c_client *client, u8 *val)
{
    int ret = 0;

    ret = i2c_smbus_read_i2c_block_data(client, TS_READ_HW_VER_ADDR, 1, &val[0]);
    ret = i2c_smbus_read_i2c_block_data(client, MELFAS_FW_VERSION, 1, &val[1]);

    printk("[melfas_tpd]:HW Version[0x%02x] SW Version[0x%02x] \n", val[0], val[1]);
	return ret;
}
#endif

#if 0//anderson
static int melfas_firmware_update(struct i2c_client *client)
{
 
  return 0; //liming:

    int ret = 0;
    uint8_t fw_ver[2] = {0, };
	
    ret = melfas_check_firmware(client, fw_ver);
    if (ret < 0)
	{
		printk("[melfas_tpd] check_firmware fail! [%d]", ret);
		melfas_ts_fw_load(client);
	}
    else
    {
		if (fw_ver[1] < MELFAS_CURRENT_FW_VERSION)
			ret = melfas_ts_fw_load(client);
    }

    return ret;
}
#endif

static ssize_t melfas_FW_show(struct device *dev,  struct device_attribute *attr,  char *buf)
{
	int r;
	u8 product_id;
	u8 product_id2;

	r = snprintf(buf, PAGE_SIZE, "%d\n", touch_fw_version);

	i2c_smbus_read_i2c_block_data(melfas_i2c_client, MELFAS_FW_VERSION, sizeof(product_id), &product_id);
	i2c_smbus_read_i2c_block_data(melfas_i2c_client, TS_READ_SW_VER_ADDR, sizeof(product_id2), &product_id2);

	return sprintf(buf, "MELFAS_FW_VERSION = %d, TS_READ_SW_VER_ADDR = %u\n", product_id, product_id2);
}

static DEVICE_ATTR(fw, 0664, melfas_FW_show, NULL);
EXPORT_SYMBOL(melfas_FW_show);


static ssize_t show_melfas_reset(struct device *dev,  struct device_attribute *attr,  char *buf)
{
	tpd_hw_disable();
	msleep(100);
	tpd_hw_enable();
	msleep(100);
	
	return 0;
}

/*read dummy interrupts.*/
static ssize_t store_melfas_reset(struct kobject *kobj, struct kobj_attribute *attr, const char *buffer100, size_t count)
{
	uint8_t buf[TS_READ_REGS_LEN] = {0,};
	int read_num = 0;

	i2c_smbus_read_i2c_block_data(melfas_i2c_client, TS_READ_LEN_ADDR, 1, buf);

	read_num = buf[0];
	if(read_num)
	{
        i2c_smbus_read_i2c_block_data(melfas_i2c_client, TS_READ_START_ADDR, read_num, buf);

		if(0x0F == buf[0])
			esd_rest_tp();
	}
		
	return count;        
}
static DEVICE_ATTR(reset, 0664, show_melfas_reset, store_melfas_reset);

static void read_dummy_interrupt(void)
{
	uint8_t buf[TS_READ_REGS_LEN] = {0,};
	int read_num = 0;

	i2c_smbus_read_i2c_block_data(melfas_i2c_client, TS_READ_LEN_ADDR, 1, buf);

	read_num = buf[0];
	if(read_num)
	{
		i2c_smbus_read_i2c_block_data(melfas_i2c_client, TS_READ_START_ADDR, read_num, buf);

		if(0x0F == buf[0])
			esd_rest_tp();
	}
    return;
}

/*MTKLM_CHANGE_S : 2012-10-16 lm@mtk.com ghost finger pattern detection*/
static void monitor_ghost_finger(struct work_struct *work)
{
    TPD_DMESG("[melfas_tpd] monitor_ghost_finger   %s\n", __func__);
	if(ghost_touch_cnt >= 45){
		printk("<MELFAS> ghost finger pattern DETECTED! : %d \n", ghost_touch_cnt);
		tpd_hw_disable();
		//flush_work_sync(&ts->work);
		melfas_tpd_flag=1;
		melfas_ts_release_all_finger();
		input_mt_sync(tpd->dev);
		input_sync(tpd->dev);
		tpd_hw_enable();
		msleep(50);
		mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
		mms_config_start(info);
		mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	}
	
	schedule_delayed_work(&ghost_monitor_work, msecs_to_jiffies(HZ * 50));

	ghost_touch_cnt = 0;
	ghost_x = 1000;
	ghost_y = 1000;
	return;
}
/*MTKLM_CHANGE_E : 2012-10-16 lm@mtk.com ghost finger pattern detection*/

static int melfas_tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{             
    int err = 0, i;
	struct task_struct *thread = NULL;
	const char *fw_name = FW_NAME;
	
	TPD_DMESG("[melfas_tpd] %s\n", __func__);
	
	//power on
	tpd_hw_enable();
    msleep(50);  

    melfas_i2c_client = client;
	
#if 0//defined (GN_MTK_BSP)
		err = tpd_create_attr();
		if(err)
		{
			TPD_DMESG(" tpd create attribute err = %d\n", err);
		}
#else
	err = device_create_file(&client->dev, &dev_attr_update);
	if (err) {
		printk( "Touchscreen : update_touch device_create_file: Fail\n");
		device_remove_file(&client->dev, &dev_attr_update);
		return err;
	}
#endif

	err = device_create_file(&client->dev, &dev_attr_fw);
	if (err) {
		printk( "Touchscreen : fw_touch device_create_file: Fail\n");
		device_remove_file(&client->dev, &dev_attr_fw);
		return err;
	}
	err = device_create_file(&client->dev, &dev_attr_reset);
	if (err) {
		printk( "Touchscreen : reset_touch device_create_file: Fail\n");
		device_remove_file(&client->dev, &dev_attr_reset);
		return err;
	}

#ifdef TPD_HAVE_BUTTON
	for(i =0; i < TPD_KEY_COUNT; i ++){
		input_set_capability(tpd->dev,EV_KEY,tpd_keys_local[i]);
	}
#endif

#if 0
	melfas_firmware_update(client);
#else
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		TPD_DMESG("Failed to allocated memory\n");
		return -ENOMEM;
	}

	info->client = client;
	//info->input_dev = input_dev;
	info->pdata = client->dev.platform_data;
	init_completion(&info->init_done);
	info->irq = -1;
	info->run_count = 3;

	mutex_init(&info->lock);
	snprintf(info->phys, sizeof(info->phys),"%s/input0", dev_name(&client->dev));

	i2c_set_clientdata(client, info);

	info->fw_name = kstrdup(fw_name, GFP_KERNEL);

	mms_fw_update_controller(NULL, info);
	mms_config_start(info);
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

    mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
    mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, melfas_i2c_tpd_eint_interrupt_handler, 1);

	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    tpd_load_status = 1;

	/*MTKLM_CHANGE_S : 2012-10-16 lm@mtk.com ghost finger pattern detection*/
	INIT_DELAYED_WORK(&ghost_monitor_work, monitor_ghost_finger);
	schedule_delayed_work(&ghost_monitor_work, msecs_to_jiffies(HZ * 50));
	/*MTKLM_CHANGE_E : 2012-10-16 lm@mtk.com ghost finger pattern detection*/

	read_dummy_interrupt();

	#ifdef MTK_EVB_TEST
	{
	  u8 product_id;
	  u8 product_id2;

	  i2c_smbus_read_i2c_block_data(melfas_i2c_client, MELFAS_FW_VERSION, sizeof(product_id), &product_id);
	  i2c_smbus_read_i2c_block_data(melfas_i2c_client, TS_READ_SW_VER_ADDR, sizeof(product_id2), &product_id2);

	  printk("liming  Korea 134: MELFAS_FW_VERSION = %d, TS_READ_SW_VER_ADDR = %u\n", product_id, product_id2);
	}
	#endif 

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
  	#if 0//def TPD_HAVE_BUTTON     
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);
	#endif   

#ifdef TPD_HAVE_BUTTON
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif
    
    tpd_type_cap = 1;

    return 0;
}

static void melfas_ts_release_all_finger(void)
{
	int i;
    TPD_DMESG("[melfas_tpd] %s\n", __func__);

	for(i=0; i<TS_MAX_TOUCH; i++)
	{
		g_Mtouch_info[i].pressure = -1;
	}
	input_mt_sync(tpd->dev); 
	input_sync(tpd->dev);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_tpd_early_suspend(struct early_suspend *h)
{
//    TPD_DMESG("[melfas_tpd] %s\n", __func__);
 
    melfas_ts_release_all_finger();
	//irq mask
	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	//power down
	tpd_hw_disable();
	/*MTKLM_CHANGE_S : 2012-10-16 lm@mtk.com ghost finger pattern detection*/
	flush_delayed_work_sync(&ghost_monitor_work);
	/*MTKLM_CHANGE_E : 2012-10-16 lm@mtk.com ghost finger pattern detection*/
     return;
}

static void melfas_tpd_late_resume(struct early_suspend *h)
{
//    TPD_DMESG("[melfas_tpd] %s\n", __func__);
    //power on
  
	tpd_hw_enable();
	melfas_ts_release_all_finger();
	/*MTKLM_CHANGE_S : 2012-09-11 lm@mtk.com. timing issue*/
	msleep(100);
	mms_config_start(info);
   	//irq unmask
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	
	/*MTKLM_CHANGE_S : 2012-10-16 lm@mtk.com ghost finger pattern detection*/
	ghost_touch_cnt = 0;
	schedule_delayed_work(&ghost_monitor_work, msecs_to_jiffies(HZ * 50));
	/*MTKLM_CHANGE_E : 2012-10-16 lm@mtk.com ghost finger pattern detection*/
    return;
}
#endif
extern int mms_get_fw_version(struct i2c_client *client, u8 *buf);
int mms_get_tpd_fw_version(void)
{
    //MELFAS_CHIP_FW_VERSION
    //char *product_id ;
    //i2c_smbus_read_i2c_block_data(melfas_i2c_client, MELFAS_FW_VERSION, sizeof(product_id), &product_id);
    //return *product_id;
    u8  product_id[3] ;
    	TPD_DMESG("[melfas_tpd] %s\n", __func__);

    return mms_get_fw_version(melfas_i2c_client,product_id);
}

static struct tpd_driver_t melfas_tpd_device_driver =
{
    .tpd_device_name = "melfas_mms134",
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
	#ifdef MTK_EVB_TEST
	//#if 0
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, 1);
    mt_set_gpio_mode(GPIO_GPS_SYNC_PIN, 1); // I2C mode

    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_dir(GPIO_GPS_SYNC_PIN, GPIO_DIR_OUT);

    mt_set_gpio_pull_enable(GPIO_GPS_SYNC_PIN, GPIO_PULL_DISABLE);
    mt_set_gpio_pull_enable(GPIO_GPS_SYNC_PIN, GPIO_PULL_DISABLE);

	i2c_register_board_info(0, &melfas_i2c_tpd, 1);
#else
	i2c_register_board_info(1, &melfas_i2c_tpd, 1);
#endif 

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


