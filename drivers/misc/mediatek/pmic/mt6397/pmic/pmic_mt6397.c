// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/kernel.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6397/registers.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/version.h>

#include <mt-plat/upmu_common.h>

#define E_PWR_INVALID_DATA 33

struct regmap *pwrap_regmap;

unsigned int pmic_read_interface(unsigned int RegNum, unsigned int *val,
				 unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;

	return_value = regmap_read(pwrap_regmap, RegNum, &pmic_reg);
	if (return_value != 0) {
		pr_notice("[Power/PMIC][%s] Reg[%x]= pmic_wrap read data fail\n",
		       __func__, RegNum);
		return return_value;
	}

	pmic_reg &= (MASK << SHIFT);
	*val = (pmic_reg >> SHIFT);

	return return_value;
}

unsigned int pmic_config_interface(unsigned int RegNum, unsigned int val,
				   unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;

	if (val > MASK) {
		pr_notice("[Power/PMIC][%s] Invalid data, Reg[%x]: MASK = 0x%x, val = 0x%x\n",
		       __func__, RegNum, MASK, val);
		return E_PWR_INVALID_DATA;
	}

	return_value = regmap_read(pwrap_regmap, RegNum, &pmic_reg);
	if (return_value != 0) {
		pr_notice("[Power/PMIC][%s] Reg[%x]= pmic_wrap read data fail\n",
		       __func__, RegNum);
		return return_value;
	}

	pmic_reg &= ~(MASK << SHIFT);
	pmic_reg |= (val << SHIFT);

	return_value = regmap_write(pwrap_regmap, RegNum, pmic_reg);
	if (return_value != 0) {
		pr_notice("[Power/PMIC][%s] Reg[%x]= pmic_wrap read data fail\n",
		       __func__, RegNum);
		return return_value;
	}

	return return_value;
}

u32 upmu_get_reg_value(u32 reg)
{
	u32 reg_val = 0;

	pmic_read_interface(reg, &reg_val, 0xFFFF, 0x0);

	return reg_val;
}
EXPORT_SYMBOL(upmu_get_reg_value);

void upmu_set_reg_value(u32 reg, u32 reg_val)
{
	pmic_config_interface(reg, reg_val, 0xFFFF, 0x0);
}

u32 g_reg_value;
static ssize_t pmic_access_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	pr_notice("[%s] 0x%x\n", __func__, g_reg_value);
	return sprintf(buf, "%04X\n", g_reg_value);
}

