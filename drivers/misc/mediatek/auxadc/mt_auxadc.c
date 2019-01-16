/*****************************************************************************
 *
 * Filename:
 * ---------
 *    mt_auxadc.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of AUXADC common code
 *
 * Author:
 * -------
 * Zhong Wang
 *
 ****************************************************************************/

#include <linux/init.h>        /* For init/exit macros */
#include <linux/module.h>      /* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <mach/mt_gpt.h>
#include <mach/mt_clkmgr.h>
#include <mach/sync_write.h>
#include <cust_adc.h>		/* generate by DCT Tool */

#include "mt_auxadc.h"
#include <mt_auxadc_sw.h>


/*****************************************************************************
 * Integrate with NVRAM
****************************************************************************/
#define AUXADC_CALI_DEVNAME    "mtk-adc-cali"

#define TEST_ADC_CALI_PRINT _IO('k', 0)
#define SET_ADC_CALI_Slop   _IOW('k', 1, int)
#define SET_ADC_CALI_Offset _IOW('k', 2, int)
#define SET_ADC_CALI_Cal    _IOW('k', 3, int)
#define ADC_CHANNEL_READ    _IOW('k', 4, int)

typedef struct adc_info {
   char channel_name[64];
   int channel_number;
   int reserve1;
   int reserve2;
   int reserve3;
}ADC_INFO;

static ADC_INFO g_adc_info[ADC_CHANNEL_MAX];
static int auxadc_cali_slop[ADC_CHANNEL_MAX]   = {0};
static int auxadc_cali_offset[ADC_CHANNEL_MAX] = {0};

static kal_bool g_AUXADC_Cali = KAL_FALSE;

static int auxadc_cali_cal[1]     = {0};
static int auxadc_in_data[2]  = {1,1};
static int auxadc_out_data[2] = {1,1};

static DEFINE_MUTEX(auxadc_mutex);
static dev_t auxadc_cali_devno;
static int auxadc_cali_major;
static struct cdev *auxadc_cali_cdev;
static struct class *auxadc_cali_class;

static struct task_struct *thread;
static int g_start_debug_thread;

static int g_adc_init_flag;

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // fop Common API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
int IMM_IsAdcInitReady(void)
{
  return g_adc_init_flag;
}

int IMM_get_adc_channel_num(char *channel_name, int len)
{
  unsigned int i;

  printk("[ADC] name = %s\n", channel_name);
  printk("[ADC] name_len = %d\n", len);
	for (i = 0; i < ADC_CHANNEL_MAX; i++) {
		if (!strncmp(channel_name, g_adc_info[i].channel_name, len)) {
      return g_adc_info[i].channel_number;
    }
  }
  printk("[ADC] find channel number failed\n");
  return -1;
}

int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata)
{
	return IMM_auxadc_GetOneChannelValue(dwChannel, data, rawdata);
}

/* 1v == 1000000 uv */
/* this function voltage Unit is uv */
int IMM_GetOneChannelValue_Cali(int Channel, int*voltage)
{
	return IMM_auxadc_GetOneChannelValue_Cali(Channel, voltage);
}


