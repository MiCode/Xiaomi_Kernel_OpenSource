/*****************************************************************************
*
* Filename:
* ---------
*   mt6333.h
*
* Project:
* --------
*   Android
*
* Description:
* ------------
*   mt6333 header file
*
* Author:
* -------
*
****************************************************************************/

#ifndef _MT6333_SW_H_
#define _MT6333_SW_H_

#define PMIC6333_E1_CID_CODE    0x1093
#define PMIC6333_E2_CID_CODE    0x2093

#define MT6333_POWER_BUCK_DEFAULT 0
#define MT6333_POWER_BUCK_VCORE   1
#define MT6333_POWER_BUCK_VMEM    2
#define MT6333_POWER_BUCK_VRF18   3

/**********************************************************
  *
  *   [Address] 
  *
  *********************************************************/
#define SWCHR_BASE (0x00000000)

#define MT6333_CID0             ((UINT32)(SWCHR_BASE+0x00))
#define MT6333_PERP_CON0        ((UINT32)(SWCHR_BASE+0x01))
#define MT6333_PERP_CON1        ((UINT32)(SWCHR_BASE+0x02))
#define MT6333_PERP_CON2        ((UINT32)(SWCHR_BASE+0x03))
#define MT6333_PERP_CON3        ((UINT32)(SWCHR_BASE+0x04))
#define MT6333_PERP_CON4        ((UINT32)(SWCHR_BASE+0x05))
#define MT6333_PERP_CON5        ((UINT32)(SWCHR_BASE+0x06))
#define MT6333_PERP_CON6        ((UINT32)(SWCHR_BASE+0x07))
#define MT6333_PERP_CON7        ((UINT32)(SWCHR_BASE+0x08))
#define MT6333_PERP_CON8        ((UINT32)(SWCHR_BASE+0x09))
#define MT6333_PERP_CON9        ((UINT32)(SWCHR_BASE+0x0A))
#define MT6333_PERP_CON10       ((UINT32)(SWCHR_BASE+0x0B))
#define MT6333_PERP_CON11       ((UINT32)(SWCHR_BASE+0x0C))
#define MT6333_CORE_CON0        ((UINT32)(SWCHR_BASE+0x0D))
#define MT6333_CORE_CON1        ((UINT32)(SWCHR_BASE+0x0E))
#define MT6333_CORE_CON2        ((UINT32)(SWCHR_BASE+0x0F))
#define MT6333_CORE_CON3        ((UINT32)(SWCHR_BASE+0x10))
#define MT6333_CORE_CON4        ((UINT32)(SWCHR_BASE+0x11))
#define MT6333_CORE_CON5        ((UINT32)(SWCHR_BASE+0x12))
#define MT6333_CORE_CON6        ((UINT32)(SWCHR_BASE+0x13))
#define MT6333_CORE_CON7        ((UINT32)(SWCHR_BASE+0x14))
#define MT6333_CORE_CON8        ((UINT32)(SWCHR_BASE+0x15))
#define MT6333_CORE_CON9        ((UINT32)(SWCHR_BASE+0x16))
#define MT6333_CORE_CON10       ((UINT32)(SWCHR_BASE+0x17))
#define MT6333_CORE_CON11       ((UINT32)(SWCHR_BASE+0x18))
#define MT6333_STA_CON0         ((UINT32)(SWCHR_BASE+0x19))
#define MT6333_STA_CON1         ((UINT32)(SWCHR_BASE+0x1A))
#define MT6333_STA_CON2         ((UINT32)(SWCHR_BASE+0x1B))
#define MT6333_STA_CON3         ((UINT32)(SWCHR_BASE+0x1C))
#define MT6333_STA_CON4         ((UINT32)(SWCHR_BASE+0x1D))
#define MT6333_STA_CON5         ((UINT32)(SWCHR_BASE+0x1E))
#define MT6333_STA_CON6         ((UINT32)(SWCHR_BASE+0x1F))
#define MT6333_STA_CON7         ((UINT32)(SWCHR_BASE+0x20))
#define MT6333_STA_CON8         ((UINT32)(SWCHR_BASE+0x21))
#define MT6333_STA_CON9         ((UINT32)(SWCHR_BASE+0x22))
#define MT6333_STA_CON10        ((UINT32)(SWCHR_BASE+0x23))
#define MT6333_STA_CON11        ((UINT32)(SWCHR_BASE+0x24))
#define MT6333_STA_CON12        ((UINT32)(SWCHR_BASE+0x25))
#define MT6333_STA_CON13        ((UINT32)(SWCHR_BASE+0x26))
#define MT6333_STA_CON14        ((UINT32)(SWCHR_BASE+0x27))
#define MT6333_STA_CON15        ((UINT32)(SWCHR_BASE+0x28))
#define MT6333_DIG_CON0         ((UINT32)(SWCHR_BASE+0x29))
#define MT6333_DIG_CON1         ((UINT32)(SWCHR_BASE+0x2A))
#define MT6333_DIG_CON2         ((UINT32)(SWCHR_BASE+0x2B))
#define MT6333_DIG_CON3         ((UINT32)(SWCHR_BASE+0x2C))
#define MT6333_DIG_CON4         ((UINT32)(SWCHR_BASE+0x2D))
#define MT6333_DIG_CON5         ((UINT32)(SWCHR_BASE+0x2E))
#define MT6333_DIG_CON6         ((UINT32)(SWCHR_BASE+0x2F))
#define MT6333_DIG_CON7         ((UINT32)(SWCHR_BASE+0x30))
#define MT6333_DIG_CON8         ((UINT32)(SWCHR_BASE+0x31))
#define MT6333_DIG_CON9         ((UINT32)(SWCHR_BASE+0x32))
#define MT6333_DIG_CON10        ((UINT32)(SWCHR_BASE+0x33))
#define MT6333_DIG_CON11        ((UINT32)(SWCHR_BASE+0x34))
#define MT6333_DIG_CON12        ((UINT32)(SWCHR_BASE+0x35))
#define MT6333_DIG_CON13        ((UINT32)(SWCHR_BASE+0x36))
#define MT6333_DIG_CON14        ((UINT32)(SWCHR_BASE+0x37))
#define MT6333_DIG_CON15        ((UINT32)(SWCHR_BASE+0x38))
#define MT6333_DIG_CON16        ((UINT32)(SWCHR_BASE+0x39))
#define MT6333_DIG_CON17        ((UINT32)(SWCHR_BASE+0x3A))
#define MT6333_DIG_CON18        ((UINT32)(SWCHR_BASE+0x3B))
#define MT6333_GPIO_CON0        ((UINT32)(SWCHR_BASE+0x3C))
#define MT6333_GPIO_CON1        ((UINT32)(SWCHR_BASE+0x3D))
#define MT6333_GPIO_CON2        ((UINT32)(SWCHR_BASE+0x3E))
#define MT6333_CLK_CON0         ((UINT32)(SWCHR_BASE+0x3F))
#define MT6333_CLK_CON0_SET     ((UINT32)(SWCHR_BASE+0x40))
#define MT6333_CLK_CON0_CLR     ((UINT32)(SWCHR_BASE+0x41))
#define MT6333_CLK_CON1         ((UINT32)(SWCHR_BASE+0x42))
#define MT6333_CLK_CON2         ((UINT32)(SWCHR_BASE+0x43))
#define MT6333_INT_CON0         ((UINT32)(SWCHR_BASE+0x44))
#define MT6333_INT_CON0_SET     ((UINT32)(SWCHR_BASE+0x45))
#define MT6333_INT_CON0_CLR     ((UINT32)(SWCHR_BASE+0x46))
#define MT6333_INT_CON1         ((UINT32)(SWCHR_BASE+0x47))
#define MT6333_INT_CON1_SET     ((UINT32)(SWCHR_BASE+0x48))
#define MT6333_INT_CON1_CLR     ((UINT32)(SWCHR_BASE+0x49))
#define MT6333_INT_CON2         ((UINT32)(SWCHR_BASE+0x4A))
#define MT6333_INT_CON2_SET     ((UINT32)(SWCHR_BASE+0x4B))
#define MT6333_INT_CON2_CLR     ((UINT32)(SWCHR_BASE+0x4C))
#define MT6333_CHRWDT_CON0      ((UINT32)(SWCHR_BASE+0x4D))
#define MT6333_CHRWDT_STATUS0   ((UINT32)(SWCHR_BASE+0x4E))
#define MT6333_INT_STATUS0      ((UINT32)(SWCHR_BASE+0x4F))
#define MT6333_INT_STATUS1      ((UINT32)(SWCHR_BASE+0x50))
#define MT6333_INT_STATUS2      ((UINT32)(SWCHR_BASE+0x51))
#define MT6333_OC_CTL_VCORE     ((UINT32)(SWCHR_BASE+0x52))
#define MT6333_OC_CTL_VMEM      ((UINT32)(SWCHR_BASE+0x53))
#define MT6333_OC_CTL_VRF18     ((UINT32)(SWCHR_BASE+0x54))
#define MT6333_INT_MISC_CON     ((UINT32)(SWCHR_BASE+0x55))
#define MT6333_BUCK_CON0        ((UINT32)(SWCHR_BASE+0x56))
#define MT6333_VCORE_CON0       ((UINT32)(SWCHR_BASE+0x57))
#define MT6333_VCORE_CON1       ((UINT32)(SWCHR_BASE+0x58))
#define MT6333_VCORE_CON2       ((UINT32)(SWCHR_BASE+0x59))
#define MT6333_VCORE_CON3       ((UINT32)(SWCHR_BASE+0x5A))
#define MT6333_VCORE_CON4       ((UINT32)(SWCHR_BASE+0x5B))
#define MT6333_VCORE_CON5       ((UINT32)(SWCHR_BASE+0x5C))
#define MT6333_VCORE_CON6       ((UINT32)(SWCHR_BASE+0x5D))
#define MT6333_VCORE_CON7       ((UINT32)(SWCHR_BASE+0x5E))
#define MT6333_VCORE_CON8       ((UINT32)(SWCHR_BASE+0x5F))
#define MT6333_VCORE_CON9       ((UINT32)(SWCHR_BASE+0x60))
#define MT6333_VCORE_CON10      ((UINT32)(SWCHR_BASE+0x61))
#define MT6333_VCORE_CON11      ((UINT32)(SWCHR_BASE+0x62))
#define MT6333_VCORE_CON12      ((UINT32)(SWCHR_BASE+0x63))
#define MT6333_VCORE_CON13      ((UINT32)(SWCHR_BASE+0x64))
#define MT6333_VCORE_CON14      ((UINT32)(SWCHR_BASE+0x65))
#define MT6333_VCORE_CON15      ((UINT32)(SWCHR_BASE+0x66))
#define MT6333_VCORE_CON16      ((UINT32)(SWCHR_BASE+0x67))
#define MT6333_VCORE_CON17      ((UINT32)(SWCHR_BASE+0x68))
#define MT6333_VCORE_CON18      ((UINT32)(SWCHR_BASE+0x69))
#define MT6333_VCORE_CON19      ((UINT32)(SWCHR_BASE+0x6A))
#define MT6333_VCORE_CON20      ((UINT32)(SWCHR_BASE+0x6B))
#define MT6333_VCORE_CON21      ((UINT32)(SWCHR_BASE+0x6C))
#define MT6333_VCORE_CON22      ((UINT32)(SWCHR_BASE+0x6D))
#define MT6333_VCORE_CON23      ((UINT32)(SWCHR_BASE+0x6E))
#define MT6333_VCORE_CON24      ((UINT32)(SWCHR_BASE+0x6F))
#define MT6333_VCORE_CON25      ((UINT32)(SWCHR_BASE+0x70))
#define MT6333_VCORE_CON26      ((UINT32)(SWCHR_BASE+0x71))
#define MT6333_VCORE_CON27      ((UINT32)(SWCHR_BASE+0x72))
#define MT6333_VCORE_CON28      ((UINT32)(SWCHR_BASE+0x73))
#define MT6333_VCORE_CON29      ((UINT32)(SWCHR_BASE+0x74))
#define MT6333_VCORE_CON30      ((UINT32)(SWCHR_BASE+0x75))
#define MT6333_VMEM_RSV0        ((UINT32)(SWCHR_BASE+0x76))
#define MT6333_VMEM_CON0        ((UINT32)(SWCHR_BASE+0x77))
#define MT6333_VMEM_CON1        ((UINT32)(SWCHR_BASE+0x78))
#define MT6333_VMEM_CON2        ((UINT32)(SWCHR_BASE+0x79))
#define MT6333_VMEM_CON3        ((UINT32)(SWCHR_BASE+0x7A))
#define MT6333_VMEM_CON4        ((UINT32)(SWCHR_BASE+0x7B))
#define MT6333_VMEM_CON5        ((UINT32)(SWCHR_BASE+0x7C))
#define MT6333_VMEM_CON6        ((UINT32)(SWCHR_BASE+0x7D))
#define MT6333_VMEM_CON7        ((UINT32)(SWCHR_BASE+0x7E))
#define MT6333_VMEM_CON8        ((UINT32)(SWCHR_BASE+0x7F))
#define MT6333_VMEM_CON9        ((UINT32)(SWCHR_BASE+0x80))
#define MT6333_VMEM_CON12       ((UINT32)(SWCHR_BASE+0x81))
#define MT6333_VMEM_CON13       ((UINT32)(SWCHR_BASE+0x82))
#define MT6333_VMEM_CON14       ((UINT32)(SWCHR_BASE+0x83))
#define MT6333_VMEM_CON15       ((UINT32)(SWCHR_BASE+0x84))
#define MT6333_VMEM_CON16       ((UINT32)(SWCHR_BASE+0x85))
#define MT6333_VMEM_CON17       ((UINT32)(SWCHR_BASE+0x86))
#define MT6333_VMEM_CON18       ((UINT32)(SWCHR_BASE+0x87))
#define MT6333_VMEM_CON19       ((UINT32)(SWCHR_BASE+0x88))
#define MT6333_VMEM_CON20       ((UINT32)(SWCHR_BASE+0x89))
#define MT6333_VMEM_CON21       ((UINT32)(SWCHR_BASE+0x8A))
#define MT6333_VMEM_CON22       ((UINT32)(SWCHR_BASE+0x8B))
#define MT6333_VRF18_RSV0       ((UINT32)(SWCHR_BASE+0x8C))
#define MT6333_VRF18_CON0       ((UINT32)(SWCHR_BASE+0x8D))
#define MT6333_VRF18_CON1       ((UINT32)(SWCHR_BASE+0x8E))
#define MT6333_VRF18_CON2       ((UINT32)(SWCHR_BASE+0x8F))
#define MT6333_VRF18_CON3       ((UINT32)(SWCHR_BASE+0x90))
#define MT6333_VRF18_CON4       ((UINT32)(SWCHR_BASE+0x91))
#define MT6333_VRF18_CON6       ((UINT32)(SWCHR_BASE+0x92))
#define MT6333_VRF18_CON7       ((UINT32)(SWCHR_BASE+0x93))
#define MT6333_VRF18_CON8       ((UINT32)(SWCHR_BASE+0x94))
#define MT6333_VRF18_CON9       ((UINT32)(SWCHR_BASE+0x95))
#define MT6333_VRF18_CON12      ((UINT32)(SWCHR_BASE+0x96))
#define MT6333_VRF18_CON13      ((UINT32)(SWCHR_BASE+0x97))
#define MT6333_VRF18_CON14      ((UINT32)(SWCHR_BASE+0x98))
#define MT6333_VRF18_CON15      ((UINT32)(SWCHR_BASE+0x99))
#define MT6333_VRF18_CON16      ((UINT32)(SWCHR_BASE+0x9A))
#define MT6333_VRF18_CON17      ((UINT32)(SWCHR_BASE+0x9B))
#define MT6333_VRF18_CON18      ((UINT32)(SWCHR_BASE+0x9C))
#define MT6333_VRF18_CON19      ((UINT32)(SWCHR_BASE+0x9D))
#define MT6333_VRF18_CON20      ((UINT32)(SWCHR_BASE+0x9E))
#define MT6333_VRF18_CON21      ((UINT32)(SWCHR_BASE+0x9F))
#define MT6333_VRF18_CON22      ((UINT32)(SWCHR_BASE+0xA0))
#define MT6333_BUCK_K_CON0      ((UINT32)(SWCHR_BASE+0xA1))
#define MT6333_BUCK_K_CON1      ((UINT32)(SWCHR_BASE+0xA2))
#define MT6333_BUCK_K_CON2      ((UINT32)(SWCHR_BASE+0xA3))
#define MT6333_BUCK_K_CON3      ((UINT32)(SWCHR_BASE+0xA4))
#define MT6333_BUCK_K_CON4      ((UINT32)(SWCHR_BASE+0xA5))
#define MT6333_BUCK_K_CON5      ((UINT32)(SWCHR_BASE+0xA6))
#define MT6333_STRUP_CON0       ((UINT32)(SWCHR_BASE+0xA7))
#define MT6333_STRUP_CON1       ((UINT32)(SWCHR_BASE+0xA8))
#define MT6333_STRUP_CON2       ((UINT32)(SWCHR_BASE+0xA9))
#define MT6333_EFUSE_CON0       ((UINT32)(SWCHR_BASE+0xAA))
#define MT6333_EFUSE_CON1       ((UINT32)(SWCHR_BASE+0xAB))
#define MT6333_EFUSE_CON2       ((UINT32)(SWCHR_BASE+0xAC))
#define MT6333_EFUSE_CON3       ((UINT32)(SWCHR_BASE+0xAD))
#define MT6333_EFUSE_CON4       ((UINT32)(SWCHR_BASE+0xAE))
#define MT6333_EFUSE_CON5       ((UINT32)(SWCHR_BASE+0xAF))
#define MT6333_EFUSE_CON6       ((UINT32)(SWCHR_BASE+0xB0))
#define MT6333_EFUSE_CON7       ((UINT32)(SWCHR_BASE+0xB1))
#define MT6333_EFUSE_CON8       ((UINT32)(SWCHR_BASE+0xB2))
#define MT6333_EFUSE_CON9       ((UINT32)(SWCHR_BASE+0xB3))
#define MT6333_EFUSE_CON10      ((UINT32)(SWCHR_BASE+0xB4))
#define MT6333_EFUSE_CON11      ((UINT32)(SWCHR_BASE+0xB5))
#define MT6333_EFUSE_CON12      ((UINT32)(SWCHR_BASE+0xB6))
#define MT6333_EFUSE_DOUT_0_7   ((UINT32)(SWCHR_BASE+0xB7))
#define MT6333_EFUSE_DOUT_8_15  ((UINT32)(SWCHR_BASE+0xB8))
#define MT6333_EFUSE_DOUT_16_23 ((UINT32)(SWCHR_BASE+0xB9))
#define MT6333_EFUSE_DOUT_24_31 ((UINT32)(SWCHR_BASE+0xBA))
#define MT6333_EFUSE_DOUT_32_39 ((UINT32)(SWCHR_BASE+0xBB))
#define MT6333_EFUSE_DOUT_40_47 ((UINT32)(SWCHR_BASE+0xBC))
#define MT6333_EFUSE_DOUT_48_55 ((UINT32)(SWCHR_BASE+0xBD))
#define MT6333_EFUSE_DOUT_56_63 ((UINT32)(SWCHR_BASE+0xBE))
#define MT6333_TESTI_CON0       ((UINT32)(SWCHR_BASE+0xE0))
#define MT6333_TESTI_CON1       ((UINT32)(SWCHR_BASE+0xE1))
#define MT6333_TESTI_CON2       ((UINT32)(SWCHR_BASE+0xE2))
#define MT6333_TESTI_CON3       ((UINT32)(SWCHR_BASE+0xE3))
#define MT6333_TESTI_CON4       ((UINT32)(SWCHR_BASE+0xE4))
#define MT6333_TESTI_CON5       ((UINT32)(SWCHR_BASE+0xE5))
#define MT6333_TESTI_CON6       ((UINT32)(SWCHR_BASE+0xE6))
#define MT6333_TESTI_CON7       ((UINT32)(SWCHR_BASE+0xE7))
#define MT6333_TESTI_CON8       ((UINT32)(SWCHR_BASE+0xE8))
#define MT6333_TESTI_MUX_CON0   ((UINT32)(SWCHR_BASE+0xE9))
#define MT6333_TESTI_MUX_CON1   ((UINT32)(SWCHR_BASE+0xEA))
#define MT6333_TESTI_MUX_CON2   ((UINT32)(SWCHR_BASE+0xEB))
#define MT6333_TESTI_MUX_CON3   ((UINT32)(SWCHR_BASE+0xEC))
#define MT6333_TESTI_MUX_CON4   ((UINT32)(SWCHR_BASE+0xED))
#define MT6333_TESTI_MUX_CON5   ((UINT32)(SWCHR_BASE+0xEE))
#define MT6333_TESTI_MUX_CON6   ((UINT32)(SWCHR_BASE+0xEF))
#define MT6333_TESTI_MUX_CON7   ((UINT32)(SWCHR_BASE+0xF0))
#define MT6333_TESTI_MUX_CON8   ((UINT32)(SWCHR_BASE+0xF1))
#define MT6333_TESTO_CON0       ((UINT32)(SWCHR_BASE+0xF2))
#define MT6333_TESTO_CON1       ((UINT32)(SWCHR_BASE+0xF3))
#define MT6333_TESTO_CON2       ((UINT32)(SWCHR_BASE+0xF4))
#define MT6333_TESTO_CON3       ((UINT32)(SWCHR_BASE+0xF5))
#define MT6333_TEST_OMUX_CON0   ((UINT32)(SWCHR_BASE+0xF6))
#define MT6333_TEST_OMUX_CON1   ((UINT32)(SWCHR_BASE+0xF7))
#define MT6333_TEST_OMUX_CON2   ((UINT32)(SWCHR_BASE+0xF8))
#define MT6333_TEST_OMUX_CON3   ((UINT32)(SWCHR_BASE+0xF9))
#define MT6333_DEBUG_CON0       ((UINT32)(SWCHR_BASE+0xFA))
#define MT6333_DEBUG_CON1       ((UINT32)(SWCHR_BASE+0xFB))
#define MT6333_DEBUG_CON2       ((UINT32)(SWCHR_BASE+0xFC))
#define MT6333_CID1             ((UINT32)(SWCHR_BASE+0xFD))
//mask is HEX;  shift is Integer
#define MT6333_PMIC_CID0_MASK                             0xFF
#define MT6333_PMIC_CID0_SHIFT                            0
#define MT6333_PMIC_RG_BGR_RSEL_MASK                      0x7
#define MT6333_PMIC_RG_BGR_RSEL_SHIFT                     0
#define MT6333_PMIC_RG_BGR_UNCHOP_MASK                    0x1
#define MT6333_PMIC_RG_BGR_UNCHOP_SHIFT                   4
#define MT6333_PMIC_RG_BGR_UNCHOP_PH_MASK                 0x1
#define MT6333_PMIC_RG_BGR_UNCHOP_PH_SHIFT                5
#define MT6333_PMIC_RG_BGR_TRIM_MASK                      0x1F
#define MT6333_PMIC_RG_BGR_TRIM_SHIFT                     0
#define MT6333_PMIC_RG_BGR_TRIM_EN_MASK                   0x1
#define MT6333_PMIC_RG_BGR_TRIM_EN_SHIFT                  7
#define MT6333_PMIC_RG_BGR_TEST_EN_MASK                   0x1
#define MT6333_PMIC_RG_BGR_TEST_EN_SHIFT                  0
#define MT6333_PMIC_RG_BGR_TEST_RSTB_MASK                 0x1
#define MT6333_PMIC_RG_BGR_TEST_RSTB_SHIFT                1
#define MT6333_PMIC_RG_BGR_TEST_CKIN_MASK                 0x1
#define MT6333_PMIC_RG_BGR_TEST_CKIN_SHIFT                2
#define MT6333_PMIC_RG_CHR_EN_MASK                        0x1
#define MT6333_PMIC_RG_CHR_EN_SHIFT                       0
#define MT6333_PMIC_RG_VBOUT_EN_MASK                      0x1
#define MT6333_PMIC_RG_VBOUT_EN_SHIFT                     0
#define MT6333_PMIC_RG_ADCIN_VBAT_EN_MASK                 0x1
#define MT6333_PMIC_RG_ADCIN_VBAT_EN_SHIFT                1
#define MT6333_PMIC_RG_ADCIN_CHRIN_EN_MASK                0x1
#define MT6333_PMIC_RG_ADCIN_CHRIN_EN_SHIFT               2
#define MT6333_PMIC_RG_ADCIN_BATON_EN_MASK                0x1
#define MT6333_PMIC_RG_ADCIN_BATON_EN_SHIFT               3
#define MT6333_PMIC_RG_BAT_ON_OPEN_VTH_MASK               0x1
#define MT6333_PMIC_RG_BAT_ON_OPEN_VTH_SHIFT              4
#define MT6333_PMIC_RG_BAT_ON_PULL_HIGH_EN_MASK           0x1
#define MT6333_PMIC_RG_BAT_ON_PULL_HIGH_EN_SHIFT          5
#define MT6333_PMIC_RG_INT_PH_ENB_MASK                    0x1
#define MT6333_PMIC_RG_INT_PH_ENB_SHIFT                   0
#define MT6333_PMIC_RG_SCL_PH_ENB_MASK                    0x1
#define MT6333_PMIC_RG_SCL_PH_ENB_SHIFT                   1
#define MT6333_PMIC_RG_SDA_PH_ENB_MASK                    0x1
#define MT6333_PMIC_RG_SDA_PH_ENB_SHIFT                   2
#define MT6333_PMIC_RG_VDRV_RDIVSEL_MASK                  0x3
#define MT6333_PMIC_RG_VDRV_RDIVSEL_SHIFT                 0
#define MT6333_PMIC_RG_SWCHR_RV1_MASK                     0xFF
#define MT6333_PMIC_RG_SWCHR_RV1_SHIFT                    0
#define MT6333_PMIC_RG_CHR_OTG_LV_TH_MASK                 0x1
#define MT6333_PMIC_RG_CHR_OTG_LV_TH_SHIFT                0
#define MT6333_PMIC_RG_CHR_OTG_HV_TH_MASK                 0x1
#define MT6333_PMIC_RG_CHR_OTG_HV_TH_SHIFT                2
#define MT6333_PMIC_RG_STRUP_THER_RG_TH_MASK              0xF
#define MT6333_PMIC_RG_STRUP_THER_RG_TH_SHIFT             0
#define MT6333_PMIC_RG_STRUP_RSV_MASK                     0xF
#define MT6333_PMIC_RG_STRUP_RSV_SHIFT                    4
#define MT6333_PMIC_RG_SWCHR_ANA_TEST_MODE_MASK           0x1
#define MT6333_PMIC_RG_SWCHR_ANA_TEST_MODE_SHIFT          0
#define MT6333_PMIC_RG_SWCHR_ANA_TEST_MODE_SEL_MASK       0x7
#define MT6333_PMIC_RG_SWCHR_ANA_TEST_MODE_SEL_SHIFT      2
#define MT6333_PMIC_RG_CSA_OTG_SEL_MASK                   0x3
#define MT6333_PMIC_RG_CSA_OTG_SEL_SHIFT                  0
#define MT6333_PMIC_RG_OTG_CS_SLP_EN_MASK                 0x1
#define MT6333_PMIC_RG_OTG_CS_SLP_EN_SHIFT                3
#define MT6333_PMIC_RG_SLP_OTG_SEL_MASK                   0x3
#define MT6333_PMIC_RG_SLP_OTG_SEL_SHIFT                  5
#define MT6333_PMIC_RG_ITERM_SEL_MASK                     0x7
#define MT6333_PMIC_RG_ITERM_SEL_SHIFT                    0
#define MT6333_PMIC_RG_ICS_LOOP_MASK                      0x3
#define MT6333_PMIC_RG_ICS_LOOP_SHIFT                     4
#define MT6333_PMIC_RG_ZXGM_TUNE_MASK                     0x3
#define MT6333_PMIC_RG_ZXGM_TUNE_SHIFT                    0
#define MT6333_PMIC_RG_CHOP_EN_MASK                       0x1
#define MT6333_PMIC_RG_CHOP_EN_SHIFT                      4
#define MT6333_PMIC_RG_FORCE_NON_OC_MASK                  0x1
#define MT6333_PMIC_RG_FORCE_NON_OC_SHIFT                 5
#define MT6333_PMIC_RG_FORCE_NON_OV_MASK                  0x1
#define MT6333_PMIC_RG_FORCE_NON_OV_SHIFT                 6
#define MT6333_PMIC_RG_GDRI_MINOFF_EN_MASK                0x1
#define MT6333_PMIC_RG_GDRI_MINOFF_EN_SHIFT               7
#define MT6333_PMIC_RG_SYS_VREFTRIM_MASK                  0x3F
#define MT6333_PMIC_RG_SYS_VREFTRIM_SHIFT                 0
#define MT6333_PMIC_RG_CS_VREFTRIM_MASK                   0x3F
#define MT6333_PMIC_RG_CS_VREFTRIM_SHIFT                  0
#define MT6333_PMIC_RG_OSC_TRIM_MASK                      0x3F
#define MT6333_PMIC_RG_OSC_TRIM_SHIFT                     0
#define MT6333_PMIC_RG_VSYS_OV_TRIM_MASK                  0x1F
#define MT6333_PMIC_RG_VSYS_OV_TRIM_SHIFT                 0
#define MT6333_PMIC_RG_SWCHR_RV2_MASK                     0xFF
#define MT6333_PMIC_RG_SWCHR_RV2_SHIFT                    0
#define MT6333_PMIC_RG_INPUT_CC_REG_MASK                  0x1
#define MT6333_PMIC_RG_INPUT_CC_REG_SHIFT                 0
#define MT6333_PMIC_RG_OTG_CHRIN_VOL_MASK                 0x7
#define MT6333_PMIC_RG_OTG_CHRIN_VOL_SHIFT                3
#define MT6333_PMIC_RG_FORCE_OTG_NON_OV_MASK              0x1
#define MT6333_PMIC_RG_FORCE_OTG_NON_OV_SHIFT             7
#define MT6333_PMIC_RG_INOUT_CSREG_SEL_MASK               0x1
#define MT6333_PMIC_RG_INOUT_CSREG_SEL_SHIFT              0
#define MT6333_PMIC_RG_CHOPFREQ_SEL_MASK                  0x1
#define MT6333_PMIC_RG_CHOPFREQ_SEL_SHIFT                 4
#define MT6333_PMIC_RG_CHGPREG_SEL_MASK                   0x3
#define MT6333_PMIC_RG_CHGPREG_SEL_SHIFT                  6
#define MT6333_PMIC_RG_FLASH_DRV_EN_MASK                  0x1
#define MT6333_PMIC_RG_FLASH_DRV_EN_SHIFT                 0
#define MT6333_PMIC_RG_FPWM_OTG_MASK                      0x1
#define MT6333_PMIC_RG_FPWM_OTG_SHIFT                     0
#define MT6333_PMIC_RG_OTG_ZX_TESTMODE_MASK               0x1
#define MT6333_PMIC_RG_OTG_ZX_TESTMODE_SHIFT              2
#define MT6333_PMIC_RG_SWCHR_ZX_TESTMODE_MASK             0x1
#define MT6333_PMIC_RG_SWCHR_ZX_TESTMODE_SHIFT            4
#define MT6333_PMIC_RG_ZX_TRIM_MASK                       0x7
#define MT6333_PMIC_RG_ZX_TRIM_SHIFT                      0
#define MT6333_PMIC_RG_ZX_TRIM_OTG_MASK                   0x7
#define MT6333_PMIC_RG_ZX_TRIM_OTG_SHIFT                  4
#define MT6333_PMIC_RGS_AUTO_RECHARGE_MASK                0x1
#define MT6333_PMIC_RGS_AUTO_RECHARGE_SHIFT               0
#define MT6333_PMIC_RGS_CHARGE_COMPLETE_HW_MASK           0x1
#define MT6333_PMIC_RGS_CHARGE_COMPLETE_HW_SHIFT          1
#define MT6333_PMIC_RGS_PWM_OC_DET_MASK                   0x1
#define MT6333_PMIC_RGS_PWM_OC_DET_SHIFT                  2
#define MT6333_PMIC_RGS_VSYS_OV_DET_MASK                  0x1
#define MT6333_PMIC_RGS_VSYS_OV_DET_SHIFT                 3
#define MT6333_PMIC_RGS_POWER_PATH_MASK                   0x1
#define MT6333_PMIC_RGS_POWER_PATH_SHIFT                  4
#define MT6333_PMIC_RGS_FORCE_NO_PP_CONFIG_MASK           0x1
#define MT6333_PMIC_RGS_FORCE_NO_PP_CONFIG_SHIFT          5
#define MT6333_PMIC_RGS_CHRG_STATUS_MASK                  0x1
#define MT6333_PMIC_RGS_CHRG_STATUS_SHIFT                 6
#define MT6333_PMIC_RGS_BAT_ST_RECC_MASK                  0x1
#define MT6333_PMIC_RGS_BAT_ST_RECC_SHIFT                 0
#define MT6333_PMIC_RGS_SYS_GT_CV_MASK                    0x1
#define MT6333_PMIC_RGS_SYS_GT_CV_SHIFT                   1
#define MT6333_PMIC_RGS_BAT_GT_CC_MASK                    0x1
#define MT6333_PMIC_RGS_BAT_GT_CC_SHIFT                   2
#define MT6333_PMIC_RGS_BAT_GT_30_MASK                    0x1
#define MT6333_PMIC_RGS_BAT_GT_30_SHIFT                   3
#define MT6333_PMIC_RGS_BAT_GT_22_MASK                    0x1
#define MT6333_PMIC_RGS_BAT_GT_22_SHIFT                   4
#define MT6333_PMIC_RGS_BUCK_MODE_MASK                    0x1
#define MT6333_PMIC_RGS_BUCK_MODE_SHIFT                   0
#define MT6333_PMIC_RGS_BUCK_PRECC_MODE_MASK              0x1
#define MT6333_PMIC_RGS_BUCK_PRECC_MODE_SHIFT             1
#define MT6333_PMIC_RGS_CHRDET_MASK                       0x1
#define MT6333_PMIC_RGS_CHRDET_SHIFT                      2
#define MT6333_PMIC_RGS_CHR_HV_DET_MASK                   0x1
#define MT6333_PMIC_RGS_CHR_HV_DET_SHIFT                  3
#define MT6333_PMIC_RGS_CHR_PLUG_IN_MASK                  0x1
#define MT6333_PMIC_RGS_CHR_PLUG_IN_SHIFT                 4
#define MT6333_PMIC_RGS_BATON_UNDET_MASK                  0x1
#define MT6333_PMIC_RGS_BATON_UNDET_SHIFT                 5
#define MT6333_PMIC_RGS_CHRIN_LV_DET_MASK                 0x1
#define MT6333_PMIC_RGS_CHRIN_LV_DET_SHIFT                6
#define MT6333_PMIC_RGS_CHRIN_HV_DET_MASK                 0x1
#define MT6333_PMIC_RGS_CHRIN_HV_DET_SHIFT                7
#define MT6333_PMIC_RGS_THERMAL_SD_MODE_MASK              0x1
#define MT6333_PMIC_RGS_THERMAL_SD_MODE_SHIFT             0
#define MT6333_PMIC_RGS_CHR_HV_MODE_MASK                  0x1
#define MT6333_PMIC_RGS_CHR_HV_MODE_SHIFT                 1
#define MT6333_PMIC_RGS_BAT_ONLY_MODE_MASK                0x1
#define MT6333_PMIC_RGS_BAT_ONLY_MODE_SHIFT               2
#define MT6333_PMIC_RGS_CHR_SUSPEND_MODE_MASK             0x1
#define MT6333_PMIC_RGS_CHR_SUSPEND_MODE_SHIFT            3
#define MT6333_PMIC_RGS_PRECC_MODE_MASK                   0x1
#define MT6333_PMIC_RGS_PRECC_MODE_SHIFT                  4
#define MT6333_PMIC_RGS_CV_MODE_MASK                      0x1
#define MT6333_PMIC_RGS_CV_MODE_SHIFT                     5
#define MT6333_PMIC_RGS_CC_MODE_MASK                      0x1
#define MT6333_PMIC_RGS_CC_MODE_SHIFT                     6
#define MT6333_PMIC_RGS_OT_REG_MASK                       0x1
#define MT6333_PMIC_RGS_OT_REG_SHIFT                      0
#define MT6333_PMIC_RGS_OT_SD_MASK                        0x1
#define MT6333_PMIC_RGS_OT_SD_SHIFT                       1
#define MT6333_PMIC_RGS_PWM_BAT_CONFIG_MASK               0x1
#define MT6333_PMIC_RGS_PWM_BAT_CONFIG_SHIFT              2
#define MT6333_PMIC_RGS_PWM_CURRENT_CONFIG_MASK           0x1
#define MT6333_PMIC_RGS_PWM_CURRENT_CONFIG_SHIFT          3
#define MT6333_PMIC_RGS_PWM_VOLTAGE_CONFIG_MASK           0x1
#define MT6333_PMIC_RGS_PWM_VOLTAGE_CONFIG_SHIFT          4
#define MT6333_PMIC_RGS_BUCK_OVERLOAD_MASK                0x1
#define MT6333_PMIC_RGS_BUCK_OVERLOAD_SHIFT               0
#define MT6333_PMIC_RGS_BAT_DPPM_MODE_MASK                0x1
#define MT6333_PMIC_RGS_BAT_DPPM_MODE_SHIFT               1
#define MT6333_PMIC_RGS_ADAPTIVE_CV_MODE_MASK             0x1
#define MT6333_PMIC_RGS_ADAPTIVE_CV_MODE_SHIFT            2
#define MT6333_PMIC_RGS_VIN_DPM_MODE_MASK                 0x1
#define MT6333_PMIC_RGS_VIN_DPM_MODE_SHIFT                3
#define MT6333_PMIC_RGS_THERMAL_REG_MODE_MASK             0x1
#define MT6333_PMIC_RGS_THERMAL_REG_MODE_SHIFT            4
#define MT6333_PMIC_RGS_ICH_SETTING_MASK                  0xF
#define MT6333_PMIC_RGS_ICH_SETTING_SHIFT                 0
#define MT6333_PMIC_RGS_CS_SEL_MASK                       0xF
#define MT6333_PMIC_RGS_CS_SEL_SHIFT                      4
#define MT6333_PMIC_RGS_SYSCV_FINE_SEL_MASK               0x7
#define MT6333_PMIC_RGS_SYSCV_FINE_SEL_SHIFT              0
#define MT6333_PMIC_RGS_OC_SD_SEL_MASK                    0x1
#define MT6333_PMIC_RGS_OC_SD_SEL_SHIFT                   4
#define MT6333_PMIC_RGS_PWM_OC_SEL_MASK                   0x3
#define MT6333_PMIC_RGS_PWM_OC_SEL_SHIFT                  6
#define MT6333_PMIC_RGS_CHRWDT_TOUT_MASK                  0x1
#define MT6333_PMIC_RGS_CHRWDT_TOUT_SHIFT                 0
#define MT6333_PMIC_RGS_VSYS_OV_VTH_MASK                  0x7
#define MT6333_PMIC_RGS_VSYS_OV_VTH_SHIFT                 1
#define MT6333_PMIC_RGS_SYSCV_COARSE_SEL_MASK             0xF
#define MT6333_PMIC_RGS_SYSCV_COARSE_SEL_SHIFT            4
#define MT6333_PMIC_RGS_USB_DL_KEY_MASK                   0x1
#define MT6333_PMIC_RGS_USB_DL_KEY_SHIFT                  0
#define MT6333_PMIC_RGS_FORCE_PP_ON_MASK                  0x1
#define MT6333_PMIC_RGS_FORCE_PP_ON_SHIFT                 1
#define MT6333_PMIC_RGS_INI_SYS_ON_MASK                   0x1
#define MT6333_PMIC_RGS_INI_SYS_ON_SHIFT                  2
#define MT6333_PMIC_RGS_ICH_OC_FLAG_CHR_CORE_MASK         0x1
#define MT6333_PMIC_RGS_ICH_OC_FLAG_CHR_CORE_SHIFT        0
#define MT6333_PMIC_RGS_PWM_OC_CHR_CORE_MASK              0x1
#define MT6333_PMIC_RGS_PWM_OC_CHR_CORE_SHIFT             1
#define MT6333_PMIC_RGS_POWER_ON_READY_MASK               0x1
#define MT6333_PMIC_RGS_POWER_ON_READY_SHIFT              0
#define MT6333_PMIC_RGS_AUTO_PWRON_MASK                   0x1
#define MT6333_PMIC_RGS_AUTO_PWRON_SHIFT                  1
#define MT6333_PMIC_RGS_AUTO_PWRON_DONE_MASK              0x1
#define MT6333_PMIC_RGS_AUTO_PWRON_DONE_SHIFT             2
#define MT6333_PMIC_RGS_CHR_MODE_MASK                     0x1
#define MT6333_PMIC_RGS_CHR_MODE_SHIFT                    3
#define MT6333_PMIC_RGS_OTG_MODE_MASK                     0x1
#define MT6333_PMIC_RGS_OTG_MODE_SHIFT                    4
#define MT6333_PMIC_RGS_POSEQ_DONE_MASK                   0x1
#define MT6333_PMIC_RGS_POSEQ_DONE_SHIFT                  5
#define MT6333_PMIC_RGS_OTG_PRECC_MASK                    0x1
#define MT6333_PMIC_RGS_OTG_PRECC_SHIFT                   6
#define MT6333_PMIC_RGS_CHRIN_SHORT_MASK                  0x1
#define MT6333_PMIC_RGS_CHRIN_SHORT_SHIFT                 0
#define MT6333_PMIC_RGS_DRVCDT_SHORT_MASK                 0x1
#define MT6333_PMIC_RGS_DRVCDT_SHORT_SHIFT                1
#define MT6333_PMIC_RGS_OTG_M3_OC_MASK                    0x1
#define MT6333_PMIC_RGS_OTG_M3_OC_SHIFT                   2
#define MT6333_PMIC_RGS_OTG_THERMAL_MASK                  0x1
#define MT6333_PMIC_RGS_OTG_THERMAL_SHIFT                 3
#define MT6333_PMIC_RGS_CHR_IN_FLASH_MASK                 0x1
#define MT6333_PMIC_RGS_CHR_IN_FLASH_SHIFT                4
#define MT6333_PMIC_RGS_VLED_SHORT_MASK                   0x1
#define MT6333_PMIC_RGS_VLED_SHORT_SHIFT                  5
#define MT6333_PMIC_RGS_VLED_OPEN_MASK                    0x1
#define MT6333_PMIC_RGS_VLED_OPEN_SHIFT                   6
#define MT6333_PMIC_RGS_FLASH_EN_TIMEOUT_MASK             0x1
#define MT6333_PMIC_RGS_FLASH_EN_TIMEOUT_SHIFT            7
#define MT6333_PMIC_RGS_CHR_OC_MASK                       0x1
#define MT6333_PMIC_RGS_CHR_OC_SHIFT                      0
#define MT6333_PMIC_RGS_PWM_EN_MASK                       0x1
#define MT6333_PMIC_RGS_PWM_EN_SHIFT                      2
#define MT6333_PMIC_RGS_OTG_EN_MASK                       0x1
#define MT6333_PMIC_RGS_OTG_EN_SHIFT                      3
#define MT6333_PMIC_RGS_OTG_EN_STB_MASK                   0x1
#define MT6333_PMIC_RGS_OTG_EN_STB_SHIFT                  4
#define MT6333_PMIC_RGS_OTG_DRV_EN_MASK                   0x1
#define MT6333_PMIC_RGS_OTG_DRV_EN_SHIFT                  5
#define MT6333_PMIC_RGS_FLASH_EN_MASK                     0x1
#define MT6333_PMIC_RGS_FLASH_EN_SHIFT                    6
#define MT6333_PMIC_RGS_M3_BOOST_EN_MASK                  0x1
#define MT6333_PMIC_RGS_M3_BOOST_EN_SHIFT                 0
#define MT6333_PMIC_RGS_M3_R_EN_MASK                      0x1
#define MT6333_PMIC_RGS_M3_R_EN_SHIFT                     1
#define MT6333_PMIC_RGS_M3_S_EN_MASK                      0x1
#define MT6333_PMIC_RGS_M3_S_EN_SHIFT                     2
#define MT6333_PMIC_RGS_M3_EN_MASK                        0x1
#define MT6333_PMIC_RGS_M3_EN_SHIFT                       3
#define MT6333_PMIC_RGS_CPCSTSYS_EN_MASK                  0x1
#define MT6333_PMIC_RGS_CPCSTSYS_EN_SHIFT                 4
#define MT6333_PMIC_RGS_SW_GATE_CTRL_MASK                 0x1
#define MT6333_PMIC_RGS_SW_GATE_CTRL_SHIFT                5
#define MT6333_PMIC_QI_OTG_CHR_GT_LV_MASK                 0x1
#define MT6333_PMIC_QI_OTG_CHR_GT_LV_SHIFT                6
#define MT6333_PMIC_RGS_THERMAL_RG_TH_MASK                0xF
#define MT6333_PMIC_RGS_THERMAL_RG_TH_SHIFT               0
#define MT6333_PMIC_RGS_OTG_OC_TH_MASK                    0x3
#define MT6333_PMIC_RGS_OTG_OC_TH_SHIFT                   5
#define MT6333_PMIC_RG_CHR_SUSPEND_MASK                   0x1
#define MT6333_PMIC_RG_CHR_SUSPEND_SHIFT                  0
#define MT6333_PMIC_RG_SYS_ON_MASK                        0x1
#define MT6333_PMIC_RG_SYS_ON_SHIFT                       1
#define MT6333_PMIC_RG_SYS_UNSTABLE_MASK                  0x1
#define MT6333_PMIC_RG_SYS_UNSTABLE_SHIFT                 2
#define MT6333_PMIC_RG_SKIP_EFUSE_OUT_MASK                0x1
#define MT6333_PMIC_RG_SKIP_EFUSE_OUT_SHIFT               0
#define MT6333_PMIC_RG_VSYS_SEL_MASK                      0x3
#define MT6333_PMIC_RG_VSYS_SEL_SHIFT                     2
#define MT6333_PMIC_RG_CV_SEL_MASK                        0xF
#define MT6333_PMIC_RG_CV_SEL_SHIFT                       4
#define MT6333_PMIC_RG_ICH_SEL_MASK                       0xF
#define MT6333_PMIC_RG_ICH_SEL_SHIFT                      0
#define MT6333_PMIC_RG_ICH_PRE_SEL_MASK                   0x3
#define MT6333_PMIC_RG_ICH_PRE_SEL_SHIFT                  6
#define MT6333_PMIC_RG_OC_SEL_MASK                        0x3
#define MT6333_PMIC_RG_OC_SEL_SHIFT                       2
#define MT6333_PMIC_RG_CHRIN_LV_VTH_MASK                  0x3
#define MT6333_PMIC_RG_CHRIN_LV_VTH_SHIFT                 4
#define MT6333_PMIC_RG_CHRIN_HV_VTH_MASK                  0x3
#define MT6333_PMIC_RG_CHRIN_HV_VTH_SHIFT                 6
#define MT6333_PMIC_RG_USBDL_EXT_MASK                     0x1
#define MT6333_PMIC_RG_USBDL_EXT_SHIFT                    0
#define MT6333_PMIC_RG_USBDL_MODE_B_MASK                  0x1
#define MT6333_PMIC_RG_USBDL_MODE_B_SHIFT                 1
#define MT6333_PMIC_RG_USBDL_OC_SEL_MASK                  0xF
#define MT6333_PMIC_RG_USBDL_OC_SEL_SHIFT                 4
#define MT6333_PMIC_RG_BUCK_OVERLOAD_PROT_EN_MASK         0x1
#define MT6333_PMIC_RG_BUCK_OVERLOAD_PROT_EN_SHIFT        0
#define MT6333_PMIC_RG_CH_COMPLETE_AUTO_OFF_MASK          0x1
#define MT6333_PMIC_RG_CH_COMPLETE_AUTO_OFF_SHIFT         1
#define MT6333_PMIC_RG_TERM_TIMER_MASK                    0x3
#define MT6333_PMIC_RG_TERM_TIMER_SHIFT                   2
#define MT6333_PMIC_RG_CHR_OC_AUTO_OFF_MASK               0x1
#define MT6333_PMIC_RG_CHR_OC_AUTO_OFF_SHIFT              0
#define MT6333_PMIC_RG_CHR_OC_RESET_MASK                  0x1
#define MT6333_PMIC_RG_CHR_OC_RESET_SHIFT                 2
#define MT6333_PMIC_RG_OTG_M3_OC_AUTO_OFF_MASK            0x1
#define MT6333_PMIC_RG_OTG_M3_OC_AUTO_OFF_SHIFT           4
#define MT6333_PMIC_RG_OTG_EN_MASK                        0x1
#define MT6333_PMIC_RG_OTG_EN_SHIFT                       0
#define MT6333_PMIC_RG_FLASH_EN_MASK                      0x1
#define MT6333_PMIC_RG_FLASH_EN_SHIFT                     0
#define MT6333_PMIC_RG_FLASH_PWM_EN_MASK                  0x1
#define MT6333_PMIC_RG_FLASH_PWM_EN_SHIFT                 0
#define MT6333_PMIC_RG_FLASH_PWM_EN_STB_MASK              0x1
#define MT6333_PMIC_RG_FLASH_PWM_EN_STB_SHIFT             2
#define MT6333_PMIC_RG_TORCH_MODE_MASK                    0x1
#define MT6333_PMIC_RG_TORCH_MODE_SHIFT                   4
#define MT6333_PMIC_RG_TORCH_CHRIN_CHK_MASK               0x1
#define MT6333_PMIC_RG_TORCH_CHRIN_CHK_SHIFT              6
#define MT6333_PMIC_RG_FLASH_DIM_DUTY_MASK                0x1F
#define MT6333_PMIC_RG_FLASH_DIM_DUTY_SHIFT               0
#define MT6333_PMIC_RG_CHK_CHRIN_TIME_EXT_MASK            0x3
#define MT6333_PMIC_RG_CHK_CHRIN_TIME_EXT_SHIFT           6
#define MT6333_PMIC_RG_FLASH_DIM_FSEL_MASK                0xFF
#define MT6333_PMIC_RG_FLASH_DIM_FSEL_SHIFT               0
#define MT6333_PMIC_RG_FLASH_ISET_MASK                    0xF
#define MT6333_PMIC_RG_FLASH_ISET_SHIFT                   0
#define MT6333_PMIC_RG_FLASH_ISET_STEP_MASK               0x3
#define MT6333_PMIC_RG_FLASH_ISET_STEP_SHIFT              5
#define MT6333_PMIC_RG_THERMAL_RG_TH_MASK                 0xF
#define MT6333_PMIC_RG_THERMAL_RG_TH_SHIFT                0
#define MT6333_PMIC_RG_THERMAL_TEMP_SEL_MASK              0x1
#define MT6333_PMIC_RG_THERMAL_TEMP_SEL_SHIFT             0
#define MT6333_PMIC_RG_THERMAL_CHECKER_SEL_MASK           0x7
#define MT6333_PMIC_RG_THERMAL_CHECKER_SEL_SHIFT          2
#define MT6333_PMIC_RG_FLASH_EN_TIMEOUT_SEL_MASK          0x3
#define MT6333_PMIC_RG_FLASH_EN_TIMEOUT_SEL_SHIFT         6
#define MT6333_PMIC_RG_OTG_OC_TH_MASK                     0x3
#define MT6333_PMIC_RG_OTG_OC_TH_SHIFT                    0
#define MT6333_PMIC_RG_RESERVE_V0_MASK                    0x3F
#define MT6333_PMIC_RG_RESERVE_V0_SHIFT                   2
#define MT6333_PMIC_RG_CV_SEL_USBDL_MASK                  0x3
#define MT6333_PMIC_RG_CV_SEL_USBDL_SHIFT                 0
#define MT6333_PMIC_RG_OV_SEL_USBDL_MASK                  0x3
#define MT6333_PMIC_RG_OV_SEL_USBDL_SHIFT                 3
#define MT6333_PMIC_RG_SW_GATE_CTRL_MASK                  0x1
#define MT6333_PMIC_RG_SW_GATE_CTRL_SHIFT                 0
#define MT6333_PMIC_RG_RESERVE_V1_MASK                    0x3F
#define MT6333_PMIC_RG_RESERVE_V1_SHIFT                   2
#define MT6333_PMIC_RG_RESERVE_V2_MASK                    0xFF
#define MT6333_PMIC_RG_RESERVE_V2_SHIFT                   0
#define MT6333_PMIC_I2C_CONFIG_MASK                       0x1
#define MT6333_PMIC_I2C_CONFIG_SHIFT                      0
#define MT6333_PMIC_I2C_DEG_EN_MASK                       0x1
#define MT6333_PMIC_I2C_DEG_EN_SHIFT                      1
#define MT6333_PMIC_SDA_MODE_MASK                         0x3
#define MT6333_PMIC_SDA_MODE_SHIFT                        0
#define MT6333_PMIC_SDA_OE_MASK                           0x1
#define MT6333_PMIC_SDA_OE_SHIFT                          2
#define MT6333_PMIC_SDA_OUT_MASK                          0x1
#define MT6333_PMIC_SDA_OUT_SHIFT                         3
#define MT6333_PMIC_SCL_MODE_MASK                         0x3
#define MT6333_PMIC_SCL_MODE_SHIFT                        4
#define MT6333_PMIC_SCL_OE_MASK                           0x1
#define MT6333_PMIC_SCL_OE_SHIFT                          6
#define MT6333_PMIC_SCL_OUT_MASK                          0x1
#define MT6333_PMIC_SCL_OUT_SHIFT                         7
#define MT6333_PMIC_INT_MODE_MASK                         0x3
#define MT6333_PMIC_INT_MODE_SHIFT                        0
#define MT6333_PMIC_INT_OE_MASK                           0x1
#define MT6333_PMIC_INT_OE_SHIFT                          2
#define MT6333_PMIC_INT_OUT_MASK                          0x1
#define MT6333_PMIC_INT_OUT_SHIFT                         3
#define MT6333_PMIC_RG_CHR_250K_CK_EN_MASK                0x1
#define MT6333_PMIC_RG_CHR_250K_CK_EN_SHIFT               0
#define MT6333_PMIC_RG_CHR_1M_CK_EN_MASK                  0x1
#define MT6333_PMIC_RG_CHR_1M_CK_EN_SHIFT                 1
#define MT6333_PMIC_RG_CHR_PWM_CK_EN_MASK                 0x1
#define MT6333_PMIC_RG_CHR_PWM_CK_EN_SHIFT                2
#define MT6333_PMIC_RG_BUCK_1M_CK_EN_MASK                 0x1
#define MT6333_PMIC_RG_BUCK_1M_CK_EN_SHIFT                4
#define MT6333_PMIC_RG_BUCK_2M_CK_EN_MASK                 0x1
#define MT6333_PMIC_RG_BUCK_2M_CK_EN_SHIFT                5
#define MT6333_PMIC_RG_BUCK_3M_CK_EN_MASK                 0x1
#define MT6333_PMIC_RG_BUCK_3M_CK_EN_SHIFT                6
#define MT6333_PMIC_RG_BUCK_6M_CK_EN_MASK                 0x1
#define MT6333_PMIC_RG_BUCK_6M_CK_EN_SHIFT                7
#define MT6333_PMIC_CLK_CON0_SET_MASK                     0x1
#define MT6333_PMIC_CLK_CON0_SET_SHIFT                    7
#define MT6333_PMIC_CLK_CON0_CLR_MASK                     0x1
#define MT6333_PMIC_CLK_CON0_CLR_SHIFT                    7
#define MT6333_PMIC_RG_BUCK_OSC_EN_MASK                   0x1
#define MT6333_PMIC_RG_BUCK_OSC_EN_SHIFT                  0
#define MT6333_PMIC_QI_OSC_EN_MASK                        0x1
#define MT6333_PMIC_QI_OSC_EN_SHIFT                       7
#define MT6333_PMIC_RG_BUCK_CALI_32K_CK_EN_MASK           0x1
#define MT6333_PMIC_RG_BUCK_CALI_32K_CK_EN_SHIFT          0
#define MT6333_PMIC_RG_BUCK_CALI_PWM_CK_EN_MASK           0x1
#define MT6333_PMIC_RG_BUCK_CALI_PWM_CK_EN_SHIFT          1
#define MT6333_PMIC_RG_BUCK_CALI_6M_CK_EN_MASK            0x1
#define MT6333_PMIC_RG_BUCK_CALI_6M_CK_EN_SHIFT           2
#define MT6333_PMIC_RG_TEST_EFUSE_MASK                    0x1
#define MT6333_PMIC_RG_TEST_EFUSE_SHIFT                   3
#define MT6333_PMIC_RG_TEST_NI_CK_MASK                    0x1
#define MT6333_PMIC_RG_TEST_NI_CK_SHIFT                   4
#define MT6333_PMIC_RG_TEST_SMPS_CK_MASK                  0x1
#define MT6333_PMIC_RG_TEST_SMPS_CK_SHIFT                 5
#define MT6333_PMIC_RG_TEST_PWM_CK_MASK                   0x1
#define MT6333_PMIC_RG_TEST_PWM_CK_SHIFT                  6
#define MT6333_PMIC_RG_INT_EN_CHR_COMPLETE_MASK           0x1
#define MT6333_PMIC_RG_INT_EN_CHR_COMPLETE_SHIFT          0
#define MT6333_PMIC_RG_INT_EN_THERMAL_SD_MASK             0x1
#define MT6333_PMIC_RG_INT_EN_THERMAL_SD_SHIFT            1
#define MT6333_PMIC_RG_INT_EN_THERMAL_REG_IN_MASK         0x1
#define MT6333_PMIC_RG_INT_EN_THERMAL_REG_IN_SHIFT        2
#define MT6333_PMIC_RG_INT_EN_THERMAL_REG_OUT_MASK        0x1
#define MT6333_PMIC_RG_INT_EN_THERMAL_REG_OUT_SHIFT       3
#define MT6333_PMIC_RG_INT_EN_OTG_OC_MASK                 0x1
#define MT6333_PMIC_RG_INT_EN_OTG_OC_SHIFT                4
#define MT6333_PMIC_RG_INT_EN_OTG_THERMAL_MASK            0x1
#define MT6333_PMIC_RG_INT_EN_OTG_THERMAL_SHIFT           5
#define MT6333_PMIC_RG_INT_EN_OTG_CHRIN_SHORT_MASK        0x1
#define MT6333_PMIC_RG_INT_EN_OTG_CHRIN_SHORT_SHIFT       6
#define MT6333_PMIC_RG_INT_EN_OTG_DRVCDT_SHORT_MASK       0x1
#define MT6333_PMIC_RG_INT_EN_OTG_DRVCDT_SHORT_SHIFT      7
#define MT6333_PMIC_RG_INT_EN_CHR_COMPLETE_SET_MASK       0x1
#define MT6333_PMIC_RG_INT_EN_CHR_COMPLETE_SET_SHIFT      0
#define MT6333_PMIC_RG_INT_EN_THERMAL_SD_SET_MASK         0x1
#define MT6333_PMIC_RG_INT_EN_THERMAL_SD_SET_SHIFT        1
#define MT6333_PMIC_RG_INT_EN_THERMAL_REG_IN_SET_MASK     0x1
#define MT6333_PMIC_RG_INT_EN_THERMAL_REG_IN_SET_SHIFT    2
#define MT6333_PMIC_RG_INT_EN_THERMAL_REG_OUT_SET_MASK    0x1
#define MT6333_PMIC_RG_INT_EN_THERMAL_REG_OUT_SET_SHIFT   3
#define MT6333_PMIC_RG_INT_EN_OTG_OC_SET_MASK             0x1
#define MT6333_PMIC_RG_INT_EN_OTG_OC_SET_SHIFT            4
#define MT6333_PMIC_RG_INT_EN_OTG_THERMAL_SET_MASK        0x1
#define MT6333_PMIC_RG_INT_EN_OTG_THERMAL_SET_SHIFT       5
#define MT6333_PMIC_RG_INT_EN_OTG_CHRIN_SHORT_SET_MASK    0x1
#define MT6333_PMIC_RG_INT_EN_OTG_CHRIN_SHORT_SET_SHIFT   6
#define MT6333_PMIC_RG_INT_EN_OTG_DRVCDT_SHORT_SET_MASK   0x1
#define MT6333_PMIC_RG_INT_EN_OTG_DRVCDT_SHORT_SET_SHIFT  7
#define MT6333_PMIC_RG_INT_EN_CHR_COMPLETE_CLR_MASK       0x1
#define MT6333_PMIC_RG_INT_EN_CHR_COMPLETE_CLR_SHIFT      0
#define MT6333_PMIC_RG_INT_EN_THERMAL_SD_CLR_MASK         0x1
#define MT6333_PMIC_RG_INT_EN_THERMAL_SD_CLR_SHIFT        1
#define MT6333_PMIC_RG_INT_EN_THERMAL_REG_IN_CLR_MASK     0x1
#define MT6333_PMIC_RG_INT_EN_THERMAL_REG_IN_CLR_SHIFT    2
#define MT6333_PMIC_RG_INT_EN_THERMAL_REG_OUT_CLR_MASK    0x1
#define MT6333_PMIC_RG_INT_EN_THERMAL_REG_OUT_CLR_SHIFT   3
#define MT6333_PMIC_RG_INT_EN_OTG_OC_CLR_MASK             0x1
#define MT6333_PMIC_RG_INT_EN_OTG_OC_CLR_SHIFT            4
#define MT6333_PMIC_RG_INT_EN_OTG_THERMAL_CLR_MASK        0x1
#define MT6333_PMIC_RG_INT_EN_OTG_THERMAL_CLR_SHIFT       5
#define MT6333_PMIC_RG_INT_EN_OTG_CHRIN_SHORT_CLR_MASK    0x1
#define MT6333_PMIC_RG_INT_EN_OTG_CHRIN_SHORT_CLR_SHIFT   6
#define MT6333_PMIC_RG_INT_EN_OTG_DRVCDT_SHORT_CLR_MASK   0x1
#define MT6333_PMIC_RG_INT_EN_OTG_DRVCDT_SHORT_CLR_SHIFT  7
#define MT6333_PMIC_RG_INT_EN_CHRWDT_FLAG_MASK            0x1
#define MT6333_PMIC_RG_INT_EN_CHRWDT_FLAG_SHIFT           0
#define MT6333_PMIC_RG_INT_EN_BUCK_VCORE_OC_MASK          0x1
#define MT6333_PMIC_RG_INT_EN_BUCK_VCORE_OC_SHIFT         1
#define MT6333_PMIC_RG_INT_EN_BUCK_VMEM_OC_MASK           0x1
#define MT6333_PMIC_RG_INT_EN_BUCK_VMEM_OC_SHIFT          2
#define MT6333_PMIC_RG_INT_EN_BUCK_VRF18_OC_MASK          0x1
#define MT6333_PMIC_RG_INT_EN_BUCK_VRF18_OC_SHIFT         3
#define MT6333_PMIC_RG_INT_EN_BUCK_THERMAL_MASK           0x1
#define MT6333_PMIC_RG_INT_EN_BUCK_THERMAL_SHIFT          4
#define MT6333_PMIC_RG_INT_EN_FLASH_EN_TIMEOUT_MASK       0x1
#define MT6333_PMIC_RG_INT_EN_FLASH_EN_TIMEOUT_SHIFT      5
#define MT6333_PMIC_RG_INT_EN_FLASH_VLED_SHORT_MASK       0x1
#define MT6333_PMIC_RG_INT_EN_FLASH_VLED_SHORT_SHIFT      6
#define MT6333_PMIC_RG_INT_EN_FLASH_VLED_OPEN_MASK        0x1
#define MT6333_PMIC_RG_INT_EN_FLASH_VLED_OPEN_SHIFT       7
#define MT6333_PMIC_RG_INT_EN_CHRWDT_FLAG_SET_MASK        0x1
#define MT6333_PMIC_RG_INT_EN_CHRWDT_FLAG_SET_SHIFT       0
#define MT6333_PMIC_RG_INT_EN_BUCK_VCORE_OC_SET_MASK      0x1
#define MT6333_PMIC_RG_INT_EN_BUCK_VCORE_OC_SET_SHIFT     1
#define MT6333_PMIC_RG_INT_EN_BUCK_VMEM_OC_SET_MASK       0x1
#define MT6333_PMIC_RG_INT_EN_BUCK_VMEM_OC_SET_SHIFT      2
#define MT6333_PMIC_RG_INT_EN_BUCK_VRF18_OC_SET_MASK      0x1
#define MT6333_PMIC_RG_INT_EN_BUCK_VRF18_OC_SET_SHIFT     3
#define MT6333_PMIC_RG_INT_EN_BUCK_THERMAL_SET_MASK       0x1
#define MT6333_PMIC_RG_INT_EN_BUCK_THERMAL_SET_SHIFT      4
#define MT6333_PMIC_RG_INT_EN_FLASH_EN_TIMEOUT_SET_MASK   0x1
#define MT6333_PMIC_RG_INT_EN_FLASH_EN_TIMEOUT_SET_SHIFT  5
#define MT6333_PMIC_RG_INT_EN_FLASH_VLED_SHORT_SET_MASK   0x1
#define MT6333_PMIC_RG_INT_EN_FLASH_VLED_SHORT_SET_SHIFT  6
#define MT6333_PMIC_RG_INT_EN_FLASH_VLED_OPEN_SET_MASK    0x1
#define MT6333_PMIC_RG_INT_EN_FLASH_VLED_OPEN_SET_SHIFT   7
#define MT6333_PMIC_RG_INT_EN_CHRWDT_FLAG_CLR_MASK        0x1
#define MT6333_PMIC_RG_INT_EN_CHRWDT_FLAG_CLR_SHIFT       0
#define MT6333_PMIC_RG_INT_EN_BUCK_VCORE_OC_CLR_MASK      0x1
#define MT6333_PMIC_RG_INT_EN_BUCK_VCORE_OC_CLR_SHIFT     1
#define MT6333_PMIC_RG_INT_EN_BUCK_VMEM_OC_CLR_MASK       0x1
#define MT6333_PMIC_RG_INT_EN_BUCK_VMEM_OC_CLR_SHIFT      2
#define MT6333_PMIC_RG_INT_EN_BUCK_VRF18_OC_CLR_MASK      0x1
#define MT6333_PMIC_RG_INT_EN_BUCK_VRF18_OC_CLR_SHIFT     3
#define MT6333_PMIC_RG_INT_EN_BUCK_THERMAL_CLR_MASK       0x1
#define MT6333_PMIC_RG_INT_EN_BUCK_THERMAL_CLR_SHIFT      4
#define MT6333_PMIC_RG_INT_EN_FLASH_EN_TIMEOUT_CLR_MASK   0x1
#define MT6333_PMIC_RG_INT_EN_FLASH_EN_TIMEOUT_CLR_SHIFT  5
#define MT6333_PMIC_RG_INT_EN_FLASH_VLED_SHORT_CLR_MASK   0x1
#define MT6333_PMIC_RG_INT_EN_FLASH_VLED_SHORT_CLR_SHIFT  6
#define MT6333_PMIC_RG_INT_EN_FLASH_VLED_OPEN_CLR_MASK    0x1
#define MT6333_PMIC_RG_INT_EN_FLASH_VLED_OPEN_CLR_SHIFT   7
#define MT6333_PMIC_RG_INT_EN_CHR_OC_MASK                 0x1
#define MT6333_PMIC_RG_INT_EN_CHR_OC_SHIFT                0
#define MT6333_PMIC_RG_INT_EN_CHR_PLUG_IN_FLASH_MASK      0x1
#define MT6333_PMIC_RG_INT_EN_CHR_PLUG_IN_FLASH_SHIFT     1
#define MT6333_PMIC_RG_INT_EN_CHR_OC_SET_MASK             0x1
#define MT6333_PMIC_RG_INT_EN_CHR_OC_SET_SHIFT            0
#define MT6333_PMIC_RG_INT_EN_CHR_PLUG_IN_FLASH_SET_MASK  0x1
#define MT6333_PMIC_RG_INT_EN_CHR_PLUG_IN_FLASH_SET_SHIFT 1
#define MT6333_PMIC_RG_INT_EN_CHR_OC_CLR_MASK             0x1
#define MT6333_PMIC_RG_INT_EN_CHR_OC_CLR_SHIFT            0
#define MT6333_PMIC_RG_INT_EN_CHR_PLUG_IN_FLASH_CLR_MASK  0x1
#define MT6333_PMIC_RG_INT_EN_CHR_PLUG_IN_FLASH_CLR_SHIFT 1
#define MT6333_PMIC_RG_CHRWDT_EN_MASK                     0x1
#define MT6333_PMIC_RG_CHRWDT_EN_SHIFT                    0
#define MT6333_PMIC_RG_CHRWDT_TD_MASK                     0x7
#define MT6333_PMIC_RG_CHRWDT_TD_SHIFT                    1
#define MT6333_PMIC_RG_CHRWDT_WR_MASK                     0x1
#define MT6333_PMIC_RG_CHRWDT_WR_SHIFT                    7
#define MT6333_PMIC_RG_CHRWDT_FLAG_MASK                   0x1
#define MT6333_PMIC_RG_CHRWDT_FLAG_SHIFT                  0
#define MT6333_PMIC_RG_INT_STATUS_CHR_COMPLETE_MASK       0x1
#define MT6333_PMIC_RG_INT_STATUS_CHR_COMPLETE_SHIFT      0
#define MT6333_PMIC_RG_INT_STATUS_THERMAL_SD_MASK         0x1
#define MT6333_PMIC_RG_INT_STATUS_THERMAL_SD_SHIFT        1
#define MT6333_PMIC_RG_INT_STATUS_THERMAL_REG_IN_MASK     0x1
#define MT6333_PMIC_RG_INT_STATUS_THERMAL_REG_IN_SHIFT    2
#define MT6333_PMIC_RG_INT_STATUS_THERMAL_REG_OUT_MASK    0x1
#define MT6333_PMIC_RG_INT_STATUS_THERMAL_REG_OUT_SHIFT   3
#define MT6333_PMIC_RG_INT_STATUS_OTG_OC_MASK             0x1
#define MT6333_PMIC_RG_INT_STATUS_OTG_OC_SHIFT            4
#define MT6333_PMIC_RG_INT_STATUS_OTG_THERMAL_MASK        0x1
#define MT6333_PMIC_RG_INT_STATUS_OTG_THERMAL_SHIFT       5
#define MT6333_PMIC_RG_INT_STATUS_OTG_CHRIN_SHORT_MASK    0x1
#define MT6333_PMIC_RG_INT_STATUS_OTG_CHRIN_SHORT_SHIFT   6
#define MT6333_PMIC_RG_INT_STATUS_OTG_DRVCDT_SHORT_MASK   0x1
#define MT6333_PMIC_RG_INT_STATUS_OTG_DRVCDT_SHORT_SHIFT  7
#define MT6333_PMIC_RG_INT_STATUS_CHRWDT_FLAG_MASK        0x1
#define MT6333_PMIC_RG_INT_STATUS_CHRWDT_FLAG_SHIFT       0
#define MT6333_PMIC_RG_INT_STATUS_BUCK_VCORE_OC_MASK      0x1
#define MT6333_PMIC_RG_INT_STATUS_BUCK_VCORE_OC_SHIFT     1
#define MT6333_PMIC_RG_INT_STATUS_BUCK_VMEM_OC_MASK       0x1
#define MT6333_PMIC_RG_INT_STATUS_BUCK_VMEM_OC_SHIFT      2
#define MT6333_PMIC_RG_INT_STATUS_BUCK_VRF18_OC_MASK      0x1
#define MT6333_PMIC_RG_INT_STATUS_BUCK_VRF18_OC_SHIFT     3
#define MT6333_PMIC_RG_INT_STATUS_BUCK_THERMAL_MASK       0x1
#define MT6333_PMIC_RG_INT_STATUS_BUCK_THERMAL_SHIFT      4
#define MT6333_PMIC_RG_INT_STATUS_FLASH_EN_TIMEOUT_MASK   0x1
#define MT6333_PMIC_RG_INT_STATUS_FLASH_EN_TIMEOUT_SHIFT  5
#define MT6333_PMIC_RG_INT_STATUS_FLASH_VLED_SHORT_MASK   0x1
#define MT6333_PMIC_RG_INT_STATUS_FLASH_VLED_SHORT_SHIFT  6
#define MT6333_PMIC_RG_INT_STATUS_FLASH_VLED_OPEN_MASK    0x1
#define MT6333_PMIC_RG_INT_STATUS_FLASH_VLED_OPEN_SHIFT   7
#define MT6333_PMIC_RG_INT_STATUS_CHR_OC_MASK             0x1
#define MT6333_PMIC_RG_INT_STATUS_CHR_OC_SHIFT            0
#define MT6333_PMIC_RG_INT_STATUS_CHR_PLUG_IN_FLASH_MASK  0x1
#define MT6333_PMIC_RG_INT_STATUS_CHR_PLUG_IN_FLASH_SHIFT 1
#define MT6333_PMIC_VCORE_DEG_EN_MASK                     0x1
#define MT6333_PMIC_VCORE_DEG_EN_SHIFT                    0
#define MT6333_PMIC_VCORE_OC_WND_MASK                     0x3
#define MT6333_PMIC_VCORE_OC_WND_SHIFT                    2
#define MT6333_PMIC_VCORE_OC_THD_MASK                     0x3
#define MT6333_PMIC_VCORE_OC_THD_SHIFT                    5
#define MT6333_PMIC_VMEM_DEG_EN_MASK                      0x1
#define MT6333_PMIC_VMEM_DEG_EN_SHIFT                     0
#define MT6333_PMIC_VMEM_OC_WND_MASK                      0x3
#define MT6333_PMIC_VMEM_OC_WND_SHIFT                     2
#define MT6333_PMIC_VMEM_OC_THD_MASK                      0x3
#define MT6333_PMIC_VMEM_OC_THD_SHIFT                     5
#define MT6333_PMIC_VRF18_DEG_EN_MASK                     0x1
#define MT6333_PMIC_VRF18_DEG_EN_SHIFT                    0
#define MT6333_PMIC_VRF18_OC_WND_MASK                     0x3
#define MT6333_PMIC_VRF18_OC_WND_SHIFT                    2
#define MT6333_PMIC_VRF18_OC_THD_MASK                     0x3
#define MT6333_PMIC_VRF18_OC_THD_SHIFT                    5
#define MT6333_PMIC_INT_POLARITY_MASK                     0x1
#define MT6333_PMIC_INT_POLARITY_SHIFT                    0
#define MT6333_PMIC_RG_SMPS_TESTMODE_B_MASK               0xFF
#define MT6333_PMIC_RG_SMPS_TESTMODE_B_SHIFT              0
#define MT6333_PMIC_RG_VCORE_TRIML_MASK                   0x7
#define MT6333_PMIC_RG_VCORE_TRIML_SHIFT                  0
#define MT6333_PMIC_RG_VCORE_TRIMH_MASK                   0x7
#define MT6333_PMIC_RG_VCORE_TRIMH_SHIFT                  5
#define MT6333_PMIC_RG_VCORE_CC_MASK                      0x3
#define MT6333_PMIC_RG_VCORE_CC_SHIFT                     3
#define MT6333_PMIC_RG_VCORE_RZSEL_MASK                   0x3
#define MT6333_PMIC_RG_VCORE_RZSEL_SHIFT                  6
#define MT6333_PMIC_RG_VCORE_SLP_MASK                     0x3
#define MT6333_PMIC_RG_VCORE_SLP_SHIFT                    2
#define MT6333_PMIC_RG_VCORE_CSL_MASK                     0x3
#define MT6333_PMIC_RG_VCORE_CSL_SHIFT                    4
#define MT6333_PMIC_RG_VCORE_CSR_MASK                     0x3
#define MT6333_PMIC_RG_VCORE_CSR_SHIFT                    6
#define MT6333_PMIC_RG_VCORE_AVP_OS_MASK                  0x7
#define MT6333_PMIC_RG_VCORE_AVP_OS_SHIFT                 0
#define MT6333_PMIC_RG_VCORE_AVP_EN_MASK                  0x1
#define MT6333_PMIC_RG_VCORE_AVP_EN_SHIFT                 3
#define MT6333_PMIC_RG_VCORE_NDIS_EN_MASK                 0x1
#define MT6333_PMIC_RG_VCORE_NDIS_EN_SHIFT                4
#define MT6333_PMIC_RG_VCORE_MODESET_MASK                 0x1
#define MT6333_PMIC_RG_VCORE_MODESET_SHIFT                5
#define MT6333_PMIC_RG_VCORE_ZX_OS_MASK                   0x3
#define MT6333_PMIC_RG_VCORE_ZX_OS_SHIFT                  6
#define MT6333_PMIC_RG_VCORE_CSM_MASK                     0x7
#define MT6333_PMIC_RG_VCORE_CSM_SHIFT                    5
#define MT6333_PMIC_RG_VCORE_ZXOS_TRIM_MASK               0x3F
#define MT6333_PMIC_RG_VCORE_ZXOS_TRIM_SHIFT              2
#define MT6333_PMIC_RG_VCORE_RSV_MASK                     0xFF
#define MT6333_PMIC_RG_VCORE_RSV_SHIFT                    0
#define MT6333_PMIC_QI_VCORE_DIG_MON_MASK                 0xF
#define MT6333_PMIC_QI_VCORE_DIG_MON_SHIFT                0
#define MT6333_PMIC_QI_VCORE_OC_STATUS_MASK               0x1
#define MT6333_PMIC_QI_VCORE_OC_STATUS_SHIFT              7
#define MT6333_PMIC_VSLEEP_SRC1_MASK                      0xF
#define MT6333_PMIC_VSLEEP_SRC1_SHIFT                     4
#define MT6333_PMIC_VSLEEP_SRC0_7_0_MASK                  0xFF
#define MT6333_PMIC_VSLEEP_SRC0_7_0_SHIFT                 0
#define MT6333_PMIC_VSLEEP_SRC0_8_MASK                    0x1
#define MT6333_PMIC_VSLEEP_SRC0_8_SHIFT                   7
#define MT6333_PMIC_R2R_SRC0_8_MASK                       0x1
#define MT6333_PMIC_R2R_SRC0_8_SHIFT                      0
#define MT6333_PMIC_R2R_SRC1_MASK                         0xF
#define MT6333_PMIC_R2R_SRC1_SHIFT                        4
#define MT6333_PMIC_R2R_SRC0_7_0_MASK                     0xFF
#define MT6333_PMIC_R2R_SRC0_7_0_SHIFT                    0
#define MT6333_PMIC_SRCLKEN_DLY_SRC1_MASK                 0xF
#define MT6333_PMIC_SRCLKEN_DLY_SRC1_SHIFT                4
#define MT6333_PMIC_RG_BUCK_RSV0_MASK                     0xFF
#define MT6333_PMIC_RG_BUCK_RSV0_SHIFT                    0
#define MT6333_PMIC_RG_SRCLKEN_DLY_SEL_MASK               0x1
#define MT6333_PMIC_RG_SRCLKEN_DLY_SEL_SHIFT              6
#define MT6333_PMIC_RG_R2R_EVENT_SEL_MASK                 0x1
#define MT6333_PMIC_RG_R2R_EVENT_SEL_SHIFT                7
#define MT6333_PMIC_QI_VCORE_VSLEEP_MASK                  0x3
#define MT6333_PMIC_QI_VCORE_VSLEEP_SHIFT                 6
#define MT6333_PMIC_VCORE_EN_MASK                         0x1
#define MT6333_PMIC_VCORE_EN_SHIFT                        1
#define MT6333_PMIC_QI_VCORE_STB_MASK                     0x1
#define MT6333_PMIC_QI_VCORE_STB_SHIFT                    2
#define MT6333_PMIC_QI_VCORE_EN_MASK                      0x1
#define MT6333_PMIC_QI_VCORE_EN_SHIFT                     3
#define MT6333_PMIC_VCORE_EN_CTRL_MASK                    0x1
#define MT6333_PMIC_VCORE_EN_CTRL_SHIFT                   4
#define MT6333_PMIC_VCORE_VOSEL_CTRL_MASK                 0x1
#define MT6333_PMIC_VCORE_VOSEL_CTRL_SHIFT                5
#define MT6333_PMIC_VCORE_DLC_CTRL_MASK                   0x1
#define MT6333_PMIC_VCORE_DLC_CTRL_SHIFT                  6
#define MT6333_PMIC_VCORE_BURST_CTRL_MASK                 0x1
#define MT6333_PMIC_VCORE_BURST_CTRL_SHIFT                7
#define MT6333_PMIC_VCORE_SFCHG_REN_MASK                  0x1
#define MT6333_PMIC_VCORE_SFCHG_REN_SHIFT                 0
#define MT6333_PMIC_VCORE_SFCHG_RRATE_MASK                0x7F
#define MT6333_PMIC_VCORE_SFCHG_RRATE_SHIFT               1
#define MT6333_PMIC_VCORE_SFCHG_FEN_MASK                  0x1
#define MT6333_PMIC_VCORE_SFCHG_FEN_SHIFT                 0
#define MT6333_PMIC_VCORE_SFCHG_FRATE_MASK                0x7F
#define MT6333_PMIC_VCORE_SFCHG_FRATE_SHIFT               1
#define MT6333_PMIC_VCORE_VOSEL_MASK                      0x7F
#define MT6333_PMIC_VCORE_VOSEL_SHIFT                     0
#define MT6333_PMIC_VCORE_VOSEL_ON_MASK                   0x7F
#define MT6333_PMIC_VCORE_VOSEL_ON_SHIFT                  0
#define MT6333_PMIC_VCORE_VOSEL_SLEEP_MASK                0x7F
#define MT6333_PMIC_VCORE_VOSEL_SLEEP_SHIFT               0
#define MT6333_PMIC_NI_VCORE_VOSEL_MASK                   0x7F
#define MT6333_PMIC_NI_VCORE_VOSEL_SHIFT                  0
#define MT6333_PMIC_VCORE_BURST_MASK                      0x3
#define MT6333_PMIC_VCORE_BURST_SHIFT                     0
#define MT6333_PMIC_VCORE_BURST_ON_MASK                   0x3
#define MT6333_PMIC_VCORE_BURST_ON_SHIFT                  2
#define MT6333_PMIC_VCORE_BURST_SLEEP_MASK                0x3
#define MT6333_PMIC_VCORE_BURST_SLEEP_SHIFT               4
#define MT6333_PMIC_QI_VCORE_BURST_MASK                   0x3
#define MT6333_PMIC_QI_VCORE_BURST_SHIFT                  6
#define MT6333_PMIC_VCORE_DLC_MASK                        0x3
#define MT6333_PMIC_VCORE_DLC_SHIFT                       0
#define MT6333_PMIC_VCORE_DLC_ON_MASK                     0x3
#define MT6333_PMIC_VCORE_DLC_ON_SHIFT                    2
#define MT6333_PMIC_VCORE_DLC_SLEEP_MASK                  0x3
#define MT6333_PMIC_VCORE_DLC_SLEEP_SHIFT                 4
#define MT6333_PMIC_QI_VCORE_DLC_MASK                     0x3
#define MT6333_PMIC_QI_VCORE_DLC_SHIFT                    6
#define MT6333_PMIC_VCORE_DLC_N_MASK                      0x3
#define MT6333_PMIC_VCORE_DLC_N_SHIFT                     0
#define MT6333_PMIC_VCORE_DLC_N_ON_MASK                   0x3
#define MT6333_PMIC_VCORE_DLC_N_ON_SHIFT                  2
#define MT6333_PMIC_VCORE_DLC_N_SLEEP_MASK                0x3
#define MT6333_PMIC_VCORE_DLC_N_SLEEP_SHIFT               4
#define MT6333_PMIC_QI_VCORE_DLC_N_MASK                   0x3
#define MT6333_PMIC_QI_VCORE_DLC_N_SHIFT                  6
#define MT6333_PMIC_VCORE_VSLEEP_EN_MASK                  0x1
#define MT6333_PMIC_VCORE_VSLEEP_EN_SHIFT                 3
#define MT6333_PMIC_VCORE_R2R_PDN_MASK                    0x1
#define MT6333_PMIC_VCORE_R2R_PDN_SHIFT                   4
#define MT6333_PMIC_VCORE_VSLEEP_SEL_MASK                 0x1
#define MT6333_PMIC_VCORE_VSLEEP_SEL_SHIFT                5
#define MT6333_PMIC_NI_VCORE_R2R_PDN_MASK                 0x1
#define MT6333_PMIC_NI_VCORE_R2R_PDN_SHIFT                6
#define MT6333_PMIC_NI_VCORE_VSLEEP_SEL_MASK              0x1
#define MT6333_PMIC_NI_VCORE_VSLEEP_SEL_SHIFT             7
#define MT6333_PMIC_VCORE_TRANSTD_MASK                    0x3
#define MT6333_PMIC_VCORE_TRANSTD_SHIFT                   0
#define MT6333_PMIC_VCORE_VOSEL_TRANS_EN_MASK             0x3
#define MT6333_PMIC_VCORE_VOSEL_TRANS_EN_SHIFT            2
#define MT6333_PMIC_VCORE_VOSEL_TRANS_ONCE_MASK           0x1
#define MT6333_PMIC_VCORE_VOSEL_TRANS_ONCE_SHIFT          4
#define MT6333_PMIC_NI_VCORE_VOSEL_TRANS_MASK             0x1
#define MT6333_PMIC_NI_VCORE_VOSEL_TRANS_SHIFT            5
#define MT6333_PMIC_VCORE_SFCHG_FEN_SLEEP_MASK            0x1
#define MT6333_PMIC_VCORE_SFCHG_FEN_SLEEP_SHIFT           0
#define MT6333_PMIC_VCORE_SFCHG_REN_SLEEP_MASK            0x1
#define MT6333_PMIC_VCORE_SFCHG_REN_SLEEP_SHIFT           1
#define MT6333_PMIC_VCORE_VOSEL_ON_SPM_MASK               0x7F
#define MT6333_PMIC_VCORE_VOSEL_ON_SPM_SHIFT              0
#define MT6333_PMIC_RG_VMEM_RSV1_5_0_MASK                 0x3F
#define MT6333_PMIC_RG_VMEM_RSV1_5_0_SHIFT                0
#define MT6333_PMIC_RG_R2R_EVENT_SYNC_SEL_MASK            0x1
#define MT6333_PMIC_RG_R2R_EVENT_SYNC_SEL_SHIFT           6
#define MT6333_PMIC_RG_VMEM_EN_THR_SDN_SEL_MASK           0x1
#define MT6333_PMIC_RG_VMEM_EN_THR_SDN_SEL_SHIFT          7
#define MT6333_PMIC_RG_VMEM_TRIML_MASK                    0x7
#define MT6333_PMIC_RG_VMEM_TRIML_SHIFT                   0
#define MT6333_PMIC_RG_VMEM_TRIMH_MASK                    0x7
#define MT6333_PMIC_RG_VMEM_TRIMH_SHIFT                   5
#define MT6333_PMIC_RG_VMEM_CC_MASK                       0x3
#define MT6333_PMIC_RG_VMEM_CC_SHIFT                      3
#define MT6333_PMIC_RG_VMEM_RZSEL_MASK                    0x3
#define MT6333_PMIC_RG_VMEM_RZSEL_SHIFT                   6
#define MT6333_PMIC_RG_VMEM_SLP_MASK                      0x3
#define MT6333_PMIC_RG_VMEM_SLP_SHIFT                     2
#define MT6333_PMIC_RG_VMEM_CSL_MASK                      0x3
#define MT6333_PMIC_RG_VMEM_CSL_SHIFT                     4
#define MT6333_PMIC_RG_VMEM_CSR_MASK                      0x3
#define MT6333_PMIC_RG_VMEM_CSR_SHIFT                     6
#define MT6333_PMIC_RG_VMEM_AVP_OS_MASK                   0x7
#define MT6333_PMIC_RG_VMEM_AVP_OS_SHIFT                  0
#define MT6333_PMIC_RG_VMEM_AVP_EN_MASK                   0x1
#define MT6333_PMIC_RG_VMEM_AVP_EN_SHIFT                  3
#define MT6333_PMIC_RG_VMEM_NDIS_EN_MASK                  0x1
#define MT6333_PMIC_RG_VMEM_NDIS_EN_SHIFT                 4
#define MT6333_PMIC_RG_VMEM_MODESET_MASK                  0x1
#define MT6333_PMIC_RG_VMEM_MODESET_SHIFT                 5
#define MT6333_PMIC_RG_VMEM_ZX_OS_MASK                    0x3
#define MT6333_PMIC_RG_VMEM_ZX_OS_SHIFT                   6
#define MT6333_PMIC_RG_VMEM_CSM_MASK                      0x7
#define MT6333_PMIC_RG_VMEM_CSM_SHIFT                     5
#define MT6333_PMIC_RG_VMEM_ZXOS_TRIM_MASK                0x3F
#define MT6333_PMIC_RG_VMEM_ZXOS_TRIM_SHIFT               2
#define MT6333_PMIC_RG_VMEM_RSV_MASK                      0xFF
#define MT6333_PMIC_RG_VMEM_RSV_SHIFT                     0
#define MT6333_PMIC_QI_VMEM_DIG_MON_MASK                  0xF
#define MT6333_PMIC_QI_VMEM_DIG_MON_SHIFT                 0
#define MT6333_PMIC_QI_VMEM_OC_STATUS_MASK                0x1
#define MT6333_PMIC_QI_VMEM_OC_STATUS_SHIFT               7
#define MT6333_PMIC_QI_VMEM_VSLEEP_MASK                   0x3
#define MT6333_PMIC_QI_VMEM_VSLEEP_SHIFT                  6
#define MT6333_PMIC_VMEM_EN_MASK                          0x1
#define MT6333_PMIC_VMEM_EN_SHIFT                         1
#define MT6333_PMIC_QI_VMEM_STB_MASK                      0x1
#define MT6333_PMIC_QI_VMEM_STB_SHIFT                     2
#define MT6333_PMIC_QI_VMEM_EN_MASK                       0x1
#define MT6333_PMIC_QI_VMEM_EN_SHIFT                      3
#define MT6333_PMIC_VMEM_EN_CTRL_MASK                     0x1
#define MT6333_PMIC_VMEM_EN_CTRL_SHIFT                    4
#define MT6333_PMIC_VMEM_VOSEL_CTRL_MASK                  0x1
#define MT6333_PMIC_VMEM_VOSEL_CTRL_SHIFT                 5
#define MT6333_PMIC_VMEM_DLC_CTRL_MASK                    0x1
#define MT6333_PMIC_VMEM_DLC_CTRL_SHIFT                   6
#define MT6333_PMIC_VMEM_BURST_CTRL_MASK                  0x1
#define MT6333_PMIC_VMEM_BURST_CTRL_SHIFT                 7
#define MT6333_PMIC_VMEM_VOSEL_MASK                       0x7F
#define MT6333_PMIC_VMEM_VOSEL_SHIFT                      0
#define MT6333_PMIC_VMEM_VOSEL_ON_MASK                    0x7F
#define MT6333_PMIC_VMEM_VOSEL_ON_SHIFT                   0
#define MT6333_PMIC_VMEM_VOSEL_SLEEP_MASK                 0x7F
#define MT6333_PMIC_VMEM_VOSEL_SLEEP_SHIFT                0
#define MT6333_PMIC_NI_VMEM_VOSEL_MASK                    0x7F
#define MT6333_PMIC_NI_VMEM_VOSEL_SHIFT                   0
#define MT6333_PMIC_QI_VMEM_BURST_MASK                    0x3
#define MT6333_PMIC_QI_VMEM_BURST_SHIFT                   6
#define MT6333_PMIC_VMEM_BURST_MASK                       0x3
#define MT6333_PMIC_VMEM_BURST_SHIFT                      0
#define MT6333_PMIC_VMEM_BURST_ON_MASK                    0x3
#define MT6333_PMIC_VMEM_BURST_ON_SHIFT                   2
#define MT6333_PMIC_VMEM_BURST_SLEEP_MASK                 0x3
#define MT6333_PMIC_VMEM_BURST_SLEEP_SHIFT                4
#define MT6333_PMIC_VMEM_DLC_MASK                         0x3
#define MT6333_PMIC_VMEM_DLC_SHIFT                        0
#define MT6333_PMIC_VMEM_DLC_ON_MASK                      0x3
#define MT6333_PMIC_VMEM_DLC_ON_SHIFT                     2
#define MT6333_PMIC_VMEM_DLC_SLEEP_MASK                   0x3
#define MT6333_PMIC_VMEM_DLC_SLEEP_SHIFT                  4
#define MT6333_PMIC_QI_VMEM_DLC_MASK                      0x3
#define MT6333_PMIC_QI_VMEM_DLC_SHIFT                     6
#define MT6333_PMIC_QI_VMEM_DLC_N_MASK                    0x3
#define MT6333_PMIC_QI_VMEM_DLC_N_SHIFT                   6
#define MT6333_PMIC_VMEM_DLC_N_MASK                       0x3
#define MT6333_PMIC_VMEM_DLC_N_SHIFT                      0
#define MT6333_PMIC_VMEM_DLC_N_ON_MASK                    0x3
#define MT6333_PMIC_VMEM_DLC_N_ON_SHIFT                   2
#define MT6333_PMIC_VMEM_DLC_N_SLEEP_MASK                 0x3
#define MT6333_PMIC_VMEM_DLC_N_SLEEP_SHIFT                4
#define MT6333_PMIC_VMEM_VSLEEP_EN_MASK                   0x1
#define MT6333_PMIC_VMEM_VSLEEP_EN_SHIFT                  3
#define MT6333_PMIC_VMEM_R2R_PDN_MASK                     0x1
#define MT6333_PMIC_VMEM_R2R_PDN_SHIFT                    4
#define MT6333_PMIC_VMEM_VSLEEP_SEL_MASK                  0x1
#define MT6333_PMIC_VMEM_VSLEEP_SEL_SHIFT                 5
#define MT6333_PMIC_NI_VMEM_R2R_PDN_MASK                  0x1
#define MT6333_PMIC_NI_VMEM_R2R_PDN_SHIFT                 6
#define MT6333_PMIC_NI_VMEM_VSLEEP_SEL_MASK               0x1
#define MT6333_PMIC_NI_VMEM_VSLEEP_SEL_SHIFT              7
#define MT6333_PMIC_VMEM_TRANSTD_MASK                     0x3
#define MT6333_PMIC_VMEM_TRANSTD_SHIFT                    0
#define MT6333_PMIC_VMEM_VOSEL_TRANS_EN_MASK              0x3
#define MT6333_PMIC_VMEM_VOSEL_TRANS_EN_SHIFT             2
#define MT6333_PMIC_VMEM_VOSEL_TRANS_ONCE_MASK            0x1
#define MT6333_PMIC_VMEM_VOSEL_TRANS_ONCE_SHIFT           4
#define MT6333_PMIC_NI_VMEM_VOSEL_TRANS_MASK              0x1
#define MT6333_PMIC_NI_VMEM_VOSEL_TRANS_SHIFT             5
#define MT6333_PMIC_RG_VRF18_VOCAL_MASK                   0x7
#define MT6333_PMIC_RG_VRF18_VOCAL_SHIFT                  4
#define MT6333_PMIC_RG_VRF18_GMSEL_MASK                   0x1
#define MT6333_PMIC_RG_VRF18_GMSEL_SHIFT                  7
#define MT6333_PMIC_RG_VRF18_BK_LDO_MASK                  0x1
#define MT6333_PMIC_RG_VRF18_BK_LDO_SHIFT                 0
#define MT6333_PMIC_RG_VRF18_SLEW_NMOS_MASK               0x3
#define MT6333_PMIC_RG_VRF18_SLEW_NMOS_SHIFT              3
#define MT6333_PMIC_RG_VRF18_SLEW_MASK                    0x3
#define MT6333_PMIC_RG_VRF18_SLEW_SHIFT                   6
#define MT6333_PMIC_RG_VRF18_CC_MASK                      0x3
#define MT6333_PMIC_RG_VRF18_CC_SHIFT                     3
#define MT6333_PMIC_RG_VRF18_RZSEL_MASK                   0x7
#define MT6333_PMIC_RG_VRF18_RZSEL_SHIFT                  5
#define MT6333_PMIC_RG_VRF18_SLP_MASK                     0x3
#define MT6333_PMIC_RG_VRF18_SLP_SHIFT                    2
#define MT6333_PMIC_RG_VRF18_CSL_MASK                     0x3
#define MT6333_PMIC_RG_VRF18_CSL_SHIFT                    4
#define MT6333_PMIC_RG_VRF18_CSR_MASK                     0x3
#define MT6333_PMIC_RG_VRF18_CSR_SHIFT                    6
#define MT6333_PMIC_RG_VRF18_NDIS_EN_MASK                 0x1
#define MT6333_PMIC_RG_VRF18_NDIS_EN_SHIFT                4
#define MT6333_PMIC_RG_VRF18_ZX_OS_MASK                   0x3
#define MT6333_PMIC_RG_VRF18_ZX_OS_SHIFT                  6
#define MT6333_PMIC_RG_VRF18_BURSTL_MASK                  0x7
#define MT6333_PMIC_RG_VRF18_BURSTL_SHIFT                 0
#define MT6333_PMIC_RG_VRF18_BURSTH_MASK                  0x7
#define MT6333_PMIC_RG_VRF18_BURSTH_SHIFT                 5
#define MT6333_PMIC_RG_VRF18_RSV_MASK                     0xFF
#define MT6333_PMIC_RG_VRF18_RSV_SHIFT                    0
#define MT6333_PMIC_QI_VRF18_OC_STATUS_MASK               0x1
#define MT6333_PMIC_QI_VRF18_OC_STATUS_SHIFT              7
#define MT6333_PMIC_RG_BUCK_RSV1_MASK                     0xFF
#define MT6333_PMIC_RG_BUCK_RSV1_SHIFT                    0
#define MT6333_PMIC_VRF18_VOSEL_CTRL_MASK                 0x1
#define MT6333_PMIC_VRF18_VOSEL_CTRL_SHIFT                5
#define MT6333_PMIC_VRF18_DLC_CTRL_MASK                   0x1
#define MT6333_PMIC_VRF18_DLC_CTRL_SHIFT                  6
#define MT6333_PMIC_VRF18_BURST_CTRL_MASK                 0x1
#define MT6333_PMIC_VRF18_BURST_CTRL_SHIFT                7
#define MT6333_PMIC_VRF18_VOSEL_MASK                      0x1F
#define MT6333_PMIC_VRF18_VOSEL_SHIFT                     3
#define MT6333_PMIC_VRF18_VOSEL_ON_MASK                   0x1F
#define MT6333_PMIC_VRF18_VOSEL_ON_SHIFT                  0
#define MT6333_PMIC_VRF18_VOSEL_SLEEP_MASK                0x1F
#define MT6333_PMIC_VRF18_VOSEL_SLEEP_SHIFT               3
#define MT6333_PMIC_NI_VRF18_VOSEL_MASK                   0x1F
#define MT6333_PMIC_NI_VRF18_VOSEL_SHIFT                  3
#define MT6333_PMIC_RG_BUCK_MON_FLAG_SEL_MASK             0xFF
#define MT6333_PMIC_RG_BUCK_MON_FLAG_SEL_SHIFT            0
#define MT6333_PMIC_RG_BUCK_RSV2_5_0_MASK                 0x3F
#define MT6333_PMIC_RG_BUCK_RSV2_5_0_SHIFT                0
#define MT6333_PMIC_RG_BUCK_BYPASS_VOSEL_LIMIT_MASK       0x1
#define MT6333_PMIC_RG_BUCK_BYPASS_VOSEL_LIMIT_SHIFT      6
#define MT6333_PMIC_RG_BUCK_MON_FLAG_EN_MASK              0x1
#define MT6333_PMIC_RG_BUCK_MON_FLAG_EN_SHIFT             7
#define MT6333_PMIC_VRF18_DLC_MASK                        0x3
#define MT6333_PMIC_VRF18_DLC_SHIFT                       0
#define MT6333_PMIC_VRF18_DLC_ON_MASK                     0x3
#define MT6333_PMIC_VRF18_DLC_ON_SHIFT                    2
#define MT6333_PMIC_VRF18_DLC_SLEEP_MASK                  0x3
#define MT6333_PMIC_VRF18_DLC_SLEEP_SHIFT                 4
#define MT6333_PMIC_QI_VRF18_DLC_MASK                     0x3
#define MT6333_PMIC_QI_VRF18_DLC_SHIFT                    6
#define MT6333_PMIC_QI_VRF18_DLC_N_MASK                   0x3
#define MT6333_PMIC_QI_VRF18_DLC_N_SHIFT                  6
#define MT6333_PMIC_VRF18_DLC_N_MASK                      0x3
#define MT6333_PMIC_VRF18_DLC_N_SHIFT                     0
#define MT6333_PMIC_VRF18_DLC_N_ON_MASK                   0x3
#define MT6333_PMIC_VRF18_DLC_N_ON_SHIFT                  2
#define MT6333_PMIC_VRF18_DLC_N_SLEEP_MASK                0x3
#define MT6333_PMIC_VRF18_DLC_N_SLEEP_SHIFT               4
#define MT6333_PMIC_RG_VRF18_MODESET_MASK                 0x1
#define MT6333_PMIC_RG_VRF18_MODESET_SHIFT                0
#define MT6333_PMIC_VRF18_EN_MASK                         0x1
#define MT6333_PMIC_VRF18_EN_SHIFT                        1
#define MT6333_PMIC_RG_VRF18_STB_SEL_MASK                 0x3
#define MT6333_PMIC_RG_VRF18_STB_SEL_SHIFT                2
#define MT6333_PMIC_QI_VRF18_STB_MASK                     0x1
#define MT6333_PMIC_QI_VRF18_STB_SHIFT                    5
#define MT6333_PMIC_QI_VRF18_EN_MASK                      0x1
#define MT6333_PMIC_QI_VRF18_EN_SHIFT                     6
#define MT6333_PMIC_VRF18_EN_CTRL_MASK                    0x1
#define MT6333_PMIC_VRF18_EN_CTRL_SHIFT                   7
#define MT6333_PMIC_RG_VRF18_MODESET_SPM_MASK             0x1
#define MT6333_PMIC_RG_VRF18_MODESET_SPM_SHIFT            0
#define MT6333_PMIC_VRF18_EN_SPM_MASK                     0x1
#define MT6333_PMIC_VRF18_EN_SPM_SHIFT                    1
#define MT6333_PMIC_RG_VCORE_VOSEL_SET_SPM_MASK           0x1
#define MT6333_PMIC_RG_VCORE_VOSEL_SET_SPM_SHIFT          2
#define MT6333_PMIC_K_RST_DONE_MASK                       0x1
#define MT6333_PMIC_K_RST_DONE_SHIFT                      0
#define MT6333_PMIC_K_MAP_SEL_MASK                        0x1
#define MT6333_PMIC_K_MAP_SEL_SHIFT                       1
#define MT6333_PMIC_K_ONCE_EN_MASK                        0x1
#define MT6333_PMIC_K_ONCE_EN_SHIFT                       2
#define MT6333_PMIC_K_ONCE_MASK                           0x1
#define MT6333_PMIC_K_ONCE_SHIFT                          3
#define MT6333_PMIC_K_START_MANUAL_MASK                   0x1
#define MT6333_PMIC_K_START_MANUAL_SHIFT                  4
#define MT6333_PMIC_K_SRC_SEL_MASK                        0x1
#define MT6333_PMIC_K_SRC_SEL_SHIFT                       5
#define MT6333_PMIC_K_AUTO_EN_MASK                        0x1
#define MT6333_PMIC_K_AUTO_EN_SHIFT                       6
#define MT6333_PMIC_K_INV_MASK                            0x1
#define MT6333_PMIC_K_INV_SHIFT                           7
#define MT6333_PMIC_K_CONTROL_SMPS_MASK                   0x1F
#define MT6333_PMIC_K_CONTROL_SMPS_SHIFT                  3
#define MT6333_PMIC_QI_SMPS_OSC_CAL_MASK                  0x1F
#define MT6333_PMIC_QI_SMPS_OSC_CAL_SHIFT                 3
#define MT6333_PMIC_K_RESULT_MASK                         0x1
#define MT6333_PMIC_K_RESULT_SHIFT                        0
#define MT6333_PMIC_K_DONE_MASK                           0x1
#define MT6333_PMIC_K_DONE_SHIFT                          1
#define MT6333_PMIC_K_CONTROL_MASK                        0x1F
#define MT6333_PMIC_K_CONTROL_SHIFT                       3
#define MT6333_PMIC_K_BUCK_CK_CNT_MASK                    0xFF
#define MT6333_PMIC_K_BUCK_CK_CNT_SHIFT                   0
#define MT6333_PMIC_K_CHR_CK_CNT_MASK                     0xFF
#define MT6333_PMIC_K_CHR_CK_CNT_SHIFT                    0
#define MT6333_PMIC_RG_STRUP_RSV1_2_0_MASK                0x7
#define MT6333_PMIC_RG_STRUP_RSV1_2_0_SHIFT               0
#define MT6333_PMIC_RG_STRUP_SLEEP_MODE_DEB_SEL_MASK      0x1
#define MT6333_PMIC_RG_STRUP_SLEEP_MODE_DEB_SEL_SHIFT     3
#define MT6333_PMIC_RG_STRUP_POSEQ_DONE_DEB_SEL_MASK      0x1
#define MT6333_PMIC_RG_STRUP_POSEQ_DONE_DEB_SEL_SHIFT     4
#define MT6333_PMIC_RG_STRUP_VMEM_EN_DEB_SEL_MASK         0x1
#define MT6333_PMIC_RG_STRUP_VMEM_EN_DEB_SEL_SHIFT        5
#define MT6333_PMIC_RG_STRUP_VCORE_EN_DEB_SEL_MASK        0x1
#define MT6333_PMIC_RG_STRUP_VCORE_EN_DEB_SEL_SHIFT       6
#define MT6333_PMIC_RG_STRUP_BUCK_EN_DEB_SEL_MASK         0x1
#define MT6333_PMIC_RG_STRUP_BUCK_EN_DEB_SEL_SHIFT        7
#define MT6333_PMIC_RG_STRUP_OSC_EN_MASK                  0x1
#define MT6333_PMIC_RG_STRUP_OSC_EN_SHIFT                 0
#define MT6333_PMIC_RG_STRUP_OSC_EN_SEL_MASK              0x1
#define MT6333_PMIC_RG_STRUP_OSC_EN_SEL_SHIFT             1
#define MT6333_PMIC_RG_STRUP_IVGEN_ENB_MASK               0x1
#define MT6333_PMIC_RG_STRUP_IVGEN_ENB_SHIFT              2
#define MT6333_PMIC_RG_STRUP_IVGEN_ENB_SEL_MASK           0x1
#define MT6333_PMIC_RG_STRUP_IVGEN_ENB_SEL_SHIFT          3
#define MT6333_PMIC_RG_STRUP_RSV2_7_4_MASK                0xF
#define MT6333_PMIC_RG_STRUP_RSV2_7_4_SHIFT               4
#define MT6333_PMIC_RG_STRUP_RSV3_7_0_MASK                0xFF
#define MT6333_PMIC_RG_STRUP_RSV3_7_0_SHIFT               0
#define MT6333_PMIC_RG_EFUSE_ADDR_MASK                    0x1F
#define MT6333_PMIC_RG_EFUSE_ADDR_SHIFT                   0
#define MT6333_PMIC_RG_EFUSE_PROG_MASK                    0x1
#define MT6333_PMIC_RG_EFUSE_PROG_SHIFT                   0
#define MT6333_PMIC_RG_EFUSE_EN_MASK                      0x1
#define MT6333_PMIC_RG_EFUSE_EN_SHIFT                     0
#define MT6333_PMIC_RG_EFUSE_PKEY_MASK                    0xFF
#define MT6333_PMIC_RG_EFUSE_PKEY_SHIFT                   0
#define MT6333_PMIC_RG_EFUSE_RD_TRIG_MASK                 0x1
#define MT6333_PMIC_RG_EFUSE_RD_TRIG_SHIFT                0
#define MT6333_PMIC_RG_EFUSE_PROG_SRC_MASK                0x1
#define MT6333_PMIC_RG_EFUSE_PROG_SRC_SHIFT               0
#define MT6333_PMIC_RG_PROG_MACRO_SEL_MASK                0x1
#define MT6333_PMIC_RG_PROG_MACRO_SEL_SHIFT               0
#define MT6333_PMIC_RG_RD_RDY_BYPASS_MASK                 0x1
#define MT6333_PMIC_RG_RD_RDY_BYPASS_SHIFT                0
#define MT6333_PMIC_RG_EFUSE_RD_ACK_MASK                  0x1
#define MT6333_PMIC_RG_EFUSE_RD_ACK_SHIFT                 0
#define MT6333_PMIC_RG_EFUSE_BUSY_MASK                    0x1
#define MT6333_PMIC_RG_EFUSE_BUSY_SHIFT                   2
#define MT6333_PMIC_RG_OTP_PA_MASK                        0x3
#define MT6333_PMIC_RG_OTP_PA_SHIFT                       0
#define MT6333_PMIC_RG_OTP_PDIN_MASK                      0xFF
#define MT6333_PMIC_RG_OTP_PDIN_SHIFT                     0
#define MT6333_PMIC_RG_OTP_PTM_MASK                       0x3
#define MT6333_PMIC_RG_OTP_PTM_SHIFT                      0
#define MT6333_PMIC_RG_FSOURCE_EN_MASK                    0x1
#define MT6333_PMIC_RG_FSOURCE_EN_SHIFT                   0
#define MT6333_PMIC_RG_EFUSE_DOUT_0_7_MASK                0xFF
#define MT6333_PMIC_RG_EFUSE_DOUT_0_7_SHIFT               0
#define MT6333_PMIC_RG_EFUSE_DOUT_8_15_MASK               0xFF
#define MT6333_PMIC_RG_EFUSE_DOUT_8_15_SHIFT              0
#define MT6333_PMIC_RG_EFUSE_DOUT_16_23_MASK              0xFF
#define MT6333_PMIC_RG_EFUSE_DOUT_16_23_SHIFT             0
#define MT6333_PMIC_RG_EFUSE_DOUT_24_31_MASK              0xFF
#define MT6333_PMIC_RG_EFUSE_DOUT_24_31_SHIFT             0
#define MT6333_PMIC_RG_EFUSE_DOUT_32_39_MASK              0xFF
#define MT6333_PMIC_RG_EFUSE_DOUT_32_39_SHIFT             0
#define MT6333_PMIC_RG_EFUSE_DOUT_40_47_MASK              0xFF
#define MT6333_PMIC_RG_EFUSE_DOUT_40_47_SHIFT             0
#define MT6333_PMIC_RG_EFUSE_DOUT_48_55_MASK              0xFF
#define MT6333_PMIC_RG_EFUSE_DOUT_48_55_SHIFT             0
#define MT6333_PMIC_RG_EFUSE_DOUT_56_63_MASK              0xFF
#define MT6333_PMIC_RG_EFUSE_DOUT_56_63_SHIFT             0
#define MT6333_PMIC_TESTI0_MASK                           0xFF
#define MT6333_PMIC_TESTI0_SHIFT                          0
#define MT6333_PMIC_TESTI1_MASK                           0xFF
#define MT6333_PMIC_TESTI1_SHIFT                          0
#define MT6333_PMIC_TESTI2_MASK                           0xFF
#define MT6333_PMIC_TESTI2_SHIFT                          0
#define MT6333_PMIC_TESTI3_MASK                           0xFF
#define MT6333_PMIC_TESTI3_SHIFT                          0
#define MT6333_PMIC_TESTI4_MASK                           0xFF
#define MT6333_PMIC_TESTI4_SHIFT                          0
#define MT6333_PMIC_TESTI5_MASK                           0xFF
#define MT6333_PMIC_TESTI5_SHIFT                          0
#define MT6333_PMIC_TESTI6_MASK                           0xFF
#define MT6333_PMIC_TESTI6_SHIFT                          0
#define MT6333_PMIC_TESTI7_MASK                           0xFF
#define MT6333_PMIC_TESTI7_SHIFT                          0
#define MT6333_PMIC_TESTI8_MASK                           0xFF
#define MT6333_PMIC_TESTI8_SHIFT                          0
#define MT6333_PMIC_TESTI0_SEL_MASK                       0xFF
#define MT6333_PMIC_TESTI0_SEL_SHIFT                      0
#define MT6333_PMIC_TESTI1_SEL_MASK                       0xFF
#define MT6333_PMIC_TESTI1_SEL_SHIFT                      0
#define MT6333_PMIC_TESTI2_SEL_MASK                       0xFF
#define MT6333_PMIC_TESTI2_SEL_SHIFT                      0
#define MT6333_PMIC_TESTI3_SEL_MASK                       0xFF
#define MT6333_PMIC_TESTI3_SEL_SHIFT                      0
#define MT6333_PMIC_TESTI4_SEL_MASK                       0xFF
#define MT6333_PMIC_TESTI4_SEL_SHIFT                      0
#define MT6333_PMIC_TESTI5_SEL_MASK                       0xFF
#define MT6333_PMIC_TESTI5_SEL_SHIFT                      0
#define MT6333_PMIC_TESTI6_SEL_MASK                       0xFF
#define MT6333_PMIC_TESTI6_SEL_SHIFT                      0
#define MT6333_PMIC_TESTI7_SEL_MASK                       0xFF
#define MT6333_PMIC_TESTI7_SEL_SHIFT                      0
#define MT6333_PMIC_TESTI8_SEL_MASK                       0xFF
#define MT6333_PMIC_TESTI8_SEL_SHIFT                      0
#define MT6333_PMIC_TESTO0_MASK                           0xFF
#define MT6333_PMIC_TESTO0_SHIFT                          0
#define MT6333_PMIC_TESTO1_MASK                           0xFF
#define MT6333_PMIC_TESTO1_SHIFT                          0
#define MT6333_PMIC_TESTO2_MASK                           0xFF
#define MT6333_PMIC_TESTO2_SHIFT                          0
#define MT6333_PMIC_TESTO3_MASK                           0xFF
#define MT6333_PMIC_TESTO3_SHIFT                          0
#define MT6333_PMIC_TESTO0_SEL_MASK                       0xFF
#define MT6333_PMIC_TESTO0_SEL_SHIFT                      0
#define MT6333_PMIC_TESTO1_SEL_MASK                       0xFF
#define MT6333_PMIC_TESTO1_SEL_SHIFT                      0
#define MT6333_PMIC_TESTO2_SEL_MASK                       0xFF
#define MT6333_PMIC_TESTO2_SEL_SHIFT                      0
#define MT6333_PMIC_TESTO3_SEL_MASK                       0xFF
#define MT6333_PMIC_TESTO3_SEL_SHIFT                      0
#define MT6333_PMIC_DEBUG_SEL_MASK                        0xFF
#define MT6333_PMIC_DEBUG_SEL_SHIFT                       0
#define MT6333_PMIC_DEBUG_MON_MASK                        0xFF
#define MT6333_PMIC_DEBUG_MON_SHIFT                       0
#define MT6333_PMIC_DEBUG_BIT_SEL_MASK                    0x7
#define MT6333_PMIC_DEBUG_BIT_SEL_SHIFT                   0
#define MT6333_PMIC_CID1_MASK                             0xFF
#define MT6333_PMIC_CID1_SHIFT                            0

