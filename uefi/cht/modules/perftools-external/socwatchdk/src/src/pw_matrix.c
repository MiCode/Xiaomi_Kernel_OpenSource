/* ***********************************************************************************************

   This file is provided under a dual BSD/GPLv2 license.  When using or 
   redistributing this file, you may do so under either license.

   GPL LICENSE SUMMARY

   Copyright(c) 2013 Intel Corporation. All rights reserved.

   This program is free software; you can redistribute it and/or modify 
   it under the terms of version 2 of the GNU General Public License as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful, but 
   WITHOUT ANY WARRANTY; without even the implied warranty of 
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
   General Public License for more details.

   You should have received a copy of the GNU General Public License 
   along with this program; if not, write to the Free Software 
   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
   The full GNU General Public License is included in this distribution 
   in the file called LICENSE.GPL.

   Contact Information:
   SOCWatch Developer Team <socwatchdevelopers@intel.com>

   BSD LICENSE 

   Copyright(c) 2013 Intel Corporation. All rights reserved.
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
   ***********************************************************************************************
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/version.h>
#ifdef CONFIG_X86_WANT_INTEL_MID
    #include <asm/intel-mid.h>
#endif
#ifdef CONFIG_RPMSG_IPC
    #include <asm/intel_mid_rpmsg.h>
#endif // CONFIG_RPMSG_IPC
#ifdef CONFIG_INTEL_SCU_IPC
    #include <asm/intel_scu_pmic.h>	/* Needed for 3.4 kernel port */
#endif // CONFIG_INTEL_SCU_IPC
#include <linux/kdev_t.h>
#include <asm/paravirt.h>
#include <linux/sfi.h> // To retrieve SCU F/W version

#include "pw_structs.h"
#include "pw_output_buffer.h"
#include "matrix.h"

// #define NAME "matrix"
#define DRV_NAME "socwatch"
//#define DRIVER_VERSION "1.0"

#define MT_SUCCESS 0
#define MT_ERROR 1

#define MCR_WRITE_OPCODE    0x11
#define BIT_POS_OPCODE      24
/*
 * Should we be doing 'direct' PCI reads and writes?
 * '1' ==> YES, call "pci_{read,write}_config_dword()" directly
 * '0' ==> NO, Use the "intel_mid_msgbus_{read32,write32}_raw()" API (defined in 'intel_mid_pcihelpers.c')
 */
#define DO_DIRECT_PCI_READ_WRITE 0
#ifndef CONFIG_X86_WANT_INTEL_MID
    /*
     * 'intel_mid_pcihelpers.h' is probably not present -- force
     * direct PCI calls in this case.
     */
    #undef DO_DIRECT_PCI_READ_WRITE
    #define DO_DIRECT_PCI_READ_WRITE  1
#endif
#if !DO_DIRECT_PCI_READ_WRITE
    #include <asm/intel_mid_pcihelpers.h>
#endif

#define PW_NUM_SOC_COUNTERS 9
extern void SOCPERF_Read_Data (void *data_buffer);

static int matrix_major_number;
static bool instantiated;
static bool mem_alloc_status;
static u8 *ptr_lut_ops = NULL;
static unsigned long io_pm_status_reg;
static unsigned long io_pm_lower_status;
static unsigned long io_pm_upper_status;
static unsigned int io_base_pwr_address;
static dev_t matrix_dev;
static struct cdev *matrix_cdev;
static struct class *matrix_class;
static struct timeval matrix_time;
static struct device *matrix_device;
static struct lookup_table *ptr_lut;
static struct mtx_size_info lut_info;

extern u16 pw_scu_fw_major_minor; // defined in 'apwr_driver.c'

static int mt_free_memory(void);

#define PRINT_LUT_LENGTHS(which, what) do { \
    printk(KERN_INFO "GU: ptr_lut has %s_%s_length = %lu\n", #which, #what, ptr_lut->which##_##what##_##length); \
} while(0)

#define PRINT_LUT_WBS(which, what) do { \
    printk(KERN_INFO "GU: ptr_lut has %s_%s_wb = %lu\n", #which, #what, ptr_lut->which##_##what##_##wb); \
} while(0)

#define TOTAL_ONE_SHOT_LENGTH(type) ({unsigned long __length = 0; \
        __length += ptr_lut->msr_##type##_length * sizeof(struct mtx_msr); \
        __length += ptr_lut->mem_##type##_length * sizeof(struct memory_map); \
        __length += ptr_lut->pci_ops_##type##_length * sizeof(struct mtx_pci_ops); \
        __length += ptr_lut->cfg_db_##type##_length * sizeof(unsigned long); \
        __length += ptr_lut->soc_perf_##type##_length * sizeof(struct mtx_soc_perf); \
        __length;})

#define TOTAL_ONE_SHOT_LEN(type) ({unsigned long __length = 0; \
        __length += ptr_lut->msr_##type##_wb * sizeof(struct mt_msr_buffer); \
        __length += ptr_lut->mem_##type##_wb * sizeof(u32); \
        __length += ptr_lut->pci_ops_##type##_wb * sizeof(u32); \
        __length += ptr_lut->cfg_db_##type##_wb * sizeof(u32); \
        /*__length += ptr_lut->soc_perf_##type##_wb * sizeof(struct mt_soc_perf_buffer); */\
        /* GU: Ring-3 now sets 'soc_perf_poll_wb' == MAX_soc_perf_VALUES */ \
        __length += ptr_lut->soc_perf_##type##_wb * sizeof(u64); \
        __length;})

static pw_mt_msg_t *mt_msg_init_buff = NULL, *mt_msg_poll_buff = NULL, *mt_msg_term_buff = NULL;

static u32 mt_platform_pci_read32(u32 address);
static void mt_platform_pci_write32(unsigned long address, unsigned long data);

/**
 * Matrix Driver works in such a way that only one thread
 * and one instance of driver can occur at a time.
 * At the time of opening the driver file, driver checks driver
 * status whether its already instantiated or not.. if it is, it
 * will not allow to open new instance..
 */

#define MATRIX_GET_TIME_STAMP(time_stamp) \
    do { \
        do_gettimeofday(&matrix_time); \
        time_stamp = \
        (((u64)matrix_time.tv_sec * 1000000) \
         + (u64)matrix_time.tv_usec); \
    } while (0)

#define MATRIX_GET_TSC(tsc) rdtscll(tsc)

#define MATRIX_INCREMENT_MEMORY(cu, cl, buffer, type, lut) \
    do { \
        if (lut) { \
            buffer##_info.init_##type##_size = \
            sizeof(cu cl) * ptr_lut->type##_##init_length; \
            buffer##_info.term_##type##_size = \
            sizeof(cu cl) * ptr_lut->type##_##term_length; \
            buffer##_info.poll_##type##_size = \
            sizeof(cu cl) * ptr_lut->type##_##poll_length; \
            lut_info.total_mem_bytes_req += \
            buffer##_info.init_##type##_size + \
            buffer##_info.term_##type##_size + \
            buffer##_info.poll_##type##_size; \
        } \
    } while (0)

