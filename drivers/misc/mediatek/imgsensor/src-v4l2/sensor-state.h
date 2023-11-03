#ifndef __SENSOR_STATE_H__
#define __SENSOR_STATE_H__
#include <linux/printk.h>
#define SHOW_LOG 1
/*
 bit  0 sensor power  state --- 1/0 (on/off)
 bit  1 sensor init   state --- 1/0 (inited/uninit)
 bit  2 sensor stream state --- 1/0 (streamon/streamoff)
 bit  3 res mode
 bit  4 res mode
 bit  5 res mode
 bit  6 res mode
 bit  7 res mode --- 00000 ~ 11111 (0--31)
 bit  8 frame num
 bit  9 frame num
 ......
 bit 22 frame num
 bit 23 frame num --- 0x0000 ~ 0xffffff
  */
#define POWER_BIT_MASK     0x000001
#define INIT_BIT_MASK      0x000002
#define STREAM_BIT_MASK    0x000004
#define RES_BIT_MASK       0x0000F8
#define FRAME_BIT_MASK     0xFFFF00
#define POWER_BIT_START    0
#define INIT_BIT_START     1
#define STREAM_BIT_START   2
#define RES_BIT_START      3
#define FRAME_BIT_START    8

/* for get sensor_state */
#define IS_POWERON(sensor_state)\
	((sensor_state & POWER_BIT_MASK) >> POWER_BIT_START)

#define IS_INIT(sensor_state)\
	((sensor_state & INIT_BIT_MASK) >> INIT_BIT_START)

#define IS_STREAMING(sensor_state)\
	((sensor_state & STREAM_BIT_MASK) >> STREAM_BIT_START)

#define WHICH_RES(sensor_state)\
	((sensor_state & RES_BIT_MASK) >> RES_BIT_START)

#define WHICH_FRAME(sensor_state)\
	((sensor_state & FRAME_BIT_MASK) >> FRAME_BIT_START)

/* for set sensor_state */
#if SHOW_LOG
#define SET_POWERON_STATE(sensor_state, state, func, line)\
do{\
	if(state)\
		sensor_state |= POWER_BIT_MASK;\
	else\
		sensor_state &= ~POWER_BIT_MASK;\
	pr_err("[%s Line:%4d]Set sensor_state power: %d\n", func, line, state);\
}while(0)
#else
#define SET_POWERON_STATE(sensor_state, state, func, line)\
do{\
	if(state)\
		sensor_state |= POWER_BIT_MASK;\
	else\
		sensor_state &= ~POWER_BIT_MASK;\
}while(0)
#endif

#if SHOW_LOG
#define SET_INIT_STATE(sensor_state, state, func, line)\
do{\
	if(state)\
		sensor_state |= INIT_BIT_MASK;\
	else\
		sensor_state &= ~INIT_BIT_MASK;\
	pr_err("[%s Line:%4d]Set sensor_state init: %d\n", func, line, state);\
}while(0)
#else
#define SET_INIT_STATE(sensor_state, state, func, line)\
do{\
	if(state)\
		sensor_state |= INIT_BIT_MASK;\
	else\
		sensor_state &= ~INIT_BIT_MASK;\
}while(0)
#endif

#if SHOW_LOG
#define SET_STREAMING_STATE(sensor_state, state, func, line)\
do{\
	if(state)\
		sensor_state |= STREAM_BIT_MASK;\
	else\
		sensor_state &= ~STREAM_BIT_MASK;\
	pr_err("[%s Line:%4d]Set sensor_state stream: %d\n", func, line, state);\
}while(0)
#else
#define SET_STREAMING_STATE(sensor_state, state, func, line)\
do{\
	if(state)\
		sensor_state |= STREAM_BIT_MASK;\
	else\
		sensor_state &= ~STREAM_BIT_MASK;\
}while(0)
#endif

#if SHOW_LOG
#define SET_RES_STATE(sensor_state, res, func, line)\
do{\
	sensor_state &= ~RES_BIT_MASK;\
	sensor_state |= (res << RES_BIT_START);\
	pr_err("[%s Line:%4d]Set sensor_state res: %d\n", func, line, res);\
}while(0)
#else
#define SET_RES_STATE(sensor_state, res, func, line)\
do{\
	sensor_state &= ~RES_BIT_MASK;\
	sensor_state |= (res << RES_BIT_START);\
}while(0)
#endif

#if SHOW_LOG
#define CLEAN_STATE(sensor_state, func, line)\
do{\
	sensor_state = 0;\
	pr_err("[%s Line:%4d]Set sensor_state clean: 0000\n", func, line);\
}while(0)
#else
#define CLEAN_STATE(sensor_state, func, line)\
do{\
	sensor_state = 0;\
}while(0)
#endif

#if 0//SHOW_LOG
#define SET_FRAME_STATE(sensor_state, num, func, line)\
do{\
	sensor_state &= ~FRAME_BIT_MASK;\
	sensor_state |= (num << FRAME_BIT_START);\
	pr_err("[%s Line:%4d]Set sensor_state frame:%5d\n", func, line, num);\
}while(0)
#else
#define SET_FRAME_STATE(sensor_state, num, func, line)\
do{\
	sensor_state &= ~FRAME_BIT_MASK;\
	sensor_state |= (num << FRAME_BIT_START);\
}while(0)
#endif

#if SHOW_LOG
#define SHOW_SENSOE_STATE(sensor_state, func, line)\
do{\
	pr_err("[%s Line:%4d]Show sensor_state: CurFrameNun:%5d CurSensorMode: %2d IsStreaming: %d IsInit: %d IsPowerOn: %d\n", func, line, WHICH_FRAME(sensor_state), WHICH_RES(sensor_state), IS_STREAMING(sensor_state), IS_INIT(sensor_state), IS_POWERON(sensor_state));\
}while(0)
#else
#define SHOW_SENSOE_STATE(sensor_state, func, line)\
do{\
}while(0)
#endif

//SHOW_SENSOE_STATE(g_sensor_state, __func__, __LINE__);
#endif
