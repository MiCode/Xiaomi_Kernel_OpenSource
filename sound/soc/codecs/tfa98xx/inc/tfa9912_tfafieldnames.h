/* 
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


/** Filename: tfa9912_tfaFieldnames.h
 *  This file was generated automatically on 09/28/18 at 12:00:21. 
 *  Source file: TFA9912_N1A_I2C_regmap_V1.43.xlsx
 */

#ifndef _TFA9912_TFAFIELDNAMES_H
#define _TFA9912_TFAFIELDNAMES_H


#define TFA9912_I2CVERSION    1.43

typedef enum Tfa9912BfEnumList {
    TFA9912_BF_PWDN  = 0x0000,    /*!< Powerdown selection                                */
    TFA9912_BF_I2CR  = 0x0010,    /*!< I2C Reset - Auto clear                             */
    TFA9912_BF_CFE   = 0x0020,    /*!< Enable CoolFlux                                    */
    TFA9912_BF_AMPE  = 0x0030,    /*!< Enables the Amplifier                              */
    TFA9912_BF_DCA   = 0x0040,    /*!< Activate DC-to-DC converter                        */
    TFA9912_BF_SBSL  = 0x0050,    /*!< Coolflux configured                                */
    TFA9912_BF_AMPC  = 0x0060,    /*!< CoolFlux controls amplifier                        */
    TFA9912_BF_INTP  = 0x0071,    /*!< Interrupt config                                   */
    TFA9912_BF_FSSSEL= 0x0090,    /*!< Audio sample reference                             */
    TFA9912_BF_BYPOCP= 0x00b0,    /*!< Bypass OCP                                         */
    TFA9912_BF_TSTOCP= 0x00c0,    /*!< OCP testing control                                */
    TFA9912_BF_AMPINSEL= 0x0101,    /*!< Amplifier input selection                          */
    TFA9912_BF_MANSCONF= 0x0120,    /*!< I2C configured                                     */
    TFA9912_BF_MANCOLD= 0x0130,    /*!< Execute cold start                                 */
    TFA9912_BF_MANAOOSC= 0x0140,    /*!< Internal osc off at PWDN                           */
    TFA9912_BF_MANROBOD= 0x0150,    /*!< Reaction on BOD                                    */
    TFA9912_BF_BODE  = 0x0160,    /*!< BOD Enable                                         */
    TFA9912_BF_BODHYS= 0x0170,    /*!< BOD Hysteresis                                     */
    TFA9912_BF_BODFILT= 0x0181,    /*!< BOD filter                                         */
    TFA9912_BF_BODTHLVL= 0x01a1,    /*!< BOD threshold                                      */
    TFA9912_BF_MUTETO= 0x01d0,    /*!< Time out SB mute sequence                          */
    TFA9912_BF_RCVNS = 0x01e0,    /*!< Noise shaper selection                             */
    TFA9912_BF_MANWDE= 0x01f0,    /*!< Watchdog enable                                    */
    TFA9912_BF_AUDFS = 0x0203,    /*!< Sample rate (fs)                                   */
    TFA9912_BF_INPLEV= 0x0240,    /*!< TDM output attenuation                             */
    TFA9912_BF_FRACTDEL= 0x0255,    /*!< V/I Fractional delay                               */
    TFA9912_BF_BYPHVBF= 0x02b0,    /*!< Bypass HVBAT filter                                */
    TFA9912_BF_TDMC  = 0x02c0,    /*!< TDM Compatibility with TFA9872                     */
    TFA9912_BF_ENBLADC10= 0x02e0,    /*!< ADC10 Enable -  I2C direct mode                    */
    TFA9912_BF_REV   = 0x030f,    /*!< Revision info                                      */
    TFA9912_BF_REFCKEXT= 0x0401,    /*!< PLL external ref clock                             */
    TFA9912_BF_REFCKSEL= 0x0420,    /*!< PLL internal ref clock                             */
    TFA9912_BF_ENCFCKSEL= 0x0430,    /*!< Coolflux DSP clock scaling, low power mode         */
    TFA9912_BF_CFCKSEL= 0x0441,    /*!< Coolflux DSP clock scaler selection for low power mode */
    TFA9912_BF_TDMINFSEL= 0x0460,    /*!< TDM clock selection                                */
    TFA9912_BF_DISBLAUTOCLKSEL= 0x0470,    /*!< Disable Automatic dsp clock source selection       */
    TFA9912_BF_SELCLKSRC= 0x0480,    /*!< I2C selection of DSP clock when auto select is disabled */
    TFA9912_BF_SELTIMSRC= 0x0490,    /*!< I2C selection of Watchdog and Timer clock          */
    TFA9912_BF_SSLEFTE= 0x0500,    /*!<                                                    */
    TFA9912_BF_SPKSSEN= 0x0510,    /*!< Enable speaker path                                */
    TFA9912_BF_VSLEFTE= 0x0520,    /*!<                                                    */
    TFA9912_BF_VSRIGHTE= 0x0530,    /*!< Voltage sense                                      */
    TFA9912_BF_CSLEFTE= 0x0540,    /*!<                                                    */
    TFA9912_BF_CSRIGHTE= 0x0550,    /*!< Current sense                                      */
    TFA9912_BF_SSPDME= 0x0560,    /*!< Sub-system PDM                                     */
    TFA9912_BF_PGALE = 0x0570,    /*!< Enable PGA chop clock for left channel             */
    TFA9912_BF_PGARE = 0x0580,    /*!< Enable PGA chop clock                              */
    TFA9912_BF_SSTDME= 0x0590,    /*!< Sub-system TDM                                     */
    TFA9912_BF_SSPBSTE= 0x05a0,    /*!< Sub-system boost                                   */
    TFA9912_BF_SSADCE= 0x05b0,    /*!< Sub-system ADC                                     */
    TFA9912_BF_SSFAIME= 0x05c0,    /*!< Sub-system FAIM                                    */
    TFA9912_BF_SSCFTIME= 0x05d0,    /*!< CF Sub-system timer                                */
    TFA9912_BF_SSCFWDTE= 0x05e0,    /*!< CF Sub-system WDT                                  */
    TFA9912_BF_FAIMVBGOVRRL= 0x05f0,    /*!< Over rule of vbg for FaIM access                   */
    TFA9912_BF_SAMSPKSEL= 0x0600,    /*!< Input selection for TAP/SAM                        */
    TFA9912_BF_PDM2IISEN= 0x0610,    /*!< PDM2IIS Bridge enable                              */
    TFA9912_BF_TAPRSTBYPASS= 0x0620,    /*!< Tap decimator reset bypass - Bypass the decimator reset from tapdec */
    TFA9912_BF_CARDECISEL0= 0x0631,    /*!< Cardec input 0 sel                                 */
    TFA9912_BF_CARDECISEL1= 0x0651,    /*!< Cardec input sel                                   */
    TFA9912_BF_TAPDECSEL= 0x0670,    /*!< Select TAP/Cardec for TAP                          */
    TFA9912_BF_COMPCOUNT= 0x0680,    /*!< Comparator o/p filter selection                    */
    TFA9912_BF_STARTUPMODE= 0x0691,    /*!< Startup Mode Selection                             */
    TFA9912_BF_AUTOTAP= 0x06b0,    /*!< Enable auto tap switching                          */
    TFA9912_BF_COMPINITIME= 0x06c1,    /*!< Comparator initialization time to be used in Tap Machine */
    TFA9912_BF_ANAPINITIME= 0x06e1,    /*!< Analog initialization time to be used in Tap Machine */
    TFA9912_BF_CCHKTH= 0x0707,    /*!< Clock check Higher Threshold                       */
    TFA9912_BF_CCHKTL= 0x0787,    /*!< Clock check Higher Threshold                       */
    TFA9912_BF_AMPOCRT= 0x0802,    /*!< Amplifier on-off criteria for shutdown             */
    TFA9912_BF_AMPTCRR= 0x0832,    /*!< Amplifier on-off criteria for tap mode entry       */
    TFA9912_BF_STGS  = 0x0d00,    /*!< PDM side tone gain selector                        */
    TFA9912_BF_STGAIN= 0x0d18,    /*!< Side tone gain                                     */
    TFA9912_BF_STSMUTE= 0x0da0,    /*!< Side tone soft mute                                */
    TFA9912_BF_ST1C  = 0x0db0,    /*!< side tone one s complement                         */
    TFA9912_BF_CMFBEL= 0x0e80,    /*!< CMFB enable left                                   */
    TFA9912_BF_VDDS  = 0x1000,    /*!< POR                                                */
    TFA9912_BF_PLLS  = 0x1010,    /*!< PLL lock                                           */
    TFA9912_BF_OTDS  = 0x1020,    /*!< OTP alarm                                          */
    TFA9912_BF_OVDS  = 0x1030,    /*!< OVP alarm                                          */
    TFA9912_BF_UVDS  = 0x1040,    /*!< UVP alarm                                          */
    TFA9912_BF_CLKS  = 0x1050,    /*!< Clocks stable                                      */
    TFA9912_BF_MTPB  = 0x1060,    /*!< MTP busy                                           */
    TFA9912_BF_NOCLK = 0x1070,    /*!< Lost clock                                         */
    TFA9912_BF_ACS   = 0x1090,    /*!< Cold Start                                         */
    TFA9912_BF_SWS   = 0x10a0,    /*!< Amplifier engage                                   */
    TFA9912_BF_WDS   = 0x10b0,    /*!< Watchdog                                           */
    TFA9912_BF_AMPS  = 0x10c0,    /*!< Amplifier enable                                   */
    TFA9912_BF_AREFS = 0x10d0,    /*!< References enable                                  */
    TFA9912_BF_ADCCR = 0x10e0,    /*!< Control ADC                                        */
    TFA9912_BF_BODNOK= 0x10f0,    /*!< BOD                                                */
    TFA9912_BF_DCIL  = 0x1100,    /*!< DCDC current limiting                              */
    TFA9912_BF_DCDCA = 0x1110,    /*!< DCDC active                                        */
    TFA9912_BF_DCOCPOK= 0x1120,    /*!< DCDC OCP nmos                                      */
    TFA9912_BF_DCPEAKCUR= 0x1130,    /*!< Indicates current is max in DC-to-DC converter     */
    TFA9912_BF_DCHVBAT= 0x1140,    /*!< DCDC level 1x                                      */
    TFA9912_BF_DCH114= 0x1150,    /*!< DCDC level 1.14x                                   */
    TFA9912_BF_DCH107= 0x1160,    /*!< DCDC level 1.07x                                   */
    TFA9912_BF_STMUTEB= 0x1170,    /*!< side tone (un)mute busy                            */
    TFA9912_BF_STMUTE= 0x1180,    /*!< side tone mute state                               */
    TFA9912_BF_TDMLUTER= 0x1190,    /*!< TDM LUT error                                      */
    TFA9912_BF_TDMSTAT= 0x11a2,    /*!< TDM status bits                                    */
    TFA9912_BF_TDMERR= 0x11d0,    /*!< TDM error                                          */
    TFA9912_BF_HAPTIC= 0x11e0,    /*!< Status haptic driver                               */
    TFA9912_BF_OCPOAP= 0x1300,    /*!< OCPOK pmos A                                       */
    TFA9912_BF_OCPOAN= 0x1310,    /*!< OCPOK nmos A                                       */
    TFA9912_BF_OCPOBP= 0x1320,    /*!< OCPOK pmos B                                       */
    TFA9912_BF_OCPOBN= 0x1330,    /*!< OCPOK nmos B                                       */
    TFA9912_BF_CLIPAH= 0x1340,    /*!< Clipping A to Vddp                                 */
    TFA9912_BF_CLIPAL= 0x1350,    /*!< Clipping A to gnd                                  */
    TFA9912_BF_CLIPBH= 0x1360,    /*!< Clipping B to Vddp                                 */
    TFA9912_BF_CLIPBL= 0x1370,    /*!< Clipping B to gnd                                  */
    TFA9912_BF_OCDS  = 0x1380,    /*!< OCP  amplifier                                     */
    TFA9912_BF_CLIPS = 0x1390,    /*!< Amplifier  clipping                                */
    TFA9912_BF_TCMPTRG= 0x13a0,    /*!< Status Tap comparator triggered                    */
    TFA9912_BF_TAPDET= 0x13b0,    /*!< Status Tap  detected                               */
    TFA9912_BF_MANWAIT1= 0x13c0,    /*!< Wait HW I2C settings                               */
    TFA9912_BF_MANWAIT2= 0x13d0,    /*!< Wait CF config                                     */
    TFA9912_BF_MANMUTE= 0x13e0,    /*!< Audio mute sequence                                */
    TFA9912_BF_MANOPER= 0x13f0,    /*!< Operating state                                    */
    TFA9912_BF_SPKSL = 0x1400,    /*!< Left speaker status                                */
    TFA9912_BF_SPKS  = 0x1410,    /*!< Speaker status                                     */
    TFA9912_BF_CLKOOR= 0x1420,    /*!< External clock status                              */
    TFA9912_BF_MANSTATE= 0x1433,    /*!< Device manager status                              */
    TFA9912_BF_DCMODE= 0x1471,    /*!< DCDC mode status bits                              */
    TFA9912_BF_DSPCLKSRC= 0x1490,    /*!< DSP clock source selected by manager               */
    TFA9912_BF_STARTUPMODSTAT= 0x14a1,    /*!< Startup Mode Selected by Manager(Read Only)        */
    TFA9912_BF_TSPMSTATE= 0x14c3,    /*!< Tap Machine State                                  */
    TFA9912_BF_BATS  = 0x1509,    /*!< Battery voltage (V)                                */
    TFA9912_BF_TEMPS = 0x1608,    /*!< IC Temperature (C)                                 */
    TFA9912_BF_VDDPS = 0x1709,    /*!< IC VDDP voltage ( 1023*VDDP/13 V)                  */
    TFA9912_BF_DCILCF= 0x17a0,    /*!< DCDC current limiting for DSP                      */
    TFA9912_BF_TDMUC = 0x2000,    /*!< Mode setting                                       */
    TFA9912_BF_DIO4SEL= 0x2011,    /*!< DIO4 Input selection                               */
    TFA9912_BF_TDME  = 0x2040,    /*!< Enable TDM interface                               */
    TFA9912_BF_TDMMODE= 0x2050,    /*!< Slave/master                                       */
    TFA9912_BF_TDMCLINV= 0x2060,    /*!< Reception data to BCK clock                        */
    TFA9912_BF_TDMFSLN= 0x2073,    /*!< FS length                                          */
    TFA9912_BF_TDMFSPOL= 0x20b0,    /*!< FS polarity                                        */
    TFA9912_BF_TDMNBCK= 0x20c3,    /*!< N-BCK's in FS                                      */
    TFA9912_BF_TDMSLOTS= 0x2103,    /*!< N-slots in Frame                                   */
    TFA9912_BF_TDMSLLN= 0x2144,    /*!< N-bits in slot                                     */
    TFA9912_BF_TDMBRMG= 0x2194,    /*!< N-bits remaining                                   */
    TFA9912_BF_TDMDEL= 0x21e0,    /*!< data delay to FS                                   */
    TFA9912_BF_TDMADJ= 0x21f0,    /*!< data adjustment                                    */
    TFA9912_BF_TDMOOMP= 0x2201,    /*!< Received audio compression                         */
    TFA9912_BF_TDMSSIZE= 0x2224,    /*!< Sample size per slot                               */
    TFA9912_BF_TDMTXDFO= 0x2271,    /*!< Format unused bits in a slot                       */
    TFA9912_BF_TDMTXUS0= 0x2291,    /*!< Format unused slots GAINIO                         */
    TFA9912_BF_TDMTXUS1= 0x22b1,    /*!< Format unused slots DIO1                           */
    TFA9912_BF_TDMTXUS2= 0x22d1,    /*!< Format unused slots DIO2                           */
    TFA9912_BF_TDMGIE= 0x2300,    /*!< Control gain (channel in 0)                        */
    TFA9912_BF_TDMDCE= 0x2310,    /*!< Control audio  left (channel in 1 )                */
    TFA9912_BF_TDMSPKE= 0x2320,    /*!< Control audio right (channel in 2 )                */
    TFA9912_BF_TDMCSE= 0x2330,    /*!< Current sense                                      */
    TFA9912_BF_TDMVSE= 0x2340,    /*!< Voltage sense                                      */
    TFA9912_BF_TDMGOE= 0x2350,    /*!< DSP Gainout                                        */
    TFA9912_BF_TDMCF2E= 0x2360,    /*!< DSP 2                                              */
    TFA9912_BF_TDMCF3E= 0x2370,    /*!< DSP 3                                              */
    TFA9912_BF_TDMCFE= 0x2380,    /*!< DSP                                                */
    TFA9912_BF_TDMES6= 0x2390,    /*!< Loopback of Audio left (channel 1)                 */
    TFA9912_BF_TDMES7= 0x23a0,    /*!< Loopback of Audio right (channel 2)                */
    TFA9912_BF_TDMCF4E= 0x23b0,    /*!< AEC ref right control                              */
    TFA9912_BF_TDMPD1E= 0x23c0,    /*!< PDM 1 control                                      */
    TFA9912_BF_TDMPD2E= 0x23d0,    /*!< PDM 2 control                                      */
    TFA9912_BF_TDMGIN= 0x2401,    /*!< IO gainin                                          */
    TFA9912_BF_TDMLIO= 0x2421,    /*!< IO audio left                                      */
    TFA9912_BF_TDMRIO= 0x2441,    /*!< IO audio right                                     */
    TFA9912_BF_TDMCSIO= 0x2461,    /*!< IO Current Sense                                   */
    TFA9912_BF_TDMVSIO= 0x2481,    /*!< IO voltage sense                                   */
    TFA9912_BF_TDMGOIO= 0x24a1,    /*!< IO gain out                                        */
    TFA9912_BF_TDMCFIO2= 0x24c1,    /*!< IO DSP 2                                           */
    TFA9912_BF_TDMCFIO3= 0x24e1,    /*!< IO DSP 3                                           */
    TFA9912_BF_TDMCFIO= 0x2501,    /*!< IO DSP                                             */
    TFA9912_BF_TDMLPB6= 0x2521,    /*!< IO Source 6                                        */
    TFA9912_BF_TDMLPB7= 0x2541,    /*!< IO Source 7                                        */
    TFA9912_BF_TDMGS = 0x2603,    /*!< Control gainin                                     */
    TFA9912_BF_TDMDCS= 0x2643,    /*!< tdm slot for audio left (channel 1)                */
    TFA9912_BF_TDMSPKS= 0x2683,    /*!< tdm slot for audio right (channel 2)               */
    TFA9912_BF_TDMCSS= 0x26c3,    /*!< Slot Position of Current Sense Out                 */
    TFA9912_BF_TDMVSS= 0x2703,    /*!< Slot Position of Voltage sense                     */
    TFA9912_BF_TDMCGOS= 0x2743,    /*!< Slot Position of GAIN out                          */
    TFA9912_BF_TDMCF2S= 0x2783,    /*!< Slot Position DSPout2                              */
    TFA9912_BF_TDMCF3S= 0x27c3,    /*!< Slot Position DSPout3                              */
    TFA9912_BF_TDMCFS= 0x2803,    /*!< Slot Position of DSPout                            */
    TFA9912_BF_TDMEDAT6S= 0x2843,    /*!< Slot Position of loopback channel left             */
    TFA9912_BF_TDMEDAT7S= 0x2883,    /*!< Slot Position of loopback channel right            */
    TFA9912_BF_TDMTXUS3= 0x2901,    /*!< Format unused slots D3                             */
    TFA9912_BF_PDMSM = 0x3100,    /*!< PDM control                                        */
    TFA9912_BF_PDMSTSEL= 0x3110,    /*!< PDM Decimator input selection                      */
    TFA9912_BF_PDMSTENBL= 0x3120,    /*!< Side tone input enable                             */
    TFA9912_BF_PDMLSEL= 0x3130,    /*!< PDM data selection for left channel during PDM direct mode */
    TFA9912_BF_PDMRSEL= 0x3140,    /*!< PDM data selection for right channel during PDM direct mode */
    TFA9912_BF_MICVDDE= 0x3150,    /*!< Enable MICVDD                                      */
    TFA9912_BF_PDMCLRAT= 0x3201,    /*!< PDM BCK/Fs ratio                                   */
    TFA9912_BF_PDMGAIN= 0x3223,    /*!< PDM gain                                           */
    TFA9912_BF_PDMOSEL= 0x3263,    /*!< PDM output selection - RE/FE data combination      */
    TFA9912_BF_SELCFHAPD= 0x32a0,    /*!< Select the source for haptic data output (not for customer) */
    TFA9912_BF_ISTVDDS= 0x4000,    /*!< Status POR                                         */
    TFA9912_BF_ISTPLLS= 0x4010,    /*!< Status PLL lock                                    */
    TFA9912_BF_ISTOTDS= 0x4020,    /*!< Status OTP alarm                                   */
    TFA9912_BF_ISTOVDS= 0x4030,    /*!< Status OVP alarm                                   */
    TFA9912_BF_ISTUVDS= 0x4040,    /*!< Status UVP alarm                                   */
    TFA9912_BF_ISTCLKS= 0x4050,    /*!< Status clocks stable                               */
    TFA9912_BF_ISTMTPB= 0x4060,    /*!< Status MTP busy                                    */
    TFA9912_BF_ISTNOCLK= 0x4070,    /*!< Status lost clock                                  */
    TFA9912_BF_ISTSPKS= 0x4080,    /*!< Status speaker error                               */
    TFA9912_BF_ISTACS= 0x4090,    /*!< Status cold start                                  */
    TFA9912_BF_ISTSWS= 0x40a0,    /*!< Status amplifier engage                            */
    TFA9912_BF_ISTWDS= 0x40b0,    /*!< Status watchdog                                    */
    TFA9912_BF_ISTAMPS= 0x40c0,    /*!< Status amplifier enable                            */
    TFA9912_BF_ISTAREFS= 0x40d0,    /*!< Status Ref enable                                  */
    TFA9912_BF_ISTADCCR= 0x40e0,    /*!< Status Control ADC                                 */
    TFA9912_BF_ISTBODNOK= 0x40f0,    /*!< Status BOD                                         */
    TFA9912_BF_ISTBSTCU= 0x4100,    /*!< Status DCDC current limiting                       */
    TFA9912_BF_ISTBSTHI= 0x4110,    /*!< Status DCDC active                                 */
    TFA9912_BF_ISTBSTOC= 0x4120,    /*!< Status DCDC OCP                                    */
    TFA9912_BF_ISTBSTPKCUR= 0x4130,    /*!< Status bst peakcur                                 */
    TFA9912_BF_ISTBSTVC= 0x4140,    /*!< Status DCDC level 1x                               */
    TFA9912_BF_ISTBST86= 0x4150,    /*!< Status DCDC level 1.14x                            */
    TFA9912_BF_ISTBST93= 0x4160,    /*!< Status DCDC level 1.07x                            */
    TFA9912_BF_ISTRCVLD= 0x4170,    /*!< Status rcvldop ready                               */
    TFA9912_BF_ISTOCPL= 0x4180,    /*!< Status ocp alarm left                              */
    TFA9912_BF_ISTOCPR= 0x4190,    /*!< Status ocp alarm                                   */
    TFA9912_BF_ISTMWSRC= 0x41a0,    /*!< Status Waits HW I2C settings                       */
    TFA9912_BF_ISTMWCFC= 0x41b0,    /*!< Status waits CF config                             */
    TFA9912_BF_ISTMWSMU= 0x41c0,    /*!< Status Audio mute sequence                         */
    TFA9912_BF_ISTCFMER= 0x41d0,    /*!< Status cfma error                                  */
    TFA9912_BF_ISTCFMAC= 0x41e0,    /*!< Status cfma ack                                    */
    TFA9912_BF_ISTCLKOOR= 0x41f0,    /*!< Status flag_clk_out_of_range                       */
    TFA9912_BF_ISTTDMER= 0x4200,    /*!< Status tdm error                                   */
    TFA9912_BF_ISTCLPL= 0x4210,    /*!< Status clip left                                   */
    TFA9912_BF_ISTCLPR= 0x4220,    /*!< Status clip                                        */
    TFA9912_BF_ISTOCPM= 0x4230,    /*!< Status mic ocpok                                   */
    TFA9912_BF_ISTLP1= 0x4250,    /*!< Status low power mode1                             */
    TFA9912_BF_ISTLA = 0x4260,    /*!< Status low amplitude detection                     */
    TFA9912_BF_ISTVDDP= 0x4270,    /*!< Status VDDP greater than VBAT                      */
    TFA9912_BF_ISTTAPDET= 0x4280,    /*!< Status Tap  detected                               */
    TFA9912_BF_ISTAUDMOD= 0x4290,    /*!< Status Audio Mode activated                        */
    TFA9912_BF_ISTSAMMOD= 0x42a0,    /*!< Status SAM Mode activated                          */
    TFA9912_BF_ISTTAPMOD= 0x42b0,    /*!< Status Tap  Mode Activated                         */
    TFA9912_BF_ISTTAPTRG= 0x42c0,    /*!< Status Tap comparator triggered                    */
    TFA9912_BF_ICLVDDS= 0x4400,    /*!< Clear POR                                          */
    TFA9912_BF_ICLPLLS= 0x4410,    /*!< Clear PLL lock                                     */
    TFA9912_BF_ICLOTDS= 0x4420,    /*!< Clear OTP alarm                                    */
    TFA9912_BF_ICLOVDS= 0x4430,    /*!< Clear OVP alarm                                    */
    TFA9912_BF_ICLUVDS= 0x4440,    /*!< Clear UVP alarm                                    */
    TFA9912_BF_ICLCLKS= 0x4450,    /*!< Clear clocks stable                                */
    TFA9912_BF_ICLMTPB= 0x4460,    /*!< Clear mtp busy                                     */
    TFA9912_BF_ICLNOCLK= 0x4470,    /*!< Clear lost clk                                     */
    TFA9912_BF_ICLSPKS= 0x4480,    /*!< Clear speaker error                                */
    TFA9912_BF_ICLACS= 0x4490,    /*!< Clear cold started                                 */
    TFA9912_BF_ICLSWS= 0x44a0,    /*!< Clear amplifier engage                             */
    TFA9912_BF_ICLWDS= 0x44b0,    /*!< Clear watchdog                                     */
    TFA9912_BF_ICLAMPS= 0x44c0,    /*!< Clear enbl amp                                     */
    TFA9912_BF_ICLAREFS= 0x44d0,    /*!< Clear ref enable                                   */
    TFA9912_BF_ICLADCCR= 0x44e0,    /*!< Clear control ADC                                  */
    TFA9912_BF_ICLBODNOK= 0x44f0,    /*!< Clear BOD                                          */
    TFA9912_BF_ICLBSTCU= 0x4500,    /*!< Clear DCDC current limiting                        */
    TFA9912_BF_ICLBSTHI= 0x4510,    /*!< Clear DCDC active                                  */
    TFA9912_BF_ICLBSTOC= 0x4520,    /*!< Clear DCDC OCP                                     */
    TFA9912_BF_ICLBSTPC= 0x4530,    /*!< Clear bst peakcur                                  */
    TFA9912_BF_ICLBSTVC= 0x4540,    /*!< Clear DCDC level 1x                                */
    TFA9912_BF_ICLBST86= 0x4550,    /*!< Clear DCDC level 1.14x                             */
    TFA9912_BF_ICLBST93= 0x4560,    /*!< Clear DCDC level 1.07x                             */
    TFA9912_BF_ICLRCVLD= 0x4570,    /*!< Clear rcvldop ready                                */
    TFA9912_BF_ICLOCPL= 0x4580,    /*!< Clear ocp alarm left                               */
    TFA9912_BF_ICLOCPR= 0x4590,    /*!< Clear ocp alarm                                    */
    TFA9912_BF_ICLMWSRC= 0x45a0,    /*!< Clear wait HW I2C settings                         */
    TFA9912_BF_ICLMWCFC= 0x45b0,    /*!< Clear wait cf config                               */
    TFA9912_BF_ICLMWSMU= 0x45c0,    /*!< Clear audio mute sequence                          */
    TFA9912_BF_ICLCFMER= 0x45d0,    /*!< Clear cfma err                                     */
    TFA9912_BF_ICLCFMAC= 0x45e0,    /*!< Clear cfma ack                                     */
    TFA9912_BF_ICLCLKOOR= 0x45f0,    /*!< Clear flag_clk_out_of_range                        */
    TFA9912_BF_ICLTDMER= 0x4600,    /*!< Clear tdm error                                    */
    TFA9912_BF_ICLCLPL= 0x4610,    /*!< Clear clip left                                    */
    TFA9912_BF_ICLCLP= 0x4620,    /*!< Clear clip                                         */
    TFA9912_BF_ICLOCPM= 0x4630,    /*!< Clear mic ocpok                                    */
    TFA9912_BF_ICLLP1= 0x4650,    /*!< Clear low power mode1                              */
    TFA9912_BF_ICLLA = 0x4660,    /*!< Clear low amplitude detection                      */
    TFA9912_BF_ICLVDDP= 0x4670,    /*!< Clear VDDP greater then VBAT                       */
    TFA9912_BF_ICLTAPDET= 0x4680,    /*!< Clear Tap  detected                                */
    TFA9912_BF_ICLAUDMOD= 0x4690,    /*!< Clear Audio Mode activated                         */
    TFA9912_BF_ICLSAMMOD= 0x46a0,    /*!< Clear SAM Mode activated                           */
    TFA9912_BF_ICLTAPMOD= 0x46b0,    /*!< Clear Tap  Mode Activated                          */
    TFA9912_BF_ICLTAPTRG= 0x46c0,    /*!< Clear Comparator Interrupt                         */
    TFA9912_BF_IEVDDS= 0x4800,    /*!< Enable por                                         */
    TFA9912_BF_IEPLLS= 0x4810,    /*!< Enable pll lock                                    */
    TFA9912_BF_IEOTDS= 0x4820,    /*!< Enable OTP alarm                                   */
    TFA9912_BF_IEOVDS= 0x4830,    /*!< Enable OVP alarm                                   */
    TFA9912_BF_IEUVDS= 0x4840,    /*!< Enable UVP alarm                                   */
    TFA9912_BF_IECLKS= 0x4850,    /*!< Enable clocks stable                               */
    TFA9912_BF_IEMTPB= 0x4860,    /*!< Enable mtp busy                                    */
    TFA9912_BF_IENOCLK= 0x4870,    /*!< Enable lost clk                                    */
    TFA9912_BF_IESPKS= 0x4880,    /*!< Enable speaker error                               */
    TFA9912_BF_IEACS = 0x4890,    /*!< Enable cold started                                */
    TFA9912_BF_IESWS = 0x48a0,    /*!< Enable amplifier engage                            */
    TFA9912_BF_IEWDS = 0x48b0,    /*!< Enable watchdog                                    */
    TFA9912_BF_IEAMPS= 0x48c0,    /*!< Enable enbl amp                                    */
    TFA9912_BF_IEAREFS= 0x48d0,    /*!< Enable ref enable                                  */
    TFA9912_BF_IEADCCR= 0x48e0,    /*!< Enable Control ADC                                 */
    TFA9912_BF_IEBODNOK= 0x48f0,    /*!< Enable BOD                                         */
    TFA9912_BF_IEBSTCU= 0x4900,    /*!< Enable DCDC current limiting                       */
    TFA9912_BF_IEBSTHI= 0x4910,    /*!< Enable DCDC active                                 */
    TFA9912_BF_IEBSTOC= 0x4920,    /*!< Enable DCDC OCP                                    */
    TFA9912_BF_IEBSTPC= 0x4930,    /*!< Enable bst peakcur                                 */
    TFA9912_BF_IEBSTVC= 0x4940,    /*!< Enable DCDC level 1x                               */
    TFA9912_BF_IEBST86= 0x4950,    /*!< Enable DCDC level 1.14x                            */
    TFA9912_BF_IEBST93= 0x4960,    /*!< Enable DCDC level 1.07x                            */
    TFA9912_BF_IERCVLD= 0x4970,    /*!< Enable rcvldop ready                               */
    TFA9912_BF_IEOCPL= 0x4980,    /*!< Enable ocp alarm left                              */
    TFA9912_BF_IEOCPR= 0x4990,    /*!< Enable ocp alarm                                   */
    TFA9912_BF_IEMWSRC= 0x49a0,    /*!< Enable waits HW I2C settings                       */
    TFA9912_BF_IEMWCFC= 0x49b0,    /*!< Enable man wait cf config                          */
    TFA9912_BF_IEMWSMU= 0x49c0,    /*!< Enable man Audio mute sequence                     */
    TFA9912_BF_IECFMER= 0x49d0,    /*!< Enable cfma err                                    */
    TFA9912_BF_IECFMAC= 0x49e0,    /*!< Enable cfma ack                                    */
    TFA9912_BF_IECLKOOR= 0x49f0,    /*!< Enable flag_clk_out_of_range                       */
    TFA9912_BF_IETDMER= 0x4a00,    /*!< Enable tdm error                                   */
    TFA9912_BF_IECLPL= 0x4a10,    /*!< Enable clip left                                   */
    TFA9912_BF_IECLPR= 0x4a20,    /*!< Enable clip                                        */
    TFA9912_BF_IEOCPM1= 0x4a30,    /*!< Enable mic ocpok                                   */
    TFA9912_BF_IELP1 = 0x4a50,    /*!< Enable low power mode1                             */
    TFA9912_BF_IELA  = 0x4a60,    /*!< Enable low amplitude detection                     */
    TFA9912_BF_IEVDDP= 0x4a70,    /*!< Enable VDDP greater than VBAT                      */
    TFA9912_BF_IETAPDET= 0x4a80,    /*!< Enable Tap  detected                               */
    TFA9912_BF_IEAUDMOD= 0x4a90,    /*!< Enable Audio Mode activated                        */
    TFA9912_BF_IESAMMOD= 0x4aa0,    /*!< Enable SAM Mode activated                          */
    TFA9912_BF_IETAPMOD= 0x4ab0,    /*!< Enable Tap  Mode Activated                         */
    TFA9912_BF_IETAPTRG= 0x4ac0,    /*!< Enable comparator interrupt                        */
    TFA9912_BF_IPOVDDS= 0x4c00,    /*!< Polarity por                                       */
    TFA9912_BF_IPOPLLS= 0x4c10,    /*!< Polarity pll lock                                  */
    TFA9912_BF_IPOOTDS= 0x4c20,    /*!< Polarity OTP alarm                                 */
    TFA9912_BF_IPOOVDS= 0x4c30,    /*!< Polarity OVP alarm                                 */
    TFA9912_BF_IPOUVDS= 0x4c40,    /*!< Polarity UVP alarm                                 */
    TFA9912_BF_IPOCLKS= 0x4c50,    /*!< Polarity clocks stable                             */
    TFA9912_BF_IPOMTPB= 0x4c60,    /*!< Polarity mtp busy                                  */
    TFA9912_BF_IPONOCLK= 0x4c70,    /*!< Polarity lost clk                                  */
    TFA9912_BF_IPOSPKS= 0x4c80,    /*!< Polarity speaker error                             */
    TFA9912_BF_IPOACS= 0x4c90,    /*!< Polarity cold started                              */
    TFA9912_BF_IPOSWS= 0x4ca0,    /*!< Polarity amplifier engage                          */
    TFA9912_BF_IPOWDS= 0x4cb0,    /*!< Polarity watchdog                                  */
    TFA9912_BF_IPOAMPS= 0x4cc0,    /*!< Polarity enbl amp                                  */
    TFA9912_BF_IPOAREFS= 0x4cd0,    /*!< Polarity ref enable                                */
    TFA9912_BF_IPOADCCR= 0x4ce0,    /*!< Polarity Control ADC                               */
    TFA9912_BF_IPOBODNOK= 0x4cf0,    /*!< Polarity BOD                                       */
    TFA9912_BF_IPOBSTCU= 0x4d00,    /*!< Polarity DCDC current limiting                     */
    TFA9912_BF_IPOBSTHI= 0x4d10,    /*!< Polarity DCDC active                               */
    TFA9912_BF_IPOBSTOC= 0x4d20,    /*!< Polarity DCDC OCP                                  */
    TFA9912_BF_IPOBSTPC= 0x4d30,    /*!< Polarity bst peakcur                               */
    TFA9912_BF_IPOBSTVC= 0x4d40,    /*!< Polarity DCDC level 1x                             */
    TFA9912_BF_IPOBST86= 0x4d50,    /*!< Polarity DCDC level 1.14x                          */
    TFA9912_BF_IPOBST93= 0x4d60,    /*!< Polarity DCDC level 1.07x                          */
    TFA9912_BF_IPORCVLD= 0x4d70,    /*!< Polarity rcvldop ready                             */
    TFA9912_BF_IPOOCPL= 0x4d80,    /*!< Polarity ocp alarm left                            */
    TFA9912_BF_IPOOCPR= 0x4d90,    /*!< Polarity ocp alarm                                 */
    TFA9912_BF_IPOMWSRC= 0x4da0,    /*!< Polarity waits HW I2C settings                     */
    TFA9912_BF_IPOMWCFC= 0x4db0,    /*!< Polarity man wait cf config                        */
    TFA9912_BF_IPOMWSMU= 0x4dc0,    /*!< Polarity man audio mute sequence                   */
    TFA9912_BF_IPOCFMER= 0x4dd0,    /*!< Polarity cfma err                                  */
    TFA9912_BF_IPOCFMAC= 0x4de0,    /*!< Polarity cfma ack                                  */
    TFA9912_BF_IPOCLKOOR= 0x4df0,    /*!< Polarity flag_clk_out_of_range                     */
    TFA9912_BF_IPOTDMER= 0x4e00,    /*!< Polarity tdm error                                 */
    TFA9912_BF_IPOCLPL= 0x4e10,    /*!< Polarity clip left                                 */
    TFA9912_BF_IPOCLPR= 0x4e20,    /*!< Polarity clip                                      */
    TFA9912_BF_IPOOCPM= 0x4e30,    /*!< Polarity mic ocpok                                 */
    TFA9912_BF_IPOLP1= 0x4e50,    /*!< Polarity low power mode1                           */
    TFA9912_BF_IPOLA = 0x4e60,    /*!< Polarity low amplitude detection                   */
    TFA9912_BF_IPOVDDP= 0x4e70,    /*!< Polarity VDDP greater than VBAT                    */
    TFA9912_BF_IPOLTAPDET= 0x4e80,    /*!< PolarityTap  detected                              */
    TFA9912_BF_IPOLAUDMOD= 0x4e90,    /*!< PolarityAudio Mode activated                       */
    TFA9912_BF_IPOLSAMMOD= 0x4ea0,    /*!< PolaritySAM Mode activated                         */
    TFA9912_BF_IPOLTAPMOD= 0x4eb0,    /*!< Polarity Tap  Mode Activated                       */
    TFA9912_BF_IPOLTAPTRG= 0x4ec0,    /*!< PolarityTap  Comparator Trigger                    */
    TFA9912_BF_BSSCR = 0x5001,    /*!< Battery Safeguard attack time                      */
    TFA9912_BF_BSST  = 0x5023,    /*!< Battery Safeguard threshold voltage level          */
    TFA9912_BF_BSSRL = 0x5061,    /*!< Battery Safeguard maximum reduction                */
    TFA9912_BF_BSSRR = 0x5082,    /*!< Battery Safeguard release time                     */
    TFA9912_BF_BSSHY = 0x50b1,    /*!< Battery Safeguard hysteresis                       */
    TFA9912_BF_BSSAC = 0x50d0,    /*!< Reset clipper - Auto clear                         */
    TFA9912_BF_BSSR  = 0x50e0,    /*!< Battery voltage read out                           */
    TFA9912_BF_BSSBY = 0x50f0,    /*!< Bypass HW clipper                                  */
    TFA9912_BF_BSSS  = 0x5100,    /*!< Vbat prot steepness                                */
    TFA9912_BF_INTSMUTE= 0x5110,    /*!< Soft mute HW                                       */
    TFA9912_BF_CFSML = 0x5120,    /*!< Soft mute FW left                                  */
    TFA9912_BF_CFSM  = 0x5130,    /*!< Soft mute FW                                       */
    TFA9912_BF_HPFBYPL= 0x5140,    /*!< Bypass HPF left                                    */
    TFA9912_BF_HPFBYP= 0x5150,    /*!< Bypass HPF                                         */
    TFA9912_BF_DPSAL = 0x5160,    /*!< Enable DPSA left                                   */
    TFA9912_BF_DPSA  = 0x5170,    /*!< Enable DPSA                                        */
    TFA9912_BF_VOL   = 0x5187,    /*!< FW volume control for primary audio channel        */
    TFA9912_BF_HNDSFRCV= 0x5200,    /*!< Selection receiver                                 */
    TFA9912_BF_CLIPCTRL= 0x5222,    /*!< Clip control setting                               */
    TFA9912_BF_AMPGAIN= 0x5257,    /*!< Amplifier gain                                     */
    TFA9912_BF_SLOPEE= 0x52d0,    /*!< Enables slope control                              */
    TFA9912_BF_SLOPESET= 0x52e0,    /*!< Slope speed setting (bin. coded)                   */
    TFA9912_BF_CFTAPPAT= 0x5c07,    /*!< Coolflux tap pattern                               */
    TFA9912_BF_TAPDBGINFO= 0x5c83,    /*!< Reserved                                           */
    TFA9912_BF_TATPSTAT1= 0x5d0f,    /*!< Tap Status 1 from CF FW                            */
    TFA9912_BF_TCOMPTHR= 0x5f03,    /*!< Comparator threshold (in uV)                       */
    TFA9912_BF_PGAGAIN= 0x6081,    /*!< PGA gain selection                                 */
    TFA9912_BF_TDMSPKG= 0x6123,    /*!< System gain (INPLEV 0)                             */
    TFA9912_BF_LPM1LVL= 0x6505,    /*!< low power mode1 detector   ctrl threshold for low_audio_lvl  */
    TFA9912_BF_LPM1HLD= 0x6565,    /*!< Low power mode1 detector, ctrl hold time before low audio is reckoned to be low audio */
    TFA9912_BF_LPM1DIS= 0x65c0,    /*!< low power mode1 detector control                   */
    TFA9912_BF_DCDIS = 0x6630,    /*!< DCDC                                               */
    TFA9912_BF_TDMSRCMAP= 0x6801,    /*!< tdm source mapping                                 */
    TFA9912_BF_TDMSRCAS= 0x6821,    /*!< frame a selection                                  */
    TFA9912_BF_TDMSRCBS= 0x6841,    /*!< frame b selection                                  */
    TFA9912_BF_ANC1C = 0x68a0,    /*!< ANC one s complement                               */
    TFA9912_BF_SAMMODE= 0x6901,    /*!< Sam mode                                           */
    TFA9912_BF_DCMCC = 0x7033,    /*!< Max coil current                                   */
    TFA9912_BF_DCCV  = 0x7071,    /*!< Slope compensation current, represents LxF (inductance x frequency) value  */
    TFA9912_BF_DCIE  = 0x7090,    /*!< Adaptive boost mode                                */
    TFA9912_BF_DCSR  = 0x70a0,    /*!< Soft ramp up/down                                  */
    TFA9912_BF_DCINSEL= 0x70c1,    /*!< DCDC IIR input Selection                           */
    TFA9912_BF_DCPWM = 0x70f0,    /*!< DCDC PWM only mode                                 */
    TFA9912_BF_DCTRIP= 0x7504,    /*!< Adaptive boost trip levels 1, effective only when boost_intelligent is set to 1 */
    TFA9912_BF_DCTRIP2= 0x7554,    /*!< Adaptive boost trip level 2, effective only when boost_intelligent is set to 1 */
    TFA9912_BF_DCTRIPT= 0x75a4,    /*!< Adaptive boost trip levels, effective only when boost_intelligent is set to 1 */
    TFA9912_BF_DCVOF = 0x7635,    /*!< First boost voltage level                          */
    TFA9912_BF_DCVOS = 0x7695,    /*!< Second boost voltage level                         */
    TFA9912_BF_RST   = 0x9000,    /*!< Reset                                              */
    TFA9912_BF_DMEM  = 0x9011,    /*!< Target memory                                      */
    TFA9912_BF_AIF   = 0x9030,    /*!< Auto increment                                     */
    TFA9912_BF_CFINT = 0x9040,    /*!< Interrupt - auto clear                             */
    TFA9912_BF_CFCGATE= 0x9050,    /*!< Coolflux clock gating disabling control            */
    TFA9912_BF_REQCMD= 0x9080,    /*!< Firmware event request rpc command                 */
    TFA9912_BF_REQRST= 0x9090,    /*!< Firmware event request reset restart               */
    TFA9912_BF_REQMIPS= 0x90a0,    /*!< Firmware event request short on mips               */
    TFA9912_BF_REQMUTED= 0x90b0,    /*!< Firmware event request mute sequence ready         */
    TFA9912_BF_REQVOL= 0x90c0,    /*!< Firmware event request volume ready                */
    TFA9912_BF_REQDMG= 0x90d0,    /*!< Firmware event request speaker damage detected     */
    TFA9912_BF_REQCAL= 0x90e0,    /*!< Firmware event request calibration completed       */
    TFA9912_BF_REQRSV= 0x90f0,    /*!< Firmware event request reserved                    */
    TFA9912_BF_MADD  = 0x910f,    /*!< Memory address                                     */
    TFA9912_BF_MEMA  = 0x920f,    /*!< Activate memory access                             */
    TFA9912_BF_ERR   = 0x9307,    /*!< Error flags                                        */
    TFA9912_BF_ACKCMD= 0x9380,    /*!< Firmware event acknowledge rpc command             */
    TFA9912_BF_ACKRST= 0x9390,    /*!< Firmware event acknowledge reset restart           */
    TFA9912_BF_ACKMIPS= 0x93a0,    /*!< Firmware event acknowledge short on mips           */
    TFA9912_BF_ACKMUTED= 0x93b0,    /*!< Firmware event acknowledge mute sequence ready     */
    TFA9912_BF_ACKVOL= 0x93c0,    /*!< Firmware event acknowledge volume ready            */
    TFA9912_BF_ACKDMG= 0x93d0,    /*!< Firmware event acknowledge speaker damage detected */
    TFA9912_BF_ACKCAL= 0x93e0,    /*!< Firmware event acknowledge calibration completed   */
    TFA9912_BF_ACKRSV= 0x93f0,    /*!< Firmware event acknowledge reserved                */
    TFA9912_BF_MTPK  = 0xa107,    /*!< MTP KEY2 register                                  */
    TFA9912_BF_KEY1LOCKED= 0xa200,    /*!< Indicates KEY1 is locked                           */
    TFA9912_BF_KEY2LOCKED= 0xa210,    /*!< Indicates KEY2 is locked                           */
    TFA9912_BF_CIMTP = 0xa360,    /*!< Start copying data from I2C mtp registers to mtp   */
    TFA9912_BF_MTPRDMSB= 0xa50f,    /*!< MSB word of MTP manual read data                   */
    TFA9912_BF_MTPRDLSB= 0xa60f,    /*!< LSB word of MTP manual read data                   */
    TFA9912_BF_EXTTS = 0xb108,    /*!< External temperature (C)                           */
    TFA9912_BF_TROS  = 0xb190,    /*!< Select temp Speaker calibration                    */
    TFA9912_BF_SWPROFIL= 0xee0f,    /*!< Software profile data                              */
    TFA9912_BF_SWVSTEP= 0xef0f,    /*!< Software vstep information                         */
    TFA9912_BF_MTPOTC= 0xf000,    /*!< Calibration schedule                               */
    TFA9912_BF_MTPEX = 0xf010,    /*!< Calibration Ron executed                           */
    TFA9912_BF_DCMCCAPI= 0xf020,    /*!< Calibration current limit DCDC                     */
    TFA9912_BF_DCMCCSB= 0xf030,    /*!< Sign bit for delta calibration current limit DCDC  */
    TFA9912_BF_DCMCCCL= 0xf042,    /*!< Calibration delta current limit DCDC               */
    TFA9912_BF_USERDEF= 0xf078,    /*!< Reserved space for allowing customer to store speaker information */
    TFA9912_BF_R25C  = 0xf40f,    /*!< Ron resistance of  speaker coil                    */
} Tfa9912BfEnumList_t;
#define TFA9912_NAMETABLE static tfaBfName_t Tfa9912DatasheetNames[]= {\
   { 0x0, "PWDN"},    /* Powerdown selection                               , */\
   { 0x10, "I2CR"},    /* I2C Reset - Auto clear                            , */\
   { 0x20, "CFE"},    /* Enable CoolFlux                                   , */\
   { 0x30, "AMPE"},    /* Enables the Amplifier                             , */\
   { 0x40, "DCA"},    /* Activate DC-to-DC converter                       , */\
   { 0x50, "SBSL"},    /* Coolflux configured                               , */\
   { 0x60, "AMPC"},    /* CoolFlux controls amplifier                       , */\
   { 0x71, "INTP"},    /* Interrupt config                                  , */\
   { 0x90, "FSSSEL"},    /* Audio sample reference                            , */\
   { 0xb0, "BYPOCP"},    /* Bypass OCP                                        , */\
   { 0xc0, "TSTOCP"},    /* OCP testing control                               , */\
   { 0x101, "AMPINSEL"},    /* Amplifier input selection                         , */\
   { 0x120, "MANSCONF"},    /* I2C configured                                    , */\
   { 0x130, "MANCOLD"},    /* Execute cold start                                , */\
   { 0x140, "MANAOOSC"},    /* Internal osc off at PWDN                          , */\
   { 0x150, "MANROBOD"},    /* Reaction on BOD                                   , */\
   { 0x160, "BODE"},    /* BOD Enable                                        , */\
   { 0x170, "BODHYS"},    /* BOD Hysteresis                                    , */\
   { 0x181, "BODFILT"},    /* BOD filter                                        , */\
   { 0x1a1, "BODTHLVL"},    /* BOD threshold                                     , */\
   { 0x1d0, "MUTETO"},    /* Time out SB mute sequence                         , */\
   { 0x1e0, "RCVNS"},    /* Noise shaper selection                            , */\
   { 0x1f0, "MANWDE"},    /* Watchdog enable                                   , */\
   { 0x203, "AUDFS"},    /* Sample rate (fs)                                  , */\
   { 0x240, "INPLEV"},    /* TDM output attenuation                            , */\
   { 0x255, "FRACTDEL"},    /* V/I Fractional delay                              , */\
   { 0x2b0, "BYPHVBF"},    /* Bypass HVBAT filter                               , */\
   { 0x2c0, "TDMC"},    /* TDM Compatibility with TFA9872                    , */\
   { 0x2e0, "ENBLADC10"},    /* ADC10 Enable -  I2C direct mode                   , */\
   { 0x30f, "REV"},    /* Revision info                                     , */\
   { 0x401, "REFCKEXT"},    /* PLL external ref clock                            , */\
   { 0x420, "REFCKSEL"},    /* PLL internal ref clock                            , */\
   { 0x430, "ENCFCKSEL"},    /* Coolflux DSP clock scaling, low power mode        , */\
   { 0x441, "CFCKSEL"},    /* Coolflux DSP clock scaler selection for low power mode, */\
   { 0x460, "TDMINFSEL"},    /* TDM clock selection                               , */\
   { 0x470, "DISBLAUTOCLKSEL"},    /* Disable Automatic dsp clock source selection      , */\
   { 0x480, "SELCLKSRC"},    /* I2C selection of DSP clock when auto select is disabled, */\
   { 0x490, "SELTIMSRC"},    /* I2C selection of Watchdog and Timer clock         , */\
   { 0x500, "SSLEFTE"},    /*                                                   , */\
   { 0x510, "SPKSSEN"},    /* Enable speaker path                               , */\
   { 0x520, "VSLEFTE"},    /*                                                   , */\
   { 0x530, "VSRIGHTE"},    /* Voltage sense                                     , */\
   { 0x540, "CSLEFTE"},    /*                                                   , */\
   { 0x550, "CSRIGHTE"},    /* Current sense                                     , */\
   { 0x560, "SSPDME"},    /* Sub-system PDM                                    , */\
   { 0x570, "PGALE"},    /* Enable PGA chop clock for left channel            , */\
   { 0x580, "PGARE"},    /* Enable PGA chop clock                             , */\
   { 0x590, "SSTDME"},    /* Sub-system TDM                                    , */\
   { 0x5a0, "SSPBSTE"},    /* Sub-system boost                                  , */\
   { 0x5b0, "SSADCE"},    /* Sub-system ADC                                    , */\
   { 0x5c0, "SSFAIME"},    /* Sub-system FAIM                                   , */\
   { 0x5d0, "SSCFTIME"},    /* CF Sub-system timer                               , */\
   { 0x5e0, "SSCFWDTE"},    /* CF Sub-system WDT                                 , */\
   { 0x5f0, "FAIMVBGOVRRL"},    /* Over rule of vbg for FaIM access                  , */\
   { 0x600, "SAMSPKSEL"},    /* Input selection for TAP/SAM                       , */\
   { 0x610, "PDM2IISEN"},    /* PDM2IIS Bridge enable                             , */\
   { 0x620, "TAPRSTBYPASS"},    /* Tap decimator reset bypass - Bypass the decimator reset from tapdec, */\
   { 0x631, "CARDECISEL0"},    /* Cardec input 0 sel                                , */\
   { 0x651, "CARDECISEL1"},    /* Cardec input sel                                  , */\
   { 0x670, "TAPDECSEL"},    /* Select TAP/Cardec for TAP                         , */\
   { 0x680, "COMPCOUNT"},    /* Comparator o/p filter selection                   , */\
   { 0x691, "STARTUPMODE"},    /* Startup Mode Selection                            , */\
   { 0x6b0, "AUTOTAP"},    /* Enable auto tap switching                         , */\
   { 0x6c1, "COMPINITIME"},    /* Comparator initialization time to be used in Tap Machine, */\
   { 0x6e1, "ANAPINITIME"},    /* Analog initialization time to be used in Tap Machine, */\
   { 0x707, "CCHKTH"},    /* Clock check Higher Threshold                      , */\
   { 0x787, "CCHKTL"},    /* Clock check Higher Threshold                      , */\
   { 0x802, "AMPOCRT"},    /* Amplifier on-off criteria for shutdown            , */\
   { 0x832, "AMPTCRR"},    /* Amplifier on-off criteria for tap mode entry      , */\
   { 0xd00, "STGS"},    /* PDM side tone gain selector                       , */\
   { 0xd18, "STGAIN"},    /* Side tone gain                                    , */\
   { 0xda0, "STSMUTE"},    /* Side tone soft mute                               , */\
   { 0xdb0, "ST1C"},    /* side tone one s complement                        , */\
   { 0xe80, "CMFBEL"},    /* CMFB enable left                                  , */\
   { 0x1000, "VDDS"},    /* POR                                               , */\
   { 0x1010, "PLLS"},    /* PLL lock                                          , */\
   { 0x1020, "OTDS"},    /* OTP alarm                                         , */\
   { 0x1030, "OVDS"},    /* OVP alarm                                         , */\
   { 0x1040, "UVDS"},    /* UVP alarm                                         , */\
   { 0x1050, "CLKS"},    /* Clocks stable                                     , */\
   { 0x1060, "MTPB"},    /* MTP busy                                          , */\
   { 0x1070, "NOCLK"},    /* Lost clock                                        , */\
   { 0x1090, "ACS"},    /* Cold Start                                        , */\
   { 0x10a0, "SWS"},    /* Amplifier engage                                  , */\
   { 0x10b0, "WDS"},    /* Watchdog                                          , */\
   { 0x10c0, "AMPS"},    /* Amplifier enable                                  , */\
   { 0x10d0, "AREFS"},    /* References enable                                 , */\
   { 0x10e0, "ADCCR"},    /* Control ADC                                       , */\
   { 0x10f0, "BODNOK"},    /* BOD                                               , */\
   { 0x1100, "DCIL"},    /* DCDC current limiting                             , */\
   { 0x1110, "DCDCA"},    /* DCDC active                                       , */\
   { 0x1120, "DCOCPOK"},    /* DCDC OCP nmos                                     , */\
   { 0x1130, "DCPEAKCUR"},    /* Indicates current is max in DC-to-DC converter    , */\
   { 0x1140, "DCHVBAT"},    /* DCDC level 1x                                     , */\
   { 0x1150, "DCH114"},    /* DCDC level 1.14x                                  , */\
   { 0x1160, "DCH107"},    /* DCDC level 1.07x                                  , */\
   { 0x1170, "STMUTEB"},    /* side tone (un)mute busy                           , */\
   { 0x1180, "STMUTE"},    /* side tone mute state                              , */\
   { 0x1190, "TDMLUTER"},    /* TDM LUT error                                     , */\
   { 0x11a2, "TDMSTAT"},    /* TDM status bits                                   , */\
   { 0x11d0, "TDMERR"},    /* TDM error                                         , */\
   { 0x11e0, "HAPTIC"},    /* Status haptic driver                              , */\
   { 0x1300, "OCPOAP"},    /* OCPOK pmos A                                      , */\
   { 0x1310, "OCPOAN"},    /* OCPOK nmos A                                      , */\
   { 0x1320, "OCPOBP"},    /* OCPOK pmos B                                      , */\
   { 0x1330, "OCPOBN"},    /* OCPOK nmos B                                      , */\
   { 0x1340, "CLIPAH"},    /* Clipping A to Vddp                                , */\
   { 0x1350, "CLIPAL"},    /* Clipping A to gnd                                 , */\
   { 0x1360, "CLIPBH"},    /* Clipping B to Vddp                                , */\
   { 0x1370, "CLIPBL"},    /* Clipping B to gnd                                 , */\
   { 0x1380, "OCDS"},    /* OCP  amplifier                                    , */\
   { 0x1390, "CLIPS"},    /* Amplifier  clipping                               , */\
   { 0x13a0, "TCMPTRG"},    /* Status Tap comparator triggered                   , */\
   { 0x13b0, "TAPDET"},    /* Status Tap  detected                              , */\
   { 0x13c0, "MANWAIT1"},    /* Wait HW I2C settings                              , */\
   { 0x13d0, "MANWAIT2"},    /* Wait CF config                                    , */\
   { 0x13e0, "MANMUTE"},    /* Audio mute sequence                               , */\
   { 0x13f0, "MANOPER"},    /* Operating state                                   , */\
   { 0x1400, "SPKSL"},    /* Left speaker status                               , */\
   { 0x1410, "SPKS"},    /* Speaker status                                    , */\
   { 0x1420, "CLKOOR"},    /* External clock status                             , */\
   { 0x1433, "MANSTATE"},    /* Device manager status                             , */\
   { 0x1471, "DCMODE"},    /* DCDC mode status bits                             , */\
   { 0x1490, "DSPCLKSRC"},    /* DSP clock source selected by manager              , */\
   { 0x14a1, "STARTUPMODSTAT"},    /* Startup Mode Selected by Manager(Read Only)       , */\
   { 0x14c3, "TSPMSTATE"},    /* Tap Machine State                                 , */\
   { 0x1509, "BATS"},    /* Battery voltage (V)                               , */\
   { 0x1608, "TEMPS"},    /* IC Temperature (C)                                , */\
   { 0x1709, "VDDPS"},    /* IC VDDP voltage ( 1023*VDDP/13 V)                 , */\
   { 0x17a0, "DCILCF"},    /* DCDC current limiting for DSP                     , */\
   { 0x2000, "TDMUC"},    /* Mode setting                                      , */\
   { 0x2011, "DIO4SEL"},    /* DIO4 Input selection                              , */\
   { 0x2040, "TDME"},    /* Enable TDM interface                              , */\
   { 0x2050, "TDMMODE"},    /* Slave/master                                      , */\
   { 0x2060, "TDMCLINV"},    /* Reception data to BCK clock                       , */\
   { 0x2073, "TDMFSLN"},    /* FS length                                         , */\
   { 0x20b0, "TDMFSPOL"},    /* FS polarity                                       , */\
   { 0x20c3, "TDMNBCK"},    /* N-BCK's in FS                                     , */\
   { 0x2103, "TDMSLOTS"},    /* N-slots in Frame                                  , */\
   { 0x2144, "TDMSLLN"},    /* N-bits in slot                                    , */\
   { 0x2194, "TDMBRMG"},    /* N-bits remaining                                  , */\
   { 0x21e0, "TDMDEL"},    /* data delay to FS                                  , */\
   { 0x21f0, "TDMADJ"},    /* data adjustment                                   , */\
   { 0x2201, "TDMOOMP"},    /* Received audio compression                        , */\
   { 0x2224, "TDMSSIZE"},    /* Sample size per slot                              , */\
   { 0x2271, "TDMTXDFO"},    /* Format unused bits in a slot                      , */\
   { 0x2291, "TDMTXUS0"},    /* Format unused slots GAINIO                        , */\
   { 0x22b1, "TDMTXUS1"},    /* Format unused slots DIO1                          , */\
   { 0x22d1, "TDMTXUS2"},    /* Format unused slots DIO2                          , */\
   { 0x2300, "TDMGIE"},    /* Control gain (channel in 0)                       , */\
   { 0x2310, "TDMDCE"},    /* Control audio  left (channel in 1 )               , */\
   { 0x2320, "TDMSPKE"},    /* Control audio right (channel in 2 )               , */\
   { 0x2330, "TDMCSE"},    /* Current sense                                     , */\
   { 0x2340, "TDMVSE"},    /* Voltage sense                                     , */\
   { 0x2350, "TDMGOE"},    /* DSP Gainout                                       , */\
   { 0x2360, "TDMCF2E"},    /* DSP 2                                             , */\
   { 0x2370, "TDMCF3E"},    /* DSP 3                                             , */\
   { 0x2380, "TDMCFE"},    /* DSP                                               , */\
   { 0x2390, "TDMES6"},    /* Loopback of Audio left (channel 1)                , */\
   { 0x23a0, "TDMES7"},    /* Loopback of Audio right (channel 2)               , */\
   { 0x23b0, "TDMCF4E"},    /* AEC ref right control                             , */\
   { 0x23c0, "TDMPD1E"},    /* PDM 1 control                                     , */\
   { 0x23d0, "TDMPD2E"},    /* PDM 2 control                                     , */\
   { 0x2401, "TDMGIN"},    /* IO gainin                                         , */\
   { 0x2421, "TDMLIO"},    /* IO audio left                                     , */\
   { 0x2441, "TDMRIO"},    /* IO audio right                                    , */\
   { 0x2461, "TDMCSIO"},    /* IO Current Sense                                  , */\
   { 0x2481, "TDMVSIO"},    /* IO voltage sense                                  , */\
   { 0x24a1, "TDMGOIO"},    /* IO gain out                                       , */\
   { 0x24c1, "TDMCFIO2"},    /* IO DSP 2                                          , */\
   { 0x24e1, "TDMCFIO3"},    /* IO DSP 3                                          , */\
   { 0x2501, "TDMCFIO"},    /* IO DSP                                            , */\
   { 0x2521, "TDMLPB6"},    /* IO Source 6                                       , */\
   { 0x2541, "TDMLPB7"},    /* IO Source 7                                       , */\
   { 0x2603, "TDMGS"},    /* Control gainin                                    , */\
   { 0x2643, "TDMDCS"},    /* tdm slot for audio left (channel 1)               , */\
   { 0x2683, "TDMSPKS"},    /* tdm slot for audio right (channel 2)              , */\
   { 0x26c3, "TDMCSS"},    /* Slot Position of Current Sense Out                , */\
   { 0x2703, "TDMVSS"},    /* Slot Position of Voltage sense                    , */\
   { 0x2743, "TDMCGOS"},    /* Slot Position of GAIN out                         , */\
   { 0x2783, "TDMCF2S"},    /* Slot Position DSPout2                             , */\
   { 0x27c3, "TDMCF3S"},    /* Slot Position DSPout3                             , */\
   { 0x2803, "TDMCFS"},    /* Slot Position of DSPout                           , */\
   { 0x2843, "TDMEDAT6S"},    /* Slot Position of loopback channel left            , */\
   { 0x2883, "TDMEDAT7S"},    /* Slot Position of loopback channel right           , */\
   { 0x2901, "TDMTXUS3"},    /* Format unused slots D3                            , */\
   { 0x3100, "PDMSM"},    /* PDM control                                       , */\
   { 0x3110, "PDMSTSEL"},    /* PDM Decimator input selection                     , */\
   { 0x3120, "PDMSTENBL"},    /* Side tone input enable                            , */\
   { 0x3130, "PDMLSEL"},    /* PDM data selection for left channel during PDM direct mode, */\
   { 0x3140, "PDMRSEL"},    /* PDM data selection for right channel during PDM direct mode, */\
   { 0x3150, "MICVDDE"},    /* Enable MICVDD                                     , */\
   { 0x3201, "PDMCLRAT"},    /* PDM BCK/Fs ratio                                  , */\
   { 0x3223, "PDMGAIN"},    /* PDM gain                                          , */\
   { 0x3263, "PDMOSEL"},    /* PDM output selection - RE/FE data combination     , */\
   { 0x32a0, "SELCFHAPD"},    /* Select the source for haptic data output (not for customer), */\
   { 0x4000, "ISTVDDS"},    /* Status POR                                        , */\
   { 0x4010, "ISTPLLS"},    /* Status PLL lock                                   , */\
   { 0x4020, "ISTOTDS"},    /* Status OTP alarm                                  , */\
   { 0x4030, "ISTOVDS"},    /* Status OVP alarm                                  , */\
   { 0x4040, "ISTUVDS"},    /* Status UVP alarm                                  , */\
   { 0x4050, "ISTCLKS"},    /* Status clocks stable                              , */\
   { 0x4060, "ISTMTPB"},    /* Status MTP busy                                   , */\
   { 0x4070, "ISTNOCLK"},    /* Status lost clock                                 , */\
   { 0x4080, "ISTSPKS"},    /* Status speaker error                              , */\
   { 0x4090, "ISTACS"},    /* Status cold start                                 , */\
   { 0x40a0, "ISTSWS"},    /* Status amplifier engage                           , */\
   { 0x40b0, "ISTWDS"},    /* Status watchdog                                   , */\
   { 0x40c0, "ISTAMPS"},    /* Status amplifier enable                           , */\
   { 0x40d0, "ISTAREFS"},    /* Status Ref enable                                 , */\
   { 0x40e0, "ISTADCCR"},    /* Status Control ADC                                , */\
   { 0x40f0, "ISTBODNOK"},    /* Status BOD                                        , */\
   { 0x4100, "ISTBSTCU"},    /* Status DCDC current limiting                      , */\
   { 0x4110, "ISTBSTHI"},    /* Status DCDC active                                , */\
   { 0x4120, "ISTBSTOC"},    /* Status DCDC OCP                                   , */\
   { 0x4130, "ISTBSTPKCUR"},    /* Status bst peakcur                                , */\
   { 0x4140, "ISTBSTVC"},    /* Status DCDC level 1x                              , */\
   { 0x4150, "ISTBST86"},    /* Status DCDC level 1.14x                           , */\
   { 0x4160, "ISTBST93"},    /* Status DCDC level 1.07x                           , */\
   { 0x4170, "ISTRCVLD"},    /* Status rcvldop ready                              , */\
   { 0x4180, "ISTOCPL"},    /* Status ocp alarm left                             , */\
   { 0x4190, "ISTOCPR"},    /* Status ocp alarm                                  , */\
   { 0x41a0, "ISTMWSRC"},    /* Status Waits HW I2C settings                      , */\
   { 0x41b0, "ISTMWCFC"},    /* Status waits CF config                            , */\
   { 0x41c0, "ISTMWSMU"},    /* Status Audio mute sequence                        , */\
   { 0x41d0, "ISTCFMER"},    /* Status cfma error                                 , */\
   { 0x41e0, "ISTCFMAC"},    /* Status cfma ack                                   , */\
   { 0x41f0, "ISTCLKOOR"},    /* Status flag_clk_out_of_range                      , */\
   { 0x4200, "ISTTDMER"},    /* Status tdm error                                  , */\
   { 0x4210, "ISTCLPL"},    /* Status clip left                                  , */\
   { 0x4220, "ISTCLPR"},    /* Status clip                                       , */\
   { 0x4230, "ISTOCPM"},    /* Status mic ocpok                                  , */\
   { 0x4250, "ISTLP1"},    /* Status low power mode1                            , */\
   { 0x4260, "ISTLA"},    /* Status low amplitude detection                    , */\
   { 0x4270, "ISTVDDP"},    /* Status VDDP greater than VBAT                     , */\
   { 0x4280, "ISTTAPDET"},    /* Status Tap  detected                              , */\
   { 0x4290, "ISTAUDMOD"},    /* Status Audio Mode activated                       , */\
   { 0x42a0, "ISTSAMMOD"},    /* Status SAM Mode activated                         , */\
   { 0x42b0, "ISTTAPMOD"},    /* Status Tap  Mode Activated                        , */\
   { 0x42c0, "ISTTAPTRG"},    /* Status Tap comparator triggered                   , */\
   { 0x4400, "ICLVDDS"},    /* Clear POR                                         , */\
   { 0x4410, "ICLPLLS"},    /* Clear PLL lock                                    , */\
   { 0x4420, "ICLOTDS"},    /* Clear OTP alarm                                   , */\
   { 0x4430, "ICLOVDS"},    /* Clear OVP alarm                                   , */\
   { 0x4440, "ICLUVDS"},    /* Clear UVP alarm                                   , */\
   { 0x4450, "ICLCLKS"},    /* Clear clocks stable                               , */\
   { 0x4460, "ICLMTPB"},    /* Clear mtp busy                                    , */\
   { 0x4470, "ICLNOCLK"},    /* Clear lost clk                                    , */\
   { 0x4480, "ICLSPKS"},    /* Clear speaker error                               , */\
   { 0x4490, "ICLACS"},    /* Clear cold started                                , */\
   { 0x44a0, "ICLSWS"},    /* Clear amplifier engage                            , */\
   { 0x44b0, "ICLWDS"},    /* Clear watchdog                                    , */\
   { 0x44c0, "ICLAMPS"},    /* Clear enbl amp                                    , */\
   { 0x44d0, "ICLAREFS"},    /* Clear ref enable                                  , */\
   { 0x44e0, "ICLADCCR"},    /* Clear control ADC                                 , */\
   { 0x44f0, "ICLBODNOK"},    /* Clear BOD                                         , */\
   { 0x4500, "ICLBSTCU"},    /* Clear DCDC current limiting                       , */\
   { 0x4510, "ICLBSTHI"},    /* Clear DCDC active                                 , */\
   { 0x4520, "ICLBSTOC"},    /* Clear DCDC OCP                                    , */\
   { 0x4530, "ICLBSTPC"},    /* Clear bst peakcur                                 , */\
   { 0x4540, "ICLBSTVC"},    /* Clear DCDC level 1x                               , */\
   { 0x4550, "ICLBST86"},    /* Clear DCDC level 1.14x                            , */\
   { 0x4560, "ICLBST93"},    /* Clear DCDC level 1.07x                            , */\
   { 0x4570, "ICLRCVLD"},    /* Clear rcvldop ready                               , */\
   { 0x4580, "ICLOCPL"},    /* Clear ocp alarm left                              , */\
   { 0x4590, "ICLOCPR"},    /* Clear ocp alarm                                   , */\
   { 0x45a0, "ICLMWSRC"},    /* Clear wait HW I2C settings                        , */\
   { 0x45b0, "ICLMWCFC"},    /* Clear wait cf config                              , */\
   { 0x45c0, "ICLMWSMU"},    /* Clear audio mute sequence                         , */\
   { 0x45d0, "ICLCFMER"},    /* Clear cfma err                                    , */\
   { 0x45e0, "ICLCFMAC"},    /* Clear cfma ack                                    , */\
   { 0x45f0, "ICLCLKOOR"},    /* Clear flag_clk_out_of_range                       , */\
   { 0x4600, "ICLTDMER"},    /* Clear tdm error                                   , */\
   { 0x4610, "ICLCLPL"},    /* Clear clip left                                   , */\
   { 0x4620, "ICLCLP"},    /* Clear clip                                        , */\
   { 0x4630, "ICLOCPM"},    /* Clear mic ocpok                                   , */\
   { 0x4650, "ICLLP1"},    /* Clear low power mode1                             , */\
   { 0x4660, "ICLLA"},    /* Clear low amplitude detection                     , */\
   { 0x4670, "ICLVDDP"},    /* Clear VDDP greater then VBAT                      , */\
   { 0x4680, "ICLTAPDET"},    /* Clear Tap  detected                               , */\
   { 0x4690, "ICLAUDMOD"},    /* Clear Audio Mode activated                        , */\
   { 0x46a0, "ICLSAMMOD"},    /* Clear SAM Mode activated                          , */\
   { 0x46b0, "ICLTAPMOD"},    /* Clear Tap  Mode Activated                         , */\
   { 0x46c0, "ICLTAPTRG"},    /* Clear Comparator Interrupt                        , */\
   { 0x4800, "IEVDDS"},    /* Enable por                                        , */\
   { 0x4810, "IEPLLS"},    /* Enable pll lock                                   , */\
   { 0x4820, "IEOTDS"},    /* Enable OTP alarm                                  , */\
   { 0x4830, "IEOVDS"},    /* Enable OVP alarm                                  , */\
   { 0x4840, "IEUVDS"},    /* Enable UVP alarm                                  , */\
   { 0x4850, "IECLKS"},    /* Enable clocks stable                              , */\
   { 0x4860, "IEMTPB"},    /* Enable mtp busy                                   , */\
   { 0x4870, "IENOCLK"},    /* Enable lost clk                                   , */\
   { 0x4880, "IESPKS"},    /* Enable speaker error                              , */\
   { 0x4890, "IEACS"},    /* Enable cold started                               , */\
   { 0x48a0, "IESWS"},    /* Enable amplifier engage                           , */\
   { 0x48b0, "IEWDS"},    /* Enable watchdog                                   , */\
   { 0x48c0, "IEAMPS"},    /* Enable enbl amp                                   , */\
   { 0x48d0, "IEAREFS"},    /* Enable ref enable                                 , */\
   { 0x48e0, "IEADCCR"},    /* Enable Control ADC                                , */\
   { 0x48f0, "IEBODNOK"},    /* Enable BOD                                        , */\
   { 0x4900, "IEBSTCU"},    /* Enable DCDC current limiting                      , */\
   { 0x4910, "IEBSTHI"},    /* Enable DCDC active                                , */\
   { 0x4920, "IEBSTOC"},    /* Enable DCDC OCP                                   , */\
   { 0x4930, "IEBSTPC"},    /* Enable bst peakcur                                , */\
   { 0x4940, "IEBSTVC"},    /* Enable DCDC level 1x                              , */\
   { 0x4950, "IEBST86"},    /* Enable DCDC level 1.14x                           , */\
   { 0x4960, "IEBST93"},    /* Enable DCDC level 1.07x                           , */\
   { 0x4970, "IERCVLD"},    /* Enable rcvldop ready                              , */\
   { 0x4980, "IEOCPL"},    /* Enable ocp alarm left                             , */\
   { 0x4990, "IEOCPR"},    /* Enable ocp alarm                                  , */\
   { 0x49a0, "IEMWSRC"},    /* Enable waits HW I2C settings                      , */\
   { 0x49b0, "IEMWCFC"},    /* Enable man wait cf config                         , */\
   { 0x49c0, "IEMWSMU"},    /* Enable man Audio mute sequence                    , */\
   { 0x49d0, "IECFMER"},    /* Enable cfma err                                   , */\
   { 0x49e0, "IECFMAC"},    /* Enable cfma ack                                   , */\
   { 0x49f0, "IECLKOOR"},    /* Enable flag_clk_out_of_range                      , */\
   { 0x4a00, "IETDMER"},    /* Enable tdm error                                  , */\
   { 0x4a10, "IECLPL"},    /* Enable clip left                                  , */\
   { 0x4a20, "IECLPR"},    /* Enable clip                                       , */\
   { 0x4a30, "IEOCPM1"},    /* Enable mic ocpok                                  , */\
   { 0x4a50, "IELP1"},    /* Enable low power mode1                            , */\
   { 0x4a60, "IELA"},    /* Enable low amplitude detection                    , */\
   { 0x4a70, "IEVDDP"},    /* Enable VDDP greater than VBAT                     , */\
   { 0x4a80, "IETAPDET"},    /* Enable Tap  detected                              , */\
   { 0x4a90, "IEAUDMOD"},    /* Enable Audio Mode activated                       , */\
   { 0x4aa0, "IESAMMOD"},    /* Enable SAM Mode activated                         , */\
   { 0x4ab0, "IETAPMOD"},    /* Enable Tap  Mode Activated                        , */\
   { 0x4ac0, "IETAPTRG"},    /* Enable comparator interrupt                       , */\
   { 0x4c00, "IPOVDDS"},    /* Polarity por                                      , */\
   { 0x4c10, "IPOPLLS"},    /* Polarity pll lock                                 , */\
   { 0x4c20, "IPOOTDS"},    /* Polarity OTP alarm                                , */\
   { 0x4c30, "IPOOVDS"},    /* Polarity OVP alarm                                , */\
   { 0x4c40, "IPOUVDS"},    /* Polarity UVP alarm                                , */\
   { 0x4c50, "IPOCLKS"},    /* Polarity clocks stable                            , */\
   { 0x4c60, "IPOMTPB"},    /* Polarity mtp busy                                 , */\
   { 0x4c70, "IPONOCLK"},    /* Polarity lost clk                                 , */\
   { 0x4c80, "IPOSPKS"},    /* Polarity speaker error                            , */\
   { 0x4c90, "IPOACS"},    /* Polarity cold started                             , */\
   { 0x4ca0, "IPOSWS"},    /* Polarity amplifier engage                         , */\
   { 0x4cb0, "IPOWDS"},    /* Polarity watchdog                                 , */\
   { 0x4cc0, "IPOAMPS"},    /* Polarity enbl amp                                 , */\
   { 0x4cd0, "IPOAREFS"},    /* Polarity ref enable                               , */\
   { 0x4ce0, "IPOADCCR"},    /* Polarity Control ADC                              , */\
   { 0x4cf0, "IPOBODNOK"},    /* Polarity BOD                                      , */\
   { 0x4d00, "IPOBSTCU"},    /* Polarity DCDC current limiting                    , */\
   { 0x4d10, "IPOBSTHI"},    /* Polarity DCDC active                              , */\
   { 0x4d20, "IPOBSTOC"},    /* Polarity DCDC OCP                                 , */\
   { 0x4d30, "IPOBSTPC"},    /* Polarity bst peakcur                              , */\
   { 0x4d40, "IPOBSTVC"},    /* Polarity DCDC level 1x                            , */\
   { 0x4d50, "IPOBST86"},    /* Polarity DCDC level 1.14x                         , */\
   { 0x4d60, "IPOBST93"},    /* Polarity DCDC level 1.07x                         , */\
   { 0x4d70, "IPORCVLD"},    /* Polarity rcvldop ready                            , */\
   { 0x4d80, "IPOOCPL"},    /* Polarity ocp alarm left                           , */\
   { 0x4d90, "IPOOCPR"},    /* Polarity ocp alarm                                , */\
   { 0x4da0, "IPOMWSRC"},    /* Polarity waits HW I2C settings                    , */\
   { 0x4db0, "IPOMWCFC"},    /* Polarity man wait cf config                       , */\
   { 0x4dc0, "IPOMWSMU"},    /* Polarity man audio mute sequence                  , */\
   { 0x4dd0, "IPOCFMER"},    /* Polarity cfma err                                 , */\
   { 0x4de0, "IPOCFMAC"},    /* Polarity cfma ack                                 , */\
   { 0x4df0, "IPOCLKOOR"},    /* Polarity flag_clk_out_of_range                    , */\
   { 0x4e00, "IPOTDMER"},    /* Polarity tdm error                                , */\
   { 0x4e10, "IPOCLPL"},    /* Polarity clip left                                , */\
   { 0x4e20, "IPOCLPR"},    /* Polarity clip                                     , */\
   { 0x4e30, "IPOOCPM"},    /* Polarity mic ocpok                                , */\
   { 0x4e50, "IPOLP1"},    /* Polarity low power mode1                          , */\
   { 0x4e60, "IPOLA"},    /* Polarity low amplitude detection                  , */\
   { 0x4e70, "IPOVDDP"},    /* Polarity VDDP greater than VBAT                   , */\
   { 0x4e80, "IPOLTAPDET"},    /* PolarityTap  detected                             , */\
   { 0x4e90, "IPOLAUDMOD"},    /* PolarityAudio Mode activated                      , */\
   { 0x4ea0, "IPOLSAMMOD"},    /* PolaritySAM Mode activated                        , */\
   { 0x4eb0, "IPOLTAPMOD"},    /* Polarity Tap  Mode Activated                      , */\
   { 0x4ec0, "IPOLTAPTRG"},    /* PolarityTap  Comparator Trigger                   , */\
   { 0x5001, "BSSCR"},    /* Battery Safeguard attack time                     , */\
   { 0x5023, "BSST"},    /* Battery Safeguard threshold voltage level         , */\
   { 0x5061, "BSSRL"},    /* Battery Safeguard maximum reduction               , */\
   { 0x5082, "BSSRR"},    /* Battery Safeguard release time                    , */\
   { 0x50b1, "BSSHY"},    /* Battery Safeguard hysteresis                      , */\
   { 0x50d0, "BSSAC"},    /* Reset clipper - Auto clear                        , */\
   { 0x50e0, "BSSR"},    /* Battery voltage read out                          , */\
   { 0x50f0, "BSSBY"},    /* Bypass HW clipper                                 , */\
   { 0x5100, "BSSS"},    /* Vbat prot steepness                               , */\
   { 0x5110, "INTSMUTE"},    /* Soft mute HW                                      , */\
   { 0x5120, "CFSML"},    /* Soft mute FW left                                 , */\
   { 0x5130, "CFSM"},    /* Soft mute FW                                      , */\
   { 0x5140, "HPFBYPL"},    /* Bypass HPF left                                   , */\
   { 0x5150, "HPFBYP"},    /* Bypass HPF                                        , */\
   { 0x5160, "DPSAL"},    /* Enable DPSA left                                  , */\
   { 0x5170, "DPSA"},    /* Enable DPSA                                       , */\
   { 0x5187, "VOL"},    /* FW volume control for primary audio channel       , */\
   { 0x5200, "HNDSFRCV"},    /* Selection receiver                                , */\
   { 0x5222, "CLIPCTRL"},    /* Clip control setting                              , */\
   { 0x5257, "AMPGAIN"},    /* Amplifier gain                                    , */\
   { 0x52d0, "SLOPEE"},    /* Enables slope control                             , */\
   { 0x52e0, "SLOPESET"},    /* Slope speed setting (bin. coded)                  , */\
   { 0x5c07, "CFTAPPAT"},    /* Coolflux tap pattern                              , */\
   { 0x5c83, "TAPDBGINFO"},    /* Reserved                                          , */\
   { 0x5d0f, "TATPSTAT1"},    /* Tap Status 1 from CF FW                           , */\
   { 0x5f03, "TCOMPTHR"},    /* Comparator threshold (in uV)                      , */\
   { 0x6081, "PGAGAIN"},    /* PGA gain selection                                , */\
   { 0x6123, "TDMSPKG"},    /* System gain (INPLEV 0)                            , */\
   { 0x6505, "LPM1LVL"},    /* low power mode1 detector   ctrl threshold for low_audio_lvl , */\
   { 0x6565, "LPM1HLD"},    /* Low power mode1 detector, ctrl hold time before low audio is reckoned to be low audio, */\
   { 0x65c0, "LPM1DIS"},    /* low power mode1 detector control                  , */\
   { 0x6630, "DCDIS"},    /* DCDC                                              , */\
   { 0x6801, "TDMSRCMAP"},    /* tdm source mapping                                , */\
   { 0x6821, "TDMSRCAS"},    /* frame a selection                                 , */\
   { 0x6841, "TDMSRCBS"},    /* frame b selection                                 , */\
   { 0x68a0, "ANC1C"},    /* ANC one s complement                              , */\
   { 0x6901, "SAMMODE"},    /* Sam mode                                          , */\
   { 0x7033, "DCMCC"},    /* Max coil current                                  , */\
   { 0x7071, "DCCV"},    /* Slope compensation current, represents LxF (inductance x frequency) value , */\
   { 0x7090, "DCIE"},    /* Adaptive boost mode                               , */\
   { 0x70a0, "DCSR"},    /* Soft ramp up/down                                 , */\
   { 0x70c1, "DCINSEL"},    /* DCDC IIR input Selection                          , */\
   { 0x70f0, "DCPWM"},    /* DCDC PWM only mode                                , */\
   { 0x7504, "DCTRIP"},    /* Adaptive boost trip levels 1, effective only when boost_intelligent is set to 1, */\
   { 0x7554, "DCTRIP2"},    /* Adaptive boost trip level 2, effective only when boost_intelligent is set to 1, */\
   { 0x75a4, "DCTRIPT"},    /* Adaptive boost trip levels, effective only when boost_intelligent is set to 1, */\
   { 0x7635, "DCVOF"},    /* First boost voltage level                         , */\
   { 0x7695, "DCVOS"},    /* Second boost voltage level                        , */\
   { 0x9000, "RST"},    /* Reset                                             , */\
   { 0x9011, "DMEM"},    /* Target memory                                     , */\
   { 0x9030, "AIF"},    /* Auto increment                                    , */\
   { 0x9040, "CFINT"},    /* Interrupt - auto clear                            , */\
   { 0x9050, "CFCGATE"},    /* Coolflux clock gating disabling control           , */\
   { 0x9080, "REQCMD"},    /* Firmware event request rpc command                , */\
   { 0x9090, "REQRST"},    /* Firmware event request reset restart              , */\
   { 0x90a0, "REQMIPS"},    /* Firmware event request short on mips              , */\
   { 0x90b0, "REQMUTED"},    /* Firmware event request mute sequence ready        , */\
   { 0x90c0, "REQVOL"},    /* Firmware event request volume ready               , */\
   { 0x90d0, "REQDMG"},    /* Firmware event request speaker damage detected    , */\
   { 0x90e0, "REQCAL"},    /* Firmware event request calibration completed      , */\
   { 0x90f0, "REQRSV"},    /* Firmware event request reserved                   , */\
   { 0x910f, "MADD"},    /* Memory address                                    , */\
   { 0x920f, "MEMA"},    /* Activate memory access                            , */\
   { 0x9307, "ERR"},    /* Error flags                                       , */\
   { 0x9380, "ACKCMD"},    /* Firmware event acknowledge rpc command            , */\
   { 0x9390, "ACKRST"},    /* Firmware event acknowledge reset restart          , */\
   { 0x93a0, "ACKMIPS"},    /* Firmware event acknowledge short on mips          , */\
   { 0x93b0, "ACKMUTED"},    /* Firmware event acknowledge mute sequence ready    , */\
   { 0x93c0, "ACKVOL"},    /* Firmware event acknowledge volume ready           , */\
   { 0x93d0, "ACKDMG"},    /* Firmware event acknowledge speaker damage detected, */\
   { 0x93e0, "ACKCAL"},    /* Firmware event acknowledge calibration completed  , */\
   { 0x93f0, "ACKRSV"},    /* Firmware event acknowledge reserved               , */\
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
   { 0xf042, "DCMCCCL"},    /* Calibration delta current limit DCDC              , */\
   { 0xf078, "USERDEF"},    /* Reserved space for allowing customer to store speaker information, */\
   { 0xf40f, "R25C"},    /* Ron resistance of  speaker coil                   , */\
   { 0xffff,"Unknown bitfield enum" }   /* not found */\
};

