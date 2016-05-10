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

/**=============================================================================
  
  vos_list.c
  
  \brief
  
  Description...
    
  
               Copyright 2008 (c) Qualcomm, Incorporated.
               All Rights Reserved.
               Qualcomm Confidential and Proprietary.
  
  ============================================================================== */
/* $HEADER$ */
#include "bapRsnTxRx.h"
#include "bapRsn8021xFsm.h"
#include "bapInternal.h"
#include "vos_trace.h"
#include "wlan_qct_tl.h"
#include "vos_memory.h"


static pnfTxCompleteHandler bapRsnFsmTxCmpHandler;
static pnfRxFrameHandler bapRsnFsmRxFrameHandler;

extern int gReadToSetKey;


VOS_STATUS bapRsnRegisterTxRxCallbacks( pnfTxCompleteHandler pfnTxCom, pnfRxFrameHandler pnfRxFrame )
{
    if( bapRsnFsmTxCmpHandler || bapRsnFsmRxFrameHandler )
    {
        return VOS_STATUS_E_ALREADY;
    }

    bapRsnFsmTxCmpHandler = pfnTxCom;
    bapRsnFsmRxFrameHandler = pnfRxFrame;

    return ( VOS_STATUS_SUCCESS );
}

void bapRsnClearTxRxCallbacks(void)
{
    bapRsnFsmTxCmpHandler = NULL;
    bapRsnFsmRxFrameHandler = NULL;
}


//To reserve a vos_packet for Tx eapol frame
//If success, pPacket is the packet and pData points to the head.
static VOS_STATUS bapRsnAcquirePacket( vos_pkt_t **ppPacket, v_U8_t **ppData, v_U16_t size )
{
    VOS_STATUS status;
    vos_pkt_t *pPacket;

    status = vos_pkt_get_packet( &pPacket, VOS_PKT_TYPE_TX_802_11_MGMT, size, 1, 
                                    VOS_TRUE, NULL, NULL );
    if( VOS_IS_STATUS_SUCCESS( status ) )
    {
        status = vos_pkt_reserve_head( pPacket, (v_VOID_t **)ppData, size );
        if( !VOS_IS_STATUS_SUCCESS( status ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "bapRsnAcquirePacket failed to reserve size = %d\n", size );
            vos_pkt_return_packet( pPacket );
        }
        else
        {
            *ppPacket = pPacket;
        }
    }
    else
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "bapRsnAcquirePacket failed to get vos_pkt\n" );
    }

    return ( status );
}


static VOS_STATUS bapRsnTxCompleteCallback( v_PVOID_t pvosGCtx, vos_pkt_t *pPacket, VOS_STATUS retStatus )
{
    int retVal;
    ptBtampContext btampContext; // use btampContext value  
    tCsrRoamSetKey setKeyInfo;
    tSuppRsnFsm *fsm;

    if (NULL == pvosGCtx) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "pvosGCtx is NULL in %s", __func__);

        return VOS_STATUS_E_FAULT;
    }

    btampContext = VOS_GET_BAP_CB(pvosGCtx); 
    if (NULL == btampContext) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "btampContext is NULL in %s", __func__);

        return VOS_STATUS_E_FAULT;
    }

    fsm = &btampContext->uFsm.suppFsm;
    if (NULL == fsm) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "fsm is NULL in %s", __func__);

        return VOS_STATUS_E_FAULT;
    }

    //If we get a disconect from upper layer before getting the pkt from TL the
    //bapRsnFsmTxCmpHandler could be NULL 
    //VOS_ASSERT( bapRsnFsmTxCmpHandler );

    if( bapRsnFsmTxCmpHandler )
    {
        //Change the state
        //Call auth or supp FSM's handler
        bapRsnFsmTxCmpHandler( pvosGCtx, pPacket, retStatus );
    }
    else
    {
        vos_pkt_return_packet( pPacket );
        return (VOS_STATUS_SUCCESS );
    }

    //fsm->suppCtx->ptk contains the 3 16-bytes keys. We need the last one.
    /*
    We will move the Set key to EAPOL Completion handler. We found a race condition betweem
    sending EAPOL frame and setting Key */
    if (BAP_SET_RSN_KEY == gReadToSetKey) {
        vos_mem_zero( &setKeyInfo, sizeof( tCsrRoamSetKey ) );
        setKeyInfo.encType = eCSR_ENCRYPT_TYPE_AES;
        setKeyInfo.keyDirection = eSIR_TX_RX;
        vos_mem_copy( setKeyInfo.peerMac, fsm->suppCtx->authMac, sizeof( tAniMacAddr ) );
        setKeyInfo.paeRole = 0; //this is a supplicant
        setKeyInfo.keyId = 0;   //always
        setKeyInfo.keyLength = CSR_AES_KEY_LEN; 
        vos_mem_copy( setKeyInfo.Key, (v_U8_t *)fsm->suppCtx->ptk + (2 * CSR_AES_KEY_LEN ), CSR_AES_KEY_LEN );

        if( !VOS_IS_STATUS_SUCCESS( bapSetKey( fsm->ctx->pvosGCtx, &setKeyInfo ) ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, " Supp: gotoStateStaKeySet fail to set key\n" );
            retVal = ANI_ERROR;
        }
        gReadToSetKey = BAP_RESET_RSN_KEY;
    }

    return (VOS_STATUS_SUCCESS );
}


