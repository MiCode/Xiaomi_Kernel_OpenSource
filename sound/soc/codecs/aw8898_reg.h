#ifndef _AW8898_REG_H_
#define _AW8898_REG_H_

/********************************************
 * Register List
 *******************************************/
#define AW8898_REG_ID           0x00
#define AW8898_REG_SYSST        0x01
#define AW8898_REG_SYSINT       0x02
#define AW8898_REG_SYSINTM      0x03
#define AW8898_REG_SYSCTRL      0x04
#define AW8898_REG_I2SCTRL      0x05
#define AW8898_REG_I2STXCFG     0x06
#define AW8898_REG_PWMCTRL      0x08
#define AW8898_REG_HAGCCFG1     0x09
#define AW8898_REG_HAGCCFG2     0x0A
#define AW8898_REG_HAGCCFG3     0x0B
#define AW8898_REG_HAGCCFG4     0x0C
#define AW8898_REG_HAGCCFG5     0x0D
#define AW8898_REG_HAGCCFG6     0x0E
#define AW8898_REG_HAGCCFG7     0x0F
#define AW8898_REG_HAGCST       0x10
#define AW8898_REG_DBGCTRL      0x20
#define AW8898_REG_I2SCFG       0x21
#define AW8898_REG_I2SSTAT      0x22
#define AW8898_REG_I2SCAPCNT    0x23
#define AW8898_REG_GENCTRL      0x60
#define AW8898_REG_BSTCTRL1     0x61
#define AW8898_REG_BSTCTRL2     0x62
#define AW8898_REG_PLLCTRL1     0x63
#define AW8898_REG_PLLCTRL2     0x64
#define AW8898_REG_TESTCTRL     0x65
#define AW8898_REG_AMPDBG1      0x66
#define AW8898_REG_AMPDBG2      0x67
#define AW8898_REG_BSTDBG1      0x68
#define AW8898_REG_CDACTRL1     0x69
#define AW8898_REG_CDACTRL2     0x6A
#define AW8898_REG_TESTCTRL2    0x6B

#define AW8898_REG_MAX          0x6F

/********************************************
 * Register Access
 *******************************************/
#define REG_NONE_ACCESS 0
#define REG_RD_ACCESS  1 << 0
#define REG_WR_ACCESS  1 << 1

const unsigned char aw8898_reg_access[AW8898_REG_MAX]={
  [AW8898_REG_ID        ]= REG_RD_ACCESS,
  [AW8898_REG_SYSST     ]= REG_RD_ACCESS,
  [AW8898_REG_SYSINT    ]= REG_RD_ACCESS,
  [AW8898_REG_SYSINTM   ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_SYSCTRL   ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_I2SCTRL   ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_I2STXCFG  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_PWMCTRL   ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_HAGCCFG1  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_HAGCCFG2  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_HAGCCFG3  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_HAGCCFG4  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_HAGCCFG5  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_HAGCCFG6  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_HAGCCFG7  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_HAGCST    ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_DBGCTRL   ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_I2SCFG    ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_I2SSTAT   ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_I2SCAPCNT ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_GENCTRL   ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_BSTCTRL1  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_BSTCTRL2  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_PLLCTRL1  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_PLLCTRL2  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_TESTCTRL  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_AMPDBG1   ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_AMPDBG2   ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_BSTDBG1   ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_CDACTRL1  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_CDACTRL2  ]= REG_RD_ACCESS|REG_WR_ACCESS,
  [AW8898_REG_TESTCTRL2 ]= REG_RD_ACCESS|REG_WR_ACCESS,
};

/******************************************************
 * Register Detail
 *****************************************************/
// SYSST
#define AW8898_BIT_SYSST_UVLOS                      ( 1<<14)
#define AW8898_BIT_SYSST_ADPS                       ( 1<<13)
#define AW8898_BIT_SYSST_DSPS                       ( 1<<12)
#define AW8898_BIT_SYSST_BSTOCS                     ( 1<<11)
#define AW8898_BIT_SYSST_OVPS                       ( 1<<10)
#define AW8898_BIT_SYSST_BSTS                       ( 1<< 9)
#define AW8898_BIT_SYSST_SWS                        ( 1<< 8)
#define AW8898_BIT_SYSST_CLIPS                      ( 1<< 7)
#define AW8898_BIT_SYSST_WDS                        ( 1<< 6)
#define AW8898_BIT_SYSST_NOCLKS                     ( 1<< 5)
#define AW8898_BIT_SYSST_CLKS                       ( 1<< 4)
#define AW8898_BIT_SYSST_OCDS                       ( 1<< 3)
#define AW8898_BIT_SYSST_OTLS                       ( 1<< 2)
#define AW8898_BIT_SYSST_OTHS                       ( 1<< 1)
#define AW8898_BIT_SYSST_PLLS                       ( 1<< 0)

