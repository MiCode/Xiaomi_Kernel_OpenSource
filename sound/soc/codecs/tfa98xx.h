/** \file Tfa98xx_genregs.h
 * This file was automatically generated.
 */
#ifndef TFA98XX_GENREGS_H_
#define TFA98XX_GENREGS_H_


/**  StatusReg Register ($00) ********************************************/

/**   \addtogroup _0x00_StatusReg
 * @{
 */
#define TFA98XX_STATUSREG          0x00
/**   \addtogroup VDDS
 * @{
 */
/*!
 Power-on-reset flag
 * -  1 = Power-on-reset detected
 * -  0 = Power-on-reset detected and cleared by reading status register
*/
#define TFA98XX_STATUSREG_VDDS           (0x1<<0)
#define TFA98XX_STATUSREG_VDDS_POS       0
#define TFA98XX_STATUSREG_VDDS_LEN       1
#define TFA98XX_STATUSREG_VDDS_MAX       1
#define TFA98XX_STATUSREG_VDDS_MSK       0x1
/** @} */

/**   \addtogroup PLLS
 * @{
 */
/*!
 PLL lock
 * -  0 = PLL not in lock
 * -  1 = PLL in lock
*/
#define TFA98XX_STATUSREG_PLLS           (0x1<<1)
#define TFA98XX_STATUSREG_PLLS_POS       1
#define TFA98XX_STATUSREG_PLLS_LEN       1
#define TFA98XX_STATUSREG_PLLS_MAX       1
#define TFA98XX_STATUSREG_PLLS_MSK       0x2
/** @} */

/**   \addtogroup OTDS
 * @{
 */
/*!
 Over Temperature Protection alarm
 * -  0 = Temperature To High
 * -  1 = Temperature OK
*/
#define TFA98XX_STATUSREG_OTDS           (0x1<<2)
#define TFA98XX_STATUSREG_OTDS_POS       2
#define TFA98XX_STATUSREG_OTDS_LEN       1
#define TFA98XX_STATUSREG_OTDS_MAX       1
#define TFA98XX_STATUSREG_OTDS_MSK       0x4
/** @} */

/**   \addtogroup OVDS
 * @{
 */
/*!
 Over Voltage Protection alarm
 * -  0 = VddP is To High
 * -  1 = VddP is OK
*/
#define TFA98XX_STATUSREG_OVDS           (0x1<<3)
#define TFA98XX_STATUSREG_OVDS_POS       3
#define TFA98XX_STATUSREG_OVDS_LEN       1
#define TFA98XX_STATUSREG_OVDS_MAX       1
#define TFA98XX_STATUSREG_OVDS_MSK       0x8
/** @} */

/**   \addtogroup UVDS
 * @{
 */
/*!
 Under Voltage Proection alarm
 * -  0 = VddP is too low
 * -  1 = VddP is OK
*/
#define TFA98XX_STATUSREG_UVDS           (0x1<<4)
#define TFA98XX_STATUSREG_UVDS_POS       4
#define TFA98XX_STATUSREG_UVDS_LEN       1
#define TFA98XX_STATUSREG_UVDS_MAX       1
#define TFA98XX_STATUSREG_UVDS_MSK       0x10
/** @} */

/**   \addtogroup OCDS
 * @{
 */
/*!
 Over Current Protection alarm
 * -  0 = Current is OK
 * -  1 = Current is to High
*/
#define TFA98XX_STATUSREG_OCDS           (0x1<<5)
#define TFA98XX_STATUSREG_OCDS_POS       5
#define TFA98XX_STATUSREG_OCDS_LEN       1
#define TFA98XX_STATUSREG_OCDS_MAX       1
#define TFA98XX_STATUSREG_OCDS_MSK       0x20
/** @} */

/**   \addtogroup CLKS
 * @{
 */
/*!
 Clocks stable flag
 * -  0 = Clock is unstable
 * -  1 = Clock is Stable
*/
#define TFA98XX_STATUSREG_CLKS           (0x1<<6)
#define TFA98XX_STATUSREG_CLKS_POS       6
#define TFA98XX_STATUSREG_CLKS_LEN       1
#define TFA98XX_STATUSREG_CLKS_MAX       1
#define TFA98XX_STATUSREG_CLKS_MSK       0x40
/** @} */

/**   \addtogroup CLIPS
 * @{
 */
/*!
 Amplifier clipping
 * -  0 = Not clipping
 * -  1 = Clipping
*/
#define TFA98XX_STATUSREG_CLIPS          (0x1<<7)
#define TFA98XX_STATUSREG_CLIPS_POS      7
#define TFA98XX_STATUSREG_CLIPS_LEN      1
#define TFA98XX_STATUSREG_CLIPS_MAX      1
#define TFA98XX_STATUSREG_CLIPS_MSK      0x80
/** @} */

/**   \addtogroup MTPB
 * @{
 */
/*!
 MTP busy
 * -  0 = MTP is idle
 * -  1 = MTP is busy copying data to/from I2C registers
*/
#define TFA98XX_STATUSREG_MTPB           (0x1<<8)
#define TFA98XX_STATUSREG_MTPB_POS       8
#define TFA98XX_STATUSREG_MTPB_LEN       1
#define TFA98XX_STATUSREG_MTPB_MAX       1
#define TFA98XX_STATUSREG_MTPB_MSK       0x100
/** @} */

/**   \addtogroup DCCS
 * @{
 */
/*!
 * - 0 = Vboost not in window
 * - 1 = Vboost in window (OK)
*/
#define TFA9887_STATUSREG_DCCS        (0x1<<9)
#define TFA9887_STATUSREG_DCCS_POS        9
#define TFA9887_STATUSREG_DCCS_MAX        1
#define TFA9887_STATUSREG_DCCS_MSK        0x200
#define TFA9890_STATUSREG_DCCS        (0x1<<9)
#define TFA9890_STATUSREG_DCCS_POS        9
#define TFA9890_STATUSREG_DCCS_MAX        1
#define TFA9890_STATUSREG_DCCS_MSK        0x200
#define TFA9895_STATUSREG_DCCS        (0x1<<9)
#define TFA9895_STATUSREG_DCCS_POS        9
#define TFA9895_STATUSREG_DCCS_MAX        1
#define TFA9895_STATUSREG_DCCS_MSK        0x200
/** @} */


/**   \addtogroup NOCLK
 * @{
 */
/*!
 Flag lost clock from clock generation unit
 * -  1 = PLL reference clock input disappeared
 * -  0 = PLL reference clock detected
*/
#define TFA9897_STATUSREG_NOCLK          (0x1<<9)
#define TFA9897_STATUSREG_NOCLK_POS      9
#define TFA9897_STATUSREG_NOCLK_LEN      1
#define TFA9897_STATUSREG_NOCLK_MAX      1
#define TFA9897_STATUSREG_NOCLK_MSK      0x200
/** @} */

/**   \addtogroup SPKS
 * @{
 */
/*!
 Speaker error flag
 * -  0 = Speaker is OK
 * -  1 = Speaker error
*/
#define TFA98XX_STATUSREG_SPKS           (0x1<<10)
#define TFA98XX_STATUSREG_SPKS_POS       10
#define TFA98XX_STATUSREG_SPKS_LEN       1
#define TFA98XX_STATUSREG_SPKS_MAX       1
#define TFA98XX_STATUSREG_SPKS_MSK       0x400
/** @} */

/**   \addtogroup ACS
 * @{
 */
/*!
 Cold Start flag
 * -  0 = Not a cold start, already running
 * -  1 = Cold start (via POR)
*/
#define TFA98XX_STATUSREG_ACS            (0x1<<11)
#define TFA98XX_STATUSREG_ACS_POS        11
#define TFA98XX_STATUSREG_ACS_LEN        1
#define TFA98XX_STATUSREG_ACS_MAX        1
#define TFA98XX_STATUSREG_ACS_MSK        0x800
/** @} */

/**   \addtogroup SWS
 * @{
 */
/*!
 Flag Engage
 * -  0 = amplifier is not switching
 * -  1 = amplifier is switching
*/
#define TFA98XX_STATUSREG_SWS            (0x1<<12)
#define TFA98XX_STATUSREG_SWS_POS        12
#define TFA98XX_STATUSREG_SWS_LEN        1
#define TFA98XX_STATUSREG_SWS_MAX        1
#define TFA98XX_STATUSREG_SWS_MSK        0x1000
/** @} */

/**   \addtogroup WDS
 * @{
 */
/*!
 Flag watchdog reset
 * -  0 = no reset due to watchdog
 * -  1 = reset due to watchdog
*/
#define TFA98XX_STATUSREG_WDS            (0x1<<13)
#define TFA98XX_STATUSREG_WDS_POS        13
#define TFA98XX_STATUSREG_WDS_LEN        1
#define TFA98XX_STATUSREG_WDS_MAX        1
#define TFA98XX_STATUSREG_WDS_MSK        0x2000
/** @} */

/**   \addtogroup AMPS
 * @{
 */
/*!
 Amplifier is enabled by manager
 * -  0 = amplifier is not enabled
 * -  1 = amplifier is enabled
*/
#define TFA98XX_STATUSREG_AMPS           (0x1<<14)
#define TFA98XX_STATUSREG_AMPS_POS       14
#define TFA98XX_STATUSREG_AMPS_LEN       1
#define TFA98XX_STATUSREG_AMPS_MAX       1
#define TFA98XX_STATUSREG_AMPS_MSK       0x4000
/** @} */

/**   \addtogroup AREFS
 * @{
 */
/*!
 References are enabled by manager
 * -  0 = references are not enabled
 * -  1 = references are enabled
*/
#define TFA98XX_STATUSREG_AREFS          (0x1<<15)
#define TFA98XX_STATUSREG_AREFS_POS      15
#define TFA98XX_STATUSREG_AREFS_LEN      1
#define TFA98XX_STATUSREG_AREFS_MAX      1
#define TFA98XX_STATUSREG_AREFS_MSK      0x8000
/** @} */

/** @} */

/**  BatteryVoltage Register ($01) ********************************************/

/**   \addtogroup _0x01_BatteryVoltage
 * @{
 */
#define TFA98XX_BATTERYVOLTAGE     0x01
/**   \addtogroup BATS
 * @{
 */
/*!
 Battery voltage readout; 0[V]..5.5[V]
*/
#define TFA98XX_BATTERYVOLTAGE_BATS      (0x3ff<<0)
#define TFA98XX_BATTERYVOLTAGE_BATS_POS  0
#define TFA98XX_BATTERYVOLTAGE_BATS_LEN  10
#define TFA98XX_BATTERYVOLTAGE_BATS_MAX  1023
#define TFA98XX_BATTERYVOLTAGE_BATS_MSK  0x3ff
/** @} */

/**   \addtogroup 10
 * @{
 */
/*!
 not used
*/
#define TFA98XX_BATTERYVOLTAGE_10        (0x3f<<10)
#define TFA98XX_BATTERYVOLTAGE_10_POS    10
#define TFA98XX_BATTERYVOLTAGE_10_LEN    6
#define TFA98XX_BATTERYVOLTAGE_10_MAX    63
#define TFA98XX_BATTERYVOLTAGE_10_MSK    0xfc00
/** @} */

/** @} */

/**  Temperature Register ($02) ********************************************/

/**   \addtogroup _0x02_Temperature
 * @{
 */
#define TFA98XX_TEMPERATURE        0x02
/**   \addtogroup TEMPS
 * @{
 */
/*!
 Temperature readout
*/
#define TFA98XX_TEMPERATURE_TEMPS        (0x1ff<<0)
#define TFA98XX_TEMPERATURE_TEMPS_POS    0
#define TFA98XX_TEMPERATURE_TEMPS_LEN    9
#define TFA98XX_TEMPERATURE_TEMPS_MAX    511
#define TFA98XX_TEMPERATURE_TEMPS_MSK    0x1ff
/** @} */

/**   \addtogroup 9
 * @{
 */
