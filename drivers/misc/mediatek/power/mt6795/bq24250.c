#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/hwmsen_helper.h>
#include <linux/xlog.h>


#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include "bq24250.h"

#include "cust_charging.h"
#include <mach/charging.h>

/**********************************************************
  *
  *   [I2C Slave Setting] 
  *
  *********************************************************/
#define BQ24250_SLAVE_ADDR_WRITE	0xD4
#define bq24250_SLAVE_ADDR_READ		0xD5

static struct i2c_client *new_client = NULL;
static const struct i2c_device_id bq24250_i2c_id[] = {{"bq24250",0},{}};   
kal_bool chargin_hw_init_done = KAL_FALSE; 
static int bq24250_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);

static struct i2c_driver bq24250_driver = {
    .driver = {
        .name    = "bq24250",
    },
    .probe       = bq24250_driver_probe,
    .id_table    = bq24250_i2c_id,
};

/**********************************************************
  *
  *   [Global Variable] 
  *
  *********************************************************/
kal_uint8 bq24250_reg[bq24250_REG_NUM] = {0};

static DEFINE_MUTEX(bq24250_i2c_access);
/**********************************************************
  *
  *   [I2C Function For Read/Write fan5405] 
  *
  *********************************************************/
int bq24250_read_byte(kal_uint8 cmd, kal_uint8 *returnData)
{
    char     cmd_buf[1]={0x00};
    char     readData = 0;
    int      ret=0;

    mutex_lock(&bq24250_i2c_access);
    
    //new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG;    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;

    cmd_buf[0] = cmd;
    ret = i2c_master_send(new_client, &cmd_buf[0], (1<<8 | 1));
    if (ret < 0) 
    {    
        //new_client->addr = new_client->addr & I2C_MASK_FLAG;
        new_client->ext_flag=0;

        mutex_unlock(&bq24250_i2c_access);
        return 0;
    }
    
    readData = cmd_buf[0];
    *returnData = readData;

    // new_client->addr = new_client->addr & I2C_MASK_FLAG;
    new_client->ext_flag=0;
    
    mutex_unlock(&bq24250_i2c_access);    
    return 1;
}

int bq24250_write_byte(kal_uint8 cmd, kal_uint8 writeData)
{
    char    write_data[2] = {0};
    int     ret=0;
    
    mutex_lock(&bq24250_i2c_access);
    
    write_data[0] = cmd;
    write_data[1] = writeData;
    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;
    
    ret = i2c_master_send(new_client, write_data, 2);
    if (ret < 0) 
    {
       
        new_client->ext_flag=0;
        mutex_unlock(&bq24250_i2c_access);
        return 0;
    }
    
    new_client->ext_flag=0;
    mutex_unlock(&bq24250_i2c_access);
    return 1;
}

/**********************************************************
  *
  *   [Read / Write Function] 
  *
  *********************************************************/
kal_uint32 bq24250_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 bq24250_reg = 0;
    int ret = 0;

   battery_xlog_printk(BAT_LOG_FULL,"--------------------------------------------------\n");

    ret = bq24250_read_byte(RegNum, &bq24250_reg);

	battery_xlog_printk(BAT_LOG_FULL,"[bq24250_read_interface] Reg[%x]=0x%x\n", RegNum, bq24250_reg);
	
    bq24250_reg &= (MASK << SHIFT);
    *val = (bq24250_reg >> SHIFT);
	
	battery_xlog_printk(BAT_LOG_FULL,"[bq24250_read_interface] val=0x%x\n", *val);
	
    return ret;
}

kal_uint32 bq24250_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 bq24250_reg = 0;
    int ret = 0;

    battery_xlog_printk(BAT_LOG_FULL,"--------------------------------------------------\n");
	
    ret = bq24250_read_byte(RegNum, &bq24250_reg);
    battery_xlog_printk(BAT_LOG_FULL,"[bq24250_config_interface] Reg[%x]=0x%x\n", RegNum-bq24250_CON0, bq24250_reg);
    
    bq24250_reg &= ~(MASK << SHIFT);
    bq24250_reg |= (val << SHIFT);

	if(RegNum == bq24250_CON1 && val == 1 && MASK ==CON1_RST_MASK && SHIFT == CON1_RST_SHIFT)
	{
		// RESET bit
	}else if(RegNum == bq24250_CON1)
	{
		bq24250_reg &= ~0x80;	//RESET bit read returs 1, so clear it
	}

    ret = bq24250_write_byte(RegNum, bq24250_reg);
    battery_xlog_printk(BAT_LOG_FULL,"[bq24250_config_interface] write Reg[%x]=0x%x\n", RegNum-bq24250_CON0, bq24250_reg);

    // Check
    //bq24250_read_byte(RegNum, &bq24250_reg);
    //printk("[bq24250_config_interface] Check Reg[%x]=0x%x\n", RegNum, bq24250_reg);

    return ret;
}

