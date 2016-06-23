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
 * $File: //depot/software/projects/feature_branches/gen5_phase1/os/linux/classic/ap/apps/ssm/auth8021x/ani8021xSuppRsnFsm.c $
 *
 * Contains definitions for the RSN EAPOL-Key FSM on the
 * supplicant side. This is based on 802.11i.
 *
 * Author:      Mayank D. Upadhyay
 * Date:        19-December-2002
 * History:-
 * Date         Modified by     Modification Information
 * ------------------------------------------------------
 *
 */
#include "bapRsnSsmServices.h"
#include "bapRsnSsmEapol.h"
#include "bapRsnErrors.h"
#include "bapRsn8021xSuppRsnFsm.h"
#include "vos_utils.h"
#include "bapRsnTxRx.h"
#include "btampFsm.h"

// The different states that this FSM transitions through
#define INITIALIZE           0
#define AUTHENTICATION       1
#define GOT_PMK              2
#define STA_KEY_START        3
#define STA_KEY_SET          4
#define KEY_UPDATE           5   
#define REKEY_MSG            6
#define GROUP_KEY_SET        7
#define NUM_STATES (GROUP_KEY_SET + 1)


static tSuppRsnFsmConsts suppConsts = { 2000, 3 };    //timeout, retry limit

int gReadToSetKey;

/**************************************
 * Static functions in this module
 **************************************/

static 
int zeroOutPtk(tSuppRsnFsm *fsm);

static
int checkMic(tSuppRsnFsm *fsm, 
             tAniEapolKeyAvailEventData *data,
             v_BOOL_t pwKeyExchange);

static int checkInfoElement(tSuppRsnFsm *fsm,
                            tAniEapolKeyAvailEventData *data);

static
int checkPeerReplayCounter(tSuppRsnFsm *fsm, 
                           tAniEapolKeyAvailEventData *data,
                           v_BOOL_t *retransmit,
                           v_BOOL_t actualMicFlag,
                           v_BOOL_t reTxMicFlag
                           );

static
int derivePtk(tSuppRsnFsm *fsm, 
              tAniEapolKeyAvailEventData *data);

static 
int checkTransition(tSuppRsnFsm *fsm, void *arg);

static int
gotoStateInit(tSuppRsnFsm *fsm);

static int suppRsnRxFrameHandler( v_PVOID_t pvosGCtx, vos_pkt_t *pPacket );
static int suppRsnTxCompleteHandler( v_PVOID_t pvosGCtx, vos_pkt_t *pPacket, VOS_STATUS retStatus );

/*************************
 * Internal Functions
 *************************/
int suppRsnAuthStartEventHandler(tSuppRsnFsm *fsm);

/*************************
 * The exported functions
 *************************/

/**
 * suppRsnFsmInit
 *
 * FUNCTION:
 * Initializes the constants and the callbacks needed by this FSM
 * module.
 *
 * @param cb callbacks to the various procedures needed by this FSM
 *
 * @return ANI_OK if the operation succeeds
 */
int
suppRsnFsmInit(void)
{
    // TODO: Read the constants in from config
    // consts = constsIn;
    suppConsts.timeoutPeriod = 2000;
    suppConsts.maxTries = 3;

    return ANI_OK;
}

/**
 * suppRsnFsmCreate
 *
 * FUNCTION
 * Allocates and initializes the state of an RSN key FSM instance for
 * the given BP context.
 * 
 * @parm ctx the BP context whose instance is being created
 * @param pskBased pass in eANI_BOOLEAN_TRUE is this BP is to be
 * authenticated based on a pre-shared key as opposed to EAP.
 *
 * @return ANI_OK if the operation succeeds
 */
