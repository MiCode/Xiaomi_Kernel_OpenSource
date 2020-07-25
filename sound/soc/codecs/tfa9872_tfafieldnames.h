/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _TFA9872_TFAFIELDNAMES_H
#define _TFA9872_TFAFIELDNAMES_H

#define TFA9872_I2CVERSION_N1A    26
#define TFA9872_I2CVERSION_N1B    29
#define TFA9872_I2CVERSION_N1B2   25

typedef enum nxpTfa9872BfEnumList {
TFA9872_BF_PWDN  = 0x0000,    /*!< Powerdown selection                                */
TFA9872_BF_I2CR  = 0x0010,    /*!< I2C Reset - Auto clear                             */
TFA9872_BF_AMPE  = 0x0030,    /*!< Activate Amplifier                                 */
TFA9872_BF_DCA   = 0x0040,    /*!< Activate DC-to-DC converter                        */
TFA9872_BF_INTP  = 0x0071,    /*!< Interrupt config                                   */
TFA9872_BF_BYPOCP = 0x00b0,    /*!< Bypass OCP                                         */
TFA9872_BF_TSTOCP = 0x00c0,    /*!< OCP testing control                                */
TFA9872_BF_MANSCONF = 0x0120,    /*!< I2C configured                                     */
TFA9872_BF_MANAOOSC = 0x0140,    /*!< Internal osc off at PWDN                           */
TFA9872_BF_MUTETO = 0x01d0,    /*!< Time out SB mute sequence                          */
TFA9872_BF_RCVNS = 0x01e0,    /*!< Noise shaper selection                             */
TFA9872_BF_AUDFS = 0x0203,    /*!< Sample rate (fs)                                   */
TFA9872_BF_INPLEV = 0x0240,    /*!< TDM output attenuation                             */
TFA9872_BF_FRACTDEL = 0x0255,    /*!< V/I Fractional delay                               */
TFA9872_BF_BYPHVBF = 0x02b0,    /*!< Bypass HVBAT filter                                */
TFA9872_BF_REV   = 0x030f,    /*!< Revision info                                      */
TFA9872_BF_REFCKEXT = 0x0401,    /*!< PLL external ref clock                             */
TFA9872_BF_REFCKSEL = 0x0420,    /*!< PLL internal ref clock                             */
TFA9872_BF_SSE   = 0x0510,    /*!< Enable speaker path                                */
TFA9872_BF_VSE   = 0x0530,    /*!< Voltage sense                                      */
TFA9872_BF_CSE   = 0x0550,    /*!< Current sense                                      */
TFA9872_BF_SSPDME = 0x0560,    /*!< Sub-system PDM                                     */
TFA9872_BF_PGAE  = 0x0580,    /*!< Enable PGA chop clock                              */
TFA9872_BF_SSTDME = 0x0590,    /*!< Sub-system TDM                                     */
TFA9872_BF_SSPBSTE = 0x05a0,    /*!< Sub-system boost                                   */
TFA9872_BF_SSADCE = 0x05b0,    /*!< Sub-system ADC                                     */
TFA9872_BF_SSFAIME = 0x05c0,    /*!< Sub-system FAIM                                    */
TFA9872_BF_STGAIN = 0x0d18,    /*!< Side tone gain                                     */
TFA9872_BF_STSMUTE = 0x0da0,    /*!< Side tone soft mute                                */
TFA9872_BF_ST1C  = 0x0db0,    /*!< side tone one s complement                         */
TFA9872_BF_VDDS  = 0x1000,    /*!< POR                                                */
TFA9872_BF_PLLS  = 0x1010,    /*!< PLL lock                                           */
TFA9872_BF_OTDS  = 0x1020,    /*!< OTP alarm                                          */
TFA9872_BF_OVDS  = 0x1030,    /*!< OVP alarm                                          */
TFA9872_BF_UVDS  = 0x1040,    /*!< UVP alarm                                          */
TFA9872_BF_CLKS  = 0x1050,    /*!< Clocks stable                                      */
TFA9872_BF_MTPB  = 0x1060,    /*!< MTP busy                                           */
TFA9872_BF_NOCLK = 0x1070,    /*!< Lost clock                                         */
TFA9872_BF_SWS   = 0x10a0,    /*!< Amplifier engage                                   */
TFA9872_BF_AMPS  = 0x10c0,    /*!< Amplifier enable                                   */
TFA9872_BF_AREFS = 0x10d0,    /*!< References enable                                  */
TFA9872_BF_ADCCR = 0x10e0,    /*!< Control ADC                                        */
TFA9872_BF_DCIL  = 0x1100,    /*!< DCDC current limiting                              */
TFA9872_BF_DCDCA = 0x1110,    /*!< DCDC active                                        */
TFA9872_BF_DCOCPOK = 0x1120,    /*!< DCDC OCP nmos                                      */
TFA9872_BF_DCHVBAT = 0x1140,    /*!< DCDC level 1x                                      */
TFA9872_BF_DCH114 = 0x1150,    /*!< DCDC level 1.14x                                   */
TFA9872_BF_DCH107 = 0x1160,    /*!< DCDC level 1.07x                                   */
TFA9872_BF_STMUTEB = 0x1170,    /*!< side tone (un)mute busy                            */
TFA9872_BF_STMUTE = 0x1180,    /*!< side tone mute state                               */
TFA9872_BF_TDMLUTER = 0x1190,    /*!< TDM LUT error                                      */
TFA9872_BF_TDMSTAT = 0x11a2,    /*!< TDM status bits                                    */
TFA9872_BF_TDMERR = 0x11d0,    /*!< TDM error                                          */
TFA9872_BF_OCPOAP = 0x1300,    /*!< OCPOK pmos A                                       */
TFA9872_BF_OCPOAN = 0x1310,    /*!< OCPOK nmos A                                       */
TFA9872_BF_OCPOBP = 0x1320,    /*!< OCPOK pmos B                                       */
TFA9872_BF_OCPOBN = 0x1330,    /*!< OCPOK nmos B                                       */
TFA9872_BF_CLIPAH = 0x1340,    /*!< Clipping A to Vddp                                 */
TFA9872_BF_CLIPAL = 0x1350,    /*!< Clipping A to gnd                                  */
TFA9872_BF_CLIPBH = 0x1360,    /*!< Clipping B to Vddp                                 */
TFA9872_BF_CLIPBL = 0x1370,    /*!< Clipping B to gnd                                  */
TFA9872_BF_OCDS  = 0x1380,    /*!< OCP  amplifier                                     */
TFA9872_BF_CLIPS = 0x1390,    /*!< Amplifier  clipping                                */
TFA9872_BF_OCPOKMC = 0x13a0,    /*!< OCPOK MICVDD                                       */
TFA9872_BF_MANALARM = 0x13b0,    /*!< Alarm state                                        */
TFA9872_BF_MANWAIT1 = 0x13c0,    /*!< Wait HW I2C settings                               */
TFA9872_BF_MANMUTE = 0x13e0,    /*!< Audio mute sequence                                */
TFA9872_BF_MANOPER = 0x13f0,    /*!< Operating state                                    */
TFA9872_BF_CLKOOR = 0x1420,    /*!< External clock status                              */
TFA9872_BF_MANSTATE = 0x1433,    /*!< Device manager status                              */
TFA9872_BF_DCMODE = 0x1471,    /*!< DCDC mode status bits                              */
TFA9872_BF_BATS  = 0x1509,    /*!< Battery voltage (V)                                */
TFA9872_BF_TEMPS = 0x1608,    /*!< IC Temperature (C)                                 */
TFA9872_BF_VDDPS = 0x1709,    /*!< IC VDDP voltage ( 1023*VDDP/9.5 V)                 */
TFA9872_BF_TDME  = 0x2040,    /*!< Enable interface                                   */
TFA9872_BF_TDMMODE = 0x2050,    /*!< Slave/master                                       */
TFA9872_BF_TDMCLINV = 0x2060,    /*!< Reception data to BCK clock                        */
TFA9872_BF_TDMFSLN = 0x2073,    /*!< FS length (master mode only)                       */
TFA9872_BF_TDMFSPOL = 0x20b0,    /*!< FS polarity                                        */
TFA9872_BF_TDMNBCK = 0x20c3,    /*!< N-BCK's in FS                                      */
TFA9872_BF_TDMSLOTS = 0x2103,    /*!< N-slots in Frame                                   */
TFA9872_BF_TDMSLLN = 0x2144,    /*!< N-bits in slot                                     */
TFA9872_BF_TDMBRMG = 0x2194,    /*!< N-bits remaining                                   */
TFA9872_BF_TDMDEL = 0x21e0,    /*!< data delay to FS                                   */
TFA9872_BF_TDMADJ = 0x21f0,    /*!< data adjustment                                    */
TFA9872_BF_TDMOOMP = 0x2201,    /*!< Received audio compression                         */
TFA9872_BF_TDMSSIZE = 0x2224,    /*!< Sample size per slot                               */
TFA9872_BF_TDMTXDFO = 0x2271,    /*!< Format unused bits                                 */
TFA9872_BF_TDMTXUS0 = 0x2291,    /*!< Format unused slots DATAO                          */
TFA9872_BF_TDMSPKE = 0x2300,    /*!< Control audio tdm channel in 0 (spkr + dcdc)       */
TFA9872_BF_TDMDCE = 0x2310,    /*!< Control audio  tdm channel in 1  (dcdc)            */
TFA9872_BF_TDMCSE = 0x2330,    /*!< current sense vbat temperature and vddp feedback   */
TFA9872_BF_TDMVSE = 0x2340,    /*!< Voltage sense vbat temperature and vddp feedback   */
TFA9872_BF_TDMSPKS = 0x2603,    /*!< tdm slot for sink 0 (speaker + dcdc)               */
TFA9872_BF_TDMDCS = 0x2643,    /*!< tdm slot for  sink 1  (dcdc)                       */
TFA9872_BF_TDMCSS = 0x26c3,    /*!< Slot Position of current sense vbat temperature and vddp feedback */
TFA9872_BF_TDMVSS = 0x2703,    /*!< Slot Position of Voltage sense vbat temperature and vddp feedback */
TFA9872_BF_PDMSTSEL = 0x3111,    /*!< Side tone input                                    */
TFA9872_BF_ISTVDDS = 0x4000,    /*!< Status POR                                         */
TFA9872_BF_ISTPLLS = 0x4010,    /*!< Status PLL lock                                    */
TFA9872_BF_ISTOTDS = 0x4020,    /*!< Status OTP alarm                                   */
TFA9872_BF_ISTOVDS = 0x4030,    /*!< Status OVP alarm                                   */
TFA9872_BF_ISTUVDS = 0x4040,    /*!< Status UVP alarm                                   */
TFA9872_BF_ISTCLKS = 0x4050,    /*!< Status clocks stable                               */
TFA9872_BF_ISTMTPB = 0x4060,    /*!< Status MTP busy                                    */
TFA9872_BF_ISTNOCLK = 0x4070,    /*!< Status lost clock                                  */
TFA9872_BF_ISTSWS = 0x40a0,    /*!< Status amplifier engage                            */
TFA9872_BF_ISTAMPS = 0x40c0,    /*!< Status amplifier enable                            */
TFA9872_BF_ISTAREFS = 0x40d0,    /*!< Status Ref enable                                  */
TFA9872_BF_ISTADCCR = 0x40e0,    /*!< Status Control ADC                                 */
TFA9872_BF_ISTBSTCU = 0x4100,    /*!< Status DCDC current limiting                       */
TFA9872_BF_ISTBSTHI = 0x4110,    /*!< Status DCDC active                                 */
TFA9872_BF_ISTBSTOC = 0x4120,    /*!< Status DCDC OCP                                    */
TFA9872_BF_ISTBSTPKCUR = 0x4130,    /*!< Status bst peakcur                                 */
TFA9872_BF_ISTBSTVC = 0x4140,    /*!< Status DCDC level 1x                               */
TFA9872_BF_ISTBST86 = 0x4150,    /*!< Status DCDC level 1.14x                            */
TFA9872_BF_ISTBST93 = 0x4160,    /*!< Status DCDC level 1.07x                            */
TFA9872_BF_ISTOCPR = 0x4190,    /*!< Status ocp alarm                                   */
TFA9872_BF_ISTMWSRC = 0x41a0,    /*!< Status Waits HW I2C settings                       */
TFA9872_BF_ISTMWSMU = 0x41c0,    /*!< Status Audio mute sequence                         */
TFA9872_BF_ISTCLKOOR = 0x41f0,    /*!< Status flag_clk_out_of_range                       */
TFA9872_BF_ISTTDMER = 0x4200,    /*!< Status tdm error                                   */
TFA9872_BF_ISTCLPR = 0x4220,    /*!< Status clip                                        */
TFA9872_BF_ISTLP0 = 0x4240,    /*!< Status low power mode0                             */
TFA9872_BF_ISTLP1 = 0x4250,    /*!< Status low power mode1                             */
TFA9872_BF_ISTLA = 0x4260,    /*!< Status low noise detection                         */
TFA9872_BF_ISTVDDPH = 0x4270,    /*!< Status VDDP greater than VBAT                      */
TFA9872_BF_ICLVDDS = 0x4400,    /*!< Clear POR                                          */
TFA9872_BF_ICLPLLS = 0x4410,    /*!< Clear PLL lock                                     */
TFA9872_BF_ICLOTDS = 0x4420,    /*!< Clear OTP alarm                                    */
TFA9872_BF_ICLOVDS = 0x4430,    /*!< Clear OVP alarm                                    */
TFA9872_BF_ICLUVDS = 0x4440,    /*!< Clear UVP alarm                                    */
TFA9872_BF_ICLCLKS = 0x4450,    /*!< Clear clocks stable                                */
TFA9872_BF_ICLMTPB = 0x4460,    /*!< Clear mtp busy                                     */
TFA9872_BF_ICLNOCLK = 0x4470,    /*!< Clear lost clk                                     */
TFA9872_BF_ICLSWS = 0x44a0,    /*!< Clear amplifier engage                             */
TFA9872_BF_ICLAMPS = 0x44c0,    /*!< Clear enbl amp                                     */
TFA9872_BF_ICLAREFS = 0x44d0,    /*!< Clear ref enable                                   */
TFA9872_BF_ICLADCCR = 0x44e0,    /*!< Clear control ADC                                  */
TFA9872_BF_ICLBSTCU = 0x4500,    /*!< Clear DCDC current limiting                        */
TFA9872_BF_ICLBSTHI = 0x4510,    /*!< Clear DCDC active                                  */
TFA9872_BF_ICLBSTOC = 0x4520,    /*!< Clear DCDC OCP                                     */
TFA9872_BF_ICLBSTPC = 0x4530,    /*!< Clear bst peakcur                                  */
TFA9872_BF_ICLBSTVC = 0x4540,    /*!< Clear DCDC level 1x                                */
TFA9872_BF_ICLBST86 = 0x4550,    /*!< Clear DCDC level 1.14x                             */
TFA9872_BF_ICLBST93 = 0x4560,    /*!< Clear DCDC level 1.07x                             */
TFA9872_BF_ICLOCPR = 0x4590,    /*!< Clear ocp alarm                                    */
TFA9872_BF_ICLMWSRC = 0x45a0,    /*!< Clear wait HW I2C settings                         */
TFA9872_BF_ICLMWSMU = 0x45c0,    /*!< Clear audio mute sequence                          */
TFA9872_BF_ICLCLKOOR = 0x45f0,    /*!< Clear flag_clk_out_of_range                        */
TFA9872_BF_ICLTDMER = 0x4600,    /*!< Clear tdm error                                    */
TFA9872_BF_ICLCLPR = 0x4620,    /*!< Clear clip                                         */
TFA9872_BF_ICLLP0 = 0x4640,    /*!< Clear low power mode0                              */
TFA9872_BF_ICLLP1 = 0x4650,    /*!< Clear low power mode1                              */
TFA9872_BF_ICLLA = 0x4660,    /*!< Clear low noise detection                          */
TFA9872_BF_ICLVDDPH = 0x4670,    /*!< Clear VDDP greater then VBAT                       */
TFA9872_BF_IEVDDS = 0x4800,    /*!< Enable por                                         */
TFA9872_BF_IEPLLS = 0x4810,    /*!< Enable pll lock                                    */
TFA9872_BF_IEOTDS = 0x4820,    /*!< Enable OTP alarm                                   */
TFA9872_BF_IEOVDS = 0x4830,    /*!< Enable OVP alarm                                   */
TFA9872_BF_IEUVDS = 0x4840,    /*!< Enable UVP alarm                                   */
TFA9872_BF_IECLKS = 0x4850,    /*!< Enable clocks stable                               */
TFA9872_BF_IEMTPB = 0x4860,    /*!< Enable mtp busy                                    */
TFA9872_BF_IENOCLK = 0x4870,    /*!< Enable lost clk                                    */
TFA9872_BF_IESWS = 0x48a0,    /*!< Enable amplifier engage                            */
TFA9872_BF_IEAMPS = 0x48c0,    /*!< Enable enbl amp                                    */
TFA9872_BF_IEAREFS = 0x48d0,    /*!< Enable ref enable                                  */
TFA9872_BF_IEADCCR = 0x48e0,    /*!< Enable Control ADC                                 */
TFA9872_BF_IEBSTCU = 0x4900,    /*!< Enable DCDC current limiting                       */
TFA9872_BF_IEBSTHI = 0x4910,    /*!< Enable DCDC active                                 */
TFA9872_BF_IEBSTOC = 0x4920,    /*!< Enable DCDC OCP                                    */
TFA9872_BF_IEBSTPC = 0x4930,    /*!< Enable bst peakcur                                 */
TFA9872_BF_IEBSTVC = 0x4940,    /*!< Enable DCDC level 1x                               */
TFA9872_BF_IEBST86 = 0x4950,    /*!< Enable DCDC level 1.14x                            */
TFA9872_BF_IEBST93 = 0x4960,    /*!< Enable DCDC level 1.07x                            */
TFA9872_BF_IEOCPR = 0x4990,    /*!< Enable ocp alarm                                   */
TFA9872_BF_IEMWSRC = 0x49a0,    /*!< Enable waits HW I2C settings                       */
TFA9872_BF_IEMWSMU = 0x49c0,    /*!< Enable man Audio mute sequence                     */
TFA9872_BF_IECLKOOR = 0x49f0,    /*!< Enable flag_clk_out_of_range                       */
TFA9872_BF_IETDMER = 0x4a00,    /*!< Enable tdm error                                   */
TFA9872_BF_IECLPR = 0x4a20,    /*!< Enable clip                                        */
TFA9872_BF_IELP0 = 0x4a40,    /*!< Enable low power mode0                             */
TFA9872_BF_IELP1 = 0x4a50,    /*!< Enable low power mode1                             */
TFA9872_BF_IELA  = 0x4a60,    /*!< Enable low noise detection                         */
TFA9872_BF_IEVDDPH = 0x4a70,    /*!< Enable VDDP greater tehn VBAT                      */
TFA9872_BF_IPOVDDS = 0x4c00,    /*!< Polarity por                                       */
TFA9872_BF_IPOPLLS = 0x4c10,    /*!< Polarity pll lock                                  */
TFA9872_BF_IPOOTDS = 0x4c20,    /*!< Polarity OTP alarm                                 */
TFA9872_BF_IPOOVDS = 0x4c30,    /*!< Polarity OVP alarm                                 */
TFA9872_BF_IPOUVDS = 0x4c40,    /*!< Polarity UVP alarm                                 */
TFA9872_BF_IPOCLKS = 0x4c50,    /*!< Polarity clocks stable                             */
TFA9872_BF_IPOMTPB = 0x4c60,    /*!< Polarity mtp busy                                  */
TFA9872_BF_IPONOCLK = 0x4c70,    /*!< Polarity lost clk                                  */
TFA9872_BF_IPOSWS = 0x4ca0,    /*!< Polarity amplifier engage                          */
TFA9872_BF_IPOAMPS = 0x4cc0,    /*!< Polarity enbl amp                                  */
TFA9872_BF_IPOAREFS = 0x4cd0,    /*!< Polarity ref enable                                */
TFA9872_BF_IPOADCCR = 0x4ce0,    /*!< Polarity Control ADC                               */
TFA9872_BF_IPOBSTCU = 0x4d00,    /*!< Polarity DCDC current limiting                     */
TFA9872_BF_IPOBSTHI = 0x4d10,    /*!< Polarity DCDC active                               */
TFA9872_BF_IPOBSTOC = 0x4d20,    /*!< Polarity DCDC OCP                                  */
TFA9872_BF_IPOBSTPC = 0x4d30,    /*!< Polarity bst peakcur                               */
TFA9872_BF_IPOBSTVC = 0x4d40,    /*!< Polarity DCDC level 1x                             */
TFA9872_BF_IPOBST86 = 0x4d50,    /*!< Polarity DCDC level 1.14x                          */
TFA9872_BF_IPOBST93 = 0x4d60,    /*!< Polarity DCDC level 1.07x                          */
TFA9872_BF_IPOOCPR = 0x4d90,    /*!< Polarity ocp alarm                                 */
TFA9872_BF_IPOMWSRC = 0x4da0,    /*!< Polarity waits HW I2C settings                     */
TFA9872_BF_IPOMWSMU = 0x4dc0,    /*!< Polarity man audio mute sequence                   */
TFA9872_BF_IPCLKOOR = 0x4df0,    /*!< Polarity flag_clk_out_of_range                     */
TFA9872_BF_IPOTDMER = 0x4e00,    /*!< Polarity tdm error                                 */
TFA9872_BF_IPOCLPR = 0x4e20,    /*!< Polarity clip right                                */
TFA9872_BF_IPOLP0 = 0x4e40,    /*!< Polarity low power mode0                           */
TFA9872_BF_IPOLP1 = 0x4e50,    /*!< Polarity low power mode1                           */
TFA9872_BF_IPOLA = 0x4e60,    /*!< Polarity low noise mode                            */
TFA9872_BF_IPOVDDPH = 0x4e70,    /*!< Polarity VDDP greater than VBAT                    */
TFA9872_BF_BSSCR = 0x5001,    /*!< Battery Safeguard attack time                      */
TFA9872_BF_BSST  = 0x5023,    /*!< Battery Safeguard threshold voltage level          */
TFA9872_BF_BSSRL = 0x5061,    /*!< Battery Safeguard maximum reduction                */
TFA9872_BF_BSSR  = 0x50e0,    /*!< Battery voltage read out                           */
TFA9872_BF_BSSBY = 0x50f0,    /*!< Bypass battery safeguard                           */
TFA9872_BF_BSSS  = 0x5100,    /*!< Vbat prot steepness                                */
TFA9872_BF_INTSMUTE = 0x5110,    /*!< Soft mute HW                                       */
TFA9872_BF_HPFBYP = 0x5150,    /*!< Bypass HPF                                         */
TFA9872_BF_DPSA  = 0x5170,    /*!< Enable DPSA                                        */
TFA9872_BF_CLIPCTRL = 0x5222,    /*!< Clip control setting                               */
TFA9872_BF_AMPGAIN = 0x5257,    /*!< Amplifier gain                                     */
TFA9872_BF_SLOPEE = 0x52d0,    /*!< Enables slope control                              */
TFA9872_BF_SLOPESET = 0x52e0,    /*!< Slope speed setting (bin. coded)                   */
TFA9872_BF_PGAGAIN = 0x6081,    /*!< PGA gain selection                                 */
TFA9872_BF_PGALPE = 0x60b0,    /*!< Lowpass enable                                     */
TFA9872_BF_LPM0BYP = 0x6110,    /*!< bypass low power idle mode                         */
TFA9872_BF_TDMDCG = 0x6123,    /*!< Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE) */
TFA9872_BF_TDMSPKG = 0x6163,    /*!< Total gain depending on INPLEV setting (channel 0) */
TFA9872_BF_STIDLEEN = 0x61b0,    /*!< enable idle feature for channel 1                  */
TFA9872_BF_LNMODE = 0x62e1,    /*!< ctrl select mode                                   */
TFA9872_BF_LPM1MODE = 0x64e1,    /*!< low power mode control                             */
TFA9872_BF_LPM1DIS = 0x65c0,    /*!< low power mode1  detector control                  */
TFA9872_BF_TDMSRCMAP = 0x6801,    /*!< tdm source mapping                                 */
TFA9872_BF_TDMSRCAS = 0x6821,    /*!< Sensed value  A                                    */
TFA9872_BF_TDMSRCBS = 0x6841,    /*!< Sensed value  B                                    */
TFA9872_BF_ANCSEL = 0x6881,    /*!< anc input                                          */
TFA9872_BF_ANC1C = 0x68a0,    /*!< ANC one s complement                               */
TFA9872_BF_SAMMODE = 0x6901,    /*!< sam enable                                         */
TFA9872_BF_SAMSEL = 0x6920,    /*!< sam source                                         */
TFA9872_BF_PDMOSELH = 0x6931,    /*!< pdm out value when pdm_clk is higth                */
TFA9872_BF_PDMOSELL = 0x6951,    /*!< pdm out value when pdm_clk is low                  */
TFA9872_BF_SAMOSEL = 0x6970,    /*!< ram output on mode sam and audio                   */
TFA9872_BF_LP0   = 0x6e00,    /*!< low power mode 0 detection                         */
TFA9872_BF_LP1   = 0x6e10,    /*!< low power mode 1 detection                         */
TFA9872_BF_LA    = 0x6e20,    /*!< low amplitude detection                            */
TFA9872_BF_VDDPH = 0x6e30,    /*!< vddp greater than vbat                             */
TFA9872_BF_DELCURCOMP = 0x6f02,    /*!< delay to allign compensation signal with current sense signal */
TFA9872_BF_SIGCURCOMP = 0x6f40,    /*!< polarity of compensation for current sense         */
TFA9872_BF_ENCURCOMP = 0x6f50,    /*!< enable current sense compensation                  */
TFA9872_BF_SELCLPPWM = 0x6f60,    /*!< Select pwm clip flag                               */
TFA9872_BF_LVLCLPPWM = 0x6f72,    /*!< set the amount of pwm pulse that may be skipped before clip-flag is triggered */
TFA9872_BF_DCVOS = 0x7002,    /*!< Second boost voltage level                         */
TFA9872_BF_DCMCC = 0x7033,    /*!< Max coil current                                   */
TFA9872_BF_DCCV  = 0x7071,    /*!< Slope compensation current, represents LxF (inductance x frequency) value  */
TFA9872_BF_DCIE  = 0x7090,    /*!< Adaptive boost mode                                */
TFA9872_BF_DCSR  = 0x70a0,    /*!< Soft ramp up/down                                  */
TFA9872_BF_DCDIS = 0x70e0,    /*!< DCDC on/off                                        */
TFA9872_BF_DCPWM = 0x70f0,    /*!< DCDC PWM only mode                                 */
TFA9872_BF_DCVOF = 0x7402,    /*!< 1st boost voltage level                            */
TFA9872_BF_DCTRACK = 0x7430,    /*!< Boost algorithm selection, effective only when boost_intelligent is set to 1 */
TFA9872_BF_DCTRIP = 0x7444,    /*!< 1st Adaptive boost trip levels, effective only when DCIE is set to 1 */
TFA9872_BF_DCHOLD = 0x7494,    /*!< Hold time for DCDC booster, effective only when boost_intelligent is set to 1 */
TFA9872_BF_DCTRIP2 = 0x7534,    /*!< 2nd Adaptive boost trip levels, effective only when DCIE is set to 1 */
TFA9872_BF_DCTRIPT = 0x7584,    /*!< Track Adaptive boost trip levels, effective only when boost_intelligent is set to 1 */
TFA9872_BF_MTPK  = 0xa107,    /*!< MTP KEY2 register                                  */
TFA9872_BF_KEY1LOCKED = 0xa200,    /*!< Indicates KEY1 is locked                           */
TFA9872_BF_KEY2LOCKED = 0xa210,    /*!< Indicates KEY2 is locked                           */
TFA9872_BF_CMTPI = 0xa350,    /*!< Start copying all the data from mtp to I2C mtp registers */
TFA9872_BF_CIMTP = 0xa360,    /*!< Start copying data from I2C mtp registers to mtp   */
TFA9872_BF_MTPRDMSB = 0xa50f,    /*!< MSB word of MTP manual read data                   */
TFA9872_BF_MTPRDLSB = 0xa60f,    /*!< LSB word of MTP manual read data                   */
TFA9872_BF_EXTTS = 0xb108,    /*!< External temperature (C)                           */
TFA9872_BF_TROS  = 0xb190,    /*!< Select temp Speaker calibration                    */
TFA9872_BF_SWPROFIL = 0xee0f,    /*!< Software profile data                              */
TFA9872_BF_SWVSTEP = 0xef0f,    /*!< Software vstep information                         */
TFA9872_BF_MTPOTC = 0xf000,    /*!< Calibration schedule                               */
TFA9872_BF_MTPEX = 0xf010,    /*!< Calibration Ron executed                           */
TFA9872_BF_DCMCCAPI = 0xf020,    /*!< Calibration current limit DCDC                     */
TFA9872_BF_DCMCCSB = 0xf030,    /*!< Sign bit for delta calibration current limit DCDC  */
TFA9872_BF_USERDEF = 0xf042,    /*!< Calibration delta current limit DCDC               */
TFA9872_BF_CUSTINFO = 0xf078,    /*!< Reserved space for allowing customer to store speaker information */
TFA9872_BF_R25C  = 0xf50f,    /*!< Ron resistance of  speaker coil                    */
} nxpTfa9872BfEnumList_t;
#define TFA9872_NAMETABLE static tfaBfName_t Tfa9872DatasheetNames[] = {\
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
{ 0x1e0, "RCVNS"},    /* Noise shaper selection                            , */\
{ 0x203, "AUDFS"},    /* Sample rate (fs)                                  , */\
{ 0x240, "INPLEV"},    /* TDM output attenuation                            , */\
{ 0x255, "FRACTDEL"},    /* V/I Fractional delay                              , */\
{ 0x2b0, "BYPHVBF"},    /* Bypass HVBAT filter                               , */\
{ 0x30f, "REV"},    /* Revision info                                     , */\
{ 0x401, "REFCKEXT"},    /* PLL external ref clock                            , */\
{ 0x420, "REFCKSEL"},    /* PLL internal ref clock                            , */\
{ 0x510, "SSE"},    /* Enable speaker path                               , */\
{ 0x530, "VSE"},    /* Voltage sense                                     , */\
{ 0x550, "CSE"},    /* Current sense                                     , */\
{ 0x560, "SSPDME"},    /* Sub-system PDM                                    , */\
{ 0x580, "PGAE"},    /* Enable PGA chop clock                             , */\
{ 0x590, "SSTDME"},    /* Sub-system TDM                                    , */\
{ 0x5a0, "SSPBSTE"},    /* Sub-system boost                                  , */\
{ 0x5b0, "SSADCE"},    /* Sub-system ADC                                    , */\
{ 0x5c0, "SSFAIME"},    /* Sub-system FAIM                                   , */\
{ 0xd18, "STGAIN"},    /* Side tone gain                                    , */\
{ 0xda0, "STSMUTE"},    /* Side tone soft mute                               , */\
{ 0xdb0, "ST1C"},    /* side tone one s complement                        , */\
{ 0x1000, "VDDS"},    /* POR                                               , */\
{ 0x1010, "PLLS"},    /* PLL lock                                          , */\
{ 0x1020, "OTDS"},    /* OTP alarm                                         , */\
{ 0x1030, "OVDS"},    /* OVP alarm                                         , */\
{ 0x1040, "UVDS"},    /* UVP alarm                                         , */\
{ 0x1050, "CLKS"},    /* Clocks stable                                     , */\
{ 0x1060, "MTPB"},    /* MTP busy                                          , */\
{ 0x1070, "NOCLK"},    /* Lost clock                                        , */\
{ 0x10a0, "SWS"},    /* Amplifier engage                                  , */\
{ 0x10c0, "AMPS"},    /* Amplifier enable                                  , */\
{ 0x10d0, "AREFS"},    /* References enable                                 , */\
{ 0x10e0, "ADCCR"},    /* Control ADC                                       , */\
{ 0x1100, "DCIL"},    /* DCDC current limiting                             , */\
{ 0x1110, "DCDCA"},    /* DCDC active                                       , */\
{ 0x1120, "DCOCPOK"},    /* DCDC OCP nmos                                     , */\
{ 0x1140, "DCHVBAT"},    /* DCDC level 1x                                     , */\
{ 0x1150, "DCH114"},    /* DCDC level 1.14x                                  , */\
{ 0x1160, "DCH107"},    /* DCDC level 1.07x                                  , */\
{ 0x1170, "STMUTEB"},    /* side tone (un)mute busy                           , */\
{ 0x1180, "STMUTE"},    /* side tone mute state                              , */\
{ 0x1190, "TDMLUTER"},    /* TDM LUT error                                     , */\
{ 0x11a2, "TDMSTAT"},    /* TDM status bits                                   , */\
{ 0x11d0, "TDMERR"},    /* TDM error                                         , */\
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
{ 0x13a0, "OCPOKMC"},    /* OCPOK MICVDD                                      , */\
{ 0x13b0, "MANALARM"},    /* Alarm state                                       , */\
{ 0x13c0, "MANWAIT1"},    /* Wait HW I2C settings                              , */\
{ 0x13e0, "MANMUTE"},    /* Audio mute sequence                               , */\
{ 0x13f0, "MANOPER"},    /* Operating state                                   , */\
{ 0x1420, "CLKOOR"},    /* External clock status                             , */\
{ 0x1433, "MANSTATE"},    /* Device manager status                             , */\
{ 0x1471, "DCMODE"},    /* DCDC mode status bits                             , */\
{ 0x1509, "BATS"},    /* Battery voltage (V)                               , */\
{ 0x1608, "TEMPS"},    /* IC Temperature (C)                                , */\
{ 0x1709, "VDDPS"},    /* IC VDDP voltage ( 1023*VDDP/9.5 V)                , */\
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
{ 0x3111, "PDMSTSEL"},    /* Side tone input                                   , */\
{ 0x4000, "ISTVDDS"},    /* Status POR                                        , */\
{ 0x4010, "ISTPLLS"},    /* Status PLL lock                                   , */\
{ 0x4020, "ISTOTDS"},    /* Status OTP alarm                                  , */\
{ 0x4030, "ISTOVDS"},    /* Status OVP alarm                                  , */\
{ 0x4040, "ISTUVDS"},    /* Status UVP alarm                                  , */\
{ 0x4050, "ISTCLKS"},    /* Status clocks stable                              , */\
{ 0x4060, "ISTMTPB"},    /* Status MTP busy                                   , */\
{ 0x4070, "ISTNOCLK"},    /* Status lost clock                                 , */\
{ 0x40a0, "ISTSWS"},    /* Status amplifier engage                           , */\
{ 0x40c0, "ISTAMPS"},    /* Status amplifier enable                           , */\
{ 0x40d0, "ISTAREFS"},    /* Status Ref enable                                 , */\
{ 0x40e0, "ISTADCCR"},    /* Status Control ADC                                , */\
{ 0x4100, "ISTBSTCU"},    /* Status DCDC current limiting                      , */\
{ 0x4110, "ISTBSTHI"},    /* Status DCDC active                                , */\
{ 0x4120, "ISTBSTOC"},    /* Status DCDC OCP                                   , */\
{ 0x4130, "ISTBSTPKCUR"},    /* Status bst peakcur                                , */\
{ 0x4140, "ISTBSTVC"},    /* Status DCDC level 1x                              , */\
{ 0x4150, "ISTBST86"},    /* Status DCDC level 1.14x                           , */\
{ 0x4160, "ISTBST93"},    /* Status DCDC level 1.07x                           , */\
{ 0x4190, "ISTOCPR"},    /* Status ocp alarm                                  , */\
{ 0x41a0, "ISTMWSRC"},    /* Status Waits HW I2C settings                      , */\
{ 0x41c0, "ISTMWSMU"},    /* Status Audio mute sequence                        , */\
{ 0x41f0, "ISTCLKOOR"},    /* Status flag_clk_out_of_range                      , */\
{ 0x4200, "ISTTDMER"},    /* Status tdm error                                  , */\
{ 0x4220, "ISTCLPR"},    /* Status clip                                       , */\
{ 0x4240, "ISTLP0"},    /* Status low power mode0                            , */\
{ 0x4250, "ISTLP1"},    /* Status low power mode1                            , */\
{ 0x4260, "ISTLA"},    /* Status low noise detection                        , */\
{ 0x4270, "ISTVDDPH"},    /* Status VDDP greater than VBAT                     , */\
{ 0x4400, "ICLVDDS"},    /* Clear POR                                         , */\
{ 0x4410, "ICLPLLS"},    /* Clear PLL lock                                    , */\
{ 0x4420, "ICLOTDS"},    /* Clear OTP alarm                                   , */\
{ 0x4430, "ICLOVDS"},    /* Clear OVP alarm                                   , */\
{ 0x4440, "ICLUVDS"},    /* Clear UVP alarm                                   , */\
{ 0x4450, "ICLCLKS"},    /* Clear clocks stable                               , */\
{ 0x4460, "ICLMTPB"},    /* Clear mtp busy                                    , */\
{ 0x4470, "ICLNOCLK"},    /* Clear lost clk                                    , */\
{ 0x44a0, "ICLSWS"},    /* Clear amplifier engage                            , */\
{ 0x44c0, "ICLAMPS"},    /* Clear enbl amp                                    , */\
{ 0x44d0, "ICLAREFS"},    /* Clear ref enable                                  , */\
{ 0x44e0, "ICLADCCR"},    /* Clear control ADC                                 , */\
{ 0x4500, "ICLBSTCU"},    /* Clear DCDC current limiting                       , */\
{ 0x4510, "ICLBSTHI"},    /* Clear DCDC active                                 , */\
{ 0x4520, "ICLBSTOC"},    /* Clear DCDC OCP                                    , */\
{ 0x4530, "ICLBSTPC"},    /* Clear bst peakcur                                 , */\
{ 0x4540, "ICLBSTVC"},    /* Clear DCDC level 1x                               , */\
{ 0x4550, "ICLBST86"},    /* Clear DCDC level 1.14x                            , */\
{ 0x4560, "ICLBST93"},    /* Clear DCDC level 1.07x                            , */\
{ 0x4590, "ICLOCPR"},    /* Clear ocp alarm                                   , */\
{ 0x45a0, "ICLMWSRC"},    /* Clear wait HW I2C settings                        , */\
{ 0x45c0, "ICLMWSMU"},    /* Clear audio mute sequence                         , */\
{ 0x45f0, "ICLCLKOOR"},    /* Clear flag_clk_out_of_range                       , */\
{ 0x4600, "ICLTDMER"},    /* Clear tdm error                                   , */\
{ 0x4620, "ICLCLPR"},    /* Clear clip                                        , */\
{ 0x4640, "ICLLP0"},    /* Clear low power mode0                             , */\
{ 0x4650, "ICLLP1"},    /* Clear low power mode1                             , */\
{ 0x4660, "ICLLA"},    /* Clear low noise detection                         , */\
{ 0x4670, "ICLVDDPH"},    /* Clear VDDP greater then VBAT                      , */\
{ 0x4800, "IEVDDS"},    /* Enable por                                        , */\
{ 0x4810, "IEPLLS"},    /* Enable pll lock                                   , */\
{ 0x4820, "IEOTDS"},    /* Enable OTP alarm                                  , */\
{ 0x4830, "IEOVDS"},    /* Enable OVP alarm                                  , */\
{ 0x4840, "IEUVDS"},    /* Enable UVP alarm                                  , */\
{ 0x4850, "IECLKS"},    /* Enable clocks stable                              , */\
{ 0x4860, "IEMTPB"},    /* Enable mtp busy                                   , */\
{ 0x4870, "IENOCLK"},    /* Enable lost clk                                   , */\
{ 0x48a0, "IESWS"},    /* Enable amplifier engage                           , */\
{ 0x48c0, "IEAMPS"},    /* Enable enbl amp                                   , */\
{ 0x48d0, "IEAREFS"},    /* Enable ref enable                                 , */\
{ 0x48e0, "IEADCCR"},    /* Enable Control ADC                                , */\
{ 0x4900, "IEBSTCU"},    /* Enable DCDC current limiting                      , */\
{ 0x4910, "IEBSTHI"},    /* Enable DCDC active                                , */\
{ 0x4920, "IEBSTOC"},    /* Enable DCDC OCP                                   , */\
{ 0x4930, "IEBSTPC"},    /* Enable bst peakcur                                , */\
{ 0x4940, "IEBSTVC"},    /* Enable DCDC level 1x                              , */\
{ 0x4950, "IEBST86"},    /* Enable DCDC level 1.14x                           , */\
{ 0x4960, "IEBST93"},    /* Enable DCDC level 1.07x                           , */\
{ 0x4990, "IEOCPR"},    /* Enable ocp alarm                                  , */\
{ 0x49a0, "IEMWSRC"},    /* Enable waits HW I2C settings                      , */\
{ 0x49c0, "IEMWSMU"},    /* Enable man Audio mute sequence                    , */\
{ 0x49f0, "IECLKOOR"},    /* Enable flag_clk_out_of_range                      , */\
{ 0x4a00, "IETDMER"},    /* Enable tdm error                                  , */\
{ 0x4a20, "IECLPR"},    /* Enable clip                                       , */\
{ 0x4a40, "IELP0"},    /* Enable low power mode0                            , */\
{ 0x4a50, "IELP1"},    /* Enable low power mode1                            , */\
{ 0x4a60, "IELA"},    /* Enable low noise detection                        , */\
{ 0x4a70, "IEVDDPH"},    /* Enable VDDP greater tehn VBAT                     , */\
{ 0x4c00, "IPOVDDS"},    /* Polarity por                                      , */\
{ 0x4c10, "IPOPLLS"},    /* Polarity pll lock                                 , */\
{ 0x4c20, "IPOOTDS"},    /* Polarity OTP alarm                                , */\
{ 0x4c30, "IPOOVDS"},    /* Polarity OVP alarm                                , */\
{ 0x4c40, "IPOUVDS"},    /* Polarity UVP alarm                                , */\
{ 0x4c50, "IPOCLKS"},    /* Polarity clocks stable                            , */\
{ 0x4c60, "IPOMTPB"},    /* Polarity mtp busy                                 , */\
{ 0x4c70, "IPONOCLK"},    /* Polarity lost clk                                 , */\
{ 0x4ca0, "IPOSWS"},    /* Polarity amplifier engage                         , */\
{ 0x4cc0, "IPOAMPS"},    /* Polarity enbl amp                                 , */\
{ 0x4cd0, "IPOAREFS"},    /* Polarity ref enable                               , */\
{ 0x4ce0, "IPOADCCR"},    /* Polarity Control ADC                              , */\
{ 0x4d00, "IPOBSTCU"},    /* Polarity DCDC current limiting                    , */\
{ 0x4d10, "IPOBSTHI"},    /* Polarity DCDC active                              , */\
{ 0x4d20, "IPOBSTOC"},    /* Polarity DCDC OCP                                 , */\
{ 0x4d30, "IPOBSTPC"},    /* Polarity bst peakcur                              , */\
{ 0x4d40, "IPOBSTVC"},    /* Polarity DCDC level 1x                            , */\
{ 0x4d50, "IPOBST86"},    /* Polarity DCDC level 1.14x                         , */\
{ 0x4d60, "IPOBST93"},    /* Polarity DCDC level 1.07x                         , */\
{ 0x4d90, "IPOOCPR"},    /* Polarity ocp alarm                                , */\
{ 0x4da0, "IPOMWSRC"},    /* Polarity waits HW I2C settings                    , */\
{ 0x4dc0, "IPOMWSMU"},    /* Polarity man audio mute sequence                  , */\
{ 0x4df0, "IPCLKOOR"},    /* Polarity flag_clk_out_of_range                    , */\
{ 0x4e00, "IPOTDMER"},    /* Polarity tdm error                                , */\
{ 0x4e20, "IPOCLPR"},    /* Polarity clip right                               , */\
{ 0x4e40, "IPOLP0"},    /* Polarity low power mode0                          , */\
{ 0x4e50, "IPOLP1"},    /* Polarity low power mode1                          , */\
{ 0x4e60, "IPOLA"},    /* Polarity low noise mode                           , */\
{ 0x4e70, "IPOVDDPH"},    /* Polarity VDDP greater than VBAT                   , */\
{ 0x5001, "BSSCR"},    /* Battery Safeguard attack time                     , */\
{ 0x5023, "BSST"},    /* Battery Safeguard threshold voltage level         , */\
{ 0x5061, "BSSRL"},    /* Battery Safeguard maximum reduction               , */\
{ 0x50e0, "BSSR"},    /* Battery voltage read out                          , */\
{ 0x50f0, "BSSBY"},    /* Bypass battery safeguard                          , */\
{ 0x5100, "BSSS"},    /* Vbat prot steepness                               , */\
{ 0x5110, "INTSMUTE"},    /* Soft mute HW                                      , */\
{ 0x5150, "HPFBYP"},    /* Bypass HPF                                        , */\
{ 0x5170, "DPSA"},    /* Enable DPSA                                       , */\
{ 0x5222, "CLIPCTRL"},    /* Clip control setting                              , */\
{ 0x5257, "AMPGAIN"},    /* Amplifier gain                                    , */\
{ 0x52d0, "SLOPEE"},    /* Enables slope control                             , */\
{ 0x52e0, "SLOPESET"},    /* Slope speed setting (bin. coded)                  , */\
{ 0x6081, "PGAGAIN"},    /* PGA gain selection                                , */\
{ 0x60b0, "PGALPE"},    /* Lowpass enable                                    , */\
{ 0x6110, "LPM0BYP"},    /* bypass low power idle mode                        , */\
{ 0x6123, "TDMDCG"},    /* Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE), */\
{ 0x6163, "TDMSPKG"},    /* Total gain depending on INPLEV setting (channel 0), */\
{ 0x61b0, "STIDLEEN"},    /* enable idle feature for channel 1                 , */\
{ 0x62e1, "LNMODE"},    /* ctrl select mode                                  , */\
{ 0x64e1, "LPM1MODE"},    /* low power mode control                            , */\
{ 0x65c0, "LPM1DIS"},    /* low power mode1  detector control                 , */\
{ 0x6801, "TDMSRCMAP"},    /* tdm source mapping                                , */\
{ 0x6821, "TDMSRCAS"},    /* Sensed value  A                                   , */\
{ 0x6841, "TDMSRCBS"},    /* Sensed value  B                                   , */\
{ 0x6881, "ANCSEL"},    /* anc input                                         , */\
{ 0x68a0, "ANC1C"},    /* ANC one s complement                              , */\
{ 0x6901, "SAMMODE"},    /* sam enable                                        , */\
{ 0x6920, "SAMSEL"},    /* sam source                                        , */\
{ 0x6931, "PDMOSELH"},    /* pdm out value when pdm_clk is higth               , */\
{ 0x6951, "PDMOSELL"},    /* pdm out value when pdm_clk is low                 , */\
{ 0x6970, "SAMOSEL"},    /* ram output on mode sam and audio                  , */\
{ 0x6e00, "LP0"},    /* low power mode 0 detection                        , */\
{ 0x6e10, "LP1"},    /* low power mode 1 detection                        , */\
{ 0x6e20, "LA"},    /* low amplitude detection                           , */\
{ 0x6e30, "VDDPH"},    /* vddp greater than vbat                            , */\
{ 0x6f02, "DELCURCOMP"},    /* delay to allign compensation signal with current sense signal, */\
{ 0x6f40, "SIGCURCOMP"},    /* polarity of compensation for current sense        , */\
{ 0x6f50, "ENCURCOMP"},    /* enable current sense compensation                 , */\
{ 0x6f60, "SELCLPPWM"},    /* Select pwm clip flag                              , */\
{ 0x6f72, "LVLCLPPWM"},    /* set the amount of pwm pulse that may be skipped before clip-flag is triggered, */\
{ 0x7002, "DCVOS"},    /* Second boost voltage level                        , */\
{ 0x7033, "DCMCC"},    /* Max coil current                                  , */\
{ 0x7071, "DCCV"},    /* Slope compensation current, represents LxF (inductance x frequency) value , */\
{ 0x7090, "DCIE"},    /* Adaptive boost mode                               , */\
{ 0x70a0, "DCSR"},    /* Soft ramp up/down                                 , */\
{ 0x70e0, "DCDIS"},    /* DCDC on/off                                       , */\
{ 0x70f0, "DCPWM"},    /* DCDC PWM only mode                                , */\
{ 0x7402, "DCVOF"},    /* 1st boost voltage level                           , */\
{ 0x7430, "DCTRACK"},    /* Boost algorithm selection, effective only when boost_intelligent is set to 1, */\
{ 0x7444, "DCTRIP"},    /* 1st Adaptive boost trip levels, effective only when DCIE is set to 1, */\
{ 0x7494, "DCHOLD"},    /* Hold time for DCDC booster, effective only when boost_intelligent is set to 1, */\
{ 0x7534, "DCTRIP2"},    /* 2nd Adaptive boost trip levels, effective only when DCIE is set to 1, */\
{ 0x7584, "DCTRIPT"},    /* Track Adaptive boost trip levels, effective only when boost_intelligent is set to 1, */\
{ 0xa107, "MTPK"},    /* MTP KEY2 register                                 , */\
{ 0xa200, "KEY1LOCKED"},    /* Indicates KEY1 is locked                          , */\
{ 0xa210, "KEY2LOCKED"},    /* Indicates KEY2 is locked                          , */\
{ 0xa350, "CMTPI"},    /* Start copying all the data from mtp to I2C mtp registers, */\
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
{ 0xffff, "Unknown bitfield enum" }   /* not found */\
};

