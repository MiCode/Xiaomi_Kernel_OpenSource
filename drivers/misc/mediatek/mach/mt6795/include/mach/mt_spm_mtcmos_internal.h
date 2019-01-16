#ifndef _MT_SPM_MTCMOS_INTERNAL_
#define _MT_SPM_MTCMOS_INTERNAL_


/**************************************
 * for CPU MTCMOS
 **************************************/
/*
 * regiser bit difinition
 */
/* SPM_CA7_CPU0_PWR_CON */
/* SPM_CA7_CPU1_PWR_CON */
/* SPM_CA7_CPU2_PWR_CON */
/* SPM_CA7_CPU3_PWR_CON */
/* SPM_CA7_DBG_PWR_CON */
/* SPM_CA7_CPUTOP_PWR_CON */
/* SPM_CA15_CPU0_PWR_CON */
/* SPM_CA15_CPU1_PWR_CON */
/* SPM_CA15_CPU2_PWR_CON */
/* SPM_CA15_CPU3_PWR_CON */
/* SPM_CA15_CPUTOP_PWR_CON */
#define SRAM_ISOINT_B           (1U << 6)
#define SRAM_CKISO              (1U << 5)
#define PWR_CLK_DIS             (1U << 4)
#define PWR_ON_2ND              (1U << 3)
#define PWR_ON                  (1U << 2)
#define PWR_ISO                 (1U << 1)
#define PWR_RST_B               (1U << 0)

/* SPM_CA7_CPU0_L1_PDN */
/* SPM_CA7_CPU1_L1_PDN */
/* SPM_CA7_CPU2_L1_PDN */
/* SPM_CA7_CPU3_L1_PDN */
#define L1_PDN_ACK              (1U << 8)
#define L1_PDN                  (1U << 0)
/* SPM_CA7_CPUTOP_L2_PDN */
#define L2_SRAM_PDN_ACK         (1U << 8)
#define L2_SRAM_PDN             (1U << 0)
/* SPM_CA7_CPUTOP_L2_SLEEP */
#define L2_SRAM_SLEEP_B_ACK     (1U << 8)
#define L2_SRAM_SLEEP_B         (1U << 0)

/* SPM_CA15_L1_PWR_CON */
#define CPU3_CA15_L1_PDN_ACK    (1U << 11)
#define CPU2_CA15_L1_PDN_ACK    (1U << 10)
#define CPU1_CA15_L1_PDN_ACK    (1U <<  9)
#define CPU0_CA15_L1_PDN_ACK    (1U <<  8)
#define CPU3_CA15_L1_PDN_ISO    (1U <<  7)
#define CPU2_CA15_L1_PDN_ISO    (1U <<  6)
#define CPU1_CA15_L1_PDN_ISO    (1U <<  5)
#define CPU0_CA15_L1_PDN_ISO    (1U <<  4)
#define CPU3_CA15_L1_PDN        (1U <<  3)
#define CPU2_CA15_L1_PDN        (1U <<  2)
#define CPU1_CA15_L1_PDN        (1U <<  1)
#define CPU0_CA15_L1_PDN        (1U <<  0)
/* SPM_CA15_L2_PWR_CON */
#define CA15_L2_SLEEPB_ACK      (1U << 10)
#define CA15_L2_PDN_ACK         (1U <<  8)
#define CA15_L2_SLEEPB_ISO      (1U <<  6)
#define CA15_L2_SLEEPB          (1U <<  4)
#define CA15_L2_PDN_ISO         (1U <<  2)
#define CA15_L2_PDN             (1U <<  0)

/* SPM_PWR_STATUS */
/* SPM_PWR_STATUS_2ND */
#define CA15_CPU3               (1U << 19)
#define CA15_CPU2               (1U << 18)
#define CA15_CPU1               (1U << 17)
#define CA15_CPU0               (1U << 16)
#define CA15_CPUTOP             (1U << 15)
#define CA7_DBG                 (1U << 13)
#define CA7_CPU3                (1U << 12)
#define CA7_CPU2                (1U << 11)
#define CA7_CPU1                (1U << 10)
#define CA7_CPU0                (1U <<  9)
#define CA7_CPUTOP              (1U <<  8)

/* SPM_SLEEP_TIMER_STA */
#define CA15_CPUTOP_STANDBYWFI  (1U << 25)
#define CA7_CPUTOP_STANDBYWFI   (1U << 24)
#define CA15_CPU3_STANDBYWFI    (1U << 23)
#define CA15_CPU2_STANDBYWFI    (1U << 22)
#define CA15_CPU1_STANDBYWFI    (1U << 21)
#define CA15_CPU0_STANDBYWFI    (1U << 20)
#define CA7_CPU3_STANDBYWFI     (1U << 19)
#define CA7_CPU2_STANDBYWFI     (1U << 18)
#define CA7_CPU1_STANDBYWFI     (1U << 17)
#define CA7_CPU0_STANDBYWFI     (1U << 16)

/* SPM_SLEEP_DUAL_VCORE_PWR_CON */
#define VCA15_PWR_ISO           (1U << 13)
#define VCA7_PWR_ISO            (1U << 12)

/* INFRA_TOPAXI_PROTECTEN */
#define CA15_PDN_REQ            (30)
#define CA7_PDN_REQ             (29)


#endif //#ifndef _MT_SPM_MTCMOS_INTERNAL_