int
suppRsnFsmCreate(tBtampContext *ctx)
{
    int retVal = ANI_OK;
    tSuppRsnFsm *fsm = &ctx->uFsm.suppFsm;

    // First, clear everything out
    vos_mem_zero( fsm, sizeof(tSuppRsnFsm));

    if( !VOS_IS_STATUS_SUCCESS( bapRsnRegisterTxRxCallbacks( suppRsnTxCompleteHandler,
                                            suppRsnRxFrameHandler ) ) )
    {
        return ANI_ERROR;
    }

    if( !VOS_IS_STATUS_SUCCESS( bapRsnRegisterRxCallback( ctx->pvosGCtx ) ) )
    {
        return ANI_ERROR;
    }

    // Allocate the supplicant context
    fsm->suppCtx = (tSuppContext *)vos_mem_malloc( sizeof(tSuppContext) );
    if (fsm->suppCtx == NULL) 
    {
        retVal = ANI_E_MALLOC_FAILED;
        VOS_ASSERT( 0 );
        goto error;
    }
    // Clear out the supplicant context
    vos_mem_zero( fsm->suppCtx, sizeof(tSuppContext));

    fsm->ctx = ctx;
    //Only support CCMP
    fsm->suppCtx->pwCipherType = eCSR_ENCRYPT_TYPE_AES;

    retVal = aniAsfPacketAllocateExplicit(&fsm->lastEapol,
                                          RSN_MAX_PACKET_SIZE,
                                          EAPOL_TX_HEADER_SIZE);
    if (retVal != ANI_OK) 
    {
        VOS_ASSERT( 0 );
        goto error;
    }

    aniAsfPacketAllocate(&fsm->suppCtx->pmk);
    if (fsm->suppCtx->pmk == NULL) 
    {
        retVal = ANI_E_MALLOC_FAILED;
        VOS_ASSERT( 0 );
        goto error;
    }

    fsm->suppCtx->ieAp = NULL; 

    fsm->cryptHandle = 0;
    if( !VOS_IS_STATUS_SUCCESS( vos_crypto_init( &fsm->cryptHandle ) ) )
    {
        retVal = ANI_E_FAILED;
        VOS_ASSERT( 0 );
    }

    fsm->currentState = INITIALIZE;
    gotoStateInit(fsm);

    suppRsnFsmProcessEvent( fsm, RSN_FSM_AUTH_START, NULL );

    return ANI_OK;

 error:
    suppRsnFsmFree( ctx );

    return retVal;
    
}

/**
 * suppRsnFsmFree
 *
 * FUNCTION
 * Frees a previously allocated RSN Key FSM in a BP context. If the
 * RSN Key FSM is not yet allocated, then this is an error.
 * 
 * @param ctx the BP context whose FSM instance is to be freed
 *
 * @return ANI_OK if the operation succeeds
 */
int
suppRsnFsmFree(tBtampContext *ctx)
{
    tSuppRsnFsm *fsm;

    fsm = &ctx->uFsm.suppFsm;

    if( fsm->cryptHandle )
    {  
        vos_crypto_deinit( fsm->cryptHandle );
    }

    bapRsnClearTxRxCallbacks(); 

    if (fsm->lastEapol)
        aniAsfPacketFree(fsm->lastEapol);

    if( fsm->suppCtx )
    {
        if ( fsm->suppCtx->pmk )
        {
            aniAsfPacketFree(fsm->suppCtx->pmk);
        }
        vos_mem_free( fsm->suppCtx );
    }

    // Finally, clear everything out
    vos_mem_zero( fsm, sizeof(tSuppRsnFsm));

    return ANI_OK;
}

/**
 * suppRsnFsmProcessEvent
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
suppRsnFsmProcessEvent(tSuppRsnFsm *fsm, tRsnFsmEvent eventId, void *arg)
{

    switch (eventId) 
    {
    case RSN_FSM_TIMER_EXPIRED:
        // Proceed straight to checkTransition
        break;
    case RSN_FSM_AUTH_START:
        fsm->authReq = eANI_BOOLEAN_TRUE;
        suppRsnAuthStartEventHandler(fsm);
        break;
    case RSN_FSM_EAPOL_FRAME_AVAILABLE:
        fsm->eapolAvail = eANI_BOOLEAN_TRUE;
        break;
    case RSN_FSM_INTEG_FAILED:
        fsm->integFailed = eANI_BOOLEAN_TRUE;
        break;
    default:
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "Supp unknown event for SuppFsm: %d\n",
                      eventId);
        VOS_ASSERT( 0 );
        return ANI_E_ILLEGAL_ARG;
        break;
    }

    checkTransition(fsm, arg);

    return ANI_OK;
}

int
suppRsnAuthStartEventHandler(tSuppRsnFsm *fsm)
{
    // Copy required info
    vos_mem_copy( &fsm->suppCtx->authMac, fsm->ctx->peer_mac_addr, 6);
    vos_mem_copy( &fsm->suppCtx->suppMac, fsm->ctx->self_mac_addr, 6);
    aniAsfPacketAppendBuffer( fsm->suppCtx->pmk, fsm->ctx->key_material, fsm->ctx->key_length);

    return ANI_OK;
}

/***********************
 * The static functions
 ***********************/

