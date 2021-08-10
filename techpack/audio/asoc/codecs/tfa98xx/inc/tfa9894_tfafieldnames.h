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


/** Filename: tfa9894_tfaFieldnames_N1Last.h
 *  This file was generated automatically on 09/28/18 at 11:24:56. 
 *  Source file: TFA9894_N1A1_I2C_RegisterMap.xlsx
 */

#ifndef _TFA9894_TFAFIELDNAMES_H
#define _TFA9894_TFAFIELDNAMES_H


#define TFA9894_I2CVERSION    17.0

typedef enum Tfa9894BfEnumList {
    TFA9894_BF_PWDN  = 0x0000,    /*!< Powerdown control                                  */
    TFA9894_BF_I2CR  = 0x0010,    /*!< I2C Reset - Auto clear                             */
    TFA9894_BF_CFE   = 0x0020,    /*!< Enable CoolFlux DSP                                */
    TFA9894_BF_AMPE  = 0x0030,    /*!< Enable Amplifier                                   */
    TFA9894_BF_DCA   = 0x0040,    /*!< Enable DCDC Boost converter                        */
    TFA9894_BF_SBSL  = 0x0050,    /*!< Coolflux configured                                */
    TFA9894_BF_AMPC  = 0x0060,    /*!< CoolFlux control over amplifier                    */
    TFA9894_BF_INTP  = 0x0071,    /*!< Interrupt config                                   */
    TFA9894_BF_FSSSEL= 0x0090,    /*!< Audio sample reference                             */
    TFA9894_BF_BYPOCP= 0x00a0,    /*!< Bypass OCP                                         */
    TFA9894_BF_TSTOCP= 0x00b0,    /*!< OCP testing control                                */
    TFA9894_BF_BSSS  = 0x00c0,    /*!< Vbat protection steepness                          */
    TFA9894_BF_HPFBYP= 0x00d0,    /*!< Bypass High Pass Filter                            */
    TFA9894_BF_DPSA  = 0x00e0,    /*!< Enable DPSA                                        */
    TFA9894_BF_AMPINSEL= 0x0101,    /*!< Amplifier input selection                          */
    TFA9894_BF_MANSCONF= 0x0120,    /*!< Device I2C settings configured                     */
    TFA9894_BF_MANCOLD= 0x0130,    /*!< Execute cold start                                 */
    TFA9894_BF_MANROBOD= 0x0140,    /*!< Reaction on BOD                                    */
    TFA9894_BF_BODE  = 0x0150,    /*!< Enable BOD (only in direct control mode)           */
    TFA9894_BF_BODHYS= 0x0160,    /*!< Enable Hysteresis of BOD                           */
    TFA9894_BF_BODFILT= 0x0171,    /*!< BOD filter                                         */
    TFA9894_BF_BODTHLVL= 0x0191,    /*!< BOD threshold                                      */
    TFA9894_BF_MUTETO= 0x01b0,    /*!< Time out SB mute sequence                          */
    TFA9894_BF_MANWDE= 0x01c0,    /*!< Watchdog enable                                    */
    TFA9894_BF_OPENMTP= 0x01e0,    /*!< Control for FAIM protection                        */
    TFA9894_BF_FAIMVBGOVRRL= 0x01f0,    /*!< Overrule the enabling of VBG for faim erase/write access */
    TFA9894_BF_AUDFS = 0x0203,    /*!< Audio sample rate Fs                               */
    TFA9894_BF_INPLEV= 0x0240,    /*!< TDM output attenuation                             */
    TFA9894_BF_FRACTDEL= 0x0255,    /*!< Current sense fractional delay                     */
    TFA9894_BF_TDMPRES= 0x02b1,    /*!< Control for HW manager                             */
    TFA9894_BF_AMPOCRT= 0x02d2,    /*!< Amplifier on-off criteria for shutdown             */
    TFA9894_BF_REV   = 0x030f,    /*!< Revision info                                      */
    TFA9894_BF_REFCKEXT= 0x0401,    /*!< PLL external reference clock                       */
    TFA9894_BF_REFCKSEL= 0x0420,    /*!< PLL internal reference clock                       */
    TFA9894_BF_MCLKSEL= 0x0432,    /*!< Master Clock Selection                             */
    TFA9894_BF_MANAOOSC= 0x0460,    /*!< Internal OSC1M off at PWDN                         */
    TFA9894_BF_ACKCLDDIS= 0x0470,    /*!< Automatic PLL reference clock selection for cold start */
    TFA9894_BF_SPKSSEN= 0x0510,    /*!< Enable speaker sub-system                          */
    TFA9894_BF_MTPSSEN= 0x0520,    /*!< Enable FAIM sub-system                             */
    TFA9894_BF_WDTCLKEN= 0x0530,    /*!< Enable Coolflux watchdog clock                     */
    TFA9894_BF_VDDS  = 0x1000,    /*!< POR                                                */
    TFA9894_BF_PLLS  = 0x1010,    /*!< PLL Lock                                           */
    TFA9894_BF_OTDS  = 0x1020,    /*!< OTP alarm                                          */
    TFA9894_BF_OVDS  = 0x1030,    /*!< OVP alarm                                          */
    TFA9894_BF_UVDS  = 0x1040,    /*!< UVP alarm                                          */
    TFA9894_BF_OCDS  = 0x1050,    /*!< OCP amplifier (sticky register, clear on read)     */
    TFA9894_BF_CLKS  = 0x1060,    /*!< Clocks stable                                      */
    TFA9894_BF_MTPB  = 0x1070,    /*!< MTP busy                                           */
    TFA9894_BF_NOCLK = 0x1080,    /*!< Lost clock                                         */
    TFA9894_BF_ACS   = 0x1090,    /*!< Cold Start                                         */
    TFA9894_BF_WDS   = 0x10a0,    /*!< Watchdog                                           */
    TFA9894_BF_SWS   = 0x10b0,    /*!< Amplifier engage                                   */
    TFA9894_BF_AMPS  = 0x10c0,    /*!< Amplifier enable                                   */
    TFA9894_BF_AREFS = 0x10d0,    /*!< References enable                                  */
    TFA9894_BF_ADCCR = 0x10e0,    /*!< Control ADC                                        */
    TFA9894_BF_BODNOK= 0x10f0,    /*!< BOD Flag - VDD NOT OK                              */
    TFA9894_BF_DCIL  = 0x1100,    /*!< DCDC current limiting                              */
    TFA9894_BF_DCDCA = 0x1110,    /*!< DCDC active (sticky register, clear on read)       */
    TFA9894_BF_DCOCPOK= 0x1120,    /*!< DCDC OCP nmos (sticky register, clear on read)     */
    TFA9894_BF_DCHVBAT= 0x1140,    /*!< DCDC level 1x                                      */
    TFA9894_BF_DCH114= 0x1150,    /*!< DCDC level 1.14x                                   */
    TFA9894_BF_DCH107= 0x1160,    /*!< DCDC level 1.07x                                   */
    TFA9894_BF_SPKS  = 0x1170,    /*!< Speaker status                                     */
    TFA9894_BF_CLKOOR= 0x1180,    /*!< External clock status                              */
    TFA9894_BF_MANALARM= 0x1190,    /*!< Alarm state                                        */
    TFA9894_BF_TDMERR= 0x11a0,    /*!< TDM error                                          */
    TFA9894_BF_TDMLUTER= 0x11b0,    /*!< TDM lookup table error                             */
    TFA9894_BF_OCPOAP= 0x1200,    /*!< OCPOK pmos A                                       */
    TFA9894_BF_OCPOAN= 0x1210,    /*!< OCPOK nmos A                                       */
    TFA9894_BF_OCPOBP= 0x1220,    /*!< OCPOK pmos B                                       */
    TFA9894_BF_OCPOBN= 0x1230,    /*!< OCPOK nmos B                                       */
    TFA9894_BF_CLIPS = 0x1240,    /*!< Amplifier clipping                                 */
    TFA9894_BF_MANMUTE= 0x1250,    /*!< Audio mute sequence                                */
    TFA9894_BF_MANOPER= 0x1260,    /*!< Device in Operating state                          */
    TFA9894_BF_LP1   = 0x1270,    /*!< Low power MODE1 detection                          */
    TFA9894_BF_LA    = 0x1280,    /*!< Low amplitude detection                            */
    TFA9894_BF_VDDPH = 0x1290,    /*!< VDDP greater than VBAT flag                        */
    TFA9894_BF_TDMSTAT= 0x1402,    /*!< TDM Status bits                                    */
    TFA9894_BF_MANSTATE= 0x1433,    /*!< Device Manager status                              */
    TFA9894_BF_DCMODE= 0x14b1,    /*!< DCDC mode status bits                              */
    TFA9894_BF_BATS  = 0x1509,    /*!< Battery voltage (V)                                */
    TFA9894_BF_TEMPS = 0x1608,    /*!< IC Temperature (C)                                 */
    TFA9894_BF_VDDPS = 0x1709,    /*!< IC VDDP voltage (1023*VDDP/13V)                    */
    TFA9894_BF_TDME  = 0x2000,    /*!< Enable interface                                   */
    TFA9894_BF_TDMSPKE= 0x2010,    /*!< Control audio tdm channel in sink0                 */
    TFA9894_BF_TDMDCE= 0x2020,    /*!< Control audio tdm channel in sink1                 */
    TFA9894_BF_TDMCSE= 0x2030,    /*!< Source 0 enable                                    */
    TFA9894_BF_TDMVSE= 0x2040,    /*!< Source 1 enable                                    */
    TFA9894_BF_TDMCFE= 0x2050,    /*!< Source 2 enable                                    */
    TFA9894_BF_TDMCF2E= 0x2060,    /*!< Source 3 enable                                    */
    TFA9894_BF_TDMCLINV= 0x2070,    /*!< Reception data to BCK clock                        */
    TFA9894_BF_TDMFSPOL= 0x2080,    /*!< FS polarity                                        */
    TFA9894_BF_TDMDEL= 0x2090,    /*!< Data delay to FS                                   */
    TFA9894_BF_TDMADJ= 0x20a0,    /*!< Data adjustment                                    */
    TFA9894_BF_TDMOOMP= 0x20b1,    /*!< Received audio compression                         */
    TFA9894_BF_TDMNBCK= 0x2103,    /*!< TDM NBCK - Bit clock to FS ratio                   */
    TFA9894_BF_TDMFSLN= 0x2143,    /*!< FS length (master mode only)                       */
    TFA9894_BF_TDMSLOTS= 0x2183,    /*!< N-slots in Frame                                   */
    TFA9894_BF_TDMTXDFO= 0x21c1,    /*!< Format unused bits                                 */
    TFA9894_BF_TDMTXUS0= 0x21e1,    /*!< Format unused slots DATAO                          */
    TFA9894_BF_TDMSLLN= 0x2204,    /*!< N-bits in slot                                     */
    TFA9894_BF_TDMBRMG= 0x2254,    /*!< N-bits remaining                                   */
    TFA9894_BF_TDMSSIZE= 0x22a4,    /*!< Sample size per slot                               */
    TFA9894_BF_TDMSPKS= 0x2303,    /*!< TDM slot for sink 0                                */
    TFA9894_BF_TDMDCS= 0x2343,    /*!< TDM slot for sink 1                                */
    TFA9894_BF_TDMCFSEL= 0x2381,    /*!< TDM Source 2 data selection                        */
    TFA9894_BF_TDMCF2SEL= 0x23a1,    /*!< TDM Source 3 data selection                        */
    TFA9894_BF_TDMCSS= 0x2403,    /*!< Slot Position of source 0 data                     */
    TFA9894_BF_TDMVSS= 0x2443,    /*!< Slot Position of source 1 data                     */
    TFA9894_BF_TDMCFS= 0x2483,    /*!< Slot Position of source 2 data                     */
    TFA9894_BF_TDMCF2S= 0x24c3,    /*!< Slot Position of source 3 data                     */
    TFA9894_BF_ISTVDDS= 0x4000,    /*!< Status POR                                         */
    TFA9894_BF_ISTBSTOC= 0x4010,    /*!< Status DCDC OCP                                    */
    TFA9894_BF_ISTOTDS= 0x4020,    /*!< Status OTP alarm                                   */
    TFA9894_BF_ISTOCPR= 0x4030,    /*!< Status OCP alarm                                   */
    TFA9894_BF_ISTUVDS= 0x4040,    /*!< Status UVP alarm                                   */
    TFA9894_BF_ISTMANALARM= 0x4050,    /*!< Status manager alarm state                         */
    TFA9894_BF_ISTTDMER= 0x4060,    /*!< Status TDM error                                   */
    TFA9894_BF_ISTNOCLK= 0x4070,    /*!< Status lost clock                                  */
    TFA9894_BF_ISTCFMER= 0x4080,    /*!< Status cfma error                                  */
    TFA9894_BF_ISTCFMAC= 0x4090,    /*!< Status cfma ack                                    */
    TFA9894_BF_ISTSPKS= 0x40a0,    /*!< Status coolflux speaker error                      */
    TFA9894_BF_ISTACS= 0x40b0,    /*!< Status cold started                                */
    TFA9894_BF_ISTWDS= 0x40c0,    /*!< Status watchdog reset                              */
    TFA9894_BF_ISTBODNOK= 0x40d0,    /*!< Status brown out detect                            */
    TFA9894_BF_ISTLP1= 0x40e0,    /*!< Status low power mode1 detect                      */
    TFA9894_BF_ISTCLKOOR= 0x40f0,    /*!< Status clock out of range                          */
    TFA9894_BF_ICLVDDS= 0x4400,    /*!< Clear POR                                          */
    TFA9894_BF_ICLBSTOC= 0x4410,    /*!< Clear DCDC OCP                                     */
    TFA9894_BF_ICLOTDS= 0x4420,    /*!< Clear OTP alarm                                    */
    TFA9894_BF_ICLOCPR= 0x4430,    /*!< Clear OCP alarm                                    */
    TFA9894_BF_ICLUVDS= 0x4440,    /*!< Clear UVP alarm                                    */
    TFA9894_BF_ICLMANALARM= 0x4450,    /*!< Clear manager alarm state                          */
    TFA9894_BF_ICLTDMER= 0x4460,    /*!< Clear TDM error                                    */
    TFA9894_BF_ICLNOCLK= 0x4470,    /*!< Clear lost clk                                     */
    TFA9894_BF_ICLCFMER= 0x4480,    /*!< Clear cfma err                                     */
    TFA9894_BF_ICLCFMAC= 0x4490,    /*!< Clear cfma ack                                     */
    TFA9894_BF_ICLSPKS= 0x44a0,    /*!< Clear coolflux speaker error                       */
    TFA9894_BF_ICLACS= 0x44b0,    /*!< Clear cold started                                 */
    TFA9894_BF_ICLWDS= 0x44c0,    /*!< Clear watchdog reset                               */
    TFA9894_BF_ICLBODNOK= 0x44d0,    /*!< Clear brown out detect                             */
    TFA9894_BF_ICLLP1= 0x44e0,    /*!< Clear low power mode1 detect                       */
    TFA9894_BF_ICLCLKOOR= 0x44f0,    /*!< Clear clock out of range                           */
    TFA9894_BF_IEVDDS= 0x4800,    /*!< Enable POR                                         */
    TFA9894_BF_IEBSTOC= 0x4810,    /*!< Enable DCDC OCP                                    */
    TFA9894_BF_IEOTDS= 0x4820,    /*!< Enable OTP alarm                                   */
    TFA9894_BF_IEOCPR= 0x4830,    /*!< Enable OCP alarm                                   */
    TFA9894_BF_IEUVDS= 0x4840,    /*!< Enable UVP alarm                                   */
    TFA9894_BF_IEMANALARM= 0x4850,    /*!< Enable Manager Alarm state                         */
    TFA9894_BF_IETDMER= 0x4860,    /*!< Enable TDM error                                   */
    TFA9894_BF_IENOCLK= 0x4870,    /*!< Enable lost clk                                    */
    TFA9894_BF_IECFMER= 0x4880,    /*!< Enable cfma err                                    */
    TFA9894_BF_IECFMAC= 0x4890,    /*!< Enable cfma ack                                    */
    TFA9894_BF_IESPKS= 0x48a0,    /*!< Enable coolflux speaker error                      */
    TFA9894_BF_IEACS = 0x48b0,    /*!< Enable cold started                                */
    TFA9894_BF_IEWDS = 0x48c0,    /*!< Enable watchdog reset                              */
    TFA9894_BF_IEBODNOK= 0x48d0,    /*!< Enable brown out detect                            */
    TFA9894_BF_IELP1 = 0x48e0,    /*!< Enable low power mode1 detect                      */
    TFA9894_BF_IECLKOOR= 0x48f0,    /*!< Enable clock out of range                          */
    TFA9894_BF_IPOVDDS= 0x4c00,    /*!< Polarity POR                                       */
    TFA9894_BF_IPOBSTOC= 0x4c10,    /*!< Polarity DCDC OCP                                  */
    TFA9894_BF_IPOOTDS= 0x4c20,    /*!< Polarity OTP alarm                                 */
    TFA9894_BF_IPOOCPR= 0x4c30,    /*!< Polarity ocp alarm                                 */
    TFA9894_BF_IPOUVDS= 0x4c40,    /*!< Polarity UVP alarm                                 */
    TFA9894_BF_IPOMANALARM= 0x4c50,    /*!< Polarity manager alarm state                       */
    TFA9894_BF_IPOTDMER= 0x4c60,    /*!< Polarity TDM error                                 */
    TFA9894_BF_IPONOCLK= 0x4c70,    /*!< Polarity lost clk                                  */
    TFA9894_BF_IPOCFMER= 0x4c80,    /*!< Polarity cfma err                                  */
    TFA9894_BF_IPOCFMAC= 0x4c90,    /*!< Polarity cfma ack                                  */
    TFA9894_BF_IPOSPKS= 0x4ca0,    /*!< Polarity coolflux speaker error                    */
    TFA9894_BF_IPOACS= 0x4cb0,    /*!< Polarity cold started                              */
    TFA9894_BF_IPOWDS= 0x4cc0,    /*!< Polarity watchdog reset                            */
    TFA9894_BF_IPOBODNOK= 0x4cd0,    /*!< Polarity brown out detect                          */
    TFA9894_BF_IPOLP1= 0x4ce0,    /*!< Polarity low power mode1 detect                    */
    TFA9894_BF_IPOCLKOOR= 0x4cf0,    /*!< Polarity clock out of range                        */
    TFA9894_BF_BSSCR = 0x5001,    /*!< Battery safeguard attack time                      */
    TFA9894_BF_BSST  = 0x5023,    /*!< Battery safeguard threshold voltage level          */
    TFA9894_BF_BSSRL = 0x5061,    /*!< Battery safeguard maximum reduction                */
    TFA9894_BF_BSSRR = 0x5082,    /*!< Battery safeguard release time                     */
    TFA9894_BF_BSSHY = 0x50b1,    /*!< Battery Safeguard hysteresis                       */
    TFA9894_BF_BSSR  = 0x50e0,    /*!< Battery voltage read out                           */
    TFA9894_BF_BSSBY = 0x50f0,    /*!< Bypass HW clipper                                  */
    TFA9894_BF_CFSM  = 0x5130,    /*!< Coolflux firmware soft mute control                */
    TFA9894_BF_VOL   = 0x5187,    /*!< CF firmware volume control                         */
    TFA9894_BF_CLIPCTRL= 0x5202,    /*!< Clip control setting                               */
    TFA9894_BF_SLOPEE= 0x5230,    /*!< Enables slope control                              */
    TFA9894_BF_SLOPESET= 0x5240,    /*!< Slope speed setting (binary coded)                 */
    TFA9894_BF_AMPGAIN= 0x5287,    /*!< Amplifier gain                                     */
    TFA9894_BF_TDMDCG= 0x5703,    /*!< Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE) */
    TFA9894_BF_TDMSPKG= 0x5743,    /*!< Total gain depending on INPLEV setting (channel 0) */
    TFA9894_BF_DCINSEL= 0x5781,    /*!< VAMP_OUT2 input selection                          */
    TFA9894_BF_LNMODE= 0x5881,    /*!< Low noise gain mode control                        */
    TFA9894_BF_LPM1MODE= 0x5ac1,    /*!< Low power mode control                             */
    TFA9894_BF_TDMSRCMAP= 0x5d02,    /*!< TDM source mapping                                 */
    TFA9894_BF_TDMSRCAS= 0x5d31,    /*!< Sensed value A                                     */
    TFA9894_BF_TDMSRCBS= 0x5d51,    /*!< Sensed value B                                     */
    TFA9894_BF_TDMSRCACLIP= 0x5d71,    /*!< Clip information (analog /digital) for source0     */
    TFA9894_BF_TDMSRCBCLIP= 0x5d91,    /*!< Clip information (analog /digital) for source1     */
    TFA9894_BF_DELCURCOMP= 0x6102,    /*!< Delay to allign compensation signal with current sense signal */
    TFA9894_BF_SIGCURCOMP= 0x6130,    /*!< Polarity of compensation for current sense         */
    TFA9894_BF_ENCURCOMP= 0x6140,    /*!< Enable current sense compensation                  */
    TFA9894_BF_LVLCLPPWM= 0x6152,    /*!< Set the amount of pwm pulse that may be skipped before clip-flag is triggered */
    TFA9894_BF_DCVOF = 0x7005,    /*!< First Boost Voltage Level                          */
    TFA9894_BF_DCVOS = 0x7065,    /*!< Second Boost Voltage Level                         */
    TFA9894_BF_DCMCC = 0x70c3,    /*!< Max Coil Current                                   */
    TFA9894_BF_DCCV  = 0x7101,    /*!< Slope compensation current, represents LxF (inductance x frequency) value  */
    TFA9894_BF_DCIE  = 0x7120,    /*!< Adaptive boost mode                                */
    TFA9894_BF_DCSR  = 0x7130,    /*!< Soft ramp up/down                                  */
    TFA9894_BF_DCDIS = 0x7140,    /*!< DCDC on/off                                        */
    TFA9894_BF_DCPWM = 0x7150,    /*!< DCDC PWM only mode                                 */
    TFA9894_BF_DCTRACK= 0x7160,    /*!< Boost algorithm selection, effective only when boost_intelligent is set to 1 */
    TFA9894_BF_DCENVSEL= 0x7170,    /*!< Selection of data for adaptive boost algorithm, effective only when boost_intelligent is set to 1 */
    TFA9894_BF_DCTRIP= 0x7204,    /*!< 1st adaptive boost trip levels, effective only when DCIE is set to 1 */
    TFA9894_BF_DCTRIP2= 0x7254,    /*!< 2nd adaptive boost trip levels, effective only when DCIE is set to 1 */
    TFA9894_BF_DCTRIPT= 0x72a4,    /*!< Track adaptive boost trip levels, effective only when boost_intelligent is set to 1 */
    TFA9894_BF_DCTRIPHYSTE= 0x72f0,    /*!< Enable hysteresis on booster trip levels           */
    TFA9894_BF_DCHOLD= 0x7304,    /*!< Hold time for DCDC booster, effective only when boost_intelligent is set to 1 */
    TFA9894_BF_RST   = 0x9000,    /*!< Reset for Coolflux DSP                             */
    TFA9894_BF_DMEM  = 0x9011,    /*!< Target memory for CFMA using I2C interface         */
    TFA9894_BF_AIF   = 0x9030,    /*!< Auto increment                                     */
    TFA9894_BF_CFINT = 0x9040,    /*!< Coolflux Interrupt - auto clear                    */
    TFA9894_BF_CFCGATE= 0x9050,    /*!< Coolflux clock gating disabling control            */
    TFA9894_BF_REQCMD= 0x9080,    /*!< Firmware event request rpc command                 */
    TFA9894_BF_REQRST= 0x9090,    /*!< Firmware event request reset restart               */
    TFA9894_BF_REQMIPS= 0x90a0,    /*!< Firmware event request short on mips               */
    TFA9894_BF_REQMUTED= 0x90b0,    /*!< Firmware event request mute sequence ready         */
    TFA9894_BF_REQVOL= 0x90c0,    /*!< Firmware event request volume ready                */
    TFA9894_BF_REQDMG= 0x90d0,    /*!< Firmware event request speaker damage detected     */
    TFA9894_BF_REQCAL= 0x90e0,    /*!< Firmware event request calibration completed       */
    TFA9894_BF_REQRSV= 0x90f0,    /*!< Firmware event request reserved                    */
    TFA9894_BF_MADD  = 0x910f,    /*!< CF memory address                                  */
    TFA9894_BF_MEMA  = 0x920f,    /*!< Activate memory access                             */
    TFA9894_BF_ERR   = 0x9307,    /*!< CF error flags                                     */
    TFA9894_BF_ACKCMD= 0x9380,    /*!< Firmware event acknowledge rpc command             */
    TFA9894_BF_ACKRST= 0x9390,    /*!< Firmware event acknowledge reset restart           */
    TFA9894_BF_ACKMIPS= 0x93a0,    /*!< Firmware event acknowledge short on mips           */
    TFA9894_BF_ACKMUTED= 0x93b0,    /*!< Firmware event acknowledge mute sequence ready     */
    TFA9894_BF_ACKVOL= 0x93c0,    /*!< Firmware event acknowledge volume ready            */
    TFA9894_BF_ACKDMG= 0x93d0,    /*!< Firmware event acknowledge speaker damage detected */
    TFA9894_BF_ACKCAL= 0x93e0,    /*!< Firmware event acknowledge calibration completed   */
    TFA9894_BF_ACKRSV= 0x93f0,    /*!< Firmware event acknowledge reserved                */
    TFA9894_BF_MTPK  = 0xa107,    /*!< KEY2 to access KEY2 protected registers, customer key */
    TFA9894_BF_KEY1LOCKED= 0xa200,    /*!< Indicates KEY1 is locked                           */
    TFA9894_BF_KEY2LOCKED= 0xa210,    /*!< Indicates KEY2 is locked                           */
    TFA9894_BF_CMTPI = 0xa350,    /*!< Start copying all the data from mtp to I2C mtp registers - auto clear */
    TFA9894_BF_CIMTP = 0xa360,    /*!< Start copying data from I2C mtp registers to mtp - auto clear */
    TFA9894_BF_MTPRDMSB= 0xa50f,    /*!< MSB word of MTP manual read data                   */
    TFA9894_BF_MTPRDLSB= 0xa60f,    /*!< LSB word of MTP manual read data                   */
    TFA9894_BF_EXTTS = 0xb108,    /*!< External temperature (C)                           */
    TFA9894_BF_TROS  = 0xb190,    /*!< Select temp Speaker calibration                    */
    TFA9894_BF_SWPROFIL= 0xe00f,    /*!< Software profile data                              */
    TFA9894_BF_SWVSTEP= 0xe10f,    /*!< Software vstep information                         */
    TFA9894_BF_MTPOTC= 0xf000,    /*!< Calibration schedule                               */
    TFA9894_BF_MTPEX = 0xf010,    /*!< Calibration Ron executed                           */
    TFA9894_BF_DCMCCAPI= 0xf020,    /*!< Calibration current limit DCDC                     */
    TFA9894_BF_DCMCCSB= 0xf030,    /*!< Sign bit for delta calibration current limit DCDC  */
    TFA9894_BF_USERDEF= 0xf042,    /*!< Calibration delta current limit DCDC               */
    TFA9894_BF_CUSTINFO= 0xf078,    /*!< Reserved space for allowing customer to store speaker information */
    TFA9894_BF_R25C  = 0xf50f,    /*!< Ron resistance of speaker coil                     */
} Tfa9894BfEnumList_t;
#define TFA9894_NAMETABLE static tfaBfName_t Tfa9894DatasheetNames[]= {\
   { 0x0, "PWDN"},    /* Powerdown control                                 , */\
   { 0x10, "I2CR"},    /* I2C Reset - Auto clear                            , */\
   { 0x20, "CFE"},    /* Enable CoolFlux DSP                               , */\
   { 0x30, "AMPE"},    /* Enable Amplifier                                  , */\
   { 0x40, "DCA"},    /* Enable DCDC Boost converter                       , */\
   { 0x50, "SBSL"},    /* Coolflux configured                               , */\
   { 0x60, "AMPC"},    /* CoolFlux control over amplifier                   , */\
   { 0x71, "INTP"},    /* Interrupt config                                  , */\
   { 0x90, "FSSSEL"},    /* Audio sample reference                            , */\
   { 0xa0, "BYPOCP"},    /* Bypass OCP                                        , */\
   { 0xb0, "TSTOCP"},    /* OCP testing control                               , */\
   { 0xc0, "BSSS"},    /* Vbat protection steepness                         , */\
   { 0xd0, "HPFBYP"},    /* Bypass High Pass Filter                           , */\
   { 0xe0, "DPSA"},    /* Enable DPSA                                       , */\
   { 0x101, "AMPINSEL"},    /* Amplifier input selection                         , */\
   { 0x120, "MANSCONF"},    /* Device I2C settings configured                    , */\
   { 0x130, "MANCOLD"},    /* Execute cold start                                , */\
   { 0x140, "MANROBOD"},    /* Reaction on BOD                                   , */\
   { 0x150, "BODE"},    /* Enable BOD (only in direct control mode)          , */\
   { 0x160, "BODHYS"},    /* Enable Hysteresis of BOD                          , */\
   { 0x171, "BODFILT"},    /* BOD filter                                        , */\
   { 0x191, "BODTHLVL"},    /* BOD threshold                                     , */\
   { 0x1b0, "MUTETO"},    /* Time out SB mute sequence                         , */\
   { 0x1c0, "MANWDE"},    /* Watchdog enable                                   , */\
   { 0x1e0, "OPENMTP"},    /* Control for FAIM protection                       , */\
   { 0x1f0, "FAIMVBGOVRRL"},    /* Overrule the enabling of VBG for faim erase/write access, */\
   { 0x203, "AUDFS"},    /* Audio sample rate Fs                              , */\
   { 0x240, "INPLEV"},    /* TDM output attenuation                            , */\
   { 0x255, "FRACTDEL"},    /* Current sense fractional delay                    , */\
   { 0x2b1, "TDMPRES"},    /* Control for HW manager                            , */\
   { 0x2d2, "AMPOCRT"},    /* Amplifier on-off criteria for shutdown            , */\
   { 0x30f, "REV"},    /* Revision info                                     , */\
   { 0x401, "REFCKEXT"},    /* PLL external reference clock                      , */\
   { 0x420, "REFCKSEL"},    /* PLL internal reference clock                      , */\
   { 0x432, "MCLKSEL"},    /* Master Clock Selection                            , */\
   { 0x460, "MANAOOSC"},    /* Internal OSC1M off at PWDN                        , */\
   { 0x470, "ACKCLDDIS"},    /* Automatic PLL reference clock selection for cold start, */\
   { 0x510, "SPKSSEN"},    /* Enable speaker sub-system                         , */\
   { 0x520, "MTPSSEN"},    /* Enable FAIM sub-system                            , */\
   { 0x530, "WDTCLKEN"},    /* Enable Coolflux watchdog clock                    , */\
   { 0x1000, "VDDS"},    /* POR                                               , */\
   { 0x1010, "PLLS"},    /* PLL Lock                                          , */\
   { 0x1020, "OTDS"},    /* OTP alarm                                         , */\
   { 0x1030, "OVDS"},    /* OVP alarm                                         , */\
   { 0x1040, "UVDS"},    /* UVP alarm                                         , */\
   { 0x1050, "OCDS"},    /* OCP amplifier (sticky register, clear on read)    , */\
   { 0x1060, "CLKS"},    /* Clocks stable                                     , */\
   { 0x1070, "MTPB"},    /* MTP busy                                          , */\
   { 0x1080, "NOCLK"},    /* Lost clock                                        , */\
   { 0x1090, "ACS"},    /* Cold Start                                        , */\
   { 0x10a0, "WDS"},    /* Watchdog                                          , */\
   { 0x10b0, "SWS"},    /* Amplifier engage                                  , */\
   { 0x10c0, "AMPS"},    /* Amplifier enable                                  , */\
   { 0x10d0, "AREFS"},    /* References enable                                 , */\
   { 0x10e0, "ADCCR"},    /* Control ADC                                       , */\
   { 0x10f0, "BODNOK"},    /* BOD Flag - VDD NOT OK                             , */\
   { 0x1100, "DCIL"},    /* DCDC current limiting                             , */\
   { 0x1110, "DCDCA"},    /* DCDC active (sticky register, clear on read)      , */\
   { 0x1120, "DCOCPOK"},    /* DCDC OCP nmos (sticky register, clear on read)    , */\
   { 0x1140, "DCHVBAT"},    /* DCDC level 1x                                     , */\
   { 0x1150, "DCH114"},    /* DCDC level 1.14x                                  , */\
   { 0x1160, "DCH107"},    /* DCDC level 1.07x                                  , */\
   { 0x1170, "SPKS"},    /* Speaker status                                    , */\
   { 0x1180, "CLKOOR"},    /* External clock status                             , */\
   { 0x1190, "MANALARM"},    /* Alarm state                                       , */\
   { 0x11a0, "TDMERR"},    /* TDM error                                         , */\
   { 0x11b0, "TDMLUTER"},    /* TDM lookup table error                            , */\
   { 0x1200, "OCPOAP"},    /* OCPOK pmos A                                      , */\
   { 0x1210, "OCPOAN"},    /* OCPOK nmos A                                      , */\
   { 0x1220, "OCPOBP"},    /* OCPOK pmos B                                      , */\
   { 0x1230, "OCPOBN"},    /* OCPOK nmos B                                      , */\
   { 0x1240, "CLIPS"},    /* Amplifier clipping                                , */\
   { 0x1250, "MANMUTE"},    /* Audio mute sequence                               , */\
   { 0x1260, "MANOPER"},    /* Device in Operating state                         , */\
   { 0x1270, "LP1"},    /* Low power MODE1 detection                         , */\
   { 0x1280, "LA"},    /* Low amplitude detection                           , */\
   { 0x1290, "VDDPH"},    /* VDDP greater than VBAT flag                       , */\
   { 0x1402, "TDMSTAT"},    /* TDM Status bits                                   , */\
   { 0x1433, "MANSTATE"},    /* Device Manager status                             , */\
   { 0x14b1, "DCMODE"},    /* DCDC mode status bits                             , */\
   { 0x1509, "BATS"},    /* Battery voltage (V)                               , */\
   { 0x1608, "TEMPS"},    /* IC Temperature (C)                                , */\
   { 0x1709, "VDDPS"},    /* IC VDDP voltage (1023*VDDP/13V)                   , */\
   { 0x2000, "TDME"},    /* Enable interface                                  , */\
   { 0x2010, "TDMSPKE"},    /* Control audio tdm channel in sink0                , */\
   { 0x2020, "TDMDCE"},    /* Control audio tdm channel in sink1                , */\
   { 0x2030, "TDMCSE"},    /* Source 0 enable                                   , */\
   { 0x2040, "TDMVSE"},    /* Source 1 enable                                   , */\
   { 0x2050, "TDMCFE"},    /* Source 2 enable                                   , */\
   { 0x2060, "TDMCF2E"},    /* Source 3 enable                                   , */\
   { 0x2070, "TDMCLINV"},    /* Reception data to BCK clock                       , */\
   { 0x2080, "TDMFSPOL"},    /* FS polarity                                       , */\
   { 0x2090, "TDMDEL"},    /* Data delay to FS                                  , */\
   { 0x20a0, "TDMADJ"},    /* Data adjustment                                   , */\
   { 0x20b1, "TDMOOMP"},    /* Received audio compression                        , */\
   { 0x2103, "TDMNBCK"},    /* TDM NBCK - Bit clock to FS ratio                  , */\
   { 0x2143, "TDMFSLN"},    /* FS length (master mode only)                      , */\
   { 0x2183, "TDMSLOTS"},    /* N-slots in Frame                                  , */\
   { 0x21c1, "TDMTXDFO"},    /* Format unused bits                                , */\
   { 0x21e1, "TDMTXUS0"},    /* Format unused slots DATAO                         , */\
   { 0x2204, "TDMSLLN"},    /* N-bits in slot                                    , */\
   { 0x2254, "TDMBRMG"},    /* N-bits remaining                                  , */\
   { 0x22a4, "TDMSSIZE"},    /* Sample size per slot                              , */\
   { 0x2303, "TDMSPKS"},    /* TDM slot for sink 0                               , */\
   { 0x2343, "TDMDCS"},    /* TDM slot for sink 1                               , */\
   { 0x2381, "TDMCFSEL"},    /* TDM Source 2 data selection                       , */\
   { 0x23a1, "TDMCF2SEL"},    /* TDM Source 3 data selection                       , */\
   { 0x2403, "TDMCSS"},    /* Slot Position of source 0 data                    , */\
   { 0x2443, "TDMVSS"},    /* Slot Position of source 1 data                    , */\
   { 0x2483, "TDMCFS"},    /* Slot Position of source 2 data                    , */\
   { 0x24c3, "TDMCF2S"},    /* Slot Position of source 3 data                    , */\
   { 0x4000, "ISTVDDS"},    /* Status POR                                        , */\
   { 0x4010, "ISTBSTOC"},    /* Status DCDC OCP                                   , */\
   { 0x4020, "ISTOTDS"},    /* Status OTP alarm                                  , */\
   { 0x4030, "ISTOCPR"},    /* Status OCP alarm                                  , */\
   { 0x4040, "ISTUVDS"},    /* Status UVP alarm                                  , */\
   { 0x4050, "ISTMANALARM"},    /* Status manager alarm state                        , */\
   { 0x4060, "ISTTDMER"},    /* Status TDM error                                  , */\
   { 0x4070, "ISTNOCLK"},    /* Status lost clock                                 , */\
   { 0x4080, "ISTCFMER"},    /* Status cfma error                                 , */\
   { 0x4090, "ISTCFMAC"},    /* Status cfma ack                                   , */\
   { 0x40a0, "ISTSPKS"},    /* Status coolflux speaker error                     , */\
   { 0x40b0, "ISTACS"},    /* Status cold started                               , */\
   { 0x40c0, "ISTWDS"},    /* Status watchdog reset                             , */\
   { 0x40d0, "ISTBODNOK"},    /* Status brown out detect                           , */\
   { 0x40e0, "ISTLP1"},    /* Status low power mode1 detect                     , */\
   { 0x40f0, "ISTCLKOOR"},    /* Status clock out of range                         , */\
   { 0x4400, "ICLVDDS"},    /* Clear POR                                         , */\
   { 0x4410, "ICLBSTOC"},    /* Clear DCDC OCP                                    , */\
   { 0x4420, "ICLOTDS"},    /* Clear OTP alarm                                   , */\
   { 0x4430, "ICLOCPR"},    /* Clear OCP alarm                                   , */\
   { 0x4440, "ICLUVDS"},    /* Clear UVP alarm                                   , */\
   { 0x4450, "ICLMANALARM"},    /* Clear manager alarm state                         , */\
   { 0x4460, "ICLTDMER"},    /* Clear TDM error                                   , */\
   { 0x4470, "ICLNOCLK"},    /* Clear lost clk                                    , */\
   { 0x4480, "ICLCFMER"},    /* Clear cfma err                                    , */\
   { 0x4490, "ICLCFMAC"},    /* Clear cfma ack                                    , */\
   { 0x44a0, "ICLSPKS"},    /* Clear coolflux speaker error                      , */\
   { 0x44b0, "ICLACS"},    /* Clear cold started                                , */\
   { 0x44c0, "ICLWDS"},    /* Clear watchdog reset                              , */\
   { 0x44d0, "ICLBODNOK"},    /* Clear brown out detect                            , */\
   { 0x44e0, "ICLLP1"},    /* Clear low power mode1 detect                      , */\
   { 0x44f0, "ICLCLKOOR"},    /* Clear clock out of range                          , */\
   { 0x4800, "IEVDDS"},    /* Enable POR                                        , */\
   { 0x4810, "IEBSTOC"},    /* Enable DCDC OCP                                   , */\
   { 0x4820, "IEOTDS"},    /* Enable OTP alarm                                  , */\
   { 0x4830, "IEOCPR"},    /* Enable OCP alarm                                  , */\
   { 0x4840, "IEUVDS"},    /* Enable UVP alarm                                  , */\
   { 0x4850, "IEMANALARM"},    /* Enable Manager Alarm state                        , */\
   { 0x4860, "IETDMER"},    /* Enable TDM error                                  , */\
   { 0x4870, "IENOCLK"},    /* Enable lost clk                                   , */\
   { 0x4880, "IECFMER"},    /* Enable cfma err                                   , */\
   { 0x4890, "IECFMAC"},    /* Enable cfma ack                                   , */\
   { 0x48a0, "IESPKS"},    /* Enable coolflux speaker error                     , */\
   { 0x48b0, "IEACS"},    /* Enable cold started                               , */\
   { 0x48c0, "IEWDS"},    /* Enable watchdog reset                             , */\
   { 0x48d0, "IEBODNOK"},    /* Enable brown out detect                           , */\
   { 0x48e0, "IELP1"},    /* Enable low power mode1 detect                     , */\
   { 0x48f0, "IECLKOOR"},    /* Enable clock out of range                         , */\
   { 0x4c00, "IPOVDDS"},    /* Polarity POR                                      , */\
   { 0x4c10, "IPOBSTOC"},    /* Polarity DCDC OCP                                 , */\
   { 0x4c20, "IPOOTDS"},    /* Polarity OTP alarm                                , */\
   { 0x4c30, "IPOOCPR"},    /* Polarity ocp alarm                                , */\
   { 0x4c40, "IPOUVDS"},    /* Polarity UVP alarm                                , */\
   { 0x4c50, "IPOMANALARM"},    /* Polarity manager alarm state                      , */\
   { 0x4c60, "IPOTDMER"},    /* Polarity TDM error                                , */\
   { 0x4c70, "IPONOCLK"},    /* Polarity lost clk                                 , */\
   { 0x4c80, "IPOCFMER"},    /* Polarity cfma err                                 , */\
   { 0x4c90, "IPOCFMAC"},    /* Polarity cfma ack                                 , */\
   { 0x4ca0, "IPOSPKS"},    /* Polarity coolflux speaker error                   , */\
   { 0x4cb0, "IPOACS"},    /* Polarity cold started                             , */\
   { 0x4cc0, "IPOWDS"},    /* Polarity watchdog reset                           , */\
   { 0x4cd0, "IPOBODNOK"},    /* Polarity brown out detect                         , */\
   { 0x4ce0, "IPOLP1"},    /* Polarity low power mode1 detect                   , */\
   { 0x4cf0, "IPOCLKOOR"},    /* Polarity clock out of range                       , */\
   { 0x5001, "BSSCR"},    /* Battery safeguard attack time                     , */\
   { 0x5023, "BSST"},    /* Battery safeguard threshold voltage level         , */\
   { 0x5061, "BSSRL"},    /* Battery safeguard maximum reduction               , */\
   { 0x5082, "BSSRR"},    /* Battery safeguard release time                    , */\
   { 0x50b1, "BSSHY"},    /* Battery Safeguard hysteresis                      , */\
   { 0x50e0, "BSSR"},    /* Battery voltage read out                          , */\
   { 0x50f0, "BSSBY"},    /* Bypass HW clipper                                 , */\
   { 0x5130, "CFSM"},    /* Coolflux firmware soft mute control               , */\
   { 0x5187, "VOL"},    /* CF firmware volume control                        , */\
   { 0x5202, "CLIPCTRL"},    /* Clip control setting                              , */\
   { 0x5230, "SLOPEE"},    /* Enables slope control                             , */\
   { 0x5240, "SLOPESET"},    /* Slope speed setting (binary coded)                , */\
   { 0x5287, "AMPGAIN"},    /* Amplifier gain                                    , */\
   { 0x5703, "TDMDCG"},    /* Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE), */\
   { 0x5743, "TDMSPKG"},    /* Total gain depending on INPLEV setting (channel 0), */\
   { 0x5781, "DCINSEL"},    /* VAMP_OUT2 input selection                         , */\
   { 0x5881, "LNMODE"},    /* Low noise gain mode control                       , */\
   { 0x5ac1, "LPM1MODE"},    /* Low power mode control                            , */\
   { 0x5d02, "TDMSRCMAP"},    /* TDM source mapping                                , */\
   { 0x5d31, "TDMSRCAS"},    /* Sensed value A                                    , */\
   { 0x5d51, "TDMSRCBS"},    /* Sensed value B                                    , */\
   { 0x5d71, "TDMSRCACLIP"},    /* Clip information (analog /digital) for source0    , */\
   { 0x5d91, "TDMSRCBCLIP"},    /* Clip information (analog /digital) for source1    , */\
   { 0x6102, "DELCURCOMP"},    /* Delay to allign compensation signal with current sense signal, */\
   { 0x6130, "SIGCURCOMP"},    /* Polarity of compensation for current sense        , */\
   { 0x6140, "ENCURCOMP"},    /* Enable current sense compensation                 , */\
   { 0x6152, "LVLCLPPWM"},    /* Set the amount of pwm pulse that may be skipped before clip-flag is triggered, */\
   { 0x7005, "DCVOF"},    /* First Boost Voltage Level                         , */\
   { 0x7065, "DCVOS"},    /* Second Boost Voltage Level                        , */\
   { 0x70c3, "DCMCC"},    /* Max Coil Current                                  , */\
   { 0x7101, "DCCV"},    /* Slope compensation current, represents LxF (inductance x frequency) value , */\
   { 0x7120, "DCIE"},    /* Adaptive boost mode                               , */\
   { 0x7130, "DCSR"},    /* Soft ramp up/down                                 , */\
   { 0x7140, "DCDIS"},    /* DCDC on/off                                       , */\
   { 0x7150, "DCPWM"},    /* DCDC PWM only mode                                , */\
   { 0x7160, "DCTRACK"},    /* Boost algorithm selection, effective only when boost_intelligent is set to 1, */\
   { 0x7170, "DCENVSEL"},    /* Selection of data for adaptive boost algorithm, effective only when boost_intelligent is set to 1, */\
   { 0x7204, "DCTRIP"},    /* 1st adaptive boost trip levels, effective only when DCIE is set to 1, */\
   { 0x7254, "DCTRIP2"},    /* 2nd adaptive boost trip levels, effective only when DCIE is set to 1, */\
   { 0x72a4, "DCTRIPT"},    /* Track adaptive boost trip levels, effective only when boost_intelligent is set to 1, */\
   { 0x72f0, "DCTRIPHYSTE"},    /* Enable hysteresis on booster trip levels          , */\
   { 0x7304, "DCHOLD"},    /* Hold time for DCDC booster, effective only when boost_intelligent is set to 1, */\
   { 0x9000, "RST"},    /* Reset for Coolflux DSP                            , */\
   { 0x9011, "DMEM"},    /* Target memory for CFMA using I2C interface        , */\
   { 0x9030, "AIF"},    /* Auto increment                                    , */\
   { 0x9040, "CFINT"},    /* Coolflux Interrupt - auto clear                   , */\
   { 0x9050, "CFCGATE"},    /* Coolflux clock gating disabling control           , */\
   { 0x9080, "REQCMD"},    /* Firmware event request rpc command                , */\
   { 0x9090, "REQRST"},    /* Firmware event request reset restart              , */\
   { 0x90a0, "REQMIPS"},    /* Firmware event request short on mips              , */\
   { 0x90b0, "REQMUTED"},    /* Firmware event request mute sequence ready        , */\
   { 0x90c0, "REQVOL"},    /* Firmware event request volume ready               , */\
   { 0x90d0, "REQDMG"},    /* Firmware event request speaker damage detected    , */\
   { 0x90e0, "REQCAL"},    /* Firmware event request calibration completed      , */\
   { 0x90f0, "REQRSV"},    /* Firmware event request reserved                   , */\
   { 0x910f, "MADD"},    /* CF memory address                                 , */\
   { 0x920f, "MEMA"},    /* Activate memory access                            , */\
   { 0x9307, "ERR"},    /* CF error flags                                    , */\
   { 0x9380, "ACKCMD"},    /* Firmware event acknowledge rpc command            , */\
   { 0x9390, "ACKRST"},    /* Firmware event acknowledge reset restart          , */\
   { 0x93a0, "ACKMIPS"},    /* Firmware event acknowledge short on mips          , */\
   { 0x93b0, "ACKMUTED"},    /* Firmware event acknowledge mute sequence ready    , */\
   { 0x93c0, "ACKVOL"},    /* Firmware event acknowledge volume ready           , */\
   { 0x93d0, "ACKDMG"},    /* Firmware event acknowledge speaker damage detected, */\
   { 0x93e0, "ACKCAL"},    /* Firmware event acknowledge calibration completed  , */\
   { 0x93f0, "ACKRSV"},    /* Firmware event acknowledge reserved               , */\
   { 0xa107, "MTPK"},    /* KEY2 to access KEY2 protected registers, customer key, */\
   { 0xa200, "KEY1LOCKED"},    /* Indicates KEY1 is locked                          , */\
   { 0xa210, "KEY2LOCKED"},    /* Indicates KEY2 is locked                          , */\
   { 0xa350, "CMTPI"},    /* Start copying all the data from mtp to I2C mtp registers - auto clear, */\
   { 0xa360, "CIMTP"},    /* Start copying data from I2C mtp registers to mtp - auto clear, */\
   { 0xa50f, "MTPRDMSB"},    /* MSB word of MTP manual read data                  , */\
   { 0xa60f, "MTPRDLSB"},    /* LSB word of MTP manual read data                  , */\
   { 0xb108, "EXTTS"},    /* External temperature (C)                          , */\
   { 0xb190, "TROS"},    /* Select temp Speaker calibration                   , */\
   { 0xe00f, "SWPROFIL"},    /* Software profile data                             , */\
   { 0xe10f, "SWVSTEP"},    /* Software vstep information                        , */\
   { 0xf000, "MTPOTC"},    /* Calibration schedule                              , */\
   { 0xf010, "MTPEX"},    /* Calibration Ron executed                          , */\
   { 0xf020, "DCMCCAPI"},    /* Calibration current limit DCDC                    , */\
   { 0xf030, "DCMCCSB"},    /* Sign bit for delta calibration current limit DCDC , */\
   { 0xf042, "USERDEF"},    /* Calibration delta current limit DCDC              , */\
   { 0xf078, "CUSTINFO"},    /* Reserved space for allowing customer to store speaker information, */\
   { 0xf50f, "R25C"},    /* Ron resistance of speaker coil                    , */\
   { 0xffff,"Unknown bitfield enum" }   /* not found */\
};