/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // fop API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static long auxadc_cali_unlocked_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
    int i = 0, ret = 0;
    long *user_data_addr;
    long *nvram_data_addr;

    mutex_lock(&auxadc_mutex);

	switch (cmd) {
        case TEST_ADC_CALI_PRINT :
            g_AUXADC_Cali = KAL_FALSE;
            break;

        case SET_ADC_CALI_Slop:
            nvram_data_addr = (long *)arg;
            ret = copy_from_user(auxadc_cali_slop, nvram_data_addr, 36);
            g_AUXADC_Cali = KAL_FALSE;
            /* Protection */
		for (i = 0; i < ADC_CHANNEL_MAX; i++) {
                if ((*(auxadc_cali_slop + i) == 0) || (*(auxadc_cali_slop + i) == 1)) {
                    *(auxadc_cali_slop + i) = 1000;
                }
            }
		for (i = 0; i < ADC_CHANNEL_MAX; i++)
			printk("auxadc_cali_slop[%d] = %d\n", i, *(auxadc_cali_slop + i));
            printk("**** MT auxadc_cali ioctl : SET_ADC_CALI_Slop Done!\n");
            break;

        case SET_ADC_CALI_Offset:
            nvram_data_addr = (long *)arg;
            ret = copy_from_user(auxadc_cali_offset, nvram_data_addr, 36);
            g_AUXADC_Cali = KAL_FALSE;
		for (i = 0; i < ADC_CHANNEL_MAX; i++)
			printk("auxadc_cali_offset[%d] = %d\n", i, *(auxadc_cali_offset + i));
            printk("**** MT auxadc_cali ioctl : SET_ADC_CALI_Offset Done!\n");
            break;

        case SET_ADC_CALI_Cal :
            nvram_data_addr = (long *)arg;
            ret = copy_from_user(auxadc_cali_cal, nvram_data_addr, 4);
            g_AUXADC_Cali = KAL_TRUE; /* enable calibration after setting AUXADC_CALI_Cal */
            if (auxadc_cali_cal[0] == 1) {
                g_AUXADC_Cali = KAL_TRUE;
            } else {
                g_AUXADC_Cali = KAL_FALSE;
            }
		for (i = 0; i < 1; i++)
			printk("auxadc_cali_cal[%d] = %d\n", i, *(auxadc_cali_cal + i));
            printk("**** MT auxadc_cali ioctl : SET_ADC_CALI_Cal Done!\n");
            break;

        case ADC_CHANNEL_READ:
            g_AUXADC_Cali = KAL_FALSE; /* 20100508 Infinity */
            user_data_addr = (long *)arg;
            ret = copy_from_user(auxadc_in_data, user_data_addr, 8); /* 2*int = 2*4 */

	    printk("this api is removed !! \n");
            ret = copy_to_user(user_data_addr, auxadc_out_data, 8);
		printk("**** ioctl : AUXADC Channel %d * %d times = %d\n", auxadc_in_data[0],
		       auxadc_in_data[1], auxadc_out_data[0]);
            break;

        default:
            g_AUXADC_Cali = KAL_FALSE;
            break;
    }

    mutex_unlock(&auxadc_mutex);

    return 0;
}

static int auxadc_cali_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int auxadc_cali_release(struct inode *inode, struct file *file)
{
    return 0;
}

static struct file_operations auxadc_cali_fops = {
    .owner      = THIS_MODULE,
    .unlocked_ioctl  = auxadc_cali_unlocked_ioctl,
    .open       = auxadc_cali_open,
    .release    = auxadc_cali_release,
};

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : AUXADC_Channel_X_Slope/Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
#if ADC_CHANNEL_MAX>0
static ssize_t show_AUXADC_Channel_0_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 0));
    printk("[EM] AUXADC_Channel_0_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_0_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_0_Slope, 0664, show_AUXADC_Channel_0_Slope,
		   store_AUXADC_Channel_0_Slope);
static ssize_t show_AUXADC_Channel_0_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 0));
    printk("[EM] AUXADC_Channel_0_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_0_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_0_Offset, 0664, show_AUXADC_Channel_0_Offset,
		   store_AUXADC_Channel_0_Offset);
#endif


#if ADC_CHANNEL_MAX>1
static ssize_t show_AUXADC_Channel_1_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 1));
    printk("[EM] AUXADC_Channel_1_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_1_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_1_Slope, 0664, show_AUXADC_Channel_1_Slope,
		   store_AUXADC_Channel_1_Slope);
static ssize_t show_AUXADC_Channel_1_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 1));
    printk("[EM] AUXADC_Channel_1_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_1_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_1_Offset, 0664, show_AUXADC_Channel_1_Offset,
		   store_AUXADC_Channel_1_Offset);
#endif


