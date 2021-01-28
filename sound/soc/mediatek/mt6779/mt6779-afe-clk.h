/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Eason Yen <eason.yen@mediatek.com>
 */

#ifndef _MT6779_AFE_CLOCK_CTRL_H_
#define _MT6779_AFE_CLOCK_CTRL_H_

#define AP_PLL_CON5 0x0014
#define APLL1_CON0 0x02c0
#define APLL1_CON1 0x02c4
#define APLL1_CON2 0x02c8
#define APLL1_CON3 0x02cc
#define APLL1_PWR_CON0 0x02d0

#define APLL2_CON0 0x02d4
#define APLL2_CON1 0x02d8
#define APLL2_CON2 0x02dc
#define APLL2_CON3 0x02e0
#define APLL2_PWR_CON0 0x02e4

#define APMIXEDSYS_MAX_LENGTH APLL2_PWR_CON0

#define CLK_CFG_6 0x0080
#define CLK_AUDDIV_0 0x0320
#define CLK_AUDDIV_1 0x0324
#define CLK_AUDDIV_2 0x0328
#define CKSYS_AUD_TOP_CFG 0x032c
#define CKSYS_AUD_TOP_MON 0x0330
#define CLK_AUDDIV_3 0x0334

#define CLK_MAX_LENGTH CLK_AUDDIV_3

/* CLK_CFG_6 */
#define CLK_AUD_INTBUS_SEL_SFT              16
#define CLK_AUD_INTBUS_SEL_MASK             0x3
#define CLK_AUD_INTBUS_SEL_MASK_SFT         (0x3 << 16)

/* CLK_AUDDIV_0 */
#define APLL_PDN_RESERVE0_SFT              0
#define APLL_PDN_RESERVE0_MASK             0x1
#define APLL_PDN_RESERVE0_MASK_SFT         (0x1 << 0)
#define APLL_PDN_RESERVE1_SFT              1
#define APLL_PDN_RESERVE1_MASK             0x1
#define APLL_PDN_RESERVE1_MASK_SFT         (0x1 << 1)
#define APLL12_DIV0_PDN_SFT                2
#define APLL12_DIV0_PDN_MASK               0x1
#define APLL12_DIV0_PDN_MASK_SFT           (0x1 << 2)
#define APLL12_DIV1_PDN_SFT                3
#define APLL12_DIV1_PDN_MASK               0x1
#define APLL12_DIV1_PDN_MASK_SFT           (0x1 << 3)
#define APLL12_DIV2_PDN_SFT                4
#define APLL12_DIV2_PDN_MASK               0x1
#define APLL12_DIV2_PDN_MASK_SFT           (0x1 << 4)
#define APLL12_DIV3_PDN_SFT                5
#define APLL12_DIV3_PDN_MASK               0x1
#define APLL12_DIV3_PDN_MASK_SFT           (0x1 << 5)
#define APLL12_DIV4_PDN_SFT                6
#define APLL12_DIV4_PDN_MASK               0x1
#define APLL12_DIV4_PDN_MASK_SFT           (0x1 << 6)
#define APLL12_DIVB_PDN_SFT                7
#define APLL12_DIVB_PDN_MASK               0x1
#define APLL12_DIVB_PDN_MASK_SFT           (0x1 << 7)
#define APLL_I2S0_MCK_SEL_SFT              8
#define APLL_I2S0_MCK_SEL_MASK             0x1
#define APLL_I2S0_MCK_SEL_MASK_SFT         (0x1 << 8)
#define APLL_I2S1_MCK_SEL_SFT              9
#define APLL_I2S1_MCK_SEL_MASK             0x1
#define APLL_I2S1_MCK_SEL_MASK_SFT         (0x1 << 9)
#define APLL_I2S2_MCK_SEL_SFT              10
#define APLL_I2S2_MCK_SEL_MASK             0x1
#define APLL_I2S2_MCK_SEL_MASK_SFT         (0x1 << 10)
#define APLL_I2S3_MCK_SEL_SFT              11
#define APLL_I2S3_MCK_SEL_MASK             0x1
#define APLL_I2S3_MCK_SEL_MASK_SFT         (0x1 << 11)
#define APLL_I2S4_MCK_SEL_SFT              12
#define APLL_I2S4_MCK_SEL_MASK             0x1
#define APLL_I2S4_MCK_SEL_MASK_SFT         (0x1 << 12)
#define APLL1_DIV0_INV_SFT                 16
#define APLL1_DIV0_INV_MASK                0x1
#define APLL1_DIV0_INV_MASK_SFT            (0x1 << 16)
#define APLL2_DIV0_INV_SFT                 17
#define APLL2_DIV0_INV_MASK                0x1
#define APLL2_DIV0_INV_MASK_SFT            (0x1 << 17)
#define APLL12_DIV0_INV_SFT                18
#define APLL12_DIV0_INV_MASK               0x1
#define APLL12_DIV0_INV_MASK_SFT           (0x1 << 18)
#define APLL12_DIV1_INV_SFT                19
#define APLL12_DIV1_INV_MASK               0x1
#define APLL12_DIV1_INV_MASK_SFT           (0x1 << 19)
#define APLL12_DIV2_INV_SFT                20
#define APLL12_DIV2_INV_MASK               0x1
#define APLL12_DIV2_INV_MASK_SFT           (0x1 << 20)
#define APLL12_DIV3_INV_SFT                21
#define APLL12_DIV3_INV_MASK               0x1
#define APLL12_DIV3_INV_MASK_SFT           (0x1 << 21)
#define APLL12_DIV4_INV_SFT                22
#define APLL12_DIV4_INV_MASK               0x1
#define APLL12_DIV4_INV_MASK_SFT           (0x1 << 22)
#define APLL12_DIVB_INV_SFT                23
#define APLL12_DIVB_INV_MASK               0x1
#define APLL12_DIVB_INV_MASK_SFT           (0x1 << 23)
#define APLL1_CK_DIV0_SFT                  24
#define APLL1_CK_DIV0_MASK                 0xf
#define APLL1_CK_DIV0_MASK_SFT             (0xf << 24)
#define APLL2_CK_DIV0_SFT                  28
#define APLL2_CK_DIV0_MASK                 0xf
#define APLL2_CK_DIV0_MASK_SFT             (0xf << 28)

