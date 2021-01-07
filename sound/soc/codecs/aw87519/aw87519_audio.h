#ifndef __AW87519_H__
#define __AW87519_H__

unsigned char aw87519_kspk_cfg_default[] = {
	0x69, 0x80,
	0x69, 0xB7,
	0x01, 0xF0,
	0x02, 0x09,
	0x03, 0xE8,
	0x04, 0x11,
	0x05, 0x10,
	0x06, 0x43,
	0x07, 0x4E,
	0x08, 0x03,
	0x09, 0x08,
	0x0A, 0x4A,
	0x60, 0x16,
	0x61, 0x20,
	0x62, 0x01,
	0x63, 0x0B,
	0x64, 0xC5,
	0x65, 0xA4,
	0x66, 0x78,
	0x67, 0xC4,
	0x68, 0X90
};

unsigned char aw87519_drcv_cfg_default[] = {
	0x69, 0x80,
	0x69, 0xB7,
	0x01, 0xF8,
	0x02, 0x09,
	0x03, 0xC8,
	0x04, 0x11,
	0x05, 0x05,
	0x06, 0x53,
	0x07, 0x4E,
	0x08, 0x0B,
	0x09, 0x08,
	0x0A, 0x4B,
	0x60, 0x16,
	0x61, 0x20,
	0x62, 0x01,
	0x63, 0x0B,
	0x64, 0xC5,
	0x65, 0xA4,
	0x66, 0x78,
	0x67, 0xC4,
	0x68, 0X90
};

unsigned char aw87519_hvload_cfg_default[] = {
	0x69, 0x80,
	0x69, 0xB7,
	0x01, 0xF0,
	0x02, 0x09,
	0x03, 0xE8,
	0x04, 0x11,
	0x05, 0x10,
	0x06, 0x43,
	0x07, 0x4E,
	0x08, 0x03,
	0x09, 0x08,
	0x0A, 0x4A,
	0x60, 0x66,
	0x61, 0xA0,
	0x62, 0x01,
	0x63, 0x0B,
	0x64, 0xD5,
	0x65, 0xA4,
	0x66, 0x78,
	0x67, 0xC4,
	0x68, 0X90
};

/******************************************************
 *
 *Load config function
 *
 *****************************************************/
#define AWINIC_CFG_UPDATE_DELAY
#define AW_I2C_RETRIES			5
#define AW_I2C_RETRY_DELAY		2
#define AW_READ_CHIPID_RETRIES		5
#define AW_READ_CHIPID_RETRY_DELAY	2

#define AW87519_REG_CHIPID		0x00
#define AW87519_REG_SYSCTRL		0x01
#define AW87519_REG_BATSAFE		0x02
#define AW87519_REG_BSTOVR		0x03
#define AW87519_REG_BSTVPR		0x04
#define AW87519_REG_PAGR		0x05
#define AW87519_REG_PAGC3OPR		0x06
#define AW87519_REG_PAGC3PR		0x07
#define AW87519_REG_PAGC2OPR		0x08
#define AW87519_REG_PAGC2PR		0x09
#define AW87519_REG_PAGC1PR		0x0A


/* PAGC3OPR: reg:0x06 RW */
#define AW87519_BIT_PAGC3OPR_PD_AGC3_MASK		(~(1<<4))
#define AW87519_BIT_PAGC3OPR_AGC3_ENABLE		(0<<4)
#define AW87519_BIT_PAGC3OPR_AGC3_DISABLE		(1<<4)

/* PAGC2OPR: reg:0x08 RW */
#define AW87519_BIT_PAGC2OPR_AGC2_PO_MASK		(~(0xF<<0))
#define AW87519_BIT_PAGC2OPR_AGC2_DISABLE		(0xB<<0)

/* PAGC1PR: reg:0x0A RW */
#define AW87519_BIT_PAGC1PR_PD_AGC1_MASK		(~(1<<0))
#define AW87519_BIT_PAGC1PR_AGC1_ENABLE			(0<<0)
#define AW87519_BIT_PAGC1PR_AGC1_DISABLE		(1<<0)

#define AW87519_CHIPID			0x59
#define AW87519_REG_MAX			11

#define REG_NONE_ACCESS			0
#define REG_RD_ACCESS			(1 << 0)
#define REG_WR_ACCESS			(1 << 1)

const unsigned char aw87349_reg_access[AW87519_REG_MAX] = {
	[AW87519_REG_CHIPID] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW87519_REG_SYSCTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW87519_REG_BATSAFE] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW87519_REG_BSTOVR] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW87519_REG_BSTVPR] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW87519_REG_PAGR] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW87519_REG_PAGC3OPR] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW87519_REG_PAGC3PR] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW87519_REG_PAGC2OPR] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW87519_REG_PAGC2PR] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW87519_REG_PAGC1PR] = REG_RD_ACCESS | REG_WR_ACCESS,

};

struct aw87519_container {
	int len;
	unsigned char data[];
};

struct aw87519 {
	struct i2c_client *i2c_client;
	int reset_gpio;
	bool AGC_bypass_flag;
	unsigned char hwen_flag;
	unsigned char kspk_cfg_update_flag;
	unsigned char drcv_cfg_update_flag;
	unsigned char hvload_cfg_update_flag;
	struct hrtimer cfg_timer;
	struct mutex cfg_lock;
	struct work_struct cfg_work;
	struct delayed_work ram_work;
};

/*******************************************************************************
* aw87519 functions
******************************************************************************/
unsigned char aw87519_audio_drcv(void);
unsigned char aw87519_audio_kspk(void);
unsigned char aw87519_audio_hvload(void);
unsigned char aw87519_audio_off(void);
#endif
