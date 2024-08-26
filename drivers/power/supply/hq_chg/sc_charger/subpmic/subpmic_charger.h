// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 - 2023 SOUTHCHIP Semiconductor Technology(Shanghai) Co., Ltd.
 */
#ifndef __LINUX_SUBPMIC_CHARGER_H__
#define __LINUX_SUBPMIC_CHARGER_H__

struct buck_init_data {
    u32 vsyslim;
    u32 batsns_en;
    u32 vbat;
    u32 ichg;
    u32 vindpm;
    u32 iindpm_dis;
    u32 iindpm;
    u32 ico_enable;
    u32 iindpm_ico;
    u32 vprechg;
    u32 iprechg;
    u32 iterm_en;
    u32 iterm;
    u32 rechg_dis;
    u32 rechg_dg;
    u32 rechg_volt;
    u32 vboost;
    u32 iboost;
    u32 conv_ocp_dis;
    u32 tsbat_jeita_dis;
    u32 ibat_ocp_dis;
    u32 vpmid_ovp_otg_dis;
    u32 vbat_ovp_buck_dis;
    u32 ibat_ocp;
};

enum subpmic_chg_fields {
    F_VAC_OVP,F_VBUS_OVP,
    F_TSBUS_FLT,
    F_TSBAT_FLT,
    F_ACDRV_MANUAL_PRE,F_ACDRV_EN,F_ACDRV_MANUAL_EN,F_WD_TIME_RST,F_WD_TIMER,
    F_REG_RST,F_VBUS_PD,F_VAC_PD,F_CID_EN,
    F_ADC_EN,F_ADC_FREEZE,F_BATID_ADC_EN,
    F_EDL_ACTIVE_LEVEL,
    /******* charger *******/
    F_VSYS_MIN,     /* REG30 */
    F_BATSNS_EN,F_VBAT, /* REG31 */
    F_ICHG_CC, /* REG32 */
    F_VINDPM_VBAT,F_VINDPM_DIS,F_VINDPM, /* REG33 */
    F_IINDPM_DIS,F_IINDPM,  /* REG34 */
    F_FORCE_ICO,F_ICO_EN,F_IINDPM_ICO,  /* REG35 */
    F_VBAT_PRECHG,F_IPRECHG,    /* REG36 */
    F_TERM_EN,F_ITERM,  /* REG37 */
    F_RECHG_DIS,F_RECHG_DG,F_VRECHG,    /* REG38 */
    F_VBOOST,F_IBOOST,  /* REG39 */
    F_CONV_OCP_DIS,F_TSBAT_JEITA_DIS,F_IBAT_OCP_DIS,F_VPMID_OVP_OTG_DIS,F_VBAT_OVP_BUCK_DIS,    /* REG3A */
    F_T_BATFET_RST,F_T_PD_nRST,F_BATFET_RST_EN,F_BATFET_DLY,F_BATFET_DIS,F_nRST_SHIPMODE_DIS,   /* REG3B */
    F_HIZ_EN,F_PERFORMANCE_EN,F_DIS_BUCKCHG_PATH,F_DIS_SLEEP_FOR_OTG,F_QB_EN,F_BOOST_EN,F_CHG_EN,   /* REG3C */
    F_VBAT_TRACK,F_IBATOCP,F_VSYSOVP_DIS,F_VSYSOVP_TH,  /* REG3D */
    F_BAT_COMP,F_VCLAMP,F_JEITA_ISET_COOL,F_JEITA_VSET_WARM,    /* REG3E */
    F_TMR2X_EN,F_CHG_TIMER_EN,F_CHG_TIMER,F_TDIE_REG_DIS,F_TDIE_REG,F_PFM_DIS,  /* REG3F */
    F_BAT_COMP_OFF,F_VBAT_LOW_OTG,F_BOOST_FREQ,F_BUCK_FREQ,F_BAT_LOAD_EN, /* REG40 */
    /*
    F_VSYS_SHORT_STAT,F_VSLEEP_BUCK_STAT,F_VBAT_DPL_STAT,F_VBAT_LOW_BOOST_STAT,F_VBUS_GOOD_STAT,
    F_CHG_STAT,F_BOOST_OK_STAT,F_VSYSMIN_REG_STAT,F_QB_ON_STAT,F_BATFET_STAT,
    F_TDIE_REG_STAT,F_TSBAT_COOL_STAT,F_TSBAT_WARM_STAT,F_ICO_STAT,F_IINDPM_STAT,F_VINDPM_STAT, */
    F_JEITA_COOL_TEMP,F_JEITA_WARM_TEMP,F_BOOST_NTC_HOT_TEMP,F_BOOST_NTC_COLD_TEMP, /* REG56 */
    F_TESTM_EN, /* REG5D */
    F_KEY_EN_OWN,   /* REG5E */
    /****** led ********/
    F_TRPT,F_FL_TX_EN,F_TLED2_EN,F_TLED1_EN,F_FLED2_EN,F_FLED1_EN,  /* reg80 */
    F_FLED1_BR, /* reg81 */
    F_FLED2_BR,/* reg82 */
    F_FTIMEOUT,F_FRPT,F_FTIMEOUT_EN,/* reg83 */
    F_TLED1_BR,/* reg84 */
    F_TLED2_BR,/* reg85 */
    F_PMID_FLED_OVP_DEG,F_VBAT_MIN_FLED,F_VBAT_MIN_FLED_DEG,F_LED_POWER,/* reg86 */
    /****** DPDPM ******/
    F_FORCE_INDET,F_AUTO_INDET_EN,F_HVDCP_EN,F_QC_EN,
    F_DP_DRIV,F_DM_DRIV,F_BC1_2_VDAT_REF_SET,F_BC1_2_DP_DM_SINK_CAP,
    F_QC2_V_MAX,F_QC3_PULS,F_QC3_MINUS,F_QC3_5_16_PLUS,F_QC3_5_16_MINUS,F_QC3_5_3_SEQ,F_QC3_5_2_SEQ,
    F_I2C_DPDM_BYPASS_EN,F_DPDM_PULL_UP_EN,F_WDT_TFCP_MASK,F_WDT_TFCP_FLAG,F_WDT_TFCP_RST,F_WDT_TFCP_CFG,F_WDT_TFCP_DIS,
    F_VBUS_STAT,F_BC1_2_DONE,F_DP_OVP,F_DM_OVP,
    F_DM_500K_PD_EN,F_DP_500K_PD_EN,F_DM_SINK_EN,F_DP_SINK_EN,F_DP_SRC_10UA,

