/*******************************************************************************
 * @file     HostComm.h
 * @author   USB PD Firmware Team
 *
 * Copyright 2018 ON Semiconductor. All rights reserved.
 *
 * This software and/or documentation is licensed by ON Semiconductor under
 * limited terms and conditions. The terms and conditions pertaining to the
 * software and/or documentation are available at
 * http://www.onsemi.com/site/pdf/ONSEMI_T&C.pdf
 * ("ON Semiconductor Standard Terms and Conditions of Sale, Section 8 Software").
 *
 * DO NOT USE THIS SOFTWARE AND/OR DOCUMENTATION UNLESS YOU HAVE CAREFULLY
 * READ AND YOU AGREE TO THE LIMITED TERMS AND CONDITIONS. BY USING THIS
 * SOFTWARE AND/OR DOCUMENTATION, YOU AGREE TO THE LIMITED TERMS AND CONDITIONS.
 ******************************************************************************/
#ifndef _HOSTCOMM_H
#define _HOSTCOMM_H

#include "platform.h"
#include "hcmd.h"
#include "Port.h"

#ifdef FSC_DEBUG
/**
 * @brief Intialize the host com
 * @param[in] port starting address of the port structures
 * @param[in] num number of ports defined
 */
void HCom_Init(struct Port *port, FSC_U8 num);

/**
 * @brief Function to process the hostCom messages. Called when the input buffer
 * has a valid message.
 */
void HCom_Process(void);

/**
 * @brief Returns pointer to input buffer of length HCOM_MSG_LENGTH
 */
FSC_U8* HCom_InBuf(void);

/**
 * @brief Returns pointer to output buffer of length HCOM_MSG_LENGTH
 */
FSC_U8* HCom_OutBuf(void);

#endif /* FSC_DEBUG */

#endif	/* HOSTCOMM_H */
