// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 *
 */

#include <dt-bindings/phy/phy.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

/* version V1 sub-banks offset base address */
/* banks shared by multiple phys */
#define SSUSB_SIFSLV_V1_SPLLC		0x000	/* shared by u3 phys */
#define SSUSB_SIFSLV_V1_U2FREQ		0x100	/* shared by u2 phys */
#define SSUSB_SIFSLV_V1_CHIP		0x300	/* shared by u3 phys */
/* u2 phy bank */
#define SSUSB_SIFSLV_V1_U2PHY_COM	0x000
/* u3/pcie/sata phy banks */
#define SSUSB_SIFSLV_V1_U3PHYD		0x000
#define SSUSB_SIFSLV_V1_U3PHYA		0x200

/* version V2 sub-banks offset base address */
/* u2 phy banks */
#define SSUSB_SIFSLV_V2_MISC		0x000
#define SSUSB_SIFSLV_V2_U2FREQ		0x100
#define SSUSB_SIFSLV_V2_U2PHY_COM	0x300
/* u3/pcie/sata phy banks */
#define SSUSB_SIFSLV_V2_SPLLC		0x000
#define SSUSB_SIFSLV_V2_CHIP		0x100
#define SSUSB_SIFSLV_V2_U3PHYD		0x200
#define SSUSB_SIFSLV_V2_U3PHYA		0x400

#define U3P_USBPHYACR0		0x000
#define PA0_RG_U2PLL_FORCE_ON		BIT(15)
#define PA0_RG_USB20_INTR_EN		BIT(5)

#define U3P_USBPHYACR1		0x004
#define PA1_RG_U2_INTR_CAL		GENMASK(23, 19)
#define PA1_RG_U2_INTR_CAL_VAL(x)	((0x1f & (x)) << 19)
#define PA1_RG_VRT_SEL			GENMASK(14, 12)
#define PA1_RG_VRT_SEL_VAL(x)	((0x7 & (x)) << 12)
#define PA1_RG_VRT_SEL_MASK	(0x7)
#define PA1_RG_VRT_SEL_OFST	(12)
#define PA1_RG_TERM_SEL		GENMASK(10, 8)
#define PA1_RG_TERM_SEL_VAL(x)	((0x7 & (x)) << 8)
#define PA1_RG_TERM_SEL_MASK	(0x7)
#define PA1_RG_TERM_SEL_OFST	(8)

#define U3P_USBPHYACR2		0x008
#define PA2_RG_SIF_U2PLL_FORCE_EN	BIT(18)

#define U3P_USBPHYACR5		0x014
#define PA5_RG_U2_HSTX_SRCAL_EN	BIT(15)
#define PA5_RG_U2_HSTX_SRCTRL		GENMASK(14, 12)
#define PA5_RG_U2_HSTX_SRCTRL_VAL(x)	((0x7 & (x)) << 12)
#define PA5_RG_U2_HS_100U_U3_EN	BIT(11)

#define U3P_USBPHYACR6		0x018
#define PA6_RG_U2_PHY_REV6		GENMASK(31, 30)
#define PA6_RG_U2_PHY_REV6_VAL(x)	((0x3 & (x)) << 30)
#define PA6_RG_U2_PHY_REV6_MASK	(0x3)
#define PA6_RG_U2_PHY_REV6_OFET	(30)

#ifndef CONFIG_MACH_MT6781
#define PA6_RG_U2_PHY_REV4		BIT(28)
#define PA6_RG_U2_PHY_REV4_VAL(x)	((0x1 & (x)) << 28)
#define PA6_RG_U2_PHY_REV4_MASK	(0x1)
#define PA6_RG_U2_PHY_REV4_OFET	(28)
#define PA6_RG_U2_PHY_REV1		BIT(25)
#endif

#define PA6_RG_U2_BC11_SW_EN		BIT(23)
#define PA6_RG_U2_OTG_VBUSCMP_EN	BIT(20)
#define PA6_RG_U2_DISCTH	GENMASK(7, 4)
#define PA6_RG_U2_DISCTH_VAL(x)	((0xf & (x)) << 4)
#define PA6_RG_U2_DISCTH_MASK	(0xf)
#define PA6_RG_U2_DISCTH_OFET	(4)
#define PA6_RG_U2_SQTH		GENMASK(3, 0)
#define PA6_RG_U2_SQTH_VAL(x)	(0xf & (x))

#define U3P_USBPHYACR3		0x01c
#define PA3_RG_USB20_PUPD_BIST_EN	BIT(12)
#define PA3_RG_USB20_EN_PU_DP		BIT(9)

#define U3P_U2PHYACR4		0x020
#define P2C_RG_USB20_DM_100K_EN		BIT(17)
#define P2C_RG_USB20_GPIO_CTL		BIT(9)
#define P2C_USB20_GPIO_MODE		BIT(8)
#define P2C_U2_GPIO_CTR_MSK	(P2C_RG_USB20_GPIO_CTL | P2C_USB20_GPIO_MODE)

#define U3D_U2PHYDCR0		0x060
#define P2C_RG_SIF_U2PLL_FORCE_ON	BIT(24)

#define U3P_U2PHYDTM0		0x068
#define P2C_RG_UART_MODE		GENMASK(31, 30)
#define P2C_RG_UART_MODE_VAL(x)		((0x3 & (x)) << 30)
#define P2C_RG_UART_MODE_OFET		(30)
#define P2C_FORCE_UART_I		BIT(29)
#define P2C_FORCE_UART_BIAS_EN		BIT(28)
#define P2C_FORCE_UART_TX_OE		BIT(27)
#define P2C_FORCE_UART_EN		BIT(26)
#define P2C_FORCE_DATAIN		BIT(23)
#define P2C_FORCE_DM_PULLDOWN		BIT(21)
#define P2C_FORCE_DP_PULLDOWN		BIT(20)
#define P2C_FORCE_XCVRSEL		BIT(19)
#define P2C_FORCE_SUSPENDM		BIT(18)
#define P2C_FORCE_TERMSEL		BIT(17)
#define P2C_RG_DATAIN			GENMASK(13, 10)
#define P2C_RG_DATAIN_VAL(x)		((0xf & (x)) << 10)
#define P2C_RG_DMPULLDOWN		BIT(7)
#define P2C_RG_DPPULLDOWN		BIT(6)
#define P2C_RG_XCVRSEL			GENMASK(5, 4)
#define P2C_RG_XCVRSEL_VAL(x)		((0x3 & (x)) << 4)
#define P2C_RG_SUSPENDM			BIT(3)
#define P2C_RG_TERMSEL			BIT(2)
#define P2C_DTM0_PART_MASK \
		(P2C_FORCE_DATAIN | P2C_FORCE_DM_PULLDOWN | \
		P2C_FORCE_DP_PULLDOWN | P2C_FORCE_XCVRSEL | \
		P2C_FORCE_SUSPENDM | P2C_FORCE_TERMSEL | \
		P2C_RG_DMPULLDOWN | P2C_RG_DPPULLDOWN | \
		P2C_RG_TERMSEL)

#define P2C_DTM0_PART_MASK2 \
		(P2C_FORCE_DM_PULLDOWN | P2C_FORCE_DP_PULLDOWN | \
		P2C_FORCE_XCVRSEL | P2C_FORCE_SUSPENDM | \
		P2C_FORCE_TERMSEL | P2C_RG_DMPULLDOWN | \
		P2C_RG_DPPULLDOWN | P2C_RG_TERMSEL)

#define U3P_U2PHYDTM1		0x06C
#define P2C_RG_UART_BIAS_EN		BIT(18)
#define P2C_RG_UART_TX_OE		BIT(17)
#define P2C_RG_UART_EN			BIT(16)
#define P2C_FORCE_VBUSVALID		BIT(13)
#define P2C_FORCE_SESSEND		BIT(12)
#define P2C_FORCE_BVALID		BIT(11)
#define P2C_FORCE_AVALID		BIT(10)
#define P2C_FORCE_IDDIG		BIT(9)
#define P2C_FORCE_IDPULLUP		BIT(8)
#define P2C_RG_VBUSVALID		BIT(5)
#define P2C_RG_SESSEND			BIT(4)
#define P2C_RG_BVALID			BIT(3)
#define P2C_RG_AVALID			BIT(2)
#define P2C_RG_IDDIG			BIT(1)
#define P2C_RG_RG_IDPULLUP		BIT(0)

#define U3P_U2PHYBC12C		0x080
#define P2C_RG_CHGDT_EN		BIT(0)

#define U3P_U3_CHIP_GPIO_CTLD		0x0c
#define P3C_REG_IP_SW_RST		BIT(31)
#define P3C_MCU_BUS_CK_GATE_EN		BIT(30)
#define P3C_FORCE_IP_SW_RST		BIT(29)

#define U3P_U3_CHIP_GPIO_CTLE		0x10
#define P3C_RG_SWRST_U3_PHYD		BIT(25)
#define P3C_RG_SWRST_U3_PHYD_FORCE_EN	BIT(24)

#define U3P_U3_PHYA_REG0	0x000
#define P3A_RG_SSUSB_IEXT_INTR_CTRL	GENMASK(15, 10)
#define P3A_RG_SSUSB_IEXT_INTR_CTRL_VAL(x)	((0x3f & (x)) << 10)

#define P3A_RG_CLKDRV_OFF		GENMASK(3, 2)
#define P3A_RG_CLKDRV_OFF_VAL(x)	((0x3 & (x)) << 2)

#define U3P_U3_PHYA_REG1	0x004
#define P3A_RG_CLKDRV_AMP		GENMASK(31, 29)
#define P3A_RG_CLKDRV_AMP_VAL(x)	((0x7 & (x)) << 29)
#define RG_SSUSB_VA_ON			BIT(29)

#define U3P_U3_PHYA_REG6	0x018
#define P3A_RG_TX_EIDLE_CM		GENMASK(31, 28)
#define P3A_RG_TX_EIDLE_CM_VAL(x)	((0xf & (x)) << 28)

#define U3P_U3_PHYA_REG9	0x024
#define P3A_RG_RX_DAC_MUX		GENMASK(5, 1)
#define P3A_RG_RX_DAC_MUX_VAL(x)	((0x1f & (x)) << 1)

#define U3P_U3_PHYA_DA_REG0	0x100
#define P3A_RG_XTAL_EXT_PE2H		GENMASK(17, 16)
#define P3A_RG_XTAL_EXT_PE2H_VAL(x)	((0x3 & (x)) << 16)
#define P3A_RG_XTAL_EXT_PE1H		GENMASK(13, 12)
#define P3A_RG_XTAL_EXT_PE1H_VAL(x)	((0x3 & (x)) << 12)
#define P3A_RG_XTAL_EXT_EN_U3		GENMASK(11, 10)
#define P3A_RG_XTAL_EXT_EN_U3_VAL(x)	((0x3 & (x)) << 10)

#define U3P_U3_PHYA_DA_REG4	0x108
#define P3A_RG_PLL_DIVEN_PE2H		GENMASK(21, 19)
#define P3A_RG_PLL_BC_PE2H		GENMASK(7, 6)
#define P3A_RG_PLL_BC_PE2H_VAL(x)	((0x3 & (x)) << 6)

#define U3P_U3_PHYA_DA_REG5	0x10c
#define P3A_RG_PLL_BR_PE2H		GENMASK(29, 28)
#define P3A_RG_PLL_BR_PE2H_VAL(x)	((0x3 & (x)) << 28)
#define P3A_RG_PLL_IC_PE2H		GENMASK(15, 12)
#define P3A_RG_PLL_IC_PE2H_VAL(x)	((0xf & (x)) << 12)

