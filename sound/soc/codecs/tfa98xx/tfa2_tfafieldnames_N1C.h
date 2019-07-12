/** Filename: Tfa98xx_TfaFieldnames.h
 *  This file was generated automatically on 09/01/15 at 09:40:28.
 *  Source file: TFA9888_N1C_I2C_regmap_V1.xlsx
 */

typedef enum nxpTfa2BfEnumList {
    TFA2_BF_PWDN  = 0x0000,    /*!< Powerdown selection                                */
    TFA2_BF_I2CR  = 0x0010,    /*!< I2C Reset - Auto clear                             */
    TFA2_BF_CFE   = 0x0020,    /*!< Enable CoolFlux                                    */
    TFA2_BF_AMPE  = 0x0030,    /*!< Activate Amplifier                                 */
    TFA2_BF_DCA   = 0x0040,    /*!< Activate DC-to-DC converter                        */
    TFA2_BF_SBSL  = 0x0050,    /*!< Coolflux configured                                */
    TFA2_BF_AMPC  = 0x0060,    /*!< CoolFlux controls amplifier                        */
    TFA2_BF_INTP  = 0x0071,    /*!< Interrupt config                                   */
    TFA2_BF_FSSSEL = 0x0091,    /*!< Audio sample reference                             */
    TFA2_BF_BYPOCP = 0x00b0,    /*!< Bypass OCP                                         */
    TFA2_BF_TSTOCP = 0x00c0,    /*!< OCP testing control                                */
    TFA2_BF_AMPINSEL = 0x0101,    /*!< Amplifier input selection                          */
    TFA2_BF_MANSCONF = 0x0120,    /*!< I2C configured                                     */
    TFA2_BF_MANCOLD = 0x0130,    /*!< Execute cold start                                 */
    TFA2_BF_MANAOOSC = 0x0140,    /*!< Internal osc off at PWDN                           */
    TFA2_BF_MANROBOD = 0x0150,    /*!< Reaction on BOD                                    */
    TFA2_BF_BODE  = 0x0160,    /*!< BOD Enable                                         */
    TFA2_BF_BODHYS = 0x0170,    /*!< BOD Hysteresis                                     */
    TFA2_BF_BODFILT = 0x0181,    /*!< BOD filter                                         */
    TFA2_BF_BODTHLVL = 0x01a1,    /*!< BOD threshold                                      */
    TFA2_BF_MUTETO = 0x01d0,    /*!< Time out SB mute sequence                          */
    TFA2_BF_RCVNS = 0x01e0,    /*!< Noise shaper selection                             */
    TFA2_BF_MANWDE = 0x01f0,    /*!< Watchdog manager reaction                          */
    TFA2_BF_AUDFS = 0x0203,    /*!< Sample rate (fs)                                   */
    TFA2_BF_INPLEV = 0x0240,    /*!< TDM output attenuation                             */
    TFA2_BF_FRACTDEL = 0x0255,    /*!< V/I Fractional delay                               */
    TFA2_BF_BYPHVBF = 0x02b0,    /*!< Bypass HVBAT filter                                */
    TFA2_BF_LDOBYP = 0x02c0,    /*!< Receiver LDO bypass                                */
    TFA2_BF_REV   = 0x030f,    /*!< Revision info                                      */
    TFA2_BF_REFCKEXT = 0x0401,    /*!< PLL external ref clock                             */
    TFA2_BF_REFCKSEL = 0x0420,    /*!< PLL internal ref clock                             */
    TFA2_BF_SSLEFTE = 0x0500,    /*!< Enable left channel                                */
    TFA2_BF_SSRIGHTE = 0x0510,    /*!< Enable right channel                               */
    TFA2_BF_VSLEFTE = 0x0520,    /*!< Voltage sense left                                 */
    TFA2_BF_VSRIGHTE = 0x0530,    /*!< Voltage sense right                                */
    TFA2_BF_CSLEFTE = 0x0540,    /*!< Current sense left                                 */
    TFA2_BF_CSRIGHTE = 0x0550,    /*!< Current sense right                                */
    TFA2_BF_SSPDME = 0x0560,    /*!< Sub-system PDM                                     */
    TFA2_BF_STGAIN = 0x0d18,    /*!< Side tone gain                                     */
    TFA2_BF_PDMSMUTE = 0x0da0,    /*!< Side tone soft mute                                */
    TFA2_BF_SWVSTEP = 0x0e06,    /*!< Register for the host SW to record the current active vstep */
    TFA2_BF_VDDS  = 0x1000,    /*!< POR                                                */
    TFA2_BF_PLLS  = 0x1010,    /*!< PLL lock                                           */
    TFA2_BF_OTDS  = 0x1020,    /*!< OTP alarm                                          */
    TFA2_BF_OVDS  = 0x1030,    /*!< OVP alarm                                          */
    TFA2_BF_UVDS  = 0x1040,    /*!< UVP alarm                                          */
    TFA2_BF_CLKS  = 0x1050,    /*!< Clocks stable                                      */
    TFA2_BF_MTPB  = 0x1060,    /*!< MTP busy                                           */
    TFA2_BF_NOCLK = 0x1070,    /*!< Lost clock                                         */
    TFA2_BF_SPKS  = 0x1080,    /*!< Speaker error                                      */
    TFA2_BF_ACS   = 0x1090,    /*!< Cold Start                                         */
    TFA2_BF_SWS   = 0x10a0,    /*!< Amplifier engage                                   */
    TFA2_BF_WDS   = 0x10b0,    /*!< Watchdog                                           */
    TFA2_BF_AMPS  = 0x10c0,    /*!< Amplifier enable                                   */
    TFA2_BF_AREFS = 0x10d0,    /*!< References enable                                  */
    TFA2_BF_ADCCR = 0x10e0,    /*!< Control ADC                                        */
    TFA2_BF_BODNOK = 0x10f0,    /*!< BOD                                                */
    TFA2_BF_DCIL  = 0x1100,    /*!< DCDC current limiting                              */
    TFA2_BF_DCDCA = 0x1110,    /*!< DCDC active                                        */
    TFA2_BF_DCOCPOK = 0x1120,    /*!< DCDC OCP nmos                                      */
    TFA2_BF_DCHVBAT = 0x1140,    /*!< DCDC level 1x                                      */
    TFA2_BF_DCH114 = 0x1150,    /*!< DCDC level 1.14x                                   */
    TFA2_BF_DCH107 = 0x1160,    /*!< DCDC level 1.07x                                   */
    TFA2_BF_STMUTEB = 0x1170,    /*!< side tone (un)mute busy                            */
    TFA2_BF_STMUTE = 0x1180,    /*!< side tone mute state                               */
    TFA2_BF_TDMLUTER = 0x1190,    /*!< TDM LUT error                                      */
    TFA2_BF_TDMSTAT = 0x11a2,    /*!< TDM status bits                                    */
    TFA2_BF_TDMERR = 0x11d0,    /*!< TDM error                                          */
    TFA2_BF_HAPTIC = 0x11e0,    /*!< Status haptic driver                               */
    TFA2_BF_OCPOAPL = 0x1200,    /*!< OCPOK pmos A left                                  */
    TFA2_BF_OCPOANL = 0x1210,    /*!< OCPOK nmos A left                                  */
    TFA2_BF_OCPOBPL = 0x1220,    /*!< OCPOK pmos B left                                  */
    TFA2_BF_OCPOBNL = 0x1230,    /*!< OCPOK nmos B left                                  */
    TFA2_BF_CLIPAHL = 0x1240,    /*!< Clipping A left to Vddp                            */
    TFA2_BF_CLIPALL = 0x1250,    /*!< Clipping A left to gnd                             */
    TFA2_BF_CLIPBHL = 0x1260,    /*!< Clipping B left to Vddp                            */
    TFA2_BF_CLIPBLL = 0x1270,    /*!< Clipping B left to gnd                             */
    TFA2_BF_OCPOAPRC = 0x1280,    /*!< OCPOK pmos A RCV                                   */
    TFA2_BF_OCPOANRC = 0x1290,    /*!< OCPOK nmos A RCV                                   */
    TFA2_BF_OCPOBPRC = 0x12a0,    /*!< OCPOK pmos B RCV                                   */
    TFA2_BF_OCPOBNRC = 0x12b0,    /*!< OCPOK nmos B RCV                                   */
    TFA2_BF_RCVLDOR = 0x12c0,    /*!< RCV LDO regulates                                  */
    TFA2_BF_RCVLDOBR = 0x12d0,    /*!< Receiver LDO ready                                 */
    TFA2_BF_OCDSL = 0x12e0,    /*!< OCP left amplifier                                 */
    TFA2_BF_CLIPSL = 0x12f0,    /*!< Amplifier left clipping                            */
    TFA2_BF_OCPOAPR = 0x1300,    /*!< OCPOK pmos A right                                 */
    TFA2_BF_OCPOANR = 0x1310,    /*!< OCPOK nmos A right                                 */
    TFA2_BF_OCPOBPR = 0x1320,    /*!< OCPOK pmos B right                                 */
    TFA2_BF_OCPOBNR = 0x1330,    /*!< OCPOK nmos B right                                 */
    TFA2_BF_CLIPAHR = 0x1340,    /*!< Clipping A right to Vddp                           */
    TFA2_BF_CLIPALR = 0x1350,    /*!< Clipping A right to gnd                            */
    TFA2_BF_CLIPBHR = 0x1360,    /*!< Clipping B left to Vddp                            */
    TFA2_BF_CLIPBLR = 0x1370,    /*!< Clipping B right to gnd                            */
    TFA2_BF_OCDSR = 0x1380,    /*!< OCP right amplifier                                */
    TFA2_BF_CLIPSR = 0x1390,    /*!< Amplifier right clipping                           */
    TFA2_BF_OCPOKMC = 0x13a0,    /*!< OCPOK MICVDD                                       */
    TFA2_BF_MANALARM = 0x13b0,    /*!< Alarm state                                        */
    TFA2_BF_MANWAIT1 = 0x13c0,    /*!< Wait HW I2C settings                               */
    TFA2_BF_MANWAIT2 = 0x13d0,    /*!< Wait CF config                                     */
    TFA2_BF_MANMUTE = 0x13e0,    /*!< Audio mute sequence                                */
    TFA2_BF_MANOPER = 0x13f0,    /*!< Operating state                                    */
    TFA2_BF_SPKSL = 0x1400,    /*!< Left speaker status                                */
    TFA2_BF_SPKSR = 0x1410,    /*!< Right speaker status                               */
    TFA2_BF_CLKOOR = 0x1420,    /*!< External clock status                              */
    TFA2_BF_MANSTATE = 0x1433,    /*!< Device manager status                              */
    TFA2_BF_BATS  = 0x1509,    /*!< Battery voltage (V)                                */
    TFA2_BF_TEMPS = 0x1608,    /*!< IC Temperature (C)                                 */
    TFA2_BF_TDMUC = 0x2003,    /*!< Usecase setting                                    */
    TFA2_BF_TDME  = 0x2040,    /*!< Enable interface                                   */
    TFA2_BF_TDMMODE = 0x2050,    /*!< Slave/master                                       */
    TFA2_BF_TDMCLINV = 0x2060,    /*!< Reception data to BCK clock                        */
    TFA2_BF_TDMFSLN = 0x2073,    /*!< FS length (master mode only)                       */
    TFA2_BF_TDMFSPOL = 0x20b0,    /*!< FS polarity                                        */
    TFA2_BF_TDMNBCK = 0x20c3,    /*!< N-BCK's in FS                                      */
    TFA2_BF_TDMSLOTS = 0x2103,    /*!< N-slots in Frame                                   */
    TFA2_BF_TDMSLLN = 0x2144,    /*!< N-bits in slot                                     */
    TFA2_BF_TDMBRMG = 0x2194,    /*!< N-bits remaining                                   */
    TFA2_BF_TDMDEL = 0x21e0,    /*!< data delay to FS                                   */
    TFA2_BF_TDMADJ = 0x21f0,    /*!< data adjustment                                    */
    TFA2_BF_TDMOOMP = 0x2201,    /*!< Received audio compression                         */
    TFA2_BF_TDMSSIZE = 0x2224,    /*!< Sample size per slot                               */
    TFA2_BF_TDMTXDFO = 0x2271,    /*!< Format unused bits                                 */
    TFA2_BF_TDMTXUS0 = 0x2291,    /*!< Format unused slots GAINIO                         */
    TFA2_BF_TDMTXUS1 = 0x22b1,    /*!< Format unused slots DIO1                           */
    TFA2_BF_TDMTXUS2 = 0x22d1,    /*!< Format unused slots DIO2                           */
    TFA2_BF_TDMLE = 0x2310,    /*!< Control audio left                                 */
    TFA2_BF_TDMRE = 0x2320,    /*!< Control audio right                                */
    TFA2_BF_TDMVSRE = 0x2340,    /*!< Control voltage sense right                        */
    TFA2_BF_TDMCSRE = 0x2350,    /*!< Control current sense right                        */
    TFA2_BF_TDMVSLE = 0x2360,    /*!< Voltage sense left control                         */
    TFA2_BF_TDMCSLE = 0x2370,    /*!< Current sense left control                         */
    TFA2_BF_TDMCFRE = 0x2380,    /*!< DSP out right control                              */
    TFA2_BF_TDMCFLE = 0x2390,    /*!< DSP out left control                               */
    TFA2_BF_TDMCF3E = 0x23a0,    /*!< AEC ref left control                               */
    TFA2_BF_TDMCF4E = 0x23b0,    /*!< AEC ref right control                              */
    TFA2_BF_TDMPD1E = 0x23c0,    /*!< PDM 1 control                                      */
    TFA2_BF_TDMPD2E = 0x23d0,    /*!< PDM 2 control                                      */
    TFA2_BF_TDMLIO = 0x2421,    /*!< IO audio left                                      */
    TFA2_BF_TDMRIO = 0x2441,    /*!< IO audio right                                     */
    TFA2_BF_TDMVSRIO = 0x2481,    /*!< IO voltage sense right                             */
    TFA2_BF_TDMCSRIO = 0x24a1,    /*!< IO current sense right                             */
    TFA2_BF_TDMVSLIO = 0x24c1,    /*!< IO voltage sense left                              */
    TFA2_BF_TDMCSLIO = 0x24e1,    /*!< IO current sense left                              */
    TFA2_BF_TDMCFRIO = 0x2501,    /*!< IO dspout right                                    */
    TFA2_BF_TDMCFLIO = 0x2521,    /*!< IO dspout left                                     */
    TFA2_BF_TDMCF3IO = 0x2541,    /*!< IO AEC ref left control                            */
    TFA2_BF_TDMCF4IO = 0x2561,    /*!< IO AEC ref right control                           */
    TFA2_BF_TDMPD1IO = 0x2581,    /*!< IO pdm1                                            */
    TFA2_BF_TDMPD2IO = 0x25a1,    /*!< IO pdm2                                            */
    TFA2_BF_TDMLS = 0x2643,    /*!< Position audio left                                */
    TFA2_BF_TDMRS = 0x2683,    /*!< Position audio right                               */
    TFA2_BF_TDMVSRS = 0x2703,    /*!< Position voltage sense right                       */
    TFA2_BF_TDMCSRS = 0x2743,    /*!< Position current sense right                       */
    TFA2_BF_TDMVSLS = 0x2783,    /*!< Position voltage sense left                        */
    TFA2_BF_TDMCSLS = 0x27c3,    /*!< Position current sense left                        */
    TFA2_BF_TDMCFRS = 0x2803,    /*!< Position dspout right                              */
    TFA2_BF_TDMCFLS = 0x2843,    /*!< Position dspout left                               */
    TFA2_BF_TDMCF3S = 0x2883,    /*!< Position AEC ref left control                      */
    TFA2_BF_TDMCF4S = 0x28c3,    /*!< Position AEC ref right control                     */
    TFA2_BF_TDMPD1S = 0x2903,    /*!< Position pdm1                                      */
    TFA2_BF_TDMPD2S = 0x2943,    /*!< Position pdm2                                      */
    TFA2_BF_PDMSM = 0x3100,    /*!< PDM control                                        */
    TFA2_BF_PDMSTSEL = 0x3111,    /*!< Side tone input                                    */
    TFA2_BF_PDMLSEL = 0x3130,    /*!< PDM data selection for left channel during PDM direct mode */
    TFA2_BF_PDMRSEL = 0x3140,    /*!< PDM data selection for right channel during PDM direct mode */
    TFA2_BF_MICVDDE = 0x3150,    /*!< Enable MICVDD                                      */
    TFA2_BF_PDMCLRAT = 0x3201,    /*!< PDM BCK/Fs ratio                                   */
    TFA2_BF_PDMGAIN = 0x3223,    /*!< PDM gain                                           */
    TFA2_BF_PDMOSEL = 0x3263,    /*!< PDM output selection - RE/FE data combination      */
    TFA2_BF_SELCFHAPD = 0x32a0,    /*!< Select the source for haptic data output (not for customer) */
    TFA2_BF_HAPTIME = 0x3307,    /*!< Duration (ms)                                      */
    TFA2_BF_HAPLEVEL = 0x3387,    /*!< DC value (FFS)                                     */
    TFA2_BF_GPIODIN = 0x3403,    /*!< Receiving value                                    */
    TFA2_BF_GPIOCTRL = 0x3500,    /*!< GPIO master control over GPIO1/2 ports (not for customer) */
    TFA2_BF_GPIOCONF = 0x3513,    /*!< Configuration                                      */
    TFA2_BF_GPIODOUT = 0x3553,    /*!< Transmitting value                                 */
    TFA2_BF_ISTVDDS = 0x4000,    /*!< Status POR                                         */
    TFA2_BF_ISTPLLS = 0x4010,    /*!< Status PLL lock                                    */
    TFA2_BF_ISTOTDS = 0x4020,    /*!< Status OTP alarm                                   */
    TFA2_BF_ISTOVDS = 0x4030,    /*!< Status OVP alarm                                   */
    TFA2_BF_ISTUVDS = 0x4040,    /*!< Status UVP alarm                                   */
    TFA2_BF_ISTCLKS = 0x4050,    /*!< Status clocks stable                               */
    TFA2_BF_ISTMTPB = 0x4060,    /*!< Status MTP busy                                    */
    TFA2_BF_ISTNOCLK = 0x4070,    /*!< Status lost clock                                  */
    TFA2_BF_ISTSPKS = 0x4080,    /*!< Status speaker error                               */
    TFA2_BF_ISTACS = 0x4090,    /*!< Status cold start                                  */
    TFA2_BF_ISTSWS = 0x40a0,    /*!< Status amplifier engage                            */
    TFA2_BF_ISTWDS = 0x40b0,    /*!< Status watchdog                                    */
    TFA2_BF_ISTAMPS = 0x40c0,    /*!< Status amplifier enable                            */
    TFA2_BF_ISTAREFS = 0x40d0,    /*!< Status Ref enable                                  */
    TFA2_BF_ISTADCCR = 0x40e0,    /*!< Status Control ADC                                 */
    TFA2_BF_ISTBODNOK = 0x40f0,    /*!< Status BOD                                         */
    TFA2_BF_ISTBSTCU = 0x4100,    /*!< Status DCDC current limiting                       */
    TFA2_BF_ISTBSTHI = 0x4110,    /*!< Status DCDC active                                 */
    TFA2_BF_ISTBSTOC = 0x4120,    /*!< Status DCDC OCP                                    */
    TFA2_BF_ISTBSTPKCUR = 0x4130,    /*!< Status bst peakcur                                 */
    TFA2_BF_ISTBSTVC = 0x4140,    /*!< Status DCDC level 1x                               */
    TFA2_BF_ISTBST86 = 0x4150,    /*!< Status DCDC level 1.14x                            */
    TFA2_BF_ISTBST93 = 0x4160,    /*!< Status DCDC level 1.07x                            */
    TFA2_BF_ISTRCVLD = 0x4170,    /*!< Status rcvldop ready                               */
    TFA2_BF_ISTOCPL = 0x4180,    /*!< Status ocp alarm left                              */
    TFA2_BF_ISTOCPR = 0x4190,    /*!< Status ocp alarm right                             */
    TFA2_BF_ISTMWSRC = 0x41a0,    /*!< Status Waits HW I2C settings                       */
    TFA2_BF_ISTMWCFC = 0x41b0,    /*!< Status waits CF config                             */
    TFA2_BF_ISTMWSMU = 0x41c0,    /*!< Status Audio mute sequence                         */
    TFA2_BF_ISTCFMER = 0x41d0,    /*!< Status cfma error                                  */
    TFA2_BF_ISTCFMAC = 0x41e0,    /*!< Status cfma ack                                    */
    TFA2_BF_ISTCLKOOR = 0x41f0,    /*!< Status flag_clk_out_of_range                       */
    TFA2_BF_ISTTDMER = 0x4200,    /*!< Status tdm error                                   */
    TFA2_BF_ISTCLPL = 0x4210,    /*!< Status clip left                                   */
    TFA2_BF_ISTCLPR = 0x4220,    /*!< Status clip right                                  */
    TFA2_BF_ISTOCPM = 0x4230,    /*!< Status mic ocpok                                   */
    TFA2_BF_ICLVDDS = 0x4400,    /*!< Clear POR                                          */
    TFA2_BF_ICLPLLS = 0x4410,    /*!< Clear PLL lock                                     */
    TFA2_BF_ICLOTDS = 0x4420,    /*!< Clear OTP alarm                                    */
    TFA2_BF_ICLOVDS = 0x4430,    /*!< Clear OVP alarm                                    */
    TFA2_BF_ICLUVDS = 0x4440,    /*!< Clear UVP alarm                                    */
    TFA2_BF_ICLCLKS = 0x4450,    /*!< Clear clocks stable                                */
    TFA2_BF_ICLMTPB = 0x4460,    /*!< Clear mtp busy                                     */
    TFA2_BF_ICLNOCLK = 0x4470,    /*!< Clear lost clk                                     */
    TFA2_BF_ICLSPKS = 0x4480,    /*!< Clear speaker error                                */
    TFA2_BF_ICLACS = 0x4490,    /*!< Clear cold started                                 */
    TFA2_BF_ICLSWS = 0x44a0,    /*!< Clear amplifier engage                             */
    TFA2_BF_ICLWDS = 0x44b0,    /*!< Clear watchdog                                     */
    TFA2_BF_ICLAMPS = 0x44c0,    /*!< Clear enbl amp                                     */
    TFA2_BF_ICLAREFS = 0x44d0,    /*!< Clear ref enable                                   */
    TFA2_BF_ICLADCCR = 0x44e0,    /*!< Clear control ADC                                  */
    TFA2_BF_ICLBODNOK = 0x44f0,    /*!< Clear BOD                                          */
    TFA2_BF_ICLBSTCU = 0x4500,    /*!< Clear DCDC current limiting                        */
    TFA2_BF_ICLBSTHI = 0x4510,    /*!< Clear DCDC active                                  */
    TFA2_BF_ICLBSTOC = 0x4520,    /*!< Clear DCDC OCP                                     */
    TFA2_BF_ICLBSTPC = 0x4530,    /*!< Clear bst peakcur                                  */
    TFA2_BF_ICLBSTVC = 0x4540,    /*!< Clear DCDC level 1x                                */
    TFA2_BF_ICLBST86 = 0x4550,    /*!< Clear DCDC level 1.14x                             */
    TFA2_BF_ICLBST93 = 0x4560,    /*!< Clear DCDC level 1.07x                             */
    TFA2_BF_ICLRCVLD = 0x4570,    /*!< Clear rcvldop ready                                */
    TFA2_BF_ICLOCPL = 0x4580,    /*!< Clear ocp alarm left                               */
    TFA2_BF_ICLOCPR = 0x4590,    /*!< Clear ocp alarm right                              */
    TFA2_BF_ICLMWSRC = 0x45a0,    /*!< Clear wait HW I2C settings                         */
    TFA2_BF_ICLMWCFC = 0x45b0,    /*!< Clear wait cf config                               */
    TFA2_BF_ICLMWSMU = 0x45c0,    /*!< Clear audio mute sequence                          */
    TFA2_BF_ICLCFMER = 0x45d0,    /*!< Clear cfma err                                     */
    TFA2_BF_ICLCFMAC = 0x45e0,    /*!< Clear cfma ack                                     */
    TFA2_BF_ICLCLKOOR = 0x45f0,    /*!< Clear flag_clk_out_of_range                        */
    TFA2_BF_ICLTDMER = 0x4600,    /*!< Clear tdm error                                    */
    TFA2_BF_ICLCLPL = 0x4610,    /*!< Clear clip left                                    */
    TFA2_BF_ICLCLPR = 0x4620,    /*!< Clear clip right                                   */
    TFA2_BF_ICLOCPM = 0x4630,    /*!< Clear mic ocpok                                    */
    TFA2_BF_IEVDDS = 0x4800,    /*!< Enable por                                         */
    TFA2_BF_IEPLLS = 0x4810,    /*!< Enable pll lock                                    */
    TFA2_BF_IEOTDS = 0x4820,    /*!< Enable OTP alarm                                   */
    TFA2_BF_IEOVDS = 0x4830,    /*!< Enable OVP alarm                                   */
    TFA2_BF_IEUVDS = 0x4840,    /*!< Enable UVP alarm                                   */
    TFA2_BF_IECLKS = 0x4850,    /*!< Enable clocks stable                               */
    TFA2_BF_IEMTPB = 0x4860,    /*!< Enable mtp busy                                    */
    TFA2_BF_IENOCLK = 0x4870,    /*!< Enable lost clk                                    */
    TFA2_BF_IESPKS = 0x4880,    /*!< Enable speaker error                               */
    TFA2_BF_IEACS = 0x4890,    /*!< Enable cold started                                */
    TFA2_BF_IESWS = 0x48a0,    /*!< Enable amplifier engage                            */
    TFA2_BF_IEWDS = 0x48b0,    /*!< Enable watchdog                                    */
    TFA2_BF_IEAMPS = 0x48c0,    /*!< Enable enbl amp                                    */
    TFA2_BF_IEAREFS = 0x48d0,    /*!< Enable ref enable                                  */
    TFA2_BF_IEADCCR = 0x48e0,    /*!< Enable Control ADC                                 */
    TFA2_BF_IEBODNOK = 0x48f0,    /*!< Enable BOD                                         */
    TFA2_BF_IEBSTCU = 0x4900,    /*!< Enable DCDC current limiting                       */
    TFA2_BF_IEBSTHI = 0x4910,    /*!< Enable DCDC active                                 */
    TFA2_BF_IEBSTOC = 0x4920,    /*!< Enable DCDC OCP                                    */
    TFA2_BF_IEBSTPC = 0x4930,    /*!< Enable bst peakcur                                 */
    TFA2_BF_IEBSTVC = 0x4940,    /*!< Enable DCDC level 1x                               */
    TFA2_BF_IEBST86 = 0x4950,    /*!< Enable DCDC level 1.14x                            */
    TFA2_BF_IEBST93 = 0x4960,    /*!< Enable DCDC level 1.07x                            */
    TFA2_BF_IERCVLD = 0x4970,    /*!< Enable rcvldop ready                               */
    TFA2_BF_IEOCPL = 0x4980,    /*!< Enable ocp alarm left                              */
    TFA2_BF_IEOCPR = 0x4990,    /*!< Enable ocp alarm right                             */
    TFA2_BF_IEMWSRC = 0x49a0,    /*!< Enable waits HW I2C settings                       */
    TFA2_BF_IEMWCFC = 0x49b0,    /*!< Enable man wait cf config                          */
    TFA2_BF_IEMWSMU = 0x49c0,    /*!< Enable man Audio mute sequence                     */
    TFA2_BF_IECFMER = 0x49d0,    /*!< Enable cfma err                                    */
    TFA2_BF_IECFMAC = 0x49e0,    /*!< Enable cfma ack                                    */
    TFA2_BF_IECLKOOR = 0x49f0,    /*!< Enable flag_clk_out_of_range                       */
    TFA2_BF_IETDMER = 0x4a00,    /*!< Enable tdm error                                   */
    TFA2_BF_IECLPL = 0x4a10,    /*!< Enable clip left                                   */
    TFA2_BF_IECLPR = 0x4a20,    /*!< Enable clip right                                  */
    TFA2_BF_IEOCPM1 = 0x4a30,    /*!< Enable mic ocpok                                   */
    TFA2_BF_IPOVDDS = 0x4c00,    /*!< Polarity por                                       */
    TFA2_BF_IPOPLLS = 0x4c10,    /*!< Polarity pll lock                                  */
    TFA2_BF_IPOOTDS = 0x4c20,    /*!< Polarity OTP alarm                                 */
    TFA2_BF_IPOOVDS = 0x4c30,    /*!< Polarity OVP alarm                                 */
    TFA2_BF_IPOUVDS = 0x4c40,    /*!< Polarity UVP alarm                                 */
    TFA2_BF_IPOCLKS = 0x4c50,    /*!< Polarity clocks stable                             */
    TFA2_BF_IPOMTPB = 0x4c60,    /*!< Polarity mtp busy                                  */
    TFA2_BF_IPONOCLK = 0x4c70,    /*!< Polarity lost clk                                  */
    TFA2_BF_IPOSPKS = 0x4c80,    /*!< Polarity speaker error                             */
    TFA2_BF_IPOACS = 0x4c90,    /*!< Polarity cold started                              */
    TFA2_BF_IPOSWS = 0x4ca0,    /*!< Polarity amplifier engage                          */
    TFA2_BF_IPOWDS = 0x4cb0,    /*!< Polarity watchdog                                  */
    TFA2_BF_IPOAMPS = 0x4cc0,    /*!< Polarity enbl amp                                  */
    TFA2_BF_IPOAREFS = 0x4cd0,    /*!< Polarity ref enable                                */
    TFA2_BF_IPOADCCR = 0x4ce0,    /*!< Polarity Control ADC                               */
    TFA2_BF_IPOBODNOK = 0x4cf0,    /*!< Polarity BOD                                       */
    TFA2_BF_IPOBSTCU = 0x4d00,    /*!< Polarity DCDC current limiting                     */
    TFA2_BF_IPOBSTHI = 0x4d10,    /*!< Polarity DCDC active                               */
    TFA2_BF_IPOBSTOC = 0x4d20,    /*!< Polarity DCDC OCP                                  */
    TFA2_BF_IPOBSTPC = 0x4d30,    /*!< Polarity bst peakcur                               */
    TFA2_BF_IPOBSTVC = 0x4d40,    /*!< Polarity DCDC level 1x                             */
    TFA2_BF_IPOBST86 = 0x4d50,    /*!< Polarity DCDC level 1.14x                          */
    TFA2_BF_IPOBST93 = 0x4d60,    /*!< Polarity DCDC level 1.07x                          */
    TFA2_BF_IPORCVLD = 0x4d70,    /*!< Polarity rcvldop ready                             */
    TFA2_BF_IPOOCPL = 0x4d80,    /*!< Polarity ocp alarm left                            */
    TFA2_BF_IPOOCPR = 0x4d90,    /*!< Polarity ocp alarm right                           */
    TFA2_BF_IPOMWSRC = 0x4da0,    /*!< Polarity waits HW I2C settings                     */
    TFA2_BF_IPOMWCFC = 0x4db0,    /*!< Polarity man wait cf config                        */
    TFA2_BF_IPOMWSMU = 0x4dc0,    /*!< Polarity man audio mute sequence                   */
    TFA2_BF_IPOCFMER = 0x4dd0,    /*!< Polarity cfma err                                  */
    TFA2_BF_IPOCFMAC = 0x4de0,    /*!< Polarity cfma ack                                  */
    TFA2_BF_IPCLKOOR = 0x4df0,    /*!< Polarity flag_clk_out_of_range                     */
    TFA2_BF_IPOTDMER = 0x4e00,    /*!< Polarity tdm error                                 */
    TFA2_BF_IPOCLPL = 0x4e10,    /*!< Polarity clip left                                 */
    TFA2_BF_IPOCLPR = 0x4e20,    /*!< Polarity clip right                                */
    TFA2_BF_IPOOCPM = 0x4e30,    /*!< Polarity mic ocpok                                 */
    TFA2_BF_BSSCR = 0x5001,    /*!< Battery protection attack Time                     */
    TFA2_BF_BSST  = 0x5023,    /*!< Battery protection threshold voltage level         */
    TFA2_BF_BSSRL = 0x5061,    /*!< Battery protection maximum reduction               */
    TFA2_BF_BSSRR = 0x5082,    /*!< Battery protection release time                    */
    TFA2_BF_BSSHY = 0x50b1,    /*!< Battery protection hysteresis                      */
    TFA2_BF_BSSR  = 0x50e0,    /*!< Battery voltage read out                           */
    TFA2_BF_BSSBY = 0x50f0,    /*!< Bypass HW clipper                                  */
    TFA2_BF_BSSS  = 0x5100,    /*!< Vbat prot steepness                                */
    TFA2_BF_INTSMUTE = 0x5110,    /*!< Soft mute HW                                       */
    TFA2_BF_CFSML = 0x5120,    /*!< Soft mute FW left                                  */
    TFA2_BF_CFSMR = 0x5130,    /*!< Soft mute FW right                                 */
    TFA2_BF_HPFBYPL = 0x5140,    /*!< Bypass HPF left                                    */
    TFA2_BF_HPFBYPR = 0x5150,    /*!< Bypass HPF right                                   */
    TFA2_BF_DPSAL = 0x5160,    /*!< Enable DPSA left                                   */
    TFA2_BF_DPSAR = 0x5170,    /*!< Enable DPSA right                                  */
    TFA2_BF_VOL   = 0x5187,    /*!< FW volume control for primary audio channel        */
    TFA2_BF_HNDSFRCV = 0x5200,    /*!< Selection receiver                                 */
    TFA2_BF_CLIPCTRL = 0x5222,    /*!< Clip control setting                               */
    TFA2_BF_AMPGAIN = 0x5257,    /*!< Amplifier gain                                     */
    TFA2_BF_SLOPEE = 0x52d0,    /*!< Enables slope control                              */
    TFA2_BF_SLOPESET = 0x52e1,    /*!< Set slope                                          */
    TFA2_BF_VOLSEC = 0x5a07,    /*!< FW volume control for secondary audio channel      */
    TFA2_BF_SWPROFIL = 0x5a87,    /*!< Software profile data                              */
    TFA2_BF_DCVO  = 0x7002,    /*!< Boost voltage                                      */
    TFA2_BF_DCMCC = 0x7033,    /*!< Max coil current                                   */
    TFA2_BF_DCCV  = 0x7071,    /*!< Coil Value                                         */
    TFA2_BF_DCIE  = 0x7090,    /*!< Adaptive boost mode                                */
    TFA2_BF_DCSR  = 0x70a0,    /*!< Soft ramp up/down                                  */
    TFA2_BF_DCSYNCP = 0x70b2,    /*!< DCDC synchronization off + 7 positions             */
    TFA2_BF_DCDIS = 0x70e0,    /*!< DCDC on/off                                        */
    TFA2_BF_RST   = 0x9000,    /*!< Reset                                              */
    TFA2_BF_DMEM  = 0x9011,    /*!< Target memory                                      */
    TFA2_BF_AIF   = 0x9030,    /*!< Auto increment                                     */
    TFA2_BF_CFINT = 0x9040,    /*!< Interrupt - auto clear                             */
    TFA2_BF_CFCGATE = 0x9050,    /*!< Coolflux clock gating disabling control            */
    TFA2_BF_REQ   = 0x9087,    /*!< request for access (8 channels)                    */
    TFA2_BF_REQCMD = 0x9080,    /*!< Firmware event request rpc command                 */
    TFA2_BF_REQRST = 0x9090,    /*!< Firmware event request reset restart               */
    TFA2_BF_REQMIPS = 0x90a0,    /*!< Firmware event request short on mips               */
    TFA2_BF_REQMUTED = 0x90b0,    /*!< Firmware event request mute sequence ready         */
    TFA2_BF_REQVOL = 0x90c0,    /*!< Firmware event request volume ready                */
    TFA2_BF_REQDMG = 0x90d0,    /*!< Firmware event request speaker damage detected     */
    TFA2_BF_REQCAL = 0x90e0,    /*!< Firmware event request calibration completed       */
    TFA2_BF_REQRSV = 0x90f0,    /*!< Firmware event request reserved                    */
    TFA2_BF_MADD  = 0x910f,    /*!< Memory address                                     */
    TFA2_BF_MEMA  = 0x920f,    /*!< Activate memory access                             */
    TFA2_BF_ERR   = 0x9307,    /*!< Error flags                                        */
    TFA2_BF_ACK   = 0x9387,    /*!< Acknowledge of requests                            */
    TFA2_BF_ACKCMD = 0x9380,    /*!< Firmware event acknowledge rpc command             */
    TFA2_BF_ACKRST = 0x9390,    /*!< Firmware event acknowledge reset restart           */
    TFA2_BF_ACKMIPS = 0x93a0,    /*!< Firmware event acknowledge short on mips           */
    TFA2_BF_ACKMUTED = 0x93b0,    /*!< Firmware event acknowledge mute sequence ready     */
    TFA2_BF_ACKVOL = 0x93c0,    /*!< Firmware event acknowledge volume ready            */
    TFA2_BF_ACKDMG = 0x93d0,    /*!< Firmware event acknowledge speaker damage detected */
    TFA2_BF_ACKCAL = 0x93e0,    /*!< Firmware event acknowledge calibration completed   */
    TFA2_BF_ACKRSV = 0x93f0,    /*!< Firmware event acknowledge reserved                */
    TFA2_BF_MTPK  = 0xa107,    /*!< MTP KEY2 register                                  */
    TFA2_BF_KEY1LOCKED = 0xa200,    /*!< Indicates KEY1 is locked                           */
    TFA2_BF_KEY2LOCKED = 0xa210,    /*!< Indicates KEY2 is locked                           */
    TFA2_BF_CIMTP = 0xa360,    /*!< Start copying data from I2C mtp registers to mtp   */
    TFA2_BF_MTPRDMSB = 0xa50f,    /*!< MSB word of MTP manual read data                   */
    TFA2_BF_MTPRDLSB = 0xa60f,    /*!< LSB word of MTP manual read data                   */
    TFA2_BF_EXTTS = 0xb108,    /*!< External temperature (C)                           */
    TFA2_BF_TROS  = 0xb190,    /*!< Select temp Speaker calibration                    */
    TFA2_BF_MTPOTC = 0xf000,    /*!< Calibration schedule                               */
    TFA2_BF_MTPEX = 0xf010,    /*!< Calibration Ron executed                           */
    TFA2_BF_DCMCCAPI = 0xf020,    /*!< Calibration current limit DCDC                     */
    TFA2_BF_DCMCCSB = 0xf030,    /*!< Sign bit for delta calibration current limit DCDC  */
    TFA2_BF_USERDEF = 0xf042,    /*!< Calibration delta current limit DCDC               */
    TFA2_BF_R25CL = 0xf40f,    /*!< Ron resistance of left channel speaker coil        */
    TFA2_BF_R25CR = 0xf50f,    /*!< Ron resistance of right channel speaker coil       */
} nxpTfa2BfEnumList_t;
#define TFA2_NAMETABLE static tfaBfName_t Tfa2DatasheetNames[] = {\
   { 0x0, "PWDN"},    /* Powerdown selection                               , */\
   { 0x10, "I2CR"},    /* I2C Reset - Auto clear                            , */\
   { 0x20, "CFE"},    /* Enable CoolFlux                                   , */\
   { 0x30, "AMPE"},    /* Activate Amplifier                                , */\
   { 0x40, "DCA"},    /* Activate DC-to-DC converter                       , */\
   { 0x50, "SBSL"},    /* Coolflux configured                               , */\
   { 0x60, "AMPC"},    /* CoolFlux controls amplifier                       , */\
   { 0x71, "INTP"},    /* Interrupt config                                  , */\
   { 0x91, "FSSSEL"},    /* Audio sample reference                            , */\
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
   { 0x1f0, "MANWDE"},    /* Watchdog manager reaction                         , */\
   { 0x203, "AUDFS"},    /* Sample rate (fs)                                  , */\
   { 0x240, "INPLEV"},    /* TDM output attenuation                            , */\
   { 0x255, "FRACTDEL"},    /* V/I Fractional delay                              , */\
   { 0x2b0, "BYPHVBF"},    /* Bypass HVBAT filter                               , */\
   { 0x2c0, "LDOBYP"},    /* Receiver LDO bypass                               , */\
   { 0x30f, "REV"},    /* Revision info                                     , */\
   { 0x401, "REFCKEXT"},    /* PLL external ref clock                            , */\
   { 0x420, "REFCKSEL"},    /* PLL internal ref clock                            , */\
   { 0x500, "SSLEFTE"},    /* Enable left channel                               , */\
   { 0x510, "SSRIGHTE"},    /* Enable right channel                              , */\
   { 0x520, "VSLEFTE"},    /* Voltage sense left                                , */\
   { 0x530, "VSRIGHTE"},    /* Voltage sense right                               , */\
   { 0x540, "CSLEFTE"},    /* Current sense left                                , */\
   { 0x550, "CSRIGHTE"},    /* Current sense right                               , */\
   { 0x560, "SSPDME"},    /* Sub-system PDM                                    , */\
   { 0xd18, "STGAIN"},    /* Side tone gain                                    , */\
   { 0xda0, "PDMSMUTE"},    /* Side tone soft mute                               , */\
   { 0xe06, "SWVSTEP"},    /* Register for the host SW to record the current active vstep, */\
   { 0x1000, "VDDS"},    /* POR                                               , */\
   { 0x1010, "PLLS"},    /* PLL lock                                          , */\
   { 0x1020, "OTDS"},    /* OTP alarm                                         , */\
   { 0x1030, "OVDS"},    /* OVP alarm                                         , */\
   { 0x1040, "UVDS"},    /* UVP alarm                                         , */\
   { 0x1050, "CLKS"},    /* Clocks stable                                     , */\
   { 0x1060, "MTPB"},    /* MTP busy                                          , */\
   { 0x1070, "NOCLK"},    /* Lost clock                                        , */\
   { 0x1080, "SPKS"},    /* Speaker error                                     , */\
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
   { 0x1140, "DCHVBAT"},    /* DCDC level 1x                                     , */\
   { 0x1150, "DCH114"},    /* DCDC level 1.14x                                  , */\
   { 0x1160, "DCH107"},    /* DCDC level 1.07x                                  , */\
   { 0x1170, "STMUTEB"},    /* side tone (un)mute busy                           , */\
   { 0x1180, "STMUTE"},    /* side tone mute state                              , */\
   { 0x1190, "TDMLUTER"},    /* TDM LUT error                                     , */\
   { 0x11a2, "TDMSTAT"},    /* TDM status bits                                   , */\
   { 0x11d0, "TDMERR"},    /* TDM error                                         , */\
   { 0x11e0, "HAPTIC"},    /* Status haptic driver                              , */\
   { 0x1200, "OCPOAPL"},    /* OCPOK pmos A left                                 , */\
   { 0x1210, "OCPOANL"},    /* OCPOK nmos A left                                 , */\
   { 0x1220, "OCPOBPL"},    /* OCPOK pmos B left                                 , */\
   { 0x1230, "OCPOBNL"},    /* OCPOK nmos B left                                 , */\
   { 0x1240, "CLIPAHL"},    /* Clipping A left to Vddp                           , */\
   { 0x1250, "CLIPALL"},    /* Clipping A left to gnd                            , */\
   { 0x1260, "CLIPBHL"},    /* Clipping B left to Vddp                           , */\
   { 0x1270, "CLIPBLL"},    /* Clipping B left to gnd                            , */\
   { 0x1280, "OCPOAPRC"},    /* OCPOK pmos A RCV                                  , */\
   { 0x1290, "OCPOANRC"},    /* OCPOK nmos A RCV                                  , */\
   { 0x12a0, "OCPOBPRC"},    /* OCPOK pmos B RCV                                  , */\
   { 0x12b0, "OCPOBNRC"},    /* OCPOK nmos B RCV                                  , */\
   { 0x12c0, "RCVLDOR"},    /* RCV LDO regulates                                 , */\
   { 0x12d0, "RCVLDOBR"},    /* Receiver LDO ready                                , */\
   { 0x12e0, "OCDSL"},    /* OCP left amplifier                                , */\
   { 0x12f0, "CLIPSL"},    /* Amplifier left clipping                           , */\
   { 0x1300, "OCPOAPR"},    /* OCPOK pmos A right                                , */\
   { 0x1310, "OCPOANR"},    /* OCPOK nmos A right                                , */\
   { 0x1320, "OCPOBPR"},    /* OCPOK pmos B right                                , */\
   { 0x1330, "OCPOBNR"},    /* OCPOK nmos B right                                , */\
   { 0x1340, "CLIPAHR"},    /* Clipping A right to Vddp                          , */\
   { 0x1350, "CLIPALR"},    /* Clipping A right to gnd                           , */\
   { 0x1360, "CLIPBHR"},    /* Clipping B left to Vddp                           , */\
   { 0x1370, "CLIPBLR"},    /* Clipping B right to gnd                           , */\
   { 0x1380, "OCDSR"},    /* OCP right amplifier                               , */\
   { 0x1390, "CLIPSR"},    /* Amplifier right clipping                          , */\
   { 0x13a0, "OCPOKMC"},    /* OCPOK MICVDD                                      , */\
   { 0x13b0, "MANALARM"},    /* Alarm state                                       , */\
   { 0x13c0, "MANWAIT1"},    /* Wait HW I2C settings                              , */\
   { 0x13d0, "MANWAIT2"},    /* Wait CF config                                    , */\
   { 0x13e0, "MANMUTE"},    /* Audio mute sequence                               , */\
   { 0x13f0, "MANOPER"},    /* Operating state                                   , */\
   { 0x1400, "SPKSL"},    /* Left speaker status                               , */\
   { 0x1410, "SPKSR"},    /* Right speaker status                              , */\
   { 0x1420, "CLKOOR"},    /* External clock status                             , */\
   { 0x1433, "MANSTATE"},    /* Device manager status                             , */\
   { 0x1509, "BATS"},    /* Battery voltage (V)                               , */\
   { 0x1608, "TEMPS"},    /* IC Temperature (C)                                , */\
   { 0x2003, "TDMUC"},    /* Usecase setting                                   , */\
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
   { 0x2291, "TDMTXUS0"},    /* Format unused slots GAINIO                        , */\
   { 0x22b1, "TDMTXUS1"},    /* Format unused slots DIO1                          , */\
   { 0x22d1, "TDMTXUS2"},    /* Format unused slots DIO2                          , */\
   { 0x2310, "TDMLE"},    /* Control audio left                                , */\
   { 0x2320, "TDMRE"},    /* Control audio right                               , */\
   { 0x2340, "TDMVSRE"},    /* Control voltage sense right                       , */\
   { 0x2350, "TDMCSRE"},    /* Control current sense right                       , */\
   { 0x2360, "TDMVSLE"},    /* Voltage sense left control                        , */\
   { 0x2370, "TDMCSLE"},    /* Current sense left control                        , */\
   { 0x2380, "TDMCFRE"},    /* DSP out right control                             , */\
   { 0x2390, "TDMCFLE"},    /* DSP out left control                              , */\
   { 0x23a0, "TDMCF3E"},    /* AEC ref left control                              , */\
   { 0x23b0, "TDMCF4E"},    /* AEC ref right control                             , */\
   { 0x23c0, "TDMPD1E"},    /* PDM 1 control                                     , */\
   { 0x23d0, "TDMPD2E"},    /* PDM 2 control                                     , */\
   { 0x2421, "TDMLIO"},    /* IO audio left                                     , */\
   { 0x2441, "TDMRIO"},    /* IO audio right                                    , */\
   { 0x2481, "TDMVSRIO"},    /* IO voltage sense right                            , */\
   { 0x24a1, "TDMCSRIO"},    /* IO current sense right                            , */\
   { 0x24c1, "TDMVSLIO"},    /* IO voltage sense left                             , */\
   { 0x24e1, "TDMCSLIO"},    /* IO current sense left                             , */\
   { 0x2501, "TDMCFRIO"},    /* IO dspout right                                   , */\
   { 0x2521, "TDMCFLIO"},    /* IO dspout left                                    , */\
   { 0x2541, "TDMCF3IO"},    /* IO AEC ref left control                           , */\
   { 0x2561, "TDMCF4IO"},    /* IO AEC ref right control                          , */\
   { 0x2581, "TDMPD1IO"},    /* IO pdm1                                           , */\
   { 0x25a1, "TDMPD2IO"},    /* IO pdm2                                           , */\
   { 0x2643, "TDMLS"},    /* Position audio left                               , */\
   { 0x2683, "TDMRS"},    /* Position audio right                              , */\
   { 0x2703, "TDMVSRS"},    /* Position voltage sense right                      , */\
   { 0x2743, "TDMCSRS"},    /* Position current sense right                      , */\
   { 0x2783, "TDMVSLS"},    /* Position voltage sense left                       , */\
   { 0x27c3, "TDMCSLS"},    /* Position current sense left                       , */\
   { 0x2803, "TDMCFRS"},    /* Position dspout right                             , */\
   { 0x2843, "TDMCFLS"},    /* Position dspout left                              , */\
   { 0x2883, "TDMCF3S"},    /* Position AEC ref left control                     , */\
   { 0x28c3, "TDMCF4S"},    /* Position AEC ref right control                    , */\
   { 0x2903, "TDMPD1S"},    /* Position pdm1                                     , */\
   { 0x2943, "TDMPD2S"},    /* Position pdm2                                     , */\
   { 0x3100, "PDMSM"},    /* PDM control                                       , */\
   { 0x3111, "PDMSTSEL"},    /* Side tone input                                   , */\
   { 0x3130, "PDMLSEL"},    /* PDM data selection for left channel during PDM direct mode, */\
   { 0x3140, "PDMRSEL"},    /* PDM data selection for right channel during PDM direct mode, */\
   { 0x3150, "MICVDDE"},    /* Enable MICVDD                                     , */\
   { 0x3201, "PDMCLRAT"},    /* PDM BCK/Fs ratio                                  , */\
   { 0x3223, "PDMGAIN"},    /* PDM gain                                          , */\
   { 0x3263, "PDMOSEL"},    /* PDM output selection - RE/FE data combination     , */\
   { 0x32a0, "SELCFHAPD"},    /* Select the source for haptic data output (not for customer), */\
   { 0x3307, "HAPTIME"},    /* Duration (ms)                                     , */\
   { 0x3387, "HAPLEVEL"},    /* DC value (FFS)                                    , */\
   { 0x3403, "GPIODIN"},    /* Receiving value                                   , */\
   { 0x3500, "GPIOCTRL"},    /* GPIO master control over GPIO1/2 ports (not for customer), */\
   { 0x3513, "GPIOCONF"},    /* Configuration                                     , */\
   { 0x3553, "GPIODOUT"},    /* Transmitting value                                , */\
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
   { 0x4190, "ISTOCPR"},    /* Status ocp alarm right                            , */\
   { 0x41a0, "ISTMWSRC"},    /* Status Waits HW I2C settings                      , */\
   { 0x41b0, "ISTMWCFC"},    /* Status waits CF config                            , */\
   { 0x41c0, "ISTMWSMU"},    /* Status Audio mute sequence                        , */\
   { 0x41d0, "ISTCFMER"},    /* Status cfma error                                 , */\
   { 0x41e0, "ISTCFMAC"},    /* Status cfma ack                                   , */\
   { 0x41f0, "ISTCLKOOR"},    /* Status flag_clk_out_of_range                      , */\
   { 0x4200, "ISTTDMER"},    /* Status tdm error                                  , */\
   { 0x4210, "ISTCLPL"},    /* Status clip left                                  , */\
   { 0x4220, "ISTCLPR"},    /* Status clip right                                 , */\
   { 0x4230, "ISTOCPM"},    /* Status mic ocpok                                  , */\
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
   { 0x4590, "ICLOCPR"},    /* Clear ocp alarm right                             , */\
   { 0x45a0, "ICLMWSRC"},    /* Clear wait HW I2C settings                        , */\
   { 0x45b0, "ICLMWCFC"},    /* Clear wait cf config                              , */\
   { 0x45c0, "ICLMWSMU"},    /* Clear audio mute sequence                         , */\
   { 0x45d0, "ICLCFMER"},    /* Clear cfma err                                    , */\
   { 0x45e0, "ICLCFMAC"},    /* Clear cfma ack                                    , */\
   { 0x45f0, "ICLCLKOOR"},    /* Clear flag_clk_out_of_range                       , */\
   { 0x4600, "ICLTDMER"},    /* Clear tdm error                                   , */\
   { 0x4610, "ICLCLPL"},    /* Clear clip left                                   , */\
   { 0x4620, "ICLCLPR"},    /* Clear clip right                                  , */\
   { 0x4630, "ICLOCPM"},    /* Clear mic ocpok                                   , */\
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
   { 0x4990, "IEOCPR"},    /* Enable ocp alarm right                            , */\
   { 0x49a0, "IEMWSRC"},    /* Enable waits HW I2C settings                      , */\
   { 0x49b0, "IEMWCFC"},    /* Enable man wait cf config                         , */\
   { 0x49c0, "IEMWSMU"},    /* Enable man Audio mute sequence                    , */\
   { 0x49d0, "IECFMER"},    /* Enable cfma err                                   , */\
   { 0x49e0, "IECFMAC"},    /* Enable cfma ack                                   , */\
   { 0x49f0, "IECLKOOR"},    /* Enable flag_clk_out_of_range                      , */\
   { 0x4a00, "IETDMER"},    /* Enable tdm error                                  , */\
   { 0x4a10, "IECLPL"},    /* Enable clip left                                  , */\
   { 0x4a20, "IECLPR"},    /* Enable clip right                                 , */\
   { 0x4a30, "IEOCPM1"},    /* Enable mic ocpok                                  , */\
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
   { 0x4d90, "IPOOCPR"},    /* Polarity ocp alarm right                          , */\
   { 0x4da0, "IPOMWSRC"},    /* Polarity waits HW I2C settings                    , */\
   { 0x4db0, "IPOMWCFC"},    /* Polarity man wait cf config                       , */\
   { 0x4dc0, "IPOMWSMU"},    /* Polarity man audio mute sequence                  , */\
   { 0x4dd0, "IPOCFMER"},    /* Polarity cfma err                                 , */\
   { 0x4de0, "IPOCFMAC"},    /* Polarity cfma ack                                 , */\
   { 0x4df0, "IPCLKOOR"},    /* Polarity flag_clk_out_of_range                    , */\
   { 0x4e00, "IPOTDMER"},    /* Polarity tdm error                                , */\
   { 0x4e10, "IPOCLPL"},    /* Polarity clip left                                , */\
   { 0x4e20, "IPOCLPR"},    /* Polarity clip right                               , */\
   { 0x4e30, "IPOOCPM"},    /* Polarity mic ocpok                                , */\
   { 0x5001, "BSSCR"},    /* Battery protection attack Time                    , */\
   { 0x5023, "BSST"},    /* Battery protection threshold voltage level        , */\
   { 0x5061, "BSSRL"},    /* Battery protection maximum reduction              , */\
   { 0x5082, "BSSRR"},    /* Battery protection release time                   , */\
   { 0x50b1, "BSSHY"},    /* Battery protection hysteresis                     , */\
   { 0x50e0, "BSSR"},    /* Battery voltage read out                          , */\
   { 0x50f0, "BSSBY"},    /* Bypass HW clipper                                 , */\
   { 0x5100, "BSSS"},    /* Vbat prot steepness                               , */\
   { 0x5110, "INTSMUTE"},    /* Soft mute HW                                      , */\
   { 0x5120, "CFSML"},    /* Soft mute FW left                                 , */\
   { 0x5130, "CFSMR"},    /* Soft mute FW right                                , */\
   { 0x5140, "HPFBYPL"},    /* Bypass HPF left                                   , */\
   { 0x5150, "HPFBYPR"},    /* Bypass HPF right                                  , */\
   { 0x5160, "DPSAL"},    /* Enable DPSA left                                  , */\
   { 0x5170, "DPSAR"},    /* Enable DPSA right                                 , */\
   { 0x5187, "VOL"},    /* FW volume control for primary audio channel       , */\
   { 0x5200, "HNDSFRCV"},    /* Selection receiver                                , */\
   { 0x5222, "CLIPCTRL"},    /* Clip control setting                              , */\
   { 0x5257, "AMPGAIN"},    /* Amplifier gain                                    , */\
   { 0x52d0, "SLOPEE"},    /* Enables slope control                             , */\
   { 0x52e1, "SLOPESET"},    /* Set slope                                         , */\
   { 0x5a07, "VOLSEC"},    /* FW volume control for secondary audio channel     , */\
   { 0x5a87, "SWPROFIL"},    /* Software profile data                             , */\
   { 0x7002, "DCVO"},    /* Boost voltage                                     , */\
   { 0x7033, "DCMCC"},    /* Max coil current                                  , */\
   { 0x7071, "DCCV"},    /* Coil Value                                        , */\
   { 0x7090, "DCIE"},    /* Adaptive boost mode                               , */\
   { 0x70a0, "DCSR"},    /* Soft ramp up/down                                 , */\
   { 0x70b2, "DCSYNCP"},    /* DCDC synchronization off + 7 positions            , */\
   { 0x70e0, "DCDIS"},    /* DCDC on/off                                       , */\
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
   { 0x9387, "ACK"},    /* Acknowledge of requests                           , */\
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
   { 0xf000, "MTPOTC"},    /* Calibration schedule                              , */\
   { 0xf010, "MTPEX"},    /* Calibration Ron executed                          , */\
   { 0xf020, "DCMCCAPI"},    /* Calibration current limit DCDC                    , */\
   { 0xf030, "DCMCCSB"},    /* Sign bit for delta calibration current limit DCDC , */\
   { 0xf042, "USERDEF"},    /* Calibration delta current limit DCDC              , */\
   { 0xf40f, "R25CL"},    /* Ron resistance of left channel speaker coil       , */\
   { 0xf50f, "R25CR"},    /* Ron resistance of right channel speaker coil      , */\
   { 0xffff, "Unknown bitfield enum" }   /* not found */\
};