// SYSINT
#define AW8898_BIT_SYSINT_UVLOI                     ( 1<<14)
#define AW8898_BIT_SYSINT_ADPI                      ( 1<<13)
#define AW8898_BIT_SYSINT_DSPI                      ( 1<<12)
#define AW8898_BIT_SYSINT_BSTOCI                    ( 1<<11)
#define AW8898_BIT_SYSINT_OVPI                      ( 1<<10)
#define AW8898_BIT_SYSINT_BSTI                      ( 1<< 9)
#define AW8898_BIT_SYSINT_SWI                       ( 1<< 8)
#define AW8898_BIT_SYSINT_CLIPI                     ( 1<< 7)
#define AW8898_BIT_SYSINT_WDI                       ( 1<< 6)
#define AW8898_BIT_SYSINT_NOCLKI                    ( 1<< 5)
#define AW8898_BIT_SYSINT_CLKI                      ( 1<< 4)
#define AW8898_BIT_SYSINT_OCDI                      ( 1<< 3)
#define AW8898_BIT_SYSINT_OTLI                      ( 1<< 2)
#define AW8898_BIT_SYSINT_OTHI                      ( 1<< 1)
#define AW8898_BIT_SYSINT_PLLI                      ( 1<< 0)

// SYSINTM
#define AW8898_BIT_SYSINTM_UVLOM                    ( 1<<14)
#define AW8898_BIT_SYSINTM_ADPM                     ( 1<<13)
#define AW8898_BIT_SYSINTM_DSPM                     ( 1<<12)
#define AW8898_BIT_SYSINTM_BSTOCM                   ( 1<<11)
#define AW8898_BIT_SYSINTM_OVPM                     ( 1<<10)
#define AW8898_BIT_SYSINTM_BSTM                     ( 1<< 9)
#define AW8898_BIT_SYSINTM_SWM                      ( 1<< 8)
#define AW8898_BIT_SYSINTM_CLIPM                    ( 1<< 7)
#define AW8898_BIT_SYSINTM_WDM                      ( 1<< 6)
#define AW8898_BIT_SYSINTM_NOCLKM                   ( 1<< 5)
#define AW8898_BIT_SYSINTM_CLKM                     ( 1<< 4)
#define AW8898_BIT_SYSINTM_OCDM                     ( 1<< 3)
#define AW8898_BIT_SYSINTM_OTLM                     ( 1<< 2)
#define AW8898_BIT_SYSINTM_OTHM                     ( 1<< 1)
#define AW8898_BIT_SYSINTM_PLLM                     ( 1<< 0)

