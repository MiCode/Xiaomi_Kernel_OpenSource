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

#include "da9210.h"
#include <cust_pmic.h>

#include <mach/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>

#include <mach/mt_boot.h> 
#include <mach/mt_chip.h>

#if defined(CONFIG_MTK_FPGA)
#else
#include <cust_i2c.h>
#endif

extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);

/**********************************************************
  *
  *   [I2C Slave Setting] 
  *
  *********************************************************/
#define da9210_SLAVE_ADDR_WRITE   0xD0
#define da9210_SLAVE_ADDR_Read    0xD1

#ifdef I2C_EXT_BUCK_CHANNEL
#define da9210_BUSNUM I2C_EXT_BUCK_CHANNEL
#else
#define da9210_BUSNUM 0//1
#endif

static struct i2c_client *new_client = NULL;
static const struct i2c_device_id da9210_i2c_id[] = {{"da9210",0},{}};   
static int da9210_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);

static struct i2c_driver da9210_driver = {
    .driver = {
        .name    = "da9210",
    },
    .probe       = da9210_driver_probe,
    .id_table    = da9210_i2c_id,
};

/**********************************************************
  *
  *   [Global Variable] 
  *
  *********************************************************/
static DEFINE_MUTEX(da9210_i2c_access);

int g_da9210_driver_ready=0;
int g_da9210_hw_exist=0;
/**********************************************************
  *
  *   [I2C Function For Read/Write da9210] 
  *
  *********************************************************/
kal_uint32 da9210_read_byte(kal_uint8 cmd, kal_uint8 *returnData)
{
    char     cmd_buf[1]={0x00};
    char     readData = 0;
    int      ret=0;

    mutex_lock(&da9210_i2c_access);
    
    //new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG;    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;

    cmd_buf[0] = cmd;
    ret = i2c_master_send(new_client, &cmd_buf[0], (1<<8 | 1));
    if (ret < 0) 
    {   
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_read_byte] ret=%d\n", ret);
        
        //new_client->addr = new_client->addr & I2C_MASK_FLAG;
        new_client->ext_flag=0;
        mutex_unlock(&da9210_i2c_access);
        return 0;
    }
    
    readData = cmd_buf[0];
    *returnData = readData;

    // new_client->addr = new_client->addr & I2C_MASK_FLAG;
    new_client->ext_flag=0;
    
    mutex_unlock(&da9210_i2c_access);    
    return 1;
}

kal_uint32 da9210_write_byte(kal_uint8 cmd, kal_uint8 writeData)
{
    char    write_data[2] = {0};
    int     ret=0;
    
    mutex_lock(&da9210_i2c_access);
    
    write_data[0] = cmd;
    write_data[1] = writeData;
    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;
    
    ret = i2c_master_send(new_client, write_data, 2);
    if (ret < 0) 
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_write_byte] ret=%d\n", ret);
        
        new_client->ext_flag=0;
        mutex_unlock(&da9210_i2c_access);
        return 0;
    }
    
    new_client->ext_flag=0;
    mutex_unlock(&da9210_i2c_access);
    return 1;
}

/**********************************************************
  *
  *   [Read / Write Function] 
  *
  *********************************************************/
kal_uint32 da9210_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 da9210_reg = 0;
    kal_uint32 ret = 0;

    //xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","--------------------------------------------------\n");

    ret = da9210_read_byte(RegNum, &da9210_reg);

    //xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_read_interface] Reg[%x]=0x%x\n", RegNum, da9210_reg);
    
    da9210_reg &= (MASK << SHIFT);
    *val = (da9210_reg >> SHIFT);
    
    //xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_read_interface] val=0x%x\n", *val);
    
    return ret;
}

kal_uint32 da9210_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 da9210_reg = 0;
    kal_uint32 ret = 0;

    //xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","--------------------------------------------------\n");

    ret = da9210_read_byte(RegNum, &da9210_reg);
    //xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_config_interface] Reg[%x]=0x%x\n", RegNum, da9210_reg);
    
    da9210_reg &= ~(MASK << SHIFT);
    da9210_reg |= (val << SHIFT);

    ret = da9210_write_byte(RegNum, da9210_reg);
    //xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_config_interface] write Reg[%x]=0x%x\n", RegNum, da9210_reg);

    // Check
    //da9210_read_byte(RegNum, &da9210_reg);
    //xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_config_interface] Check Reg[%x]=0x%x\n", RegNum, da9210_reg);

    return ret;
}

