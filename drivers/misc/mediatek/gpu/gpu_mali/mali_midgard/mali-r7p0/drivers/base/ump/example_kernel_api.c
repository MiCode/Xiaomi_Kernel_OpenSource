/*
 *
 * (C) COPYRIGHT 2010-2013 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */





#include <linux/ump.h>

#include <stdio.h>
#include <stdlib.h>

/*
 * Example routine to display information about an UMP allocation
 * The routine takes an secure_id which can come from a different kernel module
 * or from a client application (i.e. an ioctl).
 * It creates a ump handle from the secure id (which validates the secure id)
 * and if successful dumps the physical memory information.
 * It follows the API and pins the memory while "using" the physical memory.
 * Finally it calls the release function to indicate it's finished with the handle.
 *
 * If the function can't look up the handle it fails with return value -1.
 * If the testy succeeds then it return 0.
 * */

static int display_ump_memory_information(ump_secure_id secure_id)
{
	const ump_dd_physical_block_64 * ump_blocks = NULL;
	ump_dd_handle ump_mem;
	uint64_t nr_blocks;
	int i;
	ump_alloc_flags flags;

	/* get a handle from the secure id */
	ump_mem = ump_dd_from_secure_id(secure_id);

	if (UMP_DD_INVALID_MEMORY_HANDLE == ump_mem)
	{
		/* invalid ID received */
		return -1;
	}

	/* at this point we know we've added a reference to the ump allocation, so we must release it with ump_dd_release */

	ump_dd_phys_blocks_get_64(ump_mem, &nr_blocks, &ump_blocks);
	flags = ump_dd_allocation_flags_get(ump_mem);

	printf("UMP allocation with secure ID %u consists of %zd physical block(s):\n", secure_id, nr_blocks);

	for(i=0; i<nr_blocks; ++i)
	{
		printf("\tBlock %d: 0x%08zX size 0x%08zX\n", i, ump_blocks[i].addr, ump_blocks[i].size);
	}

	printf("and was allocated using the following bitflag combo:  0x%lX\n", flags);

	ump_dd_release(ump_mem);

	return 0;
}

