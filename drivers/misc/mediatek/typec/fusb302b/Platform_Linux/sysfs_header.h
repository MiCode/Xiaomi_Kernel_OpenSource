#ifndef FUSB302_SYFS_HEADER_H
#define FUSB302_SYFS_HEADER_H

#include <linux/types.h>

#define GENERATE_STR(name, val)     [val] = #name

#define LIST_OF_TYPEC_STATES(STATE)\
    STATE(Disabled,                        0),\
    STATE(ErrorRecovery,                   1),\
    STATE(Unattached,                      2),\
    STATE(AttachWaitSink,                  3),\
    STATE(AttachedSink,                    4),\
    STATE(AttachWaitSource,                5),\
    STATE(AttachedSource,                  6),\
    STATE(TrySource,                       7),\
    STATE(TryWaitSink,                     8),\
    STATE(TrySink,                         9),\
    STATE(TryWaitSource,                  10),\
    STATE(AudioAccessory,                 11),\
    STATE(DebugAccessorySource,           12),\
    STATE(AttachWaitAccessory,            13),\
    STATE(PoweredAccessory,               14),\
    STATE(UnsupportedAccessory,           15),\
    STATE(DelayUnattached,                16),\
    STATE(UnattachedSource,               17),\
    STATE(DebugAccessorySink,             18),\
    STATE(AttachWaitDebSink,              19),\
    STATE(AttachedDebSink,                20),\
    STATE(AttachWaitDebSource,            21),\
    STATE(AttachedDebSource,              22),\
    STATE(TryDebSource,                   23),\
    STATE(TryWaitDebSink,                 24),\
    STATE(UnattachedDebSource,            25),\
    STATE(IllegalCable,                   26),\

const static char *TYPEC_STATE_TBL[] = {
        LIST_OF_TYPEC_STATES(GENERATE_STR)
};

const static size_t NUM_TYPEC_STATES =
            sizeof(TYPEC_STATE_TBL)/sizeof(TYPEC_STATE_TBL[0]);

