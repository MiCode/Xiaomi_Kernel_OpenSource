/* -------------------------------------------------------------------------- */
/* (C) Copyright 2000-2005              Koninklijke Philips Electronics N.V., */
/*     All rights reserved                                                    */
/*                                                                            */
/* This source code and any compilation or derivative thereof is the          */
/* proprietary information of Konlinklijke Philips Electronics N.V. and is    */
/* Confidential in nature.                                                    */
/* Under no circumstances is this software to be exposed to or placed under an*/
/* Open Source License of any type without the expressed written permission of*/
/* Koninklijke Philips Electronics N.V.                                       */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* MoReUse - 2005-10-24   Version 118                                         */
/*                                                                            */
/* Added:                                                                     */
/*    CID_AACPENC                                                             */
/*                                                                            */
/*                                                                            */
/* Changed:                                                                   */
/*                                                                            */
/*                                                                            */
/*                                                                            */
/* Removed:                                                                   */
/*                                                                            */
/*                                                                            */
/*                                                                            */
/* General Error Codes Added                                                  */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/* FILE NAME:    tmNxCompId.h                                                 */
/*                                                                            */
/* DESCRIPTION:  This header file identifies the standard component           */
/*               identifiers (CIDs) and interface identifiers (IID) for       */
/*               Nexperia platforms.                                          */
/*               The objective of these identifiers is to enable unique       */
/*               identification of software components and interfaces.        */
/*               In addition, standard status values are also defined to make */
/*               determination of typical error cases much easier.            */
/*                                                                            */
/*               Functional errors are not real errors in the sense of        */
/*               unexpected behaviour but are part of the normal communication*/
/*               between a client an a server component. They are linked to   */
/*               an interface, rather than to a component. All implementations*/
/*               of an interface must have the same behaviour with respect to */
/*               functional errors. Functional erros are all positive         */
/*               One global functional error is defined:  TM_OK 0x00000000    */
/*                                                                            */
/*               Non-functional errors (all negative numbers) indicate        */
/*               unexpected behaviour. They are linked to concrete component  */
/*               implementations                                              */
/*                                                                            */
/*               NOTE: The current implementation is different from the prev. */
/*                     component identifier implementation, based on classes, */
/*                     types and layers. However, the new system is backward  */
/*                     compatitible with the old implementation.              */
/*                                                                            */
/*               tmNxCompId.h defines a number of general error codes that can*/
/*               be used by all components. These error codes are concatenated*/
/*               to the CID or IID value in the local component headerfile of */
/*               the component that wants to (re-)use this general error code */
/*               General error codes can be used for both functional and      */
/*               non-functional errors. They should only be used if they      */
/*               semantically fully match (if not, defined a new component or */
/*               interface specific error code.                               */
/*                                                                            */
/* General Rules:                                                             */
/*               A return value has a length of 32 bits. At the binary level, */
/*               1 bit indicates the component or interface flag; 16 bits are */
/*               used for the actual component id (CID) or interface id (IID) */
/*               and 12 bits for the return status.                           */
/*                     The component/interface flag is bit 31.                */
/*                     Bits 30--28 are all 0.                                 */
/*                     The component/interface id occupies bits 27--12.       */
/*                     The return status occupies bits 11--0.                 */
/*                                                                            */
/*                     +--------+-----+-------+-----------+                   */
/*                     | flag:1 | 0:3 | id:16 | status:12 |                   */
/*                     +--------+-----+-------+-----------+                   */
/*                                                                            */
/*                     Format of interface ids:                               */
/*                                                                            */
/*                     +-----+-----+--------+-----------+                     */
/*                     | 0:1 | 0:3 | iid:16 | status:12 |                     */
/*                     +-----+-----+--------+-----------+                     */
/*                                                                            */
/*                     Format of component ids:                               */
/*                                                                            */
/*                     +-----+-----+--------+-----------+                     */
/*                     | 1:1 | 0:3 | cid:16 | status:12 |                     */
/*                     +-----+-----+--------+-----------+                     */
/*                                                                            */
/*               At the macro level, we use the prefix "CID_" for component   */
/*               ids (previous version "CID_COMP_") and "IID_" for interface  */
/*               ids.                                                         */
/*                                                                            */
/*               Each component id will be used by only one component; each   */
/*               component will have its own component id.                    */
/*               Each interface id will be used by only one interface; each   */
/*               interface will have its own interface id.                    */
/*                                                                            */
/*               In order to avoid problems when promoting a UNIQUE interface */
/*               to a SEPARATE interface, the ranges for CIDs and IIDS must   */
/*               not overlap.                                                 */
/*                                                                            */
/*               Component names and component ids have to be registered      */
/*               together; the same applies for interface names and ids.      */
/*                                                                            */
/*           NOTE about Compatibility                                         */
/*               In the previous implementation the first four bits were      */
/*               reserved for class, and there were separate fields for       */
/*               type and tag, like this:                                     */
/*                                                                            */
/*                     +---------+--------+-------+---------+-----------+     */
/*                     | class:4 | type:4 | tag:8 | layer:4 | status:12 |     */
/*                     +---------+--------+-------+---------+-----------+     */
/*                                                                            */
/*               The values 0 or 8 are not valid classes, and this fact       */
/*               can be used to distinguish a new-style IID (class == 0),     */
/*               a new-style CID (class == 8), and an old-style CID           */
/*               (otherwise).                                                 */
/*                                                                            */
/*           NOTE about error codes                                           */
/*               The general error codes use the range 0x001 to 0x7FF.        */
/*               The component specific error codes are defined in the        */
/*               local component header file and can use 0x800 to 0xFFF.      */
/*               TM_OK has the value 0x00000000.                              */
/*               The proposed error code ranges (general and specific) are    */
/*               the same for functional and non-functional errors.           */
/*                                                                            */
/*               The previously defined ranges for external customers,        */
/*               assert errors and fatal errors have been dropped.            */
/*               The previously defined range for general errors started      */
/*               at 0x000 instead of 0x001                                    */
/*                                                                            */
/* DOCUMENT REF: Nexperia/MoReUse Naming Conventions                          */
/*                                                                            */
/* -------------------------------------------------------------------------- */

#ifndef TMNXCOMPID_H
#define TMNXCOMPID_H

/* -------------------------------------------------------------------------- */
/*                                                                            */
/*   Standard include files:                                                  */
/*                                                                            */
/* -------------------------------------------------------------------------- */
#include "tmNxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*                                                                            */
/*    Types and defines:                                                      */
/*                                                                            */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                                                            */
/*   TM_OK is the 32 bit global status value used by all Nexperia components  */
/*   to indicate successful function/operation status.  If a non-zero value is*/
/*   returned as status, it should use the component ID formats defined.      */
/*                                                                            */
/* -------------------------------------------------------------------------- */
#define TM_OK                     0U	/* Global success return status   */

/* -------------------------------------------------------------------------- */
/*                                                                            */
/*     General Defines                                                        */
/*                                                                            */
/* -------------------------------------------------------------------------- */
#define CID_IID_FLAG_BITSHIFT    31
#define CID_ID_BITSHIFT          12
#define IID_ID_BITSHIFT          12

#define CID_FLAG                 (0x1U << CID_IID_FLAG_BITSHIFT)
#define IID_FLAG                 (0x0U << CID_IID_FLAG_BITSHIFT)

#define CID_ID(number)           ((number) << CID_ID_BITSHIFT)
#define CID_ID_BITMASK           (0x7FFFFU  << CID_ID_BITSHIFT)

#define IID_ID(number)           ((number) << IID_ID_BITSHIFT)
#define IID_ID_BITMASK           (0x7FFFFU  << IID_ID_BITSHIFT)

/* -------------------------------------------------------------------------- */
/*                                                                            */
/*     Definition of the interface IDs                                        */
/*                                                                            */
/* -------------------------------------------------------------------------- */
#define IID_IENUMUNKNOWN            (IID_ID(0x001U) | IID_FLAG)
#define IID_IBIND                   (IID_ID(0x002U) | IID_FLAG)
#define IID_IBINDINFO               (IID_ID(0x003U) | IID_FLAG)
#define IID_IMEM                    (IID_ID(0x004U) | IID_FLAG)
#define IID_IUNKNOWN                (IID_ID(0x005U) | IID_FLAG)
#define IID_IIC                     (IID_ID(0x006U) | IID_FLAG)
#define IID_ACHAN                   (IID_ID(0x007U) | IID_FLAG)
#define IID_AFEAT                   (IID_ID(0x008U) | IID_FLAG)
#define IID_AMIX                    (IID_ID(0x009U) | IID_FLAG)
#define IID_ANAADEC                 (IID_ID(0x00aU) | IID_FLAG)
#define IID_ANAVENC                 (IID_ID(0x00bU) | IID_FLAG)
#define IID_ANAVENCRYPT             (IID_ID(0x00cU) | IID_FLAG)
#define IID_ANAVDEC                 (IID_ID(0x00dU) | IID_FLAG)
#define IID_BBARDETEXT              (IID_ID(0x00eU) | IID_FLAG)
#define IID_BLEVELDETEXT            (IID_ID(0x00fU) | IID_FLAG)
#define IID_BLEVELMODEXT            (IID_ID(0x010U) | IID_FLAG)
#define IID_BSLSPDI                 (IID_ID(0x011U) | IID_FLAG)
#define IID_BSLSPDO                 (IID_ID(0x012U) | IID_FLAG)
#define IID_BSL_AI                  (IID_ID(0x013U) | IID_FLAG)
#define IID_BSL_AO                  (IID_ID(0x014U) | IID_FLAG)
#define IID_BSL_AVI                 (IID_ID(0X015U) | IID_FLAG)
#define IID_BSL_AVO                 (IID_ID(0x016U) | IID_FLAG)
#define IID_BSL_EEPROM              (IID_ID(0X017U) | IID_FLAG)
#define IID_BSL_IDE                 (IID_ID(0X018U) | IID_FLAG)
#define IID_BSL_NANDFLASH           (IID_ID(0X019U) | IID_FLAG)
#define IID_BSL_NORFLASH            (IID_ID(0X01aU) | IID_FLAG)
#define IID_BSL_PARPORT             (IID_ID(0X01bU) | IID_FLAG)
#define IID_BSL_RTC                 (IID_ID(0X01cU) | IID_FLAG)
#define IID_COLENH                  (IID_ID(0x01dU) | IID_FLAG)
#define IID_COLENHEXT               (IID_ID(0x01eU) | IID_FLAG)
#define IID_CONNMGR                 (IID_ID(0x01fU) | IID_FLAG)
#define IID_CRT                     (IID_ID(0x020U) | IID_FLAG)
#define IID_CTI                     (IID_ID(0X021U) | IID_FLAG)
#define IID_CTIEXT                  (IID_ID(0X022U) | IID_FLAG)
#define IID_DIGADEC                 (IID_ID(0X023U) | IID_FLAG)
#define IID_DIGVDEC                 (IID_ID(0X024U) | IID_FLAG)
#define IID_DMX                     (IID_ID(0X025U) | IID_FLAG)
#define IID_DNR                     (IID_ID(0X026U) | IID_FLAG)
#define IID_DNREXT                  (IID_ID(0X027U) | IID_FLAG)
#define IID_DVBSUBTDEC              (IID_ID(0X028U) | IID_FLAG)
#define IID_FATERR                  (IID_ID(0X029U) | IID_FLAG)
#define IID_FREND                   (IID_ID(0X02aU) | IID_FLAG)
#define IID_GAMMAEXT                (IID_ID(0X02bU) | IID_FLAG)
#define IID_HISTOMEASEXT            (IID_ID(0X02cU) | IID_FLAG)
#define IID_HISTOMODEXT             (IID_ID(0X02dU) | IID_FLAG)
#define IID_MML                     (IID_ID(0X02eU) | IID_FLAG)
#define IID_NOISEESTEXT             (IID_ID(0X02fU) | IID_FLAG)
#define IID_OSAL                    (IID_ID(0X030U) | IID_FLAG)
#define IID_PIPSTORE                (IID_ID(0X031U) | IID_FLAG)
#define IID_SCANRATECONV            (IID_ID(0X032U) | IID_FLAG)
#define IID_SCANRATECONVEXT         (IID_ID(0X033U) | IID_FLAG)
#define IID_SHARPENH                (IID_ID(0X034U) | IID_FLAG)
#define IID_SHARPENHEXT             (IID_ID(0X035U) | IID_FLAG)
#define IID_SHARPMEASEXT            (IID_ID(0X036U) | IID_FLAG)
#define IID_SPDIFIN                 (IID_ID(0X037U) | IID_FLAG)
#define IID_SPDIFOUT                (IID_ID(0X038U) | IID_FLAG)
#define IID_SPEAKER                 (IID_ID(0X039U) | IID_FLAG)
#define IID_STCDEC                  (IID_ID(0X03aU) | IID_FLAG)
#define IID_STREAMINJ               (IID_ID(0X03bU) | IID_FLAG)
#define IID_SYNCTAG                 (IID_ID(0X03cU) | IID_FLAG)
#define IID_TSSACOM                 (IID_ID(0X03dU) | IID_FLAG)
#define IID_TXTDEC                  (IID_ID(0X03eU) | IID_FLAG)
#define IID_UTILCRYPT               (IID_ID(0X03fU) | IID_FLAG)
#define IID_UVBWDETEXT              (IID_ID(0X040U) | IID_FLAG)
#define IID_VBIINSERT               (IID_ID(0X041U) | IID_FLAG)
#define IID_VBISLICE                (IID_ID(0X042U) | IID_FLAG)
#define IID_VDCC                    (IID_ID(0X043U) | IID_FLAG)
#define IID_VDSTSCAN                (IID_ID(0X044U) | IID_FLAG)
#define IID_VFEAT                   (IID_ID(0X045U) | IID_FLAG)
#define IID_VMIX                    (IID_ID(0X046U) | IID_FLAG)
#define IID_VSCALEEXT               (IID_ID(0X047U) | IID_FLAG)
#define IID_VSRCPROP                (IID_ID(0X048U) | IID_FLAG)
#define IID_VSRCSCANPROP            (IID_ID(0X049U) | IID_FLAG)
#define IID_GENI2C                  (IID_ID(0X04aU) | IID_FLAG)
#define IID_PLFINSTVIN              (IID_ID(0X04bU) | IID_FLAG)
#define IID_PLFINSTAIN              (IID_ID(0X04cU) | IID_FLAG)
#define IID_PLFINSTAOUT             (IID_ID(0X04dU) | IID_FLAG)
#define IID_PLFINSTGFX              (IID_ID(0X04eU) | IID_FLAG)
#define IID_CONNMGRATV              (IID_ID(0X04fU) | IID_FLAG)
#define IID_IAMALIVE                (IID_ID(0X050U) | IID_FLAG)
#define IID_BBARDET                 (IID_ID(0X051U) | IID_FLAG)
#define IID_CONTRRESEXT             (IID_ID(0X052U) | IID_FLAG)
#define IID_NOISEMEAS               (IID_ID(0X053U) | IID_FLAG)
#define IID_SHARPMEAS               (IID_ID(0X054U) | IID_FLAG)
#define IID_HISTOMOD                (IID_ID(0X055U) | IID_FLAG)
#define IID_ANTIAGING               (IID_ID(0X056U) | IID_FLAG)
#define IID_AMBIENTLEVEL            (IID_ID(0X057U) | IID_FLAG)
#define IID_HAD_DRV_IIC             (IID_ID(0X058U) | IID_FLAG)
#define IID_HAD_DRV_GPIO            (IID_ID(0X059U) | IID_FLAG)
#define IID_HAD_DRV_CSM             (IID_ID(0X05aU) | IID_FLAG)
#define IID_DRIVERHAL               (IID_ID(0X05bU) | IID_FLAG)
#define IID_MUTISTR                 (IID_ID(0X05cU) | IID_FLAG)
#define IID_MUTIVEC                 (IID_ID(0X05dU) | IID_FLAG)
#define IID_MUTISTRX                (IID_ID(0X05eU) | IID_FLAG)
#define IID_MUTICMD                 (IID_ID(0X05fU) | IID_FLAG)
#define IID_TASK_CONDITION          (IID_ID(0X060U) | IID_FLAG)
#define IID_PACKET_POOL             (IID_ID(0X061U) | IID_FLAG)
#define IID_PACKET_QUEUE            (IID_ID(0X062U) | IID_FLAG)
#define IID_UDSDCD                  (IID_ID(0X063U) | IID_FLAG)
#define IID_DCSS_RL                 (IID_ID(0X064U) | IID_FLAG)
#define IID_DCSS_DD                 (IID_ID(0X065U) | IID_FLAG)
#define IID_DCSS_GD                 (IID_ID(0X066U) | IID_FLAG)
#define IID_DCSS_RSC                (IID_ID(0X067U) | IID_FLAG)
#define IID_DCSS_P                  (IID_ID(0X068U) | IID_FLAG)
#define IID_DCSS                    (IID_ID(0X069U) | IID_FLAG)
#define IID_CC_BURST_CUTTING_AREA   (IID_ID(0X06aU) | IID_FLAG)
#define IID_CC_CONFIGURATION        (IID_ID(0X06bU) | IID_FLAG)
#define IID_CC_CONTROL              (IID_ID(0X06cU) | IID_FLAG)
#define IID_CC_DEBUG                (IID_ID(0X06dU) | IID_FLAG)
#define IID_CC_DECODER              (IID_ID(0X06eU) | IID_FLAG)
#define IID_CC_ENCODER              (IID_ID(0X06fU) | IID_FLAG)
#define IID_CC_HF_PROCESSING        (IID_ID(0X070U) | IID_FLAG)
#define IID_CC_INTERFACE            (IID_ID(0X071U) | IID_FLAG)
#define IID_CC_NATLAB               (IID_ID(0X072U) | IID_FLAG)
#define IID_CC_PIC                  (IID_ID(0X073U) | IID_FLAG)
#define IID_CC_WOBBLE               (IID_ID(0X074U) | IID_FLAG)
#define IID_CC_REGISTERMAP          (IID_ID(0X075U) | IID_FLAG)
#define IID_CC_WOBBLE_REG           (IID_ID(0X076U) | IID_FLAG)
#define IID_CC_PIC_REG              (IID_ID(0X077U) | IID_FLAG)
#define IID_CC_NATLAB_REG           (IID_ID(0X078U) | IID_FLAG)
#define IID_CC_INTERFACE_REG        (IID_ID(0X079U) | IID_FLAG)
#define IID_CC_HF_PROCESSING_REG    (IID_ID(0X07aU) | IID_FLAG)
#define IID_CC_ENCODER_REG          (IID_ID(0X07bU) | IID_FLAG)
#define IID_CC_DECODER_REG          (IID_ID(0X07cU) | IID_FLAG)
#define IID_CC_DEBUG_REG            (IID_ID(0X07dU) | IID_FLAG)
#define IID_CC_CONTROL_REG          (IID_ID(0X07eU) | IID_FLAG)
#define IID_CC_CONFIGURATION_REG    (IID_ID(0X07fU) | IID_FLAG)
#define IID_CC_BURST_CUTTING_AREA_REG (IID_ID(0X080U) | IID_FLAG)
#define IID_CC_PHYSICAL_VALUES      (IID_ID(0X081U) | IID_FLAG)
#define IID_CC_GENERAL_SETTINGS     (IID_ID(0X082U) | IID_FLAG)
#define IID_CC_COEFFICIENTS         (IID_ID(0X083U) | IID_FLAG)
#define IID_REMOTE_CONTROL          (IID_ID(0X084U) | IID_FLAG)
#define IID_TUNER                   (IID_ID(0X085U) | IID_FLAG)
#define IID_MUTITST                 (IID_ID(0X086U) | IID_FLAG)
#define IID_CHIP_CONTEXT            (IID_ID(0X087U) | IID_FLAG)
#define IID_API                     (IID_ID(0X088U) | IID_FLAG)
#define IID_CHANDEC                 (IID_ID(0X089U) | IID_FLAG)
#define IID_TUNING                  (IID_ID(0X08aU) | IID_FLAG)
#define IID_TUNINGAFC               (IID_ID(0X08bU) | IID_FLAG)
#define IID_TUNINGAFCNTF            (IID_ID(0X08cU) | IID_FLAG)
#define IID_TUNINGCHAN              (IID_ID(0X08dU) | IID_FLAG)
#define IID_TUNINGSEARCHNTF         (IID_ID(0X08eU) | IID_FLAG)
#define IID_ID3EXTR                 (IID_ID(0X08fU) | IID_FLAG)
#define IID_ANAAVDEM                (IID_ID(0X090U) | IID_FLAG)
#define IID_ANAAVDEMNTF             (IID_ID(0X091U) | IID_FLAG)
#define IID_CCEXTR                  (IID_ID(0X092U) | IID_FLAG)
#define IID_CHANDECDVBC             (IID_ID(0X093U) | IID_FLAG)
#define IID_CHANDECDVBS             (IID_ID(0X094U) | IID_FLAG)
#define IID_CHANDECDVBT             (IID_ID(0X095U) | IID_FLAG)
#define IID_CHANDECNTF              (IID_ID(0X096U) | IID_FLAG)
#define IID_OOB                     (IID_ID(0X097U) | IID_FLAG)
#define IID_RFAMP                   (IID_ID(0X098U) | IID_FLAG)
#define IID_SIGSTRENGTH             (IID_ID(0X099U) | IID_FLAG)
#define IID_SIGSTRENGTHNTF          (IID_ID(0X09aU) | IID_FLAG)
#define IID_IMAGEDEC                (IID_ID(0X09bU) | IID_FLAG)
#define IID_TUNINGSEARCH            (IID_ID(0X09cU) | IID_FLAG)
#define IID_PINOBJECTS              (IID_ID(0X09dU) | IID_FLAG)
#define IID_URLSRC                  (IID_ID(0X09eU) | IID_FLAG)
#define IID_OSDKERNELAPP            (IID_ID(0X09fU) | IID_FLAG)
#define IID_OSDKERNELMEM            (IID_ID(0X0a0U) | IID_FLAG)
#define IID_OSDKERNELOSD            (IID_ID(0X0a1U) | IID_FLAG)
#define IID_OSDKERNELOSDCONTROL     (IID_ID(0X0a2U) | IID_FLAG)
#define IID_RTC                     (IID_ID(0X0a3U) | IID_FLAG)
#define IID_FS                      (IID_ID(0X0a4U) | IID_FLAG)
#define IID_BE                      (IID_ID(0X0a5U) | IID_FLAG)
#define IID_CD_LIB                  (IID_ID(0X0a6U) | IID_FLAG)
#define IID_DB                      (IID_ID(0X0a7U) | IID_FLAG)
#define IID_AVIN                    (IID_ID(0X0a8U) | IID_FLAG)
#define IID_AVOUT                   (IID_ID(0X0a9U) | IID_FLAG)
#define IID_INT                     (IID_ID(0X0aaU) | IID_FLAG)
#define IID_EVT                     (IID_ID(0X0abU) | IID_FLAG)
#define IID_DMA                     (IID_ID(0X0acU) | IID_FLAG)
#define IID_CLK                     (IID_ID(0X0adU) | IID_FLAG)
#define IID_VMIXBORDERPAINTER       (IID_ID(0X0aeU) | IID_FLAG)
#define IID_CPROCTVFLOW             (IID_ID(0X0afU) | IID_FLAG)
#define IID_VTRANTIAGING            (IID_ID(0X0b0U) | IID_FLAG)
#define IID_VTRFADE                 (IID_ID(0X0b1U) | IID_FLAG)
#define IID_VTRSCALE                (IID_ID(0X0b2U) | IID_FLAG)
#define IID_VTRSTROBE               (IID_ID(0X0b3U) | IID_FLAG)
#define IID_HDMIIN                  (IID_ID(0X0b4U) | IID_FLAG)
#define IID_ACHANSEL                (IID_ID(0X0b5U) | IID_FLAG)
#define IID_SSP                     (IID_ID(0X0b6U) | IID_FLAG)
#define IID_CONNMGR_STILL           (IID_ID(0X0b7U) | IID_FLAG)
#define IID_CONNMGR_AUDIO           (IID_ID(0X0b8U) | IID_FLAG)
#define IID_CONNMGR_MPEG2PS         (IID_ID(0X0b9U) | IID_FLAG)
#define IID_SPI_SD                  (IID_ID(0X0baU) | IID_FLAG)
#define IID_DECODERHALCST           (IID_ID(0X0bbU) | IID_FLAG)
#define IID_SOD                     (IID_ID(0X0bcU) | IID_FLAG)
#define IID_DCSS_AA                 (IID_ID(0X0bdU) | IID_FLAG)
#define IID_DCSS_AVI                (IID_ID(0X0beU) | IID_FLAG)
#define IID_DCSS_BC                 (IID_ID(0X0bfU) | IID_FLAG)
#define IID_DCSS_CLUT               (IID_ID(0X0c0U) | IID_FLAG)
#define IID_DCSS_COL                (IID_ID(0X0c1U) | IID_FLAG)
#define IID_DCSS_DFC                (IID_ID(0X0c2U) | IID_FLAG)
#define IID_DCSS_DOC                (IID_ID(0X0c3U) | IID_FLAG)
#define IID_DCSS_GIO                (IID_ID(0X0c4U) | IID_FLAG)
#define IID_DCSS_ISD                (IID_ID(0X0c5U) | IID_FLAG)
#define IID_DCSS_KBI                (IID_ID(0X0c6U) | IID_FLAG)
#define IID_DCSS_OSD                (IID_ID(0X0c7U) | IID_FLAG)
#define IID_DCSS_PIF                (IID_ID(0X0c8U) | IID_FLAG)
#define IID_DCSS_PVI                (IID_ID(0X0c9U) | IID_FLAG)
#define IID_DCSS_SIS                (IID_ID(0X0caU) | IID_FLAG)
#define IID_DCSS_TIG                (IID_ID(0X0cbU) | IID_FLAG)
#define IID_DCSS_USC                (IID_ID(0X0ccU) | IID_FLAG)
#define IID_DCSS_VCR                (IID_ID(0X0cdU) | IID_FLAG)
#define IID_CONNMGR_MP4RTP_PLAYER   (IID_ID(0X0ceU) | IID_FLAG)
#define IID_CONNMGR_AVIMP4_PLAYER   (IID_ID(0X0cfU) | IID_FLAG)
#define IID_VDECANAEXT2             (IID_ID(0X0d0U) | IID_FLAG)
#define IID_STBCOMMON               (IID_ID(0X0d1U) | IID_FLAG)
#define IID_AVSYNCCTRL              (IID_ID(0X0d2U) | IID_FLAG)
#define IID_PRIVNETSCHEMECONFIG     (IID_ID(0X0d3U) | IID_FLAG)
#define IID_SHAREDVARIABLE          (IID_ID(0X0d4U) | IID_FLAG)
#define IID_NETSCHEMECONFIG         (IID_ID(0X0d5U) | IID_FLAG)
#define IID_AVSYNCTRICK             (IID_ID(0X0d6U) | IID_FLAG)
#define IID_SETINTF                 (IID_ID(0X0d7U) | IID_FLAG)
#define IID_URLDMXMONITOR           (IID_ID(0X0d8U) | IID_FLAG)
#define IID_VDECMONITOR             (IID_ID(0X0d9U) | IID_FLAG)
#define IID_STBVIDEOTYPES           (IID_ID(0X0daU) | IID_FLAG)