#if ADC_CHANNEL_MAX>2
static ssize_t show_AUXADC_Channel_2_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 2));
    printk("[EM] AUXADC_Channel_2_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_2_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_2_Slope, 0664, show_AUXADC_Channel_2_Slope,
		   store_AUXADC_Channel_2_Slope);
static ssize_t show_AUXADC_Channel_2_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 2));
    printk("[EM] AUXADC_Channel_2_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_2_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_2_Offset, 0664, show_AUXADC_Channel_2_Offset,
		   store_AUXADC_Channel_2_Offset);
#endif


#if ADC_CHANNEL_MAX>3
static ssize_t show_AUXADC_Channel_3_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 3));
    printk("[EM] AUXADC_Channel_3_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_3_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_3_Slope, 0664, show_AUXADC_Channel_3_Slope,
		   store_AUXADC_Channel_3_Slope);
static ssize_t show_AUXADC_Channel_3_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 3));
    printk("[EM] AUXADC_Channel_3_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_3_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_3_Offset, 0664, show_AUXADC_Channel_3_Offset,
		   store_AUXADC_Channel_3_Offset);
#endif


#if ADC_CHANNEL_MAX>4
static ssize_t show_AUXADC_Channel_4_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 4));
    printk("[EM] AUXADC_Channel_4_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_4_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_4_Slope, 0664, show_AUXADC_Channel_4_Slope,
		   store_AUXADC_Channel_4_Slope);
static ssize_t show_AUXADC_Channel_4_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 4));
    printk("[EM] AUXADC_Channel_4_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_4_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_4_Offset, 0664, show_AUXADC_Channel_4_Offset,
		   store_AUXADC_Channel_4_Offset);
#endif


#if ADC_CHANNEL_MAX>5
static ssize_t show_AUXADC_Channel_5_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 5));
    printk("[EM] AUXADC_Channel_5_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_5_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_5_Slope, 0664, show_AUXADC_Channel_5_Slope,
		   store_AUXADC_Channel_5_Slope);
static ssize_t show_AUXADC_Channel_5_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 5));
    printk("[EM] AUXADC_Channel_5_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_5_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_5_Offset, 0664, show_AUXADC_Channel_5_Offset,
		   store_AUXADC_Channel_5_Offset);
#endif


#if ADC_CHANNEL_MAX>6
static ssize_t show_AUXADC_Channel_6_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 6));
    printk("[EM] AUXADC_Channel_6_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_6_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_6_Slope, 0664, show_AUXADC_Channel_6_Slope,
		   store_AUXADC_Channel_6_Slope);
static ssize_t show_AUXADC_Channel_6_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 6));
    printk("[EM] AUXADC_Channel_6_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_6_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_6_Offset, 0664, show_AUXADC_Channel_6_Offset,
		   store_AUXADC_Channel_6_Offset);
#endif


#if ADC_CHANNEL_MAX>7
static ssize_t show_AUXADC_Channel_7_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 7));
    printk("[EM] AUXADC_Channel_7_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_7_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_7_Slope, 0664, show_AUXADC_Channel_7_Slope,
		   store_AUXADC_Channel_7_Slope);
static ssize_t show_AUXADC_Channel_7_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 7));
    printk("[EM] AUXADC_Channel_7_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_7_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_7_Offset, 0664, show_AUXADC_Channel_7_Offset,
		   store_AUXADC_Channel_7_Offset);
#endif


#if ADC_CHANNEL_MAX>8
static ssize_t show_AUXADC_Channel_8_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 8));
    printk("[EM] AUXADC_Channel_8_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_8_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_8_Slope, 0664, show_AUXADC_Channel_8_Slope,
		   store_AUXADC_Channel_8_Slope);
static ssize_t show_AUXADC_Channel_8_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 8));
    printk("[EM] AUXADC_Channel_8_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_8_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_8_Offset, 0664, show_AUXADC_Channel_8_Offset,
		   store_AUXADC_Channel_8_Offset);
#endif


