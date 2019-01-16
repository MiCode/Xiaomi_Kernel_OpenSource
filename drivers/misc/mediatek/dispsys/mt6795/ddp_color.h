#ifndef __DDP_COLOR_H__
#define __DDP_COLOR_H__

#include "ddp_reg.h"
#include "ddp_aal.h"
#include "ddp_drv.h"
enum
{
    ENUM_Y_SLOPE = 0 * 28,

    ENUM_S_GAIN1 = 1 * 28,
    ENUM_S_GAIN2 = 2 * 28,
    ENUM_S_GAIN3 = 3 * 28,
    ENUM_S_P1 = 4 * 28,
    ENUM_S_P2 = 5 * 28,

    ENUM_H_FTN = 6 * 28,

};

enum
{
    COLOR_ID_0 = 0,
    COLOR_ID_1 = 1,
};

#define C0_OFFSET (0)
#define C1_OFFSET (DISPSYS_COLOR1_BASE - DISPSYS_COLOR0_BASE)

//---------------------------------------------------------------------------
#define GAMMA_SIZE 1024
#define NR_SIZE 50
#define MAX_SCE_TABLE_SIZE (7*28)
#define SCE_PHASE 28
#define SCE_SIZE SCE_PHASE*7

#define PURP_TONE    0
#define SKIN_TONE    1
#define GRASS_TONE   2
#define SKY_TONE     3

#define PURP_TONE_START    0
#define PURP_TONE_END      2
#define SKIN_TONE_START    3
#define SKIN_TONE_END     10
#define GRASS_TONE_START  11
#define GRASS_TONE_END    16
#define SKY_TONE_START    17
#define SKY_TONE_END      19

#define SG1 0
#define SG2 1
#define SG3 2
#define SP1 3
#define SP2 4

#define MIRAVISION_HW_VERSION_MASK  (0xFF000000)
#define MIRAVISION_SW_VERSION_MASK  (0x00FF0000)
#define MIRAVISION_SW_FEATURE_MASK  (0x0000FFFF)
#define MIRAVISION_HW_VERSION_SHIFT (24)
#define MIRAVISION_SW_VERSION_SHIFT (16)
#define MIRAVISION_SW_FEATURE_SHIFT (0)

#ifdef CONFIG_ARCH_MT6795
#define MIRAVISION_HW_VERSION       (3)    // 1:6595 2:6752 3:6795 4:6735
#else
#define MIRAVISION_HW_VERSION       (1)    // 1:6595 2:6752 3:6795 4:6735
#endif
#define MIRAVISION_SW_VERSION       (1)    // 1:Android Lollipop
#define MIRAVISION_SW_FEATURE_VIDEO_DC  (0x1)
#define MIRAVISION_SW_FEATURE_AAL       (0x2)
#define MIRAVISION_VERSION          ((MIRAVISION_HW_VERSION << MIRAVISION_HW_VERSION_SHIFT) |   \
                                     (MIRAVISION_SW_VERSION << MIRAVISION_SW_VERSION_SHIFT) |   \
                                     MIRAVISION_SW_FEATURE_VIDEO_DC |   \
                                     MIRAVISION_SW_FEATURE_AAL)

#define SW_VERSION_VIDEO_DC         (1)
#define SW_VERSION_AAL              (1)

#define DISP_COLOR_SWREG_START      (0xFFFF0000)
#define DISP_COLOR_SWREG_COLOR_BASE (DISP_COLOR_SWREG_START)                // 0xFFFF0000
#define DISP_COLOR_SWREG_TDSHP_BASE (DISP_COLOR_SWREG_COLOR_BASE + 0x1000)  // 0xFFFF1000
#define DISP_COLOR_SWREG_PQDC_BASE  (DISP_COLOR_SWREG_TDSHP_BASE + 0x1000)  // 0xFFFF2000
#define DISP_COLOR_SWREG_END        (DISP_COLOR_SWREG_PQDC_BASE + 0x1000)   // 0xFFFF3000

#define SWREG_COLOR_BASE_ADDRESS            (DISP_COLOR_SWREG_COLOR_BASE + 0x0000)
#define SWREG_GAMMA_BASE_ADDRESS            (DISP_COLOR_SWREG_COLOR_BASE + 0x0001)
#define SWREG_TDSHP_BASE_ADDRESS            (DISP_COLOR_SWREG_COLOR_BASE + 0x0002)
#define SWREG_AAL_BASE_ADDRESS              (DISP_COLOR_SWREG_COLOR_BASE + 0x0003)
#define SWREG_MIRAVISION_VERSION            (DISP_COLOR_SWREG_COLOR_BASE + 0x0004)
#define SWREG_SW_VERSION_VIDEO_DC           (DISP_COLOR_SWREG_COLOR_BASE + 0x0005)
#define SWREG_SW_VERSION_AAL                (DISP_COLOR_SWREG_COLOR_BASE + 0x0006)

#define SWREG_TDSHP_TUNING_MODE             (DISP_COLOR_SWREG_TDSHP_BASE + 0x0000)