#define IID_RESERVED                (CID_ID(0x7fffU) | CID_FLAG)
/* ************************************************************************** */
/* Interface Id's reserved for external organizations                         */
/*                                                                            */
/*  None                                                                      */
/*                                                                            */
/* ************************************************************************** */

/* -------------------------------------------------------------------------- */
/*                                                                            */
/*     Definition of the component IDs                                        */
/*                                                                            */
/* -------------------------------------------------------------------------- */
#define CID_MPMP1_GRINDER           (CID_ID(0x8001U) | CID_FLAG)
#define CID_MUSB_GRINDER            (CID_ID(0x8002U) | CID_FLAG)
#define CID_UOTGPFL                 (CID_ID(0x8003U) | CID_FLAG)
#define CID_CHIPBUILDER_GRINDER     (CID_ID(0x8004U) | CID_FLAG)

#define CID_AANALYZER               (CID_ID(0x8009U) | CID_FLAG)
#define CID_ADEC_AAC4               (CID_ID(0x800aU) | CID_FLAG)
#define CID_ADEC_ATV                (CID_ID(0x800bU) | CID_FLAG)
#define CID_ADEC_CELP4              (CID_ID(0x800cU) | CID_FLAG)
#define CID_ADEC_CORE               (CID_ID(0x800dU) | CID_FLAG)
#define CID_ADEC_MP3PRO             (CID_ID(0x800eU) | CID_FLAG)
#define CID_ADEC_PL2                (CID_ID(0x800fU) | CID_FLAG)
#define CID_ADEC_STB                (CID_ID(0x8010U) | CID_FLAG)
#define CID_ADEEMPH                 (CID_ID(0x8011U) | CID_FLAG)
#define CID_AENCAAC4                (CID_ID(0x8012U) | CID_FLAG)
#define CID_AREND_AO_MUX            (CID_ID(0x8013U) | CID_FLAG)
#define CID_ASP_IIRZ2               (CID_ID(0x8014U) | CID_FLAG)
#define CID_ASRC                    (CID_ID(0x8015U) | CID_FLAG)
#define CID_ASYS_CORE               (CID_ID(0x8016U) | CID_FLAG)
#define CID_ATV_PLF_BASIC           (CID_ID(0x8017U) | CID_FLAG)
#define CID_ATV_STUBS               (CID_ID(0x8018U) | CID_FLAG)
#define CID_AVI_READ_DIVX           (CID_ID(0x8019U) | CID_FLAG)
#define CID_BOOTINFO                (CID_ID(0x801aU) | CID_FLAG)
#define CID_BROWSE_EIS              (CID_ID(0x801bU) | CID_FLAG)
#define CID_BSL_7113                (CID_ID(0x801cU) | CID_FLAG)
#define CID_BSL_7113QT              (CID_ID(0x801dU) | CID_FLAG)
#define CID_BSL_7114                (CID_ID(0x801eU) | CID_FLAG)
#define CID_BSL_7118                (CID_ID(0x801fU) | CID_FLAG)
#define CID_BSL_ANABEL              (CID_ID(0x8020U) | CID_FLAG)
#define CID_BSL_ANABELQT            (CID_ID(0x8021U) | CID_FLAG)
#define CID_BSL_AVIP                (CID_ID(0x8022U) | CID_FLAG)
#define CID_BSL_BOARDS              (CID_ID(0x8023U) | CID_FLAG)
#define CID_BSL_CORE                (CID_ID(0x8024U) | CID_FLAG)
#define CID_BSL_DENC                (CID_ID(0x8025U) | CID_FLAG)
#define CID_BSL_EEPROM_ATMEL        (CID_ID(0x8026U) | CID_FLAG)
#define CID_BSL_IDEXIO              (CID_ID(0x8027U) | CID_FLAG)
#define CID_BSL_NANDSAMSUNG         (CID_ID(0x8028U) | CID_FLAG)
#define CID_BSL_NORINTEL            (CID_ID(0x8029U) | CID_FLAG)
#define CID_BSL_RTCPCF8563          (CID_ID(0x802aU) | CID_FLAG)
#define CID_BSL_UART_HWAPI          (CID_ID(0x802bU) | CID_FLAG)
#define CID_BSL_UDA1344             (CID_ID(0x802cU) | CID_FLAG)
#define CID_BT_1500                 (CID_ID(0x802dU) | CID_FLAG)
#define CID_BT_API                  (CID_ID(0x802eU) | CID_FLAG)
#define CID_BT_CORE                 (CID_ID(0x802fU) | CID_FLAG)
#define CID_BT_CPU                  (CID_ID(0x8030U) | CID_FLAG)
#define CID_BT_MIPS                 (CID_ID(0x8031U) | CID_FLAG)
#define CID_BT_TRIMEDIA             (CID_ID(0x8032U) | CID_FLAG)
#define CID_BT_V2PCI                (CID_ID(0x8033U) | CID_FLAG)
#define CID_BT_VPCI                 (CID_ID(0x8034U) | CID_FLAG)
#define CID_BT_VSTB                 (CID_ID(0x8035U) | CID_FLAG)
#define CID_BUFFEREDREAD            (CID_ID(0x8036U) | CID_FLAG)
#define CID_CONN_MGRAUDSYSSTB       (CID_ID(0x8037U) | CID_FLAG)
#define CID_DEMUXMPEGTS_SW          (CID_ID(0x8038U) | CID_FLAG)
#define CID_DIG_ADEC_AUDSYS_STB     (CID_ID(0x8039U) | CID_FLAG)
#define CID_DL_AI                   (CID_ID(0x803aU) | CID_FLAG)
#define CID_DL_AICP                 (CID_ID(0x803bU) | CID_FLAG)
#define CID_DL_AO                   (CID_ID(0x803cU) | CID_FLAG)
#define CID_DL_AVFS                 (CID_ID(0x803dU) | CID_FLAG)
#define CID_DL_CLOCK                (CID_ID(0x803eU) | CID_FLAG)
#define CID_DL_DFS                  (CID_ID(0x803fU) | CID_FLAG)
#define CID_DL_DISKSCHED            (CID_ID(0x8040U) | CID_FLAG)
#define CID_DL_DMA                  (CID_ID(0x8041U) | CID_FLAG)
#define CID_DL_ETH_IP3902           (CID_ID(0x8042U) | CID_FLAG)
#define CID_DL_GPIO                 (CID_ID(0x8043U) | CID_FLAG)
#define CID_DL_I2C                  (CID_ID(0x8044U) | CID_FLAG)
#define CID_DL_IDE                  (CID_ID(0x8045U) | CID_FLAG)
#define CID_DL_IDESTUB              (CID_ID(0x8046U) | CID_FLAG)
#define CID_DL_IIC                  (CID_ID(0x8047U) | CID_FLAG)
#define CID_DL_IR                   (CID_ID(0x8048U) | CID_FLAG)
#define CID_DL_MBS                  (CID_ID(0x8049U) | CID_FLAG)
#define CID_DL_MBS2                 (CID_ID(0x804aU) | CID_FLAG)
#define CID_DL_NANDFLASH            (CID_ID(0x804bU) | CID_FLAG)
#define CID_DL_NORFLASH             (CID_ID(0x804cU) | CID_FLAG)
#define CID_DL_PCI                  (CID_ID(0x804dU) | CID_FLAG)
#define CID_DL_PROCESSOR            (CID_ID(0x804eU) | CID_FLAG)
#define CID_DL_QTNR                 (CID_ID(0x804fU) | CID_FLAG)
#define CID_DL_QVCP                 (CID_ID(0x8050U) | CID_FLAG)
#define CID_DL_SEM                  (CID_ID(0x8051U) | CID_FLAG)
#define CID_DL_SPDI                 (CID_ID(0x8052U) | CID_FLAG)
#define CID_DL_SPDO                 (CID_ID(0x8053U) | CID_FLAG)
#define CID_DL_TIMER                (CID_ID(0x8054U) | CID_FLAG)
#define CID_DL_TSDMA                (CID_ID(0x8055U) | CID_FLAG)
#define CID_DL_TSIO                 (CID_ID(0x8056U) | CID_FLAG)
#define CID_DL_UDMA                 (CID_ID(0x8057U) | CID_FLAG)
#define CID_DL_VID_MEAS             (CID_ID(0x8058U) | CID_FLAG)
#define CID_DL_VIP                  (CID_ID(0x8059U) | CID_FLAG)
#define CID_DL_VMPG                 (CID_ID(0x805aU) | CID_FLAG)
#define CID_DL_XIO                  (CID_ID(0x805bU) | CID_FLAG)
#define CID_DRAWTEXT                (CID_ID(0x805cU) | CID_FLAG)
#define CID_DVPDEBUG                (CID_ID(0x805dU) | CID_FLAG)
#define CID_FATALERROR              (CID_ID(0x805eU) | CID_FLAG)
#define CID_FATALERROR_VT           (CID_ID(0x805fU) | CID_FLAG)
#define CID_FREADAVPROP             (CID_ID(0x8060U) | CID_FLAG)
#define CID_FWRITEAVPROP            (CID_ID(0x8061U) | CID_FLAG)
#define CID_HELP                    (CID_ID(0x8062U) | CID_FLAG)
#define CID_HTTP_IO_DRIVER          (CID_ID(0x8063U) | CID_FLAG)
#define CID_HW_AICP                 (CID_ID(0x8064U) | CID_FLAG)
#define CID_HW_CLOCK                (CID_ID(0x8065U) | CID_FLAG)
#define CID_HW_DMA                  (CID_ID(0x8066U) | CID_FLAG)
#define CID_HW_DRAW                 (CID_ID(0x8067U) | CID_FLAG)
#define CID_HW_DRAWCOMMON           (CID_ID(0x8068U) | CID_FLAG)
#define CID_HW_DRAWDE               (CID_ID(0x8069U) | CID_FLAG)
#define CID_HW_DRAWREF              (CID_ID(0x806aU) | CID_FLAG)
#define CID_HW_DRAWSHARED           (CID_ID(0x806bU) | CID_FLAG)
#define CID_HW_DRAWTMH              (CID_ID(0x806cU) | CID_FLAG)
#define CID_HW_DRAWTMT              (CID_ID(0x806dU) | CID_FLAG)
#define CID_HW_DRAWTMTH             (CID_ID(0x806eU) | CID_FLAG)
#define CID_HW_DSP                  (CID_ID(0x806fU) | CID_FLAG)
#define CID_HW_ETH_IP3902           (CID_ID(0x8070U) | CID_FLAG)
#define CID_HW_GIC                  (CID_ID(0x8071U) | CID_FLAG)
#define CID_HW_GPIO                 (CID_ID(0x8072U) | CID_FLAG)
#define CID_HW_I2C                  (CID_ID(0x8073U) | CID_FLAG)
#define CID_HW_IIC                  (CID_ID(0x8074U) | CID_FLAG)
#define CID_HW_MBS                  (CID_ID(0x8075U) | CID_FLAG)
#define CID_HW_MMIARB               (CID_ID(0x8076U) | CID_FLAG)
#define CID_HW_MMIARB1010           (CID_ID(0x8077U) | CID_FLAG)
#define CID_HW_PCI                  (CID_ID(0x8078U) | CID_FLAG)
#define CID_HW_PIC                  (CID_ID(0x8079U) | CID_FLAG)
#define CID_HW_SMC                  (CID_ID(0x807aU) | CID_FLAG)
#define CID_HW_TSDMA                (CID_ID(0x807bU) | CID_FLAG)
#define CID_HW_UART                 (CID_ID(0x807cU) | CID_FLAG)
#define CID_HW_UDMA                 (CID_ID(0x807dU) | CID_FLAG)
#define CID_HW_VIP                  (CID_ID(0x807eU) | CID_FLAG)
#define CID_HW_VMSP                 (CID_ID(0x807fU) | CID_FLAG)
#define CID_HW_XIO                  (CID_ID(0x8080U) | CID_FLAG)
#define CID_INFRA_MISC              (CID_ID(0x8081U) | CID_FLAG)
#define CID_INTERRUPT               (CID_ID(0x8082U) | CID_FLAG)
#define CID_IPC_DT                  (CID_ID(0x8083U) | CID_FLAG)
#define CID_IPC_READ                (CID_ID(0x8084U) | CID_FLAG)
#define CID_IPC_RPC                 (CID_ID(0x8085U) | CID_FLAG)
#define CID_IPC_WRITE               (CID_ID(0x8086U) | CID_FLAG)
#define CID_LIBLOAD_TM              (CID_ID(0x8087U) | CID_FLAG)
#define CID_MEMDBG                  (CID_ID(0x8088U) | CID_FLAG)
#define CID_MENU                    (CID_ID(0x8089U) | CID_FLAG)
#define CID_MP4READ                 (CID_ID(0x808aU) | CID_FLAG)
#define CID_MPEGCOLORBAR            (CID_ID(0x808bU) | CID_FLAG)
#define CID_NETSTACK_FUSION         (CID_ID(0x808cU) | CID_FLAG)
#define CID_NETSTACK_TARGET_TCP     (CID_ID(0x808dU) | CID_FLAG)
#define CID_NETSTACK_UPNP_ALLEGRO   (CID_ID(0x808dU) | CID_FLAG)
#define CID_NETSTACK_UPNP_INTEL     (CID_ID(0x808eU) | CID_FLAG)
#define CID_NETWORKREAD             (CID_ID(0x8090U) | CID_FLAG)
#define CID_NM_COMMON               (CID_ID(0x8091U) | CID_FLAG)
#define CID_NM_DEI                  (CID_ID(0x8092U) | CID_FLAG)
#define CID_NM_EST                  (CID_ID(0x8093U) | CID_FLAG)
#define CID_NM_QFD                  (CID_ID(0x8094U) | CID_FLAG)
#define CID_NM_UPC                  (CID_ID(0x8095U) | CID_FLAG)
#define CID_NM_UPC_SPIDER           (CID_ID(0x8096U) | CID_FLAG)
#define CID_OS                      (CID_ID(0x8097U) | CID_FLAG)
#define CID_PROBE                   (CID_ID(0x8098U) | CID_FLAG)
#define CID_PSIUTIL                 (CID_ID(0x8099U) | CID_FLAG)
#define CID_REALNETWORKS_ENGINE     (CID_ID(0x809aU) | CID_FLAG)
#define CID_SCAN_RATE_CONV_VSYS_TV  (CID_ID(0x809bU) | CID_FLAG)
#define CID_SPOSAL                  (CID_ID(0x809cU) | CID_FLAG)
#define CID_TIMEDOCTOR              (CID_ID(0x809dU) | CID_FLAG)
#define CID_TSA_CLOCK               (CID_ID(0x809eU) | CID_FLAG)

