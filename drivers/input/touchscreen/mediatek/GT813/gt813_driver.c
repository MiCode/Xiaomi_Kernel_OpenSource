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

#include <linux/proc_fs.h>
#include <asm/uaccess.h>


#include "tpd_custom_GT813.h"
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <mach/mt_gpt.h>

#include "tpd.h"
#include <cust_eint.h>
#include <linux/jiffies.h>

#ifndef TPD_NO_GPIO 
#include "cust_gpio_usage.h"
#endif

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
#define TPD_OK 0

#define TPD_CONFIG_REG_BASE           0xF80
#define TPD_TOUCH_INFO_REG_BASE       0xF40
#define TPD_POINT_INFO_REG_BASE       0xF42
#define TPD_VERSION_INFO_REG          0xF7D
#define TPD_POWER_MODE_REG            0xFF2
#define TPD_HANDSHAKING_START_REG     0xFFF
#define TPD_HANDSHAKING_END_REG       0x8000
#define TPD_PANEL_ID				  0x0FF5
#define TPD_POINT_INFO_LEN      5
#define TPD_MAX_POINTS          5
#define MAX_TRANSACTION_LENGTH  8
#define I2C_DEVICE_ADDRESS_LEN  2
#define I2C_MASTER_CLOCK        400

#define MAX_I2C_TRANSFER_SIZE (MAX_TRANSACTION_LENGTH - I2C_DEVICE_ADDRESS_LEN)

#define GT813_CONFIG_PROC_FILE "gt813_config"
#define CONFIG_LEN (112)

#define CREATE_WR_NODE
#ifdef CREATE_WR_NODE
extern s32 init_wr_node(struct i2c_client*);
extern void uninit_wr_node(void);
#endif


#define RESOLUTION_LOC      69
#define TRIGGER_LOC         64
#define TPD_X_RES           540
#define TPD_Y_RES           960

#define TPD_WARP_X(x)  (x)
#define TPD_WARP_Y(y)  (y)


struct tpd_info_t
{
    u8 product_id;
    u8 version_1;
    u8 version_2;
};

static struct i2c_client *i2c_client = NULL;
static const struct i2c_device_id tpd_i2c_id[] = {{"mtk-tpd",0},{}};
static unsigned short force[] = {0, 0xBA, I2C_CLIENT_END,I2C_CLIENT_END};
static const unsigned short * const forces[] = { force, NULL };
//static struct i2c_client_address_data addr_data = { .forces = forces,};
static struct i2c_board_info __initdata i2c_tpd={ I2C_BOARD_INFO("mtk-tpd", (0xBA>>1))};
static struct i2c_driver tpd_i2c_driver =
{                       
    .probe = tpd_i2c_probe,                                   
    .remove = tpd_i2c_remove,                           
    .detect = tpd_i2c_detect,                           
    .driver.name = "mtk-tpd", 
    .id_table = tpd_i2c_id,                             
    //.address_data = &addr_data,
    .address_list = (const unsigned short*) forces,                         
}; 
struct tpd_info_t tpd_info;
static u8 *cfg_data = NULL;
static u8 *cfg_data_with_charger = NULL;

/* mika 2012.2.28, esd protect
 */
//#define ESD_PROTECT
#ifdef ESD_PROTECT
#define TPD_I2C_DISABLE_REG 0x8000
static struct delayed_work tpd_esd_check_work;
static struct workqueue_struct * tpd_esd_check_workqueue = NULL;
static void tpd_esd_check_func(struct work_struct *);
#define ESD_CHECK_CIRCLE  500
#define ESD_CHECK_DATA_LEN  6
#define ESD_CHECK_TIME      3
unsigned char esd_check_data[ESD_CHECK_TIME*ESD_CHECK_DATA_LEN];
int esd_checked_time = 0;
#endif

static tpd_reset_guitar(){
	//Reset Guitar
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
        
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
    msleep(10);  
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
    msleep(10);  
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
    msleep(10);  
}

/* proc file system */
static int i2c_read_bytes( struct i2c_client *client, u16 addr, u8 *rxbuf, int len );
static int i2c_write_bytes( struct i2c_client *client, u16 addr, u8 *txbuf, int len );
static int i2c_write_dummy( struct i2c_client *client, u16 addr );
static struct proc_dir_entry *gt813_config_proc = NULL;