#if ADC_CHANNEL_MAX>9
static ssize_t show_AUXADC_Channel_9_Slope(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 9));
    printk("[EM] AUXADC_Channel_9_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_9_Slope(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_9_Slope, 0664, show_AUXADC_Channel_9_Slope,
		   store_AUXADC_Channel_9_Slope);
static ssize_t show_AUXADC_Channel_9_Offset(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 9));
    printk("[EM] AUXADC_Channel_9_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_9_Offset(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_9_Offset, 0664, show_AUXADC_Channel_9_Offset,
		   store_AUXADC_Channel_9_Offset);
#endif


#if ADC_CHANNEL_MAX>10
static ssize_t show_AUXADC_Channel_10_Slope(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 10));
    printk("[EM] AUXADC_Channel_10_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_10_Slope(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_10_Slope, 0664, show_AUXADC_Channel_10_Slope,
		   store_AUXADC_Channel_10_Slope);
static ssize_t show_AUXADC_Channel_10_Offset(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 10));
    printk("[EM] AUXADC_Channel_10_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_10_Offset(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
	printk("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(AUXADC_Channel_10_Offset, 0664, show_AUXADC_Channel_10_Offset,
		   store_AUXADC_Channel_10_Offset);
#endif


#if ADC_CHANNEL_MAX>11
static ssize_t show_AUXADC_Channel_11_Slope(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 11));
    printk("[EM] AUXADC_Channel_11_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_11_Slope(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_11_Slope, 0664, show_AUXADC_Channel_11_Slope,
		   store_AUXADC_Channel_11_Slope);
static ssize_t show_AUXADC_Channel_11_Offset(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 11));
    printk("[EM] AUXADC_Channel_11_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_11_Offset(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_11_Offset, 0664, show_AUXADC_Channel_11_Offset,
		   store_AUXADC_Channel_11_Offset);
#endif


#if ADC_CHANNEL_MAX>12
static ssize_t show_AUXADC_Channel_12_Slope(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 12));
    printk("[EM] AUXADC_Channel_12_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_12_Slope(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_12_Slope, 0664, show_AUXADC_Channel_12_Slope,
		   store_AUXADC_Channel_12_Slope);
static ssize_t show_AUXADC_Channel_12_Offset(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 12));
    printk("[EM] AUXADC_Channel_12_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_12_Offset(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_12_Offset, 0664, show_AUXADC_Channel_12_Offset,
		   store_AUXADC_Channel_12_Offset);
#endif


#if ADC_CHANNEL_MAX>13
static ssize_t show_AUXADC_Channel_13_Slope(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 13));
    printk("[EM] AUXADC_Channel_13_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_13_Slope(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_13_Slope, 0664, show_AUXADC_Channel_13_Slope,
		   store_AUXADC_Channel_13_Slope);
static ssize_t show_AUXADC_Channel_13_Offset(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 13));
    printk("[EM] AUXADC_Channel_13_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_13_Offset(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_13_Offset, 0664, show_AUXADC_Channel_13_Offset,
		   store_AUXADC_Channel_13_Offset);
#endif


#if ADC_CHANNEL_MAX>14
static ssize_t show_AUXADC_Channel_14_Slope(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 14));
    printk("[EM] AUXADC_Channel_14_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_14_Slope(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_14_Slope, 0664, show_AUXADC_Channel_14_Slope,
		   store_AUXADC_Channel_14_Slope);
static ssize_t show_AUXADC_Channel_14_Offset(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 14));
    printk("[EM] AUXADC_Channel_14_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_14_Offset(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_14_Offset, 0664, show_AUXADC_Channel_14_Offset,
		   store_AUXADC_Channel_14_Offset);
#endif


#if ADC_CHANNEL_MAX>15
static ssize_t show_AUXADC_Channel_15_Slope(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_slop + 15));
    printk("[EM] AUXADC_Channel_15_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_15_Slope(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_15_Slope, 0664, show_AUXADC_Channel_15_Slope,
		   store_AUXADC_Channel_15_Slope);
