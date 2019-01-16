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

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include "ncp1851.h"

/**********************************************************
  *
  *   [I2C Slave Setting]
  *
  *********************************************************/
#define NCP1851_SLAVE_ADDR_WRITE   0x6C
#define NCP1851_SLAVE_ADDR_READ	   0x6D

static struct i2c_client *new_client = NULL;
static const struct i2c_device_id ncp1851_i2c_id[] = {{"ncp1851",0},{}};   

static int ncp1851_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);

static struct i2c_driver ncp1851_driver = {
    .driver = {
        .name    = "ncp1851",
    },
    .probe       = ncp1851_driver_probe,
    .id_table    = ncp1851_i2c_id,
};

/**********************************************************
  *
  *   [Global Variable]
  *
  *********************************************************/
#define ncp1851_REG_NUM 19
kal_uint8 ncp1851_reg[ncp1851_REG_NUM] = {0};

static DEFINE_MUTEX(ncp1851_i2c_access);
/**********************************************************
  *
  *   [I2C Function For Read/Write ncp1851]
  *
  *********************************************************/
int ncp1851_read_byte(kal_uint8 cmd, kal_uint8 *returnData)
{
    char     cmd_buf[1]={0x00};
    char     readData = 0;
    int      ret=0;

    mutex_lock(&ncp1851_i2c_access);
    
    //new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG;    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;

    cmd_buf[0] = cmd;
    ret = i2c_master_send(new_client, &cmd_buf[0], (1<<8 | 1));
    if (ret < 0) 
    {    
        //new_client->addr = new_client->addr & I2C_MASK_FLAG;
        new_client->ext_flag=0;
		
        mutex_unlock(&ncp1851_i2c_access);
        return 0;
    }
    
    readData = cmd_buf[0];
    *returnData = readData;

    //new_client->addr = new_client->addr & I2C_MASK_FLAG;
    new_client->ext_flag=0;
	
    mutex_unlock(&ncp1851_i2c_access);    
    return 1;
}

int ncp1851_write_byte(kal_uint8 cmd, kal_uint8 writeData)
{
    char    write_data[2] = {0};
    int    ret=0;

    mutex_lock(&ncp1851_i2c_access);

    write_data[0] = cmd;
    write_data[1] = writeData;

    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;
	
    ret = i2c_master_send(new_client, write_data, 2);
    if (ret < 0)
	{
        new_client->ext_flag=0;    
        mutex_unlock(&ncp1851_i2c_access);
        return 0;
    }

    new_client->ext_flag=0;    
    mutex_unlock(&ncp1851_i2c_access);
    return 1;
}

/**********************************************************
  *
  *   [Read / Write Function]
  *
  *********************************************************/
kal_uint32 ncp1851_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT)
{   
    kal_uint8 ncp1851_reg = 0;
    int ret = 0;

    printk("--------------------------------------------------\n");

    ret = ncp1851_read_byte(RegNum, &ncp1851_reg);
    printk("[ncp1851_read_interface] Reg[%x]=0x%x\n", RegNum, ncp1851_reg);

    ncp1851_reg &= (MASK << SHIFT);
    *val = (ncp1851_reg >> SHIFT);    
    printk("[ncp1851_read_interface] Val=0x%x\n", *val);

    return ret;
}

kal_uint32 ncp1851_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 ncp1851_reg = 0;
    int ret = 0;

    printk("--------------------------------------------------\n");

    ret = ncp1851_read_byte(RegNum, &ncp1851_reg);
    //printk("[ncp1851_config_interface] Reg[%x]=0x%x\n", RegNum, ncp1851_reg);

    ncp1851_reg &= ~(MASK << SHIFT);
    ncp1851_reg |= (val << SHIFT);

    ret = ncp1851_write_byte(RegNum, ncp1851_reg);
    //printk("[ncp18516_config_interface] Write Reg[%x]=0x%x\n", RegNum, ncp1851_reg);

    // Check
    //ncp1851_read_byte(RegNum, &ncp1851_reg);
    //printk("[ncp1851_config_interface] Check Reg[%x]=0x%x\n", RegNum, ncp1851_reg);

    return ret;
}

/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/
//CON0
kal_uint32 ncp1851_get_chip_status(void)
{
    kal_uint32 ret=0;
    kal_uint32 val=0;

    ret=ncp1851_read_interface((kal_uint8)(NCP1851_CON0),
							    (kal_uint8*)(&val),
							    (kal_uint8)(CON0_STATE_MASK),
							    (kal_uint8)(CON0_STATE_SHIFT)
							    );
    return val;
}