static int gt813_config_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    char *ptr = page;
    char temp_data[CONFIG_LEN] = {0};
	int panel_id[1]={0};
	int i;

    ptr += sprintf( ptr, "==== GT813 config init value====\n" );

    for ( i = 0 ; i < CONFIG_LEN ; i++ )
    {
        ptr += sprintf( ptr, "0x%02X ", cfg_data[i] );

        if ( i%11 == 10 )
            ptr += sprintf( ptr, "\n" );
    }    
      
    ptr += sprintf( ptr, "\n" );
    ptr += sprintf( ptr, "==== GT813 charger init config ====\n" );
    for ( i = 0 ; i < CONFIG_LEN ; i++ )
    {
        ptr += sprintf( ptr, "0x%02X ", cfg_data_with_charger[i] );

        if ( i%11 == 10 )
            ptr += sprintf( ptr, "\n" );
    }

    ptr += sprintf( ptr, "\n" );
    ptr += sprintf( ptr, "==== GT813 config real value====\n" );
  //  i2c_write_dummy( i2c_client, TPD_HANDSHAKING_START_REG );	
    i2c_read_bytes( i2c_client, TPD_CONFIG_REG_BASE, temp_data, CONFIG_LEN);
    i2c_write_dummy( i2c_client, TPD_HANDSHAKING_END_REG );	

    for ( i = 0 ; i < CONFIG_LEN ; i++ )
    {
        ptr += sprintf( ptr, "0x%02X ", temp_data[i] );

        if ( i%11 == 10 )
            ptr += sprintf( ptr, "\n" );
    }

	/* read tpd panel id */
    ptr += sprintf( ptr, "\n" );
    ptr += sprintf( ptr, "==== GT813 panel id value====\n" );
    i2c_read_bytes( i2c_client, TPD_PANEL_ID, panel_id, 1);
    i2c_write_dummy( i2c_client, TPD_HANDSHAKING_END_REG );
    ptr += sprintf( ptr, "0x%02X ", panel_id[0] );

	/* */
	ptr += sprintf( ptr, "\n" );
    *eof = 1;
    return ( ptr - page );
}

static int gt813_config_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
    TPD_DEBUG("write count %ld\n", count );

    if ( count != (CONFIG_LEN*2 ) )
    {
        TPD_DEBUG("size not match [%d:%ld]\n", CONFIG_LEN*2, count );
        return -EFAULT;
    }

    if (copy_from_user( cfg_data, buffer, count/2))
    {
        TPD_DEBUG("copy from user fail\n");
        return -EFAULT;
    }
    
    if (copy_from_user( cfg_data_with_charger, buffer + CONFIG_LEN, count/2))
    {
        TPD_DEBUG("copy from user fail\n");
        return -EFAULT;
    }
  //  i2c_write_dummy( i2c_client, TPD_HANDSHAKING_START_REG );

    i2c_write_bytes( i2c_client, TPD_CONFIG_REG_BASE, cfg_data, CONFIG_LEN );

    i2c_write_dummy( i2c_client, TPD_HANDSHAKING_END_REG );
    return count;
}

/* esd protect */
#ifdef ESD_PROTECT
int i2c_enable_commands( struct i2c_client *client, u16 addr)
{
	u8 retry;
	u8 txbuf[2] = {0};

	if ( txbuf == NULL )
        return -1;

	txbuf[0] = ( addr >> 8 ) & 0xFF;
	txbuf[1] = addr & 0xFF;

	i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;// | I2C_ENEXT_FLAG;

	retry = 0;
    while ( i2c_master_send(i2c_client, &txbuf[0], I2C_DEVICE_ADDRESS_LEN ) < 0 )
    {
        retry++;

        if ( retry == 5 )
        {
            i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;
            TPD_DEBUG("I2C read 0x%X length=%d failed\n", addr, I2C_DEVICE_ADDRESS_LEN);
            return -1;
        }
    }

    return 0;
}

int tpd_init_panel()
{
	int err = 0;
#if 1	
	err = i2c_write_bytes( i2c_client, TPD_CONFIG_REG_BASE, cfg_data, sizeof( cfg_data ) );
#endif
	return err;
}

static tpd_reset_power(){
	//Power off TP
#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerDown(TPD_POWER_SOURCE_CUSTOM, "TP");
#else
	hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
	hwPowerDown(TPD_POWER_SOURCE_1800, "TP");
#endif 

    msleep(30);
    
    //Power on TP
#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
#else
	hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
	hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
#endif

    msleep(30);

}