// SYSCTRL
#define AW8898_BIT_SYSCTRL_INTMODE_MASK             (~( 3<< 8))
#define AW8898_BIT_SYSCTRL_INT_HIGH_PP              ( 3<< 8)
#define AW8898_BIT_SYSCTRL_INT_LOW_PP               ( 2<< 8)
#define AW8898_BIT_SYSCTRL_INT_HIGH_OD              ( 1<< 8)
#define AW8898_BIT_SYSCTRL_INT_LOW_OD               ( 0<< 8)
#define AW8898_BIT_SYSCTRL_MODE_MASK                (~( 1<< 7))
#define AW8898_BIT_SYSCTRL_RCV_MODE                 ( 1<< 7)
#define AW8898_BIT_SYSCTRL_SPK_MODE                 ( 0<< 7)
#define AW8898_BIT_SYSCTRL_I2SEN_MASK               (~( 1<< 6))
#define AW8898_BIT_SYSCTRL_I2S_ENABLE               ( 1<< 6)
#define AW8898_BIT_SYSCTRL_I2S_DISABLE              ( 0<< 6)
#define AW8898_BIT_SYSCTRL_WSINV_MASK               (~( 1<< 5))
#define AW8898_BIT_SYSCTRL_WS_INVERT                ( 1<< 5)
#define AW8898_BIT_SYSCTRL_WS_NO_INVERT             ( 0<< 5)
#define AW8898_BIT_SYSCTRL_BCKINV_MASK              (~( 1<< 4))
#define AW8898_BIT_SYSCTRL_BCK_INVERT               ( 1<< 4)
#define AW8898_BIT_SYSCTRL_BCK_NO_INVERT            ( 0<< 4)
#define AW8898_BIT_SYSCTRL_IPLL_MASK                (~( 1<< 3))
#define AW8898_BIT_SYSCTRL_PLL_WORD                 ( 1<< 3)
#define AW8898_BIT_SYSCTRL_PLL_BIT                  ( 0<< 3)
#define AW8898_BIT_SYSCTRL_DSPBY_MASK               (~( 1<< 2))
#define AW8898_BIT_SYSCTRL_DSP_BYPASS               ( 1<< 2)
#define AW8898_BIT_SYSCTRL_DSP_WORK                 ( 0<< 2)
#define AW8898_BIT_SYSCTRL_CP_MASK                  (~( 1<< 1))
#define AW8898_BIT_SYSCTRL_CP_PDN                   ( 1<< 1)
#define AW8898_BIT_SYSCTRL_CP_ACTIVE                ( 0<< 1)
#define AW8898_BIT_SYSCTRL_PW_MASK                  (~( 1<< 0))
#define AW8898_BIT_SYSCTRL_PW_PDN                   ( 1<< 0)
#define AW8898_BIT_SYSCTRL_PW_ACTIVE                ( 0<< 0)

// I2SCTRL
#define AW8898_BIT_I2SCTRL_INPLEV_MASK              (~( 1<<13))
#define AW8898_BIT_I2SCTRL_INPLEV_0DB               ( 1<<13)
#define AW8898_BIT_I2SCTRL_INPLEV_NEG_6DB           ( 0<<13)
#define AW8898_BIT_I2SCTRL_STEREO_MASK              (~( 1<<12))
#define AW8898_BIT_I2SCTRL_STEREO_ENABLE            ( 1<<12)
#define AW8898_BIT_I2SCTRL_STEREO_DISABLE           ( 0<<12)
#define AW8898_BIT_I2SCTRL_CHS_MASK                 (~( 3<<10))
#define AW8898_BIT_I2SCTRL_CHS_MONO                 ( 3<<10)
#define AW8898_BIT_I2SCTRL_CHS_RIGHT                ( 2<<10)
#define AW8898_BIT_I2SCTRL_CHS_LEFT                 ( 1<<10)
#define AW8898_BIT_I2SCTRL_MD_MASK                  (~( 3<< 8))
#define AW8898_BIT_I2SCTRL_MD_LSB                   ( 2<< 8)
#define AW8898_BIT_I2SCTRL_MD_MSB                   ( 1<< 8)
#define AW8898_BIT_I2SCTRL_MD_STD                   ( 0<< 8)
#define AW8898_BIT_I2SCTRL_FMS_MASK                 (~( 3<< 6))
#define AW8898_BIT_I2SCTRL_FMS_32BIT                ( 3<< 6)
#define AW8898_BIT_I2SCTRL_FMS_24BIT                ( 2<< 6)
#define AW8898_BIT_I2SCTRL_FMS_20BIT                ( 1<< 6)
#define AW8898_BIT_I2SCTRL_FMS_16BIT                ( 0<< 6)
#define AW8898_BIT_I2SCTRL_BCK_MASK                 (~( 3<< 4))
#define AW8898_BIT_I2SCTRL_BCK_64FS                 ( 2<< 4)
#define AW8898_BIT_I2SCTRL_BCK_48FS                 ( 1<< 4)
#define AW8898_BIT_I2SCTRL_BCK_32FS                 ( 0<< 4)
#define AW8898_BIT_I2SCTRL_SR_MASK                  (~(15<< 0))
#define AW8898_BIT_I2SCTRL_SR_192K                  (10<< 0)
#define AW8898_BIT_I2SCTRL_SR_96K                   ( 9<< 0)
#define AW8898_BIT_I2SCTRL_SR_48K                   ( 8<< 0)
#define AW8898_BIT_I2SCTRL_SR_44P1K                 ( 7<< 0)
#define AW8898_BIT_I2SCTRL_SR_32K                   ( 6<< 0)
#define AW8898_BIT_I2SCTRL_SR_24K                   ( 5<< 0)
#define AW8898_BIT_I2SCTRL_SR_22K                   ( 4<< 0)
#define AW8898_BIT_I2SCTRL_SR_16K                   ( 3<< 0)
#define AW8898_BIT_I2SCTRL_SR_12K                   ( 2<< 0)
#define AW8898_BIT_I2SCTRL_SR_11K                   ( 1<< 0)
#define AW8898_BIT_I2SCTRL_SR_8K                    ( 0<< 0)


