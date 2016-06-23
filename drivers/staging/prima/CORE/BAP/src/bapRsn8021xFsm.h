/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*
 * $File: //depot/software/projects/feature_branches/gen5_phase1/os/linux/classic/ap/apps/ssm/auth8021x/ani8021xFsm.h $
 * Contains the declarations for the Auth Agent's FSM's to work.
 *
 * Author:      Mayank D. Upadhyay
 * Date:        21-June-2002
 * History:-
 * Date         Modified by     Modification Information
 * ------------------------------------------------------
 *
 */
#ifndef __AAG_FSM_H_
#define __AAG_FSM_H_

#include "vos_types.h"
#include "vos_trace.h"
#include "vos_timer.h"
#include <bapRsnSsmReplayCtr.h>
#include <bapRsnAsfPacket.h>
#include <bapRsnSsmEapol.h>
#include "bapRsn8021xPrf.h"
//#include "bapInternal.h"
#include "csrApi.h"

typedef struct sBtampContext tBtampContext;
typedef struct tStaContext tStaContext;
typedef struct tSuppContext tSuppContext;

#define RSN_MAX_PACKET_SIZE 512
#define RSN_80211_KEY_LEN    16
#define RSN_IE_MAX_PACKET_SIZE 256
#define RSN_IE_HEADER_SIZE 0
#define ACCTG_SESSION_ID_SIZE 8
#define ANI_SSM_AES_KEY_WRAP_BLOCK_SIZE 8 // Bytes

#define  BAP_SET_RSN_KEY   1
#define  BAP_RESET_RSN_KEY  0 


#define AAG_ACL_LOOKUP_NEEDED(ctx) \
            ((ctx)->authType == eANI_SSM_AT_NONE || \
             (ctx)->authType == eANI_SSM_AT_RSN_PSK || \
             (ctx)->authType == eANI_SSM_AT_SHARED_KEY)

#define AAG_ACL_LOOKUP_PENDING(ctx) \
            ((ctx)->aclLookupFsm != NULL && (ctx)->radiusInfo.req != NULL)

#define AAG_STA_AWAITING_CLEANUP(ctx) \
            ((ctx)->ssid == NULL)

#define AAG_MARK_STA_AS_AWAITING_CLEANUP(ctx) \
            ((ctx)->ssid = NULL)

/************************
 * AuthRsnFsm structure:
 *************************/
typedef struct tagAuthRsnFsm 
{
    v_U8_t currentState;
    
    tBtampContext *ctx;
    tStaContext *staCtx; 

    // Variables used for EAPOL-Key messages
    v_U8_t aNonce[ANI_EAPOL_KEY_RSN_NONCE_SIZE];
    v_U8_t sNonce[ANI_EAPOL_KEY_RSN_NONCE_SIZE];
 
    // Flags set by external events
    v_U8_t authReq;
    v_U8_t eapolAvail;
    v_U8_t disconnect;
    v_U8_t integFailed;
    v_U8_t pmkAvailable;

    // Variables maintained internally
    v_U8_t numTries;
    tAniPacket *lastEapol;  //Tx
    v_BOOL_t fReceiving;
    v_U32_t cryptHandle;

    // Timers used..alternate them in different states
    vos_timer_t msg2Timer;
    vos_timer_t msg4Timer;
    v_BOOL_t msg2TimeOut;
    v_BOOL_t msg4TimeOut;
    v_U8_t advertizedRsnIe[256];
} tAuthRsnFsm;

/************************
 * SuppRsnFsm structure:
 *************************/

typedef struct tagSuppRsnFsm {

    v_U8_t currentState;
    
    tBtampContext *ctx;
    tSuppContext *suppCtx;

    // Variables used for EAPOL-Key messages
    tAniSsmReplayCtr *localReplayCtr;
    tAniSsmReplayCtr *peerReplayCtr;
    v_U8_t aNonce[ANI_EAPOL_KEY_RSN_NONCE_SIZE];
    v_U8_t sNonce[ANI_EAPOL_KEY_RSN_NONCE_SIZE];

    // Flags set by external events
    v_U8_t authReq;
    v_U8_t pmkAvail;
    v_U8_t eapolAvail;
    v_U8_t integFailed;
    v_U8_t updateKeys;

    // Variables maintained internally
    int numTries;
    tAniPacket *lastEapol;
    v_BOOL_t fReceiving;
    v_U32_t cryptHandle;
} tSuppRsnFsm;


typedef struct sAagZoneEntry tAagZoneEntry;
typedef struct sAagSsidEntry tAagSsidEntry;