#define CID_TST_AVETC_SINK          (CID_ID(0x80a0U) | CID_FLAG)
#define CID_TST_DEMUX               (CID_ID(0x80a1U) | CID_FLAG)
#define CID_TST_DEMUX_FOR_MUX       (CID_ID(0x80a2U) | CID_FLAG)
#define CID_TST_SPTS_SINK           (CID_ID(0x80a3U) | CID_FLAG)
#define CID_TTI_UTIL                (CID_ID(0x80a4U) | CID_FLAG)
#define CID_UART                    (CID_ID(0x80a5U) | CID_FLAG)
#define CID_UPCONV100MC             (CID_ID(0x80a6U) | CID_FLAG)
#define CID_UTILCPIREC              (CID_ID(0x80a7U) | CID_FLAG)
#define CID_UTILCRYPTRIJNDAEL       (CID_ID(0x80a8U) | CID_FLAG)
#define CID_VATV                    (CID_ID(0x80a9U) | CID_FLAG)
#define CID_VATV_TR                 (CID_ID(0x80aaU) | CID_FLAG)
#define CID_VBI_INSERT_VSYS_TV      (CID_ID(0x80abU) | CID_FLAG)
#define CID_VCAP_VIP2               (CID_ID(0x80acU) | CID_FLAG)
#define CID_VDEC_BMP                (CID_ID(0x80adU) | CID_FLAG)
#define CID_VDEC_DIVX               (CID_ID(0x80aeU) | CID_FLAG)
#define CID_VDEC_GIF                (CID_ID(0x80afU) | CID_FLAG)
#define CID_VDEC_JPEG               (CID_ID(0x80b0U) | CID_FLAG)
#define CID_VDEC_JPEG2K             (CID_ID(0x80b1U) | CID_FLAG)
#define CID_VDEC_MP                 (CID_ID(0x80b2U) | CID_FLAG)
#define CID_VDECMPEG4               (CID_ID(0x80b3U) | CID_FLAG)
#define CID_VENC_MPEG4              (CID_ID(0x80b4U) | CID_FLAG)
#define CID_VENCMJPEG               (CID_ID(0x80b5U) | CID_FLAG)
#define CID_VENCMPEG2               (CID_ID(0x80b6U) | CID_FLAG)
#define CID_VIDEOUTIL               (CID_ID(0x80b7U) | CID_FLAG)
#define CID_VPACK                   (CID_ID(0x80b8U) | CID_FLAG)
#define CID_VPIP_REC_PLAY           (CID_ID(0x80b9U) | CID_FLAG)
#define CID_VPOST_ICP               (CID_ID(0x80baU) | CID_FLAG)
#define CID_VREND_VCP               (CID_ID(0x80bbU) | CID_FLAG)
#define CID_VRENDVO                 (CID_ID(0x80bcU) | CID_FLAG)
#define CID_VSCHED                  (CID_ID(0x80bdU) | CID_FLAG)
#define CID_VTBLBASE                (CID_ID(0x80beU) | CID_FLAG)
#define CID_VTRANS_MBS2             (CID_ID(0x80bfU) | CID_FLAG)
#define CID_VTRANS_QTNR             (CID_ID(0x80c0U) | CID_FLAG)
#define CID_VXWORKS_BSP             (CID_ID(0x80c1U) | CID_FLAG)
#define CID_WREAD                   (CID_ID(0x80c2U) | CID_FLAG)
#define CID_CONNMGR_ATV             (CID_ID(0x80c3U) | CID_FLAG)
#define CID_DL_VPK                  (CID_ID(0x80c4U) | CID_FLAG)
#define CID_VTRANS_VPK              (CID_ID(0x80c5U) | CID_FLAG)
#define CID_DL_VIP2                 (CID_ID(0x80c6U) | CID_FLAG)
#define CID_VX_GEN_UART             (CID_ID(0x80c7U) | CID_FLAG)
#define CID_VX_GPIO                 (CID_ID(0x80c8U) | CID_FLAG)
#define CID_VX_GEN_TIMER            (CID_ID(0x80c9U) | CID_FLAG)
#define CID_M4VENC_DIS              (CID_ID(0x80caU) | CID_FLAG)
#define CID_VENC_ANA                (CID_ID(0x80cbU) | CID_FLAG)
#define CID_BSL_VENC_ANA            (CID_ID(0x80ccU) | CID_FLAG)
#define CID_BSL_VENC_ANA_EXT        (CID_ID(0x80cdU) | CID_FLAG)
#define CID_BSL_VENC_ANAVBI_EXT     (CID_ID(0x80ceU) | CID_FLAG)
#define CID_CMDX                    (CID_ID(0x80cfU) | CID_FLAG)
#define CID_LL_GPIO                 (CID_ID(0x80d0U) | CID_FLAG)
#define CID_LL_KEYPAD               (CID_ID(0x80d1U) | CID_FLAG)
#define CID_LL_TIMER                (CID_ID(0x80d2U) | CID_FLAG)
#define CID_LL_SPI                  (CID_ID(0x80d3U) | CID_FLAG)
#define CID_LL_UART                 (CID_ID(0x80d4U) | CID_FLAG)
#define CID_LL_I2C                  (CID_ID(0x80d5U) | CID_FLAG)
#define CID_LL_TR                   (CID_ID(0x80d6U) | CID_FLAG)
#define CID_HW_KEYPAD               (CID_ID(0x80d7U) | CID_FLAG)
#define CID_HW_TIMER                (CID_ID(0x80d8U) | CID_FLAG)
#define CID_HW_SPI                  (CID_ID(0x80d9U) | CID_FLAG)
#define CID_HW_VATV_IOSYNC          (CID_ID(0x80daU) | CID_FLAG)
#define CID_DL_VO                   (CID_ID(0x80dbU) | CID_FLAG)
#define CID_DL_LVDS                 (CID_ID(0x80dcU) | CID_FLAG)
#define CID_HW_DDR2031              (CID_ID(0x80ddU) | CID_FLAG)
#define CID_BSL_PHY                 (CID_ID(0x80deU) | CID_FLAG)
#define CID_ETH_TTCP                (CID_ID(0x80dfU) | CID_FLAG)
#define CID_CDIGADEC_MP3PRO         (CID_ID(0x80e0U) | CID_FLAG)
#define CID_CID3EXTR                (CID_ID(0x80e1U) | CID_FLAG)
#define CID_IMAGEDEC_JPEG           (CID_ID(0x80e2U) | CID_FLAG)
#define CID_CURLSRC_MP3PRO          (CID_ID(0x80e3U) | CID_FLAG)
#define CID_CURLSRC_IMAGEDEC        (CID_ID(0x80e4U) | CID_FLAG)
#define CID_DVP_MAIN                (CID_ID(0x80e5U) | CID_FLAG)
#define CID_TMMAN32                 (CID_ID(0x80e6U) | CID_FLAG)
#define CID_TMMAN_CRT               (CID_ID(0x80e7U) | CID_FLAG)
#define CID_UHS_HAL_PCI             (CID_ID(0x80e8U) | CID_FLAG)
#define CID_UHS_OSAL_VXWORKS        (CID_ID(0x80e9U) | CID_FLAG)
#define CID_UHS_OSAL_PSOS           (CID_ID(0x80eaU) | CID_FLAG)
#define CID_UHS_USBD                (CID_ID(0x80ebU) | CID_FLAG)
#define CID_UHS_RBC                 (CID_ID(0x80ecU) | CID_FLAG)
#define CID_UHS_UFI                 (CID_ID(0x80edU) | CID_FLAG)
#define CID_UHS_SCSI                (CID_ID(0x80eeU) | CID_FLAG)
#define CID_UHS_PRINTER             (CID_ID(0x80efU) | CID_FLAG)
#define CID_UHS_MOUSE               (CID_ID(0x80f0U) | CID_FLAG)
#define CID_UHS_KEYBOARD            (CID_ID(0x80f1U) | CID_FLAG)
#define CID_UHS_HUB                 (CID_ID(0x80f2U) | CID_FLAG)
#define CID_UHS_HCD_1561            (CID_ID(0x80f3U) | CID_FLAG)
#define CID_CLEANUP                 (CID_ID(0x80f4U) | CID_FLAG)
#define CID_ALLOCATOR               (CID_ID(0x80f5U) | CID_FLAG)
#define CID_TCS_CORE_LIBDEV         (CID_ID(0x80f6U) | CID_FLAG)
#define CID_VDI_VDO_ROUTER          (CID_ID(0x80f7U) | CID_FLAG)
#define CID_CONNMGR_ATSC            (CID_ID(0x80f8U) | CID_FLAG)
#define CID_ASPDIF                  (CID_ID(0x80f9U) | CID_FLAG)
#define CID_APLL                    (CID_ID(0x80faU) | CID_FLAG)
#define CID_ATVPLFINSTVIN           (CID_ID(0x80fbU) | CID_FLAG)
#define CID_ATV_PLF                 (CID_ID(0x80fcU) | CID_FLAG)
#define CID_DL_WATCHDOG             (CID_ID(0x80fdU) | CID_FLAG)
#define CID_WMT_NET_READER          (CID_ID(0x80feU) | CID_FLAG)
#define CID_DL_FGPO                 (CID_ID(0x80ffU) | CID_FLAG)
#define CID_DL_FGPI                 (CID_ID(0x8100U) | CID_FLAG)
#define CID_WMT_DECODER             (CID_ID(0x8101U) | CID_FLAG)
#define CID_HAD_DRV_IIC             (CID_ID(0x8102U) | CID_FLAG)
#define CID_HAD_DRV_GPIO            (CID_ID(0x8103U) | CID_FLAG)
#define CID_HAD_GLOBAL              (CID_ID(0x8104U) | CID_FLAG)
#define CID_HAD_SMM                 (CID_ID(0x8105U) | CID_FLAG)
#define CID_HAD_DRV_CSM             (CID_ID(0x8106U) | CID_FLAG)
#define CID_CARACASWDOG             (CID_ID(0x8107U) | CID_FLAG)
#define CID_CARACASADC              (CID_ID(0x8108U) | CID_FLAG)
#define CID_CARACASDMA              (CID_ID(0x8109U) | CID_FLAG)
#define CID_CARACASFLASHCTRL        (CID_ID(0x810aU) | CID_FLAG)
#define CID_CARACASGPTIMER          (CID_ID(0x810bU) | CID_FLAG)
#define CID_CARACASGPIO             (CID_ID(0x810cU) | CID_FLAG)
#define CID_CARACASI2CMO            (CID_ID(0x810dU) | CID_FLAG)
#define CID_CARACASI2CMS            (CID_ID(0x810eU) | CID_FLAG)
#define CID_CARACASRTC              (CID_ID(0x810fU) | CID_FLAG)
#define CID_CARACASSPI              (CID_ID(0x8110U) | CID_FLAG)
#define CID_CARACASTIMER            (CID_ID(0x8111U) | CID_FLAG)
#define CID_CARACASUART             (CID_ID(0x8112U) | CID_FLAG)
#define CID_TSSA40                  (CID_ID(0x8113U) | CID_FLAG)
#define CID_PACKET_POOL             (CID_ID(0x8114U) | CID_FLAG)
#define CID_TSSA15_WRAPPER          (CID_ID(0x8115U) | CID_FLAG)
#define CID_TASK_SYNC               (CID_ID(0x8116U) | CID_FLAG)
#define CID_TASK_CONDITION          (CID_ID(0x8117U) | CID_FLAG)
#define CID_PACKET_QUEUE            (CID_ID(0x8118U) | CID_FLAG)
#define CID_CONNECTION_TOOLKIT      (CID_ID(0x8119U) | CID_FLAG)
#define CID_TSSA16                  (CID_ID(0x811aU) | CID_FLAG)
#define CID_UDSDFU                  (CID_ID(0x811bU) | CID_FLAG)
#define CID_BTH                     (CID_ID(0x811cU) | CID_FLAG)
#define CID_DCDIP9021               (CID_ID(0x811dU) | CID_FLAG)
#define CID_DCDIP3501V1X            (CID_ID(0x811eU) | CID_FLAG)
#define CID_ISP1581                 (CID_ID(0x811fU) | CID_FLAG)
#define CID_DCSS_TV                 (CID_ID(0x8120U) | CID_FLAG)
#define CID_DCSS_MON                (CID_ID(0x8121U) | CID_FLAG)
#define CID_DCSS_RSC_PC             (CID_ID(0x8122U) | CID_FLAG)
#define CID_DCSS_RSC_INT            (CID_ID(0x8123U) | CID_FLAG)
#define CID_DCSS_RSC_EXT            (CID_ID(0x8124U) | CID_FLAG)
#define CID_DCSS_LIT                (CID_ID(0x8125U) | CID_FLAG)
#define CID_DCSS_LIT_C              (CID_ID(0x8126U) | CID_FLAG)
#define CID_DCSS_45A                (CID_ID(0x8127U) | CID_FLAG)
#define CID_UDSCORE                 (CID_ID(0x8128U) | CID_FLAG)
#define CID_HW_AUDIO7135            (CID_ID(0x8129U) | CID_FLAG)
#define CID_DL_AUDIO3X              (CID_ID(0x812aU) | CID_FLAG)
#define CID_REGACC                  (CID_ID(0x812bU) | CID_FLAG)
#define CID_HW_MJPEG                (CID_ID(0x812cU) | CID_FLAG)
#define CID_ISP1582                 (CID_ID(0x812dU) | CID_FLAG)
#define CID_MUTI                    (CID_ID(0x812eU) | CID_FLAG)
#define CID_CHANNEL_DECODER_ENCODER (CID_ID(0x812fU) | CID_FLAG)
#define CID_RESMGR                  (CID_ID(0x8130U) | CID_FLAG)
#define CID_WIDGET                  (CID_ID(0x8131U) | CID_FLAG)
#define CID_FB                      (CID_ID(0x8132U) | CID_FLAG)
#define CID_GFX                     (CID_ID(0x8133U) | CID_FLAG)
#define CID_HPS_DISPATCHER          (CID_ID(0x8134U) | CID_FLAG)
#define CID_DL_PLXGPIO              (CID_ID(0x8135U) | CID_FLAG)
#define CID_HW_PLXGPIO              (CID_ID(0x8136U) | CID_FLAG)
#define CID_DL_PLXPHI               (CID_ID(0x8137U) | CID_FLAG)
#define CID_HW_PLXPHI_EVALUATOR     (CID_ID(0x8138U) | CID_FLAG)
#define CID_DL_SCALER               (CID_ID(0x8139U) | CID_FLAG)
#define CID_EFM                     (CID_ID(0x813aU) | CID_FLAG)
#define CID_HW_TUNER_FM1236MK3      (CID_ID(0x813bU) | CID_FLAG)
#define CID_HW_TUNER_FM1216MK3      (CID_ID(0x813cU) | CID_FLAG)
#define CID_HW_TUNER_FM1216MK2      (CID_ID(0x813dU) | CID_FLAG)
#define CID_ANALOG_CHANNEL_TABLE    (CID_ID(0x813eU) | CID_FLAG)
#define CID_TUNER_CONTROL           (CID_ID(0x813fU) | CID_FLAG)
#define CID_DL_UIMS                 (CID_ID(0x8140U) | CID_FLAG)
#define CID_DL_RCTRANSMITTER        (CID_ID(0x8141U) | CID_FLAG)
#define CID_HW_CST_RCRECEIVER       (CID_ID(0x8142U) | CID_FLAG)
#define CID_HW_CST_RCTRANSMITTER    (CID_ID(0x8143U) | CID_FLAG)
#define CID_DCDIP3506               (CID_ID(0x8144U) | CID_FLAG)
#define CID_DCDIP3501V2X            (CID_ID(0x8145U) | CID_FLAG)
#define CID_MTV_COORD               (CID_ID(0x8146U) | CID_FLAG)
#define CID_MTV_IMG_ROT_CTRL        (CID_ID(0x8147U) | CID_FLAG)
#define CID_TFE_TRACE               (CID_ID(0x8148U) | CID_FLAG)
#define CID_TMCAL_SERVER            (CID_ID(0x8149U) | CID_FLAG)
#define CID_BOOT_LOADER             (CID_ID(0x814aU) | CID_FLAG)
#define CID_TD_SAVE_DATA            (CID_ID(0x814bU) | CID_FLAG)
#define CID_TFE_TRACE_PROCESS_DATA  (CID_ID(0x814cU) | CID_FLAG)
#define CID_VIDEOCTRL               (CID_ID(0x814dU) | CID_FLAG)
#define CID_BOOT                    (CID_ID(0x814eU) | CID_FLAG)
#define CID_EVENT                   (CID_ID(0x814fU) | CID_FLAG)
#define CID_USERINPUT               (CID_ID(0x8150U) | CID_FLAG)
#define CID_BSL_TUNER               (CID_ID(0x8151U) | CID_FLAG)
#define CID_P5KIIC                  (CID_ID(0x8152U) | CID_FLAG)
#define CID_HW_PMANSECURITY         (CID_ID(0x8153U) | CID_FLAG)
#define CID_DRM_DIVX                (CID_ID(0x8154U) | CID_FLAG)
#define CID_TMHWVIDEODEC7136        (CID_ID(0x8155U) | CID_FLAG)
#define CID_TMDLVIDEODEC            (CID_ID(0x8156U) | CID_FLAG)
#define CID_OSD_KERNEL              (CID_ID(0x8157U) | CID_FLAG)
#define CID_HW_DCSNETWORK           (CID_ID(0x8158U) | CID_FLAG)
#define CID_DL_RCRECEIVER           (CID_ID(0x8159U) | CID_FLAG)
#define CID_INT                     (CID_ID(0x815aU) | CID_FLAG)
#define CID_RTC                     (CID_ID(0x815bU) | CID_FLAG)
#define CID_TIMER                   (CID_ID(0x815cU) | CID_FLAG)
#define CID_IPC                     (CID_ID(0x815dU) | CID_FLAG)
#define CID_P5KTELETEXT             (CID_ID(0x815eU) | CID_FLAG)
#define CID_P5KAUDIOVIDEO           (CID_ID(0x815fU) | CID_FLAG)
#define CID_P5KCONFIG               (CID_ID(0x8160U) | CID_FLAG)
#define CID_HW_CST_TRANSPSTREAMIN   (CID_ID(0x8161U) | CID_FLAG)
#define CID_HOMER_KERNEL            (CID_ID(0x8162U) | CID_FLAG)
#define CID_HOMER_DRIVER            (CID_ID(0x8163U) | CID_FLAG)
#define CID_CD_FILE_SYSTEM          (CID_ID(0x8164U) | CID_FLAG)
#define CID_COBALT_APP              (CID_ID(0x8165U) | CID_FLAG)
#define CID_COBALT_UI               (CID_ID(0x8166U) | CID_FLAG)
#define CID_CD_SERVO                (CID_ID(0x8167U) | CID_FLAG)
#define CID_CD_UTILS                (CID_ID(0x8168U) | CID_FLAG)
#define CID_COBALT_SYSTEM           (CID_ID(0x8169U) | CID_FLAG)
#define CID_CDSLIM                  (CID_ID(0x816aU) | CID_FLAG)
#define CID_CD_DATABASE             (CID_ID(0x816bU) | CID_FLAG)
#define CID_CANAVENC                (CID_ID(0x816cU) | CID_FLAG)
#define CID_CANTIAGING              (CID_ID(0x816dU) | CID_FLAG)
#define CID_CAUTOPICTCTRL           (CID_ID(0x816eU) | CID_FLAG)
#define CID_CBBARCTRL               (CID_ID(0x816fU) | CID_FLAG)
#define CID_CBBARDET                (CID_ID(0x8170U) | CID_FLAG)
#define CID_CBBARDETEXT             (CID_ID(0x8171U) | CID_FLAG)
#define CID_CBLEVELDETEXT           (CID_ID(0x8172U) | CID_FLAG)
#define CID_CCOLENH                 (CID_ID(0x8173U) | CID_FLAG)
#define CID_CCOLENHEXT              (CID_ID(0x8174U) | CID_FLAG)
#define CID_CCONTRESEXT             (CID_ID(0x8175U) | CID_FLAG)
#define CID_CCTI                    (CID_ID(0x8176U) | CID_FLAG)
#define CID_CCTIEXT                 (CID_ID(0x8177U) | CID_FLAG)
#define CID_CDNR                    (CID_ID(0x8178U) | CID_FLAG)
#define CID_CDNREXT                 (CID_ID(0x8179U) | CID_FLAG)
#define CID_CGAMMAEXT               (CID_ID(0x817aU) | CID_FLAG)
#define CID_CHISTOMEASEXT           (CID_ID(0x817bU) | CID_FLAG)
#define CID_CHISTOMOD               (CID_ID(0x817cU) | CID_FLAG)
#define CID_CHISTOMODEXT            (CID_ID(0x817dU) | CID_FLAG)
#define CID_CMBSXRAY                (CID_ID(0x817eU) | CID_FLAG)
#define CID_CNOISE                  (CID_ID(0x817fU) | CID_FLAG)
#define CID_CNOISEESTEXT            (CID_ID(0x8180U) | CID_FLAG)
#define CID_CPFSPD                  (CID_ID(0x8181U) | CID_FLAG)
#define CID_CQVCPXRAY               (CID_ID(0x8182U) | CID_FLAG)
#define CID_CSCANRATECONV           (CID_ID(0x8183U) | CID_FLAG)
#define CID_CSCANRATECONVEXT        (CID_ID(0x8184U) | CID_FLAG)
#define CID_CSHARPENH               (CID_ID(0x8185U) | CID_FLAG)
#define CID_CSHARPENHEXT            (CID_ID(0x8186U) | CID_FLAG)
#define CID_CSHARPMEAS              (CID_ID(0x8187U) | CID_FLAG)
#define CID_CSHARPMEASEXT           (CID_ID(0x8188U) | CID_FLAG)
#define CID_CSYNCTAG                (CID_ID(0x8189U) | CID_FLAG)
#define CID_CUVBWDETEXT             (CID_ID(0x818aU) | CID_FLAG)
#define CID_CVBISLICE               (CID_ID(0x818bU) | CID_FLAG)
#define CID_CVFEAT                  (CID_ID(0x818cU) | CID_FLAG)
#define CID_CVFEAT2                 (CID_ID(0x818dU) | CID_FLAG)
#define CID_CVIPXRAY                (CID_ID(0x818eU) | CID_FLAG)
#define CID_CVIPXRAYDITHER          (CID_ID(0x818fU) | CID_FLAG)
#define CID_CVMIX                   (CID_ID(0x8190U) | CID_FLAG)
#define CID_CVTRSCALEEXT            (CID_ID(0x8191U) | CID_FLAG)
#define CID_CVTRANTIAGING           (CID_ID(0x8192U) | CID_FLAG)
#define CID_CVTRFADEVCP             (CID_ID(0x8193U) | CID_FLAG)
#define CID_CVTRSCALEMBSVCP         (CID_ID(0x8194U) | CID_FLAG)
#define CID_CVTRSTROBEMBS           (CID_ID(0x8195U) | CID_FLAG)
#define CID_NM_UTILS                (CID_ID(0x8196U) | CID_FLAG)
#define CID_VSEQSCHEDENGINE         (CID_ID(0x8197U) | CID_FLAG)
#define CID_VCPSCHEDENGINE          (CID_ID(0x8198U) | CID_FLAG)
#define CID_VGENTEST                (CID_ID(0x8199U) | CID_FLAG)
#define CID_VMENU                   (CID_ID(0x819aU) | CID_FLAG)
#define CID_VPROCCOMMON             (CID_ID(0x819bU) | CID_FLAG)
#define CID_VPROCTV                 (CID_ID(0x819cU) | CID_FLAG)
#define CID_VPROCTV505E             (CID_ID(0x819dU) | CID_FLAG)
#define CID_SCHEDENGINE             (CID_ID(0x819eU) | CID_FLAG)
#define CID_VSLNMCOMMON             (CID_ID(0x819fU) | CID_FLAG)
#define CID_VSLVCAPVIP              (CID_ID(0x81a0U) | CID_FLAG)
#define CID_VSLVCAPVIPVBI           (CID_ID(0x81a1U) | CID_FLAG)
#define CID_VSLVINCONVERT           (CID_ID(0x81a2U) | CID_FLAG)
#define CID_VSLIOSYNC               (CID_ID(0x81a3U) | CID_FLAG)
#define CID_VSLVRENDVCP             (CID_ID(0x81a4U) | CID_FLAG)
#define CID_VSLVRENDVCPVBI          (CID_ID(0x81a5U) | CID_FLAG)
#define CID_VSLSYNCTAG              (CID_ID(0x81a6U) | CID_FLAG)
#define CID_VSLVTRANSMBS            (CID_ID(0x81a7U) | CID_FLAG)
#define CID_VSLVTRANSNM             (CID_ID(0x81a8U) | CID_FLAG)
#define CID_VSLVTRANSQTNR           (CID_ID(0x81a9U) | CID_FLAG)
#define CID_VSLVTRANSSWTNR          (CID_ID(0x81aaU) | CID_FLAG)
#define CID_VTRANSSWTNR             (CID_ID(0x81abU) | CID_FLAG)
#define CID_LL_DMA                  (CID_ID(0x81acU) | CID_FLAG)
#define CID_BSL_PNX8550             (CID_ID(0x81adU) | CID_FLAG)
#define CID_BSL_PNX1500             (CID_ID(0x81aeU) | CID_FLAG)
#define CID_BSL_NULL                (CID_ID(0x81afU) | CID_FLAG)
#define CID_BSL_PNX2015             (CID_ID(0x81b0U) | CID_FLAG)
#define CID_HW_SCALER7136           (CID_ID(0x81b1U) | CID_FLAG)
#define CID_SPI_IP3409              (CID_ID(0x81b2U) | CID_FLAG)
#define CID_SPI_3409                (CID_ID(0x81b3U) | CID_FLAG)
#define CID_SPISD_3409              (CID_ID(0x81b4U) | CID_FLAG)
#define CID_CONNMGRMP4RTPPLAYER     (CID_ID(0x81b5U) | CID_FLAG)
#define CID_DL_NANDFLASH2           (CID_ID(0x81b6U) | CID_FLAG)
#define CID_HW_HOSTIF               (CID_ID(0x81b7U) | CID_FLAG)
#define CID_LL_HOSTIF               (CID_ID(0x81b8U) | CID_FLAG)
#define CID_LL_MJPEG                (CID_ID(0x81b9U) | CID_FLAG)
#define CID_HW_SENSORIF             (CID_ID(0x81baU) | CID_FLAG)
#define CID_LL_SENSORIF             (CID_ID(0x81bbU) | CID_FLAG)
#define CID_HW_ECSP                 (CID_ID(0x81bcU) | CID_FLAG)
#define CID_LL_ECSP                 (CID_ID(0x81bdU) | CID_FLAG)
#define CID_HW_DOWNSCALER           (CID_ID(0x81beU) | CID_FLAG)
#define CID_LL_DOWNSCALER           (CID_ID(0x81bfU) | CID_FLAG)
#define CID_HW_UPSCALER             (CID_ID(0x81c0U) | CID_FLAG)
#define CID_LL_UPSCALER             (CID_ID(0x81c1U) | CID_FLAG)
#define CID_HW_JITTEREX             (CID_ID(0x81c2U) | CID_FLAG)
#define CID_LL_JITTEREX             (CID_ID(0x81c3U) | CID_FLAG)
#define CID_HW_NOISERED             (CID_ID(0x81c4U) | CID_FLAG)
#define CID_LL_NOISERED             (CID_ID(0x81c5U) | CID_FLAG)
#define CID_HW_JPEGENCODER          (CID_ID(0x81c6U) | CID_FLAG)
#define CID_LL_JPEGENCODER          (CID_ID(0x81c7U) | CID_FLAG)
#define CID_HW_FLASHLIGHT           (CID_ID(0x81c8U) | CID_FLAG)
#define CID_LL_FLASHLIGHT           (CID_ID(0x81c9U) | CID_FLAG)
#define CID_HW_TVCONVERTER          (CID_ID(0x81caU) | CID_FLAG)
#define CID_LL_TVCONVERTER          (CID_ID(0x81cbU) | CID_FLAG)
#define CID_HW_DVDOMATRIX           (CID_ID(0x81ccU) | CID_FLAG)
#define CID_LL_DVDOMATRIX           (CID_ID(0x81cdU) | CID_FLAG)
#define CID_HW_CLCD                 (CID_ID(0x81ceU) | CID_FLAG)
#define CID_LL_CLCD                 (CID_ID(0x81cfU) | CID_FLAG)
#define CID_HW_VDE                  (CID_ID(0x81d0U) | CID_FLAG)
#define CID_LL_VDE                  (CID_ID(0x81d1U) | CID_FLAG)
#define CID_HW_MCSPI                (CID_ID(0x81d2U) | CID_FLAG)
#define CID_LL_MCSPI                (CID_ID(0x81d3U) | CID_FLAG)
#define CID_HW_PWM                  (CID_ID(0x81d4U) | CID_FLAG)
#define CID_LL_PWM                  (CID_ID(0x81d5U) | CID_FLAG)
#define CID_OSAL_NXM                (CID_ID(0x81d6U) | CID_FLAG)
#define CID_MEMPROF                 (CID_ID(0x81d7U) | CID_FLAG)
#define CID_ALCONSTRETCH            (CID_ID(0x81d8U) | CID_FLAG)
#define CID_AUTOFOCUS               (CID_ID(0x81d9U) | CID_FLAG)
#define CID_LL_DVDO2DTL             (CID_ID(0x81daU) | CID_FLAG)
#define CID_HW_DVDO2DTL             (CID_ID(0x81dbU) | CID_FLAG)
#define CID_LL_DTL2DVDO             (CID_ID(0x81dcU) | CID_FLAG)
#define CID_HW_DTL2DVDO             (CID_ID(0x81ddU) | CID_FLAG)
#define CID_LL_COLORMATRIX          (CID_ID(0x81deU) | CID_FLAG)
#define CID_HW_COLORMATRIX          (CID_ID(0x81dfU) | CID_FLAG)
#define CID_UHSPDIFOUT_ASYSATV      (CID_ID(0x81e0U) | CID_FLAG)
#define CID_DL_NANDFLASH1           (CID_ID(0x81e1U) | CID_FLAG)
#define CID_NANDBOOTFFS             (CID_ID(0x81e2U) | CID_FLAG)
#define CID_CONNMGR_APROCTV         (CID_ID(0x81e3U) | CID_FLAG)
#define CID_CONNMGRSTILLPLAYER      (CID_ID(0x81e4U) | CID_FLAG)
#define CID_CONNMGRAUDIOPLAYER      (CID_ID(0x81e5U) | CID_FLAG)
#define CID_DCDIP9028               (CID_ID(0x81e6U) | CID_FLAG)
#define CID_CURLSRC_AUDIO           (CID_ID(0x81e7U) | CID_FLAG)
#define CID_CONNMGRAVIMP4PLAYER     (CID_ID(0x81e8U) | CID_FLAG)
#define CID_AUDIOVIDEOSYNC          (CID_ID(0x81e9U) | CID_FLAG)
#define CID_PACKETLIST              (CID_ID(0x81eaU) | CID_FLAG)
#define CID_ASYNCSINK               (CID_ID(0x81ebU) | CID_FLAG)
#define CID_VSYNCSINK               (CID_ID(0x81ecU) | CID_FLAG)
#define CID_XSYNCSINK               (CID_ID(0x81edU) | CID_FLAG)
#define CID_PCIEXP                  (CID_ID(0x81eeU) | CID_FLAG)
#define CID_SOD_KERNEL              (CID_ID(0x81efU) | CID_FLAG)
#define CID_SOD_EMULATE             (CID_ID(0x81f0U) | CID_FLAG)
#define CID_SOD_MGR                 (CID_ID(0x81f1U) | CID_FLAG)
#define CID_NANDPARTTABLE           (CID_ID(0x81f2U) | CID_FLAG)
#define CID_HW_AUDIO7136            (CID_ID(0x81f3U) | CID_FLAG)
#define CID_SPI3409                 (CID_ID(0x81f4U) | CID_FLAG)
#define CID_DCSS_MATH               (CID_ID(0x81f5U) | CID_FLAG)
#define CID_DCSS_LIT_CSD            (CID_ID(0x81f6U) | CID_FLAG)
#define CID_DCSS_LIT_M              (CID_ID(0x81f7U) | CID_FLAG)
#define CID_ADT                     (CID_ID(0x81f8U) | CID_FLAG)
#define CID_ACS                     (CID_ID(0x81f9U) | CID_FLAG)
#define CID_ACB                     (CID_ID(0x81faU) | CID_FLAG)
#define CID_ACL                     (CID_ID(0x81fbU) | CID_FLAG)
#define CID_AVEPP                   (CID_ID(0x81fcU) | CID_FLAG)
#define CID_UDSSIC                  (CID_ID(0x81fdU) | CID_FLAG)
#define CID_PROXYI2C                (CID_ID(0x81feU) | CID_FLAG)
#define CID_PL081DMA                (CID_ID(0x81feU) | CID_FLAG)
#define CID_DD_CPIPE                (CID_ID(0x81ffU) | CID_FLAG)
#define CID_DD_MBVP                 (CID_ID(0x8200U) | CID_FLAG)
#define CID_CARENDAOUT              (CID_ID(0x8201U) | CID_FLAG)
#define CID_CADIGAIN                (CID_ID(0x8202U) | CID_FLAG)
#define CID_CONNMGR_TV506E          (CID_ID(0x8203U) | CID_FLAG)
#define CID_ASYNCHANDLER            (CID_ID(0x8204U) | CID_FLAG)
#define CID_COMP_M4VENCPSC          (CID_ID(0x8205U) | CID_FLAG)
#define CID_CONNMGRNETSCHEMECONFIG  (CID_ID(0x8206U) | CID_FLAG)
#define CID_CARACASSPIAHB           (CID_ID(0x8207U) | CID_FLAG)
#define CID_COMP_ADECLPCM           (CID_ID(0x8208U) | CID_FLAG)
#define CID_CDIGADEC_MULTISTD       (CID_ID(0x8209U) | CID_FLAG)
#define CID_ADB                     (CID_ID(0x820aU) | CID_FLAG)
#define CID_ADR                     (CID_ID(0x820bU) | CID_FLAG)
#define CID_AGN                     (CID_ID(0x820cU) | CID_FLAG)
#define CID_ANT                     (CID_ID(0x820dU) | CID_FLAG)
#define CID_APP                     (CID_ID(0x820eU) | CID_FLAG)
#define CID_ASC                     (CID_ID(0x820fU) | CID_FLAG)
#define CID_ASM                     (CID_ID(0x8210U) | CID_FLAG)
#define CID_ASS                     (CID_ID(0x8211U) | CID_FLAG)
#define CID_ATP                     (CID_ID(0x8212U) | CID_FLAG)
#define CID_VDEC_MJPEG              (CID_ID(0x8213U) | CID_FLAG)
#define CID_MOV_READ                (CID_ID(0x8214U) | CID_FLAG)
#define CID_EWIFI                   (CID_ID(0x8215U) | CID_FLAG)
#define CID_SCR                     (CID_ID(0x8216U) | CID_FLAG)
#define CID_AEPP                    (CID_ID(0x8217U) | CID_FLAG)
#define CID_VEPP                    (CID_ID(0x8218U) | CID_FLAG)
#define CID_MP3ENC                  (CID_ID(0x8219U) | CID_FLAG)
#define CID_TDFLOADER               (CID_ID(0x821aU) | CID_FLAG)
#define CID_VIOSYNC                 (CID_ID(0x821bU) | CID_FLAG)
#define CID_STBDP                   (CID_ID(0x821cU) | CID_FLAG)
#define CID_STBEVENT                (CID_ID(0x821dU) | CID_FLAG)
#define CID_STBFB                   (CID_ID(0x821eU) | CID_FLAG)
#define CID_STBDEMUX                (CID_ID(0x821fU) | CID_FLAG)
#define CID_STBFILE                 (CID_ID(0x8220U) | CID_FLAG)
#define CID_STBGPIO                 (CID_ID(0x8221U) | CID_FLAG)
#define CID_STBI2C                  (CID_ID(0x8222U) | CID_FLAG)
#define CID_STBMMIOBUS              (CID_ID(0x8223U) | CID_FLAG)
#define CID_STBPROC                 (CID_ID(0x8224U) | CID_FLAG)
#define CID_STBROOT                 (CID_ID(0x8225U) | CID_FLAG)
#define CID_STBRPC                  (CID_ID(0x8226U) | CID_FLAG)
#define CID_STBRTC                  (CID_ID(0x8227U) | CID_FLAG)
#define CID_STBTMLOAD               (CID_ID(0x8228U) | CID_FLAG)
#define CID_STBSTREAMINGSYSTEM      (CID_ID(0x8229U) | CID_FLAG)
#define CID_STBVIDEOSCALER          (CID_ID(0x822aU) | CID_FLAG)
#define CID_STBANALOGBACKEND        (CID_ID(0x822bU) | CID_FLAG)
#define CID_STBVIDEORENDERER        (CID_ID(0x822cU) | CID_FLAG)
#define CID_DRV_MMU                 (CID_ID(0x822dU) | CID_FLAG)
#define CID_COMP_AINJECTOR          (CID_ID(0x822eU) | CID_FLAG)
#define CID_VDEC_ANA                (CID_ID(0x822fU) | CID_FLAG)
#define CID_STBAC3AUD               (CID_ID(0x8230U) | CID_FLAG)
#define CID_STBAUDIO                (CID_ID(0x8231U) | CID_FLAG)
#define CID_PHMODARM11WRAPPER       (CID_ID(0x8232U) | CID_FLAG)
#define CID_GPIO_IP4004             (CID_ID(0x8233U) | CID_FLAG)
#define CID_TMCADIGSPDIFIN          (CID_ID(0x8234U) | CID_FLAG)
#define CID_TMCARENDSPDIFOUT        (CID_ID(0x8235U) | CID_FLAG)
#define CID_TMCPLFINSTAIN           (CID_ID(0x8236U) | CID_FLAG)
#define CID_TMCPLFINSTAOUT          (CID_ID(0x8237U) | CID_FLAG)
#define CID_TMCSPDIFIN              (CID_ID(0x8238U) | CID_FLAG)
#define CID_TMCSPDIFOUT             (CID_ID(0x8239U) | CID_FLAG)
#define CID_BSL_HDMIRX              (CID_ID(0x823aU) | CID_FLAG)
#define CID_AACPENC                 (CID_ID(0x823bU) | CID_FLAG)
#define CID_DL_HDMIRX               (CID_ID(0x823cU) | CID_FLAG)
#define CID_APP_HDMIRX              (CID_ID(0x823dU) | CID_FLAG)
#define CID_INFRA_HDMI              (CID_ID(0x823eU) | CID_FLAG)
#define CID_DL_HDMICEC              (CID_ID(0x823fU) | CID_FLAG)
#define CID_BSL_HDMITX              (CID_ID(0x8240U) | CID_FLAG)
#define CID_DL_HDMITX               (CID_ID(0x8241U) | CID_FLAG)
#define CID_APP_HDMITX              (CID_ID(0x8242U) | CID_FLAG)