#define TFA2_BITNAMETABLE static tfaBfName_t Tfa2BitNames[] = {\
   { 0x0, "powerdown"},    /* Powerdown selection                               , */\
   { 0x10, "reset"},    /* I2C Reset - Auto clear                            , */\
   { 0x20, "enbl_coolflux"},    /* Enable CoolFlux                                   , */\
   { 0x30, "enbl_amplifier"},    /* Activate Amplifier                                , */\
   { 0x40, "enbl_boost"},    /* Activate DC-to-DC converter                       , */\
   { 0x50, "coolflux_configured"},    /* Coolflux configured                               , */\
   { 0x60, "sel_enbl_amplifier"},    /* CoolFlux controls amplifier                       , */\
   { 0x71, "int_pad_io"},    /* Interrupt config                                  , */\
   { 0x91, "fs_pulse_sel"},    /* Audio sample reference                            , */\
   { 0xb0, "bypass_ocp"},    /* Bypass OCP                                        , */\
   { 0xc0, "test_ocp"},    /* OCP testing control                               , */\
   { 0x101, "vamp_sel"},    /* Amplifier input selection                         , */\
   { 0x120, "src_set_configured"},    /* I2C configured                                    , */\
   { 0x130, "execute_cold_start"},    /* Execute cold start                                , */\
   { 0x140, "enbl_osc1m_auto_off"},    /* Internal osc off at PWDN                          , */\
   { 0x150, "man_enbl_brown_out"},    /* Reaction on BOD                                   , */\
   { 0x160, "enbl_bod"},    /* BOD Enable                                        , */\
   { 0x170, "enbl_bod_hyst"},    /* BOD Hysteresis                                    , */\
   { 0x181, "bod_delay"},    /* BOD filter                                        , */\
   { 0x1a1, "bod_lvlsel"},    /* BOD threshold                                     , */\
   { 0x1d0, "disable_mute_time_out"},    /* Time out SB mute sequence                         , */\
   { 0x1e0, "pwm_sel_rcv_ns"},    /* Noise shaper selection                            , */\
   { 0x1f0, "man_enbl_watchdog"},    /* Watchdog manager reaction                         , */\
   { 0x203, "audio_fs"},    /* Sample rate (fs)                                  , */\
   { 0x240, "input_level"},    /* TDM output attenuation                            , */\
   { 0x255, "cs_frac_delay"},    /* V/I Fractional delay                              , */\
   { 0x2b0, "bypass_hvbat_filter"},    /* Bypass HVBAT filter                               , */\
   { 0x2c0, "ctrl_rcvldop_bypass"},    /* Receiver LDO bypass                               , */\
   { 0x30f, "device_rev"},    /* Revision info                                     , */\
   { 0x401, "pll_clkin_sel"},    /* PLL external ref clock                            , */\
   { 0x420, "pll_clkin_sel_osc"},    /* PLL internal ref clock                            , */\
   { 0x500, "enbl_spkr_ss_left"},    /* Enable left channel                               , */\
   { 0x510, "enbl_spkr_ss_right"},    /* Enable right channel                              , */\
   { 0x520, "enbl_volsense_left"},    /* Voltage sense left                                , */\
   { 0x530, "enbl_volsense_right"},    /* Voltage sense right                               , */\
   { 0x540, "enbl_cursense_left"},    /* Current sense left                                , */\
   { 0x550, "enbl_cursense_right"},    /* Current sense right                               , */\
   { 0x560, "enbl_pdm_ss"},    /* Sub-system PDM                                    , */\
   { 0xd00, "side_tone_gain_sel"},    /* PDM side tone gain selector                       , */\
   { 0xd18, "side_tone_gain"},    /* Side tone gain                                    , */\
   { 0xda0, "mute_side_tone"},    /* Side tone soft mute                               , */\
   { 0xe06, "ctrl_digtoana"},    /* Register for the host SW to record the current active vstep, */\
   { 0xe70, "enbl_cmfb_left"},    /* Current sense common mode feedback control for left channel, */\
   { 0xf0f, "hidden_code"},    /* 5A6Bh, 23147d to access registers (default for engineering), */\
   { 0x1000, "flag_por"},    /* POR                                               , */\
   { 0x1010, "flag_pll_lock"},    /* PLL lock                                          , */\
   { 0x1020, "flag_otpok"},    /* OTP alarm                                         , */\
   { 0x1030, "flag_ovpok"},    /* OVP alarm                                         , */\
   { 0x1040, "flag_uvpok"},    /* UVP alarm                                         , */\
   { 0x1050, "flag_clocks_stable"},    /* Clocks stable                                     , */\
   { 0x1060, "flag_mtp_busy"},    /* MTP busy                                          , */\
   { 0x1070, "flag_lost_clk"},    /* Lost clock                                        , */\
   { 0x1080, "flag_cf_speakererror"},    /* Speaker error                                     , */\
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
   { 0x11e0, "flag_haptic_busy"},    /* Status haptic driver                              , */\
   { 0x1200, "flag_ocpokap_left"},    /* OCPOK pmos A left                                 , */\
   { 0x1210, "flag_ocpokan_left"},    /* OCPOK nmos A left                                 , */\
   { 0x1220, "flag_ocpokbp_left"},    /* OCPOK pmos B left                                 , */\
   { 0x1230, "flag_ocpokbn_left"},    /* OCPOK nmos B left                                 , */\
   { 0x1240, "flag_clipa_high_left"},    /* Clipping A left to Vddp                           , */\
   { 0x1250, "flag_clipa_low_left"},    /* Clipping A left to gnd                            , */\
   { 0x1260, "flag_clipb_high_left"},    /* Clipping B left to Vddp                           , */\
   { 0x1270, "flag_clipb_low_left"},    /* Clipping B left to gnd                            , */\
   { 0x1280, "flag_ocpokap_rcv"},    /* OCPOK pmos A RCV                                  , */\
   { 0x1290, "flag_ocpokan_rcv"},    /* OCPOK nmos A RCV                                  , */\
   { 0x12a0, "flag_ocpokbp_rcv"},    /* OCPOK pmos B RCV                                  , */\
   { 0x12b0, "flag_ocpokbn_rcv"},    /* OCPOK nmos B RCV                                  , */\
   { 0x12c0, "flag_rcvldop_ready"},    /* RCV LDO regulates                                 , */\
   { 0x12d0, "flag_rcvldop_bypassready"},    /* Receiver LDO ready                                , */\
   { 0x12e0, "flag_ocp_alarm_left"},    /* OCP left amplifier                                , */\
   { 0x12f0, "flag_clip_left"},    /* Amplifier left clipping                           , */\
   { 0x1300, "flag_ocpokap_right"},    /* OCPOK pmos A right                                , */\
   { 0x1310, "flag_ocpokan_right"},    /* OCPOK nmos A right                                , */\
   { 0x1320, "flag_ocpokbp_right"},    /* OCPOK pmos B right                                , */\
   { 0x1330, "flag_ocpokbn_right"},    /* OCPOK nmos B right                                , */\
   { 0x1340, "flag_clipa_high_right"},    /* Clipping A right to Vddp                          , */\
   { 0x1350, "flag_clipa_low_right"},    /* Clipping A right to gnd                           , */\
   { 0x1360, "flag_clipb_high_right"},    /* Clipping B left to Vddp                           , */\
   { 0x1370, "flag_clipb_low_right"},    /* Clipping B right to gnd                           , */\
   { 0x1380, "flag_ocp_alarm_right"},    /* OCP right amplifier                               , */\
   { 0x1390, "flag_clip_right"},    /* Amplifier right clipping                          , */\
   { 0x13a0, "flag_mic_ocpok"},    /* OCPOK MICVDD                                      , */\
   { 0x13b0, "flag_man_alarm_state"},    /* Alarm state                                       , */\
   { 0x13c0, "flag_man_wait_src_settings"},    /* Wait HW I2C settings                              , */\
   { 0x13d0, "flag_man_wait_cf_config"},    /* Wait CF config                                    , */\
   { 0x13e0, "flag_man_start_mute_audio"},    /* Audio mute sequence                               , */\
   { 0x13f0, "flag_man_operating_state"},    /* Operating state                                   , */\
   { 0x1400, "flag_cf_speakererror_left"},    /* Left speaker status                               , */\
   { 0x1410, "flag_cf_speakererror_right"},    /* Right speaker status                              , */\
   { 0x1420, "flag_clk_out_of_range"},    /* External clock status                             , */\
   { 0x1433, "man_state"},    /* Device manager status                             , */\
   { 0x1509, "bat_adc"},    /* Battery voltage (V)                               , */\
   { 0x1608, "temp_adc"},    /* IC Temperature (C)                                , */\
   { 0x2003, "tdm_usecase"},    /* Usecase setting                                   , */\
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
   { 0x2291, "tdm_txdata_format_unused_slot_sd0"},    /* Format unused slots GAINIO                        , */\
   { 0x22b1, "tdm_txdata_format_unused_slot_sd1"},    /* Format unused slots DIO1                          , */\
   { 0x22d1, "tdm_txdata_format_unused_slot_sd2"},    /* Format unused slots DIO2                          , */\
   { 0x2300, "tdm_sink0_enable"},    /* Control gainin (not used in DSP)                  , */\
   { 0x2310, "tdm_sink1_enable"},    /* Control audio left                                , */\
   { 0x2320, "tdm_sink2_enable"},    /* Control audio right                               , */\
   { 0x2330, "tdm_source0_enable"},    /* Control gainout (not used in DSP)                 , */\
   { 0x2340, "tdm_source1_enable"},    /* Control voltage sense right                       , */\
   { 0x2350, "tdm_source2_enable"},    /* Control current sense right                       , */\
   { 0x2360, "tdm_source3_enable"},    /* Voltage sense left control                        , */\
   { 0x2370, "tdm_source4_enable"},    /* Current sense left control                        , */\
   { 0x2380, "tdm_source5_enable"},    /* DSP out right control                             , */\
   { 0x2390, "tdm_source6_enable"},    /* DSP out left control                              , */\
   { 0x23a0, "tdm_source7_enable"},    /* AEC ref left control                              , */\
   { 0x23b0, "tdm_source8_enable"},    /* AEC ref right control                             , */\
   { 0x23c0, "tdm_source9_enable"},    /* PDM 1 control                                     , */\
   { 0x23d0, "tdm_source10_enable"},    /* PDM 2 control                                     , */\
   { 0x2401, "tdm_sink0_io"},    /* IO gainin (not used in DSP)                       , */\
   { 0x2421, "tdm_sink1_io"},    /* IO audio left                                     , */\
   { 0x2441, "tdm_sink2_io"},    /* IO audio right                                    , */\
   { 0x2461, "tdm_source0_io"},    /* IO gainout (not used in DSP)                      , */\
   { 0x2481, "tdm_source1_io"},    /* IO voltage sense right                            , */\
   { 0x24a1, "tdm_source2_io"},    /* IO current sense right                            , */\
   { 0x24c1, "tdm_source3_io"},    /* IO voltage sense left                             , */\
   { 0x24e1, "tdm_source4_io"},    /* IO current sense left                             , */\
   { 0x2501, "tdm_source5_io"},    /* IO dspout right                                   , */\
   { 0x2521, "tdm_source6_io"},    /* IO dspout left                                    , */\
   { 0x2541, "tdm_source7_io"},    /* IO AEC ref left control                           , */\
   { 0x2561, "tdm_source8_io"},    /* IO AEC ref right control                          , */\
   { 0x2581, "tdm_source9_io"},    /* IO pdm1                                           , */\
   { 0x25a1, "tdm_source10_io"},    /* IO pdm2                                           , */\
   { 0x2603, "tdm_sink0_slot"},    /* Position gainin (not used in DSP)                 , */\
   { 0x2643, "tdm_sink1_slot"},    /* Position audio left                               , */\
   { 0x2683, "tdm_sink2_slot"},    /* Position audio right                              , */\
   { 0x26c3, "tdm_source0_slot"},    /* Position gainout (not used in DSP)                , */\
   { 0x2703, "tdm_source1_slot"},    /* Position voltage sense right                      , */\
   { 0x2743, "tdm_source2_slot"},    /* Position current sense right                      , */\
   { 0x2783, "tdm_source3_slot"},    /* Position voltage sense left                       , */\
   { 0x27c3, "tdm_source4_slot"},    /* Position current sense left                       , */\
   { 0x2803, "tdm_source5_slot"},    /* Position dspout right                             , */\
   { 0x2843, "tdm_source6_slot"},    /* Position dspout left                              , */\
   { 0x2883, "tdm_source7_slot"},    /* Position AEC ref left control                     , */\
   { 0x28c3, "tdm_source8_slot"},    /* Position AEC ref right control                    , */\
   { 0x2903, "tdm_source9_slot"},    /* Position pdm1                                     , */\
   { 0x2943, "tdm_source10_slot"},    /* Position pdm2                                     , */\
   { 0x3100, "pdm_mode"},    /* PDM control                                       , */\
   { 0x3111, "pdm_side_tone_sel"},    /* Side tone input                                   , */\
   { 0x3130, "pdm_left_sel"},    /* PDM data selection for left channel during PDM direct mode, */\
   { 0x3140, "pdm_right_sel"},    /* PDM data selection for right channel during PDM direct mode, */\
   { 0x3150, "enbl_micvdd"},    /* Enable MICVDD                                     , */\
   { 0x3160, "bypass_micvdd_ocp"},    /* Bypass control for the MICVDD OCP flag processing , */\
   { 0x3201, "pdm_nbck"},    /* PDM BCK/Fs ratio                                  , */\
   { 0x3223, "pdm_gain"},    /* PDM gain                                          , */\
   { 0x3263, "sel_pdm_out_data"},    /* PDM output selection - RE/FE data combination     , */\
   { 0x32a0, "sel_cf_haptic_data"},    /* Select the source for haptic data output (not for customer), */\
   { 0x3307, "haptic_duration"},    /* Duration (ms)                                     , */\
   { 0x3387, "haptic_data"},    /* DC value (FFS)                                    , */\
   { 0x3403, "gpio_datain"},    /* Receiving value                                   , */\
   { 0x3500, "gpio_ctrl"},    /* GPIO master control over GPIO1/2 ports (not for customer), */\
   { 0x3513, "gpio_dir"},    /* Configuration                                     , */\
   { 0x3553, "gpio_dataout"},    /* Transmitting value                                , */\
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
   { 0x4170, "int_out_flag_rcvldop_ready"},    /* Status rcvldop ready                              , */\
   { 0x4180, "int_out_flag_ocp_alarm_left"},    /* Status ocp alarm left                             , */\
   { 0x4190, "int_out_flag_ocp_alarm_right"},    /* Status ocp alarm right                            , */\
   { 0x41a0, "int_out_flag_man_wait_src_settings"},    /* Status Waits HW I2C settings                      , */\
   { 0x41b0, "int_out_flag_man_wait_cf_config"},    /* Status waits CF config                            , */\
   { 0x41c0, "int_out_flag_man_start_mute_audio"},    /* Status Audio mute sequence                        , */\
   { 0x41d0, "int_out_flag_cfma_err"},    /* Status cfma error                                 , */\
   { 0x41e0, "int_out_flag_cfma_ack"},    /* Status cfma ack                                   , */\
   { 0x41f0, "int_out_flag_clk_out_of_range"},    /* Status flag_clk_out_of_range                      , */\
   { 0x4200, "int_out_flag_tdm_error"},    /* Status tdm error                                  , */\
   { 0x4210, "int_out_flag_clip_left"},    /* Status clip left                                  , */\
   { 0x4220, "int_out_flag_clip_right"},    /* Status clip right                                 , */\
   { 0x4230, "int_out_flag_mic_ocpok"},    /* Status mic ocpok                                  , */\
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
   { 0x4570, "int_in_flag_rcvldop_ready"},    /* Clear rcvldop ready                               , */\
   { 0x4580, "int_in_flag_ocp_alarm_left"},    /* Clear ocp alarm left                              , */\
   { 0x4590, "int_in_flag_ocp_alarm_right"},    /* Clear ocp alarm right                             , */\
   { 0x45a0, "int_in_flag_man_wait_src_settings"},    /* Clear wait HW I2C settings                        , */\
   { 0x45b0, "int_in_flag_man_wait_cf_config"},    /* Clear wait cf config                              , */\
   { 0x45c0, "int_in_flag_man_start_mute_audio"},    /* Clear audio mute sequence                         , */\
   { 0x45d0, "int_in_flag_cfma_err"},    /* Clear cfma err                                    , */\
   { 0x45e0, "int_in_flag_cfma_ack"},    /* Clear cfma ack                                    , */\
   { 0x45f0, "int_in_flag_clk_out_of_range"},    /* Clear flag_clk_out_of_range                       , */\
   { 0x4600, "int_in_flag_tdm_error"},    /* Clear tdm error                                   , */\
   { 0x4610, "int_in_flag_clip_left"},    /* Clear clip left                                   , */\
   { 0x4620, "int_in_flag_clip_right"},    /* Clear clip right                                  , */\
   { 0x4630, "int_in_flag_mic_ocpok"},    /* Clear mic ocpok                                   , */\
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
   { 0x4970, "int_enable_flag_rcvldop_ready"},    /* Enable rcvldop ready                              , */\
   { 0x4980, "int_enable_flag_ocp_alarm_left"},    /* Enable ocp alarm left                             , */\
   { 0x4990, "int_enable_flag_ocp_alarm_right"},    /* Enable ocp alarm right                            , */\
   { 0x49a0, "int_enable_flag_man_wait_src_settings"},    /* Enable waits HW I2C settings                      , */\
   { 0x49b0, "int_enable_flag_man_wait_cf_config"},    /* Enable man wait cf config                         , */\
   { 0x49c0, "int_enable_flag_man_start_mute_audio"},    /* Enable man Audio mute sequence                    , */\
   { 0x49d0, "int_enable_flag_cfma_err"},    /* Enable cfma err                                   , */\
   { 0x49e0, "int_enable_flag_cfma_ack"},    /* Enable cfma ack                                   , */\
   { 0x49f0, "int_enable_flag_clk_out_of_range"},    /* Enable flag_clk_out_of_range                      , */\
   { 0x4a00, "int_enable_flag_tdm_error"},    /* Enable tdm error                                  , */\
   { 0x4a10, "int_enable_flag_clip_left"},    /* Enable clip left                                  , */\
   { 0x4a20, "int_enable_flag_clip_right"},    /* Enable clip right                                 , */\
   { 0x4a30, "int_enable_flag_mic_ocpok"},    /* Enable mic ocpok                                  , */\
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
   { 0x4d70, "int_polarity_flag_rcvldop_ready"},    /* Polarity rcvldop ready                            , */\
   { 0x4d80, "int_polarity_flag_ocp_alarm_left"},    /* Polarity ocp alarm left                           , */\
   { 0x4d90, "int_polarity_flag_ocp_alarm_right"},    /* Polarity ocp alarm right                          , */\
   { 0x4da0, "int_polarity_flag_man_wait_src_settings"},    /* Polarity waits HW I2C settings                    , */\
   { 0x4db0, "int_polarity_flag_man_wait_cf_config"},    /* Polarity man wait cf config                       , */\
   { 0x4dc0, "int_polarity_flag_man_start_mute_audio"},    /* Polarity man audio mute sequence                  , */\
   { 0x4dd0, "int_polarity_flag_cfma_err"},    /* Polarity cfma err                                 , */\
   { 0x4de0, "int_polarity_flag_cfma_ack"},    /* Polarity cfma ack                                 , */\
   { 0x4df0, "int_polarity_flag_clk_out_of_range"},    /* Polarity flag_clk_out_of_range                    , */\
   { 0x4e00, "int_polarity_flag_tdm_error"},    /* Polarity tdm error                                , */\
   { 0x4e10, "int_polarity_flag_clip_left"},    /* Polarity clip left                                , */\
   { 0x4e20, "int_polarity_flag_clip_right"},    /* Polarity clip right                               , */\
   { 0x4e30, "int_polarity_flag_mic_ocpok"},    /* Polarity mic ocpok                                , */\
   { 0x5001, "vbat_prot_attack_time"},    /* Battery protection attack Time                    , */\
   { 0x5023, "vbat_prot_thlevel"},    /* Battery protection threshold voltage level        , */\
   { 0x5061, "vbat_prot_max_reduct"},    /* Battery protection maximum reduction              , */\
   { 0x5082, "vbat_prot_release_time"},    /* Battery protection release time                   , */\
   { 0x50b1, "vbat_prot_hysterese"},    /* Battery protection hysteresis                     , */\
   { 0x50d0, "rst_min_vbat"},    /* Reset clipper - Auto clear                        , */\
   { 0x50e0, "sel_vbat"},    /* Battery voltage read out                          , */\
   { 0x50f0, "bypass_clipper"},    /* Bypass HW clipper                                 , */\
   { 0x5100, "batsense_steepness"},    /* Vbat prot steepness                               , */\
   { 0x5110, "soft_mute"},    /* Soft mute HW                                      , */\
   { 0x5120, "cf_mute_left"},    /* Soft mute FW left                                 , */\
   { 0x5130, "cf_mute_right"},    /* Soft mute FW right                                , */\
   { 0x5140, "bypass_hp_left"},    /* Bypass HPF left                                   , */\
   { 0x5150, "bypass_hp_right"},    /* Bypass HPF right                                  , */\
   { 0x5160, "enbl_dpsa_left"},    /* Enable DPSA left                                  , */\
   { 0x5170, "enbl_dpsa_right"},    /* Enable DPSA right                                 , */\
   { 0x5187, "cf_volume"},    /* FW volume control for primary audio channel       , */\
   { 0x5200, "ctrl_rcv"},    /* Selection receiver                                , */\
   { 0x5210, "ctrl_rcv_fb_100k"},    /* Selection of feedback resistor for receiver mode (not for customer), */\
   { 0x5222, "ctrl_cc"},    /* Clip control setting                              , */\
   { 0x5257, "gain"},    /* Amplifier gain                                    , */\
   { 0x52d0, "ctrl_slopectrl"},    /* Enables slope control                             , */\
   { 0x52e1, "ctrl_slope"},    /* Set slope                                         , */\
   { 0x5301, "dpsa_level"},    /* DPSA threshold levels                             , */\
   { 0x5321, "dpsa_release"},    /* DPSA Release time                                 , */\
   { 0x5340, "clipfast"},    /* Clock selection for HW clipper for battery protection, */\
   { 0x5350, "bypass_lp"},    /* Bypass the low power filter inside temperature sensor, */\
   { 0x5360, "enbl_low_latency"},    /* CF low latency outputs for add module             , */\
   { 0x5400, "first_order_mode"},    /* Overrule to 1st order mode of control stage when clipping, */\
   { 0x5410, "bypass_ctrlloop"},    /* Switch amplifier into open loop configuration     , */\
   { 0x5420, "fb_hz"},    /* Feedback resistor set to high ohmic               , */\
   { 0x5430, "icomp_engage"},    /* Engage of icomp                                   , */\
   { 0x5440, "ctrl_kickback"},    /* Prevent double pulses of output stage             , */\
   { 0x5450, "icomp_engage_overrule"},    /* To overrule the functional icomp_engage signal during validation, */\
   { 0x5503, "ctrl_dem"},    /* Enable DEM icomp and DEM one bit dac              , */\
   { 0x5543, "ctrl_dem_mismatch"},    /* Enable DEM icomp mismatch for testing             , */\
   { 0x5581, "dpsa_drive"},    /* Control of the number of power stage sections, total of 4 sections. Each section is 1/4 of the total power stages., */\
   { 0x560a, "enbl_amp_left"},    /* Switch on the class-D power sections, each part of the analog sections can be switched on/off individually - Left channel, */\
   { 0x56b0, "enbl_engage_left"},    /* Enables/engage power stage and control loop - left channel, */\
   { 0x570a, "enbl_amp_right"},    /* Switch on the class-D power sections, each part of the analog sections can be switched on/off individually - Right channel, */\
   { 0x57b0, "enbl_engage_right"},    /* Enables/engage power stage and control loop - right channel, */\
   { 0x5800, "hard_mute_left"},    /* Hard mute - PWM module left                       , */\
   { 0x5810, "hard_mute_right"},    /* Hard mute - PWM module right                      , */\
   { 0x5820, "pwm_shape"},    /* PWM shape                                         , */\
   { 0x5830, "pwm_bitlength"},    /* PWM bit length in noise shaper                    , */\
   { 0x5844, "pwm_delay"},    /* PWM delay bits to set the delay, clockd is 1/(k*2048*fs), */\
   { 0x5890, "reclock_pwm"},    /* Reclock the pwm signal inside analog              , */\
   { 0x58a0, "reclock_voltsense"},    /* Reclock the voltage sense pwm signal              , */\
   { 0x58b0, "enbl_pwm_phase_shift_left"},    /* Control for pwm phase shift, inverted function - left channel, */\
   { 0x58c0, "enbl_pwm_phase_shift_right"},    /* Control for pwm phase shift - right channel       , */\
   { 0x5900, "ctrl_rcvldop_pulldown"},    /* Pulldown of LDO (2.7V)                            , */\
   { 0x5910, "ctrl_rcvldop_test_comp"},    /* Enable testing of LDO comparator                  , */\
   { 0x5920, "ctrl_rcvldop_test_loadedldo"},    /* Load connected to rcvldo                          , */\
   { 0x5930, "enbl_rcvldop"},    /* Enables the LDO (2.7)                             , */\
   { 0x5a07, "cf_volume_sec"},    /* FW volume control for secondary audio channel     , */\
   { 0x5a87, "sw_profile"},    /* Software profile data                             , */\
   { 0x7002, "boost_volt"},    /* Boost voltage                                     , */\
   { 0x7033, "boost_cur"},    /* Max coil current                                  , */\
   { 0x7071, "bst_coil_value"},    /* Coil Value                                        , */\
   { 0x7090, "boost_intel"},    /* Adaptive boost mode                               , */\
   { 0x70a0, "boost_speed"},    /* Soft ramp up/down                                 , */\
   { 0x70b2, "dcdc_synchronisation"},    /* DCDC synchronization off + 7 positions            , */\
   { 0x70e0, "dcdcoff_mode"},    /* DCDC on/off                                       , */\
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
   { 0x7332, "bst_freq"},    /* DCDC bost frequency control                       , */\
   { 0x8001, "sel_clk_cs"},    /* Current sense clock duty cycle control            , */\
   { 0x8021, "micadc_speed"},    /* Current sense clock for MiCADC selection - 32/44.1/48 KHz Fs band only, */\
   { 0x8040, "cs_dc_offset"},    /* Current sense decimator offset control            , */\
   { 0x8050, "cs_gain_control"},    /* Current sense gain control                        , */\
   { 0x8060, "cs_bypass_gc"},    /* Bypasses the CS gain correction                   , */\
   { 0x8087, "cs_gain"},    /* Current sense gain                                , */\
   { 0x8110, "invertpwm_left"},    /* Current sense common mode feedback pwm invert control for left channel, */\
   { 0x8122, "cmfb_gain_left"},    /* Current sense common mode feedback control gain for left channel, */\
   { 0x8154, "cmfb_offset_left"},    /* Current sense common mode feedback control offset for left channel, */\
   { 0x8200, "enbl_cmfb_right"},    /* Current sense common mode feedback control for right channel, */\
   { 0x8210, "invertpwm_right"},    /* Current sense common mode feedback pwm invert control for right channel, */\
   { 0x8222, "cmfb_gain_right"},    /* Current sense common mode feedback control gain for right channel, */\
   { 0x8254, "cmfb_offset_right"},    /* Current sense common mode feedback control offset for right channel, */\
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
   { 0x8600, "enbl_cs_adc_left"},    /* Enable current sense ADC                          , */\
   { 0x8610, "enbl_cs_inn1_left"},    /* Enable connection of current sense negative1      , */\
   { 0x8630, "enbl_cs_inp1_left"},    /* Enable connection of current sense positive1      , */\
   { 0x8650, "enbl_cs_ldo_left"},    /* Enable current sense LDO                          , */\
   { 0x8660, "enbl_cs_nofloating_n_left"},    /* Connect current sense negative to gnda at transitions of booster or classd amplifiers. Otherwise floating (0), */\
   { 0x8670, "enbl_cs_nofloating_p_left"},    /* Connect current sense positive to gnda at transitions of booster or classd amplifiers. Otherwise floating (0), */\
   { 0x8680, "enbl_cs_vbatldo_left"},    /* Enable of current sense LDO                       , */\
   { 0x8700, "enbl_cs_adc_right"},    /* Enable current sense ADC                          , */\
   { 0x8710, "enbl_cs_inn1_right"},    /* Enable connection of current sense negative1      , */\
   { 0x8730, "enbl_cs_inp1_right"},    /* Enable connection of current sense positive1      , */\
   { 0x8750, "enbl_cs_ldo_right"},    /* Enable current sense LDO                          , */\
   { 0x8760, "enbl_cs_nofloating_n_right"},    /* Connect current sense negative to gnda at transitions of booster or classd amplifiers. Otherwise floating (0), */\
   { 0x8770, "enbl_cs_nofloating_p_right"},    /* Connect current sense positive to gnda at transitions of booster or classd amplifiers. Otherwise floating (0), */\
   { 0x8780, "enbl_cs_vbatldo_right"},    /* Enable of current sense LDO                       , */\
   { 0x8800, "volsense_pwm_sel"},    /* Voltage sense PWM source selection control        , */\
   { 0x8810, "volsense_dc_offset"},    /* Voltage sense decimator offset control            , */\
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
   { 0x9387, "cf_ack"},    /* Acknowledge of requests                           , */\
   { 0x9380, "cf_ack_cmd"},    /* Firmware event acknowledge rpc command            , */\
   { 0x9390, "cf_ack_reset"},    /* Firmware event acknowledge reset restart          , */\
   { 0x93a0, "cf_ack_mips"},    /* Firmware event acknowledge short on mips          , */\
   { 0x93b0, "cf_ack_mute_ready"},    /* Firmware event acknowledge mute sequence ready    , */\
   { 0x93c0, "cf_ack_volume_ready"},    /* Firmware event acknowledge volume ready           , */\
   { 0x93d0, "cf_ack_damage"},    /* Firmware event acknowledge speaker damage detected, */\
   { 0x93e0, "cf_ack_calibrate_ready"},    /* Firmware event acknowledge calibration completed  , */\
   { 0x93f0, "cf_ack_reserved"},    /* Firmware event acknowledge reserved               , */\
   { 0x980f, "ivt_addr0_msb"},    /* Coolflux interrupt vector table address0 MSB      , */\
   { 0x990f, "ivt_addr0_lsb"},    /* Coolflux interrupt vector table address0 LSB      , */\
   { 0x9a0f, "ivt_addr1_msb"},    /* Coolflux interrupt vector table address1 MSB      , */\
   { 0x9b0f, "ivt_addr1_lsb"},    /* Coolflux interrupt vector table address1 LSB      , */\
   { 0x9c0f, "ivt_addr2_msb"},    /* Coolflux interrupt vector table address2 MSB      , */\
   { 0x9d0f, "ivt_addr2_lsb"},    /* Coolflux interrupt vector table address2 LSB      , */\
   { 0x9e0f, "ivt_addr3_msb"},    /* Coolflux interrupt vector table address3 MSB      , */\
   { 0x9f0f, "ivt_addr3_lsb"},    /* Coolflux interrupt vector table address3 LSB      , */\
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
   { 0xc570, "test_enbl_cs"},    /* Enable for digimux mode of current sense          , */\
   { 0xc600, "enbl_pwm_dcc"},    /* Enables direct control of pwm duty cycle for DCDC power stage, */\
   { 0xc613, "pwm_dcc_cnt"},    /* Control pwm duty cycle when enbl_pwm_dcc is 1     , */\
   { 0xc650, "enbl_ldo_stress"},    /* Enable stress of internal supply voltages powerstages, */\
   { 0xc660, "bypass_diosw_ovp"},    /* Bypass ovp for memory switch diosw                , */\
   { 0xc670, "enbl_powerswitch"},    /* Vddd core power switch control - overrules the manager control, */\
   { 0xc707, "digimuxa_sel"},    /* DigimuxA input selection control routed to GPIO1 (see Digimux list for details), */\
   { 0xc787, "digimuxb_sel"},    /* DigimuxB input selection control routed to GPIO2 (see Digimux list for details), */\
   { 0xc807, "digimuxc_sel"},    /* DigimuxC input selection control routed to GPIO3 (see Digimux list for details), */\
   { 0xc887, "digimuxd_sel"},    /* DigimuxD input selection control routed to GPIO4 (see Digimux list for details), */\
   { 0xc901, "dio1_ehs"},    /* Speed/load setting for DIO1 IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
   { 0xc921, "dio2_ehs"},    /* Speed/load setting for DIO2 IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
   { 0xc941, "gainio_ehs"},    /* Speed/load setting for GAINIO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
   { 0xc961, "pdmo_ehs"},    /* Speed/load setting for PDMO IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
   { 0xc981, "int_ehs"},    /* Speed/load setting for INT IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
   { 0xc9a1, "tdo_ehs"},    /* Speed/load setting for TDO IO cell, clk or data mode range (see SLIMMF IO cell datasheet), */\
   { 0xc9c0, "hs_mode"},    /* I2C high speed mode control                       , */\
   { 0xca00, "enbl_anamux1"},    /* Enable anamux1                                    , */\
   { 0xca10, "enbl_anamux2"},    /* Enable anamux2                                    , */\
   { 0xca20, "enbl_anamux3"},    /* Enable anamux3                                    , */\
   { 0xca30, "enbl_anamux4"},    /* Enable anamux4                                    , */\
   { 0xca40, "enbl_anamux5"},    /* Enable anamux5                                    , */\
   { 0xca50, "enbl_anamux6"},    /* Enable anamux6                                    , */\
   { 0xca60, "enbl_anamux7"},    /* Enable anamux7                                    , */\
   { 0xca74, "anamux1"},    /* Anamux selection control - anamux on TEST1        , */\
   { 0xcb04, "anamux2"},    /* Anamux selection control - anamux on TEST2        , */\
   { 0xcb54, "anamux3"},    /* Anamux selection control - anamux on TEST3        , */\
   { 0xcba4, "anamux4"},    /* Anamux selection control - anamux on TEST4        , */\
   { 0xcc04, "anamux5"},    /* Anamux selection control - anamux on TEST5        , */\
   { 0xcc54, "anamux6"},    /* Anamux selection control - anamux on TEST6        , */\
   { 0xcca4, "anamux7"},    /* Anamux selection control - anamux on TEST7        , */\
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
   { 0xd243, "tsig_gain_left"},    /* Test signal gain for left channel                 , */\
   { 0xd283, "tsig_gain_right"},    /* Test signal gain for right channel                , */\
   { 0xd300, "adc10_reset"},    /* Reset for ADC10 - I2C direct control mode         , */\
   { 0xd311, "adc10_test"},    /* Test mode selection signal for ADC10 - I2C direct control mode, */\
   { 0xd332, "adc10_sel"},    /* Select the input to convert for ADC10 - I2C direct control mode, */\
   { 0xd364, "adc10_prog_sample"},    /* ADC10 program sample setting - I2C direct control mode, */\
   { 0xd3b0, "adc10_enbl"},    /* Enable ADC10 - I2C direct control mode            , */\
   { 0xd3c0, "bypass_lp_vbat"},    /* Bypass control for Low pass filter in batt sensor , */\
   { 0xd409, "data_adc10_tempbat"},    /* ADC 10 data output data for testing               , */\
   { 0xd506, "ctrl_digtoana_hidden"},    /* Spare digital to analog control bits - Hidden     , */\
   { 0xd570, "enbl_clk_out_of_range"},    /* Clock out of range                                , */\
   { 0xf000, "calibration_onetime"},    /* Calibration schedule                              , */\
   { 0xf010, "calibr_ron_done"},    /* Calibration Ron executed                          , */\
   { 0xf020, "calibr_dcdc_api_calibrate"},    /* Calibration current limit DCDC                    , */\
   { 0xf030, "calibr_dcdc_delta_sign"},    /* Sign bit for delta calibration current limit DCDC , */\
   { 0xf042, "calibr_dcdc_delta"},    /* Calibration delta current limit DCDC              , */\
   { 0xf078, "calibr_speaker_info"},    /* Reserved space for allowing customer to store speaker information, */\
   { 0xf105, "calibr_vout_offset"},    /* DCDC offset calibration 2's complement (key1 protected), */\
   { 0xf163, "calibr_gain_left"},    /* HW gain module - left channel (2's complement)    , */\
   { 0xf1a5, "calibr_offset_left"},    /* Offset for amplifier, HW gain module - left channel (2's complement), */\
   { 0xf203, "calibr_gain_right"},    /* HW gain module - right channel (2's complement)   , */\
   { 0xf245, "calibr_offset_right"},    /* Offset for amplifier, HW gain module - right channel (2's complement), */\
   { 0xf2a3, "calibr_rcvldop_trim"},    /* Trimming of LDO (2.7V)                            , */\
   { 0xf307, "calibr_gain_cs_left"},    /* Current sense gain - left channel (signed two's complement format), */\
   { 0xf387, "calibr_gain_cs_right"},    /* Current sense gain - right channel (signed two's complement format), */\
   { 0xf40f, "calibr_R25C_L"},    /* Ron resistance of left channel speaker coil       , */\
   { 0xf50f, "calibr_R25C_R"},    /* Ron resistance of right channel speaker coil      , */\
   { 0xf606, "ctrl_offset_a_left"},    /* Offset of left amplifier level shifter A          , */\
   { 0xf686, "ctrl_offset_b_left"},    /* Offset of left amplifier level shifter B          , */\
   { 0xf706, "ctrl_offset_a_right"},    /* Offset of right amplifier level shifter A         , */\
   { 0xf786, "ctrl_offset_b_right"},    /* Offset of right amplifier level shifter B         , */\
   { 0xf806, "htol_iic_addr"},    /* 7-bit I2C address to be used during HTOL testing  , */\
   { 0xf870, "htol_iic_addr_en"},    /* HTOL I2C address enable control                   , */\
   { 0xf884, "calibr_temp_offset"},    /* Temperature offset 2's compliment (key1 protected), */\
   { 0xf8d2, "calibr_temp_gain"},    /* Temperature gain 2's compliment (key1 protected)  , */\
   { 0xf900, "mtp_lock_dcdcoff_mode"},    /* Disable function dcdcoff_mode                     , */\
   { 0xf910, "mtp_lock_enbl_coolflux"},    /* Disable function enbl_coolflux                    , */\
   { 0xf920, "mtp_lock_bypass_clipper"},    /* Disable function bypass_clipper                   , */\
   { 0xf930, "mtp_lock_max_dcdc_voltage"},    /* Disable programming of max dcdc boost voltage     , */\
   { 0xf943, "calibr_vbg_trim"},    /* Bandgap trimming control                          , */\
   { 0xf987, "type_bits_fw"},    /* MTP-control FW - See Firmware I2C API document for details, */\
   { 0xfa0f, "mtpdataA"},    /* MTPdataA (key1 protected)                         , */\
   { 0xfb0f, "mtpdataB"},    /* MTPdataB (key1 protected)                         , */\
   { 0xfc0f, "mtpdataC"},    /* MTPdataC (key1 protected)                         , */\
   { 0xfd0f, "mtpdataD"},    /* MTPdataD (key1 protected)                         , */\
   { 0xfe0f, "mtpdataE"},    /* MTPdataE (key1 protected)                         , */\
   { 0xff05, "calibr_osc_delta_ndiv"},    /* Calibration data for OSC1M, signed number representation, */\
   { 0xffff, "Unknown bitfield enum" }    /* not found */\
};