static force_reset_guitar()
{
    int i;
	int err;
 
	#if 1
	tpd_reset_power();
	#else
    //Power off TP
    hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
    msleep(30);
    //Power on TP
    hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
    msleep(30);
    #endif

    for ( i = 0; i < 5; i++)
    {
        //Reset Guitar
		#if 1
		tpd_reset_guitar();
		#else
        mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
        mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
        msleep(10);  
        mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
        msleep(10);  
        mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
        msleep(20);
		#endif
        //Send config
		#if 0
        if (tpd_init_panel())
        {
            continue;
        }
		#else
		/* mika add */
		err = i2c_write_bytes( i2c_client, TPD_CONFIG_REG_BASE, cfg_data, CONFIG_LEN);
		i2c_write_dummy( i2c_client, TPD_HANDSHAKING_END_REG );
		if(err < 0 ){
			continue;
		}
		/* mika end */
		#endif

        i2c_enable_commands(i2c_client, TPD_I2C_DISABLE_REG);
        break;
    }

}

static void tpd_esd_check_func(struct work_struct *work)
{
    int ret = -1;
    int i;

    TPD_DEBUG("[TPD ESD TEST] %s !\n",__func__);
    
	if (tpd_halt)
    {
        return;
    }

    for (i = 0; i < 3; i++)
    {
	    if (i2c_enable_commands(i2c_client, TPD_I2C_DISABLE_REG))
	    {
            TPD_DEBUG("[TPD ESD TEST] I2C communication failed :(\n");
	        continue;
	    }
        TPD_DEBUG("[TPD ESD TEST] I2C communication sucessed !\n");
	    break;
	}
	
	if (i >= 3)
    {
        force_reset_guitar();
    }
    else
    {
        i2c_read_bytes(i2c_client, 0x0394, &esd_check_data[esd_checked_time * ESD_CHECK_DATA_LEN], ESD_CHECK_DATA_LEN);
        
        if ((++esd_checked_time) >= 3)
        {
            if (!strncmp(&esd_check_data[0], &esd_check_data[ESD_CHECK_DATA_LEN], ESD_CHECK_DATA_LEN)
             && !strncmp(&esd_check_data[0], &esd_check_data[2*ESD_CHECK_DATA_LEN], ESD_CHECK_DATA_LEN))
            {
				TPD_DEBUG("[TPD ESD TEST] data check failed :( ... reset ...\n");
                force_reset_guitar();
            }
            esd_checked_time = 0;
        }
    }

    if(tpd_halt) {
		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
    } else {
        queue_delayed_work(tpd_esd_check_workqueue, &tpd_esd_check_work, 1000);
    }

    return;
}
#endif

static int gt813_check_data(unsigned char *buffer, int count)
{
    unsigned char buf[128] = {0}; // need > sizeof(buffer)
    int i = 0, error = -1, ret = -1;

    do
    {
    //    i2c_write_dummy( i2c_client, TPD_HANDSHAKING_START_REG );		
        ret = i2c_read_bytes( i2c_client, TPD_CONFIG_REG_BASE, buf, CONFIG_LEN);
        i2c_write_dummy( i2c_client, TPD_HANDSHAKING_END_REG );		

        if(ret)
        {
            TPD_DMESG("read i2c error\n");
            break;
        }

        for(i = 0; i < CONFIG_LEN; i++) // the last one byte will be changed
        {
            if(buf[i] != cfg_data[i]) 
            {
                TPD_DMESG(TPD_DEVICE " fail to write touch panel config, %d bytes, expect:0x%x, real:0x%x\n", i,cfg_data[i], buf[i]);
                error = -1;
                break;
            }
        }

        if(i == CONFIG_LEN)
        {
            TPD_DMESG(TPD_DEVICE " write touch panel config OK, count:%d\n", count);
            error = 0;
            break;
        }

        if(error == -1)
        {
            for(i = 0; i < CONFIG_LEN - 1; i++)
            {
                printk("  0x%02X", buf[i]);

                if(i%8 == 7)
                    printk("\n");
            }
          //  i2c_write_dummy( i2c_client, TPD_HANDSHAKING_START_REG );
            i2c_write_bytes( i2c_client, TPD_CONFIG_REG_BASE, cfg_data, CONFIG_LEN );
            i2c_write_dummy( i2c_client, TPD_HANDSHAKING_END_REG );				
        }
    } while (count--);

    return error;
}

