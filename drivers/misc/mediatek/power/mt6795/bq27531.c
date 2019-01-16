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

#include "bq27531.h"

#include "cust_charging.h"
#include <mach/charging.h>

#include <linux/vmalloc.h>
#include <asm/unaligned.h>

#include <linux/dma-mapping.h>

/**********************************************************
  *
  *   [I2C Slave Setting] 
  *
  *********************************************************/
#define BQ27531_SLAVE_ADDR_WRITE	0xAA
#define BQ27531_SLAVE_ADDR_READ		0xAB

#define BQ27531_SLAVE_ADDR_ROM	0x16

static unsigned int bq27531_fw_update_status = 0;

static unsigned short bq27531_cmd_addr[bq27531_REG_NUM] = 
{
	0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e,
	0x10, 0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e, 
	0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e,
	0x30, 0x32, 0x34, 0x6e, 0x70, 0x72
};

static struct i2c_client *new_client = NULL;
static const struct i2c_device_id bq27531_i2c_id[] = {{"bq27531",0},{}};   
kal_bool fg_hw_init_done = KAL_FALSE; 
static int bq27531_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);

static struct i2c_driver bq27531_driver = {
    .driver = {
        .name    = "bq27531",
    },
    .probe       = bq27531_driver_probe,
    .id_table    = bq27531_i2c_id,
};

static u8 *I2CDMABuf_va = NULL;
static u32 I2CDMABuf_pa = NULL;

/**********************************************************
  *
  *   [Global Variable] 
  *
  *********************************************************/
kal_uint16 bq27531_reg[bq27531_REG_NUM] = {0};

static DEFINE_MUTEX(bq27531_i2c_access);
/**********************************************************
  *
  *   [I2C Function For Read/Write fan5405] 
  *
  *********************************************************/
int bq27531_read_byte(kal_uint8 cmd, kal_uint8 *returnData)
{
    char     cmd_buf[1]={0x00};
    char     readData = 0;
    int      ret=0;

    mutex_lock(&bq27531_i2c_access);

    //new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG;    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;

    cmd_buf[0] = cmd;

    ret = i2c_master_send(new_client, &cmd_buf[0], (1<<8 | 1));
    if (ret < 0) 
    {    
        //new_client->addr = new_client->addr & I2C_MASK_FLAG;
        new_client->ext_flag=0;
        mutex_unlock(&bq27531_i2c_access);
        return 0;
    }
    
    readData = cmd_buf[0];
    *returnData = readData;

    // new_client->addr = new_client->addr & I2C_MASK_FLAG;
    new_client->ext_flag=0;
    
    mutex_unlock(&bq27531_i2c_access);    

    return 1;
}

int bq27531_write_byte(kal_uint8 cmd, kal_uint8 writeData)
{
    char    write_data[2] = {0};
    int     ret=0;

   // printk("[bq27531] bq27531_write_byte,cmd=0x%x, data=0x%x\n", cmd, writeData);

    mutex_lock(&bq27531_i2c_access);
    
    write_data[0] = cmd;
    write_data[1] = writeData;
    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;
    
    ret = i2c_master_send(new_client, write_data, 2);
	//printk("[bq27531] bq27531_write_byte,ret=%d\n", ret);
    if (ret < 0) 
    {
       
        new_client->ext_flag=0;
        mutex_unlock(&bq27531_i2c_access);
        return 0;
    }
    
    new_client->ext_flag=0;
    mutex_unlock(&bq27531_i2c_access);

    return 1;
}

