  /*!
  * <h2><center>&copy; COPYRIGHT 2020 STMicroelectronics</center></h2>
  *
  * STSAFE DRIVER SOFTWARE LICENSE AGREEMENT (SLA0088)
  *
  * BY INSTALLING, COPYING, DOWNLOADING, ACCESSING OR OTHERWISE USING THIS SOFTWARE
  * OR ANY PART THEREOF (AND THE RELATED DOCUMENTATION) FROM STMICROELECTRONICS
  * INTERNATIONAL N.V, SWISS BRANCH AND/OR ITS AFFILIATED COMPANIES (STMICROELECTRONICS),
  * THE RECIPIENT, ON BEHALF OF HIMSELF OR HERSELF, OR ON BEHALF OF ANY ENTITY BY WHICH
  * SUCH RECIPIENT IS EMPLOYED AND/OR ENGAGED AGREES TO BE BOUND BY THIS SOFTWARE LICENSE
  * AGREEMENT.
  *
  * Under STMicroelectronics’ intellectual property rights, the redistribution,
  * reproduction and use in source and binary forms of the software or any part thereof,
  * with or without modification, are permitted provided that the following conditions
  * are met:
  * 1.  Redistribution of source code (modified or not) must retain any copyright notice,
  *     this list of conditions and the disclaimer set forth below as items 10 and 11.
  * 2.  Redistributions in binary form, except as embedded into a microcontroller or
  *     microprocessor device or a software update for such device, must reproduce any
  *     copyright notice provided with the binary code, this list of conditions, and the
  *     disclaimer set forth below as items 10 and 11, in documentation and/or other
  *     materials provided with the distribution.
  * 3.  Neither the name of STMicroelectronics nor the names of other contributors to this
  *     software may be used to endorse or promote products derived from this software or
  *     part thereof without specific written permission.
  * 4.  This software or any part thereof, including modifications and/or derivative works
  *     of this software, must be used and execute solely and exclusively in combination
  *     with a secure microcontroller device from STSAFE family manufactured by or for
  *     STMicroelectronics.
  * 5.  No use, reproduction or redistribution of this software partially or totally may be
  *     done in any manner that would subject this software to any Open Source Terms.
  *     “Open Source Terms” shall mean any open source license which requires as part of
  *     distribution of software that the source code of such software is distributed
  *     therewith or otherwise made available, or open source license that substantially
  *     complies with the Open Source definition specified at www.opensource.org and any
  *     other comparable open source license such as for example GNU General Public
  *     License(GPL), Eclipse Public License (EPL), Apache Software License, BSD license
  *     or MIT license.
  * 6.  STMicroelectronics has no obligation to provide any maintenance, support or
  *     updates for the software.
  * 7.  The software is and will remain the exclusive property of STMicroelectronics and
  *     its licensors. The recipient will not take any action that jeopardizes
  *     STMicroelectronics and its licensors' proprietary rights or acquire any rights
  *     in the software, except the limited rights specified here under.
  * 8.  The recipient shall comply with all applicable laws and regulations affecting the
  *     use of the software or any part thereof including any applicable export control
  *     law or regulation.
  * 9.  Redistribution and use of this software or any part thereof other than as  permitted
  *     under this license is void and will automatically terminate your rights under this
  *     license.
  * 10. THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" AND ANY
  *     EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
  *     OF THIRD PARTY INTELLECTUAL PROPERTY RIGHTS, WHICH ARE DISCLAIMED TO THE FULLEST
  *     EXTENT PERMITTED BY LAW. IN NO EVENT SHALL STMICROELECTRONICS OR CONTRIBUTORS BE
  *     LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  *     DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  *     THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  *     NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
  *     ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  * 11. EXCEPT AS EXPRESSLY PERMITTED HEREUNDER, NO LICENSE OR OTHER RIGHTS, WHETHER EXPRESS
  *     OR IMPLIED, ARE GRANTED UNDER ANY PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS OF
  *     STMICROELECTRONICS OR ANY THIRD PARTY.
  ******************************************************************************
  */
#ifndef STICK_TYPES_H
#define STICK_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/kernel.h>
#include <linux/module.h>

/* Exported types ------------------------------------------------------------*/
/** \addtogroup APIs_return_error_codes
 *  @{
 */

/**
 * \enum stick_ReturnCode_t
 * \brief STICK response codes enumeration
 */
typedef enum
{
	STICK_BUS_ERR		       				= -2,      /*!< Error detected on the STICK HW Bus */
	STICK_INVALID_PARAMETER,                           /*!< Wrong function/command parameter */
	STICK_OK,                                          /*!< Function call returns successfully */
	STICK_INCONSISTENT_COMMAND_DATA,     			   /*!< STICK returns command data not consistent (please refer to STICK UM) */
	STICK_ACCESS_CONDITION_NOT_SATISFIED,			   /*!< STICK returns slot access condition not satisfied (please refer to STICK UM) */
	STICK_BOUNDARY_REACHED,                            /*!< STICK returns boundary reached (please refer to STICK UM) */
	STICK_ENTRY_NOT_FOUND,               			   /*!< STICK returns entry not found  (please refer to STICK UM) */
	STICK_WRONG_ZONE_TYPE,               			   /*!< STICK_WRONG_ZONE_TYPE */
	STICK_INVALID_KEY,                                 /*!< STICK_INVALID_KEY */
	STICK_COMMUNICATION_ERROR,                         /*!< STICK_COMMUNICATION_ERROR */
	STICK_COMMAND_NOT_AUTHORIZED,                      /*!< STICK_COMMAND_NOT_AUTHORIZED */
	STICK_NVM_WRITE_ERROR,                             /*!< STICK_NVM_WRITE_ERROR */
	STICK_AUTHENTICATION_ERROR,                        /*!< STICK_NOT_GENUINE */
	STICK_HANDLER_NOT_INITIALISED,                     /*!< STICK is not initialized please run stick_init function first */
	STICK_SESSION_ERROR,                               /*!< Session between Host and STICK not established */
	STICK_PROT_OK = 0x80                               /*!< Protected command/response successfull */
} stick_ReturnCode_t;

/** @}*/

/*!
 * \struct stick_traceability_data_t
 * \brief : STICK Traceability data structure (output from Get Traceability API)
 */
typedef struct {
	uint8_t product_type[8]; 		  //<! product type (8-bytes)
	uint8_t cpsn[7];				  //<! product cpsn (7-bytes)
	uint16_t remaining_signatures;    //<! Number of remaining signature (2-bytes)
	uint16_t remaining_ECDHE;         //<! Number of remaining ecdhe (2-bytes)
	uint16_t total_pairing_slots;     //<! Number of total pairing slot
	uint16_t remaining_pairing_slots; //<! Number of remaining pairing slot
}stick_traceability_data_t;

/*!
 * \brief : STICK Read option structure
 */
typedef struct {
		uint8_t new_read_ac:4;
		uint8_t new_read_ac_change_right:1;
		uint8_t change_ac_indicator:1;
		uint8_t filler:2;
}stick_read_option_t;

/*!
 * \brief :  STICK Update option structure
 */
typedef struct {
		uint8_t new_update_ac:4;
		uint8_t new_update_ac_change_right:1;
		uint8_t change_ac_indicator:1;
		uint8_t filler:1;
		uint8_t atomicity:1;
}stick_update_option_t;

/*!
 * \brief :  STICK decrement option structure
 */
typedef struct {
		uint8_t new_update_ac:4;
		uint8_t new_update_ac_change_right:1;
		uint8_t change_ac_indicator:1;
		uint8_t filler:2;
}stick_decrement_option_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* STICK_TYPES_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