static int
gotoStateInit(tSuppRsnFsm *fsm)
{
    fsm->currentState = INITIALIZE;

    fsm->authReq = eANI_BOOLEAN_FALSE;
    fsm->eapolAvail = eANI_BOOLEAN_FALSE;
    fsm->integFailed = eANI_BOOLEAN_FALSE;
    fsm->pmkAvail = eANI_BOOLEAN_FALSE;

    // Create two replay counter's..one for our requests, and another
    // for STA's requests. Initialize the first one randomly.
    aniSsmReplayCtrCreate(fsm->cryptHandle, &fsm->localReplayCtr, ANI_EAPOL_KEY_RSN_RSC_SIZE, 0);
    aniSsmReplayCtrCreate(fsm->cryptHandle, &fsm->peerReplayCtr, ANI_EAPOL_KEY_RSN_RSC_SIZE, 0);

    return ANI_OK;
}

static int
gotoStateAuthentication(tSuppRsnFsm *fsm)
{
    fsm->currentState = AUTHENTICATION;

    if( VOS_IS_STATUS_SUCCESS( vos_rand_get_bytes( fsm->cryptHandle, fsm->sNonce, ANI_EAPOL_KEY_RSN_NONCE_SIZE ) ) )
    {
        zeroOutPtk(fsm);
        // TODO: Zero out all GTK's
        fsm->authReq = eANI_BOOLEAN_FALSE;
        /////getPmk(fsm->suppCtx);
    }
    else
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "Supp fail to random number\n" );
        return ANI_ERROR;
    }

    return ANI_OK;
}

static int
gotoStateGotPmk(tSuppRsnFsm *fsm)
{
    fsm->currentState = GOT_PMK;

    return ANI_OK;
}

