/** Filename: Tfa1_TfaFieldnames.h
 *  This file was generated automatically on 03/20/2015 at 01:55:46 PM.
 *  Source file: TFA9897N1B_I2C_list_URT_Source_v34.xls
 */
#define TFA9897_I2CVERSION 34
typedef enum nxpTfa1BfEnumList {
    TFA1_BF_VDDS  = 0x0000,    /*!< Power-on-reset flag                                */
    TFA1_BF_PLLS  = 0x0010,    /*!< PLL lock                                           */
    TFA1_BF_OTDS  = 0x0020,    /*!< Over Temperature Protection alarm                  */
    TFA1_BF_OVDS  = 0x0030,    /*!< Over Voltage Protection alarm                      */
    TFA1_BF_UVDS  = 0x0040,    /*!< Under Voltage Protection alarm                     */
    TFA1_BF_OCDS  = 0x0050,    /*!< Over Current Protection alarm                      */
    TFA1_BF_CLKS  = 0x0060,    /*!< Clocks stable flag                                 */
    TFA1_BF_CLIPS = 0x0070,    /*!< Amplifier clipping                                 */
    TFA1_BF_MTPB  = 0x0080,    /*!< MTP busy                                           */
    TFA1_BF_NOCLK = 0x0090,    /*!< Flag lost clock from clock generation unit         */
    TFA1_BF_SPKS  = 0x00a0,    /*!< Speaker error flag                                 */
    TFA1_BF_ACS   = 0x00b0,    /*!< Cold Start flag                                    */
    TFA1_BF_SWS   = 0x00c0,    /*!< Flag Engage                                        */
    TFA1_BF_WDS   = 0x00d0,    /*!< Flag watchdog reset                                */
    TFA1_BF_AMPS  = 0x00e0,    /*!< Amplifier is enabled by manager                    */
    TFA1_BF_AREFS = 0x00f0,    /*!< References are enabled by manager                  */
    TFA1_BF_BATS  = 0x0109,    /*!< Battery voltage readout; 0 .. 5.5 [V]              */
    TFA1_BF_TEMPS = 0x0208,    /*!< Temperature readout from the temperature sensor    */
    TFA1_BF_REV   = 0x030b,    /*!< Device type number is B97                          */
    TFA1_BF_RCV   = 0x0420,    /*!< Enable Receiver Mode                               */
    TFA1_BF_CHS12 = 0x0431,    /*!< Channel Selection TDM input for Coolflux           */
    TFA1_BF_INPLVL = 0x0450,    /*!< Input level selection control                      */
    TFA1_BF_CHSA  = 0x0461,    /*!< Input selection for amplifier                      */
    TFA1_BF_I2SDOE = 0x04b0,    /*!< Enable data output                                 */
    TFA1_BF_AUDFS = 0x04c3,    /*!< Audio sample rate setting                          */
    TFA1_BF_BSSCR = 0x0501,    /*!< Protection Attack Time                             */
    TFA1_BF_BSST  = 0x0523,    /*!< ProtectionThreshold                                */
    TFA1_BF_BSSRL = 0x0561,    /*!< Protection Maximum Reduction                       */
    TFA1_BF_BSSRR = 0x0582,    /*!< Battery Protection Release Time                    */
    TFA1_BF_BSSHY = 0x05b1,    /*!< Battery Protection Hysteresis                      */
    TFA1_BF_BSSR  = 0x05e0,    /*!< battery voltage for I2C read out only              */
    TFA1_BF_BSSBY = 0x05f0,    /*!< bypass clipper battery protection                  */
    TFA1_BF_DPSA  = 0x0600,    /*!< Enable dynamic powerstage activation               */
    TFA1_BF_CFSM  = 0x0650,    /*!< Soft mute in CoolFlux                              */
    TFA1_BF_BSSS  = 0x0670,    /*!< BatSenseSteepness                                  */
    TFA1_BF_VOL   = 0x0687,    /*!< volume control (in CoolFlux)                       */
    TFA1_BF_DCVO  = 0x0702,    /*!< Boost Voltage                                      */
    TFA1_BF_DCMCC = 0x0733,    /*!< Max boost coil current - step of 175 mA            */
    TFA1_BF_DCIE  = 0x07a0,    /*!< Adaptive boost mode                                */
    TFA1_BF_DCSR  = 0x07b0,    /*!< Soft RampUp/Down mode for DCDC controller          */
    TFA1_BF_DCPAVG = 0x07c0,    /*!< ctrl_peak2avg for analog part of DCDC              */
    TFA1_BF_TROS  = 0x0800,    /*!< Select external temperature also the ext_temp will be put on the temp read out  */
    TFA1_BF_EXTTS = 0x0818,    /*!< external temperature setting to be given by host   */
    TFA1_BF_PWDN  = 0x0900,    /*!< Device Mode                                        */
    TFA1_BF_I2CR  = 0x0910,    /*!< I2C Reset                                          */
    TFA1_BF_CFE   = 0x0920,    /*!< Enable CoolFlux                                    */
    TFA1_BF_AMPE  = 0x0930,    /*!< Enable Amplifier                                   */
    TFA1_BF_DCA   = 0x0940,    /*!< EnableBoost                                        */
    TFA1_BF_SBSL  = 0x0950,    /*!< Coolflux configured                                */
    TFA1_BF_AMPC  = 0x0960,    /*!< Selection on how Amplifier is enabled              */
    TFA1_BF_DCDIS = 0x0970,    /*!< DCDC not connected                                 */
    TFA1_BF_PSDR  = 0x0980,    /*!< IDDQ test amplifier                                */
    TFA1_BF_DCCV  = 0x0991,    /*!< Coil Value                                         */
    TFA1_BF_CCFD  = 0x09b0,    /*!< Selection CoolFlux Clock                           */
    TFA1_BF_INTPAD = 0x09c1,    /*!< INT pad configuration control                      */
    TFA1_BF_IPLL  = 0x09e0,    /*!< PLL input reference clock selection                */
    TFA1_BF_MTPK  = 0x0b07,    /*!< 5Ah, 90d To access KEY1_Protected registers (Default for engineering) */
    TFA1_BF_CVFDLY = 0x0c25,    /*!< Fractional delay adjustment between current and voltage sense */
    TFA1_BF_TDMPRF = 0x1011,    /*!< TDM_usecase                                        */
    TFA1_BF_TDMEN = 0x1030,    /*!< TDM interface control                              */
    TFA1_BF_TDMCKINV = 0x1040,    /*!< TDM clock inversion                                */
    TFA1_BF_TDMFSLN = 0x1053,    /*!< TDM FS length                                      */
    TFA1_BF_TDMFSPOL = 0x1090,    /*!< TDM FS polarity                                    */
    TFA1_BF_TDMSAMSZ = 0x10a4,    /*!< TDM Sample Size for all tdm sinks/sources          */
    TFA1_BF_TDMSLOTS = 0x1103,    /*!< Number of slots                                    */
    TFA1_BF_TDMSLLN = 0x1144,    /*!< Slot length                                        */
    TFA1_BF_TDMBRMG = 0x1194,    /*!< Bits remaining                                     */
    TFA1_BF_TDMDDEL = 0x11e0,    /*!< Data delay                                         */
    TFA1_BF_TDMDADJ = 0x11f0,    /*!< Data adjustment                                    */
    TFA1_BF_TDMTXFRM = 0x1201,    /*!< TXDATA format                                      */
    TFA1_BF_TDMUUS0 = 0x1221,    /*!< TXDATA format unused slot sd0                      */
    TFA1_BF_TDMUUS1 = 0x1241,    /*!< TXDATA format unused slot sd1                      */
    TFA1_BF_TDMSI0EN = 0x1270,    /*!< TDM sink0 enable                                   */
    TFA1_BF_TDMSI1EN = 0x1280,    /*!< TDM sink1 enable                                   */
    TFA1_BF_TDMSI2EN = 0x1290,    /*!< TDM sink2 enable                                   */
    TFA1_BF_TDMSO0EN = 0x12a0,    /*!< TDM source0 enable                                 */
    TFA1_BF_TDMSO1EN = 0x12b0,    /*!< TDM source1 enable                                 */
    TFA1_BF_TDMSO2EN = 0x12c0,    /*!< TDM source2 enable                                 */
    TFA1_BF_TDMSI0IO = 0x12d0,    /*!< tdm_sink0_io                                       */
    TFA1_BF_TDMSI1IO = 0x12e0,    /*!< tdm_sink1_io                                       */
    TFA1_BF_TDMSI2IO = 0x12f0,    /*!< tdm_sink2_io                                       */
    TFA1_BF_TDMSO0IO = 0x1300,    /*!< tdm_source0_io                                     */
    TFA1_BF_TDMSO1IO = 0x1310,    /*!< tdm_source1_io                                     */
    TFA1_BF_TDMSO2IO = 0x1320,    /*!< tdm_source2_io                                     */
    TFA1_BF_TDMSI0SL = 0x1333,    /*!< sink0_slot [GAIN IN]                               */
    TFA1_BF_TDMSI1SL = 0x1373,    /*!< sink1_slot [CH1 IN]                                */
    TFA1_BF_TDMSI2SL = 0x13b3,    /*!< sink2_slot [CH2 IN]                                */
    TFA1_BF_TDMSO0SL = 0x1403,    /*!< source0_slot [GAIN OUT]                            */
    TFA1_BF_TDMSO1SL = 0x1443,    /*!< source1_slot [Voltage Sense]                       */
    TFA1_BF_TDMSO2SL = 0x1483,    /*!< source2_slot [Current Sense]                       */
    TFA1_BF_NBCK  = 0x14c3,    /*!< NBCK                                               */
    TFA1_BF_INTOVDDS = 0x2000,    /*!< flag_por_int_out                                   */
    TFA1_BF_INTOPLLS = 0x2010,    /*!< flag_pll_lock_int_out                              */
    TFA1_BF_INTOOTDS = 0x2020,    /*!< flag_otpok_int_out                                 */
    TFA1_BF_INTOOVDS = 0x2030,    /*!< flag_ovpok_int_out                                 */
    TFA1_BF_INTOUVDS = 0x2040,    /*!< flag_uvpok_int_out                                 */
    TFA1_BF_INTOOCDS = 0x2050,    /*!< flag_ocp_alarm_int_out                             */
    TFA1_BF_INTOCLKS = 0x2060,    /*!< flag_clocks_stable_int_out                         */
    TFA1_BF_INTOCLIPS = 0x2070,    /*!< flag_clip_int_out                                  */
    TFA1_BF_INTOMTPB = 0x2080,    /*!< mtp_busy_int_out                                   */
    TFA1_BF_INTONOCLK = 0x2090,    /*!< flag_lost_clk_int_out                              */
    TFA1_BF_INTOSPKS = 0x20a0,    /*!< flag_cf_speakererror_int_out                       */
    TFA1_BF_INTOACS = 0x20b0,    /*!< flag_cold_started_int_out                          */
    TFA1_BF_INTOSWS = 0x20c0,    /*!< flag_engage_int_out                                */
    TFA1_BF_INTOWDS = 0x20d0,    /*!< flag_watchdog_reset_int_out                        */
    TFA1_BF_INTOAMPS = 0x20e0,    /*!< flag_enbl_amp_int_out                              */
    TFA1_BF_INTOAREFS = 0x20f0,    /*!< flag_enbl_ref_int_out                              */
    TFA1_BF_INTOACK = 0x2201,    /*!< Interrupt status register output - Corresponding flag */
    TFA1_BF_INTIVDDS = 0x2300,    /*!< flag_por_int_in                                    */
    TFA1_BF_INTIPLLS = 0x2310,    /*!< flag_pll_lock_int_in                               */
    TFA1_BF_INTIOTDS = 0x2320,    /*!< flag_otpok_int_in                                  */
    TFA1_BF_INTIOVDS = 0x2330,    /*!< flag_ovpok_int_in                                  */
    TFA1_BF_INTIUVDS = 0x2340,    /*!< flag_uvpok_int_in                                  */
    TFA1_BF_INTIOCDS = 0x2350,    /*!< flag_ocp_alarm_int_in                              */
    TFA1_BF_INTICLKS = 0x2360,    /*!< flag_clocks_stable_int_in                          */
    TFA1_BF_INTICLIPS = 0x2370,    /*!< flag_clip_int_in                                   */
    TFA1_BF_INTIMTPB = 0x2380,    /*!< mtp_busy_int_in                                    */
    TFA1_BF_INTINOCLK = 0x2390,    /*!< flag_lost_clk_int_in                               */
    TFA1_BF_INTISPKS = 0x23a0,    /*!< flag_cf_speakererror_int_in                        */
    TFA1_BF_INTIACS = 0x23b0,    /*!< flag_cold_started_int_in                           */
    TFA1_BF_INTISWS = 0x23c0,    /*!< flag_engage_int_in                                 */
    TFA1_BF_INTIWDS = 0x23d0,    /*!< flag_watchdog_reset_int_in                         */
    TFA1_BF_INTIAMPS = 0x23e0,    /*!< flag_enbl_amp_int_in                               */
    TFA1_BF_INTIAREFS = 0x23f0,    /*!< flag_enbl_ref_int_in                               */
    TFA1_BF_INTIACK = 0x2501,    /*!< Interrupt register input                           */
    TFA1_BF_INTENVDDS = 0x2600,    /*!< flag_por_int_enable                                */
    TFA1_BF_INTENPLLS = 0x2610,    /*!< flag_pll_lock_int_enable                           */
    TFA1_BF_INTENOTDS = 0x2620,    /*!< flag_otpok_int_enable                              */
    TFA1_BF_INTENOVDS = 0x2630,    /*!< flag_ovpok_int_enable                              */
    TFA1_BF_INTENUVDS = 0x2640,    /*!< flag_uvpok_int_enable                              */
    TFA1_BF_INTENOCDS = 0x2650,    /*!< flag_ocp_alarm_int_enable                          */
    TFA1_BF_INTENCLKS = 0x2660,    /*!< flag_clocks_stable_int_enable                      */
    TFA1_BF_INTENCLIPS = 0x2670,    /*!< flag_clip_int_enable                               */
    TFA1_BF_INTENMTPB = 0x2680,    /*!< mtp_busy_int_enable                                */
    TFA1_BF_INTENNOCLK = 0x2690,    /*!< flag_lost_clk_int_enable                           */
    TFA1_BF_INTENSPKS = 0x26a0,    /*!< flag_cf_speakererror_int_enable                    */
    TFA1_BF_INTENACS = 0x26b0,    /*!< flag_cold_started_int_enable                       */
    TFA1_BF_INTENSWS = 0x26c0,    /*!< flag_engage_int_enable                             */
    TFA1_BF_INTENWDS = 0x26d0,    /*!< flag_watchdog_reset_int_enable                     */
    TFA1_BF_INTENAMPS = 0x26e0,    /*!< flag_enbl_amp_int_enable                           */
    TFA1_BF_INTENAREFS = 0x26f0,    /*!< flag_enbl_ref_int_enable                           */
    TFA1_BF_INTENACK = 0x2801,    /*!< Interrupt enable register                          */
    TFA1_BF_INTPOLVDDS = 0x2900,    /*!< flag_por_int_pol                                   */
    TFA1_BF_INTPOLPLLS = 0x2910,    /*!< flag_pll_lock_int_pol                              */
    TFA1_BF_INTPOLOTDS = 0x2920,    /*!< flag_otpok_int_pol                                 */
    TFA1_BF_INTPOLOVDS = 0x2930,    /*!< flag_ovpok_int_pol                                 */
    TFA1_BF_INTPOLUVDS = 0x2940,    /*!< flag_uvpok_int_pol                                 */
    TFA1_BF_INTPOLOCDS = 0x2950,    /*!< flag_ocp_alarm_int_pol                             */
    TFA1_BF_INTPOLCLKS = 0x2960,    /*!< flag_clocks_stable_int_pol                         */
    TFA1_BF_INTPOLCLIPS = 0x2970,    /*!< flag_clip_int_pol                                  */
    TFA1_BF_INTPOLMTPB = 0x2980,    /*!< mtp_busy_int_pol                                   */
    TFA1_BF_INTPOLNOCLK = 0x2990,    /*!< flag_lost_clk_int_pol                              */
    TFA1_BF_INTPOLSPKS = 0x29a0,    /*!< flag_cf_speakererror_int_pol                       */
    TFA1_BF_INTPOLACS = 0x29b0,    /*!< flag_cold_started_int_pol                          */
    TFA1_BF_INTPOLSWS = 0x29c0,    /*!< flag_engage_int_pol                                */
    TFA1_BF_INTPOLWDS = 0x29d0,    /*!< flag_watchdog_reset_int_pol                        */
    TFA1_BF_INTPOLAMPS = 0x29e0,    /*!< flag_enbl_amp_int_pol                              */
    TFA1_BF_INTPOLAREFS = 0x29f0,    /*!< flag_enbl_ref_int_pol                              */
    TFA1_BF_INTPOLACK = 0x2b01,    /*!< Interrupt status flags polarity register           */
    TFA1_BF_CLIP  = 0x4900,    /*!< Bypass clip control                                */
    TFA1_BF_CIMTP = 0x62b0,    /*!< start copying all the data from i2cregs_mtp to mtp [Key 2 protected] */
    TFA1_BF_RST   = 0x7000,    /*!< Reset CoolFlux DSP                                 */
    TFA1_BF_DMEM  = 0x7011,    /*!< Target memory for access                           */
    TFA1_BF_AIF   = 0x7030,    /*!< Autoincrement-flag for memory-address              */
    TFA1_BF_CFINT = 0x7040,    /*!< Interrupt CoolFlux DSP                             */
    TFA1_BF_REQ   = 0x7087,    /*!< request for access (8 channels)                    */
    TFA1_BF_REQCMD = 0x7080,    /*!< Firmware event request rpc command                 */
    TFA1_BF_REQRST = 0x7090,    /*!< Firmware event request reset restart               */
    TFA1_BF_REQMIPS = 0x70a0,    /*!< Firmware event request short on mips               */
    TFA1_BF_REQMUTED = 0x70b0,    /*!< Firmware event request mute sequence ready         */
    TFA1_BF_REQVOL = 0x70c0,    /*!< Firmware event request volume ready                */
    TFA1_BF_REQDMG = 0x70d0,    /*!< Firmware event request speaker damage detected     */
    TFA1_BF_REQCAL = 0x70e0,    /*!< Firmware event request calibration completed       */
    TFA1_BF_REQRSV = 0x70f0,    /*!< Firmware event request reserved                    */
    TFA1_BF_MADD  = 0x710f,    /*!< memory-address to be accessed                      */
    TFA1_BF_MEMA  = 0x720f,    /*!< activate memory access (24- or 32-bits data is written/read to/from memory */
    TFA1_BF_ERR   = 0x7307,    /*!< Coolflux error flags                               */
    TFA1_BF_ACK   = 0x7387,    /*!< acknowledge of requests (8 channels)               */
    TFA1_BF_MTPOTC = 0x8000,    /*!< Calibration schedule (key2 protected)              */
    TFA1_BF_MTPEX = 0x8010,    /*!< (key2 protected)                                   */
} nxpTfa1BfEnumList_t;
#define TFA1_NAMETABLE static tfaBfName_t Tfa1DatasheetNames[] = {\
   { 0x0, "VDDS"},    /* Power-on-reset flag                               , */\
   { 0x10, "PLLS"},    /* PLL lock                                          , */\
   { 0x20, "OTDS"},    /* Over Temperature Protection alarm                 , */\
   { 0x30, "OVDS"},    /* Over Voltage Protection alarm                     , */\
   { 0x40, "UVDS"},    /* Under Voltage Protection alarm                    , */\
   { 0x50, "OCDS"},    /* Over Current Protection alarm                     , */\
   { 0x60, "CLKS"},    /* Clocks stable flag                                , */\
   { 0x70, "CLIPS"},    /* Amplifier clipping                                , */\
   { 0x80, "MTPB"},    /* MTP busy                                          , */\
   { 0x90, "NOCLK"},    /* Flag lost clock from clock generation unit        , */\
   { 0xa0, "SPKS"},    /* Speaker error flag                                , */\
   { 0xb0, "ACS"},    /* Cold Start flag                                   , */\
   { 0xc0, "SWS"},    /* Flag Engage                                       , */\
   { 0xd0, "WDS"},    /* Flag watchdog reset                               , */\
   { 0xe0, "AMPS"},    /* Amplifier is enabled by manager                   , */\
   { 0xf0, "AREFS"},    /* References are enabled by manager                 , */\
   { 0x109, "BATS"},    /* Battery voltage readout; 0 .. 5.5 [V]             , */\
   { 0x208, "TEMPS"},    /* Temperature readout from the temperature sensor   , */\
   { 0x30b, "REV"},    /* Device type number is B97                         , */\
   { 0x420, "RCV"},    /* Enable Receiver Mode                              , */\
   { 0x431, "CHS12"},    /* Channel Selection TDM input for Coolflux          , */\
   { 0x450, "INPLVL"},    /* Input level selection control                     , */\
   { 0x461, "CHSA"},    /* Input selection for amplifier                     , */\
   { 0x4b0, "I2SDOE"},    /* Enable data output                                , */\
   { 0x4c3, "AUDFS"},    /* Audio sample rate setting                         , */\
   { 0x501, "SSCR"},    /* Protection Attack Time                            , */\
   { 0x523, "SST"},    /* ProtectionThreshold                               , */\
   { 0x561, "SSRL"},    /* Protection Maximum Reduction                      , */\
   { 0x582, "SSRR"},    /* Battery Protection Release Time                   , */\
   { 0x5b1, "SSHY"},    /* Battery Protection Hysteresis                     , */\
   { 0x5e0, "SSR"},    /* battery voltage for I2C read out only             , */\
   { 0x5f0, "SSBY"},    /* bypass clipper battery protection                 , */\
   { 0x600, "DPSA"},    /* Enable dynamic powerstage activation              , */\
   { 0x650, "CFSM"},    /* Soft mute in CoolFlux                             , */\
   { 0x670, "SSS"},    /* BatSenseSteepness                                 , */\
   { 0x687, "VOL"},    /* volume control (in CoolFlux)                      , */\
   { 0x702, "DCVO"},    /* Boost Voltage                                     , */\
   { 0x733, "DCMCC"},    /* Max boost coil current - step of 175 mA           , */\
   { 0x7a0, "DCIE"},    /* Adaptive boost mode                               , */\
   { 0x7b0, "DCSR"},    /* Soft RampUp/Down mode for DCDC controller         , */\
   { 0x7c0, "DCPAVG"},    /* ctrl_peak2avg for analog part of DCDC             , */\
   { 0x800, "TROS"},    /* Select external temperature also the ext_temp will be put on the temp read out , */\
   { 0x818, "EXTTS"},    /* external temperature setting to be given by host  , */\
   { 0x900, "PWDN"},    /* Device Mode                                       , */\
   { 0x910, "I2CR"},    /* I2C Reset                                         , */\
   { 0x920, "CFE"},    /* Enable CoolFlux                                   , */\
   { 0x930, "AMPE"},    /* Enable Amplifier                                  , */\
   { 0x940, "DCA"},    /* EnableBoost                                       , */\
   { 0x950, "SBSL"},    /* Coolflux configured                               , */\
   { 0x960, "AMPC"},    /* Selection on how Amplifier is enabled             , */\
   { 0x970, "DCDIS"},    /* DCDC not connected                                , */\
   { 0x980, "PSDR"},    /* IDDQ test amplifier                               , */\
   { 0x991, "DCCV"},    /* Coil Value                                        , */\
   { 0x9b0, "CCFD"},    /* Selection CoolFlux Clock                          , */\
   { 0x9c1, "INTPAD"},    /* INT pad configuration control                     , */\
   { 0x9e0, "IPLL"},    /* PLL input reference clock selection               , */\
   { 0xb07, "MTPK"},    /* 5Ah, 90d To access KEY1_Protected registers (Default for engineering), */\
   { 0xc25, "CVFDLY"},    /* Fractional delay adjustment between current and voltage sense, */\
   { 0x1011, "TDMPRF"},    /* TDM_usecase                                       , */\
   { 0x1030, "TDMEN"},    /* TDM interface control                             , */\
   { 0x1040, "TDMCKINV"},    /* TDM clock inversion                               , */\
   { 0x1053, "TDMFSLN"},    /* TDM FS length                                     , */\
   { 0x1090, "TDMFSPOL"},    /* TDM FS polarity                                   , */\
   { 0x10a4, "TDMSAMSZ"},    /* TDM Sample Size for all tdm sinks/sources         , */\
   { 0x1103, "TDMSLOTS"},    /* Number of slots                                   , */\
   { 0x1144, "TDMSLLN"},    /* Slot length                                       , */\
   { 0x1194, "TDMBRMG"},    /* Bits remaining                                    , */\
   { 0x11e0, "TDMDDEL"},    /* Data delay                                        , */\
   { 0x11f0, "TDMDADJ"},    /* Data adjustment                                   , */\
   { 0x1201, "TDMTXFRM"},    /* TXDATA format                                     , */\
   { 0x1221, "TDMUUS0"},    /* TXDATA format unused slot sd0                     , */\
   { 0x1241, "TDMUUS1"},    /* TXDATA format unused slot sd1                     , */\
   { 0x1270, "TDMSI0EN"},    /* TDM sink0 enable                                  , */\
   { 0x1280, "TDMSI1EN"},    /* TDM sink1 enable                                  , */\
   { 0x1290, "TDMSI2EN"},    /* TDM sink2 enable                                  , */\
   { 0x12a0, "TDMSO0EN"},    /* TDM source0 enable                                , */\
   { 0x12b0, "TDMSO1EN"},    /* TDM source1 enable                                , */\
   { 0x12c0, "TDMSO2EN"},    /* TDM source2 enable                                , */\
   { 0x12d0, "TDMSI0IO"},    /* tdm_sink0_io                                      , */\
   { 0x12e0, "TDMSI1IO"},    /* tdm_sink1_io                                      , */\
   { 0x12f0, "TDMSI2IO"},    /* tdm_sink2_io                                      , */\
   { 0x1300, "TDMSO0IO"},    /* tdm_source0_io                                    , */\
   { 0x1310, "TDMSO1IO"},    /* tdm_source1_io                                    , */\
   { 0x1320, "TDMSO2IO"},    /* tdm_source2_io                                    , */\
   { 0x1333, "TDMSI0SL"},    /* sink0_slot [GAIN IN]                              , */\
   { 0x1373, "TDMSI1SL"},    /* sink1_slot [CH1 IN]                               , */\
   { 0x13b3, "TDMSI2SL"},    /* sink2_slot [CH2 IN]                               , */\
   { 0x1403, "TDMSO0SL"},    /* source0_slot [GAIN OUT]                           , */\
   { 0x1443, "TDMSO1SL"},    /* source1_slot [Voltage Sense]                      , */\
   { 0x1483, "TDMSO2SL"},    /* source2_slot [Current Sense]                      , */\
   { 0x14c3, "NBCK"},    /* NBCK                                              , */\
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
   { 0x2201, "INTOACK"},    /* Interrupt status register output - Corresponding flag, */\
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
   { 0x2501, "INTIACK"},    /* Interrupt register input                          , */\
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
   { 0x2801, "INTENACK"},    /* Interrupt enable register                         , */\
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
   { 0x2b01, "INTPOLACK"},    /* Interrupt status flags polarity register          , */\
   { 0x4900, "CLIP"},    /* Bypass clip control                               , */\
   { 0x62b0, "CIMTP"},    /* start copying all the data from i2cregs_mtp to mtp [Key 2 protected], */\
   { 0x7000, "RST"},    /* Reset CoolFlux DSP                                , */\
   { 0x7011, "DMEM"},    /* Target memory for access                          , */\
   { 0x7030, "AIF"},    /* Autoincrement-flag for memory-address             , */\
   { 0x7040, "CFINT"},    /* Interrupt CoolFlux DSP                            , */\
   { 0x7087, "REQ"},    /* request for access (8 channels)                   , */\
   { 0x7080, "REQCMD"},    /* Firmware event request rpc command                , */\
   { 0x7090, "REQRST"},    /* Firmware event request reset restart              , */\
   { 0x70a0, "REQMIPS"},    /* Firmware event request short on mips              , */\
   { 0x70b0, "REQMUTED"},    /* Firmware event request mute sequence ready        , */\
   { 0x70c0, "REQVOL"},    /* Firmware event request volume ready               , */\
   { 0x70d0, "REQDMG"},    /* Firmware event request speaker damage detected    , */\
   { 0x70e0, "REQCAL"},    /* Firmware event request calibration completed      , */\
   { 0x70f0, "REQRSV"},    /* Firmware event request reserved                   , */\
   { 0x710f, "MADD"},    /* memory-address to be accessed                     , */\
   { 0x720f, "MEMA"},    /* activate memory access (24- or 32-bits data is written/read to/from memory, */\
   { 0x7307, "ERR"},    /* Coolflux error flags                              , */\
   { 0x7387, "ACK"},    /* acknowledge of requests (8 channels)              , */\
   { 0x7380, "ACKCMD"},    /* Firmware event acknowledge rpc command            , */\
   { 0x7390, "ACKRST"},    /* Firmware event acknowledge reset restart          , */\
   { 0x73a0, "ACKMIPS"},    /* Firmware event acknowledge short on mips          , */\
   { 0x73b0, "ACKMUTED"},    /* Firmware event acknowledge mute sequence ready    , */\
   { 0x73c0, "ACKVOL"},    /* Firmware event acknowledge volume ready           , */\
   { 0x73d0, "ACKDMG"},    /* Firmware event acknowledge speaker damage detected, */\
   { 0x73e0, "ACKCAL"},    /* Firmware event acknowledge calibration completed  , */\
   { 0x73f0, "ACKRSV"},    /* Firmware event acknowledge reserved               , */\
   { 0x8000, "MTPOTC"},    /* Calibration schedule (key2 protected)             , */\
   { 0x8010, "MTPEX"},    /* (key2 protected)                                  , */\
   { 0x8045, "SWPROFIL" },\
   { 0x80a5, "SWVSTEP" },\
   { 0xffff, "Unknown bitfield enum" }   /* not found */\
};