int bq27531_read_2byte(kal_uint8 cmd, kal_uint16 *returnData)
{
    char     cmd_buf[2]={0x00};
    char     readData = 0;
    int      ret=0;

    mutex_lock(&bq27531_i2c_access);
    
    //new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG;    
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;

    cmd_buf[0] = cmd;

    ret = i2c_master_send(new_client, &cmd_buf[0], (2<<8 | 1));
    if (ret < 0) 
    {    
        //new_client->addr = new_client->addr & I2C_MASK_FLAG;
        new_client->ext_flag=0;

        mutex_unlock(&bq27531_i2c_access);
        return 0;
    }
    
    *returnData = (kal_uint16) (((cmd_buf[1]<<8)&0xff00) | (cmd_buf[0]&0xff));
    //*returnData = readData;

    // new_client->addr = new_client->addr & I2C_MASK_FLAG;
    new_client->ext_flag=0;
    
    mutex_unlock(&bq27531_i2c_access);    
    return 1;
}

int bq27531_write_2byte(kal_uint8 cmd, kal_uint16 writeData)
{
    char    write_data[4] = {0};
    int     ret=0;
    
    mutex_lock(&bq27531_i2c_access);
    
    write_data[0] = cmd;

    write_data[1] = (char) writeData&0xff;
    write_data[2] = (char) ((writeData>>8)&0xff);

    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;
    
    ret = i2c_master_send(new_client, write_data, 4);
    if (ret < 0) 
    {
       
        new_client->ext_flag=0;
        mutex_unlock(&bq27531_i2c_access);
        return 0;
    }
    
    new_client->ext_flag=0;
    mutex_unlock(&bq27531_i2c_access);
    return 1;
}

int bq27531_read_bytes(kal_uint8 slave_addr, kal_uint8 *returnData, kal_uint32 len)
{
	kal_uint8* buf;
	int ret=0;

	mutex_lock(&bq27531_i2c_access);

	new_client->addr = (slave_addr>>1);	
	//new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG;    
	new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;

	printk("ww_debug read 0x%x :", returnData[0]);
	ret = i2c_master_send(new_client, &returnData[0], ((len-1)<<8 | 1));
	//	printk("[bq27531] bq27531_read_byte,ret=%d, data=0x%x\n", ret, cmd_buf[0]);	
	if (ret < 0) 
	{    
		//new_client->addr = new_client->addr & I2C_MASK_FLAG;
		new_client->ext_flag=0;
		new_client->addr = (BQ27531_SLAVE_ADDR_WRITE>>1);

		printk("[bq27531] bq27531_read_byte,ret<0 err\n");
		mutex_unlock(&bq27531_i2c_access);
		//kfree(buf);

		return 0;
	}

	for(ret=0;ret<len-1;ret++)
		printk(" 0x%x", returnData[ret]);
	printk(" \n");

	//memcpy(returnData, &buf[0], len-1);

	// new_client->addr = new_client->addr & I2C_MASK_FLAG;
	new_client->ext_flag=0;
	new_client->addr = (BQ27531_SLAVE_ADDR_WRITE>>1);    
	mutex_unlock(&bq27531_i2c_access);  

	//kfree(buf);
	return ret;
}

int bq27531_write_bytes(kal_uint8 slave_addr, kal_uint8* writeData, kal_uint32 len)
{
	int     ret=0;
	//kal_uint8* buf;
	int i = 0;
	//buf = (kal_uint8*) kmalloc(sizeof(kal_uint8)*(len+1), GFP_KERNEL);
	//memcpy(&buf[1], writeData, len);

	mutex_lock(&bq27531_i2c_access);

	new_client->addr = (slave_addr>>1);

	new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;

	printk("ww_debug write:");
	for(ret=0;ret<len;ret++)
		printk(" 0x%x", writeData[ret]);
	printk(" \n");

	for(i = 0 ; i < len; i++)
	{
		I2CDMABuf_va[i] = writeData[i];
	}

	if(len <= 8)
	{
		//i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;
		//MSE_ERR("Sensor non-dma write timing is %x!\r\n", this_client->timing);
		i2c_master_send(new_client, writeData, len);
	}
	else
	{
		new_client->addr = new_client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
		//MSE_ERR("Sensor dma timing is %x!\r\n", this_client->timing);
		i2c_master_send(new_client, I2CDMABuf_pa, len);
	}   

	new_client->addr = new_client->addr & I2C_MASK_FLAG;
	new_client->ext_flag=0;
	new_client->addr = (BQ27531_SLAVE_ADDR_WRITE>>1);		
	mutex_unlock(&bq27531_i2c_access);

	//kfree(buf);

	return ret;
}

