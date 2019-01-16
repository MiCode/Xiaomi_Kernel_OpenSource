/*
   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011 Synaptics, Inc.
   
   Permission is hereby granted, free of charge, to any person obtaining a copy of 
   this software and associated documentation files (the "Software"), to deal in 
   the Software without restriction, including without limitation the rights to use, 
   copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the 
   Software, and to permit persons to whom the Software is furnished to do so, 
   subject to the following conditions:
   
   The above copyright notice and this permission notice shall be included in all 
   copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
   SOFTWARE.

  
   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
#include "RefCode_CustomerImplementation.h"
#include <linux/delay.h>	//msleep
#include <linux/gpio.h>		//gpio_get_value

#include <linux/file.h>		//for file access
#include <linux/syscalls.h> //for file access
#include <linux/uaccess.h>  //for file access

#include <mach/mt_gpio.h>
#include "cust_gpio_usage.h"


static char currentPage;
static char line[4096]={0};
int LimitFile[30][46*2];
//float LimitFile[30][46*2];

void device_I2C_read(unsigned char add, unsigned char *value, unsigned short len)
{
	// I2C read
	if(ds4_i2c_client == NULL) {
		printk("[Touch] ds4_i2c_client is NULL\n");
		return;
	} else {
		if(synaptics_ts_read(ds4_i2c_client, add, len, value) < 0) return;
	}
}

void device_I2C_write(unsigned char add, unsigned char *value, unsigned short len)
{
	// I2C write
	if(ds4_i2c_client == NULL) {
		printk("[Touch] ds4_i2c_client is NULL\n");
		return;
	} else {
		if(synaptics_ts_write(ds4_i2c_client, add, value, len) < 0) return;
	}
}

void InitPage(void)
{
	currentPage = 0;
}

void SetPage(unsigned char page)
{
	device_I2C_write(0xFF, &page, 1); //changing page
}

void readRMI(unsigned short add, unsigned char *value, unsigned short len)
{
	unsigned char temp;

	temp = add >> 8;
	if(temp != currentPage)
	{
		currentPage = temp;
		SetPage(currentPage);
	}
	device_I2C_read(add & 0xFF, value, len);
}

void longReadRMI(unsigned short add, unsigned char *value, unsigned short len)
{
	unsigned char temp;

	temp = add >> 8;
	if(temp != currentPage)
	{
		currentPage = temp;
		SetPage(currentPage);
	}

	if(ds4_i2c_client == NULL) {
		printk("[Touch] ds4_i2c_client is NULL\n");
		return;
	} else {
		if(synaptics_ts_read_f54(ds4_i2c_client, (add & 0xFF), len, value) < 0) return;
	}
}

void writeRMI(unsigned short add, unsigned char *value, unsigned short len)
{
	unsigned char temp;

	temp = add >> 8;
	if(temp != currentPage)
	{
		currentPage = temp;
		SetPage(currentPage);
	}
	device_I2C_write(add & 0xFF, value, len);
}

void delayMS(int val)
{
	msleep(val);
	// Wait for val MS
}

void cleanExit(int code)
{
	//FIXME: add kernel exit function
	return;
}

int waitATTN(int code, int time)
{
	int trial_us=0;

	while ((mt_get_gpio_in(GPIO_CTP_EINT_PIN) != 0) && (trial_us < (time * 1000))) {
		udelay(1);
		trial_us++;
	}

	if (mt_get_gpio_in(GPIO_CTP_EINT_PIN) != 0)
		return -EBUSY;
	else
		return 1;
}

void write_log(char *data)
{
	int fd;
	char *fname = "/mnt/sdcard/synaptics_f54_log.txt";

	mm_segment_t old_fs = get_fs();  
	set_fs(KERNEL_DS);  

	fd = sys_open(fname, O_WRONLY|O_CREAT|O_APPEND, 0644);  

	if (fd >= 0)
	{  	
		sys_write(fd, data, strlen(data));
		sys_close(fd);  
	}

	set_fs(old_fs);
}

int get_limit( unsigned char Tx, unsigned char Rx)
{
	int fd, i, j;
	char *fname = "/mnt/sdcard/synaptics_f54_limit.txt";
	char buff[5]={0};
	int p = 0;
	int q = 0;
	int ret = 0;

	mm_segment_t old_fs = get_fs();  
	set_fs(KERNEL_DS);

	fd = sys_open(fname, O_RDONLY, 0);

	if ( fd < 0 ) ret = 0;
	else {
		memset(LimitFile, 0, sizeof(LimitFile));
		memset(line, 0, sizeof(line));
		sys_read(fd, line, sizeof(line));
		for(i = 0; i < Tx; i++) {
			for(j = 0; j < Rx; j++) {
				while(1) {
					if( line[q] != ',' && line[q] != '\r' && line[q] != '\n' ) {
						buff[p++] = line[q++];
					} else if(line[q] == ',') {
						p=0; q++;
						LimitFile[i][j] = (int)simple_strtol(&buff[0], NULL, 10);
						//printk("LimitFile[%d][%d]=%d ", i, j, LimitFile[i][j]);
						memset(buff, 0, sizeof(buff));
						break;
					} else {
						q++;
					}
				}
			}
		}
		ret = 1;
	}
	sys_close(fd);
	set_fs(old_fs);

	return ret;
}
#if 0
int waitATTN(unsigned char set, int val)
{
	if(set ==1)
	{
		// Wait ATTN for val MS
		
		// If get ATTN within val MS, return 1;
		// else, return 0;
	}
	else
	{
		// Wait no ATTN for val MS

		// If get ATTN within val MS, return 1;
		// else, return 0;
	}
}

void main(void)
/* Please be informed this main() function is an example for host implementation */
{
	// Run PDT Scan
	SYNA_PDTScan();
	SYNA_ConstructRMI_F54();
	SYNA_ConstructRMI_F1A();

	// Run test functions
	F54_FullRawCap(0);

	// Check FW information including product ID
	FirmwareCheck();
}
#endif