static int
gotoStateStaKeyStart(tSuppRsnFsm *fsm, 
                     tAniEapolKeyAvailEventData *data,
                     v_BOOL_t retransmit)
{
    int retVal;
    tAniEapolRsnKeyDesc txDesc;
    tAniEapolRsnKeyDesc *rxDesc;
    static v_U8_t btampRSNIE[] = {0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 
        0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x00 };

    fsm->currentState = STA_KEY_START;

    // Create a new EAPOL frame if we don't have to retransmit
   // if (!retransmit) 
    //{

        rxDesc = data->keyDesc;
        if( NULL == rxDesc)
        {
            return ANI_E_NULL_VALUE;
        }
        aniAsfPacketEmptyExplicit( fsm->lastEapol, EAPOL_TX_HEADER_SIZE );
        retVal = derivePtk(fsm, data);
        if( !ANI_IS_STATUS_SUCCESS( retVal ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Supp derivePtk failed with code %d!\n", retVal);
            return retVal;
        }

        vos_mem_zero( &txDesc, sizeof(txDesc) );

        // The Key Information bits...
        if (fsm->suppCtx->pwCipherType == eCSR_ENCRYPT_TYPE_AES) 
        {
            txDesc.info.keyDescVers = ANI_EAPOL_KEY_DESC_VERS_AES;
        }
        txDesc.info.unicastFlag = eANI_BOOLEAN_TRUE;
        txDesc.info.micFlag = eANI_BOOLEAN_TRUE;
        txDesc.keyLen = 0; //RSN_80211_KEY_LEN;

        // Send back the same replayCtr that the authenticator sent
        vos_mem_copy(txDesc.replayCounter, 
               rxDesc->replayCounter, 
               sizeof(txDesc.replayCounter));

        vos_mem_copy(txDesc.keyNonce, fsm->sNonce, sizeof(txDesc.keyNonce));
        txDesc.keyDataLen = sizeof(btampRSNIE);//aniAsfPacketGetBytes(fsm->suppCtx->ieBp,
                                               //&txDesc.keyData);    
        txDesc.keyData = btampRSNIE;
        retVal = aniEapolWriteKey(fsm->cryptHandle,
                                  fsm->lastEapol,
                                  fsm->suppCtx->authMac,
                                  fsm->suppCtx->suppMac,
                                  ANI_EAPOL_KEY_DESC_TYPE_RSN_NEW,
                                  &txDesc,
                                  fsm->suppCtx->ptk, 
                                  CSR_AES_KEY_LEN);
        if( !ANI_IS_STATUS_SUCCESS( retVal ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "Supp gotoStateStaKeyStart fail to write key %d\n",
                      retVal);
            return retVal;
        }
    //}
 
    if( VOS_IS_STATUS_SUCCESS( bapRsnSendEapolFrame( fsm->ctx->pvosGCtx, fsm->lastEapol ) ) )
    {
        retVal = ANI_OK;
    }
    else
    {
        retVal = ANI_ERROR;
    }

    return retVal;
}

static int
gotoStateStaKeySet(tSuppRsnFsm *fsm, 
                   tAniEapolKeyAvailEventData *data,
                   v_BOOL_t retransmit)
{
    int retVal=0;
    tAniEapolRsnKeyDesc txDesc;
    tAniEapolRsnKeyDesc *rxDesc = NULL;
    
    fsm->currentState = STA_KEY_SET;

    if (data == NULL) 
    {
        // We don't need to do anything
        return ANI_OK;
    }
    
    // Create a new EAPOL frame if we don't have to retransmit
    if (!retransmit) 
    {
        // First check the IE that the AP sent
        retVal = checkInfoElement(fsm, data);
        if (retVal != ANI_OK) 
        {
            //FIX_RSN aagSetSuppFailureAndCleanup(fsm->suppCtx);

            // FSM does not exist after this...

            return retVal;
        }

        // Create a new EAPOL frame

        rxDesc = data->keyDesc;
        if( NULL == rxDesc )
            return ANI_E_NULL_VALUE;

        aniAsfPacketEmptyExplicit(fsm->lastEapol, 
                                  EAPOL_TX_HEADER_SIZE );

        vos_mem_zero( &txDesc, sizeof(txDesc) );

        // The Key Information bits...
        if (fsm->suppCtx->pwCipherType == eCSR_ENCRYPT_TYPE_AES) 
        {
            txDesc.info.keyDescVers = ANI_EAPOL_KEY_DESC_VERS_AES;
        }

        txDesc.info.unicastFlag = eANI_BOOLEAN_TRUE;
        txDesc.info.micFlag = eANI_BOOLEAN_TRUE;
        txDesc.info.secureFlag = eANI_BOOLEAN_TRUE;
        txDesc.keyLen = 0; //RSN_80211_KEY_LEN;

        // Send back the same replayCtr that the authenticator sent
        vos_mem_copy(txDesc.replayCounter, 
               rxDesc->replayCounter, 
               sizeof(txDesc.replayCounter));

        retVal = aniEapolWriteKey(fsm->cryptHandle,
                                  fsm->lastEapol,
                                  fsm->suppCtx->authMac,
                                  fsm->suppCtx->suppMac,
                                  ANI_EAPOL_KEY_DESC_TYPE_RSN_NEW,
                                  &txDesc,
                                  fsm->suppCtx->ptk,
                                  CSR_AES_KEY_LEN);
        if( !ANI_IS_STATUS_SUCCESS( retVal ) )
        {
            return retVal;
        }
    }
    gReadToSetKey = BAP_SET_RSN_KEY;
    if( !VOS_IS_STATUS_SUCCESS( bapRsnSendEapolFrame( fsm->ctx->pvosGCtx, fsm->lastEapol ) ) )
    {
        /* making it global to access in bapTxRx file */
#if 0
        tCsrRoamSetKey setKeyInfo;


        vos_mem_zero( &setKeyInfo, sizeof( tCsrRoamSetKey ) );
        setKeyInfo.encType = eCSR_ENCRYPT_TYPE_AES;
        setKeyInfo.keyDirection = eSIR_TX_RX;
        vos_mem_copy( setKeyInfo.peerMac, fsm->suppCtx->authMac, sizeof( tAniMacAddr ) );
        setKeyInfo.paeRole = 0; //this is a supplicant
        setKeyInfo.keyId = 0;   //always
        setKeyInfo.keyLength = CSR_AES_KEY_LEN; 
        vos_mem_copy( setKeyInfo.Key, (v_U8_t *)fsm->suppCtx->ptk + (2 * CSR_AES_KEY_LEN ), CSR_AES_KEY_LEN );
        //fsm->suppCtx->ptk contains the 3 16-bytes keys. We need the last one.
        /*
          We will move the Set key to EAPOL Completion handler. We found a race condition betweem
          sending EAPOL frame and setting Key */


        if( !VOS_IS_STATUS_SUCCESS( bapSetKey( fsm->ctx->pvosGCtx, &setKeyInfo ) ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, " Supp: gotoStateStaKeySet fail to set key\n" );
            retVal = ANI_ERROR;
        }
#endif
        gReadToSetKey = BAP_RESET_RSN_KEY;
        retVal = ANI_ERROR;
    }

    return retVal;
}

static int
gotoStateGroupKeySet(tSuppRsnFsm *fsm, 
                     tAniEapolKeyAvailEventData *data)
{
    int retVal;
    tAniEapolRsnKeyDesc txDesc;
    tAniEapolRsnKeyDesc *rxDesc;

    int groupKeyLen;
    
    fsm->currentState = GROUP_KEY_SET;
    
    do
    {
        rxDesc = (tAniEapolRsnKeyDesc *) data->keyDesc;
        if( NULL == rxDesc) 
        {
            retVal = ANI_E_NULL_VALUE;
            break;
        }

        if (rxDesc->keyDataLen == 0 || rxDesc->keyData == NULL) 
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                "Supp: AP sent no group key in group EAPOL-Key message!\n" );
            retVal = ANI_E_ILLEGAL_ARG;
            break;
        }

        if ( rxDesc->info.keyDescVers == ANI_EAPOL_KEY_DESC_VERS_AES ) 
        {
            groupKeyLen = rxDesc->keyDataLen - ANI_SSM_AES_KEY_WRAP_BLOCK_SIZE;
            if( groupKeyLen <= 0 )
            {
                VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                    "Supp: AP sent GTK too short\n" );
                retVal = ANI_E_ILLEGAL_ARG;
                break;
            }
        } 
        else
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                "Supp: AP sent unsupported keyDescVer %d!\n", rxDesc->info.keyDescVers );
            retVal = ANI_E_ILLEGAL_ARG;
            break;
        }

        // Always create a new EAPOL frame

        aniAsfPacketEmptyExplicit( fsm->lastEapol, 
                                  EAPOL_TX_HEADER_SIZE );
        
        vos_mem_zero( &txDesc, sizeof(txDesc) );

        // The Key Information bits...
        if (fsm->suppCtx->grpCipherType == eCSR_ENCRYPT_TYPE_AES) 
        {
            txDesc.info.keyDescVers = ANI_EAPOL_KEY_DESC_VERS_AES;
        }

        txDesc.info.unicastFlag = eANI_BOOLEAN_FALSE;
        txDesc.info.keyId = rxDesc->info.keyId;
        txDesc.info.micFlag = eANI_BOOLEAN_TRUE;
        txDesc.info.secureFlag = eANI_BOOLEAN_TRUE;
        txDesc.keyLen = RSN_80211_KEY_LEN;

        // Send back the same replayCtr that the authenticator sent
        vos_mem_copy(txDesc.replayCounter, 
               rxDesc->replayCounter, 
               sizeof(txDesc.replayCounter));

        retVal = aniEapolWriteKey(fsm->cryptHandle,
                                  fsm->lastEapol,
                                  fsm->suppCtx->authMac,
                                  fsm->suppCtx->suppMac,
                                  ANI_EAPOL_KEY_DESC_TYPE_RSN_NEW,
                                  &txDesc,
                                  fsm->suppCtx->ptk,
                                  CSR_AES_KEY_LEN);
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;    

        if( !VOS_IS_STATUS_SUCCESS( bapRsnSendEapolFrame( fsm->ctx->pvosGCtx, fsm->lastEapol ) ) )
        {
            retVal = ANI_ERROR;
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "Supp could not send eapol. Disconnect\n" );
            break;
        }

        //FIX_RSN there is no need to set GTK retVal = setGtk(fsm->suppCtx, rxDesc->keyRecvSeqCounter);

        // This is never retransmitted
        aniAsfPacketEmptyExplicit( fsm->lastEapol, 
                                  EAPOL_TX_HEADER_SIZE );

        checkTransition(fsm, NULL); // UCT rule
    }while( 0 );

    return retVal;
}