static ssize_t show_AUXADC_Channel_15_Offset(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
    int ret_value = 1;
    ret_value = (*(auxadc_cali_offset + 15));
    printk("[EM] AUXADC_Channel_15_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_15_Offset(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_15_Offset, 0664, show_AUXADC_Channel_15_Offset,
		   store_AUXADC_Channel_15_Offset);
#endif

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : AUXADC_Channel_Is_Calibration */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_AUXADC_Channel_Is_Calibration(struct device *dev, struct device_attribute *attr,
						  char *buf)
{
    int ret_value = 2;
    ret_value = g_AUXADC_Cali;
    printk("[EM] AUXADC_Channel_Is_Calibration : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_AUXADC_Channel_Is_Calibration(struct device *dev,
						   struct device_attribute *attr, const char *buf,
						   size_t size)
{
    printk("[EM] Not Support Write Function\n");
    return size;
}

static DEVICE_ATTR(AUXADC_Channel_Is_Calibration, 0664, show_AUXADC_Channel_Is_Calibration,
		   store_AUXADC_Channel_Is_Calibration);

static ssize_t show_AUXADC_register(struct device *dev,struct device_attribute *attr, char *buf)
{
    return mt_auxadc_dump_register(buf);
}

static ssize_t store_AUXADC_register(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
    printk("[EM] Not Support store_AUXADC_register\n");
    return size;
}

static DEVICE_ATTR(AUXADC_register, 0664, show_AUXADC_register, store_AUXADC_register);


static ssize_t show_AUXADC_chanel(struct device *dev,struct device_attribute *attr, char *buf)
{
	/* read data */
    int i = 0, data[4] = {0,0,0,0};
    char buf_temp[960];
    int res =0;
	for (i = 0; i < 5; i++) {
		res = IMM_auxadc_GetOneChannelValue(i,data,NULL);
		if (res < 0) {
			   printk("[adc_driver]: get data error\n");
			   break;

		} else {
			printk("[adc_driver]: channel[%d]=%d.%d \n",i,data[0],data[1]);
			sprintf(buf_temp,"channel[%d]=%d.%d \n",i,data[0],data[1]);
			strcat(buf,buf_temp);
		}

    }
    mt_auxadc_dump_register(buf_temp);
    strcat(buf,buf_temp);

    return strlen(buf);
}

static int dbug_thread(void *unused)
{
   int i = 0, data[4] = {0,0,0,0};
   int res =0;
   int rawdata=0;
   int cali_voltage =0;

	while (g_start_debug_thread) {
		for (i = 0; i < ADC_CHANNEL_MAX; i++) {
		res = IMM_auxadc_GetOneChannelValue(i,data,&rawdata);
			if (res < 0) {
			   printk("[adc_driver]: get data error\n");
			   break;

			} else {
		       printk("[adc_driver]: channel[%d]raw =%d\n",i,rawdata);
				printk("[adc_driver]: channel[%d]=%d.%.02d\n", i, data[0],
				       data[1]);

		}
		res = IMM_auxadc_GetOneChannelValue_Cali(i,&cali_voltage );
			if (res < 0) {
			   printk("[adc_driver]: get cali voltage error\n");
			   break;

			} else {
				printk("[adc_driver]: channel[%d] cali_voltage =%d\n", i,
				       cali_voltage);

		}
	  msleep(500);

      }
	  msleep(500);

   }
   return 0;
}


static ssize_t store_AUXADC_channel(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	unsigned int start_flag;
	int error;

		if (sscanf(buf, "%u", &start_flag) != 1) {
			printk("[adc_driver]: Invalid values\n");
			return -EINVAL;
		}

		printk("[adc_driver] start flag =%d \n",start_flag);
		g_start_debug_thread = start_flag;
	if (1 == start_flag) {
		   thread = kthread_run(dbug_thread, 0, "AUXADC");

		if (IS_ERR(thread)) {
			  error = PTR_ERR(thread);
			  printk( "[adc_driver] failed to create kernel thread: %d\n", error);
		   }
		}

    return size;
}

static DEVICE_ATTR(AUXADC_read_channel, 0664, show_AUXADC_chanel, store_AUXADC_channel);

static int mt_auxadc_create_device_attr(struct device *dev)
{
    int ret = 0;
    /* For EM */
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_register)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_read_channel)) != 0)
		goto exit;