#define TFA9912_BITNAMETABLE static tfaBfName_t Tfa9912BitNames[]= {\
   { 0x0, "powerdown"},    /* Powerdown selection                               , */\
   { 0x10, "reset"},    /* I2C Reset - Auto clear                            , */\
   { 0x20, "enbl_coolflux"},    /* Enable CoolFlux                                   , */\
   { 0x30, "enbl_amplifier"},    /* Enables the Amplifier                             , */\
   { 0x40, "enbl_boost"},    /* Activate DC-to-DC converter                       , */\
   { 0x50, "coolflux_configured"},    /* Coolflux configured                               , */\
   { 0x60, "sel_enbl_amplifier"},    /* CoolFlux controls amplifier                       , */\
   { 0x71, "int_pad_io"},    /* Interrupt config                                  , */\
   { 0x90, "fs_pulse_sel"},    /* Audio sample reference                            , */\
   { 0xb0, "bypass_ocp"},    /* Bypass OCP                                        , */\
   { 0xc0, "test_ocp"},    /* OCP testing control                               , */\
   { 0x101, "vamp_sel"},    /* Amplifier input selection                         , */\
   { 0x120, "src_set_configured"},    /* I2C configured                                    , */\
   { 0x130, "execute_cold_start"},    /* Execute cold start                                , */\
   { 0x140, "enbl_fro8m_auto_off"},    /* Internal osc off at PWDN                          , */\
   { 0x150, "man_enbl_brown_out"},    /* Reaction on BOD                                   , */\
   { 0x160, "enbl_bod"},    /* BOD Enable                                        , */\
   { 0x170, "enbl_bod_hyst"},    /* BOD Hysteresis                                    , */\
   { 0x181, "bod_delay"},    /* BOD filter                                        , */\
   { 0x1a1, "bod_lvlsel"},    /* BOD threshold                                     , */\
   { 0x1d0, "disable_mute_time_out"},    /* Time out SB mute sequence                         , */\
   { 0x1e0, "pwm_sel_rcv_ns"},    /* Noise shaper selection                            , */\
   { 0x1f0, "man_enbl_watchdog"},    /* Watchdog enable                                   , */\
   { 0x203, "audio_fs"},    /* Sample rate (fs)                                  , */\
   { 0x240, "input_level"},    /* TDM output attenuation                            , */\
   { 0x255, "cs_frac_delay"},    /* V/I Fractional delay                              , */\
   { 0x2b0, "bypass_hvbat_filter"},    /* Bypass HVBAT filter                               , */\
   { 0x2c0, "tdm_tfa9872_compatible"},    /* TDM Compatibility with TFA9872                    , */\
   { 0x2d0, "sel_hysteresis"},    /* Select hysteresis for clock range detector        , */\
   { 0x2e0, "enbl_adc10"},    /* ADC10 Enable -  I2C direct mode                   , */\
   { 0x30f, "device_rev"},    /* Revision info                                     , */\
   { 0x401, "pll_clkin_sel"},    /* PLL external ref clock                            , */\
   { 0x420, "pll_clkin_sel_osc"},    /* PLL internal ref clock                            , */\
   { 0x430, "cf_clock_scaling"},    /* Coolflux DSP clock scaling, low power mode        , */\
   { 0x441, "sel_cf_clock"},    /* Coolflux DSP clock scaler selection for low power mode, */\
   { 0x460, "tdm_intf_sel"},    /* TDM clock selection                               , */\
   { 0x470, "disable_auto_sel_refclk"},    /* Disable Automatic dsp clock source selection      , */\
   { 0x480, "sel_clk_src"},    /* I2C selection of DSP clock when auto select is disabled, */\
   { 0x490, "wdt_tim_clk_src"},    /* I2C selection of Watchdog and Timer clock         , */\
   { 0x510, "enbl_spkr_ss"},    /* Enable speaker path                               , */\
   { 0x530, "enbl_volsense"},    /* Voltage sense                                     , */\
   { 0x550, "enbl_cursense"},    /* Current sense                                     , */\
   { 0x560, "enbl_pdm_ss"},    /* Sub-system PDM                                    , */\
   { 0x580, "enbl_pga_chop"},    /* Enable PGA chop clock                             , */\
   { 0x590, "enbl_tdm_ss"},    /* Sub-system TDM                                    , */\
   { 0x5a0, "enbl_bst_ss"},    /* Sub-system boost                                  , */\
   { 0x5b0, "enbl_adc10_ss"},    /* Sub-system ADC                                    , */\
   { 0x5c0, "enbl_faim_ss"},    /* Sub-system FAIM                                   , */\
   { 0x5d0, "enbl_tim_clk"},    /* CF Sub-system timer                               , */\
   { 0x5e0, "enbl_wdt_clk"},    /* CF Sub-system WDT                                 , */\
   { 0x5f0, "faim_enable_vbg"},    /* Over rule of vbg for FaIM access                  , */\
   { 0x600, "aux_spkr_sel"},    /* Input selection for TAP/SAM                       , */\
   { 0x620, "bypass_tapdec_reset"},    /* Tap decimator reset bypass - Bypass the decimator reset from tapdec, */\
   { 0x631, "car_dec_in_sel0"},    /* Cardec input 0 sel                                , */\
   { 0x651, "car_dec_in_sel1"},    /* Cardec input sel                                  , */\
   { 0x670, "tapdec_sel"},    /* Select TAP/Cardec for TAP                         , */\
   { 0x680, "comp_count"},    /* Comparator o/p filter selection                   , */\
   { 0x691, "startup_mode"},    /* Startup Mode Selection                            , */\
   { 0x6b0, "enable_auto_tap_switching"},    /* Enable auto tap switching                         , */\
   { 0x6c1, "comp_init_time"},    /* Comparator initialization time to be used in Tap Machine, */\
   { 0x6e1, "ana_init_time"},    /* Analog initialization time to be used in Tap Machine, */\
   { 0x707, "clkchk_th_hi"},    /* Clock check Higher Threshold                      , */\
   { 0x787, "clkchk_th_lo"},    /* Clock check Higher Threshold                      , */\
   { 0x802, "ctrl_on2off_criterion"},    /* Amplifier on-off criteria for shutdown            , */\
   { 0x832, "ctrl_on2tap_criterion"},    /* Amplifier on-off criteria for tap mode entry      , */\
   { 0xd00, "side_tone_gain_sel"},    /* PDM side tone gain selector                       , */\
   { 0xd18, "side_tone_gain"},    /* Side tone gain                                    , */\
   { 0xda0, "mute_side_tone"},    /* Side tone soft mute                               , */\
   { 0xdb0, "side_tone_1scomplement"},    /* side tone one s complement                        , */\
   { 0xe07, "ctrl_digtoana"},    /* Spare control from digital to analog              , */\
   { 0xf0f, "hidden_code"},    /* 5A6Bh, 23147d to access registers (default for engineering), */\
   { 0x1000, "flag_por"},    /* POR                                               , */\
   { 0x1010, "flag_pll_lock"},    /* PLL lock                                          , */\
   { 0x1020, "flag_otpok"},    /* OTP alarm                                         , */\
   { 0x1030, "flag_ovpok"},    /* OVP alarm                                         , */\
   { 0x1040, "flag_uvpok"},    /* UVP alarm                                         , */\
   { 0x1050, "flag_clocks_stable"},    /* Clocks stable                                     , */\
   { 0x1060, "flag_mtp_busy"},    /* MTP busy                                          , */\
   { 0x1070, "flag_lost_clk"},    /* Lost clock                                        , */\
   { 0x1090, "flag_cold_started"},    /* Cold Start                                        , */\
   { 0x10a0, "flag_engage"},    /* Amplifier engage                                  , */\
   { 0x10b0, "flag_watchdog_reset"},    /* Watchdog                                          , */\
   { 0x10c0, "flag_enbl_amp"},    /* Amplifier enable                                  , */\
   { 0x10d0, "flag_enbl_ref"},    /* References enable                                 , */\
   { 0x10e0, "flag_adc10_ready"},    /* Control ADC                                       , */\
   { 0x10f0, "flag_bod_vddd_nok"},    /* BOD                                               , */\
   { 0x1100, "flag_bst_bstcur"},    /* DCDC current limiting                             , */\
   { 0x1110, "flag_bst_hiz"},    /* DCDC active                                       , */\
   { 0x1120, "flag_bst_ocpok"},    /* DCDC OCP nmos                                     , */\
   { 0x1130, "flag_bst_peakcur"},    /* Indicates current is max in DC-to-DC converter    , */\
   { 0x1140, "flag_bst_voutcomp"},    /* DCDC level 1x                                     , */\
   { 0x1150, "flag_bst_voutcomp86"},    /* DCDC level 1.14x                                  , */\
   { 0x1160, "flag_bst_voutcomp93"},    /* DCDC level 1.07x                                  , */\
   { 0x1170, "flag_soft_mute_busy"},    /* side tone (un)mute busy                           , */\
   { 0x1180, "flag_soft_mute_state"},    /* side tone mute state                              , */\
   { 0x1190, "flag_tdm_lut_error"},    /* TDM LUT error                                     , */\
   { 0x11a2, "flag_tdm_status"},    /* TDM status bits                                   , */\
   { 0x11d0, "flag_tdm_error"},    /* TDM error                                         , */\
   { 0x1300, "flag_ocpokap"},    /* OCPOK pmos A                                      , */\
   { 0x1310, "flag_ocpokan"},    /* OCPOK nmos A                                      , */\
   { 0x1320, "flag_ocpokbp"},    /* OCPOK pmos B                                      , */\
   { 0x1330, "flag_ocpokbn"},    /* OCPOK nmos B                                      , */\
   { 0x1340, "flag_clipa_high"},    /* Clipping A to Vddp                                , */\
   { 0x1350, "flag_clipa_low"},    /* Clipping A to gnd                                 , */\
   { 0x1360, "flag_clipb_high"},    /* Clipping B to Vddp                                , */\
   { 0x1370, "flag_clipb_low"},    /* Clipping B to gnd                                 , */\
   { 0x1380, "flag_ocp_alarm"},    /* OCP  amplifier                                    , */\
   { 0x1390, "flag_clip"},    /* Amplifier  clipping                               , */\
   { 0x13a0, "flag_tap_comp_trig"},    /* Status Tap comparator triggered                   , */\
   { 0x13b0, "flag_cf_tapdetected"},    /* Status Tap  detected                              , */\
   { 0x13c0, "flag_man_wait_src_settings"},    /* Wait HW I2C settings                              , */\
   { 0x13d0, "flag_man_wait_cf_config"},    /* Wait CF config                                    , */\
   { 0x13e0, "flag_man_start_mute_audio"},    /* Audio mute sequence                               , */\
   { 0x1410, "flag_cf_speakererror"},    /* Speaker status                                    , */\
   { 0x1420, "flag_clk_out_of_range"},    /* External clock status                             , */\
   { 0x1433, "man_state"},    /* Device manager status                             , */\
   { 0x1471, "status_bst_mode"},    /* DCDC mode status bits                             , */\
   { 0x1490, "man_dsp_clk_src"},    /* DSP clock source selected by manager              , */\
   { 0x14a1, "man_startup_mode"},    /* Startup Mode Selected by Manager(Read Only)       , */\
   { 0x14c3, "tap_machine_state"},    /* Tap Machine State                                 , */\
   { 0x1509, "bat_adc"},    /* Battery voltage (V)                               , */\
   { 0x1608, "temp_adc"},    /* IC Temperature (C)                                , */\
   { 0x1709, "vddp_adc"},    /* IC VDDP voltage ( 1023*VDDP/13 V)                 , */\
   { 0x17a0, "flag_bst_bstcur_cf"},    /* DCDC current limiting for DSP                     , */\
   { 0x2000, "tdm_usecase"},    /* Mode setting                                      , */\
   { 0x2011, "dio4_input_sel"},    /* DIO4 Input selection                              , */\
   { 0x2040, "tdm_enable"},    /* Enable TDM interface                              , */\
   { 0x2060, "tdm_clk_inversion"},    /* Reception data to BCK clock                       , */\
   { 0x2073, "tdm_fs_ws_length"},    /* FS length                                         , */\
   { 0x20b0, "tdm_fs_ws_polarity"},    /* FS polarity                                       , */\
   { 0x20c3, "tdm_nbck"},    /* N-BCK's in FS                                     , */\
   { 0x2103, "tdm_nb_of_slots"},    /* N-slots in Frame                                  , */\
   { 0x2144, "tdm_slot_length"},    /* N-bits in slot                                    , */\
   { 0x2194, "tdm_bits_remaining"},    /* N-bits remaining                                  , */\
   { 0x21e0, "tdm_data_delay"},    /* data delay to FS                                  , */\
   { 0x21f0, "tdm_data_adjustment"},    /* data adjustment                                   , */\
   { 0x2201, "tdm_audio_sample_compression"},    /* Received audio compression                        , */\
   { 0x2224, "tdm_sample_size"},    /* Sample size per slot                              , */\
   { 0x2271, "tdm_txdata_format"},    /* Format unused bits in a slot                      , */\
   { 0x2291, "tdm_txdata_format_unused_slot_sd0"},    /* Format unused slots GAINIO                        , */\
   { 0x22b1, "tdm_txdata_format_unused_slot_sd1"},    /* Format unused slots DIO1                          , */\
   { 0x22d1, "tdm_txdata_format_unused_slot_sd2"},    /* Format unused slots DIO2                          , */\
   { 0x2300, "tdm_sink0_enable"},    /* Control gain (channel in 0)                       , */\
   { 0x2310, "tdm_sink1_enable"},    /* Control audio  left (channel in 1 )               , */\
   { 0x2320, "tdm_sink2_enable"},    /* Control audio right (channel in 2 )               , */\
   { 0x2330, "tdm_source0_enable"},    /* Current sense                                     , */\
   { 0x2340, "tdm_source1_enable"},    /* Voltage sense                                     , */\
   { 0x2350, "tdm_source2_enable"},    /* DSP Gainout                                       , */\
   { 0x2360, "tdm_source3_enable"},    /* DSP 2                                             , */\
   { 0x2370, "tdm_source4_enable"},    /* DSP 3                                             , */\
   { 0x2380, "tdm_source5_enable"},    /* DSP                                               , */\
   { 0x2390, "tdm_source6_enable"},    /* Loopback of Audio left (channel 1)                , */\
   { 0x23a0, "tdm_source7_enable"},    /* Loopback of Audio right (channel 2)               , */\
   { 0x2401, "tdm_sink0_io"},    /* IO gainin                                         , */\
   { 0x2421, "tdm_sink1_io"},    /* IO audio left                                     , */\
   { 0x2441, "tdm_sink2_io"},    /* IO audio right                                    , */\
   { 0x2461, "tdm_source0_io"},    /* IO Current Sense                                  , */\
   { 0x2481, "tdm_source1_io"},    /* IO voltage sense                                  , */\
   { 0x24a1, "tdm_source2_io"},    /* IO gain out                                       , */\
   { 0x24c1, "tdm_source3_io"},    /* IO DSP 2                                          , */\
   { 0x24e1, "tdm_source4_io"},    /* IO DSP 3                                          , */\
   { 0x2501, "tdm_source5_io"},    /* IO DSP                                            , */\
   { 0x2521, "tdm_source6_io"},    /* IO Source 6                                       , */\
   { 0x2541, "tdm_source7_io"},    /* IO Source 7                                       , */\
   { 0x2603, "tdm_sink0_slot"},    /* Control gainin                                    , */\
   { 0x2643, "tdm_sink1_slot"},    /* tdm slot for audio left (channel 1)               , */\
   { 0x2683, "tdm_sink2_slot"},    /* tdm slot for audio right (channel 2)              , */\
   { 0x26c3, "tdm_source0_slot"},    /* Slot Position of Current Sense Out                , */\
   { 0x2703, "tdm_source1_slot"},    /* Slot Position of Voltage sense                    , */\
   { 0x2743, "tdm_source2_slot"},    /* Slot Position of GAIN out                         , */\
   { 0x2783, "tdm_source3_slot"},    /* Slot Position DSPout2                             , */\
   { 0x27c3, "tdm_source4_slot"},    /* Slot Position DSPout3                             , */\
   { 0x2803, "tdm_source5_slot"},    /* Slot Position of DSPout                           , */\
   { 0x2843, "tdm_source6_slot"},    /* Slot Position of loopback channel left            , */\
   { 0x2883, "tdm_source7_slot"},    /* Slot Position of loopback channel right           , */\
   { 0x2901, "tdm_txdata_format_unused_slot_sd3"},    /* Format unused slots D3                            , */\
   { 0x3100, "pdm_mode"},    /* PDM control                                       , */\
   { 0x3110, "pdm_input_sel"},    /* PDM Decimator input selection                     , */\
   { 0x3120, "enbl_pdm_side_tone"},    /* Side tone input enable                            , */\
   { 0x3201, "pdm_nbck"},    /* PDM BCK/Fs ratio                                  , */\
   { 0x4000, "int_out_flag_por"},    /* Status POR                                        , */\
   { 0x4010, "int_out_flag_pll_lock"},    /* Status PLL lock                                   , */\
   { 0x4020, "int_out_flag_otpok"},    /* Status OTP alarm                                  , */\
   { 0x4030, "int_out_flag_ovpok"},    /* Status OVP alarm                                  , */\
   { 0x4040, "int_out_flag_uvpok"},    /* Status UVP alarm                                  , */\
   { 0x4050, "int_out_flag_clocks_stable"},    /* Status clocks stable                              , */\
   { 0x4060, "int_out_flag_mtp_busy"},    /* Status MTP busy                                   , */\
   { 0x4070, "int_out_flag_lost_clk"},    /* Status lost clock                                 , */\
   { 0x4080, "int_out_flag_cf_speakererror"},    /* Status speaker error                              , */\
   { 0x4090, "int_out_flag_cold_started"},    /* Status cold start                                 , */\
   { 0x40a0, "int_out_flag_engage"},    /* Status amplifier engage                           , */\
   { 0x40b0, "int_out_flag_watchdog_reset"},    /* Status watchdog                                   , */\
   { 0x40c0, "int_out_flag_enbl_amp"},    /* Status amplifier enable                           , */\
   { 0x40d0, "int_out_flag_enbl_ref"},    /* Status Ref enable                                 , */\
   { 0x40e0, "int_out_flag_adc10_ready"},    /* Status Control ADC                                , */\
   { 0x40f0, "int_out_flag_bod_vddd_nok"},    /* Status BOD                                        , */\
   { 0x4100, "int_out_flag_bst_bstcur"},    /* Status DCDC current limiting                      , */\
   { 0x4110, "int_out_flag_bst_hiz"},    /* Status DCDC active                                , */\
   { 0x4120, "int_out_flag_bst_ocpok"},    /* Status DCDC OCP                                   , */\
   { 0x4130, "int_out_flag_bst_peakcur"},    /* Status bst peakcur                                , */\
   { 0x4140, "int_out_flag_bst_voutcomp"},    /* Status DCDC level 1x                              , */\
   { 0x4150, "int_out_flag_bst_voutcomp86"},    /* Status DCDC level 1.14x                           , */\
   { 0x4160, "int_out_flag_bst_voutcomp93"},    /* Status DCDC level 1.07x                           , */\
   { 0x4190, "int_out_flag_ocp_alarm"},    /* Status ocp alarm                                  , */\
   { 0x41a0, "int_out_flag_man_wait_src_settings"},    /* Status Waits HW I2C settings                      , */\
   { 0x41b0, "int_out_flag_man_wait_cf_config"},    /* Status waits CF config                            , */\
   { 0x41c0, "int_out_flag_man_start_mute_audio"},    /* Status Audio mute sequence                        , */\
   { 0x41d0, "int_out_flag_cfma_err"},    /* Status cfma error                                 , */\
   { 0x41e0, "int_out_flag_cfma_ack"},    /* Status cfma ack                                   , */\
   { 0x41f0, "int_out_flag_clk_out_of_range"},    /* Status flag_clk_out_of_range                      , */\
   { 0x4200, "int_out_flag_tdm_error"},    /* Status tdm error                                  , */\
   { 0x4220, "int_out_flag_clip"},    /* Status clip                                       , */\
   { 0x4250, "int_out_flag_lp_detect_mode1"},    /* Status low power mode1                            , */\
   { 0x4260, "int_out_flag_low_amplitude"},    /* Status low amplitude detection                    , */\
   { 0x4270, "int_out_flag_vddp_gt_vbat"},    /* Status VDDP greater than VBAT                     , */\
   { 0x4280, "int_out_newtap"},    /* Status Tap  detected                              , */\
   { 0x4290, "int_out_audiomodeactive"},    /* Status Audio Mode activated                       , */\
   { 0x42a0, "int_out_sammodeactive"},    /* Status SAM Mode activated                         , */\
   { 0x42b0, "int_out_tapmodeactive"},    /* Status Tap  Mode Activated                        , */\
   { 0x42c0, "int_out_flag_tap_comp_trig"},    /* Status Tap comparator triggered                   , */\
   { 0x4400, "int_in_flag_por"},    /* Clear POR                                         , */\
   { 0x4410, "int_in_flag_pll_lock"},    /* Clear PLL lock                                    , */\
   { 0x4420, "int_in_flag_otpok"},    /* Clear OTP alarm                                   , */\
   { 0x4430, "int_in_flag_ovpok"},    /* Clear OVP alarm                                   , */\
   { 0x4440, "int_in_flag_uvpok"},    /* Clear UVP alarm                                   , */\
   { 0x4450, "int_in_flag_clocks_stable"},    /* Clear clocks stable                               , */\
   { 0x4460, "int_in_flag_mtp_busy"},    /* Clear mtp busy                                    , */\
   { 0x4470, "int_in_flag_lost_clk"},    /* Clear lost clk                                    , */\
   { 0x4480, "int_in_flag_cf_speakererror"},    /* Clear speaker error                               , */\
   { 0x4490, "int_in_flag_cold_started"},    /* Clear cold started                                , */\
   { 0x44a0, "int_in_flag_engage"},    /* Clear amplifier engage                            , */\
   { 0x44b0, "int_in_flag_watchdog_reset"},    /* Clear watchdog                                    , */\
   { 0x44c0, "int_in_flag_enbl_amp"},    /* Clear enbl amp                                    , */\
   { 0x44d0, "int_in_flag_enbl_ref"},    /* Clear ref enable                                  , */\
   { 0x44e0, "int_in_flag_adc10_ready"},    /* Clear control ADC                                 , */\
   { 0x44f0, "int_in_flag_bod_vddd_nok"},    /* Clear BOD                                         , */\
   { 0x4500, "int_in_flag_bst_bstcur"},    /* Clear DCDC current limiting                       , */\
   { 0x4510, "int_in_flag_bst_hiz"},    /* Clear DCDC active                                 , */\
   { 0x4520, "int_in_flag_bst_ocpok"},    /* Clear DCDC OCP                                    , */\
   { 0x4530, "int_in_flag_bst_peakcur"},    /* Clear bst peakcur                                 , */\
   { 0x4540, "int_in_flag_bst_voutcomp"},    /* Clear DCDC level 1x                               , */\
   { 0x4550, "int_in_flag_bst_voutcomp86"},    /* Clear DCDC level 1.14x                            , */\
   { 0x4560, "int_in_flag_bst_voutcomp93"},    /* Clear DCDC level 1.07x                            , */\
   { 0x4590, "int_in_flag_ocp_alarm"},    /* Clear ocp alarm                                   , */\
   { 0x45a0, "int_in_flag_man_wait_src_settings"},    /* Clear wait HW I2C settings                        , */\
   { 0x45b0, "int_in_flag_man_wait_cf_config"},    /* Clear wait cf config                              , */\
   { 0x45c0, "int_in_flag_man_start_mute_audio"},    /* Clear audio mute sequence                         , */\
   { 0x45d0, "int_in_flag_cfma_err"},    /* Clear cfma err                                    , */\
   { 0x45e0, "int_in_flag_cfma_ack"},    /* Clear cfma ack                                    , */\
   { 0x45f0, "int_in_flag_clk_out_of_range"},    /* Clear flag_clk_out_of_range                       , */\
   { 0x4600, "int_in_flag_tdm_error"},    /* Clear tdm error                                   , */\
   { 0x4620, "int_in_flag_clip"},    /* Clear clip                                        , */\
   { 0x4650, "int_in_flag_lp_detect_mode1"},    /* Clear low power mode1                             , */\
   { 0x4660, "int_in_flag_low_amplitude"},    /* Clear low amplitude detection                     , */\
   { 0x4670, "int_in_flag_vddp_gt_vbat"},    /* Clear VDDP greater then VBAT                      , */\
   { 0x4680, "int_in_newtap"},    /* Clear Tap  detected                               , */\
   { 0x4690, "int_in_audiomodeactive"},    /* Clear Audio Mode activated                        , */\
   { 0x46a0, "int_in_sammodeactive"},    /* Clear SAM Mode activated                          , */\
   { 0x46b0, "int_in_tapmodeactive"},    /* Clear Tap  Mode Activated                         , */\
   { 0x46c0, "int_in_flag_tap_comp_trig"},    /* Clear Comparator Interrupt                        , */\
   { 0x4800, "int_enable_flag_por"},    /* Enable por                                        , */\
   { 0x4810, "int_enable_flag_pll_lock"},    /* Enable pll lock                                   , */\
   { 0x4820, "int_enable_flag_otpok"},    /* Enable OTP alarm                                  , */\
   { 0x4830, "int_enable_flag_ovpok"},    /* Enable OVP alarm                                  , */\
   { 0x4840, "int_enable_flag_uvpok"},    /* Enable UVP alarm                                  , */\
   { 0x4850, "int_enable_flag_clocks_stable"},    /* Enable clocks stable                              , */\
   { 0x4860, "int_enable_flag_mtp_busy"},    /* Enable mtp busy                                   , */\
   { 0x4870, "int_enable_flag_lost_clk"},    /* Enable lost clk                                   , */\
   { 0x4880, "int_enable_flag_cf_speakererror"},    /* Enable speaker error                              , */\
   { 0x4890, "int_enable_flag_cold_started"},    /* Enable cold started                               , */\
   { 0x48a0, "int_enable_flag_engage"},    /* Enable amplifier engage                           , */\
   { 0x48b0, "int_enable_flag_watchdog_reset"},    /* Enable watchdog                                   , */\
   { 0x48c0, "int_enable_flag_enbl_amp"},    /* Enable enbl amp                                   , */\
   { 0x48d0, "int_enable_flag_enbl_ref"},    /* Enable ref enable                                 , */\
   { 0x48e0, "int_enable_flag_adc10_ready"},    /* Enable Control ADC                                , */\
   { 0x48f0, "int_enable_flag_bod_vddd_nok"},    /* Enable BOD                                        , */\
   { 0x4900, "int_enable_flag_bst_bstcur"},    /* Enable DCDC current limiting                      , */\
   { 0x4910, "int_enable_flag_bst_hiz"},    /* Enable DCDC active                                , */\
   { 0x4920, "int_enable_flag_bst_ocpok"},    /* Enable DCDC OCP                                   , */\
   { 0x4930, "int_enable_flag_bst_peakcur"},    /* Enable bst peakcur                                , */\
   { 0x4940, "int_enable_flag_bst_voutcomp"},    /* Enable DCDC level 1x                              , */\
   { 0x4950, "int_enable_flag_bst_voutcomp86"},    /* Enable DCDC level 1.14x                           , */\
   { 0x4960, "int_enable_flag_bst_voutcomp93"},    /* Enable DCDC level 1.07x                           , */\
   { 0x4990, "int_enable_flag_ocp_alarm"},    /* Enable ocp alarm                                  , */\
   { 0x49a0, "int_enable_flag_man_wait_src_settings"},    /* Enable waits HW I2C settings                      , */\
   { 0x49b0, "int_enable_flag_man_wait_cf_config"},    /* Enable man wait cf config                         , */\
   { 0x49c0, "int_enable_flag_man_start_mute_audio"},    /* Enable man Audio mute sequence                    , */\
   { 0x49d0, "int_enable_flag_cfma_err"},    /* Enable cfma err                                   , */\
   { 0x49e0, "int_enable_flag_cfma_ack"},    /* Enable cfma ack                                   , */\
   { 0x49f0, "int_enable_flag_clk_out_of_range"},    /* Enable flag_clk_out_of_range                      , */\
   { 0x4a00, "int_enable_flag_tdm_error"},    /* Enable tdm error                                  , */\
   { 0x4a20, "int_enable_flag_clip"},    /* Enable clip                                       , */\
   { 0x4a50, "int_enable_flag_lp_detect_mode1"},    /* Enable low power mode1                            , */\
   { 0x4a60, "int_enable_flag_low_amplitude"},    /* Enable low amplitude detection                    , */\
   { 0x4a70, "int_enable_flag_vddp_gt_vbat"},    /* Enable VDDP greater than VBAT                     , */\
   { 0x4a80, "int_enable_newtap"},    /* Enable Tap  detected                              , */\
   { 0x4a90, "int_enable_audiomodeactive"},    /* Enable Audio Mode activated                       , */\
   { 0x4aa0, "int_enable_sammodeactive"},    /* Enable SAM Mode activated                         , */\
   { 0x4ab0, "int_enable_tapmodeactive"},    /* Enable Tap  Mode Activated                        , */\
   { 0x4ac0, "int_enable_flag_tap_comp_trig"},    /* Enable comparator interrupt                       , */\
   { 0x4c00, "int_polarity_flag_por"},    /* Polarity por                                      , */\
   { 0x4c10, "int_polarity_flag_pll_lock"},    /* Polarity pll lock                                 , */\
   { 0x4c20, "int_polarity_flag_otpok"},    /* Polarity OTP alarm                                , */\
   { 0x4c30, "int_polarity_flag_ovpok"},    /* Polarity OVP alarm                                , */\
   { 0x4c40, "int_polarity_flag_uvpok"},    /* Polarity UVP alarm                                , */\
   { 0x4c50, "int_polarity_flag_clocks_stable"},    /* Polarity clocks stable                            , */\
   { 0x4c60, "int_polarity_flag_mtp_busy"},    /* Polarity mtp busy                                 , */\
   { 0x4c70, "int_polarity_flag_lost_clk"},    /* Polarity lost clk                                 , */\
   { 0x4c80, "int_polarity_flag_cf_speakererror"},    /* Polarity speaker error                            , */\
   { 0x4c90, "int_polarity_flag_cold_started"},    /* Polarity cold started                             , */\
   { 0x4ca0, "int_polarity_flag_engage"},    /* Polarity amplifier engage                         , */\
   { 0x4cb0, "int_polarity_flag_watchdog_reset"},    /* Polarity watchdog                                 , */\
   { 0x4cc0, "int_polarity_flag_enbl_amp"},    /* Polarity enbl amp                                 , */\
   { 0x4cd0, "int_polarity_flag_enbl_ref"},    /* Polarity ref enable                               , */\
   { 0x4ce0, "int_polarity_flag_adc10_ready"},    /* Polarity Control ADC                              , */\
   { 0x4cf0, "int_polarity_flag_bod_vddd_nok"},    /* Polarity BOD                                      , */\
   { 0x4d00, "int_polarity_flag_bst_bstcur"},    /* Polarity DCDC current limiting                    , */\
   { 0x4d10, "int_polarity_flag_bst_hiz"},    /* Polarity DCDC active                              , */\
   { 0x4d20, "int_polarity_flag_bst_ocpok"},    /* Polarity DCDC OCP                                 , */\
   { 0x4d30, "int_polarity_flag_bst_peakcur"},    /* Polarity bst peakcur                              , */\
   { 0x4d40, "int_polarity_flag_bst_voutcomp"},    /* Polarity DCDC level 1x                            , */\
   { 0x4d50, "int_polarity_flag_bst_voutcomp86"},    /* Polarity DCDC level 1.14x                         , */\
   { 0x4d60, "int_polarity_flag_bst_voutcomp93"},    /* Polarity DCDC level 1.07x                         , */\
   { 0x4d90, "int_polarity_flag_ocp_alarm"},    /* Polarity ocp alarm                                , */\
   { 0x4da0, "int_polarity_flag_man_wait_src_settings"},    /* Polarity waits HW I2C settings                    , */\
   { 0x4db0, "int_polarity_flag_man_wait_cf_config"},    /* Polarity man wait cf config                       , */\
   { 0x4dc0, "int_polarity_flag_man_start_mute_audio"},    /* Polarity man audio mute sequence                  , */\
   { 0x4dd0, "int_polarity_flag_cfma_err"},    /* Polarity cfma err                                 , */\
   { 0x4de0, "int_polarity_flag_cfma_ack"},    /* Polarity cfma ack                                 , */\
   { 0x4df0, "int_polarity_flag_clk_out_of_range"},    /* Polarity flag_clk_out_of_range                    , */\
   { 0x4e00, "int_polarity_flag_tdm_error"},    /* Polarity tdm error                                , */\
   { 0x4e20, "int_polarity_flag_clip"},    /* Polarity clip                                     , */\
   { 0x4e50, "int_polarity_flag_lp_detect_mode1"},    /* Polarity low power mode1                          , */\
   { 0x4e60, "int_polarity_flag_low_amplitude"},    /* Polarity low amplitude detection                  , */\
   { 0x4e70, "int_polarity_flag_vddp_gt_vbat"},    /* Polarity VDDP greater than VBAT                   , */\
   { 0x4e80, "int_polarity_newtap"},    /* PolarityTap  detected                             , */\
   { 0x4e90, "int_polarity_audiomodeactive"},    /* PolarityAudio Mode activated                      , */\
   { 0x4ea0, "int_polarity_sammodeactive"},    /* PolaritySAM Mode activated                        , */\
   { 0x4eb0, "int_polarity_tapmodeactive"},    /* Polarity Tap  Mode Activated                      , */\
   { 0x4ec0, "int_polarity_flag_tap_comp_trig"},    /* PolarityTap  Comparator Trigger                   , */\
   { 0x5001, "vbat_prot_attack_time"},    /* Battery Safeguard attack time                     , */\
   { 0x5023, "vbat_prot_thlevel"},    /* Battery Safeguard threshold voltage level         , */\
   { 0x5061, "vbat_prot_max_reduct"},    /* Battery Safeguard maximum reduction               , */\
   { 0x5082, "vbat_prot_release_time"},    /* Battery Safeguard release time                    , */\
   { 0x50b1, "vbat_prot_hysterese"},    /* Battery Safeguard hysteresis                      , */\
   { 0x50d0, "rst_min_vbat"},    /* Reset clipper - Auto clear                        , */\
   { 0x50e0, "sel_vbat"},    /* Battery voltage read out                          , */\
   { 0x50f0, "bypass_clipper"},    /* Bypass HW clipper                                 , */\
   { 0x5100, "batsense_steepness"},    /* Vbat prot steepness                               , */\
   { 0x5110, "soft_mute"},    /* Soft mute HW                                      , */\
   { 0x5130, "cf_mute"},    /* Soft mute FW                                      , */\
   { 0x5150, "bypass_hp"},    /* Bypass HPF                                        , */\
   { 0x5170, "enbl_dpsa"},    /* Enable DPSA                                       , */\
   { 0x5187, "cf_volume"},    /* FW volume control for primary audio channel       , */\
   { 0x5222, "ctrl_cc"},    /* Clip control setting                              , */\
   { 0x5257, "gain"},    /* Amplifier gain                                    , */\
   { 0x52d0, "ctrl_slopectrl"},    /* Enables slope control                             , */\
   { 0x52e0, "ctrl_slope"},    /* Slope speed setting (bin. coded)                  , */\
   { 0x5301, "dpsa_level"},    /* DPSA threshold levels                             , */\
   { 0x5321, "dpsa_release"},    /* DPSA Release time                                 , */\
   { 0x5340, "clipfast"},    /* Clock selection for HW clipper for Battery Safeguard, */\
   { 0x5350, "bypass_lp"},    /* Bypass the low power filter inside temperature sensor, */\
   { 0x5360, "enbl_low_latency"},    /* CF low latency outputs for add module             , */\
   { 0x5400, "first_order_mode"},    /* Overrule to 1st order mode of control stage when clipping, */\
   { 0x5410, "bypass_ctrlloop"},    /* Switch amplifier into open loop configuration     , */\
   { 0x5430, "icomp_engage"},    /* Engage of icomp                                   , */\
   { 0x5440, "ctrl_kickback"},    /* Prevent double pulses of output stage             , */\
   { 0x5450, "icomp_engage_overrule"},    /* To overrule the functional icomp_engage signal during validation, */\
   { 0x5503, "ctrl_dem"},    /* Enable DEM icomp and DEM one bit dac              , */\
   { 0x5543, "ctrl_dem_mismatch"},    /* Enable DEM icomp mismatch for testing             , */\
   { 0x5582, "dpsa_drive"},    /* Drive setting (bin. coded)  - I2C direct mode     , */\
   { 0x570a, "enbl_amp"},    /* Switch on the class-D power sections, each part of the analog sections can be switched on/off individually , */\
   { 0x57b0, "enbl_engage"},    /* Enables/engage power stage and control loop - I2C direct mode, */\
   { 0x57c0, "enbl_engage_pst"},    /* Enables/engage power stage and control loop       , */\
   { 0x5810, "hard_mute"},    /* Hard mute - PWM                                   , */\
   { 0x5820, "pwm_shape"},    /* PWM shape                                         , */\
   { 0x5844, "pwm_delay"},    /* PWM delay bits to set the delay, clock is 1/(k*2048*fs), */\
   { 0x5890, "reclock_pwm"},    /* Reclock the pwm signal inside analog              , */\
   { 0x58a0, "reclock_voltsense"},    /* Reclock the voltage sense pwm signal              , */\
   { 0x58c0, "enbl_pwm_phase_shift"},    /* Control for pwm phase shift                       , */\
   { 0x5c07, "flag_cf_tap_pattern"},    /* Coolflux tap pattern                              , */\
   { 0x5c83, "tap_debug_info"},    /* Reserved                                          , */\
   { 0x5d0f, "tap_status_1"},    /* Tap Status 1 from CF FW                           , */\
   { 0x5f03, "tap_comp_threshold"},    /* Comparator threshold (in uV)                      , */\
   { 0x6081, "pga_gain_set"},    /* PGA gain selection                                , */\
   { 0x60b0, "pga_lowpass_enable"},    /* Lowpass enable                                    , */\
   { 0x60c0, "pga_pwr_enable"},    /* PGA power enable                                  , */\
   { 0x60d0, "pga_switch_enable"},    /* PGA switch enable                                 , */\
   { 0x60e0, "pga_switch_aux_enable"},    /* Switch enable aux                                 , */\
   { 0x6123, "ctrl_att"},    /* System gain (INPLEV 0)                            , */\
   { 0x6265, "zero_lvl"},    /* ctrl threshold for zero X-ing                     , */\
   { 0x62c1, "ctrl_fb_resistor"},    /* Select amplifier feedback resistor connection     , */\
   { 0x62e1, "lownoisegain_mode"},    /* ctrl select mode                                  , */\
   { 0x6305, "threshold_lvl"},    /* ctrl threshold for low_audio_lvl                  , */\
   { 0x6365, "hold_time"},    /* ctrl hold time before low audio is reckoned to be low audio, */\
   { 0x6405, "lpm1_cal_offset"},    /* low power mode1 detector ctrl cal_offset from gain module , */\
   { 0x6465, "lpm1_zero_lvl"},    /* low power mode1 detector   ctrl threshold for zero X-ing  , */\
   { 0x64e1, "lpm1_mode"},    /* low power mode1 detector ctrl select mode         , */\
   { 0x6505, "lpm1_threshold_lvl"},    /* low power mode1 detector   ctrl threshold for low_audio_lvl , */\
   { 0x6565, "lpm1_hold_time"},    /* Low power mode1 detector, ctrl hold time before low audio is reckoned to be low audio, */\
   { 0x65c0, "disable_low_power_mode"},    /* low power mode1 detector control                  , */\
   { 0x6600, "dcdc_pfm20khz_limit"},    /* DCDC in PFM mode pwm mode is activated each 50us to force a pwm pulse, */\
   { 0x6611, "dcdc_ctrl_maxzercnt"},    /* DCDC. Number of zero current flags to count before going to pfm mode, */\
   { 0x6630, "dcdcoff_mode"},    /* DCDC                                              , */\
   { 0x6656, "dcdc_vbat_delta_detect"},    /* Threshold before booster is reacting on a delta Vbat (in PFM mode) by temporarily switching to PWM mode, */\
   { 0x66c0, "dcdc_ignore_vbat"},    /* Ignore an increase on Vbat                        , */\
   { 0x6700, "enbl_minion"},    /* Enables minion (small) power stage - direct ctrl  , */\
   { 0x6713, "vth_vddpvbat"},    /* select vddp-vbat thres signal                     , */\
   { 0x6750, "lpen_vddpvbat"},    /* select vddp-vbat filtered vs unfiltered compare   , */\
   { 0x6761, "ctrl_rfb"},    /* Feedback resistor selection  - I2C direct mode    , */\
   { 0x6801, "tdm_source_mapping"},    /* tdm source mapping                                , */\
   { 0x6821, "tdm_sourcea_frame_sel"},    /* frame a selection                                 , */\
   { 0x6841, "tdm_sourceb_frame_sel"},    /* frame b selection                                 , */\
   { 0x6901, "sam_mode"},    /* Sam mode                                          , */\
   { 0x6931, "pdmdat_h_sel"},    /* pdm out value when pdm_clk is higth               , */\
   { 0x6951, "pdmdat_l_sel"},    /* pdm out value when pdm_clk is low                 , */\
   { 0x6970, "cs_sam_set"},    /* Enable SAM input for current sense - I2C Direct Mode, */\
   { 0x6980, "cs_adc_nortz"},    /* Return to zero for current sense ADC              , */\
   { 0x6990, "sam_spkr_sel"},    /* SAM o/p sel during SAM and audio                  , */\
   { 0x6b00, "disable_engage"},    /* Disable auto engage                               , */\
   { 0x6c02, "ns_hp2ln_criterion"},    /* 0..7 zeroes at ns as threshold to swap from high_power to low_noise, */\
   { 0x6c32, "ns_ln2hp_criterion"},    /* 0..7 zeroes at ns as threshold to swap from low_noise to high_power, */\
   { 0x6c60, "sel_clip_pwms"},    /* To select clip-flags                              , */\
   { 0x6c72, "pwms_clip_lvl"},    /* To set the amount of pwm pulse that may be skipped before clip-flag is triggered. , */\
   { 0x6ca5, "spare_out"},    /* spare_out                                         , */\
   { 0x6d0f, "spare_in"},    /* spare_in                                          , */\
   { 0x6e10, "flag_lp_detect_mode1"},    /* low power mode 1 detection                        , */\
   { 0x6e20, "flag_low_amplitude"},    /* low amplitude detection                           , */\
   { 0x6e30, "flag_vddp_gt_vbat"},    /* vddp greater than vbat                            , */\
   { 0x7033, "boost_cur"},    /* Max coil current                                  , */\
   { 0x7071, "bst_slpcmplvl"},    /* Slope compensation current, represents LxF (inductance x frequency) value , */\
   { 0x7090, "boost_intel"},    /* Adaptive boost mode                               , */\
   { 0x70a0, "boost_speed"},    /* Soft ramp up/down                                 , */\
   { 0x70c1, "dcdc_sel"},    /* DCDC IIR input Selection                          , */\
   { 0x70f0, "dcdc_pwmonly"},    /* DCDC PWM only mode                                , */\
   { 0x7104, "bst_drive"},    /* Binary coded drive setting for boost converter power stage, */\
   { 0x7151, "bst_scalecur"},    /* For testing direct control scale current          , */\
   { 0x7174, "bst_slopecur"},    /* For testing direct control slope current  - I2C direct mode, */\
   { 0x71c1, "bst_slope"},    /* Boost slope speed                                 , */\
   { 0x71e0, "bst_bypass_bstcur"},    /* Bypass control for boost current settings         , */\
   { 0x71f0, "bst_bypass_bstfoldback"},    /* Bypass control for boost foldback                 , */\
   { 0x7200, "enbl_bst_engage"},    /* Enable power stage dcdc controller  - I2C direct mode, */\
   { 0x7210, "enbl_bst_hizcom"},    /* Enable hiz comparator  - I2C direct mode          , */\
   { 0x7220, "enbl_bst_peak2avg"},    /* Enable boost peak2avg functionality               , */\
   { 0x7230, "enbl_bst_peakcur"},    /* Enable peak current   - I2C direct mode           , */\
   { 0x7240, "enbl_bst_power"},    /* Enable line of the powerstage  - I2C direct mode  , */\
   { 0x7250, "enbl_bst_slopecur"},    /* Enable bit of max-current dac  - I2C direct mode  , */\
   { 0x7260, "enbl_bst_voutcomp"},    /* Enable vout comparators  - I2C direct mode        , */\
   { 0x7270, "enbl_bst_voutcomp86"},    /* Enable vout-86 comparators  - I2C direct mode     , */\
   { 0x7280, "enbl_bst_voutcomp93"},    /* Enable vout-93 comparators  - I2C direct mode     , */\
   { 0x7290, "enbl_bst_windac"},    /* Enable window dac  - I2C direct mode              , */\
   { 0x72a5, "bst_windac"},    /* for testing direct control windac  - I2C direct mode, */\
   { 0x7300, "boost_alg"},    /* Control for boost adaptive loop gain              , */\
   { 0x7311, "boost_loopgain"},    /* DCDC boost loopgain setting                       , */\
   { 0x7331, "bst_freq"},    /* DCDC boost frequency control                      , */\
   { 0x7430, "boost_track"},    /* Boost algorithm selection, effective only when boost_intelligent is set to 1, */\
   { 0x7494, "boost_hold_time"},    /* Hold time for DCDC booster, effective only when boost_intelligent is set to 1, */\
   { 0x74e0, "sel_dcdc_envelope_8fs"},    /* Selection of data for adaptive boost algorithm, effective only when boost_intelligent is set to 1, */\
   { 0x74f0, "ignore_flag_voutcomp86"},    /* Ignore flag_voutcomp86                            , */\
   { 0x7504, "boost_trip_lvl_1st"},    /* Adaptive boost trip levels 1, effective only when boost_intelligent is set to 1, */\
   { 0x7554, "boost_trip_lvl_2nd"},    /* Adaptive boost trip level 2, effective only when boost_intelligent is set to 1, */\
   { 0x75a4, "boost_trip_lvl_track"},    /* Adaptive boost trip levels, effective only when boost_intelligent is set to 1, */\
   { 0x7602, "track_decay"},    /* DCDC Boost decay speed after a peak value, effective only when boost_track is set to 1, */\
   { 0x7635, "frst_boost_voltage"},    /* First boost voltage level                         , */\
   { 0x7695, "scnd_boost_voltage"},    /* Second boost voltage level                        , */\
   { 0x7720, "pga_test_ldo_bypass"},    /* bypass internal PGA LDO                           , */\
   { 0x8001, "sel_clk_cs"},    /* Current sense clock duty cycle control            , */\
   { 0x8021, "micadc_speed"},    /* Current sense clock for MiCADC selection - 32/44.1/48 KHz Fs band only, */\
   { 0x8050, "cs_gain_control"},    /* Current sense gain control                        , */\
   { 0x8060, "cs_bypass_gc"},    /* Bypasses the CS gain correction                   , */\
   { 0x8087, "cs_gain"},    /* Current sense gain                                , */\
   { 0x8200, "enbl_cmfb"},    /* Current sense common mode feedback control        , */\
   { 0x8210, "invertpwm"},    /* Current sense common mode feedback pwm invert control, */\
   { 0x8222, "cmfb_gain"},    /* Current sense common mode feedback control gain   , */\
   { 0x8256, "cmfb_offset"},    /* Current sense common mode feedback control offset , */\
   { 0x8305, "cs_ktemp"},    /* First order temperature compensation coefficient  , */\
   { 0x8364, "cs_ktemp2"},    /* Second order temperature compensation coefficient , */\
   { 0x8400, "cs_adc_bsoinv"},    /* Bitstream inversion for current sense ADC         , */\
   { 0x8421, "cs_adc_hifreq"},    /* Frequency mode current sense ADC                  , */\
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
   { 0x8700, "enbl_cs_adc"},    /* Enable current sense ADC - I2C direct mode        , */\
   { 0x8710, "enbl_cs_inn1"},    /* Enable connection of current sense negative1 - I2C direct mode, */\
   { 0x8720, "enbl_cs_inn2"},    /* Enable connection of current sense negative2 - I2C direct mode, */\
   { 0x8730, "enbl_cs_inp1"},    /* Enable connection of current sense positive1      , */\
   { 0x8740, "enbl_cs_inp2"},    /* Enable connection of current sense positive2 - I2C direct mode, */\
   { 0x8750, "enbl_cs_ldo"},    /* Enable current sense LDO - I2C direct mode        , */\
   { 0x8760, "enbl_cs_nofloating_n"},    /* Connect current sense negative to gnda at transitions of booster or classd amplifiers. Otherwise floating (0), */\
   { 0x8770, "enbl_cs_nofloating_p"},    /* Connect current sense positive to gnda at transitions of booster or classd amplifiers. Otherwise floating (0), */\
   { 0x8780, "enbl_cs_vbatldo"},    /* Enable of current sense LDO -- I2C direct mode    , */\
   { 0x8800, "volsense_pwm_sel"},    /* Voltage sense PWM source selection control        , */\
   { 0x8810, "vol_cur_sense_dc_offset"},    /* voltage and current sense decimator offset control, */\
   { 0x8902, "cursense_comp_delay"},    /* To align compensation signal with current sense signal, */\
   { 0x8930, "cursense_comp_sign"},    /* To change polarity of compensation for current sense compensation, */\
   { 0x8940, "enbl_cursense_comp"},    /* To enable current sense compensation              , */\
   { 0x9000, "cf_rst_dsp"},    /* Reset                                             , */\
   { 0x9011, "cf_dmem"},    /* Target memory                                     , */\
   { 0x9030, "cf_aif"},    /* Auto increment                                    , */\
   { 0x9040, "cf_int"},    /* Interrupt - auto clear                            , */\
   { 0x9050, "cf_cgate_off"},    /* Coolflux clock gating disabling control           , */\
   { 0x9080, "cf_req_cmd"},    /* Firmware event request rpc command                , */\
   { 0x9090, "cf_req_reset"},    /* Firmware event request reset restart              , */\
   { 0x90a0, "cf_req_mips"},    /* Firmware event request short on mips              , */\
   { 0x90b0, "cf_req_mute_ready"},    /* Firmware event request mute sequence ready        , */\
   { 0x90c0, "cf_req_volume_ready"},    /* Firmware event request volume ready               , */\
   { 0x90d0, "cf_req_damage"},    /* Firmware event request speaker damage detected    , */\
   { 0x90e0, "cf_req_calibrate_ready"},    /* Firmware event request calibration completed      , */\
   { 0x90f0, "cf_req_reserved"},    /* Firmware event request reserved                   , */\
   { 0x910f, "cf_madd"},    /* Memory address                                    , */\
   { 0x920f, "cf_mema"},    /* Activate memory access                            , */\
   { 0x9307, "cf_err"},    /* Error flags                                       , */\
   { 0x9380, "cf_ack_cmd"},    /* Firmware event acknowledge rpc command            , */\
   { 0x9390, "cf_ack_reset"},    /* Firmware event acknowledge reset restart          , */\
   { 0x93a0, "cf_ack_mips"},    /* Firmware event acknowledge short on mips          , */\
   { 0x93b0, "cf_ack_mute_ready"},    /* Firmware event acknowledge mute sequence ready    , */\
   { 0x93c0, "cf_ack_volume_ready"},    /* Firmware event acknowledge volume ready           , */\
   { 0x93d0, "cf_ack_damage"},    /* Firmware event acknowledge speaker damage detected, */\
   { 0x93e0, "cf_ack_calibrate_ready"},    /* Firmware event acknowledge calibration completed  , */\
   { 0x93f0, "cf_ack_reserved"},    /* Firmware event acknowledge reserved               , */\
   { 0xa007, "mtpkey1"},    /* 5Ah, 90d To access KEY1_Protected registers (Default for engineering), */\
   { 0xa107, "mtpkey2"},    /* MTP KEY2 register                                 , */\
   { 0xa200, "key01_locked"},    /* Indicates KEY1 is locked                          , */\
   { 0xa210, "key02_locked"},    /* Indicates KEY2 is locked                          , */\
   { 0xa302, "mtp_man_address_in"},    /* MTP address from I2C register for read/writing mtp in manual single word mode, */\
   { 0xa330, "man_copy_mtp_to_iic"},    /* Start copying single word from mtp to I2C mtp register, */\
   { 0xa340, "man_copy_iic_to_mtp"},    /* Start copying single word from I2C mtp register to mtp, */\
   { 0xa350, "auto_copy_mtp_to_iic"},    /* Start copying all the data from mtp to I2C mtp registers, */\
   { 0xa360, "auto_copy_iic_to_mtp"},    /* Start copying data from I2C mtp registers to mtp  , */\
   { 0xa400, "faim_set_clkws"},    /* Sets the FaIM controller clock wait state register, */\
   { 0xa410, "faim_sel_evenrows"},    /* All even rows of the FaIM are selected, active high, */\
   { 0xa420, "faim_sel_oddrows"},    /* All odd rows of the FaIM are selected, all rows in combination with sel_evenrows, */\
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
   { 0xc038, "enbl_ref"},    /* Switch on the analog references, each part of the references can be switched on/off individually -  - I2C direct mode, */\
   { 0xc0d0, "enbl_ringo"},    /* Enable the ring oscillator for test purpose       , */\
   { 0xc0e0, "use_direct_clk_ctrl"},    /* Direct clock control to overrule several functions for testing, */\
   { 0xc0f0, "use_direct_pll_ctrl"},    /* Direct PLL control to overrule several functions for testing, */\
   { 0xc100, "enbl_tsense"},    /* Temperature sensor enable control - I2C direct mode, */\
   { 0xc110, "tsense_hibias"},    /* Bit to set the biasing in temp sensor to high  - I2C direct mode, */\
   { 0xc120, "enbl_flag_vbg"},    /* Enable flagging of bandgap out of control         , */\
   { 0xc130, "tap_comp_enable"},    /* Tap Comparator enable control - I2C direct mode   , */\
   { 0xc140, "tap_comp_switch_enable"},    /* Tap Comparator Switch enable control - I2C direct mode, */\
   { 0xc150, "tap_comp_switch_aux_enable"},    /* Tap Comparator Switch enable control - I2C direct mode, */\
   { 0xc161, "tap_comp_test_enable"},    /* Comparator threshold - fine value                 , */\
   { 0xc180, "curdist_enable"},    /* Enable control - I2C direct mode                  , */\
   { 0xc190, "vbg2i_enbl"},    /* Enable control - I2C direct mode                  , */\
   { 0xc1a0, "bg_filt_bypass_enbl"},    /* Enable control                                    , */\
   { 0xc20f, "abist_offset"},    /* Offset control for ABIST testing (two's complement), */\
   { 0xc300, "bypasslatch"},    /* Bypass latch                                      , */\
   { 0xc311, "sourcea"},    /* Set OUTA to                                       , */\
   { 0xc331, "sourceb"},    /* Set OUTB to                                       , */\
   { 0xc350, "inverta"},    /* Invert pwma test signal                           , */\
   { 0xc360, "invertb"},    /* Invert pwmb test signal                           , */\
   { 0xc374, "pulselength"},    /* Pulse length setting test input for amplifier (clock d - k*2048*fs), */\
   { 0xc3c0, "tdm_enable_loopback"},    /* TDM loopback test                                 , */\
   { 0xc3d0, "test_abistfft_enbl"},    /* FFT Coolflux                                      , */\
   { 0xc3e0, "test_pwr_switch"},    /* Test mode for digital power switches  core sw/mem sw/micvdd sw, */\
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
   { 0xc560, "bypass_fro8"},    /* Bypass fro8 with pdm_clk                          , */\
   { 0xc570, "test_enbl_cs"},    /* Enable for digimux mode of current sense          , */\
   { 0xc5b0, "pga_test_enable"},    /* Enable PGA test mode                              , */\
   { 0xc5c0, "pga_test_offset_enable"},    /* Enable PGA test offset                            , */\
   { 0xc5d0, "pga_test_shortinput_enable"},    /* Enable PGA test short input                       , */\
   { 0xc600, "enbl_pwm_dcc"},    /* Enables direct control of pwm duty cycle for DCDC power stage, */\
   { 0xc613, "pwm_dcc_cnt"},    /* Control pwm duty cycle when enbl_pwm_dcc is 1     , */\
   { 0xc650, "enbl_ldo_stress"},    /* Enable stress of internal supply voltages powerstages, */\
   { 0xc660, "enbl_powerswitch"},    /* Vddd core power switch control - overrules the manager control, */\
   { 0xc707, "digimuxa_sel"},    /* DigimuxA input selection control routed to DIO4 (see Digimux list for details), */\
   { 0xc787, "digimuxb_sel"},    /* DigimuxB input selection control routed to DIO3 (see Digimux list for details), */\
   { 0xc807, "digimuxc_sel"},    /* DigimuxC input selection control routed to TDO (see Digimux list for details), */\
   { 0xc901, "dio1_ehs"},    /* Speed/load setting for DIO1 IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
   { 0xc921, "dio2_ehs"},    /* Speed/load setting for DIO2 IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
   { 0xc941, "dio3_ehs"},    /* Speed/load setting for DIO3 cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
   { 0xc961, "dio4_ehs"},    /* Speed/load setting for DIO4 IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
   { 0xc981, "spdmo_ehs"},    /* Speed/load setting for PDMO IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
   { 0xc9a1, "tdo_ehs"},    /* Speed/load setting for TDM IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
   { 0xc9c0, "int_ehs"},    /* Slew Rate INT IO cell, clk or data mode range (see IIC3V3 IO cell datasheet), */\
   { 0xc9d0, "pdmclk_ehs"},    /* Slew RateBCK2/PDMCLK IO cell, clk or data mode range (see IIC3V3 IO cell datasheet), */\
   { 0xc9e0, "fs2_ehs"},    /* Slew Rate DS2 IO cell, clk or data mode range (see IIC3V3 IO cell datasheet), */\
   { 0xc9f0, "hs_mode"},    /* I2C high speed mode control                       , */\
   { 0xca00, "enbl_anamux1"},    /* Enable anamux1                                    , */\
   { 0xca10, "enbl_anamux2"},    /* Enable anamux2                                    , */\
   { 0xca20, "enbl_anamux3"},    /* Enable anamux3                                    , */\
   { 0xca30, "enbl_anamux4"},    /* Enable anamux4                                    , */\
   { 0xca74, "anamux1"},    /* Anamux selection control - anamux on TEST1        , */\
   { 0xcb04, "anamux2"},    /* Anamux selection control - anamux on TEST2        , */\
   { 0xcb54, "anamux3"},    /* Anamux selection control - anamux on TEST3        , */\
   { 0xcba4, "anamux4"},    /* Anamux selection control - anamux on TEST4        , */\
   { 0xcc05, "pll_seli_lbw"},    /* PLL SELI - Low B/W PLL control mode or I2C direct PLL control mode only, */\
   { 0xcc64, "pll_selp_lbw"},    /* PLL SELP - Low B/W PLL control mode or I2C direct PLL control mode only, */\
   { 0xccb3, "pll_selr_lbw"},    /* PLL SELR - Low B/W PLL control mode or I2C direct PLL control mode only, */\
   { 0xccf0, "sel_user_pll_bw"},    /* PLL Low Bandwidth Mode control                    , */\
   { 0xcdf0, "pll_frm"},    /* PLL free running mode control; 1 in TCB direct control mode, else this control bit, */\
   { 0xce09, "pll_ndec"},    /* PLL NDEC - I2C direct PLL control mode only       , */\
   { 0xcea0, "pll_mdec_msb"},    /* MSB of pll_mdec - I2C direct PLL control mode only, */\
   { 0xceb0, "enbl_pll"},    /* Enables PLL in I2C direct PLL control mode only   , */\
   { 0xcec0, "enbl_fro8"},    /* Enables FRO8M in I2C direct control mode only     , */\
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
   { 0xd580, "enbl_clk_range_chk"},    /* Clock out of range                                , */\
   { 0xd601, "clkdiv_dsp_sel"},    /* DSP clock divider selection in direct clock control mode, */\
   { 0xd621, "clkdiv_audio_sel"},    /* Audio clock divider selection in direct clock control mode, */\
   { 0xd641, "clkdiv_muxa_sel"},    /* DCDC MUXA clock divider selection in direct clock control mode, */\
   { 0xd661, "clkdiv_muxb_sel"},    /* DCDC MUXB clock divider selection in direct clock control mode, */\
   { 0xd681, "dsp_tap_clk"},    /* Dsp clock frequency selection in TAP mode;        , */\
   { 0xd6a1, "sel_wdt_clk"},    /* Watch dog clock post divider value                , */\
   { 0xd6c1, "sel_tim_clk"},    /* Timer clock post divider value                    , */\
   { 0xd700, "ads1_ehs"},    /* Slew Rate ADS1 IO cell, clk or data mode range (see IIC3V3 IO cell datasheet), */\
   { 0xd710, "ads2_ehs"},    /* Slew Rate ADS2 IO cell, clk or data mode range (see IIC3V3 IO cell datasheet), */\
   { 0xd822, "test_parametric_io"},    /* test io parametric                                , */\
   { 0xd850, "ctrl_bst_clk_lp1"},    /* boost clock control in low power mode1            , */\
   { 0xd861, "test_spare_out1"},    /* test spare out 1                                  , */\
   { 0xd880, "bst_dcmbst"},    /* dcm boost  - I2C direct mode                      , */\
   { 0xd8a1, "force_pga_clock"},    /* force pga clock                                   , */\
   { 0xd8c3, "test_spare_out2"},    /* test spare out 1                                  , */\
   { 0xd900, "overrules_usercase"},    /* Overrule Mode control use                         , */\
   { 0xd910, "ovr_switch_ref"},    /* Overrule Value                                    , */\
   { 0xd920, "ovr_enbl_pll"},    /* Overrule Value                                    , */\
   { 0xd930, "ovr_switch_amp"},    /* Overrule Value                                    , */\
   { 0xd940, "ovr_enbl_clk_cs"},    /* Overrule Value                                    , */\
   { 0xd951, "ovr_sel_clk_cs"},    /* CS clock selection overrule                       , */\
   { 0xd970, "ovr_switch_cs"},    /* Overrule Value                                    , */\
   { 0xd980, "ovr_enbl_csvs_ss"},    /* Overrule Value                                    , */\
   { 0xd990, "ovr_enbl_comp"},    /* Overrule Value                                    , */\
   { 0xed00, "enbl_fro8cal"},    /* Enable FRO  calibration                           , */\
   { 0xed10, "start_fro8_calibration"},    /* Start FRO8 Calibration                            , */\
   { 0xed20, "fro8_calibration_done"},    /* FRO8 Calibration done - Read Only                 , */\
   { 0xed45, "fro8_auto_trim_val"},    /* Calibration value from Auto Calibration block, to be written into MTP - Read Only, */\
   { 0xee0f, "sw_profile"},    /* Software profile data                             , */\
   { 0xef0f, "sw_vstep"},    /* Software vstep information                        , */\
   { 0xf000, "calibration_onetime"},    /* Calibration schedule                              , */\
   { 0xf010, "calibr_ron_done"},    /* Calibration Ron executed                          , */\
   { 0xf020, "calibr_dcdc_api_calibrate"},    /* Calibration current limit DCDC                    , */\
   { 0xf030, "calibr_dcdc_delta_sign"},    /* Sign bit for delta calibration current limit DCDC , */\
   { 0xf042, "calibr_dcdc_delta"},    /* Calibration delta current limit DCDC              , */\
   { 0xf078, "calibr_speaker_info"},    /* Reserved space for allowing customer to store speaker information, */\
   { 0xf105, "calibr_vout_offset"},    /* DCDC offset calibration 2's complement (key1 protected), */\
   { 0xf163, "spare_mtp1_9_6"},    /* HW gain module - left channel (2's complement)    , */\
   { 0xf1a5, "spare_mtp1_15_10"},    /* Offset for amplifier, HW gain module - left channel (2's complement), */\
   { 0xf203, "calibr_gain"},    /* HW gain module  (2's complement)                  , */\
   { 0xf245, "calibr_offset"},    /* Offset for amplifier, HW gain module (2's complement), */\
   { 0xf2a3, "spare_mtp2_13_10"},    /* Trimming of LDO (2.7V)                            , */\
   { 0xf307, "spare_mtp3_7_0"},    /* SPARE                                             , */\
   { 0xf387, "calibr_gain_cs"},    /* Current sense gain (signed two's complement format), */\
   { 0xf40f, "calibr_R25C"},    /* Ron resistance of  speaker coil                   , */\
   { 0xf50f, "spare_mtp5_15_0"},    /* SPARE                                             , */\
   { 0xf600, "mtp_lock_enbl_coolflux"},    /* Disable function dcdcoff_mode                     , */\
   { 0xf610, "mtp_pwm_delay_enbl_clk_auto_gating"},    /* Auto clock gating on pwm_delay                    , */\
   { 0xf620, "mtp_ocp_enbl_clk_auto_gating"},    /* Auto clock gating on module ocp                   , */\
   { 0xf630, "mtp_disable_clk_a_gating"},    /* Disable clock_a gating                            , */\
   { 0xf642, "spare_mtp6_6_3"},    /* SPARE                                             , */\
   { 0xf686, "spare_mtp6_14_8"},    /* Offset of left amplifier level shifter B          , */\
   { 0xf706, "ctrl_offset_a"},    /* Offset of  level shifter A                        , */\
   { 0xf786, "ctrl_offset_b"},    /* Offset of  amplifier level shifter B              , */\
   { 0xf806, "htol_iic_addr"},    /* 7-bit I2C address to be used during HTOL testing  , */\
   { 0xf870, "htol_iic_addr_en"},    /* HTOL I2C address enable control                   , */\
   { 0xf884, "calibr_temp_offset"},    /* Temperature offset 2's compliment (key1 protected), */\
   { 0xf8d2, "calibr_temp_gain"},    /* Temperature gain 2's compliment (key1 protected)  , */\
   { 0xf910, "disable_sam_mode"},    /* Disable sam mode                                  , */\
   { 0xf920, "mtp_lock_bypass_clipper"},    /* Disable function bypass_clipper                   , */\
   { 0xf930, "mtp_lock_max_dcdc_voltage"},    /* Disable programming of max dcdc boost voltage     , */\
   { 0xf943, "calibr_vbg_trim"},    /* Bandgap trimming control                          , */\
   { 0xf987, "type_bits_fw"},    /* MTP-control FW - See Firmware I2C API document for details, */\
   { 0xfa0f, "mtpdataA"},    /* MTPdataA (key1 protected)                         , */\
   { 0xfb0f, "mtpdataB"},    /* MTPdataB (key1 protected)                         , */\
   { 0xfc0f, "mtpdataC"},    /* MTPdataC (key1 protected)                         , */\
   { 0xfd0f, "mtpdataD"},    /* MTPdataD (key1 protected)                         , */\
   { 0xfe0f, "mtpdataE"},    /* MTPdataE (key1 protected)                         , */\
   { 0xff05, "fro8_trim"},    /* 8 MHz oscillator trim code                        , */\
   { 0xff61, "fro8_short_nwell_r"},    /* Short 4 or 6 n-well resistors                     , */\
   { 0xff81, "fro8_boost_i"},    /* Self bias current selection                       , */\
   { 0xffff,"Unknown bitfield enum" }    /* not found */\
};