#define U3P_U3_PHYA_DA_REG6	0x110
#define P3A_RG_PLL_IR_PE2H		GENMASK(19, 16)
#define P3A_RG_PLL_IR_PE2H_VAL(x)	((0xf & (x)) << 16)

#define U3P_U3_PHYA_DA_REG7	0x114
#define P3A_RG_PLL_BP_PE2H		GENMASK(19, 16)
#define P3A_RG_PLL_BP_PE2H_VAL(x)	((0xf & (x)) << 16)

#define U3P_U3_PHYA_DA_REG20	0x13c
#define P3A_RG_PLL_DELTA1_PE2H		GENMASK(31, 16)
#define P3A_RG_PLL_DELTA1_PE2H_VAL(x)	((0xffff & (x)) << 16)

#define U3P_U3_PHYA_DA_REG25	0x148
#define P3A_RG_PLL_DELTA_PE2H		GENMASK(15, 0)
#define P3A_RG_PLL_DELTA_PE2H_VAL(x)	(0xffff & (x))

#define U3P_U3_PHYD_MIX0		0x000

#define U3P_U3_PHYD_LFPS1		0x00c
#define P3D_RG_FWAKE_TH		GENMASK(21, 16)
#define P3D_RG_FWAKE_TH_VAL(x)	((0x3f & (x)) << 16)

#define U3P_U3_PHYD_RX0			0x02c

#define U3P_U3_PHYD_T2RLB		0x030

#define U3P_U3_PHYD_PIPE0		0x040

#define U3P_U3_PHYD_IMPCAL0		0x010
#define P3D_RG_SSUSB_TX_IMPSEL		GENMASK(28, 24)
#define P3D_RG_SSUSB_TX_IMPSEL_VAL(x)	((0x1f & (x)) << 24)

#define U3P_U3_PHYD_IMPCAL1		0x014
#define P3D_RG_SSUSB_RX_IMPSEL		GENMASK(28, 24)
#define P3D_RG_SSUSB_RX_IMPSEL_VAL(x)	((0x1f & (x)) << 24)

#define U3P_U3_PHYD_CDR1		0x05c
#define P3D_RG_CDR_BIR_LTD1		GENMASK(28, 24)
#define P3D_RG_CDR_BIR_LTD1_VAL(x)	((0x1f & (x)) << 24)
#define P3D_RG_CDR_BIR_LTD0		GENMASK(12, 8)
#define P3D_RG_CDR_BIR_LTD0_VAL(x)	((0x1f & (x)) << 8)

#define U3P_U3_PHYD_RXDET1		0x128
#define P3D_RG_RXDET_STB2_SET		GENMASK(17, 9)
#define P3D_RG_RXDET_STB2_SET_VAL(x)	((0x1ff & (x)) << 9)

#define U3P_U3_PHYD_RXDET2		0x12c
#define P3D_RG_RXDET_STB2_SET_P3	GENMASK(8, 0)
#define P3D_RG_RXDET_STB2_SET_P3_VAL(x)	(0x1ff & (x))

#define U3P_SPLLC_XTALCTL3		0x018
#define XC3_RG_U3_XTAL_RX_PWD		BIT(9)
#define XC3_RG_U3_FRC_XTAL_RX_PWD	BIT(8)

#define U3P_U2FREQ_FMCR0	0x00
#define P2F_RG_MONCLK_SEL	GENMASK(27, 26)
#define P2F_RG_MONCLK_SEL_VAL(x)	((0x3 & (x)) << 26)
#define P2F_RG_FREQDET_EN	BIT(24)
#define P2F_RG_CYCLECNT		GENMASK(23, 0)
#define P2F_RG_CYCLECNT_VAL(x)	((P2F_RG_CYCLECNT) & (x))

#define U3P_U2FREQ_VALUE	0x0c

#define U3P_U2FREQ_FMMONR1	0x10
#define P2F_USB_FM_VALID	BIT(0)
#define P2F_RG_FRCK_EN		BIT(8)

#define U3P_REF_CLK		26	/* MHZ */
#define U3P_SLEW_RATE_COEF	28
#define U3P_SR_COEF_DIVISOR	1000
#define U3P_FM_DET_CYCLE_CNT	1024

/* SATA register setting */
#define PHYD_CTRL_SIGNAL_MODE4		0x1c
/* CDR Charge Pump P-path current adjustment */
#define RG_CDR_BICLTD1_GEN1_MSK		GENMASK(23, 20)
#define RG_CDR_BICLTD1_GEN1_VAL(x)	((0xf & (x)) << 20)
#define RG_CDR_BICLTD0_GEN1_MSK		GENMASK(11, 8)
#define RG_CDR_BICLTD0_GEN1_VAL(x)	((0xf & (x)) << 8)

#define PHYD_DESIGN_OPTION2		0x24
/* Symbol lock count selection */
#define RG_LOCK_CNT_SEL_MSK		GENMASK(5, 4)
#define RG_LOCK_CNT_SEL_VAL(x)		((0x3 & (x)) << 4)

#define PHYD_DESIGN_OPTION9	0x40
/* COMWAK GAP width window */
#define RG_TG_MAX_MSK		GENMASK(20, 16)
#define RG_TG_MAX_VAL(x)	((0x1f & (x)) << 16)
/* COMINIT GAP width window */
#define RG_T2_MAX_MSK		GENMASK(13, 8)
#define RG_T2_MAX_VAL(x)	((0x3f & (x)) << 8)
/* COMWAK GAP width window */
#define RG_TG_MIN_MSK		GENMASK(7, 5)
#define RG_TG_MIN_VAL(x)	((0x7 & (x)) << 5)
/* COMINIT GAP width window */
#define RG_T2_MIN_MSK		GENMASK(4, 0)
#define RG_T2_MIN_VAL(x)	(0x1f & (x))

#define ANA_RG_CTRL_SIGNAL1		0x4c
/* TX driver tail current control for 0dB de-empahsis mdoe for Gen1 speed */
#define RG_IDRV_0DB_GEN1_MSK		GENMASK(13, 8)
#define RG_IDRV_0DB_GEN1_VAL(x)		((0x3f & (x)) << 8)

#define ANA_RG_CTRL_SIGNAL4		0x58
#define RG_CDR_BICLTR_GEN1_MSK		GENMASK(23, 20)
#define RG_CDR_BICLTR_GEN1_VAL(x)	((0xf & (x)) << 20)
/* Loop filter R1 resistance adjustment for Gen1 speed */
#define RG_CDR_BR_GEN2_MSK		GENMASK(10, 8)
#define RG_CDR_BR_GEN2_VAL(x)		((0x7 & (x)) << 8)

#define ANA_RG_CTRL_SIGNAL6		0x60
/* I-path capacitance adjustment for Gen1 */
#define RG_CDR_BC_GEN1_MSK		GENMASK(28, 24)
#define RG_CDR_BC_GEN1_VAL(x)		((0x1f & (x)) << 24)
#define RG_CDR_BIRLTR_GEN1_MSK		GENMASK(4, 0)
#define RG_CDR_BIRLTR_GEN1_VAL(x)	(0x1f & (x))

#define ANA_EQ_EYE_CTRL_SIGNAL1		0x6c
/* RX Gen1 LEQ tuning step */
#define RG_EQ_DLEQ_LFI_GEN1_MSK		GENMASK(11, 8)
#define RG_EQ_DLEQ_LFI_GEN1_VAL(x)	((0xf & (x)) << 8)

#define ANA_EQ_EYE_CTRL_SIGNAL4		0xd8
#define RG_CDR_BIRLTD0_GEN1_MSK		GENMASK(20, 16)
#define RG_CDR_BIRLTD0_GEN1_VAL(x)	((0x1f & (x)) << 16)

#define ANA_EQ_EYE_CTRL_SIGNAL5		0xdc
#define RG_CDR_BIRLTD0_GEN3_MSK		GENMASK(4, 0)
#define RG_CDR_BIRLTD0_GEN3_VAL(x)	(0x1f & (x))

#define PHY_MODE_BC11_SW_SET 1
#define PHY_MODE_BC11_SW_CLR 2

#ifndef CONFIG_MACH_MT6781
#define PHY_MODE_DPDMPULLDOWN_SET 3
#define PHY_MODE_DPDMPULLDOWN_CLR 4
#define PHY_MODE_DPPULLUP_SET 5
#define PHY_MODE_DPPULLUP_CLR 6
#endif

#define PROC_FILE_TERM_SEL "term_sel"
#define PROC_FILE_VRT_SEL "vrt_sel"
#define PROC_FILE_PHY_REV6 "phy_rev6"
#define PROC_FILE_DISCTH "discth"
#define LOOPBACK_STR "loopback_test"

enum mtk_phy_version {
	MTK_PHY_V1 = 1,
	MTK_PHY_V2,
};

enum mtk_phy_efuse {
	INTR_CAL = 0,
	IEXT_INTR_CTRL,
	RX_IMPSEL,
	TX_IMPSEL,
};

static char *efuse_name[4] = {
	"intr_cal",
	"iext_intr_ctrl",
	"rx_impsel",
	"tx_impsel",
};

struct mtk_phy_pdata {
	/* avoid RX sensitivity level degradation only for mt8173 */
	bool avoid_rx_sen_degradation;
	enum mtk_phy_version version;
};

struct u2phy_banks {
	void __iomem *misc;
	void __iomem *fmreg;
	void __iomem *com;
};

struct u3phy_banks {
	void __iomem *spllc;
	void __iomem *chip;
	void __iomem *phyd; /* include u3phyd_bank2 */
	void __iomem *phya; /* include u3phya_da */
};

struct mtk_phy_instance {
	struct phy *phy;
	void __iomem *port_base;
	union {
		struct u2phy_banks u2_banks;
		struct u3phy_banks u3_banks;
	};
	struct clk *ref_clk;	/* reference clock of anolog phy */
	u32 index;
	u8 type;
	int eye_src;
	int eye_vrt;
	int eye_term;
	int eye_rev6;
	int eye_disc;
	bool bc12_en;
	struct proc_dir_entry *phy_root;
};

struct mtk_tphy {
	struct device *dev;
	void __iomem *sif_base;	/* only shared sif */
	/* deprecated, use @ref_clk instead in phy instance */
	struct clk *u3phya_ref;	/* reference clock of usb3 anolog phy */
	const struct mtk_phy_pdata *pdata;
	struct mtk_phy_instance **phys;
	int nphys;
	int src_ref_clk; /* MHZ, reference clock for slew rate calibrate */
	int src_coef; /* coefficient for slew rate calibrate */
	struct proc_dir_entry *root;
};

static void u2_phy_props_set(struct mtk_tphy *tphy,
			     struct mtk_phy_instance *instance);

static void u2_phy_instance_set_mode_ext(struct mtk_tphy *tphy,
				     struct mtk_phy_instance *instance,
				     int submode);

void cover_val_to_str(u32 val, u8 width, char *str)
{
	int i, temp;

	temp = val;
	str[width] = '\0';
	for (i = (width - 1); i >= 0; i--) {
		if (val % 2)
			str[i] = '1';
		else
			str[i] = '0';
		val /= 2;
	}
}