#define LIST_OF_PE_STATES(STATE)\
    STATE(peDisabled,                      0),\
    STATE(peErrorRecovery,                 1),\
    STATE(peSourceHardReset,               2),\
    STATE(peSourceSendHardReset,           3),\
    STATE(peSourceSoftReset,               4),\
    STATE(peSourceSendSoftReset,           5),\
    STATE(peSourceStartup,                 6),\
    STATE(peSourceSendCaps,                7),\
    STATE(peSourceDiscovery,               8),\
    STATE(peSourceDisabled,                9),\
    STATE(peSourceTransitionDefault,      10),\
    STATE(peSourceNegotiateCap,           11),\
    STATE(peSourceCapabilityResponse,     12),\
    STATE(peSourceWaitNewCapabilities,    13),\
    STATE(peSourceTransitionSupply,       14),\
    STATE(peSourceReady,                  15),\
    STATE(peSourceGiveSourceCaps,         16),\
    STATE(peSourceGetSinkCaps,            17),\
    STATE(peSourceSendPing,               18),\
    STATE(peSourceGotoMin,                19),\
    STATE(peSourceGiveSinkCaps,           20),\
    STATE(peSourceGetSourceCaps,          21),\
    STATE(peSourceSendDRSwap,             22),\
    STATE(peSourceEvaluateDRSwap,         23),\
    STATE(peSourceAlertReceived,          24),\
    STATE(peSinkHardReset,                25),\
    STATE(peSinkSendHardReset,            26),\
    STATE(peSinkSoftReset,                27),\
    STATE(peSinkSendSoftReset,            28),\
    STATE(peSinkTransitionDefault,        29),\
    STATE(peSinkStartup,                  30),\
    STATE(peSinkDiscovery,                31),\
    STATE(peSinkWaitCaps,                 32),\
    STATE(peSinkEvaluateCaps,             33),\
    STATE(peSinkSelectCapability,         34),\
    STATE(peSinkTransitionSink,           35),\
    STATE(peSinkReady,                    36),\
    STATE(peSinkGiveSinkCap,              37),\
    STATE(peSinkGetSourceCap,             38),\
    STATE(peSinkGetSinkCap,               39),\
    STATE(peSinkGiveSourceCap,            40),\
    STATE(peSinkSendDRSwap,               41),\
    STATE(peSinkAlertReceived,            42),\
    STATE(peSinkEvaluateDRSwap,           43),\
    STATE(peSourceSendVCONNSwap,          44),\
    STATE(peSourceEvaluateVCONNSwap,      45),\
    STATE(peSinkSendVCONNSwap,            46),\
    STATE(peSinkEvaluateVCONNSwap,        47),\
    STATE(peSourceSendPRSwap,             48),\
    STATE(peSourceEvaluatePRSwap,         49),\
    STATE(peSinkSendPRSwap,               50),\
    STATE(peSinkEvaluatePRSwap,           51),\
    STATE(peGetCountryCodes,              52),\
    STATE(peGiveCountryCodes,             53),\
    STATE(peNotSupported,                 54),\
    STATE(peGetPPSStatus,                 55),\
    STATE(peGivePPSStatus,                56),\
    STATE(peGiveCountryInfo,              57),\
    STATE(peGiveVdm,                      58),\
    STATE(peUfpVdmGetIdentity,            59),\
    STATE(peUfpVdmSendIdentity,           60),\
    STATE(peUfpVdmGetSvids,               61),\
    STATE(peUfpVdmSendSvids,              62),\
    STATE(peUfpVdmGetModes,               63),\
    STATE(peUfpVdmSendModes,              64),\
    STATE(peUfpVdmEvaluateModeEntry,      65),\
    STATE(peUfpVdmModeEntryNak,           66),\
    STATE(peUfpVdmModeEntryAck,           67),\
    STATE(peUfpVdmModeExit,               68),\
    STATE(peUfpVdmModeExitNak,            69),\
    STATE(peUfpVdmModeExitAck,            70),\
    STATE(peUfpVdmAttentionRequest,       71),\
    STATE(peDfpUfpVdmIdentityRequest,     72),\
    STATE(peDfpUfpVdmIdentityAcked,       73),\
    STATE(peDfpUfpVdmIdentityNaked,       74),\
    STATE(peDfpCblVdmIdentityRequest,     75),\
    STATE(peDfpCblVdmIdentityAcked,       76),\
    STATE(peDfpCblVdmIdentityNaked,       77),\
    STATE(peDfpVdmSvidsRequest,           78),\
    STATE(peDfpVdmSvidsAcked,             79),\
    STATE(peDfpVdmSvidsNaked,             80),\
    STATE(peDfpVdmModesRequest,           81),\
    STATE(peDfpVdmModesAcked,             82),\
    STATE(peDfpVdmModesNaked,             83),\
    STATE(peDfpVdmModeEntryRequest,       84),\
    STATE(peDfpVdmModeEntryAcked,         85),\
    STATE(peDfpVdmModeEntryNaked,         86),\
    STATE(peDfpVdmModeExitRequest,        87),\
    STATE(peDfpVdmExitModeAcked,          88),\
    STATE(peSrcVdmIdentityRequest,        89),\
    STATE(peSrcVdmIdentityAcked,          90),\
    STATE(peSrcVdmIdentityNaked,          91),\
    STATE(peDfpVdmAttentionRequest,       92),\
    STATE(peCblReady,                     93),\
    STATE(peCblGetIdentity,               94),\
    STATE(peCblGetIdentityNak,            95),\
    STATE(peCblSendIdentity,              96),\
    STATE(peCblGetSvids,                  97),\
    STATE(peCblGetSvidsNak,               98),\
    STATE(peCblSendSvids,                 99),\
    STATE(peCblGetModes,                 100),\
    STATE(peCblGetModesNak,              101),\
    STATE(peCblSendModes,                102),\
    STATE(peCblEvaluateModeEntry,        103),\
    STATE(peCblModeEntryAck,             104),\
    STATE(peCblModeEntryNak,             105),\
    STATE(peCblModeExit,                 106),\
    STATE(peCblModeExitAck,              107),\
    STATE(peCblModeExitNak,              108),\
    STATE(peDpRequestStatus,             109),\
    STATE(peDpRequestStatusAck,          110),\
    STATE(peDpRequestStatusNak,          111),\
    STATE(peDpRequestConfig,             112),\
    STATE(peDpRequestConfigAck,          113),\
    STATE(peDpRequestConfigNak,          114),\
    STATE(PE_BIST_Receive_Mode,          115),\
    STATE(PE_BIST_Frame_Received,        116),\
    STATE(PE_BIST_Carrier_Mode_2,        117),\
    STATE(PE_BIST_Test_Data,             118),\
    STATE(dbgGetRxPacket,                119),\
    STATE(dbgSendTxPacket,               120),\

const static char *PE_STATE_TBL[] = {
        LIST_OF_PE_STATES(GENERATE_STR)
};

const static size_t NUM_PE_STATES = sizeof(PE_STATE_TBL) / sizeof(PE_STATE_TBL[0]);

#define LIST_OF_PROTOCOL_STATES(STATE)\
    STATE(PRLDisabled,                     0),\
    STATE(PRLIdle,                         1),\
    STATE(PRLReset,                        2),\
    STATE(PRLResetWait,                    3),\
    STATE(PRLRxWait,                       4),\
    STATE(PRLTxSendingMessage,             5),\
    STATE(PRLTxWaitForPHYResponse,         6),\
    STATE(PRLTxVerifyGoodCRC,              7),\
    STATE(PRLManualRetries,                8),\
    STATE(PRL_BIST_Rx_Reset_Counter,       9),\
    STATE(PRL_BIST_Rx_Test_Frame,         10),\
    STATE(PRL_BIST_Rx_Error_Count,        11),\
    STATE(PRL_BIST_Rx_Inform_Policy,      12),\

const static char *PRL_STATE_TBL[] = {
        LIST_OF_PROTOCOL_STATES(GENERATE_STR)
};

const static size_t NUM_PRL_STATE = sizeof(PRL_STATE_TBL) / sizeof(PRL_STATE_TBL[0]);

#define LIST_OF_CC_TERM_STATES(STATE)\
    STATE(Open,             0),\
    STATE(Ra,               1),\
    STATE(Rdef,             2),\
    STATE(R1p5,             3),\
    STATE(R3p0,             4),\
    STATE(Undefined,        5),\

const static char *CC_TERM_TBL[] = {
    LIST_OF_CC_TERM_STATES(GENERATE_STR)
};

const static size_t NUM_CC_TERMS = sizeof(CC_TERM_TBL) / sizeof(CC_TERM_TBL[0]);

#endif
