/** Filename: tfa9896_tfafieldnames.h
 *  This file was generated automatically on 08/15/16 at 09:43:38.
 *  Source file: TFA9896_N1B1_I2C_regmap_V16.xlsx
 */

#ifndef _TFA9896_TFAFIELDNAMES_H
#define _TFA9896_TFAFIELDNAMES_H


#define TFA9896_I2CVERSION    16

typedef enum nxpTFA9896BfEnumList {
    TFA9896_BF_VDDS  = 0x0000,    /*!< Power-on-reset flag (auto clear by reading)        */
    TFA9896_BF_PLLS  = 0x0010,    /*!< PLL lock to programmed frequency                   */
    TFA9896_BF_OTDS  = 0x0020,    /*!< Over Temperature Protection alarm                  */
    TFA9896_BF_OVDS  = 0x0030,    /*!< Over Voltage Protection alarm                      */
    TFA9896_BF_UVDS  = 0x0040,    /*!< Under Voltage Protection alarm                     */
    TFA9896_BF_OCDS  = 0x0050,    /*!< Over Current Protection alarm                      */
    TFA9896_BF_CLKS  = 0x0060,    /*!< Clocks stable flag                                 */
    TFA9896_BF_CLIPS = 0x0070,    /*!< Amplifier clipping                                 */
    TFA9896_BF_MTPB  = 0x0080,    /*!< MTP busy copying data to/from I2C registers        */
    TFA9896_BF_NOCLK = 0x0090,    /*!< lost clock detection (reference input clock)       */
    TFA9896_BF_SPKS  = 0x00a0,    /*!< Speaker error                                      */
    TFA9896_BF_ACS   = 0x00b0,    /*!< Cold Start required                                */
    TFA9896_BF_SWS   = 0x00c0,    /*!< Amplifier engage (Amp Switching)                   */
    TFA9896_BF_WDS   = 0x00d0,    /*!< watchdog reset (activates reset)                   */
    TFA9896_BF_AMPS  = 0x00e0,    /*!< Amplifier is enabled by manager                    */
    TFA9896_BF_AREFS = 0x00f0,    /*!< References are enabled by manager                  */
    TFA9896_BF_BATS  = 0x0109,    /*!< Battery voltage from ADC readout                   */
    TFA9896_BF_TEMPS = 0x0208,    /*!< Temperature readout from the temperature sensor ( C) */
    TFA9896_BF_REV   = 0x030f,    /*!< Device revision information                        */
    TFA9896_BF_RCV   = 0x0420,    /*!< Enable receiver mode                               */
    TFA9896_BF_CHS12 = 0x0431,    /*!< Channel Selection TDM input for Coolflux           */
    TFA9896_BF_INPLVL= 0x0450,    /*!< Input level selection attenuator (                 */
    TFA9896_BF_CHSA  = 0x0461,    /*!< Input selection for amplifier                      */
    TFA9896_BF_AUDFS = 0x04c3,    /*!< Audio sample rate setting                          */
    TFA9896_BF_BSSCR = 0x0501,    /*!< Batteery protection attack time                    */
    TFA9896_BF_BSST  = 0x0523,    /*!< Battery protection threshold level                 */
    TFA9896_BF_BSSRL = 0x0561,    /*!< Battery protection maximum reduction               */
    TFA9896_BF_BSSRR = 0x0582,    /*!< Battery protection release time                    */
    TFA9896_BF_BSSHY = 0x05b1,    /*!< Battery Protection Hysteresis                      */
    TFA9896_BF_BSSR  = 0x05e0,    /*!< Battery voltage value for read out (only)          */
    TFA9896_BF_BSSBY = 0x05f0,    /*!< Bypass clipper battery protection                  */
    TFA9896_BF_DPSA  = 0x0600,    /*!< Enable dynamic powerstage activation (DPSA)        */
    TFA9896_BF_ATTEN = 0x0613,    /*!< Gain attenuation setting                           */
    TFA9896_BF_CFSM  = 0x0650,    /*!< Soft mute in CoolFlux                              */
    TFA9896_BF_BSSS  = 0x0670,    /*!< Battery sense steepness                            */
    TFA9896_BF_VOL   = 0x0687,    /*!< Coolflux volume control                            */
    TFA9896_BF_DCVO2 = 0x0702,    /*!< Second Boost Voltage                               */
    TFA9896_BF_DCMCC = 0x0733,    /*!< Max boost coil current - step of 175 mA            */
    TFA9896_BF_DCVO1 = 0x0772,    /*!< First Boost Voltage                                */
    TFA9896_BF_DCIE  = 0x07a0,    /*!< Adaptive boost mode                                */
    TFA9896_BF_DCSR  = 0x07b0,    /*!< Soft Rampup/down mode for DCDC controller          */
    TFA9896_BF_DCPAVG= 0x07c0,    /*!< ctrl_peak2avg for analog part of DCDC              */
    TFA9896_BF_DCPWM = 0x07d0,    /*!< DCDC PWM only mode                                 */
    TFA9896_BF_TROS  = 0x0800,    /*!< Selection ambient temperature for speaker calibration  */
    TFA9896_BF_EXTTS = 0x0818,    /*!< External temperature for speaker calibration (C)   */
    TFA9896_BF_PWDN  = 0x0900,    /*!< powerdown selection                                */
    TFA9896_BF_I2CR  = 0x0910,    /*!< All I2C registers reset to default                 */
    TFA9896_BF_CFE   = 0x0920,    /*!< Enable CoolFlux                                    */
    TFA9896_BF_AMPE  = 0x0930,    /*!< Enable Amplifier                                   */
    TFA9896_BF_DCA   = 0x0940,    /*!< Enable DCDC Boost converter                        */
    TFA9896_BF_SBSL  = 0x0950,    /*!< Coolflux configured                                */
    TFA9896_BF_AMPC  = 0x0960,    /*!< Selection if Coolflux enables amplifier            */
    TFA9896_BF_DCDIS = 0x0970,    /*!< DCDC boost converter not connected                 */
    TFA9896_BF_PSDR  = 0x0980,    /*!< IDDQ amplifier test selection                      */
    TFA9896_BF_INTPAD= 0x09c1,    /*!< INT pad (interrupt bump output) configuration      */
    TFA9896_BF_IPLL  = 0x09e0,    /*!< PLL input reference clock selection                */
    TFA9896_BF_DCTRIP= 0x0a04,    /*!< Adaptive boost trip levels (effective only when boost_intel is set to 1) */
    TFA9896_BF_DCHOLD= 0x0a54,    /*!< Hold time for DCDC booster (effective only when boost_intel is set to 1) */
    TFA9896_BF_MTPK  = 0x0b07,    /*!< KEY2 to access key2 protected registers (default for engineering) */
    TFA9896_BF_CVFDLY= 0x0c25,    /*!< Fractional delay adjustment between current and voltage sense */
    TFA9896_BF_OPENMTP= 0x0ec0,    /*!< Enable programming of the MTP memory               */
    TFA9896_BF_TDMPRF= 0x1011,    /*!< TDM usecase selection control                      */
    TFA9896_BF_TDMEN = 0x1030,    /*!< TDM interface enable                               */
    TFA9896_BF_TDMCKINV= 0x1040,    /*!< TDM clock inversion, receive on                    */
    TFA9896_BF_TDMFSLN= 0x1053,    /*!< TDM FS length                                      */
    TFA9896_BF_TDMFSPOL= 0x1090,    /*!< TDM FS polarity (start frame)                      */
    TFA9896_BF_TDMSAMSZ= 0x10a4,    /*!< TDM sample size for all TDM sinks and sources      */
    TFA9896_BF_TDMSLOTS= 0x1103,    /*!< TDM number of slots                                */
    TFA9896_BF_TDMSLLN= 0x1144,    /*!< TDM slot length                                    */
    TFA9896_BF_TDMBRMG= 0x1194,    /*!< TDM bits remaining after the last slot             */
    TFA9896_BF_TDMDDEL= 0x11e0,    /*!< TDM data delay                                     */
    TFA9896_BF_TDMDADJ= 0x11f0,    /*!< TDM data adjustment                                */
    TFA9896_BF_TDMTXFRM= 0x1201,    /*!< TDM TXDATA format                                  */
    TFA9896_BF_TDMUUS0= 0x1221,    /*!< TDM TXDATA format unused slot SD0                  */
    TFA9896_BF_TDMUUS1= 0x1241,    /*!< TDM TXDATA format unused slot SD1                  */
    TFA9896_BF_TDMSI0EN= 0x1270,    /*!< TDM sink0 enable                                   */
    TFA9896_BF_TDMSI1EN= 0x1280,    /*!< TDM sink1 enable                                   */
    TFA9896_BF_TDMSI2EN= 0x1290,    /*!< TDM sink2 enable                                   */
    TFA9896_BF_TDMSO0EN= 0x12a0,    /*!< TDM source0 enable                                 */
    TFA9896_BF_TDMSO1EN= 0x12b0,    /*!< TDM source1 enable                                 */
    TFA9896_BF_TDMSO2EN= 0x12c0,    /*!< TDM source2 enable                                 */
    TFA9896_BF_TDMSI0IO= 0x12d0,    /*!< TDM sink0 IO selection                             */
    TFA9896_BF_TDMSI1IO= 0x12e0,    /*!< TDM sink1 IO selection                             */
    TFA9896_BF_TDMSI2IO= 0x12f0,    /*!< TDM sink2 IO selection                             */
    TFA9896_BF_TDMSO0IO= 0x1300,    /*!< TDM source0 IO selection                           */
    TFA9896_BF_TDMSO1IO= 0x1310,    /*!< TDM source1 IO selection                           */
    TFA9896_BF_TDMSO2IO= 0x1320,    /*!< TDM source2 IO selection                           */
    TFA9896_BF_TDMSI0SL= 0x1333,    /*!< TDM sink0 slot position [GAIN IN]                  */
    TFA9896_BF_TDMSI1SL= 0x1373,    /*!< TDM sink1 slot position [CH1 IN]                   */
    TFA9896_BF_TDMSI2SL= 0x13b3,    /*!< TDM sink2 slot position [CH2 IN]                   */
    TFA9896_BF_TDMSO0SL= 0x1403,    /*!< TDM source0 slot position [GAIN OUT]               */
    TFA9896_BF_TDMSO1SL= 0x1443,    /*!< TDM source1 slot position [Voltage Sense]          */
    TFA9896_BF_TDMSO2SL= 0x1483,    /*!< TDM source2 slot position [Current Sense]          */
    TFA9896_BF_NBCK  = 0x14c3,    /*!< TDM NBCK bit clock ratio                           */
    TFA9896_BF_INTOVDDS= 0x2000,    /*!< flag_por_int_out                                   */
    TFA9896_BF_INTOPLLS= 0x2010,    /*!< flag_pll_lock_int_out                              */
    TFA9896_BF_INTOOTDS= 0x2020,    /*!< flag_otpok_int_out                                 */
    TFA9896_BF_INTOOVDS= 0x2030,    /*!< flag_ovpok_int_out                                 */
    TFA9896_BF_INTOUVDS= 0x2040,    /*!< flag_uvpok_int_out                                 */
    TFA9896_BF_INTOOCDS= 0x2050,    /*!< flag_ocp_alarm_int_out                             */
    TFA9896_BF_INTOCLKS= 0x2060,    /*!< flag_clocks_stable_int_out                         */
    TFA9896_BF_INTOCLIPS= 0x2070,    /*!< flag_clip_int_out                                  */
    TFA9896_BF_INTOMTPB= 0x2080,    /*!< mtp_busy_int_out                                   */
    TFA9896_BF_INTONOCLK= 0x2090,    /*!< flag_lost_clk_int_out                              */
    TFA9896_BF_INTOSPKS= 0x20a0,    /*!< flag_cf_speakererror_int_out                       */
    TFA9896_BF_INTOACS= 0x20b0,    /*!< flag_cold_started_int_out                          */
    TFA9896_BF_INTOSWS= 0x20c0,    /*!< flag_engage_int_out                                */
    TFA9896_BF_INTOWDS= 0x20d0,    /*!< flag_watchdog_reset_int_out                        */
    TFA9896_BF_INTOAMPS= 0x20e0,    /*!< flag_enbl_amp_int_out                              */
    TFA9896_BF_INTOAREFS= 0x20f0,    /*!< flag_enbl_ref_int_out                              */
    TFA9896_BF_INTOERR= 0x2200,    /*!< flag_cfma_err_int_out                              */
    TFA9896_BF_INTOACK= 0x2210,    /*!< flag_cfma_ack_int_out                              */
    TFA9896_BF_INTIVDDS= 0x2300,    /*!< flag_por_int_in                                    */
    TFA9896_BF_INTIPLLS= 0x2310,    /*!< flag_pll_lock_int_in                               */
    TFA9896_BF_INTIOTDS= 0x2320,    /*!< flag_otpok_int_in                                  */
    TFA9896_BF_INTIOVDS= 0x2330,    /*!< flag_ovpok_int_in                                  */
    TFA9896_BF_INTIUVDS= 0x2340,    /*!< flag_uvpok_int_in                                  */
    TFA9896_BF_INTIOCDS= 0x2350,    /*!< flag_ocp_alarm_int_in                              */
    TFA9896_BF_INTICLKS= 0x2360,    /*!< flag_clocks_stable_int_in                          */
    TFA9896_BF_INTICLIPS= 0x2370,    /*!< flag_clip_int_in                                   */
    TFA9896_BF_INTIMTPB= 0x2380,    /*!< mtp_busy_int_in                                    */
    TFA9896_BF_INTINOCLK= 0x2390,    /*!< flag_lost_clk_int_in                               */
    TFA9896_BF_INTISPKS= 0x23a0,    /*!< flag_cf_speakererror_int_in                        */
    TFA9896_BF_INTIACS= 0x23b0,    /*!< flag_cold_started_int_in                           */
    TFA9896_BF_INTISWS= 0x23c0,    /*!< flag_engage_int_in                                 */
    TFA9896_BF_INTIWDS= 0x23d0,    /*!< flag_watchdog_reset_int_in                         */
    TFA9896_BF_INTIAMPS= 0x23e0,    /*!< flag_enbl_amp_int_in                               */
    TFA9896_BF_INTIAREFS= 0x23f0,    /*!< flag_enbl_ref_int_in                               */
    TFA9896_BF_INTIERR= 0x2500,    /*!< flag_cfma_err_int_in                               */
    TFA9896_BF_INTIACK= 0x2510,    /*!< flag_cfma_ack_int_in                               */
    TFA9896_BF_INTENVDDS= 0x2600,    /*!< flag_por_int_enable                                */
    TFA9896_BF_INTENPLLS= 0x2610,    /*!< flag_pll_lock_int_enable                           */
    TFA9896_BF_INTENOTDS= 0x2620,    /*!< flag_otpok_int_enable                              */
    TFA9896_BF_INTENOVDS= 0x2630,    /*!< flag_ovpok_int_enable                              */
    TFA9896_BF_INTENUVDS= 0x2640,    /*!< flag_uvpok_int_enable                              */
    TFA9896_BF_INTENOCDS= 0x2650,    /*!< flag_ocp_alarm_int_enable                          */
    TFA9896_BF_INTENCLKS= 0x2660,    /*!< flag_clocks_stable_int_enable                      */
    TFA9896_BF_INTENCLIPS= 0x2670,    /*!< flag_clip_int_enable                               */
    TFA9896_BF_INTENMTPB= 0x2680,    /*!< mtp_busy_int_enable                                */
    TFA9896_BF_INTENNOCLK= 0x2690,    /*!< flag_lost_clk_int_enable                           */
    TFA9896_BF_INTENSPKS= 0x26a0,    /*!< flag_cf_speakererror_int_enable                    */
    TFA9896_BF_INTENACS= 0x26b0,    /*!< flag_cold_started_int_enable                       */
    TFA9896_BF_INTENSWS= 0x26c0,    /*!< flag_engage_int_enable                             */
    TFA9896_BF_INTENWDS= 0x26d0,    /*!< flag_watchdog_reset_int_enable                     */
    TFA9896_BF_INTENAMPS= 0x26e0,    /*!< flag_enbl_amp_int_enable                           */
    TFA9896_BF_INTENAREFS= 0x26f0,    /*!< flag_enbl_ref_int_enable                           */
    TFA9896_BF_INTENERR= 0x2800,    /*!< flag_cfma_err_int_enable                           */
    TFA9896_BF_INTENACK= 0x2810,    /*!< flag_cfma_ack_int_enable                           */
    TFA9896_BF_INTPOLVDDS= 0x2900,    /*!< flag_por_int_pol                                   */
    TFA9896_BF_INTPOLPLLS= 0x2910,    /*!< flag_pll_lock_int_pol                              */
    TFA9896_BF_INTPOLOTDS= 0x2920,    /*!< flag_otpok_int_pol                                 */
    TFA9896_BF_INTPOLOVDS= 0x2930,    /*!< flag_ovpok_int_pol                                 */
    TFA9896_BF_INTPOLUVDS= 0x2940,    /*!< flag_uvpok_int_pol                                 */
    TFA9896_BF_INTPOLOCDS= 0x2950,    /*!< flag_ocp_alarm_int_pol                             */
    TFA9896_BF_INTPOLCLKS= 0x2960,    /*!< flag_clocks_stable_int_pol                         */
    TFA9896_BF_INTPOLCLIPS= 0x2970,    /*!< flag_clip_int_pol                                  */
    TFA9896_BF_INTPOLMTPB= 0x2980,    /*!< mtp_busy_int_pol                                   */
    TFA9896_BF_INTPOLNOCLK= 0x2990,    /*!< flag_lost_clk_int_pol                              */
    TFA9896_BF_INTPOLSPKS= 0x29a0,    /*!< flag_cf_speakererror_int_pol                       */
    TFA9896_BF_INTPOLACS= 0x29b0,    /*!< flag_cold_started_int_pol                          */
    TFA9896_BF_INTPOLSWS= 0x29c0,    /*!< flag_engage_int_pol                                */
    TFA9896_BF_INTPOLWDS= 0x29d0,    /*!< flag_watchdog_reset_int_pol                        */
    TFA9896_BF_INTPOLAMPS= 0x29e0,    /*!< flag_enbl_amp_int_pol                              */
    TFA9896_BF_INTPOLAREFS= 0x29f0,    /*!< flag_enbl_ref_int_pol                              */
    TFA9896_BF_INTPOLERR= 0x2b00,    /*!< flag_cfma_err_int_pol                              */
    TFA9896_BF_INTPOLACK= 0x2b10,    /*!< flag_cfma_ack_int_pol                              */
    TFA9896_BF_CLIP  = 0x4900,    /*!< Bypass clip control                                */
    TFA9896_BF_CIMTP = 0x62b0,    /*!< Start copying data from I2C mtp registers to mtp   */
    TFA9896_BF_RST   = 0x7000,    /*!< Reset CoolFlux DSP                                 */
    TFA9896_BF_DMEM  = 0x7011,    /*!< Target memory for access                           */
    TFA9896_BF_AIF   = 0x7030,    /*!< Auto increment flag for memory-address             */
    TFA9896_BF_CFINT = 0x7040,    /*!< CF Interrupt - auto clear                          */
    TFA9896_BF_REQ   = 0x7087,    /*!< CF request for accessing the 8 channels            */
    TFA9896_BF_MADD  = 0x710f,    /*!< Memory address                                     */
    TFA9896_BF_MEMA  = 0x720f,    /*!< Activate memory access                             */
    TFA9896_BF_ERR   = 0x7307,    /*!< CF error flags                                     */
    TFA9896_BF_ACK   = 0x7387,    /*!< CF acknowledgement of the requests channels        */
    TFA9896_BF_MTPOTC= 0x8000,    /*!< Calibration schedule selection                     */
    TFA9896_BF_MTPEX = 0x8010,    /*!< Calibration of RON status bit                      */
} nxpTFA9896BfEnumList_t;
#define TFA9896_NAMETABLE static tfaBfName_t Tfa9896DatasheetNames[]= {\
   { 0x0, "VDDS"},    /* Power-on-reset flag (auto clear by reading)       , */\
   { 0x10, "PLLS"},    /* PLL lock to programmed frequency                  , */\
   { 0x20, "OTDS"},    /* Over Temperature Protection alarm                 , */\
   { 0x30, "OVDS"},    /* Over Voltage Protection alarm                     , */\
   { 0x40, "UVDS"},    /* Under Voltage Protection alarm                    , */\
   { 0x50, "OCDS"},    /* Over Current Protection alarm                     , */\
   { 0x60, "CLKS"},    /* Clocks stable flag                                , */\
   { 0x70, "CLIPS"},    /* Amplifier clipping                                , */\
   { 0x80, "MTPB"},    /* MTP busy copying data to/from I2C registers       , */\
   { 0x90, "NOCLK"},    /* lost clock detection (reference input clock)      , */\
   { 0xa0, "SPKS"},    /* Speaker error                                     , */\
   { 0xb0, "ACS"},    /* Cold Start required                               , */\
   { 0xc0, "SWS"},    /* Amplifier engage (Amp Switching)                  , */\
   { 0xd0, "WDS"},    /* watchdog reset (activates reset)                  , */\
   { 0xe0, "AMPS"},    /* Amplifier is enabled by manager                   , */\
   { 0xf0, "AREFS"},    /* References are enabled by manager                 , */\
   { 0x109, "BATS"},    /* Battery voltage from ADC readout                  , */\
   { 0x208, "TEMPS"},    /* Temperature readout from the temperature sensor ( C), */\
   { 0x30f, "REV"},    /* Device revision information                       , */\
   { 0x420, "RCV"},    /* Enable receiver mode                              , */\
   { 0x431, "CHS12"},    /* Channel Selection TDM input for Coolflux          , */\
   { 0x450, "INPLVL"},    /* Input level selection attenuator (                , */\
   { 0x461, "CHSA"},    /* Input selection for amplifier                     , */\
   { 0x4c3, "AUDFS"},    /* Audio sample rate setting                         , */\
   { 0x501, "BSSCR"},    /* Batteery protection attack time                   , */\
   { 0x523, "BSST"},    /* Battery protection threshold level                , */\
   { 0x561, "BSSRL"},    /* Battery protection maximum reduction              , */\
   { 0x582, "BSSRR"},    /* Battery protection release time                   , */\
   { 0x5b1, "BSSHY"},    /* Battery Protection Hysteresis                     , */\
   { 0x5e0, "BSSR"},    /* Battery voltage value for read out (only)         , */\
   { 0x5f0, "BSSBY"},    /* Bypass clipper battery protection                 , */\
   { 0x600, "DPSA"},    /* Enable dynamic powerstage activation (DPSA)       , */\
   { 0x613, "ATTEN"},    /* Gain attenuation setting                          , */\
   { 0x650, "CFSM"},    /* Soft mute in CoolFlux                             , */\
   { 0x670, "BSSS"},    /* Battery sense steepness                           , */\
   { 0x687, "VOL"},    /* Coolflux volume control                           , */\
   { 0x702, "DCVO2"},    /* Second Boost Voltage                              , */\
   { 0x733, "DCMCC"},    /* Max boost coil current - step of 175 mA           , */\
   { 0x772, "DCVO1"},    /* First Boost Voltage                               , */\
   { 0x7a0, "DCIE"},    /* Adaptive boost mode                               , */\
   { 0x7b0, "DCSR"},    /* Soft Rampup/down mode for DCDC controller         , */\
   { 0x7c0, "DCPAVG"},    /* ctrl_peak2avg for analog part of DCDC             , */\
   { 0x7d0, "DCPWM"},    /* DCDC PWM only mode                                , */\
   { 0x800, "TROS"},    /* Selection ambient temperature for speaker calibration , */\
   { 0x818, "EXTTS"},    /* External temperature for speaker calibration (C)  , */\
   { 0x900, "PWDN"},    /* powerdown selection                               , */\
   { 0x910, "I2CR"},    /* All I2C registers reset to default                , */\
   { 0x920, "CFE"},    /* Enable CoolFlux                                   , */\
   { 0x930, "AMPE"},    /* Enable Amplifier                                  , */\
   { 0x940, "DCA"},    /* Enable DCDC Boost converter                       , */\
   { 0x950, "SBSL"},    /* Coolflux configured                               , */\
   { 0x960, "AMPC"},    /* Selection if Coolflux enables amplifier           , */\
   { 0x970, "DCDIS"},    /* DCDC boost converter not connected                , */\
   { 0x980, "PSDR"},    /* IDDQ amplifier test selection                     , */\
   { 0x9c1, "INTPAD"},    /* INT pad (interrupt bump output) configuration     , */\
   { 0x9e0, "IPLL"},    /* PLL input reference clock selection               , */\
   { 0xa04, "DCTRIP"},    /* Adaptive boost trip levels (effective only when boost_intel is set to 1), */\
   { 0xa54, "DCHOLD"},    /* Hold time for DCDC booster (effective only when boost_intel is set to 1), */\
   { 0xb07, "MTPK"},    /* KEY2 to access key2 protected registers (default for engineering), */\
   { 0xc25, "CVFDLY"},    /* Fractional delay adjustment between current and voltage sense, */\
   { 0xec0, "OPENMTP"},    /* Enable programming of the MTP memory              , */\
   { 0x1011, "TDMPRF"},    /* TDM usecase selection control                     , */\
   { 0x1030, "TDMEN"},    /* TDM interface enable                              , */\
   { 0x1040, "TDMCKINV"},    /* TDM clock inversion, receive on                   , */\
   { 0x1053, "TDMFSLN"},    /* TDM FS length                                     , */\
   { 0x1090, "TDMFSPOL"},    /* TDM FS polarity (start frame)                     , */\
   { 0x10a4, "TDMSAMSZ"},    /* TDM sample size for all TDM sinks and sources     , */\
   { 0x1103, "TDMSLOTS"},    /* TDM number of slots                               , */\
   { 0x1144, "TDMSLLN"},    /* TDM slot length                                   , */\
   { 0x1194, "TDMBRMG"},    /* TDM bits remaining after the last slot            , */\
   { 0x11e0, "TDMDDEL"},    /* TDM data delay                                    , */\
   { 0x11f0, "TDMDADJ"},    /* TDM data adjustment                               , */\
   { 0x1201, "TDMTXFRM"},    /* TDM TXDATA format                                 , */\
   { 0x1221, "TDMUUS0"},    /* TDM TXDATA format unused slot SD0                 , */\
   { 0x1241, "TDMUUS1"},    /* TDM TXDATA format unused slot SD1                 , */\
   { 0x1270, "TDMSI0EN"},    /* TDM sink0 enable                                  , */\
   { 0x1280, "TDMSI1EN"},    /* TDM sink1 enable                                  , */\
   { 0x1290, "TDMSI2EN"},    /* TDM sink2 enable                                  , */\
   { 0x12a0, "TDMSO0EN"},    /* TDM source0 enable                                , */\
   { 0x12b0, "TDMSO1EN"},    /* TDM source1 enable                                , */\
   { 0x12c0, "TDMSO2EN"},    /* TDM source2 enable                                , */\
   { 0x12d0, "TDMSI0IO"},    /* TDM sink0 IO selection                            , */\
   { 0x12e0, "TDMSI1IO"},    /* TDM sink1 IO selection                            , */\
   { 0x12f0, "TDMSI2IO"},    /* TDM sink2 IO selection                            , */\
   { 0x1300, "TDMSO0IO"},    /* TDM source0 IO selection                          , */\
   { 0x1310, "TDMSO1IO"},    /* TDM source1 IO selection                          , */\
   { 0x1320, "TDMSO2IO"},    /* TDM source2 IO selection                          , */\
   { 0x1333, "TDMSI0SL"},    /* TDM sink0 slot position [GAIN IN]                 , */\
   { 0x1373, "TDMSI1SL"},    /* TDM sink1 slot position [CH1 IN]                  , */\
   { 0x13b3, "TDMSI2SL"},    /* TDM sink2 slot position [CH2 IN]                  , */\
   { 0x1403, "TDMSO0SL"},    /* TDM source0 slot position [GAIN OUT]              , */\
   { 0x1443, "TDMSO1SL"},    /* TDM source1 slot position [Voltage Sense]         , */\
   { 0x1483, "TDMSO2SL"},    /* TDM source2 slot position [Current Sense]         , */\
   { 0x14c3, "NBCK"},    /* TDM NBCK bit clock ratio                          , */\
   { 0x2000, "INTOVDDS"},    /* flag_por_int_out                                  , */\
   { 0x2010, "INTOPLLS"},    /* flag_pll_lock_int_out                             , */\
   { 0x2020, "INTOOTDS"},    /* flag_otpok_int_out                                , */\
   { 0x2030, "INTOOVDS"},    /* flag_ovpok_int_out                                , */\
   { 0x2040, "INTOUVDS"},    /* flag_uvpok_int_out                                , */\
   { 0x2050, "INTOOCDS"},    /* flag_ocp_alarm_int_out                            , */\
   { 0x2060, "INTOCLKS"},    /* flag_clocks_stable_int_out                        , */\
   { 0x2070, "INTOCLIPS"},    /* flag_clip_int_out                                 , */\
   { 0x2080, "INTOMTPB"},    /* mtp_busy_int_out                                  , */\
   { 0x2090, "INTONOCLK"},    /* flag_lost_clk_int_out                             , */\
   { 0x20a0, "INTOSPKS"},    /* flag_cf_speakererror_int_out                      , */\
   { 0x20b0, "INTOACS"},    /* flag_cold_started_int_out                         , */\
   { 0x20c0, "INTOSWS"},    /* flag_engage_int_out                               , */\
   { 0x20d0, "INTOWDS"},    /* flag_watchdog_reset_int_out                       , */\
   { 0x20e0, "INTOAMPS"},    /* flag_enbl_amp_int_out                             , */\
   { 0x20f0, "INTOAREFS"},    /* flag_enbl_ref_int_out                             , */\
   { 0x2200, "INTOERR"},    /* flag_cfma_err_int_out                             , */\
   { 0x2210, "INTOACK"},    /* flag_cfma_ack_int_out                             , */\
   { 0x2300, "INTIVDDS"},    /* flag_por_int_in                                   , */\
   { 0x2310, "INTIPLLS"},    /* flag_pll_lock_int_in                              , */\
   { 0x2320, "INTIOTDS"},    /* flag_otpok_int_in                                 , */\
   { 0x2330, "INTIOVDS"},    /* flag_ovpok_int_in                                 , */\
   { 0x2340, "INTIUVDS"},    /* flag_uvpok_int_in                                 , */\
   { 0x2350, "INTIOCDS"},    /* flag_ocp_alarm_int_in                             , */\
   { 0x2360, "INTICLKS"},    /* flag_clocks_stable_int_in                         , */\
   { 0x2370, "INTICLIPS"},    /* flag_clip_int_in                                  , */\
   { 0x2380, "INTIMTPB"},    /* mtp_busy_int_in                                   , */\
   { 0x2390, "INTINOCLK"},    /* flag_lost_clk_int_in                              , */\
   { 0x23a0, "INTISPKS"},    /* flag_cf_speakererror_int_in                       , */\
   { 0x23b0, "INTIACS"},    /* flag_cold_started_int_in                          , */\
   { 0x23c0, "INTISWS"},    /* flag_engage_int_in                                , */\
   { 0x23d0, "INTIWDS"},    /* flag_watchdog_reset_int_in                        , */\
   { 0x23e0, "INTIAMPS"},    /* flag_enbl_amp_int_in                              , */\
   { 0x23f0, "INTIAREFS"},    /* flag_enbl_ref_int_in                              , */\
   { 0x2500, "INTIERR"},    /* flag_cfma_err_int_in                              , */\
   { 0x2510, "INTIACK"},    /* flag_cfma_ack_int_in                              , */\
   { 0x2600, "INTENVDDS"},    /* flag_por_int_enable                               , */\
   { 0x2610, "INTENPLLS"},    /* flag_pll_lock_int_enable                          , */\
   { 0x2620, "INTENOTDS"},    /* flag_otpok_int_enable                             , */\
   { 0x2630, "INTENOVDS"},    /* flag_ovpok_int_enable                             , */\
   { 0x2640, "INTENUVDS"},    /* flag_uvpok_int_enable                             , */\
   { 0x2650, "INTENOCDS"},    /* flag_ocp_alarm_int_enable                         , */\
   { 0x2660, "INTENCLKS"},    /* flag_clocks_stable_int_enable                     , */\
   { 0x2670, "INTENCLIPS"},    /* flag_clip_int_enable                              , */\
   { 0x2680, "INTENMTPB"},    /* mtp_busy_int_enable                               , */\
   { 0x2690, "INTENNOCLK"},    /* flag_lost_clk_int_enable                          , */\
   { 0x26a0, "INTENSPKS"},    /* flag_cf_speakererror_int_enable                   , */\
   { 0x26b0, "INTENACS"},    /* flag_cold_started_int_enable                      , */\
   { 0x26c0, "INTENSWS"},    /* flag_engage_int_enable                            , */\
   { 0x26d0, "INTENWDS"},    /* flag_watchdog_reset_int_enable                    , */\
   { 0x26e0, "INTENAMPS"},    /* flag_enbl_amp_int_enable                          , */\
   { 0x26f0, "INTENAREFS"},    /* flag_enbl_ref_int_enable                          , */\
   { 0x2800, "INTENERR"},    /* flag_cfma_err_int_enable                          , */\
   { 0x2810, "INTENACK"},    /* flag_cfma_ack_int_enable                          , */\
   { 0x2900, "INTPOLVDDS"},    /* flag_por_int_pol                                  , */\
   { 0x2910, "INTPOLPLLS"},    /* flag_pll_lock_int_pol                             , */\
   { 0x2920, "INTPOLOTDS"},    /* flag_otpok_int_pol                                , */\
   { 0x2930, "INTPOLOVDS"},    /* flag_ovpok_int_pol                                , */\
   { 0x2940, "INTPOLUVDS"},    /* flag_uvpok_int_pol                                , */\
   { 0x2950, "INTPOLOCDS"},    /* flag_ocp_alarm_int_pol                            , */\
   { 0x2960, "INTPOLCLKS"},    /* flag_clocks_stable_int_pol                        , */\
   { 0x2970, "INTPOLCLIPS"},    /* flag_clip_int_pol                                 , */\
   { 0x2980, "INTPOLMTPB"},    /* mtp_busy_int_pol                                  , */\
   { 0x2990, "INTPOLNOCLK"},    /* flag_lost_clk_int_pol                             , */\
   { 0x29a0, "INTPOLSPKS"},    /* flag_cf_speakererror_int_pol                      , */\
   { 0x29b0, "INTPOLACS"},    /* flag_cold_started_int_pol                         , */\
   { 0x29c0, "INTPOLSWS"},    /* flag_engage_int_pol                               , */\
   { 0x29d0, "INTPOLWDS"},    /* flag_watchdog_reset_int_pol                       , */\
   { 0x29e0, "INTPOLAMPS"},    /* flag_enbl_amp_int_pol                             , */\
   { 0x29f0, "INTPOLAREFS"},    /* flag_enbl_ref_int_pol                             , */\
   { 0x2b00, "INTPOLERR"},    /* flag_cfma_err_int_pol                             , */\
   { 0x2b10, "INTPOLACK"},    /* flag_cfma_ack_int_pol                             , */\
   { 0x4900, "CLIP"},    /* Bypass clip control                               , */\
   { 0x62b0, "CIMTP"},    /* Start copying data from I2C mtp registers to mtp  , */\
   { 0x7000, "RST"},    /* Reset CoolFlux DSP                                , */\
   { 0x7011, "DMEM"},    /* Target memory for access                          , */\
   { 0x7030, "AIF"},    /* Auto increment flag for memory-address            , */\
   { 0x7040, "CFINT"},    /* CF Interrupt - auto clear                         , */\
   { 0x7087, "REQ"},    /* CF request for accessing the 8 channels           , */\
   { 0x710f, "MADD"},    /* Memory address                                    , */\
   { 0x720f, "MEMA"},    /* Activate memory access                            , */\
   { 0x7307, "ERR"},    /* CF error flags                                    , */\
   { 0x7387, "ACK"},    /* CF acknowledgement of the requests channels       , */\
   { 0x8000, "MTPOTC"},    /* Calibration schedule selection                    , */\
   { 0x8010, "MTPEX"},    /* Calibration of RON status bit                     , */\
   { 0x8045, "SWPROFIL" },\
   { 0x80a5, "SWVSTEP" },\
   { 0xffff, "Unknown bitfield enum" }   /* not found */\
};