    F_MAX_FIELDS,
};

enum ADC_MODULE {
    ADC_IBUS = 0,
    ADC_VBUS,
    ADC_VAC,
    ADC_VBATSNS,
    ADC_VBAT,
    ADC_IBAT,
    ADC_VSYS,
    ADC_TSBUS,
    ADC_TSBAT,
    ADC_TDIE,
    ADC_BATID,
};

static const u32 adc_step[] = { 
    2500, 3750, 5000, 1250, 1250,
    1220, 1250, 9766, 9766, 5, 156,
};

enum {
    SUBPMIC_CHG_STATE_NO_CHG = 0,
    SUBPMIC_CHG_STATE_TRICK,
    SUBPMIC_CHG_STATE_PRECHG,
    SUBPMIC_CHG_STATE_CC,
    SUBPMIC_CHG_STATE_CV,
    SUBPMIC_CHG_STATE_TERM,
};

enum DPDM_DRIVE {
    DPDM_HIZ = 0,
    DPDM_20K_DOWN,
    DPDM_V0_6,
    DPDM_V1_8,
    DPDM_V2_7,
    DPDM_V3_3,
    DPDM_500K_DOWN,
};

enum DPDM_CAP {
    DPDM_CAP_SNK_50UA = 0,
    DPDM_CAP_SNK_100UA,
    DPDM_CAP_SRC_10UA,
    DPDM_CAP_SRC_250UA,
    DPDM_CAP_DISABLE,
};

struct chip_state {
    bool online;
    bool boost_good;
    int vbus_type;
    int chg_state;
    int vindpm;
};

enum {
    IRQ_HK = 0,
    IRQ_BUCK,
    IRQ_DPDM,
    IRQ_LED,
    IRQ_MAX,
};

enum LED_FLASH_MODULE {
    LED1_FLASH = 1,
    LED2_FLASH,
    LED_ALL_FLASH,
};

struct subpmic_chg_device {
    struct i2c_client *client;
    struct device *dev;
    struct regmap *rmap;
    struct regmap_field *rmap_fields[F_MAX_FIELDS];

    struct buck_init_data buck_init;
    struct chip_state state;

    struct delayed_work led_work;
    enum LED_FLASH_MODULE led_index;
    struct completion flash_run;
    struct completion flash_end;
    bool led_state;
    atomic_t led_work_running;

    unsigned long request_otg;
    int irq[IRQ_MAX];

    struct charger_dev *sc_charger;
    struct chargerpump_dev *charger_pump;

    struct subpmic_led_dev *led_dev;

    bool use_soft_bc12;
    struct soft_bc12 *bc;
    struct notifier_block bc12_result_nb;

    struct timer_list bc12_timeout;

    struct mutex bc_detect_lock;
    struct mutex adc_read_lock;

    struct work_struct qc_detect_work;
    int qc_result;
    int qc_vbus;
    bool qc3_support;
};

extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
#endif