//write one register directly
kal_uint32 bq24250_config_interface_liao (kal_uint8 RegNum, kal_uint8 val)
{   
    int ret = 0;
    
    ret = bq24250_write_byte(RegNum, val);

    return ret;
}

/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/
//CON0----------------------------------------------------
int bq24250_get_wdt_fault(void)
{
	kal_uint32 ret=0, val=0;  
	
	ret = bq24250_read_interface(   (kal_uint8)(bq24250_CON0), 
				                    (kal_uint8*)(&val),
				                    (kal_uint8)(CON0_WD_FAULT_MASK),
				                    (kal_uint8)(CON0_WD_FAULT_SHIFT)
				                    ); 

	return val;
}

void bq24250_set_en_wdt(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON0), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON0_WD_EN_MASK),
                                    (kal_uint8)(CON0_WD_EN_SHIFT)
                                    );
}

void bq24250_set_wdt_rst()
{
    bq24250_set_en_wdt(1);
}

int bq24250_get_state(void)
{
	kal_uint32 ret=0, val=0;
	
	ret = bq24250_read_interface(   (kal_uint8)(bq24250_CON0), 
				                    (kal_uint8*)(&val),
				                    (kal_uint8)(CON0_STATE_MASK),
				                    (kal_uint8)(CON0_STATE_SHIFT)
				                    ); 

	return val;
}

int bq24250_get_fault(void)
{
	kal_uint32 ret=0, val=0;  
	
	ret = bq24250_read_interface(   (kal_uint8)(bq24250_CON0), 
				                    (kal_uint8*)(&val),
				                    (kal_uint8)(CON0_FAULT_MASK),
				                    (kal_uint8)(CON0_FAULT_SHIFT)
				                    ); 

	return val;
}

//CON1----------------------------------------------------
void bq24250_set_reg_rst(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_RST_MASK),
                                    (kal_uint8)(CON1_RST_SHIFT)
                                    );
}

void bq24250_set_iinlim(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_LIN_LIMIT_MASK),
                                    (kal_uint8)(CON1_LIN_LIMIT_SHIFT)
                                    );	
}

void bq24250_set_en_stat(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_EN_STAT_MASK),
                                    (kal_uint8)(CON1_EN_STAT_SHIFT)
                                    );	
}

void bq24250_set_en_term(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_EN_TERM_MASK),
                                    (kal_uint8)(CON1_EN_TERM_SHIFT)
                                    );		
}

void bq24250_set_en_chg(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_EN_CHG_MASK),
                                    (kal_uint8)(CON1_EN_CHG_SHIFT)
                                    );	
}

void bq24250_set_en_hiz(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_HZ_MODE_MASK),
                                    (kal_uint8)(CON1_HZ_MODE_SHIFT)
                                    );	
}

//CON2----------------------------------------------------
void bq24250_set_vreg(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_VBAT_REG_MASK),
                                    (kal_uint8)(CON2_VBAT_REG_SHIFT)
                                    );	
}

int bq24250_get_usb_det(void)
{
	kal_uint32 ret=0, val=0;  
	
	ret = bq24250_read_interface(   (kal_uint8)(bq24250_CON2), 
				                    (kal_uint8*)(&val),
				                    (kal_uint8)(CON2_USB_DET_MASK),
				                    (kal_uint8)(CON2_USB_DET_SHIFT)
				                    ); 

	return val;
}

//CON3----------------------------------------------------
void bq24250_set_ichg(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON3), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON3_ICHG_MASK),
                                    (kal_uint8)(CON3_ICHG_SHIFT)
                                    );	
}

unsigned int bq24250_get_ichg(void)
{
	kal_uint32 ret=0, val=0;    

    ret=bq24250_read_interface(   (kal_uint8)(bq24250_CON3), 
                                    (kal_uint8)(&val),
                                    (kal_uint8)(CON3_ICHG_MASK),
                                    (kal_uint8)(CON3_ICHG_SHIFT)
                                    );	

	return val;
}

