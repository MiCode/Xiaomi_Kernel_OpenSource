 /* Intel Management Engine Interface (Intel MEI) Linux driver
  Intel MEI Interface Header

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2003-2012 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  Contact Information:
	  Intel Corporation.
	  linux-mei@linux.intel.com
	  http://www.intel.com


  BSD LICENSE

  Copyright(c) 2003-2011 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef _MEI_MM_H_
#define _MEI_MM_H_

#ifdef __KERNEL__

/*
 * Structure type definition for  DMA physical memory
 * @kvaddr - kernel virtual address of the buffer
 * @cpaddr - physical address (CPU centric)
 * @dmaddr - address as seen by the PCI device
 * @offset - bytes, already allocated from the chunk
 * @bytes  - total size
 */
struct mei_mm_desc {
	void *kvaddr;
	unsigned long cpaddr;
	dma_addr_t    dmaddr;
	unsigned long offset;
	unsigned long bytes;
};

/**
 * mei_dma_init -  Init Routine mei_dma misc device
 *
 * mei_dma_init is called by probe function of mei module.
 * @mm_desc - ptr to ddma_chunk_t, containing information about allocated memory chunk
 */
struct mei_mm_device *mei_mm_init(struct device *dev,
	void *vaddr, dma_addr_t paddr, size_t size);

/**
 * mei_dma_deinit - De-Init Routine for mei_dma misc device
 *
 * mei_dma_deinit is called by release function of mei module.
 */
void mei_mm_deinit(struct mei_mm_device *mdev);

#endif /*__KERNEL__ */

/* structure is used to supply DMA chunk distribution to SEC application*/
struct mei_mm_data {
	__u64 size;
};

/* IOCTL number of commands */
#define MEI_IOC_MAXNR 3

/*
 * This IOCTLs are used allocate/free memory from/to DMA chunk -
 * physical contiguous memory chunk, allocated in mei driver init
 */

#define IOCTL_MEI_MM_ALLOC \
	_IOWR('H' , 0x02, struct mei_mm_data)

#define IOCTL_MEI_MM_FREE \
	_IOWR('H' , 0x03, struct mei_mm_data)


#endif /* _MEI_MM_H_ */