/*
 * loopback_test: default test pattern
 *   readl(U3D_PHYD_PIPE0) &
 *     ~(0x01<<30)) | 0x01<<30,
 *     ~(0x01<<28)) | 0x00<<28,
 *     ~(0x03<<26)) | 0x01<<26,
 *     ~(0x03<<24)) | 0x00<<24,
 *     ~(0x01<<22)) | 0x00<<22,
 *     ~(0x01<<21)) | 0x00<<21,
 *     ~(0x01<<20)) | 0x01<<20.
 */
#define U3P_U3_PHYD_PIPE0_CLR_PATTERN	0x5f700000
#define U3P_U3_PHYD_PIPE0_SET_PATTERN	0x44100000

static int proc_loopback_test_show(struct seq_file *s, void *unused)
{
	struct mtk_phy_instance *instance = s->private;
	struct device *dev = &instance->phy->dev;
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	int r_pipe0, r_rx0, r_mix0, r_t2rlb;
	bool ret = false;
	u32 tmp;

	r_mix0 = readl(u3_banks->phyd + U3P_U3_PHYD_MIX0);
	r_rx0 = readl(u3_banks->phyd + U3P_U3_PHYD_RX0);
	r_t2rlb = readl(u3_banks->phyd + U3P_U3_PHYD_T2RLB);
	r_pipe0 = readl(u3_banks->phyd + U3P_U3_PHYD_PIPE0);

	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_PIPE0);
	tmp &= ~(U3P_U3_PHYD_PIPE0_CLR_PATTERN);
	tmp |= U3P_U3_PHYD_PIPE0_SET_PATTERN;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_PIPE0);

	mdelay(10);

	/* T2R loop back disable */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RX0);
	tmp &= ~(0x01<<15);
	tmp |= 0x00<<15;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RX0);

	mdelay(10);

	/* TSEQ lock detect threshold */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_MIX0);
	tmp &= ~(0x07<<24);
	tmp |= 0x07<<24;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_MIX0);

	/* set default TSEQ polarity check value = 1 */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_MIX0);
	tmp &= ~(0x01<<28);
	tmp |= 0x01<<28;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_MIX0);

	/* TSEQ polarity check enable */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_MIX0);
	tmp &= ~(0x01<<29);
	tmp |= 0x01<<29;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_MIX0);

	/* TSEQ decoder enable */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_MIX0);
	tmp &= ~(0x01<<30);
	tmp |= 0x01<<30;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_MIX0);

	mdelay(10);

	/* set T2R loop back TSEQ length (x 16us) */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_T2RLB);
	tmp &= ~(0xff<<0);
	tmp |= 0xf0<<0;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_T2RLB);

	/* set T2R loop back BDAT reset period (x 16us) */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_T2RLB);
	tmp &= ~(0x0f<<12);
	tmp |= 0x0f<<12;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_T2RLB);

	/* T2R loop back pattern select */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_T2RLB);
	tmp &= ~(0x03<<8);
	tmp |= 0x00<<8;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_T2RLB);

	mdelay(10);

	/* T2R loop back serial mode */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RX0);
	tmp &= ~(0x01<<13);
	tmp |= 0x01<<13;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RX0);

	/* T2R loop back parallel mode = 0 */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RX0);
	tmp &= ~(0x01<<12);
	tmp |= 0x00<<12;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RX0);

	/* T2R loop back mode enable */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RX0);
	tmp &= ~(0x01<<11);
	tmp |= 0x01<<11;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RX0);

	/* T2R loop back enable */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RX0);
	tmp &= ~(0x01<<15);
	tmp |= 0x01<<15;
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RX0);
	mdelay(100);

	dev_info(dev, "%s, U3 loop back started\n", __func__);

	/* check result */
	tmp = readl(u3_banks->phyd + 0x0b4);

	/* verbose dump */
	dev_info(dev, "rb back             : 0x%x\n", tmp);
	dev_info(dev, "rb t2rlb_lock  : %d\n", (tmp >> 2) & 0x01);
	dev_info(dev, "rb t2rlb_pass  : %d\n", (tmp >> 3) & 0x01);
	dev_info(dev, "rb t2rlb_passth: %d\n", (tmp >> 4) & 0x01);

	/* return result */
	tmp &= 0x0E;
	if (tmp == 0x0E)
		ret = true;
	else
		ret = false;

	/* restore settings */
	writel(r_rx0, u3_banks->phyd + U3P_U3_PHYD_RX0);
	writel(r_pipe0, u3_banks->phyd + U3P_U3_PHYD_PIPE0);
	writel(r_mix0, u3_banks->phyd + U3P_U3_PHYD_MIX0);
	writel(r_t2rlb, u3_banks->phyd + U3P_U3_PHYD_T2RLB);

	dev_info(dev, "%s, loopback_test=0x%x\n", __func__, tmp);

	seq_printf(s,  "%d\n", ret);
	return 0;
}

static int proc_loopback_test_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_loopback_test_show, PDE_DATA(inode));
}

static const struct file_operations proc_loopback_test_fops = {
	.open = proc_loopback_test_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int u3_phy_procfs_init(struct mtk_tphy *tphy,
			struct mtk_phy_instance *instance)
{
	struct device *dev = &instance->phy->dev;
	struct proc_dir_entry *root = tphy->root;
	struct proc_dir_entry *phy_root;
	struct proc_dir_entry *file;
	int ret;

	if (!root) {
		dev_info(dev, "phy proc root not exist\n");
		ret = -ENOMEM;
		goto err0;
	}

	phy_root = proc_mkdir("u3_phy", root);
	if (!root) {
		dev_info(dev, "failed to creat dir proc u3_phy\n");
		ret = -ENOMEM;
		goto err0;
	}

	file = proc_create_data(LOOPBACK_STR, 0444,
			phy_root, &proc_loopback_test_fops, instance);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", LOOPBACK_STR);
		ret = -ENOMEM;
		goto err1;
	}

	instance->phy_root = phy_root;
	return 0;
err1:
	proc_remove(phy_root);

err0:
	return ret;
}

static int u3_phy_procfs_exit(struct mtk_phy_instance *instance)
{
	proc_remove(instance->phy_root);
	return 0;
}

static int proc_term_sel_show(struct seq_file *s, void *unused)
{
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 tmp;
	char str[16];

	tmp = readl(com + U3P_USBPHYACR1);
	tmp >>= PA1_RG_TERM_SEL_OFST;
	tmp &= PA1_RG_TERM_SEL_MASK;

	cover_val_to_str(tmp, 3, str);

	seq_printf(s, "\n%s = %s\n", PROC_FILE_TERM_SEL, str);
	return 0;
}

static int proc_term_sel_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_term_sel_show, PDE_DATA(inode));
}

static ssize_t proc_term_sel_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	char buf[20];
	u32 tmp, val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	tmp = readl(com + U3P_USBPHYACR1);
	tmp &= ~PA1_RG_TERM_SEL;
	tmp |= PA1_RG_TERM_SEL_VAL(val);
	writel(tmp, com + U3P_USBPHYACR1);

	return count;
}

static const struct file_operations proc_term_sel_fops = {
	.open = proc_term_sel_open,
	.write = proc_term_sel_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int proc_vrt_sel_show(struct seq_file *s, void *unused)
{
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 tmp;
	char str[16];

	tmp = readl(com + U3P_USBPHYACR1);
	tmp >>= PA1_RG_VRT_SEL_OFST;
	tmp &= PA1_RG_VRT_SEL_MASK;

	cover_val_to_str(tmp, 3, str);

	seq_printf(s, "\n%s = %s\n", PROC_FILE_VRT_SEL, str);
	return 0;
}

static int proc_vrt_sel_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_vrt_sel_show, PDE_DATA(inode));
}

static ssize_t proc_vrt_sel_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	char buf[20];
	u32 tmp, val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	tmp = readl(com + U3P_USBPHYACR1);
	tmp &= ~PA1_RG_VRT_SEL;
	tmp |= PA1_RG_VRT_SEL_VAL(val);
	writel(tmp, com + U3P_USBPHYACR1);

	return count;
}

static const struct file_operations proc_vrt_sel_fops = {
	.open = proc_vrt_sel_open,
	.write = proc_vrt_sel_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int proc_phy_rev6_show(struct seq_file *s, void *unused)
{
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 tmp;
	char str[16];

	tmp = readl(com + U3P_USBPHYACR6);
	tmp >>= PA6_RG_U2_PHY_REV6_OFET;
	tmp &= PA6_RG_U2_PHY_REV6_MASK;

	cover_val_to_str(tmp, 2, str);

	seq_printf(s, "\n%s = %s\n", PROC_FILE_PHY_REV6, str);
	return 0;
}

static int proc_phy_rev6_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_phy_rev6_show, PDE_DATA(inode));
}

static ssize_t proc_phy_rev6_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	char buf[20];
	u32 tmp, val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	tmp = readl(com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_PHY_REV6;
	tmp |= PA6_RG_U2_PHY_REV6_VAL(val);
	writel(tmp, com + U3P_USBPHYACR6);

	return count;
}

static const struct file_operations proc_phy_rev6_fops = {
	.open = proc_phy_rev6_open,
	.write = proc_phy_rev6_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int proc_discth_show(struct seq_file *s, void *unused)
{
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 tmp;
	char str[16];

	tmp = readl(com + U3P_USBPHYACR6);
	tmp >>= PA6_RG_U2_DISCTH_OFET;
	tmp &= PA6_RG_U2_DISCTH_MASK;

	cover_val_to_str(tmp, 4, str);

	seq_printf(s, "\n%s = %s\n", PROC_FILE_DISCTH, str);
	return 0;
}

static int proc_discth_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_discth_show, PDE_DATA(inode));
}

static ssize_t proc_discth_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct mtk_phy_instance *instance = s->private;
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	char buf[20];
	u32 tmp, val;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 2, &val))
		return -EINVAL;

	tmp = readl(com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_DISCTH;
	tmp |= PA6_RG_U2_DISCTH_VAL(val);
	writel(tmp, com + U3P_USBPHYACR6);

	return count;
}

static const struct file_operations proc_discth_fops = {
	.open = proc_discth_open,
	.write = proc_discth_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int u2_phy_procfs_init(struct mtk_tphy *tphy,
			struct mtk_phy_instance *instance)
{
	struct device *dev = &instance->phy->dev;
	struct proc_dir_entry *root = tphy->root;
	struct proc_dir_entry *phy_root;
	struct proc_dir_entry *file;
	int ret;

	if (!root) {
		dev_info(dev, "proc root not exist\n");
		ret = -ENOMEM;
		goto err0;
	}

	phy_root = proc_mkdir("u2_phy", root);
	if (!root) {
		dev_info(dev, "failed to creat dir proc /u2_phy\n");
		ret = -ENOMEM;
		goto err0;
	}

	file = proc_create_data(PROC_FILE_TERM_SEL, 0644,
			phy_root, &proc_term_sel_fops, instance);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", PROC_FILE_TERM_SEL);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(PROC_FILE_VRT_SEL, 0644,
			phy_root, &proc_vrt_sel_fops, instance);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", PROC_FILE_VRT_SEL);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(PROC_FILE_PHY_REV6, 0644,
			phy_root, &proc_phy_rev6_fops, instance);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", PROC_FILE_PHY_REV6);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(PROC_FILE_DISCTH, 0644,
			phy_root, &proc_discth_fops, instance);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", PROC_FILE_DISCTH);
		ret = -ENOMEM;
		goto err1;
	}

	instance->phy_root = phy_root;
	return 0;
err1:
	proc_remove(phy_root);

err0:
	return ret;
}

static int u2_phy_procfs_exit(struct mtk_phy_instance *instance)
{
	proc_remove(instance->phy_root);
	return 0;
}

