/*
 *
 * (C) COPYRIGHT 2010-2011, 2013 ARM Limited. All rights reserved.
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





#include <ump/ump.h>
#include <memory.h>
#include <stdio.h>

/*
 * Example routine to exercise the user space UMP api.
 * This routine initializes the UMP api and allocates some CPU+device X memory.
 * No usage hints are given, so the driver will use the default cacheability policy.
 * With the allocation it creates a duplicate handle and plays with the reference count.
 * Then it simulates interacting with a device and contains pseudo code for the device.
 *
 * If any error is detected correct cleanup will be performed and -1 will be returned.
 * If successful then 0 will be returned.
 */

static int test_ump_user_api(void)
{
	/* This is the size we try to allocate*/
	const size_t alloc_size = 4096;

	ump_handle h = UMP_INVALID_MEMORY_HANDLE;
	ump_handle h_copy = UMP_INVALID_MEMORY_HANDLE;
	ump_handle h_clone = UMP_INVALID_MEMORY_HANDLE;

	void * mapping = NULL;

	ump_result ump_api_res;
	int result = -1;

	ump_secure_id id;

	size_t size_returned;

	ump_api_res = ump_open();
	if (UMP_OK != ump_api_res)
	{
		/* failed to open an ump session */
		/* early out */
		return -1;
	}

	h = ump_allocate_64(alloc_size, UMP_PROT_CPU_RD | UMP_PROT_CPU_WR | UMP_PROT_X_RD | UMP_PROT_X_WR);
	/* the refcount is now 1 */
	if (UMP_INVALID_MEMORY_HANDLE == h)
	{
		/* allocation failure */
		goto cleanup;
	}

	/* this is how we could share this allocation with another process */

	/* in process A: */
	id = ump_secure_id_get(h);
	/* still ref count 1 */
	/* send the id to process B */

	/* in process B: */
	/* receive the id from A */
	h_clone = ump_from_secure_id(id);
	/* the ref count of the allocation is now 2 (one from each handle to it) */
	/* do something ... */
	/* release our clone */
	ump_release(h_clone); /* safe to call even if ump_from_secure_id failed */
	h_clone = UMP_INVALID_MEMORY_HANDLE;


	/* a simple save-for-future-use logic inside the driver would just copy the handle (but add a ref manually too!) */
	/*
	 * void assign_memory_to_job(h)
	 * {
	  */
	h_copy = h;
	ump_retain(h_copy); /* manual retain needed as we just assigned the handle, now 2 */
	/*
	 * }
	 *
	 * void job_completed(void)
	 * {
	 */
	 ump_release(h_copy); /* normal handle release as if we got via an ump_allocate */
	 h_copy = UMP_INVALID_MEMORY_HANDLE;
	 /*
	 * }
	 */
	
	/* we're now back at ref count 1, and only h is a valid handle */
	/* enough handle duplication show-off, let's play with the contents instead */

	mapping = ump_map(h, 0, alloc_size);
	if (NULL == mapping)
	{
		/* mapping failure, either out of address space or some other error */
		goto cleanup;
	}

	memset(mapping, 0, alloc_size);

	/* let's pretend we're going to start some hw device on this buffer and read the result afterwards */
	ump_cpu_msync_now(h, UMP_MSYNC_CLEAN, mapping, alloc_size);
 	/*
		device cache invalidate

		memory barrier

		start device

		memory barrier

		wait for device

		memory barrier

		device cache clean

		memory barrier
	*/
	ump_cpu_msync_now(h, UMP_MSYNC_CLEAN_AND_INVALIDATE, mapping, alloc_size);

	/* we could now peek at the result produced by the hw device, which is now accessible via our mapping */

	/* unmap the buffer when we're done with it */
	ump_unmap(h, mapping, alloc_size);

	result = 0;

cleanup:
	ump_release(h);
	h = UMP_INVALID_MEMORY_HANDLE;

	ump_close();

	return result;
}