typedef enum
{
    //Internal to RSN
    //This event is triggered by RSN’s timers
    RSN_FSM_TIMER_EXPIRED,  
    //BAP use this event to inform auth/supp to start processing
    //authentication. When BAP send this event to RSN, it is presumed 
    //that the PMK is available.
    RSN_FSM_AUTH_START,    
    //Internal to RSN
    //This event is triggered by the Rx routine when called by TL
    RSN_FSM_EAPOL_FRAME_AVAILABLE,  
    //BAP use this event to inform RSN that the connection is lost  
    RSN_FSM_DISCONNECT,    
    //Internal to RSN
    //This event hannpens when RSN detect key integraty check fails
    RSN_FSM_INTEG_FAILED,  

}tRsnFsmEvent;

/**
 * Stores information about an EAP message that was last received or
 * EAPOL messages that were last sent.
 *
 * 1. EAP messages last received are stripped out of their outer
 *    encapsulation which may be either EAPOL or RADIUS, and are
 *    preserved within this structure for the lifetime of one event:
 *        - EAPOL_MESSAGE_AVAILABLE  => process and send to RADIUS
 *        - RADIUS_MESSAGE_AVAILABLE => process and send to STA
 *    When the event is fully handled, the incoming packet is freed,
 *    therefore, the contents of this structure are no longer valid.
 *
 * 2. EAPOL messages last sent are stored in their entirety. They are
 *     created and delete a little differently on the AP and BP sides:
 *       - AP side: The EAPOL message contains the last EAP message
 *     that was sent to the STA. As soon as a new EAP message arrives
 *     from RADIUS, this EAPOL mesage is freed because a new one will
 *     be generated.
 *       - BP side: The EAPOL message contains the last EAP message
 *     generated by the local supplicant. As soon as a new EAPOL
 *     message is generated, this one is freed and the new one is
 *     stored.
 */
typedef struct tEapInfo 

{
    tAniPacket *message;
    v_U8_t id;
} tEapInfo;


typedef enum eAniSsmAuthState {
    eANI_SSM_AUTH_STATE_INIT = 0,
    eANI_SSM_AS_PW_KEY_CONF_AWAITED,
    eANI_SSM_AS_PW_KEY_SET,
} tAniSsmAuthState;


/**
 * The Station's context is stored in this structure. It contains
 * pointers to the FSM's used by the STA (which in turn point back to
 * the context). It also contains the transient event data like
 * EAP-Message and RADIUS state that is obtained from various network
 * packets.
 */
struct tStaContext {

    // STA identification information
    tAniMacAddr suppMac;
    v_BOOL_t bpIndicator;

    // Local association point
    tAniMacAddr authMac;
    v_U8_t ssidName[SIR_MAC_MAX_SSID_LENGTH + 1];    
    tAagSsidEntry *ssid;

    // The different FSM's that can be instantiated for the STA
    tAuthRsnFsm *authRsnFsm;

    // Keys derived for STA
    v_U8_t ptk[AAG_PRF_MAX_OUTPUT_SIZE];
    tAniPacket *pmk; // MS-MPPE-Recv-Key
    tAniPacket *serverKey; // MS-MPPE-Send-Key
    v_U8_t keyId;

    // STA context timers
    v_U32_t  sessionTimeout;
    vos_timer_t reAuthTimer;
    vos_timer_t sessionCleanupTimer;

    // Radius Authentication attributes
    v_U8_t *authClassAttr;

    // Misc. authentication related state
    eCsrAuthType authType;
    eCsrEncryptionType pwCipherType;
    tAniPacket *ieSta;
    tAniSsmAuthState authState;
    v_BOOL_t prmLookupInProgress;
    v_BOOL_t firstTimeAuth;
    v_BOOL_t secureLink; // 4-way h/s requries this to be 0 at startup or on MIC failures
    tAniSsmReplayCtr *localReplayCtr;
    tAniSsmReplayCtr *peerReplayCtr; // Goes hand in hand with flag below
    v_BOOL_t pastFirstPeerRequest; // For use with peer replay counter

    tAniPacket *cachedPmk; // MS-MPPE-Recv-Key
    tAniPacket *cachedServerKey; // MS-MPPE-Send-Key
    v_U8_t cachedKeyId;

};


struct tSuppContext {

    // AP (peer) identification information
    tAniMacAddr authMac;
    v_U8_t *ssidStr;

    // Local association point
    tAniMacAddr suppMac;

    // Keys derived on supplicant
    v_U8_t ptk[AAG_PRF_MAX_OUTPUT_SIZE];
    v_U8_t pwKeyLen; // # of bytes of PTK to send to LIM
    tAniPacket *pmk; // MS-MPPE-Recv-Key
    tAniPacket *serverKey; // MS-MPPE-Send-Key

    // Misc. authentication related state
    eCsrAuthType authType;
    eCsrEncryptionType pwCipherType;
    eCsrEncryptionType grpCipherType;
    tAniPacket *ieBp;
    tAniPacket *ieAp;
    v_BOOL_t firstTimeAuth;

};

typedef struct tAniEapolKeyAvailEventData {
    void *keyDesc;
    tAniPacket *eapolFrame;
} tAniEapolKeyAvailEventData;