#define TFA9872_BITNAMETABLE static tfaBfName_t Tfa9872BitNames[] = {\
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
{ 0x203, "audio_fs"},    /* Sample rate (fs)                                  , */\
{ 0x240, "input_level"},    /* TDM output attenuation                            , */\
{ 0x255, "cs_frac_delay"},    /* V/I Fractional delay                              , */\
{ 0x2b0, "bypass_hvbat_filter"},    /* Bypass HVBAT filter                               , */\
{ 0x2d0, "sel_hysteresis"},    /* Select hysteresis for clock range detector        , */\
{ 0x30f, "device_rev"},    /* Revision info                                     , */\
{ 0x401, "pll_clkin_sel"},    /* PLL external ref clock                            , */\
{ 0x420, "pll_clkin_sel_osc"},    /* PLL internal ref clock                            , */\
{ 0x510, "enbl_spkr_ss"},    /* Enable speaker path                               , */\
{ 0x530, "enbl_volsense"},    /* Voltage sense                                     , */\
{ 0x550, "enbl_cursense"},    /* Current sense                                     , */\
{ 0x560, "enbl_pdm_ss"},    /* Sub-system PDM                                    , */\
{ 0x580, "enbl_pga_chop"},    /* Enable PGA chop clock                             , */\
{ 0x590, "enbl_tdm_ss"},    /* Sub-system TDM                                    , */\
{ 0x5a0, "enbl_bst_ss"},    /* Sub-system boost                                  , */\
{ 0x5b0, "enbl_adc_ss"},    /* Sub-system ADC                                    , */\
{ 0x5c0, "enbl_faim_ss"},    /* Sub-system FAIM                                   , */\
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
{ 0x10a0, "flag_engage"},    /* Amplifier engage                                  , */\
{ 0x10c0, "flag_enbl_amp"},    /* Amplifier enable                                  , */\
{ 0x10d0, "flag_enbl_ref"},    /* References enable                                 , */\
{ 0x10e0, "flag_adc10_ready"},    /* Control ADC                                       , */\
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
{ 0x13b0, "flag_man_alarm_state"},    /* Alarm state                                       , */\
{ 0x13c0, "flag_man_wait_src_settings"},    /* Wait HW I2C settings                              , */\
{ 0x13e0, "flag_man_start_mute_audio"},    /* Audio mute sequence                               , */\
{ 0x13f0, "flag_man_operating_state"},    /* Operating state                                   , */\
{ 0x1420, "flag_clk_out_of_range"},    /* External clock status                             , */\
{ 0x1433, "man_state"},    /* Device manager status                             , */\
{ 0x1471, "status_bst_mode"},    /* DCDC mode status bits                             , */\
{ 0x1509, "bat_adc"},    /* Battery voltage (V)                               , */\
{ 0x1608, "temp_adc"},    /* IC Temperature (C)                                , */\
{ 0x1709, "vddp_adc"},    /* IC VDDP voltage ( 1023*VDDP/9.5 V)                , */\
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
{ 0x3111, "pdm_side_tone_sel"},    /* Side tone input                                   , */\
{ 0x3201, "pdm_nbck"},    /* PDM BCK/Fs ratio                                  , */\
{ 0x4000, "int_out_flag_por"},    /* Status POR                                        , */\
{ 0x4010, "int_out_flag_pll_lock"},    /* Status PLL lock                                   , */\
{ 0x4020, "int_out_flag_otpok"},    /* Status OTP alarm                                  , */\
{ 0x4030, "int_out_flag_ovpok"},    /* Status OVP alarm                                  , */\
{ 0x4040, "int_out_flag_uvpok"},    /* Status UVP alarm                                  , */\
{ 0x4050, "int_out_flag_clocks_stable"},    /* Status clocks stable                              , */\
{ 0x4060, "int_out_flag_mtp_busy"},    /* Status MTP busy                                   , */\
{ 0x4070, "int_out_flag_lost_clk"},    /* Status lost clock                                 , */\
{ 0x40a0, "int_out_flag_engage"},    /* Status amplifier engage                           , */\
{ 0x40c0, "int_out_flag_enbl_amp"},    /* Status amplifier enable                           , */\
{ 0x40d0, "int_out_flag_enbl_ref"},    /* Status Ref enable                                 , */\
{ 0x40e0, "int_out_flag_adc10_ready"},    /* Status Control ADC                                , */\
{ 0x4100, "int_out_flag_bst_bstcur"},    /* Status DCDC current limiting                      , */\
{ 0x4110, "int_out_flag_bst_hiz"},    /* Status DCDC active                                , */\
{ 0x4120, "int_out_flag_bst_ocpok"},    /* Status DCDC OCP                                   , */\
{ 0x4130, "int_out_flag_bst_peakcur"},    /* Status bst peakcur                                , */\
{ 0x4140, "int_out_flag_bst_voutcomp"},    /* Status DCDC level 1x                              , */\
{ 0x4150, "int_out_flag_bst_voutcomp86"},    /* Status DCDC level 1.14x                           , */\
{ 0x4160, "int_out_flag_bst_voutcomp93"},    /* Status DCDC level 1.07x                           , */\
{ 0x4190, "int_out_flag_ocp_alarm"},    /* Status ocp alarm                                  , */\
{ 0x41a0, "int_out_flag_man_wait_src_settings"},    /* Status Waits HW I2C settings                      , */\
{ 0x41c0, "int_out_flag_man_start_mute_audio"},    /* Status Audio mute sequence                        , */\
{ 0x41f0, "int_out_flag_clk_out_of_range"},    /* Status flag_clk_out_of_range                      , */\
{ 0x4200, "int_out_flag_tdm_error"},    /* Status tdm error                                  , */\
{ 0x4220, "int_out_flag_clip"},    /* Status clip                                       , */\
{ 0x4240, "int_out_flag_lp_detect_mode0"},    /* Status low power mode0                            , */\
{ 0x4250, "int_out_flag_lp_detect_mode1"},    /* Status low power mode1                            , */\
{ 0x4260, "int_out_flag_low_amplitude"},    /* Status low noise detection                        , */\
{ 0x4270, "int_out_flag_vddp_gt_vbat"},    /* Status VDDP greater than VBAT                     , */\
{ 0x4400, "int_in_flag_por"},    /* Clear POR                                         , */\
{ 0x4410, "int_in_flag_pll_lock"},    /* Clear PLL lock                                    , */\
{ 0x4420, "int_in_flag_otpok"},    /* Clear OTP alarm                                   , */\
{ 0x4430, "int_in_flag_ovpok"},    /* Clear OVP alarm                                   , */\
{ 0x4440, "int_in_flag_uvpok"},    /* Clear UVP alarm                                   , */\
{ 0x4450, "int_in_flag_clocks_stable"},    /* Clear clocks stable                               , */\
{ 0x4460, "int_in_flag_mtp_busy"},    /* Clear mtp busy                                    , */\
{ 0x4470, "int_in_flag_lost_clk"},    /* Clear lost clk                                    , */\
{ 0x44a0, "int_in_flag_engage"},    /* Clear amplifier engage                            , */\
{ 0x44c0, "int_in_flag_enbl_amp"},    /* Clear enbl amp                                    , */\
{ 0x44d0, "int_in_flag_enbl_ref"},    /* Clear ref enable                                  , */\
{ 0x44e0, "int_in_flag_adc10_ready"},    /* Clear control ADC                                 , */\
{ 0x4500, "int_in_flag_bst_bstcur"},    /* Clear DCDC current limiting                       , */\
{ 0x4510, "int_in_flag_bst_hiz"},    /* Clear DCDC active                                 , */\
{ 0x4520, "int_in_flag_bst_ocpok"},    /* Clear DCDC OCP                                    , */\
{ 0x4530, "int_in_flag_bst_peakcur"},    /* Clear bst peakcur                                 , */\
{ 0x4540, "int_in_flag_bst_voutcomp"},    /* Clear DCDC level 1x                               , */\
{ 0x4550, "int_in_flag_bst_voutcomp86"},    /* Clear DCDC level 1.14x                            , */\
{ 0x4560, "int_in_flag_bst_voutcomp93"},    /* Clear DCDC level 1.07x                            , */\
{ 0x4590, "int_in_flag_ocp_alarm"},    /* Clear ocp alarm                                   , */\
{ 0x45a0, "int_in_flag_man_wait_src_settings"},    /* Clear wait HW I2C settings                        , */\
{ 0x45c0, "int_in_flag_man_start_mute_audio"},    /* Clear audio mute sequence                         , */\
{ 0x45f0, "int_in_flag_clk_out_of_range"},    /* Clear flag_clk_out_of_range                       , */\
{ 0x4600, "int_in_flag_tdm_error"},    /* Clear tdm error                                   , */\
{ 0x4620, "int_in_flag_clip"},    /* Clear clip                                        , */\
{ 0x4640, "int_in_flag_lp_detect_mode0"},    /* Clear low power mode0                             , */\
{ 0x4650, "int_in_flag_lp_detect_mode1"},    /* Clear low power mode1                             , */\
{ 0x4660, "int_in_flag_low_amplitude"},    /* Clear low noise detection                         , */\
{ 0x4670, "int_in_flag_vddp_gt_vbat"},    /* Clear VDDP greater then VBAT                      , */\
{ 0x4800, "int_enable_flag_por"},    /* Enable por                                        , */\
{ 0x4810, "int_enable_flag_pll_lock"},    /* Enable pll lock                                   , */\
{ 0x4820, "int_enable_flag_otpok"},    /* Enable OTP alarm                                  , */\
{ 0x4830, "int_enable_flag_ovpok"},    /* Enable OVP alarm                                  , */\
{ 0x4840, "int_enable_flag_uvpok"},    /* Enable UVP alarm                                  , */\
{ 0x4850, "int_enable_flag_clocks_stable"},    /* Enable clocks stable                              , */\
{ 0x4860, "int_enable_flag_mtp_busy"},    /* Enable mtp busy                                   , */\
{ 0x4870, "int_enable_flag_lost_clk"},    /* Enable lost clk                                   , */\
{ 0x48a0, "int_enable_flag_engage"},    /* Enable amplifier engage                           , */\
{ 0x48c0, "int_enable_flag_enbl_amp"},    /* Enable enbl amp                                   , */\
{ 0x48d0, "int_enable_flag_enbl_ref"},    /* Enable ref enable                                 , */\
{ 0x48e0, "int_enable_flag_adc10_ready"},    /* Enable Control ADC                                , */\
{ 0x4900, "int_enable_flag_bst_bstcur"},    /* Enable DCDC current limiting                      , */\
{ 0x4910, "int_enable_flag_bst_hiz"},    /* Enable DCDC active                                , */\
{ 0x4920, "int_enable_flag_bst_ocpok"},    /* Enable DCDC OCP                                   , */\
{ 0x4930, "int_enable_flag_bst_peakcur"},    /* Enable bst peakcur                                , */\
{ 0x4940, "int_enable_flag_bst_voutcomp"},    /* Enable DCDC level 1x                              , */\
{ 0x4950, "int_enable_flag_bst_voutcomp86"},    /* Enable DCDC level 1.14x                           , */\
{ 0x4960, "int_enable_flag_bst_voutcomp93"},    /* Enable DCDC level 1.07x                           , */\
{ 0x4990, "int_enable_flag_ocp_alarm"},    /* Enable ocp alarm                                  , */\
{ 0x49a0, "int_enable_flag_man_wait_src_settings"},    /* Enable waits HW I2C settings                      , */\
{ 0x49c0, "int_enable_flag_man_start_mute_audio"},    /* Enable man Audio mute sequence                    , */\
{ 0x49f0, "int_enable_flag_clk_out_of_range"},    /* Enable flag_clk_out_of_range                      , */\
{ 0x4a00, "int_enable_flag_tdm_error"},    /* Enable tdm error                                  , */\
{ 0x4a20, "int_enable_flag_clip"},    /* Enable clip                                       , */\
{ 0x4a40, "int_enable_flag_lp_detect_mode0"},    /* Enable low power mode0                            , */\
{ 0x4a50, "int_enable_flag_lp_detect_mode1"},    /* Enable low power mode1                            , */\
{ 0x4a60, "int_enable_flag_low_amplitude"},    /* Enable low noise detection                        , */\
{ 0x4a70, "int_enable_flag_vddp_gt_vbat"},    /* Enable VDDP greater tehn VBAT                     , */\
{ 0x4c00, "int_polarity_flag_por"},    /* Polarity por                                      , */\
{ 0x4c10, "int_polarity_flag_pll_lock"},    /* Polarity pll lock                                 , */\
{ 0x4c20, "int_polarity_flag_otpok"},    /* Polarity OTP alarm                                , */\
{ 0x4c30, "int_polarity_flag_ovpok"},    /* Polarity OVP alarm                                , */\
{ 0x4c40, "int_polarity_flag_uvpok"},    /* Polarity UVP alarm                                , */\
{ 0x4c50, "int_polarity_flag_clocks_stable"},    /* Polarity clocks stable                            , */\
{ 0x4c60, "int_polarity_flag_mtp_busy"},    /* Polarity mtp busy                                 , */\
{ 0x4c70, "int_polarity_flag_lost_clk"},    /* Polarity lost clk                                 , */\
{ 0x4ca0, "int_polarity_flag_engage"},    /* Polarity amplifier engage                         , */\
{ 0x4cc0, "int_polarity_flag_enbl_amp"},    /* Polarity enbl amp                                 , */\
{ 0x4cd0, "int_polarity_flag_enbl_ref"},    /* Polarity ref enable                               , */\
{ 0x4ce0, "int_polarity_flag_adc10_ready"},    /* Polarity Control ADC                              , */\
{ 0x4d00, "int_polarity_flag_bst_bstcur"},    /* Polarity DCDC current limiting                    , */\
{ 0x4d10, "int_polarity_flag_bst_hiz"},    /* Polarity DCDC active                              , */\
{ 0x4d20, "int_polarity_flag_bst_ocpok"},    /* Polarity DCDC OCP                                 , */\
{ 0x4d30, "int_polarity_flag_bst_peakcur"},    /* Polarity bst peakcur                              , */\
{ 0x4d40, "int_polarity_flag_bst_voutcomp"},    /* Polarity DCDC level 1x                            , */\
{ 0x4d50, "int_polarity_flag_bst_voutcomp86"},    /* Polarity DCDC level 1.14x                         , */\
{ 0x4d60, "int_polarity_flag_bst_voutcomp93"},    /* Polarity DCDC level 1.07x                         , */\
{ 0x4d90, "int_polarity_flag_ocp_alarm"},    /* Polarity ocp alarm                                , */\
{ 0x4da0, "int_polarity_flag_man_wait_src_settings"},    /* Polarity waits HW I2C settings                    , */\
{ 0x4dc0, "int_polarity_flag_man_start_mute_audio"},    /* Polarity man audio mute sequence                  , */\
{ 0x4df0, "int_polarity_flag_clk_out_of_range"},    /* Polarity flag_clk_out_of_range                    , */\
{ 0x4e00, "int_polarity_flag_tdm_error"},    /* Polarity tdm error                                , */\
{ 0x4e20, "int_polarity_flag_clip"},    /* Polarity clip right                               , */\
{ 0x4e40, "int_polarity_flag_lp_detect_mode0"},    /* Polarity low power mode0                          , */\
{ 0x4e50, "int_polarity_flag_lp_detect_mode1"},    /* Polarity low power mode1                          , */\
{ 0x4e60, "int_polarity_flag_low_amplitude"},    /* Polarity low noise mode                           , */\
{ 0x4e70, "int_polarity_flag_vddp_gt_vbat"},    /* Polarity VDDP greater than VBAT                   , */\
{ 0x5001, "vbat_prot_attack_time"},    /* Battery Safeguard attack time                     , */\
{ 0x5023, "vbat_prot_thlevel"},    /* Battery Safeguard threshold voltage level         , */\
{ 0x5061, "vbat_prot_max_reduct"},    /* Battery Safeguard maximum reduction               , */\
{ 0x50d0, "rst_min_vbat"},    /* Reset clipper - Auto clear                        , */\
{ 0x50e0, "sel_vbat"},    /* Battery voltage read out                          , */\
{ 0x50f0, "bypass_clipper"},    /* Bypass battery safeguard                          , */\
{ 0x5100, "batsense_steepness"},    /* Vbat prot steepness                               , */\
{ 0x5110, "soft_mute"},    /* Soft mute HW                                      , */\
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
{ 0x5420, "fb_hz"},    /* Feedback resistor set to high ohmic               , */\
{ 0x5430, "icomp_engage"},    /* Engage of icomp                                   , */\
{ 0x5440, "ctrl_kickback"},    /* Prevent double pulses of output stage             , */\
{ 0x5450, "icomp_engage_overrule"},    /* To overrule the functional icomp_engage signal during validation, */\
{ 0x5503, "ctrl_dem"},    /* Enable DEM icomp and DEM one bit dac              , */\
{ 0x5543, "ctrl_dem_mismatch"},    /* Enable DEM icomp mismatch for testing             , */\
{ 0x5582, "dpsa_drive"},    /* Drive setting (bin. coded)                        , */\
{ 0x570a, "enbl_amp"},    /* Switch on the class-D power sections, each part of the analog sections can be switched on/off individually, */\
{ 0x57b0, "enbl_engage"},    /* Enables/engage power stage and control loop       , */\
{ 0x57c0, "enbl_engage_pst"},    /* Enables/engage power stage and control loop       , */\
{ 0x5810, "hard_mute"},    /* Hard mute - PWM                                   , */\
{ 0x5820, "pwm_shape"},    /* PWM shape                                         , */\
{ 0x5844, "pwm_delay"},    /* PWM delay bits to set the delay, clockd is 1/(k*2048*fs), */\
{ 0x5890, "reclock_pwm"},    /* Reclock the pwm signal inside analog              , */\
{ 0x58a0, "reclock_voltsense"},    /* Reclock the voltage sense pwm signal              , */\
{ 0x58c0, "enbl_pwm_phase_shift"},    /* Control for pwm phase shift                       , */\
{ 0x6081, "pga_gain_set"},    /* PGA gain selection                                , */\
{ 0x60b0, "pga_lowpass_enable"},    /* Lowpass enable                                    , */\
{ 0x60c0, "pga_pwr_enable"},    /* Power enable, directcontrol mode only             , */\
{ 0x60d0, "pga_switch_enable"},    /* Switch enable, directcontrol mode only            , */\
{ 0x60e0, "pga_switch_aux_enable"},    /* Switch enable aux, directcontrol mode only        , */\
{ 0x6100, "force_idle"},    /* force low power in idle mode                      , */\
{ 0x6110, "bypass_idle"},    /* bypass low power idle mode                        , */\
{ 0x6123, "ctrl_attl"},    /* Second channel gain in case of stereo using a single coil. (Total gain depending on INPLEV). (In case of mono OR stereo using 2 separate DCDC channel 1 should be disabled using TDMDCE), */\
{ 0x6163, "ctrl_attr"},    /* Total gain depending on INPLEV setting (channel 0), */\
{ 0x61a0, "idle_cnt"},    /* idle counter                                      , */\
{ 0x61b0, "enbl_idle_ch1"},    /* enable idle feature for channel 1                 , */\
{ 0x6265, "zero_lvl"},    /* low noise gain switch zero trigger level          , */\
{ 0x62c1, "ctrl_fb_classd"},    /* class D gain ctrl_fb_50k ctrl_fb_100k             , */\
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
{ 0x6801, "tdm_source_mapping"},    /* tdm source mapping                                , */\
{ 0x6821, "tdm_sourcea_frame_sel"},    /* Sensed value  A                                   , */\
{ 0x6841, "tdm_sourceb_frame_sel"},    /* Sensed value  B                                   , */\
{ 0x6881, "pdm_anc_sel"},    /* anc input                                         , */\
{ 0x68a0, "anc_1scomplement"},    /* ANC one s complement                              , */\
{ 0x6901, "sam_mode"},    /* sam enable                                        , */\
{ 0x6920, "sam_src"},    /* sam source                                        , */\
{ 0x6931, "pdmdat_h_sel"},    /* pdm out value when pdm_clk is higth               , */\
{ 0x6951, "pdmdat_l_sel"},    /* pdm out value when pdm_clk is low                 , */\
{ 0x6970, "sam_spkr_sel"},    /* ram output on mode sam and audio                  , */\
{ 0x6a02, "rst_min_vbat_delay"},    /* rst_min_vbat delay (nb fs)                        , */\
{ 0x6b00, "disable_auto_engage"},    /* disable auto engange                              , */\
{ 0x6b10, "sel_tdm_data_valid"},    /* select tdm valid for speaker subsystem            , */\
{ 0x6c02, "ns_hp2ln_criterion"},    /* 0..7 zeroes at ns as threshold to swap from high_power to low_noise, */\
{ 0x6c32, "ns_ln2hp_criterion"},    /* 0..7 zeroes at ns as threshold to swap from low_noise to high_power, */\
{ 0x6c69, "spare_out"},    /* spare_out                                         , */\
{ 0x6d0f, "spare_in"},    /* spare_in                                          , */\
{ 0x6e00, "flag_lp_detect_mode0"},    /* low power mode 0 detection                        , */\
{ 0x6e10, "flag_lp_detect_mode1"},    /* low power mode 1 detection                        , */\
{ 0x6e20, "flag_low_amplitude"},    /* low amplitude detection                           , */\
{ 0x6e30, "flag_vddp_gt_vbat"},    /* vddp greater than vbat                            , */\
{ 0x6f02, "cursense_comp_delay"},    /* delay to allign compensation signal with current sense signal, */\
{ 0x6f40, "cursense_comp_sign"},    /* polarity of compensation for current sense        , */\
{ 0x6f50, "enbl_cursense_comp"},    /* enable current sense compensation                 , */\
{ 0x6f60, "sel_clip_pwms"},    /* Select pwm clip flag                              , */\
{ 0x6f72, "pwms_clip_lvl"},    /* set the amount of pwm pulse that may be skipped before clip-flag is triggered, */\
{ 0x7002, "scnd_boost_voltage"},    /* Second boost voltage level                        , */\
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
{ 0x7402, "frst_boost_voltage"},    /* 1st boost voltage level                           , */\
{ 0x7430, "boost_track"},    /* Boost algorithm selection, effective only when boost_intelligent is set to 1, */\
{ 0x7444, "boost_trip_lvl_1st"},    /* 1st Adaptive boost trip levels, effective only when DCIE is set to 1, */\
{ 0x7494, "boost_hold_time"},    /* Hold time for DCDC booster, effective only when boost_intelligent is set to 1, */\
{ 0x74e0, "sel_dcdc_envelope_8fs"},    /* Selection of data for adaptive boost algorithm, effective only when boost_intelligent is set to 1, */\
{ 0x74f0, "ignore_flag_voutcomp86"},    /* Ignore flag_voutcomp86                            , */\
{ 0x7502, "track_decay"},    /* DCDC Boost decay speed after a peak value, effective only when boost_track is set to 1, */\
{ 0x7534, "boost_trip_lvl_2nd"},    /* 2nd Adaptive boost trip levels, effective only when DCIE is set to 1, */\
{ 0x7584, "boost_trip_lvl_track"},    /* Track Adaptive boost trip levels, effective only when boost_intelligent is set to 1, */\
{ 0x7620, "pga_test_ldo_bypass"},    /* bypass internal PGA LDO                           , */\
{ 0x8001, "sel_clk_cs"},    /* Current sense clock duty cycle control            , */\
{ 0x8021, "micadc_speed"},    /* Current sense clock for MiCADC selection - 32/44.1/48 KHz Fs band only, */\
{ 0x8050, "cs_gain_control"},    /* Current sense gain control                        , */\
{ 0x8060, "cs_bypass_gc"},    /* Bypasses the CS gain correction                   , */\
{ 0x8087, "cs_gain"},    /* Current sense gain                                , */\
{ 0x8200, "enbl_cmfb"},    /* Current sense common mode feedback control        , */\
{ 0x8210, "invertpwm"},    /* Current sense common mode feedback pwm invert control, */\
{ 0x8222, "cmfb_gain"},    /* Current sense common mode feedback control gain   , */\
{ 0x8254, "cmfb_offset"},    /* Current sense common mode feedback control offset , */\
{ 0x82a0, "cs_sam_set"},    /* Enable SAM input for current sense                , */\
{ 0x8305, "cs_ktemp"},    /* Current sense temperature compensation trimming (1 - VALUE*TEMP)*signal, */\
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
{ 0x8760, "enbl_cs_nofloating_n"},    /* Connect current sense negative to gnda at transitions of booster or classd amplifiers. Otherwise floating (0), */\
{ 0x8770, "enbl_cs_nofloating_p"},    /* Connect current sense positive to gnda at transitions of booster or classd amplifiers. Otherwise floating (0), */\
{ 0x8780, "enbl_cs_vbatldo"},    /* Enable of current sense LDO                       , */\
{ 0x8800, "volsense_pwm_sel"},    /* Voltage sense PWM source selection control        , */\
{ 0x8810, "vol_cur_sense_dc_offset"},    /* voltage and current sense decimator offset control, */\
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
{ 0xc5b0, "pga_test_enable"},    /* Enable PGA test mode                              , */\
{ 0xc5c0, "pga_test_offset_enable"},    /* Enable PGA test offset                            , */\
{ 0xc5d0, "pga_test_shortinput_enable"},    /* Enable PGA test shortinput                        , */\
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
{ 0xcb54, "anamux3"},    /* Anamux selection control - anamux on TEST3        , */\
{ 0xcba4, "anamux4"},    /* Anamux selection control - anamux on TEST4        , */\
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
{ 0xd701, "pdmdat_ehs"},    /* Speed/load setting for PDMDAT  IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
{ 0xd721, "datao_ehs"},    /* Speed/load setting for DATAO  IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
{ 0xd740, "bck_ehs"},    /* Speed/load setting for DATAO  IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
{ 0xd750, "datai_ehs"},    /* Speed/load setting for DATAO  IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
{ 0xd760, "pdmclk_ehs"},    /* Speed/load setting for DATAO  IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
{ 0xd800, "source_in_testmode"},    /* tdm source in test mode (return only current and voltage sense), */\
{ 0xd810, "gainatt_feedback"},    /* gainatt feedback to tdm                           , */\
{ 0xd822, "test_parametric_io"},    /* test io parametric                                , */\
{ 0xd850, "ctrl_bst_clk_lp1"},    /* boost clock control in low power mode1            , */\
{ 0xd861, "test_spare_out1"},    /* test spare out 1                                  , */\
{ 0xd880, "bst_dcmbst"},    /* dcm boost                                         , */\
{ 0xd890, "pdm_loopback"},    /* pdm loop back to tdm                              , */\
{ 0xd8a1, "force_pga_clock"},    /* force pga clock                                   , */\
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
{ 0xf163, "spare_mpt1_9_6"},    /* HW gain module - left channel (2's complement)    , */\
{ 0xf1a5, "spare_mpt1_15_10"},    /* Offset for amplifier, HW gain module - left channel (2's complement), */\
{ 0xf203, "calibr_gain"},    /* HW gain module  (2's complement)                  , */\
{ 0xf245, "calibr_offset"},    /* Offset for amplifier, HW gain module (2's complement), */\
{ 0xf2a3, "spare_mpt2_13_10"},    /* Trimming of LDO (2.7V)                            , */\
{ 0xf307, "spare_mpt3_7_0"},    /* SPARE                                             , */\
{ 0xf387, "calibr_gain_cs"},    /* Current sense gain (signed two's complement format), */\
{ 0xf40f, "spare_mtp4_15_0"},    /* SPARE                                             , */\
{ 0xf50f, "calibr_R25C_R"},    /* Ron resistance of  speaker coil                   , */\
{ 0xf606, "spare_mpt6_6_0"},    /* SPARE                                             , */\
{ 0xf686, "spare_mpt6_14_8"},    /* Offset of left amplifier level shifter B          , */\
{ 0xf706, "ctrl_offset_a"},    /* Offset of  level shifter A                        , */\
{ 0xf786, "ctrl_offset_b"},    /* Offset of  amplifier level shifter B              , */\
{ 0xf806, "htol_iic_addr"},    /* 7-bit I2C address to be used during HTOL testing  , */\
{ 0xf870, "htol_iic_addr_en"},    /* HTOL I2C address enable control                   , */\
{ 0xf884, "calibr_temp_offset"},    /* Temperature offset 2's compliment (key1 protected), */\
{ 0xf8d2, "calibr_temp_gain"},    /* Temperature gain 2's compliment (key1 protected)  , */\
{ 0xf900, "mtp_lock_dcdcoff_mode"},    /* Disable function dcdcoff_mode                     , */\
{ 0xf910, "disable_sam_mode"},    /* Disable same mode                                 , */\
{ 0xf920, "mtp_lock_bypass_clipper"},    /* Disable function bypass_clipper                   , */\
{ 0xf930, "mtp_lock_max_dcdc_voltage"},    /* Disable programming of max dcdc boost voltage     , */\
{ 0xf943, "calibr_vbg_trim"},    /* Bandgap trimming control                          , */\
{ 0xf980, "mtp_enbl_amp_in_state_alarm"},    /* Enbl_amp in alarm state                           , */\
{ 0xf990, "mtp_enbl_pwm_delay_clock_gating"},    /* pwm delay clock auto gating                       , */\
{ 0xf9a0, "mtp_enbl_ocp_clock_gating"},    /* ocpclock auto gating                              , */\
{ 0xf9b0, "mtp_gate_cgu_clock_for_test"},    /* cgu test clock control                            , */\
{ 0xf9c3, "spare_mtp9_15_12"},    /* MTP-control FW - See Firmware I2C API document for details, */\
{ 0xfa0f, "mtpdataA"},    /* MTPdataA (key1 protected)                         , */\
{ 0xfb0f, "mtpdataB"},    /* MTPdataB (key1 protected)                         , */\
{ 0xfc0f, "mtpdataC"},    /* MTPdataC (key1 protected)                         , */\
{ 0xfd0f, "mtpdataD"},    /* MTPdataD (key1 protected)                         , */\
{ 0xfe0f, "mtpdataE"},    /* MTPdataE (key1 protected)                         , */\
{ 0xff07, "calibr_osc_delta_ndiv"},    /* Calibration data for OSC1M, signed number representation, */\
{ 0xffff, "Unknown bitfield enum" }    /* not found */\
};