kal_uint32 da9210_get_reg_value(kal_uint32 reg)
{
    kal_uint32 ret=0;
    kal_uint8 reg_val=0;

    ret=da9210_read_interface( (kal_uint8) reg, &reg_val, 0xFF, 0x0);

    return reg_val;
}

/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/
void da9210_dump_register(void)
{
    kal_uint8 i=0;
    //----------------------------------------------------------------
    printk("[da9210] page 0,1: ");   
    printk("[0x%x]=0x%x ", 0x0, da9210_get_reg_value(0x0));
    for (i=0x50;i<=0x5D;i++) {     
        printk("[0x%x]=0x%x ", i, da9210_get_reg_value(i));
    }    
    for (i=0xD0;i<=0xD9;i++) {
        printk("[0x%x]=0x%x ", i, da9210_get_reg_value(i));
    }
    printk("\n");
    //----------------------------------------------------------------
    printk("[da9210] page 2,3: ");    
    for (i=0x05;i<=0x06;i++)
    {
        da9210_config_interface(0x0, 0x2, 0xF, 0); // select to page 2,3
        printk("[0x%x]=0x%x ", i, da9210_get_reg_value(i));
    }
    for (i=0x43;i<=0x4F;i++)
    {
        da9210_config_interface(0x0, 0x2, 0xF, 0); // select to page 2,3
        printk("[0x%x]=0x%x ", i, da9210_get_reg_value(i));
    }
    printk("\n");
    //----------------------------------------------------------------
    da9210_config_interface(0x0, 0x0, 0xF, 0); // select to page 0,1
    //----------------------------------------------------------------
}

int get_da9210_i2c_ch_num(void)
{
    return da9210_BUSNUM;
}

int set_da9210_buck_en(int en_bit)
{
    int ret=0;

    if(g_da9210_driver_ready==1)
    {
        if(g_da9210_hw_exist==1)
        {            
            if(en_bit==0)
                ret = da9210_config_interface(0x5D,0x0,0x1,0);
            else
                ret = da9210_config_interface(0x5D,0x1,0x1,0);            

            printk("[set_da9210_buck_en] en_bit=%d\n", en_bit);
        }
        else
        {
            printk("[set_da9210_buck_en] da9210 driver is not exist\n");
        }
    }
    else
    {
        printk("[set_da9210_buck_en] da9210 driver is not ready\n");
    }
    
    return ret; //1:PASS, 0:FAIL
}

extern void ext_buck_vproc_vsel(int val);
extern unsigned int g_vproc_vsel_gpio_number;
extern unsigned int g_vproc_en_gpio_number;

void da9210_hw_init(void)
{    
    kal_uint32 ret=0;

    ret = da9210_config_interface(0x0, 0x1, 0x1, 7); // page reverts to 0 after one access
    ret = da9210_config_interface(0x5D,0x1, 0x1, 0); // BUCK_EN=1
    ret = da9210_config_interface(0x58,0x44,0xFF, 0);// 20140221,Trevor
    ret = da9210_config_interface(0x59,0x0, 0xF, 4); // GPIO3=GPI activr low
    ret = da9210_config_interface(0x59,0x4, 0xF, 0); // 20140221,Trevor
    ret = da9210_config_interface(0x5A,0x54,0xFF, 0);// 20140221,Trevor
    ret = da9210_config_interface(0x5D,0x0, 0x1, 4); // select BUCK_A

    //-----------------------------------------------
    ret = da9210_config_interface(0x5D,0x2,0x3,5);   //Setting VOSEL controlled by GPIO3
    ret = da9210_config_interface(0x5D,0x0,0x3,1);   //VOUT_EN not controlled by GPIO
    ret = da9210_config_interface(0xD2,0x1,0x1,4);   //Disable force PWM mode (this is reserve register)
    ret = da9210_config_interface(0xD6,0x64,0x7F,0); //Setting Vmax=1.3V   
    ret = da9210_config_interface(0xD8,0x50,0x7F,0); // VSEL=high, 1.1V, Setting VBUCK_A=1.1V
    if(g_vproc_vsel_gpio_number!=0)
    {
        ext_buck_vproc_vsel(1); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[da9210_hw_init] ext_buck_vproc_vsel(1)\n");
    }
    ret = da9210_config_interface(0xD9,0xA8,0xFF,0); // VSEL=low, 0.7V, Setting VBUCK_B=0.7V, 20140221, KL
    //ret = da9210_config_interface(0xD9,0x46,0xFF,0); // VSEL=low, 1.0V, Setting VBUCK_B=1.0V, 20140311, workaround

    ret = da9210_config_interface(0xD1,0x3,0x3,0); // 20140511, KL
    //-----------------------------------------------
    
    printk("[da9210_hw_init] [0x0]=0x%x, [0x58]=0x%x, [0x59]=0x%x, [0x5A]=0x%x, [0x5D]=0x%x, [0xD1]=0x%x, [0xD2]=0x%x, [0xD6]=0x%x, [0xD8]=0x%x, [0xD9]=0x%x\n", 
        da9210_get_reg_value(0x0), 
        da9210_get_reg_value(0x58), da9210_get_reg_value(0x59), 
        da9210_get_reg_value(0x5A), da9210_get_reg_value(0x5D), 
        da9210_get_reg_value(0xD1), da9210_get_reg_value(0xD2), 
        da9210_get_reg_value(0xD6), 
        da9210_get_reg_value(0xD8), da9210_get_reg_value(0xD9)
        );
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_hw_init] Done\n");       
}

