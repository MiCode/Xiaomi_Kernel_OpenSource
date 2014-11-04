#ifndef __ISP_PRIVATE_H_INCLUDED__
#define __ISP_PRIVATE_H_INCLUDED__

#include "isp_public.h"

#ifdef C_RUN
#include <string.h>		/* memcpy() */
#endif

#include "device_access.h"

#include "assert_support.h"

STORAGE_CLASS_ISP_C void isp_ctrl_store(
	const isp_ID_t		ID,
	const unsigned int	reg,
	const hrt_data		value)
{
assert(ID < N_ISP_ID);
assert(ISP_CTRL_BASE[ID] != (hrt_address)-1);
	device_store_uint32(ISP_CTRL_BASE[ID] + reg*sizeof(hrt_data), value);
return;
}

STORAGE_CLASS_ISP_C hrt_data isp_ctrl_load(
	const isp_ID_t		ID,
	const unsigned int	reg)
{
assert(ID < N_ISP_ID);
assert(ISP_CTRL_BASE[ID] != (hrt_address)-1);
return device_load_uint32(ISP_CTRL_BASE[ID] + reg*sizeof(hrt_data));
}

STORAGE_CLASS_ISP_C bool isp_ctrl_getbit(
	const isp_ID_t		ID,
	const unsigned int	reg,
	const unsigned int	bit)
{
	hrt_data val = isp_ctrl_load(ID, reg);
return (val & (1UL << bit)) != 0;
}

STORAGE_CLASS_ISP_C void isp_ctrl_setbit(
	const isp_ID_t		ID,
	const unsigned int	reg,
	const unsigned int	bit)
{
	hrt_data	data = isp_ctrl_load(ID, reg);
	isp_ctrl_store(ID, reg, (data | (1UL << bit)));
return;
}

STORAGE_CLASS_ISP_C void isp_ctrl_clearbit(
	const isp_ID_t		ID,
	const unsigned int	reg,
	const unsigned int	bit)
{
	hrt_data	data = isp_ctrl_load(ID, reg);
	isp_ctrl_store(ID, reg, (data & ~(1UL << bit)));
return;
}

STORAGE_CLASS_ISP_C void isp_dmem_store(
	const isp_ID_t		ID,
	unsigned int		addr,
	const void			*data,
	const size_t		size)
{
assert(ID < N_ISP_ID);
assert(ISP_DMEM_BASE[ID] != (hrt_address)-1);
#ifndef C_RUN
	device_store(ISP_DMEM_BASE[ID] + addr, data, size);
#else
	memcpy((void *)addr, data, size);
#endif
return;
}

STORAGE_CLASS_ISP_C void isp_dmem_load(
	const isp_ID_t		ID,
	const unsigned int	addr,
	void				*data,
	const size_t		size)
{
assert(ID < N_ISP_ID);
assert(ISP_DMEM_BASE[ID] != (hrt_address)-1);
#ifndef C_RUN
	device_load(ISP_DMEM_BASE[ID] + addr, data, size);
#else
	memcpy(data, (void *)addr, size);
#endif
return;
}

STORAGE_CLASS_ISP_C void isp_dmem_store_uint32(
	const isp_ID_t		ID,
	unsigned int		addr,
	const uint32_t		data)
{
assert(ID < N_ISP_ID);
assert(ISP_DMEM_BASE[ID] != (hrt_address)-1);
#ifndef C_RUN
	device_store_uint32(ISP_DMEM_BASE[ISP0_ID] + addr, data);
#else
	*(uint32_t *)addr = data;
#endif
return;
}

STORAGE_CLASS_ISP_C uint32_t isp_dmem_load_uint32(
	const isp_ID_t		ID,
	const unsigned int	addr)
{
assert(ID < N_ISP_ID);
assert(ISP_DMEM_BASE[ID] != (hrt_address)-1);
#ifndef C_RUN
	return device_load_uint32(ISP_DMEM_BASE[ISP0_ID] + addr);
#else
	return *(uint32_t *)addr;
#endif
}

STORAGE_CLASS_ISP_C uint32_t isp_2w_cat_1w(
	const uint16_t		x0,
	const uint16_t		x1)
{
	uint32_t out = ((uint32_t)(x1 & HIVE_ISP_VMEM_MASK) << ISP_VMEM_ELEMBITS)
		| (x0 & HIVE_ISP_VMEM_MASK);
return out;
}

#endif /* __ISP_PRIVATE_H_INCLUDED__ */