// I2STXCFG
#define AW8898_BIT_I2STXCFG_FSYNC_MASK              (~( 1<<15))
#define AW8898_BIT_I2STXCFG_FSYNC_BCK_CYCLE         ( 1<<15)
#define AW8898_BIT_I2STXCFG_FSYNC_ONE_SLOT          ( 0<<15)
#define AW8898_BIT_I2STXCFG_SLOT_NUM_MASK           (~( 1<<14))
#define AW8898_BIT_I2STXCFG_SLOT_NUM_4_TIMES        ( 1<<14)
#define AW8898_BIT_I2STXCFG_SLOT_NUM_2_TIMES        ( 0<<14)
#define AW8898_BIT_I2STXCFG_TX_SLOT_VLD_MASK        (~(15<<12))
#define AW8898_BIT_I2STXCFG_TX_SLOT_VLD_3           ( 3<<12)
#define AW8898_BIT_I2STXCFG_TX_SLOT_VLD_2           ( 2<<12)
#define AW8898_BIT_I2STXCFG_TX_SLOT_VLD_1           ( 1<<12)
#define AW8898_BIT_I2STXCFG_TX_SLOT_VLD_0           ( 0<<12)
#define AW8898_BIT_I2STXCFG_RX_SLOT_VLD_MASK        (~(15<< 8))
#define AW8898_BIT_I2STXCFG_RX_SLOT_VLD_3_2         (12<< 8)
#define AW8898_BIT_I2STXCFG_RX_SLOT_VLD_3_1         (10<< 8)
#define AW8898_BIT_I2STXCFG_RX_SLOT_VLD_3_0         ( 9<< 8)
#define AW8898_BIT_I2STXCFG_RX_SLOT_VLD_2_1         ( 6<< 8)
#define AW8898_BIT_I2STXCFG_RX_SLOT_VLD_2_0         ( 5<< 8)
#define AW8898_BIT_I2STXCFG_RX_SLOT_VLD_1_0         ( 3<< 8)
#define AW8898_BIT_I2STXCFG_RX_SLOT_VLD_3           ( 8<< 8)
#define AW8898_BIT_I2STXCFG_RX_SLOT_VLD_2           ( 4<< 8)
#define AW8898_BIT_I2STXCFG_RX_SLOT_VLD_1           ( 2<< 8)
#define AW8898_BIT_I2STXCFG_RX_SLOT_VLD_0           ( 1<< 8)
#define AW8898_BIT_I2STXCFG_DRVSTREN_MASK           (~( 1<< 5))
#define AW8898_BIT_I2STXCFG_DRVSTREN_8MA            ( 1<< 5)
#define AW8898_BIT_I2STXCFG_DRVSTREN_2MA            ( 0<< 5)
#define AW8898_BIT_I2STXCFG_DOHZ_MASK               (~( 1<< 4))
#define AW8898_BIT_I2STXCFG_DOHZ_HIZ                ( 1<< 4)
#define AW8898_BIT_I2STXCFG_DOHZ_GND                ( 0<< 4)
#define AW8898_BIT_I2STXCFG_DSEL_MASK               (~( 3<< 2))
#define AW8898_BIT_I2STXCFG_DSEL_DSP                ( 2<< 2)
#define AW8898_BIT_I2STXCFG_DSEL_GAIN               ( 1<< 2)
#define AW8898_BIT_I2STXCFG_DSEL_ZERO               ( 0<< 2)
#define AW8898_BIT_I2STXCFG_CHS_MASK                (~( 1<< 1))
#define AW8898_BIT_I2STXCFG_CHS_RIGHT               ( 1<< 1)
#define AW8898_BIT_I2STXCFG_CHS_LEFT                ( 0<< 1)
#define AW8898_BIT_I2STXCFG_TX_MASK                 (~( 1<< 0))
#define AW8898_BIT_I2STXCFG_TX_ENABLE               ( 1<< 0)
#define AW8898_BIT_I2STXCFG_TX_DISABLE              ( 0<< 0)

