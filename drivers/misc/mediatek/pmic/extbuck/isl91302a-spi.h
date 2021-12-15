/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#ifndef __LINUX_ISL91302A_SPI_H
#define __LINUX_ISL91302A_SPI_H

#include <linux/mutex.h>
#include <linux/regulator/consumer.h>

struct isl91302a_chip {
#ifndef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	struct spi_device *spi;
	struct mutex io_lock;
	#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *regmap_dev;
	#endif /* CONFIG_RT_REGMAP */
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	struct device *dev;
};

#define ISL91302A_CHIPNAME	(0x03)

/* register map */
#define ISL91302A_CHIPNAME_R		(0x01)
#define ISL91302A_FLT_RECORDTEMP_R	(0x13)
#define ISL91302A_MODECTRL_R		(0x24)
#define ISL91302A_IRQ_MASK_R		(0x32)
#define ISL91302A_BUCK1_DCM_R		(0x3E)
#define ISL91302A_BUCK1_UP_R		(0x48)
#define ISL91302A_BUCK1_LO_R		(0x49)
#define ISL91302A_BUCK1_RSPCFG1_R	(0x54)
#define ISL91302A_BUCK2_DCM_R		(0x5B)
#define ISL91302A_BUCK2_UP_R		(0x62)
#define ISL91302A_BUCK2_LO_R		(0x63)
#define ISL91302A_BUCK2_RSPCFG1_R	(0x6E)
#define ISL91302A_BUCK3_DCM_R		(0x75)
#define ISL91302A_BUCK3_UP_R		(0x7C)
#define ISL91302A_BUCK3_LO_R		(0x7D)
#define ISL91302A_BUCK3_RSPCFG1_R	(0x88)

/* 4.DVS slew rat */
#define ISL91302A_BUCK_RSPCFG1_RSPUP_M	(0x70)
#define ISL91302A_BUCK_RSPCFG1_RSPUP_S	4
#define ISL91302A_BUCK_RSPSEL_M	(0x40)

/* 6.Interrupt : OTP/OCP/OV */
#define FLT_RECORDTEMP_FLT_TEMPSDR_M        (0x08)
#define FLT_RECORDTEMP_FLT_TEMPWARNR_M      (0x04)
#define FLT_RECORDTEMP_FLT_TEMPWARNF_M      (0x02)
#define FLT_RECORDTEMP_FLT_TEMPSDF_M        (0x01)
#define FLT_RECORDBUCK1_FLT_BUCK1_WOC_M     (0x40)
#define FLT_RECORDBUCK1_FLT_BUCK1_OV_M      (0x20)
#define FLT_RECORDBUCK1_FLT_BUCK1_UV_M      (0x10)
#define FLT_RECORDBUCK2_FLT_BUCK2_WOC_M     (0x40)
#define FLT_RECORDBUCK2_FLT_BUCK2_OV_M      (0x20)
#define FLT_RECORDBUCK2_FLT_BUCK2_UV_M      (0x10)

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
static inline int isl91302a_read_byte(void *client, uint32_t addr,
				      uint32_t *val)
{
	pr_notice("%s not support in sspm\n", __func__);
	return -EINVAL;
}
static inline int isl91302a_write_byte(void *client, uint32_t addr,
				       uint32_t value)
{
	pr_notice("%s not support in sspm\n", __func__);
	return -EINVAL;
}
static inline int isl91302a_assign_bit(void *client, uint32_t reg,
					uint32_t mask, uint32_t data)
{
	pr_notice("%s not support in sspm\n", __func__);
	return -EINVAL;
}
#else
extern int isl91302a_read_byte(void *client, uint32_t addr, uint32_t *val);
extern int isl91302a_write_byte(void *client, uint32_t addr, uint32_t value);
extern int isl91302a_assign_bit(void *client, uint32_t reg,
					uint32_t mask, uint32_t data);
#endif /*  CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
extern int isl91302a_regulator_init(struct isl91302a_chip *chip);
extern int isl91302a_regulator_deinit(struct isl91302a_chip *chip);

#define isl91302a_set_bit(spi, reg, mask) \
	isl91302a_assign_bit(spi, reg, mask, mask)

#define isl91302a_clr_bit(spi, reg, mask) \
	isl91302a_assign_bit(spi, reg, mask, 0x00)

#define ISL91302A_INFO(format, args...) pr_info(format, ##args)
#define ISL91302A_pr_notice(format, args...)	pr_notice(format, ##args)

#endif /* __LINUX_ISL91302A_SPI_H */