#if ADC_CHANNEL_MAX>0
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_0_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_0_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>1
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_1_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_1_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>2
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_2_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_2_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>3
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_3_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_3_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>4
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_4_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_4_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>5
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_5_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_5_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>6
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_6_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_6_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>7
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_7_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_7_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>8
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_8_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_8_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>9
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_9_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_9_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>10
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_10_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_10_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>11
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_11_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_11_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>12
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_12_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_12_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>13
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_13_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_13_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>14
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_14_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_14_Offset)) != 0)
		goto exit;
#endif
#if ADC_CHANNEL_MAX>15
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_15_Slope)) != 0)
		goto exit;
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_15_Offset)) != 0)
		goto exit;
#endif
	if ((ret = device_create_file(dev, &dev_attr_AUXADC_Channel_Is_Calibration)) != 0)
		goto exit;

    return 0;
exit:
    return 1;
}


static int adc_channel_info_init(void)
{
	unsigned int used_channel_counter = 0;
	used_channel_counter = 0;
	#ifdef AUXADC_TEMPERATURE_CHANNEL
	/* ap_domain &= ~(1<<CUST_ADC_MD_CHANNEL); */
    sprintf(g_adc_info[used_channel_counter].channel_name, "ADC_RFTMP");
    g_adc_info[used_channel_counter].channel_number = AUXADC_TEMPERATURE_CHANNEL;
	printk("[ADC] channel_name = %s channel num=%d\n",
	       g_adc_info[used_channel_counter].channel_name,
	       g_adc_info[used_channel_counter].channel_number);
    used_channel_counter++;
	#endif

	#ifdef AUXADC_TEMPERATURE1_CHANNEL
    sprintf(g_adc_info[used_channel_counter].channel_name, "ADC_APTMP");
    g_adc_info[used_channel_counter].channel_number = AUXADC_TEMPERATURE1_CHANNEL;
	printk("[ADC] channel_name = %s channel num=%d\n", g_adc_info[used_channel_counter].channel_name
		,g_adc_info[used_channel_counter].channel_number);
    used_channel_counter++;
	#endif

	#ifdef AUXADC_ADC_FDD_RF_PARAMS_DYNAMIC_CUSTOM_CH_CHANNEL
	sprintf(g_adc_info[used_channel_counter].channel_name, "ADC_FDD_Rf_Params_Dynamic_Custom");
	g_adc_info[used_channel_counter].channel_number =
	    AUXADC_ADC_FDD_RF_PARAMS_DYNAMIC_CUSTOM_CH_CHANNEL;
	printk("[ADC] channel_name = %s channel num=%d\n",
	       g_adc_info[used_channel_counter].channel_name,
	       g_adc_info[used_channel_counter].channel_number);
    used_channel_counter++;
	#endif

	#ifdef AUXADC_HF_MIC_CHANNEL
	sprintf(g_adc_info[used_channel_counter].channel_name, "ADC_MIC");
    g_adc_info[used_channel_counter].channel_number = AUXADC_HF_MIC_CHANNEL;
	printk("[ADC] channel_name = %s channel num=%d\n",
	       g_adc_info[used_channel_counter].channel_name,
	       g_adc_info[used_channel_counter].channel_number);
    used_channel_counter++;
	#endif

	return 0;

}