/*!
 not used
*/
#define TFA98XX_TEMPERATURE_9            (0x7f<<9)
#define TFA98XX_TEMPERATURE_9_POS        9
#define TFA98XX_TEMPERATURE_9_LEN        7
#define TFA98XX_TEMPERATURE_9_MAX        127
#define TFA98XX_TEMPERATURE_9_MSK        0xfe00
/** @} */

/** @} */

/**  RevisionNumber Register ($03) ********************************************/

/**   \addtogroup _0x03_RevisionNumber
 * @{
 */
#define TFA98XX_REVISIONNUMBER     0x03
/**   \addtogroup REV
 * @{
 */
/*!
 Device type number = 97
*/
#define TFA98XX_REVISIONNUMBER_REV       (0xff<<0)
#define TFA98XX_REVISIONNUMBER_REV_POS   0
#define TFA98XX_REVISIONNUMBER_REV_LEN   8
#define TFA98XX_REVISIONNUMBER_REV_MAX   255
#define TFA98XX_REVISIONNUMBER_REV_MSK   0xff
/** @} */

/** @} */

/**  AudioReg Register ($04) ********************************************/

/**   \addtogroup _0x04_AudioReg_TFA9897
 * @{
 */
#define TFA9897_AUDIOREG           0x04
/**   \addtogroup 0
 * @{
 */
/*!
 reserved
*/
#define TFA9897_AUDIOREG_0               (0x7<<0)
#define TFA9897_AUDIOREG_0_POS           0
#define TFA9897_AUDIOREG_0_LEN           3
#define TFA9897_AUDIOREG_0_MAX           7
#define TFA9897_AUDIOREG_0_MSK           0x7
/** @} */

/**   \addtogroup CHS12
 * @{
 */
/*!
 Channel Selection TDM input for Coolflux
 * -  0 = Stereo
 * -  1 = Left [default]
 * -  2 = Right
 * -  3 = Mono =(L+R)/2
*/
#define TFA9897_AUDIOREG_CHS12           (0x3<<3)
#define TFA9897_AUDIOREG_CHS12_POS       3
#define TFA9897_AUDIOREG_CHS12_LEN       2
#define TFA9897_AUDIOREG_CHS12_MAX       3
#define TFA9897_AUDIOREG_CHS12_MSK       0x18
/** @} */

/**   \addtogroup ILVL
 * @{
 */
/*!
 Input level selection control
 * -  0 = input is -6 dbFS, attenuation is bypassed
 * -  1 = input is 0 dbFS, attenuated by 6 dBFS
*/
#define TFA9897_AUDIOREG_ILVL            (0x1<<5)
#define TFA9897_AUDIOREG_ILVL_POS        5
#define TFA9897_AUDIOREG_ILVL_LEN        1
#define TFA9897_AUDIOREG_ILVL_MAX        1
#define TFA9897_AUDIOREG_ILVL_MSK        0x20
/** @} */

/**   \addtogroup CHSA
 * @{
 */
/*!
 Input selection for amplifier
 * -  0 = TDM data channel1, CoolFlux bypassed
 * -  1 = TDM data channel2, CoolFlux bypassed
 * -  2 = Coolflux DSP Output [default]
 * -  3 = Coolflux DSP Output
*/
#define TFA9897_AUDIOREG_CHSA            (0x3<<6)
#define TFA9897_AUDIOREG_CHSA_POS        6
#define TFA9897_AUDIOREG_CHSA_LEN        2
#define TFA9897_AUDIOREG_CHSA_MAX        3
#define TFA9897_AUDIOREG_CHSA_MSK        0xc0
/** @} */

/**   \addtogroup 8
 * @{
 */
/*!

*/
#define TFA9897_AUDIOREG_8               (0xf<<8)
#define TFA9897_AUDIOREG_8_POS           8
#define TFA9897_AUDIOREG_8_LEN           4
#define TFA9897_AUDIOREG_8_MAX           15
#define TFA9897_AUDIOREG_8_MSK           0xf00
/** @} */

/**   \addtogroup AUDFS
 * @{
 */
/*!
 Audio sample rate setting
 * -  6 = 32 KHz
 * -  7 = 44.1 kHz
 * -  8 = 48 KHz [default]
 * -  Others = Reserved
*/
#define TFA9897_AUDIOREG_AUDFS           (0xf<<12)
#define TFA9897_AUDIOREG_AUDFS_POS       12
#define TFA9897_AUDIOREG_AUDFS_LEN       4
#define TFA9897_AUDIOREG_AUDFS_MAX       15
#define TFA9897_AUDIOREG_AUDFS_MSK       0xf000
/** @} */

/** @} */

/**  I2SReg Register ($04) ********************************************/

/**   \addtogroup _0x04_I2SReg_TFA9887
 * @{
 */
#define TFA9887_I2SREG             0x04
/**   \addtogroup I2SF
 * @{
 */
/*!
 I2SFormat data 1, 2 input and output:
 * -  0=not used
 * -  1=not used
 * -  2=MSB justify
 * -  3=Philips standard I2S (default)
 * -  4=LSB Justify 16 bits
 * -  5=LSB Justify 18 bits
 * -  6=LSB Justify 20 bits
 * -  7=LSB Justify 24 bits
*/
#define TFA9887_I2SREG_I2SF              (0x7<<0)
#define TFA9887_I2SREG_I2SF_POS          0
#define TFA9887_I2SREG_I2SF_LEN          3
#define TFA9887_I2SREG_I2SF_MAX          7
#define TFA9887_I2SREG_I2SF_MSK          0x7
/** @} */

/**   \addtogroup CHS12
 * @{
 */
/*!
 ChannelSelection data1 input  (In CoolFlux)
 * -  0=Stereo
 * -  1=Left
 * -  2=Right
 * -  3=Mono =(L+R)/2
*/
#define TFA9887_I2SREG_CHS12             (0x3<<3)
#define TFA9887_I2SREG_CHS12_POS         3
#define TFA9887_I2SREG_CHS12_LEN         2
#define TFA9887_I2SREG_CHS12_MAX         3
#define TFA9887_I2SREG_CHS12_MSK         0x18
/** @} */

/**   \addtogroup CHS3
 * @{
 */
/*!
 ChannelSelection data 3 input (coolflux input, the DCDC converter gets the other signal)
 * -  0=Left
 * -  1=Right
*/
#define TFA9887_I2SREG_CHS3              (0x1<<5)
#define TFA9887_I2SREG_CHS3_POS          5
#define TFA9887_I2SREG_CHS3_LEN          1
#define TFA9887_I2SREG_CHS3_MAX          1
#define TFA9887_I2SREG_CHS3_MSK          0x20
/** @} */

/**   \addtogroup CHSA
 * @{
 */
/*!
 Input selection for amplifier
 * -  00b                I2S input 1 left channel (CoolFlux bypassed)
 * -  01b                I2S input 1 Right channel (CoolFlux bypassed)
 * -  10b                Output Coolflux DSP
 * -  11b                Output Collflux DSP
*/
#define TFA9887_I2SREG_CHSA              (0x3<<6)
#define TFA9887_I2SREG_CHSA_POS          6
#define TFA9887_I2SREG_CHSA_LEN          2
#define TFA9887_I2SREG_CHSA_MAX          3
#define TFA9887_I2SREG_CHSA_MSK          0xc0
/** @} */

/**   \addtogroup I2SDOE
 * @{
 */
/*!
 data out tristate 0 = tristate
*/
#define TFA9887_I2SREG_I2SDOE            (0x1<<11)
#define TFA9887_I2SREG_I2SDOE_POS        11
#define TFA9887_I2SREG_I2SDOE_LEN        1
#define TFA9887_I2SREG_I2SDOE_MAX        1
#define TFA9887_I2SREG_I2SDOE_MSK        0x800
/** @} */

/**   \addtogroup I2SSR
 * @{
 */
/*!
 sample rate setting
 * -  0000b 8 kHz
 * -  0001b 11.025 kHz
 * -  0010b 12 kHz
 * -  0011b 16 kHz
 * -  0100b 22.05 kHz
 * -  0101b 24 kHz
 * -  0110b 32 kHz
 * -  0111b 44.1 kHz
 * -  1000b* 48 KHz
*/
#define TFA9887_I2SREG_I2SSR             (0xf<<12)
#define TFA9887_I2SREG_I2SSR_POS         12
#define TFA9887_I2SREG_I2SSR_LEN         4
#define TFA9887_I2SREG_I2SSR_MAX         15
#define TFA9887_I2SREG_I2SSR_MSK         0xf000
/** @} */

/** @} */

/**  I2SReg Register ($04) ********************************************/

/**   \addtogroup _0x04_I2SReg
 * @{
 */
#define TFA98XX_I2SREG             0x04
/**   \addtogroup I2SF
 * @{
 */
/*!
 I2SFormat data 1 input:
 * -  0 = Philips standard I2S
 * -  1 = Philips standard I2S
 * -  2 = MSB justify
 * -  3 = Philips standard I2S [default]
 * -  4 = LSB Justify 16 bits
 * -  5 = LSB Justify 18 bits
 * -  6 = LSB Justify 20 bits
 * -  7 = LSB Justify 24 bits
*/
#define TFA98XX_I2SREG_I2SF              (0x7<<0)
#define TFA98XX_I2SREG_I2SF_POS          0
#define TFA98XX_I2SREG_I2SF_LEN          3
#define TFA98XX_I2SREG_I2SF_MAX          7
#define TFA98XX_I2SREG_I2SF_MSK          0x7
/** @} */

/**   \addtogroup CHS12
 * @{
 */
/*!
 ChannelSelection data1 input  (In CoolFlux)
 * -  0 = Stereo
 * -  1 = Left [default]
 * -  2 = Right
 * -  3 = Mono =(L+R)/2
*/
#define TFA98XX_I2SREG_CHS12             (0x3<<3)
#define TFA98XX_I2SREG_CHS12_POS         3
#define TFA98XX_I2SREG_CHS12_LEN         2
#define TFA98XX_I2SREG_CHS12_MAX         3
#define TFA98XX_I2SREG_CHS12_MSK         0x18
/** @} */

/**   \addtogroup CHS3
 * @{
 */
/*!
 ChannelSelection data 2 input (coolflux input, the DCDC converter gets the other signal)
 * -  0 = Left channel to CF DSP right channel to other vamp mux [default]
 * -  1 = Right channel to CF DSP left channel to other vamp mux
*/
#define TFA98XX_I2SREG_CHS3              (0x1<<5)
#define TFA98XX_I2SREG_CHS3_POS          5
#define TFA98XX_I2SREG_CHS3_LEN          1
#define TFA98XX_I2SREG_CHS3_MAX          1
#define TFA98XX_I2SREG_CHS3_MSK          0x20
/** @} */

/**   \addtogroup CHSA
 * @{
 */
/*!
 Input selection for amplifier
 * -  0 = I2S input 1 left channel (CoolFlux bypassed)
 * -  1 = I2S input 1 Right channel (CoolFlux bypassed)
 * -  2 = Output Coolflux DSP [default]
 * -  3 = Output Collflux DSP
*/
#define TFA98XX_I2SREG_CHSA              (0x3<<6)
#define TFA98XX_I2SREG_CHSA_POS          6
#define TFA98XX_I2SREG_CHSA_LEN          2
#define TFA98XX_I2SREG_CHSA_MAX          3
#define TFA98XX_I2SREG_CHSA_MSK          0xc0
/** @} */

/**   \addtogroup I2SDOC
 * @{
 */
/*!
 selection data out
 * -  0 = I2S-TX [default]
 * -  1 = datai1
 * -  2 = datai2
 * -  3 = datai3
*/
#define TFA9890_I2SREG_I2SDOC            (0x3<<8)
#define TFA9890_I2SREG_I2SDOC_POS        8
#define TFA9890_I2SREG_I2SDOC_LEN        2
#define TFA9890_I2SREG_I2SDOC_MAX        3
#define TFA9890_I2SREG_I2SDOC_MSK        0x300
/** @} */