static int mtk_phy_procfs_init(struct mtk_tphy *tphy)
{
	struct proc_dir_entry *root = NULL;

	proc_mkdir("mtk_usb", NULL);

	root = proc_mkdir("mtk_usb/usb-phy0", NULL);
	if (!root) {
		dev_info(tphy->dev, "failed to creat usb-phy0  dir\n");
		return -ENOMEM;
	}

	tphy->root = root;
	return 0;
}

static int mtk_phy_procfs_exit(struct mtk_tphy *tphy)
{
	proc_remove(tphy->root);
	return 0;
}

static int phy_efuse_set(struct mtk_phy_instance *instance,
			     enum mtk_phy_efuse type)
{
	struct device *dev = &instance->phy->dev;
	struct device_node *np = dev->of_node;
	struct u2phy_banks *u2_banks;
	struct u3phy_banks *u3_banks;
	u32 val, tmp, mask;
	int index = 0, ret = 0;

	index = of_property_match_string(np,
			"nvmem-cell-names", efuse_name[type]);
	if (index < 0)
		return index;

	ret = of_property_read_u32_index(np, "nvmem-cell-masks",
			index, &mask);
	if (ret)
		return ret;

	ret = nvmem_cell_read_u32(dev, efuse_name[type], &val);
	if (ret)
		return ret;

	if (!val || !mask)
		return 0;

	val = (val & mask) >> (ffs(mask) - 1);
	dev_info(dev, "%s, %s=0x%x\n", __func__, efuse_name[type], val);

	switch (type) {
	case INTR_CAL:
		u2_banks = &instance->u2_banks;
		tmp = readl(u2_banks->com + U3P_USBPHYACR1);
		tmp &= ~PA1_RG_U2_INTR_CAL;
		tmp |= PA1_RG_U2_INTR_CAL_VAL(val);
		writel(tmp, u2_banks->com + U3P_USBPHYACR1);
		break;
	case IEXT_INTR_CTRL:
		u3_banks = &instance->u3_banks;
		tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG0);
		tmp &= ~P3A_RG_SSUSB_IEXT_INTR_CTRL;
		tmp |= P3A_RG_SSUSB_IEXT_INTR_CTRL_VAL(val);
		writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG0);
		break;
	case RX_IMPSEL:
		u3_banks = &instance->u3_banks;
		tmp = readl(u3_banks->phyd + U3P_U3_PHYD_IMPCAL1);
		tmp &= ~P3D_RG_SSUSB_RX_IMPSEL;
		tmp |= P3D_RG_SSUSB_RX_IMPSEL_VAL(val);
		writel(tmp, u3_banks->phyd + U3P_U3_PHYD_IMPCAL1);
		break;
	case TX_IMPSEL:
		u3_banks = &instance->u3_banks;
		tmp = readl(u3_banks->phyd + U3P_U3_PHYD_IMPCAL0);
		tmp &= ~P3D_RG_SSUSB_TX_IMPSEL;
		tmp |= P3D_RG_SSUSB_TX_IMPSEL_VAL(val);
		writel(tmp, u3_banks->phyd + U3P_U3_PHYD_IMPCAL0);
		break;
	default:
		return 0;
	}

	return 0;
}

static void u2_phy_efuse_set(struct mtk_tphy *tphy,
			     struct mtk_phy_instance *instance)
{
	phy_efuse_set(instance, INTR_CAL);
}

static void u3_phy_efuse_set(struct mtk_tphy *tphy,
			     struct mtk_phy_instance *instance)
{
	phy_efuse_set(instance, IEXT_INTR_CTRL);
	phy_efuse_set(instance, RX_IMPSEL);
	phy_efuse_set(instance, TX_IMPSEL);
}

static void hs_slew_rate_calibrate(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *fmreg = u2_banks->fmreg;
	void __iomem *com = u2_banks->com;
	int calibration_val;
	int fm_out;
	u32 tmp;

	/* use force value */
	if (instance->eye_src)
		return;

	/* enable USB ring oscillator */
	tmp = readl(com + U3P_USBPHYACR5);
	tmp |= PA5_RG_U2_HSTX_SRCAL_EN;
	writel(tmp, com + U3P_USBPHYACR5);
	udelay(1);

	/*enable free run clock */
	tmp = readl(fmreg + U3P_U2FREQ_FMMONR1);
	tmp |= P2F_RG_FRCK_EN;
	writel(tmp, fmreg + U3P_U2FREQ_FMMONR1);

	/* set cycle count as 1024, and select u2 channel */
	tmp = readl(fmreg + U3P_U2FREQ_FMCR0);
	tmp &= ~(P2F_RG_CYCLECNT | P2F_RG_MONCLK_SEL);
	tmp |= P2F_RG_CYCLECNT_VAL(U3P_FM_DET_CYCLE_CNT);
	if (tphy->pdata->version == MTK_PHY_V1)
		tmp |= P2F_RG_MONCLK_SEL_VAL(instance->index >> 1);

	writel(tmp, fmreg + U3P_U2FREQ_FMCR0);

	/* enable frequency meter */
	tmp = readl(fmreg + U3P_U2FREQ_FMCR0);
	tmp |= P2F_RG_FREQDET_EN;
	writel(tmp, fmreg + U3P_U2FREQ_FMCR0);

	/* ignore return value */
	readl_poll_timeout_atomic(fmreg + U3P_U2FREQ_FMMONR1, tmp,
			   (tmp & P2F_USB_FM_VALID), 10, 200);

	fm_out = readl(fmreg + U3P_U2FREQ_VALUE);

	/* disable frequency meter */
	tmp = readl(fmreg + U3P_U2FREQ_FMCR0);
	tmp &= ~P2F_RG_FREQDET_EN;
	writel(tmp, fmreg + U3P_U2FREQ_FMCR0);

	/*disable free run clock */
	tmp = readl(fmreg + U3P_U2FREQ_FMMONR1);
	tmp &= ~P2F_RG_FRCK_EN;
	writel(tmp, fmreg + U3P_U2FREQ_FMMONR1);

	if (fm_out) {
		/* ( 1024 / FM_OUT ) x reference clock frequency x coef */
		tmp = tphy->src_ref_clk * tphy->src_coef;
		tmp = (tmp * U3P_FM_DET_CYCLE_CNT) / fm_out;
		calibration_val = DIV_ROUND_CLOSEST(tmp, U3P_SR_COEF_DIVISOR);
	} else {
		/* if FM detection fail, set default value */
		calibration_val = 4;
	}
	dev_dbg(tphy->dev, "phy:%d, fm_out:%d, calib:%d (clk:%d, coef:%d)\n",
		instance->index, fm_out, calibration_val,
		tphy->src_ref_clk, tphy->src_coef);

	/* set HS slew rate */
	tmp = readl(com + U3P_USBPHYACR5);
	tmp &= ~PA5_RG_U2_HSTX_SRCTRL;
	tmp |= PA5_RG_U2_HSTX_SRCTRL_VAL(calibration_val);
	writel(tmp, com + U3P_USBPHYACR5);

	/* disable USB ring oscillator */
	tmp = readl(com + U3P_USBPHYACR5);
	tmp &= ~PA5_RG_U2_HSTX_SRCAL_EN;
	writel(tmp, com + U3P_USBPHYACR5);
}

static void u3_phy_instance_init(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	u32 tmp;

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG1);
	tmp |= RG_SSUSB_VA_ON;
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG1);

	/* gating PCIe Analog XTAL clock */
	tmp = readl(u3_banks->spllc + U3P_SPLLC_XTALCTL3);
	tmp |= XC3_RG_U3_XTAL_RX_PWD | XC3_RG_U3_FRC_XTAL_RX_PWD;
	writel(tmp, u3_banks->spllc + U3P_SPLLC_XTALCTL3);

	/* gating XSQ */
	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG0);
	tmp &= ~P3A_RG_XTAL_EXT_EN_U3;
	tmp |= P3A_RG_XTAL_EXT_EN_U3_VAL(2);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG0);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG9);
	tmp &= ~P3A_RG_RX_DAC_MUX;
	tmp |= P3A_RG_RX_DAC_MUX_VAL(4);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG9);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG6);
	tmp &= ~P3A_RG_TX_EIDLE_CM;
	tmp |= P3A_RG_TX_EIDLE_CM_VAL(0xe);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG6);

	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_CDR1);
	tmp &= ~(P3D_RG_CDR_BIR_LTD0 | P3D_RG_CDR_BIR_LTD1);
	tmp |= P3D_RG_CDR_BIR_LTD0_VAL(0xc) | P3D_RG_CDR_BIR_LTD1_VAL(0x3);
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_CDR1);

	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_LFPS1);
	tmp &= ~P3D_RG_FWAKE_TH;
	tmp |= P3D_RG_FWAKE_TH_VAL(0x34);
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_LFPS1);

	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RXDET1);
	tmp &= ~P3D_RG_RXDET_STB2_SET;
	tmp |= P3D_RG_RXDET_STB2_SET_VAL(0x10);
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RXDET1);

	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RXDET2);
	tmp &= ~P3D_RG_RXDET_STB2_SET_P3;
	tmp |= P3D_RG_RXDET_STB2_SET_P3_VAL(0x10);
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RXDET2);

	dev_dbg(tphy->dev, "%s(%d)\n", __func__, instance->index);
}

