#ifndef _MT_BOARD_TYPE_H
#define _MT_BOARD_TYPE_H

#define GPIO_PHONE_EVB_DETECT (GPIO143|0x80000000)

/* MTK_POWER_EXT_DETECT */
enum mt_board_type {
	MT_BOARD_NONE = 0,
	MT_BOARD_EVB = 1,
	MT_BOARD_PHONE = 2
};

static DEFINE_SPINLOCK(mt_board_lock);

#endif