/**   \addtogroup DISP
 * @{
 */
/*!
 idp protection
 * -  0 = on
 * -  1 = off
*/
#define TFA9890_I2SREG_DISP              (0x1<<10)
#define TFA9890_I2SREG_DISP_POS          10
#define TFA9890_I2SREG_DISP_LEN          1
#define TFA9890_I2SREG_DISP_MAX          1
#define TFA9890_I2SREG_DISP_MSK          0x400
/** @} */

/**   \addtogroup I2SDOE
 * @{
 */
/*!
 Enable data output
 * -  0 = data output in tristate
 * -  1 = normal mode [default]
*/
#define TFA98XX_I2SREG_I2SDOE            (0x1<<11)
#define TFA98XX_I2SREG_I2SDOE_POS        11
#define TFA98XX_I2SREG_I2SDOE_LEN        1
#define TFA98XX_I2SREG_I2SDOE_MAX        1
#define TFA98XX_I2SREG_I2SDOE_MSK        0x800
/** @} */

/**   \addtogroup I2SSR
 * @{
 */
/*!
 sample rate setting
 * -  0 = 8kHk
 * -  1 =11.025kHz
 * -  2 = 12kHz
 * -  3 = 16kHz
 * -  4 = 22.05kHz
 * -  5 = 24kHz
 * -  6 = 32kHz
 * -  7 = 44.1kHz
 * -  8 = 48kHz [default]
*/
#define TFA98XX_I2SREG_I2SSR             (0xf<<12)
#define TFA98XX_I2SREG_I2SSR_POS         12
#define TFA98XX_I2SREG_I2SSR_LEN         4
#define TFA98XX_I2SREG_I2SSR_MAX         15
#define TFA98XX_I2SREG_I2SSR_MSK         0xf000
/** @} */

/** @} */

/**  bat_prot Register ($05) ********************************************/

/**   \addtogroup _0x05_bat_prot
 * @{
 */
#define TFA98XX_BAT_PROT           0x05
/**   \addtogroup BSSCR
 * @{
 */
/*!
 Protection Attack Time
 * -  0 = 0.56 dB/Sample
 * -  1 = 1.12 dB/sample
 * -  2 = 2.32 dB/Sample [default]
 * -  3 = infinite dB/Sample
*/
#define TFA989X_BAT_PROT_BSSCR           (0x3<<0)
#define TFA989X_BAT_PROT_BSSCR_POS       0
#define TFA989X_BAT_PROT_BSSCR_LEN       2
#define TFA989X_BAT_PROT_BSSCR_MAX       3
#define TFA989X_BAT_PROT_BSSCR_MSK       0x3
/** @} */

/**   \addtogroup BSST
 * @{
 */
/*!
 ProtectionThreshold
 * -     normal   steep
 * -  0 = 2.73V   2.99V
 * -  1 = 2.83V   3.09V
 * -  2 = 2.93V   3.19V
 * -  3 = 3.03V   3.29V
 * -  4 = 3.13V   3.39V (default)
 * -  5 = 3.23V   3.49V
 * -  6 = 3.33V   3.59V
 * -  7 = 3.43V   3.69V
 * -  8 = 3.53V   3.79V
 * -  9 = 3.63V   3.89V
 * -  10 = 3.73V   3.99V
 * -  11 = 3.83V   4.09V
 * -  12 = 3.93V   4.19V
 * -  13 = 4.03V   4.29V
 * -  14 = 4.13V   4.39V
 * -  15 = 4.23V   4.49V
*/
#define TFA989X_BAT_PROT_BSST            (0xf<<2)
#define TFA989X_BAT_PROT_BSST_POS        2
#define TFA989X_BAT_PROT_BSST_LEN        4
#define TFA989X_BAT_PROT_BSST_MAX        15
#define TFA989X_BAT_PROT_BSST_MSK        0x3c
/** @} */

/**   \addtogroup BSSRL
 * @{
 */
/*!
 Protection Maximum Reduction
 * -  0 = 3V
 * -  1 = 4V
 * -  2 = 5V [default]
 * -  3 = not permitted
*/
#define TFA98XX_BAT_PROT_BSSRL           (0x3<<6)
#define TFA98XX_BAT_PROT_BSSRL_POS       6
#define TFA98XX_BAT_PROT_BSSRL_LEN       2
#define TFA98XX_BAT_PROT_BSSRL_MAX       3
#define TFA98XX_BAT_PROT_BSSRL_MSK       0xc0
/** @} */

/**   \addtogroup BSSRR
 * @{
 */
/*!
 Battery Protection Release Time
 * -  0 = 0.4 sec
 * -  1 = 0.8 sec
 * -  2 = 1.2 sec
 * -  3 = 1.6 sec [default]
 * -  4 = 2 sec
 * -  5 = 2,4 sec
 * -  6 = 2.8 sec
 * -  7 = 3.2 sec
*/
#define TFA98XX_BAT_PROT_BSSRR           (0x7<<8)
#define TFA98XX_BAT_PROT_BSSRR_POS       8
#define TFA98XX_BAT_PROT_BSSRR_LEN       3
#define TFA98XX_BAT_PROT_BSSRR_MAX       7
#define TFA98XX_BAT_PROT_BSSRR_MSK       0x700
/** @} */

/**   \addtogroup BSSHY
 * @{
 */
/*!
 Battery Protection Hysterese
 * -  0 = No hysterese
 * -  1 = 0.05V
 * -  2 = 0.1V [default]
 * -  3 = 0.2V
*/
#define TFA98XX_BAT_PROT_BSSHY           (0x3<<11)
#define TFA98XX_BAT_PROT_BSSHY_POS       11
#define TFA98XX_BAT_PROT_BSSHY_LEN       2
#define TFA98XX_BAT_PROT_BSSHY_MAX       3
#define TFA98XX_BAT_PROT_BSSHY_MSK       0x1800
/** @} */

/**   \addtogroup 13
 * @{
 */
/*!
 reset clipper
 * -  0 = clipper is not reset if CF is bypassed [default]
 * -  1 = reset the clipper via I2C in case the CF is bypassed
*/
#define TFA98XX_BAT_PROT_13              (0x1<<13)
#define TFA98XX_BAT_PROT_13_POS          13
#define TFA98XX_BAT_PROT_13_LEN          1
#define TFA98XX_BAT_PROT_13_MAX          1
#define TFA98XX_BAT_PROT_13_MSK          0x2000
/** @} */

/**   \addtogroup BSSR
 * @{
 */
/*!
 battery voltage for I2C read out only
 * -  0 = minimum battery value [reset]
 * -  1 = avarage battery value
*/
#define TFA98XX_BAT_PROT_BSSR            (0x1<<14)
#define TFA98XX_BAT_PROT_BSSR_POS        14
#define TFA98XX_BAT_PROT_BSSR_LEN        1
#define TFA98XX_BAT_PROT_BSSR_MAX        1
#define TFA98XX_BAT_PROT_BSSR_MSK        0x4000
/** @} */

/**   \addtogroup BSSBY
 * @{
 */
/*!
 bypass clipper battery protection
 * -  0 = clipper active  [device default]
 * -  1 = clipper bypassed  [new default]
*/
#define TFA989X_BAT_PROT_BSSBY           (0x1<<15)
#define TFA989X_BAT_PROT_BSSBY_POS       15
#define TFA989X_BAT_PROT_BSSBY_LEN       1
#define TFA989X_BAT_PROT_BSSBY_MAX       1
#define TFA989X_BAT_PROT_BSSBY_MSK       0x8000
/** @} */

/** @} */

/**  bat_prot Register ($05) ********************************************/

/**   \addtogroup _0x05_bat_prot_TFA9887
 * @{
 */
#define TFA9887_BAT_PROT           0x05
/**   \addtogroup BSSBY
 * @{
 */
/*!

*/
#define TFA9887_BAT_PROT_BSSBY           (0x1<<0)
#define TFA9887_BAT_PROT_BSSBY_POS       0
#define TFA9887_BAT_PROT_BSSBY_LEN       1
#define TFA9887_BAT_PROT_BSSBY_MAX       1
#define TFA9887_BAT_PROT_BSSBY_MSK       0x1
/** @} */

/**   \addtogroup BSSCR
 * @{
 */
/*!
 00 = 0.56 dB/Sample
 * -  01 = 1.12 dB/sample
 * -  10 = 2.32 dB/Sample (default)
 * -  11 = infinite dB/Sample
*/
#define TFA9887_BAT_PROT_BSSCR           (0x3<<1)
#define TFA9887_BAT_PROT_BSSCR_POS       1
#define TFA9887_BAT_PROT_BSSCR_LEN       2
#define TFA9887_BAT_PROT_BSSCR_MAX       3
#define TFA9887_BAT_PROT_BSSCR_MSK       0x6
/** @} */

/**   \addtogroup BSST
 * @{
 */
/*!
 000 = 2.92V
 * -  001 = 3.05 V
 * -  010 = 3.17
 * -  011 = 3.3  V
 * -  100 = 3.42 V
 * -  101 - 3.55 V (default)
 * -  110 = 3.67 V
 * -  111 = 3.8 V
*/
#define TFA9887_BAT_PROT_BSST            (0x7<<3)
#define TFA9887_BAT_PROT_BSST_POS        3
#define TFA9887_BAT_PROT_BSST_LEN        3
#define TFA9887_BAT_PROT_BSST_MAX        7
#define TFA9887_BAT_PROT_BSST_MSK        0x38
/** @} */

/**   \addtogroup 13
 * @{
 */
/*!
 to reset the clipper via I2C in case the CF is bypassed
*/
#define TFA9887_BAT_PROT_13              (0x1<<13)
#define TFA9887_BAT_PROT_13_POS          13
#define TFA9887_BAT_PROT_13_LEN          1
#define TFA9887_BAT_PROT_13_MAX          1
#define TFA9887_BAT_PROT_13_MSK          0x2000
/** @} */

/**   \addtogroup I2SDOC
 * @{
 */
/*!
 selection data out
 * -  0 = I2S-TX; 1 = datai3
*/
#define TFA9887_BAT_PROT_I2SDOC          (0x1<<15)
#define TFA9887_BAT_PROT_I2SDOC_POS      15
#define TFA9887_BAT_PROT_I2SDOC_LEN      1
#define TFA9887_BAT_PROT_I2SDOC_MAX      1
#define TFA9887_BAT_PROT_I2SDOC_MSK      0x8000
/** @} */

/** @} */

/**  audio_ctr Register ($06) ********************************************/

/**   \addtogroup _0x06_audio_ctr
 * @{
 */
#define TFA98XX_AUDIO_CTR          0x06
/**   \addtogroup DPSA
 * @{
 */
/*!
 Enable dynamic powerstage activation
 * -  0 = dpsa off
 * -  1 = dpsa on [default]
*/
#define TFA98XX_AUDIO_CTR_DPSA           (0x1<<0)
#define TFA98XX_AUDIO_CTR_DPSA_POS       0
#define TFA98XX_AUDIO_CTR_DPSA_LEN       1
#define TFA98XX_AUDIO_CTR_DPSA_MAX       1
#define TFA98XX_AUDIO_CTR_DPSA_MSK       0x1
/** @} */

/**   \addtogroup control slope
 * @{
 */
/*!
   0 =  slope 1
     1 =  slope 1
    2 = slope 1
     3 = slope 2
     4 = slope 1
     5 = slope 2
     6 = slope 2
     7 = slope 3  [default]
     8 = slope 1
     9 = slope 2
     10 = slope 2
     11 = slope 3
     12 = slope 2
     13 = slope 3
     14 = slope 3
   15 = maximal
*/
#define TFA98XX_AUDIO_CTR_AMPSL       (0xf<<1)
#define TFA98XX_AUDIO_CTR_AMPSL_POS       1
#define TFA98XX_AUDIO_CTR_AMPSL_LEN       4
#define TFA98XX_AUDIO_CTR_AMPSL_MAX       15
#define TFA98XX_AUDIO_CTR_AMPSL_MSK       0x1e
/** @} */

