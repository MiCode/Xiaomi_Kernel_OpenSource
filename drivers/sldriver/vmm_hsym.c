/*
 * The file implements SL VMM hypercalls.
 * Copyright (c) 2014, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include "include/vmm_hsym.h"
#include "include/vmm_hsym_common.h"

enum{
	no_rights    = 0,
	r_rights     = 1,
	w_rights     = 2,
	x_rights     = 4,
	rw_rights    = 3,
	rx_rights    = 5,
	rwx_rights   = 7,
	all_rights   = 7
}perm_t;


/* FIXME: these inlines should come from an appropriate header file */
#ifdef __x86_64__
inline int cpuid_asm64(uint32_t leaf, uint32_t b_val, uint64_t c, 
        uint64_t d, uint64_t S, uint64_t D)
{
    int status;
      asm volatile("cpuid"
            : "=a" (status), "+g" (b_val), "+c" (c), "+d" (d)
            : "0" (leaf), "m" (b_val), "S" (S), "D" (D)
            : );
    //printk(KERN_ERR"exit cpuid status (%x) %x %x %x %x %x %x", 
    //status, leaf, b_val, c, d, S, D);
    return status;
}
#else
inline int cpuid_asm(uint32_t leaf, uint32_t b_val, uint32_t c, 
        uint32_t d, uint32_t S, uint32_t D)
{
    int status;
    //printk(KERN_ERR"entry cpuid %x %x %x %x %x %x \n", 
    //leaf, b_val, c, d, S, D);
      asm volatile("cpuid"
            : "=a" (status), "+g" (b_val), "+c" (c), "+d" (d)
            : "0" (leaf), "m" (b_val), "S" (S), "D" (D)
            : );
    //printk(KERN_ERR"exit cpuid status (%x) %x %x %x %x %x %x", 
    //status, leaf, b_val, c, d, S, D);
    return status;
}
#endif

#ifdef __x86_64__
inline int vmcall_asm64(uint32_t leaf, uint32_t b_val, uint64_t c,
			uint64_t d, uint64_t S, uint64_t D)
{
	int status;
	asm volatile("vmcall"
		     : "=a" (status), "+g" (b_val), "+c" (c), "+d" (d)
		     : "0" (leaf), "m" (b_val), "S" (S), "D" (D)
		     : );
	/* printk(KERN_ERR"exit cpuid status (%x) %x %x %x %x %x %x",
	   status, leaf, b_val, c, d, S, D); */
	return status;
}
#else
inline int vmcall_asm(uint32_t leaf, uint32_t b_val, uint32_t c,
		      uint32_t d, uint32_t S, uint32_t D)
{
	int status;
	/* pr_err("entry cpuid %x %x %x %x %x %x\n",
	   leaf, b_val, c, d, S, D); */
	asm volatile("vmcall"
		     : "=a" (status), "+g" (b_val), "+c" (c), "+d" (d)
		     : "0" (leaf), "m" (b_val), "S" (S), "D" (D)
		     : );
	/* printk(KERN_ERR"exit cpuid status (%x) %x %x %x %x %x %x",
	   status, leaf, b_val, c, d, S, D); */
	return status;
}
#endif

static int do_get_data(const char *msg, perf_type_t type, uint32_t cmd,
                        void *buf, uint32_t n, uint32_t cpu, uint32_t offset)
{
    int limit = 0;
    uint32_t count = 0;
    uint8_t *dst = (uint8_t *)buf;
    uint32_t junk;
    
    //printk(KERN_INFO "%s \n", msg);
    //printk(KERN_INFO "%x \n", type);

    while (count < n) {
        // Make sure page is mapped and writable.
        uint32_t actual_bytes;
        junk = *(uint32_t*)dst; 
        *(uint32_t*)dst = junk;

        //printk(KERN_INFO "issuing cpuid %x %x %x %x %x %x\n", 
        //type, cmd, (uint32_t)dst, n - count, offset + count, cpu);

#ifdef __x86_64__
	actual_bytes = vmcall_asm64(type, cmd, dst, n - count,
                                    offset + count, cpu);
#else
	actual_bytes = vmcall_asm(type, cmd, (uint32_t)dst, n - count,
				  offset + count, cpu);
#endif
	/* pr_err("after cpuid %x\n", actual_bytes); */

        if (actual_bytes == -1u) {
            if (*msg == '+')
                printk(KERN_INFO "vmmctrl: %s\n", msg+1);
            else
                printk(KERN_INFO "vmmctrl: %s: Command failed\n", msg);
            return -1;
        }

        // This case can happen when running on bare hardware.
        if (actual_bytes > n - count)
            break;

        if (actual_bytes == 0) {
            if (++limit > 3)
                break;
            continue;
        }
        limit = 0;

        count += actual_bytes;
        dst += actual_bytes;
    }

    if (count != n) {
        if (*msg == '+')
            printk(KERN_INFO "vmmctrl: %s\n", msg+1);
        else
            printk(KERN_INFO "vmmctrl: %s: Did not get the expected amount"
                    " of data (expected %u bytes, got %u bytes)\n",
                    msg, n, count);
        return -1;
    }
    return 0;
}

//void reg_vIDT(hsec_vIDT_param_t *vIDT_info, uint32_t cpu)
void reg_vIDT(void *data)
{
	hsec_vIDT_param_t *vIDT_info = (hsec_vIDT_param_t *) data;
	if (!vIDT_info) {
		pr_err("hypersec: vIDT_info is invalid\n");
		return;
	}

#ifdef __x86_64__
	vmcall_asm64(SL_CMD_HSEC_REG_VIDT, CMD_GET, (uint64_t)vIDT_info,
		     sizeof(*vIDT_info), 0, vIDT_info->cpu);
#else
	vmcall_asm(SL_CMD_HSEC_REG_VIDT, CMD_GET, (uint32_t)vIDT_info,
		   sizeof(*vIDT_info), 0, vIDT_info->cpu);
#endif
	return;
}
EXPORT_SYMBOL(reg_vIDT);

void reg_sl_global_info(hsec_sl_param_t *sl_info)
{
	if (!sl_info) {
		pr_err("hypersec: sl_info is invalid\n");
		return;
	}
#ifdef __x86_64__
	vmcall_asm64(SL_CMD_HSEC_REG_SL_INFO, CMD_GET, (uint64_t)sl_info,
		     sizeof(*sl_info), 0, 0);
#else
	vmcall_asm(SL_CMD_HSEC_REG_SL_INFO, CMD_GET, (uint32_t)sl_info,
		   sizeof(*sl_info), 0, 0);
#endif
	return;
}
EXPORT_SYMBOL(reg_sl_global_info);

