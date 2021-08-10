/* 
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Copyright 2020 GOODIX 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


/** Filename: tfa9874_tfaFieldnames_last.h
 *  This file was generated automatically on 09/28/18 at 11:20:52. 
 *  Source file: TFA9874N1C0_DefaultI2CSettings.xlsx
 */

#ifndef _TFA9874_TFAFIELDNAMES_H
#define _TFA9874_TFAFIELDNAMES_H


#define TFA9874_I2CVERSION    1.16

typedef enum Tfa9874BfEnumList {
    TFA9874_BF_PWDN  = 0x0000,    /*!< Powerdown selection                                */
    TFA9874_BF_I2CR  = 0x0010,    /*!< I2C Reset - Auto clear                             */
    TFA9874_BF_AMPE  = 0x0030,    /*!< Activate Amplifier                                 */
    TFA9874_BF_DCA   = 0x0040,    /*!< Activate DC-to-DC converter                        */
    TFA9874_BF_INTP  = 0x0071,    /*!< Interrupt config                                   */
    TFA9874_BF_BYPOCP= 0x00b0,    /*!< Bypass OCP                                         */
    TFA9874_BF_TSTOCP= 0x00c0,    /*!< OCP testing control                                */
    TFA9874_BF_MANSCONF= 0x0120,    /*!< I2C configured                                     */
    TFA9874_BF_MANAOOSC= 0x0140,    /*!< Internal osc off at PWDN                           */
    TFA9874_BF_MUTETO= 0x01d0,    /*!< Time out SB mute sequence                          */
    TFA9874_BF_OPENMTP= 0x01e0,    /*!< Control for FAIM protection                        */
    TFA9874_BF_AUDFS = 0x0203,    /*!< Sample rate (fs)                                   */
    TFA9874_BF_INPLEV= 0x0240,    /*!< TDM output attenuation                             */
    TFA9874_BF_FRACTDEL= 0x0255,    /*!< V/I Fractional delay                               */
    TFA9874_BF_REV   = 0x030f,    /*!< Revision info                                      */
    TFA9874_BF_REFCKEXT= 0x0401,    /*!< PLL external ref clock                             */
    TFA9874_BF_REFCKSEL= 0x0420,    /*!< PLL internal ref clock                             */
    TFA9874_BF_SSFAIME= 0x05c0,    /*!< Sub-system FAIM                                    */
    TFA9874_BF_AMPOCRT= 0x0802,    /*!< Amplifier on-off criteria for shutdown             */
    TFA9874_BF_VDDS  = 0x1000,    /*!< POR                                                */
    TFA9874_BF_DCOCPOK= 0x1010,    /*!< DCDC OCP nmos   (sticky register , clear on read)  */
    TFA9874_BF_OTDS  = 0x1020,    /*!< OTP alarm   (sticky register , clear on read)      */
    TFA9874_BF_OCDS  = 0x1030,    /*!< OCP  amplifier   (sticky register , clear on read) */
    TFA9874_BF_UVDS  = 0x1040,    /*!< UVP alarm  (sticky register , clear on read)       */
    TFA9874_BF_MANALARM= 0x1050,    /*!< Alarm state                                        */
    TFA9874_BF_TDMERR= 0x1060,    /*!< TDM error                                          */
    TFA9874_BF_NOCLK = 0x1070,    /*!< Lost clock  (sticky register , clear on read)      */
    TFA9874_BF_DCIL  = 0x1100,    /*!< DCDC current limiting                              */
    TFA9874_BF_DCDCA = 0x1110,    /*!< DCDC active  (sticky register , clear on read)     */
    TFA9874_BF_DCHVBAT= 0x1130,    /*!< DCDC level 1x                                      */
    TFA9874_BF_DCH114= 0x1140,    /*!< DCDC level 1.14x                                   */
    TFA9874_BF_DCH107= 0x1150,    /*!< DCDC level 1.07x                                   */
    TFA9874_BF_PLLS  = 0x1160,    /*!< PLL lock                                           */
    TFA9874_BF_CLKS  = 0x1170,    /*!< Clocks stable                                      */
    TFA9874_BF_TDMLUTER= 0x1180,    /*!< TDM LUT error                                      */
    TFA9874_BF_TDMSTAT= 0x1192,    /*!< TDM status bits                                    */
    TFA9874_BF_MTPB  = 0x11c0,    /*!< MTP busy                                           */
    TFA9874_BF_SWS   = 0x11d0,    /*!< Amplifier engage                                   */
    TFA9874_BF_AMPS  = 0x11e0,    /*!< Amplifier enable                                   */
    TFA9874_BF_AREFS = 0x11f0,    /*!< References enable                                  */
    TFA9874_BF_OCPOAP= 0x1300,    /*!< OCPOK pmos A                                       */
    TFA9874_BF_OCPOAN= 0x1310,    /*!< OCPOK nmos A                                       */
    TFA9874_BF_OCPOBP= 0x1320,    /*!< OCPOK pmos B                                       */
    TFA9874_BF_OCPOBN= 0x1330,    /*!< OCPOK nmos B                                       */
    TFA9874_BF_OVDS  = 0x1380,    /*!< OVP alarm                                          */
    TFA9874_BF_CLIPS = 0x1390,    /*!< Amplifier  clipping                                */
    TFA9874_BF_ADCCR = 0x13a0,    /*!< Control ADC                                        */
    TFA9874_BF_MANWAIT1= 0x13c0,    /*!< Wait HW I2C settings                               */
    TFA9874_BF_MANMUTE= 0x13e0,    /*!< Audio mute sequence                                */
    TFA9874_BF_MANOPER= 0x13f0,    /*!< Operating state                                    */
    TFA9874_BF_CLKOOR= 0x1420,    /*!< External clock status                              */
    TFA9874_BF_MANSTATE= 0x1433,    /*!< Device manager status                              */
    TFA9874_BF_DCMODE= 0x1471,    /*!< DCDC mode status bits                              */
    TFA9874_BF_BATS  = 0x1509,    /*!< Battery voltage (V)                                */
    TFA9874_BF_TEMPS = 0x1608,    /*!< IC Temperature (C)                                 */
    TFA9874_BF_VDDPS = 0x1709,    /*!< IC VDDP voltage ( 1023*VDDP/13 V)                  */
    TFA9874_BF_TDME  = 0x2040,    /*!< Enable interface                                   */
    TFA9874_BF_TDMMODE= 0x2050,    /*!< Slave/master                                       */
    TFA9874_BF_TDMCLINV= 0x2060,    /*!< Reception data to BCK clock                        */
    TFA9874_BF_TDMFSLN= 0x2073,    /*!< FS length (master mode only)                       */
    TFA9874_BF_TDMFSPOL= 0x20b0,    /*!< FS polarity                                        */
    TFA9874_BF_TDMNBCK= 0x20c3,    /*!< N-BCK's in FS                                      */
    TFA9874_BF_TDMSLOTS= 0x2103,    /*!< N-slots in Frame                                   */
    TFA9874_BF_TDMSLLN= 0x2144,    /*!< N-bits in slot                                     */
    TFA9874_BF_TDMBRMG= 0x2194,    /*!< N-bits remaining                                   */
    TFA9874_BF_TDMDEL= 0x21e0,    /*!< data delay to FS                                   */
    TFA9874_BF_TDMADJ= 0x21f0,    /*!< data adjustment                                    */
    TFA9874_BF_TDMOOMP= 0x2201,    /*!< Received audio compression                         */
    TFA9874_BF_TDMSSIZE= 0x2224,    /*!< Sample size per slot                               */
    TFA9874_BF_TDMTXDFO= 0x2271,    /*!< Format unused bits                                 */
    TFA9874_BF_TDMTXUS0= 0x2291,    /*!< Format unused slots DATAO                          */
    TFA9874_BF_TDMSPKE= 0x2300,    /*!< Control audio tdm channel in 0 (spkr + dcdc)       */
    TFA9874_BF_TDMDCE= 0x2310,    /*!< Control audio  tdm channel in 1  (dcdc)            */
    TFA9874_BF_TDMCSE= 0x2330,    /*!< current sense vbat temperature and vddp feedback   */
    TFA9874_BF_TDMVSE= 0x2340,    /*!< Voltage sense vbat temperature and vddp feedback   */
    TFA9874_BF_TDMSPKS= 0x2603,    /*!< tdm slot for sink 0 (speaker + dcdc)               */
    TFA9874_BF_TDMDCS= 0x2643,    /*!< tdm slot for  sink 1  (dcdc)                       */
    TFA9874_BF_TDMCSS= 0x26c3,    /*!< Slot Position of current sense vbat temperature and vddp feedback */
    TFA9874_BF_TDMVSS= 0x2703,    /*!< Slot Position of Voltage sense vbat temperature and vddp feedback */
    TFA9874_BF_ISTVDDS= 0x4000,    /*!< Status POR                                         */
    TFA9874_BF_ISTBSTOC= 0x4010,    /*!< Status DCDC OCP                                    */
    TFA9874_BF_ISTOTDS= 0x4020,    /*!< Status OTP alarm                                   */
    TFA9874_BF_ISTOCPR= 0x4030,    /*!< Status ocp alarm                                   */
    TFA9874_BF_ISTUVDS= 0x4040,    /*!< Status UVP alarm                                   */
    TFA9874_BF_ISTMANALARM= 0x4050,    /*!< Status  nanager Alarm state                        */
    TFA9874_BF_ISTTDMER= 0x4060,    /*!< Status tdm error                                   */
    TFA9874_BF_ISTNOCLK= 0x4070,    /*!< Status lost clock                                  */
    TFA9874_BF_ICLVDDS= 0x4400,    /*!< Clear POR                                          */
    TFA9874_BF_ICLBSTOC= 0x4410,    /*!< Clear DCDC OCP                                     */
    TFA9874_BF_ICLOTDS= 0x4420,    /*!< Clear OTP alarm                                    */
    TFA9874_BF_ICLOCPR= 0x4430,    /*!< Clear ocp alarm                                    */
    TFA9874_BF_ICLUVDS= 0x4440,    /*!< Clear UVP alarm                                    */
    TFA9874_BF_ICLMANALARM= 0x4450,    /*!< clear  nanager Alarm state                         */
    TFA9874_BF_ICLTDMER= 0x4460,    /*!< Clear tdm error                                    */
    TFA9874_BF_ICLNOCLK= 0x4470,    /*!< Clear lost clk                                     */
    TFA9874_BF_IEVDDS= 0x4800,    /*!< Enable por                                         */
    TFA9874_BF_IEBSTOC= 0x4810,    /*!< Enable DCDC OCP                                    */
    TFA9874_BF_IEOTDS= 0x4820,    /*!< Enable OTP alarm                                   */
    TFA9874_BF_IEOCPR= 0x4830,    /*!< Enable ocp alarm                                   */
    TFA9874_BF_IEUVDS= 0x4840,    /*!< Enable UVP alarm                                   */
    TFA9874_BF_IEMANALARM= 0x4850,    /*!< Enable  nanager Alarm state                        */
    TFA9874_BF_IETDMER= 0x4860,    /*!< Enable tdm error                                   */
    TFA9874_BF_IENOCLK= 0x4870,    /*!< Enable lost clk                                    */
    TFA9874_BF_IPOVDDS= 0x4c00,    /*!< Polarity por                                       */
    TFA9874_BF_IPOBSTOC= 0x4c10,    /*!< Polarity DCDC OCP                                  */
    TFA9874_BF_IPOOTDS= 0x4c20,    /*!< Polarity OTP alarm                                 */
    TFA9874_BF_IPOOCPR= 0x4c30,    /*!< Polarity ocp alarm                                 */
    TFA9874_BF_IPOUVDS= 0x4c40,    /*!< Polarity UVP alarm                                 */
    TFA9874_BF_IPOMANALARM= 0x4c50,    /*!< Polarity  nanager Alarm state                      */
    TFA9874_BF_IPOTDMER= 0x4c60,    /*!< Polarity tdm error                                 */
    TFA9874_BF_IPONOCLK= 0x4c70,    /*!< Polarity lost clk                                  */
    TFA9874_BF_BSSCR = 0x5001,    /*!< Battery Safeguard attack time                      */
    TFA9874_BF_BSST  = 0x5023,    /*!< Battery Safeguard threshold voltage level          */
    TFA9874_BF_BSSRL = 0x5061,    /*!< Battery Safeguard maximum reduction                */
    TFA9874_BF_VBATFLTL= 0x5080,    /*!< vbat filter limit                                  */
    TFA9874_BF_BSSR  = 0x50e0,    /*!< Battery voltage read out                           */
    TFA9874_BF_BSSBY = 0x50f0,    /*!< Bypass battery safeguard                           */
    TFA9874_BF_BSSS  = 0x5100,    /*!< Vbat prot steepness                                */
    TFA9874_BF_HPFBYP= 0x5150,    /*!< Bypass HPF                                         */
    TFA9874_BF_DPSA  = 0x5170,    /*!< Enable DPSA                                        */
    TFA9874_BF_CLIPCTRL= 0x5222,    /*!< Clip control setting                               */
    TFA9874_BF_AMPGAIN= 0x5257,    /*!< Amplifier gain                                     */
    TFA9874_BF_SLOPEE= 0x52d0,    /*!< Enables slope control                              */
    TFA9874_BF_SLOPESET= 0x52e0,    /*!< Slope speed setting (bin. coded)                   */
    TFA9874_BF_TDMDCG= 0x6123,    /*!< Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE) */
    TFA9874_BF_TDMSPKG= 0x6163,    /*!< Total gain depending on INPLEV setting (channel 0) */
    TFA9874_BF_LNMODE= 0x62e1,    /*!< ctrl select mode                                   */
    TFA9874_BF_LPM1MODE= 0x64e1,    /*!< low power mode control                             */
    TFA9874_BF_TDMSRCMAP= 0x6802,    /*!< tdm source mapping                                 */
    TFA9874_BF_TDMSRCAS= 0x6831,    /*!< Sensed value  A                                    */
    TFA9874_BF_TDMSRCBS= 0x6851,    /*!< Sensed value  B                                    */
    TFA9874_BF_TDMSRCACLIP= 0x6871,    /*!< clip information  (analog /digital) for source0    */
    TFA9874_BF_TDMSRCBCLIP= 0x6891,    /*!< clip information  (analog /digital) for source1    */
    TFA9874_BF_LP1   = 0x6e10,    /*!< low power mode 1 detection                         */
    TFA9874_BF_LA    = 0x6e20,    /*!< low amplitude detection                            */
    TFA9874_BF_VDDPH = 0x6e30,    /*!< vddp greater than vbat                             */
    TFA9874_BF_DELCURCOMP= 0x6f02,    /*!< delay to allign compensation signal with current sense signal */
    TFA9874_BF_SIGCURCOMP= 0x6f40,    /*!< polarity of compensation for current sense         */
    TFA9874_BF_ENCURCOMP= 0x6f50,    /*!< enable current sense compensation                  */
    TFA9874_BF_LVLCLPPWM= 0x6f72,    /*!< set the amount of pwm pulse that may be skipped before clip-flag is triggered */
    TFA9874_BF_DCMCC = 0x7033,    /*!< Max coil current                                   */
    TFA9874_BF_DCCV  = 0x7071,    /*!< Slope compensation current, represents LxF (inductance x frequency) value  */
    TFA9874_BF_DCIE  = 0x7090,    /*!< Adaptive boost mode                                */
    TFA9874_BF_DCSR  = 0x70a0,    /*!< Soft ramp up/down                                  */
    TFA9874_BF_DCDIS = 0x70e0,    /*!< DCDC on/off                                        */
    TFA9874_BF_DCPWM = 0x70f0,    /*!< DCDC PWM only mode                                 */
    TFA9874_BF_DCTRACK= 0x7430,    /*!< Boost algorithm selection, effective only when boost_intelligent is set to 1 */
    TFA9874_BF_DCTRIP= 0x7444,    /*!< 1st Adaptive boost trip levels, effective only when DCIE is set to 1 */
    TFA9874_BF_DCHOLD= 0x7494,    /*!< Hold time for DCDC booster, effective only when boost_intelligent is set to 1 */
    TFA9874_BF_DCINT = 0x74e0,    /*!< Selection of data for adaptive boost algorithm, effective only when boost_intelligent is set to 1 */
    TFA9874_BF_DCTRIP2= 0x7534,    /*!< 2nd Adaptive boost trip levels, effective only when DCIE is set to 1 */
    TFA9874_BF_DCTRIPT= 0x7584,    /*!< Track Adaptive boost trip levels, effective only when boost_intelligent is set to 1 */
    TFA9874_BF_DCTRIPHYSTE= 0x75f0,    /*!< Enable hysteresis on booster trip levels           */
    TFA9874_BF_DCVOF = 0x7635,    /*!< First boost voltage level                          */
    TFA9874_BF_DCVOS = 0x7695,    /*!< Second boost voltage level                         */
    TFA9874_BF_MTPK  = 0xa107,    /*!< MTP KEY2 register                                  */
    TFA9874_BF_KEY1LOCKED= 0xa200,    /*!< Indicates KEY1 is locked                           */
    TFA9874_BF_KEY2LOCKED= 0xa210,    /*!< Indicates KEY2 is locked                           */
    TFA9874_BF_CIMTP = 0xa360,    /*!< Start copying data from I2C mtp registers to mtp   */
    TFA9874_BF_MTPRDMSB= 0xa50f,    /*!< MSB word of MTP manual read data                   */
    TFA9874_BF_MTPRDLSB= 0xa60f,    /*!< LSB word of MTP manual read data                   */
    TFA9874_BF_EXTTS = 0xb108,    /*!< External temperature (C)                           */
    TFA9874_BF_TROS  = 0xb190,    /*!< Select temp Speaker calibration                    */
    TFA9874_BF_SWPROFIL= 0xee0f,    /*!< Software profile data                              */
    TFA9874_BF_SWVSTEP= 0xef0f,    /*!< Software vstep information                         */
    TFA9874_BF_MTPOTC= 0xf000,    /*!< Calibration schedule                               */
    TFA9874_BF_MTPEX = 0xf010,    /*!< Calibration Ron executed                           */
    TFA9874_BF_DCMCCAPI= 0xf020,    /*!< Calibration current limit DCDC                     */
    TFA9874_BF_DCMCCSB= 0xf030,    /*!< Sign bit for delta calibration current limit DCDC  */
    TFA9874_BF_USERDEF= 0xf042,    /*!< Calibration delta current limit DCDC               */
    TFA9874_BF_CUSTINFO= 0xf078,    /*!< Reserved space for allowing customer to store speaker information */
    TFA9874_BF_R25C  = 0xf50f,    /*!< Ron resistance of  speaker coil                    */
} Tfa9874BfEnumList_t;
#define TFA9874_NAMETABLE static tfaBfName_t Tfa9874DatasheetNames[]= {\
   { 0x0, "PWDN"},    /* Powerdown selection                               , */\
   { 0x10, "I2CR"},    /* I2C Reset - Auto clear                            , */\
   { 0x30, "AMPE"},    /* Activate Amplifier                                , */\
   { 0x40, "DCA"},    /* Activate DC-to-DC converter                       , */\
   { 0x71, "INTP"},    /* Interrupt config                                  , */\
   { 0xb0, "BYPOCP"},    /* Bypass OCP                                        , */\
   { 0xc0, "TSTOCP"},    /* OCP testing control                               , */\
   { 0x120, "MANSCONF"},    /* I2C configured                                    , */\
   { 0x140, "MANAOOSC"},    /* Internal osc off at PWDN                          , */\
   { 0x1d0, "MUTETO"},    /* Time out SB mute sequence                         , */\
   { 0x1e0, "OPENMTP"},    /* Control for FAIM protection                       , */\
   { 0x203, "AUDFS"},    /* Sample rate (fs)                                  , */\
   { 0x240, "INPLEV"},    /* TDM output attenuation                            , */\
   { 0x255, "FRACTDEL"},    /* V/I Fractional delay                              , */\
   { 0x30f, "REV"},    /* Revision info                                     , */\
   { 0x401, "REFCKEXT"},    /* PLL external ref clock                            , */\
   { 0x420, "REFCKSEL"},    /* PLL internal ref clock                            , */\
   { 0x5c0, "SSFAIME"},    /* Sub-system FAIM                                   , */\
   { 0x802, "AMPOCRT"},    /* Amplifier on-off criteria for shutdown            , */\
   { 0x1000, "VDDS"},    /* POR                                               , */\
   { 0x1010, "DCOCPOK"},    /* DCDC OCP nmos   (sticky register , clear on read) , */\
   { 0x1020, "OTDS"},    /* OTP alarm   (sticky register , clear on read)     , */\
   { 0x1030, "OCDS"},    /* OCP  amplifier   (sticky register , clear on read), */\
   { 0x1040, "UVDS"},    /* UVP alarm  (sticky register , clear on read)      , */\
   { 0x1050, "MANALARM"},    /* Alarm state                                       , */\
   { 0x1060, "TDMERR"},    /* TDM error                                         , */\
   { 0x1070, "NOCLK"},    /* Lost clock  (sticky register , clear on read)     , */\
   { 0x1100, "DCIL"},    /* DCDC current limiting                             , */\
   { 0x1110, "DCDCA"},    /* DCDC active  (sticky register , clear on read)    , */\
   { 0x1130, "DCHVBAT"},    /* DCDC level 1x                                     , */\
   { 0x1140, "DCH114"},    /* DCDC level 1.14x                                  , */\
   { 0x1150, "DCH107"},    /* DCDC level 1.07x                                  , */\
   { 0x1160, "PLLS"},    /* PLL lock                                          , */\
   { 0x1170, "CLKS"},    /* Clocks stable                                     , */\
   { 0x1180, "TDMLUTER"},    /* TDM LUT error                                     , */\
   { 0x1192, "TDMSTAT"},    /* TDM status bits                                   , */\
   { 0x11c0, "MTPB"},    /* MTP busy                                          , */\
   { 0x11d0, "SWS"},    /* Amplifier engage                                  , */\
   { 0x11e0, "AMPS"},    /* Amplifier enable                                  , */\
   { 0x11f0, "AREFS"},    /* References enable                                 , */\
   { 0x1300, "OCPOAP"},    /* OCPOK pmos A                                      , */\
   { 0x1310, "OCPOAN"},    /* OCPOK nmos A                                      , */\
   { 0x1320, "OCPOBP"},    /* OCPOK pmos B                                      , */\
   { 0x1330, "OCPOBN"},    /* OCPOK nmos B                                      , */\
   { 0x1380, "OVDS"},    /* OVP alarm                                         , */\
   { 0x1390, "CLIPS"},    /* Amplifier  clipping                               , */\
   { 0x13a0, "ADCCR"},    /* Control ADC                                       , */\
   { 0x13c0, "MANWAIT1"},    /* Wait HW I2C settings                              , */\
   { 0x13e0, "MANMUTE"},    /* Audio mute sequence                               , */\
   { 0x13f0, "MANOPER"},    /* Operating state                                   , */\
   { 0x1420, "CLKOOR"},    /* External clock status                             , */\
   { 0x1433, "MANSTATE"},    /* Device manager status                             , */\
   { 0x1471, "DCMODE"},    /* DCDC mode status bits                             , */\
   { 0x1509, "BATS"},    /* Battery voltage (V)                               , */\
   { 0x1608, "TEMPS"},    /* IC Temperature (C)                                , */\
   { 0x1709, "VDDPS"},    /* IC VDDP voltage ( 1023*VDDP/13 V)                 , */\
   { 0x2040, "TDME"},    /* Enable interface                                  , */\
   { 0x2050, "TDMMODE"},    /* Slave/master                                      , */\
   { 0x2060, "TDMCLINV"},    /* Reception data to BCK clock                       , */\
   { 0x2073, "TDMFSLN"},    /* FS length (master mode only)                      , */\
   { 0x20b0, "TDMFSPOL"},    /* FS polarity                                       , */\
   { 0x20c3, "TDMNBCK"},    /* N-BCK's in FS                                     , */\
   { 0x2103, "TDMSLOTS"},    /* N-slots in Frame                                  , */\
   { 0x2144, "TDMSLLN"},    /* N-bits in slot                                    , */\
   { 0x2194, "TDMBRMG"},    /* N-bits remaining                                  , */\
   { 0x21e0, "TDMDEL"},    /* data delay to FS                                  , */\
   { 0x21f0, "TDMADJ"},    /* data adjustment                                   , */\
   { 0x2201, "TDMOOMP"},    /* Received audio compression                        , */\
   { 0x2224, "TDMSSIZE"},    /* Sample size per slot                              , */\
   { 0x2271, "TDMTXDFO"},    /* Format unused bits                                , */\
   { 0x2291, "TDMTXUS0"},    /* Format unused slots DATAO                         , */\
   { 0x2300, "TDMSPKE"},    /* Control audio tdm channel in 0 (spkr + dcdc)      , */\
   { 0x2310, "TDMDCE"},    /* Control audio  tdm channel in 1  (dcdc)           , */\
   { 0x2330, "TDMCSE"},    /* current sense vbat temperature and vddp feedback  , */\
   { 0x2340, "TDMVSE"},    /* Voltage sense vbat temperature and vddp feedback  , */\
   { 0x2603, "TDMSPKS"},    /* tdm slot for sink 0 (speaker + dcdc)              , */\
   { 0x2643, "TDMDCS"},    /* tdm slot for  sink 1  (dcdc)                      , */\
   { 0x26c3, "TDMCSS"},    /* Slot Position of current sense vbat temperature and vddp feedback, */\
   { 0x2703, "TDMVSS"},    /* Slot Position of Voltage sense vbat temperature and vddp feedback, */\
   { 0x4000, "ISTVDDS"},    /* Status POR                                        , */\
   { 0x4010, "ISTBSTOC"},    /* Status DCDC OCP                                   , */\
   { 0x4020, "ISTOTDS"},    /* Status OTP alarm                                  , */\
   { 0x4030, "ISTOCPR"},    /* Status ocp alarm                                  , */\
   { 0x4040, "ISTUVDS"},    /* Status UVP alarm                                  , */\
   { 0x4050, "ISTMANALARM"},    /* Status  nanager Alarm state                       , */\
   { 0x4060, "ISTTDMER"},    /* Status tdm error                                  , */\
   { 0x4070, "ISTNOCLK"},    /* Status lost clock                                 , */\
   { 0x4400, "ICLVDDS"},    /* Clear POR                                         , */\
   { 0x4410, "ICLBSTOC"},    /* Clear DCDC OCP                                    , */\
   { 0x4420, "ICLOTDS"},    /* Clear OTP alarm                                   , */\
   { 0x4430, "ICLOCPR"},    /* Clear ocp alarm                                   , */\
   { 0x4440, "ICLUVDS"},    /* Clear UVP alarm                                   , */\
   { 0x4450, "ICLMANALARM"},    /* clear  nanager Alarm state                        , */\
   { 0x4460, "ICLTDMER"},    /* Clear tdm error                                   , */\
   { 0x4470, "ICLNOCLK"},    /* Clear lost clk                                    , */\
   { 0x4800, "IEVDDS"},    /* Enable por                                        , */\
   { 0x4810, "IEBSTOC"},    /* Enable DCDC OCP                                   , */\
   { 0x4820, "IEOTDS"},    /* Enable OTP alarm                                  , */\
   { 0x4830, "IEOCPR"},    /* Enable ocp alarm                                  , */\
   { 0x4840, "IEUVDS"},    /* Enable UVP alarm                                  , */\
   { 0x4850, "IEMANALARM"},    /* Enable  nanager Alarm state                       , */\
   { 0x4860, "IETDMER"},    /* Enable tdm error                                  , */\
   { 0x4870, "IENOCLK"},    /* Enable lost clk                                   , */\
   { 0x4c00, "IPOVDDS"},    /* Polarity por                                      , */\
   { 0x4c10, "IPOBSTOC"},    /* Polarity DCDC OCP                                 , */\
   { 0x4c20, "IPOOTDS"},    /* Polarity OTP alarm                                , */\
   { 0x4c30, "IPOOCPR"},    /* Polarity ocp alarm                                , */\
   { 0x4c40, "IPOUVDS"},    /* Polarity UVP alarm                                , */\
   { 0x4c50, "IPOMANALARM"},    /* Polarity  nanager Alarm state                     , */\
   { 0x4c60, "IPOTDMER"},    /* Polarity tdm error                                , */\
   { 0x4c70, "IPONOCLK"},    /* Polarity lost clk                                 , */\
   { 0x5001, "BSSCR"},    /* Battery Safeguard attack time                     , */\
   { 0x5023, "BSST"},    /* Battery Safeguard threshold voltage level         , */\
   { 0x5061, "BSSRL"},    /* Battery Safeguard maximum reduction               , */\
   { 0x5080, "VBATFLTL"},    /* vbat filter limit                                 , */\
   { 0x50e0, "BSSR"},    /* Battery voltage read out                          , */\
   { 0x50f0, "BSSBY"},    /* Bypass battery safeguard                          , */\
   { 0x5100, "BSSS"},    /* Vbat prot steepness                               , */\
   { 0x5150, "HPFBYP"},    /* Bypass HPF                                        , */\
   { 0x5170, "DPSA"},    /* Enable DPSA                                       , */\
   { 0x5222, "CLIPCTRL"},    /* Clip control setting                              , */\
   { 0x5257, "AMPGAIN"},    /* Amplifier gain                                    , */\
   { 0x52d0, "SLOPEE"},    /* Enables slope control                             , */\
   { 0x52e0, "SLOPESET"},    /* Slope speed setting (bin. coded)                  , */\
   { 0x6123, "TDMDCG"},    /* Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE), */\
   { 0x6163, "TDMSPKG"},    /* Total gain depending on INPLEV setting (channel 0), */\
   { 0x62e1, "LNMODE"},    /* ctrl select mode                                  , */\
   { 0x64e1, "LPM1MODE"},    /* low power mode control                            , */\
   { 0x6802, "TDMSRCMAP"},    /* tdm source mapping                                , */\
   { 0x6831, "TDMSRCAS"},    /* Sensed value  A                                   , */\
   { 0x6851, "TDMSRCBS"},    /* Sensed value  B                                   , */\
   { 0x6871, "TDMSRCACLIP"},    /* clip information  (analog /digital) for source0   , */\
   { 0x6891, "TDMSRCBCLIP"},    /* clip information  (analog /digital) for source1   , */\
   { 0x6e10, "LP1"},    /* low power mode 1 detection                        , */\
   { 0x6e20, "LA"},    /* low amplitude detection                           , */\
   { 0x6e30, "VDDPH"},    /* vddp greater than vbat                            , */\
   { 0x6f02, "DELCURCOMP"},    /* delay to allign compensation signal with current sense signal, */\
   { 0x6f40, "SIGCURCOMP"},    /* polarity of compensation for current sense        , */\
   { 0x6f50, "ENCURCOMP"},    /* enable current sense compensation                 , */\
   { 0x6f72, "LVLCLPPWM"},    /* set the amount of pwm pulse that may be skipped before clip-flag is triggered, */\
   { 0x7033, "DCMCC"},    /* Max coil current                                  , */\
   { 0x7071, "DCCV"},    /* Slope compensation current, represents LxF (inductance x frequency) value , */\
   { 0x7090, "DCIE"},    /* Adaptive boost mode                               , */\
   { 0x70a0, "DCSR"},    /* Soft ramp up/down                                 , */\
   { 0x70e0, "DCDIS"},    /* DCDC on/off                                       , */\
   { 0x70f0, "DCPWM"},    /* DCDC PWM only mode                                , */\
   { 0x7430, "DCTRACK"},    /* Boost algorithm selection, effective only when boost_intelligent is set to 1, */\
   { 0x7444, "DCTRIP"},    /* 1st Adaptive boost trip levels, effective only when DCIE is set to 1, */\
   { 0x7494, "DCHOLD"},    /* Hold time for DCDC booster, effective only when boost_intelligent is set to 1, */\
   { 0x74e0, "DCINT"},    /* Selection of data for adaptive boost algorithm, effective only when boost_intelligent is set to 1, */\
   { 0x7534, "DCTRIP2"},    /* 2nd Adaptive boost trip levels, effective only when DCIE is set to 1, */\
   { 0x7584, "DCTRIPT"},    /* Track Adaptive boost trip levels, effective only when boost_intelligent is set to 1, */\
   { 0x75f0, "DCTRIPHYSTE"},    /* Enable hysteresis on booster trip levels          , */\
   { 0x7635, "DCVOF"},    /* First boost voltage level                         , */\
   { 0x7695, "DCVOS"},    /* Second boost voltage level                        , */\
   { 0xa107, "MTPK"},    /* MTP KEY2 register                                 , */\
   { 0xa200, "KEY1LOCKED"},    /* Indicates KEY1 is locked                          , */\
   { 0xa210, "KEY2LOCKED"},    /* Indicates KEY2 is locked                          , */\
   { 0xa360, "CIMTP"},    /* Start copying data from I2C mtp registers to mtp  , */\
   { 0xa50f, "MTPRDMSB"},    /* MSB word of MTP manual read data                  , */\
   { 0xa60f, "MTPRDLSB"},    /* LSB word of MTP manual read data                  , */\
   { 0xb108, "EXTTS"},    /* External temperature (C)                          , */\
   { 0xb190, "TROS"},    /* Select temp Speaker calibration                   , */\
   { 0xee0f, "SWPROFIL"},    /* Software profile data                             , */\
   { 0xef0f, "SWVSTEP"},    /* Software vstep information                        , */\
   { 0xf000, "MTPOTC"},    /* Calibration schedule                              , */\
   { 0xf010, "MTPEX"},    /* Calibration Ron executed                          , */\
   { 0xf020, "DCMCCAPI"},    /* Calibration current limit DCDC                    , */\
   { 0xf030, "DCMCCSB"},    /* Sign bit for delta calibration current limit DCDC , */\
   { 0xf042, "USERDEF"},    /* Calibration delta current limit DCDC              , */\
   { 0xf078, "CUSTINFO"},    /* Reserved space for allowing customer to store speaker information, */\
   { 0xf50f, "R25C"},    /* Ron resistance of  speaker coil                   , */\
   { 0xffff,"Unknown bitfield enum" }   /* not found */\
};

