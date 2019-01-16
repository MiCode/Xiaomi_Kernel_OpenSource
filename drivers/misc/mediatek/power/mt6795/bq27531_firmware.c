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
#include <asm/unaligned.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include "cust_charging.h"
#include <mach/charging.h>

#include <linux/vmalloc.h>

#include "bq27531.h"
#include "bq27531_firmware.h"

static int bq27531_fw_upgrade(void)
{
	int i;
	int ret;

	bq27531_enter_rommode();
	msleep(1000);

	for(i=0;i<BQFS_INDEX_LEN;i++)	
	//for(i=0;i<5;i++)//test 5 cmd	
	{
		printk("ww_debug cmd 0x%x, addr=0x%x, offset=%d, size=%d\n", bqfs_index[i].i2c_cmd, bqfs_index[i].i2c_addr, bqfs_index[i].data_offset, bqfs_index[i].data_size);
		switch(bqfs_index[i].i2c_cmd)
		{
			case 'W':
				ret = bq27531_write_bytes(bqfs_index[i].i2c_addr, &firmware_data[bqfs_index[i].data_offset], bqfs_index[i].data_size);
				printk("ww_debug write %d\n", ret);
				break;
			case 'X':
				printk("ww_debug sleep %d\n", bqfs_index[i].i2c_addr);
				msleep(bqfs_index[i].i2c_addr);
				break;
			case 'C':
				{
					unsigned char *buf;
					int j;

					buf = (kal_uint8*) kmalloc(sizeof(kal_uint8)*(bqfs_index[i].data_size), GFP_KERNEL);
					memcpy(buf, &firmware_data[bqfs_index[i].data_offset], bqfs_index[i].data_size);
					
					ret = bq27531_read_bytes(bqfs_index[i].i2c_addr, buf, bqfs_index[i].data_size);
					printk("ww_debug read %d\n", ret);
					for(j=0;j<bqfs_index[i].data_size-1;j++)
					{
						unsigned char data1 = buf[j];
						unsigned char data2 = firmware_data[bqfs_index[i].data_offset+1+j];
						printk("ww_debug !(%d) data1=0x%x, data2=0x%x\n", j, data1, data2);
						if(data1!=data2)
						{
							printk("ERROR! bq27531 fw upgrade error! data1=0x%x, data2=0x%x\n", data1, data2);
							kfree(buf);
							//bq27531_exit_rommode();
							return ERR_UPDATE;
						}
					}

					kfree(buf);
				}
				break;	
			default:
				printk("%s : unsupported cmd!\n", __func__);
		}
	}

	msleep(1000);
	//bq27531_exit_rommode();
	return 0;
}

int bq27531_check_fw_ver(void)
{
    int chipid = bq27531_get_ctrl_devicetype();
	int fwver = bq27531_get_ctrl_fwver();
	int dfver = bq27531_get_ctrl_dfver();
	printk("ww_debug chi id =0x%x, fw ver = 0x%x, dfver = 0x%x\n", chipid, fwver, dfver);


	//if(((chipid!=BQ27531_CHIPID)&&(chipid!=BQ27530_CHIPID))||(dfver==0xffff))
	//	return;
			
	//if((chipid!=BQ27531_CHIPID)||(dfver<BQ27531_DFVER))
	//if(dfver != BQ27531_DFVER)
	{
		return bq27531_fw_upgrade();

	}
}