#define TFA9896_BITNAMETABLE static tfaBfName_t Tfa9896BitNames[]= {\
   { 0x0, "flag_por"},    /* Power-on-reset flag (auto clear by reading)       , */\
   { 0x10, "flag_pll_lock"},    /* PLL lock to programmed frequency                  , */\
   { 0x20, "flag_otpok"},    /* Over Temperature Protection alarm                 , */\
   { 0x30, "flag_ovpok"},    /* Over Voltage Protection alarm                     , */\
   { 0x40, "flag_uvpok"},    /* Under Voltage Protection alarm                    , */\
   { 0x50, "flag_ocp_alarm"},    /* Over Current Protection alarm                     , */\
   { 0x60, "flag_clocks_stable"},    /* Clocks stable flag                                , */\
   { 0x70, "flag_clip"},    /* Amplifier clipping                                , */\
   { 0x80, "mtp_busy"},    /* MTP busy copying data to/from I2C registers       , */\
   { 0x90, "flag_lost_clk"},    /* lost clock detection (reference input clock)      , */\
   { 0xa0, "flag_cf_speakererror"},    /* Speaker error                                     , */\
   { 0xb0, "flag_cold_started"},    /* Cold Start required                               , */\
   { 0xc0, "flag_engage"},    /* Amplifier engage (Amp Switching)                  , */\
   { 0xd0, "flag_watchdog_reset"},    /* watchdog reset (activates reset)                  , */\
   { 0xe0, "flag_enbl_amp"},    /* Amplifier is enabled by manager                   , */\
   { 0xf0, "flag_enbl_ref"},    /* References are enabled by manager                 , */\
   { 0x109, "bat_adc"},    /* Battery voltage from ADC readout                  , */\
   { 0x208, "temp_adc"},    /* Temperature readout from the temperature sensor ( C), */\
   { 0x30f, "device_rev"},    /* Device revision information                       , */\
   { 0x420, "ctrl_rcv"},    /* Enable receiver mode                              , */\
   { 0x431, "chan_sel"},    /* Channel Selection TDM input for Coolflux          , */\
   { 0x450, "input_level"},    /* Input level selection attenuator (                , */\
   { 0x461, "vamp_sel"},    /* Input selection for amplifier                     , */\
   { 0x4c3, "audio_fs"},    /* Audio sample rate setting                         , */\
   { 0x501, "vbat_prot_attacktime"},    /* Batteery protection attack time                   , */\
   { 0x523, "vbat_prot_thlevel"},    /* Battery protection threshold level                , */\
   { 0x561, "vbat_prot_max_reduct"},    /* Battery protection maximum reduction              , */\
   { 0x582, "vbat_prot_release_t"},    /* Battery protection release time                   , */\
   { 0x5b1, "vbat_prot_hysterese"},    /* Battery Protection Hysteresis                     , */\
   { 0x5d0, "reset_min_vbat"},    /* Battery supply safeguard clipper reset ( if CF_DSP is bypassed), */\
   { 0x5e0, "sel_vbat"},    /* Battery voltage value for read out (only)         , */\
   { 0x5f0, "bypass_clipper"},    /* Bypass clipper battery protection                 , */\
   { 0x600, "dpsa"},    /* Enable dynamic powerstage activation (DPSA)       , */\
   { 0x613, "ctrl_att"},    /* Gain attenuation setting                          , */\
   { 0x650, "cf_mute"},    /* Soft mute in CoolFlux                             , */\
   { 0x670, "batsense_steepness"},    /* Battery sense steepness                           , */\
   { 0x687, "vol"},    /* Coolflux volume control                           , */\
   { 0x702, "scnd_boost_voltage"},    /* Second Boost Voltage                              , */\
   { 0x733, "boost_cur"},    /* Max boost coil current - step of 175 mA           , */\
   { 0x772, "frst_boost_voltage"},    /* First Boost Voltage                               , */\
   { 0x7a0, "boost_intel"},    /* Adaptive boost mode                               , */\
   { 0x7b0, "boost_speed"},    /* Soft Rampup/down mode for DCDC controller         , */\
   { 0x7c0, "boost_peak2avg"},    /* ctrl_peak2avg for analog part of DCDC             , */\
   { 0x7d0, "dcdc_pwmonly"},    /* DCDC PWM only mode                                , */\
   { 0x7e0, "ignore_flag_voutcomp86"},    /* Ignore flag_voutcomp86  (flag from analog)        , */\
   { 0x800, "ext_temp_sel"},    /* Selection ambient temperature for speaker calibration , */\
   { 0x818, "ext_temp"},    /* External temperature for speaker calibration (C)  , */\
   { 0x900, "powerdown"},    /* powerdown selection                               , */\
   { 0x910, "reset"},    /* All I2C registers reset to default                , */\
   { 0x920, "enbl_coolflux"},    /* Enable CoolFlux                                   , */\
   { 0x930, "enbl_amplifier"},    /* Enable Amplifier                                  , */\
   { 0x940, "enbl_boost"},    /* Enable DCDC Boost converter                       , */\
   { 0x950, "coolflux_configured"},    /* Coolflux configured                               , */\
   { 0x960, "sel_enbl_amplifier"},    /* Selection if Coolflux enables amplifier           , */\
   { 0x970, "dcdcoff_mode"},    /* DCDC boost converter not connected                , */\
   { 0x980, "iddqtest"},    /* IDDQ amplifier test selection                     , */\
   { 0x9c1, "int_pad_io"},    /* INT pad (interrupt bump output) configuration     , */\
   { 0x9e0, "sel_fs_bck"},    /* PLL input reference clock selection               , */\
   { 0x9f0, "sel_scl_cf_clock"},    /* Coolflux sub-system clock selection               , */\
   { 0xa04, "boost_trip_lvl"},    /* Adaptive boost trip levels (effective only when boost_intel is set to 1), */\
   { 0xa54, "boost_hold_time"},    /* Hold time for DCDC booster (effective only when boost_intel is set to 1), */\
   { 0xaa1, "bst_slpcmplvl"},    /* Slope compensation current, represents LxF (inductance x frequency) value , */\
   { 0xb07, "mtpkey2"},    /* KEY2 to access key2 protected registers (default for engineering), */\
   { 0xc00, "enbl_volt_sense"},    /* Voltage sense enabling control bit                , */\
   { 0xc10, "vsense_pwm_sel"},    /* Voltage sense source selection                    , */\
   { 0xc25, "vi_frac_delay"},    /* Fractional delay adjustment between current and voltage sense, */\
   { 0xc80, "sel_voltsense_out"},    /* TDM output data selection for AEC                 , */\
   { 0xc90, "vsense_bypass_avg"},    /* Voltage sense average block bypass                , */\
   { 0xd05, "cf_frac_delay"},    /* Fractional delay adjustment between current and voltage sense by firmware, */\
   { 0xe00, "bypass_dcdc_curr_prot"},    /* Control to switch off dcdc current reduction with bat protection, */\
   { 0xe10, "bypass_ocp"},    /* Bypass OCP (digital IP block)                     , */\
   { 0xe20, "ocptest"},    /* ocptest (analog IP block) enable                  , */\
   { 0xe80, "disable_clock_sh_prot"},    /* Disable clock_sh protection                       , */\
   { 0xe92, "reserve_reg_15_09"},    /* Spare control bits for future usage               , */\
   { 0xec0, "unprotect_mtp"},    /* Enable programming of the MTP memory              , */\
   { 0xed2, "reserve_reg_15_13"},    /* Spare control bits for future usage               , */\
   { 0xf00, "dcdc_pfm20khz_limit"},    /* DCDC in PFM mode forcing each 50us a pwm pulse    , */\
   { 0xf11, "dcdc_ctrl_maxzercnt"},    /* DCDC number of zero current flags required to go to pfm mode, */\
   { 0xf36, "dcdc_vbat_delta_detect"},    /* DCDC threshold required on a delta Vbat (in PFM mode) switching to PWM mode, */\
   { 0xfa0, "dcdc_ignore_vbat"},    /* Ignore an increase on Vbat                        , */\
   { 0x1011, "tdm_usecase"},    /* TDM usecase selection control                     , */\
   { 0x1030, "tdm_enable"},    /* TDM interface enable                              , */\
   { 0x1040, "tdm_clk_inversion"},    /* TDM clock inversion, receive on                   , */\
   { 0x1053, "tdm_fs_ws_length"},    /* TDM FS length                                     , */\
   { 0x1090, "tdm_fs_ws_polarity"},    /* TDM FS polarity (start frame)                     , */\
   { 0x10a4, "tdm_sample_size"},    /* TDM sample size for all TDM sinks and sources     , */\
   { 0x1103, "tdm_nb_of_slots"},    /* TDM number of slots                               , */\
   { 0x1144, "tdm_slot_length"},    /* TDM slot length                                   , */\
   { 0x1194, "tdm_bits_remaining"},    /* TDM bits remaining after the last slot            , */\
   { 0x11e0, "tdm_data_delay"},    /* TDM data delay                                    , */\
   { 0x11f0, "tdm_data_adjustment"},    /* TDM data adjustment                               , */\
   { 0x1201, "tdm_txdata_format"},    /* TDM TXDATA format                                 , */\
   { 0x1221, "tdm_txdata_format_unused_slot_sd0"},    /* TDM TXDATA format unused slot SD0                 , */\
   { 0x1241, "tdm_txdata_format_unused_slot_sd1"},    /* TDM TXDATA format unused slot SD1                 , */\
   { 0x1270, "tdm_sink0_enable"},    /* TDM sink0 enable                                  , */\
   { 0x1280, "tdm_sink1_enable"},    /* TDM sink1 enable                                  , */\
   { 0x1290, "tdm_sink2_enable"},    /* TDM sink2 enable                                  , */\
   { 0x12a0, "tdm_source0_enable"},    /* TDM source0 enable                                , */\
   { 0x12b0, "tdm_source1_enable"},    /* TDM source1 enable                                , */\
   { 0x12c0, "tdm_source2_enable"},    /* TDM source2 enable                                , */\
   { 0x12d0, "tdm_sink0_io"},    /* TDM sink0 IO selection                            , */\
   { 0x12e0, "tdm_sink1_io"},    /* TDM sink1 IO selection                            , */\
   { 0x12f0, "tdm_sink2_io"},    /* TDM sink2 IO selection                            , */\
   { 0x1300, "tdm_source0_io"},    /* TDM source0 IO selection                          , */\
   { 0x1310, "tdm_source1_io"},    /* TDM source1 IO selection                          , */\
   { 0x1320, "tdm_source2_io"},    /* TDM source2 IO selection                          , */\
   { 0x1333, "tdm_sink0_slot"},    /* TDM sink0 slot position [GAIN IN]                 , */\
   { 0x1373, "tdm_sink1_slot"},    /* TDM sink1 slot position [CH1 IN]                  , */\
   { 0x13b3, "tdm_sink2_slot"},    /* TDM sink2 slot position [CH2 IN]                  , */\
   { 0x1403, "tdm_source0_slot"},    /* TDM source0 slot position [GAIN OUT]              , */\
   { 0x1443, "tdm_source1_slot"},    /* TDM source1 slot position [Voltage Sense]         , */\
   { 0x1483, "tdm_source2_slot"},    /* TDM source2 slot position [Current Sense]         , */\
   { 0x14c3, "tdm_nbck"},    /* TDM NBCK bit clock ratio                          , */\
   { 0x1500, "flag_tdm_lut_error"},    /* TDM LUT error flag                                , */\
   { 0x1512, "flag_tdm_status"},    /* TDM interface status bits                         , */\
   { 0x1540, "flag_tdm_error"},    /* TDM interface error indicator                     , */\
   { 0x1551, "status_bst_mode"},    /* DCDC mode status bits                             , */\
   { 0x2000, "flag_por_int_out"},    /* flag_por_int_out                                  , */\
   { 0x2010, "flag_pll_lock_int_out"},    /* flag_pll_lock_int_out                             , */\
   { 0x2020, "flag_otpok_int_out"},    /* flag_otpok_int_out                                , */\
   { 0x2030, "flag_ovpok_int_out"},    /* flag_ovpok_int_out                                , */\
   { 0x2040, "flag_uvpok_int_out"},    /* flag_uvpok_int_out                                , */\
   { 0x2050, "flag_ocp_alarm_int_out"},    /* flag_ocp_alarm_int_out                            , */\
   { 0x2060, "flag_clocks_stable_int_out"},    /* flag_clocks_stable_int_out                        , */\
   { 0x2070, "flag_clip_int_out"},    /* flag_clip_int_out                                 , */\
   { 0x2080, "mtp_busy_int_out"},    /* mtp_busy_int_out                                  , */\
   { 0x2090, "flag_lost_clk_int_out"},    /* flag_lost_clk_int_out                             , */\
   { 0x20a0, "flag_cf_speakererror_int_out"},    /* flag_cf_speakererror_int_out                      , */\
   { 0x20b0, "flag_cold_started_int_out"},    /* flag_cold_started_int_out                         , */\
   { 0x20c0, "flag_engage_int_out"},    /* flag_engage_int_out                               , */\
   { 0x20d0, "flag_watchdog_reset_int_out"},    /* flag_watchdog_reset_int_out                       , */\
   { 0x20e0, "flag_enbl_amp_int_out"},    /* flag_enbl_amp_int_out                             , */\
   { 0x20f0, "flag_enbl_ref_int_out"},    /* flag_enbl_ref_int_out                             , */\
   { 0x2100, "flag_voutcomp_int_out"},    /* flag_voutcomp_int_out                             , */\
   { 0x2110, "flag_voutcomp93_int_out"},    /* flag_voutcomp93_int_out                           , */\
   { 0x2120, "flag_voutcomp86_int_out"},    /* flag_voutcomp86_int_out                           , */\
   { 0x2130, "flag_hiz_int_out"},    /* flag_hiz_int_out                                  , */\
   { 0x2140, "flag_ocpokbst_int_out"},    /* flag_ocpokbst_int_out                             , */\
   { 0x2150, "flag_peakcur_int_out"},    /* flag_peakcur_int_out                              , */\
   { 0x2160, "flag_ocpokap_int_out"},    /* flag_ocpokap_int_out                              , */\
   { 0x2170, "flag_ocpokan_int_out"},    /* flag_ocpokan_int_out                              , */\
   { 0x2180, "flag_ocpokbp_int_out"},    /* flag_ocpokbp_int_out                              , */\
   { 0x2190, "flag_ocpokbn_int_out"},    /* flag_ocpokbn_int_out                              , */\
   { 0x21a0, "flag_adc10_ready_int_out"},    /* flag_adc10_ready_int_out                          , */\
   { 0x21b0, "flag_clipa_high_int_out"},    /* flag_clipa_high_int_out                           , */\
   { 0x21c0, "flag_clipa_low_int_out"},    /* flag_clipa_low_int_out                            , */\
   { 0x21d0, "flag_clipb_high_int_out"},    /* flag_clipb_high_int_out                           , */\
   { 0x21e0, "flag_clipb_low_int_out"},    /* flag_clipb_low_int_out                            , */\
   { 0x21f0, "flag_tdm_error_int_out"},    /* flag_tdm_error_int_out                            , */\
   { 0x2200, "flag_cfma_err_int_out"},    /* flag_cfma_err_int_out                             , */\
   { 0x2210, "flag_cfma_ack_int_out"},    /* flag_cfma_ack_int_out                             , */\
   { 0x2300, "flag_por_int_in"},    /* flag_por_int_in                                   , */\
   { 0x2310, "flag_pll_lock_int_in"},    /* flag_pll_lock_int_in                              , */\
   { 0x2320, "flag_otpok_int_in"},    /* flag_otpok_int_in                                 , */\
   { 0x2330, "flag_ovpok_int_in"},    /* flag_ovpok_int_in                                 , */\
   { 0x2340, "flag_uvpok_int_in"},    /* flag_uvpok_int_in                                 , */\
   { 0x2350, "flag_ocp_alarm_int_in"},    /* flag_ocp_alarm_int_in                             , */\
   { 0x2360, "flag_clocks_stable_int_in"},    /* flag_clocks_stable_int_in                         , */\
   { 0x2370, "flag_clip_int_in"},    /* flag_clip_int_in                                  , */\
   { 0x2380, "mtp_busy_int_in"},    /* mtp_busy_int_in                                   , */\
   { 0x2390, "flag_lost_clk_int_in"},    /* flag_lost_clk_int_in                              , */\
   { 0x23a0, "flag_cf_speakererror_int_in"},    /* flag_cf_speakererror_int_in                       , */\
   { 0x23b0, "flag_cold_started_int_in"},    /* flag_cold_started_int_in                          , */\
   { 0x23c0, "flag_engage_int_in"},    /* flag_engage_int_in                                , */\
   { 0x23d0, "flag_watchdog_reset_int_in"},    /* flag_watchdog_reset_int_in                        , */\
   { 0x23e0, "flag_enbl_amp_int_in"},    /* flag_enbl_amp_int_in                              , */\
   { 0x23f0, "flag_enbl_ref_int_in"},    /* flag_enbl_ref_int_in                              , */\
   { 0x2400, "flag_voutcomp_int_in"},    /* flag_voutcomp_int_in                              , */\
   { 0x2410, "flag_voutcomp93_int_in"},    /* flag_voutcomp93_int_in                            , */\
   { 0x2420, "flag_voutcomp86_int_in"},    /* flag_voutcomp86_int_in                            , */\
   { 0x2430, "flag_hiz_int_in"},    /* flag_hiz_int_in                                   , */\
   { 0x2440, "flag_ocpokbst_int_in"},    /* flag_ocpokbst_int_in                              , */\
   { 0x2450, "flag_peakcur_int_in"},    /* flag_peakcur_int_in                               , */\
   { 0x2460, "flag_ocpokap_int_in"},    /* flag_ocpokap_int_in                               , */\
   { 0x2470, "flag_ocpokan_int_in"},    /* flag_ocpokan_int_in                               , */\
   { 0x2480, "flag_ocpokbp_int_in"},    /* flag_ocpokbp_int_in                               , */\
   { 0x2490, "flag_ocpokbn_int_in"},    /* flag_ocpokbn_int_in                               , */\
   { 0x24a0, "flag_adc10_ready_int_in"},    /* flag_adc10_ready_int_in                           , */\
   { 0x24b0, "flag_clipa_high_int_in"},    /* flag_clipa_high_int_in                            , */\
   { 0x24c0, "flag_clipa_low_int_in"},    /* flag_clipa_low_int_in                             , */\
   { 0x24d0, "flag_clipb_high_int_in"},    /* flag_clipb_high_int_in                            , */\
   { 0x24e0, "flag_clipb_low_int_in"},    /* flag_clipb_low_int_in                             , */\
   { 0x24f0, "flag_tdm_error_int_in"},    /* flag_tdm_error_int_in                             , */\
   { 0x2500, "flag_cfma_err_int_in"},    /* flag_cfma_err_int_in                              , */\
   { 0x2510, "flag_cfma_ack_int_in"},    /* flag_cfma_ack_int_in                              , */\
   { 0x2600, "flag_por_int_enable"},    /* flag_por_int_enable                               , */\
   { 0x2610, "flag_pll_lock_int_enable"},    /* flag_pll_lock_int_enable                          , */\
   { 0x2620, "flag_otpok_int_enable"},    /* flag_otpok_int_enable                             , */\
   { 0x2630, "flag_ovpok_int_enable"},    /* flag_ovpok_int_enable                             , */\
   { 0x2640, "flag_uvpok_int_enable"},    /* flag_uvpok_int_enable                             , */\
   { 0x2650, "flag_ocp_alarm_int_enable"},    /* flag_ocp_alarm_int_enable                         , */\
   { 0x2660, "flag_clocks_stable_int_enable"},    /* flag_clocks_stable_int_enable                     , */\
   { 0x2670, "flag_clip_int_enable"},    /* flag_clip_int_enable                              , */\
   { 0x2680, "mtp_busy_int_enable"},    /* mtp_busy_int_enable                               , */\
   { 0x2690, "flag_lost_clk_int_enable"},    /* flag_lost_clk_int_enable                          , */\
   { 0x26a0, "flag_cf_speakererror_int_enable"},    /* flag_cf_speakererror_int_enable                   , */\
   { 0x26b0, "flag_cold_started_int_enable"},    /* flag_cold_started_int_enable                      , */\
   { 0x26c0, "flag_engage_int_enable"},    /* flag_engage_int_enable                            , */\
   { 0x26d0, "flag_watchdog_reset_int_enable"},    /* flag_watchdog_reset_int_enable                    , */\
   { 0x26e0, "flag_enbl_amp_int_enable"},    /* flag_enbl_amp_int_enable                          , */\
   { 0x26f0, "flag_enbl_ref_int_enable"},    /* flag_enbl_ref_int_enable                          , */\
   { 0x2700, "flag_voutcomp_int_enable"},    /* flag_voutcomp_int_enable                          , */\
   { 0x2710, "flag_voutcomp93_int_enable"},    /* flag_voutcomp93_int_enable                        , */\
   { 0x2720, "flag_voutcomp86_int_enable"},    /* flag_voutcomp86_int_enable                        , */\
   { 0x2730, "flag_hiz_int_enable"},    /* flag_hiz_int_enable                               , */\
   { 0x2740, "flag_ocpokbst_int_enable"},    /* flag_ocpokbst_int_enable                          , */\
   { 0x2750, "flag_peakcur_int_enable"},    /* flag_peakcur_int_enable                           , */\
   { 0x2760, "flag_ocpokap_int_enable"},    /* flag_ocpokap_int_enable                           , */\
   { 0x2770, "flag_ocpokan_int_enable"},    /* flag_ocpokan_int_enable                           , */\
   { 0x2780, "flag_ocpokbp_int_enable"},    /* flag_ocpokbp_int_enable                           , */\
   { 0x2790, "flag_ocpokbn_int_enable"},    /* flag_ocpokbn_int_enable                           , */\
   { 0x27a0, "flag_adc10_ready_int_enable"},    /* flag_adc10_ready_int_enable                       , */\
   { 0x27b0, "flag_clipa_high_int_enable"},    /* flag_clipa_high_int_enable                        , */\
   { 0x27c0, "flag_clipa_low_int_enable"},    /* flag_clipa_low_int_enable                         , */\
   { 0x27d0, "flag_clipb_high_int_enable"},    /* flag_clipb_high_int_enable                        , */\
   { 0x27e0, "flag_clipb_low_int_enable"},    /* flag_clipb_low_int_enable                         , */\
   { 0x27f0, "flag_tdm_error_int_enable"},    /* flag_tdm_error_int_enable                         , */\
   { 0x2800, "flag_cfma_err_int_enable"},    /* flag_cfma_err_int_enable                          , */\
   { 0x2810, "flag_cfma_ack_int_enable"},    /* flag_cfma_ack_int_enable                          , */\
   { 0x2900, "flag_por_int_pol"},    /* flag_por_int_pol                                  , */\
   { 0x2910, "flag_pll_lock_int_pol"},    /* flag_pll_lock_int_pol                             , */\
   { 0x2920, "flag_otpok_int_pol"},    /* flag_otpok_int_pol                                , */\
   { 0x2930, "flag_ovpok_int_pol"},    /* flag_ovpok_int_pol                                , */\
   { 0x2940, "flag_uvpok_int_pol"},    /* flag_uvpok_int_pol                                , */\
   { 0x2950, "flag_ocp_alarm_int_pol"},    /* flag_ocp_alarm_int_pol                            , */\
   { 0x2960, "flag_clocks_stable_int_pol"},    /* flag_clocks_stable_int_pol                        , */\
   { 0x2970, "flag_clip_int_pol"},    /* flag_clip_int_pol                                 , */\
   { 0x2980, "mtp_busy_int_pol"},    /* mtp_busy_int_pol                                  , */\
   { 0x2990, "flag_lost_clk_int_pol"},    /* flag_lost_clk_int_pol                             , */\
   { 0x29a0, "flag_cf_speakererror_int_pol"},    /* flag_cf_speakererror_int_pol                      , */\
   { 0x29b0, "flag_cold_started_int_pol"},    /* flag_cold_started_int_pol                         , */\
   { 0x29c0, "flag_engage_int_pol"},    /* flag_engage_int_pol                               , */\
   { 0x29d0, "flag_watchdog_reset_int_pol"},    /* flag_watchdog_reset_int_pol                       , */\
   { 0x29e0, "flag_enbl_amp_int_pol"},    /* flag_enbl_amp_int_pol                             , */\
   { 0x29f0, "flag_enbl_ref_int_pol"},    /* flag_enbl_ref_int_pol                             , */\
   { 0x2a00, "flag_voutcomp_int_pol"},    /* flag_voutcomp_int_pol                             , */\
   { 0x2a10, "flag_voutcomp93_int_pol"},    /* flag_voutcomp93_int_pol                           , */\
   { 0x2a20, "flag_voutcomp86_int_pol"},    /* flag_voutcomp86_int_pol                           , */\
   { 0x2a30, "flag_hiz_int_pol"},    /* flag_hiz_int_pol                                  , */\
   { 0x2a40, "flag_ocpokbst_int_pol"},    /* flag_ocpokbst_int_pol                             , */\
   { 0x2a50, "flag_peakcur_int_pol"},    /* flag_peakcur_int_pol                              , */\
   { 0x2a60, "flag_ocpokap_int_pol"},    /* flag_ocpokap_int_pol                              , */\
   { 0x2a70, "flag_ocpokan_int_pol"},    /* flag_ocpokan_int_pol                              , */\
   { 0x2a80, "flag_ocpokbp_int_pol"},    /* flag_ocpokbp_int_pol                              , */\
   { 0x2a90, "flag_ocpokbn_int_pol"},    /* flag_ocpokbn_int_pol                              , */\
   { 0x2aa0, "flag_adc10_ready_int_pol"},    /* flag_adc10_ready_int_pol                          , */\
   { 0x2ab0, "flag_clipa_high_int_pol"},    /* flag_clipa_high_int_pol                           , */\
   { 0x2ac0, "flag_clipa_low_int_pol"},    /* flag_clipa_low_int_pol                            , */\
   { 0x2ad0, "flag_clipb_high_int_pol"},    /* flag_clipb_high_int_pol                           , */\
   { 0x2ae0, "flag_clipb_low_int_pol"},    /* flag_clipb_low_int_pol                            , */\
   { 0x2af0, "flag_tdm_error_int_pol"},    /* flag_tdm_error_int_pol                            , */\
   { 0x2b00, "flag_cfma_err_int_pol"},    /* flag_cfma_err_int_pol                             , */\
   { 0x2b10, "flag_cfma_ack_int_pol"},    /* flag_cfma_ack_int_pol                             , */\
   { 0x3000, "flag_voutcomp"},    /* Status flag_voutcomp, indication Vset is larger than Vbat, */\
   { 0x3010, "flag_voutcomp93"},    /* Status flag_voutcomp93, indication Vset is larger than 1.07 x Vbat, */\
   { 0x3020, "flag_voutcomp86"},    /* Status flag voutcomp86, indication Vset is larger than 1.14 x Vbat, */\
   { 0x3030, "flag_hiz"},    /* Status flag_hiz, indication Vbst is larger than Vbat, */\
   { 0x3040, "flag_ocpokbst"},    /* Status flag_ocpokbst, indication no over current in boost converter PMOS switch, */\
   { 0x3050, "flag_peakcur"},    /* Status flag_peakcur, indication current is max in dcdc converter, */\
   { 0x3060, "flag_ocpokap"},    /* Status flag_ocpokap, indication no over current in amplifier A PMOS output stage, */\
   { 0x3070, "flag_ocpokan"},    /* Status flag_ocpokan, indication no over current in amplifier A NMOS output stage, */\
   { 0x3080, "flag_ocpokbp"},    /* Status flag_ocpokbp, indication no over current in amplifier B PMOS output stage, */\
   { 0x3090, "flag_ocpokbn"},    /* Status flag_ocpokbn, indication no over current in amplifier B NMOS output stage, */\
   { 0x30a0, "flag_adc10_ready"},    /* Status flag_adc10_ready, indication adc10 is ready, */\
   { 0x30b0, "flag_clipa_high"},    /* Status flag_clipa_high, indication pmos amplifier A is clipping, */\
   { 0x30c0, "flag_clipa_low"},    /* Status flag_clipa_low, indication nmos amplifier A is clipping, */\
   { 0x30d0, "flag_clipb_high"},    /* Status flag_clipb_high, indication pmos amplifier B is clipping, */\
   { 0x30e0, "flag_clipb_low"},    /* Status flag_clipb_low, indication nmos amplifier B is clipping, */\
   { 0x310f, "mtp_man_data_out"},    /* MTP manual read out data                          , */\
   { 0x3200, "key01_locked"},    /* Indicates KEY1 is locked                          , */\
   { 0x3210, "key02_locked"},    /* Indicates KEY2 is locked                          , */\
   { 0x3225, "mtp_ecc_tcout"},    /* MTP error correction test data out                , */\
   { 0x3280, "mtpctrl_valid_test_rd"},    /* MTP test readout for read                         , */\
   { 0x3290, "mtpctrl_valid_test_wr"},    /* MTP test readout for write                        , */\
   { 0x32a0, "flag_in_alarm_state"},    /* Flag alarm state                                  , */\
   { 0x32b0, "mtp_ecc_err2"},    /* Two or more bit errors detected in MTP, can not reconstruct value, */\
   { 0x32c0, "mtp_ecc_err1"},    /* One bit error detected in MTP, reconstructed value, */\
   { 0x32d0, "mtp_mtp_hvf"},    /* High voltage ready flag for MTP                   , */\
   { 0x32f0, "mtp_zero_check_fail"},    /* Zero check failed for MTP                         , */\
   { 0x3309, "data_adc10_tempbat"},    /* ADC10 data output for testing battery voltage and temperature, */\
   { 0x400f, "hid_code"},    /* 5A6Bh, 23147d to access hidden registers (default for engineering), */\
   { 0x4100, "bypass_hp"},    /* Bypass High Pass Filter                           , */\
   { 0x4110, "hard_mute"},    /* Hard Mute                                         , */\
   { 0x4120, "soft_mute"},    /* Soft Mute                                         , */\
   { 0x4134, "pwm_delay"},    /* PWM delay setting                                 , */\
   { 0x4180, "pwm_shape"},    /* PWM Shape                                         , */\
   { 0x4190, "pwm_bitlength"},    /* PWM Bitlength in noise shaper                     , */\
   { 0x4203, "drive"},    /* Drive bits to select number of amplifier power stages, */\
   { 0x4240, "reclock_pwm"},    /* Control for enabling reclocking of PWM signal     , */\
   { 0x4250, "reclock_voltsense"},    /* Control for enabling reclocking of voltage sense signal, */\
   { 0x4281, "dpsalevel"},    /* DPSA threshold level                              , */\
   { 0x42a1, "dpsa_release"},    /* DPSA release time                                 , */\
   { 0x42c0, "coincidence"},    /* Prevent simultaneously switching of output stage  , */\
   { 0x42d0, "kickback"},    /* Prevent double pulses of output stage             , */\
   { 0x4306, "drivebst"},    /* Drive bits to select the power transistor sections boost converter, */\
   { 0x4370, "boost_alg"},    /* Control for boost adaptive loop gain              , */\
   { 0x4381, "boost_loopgain"},    /* DCDC boost loopgain setting                       , */\
   { 0x43a0, "ocptestbst"},    /* Boost OCP. For old ocp (ctrl_reversebst is 0); For new ocp (ctrl_reversebst is 1), */\
   { 0x43d0, "test_abistfft_enbl"},    /* FFT Coolflux                                      , */\
   { 0x43e0, "bst_dcmbst"},    /* DCM mode control for DCDC during I2C direct control mode, */\
   { 0x43f0, "test_bcontrol"},    /* test_bcontrol                                     , */\
   { 0x4400, "reversebst"},    /* OverCurrent Protection selection of power stage boost converter, */\
   { 0x4410, "sensetest"},    /* Test option for the sense NMOS in booster for current mode control., */\
   { 0x4420, "enbl_engagebst"},    /* Enable power stage of dcdc controller             , */\
   { 0x4470, "enbl_slopecur"},    /* Enable bit of max-current dac                     , */\
   { 0x4480, "enbl_voutcomp"},    /* Enable vout comparators                           , */\
   { 0x4490, "enbl_voutcomp93"},    /* Enable vout-93 comparators                        , */\
   { 0x44a0, "enbl_voutcomp86"},    /* Enable vout-86 comparators                        , */\
   { 0x44b0, "enbl_hizcom"},    /* Enable hiz comparator                             , */\
   { 0x44c0, "enbl_peakcur"},    /* Enable peak current                               , */\
   { 0x44d0, "bypass_ovpglitch"},    /* Bypass OVP Glitch Filter                          , */\
   { 0x44e0, "enbl_windac"},    /* Enable window dac                                 , */\
   { 0x44f0, "enbl_powerbst"},    /* Enable line of the powerstage                     , */\
   { 0x4507, "ocp_thr"},    /* OCP threshold level                               , */\
   { 0x4580, "bypass_glitchfilter"},    /* Bypass glitch filter                              , */\
   { 0x4590, "bypass_ovp"},    /* Bypass OVP                                        , */\
   { 0x45a0, "bypass_uvp"},    /* Bypass UVP                                        , */\
   { 0x45b0, "bypass_otp"},    /* Bypass OTP                                        , */\
   { 0x45d0, "bypass_ocpcounter"},    /* Bypass OCP counter                                , */\
   { 0x45e0, "bypass_lost_clk"},    /* Bypass lost clock detector                        , */\
   { 0x45f0, "vpalarm"},    /* vpalarm (UVP/OUP handling)                        , */\
   { 0x4600, "bypass_gc"},    /* Bypasses the CS gain correction                   , */\
   { 0x4610, "cs_gain_control"},    /* Current sense gain control                        , */\
   { 0x4627, "cs_gain"},    /* Current sense gain                                , */\
   { 0x46a0, "bypass_lp"},    /* Bypass the low power filter inside temperature sensor, */\
   { 0x46b0, "bypass_pwmcounter"},    /* Bypass PWM Counter                                , */\
   { 0x46c0, "cs_negfixed"},    /* Current sense does not switch to neg              , */\
   { 0x46d2, "cs_neghyst"},    /* Current sense switches to neg depending on hyseteris level, */\
   { 0x4700, "switch_fb"},    /* Current sense control switch_fb                   , */\
   { 0x4713, "se_hyst"},    /* Current sense control se_hyst                     , */\
   { 0x4754, "se_level"},    /* Current sense control se_level                    , */\
   { 0x47a5, "ktemp"},    /* Current sense control temperature compensation trimming, */\
   { 0x4800, "cs_negin"},    /* Current sense control negin                       , */\
   { 0x4810, "cs_sein"},    /* Current sense control cs_sein                     , */\
   { 0x4820, "cs_coincidence"},    /* Coincidence current sense                         , */\
   { 0x4830, "iddqtestbst"},    /* IDDQ testing in powerstage of DCDC boost converter, */\
   { 0x4840, "coincidencebst"},    /* Switch protection on to prevent simultaneously switching power stages bst and amp, */\
   { 0x4876, "delay_se_neg"},    /* delay of se and neg                               , */\
   { 0x48e1, "cs_ttrack"},    /* Sample and hold track time                        , */\
   { 0x4900, "bypass_clip"},    /* Bypass clip control                               , */\
   { 0x4920, "cf_cgate_off"},    /* Disable clock gating in the coolflux              , */\
   { 0x4940, "clipfast"},    /* Clock selection for HW clipper for battery safeguard, */\
   { 0x4950, "cs_8ohm"},    /* 8 ohm mode for current sense (gain mode)          , */\
   { 0x4974, "delay_clock_sh"},    /* delay_sh, tunes S7H delay                         , */\
   { 0x49c0, "inv_clksh"},    /* Invert the sample/hold clock for current sense ADC, */\
   { 0x49d0, "inv_neg"},    /* Invert neg signal                                 , */\
   { 0x49e0, "inv_se"},    /* Invert se signal                                  , */\
   { 0x49f0, "setse"},    /* Switches between Single Ended and differential mode; 1 is single ended, */\
   { 0x4a12, "adc10_sel"},    /* Select the input to convert the 10b ADC           , */\
   { 0x4a60, "adc10_reset"},    /* Reset for ADC10 - I2C direct control mode         , */\
   { 0x4a81, "adc10_test"},    /* Test mode selection signal for ADC10 - I2C direct control mode, */\
   { 0x4aa0, "bypass_lp_vbat"},    /* LP filter in batt sensor                          , */\
   { 0x4ae0, "dc_offset"},    /* Current sense decimator offset control            , */\
   { 0x4af0, "tsense_hibias"},    /* Bit to set the biasing in temp sensor to high     , */\
   { 0x4b00, "adc13_iset"},    /* MICADC setting of current consumption (debug use only), */\
   { 0x4b14, "adc13_gain"},    /* MICADC gain setting (two's complement format)     , */\
   { 0x4b61, "adc13_slowdel"},    /* MICADC delay setting for internal clock (debug use only), */\
   { 0x4b83, "adc13_offset"},    /* MICADC offset setting                             , */\
   { 0x4bc0, "adc13_bsoinv"},    /* MICADC bit stream output invert mode for test     , */\
   { 0x4bd0, "adc13_resonator_enable"},    /* MICADC give extra SNR with less stability (debug use only), */\
   { 0x4be0, "testmicadc"},    /* Mux at input of MICADC for test purpose           , */\
   { 0x4c0f, "abist_offset"},    /* Offset control for ABIST testing                  , */\
   { 0x4d05, "windac"},    /* For testing direct control windac                 , */\
   { 0x4dc3, "pwm_dcc_cnt"},    /* control pwm duty cycle when enbl_pwm_dcc is 1     , */\
   { 0x4e04, "slopecur"},    /* For testing direct control slopecur               , */\
   { 0x4e50, "ctrl_dem"},    /* Dynamic element matching control, rest of codes are optional, */\
   { 0x4ed0, "enbl_pwm_dcc"},    /* Enable direct control of pwm duty cycle           , */\
   { 0x4f00, "bst_bypass_bstcur"},    /* Bypass control for boost current settings         , */\
   { 0x4f10, "bst_bypass_bstfoldback"},    /* Bypass control for boost foldback                 , */\
   { 0x4f20, "bst_ctrl_azbst"},    /* Control of auto-zeroing of zero current comparator, */\
   { 0x5007, "gain"},    /* Gain setting of the gain multiplier               , */\
   { 0x5081, "sourceb"},    /* PWM OUTB selection control                        , */\
   { 0x50a1, "sourcea"},    /* PWM OUTA selection control                        , */\
   { 0x50c1, "sourcebst"},    /* Sets the source of the pwmbst output to boost converter input for testing, */\
   { 0x50e0, "tdm_enable_loopback"},    /* TDM loopback test                                 , */\
   { 0x5104, "pulselengthbst"},    /* Pulse length setting test input for boost converter, */\
   { 0x5150, "bypasslatchbst"},    /* Bypass latch in boost converter                   , */\
   { 0x5160, "invertbst"},    /* Invert pwmbst test signal                         , */\
   { 0x5174, "pulselength"},    /* Pulse length setting test input for amplifier     , */\
   { 0x51c0, "bypasslatch"},    /* Bypass latch in PWM source selection module       , */\
   { 0x51d0, "invertb"},    /* invert pwmb test signal                           , */\
   { 0x51e0, "inverta"},    /* invert pwma test signal                           , */\
   { 0x51f0, "bypass_ctrlloop"},    /* bypass_ctrlloop bypasses the control loop of the amplifier, */\
   { 0x5210, "test_rdsona"},    /* tbd for rdson testing                             , */\
   { 0x5220, "test_rdsonb"},    /* tbd for rdson testing                             , */\
   { 0x5230, "test_rdsonbst"},    /* tbd for rdson testing                             , */\
   { 0x5240, "test_cvia"},    /* tbd for rdson testing                             , */\
   { 0x5250, "test_cvib"},    /* tbd for rdson testing                             , */\
   { 0x5260, "test_cvibst"},    /* tbd for rdson testing                             , */\
   { 0x5306, "digimuxa_sel"},    /* DigimuxA input selection control (see Digimux list for details), */\
   { 0x5376, "digimuxb_sel"},    /* DigimuxB input selection control (see Digimux list for details), */\
   { 0x5400, "hs_mode"},    /* I2C high speed mode selection control             , */\
   { 0x5412, "test_parametric_io"},    /* Control for parametric tests of IO cells          , */\
   { 0x5440, "enbl_ringo"},    /* Enable ring oscillator control, for test purpose to check with ringo, */\
   { 0x5456, "digimuxc_sel"},    /* DigimuxC input selection control (see Digimux list for details), */\
   { 0x54c0, "dio_ehs"},    /* Slew control for DIO in output mode               , */\
   { 0x54d0, "gainio_ehs"},    /* Slew control for GAINIO in output mode            , */\
   { 0x550d, "enbl_amp"},    /* enbl_amp for testing to enable all analoge blocks in amplifier, */\
   { 0x5600, "use_direct_ctrls"},    /* Use direct controls to overrule several functions for testing - I2C direct control mode, */\
   { 0x5610, "rst_datapath"},    /* Reset datapath during direct control mode         , */\
   { 0x5620, "rst_cgu"},    /* Reset CGU during durect control mode              , */\
   { 0x5637, "enbl_ref"},    /* For testing to enable all analoge blocks in references, */\
   { 0x56b0, "enbl_engage"},    /* Enable output stage amplifier                     , */\
   { 0x56c0, "use_direct_clk_ctrl"},    /* use_direct_clk_ctrl, to overrule several functions direct for testing, */\
   { 0x56d0, "use_direct_pll_ctrl"},    /* use_direct_pll_ctrl, to overrule several functions direct for testing, */\
   { 0x5707, "anamux"},    /* Anamux control                                    , */\
   { 0x57e0, "otptest"},    /* otptest, test mode otp amplifier                  , */\
   { 0x57f0, "reverse"},    /* 1b = Normal mode, slope is controlled             , */\
   { 0x5813, "pll_selr"},    /* PLL pll_selr                                      , */\
   { 0x5854, "pll_selp"},    /* PLL pll_selp                                      , */\
   { 0x58a5, "pll_seli"},    /* PLL pll_seli                                      , */\
   { 0x5950, "pll_mdec_msb"},    /* Most significant bits of pll_mdec[16]             , */\
   { 0x5960, "pll_ndec_msb"},    /* Most significant bits of pll_ndec[9]              , */\
   { 0x5970, "pll_frm"},    /* PLL pll_frm                                       , */\
   { 0x5980, "pll_directi"},    /* PLL pll_directi                                   , */\
   { 0x5990, "pll_directo"},    /* PLL pll_directo                                   , */\
   { 0x59a0, "enbl_pll"},    /* PLL enbl_pll                                      , */\
   { 0x59f0, "pll_bypass"},    /* PLL bypass                                        , */\
   { 0x5a0f, "tsig_freq"},    /* Internal sinus test generator frequency control LSB bits, */\
   { 0x5b02, "tsig_freq_msb"},    /* Select internal sine wave generator, frequency control MSB bits, */\
   { 0x5b30, "inject_tsig"},    /* Control bit to switch to internal sinus test generator, */\
   { 0x5b44, "adc10_prog_sample"},    /* ADC10 program sample setting - I2C direct control mode, */\
   { 0x5c0f, "pll_mdec"},    /* PLL MDEC - I2C direct PLL control mode only       , */\
   { 0x5d06, "pll_pdec"},    /* PLL PDEC - I2C direct PLL control mode only       , */\
   { 0x5d78, "pll_ndec"},    /* PLL NDEC - I2C direct PLL control mode only       , */\
   { 0x6007, "mtpkey1"},    /* 5Ah, 90d To access KEY1_Protected registers (Default for engineering), */\
   { 0x6185, "mtp_ecc_tcin"},    /* MTP ECC TCIN data                                 , */\
   { 0x6203, "mtp_man_address_in"},    /* MTP address from I2C register for read/writing mtp in manual single word mode, */\
   { 0x6260, "mtp_ecc_eeb"},    /* Enable code bit generation (active low!)          , */\
   { 0x6270, "mtp_ecc_ecb"},    /* Enable correction signal (active low!)            , */\
   { 0x6280, "man_copy_mtp_to_iic"},    /* Start copying single word from mtp to I2C mtp register, */\
   { 0x6290, "man_copy_iic_to_mtp"},    /* Start copying single word from I2C mtp register to mtp, */\
   { 0x62a0, "auto_copy_mtp_to_iic"},    /* Start copying all the data from mtp to I2C mtp registers, */\
   { 0x62b0, "auto_copy_iic_to_mtp"},    /* Start copying data from I2C mtp registers to mtp  , */\
   { 0x62d2, "mtp_speed_mode"},    /* MTP speed mode                                    , */\
   { 0x6340, "mtp_direct_enable"},    /* mtp_direct_enable                                 , */\
   { 0x6350, "mtp_direct_wr"},    /* mtp_direct_wr                                     , */\
   { 0x6360, "mtp_direct_rd"},    /* mtp_direct_rd                                     , */\
   { 0x6370, "mtp_direct_rst"},    /* mtp_direct_rst                                    , */\
   { 0x6380, "mtp_direct_ers"},    /* mtp_direct_ers                                    , */\
   { 0x6390, "mtp_direct_prg"},    /* mtp_direct_prg                                    , */\
   { 0x63a0, "mtp_direct_epp"},    /* mtp_direct_epp                                    , */\
   { 0x63b4, "mtp_direct_test"},    /* mtp_direct_test                                   , */\
   { 0x640f, "mtp_man_data_in"},    /* Write data for MTP manual write                   , */\
   { 0x7000, "cf_rst_dsp"},    /* Reset CoolFlux DSP                                , */\
   { 0x7011, "cf_dmem"},    /* Target memory for access                          , */\
   { 0x7030, "cf_aif"},    /* Auto increment flag for memory-address            , */\
   { 0x7040, "cf_int"},    /* CF Interrupt - auto clear                         , */\
   { 0x7087, "cf_req"},    /* CF request for accessing the 8 channels           , */\
   { 0x710f, "cf_madd"},    /* Memory address                                    , */\
   { 0x720f, "cf_mema"},    /* Activate memory access                            , */\
   { 0x7307, "cf_err"},    /* CF error flags                                    , */\
   { 0x7387, "cf_ack"},    /* CF acknowledgement of the requests channels       , */\
   { 0x8000, "calibration_onetime"},    /* Calibration schedule selection                    , */\
   { 0x8010, "calibr_ron_done"},    /* Calibration of RON status bit                     , */\
   { 0x8105, "calibr_vout_offset"},    /* calibr_vout_offset (DCDCoffset) 2's compliment (key1 protected), */\
   { 0x8163, "calibr_delta_gain"},    /* delta gain for vamp (alpha) 2's compliment (key1 protected), */\
   { 0x81a5, "calibr_offs_amp"},    /* offset for vamp (Ampoffset) 2's compliment (key1 protected), */\
   { 0x8207, "calibr_gain_cs"},    /* gain current sense (Imeasalpha) 2's compliment (key1 protected), */\
   { 0x8284, "calibr_temp_offset"},    /* temperature offset 2's compliment (key1 protected), */\
   { 0x82d2, "calibr_temp_gain"},    /* temperature gain 2's compliment (key1 protected)  , */\
   { 0x830f, "calibr_ron"},    /* calibration value of the RON resistance of the coil, */\
   { 0x8505, "type_bits_hw"},    /* bit0 = disable function dcdcoff_mode ($09[7])     , */\
   { 0x8601, "type_bits_1_0_sw"},    /* MTP control SW                                    , */\
   { 0x8681, "type_bits_9_8_sw"},    /* MTP control SW                                    , */\
   { 0x870f, "type_bits2_sw"},    /* MTP-control SW2                                   , */\
   { 0x8806, "htol_iic_addr"},    /* 7-bit I2C address to be used during HTOL testing  , */\
   { 0x8870, "htol_iic_addr_en"},    /* HTOL I2C_Address_Enable                           , */\
   { 0x8881, "ctrl_ovp_response"},    /* OVP response control                              , */\
   { 0x88a0, "disable_ovp_alarm_state"},    /* OVP alarm state control                           , */\
   { 0x88b0, "enbl_stretch_ovp"},    /* OVP alram strech control                          , */\
   { 0x88c0, "cf_debug_mode"},    /* Coolflux debug mode                               , */\
   { 0x8a0f, "production_data1"},    /* production_data1                                  , */\
   { 0x8b0f, "production_data2"},    /* production_data2                                  , */\
   { 0x8c0f, "production_data3"},    /* production_data3                                  , */\
   { 0x8d0f, "production_data4"},    /* production_data4                                  , */\
   { 0x8e0f, "production_data5"},    /* production_data5                                  , */\
   { 0x8f0f, "production_data6"},    /* production_data6                                  , */\
   { 0xffff, "Unknown bitfield enum" }    /* not found */\
};

