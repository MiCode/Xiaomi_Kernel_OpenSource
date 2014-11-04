/*
* Support for Medfield PNW Camera Imaging ISP subsystem.
*
* Copyright (c) 2010 Intel Corporation. All Rights Reserved.
*
* Copyright (c) 2010 Silicon Hive www.siliconhive.com.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version
* 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
* 02110-1301, USA.
*
*/

#include "sh_css_sp_start.h"
#include "sh_css_sp.h"
#include "sh_css_firmware.h"

#define __INLINE_SP__
#include "sp.h"

#include "mmu_device.h"

#include "memory_access.h"

#include "assert_support.h"

static bool invalidate_mmu;

void
sh_css_sp_invalidate_mmu(void)
{
	invalidate_mmu = true;
}

void sh_css_sp_start(
	unsigned int start_address)
{
assert(sizeof(unsigned int) <= sizeof(hrt_data));

	if (invalidate_mmu) {
		mmu_invalidate_cache(MMU0_ID);
		invalidate_mmu = false;
	}
	/* set the start address */
	sp_ctrl_store(SP0_ID, SP_START_ADDR_REG, (hrt_data)start_address);
	sp_ctrl_setbit(SP0_ID, SP_SC_REG, SP_RUN_BIT);
	sp_ctrl_setbit(SP0_ID, SP_SC_REG, SP_START_BIT);
return;
}


hrt_vaddress sh_css_sp_load_program(
	const struct sh_css_fw_info *fw,
	const char *sp_prog,
	hrt_vaddress code_addr)
{
	if (code_addr == mmgr_NULL) {
		/* store code (text section) to DDR */
		code_addr = mmgr_malloc(fw->blob.text_size);
		if (code_addr == mmgr_NULL)
			return code_addr;
		mmgr_store(code_addr, fw->blob.text, fw->blob.text_size);
	}

	/* Set the correct start address for the SP program */
	sh_css_sp_activate_program(fw, code_addr, sp_prog);

return code_addr;
}

void sh_css_sp_activate_program(
	const struct sh_css_fw_info *fw,
	hrt_vaddress code_addr,
	const char *sp_prog)
{
	(void)sp_prog; /* not used on hardware, only for simulation */

assert(sizeof(hrt_vaddress) <= sizeof(hrt_data));
	/* now we program the base address into the icache and
	 * invalidate the cache.
	 */
	sp_ctrl_store(SP0_ID, SP_ICACHE_ADDR_REG, (hrt_data)code_addr);
	sp_ctrl_setbit(SP0_ID, SP_ICACHE_INV_REG, SP_ICACHE_INV_BIT);

	/* Set descr in the SP to initialize the SP DMEM */
	sh_css_sp_store_init_dmem(fw);
return;
}
