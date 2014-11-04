#ifndef __MMU_PRIVATE_H_INCLUDED__
#define __MMU_PRIVATE_H_INCLUDED__

#include "mmu_public.h"

#include "device_access.h"

#include "assert_support.h"

STORAGE_CLASS_MMU_C void mmu_reg_store(
	const mmu_ID_t		ID,
	const unsigned int	reg,
	const hrt_data		value)
{
assert(ID < N_MMU_ID);
assert(MMU_BASE[ID] != (hrt_address)-1);
	device_store_uint32(MMU_BASE[ID] + reg*sizeof(hrt_data), value);
return;
}

STORAGE_CLASS_MMU_C hrt_data mmu_reg_load(
	const mmu_ID_t		ID,
	const unsigned int	reg)
{
assert(ID < N_MMU_ID);
assert(MMU_BASE[ID] != (hrt_address)-1);
return device_load_uint32(MMU_BASE[ID] + reg*sizeof(hrt_data));
}

#endif /* __MMU_PRIVATE_H_INCLUDED__ */