#define SWREG_PQDC_BLACK_EFFECT_ENABLE      (DISP_COLOR_SWREG_PQDC_BASE + BlackEffectEnable)
#define SWREG_PQDC_WHITE_EFFECT_ENABLE      (DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectEnable)
#define SWREG_PQDC_STRONG_BLACK_EFFECT      (DISP_COLOR_SWREG_PQDC_BASE + StrongBlackEffect)
#define SWREG_PQDC_STRONG_WHITE_EFFECT      (DISP_COLOR_SWREG_PQDC_BASE + StrongWhiteEffect)
#define SWREG_PQDC_ADAPTIVE_BLACK_EFFECT    (DISP_COLOR_SWREG_PQDC_BASE + AdaptiveBlackEffect)
#define SWREG_PQDC_ADAPTIVE_WHITE_EFFECT    (DISP_COLOR_SWREG_PQDC_BASE + AdaptiveWhiteEffect)
#define SWREG_PQDC_SCENCE_CHANGE_ONCE_EN    (DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeOnceEn)
#define SWREG_PQDC_SCENCE_CHANGE_CONTROL_EN (DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeControlEn)
#define SWREG_PQDC_SCENCE_CHANGE_CONTROL    (DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeControl)
#define SWREG_PQDC_SCENCE_CHANGE_TH1        (DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeTh1)
#define SWREG_PQDC_SCENCE_CHANGE_TH2        (DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeTh2)
#define SWREG_PQDC_SCENCE_CHANGE_TH3        (DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeTh3)
#define SWREG_PQDC_CONTENT_SMOOTH1          (DISP_COLOR_SWREG_PQDC_BASE + ContentSmooth1)
#define SWREG_PQDC_CONTENT_SMOOTH2          (DISP_COLOR_SWREG_PQDC_BASE + ContentSmooth2)
#define SWREG_PQDC_CONTENT_SMOOTH3          (DISP_COLOR_SWREG_PQDC_BASE + ContentSmooth3)
#define SWREG_PQDC_MIDDLE_REGION_GAIN1      (DISP_COLOR_SWREG_PQDC_BASE + MiddleRegionGain1)
#define SWREG_PQDC_MIDDLE_REGION_GAIN2      (DISP_COLOR_SWREG_PQDC_BASE + MiddleRegionGain2)
#define SWREG_PQDC_BLACK_REGION_GAIN1       (DISP_COLOR_SWREG_PQDC_BASE + BlackRegionGain1)
#define SWREG_PQDC_BLACK_REGION_GAIN2       (DISP_COLOR_SWREG_PQDC_BASE + BlackRegionGain2)
#define SWREG_PQDC_BLACK_REGION_RANGE       (DISP_COLOR_SWREG_PQDC_BASE + BlackRegionRange)
#define SWREG_PQDC_BLACK_EFFECT_LEVEL       (DISP_COLOR_SWREG_PQDC_BASE + BlackEffectLevel)
#define SWREG_PQDC_BLACK_EFFECT_PARAM1      (DISP_COLOR_SWREG_PQDC_BASE + BlackEffectParam1)
#define SWREG_PQDC_BLACK_EFFECT_PARAM2      (DISP_COLOR_SWREG_PQDC_BASE + BlackEffectParam2)
#define SWREG_PQDC_BLACK_EFFECT_PARAM3      (DISP_COLOR_SWREG_PQDC_BASE + BlackEffectParam3)
#define SWREG_PQDC_BLACK_EFFECT_PARAM4      (DISP_COLOR_SWREG_PQDC_BASE + BlackEffectParam4)
#define SWREG_PQDC_WHITE_REGION_GAIN1       (DISP_COLOR_SWREG_PQDC_BASE + WhiteRegionGain1)
#define SWREG_PQDC_WHITE_REGION_GAIN2       (DISP_COLOR_SWREG_PQDC_BASE + WhiteRegionGain2)
#define SWREG_PQDC_WHITE_REGION_RANGE       (DISP_COLOR_SWREG_PQDC_BASE + WhiteRegionRange)
#define SWREG_PQDC_WHITE_EFFECT_LEVEL       (DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectLevel)
#define SWREG_PQDC_WHITE_EFFECT_PARAM1      (DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectParam1)
#define SWREG_PQDC_WHITE_EFFECT_PARAM2      (DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectParam2)
#define SWREG_PQDC_WHITE_EFFECT_PARAM3      (DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectParam3)
#define SWREG_PQDC_WHITE_EFFECT_PARAM4      (DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectParam4)
#define SWREG_PQDC_CONTRAST_ADJUST1         (DISP_COLOR_SWREG_PQDC_BASE + ContrastAdjust1)
#define SWREG_PQDC_CONTRAST_ADJUST2         (DISP_COLOR_SWREG_PQDC_BASE + ContrastAdjust2)
#define SWREG_PQDC_DC_CHANGE_SPEED_LEVEL    (DISP_COLOR_SWREG_PQDC_BASE + DCChangeSpeedLevel)
#define SWREG_PQDC_PROTECT_REGION_EFFECT    (DISP_COLOR_SWREG_PQDC_BASE + ProtectRegionEffect)
#define SWREG_PQDC_DC_CHANGE_SPEED_LEVEL2   (DISP_COLOR_SWREG_PQDC_BASE + DCChangeSpeedLevel2)
#define SWREG_PQDC_PROTECT_REGION_WEIGHT    (DISP_COLOR_SWREG_PQDC_BASE + ProtectRegionWeight)
#define SWREG_PQDC_DC_ENABLE                (DISP_COLOR_SWREG_PQDC_BASE + DCEnable)



//---------------------------------------------------------------------------
void DpEngine_COLORonInit(DISP_MODULE_ENUM module, void* __cmdq);
void DpEngine_COLORonConfig(DISP_MODULE_ENUM module, unsigned int srcWidth, unsigned int srcHeight, void* __cmdq);

DISP_PQ_PARAM * get_Color_config(int id);
DISP_PQ_PARAM * get_Color_Cam_config(void);
DISP_PQ_PARAM * get_Color_Gal_config(void);
DISPLAY_PQ_T * get_Color_index(void);
extern DISPLAY_TDSHP_T * get_TDSHP_index(void);

void disp_color_set_window(unsigned int sat_upper, unsigned int sat_lower,
			   unsigned int hue_upper, unsigned int hue_lower);

#endif