void bq24250_set_iterm(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON3), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON3_ITERM_MASK),
                                    (kal_uint8)(CON3_ITERM_SHIFT)
                                    );	
}

//CON4----------------------------------------------------
int bq24250_get_loop_state(void)
{
	kal_uint32 ret=0, val=0;  
	
	ret = bq24250_read_interface(   (kal_uint8)(bq24250_CON4), 
				                    (kal_uint8*)(&val),
				                    (kal_uint8)(CON4_LOOP_STATUS_MASK),
				                    (kal_uint8)(CON4_LOOP_STATUS_SHIFT)
				                    ); 

	return val;
}

void bq24250_set_low_chg(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON4), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON4_LOW_CHG_MASK),
                                    (kal_uint8)(CON4_LOW_CHG_SHIFT)
                                    );	
}

void bq24250_set_dpdm(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON4), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON4_DPDM_EN_MASK),
                                    (kal_uint8)(CON4_DPDM_EN_SHIFT)
                                    );	
}

int bq24250_get_ce_state(void)
{
	kal_uint32 ret=0, val=0;  
	
	ret = bq24250_read_interface(   (kal_uint8)(bq24250_CON4), 
				                    (kal_uint8*)(&val),
				                    (kal_uint8)(CON4_CE_STATUS_MASK),
				                    (kal_uint8)(CON4_CE_STATUS_SHIFT)
				                    ); 

	return val;
}

void bq24250_set_vin_dpm(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON4), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON4_VIN_DPM_MASK),
                                    (kal_uint8)(CON4_VIN_DPM_SHIFT)
                                    );	
}

//CON5----------------------------------------------------
void bq24250_set_en_2xtimer(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_2XTMR_EN_MASK),
                                    (kal_uint8)(CON5_2XTMR_EN_SHIFT)
                                    );	
}

void bq24250_set_tmr(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_TMR_MASK),
                                    (kal_uint8)(CON5_TMR_SHIFT)
                                    );	
}

void bq24250_set_en_sysoff(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_SYSOFF_MASK),
                                    (kal_uint8)(CON5_SYSOFF_SHIFT)
                                    );	
}

void bq24250_set_en_ts(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_TS_EN_MASK),
                                    (kal_uint8)(CON5_TS_EN_SHIFT)
                                    );	
}

int bq24250_get_ts_state(void)
{
	kal_uint32 ret=0, val=0;  
	
	ret = bq24250_read_interface(   (kal_uint8)(bq24250_CON5), 
				                    (kal_uint8*)(&val),
				                    (kal_uint8)(CON5_TS_STAT_MASK),
				                    (kal_uint8)(CON5_TS_STAT_SHIFT)
				                    ); 

	return val;
}

//CON6----------------------------------------------------
void bq24250_set_en_vovp(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON6), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON6_VOVP_MASK),
                                    (kal_uint8)(CON6_VOVP_SHIFT)
                                    );	
}

void bq24250_set_cls_vdp(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON6), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON6_CLR_VDP_MASK),
                                    (kal_uint8)(CON6_CLR_VDP_SHIFT)
                                    );	
}

void bq24250_set_force_batdet(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON6), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON6_FORCE_BATDET_MASK),
                                    (kal_uint8)(CON6_FORCE_BATDET_SHIFT)
                                    );	
}

void bq24250_set_force_ptm(kal_uint32 val)
{
	kal_uint32 ret=0;    

    ret=bq24250_config_interface(   (kal_uint8)(bq24250_CON6), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON6_FORCE_PTM_MASK),
                                    (kal_uint8)(CON6_FORCE_PTM_SHIFT)
                                    );	
}


/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/
void bq24250_dump_register(void)
{
    int i=0;
    printk("[bq24250] ");

#if 1
    for (i=0;i<bq24250_REG_NUM;i++)
    {
        bq24250_read_byte(i+bq24250_CON0, &bq24250_reg[i]);
        printk("[0x%x]=0x%x ", i, bq24250_reg[i]);        
    }
    printk("\n");
#endif

}