enum tfa9872_irq {
tfa9872_irq_stvdds = 0,
tfa9872_irq_stplls = 1,
tfa9872_irq_stotds = 2,
tfa9872_irq_stovds = 3,
tfa9872_irq_stuvds = 4,
tfa9872_irq_stclks = 5,
tfa9872_irq_stmtpb = 6,
tfa9872_irq_stnoclk = 7,
tfa9872_irq_stsws = 10,
tfa9872_irq_stamps = 12,
tfa9872_irq_starefs = 13,
tfa9872_irq_stadccr = 14,
tfa9872_irq_stbstcu = 16,
tfa9872_irq_stbsthi = 17,
tfa9872_irq_stbstoc = 18,
tfa9872_irq_stbstpkcur = 19,
tfa9872_irq_stbstvc = 20,
tfa9872_irq_stbst86 = 21,
tfa9872_irq_stbst93 = 22,
tfa9872_irq_stocpr = 25,
tfa9872_irq_stmwsrc = 26,
tfa9872_irq_stmwsmu = 28,
tfa9872_irq_stclkoor = 31,
tfa9872_irq_sttdmer = 32,
tfa9872_irq_stclpr = 34,
tfa9872_irq_stlp0 = 36,
tfa9872_irq_stlp1 = 37,
tfa9872_irq_stla = 38,
tfa9872_irq_stvddph = 39,
tfa9872_irq_max = 40,
tfa9872_irq_all = -1 /* all irqs */};