/**   \addtogroup CFSM
 * @{
 */
/*!
 Soft mute in CoolFlux
 * -  0= no mute [default]
 * -  1= muted
*/
#define TFA98XX_AUDIO_CTR_CFSM           (0x1<<5)
#define TFA98XX_AUDIO_CTR_CFSM_POS       5
#define TFA98XX_AUDIO_CTR_CFSM_LEN       1
#define TFA98XX_AUDIO_CTR_CFSM_MAX       1
#define TFA98XX_AUDIO_CTR_CFSM_MSK       0x20
/** @} */

/**   \addtogroup 6
 * @{
 */
/*!

*/
#define TFA98XX_AUDIO_CTR_6              (0x1<<6)
#define TFA98XX_AUDIO_CTR_6_POS          6
#define TFA98XX_AUDIO_CTR_6_LEN          1
#define TFA98XX_AUDIO_CTR_6_MAX          1
#define TFA98XX_AUDIO_CTR_6_MSK          0x40
/** @} */

/**   \addtogroup BSSS
 * @{
 */
/*!
 batsensesteepness
 * -  0 = 5.4V/V if ctr_supplysense = 1
 * -  1 = 10.8V/V if ctrl_supplysense = 1
 * -  0 = 3.13V/V if ctrl_supplysense = 0
 * -  1 = 6.25V/V if ctrl_supplysense = 0
*/
#define TFA989X_AUDIO_CTR_BSSS           (0x1<<7)
#define TFA989X_AUDIO_CTR_BSSS_POS       7
#define TFA989X_AUDIO_CTR_BSSS_LEN       1
#define TFA989X_AUDIO_CTR_BSSS_MAX       1
#define TFA989X_AUDIO_CTR_BSSS_MSK       0x80
/** @} */

/**   \addtogroup VOL
 * @{
 */
/*!
 volume control (in CoolFlux)
*/
#define TFA98XX_AUDIO_CTR_VOL            (0xff<<8)
#define TFA98XX_AUDIO_CTR_VOL_POS        8
#define TFA98XX_AUDIO_CTR_VOL_LEN        8
#define TFA98XX_AUDIO_CTR_VOL_MAX        255
#define TFA98XX_AUDIO_CTR_VOL_MSK        0xff00
/** @} */

/** @} */

/**  DCDCboost Register ($07) ********************************************/

/**   \addtogroup _0x07_DCDCboost
 * @{
 */
#define TFA98XX_DCDCBOOST          0x07
/**   \addtogroup DCVO
 * @{
 */
/*!
 Boost Voltage
 * -  0 = 4.0 V
 * -  1 = 4.2 V
 * -  2 = 4.4 V
 * -  3 = 4.6 V
 * -  4 = 4.8 V
 * -  5 = 5.0 V
 * -  6 = 5.2 V (default)
 * -  7 = 5.4 V
*/
#define TFA98XX_DCDCBOOST_DCVO           (0x7<<0)
#define TFA98XX_DCDCBOOST_DCVO_POS       0
#define TFA98XX_DCDCBOOST_DCVO_LEN       3
#define TFA98XX_DCDCBOOST_DCVO_MAX       7
#define TFA98XX_DCDCBOOST_DCVO_MSK       0x7
/** @} */

/**   \addtogroup DCMCC
 * @{
 */
/*!
 for 87,90,95
 * - 0 = 0.48 A
 * - 1 = 0.96 A
 * - 2 = 1.44 A
 * - 3 = 1.92 A
 * - 4 = 2.4  A [87/95 default]
 * - 5 = 2.88 A
 * - 6 = 3.56 A
 * - 7 = 3.8  A [90 default]
*/
#define TFA98XX_DCDCBOOST_DCMCC       (0x7<<3)
#define TFA98XX_DCDCBOOST_DCMCC_POS       3
#define TFA98XX_DCDCBOOST_DCMCC_LEN       3
#define TFA98XX_DCDCBOOST_DCMCC_MAX       7
#define TFA98XX_DCDCBOOST_DCMCC_MSK       0x38
#define TFA9897_DCDCBOOST_DCMCC          (0xf<<3)
#define TFA9897_DCDCBOOST_DCMCC_POS      3
#define TFA9897_DCDCBOOST_DCMCC_LEN      4
#define TFA9897_DCDCBOOST_DCMCC_MAX      15
#define TFA9897_DCDCBOOST_DCMCC_MSK      0x78

/**   \addtogroup DCIE
 * @{
 */
/*!
 Adaptive / Intelligent boost mode
 * -  0 = Off
 * -  1 = On [default]
*/
#define TFA98XX_DCDCBOOST_DCIE           (0x1<<10)
#define TFA98XX_DCDCBOOST_DCIE_POS       10
#define TFA98XX_DCDCBOOST_DCIE_LEN       1
#define TFA98XX_DCDCBOOST_DCIE_MAX       1
#define TFA98XX_DCDCBOOST_DCIE_MSK       0x400
/** @} */

/**   \addtogroup DCSR
 * @{
 */
/*!
 Soft RampUp/Down mode for DCDC controller
 * -  0 =  Immediate               : 0 Cycle
 * -  1 =  Fast      (Default)    : 32 Cycles/step at 2MHz, 16 cycles/step at 1MHz and 0.5MHz
*/
#define TFA98XX_DCDCBOOST_DCSR           (0x1<<11)
#define TFA98XX_DCDCBOOST_DCSR_POS       11
#define TFA98XX_DCDCBOOST_DCSR_LEN       1
#define TFA98XX_DCDCBOOST_DCSR_MAX       1
#define TFA98XX_DCDCBOOST_DCSR_MSK       0x800
/** @} */


/** @} */

/**  spkr_calibration Register ($08) ********************************************/

/**   \addtogroup _0x08_spkr_calibration
 * @{
 */
#define TFA98XX_SPKR_CALIBRATION   0x08
/**   \addtogroup TROS
 * @{
 */
/*!
 Select external temperature also the ext_temp will be put on the temp read out
 * -  0 = internal temperature
 * -  1 = external temperature
*/
#define TFA98XX_SPKR_CALIBRATION_TROS     (0x1<<0)
#define TFA98XX_SPKR_CALIBRATION_TROS_POS 0
#define TFA98XX_SPKR_CALIBRATION_TROS_LEN 1
#define TFA98XX_SPKR_CALIBRATION_TROS_MAX 1
#define TFA98XX_SPKR_CALIBRATION_TROS_MSK 0x1
/** @} */

/**   \addtogroup EXTTS
 * @{
 */
/*!
 external temperature setting to be given by host
*/
#define TFA98XX_SPKR_CALIBRATION_EXTTS     (0x1ff<<1)
#define TFA98XX_SPKR_CALIBRATION_EXTTS_POS 1
#define TFA98XX_SPKR_CALIBRATION_EXTTS_LEN 9
#define TFA98XX_SPKR_CALIBRATION_EXTTS_MAX 511
#define TFA98XX_SPKR_CALIBRATION_EXTTS_MSK 0x3fe
/** @} */

/**   \addtogroup 10
 * @{
 */
/*!

*/
#define TFA98XX_SPKR_CALIBRATION_10      (0x1<<10)
#define TFA98XX_SPKR_CALIBRATION_10_POS  10
#define TFA98XX_SPKR_CALIBRATION_10_LEN  1
#define TFA98XX_SPKR_CALIBRATION_10_MAX  1
#define TFA98XX_SPKR_CALIBRATION_10_MSK  0x400
/** @} */

/**   \addtogroup DCSYN
 * @{
 */
/*!
 DCDC synchronisation off + 7 positions
 * -  0 = off + 7 positions
 * -  1 = off
 * -  2 = on min
 * -  3 = on, 3
 * -  4 = on, 4
 * -  5 = on, 5
 * -  6 = on, 6
 * -  7 = on, max
*/
#define TFA98XX_SPKR_CALIBRATION_DCSYN     (0x7<<11)
#define TFA98XX_SPKR_CALIBRATION_DCSYN_POS 11
#define TFA98XX_SPKR_CALIBRATION_DCSYN_LEN 3
#define TFA98XX_SPKR_CALIBRATION_DCSYN_MAX 7
#define TFA98XX_SPKR_CALIBRATION_DCSYN_MSK 0x3800
/** @} */


/**  sys_ctrl Register ($09) ********************************************/

/**   \addtogroup _0x09_sys_ctrl
 * @{
 */
#define TFA98XX_SYS_CTRL           0x09
/**   \addtogroup PWDN
 * @{
 */
/*!
 Device Mode
 * -  0 = Device is set in operating mode
 * -  1 = Device is set in Powerdown mode [default]
*/
#define TFA98XX_SYS_CTRL_PWDN            (0x1<<0)
#define TFA98XX_SYS_CTRL_PWDN_POS        0
#define TFA98XX_SYS_CTRL_PWDN_LEN        1
#define TFA98XX_SYS_CTRL_PWDN_MAX        1
#define TFA98XX_SYS_CTRL_PWDN_MSK        0x1
/** @} */

/**   \addtogroup I2CR
 * @{
 */
/*!
 I2C Reset
 * -  0 = Normal operation [default]
 * -  1 = Reset all register to default
*/
#define TFA98XX_SYS_CTRL_I2CR            (0x1<<1)
#define TFA98XX_SYS_CTRL_I2CR_POS        1
#define TFA98XX_SYS_CTRL_I2CR_LEN        1
#define TFA98XX_SYS_CTRL_I2CR_MAX        1
#define TFA98XX_SYS_CTRL_I2CR_MSK        0x2
/** @} */

/**   \addtogroup CFE
 * @{
 */
/*!
 Enable CoolFlux
 * -  0 = Coolflux OFF
 * -  1 = Coolflux ON [default]
*/
#define TFA98XX_SYS_CTRL_CFE             (0x1<<2)
#define TFA98XX_SYS_CTRL_CFE_POS         2
#define TFA98XX_SYS_CTRL_CFE_LEN         1
#define TFA98XX_SYS_CTRL_CFE_MAX         1
#define TFA98XX_SYS_CTRL_CFE_MSK         0x4
/** @} */

/**   \addtogroup AMPE
 * @{
 */
/*!
 Enable Amplifier
 * -  0 = Amplifier OFF
 * -  1 = Amplifier ON [default]
*/
#define TFA98XX_SYS_CTRL_AMPE            (0x1<<3)
#define TFA98XX_SYS_CTRL_AMPE_POS        3
#define TFA98XX_SYS_CTRL_AMPE_LEN        1
#define TFA98XX_SYS_CTRL_AMPE_MAX        1
#define TFA98XX_SYS_CTRL_AMPE_MSK        0x8
/** @} */

/**   \addtogroup DCA
 * @{
 */
/*!
 EnableBoost
 * -  0 = Boost OFF (Follower mode) [default]
 * -  1 = Boost ON
*/
#define TFA98XX_SYS_CTRL_DCA             (0x1<<4)
#define TFA98XX_SYS_CTRL_DCA_POS         4
#define TFA98XX_SYS_CTRL_DCA_LEN         1
#define TFA98XX_SYS_CTRL_DCA_MAX         1
#define TFA98XX_SYS_CTRL_DCA_MSK         0x10
/** @} */

/**   \addtogroup SBSL
 * @{
 */
/*!
 Coolflux configured
 * -  0 = coolflux not configured [default]
 * -  1 = coolflux configured
*/
#define TFA98XX_SYS_CTRL_SBSL            (0x1<<5)
#define TFA98XX_SYS_CTRL_SBSL_POS        5
#define TFA98XX_SYS_CTRL_SBSL_LEN        1
#define TFA98XX_SYS_CTRL_SBSL_MAX        1
#define TFA98XX_SYS_CTRL_SBSL_MSK        0x20
/** @} */

/**   \addtogroup AMPC
 * @{
 */