int bq27531_write_single_bytes(kal_uint8 slave_addr, kal_uint8* writeData, kal_uint32 len)
{
	int     ret=0;
	int tmp=1;
	//kal_uint8* buf;

	//buf = (kal_uint8*) kmalloc(sizeof(kal_uint8)*(len+1), GFP_KERNEL);
	//memcpy(&buf[1], writeData, len);
	unsigned char buf[2];
	int i;
	
	mutex_lock(&bq27531_i2c_access);

	new_client->addr = (slave_addr>>1);

	new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;

	printk("ww_debug write:");
	for(ret=0;ret<len;ret++)
		printk(" 0x%x", writeData[ret]);
	printk(" \n");

	printk("ww_debug single write:");
	for(i=0;i<len-1;i++)
	{
		buf[0] = writeData[0] + i;
		buf[1] = writeData[i+1];

		printk("data1=0x%x, data2=0x%x  |  ", buf[0], buf[1]);
		ret = i2c_master_send(new_client, buf, 2);
		if (ret < 0) 
		{
			new_client->ext_flag=0;
			new_client->addr = (BQ27531_SLAVE_ADDR_WRITE>>1);	

			mutex_unlock(&bq27531_i2c_access);

			//kfree(buf);
			return 0;
		}

		ret -= 1;
		tmp += ret;
	}
	printk(" \n");
	
	new_client->ext_flag=0;
	new_client->addr = (BQ27531_SLAVE_ADDR_WRITE>>1);		
	mutex_unlock(&bq27531_i2c_access);

	//kfree(buf);

	return tmp;
}

int bq27531_read_ctrl(kal_uint16 cmd, kal_uint16 *returnData)
{
    char     cmd_buf[4]={0x00};
    char     readData = 0;
    int      ret=0;

    mutex_lock(&bq27531_i2c_access);
    
    //new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG;    
    //new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;
	
    cmd_buf[0] = (char) 0x00;

    cmd_buf[1] = (char) cmd&0xff;
    cmd_buf[2] = (char) (cmd>>8)&0xff;
	
    ret = i2c_master_send(new_client, &cmd_buf[0], 3);
    if (ret < 0) 
    {    
        //new_client->addr = new_client->addr & I2C_MASK_FLAG;
        new_client->ext_flag=0;
        printk("CL bq27531_get_ctrl_dfver err1 cmd=%x\n",cmd);
        mutex_unlock(&bq27531_i2c_access);
        return 0;
    }

	new_client->ext_flag=0;	
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_WR_FLAG | I2C_DIRECTION_FLAG;

    ret = i2c_master_send(new_client, &cmd_buf[0], ((2<<8 | 1)));
    if (ret < 0) 
    {    
        //new_client->addr = new_client->addr & I2C_MASK_FLAG;
        new_client->ext_flag=0;
        printk("CL bq27531_get_ctrl_dfver err2 cmd=%x\n",cmd);
        mutex_unlock(&bq27531_i2c_access);
        return 0;
    }
	
    *returnData = (kal_uint16) (((cmd_buf[1]<<8)&0xff00) | (cmd_buf[0]&0xff));
    //*returnData = readData;

    // new_client->addr = new_client->addr & I2C_MASK_FLAG;
    new_client->ext_flag=0;
    
    mutex_unlock(&bq27531_i2c_access);    
    return 1;
}