static int
gotoStateKeyUpdate(tSuppRsnFsm *fsm, tSirMicFailureInfo *micFailureInfo)
{
    //we don't update keys
    bapSuppDisconnect( fsm->ctx );

    return ANI_OK;
}

static int
gotoStateRekeyMsg(tSuppRsnFsm *fsm, tSirMicFailureInfo *micFailureInfo)
{
    //We don't support rekey, simply disconnect
    bapSuppDisconnect( fsm->ctx );

    return ANI_OK;
}

static 
int zeroOutPtk(tSuppRsnFsm *fsm)
{
    return ANI_OK;
}

static
int derivePtk(tSuppRsnFsm *fsm, tAniEapolKeyAvailEventData *data)
{
    v_U32_t prfLen;
    tAniEapolRsnKeyDesc *rxDesc;

    switch (fsm->suppCtx->pwCipherType) 
    {
    case eCSR_ENCRYPT_TYPE_AES:
        prfLen = AAG_RSN_PTK_PRF_LEN_CCMP;
        fsm->suppCtx->pwKeyLen = AAG_RSN_KEY_MATERIAL_LEN_CCMP;
        break;
    default:
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "Cannot generate PTK for BP for invalid algorithm %d\n",
                      fsm->suppCtx->pwCipherType);
        return ANI_E_ILLEGAL_ARG;
        break;
    };

    rxDesc = (tAniEapolRsnKeyDesc *) data->keyDesc;
    return aagPtkPrf(fsm->cryptHandle,
                     fsm->suppCtx->ptk,
                     prfLen,
                     fsm->suppCtx->pmk,
                     fsm->suppCtx->authMac,
                     fsm->suppCtx->suppMac,
                     rxDesc->keyNonce,
                     fsm->sNonce);
}

