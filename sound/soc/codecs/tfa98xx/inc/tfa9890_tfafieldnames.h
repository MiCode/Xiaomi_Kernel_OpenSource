/** Filename: Tfa9890_TfaFieldnames.h
 *  This file was generated automatically on 04/07/15 at 14:46:37.
 *  Source file: TFA9897N1B_I2C_list_URT_source_v34_90Only.xls
 */
#define TFA9890_I2CVERSION    34
#define TFA9890_NAMETABLE static tfaBfName_t Tfa9890DatasheetNames[]= {\
   { 0x402, "I2SF"},    /* I2SFormat data 1 input:                           , */\
   { 0x431, "CHS12"},    /* ChannelSelection data1 input  (In CoolFlux)       , */\
   { 0x450, "CHS3"},    /* ChannelSelection data 2 input (coolflux input, the DCDC converter gets the other signal), */\
   { 0x461, "CHSA"},    /* Input selection for amplifier                     , */\
   { 0x481, "I2SDOC"},    /* selection data out                                , */\
   { 0x4a0, "DISP"},    /* idp protection                                    , */\
   { 0x4b0, "I2SDOE"},    /* Enable data output                                , */\
   { 0x4c3, "I2SSR"},    /* sample rate setting                               , */\
   { 0x732, "DCMCC"},   /* Max boost coil current - step of 500 mA           , */\
   { 0x9c0, "CCFD"},    /* Selection CoolFlux Clock                          , */\
   { 0x9d0, "ISEL"},    /* selection input 1 or 2                            , */\
   { 0xa02, "DOLS"},    /* Output selection dataout left channel             , */\
   { 0xa32, "DORS"},    /* Output selection dataout right channel            , */\
   { 0xa62, "SPKL"},    /* Selection speaker induction                       , */\
   { 0xa91, "SPKR"},    /* Selection speaker impedance                       , */\
   { 0xab3, "DCFG"},    /* DCDC speaker current compensation gain            , */\
   { 0xf00, "VDDD"},    /* mask flag_por for interupt generation             , */\
   { 0xf10, "OTDD"},    /* mask flag_otpok for interupt generation           , */\
   { 0xf20, "OVDD"},    /* mask flag_ovpok for interupt generation           , */\
   { 0xf30, "UVDD"},    /* mask flag_uvpok for interupt generation           , */\
   { 0xf40, "OCDD"},    /* mask flag_ocp_alarm for interupt generation       , */\
   { 0xf50, "CLKD"},    /* mask flag_clocks_stable for interupt generation   , */\
   { 0xf60, "DCCD"},    /* mask flag_pwrokbst for interupt generation        , */\
   { 0xf70, "SPKD"},    /* mask flag_cf_speakererror for interupt generation , */\
   { 0xf80, "WDD"},    /* mask flag_watchdog_reset for interupt generation  , */\
   { 0xf90, "LCLK"},    /* mask flag_lost_clk for interupt generation        , */\
   { 0xfe0, "INT"},    /* enabling interrupt                                , */\
   { 0xff0, "INTP"},    /* Setting polarity interupt                         , */\
   { 0x8f0f, "VERSION"},    /* (key1 protected)                                  , */\
   { 0xffff, "Unknown bitfield enum" }   /* not found */\
};

#define TFA9890_BITNAMETABLE static tfaBfName_t Tfa9890BitNames[]= {\
   { 0x402, "i2s_seti"},    /* I2SFormat data 1 input:                           , */\
   { 0x431, "chan_sel1"},    /* ChannelSelection data1 input  (In CoolFlux)       , */\
   { 0x450, "lr_sw_i2si2"},    /* ChannelSelection data 2 input (coolflux input, the DCDC converter gets the other signal), */\
   { 0x461, "input_sel"},    /* Input selection for amplifier                     , */\
   { 0x481, "datao_sel"},    /* selection data out                                , */\
   { 0x4a0, "disable_idp"},    /* idp protection                                    , */\
   { 0x4b0, "enbl_datao"},    /* Enable data output                                , */\
   { 0x4c3, "i2s_fs"},    /* sample rate setting                               , */\
   { 0x732, "ctrl_bstcur"},   /* Max boost coil current - step of 500 mA           , */\
   { 0x9c0, "sel_cf_clk"},  /* Selection CoolFlux Clock                          , */\
   { 0x9d0, "intf_sel"},    /* selection input 1 or 2                            , */\
   { 0xa02, "sel_i2so_l"},    /* Output selection dataout left channel             , */\
   { 0xa32, "sel_i2so_r"},    /* Output selection dataout right channel            , */\
   { 0xa62, "ctrl_spkr_coil"},    /* Selection speaker induction                       , */\
   { 0xa91, "ctrl_spr_res"},    /* Selection speaker impedance                       , */\
   { 0xab3, "ctrl_dcdc_spkr_i_comp_gain"},    /* DCDC speaker current compensation gain            , */\
   { 0xaf0, "ctrl_dcdc_spkr_i_comp_sign"},    /* DCDC speaker current compensation sign            , */\
   { 0xf00, "flag_por_mask"},    /* mask flag_por for interupt generation             , */\
   { 0xf10, "flag_otpok_mask"},    /* mask flag_otpok for interupt generation           , */\
   { 0xf20, "flag_ovpok_mask"},    /* mask flag_ovpok for interupt generation           , */\
   { 0xf30, "flag_uvpok_mask"},    /* mask flag_uvpok for interupt generation           , */\
   { 0xf40, "flag_ocp_alarm_mask"},    /* mask flag_ocp_alarm for interupt generation       , */\
   { 0xf50, "flag_clocks_stable_mask"},    /* mask flag_clocks_stable for interupt generation   , */\
   { 0xf60, "flag_pwrokbst_mask"},    /* mask flag_pwrokbst for interupt generation        , */\
   { 0xf70, "flag_cf_speakererror_mask"},    /* mask flag_cf_speakererror for interupt generation , */\
   { 0xf80, "flag_watchdog_reset_mask"},    /* mask flag_watchdog_reset for interupt generation  , */\
   { 0xf90, "flag_lost_clk_mask"},    /* mask flag_lost_clk for interupt generation        , */\
   { 0xfe0, "enable_interrupt"},    /* enabling interrupt                                , */\
   { 0xff0, "invert_int_polarity"},    /* Setting polarity interupt                         , */\
   { 0x4700, "switch_fb"},    /* switch_fb                                         , */\
   { 0x4713, "se_hyst"},    /* se_hyst                                           , */\
   { 0x4754, "se_level"},    /* se_level                                          , */\
   { 0x47a5, "ktemp"},    /* temperature compensation trimming                 , */\
   { 0x8f0f, "production_data6"},    /* (key1 protected)                                  , */\
   { 0xffff, "Unknown bitfield enum" }    /* not found */\
};