void da9210_hw_init_v2(void)
{    
    kal_uint32 ret=0;

    ret = da9210_config_interface(0x0, 0x1, 0x1, 7); // page reverts to 0 after one access
    ret = da9210_config_interface(0x5D,0x1, 0x1, 0); // BUCK_EN=1
    ret = da9210_config_interface(0x5D,0x0, 0x1, 3); // BUCK_PD_DIS = 0
    ret = da9210_config_interface(0x58,0x44,0xFF, 0);// 20140221,Trevor
    ret = da9210_config_interface(0x59,0x0, 0xF, 4); // GPIO3=GPI activr low
    ret = da9210_config_interface(0x59,0x4, 0xF, 0); // 20140221,Trevor
    ret = da9210_config_interface(0x5A,0x54,0xFF, 0);// 20140221,Trevor
    ret = da9210_config_interface(0x5D,0x0, 0x1, 4); // select BUCK_A

    //-----------------------------------------------
    ret = da9210_config_interface(0x5D,0x2,0x3,5);   //Setting VOSEL controlled by GPIO3
    ret = da9210_config_interface(0x5D,0x0,0x3,1);   //VOUT_EN not controlled by GPIO
    ret = da9210_config_interface(0xD2,0x1,0x1,4);   //Disable force PWM mode (this is reserve register)
    ret = da9210_config_interface(0xD6,0x64,0x7F,0); //Setting Vmax=1.3V   
    ret = da9210_config_interface(0xD8,0x46,0xFF,0); // VSEL=high, 1.0V 
    if(g_vproc_vsel_gpio_number!=0)
    {
        ext_buck_vproc_vsel(1); 
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[da9210_hw_init_v2] ext_buck_vproc_vsel(1)\n");
    }
    ret = da9210_config_interface(0xD9,0xB2,0xFF,0); // VSEL=low, 0.8V, Setting VBUCK_B=0.8V, 20141028, KL
    
    ret = da9210_config_interface(0xD1,0x0,0x3,0); // 20140511, KL
    //-----------------------------------------------
    
    printk("[da9210_hw_init_v2] [0x0]=0x%x, [0x58]=0x%x, [0x59]=0x%x, [0x5A]=0x%x, [0x5D]=0x%x, [0xD1]=0x%x, [0xD2]=0x%x, [0xD6]=0x%x, [0xD8]=0x%x, [0xD9]=0x%x\n", 
        da9210_get_reg_value(0x0), 
        da9210_get_reg_value(0x58), da9210_get_reg_value(0x59), 
        da9210_get_reg_value(0x5A), da9210_get_reg_value(0x5D), 
        da9210_get_reg_value(0xD1), da9210_get_reg_value(0xD2), 
        da9210_get_reg_value(0xD6), 
        da9210_get_reg_value(0xD8), da9210_get_reg_value(0xD9)
        );
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_hw_init_v2] Done\n");       
}

void da9210_hw_component_detect(void)
{
    kal_uint32 ret=0;
    kal_uint8 val=0;

    ret=da9210_config_interface(0x0, 0x1, 0x1, 7); // page reverts to 0 after one access
    ret=da9210_config_interface(0x0, 0x2, 0xF, 0); // select to page 2,3
    
    ret=da9210_read_interface(0x5,&val,0xF,4);
    
    // check default SPEC. value
    if(val==0xD)
    {
        g_da9210_hw_exist=1;        
    }
    else
    {
        g_da9210_hw_exist=0;
    }
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_hw_component_detect] exist=%d, Reg[0x105][7:4]=0x%x\n",
        g_da9210_hw_exist, val);
}