static
int checkMic(tSuppRsnFsm *fsm, 
             tAniEapolKeyAvailEventData *data,
             v_BOOL_t pwKeyExchange)
{
    int retVal;

    retVal = aniEapolKeyCheckMic(fsm->cryptHandle,
                                 data->eapolFrame,
                                 ANI_EAPOL_KEY_DESC_TYPE_RSN_NEW,
                                 data->keyDesc,
                                 fsm->suppCtx->ptk,
                                 CSR_AES_KEY_LEN);

    return retVal;
}


static int checkInfoElement(tSuppRsnFsm *fsm,
                            tAniEapolKeyAvailEventData *data)
{
    tAniEapolRsnKeyDesc *desc;
    v_U8_t *ieApBytes;
    int ieApLen;

    desc = (tAniEapolRsnKeyDesc *) data->keyDesc;
    if( NULL == desc )
    {
        return ANI_E_NULL_VALUE;
    }

    ieApLen = aniAsfPacketGetBytes(fsm->suppCtx->ieAp, &ieApBytes);
    if( ANI_IS_STATUS_SUCCESS( ieApLen ) )
    {

        if ((desc->keyDataLen != ieApLen) ||
            ( vos_mem_compare(desc->keyData, ieApBytes, ieApLen) )) 
        {
            // TODO: Send a fault here
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                         "Supp AP sent inconsistent RSN IE!\n" );
            return ANI_E_FAILED;
        }
    }

    return ANI_OK;
}

static 
int checkPeerReplayCounter(tSuppRsnFsm *fsm, 
                           tAniEapolKeyAvailEventData *data,
                           v_BOOL_t *retransmit,
                           v_BOOL_t actualMicFlag,
                           v_BOOL_t reTxMicFlag)
{
    int retVal = ANI_OK;
    int cmp;
    tAniEapolRsnKeyDesc *rxDesc;
    
    rxDesc = data->keyDesc;
    if( NULL == rxDesc )
    {
        return ANI_E_NULL_VALUE;
    }

    *retransmit = eANI_BOOLEAN_FALSE;
    
    cmp = aniSsmReplayCtrCmp(fsm->peerReplayCtr, rxDesc->replayCounter);
    
    // The AP should send us a replay counter greater than or equal to
    // the last one it sent
    /*Unless we are forgiving with this we will have interop issues with some vendros like CSR*/
    if (cmp > 0) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
            "BP got old EAPOL replay counter from AP" );
        retVal = ANI_E_REPLAY_CHECK_FAILED;

    } 
    else if (cmp <= 0) 
    {
        if ( actualMicFlag == reTxMicFlag ) 
        { 
            *retransmit = eANI_BOOLEAN_TRUE; 
        }
    }

    return retVal;
}

