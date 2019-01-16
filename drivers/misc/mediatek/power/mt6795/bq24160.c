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

#include "bq24160.h"
#include "cust_charging.h"
#include <mach/charging.h>

#if defined(CONFIG_MTK_FPGA)
#else
#include <cust_i2c.h>
#endif

/**********************************************************
  *
  *   [I2C Slave Setting] 
  *
  *********************************************************/
#define bq24160_SLAVE_ADDR_WRITE   0xD6
#define bq24160_SLAVE_ADDR_Read    0xD7

#ifdef I2C_SWITHING_CHARGER_CHANNEL
#define bq24160_BUSNUM I2C_SWITHING_CHARGER_CHANNEL
#else
#define bq24160_BUSNUM 0//2
#endif

static struct i2c_client *new_client = NULL;
static const struct i2c_device_id bq24160_i2c_id[] = {{"bq24160",0},{}};   
kal_bool chargin_hw_init_done = KAL_FALSE; 
static int bq24160_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);

static struct i2c_driver bq24160_driver = {
    .driver = {
        .name    = "bq24160",
    },
    .probe       = bq24160_driver_probe,
    .id_table    = bq24160_i2c_id,
};

/**********************************************************
  *
  *   [Global Variable] 
  *
  *********************************************************/
kal_uint8 bq24160_reg[bq24160_REG_NUM] = {0};

static DEFINE_MUTEX(bq24160_i2c_access);

int g_bq24160_hw_exist=0;

/**********************************************************
  *
  *   [I2C Function For Read/Write bq24160] 
  *
  *********************************************************/
kal_uint32 bq24160_read_byte(kal_uint8 cmd, kal_uint8 *returnData)
{
    char     cmd_buf[1]={0x00};
    char     readData = 0;
    int      ret=0;

    mutex_lock(&bq24160_i2c_access);
    
    //new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG;    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;

    cmd_buf[0] = cmd;
    ret = i2c_master_send(new_client, &cmd_buf[0], (1<<8 | 1));
    if (ret < 0) 
    {    
        //new_client->addr = new_client->addr & I2C_MASK_FLAG;
        new_client->ext_flag=0;

        mutex_unlock(&bq24160_i2c_access);
        return 0;
    }
    
    readData = cmd_buf[0];
    *returnData = readData;

    // new_client->addr = new_client->addr & I2C_MASK_FLAG;
    new_client->ext_flag=0;
    
    mutex_unlock(&bq24160_i2c_access);    
    return 1;
}

kal_uint32 bq24160_write_byte(kal_uint8 cmd, kal_uint8 writeData)
{
    char    write_data[2] = {0};
    int     ret=0;
    
    mutex_lock(&bq24160_i2c_access);
    
    write_data[0] = cmd;
    write_data[1] = writeData;
    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;
    
    ret = i2c_master_send(new_client, write_data, 2);
    if (ret < 0) 
    {
       
        new_client->ext_flag=0;
        mutex_unlock(&bq24160_i2c_access);
        return 0;
    }
    
    new_client->ext_flag=0;
    mutex_unlock(&bq24160_i2c_access);
    return 1;
}

/**********************************************************
  *
  *   [Read / Write Function] 
  *
  *********************************************************/
kal_uint32 bq24160_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 bq24160_reg = 0;
    kal_uint32 ret = 0;

    battery_xlog_printk(BAT_LOG_FULL,"--------------------------------------------------\n");

    ret = bq24160_read_byte(RegNum, &bq24160_reg);

    battery_xlog_printk(BAT_LOG_FULL,"[bq24160_read_interface] Reg[%x]=0x%x\n", RegNum, bq24160_reg);
	
    bq24160_reg &= (MASK << SHIFT);
    *val = (bq24160_reg >> SHIFT);
	
    battery_xlog_printk(BAT_LOG_FULL,"[bq24160_read_interface] val=0x%x\n", *val);
	
    return ret;
}