/*!
 Selection on how Amplifier is enabled
 * -  0 = Enable amplifier independent of CoolFlux [default]
 * -  1 = CoolFlux enables amplifier (SW_Bit: cf_enbl_amplifier)
*/
#define TFA98XX_SYS_CTRL_AMPC            (0x1<<6)
#define TFA98XX_SYS_CTRL_AMPC_POS        6
#define TFA98XX_SYS_CTRL_AMPC_LEN        1
#define TFA98XX_SYS_CTRL_AMPC_MAX        1
#define TFA98XX_SYS_CTRL_AMPC_MSK        0x40
/** @} */

/**   \addtogroup DCDIS
 * @{
 */
/*!
 DCDC not connected
 * -  0 = normal DCDC functionality [default]
 * -  1 = DCDC switched off
*/
#define TFA98XX_SYS_CTRL_DCDIS           (0x1<<7)
#define TFA98XX_SYS_CTRL_DCDIS_POS       7
#define TFA98XX_SYS_CTRL_DCDIS_LEN       1
#define TFA98XX_SYS_CTRL_DCDIS_MAX       1
#define TFA98XX_SYS_CTRL_DCDIS_MSK       0x80
/** @} */

/**   \addtogroup PSDR
 * @{
 */
/*!
 IDDQ test amplifier
 * -  0 = amplifier is normal mode [default]
 * -  1 = amplifier is in the test mode
*/
#define TFA98XX_SYS_CTRL_PSDR            (0x1<<8)
#define TFA98XX_SYS_CTRL_PSDR_POS        8
#define TFA98XX_SYS_CTRL_PSDR_LEN        1
#define TFA98XX_SYS_CTRL_PSDR_MAX        1
#define TFA98XX_SYS_CTRL_PSDR_MSK        0x100
/** @} */

/**   \addtogroup DCCV
 * @{
 */
/*!
 Coil Value
 * -  0 = 0.7 uH
 * -  1 = 1.0 uH  [new default]
 * -  2 = 1.5 uH  [device default]
 * -  3 = 2.2 uH
*/
#define TFA98XX_SYS_CTRL_DCCV            (0x3<<9)
#define TFA98XX_SYS_CTRL_DCCV_POS        9
#define TFA98XX_SYS_CTRL_DCCV_LEN        2
#define TFA98XX_SYS_CTRL_DCCV_MAX        3
#define TFA98XX_SYS_CTRL_DCCV_MSK        0x600
/** @} */

/**   \addtogroup ISEL
 * @{
 */
/*!
 selection input 1 or 2
 *     0 = input 1 [default]
 *     1 = input 2
*/
#define TFA98XX_SYS_CTRL_ISEL         (0x1<<13)
#define TFA98XX_SYS_CTRL_ISEL_POS         13
#define TFA98XX_SYS_CTRL_ISEL_LEN         1
#define TFA98XX_SYS_CTRL_ISEL_MAX         1
#define TFA98XX_SYS_CTRL_ISEL_MSK         0x2000
/** @} */

/**   \addtogroup INTPAD
 * @{
 */
/*!
 INT pad configuration control
 * -  00 = INT is active low driven all the time
 * -  01 = INT is active high driven all the time
 * -  10 = INT pad in pull up mode, driven 0 only when interrupt is raised
 * -  11 = INT pad in pull down mode, driven 1 only when interrupt is raised
*/
#define TFA9897_SYS_CTRL_INTPAD          (0x3<<12)
#define TFA9897_SYS_CTRL_INTPAD_POS      12
#define TFA9897_SYS_CTRL_INTPAD_LEN      2
#define TFA9897_SYS_CTRL_INTPAD_MAX      3
#define TFA9897_SYS_CTRL_INTPAD_MSK      0x3000
/** @} */

/**   \addtogroup IPLL
 * @{
 */
/*!
 PLL input refrence clock selection
 * -  0 = Bit clock BCK [default]
 * -  1 = Frame Sync FS
*/
#define TFA98XX_SYS_CTRL_IPLL            (0x1<<14)
#define TFA98XX_SYS_CTRL_IPLL_POS        14
#define TFA98XX_SYS_CTRL_IPLL_LEN        1
#define TFA98XX_SYS_CTRL_IPLL_MAX        1
#define TFA98XX_SYS_CTRL_IPLL_MSK        0x4000
/** @} */

/**   \addtogroup CFCLK
 * @{
 */
/*!
 Coolflux sub-system clock
 * -  0 = clk_e selected for CF sub-system
 * -  1 = I2C clock SCL selected for CF sub-system
*/
#define TFA98XX_SYS_CTRL_CFCLK           (0x1<<15)
#define TFA98XX_SYS_CTRL_CFCLK_POS       15
#define TFA98XX_SYS_CTRL_CFCLK_LEN       1
#define TFA98XX_SYS_CTRL_CFCLK_MAX       1
#define TFA98XX_SYS_CTRL_CFCLK_MSK       0x8000
/** @} */

/** @} */

/**  sys_ctrl Register ($09) ********************************************/

/**   \addtogroup _0x09_sys_ctrl
 * @{
 */
#define TFA98XX_SYS_CTRL           0x09
/**   \addtogroup ISEL
 * @{
 */
/*!
 selection input 1 or 2
 * -  0 = input 1 [default]
 * -  1 = input 2
*/
#define TFA9890_SYS_CTRL_ISEL            (0x1<<13)
#define TFA9890_SYS_CTRL_ISEL_POS        13
#define TFA9890_SYS_CTRL_ISEL_LEN        1
#define TFA9890_SYS_CTRL_ISEL_MAX        1
#define TFA9890_SYS_CTRL_ISEL_MSK        0x2000
/** @} */

/** @} */

/**  I2S_sel_reg Register ($0a) ********************************************/

/**   \addtogroup _0x0a_I2S_sel_reg
 * @{
 */
#define TFA98XX_I2S_SEL_REG        0x0a
#define TFA9890_I2S_SEL_REG_POR     0x3ec3
/**   \addtogroup DOLS
 * @{
 */
/*!
 Output selection dataout left channel
 * -  0=CurrentSense signal
 * -  1=Coolflux output 3 (e.g. gain)
 * -  2=Coolflux output 2 (second channel)
 * -  3=Coolflux output 1 (main channel) [default]
 * -  4=datai3 left
 * -  5=datai3 right
 * -  6= dcdc feedforward audio current
*/
#define TFA98XX_I2S_SEL_REG_DOLS         (0x7<<0)
#define TFA98XX_I2S_SEL_REG_DOLS_POS     0
#define TFA98XX_I2S_SEL_REG_DOLS_LEN     3
#define TFA98XX_I2S_SEL_REG_DOLS_MAX     7
#define TFA98XX_I2S_SEL_REG_DOLS_MSK     0x7
/** @} */

/**   \addtogroup DORS
 * @{
 */
/*!
 Output selection dataout right channel
 * -  0=CurrentSense signal [default]
 * -  1=Coolflux output 3 (e.g. gain)
 * -  2=Coolflux output 2 (second channel)
 * -  3=Coolflux output 1 (main channel)
 * -  4=datai3 left
 * -  5=datai3 right
 * -  6= dcdc feedforward audio current
*/
#define TFA98XX_I2S_SEL_REG_DORS         (0x7<<3)
#define TFA98XX_I2S_SEL_REG_DORS_POS     3
#define TFA98XX_I2S_SEL_REG_DORS_LEN     3
#define TFA98XX_I2S_SEL_REG_DORS_MAX     7
#define TFA98XX_I2S_SEL_REG_DORS_MSK     0x38
/** @} */

/**   \addtogroup SPKL
 * @{
 */
/*!
 Selection speaker induction
 * -  0 = 22 uH
 * -  1 = 27 uH
 * -  2 = 33 uH
 * -  3 = 39 uH [default]
 * -  4 = 47 uH
 * -  5 = 56 uH
 * -  6 = 68 uH
 * -  7 = 82 uH
*/
#define TFA98XX_I2S_SEL_REG_SPKL         (0x7<<6)
#define TFA98XX_I2S_SEL_REG_SPKL_POS     6
#define TFA98XX_I2S_SEL_REG_SPKL_LEN     3
#define TFA98XX_I2S_SEL_REG_SPKL_MAX     7
#define TFA98XX_I2S_SEL_REG_SPKL_MSK     0x1c0
/** @} */

/**   \addtogroup SPKR
 * @{
 */
/*!
 Selection speaker impedance
 * -  0 = defined by DSP
 * -  1 = 4 ohm
 * -  2 = 6 ohm
 * -  3 = 8 ohm [default]
*/
#define TFA98XX_I2S_SEL_REG_SPKR         (0x3<<9)
#define TFA98XX_I2S_SEL_REG_SPKR_POS     9
#define TFA98XX_I2S_SEL_REG_SPKR_LEN     2
#define TFA98XX_I2S_SEL_REG_SPKR_MAX     3
#define TFA98XX_I2S_SEL_REG_SPKR_MSK     0x600
/** @} */

/**   \addtogroup DCFG
 * @{
 */
/*!
 DCDC speaker current compensation gain
 * -  0 = Off [new default]
 * -  1 = 70%
 * -  2 = 75%
 * -  3 = 80%
 * -  4 = 85%
 * -  5 = 90%
 * -  6 = 95%
 * -  7 = 100% [device default]
 * -  8 = 105%
 * -  9 = 110%
 * -  10 = 115%
 * -  11 = 120%
 * -  12 = 125%
 * -  13 = 130%
 * -  14 = 135%
 * -  15 = 140%
*/
#define TFA98XX_I2S_SEL_REG_DCFG         (0xf<<11)
#define TFA98XX_I2S_SEL_REG_DCFG_POS     11
#define TFA98XX_I2S_SEL_REG_DCFG_LEN     4
#define TFA98XX_I2S_SEL_REG_DCFG_MAX     15
#define TFA98XX_I2S_SEL_REG_DCFG_MSK     0x7800
/** @} */

/**   \addtogroup 15
 * @{
 */
/*!
 DCDC speaker current compensation sign
 * -  0 = positive [default]
 * -  1 = negative
*/
#define TFA98XX_I2S_SEL_REG_15           (0x1<<15)
#define TFA98XX_I2S_SEL_REG_15_POS       15
#define TFA98XX_I2S_SEL_REG_15_LEN       1
#define TFA98XX_I2S_SEL_REG_15_MAX       1
#define TFA98XX_I2S_SEL_REG_15_MSK       0x8000
/** @} */

/** @} */

/**  mtpkey2_reg Register ($0b) ********************************************/

/**   \addtogroup _0x0b_mtpkey2_reg
 * @{
 */
#define TFA98XX_MTPKEY2_REG        0x0b
/**   \addtogroup MTPK
 * @{
 */
/*!
 5Ah, 90d To access KEY1_Protected registers (=Default for engineering)
*/
#define TFA98XX_MTPKEY2_REG_MTPK         (0xff<<0)
#define TFA98XX_MTPKEY2_REG_MTPK_POS     0
#define TFA98XX_MTPKEY2_REG_MTPK_LEN     8
#define TFA98XX_MTPKEY2_REG_MTPK_MAX     255
#define TFA98XX_MTPKEY2_REG_MTPK_MSK     0xff
/** @} */

/**   \addtogroup 8
 * @{
 */
/*!
 not used
*/
#define TFA98XX_MTPKEY2_REG_8            (0xff<<8)
#define TFA98XX_MTPKEY2_REG_8_POS        8
#define TFA98XX_MTPKEY2_REG_8_LEN        8
#define TFA98XX_MTPKEY2_REG_8_MAX        255
#define TFA98XX_MTPKEY2_REG_8_MSK        0xff00
/** @} */

/** @} */

/**  voltage_sense_config Register ($0c) ********************************************/

/**   \addtogroup _0x0c_voltage_sense_config
 * @{
 */
#define TFA98XX_VOLTAGE_SENSE_CONFIG  0x0c
/**   \addtogroup VSENA
 * @{
 */