static ssize_t pmic_access_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	int ret = 0;
	char temp_buf[32];
	char *pvalue;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	strncpy(temp_buf, buf, sizeof(temp_buf));
	temp_buf[sizeof(temp_buf) - 1] = 0;
	pvalue = temp_buf;

	if (size != 0) {
		if (size > 5) {
			ret = kstrtouint(strsep(&pvalue, " "), 16,
					 &reg_address);
			if (ret)
				return ret;
			ret = kstrtouint(pvalue, 16, &reg_value);
			if (ret)
				return ret;
			pr_notice(
				"[%s] write PMU reg 0x%x with value 0x%x !\n",
				__func__, reg_address, reg_value);
			ret = pmic_config_interface(reg_address, reg_value,
						    0xFFFF, 0x0);
		} else {
			ret = kstrtouint(pvalue, 16, &reg_address);
			if (ret)
				return ret;
			ret = pmic_read_interface(reg_address, &g_reg_value,
						  0xFFFF, 0x0);
			pr_notice(
				"[%s] read PMU reg 0x%x with value 0x%x !\n",
				__func__, reg_address, g_reg_value);
			pr_notice(
				"Please use \"cat pmic_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR_RW(pmic_access);

void PMIC_INIT_SETTING_V1(void)
{
	unsigned int chip_version = 0;
	unsigned int ret = 0;

	/* [0:0]: RG_VCDT_HV_EN; Disable HV. Only compare LV threshold. */
	ret = pmic_config_interface(0x0, 0x0, 0x1, 0);

	/* put init setting from DE/SA */
	chip_version = upmu_get_cid();

	switch (chip_version & 0xFF) {
	case 0x91:
		/* [7:4]: RG_VCDT_HV_VTH; 7V OVP */
		ret = pmic_config_interface(0x2, 0xC, 0xF, 4);
		/* [11:10]: QI_VCORE_VSLEEP; sleep mode only (0.7V) */
		ret = pmic_config_interface(0x210, 0x0, 0x3, 10);
		break;
	case 0x97:
		/* [7:4]: RG_VCDT_HV_VTH; 7V OVP */
		ret = pmic_config_interface(0x2, 0xB, 0xF, 4);
		/* [11:10]: QI_VCORE_VSLEEP; sleep mode only (0.7V) */
		ret = pmic_config_interface(0x210, 0x1, 0x3, 10);
		break;
	default:
		pr_notice("[Power/PMIC] Error chip ID %d\r\n", chip_version);
		break;
	}

	ret = pmic_config_interface(
		0xC, 0x1, 0x7, 1); /* [3:1]: RG_VBAT_OV_VTH; VBAT_OV=4.3V */
	ret = pmic_config_interface(0x24, 0x1, 0x1,
				    1); /* [1:1]: RG_BC11_RST; */
	ret = pmic_config_interface(
		0x2A, 0x0, 0x7,
		4); /* [6:4]: RG_CSDAC_STP; align to 6250's setting */
	ret = pmic_config_interface(0x2E, 0x1, 0x1,
				    7); /* [7:7]: RG_ULC_DET_EN; */
	ret = pmic_config_interface(0x2E, 0x1, 0x1, 6); /* [6:6]: RG_HWCV_EN; */
	ret = pmic_config_interface(0x2E, 0x1, 0x1,
				    2); /* [2:2]: RG_CSDAC_MODE; */
	ret = pmic_config_interface(
		0x102, 0x0, 0x1,
		3); /* [3:3]: RG_PWMOC_CK_PDN; For OC protection */
	ret = pmic_config_interface(0x128, 0x1, 0x1,
				    9); /* [9:9]: RG_SRCVOLT_HW_AUTO_EN; */
	ret = pmic_config_interface(0x128, 0x1, 0x1,
				    8); /* [8:8]: RG_OSC_SEL_AUTO; */
	ret = pmic_config_interface(
		0x128, 0x1, 0x1, 6); /* [6:6]: RG_SMPS_DIV2_SRC_AUTOFF_DIS; */
	ret = pmic_config_interface(0x128, 0x1, 0x1,
				    5); /* [5:5]: RG_SMPS_AUTOFF_DIS; */
	ret = pmic_config_interface(0x130, 0x1, 0x1,
				    7); /* [7:7]: VDRM_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1,
				    6); /* [6:6]: VSRMCA7_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1,
				    5); /* [5:5]: VPCA7_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1,
				    4); /* [4:4]: VIO18_DEG_EN; */
	ret = pmic_config_interface(
		0x130, 0x1, 0x1, 3); /* [3:3]: VGPU_DEG_EN; For OC protection */
	ret = pmic_config_interface(0x130, 0x1, 0x1,
				    2); /* [2:2]: VCORE_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1,
				    1); /* [1:1]: VSRMCA15_DEG_EN; */
	ret = pmic_config_interface(0x130, 0x1, 0x1,
				    0); /* [0:0]: VCA15_DEG_EN; */
	ret = pmic_config_interface(
		0x206, 0x600, 0x0FFF,
		0); /* [12:0]: BUCK_RSV; for OC protection */
	/* [7:6]: QI_VSRMCA7_VSLEEP; sleep mode only (0.85V) */
	ret = pmic_config_interface(0x210, 0x0, 0x3, 6);
	/* [5:4]: QI_VSRMCA15_VSLEEP; sleep mode only (0.7V) */
	ret = pmic_config_interface(0x210, 0x1, 0x3, 4);
	/* [3:2]: QI_VPCA7_VSLEEP; sleep mode only (0.85V) */
	ret = pmic_config_interface(0x210, 0x0, 0x3, 2);
	/* [1:0]: QI_VCA15_VSLEEP; sleep mode only (0.7V) */
	ret = pmic_config_interface(0x210, 0x1, 0x3, 0);
	/* [13:12]: RG_VCA15_CSL2; for OC protection */
	ret = pmic_config_interface(0x216, 0x0, 0x3, 12);
	/* [11:10]: RG_VCA15_CSL1; for OC protection */
	ret = pmic_config_interface(0x216, 0x0, 0x3, 10);
	/* [15:15]: VCA15_SFCHG_REN; soft change rising enable */
	ret = pmic_config_interface(0x224, 0x1, 0x1, 15);
	/* [14:8]: VCA15_SFCHG_RRATE; soft change rising step=0.5us */
	ret = pmic_config_interface(0x224, 0x5, 0x7F, 8);
	/* [7:7]: VCA15_SFCHG_FEN; soft change falling enable */
	ret = pmic_config_interface(0x224, 0x1, 0x1, 7);
	/* [6:0]: VCA15_SFCHG_FRATE; soft change falling step=2us */
	ret = pmic_config_interface(0x224, 0x17, 0x7F, 0);
	/* [6:0]: VCA15_VOSEL_SLEEP; sleep mode only (0.7V) */
	ret = pmic_config_interface(0x22A, 0x0, 0x7F, 0);
	/* [8:8]: VCA15_VSLEEP_EN; set sleep mode reference voltage from R2R to
	 * V2V
	 */
	ret = pmic_config_interface(0x238, 0x1, 0x1, 8);
	/* [5:4]: VCA15_VOSEL_TRANS_EN; rising & falling enable */
	ret = pmic_config_interface(0x238, 0x3, 0x3, 4);
	ret = pmic_config_interface(0x244, 0x1, 0x1,
				    5); /* [5:5]: VSRMCA15_TRACK_SLEEP_CTRL; */
	ret = pmic_config_interface(0x246, 0x0, 0x3,
				    4); /* [5:4]: VSRMCA15_VOSEL_SEL; */
	ret = pmic_config_interface(0x24A, 0x1, 0x1,
				    15); /* [15:15]: VSRMCA15_SFCHG_REN; */
	ret = pmic_config_interface(0x24A, 0x5, 0x7F,
				    8); /* [14:8]: VSRMCA15_SFCHG_RRATE; */
	ret = pmic_config_interface(0x24A, 0x1, 0x1,
				    7); /* [7:7]: VSRMCA15_SFCHG_FEN; */
	ret = pmic_config_interface(0x24A, 0x17, 0x7F,
				    0); /* [6:0]: VSRMCA15_SFCHG_FRATE; */
	/* [6:0]: VSRMCA15_VOSEL_SLEEP; Sleep mode setting only (0.7V) */
	ret = pmic_config_interface(0x250, 0x00, 0x7F, 0);
	/* [8:8]: VSRMCA15_VSLEEP_EN; set sleep mode reference voltage from R2R
	 * to V2V
	 */
	ret = pmic_config_interface(0x25E, 0x1, 0x1, 8);
	/* [5:4]: VSRMCA15_VOSEL_TRANS_EN; rising & falling enable */
	ret = pmic_config_interface(0x25E, 0x3, 0x3, 4);
	/* [1:1]: VCORE_VOSEL_CTRL; sleep mode voltage control follow SRCLKEN */
	ret = pmic_config_interface(0x270, 0x1, 0x1, 1);
	ret = pmic_config_interface(0x272, 0x0, 0x3,
				    4); /* [5:4]: VCORE_VOSEL_SEL; */
	ret = pmic_config_interface(0x276, 0x1, 0x1,
				    15); /* [15:15]: VCORE_SFCHG_REN; */
	ret = pmic_config_interface(0x276, 0x5, 0x7F,
				    8); /* [14:8]: VCORE_SFCHG_RRATE; */
	ret = pmic_config_interface(0x276, 0x17, 0x7F,
				    0); /* [6:0]: VCORE_SFCHG_FRATE; */
	/* [6:0]: VCORE_VOSEL_SLEEP; Sleep mode setting only (0.7V) */
	ret = pmic_config_interface(0x27C, 0x0, 0x7F, 0);
	/* [8:8]: VCORE_VSLEEP_EN; Sleep mode HW control  R2R to VtoV */
	ret = pmic_config_interface(0x28A, 0x1, 0x1, 8);
	/* [5:4]: VCORE_VOSEL_TRANS_EN; Follows MT6320 VCORE setting. */
	ret = pmic_config_interface(0x28A, 0x0, 0x3, 4);
	ret = pmic_config_interface(0x28A, 0x3, 0x3,
				    0); /* [1:0]: VCORE_TRANSTD; */
	ret = pmic_config_interface(
		0x28E, 0x1, 0x3, 8); /* [9:8]: RG_VGPU_CSL; for OC protection */
	ret = pmic_config_interface(0x29C, 0x1, 0x1,
				    15); /* [15:15]: VGPU_SFCHG_REN; */
	ret = pmic_config_interface(0x29C, 0x5, 0x7F,
				    8); /* [14:8]: VGPU_SFCHG_RRATE; */
	ret = pmic_config_interface(0x29C, 0x17, 0x7F,
				    0); /* [6:0]: VGPU_SFCHG_FRATE; */
	ret = pmic_config_interface(0x2B0, 0x0, 0x3,
				    4); /* [5:4]: VGPU_VOSEL_TRANS_EN; */
	ret = pmic_config_interface(0x2B0, 0x3, 0x3,
				    0); /* [1:0]: VGPU_TRANSTD; */
	ret = pmic_config_interface(0x332, 0x0, 0x3,
				    4); /* [5:4]: VPCA7_VOSEL_SEL; */
	ret = pmic_config_interface(0x336, 0x1, 0x1,
				    15); /* [15:15]: VPCA7_SFCHG_REN; */
	ret = pmic_config_interface(0x336, 0x5, 0x7F,
				    8); /* [14:8]: VPCA7_SFCHG_RRATE; */
	ret = pmic_config_interface(0x336, 0x1, 0x1,
				    7); /* [7:7]: VPCA7_SFCHG_FEN; */
	ret = pmic_config_interface(0x336, 0x17, 0x7F,
				    0); /* [6:0]: VPCA7_SFCHG_FRATE; */
	ret = pmic_config_interface(0x33C, 0x18, 0x7F,
				    0); /* [6:0]: VPCA7_VOSEL_SLEEP; */
	ret = pmic_config_interface(0x34A, 0x1, 0x1,
				    8); /* [8:8]: VPCA7_VSLEEP_EN; */
	ret = pmic_config_interface(0x34A, 0x3, 0x3,
				    4); /* [5:4]: VPCA7_VOSEL_TRANS_EN; */
	ret = pmic_config_interface(0x356, 0x0, 0x1,
				    5); /* [5:5]: VSRMCA7_TRACK_SLEEP_CTRL; */
	ret = pmic_config_interface(0x358, 0x0, 0x3,
				    4); /* [5:4]: VSRMCA7_VOSEL_SEL; */
	ret = pmic_config_interface(0x35C, 0x1, 0x1,
				    15); /* [15:15]: VSRMCA7_SFCHG_REN; */
	ret = pmic_config_interface(0x35C, 0x5, 0x7F,
				    8); /* [14:8]: VSRMCA7_SFCHG_RRATE; */
	ret = pmic_config_interface(0x35C, 0x1, 0x1,
				    7); /* [7:7]: VSRMCA7_SFCHG_FEN; */
	ret = pmic_config_interface(0x35C, 0x17, 0x7F,
				    0); /* [6:0]: VSRMCA7_SFCHG_FRATE; */
	ret = pmic_config_interface(0x362, 0x18, 0x7F,
				    0); /* [6:0]: VSRMCA7_VOSEL_SLEEP; */
	ret = pmic_config_interface(0x370, 0x1, 0x1,
				    8); /* [8:8]: VSRMCA7_VSLEEP_EN; */
	ret = pmic_config_interface(0x370, 0x3, 0x3,
				    4); /* [5:4]: VSRMCA7_VOSEL_TRANS_EN; */
	ret = pmic_config_interface(0x39C, 0x1, 0x1,
				    8); /* [8:8]: VDRM_VSLEEP_EN; */
	ret = pmic_config_interface(0x440, 0x1, 0x1,
				    2); /* [2:2]: VIBR_THER_SHEN_EN; */
	ret = pmic_config_interface(0x500, 0x1, 0x1,
				    5); /* [5:5]: THR_HWPDN_EN; */
	ret = pmic_config_interface(0x502, 0x1, 0x1,
				    3); /* [3:3]: RG_RST_DRVSEL; */
	ret = pmic_config_interface(0x502, 0x1, 0x1,
				    2); /* [2:2]: RG_EN_DRVSEL; */
	ret = pmic_config_interface(0x508, 0x1, 0x1,
				    1); /* [1:1]: PWRBB_DEB_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1,
				    12); /* [12:12]: VSRMCA15_PG_H2L_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1,
				    11); /* [11:11]: VPCA15_PG_H2L_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1,
				    10); /* [10:10]: VCORE_PG_H2L_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1,
				    9); /* [9:9]: VSRMCA7_PG_H2L_EN; */
	ret = pmic_config_interface(0x50C, 0x1, 0x1,
				    8); /* [8:8]: VPCA7_PG_H2L_EN; */
	ret = pmic_config_interface(0x512, 0x1, 0x1,
				    1); /* [1:1]: STRUP_PWROFF_PREOFF_EN; */
	ret = pmic_config_interface(0x512, 0x1, 0x1,
				    0); /* [0:0]: STRUP_PWROFF_SEQ_EN; */
	ret = pmic_config_interface(0x55E, 0xFC, 0xFF,
				    8); /* [15:8]: RG_ADC_TRIM_CH_SEL; */
	ret = pmic_config_interface(0x560, 0x1, 0x1,
				    1); /* [1:1]: FLASH_THER_SHDN_EN; */
	ret = pmic_config_interface(0x566, 0x1, 0x1,
				    1); /* [1:1]: KPLED_THER_SHDN_EN; */
	ret = pmic_config_interface(0x600, 0x1, 0x1,
				    9); /* [9:9]: SPK_THER_SHDN_L_EN; */
	ret = pmic_config_interface(0x604, 0x1, 0x1,
				    0); /* [0:0]: RG_SPK_INTG_RST_L; */
	ret = pmic_config_interface(0x606, 0x1, 0x1,
				    9); /* [9:9]: SPK_THER_SHDN_R_EN; */
	ret = pmic_config_interface(0x60A, 0x1, 0xF,
				    11); /* [14:11]: RG_SPKPGA_GAINR; */
	ret = pmic_config_interface(0x612, 0x1, 0xF,
				    8); /* [11:8]: RG_SPKPGA_GAINL; */
	ret = pmic_config_interface(0x632, 0x1, 0x1, 8); /* [8:8]: FG_SLP_EN; */
	ret = pmic_config_interface(0x638, 0xFFC2, 0xFFFF,
				    0); /* [15:0]: FG_SLP_CUR_TH; */
	ret = pmic_config_interface(0x63A, 0x14, 0xFF,
				    0); /* [7:0]: FG_SLP_TIME; */
	ret = pmic_config_interface(0x63C, 0xFF, 0xFF,
				    8); /* [15:8]: FG_DET_TIME; */
	ret = pmic_config_interface(
		0x714, 0x1, 0x1,
		7); /* [7:7]: RG_LCLDO_ENC_REMOTE_SENSE_VA28; */
	ret = pmic_config_interface(0x714, 0x1, 0x1,
				    4); /* [4:4]: RG_LCLDO_REMOTE_SENSE_VA33; */
	ret = pmic_config_interface(0x714, 0x1, 0x1,
				    1); /* [1:1]: RG_HCLDO_REMOTE_SENSE_VA33; */
	ret = pmic_config_interface(
		0x71A, 0x1, 0x1, 15); /* [15:15]: RG_NCP_REMOTE_SENSE_VA18; */
	ret = pmic_config_interface(
		0x260, 0x10, 0x7F,
		8); /* [14:8]: VSRMCA15_VOSEL_OFFSET; set offset=100mV */
	ret = pmic_config_interface(
		0x260, 0x0, 0x7F,
		0); /* [6:0]: VSRMCA15_VOSEL_DELTA; set delta=0mV */
	ret = pmic_config_interface(
		0x262, 0x48, 0x7F,
		8); /* [14:8]: VSRMCA15_VOSEL_ON_HB; set HB=1.15V */
	ret = pmic_config_interface(
		0x262, 0x25, 0x7F,
		0); /* [6:0]: VSRMCA15_VOSEL_ON_LB; set LB=0.93125V */
	ret = pmic_config_interface(
		0x264, 0x0, 0x7F,
		0); /* [6:0]: VSRMCA15_VOSEL_SLEEP_LB; set sleep LB=0.7V */
	ret = pmic_config_interface(
		0x372, 0x4, 0x7F,
		8); /* [14:8]: VSRMCA7_VOSEL_OFFSET; set offset=25mV */
	ret = pmic_config_interface(
		0x372, 0x0, 0x7F,
		0); /* [6:0]: VSRMCA7_VOSEL_DELTA; set delta=0mV */
	ret = pmic_config_interface(
		0x374, 0x48, 0x7F,
		8); /* [14:8]: VSRMCA7_VOSEL_ON_HB; set HB=1.15V */
	ret = pmic_config_interface(
		0x374, 0x25, 0x7F,
		0); /* [6:0]: VSRMCA7_VOSEL_ON_LB; set LB=0.93125V */
	ret = pmic_config_interface(0x376, 0x18, 0x7F,
				    0); /* [6:0]: set sleep LB=0.85000V */
	ret = pmic_config_interface(0x21E, 0x3, 0x3,
				    0); /* [1:1]: DVS HW control by SRCLKEN */
	ret = pmic_config_interface(
		0x244, 0x3, 0x3,
		0); /* [1:1]: VSRMCA15_VOSEL_CTRL, VSRMCA15_EN_CTRL; */
	ret = pmic_config_interface(0x330, 0x0, 0x1,
				    1); /* [1:1]: VPCA7_VOSEL_CTRL; */
	ret = pmic_config_interface(0x356, 0x0, 0x1,
				    1); /* [1:1]: VSRMCA7_VOSEL_CTRL; */
	ret = pmic_config_interface(
		0x21E, 0x1, 0x1,
		4); /* [4:4]: VCA15_TRACK_ON_CTRL; DVFS tracking enable */
	ret = pmic_config_interface(0x244, 0x1, 0x1,
				    4); /* [4:4]: VSRMCA15_TRACK_ON_CTRL; */
	ret = pmic_config_interface(0x330, 0x0, 0x1,
				    4); /* [4:4]: VPCA7_TRACK_ON_CTRL; */
	ret = pmic_config_interface(0x356, 0x0, 0x1,
				    4); /* [4:4]: VSRMCA7_TRACK_ON_CTRL; */
	ret = pmic_config_interface(0x134, 0x3, 0x3,
				    14);		 /* [15:14]: VGPU OC; */
	ret = pmic_config_interface(0x134, 0x3, 0x3, 2); /* [3:2]: VCA15 OC; */
	ret = pmic_config_interface(0x432, 0x0, 0x1,
				    6); /* [6]: RG_VMCH_STB_SEL; */
	ret = pmic_config_interface(0x44E, 0x0, 0x1,
				    15); /* [15]: RG_STB_SEL; */
	ret = pmic_config_interface(0x432, 0x1, 0x1,
				    2); /* [2]: RG_VMCH_OCFB; */
#ifdef CONFIG_MTK_PMIC_VMCH_PG_DISABLE
	ret = pmic_config_interface(0x50A, 0x1, 0x1,
				    10); /* [10]: VMCH_PG_ENB; */
#endif
}

void PMIC_CUSTOM_SETTING_V1(void)
{
	/* enable HW control DCXO 26MHz on-off, request by SPM module */
	upmu_set_rg_srcvolt_hw_auto_en(1);
	upmu_set_rg_dcxo_ldo_dbb_reg_en(0);

	/* enable HW control DCXO RF clk on-off, request by low power module
	 * task
	 */
	upmu_set_rg_srclkperi_hw_auto_en(1);
	upmu_set_rg_dcxo_ldo_rf1_reg_en(0);

#ifndef CONFIG_MTK_PMIC_RF2_26M_ALWAYS_ON
	/* disable RF2 26MHz clock */
	upmu_set_rg_dcxo_ldo_rf2_reg_en(0x1);
	upmu_set_rg_dcxo_s2a_ldo_rf2_en(0x0);  /* clock off for internal 32K */
	upmu_set_rg_dcxo_por2_ldo_rf2_en(0x0); /* clock off for external 32K */
#else
	/* enable RF2 26MHz clock */
	upmu_set_rg_dcxo_ldo_rf2_reg_en(0x1);
	upmu_set_rg_dcxo_s2a_ldo_rf2_en(0x1);  /* clock on for internal 32K */
	upmu_set_rg_dcxo_por2_ldo_rf2_en(0x1); /* clock on for external 32K */
#endif
}

static void pmic_low_power_setting(void)
{
	unsigned int ret = 0;

	upmu_set_vio18_vsleep_en(1);
	/* top */
	ret = pmic_config_interface(0x102, 0x8080, 0x8080, 0);
	ret = pmic_config_interface(0x108, 0x0882, 0x0882, 0);
	ret = pmic_config_interface(0x12a, 0x0000, 0x8c00,
				    0); /* reg_ck:24MHz */
	ret = pmic_config_interface(0x206, 0x0060, 0x0060, 0);
	ret = pmic_config_interface(0x402, 0x0001, 0x0001, 0);

	/* chip_version > PMIC6397_E1_CID_CODE*/
	ret = pmic_config_interface(0x128, 0x0000, 0x0060, 0);

	/* VTCXO control */
	/* chip_version > PMIC6397_E1_CID_CODE*/
	/* enter low power mode when suspend */
	ret = pmic_config_interface(0x400, 0x4400, 0x6c01, 0);
	ret = pmic_config_interface(0x446, 0x0100, 0x0100, 0);
}

static irqreturn_t thr_h_int_handler(int irq, void *dev_id)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pr_notice("%s!\n", __func__);

	/* Read PMIC test register to get VMCH PG status */
	pmic_config_interface(0x13A, 0x0101, 0xFFFF, 0);
	ret = pmic_read_interface(0x150, &val, 0x1, 14);
	if (val == 0) {
		/* VMCH is not good */
		upmu_set_rg_vmch_en(0);
		pr_notice("%s: VMCH not good with status: 0x%x, turn off!\n",
		       __func__, upmu_get_reg_value(0x150));
	}

	return IRQ_HANDLED;
}

static irqreturn_t thr_l_int_handler(int irq, void *dev_id)
{
	pr_notice("%s!\n", __func__);

	return IRQ_HANDLED;
}

static int mt6397_pmic_probe(struct platform_device *dev)
{
	struct resource *res;
	int ret_val = 0;
	int irq_thr_l, irq_thr_h;
	struct mt6397_chip *mt6397_chip = dev_get_drvdata(dev->dev.parent);

	pr_debug("[Power/PMIC] ******** MT6397 pmic driver probe!! ********\n");

	pwrap_regmap = mt6397_chip->regmap;

	/* get PMIC CID */
	ret_val = upmu_get_cid();
	pr_notice("Power/PMIC MT6397 PMIC CID=0x%x\n", ret_val);

	/* pmic initial setting */
	PMIC_INIT_SETTING_V1();
	pr_debug("[Power/PMIC][PMIC_INIT_SETTING_V1] Done\n");

	PMIC_CUSTOM_SETTING_V1();
	pr_debug("[Power/PMIC][PMIC_CUSTOM_SETTING_V1] Done\n");

	/* pmic low power setting */
	pmic_low_power_setting();
	pr_debug("[Power/PMIC][pmic_low_power_setting] Done\n");

	res = platform_get_resource(dev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_info(&dev->dev, "no IRQ resource\n");
		return -ENODEV;
	}

	irq_thr_l = irq_create_mapping(mt6397_chip->irq_domain, res->start);
	if (irq_thr_l <= 0)
		return -EINVAL;

	ret_val = request_threaded_irq(irq_thr_l, NULL, thr_l_int_handler,
				       IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
				       "mt6397-thr_l", &dev->dev);
	if (ret_val) {
		dev_info(&dev->dev,
			 "Failed to request mt6397-thr_l IRQ: %d: %d\n",
			 irq_thr_l, ret_val);
	}

	irq_thr_h = irq_create_mapping(mt6397_chip->irq_domain, res->end);
	if (irq_thr_h <= 0)
		return -EINVAL;

	ret_val = request_threaded_irq(irq_thr_h, NULL, thr_h_int_handler,
				       IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
				       "mt6397-thr_h", &dev->dev);
	if (ret_val) {
		dev_info(&dev->dev,
			 "Failed to request mt6397-thr_h IRQ: %d: %d\n",
			 irq_thr_h, ret_val);
	}

	device_create_file(&(dev->dev), &dev_attr_pmic_access);
	return 0;
}

static const struct platform_device_id mt6397_pmic_ids[] = {
	{"mt6397-pmic", 0}, {/* sentinel */},
};
MODULE_DEVICE_TABLE(platform, mt6397_pmic_ids);

static const struct of_device_id mt6397_pmic_of_match[] = {
	{
		.compatible = "mediatek,mt6397-pmic",
	},
	{/* sentinel */},
};
MODULE_DEVICE_TABLE(of, mt6397_pmic_of_match);

static struct platform_driver mt6397_pmic_driver = {
	.driver = {

			.name = "mt6397-pmic",
			.of_match_table = of_match_ptr(mt6397_pmic_of_match),
		},
	.probe = mt6397_pmic_probe,
	.id_table = mt6397_pmic_ids,
};

module_platform_driver(mt6397_pmic_driver);

MODULE_AUTHOR("Chen Zhong <chen.zhong@mediatek.com>");
MODULE_DESCRIPTION("PMIC Misc Setting Driver for MediaTek MT6397 PMIC");
MODULE_LICENSE("GPL v2");