kal_uint32 bq24160_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 bq24160_reg = 0;
    kal_uint32 ret = 0;

    battery_xlog_printk(BAT_LOG_FULL,"--------------------------------------------------\n");

    ret = bq24160_read_byte(RegNum, &bq24160_reg);
    battery_xlog_printk(BAT_LOG_FULL,"[bq24160_config_interface] Reg[%x]=0x%x\n", RegNum, bq24160_reg);
    
    bq24160_reg &= ~(MASK << SHIFT);
    bq24160_reg |= (val << SHIFT);

    if(RegNum == bq24160_CON2 && val == 1 && MASK ==CON2_RESET_MASK && SHIFT == CON2_RESET_SHIFT)
    {
        // RESET bit
    }
    else if(RegNum == bq24160_CON2)
    {
        bq24160_reg &= ~0x80;	//RESET bit read returs 1, so clear it
    }
	 

    ret = bq24160_write_byte(RegNum, bq24160_reg);
    battery_xlog_printk(BAT_LOG_FULL,"[bq24160_config_interface] write Reg[%x]=0x%x\n", RegNum, bq24160_reg);

    // Check
    //bq24160_read_byte(RegNum, &bq24160_reg);
    //printk("[bq24160_config_interface] Check Reg[%x]=0x%x\n", RegNum, bq24160_reg);

    return ret;
}

//write one register directly
kal_uint32 bq24160_reg_config_interface (kal_uint8 RegNum, kal_uint8 val)
{   
    kal_uint32 ret = 0;
    
    ret = bq24160_write_byte(RegNum, val);

    return ret;
}

/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/
//CON0----------------------------------------------------
void bq24160_set_tmr_rst(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON0), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON0_TMR_RST_MASK),
                                    (kal_uint8)(CON0_TMR_RST_SHIFT)
                                    );
}

kal_uint32 bq24160_get_stat(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24160_read_interface(     (kal_uint8)(bq24160_CON0), 
                                    (&val),
                                    (kal_uint8)(CON0_STAT_MASK),
                                    (kal_uint8)(CON0_STAT_SHIFT)
                                    );
    return val;
}

void bq24160_set_supply_sel(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON0), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON0_SUPPLY_SEL_MASK),
                                    (kal_uint8)(CON0_SUPPLY_SEL_SHIFT)
                                    );
}

kal_uint32 bq24160_get_fault(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24160_read_interface(     (kal_uint8)(bq24160_CON0), 
                                    (&val),
                                    (kal_uint8)(CON0_FAULT_MASK),
                                    (kal_uint8)(CON0_FAULT_SHIFT)
                                    );
    return val;
}

//CON1----------------------------------------------------
kal_uint32 bq24160_get_instat(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24160_read_interface(     (kal_uint8)(bq24160_CON1), 
                                    (&val),
                                    (kal_uint8)(CON1_INSTAT_MASK),
                                    (kal_uint8)(CON1_INSTAT_SHIFT)
                                    );
    return val;
}

kal_uint32 bq24160_get_usbstat(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24160_read_interface(     (kal_uint8)(bq24160_CON1), 
                                    (&val),
                                    (kal_uint8)(CON1_USBSTAT_MASK),
                                    (kal_uint8)(CON1_USBSTAT_SHIFT)
                                    );
    return val;
}

void bq24160_set_otg_lock(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_OTG_LOCK_MASK),
                                    (kal_uint8)(CON1_OTG_LOCK_SHIFT)
                                    );
}

kal_uint32 bq24160_get_batstat(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24160_read_interface(     (kal_uint8)(bq24160_CON1), 
                                    (&val),
                                    (kal_uint8)(CON1_BATSTAT_MASK),
                                    (kal_uint8)(CON1_BATSTAT_SHIFT)
                                    );
    return val;
}

void bq24160_set_en_nobatop(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON1), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON1_EN_NOBATOP_MASK),
                                    (kal_uint8)(CON1_EN_NOBATOP_SHIFT)
                                    );
}

//CON2----------------------------------------------------
void bq24160_set_reset(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_RESET_MASK),
                                    (kal_uint8)(CON2_RESET_SHIFT)
                                    );
}

void bq24160_set_iusb_limit(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_IUSB_LIMIT_MASK),
                                    (kal_uint8)(CON2_IUSB_LIMIT_SHIFT)
                                    );
}

void bq24160_set_en_stat(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_EN_STAT_MASK),
                                    (kal_uint8)(CON2_EN_STAT_SHIFT)
                                    );
}