// PWMCTRL
#define AW8898_BIT_PWMCTRL_DSMZTH_MASK              (~(15<<12))
#define AW8898_BIT_PWMCTRL_DSMZTH_UNIT              ( 1<<12)
#define AW8898_BIT_PWMCTRL_PWMDELA_MASK             (~(15<< 8))
#define AW8898_BIT_PWMCTRL_PWMDELA_UNIT             ( 1<< 8)
#define AW8898_BIT_PWMCTRL_PWMDELB_MASK             (~(15<< 4))
#define AW8898_BIT_PWMCTRL_PWMDELB_UNIT             ( 1<< 4)
#define AW8898_BIT_PWMCTRL_PWMSH_MASK               (~( 1<< 3))
#define AW8898_BIT_PWMCTRL_PWMSH_TRIANGLE           ( 1<< 3)
#define AW8898_BIT_PWMCTRL_PWMSH_SAWTOOTH           ( 0<< 3)
#define AW8898_BIT_PWMCTRL_PWMRES_MASK              (~( 1<< 2))
#define AW8898_BIT_PWMCTRL_PWMRES_8BIT              ( 1<< 2)
#define AW8898_BIT_PWMCTRL_PWMRES_7BIT              ( 0<< 2)
#define AW8898_BIT_PWMCTRL_HDCCE_MASK               (~( 1<< 1))
#define AW8898_BIT_PWMCTRL_HDCCE_ENABLE             ( 1<< 1)
#define AW8898_BIT_PWMCTRL_HDCCE_DISABLE            ( 0<< 1)
#define AW8898_BIT_PWMCTRL_HMUTE_MASK               (~( 1<< 0))
#define AW8898_BIT_PWMCTRL_HMUTE_ENABLE             ( 1<< 0)
#define AW8898_BIT_PWMCTRL_HMUTE_DISABLE            ( 0<< 0)

// HAGCCFG1
#define AW8898_BIT_HAGCCFG1_RVTH_MASK               (~(255<<8))
#define AW8898_BIT_HAGCCFG1_RVTH_UNIT               ( 1<< 8)
#define AW8898_BIT_HAGCCFG1_AVTH_MASK               (~(255<<0))
#define AW8898_BIT_HAGCCFG1_AVTH_UNIT               ( 1<< 0)

// HAGCCFG2
#define AW8898_BIT_HAGCCFG2_ATTH_UNIT               ( 1<< 0)

// HAGCCFG3
#define AW8898_BIT_HAGCCFG3_RTTH_UNIT               ( 1<< 0)

// HAGCCFG4
#define AW8898_BIT_HAGCCFG4_MPD_MASK                (~( 1<<14))
#define AW8898_BIT_HAGCCFG4_MPD_ENABLE              ( 1<<14)
#define AW8898_BIT_HAGCCFG4_MPD_DISABLE             ( 0<<14)
#define AW8898_BIT_HAGCCFG4_MPD_TTH_MASK            (~( 3<<12))
#define AW8898_BIT_HAGCCFG4_MPD_TTH_0P047           ( 3<<12)
#define AW8898_BIT_HAGCCFG4_MPD_TTH_0P032           ( 2<<12)
#define AW8898_BIT_HAGCCFG4_MPD_TTH_0P016           ( 1<<12)
#define AW8898_BIT_HAGCCFG4_MPD_TTH_0P008           ( 0<<12)
#define AW8898_BIT_HAGCCFG4_MPD_RTH_MASK            (~( 3<<10))
#define AW8898_BIT_HAGCCFG4_MPD_RTH_0P047           ( 3<<10)
#define AW8898_BIT_HAGCCFG4_MPD_RTH_0P032           ( 2<<10)
#define AW8898_BIT_HAGCCFG4_MPD_RTH_0P016           ( 1<<10)
#define AW8898_BIT_HAGCCFG4_MPD_RTH_0P008           ( 0<<10)
#define AW8898_BIT_HAGCCFG4_MPD_ATH_MASK            (~( 3<< 8))
#define AW8898_BIT_HAGCCFG4_MPD_ATH_0P047           ( 3<< 8)
#define AW8898_BIT_HAGCCFG4_MPD_ATH_0P032           ( 2<< 8)
#define AW8898_BIT_HAGCCFG4_MPD_ATH_0P016           ( 1<< 8)
#define AW8898_BIT_HAGCCFG4_MPD_ATH_0P008           ( 0<< 8)
#define AW8898_BIT_HAGCCFG4_HOLDTH_MASK             (~(255<< 0))