enum tfa9912_irq {
	tfa9912_irq_stvdds = 0,
	tfa9912_irq_stplls = 1,
	tfa9912_irq_stotds = 2,
	tfa9912_irq_stovds = 3,
	tfa9912_irq_stuvds = 4,
	tfa9912_irq_stclks = 5,
	tfa9912_irq_stmtpb = 6,
	tfa9912_irq_stnoclk = 7,
	tfa9912_irq_stspks = 8,
	tfa9912_irq_stacs = 9,
	tfa9912_irq_stsws = 10,
	tfa9912_irq_stwds = 11,
	tfa9912_irq_stamps = 12,
	tfa9912_irq_starefs = 13,
	tfa9912_irq_stadccr = 14,
	tfa9912_irq_stbodnok = 15,
	tfa9912_irq_stbstcu = 16,
	tfa9912_irq_stbsthi = 17,
	tfa9912_irq_stbstoc = 18,
	tfa9912_irq_stbstpkcur = 19,
	tfa9912_irq_stbstvc = 20,
	tfa9912_irq_stbst86 = 21,
	tfa9912_irq_stbst93 = 22,
	tfa9912_irq_strcvld = 23,
	tfa9912_irq_stocpl = 24,
	tfa9912_irq_stocpr = 25,
	tfa9912_irq_stmwsrc = 26,
	tfa9912_irq_stmwcfc = 27,
	tfa9912_irq_stmwsmu = 28,
	tfa9912_irq_stcfmer = 29,
	tfa9912_irq_stcfmac = 30,
	tfa9912_irq_stclkoor = 31,
	tfa9912_irq_sttdmer = 32,
	tfa9912_irq_stclpl = 33,
	tfa9912_irq_stclpr = 34,
	tfa9912_irq_stocpm = 35,
	tfa9912_irq_stlp1 = 37,
	tfa9912_irq_stla = 38,
	tfa9912_irq_stvddp = 39,
	tfa9912_irq_sttapdet = 40,
	tfa9912_irq_staudmod = 41,
	tfa9912_irq_stsammod = 42,
	tfa9912_irq_sttapmod = 43,
	tfa9912_irq_sttaptrg = 44,
	tfa9912_irq_max = 45,
	tfa9912_irq_all = -1 /* all irqs */};

