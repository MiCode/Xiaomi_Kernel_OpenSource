/**
 * Copyright (C) 2009 NXP N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of NXP N.V. and is confidential in nature. Under no circumstances
 * is this software to be  exposed to or placed under an Open Source License of
 * any type without the expressed written permission of NXP N.V.
 *
 * \file          tmbslHdmiTx_funcMapping.h
 *
 * \version       $Revision: 2 $
 *
*/



#ifndef TMDLHDMITXTDA9989_CFG_H
#define TMDLHDMITXTDA9989_CFG_H

#include "tmbslTDA9989_Functions.h"

#ifdef __cplusplus
extern "C" {
#endif

#define tmbslHdmiTxInit                             tmbslTDA9989Init
#define tmbslHdmiTxEdidRequestBlockData             tmbslTDA9989EdidRequestBlockData
#define tmbslHdmiTxVideoOutSetConfig                tmbslTDA9989VideoOutSetConfig
#define tmbslHdmiTxAudioInResetCts                  tmbslTDA9989AudioInResetCts
#define tmbslHdmiTxAudioInSetConfig                 tmbslTDA9989AudioInSetConfig
#define tmbslHdmiTxAudioInSetCts                    tmbslTDA9989AudioInSetCts
#define tmbslHdmiTxAudioOutSetChanStatus            tmbslTDA9989AudioOutSetChanStatus
#define tmbslHdmiTxAudioOutSetChanStatusMapping     tmbslTDA9989AudioOutSetChanStatusMapping
#define tmbslHdmiTxAudioOutSetMute                  tmbslTDA9989AudioOutSetMute
#define tmbslHdmiTxDeinit                           tmbslTDA9989Deinit
#define tmbslHdmiTxEdidGetAudioCapabilities         tmbslTDA9989EdidGetAudioCapabilities
#define tmbslHdmiTxEdidGetBlockCount                tmbslTDA9989EdidGetBlockCount
#define tmbslHdmiTxEdidGetStatus                    tmbslTDA9989EdidGetStatus
#define tmbslHdmiTxEdidGetSinkType                  tmbslTDA9989EdidGetSinkType
#define tmbslHdmiTxEdidGetSourceAddress             tmbslTDA9989EdidGetSourceAddress
#define tmbslHdmiTxEdidGetVideoCapabilities         tmbslTDA9989EdidGetVideoCapabilities
#define tmbslHdmiTxEdidGetVideoPreferred            tmbslTDA9989EdidGetVideoPreferred
#define tmbslHdmiTxHdcpCheck                        tmbslTDA9989HdcpCheck
#define tmbslHdmiTxHdcpConfigure                    tmbslTDA9989HdcpConfigure
#define tmbslHdmiTxHdcpDownloadKeys                 tmbslTDA9989HdcpDownloadKeys
#define tmbslHdmiTxHdcpEncryptionOn                 tmbslTDA9989HdcpEncryptionOn
#define tmbslHdmiTxHdcpGetOtp                       tmbslTDA9989HdcpGetOtp
#define tmbslHdmiTxHdcpGetT0FailState               tmbslTDA9989HdcpGetT0FailState
#define tmbslHdmiTxHdcpHandleBCAPS                  tmbslTDA9989HdcpHandleBCAPS
#define tmbslHdmiTxHdcpHandleBKSV                   tmbslTDA9989HdcpHandleBKSV
#define tmbslHdmiTxHdcpHandleBKSVResult             tmbslTDA9989HdcpHandleBKSVResult
#define tmbslHdmiTxHdcpHandleBSTATUS                tmbslTDA9989HdcpHandleBSTATUS
#define tmbslHdmiTxHdcpHandleENCRYPT                tmbslTDA9989HdcpHandleENCRYPT
#define tmbslHdmiTxHdcpHandlePJ                     tmbslTDA9989HdcpHandlePJ
#define tmbslHdmiTxHdcpHandleSHA_1                  tmbslTDA9989HdcpHandleSHA_1
#define tmbslHdmiTxHdcpHandleSHA_1Result            tmbslTDA9989HdcpHandleSHA_1Result
#define tmbslHdmiTxHdcpHandleT0                     tmbslTDA9989HdcpHandleT0
#define tmbslHdmiTxHdcpInit                         tmbslTDA9989HdcpInit
#define tmbslHdmiTxHdcpRun                          tmbslTDA9989HdcpRun
#define tmbslHdmiTxHdcpStop                         tmbslTDA9989HdcpStop
#define tmbslHdmiTxHotPlugGetStatus                 tmbslTDA9989HotPlugGetStatus
#define tmbslHdmiTxRxSenseGetStatus                 tmbslTDA9989RxSenseGetStatus
#define tmbslHdmiTxHwGetRegisters                   tmbslTDA9989HwGetRegisters
#define tmbslHdmiTxHwGetVersion                     tmbslTDA9989HwGetVersion
#define tmbslHdmiTxHwGetCapabilities                tmbslTDA9989HwGetCapabilities
#define tmbslHdmiTxHwHandleInterrupt                tmbslTDA9989HwHandleInterrupt
#define tmbslHdmiTxHwSetRegisters                   tmbslTDA9989HwSetRegisters
#define tmbslHdmiTxHwStartup                        tmbslTDA9989HwStartup
#define tmbslHdmiTxMatrixSetCoeffs                  tmbslTDA9989MatrixSetCoeffs
#define tmbslHdmiTxMatrixSetConversion              tmbslTDA9989MatrixSetConversion
#define tmbslHdmiTxMatrixSetInputOffset             tmbslTDA9989MatrixSetInputOffset
#define tmbslHdmiTxMatrixSetMode                    tmbslTDA9989MatrixSetMode
#define tmbslHdmiTxMatrixSetOutputOffset            tmbslTDA9989MatrixSetOutputOffset
#define tmbslHdmiTxPktSetAclkRecovery               tmbslTDA9989PktSetAclkRecovery
#define tmbslHdmiTxPktSetAcp                        tmbslTDA9989PktSetAcp
#define tmbslHdmiTxPktSetAudioInfoframe             tmbslTDA9989PktSetAudioInfoframe
#define tmbslHdmiTxPktSetGeneralCntrl               tmbslTDA9989PktSetGeneralCntrl
#define tmbslHdmiTxPktSetIsrc1                      tmbslTDA9989PktSetIsrc1
#define tmbslHdmiTxPktSetIsrc2                      tmbslTDA9989PktSetIsrc2
#define tmbslHdmiTxPktSetMpegInfoframe              tmbslTDA9989PktSetMpegInfoframe
#define tmbslHdmiTxPktSetNullInsert                 tmbslTDA9989PktSetNullInsert
#define tmbslHdmiTxPktSetNullSingle                 tmbslTDA9989PktSetNullSingle
#define tmbslHdmiTxPktSetSpdInfoframe               tmbslTDA9989PktSetSpdInfoframe
#define tmbslHdmiTxPktSetVideoInfoframe             tmbslTDA9989PktSetVideoInfoframe
#define tmbslHdmiTxPktSetVsInfoframe                tmbslTDA9989PktSetVsInfoframe
#define tmbslHdmiTxPktSetRawVideoInfoframe          tmbslTDA9989PktSetRawVideoInfoframe
#define tmbslHdmiTxPowerGetState                    tmbslTDA9989PowerGetState
#define tmbslHdmiTxPowerSetState                    tmbslTDA9989PowerSetState
#define tmbslHdmiTxReset                            tmbslTDA9989Reset
#define tmbslHdmiTxScalerGet                        tmbslTDA9989ScalerGet
#define tmbslHdmiTxScalerGetMode                    tmbslTDA9989ScalerGetMode
#define tmbslHdmiTxScalerInDisable                  tmbslTDA9989ScalerInDisable
#define tmbslHdmiTxScalerSetCoeffs                  tmbslTDA9989ScalerSetCoeffs
#define tmbslHdmiTxScalerSetFieldOrder              tmbslTDA9989ScalerSetFieldOrder
#define tmbslHdmiTxScalerSetFine                    tmbslTDA9989ScalerSetFine
#define tmbslHdmiTxScalerSetPhase                   tmbslTDA9989ScalerSetPhase
#define tmbslHdmiTxScalerSetLatency                 tmbslTDA9989ScalerSetLatency
#define tmbslHdmiTxScalerSetSync                    tmbslTDA9989ScalerSetSync
#define tmbslHdmiTxSwGetVersion                     tmbslTDA9989SwGetVersion
#define tmbslHdmiTxSysTimerWait                     tmbslTDA9989SysTimerWait
#define tmbslHdmiTxTmdsSetOutputs                   tmbslTDA9989TmdsSetOutputs
#define tmbslHdmiTxTmdsSetSerializer                tmbslTDA9989TmdsSetSerializer
#define tmbslHdmiTxTestSetPattern                   tmbslTDA9989TestSetPattern
#define tmbslHdmiTxTestSetMode                      tmbslTDA9989TestSetMode
#define tmbslHdmiTxVideoInSetBlanking               tmbslTDA9989VideoInSetBlanking
#define tmbslHdmiTxVideoInSetConfig                 tmbslTDA9989VideoInSetConfig
#define tmbslHdmiTxVideoInSetFine                   tmbslTDA9989VideoInSetFine
#define tmbslHdmiTxVideoInSetMapping                tmbslTDA9989VideoInSetMapping
#define tmbslHdmiTxSetVideoPortConfig               tmbslTDA9989SetVideoPortConfig
#define tmbslHdmiTxSetAudioPortConfig               tmbslTDA9989SetAudioPortConfig
#define tmbslHdmiTxSetAudioClockPortConfig          tmbslTDA9989SetAudioClockPortConfig
#define tmbslHdmiTxVideoInSetSyncAuto               tmbslTDA9989VideoInSetSyncAuto
#define tmbslHdmiTxVideoInSetSyncManual             tmbslTDA9989VideoInSetSyncManual
#define tmbslHdmiTxVideoOutDisable                  tmbslTDA9989VideoOutDisable
#define tmbslHdmiTxVideoOutSetSync                  tmbslTDA9989VideoOutSetSync
#define tmbslHdmiTxVideoSetInOut                    tmbslTDA9989VideoSetInOut
#define tmbslHdmiTxFlagSwInt                        tmbslTDA9989FlagSwInt
#define tmbslHdmiTxSet5vpower                       tmbslTDA9989Set5vpower
#define tmbslHdmiTxEnableCallback                   tmbslTDA9989EnableCallback
#define tmbslHdmiTxSetColorDepth                    tmbslTDA9989SetColorDepth
#define tmbslHdmiTxSetDefaultPhase                  tmbslTDA9989SetDefaultPhase
#define tmbslHdmiTxPktFillGamut                     tmbslTDA9989PktFillGamut
#define tmbslHdmiTxPktSendGamut                     tmbslTDA9989PktSendGamut
#define tmbslHdmiTxEdidGetMonitorDescriptors        tmbslTDA9989EdidGetMonitorDescriptors
#define tmbslHdmiTxEdidGetDetailedTimingDescriptors tmbslTDA9989EdidGetDetailedTimingDescriptors
#define tmbslHdmiTxEdidGetBasicDisplayParam         tmbslTDA9989EdidGetBasicDisplayParam
#define tmbslHdmiTxHdcpGetSinkCategory              tmbslTDA9989HdcpGetSinkCategory
#define tmbslHdmiTxEdidGetLatencyInfo               tmbslTDA9989EdidGetLatencyInfo
#define tmbslHdmiTxEdidGetExtraVsdbData             tmbslTDA9989EdidGetExtraVsdbData
#ifdef TMFL_TDA19989
#define tmbslHdmiTxHdcpPowerDown                    tmbslTDA9989HdcpPowerDown
#endif

#ifdef __cplusplus
}
#endif
#endif				/* TMDLHDMITXTDA9989_CFG_H */
/*============================================================================*//*                               END OF FILE                                  *//*============================================================================*/