// HAGCCFG5

// HAGCCFG6

// HAGCCFG7
#define AW8898_BIT_HAGCCFG7_VOL_MASK                (~(255< 8))
#define AW8898_VOLUME_MAX                           (0)
#define AW8898_VOLUME_MIN                           (-255)
#define AW8898_VOL_REG_SHIFT                        (8)

// HAGCST
#define AW8898_BIT_BSTVOUT_ST_10P25V                (15<< 0)
#define AW8898_BIT_BSTVOUT_ST_10V                   (14<< 0)
#define AW8898_BIT_BSTVOUT_ST_9P75V                 (13<< 0)
#define AW8898_BIT_BSTVOUT_ST_9P5V                  (12<< 0)
#define AW8898_BIT_BSTVOUT_ST_9P25V                 (11<< 0)
#define AW8898_BIT_BSTVOUT_ST_9V                    (10<< 0)
#define AW8898_BIT_BSTVOUT_ST_8P75V                 ( 9<< 0)
#define AW8898_BIT_BSTVOUT_ST_8P5V                  ( 8<< 0)
#define AW8898_BIT_BSTVOUT_ST_8P25V                 ( 7<< 0)
#define AW8898_BIT_BSTVOUT_ST_8V                    ( 6<< 0)
#define AW8898_BIT_BSTVOUT_ST_7P75V                 ( 5<< 0)
#define AW8898_BIT_BSTVOUT_ST_7P5V                  ( 4<< 0)
#define AW8898_BIT_BSTVOUT_ST_7P25V                 ( 3<< 0)
#define AW8898_BIT_BSTVOUT_ST_7V                    ( 2<< 0)
#define AW8898_BIT_BSTVOUT_ST_6P75V                 ( 1<< 0)
#define AW8898_BIT_BSTVOUT_ST_6P5V                  ( 0<< 0)