/*!
 Voltage sense enabling control bit
 * -  0 = disables the voltage sense measurment
 * -  1 = enables the voltage sense measurment
*/
#define TFA98XX_VOLTAGE_SENSE_CONFIG_VSENA     (0x1<<0)
#define TFA98XX_VOLTAGE_SENSE_CONFIG_VSENA_POS 0
#define TFA98XX_VOLTAGE_SENSE_CONFIG_VSENA_LEN 1
#define TFA98XX_VOLTAGE_SENSE_CONFIG_VSENA_MAX 1
#define TFA98XX_VOLTAGE_SENSE_CONFIG_VSENA_MSK 0x1
/** @} */

/**   \addtogroup VSPWM
 * @{
 */
/*!
 Voltage sense PWM source selection control
 * -  0 = PWM signal from coolflux output
 * -  1 = PWM signal from analog clipper module
*/
#define TFA98XX_VOLTAGE_SENSE_CONFIG_VSPWM     (0x1<<1)
#define TFA98XX_VOLTAGE_SENSE_CONFIG_VSPWM_POS 1
#define TFA98XX_VOLTAGE_SENSE_CONFIG_VSPWM_LEN 1
#define TFA98XX_VOLTAGE_SENSE_CONFIG_VSPWM_MAX 1
#define TFA98XX_VOLTAGE_SENSE_CONFIG_VSPWM_MSK 0x2
/** @} */

/**   \addtogroup 2
 * @{
 */
/*!

*/
#define TFA98XX_VOLTAGE_SENSE_CONFIG_2     (0x3fff<<2)
#define TFA98XX_VOLTAGE_SENSE_CONFIG_2_POS 2
#define TFA98XX_VOLTAGE_SENSE_CONFIG_2_LEN 14
#define TFA98XX_VOLTAGE_SENSE_CONFIG_2_MAX 16383
#define TFA98XX_VOLTAGE_SENSE_CONFIG_2_MSK 0xfffc
/** @} */

/**   cgu_clock_sync_reg Register ($0c) **/
#define TFA98XX_CGU_CLOCK_SYNC_REG  0x0c
#define TFA9890_CGU_CLOCK_SYNC_REG_POR  0x8000
/* Delay count for clock synchronisation */
#define TFA98XX_CGU_CLOCK_SYNC_REG_0  (0x1fff<<0)
#define TFA98XX_CGU_CLOCK_SYNC_REG_0_POS  0
#define TFA98XX_CGU_CLOCK_SYNC_REG_0_MAX  8191
#define TFA98XX_CGU_CLOCK_SYNC_REG_0_MSK  0x1fff
/* not used */
#define TFA98XX_CGU_CLOCK_SYNC_REG_13  (0x3<<13)
#define TFA98XX_CGU_CLOCK_SYNC_REG_13_POS  13
#define TFA98XX_CGU_CLOCK_SYNC_REG_13_MAX  3
#define TFA98XX_CGU_CLOCK_SYNC_REG_13_MSK  0x6000
/* Enable CGU clock synchronisation */
#define TFA98XX_CGU_CLOCK_SYNC_REG_15  (0x1<<15)
#define TFA98XX_CGU_CLOCK_SYNC_REG_15_POS  15
#define TFA98XX_CGU_CLOCK_SYNC_REG_15_MAX  1
#define TFA98XX_CGU_CLOCK_SYNC_REG_15_MSK  0x8000

/**   adc_sync_reg Register ($0d) **/
#define TFA98XX_ADC_SYNC_REG        0x0d
#define TFA9890_ADC_SYNC_REG_POR    0x8000
/* Delay count for ADC synchronisation */
#define TFA98XX_ADC_SYNC_REG_0        (0x1fff<<0)
#define TFA98XX_ADC_SYNC_REG_0_POS        0
#define TFA98XX_ADC_SYNC_REG_0_MAX        8191
#define TFA98XX_ADC_SYNC_REG_0_MSK        0x1fff
/* not used */
#define TFA98XX_ADC_SYNC_REG_13       (0x3<<13)
#define TFA98XX_ADC_SYNC_REG_13_POS       13
#define TFA98XX_ADC_SYNC_REG_13_MAX       3
#define TFA98XX_ADC_SYNC_REG_13_MSK       0x6000
/* Enable ADC synchronisation */
#define TFA98XX_ADC_SYNC_REG_15       (0x1<<15)
#define TFA98XX_ADC_SYNC_REG_15_POS       15
#define TFA98XX_ADC_SYNC_REG_15_MAX       1
#define TFA98XX_ADC_SYNC_REG_15_MSK       0x8000

/**   reserved_1 Register ($0e) **/
#define TFA98XX_RESERVED_1          0x0e
#define TFA9890_RESERVED_1_POR      0x0f01
/* to switch off dcdc reduction with bat prot  */
#define TFA98XX_RESERVED_1_0          (0x1<<0)
#define TFA98XX_RESERVED_1_0_POS          0
#define TFA98XX_RESERVED_1_0_MAX          1
#define TFA98XX_RESERVED_1_0_MSK          0x1
/* test option for frinch caps */
#define TFA98XX_RESERVED_1_1          (0x1<<1)
#define TFA98XX_RESERVED_1_1_POS          1
#define TFA98XX_RESERVED_1_1_MAX          1
#define TFA98XX_RESERVED_1_1_MSK          0x2
/* for extra connections digital to analog  */
#define TFA98XX_RESERVED_1_2          (0x1f<<2)
#define TFA98XX_RESERVED_1_2_POS          2
#define TFA98XX_RESERVED_1_2_MAX          31
#define TFA98XX_RESERVED_1_2_MSK          0x7c
/* icomp dem switch */
#define TFA98XX_RESERVED_1_7          (0x1<<7)
#define TFA98XX_RESERVED_1_7_POS          7
#define TFA98XX_RESERVED_1_7_MAX          1
#define TFA98XX_RESERVED_1_7_MSK          0x80
/*  */
#define TFA98XX_RESERVED_1_8          (0xff<<8)
#define TFA98XX_RESERVED_1_8_POS          8
#define TFA98XX_RESERVED_1_8_MAX          255
#define TFA98XX_RESERVED_1_8_MSK          0xff00


/** @} */

/**  interrupt_reg Register ($0f) ********************************************/

/**   \addtogroup _0x0f_interrupt_reg
 * @{
 */
#define TFA98XX_INTERRUPT_REG      0x0f
/**   \addtogroup VDDD
 * @{
 */
/*!
 mask flag_por for interupt generation
 * -  0 = enable interrupt
 * -  1 = mask interrupt
*/
#define TFA98XX_INTERRUPT_REG_VDDD       (0x1<<0)
#define TFA98XX_INTERRUPT_REG_VDDD_POS   0
#define TFA98XX_INTERRUPT_REG_VDDD_LEN   1
#define TFA98XX_INTERRUPT_REG_VDDD_MAX   1
#define TFA98XX_INTERRUPT_REG_VDDD_MSK   0x1
/** @} */

/**   \addtogroup OTDD
 * @{
 */
/*!
 mask flag_otpok for interupt generation
 * -  0 = enable interrupt
 * -  1 = mask interrupt
*/
#define TFA98XX_INTERRUPT_REG_OTDD       (0x1<<1)
#define TFA98XX_INTERRUPT_REG_OTDD_POS   1
#define TFA98XX_INTERRUPT_REG_OTDD_LEN   1
#define TFA98XX_INTERRUPT_REG_OTDD_MAX   1
#define TFA98XX_INTERRUPT_REG_OTDD_MSK   0x2
/** @} */

/**   \addtogroup OVDD
 * @{
 */
/*!
 mask flag_ovpok for interupt generation
 * -  0 = enable interrupt
 * -  1 = mask interrupt
*/
#define TFA9890_INTERRUPT_REG_OVDD       (0x1<<2)
#define TFA9890_INTERRUPT_REG_OVDD_POS   2
#define TFA9890_INTERRUPT_REG_OVDD_LEN   1
#define TFA9890_INTERRUPT_REG_OVDD_MAX   1
#define TFA9890_INTERRUPT_REG_OVDD_MSK   0x4
/** @} */

/**   \addtogroup UVDD
 * @{
 */
/*!
 mask flag_uvpok for interupt generation
 * -  0 = enable interrupt
 * -  1 = mask interrupt
*/
#define TFA9890_INTERRUPT_REG_UVDD       (0x1<<3)
#define TFA9890_INTERRUPT_REG_UVDD_POS   3
#define TFA9890_INTERRUPT_REG_UVDD_LEN   1
#define TFA9890_INTERRUPT_REG_UVDD_MAX   1
#define TFA9890_INTERRUPT_REG_UVDD_MSK   0x8
/** @} */

/**   \addtogroup OCDD
 * @{
 */
/*!
 mask flag_ocp_alarm for interupt generation
 * -  0 = enable interrupt
 * -  1 = mask interrupt
*/
#define TFA9890_INTERRUPT_REG_OCDD       (0x1<<4)
#define TFA9890_INTERRUPT_REG_OCDD_POS   4
#define TFA9890_INTERRUPT_REG_OCDD_LEN   1
#define TFA9890_INTERRUPT_REG_OCDD_MAX   1
#define TFA9890_INTERRUPT_REG_OCDD_MSK   0x10
/** @} */

/**   \addtogroup CLKD
 * @{
 */
/*!
 mask flag_clocks_stable for interupt generation
 * -  0 = enable interrupt
 * -  1 = mask interrupt
*/
#define TFA9890_INTERRUPT_REG_CLKD       (0x1<<5)
#define TFA9890_INTERRUPT_REG_CLKD_POS   5
#define TFA9890_INTERRUPT_REG_CLKD_LEN   1
#define TFA9890_INTERRUPT_REG_CLKD_MAX   1
#define TFA9890_INTERRUPT_REG_CLKD_MSK   0x20
/** @} */

/**   \addtogroup DCCD
 * @{
 */
/*!
 mask flag_pwrokbst for interupt generation
 * -  0 = enable interrupt
 * -  1 = mask interrupt
*/
#define TFA9890_INTERRUPT_REG_DCCD       (0x1<<6)
#define TFA9890_INTERRUPT_REG_DCCD_POS   6
#define TFA9890_INTERRUPT_REG_DCCD_LEN   1
#define TFA9890_INTERRUPT_REG_DCCD_MAX   1
#define TFA9890_INTERRUPT_REG_DCCD_MSK   0x40
/** @} */

/**   \addtogroup SPKD
 * @{
 */
/*!
 mask flag_cf_speakererror for interupt generation
 * -  0 = enable interrupt
 * -  1 = mask interrupt
*/
#define TFA9890_INTERRUPT_REG_SPKD       (0x1<<7)
#define TFA9890_INTERRUPT_REG_SPKD_POS   7
#define TFA9890_INTERRUPT_REG_SPKD_LEN   1
#define TFA9890_INTERRUPT_REG_SPKD_MAX   1
#define TFA9890_INTERRUPT_REG_SPKD_MSK   0x80
/** @} */

/**   \addtogroup WDD
 * @{
 */
/*!
 mask flag_watchdog_reset for interupt generation
 * -  0 = enable interrupt
 * -  1 = mask interrupt
*/
#define TFA9890_INTERRUPT_REG_WDD        (0x1<<8)
#define TFA9890_INTERRUPT_REG_WDD_POS    8
#define TFA9890_INTERRUPT_REG_WDD_LEN    1
#define TFA9890_INTERRUPT_REG_WDD_MAX    1
#define TFA9890_INTERRUPT_REG_WDD_MSK    0x100
/** @} */

/**   \addtogroup LCLK
 * @{
 */
/*!
 mask flag_lost_clk for interupt generation
 * -  0 = enable interrupt
 * -  1 = mask interrupt
*/
#define TFA9890_INTERRUPT_REG_LCLK       (0x1<<9)
#define TFA9890_INTERRUPT_REG_LCLK_POS   9
#define TFA9890_INTERRUPT_REG_LCLK_LEN   1
#define TFA9890_INTERRUPT_REG_LCLK_MAX   1
#define TFA9890_INTERRUPT_REG_LCLK_MSK   0x200
/** @} */

