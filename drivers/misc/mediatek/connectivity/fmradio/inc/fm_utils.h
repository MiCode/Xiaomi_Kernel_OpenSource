/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __FM_UTILS_H__
#define __FM_UTILS_H__

#include <linux/version.h>

#include "fm_typedef.h"

/**
* Base structure of fm object
*/
#define FM_NAME_MAX 20
struct fm_object {
	signed char name[FM_NAME_MAX + 1];	/* name of fm object */
	unsigned char type;		/* type of fm object */
	unsigned char flag;		/* flag of fm object */
	signed int ref;
	void *priv;
};

/*
 * FM FIFO
 */
struct fm_fifo {
	struct fm_object obj;
	signed int size;
	signed int in;
	signed int out;
	signed int len;
	signed int item_size;
	signed int (*input)(struct fm_fifo *thiz, void *item);
	signed int (*output)(struct fm_fifo *thiz, void *item);
	bool (*is_full)(struct fm_fifo *thiz);
	bool (*is_empty)(struct fm_fifo *thiz);
	signed int (*get_total_len)(struct fm_fifo *thiz);
	signed int (*get_valid_len)(struct fm_fifo *thiz);
	signed int (*reset)(struct fm_fifo *thiz);
};

extern struct fm_fifo *fm_fifo_init(struct fm_fifo *fifo, void *buf, const signed char *name,
				    signed int item_size, signed int item_num);

extern struct fm_fifo *fm_fifo_create(const signed char *name, signed int item_size, signed int item_num);

extern signed int fm_fifo_release(struct fm_fifo *fifo);

#define FM_FIFO_INPUT(fifop, item)  \
({                                    \
	signed int __ret = (signed int)0;              \
	if (fifop && (fifop)->input) {          \
		__ret = (fifop)->input(fifop, item);    \
	}                               \
	__ret;                          \
})

#define FM_FIFO_OUTPUT(fifop, item)  \
({                                    \
	signed int __ret = (signed int)0;              \
	if (fifop && (fifop)->output) {          \
		__ret = (fifop)->output(fifop, item);    \
	}                               \
	__ret;                          \
})

#define FM_FIFO_IS_FULL(fifop)  \
({                                    \
	bool __ret = false;              \
	if (fifop && (fifop)->is_full) {          \
		__ret = (fifop)->is_full(fifop);    \
	}                               \
	__ret;                          \
})

#define FM_FIFO_IS_EMPTY(fifop)  \
({                                    \
	bool __ret = false;              \
	if (fifop && (fifop)->is_empty) {          \
		__ret = (fifop)->is_empty(fifop);    \
	}                               \
	__ret;                          \
})

#define FM_FIFO_RESET(fifop)  \
({                                    \
	signed int __ret = (signed int)0;              \
	if (fifop && (fifop)->reset) {          \
		__ret = (fifop)->reset(fifop);    \
	}                               \
	__ret;                          \
})

#define FM_FIFO_GET_TOTAL_LEN(fifop)  \
({                                    \
	signed int __ret = (signed int)0;              \
	if (fifop && (fifop)->get_total_len) {          \
		__ret = (fifop)->get_total_len(fifop);    \
	}                               \
	__ret;                          \
})

#define FM_FIFO_GET_VALID_LEN(fifop)  \
({                                    \
	signed int __ret = (signed int)0;              \
	if (fifop && (fifop)->get_valid_len) {          \
		__ret = (fifop)->get_valid_len(fifop);    \
	}                               \
	__ret;                          \
})

/*
 * FM asynchronous information mechanism
 */
struct fm_flag_event {
	signed int ref;
	signed char name[FM_NAME_MAX + 1];
	void *priv;

	unsigned int flag;

	/* flag methods */
	unsigned int (*send)(struct fm_flag_event *thiz, unsigned int mask);
	signed int (*wait)(struct fm_flag_event *thiz, unsigned int mask);
	long (*wait_timeout)(struct fm_flag_event *thiz, unsigned int mask, long timeout);
	unsigned int (*clr)(struct fm_flag_event *thiz, unsigned int mask);
	unsigned int (*get)(struct fm_flag_event *thiz);
	unsigned int (*rst)(struct fm_flag_event *thiz);
};

extern struct fm_flag_event *fm_flag_event_create(const signed char *name);

extern signed int fm_flag_event_get(struct fm_flag_event *thiz);

extern signed int fm_flag_event_put(struct fm_flag_event *thiz);

#define FM_EVENT_SEND(eventp, mask)  \
({                                    \
	unsigned int __ret = (unsigned int)0;              \
	if (eventp && (eventp)->send) {          \
		__ret = (eventp)->send(eventp, mask);    \
	}                               \
	__ret;                          \
})

#define FM_EVENT_WAIT(eventp, mask)  \
({                                    \
	signed int __ret = (signed int)0;              \
	if (eventp && (eventp)->wait) {          \
		__ret = (eventp)->wait(eventp, mask);    \
	}                               \
	__ret;                          \
})

#define FM_EVENT_WAIT_TIMEOUT(eventp, mask, timeout)  \
({                                    \
	long __ret = (long)0;              \
	if (eventp && (eventp)->wait_timeout) {          \
		__ret = (eventp)->wait_timeout(eventp, mask, timeout);    \
	}                               \
	__ret;                          \
})

