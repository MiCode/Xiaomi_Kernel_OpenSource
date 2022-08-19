#ifndef _AIROHA_GPS_DRIVER_H
#define _AIROHA_GPS_DRIVER_H


#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/mod_devicetable.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/types.h>
/** This macro will make msg readable */
#define AIROHA_STRING_INFO


static int airoha_gps_open(struct inode *inode, struct file *file_p);
static ssize_t airoha_gps_read(struct file *file_p, char __user *user, size_t len, loff_t *offset);
static ssize_t  airoha_gps_write(struct file *file_p, const char __user *user, size_t len, loff_t *offset);

//Common API
static int gps_chip_enable(bool enable);

//END of API


typedef struct {
	int msg_number;
	#ifdef AIROHA_STRING_INFO
	char msg_buffer[20];
	#endif
} airoha_gps_msg;

typedef airoha_gps_msg airoha_gps_msg_t;
/* gps chip event */
#define GPS_EVENT_GPS_DATA_IN 0xFF00
#define GPS_EVENT_TEST_RESPONSE    0xFFA0
/* gps chip event end */

/* gps action */
#define GPS_ACTION_CHIP_ENABLE     0xAA00
#define GPS_ACTION_CHIP_DISABLE    0xAA01
#define GPS_ACTION_WAKEUP_CHIP     0xAA02
#define GPS_ACTION_TEST            0xA0A0



#endif