/*define CID_UART                   (CID_ID(0x80a5U) | CID_FLAG) already defined*/
#define CID_CHIP                    (CID_ID(0x815bU) | CID_FLAG)

#define CID_RESERVED                (CID_ID(0xff80U) | CID_FLAG)
/* ************************************************************************** */
/* Component Id's reserved for external organizations                         */
/*                                                                            */
/*                            0xff80 thru 0xffbf                              */
/* Range of component ID's is reserved for the use of parties outside of      */
/*  Philips that wish to use component ID's privately.                        */
/*  If a component is going to be exchanged in the 'PS Ecosystem', then a     */
/*  public component ID should be registered with MoReUse.                    */
/*                                                                            */
/* Range to be used by CE Television Systems                                  */
/*                            0xffc0 thru 0xffff                              */
/*                                                                            */
/* ************************************************************************** */

/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Component ID types are defined as unsigned 32 bit integers (UInt32)        */
/* Interface ID types are defined as unsigned 32 bit integers (UInt32)        */
/*                                                                            */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                                                            */
/*  Obsolete Component ID values                                              */
/*                                                                            */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Component Class definitions (bits 31:28, 4 bits)                           */
/* NOTE: A class of 0x0 must not be defined to ensure that the overall 32 bit */
/*       component ID/status combination is always non-0 (no TM_OK conflict). */
/* -------------------------------------------------------------------------- */
#define CID_CLASS_BITSHIFT  28
#define CID_CLASS_BITMASK   (0xFU << CID_CLASS_BITSHIFT)
#define CID_GET_CLASS(compId) ((compId & CID_CLASS_BITMASK) >> CID_CLASS_BITSHIFT)

