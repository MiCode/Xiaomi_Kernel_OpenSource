/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2019-01-29 File created.
 */

#include "fsm_public.h"
#if defined(CONFIG_FSM_FS1815)

#define FS1815_STATUS         0xF000
#define FS1815_BOVDS          0x0000
#define FS1815_OTPDS          0x0200
#define FS1815_OVDS           0x0300
#define FS1815_UVDS           0x0400
#define FS1815_OCDS           0x0500
#define FS1815_OTWDS          0x0800
#define FS1815_BOPS           0x0900
#define FS1815_SPKS           0x0A00
#define FS1815_SPKT           0x0B00

#define FS1815_DEVID          0xF001
#define FS1815_REVID          0xF002

#define FS1815_ANASTAT        0xF003
#define FS1815_DIGSTAT        0xF004

#define FS1815_CHIPINI        0xF00E

#define FS1815_PWRCTRL        0xF010
#define FS1815_PWDN           0x0010
#define FS1815_I2CR           0x0110

#define FS1815_SYSCTRL        0xF011
#define FS1815_OSCEN          0x0011
#define FS1815_VBGEN          0x0111
#define FS1815_ZMEN           0x0211
#define FS1815_ADCEN          0x0311
#define FS1815_BSTPD          0x0611
#define FS1815_AMPEN          0x0711

#define FS1815_SPKCOEF        0xF014

#define FS1815_GAINCTRL       0xF018

#define FS1815_ACCKEY         0xF01F

#define FS1815_BSTCTRL        0xF020
#define FS1815_SSEND          0x0F20
#define FS1815_VOUTSEL        0x4820
#define FS1815_ILIMSEL        0x2420
#define FS1815_BSTMODE        0x1020

#define FS1815_DACCTRL        0xF030

#define FS1815_LNMCTRL        0xF03F

#define FS1815_TSCTRL         0xF04C
#define FS1815_OFFSTA         0x0E4C
#define FS1815_OFF_AUTOEN     0x0D4C
#define FS1815_TSEN           0x034C

#define FS1815_AGC1           0x3059
#define FS1815_AGC1RT         0x3059
#define FS1815_AGC2           0x305A
#define FS1815_AGC3           0x305B
#define FS1815_AGC4           0x305C

#define FS1815_BSGCTRL        0xF0BC

#define FS1815_FW_NAME        "fs1815.fsm"


static int fs1815_i2c_reset(fsm_dev_t *fsm_dev)
{
	uint16_t val;
	int ret;
	int i;

	if (!fsm_dev) {
		return -EINVAL;
	}

	for (i = 0; i < FSM_I2C_RETRY; i++) {
		fsm_reg_write(fsm_dev, REG(FS1815_PWRCTRL), 0x0002); // reset nack
		fsm_reg_read(fsm_dev, REG(FS1815_PWRCTRL), NULL); // dummy read
		fsm_delay_ms(15); // 15ms
		ret = fsm_reg_write(fsm_dev, REG(FS1815_PWRCTRL), 0x0001);
		ret |= fsm_reg_read(fsm_dev, REG(FS1815_CHIPINI), &val);
		if ((val == 0x0003) || (val == 0x0300)) { // init finished
			break;
		}
	}
	if (i == FSM_I2C_RETRY) {
		pr_addr(err, "retry timeout");
		ret = -ETIMEDOUT;
	}

	return ret;
}

int fs1815_reg_init(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	int ret;

	if (fsm_dev == NULL || cfg == NULL) {
		return -EINVAL;
	}

	ret = fs1815_i2c_reset(fsm_dev);
	ret |= fsm_reg_write(fsm_dev, REG(FS1815_SPKCOEF), (fsm_dev->tcoef << 1));
	ret |= fsm_write_reg_tbl(fsm_dev, FSM_SCENE_COMMON);
	ret |= fsm_write_reg_tbl(fsm_dev, cfg->next_scene);
	if (ret) {
		pr_addr(err, "init failed:%d", ret);
		fsm_dev->state.dev_inited = false;
	}

	return ret;
}

int fs1815_start_up(fsm_dev_t *fsm_dev)
{
	int ret;

	if (!fsm_dev) {
		return -EINVAL;
	}
	if (fsm_dev->revid == 0x0011) {
		ret = fsm_reg_write(fsm_dev, REG(FS1815_BSGCTRL), 0x9433);
	}
	// 0x1100EF; all enable
	ret = fsm_reg_write(fsm_dev, REG(FS1815_SYSCTRL), 0x008F);
	// 0x100000; power up
	ret |= fsm_reg_write(fsm_dev, REG(FS1815_PWRCTRL), 0x0000);
	ret |= fsm_config_vol(fsm_dev);
	fsm_delay_ms(10);
	fsm_dev->errcode = ret;

	return ret;
}

int fs1815_shut_down(fsm_dev_t *fsm_dev)
{
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	ret = fsm_reg_write(fsm_dev, REG(FS1815_PWRCTRL), 0x0001);
	ret |= fsm_reg_write(fsm_dev, REG(FS1815_SYSCTRL), 0x0000);
	fsm_delay_ms(10);

	return ret;
}

void fs1815_ops(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();

	if (!fsm_dev || !cfg) {
		return;
	}

	fsm_set_fw_name(FS1815_FW_NAME);
	fsm_dev->dev_ops.reg_init = fs1815_reg_init;
	fsm_dev->dev_ops.start_up = fs1815_start_up;
	fsm_dev->dev_ops.shut_down = fs1815_shut_down;
	cfg->nondsp_mode = true;
	cfg->store_otp = false;
}
#endif