static 
int checkTransition(tSuppRsnFsm *fsm, void *arg)
{
    tAniEapolKeyAvailEventData *data;
    tAniEapolRsnKeyDesc *rxDesc;
    v_BOOL_t retransmit;
    int retVal;

    if (fsm->authReq) 
    {
        gotoStateAuthentication(fsm);
        return ANI_OK;
    }

    switch (fsm->currentState) 
    {
    case INITIALIZE:
        break;
    case AUTHENTICATION:
        gotoStateGotPmk(fsm);
        checkTransition(fsm, arg);
        break;
    case GOT_PMK:
        if (fsm->eapolAvail) {

            fsm->eapolAvail = eANI_BOOLEAN_FALSE;
            data = (tAniEapolKeyAvailEventData *) arg;
            rxDesc = (tAniEapolRsnKeyDesc *) data->keyDesc;

            if (rxDesc->info.ackFlag) 
            {

                aniSsmReplayCtrUpdate(fsm->peerReplayCtr,
                                      rxDesc->replayCounter);

                // Going from one state to another cannot be a retransmit
                retVal = gotoStateStaKeyStart(fsm, data, eANI_BOOLEAN_FALSE);

            }
        }
        break;
    case STA_KEY_START:
        if (fsm->eapolAvail) {

            fsm->eapolAvail = eANI_BOOLEAN_FALSE;
            data = (tAniEapolKeyAvailEventData *) arg;
            rxDesc = (tAniEapolRsnKeyDesc *) data->keyDesc;

            if (rxDesc->info.ackFlag) {

                retVal = checkPeerReplayCounter(
                        fsm, 
                        data, 
                        &retransmit, 
                        rxDesc->info.micFlag,
                        0);  // MIC not set means check for re-Tx M1.
                if (retVal != ANI_OK)
                    return ANI_OK; // Caller should not fail

                if (retransmit) {

                    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                                    "Resending EAPOL-Key Msg2 from "
                                  "supplicant to AP" );
                    retVal = gotoStateStaKeyStart(fsm, data, eANI_BOOLEAN_TRUE);

                } 
                else {
                    retVal = checkMic(fsm, data, rxDesc->info.unicastFlag);
                    if (retVal != ANI_OK) 
                    {
                        bapSuppDisconnect( fsm->ctx );
                        return retVal;
                    }

                    aniSsmReplayCtrUpdate(fsm->peerReplayCtr,
                                          rxDesc->replayCounter);

                    gotoStateStaKeySet(fsm, data, eANI_BOOLEAN_FALSE);

                }
            }
        }
        break;
    case STA_KEY_SET:
        if (fsm->eapolAvail) 
        {
            fsm->eapolAvail = eANI_BOOLEAN_FALSE;
            data = (tAniEapolKeyAvailEventData *) arg;
            rxDesc = (tAniEapolRsnKeyDesc *) data->keyDesc;

            retVal = checkPeerReplayCounter(
                        fsm, 
                        data, 
                        &retransmit, 
                        rxDesc->info.micFlag,
                        1);  // MIC set means check for re-Tx M3.
            if (retVal != ANI_OK)
                return ANI_OK; // Caller should not fail

            if (!retransmit) 
            {
                retVal = checkMic(fsm, data, rxDesc->info.unicastFlag);
                if (retVal != ANI_OK) 
                {
                    bapSuppDisconnect( fsm->ctx );
                    return retVal;
                }
                aniSsmReplayCtrUpdate(fsm->peerReplayCtr,
                                      rxDesc->replayCounter);

            }

            if (rxDesc->info.unicastFlag) 
            {
                /* 
                 * Handle pairwise key message...in this state
                 * pairwise key messages can only be for retransmissions.
                 */
                if (retransmit) 
                {
                    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                                   "Resending EAPOL-Key Msg4 from "
                                  "supplicant \n" );
                    retVal = gotoStateStaKeySet(fsm, data, eANI_BOOLEAN_TRUE);
                }
            }
            else 
            {
                /*
                 * Handle group key message...with group key messages,
                 * the replay counter has to change on
                 * retransmissions.
                 */
                if (!retransmit) 
                {
                    retVal = gotoStateGroupKeySet(fsm, data);
                    if( !ANI_IS_STATUS_SUCCESS( retVal ) )
                    {
                        bapSuppDisconnect( fsm->ctx );
                        return retVal;
                    }
                }
            } 
        } 
        else {
            if (fsm->integFailed) 
            {
                gotoStateKeyUpdate(fsm, arg);
            }
        }
        break;
    case GROUP_KEY_SET:
        gotoStateStaKeySet(fsm, NULL, eANI_BOOLEAN_FALSE);
        break;
    case KEY_UPDATE:
        gotoStateRekeyMsg(fsm, arg);
        break;
    default:
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "Illegal state for SuppRsnFsm: %d",
                      fsm->currentState);
        VOS_ASSERT( 0 );
        return ANI_E_FAILED;
    }

    return ANI_OK;
}



static int
suppEapolKeyHandler( tSuppRsnFsm *fsm,
                    tAniPacket *eapolFrame, 
                    tAniMacAddr suppMac)
{
    int retVal;
    
    int descType;
    void *keyDesc;
    tAniEapolRsnKeyDesc *rsnDesc;
    tAniEapolKeyAvailEventData data;

    do
    {
        retVal = aniEapolParseKey(eapolFrame, &descType, &keyDesc);
        if( !ANI_IS_STATUS_SUCCESS( retVal ) )
        {
            return retVal;
        }

        if (descType == ANI_EAPOL_KEY_DESC_TYPE_RSN_NEW) 
        {
            rsnDesc = (tAniEapolRsnKeyDesc *) keyDesc;
            /*
             * Pass on the event to the RSN FSM irrespective if it is
             * pairwise or not.
             */
            data.keyDesc = keyDesc;
            data.eapolFrame = eapolFrame;
            retVal = suppRsnFsmProcessEvent(fsm, 
                                            RSN_FSM_EAPOL_FRAME_AVAILABLE,
                                            &data);
        } 
        else {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "Supp: Got unexpected 802.1x RC4 Key message \n" );
            retVal = ANI_E_FAILED;
            break;
        }
    }while( 0 );

    aniEapolKeyFreeDesc(descType, keyDesc);
    return retVal;
}