void bq24160_set_te(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_TE_MASK),
                                    (kal_uint8)(CON2_TE_SHIFT)
                                    );
}

void bq24160_set_ce(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_CE_MASK),
                                    (kal_uint8)(CON2_CE_SHIFT)
                                    );
}

void bq24160_set_hz_mode(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON2), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON2_HZ_MODE_MASK),
                                    (kal_uint8)(CON2_HZ_MODE_SHIFT)
                                    );
}

//CON3----------------------------------------------------
void bq24160_set_vbreg(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON3), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON3_VBREG_MASK),
                                    (kal_uint8)(CON3_VBREG_SHIFT)
                                    );
}

void bq24160_set_i_in_limit(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON3), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON3_I_IN_LIMIT_MASK),
                                    (kal_uint8)(CON3_I_IN_LIMIT_SHIFT)
                                    );
}

void bq24160_set_dpdm_en(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON3), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON3_DPDM_EN_MASK),
                                    (kal_uint8)(CON3_DPDM_EN_SHIFT)
                                    );
}

//CON4----------------------------------------------------
kal_uint32 bq24160_get_vender_code(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24160_read_interface(     (kal_uint8)(bq24160_CON4), 
                                    (&val),
                                    (kal_uint8)(CON4_VENDER_CODE_MASK),
                                    (kal_uint8)(CON4_VENDER_CODE_SHIFT)
                                    );
    return val;
}

kal_uint32 bq24160_get_pn(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24160_read_interface(     (kal_uint8)(bq24160_CON4), 
                                    (&val),
                                    (kal_uint8)(CON4_PN_MASK),
                                    (kal_uint8)(CON4_PN_SHIFT)
                                    );
    return val;
}

kal_uint32 bq24160_get_revision(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24160_read_interface(     (kal_uint8)(bq24160_CON4), 
                                    (&val),
                                    (kal_uint8)(CON4_REVISION_MASK),
                                    (kal_uint8)(CON4_REVISION_SHIFT)
                                    );
    return val;
}

//CON5----------------------------------------------------
void bq24160_set_ichrg(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_ICHRG_MASK),
                                    (kal_uint8)(CON5_ICHRG_SHIFT)
                                    );
}

void bq24160_set_iterm(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON5), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON5_ITERM_MASK),
                                    (kal_uint8)(CON5_ITERM_SHIFT)
                                    );
}

//CON6----------------------------------------------------
kal_uint32 bq24160_get_minsys_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24160_read_interface(     (kal_uint8)(bq24160_CON6), 
                                    (&val),
                                    (kal_uint8)(CON6_MINSYS_STATUS_MASK),
                                    (kal_uint8)(CON6_MINSYS_STATUS_SHIFT)
                                    );
    return val;
}

kal_uint32 bq24160_get_dpm_status(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24160_read_interface(     (kal_uint8)(bq24160_CON6), 
                                    (&val),
                                    (kal_uint8)(CON6_DPM_STATUS_MASK),
                                    (kal_uint8)(CON6_DPM_STATUS_SHIFT)
                                    );
    return val;
}

void bq24160_set_vindpm_usb(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON6), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON6_VINDPM_USB_MASK),
                                    (kal_uint8)(CON6_VINDPM_USB_SHIFT)
                                    );
}

void bq24160_set_vindpm_in(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON6), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON6_VINDPM_IN_MASK),
                                    (kal_uint8)(CON6_VINDPM_IN_SHIFT)
                                    );
}

//CON7----------------------------------------------------
void bq24160_set_2xtmr_en(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON7), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON7_2XTMR_EN_MASK),
                                    (kal_uint8)(CON7_2XTMR_EN_SHIFT)
                                    );
}

void bq24160_set_tmr(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON7), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON7_TMR_MASK),
                                    (kal_uint8)(CON7_TMR_SHIFT)
                                    );
}

void bq24160_set_ts_en(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON7), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON7_TS_EN_MASK),
                                    (kal_uint8)(CON7_TS_EN_SHIFT)
                                    );
}