static void u2_phy_instance_init(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 index = instance->index;
	u32 tmp;

	/* switch to USB function, and enable usb pll */
	tmp = readl(com + U3P_U2PHYDTM0);
	tmp &= ~(P2C_FORCE_UART_EN | P2C_FORCE_SUSPENDM);
	tmp |= P2C_RG_XCVRSEL_VAL(1) | P2C_RG_DATAIN_VAL(0);
	writel(tmp, com + U3P_U2PHYDTM0);

	tmp = readl(com + U3P_U2PHYDTM1);
	tmp &= ~P2C_RG_UART_EN;
	writel(tmp, com + U3P_U2PHYDTM1);

	tmp = readl(com + U3P_USBPHYACR0);
	tmp |= PA0_RG_USB20_INTR_EN;
	writel(tmp, com + U3P_USBPHYACR0);

	/* disable switch 100uA current to SSUSB */
	tmp = readl(com + U3P_USBPHYACR5);
	tmp &= ~PA5_RG_U2_HS_100U_U3_EN;
	writel(tmp, com + U3P_USBPHYACR5);

	if (!index) {
		tmp = readl(com + U3P_U2PHYACR4);
		tmp &= ~P2C_U2_GPIO_CTR_MSK;
		writel(tmp, com + U3P_U2PHYACR4);
	}

	if (tphy->pdata->avoid_rx_sen_degradation) {
		if (!index) {
			tmp = readl(com + U3P_USBPHYACR2);
			tmp |= PA2_RG_SIF_U2PLL_FORCE_EN;
			writel(tmp, com + U3P_USBPHYACR2);

			tmp = readl(com + U3D_U2PHYDCR0);
			tmp &= ~P2C_RG_SIF_U2PLL_FORCE_ON;
			writel(tmp, com + U3D_U2PHYDCR0);
		} else {
			tmp = readl(com + U3D_U2PHYDCR0);
			tmp |= P2C_RG_SIF_U2PLL_FORCE_ON;
			writel(tmp, com + U3D_U2PHYDCR0);

			tmp = readl(com + U3P_U2PHYDTM0);
			tmp |= P2C_RG_SUSPENDM | P2C_FORCE_SUSPENDM;
			writel(tmp, com + U3P_U2PHYDTM0);
		}
	}

	tmp = readl(com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_BC11_SW_EN;	/* DP/DM BC1.1 path Disable */
	tmp &= ~PA6_RG_U2_SQTH;
	tmp |= PA6_RG_U2_SQTH_VAL(2);
	writel(tmp, com + U3P_USBPHYACR6);

	dev_dbg(tphy->dev, "%s(%d)\n", __func__, index);
}

static void u2_phy_instance_power_on(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 index = instance->index;
	u32 tmp;

	tmp = readl(com + U3P_U2PHYDTM0);

#ifndef CONFIG_MACH_MT6781
	tmp |= P2C_FORCE_SUSPENDM;
	writel(tmp, com + U3P_U2PHYDTM0);

	tmp = readl(com + U3P_U2PHYDTM0);
	tmp &= ~P2C_RG_SUSPENDM;
	writel(tmp, com + U3P_U2PHYDTM0);

	tmp = readl(com + U3P_U2PHYDTM0);
	tmp |= P2C_RG_SUSPENDM;
	writel(tmp, com + U3P_U2PHYDTM0);

	udelay(30);

	tmp = readl(com + U3P_U2PHYDTM0);
	tmp &= ~P2C_FORCE_SUSPENDM;
	writel(tmp, com + U3P_U2PHYDTM0);

	tmp = readl(com + U3P_U2PHYDTM0);
	tmp &= ~P2C_RG_SUSPENDM;
	writel(tmp, com + U3P_U2PHYDTM0);

	tmp = readl(com + U3P_U2PHYDTM0);
	tmp &= ~(P2C_FORCE_UART_EN);
	writel(tmp, com + U3P_U2PHYDTM0);

	tmp = readl(com + U3P_U2PHYDTM1);
	tmp &= ~P2C_RG_UART_EN;
	writel(tmp, com + U3P_U2PHYDTM1);

	tmp = readl(com + U3P_U2PHYACR4);
	tmp &= ~P2C_U2_GPIO_CTR_MSK;
	writel(tmp, com + U3P_U2PHYACR4);

	tmp = readl(com + U3P_U2PHYDTM0);
	tmp &= ~P2C_FORCE_SUSPENDM;
	writel(tmp, com + U3P_U2PHYDTM0);

	tmp = readl(com + U3P_U2PHYDTM0);
#endif
	tmp &= ~(P2C_RG_XCVRSEL | P2C_RG_DATAIN | P2C_DTM0_PART_MASK);
	writel(tmp, com + U3P_U2PHYDTM0);

#ifndef CONFIG_MACH_MT6781
	tmp = readl(com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_BC11_SW_EN;
	writel(tmp, com + U3P_USBPHYACR6);
#endif

	/* OTG Enable */
	tmp = readl(com + U3P_USBPHYACR6);
	tmp |= PA6_RG_U2_OTG_VBUSCMP_EN;
	writel(tmp, com + U3P_USBPHYACR6);

	tmp = readl(com + U3P_U2PHYDTM1);
	tmp |= P2C_RG_VBUSVALID | P2C_RG_AVALID;
	tmp &= ~P2C_RG_SESSEND;
	writel(tmp, com + U3P_U2PHYDTM1);

#ifndef CONFIG_MACH_MT6781
	tmp = readl(com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_PHY_REV6;
	tmp |= PA6_RG_U2_PHY_REV6_VAL(1);
	writel(tmp, com + U3P_USBPHYACR6);

	udelay(800);
#endif

	if (tphy->pdata->avoid_rx_sen_degradation && index) {
		tmp = readl(com + U3D_U2PHYDCR0);
		tmp |= P2C_RG_SIF_U2PLL_FORCE_ON;
		writel(tmp, com + U3D_U2PHYDCR0);

		tmp = readl(com + U3P_U2PHYDTM0);
		tmp |= P2C_RG_SUSPENDM | P2C_FORCE_SUSPENDM;
		writel(tmp, com + U3P_U2PHYDTM0);
	}

#ifdef CONFIG_MACH_MT6781
	/* set SW_BC11_EN as 0 which is usb control DPDM */
	u2_phy_instance_set_mode_ext(tphy, instance, PHY_MODE_BC11_SW_CLR);
#endif

#ifdef CONFIG_USB_MTK_HDRC
	/* Used by phone products */
	/* HQA Setting */
	tmp = readl(com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_DISCTH;
	tmp |= PA6_RG_U2_DISCTH_VAL(0xf);
	writel(tmp, com + U3P_USBPHYACR6);
#endif

	dev_info(tphy->dev, "%s(%d)\n", __func__, index);
}

static void u2_phy_instance_power_off(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 index = instance->index;
	u32 tmp;

	tmp = readl(com + U3P_U2PHYDTM0);
#ifdef CONFIG_MACH_MT6781
	tmp &= ~(P2C_RG_XCVRSEL | P2C_RG_DATAIN);
	tmp |= P2C_RG_XCVRSEL_VAL(1) | P2C_DTM0_PART_MASK2;
#if defined(CONFIG_MACH_MT6739)
	dev_info(tphy->dev, "%s, write DTM0 SUSPENDM\n", __func__);
	tmp |= P2C_RG_SUSPENDM;
#endif
#else
	tmp &= ~(P2C_FORCE_UART_EN);
#endif

	writel(tmp, com + U3P_U2PHYDTM0);

#ifndef CONFIG_MACH_MT6781
	tmp = readl(com + U3P_U2PHYDTM1);
	tmp &= ~P2C_RG_UART_EN;
	writel(tmp, com + U3P_U2PHYDTM1);

	tmp = readl(com + U3P_U2PHYACR4);
	tmp &= ~P2C_U2_GPIO_CTR_MSK;
	writel(tmp, com + U3P_U2PHYACR4);

	tmp = readl(com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_BC11_SW_EN;
	writel(tmp, com + U3P_USBPHYACR6);
#endif

	/* OTG Disable */
	tmp = readl(com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_OTG_VBUSCMP_EN;
	writel(tmp, com + U3P_USBPHYACR6);

	tmp = readl(com + U3P_U2PHYDTM1);
	tmp &= ~(P2C_RG_VBUSVALID | P2C_RG_AVALID);
	tmp |= P2C_RG_SESSEND;
	writel(tmp, com + U3P_U2PHYDTM1);

#ifndef CONFIG_MACH_MT6781
	tmp = readl(com + U3P_U2PHYDTM0);
	tmp |= P2C_RG_SUSPENDM | P2C_FORCE_SUSPENDM;
	writel(tmp, com + U3P_U2PHYDTM0);

	mdelay(2);

	tmp = readl(com + U3P_U2PHYDTM0);
	tmp &= ~P2C_RG_DATAIN;
	tmp |= (P2C_RG_XCVRSEL_VAL(1) | P2C_DTM0_PART_MASK);
	writel(tmp, com + U3P_U2PHYDTM0);

	tmp = readl(com + U3P_USBPHYACR6);
	tmp |= PA6_RG_U2_PHY_REV6_VAL(1);
	writel(tmp, com + U3P_USBPHYACR6);

	udelay(800);

	tmp = readl(com + U3P_U2PHYDTM0);
	tmp &= ~P2C_RG_SUSPENDM;
	writel(tmp, com + U3P_U2PHYDTM0);

	udelay(1);
#endif

	if (tphy->pdata->avoid_rx_sen_degradation && index) {
		tmp = readl(com + U3P_U2PHYDTM0);
		tmp &= ~(P2C_RG_SUSPENDM | P2C_FORCE_SUSPENDM);
		writel(tmp, com + U3P_U2PHYDTM0);

		tmp = readl(com + U3D_U2PHYDCR0);
		tmp &= ~P2C_RG_SIF_U2PLL_FORCE_ON;
		writel(tmp, com + U3D_U2PHYDCR0);
	}

#ifdef CONFIG_MACH_MT6781
	/*
	 * set SW_BC11_EN as 0 which is charger control DPDM
	 * to disable USB DPDM
	 */
	u2_phy_instance_set_mode_ext(tphy, instance, PHY_MODE_BC11_SW_SET);
#endif
	dev_info(tphy->dev, "%s(%d)\n", __func__, index);
}

static void u2_phy_instance_exit(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 index = instance->index;
	u32 tmp;

	if (tphy->pdata->avoid_rx_sen_degradation && index) {
		tmp = readl(com + U3D_U2PHYDCR0);
		tmp &= ~P2C_RG_SIF_U2PLL_FORCE_ON;
		writel(tmp, com + U3D_U2PHYDCR0);

		tmp = readl(com + U3P_U2PHYDTM0);
		tmp &= ~P2C_FORCE_SUSPENDM;
		writel(tmp, com + U3P_U2PHYDTM0);
	}
}

static void u2_phy_instance_set_mode_2uart(struct u2phy_banks *u2_banks)
{
	u32 tmp;

	/* Clear PA6_RG_U2_BC11_SW_EN */
	tmp = readl(u2_banks->com + U3P_USBPHYACR6);
	tmp &= ~(PA6_RG_U2_BC11_SW_EN);
	writel(tmp, u2_banks->com + U3P_USBPHYACR6);

	/* Set P2C_RG_SUSPENDM */
	tmp = readl(u2_banks->com + U3P_U2PHYDTM0);
	tmp |= P2C_RG_SUSPENDM;
	writel(tmp, u2_banks->com + U3P_U2PHYDTM0);

	/* Set P2C_FORCE_SUSPENDM */
	tmp = readl(u2_banks->com + U3P_U2PHYDTM0);
	tmp |= P2C_FORCE_SUSPENDM;
	writel(tmp, u2_banks->com + U3P_U2PHYDTM0);

	/* Clear and Set P2C_RG_UART_MODE to 2'b01 */
	tmp = readl(u2_banks->com + U3P_U2PHYDTM0);
	tmp &= ~(P2C_RG_UART_MODE);
	tmp |= P2C_RG_UART_MODE_VAL(0x1);
	writel(tmp, u2_banks->com + U3P_U2PHYDTM0);

	/* Clear P2C_FORCE_UART_I */
	tmp = readl(u2_banks->com + U3P_U2PHYDTM0);
	tmp &= ~(P2C_FORCE_UART_I);
	writel(tmp, u2_banks->com + U3P_U2PHYDTM0);

	/* Set P2C_FORCE_UART_BIAS_EN */
	tmp = readl(u2_banks->com + U3P_U2PHYDTM0);
	tmp |= P2C_FORCE_UART_BIAS_EN;
	writel(tmp, u2_banks->com + U3P_U2PHYDTM0);

	/* Set P2C_FORCE_UART_TX_OE */
	tmp = readl(u2_banks->com + U3P_U2PHYDTM0);
	tmp |= P2C_FORCE_UART_TX_OE;
	writel(tmp, u2_banks->com + U3P_U2PHYDTM0);

	/* Set P2C_FORCE_UART_EN */
	tmp = readl(u2_banks->com + U3P_U2PHYDTM0);
	tmp |= P2C_FORCE_UART_EN;
	writel(tmp, u2_banks->com + U3P_U2PHYDTM0);

	/* Set P2C_RG_UART_BIAS_EN */
	tmp = readl(u2_banks->com + U3P_U2PHYDTM0);
	tmp |= P2C_RG_UART_BIAS_EN;
	writel(tmp, u2_banks->com + U3P_U2PHYDTM0);

	/* Set P2C_RG_UART_TX_OE */
	tmp = readl(u2_banks->com + U3P_U2PHYDTM0);
	tmp |= P2C_RG_UART_TX_OE;
	writel(tmp, u2_banks->com + U3P_U2PHYDTM0);

	/* Set P2C_RG_UART_EN */
	tmp = readl(u2_banks->com + U3P_U2PHYDTM0);
	tmp |= P2C_RG_UART_EN;
	writel(tmp, u2_banks->com + U3P_U2PHYDTM0);

	/* Set P2C_RG_USB20_DM_100K_EN */
	tmp = readl(u2_banks->com + U3P_U2PHYACR4);
	tmp |= P2C_RG_USB20_DM_100K_EN;
	writel(tmp, u2_banks->com + U3P_U2PHYACR4);

	/* Clear P2C_RG_DMPULLDOWN, P2C_RG_DPPULLDOWN */
	tmp = readl(u2_banks->com + U3P_U2PHYDTM0);
	tmp &= ~(P2C_RG_DPPULLDOWN | P2C_RG_DMPULLDOWN);
	writel(tmp, u2_banks->com + U3P_U2PHYDTM0);
}

static void u2_phy_instance_set_mode_2usb(struct u2phy_banks *u2_banks)
{
	u32 tmp;

	/* Cear P2C_FORCE_UART_EN and P2C_RG_UART_MODE */
	tmp = readl(u2_banks->com + U3P_U2PHYDTM0);
	tmp &= ~(P2C_FORCE_UART_EN | P2C_RG_UART_MODE);
	writel(tmp, u2_banks->com + U3P_U2PHYDTM0);
}

static int u2_phy_instance_get_mode_ext(struct mtk_tphy *tphy, struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	u32 tmp;

	tmp = readl(u2_banks->com + U3P_U2PHYDTM0);

	if ((tmp & P2C_RG_UART_MODE) >> P2C_RG_UART_MODE_OFET)
		return PHY_MODE_UART;
	else
		return PHY_MODE_USB_OTG;
}

static void u2_phy_instance_set_mode(struct mtk_tphy *tphy,
				     struct mtk_phy_instance *instance,
				     enum phy_mode mode)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	struct device *dev = &instance->phy->dev;
	u32 tmp;

	tmp = readl(u2_banks->com + U3P_U2PHYDTM1);
	switch (mode) {
	case PHY_MODE_UART:
		u2_phy_instance_set_mode_2uart(u2_banks);
		return;
	case PHY_MODE_USB_DEVICE:
		tmp |= P2C_FORCE_IDDIG | P2C_RG_IDDIG;
		device_property_read_u32(dev, "mediatek,eye-src",
				 &instance->eye_src);
		device_property_read_u32(dev, "mediatek,eye-vrt",
				 &instance->eye_vrt);
		device_property_read_u32(dev, "mediatek,eye-term",
				 &instance->eye_term);
		device_property_read_u32(dev, "mediatek,eye-rev6",
				 &instance->eye_rev6);
		device_property_read_u32(dev, "mediatek,eye-disc",
				 &instance->eye_disc);
		u2_phy_props_set(tphy, instance);
		break;
	case PHY_MODE_USB_HOST:
		tmp |= P2C_FORCE_IDDIG;
		tmp &= ~P2C_RG_IDDIG;
#ifdef	CONFIG_USB_MTK_HDRC
		/* Used by phone products */
		tmp |= P2C_RG_VBUSVALID | P2C_RG_BVALID | P2C_RG_AVALID;
		tmp &= ~P2C_RG_SESSEND;
#endif
		device_property_read_u32(dev, "mediatek,host-eye-src",
				 &instance->eye_src);
		device_property_read_u32(dev, "mediatek,host-eye-vrt",
				 &instance->eye_vrt);
		device_property_read_u32(dev, "mediatek,host-eye-term",
				 &instance->eye_term);
		device_property_read_u32(dev, "mediatek,host-eye-rev6",
				 &instance->eye_rev6);
		device_property_read_u32(dev, "mediatek,host-eye-disc",
				 &instance->eye_disc);
		u2_phy_props_set(tphy, instance);
		break;
	case PHY_MODE_USB_OTG:
		u2_phy_instance_set_mode_2usb(u2_banks);
		tmp &= ~(P2C_FORCE_IDDIG | P2C_RG_IDDIG);
		break;
	case PHY_MODE_INVALID:
		/* Used by phone products */
		tmp |= P2C_RG_SESSEND | P2C_RG_RG_IDPULLUP;
		tmp &= ~(P2C_RG_VBUSVALID | P2C_RG_BVALID | P2C_RG_AVALID |
			P2C_RG_IDDIG);
		tmp |= P2C_FORCE_IDDIG;
		break;
	default:
		return;
	}
#ifdef	CONFIG_USB_MTK_HDRC
	/* Used by phone products */
	tmp |= P2C_FORCE_VBUSVALID | P2C_FORCE_SESSEND | P2C_FORCE_BVALID |
		P2C_FORCE_AVALID | P2C_FORCE_IDPULLUP;
#endif
	writel(tmp, u2_banks->com + U3P_U2PHYDTM1);
}

static void u2_phy_instance_set_mode_ext(struct mtk_tphy *tphy,
				     struct mtk_phy_instance *instance,
				     int submode)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	u32 tmp;

	dev_info(tphy->dev, "%s submode(%d)\n", __func__, submode);

	switch (submode) {
	case PHY_MODE_BC11_SW_SET:
		tmp = readl(u2_banks->com + U3P_USBPHYACR6);
		tmp |= PA6_RG_U2_BC11_SW_EN;
		writel(tmp, u2_banks->com + U3P_USBPHYACR6);
		break;
	case PHY_MODE_BC11_SW_CLR:
		tmp = readl(u2_banks->com + U3P_USBPHYACR6);
		tmp &= ~PA6_RG_U2_BC11_SW_EN;
		writel(tmp, u2_banks->com + U3P_USBPHYACR6);
		break;

#ifndef CONFIG_MACH_MT6781
	case PHY_MODE_DPDMPULLDOWN_SET:
		tmp = readl(u2_banks->com + U3P_U2PHYDTM0);
		tmp |= P2C_RG_DPPULLDOWN | P2C_RG_DMPULLDOWN;
		writel(tmp, u2_banks->com + U3P_U2PHYDTM0);

		tmp = readl(u2_banks->com + U3P_USBPHYACR6);
		tmp &= ~PA6_RG_U2_PHY_REV1;
		writel(tmp, u2_banks->com + U3P_USBPHYACR6);

		tmp = readl(u2_banks->com + U3P_USBPHYACR6);
		tmp &= ~PA6_RG_U2_BC11_SW_EN;
		writel(tmp, u2_banks->com + U3P_USBPHYACR6);
		break;
	case PHY_MODE_DPDMPULLDOWN_CLR:
		tmp = readl(u2_banks->com + U3P_U2PHYDTM0);
		tmp &= ~(P2C_RG_DPPULLDOWN | P2C_RG_DMPULLDOWN);
		writel(tmp, u2_banks->com + U3P_U2PHYDTM0);

		tmp = readl(u2_banks->com + U3P_USBPHYACR6);
		tmp |= PA6_RG_U2_PHY_REV1;
		writel(tmp, u2_banks->com + U3P_USBPHYACR6);

		tmp = readl(u2_banks->com + U3P_USBPHYACR6);
		tmp |= PA6_RG_U2_BC11_SW_EN;
		writel(tmp, u2_banks->com + U3P_USBPHYACR6);
		break;
	case PHY_MODE_DPPULLUP_SET:
		tmp = readl(u2_banks->com + U3P_USBPHYACR3);
		tmp |= PA3_RG_USB20_PUPD_BIST_EN |
			PA3_RG_USB20_EN_PU_DP;
		writel(tmp, u2_banks->com + U3P_USBPHYACR3);
		break;
	case PHY_MODE_DPPULLUP_CLR:
		tmp = readl(u2_banks->com + U3P_USBPHYACR3);
		tmp &= ~(PA3_RG_USB20_PUPD_BIST_EN |
			PA3_RG_USB20_EN_PU_DP);
		writel(tmp, u2_banks->com + U3P_USBPHYACR3);
		break;
#endif
	default:
		return;
	}
}

static void u3_phy_instance_power_on(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *bank = &instance->u3_banks;
	u32 index = instance->index;
	u32 tmp;

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLD);
	tmp &= ~(P3C_FORCE_IP_SW_RST | P3C_REG_IP_SW_RST);
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLD);

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLE);
	tmp &= ~(P3C_RG_SWRST_U3_PHYD_FORCE_EN | P3C_RG_SWRST_U3_PHYD);
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLE);

	dev_info(tphy->dev, "%s(%d)\n", __func__, index);
}