//
//This function alwasy assume the incoming vos_packet is 802_3 frame.
static int suppRsnRxFrameHandler( v_PVOID_t pvosGCtx, vos_pkt_t *pPacket )
{
    int retVal = ANI_ERROR;
    tAniPacket *pAniPacket;
    tBtampContext *ctx;
    tSuppRsnFsm *fsm;
    /* Validate params */ 
    if ((pvosGCtx == NULL) || (NULL == pPacket))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "param is NULL in %s", __func__);

        return retVal;
    }
    ctx = (tBtampContext *)VOS_GET_BAP_CB( pvosGCtx );
    if (NULL == ctx) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "ctx is NULL in %s", __func__);

        return retVal;
    }

    fsm = &ctx->uFsm.suppFsm;
    if (NULL == fsm) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "fsm is NULL in %s", __func__);

        return retVal;
    }

    do
    {
        //ToDO: We need to synchronize this. For now, use the simplest form, drop the packet comes later.
        if( fsm->fReceiving )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                " ******suppRsnRxFrameHandler receive eapol packet while processing. Drop the new comer\n" );
            break;
        }
        fsm->fReceiving = VOS_TRUE;
        retVal = bapRsnFormPktFromVosPkt( &pAniPacket, pPacket );
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;
        //Now we can process the eapol frame
        //handler will free the pAniPacket
        bapRsnEapolHandler( fsm, pAniPacket, VOS_FALSE );
    }while( 0 );

    fsm->fReceiving = VOS_FALSE;
    vos_pkt_return_packet( pPacket );

    return retVal;
}


static int suppRsnTxCompleteHandler( v_PVOID_t pvosGCtx, vos_pkt_t *pPacket, VOS_STATUS retStatus )
{
    tBtampContext *ctx = (tBtampContext *)VOS_GET_BAP_CB( pvosGCtx );
    tAuthRsnFsm *fsm;

    vos_pkt_return_packet( pPacket );
    if (pvosGCtx == NULL)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "param is NULL in %s", __func__);

        return ANI_ERROR;
    }

    if (NULL == ctx) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "ctx is NULL in %s", __func__);

        return ANI_ERROR;
    }

    fsm = &ctx->uFsm.authFsm;
    if (NULL == fsm) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "fsm is NULL in %s", __func__);

        return ANI_ERROR;
    }

    //Synchronization needed
    
    if(!VOS_IS_STATUS_SUCCESS( retStatus ) )
    {
        //This is bad.
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
            "Supp: TL Tx complete with error %d current state is %d\n", retStatus, fsm->currentState );
        if( fsm->numTries <= suppConsts.maxTries )
        {
            //retransmit
            fsm->numTries++;
            if( !VOS_IS_STATUS_SUCCESS( bapRsnSendEapolFrame( fsm->ctx->pvosGCtx, fsm->lastEapol ) ) )
            {
                bapSuppDisconnect( fsm->ctx->pvosGCtx );
            }
        }
        else
        {
            bapSuppDisconnect( fsm->ctx->pvosGCtx );
        }
    }

    return ANI_OK;
}


/**
 * suppEapolHandler
 *
 * Handles an incoming EAPOL frame on the supplicant side.
 *
 * @param eapolFrame the packet containing the EAPOL frame, with the
 * head of the packet still at the start of the EAPOL frame
 * @param dstMac the dstMac pointing inside the frame
 * @param srcMac the srcMac pointing inside the frame
 * @param type the type pointing inside the frame at the type field
 *
 * @return ANI_OK if the operation succeeds
 */
void suppEapolHandler( tSuppRsnFsm *fsm, tAniPacket *eapolFrame,
                 tAniMacAddr dstMac, 
                 tAniMacAddr srcMac,
                 v_U8_t *type )
{
    switch (*type) 
    {
    case ANI_EAPOL_TYPE_PACKET:
        // Ignore EAP becasue it is only WPA2-PSK
        break;
    case ANI_EAPOL_TYPE_KEY:
        suppEapolKeyHandler( fsm, eapolFrame, dstMac );
        break;
    case ANI_EAPOL_TYPE_ASF_ALERT:
    default:
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
            "Supp: EAPOL type not implemented: %.2x\n", *type);
        break;
    }
}