/**   \addtogroup 10
 * @{
 */
/*!
 Reserved
*/
#define TFA9890_INTERRUPT_REG_10         (0xf<<10)
#define TFA9890_INTERRUPT_REG_10_POS     10
#define TFA9890_INTERRUPT_REG_10_LEN     4
#define TFA9890_INTERRUPT_REG_10_MAX     15
#define TFA9890_INTERRUPT_REG_10_MSK     0x3c00
/** @} */

/**   \addtogroup INT
 * @{
 */
/*!
 enabling interrupt
 * -  0 = interrupt disabled
 * -  1 = interrupt enabled
*/
#define TFA9890_INTERRUPT_REG_INT        (0x1<<14)
#define TFA9890_INTERRUPT_REG_INT_POS    14
#define TFA9890_INTERRUPT_REG_INT_LEN    1
#define TFA9890_INTERRUPT_REG_INT_MAX    1
#define TFA9890_INTERRUPT_REG_INT_MSK    0x4000
/** @} */

/**   \addtogroup INTP
 * @{
 */
/*!
 Setting polarity interupt
 * -  0 = interrupt active low
 * -  1 = interrupt active high
*/
#define TFA9890_INTERRUPT_REG_INTP       (0x1<<15)
#define TFA9890_INTERRUPT_REG_INTP_POS   15
#define TFA9890_INTERRUPT_REG_INTP_LEN   1
#define TFA9890_INTERRUPT_REG_INTP_MAX   1
#define TFA9890_INTERRUPT_REG_INTP_MSK   0x8000
/** @} */

/** @} */

/**  pwm_mute_set Register ($41) ********************************************/

/**   \addtogroup _0x41_pwm_mute_set_TFA9887
 * @{
 */
#define TFA9887_PWM_MUTE_SET       0x41
/**   \addtogroup 0
 * @{
 */
/*!
 bypass_hp, to bypass the hp filter byhind the CoolFlux
*/
#define TFA9887_PWM_MUTE_SET_0           (0x1<<0)
#define TFA9887_PWM_MUTE_SET_0_POS       0
#define TFA9887_PWM_MUTE_SET_0_LEN       1
#define TFA9887_PWM_MUTE_SET_0_MAX       1
#define TFA9887_PWM_MUTE_SET_0_MSK       0x1
/** @} */

/**   \addtogroup 1
 * @{
 */
/*!
 hard mute setting in HW
 * -  0=no mute
 * -  1=hard-muted
*/
#define TFA9887_PWM_MUTE_SET_1           (0x1<<1)
#define TFA9887_PWM_MUTE_SET_1_POS       1
#define TFA9887_PWM_MUTE_SET_1_LEN       1
#define TFA9887_PWM_MUTE_SET_1_MAX       1
#define TFA9887_PWM_MUTE_SET_1_MSK       0x2
/** @} */

/**   \addtogroup 2
 * @{
 */
/*!
 Soft mute setting in HW
 * -  0=no mute
 * -  1=soft-muted
*/
#define TFA9887_PWM_MUTE_SET_2           (0x1<<2)
#define TFA9887_PWM_MUTE_SET_2_POS       2
#define TFA9887_PWM_MUTE_SET_2_LEN       1
#define TFA9887_PWM_MUTE_SET_2_MAX       1
#define TFA9887_PWM_MUTE_SET_2_MSK       0x4
/** @} */

/**   \addtogroup PWMDEL
 * @{
 */
/*!
 PWM DelayBits to set the delay
*/
#define TFA9887_PWM_MUTE_SET_PWMDEL      (0x1f<<3)
#define TFA9887_PWM_MUTE_SET_PWMDEL_POS  3
#define TFA9887_PWM_MUTE_SET_PWMDEL_LEN  5
#define TFA9887_PWM_MUTE_SET_PWMDEL_MAX  31
#define TFA9887_PWM_MUTE_SET_PWMDEL_MSK  0xf8
/** @} */

/**   \addtogroup PWMSH
 * @{
 */
/*!
 PWM Shape
 * -  0=Single sided
 * -  1=Double sided
*/
#define TFA9887_PWM_MUTE_SET_PWMSH       (0x1<<8)
#define TFA9887_PWM_MUTE_SET_PWMSH_POS   8
#define TFA9887_PWM_MUTE_SET_PWMSH_LEN   1
#define TFA9887_PWM_MUTE_SET_PWMSH_MAX   1
#define TFA9887_PWM_MUTE_SET_PWMSH_MSK   0x100
/** @} */

/**   \addtogroup PWMRE
 * @{
 */
/*!
 PWM Bitlength in noise shaper
 * -  0=7 bits
 * -  1=8 bits
*/
#define TFA9887_PWM_MUTE_SET_PWMRE       (0x1<<9)
#define TFA9887_PWM_MUTE_SET_PWMRE_POS   9
#define TFA9887_PWM_MUTE_SET_PWMRE_LEN   1
#define TFA9887_PWM_MUTE_SET_PWMRE_MAX   1
#define TFA9887_PWM_MUTE_SET_PWMRE_MSK   0x200
/** @} */

/** @} */

/**  currentsense3 Register ($48) ********************************************/

/**   \addtogroup _0x48_currentsense3_TFA9887
 * @{
 */
#define TFA9887_CURRENTSENSE3      0x48
/**   \addtogroup 0
 * @{
 */
/*!

*/
#define TFA9887_CURRENTSENSE3_0          (0x1<<0)
#define TFA9887_CURRENTSENSE3_0_POS      0
#define TFA9887_CURRENTSENSE3_0_LEN      1
#define TFA9887_CURRENTSENSE3_0_MAX      1
#define TFA9887_CURRENTSENSE3_0_MSK      0x1
/** @} */

/**   \addtogroup 1
 * @{
 */
/*!

*/
#define TFA9887_CURRENTSENSE3_1          (0x1<<1)
#define TFA9887_CURRENTSENSE3_1_POS      1
#define TFA9887_CURRENTSENSE3_1_LEN      1
#define TFA9887_CURRENTSENSE3_1_MAX      1
#define TFA9887_CURRENTSENSE3_1_MSK      0x2
/** @} */

/**   \addtogroup 2
 * @{
 */
/*!
 HIGH => Prevent dcdc switching during clk_cs_clksh
 * -  LOW => Allow switch of dcdc during clk_cs_clksh
*/
#define TFA9887_CURRENTSENSE3_2          (0x1<<2)
#define TFA9887_CURRENTSENSE3_2_POS      2
#define TFA9887_CURRENTSENSE3_2_LEN      1
#define TFA9887_CURRENTSENSE3_2_MAX      1
#define TFA9887_CURRENTSENSE3_2_MSK      0x4
/** @} */

/**   \addtogroup 7
 * @{
 */
/*!
 delayshiftse2
*/
#define TFA9887_CURRENTSENSE3_7          (0x7f<<7)
#define TFA9887_CURRENTSENSE3_7_POS      7
#define TFA9887_CURRENTSENSE3_7_LEN      7
#define TFA9887_CURRENTSENSE3_7_MAX      127
#define TFA9887_CURRENTSENSE3_7_MSK      0x3f80
/** @} */

/**   \addtogroup TCC
 * @{
 */
/*!
 sample & hold track time:
 * -  00 = 2 clock cycles
 * -  01 = 4 clock cycles
 * -  10 = 8 clock cycles
 * -  11 = is no fixed time, but as N1B
*/
#define TFA9887_CURRENTSENSE3_TCC        (0x1f<<1)
#define TFA9887_CURRENTSENSE3_TCC_POS    1
#define TFA9887_CURRENTSENSE3_TCC_LEN    5
#define TFA9887_CURRENTSENSE3_TCC_MAX    31
#define TFA9887_CURRENTSENSE3_TCC_MSK    0x3e
/** @} */

/** @} */

/**  CurrentSense4 Register ($49) ********************************************/

/**   \addtogroup _0x49_CurrentSense4
 * @{
 */
#define TFA98XX_CURRENTSENSE4      0x49
/**   \addtogroup CLIP
 * @{
 */
/*!
 Bypass clip control
 * -  0 = clip control enabled
 * -  1 = clip control bypassed
*/
#define TFA989X_CURRENTSENSE4_CLIP       (0x1<<0)
#define TFA989X_CURRENTSENSE4_CLIP_POS   0
#define TFA989X_CURRENTSENSE4_CLIP_LEN   1
#define TFA989X_CURRENTSENSE4_CLIP_MAX   1
#define TFA989X_CURRENTSENSE4_CLIP_MSK   0x1
/** @} */

/**   \addtogroup 1
 * @{
 */
/*!

*/
#define TFA98XX_CURRENTSENSE4_1          (0x1<<1)
#define TFA98XX_CURRENTSENSE4_1_POS      1
#define TFA98XX_CURRENTSENSE4_1_LEN      1
#define TFA98XX_CURRENTSENSE4_1_MAX      1
#define TFA98XX_CURRENTSENSE4_1_MSK      0x2
/** @} */

/**   \addtogroup 2
 * @{
 */
/*!
 to disable clock gating in the coolflux
*/
#define TFA9897_CURRENTSENSE4_2          (0x1<<2)
#define TFA9897_CURRENTSENSE4_2_POS      2
#define TFA9897_CURRENTSENSE4_2_LEN      1
#define TFA9897_CURRENTSENSE4_2_MAX      1
#define TFA9897_CURRENTSENSE4_2_MSK      0x4
/** @} */

/**   \addtogroup 3
 * @{
 */
/*!

*/
#define TFA98XX_CURRENTSENSE4_3          (0x1<<3)
#define TFA98XX_CURRENTSENSE4_3_POS      3
#define TFA98XX_CURRENTSENSE4_3_LEN      1
#define TFA98XX_CURRENTSENSE4_3_MAX      1
#define TFA98XX_CURRENTSENSE4_3_MSK      0x8
/** @} */

/**   \addtogroup 4
 * @{
 */
/*!
 clock switch for battery protection clipper, it switches back to old frequency
*/
#define TFA9897_CURRENTSENSE4_4          (0x1<<4)
#define TFA9897_CURRENTSENSE4_4_POS      4
#define TFA9897_CURRENTSENSE4_4_LEN      1
#define TFA9897_CURRENTSENSE4_4_MAX      1
#define TFA9897_CURRENTSENSE4_4_MSK      0x10
/** @} */

/**   \addtogroup 5
 * @{
 */
/*!
 8 ohm mode for current sense (gain mode)
 * -  0 = 4 ohm (default)
 * -  1 = 8 ohm
*/
#define TFA9897_CURRENTSENSE4_5          (0x1<<5)
#define TFA9897_CURRENTSENSE4_5_POS      5
#define TFA9897_CURRENTSENSE4_5_LEN      1
#define TFA9897_CURRENTSENSE4_5_MAX      1
#define TFA9897_CURRENTSENSE4_5_MSK      0x20
/** @} */

/**   \addtogroup 6
 * @{
 */
/*!

*/
#define TFA98XX_CURRENTSENSE4_6          (0x1<<6)
#define TFA98XX_CURRENTSENSE4_6_POS      6
#define TFA98XX_CURRENTSENSE4_6_LEN      1
#define TFA98XX_CURRENTSENSE4_6_MAX      1
#define TFA98XX_CURRENTSENSE4_6_MSK      0x40
/** @} */

/**   \addtogroup 7
 * @{
 */
/*!
 delay_sh, tunes S7H delay
*/
#define TFA9897_CURRENTSENSE4_7          (0x1f<<7)
#define TFA9897_CURRENTSENSE4_7_POS      7
#define TFA9897_CURRENTSENSE4_7_LEN      5
#define TFA9897_CURRENTSENSE4_7_MAX      31
#define TFA9897_CURRENTSENSE4_7_MSK      0xf80
/** @} */

/**   \addtogroup 12
 * @{
 */