#define FM_EVENT_GET(eventp)  \
({                                    \
	unsigned int __ret = (unsigned int)0;              \
	if (eventp && (eventp)->get) {          \
		__ret = (eventp)->get(eventp);    \
	}                               \
	__ret;                          \
})

#define FM_EVENT_RESET(eventp)  \
({                                    \
	unsigned int __ret = (unsigned int)0;              \
	if (eventp && (eventp)->rst) {          \
		__ret = (eventp)->rst(eventp);    \
	}                               \
	__ret;                          \
})

#define FM_EVENT_CLR(eventp, mask)  \
({                                    \
	unsigned int __ret = (unsigned int)0;              \
	if (eventp && (eventp)->clr) {          \
		__ret = (eventp)->clr(eventp, mask);    \
	}                               \
	__ret;                          \
})

/*
 * FM lock mechanism
 */
struct fm_lock {
	signed char name[FM_NAME_MAX + 1];
	signed int ref;
	void *priv;

	/* lock methods */
	signed int (*lock)(struct fm_lock *thiz);
	signed int (*trylock)(struct fm_lock *thiz, signed int retryCnt);
	signed int (*unlock)(struct fm_lock *thiz);
};

extern struct fm_lock *fm_lock_create(const signed char *name);

extern signed int fm_lock_get(struct fm_lock *thiz);

extern signed int fm_lock_put(struct fm_lock *thiz);

extern struct fm_lock *fm_spin_lock_create(const signed char *name);

extern signed int fm_spin_lock_get(struct fm_lock *thiz);

extern signed int fm_spin_lock_put(struct fm_lock *thiz);

#define FM_LOCK(a)					\
	({						\
		signed int __ret = (signed int)0;	\
		if (!a) {				\
			__ret = -1;			\
		} else if ((a)->lock) {			\
			__ret = (a)->lock(a);		\
		}					\
		__ret;					\
	})

#define FM_UNLOCK(a)				\
	{					\
		if (a && (a)->unlock) {		\
			(a)->unlock(a);		\
		}				\
	}

/*
 * FM timer mechanism
 */
enum fm_timer_ctrl {
	FM_TIMER_CTRL_GET_TIME = 0,
	FM_TIMER_CTRL_SET_TIME = 1,
	FM_TIMER_CTRL_MAX
};

#define FM_TIMER_FLAG_ACTIVATED (1<<0)

struct fm_timer {
	signed int ref;
	signed char name[FM_NAME_MAX + 1];
	void *priv;		/* platform detail impliment */

	signed int flag;		/* timer active/inactive */
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	void (*timeout_func)(struct timer_list *timer);	/* timeout function */
#else
	void (*timeout_func)(unsigned long data);	/* timeout function */
#endif
	unsigned long data;	/* timeout function's parameter */
	signed long timeout_ms;	/* timeout tick */
	/* Tx parameters */
	unsigned int count;
	unsigned char tx_pwr_ctrl_en;
	unsigned char tx_rtc_ctrl_en;
	unsigned char tx_desense_en;

	/* timer methods */
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	signed int (*init)(struct fm_timer *thiz, void (*timeout) (struct timer_list *timer),
		unsigned long data, signed long time, signed int flag);
#else
	signed int (*init)(struct fm_timer *thiz, void (*timeout) (unsigned long data),
		unsigned long data, signed long time, signed int flag);
#endif
	signed int (*start)(struct fm_timer *thiz);
	signed int (*update)(struct fm_timer *thiz);
	signed int (*stop)(struct fm_timer *thiz);
	signed int (*control)(struct fm_timer *thiz, enum fm_timer_ctrl cmd, void *arg);
};

extern struct fm_timer *fm_timer_create(const signed char *name);

extern signed int fm_timer_get(struct fm_timer *thiz);

extern signed int fm_timer_put(struct fm_timer *thiz);

/*
 * FM work thread mechanism
 */
struct fm_work {
	signed int ref;
	signed char name[FM_NAME_MAX + 1];
	void *priv;

	work_func_t work_func;
	unsigned long data;
	/* work methods */
	signed int (*init)(struct fm_work *thiz, work_func_t work_func, unsigned long data);
};

extern struct fm_work *fm_work_create(const signed char *name);

extern signed int fm_work_get(struct fm_work *thiz);

extern signed int fm_work_put(struct fm_work *thiz);

struct fm_workthread {
	signed int ref;
	signed char name[FM_NAME_MAX + 1];
	void *priv;

	/* workthread methods */
	signed int (*add_work)(struct fm_workthread *thiz, struct fm_work *work);
};

extern struct fm_workthread *fm_workthread_create(const signed char *name);

extern signed int fm_workthread_get(struct fm_workthread *thiz);

extern signed int fm_workthread_put(struct fm_workthread *thiz);

signed int fm_delayms(unsigned int data);

signed int fm_delayus(unsigned int data);

unsigned short fm_get_u16_from_auc(unsigned char *buf);

void fm_set_u16_to_auc(unsigned char *buf, unsigned short val);

unsigned int fm_get_u32_from_auc(unsigned char *buf);

void fm_set_u32_to_auc(unsigned char *buf, unsigned int val);

#endif /* __FM_UTILS_H__ */