#define CID_CLASS_NONE      (0x1U << CID_CLASS_BITSHIFT)
#define CID_CLASS_VIDEO     (0x2U << CID_CLASS_BITSHIFT)
#define CID_CLASS_AUDIO     (0x3U << CID_CLASS_BITSHIFT)
#define CID_CLASS_GRAPHICS  (0x4U << CID_CLASS_BITSHIFT)
#define CID_CLASS_BUS       (0x5U << CID_CLASS_BITSHIFT)
#define CID_CLASS_INFRASTR  (0x6U << CID_CLASS_BITSHIFT)

#define CID_CLASS_CUSTOMER  (0xFU << CID_CLASS_BITSHIFT)

/* -------------------------------------------------------------------------- */
/* Component Type definitions (bits 27:24, 4 bits)                            */
/* -------------------------------------------------------------------------- */
#define CID_TYPE_BITSHIFT   24
#define CID_TYPE_BITMASK    (0xFU << CID_TYPE_BITSHIFT)
#define CID_GET_TYPE(compId)  ((compId & CID_TYPE_BITMASK) >> CID_TYPE_BITSHIFT)

#define CID_TYPE_NONE       (0x0U << CID_TYPE_BITSHIFT)
#define CID_TYPE_SOURCE     (0x1U << CID_TYPE_BITSHIFT)
#define CID_TYPE_SINK       (0x2U << CID_TYPE_BITSHIFT)
#define CID_TYPE_ENCODER    (0x3U << CID_TYPE_BITSHIFT)
#define CID_TYPE_DECODER    (0x4U << CID_TYPE_BITSHIFT)
#define CID_TYPE_MUX        (0x5U << CID_TYPE_BITSHIFT)
#define CID_TYPE_DEMUX      (0x6U << CID_TYPE_BITSHIFT)
#define CID_TYPE_DIGITIZER  (0x7U << CID_TYPE_BITSHIFT)
#define CID_TYPE_RENDERER   (0x8U << CID_TYPE_BITSHIFT)
#define CID_TYPE_FILTER     (0x9U << CID_TYPE_BITSHIFT)
#define CID_TYPE_CONTROL    (0xAU << CID_TYPE_BITSHIFT)
#define CID_TYPE_DATABASE   (0xBU << CID_TYPE_BITSHIFT)
#define CID_TYPE_SUBSYSTEM  (0xCU << CID_TYPE_BITSHIFT)
#define CID_TYPE_CUSTOMER   (0xFU << CID_TYPE_BITSHIFT)

/* -------------------------------------------------------------------------- */
/* Component Tag definitions (bits 23:16, 8 bits)                             */
/* NOTE: Component tags are defined in groups, dependent on the class and     */
/* type.                                                                      */
/* -------------------------------------------------------------------------- */
#define CID_TAG_BITSHIFT    16
#define CID_TAG_BITMASK     (0xFFU << CID_TAG_BITSHIFT)

#define CID_TAG_NONE        (0x00U << CID_TAG_BITSHIFT)

#define CID_TAG_CUSTOMER    (0xE0U << CID_TAG_BITSHIFT)

#define TAG(number)         ((number) << CID_TAG_BITSHIFT)

/* -------------------------------------------------------------------------- */
/* General Component Layer definitions (bits 15:12, 4 bits)                   */
/* -------------------------------------------------------------------------- */
#define CID_LAYER_BITSHIFT  12
#define CID_LAYER_BITMASK   (0xF << CID_LAYER_BITSHIFT)
#define CID_GET_LAYER(compId) ((compId & CID_LAYER_BITMASK) >> CID_LAYER_BITSHIFT)

#define CID_LAYER_NONE      (0x0U << CID_LAYER_BITSHIFT)
#define CID_LAYER_BTM       (0x1U << CID_LAYER_BITSHIFT)
#define CID_LAYER_HWAPI     (0x2U << CID_LAYER_BITSHIFT)
#define CID_LAYER_BSL       (0x3U << CID_LAYER_BITSHIFT)
#define CID_LAYER_DEVLIB    (0x4U << CID_LAYER_BITSHIFT)
#define CID_LAYER_TMAL      (0x5U << CID_LAYER_BITSHIFT)
#define CID_LAYER_TMOL      (0x6U << CID_LAYER_BITSHIFT)
#define CID_LAYER_TMNL      (0xEU << CID_LAYER_BITSHIFT)

/* -------------------------------------------------------------------------- */
/*   "new" i.e. after 2002-01-31 layer definitions                            */
/* "New" Component Layers depend on the component type and class              */
/* So we can have an identical layer value for each type/class combination    */
/* In order not to break existing code that assumes that layers are unique,   */
/* we start new layers at 0x7                                                 */
/* -------------------------------------------------------------------------- */

/*------------------ CTYP_BUS_NOTYPE dependent layer definitions -------------*/
#define CID_LAYER_UDS      (0x7U << CID_LAYER_BITSHIFT)	/* USB Device Stack   */
#define CID_LAYER_UHS      (0x8U << CID_LAYER_BITSHIFT)	/* USB Host stack     */
#define CID_LAYER_UOTG     (0x9U << CID_LAYER_BITSHIFT)	/* USB OTG stack      */

#define CID_LAYER_CUSTOMER  (0xFU << CID_LAYER_BITSHIFT)	/* Customer Defined   */