static void u3_phy_instance_power_off(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *bank = &instance->u3_banks;
	u32 index = instance->index;
	u32 tmp;

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLD);
	tmp |= P3C_FORCE_IP_SW_RST | P3C_REG_IP_SW_RST;
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLD);

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLE);
	tmp |= P3C_RG_SWRST_U3_PHYD_FORCE_EN | P3C_RG_SWRST_U3_PHYD;
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLE);

	dev_info(tphy->dev, "%s(%d)\n", __func__, index);
}

static void pcie_phy_instance_init(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	u32 tmp;

	if (tphy->pdata->version != MTK_PHY_V1)
		return;

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG0);
	tmp &= ~(P3A_RG_XTAL_EXT_PE1H | P3A_RG_XTAL_EXT_PE2H);
	tmp |= P3A_RG_XTAL_EXT_PE1H_VAL(0x2) | P3A_RG_XTAL_EXT_PE2H_VAL(0x2);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG0);

	/* ref clk drive */
	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG1);
	tmp &= ~P3A_RG_CLKDRV_AMP;
	tmp |= P3A_RG_CLKDRV_AMP_VAL(0x4);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG1);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG0);
	tmp &= ~P3A_RG_CLKDRV_OFF;
	tmp |= P3A_RG_CLKDRV_OFF_VAL(0x1);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG0);

	/* SSC delta -5000ppm */
	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG20);
	tmp &= ~P3A_RG_PLL_DELTA1_PE2H;
	tmp |= P3A_RG_PLL_DELTA1_PE2H_VAL(0x3c);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG20);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG25);
	tmp &= ~P3A_RG_PLL_DELTA_PE2H;
	tmp |= P3A_RG_PLL_DELTA_PE2H_VAL(0x36);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG25);

	/* change pll BW 0.6M */
	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG5);
	tmp &= ~(P3A_RG_PLL_BR_PE2H | P3A_RG_PLL_IC_PE2H);
	tmp |= P3A_RG_PLL_BR_PE2H_VAL(0x1) | P3A_RG_PLL_IC_PE2H_VAL(0x1);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG5);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG4);
	tmp &= ~(P3A_RG_PLL_DIVEN_PE2H | P3A_RG_PLL_BC_PE2H);
	tmp |= P3A_RG_PLL_BC_PE2H_VAL(0x3);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG4);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG6);
	tmp &= ~P3A_RG_PLL_IR_PE2H;
	tmp |= P3A_RG_PLL_IR_PE2H_VAL(0x2);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG6);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG7);
	tmp &= ~P3A_RG_PLL_BP_PE2H;
	tmp |= P3A_RG_PLL_BP_PE2H_VAL(0xa);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG7);

	/* Tx Detect Rx Timing: 10us -> 5us */
	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RXDET1);
	tmp &= ~P3D_RG_RXDET_STB2_SET;
	tmp |= P3D_RG_RXDET_STB2_SET_VAL(0x10);
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RXDET1);

	tmp = readl(u3_banks->phyd + U3P_U3_PHYD_RXDET2);
	tmp &= ~P3D_RG_RXDET_STB2_SET_P3;
	tmp |= P3D_RG_RXDET_STB2_SET_P3_VAL(0x10);
	writel(tmp, u3_banks->phyd + U3P_U3_PHYD_RXDET2);

	/* wait for PCIe subsys register to active */
	usleep_range(2500, 3000);
	dev_dbg(tphy->dev, "%s(%d)\n", __func__, instance->index);
}