/**********************************************************
  *
  *   [Extern Function] 
  *
  *********************************************************/
extern kal_uint8 mt6333_get_cid0(void);
extern void mt6333_set_rg_bgr_rsel(kal_uint8 val);
extern void mt6333_set_rg_bgr_unchop(kal_uint8 val);
extern void mt6333_set_rg_bgr_unchop_ph(kal_uint8 val);
extern void mt6333_set_rg_bgr_trim(kal_uint8 val);
extern void mt6333_set_rg_bgr_trim_en(kal_uint8 val);
extern void mt6333_set_rg_bgr_test_en(kal_uint8 val);
extern void mt6333_set_rg_bgr_test_rstb(kal_uint8 val);
extern void mt6333_set_rg_bgr_test_ckin(kal_uint8 val);
extern void mt6333_set_rg_chr_en(kal_uint8 val);
extern void mt6333_set_rg_vbout_en(kal_uint8 val);
extern void mt6333_set_rg_adcin_vbat_en(kal_uint8 val);
extern void mt6333_set_rg_adcin_chrin_en(kal_uint8 val);
extern void mt6333_set_rg_adcin_baton_en(kal_uint8 val);
extern void mt6333_set_rg_bat_on_open_vth(kal_uint8 val);
extern void mt6333_set_rg_bat_on_pull_high_en(kal_uint8 val);
extern void mt6333_set_rg_int_ph_enb(kal_uint8 val);
extern void mt6333_set_rg_scl_ph_enb(kal_uint8 val);
extern void mt6333_set_rg_sda_ph_enb(kal_uint8 val);
extern void mt6333_set_rg_vdrv_rdivsel(kal_uint8 val);
extern void mt6333_set_rg_swchr_rv1(kal_uint8 val);
extern void mt6333_set_rg_chr_otg_lv_th(kal_uint8 val);
extern void mt6333_set_rg_chr_otg_hv_th(kal_uint8 val);
extern void mt6333_set_rg_strup_ther_rg_th(kal_uint8 val);
extern void mt6333_set_rg_strup_rsv(kal_uint8 val);
extern void mt6333_set_rg_swchr_ana_test_mode(kal_uint8 val);
extern void mt6333_set_rg_swchr_ana_test_mode_sel(kal_uint8 val);
extern void mt6333_set_rg_csa_otg_sel(kal_uint8 val);
extern void mt6333_set_rg_otg_cs_slp_en(kal_uint8 val);
extern void mt6333_set_rg_slp_otg_sel(kal_uint8 val);
extern void mt6333_set_rg_iterm_sel(kal_uint8 val);
extern void mt6333_set_rg_ics_loop(kal_uint8 val);
extern void mt6333_set_rg_zxgm_tune(kal_uint8 val);
extern void mt6333_set_rg_chop_en(kal_uint8 val);
extern void mt6333_set_rg_force_non_oc(kal_uint8 val);
extern void mt6333_set_rg_force_non_ov(kal_uint8 val);
extern void mt6333_set_rg_gdri_minoff_en(kal_uint8 val);
extern void mt6333_set_rg_sys_vreftrim(kal_uint8 val);
extern void mt6333_set_rg_cs_vreftrim(kal_uint8 val);
extern void mt6333_set_rg_osc_trim(kal_uint8 val);
extern void mt6333_set_rg_vsys_ov_trim(kal_uint8 val);
extern void mt6333_set_rg_swchr_rv2(kal_uint8 val);
extern void mt6333_set_rg_input_cc_reg(kal_uint8 val);
extern void mt6333_set_rg_otg_chrin_vol(kal_uint8 val);
extern void mt6333_set_rg_force_otg_non_ov(kal_uint8 val);
extern void mt6333_set_rg_inout_csreg_sel(kal_uint8 val);
extern void mt6333_set_rg_chopfreq_sel(kal_uint8 val);
extern void mt6333_set_rg_chgpreg_sel(kal_uint8 val);
extern void mt6333_set_rg_flash_drv_en(kal_uint8 val);
extern void mt6333_set_rg_fpwm_otg(kal_uint8 val);
extern void mt6333_set_rg_otg_zx_testmode(kal_uint8 val);
extern void mt6333_set_rg_swchr_zx_testmode(kal_uint8 val);
extern void mt6333_set_rg_zx_trim(kal_uint8 val);
extern void mt6333_set_rg_zx_trim_otg(kal_uint8 val);
extern kal_uint8 mt6333_get_rgs_auto_recharge(void);
extern kal_uint8 mt6333_get_rgs_charge_complete_hw(void);
extern kal_uint8 mt6333_get_rgs_pwm_oc_det(void);
extern kal_uint8 mt6333_get_rgs_vsys_ov_det(void);
extern kal_uint8 mt6333_get_rgs_power_path(void);
extern kal_uint8 mt6333_get_rgs_force_no_pp_config(void);
extern kal_uint8 mt6333_get_rgs_chrg_status(void);
extern kal_uint8 mt6333_get_rgs_bat_st_recc(void);
extern kal_uint8 mt6333_get_rgs_sys_gt_cv(void);
extern kal_uint8 mt6333_get_rgs_bat_gt_cc(void);
extern kal_uint8 mt6333_get_rgs_bat_gt_30(void);
extern kal_uint8 mt6333_get_rgs_bat_gt_22(void);
extern kal_uint8 mt6333_get_rgs_buck_mode(void);
extern kal_uint8 mt6333_get_rgs_buck_precc_mode(void);
extern kal_uint8 mt6333_get_rgs_chrdet(void);
extern kal_uint8 mt6333_get_rgs_chr_hv_det(void);
extern kal_uint8 mt6333_get_rgs_chr_plug_in(void);
extern kal_uint8 mt6333_get_rgs_baton_undet(void);
extern kal_uint8 mt6333_get_rgs_chrin_lv_det(void);
extern kal_uint8 mt6333_get_rgs_chrin_hv_det(void);
extern kal_uint8 mt6333_get_rgs_thermal_sd_mode(void);
extern kal_uint8 mt6333_get_rgs_chr_hv_mode(void);
extern kal_uint8 mt6333_get_rgs_bat_only_mode(void);
extern kal_uint8 mt6333_get_rgs_chr_suspend_mode(void);
extern kal_uint8 mt6333_get_rgs_precc_mode(void);
extern kal_uint8 mt6333_get_rgs_cv_mode(void);
extern kal_uint8 mt6333_get_rgs_cc_mode(void);
extern kal_uint8 mt6333_get_rgs_ot_reg(void);
extern kal_uint8 mt6333_get_rgs_ot_sd(void);
extern kal_uint8 mt6333_get_rgs_pwm_bat_config(void);
extern kal_uint8 mt6333_get_rgs_pwm_current_config(void);
extern kal_uint8 mt6333_get_rgs_pwm_voltage_config(void);
extern kal_uint8 mt6333_get_rgs_buck_overload(void);
extern kal_uint8 mt6333_get_rgs_bat_dppm_mode(void);
extern kal_uint8 mt6333_get_rgs_adaptive_cv_mode(void);
extern kal_uint8 mt6333_get_rgs_vin_dpm_mode(void);
extern kal_uint8 mt6333_get_rgs_thermal_reg_mode(void);
extern kal_uint8 mt6333_get_rgs_ich_setting(void);
extern kal_uint8 mt6333_get_rgs_cs_sel(void);
extern kal_uint8 mt6333_get_rgs_syscv_fine_sel(void);
extern kal_uint8 mt6333_get_rgs_oc_sd_sel(void);
extern kal_uint8 mt6333_get_rgs_pwm_oc_sel(void);
extern kal_uint8 mt6333_get_rgs_chrwdt_tout(void);
extern kal_uint8 mt6333_get_rgs_vsys_ov_vth(void);
extern kal_uint8 mt6333_get_rgs_syscv_coarse_sel(void);
extern kal_uint8 mt6333_get_rgs_usb_dl_key(void);
extern kal_uint8 mt6333_get_rgs_force_pp_on(void);
extern kal_uint8 mt6333_get_rgs_ini_sys_on(void);
extern kal_uint8 mt6333_get_rgs_ich_oc_flag_chr_core(void);
extern kal_uint8 mt6333_get_rgs_pwm_oc_chr_core(void);
extern kal_uint8 mt6333_get_rgs_power_on_ready(void);
extern kal_uint8 mt6333_get_rgs_auto_pwron(void);
extern kal_uint8 mt6333_get_rgs_auto_pwron_done(void);
extern kal_uint8 mt6333_get_rgs_chr_mode(void);
extern kal_uint8 mt6333_get_rgs_otg_mode(void);
extern kal_uint8 mt6333_get_rgs_poseq_done(void);
extern kal_uint8 mt6333_get_rgs_otg_precc(void);
extern kal_uint8 mt6333_get_rgs_chrin_short(void);
extern kal_uint8 mt6333_get_rgs_drvcdt_short(void);
extern kal_uint8 mt6333_get_rgs_otg_m3_oc(void);
extern kal_uint8 mt6333_get_rgs_otg_thermal(void);
extern kal_uint8 mt6333_get_rgs_chr_in_flash(void);
extern kal_uint8 mt6333_get_rgs_vled_short(void);
extern kal_uint8 mt6333_get_rgs_vled_open(void);
extern kal_uint8 mt6333_get_rgs_flash_en_timeout(void);
extern kal_uint8 mt6333_get_rgs_chr_oc(void);
extern kal_uint8 mt6333_get_rgs_pwm_en(void);
extern kal_uint8 mt6333_get_rgs_otg_en(void);
extern kal_uint8 mt6333_get_rgs_otg_en_stb(void);
extern kal_uint8 mt6333_get_rgs_otg_drv_en(void);
extern kal_uint8 mt6333_get_rgs_flash_en(void);
extern kal_uint8 mt6333_get_rgs_m3_boost_en(void);
extern kal_uint8 mt6333_get_rgs_m3_r_en(void);
extern kal_uint8 mt6333_get_rgs_m3_s_en(void);
extern kal_uint8 mt6333_get_rgs_m3_en(void);
extern kal_uint8 mt6333_get_rgs_cpcstsys_en(void);
extern kal_uint8 mt6333_get_rgs_sw_gate_ctrl(void);
extern kal_uint8 mt6333_get_qi_otg_chr_gt_lv(void);
extern kal_uint8 mt6333_get_rgs_thermal_rg_th(void);
extern kal_uint8 mt6333_get_rgs_otg_oc_th(void);
extern void mt6333_set_rg_chr_suspend(kal_uint8 val);
extern void mt6333_set_rg_sys_on(kal_uint8 val);
extern void mt6333_set_rg_sys_unstable(kal_uint8 val);
extern void mt6333_set_rg_skip_efuse_out(kal_uint8 val);
extern void mt6333_set_rg_vsys_sel(kal_uint8 val);
extern void mt6333_set_rg_cv_sel(kal_uint8 val);
extern void mt6333_set_rg_ich_sel(kal_uint8 val);
extern void mt6333_set_rg_ich_pre_sel(kal_uint8 val);
extern void mt6333_set_rg_oc_sel(kal_uint8 val);
extern void mt6333_set_rg_chrin_lv_vth(kal_uint8 val);
extern void mt6333_set_rg_chrin_hv_vth(kal_uint8 val);
extern void mt6333_set_rg_usbdl_ext(kal_uint8 val);
extern void mt6333_set_rg_usbdl_mode_b(kal_uint8 val);
extern void mt6333_set_rg_usbdl_oc_sel(kal_uint8 val);
extern void mt6333_set_rg_buck_overload_prot_en(kal_uint8 val);
extern void mt6333_set_rg_ch_complete_auto_off(kal_uint8 val);
extern void mt6333_set_rg_term_timer(kal_uint8 val);
extern void mt6333_set_rg_chr_oc_auto_off(kal_uint8 val);
extern void mt6333_set_rg_chr_oc_reset(kal_uint8 val);
extern void mt6333_set_rg_otg_m3_oc_auto_off(kal_uint8 val);
extern void mt6333_set_rg_otg_en(kal_uint8 val);
extern void mt6333_set_rg_flash_en(kal_uint8 val);
extern void mt6333_set_rg_flash_pwm_en(kal_uint8 val);
extern void mt6333_set_rg_flash_pwm_en_stb(kal_uint8 val);
extern void mt6333_set_rg_torch_mode(kal_uint8 val);
extern void mt6333_set_rg_torch_chrin_chk(kal_uint8 val);
extern void mt6333_set_rg_flash_dim_duty(kal_uint8 val);
extern void mt6333_set_rg_chk_chrin_time_ext(kal_uint8 val);
extern void mt6333_set_rg_flash_dim_fsel(kal_uint8 val);
extern void mt6333_set_rg_flash_iset(kal_uint8 val);
extern void mt6333_set_rg_flash_iset_step(kal_uint8 val);
extern void mt6333_set_rg_thermal_rg_th(kal_uint8 val);
extern void mt6333_set_rg_thermal_temp_sel(kal_uint8 val);
extern void mt6333_set_rg_thermal_checker_sel(kal_uint8 val);
extern void mt6333_set_rg_flash_en_timeout_sel(kal_uint8 val);
extern void mt6333_set_rg_otg_oc_th(kal_uint8 val);
extern void mt6333_set_rg_reserve_v0(kal_uint8 val);
extern void mt6333_set_rg_cv_sel_usbdl(kal_uint8 val);
extern void mt6333_set_rg_ov_sel_usbdl(kal_uint8 val);
extern void mt6333_set_rg_sw_gate_ctrl(kal_uint8 val);
extern void mt6333_set_rg_reserve_v1(kal_uint8 val);
extern void mt6333_set_rg_reserve_v2(kal_uint8 val);
extern void mt6333_set_i2c_config(kal_uint8 val);
extern void mt6333_set_i2c_deg_en(kal_uint8 val);
extern void mt6333_set_sda_mode(kal_uint8 val);
extern void mt6333_set_sda_oe(kal_uint8 val);
extern void mt6333_set_sda_out(kal_uint8 val);
extern void mt6333_set_scl_mode(kal_uint8 val);
extern void mt6333_set_scl_oe(kal_uint8 val);
extern void mt6333_set_scl_out(kal_uint8 val);
extern void mt6333_set_int_mode(kal_uint8 val);
extern void mt6333_set_int_oe(kal_uint8 val);
extern void mt6333_set_int_out(kal_uint8 val);
extern void mt6333_set_rg_chr_250k_ck_en(kal_uint8 val);
extern void mt6333_set_rg_chr_1m_ck_en(kal_uint8 val);
extern void mt6333_set_rg_chr_pwm_ck_en(kal_uint8 val);
extern void mt6333_set_rg_buck_1m_ck_en(kal_uint8 val);
extern void mt6333_set_rg_buck_2m_ck_en(kal_uint8 val);
extern void mt6333_set_rg_buck_3m_ck_en(kal_uint8 val);
extern void mt6333_set_rg_buck_6m_ck_en(kal_uint8 val);
extern void mt6333_set_rg_buck_osc_en(kal_uint8 val);
extern kal_uint8 mt6333_get_qi_osc_en(void);
extern void mt6333_set_rg_buck_cali_32k_ck_en(kal_uint8 val);
extern void mt6333_set_rg_buck_cali_pwm_ck_en(kal_uint8 val);
extern void mt6333_set_rg_buck_cali_6m_ck_en(kal_uint8 val);
extern void mt6333_set_rg_test_efuse(kal_uint8 val);
extern void mt6333_set_rg_test_ni_ck(kal_uint8 val);
extern void mt6333_set_rg_test_smps_ck(kal_uint8 val);
extern void mt6333_set_rg_test_pwm_ck(kal_uint8 val);
extern void mt6333_set_rg_int_en_chr_complete(kal_uint8 val);
extern void mt6333_set_rg_int_en_thermal_sd(kal_uint8 val);
extern void mt6333_set_rg_int_en_thermal_reg_in(kal_uint8 val);
extern void mt6333_set_rg_int_en_thermal_reg_out(kal_uint8 val);
extern void mt6333_set_rg_int_en_otg_oc(kal_uint8 val);
extern void mt6333_set_rg_int_en_otg_thermal(kal_uint8 val);
extern void mt6333_set_rg_int_en_otg_chrin_short(kal_uint8 val);
extern void mt6333_set_rg_int_en_otg_drvcdt_short(kal_uint8 val);
extern void mt6333_set_rg_int_en_chrwdt_flag(kal_uint8 val);
extern void mt6333_set_rg_int_en_buck_vcore_oc(kal_uint8 val);
extern void mt6333_set_rg_int_en_buck_vmem_oc(kal_uint8 val);
extern void mt6333_set_rg_int_en_buck_vrf18_oc(kal_uint8 val);
extern void mt6333_set_rg_int_en_buck_thermal(kal_uint8 val);
extern void mt6333_set_rg_int_en_flash_en_timeout(kal_uint8 val);
extern void mt6333_set_rg_int_en_flash_vled_short(kal_uint8 val);
extern void mt6333_set_rg_int_en_flash_vled_open(kal_uint8 val);
extern void mt6333_set_rg_int_en_chr_oc(kal_uint8 val);
extern void mt6333_set_rg_int_en_chr_plug_in_flash(kal_uint8 val);
extern void mt6333_set_rg_chrwdt_en(kal_uint8 val);
extern void mt6333_set_rg_chrwdt_td(kal_uint8 val);
extern void mt6333_set_rg_chrwdt_wr(kal_uint8 val);
extern kal_uint8 mt6333_get_rg_chrwdt_flag(void);
extern kal_uint8 mt6333_get_rg_int_status_chr_complete(void);
extern kal_uint8 mt6333_get_rg_int_status_thermal_sd(void);
extern kal_uint8 mt6333_get_rg_int_status_thermal_reg_in(void);
extern kal_uint8 mt6333_get_rg_int_status_thermal_reg_out(void);
extern kal_uint8 mt6333_get_rg_int_status_otg_oc(void);
extern kal_uint8 mt6333_get_rg_int_status_otg_thermal(void);
extern kal_uint8 mt6333_get_rg_int_status_otg_chrin_short(void);
extern kal_uint8 mt6333_get_rg_int_status_otg_drvcdt_short(void);
extern kal_uint8 mt6333_get_rg_int_status_chrwdt_flag(void);
extern kal_uint8 mt6333_get_rg_int_status_buck_vcore_oc(void);
extern kal_uint8 mt6333_get_rg_int_status_buck_vmem_oc(void);
extern kal_uint8 mt6333_get_rg_int_status_buck_vrf18_oc(void);
extern kal_uint8 mt6333_get_rg_int_status_buck_thermal(void);
extern kal_uint8 mt6333_get_rg_int_status_flash_en_timeout(void);
extern kal_uint8 mt6333_get_rg_int_status_flash_vled_short(void);
extern kal_uint8 mt6333_get_rg_int_status_flash_vled_open(void);
extern kal_uint8 mt6333_get_rg_int_status_chr_oc(void);
extern kal_uint8 mt6333_get_rg_int_status_chr_plug_in_flash(void);
extern void mt6333_set_vcore_deg_en(kal_uint8 val);
extern void mt6333_set_vcore_oc_wnd(kal_uint8 val);
extern void mt6333_set_vcore_oc_thd(kal_uint8 val);
extern void mt6333_set_vmem_deg_en(kal_uint8 val);
extern void mt6333_set_vmem_oc_wnd(kal_uint8 val);
extern void mt6333_set_vmem_oc_thd(kal_uint8 val);
extern void mt6333_set_vrf18_deg_en(kal_uint8 val);
extern void mt6333_set_vrf18_oc_wnd(kal_uint8 val);
extern void mt6333_set_vrf18_oc_thd(kal_uint8 val);
extern void mt6333_set_int_polarity(kal_uint8 val);
extern void mt6333_set_rg_smps_testmode_b(kal_uint8 val);
extern void mt6333_set_rg_vcore_triml(kal_uint8 val);
extern void mt6333_set_rg_vcore_trimh(kal_uint8 val);
extern void mt6333_set_rg_vcore_cc(kal_uint8 val);
extern void mt6333_set_rg_vcore_rzsel(kal_uint8 val);
extern void mt6333_set_rg_vcore_slp(kal_uint8 val);
extern void mt6333_set_rg_vcore_csl(kal_uint8 val);
extern void mt6333_set_rg_vcore_csr(kal_uint8 val);
extern void mt6333_set_rg_vcore_avp_os(kal_uint8 val);
extern void mt6333_set_rg_vcore_avp_en(kal_uint8 val);
extern void mt6333_set_rg_vcore_ndis_en(kal_uint8 val);
extern void mt6333_set_rg_vcore_modeset(kal_uint8 val);
extern void mt6333_set_rg_vcore_zx_os(kal_uint8 val);
extern void mt6333_set_rg_vcore_csm(kal_uint8 val);
extern void mt6333_set_rg_vcore_zxos_trim(kal_uint8 val);
extern void mt6333_set_rg_vcore_rsv(kal_uint8 val);
extern kal_uint8 mt6333_get_qi_vcore_dig_mon(void);
extern kal_uint8 mt6333_get_qi_vcore_oc_status(void);
extern void mt6333_set_vsleep_src1(kal_uint8 val);
extern void mt6333_set_vsleep_src0_7_0(kal_uint8 val);
extern void mt6333_set_vsleep_src0_8(kal_uint8 val);
extern void mt6333_set_r2r_src0_8(kal_uint8 val);
extern void mt6333_set_r2r_src1(kal_uint8 val);
extern void mt6333_set_r2r_src0_7_0(kal_uint8 val);
extern void mt6333_set_srclken_dly_src1(kal_uint8 val);
extern void mt6333_set_rg_buck_rsv0(kal_uint8 val);
extern void mt6333_set_rg_srclken_dly_sel(kal_uint8 val);
extern void mt6333_set_rg_r2r_event_sel(kal_uint8 val);
extern void mt6333_set_qi_vcore_vsleep(kal_uint8 val);
extern void mt6333_set_vcore_en(kal_uint8 val);
extern kal_uint8 mt6333_get_qi_vcore_stb(void);
extern kal_uint8 mt6333_get_qi_vcore_en(void);
extern void mt6333_set_vcore_en_ctrl(kal_uint8 val);
extern void mt6333_set_vcore_vosel_ctrl(kal_uint8 val);
extern void mt6333_set_vcore_dlc_ctrl(kal_uint8 val);
extern void mt6333_set_vcore_burst_ctrl(kal_uint8 val);
extern void mt6333_set_vcore_sfchg_ren(kal_uint8 val);
extern void mt6333_set_vcore_sfchg_rrate(kal_uint8 val);
extern void mt6333_set_vcore_sfchg_fen(kal_uint8 val);
extern void mt6333_set_vcore_sfchg_frate(kal_uint8 val);
extern void mt6333_set_vcore_vosel(kal_uint8 val);
extern void mt6333_set_vcore_vosel_on(kal_uint8 val);
extern void mt6333_set_vcore_vosel_sleep(kal_uint8 val);
extern kal_uint8 mt6333_get_ni_vcore_vosel(void);
extern void mt6333_set_vcore_burst(kal_uint8 val);
extern void mt6333_set_vcore_burst_on(kal_uint8 val);
extern void mt6333_set_vcore_burst_sleep(kal_uint8 val);
extern kal_uint8 mt6333_get_qi_vcore_burst(void);
extern void mt6333_set_vcore_dlc(kal_uint8 val);
extern void mt6333_set_vcore_dlc_on(kal_uint8 val);
extern void mt6333_set_vcore_dlc_sleep(kal_uint8 val);
extern kal_uint8 mt6333_get_qi_vcore_dlc(void);
extern void mt6333_set_vcore_dlc_n(kal_uint8 val);
extern void mt6333_set_vcore_dlc_n_on(kal_uint8 val);
extern void mt6333_set_vcore_dlc_n_sleep(kal_uint8 val);
extern kal_uint8 mt6333_get_qi_vcore_dlc_n(void);
extern void mt6333_set_vcore_vsleep_en(kal_uint8 val);
extern void mt6333_set_vcore_r2r_pdn(kal_uint8 val);
extern void mt6333_set_vcore_vsleep_sel(kal_uint8 val);
extern kal_uint8 mt6333_get_ni_vcore_r2r_pdn(void);
extern kal_uint8 mt6333_get_ni_vcore_vsleep_sel(void);
extern void mt6333_set_vcore_transtd(kal_uint8 val);
extern void mt6333_set_vcore_vosel_trans_en(kal_uint8 val);
extern void mt6333_set_vcore_vosel_trans_once(kal_uint8 val);
extern kal_uint8 mt6333_get_ni_vcore_vosel_trans(void);
extern void mt6333_set_vcore_sfchg_fen_sleep(kal_uint8 val);
extern void mt6333_set_vcore_sfchg_ren_sleep(kal_uint8 val);
extern void mt6333_set_vcore_vosel_on_spm(kal_uint8 val);
extern void mt6333_set_rg_vmem_rsv1_5_0(kal_uint8 val);
extern void mt6333_set_rg_r2r_event_sync_sel(kal_uint8 val);
extern void mt6333_set_rg_vmem_en_thr_sdn_sel(kal_uint8 val);
extern void mt6333_set_rg_vmem_triml(kal_uint8 val);
extern void mt6333_set_rg_vmem_trimh(kal_uint8 val);
extern void mt6333_set_rg_vmem_cc(kal_uint8 val);
extern void mt6333_set_rg_vmem_rzsel(kal_uint8 val);
extern void mt6333_set_rg_vmem_slp(kal_uint8 val);
extern void mt6333_set_rg_vmem_csl(kal_uint8 val);
extern void mt6333_set_rg_vmem_csr(kal_uint8 val);
extern void mt6333_set_rg_vmem_avp_os(kal_uint8 val);
extern void mt6333_set_rg_vmem_avp_en(kal_uint8 val);
extern void mt6333_set_rg_vmem_ndis_en(kal_uint8 val);
extern void mt6333_set_rg_vmem_modeset(kal_uint8 val);
extern void mt6333_set_rg_vmem_zx_os(kal_uint8 val);
extern void mt6333_set_rg_vmem_csm(kal_uint8 val);
extern void mt6333_set_rg_vmem_zxos_trim(kal_uint8 val);
extern void mt6333_set_rg_vmem_rsv(kal_uint8 val);
extern kal_uint8 mt6333_get_qi_vmem_dig_mon(void);
extern kal_uint8 mt6333_get_qi_vmem_oc_status(void);
extern void mt6333_set_qi_vmem_vsleep(kal_uint8 val);
extern void mt6333_set_vmem_en(kal_uint8 val);
extern kal_uint8 mt6333_get_qi_vmem_stb(void);
extern kal_uint8 mt6333_get_qi_vmem_en(void);
extern void mt6333_set_vmem_en_ctrl(kal_uint8 val);
extern void mt6333_set_vmem_vosel_ctrl(kal_uint8 val);
extern void mt6333_set_vmem_dlc_ctrl(kal_uint8 val);
extern void mt6333_set_vmem_burst_ctrl(kal_uint8 val);
extern void mt6333_set_vmem_vosel(kal_uint8 val);
extern void mt6333_set_vmem_vosel_on(kal_uint8 val);
extern void mt6333_set_vmem_vosel_sleep(kal_uint8 val);
extern kal_uint8 mt6333_get_ni_vmem_vosel(void);
extern kal_uint8 mt6333_get_qi_vmem_burst(void);
extern void mt6333_set_vmem_burst(kal_uint8 val);
extern void mt6333_set_vmem_burst_on(kal_uint8 val);
extern void mt6333_set_vmem_burst_sleep(kal_uint8 val);
extern void mt6333_set_vmem_dlc(kal_uint8 val);
extern void mt6333_set_vmem_dlc_on(kal_uint8 val);
extern void mt6333_set_vmem_dlc_sleep(kal_uint8 val);
extern kal_uint8 mt6333_get_qi_vmem_dlc(void);
extern kal_uint8 mt6333_get_qi_vmem_dlc_n(void);
extern void mt6333_set_vmem_dlc_n(kal_uint8 val);
extern void mt6333_set_vmem_dlc_n_on(kal_uint8 val);
extern void mt6333_set_vmem_dlc_n_sleep(kal_uint8 val);
extern void mt6333_set_vmem_vsleep_en(kal_uint8 val);
extern void mt6333_set_vmem_r2r_pdn(kal_uint8 val);
extern void mt6333_set_vmem_vsleep_sel(kal_uint8 val);
extern kal_uint8 mt6333_get_ni_vmem_r2r_pdn(void);
extern kal_uint8 mt6333_get_ni_vmem_vsleep_sel(void);
extern void mt6333_set_vmem_transtd(kal_uint8 val);
extern void mt6333_set_vmem_vosel_trans_en(kal_uint8 val);
extern void mt6333_set_vmem_vosel_trans_once(kal_uint8 val);
extern kal_uint8 mt6333_get_ni_vmem_vosel_trans(void);
extern void mt6333_set_rg_vrf18_vocal(kal_uint8 val);
extern void mt6333_set_rg_vrf18_gmsel(kal_uint8 val);
extern void mt6333_set_rg_vrf18_bk_ldo(kal_uint8 val);
extern void mt6333_set_rg_vrf18_slew_nmos(kal_uint8 val);
extern void mt6333_set_rg_vrf18_slew(kal_uint8 val);
extern void mt6333_set_rg_vrf18_cc(kal_uint8 val);
extern void mt6333_set_rg_vrf18_rzsel(kal_uint8 val);
extern void mt6333_set_rg_vrf18_slp(kal_uint8 val);
extern void mt6333_set_rg_vrf18_csl(kal_uint8 val);
extern void mt6333_set_rg_vrf18_csr(kal_uint8 val);
extern void mt6333_set_rg_vrf18_ndis_en(kal_uint8 val);
extern void mt6333_set_rg_vrf18_zx_os(kal_uint8 val);
extern void mt6333_set_rg_vrf18_burstl(kal_uint8 val);
extern void mt6333_set_rg_vrf18_bursth(kal_uint8 val);
extern void mt6333_set_rg_vrf18_rsv(kal_uint8 val);
extern kal_uint8 mt6333_get_qi_vrf18_oc_status(void);
extern void mt6333_set_rg_buck_rsv1(kal_uint8 val);
extern void mt6333_set_vrf18_vosel_ctrl(kal_uint8 val);
extern void mt6333_set_vrf18_dlc_ctrl(kal_uint8 val);
extern void mt6333_set_vrf18_burst_ctrl(kal_uint8 val);
extern void mt6333_set_vrf18_vosel(kal_uint8 val);
extern void mt6333_set_vrf18_vosel_on(kal_uint8 val);
extern void mt6333_set_vrf18_vosel_sleep(kal_uint8 val);
extern kal_uint8 mt6333_get_ni_vrf18_vosel(void);
extern void mt6333_set_rg_buck_mon_flag_sel(kal_uint8 val);
extern void mt6333_set_rg_buck_rsv2_5_0(kal_uint8 val);
extern void mt6333_set_rg_buck_bypass_vosel_limit(kal_uint8 val);
extern void mt6333_set_rg_buck_mon_flag_en(kal_uint8 val);
extern void mt6333_set_vrf18_dlc(kal_uint8 val);
extern void mt6333_set_vrf18_dlc_on(kal_uint8 val);
extern void mt6333_set_vrf18_dlc_sleep(kal_uint8 val);
extern kal_uint8 mt6333_get_qi_vrf18_dlc(void);
extern kal_uint8 mt6333_get_qi_vrf18_dlc_n(void);
extern void mt6333_set_vrf18_dlc_n(kal_uint8 val);
extern void mt6333_set_vrf18_dlc_n_on(kal_uint8 val);
extern void mt6333_set_vrf18_dlc_n_sleep(kal_uint8 val);
extern void mt6333_set_rg_vrf18_modeset(kal_uint8 val);
extern void mt6333_set_vrf18_en(kal_uint8 val);
extern void mt6333_set_rg_vrf18_stb_sel(kal_uint8 val);
extern kal_uint8 mt6333_get_qi_vrf18_stb(void);
extern kal_uint8 mt6333_get_qi_vrf18_en(void);
extern void mt6333_set_vrf18_en_ctrl(kal_uint8 val);
extern void mt6333_set_rg_vrf18_modeset_spm(kal_uint8 val);
extern void mt6333_set_vrf18_en_spm(kal_uint8 val);
extern void mt6333_set_rg_vcore_vosel_set_spm(kal_uint8 val);
extern void mt6333_set_k_rst_done(kal_uint8 val);
extern void mt6333_set_k_map_sel(kal_uint8 val);
extern void mt6333_set_k_once_en(kal_uint8 val);
extern void mt6333_set_k_once(kal_uint8 val);
extern void mt6333_set_k_start_manual(kal_uint8 val);
extern void mt6333_set_k_src_sel(kal_uint8 val);
extern void mt6333_set_k_auto_en(kal_uint8 val);
extern void mt6333_set_k_inv(kal_uint8 val);
extern void mt6333_set_k_control_smps(kal_uint8 val);
extern kal_uint8 mt6333_get_qi_smps_osc_cal(void);
extern kal_uint8 mt6333_get_k_result(void);
extern kal_uint8 mt6333_get_k_done(void);
extern kal_uint8 mt6333_get_k_control(void);
extern void mt6333_set_k_buck_ck_cnt(kal_uint8 val);
extern void mt6333_set_k_chr_ck_cnt(kal_uint8 val);
extern void mt6333_set_rg_strup_rsv1_2_0(kal_uint8 val);
extern void mt6333_set_rg_strup_sleep_mode_deb_sel(kal_uint8 val);
extern void mt6333_set_rg_strup_poseq_done_deb_sel(kal_uint8 val);
extern void mt6333_set_rg_strup_vmem_en_deb_sel(kal_uint8 val);
extern void mt6333_set_rg_strup_vcore_en_deb_sel(kal_uint8 val);
extern void mt6333_set_rg_strup_buck_en_deb_sel(kal_uint8 val);
extern void mt6333_set_rg_strup_osc_en(kal_uint8 val);
extern void mt6333_set_rg_strup_osc_en_sel(kal_uint8 val);
extern void mt6333_set_rg_strup_ivgen_enb(kal_uint8 val);
extern void mt6333_set_rg_strup_ivgen_enb_sel(kal_uint8 val);
extern void mt6333_set_rg_strup_rsv2_7_4(kal_uint8 val);
extern void mt6333_set_rg_strup_rsv3_7_0(kal_uint8 val);
extern void mt6333_set_rg_efuse_addr(kal_uint8 val);
extern void mt6333_set_rg_efuse_prog(kal_uint8 val);
extern void mt6333_set_rg_efuse_en(kal_uint8 val);
extern void mt6333_set_rg_efuse_pkey(kal_uint8 val);
extern void mt6333_set_rg_efuse_rd_trig(kal_uint8 val);
extern void mt6333_set_rg_efuse_prog_src(kal_uint8 val);
extern void mt6333_set_rg_prog_macro_sel(kal_uint8 val);
extern void mt6333_set_rg_rd_rdy_bypass(kal_uint8 val);
extern kal_uint8 mt6333_get_rg_efuse_rd_ack(void);
extern kal_uint8 mt6333_get_rg_efuse_busy(void);
extern void mt6333_set_rg_otp_pa(kal_uint8 val);
extern void mt6333_set_rg_otp_pdin(kal_uint8 val);
extern void mt6333_set_rg_otp_ptm(kal_uint8 val);
extern void mt6333_set_rg_fsource_en(kal_uint8 val);
extern kal_uint8 mt6333_get_rg_efuse_dout_0_7(void);
extern kal_uint8 mt6333_get_rg_efuse_dout_8_15(void);
extern kal_uint8 mt6333_get_rg_efuse_dout_16_23(void);
extern kal_uint8 mt6333_get_rg_efuse_dout_24_31(void);
extern kal_uint8 mt6333_get_rg_efuse_dout_32_39(void);
extern kal_uint8 mt6333_get_rg_efuse_dout_40_47(void);
extern kal_uint8 mt6333_get_rg_efuse_dout_48_55(void);
extern kal_uint8 mt6333_get_rg_efuse_dout_56_63(void);
extern void mt6333_set_testi0(kal_uint8 val);
extern void mt6333_set_testi1(kal_uint8 val);
extern void mt6333_set_testi2(kal_uint8 val);
extern void mt6333_set_testi3(kal_uint8 val);
extern void mt6333_set_testi4(kal_uint8 val);
extern void mt6333_set_testi5(kal_uint8 val);
extern void mt6333_set_testi6(kal_uint8 val);
extern void mt6333_set_testi7(kal_uint8 val);
extern void mt6333_set_testi8(kal_uint8 val);
extern void mt6333_set_testi0_sel(kal_uint8 val);
extern void mt6333_set_testi1_sel(kal_uint8 val);
extern void mt6333_set_testi2_sel(kal_uint8 val);
extern void mt6333_set_testi3_sel(kal_uint8 val);
extern void mt6333_set_testi4_sel(kal_uint8 val);
extern void mt6333_set_testi5_sel(kal_uint8 val);
extern void mt6333_set_testi6_sel(kal_uint8 val);
extern void mt6333_set_testi7_sel(kal_uint8 val);
extern void mt6333_set_testi8_sel(kal_uint8 val);
extern void mt6333_set_testo0(kal_uint8 val);
extern void mt6333_set_testo1(kal_uint8 val);
extern void mt6333_set_testo2(kal_uint8 val);
extern void mt6333_set_testo3(kal_uint8 val);
extern void mt6333_set_testo0_sel(kal_uint8 val);
extern void mt6333_set_testo1_sel(kal_uint8 val);
extern void mt6333_set_testo2_sel(kal_uint8 val);
extern void mt6333_set_testo3_sel(kal_uint8 val);
extern void mt6333_set_debug_sel(kal_uint8 val);
extern kal_uint8 mt6333_get_debug_mon(void);
extern void mt6333_set_debug_bit_sel(kal_uint8 val);
extern kal_uint8 mt6333_get_cid1(void);

extern void mt6333_dump_register(void);
extern kal_uint32 mt6333_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT);
extern kal_uint32 mt6333_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT);

#endif // _MT6333_SW_H_