kal_uint32 ncp1851_get_batfet(void)
{
    kal_uint32 ret=0;
    kal_uint32 val=0;

    ret=ncp1851_read_interface((kal_uint8)(NCP1851_CON0),
	        					      (kal_uint8*)(&val),
							      (kal_uint8)(CON0_BATFET_MASK),
							      (kal_uint8)(CON0_BATFET_SHIFT)
							      );
    return val;
}

kal_uint32 ncp1851_get_ntc(void)
{
    kal_uint32 ret=0;
    kal_uint32 val=0;

    ret=ncp1851_read_interface((kal_uint8)(NCP1851_CON0),
	        					      (kal_uint8*)(&val),
							      (kal_uint8)(CON0_NTC_MASK),
							      (kal_uint8)(CON0_NTC_SHIFT)
							      );
    return val;
}

kal_uint32 ncp1851_get_statint(void)
{
    kal_uint32 ret=0;
    kal_uint32 val=0;

    ret=ncp1851_read_interface((kal_uint8)(NCP1851_CON0),
	        					      (kal_uint8*)(&val),
							      (kal_uint8)(CON0_STATINT_MASK),
							      (kal_uint8)(CON0_STATINT_SHIFT)
							      );
    return val;
}

kal_uint32 ncp1851_get_faultint(void)
{
    kal_uint32 ret=0;
    kal_uint32 val=0;

    ret=ncp1851_read_interface((kal_uint8)(NCP1851_CON0),
	        					      (kal_uint8*)(&val),
							      (kal_uint8)(CON0_FAULTINT_MASK),
							      (kal_uint8)(CON0_FAULTINT_SHIFT)
							      );
    return val;
}

//CON1
void ncp1851_set_reset(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON1),
								(kal_uint8)(val),
								(kal_uint8)(CON1_REG_RST_MASK),
								(kal_uint8)(CON1_REG_RST_SHIFT)
								);
}

void ncp1851_set_chg_en(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON1),
    								(kal_uint8)(val),
    								(kal_uint8)(CON1_CHG_EN_MASK),
    								(kal_uint8)(CON1_CHG_EN_SHIFT)
    								);
}

void ncp1851_set_otg_en(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON1),
                                                   (kal_uint8)(val),
                                                   (kal_uint8)(CON1_OTG_EN_MASK),
                                                   (kal_uint8)(CON1_OTG_EN_SHIFT)
                                                   );
}

kal_uint32 ncp1851_get_otg_en(void)
{
    kal_uint32 ret=0;
    kal_uint32 val=0;

    ret=ncp1851_read_interface((kal_uint8)(NCP1851_CON1),
	        					      (kal_uint8*)(&val),
							      (kal_uint8)(CON1_OTG_EN_MASK),
							      (kal_uint8)(CON1_OTG_EN_SHIFT)
							      );
    return val;
}

void ncp1851_set_ntc_en(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON1),
    								(kal_uint8)(val),
    								(kal_uint8)(CON1_NTC_EN_MASK),
    								(kal_uint8)(CON1_NTC_EN_SHIFT)
    								);
}

void ncp1851_set_tj_warn_opt(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON1),
                                                   (kal_uint8)(val),
                                                   (kal_uint8)(CON1_TJ_WARN_OPT_MASK),
                                                   (kal_uint8)(CON1_TJ_WARN_OPT_SHIFT)
                                                   );
}

void ncp1851_set_jeita_opt(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON1),
                                                   (kal_uint8)(val),
                                                   (kal_uint8)(CON1_JEITA_OPT_MASK),
                                                   (kal_uint8)(CON1_JEITA_OPT_SHIFT)
                                                   );
}

void ncp1851_set_tchg_rst(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface(	(kal_uint8)(NCP1851_CON1),
								(kal_uint8)(val),
								(kal_uint8)(CON1_TCHG_RST_MASK),
								(kal_uint8)(CON1_TCHG_RST_SHIFT)
								);
}

void ncp1851_set_int_mask(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON1),
                                                   (kal_uint8)(val),
                                                   (kal_uint8)(CON1_INT_MASK_MASK),
                                                   (kal_uint8)(CON1_INT_MASK_SHIFT)
                                                   );
}

//CON2
void ncp1851_set_wdto_dis(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON2),
								(kal_uint8)(val),
								(kal_uint8)(CON2_WDTO_DIS_MASK),
								(kal_uint8)(CON2_WDTO_DIS_SHIFT)
								);
}

void ncp1851_set_chgto_dis(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON2),
								(kal_uint8)(val),
								(kal_uint8)(CON2_CHGTO_DIS_MASK),
								(kal_uint8)(CON2_CHGTO_DIS_SHIFT)
								);
}

void ncp1851_set_pwr_path(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON2),
								(kal_uint8)(val),
								(kal_uint8)(CON2_PWR_PATH_MASK),
								(kal_uint8)(CON2_PWR_PATH_SHIFT)
								);
}