#define TFA9872_IRQ_NAMETABLE static tfaIrqName_t Tfa9872IrqNames[] = {\
{ 0, "STVDDS"},\
{ 1, "STPLLS"},\
{ 2, "STOTDS"},\
{ 3, "STOVDS"},\
{ 4, "STUVDS"},\
{ 5, "STCLKS"},\
{ 6, "STMTPB"},\
{ 7, "STNOCLK"},\
{ 8, "8"},\
{ 9, "9"},\
{ 10, "STSWS"},\
{ 11, "11"},\
{ 12, "STAMPS"},\
{ 13, "STAREFS"},\
{ 14, "STADCCR"},\
{ 15, "15"},\
{ 16, "STBSTCU"},\
{ 17, "STBSTHI"},\
{ 18, "STBSTOC"},\
{ 19, "STBSTPKCUR"},\
{ 20, "STBSTVC"},\
{ 21, "STBST86"},\
{ 22, "STBST93"},\
{ 23, "23"},\
{ 24, "24"},\
{ 25, "STOCPR"},\
{ 26, "STMWSRC"},\
{ 27, "27"},\
{ 28, "STMWSMU"},\
{ 29, "29"},\
{ 30, "30"},\
{ 31, "STCLKOOR"},\
{ 32, "STTDMER"},\
{ 33, "33"},\
{ 34, "STCLPR"},\
{ 35, "35"},\
{ 36, "STLP0"},\
{ 37, "STLP1"},\
{ 38, "STLA"},\
{ 39, "STVDDPH"},\
{ 40, "40"},\
};
#endif /* _TFA9872_TFAFIELDNAMES_H */
