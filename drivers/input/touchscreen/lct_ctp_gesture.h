
#ifndef __LCT_CTP_GESTURE_H__
#define __LCT_CTP_GESTURE_H__

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/string.h>
#include <asm/unistd.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/input.h>

#define GESTURE_MODE_DOUBLE_TAP	1
#define GESTURE_MODE_SWIPE		2
#define GESTURE_MODE_UNICODE	3


#define GESTURE_ERROR	0x00
/* Double tap */
#define DOUBLE_TAP		0xA0
/* Swipe */
#define SWIPE_X_LEFT	0xB0
#define SWIPE_X_RIGHT	0xB1
#define SWIPE_Y_UP		0xB2
#define SWIPE_Y_DOWN	0xB3
/* Unicode */
#define UNICODE_E		0xC0
#define UNICODE_C		0xC1
#define UNICODE_W		0xC2
#define UNICODE_M		0xC3
#define UNICODE_O		0xC4
#define UNICODE_S		0xC5
/* Latter V */
#define UNICODE_V_UP	0xC6
#define UNICODE_V_DOWN	0xC7
#define UNICODE_V_L		0xC8
#define UNICODE_V_R		0xC9

#define UNICODE_Z		0xCA



#define BIT_SWIPE_RIGHT	(1<<0)
#define BIT_SWIPE_LEFT	(1<<1)
#define BIT_SWIPE_DOWN	(1<<2)
#define BIT_SWIPE_UP	(1<<3)

#define BIT_DOUBLE_TAP	(1<<8)

#define BIT_UNICODE_E	(1<<16)
#define BIT_UNICODE_C	(1<<17)
#define BIT_UNICODE_W	(1<<18)
#define BIT_UNICODE_M	(1<<19)
#define BIT_UNICODE_O	(1<<20)
#define BIT_UNICODE_S	(1<<21)
#define BIT_UNICODE_V	(1<<22)
#define BIT_UNICODE_Z	(1<<23)

#define BIT_STATUS_OFF	(0<<0)
#define BIT_STATUS_ALL	(1<<24)
#define BIT_STATUS_PART	(1<<28)


#define BIT_GESTURE_ALL	( \
	BIT_SWIPE_RIGHT|BIT_SWIPE_LEFT|BIT_SWIPE_DOWN|BIT_SWIPE_UP | \
	BIT_DOUBLE_TAP | \
	BIT_UNICODE_E|BIT_UNICODE_C|BIT_UNICODE_W|BIT_UNICODE_M | \
	BIT_UNICODE_O|BIT_UNICODE_S|BIT_UNICODE_V|BIT_UNICODE_Z)


#define CONTROL_ALL		0x00010000
#define CONTROL_TAP		0x00020000
#define CONTROL_UNICODE	0x00030000
#define CONTROL_SWIPE	0x00040000
#define CONTROL_AUTOTEST_ENTER	0xBFC3BFC3
#define CONTROL_AUTOTEST_EXIT	0x30406030

#define CONTROL_ALL_ENABLE				0x01
#define CONTROL_TAP_ENABLE				0x01
#define CONTROL_SWIPE_RIGHT_ENABLE		0x01
#define CONTROL_SWIPE_LEFT_ENABLE		0x02
#define CONTROL_SWIPE_DOWN_ENABLE		0x04
#define CONTROL_SWIPE_UP_ENABLE			0x08
#define CONTROL_UNICODE_V_ENABLE		0x01
#define CONTROL_UNICODE_C_ENABLE		0x02
#define CONTROL_UNICODE_E_ENABLE		0x04
#define CONTROL_UNICODE_W_ENABLE		0x08
#define CONTROL_UNICODE_M_ENABLE		0x10
#define CONTROL_UNICODE_S_ENABLE		0x20
#define CONTROL_UNICODE_Z_ENABLE		0x40
#define CONTROL_UNICODE_O_ENABLE		0x80

#define CONTROL_SWIPE_ALL			\
	(CONTROL_SWIPE_RIGHT_ENABLE |	\
	CONTROL_SWIPE_LEFT_ENABLE |		\
	CONTROL_SWIPE_DOWN_ENABLE |		\
	CONTROL_SWIPE_UP_ENABLE)

#define CONTROL_UNICODE_ALL			\
	(CONTROL_UNICODE_V_ENABLE |		\
	CONTROL_UNICODE_C_ENABLE |		\
	CONTROL_UNICODE_E_ENABLE |		\
	CONTROL_UNICODE_W_ENABLE |		\
	CONTROL_UNICODE_M_ENABLE |		\
	CONTROL_UNICODE_S_ENABLE |		\
	CONTROL_UNICODE_Z_ENABLE |		\
	CONTROL_UNICODE_O_ENABLE)

struct ctp_gesture_device {
	struct device gesture_dev;
	u32 current_control_value;
	u32 current_gesture_val;
	u32 control_state;
	u32 control_state_bakup;
	u32	autotest_mode;
	struct mutex gesture_lock;
	struct input_dev *input_device;
#if defined(CONFIG_TOUCHSCREEN_COVER)
	struct mutex cover_lock;
	u32 cover_state;
#endif
};

typedef int (*Func_Ctp_Gesture_Mode)(int);


int ctp_get_gesture_data(void);
void ctp_set_gesture_data(int value);
struct input_dev *ctp_gesture_get_input_device(void);
void ctp_gesture_set_input_device(struct input_dev *input);
int ctp_get_gesture_control(void);
bool ctp_check_gesture_needed(u8 report_data);
void ctp_gesture_switch_init(Func_Ctp_Gesture_Mode gesture_func);

#if defined(CONFIG_TOUCHSCREEN_COVER)
typedef int (*Func_Ctp_Cover_State)(int);

void ctp_cover_switch_init(Func_Ctp_Cover_State cover_func);
#endif

#endif