#define TFA9894_BITNAMETABLE static tfaBfName_t Tfa9894BitNames[]= {\
   { 0x0, "powerdown"},    /* Powerdown control                                 , */\
   { 0x10, "reset"},    /* I2C Reset - Auto clear                            , */\
   { 0x20, "enbl_coolflux"},    /* Enable CoolFlux DSP                               , */\
   { 0x30, "enbl_amplifier"},    /* Enable Amplifier                                  , */\
   { 0x40, "enbl_boost"},    /* Enable DCDC Boost converter                       , */\
   { 0x50, "coolflux_configured"},    /* Coolflux configured                               , */\
   { 0x60, "sel_enbl_amplifier"},    /* CoolFlux control over amplifier                   , */\
   { 0x71, "int_pad_io"},    /* Interrupt config                                  , */\
   { 0x90, "fs_pulse_sel"},    /* Audio sample reference                            , */\
   { 0xa0, "bypass_ocp"},    /* Bypass OCP                                        , */\
   { 0xb0, "test_ocp"},    /* OCP testing control                               , */\
   { 0xc0, "batsense_steepness"},    /* Vbat protection steepness                         , */\
   { 0xd0, "bypass_hp"},    /* Bypass High Pass Filter                           , */\
   { 0xe0, "enbl_dpsa"},    /* Enable DPSA                                       , */\
   { 0xf0, "sel_hysteresis"},    /* Select hysteresis for clock range detector        , */\
   { 0x101, "vamp_sel1"},    /* Amplifier input selection                         , */\
   { 0x120, "src_set_configured"},    /* Device I2C settings configured                    , */\
   { 0x130, "execute_cold_start"},    /* Execute cold start                                , */\
   { 0x140, "man_enbl_brown_out"},    /* Reaction on BOD                                   , */\
   { 0x150, "bod_enbl"},    /* Enable BOD (only in direct control mode)          , */\
   { 0x160, "bod_hyst_enbl"},    /* Enable Hysteresis of BOD                          , */\
   { 0x171, "bod_delay_set"},    /* BOD filter                                        , */\
   { 0x191, "bod_lvl_set"},    /* BOD threshold                                     , */\
   { 0x1b0, "disable_mute_time_out"},    /* Time out SB mute sequence                         , */\
   { 0x1c0, "man_enbl_watchdog"},    /* Watchdog enable                                   , */\
   { 0x1d0, "disable_engage"},    /* Disable Engage                                    , */\
   { 0x1e0, "unprotect_faim"},    /* Control for FAIM protection                       , */\
   { 0x1f0, "faim_enable_vbg"},    /* Overrule the enabling of VBG for faim erase/write access, */\
   { 0x203, "audio_fs"},    /* Audio sample rate Fs                              , */\
   { 0x240, "input_level"},    /* TDM output attenuation                            , */\
   { 0x255, "cs_frac_delay"},    /* Current sense fractional delay                    , */\
   { 0x2b1, "use_tdm_presence"},    /* Control for HW manager                            , */\
   { 0x2d2, "ctrl_on2off_criterion"},    /* Amplifier on-off criteria for shutdown            , */\
   { 0x30f, "device_rev"},    /* Revision info                                     , */\
   { 0x401, "pll_clkin_sel"},    /* PLL external reference clock                      , */\
   { 0x420, "pll_clkin_sel_osc"},    /* PLL internal reference clock                      , */\
   { 0x432, "mclk_sel"},    /* Master Clock Selection                            , */\
   { 0x460, "enbl_osc1m_auto_off"},    /* Internal OSC1M off at PWDN                        , */\
   { 0x470, "disable_auto_sel_refclk"},    /* Automatic PLL reference clock selection for cold start, */\
   { 0x510, "enbl_spkr_ss"},    /* Enable speaker sub-system                         , */\
   { 0x520, "enbl_faim_ss"},    /* Enable FAIM sub-system                            , */\
   { 0x530, "enbl_wdt_clk"},    /* Enable Coolflux watchdog clock                    , */\
   { 0xe07, "ctrl_digtoana"},    /* Spare control from digital to analog              , */\
   { 0xf0f, "hidden_code"},    /* Hidden code to enable access to hidden register. (0x5A6B/23147 default for engineering), */\
   { 0x1000, "flag_por"},    /* POR                                               , */\
   { 0x1010, "flag_pll_lock"},    /* PLL Lock                                          , */\
   { 0x1020, "flag_otpok"},    /* OTP alarm                                         , */\
   { 0x1030, "flag_ovpok"},    /* OVP alarm                                         , */\
   { 0x1040, "flag_uvpok"},    /* UVP alarm                                         , */\
   { 0x1050, "flag_ocp_alarm"},    /* OCP amplifier (sticky register, clear on read)    , */\
   { 0x1060, "flag_clocks_stable"},    /* Clocks stable                                     , */\
   { 0x1070, "flag_mtp_busy"},    /* MTP busy                                          , */\
   { 0x1080, "flag_lost_clk"},    /* Lost clock                                        , */\
   { 0x1090, "flag_cold_started"},    /* Cold Start                                        , */\
   { 0x10a0, "flag_watchdog_reset"},    /* Watchdog                                          , */\
   { 0x10b0, "flag_engage"},    /* Amplifier engage                                  , */\
   { 0x10c0, "flag_enbl_amp"},    /* Amplifier enable                                  , */\
   { 0x10d0, "flag_enbl_ref"},    /* References enable                                 , */\
   { 0x10e0, "flag_adc10_ready"},    /* Control ADC                                       , */\
   { 0x10f0, "flag_bod_vddd_nok"},    /* BOD Flag - VDD NOT OK                             , */\
   { 0x1100, "flag_bst_bstcur"},    /* DCDC current limiting                             , */\
   { 0x1110, "flag_bst_hiz"},    /* DCDC active (sticky register, clear on read)      , */\
   { 0x1120, "flag_bst_ocpok"},    /* DCDC OCP nmos (sticky register, clear on read)    , */\
   { 0x1130, "flag_bst_peakcur"},    /* Indicates current is max in DC-to-DC converter    , */\
   { 0x1140, "flag_bst_voutcomp"},    /* DCDC level 1x                                     , */\
   { 0x1150, "flag_bst_voutcomp86"},    /* DCDC level 1.14x                                  , */\
   { 0x1160, "flag_bst_voutcomp93"},    /* DCDC level 1.07x                                  , */\
   { 0x1170, "flag_cf_speakererror"},    /* Speaker status                                    , */\
   { 0x1180, "flag_clk_out_of_range"},    /* External clock status                             , */\
   { 0x1190, "flag_man_alarm_state"},    /* Alarm state                                       , */\
   { 0x11a0, "flag_tdm_error"},    /* TDM error                                         , */\
   { 0x11b0, "flag_tdm_lut_error"},    /* TDM lookup table error                            , */\
   { 0x1200, "flag_ocpokap"},    /* OCPOK pmos A                                      , */\
   { 0x1210, "flag_ocpokan"},    /* OCPOK nmos A                                      , */\
   { 0x1220, "flag_ocpokbp"},    /* OCPOK pmos B                                      , */\
   { 0x1230, "flag_ocpokbn"},    /* OCPOK nmos B                                      , */\
   { 0x1240, "flag_clip"},    /* Amplifier clipping                                , */\
   { 0x1250, "flag_man_start_mute_audio"},    /* Audio mute sequence                               , */\
   { 0x1260, "flag_man_operating_state"},    /* Device in Operating state                         , */\
   { 0x1270, "flag_lp_detect_mode1"},    /* Low power MODE1 detection                         , */\
   { 0x1280, "flag_low_amplitude"},    /* Low amplitude detection                           , */\
   { 0x1290, "flag_vddp_gt_vbat"},    /* VDDP greater than VBAT flag                       , */\
   { 0x1402, "tdm_status"},    /* TDM Status bits                                   , */\
   { 0x1433, "man_state"},    /* Device Manager status                             , */\
   { 0x1473, "amp_ctrl_state"},    /* Amplifier control status                          , */\
   { 0x14b1, "status_bst_mode"},    /* DCDC mode status bits                             , */\
   { 0x1509, "bat_adc"},    /* Battery voltage (V)                               , */\
   { 0x1608, "temp_adc"},    /* IC Temperature (C)                                , */\
   { 0x1709, "vddp_adc"},    /* IC VDDP voltage (1023*VDDP/13V)                   , */\
   { 0x2000, "tdm_enable"},    /* Enable interface                                  , */\
   { 0x2010, "tdm_sink0_enable"},    /* Control audio tdm channel in sink0                , */\
   { 0x2020, "tdm_sink1_enable"},    /* Control audio tdm channel in sink1                , */\
   { 0x2030, "tdm_source0_enable"},    /* Source 0 enable                                   , */\
   { 0x2040, "tdm_source1_enable"},    /* Source 1 enable                                   , */\
   { 0x2050, "tdm_source2_enable"},    /* Source 2 enable                                   , */\
   { 0x2060, "tdm_source3_enable"},    /* Source 3 enable                                   , */\
   { 0x2070, "tdm_clk_inversion"},    /* Reception data to BCK clock                       , */\
   { 0x2080, "tdm_fs_ws_polarity"},    /* FS polarity                                       , */\
   { 0x2090, "tdm_data_delay"},    /* Data delay to FS                                  , */\
   { 0x20a0, "tdm_data_adjustment"},    /* Data adjustment                                   , */\
   { 0x20b1, "tdm_audio_sample_compression"},    /* Received audio compression                        , */\
   { 0x2103, "tdm_nbck"},    /* TDM NBCK - Bit clock to FS ratio                  , */\
   { 0x2143, "tdm_fs_ws_length"},    /* FS length (master mode only)                      , */\
   { 0x2183, "tdm_nb_of_slots"},    /* N-slots in Frame                                  , */\
   { 0x21c1, "tdm_txdata_format"},    /* Format unused bits                                , */\
   { 0x21e1, "tdm_txdata_format_unused_slot"},    /* Format unused slots DATAO                         , */\
   { 0x2204, "tdm_slot_length"},    /* N-bits in slot                                    , */\
   { 0x2254, "tdm_bits_remaining"},    /* N-bits remaining                                  , */\
   { 0x22a4, "tdm_sample_size"},    /* Sample size per slot                              , */\
   { 0x2303, "tdm_sink0_slot"},    /* TDM slot for sink 0                               , */\
   { 0x2343, "tdm_sink1_slot"},    /* TDM slot for sink 1                               , */\
   { 0x2381, "tdm_source2_sel"},    /* TDM Source 2 data selection                       , */\
   { 0x23a1, "tdm_source3_sel"},    /* TDM Source 3 data selection                       , */\
   { 0x2403, "tdm_source0_slot"},    /* Slot Position of source 0 data                    , */\
   { 0x2443, "tdm_source1_slot"},    /* Slot Position of source 1 data                    , */\
   { 0x2483, "tdm_source2_slot"},    /* Slot Position of source 2 data                    , */\
   { 0x24c3, "tdm_source3_slot"},    /* Slot Position of source 3 data                    , */\
   { 0x4000, "int_out_flag_por"},    /* Status POR                                        , */\
   { 0x4010, "int_out_flag_bst_ocpok"},    /* Status DCDC OCP                                   , */\
   { 0x4020, "int_out_flag_otpok"},    /* Status OTP alarm                                  , */\
   { 0x4030, "int_out_flag_ocp_alarm"},    /* Status OCP alarm                                  , */\
   { 0x4040, "int_out_flag_uvpok"},    /* Status UVP alarm                                  , */\
   { 0x4050, "int_out_flag_man_alarm_state"},    /* Status manager alarm state                        , */\
   { 0x4060, "int_out_flag_tdm_error"},    /* Status TDM error                                  , */\
   { 0x4070, "int_out_flag_lost_clk"},    /* Status lost clock                                 , */\
   { 0x4080, "int_out_flag_cfma_err"},    /* Status cfma error                                 , */\
   { 0x4090, "int_out_flag_cfma_ack"},    /* Status cfma ack                                   , */\
   { 0x40a0, "int_out_flag_cf_speakererror"},    /* Status coolflux speaker error                     , */\
   { 0x40b0, "int_out_flag_cold_started"},    /* Status cold started                               , */\
   { 0x40c0, "int_out_flag_watchdog_reset"},    /* Status watchdog reset                             , */\
   { 0x40d0, "int_out_flag_bod_vddd_nok"},    /* Status brown out detect                           , */\
   { 0x40e0, "int_out_flag_lp_detect_mode1"},    /* Status low power mode1 detect                     , */\
   { 0x40f0, "int_out_flag_clk_out_of_range"},    /* Status clock out of range                         , */\
   { 0x4400, "int_in_flag_por"},    /* Clear POR                                         , */\
   { 0x4410, "int_in_flag_bst_ocpok"},    /* Clear DCDC OCP                                    , */\
   { 0x4420, "int_in_flag_otpok"},    /* Clear OTP alarm                                   , */\
   { 0x4430, "int_in_flag_ocp_alarm"},    /* Clear OCP alarm                                   , */\
   { 0x4440, "int_in_flag_uvpok"},    /* Clear UVP alarm                                   , */\
   { 0x4450, "int_in_flag_man_alarm_state"},    /* Clear manager alarm state                         , */\
   { 0x4460, "int_in_flag_tdm_error"},    /* Clear TDM error                                   , */\
   { 0x4470, "int_in_flag_lost_clk"},    /* Clear lost clk                                    , */\
   { 0x4480, "int_in_flag_cfma_err"},    /* Clear cfma err                                    , */\
   { 0x4490, "int_in_flag_cfma_ack"},    /* Clear cfma ack                                    , */\
   { 0x44a0, "int_in_flag_cf_speakererror"},    /* Clear coolflux speaker error                      , */\
   { 0x44b0, "int_in_flag_cold_started"},    /* Clear cold started                                , */\
   { 0x44c0, "int_in_flag_watchdog_reset"},    /* Clear watchdog reset                              , */\
   { 0x44d0, "int_in_flag_bod_vddd_nok"},    /* Clear brown out detect                            , */\
   { 0x44e0, "int_in_flag_lp_detect_mode1"},    /* Clear low power mode1 detect                      , */\
   { 0x44f0, "int_in_flag_clk_out_of_range"},    /* Clear clock out of range                          , */\
   { 0x4800, "int_enable_flag_por"},    /* Enable POR                                        , */\
   { 0x4810, "int_enable_flag_bst_ocpok"},    /* Enable DCDC OCP                                   , */\
   { 0x4820, "int_enable_flag_otpok"},    /* Enable OTP alarm                                  , */\
   { 0x4830, "int_enable_flag_ocp_alarm"},    /* Enable OCP alarm                                  , */\
   { 0x4840, "int_enable_flag_uvpok"},    /* Enable UVP alarm                                  , */\
   { 0x4850, "int_enable_flag_man_alarm_state"},    /* Enable Manager Alarm state                        , */\
   { 0x4860, "int_enable_flag_tdm_error"},    /* Enable TDM error                                  , */\
   { 0x4870, "int_enable_flag_lost_clk"},    /* Enable lost clk                                   , */\
   { 0x4880, "int_enable_flag_cfma_err"},    /* Enable cfma err                                   , */\
   { 0x4890, "int_enable_flag_cfma_ack"},    /* Enable cfma ack                                   , */\
   { 0x48a0, "int_enable_flag_cf_speakererror"},    /* Enable coolflux speaker error                     , */\
   { 0x48b0, "int_enable_flag_cold_started"},    /* Enable cold started                               , */\
   { 0x48c0, "int_enable_flag_watchdog_reset"},    /* Enable watchdog reset                             , */\
   { 0x48d0, "int_enable_flag_bod_vddd_nok"},    /* Enable brown out detect                           , */\
   { 0x48e0, "int_enable_flag_lp_detect_mode1"},    /* Enable low power mode1 detect                     , */\
   { 0x48f0, "int_enable_flag_clk_out_of_range"},    /* Enable clock out of range                         , */\
   { 0x4c00, "int_polarity_flag_por"},    /* Polarity POR                                      , */\
   { 0x4c10, "int_polarity_flag_bst_ocpok"},    /* Polarity DCDC OCP                                 , */\
   { 0x4c20, "int_polarity_flag_otpok"},    /* Polarity OTP alarm                                , */\
   { 0x4c30, "int_polarity_flag_ocp_alarm"},    /* Polarity ocp alarm                                , */\
   { 0x4c40, "int_polarity_flag_uvpok"},    /* Polarity UVP alarm                                , */\
   { 0x4c50, "int_polarity_flag_man_alarm_state"},    /* Polarity manager alarm state                      , */\
   { 0x4c60, "int_polarity_flag_tdm_error"},    /* Polarity TDM error                                , */\
   { 0x4c70, "int_polarity_flag_lost_clk"},    /* Polarity lost clk                                 , */\
   { 0x4c80, "int_polarity_flag_cfma_err"},    /* Polarity cfma err                                 , */\
   { 0x4c90, "int_polarity_flag_cfma_ack"},    /* Polarity cfma ack                                 , */\
   { 0x4ca0, "int_polarity_flag_cf_speakererror"},    /* Polarity coolflux speaker error                   , */\
   { 0x4cb0, "int_polarity_flag_cold_started"},    /* Polarity cold started                             , */\
   { 0x4cc0, "int_polarity_flag_watchdog_reset"},    /* Polarity watchdog reset                           , */\
   { 0x4cd0, "int_polarity_flag_bod_vddd_nok"},    /* Polarity brown out detect                         , */\
   { 0x4ce0, "int_polarity_flag_lp_detect_mode1"},    /* Polarity low power mode1 detect                   , */\
   { 0x4cf0, "int_polarity_flag_clk_out_of_range"},    /* Polarity clock out of range                       , */\
   { 0x5001, "vbat_prot_attack_time"},    /* Battery safeguard attack time                     , */\
   { 0x5023, "vbat_prot_thlevel"},    /* Battery safeguard threshold voltage level         , */\
   { 0x5061, "vbat_prot_max_reduct"},    /* Battery safeguard maximum reduction               , */\
   { 0x5082, "vbat_prot_release_time"},    /* Battery safeguard release time                    , */\
   { 0x50b1, "vbat_prot_hysterese"},    /* Battery Safeguard hysteresis                      , */\
   { 0x50d0, "rst_min_vbat"},    /* Reset clipper - auto clear                        , */\
   { 0x50e0, "sel_vbat"},    /* Battery voltage read out                          , */\
   { 0x50f0, "bypass_clipper"},    /* Bypass HW clipper                                 , */\
   { 0x5130, "cf_mute"},    /* Coolflux firmware soft mute control               , */\
   { 0x5187, "cf_volume"},    /* CF firmware volume control                        , */\
   { 0x5202, "ctrl_cc"},    /* Clip control setting                              , */\
   { 0x5230, "ctrl_slopectrl"},    /* Enables slope control                             , */\
   { 0x5240, "ctrl_slope"},    /* Slope speed setting (binary coded)                , */\
   { 0x5287, "gain"},    /* Amplifier gain                                    , */\
   { 0x5301, "dpsa_level"},    /* DPSA threshold levels                             , */\
   { 0x5321, "dpsa_release"},    /* DPSA Release time                                 , */\
   { 0x5340, "clipfast"},    /* Clock selection for HW clipper for battery safeguard, */\
   { 0x5350, "bypass_lp"},    /* Bypass the low power filter inside temperature sensor, */\
   { 0x5360, "first_order_mode"},    /* Overrule to 1st order mode of control stage when clipping, */\
   { 0x5370, "icomp_engage"},    /* Engage of icomp                                   , */\
   { 0x5380, "ctrl_kickback"},    /* Prevent double pulses of output stage             , */\
   { 0x5390, "icomp_engage_overrule"},    /* To overrule the functional icomp_engage signal during validation, */\
   { 0x53a3, "ctrl_dem"},    /* Enable DEM icomp and DEM one bit dac              , */\
   { 0x5400, "bypass_ctrlloop"},    /* Switch amplifier into open loop configuration     , */\
   { 0x5413, "ctrl_dem_mismatch"},    /* Enable DEM icomp mismatch for testing             , */\
   { 0x5452, "dpsa_drive"},    /* Drive setting (binary coded)                      , */\
   { 0x550a, "enbl_amp"},    /* Switch on the class-D power sections, each part of the analog sections can be switched on/off individually, */\
   { 0x55b0, "enbl_engage"},    /* Enables/engage power stage and control loop       , */\
   { 0x55c0, "enbl_engage_pst"},    /* Enables/engage power stage and control loop       , */\
   { 0x5600, "pwm_shape"},    /* PWM shape                                         , */\
   { 0x5614, "pwm_delay"},    /* PWM delay bits to set the delay, clockd is 1/(k*2048*fs), */\
   { 0x5660, "reclock_pwm"},    /* Reclock the PWM signal inside analog              , */\
   { 0x5670, "reclock_voltsense"},    /* Reclock the voltage sense PWM signal              , */\
   { 0x5680, "enbl_pwm_phase_shift"},    /* Control for PWM phase shift                       , */\
   { 0x5690, "sel_pwm_delay_src"},    /* Control for selection for PWM delay line source   , */\
   { 0x56a1, "enbl_odd_up_even_down"},    /* Control for PWM reference sawtooth generartion    , */\
   { 0x5703, "ctrl_att_dcdc"},    /* Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE), */\
   { 0x5743, "ctrl_att_spkr"},    /* Total gain depending on INPLEV setting (channel 0), */\
   { 0x5781, "vamp_sel2"},    /* VAMP_OUT2 input selection                         , */\
   { 0x5805, "zero_lvl"},    /* Low noise gain switch zero trigger level          , */\
   { 0x5861, "ctrl_fb_resistor"},    /* Select amplifier feedback resistor connection     , */\
   { 0x5881, "lownoisegain_mode"},    /* Low noise gain mode control                       , */\
   { 0x5905, "threshold_lvl"},    /* Low noise gain switch trigger level               , */\
   { 0x5965, "hold_time"},    /* Low noise mode hold time before entering into low noise mode, */\
   { 0x5a05, "lpm1_cal_offset"},    /* Low power mode1 detector ctrl cal_offset from gain module , */\
   { 0x5a65, "lpm1_zero_lvl"},    /* Low power mode1 zero crossing detection level     , */\
   { 0x5ac1, "lpm1_mode"},    /* Low power mode control                            , */\
   { 0x5b05, "lpm1_threshold_lvl"},    /* Low power mode1 amplitude trigger level           , */\
   { 0x5b65, "lpm1_hold_time"},    /* Low power mode hold time before entering into low power mode, */\
   { 0x5bc0, "disable_low_power_mode"},    /* Low power mode1 detector control                  , */\
   { 0x5c00, "enbl_minion"},    /* Enables minion (small) power stage                , */\
   { 0x5c13, "vth_vddpvbat"},    /* Select vddp-vbat threshold signal                 , */\
   { 0x5c50, "lpen_vddpvbat"},    /* Select vddp-vbat filtred vs unfiltered compare    , */\
   { 0x5c61, "ctrl_rfb"},    /* Feedback resistor selection - I2C direct mode     , */\
   { 0x5d02, "tdm_source_mapping"},    /* TDM source mapping                                , */\
   { 0x5d31, "tdm_sourcea_frame_sel"},    /* Sensed value A                                    , */\
   { 0x5d51, "tdm_sourceb_frame_sel"},    /* Sensed value B                                    , */\
   { 0x5d71, "tdm_source0_clip_sel"},    /* Clip information (analog /digital) for source0    , */\
   { 0x5d91, "tdm_source1_clip_sel"},    /* Clip information (analog /digital) for source1    , */\
   { 0x5e02, "rst_min_vbat_delay"},    /* Delay for reseting the min_vbat value inside HW Clipper (number of Fs pulses), */\
   { 0x5e30, "rst_min_vbat_sel"},    /* Control for selecting reset signal for min_bat    , */\
   { 0x5f00, "hard_mute"},    /* Hard mute - PWM                                   , */\
   { 0x5f12, "ns_hp2ln_criterion"},    /* 0..7 zeroes at ns as threshold to swap from high_power to low_noise, */\
   { 0x5f42, "ns_ln2hp_criterion"},    /* 0..7 zeroes at ns as threshold to swap from low_noise to high_power, */\
   { 0x5f78, "spare_out"},    /* Spare out register                                , */\
   { 0x600f, "spare_in"},    /* Spare IN                                          , */\
   { 0x6102, "cursense_comp_delay"},    /* Delay to allign compensation signal with current sense signal, */\
   { 0x6130, "cursense_comp_sign"},    /* Polarity of compensation for current sense        , */\
   { 0x6140, "enbl_cursense_comp"},    /* Enable current sense compensation                 , */\
   { 0x6152, "pwms_clip_lvl"},    /* Set the amount of pwm pulse that may be skipped before clip-flag is triggered, */\
   { 0x7005, "frst_boost_voltage"},    /* First Boost Voltage Level                         , */\
   { 0x7065, "scnd_boost_voltage"},    /* Second Boost Voltage Level                        , */\
   { 0x70c3, "boost_cur"},    /* Max Coil Current                                  , */\
   { 0x7101, "bst_slpcmplvl"},    /* Slope compensation current, represents LxF (inductance x frequency) value , */\
   { 0x7120, "boost_intel"},    /* Adaptive boost mode                               , */\
   { 0x7130, "boost_speed"},    /* Soft ramp up/down                                 , */\
   { 0x7140, "dcdcoff_mode"},    /* DCDC on/off                                       , */\
   { 0x7150, "dcdc_pwmonly"},    /* DCDC PWM only mode                                , */\
   { 0x7160, "boost_track"},    /* Boost algorithm selection, effective only when boost_intelligent is set to 1, */\
   { 0x7170, "sel_dcdc_envelope_8fs"},    /* Selection of data for adaptive boost algorithm, effective only when boost_intelligent is set to 1, */\
   { 0x7180, "ignore_flag_voutcomp86"},    /* Determines the maximum PWM frequency be the most efficient in relation to the Booster inductor value, */\
   { 0x7204, "boost_trip_lvl_1st"},    /* 1st adaptive boost trip levels, effective only when DCIE is set to 1, */\
   { 0x7254, "boost_trip_lvl_2nd"},    /* 2nd adaptive boost trip levels, effective only when DCIE is set to 1, */\
   { 0x72a4, "boost_trip_lvl_track"},    /* Track adaptive boost trip levels, effective only when boost_intelligent is set to 1, */\
   { 0x72f0, "enbl_trip_hyst"},    /* Enable hysteresis on booster trip levels          , */\
   { 0x7304, "boost_hold_time"},    /* Hold time for DCDC booster, effective only when boost_intelligent is set to 1, */\
   { 0x7350, "dcdc_pfm20khz_limit"},    /* DCDC in PFM mode pwm mode is activated each 50us to force a pwm pulse, */\
   { 0x7361, "dcdc_ctrl_maxzercnt"},    /* Number of zero current flags to count before going to pfm mode, */\
   { 0x7386, "dcdc_vbat_delta_detect"},    /* Threshold before booster is reacting on a delta Vbat (in PFM mode) by temporarily switching to PWM mode, */\
   { 0x73f0, "dcdc_ignore_vbat"},    /* Ignore an increase on Vbat                        , */\
   { 0x7404, "bst_drive"},    /* Binary coded drive setting for boost converter power stage, */\
   { 0x7451, "bst_scalecur"},    /* For testing direct control scale current          , */\
   { 0x7474, "bst_slopecur"},    /* For testing direct control slope current          , */\
   { 0x74c1, "bst_slope"},    /* Boost slope speed                                 , */\
   { 0x74e0, "bst_bypass_bstcur"},    /* Bypass control for boost current settings         , */\
   { 0x74f0, "bst_bypass_bstfoldback"},    /* Bypass control for boost foldback                 , */\
   { 0x7500, "enbl_bst_engage"},    /* Enable power stage dcdc controller                , */\
   { 0x7510, "enbl_bst_hizcom"},    /* Enable hiz comparator                             , */\
   { 0x7520, "enbl_bst_peakcur"},    /* Enable peak current                               , */\
   { 0x7530, "enbl_bst_power"},    /* Enable line of the powerstage                     , */\
   { 0x7540, "enbl_bst_slopecur"},    /* Enable bit of max-current dac                     , */\
   { 0x7550, "enbl_bst_voutcomp"},    /* Enable vout comparators                           , */\
   { 0x7560, "enbl_bst_voutcomp86"},    /* Enable vout-86 comparators                        , */\
   { 0x7570, "enbl_bst_voutcomp93"},    /* Enable vout-93 comparators                        , */\
   { 0x7580, "enbl_bst_windac"},    /* Enable window dac                                 , */\
   { 0x7595, "bst_windac"},    /* For testing direct control windac                 , */\
   { 0x7600, "boost_alg"},    /* Control for boost adaptive loop gain              , */\
   { 0x7611, "boost_loopgain"},    /* DCDC boost loopgain setting                       , */\
   { 0x7631, "bst_freq"},    /* DCDC boost frequency control                      , */\
   { 0x7650, "enbl_bst_peak2avg"},    /* Enable boost peak2avg functionality               , */\
   { 0x7660, "bst_use_new_zercur_detect"},    /* Enable new zero current detection for boost control, */\
   { 0x8001, "sel_clk_cs"},    /* Current sense clock duty cycle control            , */\
   { 0x8021, "micadc_speed"},    /* Current sense clock for MiCADC selection - 32/44.1/48 KHz Fs band only, */\
   { 0x8040, "cs_gain_control"},    /* Current sense gain control                        , */\
   { 0x8050, "cs_bypass_gc"},    /* Bypasses the CS gain correction                   , */\
   { 0x8060, "invertpwm"},    /* Current sense common mode feedback pwm invert control, */\
   { 0x8087, "cs_gain"},    /* Current sense gain                                , */\
   { 0x8105, "cs_ktemp"},    /* Current sense temperature compensation trimming (1 - VALUE*TEMP) * signal, */\
   { 0x8164, "cs_ktemp2"},    /* Second order temperature compensation coefficient , */\
   { 0x81b0, "enbl_cs_adc"},    /* Enable current sense ADC                          , */\
   { 0x81c0, "enbl_cs_inn1"},    /* Enable connection of current sense negative1      , */\
   { 0x81d0, "enbl_cs_inn2"},    /* Enable connection of current sense negative2      , */\
   { 0x81e0, "enbl_cs_inp1"},    /* Enable connection of current sense positive1      , */\
   { 0x81f0, "enbl_cs_inp2"},    /* Enable connection of current sense positive2      , */\
   { 0x8200, "enbl_cs_ldo"},    /* Enable current sense LDO                          , */\
   { 0x8210, "enbl_cs_vbatldo"},    /* Enable of current sense LDO                       , */\
   { 0x8220, "cs_adc_bsoinv"},    /* Bitstream inversion for current sense ADC         , */\
   { 0x8231, "cs_adc_hifreq"},    /* Frequency mode current sense ADC                  , */\
   { 0x8250, "cs_adc_nortz"},    /* Return to zero for current sense ADC              , */\
   { 0x8263, "cs_adc_offset"},    /* Micadc ADC offset setting                         , */\
   { 0x82a0, "cs_adc_slowdel"},    /* Select delay for current sense ADC (internal decision circuitry), */\
   { 0x82b4, "cs_adc_gain"},    /* Gain setting for current sense ADC (two's complement), */\
   { 0x8300, "cs_resonator_enable"},    /* Enable for resonator to improve SRN               , */\
   { 0x8310, "cs_classd_tran_skip"},    /* Skip current sense connection during a classD amplifier transition, */\
   { 0x8320, "cs_inn_short"},    /* Short current sense negative to common mode       , */\
   { 0x8330, "cs_inp_short"},    /* Short current sense positive to common mode       , */\
   { 0x8340, "cs_ldo_bypass"},    /* Bypass current sense LDO                          , */\
   { 0x8350, "cs_ldo_pulldown"},    /* Pull down current sense LDO, only valid if enbl_cs_ldo is high, */\
   { 0x8364, "cs_ldo_voset"},    /* Current sense LDO voltage level setting (two's complement), */\
   { 0x8800, "ctrl_vs_igen_supply"},    /* Control for selecting supply for VS current generator, */\
   { 0x8810, "ctrl_vs_force_div2"},    /* Select input resistive divider gain               , */\
   { 0x8820, "enbl_dc_filter"},    /* Control for enabling the DC blocking filter for voltage and current sense, */\
   { 0x8901, "volsense_pwm_sel"},    /* Voltage sense source selection control            , */\
   { 0x8920, "vs_gain_control"},    /* Voltage sense gain control                        , */\
   { 0x8930, "vs_bypass_gc"},    /* Bypasses the VS gain correction                   , */\
   { 0x8940, "vs_adc_bsoinv"},    /* Bitstream inversion for voltage sense ADC         , */\
   { 0x8950, "vs_adc_nortz"},    /* Return to zero for voltage sense ADC              , */\
   { 0x8960, "vs_adc_slowdel"},    /* Select delay for voltage sense ADC (internal decision circuitry), */\
   { 0x8970, "vs_classd_tran_skip"},    /* Skip voltage sense connection during a classD amplifier transition, */\
   { 0x8987, "vs_gain"},    /* Voltage sense gain                                , */\
   { 0x8a00, "vs_inn_short"},    /* Short voltage sense negative to common mode       , */\
   { 0x8a10, "vs_inp_short"},    /* Short voltage sense positive to common mode       , */\
   { 0x8a20, "vs_ldo_bypass"},    /* Bypass voltage sense LDO                          , */\
   { 0x8a30, "vs_ldo_pulldown"},    /* Pull down voltage sense LDO, only valid if enbl_cs_ldo is high, */\
   { 0x8a44, "vs_ldo_voset"},    /* Voltage sense LDO voltage level setting (two's complement), */\
   { 0x8a90, "enbl_vs_adc"},    /* Enable voltage sense ADC                          , */\
   { 0x8aa0, "enbl_vs_inn1"},    /* Enable connection of voltage sense negative1      , */\
   { 0x8ab0, "enbl_vs_inn2"},    /* Enable connection of voltage sense negative2      , */\
   { 0x8ac0, "enbl_vs_inp1"},    /* Enable connection of voltage sense positive1      , */\
   { 0x8ad0, "enbl_vs_inp2"},    /* Enable connection of voltage sense positive2      , */\
   { 0x8ae0, "enbl_vs_ldo"},    /* Enable voltage sense LDO                          , */\
   { 0x8af0, "enbl_vs_vbatldo"},    /* Enable of voltage sense LDO                       , */\
   { 0x9000, "cf_rst_dsp"},    /* Reset for Coolflux DSP                            , */\
   { 0x9011, "cf_dmem"},    /* Target memory for CFMA using I2C interface        , */\
   { 0x9030, "cf_aif"},    /* Auto increment                                    , */\
   { 0x9040, "cf_int"},    /* Coolflux Interrupt - auto clear                   , */\
   { 0x9050, "cf_cgate_off"},    /* Coolflux clock gating disabling control           , */\
   { 0x9080, "cf_req_cmd"},    /* Firmware event request rpc command                , */\
   { 0x9090, "cf_req_reset"},    /* Firmware event request reset restart              , */\
   { 0x90a0, "cf_req_mips"},    /* Firmware event request short on mips              , */\
   { 0x90b0, "cf_req_mute_ready"},    /* Firmware event request mute sequence ready        , */\
   { 0x90c0, "cf_req_volume_ready"},    /* Firmware event request volume ready               , */\
   { 0x90d0, "cf_req_damage"},    /* Firmware event request speaker damage detected    , */\
   { 0x90e0, "cf_req_calibrate_ready"},    /* Firmware event request calibration completed      , */\
   { 0x90f0, "cf_req_reserved"},    /* Firmware event request reserved                   , */\
   { 0x910f, "cf_madd"},    /* CF memory address                                 , */\
   { 0x920f, "cf_mema"},    /* Activate memory access                            , */\
   { 0x9307, "cf_err"},    /* CF error flags                                    , */\
   { 0x9380, "cf_ack_cmd"},    /* Firmware event acknowledge rpc command            , */\
   { 0x9390, "cf_ack_reset"},    /* Firmware event acknowledge reset restart          , */\
   { 0x93a0, "cf_ack_mips"},    /* Firmware event acknowledge short on mips          , */\
   { 0x93b0, "cf_ack_mute_ready"},    /* Firmware event acknowledge mute sequence ready    , */\
   { 0x93c0, "cf_ack_volume_ready"},    /* Firmware event acknowledge volume ready           , */\
   { 0x93d0, "cf_ack_damage"},    /* Firmware event acknowledge speaker damage detected, */\
   { 0x93e0, "cf_ack_calibrate_ready"},    /* Firmware event acknowledge calibration completed  , */\
   { 0x93f0, "cf_ack_reserved"},    /* Firmware event acknowledge reserved               , */\
   { 0xa007, "mtpkey1"},    /* KEY1 To access KEY1 protected registers 0x5A/90d  (default for engineering), */\
   { 0xa107, "mtpkey2"},    /* KEY2 to access KEY2 protected registers, customer key, */\
   { 0xa200, "key01_locked"},    /* Indicates KEY1 is locked                          , */\
   { 0xa210, "key02_locked"},    /* Indicates KEY2 is locked                          , */\
   { 0xa302, "mtp_man_address_in"},    /* MTP address from I2C register for read/writing mtp in manual single word mode, */\
   { 0xa330, "man_copy_mtp_to_iic"},    /* Start copying single word from MTP to I2C mtp register - auto clear, */\
   { 0xa340, "man_copy_iic_to_mtp"},    /* Start copying single word from I2C mtp register to mtp - auto clear, */\
   { 0xa350, "auto_copy_mtp_to_iic"},    /* Start copying all the data from mtp to I2C mtp registers - auto clear, */\
   { 0xa360, "auto_copy_iic_to_mtp"},    /* Start copying data from I2C mtp registers to mtp - auto clear, */\
   { 0xa400, "faim_set_clkws"},    /* Sets the FaIM controller clock wait state register, */\
   { 0xa410, "faim_sel_evenrows"},    /* All even rows of the FaIM are selected, active high, */\
   { 0xa420, "faim_sel_oddrows"},    /* All odd rows of the FaIM are selected, all rows in combination with sel_evenrows, */\
   { 0xa430, "faim_program_only"},    /* Skip the erase access at wr_faim command (write-program-marginread), */\
   { 0xa440, "faim_erase_only"},    /* Skip the program access at wr_faim command (write-erase-marginread), */\
   { 0xa50f, "mtp_man_data_out_msb"},    /* MSB word of MTP manual read data                  , */\
   { 0xa60f, "mtp_man_data_out_lsb"},    /* LSB word of MTP manual read data                  , */\
   { 0xa70f, "mtp_man_data_in_msb"},    /* MSB word of write data for MTP manual write       , */\
   { 0xa80f, "mtp_man_data_in_lsb"},    /* LSB word of write data for MTP manual write       , */\
   { 0xb000, "bypass_ocpcounter"},    /* Bypass OCP Counter                                , */\
   { 0xb010, "bypass_glitchfilter"},    /* Bypass glitch filter                              , */\
   { 0xb020, "bypass_ovp"},    /* Bypass OVP                                        , */\
   { 0xb030, "bypass_uvp"},    /* Bypass UVP                                        , */\
   { 0xb040, "bypass_otp"},    /* Bypass OTP                                        , */\
   { 0xb050, "bypass_lost_clk"},    /* Bypass lost clock detector                        , */\
   { 0xb060, "ctrl_vpalarm"},    /* vpalarm (uvp ovp handling)                        , */\
   { 0xb070, "disable_main_ctrl_change_prot"},    /* Disable main control change protection            , */\
   { 0xb087, "ocp_threshold"},    /* OCP threshold level                               , */\
   { 0xb108, "ext_temp"},    /* External temperature (C)                          , */\
   { 0xb190, "ext_temp_sel"},    /* Select temp Speaker calibration                   , */\
   { 0xc000, "use_direct_ctrls"},    /* Direct control to overrule several functions for testing, */\
   { 0xc010, "rst_datapath"},    /* Direct control for datapath reset                 , */\
   { 0xc020, "rst_cgu"},    /* Direct control for cgu reset                      , */\
   { 0xc038, "enbl_ref"},    /* Switch on the analog references, each part of the references can be switched on/off individually, */\
   { 0xc0c0, "use_direct_vs_ctrls"},    /* Voltage sense direct control to overrule several functions for testing, */\
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
   { 0xc3d0, "test_abistfft_enbl"},    /* Enable ABIST with FFT on Coolflux DSP             , */\
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
   { 0xc530, "test_rdson"},    /* Analog BIST, switch to enable Rdson measurement   , */\
   { 0xc540, "test_sdelta"},    /* Analog BIST, noise test                           , */\
   { 0xc550, "test_enbl_cs"},    /* Enable for digimux mode of current sense          , */\
   { 0xc560, "test_enbl_vs"},    /* Enable for digimux mode of voltage sense          , */\
   { 0xc570, "enbl_pwm_dcc"},    /* Enables direct control of pwm duty cycle for DCDC power stage, */\
   { 0xc583, "pwm_dcc_cnt"},    /* Control pwm duty cycle when enbl_pwm_dcc is 1     , */\
   { 0xc5c0, "enbl_ldo_stress"},    /* Enable stress of internal supply voltages powerstages, */\
   { 0xc607, "digimuxa_sel"},    /* DigimuxA input selection control routed to DATAO  , */\
   { 0xc687, "digimuxb_sel"},    /* DigimuxB input selection control routed to INT    , */\
   { 0xc707, "digimuxc_sel"},    /* DigimuxC input selection control routed to ADS1   , */\
   { 0xc800, "enbl_anamux1"},    /* Enable anamux1                                    , */\
   { 0xc810, "enbl_anamux2"},    /* Enable anamux2                                    , */\
   { 0xc820, "enbl_anamux3"},    /* Enable anamux3                                    , */\
   { 0xc830, "enbl_anamux4"},    /* Enable anamux4                                    , */\
   { 0xc844, "anamux1"},    /* Anamux selection control - anamux on TEST1        , */\
   { 0xc894, "anamux2"},    /* Anamux selection control - anamux on TEST2        , */\
   { 0xc903, "anamux3"},    /* Anamux selection control - anamux on TEST3        , */\
   { 0xc943, "anamux4"},    /* Anamux selection control - anamux on TEST4        , */\
   { 0xca05, "pll_seli"},    /* PLL SELI - I2C direct PLL control mode only       , */\
   { 0xca64, "pll_selp"},    /* PLL SELP - I2C direct PLL control mode only       , */\
   { 0xcab3, "pll_selr"},    /* PLL SELR - I2C direct PLL control mode only       , */\
   { 0xcaf0, "pll_frm"},    /* PLL free running mode control; 1 in TCB direct control mode, else this control bit, */\
   { 0xcb09, "pll_ndec"},    /* PLL NDEC - I2C direct PLL control mode only       , */\
   { 0xcba0, "pll_mdec_msb"},    /* MSB of PLL_mdec - I2C direct PLL control mode only, */\
   { 0xcbb0, "enbl_pll"},    /* Enables PLL in I2C direct PLL control mode only   , */\
   { 0xcbc0, "enbl_osc"},    /* Enables OSC1M in I2C direct control mode only     , */\
   { 0xcbd0, "pll_bypass"},    /* PLL bypass control in I2C direct PLL control mode only, */\
   { 0xcbe0, "pll_directi"},    /* PLL directi control in I2C direct PLL control mode only, */\
   { 0xcbf0, "pll_directo"},    /* PLL directo control in I2C direct PLL control mode only, */\
   { 0xcc0f, "pll_mdec_lsb"},    /* Bits 15..0 of PLL MDEC are I2C direct PLL control mode only, */\
   { 0xcd06, "pll_pdec"},    /* PLL PDEC - I2C direct PLL control mode only       , */\
   { 0xce0f, "tsig_freq_lsb"},    /* Internal sinus test generator frequency control   , */\
   { 0xcf02, "tsig_freq_msb"},    /* Select internal sinus test generator, frequency control msb bits, */\
   { 0xcf33, "tsig_gain"},    /* Test signal gain                                  , */\
   { 0xd000, "adc10_reset"},    /* Reset for ADC10 - I2C direct control mode         , */\
   { 0xd011, "adc10_test"},    /* Test mode selection signal for ADC10 - I2C direct control mode, */\
   { 0xd032, "adc10_sel"},    /* Select the input to convert for ADC10 - I2C direct control mode, */\
   { 0xd064, "adc10_prog_sample"},    /* ADC10 program sample setting - I2C direct control mode, */\
   { 0xd0b0, "adc10_enbl"},    /* Enable ADC10 - I2C direct control mode            , */\
   { 0xd0c0, "bypass_lp_vbat"},    /* Bypass control for Low pass filter in batt sensor , */\
   { 0xd109, "data_adc10_tempbat"},    /* ADC 10 data output data for testing               , */\
   { 0xd201, "clkdiv_audio_sel"},    /* Audio clock divider selection in direct clock control mode, */\
   { 0xd221, "clkdiv_muxa_sel"},    /* DCDC MUXA clock divider selection in direct clock control mode, */\
   { 0xd241, "clkdiv_muxb_sel"},    /* DCDC MUXB clock divider selection in direct clock control mode, */\
   { 0xd301, "int_ehs"},    /* Speed/load setting for INT IO cell, clk or data mode range (see SLIMMF IO datasheet), */\
   { 0xd321, "datao_ehs"},    /* Speed/load setting for DATAO IO cell, clk or data mode range (see SLIMMF IO datasheet), */\
   { 0xd340, "hs_mode"},    /* I2C high speed mode control                       , */\
   { 0xd407, "ctrl_digtoana_hidden"},    /* Spare digital to analog control bits - Hidden     , */\
   { 0xd480, "enbl_clk_out_of_range"},    /* Clock out of range                                , */\
   { 0xd491, "sel_wdt_clk"},    /* Watch dog clock divider settings                  , */\
   { 0xd4b0, "inject_tsig"},    /* Control bit to switch to internal sinus test generator, */\
   { 0xd500, "source_in_testmode"},    /* TDM source in test mode (return only current and voltage sense), */\
   { 0xd510, "gainatt_feedback"},    /* Gainatt feedback to tdm                           , */\
   { 0xd522, "test_parametric_io"},    /* Test IO parametric                                , */\
   { 0xd550, "ctrl_bst_clk_lp1"},    /* Boost clock control in low power mode1            , */\
   { 0xd561, "test_spare_out1"},    /* Test spare out 1                                  , */\
   { 0xd580, "bst_dcmbst"},    /* DCM boost                                         , */\
   { 0xd593, "test_spare_out2"},    /* Test spare out 2                                  , */\
   { 0xe00f, "sw_profile"},    /* Software profile data                             , */\
   { 0xe10f, "sw_vstep"},    /* Software vstep information                        , */\
   { 0xf000, "calibration_onetime"},    /* Calibration schedule                              , */\
   { 0xf010, "calibr_ron_done"},    /* Calibration Ron executed                          , */\
   { 0xf020, "calibr_dcdc_api_calibrate"},    /* Calibration current limit DCDC                    , */\
   { 0xf030, "calibr_dcdc_delta_sign"},    /* Sign bit for delta calibration current limit DCDC , */\
   { 0xf042, "calibr_dcdc_delta"},    /* Calibration delta current limit DCDC              , */\
   { 0xf078, "calibr_speaker_info"},    /* Reserved space for allowing customer to store speaker information, */\
   { 0xf105, "calibr_vout_offset"},    /* DCDC offset calibration 2's complement (key1 protected), */\
   { 0xf163, "calibr_vbg_trim"},    /* Bandgap trimming control                          , */\
   { 0xf203, "calibr_gain"},    /* HW gain module (2's complement)                   , */\
   { 0xf245, "calibr_offset"},    /* Offset for amplifier, HW gain module (2's complement), */\
   { 0xf307, "calibr_gain_vs"},    /* Voltage sense gain                                , */\
   { 0xf387, "calibr_gain_cs"},    /* Current sense gain (signed two's complement format), */\
   { 0xf40f, "mtpdata4"},    /* MTP4 data                                         , */\
   { 0xf50f, "calibr_R25C"},    /* Ron resistance of speaker coil                    , */\
   { 0xf60f, "mtpdata6"},    /* MTP6 data                                         , */\
   { 0xf706, "ctrl_offset_a"},    /* Offset of level shifter A                         , */\
   { 0xf786, "ctrl_offset_b"},    /* Offset of amplifier level shifter B               , */\
   { 0xf806, "htol_iic_addr"},    /* 7-bit I2C address to be used during HTOL testing  , */\
   { 0xf870, "htol_iic_addr_en"},    /* HTOL I2C address enable control                   , */\
   { 0xf884, "calibr_temp_offset"},    /* Temperature offset 2's compliment (key1 protected), */\
   { 0xf8d2, "calibr_temp_gain"},    /* Temperature gain 2's compliment (key1 protected)  , */\
   { 0xf900, "mtp_lock_dcdcoff_mode"},    /* Disable functionality of dcdcoff_mode bit         , */\
   { 0xf910, "mtp_lock_enbl_coolflux"},    /* Disable functionality of enbl_coolflux bit        , */\
   { 0xf920, "mtp_lock_bypass_clipper"},    /* Disable function bypass_clipper                   , */\
   { 0xf930, "mtp_enbl_pwm_delay_clock_gating"},    /* PWM delay clock auto gating                       , */\
   { 0xf940, "mtp_enbl_ocp_clock_gating"},    /* OCP clock auto gating                             , */\
   { 0xf950, "mtp_gate_cgu_clock_for_test"},    /* CGU test clock control                            , */\
   { 0xf987, "type_bits_fw"},    /* MTP control for firmware features - See Firmware I2C API document for details, */\
   { 0xfa0f, "mtpdataA"},    /* MTPdataA                                          , */\
   { 0xfb0f, "mtpdataB"},    /* MTPdataB                                          , */\
   { 0xfc0f, "mtpdataC"},    /* MTPdataC                                          , */\
   { 0xfd0f, "mtpdataD"},    /* MTPdataD                                          , */\
   { 0xfe0f, "mtpdataE"},    /* MTPdataE                                          , */\
   { 0xff07, "calibr_osc_delta_ndiv"},    /* Calibration data for OSC1M, signed number representation, */\
   { 0xffff,"Unknown bitfield enum" }    /* not found */\
};

enum tfa9894_irq {
	tfa9894_irq_max = -1,
	tfa9894_irq_all = -1 /* all irqs */};

#define TFA9894_IRQ_NAMETABLE static tfaIrqName_t Tfa9894IrqNames[]= {\
};
#endif /* _TFA9894_TFAFIELDNAMES_H */