static void pcie_phy_instance_power_on(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *bank = &instance->u3_banks;
	u32 tmp;

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLD);
	tmp &= ~(P3C_FORCE_IP_SW_RST | P3C_REG_IP_SW_RST);
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLD);

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLE);
	tmp &= ~(P3C_RG_SWRST_U3_PHYD_FORCE_EN | P3C_RG_SWRST_U3_PHYD);
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLE);
}

static void pcie_phy_instance_power_off(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)

{
	struct u3phy_banks *bank = &instance->u3_banks;
	u32 tmp;

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLD);
	tmp |= P3C_FORCE_IP_SW_RST | P3C_REG_IP_SW_RST;
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLD);

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLE);
	tmp |= P3C_RG_SWRST_U3_PHYD_FORCE_EN | P3C_RG_SWRST_U3_PHYD;
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLE);
}

static void sata_phy_instance_init(struct mtk_tphy *tphy,
	struct mtk_phy_instance *instance)
{
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	void __iomem *phyd = u3_banks->phyd;
	u32 tmp;

	/* charge current adjustment */
	tmp = readl(phyd + ANA_RG_CTRL_SIGNAL6);
	tmp &= ~(RG_CDR_BIRLTR_GEN1_MSK | RG_CDR_BC_GEN1_MSK);
	tmp |= RG_CDR_BIRLTR_GEN1_VAL(0x6) | RG_CDR_BC_GEN1_VAL(0x1a);
	writel(tmp, phyd + ANA_RG_CTRL_SIGNAL6);

	tmp = readl(phyd + ANA_EQ_EYE_CTRL_SIGNAL4);
	tmp &= ~RG_CDR_BIRLTD0_GEN1_MSK;
	tmp |= RG_CDR_BIRLTD0_GEN1_VAL(0x18);
	writel(tmp, phyd + ANA_EQ_EYE_CTRL_SIGNAL4);

	tmp = readl(phyd + ANA_EQ_EYE_CTRL_SIGNAL5);
	tmp &= ~RG_CDR_BIRLTD0_GEN3_MSK;
	tmp |= RG_CDR_BIRLTD0_GEN3_VAL(0x06);
	writel(tmp, phyd + ANA_EQ_EYE_CTRL_SIGNAL5);

	tmp = readl(phyd + ANA_RG_CTRL_SIGNAL4);
	tmp &= ~(RG_CDR_BICLTR_GEN1_MSK | RG_CDR_BR_GEN2_MSK);
	tmp |= RG_CDR_BICLTR_GEN1_VAL(0x0c) | RG_CDR_BR_GEN2_VAL(0x07);
	writel(tmp, phyd + ANA_RG_CTRL_SIGNAL4);

	tmp = readl(phyd + PHYD_CTRL_SIGNAL_MODE4);
	tmp &= ~(RG_CDR_BICLTD0_GEN1_MSK | RG_CDR_BICLTD1_GEN1_MSK);
	tmp |= RG_CDR_BICLTD0_GEN1_VAL(0x08) | RG_CDR_BICLTD1_GEN1_VAL(0x02);
	writel(tmp, phyd + PHYD_CTRL_SIGNAL_MODE4);

	tmp = readl(phyd + PHYD_DESIGN_OPTION2);
	tmp &= ~RG_LOCK_CNT_SEL_MSK;
	tmp |= RG_LOCK_CNT_SEL_VAL(0x02);
	writel(tmp, phyd + PHYD_DESIGN_OPTION2);

	tmp = readl(phyd + PHYD_DESIGN_OPTION9);
	tmp &= ~(RG_T2_MIN_MSK | RG_TG_MIN_MSK |
		 RG_T2_MAX_MSK | RG_TG_MAX_MSK);
	tmp |= RG_T2_MIN_VAL(0x12) | RG_TG_MIN_VAL(0x04) |
	       RG_T2_MAX_VAL(0x31) | RG_TG_MAX_VAL(0x0e);
	writel(tmp, phyd + PHYD_DESIGN_OPTION9);

	tmp = readl(phyd + ANA_RG_CTRL_SIGNAL1);
	tmp &= ~RG_IDRV_0DB_GEN1_MSK;
	tmp |= RG_IDRV_0DB_GEN1_VAL(0x20);
	writel(tmp, phyd + ANA_RG_CTRL_SIGNAL1);

	tmp = readl(phyd + ANA_EQ_EYE_CTRL_SIGNAL1);
	tmp &= ~RG_EQ_DLEQ_LFI_GEN1_MSK;
	tmp |= RG_EQ_DLEQ_LFI_GEN1_VAL(0x03);
	writel(tmp, phyd + ANA_EQ_EYE_CTRL_SIGNAL1);

	dev_dbg(tphy->dev, "%s(%d)\n", __func__, instance->index);
}

static void phy_v1_banks_init(struct mtk_tphy *tphy,
			      struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	struct u3phy_banks *u3_banks = &instance->u3_banks;

	switch (instance->type) {
	case PHY_TYPE_USB2:
		u2_banks->misc = NULL;
		u2_banks->fmreg = tphy->sif_base + SSUSB_SIFSLV_V1_U2FREQ;
		u2_banks->com = instance->port_base + SSUSB_SIFSLV_V1_U2PHY_COM;
		break;
	case PHY_TYPE_USB3:
	case PHY_TYPE_PCIE:
		u3_banks->spllc = tphy->sif_base + SSUSB_SIFSLV_V1_SPLLC;
		u3_banks->chip = tphy->sif_base + SSUSB_SIFSLV_V1_CHIP;
		u3_banks->phyd = instance->port_base + SSUSB_SIFSLV_V1_U3PHYD;
		u3_banks->phya = instance->port_base + SSUSB_SIFSLV_V1_U3PHYA;
		break;
	case PHY_TYPE_SATA:
		u3_banks->phyd = instance->port_base + SSUSB_SIFSLV_V1_U3PHYD;
		break;
	default:
		dev_err(tphy->dev, "incompatible PHY type\n");
		return;
	}
}

#ifdef CONFIG_MACH_MT6771
static struct mtk_phy_instance *bc11_instance;
#endif

static void phy_v2_banks_init(struct mtk_tphy *tphy,
			      struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	struct u3phy_banks *u3_banks = &instance->u3_banks;

	switch (instance->type) {
	case PHY_TYPE_USB2:
		u2_banks->misc = instance->port_base + SSUSB_SIFSLV_V2_MISC;
		u2_banks->fmreg = instance->port_base + SSUSB_SIFSLV_V2_U2FREQ;
		u2_banks->com = instance->port_base + SSUSB_SIFSLV_V2_U2PHY_COM;
		break;
	case PHY_TYPE_USB3:
	case PHY_TYPE_PCIE:
		u3_banks->spllc = instance->port_base + SSUSB_SIFSLV_V2_SPLLC;
		u3_banks->chip = instance->port_base + SSUSB_SIFSLV_V2_CHIP;
		u3_banks->phyd = instance->port_base + SSUSB_SIFSLV_V2_U3PHYD;
		u3_banks->phya = instance->port_base + SSUSB_SIFSLV_V2_U3PHYA;
		break;
	default:
		dev_err(tphy->dev, "incompatible PHY type\n");
		return;
	}
}

static void phy_parse_property(struct mtk_tphy *tphy,
				struct mtk_phy_instance *instance)
{
	struct device *dev = &instance->phy->dev;

	if (instance->type != PHY_TYPE_USB2)
		return;

	instance->bc12_en = device_property_read_bool(dev, "mediatek,bc12");
	device_property_read_u32(dev, "mediatek,eye-src",
				 &instance->eye_src);
	device_property_read_u32(dev, "mediatek,eye-vrt",
				 &instance->eye_vrt);
	device_property_read_u32(dev, "mediatek,eye-term",
				 &instance->eye_term);
	device_property_read_u32(dev, "mediatek,eye-rev6",
				 &instance->eye_rev6);
	device_property_read_u32(dev, "mediatek,eye-disc",
				 &instance->eye_disc);
	dev_dbg(dev, "bc12:%d, src:%d, vrt:%d, term:%d, rev6:%d, disc:%d\n",
		instance->bc12_en, instance->eye_src,
		instance->eye_vrt, instance->eye_term,
		instance->eye_rev6, instance->eye_disc);
}

static void u2_phy_props_set(struct mtk_tphy *tphy,
			     struct mtk_phy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	struct device *dev = &instance->phy->dev;
	u32 tmp;

	dev_info(dev, "%s, bc12:%d, src:%d, vrt:%d, term:%d, rev6:%d, dis:%d",
		__func__, instance->bc12_en, instance->eye_src,
		instance->eye_vrt, instance->eye_term, instance->eye_rev6,
		instance->eye_disc);

	if (instance->bc12_en) {
		tmp = readl(com + U3P_U2PHYBC12C);
		tmp |= P2C_RG_CHGDT_EN;	/* BC1.2 path Enable */
		writel(tmp, com + U3P_U2PHYBC12C);
	}

	if (instance->eye_src) {
		tmp = readl(com + U3P_USBPHYACR5);
		tmp &= ~PA5_RG_U2_HSTX_SRCTRL;
		tmp |= PA5_RG_U2_HSTX_SRCTRL_VAL(instance->eye_src);
		writel(tmp, com + U3P_USBPHYACR5);
	}

	if (instance->eye_vrt) {
		tmp = readl(com + U3P_USBPHYACR1);
		tmp &= ~PA1_RG_VRT_SEL;
		tmp |= PA1_RG_VRT_SEL_VAL(instance->eye_vrt);
		writel(tmp, com + U3P_USBPHYACR1);
	}

	if (instance->eye_term) {
		tmp = readl(com + U3P_USBPHYACR1);
		tmp &= ~PA1_RG_TERM_SEL;
		tmp |= PA1_RG_TERM_SEL_VAL(instance->eye_term);
		writel(tmp, com + U3P_USBPHYACR1);
	}

	if (instance->eye_rev6) {
		tmp = readl(com + U3P_USBPHYACR6);
		tmp &= ~PA6_RG_U2_PHY_REV6;
		tmp |= PA6_RG_U2_PHY_REV6_VAL(instance->eye_rev6);
		writel(tmp, com + U3P_USBPHYACR6);
	}

	if (instance->eye_disc) {
		tmp = readl(com + U3P_USBPHYACR6);
		tmp &= ~PA6_RG_U2_DISCTH;
		tmp |= PA6_RG_U2_DISCTH_VAL(instance->eye_disc);
		writel(tmp, com + U3P_USBPHYACR6);
	}

#ifdef CONFIG_MACH_MT6771
	if ((tphy->phys[0] == instance) && (instance->type == PHY_TYPE_USB2))
		bc11_instance = instance;
#endif
}