/* -------------------------------------------------------------------------- */
/* Component Identifier definitions (bits 31:12, 20 bits)                     */
/* NOTE: These DVP platform component identifiers are designed to be unique   */
/*       within the system.  The component identifier encompasses the class   */
/*       (CID_CLASS_XXX), type (CID_TYPE_XXX), tag, and layer (CID_LAYER_XXX) */
/*       fields to form the unique component identifier.  This allows any     */
/*       error/progress status value to be identified as to its original      */
/*       source, whether or not the source component s header file is present.*/
/*       The standard error/progress status definitions should be used        */
/*       whenever possible to ease status interpretation.  No layer           */
/*       information is defined at this point; it should be ORed into the API */
/*       status values defined in the APIs header file.                       */
/* -------------------------------------------------------------------------- */
#if     (CID_LAYER_NONE != 0)
#error  ERROR: DVP component identifiers require the layer type 'NONE' = 0 !
#endif

/* -------------------------------------------------------------------------- */
/* Classless Types/Components (don t fit into other class categories)         */
/* -------------------------------------------------------------------------- */
#define CTYP_NOCLASS_NOTYPE       (CID_CLASS_NONE | CID_TYPE_NONE)
#define CTYP_NOCLASS_SOURCE       (CID_CLASS_NONE | CID_TYPE_SOURCE)
#define CTYP_NOCLASS_SINK         (CID_CLASS_NONE | CID_TYPE_SINK)
#define CTYP_NOCLASS_MUX          (CID_CLASS_NONE | CID_TYPE_MUX)
#define CTYP_NOCLASS_DEMUX        (CID_CLASS_NONE | CID_TYPE_DEMUX)
#define CTYP_NOCLASS_FILTER       (CID_CLASS_NONE | CID_TYPE_FILTER)
#define CTYP_NOCLASS_CONTROL      (CID_CLASS_NONE | CID_TYPE_CONTROL)
#define CTYP_NOCLASS_DATABASE     (CID_CLASS_NONE | CID_TYPE_DATABASE)
#define CTYP_NOCLASS_SUBSYS       (CID_CLASS_NONE | CID_TYPE_SUBSYSTEM)

#define CID_COMP_CLOCK            (TAG(0x01U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_DMA              (TAG(0x02U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_PIC              (TAG(0x03U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_NORFLASH         (TAG(0x04U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_NANDFLASH        (TAG(0x05U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_GPIO             (TAG(0x06U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_SMARTCARD        (TAG(0x07U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_UDMA             (TAG(0x08U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_DSP              (TAG(0x09U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_TIMER            (TAG(0x0AU) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_TSDMA            (TAG(0x0BU) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_MMIARB           (TAG(0x0CU) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_EEPROM           (TAG(0x0DU) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_PARPORT          (TAG(0x0EU) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_VSS              (TAG(0x0FU) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_TSIO             (TAG(0x10U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_DBG              (TAG(0x11U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_TTE              (TAG(0x12U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_AVPROP           (TAG(0x13U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_SERIAL_RAM       (TAG(0x14U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_SMARTMEDIA       (TAG(0x15U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_COMPACT_FLASH    (TAG(0x16U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_CI               (TAG(0x17U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_INT_ALARM        (TAG(0x18U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_TASK_ALARM       (TAG(0x19U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_XDMA             (TAG(0x1AU) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_ICC              (TAG(0x1BU) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_CONNMGR          (TAG(0x1CU) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_CONNMGRVSYSTV    (TAG(0x1DU) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_VBISLICERVSYSTV  (TAG(0x1EU) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_VMIXVSYSTV       (TAG(0x1FU) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_NTF              (TAG(0x20U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_NTY              CID_COMP_NTF	/* legacy */
#define CID_COMP_FATERR           (TAG(0x21U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_DVBTDEMOD        (TAG(0x22U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_HYBRIDTUNER      (TAG(0x23U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_VLD              (TAG(0x24U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_GIC              (TAG(0x25U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_WEB              (TAG(0x26U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_ANAEPGDB         (TAG(0x27U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_HWSEM            (TAG(0x28U) | CTYP_NOCLASS_NOTYPE)
#define CID_COMP_MMON             (TAG(0x29U) | CTYP_NOCLASS_NOTYPE)

#define CID_COMP_FREAD            (TAG(0x01U) | CTYP_NOCLASS_SOURCE)
#define CID_COMP_CDRREAD          (TAG(0x02U) | CTYP_NOCLASS_SOURCE)
#define CID_COMP_VSB              (TAG(0x03U) | CTYP_NOCLASS_SOURCE)
#define CID_COMP_ANALOGTVTUNER    (TAG(0x04U) | CTYP_NOCLASS_SOURCE)
#define CID_COMP_TPINMPEG2        (TAG(0x05U) | CTYP_NOCLASS_SOURCE)
#define CID_COMP_DREAD            (TAG(0x06U) | CTYP_NOCLASS_SOURCE)
#define CID_COMP_TREAD            (TAG(0x07U) | CTYP_NOCLASS_SOURCE)
#define CID_COMP_RTC              (TAG(0x08U) | CTYP_NOCLASS_SOURCE)
#define CID_COMP_TOUCHC           (TAG(0x09U) | CTYP_NOCLASS_SOURCE)
#define CID_COMP_KEYPAD           (TAG(0x0AU) | CTYP_NOCLASS_SOURCE)
#define CID_COMP_ADC              (TAG(0x0BU) | CTYP_NOCLASS_SOURCE)
#define CID_COMP_READLIST         (TAG(0x0CU) | CTYP_NOCLASS_SOURCE)
#define CID_COMP_FROMDISK         (TAG(0x0DU) | CTYP_NOCLASS_SOURCE)
#define CID_COMP_SOURCE           (TAG(0x0EU) | CTYP_NOCLASS_SOURCE)

#define CID_COMP_FWRITE           (TAG(0x01U) | CTYP_NOCLASS_SINK)
#define CID_COMP_CDWRITE          (TAG(0x02U) | CTYP_NOCLASS_SINK)
#define CID_COMP_CHARLCD          (TAG(0x03U) | CTYP_NOCLASS_SINK)
#define CID_COMP_PWM              (TAG(0x04U) | CTYP_NOCLASS_SINK)
#define CID_COMP_DAC              (TAG(0x05U) | CTYP_NOCLASS_SINK)
#define CID_COMP_TSDMAINJECTOR    (TAG(0x06U) | CTYP_NOCLASS_SINK)
#define CID_COMP_TODISK           (TAG(0x07U) | CTYP_NOCLASS_SINK)

#define CID_COMP_MUXMPEGPS        (TAG(0x01U) | CTYP_NOCLASS_MUX)
#define CID_COMP_MUXMPEG          (TAG(0x02U) | CTYP_NOCLASS_MUX)

#define CID_COMP_DEMUXMPEGTS      (TAG(0x01U) | CTYP_NOCLASS_DEMUX)
#define CID_COMP_DEMUXMPEGPS      (TAG(0x02U) | CTYP_NOCLASS_DEMUX)
#define CID_COMP_DEMUXDV          (TAG(0x03U) | CTYP_NOCLASS_DEMUX)

#define CID_COMP_COPYIO           (TAG(0x01U) | CTYP_NOCLASS_FILTER)
#define CID_COMP_COPYINPLACE      (TAG(0x02U) | CTYP_NOCLASS_FILTER)
#define CID_COMP_UART             (TAG(0x03U) | CTYP_NOCLASS_FILTER)
#define CID_COMP_SSI              (TAG(0x04U) | CTYP_NOCLASS_FILTER)
#define CID_COMP_MODEMV34         (TAG(0x05U) | CTYP_NOCLASS_FILTER)
#define CID_COMP_MODEMV42         (TAG(0x06U) | CTYP_NOCLASS_FILTER)
#define CID_COMP_HTMLPARSER       (TAG(0x07U) | CTYP_NOCLASS_FILTER)
#define CID_COMP_VMSP             (TAG(0x08U) | CTYP_NOCLASS_FILTER)
#define CID_COMP_X                (TAG(0x09U) | CTYP_NOCLASS_FILTER)
#define CID_COMP_TXTSUBTDECEBU    (TAG(0x0AU) | CTYP_NOCLASS_FILTER)
#define CID_COMP_CPI              (TAG(0x0BU) | CTYP_NOCLASS_FILTER)
#define CID_COMP_TRICK            (TAG(0x0CU) | CTYP_NOCLASS_FILTER)
#define CID_COMP_FWRITEFREAD      (TAG(0x0DU) | CTYP_NOCLASS_FILTER)

#define CID_COMP_REMCTL5          (TAG(0x01U) | CTYP_NOCLASS_CONTROL)
#define CID_COMP_INFRARED         (TAG(0x02U) | CTYP_NOCLASS_CONTROL)

#define CID_COMP_PSIP             (TAG(0x01U) | CTYP_NOCLASS_DATABASE)
#define CID_COMP_IDE              (TAG(0x02U) | CTYP_NOCLASS_DATABASE)
#define CID_COMP_DISKSCHED        (TAG(0x03U) | CTYP_NOCLASS_DATABASE)
#define CID_COMP_AVFS             (TAG(0x04U) | CTYP_NOCLASS_DATABASE)
#define CID_COMP_MDB              (TAG(0x05U) | CTYP_NOCLASS_DATABASE)
#define CID_COMP_ATAPI_CMDS       (TAG(0x06U) | CTYP_NOCLASS_DATABASE)

#define CID_COMP_IRDMMPEG         (TAG(0x01U) | CTYP_NOCLASS_SUBSYS)
#define CID_COMP_STORSYS          (TAG(0x02U) | CTYP_NOCLASS_SUBSYS)
#define CID_COMP_PMU              (TAG(0x03U) | CTYP_NOCLASS_SUBSYS)

/* -------------------------------------------------------------------------- */
/* Video Class Types/Components (video types handle video/graphics data)      */
/* -------------------------------------------------------------------------- */
#define CTYP_VIDEO_SINK            (CID_CLASS_VIDEO | CID_TYPE_SINK)
#define CTYP_VIDEO_SOURCE          (CID_CLASS_VIDEO | CID_TYPE_SOURCE)
#define CTYP_VIDEO_ENCODER         (CID_CLASS_VIDEO | CID_TYPE_ENCODER)
#define CTYP_VIDEO_DECODER         (CID_CLASS_VIDEO | CID_TYPE_DECODER)
#define CTYP_VIDEO_DIGITIZER       (CID_CLASS_VIDEO | CID_TYPE_DIGITIZER)
#define CTYP_VIDEO_RENDERER        (CID_CLASS_VIDEO | CID_TYPE_RENDERER)
#define CTYP_VIDEO_FILTER          (CID_CLASS_VIDEO | CID_TYPE_FILTER)
#define CTYP_VIDEO_SUBSYS          (CID_CLASS_VIDEO | CID_TYPE_SUBSYSTEM)

#define CID_COMP_LCD               (TAG(0x01U) | CTYP_VIDEO_SINK)

#define CID_COMP_VCAPVI            (TAG(0x01U) | CTYP_VIDEO_SOURCE)
#define CID_COMP_VIP               (TAG(0x02U) | CTYP_VIDEO_SOURCE)
#define CID_COMP_VI                (TAG(0x03U) | CTYP_VIDEO_SOURCE)
#define CID_COMP_VSLICER           (TAG(0x04U) | CTYP_VIDEO_SOURCE)
#define CID_COMP_FBREAD            (TAG(0x05U) | CTYP_VIDEO_SOURCE)
#define CID_COMP_QVI               (TAG(0x06U) | CTYP_VIDEO_SOURCE)
#define CID_COMP_CAMERA            (TAG(0x07U) | CTYP_VIDEO_SOURCE)
#define CID_COMP_CAM_SENSOR        (TAG(0x08U) | CTYP_VIDEO_SOURCE)

#define CID_COMP_VENCM1            (TAG(0x01U) | CTYP_VIDEO_ENCODER)
#define CID_COMP_VENCM2            (TAG(0x02U) | CTYP_VIDEO_ENCODER)
#define CID_COMP_VENCMJ            (TAG(0x03U) | CTYP_VIDEO_ENCODER)
#define CID_COMP_VENCH263          (TAG(0x04U) | CTYP_VIDEO_ENCODER)
#define CID_COMP_VENCH261          (TAG(0x05U) | CTYP_VIDEO_ENCODER)
#define CID_COMP_M4VENC            (TAG(0x06U) | CTYP_VIDEO_ENCODER)
#define CID_COMP_M4VENCME          (TAG(0x07U) | CTYP_VIDEO_ENCODER)
#define CID_COMP_M4VENCTC          (TAG(0x08U) | CTYP_VIDEO_ENCODER)
#define CID_COMP_M4VENCBSG         (TAG(0x09U) | CTYP_VIDEO_ENCODER)
#define CID_COMP_M4VENCJPEG        (TAG(0x0AU) | CTYP_VIDEO_ENCODER)

#define CID_COMP_VDECM1            (TAG(0x01U) | CTYP_VIDEO_DECODER)
#define CID_COMP_VDECM2            (TAG(0x02U) | CTYP_VIDEO_DECODER)
#define CID_COMP_VDECMPEG          (TAG(0x03U) | CTYP_VIDEO_DECODER)
#define CID_COMP_VDECMJ            (TAG(0x04U) | CTYP_VIDEO_DECODER)
#define CID_COMP_VDECSUBPICSVCD    (TAG(0x05U) | CTYP_VIDEO_DECODER)
#define CID_COMP_VDECH263          (TAG(0x06U) | CTYP_VIDEO_DECODER)
#define CID_COMP_VDECH261          (TAG(0x07U) | CTYP_VIDEO_DECODER)
#define CID_COMP_VDEC              (TAG(0x08U) | CTYP_VIDEO_DECODER)
#define CID_COMP_VDECSUBPICDVD     (TAG(0x09U) | CTYP_VIDEO_DECODER)
#define CID_COMP_VDECSUBPICBMPDVD  (TAG(0x0AU) | CTYP_VIDEO_DECODER)
#define CID_COMP_VDECSUBPICRENDDVD (TAG(0x0BU) | CTYP_VIDEO_DECODER)
#define CID_COMP_M4PP              (TAG(0x0CU) | CTYP_VIDEO_DECODER)
#define CID_COMP_M4MC              (TAG(0x0DU) | CTYP_VIDEO_DECODER)
#define CID_COMP_M4CSC             (TAG(0x0EU) | CTYP_VIDEO_DECODER)
#define CID_COMP_VDECTXT           (TAG(0x0FU) | CTYP_VIDEO_DECODER)
#define CID_COMP_VDECDV            (TAG(0x10U) | CTYP_VIDEO_DECODER)
#define CID_COMP_BACKANIM          (TAG(0x11U) | CTYP_VIDEO_DECODER)

#define CID_COMP_VDIG              (TAG(0x01U) | CTYP_VIDEO_DIGITIZER)
#define CID_COMP_VDIGVIRAW         (TAG(0x02U) | CTYP_VIDEO_DIGITIZER)
#define CID_COMP_VDIG_EXT          (TAG(0x03U) | CTYP_VIDEO_DIGITIZER)
#define CID_COMP_VDIG_VBI          (TAG(0x04U) | CTYP_VIDEO_DIGITIZER)
#define CID_COMP_VDIG_EXT_VBI      (TAG(0x05U) | CTYP_VIDEO_DIGITIZER)

#define CID_COMP_VREND             (TAG(0x01U) | CTYP_VIDEO_RENDERER)
#define CID_COMP_HDVO              (TAG(0x02U) | CTYP_VIDEO_RENDERER)
#define CID_COMP_VRENDGFXVO        (TAG(0x03U) | CTYP_VIDEO_RENDERER)
#define CID_COMP_AICP              (TAG(0x04U) | CTYP_VIDEO_RENDERER)
#define CID_COMP_VRENDVORAW        (TAG(0x05U) | CTYP_VIDEO_RENDERER)
#define CID_COMP_VO                (TAG(0x06U) | CTYP_VIDEO_RENDERER)
#define CID_COMP_VRENDVOICP        (TAG(0x07U) | CTYP_VIDEO_RENDERER)
#define CID_COMP_VMIX              (TAG(0x08U) | CTYP_VIDEO_RENDERER)
#define CID_COMP_QVCP              (TAG(0x09U) | CTYP_VIDEO_RENDERER)
#define CID_COMP_VREND_EXT         (TAG(0x0AU) | CTYP_VIDEO_RENDERER)
#define CID_COMP_VENCANA           (TAG(0x0BU) | CTYP_VIDEO_RENDERER)
#define CID_COMP_QVO               (TAG(0x0CU) | CTYP_VIDEO_RENDERER)

#define CID_COMP_MBS               (TAG(0x01U) | CTYP_VIDEO_FILTER)
#define CID_COMP_VTRANS            (TAG(0x02U) | CTYP_VIDEO_FILTER)
#define CID_COMP_QNM               (TAG(0x03U) | CTYP_VIDEO_FILTER)
#define CID_COMP_ICP               (TAG(0x04U) | CTYP_VIDEO_FILTER)
#define CID_COMP_VTRANSNM          (TAG(0x05U) | CTYP_VIDEO_FILTER)
#define CID_COMP_QFD               (TAG(0x06U) | CTYP_VIDEO_FILTER)
#define CID_COMP_VTRANSDVD         (TAG(0x07U) | CTYP_VIDEO_FILTER)
#define CID_COMP_VTRANSCRYSTAL     (TAG(0x08U) | CTYP_VIDEO_FILTER)
#define CID_COMP_VTRANSUD          (TAG(0x09U) | CTYP_VIDEO_FILTER)
/*#define CID_COMP_QTNR           (TAG(0x0AU) | CTYP_VIDEO_FILTER) Removed v17:  Replaced with CID_VTRANS_QTNR */

#define CID_COMP_VSYSMT3           (TAG(0x01U) | CTYP_VIDEO_SUBSYS)
#define CID_COMP_VSYSSTB           (TAG(0x01U) | CTYP_VIDEO_SUBSYS)
#define CID_COMP_DVDVIDSYS         (TAG(0x02U) | CTYP_VIDEO_SUBSYS)
#define CID_COMP_VDECUD            (TAG(0x03U) | CTYP_VIDEO_SUBSYS)
#define CID_COMP_VIDSYS            (TAG(0x04U) | CTYP_VIDEO_SUBSYS)
#define CID_COMP_VSYSTV            (TAG(0x05U) | CTYP_VIDEO_SUBSYS)