kal_uint32 bq24160_get_ts_fault(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24160_read_interface(     (kal_uint8)(bq24160_CON7), 
                                    (&val),
                                    (kal_uint8)(CON7_TS_FAULT_MASK),
                                    (kal_uint8)(CON7_TS_FAULT_SHIFT)
                                    );
    return val;
}

void bq24160_set_low_chg(kal_uint32 val)
{
    kal_uint32 ret=0;    

    ret=bq24160_config_interface(   (kal_uint8)(bq24160_CON7), 
                                    (kal_uint8)(val),
                                    (kal_uint8)(CON7_LOW_CHG_MASK),
                                    (kal_uint8)(CON7_LOW_CHG_SHIFT)
                                    );
}

/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/
void bq24160_hw_component_detect(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=bq24160_read_interface(0x04, &val, 0xFF, 0x0);
    
    if(val == 0)
        g_bq24160_hw_exist=0;
    else
        g_bq24160_hw_exist=1;

    printk("[bq24160_hw_component_detect] exist=%d, Reg[0x04]=0x%x\n", 
        g_bq24160_hw_exist, val);
}

int is_bq24160_exist(void)
{
    printk("[is_bq24160_exist] g_bq24160_hw_exist=%d\n", g_bq24160_hw_exist);
    
    return g_bq24160_hw_exist;
}

void bq24160_dump_register(void)
{
    kal_uint8 i=0;
    printk("[bq24160] ");
    for (i=0;i<bq24160_REG_NUM;i++)
    {
        bq24160_read_byte(i, &bq24160_reg[i]);
        printk("[0x%x]=0x%x ", i, bq24160_reg[i]);        
    }
    printk("\n");
}

void bq24160_hw_init(void)
{   
    #if 0
    battery_xlog_printk(BAT_LOG_CRTI,"[bq24160_hw_init] From Albert\n");
    bq24160_config_interface(bq24160_CON2, 0x2C, 0xFF, 0); // set USB input current limit at 500mA
    bq24160_config_interface(bq24160_CON3, 0x8C, 0xFF, 0); // set AC input current limit at 1.5A and set battery regulation voltage at 4.2V
    bq24160_config_interface(bq24160_CON5, 0x32, 0xFF, 0); // set charge current at 1A (default) and set termination current at 150mA (default)
    bq24160_config_interface(bq24160_CON6, 0x04, 0xFF, 0); // set Vin_DPM at 4.52V
    #endif
    
    #if 0
    battery_xlog_printk(BAT_LOG_CRTI,"[bq24160_hw_init] From Tim\n");
    bq24160_config_interface(bq24160_CON0, 0x1, 0x1, 7); // wdt reset
    bq24160_config_interface(bq24160_CON7, 0x1, 0x3, 5); // Safty timer
    bq24160_config_interface(bq24160_CON2, 0x2, 0x7, 4); // USB current limit at 500mA
    bq24160_config_interface(bq24160_CON3, 0x1, 0x1, 1); // IN current limit
    bq24160_config_interface(bq24160_CON5, 0x13,0x1F,3); // ICHG to BAT
    bq24160_config_interface(bq24160_CON5, 0x3, 0x7, 0); // ITERM to BAT
    bq24160_config_interface(bq24160_CON3, 0x23,0x3F,2); // CV=4.2V
    bq24160_config_interface(bq24160_CON6, 0x3, 0x7, 3); // VINDPM_USB
    bq24160_config_interface(bq24160_CON6, 0x3, 0x7, 0); // VINDPM_IN
    bq24160_config_interface(bq24160_CON7, 0x0, 0x1, 3); // Thermal sense    
    #endif

    battery_xlog_printk(BAT_LOG_CRTI,"[bq24160_hw_init] After HW init\n");    
    bq24160_dump_register();
}