int bq27531_write_ctrl(kal_uint16 cmd, kal_uint16 writeData)
{
    char    write_data[6] = {0};
    int     ret=0;
    
    mutex_lock(&bq27531_i2c_access);
    
    write_data[0] = (char) 0x00;

    write_data[1] = (char) cmd&0xff;
    write_data[2] = (char) (cmd>>8)&0xff;

    write_data[3] = (char) writeData&0xff;
    write_data[4] = (char) (writeData>>8)&0xff;

	for(ret=0;ret<5;ret++)
		printk("ww_debug data[%d]=0x%x\n", ret, write_data[ret]);
	
    new_client->ext_flag=((new_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;
    
    ret = i2c_master_send(new_client, write_data, 5);
    if (ret < 0) 
    {
       
        new_client->ext_flag=0;
        mutex_unlock(&bq27531_i2c_access);
        return 0;
    }
    
    new_client->ext_flag=0;
    mutex_unlock(&bq27531_i2c_access);
    return 1;
}

/**********************************************************
  *
  *   [Read / Write Function] 
  *
  *********************************************************/
kal_uint32 bq27531_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 bq27531_reg = 0;
    int ret = 0;

   battery_xlog_printk(BAT_LOG_FULL,"--------------------------------------------------\n");

    ret = bq27531_read_byte(RegNum, &bq27531_reg);

	battery_xlog_printk(BAT_LOG_FULL,"[bq27531_read_interface] Reg[%x]=0x%x\n", RegNum, bq27531_reg);
	
    bq27531_reg &= (MASK << SHIFT);
    *val = (bq27531_reg >> SHIFT);
	
	battery_xlog_printk(BAT_LOG_FULL,"[bq27531_read_interface] val=0x%x\n", *val);
	
    return ret;
}

kal_uint32 bq27531_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT)
{
    kal_uint8 bq27531_reg = 0;
    int ret = 0;

    battery_xlog_printk(BAT_LOG_FULL,"--------------------------------------------------\n");

    ret = bq27531_read_byte(RegNum, &bq27531_reg);
    battery_xlog_printk(BAT_LOG_FULL,"[bq27531_config_interface] Reg[%x]=0x%x\n", RegNum, bq27531_reg);
    
    bq27531_reg &= ~(MASK << SHIFT);
    bq27531_reg |= (val << SHIFT);

    ret = bq27531_write_byte(RegNum, bq27531_reg);
    battery_xlog_printk(BAT_LOG_FULL,"[bq27531_config_interface] write Reg[%x]=0x%x\n", RegNum, bq27531_reg);

    // Check
    //bq27531_read_byte(RegNum, &bq27531_reg);
    //printk("[bq27531_config_interface] Check Reg[%x]=0x%x\n", RegNum, bq27531_reg);

    return ret;
}

//write one register directly
kal_uint32 bq27531_config_interface_liao (kal_uint8 RegNum, kal_uint8 val)
{   
    int ret = 0;
    
    ret = bq27531_write_byte(RegNum, val);

    return ret;
}

/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/

//command 0 : control
unsigned int bq27531_get_ctrl_devicetype(void)
{
	kal_uint32 ret=0;    
	kal_uint32 val = 0;

	ret=bq27531_read_ctrl(bq27531_CTRL_DEVTYPE, (kal_uint16*) &val);
	if(ret<=0)
		val = 0xffff;
	
	return val;
}

unsigned int bq27531_get_ctrl_chipstatus(void)
{
	kal_uint32 ret=0;    
	kal_uint32 val = 0;

	ret=bq27531_read_ctrl(bq27531_CTRL_STATUS, (kal_uint16*) &val);	
	if(ret<=0)
		val = 0xffff;
	
	return val;
}

unsigned int bq27531_get_ctrl_fwver(void)
{
    	kal_uint32 ret=0;    
	kal_uint32 val = 0;

    	ret=bq27531_read_ctrl(bq27531_CTRL_FWVER, (kal_uint16*) &val);
	if(ret<=0)
		val = 0xffff;
	
	return val;
}


unsigned int bq27531_get_ocv(void)
{
    kal_uint32 ret=0;     		
	kal_uint32 val = 0;

    ret=bq27531_read_ctrl(bq27531_CTRL_OCVCMD, (kal_uint16*) &val);
	if(ret<=0)
		val = 0xffff;
	
	return val;
}
void bq27531_ctrl_enableotg(void)
{
    kal_uint32 ret=0;    

    ret=bq27531_write_2byte(bq27531_CMD_CONTROL, bq27531_CTRL_OTGENABLE);

	return ret;
}