void ncp1851_set_trans_en(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON2),
								(kal_uint8)(val),
								(kal_uint8)(CON2_TRANS_EN_MASK),
								(kal_uint8)(CON2_TRANS_EN_SHIFT)
								);
}

void ncp1851_set_factory_mode(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON2),
								(kal_uint8)(val),
								(kal_uint8)(CON2_FCTRY_MOD_MASK),
								(kal_uint8)(CON2_FCTRY_MOD_SHIFT)
								);
}

void ncp1851_set_iinset_pin_en(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON2),
								(kal_uint8)(val),
								(kal_uint8)(CON2_IINSET_PIN_EN_MASK),
								(kal_uint8)(CON2_IINSET_PIN_EN_SHIFT)
								);
}

void ncp1851_set_iinlim_en(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON2),
								(kal_uint8)(val),
								(kal_uint8)(CON2_IINLIM_EN_MASK),
								(kal_uint8)(CON2_IINLIM_EN_SHIFT)
								);
}

void ncp1851_set_aicl_en(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON2),
								(kal_uint8)(val),
								(kal_uint8)(CON2_AICL_EN_MASK),
								(kal_uint8)(CON2_AICL_EN_SHIFT)
								);
}

//CON8
kal_uint32 ncp1851_get_vfet_ok(void)
{
    kal_uint32 ret=0;
    kal_uint32 val=0;

    ret=ncp1851_read_interface((kal_uint8)(NCP1851_CON8),
	        					      (kal_uint8*)(&val),
							      (kal_uint8)(CON8_VFET_OK_MASK),
							      (kal_uint8)(CON8_VFET_OK_SHIFT)
							      );
    return val;
}


//CON14
void ncp1851_set_ctrl_vbat(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON14),
								(kal_uint8)(val),
								(kal_uint8)(CON14_CTRL_VBAT_MASK),
								(kal_uint8)(CON14_CTRL_VBAT_SHIFT)
								);
}

//CON15
void ncp1851_set_ieoc(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON15),
								(kal_uint8)(val),
								(kal_uint8)(CON15_IEOC_MASK),
								(kal_uint8)(CON15_IEOC_SHIFT)
								);
}

void ncp1851_set_ichg(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON15),
								(kal_uint8)(val),
								(kal_uint8)(CON15_ICHG_MASK),
								(kal_uint8)(CON15_ICHG_SHIFT)
								);
}

//CON16
void ncp1851_set_iweak(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON16),
								(kal_uint8)(val),
								(kal_uint8)(CON16_IWEAK_MASK),
								(kal_uint8)(CON16_IWEAK_SHIFT)
								);
}

void ncp1851_set_ctrl_vfet(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON16),
								(kal_uint8)(val),
								(kal_uint8)(CON16_CTRL_VFET_MASK),
								(kal_uint8)(CON16_CTRL_VFET_SHIFT)
								);
}

void ncp1851_set_iinlim(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON16),
								(kal_uint8)(val),
								(kal_uint8)(CON16_IINLIM_MASK),
								(kal_uint8)(CON16_IINLIM_SHIFT)
								);
}

//CON17
void ncp1851_set_vchred(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON17),
								(kal_uint8)(val),
								(kal_uint8)(CON17_VCHRED_MASK),
								(kal_uint8)(CON17_VCHRED_SHIFT)
								);
}

void ncp1851_set_ichred(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON17),
								(kal_uint8)(val),
								(kal_uint8)(CON17_ICHRED_MASK),
								(kal_uint8)(CON17_ICHRED_SHIFT)
								);
}

//CON18
void ncp1851_set_batcold(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON18),
								(kal_uint8)(val),
								(kal_uint8)(CON18_BATCOLD_MASK),
								(kal_uint8)(CON18_BATCOLD_SHIFT)
								);
}

void ncp1851_set_bathot(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON18),
								(kal_uint8)(val),
								(kal_uint8)(CON18_BATHOT_MASK),
								(kal_uint8)(CON18_BATHOT_SHIFT)
								);
}

void ncp1851_set_batchilly(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON18),
								(kal_uint8)(val),
								(kal_uint8)(CON18_BATCHIL_MASK),
								(kal_uint8)(CON18_BATCHIL_SHIFT)
								);
}