int is_da9210_sw_ready(void)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC","g_da9210_driver_ready=%d\n", g_da9210_driver_ready);
    
    return g_da9210_driver_ready;
}

int is_da9210_exist(void)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC","g_da9210_hw_exist=%d\n", g_da9210_hw_exist);
    
    return g_da9210_hw_exist;
}

int da9210_vosel(unsigned long val)
{
    int ret=1;
    unsigned long reg_val=0;

    //reg_val = ( (val) - 30000 ) / 1000; //300mV~1570mV, step=10mV
    reg_val = ((((val*10)-300000)/1000)+9)/10;

    if(reg_val > 127)
        reg_val = 127;

    ret=da9210_write_byte(0xD8, reg_val);

    xlog_printk(ANDROID_LOG_DEBUG, "Power/PMIC","[da9210_vosel] val=%d, reg_val=%d, Reg[0xD8]=0x%x\n", 
        val, reg_val, da9210_get_reg_value(0xD8));

    return ret;
}

void bigcore_power_init(void)
{
    kal_uint32 ret=0;
    CHIP_SW_VER ver = mt_get_chip_sw_ver();
    unsigned int code = mt_get_chip_hw_code();

    if (0x6795 == code) 
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[bigcore_power_init] NA\n");
    } 
    else 
    {
        if (ver == CHIP_SW_VER_01)
        //if (1)
        {
            ret = da9210_config_interface(0x5D, 0x1, 0x3, 1);    // VOUT_EN controlled by GPIO0    
            mt6331_upmu_set_rg_vsram_dvfs1_vosel(0);             // 0.7V
            mt6331_upmu_set_vsram_dvfs1_vosel_sleep(0);          // 0.7V 
            mt6331_upmu_set_rg_vsram_dvfs1_on_ctrl(1);
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[bigcore_power_init] Done. ret=%d, ver=%d.\n", ret, ver);
        }
    }
}

void bigcore_power_off(void)
{   
    CHIP_SW_VER ver = mt_get_chip_sw_ver();
    unsigned int code = mt_get_chip_hw_code();
    U32 reg_val=0;

    if (0x6795 == code) 
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[bigcore_power_off] NA\n");
    } 
    else 
    {
        if (ver == CHIP_SW_VER_01)
        //if (1)
        {
            ext_buck_vproc_vsel(0); 
            
            // CA15 LDO Fast Transient Disable: Set MT6331 0x051C[6] = 1¡¦b1, other bit=0
            pmic_config_interface(0x051C,0x0040,0xFFFF,0);
            
            mt6331_upmu_set_vsram_dvfs1_vosel_ctrl(0);    
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[bigcore_power_off] Done. ver=%d\n", ver);
        }
        else
        {
            /* turn off ca15l vproc(ext buck) */
            set_da9210_buck_en(0);
            /* turn off ca15l vsram */
            pmic_read_interface(0x63C,&reg_val,0x3,13);
            if(reg_val==0x0) {
                pmic_config_interface(0x18,0x0,0x1,5);
            } else if(reg_val==0x1) {
                pmic_config_interface(0x18,0x0,0x1,6);
            } else if(reg_val==0x2) {
                pmic_config_interface(0x18,0x0,0x1,2);
            } else if(reg_val==0x3) {
                pmic_config_interface(0x524,0x0,0x1,10);
            } else {
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[bigcore_power_off] wrong reg_val=%d\n", reg_val);
            }        
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[bigcore_power_off] Turn off bigcore vproc and vsram. ver=%d\n", ver);
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[bigcore_power_off] [0x%x]=0x%x, [0x%x]=0x%x, [0x%x]=0x%x, reg_val=%d\n",
                0x63C, upmu_get_reg_value(0x63C),
                0x18, upmu_get_reg_value(0x18),
                0x524, upmu_get_reg_value(0x524),
                reg_val
                );        
        }
    }
}