static int i2c_read_bytes( struct i2c_client *client, u16 addr, u8 *rxbuf, int len )
{
    u8 buffer[I2C_DEVICE_ADDRESS_LEN];
    u8 retry;
    u16 left = len;
    u16 offset = 0;

    struct i2c_msg msg[2] =
    {
        {
            .addr = ((client->addr&I2C_MASK_FLAG )|(I2C_ENEXT_FLAG )),
            .flags = 0,
            .buf = buffer,
            .len = I2C_DEVICE_ADDRESS_LEN,
            .timing = I2C_MASTER_CLOCK
        },
        {
            .addr = ((client->addr&I2C_MASK_FLAG )|(I2C_ENEXT_FLAG )),
            .flags = I2C_M_RD,
            .timing = I2C_MASTER_CLOCK
        },
    };

    if ( rxbuf == NULL )
        return -1;

    TPD_DEBUG("i2c_read_bytes to device %02X address %04X len %d\n", client->addr, addr, len );

    while ( left > 0 )
    {
        buffer[0] = ( ( addr+offset ) >> 8 ) & 0xFF;
        buffer[1] = ( addr+offset ) & 0xFF;

        msg[1].buf = &rxbuf[offset];

        if ( left > MAX_TRANSACTION_LENGTH )
        {
            msg[1].len = MAX_TRANSACTION_LENGTH;
            left -= MAX_TRANSACTION_LENGTH;
            offset += MAX_TRANSACTION_LENGTH;
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

            if ( retry == 20 )
            {
                TPD_DEBUG("I2C read 0x%X length=%d failed\n", addr + offset, len);
                TPD_DMESG("I2C read 0x%X length=%d failed\n", addr + offset, len);
                return -1;
            }
        }
    }

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
        .flags = 0,
        .buf = buffer,
        .timing = I2C_MASTER_CLOCK,
    };


    if ( txbuf == NULL )
        return -1;

    TPD_DEBUG("i2c_write_bytes to device %02X address %04X len %d\n", client->addr, addr, len );

    while ( left > 0 )
    {
        retry = 0;

        buffer[0] = ( (addr+offset) >> 8 ) & 0xFF;
        buffer[1] = ( addr+offset ) & 0xFF;

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

            if ( retry == 20 )
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

static int i2c_write_dummy( struct i2c_client *client, u16 addr )
{
    u8 buffer[MAX_TRANSACTION_LENGTH];

    struct i2c_msg msg =
    {
        .addr = client->addr,
        .flags = 0,
        .buf = buffer,
        .timing = I2C_MASTER_CLOCK,
        .len = 2
    };

    TPD_DEBUG("i2c_write_dummy to device %02X address %04X\n", client->addr, addr );

    buffer[0] = (addr >> 8) & 0xFF;
    buffer[1] = (addr) & 0xFF;

    i2c_transfer( client->adapter, &msg, 1 ); 

    return 0;
}

static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
    strcpy(info->type, "mtk-tpd");
    return 0;
}

static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{             
    int err = 0;
    int retry = 0;
#ifdef ESD_PROTECT
	int ret;
#endif

    //power on, need confirm with SA
#ifdef TPD_POWER_SOURCE_CUSTOM
		hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
#else
		hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
#endif

#ifdef TPD_POWER_SOURCE_1800
		hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
#endif
   
    // set deep sleep off
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);  
    msleep(10);  
 
     
    memset( &tpd_info, 0, sizeof( struct tpd_info_t ) );
   // i2c_write_dummy( client, TPD_HANDSHAKING_START_REG );
    err = i2c_read_bytes( client, TPD_VERSION_INFO_REG, (u8 *)&tpd_info, sizeof( struct tpd_info_t ) );
    i2c_write_dummy( client, TPD_HANDSHAKING_END_REG );		
    if ( err )
    {
        TPD_DMESG(TPD_DEVICE " fail to get tpd info %d\n", err );
        return err;
    }
    else
    {
        TPD_DMESG( "TPD info\n");
        TPD_DMESG( "product %02X\n", tpd_info.product_id );
        TPD_DMESG( "version %02X %02X\n", tpd_info.version_1, tpd_info.version_2 );

        // check if upgradable
        //gt818_downloader( client, tpd_info.version_2*256 + tpd_info.version_1, goodix_gt818_firmware );

    }

    i2c_client = client;
   
    // Create proc file system
    gt813_config_proc = create_proc_entry( GT813_CONFIG_PROC_FILE, 0666, NULL);

    if ( gt813_config_proc == NULL )
    {
        TPD_DEBUG("create_proc_entry %s failed\n", GT813_CONFIG_PROC_FILE );
    }
    else 
    {
        gt813_config_proc->read_proc = gt813_config_read_proc;
        gt813_config_proc->write_proc = gt813_config_write_proc;
    }
	#ifdef USE_TRULY_CTP
	cfg_data = cfg_data_version_TRULY;
	cfg_data_with_charger = cfg_data_version_TRULY;
	#else
	cfg_data = cfg_data_version_OFG;
	cfg_data_with_charger = cfg_data_version_OFG;
    #endif