// DBGCTRL
#define AW8898_BIT_DBGCTRL_LPBK_FAR_MASK            (~( 1<<15))
#define AW8898_BIT_DBGCTRL_LPBK_FAR_ENABLE          ( 1<<15)
#define AW8898_BIT_DBGCTRL_LPBK_FAR_DISABLE         ( 0<<15)
#define AW8898_BIT_DBGCTRL_LPBK_NEAR_MASK           (~( 1<<14))
#define AW8898_BIT_DBGCTRL_LPBK_NEAR_ENABLE         ( 1<<14)
#define AW8898_BIT_DBGCTRL_LPBK_NEAR_DISABLE        ( 0<<14)
#define AW8898_BIT_DBGCTRL_PDUVL_MASK               (~( 1<<13))
#define AW8898_BIT_DBGCTRL_PDUVL_DISABLE            ( 1<<13)
#define AW8898_BIT_DBGCTRL_PDUVL_ENABLE             ( 0<<13)
#define AW8898_BIT_DBGCTRL_MUTE_MASK                (~( 1<<12))
#define AW8898_BIT_DBGCTRL_MUTE_NO_AUTO             ( 1<<12)
#define AW8898_BIT_DBGCTRL_MUTE_AUTO                ( 0<<12)
#define AW8898_BIT_DBGCTRL_NOCLK_RESET_MASK         (~( 1<<11))
#define AW8898_BIT_DBGCTRL_NOCLK_NO_RESET           ( 1<<11)
#define AW8898_BIT_DBGCTRL_NOCLK_RESET              ( 0<<11)
#define AW8898_BIT_DBGCTRL_PLL_UNLOCK_RESET_MASK    (~( 1<<10))
#define AW8898_BIT_DBGCTRL_PLL_UNLOCK_NO_RESET      ( 1<<10)
#define AW8898_BIT_DBGCTRL_PLL_UNLOCK_RESET         ( 0<<10)
#define AW8898_BIT_DBGCTRL_CLKMD_MASK               (~( 1<< 9))
#define AW8898_BIT_DBGCTRL_CLKMD_HALF               ( 1<< 9)
#define AW8898_BIT_DBGCTRL_CLKMD_NORMAL             ( 0<< 9)
#define AW8898_BIT_DBGCTRL_OSCPD_MASK               (~( 1<< 8))
#define AW8898_BIT_DBGCTRL_OSCPD_ENABLE             ( 1<< 8)
#define AW8898_BIT_DBGCTRL_OSCPD_DISABLE            ( 0<< 8)
#define AW8898_BIT_DBGCTRL_AMPPD_MASK               (~( 1<< 7))
#define AW8898_BIT_DBGCTRL_AMPPD_PDN                ( 1<< 7)
#define AW8898_BIT_DBGCTRL_AMPPD_ACTIVE             ( 0<< 7)
#define AW8898_BIT_DBGCTRL_PLLPD_MASK               (~( 1<< 6))
#define AW8898_BIT_DBGCTRL_PLLPD_PDN                ( 1<< 6)
#define AW8898_BIT_DBGCTRL_PLLPD_ACTIVE             ( 0<< 6)
#define AW8898_BIT_DBGCTRL_I2SRST_MASK              (~( 1<< 5))
#define AW8898_BIT_DBGCTRL_I2SRST_RESET             ( 1<< 5)
#define AW8898_BIT_DBGCTRL_I2SRST_WORK              ( 0<< 5)
#define AW8898_BIT_DBGCTRL_SYSRST_MASK              (~( 1<< 4))
#define AW8898_BIT_DBGCTRL_SYSRST_RESET             ( 1<< 4)
#define AW8898_BIT_DBGCTRL_SYSRST_WORK              ( 0<< 4)
#define AW8898_BIT_DBGCTRL_SYSCE_MASK               (~( 1<< 0))
#define AW8898_BIT_DBGCTRL_SYSCE_ENABLE             ( 1<< 0)
#define AW8898_BIT_DBGCTRL_SYSCE_DISABLE            ( 0<< 0)


// I2SCFG
#define AW8898_BIT_I2SCFG_I2SRX_MASK                (~( 1<< 0))
#define AW8898_BIT_I2SCFG_I2SRX_ENABLE              ( 1<< 0)
#define AW8898_BIT_I2SCFG_I2SRX_DISABLE             ( 0<< 0)

// I2SSAT
#define AW8898_BIT_I2SSAT_DPSTAT                    ( 1<< 2)
#define AW8898_BIT_I2SSAT_I2SROVS                   ( 1<< 1)
#define AW8898_BIT_I2SSAT_I2STOVS                   ( 1<< 0)