static int bq24250_driver_probe(struct i2c_client *client, const struct i2c_device_id *id) 
{             
    int err=0; 

    battery_xlog_printk(BAT_LOG_CRTI,"[bq24250_driver_probe] \n");

    if (!(new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) {
        err = -ENOMEM;
        goto exit;
    }    
    memset(new_client, 0, sizeof(struct i2c_client));

    new_client = client;    

    //---------------------
    bq24250_dump_register();
    chargin_hw_init_done = KAL_TRUE;
	
    return 0;                                                                                       

exit:
    return err;

}

/**********************************************************
  *
  *   [platform_driver API] 
  *
  *********************************************************/
kal_uint8 g_reg_value_bq24250=0;
static ssize_t show_bq24250_access(struct device *dev,struct device_attribute *attr, char *buf)
{
    battery_xlog_printk(BAT_LOG_FULL,"[show_fan5405_access] 0x%x\n", g_reg_value_bq24250);
    return sprintf(buf, "%u\n", g_reg_value_bq24250);
}
static ssize_t store_bq24250_access(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    int ret=0;
    char *pvalue = NULL;
    unsigned int reg_value = 0;
    unsigned int reg_address = 0;
    
    battery_xlog_printk(BAT_LOG_FULL,"[store_bq24250_access] \n");
    
    if(buf != NULL && size != 0)
    {
        battery_xlog_printk(BAT_LOG_FULL,"[store_bq24250_access] buf is %s and size is %d \n",buf,size);
        reg_address = simple_strtoul(buf,&pvalue,16);
        
        if(size > 3)
        {        
            reg_value = simple_strtoul((pvalue+1),NULL,16);        
            battery_xlog_printk(BAT_LOG_FULL,"[store_bq24250_access] write bq24250 reg 0x%x with value 0x%x !\n",reg_address,reg_value);
            ret=bq24250_config_interface(reg_address, reg_value, 0xFF, 0x0);
        }
        else
        {    
            ret=bq24250_read_interface(reg_address, &g_reg_value_bq24250, 0xFF, 0x0);
            battery_xlog_printk(BAT_LOG_FULL,"[store_bq24250_access] read bq24250 reg 0x%x with value 0x%x !\n",reg_address,g_reg_value_bq24250);
            battery_xlog_printk(BAT_LOG_FULL,"[store_bq24250_access] Please use \"cat bq24250_access\" to get value\r\n");
        }        
    }    
    return size;
}
static DEVICE_ATTR(bq24250_access, 0664, show_bq24250_access, store_bq24250_access); //664

static int bq24250_user_space_probe(struct platform_device *dev)    
{    
    int ret_device_file = 0;

    battery_xlog_printk(BAT_LOG_CRTI,"******** bq24250_user_space_probe!! ********\n" );
    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_bq24250_access);
    
    return 0;
}

struct platform_device bq24250_user_space_device = {
    .name   = "bq24250-user",
    .id     = -1,
};

static struct platform_driver bq24250_user_space_driver = {
    .probe      = bq24250_user_space_probe,
    .driver     = {
        .name = "bq24250-user",
    },
};


static struct i2c_board_info __initdata i2c_bq24250 = { I2C_BOARD_INFO("bq24250", (BQ24250_SLAVE_ADDR_WRITE>>1))};

static int __init bq24250_init(void)
{    
    int ret=0;
    
    battery_xlog_printk(BAT_LOG_CRTI,"[bq24250_init] init start\n");
    
    i2c_register_board_info(2, &i2c_bq24250, 1);

    if(i2c_add_driver(&bq24250_driver)!=0)
    {
        battery_xlog_printk(BAT_LOG_CRTI,"[bq24250_init] failed to register bq24250 i2c driver.\n");
    }
    else
    {
        battery_xlog_printk(BAT_LOG_CRTI,"[bq24250_init] Success to register bq24250 i2c driver.\n");
    }

    // fan5405 user space access interface
    ret = platform_device_register(&bq24250_user_space_device);
    if (ret) {
        battery_xlog_printk(BAT_LOG_CRTI,"****[bq24250_init] Unable to device register(%d)\n", ret);
        return ret;
    }    
    ret = platform_driver_register(&bq24250_user_space_driver);
    if (ret) {
        battery_xlog_printk(BAT_LOG_CRTI,"****[bq24250_init] Unable to register driver (%d)\n", ret);
        return ret;
    }
    
    return 0;        
}

static void __exit bq24250_exit(void)
{
    i2c_del_driver(&bq24250_driver);
}

module_init(bq24250_init);
module_exit(bq24250_exit);
   
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C fan5405 Driver");
MODULE_AUTHOR("James Lo<james.lo@mediatek.com>");