void bq27531_ctrl_disableotg(void)
{
    kal_uint32 ret=0;    

    ret=bq27531_write_2byte(bq27531_CMD_CONTROL, bq27531_CTRL_OTGDISABLE);

	return ret;
}

unsigned int bq27531_ctrl_ctrlenablecharge(void)
{
    kal_uint32 ret=0;    

    ret=bq27531_write_2byte(bq27531_CMD_CONTROL, bq27531_CTRL_CHGCTLENABLE);
	
	return ret;
}

unsigned int bq27531_ctrl_ctrldisablecharge(void)
{
    kal_uint32 ret=0;    

    ret=bq27531_write_2byte(bq27531_CMD_CONTROL, bq27531_CTRL_CHGCTLDISABLE);
	
	return ret;
}

unsigned int bq27531_ctrl_enablecharge(void)
{
    kal_uint32 ret=0;    

    ret=bq27531_write_2byte(bq27531_CMD_CONTROL, bq27531_CTRL_CHGENABLE);

	return ret;
}

unsigned int bq27531_ctrl_disablecharge(void)
{
    kal_uint32 ret=0;    

    ret=bq27531_write_2byte(bq27531_CMD_CONTROL, bq27531_CTRL_CHGDISABLE);

	return ret;
}

unsigned int bq27531_get_ctrl_dfver(void)
{
    	kal_uint32 ret=0;    
	kal_uint32 val = 0;

   	 ret=bq27531_read_ctrl(bq27531_CTRL_DFVER, (kal_uint16*) &val);
	 printk("CL bq27531_get_ctrl_dfver ret=%d\n",ret);
	if(ret<=0)
		val = 0xffff;
	

	return val;
}

//other command
void bq27531_set_temperature(kal_int32 temp)
{
	//unsigned short ret = 0;
	temp = temp*10 + 2735;
	
	bq27531_write_2byte(bq27531_CMD_TEMPERATURE, temp);

	//return ret;
}
kal_int32 bq27531_get_temperature(void)
{
	unsigned short ret = 0;
	
	bq27531_read_2byte(bq27531_CMD_TEMPERATURE, &ret);

	return (ret-2735)/10;
}
unsigned short bq27531_get_voltage(void)
{
	unsigned short ret = 0;
	
	bq27531_read_2byte(bq27531_CMD_VOLTAGE, &ret);

	return ret;
}
short bq27531_get_averagecurrent(void)
{
	short ret = 0;
	
	bq27531_read_2byte(bq27531_CMD_AVGCUR, (unsigned short*)&ret);

 	printk("[bq27531] ww_debug cur = %d\n", (int) ret);
	
	return ret;
}

short bq27531_get_instantaneouscurrent(void)
{
	short ret = 0;
	
	bq27531_read_2byte(bq27531_CMD_CURREADING, (unsigned short*)&ret);

 	printk("[bq27531] ww_debug cur = %d\n", (int) ret);
	
	return ret;
}
short bq27531_get_percengage_of_fullchargercapacity(void)
{
	short ret = 0;
	
	bq27531_read_2byte(bq27531_CMD_STATECHARGE, (unsigned short*)&ret);

 	printk("[bq27531] ww_debug cur = %d\n", (int) ret);
	
	return ret;
}
short bq27531_get_remaincap(void)
{
	short ret = 0;
	
	bq27531_read_2byte(bq27531_CMD_REMAINCAP, (unsigned short*)&ret);

 	printk("[bq27531] ww_debug cur = %d\n", (int) ret);
	
	return ret;
}

void bq27531_set_charge_voltage(unsigned short vol)
{
	unsigned short ret = 0;
	
	bq27531_write_2byte(bq27531_CMD_PROGCARGINGVOL, vol);

	return ret;
}