/* platform_driver API */
static int mt_auxadc_probe(struct platform_device *dev)
{
    int ret = 0;
    struct device *adc_dev = NULL;

    printk("******** MT AUXADC driver probe!! ********\n");
    adc_channel_info_init();

	if (enable_clock(MT_PDN_PERI_AUXADC, "AUXADC"))
		printk("hwEnableClock AUXADC failed.");

    /* Integrate with NVRAM */
    ret = alloc_chrdev_region(&auxadc_cali_devno, 0, 1, AUXADC_CALI_DEVNAME);
    if (ret)
        printk("Error: Can't Get Major number for auxadc_cali\n");

    auxadc_cali_cdev = cdev_alloc();
    auxadc_cali_cdev->owner = THIS_MODULE;
    auxadc_cali_cdev->ops = &auxadc_cali_fops;
    ret = cdev_add(auxadc_cali_cdev, auxadc_cali_devno, 1);
    if(ret)
        printk("auxadc_cali Error: cdev_add\n");

    auxadc_cali_major = MAJOR(auxadc_cali_devno);
    auxadc_cali_class = class_create(THIS_MODULE, AUXADC_CALI_DEVNAME);
    adc_dev = (struct device *)device_create(auxadc_cali_class,
							 NULL, auxadc_cali_devno, NULL,
							 AUXADC_CALI_DEVNAME);
    printk("[MT AUXADC_probe] NVRAM prepare : done !!\n");

    if(mt_auxadc_create_device_attr(adc_dev))
		goto exit;

    g_adc_init_flag =1;
	/* read calibration data from EFUSE */
    mt_auxadc_hal_init();
exit:
    return ret;
}

static int mt_auxadc_remove(struct platform_device *dev)
{
    printk("******** MT auxadc driver remove!! ********\n" );
    return 0;
}

static void mt_auxadc_shutdown(struct platform_device *dev)
{
    printk("******** MT auxadc driver shutdown!! ********\n" );
}

static int mt_auxadc_suspend(struct platform_device *dev, pm_message_t state)
{
	/* printk("******** MT auxadc driver suspend!! ********\n" ); */
	/*
	if(disable_clock(MT_PDN_PERI_AUXADC,"AUXADC"))
        printk("hwEnableClock AUXADC failed.");
	*/
    mt_auxadc_hal_suspend();
    return 0;
}

static int mt_auxadc_resume(struct platform_device *dev)
{
	/* printk("******** MT auxadc driver resume!! ********\n" ); */
	/*
	if(enable_clock(MT_PDN_PERI_AUXADC,"AUXADC"))
	{
	    printk("hwEnableClock AUXADC again!!!.");
	    if(enable_clock(MT_PDN_PERI_AUXADC,"AUXADC"))
	    {printk("hwEnableClock AUXADC failed.");}

	}
	*/
    mt_auxadc_hal_resume();
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_auxadc_of_match[] = {
	{ .compatible = "mediatek,AUXADC", },
	{},
};
#endif

static struct platform_driver mt_auxadc_driver = {
    .probe      = mt_auxadc_probe,
    .remove     = mt_auxadc_remove,
    .shutdown   = mt_auxadc_shutdown,
    #ifdef CONFIG_PM
        .suspend = mt_auxadc_suspend,
        .resume	 = mt_auxadc_resume,
    #endif
    .driver     = {
    	.name       = "mt-auxadc",
		#ifdef CONFIG_OF    
		.of_match_table = mt_auxadc_of_match,
		#endif	
    },
};

static int __init mt_auxadc_init(void)
{
    int ret;

    ret = platform_driver_register(&mt_auxadc_driver);
    if (ret) {
        printk("****[mt_auxadc_driver] Unable to register driver (%d)\n", ret);
        return ret;
    }
    printk("****[mt_auxadc_driver] Initialization : DONE \n");

#ifndef CONFIG_MTK_FPGA	
    if(enable_clock(MT_PDN_PERI_AUXADC,"AUXADC"))
    printk("hwEnableClock AUXADC failed.");
#endif
    return 0;
}

static void __exit mt_auxadc_exit (void)
{
}

module_init(mt_auxadc_init);
module_exit(mt_auxadc_exit);

MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("MTK AUXADC Device Driver");
MODULE_LICENSE("GPL");

