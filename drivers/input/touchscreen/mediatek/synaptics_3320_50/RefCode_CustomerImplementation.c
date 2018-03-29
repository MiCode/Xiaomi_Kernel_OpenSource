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
#include "RefCode_F54.h"
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>	/* msleep */
#include <linux/file.h>		/* for file access */
#include <linux/syscalls.h>	/* for file access */
#include <linux/uaccess.h>	/* for file access */
#include <linux/firmware.h>
#include "include/s3320_driver.h"

/* static char line[49152]={0}; */
int UpperImage[32][32];
int LowerImage[32][32];
int SensorSpeedUpperImage[32][32];
int SensorSpeedLowerImage[32][32];
int ADCUpperImage[32][32];
int ADCLowerImage[32][32];

/* extern struct i2c_client *ds4_i2c_client; */
/* extern struct i2c_client *tpd_i2c_client; */


/* extern int touch_i2c_read(struct i2c_client *client, u8 reg, int len, u8 *buf); */
/* extern int touch_i2c_write(struct i2c_client *client, u8 reg, int len, u8 *buf); */

/* extern int synaptics_ts_read(struct i2c_client *client, u8 reg, int num, u8 *buf); */
/* extern int synaptics_ts_write(struct i2c_client *client, u8 reg, u8 *buf, int len); */
/* extern int synaptics_ts_read_f54(struct i2c_client *client, u8 reg, int num, u8 *buf); */

int Read8BitRegisters(unsigned short regAddr, unsigned char *data, int length)
{
	/* I2C read */
	int rst = 0;

	if (ds4_i2c_client == NULL) {
		pr_debug("s3528 ds4_i2c_client is null");
		return -1;
	}

	rst = synaptics_ts_read(ds4_i2c_client, regAddr, length, data);
	if (rst < 0)
		TPD_ERR("Read8BitRegisters read fail\n");




	return rst;
}

int ReadF54BitRegisters(unsigned short regAddr, unsigned char *data, int length)
{
	/* I2C read */
	int rst = 0;

	TPD_FUN();
	if (ds4_i2c_client == NULL) {
		pr_debug("s3528 ds4_i2c_client is null");
		return -1;
	}

	rst = synaptics_ts_read_f54(ds4_i2c_client, regAddr, length, data);
	if (rst < 0)
		TPD_ERR("Read8BitRegisters read fail\n");

	return rst;
}



int Write8BitRegisters(unsigned short regAddr, unsigned char *data, int length)
{
	/* I2C write */
	int rst = 0;

	if (ds4_i2c_client == NULL) {
		pr_debug("s3528 ds4_i2c_client is null");
		return -1;
	}


	rst = synaptics_ts_write(ds4_i2c_client, regAddr, data, length);

	if (rst < 0)
		TPD_ERR("Write8BitRegisters read fail\n");

	return rst;
}

void delayMS_DS5(int val)
{
	/* Wait for val MS */
	msleep(val);
}

int write_file(char *filename, char *data)
{
	int fd = 0;

	fd = sys_open(filename, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd < 0) {
		pr_debug("[s3528_write_file] :  Open file error [ %d ]\n", fd);
		return fd;
	}

	sys_write(fd, data, strlen(data));
	sys_close(fd);

	return 0;
}


int write_log_DS5(char *filename, char *data)
{
	/* extern int f54_window_crack; */
	/* extern int f54_window_crack_check_mode; */

	int fd;
	char *fname = "/mnt/sdcard/touch_self_test.txt";
	int cap_file_exist = 0;

	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	if (filename == NULL) {
		fd = sys_open(fname, O_WRONLY | O_CREAT | O_APPEND, 0666);
		pr_debug("write log in /mnt/sdcard/touch_self_test.txt\n");
	} else {
		fd = sys_open(filename, O_WRONLY | O_CREAT, 0666);
		pr_debug("write log in /sns/touch/cap_diff_test.txt\n");
	}

	pr_debug("[s3528-write_log]write file open %s, fd : %d\n", (fd >= 0) ? "success" : "fail",
		 fd);

	if (fd >= 0) {
		/*because of erro, this code is blocked. */
		/*if(sys_newstat((char __user *) fname, (struct stat *)&fstat) < 0) {
		   pr_debug("[Touch] cannot read %s stat info\n", fname);
		   } else {
		   if(fstat.st_size > 5 * 1024 * 1024) {
		   pr_debug("[Touch] delete %s\n", fname);
		   sys_unlink(fname);
		   sys_close(fd);

		   fd = sys_open(fname, O_WRONLY|O_CREAT|O_APPEND, 0644);
		   if(fd >= 0) {
		   sys_write(fd, data, strlen(data));
		   }
		   } else {
		   sys_write(fd, data, strlen(data));
		   }
		   sys_close(fd);
		   } */
		sys_write(fd, data, strlen(data));
		sys_close(fd);

		if (filename != NULL)
			cap_file_exist = 1;
	}
	set_fs(old_fs);

	return cap_file_exist;
}