#define TFA1_BITNAMETABLE static tfaBfName_t Tfa1BitNames[] = {\
   { 0x0, "flag_por"},    /* Power-on-reset flag                               , */\
   { 0x10, "flag_pll_lock"},    /* PLL lock                                          , */\
   { 0x20, "flag_otpok"},    /* Over Temperature Protection alarm                 , */\
   { 0x30, "flag_ovpok"},    /* Over Voltage Protection alarm                     , */\
   { 0x40, "flag_uvpok"},    /* Under Voltage Protection alarm                    , */\
   { 0x50, "flag_ocp_alarm"},    /* Over Current Protection alarm                     , */\
   { 0x60, "flag_clocks_stable"},    /* Clocks stable flag                                , */\
   { 0x70, "flag_clip"},    /* Amplifier clipping                                , */\
   { 0x80, "mtp_busy"},    /* MTP busy                                          , */\
   { 0x90, "flag_lost_clk"},    /* Flag lost clock from clock generation unit        , */\
   { 0xa0, "flag_cf_speakererror"},    /* Speaker error flag                                , */\
   { 0xb0, "flag_cold_started"},    /* Cold Start flag                                   , */\
   { 0xc0, "flag_engage"},    /* Flag Engage                                       , */\
   { 0xd0, "flag_watchdog_reset"},    /* Flag watchdog reset                               , */\
   { 0xe0, "flag_enbl_amp"},    /* Amplifier is enabled by manager                   , */\
   { 0xf0, "flag_enbl_ref"},    /* References are enabled by manager                 , */\
   { 0x109, "bat_adc"},    /* Battery voltage readout; 0 .. 5.5 [V]             , */\
   { 0x208, "temp_adc"},    /* Temperature readout from the temperature sensor   , */\
   { 0x30b, "rev_reg"},    /* Device type number is B97                         , */\
   { 0x420, "ctrl_rcv"},    /* Enable Receiver Mode                              , */\
   { 0x431, "chan_sel"},    /* Channel Selection TDM input for Coolflux          , */\
   { 0x450, "input_level"},    /* Input level selection control                     , */\
   { 0x461, "vamp_sel"},    /* Input selection for amplifier                     , */\
   { 0x4c3, "audio_fs"},    /* Audio sample rate setting                         , */\
   { 0x501, "vbat_prot_attacktime"},    /* Protection Attack Time                            , */\
   { 0x523, "vbat_prot_thlevel"},    /* ProtectionThreshold                               , */\
   { 0x561, "vbat_prot_max_reduct"},    /* Protection Maximum Reduction                      , */\
   { 0x582, "vbat_prot_release_t"},    /* Battery Protection Release Time                   , */\
   { 0x5b1, "vbat_prot_hysterese"},    /* Battery Protection Hysteresis                     , */\
   { 0x5d0, "reset_min_vbat"},    /* reset clipper                                     , */\
   { 0x5e0, "sel_vbat"},    /* battery voltage for I2C read out only             , */\
   { 0x5f0, "bypass_clipper"},    /* bypass clipper battery protection                 , */\
   { 0x600, "dpsa"},    /* Enable dynamic powerstage activation              , */\
   { 0x650, "cf_mute"},    /* Soft mute in CoolFlux                             , */\
   { 0x670, "batsense_steepness"},    /* BatSenseSteepness                                 , */\
   { 0x687, "vol"},    /* volume control (in CoolFlux)                      , */\
   { 0x702, "boost_volt"},    /* Boost Voltage                                     , */\
   { 0x733, "boost_cur"},    /* Max boost coil current - step of 175 mA           , */\
   { 0x7a0, "boost_intel"},    /* Adaptive boost mode                               , */\
   { 0x7b0, "boost_speed"},    /* Soft RampUp/Down mode for DCDC controller         , */\
   { 0x7c0, "boost_peak2avg"},    /* ctrl_peak2avg for analog part of DCDC             , */\
   { 0x800, "ext_temp_sel"},    /* Select external temperature also the ext_temp will be put on the temp read out , */\
   { 0x818, "ext_temp"},    /* external temperature setting to be given by host  , */\
   { 0x8b2, "dcdc_synchronisation"},    /* DCDC synchronisation off + 7 positions            , */\
   { 0x900, "powerdown"},    /* Device Mode                                       , */\
   { 0x910, "reset"},    /* I2C Reset                                         , */\
   { 0x920, "enbl_coolflux"},    /* Enable CoolFlux                                   , */\
   { 0x930, "enbl_amplifier"},    /* Enable Amplifier                                  , */\
   { 0x940, "enbl_boost"},    /* EnableBoost                                       , */\
   { 0x950, "coolflux_configured"},    /* Coolflux configured                               , */\
   { 0x960, "sel_enbl_amplifier"},    /* Selection on how Amplifier is enabled             , */\
   { 0x970, "dcdcoff_mode"},    /* DCDC not connected                                , */\
   { 0x980, "iddqtest"},    /* IDDQ test amplifier                               , */\
   { 0x991, "coil_value"},    /* Coil Value                                        , */\
   { 0x9b0, "sel_cf_clock"},    /* Selection CoolFlux Clock                          , */\
   { 0x9c1, "int_pad_io"},    /* INT pad configuration control                     , */\
   { 0x9e0, "sel_fs_bck"},    /* PLL input reference clock selection               , */\
   { 0x9f0, "sel_scl_cf_clock"},    /* Coolflux sub-system clock                         , */\
   { 0xb07, "mtpkey2"},    /* 5Ah, 90d To access KEY1_Protected registers (Default for engineering), */\
   { 0xc00, "enbl_volt_sense"},    /* Voltage sense enabling control bit                , */\
   { 0xc10, "vsense_pwm_sel"},    /* Voltage sense PWM source selection control        , */\
   { 0xc25, "vi_frac_delay"},    /* Fractional delay adjustment between current and voltage sense, */\
   { 0xc80, "sel_voltsense_out"},    /* TDM output data selection control                 , */\
   { 0xc90, "vsense_bypass_avg"},    /* Voltage Sense Average Block Bypass                , */\
   { 0xd05, "cf_frac_delay"},    /* Fractional delay adjustment between current and voltage sense by firmware, */\
   { 0xe00, "bypass_dcdc_curr_prot"},    /* Control to switch off dcdc current reduction with bat protection, */\
   { 0xe80, "disable_clock_sh_prot"},    /* disable clock_sh protection                       , */\
   { 0xe96, "reserve_reg_1_15_9"},    /*                                                   , */\
   { 0x1011, "tdm_usecase"},    /* TDM_usecase                                       , */\
   { 0x1030, "tdm_enable"},    /* TDM interface control                             , */\
   { 0x1040, "tdm_clk_inversion"},    /* TDM clock inversion                               , */\
   { 0x1053, "tdm_fs_ws_length"},    /* TDM FS length                                     , */\
   { 0x1090, "tdm_fs_ws_polarity"},    /* TDM FS polarity                                   , */\
   { 0x10a4, "tdm_sample_size"},    /* TDM Sample Size for all tdm sinks/sources         , */\
   { 0x1103, "tdm_nb_of_slots"},    /* Number of slots                                   , */\
   { 0x1144, "tdm_slot_length"},    /* Slot length                                       , */\
   { 0x1194, "tdm_bits_remaining"},    /* Bits remaining                                    , */\
   { 0x11e0, "tdm_data_delay"},    /* Data delay                                        , */\
   { 0x11f0, "tdm_data_adjustment"},    /* Data adjustment                                   , */\
   { 0x1201, "tdm_txdata_format"},    /* TXDATA format                                     , */\
   { 0x1221, "tdm_txdata_format_unused_slot_sd0"},    /* TXDATA format unused slot sd0                     , */\
   { 0x1241, "tdm_txdata_format_unused_slot_sd1"},    /* TXDATA format unused slot sd1                     , */\
   { 0x1270, "tdm_sink0_enable"},    /* TDM sink0 enable                                  , */\
   { 0x1280, "tdm_sink1_enable"},    /* TDM sink1 enable                                  , */\
   { 0x1290, "tdm_sink2_enable"},    /* TDM sink2 enable                                  , */\
   { 0x12a0, "tdm_source0_enable"},    /* TDM source0 enable                                , */\
   { 0x12b0, "tdm_source1_enable"},    /* TDM source1 enable                                , */\
   { 0x12c0, "tdm_source2_enable"},    /* TDM source2 enable                                , */\
   { 0x12d0, "tdm_sink0_io"},    /* tdm_sink0_io                                      , */\
   { 0x12e0, "tdm_sink1_io"},    /* tdm_sink1_io                                      , */\
   { 0x12f0, "tdm_sink2_io"},    /* tdm_sink2_io                                      , */\
   { 0x1300, "tdm_source0_io"},    /* tdm_source0_io                                    , */\
   { 0x1310, "tdm_source1_io"},    /* tdm_source1_io                                    , */\
   { 0x1320, "tdm_source2_io"},    /* tdm_source2_io                                    , */\
   { 0x1333, "tdm_sink0_slot"},    /* sink0_slot [GAIN IN]                              , */\
   { 0x1373, "tdm_sink1_slot"},    /* sink1_slot [CH1 IN]                               , */\
   { 0x13b3, "tdm_sink2_slot"},    /* sink2_slot [CH2 IN]                               , */\
   { 0x1403, "tdm_source0_slot"},    /* source0_slot [GAIN OUT]                           , */\
   { 0x1443, "tdm_source1_slot"},    /* source1_slot [Voltage Sense]                      , */\
   { 0x1483, "tdm_source2_slot"},    /* source2_slot [Current Sense]                      , */\
   { 0x14c3, "tdm_nbck"},    /* NBCK                                              , */\
   { 0x1500, "flag_tdm_lut_error"},    /* TDM LUT error flag                                , */\
   { 0x1512, "flag_tdm_status"},    /* TDM interface status bits                         , */\
   { 0x1540, "flag_tdm_error"},    /* TDM interface error indicator                     , */\
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
   { 0x2201, "interrupt_out3"},    /* Interrupt status register output - Corresponding flag, */\
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
   { 0x2501, "interrupt_in3"},    /* Interrupt register input                          , */\
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
   { 0x2801, "interrupt_enable3"},    /* Interrupt enable register                         , */\
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
   { 0x2b01, "status_polarity3"},    /* Interrupt status flags polarity register          , */\
   { 0x3000, "flag_voutcomp"},    /* flag_voutcomp, indication Vset is larger than Vbat, */\
   { 0x3010, "flag_voutcomp93"},    /* flag_voutcomp93, indication Vset is larger than 1.07* Vbat, */\
   { 0x3020, "flag_voutcomp86"},    /* flag_voutcomp86, indication Vset is larger than 1.14* Vbat, */\
   { 0x3030, "flag_hiz"},    /* flag_hiz, indication Vbst is larger than  Vbat    , */\
   { 0x3040, "flag_ocpokbst"},    /* flag_ocpokbst, indication no over current in boost converter pmos switch, */\
   { 0x3050, "flag_peakcur"},    /* flag_peakcur, indication current is max in dcdc converter, */\
   { 0x3060, "flag_ocpokap"},    /* flag_ocpokap, indication no over current in amplifier "a" pmos output stage, */\
   { 0x3070, "flag_ocpokan"},    /* flag_ocpokan, indication no over current in amplifier "a" nmos output stage, */\
   { 0x3080, "flag_ocpokbp"},    /* flag_ocpokbp, indication no over current in amplifier "b" pmos output stage, */\
   { 0x3090, "flag_ocpokbn"},    /* flag_ocpokbn, indication no over current in amplifier"b" nmos output stage, */\
   { 0x30a0, "flag_adc10_ready"},    /* flag_adc10_ready, indication adc10 is ready       , */\
   { 0x30b0, "flag_clipa_high"},    /* flag_clipa_high, indication pmos amplifier "a" is clipping, */\
   { 0x30c0, "flag_clipa_low"},    /* flag_clipa_low, indication nmos amplifier "a" is clipping, */\
   { 0x30d0, "flag_clipb_high"},    /* flag_clipb_high, indication pmos amplifier "b" is clipping, */\
   { 0x30e0, "flag_clipb_low"},    /* flag_clipb_low, indication nmos amplifier "b" is clipping, */\
   { 0x310f, "mtp_man_data_out"},    /* single word read from MTP (manual copy)           , */\
   { 0x3200, "key01_locked"},    /* key01_locked, indication key 1 is locked          , */\
   { 0x3210, "key02_locked"},    /* key02_locked, indication key 2 is locked          , */\
   { 0x3225, "mtp_ecc_tcout"},    /* mtp_ecc_tcout                                     , */\
   { 0x3280, "mtpctrl_valid_test_rd"},    /* mtp test readout for read                         , */\
   { 0x3290, "mtpctrl_valid_test_wr"},    /* mtp test readout for write                        , */\
   { 0x32a0, "flag_in_alarm_state"},    /* Alarm state                                       , */\
   { 0x32b0, "mtp_ecc_err2"},    /* two or more bit errors detected in MTP, can not reconstruct value, */\
   { 0x32c0, "mtp_ecc_err1"},    /* one bit error detected in MTP, reconstructed value, */\
   { 0x32d0, "mtp_mtp_hvf"},    /* high voltage ready flag for MTP                   , */\
   { 0x32f0, "mtp_zero_check_fail"},    /* zero check failed (tbd) for MTP                   , */\
   { 0x3309, "data_adc10_tempbat"},    /* data_adc10_tempbat[9;0], adc 10 data output for testing, */\
   { 0x400f, "hid_code"},    /* 5A6Bh, 23147d to access registers (Default for engineering), */\
   { 0x4100, "bypass_hp"},    /* Bypass_High Pass Filter                           , */\
   { 0x4110, "hard_mute"},    /* Hard Mute                                         , */\
   { 0x4120, "soft_mute"},    /* Soft Mute                                         , */\
   { 0x4134, "pwm_delay"},    /* PWM DelayBits to set the delay                    , */\
   { 0x4180, "pwm_shape"},    /* PWM Shape                                         , */\
   { 0x4190, "pwm_bitlength"},    /* PWM Bitlength in noise shaper                     , */\
   { 0x4203, "drive"},    /* Drive bits to select amount of power stage amplifier, */\
   { 0x4240, "reclock_pwm"},    /*                                                   , */\
   { 0x4250, "reclock_voltsense"},    /*                                                   , */\
   { 0x4281, "dpsalevel"},    /* DPSA Threshold level                              , */\
   { 0x42a1, "dpsa_release"},    /* DPSA Release time                                 , */\
   { 0x42c0, "coincidence"},    /* Prevent simultaneously switching of output stage  , */\
   { 0x42d0, "kickback"},    /* Prevent double pulses of output stage             , */\
   { 0x4306, "drivebst"},    /* Drive bits to select the powertransistor sections boost converter, */\
   { 0x43a0, "ocptestbst"},    /* Boost OCP. For old ocp (ctrl_reversebst is 0);For new ocp (ctrl_reversebst is 1);, */\
   { 0x43d0, "test_abistfft_enbl"},    /* FFT coolflux                                      , */\
   { 0x43f0, "test_bcontrol"},    /* test _bcontrol                                    , */\
   { 0x4400, "reversebst"},    /* OverCurrent Protection selection of power stage boost converter, */\
   { 0x4410, "sensetest"},    /* Test option for the sense NMOS in booster for current mode control., */\
   { 0x4420, "enbl_engagebst"},    /* Enable power stage dcdc controller                , */\
   { 0x4470, "enbl_slopecur"},    /* Enable bit of max-current dac                     , */\
   { 0x4480, "enbl_voutcomp"},    /* Enable vout comparators                           , */\
   { 0x4490, "enbl_voutcomp93"},    /* Enable vout-93 comparators                        , */\
   { 0x44a0, "enbl_voutcomp86"},    /* Enable vout-86 comparators                        , */\
   { 0x44b0, "enbl_hizcom"},    /* Enable hiz comparator                             , */\
   { 0x44c0, "enbl_peakcur"},    /* Enable peak current                               , */\
   { 0x44d0, "bypass_ovpglitch"},    /* Bypass OVP Glitch Filter                          , */\
   { 0x44e0, "enbl_windac"},    /* Enable window dac                                 , */\
   { 0x44f0, "enbl_powerbst"},    /* Enable line of the powerstage                     , */\
   { 0x4507, "ocp_thr"},    /* ocp_thr threshold level for OCP                   , */\
   { 0x4580, "bypass_glitchfilter"},    /* Bypass glitch filter                              , */\
   { 0x4590, "bypass_ovp"},    /* Bypass OVP                                        , */\
   { 0x45a0, "bypass_uvp"},    /* Bypass UVP                                        , */\
   { 0x45b0, "bypass_otp"},    /* Bypass OTP                                        , */\
   { 0x45c0, "bypass_ocp"},    /* Bypass OCP                                        , */\
   { 0x45d0, "bypass_ocpcounter"},    /* BypassOCPCounter                                  , */\
   { 0x45e0, "bypass_lost_clk"},    /* Bypasslost_clk detector                           , */\
   { 0x45f0, "vpalarm"},    /* vpalarm (uvp ovp handling)                        , */\
   { 0x4600, "bypass_gc"},    /* bypass_gc, bypasses the CS gain correction        , */\
   { 0x4610, "cs_gain_control"},    /* gain control by means of MTP or i2c; 0 is MTP     , */\
   { 0x4627, "cs_gain"},    /* + / - 128 steps in steps of 1/4 percent  2's compliment, */\
   { 0x46a0, "bypass_lp"},    /* bypass Low-Pass filter in temperature sensor      , */\
   { 0x46b0, "bypass_pwmcounter"},    /* bypass_pwmcounter                                 , */\
   { 0x46c0, "cs_negfixed"},    /* does not switch to neg                            , */\
   { 0x46d2, "cs_neghyst"},    /* switches to neg depending on level                , */\
   { 0x4700, "switch_fb"},    /* switch_fb                                         , */\
   { 0x4713, "se_hyst"},    /* se_hyst                                           , */\
   { 0x4754, "se_level"},    /* se_level                                          , */\
   { 0x47a5, "ktemp"},    /* temperature compensation trimming                 , */\
   { 0x4800, "cs_negin"},    /* negin                                             , */\
   { 0x4810, "cs_sein"},    /* cs_sein                                           , */\
   { 0x4820, "cs_coincidence"},    /* Coincidence current sense                         , */\
   { 0x4830, "iddqtestbst"},    /* for iddq testing in powerstage of boost convertor , */\
   { 0x4840, "coincidencebst"},    /* Switch protection on to prevent simultaneously switching power stages bst and amp, */\
   { 0x4876, "delay_se_neg"},    /* delay of se and neg                               , */\
   { 0x48e1, "cs_ttrack"},    /* sample & hold track time                          , */\
   { 0x4900, "bypass_clip"},    /* Bypass clip control                               , */\
   { 0x4920, "cf_cgate_off"},    /* to disable clock gating in the coolflux           , */\
   { 0x4940, "clipfast"},    /* clock switch for battery protection clipper, it switches back to old frequency, */\
   { 0x4950, "cs_8ohm"},    /* 8 ohm mode for current sense (gain mode)          , */\
   { 0x4974, "delay_clock_sh"},    /* delay_sh, tunes S7H delay                         , */\
   { 0x49c0, "inv_clksh"},    /* Invert the sample/hold clock for current sense ADC, */\
   { 0x49d0, "inv_neg"},    /* Invert neg signal                                 , */\
   { 0x49e0, "inv_se"},    /* Invert se signal                                  , */\
   { 0x49f0, "setse"},    /* switches between Single Ende and differential mode; 1 is single ended, */\
   { 0x4a12, "adc10_sel"},    /* select the input to convert the 10b ADC           , */\
   { 0x4a60, "adc10_reset"},    /* Global asynchronous reset (active HIGH) 10 bit ADC, */\
   { 0x4a81, "adc10_test"},    /* Test mode selection signal 10 bit ADC             , */\
   { 0x4aa0, "bypass_lp_vbat"},    /* lp filter in batt sensor                          , */\
   { 0x4ae0, "dc_offset"},    /* switch offset control on/off, is decimator offset control, */\
   { 0x4af0, "tsense_hibias"},    /* bit to set the biasing in temp sensor to high     , */\
   { 0x4b00, "adc13_iset"},    /* Micadc Setting of current consumption. Debug use only, */\
   { 0x4b14, "adc13_gain"},    /* Micadc gain setting (2-compl)                     , */\
   { 0x4b61, "adc13_slowdel"},    /* Micadc Delay setting for internal clock. Debug use only, */\
   { 0x4b83, "adc13_offset"},    /* Micadc ADC offset setting                         , */\
   { 0x4bc0, "adc13_bsoinv"},    /* Micadc bit stream output invert mode for test     , */\
   { 0x4bd0, "adc13_resonator_enable"},    /* Micadc Give extra SNR with less stability. Debug use only, */\
   { 0x4be0, "testmicadc"},    /* Mux at input of MICADC for test purpose           , */\
   { 0x4c0f, "abist_offset"},    /* offset control for ABIST testing                  , */\
   { 0x4d05, "windac"},    /* for testing direct control windac                 , */\
   { 0x4dc3, "pwm_dcc_cnt"},    /* control pwm duty cycle when enbl_pwm_dcc is 1     , */\
   { 0x4e04, "slopecur"},    /* for testing direct control slopecur               , */\
   { 0x4e50, "ctrl_dem"},    /* dyn element matching control, rest of codes are optional, */\
   { 0x4ed0, "enbl_pwm_dcc"},    /* to enable direct control of pwm duty cycle        , */\
   { 0x5007, "gain"},    /* Gain setting of the gain multiplier               , */\
   { 0x5081, "sourceb"},    /* Set OUTB to                                       , */\
   { 0x50a1, "sourcea"},    /* Set OUTA to                                       , */\
   { 0x50c1, "sourcebst"},    /* Sets the source of the pwmbst output to boost converter input for testing, */\
   { 0x50e0, "tdm_enable_loopback"},    /* TDM loopback test                                 , */\
   { 0x5104, "pulselengthbst"},    /* pulse length setting test input for boost converter, */\
   { 0x5150, "bypasslatchbst"},    /* bypass_latch in boost converter                   , */\
   { 0x5160, "invertbst"},    /* invert pwmbst test signal                         , */\
   { 0x5174, "pulselength"},    /* pulse length setting test input for amplifier     , */\
   { 0x51c0, "bypasslatch"},    /* bypass_latch in PWM source selection module       , */\
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
   { 0x5400, "hs_mode"},    /* hs_mode, high speed mode I2C bus                  , */\
   { 0x5412, "test_parametric_io"},    /* test_parametric_io for testing pads               , */\
   { 0x5440, "enbl_ringo"},    /* enbl_ringo, for test purpose to check with ringo  , */\
   { 0x5456, "digimuxc_sel"},    /* DigimuxC input selection control (see Digimux list for details), */\
   { 0x54c0, "dio_ehs"},    /* Slew control for DIO in output mode               , */\
   { 0x54d0, "gainio_ehs"},    /* Slew control for GAINIO in output mode            , */\
   { 0x550d, "enbl_amp"},    /* enbl_amp for testing to enable all analoge blocks in amplifier, */\
   { 0x5600, "use_direct_ctrls"},    /* use_direct_ctrls, to overrule several functions direct for testing, */\
   { 0x5610, "rst_datapath"},    /* rst_datapath, datapath reset                      , */\
   { 0x5620, "rst_cgu"},    /* rst_cgu, cgu reset                                , */\
   { 0x5637, "enbl_ref"},    /* for testing to enable all analoge blocks in references, */\
   { 0x56b0, "enbl_engage"},    /* Enable output stage amplifier                     , */\
   { 0x56c0, "use_direct_clk_ctrl"},    /* use_direct_clk_ctrl, to overrule several functions direct for testing, */\
   { 0x56d0, "use_direct_pll_ctrl"},    /* use_direct_pll_ctrl, to overrule several functions direct for testing, */\
   { 0x56e0, "use_direct_ctrls_2"},    /* use_direct_sourseamp_ctrls, to overrule several functions direct for testing, */\
   { 0x5707, "anamux"},    /* Anamux control                                    , */\
   { 0x57c0, "ocptest"},    /* ctrl_ocptest, deactivates the over current protection in the power stages of the amplifier. The ocp flag signals stay active., */\
   { 0x57e0, "otptest"},    /* otptest, test mode otp amplifier                  , */\
   { 0x57f0, "reverse"},    /* 1: Normal mode, slope is controlled               , */\
   { 0x5813, "pll_selr"},    /* pll_selr                                          , */\
   { 0x5854, "pll_selp"},    /* pll_selp                                          , */\
   { 0x58a5, "pll_seli"},    /* pll_seli                                          , */\
   { 0x5950, "pll_mdec_msb"},    /* most significant bits of pll_mdec[16]             , */\
   { 0x5960, "pll_ndec_msb"},    /* most significant bits of pll_ndec[9]              , */\
   { 0x5970, "pll_frm"},    /* pll_frm                                           , */\
   { 0x5980, "pll_directi"},    /* pll_directi                                       , */\
   { 0x5990, "pll_directo"},    /* pll_directo                                       , */\
   { 0x59a0, "enbl_pll"},    /* enbl_pll                                          , */\
   { 0x59f0, "pll_bypass"},    /* pll_bypass                                        , */\
   { 0x5a0f, "tsig_freq"},    /* tsig_freq, internal sinus test generator, frequency control, */\
   { 0x5b02, "tsig_freq_msb"},    /* select internal sinus test generator, frequency control msb bits, */\
   { 0x5b30, "inject_tsig"},    /* inject_tsig, control bit to switch to internal sinus test generator, */\
   { 0x5b44, "adc10_prog_sample"},    /* control ADC10                                     , */\
   { 0x5c0f, "pll_mdec"},    /* bits 15..0 of pll_mdec[16;0]                      , */\
   { 0x5d06, "pll_pdec"},    /* pll_pdec                                          , */\
   { 0x5d78, "pll_ndec"},    /* bits 8..0 of pll_ndec[9;0]                        , */\
   { 0x6007, "mtpkey1"},    /* 5Ah, 90d To access KEY1_Protected registers (Default for engineering), */\
   { 0x6185, "mtp_ecc_tcin"},    /* Mtp_ecc_tcin                                      , */\
   { 0x6203, "mtp_man_address_in"},    /* address from I2C regs for writing one word single mtp, */\
   { 0x6260, "mtp_ecc_eeb"},    /* enable code bit generation (active low!)          , */\
   { 0x6270, "mtp_ecc_ecb"},    /* enable correction signal (active low!)            , */\
   { 0x6280, "man_copy_mtp_to_iic"},    /* start copying single word from mtp to i2cregs_mtp , */\
   { 0x6290, "man_copy_iic_to_mtp"},    /* start copying single word from i2cregs_mtp to mtp [Key 1 protected], */\
   { 0x62a0, "auto_copy_mtp_to_iic"},    /* start copying all the data from mtp to i2cregs_mtp, */\
   { 0x62b0, "auto_copy_iic_to_mtp"},    /* start copying all the data from i2cregs_mtp to mtp [Key 2 protected], */\
   { 0x62d2, "mtp_speed_mode"},    /* Speed mode                                        , */\
   { 0x6340, "mtp_direct_enable"},    /* mtp_direct_enable (key1 protected)                , */\
   { 0x6350, "mtp_direct_wr"},    /* mtp_direct_wr (key1 protected)                    , */\
   { 0x6360, "mtp_direct_rd"},    /* mtp_direct_rd  (key1 protected)                   , */\
   { 0x6370, "mtp_direct_rst"},    /* mtp_direct_rst  (key1 protected)                  , */\
   { 0x6380, "mtp_direct_ers"},    /* mtp_direct_ers  (key1 protected)                  , */\
   { 0x6390, "mtp_direct_prg"},    /* mtp_direct_prg  (key1 protected)                  , */\
   { 0x63a0, "mtp_direct_epp"},    /* mtp_direct_epp  (key1 protected)                  , */\
   { 0x63b4, "mtp_direct_test"},    /* mtp_direct_test  (key1 protected)                 , */\
   { 0x640f, "mtp_man_data_in"},    /* single word to be written to MTP (manual copy)    , */\
   { 0x7000, "cf_rst_dsp"},    /* Reset CoolFlux DSP                                , */\
   { 0x7011, "cf_dmem"},    /* Target memory for access                          , */\
   { 0x7030, "cf_aif"},    /* Autoincrement-flag for memory-address             , */\
   { 0x7040, "cf_int"},    /* Interrupt CoolFlux DSP                            , */\
   { 0x7087, "cf_req"},    /* request for access (8 channels)                   , */\
   { 0x710f, "cf_madd"},    /* memory-address to be accessed                     , */\
   { 0x720f, "cf_mema"},    /* activate memory access (24- or 32-bits data is written/read to/from memory, */\
   { 0x7307, "cf_err"},    /* Coolflux error flags                              , */\
   { 0x7387, "cf_ack"},    /* acknowledge of requests (8 channels)              , */\
   { 0x8000, "calibration_onetime"},    /* Calibration schedule (key2 protected)             , */\
   { 0x8010, "calibr_ron_done"},    /* (key2 protected)                                  , */\
   { 0x8105, "calibr_vout_offset"},    /* calibr_vout_offset (DCDCoffset) 2's compliment (key1 protected), */\
   { 0x8163, "calibr_delta_gain"},    /* delta gain for vamp (alpha) 2's compliment (key1 protected), */\
   { 0x81a5, "calibr_offs_amp"},    /* offset for vamp (Ampoffset) 2's compliment (key1 protected), */\
   { 0x8207, "calibr_gain_cs"},    /* gain current sense (Imeasalpha) 2's compliment (key1 protected), */\
   { 0x8284, "calibr_temp_offset"},    /* temperature offset 2's compliment (key1 protected), */\
   { 0x82d2, "calibr_temp_gain"},    /* temperature gain 2's compliment (key1 protected)  , */\
   { 0x830f, "calibr_ron"},    /* Ron resistance of coil (key1 protected)           , */\
   { 0x8505, "type_bits_HW"},    /* Key1_Protected_MTP5                               , */\
   { 0x8601, "type_bits_1_0_SW"},    /* MTP-control SW                                    , */\
   { 0x8681, "type_bits_8_9_SW"},    /* MTP-control SW                                    , */\
   { 0x870f, "type_bits2_SW"},    /* MTP-control SW2                                   , */\
   { 0x8806, "htol_iic_addr"},    /* 7-bit I2C address to be used during HTOL testing  , */\
   { 0x8870, "htol_iic_addr_en"},    /* HTOL_I2C_Address_Enable                           , */\
   { 0x8881, "ctrl_ovp_response"},    /* OVP response control                              , */\
   { 0x88a0, "disable_ovp_alarm_state"},    /* OVP alarm state control                           , */\
   { 0x88b0, "enbl_stretch_ovp"},    /* OVP alram strech control                          , */\
   { 0x88c0, "cf_debug_mode"},    /* Coolflux debug mode                               , */\
   { 0x8a0f, "production_data1"},    /* (key1 protected)                                  , */\
   { 0x8b0f, "production_data2"},    /* (key1 protected)                                  , */\
   { 0x8c0f, "production_data3"},    /* (key1 protected)                                  , */\
   { 0x8d0f, "production_data4"},    /* (key1 protected)                                  , */\
   { 0x8e0f, "production_data5"},    /* (key1 protected)                                  , */\
   { 0x8f0f, "production_data6"},    /* (key1 protected)                                  , */\
   { 0xffff, "Unknown bitfield enum" }    /* not found */\
};

enum tfa1_irq {
	tfa1_irq_vdds = 0,
	tfa1_irq_plls = 1,
	tfa1_irq_ds = 2,
	tfa1_irq_vds = 3,
	tfa1_irq_uvds = 4,
	tfa1_irq_cds = 5,
	tfa1_irq_clks = 6,
	tfa1_irq_clips = 7,
	tfa1_irq_mtpb = 8,
	tfa1_irq_clk = 9,
	tfa1_irq_spks = 10,
	tfa1_irq_acs = 11,
	tfa1_irq_sws = 12,
	tfa1_irq_wds = 13,
	tfa1_irq_amps = 14,
	tfa1_irq_arefs = 15,
	tfa1_irq_ack = 32,
	tfa1_irq_max = 33,
	tfa1_irq_all = -1 /* all irqs */};

#define TFA1_IRQ_NAMETABLE static tfaIrqName_t Tfa1IrqNames[] = {\
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
	{ 32, "ACK"},\
	{ 33, "33"},\
};
