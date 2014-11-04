#ifndef __GPIO_PUBLIC_H_INCLUDED__
#define __GPIO_PUBLIC_H_INCLUDED__

#include "system_types.h"

/*! Write to a control register of GPIO[ID]

 \param	ID[in]				GPIO identifier
 \param	reg_addr[in]		register byte address
 \param value[in]			The data to be written

 \return none, GPIO[ID].ctrl[reg] = value
 */
STORAGE_CLASS_GPIO_H void gpio_reg_store(
	const gpio_ID_t	ID,
	const unsigned int		reg_addr,
	const hrt_data			value);

/*! Read from a control register of GPIO[ID]
 
 \param	ID[in]				GPIO identifier
 \param	reg_addr[in]		register byte address
 \param value[in]			The data to be written

 \return GPIO[ID].ctrl[reg]
 */
STORAGE_CLASS_GPIO_H hrt_data gpio_reg_load(
	const gpio_ID_t	ID,
	const unsigned int		reg_addr);

#endif /* __GPIO_PUBLIC_H_INCLUDED__ */
