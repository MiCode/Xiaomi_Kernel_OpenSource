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


#if!defined( __LIM_SESSION_UTILS_H )
#define __LIM_SESSION_UTILS_H

/*
* Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
* All Rights Reserved.
* Qualcomm Atheros Confidential and Proprietary.
*/

/**=========================================================================
  
  \file  limSessionUtils.h
  
  \brief prototype for lim Session Utility related APIs

  \author Sunit Bhatia
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/


/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/



/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/


/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------
  
  \brief peGetVhtCapable() - Returns the Vht capable from a valid session.
 
  This function iterates the session Table and returns the VHT capable from first valid session
  if no sessions are valid/present  it returns FALSE

  \param pMac - pointer to global adapter context
  \return     - channel to scan from valid session else zero.

  \sa

 --------------------------------------------------------------------------*/
tANI_U8 peGetVhtCapable(tpAniSirGlobal pMac);


/*--------------------------------------------------------------------------
  \brief peValidateJoinReq() - validates the Join request .

  This function is called to validate the Join Request for a BT-AMP station. If start BSS session is present
  this function returns TRUE else returns FALSE.

  \param pMac  - pointer to global adapter context
  \return      - return TRUE if start BSS session is present else return FALSE.

  \sa
  --------------------------------------------------------------------------*/
tANI_U8 peValidateBtJoinRequest(tpAniSirGlobal pMac);

/* --------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------
  \brief peGetValidPowerSaveSession() - Fetches the valid session for powersave .

  This function is called to check the valid session for power save, if more than one session is active , this function 
  it returns NULL.
  if there is only one valid "infrastructure" session present in "linkestablished" state this function returns sessionentry.
  For all other cases it returns NULL.
    
  \param pMac                   - pointer to global adapter context
  \return                            - return session is address if valid session is  present else return NULL.
  
  \sa
  --------------------------------------------------------------------------*/


tpPESession peGetValidPowerSaveSession(tpAniSirGlobal pMac);

/* --------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------
  \brief peIsAnySessionActive() - checks for the active session presence .

  This function returns TRUE if atleast one valid session is present else it returns FALSE
      
  \param pMac                   - pointer to global adapter context
  \return                            - return TRUE if atleast one session is active else return FALSE.
  
  \sa
  --------------------------------------------------------------------------*/

tANI_U8 peIsAnySessionActive(tpAniSirGlobal pMac);

/* --------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------
  \brief pePrintActiveSession() - print all the active pesession present .

  This function print all the active pesession present

  \param pMac                   - pointer to global adapter context

  \sa
  --------------------------------------------------------------------------*/

void pePrintActiveSession(tpAniSirGlobal pMac);

/* --------------------------------------------------------------------------*/



/*--------------------------------------------------------------------------
  \brief isLimSessionOffChannel() - Determines if the session is 
                                        off channel.

  This function returns TRUE if the session Id passed needs to be on a different
  channel than atleast one session already active.
    
  \param pMac                   - pointer to global adapter context
  \param sessionId              - session ID of the session to be verified.  
  
  \return tANI_U8               - Boolean value for off-channel operation.
  
  \sa
  --------------------------------------------------------------------------*/
tANI_U8
isLimSessionOffChannel(tpAniSirGlobal pMac, tANI_U8 sessionId);
/* --------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
  \brief peGetActiveSessionChannel() - Gets the first valid sessions primary and secondary
                                        channel. If not found returns invalid channel ID (=0)
  \param pMac              - pointer to global adapter context
  \param resumeChannel     - Primary channel of the first valid session. This is an output argument.
  \return resumePhyCbState - Secondary channel of the first valid session. This is an output argument.
--------------------------------------------------------------------------*/
void
peGetActiveSessionChannel(tpAniSirGlobal pMac, tANI_U8* resumeChannel, ePhyChanBondState* resumePhyCbState);

/*--------------------------------------------------------------------------
  \brief limIsChanSwitchRunning() - Check if channel switch is running on any  
                                    valid session.

  \param pMac                   - pointer to global adapter context
  
  \return tANI_U8               - 1 - if chann switching running.
                                  0 - if chann switching is not running. 
  
  \sa
  --------------------------------------------------------------------------*/
tANI_U8
limIsChanSwitchRunning (tpAniSirGlobal pMac);

/*--------------------------------------------------------------------------
  \brief limIsInQuietDuration() - Check if channel quieting is running on any  
                                    valid session.

  \param pMac                   - pointer to global adapter context
  
  \return tANI_U8               - 1 - if chann quiet running.
                                  0 - if chann quiet is not running. 
  
  \sa
  --------------------------------------------------------------------------*/
tANI_U8
limIsInQuietDuration (tpAniSirGlobal pMac);

/*--------------------------------------------------------------------------
  \brief limIsQuietBegin() - Check if channel quieting is begining on any  
                                    valid session.

  \param pMac                   - pointer to global adapter context
  
  \return tANI_U8               - 1 - if chann quiet running.
                                  0 - if chann quiet is not running. 
  
  \sa
  --------------------------------------------------------------------------*/
tANI_U8
limIsQuietBegin (tpAniSirGlobal pMac);
/*--------------------------------------------------------------------------
  \brief limIsInMCC() - Check if Device is in MCC.

  \param pMac                   - pointer to global adapter context
  
  \return tANI_U8               - TRUE - if in MCC.
                                  FALSE - NOT in MCC. 
  
  \sa
  --------------------------------------------------------------------------*/
tANI_U8
limIsInMCC (tpAniSirGlobal pMac);
/*--------------------------------------------------------------------------
  \brief peGetCurrentSTAsCount() - Returns total stations associated on 
                                      all session.

  \param pMac                   - pointer to global adapter context
  \return                       - Number of station active on all sessions.
  
  \sa
  --------------------------------------------------------------------------*/
tANI_U8
peGetCurrentSTAsCount(tpAniSirGlobal pMac);

#ifdef FEATURE_WLAN_LFR
/*--------------------------------------------------------------------------
  \brief limIsFastRoamEnabled() - To check Fast roaming is enabled or not

  \param pMac                   - pointer to global adapter context
  \param sessionId              - session id
  \return                       - TRUE or FALSE

  \sa
  --------------------------------------------------------------------------*/
tANI_U8
limIsFastRoamEnabled(tpAniSirGlobal pMac, tANI_U8 sessionId);
#endif


#endif //#if !defined( __LIM_SESSION_UTILS_H )