// GENCTRL
#define AW8898_BIT_GENCTRL_BURST_PEAK_MASK          (~( 3<<14))
#define AW8898_BIT_GENCTRL_BURST_PEAK_200MA         ( 3<<14)
#define AW8898_BIT_GENCTRL_BURST_PEAK_160MA         ( 2<<14)
#define AW8898_BIT_GENCTRL_BURST_PEAK_100MA         ( 1<<14)
#define AW8898_BIT_GENCTRL_BURST_PEAK_130MA         ( 0<<14)
#define AW8898_BIT_GENCTRL_BST_TDEG2_MASK           (~( 7<< 9))
#define AW8898_BIT_GENCTRL_BST_TDEG2_2P7S           ( 7<< 9)
#define AW8898_BIT_GENCTRL_BST_TDEG2_1P3S           ( 6<< 9)
#define AW8898_BIT_GENCTRL_BST_TDEG2_672MS          ( 5<< 9)
#define AW8898_BIT_GENCTRL_BST_TDEG2_336MS          ( 4<< 9)
#define AW8898_BIT_GENCTRL_BST_TDEG2_168MS          ( 3<< 9)
#define AW8898_BIT_GENCTRL_BST_TDEG2_84MS           ( 2<< 9)
#define AW8898_BIT_GENCTRL_BST_TDEG2_42MS           ( 1<< 9)
#define AW8898_BIT_GENCTRL_BST_TDEG2_21MS           ( 0<< 9)
#define AW8898_BIT_GENCTRL_BST_OCAP_MASK            (~( 1<< 8))
#define AW8898_BIT_GENCTRL_BST_OCAP_SLOW            ( 1<< 8)
#define AW8898_BIT_GENCTRL_BST_OCAP_FAST            ( 0<< 8)
#define AW8898_BIT_GENCTRL_BST_EN_MASK              (~( 1<< 7))
#define AW8898_BIT_GENCTRL_BST_ENABLE               ( 1<< 7)
#define AW8898_BIT_GENCTRL_BST_DISABLE              ( 0<< 7)
#define AW8898_BIT_GENCTRL_BST_ILIMIT_MASK          (~( 7<< 4))
#define AW8898_BIT_GENCTRL_BST_ILIMIT_4P5A          ( 7<< 4)
#define AW8898_BIT_GENCTRL_BST_ILIMIT_4P25A         ( 6<< 4)
#define AW8898_BIT_GENCTRL_BST_ILIMIT_4A            ( 5<< 4)
#define AW8898_BIT_GENCTRL_BST_ILIMIT_3P75A         ( 4<< 4)
#define AW8898_BIT_GENCTRL_BST_ILIMIT_3P5A          ( 3<< 4)
#define AW8898_BIT_GENCTRL_BST_ILIMIT_3P25A         ( 2<< 4)
#define AW8898_BIT_GENCTRL_BST_ILIMIT_3A            ( 1<< 4)
#define AW8898_BIT_GENCTRL_BST_ILIMIT_2P75A         ( 0<< 4)
#define AW8898_BIT_GENCTRL_BST_VOUT_MASK            (~(15<< 0))
#define AW8898_BIT_GENCTRL_BST_VOUT_10P25V          (15<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_10V             (14<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_9P75V           (13<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_9P5V            (12<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_9P25V           (11<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_9V              (10<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_8P75V           ( 9<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_8P5V            ( 8<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_8P25V           ( 7<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_8V              ( 6<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_7P75V           ( 5<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_7P5V            ( 4<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_7P25V           ( 3<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_7V              ( 2<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_6P75V           ( 1<< 0)
#define AW8898_BIT_GENCTRL_BST_VOUT_6P5V            ( 0<< 0)

// BSTCTRL1
#define AW8898_BIT_BSTCTRL1_RTH_MASK                (~(64<< 8))
#define AW8898_BIT_BSTCTRL1_ATH_MASK                (~(64<< 0))

// BSTCTRL2
#define AW8898_BIT_BST_MODE_MASK                    (~( 7<< 3))
#define AW8898_BIT_BST_MODE_SMART_BOOST             ( 6<< 3)
#define AW8898_BIT_BST_MODE_ADAPT_BOOST             ( 5<< 3)
#define AW8898_BIT_BST_MODE_FORCE_BOOST             ( 1<< 3)
#define AW8898_BIT_BST_MODE_TRANSP_BOOST            ( 0<< 3)
#define AW8898_BIT_BST_TDEG_MASK                    (~( 7<< 0))
#define AW8898_BIT_BST_TDEG_2P7S                    ( 7<< 0)
#define AW8898_BIT_BST_TDEG_1P3S                    ( 6<< 0)
#define AW8898_BIT_BST_TDEG_672MS                   ( 5<< 0)
#define AW8898_BIT_BST_TDEG_336MS                   ( 4<< 0)
#define AW8898_BIT_BST_TDEG_168MS                   ( 3<< 0)
#define AW8898_BIT_BST_TDEG_84MS                    ( 2<< 0)
#define AW8898_BIT_BST_TDEG_42MS                    ( 1<< 0)
#define AW8898_BIT_BST_TDEG_21MS                    ( 0<< 0)

#endif
