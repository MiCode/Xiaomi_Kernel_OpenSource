/*                     COPYRIGHT 2016 STMicroelectronics
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
*     in the software, except the limited rights specified hereunder.
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



#ifndef STICK_API_H
#define STICK_API_H

#ifdef __cplusplus
extern "C" {
#endif


/* Includes ------------------------------------------------------------------*/
#ifndef RELEASE_BUILDING
#include <linux/kernel.h>
#include <linux/module.h>
#include "stick_types.h"

typedef void stick_Handler_t;

#else
#include "stick_core.h"
#endif // RELEASE_BUILDING

/* Exported Functions  ------------------------------------------------------------*/
/*!
 * \brief : Returns the minimum memory size required to store a stick_Handler_t
 * \result : Size of the structure in bytes
 */
uint32_t stick_get_handler_size(void);

/*!
 * \brief : Returns the STICK-A10x host library Middleware revision
 * \result : Library version
 */
uint32_t stick_get_lib_version(void);

/*!
 * \brief : Initializes STICK hardware and associated device handler
 * \param[in]	*pSTICK Pointer to target STICK handler
 * \result  STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_init(stick_Handler_t *pSTICK);

/*!
 * \brief : Authenticate STICK device
 * \param[in] 	*pSTICK Pointer to target STICK handler
 * \result STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_authenticate(stick_Handler_t * pSTICK);

/*!
 * \brief : Get STICK traceability information
 * \param[in] 	*pSTICK Pointer to target STICK handler
 * \param[out] 	*pTraceData pointer to Trace Data output structure
 * \result STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_get_traceability(stick_Handler_t *pSTICK,
										  stick_traceability_data_t * pTraceData);

/*!
 * \brief : Update one Zone of the STICK device
 * \param[in] pSTICK 		Pointer to target STICK handler
 * \param[in] protected 	Session protection (0 = no protection ; 1 = session protected)
 * \param[in] zone_index 	The zone index to be updated in target STICK device
 * \param[in] option 		Update option
 * \param[in] offset 		Update offset
 * \param[in] *data 		Pointer to new data buffer (data to be written in the zone)
 * \param[in] data_length 	Update length in byte
 * \result : STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_update_zone(stick_Handler_t * pSTICK,
                                    uint8_t isprotected,
                                    uint32_t zone_index,
                                    stick_update_option_t option,
                                    uint16_t offset,
                                    uint8_t *data ,
                                    uint32_t data_length);

/*!
 * \brief : Read one zone of the STICK device
 * \param[in]  *pSTICK 			Pointer to target STICK handler
 * \param[in]  	protected 		Session protection indicator (0 = no protection ; 1 = session protected)
 * \param[in]  	zone_index 		Zone Index
 * \param[in]  	option Read 	Option
 * \param[in]  	read_offset 	Read offset
 * \param[out] 	*pReadBuffer 	Pointer to read buffer
 * \param[in]  	read_length 	Read length in byte
 * \param[out] 	*rsp_length 	Pointer to responce length variable (response lentgh can be different from
 * \result 		STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_read_zone(stick_Handler_t * pSTICK,
                                    uint8_t isprotected,
                                    uint32_t zone_index,
                                    stick_read_option_t  option,
                                    uint16_t read_offset,
                                    uint8_t *pReadBuffer,
                                    uint16_t read_length,
                                    uint16_t *rsp_length);

/*!
 * \brief : 	Decrement counter stored in specific zone of the STICK device
 * \param[in] 	*pSTICK 			Pointer to target STICK handler
 * \param[in] 	protected 			Protection (0 = no protection ; 1 = session protected)
 * \param[in] 	zone_index 			Zone Index
 * \param[in] 	option 				Decrement option
 * \param[in] 	amount 				Amount to be decrement from zone counter
 * \param[in] 	offset 				Offset used in case of associated data update
 * \param[in] 	*data 				Pointer to the new associated data buffer (data to be written along with counter decrement in the zone)
 * \param[in] 	data_length 		New associated data buffer length in byte
 * \param[out] 	*new_counter_value 	Pointer to counter read buffer
 * \result : STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_decrement_counter(stick_Handler_t *pSTICK,
                                          uint8_t isprotected,
                                          uint8_t zone_index,
                                          stick_decrement_option_t option,
                                          uint32_t amount,
                                          uint16_t offset,
                                          uint8_t *data,
                                          uint8_t  data_length,
                                          uint32_t *new_counter_value);

/*!
 * \brief : 	Open a secure session with the target STICK device
 * \param[in] 	*pSTICK 	Pointer to target STICK handler
 * \result : 	STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_open_session(stick_Handler_t * pSTICK);

/*!
 * \brief : 	Close ongoing secure session with the target STICK device
 * \param[in] 	*pSTICK 	Pointer to target STICK handler
 * \result : 	STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_close_session(stick_Handler_t * pSTICK);

/*!
 * \brief :		Echo message with a STICK device
 * \param[in] 	*pSTICK 		Pointer to target STICK handler
 * \param[in] 	*message 		Pointer to message buffer
 * \param[out]	echoed_message	Pointer to echoed message buffer
 * \param[in] 	length			Length of the message in byte
 * \result 		STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_echo(stick_Handler_t *pSTICK,
                              uint8_t *message,
                              uint8_t* echoed_message,
                              uint8_t length);

/*!
 * \brief : 	Put the target STICK device in Hibernate mode
 * \param[in] 	*pSTICK Pointer to target STICK handler
 * \result : 	STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_hibernate(stick_Handler_t *pSTICK);

/*!
 * \brief : 	Wake-up a STICK device from Hibernate mode
 * \param[in] 	*pSTICK Pointer to target STICK handler
 * \result : 	STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_wakeup(stick_Handler_t *pSTICK);

/*!
 * \brief : 	Remove STICK information from Host table
 * \param[in] 	*pSTICK 	Pointer to target STICK handler
 * \result : 	STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_remove_information(stick_Handler_t *pSTICK);


/* -------------------------- STICK OS V2.x APIs -------------------------- */