#ifdef CREATE_WR_NODE
    init_wr_node(client);
#endif
  
    #ifdef TPD_HAVE_BUTTON
    for(retry =0; retry < 3; retry ++)
    {
	input_set_capability(tpd->dev,EV_KEY,tpd_keys_local[retry]);
    }
    #endif
    // setting resolution, RES_X, RES_Y
    cfg_data[RESOLUTION_LOC]     = ((TPD_X_RES>>8)&0xff);
    cfg_data[RESOLUTION_LOC + 1] = (TPD_X_RES&0xff);
    cfg_data[RESOLUTION_LOC + 2] = ((TPD_Y_RES>>8)&0xff);
    cfg_data[RESOLUTION_LOC + 3] = (TPD_Y_RES&0xff);
    cfg_data[TRIGGER_LOC] |= 0x08;

    //i2c_write_dummy( client, TPD_HANDSHAKING_START_REG );
    err = i2c_write_bytes( client, TPD_CONFIG_REG_BASE, cfg_data, CONFIG_LEN );
	//err = tpd_init_panel();
    i2c_write_dummy( client, TPD_HANDSHAKING_END_REG );	
/*
    err = gt813_check_data(cfg_data, 3); 

    if ( err )
    {
        TPD_DMESG(TPD_DEVICE " retry TPD_MAX_RESET_COUNT fail to write tpd cfg %d\n", err );
        return err;
    }
//*/
    thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);

    if (IS_ERR(thread))
    { 
        err = PTR_ERR(thread);
        TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", err);
    }
 

    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
    msleep(10);

    // set INT mode
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_DISABLE);
    //mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);

    //mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
    //mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
    //mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY, tpd_eint_interrupt_handler, 1);
    mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, tpd_eint_interrupt_handler, 1);
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);

    tpd_load_status = 1;


#ifdef ESD_PROTECT
    tpd_esd_check_workqueue = create_workqueue("tpd_esd_check");
    INIT_DELAYED_WORK(&tpd_esd_check_work, tpd_esd_check_func);
    ret = queue_delayed_work(tpd_esd_check_workqueue, &tpd_esd_check_work,1000); 
    printk("[TSP] ret =%d\n",ret);
#endif
    
    return 0;
}

static void tpd_eint_interrupt_handler(void)
{ 
    TPD_DEBUG_PRINT_INT;
    tpd_flag=1;
    wake_up_interruptible(&waiter);
} 
static int tpd_i2c_remove(struct i2c_client *client)
{
    
#ifdef CREATE_WR_NODE
    uninit_wr_node();
#endif

#ifdef ESD_PROTECT
    destroy_workqueue(tpd_esd_check_workqueue);
#endif

    return 0;
}

static void tpd_down(int x, int y, int size, int id)
{
    input_report_abs(tpd->dev, ABS_PRESSURE, size);
    input_report_key(tpd->dev, BTN_TOUCH, 1);
    input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, size);
    input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, 0);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    /* track id Start 0 */
    //input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, id);
    input_mt_sync(tpd->dev);
    //TPD_DEBUG_PRINT_POINT( x, y, 1 );
    TPD_EM_PRINT(x, y, x, y, size, 1);
//add by hupeng 120216 for ftm ,already report key_value ,so don't use tpd_button
#if 0
    if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
    {   
        tpd_button(x, y, 1);  
    }
#endif
}

