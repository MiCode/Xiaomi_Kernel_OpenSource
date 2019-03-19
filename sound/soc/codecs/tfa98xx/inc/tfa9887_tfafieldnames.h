/** Filename: Tfa9887_TfaFieldnames.h
 *  This file was generated automatically on 04/14/15 at 10:23:40.
 *  Source file: TFA9897N1B_I2C_list_URT_source_v34_87Only.xls
 */
#define TFA9887_I2CVERSION		34
#define TFA9895_I2CVERSION		34
#define TFA9887_NAMETABLE static tfaBfName_t Tfa9887DatasheetNames[]= {\
   { 0x402, "I2SF"},    /* I2SFormat data 1 input:                           , */\
   { 0x431, "CHS12"},    /* ChannelSelection data1 input  (In CoolFlux)       , */\
   { 0x450, "CHS3"},    /* ChannelSelection data 2 input (coolflux input, the DCDC converter gets the other signal), */\
   { 0x461, "CHSA"},    /* Input selection for amplifier                     , */\
   { 0x4b0, "I2SDOE"},    /* Enable data output                                , */\
   { 0x4c3, "I2SSR"},    /* sample rate setting                               , */\
   { 0x500, "BSSBY"},    /*                                                   , */\
   { 0x511, "BSSCR"},    /* 00 = 0.56 dB/Sample                               , */\
   { 0x532, "BSST"},    /* 000 = 2.92V                                       , */\
   { 0x5f0, "I2SDOC"},    /* selection data out                                , */\
   { 0xa02, "DOLS"},    /* Output selection dataout left channel             , */\
   { 0xa32, "DORS"},    /* Output selection dataout right channel            , */\
   { 0xa62, "SPKL"},    /* Selection speaker induction                       , */\
   { 0xa91, "SPKR"},    /* Selection speaker impedance                       , */\
   { 0xab3, "DCFG"},    /* DCDC speaker current compensation gain            , */\
   { 0x4134, "PWMDEL"},    /* PWM DelayBits to set the delay                    , */\
   { 0x4180, "PWMSH"},    /* PWM Shape                                         , */\
   { 0x4190, "PWMRE"},    /* PWM Bitlength in noise shaper                     , */\
   { 0x48e1, "TCC"},    /* sample & hold track time:                         , */\
   { 0xffff, "Unknown bitfield enum" }   /* not found */\
};

#define TFA9887_BITNAMETABLE static tfaBfName_t Tfa9887BitNames[]= {\
   { 0x402, "i2s_seti"},    /* I2SFormat data 1 input:                           , */\
   { 0x431, "chan_sel1"},    /* ChannelSelection data1 input  (In CoolFlux)       , */\
   { 0x450, "lr_sw_i2si2"},    /* ChannelSelection data 2 input (coolflux input, the DCDC converter gets the other signal), */\
   { 0x461, "input_sel"},    /* Input selection for amplifier                     , */\
   { 0x4b0, "enbl_datao"},    /* Enable data output                                , */\
   { 0x4c3, "i2s_fs"},    /* sample rate setting                               , */\
   { 0x500, "bypass_clipper"},    /*                                                   , */\
   { 0x511, "vbat_prot_attacktime[1:0]"},    /* 00 = 0.56 dB/Sample                               , */\
   { 0x532, "vbat_prot_thlevel[2:0]"},    /* 000 = 2.92V                                       , */\
   { 0x5d0, "reset_min_vbat"},    /* to reset the clipper via I2C in case the CF is bypassed, */\
   { 0x5f0, "datao_sel"},    /* selection data out                                , */\
   { 0xa02, "sel_i2so_l"},    /* Output selection dataout left channel             , */\
   { 0xa32, "sel_i2so_r"},    /* Output selection dataout right channel            , */\
   { 0xa62, "ctrl_spkr_coil"},    /* Selection speaker induction                       , */\
   { 0xa91, "ctrl_spr_res"},    /* Selection speaker impedance                       , */\
   { 0xab3, "ctrl_dcdc_spkr_i_comp_gain"},    /* DCDC speaker current compensation gain            , */\
   { 0xaf0, "ctrl_dcdc_spkr_i_comp_sign"},    /* DCDC speaker current compensation sign            , */\
   { 0x4100, "bypass_hp"},    /* bypass_hp, to bypass the hp filter byhind the CoolFlux, */\
   { 0x4110, "hard_mute"},    /* hard mute setting in HW                           , */\
   { 0x4120, "soft_mute"},    /* Soft mute setting in HW                           , */\
   { 0x4134, "PWM_Delay[4:0]"},    /* PWM DelayBits to set the delay                    , */\
   { 0x4180, "PWM_Shape"},    /* PWM Shape                                         , */\
   { 0x4190, "PWM_BitLength"},    /* PWM Bitlength in noise shaper                     , */\
   { 0x4800, "ctrl_negin"},    /*                                                   , */\
   { 0x4810, "ctrl_cs_sein"},    /*                                                   , */\
   { 0x4820, "ctrl_coincidencecs"},    /* HIGH => Prevent dcdc switching during clk_cs_clksh, */\
   { 0x4876, "delay_se_neg[6:0]"},    /* delayshiftse2                                     , */\
   { 0x48e1, "ctrl_cs_ttrack[1:0]"},    /* sample & hold track time:                         , */\
   { 0xffff, "Unknown bitfield enum" }    /* not found */\
};