static VOS_STATUS bapRsnTxFrame( v_PVOID_t pvosGCtx, vos_pkt_t *pPacket )
{
    VOS_STATUS status;
    WLANTL_MetaInfoType metaInfo;

    vos_mem_zero( &metaInfo, sizeof( WLANTL_MetaInfoType ) );
    metaInfo.ucIsEapol = 1; //only send eapol frame
    status = WLANTL_TxBAPFrm( pvosGCtx, pPacket, &metaInfo, bapRsnTxCompleteCallback );
    if( !VOS_IS_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "bapRsnTxFrame failed to send vos_pkt status = %d\n", status );
    }

    return ( status );
}


/*
    \brief bapRsnSendEapolFrame
    To push an eapol frame to TL. 

    \param pAniPkt - a ready eapol frame that is prepared in tAniPacket format
*/
VOS_STATUS bapRsnSendEapolFrame( v_PVOID_t pvosGCtx, tAniPacket *pAniPkt )
{
    VOS_STATUS status;
    vos_pkt_t *pPacket = NULL;
    v_U8_t *pData, *pSrc;
    int pktLen = aniAsfPacketGetBytes( pAniPkt, &pSrc );

    if( pktLen <= 0 )
    {
        return VOS_STATUS_E_EMPTY;
    }
    status = bapRsnAcquirePacket( &pPacket, &pData, pktLen );
    if( VOS_IS_STATUS_SUCCESS( status ) && ( NULL != pPacket ))
    {
        vos_mem_copy( pData, pSrc, pktLen );
        //Send the packet, need to check whether we have an outstanding packet first.
        status = bapRsnTxFrame( pvosGCtx, pPacket );
        if( !VOS_IS_STATUS_SUCCESS( status ) )
        {
            vos_pkt_return_packet( pPacket );
        }
    }

    return ( status );
}


//TL call this function on Rx frames, should only be EAPOL frames
VOS_STATUS bapRsnRxCallback( v_PVOID_t pv, vos_pkt_t *pPacket )
{
    //Callback to auth or supp FSM's handler
    VOS_ASSERT( bapRsnFsmRxFrameHandler );
    if( bapRsnFsmRxFrameHandler )
    {
        bapRsnFsmRxFrameHandler( pv, pPacket );
    }
    else
    {
        //done
        vos_pkt_return_packet( pPacket );
    }

    return ( VOS_STATUS_SUCCESS );
}



VOS_STATUS bapRsnRegisterRxCallback( v_PVOID_t pvosGCtx )
{
    VOS_STATUS status;

    status = WLANTL_RegisterBAPClient( pvosGCtx, WLANBAP_RxCallback, WLANBAP_TLFlushCompCallback );
    if( !VOS_IS_STATUS_SUCCESS( status ) )
    {
        if( VOS_STATUS_E_EXISTS != status )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "bapRsnRegisterRxCallback failed with status = %d\n", status );
        }
        else
        {
            //We consider it ok to register it multiple times because only BAP's RSN should call this
            status = VOS_STATUS_SUCCESS;
        }
    }

    return ( status );
}


