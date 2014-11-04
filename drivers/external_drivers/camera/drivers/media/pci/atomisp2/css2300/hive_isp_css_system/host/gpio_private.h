#ifndef __GPIO_PRIVATE_H_INCLUDED__
#define __GPIO_PRIVATE_H_INCLUDED__

#include "gpio_public.h"

#include "device_access.h"

#include "assert_support.h"

STORAGE_CLASS_GPIO_C void gpio_reg_store(
	const gpio_ID_t	ID,
	const unsigned int		reg,
	const hrt_data			value)
{
OP___assert(ID < N_GPIO_ID);
OP___assert(GPIO_BASE[ID] != (hrt_address)-1);
	device_store_uint32(GPIO_BASE[ID] + reg*sizeof(hrt_data), value);
return;
}

STORAGE_CLASS_GPIO_C hrt_data gpio_reg_load(
	const gpio_ID_t	ID,
	const unsigned int		reg)
{
OP___assert(ID < N_GPIO_ID);
OP___assert(GPIO_BASE[ID] != (hrt_address)-1);
return device_load_uint32(GPIO_BASE[ID] + reg*sizeof(hrt_data));
}

#endif /* __GPIO_PRIVATE_H_INCLUDED__ */