static void tpd_up(int x, int y, int id)
{
    //input_report_abs(tpd->dev, ABS_PRESSURE, 0);
    input_report_key(tpd->dev, BTN_TOUCH, 0);
    //input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
    //input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, 0);
    //input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    //input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    /* track id Start 0 */
    //input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, id);
    input_mt_sync(tpd->dev);
    TPD_EM_PRINT(x, y, x, y, id, 0);
	//add by hupeng 120216 for ftm ,already report key_value ,so don't use tpd_button
#if 0
    if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
    {   
        tpd_button(x, y, 0); 
    }
#endif
    //TPD_DEBUG_PRINT_POINT( x, y, 0 );
}

static int touch_event_handler(void *unused)
{
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD }; 
    int x = 0;
    int y = 0;
    int id = 0;
    int size = 0;
    int finger_num = 0;
    static u8 buffer[ TPD_POINT_INFO_LEN*TPD_MAX_POINTS + 3 ];
    static u8 id_mask = 0;
    static u8 key_history = 0;
    u8 key;
    u8 chk_sum = 0;
    u8 cur_mask;
    int idx;
    static int x_history[TPD_MAX_POINTS+1];
    static int y_history[TPD_MAX_POINTS+1];

	int buf_id;

    sched_setscheduler(current, SCHED_RR, &param); 

    do
    {
		TPD_DEBUG("%s entry\n", __func__);

        set_current_state(TASK_INTERRUPTIBLE);

        while ( tpd_halt )
        {
            tpd_flag = 0;
            msleep(20);
        }

        wait_event_interruptible(waiter, tpd_flag != 0);
        tpd_flag = 0;
        TPD_DEBUG_SET_TIME;
        set_current_state(TASK_RUNNING); 
        
       // i2c_write_dummy( i2c_client, TPD_HANDSHAKING_START_REG );

        i2c_read_bytes( i2c_client, TPD_TOUCH_INFO_REG_BASE, buffer, 28);
        i2c_write_dummy( i2c_client, TPD_HANDSHAKING_END_REG ); 
        
        printk("[mtk-tpd] STATUS : %x\n", buffer[0]);

        if( ( buffer[0]&0xC0 ) != 0x80 )
        {
            continue;        
        }   

        if ( buffer[0] & 0x20 )
        {
            printk("Report large area\n");
        }

        key = buffer[1] & 0x0f;
        if ( key == 0x0f)
        {
            int err; 
            
            printk("reload config.\n");
			err = i2c_write_bytes( i2c_client, TPD_CONFIG_REG_BASE, cfg_data, CONFIG_LEN );
            i2c_write_dummy( i2c_client, TPD_HANDSHAKING_END_REG );	

            if (err)
            {
                printk("Reload config failed!\n");
            }
        }

        cur_mask = buffer[0]&0x1F;	// 0001 1111, five fingers
		finger_num = 0;
		chk_sum = 0;

        for (idx = 0; (idx < TPD_MAX_POINTS); idx++)
        {
            if ((cur_mask >> idx) & 0x01)
            {
                finger_num++;
            }
        }

        for (idx = 0; idx < TPD_POINT_INFO_LEN * finger_num; idx++)
        {
            chk_sum += buffer[idx + 2];
        }
        if (chk_sum != buffer[TPD_POINT_INFO_LEN * finger_num + 2])
        {
			printk("Check sum error!\n");
		    continue;
        }

/*        finger_num = buffer[0]&0x1F;

        if ( tpd == NULL || tpd->dev == NULL )
        {
            i2c_write_dummy( i2c_client, TPD_HANDSHAKING_END_REG ); 
            continue;
        }

        if ( finger_num )
        {
            i2c_read_bytes( i2c_client, TPD_POINT_INFO_REG_BASE, buffer, TPD_MAX_POINTS*TPD_POINT_INFO_LEN);
        }
        else
        {
            i2c_read_bytes( i2c_client, TPD_POINT_INFO_REG_BASE, buffer, 1);
        }
*/
        
        for ( idx = 0 ; idx < TPD_MAX_POINTS ; idx++ )
        {
            u8 *ptr = &buffer[ idx*TPD_POINT_INFO_LEN + 2];
            id = idx;

            if ( ((cur_mask>>idx)&0x01) && (id < TPD_MAX_POINTS) )
            {
                x = ptr[1] + (((int)ptr[0]) << 8);
                y = ptr[3] + (((int)ptr[2]) << 8);
                size = ptr[4];

                //if ( x == 0 )
                //    x = 1;

                tpd_down( TPD_WARP_X(x), TPD_WARP_Y(y), size, id);
                printk("[touch]x:%d, y:%d, size:%d, id:%d.\n", x, y, size, id);

                x_history[id] = x;
                y_history[id] = y;
            }
            else
                TPD_DEBUG("Invalid id %d\n", id );
        }   

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
                    tpd_up( TPD_WARP_X(x_history[idx]), TPD_WARP_Y(y_history[idx]), id);                    
                }

                diff = ( diff >> 1 );
                idx++;
            }
            id_mask = cur_mask;
        }

        //Report key event
        if((key_history != 0) || (key != 0))
        {
//[[modified by renyufeng for entering SAFE MODE 201203062157        
          	if(6 == key)
				key = 0;
//modified by renyufeng for entering SAFE MODE 201203062157			
            for(idx = 0; idx < 3; idx++)
            {              
		input_report_key(tpd->dev, tpd_keys_local[idx], !!(key&(0x01<<idx)));
		//input_report_key(tpd->kpd, tpd_keys_local[idx], !!(key&(0x01<<idx)));
            }
        }
        key_history = key; 

        if ( tpd != NULL && tpd->dev != NULL )
            input_sync(tpd->dev);

      //  i2c_write_dummy( i2c_client, TPD_HANDSHAKING_END_REG );

    } while ( !kthread_should_stop() ); 

    return 0;
}