/* -------------------------------------------------------------------------- */
/* Audio Class Types/Components (audio types primarily handle audio data)     */
/* -------------------------------------------------------------------------- */
#define CTYP_AUDIO_NOTYPE       (CID_CLASS_AUDIO | CID_TYPE_NONE)
#define CTYP_AUDIO_SINK         (CID_CLASS_AUDIO | CID_TYPE_SINK)
#define CTYP_AUDIO_SOURCE       (CID_CLASS_AUDIO | CID_TYPE_SOURCE)
#define CTYP_AUDIO_ENCODER      (CID_CLASS_AUDIO | CID_TYPE_ENCODER)
#define CTYP_AUDIO_DECODER      (CID_CLASS_AUDIO | CID_TYPE_DECODER)
#define CTYP_AUDIO_DIGITIZER    (CID_CLASS_AUDIO | CID_TYPE_DIGITIZER)
#define CTYP_AUDIO_RENDERER     (CID_CLASS_AUDIO | CID_TYPE_RENDERER)
#define CTYP_AUDIO_FILTER       (CID_CLASS_AUDIO | CID_TYPE_FILTER)
#define CTYP_AUDIO_SUBSYS       (CID_CLASS_AUDIO | CID_TYPE_SUBSYSTEM)

#define CID_COMP_CODEC          (TAG(0x01U) | CTYP_AUDIO_NOTYPE)

#define CID_COMP_SDAC           (TAG(0x01U) | CTYP_AUDIO_SINK)

#define CID_COMP_ADIGAI         (TAG(0x01U) | CTYP_AUDIO_DIGITIZER)
#define CID_COMP_ADIGSPDIF      (TAG(0x02U) | CTYP_AUDIO_DIGITIZER)

#define CID_COMP_ARENDAO        (TAG(0x01U) | CTYP_AUDIO_RENDERER)
#define CID_COMP_ARENDSPDIF     (TAG(0x02U) | CTYP_AUDIO_RENDERER)

#define CID_COMP_NOISESEQ       (TAG(0x03U) | CTYP_AUDIO_SOURCE)

#define CID_COMP_AENCAC3        (TAG(0x01U) | CTYP_AUDIO_ENCODER)
#define CID_COMP_AENCMPEG1      (TAG(0x02U) | CTYP_AUDIO_ENCODER)
#define CID_COMP_AENCAAC        (TAG(0x03U) | CTYP_AUDIO_ENCODER)
#define CID_COMP_AENCG723       (TAG(0x04U) | CTYP_AUDIO_ENCODER)
#define CID_COMP_AENCG728       (TAG(0x05U) | CTYP_AUDIO_ENCODER)
#define CID_COMP_AENCWMA        (TAG(0x06U) | CTYP_AUDIO_ENCODER)
#define CID_COMP_AVENCMPEG      (TAG(0x07U) | CTYP_AUDIO_ENCODER)
#define CID_COMP_AENCMP3        (TAG(0x08U) | CTYP_AUDIO_ENCODER)

#define CID_COMP_ADECPROLOGIC   (TAG(0x01U) | CTYP_AUDIO_DECODER)
#define CID_COMP_ADECAC3        (TAG(0x02U) | CTYP_AUDIO_DECODER)
#define CID_COMP_ADECMPEG1      (TAG(0x03U) | CTYP_AUDIO_DECODER)
#define CID_COMP_ADECMP3        (TAG(0x04U) | CTYP_AUDIO_DECODER)
#define CID_COMP_ADECAAC        (TAG(0x05U) | CTYP_AUDIO_DECODER)
#define CID_COMP_ADECG723       (TAG(0x06U) | CTYP_AUDIO_DECODER)
#define CID_COMP_ADECG728       (TAG(0x07U) | CTYP_AUDIO_DECODER)
#define CID_COMP_ADECWMA        (TAG(0x08U) | CTYP_AUDIO_DECODER)
#define CID_COMP_ADECTHRU       (TAG(0x09U) | CTYP_AUDIO_DECODER)
#define CID_COMP_ADEC           (TAG(0x0AU) | CTYP_AUDIO_DECODER)
#define CID_COMP_ADECPCM        (TAG(0x0BU) | CTYP_AUDIO_DECODER)
#define CID_COMP_ADECDV         (TAG(0x0CU) | CTYP_AUDIO_DECODER)
#define CID_COMP_ADECDTS        (TAG(0x0DU) | CTYP_AUDIO_DECODER)

#define CID_COMP_ASPLIB         (TAG(0x01U) | CTYP_AUDIO_FILTER)
#define CID_COMP_IIR            (TAG(0x02U) | CTYP_AUDIO_FILTER)
#define CID_COMP_ASPEQ2         (TAG(0x03U) | CTYP_AUDIO_FILTER)
#define CID_COMP_ASPEQ5         (TAG(0x04U) | CTYP_AUDIO_FILTER)
#define CID_COMP_ASPBASSREDIR   (TAG(0x05U) | CTYP_AUDIO_FILTER)
#define CID_COMP_ASPLAT2        (TAG(0x06U) | CTYP_AUDIO_FILTER)
#define CID_COMP_ASPPLUGIN      (TAG(0x07U) | CTYP_AUDIO_FILTER)
#define CID_COMP_AMIXDTV        (TAG(0x08U) | CTYP_AUDIO_FILTER)
#define CID_COMP_AMIXSIMPLE     (TAG(0x09U) | CTYP_AUDIO_FILTER)
#define CID_COMP_AMIXSTB        (TAG(0x0AU) | CTYP_AUDIO_FILTER)
#define CID_COMP_ASPEQ          (TAG(0x0BU) | CTYP_AUDIO_FILTER)
#define CID_COMP_ATESTSIG       (TAG(0x0CU) | CTYP_AUDIO_FILTER)
#define CID_COMP_APROC          (TAG(0x0DU) | CTYP_AUDIO_FILTER)

#define CID_COMP_AUDSUBSYS      (TAG(0x01U) | CTYP_AUDIO_SUBSYS)
#define CID_COMP_AUDSYSSTB      (TAG(0x02U) | CTYP_AUDIO_SUBSYS)
#define CID_COMP_AUDSYSDVD      (TAG(0x03U) | CTYP_AUDIO_SUBSYS)
#define CID_COMP_MMC            (TAG(0x04U) | CTYP_AUDIO_SUBSYS)
#define CID_COMP_COMP_MMC       CID_COMP_MMC	/* legacy */
#define CID_COMP_ASYSATV        (TAG(0x05U) | CTYP_AUDIO_SUBSYS)

/* -------------------------------------------------------------------------- */
/* Graphics Class Types/Components                                            */
/* -------------------------------------------------------------------------- */
#define CTYP_GRAPHICS_RENDERER  (CID_CLASS_GRAPHICS | CID_TYPE_SINK)

#define CID_COMP_WM             (TAG(0x01U) | CTYP_GRAPHICS_RENDERER)
#define CID_COMP_WIDGET         (TAG(0x02U) | CTYP_GRAPHICS_RENDERER)
#define CID_COMP_OM             (TAG(0x03U) | CTYP_GRAPHICS_RENDERER)
#define CID_COMP_HTMLRENDER     (TAG(0x04U) | CTYP_GRAPHICS_RENDERER)
#define CID_COMP_VRENDEIA708    (TAG(0x05U) | CTYP_GRAPHICS_RENDERER)
#define CID_COMP_VRENDEIA608    (TAG(0x06U) | CTYP_GRAPHICS_RENDERER)

#define CTYP_GRAPHICS_DRAW      (CID_CLASS_GRAPHICS | CID_TYPE_NONE)

#define CID_COMP_DRAW           (TAG(0x10U) | CTYP_GRAPHICS_DRAW)
#define CID_COMP_DRAW_UT        (TAG(0x11U) | CTYP_GRAPHICS_DRAW)
#define CID_COMP_DRAW_DE        (TAG(0x12U) | CTYP_GRAPHICS_DRAW)
#define CID_COMP_DRAW_REF       (TAG(0x13U) | CTYP_GRAPHICS_DRAW)
#define CID_COMP_DRAW_TMH       (TAG(0x14U) | CTYP_GRAPHICS_DRAW)
#define CID_COMP_DRAW_TMT       (TAG(0x15U) | CTYP_GRAPHICS_DRAW)
#define CID_COMP_DRAW_TMTH      (TAG(0x16U) | CTYP_GRAPHICS_DRAW)

#define CID_COMP_3D             (TAG(0x30U) | CTYP_GRAPHICS_DRAW)
#define CID_COMP_JAWT           (TAG(0x31U) | CTYP_GRAPHICS_DRAW)
#define CID_COMP_JINPUT         (TAG(0x32U) | CTYP_GRAPHICS_DRAW)
#define CID_COMP_LWM            (TAG(0x33U) | CTYP_GRAPHICS_DRAW)
#define CID_COMP_2D             (TAG(0x34U) | CTYP_GRAPHICS_DRAW)

/* -------------------------------------------------------------------------- */
/* Bus Class Types/Components (busses connect hardware components together)   */
/* -------------------------------------------------------------------------- */
#define CTYP_BUS_NOTYPE         (CID_CLASS_BUS | CID_TYPE_NONE)

#define CID_COMP_XIO            (TAG(0x01U) | CTYP_BUS_NOTYPE)
#define CID_COMP_IIC            (TAG(0x02U) | CTYP_BUS_NOTYPE)
#define CID_COMP_PCI            (TAG(0x03U) | CTYP_BUS_NOTYPE)
#define CID_COMP_P1394          (TAG(0x04U) | CTYP_BUS_NOTYPE)
#define CID_COMP_ENET           (TAG(0x05U) | CTYP_BUS_NOTYPE)
#define CID_COMP_ATA            (TAG(0x06U) | CTYP_BUS_NOTYPE)
#define CID_COMP_CAN            (TAG(0x07U) | CTYP_BUS_NOTYPE)
#define CID_COMP_UCGDMA         (TAG(0x08U) | CTYP_BUS_NOTYPE)
#define CID_COMP_I2S            (TAG(0x09U) | CTYP_BUS_NOTYPE)
#define CID_COMP_SPI            (TAG(0x0AU) | CTYP_BUS_NOTYPE)
#define CID_COMP_PCM            (TAG(0x0BU) | CTYP_BUS_NOTYPE)
#define CID_COMP_L3             (TAG(0x0CU) | CTYP_BUS_NOTYPE)
#define CID_COMP_UDSPFL         (TAG(0x0DU) | CTYP_BUS_NOTYPE)
#define CID_COMP_UDSRSL         (TAG(0x0EU) | CTYP_BUS_NOTYPE)
#define CID_COMP_UDSMSBOT       (TAG(0x0FU) | CTYP_BUS_NOTYPE)
#define CID_COMP_UDSMSCBI       (TAG(0x10U) | CTYP_BUS_NOTYPE)
#define CID_COMP_UDSAUDIO       (TAG(0x11U) | CTYP_BUS_NOTYPE)
#define CID_COMP_UDSHID         (TAG(0x12U) | CTYP_BUS_NOTYPE)
#define CID_COMP_UDSCDC         (TAG(0x13U) | CTYP_BUS_NOTYPE)
#define CID_COMP_UDSPRINTER     (TAG(0x14U) | CTYP_BUS_NOTYPE)
#define CID_COMP_UDSSCSI        (TAG(0x15U) | CTYP_BUS_NOTYPE)
#define CID_COMP_UDSMODEM       (TAG(0x16U) | CTYP_BUS_NOTYPE)
#define CID_COMP_UDSETHERNET    (TAG(0x17U) | CTYP_BUS_NOTYPE)
#define CID_COMP_UHSPFL         (TAG(0x18U) | CTYP_BUS_NOTYPE)
#define CID_COMP_UHSMS          (TAG(0x19U) | CTYP_BUS_NOTYPE)
#define CID_COMP_UHSAUDIO       (TAG(0x1AU) | CTYP_BUS_NOTYPE)
#define CID_COMP_UHSSCSI        (TAG(0x1BU) | CTYP_BUS_NOTYPE)

/* -------------------------------------------------------------------------- */
/* Infrastructure Class Types/Components                                      */
/* -------------------------------------------------------------------------- */
#define CTYP_INFRASTR_NOTYPE    (CID_CLASS_INFRASTR | CID_TYPE_NONE)
#define CTYP_INFRASTR_DATABASE  (CID_CLASS_INFRASTR | CID_TYPE_DATABASE)

#define CID_COMP_OSAL           (TAG(0x01U) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_MML            (TAG(0x02U) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_TSSA_DEFAULTS  (TAG(0x03U) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_RPC            (TAG(0x04U) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_THI            (TAG(0x05U) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_REGISTRY       (TAG(0x06U) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_TMMAN          (TAG(0x07U) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_LDT            (TAG(0x08U) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_CPUCONN        (TAG(0x09U) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_COMMQUE        (TAG(0x0AU) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_BSLMGR         (TAG(0x0BU) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_CR             (TAG(0x0CU) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_NODE           (TAG(0x0DU) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_COM            (TAG(0x0EU) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_UTIL           (TAG(0x0FU) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_SGLIST         (TAG(0x10U) | CTYP_INFRASTR_NOTYPE)
#define CID_COMP_ARITH          (TAG(0x11U) | CTYP_INFRASTR_NOTYPE)

#define CID_COMP_MULTIFS        (TAG(0x01U) | CTYP_INFRASTR_DATABASE)
#define CID_COMP_SFS            (TAG(0x02U) | CTYP_INFRASTR_DATABASE)

/* -------------------------------------------------------------------------- */
/* Component Standard Error/Progress Status definitions (bits 11:0, 12 bits)  */
/* NOTE: These status codes are ORed with the component identifier to create  */
/*       component unique 32 bit status values.  The component status values  */
/*       should be defined in the header files where the APIs are defined.    */
/* -------------------------------------------------------------------------- */
#define CID_ERR_BITMASK                 0xFFFU
#define CID_ERR_BITSHIFT                0
#define CID_GET_ERROR(compId)   ((compId & CID_ERR_BITMASK) >> CID_ERR_BITSHIFT)