short bq27531_get_charge_voltage(void)
{
	short ret = 0;
	
	bq27531_read_2byte(bq27531_CMD_PROGCARGINGVOL, (unsigned short*)&ret);
 	printk("[bq27531] ww_debug chg vol = %d\n", (short) ret);

	return ret;
}
//------------rom mode for firmware-----------------
unsigned int bq27531_enter_rommode(void)
{
    kal_uint32 ret=0;    
	kal_uint16 cmd = 0x0f00;

    ret=bq27531_write_2byte(bq27531_CMD_CONTROL, cmd);
	
	return ret;
}
unsigned int bq27531_exit_rommode(void)
{
    	kal_uint32 ret=0;    
	kal_uint8 cmd[2] = {0x00, 0x0f};

    	ret=bq27531_write_bytes(BQ27531_SLAVE_ADDR_ROM, cmd, 2);
		
	cmd[0] = 0x64;
	cmd[1] = 0x0f;	
   	 ret=bq27531_write_bytes(BQ27531_SLAVE_ADDR_ROM, cmd, 2);
	
	cmd[0] = 0x65;
	cmd[1] = 0x00;	
    	ret=bq27531_write_bytes(BQ27531_SLAVE_ADDR_ROM, cmd, 2);
	
	return ret;
}
/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/
  
void bq27531_dump_2register(void)
{
    int i=0;
unsigned char templ;
unsigned char temph;

    printk("[bq27531] ");
    for (i=2;i<bq27531_REG_NUM;i++)
    {
        bq27531_read_byte(i, &templ);
        bq27531_read_byte(i+1, &temph);
		bq27531_reg[i] = ((temph<<8)&0xff00)|templ;
        printk("bq27531_dump_2register: [0x%x]=0x%x, l=0x%x, h=0x%x \n", i, bq27531_reg[i], templ, temph);        
    }
    printk("\n");
}

void bq27531_dump_register(void)
{
    int i=0;
	unsigned int id = 0;
	
    printk("[bq27531] ");
#if 0
    for (i=1;i<bq27531_REG_NUM;i++)
    {
        bq27531_read_2byte(bq27531_cmd_addr[i], &bq27531_reg[i]);
        printk("[0x%x]=0x%x ", bq27531_cmd_addr[i], bq27531_reg[i]);        
    }
    printk("\n");
#endif

#if 0 //test id
	msleep(500);	
	id = bq27531_get_ctrl_devicetype();
 	printk("ww_debug id = 0x%x\n", id);

	id = bq27531_get_ctrl_fwver();
 	printk("ww_debug fwver = 0x%x\n", id);	
#endif

}