static int mtk_phy_init(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);
	int ret;

	ret = clk_prepare_enable(tphy->u3phya_ref);
	if (ret) {
		dev_err(tphy->dev, "failed to enable u3phya_ref\n");
		return ret;
	}

	ret = clk_prepare_enable(instance->ref_clk);
	if (ret) {
		dev_err(tphy->dev, "failed to enable ref_clk\n");
		return ret;
	}

	ret = u2_phy_instance_get_mode_ext(tphy, instance);
	if (ret == PHY_MODE_UART)
		return 0;

	switch (instance->type) {
	case PHY_TYPE_USB2:
		u2_phy_instance_init(tphy, instance);
		u2_phy_efuse_set(tphy, instance);
		u2_phy_props_set(tphy, instance);
		u2_phy_procfs_init(tphy, instance);
		break;
	case PHY_TYPE_USB3:
		u3_phy_instance_init(tphy, instance);
		u3_phy_efuse_set(tphy, instance);
		u3_phy_procfs_init(tphy, instance);
		break;
	case PHY_TYPE_PCIE:
		pcie_phy_instance_init(tphy, instance);
		break;
	case PHY_TYPE_SATA:
		sata_phy_instance_init(tphy, instance);
		break;
	default:
		dev_err(tphy->dev, "incompatible PHY type\n");
		return -EINVAL;
	}

	return 0;
}

static int mtk_phy_power_on(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_USB2) {
		u2_phy_instance_power_on(tphy, instance);
		hs_slew_rate_calibrate(tphy, instance);
	} else if (instance->type == PHY_TYPE_USB3) {
		u3_phy_instance_power_on(tphy, instance);
	} else if (instance->type == PHY_TYPE_PCIE) {
		pcie_phy_instance_power_on(tphy, instance);
	}

	return 0;
}

static int mtk_phy_power_off(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_USB2)
		u2_phy_instance_power_off(tphy, instance);
	else if (instance->type == PHY_TYPE_USB3)
		u3_phy_instance_power_off(tphy, instance);
	else if (instance->type == PHY_TYPE_PCIE)
		pcie_phy_instance_power_off(tphy, instance);

	return 0;
}

static int mtk_phy_exit(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_USB2) {
		u2_phy_instance_exit(tphy, instance);
		u2_phy_procfs_exit(instance);
	}

	if (instance->type == PHY_TYPE_USB3)
		u3_phy_procfs_exit(instance);

	clk_disable_unprepare(instance->ref_clk);
	clk_disable_unprepare(tphy->u3phya_ref);
	return 0;
}

static int mtk_phy_get_mode_ext(struct phy *phy)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);
	int ret;

	ret = 0;
	if (instance->type == PHY_TYPE_USB2)
		ret = u2_phy_instance_get_mode_ext(tphy, instance);

	return ret;
}

static int mtk_phy_set_mode(struct phy *phy, enum phy_mode mode, int submode)
{
	struct mtk_phy_instance *instance = phy_get_drvdata(phy);
	struct mtk_tphy *tphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_USB2) {
		if (submode)
			u2_phy_instance_set_mode_ext(tphy, instance, submode);
		else
			u2_phy_instance_set_mode(tphy, instance, mode);
	}

	return 0;
}

static struct phy *mtk_phy_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct mtk_tphy *tphy = dev_get_drvdata(dev);
	struct mtk_phy_instance *instance = NULL;
	struct device_node *phy_np = args->np;
	int index;

	if (args->args_count != 1) {
		dev_err(dev, "invalid number of cells in 'phy' property\n");
		return ERR_PTR(-EINVAL);
	}

	for (index = 0; index < tphy->nphys; index++)
		if (phy_np == tphy->phys[index]->phy->dev.of_node) {
			instance = tphy->phys[index];
			break;
		}

	if (!instance) {
		dev_err(dev, "failed to find appropriate phy\n");
		return ERR_PTR(-EINVAL);
	}

	instance->type = args->args[0];
	if (!(instance->type == PHY_TYPE_USB2 ||
	      instance->type == PHY_TYPE_USB3 ||
	      instance->type == PHY_TYPE_PCIE ||
	      instance->type == PHY_TYPE_SATA)) {
		dev_err(dev, "unsupported device type: %d\n", instance->type);
		return ERR_PTR(-EINVAL);
	}

	if (tphy->pdata->version == MTK_PHY_V1) {
		phy_v1_banks_init(tphy, instance);
	} else if (tphy->pdata->version == MTK_PHY_V2) {
		phy_v2_banks_init(tphy, instance);
	} else {
		dev_err(dev, "phy version is not supported\n");
		return ERR_PTR(-EINVAL);
	}

	phy_parse_property(tphy, instance);

	return instance->phy;
}

static const struct phy_ops mtk_tphy_ops = {
	.init		= mtk_phy_init,
	.exit		= mtk_phy_exit,
	.power_on	= mtk_phy_power_on,
	.power_off	= mtk_phy_power_off,
	.set_mode	= mtk_phy_set_mode,
	.get_mode_ext	= mtk_phy_get_mode_ext,
	.owner		= THIS_MODULE,
};

static const struct mtk_phy_pdata tphy_v1_pdata = {
	.avoid_rx_sen_degradation = false,
	.version = MTK_PHY_V1,
};

static const struct mtk_phy_pdata tphy_v2_pdata = {
	.avoid_rx_sen_degradation = false,
	.version = MTK_PHY_V2,
};

static const struct mtk_phy_pdata mt8173_pdata = {
	.avoid_rx_sen_degradation = true,
	.version = MTK_PHY_V1,
};

static const struct of_device_id mtk_tphy_id_table[] = {
	{ .compatible = "mediatek,mt2701-u3phy", .data = &tphy_v1_pdata },
	{ .compatible = "mediatek,mt2712-u3phy", .data = &tphy_v2_pdata },
	{ .compatible = "mediatek,mt8173-u3phy", .data = &mt8173_pdata },
	{ .compatible = "mediatek,generic-tphy-v1", .data = &tphy_v1_pdata },
	{ .compatible = "mediatek,generic-tphy-v2", .data = &tphy_v2_pdata },
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_tphy_id_table);

static int mtk_tphy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	struct phy_provider *provider;
	struct resource *sif_res;
	struct mtk_tphy *tphy;
	struct resource res;
	int port, retval;

	tphy = devm_kzalloc(dev, sizeof(*tphy), GFP_KERNEL);
	if (!tphy)
		return -ENOMEM;

	tphy->pdata = of_device_get_match_data(dev);
	if (!tphy->pdata)
		return -EINVAL;

	tphy->nphys = of_get_child_count(np);
	tphy->phys = devm_kcalloc(dev, tphy->nphys,
				       sizeof(*tphy->phys), GFP_KERNEL);
	if (!tphy->phys)
		return -ENOMEM;

	retval = device_rename(dev, np->name);
	if (retval)
		dev_info(&pdev->dev, "failed to rename\n");
	/* fix uaf(use after free) issue: backup pdev->name,
	 * device_rename will free pdev->name
	 */
	pdev->name = pdev->dev.kobj.name;

	tphy->dev = dev;
	platform_set_drvdata(pdev, tphy);

	sif_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	/* SATA phy of V1 needn't it if not shared with PCIe or USB */
	if (sif_res && tphy->pdata->version == MTK_PHY_V1) {
		/* get banks shared by multiple phys */
		tphy->sif_base = devm_ioremap_resource(dev, sif_res);
		if (IS_ERR(tphy->sif_base)) {
			dev_err(dev, "failed to remap sif regs\n");
			return PTR_ERR(tphy->sif_base);
		}
	}

	/* it's deprecated, make it optional for backward compatibility */
	tphy->u3phya_ref = devm_clk_get_optional(dev, "u3phya_ref");
	if (IS_ERR(tphy->u3phya_ref))
		return PTR_ERR(tphy->u3phya_ref);

	tphy->src_ref_clk = U3P_REF_CLK;
	tphy->src_coef = U3P_SLEW_RATE_COEF;
	/* update parameters of slew rate calibrate if exist */
	device_property_read_u32(dev, "mediatek,src-ref-clk-mhz",
		&tphy->src_ref_clk);
	device_property_read_u32(dev, "mediatek,src-coef", &tphy->src_coef);

	port = 0;
	for_each_child_of_node(np, child_np) {
		struct mtk_phy_instance *instance;
		struct phy *phy;

		instance = devm_kzalloc(dev, sizeof(*instance), GFP_KERNEL);
		if (!instance) {
			retval = -ENOMEM;
			goto put_child;
		}

		tphy->phys[port] = instance;

		phy = devm_phy_create(dev, child_np, &mtk_tphy_ops);
		if (IS_ERR(phy)) {
			dev_err(dev, "failed to create phy\n");
			retval = PTR_ERR(phy);
			goto put_child;
		}

		retval = of_address_to_resource(child_np, 0, &res);
		if (retval) {
			dev_err(dev, "failed to get address resource(id-%d)\n",
				port);
			goto put_child;
		}

		instance->port_base = devm_ioremap_resource(&phy->dev, &res);
		if (IS_ERR(instance->port_base)) {
			dev_err(dev, "failed to remap phy regs\n");
			retval = PTR_ERR(instance->port_base);
			goto put_child;
		}

		instance->phy = phy;
		instance->index = port;
		phy_set_drvdata(phy, instance);
		port++;

		/* if deprecated clock is provided, ignore instance's one */
		if (tphy->u3phya_ref)
			continue;

		instance->ref_clk = devm_clk_get(&phy->dev, "ref");
		if (IS_ERR(instance->ref_clk)) {
			dev_err(dev, "failed to get ref_clk(id-%d)\n", port);
			retval = PTR_ERR(instance->ref_clk);
			goto put_child;
		}
	}

	mtk_phy_procfs_init(tphy);

	provider = devm_of_phy_provider_register(dev, mtk_phy_xlate);

	return PTR_ERR_OR_ZERO(provider);
put_child:
	of_node_put(child_np);
	return retval;
}

static int mtk_tphy_remove(struct platform_device *pdev)
{
	struct mtk_tphy *tphy = dev_get_drvdata(&pdev->dev);

	mtk_phy_procfs_exit(tphy);
	return 0;
}

static struct platform_driver mtk_tphy_driver = {
	.probe		= mtk_tphy_probe,
	.remove		= mtk_tphy_remove,
	.driver		= {
		.name	= "mtk-tphy",
		.of_match_table = mtk_tphy_id_table,
	},
};

module_platform_driver(mtk_tphy_driver);

#ifdef CONFIG_MACH_MT6771
void Charger_Detect_Init(void)
{
	struct u2phy_banks *u2_banks;
	void __iomem *com;
	u32 tmp;

	if (!bc11_instance)
		return;

	u2_banks = &bc11_instance->u2_banks;
	com = u2_banks->com;
	tmp = readl(com + U3P_USBPHYACR6);
	tmp |= PA6_RG_U2_BC11_SW_EN;   /* DP/DM BC1.1 path Disable */
	writel(tmp, com + U3P_USBPHYACR6);
}
EXPORT_SYMBOL_GPL(Charger_Detect_Init);

void Charger_Detect_Release(void)
{
	struct u2phy_banks *u2_banks;
	void __iomem *com;
	u32 tmp;

	if (!bc11_instance)
		return;

	u2_banks = &bc11_instance->u2_banks;
	com = u2_banks->com;
	tmp = readl(com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_BC11_SW_EN;   /* DP/DM BC1.1 path Disable */
	writel(tmp, com + U3P_USBPHYACR6);
}
EXPORT_SYMBOL_GPL(Charger_Detect_Release);
#endif

MODULE_AUTHOR("Chunfeng Yun <chunfeng.yun@mediatek.com>");
MODULE_DESCRIPTION("MediaTek T-PHY driver");
MODULE_LICENSE("GPL v2");