static int tpd_local_init(void) 
{

    if(i2c_add_driver(&tpd_i2c_driver)!=0)
    {
        TPD_DMESG("unable to add i2c driver.\n");
        return -1;
    }
    if(tpd_load_status == 0)
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

    // set vendor string
    tpd->dev->id.product = tpd_info.product_id;
    tpd->dev->id.version = (tpd_info.version_2 << 8 ) | tpd_info.version_1;

    TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);  
    tpd_type_cap = 1;

    return 0;
}

/* Function to manage low power suspend */
//void tpd_suspend(struct i2c_client *client, pm_message_t message)
static void tpd_suspend( struct early_suspend *h )
{
    u8 mode = 0xC0;
    tpd_halt = 1;     
    msleep(1);     
  //  i2c_write_dummy( i2c_client, TPD_HANDSHAKING_START_REG );
    i2c_write_bytes( i2c_client, TPD_POWER_MODE_REG, &mode, 1 );
    i2c_write_dummy( i2c_client, TPD_HANDSHAKING_END_REG );    
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

#ifdef ESD_PROTECT
    flush_workqueue(tpd_esd_check_workqueue);
#endif
}

/* Function to manage power-on resume */
//void tpd_resume(struct i2c_client *client)
static void tpd_resume( struct early_suspend *h )
{
    TPD_DMESG(TPD_DEVICE " tpd_resume start \n"); 
//#ifdef ESD_PROTECT
	/* CTP ESD TEST
	reset ctp when ctp resume
	mika add 2012.2.28
	*/
	tpd_reset_guitar();
//#endif

	/* resume int pin */
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_EINT_PIN, GPIO_OUT_ZERO);
    msleep(1);
    mt_set_gpio_out(GPIO_CTP_EINT_PIN, GPIO_OUT_ONE);
    msleep(1);
    mt_set_gpio_out(GPIO_CTP_EINT_PIN, GPIO_OUT_ZERO);
    msleep(1);
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);

    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 

    tpd_halt = 0;
    TPD_DMESG(TPD_DEVICE " tpd_resume end \n" ); 	    
		
#ifdef ESD_PROTECT
	queue_delayed_work(tpd_esd_check_workqueue, &tpd_esd_check_work,1000);
#endif
}

static struct tpd_driver_t tpd_device_driver =
{
    .tpd_device_name = "gt813",
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
static int __init tpd_driver_init(void)
{
    TPD_DMESG("MediaTek gt813 touch panel driver init\n");
    i2c_register_board_info(0, &i2c_tpd, 1);
    if ( tpd_driver_add(&tpd_device_driver) < 0)
        TPD_DMESG("add generic driver failed\n");

    return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void)
{
    TPD_DMESG("MediaTek gt813 touch panel driver exit\n");
    //input_unregister_device(tpd->dev);
    tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