/*!
 Invert the sample/hold clock for current sense ADC
*/
#define TFA9897_CURRENTSENSE4_12         (0x1<<12)
#define TFA9897_CURRENTSENSE4_12_POS     12
#define TFA9897_CURRENTSENSE4_12_LEN     1
#define TFA9897_CURRENTSENSE4_12_MAX     1
#define TFA9897_CURRENTSENSE4_12_MSK     0x1000
/** @} */

/**   \addtogroup 13
 * @{
 */
/*!
 Invert neg signal
*/
#define TFA9897_CURRENTSENSE4_13         (0x1<<13)
#define TFA9897_CURRENTSENSE4_13_POS     13
#define TFA9897_CURRENTSENSE4_13_LEN     1
#define TFA9897_CURRENTSENSE4_13_MAX     1
#define TFA9897_CURRENTSENSE4_13_MSK     0x2000
/** @} */

/**   \addtogroup 14
 * @{
 */
/*!
 Invert se signal
*/
#define TFA9897_CURRENTSENSE4_14         (0x1<<14)
#define TFA9897_CURRENTSENSE4_14_POS     14
#define TFA9897_CURRENTSENSE4_14_LEN     1
#define TFA9897_CURRENTSENSE4_14_MAX     1
#define TFA9897_CURRENTSENSE4_14_MSK     0x4000
/** @} */

/**   \addtogroup 15
 * @{
 */
/*!
 switches between Single Ende and differentail mode; 1 = single ended
*/
#define TFA9897_CURRENTSENSE4_15         (0x1<<15)
#define TFA9897_CURRENTSENSE4_15_POS     15
#define TFA9897_CURRENTSENSE4_15_LEN     1
#define TFA9897_CURRENTSENSE4_15_MAX     1
#define TFA9897_CURRENTSENSE4_15_MSK     0x8000
/** @} */

/** @} */

/**  mtp_ctrl_reg3 Register ($62) ********************************************/

/**   \addtogroup _0x62_mtp_ctrl_reg3
 * @{
 */
#define TFA98XX_MTP_CTRL_REG3      0x62
/**   \addtogroup 4
 * @{
 */
/*!
 not used
*/
#define TFA98XX_MTP_CTRL_REG3_4          (0x3<<4)
#define TFA98XX_MTP_CTRL_REG3_4_POS      4
#define TFA98XX_MTP_CTRL_REG3_4_LEN      2
#define TFA98XX_MTP_CTRL_REG3_4_MAX      3
#define TFA98XX_MTP_CTRL_REG3_4_MSK      0x30
/** @} */

/**   \addtogroup CIMTP
 * @{
 */
/*!
 start copying all the data from i2cregs_mtp to mtp [Key 2 protected]
*/
#define TFA98XX_MTP_CTRL_REG3_CIMTP      (0x1<<11)
#define TFA98XX_MTP_CTRL_REG3_CIMTP_POS  11
#define TFA98XX_MTP_CTRL_REG3_CIMTP_LEN  1
#define TFA98XX_MTP_CTRL_REG3_CIMTP_MAX  1
#define TFA98XX_MTP_CTRL_REG3_CIMTP_MSK  0x800
/** @} */

/**   \addtogroup 12
 * @{
 */
/*!
 not used
*/
#define TFA98XX_MTP_CTRL_REG3_12         (0x1<<12)
#define TFA98XX_MTP_CTRL_REG3_12_POS     12
#define TFA98XX_MTP_CTRL_REG3_12_LEN     1
#define TFA98XX_MTP_CTRL_REG3_12_MAX     1
#define TFA98XX_MTP_CTRL_REG3_12_MSK     0x1000
/** @} */

/** @} */

/**  cf_controls Register ($70) ********************************************/

/**   \addtogroup _0x70_cf_controls
 * @{
 */
#define TFA98XX_CF_CONTROLS        0x70
/**   \addtogroup RST
 * @{
 */
/*!
 Reset CoolFlux DSP
 * -  0 = Reset not active [default]
 * -  1 = Reset active
*/
#define TFA98XX_CF_CONTROLS_RST          (0x1<<0)
#define TFA98XX_CF_CONTROLS_RST_POS      0
#define TFA98XX_CF_CONTROLS_RST_LEN      1
#define TFA98XX_CF_CONTROLS_RST_MAX      1
#define TFA98XX_CF_CONTROLS_RST_MSK      0x1
/** @} */

/**   \addtogroup DMEM
 * @{
 */
/*!
 Target memory for access
 * -  0 =  pmem [default]
 * -  1 =  xmem
 * -  2 =  ymem
 * -  3 =  iomem
*/
#define TFA98XX_CF_CONTROLS_DMEM         (0x3<<1)
#define TFA98XX_CF_CONTROLS_DMEM_POS     1
#define TFA98XX_CF_CONTROLS_DMEM_LEN     2
#define TFA98XX_CF_CONTROLS_DMEM_MAX     3
#define TFA98XX_CF_CONTROLS_DMEM_MSK     0x6
/** @} */

/**   \addtogroup AIF
 * @{
 */
/*!
 Autoincrement-flag for memory-address
 * -  0 = Autoincrement ON [default]
 * -  1 = Autoincrement OFF
*/
#define TFA98XX_CF_CONTROLS_AIF          (0x1<<3)
#define TFA98XX_CF_CONTROLS_AIF_POS      3
#define TFA98XX_CF_CONTROLS_AIF_LEN      1
#define TFA98XX_CF_CONTROLS_AIF_MAX      1
#define TFA98XX_CF_CONTROLS_AIF_MSK      0x8
/** @} */

/**   \addtogroup CFINT
 * @{
 */
/*!
 Interrupt CoolFlux DSP
*/
#define TFA98XX_CF_CONTROLS_CFINT        (0x1<<4)
#define TFA98XX_CF_CONTROLS_CFINT_POS    4
#define TFA98XX_CF_CONTROLS_CFINT_LEN    1
#define TFA98XX_CF_CONTROLS_CFINT_MAX    1
#define TFA98XX_CF_CONTROLS_CFINT_MSK    0x10
/** @} */

/**   \addtogroup 5
 * @{
 */
/*!
 not used
*/
#define TFA98XX_CF_CONTROLS_5            (0x7<<5)
#define TFA98XX_CF_CONTROLS_5_POS        5
#define TFA98XX_CF_CONTROLS_5_LEN        3
#define TFA98XX_CF_CONTROLS_5_MAX        7
#define TFA98XX_CF_CONTROLS_5_MSK        0xe0
/** @} */

/**   \addtogroup REQ
 * @{
 */
/*!
 request for access (8 channels)
*/
#define TFA98XX_CF_CONTROLS_REQ          (0xff<<8)
#define TFA98XX_CF_CONTROLS_REQ_POS      8
#define TFA98XX_CF_CONTROLS_REQ_LEN      8
#define TFA98XX_CF_CONTROLS_REQ_MAX      255
#define TFA98XX_CF_CONTROLS_REQ_MSK      0xff00
/** @} */

/** @} */

/**  cf_mad Register ($71) ********************************************/

/**   \addtogroup _0x71_cf_mad
 * @{
 */
#define TFA98XX_CF_MAD             0x71
/**   \addtogroup MADD
 * @{
 */
/*!
 memory-address to be accessed
*/
#define TFA98XX_CF_MAD_MADD              (0xffff<<0)
#define TFA98XX_CF_MAD_MADD_POS          0
#define TFA98XX_CF_MAD_MADD_LEN          16
#define TFA98XX_CF_MAD_MADD_MAX          65535
#define TFA98XX_CF_MAD_MADD_MSK          0xffff
/** @} */

/** @} */

/**  cf_mem Register ($72) ********************************************/

/**   \addtogroup _0x72_cf_mem
 * @{
 */
#define TFA98XX_CF_MEM             0x72
/**   \addtogroup MEMA
 * @{
 */
/*!
 activate memory access (24- or 32-bits data is written/read to/from memory
*/
#define TFA98XX_CF_MEM_MEMA              (0xffff<<0)
#define TFA98XX_CF_MEM_MEMA_POS          0
#define TFA98XX_CF_MEM_MEMA_LEN          16
#define TFA98XX_CF_MEM_MEMA_MAX          65535
#define TFA98XX_CF_MEM_MEMA_MSK          0xffff
/** @} */

/** @} */

/**  cf_status Register ($73) ********************************************/

/**   \addtogroup _0x73_cf_status
 * @{
 */
#define TFA98XX_CF_STATUS          0x73
/**   \addtogroup ERR
 * @{
 */
/*!
 Coolflux error flags
*/
#define TFA98XX_CF_STATUS_ERR            (0xff<<0)
#define TFA98XX_CF_STATUS_ERR_POS        0
#define TFA98XX_CF_STATUS_ERR_LEN        8
#define TFA98XX_CF_STATUS_ERR_MAX        255
#define TFA98XX_CF_STATUS_ERR_MSK        0xff
/** @} */

/**   \addtogroup ACK
 * @{
 */
/*!
 acknowledge of requests (8 channels)
*/
#define TFA98XX_CF_STATUS_ACK            (0xff<<8)
#define TFA98XX_CF_STATUS_ACK_POS        8
#define TFA98XX_CF_STATUS_ACK_LEN        8
#define TFA98XX_CF_STATUS_ACK_MAX        255
#define TFA98XX_CF_STATUS_ACK_MSK        0xff00
/** @} */

/** @} */

/**  mtp_spkr_cal Register ($80) ********************************************/

/**   \addtogroup _0x80_mtp_spkr_cal
 * @{
 */
#define TFA98XX_MTP_SPKR_CAL       0x80
#define TFA98XX_KEY2PROTECTED_SPKR_CAL_MTP   0x80
#define TFA9890_KEY2PROTECTED_SPKR_CAL_MTP_POR  0x0000

/**   \addtogroup MTPOTC
 * @{
 */
/*!
 Calibration schedule (key2 protected)
 * -  0 = Calibrate after each POR [default]
 * -  1 = One time calibration
*/
#define TFA98XX_MTP_SPKR_CAL_MTPOTC      (0x1<<0)
#define TFA98XX_MTP_SPKR_CAL_MTPOTC_POS  0
#define TFA98XX_MTP_SPKR_CAL_MTPOTC_LEN  1
#define TFA98XX_MTP_SPKR_CAL_MTPOTC_MAX  1
#define TFA98XX_MTP_SPKR_CAL_MTPOTC_MSK  0x1
/** @} */

/**   \addtogroup MTPEX
 * @{
 */
/*!
 (key2 protected)
 * -  calibration of Ron has been executed.
*/
#define TFA98XX_MTP_SPKR_CAL_MTPEX       (0x1<<1)
#define TFA98XX_MTP_SPKR_CAL_MTPEX_POS   1
#define TFA98XX_MTP_SPKR_CAL_MTPEX_LEN   1
#define TFA98XX_MTP_SPKR_CAL_MTPEX_MAX   1
#define TFA98XX_MTP_SPKR_CAL_MTPEX_MSK   0x2
/** @} */

/**   \addtogroup 2
 * @{
 */
/*!

*/
#define TFA98XX_MTP_SPKR_CAL_2           (0x3fff<<2)
#define TFA98XX_MTP_SPKR_CAL_2_POS       2
#define TFA98XX_MTP_SPKR_CAL_2_LEN       14
#define TFA98XX_MTP_SPKR_CAL_2_MAX       16383
#define TFA98XX_MTP_SPKR_CAL_2_MSK       0xfffc
/** @} */

/** @} */

/**  MTPF Register ($8f) ********************************************/

/**   \addtogroup _0x8f_MTPF_TFA9890
 * @{
 */
#define TFA9890_MTPF               0x8f
/**   \addtogroup VERSION
 * @{
 */
/*!
 (key1 protected)
*/
#define TFA9890_MTPF_VERSION             (0xffff<<0)
#define TFA9890_MTPF_VERSION_POS         0
#define TFA9890_MTPF_VERSION_LEN         16
#define TFA9890_MTPF_VERSION_MAX         65535
#define TFA9890_MTPF_VERSION_MSK         0xffff
/** @} */

/** @} */

#endif /* TFA98XX_GENREGS_H_ */
