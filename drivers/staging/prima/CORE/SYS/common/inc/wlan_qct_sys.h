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

#if !defined( WLAN_QCT_SYS_H__ )
#define WLAN_QCT_SYS_H__

/**===========================================================================

  \file  wlan_qct_sys.h

  \brief System module API

  ==========================================================================*/

/* $HEADER$ */

/*---------------------------------------------------------------------------
  Include files
  -------------------------------------------------------------------------*/
#include <vos_types.h>
#include <vos_status.h>
#include <vos_mq.h>

/*---------------------------------------------------------------------------
  Preprocessor definitions and constants
  -------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------
  Type declarations
  -------------------------------------------------------------------------*/

  /*----------------------------------------------------------------------------

  \brief sysResponseCback() - SYS async resonse callback

  This is a protype for the callback function that SYS makes to various
  modules in the system.

  \param  pUserData - user data that is passed to the Callback function
                      when it is invoked.

  \return Nothing

  \sa sysMcStart(), sysMcThreadProbe(), sysTxThreadProbe()

  --------------------------------------------------------------------------*/
typedef v_VOID_t ( * sysResponseCback ) ( v_VOID_t *pUserData );



typedef enum
{
   SYS_MSG_ID_MC_START,
   SYS_MSG_ID_MC_THR_PROBE,
   SYS_MSG_ID_MC_TIMER,

   SYS_MSG_ID_TX_THR_PROBE,
   SYS_MSG_ID_TX_TIMER,

   SYS_MSG_ID_RX_TIMER,

   SYS_MSG_ID_MC_STOP,
   SYS_MSG_ID_FTM_RSP,

} SYS_MSG_ID;

/*---------------------------------------------------------------------------
  Preprocessor definitions and constants
  -------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
  Function declarations and documenation
  -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------

  \brief sysBuildMessageHeader() - Build / initialize a SYS message header

  This function will initialize the SYS message header with the message type
  and any internal fields needed for a new SYS message.  This function sets
  all but the message body, which is up to the caller to setup based on the
  specific message being built.

  \note There are internal / reserved items in a SYS message that must be
        set correctly for the message to be recognized as a SYS message by
        the SYS message handlers.  It is important for every SYS message to
        be setup / built / initialized through this function.

  \param sysMsgId - a valid message ID for a SYS message.  See the
         SYS_MSG_ID enum for all the valid SYS message IDs.

  \param pMsg - pointer to the message structure to be setup.

  \return

  \sa

  --------------------------------------------------------------------------*/
VOS_STATUS sysBuildMessageHeader( SYS_MSG_ID sysMsgId, vos_msg_t *pMsg );

/*----------------------------------------------------------------------------

  \brief sysOpen() - Open (initialize) the SYS module.

  This function opens the SYS modules.  All SYS resources are allocated
  as a result of this open call.

  \param pVosContext - pointer to the VOS Context (from which all other
         context entities can be derived).

  \return VOS_STATUS_SUCCESS - the SYS module is open.  All resources needed
          for operation of the SYS modules are allocated and initialized.

          VOS_STATUS_E_RESOURCES - the SYS module open failed because needed
          system resources are not available.

          VOS_STATUS_E_FAILURE - the SYS module open failed due to some
          unknown reason.

  \sa

  --------------------------------------------------------------------------*/
VOS_STATUS sysOpen( v_CONTEXT_t pVosContext );


/*----------------------------------------------------------------------------

  \brief sysMcStart() - start the system Main Controller thread.

  This function starts the SYS (Main Controller) module.  Starting this
  module triggers the CFG download to the 'legacy' MAC software.

  \param pVosContext - pointer to the VOS Context

  \param userCallback - this is a callback that is called when the SYS
         has completed the 'start' funciton.

  \param pUserData - pointer to some user data entity that is passed to
         the callback function as a parameter when invoked.

  \return VOS_STATUS_SUCCESS -

  \todo:  We have not 'status' on the callback.  How do we notify the
          callback that there is a failure ?

  \sa

  --------------------------------------------------------------------------*/
VOS_STATUS sysMcStart( v_CONTEXT_t pVosContext, sysResponseCback userCallback,
                       v_VOID_t *pUserData );