static int bq24160_driver_probe(struct i2c_client *client, const struct i2c_device_id *id) 
{             
    int err=0; 

    battery_xlog_printk(BAT_LOG_CRTI,"[bq24160_driver_probe] \n");

    if (!(new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) {
        err = -ENOMEM;
        goto exit;
    }    
    memset(new_client, 0, sizeof(struct i2c_client));

    new_client = client;    

    //---------------------
    bq24160_hw_component_detect();
    bq24160_dump_register();
    //bq24160_hw_init(); //move to charging_hw_xxx.c   
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
kal_uint8 g_reg_value_bq24160=0;
static ssize_t show_bq24160_access(struct device *dev,struct device_attribute *attr, char *buf)
{
    battery_xlog_printk(BAT_LOG_CRTI,"[show_bq24160_access] 0x%x\n", g_reg_value_bq24160);
    return sprintf(buf, "%u\n", g_reg_value_bq24160);
}
static ssize_t store_bq24160_access(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    int ret=0;
    char *pvalue = NULL;
    unsigned int reg_value = 0;
    unsigned int reg_address = 0;
    
    battery_xlog_printk(BAT_LOG_CRTI,"[store_bq24160_access] \n");
    
    if(buf != NULL && size != 0)
    {
        battery_xlog_printk(BAT_LOG_CRTI,"[store_bq24160_access] buf is %s and size is %d \n",buf,size);
        reg_address = simple_strtoul(buf,&pvalue,16);
        
        if(size > 3)
        {        
            reg_value = simple_strtoul((pvalue+1),NULL,16);        
            battery_xlog_printk(BAT_LOG_CRTI,"[store_bq24160_access] write bq24160 reg 0x%x with value 0x%x !\n",reg_address,reg_value);
            ret=bq24160_config_interface(reg_address, reg_value, 0xFF, 0x0);
        }
        else
        {    
            ret=bq24160_read_interface(reg_address, &g_reg_value_bq24160, 0xFF, 0x0);
            battery_xlog_printk(BAT_LOG_CRTI,"[store_bq24160_access] read bq24160 reg 0x%x with value 0x%x !\n",reg_address,g_reg_value_bq24160);
            battery_xlog_printk(BAT_LOG_CRTI,"[store_bq24160_access] Please use \"cat bq24160_access\" to get value\r\n");
        }        
    }    
    return size;
}
static DEVICE_ATTR(bq24160_access, 0664, show_bq24160_access, store_bq24160_access); //664

static int bq24160_user_space_probe(struct platform_device *dev)    
{    
    int ret_device_file = 0;

    battery_xlog_printk(BAT_LOG_CRTI,"******** bq24160_user_space_probe!! ********\n" );
    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_bq24160_access);
    
    return 0;
}

struct platform_device bq24160_user_space_device = {
    .name   = "bq24160-user",
    .id     = -1,
};

static struct platform_driver bq24160_user_space_driver = {
    .probe      = bq24160_user_space_probe,
    .driver     = {
        .name = "bq24160-user",
    },
};


static struct i2c_board_info __initdata i2c_bq24160 = { I2C_BOARD_INFO("bq24160", (bq24160_SLAVE_ADDR_WRITE>>1))};

static int __init bq24160_init(void)
{    
    int ret=0;
    
    battery_xlog_printk(BAT_LOG_CRTI,"[bq24160_init] init start. ch=%d\n", bq24160_BUSNUM);
    
    i2c_register_board_info(bq24160_BUSNUM, &i2c_bq24160, 1);

    if(i2c_add_driver(&bq24160_driver)!=0)
    {
        battery_xlog_printk(BAT_LOG_CRTI,"[bq24160_init] failed to register bq24160 i2c driver.\n");
    }
    else
    {
        battery_xlog_printk(BAT_LOG_CRTI,"[bq24160_init] Success to register bq24160 i2c driver.\n");
    }

    // bq24160 user space access interface
    ret = platform_device_register(&bq24160_user_space_device);
    if (ret) {
        battery_xlog_printk(BAT_LOG_CRTI,"****[bq24160_init] Unable to device register(%d)\n", ret);
        return ret;
    }    
    ret = platform_driver_register(&bq24160_user_space_driver);
    if (ret) {
        battery_xlog_printk(BAT_LOG_CRTI,"****[bq24160_init] Unable to register driver (%d)\n", ret);
        return ret;
    }
    
    return 0;        
}

static void __exit bq24160_exit(void)
{
    i2c_del_driver(&bq24160_driver);
}

module_init(bq24160_init);
module_exit(bq24160_exit);
   
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq24160 Driver");
MODULE_AUTHOR("James Lo<james.lo@mediatek.com>");