typedef struct tAniAagTimerEventData {
    vos_timer_t timer;
    void *appData;
} tAniAagTimerEventData;


/**
 * Callback funtion that sets some status for a given STA context,
 * e.g., the status of the controlled port.
 */
#if 0
typedef int (*tAagSetStatus)(tStaContext *ctx);

typedef int (*tAagTxEapolSupp)(tSuppContext *ctx);
typedef int (*tAagSetStatusSupp)(tSuppContext *ctx);
typedef int (*tAagSendEventToFsmSupp)(tSuppContext *ctx);
#endif

/**
 * Callback function that posts a XXX_TIMER_EXPIRED event when a timer
 * goes off. XXX represents the kind of timer that caused the event.
 */
typedef void (*tAagTimerCallback)(void *data);

/**
 * Callbacks provided to the GroupKeyFsm module from the FSM Manager
 * module so that it can access procedures needed for network
 * transmission, inter-FSM signalling, and communication with the main
 * application.
 */
/*typedef struct tGroupKeyFsmCallbacks {
    int (*getDefaultWepKeyId)(v_U32_t radioId);
    int (*copyDefaultWepKey)(v_U32_t radioId);
    int (*updateAllSta)(v_U32_t radioId);
} tGroupKeyFsmCallbacks;*/

/**
 * This structure stores contants used by the AuthFsm as defined in
 * [802.1X].
 */
typedef struct tAuthFsmConsts {
    // Amount of time to ignore a misbehaving STA
    v_U16_t quietPeriod;
    // Number of reauthentication attempts allowed before ignoring STA
    v_U8_t reAuthMax;
    // Amount of time to wait for response from STA
    v_U16_t txPeriod;
} tAuthFsmConsts;



/**
 * This structure stores constants used by the AuthRsnFsm as defined in
 * [802.11i].
 */
typedef struct tAuthRsnFsmConsts {
    v_U32_t timeoutPeriod;
    v_U32_t maxTries;
} tAuthRsnFsmConsts;


/**
 * This structure stores contants used by the SuppFsm as defined in
 * [802.1X].
 */
typedef struct tSuppFsmConsts {
    v_U16_t authPeriod;
    v_U16_t heldPeriod;
    v_U16_t startPeriod;
    v_U8_t maxStart;    
} tSuppFsmConsts;

/**
 * This structure stores constants used by the SuppRsnFsm as defined in
 * [802.11i].
 */
typedef struct tSuppRsnFsmConsts {
    v_U32_t timeoutPeriod;
    v_U32_t maxTries;
} tSuppRsnFsmConsts;


/**
 * This structure stores constants used by the AuthRsnGroupKeyFsm as
 * defined in [802.11i].
 */
typedef struct tAuthRsnGroupKeyFsmConsts {
    v_U32_t timeoutPeriod;
    v_U32_t maxTries;
} tAuthRsnGroupKeyFsmConsts;

/**
 * authRsnFsmFree
 *
 * FUNCTION
 * Frees a previously allocated RSN Key FSM in a STA context. If the
 * RSN Key FSM is not yet allocated, then this is an error.
 * 
 * @param ctx the STA context whose FSM instance is to be freed
 *
 * @return ANI_OK if the operation succeeds
 */
int
authRsnFsmFree(tBtampContext *ctx);

/**
 * authRsnFsmProcessEvent
 *
 * FUNCTION
 * Passes an event to the RSN key FSM instance for immediate processing.
 * 
 * @param fsm the RSN Key FSM instance
 * @param eventId the AAG event to process
 * @param arg an optional argument for this event
 *
 * @return ANI_OK if the operation succeeds
 */
int
authRsnFsmProcessEvent(tAuthRsnFsm *fsm, tRsnFsmEvent eventId, void *arg);


/**
 * suppFsmCreate
 *
 * FUNCTION
 * Allocates and initializes the state of an SuppFsm instance for the
 * given STA context.
 * 
 * @parm ctx the supplicant context whose SuppFsm is being created
 *
 * @return ANI_OK if the operation succeeds
 */
int
suppRsnFsmCreate(tBtampContext *ctx);

/**
 * suppFsmFree
 *
 * FUNCTION
 * Frees a previously allocated SuppFsm.
 * 
 * @param suppCtx the supplicant context whose suppFsm is to be freed
 *
 * @return ANI_OK if the operation succeeds
 */
int
suppRsnFsmFree(tBtampContext *ctx);

/**
 * suppFsmProcessEvent
 *
 * FUNCTION
 * Passes an event to the suppFsm for immediate processing.
 * 
 * Note: The pertinent event data is already stored in the context.
 *
 * @param suppFsm the suppFsm
 * @param eventId the AAG event to process
 *
 * @return ANI_OK if the operation succeeds
 */
int
suppRsnFsmProcessEvent(tSuppRsnFsm *fsm, tRsnFsmEvent eventId, void *arg);

#endif // __AAG_FSM_H_