#define TM_ERR_COMPATIBILITY            0x001U	/* SW Interface compatibility   */
#define TM_ERR_MAJOR_VERSION            0x002U	/* SW Major Version error       */
#define TM_ERR_COMP_VERSION             0x003U	/* SW component version error   */
#define TM_ERR_BAD_MODULE_ID            0x004U	/* SW - HW module ID error      */
#define TM_ERR_BAD_UNIT_NUMBER          0x005U	/* Invalid device unit number   */
#define TM_ERR_BAD_INSTANCE             0x006U	/* Bad input instance value     */
#define TM_ERR_BAD_HANDLE               0x007U	/* Bad input handle             */
#define TM_ERR_BAD_INDEX                0x008U	/* Bad input index              */
#define TM_ERR_BAD_PARAMETER            0x009U	/* Invalid input parameter      */
#define TM_ERR_NO_INSTANCES             0x00AU	/* No instances available       */
#define TM_ERR_NO_COMPONENT             0x00BU	/* Component is not present     */
#define TM_ERR_NO_RESOURCES             0x00CU	/* Resource is not available    */
#define TM_ERR_INSTANCE_IN_USE          0x00DU	/* Instance is already in use   */
#define TM_ERR_RESOURCE_OWNED           0x00EU	/* Resource is already in use   */
#define TM_ERR_RESOURCE_NOT_OWNED       0x00FU	/* Caller does not own resource */
#define TM_ERR_INCONSISTENT_PARAMS      0x010U	/* Inconsistent input params    */
#define TM_ERR_NOT_INITIALIZED          0x011U	/* Component is not initialized */
#define TM_ERR_NOT_ENABLED              0x012U	/* Component is not enabled     */
#define TM_ERR_NOT_SUPPORTED            0x013U	/* Function is not supported    */
#define TM_ERR_INIT_FAILED              0x014U	/* Initialization failed        */
#define TM_ERR_BUSY                     0x015U	/* Component is busy            */
#define TM_ERR_NOT_BUSY                 0x016U	/* Component is not busy        */
#define TM_ERR_READ                     0x017U	/* Read error                   */
#define TM_ERR_WRITE                    0x018U	/* Write error                  */
#define TM_ERR_ERASE                    0x019U	/* Erase error                  */
#define TM_ERR_LOCK                     0x01AU	/* Lock error                   */
#define TM_ERR_UNLOCK                   0x01BU	/* Unlock error                 */
#define TM_ERR_OUT_OF_MEMORY            0x01CU	/* Memory allocation failed     */
#define TM_ERR_BAD_VIRT_ADDRESS         0x01DU	/* Bad virtual address          */
#define TM_ERR_BAD_PHYS_ADDRESS         0x01EU	/* Bad physical address         */
#define TM_ERR_TIMEOUT                  0x01FU	/* Timeout error                */
#define TM_ERR_OVERFLOW                 0x020U	/* Data overflow/overrun error  */
#define TM_ERR_FULL                     0x021U	/* Queue (etc.) is full         */
#define TM_ERR_EMPTY                    0x022U	/* Queue (etc.) is empty        */
#define TM_ERR_NOT_STARTED              0x023U	/* Streaming function failed    */
#define TM_ERR_ALREADY_STARTED          0x024U	/* Start function failed        */
#define TM_ERR_NOT_STOPPED              0x025U	/* Non-streaming function failed */
#define TM_ERR_ALREADY_STOPPED          0x026U	/* Stop function failed         */
#define TM_ERR_ALREADY_SETUP            0x027U	/* Setup function failed        */
#define TM_ERR_NULL_PARAMETER           0x028U	/* Null input parameter         */
#define TM_ERR_NULL_DATAINFUNC          0x029U	/* Null data input function     */
#define TM_ERR_NULL_DATAOUTFUNC         0x02AU	/* Null data output function    */
#define TM_ERR_NULL_CONTROLFUNC         0x02BU	/* Null control function        */
#define TM_ERR_NULL_COMPLETIONFUNC      0x02CU	/* Null completion function     */
#define TM_ERR_NULL_PROGRESSFUNC        0x02DU	/* Null progress function       */
#define TM_ERR_NULL_ERRORFUNC           0x02EU	/* Null error handler function  */
#define TM_ERR_NULL_MEMALLOCFUNC        0x02FU	/* Null memory alloc function   */
#define TM_ERR_NULL_MEMFREEFUNC         0x030U	/* Null memory free  function   */
#define TM_ERR_NULL_CONFIGFUNC          0x031U	/* Null configuration function  */
#define TM_ERR_NULL_PARENT              0x032U	/* Null parent data             */
#define TM_ERR_NULL_IODESC              0x033U	/* Null in/out descriptor       */
#define TM_ERR_NULL_CTRLDESC            0x034U	/* Null control descriptor      */
#define TM_ERR_UNSUPPORTED_DATACLASS    0x035U	/* Unsupported data class       */
#define TM_ERR_UNSUPPORTED_DATATYPE     0x036U	/* Unsupported data type        */
#define TM_ERR_UNSUPPORTED_DATASUBTYPE  0x037U	/* Unsupported data subtype     */
#define TM_ERR_FORMAT                   0x038U	/* Invalid/unsupported format   */
#define TM_ERR_INPUT_DESC_FLAGS         0x039U	/* Bad input  descriptor flags  */
#define TM_ERR_OUTPUT_DESC_FLAGS        0x03AU	/* Bad output descriptor flags  */
#define TM_ERR_CAP_REQUIRED             0x03BU	/* Capabilities required ???    */
#define TM_ERR_BAD_TMALFUNC_TABLE       0x03CU	/* Bad TMAL function table      */
#define TM_ERR_INVALID_CHANNEL_ID       0x03DU	/* Invalid channel identifier   */
#define TM_ERR_INVALID_COMMAND          0x03EU	/* Invalid command/request      */
#define TM_ERR_STREAM_MODE_CONFUSION    0x03FU	/* Stream mode config conflict  */
#define TM_ERR_UNDERRUN                 0x040U	/* Data underflow/underrun      */
#define TM_ERR_EMPTY_PACKET_RECVD       0x041U	/* Empty data packet received   */
#define TM_ERR_OTHER_DATAINOUT_ERR      0x042U	/* Other data input/output err  */
#define TM_ERR_STOP_REQUESTED           0x043U	/* Stop in progress             */
#define TM_ERR_ASSERTION                0x049U	/* Assertion failure            */
#define TM_ERR_HIGHWAY_BANDWIDTH        0x04AU	/* Highway bandwidth bus error  */
#define TM_ERR_HW_RESET_FAILED          0x04BU	/* Hardware reset failed        */
#define TM_ERR_BAD_FLAGS                0x04DU	/* Bad flags                    */
#define TM_ERR_BAD_PRIORITY             0x04EU	/* Bad priority                 */
#define TM_ERR_BAD_REFERENCE_COUNT      0x04FU	/* Bad reference count          */
#define TM_ERR_BAD_SETUP                0x050U	/* Bad setup                    */
#define TM_ERR_BAD_STACK_SIZE           0x051U	/* Bad stack size               */
#define TM_ERR_BAD_TEE                  0x052U	/* Bad tee                      */
#define TM_ERR_IN_PLACE                 0x053U	/* In place                     */
#define TM_ERR_NOT_CACHE_ALIGNED        0x054U	/* Not cache aligned            */
#define TM_ERR_NO_ROOT_TEE              0x055U	/* No root tee                  */
#define TM_ERR_NO_TEE_ALLOWED           0x056U	/* No tee allowed               */
#define TM_ERR_NO_TEE_EMPTY_PACKET      0x057U	/* No tee empty packet          */
#define TM_ERR_NULL_PACKET              0x059U	/* Null packet                  */
#define TM_ERR_FORMAT_FREED             0x05AU	/* Format freed                 */
#define TM_ERR_FORMAT_INTERNAL          0x05BU	/* Format internal              */
#define TM_ERR_BAD_FORMAT               0x05CU	/* Bad format                   */
#define TM_ERR_FORMAT_NEGOTIATE_DATACLASS 0x05DU	/* Format negotiate class     */
#define TM_ERR_FORMAT_NEGOTIATE_DATATYPE 0x05EU	/* Format negotiate type       */
#define TM_ERR_FORMAT_NEGOTIATE_DATASUBTYPE 0x05FU	/* Format negotiate subtype */
#define TM_ERR_FORMAT_NEGOTIATE_DESCRIPTION 0x060U	/* Format negotiate desc    */
#define TM_ERR_NULL_FORMAT              0x061U	/* Null format                  */
#define TM_ERR_FORMAT_REFERENCE_COUNT   0x062U	/* Format reference count       */
#define TM_ERR_FORMAT_NOT_UNIQUE        0x063U	/* Format not unique            */
#define TM_NEW_FORMAT                   0x064U	/* New format (not an error)    */
#define TM_ERR_FORMAT_NEGOTIATE_EXTENSION 0x065U	/* Format negotiate extension */
#define TM_ERR_INVALID_STATE            0x066U	/* Invalid state for function   */
#define TM_ERR_NULL_CONNECTION          0x067U	/* No connection to this pin    */
#define TM_ERR_OPERATION_NOT_PERMITTED  0x068U	/* corresponds to posix EPERM   */
#define TM_ERR_NOT_CLOCKED              0x069U	/* Power down - clocked off     */


#define PH_ERR_COMPATIBILITY            0x001U	/* SW Interface compatibility   */
#define PH_ERR_MAJOR_VERSION            0x002U	/* SW Major Version error       */
#define PH_ERR_COMP_VERSION             0x003U	/* SW component version error   */
#define PH_ERR_BAD_MODULE_ID            0x004U	/* SW - HW module ID error      */
#define PH_ERR_BAD_UNIT_NUMBER          0x005U	/* Invalid device unit number   */
#define PH_ERR_BAD_INSTANCE             0x006U	/* Bad input instance value     */
#define PH_ERR_BAD_HANDLE               0x007U	/* Bad input handle             */
#define PH_ERR_BAD_INDEX                0x008U	/* Bad input index              */
#define PH_ERR_BAD_PARAMETER            0x009U	/* Invalid input parameter      */
#define PH_ERR_NO_INSTANCES             0x00AU	/* No instances available       */
#define PH_ERR_NO_COMPONENT             0x00BU	/* Component is not present     */
#define PH_ERR_NO_RESOURCES             0x00CU	/* Resource is not available    */
#define PH_ERR_INSTANCE_IN_USE          0x00DU	/* Instance is already in use   */
#define PH_ERR_RESOURCE_OWNED           0x00EU	/* Resource is already in use   */
#define PH_ERR_RESOURCE_NOT_OWNED       0x00FU	/* Caller does not own resource */
#define PH_ERR_INCONSISTENT_PARAMS      0x010U	/* Inconsistent input params    */
#define PH_ERR_NOT_INITIALIZED          0x011U	/* Component is not initialized */
#define PH_ERR_NOT_ENABLED              0x012U	/* Component is not enabled     */
#define PH_ERR_NOT_SUPPORTED            0x013U	/* Function is not supported    */
#define PH_ERR_INIT_FAILED              0x014U	/* Initialization failed        */
#define PH_ERR_BUSY                     0x015U	/* Component is busy            */
#define PH_ERR_NOT_BUSY                 0x016U	/* Component is not busy        */
#define PH_ERR_READ                     0x017U	/* Read error                   */
#define PH_ERR_WRITE                    0x018U	/* Write error                  */
#define PH_ERR_ERASE                    0x019U	/* Erase error                  */
#define PH_ERR_LOCK                     0x01AU	/* Lock error                   */
#define PH_ERR_UNLOCK                   0x01BU	/* Unlock error                 */
#define PH_ERR_OUT_OF_MEMORY            0x01CU	/* Memory allocation failed     */
#define PH_ERR_BAD_VIRT_ADDRESS         0x01DU	/* Bad virtual address          */
#define PH_ERR_BAD_PHYS_ADDRESS         0x01EU	/* Bad physical address         */
#define PH_ERR_TIMEOUT                  0x01FU	/* Timeout error                */
#define PH_ERR_OVERFLOW                 0x020U	/* Data overflow/overrun error  */
#define PH_ERR_FULL                     0x021U	/* Queue (etc.) is full         */
#define PH_ERR_EMPTY                    0x022U	/* Queue (etc.) is empty        */
#define PH_ERR_NOT_STARTED              0x023U	/* Streaming function failed    */
#define PH_ERR_ALREADY_STARTED          0x024U	/* Start function failed        */
#define PH_ERR_NOT_STOPPED              0x025U	/* Non-streaming function failed */
#define PH_ERR_ALREADY_STOPPED          0x026U	/* Stop function failed         */
#define PH_ERR_ALREADY_SETUP            0x027U	/* Setup function failed        */
#define PH_ERR_NULL_PARAMETER           0x028U	/* Null input parameter         */
#define PH_ERR_NULL_DATAINFUNC          0x029U	/* Null data input function     */
#define PH_ERR_NULL_DATAOUTFUNC         0x02AU	/* Null data output function    */
#define PH_ERR_NULL_CONTROLFUNC         0x02BU	/* Null control function        */
#define PH_ERR_NULL_COMPLETIONFUNC      0x02CU	/* Null completion function     */
#define PH_ERR_NULL_PROGRESSFUNC        0x02DU	/* Null progress function       */
#define PH_ERR_NULL_ERRORFUNC           0x02EU	/* Null error handler function  */
#define PH_ERR_NULL_MEMALLOCFUNC        0x02FU	/* Null memory alloc function   */
#define PH_ERR_NULL_MEMFREEFUNC         0x030U	/* Null memory free  function   */
#define PH_ERR_NULL_CONFIGFUNC          0x031U	/* Null configuration function  */
#define PH_ERR_NULL_PARENT              0x032U	/* Null parent data             */
#define PH_ERR_NULL_IODESC              0x033U	/* Null in/out descriptor       */
#define PH_ERR_NULL_CTRLDESC            0x034U	/* Null control descriptor      */
#define PH_ERR_UNSUPPORTED_DATACLASS    0x035U	/* Unsupported data class       */
#define PH_ERR_UNSUPPORTED_DATATYPE     0x036U	/* Unsupported data type        */
#define PH_ERR_UNSUPPORTED_DATASUBTYPE  0x037U	/* Unsupported data subtype     */
#define PH_ERR_FORMAT                   0x038U	/* Invalid/unsupported format   */
#define PH_ERR_INPUT_DESC_FLAGS         0x039U	/* Bad input  descriptor flags  */
#define PH_ERR_OUTPUT_DESC_FLAGS        0x03AU	/* Bad output descriptor flags  */
#define PH_ERR_CAP_REQUIRED             0x03BU	/* Capabilities required ???    */
#define PH_ERR_BAD_TMALFUNC_TABLE       0x03CU	/* Bad TMAL function table      */
#define PH_ERR_INVALID_CHANNEL_ID       0x03DU	/* Invalid channel identifier   */
#define PH_ERR_INVALID_COMMAND          0x03EU	/* Invalid command/request      */
#define PH_ERR_STREAM_MODE_CONFUSION    0x03FU	/* Stream mode config conflict  */
#define PH_ERR_UNDERRUN                 0x040U	/* Data underflow/underrun      */
#define PH_ERR_EMPTY_PACKET_RECVD       0x041U	/* Empty data packet received   */
#define PH_ERR_OTHER_DATAINOUT_ERR      0x042U	/* Other data input/output err  */
#define PH_ERR_STOP_REQUESTED           0x043U	/* Stop in progress             */
#define PH_ERR_ASSERTION                0x049U	/* Assertion failure            */
#define PH_ERR_HIGHWAY_BANDWIDTH        0x04AU	/* Highway bandwidth bus error  */
#define PH_ERR_HW_RESET_FAILED          0x04BU	/* Hardware reset failed        */
#define PH_ERR_BAD_FLAGS                0x04DU	/* Bad flags                    */
#define PH_ERR_BAD_PRIORITY             0x04EU	/* Bad priority                 */
#define PH_ERR_BAD_REFERENCE_COUNT      0x04FU	/* Bad reference count          */
#define PH_ERR_BAD_SETUP                0x050U	/* Bad setup                    */
#define PH_ERR_BAD_STACK_SIZE           0x051U	/* Bad stack size               */
#define PH_ERR_BAD_TEE                  0x052U	/* Bad tee                      */
#define PH_ERR_IN_PLACE                 0x053U	/* In place                     */
#define PH_ERR_NOT_CACHE_ALIGNED        0x054U	/* Not cache aligned            */
#define PH_ERR_NO_ROOT_TEE              0x055U	/* No root tee                  */
#define PH_ERR_NO_TEE_ALLOWED           0x056U	/* No tee allowed               */
#define PH_ERR_NO_TEE_EMPTY_PACKET      0x057U	/* No tee empty packet          */
#define PH_ERR_NULL_PACKET              0x059U	/* Null packet                  */
#define PH_ERR_FORMAT_FREED             0x05AU	/* Format freed                 */
#define PH_ERR_FORMAT_INTERNAL          0x05BU	/* Format internal              */
#define PH_ERR_BAD_FORMAT               0x05CU	/* Bad format                   */
#define PH_ERR_FORMAT_NEGOTIATE_DATACLASS 0x05DU	/* Format negotiate class     */
#define PH_ERR_FORMAT_NEGOTIATE_DATATYPE 0x05EU	/* Format negotiate type       */
#define PH_ERR_FORMAT_NEGOTIATE_DATASUBTYPE 0x05FU	/* Format negotiate subtype */
#define PH_ERR_FORMAT_NEGOTIATE_DESCRIPTION 0x060U	/* Format negotiate desc    */
#define PH_ERR_NULL_FORMAT              0x061U	/* Null format                  */
#define PH_ERR_FORMAT_REFERENCE_COUNT   0x062U	/* Format reference count       */
#define PH_ERR_FORMAT_NOT_UNIQUE        0x063U	/* Format not unique            */
#define PH_NEW_FORMAT                   0x064U	/* New format (not an error)    */
#define PH_ERR_FORMAT_NEGOTIATE_EXTENSION 0x065U	/* Format negotiate extension */
#define PH_ERR_INVALID_STATE            0x066U	/* Invalid state for function   */
#define PH_ERR_NULL_CONNECTION          0x067U	/* No connection to this pin    */
#define PH_ERR_OPERATION_NOT_PERMITTED  0x068U	/* corresponds to posix EPERM   */
#define PH_ERR_NOT_CLOCKED              0x069U	/* Power down - clocked off     */

/* Add new standard error/progress status codes here                          */

#define TM_ERR_COMP_UNIQUE_START    0x800U	/* 0x800-0xBFF: Component unique    */
#define PH_ERR_COMP_UNIQUE_START    0x800U	/* 0x800-0xBFF: Component unique    */
#define TM_ERR_CUSTOMER_START       0xC00U	/* 0xC00-0xDFF: Customer defined    */
#define PH_ERR_CUSTOMER_START       0xC00U	/* 0xC00-0xDFF: Customer defined    */

/* Legacy and withdrawn error codes */
#define TM_ERR_FORMAT_NEGOTIATE_SUBCLASS TM_ERR_FORMAT_NEGOTIATE_DATACLASS
#define TM_ERR_NEW_FORMAT                TM_NEW_FORMAT
#define TM_ERR_PAUSE_PIN_REQUESTED       TM_ERR_STOP_REQUESTED
#define TM_ERR_PIN_ALREADY_STARTED       TM_ERR_ALREADY_STARTED
#define TM_ERR_PIN_ALREADY_STOPPED       TM_ERR_ALREADY_STOPPED
#define TM_ERR_PIN_NOT_STARTED           TM_ERR_NOT_STARTED
#define TM_ERR_PIN_NOT_STOPPED           TM_ERR_NOT_STOPPED
#define TM_ERR_PIN_PAUSED                TM_ERR_NOT_STARTED

/* -------------------------------------------------------------------------- */
/* Standard assert error code start offset                                    */
/* NOTE: These ranges are FOR LEGACY CODE ONLY and must not be used in new    */
/*  components                                                                */
/* -------------------------------------------------------------------------- */
#define TM_ERR_ASSERT_START         0xE00U	/* 0xE00-0xEFF: Assert failures     */
#define TM_ERR_ASSERT_LAST          0xEFFU	/* Last assert error range value    */
#define CID_IS_ASSERT_ERROR(compId) ((CID_GET_ERROR(compId) >= TM_ERR_ASSERT_START) && (CID_GET_ERROR(compId) <= TM_ERR_ASSERT_LAST))

/* -------------------------------------------------------------------------- */
/* Standard fatal error code start offset                                     */
/* NOTE: These ranges are FOR LEGACY CODE ONLY and must not be used in new    */
/*  components                                                                */
/* -------------------------------------------------------------------------- */

#define TM_ERR_FATAL_START          0xF00U	/* 0xF00-0xFFF: Fatal failures      */
#define TM_ERR_FATAL_LAST           0xFFFU	/* Last fatal error range value     */
#define CID_IS_FATAL_ERROR(compId)  ((CID_GET_ERROR(compId) >= TM_ERR_FATAL_START) && (CID_GET_ERROR(compId) <= TM_ERR_FATAL_LAST))

#ifdef __cplusplus
}
#endif
#endif				/* TMNXCOMPID_H ----------------- */