static int bq27531_driver_probe(struct i2c_client *client, const struct i2c_device_id *id) 
{             
    int err=0; 

    battery_xlog_printk(BAT_LOG_CRTI,"[bq27531_driver_probe] \n");

    if (!(new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) {
        err = -ENOMEM;
        goto exit;
    }    
    memset(new_client, 0, sizeof(struct i2c_client));

    new_client = client;    

	//new_client->addr = (BQ27531_SLAVE_ADDR_WRITE>>1);

	I2CDMABuf_va = (u8 *)dma_alloc_coherent(NULL, 4096, &I2CDMABuf_pa, GFP_KERNEL);
    	if(!I2CDMABuf_va)
	{
    		printk("[BQ27531] dma_alloc_coherent error\n");
	}
		
    //---------------------
    //bq27531_dump_register();
    fg_hw_init_done = KAL_TRUE;
	
    return 0;                                                                                       

exit:
    return err;

}

/**********************************************************
  *
  *   [platform_driver API] 
  *
  *********************************************************/
static ssize_t show_bq27531_download_fw(struct device *dev,struct device_attribute *attr, char *buf)
{
        unsigned int tmp = 0;
		
        int chipid = bq27531_get_ctrl_devicetype();
		int fwver = bq27531_get_ctrl_fwver();
		int dfver = bq27531_get_ctrl_dfver();
		
		printk("[show_bq27531_download_fw] chi id =0x%x, fw ver = 0x%x, dfver = 0x%x\n", chipid, fwver, dfver);

		if(BQ27531_CHIPID != chipid)
		{
            tmp |= ERR_CHIPID; 
		}
		
		if(BQ27531_FWVER != fwver)
		{
            tmp |= ERR_FWVER; 
		}

		if(BQ27531_DFVER != dfver)
		{
            tmp |= ERR_DFVER; 
		}

        return sprintf(buf, "%u\n", (bq27531_fw_update_status | tmp));

		printk("[show_bq27531_download_fw] bq27531_fw_update_status =0x%x, tmp =0x%x \n", bq27531_fw_update_status,tmp);
}

static ssize_t store_bq27531_download_fw(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    char *pvalue = NULL;
	unsigned int download_cmd = 0;
	int ret;
	int i = 0;
	
    printk( "[store_bq27531_download_fw] in\n");
    if(buf != NULL && size != 0)
    {
        download_cmd = simple_strtoul(buf,&pvalue,16);      
        printk("[store_bq27531_download_fw] buf is %s and size is %d,cmd is %d \n",buf,size,download_cmd);

		do
		{
           bq27531_fw_update_status = bq27531_check_fw_ver();
		   i++;
		   printk(" [store_bq27531_download_fw] download times is %d \n",i);
		}
		while((bq27531_fw_update_status != 0) && (i < 3));
    }        
    return size;
}


static DEVICE_ATTR(bq27531_download_fw, 0664, show_bq27531_download_fw, store_bq27531_download_fw); //664

static int bq27531_user_space_probe(struct platform_device *dev)    
{    
    int ret_device_file = 0;

    battery_xlog_printk(BAT_LOG_CRTI,"******** bq27531_user_space_probe!! ********\n" );
    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_bq27531_download_fw);
    
    return 0;
}

struct platform_device bq27531_user_space_device = {
    .name   = "bq27531-user",
    .id     = -1,
};

static struct platform_driver bq27531_user_space_driver = {
    .probe      = bq27531_user_space_probe,
    .driver     = {
        .name = "bq27531-user",
    },
};


static struct i2c_board_info __initdata i2c_bq27531 = { I2C_BOARD_INFO("bq27531", (BQ27531_SLAVE_ADDR_WRITE>>1))};
//static struct i2c_board_info __initdata i2c_bq27531 = { I2C_BOARD_INFO("bq27531", (0x56))};

static int __init bq27531_init(void)
{    
    int ret=0;
    
    battery_xlog_printk(BAT_LOG_CRTI,"[bq27531_init] init start\n");
    
    i2c_register_board_info(BQ27531_BUSNUM, &i2c_bq27531, 1);

    if(i2c_add_driver(&bq27531_driver)!=0)
    {
        battery_xlog_printk(BAT_LOG_CRTI,"[bq27531_init] failed to register bq27531 i2c driver.\n");
    }
    else
    {
        battery_xlog_printk(BAT_LOG_CRTI,"[bq27531_init] Success to register bq27531 i2c driver.\n");
    }

    // fan5405 user space access interface
    ret = platform_device_register(&bq27531_user_space_device);
    if (ret) {
        battery_xlog_printk(BAT_LOG_CRTI,"****[bq27531_init] Unable to device register(%d)\n", ret);
        return ret;
    }    
    ret = platform_driver_register(&bq27531_user_space_driver);
    if (ret) {
        battery_xlog_printk(BAT_LOG_CRTI,"****[bq27531_init] Unable to register driver (%d)\n", ret);
        return ret;
    }

    return 0;        
}

static void __exit bq27531_exit(void)
{
    i2c_del_driver(&bq27531_driver);
}

module_init(bq27531_init);
module_exit(bq27531_exit);
   
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq27531 Driver");
MODULE_AUTHOR("James Lo<james.lo@mediatek.com>");