#define TFA9874_BITNAMETABLE static tfaBfName_t Tfa9874BitNames[]= {\
   { 0x0, "powerdown"},    /* Powerdown selection                               , */\
   { 0x10, "reset"},    /* I2C Reset - Auto clear                            , */\
   { 0x30, "enbl_amplifier"},    /* Activate Amplifier                                , */\
   { 0x40, "enbl_boost"},    /* Activate DC-to-DC converter                       , */\
   { 0x71, "int_pad_io"},    /* Interrupt config                                  , */\
   { 0xb0, "bypass_ocp"},    /* Bypass OCP                                        , */\
   { 0xc0, "test_ocp"},    /* OCP testing control                               , */\
   { 0x120, "src_set_configured"},    /* I2C configured                                    , */\
   { 0x140, "enbl_osc1m_auto_off"},    /* Internal osc off at PWDN                          , */\
   { 0x1d0, "disable_mute_time_out"},    /* Time out SB mute sequence                         , */\
   { 0x1e0, "unprotect_faim"},    /* Control for FAIM protection                       , */\
   { 0x203, "audio_fs"},    /* Sample rate (fs)                                  , */\
   { 0x240, "input_level"},    /* TDM output attenuation                            , */\
   { 0x255, "cs_frac_delay"},    /* V/I Fractional delay                              , */\
   { 0x2d0, "sel_hysteresis"},    /* Select hysteresis for clock range detector        , */\
   { 0x30f, "device_rev"},    /* Revision info                                     , */\
   { 0x401, "pll_clkin_sel"},    /* PLL external ref clock                            , */\
   { 0x420, "pll_clkin_sel_osc"},    /* PLL internal ref clock                            , */\
   { 0x5c0, "enbl_faim_ss"},    /* Sub-system FAIM                                   , */\
   { 0x802, "ctrl_on2off_criterion"},    /* Amplifier on-off criteria for shutdown            , */\
   { 0xe07, "ctrl_digtoana"},    /* Spare control from digital to analog              , */\
   { 0xf0f, "hidden_code"},    /* 5A6Bh, 23147d to access registers (default for engineering), */\
   { 0x1000, "flag_por"},    /* POR                                               , */\
   { 0x1010, "flag_bst_ocpok"},    /* DCDC OCP nmos   (sticky register , clear on read) , */\
   { 0x1020, "flag_otpok"},    /* OTP alarm   (sticky register , clear on read)     , */\
   { 0x1030, "flag_ocp_alarm"},    /* OCP  amplifier   (sticky register , clear on read), */\
   { 0x1040, "flag_uvpok"},    /* UVP alarm  (sticky register , clear on read)      , */\
   { 0x1050, "flag_man_alarm_state"},    /* Alarm state                                       , */\
   { 0x1060, "flag_tdm_error"},    /* TDM error                                         , */\
   { 0x1070, "flag_lost_clk"},    /* Lost clock  (sticky register , clear on read)     , */\
   { 0x1100, "flag_bst_bstcur"},    /* DCDC current limiting                             , */\
   { 0x1110, "flag_bst_hiz"},    /* DCDC active  (sticky register , clear on read)    , */\
   { 0x1120, "flag_bst_peakcur"},    /* Indicates current is max in DC-to-DC converter    , */\
   { 0x1130, "flag_bst_voutcomp"},    /* DCDC level 1x                                     , */\
   { 0x1140, "flag_bst_voutcomp86"},    /* DCDC level 1.14x                                  , */\
   { 0x1150, "flag_bst_voutcomp93"},    /* DCDC level 1.07x                                  , */\
   { 0x1160, "flag_pll_lock"},    /* PLL lock                                          , */\
   { 0x1170, "flag_clocks_stable"},    /* Clocks stable                                     , */\
   { 0x1180, "flag_tdm_lut_error"},    /* TDM LUT error                                     , */\
   { 0x1192, "flag_tdm_status"},    /* TDM status bits                                   , */\
   { 0x11c0, "flag_mtp_busy"},    /* MTP busy                                          , */\
   { 0x11d0, "flag_engage"},    /* Amplifier engage                                  , */\
   { 0x11e0, "flag_enbl_amp"},    /* Amplifier enable                                  , */\
   { 0x11f0, "flag_enbl_ref"},    /* References enable                                 , */\
   { 0x1300, "flag_ocpokap"},    /* OCPOK pmos A                                      , */\
   { 0x1310, "flag_ocpokan"},    /* OCPOK nmos A                                      , */\
   { 0x1320, "flag_ocpokbp"},    /* OCPOK pmos B                                      , */\
   { 0x1330, "flag_ocpokbn"},    /* OCPOK nmos B                                      , */\
   { 0x1380, "flag_ovpok"},    /* OVP alarm                                         , */\
   { 0x1390, "flag_clip"},    /* Amplifier  clipping                               , */\
   { 0x13a0, "flag_adc10_ready"},    /* Control ADC                                       , */\
   { 0x13c0, "flag_man_wait_src_settings"},    /* Wait HW I2C settings                              , */\
   { 0x13e0, "flag_man_start_mute_audio"},    /* Audio mute sequence                               , */\
   { 0x13f0, "flag_man_operating_state"},    /* Operating state                                   , */\
   { 0x1420, "flag_clk_out_of_range"},    /* External clock status                             , */\
   { 0x1433, "man_state"},    /* Device manager status                             , */\
   { 0x1471, "status_bst_mode"},    /* DCDC mode status bits                             , */\
   { 0x1509, "bat_adc"},    /* Battery voltage (V)                               , */\
   { 0x1608, "temp_adc"},    /* IC Temperature (C)                                , */\
   { 0x1709, "vddp_adc"},    /* IC VDDP voltage ( 1023*VDDP/13 V)                 , */\
   { 0x2040, "tdm_enable"},    /* Enable interface                                  , */\
   { 0x2050, "tdm_mode"},    /* Slave/master                                      , */\
   { 0x2060, "tdm_clk_inversion"},    /* Reception data to BCK clock                       , */\
   { 0x2073, "tdm_fs_ws_length"},    /* FS length (master mode only)                      , */\
   { 0x20b0, "tdm_fs_ws_polarity"},    /* FS polarity                                       , */\
   { 0x20c3, "tdm_nbck"},    /* N-BCK's in FS                                     , */\
   { 0x2103, "tdm_nb_of_slots"},    /* N-slots in Frame                                  , */\
   { 0x2144, "tdm_slot_length"},    /* N-bits in slot                                    , */\
   { 0x2194, "tdm_bits_remaining"},    /* N-bits remaining                                  , */\
   { 0x21e0, "tdm_data_delay"},    /* data delay to FS                                  , */\
   { 0x21f0, "tdm_data_adjustment"},    /* data adjustment                                   , */\
   { 0x2201, "tdm_audio_sample_compression"},    /* Received audio compression                        , */\
   { 0x2224, "tdm_sample_size"},    /* Sample size per slot                              , */\
   { 0x2271, "tdm_txdata_format"},    /* Format unused bits                                , */\
   { 0x2291, "tdm_txdata_format_unused_slot_sd0"},    /* Format unused slots DATAO                         , */\
   { 0x2300, "tdm_sink0_enable"},    /* Control audio tdm channel in 0 (spkr + dcdc)      , */\
   { 0x2310, "tdm_sink1_enable"},    /* Control audio  tdm channel in 1  (dcdc)           , */\
   { 0x2330, "tdm_source0_enable"},    /* current sense vbat temperature and vddp feedback  , */\
   { 0x2340, "tdm_source1_enable"},    /* Voltage sense vbat temperature and vddp feedback  , */\
   { 0x2603, "tdm_sink0_slot"},    /* tdm slot for sink 0 (speaker + dcdc)              , */\
   { 0x2643, "tdm_sink1_slot"},    /* tdm slot for  sink 1  (dcdc)                      , */\
   { 0x26c3, "tdm_source0_slot"},    /* Slot Position of current sense vbat temperature and vddp feedback, */\
   { 0x2703, "tdm_source1_slot"},    /* Slot Position of Voltage sense vbat temperature and vddp feedback, */\
   { 0x4000, "int_out_flag_por"},    /* Status POR                                        , */\
   { 0x4010, "int_out_flag_bst_ocpok"},    /* Status DCDC OCP                                   , */\
   { 0x4020, "int_out_flag_otpok"},    /* Status OTP alarm                                  , */\
   { 0x4030, "int_out_flag_ocp_alarm"},    /* Status ocp alarm                                  , */\
   { 0x4040, "int_out_flag_uvpok"},    /* Status UVP alarm                                  , */\
   { 0x4050, "int_out_flag_man_alarm_state"},    /* Status  nanager Alarm state                       , */\
   { 0x4060, "int_out_flag_tdm_error"},    /* Status tdm error                                  , */\
   { 0x4070, "int_out_flag_lost_clk"},    /* Status lost clock                                 , */\
   { 0x4400, "int_in_flag_por"},    /* Clear POR                                         , */\
   { 0x4410, "int_in_flag_bst_ocpok"},    /* Clear DCDC OCP                                    , */\
   { 0x4420, "int_in_flag_otpok"},    /* Clear OTP alarm                                   , */\
   { 0x4430, "int_in_flag_ocp_alarm"},    /* Clear ocp alarm                                   , */\
   { 0x4440, "int_in_flag_uvpok"},    /* Clear UVP alarm                                   , */\
   { 0x4450, "int_in_flag_man_alarm_state"},    /* clear  nanager Alarm state                        , */\
   { 0x4460, "int_in_flag_tdm_error"},    /* Clear tdm error                                   , */\
   { 0x4470, "int_in_flag_lost_clk"},    /* Clear lost clk                                    , */\
   { 0x4800, "int_enable_flag_por"},    /* Enable por                                        , */\
   { 0x4810, "int_enable_flag_bst_ocpok"},    /* Enable DCDC OCP                                   , */\
   { 0x4820, "int_enable_flag_otpok"},    /* Enable OTP alarm                                  , */\
   { 0x4830, "int_enable_flag_ocp_alarm"},    /* Enable ocp alarm                                  , */\
   { 0x4840, "int_enable_flag_uvpok"},    /* Enable UVP alarm                                  , */\
   { 0x4850, "int_enable_flag_man_alarm_state"},    /* Enable  nanager Alarm state                       , */\
   { 0x4860, "int_enable_flag_tdm_error"},    /* Enable tdm error                                  , */\
   { 0x4870, "int_enable_flag_lost_clk"},    /* Enable lost clk                                   , */\
   { 0x4c00, "int_polarity_flag_por"},    /* Polarity por                                      , */\
   { 0x4c10, "int_polarity_flag_bst_ocpok"},    /* Polarity DCDC OCP                                 , */\
   { 0x4c20, "int_polarity_flag_otpok"},    /* Polarity OTP alarm                                , */\
   { 0x4c30, "int_polarity_flag_ocp_alarm"},    /* Polarity ocp alarm                                , */\
   { 0x4c40, "int_polarity_flag_uvpok"},    /* Polarity UVP alarm                                , */\
   { 0x4c50, "int_polarity_flag_man_alarm_state"},    /* Polarity  nanager Alarm state                     , */\
   { 0x4c60, "int_polarity_flag_tdm_error"},    /* Polarity tdm error                                , */\
   { 0x4c70, "int_polarity_flag_lost_clk"},    /* Polarity lost clk                                 , */\
   { 0x5001, "vbat_prot_attack_time"},    /* Battery Safeguard attack time                     , */\
   { 0x5023, "vbat_prot_thlevel"},    /* Battery Safeguard threshold voltage level         , */\
   { 0x5061, "vbat_prot_max_reduct"},    /* Battery Safeguard maximum reduction               , */\
   { 0x5080, "vbat_flt_limit"},    /* vbat filter limit                                 , */\
   { 0x50d0, "rst_min_vbat"},    /* Reset clipper - Auto clear                        , */\
   { 0x50e0, "sel_vbat"},    /* Battery voltage read out                          , */\
   { 0x50f0, "bypass_clipper"},    /* Bypass battery safeguard                          , */\
   { 0x5100, "batsense_steepness"},    /* Vbat prot steepness                               , */\
   { 0x5150, "bypass_hp"},    /* Bypass HPF                                        , */\
   { 0x5170, "enbl_dpsa"},    /* Enable DPSA                                       , */\
   { 0x5222, "ctrl_cc"},    /* Clip control setting                              , */\
   { 0x5257, "gain"},    /* Amplifier gain                                    , */\
   { 0x52d0, "ctrl_slopectrl"},    /* Enables slope control                             , */\
   { 0x52e0, "ctrl_slope"},    /* Slope speed setting (bin. coded)                  , */\
   { 0x5301, "dpsa_level"},    /* DPSA threshold levels                             , */\
   { 0x5321, "dpsa_release"},    /* DPSA Release time                                 , */\
   { 0x5340, "clipfast"},    /* Clock selection for HW clipper for Battery Safeguard, */\
   { 0x5350, "bypass_lp"},    /* Bypass the low power filter inside temperature sensor, */\
   { 0x5400, "first_order_mode"},    /* Overrule to 1st order mode of control stage when clipping, */\
   { 0x5410, "bypass_ctrlloop"},    /* Switch amplifier into open loop configuration     , */\
   { 0x5430, "icomp_engage"},    /* Engage of icomp                                   , */\
   { 0x5440, "ctrl_kickback"},    /* Prevent double pulses of output stage             , */\
   { 0x5450, "icomp_engage_overrule"},    /* To overrule the functional icomp_engage signal during validation, */\
   { 0x5503, "ctrl_dem"},    /* Enable DEM icomp and DEM one bit dac              , */\
   { 0x5543, "ctrl_dem_mismatch"},    /* Enable DEM icomp mismatch for testing             , */\
   { 0x5582, "dpsa_drive"},    /* Drive setting (bin. coded)                        , */\
   { 0x5690, "sel_pwm_delay_src"},    /* Control for selection for PWM delay line source   , */\
   { 0x56a1, "enbl_odd_up_even_down"},    /* Control for PWM reference sawtooth generartion    , */\
   { 0x570a, "enbl_amp"},    /* Switch on the class-D power sections, each part of the analog sections can be switched on/off individually, */\
   { 0x57b0, "enbl_engage"},    /* Enables/engage power stage and control loop       , */\
   { 0x57c0, "enbl_engage_pst"},    /* Enables/engage power stage and control loop       , */\
   { 0x5810, "hard_mute"},    /* Hard mute - PWM                                   , */\
   { 0x5820, "pwm_shape"},    /* PWM shape                                         , */\
   { 0x5844, "pwm_delay"},    /* PWM delay bits to set the delay, clockd is 1/(k*2048*fs), */\
   { 0x5890, "reclock_pwm"},    /* Reclock the pwm signal inside analog              , */\
   { 0x58a0, "reclock_voltsense"},    /* Reclock the voltage sense pwm signal              , */\
   { 0x58c0, "enbl_pwm_phase_shift"},    /* Control for pwm phase shift                       , */\
   { 0x6123, "ctrl_attl"},    /* Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE), */\
   { 0x6163, "ctrl_attr"},    /* Total gain depending on INPLEV setting (channel 0), */\
   { 0x6265, "zero_lvl"},    /* low noise gain switch zero trigger level          , */\
   { 0x62c1, "ctrl_fb_resistor"},    /* Select amplifier feedback resistor connection     , */\
   { 0x62e1, "lownoisegain_mode"},    /* ctrl select mode                                  , */\
   { 0x6305, "threshold_lvl"},    /* low noise gain switch trigger level               , */\
   { 0x6365, "hold_time"},    /* ctrl hold time before low audio is reckoned to be low audio, */\
   { 0x6405, "lpm1_cal_offset"},    /* low power mode1 detector   ctrl cal_offset from gain module , */\
   { 0x6465, "lpm1_zero_lvl"},    /* low power mode1   zero crossing  detection level  , */\
   { 0x64e1, "lpm1_mode"},    /* low power mode control                            , */\
   { 0x6505, "lpm1_threshold_lvl"},    /* low power mode1  amplitude trigger level          , */\
   { 0x6565, "lpm1_hold_time"},    /* low power mode1 detector   ctrl hold time before low audio is reckoned to be low audio, */\
   { 0x65c0, "disable_low_power_mode"},    /* low power mode1  detector control                 , */\
   { 0x6600, "dcdc_pfm20khz_limit"},    /* DCDC in PFM mode pwm mode is activated each 50us to force a pwm pulse, */\
   { 0x6611, "dcdc_ctrl_maxzercnt"},    /* DCDC. Number of zero current flags to count before going to pfm mode, */\
   { 0x6656, "dcdc_vbat_delta_detect"},    /* Threshold before booster is reacting on a delta Vbat (in PFM mode) by temporarily switching to PWM mode, */\
   { 0x66c0, "dcdc_ignore_vbat"},    /* Ignore an increase on Vbat                        , */\
   { 0x6700, "enbl_minion"},    /* Enables minion (small) power stage                , */\
   { 0x6713, "vth_vddpvbat"},    /* select vddp-vbat thres signal                     , */\
   { 0x6750, "lpen_vddpvbat"},    /* select vddp-vbat filtred vs unfiltered compare    , */\
   { 0x6761, "ctrl_rfb"},    /* Feedback resistor selection  - I2C direct mode    , */\
   { 0x6802, "tdm_source_mapping"},    /* tdm source mapping                                , */\
   { 0x6831, "tdm_sourcea_frame_sel"},    /* Sensed value  A                                   , */\
   { 0x6851, "tdm_sourceb_frame_sel"},    /* Sensed value  B                                   , */\
   { 0x6871, "tdm_source0_clip_sel"},    /* clip information  (analog /digital) for source0   , */\
   { 0x6891, "tdm_source1_clip_sel"},    /* clip information  (analog /digital) for source1   , */\
   { 0x6a02, "rst_min_vbat_delay"},    /* rst_min_vbat delay (nb fs)                        , */\
   { 0x6b00, "disable_auto_engage"},    /* disable auto engange                              , */\
   { 0x6b10, "disable_engage"},    /* disable engange                                   , */\
   { 0x6c02, "ns_hp2ln_criterion"},    /* 0..7 zeroes at ns as threshold to swap from high_power to low_noise, */\
   { 0x6c32, "ns_ln2hp_criterion"},    /* 0..7 zeroes at ns as threshold to swap from low_noise to high_power, */\
   { 0x6c69, "spare_out"},    /* spare_out                                         , */\
   { 0x6d0f, "spare_in"},    /* spare_in                                          , */\
   { 0x6e10, "flag_lp_detect_mode1"},    /* low power mode 1 detection                        , */\
   { 0x6e20, "flag_low_amplitude"},    /* low amplitude detection                           , */\
   { 0x6e30, "flag_vddp_gt_vbat"},    /* vddp greater than vbat                            , */\
   { 0x6f02, "cursense_comp_delay"},    /* delay to allign compensation signal with current sense signal, */\
   { 0x6f40, "cursense_comp_sign"},    /* polarity of compensation for current sense        , */\
   { 0x6f50, "enbl_cursense_comp"},    /* enable current sense compensation                 , */\
   { 0x6f72, "pwms_clip_lvl"},    /* set the amount of pwm pulse that may be skipped before clip-flag is triggered, */\
   { 0x7033, "boost_cur"},    /* Max coil current                                  , */\
   { 0x7071, "bst_slpcmplvl"},    /* Slope compensation current, represents LxF (inductance x frequency) value , */\
   { 0x7090, "boost_intel"},    /* Adaptive boost mode                               , */\
   { 0x70a0, "boost_speed"},    /* Soft ramp up/down                                 , */\
   { 0x70e0, "dcdcoff_mode"},    /* DCDC on/off                                       , */\
   { 0x70f0, "dcdc_pwmonly"},    /* DCDC PWM only mode                                , */\
   { 0x7104, "bst_drive"},    /* Binary coded drive setting for boost converter power stage, */\
   { 0x7151, "bst_scalecur"},    /* For testing direct control scale current          , */\
   { 0x7174, "bst_slopecur"},    /* For testing direct control slope current          , */\
   { 0x71c1, "bst_slope"},    /* Boost slope speed                                 , */\
   { 0x71e0, "bst_bypass_bstcur"},    /* Bypass control for boost current settings         , */\
   { 0x71f0, "bst_bypass_bstfoldback"},    /* Bypass control for boost foldback                 , */\
   { 0x7200, "enbl_bst_engage"},    /* Enable power stage dcdc controller                , */\
   { 0x7210, "enbl_bst_hizcom"},    /* Enable hiz comparator                             , */\
   { 0x7220, "enbl_bst_peak2avg"},    /* Enable boost peak2avg functionality               , */\
   { 0x7230, "enbl_bst_peakcur"},    /* Enable peak current                               , */\
   { 0x7240, "enbl_bst_power"},    /* Enable line of the powerstage                     , */\
   { 0x7250, "enbl_bst_slopecur"},    /* Enable bit of max-current dac                     , */\
   { 0x7260, "enbl_bst_voutcomp"},    /* Enable vout comparators                           , */\
   { 0x7270, "enbl_bst_voutcomp86"},    /* Enable vout-86 comparators                        , */\
   { 0x7280, "enbl_bst_voutcomp93"},    /* Enable vout-93 comparators                        , */\
   { 0x7290, "enbl_bst_windac"},    /* Enable window dac                                 , */\
   { 0x72a5, "bst_windac"},    /* for testing direct control windac                 , */\
   { 0x7300, "boost_alg"},    /* Control for boost adaptive loop gain              , */\
   { 0x7311, "boost_loopgain"},    /* DCDC boost loopgain setting                       , */\
   { 0x7331, "bst_freq"},    /* DCDC boost frequency control                      , */\
   { 0x7360, "bst_use_new_zercur_detect"},    /* Enable new zero current detection for boost control, */\
   { 0x7430, "boost_track"},    /* Boost algorithm selection, effective only when boost_intelligent is set to 1, */\
   { 0x7444, "boost_trip_lvl_1st"},    /* 1st Adaptive boost trip levels, effective only when DCIE is set to 1, */\
   { 0x7494, "boost_hold_time"},    /* Hold time for DCDC booster, effective only when boost_intelligent is set to 1, */\
   { 0x74e0, "sel_dcdc_envelope_8fs"},    /* Selection of data for adaptive boost algorithm, effective only when boost_intelligent is set to 1, */\
   { 0x74f0, "ignore_flag_voutcomp86"},    /* Determines the maximum PWM frequency be the most efficient in relation to the Booster inductor value, */\
   { 0x7534, "boost_trip_lvl_2nd"},    /* 2nd Adaptive boost trip levels, effective only when DCIE is set to 1, */\
   { 0x7584, "boost_trip_lvl_track"},    /* Track Adaptive boost trip levels, effective only when boost_intelligent is set to 1, */\
   { 0x75f0, "enbl_trip_hyst"},    /* Enable hysteresis on booster trip levels          , */\
   { 0x7635, "frst_boost_voltage"},    /* First boost voltage level                         , */\
   { 0x7695, "scnd_boost_voltage"},    /* Second boost voltage level                        , */\
   { 0x8001, "sel_clk_cs"},    /* Current sense clock duty cycle control            , */\
   { 0x8021, "micadc_speed"},    /* Current sense clock for MiCADC selection - 32/44.1/48 KHz Fs band only, */\
   { 0x8050, "cs_gain_control"},    /* Current sense gain control                        , */\
   { 0x8060, "cs_bypass_gc"},    /* Bypasses the CS gain correction                   , */\
   { 0x8087, "cs_gain"},    /* Current sense gain                                , */\
   { 0x8210, "invertpwm"},    /* Current sense common mode feedback pwm invert control, */\
   { 0x8305, "cs_ktemp"},    /* Current sense temperature compensation trimming (1 - VALUE*TEMP)*signal, */\
   { 0x8364, "cs_ktemp2"},    /* Second order temperature compensation coefficient , */\
   { 0x8400, "cs_adc_bsoinv"},    /* Bitstream inversion for current sense ADC         , */\
   { 0x8421, "cs_adc_hifreq"},    /* Frequency mode current sense ADC                  , */\
   { 0x8440, "cs_adc_nortz"},    /* Return to zero for current sense ADC              , */\
   { 0x8453, "cs_adc_offset"},    /* Micadc ADC offset setting                         , */\
   { 0x8490, "cs_adc_slowdel"},    /* Select delay for current sense ADC (internal decision circuitry), */\
   { 0x84a4, "cs_adc_gain"},    /* Gain setting for current sense ADC (two's complement), */\
   { 0x8500, "cs_resonator_enable"},    /* Enable for resonator to improve SRN               , */\
   { 0x8510, "cs_classd_tran_skip"},    /* Skip current sense connection during a classD amplifier transition, */\
   { 0x8530, "cs_inn_short"},    /* Short current sense negative to common mode       , */\
   { 0x8540, "cs_inp_short"},    /* Short current sense positive to common mode       , */\
   { 0x8550, "cs_ldo_bypass"},    /* Bypass current sense LDO                          , */\
   { 0x8560, "cs_ldo_pulldown"},    /* Pull down current sense LDO, only valid if left_enbl_cs_ldo is high, */\
   { 0x8574, "cs_ldo_voset"},    /* Current sense LDO voltage level setting (two's complement), */\
   { 0x8700, "enbl_cs_adc"},    /* Enable current sense ADC                          , */\
   { 0x8710, "enbl_cs_inn1"},    /* Enable connection of current sense negative1      , */\
   { 0x8720, "enbl_cs_inn2"},    /* Enable connection of current sense negative2      , */\
   { 0x8730, "enbl_cs_inp1"},    /* Enable connection of current sense positive1      , */\
   { 0x8740, "enbl_cs_inp2"},    /* Enable connection of current sense positive2      , */\
   { 0x8750, "enbl_cs_ldo"},    /* Enable current sense LDO                          , */\
   { 0x8780, "enbl_cs_vbatldo"},    /* Enable of current sense LDO                       , */\
   { 0x8790, "enbl_dc_filter"},    /* Control for enabling the DC blocking filter for voltage and current sense, */\
   { 0x8801, "volsense_pwm_sel"},    /* Voltage sense  source selection control           , */\
   { 0x8850, "vs_gain_control"},    /* Voltage sense gain control                        , */\
   { 0x8860, "vs_bypass_gc"},    /* Bypasses the VS gain correction                   , */\
   { 0x8870, "vs_igen_supply"},    /* Switch internal supply of current generator       , */\
   { 0x8887, "vs_gain"},    /* voltage sense gain                                , */\
   { 0x8c00, "vs_adc_bsoinv"},    /* Bitstream inversion for voltage sense ADC         , */\
   { 0x8c40, "vs_adc_nortz"},    /* Return to zero for voltage sense ADC              , */\
   { 0x8c90, "vs_adc_slowdel"},    /* Select delay for voltage sense ADC (internal decision circuitry), */\
   { 0x8d10, "vs_classd_tran_skip"},    /* Skip voltage sense connection during a classD amplifier transition, */\
   { 0x8d30, "vs_inn_short"},    /* Short voltage sense negative to common mode       , */\
   { 0x8d40, "vs_inp_short"},    /* Short voltage sense positive to common mode       , */\
   { 0x8d50, "vs_ldo_bypass"},    /* Bypass voltage sense LDO                          , */\
   { 0x8d60, "vs_ldo_pulldown"},    /* Pull down voltage sense LDO, only valid if left_enbl_cs_ldo is high, */\
   { 0x8d74, "vs_ldo_voset"},    /* Voltage sense LDO voltage level setting (two's complement), */\
   { 0x8f00, "enbl_vs_adc"},    /* Enable voltage sense ADC  (Direct Control only only others done by manager), */\
   { 0x8f10, "enbl_vs_inn1"},    /* Enable connection of voltage sense negative1      , */\
   { 0x8f20, "enbl_vs_inn2"},    /* Enable connection of voltage sense negative2      , */\
   { 0x8f30, "enbl_vs_inp1"},    /* Enable connection of voltage sense positive1      , */\
   { 0x8f40, "enbl_vs_inp2"},    /* Enable connection of voltage sense positive2      , */\
   { 0x8f50, "enbl_vs_ldo"},    /* Enable voltage sense LDO (Direct Control only only others done by manager), */\
   { 0x8f80, "enbl_vs_vbatldo"},    /* Enable of voltage sense LDO (Direct Control only others done by manager), */\
   { 0xa007, "mtpkey1"},    /* 5Ah, 90d To access KEY1_Protected registers (Default for engineering), */\
   { 0xa107, "mtpkey2"},    /* MTP KEY2 register                                 , */\
   { 0xa200, "key01_locked"},    /* Indicates KEY1 is locked                          , */\
   { 0xa210, "key02_locked"},    /* Indicates KEY2 is locked                          , */\
   { 0xa302, "mtp_man_address_in"},    /* MTP address from I2C register for read/writing mtp in manual single word mode, */\
   { 0xa330, "man_copy_mtp_to_iic"},    /* Start copying single word from mtp to I2C mtp register, */\
   { 0xa340, "man_copy_iic_to_mtp"},    /* Start copying single word from I2C mtp register to mtp, */\
   { 0xa350, "auto_copy_mtp_to_iic"},    /* Start copying all the data from mtp to I2C mtp registers, */\
   { 0xa360, "auto_copy_iic_to_mtp"},    /* Start copying data from I2C mtp registers to mtp  , */\
   { 0xa400, "faim_set_clkws"},    /* Sets the faim controller clock wait state register, */\
   { 0xa410, "faim_sel_evenrows"},    /* All even rows of the faim are selected, active high, */\
   { 0xa420, "faim_sel_oddrows"},    /* All odd rows of the faim are selected, all rows in combination with sel_evenrows, */\
   { 0xa430, "faim_program_only"},    /* Skip the erase access at wr_faim command (write-program-marginread), */\
   { 0xa440, "faim_erase_only"},    /* Skip the program access at wr_faim command (write-erase-marginread), */\
   { 0xa50f, "mtp_man_data_out_msb"},    /* MSB word of MTP manual read data                  , */\
   { 0xa60f, "mtp_man_data_out_lsb"},    /* LSB word of MTP manual read data                  , */\
   { 0xa70f, "mtp_man_data_in_msb"},    /* MSB word of write data for MTP manual write       , */\
   { 0xa80f, "mtp_man_data_in_lsb"},    /* LSB word of write data for MTP manual write       , */\
   { 0xb010, "bypass_ocpcounter"},    /* Bypass OCP Counter                                , */\
   { 0xb020, "bypass_glitchfilter"},    /* Bypass glitch filter                              , */\
   { 0xb030, "bypass_ovp"},    /* Bypass OVP                                        , */\
   { 0xb040, "bypass_uvp"},    /* Bypass UVP                                        , */\
   { 0xb050, "bypass_otp"},    /* Bypass OTP                                        , */\
   { 0xb060, "bypass_lost_clk"},    /* Bypass lost clock detector                        , */\
   { 0xb070, "ctrl_vpalarm"},    /* vpalarm (uvp ovp handling)                        , */\
   { 0xb087, "ocp_threshold"},    /* OCP threshold level                               , */\
   { 0xb108, "ext_temp"},    /* External temperature (C)                          , */\
   { 0xb190, "ext_temp_sel"},    /* Select temp Speaker calibration                   , */\
   { 0xc000, "use_direct_ctrls"},    /* Direct control to overrule several functions for testing, */\
   { 0xc010, "rst_datapath"},    /* Direct control for datapath reset                 , */\
   { 0xc020, "rst_cgu"},    /* Direct control for cgu reset                      , */\
   { 0xc038, "enbl_ref"},    /* Switch on the analog references, each part of the references can be switched on/off individually, */\
   { 0xc0c0, "use_direct_vs_ctrls"},    /* voltage sense Direct control to overrule several functions for testing, */\
   { 0xc0d0, "enbl_ringo"},    /* Enable the ring oscillator for test purpose       , */\
   { 0xc0e0, "use_direct_clk_ctrl"},    /* Direct clock control to overrule several functions for testing, */\
   { 0xc0f0, "use_direct_pll_ctrl"},    /* Direct PLL control to overrule several functions for testing, */\
   { 0xc100, "enbl_tsense"},    /* Temperature sensor enable control - I2C direct mode, */\
   { 0xc110, "tsense_hibias"},    /* Bit to set the biasing in temp sensor to high     , */\
   { 0xc120, "enbl_flag_vbg"},    /* Enable flagging of bandgap out of control         , */\
   { 0xc20f, "abist_offset"},    /* Offset control for ABIST testing (two's complement), */\
   { 0xc300, "bypasslatch"},    /* Bypass latch                                      , */\
   { 0xc311, "sourcea"},    /* Set OUTA to                                       , */\
   { 0xc331, "sourceb"},    /* Set OUTB to                                       , */\
   { 0xc350, "inverta"},    /* Invert pwma test signal                           , */\
   { 0xc360, "invertb"},    /* Invert pwmb test signal                           , */\
   { 0xc374, "pulselength"},    /* Pulse length setting test input for amplifier (clock d - k*2048*fs), */\
   { 0xc3c0, "tdm_enable_loopback"},    /* TDM loopback test                                 , */\
   { 0xc400, "bst_bypasslatch"},    /* Bypass latch in boost converter                   , */\
   { 0xc411, "bst_source"},    /* Sets the source of the pwmbst output to boost converter input for testing, */\
   { 0xc430, "bst_invertb"},    /* Invert pwmbst test signal                         , */\
   { 0xc444, "bst_pulselength"},    /* Pulse length setting test input for boost converter , */\
   { 0xc490, "test_bst_ctrlsthv"},    /* Test mode for boost control stage                 , */\
   { 0xc4a0, "test_bst_iddq"},    /* IDDQ testing in power stage of boost converter    , */\
   { 0xc4b0, "test_bst_rdson"},    /* RDSON testing - boost power stage                 , */\
   { 0xc4c0, "test_bst_cvi"},    /* CVI testing - boost power stage                   , */\
   { 0xc4d0, "test_bst_ocp"},    /* Boost OCP. For old ocp (ctrl_reversebst is 0), For new ocp (ctrl_reversebst is 1), */\
   { 0xc4e0, "test_bst_sense"},    /* Test option for the sense NMOS in booster for current mode control., */\
   { 0xc500, "test_cvi"},    /* Analog BIST, switch choose which transistor will be used as current source (also cross coupled sources possible), */\
   { 0xc510, "test_discrete"},    /* Test function noise measurement                   , */\
   { 0xc520, "test_iddq"},    /* Set the power stages in iddq mode for gate stress., */\
   { 0xc540, "test_rdson"},    /* Analog BIST, switch to enable Rdson measurement   , */\
   { 0xc550, "test_sdelta"},    /* Analog BIST, noise test                           , */\
   { 0xc570, "test_enbl_cs"},    /* Enable for digimux mode of current sense          , */\
   { 0xc580, "test_enbl_vs"},    /* Enable for digimux mode of voltage sense          , */\
   { 0xc600, "enbl_pwm_dcc"},    /* Enables direct control of pwm duty cycle for DCDC power stage, */\
   { 0xc613, "pwm_dcc_cnt"},    /* Control pwm duty cycle when enbl_pwm_dcc is 1     , */\
   { 0xc650, "enbl_ldo_stress"},    /* Enable stress of internal supply voltages powerstages, */\
   { 0xc707, "digimuxa_sel"},    /* DigimuxA input selection control routed to DATAO (see Digimux list for details), */\
   { 0xc787, "digimuxb_sel"},    /* DigimuxB input selection control routed to INT (see Digimux list for details), */\
   { 0xc807, "digimuxc_sel"},    /* DigimuxC input selection control routed to PDMDAT (see Digimux list for details), */\
   { 0xc981, "int_ehs"},    /* Speed/load setting for INT IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
   { 0xc9c0, "hs_mode"},    /* I2C high speed mode control                       , */\
   { 0xca00, "enbl_anamux1"},    /* Enable anamux1                                    , */\
   { 0xca10, "enbl_anamux2"},    /* Enable anamux2                                    , */\
   { 0xca20, "enbl_anamux3"},    /* Enable anamux3                                    , */\
   { 0xca30, "enbl_anamux4"},    /* Enable anamux4                                    , */\
   { 0xca74, "anamux1"},    /* Anamux selection control - anamux on TEST1        , */\
   { 0xcb04, "anamux2"},    /* Anamux selection control - anamux on TEST2        , */\
   { 0xcb53, "anamux3"},    /* Anamux selection control - anamux on TEST3        , */\
   { 0xcba3, "anamux4"},    /* Anamux selection control - anamux on TEST4        , */\
   { 0xcd05, "pll_seli"},    /* PLL SELI - I2C direct PLL control mode only       , */\
   { 0xcd64, "pll_selp"},    /* PLL SELP - I2C direct PLL control mode only       , */\
   { 0xcdb3, "pll_selr"},    /* PLL SELR - I2C direct PLL control mode only       , */\
   { 0xcdf0, "pll_frm"},    /* PLL free running mode control; 1 in TCB direct control mode, else this control bit, */\
   { 0xce09, "pll_ndec"},    /* PLL NDEC - I2C direct PLL control mode only       , */\
   { 0xcea0, "pll_mdec_msb"},    /* MSB of pll_mdec - I2C direct PLL control mode only, */\
   { 0xceb0, "enbl_pll"},    /* Enables PLL in I2C direct PLL control mode only   , */\
   { 0xcec0, "enbl_osc"},    /* Enables OSC1M in I2C direct control mode only     , */\
   { 0xced0, "pll_bypass"},    /* PLL bypass control in I2C direct PLL control mode only, */\
   { 0xcee0, "pll_directi"},    /* PLL directi control in I2C direct PLL control mode only, */\
   { 0xcef0, "pll_directo"},    /* PLL directo control in I2C direct PLL control mode only, */\
   { 0xcf0f, "pll_mdec_lsb"},    /* Bits 15..0 of PLL MDEC are I2C direct PLL control mode only, */\
   { 0xd006, "pll_pdec"},    /* PLL PDEC - I2C direct PLL control mode only       , */\
   { 0xd10f, "tsig_freq_lsb"},    /* Internal sinus test generator frequency control   , */\
   { 0xd202, "tsig_freq_msb"},    /* Select internal sinus test generator, frequency control msb bits, */\
   { 0xd230, "inject_tsig"},    /* Control bit to switch to internal sinus test generator, */\
   { 0xd283, "tsig_gain"},    /* Test signal gain                                  , */\
   { 0xd300, "adc10_reset"},    /* Reset for ADC10 - I2C direct control mode         , */\
   { 0xd311, "adc10_test"},    /* Test mode selection signal for ADC10 - I2C direct control mode, */\
   { 0xd332, "adc10_sel"},    /* Select the input to convert for ADC10 - I2C direct control mode, */\
   { 0xd364, "adc10_prog_sample"},    /* ADC10 program sample setting - I2C direct control mode, */\
   { 0xd3b0, "adc10_enbl"},    /* Enable ADC10 - I2C direct control mode            , */\
   { 0xd3c0, "bypass_lp_vbat"},    /* Bypass control for Low pass filter in batt sensor , */\
   { 0xd409, "data_adc10_tempbat"},    /* ADC 10 data output data for testing               , */\
   { 0xd507, "ctrl_digtoana_hidden"},    /* Spare digital to analog control bits - Hidden     , */\
   { 0xd580, "enbl_clk_out_of_range"},    /* Clock out of range                                , */\
   { 0xd621, "clkdiv_audio_sel"},    /* Audio clock divider selection in direct clock control mode, */\
   { 0xd641, "clkdiv_muxa_sel"},    /* DCDC MUXA clock divider selection in direct clock control mode, */\
   { 0xd661, "clkdiv_muxb_sel"},    /* DCDC MUXB clock divider selection in direct clock control mode, */\
   { 0xd721, "datao_ehs"},    /* Speed/load setting for DATAO  IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
   { 0xd740, "bck_ehs"},    /* High-speed and standard/fast mode selection for BCK  IO cell (see IIC3V3  IO cell datasheet), */\
   { 0xd750, "datai_ehs"},    /* High-speed and standard/fast mode selection for DATAI  IO cell (see IIC3V3  IO cell datasheet), */\
   { 0xd800, "source_in_testmode"},    /* tdm source in test mode (return only current and voltage sense), */\
   { 0xd810, "gainatt_feedback"},    /* gainatt feedback to tdm                           , */\
   { 0xd822, "test_parametric_io"},    /* test io parametric                                , */\
   { 0xd850, "ctrl_bst_clk_lp1"},    /* boost clock control in low power mode1            , */\
   { 0xd861, "test_spare_out1"},    /* test spare out 1                                  , */\
   { 0xd880, "bst_dcmbst"},    /* dcm boost                                         , */\
   { 0xd8c3, "test_spare_out2"},    /* test spare out 1                                  , */\
   { 0xee0f, "sw_profile"},    /* Software profile data                             , */\
   { 0xef0f, "sw_vstep"},    /* Software vstep information                        , */\
   { 0xf000, "calibration_onetime"},    /* Calibration schedule                              , */\
   { 0xf010, "calibr_ron_done"},    /* Calibration Ron executed                          , */\
   { 0xf020, "calibr_dcdc_api_calibrate"},    /* Calibration current limit DCDC                    , */\
   { 0xf030, "calibr_dcdc_delta_sign"},    /* Sign bit for delta calibration current limit DCDC , */\
   { 0xf042, "calibr_dcdc_delta"},    /* Calibration delta current limit DCDC              , */\
   { 0xf078, "calibr_speaker_info"},    /* Reserved space for allowing customer to store speaker information, */\
   { 0xf105, "calibr_vout_offset"},    /* DCDC offset calibration 2's complement (key1 protected), */\
   { 0xf169, "spare_mpt1_15_6"},    /* SPARE                                             , */\
   { 0xf203, "calibr_gain"},    /* HW gain module  (2's complement)                  , */\
   { 0xf245, "calibr_offset"},    /* Offset for amplifier, HW gain module (2's complement), */\
   { 0xf2a5, "spare_mtp2_15_10"},    /* SPARE                                             , */\
   { 0xf307, "calibr_gain_vs"},    /* Voltage sense gain                                , */\
   { 0xf387, "calibr_gain_cs"},    /* Current sense gain (signed two's complement format), */\
   { 0xf407, "spare_mtp4_15_0"},    /* SPARE                                             , */\
   { 0xf487, "vs_trim"},    /* VS Trimming                                       , */\
   { 0xf50f, "calibr_R25C_R"},    /* Ron resistance of  speaker coil                   , */\
   { 0xf60f, "spare_mpt6_6_0"},    /* SPARE                                             , */\
   { 0xf706, "ctrl_offset_a"},    /* Offset of  level shifter A                        , */\
   { 0xf770, "spare_mtp7_07"},    /* SPARE                                             , */\
   { 0xf786, "ctrl_offset_b"},    /* Offset of  amplifier level shifter B              , */\
   { 0xf7f0, "spare_mtp7_15"},    /* SPARE                                             , */\
   { 0xf806, "htol_iic_addr"},    /* 7-bit I2C address to be used during HTOL testing  , */\
   { 0xf870, "htol_iic_addr_en"},    /* HTOL I2C address enable control                   , */\
   { 0xf884, "calibr_temp_offset"},    /* Temperature offset 2's compliment (key1 protected), */\
   { 0xf8d2, "calibr_temp_gain"},    /* Temperature gain 2's compliment (key1 protected)  , */\
   { 0xf900, "mtp_lock_dcdcoff_mode"},    /* Disable function dcdcoff_mode                     , */\
   { 0xf910, "spare_mtp9_1"},    /* SPARE                                             , */\
   { 0xf920, "mtp_lock_bypass_clipper"},    /* Disable function bypass_clipper                   , */\
   { 0xf930, "mtp_lock_max_dcdc_voltage"},    /* Force Boost in follower mode                      , */\
   { 0xf943, "calibr_vbg_trim"},    /* Bandgap trimming control                          , */\
   { 0xf980, "spare_mtp9_8"},    /* SPARE                                             , */\
   { 0xf990, "mtp_enbl_pwm_delay_clock_gating"},    /* pwm delay clock auto gating                       , */\
   { 0xf9a0, "mtp_enbl_ocp_clock_gating"},    /* ocpclock auto gating                              , */\
   { 0xf9b0, "mtp_gate_cgu_clock_for_test"},    /* cgu test clock control                            , */\
   { 0xf9c0, "mtp_tdm_pad_sel"},    /* tdm pad selection                                 , */\
   { 0xf9d2, "spare_mtp9_15_12"},    /* MTP-control FW - See Firmware I2C API document for details, */\
   { 0xfa0f, "mtpdataA"},    /* MTPdataA (key1 protected)                         , */\
   { 0xfb0f, "mtpdataB"},    /* MTPdataB (key1 protected)                         , */\
   { 0xfc0f, "mtpdataC"},    /* MTPdataC (key1 protected)                         , */\
   { 0xfd0f, "mtpdataD"},    /* MTPdataD (key1 protected)                         , */\
   { 0xfe0f, "mtpdataE"},    /* MTPdataE (key1 protected)                         , */\
   { 0xff07, "calibr_osc_delta_ndiv"},    /* Calibration data for OSC1M, signed number representation, */\
   { 0xff87, "spare_mtp7_15_08"},    /* SPARE                                             , */\
   { 0xffff,"Unknown bitfield enum" }    /* not found */\
};

enum tfa9874_irq {
	tfa9874_irq_stvdds = 0,
	tfa9874_irq_stbstoc = 1,
	tfa9874_irq_stotds = 2,
	tfa9874_irq_stocpr = 3,
	tfa9874_irq_stuvds = 4,
	tfa9874_irq_stmanalarm = 5,
	tfa9874_irq_sttdmer = 6,
	tfa9874_irq_stnoclk = 7,
	tfa9874_irq_max = 8,
	tfa9874_irq_all = -1 /* all irqs */};

#define TFA9874_IRQ_NAMETABLE static tfaIrqName_t Tfa9874IrqNames[]= {\
	{ 0, "STVDDS"},\
	{ 1, "STBSTOC"},\
	{ 2, "STOTDS"},\
	{ 3, "STOCPR"},\
	{ 4, "STUVDS"},\
	{ 5, "STMANALARM"},\
	{ 6, "STTDMER"},\
	{ 7, "STNOCLK"},\
	{ 8, "8"},\
};
#endif /* _TFA9874_TFAFIELDNAMES_H */