/*----------------------------------------------------------------------------

  \brief sysStop() - Stop the SYS module.

  This function stops the SYS module.

  \todo: What else do we need to do on sysStop()?

  \param pVosContext - pointer to the VOS Context

  \return VOS_STATUS_SUCCESS - the SYS module is stopped.

          VOS_STATUS_E_FAILURE - the SYS module open failed to stop.

  \sa

  --------------------------------------------------------------------------*/
VOS_STATUS sysStop( v_CONTEXT_t pVosContext );


/*----------------------------------------------------------------------------

  \brief sysClose() - Close the SYS module.

  This function closes the SYS module.  All resources allocated during
  sysOpen() are free'd and returned to the system.  The Sys module is unable
  to operate until opened again through a call to sysOpen().

  \param pVosContext - pointer to the VOS Context

  \return VOS_STATUS_SUCCESS - the SYS module is closed.

          VOS_STATUS_E_FAILURE - the SYS module open failed to close

  \sa sysOpen(), sysMcStart()

  --------------------------------------------------------------------------*/
VOS_STATUS sysClose( v_CONTEXT_t pVosContext );


/*----------------------------------------------------------------------------

  \brief sysMcThreadProbe() - Probe the SYS Main Controller thread

  This function is called during initialization to 'probe' the Main Controller
  thread.  Probing means a specific message is posted to the SYS module to
  assure the Main Controller thread is operating and processing messages
  correctly.

  Following the successful 'probe' of the Main Controller thread, the
  callback specified on this function is called to notify another entity
  that the Main Controller is operational.

  \param pVosContext - pointer to the VOS Context

  \param userCallback - this is a callback that is called when the SYS
         has completed probing the Main Controller thread.

  \param pUserData - pointer to some user data entity that is passed to
         the callback function as a parameter when invoked.

  \return VOS_STATUS_SUCCESS -
          \todo: how do we tell the callback there is a failure?

  \sa sysOpen(), sysMcStart()

  --------------------------------------------------------------------------*/
v_VOID_t sysMcThreadProbe( v_CONTEXT_t pVosContex, sysResponseCback userCallback,
                           v_VOID_t *pUserData );

/*----------------------------------------------------------------------------

  \brief sysTxThreadProbe() - Probe the Tx thread

  This function is called during initialization to 'probe' the Tx
  thread.  Probing means a specific message is posted to the SYS module to
  assure the Tx is operating and processing messages correctly.

  Following the successful 'probe' of the Tx, the callback specified
  on this function is called to notify another entity that the Tx thread
  is operational.

  \param pVosContext - pointer to the VOS Context

  \param userCallback - this is a callback that is called when the SYS
         has completed probing the Tx thread.

  \param pUserData - pointer to some user data entity that is passed to
         the callback function as a parameter when invoked.

  \return VOS_STATUS_SUCCESS -
          \todo: how do we tell the callback there is a failure?

  \sa sysOpen(), sysMcStart()

  --------------------------------------------------------------------------*/
v_VOID_t sysTxThreadProbe( v_CONTEXT_t pVosContex, sysResponseCback userCallback,
                           v_VOID_t *pUserData );

/*----------------------------------------------------------------------------

  \brief sysMcProcessMsg() - process SYS messages on the Main Controller thread

  This function processes SYS Messages on the Main Controller thread.
  SYS messages consist of all 'legacy' messages (messages bound for legacy
  modules like LIM, HAL, PE, etc.) as well as newly defined SYS message
  types.

  SYS messages are identified by their type (in the SYS_MESSAGES enum) as
  well as a 'cookie' that is in the reserved field of the message structure.
  This 'cookie' is introduced to prevent any message type/ID conflicts with
  the 'legacy' message types.

  Any module attempting to post a message to the SYS module must set the
  message type to one of the types in the SYS_MESSAGE enum *and* must also
  set the Reserved field in the message body to SYS_MSG_COOKIE.

  \param pVosContext - pointer to the VOS Context

  \param pMsg - pointer to the message to be processed.

  \return - VOS_STATUS_SUCCESS - the message was processed successfully.

            VOS_STATUS_E_BADMSG - a bad (unknown type) message was received
            and subsequently not processed.
  \sa

  --------------------------------------------------------------------------*/