enum TFA9896_irq {
	TFA9896_irq_vdds = 0,
	TFA9896_irq_plls = 1,
	TFA9896_irq_ds = 2,
	TFA9896_irq_vds = 3,
	TFA9896_irq_uvds = 4,
	TFA9896_irq_cds = 5,
	TFA9896_irq_clks = 6,
	TFA9896_irq_clips = 7,
	TFA9896_irq_mtpb = 8,
	TFA9896_irq_clk = 9,
	TFA9896_irq_spks = 10,
	TFA9896_irq_acs = 11,
	TFA9896_irq_sws = 12,
	TFA9896_irq_wds = 13,
	TFA9896_irq_amps = 14,
	TFA9896_irq_arefs = 15,
	TFA9896_irq_err = 32,
	TFA9896_irq_ack = 33,
	TFA9896_irq_max = 34,
	TFA9896_irq_all = -1 /* all irqs */};

#define TFA9896_IRQ_NAMETABLE static tfaIrqName_t TFA9896IrqNames[]= {\
	{ 0, "VDDS"},\
	{ 1, "PLLS"},\
	{ 2, "DS"},\
	{ 3, "VDS"},\
	{ 4, "UVDS"},\
	{ 5, "CDS"},\
	{ 6, "CLKS"},\
	{ 7, "CLIPS"},\
	{ 8, "MTPB"},\
	{ 9, "CLK"},\
	{ 10, "SPKS"},\
	{ 11, "ACS"},\
	{ 12, "SWS"},\
	{ 13, "WDS"},\
	{ 14, "AMPS"},\
	{ 15, "AREFS"},\
	{ 16, "16"},\
	{ 17, "17"},\
	{ 18, "18"},\
	{ 19, "19"},\
	{ 20, "20"},\
	{ 21, "21"},\
	{ 22, "22"},\
	{ 23, "23"},\
	{ 24, "24"},\
	{ 25, "25"},\
	{ 26, "26"},\
	{ 27, "27"},\
	{ 28, "28"},\
	{ 29, "29"},\
	{ 30, "30"},\
	{ 31, "31"},\
	{ 32, "ERR"},\
	{ 33, "ACK"},\
	{ 34, "34"},\
};
#endif /* _TFA9896_TFAFIELDNAMES_H */
