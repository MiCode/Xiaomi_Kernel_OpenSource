#ifndef __SP_PRIVATE_H_INCLUDED__
#define __SP_PRIVATE_H_INCLUDED__

#include "sp_public.h"

#include "device_access.h"
#ifdef C_RUN
#include <string.h>	/* memcpy() */
#endif

#include "assert_support.h"

STORAGE_CLASS_SP_C void sp_ctrl_store(
	const sp_ID_t		ID,
	const unsigned int	reg,
	const hrt_data		value)
{
assert(ID < N_SP_ID);
assert(SP_CTRL_BASE[ID] != (hrt_address)-1);
	device_store_uint32(SP_CTRL_BASE[ID] + reg*sizeof(hrt_data), value);
return;
}

STORAGE_CLASS_SP_C hrt_data sp_ctrl_load(
	const sp_ID_t		ID,
	const unsigned int	reg)
{
assert(ID < N_SP_ID);
assert(SP_CTRL_BASE[ID] != (hrt_address)-1);
return device_load_uint32(SP_CTRL_BASE[ID] + reg*sizeof(hrt_data));
}

STORAGE_CLASS_SP_C bool sp_ctrl_getbit(
	const sp_ID_t		ID,
	const unsigned int	reg,
	const unsigned int	bit)
{
	hrt_data val = sp_ctrl_load(ID, reg);
return (val & (1UL << bit)) != 0;
}

STORAGE_CLASS_SP_C void sp_ctrl_setbit(
	const sp_ID_t		ID,
	const unsigned int	reg,
	const unsigned int	bit)
{
	hrt_data	data = sp_ctrl_load(ID, reg);
	sp_ctrl_store(ID, reg, (data | (1UL << bit)));
return;
}

STORAGE_CLASS_SP_C void sp_ctrl_clearbit(
	const sp_ID_t		ID,
	const unsigned int	reg,
	const unsigned int	bit)
{
	hrt_data	data = sp_ctrl_load(ID, reg);
	sp_ctrl_store(ID, reg, (data & ~(1UL << bit)));
return;
}

STORAGE_CLASS_SP_C void sp_dmem_store(
	const sp_ID_t		ID,
	unsigned int		addr,
	const void			*data,
	const size_t		size)
{
assert(ID < N_SP_ID);
assert(SP_DMEM_BASE[ID] != (hrt_address)-1);
#ifndef C_RUN
	device_store(SP_DMEM_BASE[ID] + addr, data, size);
#else
	memcpy((void *)addr, data, size);
#endif
return;
}

STORAGE_CLASS_SP_C void sp_dmem_load(
	const sp_ID_t		ID,
	const unsigned int	addr,
	void				*data,
	const size_t		size)
{
assert(ID < N_SP_ID);
assert(SP_DMEM_BASE[ID] != (hrt_address)-1);
#ifndef C_RUN
	device_load(SP_DMEM_BASE[ID] + addr, data, size);
#else
	memcpy(data, (void *)addr, size);
#endif
return;
}

STORAGE_CLASS_SP_C void sp_dmem_store_uint8(
	const sp_ID_t		ID,
	unsigned int		addr,
	const uint8_t		data)
{
assert(ID < N_SP_ID);
assert(SP_DMEM_BASE[ID] != (hrt_address)-1);
#ifndef C_RUN
	device_store_uint8(SP_DMEM_BASE[SP0_ID] + addr, data);
#else
	*(uint8_t *)addr = data;
#endif
return;
}

STORAGE_CLASS_SP_C void sp_dmem_store_uint16(
	const sp_ID_t		ID,
	unsigned int		addr,
	const uint16_t		data)
{
assert(ID < N_SP_ID);
assert(SP_DMEM_BASE[ID] != (hrt_address)-1);
#ifndef C_RUN
	device_store_uint16(SP_DMEM_BASE[SP0_ID] + addr, data);
#else
	*(uint16_t *)addr = data;
#endif
return;
}

STORAGE_CLASS_SP_C void sp_dmem_store_uint32(
	const sp_ID_t		ID,
	unsigned int		addr,
	const uint32_t		data)
{
assert(ID < N_SP_ID);
assert(SP_DMEM_BASE[ID] != (hrt_address)-1);
#ifndef C_RUN
	device_store_uint32(SP_DMEM_BASE[SP0_ID] + addr, data);
#else
	*(uint32_t *)addr = data;
#endif
return;
}

STORAGE_CLASS_SP_C uint8_t sp_dmem_load_uint8(
	const sp_ID_t		ID,
	const unsigned int	addr)
{
assert(ID < N_SP_ID);
assert(SP_DMEM_BASE[ID] != (hrt_address)-1);
#ifndef C_RUN
	return device_load_uint8(SP_DMEM_BASE[SP0_ID] + addr);
#else
	return *(uint8_t *)addr;
#endif
}

STORAGE_CLASS_SP_C uint16_t sp_dmem_load_uint16(
	const sp_ID_t		ID,
	const unsigned int	addr)
{
assert(ID < N_SP_ID);
assert(SP_DMEM_BASE[ID] != (hrt_address)-1);
#ifndef C_RUN
	return device_load_uint16(SP_DMEM_BASE[SP0_ID] + addr);
#else
	return *(uint16_t *)addr;
#endif
}

STORAGE_CLASS_SP_C uint32_t sp_dmem_load_uint32(
	const sp_ID_t		ID,
	const unsigned int	addr)
{
assert(ID < N_SP_ID);
assert(SP_DMEM_BASE[ID] != (hrt_address)-1);
#ifndef C_RUN
	return device_load_uint32(SP_DMEM_BASE[SP0_ID] + addr);
#else
	return *(uint32_t *)addr;
#endif
}


#endif /* __SP_PRIVATE_H_INCLUDED__ */
