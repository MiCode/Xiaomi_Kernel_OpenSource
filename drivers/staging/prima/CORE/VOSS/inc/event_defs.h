/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef EVENT_DEFS_H
#define EVENT_DEFS_H


typedef enum
{
  EVENT_DROP_ID = 0,

  EVENT_BAND_CLASS_CHANGE = 0x0100,   /* Includes band class as payload */
  EVENT_CDMA_CH_CHANGE,               /* Includes cdma channel as payload */
  EVENT_BS_P_REV_CHANGE,              /* Includes BS p_rev as payload */
  EVENT_P_REV_IN_USE_CHANGE,          /* Includes p_rev_in_use as payload */
  EVENT_SID_CHANGE,                   /* Includes SID as payload */
  EVENT_NID_CHANGE,                   /* Includes NID as payload */
  EVENT_PZID_CHANGE,                  /* Includes PZID as payload */
  EVENT_PDE_SESSION_END,              /* No payload */
  EVENT_OP_MODE_CHANGE,               /* Includes operation mode as payload */
  EVENT_MESSAGE_RECEIVED,             /* Includes channel and message ID as 
                                         payload */
  EVENT_MESSAGE_TRANSMITTED,          /* Includes channel and message ID as 
                                         payload */
  EVENT_TIMER_EXPIRED,                /* Includes timer ID as payload */
  EVENT_COUNTER_THRESHOLD,            /* Includes counter ID as payload */
  EVENT_CALL_PROCESSING_STATE_CHANGE, /* Includes new state and old state as
                                         payload */ 
  EVENT_CALL_CONTROL_INSTANTIATED,    /* Includes con_ref as payload */
  EVENT_CALL_CONTROL_STATE_CHANGE,    /* Includes con_ref, old substate and
                                         new substate as payload */
  EVENT_CALL_CONTROL_TERMINATED,      /* Includes con_ref as payload */
  EVENT_REG_ZONE_CHANGE,              /* Includes reg_zone as payload */ 
  EVENT_SLOTTED_MODE_OPERATION,       /* Includes enter/exit bit as payload */
  EVENT_QPCH_IN_USE,                  /* Includes enable/disable bit as payload */
  EVENT_IDLE_HANDOFF,                 /* Includes pn_offset as payload */
  EVENT_ACCESS_HANDOFF,               /* Includes pn_offset as payload */
  EVENT_ACCESS_PROBE_HANDOFF,         /* Includes pn_offset as payload */
  EVENT_SOFT_HANDOFF,   
    /* Includes pn_offsets of each BS in aset and indicators whether BS in SCH 
       aset*/ 
  EVENT_HARD_HANDOFF_FREQ_CHANGE, 
    /* Includes pn_offsets of each BS in aset and indicators whether BS in SCH 
       aset*/ 
  EVENT_HARD_HANDOFF_FRAME_OFFSET_CHANGE, 
    /* Includes pn_offsets of each BS in aset and indicators whether BS in SCH 
       aset*/ 
  EVENT_HARD_HANDOFF_DISJOINT_ASET,
    /* Includes pn_offsets of each BS in aset and indicators whether BS in SCH 
       aset*/ 
  EVENT_UNSUCCESSFUL_HARD_HANDOFF,    /* No payload */
  EVENT_TMSI_ASSIGNED,                /* Includes TMSI as payload */
  EVENT_SERVICE_NEGOTIATION_COMPLETED,/* No payload */
  EVENT_SO_NEGOTIATION_COMPLETED,     /* No payload */
  EVENT_ENTER_CONTROL_HOLD_MODE,      /* No payload */
  EVENT_EXIT_CONTROL_HOLD_MODE,       /* No payload */
  EVENT_START_FWD_SUPP_BURST_ASSGN,   /* Includes SCH rate as payload */
  EVENT_END_FWD_SUPP_BURST_ASSGN,     /* No payload */
  EVENT_START_REV_SUPP_BURST_ASSGN,   /* Includes SCH rate as payload */
  EVENT_END_REV_SUPP_BURST_ASSGN,     /* No payload */
  EVENT_DTX,                          /* No payload */
  EVENT_T_ADD_ABORT,                  /* No payload */
  EVENT_CH_IND_CHANGE,                /* Include ch_ind as payload */
  EVENT_TRANSMITTER_DISABLED,         /* No payload */
  EVENT_TRANSMITTER_ENABLED,          /* No payload */
  EVENT_SMS_RECEIVED,                 /* No payload */
  EVENT_SMS_SENT,                     /* No payload */
  EVENT_INACTIVITY_TIMER_EXPIRED,     /* No payload */
  EVENT_DORMANT_TIMER_EXPIRED,        /* No payload */
  EVENT_ACCESS_ATTEMPT_FAIL_MAX_PROBES_SENT, /* No payload */
  EVENT_ACCESS_ATTEMPT_FAIL_LOSS_OF_PC_OR_FCCC, /* No payload */
  EVENT_PCH_ACQUIRED,                 /* Includes pagech and pn_offset
                                         as payload */
  EVENT_BCCH_ACQUIRED,                /* Includes walsh code for BCCH and 
                                         pn_offset as payload */
  EVENT_FFCH_ACQUIRED,                /* Payload: 14 bytes */
  EVENT_FDCCH_ACQUIRED,               /* Payload: 14 bytes */
  EVENT_FFCH_PLUS_DCCH_ACQUIRED,      /* No payload */
  EVENT_REGISTRATION_PERFORMED,       /* Includes reg_type as payload */ 
  EVENT_NEW_SYSTEM_IDLE_HANDOFF,      /* No payload */
  EVENT_SYSTEM_RESELECTION,           /* Includes ecio and ps as payload */
  EVENT_RESCAN,                       /* No payload */
  EVENT_PROTOCOL_MISMATCH,            /* No payload */
  EVENT_LOCK,                         /* No payload */
  EVENT_UNLOCK,                       /* No payload */
  EVENT_ACCESS_DENIED,                /* No payload */
  EVENT_NDSS_OFF,                     /* No payload */
  EVENT_RELEASE,                      /* Payload: 1 byte */
  EVENT_ERROR,                        /* No payload */
  EVENT_REDIRECTION,                  /* No payload */
  EVENT_REGISTRATION_REJECTED,        /* No payload */
  EVENT_WRONG_SYSTEM,                 /* No payload */
  EVENT_WRONG_NETWORK,                /* No payload */
  EVENT_LOSS_OF_ACQ_AFTER_SLEEP,      /* No payload */
  EVENT_POWER_DOWN,                   /* No payload */
  EVENT_CALL_RELEASE_REQUEST,         /* No payload */
  EVENT_SERVICE_INACTIVE,             /* No payload */
  EVENT_EXTENDED_RELEASE,             /* No payload */

  EVENT_HDR_MSG_RX,                   /* protocol, msg- 3 bytes */
  EVENT_HDR_RXMSG_IGNORED_STATE,      /* protocol, msg- 3 bytes */
  EVENT_HDR_RXMSG_IGNORED_SEQ,        /* protocol, msg- 3 bytes */
  EVENT_HDR_TXMSG_ACKED,              /* protocol, msg- 3 bytes */
  EVENT_HDR_TXMSG_DROPPED,            /* protocol, msg- 3 bytes */      
  EVENT_HDR_STATE_CHANGE,             /* protocol, from, to - 5 bytes */
  EVENT_HDR_ALMP_OBEYING_REDIRECTION, /* No payload */
  EVENT_HDR_ALMP_CONNECTION_CLOSED,   /* No payload */
  EVENT_HDR_ALMP_T_SD_RESELECT,       /* No payload */
  EVENT_HDR_ALMP_CONNECTION_OPENED,   /* No payload */
  EVENT_HDR_HMP_QUEUED_MSG,           /* protocol, msg- 3 bytes */
  EVENT_HDR_HMP_SENT_MSG,             /* protocol, msg, chan, is_reliable - 5 bytes */
  EVENT_HDR_HMP_ABORTING_ACMAC_ACTIVATION, /* No payload */
  EVENT_HDR_IDLE_T_CONFIG_RSP,        /* No payload */
  EVENT_HDR_IDLE_T_AT_SETUP,          /* No payload */
  EVENT_HDR_IDLE_T_SUSPEND,           /* No payload */
  EVENT_HDR_IDLE_CONNECTION_DENIED,   /* No payload */
  EVENT_HDR_INIT_T_SYNC_ACQ,          /* No payload */
  EVENT_HDR_INIT_PROTOCOL_MISMATCH,   /* No payload */
  EVENT_HDR_OVHD_INFO_CURRENT,        /* No payload */
  EVENT_HDR_OVHD_T_QC_SUPERVISION,    /* No payload */
  EVENT_HDR_OVHD_T_SP_SUPERVISION,    /* No payload */
  EVENT_HDR_OVHD_T_AP_SUPERVISION,    /* No payload */
  EVENT_HDR_OVHD_IGNORED_MSG_UNEXPECTED_LINK, /* msg, exp_link.chan_num,
                                              exp_link.pilot, rx_link.chan_num,
                                              rx_link.pilot - 10 bytes */
  EVENT_HDR_OVHD_IGNORED_SP_MSG_DIFF_SEC_SIG, /* exp_sig, rx_sig - 8 bytes */
  EVENT_HDR_OVHD_IGNORED_AP_MSG_DIFF_ACC_SIG, /* exp_sig, rx_sig - 8 bytes */
  EVENT_HDR_OVHD_IGNORED_SP_MSG_DIFF_SEC_ID,  /* No payload */
  EVENT_HDR_OVHD_SP_MSG_RX,           /* No payload */
  EVENT_HDR_OVHD_AP_MSG_RX,           /* No payload */
  EVENT_HDR_RUP_T_CONNECTION_SETUP,   /* No payload */
  EVENT_HDR_SLP_MAX_RETRIES,          /* msg - 2 bytes */
  EVENT_HDR_LMAC_ACQ_FAIL_PILOT,      /* No payload */
  EVENT_HDR_LMAC_ACQ_SUCCESS,         /* No payload */
  EVENT_HDR_LMAC_NETWORK_LOST,        /* No payload */
  EVENT_HDR_LMAC_IDLE_HO,             /* new_pilot - 2 bytes */
  EVENT_HDR_LMAC_CHAN_CHANGE_COMPLETE, /* No payload */
  EVENT_HDR_LMAC_ACCESS_HO_NEEDED,    /* suggested_pilot - 2 bytes */
  EVENT_HDR_LMAC_ACCESS_HO_COMPLETE,  /* new_pilot - 2 bytes */
  EVENT_HDR_LMAC_ACQUIRE,             /* channel 2 bytes */
  EVENT_HDR_LMAC_CHANGING_CC_HASH,    /* cc_hash - 1 byte */
  EVENT_HDR_LMAC_IDLE_CHAN_CHANGE,    /* channel - 2 bytes */
  EVENT_HDR_CMAC_T_SUPERVISION,       /* No payload */
  EVENT_HDR_AMAC_START_ACCESS,        /* No payload */
  EVENT_HDR_AMAC_PROBING_STOPPED,     /* No payload */
  EVENT_HDR_AMAC_ACCESS_COMPLETE,     /* No payload */
  EVENT_HDR_AMAC_ACCESS_ABORTED,      /* No payload */
  EVENT_HDR_AMAC_MAX_PROBES,          /* No payload */
  EVENT_HDR_FMAC_DROP_PKT,            /* No payload */
  EVENT_HDR_RMAC_T_RATE_LIMIT,        /* No payload */
  EVENT_HDR_RMAC_TX_STARTED,          /* No payload */
  EVENT_HDR_RMAC_TX_STOPPED,          /* No payload */
  EVENT_HDR_SMP_T_KEEP_ALIVE,         /* No payload */
  EVENT_HDR_AMP_ASSIGN_MSG_IGNORED_FRESH, /* No payload */
  EVENT_HDR_AMP_T_AT_RESPONSE,        /* No payload */
  EVENT_HDR_AMP_T_DUAL_ADDRESS,       /* No payload */
  EVENT_HDR_SCP_BEGIN_CONFIGURATION,  /* No payload */
  EVENT_HDR_SCP_T_CONFIG_RSP,         /* No payload */
  EVENT_HDR_SCP_T_AN_INIT_STATE,      /* No payload */

  EVENT_WCDMA_L1_STATE,               /* l1_state - 1 byte */
  EVENT_WCDMA_IMSI,                   /* IMSI - 9 bytes */
  EVENT_GSM_L1_STATE,                 /* GSM l1_state - 1 byte */
  EVENT_RANDOM_ACCESS_REQUEST,        /* GSM Random Access Request - 4 bytes */
  EVENT_HIGH_LEVEL_CALL_PROCESSING_STATE_CHANGE, /* Puma requested event */
                                                 /* (same payload as CALL_PROCESSING_STATE_CHANGE) */
  EVENT_ENCRYPTION_FAILURE,                  /* Puma event, no payload */
  EVENT_ACCT_BLOCKED,                        /* Puma event, no payload */
  EVENT_COMMON_CHANNEL_MONITORED,            /* Puma event, 1 byte payload */
  EVENT_SOFT_HANDOFF_V2,                     /* Puma event, 14 byte payload */
  EVENT_HARD_HANDOFF_FREQ_CHANGE_V2,         /* Puma event, 14 byte payload */
  EVENT_HARD_HANDOFF_FRAME_OFFSET_CHANGE_V2, /* Puma event, 14 byte payload */
  EVENT_HARD_HANDOFF_DISJOINT_ASET_V2,       /* Puma event, 14 byte payload */
  EVENT_WCDMA_NEW_REFERENCE_CELL,
  EVENT_CALL_CONTROL_CONREF_CHANGE,          /* Puma event, 2 byte payload */

  EVENT_GPS_SESSION_BEGIN,
  EVENT_GPS_SESSION_END,
  EVENT_GPS_WAITING_ON_SA,
  EVENT_GPS_PPM_START,
  EVENT_GPS_PPM_RESULTS,
  EVENT_GPS_PPM_END,
  EVENT_GPS_VISIT_BEGIN,
  EVENT_GPS_VISIT_END,
  EVENT_GPS_CDMA_RESUMED_AFTER_GPS_VISIT,
  EVENT_GPS_PD_SESSION_BEGIN,
  EVENT_GPS_PD_SESSION_END,                  /* Payload: 1 byte PDSM substate */
  EVENT_GPS_IS801_RX,                        /* Payload, 1 byte msg_type */
  EVENT_GPS_IS801_TX,                        /* Payload: 1 byte msg_type */
  EVENT_POWERUP,
  EVENT_WCDMA_ASET,
  EVENT_CM_CALL_STATE,                       /* (1 byte payload: overall call state) */
  EVENT_CM_OPERATIONAL_MODE,                 /* (1 byte payload: op mode) */
  EVENT_CM_SYSTEM_MODE,                      /* (1 byte payload: sys_mode) */

  EVENT_DEEP_SLEEP,                          /* no payload */
  EVENT_WAKEUP,                              /* unsigned long (4 bytes) payload */
  EVENT_ACQUISITION_MODE,                    /* unsigned char (1 byte) payload */
  EVENT_ACQUISITION_TYPE,                    /* unsigned char (1 byte) payload */
  EVENT_ACP_EXIT,                            /* unsigned char (1 byte) payload */
  EVENT_CDMA_EXIT,                           /* unsigned char (1 byte) payload */

  EVENT_HDR_HYBRID_POWER_SAVE,               /* No payload */
  EVENT_HDR_DEEP_SLEEP,                      /* No payload */
  EVENT_HDR_RESELECTION,                     /* No payload */
  EVENT_SAM_LOCK_GRANTED,                    /* <NewOwner (1 byte) <Duration (2 bytes) */
  EVENT_SAM_LOCK_RELEASED,                   /* <OldOwner (1 byte) */

  EVENT_GSM_HANDOVER_START,
  EVENT_GSM_HANDOVER_END,
  EVENT_GSM_LINK_FAILURE,
  EVENT_GSM_RESELECT_START,
  EVENT_GSM_RESELECT_END,
  EVENT_GSM_CAMP_ATTEMPT_START,
  EVENT_GSM_RR_IN_SERVICE,
  EVENT_GSM_RR_OUT_OF_SERVICE,
  EVENT_GSM_PAGE_RECEIVED,
  EVENT_GSM_CAMP_ATTEMPT_END,
  EVENT_GPS_IS801_TIMEOUT,
  EVENT_GPS_IS801_DISCARD,                   /* Payload: 1 byte msg type */
  EVENT_GSM_CELL_SELECTION_START,            /* Payload: 1 byte cell_selection_reason */
  EVENT_GSM_CELL_SELECTION_END,              /* Payload: 1 byte end_status success/failure reason */
  EVENT_GSM_POWER_SCAN_STATUS,               /* Payload: 1 byte status (started or completed) */
  EVENT_GSM_PLMN_LIST_START,                 /* Payload: 1 byte */
  EVENT_GSM_PLMN_LIST_END,
  EVENT_WCDMA_INTER_RAT_HANDOVER_START,      /* Payload: 4 bytes handover type, BCCH ARFCN, BSIC */
  EVENT_WCDMA_INTER_RAT_HANDOVER_END,        /* Payload: 2 bytes success, failure reason */

  EVENT_GSM_MESSAGE_SENT,                    /* Payload: 2 bytes indicating identity of the message */
  EVENT_GSM_MESSAGE_RECEIVED,                /* Payload: 2 bytes indicating identity of the message */
  EVENT_GSM_TIMER_EXPIRED,                   /* Payload: 2 bytes indicating identity of the message */
  EVENT_GSM_COUNTER_EXPIRED,                 /* Payload: 2 bytes indicating identity of the message */

  EVENT_NAS_MESSAGE_SENT,
  EVENT_NAS_MESSAGE_RECEIVED,
  EVENT_RRC_MESSAGE_SENT,
  EVENT_RRC_MESSAGE_RECEIVED,
                                    /* Camera events: No Payload */
  EVENT_CAMERA_CANNOT_CAPTURE,      /* Cannot capture, transition to ready */
  EVENT_CAMERA_CANNOT_CONFIG_JPEG,  /* Cannot config JPEG, transition to ready */
  EVENT_CAMERA_CANNOT_CONFIG_VFE,   /* Cannot config VFE, transition to ready */
  EVENT_CAMERA_CANNOT_ENCODE,       /* Cannot encode, transition to ready */
  EVENT_CAMERA_CANNOT_IDLE_DSP,     /* Cannot idle DSP, transition to ready */
  EVENT_CAMERA_CANNOT_LOAD_DSP,     /* Cannot load DSP, transition to ready */
  EVENT_CAMERA_DSP_FATAL,           /* DSP fatal error, transition to ready */
  EVENT_CAMERA_DSP_REQ_ILLEGAL,     /* DSP request illegal, transition to ready */
  EVENT_CAMERA_EFS_FAILED,          /* EFS failed, transition to ready */
  EVENT_CAMERA_EXIT,                /* Stop, transition to ready */
  EVENT_CAMERA_FORMAT_NOT_SUPPORTED,/* Format not supported */
  EVENT_CAMERA_FUNCTION_REJECTED,   /* Function rejected, transition to ready */
  EVENT_CAMERA_IMAGE_CORRUPT,       /* Image corrupted, reconfig */
  EVENT_CAMERA_INVALID_CONFIG_PARM, /* Invalid config parm, to ready */
  EVENT_CAMERA_INVALID_SET_ID,      /* Invalid set param ID type */
  EVENT_CAMERA_INVALID_STATE,       /* Invalid state */
  EVENT_CAMERA_JPEG_ENCODED,        /* JPEG encoded */
  EVENT_CAMERA_NO_MEMORY,           /* No memory, transition to ready */
  EVENT_CAMERA_NO_PICTURE,          /* Picture not ready */
  EVENT_CAMERA_PICTURE_SAVED,       /* Picture saved, transition to ready */
  EVENT_CAMERA_PICTURE_TAKEN,       /* Picture taken, transition to ready */
  EVENT_CAMERA_PREVIEW,             /* Enter preview */
  EVENT_CAMERA_RECORD,              /* Enter recording */
  EVENT_CAMERA_SAVE_PICTURE,        /* Enter save picture */
  EVENT_CAMERA_SET_FAILED,          /* Set failed */
  EVENT_CAMERA_SET_SUCCEEDED,       /* Set succeeded */
  EVENT_CAMERA_START,               /* Start, transition to ready */
  EVENT_CAMERA_STOP,                /* Stop, transition to init */
  EVENT_CAMERA_TAKE_PICTURE,        /* Enter take picture */

  EVENT_DIAG_STRESS_TEST_NO_PAYLOAD,
  EVENT_DIAG_STRESS_TEST_WITH_PAYLOAD,

  EVENT_CM_CALL_ORIG_START_P1,      /* Payload: 5 single bytes */
  EVENT_CM_CALL_ORIG_START_P2,      /* Payload: 3 single bytes */
  EVENT_CM_CALL_ORIG_START_P3,      /* Payload: 3 single bytes */
  EVENT_CM_CALL_ORIG_SWITCH_TO_HDR, /* No payload */
  EVENT_CM_CALL_ORIG_REDIAL,        /* Payload: 4 bytes */
  EVENT_CM_CALL_ORIG_SEND_HDR_ORIG, /* No payload */
  EVENT_CM_CALL_ORIG_SEND_MC_ORIG,  /* No payload */
  EVENT_CM_CALL_ORIG_END,           /* Payload: 2 bytes */
  EVENT_CM_CALL_ORIG_CONNECTED,     /* Payload: 3 bytes */

  EVENT_MT_SMS_NOTIFY,              /* Payload: 1 byte mem_type, 4 byte msg_index */
  EVENT_SMS_SLOT_WAKEUP,            /* Payload: 1 byte slot_mask */
  EVENT_MO_SMS_STATUS,              /* Payload: 4 byte transaction_id, 1 byte mem_type,
                                                4 byte msg_index, 1 byte report_status, 
                                                1 byte case_code */
  EVENT_GPRS_SURROUND_SEARCH_START,
  EVENT_GPRS_SURROUND_SEARCH_END,
  EVENT_GPRS_MAC_RESELECT_IND,
  EVENT_GPRS_PAGE_RECEIVED,
  EVENT_GPRS_LINK_FAILURE,
  EVENT_GPRS_CELL_UPDATE_START,
  EVENT_GPRS_CELL_UPDATE_END,
  EVENT_GPRS_EARLY_CAMPING,
  EVENT_PACKET_RANDOM_ACCESS_REQ,
  EVENT_GPRS_MAC_MSG_SENT,          /* Payload: 3 bytes giving message identity and channel */
  EVENT_GPRS_MAC_MSG_RECEIVED,      /* Payload: 3 bytes giving message identity and channel */
  EVENT_GPRS_SMGMM_MSG_SENT,        /* Payload: 3 bytes giving message identity and channel */
  EVENT_GPRS_SMGMM_MSG_RECEIVED,    /* Payload: 3 bytes giving message identity and channel */

  EVENT_CP_MATCHED_MSG,             /* Payload: 1 byte */
  EVENT_PREF_SYS_RESEL,             /* Payload: none */ 

  EVENT_WCDMA_LAYER1_PRACH,         /* Payload: 4 bytes */
  EVENT_WCDMA_LAYER1_MEASUREMENT,   /* Payload: Unspecified */

  EVENT_MOBILITY_MANAGEMENT_STATE_CHANGE, /* Payload: 1 byte old state, 1 byte new state */

  EVENT_LSM_STATE_CHANGE,           /* Payload: 1 byte */
  EVENT_RLP,                        /* Payload: 2 bytes */

  EVENT_CM_MODE_PREF,               /* Payload: 4 bytes */
  EVENT_CM_BAND_PREF,               /* Payload: 4 bytes */
  EVENT_CM_ROAM_PREF,               /* Payload: 4 bytes */
  EVENT_CM_SRV_DOMAIN_PREF,         /* Payload: 4 bytes */
  EVENT_CM_GW_ACQ_ORDER_PREF,       /* Payload: 4 bytes */
  EVENT_CM_HYBRID_PREF,             /* Payload: 4 bytes */
  EVENT_CM_NETWORK_SEL_MODE_PREF,   /* Payload: 4 bytes */

  EVENT_WCDMA_L1_SUSPEND,                 /* Payload: 3 bytes */
  EVENT_WCDMA_L1_RESUME,                  /* Payload: 5 bytes */
  EVENT_WCDMA_L1_STOPPED,                 /* Payload: 4 bytes */
  EVENT_WCDMA_TO_WCDMA_RESELECTION_START, /* Payload: 6 bytes */
  EVENT_WCDMA_TO_GSM_RESELECTION_START,   /* Payload: 4 bytes */
  EVENT_WCDMA_TO_GSM_RESELECTION_END,     /* Payload: 2 bytes */
  EVENT_WCDMA_TO_WCDMA_RESELECTION_END,   /* Payload: 4 bytes */
  EVENT_WCDMA_RACH_ATTEMPT,               /* Payload: 3 bytes */

  EVENT_START_FWD_SUPP_BURST_ASSIGN,      /* Payload: 3 bytes */
  EVENT_START_REV_SUPP_BURST_ASSIGN,      /* Payload: 3 bytes */
  EVENT_REV_FCH_GATING_IN_USE,            /* Payload: 1 byte  */
  EVENT_PPP,                              /* Payload: 7 bytes */
  EVENT_MIP,                              /* Payload: 2 bytes */
  EVENT_TCP,                              /* Payload: 5 bytes */
  EVENT_CAMERA_EXIF_FAILED,               /* EXIF encoding failed */
  EVENT_CAMERA_VIDEO_FAILED,              /* Video encoding failed */
  EVENT_CAMERA_NO_SENSOR,                 /* No sensor */
  EVENT_CAMERA_ABORT,                     /* Operation aborted */

  EVENT_CM_BLOCK_HDR_ORIG_DURING_GPS,     /* No payload */
  EVENT_CM_ALLOW_HDR_ORIG_DURING_GPS,     /* No payload */

  EVENT_GSM_AMR_STATE_CHANGE,             /* 2 bytes payload */
  EVENT_GSM_RATSCCH_IN_DTX,               /* No Payload */
  EVENT_GSM_FACCH_IN_DTX,                 /* No Payload */
  EVENT_GSM_FACCH_AND_RATSCCH_COLLISION,  /* No Payload */
  EVENT_GSM_FACCH_AND_SID_UPDATE_COLLISION,  /* No Payload */
  EVENT_GSM_RATSCCH_AND_SID_UPDATE_COLLISION,/* No Payload */
  EVENT_GSM_RATSCCH_CMI_PHASE_CHANGE,     /* 2 bytes payload */
  EVENT_GSM_RATSCCH_REQ_ACT_TIMER_EXPIRY, /* 8 bytes payload */
  EVENT_GSM_RATSCCH_ACK_ACT_TIMER_EXPIRY, /* 2 bytes payload */
  EVENT_GSM_AMR_CMC_TURNAROUND_TIME,      /* 2 bytes payload */
  EVENT_CM_PLMN_FOUND,                    /* 12 bytes */
  EVENT_CM_SERVICE_CONFIRMED,             /* 12 bytes */

  EVENT_GPRS_MAC_CAMPED_ON_CELL,          /* Event Id : 559, No payload */
  EVENT_GPRS_LLC_READY_TIMER_START,       /* Event Id : 560, No payload */
  EVENT_GPRS_LLC_READY_TIMER_END,         /* Event Id : 561, No payload */

  EVENT_WCDMA_PHYCHAN_ESTABLISHED,        /* Payload TBD */
  EVENT_HS_DISPLAY_BMP_CAPTURE_STATUS,    /* Payload 4 bytes */

  EVENT_WCDMA_CELL_SELECTED,              /* 4 byte Payload */
  EVENT_WCDMA_PAGE_RECEIVED,              /* 2 byte Payload */
  EVENT_WCDMA_SEND_KEY,                   /* Payload TBD */
  EVENT_WCDMA_RL_FAILURE,                 /* No Payload */
  EVENT_WCDMA_MAX_RESET,                  /* 2 byte Payload */
  EVENT_WCDMA_CALL_SETUP,                 /* Payload TBD */
  EVENT_WCDMA_CALL_DROPPED,               /* Payload TBD */
  EVENT_WCDMA_RRC_STATE,                  /* 3 byte Payload */
  EVENT_GPS_PD_CONNECTION_TIMEOUT,        /* No Payload */
  EVENT_GPS_PD_DISCONNECTION_COMPLETE,    /* No Payload */

  EVENT_MEDIA_PLAYER_START,               /* media player starts playing a clip, no payload */
  EVENT_MEDIA_PLAYER_STOP,                /* media player stopped playing a clip, no payload */
  EVENT_MEDIA_PLAYER_SEEK,                /* media player repositioned itself, no payload */

  EVENT_GPS_SRCH_START,             /* Payload: session_type (1 byte) */

  EVENT_GPS_SRCH_END,               /* No Payload */
  EVENT_GPS_PPM_PAUSE,              /* Payload: pause_reason (1 byte) */
  EVENT_GPS_PPM_RESUME,             /* No Payload */
  EVENT_GPS_SA_RECEIVED,            /* Payload: REF_BIT_NUM (2 bytes), 
                                                DR_SIZE     (1byte) 
                                    */
  EVENT_GPS_CLK_ON,                 /* No Payload */
  EVENT_GPS_CLK_OFF,                /* No Payload */
  EVENT_GPS_VISIT_REQUEST,          /* No Payload */ 
  EVENT_GPS_VISIT_RESPONSE,         /* Payload: response_result (1 byte)
                                    */ 
  EVENT_GPS_TA_START,               /* No Payload */
  EVENT_GPS_DSP_READY,              /* No Payload */
  EVENT_GPS_DSP_CHANNEL_START,      /* Payload: SV_ID         (1 byte), 
                                                SRCH_MODE     (1 byte), 
                                                CHANNEL_NUM   (1 byte),
                                                RESERVED      (1 byte)
                                    */
  EVENT_GPS_DSP_CHANNEL_DONE,       /* Payload: channel_num (1 byte) */
  EVENT_GPS_DSP_STOP,               /* No Payload */
  EVENT_GPS_DSP_DONE,               /* No Payload */
  EVENT_GPS_TB_END,                 /* No Payload */
  EVENT_GPS_SRCH_LARGE_DOPP_WIN,    /* Payload: sv_prn_num (1 byte),
                                                srch_mode (1 byte),
                                                dopp_wind (2 byte)
                                    */ 
  EVENT_GPS_SRCH_EXCEPTION,         /* Payload: grid_log_id (2 byte),
                                                exception_type (1 byte)
                                    */
  EVENT_GPS_SRCH_HW_POLLING1,       /* Payload: agc_val (2 byte),
                                                dci_off (2 byte),
                                                dcq_off (2 byte),
                                                trk_lo (2 byte),
                                                lo_bias (2 byte)
                                    */

  EVENT_GPS_SRCH_HW_POLLING2,       /* Payload: sync80 (2 byte)
                                    */
  EVENT_GPS_PGI_ACTION_PROCESS,     /* Payload: pgi_substate (1 byte),
                                                pgi_cmd (1 byte)
                                    */
  EVENT_GPS_GSC_ACTION_PROCESS,     /* Payload: gsc_substate (1 byte),
                                                gsc_cmd (1 byte)
                                    */
  EVENT_GPS_PGI_ABORT,              /* Payload: pgi_subsate (1 byte) */
  EVENT_GPS_GSC_ABORT,              /* Payload: gsc_subsate (1 byte) */



  EVENT_GPS_PD_FIX_START,                /* Payload: event_log_cnt  (2 byte), 
                                                operation_mode (1 byte)
                                         */
  EVENT_GPS_PD_FIX_END,                  /* Payload: end_status     (1 byte)  */
  EVENT_GPS_DATA_DOWNLOAD_START,         /* Payload: data_type   (1 byte),
                                                     sv_mask     (4 byte)     */
  EVENT_GPS_DATA_DOWNLOAD_END,           /* Payload: end_status      (1 byte) */
  EVENT_GPS_PD_SESSION_START,            /* Payload: start_source    (4 bit)
                                                     operation_mode  (4 bit)
                                                     session_type    (4 bit)
                                                     privacy         (4 bit)
                                                     num_fixed       (2 byte)
                                                     fix_period      (2 byte)
                                                     nav_data_dl     (4 bit)
                                                     prq             (1 byte)
                                                     threshold       (2 byte)
                                                     transport_type  (4 bit)  */
  EVENT_GPS_DORMANCY_BEGIN,               /* No Payload */
  EVENT_GPS_DORMANCY_END,                 /* No Payload */
  EVENT_GPS_PRQ_TIMEOUT,                  /* No Payload */
  EVENT_GPS_PD_CONNECTION_START,          /* No Payload */
  EVENT_GPS_PD_CONNECTION_ESTABLISHED,    /* No Payload */
  EVENT_GPS_PD_DISCONNECTION_START,       /* No Payload */
  EVENT_GPS_FTEST_FIX_START,              /* Payload: reserved (4 byte) */
  EVENT_GPS_FTEST_FIX_END,                /* Payload: reserved (4 byte) */
  EVENT_GPS_PD_POSITION,                  /* No Payload */
  EVENT_GPS_E911_START,                   /* No Payload */
  EVENT_GPS_E911_END,                     /* No Payload */
  EVENT_GPS_DBM_SEND_FAILURE,             /* No Payload */
  EVENT_GPS_UAPDMS_STATE_CHANGE,          /* Payload: new_state (1 byte)
                                                      reason    (1 byte) */
  EVENT_WCDMA_OUT_OF_SERVICE,             /* No Payload */
  EVENT_GSM_L1_SUBSTATE,                  /* 2 bytes payload */
  EVENT_SD_EVENT_ACTION,                  /* 8 byte payload */
  EVENT_SD_EVENT_ACTION_HYBR,             /* 8 byte payload */

  EVENT_UMTS_CALLS_STATISTICS,            /* 1 byte payload */
  EVENT_PZID_HAT_STARTED,                 /* No payload */
  EVENT_WCDMA_DRX_CYCLE,                  /* 3 byte payload */
  EVENT_WCDMA_RE_ACQUISITION_FAIL,        /* No payload */
  EVENT_WCDMA_RRC_RB0_SETUP_FAILURE,      /* No payload */
  EVENT_WCDMA_RRC_PHYCHAN_EST_FAILURE,    /* No payload */
  EVENT_CM_CALL_EVENT_ORIG,               /* 3 byte payload */
  EVENT_CM_CALL_EVENT_CONNECT,            /* 3 byte payload */
  EVENT_CM_CALL_EVENT_END,                /* 2 byte payload */
  EVENT_CM_ENTER_EMERGENCY_CB,            /* No payload */
  EVENT_CM_EXIT_EMERGENCY_CB,             /* No payload */
  EVENT_PZID_HAT_EXPIRED,                 /* No payload */
  EVENT_HDR_SMP_SESSION_CLOSED,           /* 1 byte payload */
  EVENT_WCDMA_MEMORY_LEAK,                /* No payload */
  EVENT_PZID_HT_STARTED,                  /* 1 byte payload */
  EVENT_PZID_HT_EXPIRED,                  /* 1 byte payload */
  EVENT_ACCESS_ENTRY_HANDOFF,             /* 2 byte payload */

  EVENT_BREW_APP_START,                   /* 8 byte payload */
  EVENT_BREW_APP_STOP,                    /* 8 byte payload */
  EVENT_BREW_APP_PAUSE,                   /* 8 byte payload */
  EVENT_BREW_APP_RESUME,                  /* 8 byte payload */
  EVENT_BREW_EXT_MODULE_START,            /* 8 byte payload */
  EVENT_BREW_EXT_MODULE_STOP,             /* 8 byte payload */
  EVENT_BREW_ERROR,                       /* 8 byte payload */
  EVENT_BREW_RESERVED_647,                /* BREW internal use only */
  EVENT_BREW_RESERVED_648,                /* BREW internal use only */
  EVENT_BREW_RESERVED_649,                /* BREW internal use only */
  EVENT_BREW_RESERVED_650,                /* BREW internal use only */
  EVENT_BREW_RESERVED_651,                /* BREW internal use only */
  EVENT_BREW_RESERVED_652,                /* BREW internal use only */
  EVENT_BREW_RESERVED_653,                /* BREW internal use only */
  EVENT_BREW_RESERVED_654,                /* BREW internal use only */
  EVENT_BREW_RESERVED_655,                /* BREW internal use only */
  EVENT_BREW_USER_656,                    /* Start of BREW user events */
  EVENT_BREW_GENERIC,                     /* 8 byte payload: clsid + data */
  EVENT_BREW_MEDIAPLAYER_SELECT_FILE,     /* no payload */
  EVENT_BREW_MEDIAPLAYER_CONTROL,         /* no payload */
  EVENT_BREW_APP_FORMITEM_STACK_CHANGE,   /* 1 byte payload */
  EVENT_BREW_CATAPP_RECV_PROACTIVE_CMD,   /* No payload */
  EVENT_BREW_CATAPP_TERMINAL_RSP,         /* No payload */
  EVENT_BREW_CATAPP_NO_DISPLAY,           /* No payload */
  EVENT_BREW_SIRIUS_EMAIL_DELETE,                  /* No payload */
  EVENT_BREW_SIRIUS_EMAIL_OPERATION_COMPLETE,      /* 8 byte payload */
  EVENT_BREW_SIRIUS_EMAIL_NEW_EMAIL_NOTIFICATION,  /* No payload */
  EVENT_BREW_UNDEFINED_667,
  EVENT_BREW_UNDEFINED_668,
  EVENT_BREW_UNDEFINED_669,
  EVENT_BREW_UNDEFINED_670,
  EVENT_BREW_UNDEFINED_671,
  EVENT_BREW_UNDEFINED_672,
  EVENT_BREW_UNDEFINED_673,
  EVENT_BREW_UNDEFINED_674,
  EVENT_BREW_UNDEFINED_675,
  EVENT_BREW_UNDEFINED_676,
  EVENT_BREW_UNDEFINED_677,
  EVENT_BREW_UNDEFINED_678,
  EVENT_BREW_UNDEFINED_679,
  EVENT_BREW_UNDEFINED_680,
  EVENT_BREW_UNDEFINED_681,
  EVENT_BREW_UNDEFINED_682,
  EVENT_BREW_UNDEFINED_683,
  EVENT_BREW_UNDEFINED_684,
  EVENT_BREW_UNDEFINED_685,
  EVENT_BREW_UNDEFINED_686,
  EVENT_BREW_UNDEFINED_687,
  EVENT_BREW_UNDEFINED_688,
  EVENT_BREW_UNDEFINED_689,
  EVENT_BREW_UNDEFINED_690,
  EVENT_BREW_UNDEFINED_691,
  EVENT_BREW_UNDEFINED_692,
  EVENT_BREW_UNDEFINED_693,
  EVENT_BREW_UNDEFINED_694,
  EVENT_BREW_UNDEFINED_695,
  EVENT_BREW_UNDEFINED_696,
  EVENT_BREW_UNDEFINED_697,
  EVENT_BREW_UNDEFINED_698,
  EVENT_BREW_UNDEFINED_699,
  EVENT_BREW_UNDEFINED_700,
  EVENT_BREW_UNDEFINED_701,
  EVENT_BREW_UNDEFINED_702,
  EVENT_BREW_UNDEFINED_703,
  EVENT_BREW_UNDEFINED_704,
  EVENT_BREW_UNDEFINED_705,
  EVENT_BREW_UNDEFINED_706,
  EVENT_BREW_UNDEFINED_707,
  EVENT_BREW_UNDEFINED_708,
  EVENT_BREW_UNDEFINED_709,
  EVENT_BREW_UNDEFINED_710,
  EVENT_BREW_UNDEFINED_711,
  EVENT_BREW_UNDEFINED_712,
  EVENT_BREW_UNDEFINED_713,
  EVENT_BREW_UNDEFINED_714,
  EVENT_BREW_UNDEFINED_715,
  EVENT_BREW_UNDEFINED_716,
  EVENT_BREW_UNDEFINED_717,
  EVENT_BREW_UNDEFINED_718,
  EVENT_BREW_UNDEFINED_719,
  EVENT_BREW_UNDEFINED_720,
  EVENT_BREW_UNDEFINED_721,
  EVENT_BREW_UNDEFINED_722,
  EVENT_BREW_UNDEFINED_723,
  EVENT_BREW_UNDEFINED_724,
  EVENT_BREW_UNDEFINED_725,
  EVENT_BREW_UNDEFINED_726,
  EVENT_BREW_UNDEFINED_727,
  EVENT_BREW_UNDEFINED_728,
  EVENT_BREW_UNDEFINED_729,
  EVENT_BREW_UNDEFINED_730,
  EVENT_BREW_UNDEFINED_731,
  EVENT_BREW_UNDEFINED_732,
  EVENT_BREW_UNDEFINED_733,
  EVENT_BREW_UNDEFINED_734,
  EVENT_BREW_UNDEFINED_735,
  EVENT_BREW_UNDEFINED_736,
  EVENT_BREW_UNDEFINED_737,
  EVENT_BREW_UNDEFINED_738,
  EVENT_BREW_UNDEFINED_739,
  EVENT_BREW_UNDEFINED_740,
  EVENT_BREW_UNDEFINED_741,
  EVENT_BREW_UNDEFINED_742,
  EVENT_BREW_UNDEFINED_743,
  EVENT_BREW_UNDEFINED_744,
  EVENT_BREW_UNDEFINED_745,
  EVENT_BREW_UNDEFINED_746,
  EVENT_BREW_UNDEFINED_747,
  EVENT_BREW_UNDEFINED_748,
  EVENT_BREW_UNDEFINED_749,
  EVENT_BREW_UNDEFINED_750,
  EVENT_BREW_UNDEFINED_751,
  EVENT_BREW_UNDEFINED_752,
  EVENT_BREW_UNDEFINED_753,
  EVENT_BREW_UNDEFINED_754,
  EVENT_BREW_UNDEFINED_755,
  EVENT_BREW_UNDEFINED_756,
  EVENT_BREW_UNDEFINED_757,
  EVENT_BREW_UNDEFINED_758,
  EVENT_BREW_UNDEFINED_759,
  EVENT_BREW_UNDEFINED_760,
  EVENT_BREW_UNDEFINED_761,
  EVENT_BREW_UNDEFINED_762,
  EVENT_BREW_UNDEFINED_763,
  EVENT_BREW_UNDEFINED_764,
  EVENT_BREW_UNDEFINED_765,
  EVENT_BREW_UNDEFINED_766,
  EVENT_BREW_UNDEFINED_767,
  EVENT_BREW_UNDEFINED_768,
  EVENT_BREW_UNDEFINED_769,
  EVENT_BREW_UNDEFINED_770,
  EVENT_BREW_UNDEFINED_771,
  EVENT_BREW_UNDEFINED_772,
  EVENT_BREW_UNDEFINED_773,
  EVENT_BREW_UNDEFINED_774,
  EVENT_BREW_UNDEFINED_775,
  EVENT_BREW_UNDEFINED_776,
  EVENT_BREW_UNDEFINED_777,
  EVENT_BREW_UNDEFINED_778,
  EVENT_BREW_UNDEFINED_779,
  EVENT_BREW_UNDEFINED_780,
  EVENT_BREW_UNDEFINED_781,
  EVENT_BREW_UNDEFINED_782,
  EVENT_BREW_UNDEFINED_783,
  EVENT_BREW_UNDEFINED_784,
  EVENT_BREW_UNDEFINED_785,
  EVENT_BREW_UNDEFINED_786,
  EVENT_BREW_UNDEFINED_787,
  EVENT_BREW_UNDEFINED_788,
  EVENT_BREW_UNDEFINED_789,
  EVENT_BREW_UNDEFINED_790,
  EVENT_BREW_UNDEFINED_791,
  EVENT_BREW_UNDEFINED_792,
  EVENT_BREW_UNDEFINED_793,
  EVENT_BREW_UNDEFINED_794,
  EVENT_BREW_UNDEFINED_795,
  EVENT_BREW_UNDEFINED_796,
  EVENT_BREW_UNDEFINED_797,
  EVENT_BREW_UNDEFINED_798,
  EVENT_BREW_UNDEFINED_799,
  EVENT_BREW_UNDEFINED_800,
  EVENT_BREW_UNDEFINED_801,
  EVENT_BREW_UNDEFINED_802,
  EVENT_BREW_UNDEFINED_803,
  EVENT_BREW_UNDEFINED_804,
  EVENT_BREW_UNDEFINED_805,
  EVENT_BREW_UNDEFINED_806,
  EVENT_BREW_UNDEFINED_807,
  EVENT_BREW_UNDEFINED_808,
  EVENT_BREW_UNDEFINED_809,
  EVENT_BREW_UNDEFINED_810,
  EVENT_BREW_UNDEFINED_811,
  EVENT_BREW_UNDEFINED_812,
  EVENT_BREW_UNDEFINED_813,
  EVENT_BREW_UNDEFINED_814,
  EVENT_BREW_UNDEFINED_815,
  EVENT_BREW_UNDEFINED_816,
  EVENT_BREW_UNDEFINED_817,
  EVENT_BREW_UNDEFINED_818,
  EVENT_BREW_UNDEFINED_819,
  EVENT_BREW_UNDEFINED_820,
  EVENT_BREW_UNDEFINED_821,
  EVENT_BREW_UNDEFINED_822,
  EVENT_BREW_UNDEFINED_823,
  EVENT_BREW_UNDEFINED_824,
  EVENT_BREW_UNDEFINED_825,
  EVENT_BREW_UNDEFINED_826,
  EVENT_BREW_UNDEFINED_827,
  EVENT_BREW_UNDEFINED_828,
  EVENT_BREW_UNDEFINED_829,
  EVENT_BREW_UNDEFINED_830,
  EVENT_BREW_UNDEFINED_831,
  EVENT_BREW_UNDEFINED_832,
  EVENT_BREW_UNDEFINED_833,
  EVENT_BREW_UNDEFINED_834,
  EVENT_BREW_UNDEFINED_835,
  EVENT_BREW_UNDEFINED_836,
  EVENT_BREW_UNDEFINED_837,
  EVENT_BREW_UNDEFINED_838,
  EVENT_BREW_UNDEFINED_839,
  EVENT_BREW_UNDEFINED_840,
  EVENT_BREW_UNDEFINED_841,
  EVENT_BREW_UNDEFINED_842,
  EVENT_BREW_UNDEFINED_843,
  EVENT_BREW_UNDEFINED_844,
  EVENT_BREW_UNDEFINED_845,
  EVENT_BREW_UNDEFINED_846,
  EVENT_BREW_UNDEFINED_847,
  EVENT_BREW_UNDEFINED_848,
  EVENT_BREW_UNDEFINED_849,
  EVENT_BREW_UNDEFINED_850,
  EVENT_BREW_UNDEFINED_851,
  EVENT_BREW_UNDEFINED_852,
  EVENT_BREW_UNDEFINED_853,
  EVENT_BREW_UNDEFINED_854,
  EVENT_BREW_UNDEFINED_855,
  EVENT_BREW_UNDEFINED_856,
  EVENT_BREW_UNDEFINED_857,
  EVENT_BREW_UNDEFINED_858,
  EVENT_BREW_UNDEFINED_859,
  EVENT_BREW_UNDEFINED_860,
  EVENT_BREW_UNDEFINED_861,
  EVENT_BREW_UNDEFINED_862,
  EVENT_BREW_UNDEFINED_863,
  EVENT_BREW_UNDEFINED_864,
  EVENT_BREW_UNDEFINED_865,
  EVENT_BREW_UNDEFINED_866,
  EVENT_BREW_UNDEFINED_867,
  EVENT_BREW_UNDEFINED_868,
  EVENT_BREW_UNDEFINED_869,
  EVENT_BREW_UNDEFINED_870,
  EVENT_BREW_UNDEFINED_871,
  EVENT_BREW_UNDEFINED_872,
  EVENT_BREW_UNDEFINED_873,
  EVENT_BREW_UNDEFINED_874,
  EVENT_BREW_UNDEFINED_875,
  EVENT_BREW_UNDEFINED_876,
  EVENT_BREW_UNDEFINED_877,
  EVENT_BREW_UNDEFINED_878,
  EVENT_BREW_UNDEFINED_879,
  EVENT_BREW_UNDEFINED_880,
  EVENT_BREW_UNDEFINED_881,
  EVENT_BREW_UNDEFINED_882,
  EVENT_BREW_UNDEFINED_883,
  EVENT_BREW_UNDEFINED_884,
  EVENT_BREW_UNDEFINED_885,
  EVENT_BREW_UNDEFINED_886,
  EVENT_BREW_UNDEFINED_887,
  EVENT_BREW_UNDEFINED_888,
  EVENT_BREW_UNDEFINED_889,
  EVENT_BREW_UNDEFINED_890,
  EVENT_BREW_UNDEFINED_891,
  EVENT_BREW_UNDEFINED_892,
  EVENT_BREW_UNDEFINED_893,
  EVENT_BREW_UNDEFINED_894,
  EVENT_BREW_UNDEFINED_895,

  EVENT_WCDMA_PS_DATA_RATE,                  /* 2 byte payload */
  EVENT_GSM_TO_WCDMA_RESELECT_END,           /* 5 byte payload */
  EVENT_PZID_HAI_ENABLED,                    /* No payload*/
  EVENT_PZID_HAI_DISABLED,                   /* No payload*/
  EVENT_GSM_TO_WCDMA_HANDOVER_START,         /* 4 byte payload */
  EVENT_WCDMA_RRC_MODE,                      /* 1 byte payload */
  EVENT_WCDMA_L1_ACQ_SUBSTATE,               /* 1 byte payload */
  EVENT_WCDMA_PHYCHAN_CFG_CHANGED,           /* 1 byte payload */

  EVENT_QTV_CLIP_STARTED,                    /* 7 byte payload */
  EVENT_QTV_CLIP_ENDED,                      /* 5 byte payload */
  EVENT_QTV_SDP_PARSER_REJECT,               /* No payload */
  EVENT_QTV_CLIP_PAUSE,                      /* 4 byte payload */
  EVENT_QTV_CLIP_REPOSITIONING,              /* 4 byte payload */
  EVENT_QTV_CLIP_ZOOM_IN,                    /* No payload */
  EVENT_QTV_CLIP_ZOOM_OUT,                   /* No payload */
  EVENT_QTV_CLIP_ROTATE,                     /* 4 byte payload */
  EVENT_QTV_CLIP_PAUSE_RESUME,               /* 4 byte payload */
  EVENT_QTV_CLIP_REPOSITION_RESUME,          /* 4 byte payload */
  EVENT_QTV_DSP_INIT,                        /* No payload */
  EVENT_QTV_STREAMING_SERVER_URL,            /* 22 byte payload */
  EVENT_QTV_SERVER_PORTS_USED,               /* 4 byte payload */
  EVENT_QTV_USING_PROXY_SERVER,              /* 6 byte payload */
  EVENT_QTV_STREAMER_STATE_IDLE,             /* No payload */
  EVENT_QTV_STREAMER_STATE_CONNECTING,       /* No payload */
  EVENT_QTV_STREAMER_STATE_SETTING_TRACKS,   /* No payload */
  EVENT_QTV_STREAMER_STATE_STREAMING,        /* No payload */
  EVENT_QTV_STREAMER_STATE_PAUSED,           /* No payload */
  EVENT_QTV_STREAMER_STATE_SUSPENDED,        /* No payload */
  EVENT_QTV_STREAMER_CONNECTED,              /* No payload */
  EVENT_QTV_STREAMER_INITSTREAM_FAIL,        /* No payload */
  EVENT_QTV_BUFFERING_STARTED,               /* 5 byte payload */
  EVENT_QTV_BUFFERING_ENDED,                 /* 5 byte payload */
  EVENT_QTV_CLIP_FULLSCREEN,                 /* No payload */
  EVENT_QTV_PS_DOWNLOAD_STARTED,             /* 8 byte payload */
  EVENT_QTV_PSEUDO_STREAM_STARTED,           /* No Payload */
  EVENT_QTV_PS_PLAYER_STATE_PSEUDO_PAUSE,    /* No payload */
  EVENT_QTV_PS_PLAYER_STATE_PSEUDO_RESUME,   /* 4 byte payload */
  EVENT_QTV_PARSER_STATE_READY,              /* 14 byte payload */
  EVENT_QTV_FRAGMENT_PLAYBACK_BEGIN,         /* 2 byte payload */
  EVENT_QTV_FRAGMENT_PLAYBACK_COMPLETE,      /* 2 byte payload */
  EVENT_QTV_PARSER_STATE_PSEUDO_PAUSE,       /* No payload */
  EVENT_QTV_PLAYER_STATE_PSEUDO_PAUSE,       /* No payload */
  EVENT_QTV_PARSER_STATE_PSEUDO_RESUME,      /* 4 byte payload */
  EVENT_QTV_PLAYER_STATE_PSEUDO_RESUME,      /* 4 byte payload */
  EVENT_QTV_FRAGMENTED_FILE_DECODE_START,    /* 2 byte payload */
  EVENT_QTV_FRAGMENTED_FILE_END_SUCCESS,     /* 2 byte payload */
  EVENT_QTV_DOWNLOAD_DATA_REPORT,            /* 4 byte payload */
  EVENT_QTV_VDEC_DIAG_DECODE_CALLBACK,       /* 5 byte payload */
  EVENT_QTV_URL_PLAYED_IS_MULTICAST,         /* No payload */
  EVENT_QTV_VDEC_DIAG_STATUS,                /* 4 byte payload */
  EVENT_QTV_STREAMING_URL_OPEN,              /* 4 byte payload */
  EVENT_QTV_STREAMING_URL_OPENING,           /* No payload */
  EVENT_QTV_CLIP_ENDED_VER2,                 /* 13 byte payload */
  EVENT_QTV_SILENCE_INSERTION_STARTED,       /* No payload */
  EVENT_QTV_SILENCE_INSERTION_ENDED,         /* 8 byte payload */
  EVENT_QTV_AUDIO_CHANNEL_SWITCH_FRAME,      /* 8 byte payload */
  EVENT_QTV_FIRST_VIDEO_FRAME_RENDERED,      /* No payload */
  EVENT_QTV_FIRST_VIDEO_I_FRAME_RENDERED,    /* No payload */
  EVENT_QTV_SDP_SELECTED,                    /* No payload */
  EVENT_QTV_DIAG_PLAYER_STATUS,              /* 12 byte payload */
  EVENT_QTV_SILENCE_INSERTION_DURATION,      /* 4 byte payload */
  EVENT_QTV_UNDEFINED_957,
  EVENT_QTV_UNDEFINED_958,
  EVENT_QTV_UNDEFINED_959,
  EVENT_QTV_UNDEFINED_960,
  EVENT_QTV_UNDEFINED_961,
  EVENT_QTV_UNDEFINED_962,
  EVENT_QTV_UNDEFINED_963,
  EVENT_QTV_UNDEFINED_964,
  EVENT_QTV_UNDEFINED_965,
  EVENT_QTV_UNDEFINED_966,
  EVENT_QTV_UNDEFINED_967,

  EVENT_DS_SETS_ARM_CLOCK_FASTER,      /* No payload */
  EVENT_DS_SETS_ARM_CLOCK_SLOWER,      /* No payload */

  EVENT_SMS_STATISTICS,                /* 2 byte payload */
  EVENT_SM_PDP_STATE,                  /* 4 byte payload */
  EVENT_MVS_STATE,                     /* 2 byte payload */

  EVENT_SECSSL,                        /* 16 byte payload */
  EVENT_SECTEST,                       /* 16 byte payload */
  EVENT_SECVPN,                        /* 16 byte payload */
  EVENT_SECCRYPT,                      /* 16 byte payload */
  EVENT_SECCRYPT_CMD,                  /* 16 byte payload */

  EVENT_SEC_RESERVED_978,              /* unknown payload */
  EVENT_SEC_RESERVED_979,              /* unknown payload */
  EVENT_SEC_RESERVED_980,              /* unknown payload */
  EVENT_SEC_RESERVED_981,              /* unknown payload */

  EVENT_ARM_CLK_FREQUENCY_CHANGE,      /* 12 byte payload */
  EVENT_ADSP_CLK_FREQUENCY_CHANGE,     /* 4 byte payload */
  EVENT_MDSP_CLK_FREQUENCY_CHANGE,     /* 4 byte payload */

  EVENT_CELL_CHANGE_INDICATION,        /* 1 byte payload */
  EVENT_CB_STATE_CHANGE,               /* 4 byte payload */
  EVENT_SMSCB_L1_STATE_CHANGE,         /* 3 byte payload */
  EVENT_SMSCB_L1_COLLISION,            /* 1 byte payload */
  EVENT_WMS_SEARCH_REQUEST,            /* 1 byte payload */
  EVENT_CM_GET_PASSWORD_IND,           /* 2 byte payload */
  EVENT_CM_PASSWORD_AUTHENTICATION_STATUS, /* 2 byte payload */
  EVENT_CM_USS_RESPONSE_NOTIFY_IND,    /* 3 byte payload */
  EVENT_CM_USS_CONF,                   /* 4 byte payload */
  EVENT_CM_RELEASE_USS_IND,            /* 4 byte payload */
  EVENT_CM_FWD_AOC_IND,                /* 1 byte payload */
  EVENT_PZID_ID,                       /* 2 byte payload */
  EVENT_PZID_HT_VALUE,                 /* 9 byte payload */
  EVENT_PZID_EXISTS_IN_LIST,           /* 1 byte payload */
  EVENT_GSDI_GET_FILE_ATTRIBUTES,      /* 6 byte payload */
  EVENT_GSDI_SIM_READ,                 /* 6 byte payload */
  EVENT_GSDI_SIM_WRITE,                /* 6 byte payload */
  EVENT_GSDI_GET_PIN_STATUS,           /* 8 byte payload */
  EVENT_GSDI_VERIFY_PIN,               /* 7 byte payload */
  EVENT_GSDI_UNBLOCK_PIN,              /* 7 byte payload */
  EVENT_GSDI_DISABLE_PIN,              /* 7 byte payload */
  EVENT_GSDI_ENABLE_PIN,               /* 7 byte payload */
  EVENT_GSDI_SIM_INCREASE,             /* 6 byte payload */
  EVENT_GSDI_EXECUTE_APDU_REQ,         /* 6 byte payload */
  EVENT_SEG_UPM_ADDR_MISMATCH,         /* 2 byte payload */
  EVENT_WCDMA_PRACH,                   /* 3 byte payload */
  EVENT_GSDI_SELECT,                   /* 6 byte payload */
  EVENT_WCDMA_RAB_RATE_RECONFIG,       /* 2 byte payload */
  EVENT_WCDMA_RLC_RESETS,              /* 3 byte payload */
  EVENT_WCDMA_RLC_OPEN_CLOSE,          /* 2 byte payload */
  EVENT_WCDMA_RLC_MRW,                 /* 3 byte payload */
  EVENT_QVP_APP_PROCESS_EVENT,         /* 2 byte payload */
  EVENT_QVP_APP_STATE_CHANGED_EVENT,   /* 2 byte payload */
  EVENT_QVP_APP_CALL_CONNECTED_EVENT,  /* 1 byte payload */
  EVENT_GSDI_CARD_EVENT_NOTIFICATION,  /* 4 byte payload */
  EVENT_CM_DATA_AVAILABLE,             /* 1 byte payload */
  EVENT_CM_DS_INTERRAT_STATE,          /* 2 byte payload */
  EVENT_MM_STATE,                      /* 2 byte payload */
  EVENT_GMM_STATE,                     /* 2 byte payload */
  EVENT_PLMN_INFORMATION,              /* 8 byte payload */
  EVENT_COREAPP_SET_VOICE_PRIVACY,     /* 5 byte payload */
  EVENT_COREAPP_GET_VOICE_PRIVACY,     /* 5 byte payload */
  EVENT_HARD_HANDOFF_LONG_CODE_MASK_CHANGE, /* 14 byte payload */
  EVENT_VCTCXO_FREEZE,                 /* payload */
  EVENT_VCTCXO_UNFREEZE,               /* payload */
  EVENT_SMS_SLOT_WAKEUP_V2,            /* 2 byte payload */
  EVENT_QVP_RCVD_FIRST_VIDEO_FRAME,    /* no payload */
  EVENT_QVP_CALL_RELEASED,             /* 8 byte payload */
  EVENT_CB_SMS_NOTIFY,                 /* 10 byte payload */
  EVENT_GPS_PDSM_EVENT_REPORT,         /* 6 byte payload */
  EVENT_LONG_CODE_MASK_CHANGED,        /* 2 byte payload */
  EVENT_DS707,                         /* 5 byte payload */

  EVENT_GSDI_ACTIVATE_FEATURE_IND,     /* 8 byte payload */
  EVENT_GSDI_DEACTIVATE_FEATURE_IND,   /* 8 byte payload */
  EVENT_GSDI_GET_FEATURE_IND,          /* 11 byte payload */
  EVENT_GSDI_SET_FEATURE_DATA,         /* 6 byte payload */
  EVENT_GSDI_UNBLOCK_FEATURE_IND,      /* 8 byte payload */
  EVENT_GSDI_GET_CONTROL_KEY,          /* 6 byte payload */
  EVENT_GSDI_OTA_DEPERSO,              /* 26 byte payload */
  EVENT_GSDI_GET_PERM_FEATURE_IND,     /* 11 byte payload */
  EVENT_GSDI_PERM_DISBALE_FEATURE_IND, /* 8 byte payload */
  EVENT_GSM_L1_VOCODER_INITIALIZE,     /* TBD */
  EVENT_GSM_L1_ALIGN_VFR,              /* TBD */
  EVENT_GSM_L1_VOCODER_ENABLED,        /* TBD */
  EVENT_HDR_AMAC_PERSISTENCE_FAILED,   /* no payload */
  EVENT_HDR_AMAC_PERSISTENCE_PASSED,   /* no payload */

  /* 20 events reserved for MediaFLO */
  EVENT_MFLO_STREAM_STATE,                /* 12 byte payload */
  EVENT_MFLO_CONTROL_CHANNEL_STATE_CHANGE,/* 12 byte payload */
  EVENT_MFLO_SLEEP_STATE_CHANGE,          /* 12 byte payload */
  EVENT_MFLO_NETWORK_STATE_CHANGE,        /* 20 byte payload */
  EVENT_MFLO_TRANS_STATE,                 /* 12 byte payload */
  EVENT_MFLO_OIS_STATE,                   /* 16 byte payload */
  EVENT_MFLO_RXD_STATE,                   /* 12 byte payload */
  EVENT_MFLO_HIPRI_STATE_CHANGE,          /* 8  byte payload */
  EVENT_MFLO_CAS_STATE,                   /* 12 byte payload */
  EVENT_MFLO_ACQ_STATE,                   /* 8  byte payload */
  EVENT_MFLO_OSCAR_FRAME_DECODED,         /* 8  byte payload */
  EVENT_MFLO_CHAN_SWITCH_RENDERED,        /* 16 byte payload */
  EVENT_MFLO_OSCAR_DEC_EXCEPTION_DETECTED,/* 4  byte payload */
  EVENT_MFLO_MFN_SUBSTATE,                /* 8  byte payload */
  EVENT_MFLO_MFN_STATE,                   /* 12  byte payload */
  EVENT_MFLO_MFN_VERTICAL_HANDOFF,        /* 16  byte payload */
  EVENT_MFLO_MFN_ACQ_STATE,               /* 10  byte payload */
  EVENT_MFLO_FLOW_STATUS,                 /* 12  byte payload */
  EVENT_MFLO_NETWORK_STATUS,              /* 12  byte payload */
  EVENT_MFLO_UNDEFINED_1070,

  EVENT_CM_LCS_MOLR_CONF,                 /* 1 byte payload */
  EVENT_PPP_NETMODEL,                     /* 7 byte payload */
  EVENT_CAMERA_PROFILING,                 /* 1 byte payload */
  EVENT_MAC_HS_T1_EXPIRY,                 /* 2 byte payload */
  EVENT_ASYNC_DS707,                      /* 4 byte payload */
  EVENT_PKT_DS707,                        /* 4 byte payload */
  EVENT_GPRS_TIMER_EXPIRY,                /* 1 byte payload */
  EVENT_GPRS_MAC_IDLE_IND,                /* no payload */
  EVENT_GPRS_PACKET_CHANNEL_REQUEST,      /* 1 byte payload */
  EVENT_GPRS_ACCESS_REJECT,               /* 1 byte payload */
  EVENT_GPRS_PACKET_RESOURCE_REQUEST,     /* 1 byte payload */
  EVENT_GPRS_PACKET_UPLINK_ASSIGNMENT,    /* 2 byte payload */
  EVENT_GPRS_PACKET_DOWNLINK_ASSIGNMENT,  /* 2 byte payload */
  EVENT_PACKET_TIMESLOT_RECONFIGURE,      /* 3 byte payload */
  EVENT_GPRS_TBF_RELEASE,                 /* 1 byte payload */
  EVENT_GPRS_CELL_CHANGE_ORDER,           /* 1 byte payload */
  EVENT_GPRS_CELL_CHANGE_FAILURE,         /* 1 byte payload */
  EVENT_GSM_AMR_RATSCCH_REQ,              /* 1 byte payload */
  EVENT_GSM_AMR_RATSCCH_RSP,              /* 1 byte payload */
  EVENT_SD_SRV_IND_HYBR_WLAN,
  EVENT_SD_EVENT_ACTION_HYBR_WLAN,
  EVENT_GPS_PD_DEMOD_SESS_START,          /* 5 byte payload */
  EVENT_GPS_PD_DEMOD_SESS_END,            /* 1 byte payload */
  EVENT_GPS_SV_ACQUIRED,                  /* 4 byte payload */
  EVENT_GPS_SV_BIT_EDGE_FOUND,            /* 4 byte payload */
  EVENT_GPS_DEMOD_STARTED,                /* 4 byte payload */
  EVENT_GPS_DEMOD_OUT_OF_LOCK,            /* 3 byte payload */
  EVENT_GPS_DEMOD_STOPPED,                /* 3 byte payload */
  EVENT_GPS_DEMOD_PREAMBLE_FOUND,         /* 3 byte payload */
  EVENT_GPS_DEMOD_FRAME_SYNC_STATUS,      /* 4 byte payload */
  EVENT_GPS_DEMOD_SUBFRAME,               /* 6 byte payload */
  EVENT_GPS_DEMOD_EPHEMERIS_COMPLETE,     /* 1 byte payload */
  EVENT_GPS_DEMOD_ALMANAC_COMPLETE,       /* 1 byte payload */
  EVENT_GPS_DEMOD_BIT_EDGE_STATUS,        /* 4 byte payload */
  EVENT_RAT_CHANGE,                       /* 1 byte payload */
  EVENT_REGISTRATION_SUPPRESSED,          /* 1 byte payload */
  EVENT_HDR_RUP_DIST_BASED_REG,           /* 3 byte payload */
  EVENT_GPS_DIAG_APP_TRACKING_START,      /* 4 byte payload */
  EVENT_GPS_DIAG_APP_TRACKING_END,        /* 12 byte payload */
  EVENT_GPS_DIAG_APP_POSITION_SUCCESS,    /* 16 byte payload */
  EVENT_GPS_DIAG_APP_POSITION_FAILURE,    /* 6 byte payload */
  EVENT_GSM_AMR_MULTIRATE_IE,             /* 9 byte payload */
  EVENT_EPZID_HYSTERESIS_ENABLED,         /* no payload */
  EVENT_EPZID_HYSTERESIS_DISABLED,        /* no payload */
  EVENT_EPZID_HT_STARTED,                 /* 10 byte payload */
  EVENT_EPZID_HT_EXPIRED,                 /* 10 byte payload */
  EVENT_HDR_BCMCS_FLOW_STATE_CHANGE,      /* 6 byte payload */
  EVENT_HDR_LMAC_UPDATE_BC_STATUS,        /* 1 byte payload */
  EVENT_DS_CAM_TIMER,                     /* 5 byte payload */
  EVENT_DS_RDUD_TIMER,                    /* 5 byte payload */
  EVENT_DS_CTA_TIMER,                     /* 8 bytes payload */
  EVENT_DS_FALLBACK,                      /* 1 byte payload */
  EVENT_DS3G_CAM_FLOW_CTRL_TIMER,         /* 5 byte payload */
  EVENT_GPS_JAMMER_DETECTION_TEST_PASS,   /* no payload */
  EVENT_GPS_JAMMER_DETECTION_TEST_FAILURE,/* 8 byte payload */
  EVENT_JAMMER_DETECT_NOISE_STATS,        /* 8 byte payload */
  EVENT_GPS_GET_PARAM,                    /* 8 byte payload */
  EVENT_GPS_GET_PARAM_BS_INFO,            /* 18 byte payload */
  EVENT_HS_SERVING_CELL_CHANGE,           /* 8 byte payload */
  EVENT_HS_DSCH_STATUS,                   /* 1 byte payload */
  EVENT_SMGMM_REQUEST_SENT,               /* 2 byte payload */
  EVENT_SMGMM_REJECT_RECEIVED,            /* 2 byte payload */
  EVENT_LINUX_APP_STOP,                   /* 8 byte payload */
  EVENT_GPS_PD_CME_SESSION_START,         /* 1 byte payload */
  EVENT_GPS_PD_CME_SESSION_END,           /* 1 byte payload */

  /* 20 events reserved for QVideoPhone */
  EVENT_SIP_REGISTER_START,               /* 4 byte payload */
  EVENT_SIP_REGISTER_DONE,                /* 1 byte payload */
  EVENT_SIP_CALL_SETUP_START,             /* 5 byte payload */
  EVENT_SIP_CALL_SETUP_DONE,              /* No payload */
  EVENT_SIP_CALL_RELEASE_START,           /* 5 byte payload */
  EVENT_SIP_CALL_RELEASE_DONE,            /* No payload */
  EVENT_AUDIO_FRAME_SENT_TO_DECODER,      /* 12 byte payload */
  EVENT_VIDEO_FRAME_SENT_TO_DECODER,      /* 12 byte payload */
  EVENT_DEC_RENDER_FRAME,                 /* 8 byte payload */
  EVENT_DEC_RENDER_DONE,                  /* No payload */
  EVENT_DEC_START_DECODING,               /* 4 byte payload */
  EVENT_DEC_FRAME_DECODED,                /* 4 byte payload */
  EVENT_V_ENCODED,                        /* 13 byte payload */
  EVENT_DEC_START_DECODING_EXT,           /* 8 byte payload */
  EVENT_DEC_FRAME_DECODED_EXT,            /* 8 byte payload */
  EVENT_QVIDEOPHONE_UNDEFINED_1151,
  EVENT_QVIDEOPHONE_UNDEFINED_1152,
  EVENT_QVIDEOPHONE_UNDEFINED_1153,
  EVENT_QVIDEOPHONE_UNDEFINED_1154,
  EVENT_QVIDEOPHONE_UNDEFINED_1155,

  EVENT_GPS_CME_POS_REQ,                  /* no payload */
  EVENT_GPS_CME_FIX_START,                /* no payload */
  EVENT_GPS_CME_FIX_END,                  /* no payload */
  EVENT_GPS_SEED_CLM,                     /* 12 byte payload */
  EVENT_GPS_SEED_SID,                     /* 10 byte payload */
  EVENT_GPS_SEED_SL,                      /* 11 byte payload */
  EVENT_GPS_SEED_GET,                     /* 13 byte payload */

  EVENT_HDR_OVHD_BC_MSG_RX,               /* no payload */
  EVENT_HDR_OVHD_T_BC_SUPERVISION,        /* no payload */
  EVENT_HDR_LMAC_SET_BCMCS_PAGE_CYCLE,    /* 1 byte payload */
  EVENT_HDR_HMP_SESSION_CLOSED,           /* 2 byte payload */

  EVENT_WLAN_CP,                          /* 15 byte payload */
  EVENT_ARP,                              /* 12 byte payload */
  EVENT_DHCP,                             /* 10 byte payload */
  EVENT_WLAN_WPA,                         /* 7 byte payload */
  EVENT_EAP,                              /* 7 byte payload */
  EVENT_LAN_1X,                           /* 7 byte payload */

  EVENT_CAMERA_SVCS_START,                /* no payload */
  EVENT_CAMERA_SVCS_STOP,                 /* no payload */

  EVENT_BCMCS_SRVC_AVAILABLE,             /* 1 byte payload */
  EVENT_BCMCS_SRVC_LOST,                  /* 1 byte payload */
  EVENT_BCMCS_FLOW_REGISTERED,            /* 18 byte payload */
  EVENT_BCMCS_FLOW_DEREGISTERED,          /* 18 byte payload */
  EVENT_BCMCS_FLOW_STATUS_CHANGED,        /* 19 byte payload */

  EVENT_CAMERA_SVCS_X,                    /* 2 byte payload */
  EVENT_CM_CALL_EVENT_ORIG_THR,           /* 3 byte payload */

  EVENT_VFE_MSG_CONFIG_COMPLETE,          /* No payload */
  EVENT_VFE_MSG_IDLE_COMPLETE,            /* No payload */
  EVENT_VFE_MSG_UPDATE_COMPLETE,          /* No payload */
  EVENT_VFE_MSG_AE_AWB_STATS,             /* No payload */
  EVENT_DSP_VIDEO_ENC_DOWNLOAD_DONE,      /* No payload */
  EVENT_DSP_VIDEO_ENC_SELECTION_DONE,     /* No payload */
  EVENT_DSP_VIDEO_ENC_CONFIG_DONE,        /* No payload */
  EVENT_DSP_VIDEO_ENC_FRAME_DONE,         /* No payload */

  EVENT_HDR_OVHD_BCMCS_CHAN_CHANGE,       /* 6 byte payload */

  EVENT_QVS_REGISTER_START,               /* 4 byte payload */
  EVENT_QVS_REGISTER_DONE,                /* 4 byte payload */
  EVENT_QVS_REGISTER_FAILED,              /* No payload */
  EVENT_QVS_CALL_SETUP_START,             /* 5 byte payload */
  EVENT_QVS_CALL_SETUP_DONE,              /* No payload */
  EVENT_QVS_CALL_SETUP_FAILED,            /* No payload */
  EVENT_QVS_CALL_RELEASE_START,           /* 5 byte payload */
  EVENT_QVS_CALL_RELEASE_DONE,            /* No payload */
  EVENT_QVS_CALL_RELEASE_FAILED,          /* No payload */

  EVENT_CAMCORDER_START_RECORD,           /* 9 byte payload */
  EVENT_CAMCORDER_START_TRANSCODE,        /* 6 byte payload */
  EVENT_CAMCORDER_FRAME_DROP,             /* No payload */
  EVENT_CAMCORDER_AUDIODUB,               /* 2 byte payload */

  EVENT_PSMM_SENT,                        /* 16 byte payload */
  EVENT_GPS_PD_FALLBACK_MODE,             /* 3 byte payload */

  EVENT_PEAP,                             /* 4 byte payload */
  EVENT_TTLS,                             /* 3 byte payload */
  EVENT_TLS,                              /* 2 byte payload */

  EVENT_WCDMA_TO_WCDMA_RESELECTION_VER2_START, /* 7 byte payload */

  EVENT_EUL_RECONFIG_OR_ASU,              /* 10 byte payload */
  EVENT_EUL_SERVING_CELL_CHANGE,          /* 4 byte payload */
  EVENT_EUL_PHYSICAL_LAYER_RECONFIG,      /* 10 byte payload */

  EVENT_DRM_ROAP_TRIGGER_RECIEVED,        /* 1 byte payload */
  EVENT_DRM_ROAP_PROTOCOL_START,          /* 1 byte payload */
  EVENT_DRM_ROAP_REQUEST,                 /* 1 byte payload */
  EVENT_DRM_ROAP_REQUEST_EXTENSION,       /* 2 byte payload */
  EVENT_DRM_ROAP_RESPONSE,                /* 2 byte payload */
  EVENT_DRM_ROAP_RESPONSE_EXTENSION,      /* 2 byte payload */
  EVENT_DRM_ROAP_RI_CONTEXT,              /* 1 byte payload */
  EVENT_DRM_ROAP_ERROR,                   /* 1 byte payload */
  EVENT_DRM_ROAP_RSP_VALIDATION,          /* 3 byte payload */
  EVENT_DRM_ROAP_PROTOCOL_END,            /* 2 byte payload */

  EVENT_DS_WMK_ALLOCATED,                 /* 16 byte payload */
  EVENT_DS_WMK_DEALLOCATED,               /* 8  byte payload */
  EVENT_DS_WMK_FLUSHED,                   /* 12 byte payload */
  EVENT_DS_WMK_FLOW_ENABLED,              /* 12 byte payload */
  EVENT_DS_WMK_FLOW_DISABLED,             /* 12 byte payload */

  EVENT_HDR_IDLE_SET_SLEEP_DURATION,      /* 2 byte payload */
  EVENT_HDR_SCM_SESSION_CHANGED,          /* 1 byte payload */

  EVENT_UMTS_TO_CDMA_DATA_HANDOVER,       /* No payload */ 
  EVENT_UMTS_TO_CDMA_VOICE_HANDOVER,      /* 18 byte payload */ 

  EVENT_MO_SMS_RETRY_ATTEMPT,             /* 14 byte payload */ 

  EVENT_HDR_LMAC_UPDATE_QSM_STATUS,       /* 1 byte payload */ 

  EVENT_CM_CELL_SRV_IND,                  /* 5 byte payload */ 

  EVENT_RLP_NAK_ABORT,                    /* 9 byte payload */ 

  EVENT_DRM_RIGHTS_OPERATION,             /* 2 byte payload */

  EVENT_DS_RESV_MSG_SENT_REV_FLOWS,       /* 15 byte payload */
  EVENT_DS_RESV_MSG_SENT_FWD_FLOWS,       /* 15 byte payload */
  EVENT_DS_RESV_RESP_SUCCESS_RECD,        /* 4 byte payload */
  EVENT_DS_RESV_RESP_FAILURE_RECD,        /* 4 byte payload */

  EVENT_GPS_PD_COMM_FAILURE,              /* 2 byte payload */
  EVENT_GPS_PD_COMM_DONE,                 /* No payload */
  EVENT_GPS_PD_EVENT_END,                 /* 1 byte payload */
  EVENT_GPS_PA_EVENT_CALLBACK,            /* 1 byte payload */
  EVENT_GPS_PD_CMD_ERR_CALLBACK,          /* 2 byte payload */
  EVENT_GPS_PA_CMD_ERR_CALLBACK,          /* 2 byte payload */

  EVENT_GPS_LM_ENTER_SA_RF_VERIF,         /* 1 byte payload */
  EVENT_GPS_LM_EXIT_SA_RF_VERIF,          /* 1 byte payload */
  EVENT_GPS_LM_ERROR_SA_RF_VERIF,         /* 1 byte payload */
  EVENT_GPS_LM_PD_COMPLETE,               /* No payload */
  EVENT_GPS_LM_IQ_TEST_COMPLETE,          /* No payload */

  EVENT_PM_APP_OTG_INIT,                  /* No payload */
  EVENT_PM_APP_OTG_RESET,                 /* No payload */
  EVENT_PM_APP_OTG_ACQUIRE_BUS_REQ,                         /* 2 byte payload */
  EVENT_PM_APP_OTG_RELINQUISH_BUS_REQ,                      /* No payload */
  EVENT_PM_APP_OTG_SUSPEND,                           /* No payload */
  EVENT_PM_APP_OTG_RESUME,                              /* No payload */
  EVENT_PM_APP_OTG_DEVICE_ATTACHED,                            /* 1 byTe payload */
  EVENT_PM_APP_OTG_DEVICE_DETACHED,                        /* No payload */
  EVENT_PM_APP_OTG_HOST_MODE_REM_PERI_DIS,                     /* No payload */
  EVENT_PM_APP_OTG_PERI_MODE_PREPARE_FOR_REM_HOST_WAKEUP_SIG,  /* No payload */
  EVENT_PM_APP_OTG_PERI_MODE_REM_HOST_WAKEUP_SIG_DONE,   /* No payload */
  EVENT_PM_APP_OTG_SET_REM_WAKEUP_CAPABILITY,              /* 1 byte payload */
  EVENT_PM_APP_OTG_OPERATIONAL_ERROR,                          /* 1 byte Payload */
  EVENT_PM_APP_OTG_CONFIGURE_USB_POWER_CONSUMER,         /* No payload */
  EVENT_PM_APP_OTG_SET_USB_POWER_CONSUMPTION_REQUIREMENT,      /* 1 byte payload */
  EVENT_PM_APP_OTG_PERI_MODE_PROCESS_USB_POWER_LINE_CONT_REQ,  /* 1 byte payload */
  EVENT_PM_APP_OTG_PERI_MODE_SET_REM_A_DEV_INFO,               /* 3 byte payload */
  EVENT_PM_APP_OTG_STATE_TRANSITION,                           /* 2 byte payload */

  EVENT_DTV_TABLE_ACQ_SUCCESS,                                 /* 10 byte payload */  
  EVENT_DTV_TABLE_ACQ_FAIL,                                    /* 5 byte payload */  
  EVENT_DTV_DVBH_SEL_PLTFM_REQ_RCVD,                           /* 4 byte payload */
  EVENT_DTV_DVBH_PLTFM_ACQ_SUCCESS,                            /* 4 byte payload */
  EVENT_DTV_DVBH_PLTFM_ACQ_FAIL,                               /* 4 byte payload */  
  EVENT_DTV_DVBH_TBL_MGR_STATE_CHANGED,                        /* 2 byte payload */
  EVENT_DTV_DVBH_CE_STATE_CHANGED,                             /* 2 byte payload */
  EVENT_DTV_DVBH_MCAST_JOIN_REQ_RCVD,                          /* 18 byte payload */
  EVENT_DTV_DVBH_MCAST_LEAVE_REQ_RCVD,                         /* 18 byte payload */
  EVENT_DTV_DVBH_INIT_REQ_RCVD,                                /* No payload */
  EVENT_DTV_DVBH_MCAST_JOIN_SUCCESS,                           /* 18 byte payload */
  EVENT_DTV_DVBH_MCAST_JOIN_FAILURE,                           /* 18 byte payload */
  EVENT_DTV_DVBH_MCAST_LEAVE_SUCCESS,                          /* 18 byte payload */
  EVENT_DTV_DVBH_MCAST_LEAVE_FAILURE,                          /* 18 byte payload */
  EVENT_DTV_DVBH_INIT_SUCCESS,                                 /* No payload */
  EVENT_DTV_DVBH_INIT_FAILURE,                                 /* No payload */

  EVENT_GPS_LM_SESSION_START,             /* 1 byte payload */
  EVENT_GPS_LM_SESSION_END,               /* No payload */
  EVENT_GPS_LM_FIX_REQUEST_START,         /* No payload */
  EVENT_GPS_LM_FIX_REQUEST_END,           /* No payload */
  EVENT_GPS_LM_PRM_REQUEST_START,         /* No payload */
  EVENT_GPS_LM_PRM_REQUEST_END,           /* No payload */
  EVENT_GPS_LM_SESSION_CONTINUE,          /* 1 byte payload */
  EVENT_GPS_LM_FIX_REQUEST_CONTINUE,      /* No payload */
  EVENT_GPS_LM_PRM_REQUEST_CONTINUE,      /* No payload */
  EVENT_GPS_LM_PPM_REQUEST_CONTINUE,      /* No payload */
  EVENT_GPS_LM_AIDING_DATA_RECEIVED,      /* 1 byte payload */
  EVENT_GPS_LM_RC_ON_TIMER_TIMEOUT,       /* No payload */
  EVENT_GPS_LM_SHUT_OFF_TIMER_TIMEOUT,    /* No payload */
  EVENT_GPS_LM_MGP_ON,                    /* No payload */
  EVENT_GPS_LM_MGP_IDLE,                  /* No payload */
  EVENT_GPS_LM_MGP_OFF,                   /* No payload */

  EVENT_DRM_RO_CONSUMPTION_VALIDATION,    /* 2 byte payload */
  EVENT_DRM_RO_INSTALLATION_VALIDATION,   /* 2 byte payload */

  EVENT_FLUTE_FDT_INST_RCVD,              /* 8 byte payload */
  EVENT_FLUTE_FDT_INST_RCV_FAIL,          /* 9 byte payload */
  EVENT_FLUTE_FDT_INST_EXPIRED,           /* 8 byte payload */
  EVENT_FLUTE_JOIN_SESSION_REQ_RCVD,      /* 24 byte payload */
  EVENT_FLUTE_LEAVE_SESSION_REQ_RCVD,     /* 4 byte payload */
  EVENT_FLUTE_SESSION_CLOSED,             /* 5 byte payload */ 
  EVENT_FLUTE_SESSION_CLOSED_BY_APP,      /* 4 byte payload */ 
  EVENT_FLUTE_B_FLAG_RCVD,                /* 8 byte payload */ 
  EVENT_FLUTE_GET_FILE_REQUEST_RCVD,      /* 8 byte payload */ 
  EVENT_FLUTE_JOIN_SESSION_RSP,           /* 6 byte payload */ 
  EVENT_FLUTE_FILE_STATUS_RSP,            /* 16 byte payload */ 
  EVENT_FLUTE_CANCEL_FILE_REQ_RCVD,       /* 8 byte payload */

  EVENT_DTV_DVBH_DEINIT_REQ_RCVD,         /* No payload */
  EVENT_DTV_DVBH_DEINIT_SUCCESS,          /* No payload */
  EVENT_DTV_DVBH_DEINIT_FAILURE,          /* No payload */

  EVENT_CONTENT_INSTALL_BEGIN,            /* No payload */
  EVENT_CONTENT_INSTALL_COMPLETE,         /* 2 byte payload */
  EVENT_CONTENT_RETRIEVAL_BEGIN ,         /* No payload */
  EVENT_CONTENT_RETRIEVAL_COMPLETE,       /* 4 byte payload */
  EVENT_CONTENT_BACKUP_BEGIN,             /* No payload */
  EVENT_CONTENT_BACKUP_COMPLETE,          /* 2 byte payload */
  EVENT_CONTENT_FWD_BEGIN,                /* No payload */
  EVENT_CONTENT_FWD_COMPLETE,             /* 2 byte payload */

  EVENT_HARD_HANDOFF_VOIP_TO_CDMA,        /* 14 byte payload */

  EVENT_EAP_SIM_AKA,                      /* 14 byte payload */
  EVENT_WLAN_CP_MEAS,                     /* 16 byte payload */
  EVENT_WLAN_CP_HO,                       /* 13 byte payload */
  EVENT_WLAN_CP_11D,                      /* 9  byte payload */
  EVENT_WLAN_MC,                          /* 2  byte payload */

  EVENT_SVG_CONTENT_SET,                  /* 2  byte payload */
  EVENT_SVG_CONTENT_PLAY,                 /* 2  byte payload */
  EVENT_SVG_CONTENT_RESUME,               /* 1  byte payload */
  EVENT_SVG_CONTENT_PAUSE,                /* 1  byte payload */
  EVENT_SVG_CONTENT_STOP,                 /* 1  byte payload */
  EVENT_SVG_CONTENT_USEREVENT,            /* 10 byte payload */
  EVENT_SVG_CONTENT_GETURIDATA,           /* 3  byte payload */
  EVENT_SVG_CONTENT_TRANSFORM,            /* 10 byte payload */
  EVENT_SVG_GET_PARAM,                    /* 3  byte payload */
  EVENT_SVG_SET_PARAM,                    /* 3  byte payload */

  EVENT_WLAN_WPA2,                        /* 5 byte payload */

  EVENT_WCDMA_PSC_SCANNER_STOP,           /* 1 byte payload */

  EVENT_MEDIA_PLAYER_KEYPRESS,            /* 4 byte payload */

  EVENT_WLAN_MC_QOS,                      /* 5 byte payload */

  EVENT_WCDMA_PSC_SCANNER_STATE,          /* 1 byte payload */

  EVENT_WLAN_CP_ADHOC,                    /* 16 byte payload */

  EVENT_DMB_STACK_SHUTDOWN,               /* 4 byte payload */
  EVENT_DMB_TUNE_DONE_SUCCESS,            /* 4 byte payload */
  EVENT_DMB_TUNE_DONE_FAILURE,            /* 4 byte payload */
  EVENT_DMB_SEARCH_DONE,                  /* 4 byte payload */
  EVENT_DMB_SCAN_DONE,                    /* 4 byte payload */
  EVENT_DMB_RECEPTION_INFO_CHANGED,       /* 4 byte payload */
  EVENT_DMB_DMB_GUIDE_CHANGED,            /* 4 byte payload */
  EVENT_DMB_LOCATION_INFO_CHANGED,        /* 4 byte payload */
  EVENT_DMB_LOST_ENSEMBLE,                /* 4 byte payload */
  EVENT_DMB_STREAM_TERMINATED,            /* 4 byte payload */
  EVENT_DMB_STREAM_DATA_AVAILABLE,        /* 4 byte payload */
  EVENT_DMB_RESERVED1,                    /* 4 byte payload */
  EVENT_DMB_RESERVED2,                    /* 4 byte payload */
  EVENT_DMB_RESERVED3,                    /* 4 byte payload */
  EVENT_DMB_RESERVED4,                    /* 4 byte payload */
  EVENT_DMB_RESERVED5,                    /* 4 byte payload */
  EVENT_DMB_RESERVED6,                    /* 4 byte payload */
  EVENT_DMB_RESERVED7,                    /* 4 byte payload */
  EVENT_DMB_RESERVED8,                    /* 4 byte payload */
  EVENT_DMB_RESERVED9,                    /* 4 byte payload */
  EVENT_DMB_RESERVED10,                   /* 4 byte payload */

  EVENT_MOBILEVIEW_RESERVED1,             /* TBD */
  EVENT_MOBILEVIEW_RESERVED2,             /* TBD */
  EVENT_MOBILEVIEW_RESERVED3,             /* TBD */
  EVENT_MOBILEVIEW_RESERVED4,             /* TBD */
  EVENT_MOBILEVIEW_RESERVED5,             /* TBD */

  EVENT_HDR_DOS_MO_DOS_STATUS,            /* 3 byte payload */ 

  EVENT_GPSONEXTRA_START_DOWNLOAD,        /* 4 byte payload */
  EVENT_GPSONEXTRA_END_DOWNLOAD,          /* 4 byte payload */

  EVENT_SNSD_GENERIC,                     /* 8 byte payload */
  EVENT_SNSD_DEVICE_INIT,                 /* 8 byte payload */
  EVENT_SNSD_DEVICE_CONFIGURED,           /* 8 byte payload */
  EVENT_SNSD_EVENT_DATA_READY,            /* 8 byte payload */
  EVENT_SNSD_EVENT_COND_MET,              /* 8 byte payload */
  EVENT_SNSD_DEVICE_DOWN,                 /* 8 byte payload */
  EVENT_SNSD_ERROR,                       /* 8 byte payload */
  
  EVENT_CM_COUNTRY_SELECTED,              /* 2 byte payload */
  EVENT_CM_SELECT_COUNTRY,                /* 7 byte payload */

  EVENT_GPS_DCME_NEW_SV_ADDED_IN_AA,      /* 1 byte payload */
  EVENT_GPS_DCME_SV_REMOVED_FROM_AA,      /* 1 byte payload */

  EVENT_ESG_GET_PROV_LIST_REQ_RCVD,       /* No payload */
  EVENT_ESG_GET_PROV_LIST_REQ_FAIL,       /* No payload */
  EVENT_ESG_PROV_LIST_AVAILABLE,          /* No payload */
  EVENT_ESG_ACQ_REQ_RCVD,                 /* 2 byte payload */
  EVENT_ESG_ACQ_REQ_FAIL,                 /* 2 byte payload */
  EVENT_ESG_STOP_REQ_RCVD,                /* No payload */
  EVENT_ESG_STOP_REQ_FAIL,                /* No payload */
  EVENT_ESG_STOP_COMPLETE,                /* 1 byte payload */

  EVENT_ADC_ONDIE_THERM_READ,             /* 2 byte payload */

  EVENT_CONTENT_NO_VALID_OR_EXPIRED_RIGHTS, /* No Payload */

  EVENT_MOBILEVIEW_RESERVED30,            /* EVENT TO BE REPLACED */
  EVENT_MOBILEVIEW_RESERVED31,            /* EVENT TO BE REPLACED */
  EVENT_MOBILEVIEW_RESERVED32,            /* EVENT TO BE REPLACED */

  EVENT_GPS_DCME_MEAS_CYCLE_START,        /* No payload */ 
  EVENT_GPS_DCME_MEAS_CYCLE_END,          /* No payload */ 
  EVENT_GPS_CME_ENGAGED,                  /* No payload */ 
  EVENT_GPS_CME_NOT_ENGAGED,              /* No payload */ 
  EVENT_GPS_DCME_ENGAGED,                 /* No payload */ 
  EVENT_GPS_DCME_NOT_ENGAGED,             /* No payload */ 

  EVENT_HS_USB_DEVICE_ATTACHED,           /* No payload */
  EVENT_HS_USB_HID_DISCONECT,             /* No payload */
  EVENT_HS_USB_HID_CONNECT,               /* 2 byte payload */
  EVENT_HS_USB_MSD_CONNECT,               /* No payload */
  EVENT_HS_USB_MSD_DISCONECT,             /* No payload */
  EVENT_HS_USB_STACK_SUSPENDED,           /* 1 byte payload */
  EVENT_HS_USB_STACK_RESUMED,             /* 1 byte payload */
  EVENT_HS_USB_ENTER_HOST_MODE,           /* No payload */
  EVENT_HS_USB_OPERATIONAL_ERROR,         /* 2 byte payload */

  EVENT_DTV_L1_ACQ_DONE,                  /* 5 byte payload */
  EVENT_DTV_L1_SCAN,                      /* 4 byte payload */
  EVENT_DTV_L1_ONLINE,                    /* 1 byte payload */
  EVENT_DTV_L1_SNOOZE,                    /* No payload */
  EVENT_DTV_L1_SLEEP,                     /* No payload */
  EVENT_DTV_L1_HANDOFF,                   /* 5 byte payload */
  EVENT_DTV_L1_SIGNAL_LOST,               /* No payload */

  EVENT_IMS_SIP_REGISTRATION_START,       /* 4 byte payload */
  EVENT_IMS_SIP_REGISTER_END,             /* 4 byte payload */
  EVENT_IMS_SIP_DEREGISTER_START,         /* 4 byte payload */
  EVENT_IMS_SIP_DEREGISTER_END,           /* 4 byte payload */
  EVENT_IMS_SIP_SESSION_START,            /* 4 byte payload */
  EVENT_IMS_SIP_SESSION_RINGING,          /* 4 byte payload */
  EVENT_IMS_SIP_SESSION_ESTABLISHED,      /* 4 byte payload */
  EVENT_IMS_SIP_SESSION_TERMINATED,       /* 4 byte payload */
  EVENT_IMS_SIP_SESSION_CANCEL,           /* 4 byte payload */
  EVENT_IMS_SIP_SESSION_FAILURE,          /* 4 byte payload */
  EVENT_IMS_SIP_RESPONSE_RECV,            /* 4 byte payload */
  EVENT_IMS_SIP_REQUEST_RECV,             /* 4 byte payload */
  EVENT_IMS_SIP_RESPONSE_SEND,            /* 4 byte payload */
  EVENT_IMS_SIP_REQUEST_SEND,             /* 4 byte payload */

  EVENT_WLAN_TKIP_COUNTER_MEAS,           /* 2 byte payload */

  EVENT_GPS_BLANKING_OFF,                 /* No payload */
  EVENT_GPS_BLANKING_ON,                  /* No payload */

  EVENT_MMGSDI_EVENT,                     /* 16 byte payload */

  EVENT_WLAN_CP_SYS_MGR_STATE_TRANS,      /* 3 byte payload */

  EVENT_GPS_OPTIMISTIC_PUNC_START,        /* 4 byte payload */
  EVENT_GPS_OPTIMISTIC_PUNC_END,          /* 4 byte payload */

  EVENT_QVP_SEND_RTP_PACKET,              /* 7 byte payload */
  EVENT_QVP_RECV_RTP_PACKET,              /* 7 byte payload */

  EVENT_HDR_IDLE_REACQ_FAIL_DDARF,        /* 3 byte payload */

  EVENT_BCAST_SEC_STKM_PARSE_STATUS,      /* 2 byte payload */
  EVENT_BCAST_SEC_STKM_RECEIVED,          /* No payload */
  EVENT_BCAST_SEC_SDP_PARSE_STATUS,       /* 2 byte payload */

  EVENT_CGPS_ME_DPO_STATUS,               /* 1 byte payload */
  EVENT_GPS_SV_SEARCH_STATE,              /* 6 byte payload */
  EVENT_GPS_TM_ON_DEMAND_MODE_CHANGE,     /* 3 byte payload */
  EVENT_GPS_TM_ON_DEMAND_BEGIN,           /* 6 byte payload */
  EVENT_GPS_TM_ON_DEMAND_DONE,            /* 1 byte payload */

  EVENT_RMAC_CARRIER_STATE_CHANGED,       /* 6 byte payload */

  EVENT_GPS_SBAS_DEMOD_REPORT,            /* 9 byte payload */
  EVENT_GPS_EXTERN_COARSE_POS_INJ_START,  /* No payload */
  EVENT_GPS_EXTERN_COARSE_POS_INJ_END,    /* 1 byte payload */
  EVENT_GPS_EPH_REREQUEST_TIME,           /* 2 byte payload */

  EVENT_WLAN_QOS_PSTREAM,                 /* 3 byte payload */
  EVENT_WLAN_CP_VCC,                      /* 9 byte payload */

  EVENT_CGPS_DIAG_FIRST_SUCCESSFUL_FIX,   /* No payload */

  EVENT_EUL_RECONFIG_OR_ASU_OR_TTI_RECFG, /* 12 byte payload */

  EVENT_DS707_PKT_LN_UPDATE,              /* 3 byte payload */
  EVENT_DS707_PKT_IDM_CHANGE,             /* 2 byte payload */

  EVENT_RLP_QN_ADD,                       /* 3 byte payload */
  EVENT_RLP_QN_DROP,                      /* 3 byte payload */
  EVENT_RLP_MULTILINK_NAK,                /* 9 byte payload */
  EVENT_RLP_REV_LINK_NAK,                 /* 9 byte payload */
  EVENT_GSTK_EVENT,                       /* 16 byte payload */

  EVENT_GAN_REGISTRATION_REQUEST,         /* 1 byte payload */
  EVENT_GAN_REGISTER_ACCEPT,              /* 1 byte payload */
  EVENT_CALL_RINGING_ALERT,               /* 1 byte payload */
  EVENT_GAN_PAGING_RECEIVED,              /* 1 byte payload */
  EVENT_GAN_CALL_DISCONNECT,              /* 1 byte payload */
  EVENT_GAN_CALL_RELEASE_COMPLETE,        /* 1 byte payload */
  EVENT_GAN_HANDIN_COMMAND,               /* 1 byte payload */
  EVENT_GAN_HANDIN_COMPLETE,              /* 1 byte payload */
  EVENT_GAN_HANDOUT_COMMAND,              /* 1 byte payload */
  EVENT_GAN_HANDOUT_COMPLETE,             /* 1 byte payload */
  EVENT_GAN_SMS_START,                    /* 1 byte payload */
  EVENT_GAN_SMS_ACK,                      /* 1 byte payload */
  EVENT_GAN_QDJ_ENQUEUE,                  /* No payload */
  EVENT_GAN_QDJ_DEQUEUE,                  /* No payload */
  EVENT_GAN_ACTIVATE_DATA_CHANNEL,        /* 1 byte payload */
  EVENT_GAN_DATA_CHANNEL_CONNECTED,       /* 1 byte payload */
  EVENT_GAN_RLP_SUSPEND,                  /* 1 byte payload */
  EVENT_GAN_RLP_RESUME,                   /* 1 byte payload */
  EVENT_GAN_WAKEUP_REQ,                   /* No payload */
  EVENT_GAN_WAKEUP_CNF,                   /* No payload */
  EVENT_GAN_HIBERNATION_REQ,              /* No payload */
  EVENT_GAN_HIBERNATION_CNF,              /* No payload */

  EVENT_WCDMA_UL_AMR_RATE,                /* 11 byte payload */
  EVENT_EUL_TTI_RECONFIG,                 /* 1 byte payload */
  EVENT_WCDMA_CONN_REL_CAUSE,             /* 1 byte payload */
  EVENT_WCDMA_CONN_REQ_CAUSE,             /* 1 byte payload */

  EVENT_LTE_TIMING_ADVANCE,               /* 3 byte payload */
  EVENT_LTE_UL_OUT_OF_SYNC,               /* No payload */
  EVENT_LTE_SPS_DEACTIVATED,              /* 1 byte payload */
  EVENT_LTE_RACH_ACCESS_START,            /* 2 byte payload */
  EVENT_LTE_RACH_RAID_MATCH,              /* 1 byte payload */
  EVENT_LTE_RACH_ACCESS_RESULT,           /* 1 byte payload */

  EVENT_DTV_L1_POWERUP,                   /* 2 byte payload */
  EVENT_DTV_L1_POWERDOWN,                 /* 2 byte payload */
  EVENT_DTV_L1_SOFT_RESET,                /* 2 byte payload */
  EVENT_DTV_L1_STATE_CHANGE,              /* 8 byte payload */
  EVENT_DTV_L1_ACQ_TUNE_STATUS,           /* 9 byte payload */
  EVENT_DTV_L1_ACQ_DONE_STATUS,           /* 9 byte payload */
  EVENT_DTV_L1_ACQ_FAIL,                  /* 3 byte payload */
  EVENT_DTV_L1_TRAFFIC_STARTED,           /* 2 byte payload */
  EVENT_DTV_L1_BAD_FRAME_RECEIVED,        /* 6 byte payload */
  EVENT_DTV_L1_TMCC_FAILURE,              /* 4 byte payload */
  EVENT_DTV_L1_RECOVERY_STATUS,           /* 5 byte payload */
  EVENT_DTV_L1_INTERRUPT_LOG_RECEIVED,    /* 2 byte payload */
  EVENT_DTV_L1_L3_API_COMMAND,            /* 4 byte payload */
  EVENT_DTV_L1_MODEM_FAILURE,             /* 6 byte payload */

  EVENT_GSM_CALL_DROP,                    /* 2 byte payload */
  EVENT_GSM_ACCESS_FAILURE,               /* 3 byte payload */

  EVENT_DTV_ISDB_ACTIVATE,                /* 9 byte payload */
  EVENT_DTV_ISDB_DEACTIVATE,              /* 9 byte payload */
  EVENT_DTV_ISDB_TUNE,                    /* 13 byte payload */
  EVENT_DTV_ISDB_UNTUNE,                  /* 9 byte payload */
  EVENT_DTV_ISDB_SELECT_SERVICE,          /* 11 byte payload */
  EVENT_DTV_ISDB_SERVICE_AVAILABLE,       /* 6 byte payload */
  EVENT_DTV_ISDB_TRAFFIC_LOST,            /* 4 byte payload */
  EVENT_DTV_ISDB_TABLE_UPDATE,            /* 7 byte payload */
  EVENT_DTV_ISDB_TRACKS_SELECTED,         /* 13 byte payload */
  EVENT_DTV_ISDB_PES_BUFFER_OVERFLOW,     /* 5 byte payload */
  EVENT_DTV_ISDB_PES_BUFFER_UNDERFLOW,    /* 5 byte payload */
  EVENT_DTV_ISDB_ACQUIRE_DATA_COMPONENT,  /* 10 byte payload */
  EVENT_DTV_ISDB_STOP_COMPONENT_ACQUISITION, /* 10 byte payload */
  EVENT_DTV_ISDB_DII_CHANGED,             /* 5 byte payload */
  EVENT_DTV_ISDB_DATA_EVENT_MESSAGE,      /* 5 byte payload */
  EVENT_DTV_ISDB_MODULE_CONSTRUCTION,     /* 13 byte payload */
  EVENT_DTV_ISDB_PARSING_ERROR,           /* 13 byte payload */

  EVENT_HDR_SLP_SLPQH_TIMER_STARTED,      /* No payload */
  EVENT_HDR_SLP_SLPQH_TIMER_STOPPED,      /* 1 byte payload */
  EVENT_HDR_SLP_SLPQH_NUM_PENDING_MSGS,   /* 1 byte payload */
  EVENT_HDR_OVHD_FIND_CACHED_MSG,         /* 1 byte payload */

  EVENT_WCDMA_RRC_TIMER_EXPIRED,          /* 1 byte payload */
  EVENT_WCDMA_UOOS_TIMER_USED,            /* 4 byte payload */
  EVENT_WCDMA_UOOS_TIMER_START,           /* 1 byte payload */
  EVENT_WCDMA_UOOS_TIMER_STOP,            /* 1 byte payload */
  EVENT_WCDMA_UOOS_TIME_REMAINING,        /* 6 byte payload */
  EVENT_WCDMA_RRCCSP_SCAN_START,          /* 1 byte payload */
  EVENT_WCDMA_ACQUISITON_SUCCESS,         /* 5 byte payload */
  EVENT_WCDMA_CELL_SELECTION_FAIL,        /* 1 byte payload */
  EVENT_WCDMA_BPLMN_START,                /* 1 byte payload */
  EVENT_WCDMA_BPLMN_END,                  /* 1 byte payload */
  EVENT_WCDMA_BPLMN_SCAN_START,           /* 1 byte payload */
  EVENT_WCDMA_BPLMN_SCAN_END,             /* 1 byte payload */

  EVENT_MSG_HIGH,                         /* 32 byte payload */
  EVENT_MSG_MED,                          /* 32 byte payload */
  EVENT_MSG_LOW,                          /* 32 byte payload */
  EVENT_MSG_ERROR,                        /* 32 byte payload */
  EVENT_MSG_FATAL,                        /* 32 byte payload */

  EVENT_GAN_START_TU3910,                 /* No payload */
  EVENT_GAN_STOP_TU3910,                  /* No payload */ 
  EVENT_GAN_EXPIRY_TU3910,                /* No payload */
  EVENT_GAN_START_TU3920,                 /* No payload */
  EVENT_GAN_STOP_TU3920,                  /* No payload */
  EVENT_GAN_EXPIRY_TU3920,                /* No payload */
  EVENT_GAN_START_TU3906,                 /* No payload */ 
  EVENT_GAN_STOP_TU3906,                  /* No payload */ 
  EVENT_GAN_EXPIRY_TU3906,                /* No payload */
  EVENT_GAN_URR_REGISTER_UPDATE,          /* 1 byte payload */

  EVENT_IPSEC_IKE_SA_INIT_SENT,            /* 8 bytes payload */
  EVENT_IPSEC_IKE_SA_INIT_RECV,            /* 16 bytes payload */
  EVENT_IPSEC_IKE_SA_ESTABLISHED,          /* 16 bytes payload */
  EVENT_IPSEC_IKE_AUTH_SENT,               /* 16 bytes payload */
  EVENT_IPSEC_IKE_AUTH_RECV,               /* 16 bytes payload */
  EVENT_IPSEC_IKE_EAP_START,               /* No payload */
  EVENT_IPSEC_IKE_EAP_FINISH,              /* 1 byte payload */
  EVENT_IPSEC_CHILD_SA_ESTABLISHED,        /* 8 bytes payload */
  EVENT_IPSEC_IKE_INFO_MSG_SENT,           /* 16 bytes payload */ 
  EVENT_IPSEC_IKE_INFO_MSG_RECV,           /* 16 bytes payload */
  EVENT_IPSEC_CREATE_CHILD_SA_SENT,        /* 16 bytes payload */
  EVENT_IPSEC_CREATE_CHILD_SA_RECV,        /* 16 bytes payload */
  EVENT_IPSEC_IKE_SA_DELETE_START,         /* 16 bytes payload */
  EVENT_IPSEC_IKE_SA_DELETE_DONE,          /* 16 bytes payload */
  EVENT_IPSEC_CHILD_SA_DELETE_START,       /* 8 bytes payload */
  EVENT_IPSEC_CHILD_SA_DELETE_DONE,        /* 8 bytes payload */
  EVENT_IPSEC_IKE_SA_REKEY_START,          /* 16 bytes payload */
  EVENT_IPSEC_IKE_SA_REKEY_DONE,           /* 16 bytes payload */
  EVENT_IPSEC_CHILD_SA_REKEY_START,        /* 8 bytes payload */
  EVENT_IPSEC_CHILD_SA_REKEY_DONE,         /* 8 bytes payload */
  EVENT_IPSEC_IKE_MESG_RETRANSMIT,         /* 4 bytes payload */
  EVENT_IPSEC_IKE_NAT_DETECTED,            /* No payload */
  EVENT_IPSEC_IKE_NAT_KEEPALIVE_SENT,      /* 16 bytes payload */
  EVENT_IPSEC_IKE_DPD_SENT,                /* 16 bytes payload */
  EVENT_IPSEC_IKE_ERR_NOTIFY_SENT,         /* 4 bytes payload */
  EVENT_IPSEC_IKE_ERR_NOTIFY_RECV,         /* 4 bytes payload */

  EVENT_GAN_ROVEIN_CNF,                    /* No payload */
  EVENT_GAN_ROVEOUT_CNF,                   /* No payload */
  EVENT_GAN_RRC_ROVEIN_CNF,                /* No payload */
  EVENT_GAN_RRC_ROVEIN_REJ,                /* 1 byte payload */
  EVENT_GAN_RRC_ROVEOUT_CNF,               /* No payload */
  EVENT_GAN_RRC_ROVEOUT_REJ,               /* 1 byte payload */

  EVENT_GPSXTRA_T_SESS_BEGIN,              /* 1 byte payload */
  EVENT_GPSXTRA_T_SESS_DATA,               /* 8 byte payload */
  EVENT_GPSXTRA_T_SESS_DONE,               /* 1 byte payload */
  EVENT_GPSXTRA_T_SESS_END,                /* 4 byte payload */

  EVENT_DS_GO_NULL_TIMER,                  /* 5 byte payload */

  EVENT_LTE_RRC_TIMER_STATUS,              /* 6 byte payload */
  EVENT_LTE_RRC_STATE_CHANGE,              /* 1 byte payload */
  EVENT_LTE_RRC_OUT_OF_SERVICE,            /* 2 byte payload */
  EVENT_LTE_RRC_RADIO_LINK_FAILURE,        /* 2 byte payload */
  EVENT_LTE_RRC_DL_MSG,                    /* 2 byte payload */ 
  EVENT_LTE_RRC_UL_MSG,                    /* 2 byte payload */
  EVENT_LTE_RRC_NEW_CELL_IND,              /* 5 byte payload */
  EVENT_LTE_RRC_CELL_RESEL_FAILURE,        /* 5 byte payload */
  EVENT_LTE_RRC_HO_FAILURE,                /* 5 byte payload */
  EVENT_LTE_RRC_PAGING_DRX_CYCLE,          /* 2 byte payload */
  EVENT_LTE_RRC_IRAT_HO_FROM_EUTRAN,       /* 1 byte payload */
  EVENT_LTE_RRC_IRAT_HO_FROM_EUTRAN_FAILURE, /* 1 byte payload */
  EVENT_LTE_RRC_IRAT_RESEL_FROM_EUTRAN,            /* 1 byte payload */
  EVENT_LTE_RRC_IRAT_RESEL_FROM_EUTRAN_FAILURE, /* 1 byte payload */
  EVENT_LTE_RRC_SIB_READ_FAILURE,          /* 6 byte payload */

  EVENT_GAN_ROVEIN_REQ,                    /* No payload */
  EVENT_GAN_ROVEOUT_REQ,                   /* No payload */

  EVENT_MBP_RF_ANALOG_JD_MODE_CHANGE,      /* 7 byte payload */
  EVENT_MBP_RF_ANALOG_JD_INT,              /* 4 byte payload */

  EVENT_CGPS_QWIP_SYSD_TRANSITION,         /* 4 byte payload */
  
  EVENT_HPLMN_TIMER_EXPIRED,               /* No Payload */

  EVENT_GSDI_GET_FEATURE_INDICATOR_DATA,   /* 6 byte payload */

  EVENT_LTE_CM_INCOMING_MSG,               /* 1 byte payload */
  EVENT_LTE_CM_OUTGOING_MSG,               /* 1 byte payload */
  EVENT_LTE_EMM_INCOMING_MSG,              /* 1 byte payload */
  EVENT_LTE_EMM_OUTGOING_MSG,              /* 1 byte payload */
  EVENT_LTE_EMM_TIMER_START,               /* 1 byte payload */
  EVENT_LTE_EMM_TIMER_EXPIRY,              /* 1 byte payload */

  EVENT_LTE_REG_INCOMING_MSG,              /* 1 byte payload */
  EVENT_LTE_REG_OUTGOING_MSG,              /* 1 byte payload */
  EVENT_LTE_ESM_INCOMING_MSG,              /* 1 byte payload */
  EVENT_LTE_ESM_OUTGOING_MSG,              /* 1 byte payload */
  EVENT_LTE_ESM_TIMER_START,               /* 1 byte payload */
  EVENT_LTE_ESM_TIMER_EXPIRY,              /* 1 byte payload */

  EVENT_SNS_CONTEXT_OPEN,                  /* 4 byte payload */
  EVENT_SNS_CONTEXT_CLOSE,                 /* 4 byte payload */
  EVENT_SNS_COND_SET,                      /* 8 byte payload */
  EVENT_SNS_COND_CANCEL,                   /* 8 byte payload */
  EVENT_SNS_COND_MET,                      /* 8 byte payload */
  EVENT_SNS_DATA_START,                    /* 12 byte payload */
  EVENT_SNS_DATA_STOP,                     /* 8 byte payload */

  EVENT_WCDMA_RLC_CONFIG,                  /* 4 byte payload */

  EVENT_HSPA_PLUS_CFG,                     /* 6 byte payload */

  EVENT_SNS_DRIVER_STATE_CHANGE,           /* 9 byte payload */

  EVENT_WCDMA_TIMER_DISCARD_EXPIRY,        /* 3 byte payload */  

  EVENT_NAS_CB_PAGE_RECEIVED,              /* 5 byte payload */  

  EVENT_WCDMA_RLC_RESET,                   /* 1 byte payload */  

  EVENT_HDR_MRLP_EHRPD_PERSONALITY_IS_ACTIVE, /* 1 byte payload */  
  
  EVENT_WLAN_SECURITY,                     /* 13 byte payload */
  EVENT_WLAN_STATUS,                       /* 15 byte payload */
  EVENT_WLAN_HANDOFF,                      /* 15 byte payload */
  EVENT_WLAN_VCC,                          /* 8 byte payload */
  EVENT_WLAN_QOS,                          /* 2 byte payload */
  EVENT_WLAN_PE,                           /* 16 byte payload */
  EVENT_WLAN_ADD_BLOCK_ACK_SUCCESS,        /* 11 byte payload */
  EVENT_WLAN_ADD_BLOCK_ACK_FAILED,         /* 9 byte payload */
  EVENT_WLAN_DELETE_BLOCK_ACK_SUCCESS,     /* 8 byte payload */
  EVENT_WLAN_DELETE_BLOCK_ACK_FAILED,      /* 8 byte payload */
  EVENT_WLAN_BSS_PROTECTION,               /* 2 byte payload */
  EVENT_WLAN_BRINGUP_STATUS,               /* 12 byte payload */
  EVENT_WLAN_POWERSAVE_GENERIC,            /* 16 byte payload */
  EVENT_WLAN_POWERSAVE_WOW,                /* 11 byte payload */
  EVENT_WLAN_WCM,                          /* 17 byte payload */
  EVENT_WLAN_WPS_SCAN_START,               /* 16 byte payload */
  EVENT_WLAN_WPS_SCAN_COMPLETE,            /* 2 byte payload */
  EVENT_WLAN_WPS_CONNECT_REQUEST,          /* 9 byte payload */
  EVENT_WLAN_WPS_CONNECT_RESPONSE,         /* 6 byte payload */
  EVENT_WLAN_WPS_PBC_SESSION_OVERLAP,      /* 16 byte payload */
  EVENT_WLAN_WPS_PBC_WALK_TIMER_START,     /* No payload */
  EVENT_WLAN_WPS_PBC_WALK_TIMER_STOP,      /* No payload */
  EVENT_WLAN_WPS_PBC_AP_DETECTED,          /* 14 byte payload */
  EVENT_WLAN_WPS_REGISTRATION_START,       /* 1 byte payload */
  EVENT_WLAN_WPS_WSC_MESSAGE,              /* 1 byte payload */
  EVENT_WLAN_WPS_DISCOVERY,                /* 7 byte payload */
  EVENT_WLAN_WPS_REGISTRATION_COMPLETE,    /* 1 byte payload */
  EVENT_WLAN_WPS_DISCONNECT,               /* No payload */
  EVENT_WLAN_BTC,                          /* 15 byte payload */

  EVENT_IPV6_SM_EVENT,                     /* 4 byte payload */
  EVENT_IPV6_SM_TRANSITION,                /* 5 byte payload */
  EVENT_IPV6_PREFIX_UPDATE,                /* 13 byte payload */

  EVENT_LTE_ML1_STATE_CHANGE,              /* 2 byte payload */

  EVENT_AUTH_PROTO,                        /* 2 byte payload */
  EVENT_VSNCP,                             /* 2 byte payload */
  EVENT_IID,                               /* 2 byte payload */

  EVENT_IMS_VIDEOSHARE_REGISTRATION_SUCCESS,   /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_INVITE_SENT,            /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_INCOMING_INVITE,        /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ACCEPT_REJECT_INVITE,   /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ACCEPTING_SESSION,      /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_SESSION_ESTABLISHED,    /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_END_SESSION,            /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_PREVIEW_VIDEO_FRAME,    /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_DECODED_VIDEO_FRAME,    /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_RECEIVING,              /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_START_APPLICATION,      /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_END_APPLICATION,        /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_CAPABILITY_SUCCESS,     /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_MEDIA_RECORDING_RESOURCE_ACQUIRED,   /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_MEDIA_RECORDING_RESOURCE_RELEASED,   /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_SENDING,                             /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_INCOMING_OPTION_RECEIVED,            /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_INCOMING_OPTION_RESPONDED,           /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ERR_CALL_FAILED,                     /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ERR_REGISTRATION_FAILED,             /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ERR_RECORDER_ERROR,                  /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ERR_PLAYER_ERROR,                    /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ERR_MEDIA_SESSION_FAILURE,           /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ERR_CAPABILITY_FAILURE,              /* 4 byte payload */
  EVENT_IMS_VIDEOSHARE_ERR_MEDIA_RECORDING_FAILED,          /* 4 byte payload */

  EVENT_WLAN_PE_FRAME,                     /* 16 byte payload */

  EVENT_SNS_VCPS_HEADING_COMPUTED,         /* 12 byte payload */ 
  EVENT_SNS_VCPS_TRACKED_CAL_SET_SAVED,    /* 22 byte payload */ 

  EVENT_GNSS_PRESC_DWELL_COMPLETE,         /* 2 byte payload */ 

  EVENT_LTE_MAC_RESET,                     /* 1 byte payload */ 
  EVENT_LTE_BSR_SR_REQUEST,                /* 1 byte payload */ 
  EVENT_LTE_MAC_TIMER,                     /* 2 byte payload */ 

  EVENT_CM_DS_OPERATIONAL_MODE,            /* 2 byte payload */
  EVENT_CM_DS_MODE_PREF,                   /* 5 byte payload */
  EVENT_CM_DS_GW_ACQ_ORDER_PREF,           /* 5 byte payload */
  EVENT_CM_DS_SRV_DOMAIN_PREF,             /* 5 byte payload */
  EVENT_CM_DS_BAND_PREF,                   /* 5 byte payload */
  EVENT_CM_DS_ROAM_PREF,                   /* 5 byte payload */
  EVENT_CM_DS_HYBRID_PREF,                 /* 5 byte payload */
  EVENT_CM_DS_NETWORK_SEL_MODE_PREF,       /* 5 byte payload */
  EVENT_CM_DS_CALL_EVENT_ORIG,             /* 4 byte payload */
  EVENT_CM_DS_CALL_EVENT_CONNECT,          /* 4 byte payload */
  EVENT_CM_DS_CALL_EVENT_END,              /* 3 byte payload */
  EVENT_CM_DS_ENTER_EMERGENCY_CB,          /* 1 byte payload */
  EVENT_CM_DS_EXIT_EMERGENCY_CB,           /* 1 byte payload */
  EVENT_CM_DS_CALL_STATE,                  /* 2 byte payload */
  EVENT_CM_DS_DS_INTERRAT_STATE,           /* 3 byte payload */
  EVENT_CM_DS_CELL_SRV_IND,                /* 6 byte payload */
  EVENT_CM_DS_COUNTRY_SELECTED,            /* 3 byte payload */
  EVENT_CM_DS_DATA_AVAILABLE,              /* 2 byte payload */
  EVENT_CM_DS_SELECT_COUNTRY,              /* 8 byte payload */
  EVENT_CM_DS_CALL_EVENT_ORIG_THR,         /* 4 byte payload */
  EVENT_CM_DS_PLMN_FOUND,                  /* 13 byte payload */
  EVENT_CM_DS_SERVICE_CONFIRMED,           /* 13 byte payload */
  EVENT_CM_DS_GET_PASSWORD_IND,            /* 3 byte payload */
  EVENT_CM_DS_PASSWORD_AUTHENTICATION_STATUS, /* 3 byte payload */
  EVENT_CM_DS_USS_RESPONSE_NOTIFY_IND,     /* 4 byte payload */
  EVENT_CM_DS_LCS_MOLR_CONF,               /* 2 byte payload */

  EVENT_DS_NAS_MESSAGE_SENT,               /* 5 byte payload */ 
  EVENT_DS_NAS_MESSAGE_RECEIVED,           /* 5 byte payload */  
  EVENT_DS_MM_STATE,                       /* 3 byte payload */ 
  EVENT_DS_GMM_STATE,                      /* 3 byte payload */ 
  EVENT_DS_PLMN_INFORMATION,               /* 10 byte payload */ 

  EVENT_DIAG_STRESS_TEST_COMPLETED,        /* 4 byte payload */ 

  EVENT_GNSS_CC_STATUS,                    /* 2 byte payload */ 

  EVENT_SNS_USER_STATE_CHANGE,             /* 6 byte payload */ 

  EVENT_DS_HPLMN_TIMER_EXPIRED,            /* 1 byte payload */  
  EVENT_DS_RAT_CHANGE,                     /* 2 byte payload */ 

  EVENT_DTV_CMMB_API_CALL_ACTIVATE,               /*9 byte payload*/ /*ID=1757*/ 
  EVENT_DTV_CMMB_API_CALL_DEACTIVATE,           /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_TUNE,                     /*13 byte payload*/
  EVENT_DTV_CMMB_API_CALL_SELECT_SERVICE,     /*11 byte payload*/
  EVENT_DTV_CMMB_API_CALL_DESELECT_SERVICE,  /*11 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_SIGNAL_PARAMETERS,      /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_NIT,                /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_CMCT,             /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_SMCT,             /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_CSCT,             /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_SSCT,             /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_EADT,             /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_REQUEST_CA_CARD_NUMBER,   /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_REQUEST_CAS_ID,                 /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_REGISTER_FOR_CONTROL_NOTIFICATIONS,         /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_DEREGISTER_FROM_CONTROL_NOTIFICATIONS,   /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_ACTIVATE,               /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_DEACTIVATE,           /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_TUNE,      /*13 byte payload*/ 
  EVENT_DTV_CMMB_API_NOTIFICATION_SELECT_SERVICE,     /*11 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_DESELECT_SERVICE,  /*11 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_TABLE_UPDATE,        /*6 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_SIGNAL_PARAMETERS,/*14 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_AUTHORIZATION_FAILURE,                 /*11 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_REGISTER_FOR_CONTROL_NOTIFICATIONS_COMPLETE,      /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_DEREGISTER_FROM_CONTROL_NOTIFICATIONS_COMPLETE, /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_CA_CARD_NUMBER,   /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_CAS_ID,                 /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_EMERGENCY_BROADCASTING_TRIGGER, /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_EMERGENCY_BROADCASTING_MESSAGE,/*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_REGISTER_FOR_ESG_NOTIFICATIONS,                /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_DEREGISTER_FROM_ESG_NOTIFICATIONS,          /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_GET_BASIC_DESCRIPTION_INFORMATION,          /*9 byte payload*/
  EVENT_DTV_CMMB_API_CALL_SET_OUTPUT_PATH,/*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_ESG_DATA_INFORMATION,                  /*6 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_ESG_DATA_INFORMATION_DOWNLOAD_COMPLETE,         /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_ESG_PROGRAM_INDICATION_INFORMATION,                   /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_REGISTER_FOR_ESG_NOTIFICATIONS_COMPLETE,           /*9 byte payload*/
  EVENT_DTV_CMMB_API_NOTIFICATION_DEREGISTER_FROM_ESG_NOTIFICATIONS_COMPLETE,     /*9 byte payload*/
  EVENT_DTV_CMMB_CAS_INITIALIZED,                 /*9 byte payload*/
  EVENT_DTV_CMMB_CAS_EMM_RECEIVED_AND_PROCESSED,    /*9 byte payload*/
  EVENT_DTV_CMMB_CAS_ECM_RECEIVED_AND_PROCESSED,    /*11 byte payload*/ /*ID=1798*/
  
  EVENT_ECALL_START,                                      /*3 byte payload*/ 
  EVENT_ECALL_STOP,                                       /*1 byte payload*/ 
  EVENT_ECALL_SESSION_START,                        /*1 byte payload*/
  EVENT_ECALL_SESSION_FAILURE,                      /*1 byte payload*/
  EVENT_ECALL_SESSION_COMPLETE,                   /*3 byte payload*/
  EVENT_ECALL_SESSION_RESET,                        /*1 byte payload*/
  EVENT_ECALL_PSAP_MSD_DECODE_SUCCESS,     /*2 byte payload*/
  EVENT_ECALL_PSAP_LOST_SYNC,                     /*1 byte payload*/ /*ID = 1806*/

  EVENT_LTE_RRC_IRAT_REDIR_FROM_EUTRAN_START, /*1 byte payload */
  EVENT_LTE_RRC_IRAT_REDIR_FROM_EUTRAN_END,    /* 2 byte payload */ /*ID = 1808*/

  EVENT_GPRS_DS_CELL_CHANGE_ORDER,            /* 2 byte payload */ /*1809*/
  EVENT_GSM_DS_CELL_SELECTION_END,            /* 2 byte payload */ /*1810*/
  EVENT_GSM_DS_L1_STATE,                      /* 2 byte payload */ /*1811*/
  EVENT_GSM_DS_PLMN_LIST_START,               /* 2 byte payload */ /*1812*/
  EVENT_GSM_DS_PLMN_LIST_END,                 /* 1 byte payload */ /*1813*/
  EVENT_GSM_DS_POWER_SCAN_STATUS,             /* 2 byte payload */ /*1814*/
  EVENT_GSM_DS_RESELECT_START,                /* 2 byte payload */ /*1815*/
  EVENT_GSM_DS_RR_IN_SERVICE,                 /* 1 byte payload */ /*1816*/
  EVENT_GSM_DS_RR_OUT_OF_SERVICE,             /* 1 byte payload */ /*1817*/
  EVENT_GSM_DS_TIMER_EXPIRED,                 /* 3 byte payload */ /*1818*/
  EVENT_GSM_DS_TO_WCDMA_RESELECT_END,         /* 6 byte payload */ /*1819*/

  EVENT_CM_DS_SYSTEM_MODE,                    /*2 byte payload*/ /*1820*/
  EVENT_SD_DS_EVENT_ACTION,                   /*9 byte payload*/ /*1821*/
  EVENT_SMGMM_DS_REQUEST_SENT,                /*3 byte payload*/ /*1822*/

  EVENT_IFACE,                                /*4 byte payload*/ /*1823*/
  EVENTS_DS_GSM_L1_ALIGN_VFR = 0x720,
  EVENTS_DS_GSM_L1_STATE = 0x721,
  EVENTS_DS_GSM_RATSCCH_IN_DTX = 0x722,
  EVENTS_DS_GSM_FACCH_IN_DTX = 0x723,
  EVENTS_DS_GSM_FACCH_AND_RATSCCH_COLLISION = 0x724,
  EVENTS_DS_GSM_FACCH_AND_SID_UPDATE_COLLISION = 0x725,
  EVENTS_DS_GSM_RATSCCH_AND_SID_UPDATE_COLLISION = 0x726,
  EVENTS_DS_GSM_AMR_STATE_CHANGE = 0x727,
  EVENTS_DS_GSM_RATSCCH_CMI_PHASE_CHANGE = 0x728,
  EVENTS_DS_GSM_RATSCCH_REQ_ACT_TIMER_EXPIRY = 0x729,
  EVENTS_DS_GSM_RATSCCH_ACK_ACT_TIMER_EXPIRY = 0x72a,
  EVENTS_DS_GSM_AMR_RATSCCH_REQ = 0x72b,
  EVENTS_DS_GSM_AMR_RATSCCH_RSP = 0x72c,
  EVENTS_DS_GSM_AMR_CMC_TURNAROUND_TIME = 0x72d,
  EVENTS_DS_GPRS_SMGMM_MSG_RECEIVED = 0x72e,
  EVENTS_DS_GPRS_SMGMM_MSG_SENT = 0x72f,
  EVENTS_DS_GPRS_LLC_READY_TIMER_START = 0x730,
  EVENTS_DS_GPRS_LLC_READY_TIMER_END = 0x731,
  EVENTS_DS_PACKET_TIMESLOT_RECONFIGURE = 0x732,
  EVENTS_DS_GPRS_MAC_MSG_RECEIVED = 0x733,
  EVENTS_DS_GPRS_MAC_MSG_SENT = 0x734,
  EVENTS_DS_GPRS_MAC_CAMPED_ON_CELL = 0x735,
  EVENTS_DS_GPRS_CELL_CHANGE_FAILURE = 0x736,
  EVENTS_DS_GPRS_PACKET_CHANNEL_REQUEST = 0x737,
  EVENTS_DS_GPRS_PACKET_UPLINK_ASSIGNMENT = 0x738,
  EVENTS_DS_GPRS_PACKET_DOWNLINK_ASSIGNMENT = 0x739,
  EVENTS_DS_GPRS_TBF_RELEASE = 0x73a,
  EVENTS_DS_GPRS_TIMER_EXPIRY = 0x73b,
  EVENTS_DS_GPRS_PACKET_RESOURCE_REQUEST = 0x73c,
  EVENTS_DS_RANDOM_ACCESS_REQUEST = 0x73d,
  EVENTS_DS_GSM_HANDOVER_START = 0x73e,
  EVENTS_DS_GSM_HANDOVER_END = 0x73f,
  EVENTS_DS_GSM_RESELECT_START = 0x740,
  EVENTS_DS_GSM_RESELECT_END = 0x741,
  EVENTS_DS_GSM_TO_WCDMA_RESELECT_END = 0x742,
  EVENTS_DS_GSM_MESSAGE_RECEIVED = 0x743,
  EVENTS_DS_GSM_RR_IN_SERVICE = 0x744,
  EVENTS_DS_GSM_RR_OUT_OF_SERVICE = 0x745,
  EVENTS_DS_GSM_PAGE_RECEIVED = 0x746,
  EVENTS_DS_GSM_CAMP_ATTEMPT_START = 0x747,
  EVENTS_DS_GSM_CAMP_ATTEMPT_END = 0x748,
  EVENTS_DS_GSM_CALL_DROP = 0x749,
  EVENTS_DS_GSM_ACCESS_FAILURE = 0x74a,
  EVENTS_DS_GSM_CELL_SELECTION_START = 0x74b,
  EVENTS_DS_GSM_CELL_SELECTION_END = 0x74c,
  EVENTS_DS_GSM_POWER_SCAN_STATUS = 0x74d,
  EVENTS_DS_GSM_PLMN_LIST_START = 0x74e,
  EVENTS_DS_GSM_PLMN_LIST_END = 0x74f,
  EVENTS_DS_GSM_AMR_MULTIRATE_IE = 0x750,
  EVENTS_DS_GPRS_LINK_FAILURE = 0x751,
  EVENTS_DS_GPRS_PAGE_RECEIVED = 0x752,
  EVENTS_DS_GPRS_SURROUND_SEARCH_START = 0x753,
  EVENTS_DS_GPRS_SURROUND_SEARCH_END = 0x754,
  EVENTS_DS_GPRS_EARLY_CAMPING = 0x755,
  EVENTS_DS_GSM_LINK_FAILURE = 0x756,


  EVENT_MTP_FILE_DELETED = 0x757,
  EVENT_MTP_PLAYLIST_REMOVED_OBJECT = 0x758,
  EVENT_MTP_SYNC_STARTED = 0x759,
  EVENT_MTP_SYNC_FINISHED = 0x75a,
  EVENT_MTP_SAVE_ALBUMART_STARTED = 0x75b,
  EVENT_MTP_SAVE_ALBUMART_FINISHED = 0x75c,
  EVENT_MTP_FORMAT_STORE_STARTED = 0x75d,
  EVENT_MTP_FORMAT_STORE_DONE = 0x75e,
  EVENT_MTP_FORMAT_STORE_ERROR = 0x75f,
  EVENT_LTE_RRC_SECURITY_CONFIG = 0x760,
  EVENT_LTE_RRC_IRAT_RESEL_FROM_EUTRAN_START = 0x761,
  EVENT_LTE_RRC_IRAT_RESEL_FROM_EUTRAN_END = 0x762,
  EVENT_SNS_REST_DETECT_ACCEL_ACTIVE_TS = 0x763,
  EVENT_SNS_REST_DETECT_ACCEL_STOP_TS = 0x764,
  EVENT_CPC_CONFIG_ACTION = 0x765,
  EVENT_FDPCH_CONFIG_ACTION = 0x766,
  EVENT_SNS_DRV_MOTION_DETECT_SIG = 0x767,
  EVENT_SNS_DRV_OPMODE_CHANGE = 0x768,

  EVENT_NEXT_UNUSED_EVENT,
  EVENT_RSVD_START = 0x0800,
  EVENT_RSVD_END   = 0x083F,
  EVENT_LAST_ID    = 0x083F,

  EVENT_MAX_ID     = 0x0FFF
} event_id_enum_type;


#endif /* EVENT_DEFS_H */