void bigcore_power_on(void)
{   
    CHIP_SW_VER ver = mt_get_chip_sw_ver();
    unsigned int code = mt_get_chip_hw_code();
    U32 reg_val=0;

    if (0x6795 == code) 
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[bigcore_power_on] NA\n");
    } 
    else 
    {
        if (ver == CHIP_SW_VER_01)
        //if (1)
        {        
            mt6331_upmu_set_vsram_dvfs1_vosel_ctrl(1);
            
            // CA15 LDO Fast Transient Enable: Set MT6331 0x051C[6] = 1¡¦b0, other bit=0 
            pmic_config_interface(0x051C,0x0000,0xFFFF,0);
                    
            ext_buck_vproc_vsel(1); 
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[bigcore_power_on] Done. ver=%d\n", ver);
        }
        else
        {
            /* turn on ca15l vsram */
            pmic_read_interface(0x63C,&reg_val,0x3,13);
            if(reg_val==0x0) {
                pmic_config_interface(0x18,0x1,0x1,5);
            } else if(reg_val==0x1) {
                pmic_config_interface(0x18,0x1,0x1,6);
            } else if(reg_val==0x2) {
                pmic_config_interface(0x18,0x1,0x1,2);
            } else if(reg_val==0x3) {
                pmic_config_interface(0x524,0x1,0x1,10);       
            } else {
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[bigcore_power_on] wrong reg_val=%d\n", reg_val);
            }         
            /* turn on ca15l vproc(ext buck) */
            set_da9210_buck_en(1);
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[bigcore_power_on] Turn on bigcore vproc and vsram. ver=%d\n", ver);
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[bigcore_power_on] [0x%x]=0x%x, [0x%x]=0x%x, [0x%x]=0x%x, reg_val=%d\n",
                0x63C, upmu_get_reg_value(0x63C),
                0x18, upmu_get_reg_value(0x18),
                0x524, upmu_get_reg_value(0x524),
                reg_val
                ); 
        }
    }
}

static int da9210_driver_probe(struct i2c_client *client, const struct i2c_device_id *id) 
{             
    int err=0;
    unsigned int code = mt_get_chip_hw_code(); 

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_driver_probe] \n");

    if (!(new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) {
        err = -ENOMEM;
        goto exit;
    }    
    memset(new_client, 0, sizeof(struct i2c_client));

    new_client = client;    
    
    //---------------------        
    da9210_hw_component_detect();        
    if(g_da9210_hw_exist==1)
    {
        if (0x6795 == code) {
            // 67XX
            da9210_hw_init_v2();
        } else {
            // 65XX
            da9210_hw_init();
            bigcore_power_init();
        }
        
        da9210_dump_register();
    }
    g_da9210_driver_ready=1;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_driver_probe] g_da9210_hw_exist=%d, g_da9210_driver_ready=%d\n", 
        g_da9210_hw_exist, g_da9210_driver_ready);

    if(g_da9210_hw_exist==0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_driver_probe] return err\n");
        return err;
    }

    return 0;                                                                                       

exit:
    return err;

}

/**********************************************************
  *
  *   [platform_driver API] 
  *
  *********************************************************/
#ifdef da9210_AUTO_DETECT_DISABLE
    //
#else
//==============================================================================
// da9210_access
//==============================================================================
kal_uint8 g_reg_value_da9210=0;
static ssize_t show_da9210_access(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[show_da9210_access] 0x%x\n", g_reg_value_da9210);
    return sprintf(buf, "%u\n", g_reg_value_da9210);
}
static ssize_t store_da9210_access(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    int ret=0;
    char *pvalue = NULL;
    unsigned int reg_value = 0;
    unsigned int reg_address = 0;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[store_da9210_access] \n");
    
    if(buf != NULL && size != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[store_da9210_access] buf is %s \n",buf);
        reg_address = simple_strtoul(buf,&pvalue,16);
        
        if(size > 4)
        {        
            reg_value = simple_strtoul((pvalue+1),NULL,16);        
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[store_da9210_access] write da9210 reg 0x%x with value 0x%x !\n",reg_address,reg_value);

            if(reg_address < 0x100)
            {
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[store_da9210_access] page 0,1\n");
                da9210_config_interface(0x0, 0x0, 0xF, 0); // select to page 0,1
            }
            else
            {
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[store_da9210_access] page 2,3\n");
                da9210_config_interface(0x0, 0x2, 0xF, 0); // select to page 2,3
                reg_address = reg_address & 0xFF;
            }
            ret=da9210_config_interface(reg_address, reg_value, 0xFF, 0x0);

            //restore to page 0,1
            da9210_config_interface(0x0, 0x0, 0xF, 0); // select to page 0,1
        }
        else
        {               
            if(reg_address < 0x100)
            {
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[store_da9210_access] page 0,1\n");
                da9210_config_interface(0x0, 0x0, 0xF, 0); // select to page 0,1
            }
            else
            {
                xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[store_da9210_access] page 2,3\n");
                da9210_config_interface(0x0, 0x2, 0xF, 0); // select to page 2,3
                reg_address = reg_address & 0xFF;
            }
            ret=da9210_read_interface(reg_address, &g_reg_value_da9210, 0xFF, 0x0);

            //restore to page 0,1
            da9210_config_interface(0x0, 0x0, 0xF, 0); // select to page 0,1

            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[store_da9210_access] read da9210 reg 0x%x with value 0x%x !\n",reg_address,g_reg_value_da9210);
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[store_da9210_access] Please use \"cat da9210_access\" to get value\r\n");
        }        
    }    
    return size;
}
static DEVICE_ATTR(da9210_access, 0664, show_da9210_access, store_da9210_access); //664