enum tfa2_irq {
	tfa2_irq_stvdds = 0,
	tfa2_irq_stplls = 1,
	tfa2_irq_stotds = 2,
	tfa2_irq_stovds = 3,
	tfa2_irq_stuvds = 4,
	tfa2_irq_stclks = 5,
	tfa2_irq_stmtpb = 6,
	tfa2_irq_stnoclk = 7,
	tfa2_irq_stspks = 8,
	tfa2_irq_stacs = 9,
	tfa2_irq_stsws = 10,
	tfa2_irq_stwds = 11,
	tfa2_irq_stamps = 12,
	tfa2_irq_starefs = 13,
	tfa2_irq_stadccr = 14,
	tfa2_irq_stbodnok = 15,
	tfa2_irq_stbstcu = 16,
	tfa2_irq_stbsthi = 17,
	tfa2_irq_stbstoc = 18,
	tfa2_irq_stbstpkcur = 19,
	tfa2_irq_stbstvc = 20,
	tfa2_irq_stbst86 = 21,
	tfa2_irq_stbst93 = 22,
	tfa2_irq_strcvld = 23,
	tfa2_irq_stocpl = 24,
	tfa2_irq_stocpr = 25,
	tfa2_irq_stmwsrc = 26,
	tfa2_irq_stmwcfc = 27,
	tfa2_irq_stmwsmu = 28,
	tfa2_irq_stcfmer = 29,
	tfa2_irq_stcfmac = 30,
	tfa2_irq_stclkoor = 31,
	tfa2_irq_sttdmer = 32,
	tfa2_irq_stclpl = 33,
	tfa2_irq_stclpr = 34,
	tfa2_irq_stocpm = 35,
	tfa2_irq_max = 36,
	tfa2_irq_all = -1 /* all irqs */};

#define TFA2_IRQ_NAMETABLE static tfaIrqName_t Tfa2IrqNames[] = {\
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
};
