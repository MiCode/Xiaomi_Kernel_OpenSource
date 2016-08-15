/*
  * Copyright (C) 2016 XiaoMi, Inc.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 as
  * published by the Free Software Foundation.
  */

#ifndef _FSA8108_H_
#define _FSA8108_H_

struct reg_default;

struct fsa8108_intmask_event {
	int intmask;
	int code;
	int type;
	bool value;
};
struct fsa8108_platform_data {
	struct reg_default *reg_map;
	struct fsa8108_intmask_event *fsa8108_event;
	int reg_map_size;
	int fsa8108_event_size;
	int irq_gpio;
	int reset_gpio;
	const char *supply;
};

enum fsa8108_reg {
	DEVICE_ID_REG = 0x01,
	INTERRUPT_1_REG = 0x02,
	INTERRUPT_2_REG = 0x03,
	INTERRUPT_MASK_1_REG = 0x04,
	INTERRUPT_MASK_2_REG = 0x05,
	GLOBAL_MULTIPLIER_REG = 0x06,
	J_DET_TIMING_REG = 0x07,
	KEY_PRESS_TIMING_REG = 0x08,
	MP3_MODE_TIMING_REG = 0x09,
	DETECTION_TIMING_REG = 0x0A,
	DEBOUNCE_TIMIG_REG = 0x0B,
	CONTROL_REG = 0x0C,
	DET_THRESHOLDS_1_REG = 0x0D,
	DET_THRESHOLDS_2_REG = 0x0E,
	RESET_CONTROL_REG = 0x0F,
};

#define FSA8108_INSERTED_MASK   	(3<<0)
#define FSA8108_3POLE_INSERTED_MASK   	(1<<0)
#define FSA8108_4POLE_INSERTED_MASK   	(1<<1)
#define FSA8108_DISCONNECT_MASK 	(1<<2)

#define FSA8108_SW_MASK         	(1<<3)
#define FSA8108_DSW_MASK		(1<<4)
#define FSA8108_LSW_MASK		(1<<5)

#define FSA8108_VOLUP_MASK      	(1<<(0+8))
#define FSA8108_LVOLUP_PRESS_MASK	(1<<(1+8))
#define FSA8108_LVOLUP_RELEASE_MASK     (1<<(2+8))

#define FSA8108_VOLDOWN_MASK      	(1<<(3+8))
#define FSA8108_LVOLDOWN_PRESS_MASK	(1<<(4+8))
#define FSA8108_LVOLDOWN_RELEASE_MASK   (1<<(5+8))

#define DEFAULT_VALUE_CONTROL_REG 0x48

#define INSERT_3POLE_VALUE_CONTROL_REG 0x4B

#endif /* _FSA8108_H_ */