//==============================================================================
// da9210_vosel_pin
//==============================================================================
int g_da9210_vosel_pin=0;
static ssize_t show_da9210_vosel_pin(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[show_da9210_vosel_pin] g_da9210_vosel_pin=%d\n", g_da9210_vosel_pin);
    return sprintf(buf, "%u\n", g_da9210_vosel_pin);
}
static ssize_t store_da9210_vosel_pin(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    int val=0;
    char *pvalue = NULL;
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_da9210_vosel_pin] \n");
    
    val = simple_strtoul(buf,&pvalue,16);
    g_da9210_vosel_pin = val;
    ext_buck_vproc_vsel(g_da9210_vosel_pin);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[store_da9210_vosel_pin] ext_buck_vproc_vsel(%d)\n", g_da9210_vosel_pin);
        
    return size;
}
static DEVICE_ATTR(da9210_vosel_pin, 0664, show_da9210_vosel_pin, store_da9210_vosel_pin); //664

//==============================================================================
// da9210_user_space_probe
//==============================================================================
static int da9210_user_space_probe(struct platform_device *dev)    
{    
    int ret_device_file = 0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","******** da9210_user_space_probe!! ********\n" );
    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_da9210_access);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_da9210_vosel_pin);
    
    return 0;
}

struct platform_device da9210_user_space_device = {
    .name   = "da9210-user",
    .id     = -1,
};

static struct platform_driver da9210_user_space_driver = {
    .probe      = da9210_user_space_probe,
    .driver     = {
        .name = "da9210-user",
    },
};

static struct i2c_board_info __initdata i2c_da9210 = { I2C_BOARD_INFO("da9210", (da9210_SLAVE_ADDR_WRITE>>1))};
#endif

static int __init da9210_init(void)
{   
#ifdef da9210_AUTO_DETECT_DISABLE

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_init] da9210_AUTO_DETECT_DISABLE\n");    
    g_da9210_hw_exist=0;
    g_da9210_driver_ready=1;

#else

    int ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_init] g_vproc_vsel_gpio_number=0x%x, g_vproc_en_gpio_number=0x%x\n", 
        g_vproc_vsel_gpio_number, g_vproc_en_gpio_number);

    if(g_vproc_vsel_gpio_number != 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_init] init start. ch=%d\n", da9210_BUSNUM);
        
        i2c_register_board_info(da9210_BUSNUM, &i2c_da9210, 1);

        if(i2c_add_driver(&da9210_driver)!=0)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_init] failed to register da9210 i2c driver.\n");
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_init] Success to register da9210 i2c driver.\n");
        }

        // da9210 user space access interface
        ret = platform_device_register(&da9210_user_space_device);
        if (ret) {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","****[da9210_init] Unable to device register(%d)\n", ret);
            return ret;
        }    
        ret = platform_driver_register(&da9210_user_space_driver);
        if (ret) {
            xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","****[da9210_init] Unable to register driver (%d)\n", ret);
            return ret;
        }
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC","[da9210_init] DCT no define EXT BUCK\n");    
        g_da9210_hw_exist=0;
        g_da9210_driver_ready=1;
    }
    
#endif    
    
    return 0;        
}

static void __exit da9210_exit(void)
{
    i2c_del_driver(&da9210_driver);
}

module_init(da9210_init);
module_exit(da9210_exit);
   
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C da9210 Driver");
MODULE_AUTHOR("James Lo<james.lo@mediatek.com>");