VOS_STATUS sysMcProcessMsg( v_CONTEXT_t pVosContext, vos_msg_t* pMsg );

/*----------------------------------------------------------------------------

  \brief sysTxProcessMsg() - process SYS messages on the Tx thread

  This function processes SYS Messages on the Tx thread.
  SYS messages consist of all 'legacy' messages (messages bound for legacy
  modules like LIM, HAL, PE, etc.) as well as newly defined SYS message
  types.

  SYS messages are identified by their type (in the SYS_MESSAGES enum) as
  well as a 'cookie' that is in the reserved field of the message structure.
  This 'cookie' is introduced to prevent any message type/ID conflicts with
  the 'legacy' message types.

  Any module attempting to post a message to the SYS module must set the
  message type to one of the types in the SYS_MESSAGE enum *and* must also
  set the Reserved field in the message body to SYS_MSG_COOKIE.

  \param pVosContext - pointer to the VOS Context

  \param pMsg - pointer to the message to be processed.

  \return - VOS_STATUS_SUCCESS - the message was processed successfully.

            VOS_STATUS_E_BADMSG - a bad (unknown type) message was received
            and subsequently not processed.

  \sa

  --------------------------------------------------------------------------*/
VOS_STATUS sysTxProcessMsg( v_CONTEXT_t pVContext, vos_msg_t* pMsg );

/*----------------------------------------------------------------------------

  \brief sysTxProcessMsg() - process SYS messages on the Rx thread

  This function processes SYS Messages on the Rx thread.
  SYS messages consist of all 'legacy' messages (messages bound for legacy
  modules like LIM, HAL, PE, etc.) as well as newly defined SYS message
  types.

  SYS messages are identified by their type (in the SYS_MESSAGES enum) as
  well as a 'cookie' that is in the reserved field of the message structure.
  This 'cookie' is introduced to prevent any message type/ID conflicts with
  the 'legacy' message types.

  Any module attempting to post a message to the SYS module must set the
  message type to one of the types in the SYS_MESSAGE enum *and* must also
  set the Reserved field in the message body to SYS_MSG_COOKIE.

  \param pVosContext - pointer to the VOS Context

  \param pMsg - pointer to the message to be processed.

  \return - VOS_STATUS_SUCCESS - the message was processed successfully.

            VOS_STATUS_E_BADMSG - a bad (unknown type) message was received
            and subsequently not processed.

  \sa

  --------------------------------------------------------------------------*/
VOS_STATUS sysRxProcessMsg( v_CONTEXT_t pVContext, vos_msg_t* pMsg );

/*----------------------------------------------------------------------------

  \brief sysMcFreeMsg() - free a message queue'd to the Main Controller thread

  This fnction will free a SYS Message that is pending in the main controller
  thread queue.  These messages are free'd when the message queue needs to be
  purged, for example during a Reset of Shutdown of the system.

  \param pVosContext - pointer to the VOS Context

  \param pMsg - the message to be free'd

  \return Nothing.

  --------------------------------------------------------------------------*/
v_VOID_t sysMcFreeMsg( v_CONTEXT_t pVosContext, vos_msg_t* pMsg );

/*----------------------------------------------------------------------------

  \brief sysTxFreeMsg() - free a message queue'd to the Tx thread

  This fnction will free a SYS Message that is pending in the Tx
  thread queue.  These messages are free'd when the message queue needs to be
  purged, for example during a Reset of Shutdown of the system.

  \param pVosContext - pointer to the VOS Context

  \param pMsg - the message to be free'd

  \return Nothing.

  --------------------------------------------------------------------------*/
v_VOID_t sysTxFreeMsg( v_CONTEXT_t pVContext, vos_msg_t* pMsg );

/*----------------------------------------------------------------------------

  \brief wlan_sys_ftm() - FTM Cmd Response from halPhy

  This fnction is called by halPhy and carried the FTM command response.
  This message is handled by SYS thread and finally the message will be convyed to used space


  \param pttMsgBuffer - pointer to the pttMsgBuffer


  \return Nothing.

  --------------------------------------------------------------------------*/

void wlan_sys_ftm(void *pMsgPtr);
void wlan_sys_probe(void);


#endif  // WLAN_QCT_SYS_H__