#define MATRIX_IO_REMAP_MEMORY(state) \
    do { \
        unsigned long count; \
        for (count = 0; \
                count < ptr_lut->mem_##state##_length; count++) { \
            if (ptr_lut->mmap_##state[count].ctrl_addr) { \
                ptr_lut->mmap_##state[count].ctrl_remap_address = \
                ioremap_nocache(ptr_lut-> \
                        mmap_##state[count].ctrl_addr, \
                        sizeof(unsigned long)); \
            } else { \
                ptr_lut->mmap_##state[count].ctrl_remap_address = NULL; \
            } \
            if (ptr_lut->mmap_##state[count].data_addr) { \
                ptr_lut->mmap_##state[count].data_remap_address = \
                ioremap_nocache(ptr_lut-> \
                        mmap_##state[count].data_addr, \
                        (ptr_lut-> \
                         mmap_##state[count].data_size) \
                        * sizeof(unsigned long));\
            }  else { \
                ptr_lut->mmap_##state[count].data_remap_address = NULL; \
            }  \
        } \
    } while (0)

#define MATRIX_IOUNMAP_MEMORY(state) \
    do { \
        unsigned long count; \
        for (count = 0; \
                count < ptr_lut->mem_##state##_length; count++) { \
            if (ptr_lut->mmap_##state[count].ctrl_remap_address) { \
                iounmap(ptr_lut->mmap_##state[count]. \
                        ctrl_remap_address); \
                ptr_lut->mmap_##state[count]. \
                ctrl_remap_address = NULL; \
            } \
            if (ptr_lut->mmap_##state[count].data_remap_address) { \
                iounmap(ptr_lut->mmap_##state[count]. \
                        data_remap_address); \
                ptr_lut->mmap_##state[count]. \
                data_remap_address = NULL; \
            } \
        } \
    } while (0)

#define MATRIX_BOOK_MARK_LUT(state, type, struct_init, structure, member, mem, label) \
    do { \
        if (lut_info.state##_##type##_size) { \
            if (copy_from_user \
                    (&ptr_lut_ops[offset], ptr_lut->member##_##state, \
                     lut_info.state##_##type##_size) > 0) { \
                dev_dbg(matrix_device, \
                        "file : %s ,function : %s ,line %i\n", \
                        __FILE__, __func__, __LINE__); \
                goto label; \
            } \
            ptr_lut->member##_##state =  \
            (struct_init structure*)&ptr_lut_ops[offset]; \
            offset += lut_info.state##_##type##_size; \
            if (mem) \
            MATRIX_IO_REMAP_MEMORY(state); \
        } else \
        ptr_lut->member##_##state = NULL; \
    } while (0)

#define ALLOW_MATRIX_MSR_READ_WRITE 1
#if ALLOW_MATRIX_MSR_READ_WRITE
    #define MATRIX_RDMSR_ON_CPU(cpu, addr, low, high) ({int __tmp = rdmsr_on_cpu((cpu), (addr), (low), (high)); __tmp;})
    #define MATRIX_WRMSR_ON_CPU(cpu, addr, low, high) ({int __tmp = wrmsr_on_cpu((cpu), (addr), (low), (high)); __tmp;})
#else
    #define MATRIX_RDMSR_ON_CPU(cpu, addr, low, high) ({int __tmp = 0; *(low) = 0; *(high) = 0; __tmp;})
    #define MATRIX_WRMSR_ON_CPU(cpu, addr, low, high) ({int __tmp = 0; __tmp;})
#endif // ALLOW_MATRIX_MSR_READ

/*
 * Function declarations (incomplete).
 */
#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
    static long mt_device_compat_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param);
    static long mt_device_compat_init_ioctl_i(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param);
    static long mt_device_compat_msr_ioctl_i(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param);
    static long mt_device_compat_pci_config_ioctl_i(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param);
    static long mt_device_compat_config_db_ioctl_i(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param);
    static int mt_ioctl_mtx_msr_compat_i(struct mtx_msr_container __user *remote_args, struct mtx_msr_container32 __user *remote_args32);
    static int mt_ioctl_pci_config_compat_i(struct pci_config __user *remote_args, struct pci_config32 __user *remote_args32);
    static long mt_get_scu_fw_version_compat_i(u16 __user *remote_args32);
    static int mt_copy_mtx_msr_info_i(struct mtx_msr *msr, const struct mtx_msr32 __user *msr32, u32 length);
    static int mt_copy_mmap_info_i(struct memory_map *mem, const struct memory_map32 __user *mem32, unsigned long length);
    static int mt_copy_pci_info_i(struct mtx_pci_ops *pci, const struct mtx_pci_ops32 __user *pci32, unsigned long length);
    static int mt_copy_cfg_db_info_i(unsigned long *cfg, const u32 __user *cfg32, unsigned long length);
    static int mt_copy_soc_perf_info_i(struct mtx_soc_perf *soc_perf, const struct mtx_soc_perf32 __user *soc_perf32, unsigned long length);
#endif
/*
 * MT_MSG functions.
 */
static void mt_free_msg_memory(void)
{
    if (mt_msg_init_buff) {
        vfree(mt_msg_init_buff);
    }
    if (mt_msg_poll_buff) {
        vfree(mt_msg_poll_buff);
    }
    if (mt_msg_term_buff) {
        vfree(mt_msg_term_buff);
    }

    mt_msg_init_buff = 0x0;
    mt_msg_poll_buff = 0x0;
    mt_msg_term_buff = 0x0;

    // printk(KERN_INFO "OK, freed matrix MSG temp buffers!\n");
};

static int mt_init_msg_memory(void)
{
    unsigned int init_len = TOTAL_ONE_SHOT_LEN(init);
    unsigned int poll_len = TOTAL_ONE_SHOT_LEN(poll);
    unsigned int term_len = TOTAL_ONE_SHOT_LEN(term);

    // printk(KERN_INFO "sizeof mt_xchange = %lu, sizeof mt_msr_buffer = %lu, header_size = %lu\n", sizeof(struct mt_xchange_buffer), sizeof(struct mt_msr_buffer), PW_MT_MSG_HEADER_SIZE());

    mt_msg_init_buff = (pw_mt_msg_t *)vmalloc(PW_MT_MSG_HEADER_SIZE() + sizeof(struct mt_xchange_buffer) + init_len);
    if (!mt_msg_init_buff) {
        mt_free_msg_memory();
        return -MT_ERROR;
    }
    memset(mt_msg_init_buff, 0, PW_MT_MSG_HEADER_SIZE() + sizeof(struct mt_xchange_buffer) + init_len);
    mt_msg_init_buff->data_type = (u16)PW_MT_MSG_INIT;
    mt_msg_init_buff->data_len = (u16)(sizeof(struct mt_xchange_buffer) + init_len);
    {
        int __dst_idx = 0;
        char *__buff_ops = (char *)&mt_msg_init_buff->p_data;
        struct mt_xchange_buffer *__buff = (struct mt_xchange_buffer *)&__buff_ops[__dst_idx]; __dst_idx += sizeof(*__buff);
        __buff->msr_length = ptr_lut->msr_init_wb; // printk(KERN_INFO "GU: set INIT msr_length = %lu\n", __buff->msr_length);
        __buff->mem_length = ptr_lut->mem_init_wb;
        __buff->pci_ops_length = ptr_lut->pci_ops_init_wb;
        __buff->cfg_db_length = ptr_lut->cfg_db_init_wb;
        __buff->soc_perf_length = ptr_lut->soc_perf_init_wb;

        if (__buff->msr_length) {
            __buff->ptr_msr_buff = (u64)(unsigned long)&__buff_ops[__dst_idx]; __dst_idx += sizeof(struct mt_msr_buffer) * __buff->msr_length;
        }
        if (__buff->mem_length) {
            __buff->ptr_mem_buff = (u64)(unsigned long)&__buff_ops[__dst_idx]; __dst_idx += sizeof(u32) * __buff->mem_length;
        }
        if (__buff->pci_ops_length) {
            __buff->ptr_pci_ops_buff = (u64)(unsigned long)&__buff_ops[__dst_idx]; __dst_idx += sizeof(u32) * __buff->pci_ops_length;
        }
        if (__buff->cfg_db_length) {
            __buff->ptr_cfg_db_buff = (u64)(unsigned long)&__buff_ops[__dst_idx]; __dst_idx += sizeof(u32) * __buff->cfg_db_length;
        }
        if (__buff->soc_perf_length) {
            __buff->ptr_soc_perf_buff = (u64)(unsigned long)&__buff_ops[__dst_idx];
        }
    }

    mt_msg_poll_buff = (pw_mt_msg_t *)vmalloc(PW_MT_MSG_HEADER_SIZE() + sizeof(struct mt_xchange_buffer) + poll_len);
    if (!mt_msg_poll_buff) {
        mt_free_msg_memory();
        return -MT_ERROR;
    }
    memset(mt_msg_poll_buff, 0, PW_MT_MSG_HEADER_SIZE() + sizeof(struct mt_xchange_buffer) + poll_len);
    mt_msg_poll_buff->data_type = (u16)PW_MT_MSG_POLL;
    mt_msg_poll_buff->data_len = (u16)(sizeof(struct mt_xchange_buffer) + poll_len);
    {
        int __dst_idx = 0;
        char *__buff_ops = (char *)&mt_msg_poll_buff->p_data;
        struct mt_xchange_buffer *__buff = (struct mt_xchange_buffer *)&__buff_ops[__dst_idx]; __dst_idx += sizeof(*__buff);
        __buff->msr_length = ptr_lut->msr_poll_wb; // printk(KERN_INFO "GU: set poll msr_length = %lu\n", __buff->msr_length);
        __buff->mem_length = ptr_lut->mem_poll_wb;
        __buff->pci_ops_length = ptr_lut->pci_ops_poll_wb;
        __buff->cfg_db_length = ptr_lut->cfg_db_poll_wb;
        __buff->soc_perf_length = ptr_lut->soc_perf_poll_wb;

        if (__buff->msr_length) {
            __buff->ptr_msr_buff = (u64)(unsigned long)&__buff_ops[__dst_idx]; __dst_idx += sizeof(struct mt_msr_buffer) * __buff->msr_length;
        }
        if (__buff->mem_length) {
            __buff->ptr_mem_buff = (u64)(unsigned long)&__buff_ops[__dst_idx]; __dst_idx += sizeof(u32) * __buff->mem_length;
        }
        if (__buff->pci_ops_length) {
            __buff->ptr_pci_ops_buff = (u64)(unsigned long)&__buff_ops[__dst_idx]; __dst_idx += sizeof(u32) * __buff->pci_ops_length;
        }
        if (__buff->cfg_db_length) {
            __buff->ptr_cfg_db_buff = (u64)(unsigned long)&__buff_ops[__dst_idx]; __dst_idx += sizeof(u32) * __buff->cfg_db_length;
        }
        if (__buff->soc_perf_length) {
            __buff->ptr_soc_perf_buff = (u64)(unsigned long)&__buff_ops[__dst_idx];
        }
    }

    mt_msg_term_buff = (pw_mt_msg_t *)vmalloc(PW_MT_MSG_HEADER_SIZE() + sizeof(struct mt_xchange_buffer) + term_len);
    if (!mt_msg_term_buff) {
        mt_free_msg_memory();
        return -MT_ERROR;
    }
    memset(mt_msg_term_buff, 0, PW_MT_MSG_HEADER_SIZE() + sizeof(struct mt_xchange_buffer) + term_len);
    mt_msg_term_buff->data_type = (u16)PW_MT_MSG_TERM;
    mt_msg_term_buff->data_len = (u16)(sizeof(struct mt_xchange_buffer) + term_len);
    {
        int __dst_idx = 0;
        char *__buff_ops = (char *)&mt_msg_term_buff->p_data;
        struct mt_xchange_buffer *__buff = (struct mt_xchange_buffer *)&__buff_ops[__dst_idx]; __dst_idx += sizeof(*__buff);
        __buff->msr_length = ptr_lut->msr_term_wb; // printk(KERN_INFO "GU: set term msr_length = %lu\n", __buff->msr_length);
        __buff->mem_length = ptr_lut->mem_term_wb;
        __buff->pci_ops_length = ptr_lut->pci_ops_term_wb;
        __buff->cfg_db_length = ptr_lut->cfg_db_term_wb;
        __buff->soc_perf_length = ptr_lut->soc_perf_term_wb;

        if (__buff->msr_length) {
            __buff->ptr_msr_buff = (u64)(unsigned long)&__buff_ops[__dst_idx]; __dst_idx += sizeof(struct mt_msr_buffer) * __buff->msr_length;
        }
        if (__buff->mem_length) {
            __buff->ptr_mem_buff = (u64)(unsigned long)&__buff_ops[__dst_idx]; __dst_idx += sizeof(u32) * __buff->mem_length;
        }
        if (__buff->pci_ops_length) {
            __buff->ptr_pci_ops_buff = (u64)(unsigned long)&__buff_ops[__dst_idx]; __dst_idx += sizeof(u32) * __buff->pci_ops_length;
        }
        if (__buff->cfg_db_length) {
            __buff->ptr_cfg_db_buff = (u64)(unsigned long)&__buff_ops[__dst_idx]; __dst_idx += sizeof(u32) * __buff->cfg_db_length;
        }
        if (__buff->soc_perf_length) {
            __buff->ptr_soc_perf_buff = (u64)(unsigned long)&__buff_ops[__dst_idx];
        }
    }
    return MT_SUCCESS;
};

static int mt_msg_scan_msr(struct mt_xchange_buffer *xbuff, const struct mtx_msr *msrs, unsigned long max_msr_loop)
{
    unsigned long lut_loop = 0, msr_loop = 0;
    for (lut_loop=0; lut_loop<max_msr_loop; ++lut_loop) {
        unsigned int cpu;
        u32 *lo_rd, *high_rd, lo_wr, high_wr;
        u32 msr_no;
        struct mt_msr_buffer *msr_buff = (struct mt_msr_buffer *)(unsigned long)xbuff->ptr_msr_buff;

        cpu = (unsigned int)msrs[lut_loop].n_cpu;
        msr_no = msrs[lut_loop].ecx_address;
        /*
        lo_rd = (u32 *)&xbuff->ptr_msr_buff[msr_loop].eax_LSB;
        high_rd = (u32 *)&xbuff->ptr_msr_buff[msr_loop].edx_MSB;
        lo_wr = xbuff->ptr_msr_buff[msr_loop].eax_LSB;
        high_wr = xbuff->ptr_msr_buff[msr_loop].edx_MSB;
        */
        lo_rd = (u32 *)&msr_buff[msr_loop].eax_LSB;
        high_rd = (u32 *)&msr_buff[msr_loop].edx_MSB;
        lo_wr = msr_buff[msr_loop].eax_LSB;
        high_wr = msr_buff[msr_loop].edx_MSB;

        switch (msrs[lut_loop].operation) {
            case READ_OP:
                MATRIX_RDMSR_ON_CPU(cpu, msr_no, lo_rd, high_rd);
                ++msr_loop;
                // printk(KERN_INFO "GU: read MSR addr 0x%lx on cpu %d in INIT/TERM MT_MSG scan! Val = %llu\n", msr_no, cpu, ((u64)*high_rd << 32 | (u64)*lo_rd));
                break;
            case WRITE_OP:
                MATRIX_WRMSR_ON_CPU(cpu, msr_no, lo_wr, high_wr);
                break;
            case SET_BITS_OP:
                {
                    u32 eax_LSB, edx_MSB;
                    MATRIX_RDMSR_ON_CPU(cpu, msr_no, &eax_LSB, &edx_MSB);
                    MATRIX_WRMSR_ON_CPU(cpu, msr_no, (eax_LSB | lo_wr), (edx_MSB | high_wr));
                }
                break;
            case RESET_BITS_OP:
                {
                    u32 eax_LSB, edx_MSB;
                    MATRIX_RDMSR_ON_CPU(cpu, msr_no, &eax_LSB, &edx_MSB);
                    MATRIX_WRMSR_ON_CPU(cpu, msr_no, (eax_LSB & ~(lo_wr)), (edx_MSB & ~(high_wr)));
                }
                break;
            default:
                dev_dbg(matrix_device, "Error in MSR_OP value..\n");
                return -MT_ERROR;
        }
    }
    return MT_SUCCESS;
};

#if DO_ANDROID

#ifdef CONFIG_RPMSG_IPC
    #define MATRIX_SCAN_MMAP_DO_IPC(cmd, sub_cmd) rpmsg_send_generic_simple_command(cmd, sub_cmd)
#else
    #define MATRIX_SCAN_MMAP_DO_IPC(cmd, sub_cmd) (-ENODEV)
#endif // CONFIG_RPMSG_IPC

static int mt_msg_scan_mmap(struct mt_xchange_buffer *xbuff, const struct memory_map *mmap, unsigned long max_mem_loop, unsigned long max_mem_lut_loop)
{
    unsigned long mem_loop = 0;
    unsigned long scu_sub_cmd = 0;
    unsigned long scu_cmd = 0;
    unsigned long lut_loop = 0;

    for (lut_loop = 0; lut_loop < max_mem_lut_loop; ++lut_loop) {
        /* If ctrl_addr != NULL, we do scu IPC command
         * else it is a case of mmio read and data_remap
         * _address should point to the mmio address from which
         * we need to read 
         */
        // printk(KERN_INFO "lut_loop = %lu, ctrl_addr = %lu, data_addr = %lu, data_size = %lu\n", lut_loop, mmap[lut_loop].ctrl_addr, mmap[lut_loop].data_addr, mmap[lut_loop].data_size);
        if (mmap[lut_loop].ctrl_addr) {
            scu_cmd = mmap[lut_loop].ctrl_data & 0xFF;
            scu_sub_cmd = (mmap[lut_loop].ctrl_data >> 12) & 0xF;
            if (MATRIX_SCAN_MMAP_DO_IPC(scu_cmd, scu_sub_cmd)) {
                dev_dbg(matrix_device, "Unable to get SCU data...\n");
                return -MT_ERROR;
            }
        }
        if (mmap[lut_loop].data_size != 0) {
            memcpy(&((u32 *)(unsigned long)xbuff->ptr_mem_buff)[mem_loop], mmap[lut_loop].data_remap_address, mmap[lut_loop].data_size * sizeof(u32));
            mem_loop += mmap[lut_loop].data_size;
            if (mem_loop > max_mem_loop) {
                dev_dbg(matrix_device, "A(%04d) [0x%40lu]of [0x%40lu]\n", __LINE__, mem_loop, max_mem_loop);
                return -MT_ERROR;
            }
        }
    }
    return MT_SUCCESS;
};
#else // DO_ANDROID
static int mt_msg_scan_mmap(struct mt_xchange_buffer *xbuff, const struct memory_map *mmap, unsigned long max_mem_loop, unsigned long max_mem_lut_loop)
{
    return MT_SUCCESS;
};
#endif // DO_ANDROID

static int mt_msg_scan(struct mt_xchange_buffer *xbuff, const struct mtx_msr *msrs, const struct memory_map *mmap, const unsigned long *cfg_db, unsigned long max_msr_loop, unsigned long max_mem_loop, unsigned long max_mem_lut_loop, unsigned long max_cfg_db_loop)
{
    unsigned long lut_loop = 0;
    // printk(KERN_INFO "MT_MSG_SCAN: msrs = %p, mmap = %p, cfg_db = %p\n", msrs, mmap, cfg_db);
    if (msrs && mt_msg_scan_msr(xbuff, msrs, max_msr_loop)) {
        printk(KERN_INFO "ERROR reading MT_MSG MSRs!\n");
        return -MT_ERROR;
    }
    if (mmap && mt_msg_scan_mmap(xbuff, mmap, max_mem_loop, max_mem_lut_loop)) {
        printk(KERN_INFO "ERROR reading MT_MSG MMAPs!\n");
        return -MT_ERROR;
    }
    for (lut_loop = 0; lut_loop < max_cfg_db_loop; ++lut_loop) {
        ((u32 *)(unsigned long)xbuff->ptr_cfg_db_buff)[lut_loop] = mt_platform_pci_read32(cfg_db[lut_loop]);
    }
    // TODO pci_ops?
    return MT_SUCCESS;
};

static int mt_produce_mt_msg(const pw_mt_msg_t *mt_msg, u64 tsc)
{
    PWCollector_msg_t msg;

    if (!mt_msg) {
        printk(KERN_INFO "ERROR: trying to produce a NULL MT_MSG?!\n");
        return -MT_ERROR;
    }

    msg.cpuidx = 0; // TODO: set this to 'pw_max_num_cpus'???
    msg.tsc = tsc;
    msg.data_type = MATRIX_MSG; msg.data_len = mt_msg->data_len + PW_MT_MSG_HEADER_SIZE();
    msg.p_data = (u64)(unsigned long)mt_msg;

    // printk(KERN_INFO "PRODUCING mt_msg with TSC = %llu (len = %u, %u)\n", tsc, mt_msg->data_len, msg.data_len);

    return pw_produce_generic_msg_on_cpu(pw_max_num_cpus, &msg, true /* wakeup sleeping readers, if required */);
};

static int mt_msg_init_scan(void)
{
    struct mt_xchange_buffer *xbuff = (struct mt_xchange_buffer *)&mt_msg_init_buff->p_data;
    u64 tsc;

    if (!xbuff) {
        printk(KERN_INFO "ERROR: trying an INIT scan without allocating space?!\n");
        return -MT_ERROR;
    }

    MATRIX_GET_TIME_STAMP(mt_msg_init_buff->timestamp);
    rdtscll(tsc);

    if (mt_msg_scan(xbuff, ptr_lut->msrs_init, ptr_lut->mmap_init, ptr_lut->cfg_db_init, ptr_lut->msr_init_length, xbuff->mem_length, ptr_lut->mem_init_length, ptr_lut->cfg_db_init_length)) {
        printk(KERN_INFO "ERROR doing an MT_INIT scan!\n");
        return -MT_ERROR;
    }

    if (mt_produce_mt_msg(mt_msg_init_buff, tsc)) {
        printk(KERN_INFO "ERROR producing an INIT MT_MSG!\n");
        return -MT_ERROR;
    }

    return MT_SUCCESS;
};

static int mt_msg_term_scan(void)
{
    struct mt_xchange_buffer *xbuff = (struct mt_xchange_buffer *)&mt_msg_term_buff->p_data;
    u64 tsc;

    if (!xbuff) {
        printk(KERN_INFO "ERROR: trying an TERM scan without allocating space?!\n");
        return -MT_ERROR;
    }

    MATRIX_GET_TIME_STAMP(mt_msg_term_buff->timestamp);
    rdtscll(tsc);

    if (mt_msg_scan(xbuff, ptr_lut->msrs_term, ptr_lut->mmap_term, ptr_lut->cfg_db_term, ptr_lut->msr_term_length, xbuff->mem_length, ptr_lut->mem_term_length, ptr_lut->cfg_db_term_length)) {
        printk(KERN_INFO "ERROR doing an MT_TERM scan!\n");
        return -MT_ERROR;
    }

    if (mt_produce_mt_msg(mt_msg_term_buff, tsc)) {
        printk(KERN_INFO "ERROR producing a TERM MT_MSG!\n");
        return -MT_ERROR;
    }

    return MT_SUCCESS;
};

/**
 * poll_scan - function that is called at each iteration of the poll.
 * at each poll observations are made and stored in kernel buffer.
 * @poll_loop : specifies the current iteration of polling
 */
static int mt_msg_poll_scan(unsigned long poll_loop)
{
    unsigned long msr_loop = 0;
    unsigned long mem_loop = 0;
    unsigned long lut_loop;
    unsigned long max_msr_loop;
    unsigned long max_mem_loop;
    unsigned long msr_base_addr;
    unsigned long mem_base_addr;
    unsigned long max_msr_read;
    unsigned long max_cfg_db_loop;
    unsigned long cfg_db_base_addr;
    // unsigned long delta_time;

    u64 tsc;

    struct mt_xchange_buffer *xbuff = (struct mt_xchange_buffer *)&mt_msg_poll_buff->p_data;

    if (ptr_lut == NULL || xbuff == NULL) {
        printk(KERN_INFO "ERROR: trying a POLL scan without allocating space?!\n");
        goto MT_MSG_POLL_ERROR;
    }

    MATRIX_GET_TIME_STAMP(mt_msg_poll_buff->timestamp);
    rdtscll(tsc);

    max_msr_loop = ptr_lut->msr_poll_length;
    max_msr_read = ptr_lut->msr_poll_wb;
    max_mem_loop = xbuff->mem_length;
    max_cfg_db_loop = ptr_lut->cfg_db_poll_length;
    msr_base_addr = 0; // (poll_loop * max_msr_read);
    mem_base_addr = 0; // (poll_loop * max_mem_loop);
    cfg_db_base_addr = 0; // (poll_loop * max_cfg_db_loop);

    if (ptr_lut->msrs_poll) {
        for (lut_loop = 0; lut_loop < max_msr_loop; lut_loop++) {
            if (ptr_lut->msrs_poll[lut_loop].operation == READ_OP) {
                u32 *__lsb = &(((struct mt_msr_buffer *)(unsigned long)xbuff->ptr_msr_buff)[msr_base_addr+msr_loop].eax_LSB);
                u32 *__msb = &(((struct mt_msr_buffer *)(unsigned long)xbuff->ptr_msr_buff)[msr_base_addr+msr_loop].edx_MSB);
                MATRIX_RDMSR_ON_CPU(ptr_lut->msrs_poll[lut_loop].n_cpu, ptr_lut->msrs_poll[lut_loop].ecx_address, __lsb, __msb);
                msr_loop++;
            } else if (ptr_lut->msrs_poll[lut_loop].operation == WRITE_OP) {
                MATRIX_WRMSR_ON_CPU(ptr_lut->msrs_poll[lut_loop].n_cpu, ptr_lut->msrs_poll[lut_loop].ecx_address, ptr_lut->msrs_poll[lut_loop].eax_LSB, ptr_lut->msrs_poll[lut_loop].edx_MSB);
            } else {
                dev_dbg(matrix_device, "Error in MSR_OP value..\n");
                goto MT_MSG_POLL_ERROR;
            }
        }
    }
#if DO_ANDROID
    if (ptr_lut->mmap_poll) {
        for (lut_loop = 0; lut_loop < max_mem_loop; lut_loop++) {
            /*
             * If ctrl_remap_adr = NULL, then the interface does
             * mmio read
             */
            if (ptr_lut->mmap_poll[lut_loop].ctrl_remap_address)
                writel(ptr_lut->mmap_poll[lut_loop].ctrl_data, ptr_lut->mmap_poll[lut_loop].ctrl_remap_address);
            if (ptr_lut->mmap_poll[lut_loop].data_size != 0) {
                memcpy(&((u32 *)(unsigned long)xbuff->ptr_mem_buff)[mem_base_addr + mem_loop], ptr_lut->mmap_poll[lut_loop].data_remap_address, ptr_lut->mmap_poll[lut_loop].data_size * sizeof(u32));
                mem_loop += ptr_lut->mmap_poll[lut_loop].data_size;
                if (mem_loop > max_mem_loop) {
                    dev_dbg(matrix_device, "A(%04d) [0x%40lu]of [0x%40lu]\n", __LINE__, mem_loop, max_mem_loop);
                    goto MT_MSG_POLL_ERROR;
                }
            }
        }
    }

    /* Get the status of power islands in the North Complex */
    io_pm_lower_status = inl(io_pm_status_reg + PWR_STS_NORTH_CMPLX_LOWER);
    io_pm_upper_status = inl(io_base_pwr_address + PWR_STS_NORTH_CMPLX_UPPER);
    memcpy(&((u32 *)(unsigned long)xbuff->ptr_pci_ops_buff)[0], &io_pm_lower_status, sizeof(u32));
    memcpy(&((u32 *)(unsigned long)xbuff->ptr_pci_ops_buff)[1], &io_pm_upper_status, sizeof(u32));

    /* SCU IO */
#ifdef CONFIG_INTEL_SCU_IPC
    if (0 != ptr_lut->scu_poll.length) {
        int status;
        unsigned long offset = 0; // (ptr_lut->scu_poll.length * poll_loop);
        for (lut_loop = 0; lut_loop < ptr_lut->scu_poll.length; lut_loop++) {
            status = intel_scu_ipc_ioread8(ptr_lut->scu_poll.address[lut_loop], &ptr_lut->scu_poll.drv_data[offset + lut_loop]);
            if (status == 0) {
                dev_dbg(matrix_device, "IPC failed for reg: %lu addr: %c ..\n", ptr_lut->scu_poll.address[lut_loop], ptr_lut->scu_poll.drv_data[offset + lut_loop]);
                goto MT_MSG_POLL_ERROR;
            }
        }
    }
#endif // CONFIG_INTEL_SCU_IPC
    cfg_db_base_addr = 0; // (poll_loop * max_cfg_db_loop);
    for (lut_loop = 0; lut_loop < max_cfg_db_loop; lut_loop++) {
        ((u32 *)(unsigned long)xbuff->ptr_cfg_db_buff)[cfg_db_base_addr + lut_loop] = mt_platform_pci_read32(ptr_lut->cfg_db_poll[lut_loop]);
        // printk(KERN_INFO "DEBUG: cfg_db_poll val[%lu] (cfg_db address 0x%lx) = %u\n", cfg_db_base_addr+lut_loop, ptr_lut->cfg_db_poll[lut_loop], ((u32 *)(unsigned long)xbuff->ptr_cfg_db_buff)[cfg_db_base_addr + lut_loop]);
    }
#endif // DO_ANDROID

    /* Get the SOCPERF counter values */
    if (NULL != ptr_lut->soc_perf_poll) {
        if (ptr_lut->soc_perf_poll[0].operation == READ_OP) {
            //int i = 0;
            u64 __soc_perf_buffer[PW_NUM_SOC_COUNTERS];

            memset(__soc_perf_buffer, 0, sizeof(__soc_perf_buffer));

            SOCPERF_Read_Data(__soc_perf_buffer);

            memcpy(&((struct soc_perf_buffer *)(unsigned long)xbuff->ptr_soc_perf_buff)[0], __soc_perf_buffer, sizeof(__soc_perf_buffer));
        } else {
            dev_dbg(matrix_device, "SOCPERF operation is NOT read!\n");
        }
    } else {
        dev_dbg(matrix_device, "SOCPERF poll is NULL!\n");
    }

    if (mt_produce_mt_msg(mt_msg_poll_buff, tsc)) {
        printk(KERN_INFO "ERROR producing a POLL MT_MSG!\n");
        goto MT_MSG_POLL_ERROR;
    }

    // printk(KERN_INFO "OK: POLL MT_MSG scan was SUCCESSFUL!\n");
    return MT_SUCCESS;

MT_MSG_POLL_ERROR:
    printk(KERN_INFO "ERROR doing a POLL MT_MSG scan!\n");
    mt_free_memory();
    return -MT_ERROR;
};

#define MATRIX_VMALLOC(ptr, size, label) \
    do { \
        if (size > 0) { \
            ptr = vmalloc(size); \
            if (ptr == NULL) { \
                dev_dbg(matrix_device, "file : %s line %i\n", \
                        __FILE__, __LINE__); \
                goto label; \
            } \
        } \
    } while (0)

static int matrix_open(struct inode *in, struct file *filp)
{
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;
	if (instantiated) {
		module_put(THIS_MODULE);
		return -EBUSY;
	} else {
		instantiated = true;
		return 0;
	}
}

/**
 * platform_pci_read32 - for reading PCI space through config registers
 * of the platform.
 * @address : an address in the pci space
 */
static u32 mt_platform_pci_read32(u32 address)
{
	u32 read_value = 0;
#if DO_DIRECT_PCI_READ_WRITE
	struct pci_dev *pci_root = pci_get_bus_and_slot(0, PCI_DEVFN(0, 0));
	if (!pci_root)
		return 0; /* Application will verify the data */
	pci_write_config_dword(pci_root, MTX_PCI_MSG_CTRL_REG, address);
	pci_read_config_dword(pci_root, MTX_PCI_MSG_DATA_REG, &read_value);
#else // !DO_DIRECT_PCI_READ_WRITE
        read_value = intel_mid_msgbus_read32_raw(address);
#endif // if DO_DIRECT_PCI_READ_WRITE
	return read_value;
}

/**
 * platform_pci_write32 - for writing into PCI space through config registers
 * of the platform.
 * @address : an address in the pci space
 * @data : data that has to wrriten
 */
static void mt_platform_pci_write32(unsigned long address, unsigned long data)
{
#if DO_DIRECT_PCI_READ_WRITE
	struct pci_dev *pci_root = pci_get_bus_and_slot(0, PCI_DEVFN(0, 0));
	if (pci_root) {
		pci_write_config_dword(pci_root, MTX_PCI_MSG_DATA_REG, data);
		pci_write_config_dword(pci_root, MTX_PCI_MSG_CTRL_REG, address);
	}
#else // !DO_DIRECT_PCI_READ_WRITE
        intel_mid_msgbus_write32_raw(address, data);
#endif // if DO_DIRECT_PCI_READ_WRITE
}

/**
 * calculate_memory_requirements - determine the amount of memory required based * on data passed in from the user space
 */
static void mt_calculate_memory_requirements(void)
{
	lut_info.total_mem_bytes_req = 0;

	/* Find out memory required for Lookup table */
	MATRIX_INCREMENT_MEMORY(struct, mtx_msr, lut, msr, 1);
	MATRIX_INCREMENT_MEMORY(struct, memory_map, lut, mem, 1);
	MATRIX_INCREMENT_MEMORY(struct, mtx_pci_ops, lut, pci_ops, 1);
	MATRIX_INCREMENT_MEMORY(unsigned, long, lut, cfg_db, 1);
	// MATRIX_INCREMENT_MEMORY(unsigned, int, lut, cfg_db, 1);
	MATRIX_INCREMENT_MEMORY(struct, mtx_soc_perf, lut, soc_perf, 1);
	lut_info.poll_scu_drv_size =
	    ptr_lut->scu_poll.length * ptr_lut->scu_poll_length;
	lut_info.total_mem_bytes_req += lut_info.poll_scu_drv_size;

        // printk(KERN_INFO "GU: total LUT_INFO size = %u\n", lut_info.total_mem_bytes_req);
}

/**
 * bookmark_lookup_table - bookmark memory locations of structures within the
 * chunk of memory allocated earlier
 */
static int mt_bookmark_lookup_table(void)
{
	unsigned long offset = 0;

	/* msr part of the lookup table */
	MATRIX_BOOK_MARK_LUT(init, msr, struct, mtx_msr, msrs, 0, COPY_FAIL);
	MATRIX_BOOK_MARK_LUT(poll, msr, struct, mtx_msr, msrs, 0, COPY_FAIL);
	MATRIX_BOOK_MARK_LUT(term, msr, struct, mtx_msr, msrs, 0, COPY_FAIL);

	/* mem part of the lookup table */
	MATRIX_BOOK_MARK_LUT(init, mem, struct, memory_map, mmap, 1, COPY_FAIL);
	MATRIX_BOOK_MARK_LUT(poll, mem, struct, memory_map, mmap, 1, COPY_FAIL);
	MATRIX_BOOK_MARK_LUT(term, mem, struct, memory_map, mmap, 1, COPY_FAIL);

	/* pci part of the lookup table */
	MATRIX_BOOK_MARK_LUT(init, pci_ops, struct, mtx_pci_ops, pci_ops, 0,
			     COPY_FAIL);
	MATRIX_BOOK_MARK_LUT(poll, pci_ops, struct, mtx_pci_ops, pci_ops, 0,
			     COPY_FAIL);
	MATRIX_BOOK_MARK_LUT(term, pci_ops, struct, mtx_pci_ops, pci_ops, 0,
			     COPY_FAIL);

	/* config_db part of the lookup table */
	MATRIX_BOOK_MARK_LUT(init, cfg_db, unsigned, long, cfg_db, 0,
			     COPY_FAIL);
	MATRIX_BOOK_MARK_LUT(poll, cfg_db, unsigned, long, cfg_db, 0,
			     COPY_FAIL);
	MATRIX_BOOK_MARK_LUT(term, cfg_db, unsigned, long, cfg_db, 0,
			     COPY_FAIL);

	/* scu part of the lookup table */
	ptr_lut->scu_poll.drv_data = (unsigned char *)&ptr_lut_ops[offset];

	/* soc_perf part of the lookup table */
	MATRIX_BOOK_MARK_LUT(init, soc_perf, struct, mtx_soc_perf, soc_perf, 0,
			     COPY_FAIL);
	MATRIX_BOOK_MARK_LUT(poll, soc_perf, struct, mtx_soc_perf, soc_perf, 0,
			     COPY_FAIL);
	MATRIX_BOOK_MARK_LUT(term, soc_perf, struct, mtx_soc_perf, soc_perf, 0,
			     COPY_FAIL);
	return 0;
COPY_FAIL:
	return -EFAULT;
}

/**
 * free_memory - frees up all of the memory obtained
 */
static int mt_free_memory(void)
{
	/* Freeing IOREMAP Memory */
	if (ptr_lut) {
		MATRIX_IOUNMAP_MEMORY(init);
		MATRIX_IOUNMAP_MEMORY(term);
		MATRIX_IOUNMAP_MEMORY(poll);
		vfree(ptr_lut);
		ptr_lut = NULL;
	}
	/*Freeing LUT Memory */
        if (ptr_lut_ops) {
            vfree(ptr_lut_ops);
            ptr_lut_ops = NULL;
        }

	mem_alloc_status = false;

        mt_free_msg_memory();

	return 0;
}

/**
 * initialize_memory - initializes all of the required memory as requested
 * @ptr_data : gets the address of the lookup table that has all the info
 */
static int mt_initialize_memory(unsigned long ptr_data)
{

	if (mem_alloc_status) {
		dev_dbg(matrix_device,
			"Initialization of Memory is already done..\n");
                // printk(KERN_INFO "Initialization of Memory is already done..\n");
		return -EPERM;
	}

	/* get information about lookup table from user space */
	MATRIX_VMALLOC(ptr_lut, sizeof(struct lookup_table), ERROR);


	if (copy_from_user(ptr_lut,
			   (struct lookup_table *)ptr_data,
			   sizeof(struct lookup_table)) > 0) {
		dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		// printk(KERN_INFO "file : %s ,function : %s ,line %i\n", __FILE__, __func__, __LINE__);
		goto ERROR;
	}


        {
            // unsigned long init_length = TOTAL_ONE_SHOT_LENGTH(init);
            // unsigned long poll_length = TOTAL_ONE_SHOT_LENGTH(poll);
            // unsigned long term_length = TOTAL_ONE_SHOT_LENGTH(term);


            // printk(KERN_INFO "GU: init_length = %lu, poll_length = %lu, term_length = %lu\n", init_length, poll_length, term_length);
            // printk(KERN_INFO "GU: sizeof(xchange) = %u, init_len = %lu, poll_len = %lu, term_len = %lu\n", sizeof(struct mt_xchange_buffer), TOTAL_ONE_SHOT_LEN(init), TOTAL_ONE_SHOT_LEN(poll), TOTAL_ONE_SHOT_LEN(term));
            // printk(KERN_INFO "GU: # records = %u\n", ptr_lut->records);

            if (mt_init_msg_memory()) {
                printk(KERN_INFO "ERROR allocating memory for matrix messages!\n");
                goto ERROR;
            }
        }

	mt_calculate_memory_requirements();

	/* allocate once and for all memory required for lookup table */
	MATRIX_VMALLOC(ptr_lut_ops, lut_info.total_mem_bytes_req, ERROR);

	if (mt_bookmark_lookup_table() < 0) {
		dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		// printk(KERN_INFO "file : %s ,function : %s ,line %i\n", __FILE__, __func__, __LINE__);
		goto ERROR;
	}

	io_pm_status_reg =
	    (mt_platform_pci_read32(ptr_lut->pci_ops_poll->port) &
	     PWR_MGMT_BASE_ADDR_MASK);
	io_base_pwr_address =
	    (mt_platform_pci_read32(ptr_lut->pci_ops_poll->port_island) &
	     PWR_MGMT_BASE_ADDR_MASK);
	mem_alloc_status = true;

	return 0;
ERROR:
        printk(KERN_INFO "Memory Initialization Error!\n");
	mt_free_memory();
	return -EFAULT;
}
#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
    #define MT_MATCH_IOCTL(num, pred) ( (num) == (pred) || (num) == (pred##32) )
#else
    #define MT_MATCH_IOCTL(num, pred) ( (num) == (pred) )
#endif // COMPAT && x64

/**
 * initial_scan - function that is run once before polling at regular intervals
 * sets the msr's and other variables to their default values
 */
static int mt_msg_data_scan(unsigned long ioctl_request)
{
    // printk(KERN_INFO "MT_MSG_DATA_SCAN\n");
    if (ptr_lut == NULL) {
        printk(KERN_INFO "FATAL: NULL lookup table?!\n");
        goto ERROR;
    }
    if (MT_MATCH_IOCTL(ioctl_request, IOCTL_INIT_SCAN)) {
        if (mt_msg_init_scan()) {
            printk(KERN_INFO "ERROR doing an MT_MSG init scan!\n");
            goto ERROR;
        } else {
            // printk(KERN_INFO "OK: INIT MT_MSG scan was SUCCESSFUL!\n");
        }
    }
    else if (MT_MATCH_IOCTL(ioctl_request, IOCTL_TERM_SCAN)) {
        if (mt_msg_term_scan()) {
            printk(KERN_INFO "ERROR doing an MT_MSG term scan!\n");
            goto ERROR;
        } else {
            // printk(KERN_INFO "OK: TERM MT_MSG scan was SUCCESSFUL!\n");
        }
    }
    else {
        goto ERROR;
    }
    return MT_SUCCESS;
ERROR:
    mt_free_memory();
    return -MT_ERROR;
};


/**
 * transfer_data - transfers all the recorded info to user space for profiling
 * @ptr_data : gets the address of the user buffer that has to be populated
 */
static int mt_transfer_data(unsigned long ptr_data)
{
    return 0; // SUCCESS
}

/**
 * ioctl_mtx_msr - mtx_msr_container refers to structure designed to hold data related
 * to MSR( Model Specific Registers).
 * @ptr_data : gets the address of the user buffer that has to be populated
 */
static int IOCTL_mtx_msr(unsigned long ptr_data, unsigned int request)
{
	struct mtx_msr_container mtx_msr_drv;
	unsigned long *buffer = NULL;
	int err = 0;

	if ((struct mtx_msr_container *)ptr_data == NULL) {
		dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		return -EFAULT;
	}
	if (copy_from_user
	    (&mtx_msr_drv, (struct mtx_msr_container *)ptr_data,
	     sizeof(mtx_msr_drv)) > 0) {
		dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		return -EFAULT;
	}
	if (mtx_msr_drv.length > 0) {
		MATRIX_VMALLOC(buffer,
			       sizeof(unsigned long) * mtx_msr_drv.length,
			       ERROR);
		if (copy_from_user
		    (buffer, mtx_msr_drv.buffer,
		     (sizeof(unsigned long) * mtx_msr_drv.length)) > 0) {
			dev_dbg(matrix_device,
				"file : %s ,function : %s ,line %i\n", __FILE__,
				__func__, __LINE__);
			goto ERROR;
		}
	}
	switch (mtx_msr_drv.msrType1.operation) {
	case WRITE_OP:
		err = MATRIX_WRMSR_ON_CPU(mtx_msr_drv.msrType1.n_cpu,
				   mtx_msr_drv.msrType1.ecx_address,
				   mtx_msr_drv.msrType1.eax_LSB,
				   mtx_msr_drv.msrType1.edx_MSB);
		break;
	case READ_OP:
		err = MATRIX_RDMSR_ON_CPU(mtx_msr_drv.msrType1.n_cpu,
				   mtx_msr_drv.msrType1.ecx_address,
				   (u32 *) & mtx_msr_drv.msrType1.eax_LSB,
				   (u32 *) & mtx_msr_drv.msrType1.edx_MSB);
		break;
	case ENABLE_OP:
		wrmsrl(mtx_msr_drv.msrType1.ecx_address,
		       (unsigned long)&buffer[0]);
		wrmsr(mtx_msr_drv.msrType1.ebx_value, 0x01, 0x00);
		vfree(buffer);
		return 0;
	default:
		dev_dbg(matrix_device,
			"There is a problem in MSR Operation..\n");
		goto ERROR;
	}
	if (err != 0)
		goto ERROR;

#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
        if (request == IOCTL_MSR32) {
            struct mtx_msr_container32 __user *__msr32 = compat_ptr(ptr_data);
            // struct mtx_msr_container32 __user *__msr32 = (struct mtx_msr_container32 *)ptr_data;
            u32 data;
            // printk(KERN_INFO "SIZE = %u (%u)\n", sizeof(*__msr32), sizeof(__msr32->msrType1));
            data = (u32)mtx_msr_drv.length;
            if (put_user(data, &__msr32->length)) {
                goto ERROR;
            }
            data = (u32)mtx_msr_drv.msrType1.eax_LSB;
            if (put_user(data, &__msr32->msrType1.eax_LSB)) {
                goto ERROR;
            }
            // printk(KERN_INFO "eax_LSB = %u\n", data);
            data = (u32)mtx_msr_drv.msrType1.edx_MSB;
            if (put_user(data, &__msr32->msrType1.edx_MSB)) {
                goto ERROR;
            }
            // printk(KERN_INFO "edx_MSB = %u\n", data);
            data = (u32)mtx_msr_drv.msrType1.ecx_address;
            if (put_user(data, &__msr32->msrType1.ecx_address)) {
                goto ERROR;
            }
            data = (u32)mtx_msr_drv.msrType1.ebx_value;
            if (put_user(data, &__msr32->msrType1.ebx_value)) {
                goto ERROR;
            }
            data = (u32)mtx_msr_drv.msrType1.n_cpu;
            if (put_user(data, &__msr32->msrType1.n_cpu)) {
                goto ERROR;
            }
            data = (u32)mtx_msr_drv.msrType1.operation;
            if (put_user(data, &__msr32->msrType1.operation)) {
                goto ERROR;
            }
            // printk(KERN_INFO "OK: copied MSR32!\n");
            vfree(buffer);
            return 0;
        }
#endif // COMPAT && x64

	if (copy_to_user
	    ((struct mtx_msr_container *)ptr_data, &mtx_msr_drv,
	     sizeof(mtx_msr_drv)) > 0) {
		dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		goto ERROR;
	}
	vfree(buffer);
	return 0;
ERROR:
	vfree(buffer);
	return -EFAULT;
}

/**
 * ioctl_sram - memory map refers to a structure designed to hold data related
 * to SRAM (Shared RAM).
 * @ptr_data : gets the address of the user buffer that has to be populated
 */
static int IOCTL_sram(unsigned long ptr_data)
{
#if DO_ANDROID
	struct memory_map mem_map_drv;
	char *buffer = NULL;
	if ((struct memory_map *)ptr_data == NULL) {
		dev_dbg(matrix_device,
			"Data Transfer can not be done as user buffer is NULL..\n");
		return -EFAULT;
	}
	if (copy_from_user
	    (&mem_map_drv,
	     (struct memory_map *)ptr_data, sizeof(mem_map_drv)) > 0) {
		dev_dbg(matrix_device, "Transferring data had issues..\n");
		return -EFAULT;
	}
	if (mem_map_drv.ctrl_addr != 0) {
		void *remap_addr = ioremap_nocache
		    (mem_map_drv.ctrl_addr, sizeof(unsigned long));
		if (remap_addr == NULL) {
			dev_dbg(matrix_device, "IOREMAP has issue..\n");
			return -ENOMEM;
		}
		writel(mem_map_drv.ctrl_data, remap_addr);
		iounmap(remap_addr);
	} else {
		dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		return -EFAULT;
	}
	MATRIX_VMALLOC(buffer, mem_map_drv.data_size, ERROR);
	mem_map_drv.data_remap_address =
	    ioremap_nocache(mem_map_drv.data_addr, mem_map_drv.data_size);
	if (mem_map_drv.data_remap_address == NULL) {
		dev_dbg(matrix_device, "IOREMAP has issue..\n");
		goto ERROR;
	}
	memcpy(buffer, mem_map_drv.data_remap_address, mem_map_drv.data_size);
	if (copy_to_user
	    (mem_map_drv.ptr_data_usr, buffer, mem_map_drv.data_size) > 0) {
		dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		iounmap(mem_map_drv.data_remap_address);
		goto ERROR;
	}
	iounmap(mem_map_drv.data_remap_address);
	vfree(buffer);
        return 0;
ERROR:
	vfree(buffer);
	return -EFAULT;
#endif // DO_ANDROID
        return 0;
}

/**
 * read_config - procedure to read the config db registers (very generic)
 * @ptr_data : gets the address of the user buffer that has to be populated
 */
static int mt_read_config(unsigned long *ptr_data)
{
#if DO_ANDROID
	unsigned long buf, data;

	if (copy_from_user(&buf, (u32 *) ptr_data, sizeof(unsigned long)) > 0) {
		dev_err(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		return -EFAULT;
	}
	data = mt_platform_pci_read32(buf);
	/* Write back to the same user buffer */
	if (copy_to_user
	    ((unsigned long *)ptr_data, &data, sizeof(unsigned long)) > 0) {
		dev_err(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		return -EFAULT;
	}
#endif // DO_ANDROID
	return 0;
}

/**
 * write_config - proceduer to write the config db registers
 * @ptr_data : user buffer address that contains information like
 * mcr (port) and mdr (data) used for writing config DB registers.
 */
static inline int mt_write_config(unsigned long *ptr_data)
{
#if DO_ANDROID
	unsigned long addr, val;
	struct mtx_pci_ops pci_data;
	if (copy_from_user
	    (&pci_data,
	     (struct mtx_pci_ops *)ptr_data, sizeof(struct mtx_pci_ops)) > 0) {
		dev_err(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		return -EFAULT;
	}
	addr = pci_data.port | (MCR_WRITE_OPCODE << BIT_POS_OPCODE);
	val = pci_data.data;
	mt_platform_pci_write32(addr, val);
#endif // DO_ANDROID
	return 0;
}

/**
 * read_pci_config - procedure to read the pci configuration space
 * @ptr_data : gets the pci configuration info like bus, device,
 * function and the offset in the config space. Also, returns
 * the read data in "data" field of the structure
 */
static inline int mt_read_pci_config(unsigned long *ptr_data)
{
	int ret = 0;
#if DO_ANDROID
	struct pci_config pci_config_data;
	struct pci_dev *pdev = NULL;
	if (copy_from_user
	    (&pci_config_data,
	     (struct pci_config *)ptr_data, sizeof(struct pci_config)) > 0) {
		dev_err(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		return -EFAULT;
	}
	pdev = pci_get_bus_and_slot(pci_config_data.bus,
			PCI_DEVFN(pci_config_data.device,
				pci_config_data.function));
	if (!pdev) {
		ret = -EINVAL;
		goto exit;
	}
	ret = pci_read_config_dword(pdev, pci_config_data.offset,
			(u32 *)&pci_config_data.data);
	/* Write back to the same user buffer */
	if (copy_to_user
	    ((unsigned long *)ptr_data, &pci_config_data,
		 sizeof(struct pci_config)) > 0) {
		dev_err(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		return -EFAULT;
	}
#endif // DO_ANDROID
exit:
	return ret;
}

/**
 * read_gmch_gen_pur_regs -  use this function to retrieve the complete set of
 * general purpose gmch registers
 * @data : gets the address of the user buffer that has to be populated
 * @read_mask : read_mask applies mask corresponding to the platform
 */
static void mt_read_gmch_gen_pur_regs(unsigned long *data, unsigned long *clks,
				      unsigned long read_mask)
{
#if DO_ANDROID
	if (data && clks) {
		data[0] = mt_platform_pci_read32(MTX_GMCH_PMON_GP_CTR0_L | read_mask);
		data[1] = mt_platform_pci_read32(MTX_GMCH_PMON_GP_CTR0_H | read_mask);
		data[2] = mt_platform_pci_read32(MTX_GMCH_PMON_GP_CTR1_L | read_mask);
		data[3] = mt_platform_pci_read32(MTX_GMCH_PMON_GP_CTR1_H | read_mask);
		data[4] = mt_platform_pci_read32(MTX_GMCH_PMON_GP_CTR2_L | read_mask);
		data[5] = mt_platform_pci_read32(MTX_GMCH_PMON_GP_CTR2_H | read_mask);
		data[6] = mt_platform_pci_read32(MTX_GMCH_PMON_GP_CTR3_L | read_mask);
		data[7] = mt_platform_pci_read32(MTX_GMCH_PMON_GP_CTR3_H | read_mask);
		clks[0] = mt_platform_pci_read32(MTX_GMCH_PMON_FIXED_CTR0 | read_mask);
	}
#endif // DO_ANDROID
}

/**
 * gmch_gen_pur_regs_trigger_enable -  use this function to trigger global flag
 * that enables/disables gmch counters.
 * @enable : enable is boolean.
 * @write_mask : write_mask applies mask corresponding to the platform
 */
static void mt_gmch_gen_pur_regs_trigger_enable(bool enable,
						unsigned long write_mask)
{
#if DO_ANDROID
	if (enable)
		mt_platform_pci_write32((MTX_GMCH_PMON_GLOBAL_CTRL |
					 write_mask),
					MTX_GMCH_PMON_GLOBAL_CTRL_ENABLE);
	else
		mt_platform_pci_write32((MTX_GMCH_PMON_GLOBAL_CTRL |
					 write_mask),
					MTX_GMCH_PMON_GLOBAL_CTRL_DISABLE);
#endif // DO_ANDROID
}

/**
 * write_mt_gmch_gen_pur_regs -  use this function to write to the complete set of
 * general purpose gmch registers
 * @write_mask : write_mask applies mask corresponding to the platform
 */
static void mt_write_gmch_gen_pur_regs(unsigned long data,
				       unsigned long write_mask)
{
#if DO_ANDROID
	mt_platform_pci_write32((MTX_GMCH_PMON_GP_CTR0_L | write_mask), data);
	mt_platform_pci_write32((MTX_GMCH_PMON_GP_CTR0_H | write_mask), data);
	mt_platform_pci_write32((MTX_GMCH_PMON_GP_CTR1_L | write_mask), data);
	mt_platform_pci_write32((MTX_GMCH_PMON_GP_CTR1_H | write_mask), data);
	mt_platform_pci_write32((MTX_GMCH_PMON_GP_CTR2_L | write_mask), data);
	mt_platform_pci_write32((MTX_GMCH_PMON_GP_CTR2_H | write_mask), data);
	mt_platform_pci_write32((MTX_GMCH_PMON_GP_CTR3_L | write_mask), data);
	mt_platform_pci_write32((MTX_GMCH_PMON_GP_CTR3_H | write_mask), data);
	mt_platform_pci_write32((MTX_GMCH_PMON_FIXED_CTR_CTRL | write_mask),
				data);
	mt_platform_pci_write32((MTX_GMCH_PMON_FIXED_CTR_CTRL | write_mask),
				DATA_ENABLE);
#endif // DO_ANDROID
}

/**
 * mt_reset_gmch_gen_pur_regs -  use this function to reset all of the gmch
 * peformance counters
 * @event : event points to the first of the event passed in from the
 * application space.
 * @mcr1 : config register 1 for perf event selection
 * @mcr2 : config register 2 for perf event selection
 * @mcr3 : config register 3 if ,for perf event selection depends on platform
 */
static void mt_reset_gmch_gen_pur_regs(unsigned long
				       *event, unsigned long
				       *mcr1, unsigned long
				       *mcr2, unsigned long
				       *mcr3, unsigned long write_mask)
{
	unsigned long count = 0;
#if DO_ANDROID
	if (event == NULL || mcr1 == NULL || mcr2 == NULL || mcr3 == NULL)
		return;

	/*disable  gmch general purpose counter */
	mt_gmch_gen_pur_regs_trigger_enable(false, write_mask);

	/*re-initialize gmch general purpose counter */
	mt_write_gmch_gen_pur_regs(0x00000000, write_mask);

	/*trigger performance counters */
	for (count = 0; count < 4; count++) {
		if (mcr1[count])
			mt_platform_pci_write32(mcr1[count], event[count]);
		if (mcr2[count])
			mt_platform_pci_write32(mcr2[count], event[count]);
		if (mcr3[count])
			mt_platform_pci_write32(mcr3[count], event[count]);
	}

	/*enable gmch general purpose counter */
	mt_gmch_gen_pur_regs_trigger_enable(true, write_mask);
#endif // DO_ANDROID
}

/**
 * ioctl_gmch - gmch_container refers to a struct designed to hold data related
 * to GMCH( The Greater Memeory Controller Hub, giving access to all Emons
 * @ptr_data : gets the address of the user buffer that has to be populated
 */
static int IOCTL_gmch(unsigned long ioctl_request, unsigned long ptr_data)
{
#if DO_ANDROID
	struct gmch_container gmch_drv;
	if ((struct gmch_container *)ptr_data == NULL) {
		dev_dbg(matrix_device,
			"Data Transfer can not be done as user buffer is NULL..\n");
		return -EFAULT;
	}
	if (copy_from_user
	    (&gmch_drv,
	     (struct gmch_container *)ptr_data,
	     sizeof(struct gmch_container)) > 0) {
		dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		return -EFAULT;
	}

	/* read gmch counters */
	mt_read_gmch_gen_pur_regs(gmch_drv.data, &gmch_drv.core_clks,
				  gmch_drv.read_mask);
	MATRIX_GET_TIME_STAMP(gmch_drv.time_stamp);

	/* reset gmch counters */
	if (ioctl_request == IOCTL_GMCH_RESET) {
		mt_reset_gmch_gen_pur_regs(gmch_drv.event,
					   gmch_drv.mcr1,
					   gmch_drv.mcr2,
					   gmch_drv.mcr3, gmch_drv.write_mask);
	}
	if (copy_to_user
	    ((struct gmch_container *)ptr_data,
	     &gmch_drv, sizeof(struct gmch_container)) > 0) {
		dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		return -EFAULT;
	}
#endif // DO_ANDROID
	return 0;
}

/*
 * The following function reads/writes to an MSR with
 * inputs given by the user. The two primary use cases of
 * this  function are: a) Request to read IA32_PERF_STATUS MSR from ring3.
 * b) Debugging from user space. There could be other users of this in the
 * future.
 */
static int mt_operate_on_msr(unsigned long ptr_data)
{
	struct mtx_msr data_msr;
	if (copy_from_user
	    (&data_msr, (struct mtx_msr *)ptr_data,
	     sizeof(struct mtx_msr)) > 0) {
		dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		return -EFAULT;
	}
	if (data_msr.operation == READ_OP)
		rdmsr(data_msr.ecx_address, data_msr.eax_LSB, data_msr.edx_MSB);
	else if (data_msr.operation == WRITE_OP)
		wrmsr(data_msr.ecx_address, data_msr.eax_LSB, data_msr.edx_MSB);
	else
		return -EFAULT;
	if (copy_to_user((struct mtx_msr *)ptr_data, &data_msr,
			 sizeof(struct mtx_msr)) > 0) {
		dev_dbg(matrix_device,
			"file : %s ,function : %s ,line %i\n",
			__FILE__, __func__, __LINE__);
		return -EFAULT;
	}
	return 0;
}

static long mt_get_scu_fw_version(u16 __user *remote_data)
{
    u16 local_data = pw_scu_fw_major_minor;
    if (put_user(local_data, remote_data)) {
        printk(KERN_INFO "ERROR transfering SCU F/W version to userspace!\n");
        return -EFAULT;
    }
    return 0; // SUCCESS
};

static long mt_get_soc_stepping(u32 __user *remote_data)
{
#ifdef CONFIG_X86_WANT_INTEL_MID
    {
        u32 local_data = intel_mid_soc_stepping();
        if (put_user(local_data, remote_data)) {
            printk(KERN_INFO "ERROR transfering soc stepping number to userspace!\n");
            return -EFAULT;
        }
        return 0; // SUCCESS
    }
#endif // CONFIG_X86_WANT_INTEL_MID
    return -EFAULT;
};

static long mt_get_version(u32 __user *remote_data)
{
    /*
     * Protocol:
     * Driver version info is:
     * [Internal/External] << 24 | [Major Num] << 16 | [Minor Num] << 8 | [Other Num]
     * Internal/External is: 1 ==> INTERNAL, 0 ==> EXTERNAL
     */
    u32 local_data = (0 /*External*/ << 24) | ((u8)PW_DRV_VERSION_MAJOR) << 16 | ((u8)PW_DRV_VERSION_MINOR) << 8 | (u8)PW_DRV_VERSION_OTHER;
    if (put_user(local_data, remote_data)) {
        printk(KERN_INFO "ERROR transfering driver version information to userspace!\n");
        return -EFAULT;
    }
    return 0; // SUCCESS
};

/*
 * GU: Use this version of the function for now, for debugging
 */
static long matrix_ioctl(struct file
			 *filp, unsigned int request, unsigned long ptr_data)
{
    // printk(KERN_INFO "Received MATRIX IOCTL: %u\n", request);
    switch (request) {
        case IOCTL_VERSION_INFO:
#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
        case IOCTL_VERSION_INFO32:
#endif // COMPAT && x64
            /*
             * UNSUPPORTED
             * Use 'IOCTL_GET_DRIVER_VERSION/IOCTL_GET_DRIVER_VERSION32' instead.
             */
            printk(KERN_INFO "ERROR: INVALID ioctl num %u received!\n", request);
            return -PW_ERROR;
            break;
        case IOCTL_INIT_MEMORY:
#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
        case IOCTL_INIT_MEMORY32:
#endif // COMPAT && x64
            // printk(KERN_INFO "IOCTL_INIT_MEMORY received!\n");
            return mt_initialize_memory(ptr_data);
        case IOCTL_FREE_MEMORY:
            // printk(KERN_INFO "IOCTL_FREE_MEMORY received!\n");
            return mt_free_memory();
        case IOCTL_OPERATE_ON_MSR:
            // printk(KERN_INFO "IOCTL_OPERATE_ON_MSR received!\n");
            return mt_operate_on_msr(ptr_data);
        case IOCTL_INIT_SCAN:
        case IOCTL_TERM_SCAN:
#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
        case IOCTL_INIT_SCAN32:
        case IOCTL_TERM_SCAN32:
#endif // COMPAT && x64
            // printk(KERN_INFO "IOCTL_{INIT,TERM}_SCAN received!\n");
            return mt_msg_data_scan(request);
        case IOCTL_POLL_SCAN:
#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
        case IOCTL_POLL_SCAN32:
#endif // COMPAT && x64
            // printk(KERN_INFO "IOCTL_POLL_SCAN received!\n");
            // return mt_poll_scan(ptr_data);
            return mt_msg_poll_scan(ptr_data);
        case IOCTL_COPY_TO_USER:
            // printk(KERN_INFO "IOCTL_COPY_TO_USER received!\n");
            return mt_transfer_data(ptr_data);
            /* MSR based ioctl's */
        case IOCTL_MSR:
            // printk(KERN_INFO "IOCTL_MSR received!\n");
            return IOCTL_mtx_msr(ptr_data, request);
            /* SRAM based ioctl's */
        case IOCTL_SRAM:
            // printk(KERN_INFO "IOCTL_SRAM received!\n");
            return IOCTL_sram(ptr_data);
            // return -1;
            /* GMCH based ioctl's */
        case IOCTL_GMCH:
            // printk(KERN_INFO "IOCTL_GMCH received!\n");
            return IOCTL_gmch(request, ptr_data);
        case IOCTL_GMCH_RESET:
            // printk(KERN_INFO "IOCTL_GMCH_REQUEST received!\n");
            return IOCTL_gmch(request, ptr_data);
        case IOCTL_READ_CONFIG_DB:
            // printk(KERN_INFO "IOCTL_READ_CONFIG_DB received!\n");
            return mt_read_config((unsigned long *)ptr_data);
            // return -1;
        case IOCTL_WRITE_CONFIG_DB:
            // printk(KERN_INFO "IOCTL_WRITE_CONFIG_DB received!\n");
            return mt_write_config((unsigned long *)ptr_data);
            // return -1;
        case IOCTL_READ_PCI_CONFIG:
            return mt_read_pci_config((unsigned long *)ptr_data);
        case IOCTL_GET_SOC_STEPPING:
#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
        case IOCTL_GET_SOC_STEPPING32:
#endif // compat && x64
            if (!ptr_data) {
                printk(KERN_INFO "NULL ptr_data in matrix_ioctl IOCTL_GET_SOC_STEPPING\n");
                return -EFAULT;
            }
            return mt_get_soc_stepping((u32 *)ptr_data);
        case IOCTL_GET_SCU_FW_VERSION:
#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
        case IOCTL_GET_SCU_FW_VERSION32:
#endif // compat && x64
            if (!ptr_data) {
                printk(KERN_INFO "NULL ptr_data in matrix_ioctl IOCTL_GET_SCU_FW_VERSION\n");
                return -EFAULT;
            }
            return mt_get_scu_fw_version((u16 __user *)ptr_data);
        case IOCTL_GET_DRIVER_VERSION:
#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
        case IOCTL_GET_DRIVER_VERSION32:
#endif // compat && x64
            return mt_get_version((u32 *)ptr_data);
        default:
            printk(KERN_INFO "INVALID IOCTL = %u received!\n", request);
            dev_dbg(matrix_device,
                    "file : %s ,function : %s ,line %i\n", __FILE__,
                    __func__, __LINE__);
            return -EFAULT;
    }
    return 0;
}

static int matrix_release(struct inode *in, struct file *filp)
{
	if (instantiated) {
		mt_free_memory();
		instantiated = false;
	}
	module_put(THIS_MODULE);
	return 0;
}

static const struct file_operations matrix_fops = {
	.owner = THIS_MODULE,
	.open = matrix_open,
	.unlocked_ioctl = matrix_ioctl,
#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
        .compat_ioctl = &mt_device_compat_ioctl,
#endif // COMPAT && x64
	.release = matrix_release
};

/*
 * Compat IOCTL support.
 */
#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)

#if 1
static int mt_copy_mtx_msr_info_i(struct mtx_msr *local_msr, const struct mtx_msr32 __user *remote_msr32, u32 length)
{
    int retVal = PW_SUCCESS;
    unsigned long i=0;
    // u32 data;

    for (i=0; i<length; ++i) {
        if (get_user(local_msr[i].eax_LSB, &remote_msr32[i].eax_LSB)) {
            return -EFAULT;
        }
        if (get_user(local_msr[i].edx_MSB, &remote_msr32[i].edx_MSB)) {
            return -EFAULT;
        }
        if (get_user(local_msr[i].ecx_address, &remote_msr32[i].ecx_address)) {
            return -EFAULT;
        }
        if (get_user(local_msr[i].ebx_value, &remote_msr32[i].ebx_value)) {
            return -EFAULT;
        }
        if (get_user(local_msr[i].n_cpu, &remote_msr32[i].n_cpu)) {
            return -EFAULT;
        }
        if (get_user(local_msr[i].operation, &remote_msr32[i].operation)) {
            return -EFAULT;
        }
        // printk(KERN_INFO "DEBUG: addr[%lu] = 0x%lx, cpu = %lu, operation = %lu\n", i, local_msr[i].ecx_address, local_msr[i].n_cpu, local_msr[i].operation);
    }
    return retVal;
};
#endif // if 0

#if 1
static int mt_copy_mmap_info_i(struct memory_map *local_mem, const struct memory_map32 __user *remote_mem32, unsigned long length)
{
    unsigned long i=0;
    int retVal = PW_SUCCESS;
    // u32 data;

    for (i=0; i<length; ++i) {
        if (get_user(local_mem[i].ctrl_addr, &remote_mem32[i].ctrl_addr)) {
            return -EFAULT;
        }
        /*
         * 'ctrl_remap_address' is never passed in from user space.
         * Set to NULL explicitly.
         */
        /*
        if (copy_from_user(&local_mem[i].ctrl_remap_address, &remote_remote_mem32[i].ctrl_remap_address, sizeof(remote_remote_mem32[i].ctrl_remap_address))) {
            return -EFAULT;
        }
        */
        local_mem[i].ctrl_remap_address = NULL;
        if (get_user(local_mem[i].ctrl_data, &remote_mem32[i].ctrl_data)) {
            return -EFAULT;
        }
        if (get_user(local_mem[i].data_addr, &remote_mem32[i].data_addr)) {
            return -EFAULT;
        }
        /*
         * 'data_remap_address' is never passed in from user space.
         * Set to NULL explicitly.
         */
        local_mem[i].data_remap_address = NULL;
        /*
        if (get_user(local_mem[i].data_remap_address, compat_ptr(remote_mem32[i].data_remap_address))) {
            return -EFAULT;
        }
        */
        /*
         * 'ptr_data_usr' is only used in 'IOCTL_sram', which doesn't utilize
         * the lookup table.
         * Set to NULL explicitly.
         */
        /*
        if (get_user(local_mem[i].ptr_data_usr, &remote_mem32[i].ptr_data_usr)) {
            return -EFAULT;
        }
        */
        local_mem[i].ptr_data_usr = NULL;
        if (get_user(local_mem[i].data_size, &remote_mem32[i].data_size)) {
            return -EFAULT;
        }
        if (get_user(local_mem[i].operation, &remote_mem32[i].operation)) {
            return -EFAULT;
        }
        // printk(KERN_INFO "DEBUG: [%lu]: ctrl_addr = %lu, ctrl_data = %lu, data_addr = %lu, data_size = %lu, operation = %lu\n", i, local_mem[i].ctrl_addr, local_mem[i].ctrl_data, local_mem[i].data_addr, local_mem[i].data_size, local_mem[i].operation);
    }
    return retVal;
};
#endif // if 0

#if 1
static int mt_copy_pci_info_i(struct mtx_pci_ops *local_pci, const struct mtx_pci_ops32 __user *remote_pci32, unsigned long length)
{
    unsigned long i=0;

    for (i=0; i<length; ++i) {
        if (get_user(local_pci[i].port, &remote_pci32[i].port)) {
            return -EFAULT;
        }
        if (get_user(local_pci[i].data, &remote_pci32[i].data)) {
            return -EFAULT;
        }
        if (get_user(local_pci[i].io_type, &remote_pci32[i].io_type)) {
            return -EFAULT;
        }
        if (get_user(local_pci[i].port_island, &remote_pci32[i].port_island)) {
            return -EFAULT;
        }
        // printk(KERN_INFO "DEBUG: pci port = %lu, data = %lu, type = %lu, port_island = %lu\n", local_pci[i].port, local_pci[i].data, local_pci[i].io_type, local_pci[i].port_island);
    }
    return PW_SUCCESS;
};
#endif // if 0

#if 1
static int mt_copy_cfg_db_info_i(unsigned long *local_cfg, const u32 __user *remote_cfg32, unsigned long length)
{
    unsigned long i=0;

    for (i=0; i<length; ++i) {
        if (get_user(local_cfg[i], &remote_cfg32[i])) {
            return -EFAULT;
        }
        // printk(KERN_INFO "DEBUG: local_cfg[%lu] = %lu\n", i, local_cfg[i]);
    }
    return PW_SUCCESS;
};
#endif // if 0

#if 1
static int mt_copy_soc_perf_info_i(struct mtx_soc_perf *local_soc_perf, const struct mtx_soc_perf32 __user *remote_soc_perf32, unsigned long length)
{
    unsigned long i=0;

    for (i=0; i<length; ++i) {
        /*
         * 'ptr_data_usr' is never used.
         * Set to NULL explicitly.
         */
        local_soc_perf[i].ptr_data_usr = NULL;
        if (get_user(local_soc_perf[i].data_size, &remote_soc_perf32[i].data_size)) {
            return -EFAULT;
        }
        if (get_user(local_soc_perf[i].operation, &remote_soc_perf32[i].operation)) {
            return -EFAULT;
        }
    }
    return PW_SUCCESS;
};
#endif // if 0

static long mt_device_compat_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
    switch (ioctl_num) {
        case IOCTL_INIT_MEMORY32:
            // printk(KERN_INFO "IOCTL_INIT_MEMORY32\n");
            return mt_device_compat_init_ioctl_i(file, ioctl_num, ioctl_param);
        case IOCTL_MSR32:
            // printk(KERN_INFO "IOCTL_MSR32\n");
            return mt_device_compat_msr_ioctl_i(file, ioctl_num, ioctl_param);
        case IOCTL_READ_CONFIG_DB32:
            // printk(KERN_INFO "IOCTL_READ_CONFIG_DB32\n");
            return mt_device_compat_config_db_ioctl_i(file, ioctl_num, ioctl_param);
        case IOCTL_READ_PCI_CONFIG32:
            // printk(KERN_INFO "IOCTL_READ_PCI_CONFIG32\n");
            return mt_device_compat_pci_config_ioctl_i(file, ioctl_num, ioctl_param);
        case IOCTL_GET_SCU_FW_VERSION32:
            // printk(KERN_INFO "IOCTL_GET_SCU_FW_VERSION32\n");
            return mt_get_scu_fw_version_compat_i(compat_ptr(ioctl_param));
        default:
            // printk(KERN_INFO "OTHER\n");
            break;
    }
    return matrix_ioctl(file, ioctl_num, ioctl_param);
};

#if 1
static long mt_device_compat_init_ioctl_i(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
    struct lookup_table32 __user *__tab32 = compat_ptr(ioctl_param);
    // struct lookup_table __user *__tab = NULL;
    // u8 *__buffer = NULL;
    // u32 data = 0;
    struct lookup_table32 __tmp;
    // struct lookup_table *ptr_lut = NULL;
    // struct lookup_table __user *remote_tab = NULL;
    size_t __size = 0;
    long retVal = PW_SUCCESS;

    /*
     * Basic algo:
     * 1. Calculate total memory requirement for the 64b structure (including space required for all arrays etc.)
     * 2. Allocate entire lookup table chunk in 'compat' space (via 'compat_alloc_user_space()').
     * 3. Patch up pointers (individual array pointers in the lookup table need to point into the chunk allocated in 2, above).
     * 4. Copy over the "header" (all of the non-pointer fields) from 32b --> 64b
     * 5. For each pointer
     * a. Manually copy all fields from 32b userspace struct to 64b userspace struct.
     */

    if (copy_from_user(&__tmp, __tab32, sizeof(__tmp))) {
        printk(KERN_INFO "ERROR in length user copy!\n");
        return -EFAULT;
    }
    ptr_lut = vmalloc(sizeof(*ptr_lut));
    if (!ptr_lut) {
        printk(KERN_INFO "ERROR allocating ptr_lut\n");
        return -EFAULT;
    }
    memset(ptr_lut, 0, sizeof(*ptr_lut));
    /*
    if (true) {
        printk(KERN_INFO "DEBUG: setting pci_ops length to zero!\n");
        __tmp.pci_ops_init_length = __tmp.pci_ops_poll_length = __tmp.pci_ops_term_length = 0;
    }
    */
    /*
     * Step 1.
     */
    {
        __size += (size_t)__tmp.msr_init_length * sizeof(struct mtx_msr);
        __size += (size_t)__tmp.msr_poll_length * sizeof(struct mtx_msr);
        __size += (size_t)__tmp.msr_term_length * sizeof(struct mtx_msr);

        // printk(KERN_INFO "msr_init_length = %u\n", __tmp.msr_init_length);
        // printk(KERN_INFO "msr_poll_length = %u\n", __tmp.msr_poll_length);
        // printk(KERN_INFO "msr_term_length = %u\n", __tmp.msr_term_length);

        __size += (size_t)__tmp.mem_init_length * sizeof(struct memory_map);
        __size += (size_t)__tmp.mem_poll_length * sizeof(struct memory_map);
        __size += (size_t)__tmp.mem_term_length * sizeof(struct memory_map);

        // printk(KERN_INFO "mem_init_length = %u\n", __tmp.mem_init_length);
        // printk(KERN_INFO "mem_poll_length = %u\n", __tmp.mem_poll_length);
        // printk(KERN_INFO "mem_term_length = %u\n", __tmp.mem_term_length);

        __size += (size_t)__tmp.pci_ops_init_length * sizeof(struct mtx_pci_ops);
        __size += (size_t)__tmp.pci_ops_poll_length * sizeof(struct mtx_pci_ops);
        __size += (size_t)__tmp.pci_ops_term_length * sizeof(struct mtx_pci_ops);

        // printk(KERN_INFO "pci_ops_init_length = %u\n", __tmp.pci_ops_init_length);
        // printk(KERN_INFO "pci_ops_poll_length = %u\n", __tmp.pci_ops_poll_length);
        // printk(KERN_INFO "pci_ops_term_length = %u\n", __tmp.pci_ops_term_length);

        __size += (size_t)__tmp.cfg_db_init_length * sizeof(unsigned long);
        __size += (size_t)__tmp.cfg_db_poll_length * sizeof(unsigned long);
        __size += (size_t)__tmp.cfg_db_term_length * sizeof(unsigned long);

        // printk(KERN_INFO "cfg_db_init_length = %u\n", __tmp.cfg_db_init_length);
        // printk(KERN_INFO "cfg_db_poll_length = %u\n", __tmp.cfg_db_poll_length);
        // printk(KERN_INFO "cfg_db_term_length = %u\n", __tmp.cfg_db_term_length);

        __size += (size_t)__tmp.soc_perf_init_length * sizeof(struct mtx_soc_perf);
        __size += (size_t)__tmp.soc_perf_poll_length * sizeof(struct mtx_soc_perf);
        __size += (size_t)__tmp.soc_perf_term_length * sizeof(struct mtx_soc_perf);

        // printk(KERN_INFO "soc_perf_init_length = %u\n", __tmp.soc_perf_init_length);
        // printk(KERN_INFO "soc_perf_poll_length = %u\n", __tmp.soc_perf_poll_length);
        // printk(KERN_INFO "soc_perf_term_length = %u\n", __tmp.soc_perf_term_length);

        // __size += sizeof(struct lookup_table);
    }

    // printk(KERN_INFO "SIZE = %lu\n", __size);
    /*
     * Step 2.
     */
    {
        ptr_lut_ops = vmalloc(__size);
        if (!ptr_lut_ops) {
            printk(KERN_INFO "ERROR allocating space for lookup_table\n");
            goto error;
        }
    }
    /*
     * Step 3.
     */
    {
        int __dst_idx = 0;
        // ptr_lut = (struct lookup_table *)&__buffer[__dst_idx]; __dst_idx += sizeof(*ptr_lut);

        ptr_lut->msrs_init = (struct mtx_msr *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.msr_init_length * sizeof(*ptr_lut->msrs_init);
        ptr_lut->mmap_init = (struct memory_map *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.mem_init_length * sizeof(*ptr_lut->mmap_init);
        ptr_lut->pci_ops_init = (struct mtx_pci_ops *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.pci_ops_init_length * sizeof(*ptr_lut->pci_ops_init);
        ptr_lut->cfg_db_init = (unsigned long *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.cfg_db_init_length * sizeof(*ptr_lut->cfg_db_init);
        ptr_lut->soc_perf_init = (struct mtx_soc_perf *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.soc_perf_init_length * sizeof(*ptr_lut->soc_perf_init);

        ptr_lut->msrs_poll = (struct mtx_msr *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.msr_poll_length * sizeof(*ptr_lut->msrs_poll);
        ptr_lut->mmap_poll = (struct memory_map *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.mem_poll_length * sizeof(*ptr_lut->mmap_poll);
        ptr_lut->pci_ops_poll = (struct mtx_pci_ops *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.pci_ops_poll_length * sizeof(*ptr_lut->pci_ops_poll);
        ptr_lut->cfg_db_poll = (unsigned long *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.cfg_db_poll_length * sizeof(*ptr_lut->cfg_db_poll);
        ptr_lut->soc_perf_poll = (struct mtx_soc_perf *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.soc_perf_poll_length * sizeof(*ptr_lut->soc_perf_poll);

        ptr_lut->msrs_term = (struct mtx_msr *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.msr_term_length * sizeof(*ptr_lut->msrs_term);
        ptr_lut->mmap_term = (struct memory_map *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.mem_term_length * sizeof(*ptr_lut->mmap_term);
        ptr_lut->pci_ops_term = (struct mtx_pci_ops *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.pci_ops_term_length * sizeof(*ptr_lut->pci_ops_term);
        ptr_lut->cfg_db_term = (unsigned long *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.cfg_db_term_length * sizeof(*ptr_lut->cfg_db_term);
        ptr_lut->soc_perf_term = (struct mtx_soc_perf *)&ptr_lut_ops[__dst_idx]; __dst_idx += __tmp.soc_perf_term_length * sizeof(*ptr_lut->soc_perf_term);
    }
    /*
     * Step 4.
     */
    {
        /*
         * INIT
         */
        {
            ptr_lut->msr_init_length = __tmp.msr_init_length;
            ptr_lut->msr_init_wb = __tmp.msr_init_wb;
            ptr_lut->mem_init_length = __tmp.mem_init_length;
            ptr_lut->mem_init_wb = __tmp.mem_init_wb;
            ptr_lut->pci_ops_init_length = __tmp.pci_ops_init_length;
            ptr_lut->pci_ops_init_wb = __tmp.pci_ops_init_wb;
            ptr_lut->cfg_db_init_length = __tmp.cfg_db_init_length;
            ptr_lut->cfg_db_init_wb = __tmp.cfg_db_init_wb;
            ptr_lut->soc_perf_init_length = __tmp.soc_perf_init_length;
            ptr_lut->soc_perf_init_wb = __tmp.soc_perf_init_wb;
        }
        /*
         * POLL
         */
        {
            ptr_lut->msr_poll_length = __tmp.msr_poll_length;
            ptr_lut->msr_poll_wb = __tmp.msr_poll_wb;
            ptr_lut->mem_poll_length = __tmp.mem_poll_length;
            ptr_lut->mem_poll_wb = __tmp.mem_poll_wb;
            ptr_lut->records = __tmp.records;
            ptr_lut->pci_ops_poll_length = __tmp.pci_ops_poll_length;
            ptr_lut->pci_ops_poll_wb = __tmp.pci_ops_poll_wb;
            ptr_lut->pci_ops_records = __tmp.pci_ops_records;
            ptr_lut->cfg_db_poll_length = __tmp.cfg_db_poll_length;
            ptr_lut->cfg_db_poll_wb = __tmp.cfg_db_poll_wb;
            /*
             * TODO
             * 'scu_poll'
             */
            {
                ptr_lut->scu_poll_length = 0; // __tmp.scu_poll_length;
                ptr_lut->scu_poll.address = NULL;
                ptr_lut->scu_poll.usr_data = NULL;
                ptr_lut->scu_poll.drv_data = NULL;
                ptr_lut->scu_poll.length = 0;
            }
            ptr_lut->soc_perf_poll_length = __tmp.soc_perf_poll_length;
            ptr_lut->soc_perf_poll_wb = __tmp.soc_perf_poll_wb;
            ptr_lut->soc_perf_records = __tmp.soc_perf_records;
        }
        /*
         * TERM
         */
        {
            ptr_lut->msr_term_length = __tmp.msr_term_length;
            ptr_lut->msr_term_wb = __tmp.msr_term_wb;
            ptr_lut->mem_term_length = __tmp.mem_term_length;
            ptr_lut->mem_term_wb = __tmp.mem_term_wb;
            ptr_lut->pci_ops_term_length = __tmp.pci_ops_term_length;
            ptr_lut->pci_ops_term_wb = __tmp.pci_ops_term_wb;
            ptr_lut->cfg_db_term_length = __tmp.cfg_db_term_length;
            ptr_lut->cfg_db_term_wb = __tmp.cfg_db_term_wb;
            ptr_lut->soc_perf_term_length = __tmp.soc_perf_term_length;
            ptr_lut->soc_perf_term_wb = __tmp.soc_perf_term_wb;
        }
    }
    /*
     * Step 5.
     */
    {
        /*
         * INIT
         */
        {
            if (mt_copy_mtx_msr_info_i(ptr_lut->msrs_init, (struct mtx_msr32 __user *)compat_ptr(__tmp.msrs_init), __tmp.msr_init_length)) {
                printk(KERN_INFO "ERROR copying init msrs!\n");
                retVal = -EFAULT;
                goto error;
            }
            if (mt_copy_mmap_info_i(ptr_lut->mmap_init, (struct memory_map32 __user *)compat_ptr(__tmp.mmap_init), __tmp.mem_init_length)) {
                printk(KERN_INFO "ERROR copying init mmap!\n");
                retVal = -EFAULT;
                goto error;
            }
            if (mt_copy_pci_info_i(ptr_lut->pci_ops_init, (struct mtx_pci_ops32 __user *)compat_ptr(__tmp.pci_ops_init), __tmp.pci_ops_init_length)) {
                printk(KERN_INFO "ERROR copying init pci!\n");
                retVal = -EFAULT;
                goto error;
            }
            if (mt_copy_cfg_db_info_i(ptr_lut->cfg_db_init, (u32 __user *)compat_ptr(__tmp.cfg_db_init), __tmp.cfg_db_init_length)) {
                printk(KERN_INFO "ERROR copying init cfg_db!\n");
                retVal = -EFAULT; 
                goto error;
            }
            if (mt_copy_soc_perf_info_i(ptr_lut->soc_perf_init, (struct mtx_soc_perf32 __user *)compat_ptr(__tmp.soc_perf_init), __tmp.soc_perf_init_length)) {
                printk(KERN_INFO "ERROR copying term soc_perf!\n");
                retVal = -EFAULT; 
                goto error;
            }
        }
        /*
         * POLL 
         */
        {
            if (mt_copy_mtx_msr_info_i(ptr_lut->msrs_poll, (struct mtx_msr32 __user *)compat_ptr(__tmp.msrs_poll), __tmp.msr_poll_length)) {
                printk(KERN_INFO "ERROR copying poll msrs!\n");
                retVal = -EFAULT;
                goto error;
            }
            if (mt_copy_mmap_info_i(ptr_lut->mmap_poll, (struct memory_map32 __user *)compat_ptr(__tmp.mmap_poll), __tmp.mem_poll_length)) {
                printk(KERN_INFO "ERROR copying poll mmap!\n");
                retVal = -EFAULT;
                goto error;
            }
            if (mt_copy_pci_info_i(ptr_lut->pci_ops_poll, (struct mtx_pci_ops32 __user *)compat_ptr(__tmp.pci_ops_poll), __tmp.pci_ops_poll_length)) {
                printk(KERN_INFO "ERROR copying poll pci!\n");
                retVal = -EFAULT; 
                goto error;
            }
            if (mt_copy_cfg_db_info_i(ptr_lut->cfg_db_poll, (u32 __user *)compat_ptr(__tmp.cfg_db_poll), __tmp.cfg_db_poll_length)) {
                printk(KERN_INFO "ERROR copying poll cfg_db!\n");
                retVal = -EFAULT;
                goto error;
            }
            if (mt_copy_soc_perf_info_i(ptr_lut->soc_perf_poll, (struct mtx_soc_perf32 __user *)compat_ptr(__tmp.soc_perf_poll), __tmp.soc_perf_poll_length)) {
                printk(KERN_INFO "ERROR copying term soc_perf!\n");
                retVal = -EFAULT;
                goto error;
            }
        }
        /*
         * TERM
         */
        {
            if (mt_copy_mtx_msr_info_i(ptr_lut->msrs_term, (struct mtx_msr32 __user *)compat_ptr(__tmp.msrs_term), __tmp.msr_term_length)) {
                printk(KERN_INFO "ERROR copying term msrs!\n");
                retVal = -EFAULT;
                goto error;
            }
            if (mt_copy_mmap_info_i(ptr_lut->mmap_term, (struct memory_map32 __user *)compat_ptr(__tmp.mmap_term), __tmp.mem_term_length)) {
                printk(KERN_INFO "ERROR copying term mmap!\n");
                retVal = -EFAULT;
                goto error;
            }
            if (mt_copy_pci_info_i(ptr_lut->pci_ops_term, (struct mtx_pci_ops32 __user *)compat_ptr(__tmp.pci_ops_term), __tmp.pci_ops_term_length)) {
                printk(KERN_INFO "ERROR copying term pci!\n");
                retVal = -EFAULT;
                goto error;
            }
            if (mt_copy_cfg_db_info_i(ptr_lut->cfg_db_term, (u32 __user *)compat_ptr(__tmp.cfg_db_term), __tmp.cfg_db_term_length)) {
                printk(KERN_INFO "ERROR copying term cfg_db!\n");
                retVal = -EFAULT;
                goto error;
            }
            if (mt_copy_soc_perf_info_i(ptr_lut->soc_perf_term, (struct mtx_soc_perf32 __user *)compat_ptr(__tmp.soc_perf_term), __tmp.soc_perf_term_length)) {
                printk(KERN_INFO "ERROR copying term soc_perf!\n");
                retVal = -EFAULT;
                goto error;
            }
        }
    }

    if (mt_init_msg_memory()) {
        printk(KERN_INFO "ERROR allocating memory for matrix messages!\n");
        goto error;
    }
    mt_calculate_memory_requirements();

    io_pm_status_reg =
        (mt_platform_pci_read32(ptr_lut->pci_ops_poll->port) &
         PWR_MGMT_BASE_ADDR_MASK);
    io_base_pwr_address =
        (mt_platform_pci_read32(ptr_lut->pci_ops_poll->port_island) &
         PWR_MGMT_BASE_ADDR_MASK);
    mem_alloc_status = true;

    /*
     * io_remap
     */
    {
        MATRIX_IO_REMAP_MEMORY(init);
        MATRIX_IO_REMAP_MEMORY(poll);
        MATRIX_IO_REMAP_MEMORY(term);
    }

    return retVal;

error:
    printk(KERN_INFO "Memory Initialization Error!\n");
    mt_free_memory();
    return -EFAULT;
};
#endif // if 1

#if 1
static int mt_ioctl_mtx_msr_compat_i(struct mtx_msr_container __user *remote_args, struct mtx_msr_container32 __user *remote_args32)
{
    struct mtx_msr_container local_args;
    struct mtx_msr_container32 local_args32;
    unsigned long *buffer = NULL;
    int err = 0;

    if (copy_from_user(&local_args, remote_args, sizeof(local_args)) > 0) {
        dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n",
                __FILE__, __func__, __LINE__);
        return -EFAULT;
    }

    // printk(KERN_INFO "local.op = %lu, local.address = %lu\n", local_args.msrType1.operation, local_args.msrType1.ecx_address);

    if (local_args.length > 0) {
        MATRIX_VMALLOC(buffer, sizeof(unsigned long) * local_args.length, ERROR);
        if (copy_from_user(buffer, local_args.buffer, (sizeof(unsigned long) * local_args.length)) > 0) {
            dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n", __FILE__, __func__, __LINE__);
            goto ERROR;
        }
    }
    switch (local_args.msrType1.operation) {
        case WRITE_OP:
            err = MATRIX_WRMSR_ON_CPU(local_args.msrType1.n_cpu,
                    local_args.msrType1.ecx_address,
                    local_args.msrType1.eax_LSB,
                    local_args.msrType1.edx_MSB);
            break;
        case READ_OP:
            err = MATRIX_RDMSR_ON_CPU(local_args.msrType1.n_cpu,
                    local_args.msrType1.ecx_address,
                    (u32 *) & local_args.msrType1.eax_LSB,
                    (u32 *) & local_args.msrType1.edx_MSB);
            break;
        case ENABLE_OP:
            wrmsrl(local_args.msrType1.ecx_address,
                    (unsigned long)&buffer[0]);
            wrmsr(local_args.msrType1.ebx_value, 0x01, 0x00);
            vfree(buffer);
            return 0;
        default:
            dev_dbg(matrix_device,
                    "There is a problem in MSR Operation..\n");
            goto ERROR;
    }
    if (err != 0)
        goto ERROR;

    local_args32.buffer = 0x0; // MAKE THIS EXPLICIT!
    local_args32.length = local_args.length;
    local_args32.msrType1.eax_LSB = local_args.msrType1.eax_LSB;
    local_args32.msrType1.edx_MSB = local_args.msrType1.edx_MSB;
    local_args32.msrType1.ecx_address = local_args.msrType1.ecx_address;
    local_args32.msrType1.ebx_value = local_args.msrType1.ebx_value;
    local_args32.msrType1.n_cpu = local_args.msrType1.n_cpu;
    local_args32.msrType1.operation = local_args.msrType1.operation;

    if (copy_to_user(remote_args32, &local_args32, sizeof(local_args32))) {
        dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n", __FILE__, __func__, __LINE__);
        goto ERROR;
    }
    // printk(KERN_INFO "OK: copied MSR32!\n");
    vfree(buffer);
    return 0;
ERROR:
    vfree(buffer);
    return -EFAULT;
};
#endif // if 0

static int mt_ioctl_pci_config_compat_i(struct pci_config __user *remote_args, struct pci_config32 __user *remote_args32)
{
    struct pci_config local_args;
    struct pci_config32 local_args32;
    int ret = 0;

#if DO_ANDROID
    struct pci_dev *pdev = NULL;

    if (copy_from_user(&local_args, remote_args, sizeof(local_args)) > 0) {
        dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n",
                __FILE__, __func__, __LINE__);
        return -EFAULT;
    }

    pdev = pci_get_bus_and_slot(local_args.bus, PCI_DEVFN(local_args.device, local_args.function));
    if (!pdev) {
        ret = -EINVAL;
        goto exit;
    }
    ret = pci_read_config_dword(pdev, local_args.offset, (u32 *)&local_args.data);

    /* Write back to the same user buffer */

    local_args32.bus = local_args.bus;
    local_args32.device = local_args.device;
    local_args32.function = local_args.function;
    local_args32.offset = local_args.offset;
    local_args32.data = local_args.data;

    if (copy_to_user(remote_args32, &local_args32, sizeof(local_args32))) {
        dev_dbg(matrix_device, "file : %s ,function : %s ,line %i\n", __FILE__, __func__, __LINE__);
        ret = -EFAULT;
        goto exit;
    }

#endif // DO_ANDROID
exit:
    return ret;

};

static long mt_get_scu_fw_version_compat_i(u16 __user *remote_data)
{
    u16 local_data = pw_scu_fw_major_minor;
    if (put_user(local_data, remote_data)) {
        printk(KERN_INFO "ERROR transfering SCU F/W version to userspace!\n");
        return -EFAULT;
    }
    return 0; // SUCCESS
};

static long mt_device_compat_msr_ioctl_i(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
    struct mtx_msr_container32 __user *__msr32 = compat_ptr(ioctl_param);
    struct mtx_msr_container __user *__msr = NULL;
    struct mtx_msr_container32 __tmp;
    u32 data = 0;

    if (copy_from_user(&__tmp, __msr32, sizeof(*__msr32))) {
        printk(KERN_INFO "ERROR copying in mtx_msr32 struct from userspace\n");
        return -PW_ERROR;
    }
    __msr = compat_alloc_user_space(sizeof(*__msr));
    // printk(KERN_INFO "length = %u, __msr = %p\n", __tmp.length, __msr);
    if (__tmp.length == 0 || compat_ptr(__tmp.buffer) == NULL) {
        if (put_user(__tmp.length, &__msr->length)) {
            return -PW_ERROR;
        }
        if (put_user(compat_ptr(__tmp.buffer), &__msr->buffer)) {
            return -PW_ERROR;
        }
        if (get_user(data, &__msr32->msrType1.eax_LSB) || put_user(data, &__msr->msrType1.eax_LSB)) {
            return -PW_ERROR;
        }
        if (get_user(data, &__msr32->msrType1.edx_MSB) || put_user(data, &__msr->msrType1.edx_MSB)) {
            return -PW_ERROR;
        }
        if (get_user(data, &__msr32->msrType1.ecx_address) || put_user(data, &__msr->msrType1.ecx_address)) {
            return -PW_ERROR;
        }
        if (get_user(data, &__msr32->msrType1.ebx_value) || put_user(data, &__msr->msrType1.ebx_value)) {
            return -PW_ERROR;
        }
        if (get_user(data, &__msr32->msrType1.n_cpu) || put_user(data, &__msr->msrType1.n_cpu)) {
            return -PW_ERROR;
        }
        if (get_user(data, &__msr32->msrType1.operation) || put_user(data, &__msr->msrType1.operation)) {
            return -PW_ERROR;
        }
        /*
         * OK, everything copied. Now perform the IOCTL action here.
         * We need to do this here instead of delegating to 'matrix_ioctl()'
         * because this IOCTL needs to return information to Ring3, which means
         * we need the *original* ptr to call 'put_user()' on.
         */
        return mt_ioctl_mtx_msr_compat_i(__msr, __msr32);
        /*
        printk(KERN_INFO "ERROR: compat_msr_ioctl not supported (yet)!\n");
        return -PW_ERROR;
        */
    } else {
        printk(KERN_INFO "ERROR: NO support for \"ENABLE_OP\" in compat space!\n");
        return -PW_ERROR;
    }
};

static long mt_device_compat_pci_config_ioctl_i(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
    struct pci_config32 __user *__pci32 = compat_ptr(ioctl_param);
    struct pci_config __user *__pci = NULL;
    struct pci_config32 __tmp;
    // u32 data;
    size_t __size = 0;
    u32 __dst_idx = 0;
    u8 __user *__buffer = NULL;
    // struct pci_config mtx_pci_drv;
    // unsigned long *buffer = NULL;
    // int err = 0;


    // printk(KERN_INFO "OK, received \"matrix\" compat ioctl!\n");

    // printk(KERN_INFO "sizeof __tmp = %u\n", sizeof(__tmp));

    /*
     * Basic algo:
     * 1. Calculate total memory requirement for the 64b structure (including space required for all arrays etc.)
     * 2. Allocate entire lookup table chunk in 'compat' space (via 'compat_alloc_user_space()').
     * 3. Patch up pointers.
     * 4. Copy over the "header" (all of the non-pointer fields) from 32b --> 64b
     */

    if (copy_from_user(&__tmp, __pci32, sizeof(__tmp))) {
        printk(KERN_INFO "ERROR in length user copy!\n");
        return -EFAULT;
    }
    /*
     * Step 1.
     */
    {
        __size = sizeof(struct pci_config);
    }
    /*
     * Step 2
     */
    __buffer = compat_alloc_user_space(__size);
    if (!__buffer) {
        printk(KERN_INFO "ERROR allocating compat space for size = %u!\n", (unsigned)__size);
        return -EFAULT;
    }
    // printk(KERN_INFO "OK: ALLOCATED compat space of size = %u\n", __size);
    /*
     * Step 3
     */
    {
        __dst_idx = 0;
        __pci = (struct pci_config *)&__buffer[__dst_idx]; __dst_idx += sizeof(*__pci);
    }
    /*
     * Step 4
     */
    {
        if (put_user(__tmp.bus, &__pci->bus)) {
            return -EFAULT;
        }
        if (put_user(__tmp.device, &__pci->device)) {
            return -EFAULT;
        }
        if (put_user(__tmp.function, &__pci->function)) {
            return -EFAULT;
        }
        if (put_user(__tmp.offset, &__pci->offset)) {
            return -EFAULT;
        }
        if (put_user(__tmp.data, &__pci->data)) {
            return -EFAULT;
        }
    }
    /*
     * OK, everything copied. Now perform the IOCTL action here.
     * We need to do this here instead of delegating to 'matrix_ioctl()'
     * because this IOCTL needs to return information to Ring3, which means
     * we need the *original* ptr to call 'put_user()' on.
     */
    return mt_ioctl_pci_config_compat_i(__pci, __pci32);
};

static long mt_device_compat_config_db_ioctl_i(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
#if DO_ANDROID
    u32 __user *__addr32 = compat_ptr(ioctl_param);
    u32 buf, data;
    if (get_user(buf, __addr32)) {
        printk(KERN_INFO "ERROR getting value!\n");
        return -EFAULT;
    }
    /*
     * It is OK to use this 32b value directly
     */
    // printk(KERN_INFO "ADDR = 0x%x\n", buf);

    data = mt_platform_pci_read32(buf);
    /* Write back to the same user buffer */
    if (put_user(data, __addr32)) {
        printk(KERN_INFO "ERROR putting value back to userspace!\n");
        return -EFAULT;
    }
#endif // DO_ANDROID
    return 0;
};

#endif // HAVE_COMPAT_IOCTL && CONFIG_X86_64

int mt_register_dev(void)
{
	int error;
	error = alloc_chrdev_region(&matrix_dev, 0, 1, DRV_NAME);
	if (error < 0) {
		pr_err("Matrix : Could not allocate char dev region");
		// return 1;
		return error;
	}
	matrix_major_number = MAJOR(matrix_dev);
	matrix_class = class_create(THIS_MODULE, DRV_NAME);
	if (IS_ERR(matrix_class)) {
		pr_err("Matrix :Error registering class\n");
		// return 1;
		return -MT_ERROR;
	}
	device_create(matrix_class, NULL, matrix_dev, NULL, DRV_NAME);

	/*Device Registration */
	matrix_cdev = cdev_alloc();
	if (!matrix_cdev) {
		pr_err("Matrix :Could not create device");
		return -ENOMEM;
	}
	matrix_cdev->owner = THIS_MODULE;
	matrix_cdev->ops = &matrix_fops;
	matrix_device = (struct device *)(unsigned long)matrix_cdev->dev;
	if (cdev_add(matrix_cdev, matrix_dev, 1) < 0) {
		pr_err("Error registering device driver\n");
		// return error;
		return -MT_ERROR;
	}
	pr_info("Matrix Registered Successfully with major no.[%d]\n",
		matrix_major_number);
	return MT_SUCCESS;
}

void mt_unregister_dev(void)
{
	pr_info("Matrix De-Registered Successfully...\n");
	unregister_chrdev(matrix_major_number, DRV_NAME);
	device_destroy(matrix_class, matrix_dev);
	class_destroy(matrix_class);
	unregister_chrdev_region(matrix_dev, 1);
	cdev_del(matrix_cdev);
};
