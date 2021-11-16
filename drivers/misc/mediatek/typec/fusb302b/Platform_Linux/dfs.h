/**
 *******************************************************************************
 * @file        dfs.h
 * @author      ON Semiconductor USB-PD Firmware Team
 * @brief       Defines a set of DebugFS accessors
 *
 * Software License Agreement:
 *
 * The software supplied herewith by ON Semiconductor (the Company)
 * is supplied to you, the Company's customer, for exclusive use with its
 * USB Type C / USB PD products.  The software is owned by the Company and/or
 * its supplier, and is protected under applicable copyright laws.
 * All rights are reserved. Any use in violation of the foregoing restrictions
 * may subject the user to criminal sanctions under applicable laws, as well
 * as to civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN AS IS CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *******************************************************************************
 */

#ifndef _FSC_DFS_H_
#define _FSC_DFS_H_

#ifdef FSC_DEBUG

#include "FSCTypes.h"

/*******************************************************************************
* Function:        fusb_DFS_Init
* Input:           none
* Return:          0 on success, error code otherwise
* Description:     Initializes methods for using DebugFS.
*******************************************************************************/
FSC_S32 fusb_DFS_Init(void);

FSC_S32 fusb_DFS_Cleanup(void);

#endif /* FSC_DEBUG */

#endif /* _FUSB_DFS_H_ */