#define TFA9912_IRQ_NAMETABLE static tfaIrqName_t Tfa9912IrqNames[]= {\
	{ 0, "STVDDS"},\
	{ 1, "STPLLS"},\
	{ 2, "STOTDS"},\
	{ 3, "STOVDS"},\
	{ 4, "STUVDS"},\
	{ 5, "STCLKS"},\
	{ 6, "STMTPB"},\
	{ 7, "STNOCLK"},\
	{ 8, "STSPKS"},\
	{ 9, "STACS"},\
	{ 10, "STSWS"},\
	{ 11, "STWDS"},\
	{ 12, "STAMPS"},\
	{ 13, "STAREFS"},\
	{ 14, "STADCCR"},\
	{ 15, "STBODNOK"},\
	{ 16, "STBSTCU"},\
	{ 17, "STBSTHI"},\
	{ 18, "STBSTOC"},\
	{ 19, "STBSTPKCUR"},\
	{ 20, "STBSTVC"},\
	{ 21, "STBST86"},\
	{ 22, "STBST93"},\
	{ 23, "STRCVLD"},\
	{ 24, "STOCPL"},\
	{ 25, "STOCPR"},\
	{ 26, "STMWSRC"},\
	{ 27, "STMWCFC"},\
	{ 28, "STMWSMU"},\
	{ 29, "STCFMER"},\
	{ 30, "STCFMAC"},\
	{ 31, "STCLKOOR"},\
	{ 32, "STTDMER"},\
	{ 33, "STCLPL"},\
	{ 34, "STCLPR"},\
	{ 35, "STOCPM"},\
	{ 36, "36"},\
	{ 37, "STLP1"},\
	{ 38, "STLA"},\
	{ 39, "STVDDP"},\
	{ 40, "STTAPDET"},\
	{ 41, "STAUDMOD"},\
	{ 42, "STSAMMOD"},\
	{ 43, "STTAPMOD"},\
	{ 44, "STTAPTRG"},\
	{ 45, "45"},\
};
#endif /* _TFA9912_TFAFIELDNAMES_H */