void ncp1851_set_batwarm(kal_uint32 val)
{
    kal_uint32 ret=0;

    ret=ncp1851_config_interface((kal_uint8)(NCP1851_CON18),
								(kal_uint8)(val),
								(kal_uint8)(CON18_BATWARM_MASK),
								(kal_uint8)(CON18_BATWARM_SHIFT)
								);
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
void ncp1851_dump_register(void)
{
    int i=0;
    for (i=0;i<ncp1851_REG_NUM;i++)
    {
        if((i == 3) || (i == 4) || (i == 5) || (i == 6)) //do not dump read clear status register
            continue;
        if((i == 10) || (i == 11) || (i == 12) || (i == 13)) //do not dump interrupt mask bit register
            continue;		
        ncp1851_read_byte(i, &ncp1851_reg[i]);
        printk("[ncp1851_dump_register] Reg[0x%X]=0x%X\n", i, ncp1851_reg[i]);
    }
}

void ncp1851_read_register(int i)
{
    ncp1851_read_byte(i, &ncp1851_reg[i]);
    printk("[ncp1851_read_register] Reg[0x%X]=0x%X\n", i, ncp1851_reg[i]);
}

extern int g_pmic_init_for_ncp1851;

static int ncp1851_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int err=0;

    printk("[ncp1851_driver_probe] \n");

    if (!(new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) {
         err = -ENOMEM;
        goto exit;
    }
    memset(new_client, 0, sizeof(struct i2c_client));

     new_client = client;

    //---------------------

    g_pmic_init_for_ncp1851 = 1;
    
    return 0;

exit:
    return err;

}

/**********************************************************
  *
  *   [platform_driver API]
  *
  *********************************************************/
kal_uint8 g_reg_value_ncp1851=0;
static ssize_t show_ncp1851_access(struct device *dev,struct device_attribute *attr, char *buf)
{
    printk("[show_ncp1851_access] 0x%x\n", g_reg_value_ncp1851);
    return sprintf(buf, "%u\n", g_reg_value_ncp1851);
}
static ssize_t store_ncp1851_access(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    int ret=0;
    char *pvalue = NULL;
    unsigned int reg_value = 0;
    unsigned int reg_address = 0;

    printk("[store_ncp1851_access] \n");

    if(buf != NULL && size != 0)
    {
        printk("[store_ncp1851_access] buf is %s and size is %d \n",buf,size);
        reg_address = simple_strtoul(buf,&pvalue,16);

        if(size > 3)
        {
            reg_value = simple_strtoul((pvalue+1),NULL,16);
            printk("[store_ncp1851_access] write ncp1851 reg 0x%x with value 0x%x !\n",reg_address,reg_value);
            ret=ncp1851_config_interface(reg_address, reg_value, 0xFF, 0x0);
        }
        else
        {
            ret=ncp1851_read_interface(reg_address, &g_reg_value_ncp1851, 0xFF, 0x0);
            printk("[store_ncp1851_access] read ncp1851 reg 0x%x with value 0x%x !\n",reg_address,g_reg_value_ncp1851);
            printk("[store_ncp1851_access] Please use \"cat ncp1851_access\" to get value\r\n");
        }
    }
    return size;
}
static DEVICE_ATTR(ncp1851_access, 0664, show_ncp1851_access, store_ncp1851_access); //664

static int ncp1851_user_space_probe(struct platform_device *dev)
{
    int ret_device_file = 0;

    printk("******** ncp1851_user_space_probe!! ********\n" );

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ncp1851_access);

    return 0;
}

struct platform_device ncp1851_user_space_device = {
    .name   = "ncp1851-user",
    .id	    = -1,
};

static struct platform_driver ncp1851_user_space_driver = {
    .probe		= ncp1851_user_space_probe,
    .driver     = {
        .name = "ncp1851-user",
    },
};

#define NCP1851_BUSNUM 6
static struct i2c_board_info __initdata i2c_ncp1851 = { I2C_BOARD_INFO("ncp1851", (0x6c>>1))};

static int __init ncp1851_init(void)
{
    int ret=0;

    printk("[ncp1851_init] init start\n");

    i2c_register_board_info(NCP1851_BUSNUM, &i2c_ncp1851, 1);

    if(i2c_add_driver(&ncp1851_driver)!=0)
    {
        printk("[ncp1851_init] failed to register ncp1851 i2c driver.\n");
    }
    else
    {
        printk("[ncp1851_init] Success to register ncp1851 i2c driver.\n");
    }

    // ncp1851 user space access interface
    ret = platform_device_register(&ncp1851_user_space_device);
    if (ret) {
        printk("****[ncp1851_init] Unable to device register(%d)\n", ret);
        return ret;
    }
    ret = platform_driver_register(&ncp1851_user_space_driver);
    if (ret) {
        printk("****[ncp1851_init] Unable to register driver (%d)\n", ret);
        return ret;
    }

    return 0;
}

static void __exit ncp1851_exit(void)
{
    i2c_del_driver(&ncp1851_driver);
}

module_init(ncp1851_init);
module_exit(ncp1851_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C ncp1851 Driver");
MODULE_AUTHOR("YT Lee<yt.lee@mediatek.com>");

