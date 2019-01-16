/**
 * Copyright (C) 2009 Koninklijke Philips Electronics N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Koninklijke Philips Electronics N.V. and is confidential in
 * nature. Under no circumstances is this software to be  exposed to or placed
 * under an Open Source License of any type without the expressed written
 * permission of Koninklijke Philips Electronics N.V.
 *
 * \file          tmbslTDA9989_State.h
 *
 * \version       $Revision: 2 $
 *
 *
*/

#ifndef TMBSLTDA9989_STATE_L_H
#define TMBSLTDA9989_STATE_L_H

/*============================================================================*/
/*                       INCLUDE FILES                                        */
/*============================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/*                       MACRO DEFINITIONS                                    */
/*============================================================================*/


/*============================================================================*/
/*                       ENUM OR TYPE DEFINITIONS                             */
/*============================================================================*/



/*============================================================================*/
/*                       EXTERN DATA DEFINITION                               */
/*============================================================================*/


/*============================================================================*/
/*                       EXTERN FUNCTION PROTOTYPES                           */
/*============================================================================*/

	tmErrorCode_t setState(tmHdmiTxobject_t *pDis, tmbslTDA9989Event_t event);

#ifdef __cplusplus
}
#endif
#endif				/* TMBSLTDA9989_STATE_L_H */
/*============================================================================*//*                            END OF FILE                                     *//*============================================================================*/