/* CLK_AUDDIV_1 */
#define APLL12_CK_DIV0_SFT                 0
#define APLL12_CK_DIV0_MASK                0xff
#define APLL12_CK_DIV0_MASK_SFT            (0xff << 0)
#define APLL12_CK_DIV1_SFT                 8
#define APLL12_CK_DIV1_MASK                0xff
#define APLL12_CK_DIV1_MASK_SFT            (0xff << 8)
#define APLL12_CK_DIV2_SFT                 16
#define APLL12_CK_DIV2_MASK                0xff
#define APLL12_CK_DIV2_MASK_SFT            (0xff << 16)
#define APLL12_CK_DIV3_SFT                 24
#define APLL12_CK_DIV3_MASK                0xff
#define APLL12_CK_DIV3_MASK_SFT            (0xff << 24)

/* CLK_AUDDIV_2 */
#define APLL12_CK_DIV4_SFT                 0
#define APLL12_CK_DIV4_MASK                0xff
#define APLL12_CK_DIV4_MASK_SFT            (0xff << 0)
#define APLL12_CK_DIVB_SFT                 8
#define APLL12_CK_DIVB_MASK                0xff
#define APLL12_CK_DIVB_MASK_SFT            (0xff << 8)
#define APLL12_DIV5_PDN_SFT                16
#define APLL12_DIV5_PDN_MASK               0x1
#define APLL12_DIV5_PDN_MASK_SFT           (0x1 << 16)
#define RESERVED_0328_1_SFT                17
#define RESERVED_0328_1_MASK               0x7
#define RESERVED_0328_1_MASK_SFT           (0x7 << 17)
#define APLL_I2S5_MCK_SEL_SFT              20
#define APLL_I2S5_MCK_SEL_MASK             0x1
#define APLL_I2S5_MCK_SEL_MASK_SFT         (0x1 << 20)
#define RESERVED_0328_2_SFT                21
#define RESERVED_0328_2_MASK               0x7
#define RESERVED_0328_2_MASK_SFT           (0x7 << 21)
#define APLL12_DIV5_INV_SFT                24
#define APLL12_DIV5_INV_MASK               0x1
#define APLL12_DIV5_INV_MASK_SFT           (0x1 << 24)
#define RESERVED_0328_3_SFT                25
#define RESERVED_0328_3_MASK               0x7
#define RESERVED_0328_3_MASK_SFT           (0x7 << 25)
#define APLL12_CK_DIV5_LSB_SFT             28
#define APLL12_CK_DIV5_LSB_MASK            0xf
#define APLL12_CK_DIV5_LSB_MASK_SFT        (0xf << 28)

/* AUD_TOP_CFG */
#define AUD_TOP_CFG_SFT                    0
#define AUD_TOP_CFG_MASK                   0xffffffff
#define AUD_TOP_CFG_MASK_SFT               (0xffffffff << 0)

/* AUD_TOP_MON */
#define AUD_TOP_MON_SFT                    0
#define AUD_TOP_MON_MASK                   0xffffffff
#define AUD_TOP_MON_MASK_SFT               (0xffffffff << 0)

/* CLK_AUDDIV_3 */
#define APLL12_CK_DIV5_MSB_SFT             0
#define APLL12_CK_DIV5_MSB_MASK            0xf
#define APLL12_CK_DIV5_MSB_MASK_SFT        (0xf << 0)
#define RESERVED0_SFT                      4
#define RESERVED0_MASK                     0xfffffff
#define RESERVED0_MASK_SFT                 (0xfffffff << 4)

/* APLL */
#define APLL1_W_NAME "APLL1"
#define APLL2_W_NAME "APLL2"
enum {
	MT6779_APLL1 = 0,
	MT6779_APLL2,
};

struct mtk_base_afe;

int mt6779_init_clock(struct mtk_base_afe *afe);
int mt6779_afe_enable_clock(struct mtk_base_afe *afe);
void mt6779_afe_disable_clock(struct mtk_base_afe *afe);

int mt6779_afe_suspend_clock(struct mtk_base_afe *afe);
int mt6779_afe_resume_clock(struct mtk_base_afe *afe);

int mt6779_afe_dram_request(struct device *dev);
int mt6779_afe_dram_release(struct device *dev);

int mt6779_apll1_enable(struct mtk_base_afe *afe);
void mt6779_apll1_disable(struct mtk_base_afe *afe);

int mt6779_apll2_enable(struct mtk_base_afe *afe);
void mt6779_apll2_disable(struct mtk_base_afe *afe);

int mt6779_get_apll_rate(struct mtk_base_afe *afe, int apll);
int mt6779_get_apll_by_rate(struct mtk_base_afe *afe, int rate);
int mt6779_get_apll_by_name(struct mtk_base_afe *afe, const char *name);

extern void aud_intbus_mux_sel(unsigned int aud_idx);

/* these will be replaced by using CCF */
int mt6779_mck_enable(struct mtk_base_afe *afe, unsigned int mck_id, int rate);
void mt6779_mck_disable(struct mtk_base_afe *afe, unsigned int mck_id);

#endif
