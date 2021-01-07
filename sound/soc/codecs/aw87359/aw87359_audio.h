#ifndef __AW87359_H__
#define __AW87359_H__

static unsigned char aw87359_dspk_cfg_default[] = {
	0x70, 0x80,
	0x01, 0x00,
	0x01, 0x00,
	0x02, 0x0C,
	0x03, 0x08,
	0x04, 0x05,
	0x05, 0x10,
	0x06, 0x07,
	0x07, 0x4E,
	0x08, 0x06,
	0x09, 0x08,
	0x0A, 0x4A,
	0x61, 0xBB,
	0x62, 0x80,
	0x63, 0x29,
	0x64, 0x58,
	0x65, 0xCD,
	0x66, 0x3C,
	0x67, 0x2F,
	0x68, 0x07,
	0x69, 0xDB,
	0x01, 0x0D,
};
static unsigned char aw87359_drcv_cfg_default[] = {
	0x70, 0x80,
	0x01, 0x00,
	0x01, 0x00,
	0x02, 0x00,
	0x03, 0x08,
	0x04, 0x05,
	0x05, 0x00,
	0x06, 0x0F,
	0x07, 0x4E,
	0x08, 0x09,
	0x09, 0x08,
	0x0A, 0x4B,
	0x61, 0xBB,
	0x62, 0x80,
	0x63, 0x29,
	0x64, 0x58,
	0x65, 0xCD,
	0x66, 0x80,
	0x67, 0x2F,
	0x68, 0x07,
	0x69, 0xDB,
	0x01, 0x0D,
};
static unsigned char aw87359_abspk_cfg_default[] = {
	0x70, 0x80,
	0x01, 0x00,
	0x01, 0x00,
	0x02, 0x05,
	0x03, 0x08,
	0x04, 0x05,
	0x05, 0x10,
	0x06, 0x07,
	0x07, 0x4E,
	0x08, 0x06,
	0x09, 0x08,
	0x0A, 0x4A,
	0x61, 0xBB,
	0x62, 0x80,
	0x63, 0x29,
	0x64, 0x54,
	0x65, 0xCD,
	0x66, 0x80,
	0x67, 0x2F,
	0x68, 0x07,
	0x69, 0xDB,
	0x01, 0x0D,
};
static unsigned char aw87359_abrcv_cfg_default[] = {
	0x70, 0x80,
	0x01, 0x00,
	0x01, 0x00,
	0x02, 0x01,
	0x03, 0x08,
	0x04, 0x05,
	0x05, 0x00,
	0x06, 0x0F,
	0x07, 0x4E,
	0x08, 0x09,
	0x09, 0x08,
	0x0A, 0x4B,
	0x61, 0xBB,
	0x62, 0x80,
	0x63, 0x29,
	0x64, 0x58,
	0x65, 0xCD,
	0x66, 0x80,
	0x67, 0x2F,
	0x68, 0x07,
	0x69, 0xDB,
	0x01, 0x0D,
};


#define AWINIC_CFG_UPDATE_DELAY
#define AW_I2C_RETRIES 5
#define AW_I2C_RETRY_DELAY 2
#define AW_READ_CHIPID_RETRIES 5
#define AW_READ_CHIPID_RETRY_DELAY 2

#define AW87359_CHIPID      0x59

#define REG_CHIPID              0x00
#define REG_SYSCTRL             0x01
#define REG_MDCRTL              0x02
#define REG_CPOVP               0x03
#define REG_CPP                 0x04
#define REG_PAG                 0x05
#define REG_AGC3PO              0x06
#define REG_AGC3PA              0x07
#define REG_AGC2PO              0x08
#define REG_AGC2PA              0x09
#define REG_AGC1PA              0x0A
#define REG_DFT_SYSCTRL         0x61
#define REG_DFT_MDCTRL          0x62
#define REG_DFT_CPOVP2          0x63
#define REG_DFT_AGCPA           0x64
#define REG_DFT_POFR            0x65
#define REG_DFT_OC              0x66
#define REG_DFT_OTA             0x67
#define REG_DFT_REF             0x68
#define REG_DFT_LDO             0x69
#define REG_ENCR                0x70

/* AGC3OPO: reg:0x06 RW */
#define AW87359_BIT_AGC3PO_PD_AGC3_MASK		(~(0x0F<<0))
#define AW87359_BIT_AGC3PO_AGC3_DISABLE		(0x0F<<0)

/* AGC2OPO: reg:0x08 RW */
#define AW87359_BIT_AGC2PO_AGC2_PO_MASK		(~(0xF<<0))
#define AW87359_BIT_AGC2PO_AGC2_DISABLE		(0x9<<0)

/* AGC1PA: reg:0x0A RW */
#define AW87359_BIT_AGC1PA_PD_AGC1_MASK		(~(1<<0))
#define AW87359_BIT_AGC1PA_AGC1_DISABLE		(1<<0)


/********************************************
 * Register Access
 *******************************************/
#define REG_NONE_ACCESS         0
#define REG_RD_ACCESS           (1 << 0)
#define REG_WR_ACCESS           (1 << 1)
#define AW87359_REG_MAX         0xFF

const unsigned char aw87359_reg_access[AW87359_REG_MAX] = {
	[REG_CHIPID] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_SYSCTRL] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_MDCRTL] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_CPOVP] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_CPP] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_PAG] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_AGC3PO] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_AGC3PA] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_AGC2PO] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_AGC2PA] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_AGC1PA] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_DFT_SYSCTRL] = REG_RD_ACCESS,
	[REG_DFT_MDCTRL] = REG_RD_ACCESS,
	[REG_DFT_CPOVP2] = REG_RD_ACCESS,
	[REG_DFT_AGCPA] = REG_RD_ACCESS,
	[REG_DFT_POFR] = REG_RD_ACCESS,
	[REG_DFT_OC] = REG_RD_ACCESS,
	[REG_DFT_OTA] = REG_RD_ACCESS,
	[REG_DFT_REF] = REG_RD_ACCESS,
	[REG_DFT_LDO] = REG_RD_ACCESS,
	[REG_ENCR] = REG_RD_ACCESS,
};

struct aw87359_container {
	int len;
	unsigned char data[];
};


struct aw87359 {
	struct i2c_client *i2c_client;
	bool AGC_bypass_flag;
	unsigned char hwen_flag;
	unsigned char dspk_cfg_update_flag;
	unsigned char drcv_cfg_update_flag;
	unsigned char abspk_cfg_update_flag;
	unsigned char abrcv_cfg_update_flag;
	struct hrtimer cfg_timer;
	struct mutex cfg_lock;
	struct delayed_work ram_work;
};
/******************************************************************
* aw87359 functions
*******************************************************************/
unsigned char aw87359_audio_off(void);
unsigned char aw87359_audio_dspk(void);
unsigned char aw87359_audio_drcv(void);
unsigned char aw87359_audio_abspk(void);
unsigned char aw87359_audio_abrcv(void);
#endif