/*!
 * \brief : 	Put Data in STICK device
 * \param[in]  	*pSTICK 		Pointer to target STICK handler
 * \param[in]  	tag				Tag of the configuration Data
 * \param[in]  	*pvalue			Pointer to Data to be put
 * \param[in]  	value_length	Length of the Data
 * \result 	 	STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_put_data(stick_Handler_t *pSTICK,
								  uint8_t tag,
								  uint8_t *pvalue,
								  uint16_t value_length);

/*!
 * \brief : 	Get Data from target STICK device
 * \param[in]	*pSTICK 		Pointer to target STICK handler
 * \param[in]	tag 			Tag of the Data
 * \param[in]	additional_data	Additional Data associated to Tag value (see STICK user-manual)
 * \param[out]  *pDataBuffer    Pointer to the Data to be read
 * \param[out]	*DataLength	    Length the Data returned by the STICK
 * \result		STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_get_data(stick_Handler_t *pSTICK,
								  uint8_t tag,
								  uint8_t additional_data,
								  uint8_t *pDataBuffer,
								  uint16_t *DataLength);
/*!
 * \brief : 	Regenerate target STICK device
 * \param[in]	*pSTICK 	Pointer to target STICK handler
 * \param[in]	*pPassword 	Pointer to the regenerate password
 * \result : 	STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_regenerate(stick_Handler_t *pSTICK,
		                            uint8_t *pPassword);

/*!
 * \brief : 	kill target STICK device (must be sent under active session)
 * \param[in]	*pSTICK 	Pointer to target STICK handler
 * \result		STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_kill(stick_Handler_t *pSTICK);


/*!
 * \brief : 	Authenticate target STICK device using EDDSA
 * \param[in]	*pSTICK 				Pointer to target STICK handler
 * \param[in]	certificate_zone_index	Index of the zone containing public key certificate to be used
 * \result		STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_eddsa_authenticate(stick_Handler_t *pSTICK,
											uint8_t certificate_zone_index);

/*!
 * \brief : 	Authenticate Traceability information from target STICK device using EDDSA
 * \param[in] 	*pSTICK 				Pointer to target STICK handler
 * \param[in]	certificate_zone_index	Index of the zone containing public key certificate to be used
 * \param[out]	*pTraceData      		Pointer to traceability data
 * \result 		STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_eddsa_authenticate_traceability(stick_Handler_t *pSTICK,
														 uint8_t certificate_zone_index,
														 stick_traceability_data_t * pTraceData);

/*!
 * \brief : 	Authenticate zone content from target STICK device using EDDSA
 * \param[in] 	*pSTICK 				Pointer to target STICK handler
 * \param[in]	certificate_zone_index	Index of the zone containing public key certificate to be used
 * \param[in] 	data_zone_index			Index of the data zone to be signed
 * \param[in] 	data_zone_length		length of the data zone to be signed
 * \param[in] 	*pdatabuffer
 * \result 		STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_eddsa_authenticate_zone(stick_Handler_t *pSTICK,
                                                 uint8_t certificate_zone_index,
												 uint8_t data_zone_index,
												 uint16_t data_zone_length,
												 uint8_t *pdatabuffer);
/*!
 * \brief : 	Open a session with target STICK device using the sigma key
 * \param[in] 	*pSTICK Pointer to target STICK handler
 * \result  	SICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_open_sigma_key_session(stick_Handler_t *pSTICK);

/*!
 * \brief : 	Reset Zone AC (must be executed under Sigma key session)
 * \param[in] 	*pSTICK Pointer to target STICK handler
 * \result  	SICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_reset_zone_ac(stick_Handler_t * pSTICK, uint8_t zone_index , uint8_t option);


/** @}*/

/*!
 * \brief : Reset
 * \param[in] : pSTICK pointer to STICK handler
 * \result : STICK_OK on success ; stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_reset(stick_Handler_t * pSTICK);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* STICK_API_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
