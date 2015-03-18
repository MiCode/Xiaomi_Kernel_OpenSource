/*
 * The file implements main user interface functiothat
 * sets up the vIDT ISRs.
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
#ifdef APP_MODE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#else
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/smp.h>
#include <linux/slab.h>
#include "include/vmm_hsym.h"
#include "include/vmm_hsym_common.h"
#include "include/vidt_ioctl.h"
#include "include/types.h"
#include "include/arch72.h"
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/notifier.h>
#include <linux/list.h>
#include <linux/cpu.h>
#include <linux/fs.h>
#endif
#define ALIGN_DATA_SECT(x) __attribute__ ((aligned (4096), section (x)))
#define DATA_SECT(x) __attribute__ ((section (x)))
#define ALIGN_CODE_SECT(x) __attribute__ ((aligned (4096), section (x), noinline))

//#define STRINGIFY(str)  (#str)
int setup_vidt(void);
void restore_os_idt(void);
#define EENTER_VECTOR  29
#define ERESUME_VECTOR 30
#define EEXIT_VECTOR   31
#define MAX_VIEW_NUM 16
#define PRINT_ALWAYS 1
#define PRINT_DEBUG  0
#define KdPrint(flag, x) if (flag) { printk x; }
#define SECS_SCV_UNTRUSTED (0xDDBBAACCDDBBAACC)

#define ADD_REGION(map, start, npages, patch, npatches, nbytes) \
    do {    \
       uint32_t n = map.n_regions;  \
       map.region[n].start_gva  = (unsigned long) (start); \
       map.region[n].n_pages    = (npages);  \
       map.region[n].n_bytes    = (nbytes);   \
       map.region[n].patch_info = (unsigned long)(patch); \
       map.region[n].n_patches   = (npatches);    \
       map.n_regions++; \
    } while(0)

#ifdef __x86_64__
#define MOV_OPCODE_SIZE (0x2)
#else
#define MOV_OPCODE_SIZE (0x1)
#endif //end of !__x86_64__

#define ADD_SECS_PATCH(patch_id, str, idx)  \
    do {    \
        str##_offset = (unsigned long)str##_secs_patch##patch_id - (unsigned long)str;   \
        str##_patch[idx].val = (unsigned long)secs_ptr;  \
        str##_patch[idx].offset = str##_offset + MOV_OPCODE_SIZE;   \
        str##_patch[idx].type = PATCH_TYPE_SECS_PTR;   \
    } while(0)

#define REGION_SIZE(region) ((unsigned long)region##_end - (unsigned long)region)

#define VIDT_SUCCESS (0)
#define VIDT_FAIL    (-1)

#define SAFE_FREE(ptr) \
do { \
    if (ptr) { \
         kfree(ptr); \
         ptr = NULL; \
    } \
} while (0)

#ifndef APP_MODE
bool set_page_exec(unsigned long address)
{
    unsigned int level;
    pte_t* pte;

    pte = lookup_address((unsigned long)address, &level);

    if (pte) {
        KdPrint(PRINT_ALWAYS, ("pte data 0x%llx \n", pte_val(*pte)));

        if (pte_val(*pte) & _PAGE_NX) {
            KdPrint(PRINT_ALWAYS, ("page is NX \n"));
            pte->pte &= ~_PAGE_NX;
            //VT??? Don't we need to do TLB flush here (invlpg()
            //and probably TLB shootdown)
        } else {
            KdPrint(PRINT_ALWAYS, ("page is X \n"));
        }
        return true;
    }

    return false;
}

typedef struct {
	uint16  limit;
	uint32  base;
} __attribute__((packed)) IA32_GDTR, IA32_IDTR, IA32_LDTR;

typedef struct {
  uint32  OffsetLow :16;   // Offset bits 15..0
  uint32  Selector  :16;    // Selector
  uint32  Reserved_0:8;   // Reserved
  uint32  GateType  :5;     // Gate Type.  See #defines above
  uint32  dpl       :2;
  uint32  present   :1;
  uint32  OffsetHigh:16;  // Offset bits 31..16
} __attribute__((packed)) IA32_IDT_GATE_DESCRIPTOR;

typedef struct {
	uint32 addr_orig;
	uint32 addr_vidt;
} __attribute__((packed)) IA32_IDT_REDIRECT_TABLE;

typedef struct {
	uint64 addr_orig;
	uint64 addr_vidt;
} __attribute__((packed)) IA32e_IDT_REDIRECT_TABLE;

typedef struct {
  uint32  OffsetLow :16;   // Offset bits 15..0
  uint32  Selector  :16;    // Selector
  uint32  ist_index :3;   //IST index 
  uint32  reserved1 :5;
  uint32  GateType  :4;     // Gate Type.  See #defines above
  uint32  reserved2 :1;
  uint32  dpl       :2;
  uint32  present   :1;
  uint32  OffsetMid :16;  // Offset bits 31..16
  uint32  OffsetHigh:32;  // Offset bits 31..16
  uint32  reserved3 :32;  // Offset bits 31..16
} __attribute__((packed)) IA32e_IDT_GATE_DESCRIPTOR;

typedef struct {
	uint16  limit;
	uint64  base;
} __attribute__((packed)) IA32e_GDTR, IA32e_IDTR, IA32e_LDTR;

typedef struct {
	struct {
		uint32	limit_15_00			: 16;
		uint32	base_address_15_00	: 16;
	} lo;
	struct {
		uint32	base_address_23_16	: 8;
		uint32	accessed			: 1;
		uint32	readable			: 1;
		uint32	conforming			: 1;
		uint32	mbo_11				: 1;	// Must Be One
		uint32	mbo_12				: 1;	// Must Be One
		uint32	dpl					: 2;	// Descriptor Privilege Level
		uint32	present				: 1;
		uint32	limit_19_16			: 4;
		uint32	avl					: 1;	// Available to software
		uint32	mbz_21				: 1;	// Must Be Zero
		uint32	default_size		: 1;    // 0 = 16-bit segment; 1 = 32-bit segment
		uint32	granularity 		: 1;
		uint32	base_address_31_24	: 8;
	} hi;
} __attribute__((packed)) IA32_CODE_SEGMENT_DESCRIPTOR;

typedef struct {
	uint32 vep_max; //ptr
	uint32 vep_tos; //ptr
	uint32 redirect_size;
#ifdef __x86_64__
	uint64 redirect_addr; //ptr
	IA32e_IDTR os_idtr;
#else
	uint32 redirect_addr; //ptr
	IA32_IDTR os_idtr;
#endif
} __attribute__((packed)) PER_CORE_STATE;

typedef struct {
	uint32 per_cpu_vidt_start;
    uint32 per_cpu_vidt_end;
	uint32 per_cpu_vidt_stub_start;
    uint32 per_cpu_vidt_stub_end;
    uint32 per_cpu_test_code_start;
    uint32 per_cpu_test_code_end;
} __attribute__((packed)) PER_CORE_VIDT_STATE;

struct vidt_priv_data{
       unsigned long int pid;
       unsigned long int viewid;
       unsigned long int magic;
};

struct view_list {
      struct vidt_priv_data view_data;
      struct list_head list;
};

#define VIDT_PRIV_MAGIC (unsigned long int)0xABCDEF12

#define MAX_CORES 8

PER_CORE_VIDT_STATE core_vidt_state[MAX_CORES] DATA_SECT(".vidtd");
PER_CORE_STATE core_state[MAX_CORES] DATA_SECT(".vidtd");
#ifdef __x86_64__
IA32e_IDT_REDIRECT_TABLE redirect[MAX_CORES][256] DATA_SECT(".vidtd");
#else
IA32_IDT_REDIRECT_TABLE redirect[MAX_CORES][256] DATA_SECT(".vidtd");
#endif
uint64 ptr_core_state DATA_SECT(".vidtd") = 1; //initialize before use in vidt_setup
uint64 secs_la ALIGN_DATA_SECT(".vidtd") = 0x0; //alloc and init during vidt setup


/* FIXME: these inlines should come from an appropriate header file */
#ifdef __x86_64__
extern inline int cpuid_asm64(uint32_t leaf, uint32_t b_val, uint64_t c,
			      uint64_t d, uint64_t S, uint64_t D);
#else
extern inline int cpuid_asm(uint32_t leaf, uint32_t b_val, uint32_t c,
			    uint32_t d, uint32_t S, uint32_t D);
#endif

#ifdef __x86_64__
extern inline int vmcall_asm64(uint32_t leaf, uint32_t b_val, uint64_t c,
			       uint64_t d, uint64_t S, uint64_t D);
#else
extern inline int vmcall_asm(uint32_t leaf, uint32_t b_val, uint32_t c,
			     uint32_t d, uint32_t S, uint32_t D);
#endif

int map_pid_viewId(void *priv_data, struct entry_pid_viewid view_new);
int unmap_pid_viewId(void *priv_data, struct entry_pid_viewid view_entry);
int clean_ta_view(void *priv_data);
void clean_view_map_list(struct file *file);

int init_view_map_list(struct file *file)
{
    if (!file)
        return VIDT_FAIL;
    struct view_list *pfd_view_map =
        (struct view_list*) kmalloc(sizeof(struct view_list), GFP_KERNEL);
    if(pfd_view_map == NULL)
        return VIDT_FAIL;
    INIT_LIST_HEAD(&pfd_view_map->list);
    file->private_data = (void *)pfd_view_map;

    return VIDT_SUCCESS;
}

void clean_view_map_list(struct file *file)
{
    if (file->private_data)
        kfree(file->private_data);
    file->private_data = NULL;
}

int clean_ta_view(void *priv_data)
{
	struct view_list *temp_head = NULL;
	struct view_list *pos = NULL;
	struct view_list *temp_node = NULL;
	temp_head = (struct view_list *) priv_data;
	if (temp_head == NULL)
		return VIDT_FAIL;
	list_for_each_entry_safe(pos, temp_node, &temp_head->list, list) {
		if (pos->view_data.magic == VIDT_PRIV_MAGIC) {
#ifdef __x86_64__
			vmcall_asm64(SL_CMD_HSEC_REMOVE_VIEW, 0, 0,
				     pos->view_data.viewid, 0, 0);
#else
			vmcall_asm(SL_CMD_HSEC_REMOVE_VIEW, 0, 0,
				   pos->view_data.viewid, 0, 0);
#endif
			list_del(&pos->list);
			kfree(pos);
		} else {
			pr_info("MAGIC 0x%lx doesn't match 0x%lx\n",
				pos->view_data.magic, VIDT_PRIV_MAGIC);
		}
	}

	return VIDT_SUCCESS;
}


int unmap_pid_viewId(void *priv_data, struct entry_pid_viewid view_entry)
{
     struct view_list *temp_head = NULL,*pos=NULL,*temp_node=NULL;

     temp_head = (struct view_list*) priv_data;
     if(temp_head == NULL) {
       printk(KERN_INFO "nothing there to clean here unmap_pid_viewId!!\n");
       return VIDT_FAIL;
     }
     list_for_each_entry_safe(pos,temp_node,&(temp_head->list),list){
         if((pos->view_data.magic == VIDT_PRIV_MAGIC) &&
             pos->view_data.viewid == view_entry.viewid && pos->view_data.pid == view_entry.pid) {
            list_del(&pos->list);
            kfree(pos);
            return VIDT_SUCCESS;
         }
     }
     return VIDT_FAIL;
}

int map_pid_viewId(void *priv_data, struct entry_pid_viewid view_new)
{
     struct view_list *temp_head= NULL,*temp_node = NULL;
     temp_head = (struct view_list *)priv_data;
     temp_node = (struct view_list*) kmalloc(sizeof(struct view_list), GFP_KERNEL);
     if(temp_node == NULL)
         return VIDT_FAIL;
     temp_node->view_data.pid    =  view_new.pid;
     temp_node->view_data.viewid =  view_new.viewid;
     temp_node->view_data.magic  =  VIDT_PRIV_MAGIC;
     INIT_LIST_HEAD(&temp_node->list);

     list_add(&(temp_node->list),&(temp_head->list));

     return VIDT_SUCCESS;
}

#if !defined(__x86_64__)
static void ia32_read_idtr(void *p_descriptor)
{
    asm  (
            "sidt  %0\n"
            : "=m" (*(IA32_IDTR*)p_descriptor)
            );
}

void ia32_load_idtr(void *p_descriptor)
{
    __asm__  (
            "cli\n"
            "lidt  %0\n"
            "sti\n"
            :
            : "m" (*(IA32_IDTR*)p_descriptor)
            );
}

static inline void ia32_read_gdtr(void *p_descriptor)
{
	__asm__  (
            "sgdt %0\n"
            : "=m" (*(IA32_GDTR*)p_descriptor)
            );
}

static inline void ia32_read_ldtr(void *p_descriptor)
{
	__asm__  (
            "sldt %0\n"
            : "=m" (*(IA32_LDTR*)p_descriptor)
            );
}
#else
static inline void ia32e_load_idtr(void *p_descriptor)
{
    __asm__  (
            "cli\n"
            "lidt  %0\n"
            "sti\n"
            :
            : "m" (*(IA32e_IDTR*)p_descriptor)
            );
}
static inline void ia32e_read_gdtr(void *p_descriptor)
{
	__asm__  (
            "sgdt %0\n"
            : "=m" (*(IA32e_GDTR*)p_descriptor)
            );
}

static inline void ia32e_read_ldtr(void *p_descriptor)
{
	__asm__  (
            "sldt %0\n"
            : "=m" (*(IA32e_LDTR*)p_descriptor)
            );
}

static void ia32e_read_idtr(void *p_descriptor)
{
    asm  (
            "sidt  %0\n"
            : "=m" (*(IA32e_IDTR*)p_descriptor)
            );
}
#endif

#define IA32_IDT_GATE_TYPE_TASK					0x5
#define IA32_IDT_GATE_TYPE_INTERRUPT_16			0x86
#define IA32_IDT_GATE_TYPE_TRAP_16				0x87
#define IA32_IDT_GATE_TYPE_INTERRUPT_DPL0_32	0x8E
#define IA32_IDT_GATE_TYPE_TRAP_32				0x8F
#define IA32_DPL3                               0x3
#define IA32_GATE_TYPE_INTR                     0xE

#if !defined(__x86_64__)
void print_ldt(void* x)
{
	uint32 *pTab;
	uint16 i;
	uint32 base;
	uint16 limit;
	IA32_LDTR ldtr;

	ia32_read_ldtr(&ldtr);
	base = ldtr.base;
	limit = ldtr.limit;
	KdPrint(PRINT_ALWAYS, ("LDT BASE = 0x%x LDT LIMIT = 0x%x \n", 
                (uint32) base, (uint32) limit));
	pTab = (uint32 *) base;
	for (i = 0; i < (limit+1) / sizeof(IA32_CODE_SEGMENT_DESCRIPTOR); ++i)
	{
		KdPrint(PRINT_DEBUG, ("0x%x 0x%x 0x%x \n", 
                    (uint32)i, pTab[i*2], pTab[i*2+1]));
	}
}

void print_gdt(void* x)
{
	uint32 *pTab;
	uint16 i;
	uint32 base;
	uint16 limit;
	IA32_GDTR gdtr;

	ia32_read_gdtr(&gdtr);
	base = gdtr.base;
	limit = gdtr.limit;

	KdPrint(PRINT_ALWAYS, ("GDT BASE = 0x%x GDT LIMIT = 0x%x \n", 
                (uint32) base, (uint32) limit));

	pTab = (uint32 *) base;

	for (i = 0; i < (limit+1) / sizeof(IA32_CODE_SEGMENT_DESCRIPTOR); ++i)
	{
		KdPrint(PRINT_DEBUG, ("0x%x 0x%x 0x%x \n", 
                    (uint32)i, pTab[i*2], pTab[i*2+1]));
	}
}
#endif // end of !__x86_64__
#endif // end of !APP_MODE 

unsigned int isr_size = 0;
extern void MyHandler_non_arch_end(void);
extern void MyHandler(void);
extern void MyHandler_arch_end(void);

#define NUM_AEX_CODE_PATCHES 6
extern void test_code_cpuindex(void);
extern void test_code_ptr_core_state_patch(void);
extern void test_code_secs_patch1(void);
extern void test_code_secs_patch2(void);
extern void test_code_cmp_patch(void);
extern void test_code_exit_page_patch(void);
extern void test_code(void);

#define NUM_EXIT_CODE_PATCHES 4
extern void exit_code_cpuindex(void);
extern void exit_code_cmp_patch(void);
extern void exit_code_secs_patch1(void);
extern void exit_code_exit_page_patch(void);
extern void exit_code(void);

#define NUM_ENT_RES_CODE_PATCHES 10
extern void enter_eresume_code_cpuindex(void);
extern void enter_eresume_code_cmp_patch1(void);
extern void enter_eresume_code_cmp_patch2(void);
extern void enter_eresume_code_secs_patch1(void);
extern void enter_eresume_code_secs_patch2(void);
extern void enter_eresume_code_secs_patch3(void);
extern void enter_eresume_code_secs_patch4(void);
extern void enter_eresume_code_secs_patch5(void);
extern void enter_eresume_code_secs_patch6(void);
extern void enter_eresume_code_enter_page_patch(void);
extern void enter_eresume_code(void);

extern void test_code_end(void);
extern void exit_code_end(void);
extern void enter_eresume_code_end(void);
extern void vidt_stub_patch_val0(void);
extern void vidt_stub_patch_callee0(void);
extern void begin_vidt_stub0(void);

#ifdef APP_MODE
int get_vidt_code_pages(uint8_t **vidt_buf, uint32_t *vidt_size)
{

    void* per_cpu_vidt_stub = NULL;
    void* per_cpu_test_code = NULL;
    void* per_cpu_exit_code = NULL;
    void* per_cpu_enter_eresume_code = NULL;
    void *code_buf = NULL;

    uint32_t size_stub = 0, size_tc = 0, size_exitc = 0, tmp_size = 0;
    uint32_t size_enter_eresumec = 0, size_total = 0;
    uint32_t off = 0;
    int i, ret = 0;

    // --------------------------------------------------------
    //  Step1: Do allocations for different flows
    // --------------------------------------------------------
    //alloc and clear pages for per_cpu_vidt_stub (MyHandler)
    isr_size = ((unsigned int)MyHandler_non_arch_end - 
            (unsigned int)MyHandler_arch_end);
    per_cpu_vidt_stub = calloc(1, isr_size*256);
    if (per_cpu_vidt_stub == NULL) {
        printf("%s:%d: Failed allocating memory\n", __func__, __LINE__);
        ret = -1;
        goto exit_vidt_code_pages;
    }
    size_stub += isr_size*256;

    //alloc and clear pages for AEX Exit stub code
    tmp_size = REGION_SIZE(test_code);
    per_cpu_test_code = calloc(1, tmp_size);
    if (per_cpu_test_code == NULL) {
        printf("%s:%d: Failed allocating memory\n", __func__, __LINE__);
        ret = -1;
        goto exit_vidt_code_pages;
    }
    size_tc = tmp_size;

    //alloc and clear pages for sync exit stub code
    tmp_size = REGION_SIZE(exit_code);
    per_cpu_exit_code = calloc(1, tmp_size);
    if (per_cpu_exit_code == NULL) {
        printf("%s:%d: Failed allocating memory\n", __func__, __LINE__);
        ret = -1;
        goto exit_vidt_code_pages;
    }
    size_exitc = tmp_size;

    //alloc and clear pages for enter/resume stub code
    tmp_size = REGION_SIZE(enter_eresume_code);
    per_cpu_enter_eresume_code = calloc(1, tmp_size);
    if (per_cpu_enter_eresume_code == NULL) {
        printf("%s:%d: Failed allocating memory\n", __func__, __LINE__);
        ret = -1;
        goto exit_vidt_code_pages;
    }
    size_enter_eresumec = tmp_size;

    // --------------------------------------------------------
    // Step2: Copy the code pages of flows individually
    // --------------------------------------------------------
    memcpy((char*)((unsigned int)per_cpu_vidt_stub),
            (char*)MyHandler, isr_size*21);
    //copy non-arch handlers 21-255
    for (i=21; i<256; i++) {
        memcpy( (char*)((unsigned int)per_cpu_vidt_stub + (isr_size*i)), 
                (char*)MyHandler_arch_end, isr_size);
    }

    memcpy( (char*)((unsigned int)per_cpu_test_code), 
            (char*)test_code, (unsigned int)test_code_end - 
            (unsigned int)test_code);


    memcpy( (char*)((unsigned int)per_cpu_enter_eresume_code), 
            (char*)enter_eresume_code, (unsigned int)enter_eresume_code_end - 
            (unsigned int)enter_eresume_code);

    memcpy( (char*)((unsigned int)per_cpu_exit_code), 
            (char*)exit_code, (unsigned int)exit_code_end - 
            (unsigned int)exit_code);


    // --------------------------------------------------------
    // Step3: Concatenate all the code pages into 1 memory chunk
    // --------------------------------------------------------
    size_total = size_stub + size_tc + size_exitc + size_enter_eresumec;
    code_buf = calloc(1, size_total);
    if (!code_buf) {
        printf("%s:%d: Failed allocating memory\n", __func__, __LINE__);
        ret = -1;
        size_total = 0;
        goto exit_vidt_code_pages;
    }

    memcpy(code_buf + off, (char *)per_cpu_test_code, size_tc);
    off += size_tc;
    printf("size tc = %lu\n", size_tc);
    memcpy(code_buf + off, (char *)per_cpu_enter_eresume_code, size_enter_eresumec);
    off += size_enter_eresumec;
    printf("size enter_eresume= %lu\n", size_enter_eresumec);
    memcpy(code_buf + off, (char *)per_cpu_exit_code, size_exitc);
    off += size_exitc;
    printf("size exit code = %lu\n", size_exitc);
    memcpy(code_buf + off, (char *)per_cpu_vidt_stub, size_stub);
    off += size_stub;
    printf("size stub = %lu\n", size_stub);

exit_vidt_code_pages:
    // --------------------------------------------------------
    // Step4: Free memory for individual flows 
    // --------------------------------------------------------
    if (per_cpu_vidt_stub)
        free(per_cpu_vidt_stub);
    if (per_cpu_test_code)
        free(per_cpu_test_code);
    if (per_cpu_enter_eresume_code)
        free(per_cpu_enter_eresume_code);
    if (per_cpu_exit_code)
        free(per_cpu_exit_code);

    // --------------------------------------------------------
    // Step5: Return buffer and size 
    // --------------------------------------------------------
    *vidt_buf = code_buf;
    *vidt_size = size_total;

    return ret;
}

void free_vidt_code_pages(void *buf)
{
    if (buf)
        free(buf);
}
#else

void print_idt(void *desc)
{
    int i;

#ifdef __x86_64__
    IA32e_IDT_GATE_DESCRIPTOR *Idt = (IA32e_IDT_GATE_DESCRIPTOR *)desc;
#else
    IA32_IDT_GATE_DESCRIPTOR *Idt = (IA32_IDT_GATE_DESCRIPTOR *)desc;
#endif

	for (i = 0 ; i < 256 ; i++)
    {
		KdPrint(PRINT_DEBUG, ("Gate Type 0x%x Selector:Address 0x%x:0x%llx \n", 
                    Idt[i].GateType, 
                    Idt[i].Selector, 
#ifdef __x86_64__
                ((uint64_t)Idt[i].OffsetHigh << 32) | (Idt[i].OffsetMid << 16) | (Idt[i].OffsetLow) ));
#else
                    (Idt[i].OffsetHigh << 16) | (Idt[i].OffsetLow)));
#endif
    }

}


void read_idt(uint32 cpuindex, uint64 codebase, void *desc)
{
    int i;

#ifdef __x86_64__
    IA32e_IDT_GATE_DESCRIPTOR *Idt = (IA32e_IDT_GATE_DESCRIPTOR *)desc;
#else
    IA32_IDT_GATE_DESCRIPTOR *Idt = (IA32_IDT_GATE_DESCRIPTOR *)desc;
#endif

	for (i = 0 ; i < 256 ; i++)
    {
#ifdef __x86_64__

        redirect[cpuindex][i].addr_orig =
                (((uint64_t)Idt[i].OffsetHigh << 32) |
                 ((uint64_t)Idt[i].OffsetMid << 16) |
                 (Idt[i].OffsetLow));
#else
		redirect[cpuindex][i].addr_orig = 
            (Idt[i].OffsetHigh << 16) | (Idt[i].OffsetLow);
#endif
		redirect[cpuindex][i].addr_vidt = (unsigned long)codebase + isr_size * i;
		
        KdPrint(PRINT_DEBUG, ("Gate Type 0x%x Selector:Address 0x%x:0x%llx ",
                Idt[i].GateType,
                Idt[i].Selector,
                redirect[cpuindex][i].addr_orig));
		KdPrint(PRINT_DEBUG, ("Redirect Address 0x%llx ", 
                    redirect[cpuindex][i].addr_vidt));
    }

}

void InstallExceptionHandler(void *desc,
        uint32 ExceptionIndex, uint64 HandlerAddr, uint32 GateType, uint32 dpl)
{
#ifdef __x86_64__
    IA32e_IDT_GATE_DESCRIPTOR *Idt = (IA32e_IDT_GATE_DESCRIPTOR*)desc;
#else
    IA32_IDT_GATE_DESCRIPTOR *Idt = (IA32_IDT_GATE_DESCRIPTOR*)desc;
#endif
    if (!desc)
        return;
	//assuming Idt ptr is valid
#ifdef __x86_64__
    Idt[ExceptionIndex].OffsetLow  = HandlerAddr & 0xFFFF;
    Idt[ExceptionIndex].OffsetMid  = ((HandlerAddr >> 16 ) & 0xFFFF);
    Idt[ExceptionIndex].OffsetHigh  = ((HandlerAddr >> 32 ) & 0xFFFFFFFF);
#else
    Idt[ExceptionIndex].OffsetLow  = HandlerAddr & 0xFFFF;
    Idt[ExceptionIndex].OffsetHigh = HandlerAddr >> 16;
#endif
    Idt[ExceptionIndex].GateType = GateType;
    Idt[ExceptionIndex].dpl = dpl;
}

void update_vidt(uint32 cpuindex, void *desc)
{
    int i;
#ifdef __x86_64__
    IA32e_IDT_GATE_DESCRIPTOR* Idt = (IA32e_IDT_GATE_DESCRIPTOR*) desc;
#else
    IA32_IDT_GATE_DESCRIPTOR *Idt = (IA32_IDT_GATE_DESCRIPTOR *)desc;
#endif

	for (i = 0 ; i < 256 ; i++)
    {
		//check for gate type - only if intr/trap gate then install handler 
		//for task gate - point to original selector in gdt/ldt
		if (IA32_IDT_GATE_TYPE_TASK == Idt[i].GateType) 
		{
			KdPrint(PRINT_DEBUG, ("Gate Type - task gate - skipping "));
			continue;
		}

        InstallExceptionHandler((void *)Idt, i, redirect[cpuindex][i].addr_vidt,
                Idt[i].GateType, Idt[i].dpl);

		KdPrint(PRINT_DEBUG, ("Gate Type 0x%x Selector:Address 0x%x:0x%llx ", 
                Idt[i].GateType, Idt[i].Selector, 
#ifdef __x86_64__
                ((uint64_t)Idt[i].OffsetHigh << 32) | ((uint64_t)Idt[i].OffsetMid << 16) | (Idt[i].OffsetLow) ));
#else
                (Idt[i].OffsetHigh << 16) | (Idt[i].OffsetLow) ));
#endif
		KdPrint(PRINT_DEBUG, ("original Address 0x%llx\n", 
                    redirect[cpuindex][i].addr_orig)); 
    }
}

void update_vidt_special_handlers(uint32 cpuindex, void *param)
{
#ifdef __x86_64__
    IA32e_IDT_GATE_DESCRIPTOR *Idt = (IA32e_IDT_GATE_DESCRIPTOR *)param;
#else
    IA32_IDT_GATE_DESCRIPTOR *Idt = (IA32_IDT_GATE_DESCRIPTOR *)param;
#endif

    int i=EENTER_VECTOR;

    InstallExceptionHandler((void *)Idt,
            i, redirect[cpuindex][i].addr_vidt,
            IA32_GATE_TYPE_INTR, IA32_DPL3);

    KdPrint(PRINT_ALWAYS, ("Gate Type 0x%x Selector:Address 0x%x:0x%llx \n", 
                Idt[i].GateType, Idt[i].Selector,
#ifdef __x86_64__
                ((uint64_t)Idt[i].OffsetHigh << 32) | ((uint64_t)Idt[i].OffsetMid << 16) | (Idt[i].OffsetLow) ));
#else
                (Idt[i].OffsetHigh << 16) | (Idt[i].OffsetLow) ));
#endif

    KdPrint(PRINT_ALWAYS, ("original Address 0x%llx \n",
                redirect[cpuindex][i].addr_orig));

    i=EEXIT_VECTOR;

    InstallExceptionHandler((void *)Idt,
            i, redirect[cpuindex][i].addr_vidt,
            IA32_GATE_TYPE_INTR, IA32_DPL3);
    KdPrint(PRINT_DEBUG, ("Gate Type 0x%x Selector:Address 0x%x:0x%llx\n", 
                Idt[i].GateType, Idt[i].Selector, 
#ifdef __x86_64__
                ((uint64_t)Idt[i].OffsetHigh << 32) | ((uint64_t)Idt[i].OffsetMid << 16) | (Idt[i].OffsetLow) ));
#else
                (Idt[i].OffsetHigh << 16) | (Idt[i].OffsetLow) ));
#endif
    KdPrint(PRINT_DEBUG, ("original Address 0x%llx \n", 
                redirect[cpuindex][i].addr_orig)); 
    
    i=ERESUME_VECTOR;
    InstallExceptionHandler((void *)Idt,
       i, redirect[cpuindex][i].addr_vidt,
            IA32_GATE_TYPE_INTR, IA32_DPL3);
    KdPrint(PRINT_DEBUG, ("Gate Type 0x%x Selector:Address 0x%x:0x%llx\n",
                Idt[i].GateType, Idt[i].Selector,
#ifdef __x86_64__
                ((uint64_t)Idt[i].OffsetHigh << 32) | ((uint64_t)Idt[i].OffsetMid << 16) | (Idt[i].OffsetLow) ));
#else
                (Idt[i].OffsetHigh << 16) | (Idt[i].OffsetLow) ));
#endif
    KdPrint(PRINT_DEBUG, ("original Address 0x%llx \n",
                redirect[cpuindex][i].addr_orig));
}

void restore_os_idt(void)
{
	uint32 cpuindex;
    get_online_cpus();
	for_each_online_cpu(cpuindex) {
#ifdef __x86_64__
			smp_call_function_single(cpuindex, ia32e_load_idtr,
				&core_state[cpuindex].os_idtr, 1);
#else
			smp_call_function_single(cpuindex, ia32_load_idtr,
				&core_state[cpuindex].os_idtr, 1);
#endif
	}
    put_online_cpus();
}

typedef void (*segment_reg_func_t)(void *desc);

#define NUM_STACK_PAGES 8

int setup_vidt(void)
{
    void* vidt = NULL;
    unsigned int page_offset = 0;

#ifdef __x86_64__
    IA32e_IDT_GATE_DESCRIPTOR* vidt_start, *idt_start;
    IA32e_IDTR Idtr;
#else
    IA32_IDT_GATE_DESCRIPTOR* vidt_start, *idt_start;
    IA32_IDTR Idtr;
#endif
    void* per_cpu_vidt_stub = NULL;
    void* per_cpu_test_code = NULL;
    void* per_cpu_exit_code = NULL;
    void* per_cpu_enter_eresume_code = NULL;
    void* per_cpu_r0stack_pages = NULL;
    void* per_cpu_enter_page = NULL;
    void* per_cpu_exit_page = NULL;
    uint32 cpuindex = 0;
    unsigned int ActiveProcessors = 0;
    unsigned int i = 0;

    unsigned int test_code_offset = 0;
    unsigned int enter_eresume_code_offset = 0;
    unsigned int exit_code_offset = 0;
    unsigned int region_size = 0;
    hsec_sl_param_t sl_info;

    char* redirect_ptr = NULL;
    secs_t *secs_ptr = NULL;

    memset(&sl_info, 0, sizeof(hsec_sl_param_t));

    printk("setup_vidt loaded at %p\n", (void *)setup_vidt);
    secs_la = (uint64)kmalloc(0x1000, GFP_KERNEL); //1 page for SECS la
    if(secs_la == NULL)
    {
        KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
        return SL_EUNKNOWN;
    }
    printk("secs gva at %p\n", (void*) secs_la);
    secs_ptr = (secs_t*)secs_la;
    //Temp setup of SECS is no longer needed.
    //But keeping it here, as these numbers serve as magic
    //numbers while debugging.
    /*setup psuedo SECS for view 0 -- note this is for test*/
    memset((char*)secs_ptr, 0, 0x1000);
	secs_ptr->scv = SECS_SCV_UNTRUSTED;
    //secs_ptr->size = 4096;
    secs_ptr->size = 0x6A000;
    secs_ptr->base = 0; /*fill in for testing with simple app*/
    secs_ptr->ssa_frame_size = 1; //1 page

#ifdef __x86_64__
    sl_info.secs_gva = (uint64_t)secs_ptr;
#else
    sl_info.secs_gva = (uint32_t)secs_ptr;
#endif
    KdPrint(PRINT_ALWAYS, ("Registering sl global info 0x%llx\n", 
                sl_info.secs_gva));
    reg_sl_global_info(&sl_info);

    //allocate redirect table from non-pageable memory
#ifdef __x86_64__
    redirect_ptr = kmalloc(sizeof(IA32e_IDT_REDIRECT_TABLE) * 256 * MAX_CORES,
            GFP_KERNEL);
#else
    redirect_ptr = kmalloc(sizeof(IA32_IDT_REDIRECT_TABLE) * 256 * MAX_CORES,
            GFP_KERNEL);
#endif
    if(redirect_ptr == NULL)
    {
        KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
        SAFE_FREE(secs_ptr);
        return SL_EUNKNOWN;
    }

    //allocate ptr_core_state from non-pageable memory
    ptr_core_state = (unsigned long) kmalloc(sizeof(PER_CORE_STATE) * MAX_CORES, GFP_KERNEL);

    if(ptr_core_state == NULL)
    {
        KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
        SAFE_FREE(secs_ptr);
        SAFE_FREE(redirect_ptr);
        return SL_EUNKNOWN;
    }
    memset((char*)ptr_core_state, 0, sizeof(PER_CORE_STATE) * MAX_CORES);

	ActiveProcessors = NR_CPUS;
	KdPrint(PRINT_DEBUG, ("ActiveProcessors = %x \n",ActiveProcessors));

	cpuindex = 0;
    get_online_cpus();
    for_each_online_cpu(cpuindex) {
        hsec_vIDT_param_t vIDT_info;
        hsec_map_t region_map;
        unsigned int n_stub_patches = 0;
        segment_reg_func_t read_gdtr_fp, read_idtr_fp, read_ldtr_fp;
#ifdef __x86_64__
        IA32e_LDTR ldtr;
        IA32e_GDTR gdtr;
        IA32e_IDTR idtr;
        IA32e_IDTR idt_desc;
#else
        IA32_LDTR ldtr;
        IA32_GDTR gdtr;
        IA32_IDTR idtr;
        IA32_IDTR idt_desc;
#endif
        hsec_patch_info_t *stub_patch, *test_code_patch, *enter_eresume_code_patch, *exit_code_patch;

        test_code_patch=stub_patch=enter_eresume_code_patch=exit_code_patch = NULL;

        KdPrint(PRINT_ALWAYS, ("======cpuindex = 0x%x ========\n", cpuindex));

        //FIXME - these values should be per logical processor
        //update psuedo secs cached gdtr, idtr, ldtr
#ifdef __x86_64__
        read_gdtr_fp = ia32e_read_gdtr;
        read_idtr_fp = ia32e_read_idtr;
        read_ldtr_fp = ia32e_read_ldtr;
#else
        read_gdtr_fp = ia32_read_gdtr;
        read_idtr_fp = ia32_read_idtr;
        read_ldtr_fp = ia32_read_ldtr;
#endif

        smp_call_function_single(cpuindex, read_gdtr_fp, (void *)&gdtr, 1);

        secs_ptr->pcd[cpuindex].gdtr = gdtr.base;

        smp_call_function_single(cpuindex, read_ldtr_fp, (void *)&ldtr, 1);
        secs_ptr->pcd[cpuindex].ldtr = ldtr.base;

        smp_call_function_single(cpuindex, read_idtr_fp, (void *)&idtr, 1);

        secs_ptr->pcd[cpuindex].idtr = idtr.base;

        //alloc and clear page for this cpus vidt
        vidt = kmalloc(0x1000, GFP_KERNEL); //1 page
        if(vidt == NULL)
        {
            KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
            goto fail_msg;
        }

        //alloc and clear pages for per_cpu_vidt_stub (MyHandler)
        isr_size = ((unsigned long)MyHandler_non_arch_end - 
                (unsigned long)MyHandler_arch_end);
        per_cpu_vidt_stub = kmalloc(isr_size*256, GFP_KERNEL); 
        if(per_cpu_vidt_stub == NULL)
        {
            KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
            goto fail_msg;
        }
        //alloc and clear pages for AEX Exit stub code
        if (((unsigned long)test_code_end - (unsigned long)test_code)<0x1000) {
            per_cpu_test_code = kmalloc(0x1000, GFP_KERNEL); 
        } else {
            per_cpu_test_code = kmalloc((unsigned long)test_code_end - 
                    (unsigned long)test_code, GFP_KERNEL); 
        }
        if(per_cpu_test_code == NULL)
        {
            KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
            goto fail_msg;
        }
        //alloc and clear pages for sync exit stub code
        if (((unsigned long)exit_code_end - (unsigned long)exit_code)<0x1000) {
            per_cpu_exit_code = kmalloc(0x1000, GFP_KERNEL); 
        }
        else {
            per_cpu_exit_code = kmalloc((unsigned long)exit_code_end - 
                    (unsigned long)exit_code, GFP_KERNEL);
        }
        if(per_cpu_exit_code == NULL)
        {
            KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
            goto fail_msg;
        }
        //alloc and clear pages for enter/resume stub code
        if (((unsigned long)enter_eresume_code_end - 
                    (unsigned long)enter_eresume_code)<0x1000) {
            per_cpu_enter_eresume_code = kmalloc(0x1000, GFP_KERNEL); 
        }
        else {
            per_cpu_enter_eresume_code = kmalloc((unsigned long)enter_eresume_code_end - 
                    (unsigned long)enter_eresume_code, GFP_KERNEL); 
        }

        if(per_cpu_enter_eresume_code == NULL)
        {
            KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
            goto fail_msg;
        }

	printk("allocating per cpu thread r0 stack pages \n");
	per_cpu_r0stack_pages = kmalloc(0x1000*NUM_STACK_PAGES, GFP_KERNEL); 
    if(per_cpu_r0stack_pages == NULL)
    {
        KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
        goto fail_msg;
    }
	per_cpu_enter_page = kmalloc(0x1000, GFP_KERNEL); 
    if(per_cpu_enter_page == NULL)
    {
        KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
        goto fail_msg;
    }
	per_cpu_exit_page = kmalloc(0x1000, GFP_KERNEL); 
    if(per_cpu_exit_page == NULL)
    {
        KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
        goto fail_msg;
    }
	
	//if any kmalloc failed goto fail_msg
	if ((!vidt) || (!per_cpu_vidt_stub) || (!per_cpu_test_code) 
			|| (!per_cpu_exit_code) || (!per_cpu_enter_eresume_code)
			|| (!per_cpu_r0stack_pages) 
			|| (!per_cpu_enter_page) || (!per_cpu_exit_page)) 
		goto fail_msg;

#ifdef __x86_64__

	KdPrint(PRINT_ALWAYS, ("vidt is at 0x%llx\n", (uint64)vidt));	
	KdPrint(PRINT_ALWAYS, ("per_cpu_vidt_stub is at 0x%llx size 0x%x\n",
				(uint64)per_cpu_vidt_stub, isr_size*256));
	KdPrint(PRINT_ALWAYS, ("per_cpu_test_code is at 0x%llx \n", 
				(uint64)per_cpu_test_code));
	KdPrint(PRINT_ALWAYS, ("per_cpu_exit_code is at 0x%llx \n", 
				(uint64)per_cpu_exit_code));
	KdPrint(PRINT_ALWAYS, ("per_cpu_enter_eresume_code is at 0x%llx \n", 
				(uint64)per_cpu_enter_eresume_code));
	KdPrint(PRINT_ALWAYS, ("per_cpu_r0stack_pages start at 0x%llx \n", 
				(uint64)per_cpu_r0stack_pages));
	KdPrint(PRINT_ALWAYS, ("per_cpu_enter_pages start at 0x%llx \n", 
				(uint64)per_cpu_enter_page));
	KdPrint(PRINT_ALWAYS, ("per_cpu_exit_page start at 0x%llx \n", 
				(uint64)per_cpu_exit_page));
#else

	KdPrint(PRINT_ALWAYS, ("vidt is at 0x%x\n", (uint32)vidt));	
	KdPrint(PRINT_ALWAYS, ("per_cpu_vidt_stub is at 0x%x size 0x%x\n",
				(uint32)per_cpu_vidt_stub, isr_size*256));
	KdPrint(PRINT_ALWAYS, ("per_cpu_test_code is at 0x%x \n", 
				(uint32)per_cpu_test_code));
	KdPrint(PRINT_ALWAYS, ("per_cpu_exit_code is at 0x%x \n", 
				(uint32)per_cpu_exit_code));
	KdPrint(PRINT_ALWAYS, ("per_cpu_enter_eresume_code is at 0x%x \n", 
				(uint32)per_cpu_enter_eresume_code));
	KdPrint(PRINT_ALWAYS, ("per_cpu_r0stack_pages start at 0x%lx \n", 
				(uint32)per_cpu_r0stack_pages));
	KdPrint(PRINT_ALWAYS, ("per_cpu_enter_pages start at 0x%lx \n", 
				(uint32)per_cpu_enter_page));
	KdPrint(PRINT_ALWAYS, ("per_cpu_exit_page start at 0x%lx \n", 
				(uint32)per_cpu_exit_page));
#endif
	KdPrint(PRINT_ALWAYS, ("==============\n"));

        memset(vidt, 0, 0x1000);
        memset( per_cpu_vidt_stub, 0, isr_size*256); 
        memset( per_cpu_test_code, 0, 
                (unsigned long)test_code_end - (unsigned long)test_code);
        memset( per_cpu_exit_code, 0, 
                (unsigned long)exit_code_end - (unsigned long)exit_code);
        memset( per_cpu_enter_eresume_code, 0, 
                (unsigned long)enter_eresume_code_end - 
                (unsigned long)enter_eresume_code);
        memset(&region_map, 0, sizeof(region_map));
        memset(&vIDT_info, 0, sizeof(vIDT_info));
        //copy handler code
        //copy architechural handlers 0-20
        memcpy((char*)((unsigned long)per_cpu_vidt_stub), 
               (char*)MyHandler, isr_size*21);
        //copy non-arch handlers 21-255
        for (i=21; i<256; i++) { 
            memcpy( (char*)((unsigned long)per_cpu_vidt_stub + (isr_size*i)),
                (char*)MyHandler_arch_end, isr_size);
        }

        //--------setup aex flow--------------
        //copy main body of exit stub code
        memcpy( (char*)((unsigned long)per_cpu_test_code),
                (char*)test_code, (unsigned long)test_code_end -
                (unsigned long)test_code);
		

        // 6 patches - cpuindex, ptr_core_state, 2 secs ptr, secs scv, exit_page
        test_code_patch =
            (hsec_patch_info_t *)kmalloc(NUM_AEX_CODE_PATCHES * sizeof(hsec_patch_info_t),
                                    GFP_KERNEL);
        if(test_code_patch == NULL)
        {
            KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
            goto fail_msg;
        }
        //patch per cpu test code with core id
        test_code_offset = (unsigned long)test_code_cpuindex -
            (unsigned long)test_code;
#if 0
        memcpy( (char*)((unsigned long)per_cpu_test_code +
                    test_code_offset + 0x1),
           (char*)&cpuindex, sizeof(unsigned long));
#endif
        
        test_code_patch[0].val = (unsigned long)cpuindex;
        test_code_patch[0].offset = test_code_offset + MOV_OPCODE_SIZE;
        test_code_patch[0].type = PATCH_TYPE_CORE_ID;
       

        //patch per cpu test code with secs
        ADD_SECS_PATCH(1, test_code, 1);

        //patch per cpu test code with ptr_core_state
        test_code_offset = (unsigned long)test_code_ptr_core_state_patch - 
            (unsigned long)test_code;
#if 0
        memcpy( (char*)((unsigned long)per_cpu_test_code + 
                    test_code_offset + 0x1), 
                (char*)&ptr_core_state, sizeof(unsigned long));
#endif

        test_code_patch[2].val = (unsigned long)ptr_core_state;
        test_code_patch[2].offset = test_code_offset + MOV_OPCODE_SIZE;
        test_code_patch[2].type = PATCH_TYPE_CORE_STATE_PTR;


        test_code_offset = (unsigned long)test_code_cmp_patch -
            (unsigned long)test_code;

        test_code_patch[3].val = (unsigned long)~0x0; //special value to  signify that hypersim should patch by itself. 
        test_code_patch[3].offset = test_code_offset + MOV_OPCODE_SIZE;
        test_code_patch[3].type = PATCH_TYPE_SECS_SCV;

        ADD_SECS_PATCH(2, test_code, 4);

	/*FIXME noticed that the data values were being type-casted to 
	  unsigned long which will cause issues - must replace with #def 
	  data_type to adjust for 32 bit and 64 bit*/

        test_code_offset = (unsigned long)test_code_exit_page_patch -
            (unsigned long)test_code;
        test_code_patch[5].val = (unsigned long)per_cpu_exit_page;
        test_code_patch[5].offset = test_code_offset + MOV_OPCODE_SIZE;
        test_code_patch[5].type = PATCH_TYPE_EXIT_PAGE;

	region_size = REGION_SIZE(test_code);
        ADD_REGION(region_map, per_cpu_test_code,1,
                test_code_patch, NUM_AEX_CODE_PATCHES, region_size);

        set_page_exec((unsigned long)per_cpu_test_code);

        //--------setup entry flow--------------
        memcpy( (char*)((unsigned long)per_cpu_enter_eresume_code), 
                (char*)enter_eresume_code, (unsigned long)enter_eresume_code_end - 
                (unsigned long)enter_eresume_code);
			

        enter_eresume_code_patch =
            (hsec_patch_info_t *)kmalloc(NUM_ENT_RES_CODE_PATCHES * sizeof(hsec_patch_info_t),
                                    GFP_KERNEL);
        if(enter_eresume_code_patch == NULL)
        {
            KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
            goto fail_msg;
        }
        //patch per cpu enter code with core id
        enter_eresume_code_offset = (unsigned long)enter_eresume_code_cpuindex - 
            (unsigned long)enter_eresume_code;
#if 0
        memcpy( (char*)((unsigned long)per_cpu_enter_eresume_code + 
                    enter_eresume_code_offset + 0x1), 
                (char*)&cpuindex, sizeof(unsigned long));
#endif
        enter_eresume_code_patch[0].val = (unsigned long)cpuindex;
        enter_eresume_code_patch[0].offset = enter_eresume_code_offset + MOV_OPCODE_SIZE;
        enter_eresume_code_patch[0].type = PATCH_TYPE_CORE_ID;

        enter_eresume_code_offset = (unsigned long)enter_eresume_code_cmp_patch1 -
            (unsigned long)enter_eresume_code;
        enter_eresume_code_patch[1].val = (unsigned long)~0x0;
        enter_eresume_code_patch[1].offset = enter_eresume_code_offset + MOV_OPCODE_SIZE;
	enter_eresume_code_patch[1].type = PATCH_TYPE_SECS_SCV_UN;


        enter_eresume_code_offset = (unsigned long)enter_eresume_code_cmp_patch2 -
            (unsigned long)enter_eresume_code;
        enter_eresume_code_patch[2].val = (unsigned long)~0x0;
        enter_eresume_code_patch[2].offset = enter_eresume_code_offset + MOV_OPCODE_SIZE;
        enter_eresume_code_patch[2].type = PATCH_TYPE_SECS_SCV;

        ADD_SECS_PATCH(1, enter_eresume_code, 3);
        ADD_SECS_PATCH(2, enter_eresume_code, 4);
        ADD_SECS_PATCH(3, enter_eresume_code, 5);
        ADD_SECS_PATCH(4, enter_eresume_code, 6);
        ADD_SECS_PATCH(5, enter_eresume_code, 7);
        ADD_SECS_PATCH(6, enter_eresume_code, 8);

        enter_eresume_code_offset = (unsigned long)enter_eresume_code_enter_page_patch -
            (unsigned long)enter_eresume_code;
        enter_eresume_code_patch[9].val = (unsigned long)per_cpu_enter_page;
        enter_eresume_code_patch[9].offset = enter_eresume_code_offset + MOV_OPCODE_SIZE;
        enter_eresume_code_patch[9].type = PATCH_TYPE_ENTER_PAGE;

        region_size = REGION_SIZE(enter_eresume_code);
        ADD_REGION(region_map, per_cpu_enter_eresume_code, 1,
                enter_eresume_code_patch, NUM_ENT_RES_CODE_PATCHES, region_size);
        set_page_exec((unsigned long)per_cpu_enter_eresume_code);

        //--------setup exit flow--------------
        memcpy( (char*)((unsigned long)per_cpu_exit_code), 
                (char*)exit_code, (unsigned long)exit_code_end - 
                (unsigned long)exit_code);
			
        exit_code_patch =
            (hsec_patch_info_t *)kmalloc(NUM_EXIT_CODE_PATCHES * sizeof(hsec_patch_info_t),
                                    GFP_KERNEL);
        if(exit_code_patch == NULL)
        {
            KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
            goto fail_msg;
        }
        //patch per cpu exit code with core id
        exit_code_offset = (unsigned long)exit_code_cpuindex - 
            (unsigned long)exit_code;
#if 0
        memcpy( (char*)((unsigned long)per_cpu_exit_code + 
                    exit_code_offset + 0x1), 
                (char*)&cpuindex, sizeof(unsigned long));
#endif
        exit_code_patch[0].val = (unsigned long)cpuindex;
        exit_code_patch[0].offset = exit_code_offset + MOV_OPCODE_SIZE;
        exit_code_patch[0].type = PATCH_TYPE_CORE_ID;

        exit_code_offset = (unsigned long)exit_code_cmp_patch - 
            (unsigned long)exit_code;
        exit_code_patch[1].val = (unsigned long)~0x0;
        exit_code_patch[1].offset = exit_code_offset + MOV_OPCODE_SIZE;
        exit_code_patch[1].type = PATCH_TYPE_SECS_SCV;

        ADD_SECS_PATCH(1, exit_code, 2);

        exit_code_offset = (unsigned long)exit_code_exit_page_patch - 
            (unsigned long)exit_code;
        exit_code_patch[3].val = (unsigned long)per_cpu_exit_page;
        exit_code_patch[3].offset = exit_code_offset + MOV_OPCODE_SIZE;
        exit_code_patch[3].type = PATCH_TYPE_EXIT_PAGE;

        region_size = REGION_SIZE(exit_code);
        ADD_REGION(region_map, per_cpu_exit_code, 1,
                exit_code_patch, NUM_EXIT_CODE_PATCHES, region_size);
        set_page_exec((unsigned long)per_cpu_exit_code);

        //prepare vidt per core
        core_state[cpuindex].os_idtr = idtr;
//        smp_call_function_single(cpuindex, print_gdt, NULL, 0);

        KdPrint(PRINT_DEBUG, ("==============\n"));

        KdPrint(PRINT_ALWAYS, ("CPU %x IDT lim is 0x%hx\n", 
                    cpuindex, idtr.limit));
        KdPrint(PRINT_ALWAYS, ("CPU %x IDT base is 0x%llx\n", 
                    cpuindex, idtr.base));
		KdPrint(PRINT_ALWAYS, ("==============\n"));
		
#ifdef __x86_64__
        idt_start = (IA32e_IDT_GATE_DESCRIPTOR*)idtr.base;
#else
        idt_start = (IA32_IDT_GATE_DESCRIPTOR*)idtr.base;
#endif
        //print_idt(idt_start);
		KdPrint(PRINT_DEBUG, ("==============\n"));

        page_offset = (unsigned long)idtr.base & 0x00000FFF;
        if (idtr.limit > 0x1000) {
            KdPrint(PRINT_ALWAYS, ("Cannot copy idt into vidt memory\n"));
            goto fail_msg;
        }
        memcpy( (char*)((unsigned long)vidt+page_offset), 
                (char*)idtr.base, idtr.limit+1);

#ifdef __x86_64__
        vidt_start = (IA32e_IDT_GATE_DESCRIPTOR*)((unsigned long)vidt+page_offset);
#else
        vidt_start = (IA32_IDT_GATE_DESCRIPTOR*)((unsigned long)vidt+page_offset);
#endif

			
        //save os orig handlers and update our 
        //handlers in cpu specific redirect table
        read_idt(cpuindex, (unsigned long)per_cpu_vidt_stub, (void *)idt_start);
        KdPrint(PRINT_DEBUG, ("==============\n"));

        update_vidt(cpuindex, (void *)vidt_start);
        update_vidt_special_handlers(cpuindex, (void *)vidt_start);
			
        //print_idt(vidt_start);
		KdPrint(PRINT_DEBUG, ("==============\n"));

        //patch vidt handler code with vector numbers for non-arch handlers
        //from 21 to 255 and for all isrs patch vidt handler code with 
        //entrypoint of per cpu test code
        
        // Per vector, vector# and handler code address is patched
        stub_patch = (hsec_patch_info_t *)kmalloc(256*2* sizeof(hsec_patch_info_t),
                                    GFP_KERNEL);
        if(stub_patch == NULL)
        {
            KdPrint(PRINT_ALWAYS,("kmalloc failed %s: %d\n", __func__, __LINE__));
            goto fail_msg;
        }
        //VT: TODO - the following code needs change in offsets
        //(0x7 and 0xF need to be changed for 64 bit)
        // TODO: Check if stub_patch is non null
        for (i = 0 ; i < 256 ; i++)
        {
            // Fist record patch info for patching the vector #
            stub_patch[i*2].val = i;
            stub_patch[i*2].offset = i*isr_size +
                (unsigned long)vidt_stub_patch_val0 -
                (unsigned long)MyHandler +
                MOV_OPCODE_SIZE;
           stub_patch[i*2].type = PATCH_TYPE_IDT_VECTOR;

            // Now record patch info for patching the stub handler address
            stub_patch[i*2+1].offset = i*isr_size +
                (unsigned long)vidt_stub_patch_callee0 -
                (unsigned long)MyHandler + MOV_OPCODE_SIZE;
            stub_patch[i*2+1].type = PATCH_TYPE_REGION_ADDR;
            switch (i) {
                case EENTER_VECTOR:
                case ERESUME_VECTOR:
                    stub_patch[i*2+1].val    = (unsigned long)per_cpu_enter_eresume_code;
                    break;
                case EEXIT_VECTOR:
                    stub_patch[i*2+1].val    = (unsigned long)per_cpu_exit_code;
                    break;
                default:
                    stub_patch[i*2+1].val    = (unsigned long)per_cpu_test_code;
                    break;
            }
             n_stub_patches += 2;
        }

        //copy redirect table into non-pageable memory
        //note must be done before core_state redirect_addr set
#ifdef __x86_64__
        memcpy((char*)redirect_ptr + (cpuindex * 256 * sizeof(IA32e_IDT_REDIRECT_TABLE)),
                (char*)redirect[cpuindex],
                256 * sizeof(IA32e_IDT_REDIRECT_TABLE));

        core_state[cpuindex].redirect_addr =(unsigned long)(redirect_ptr +
                (cpuindex * 256 * sizeof(IA32e_IDT_REDIRECT_TABLE)));
            //(unsigned long)redirect[cpuindex];
        core_state[cpuindex].redirect_size = sizeof(IA32e_IDT_REDIRECT_TABLE) * 256;
        core_state[cpuindex].vep_max = 0; //unused remove
        core_state[cpuindex].vep_tos = 0; //unused remove
			
#else
        memcpy((char*)redirect_ptr + (cpuindex * 256 * sizeof(IA32_IDT_REDIRECT_TABLE)),
                (char*)redirect[cpuindex], 
                256 * sizeof(IA32_IDT_REDIRECT_TABLE));

        core_state[cpuindex].redirect_addr =(unsigned long)(redirect_ptr +
                (cpuindex * 256 * sizeof(IA32_IDT_REDIRECT_TABLE)));
            //(unsigned long)redirect[cpuindex];
        core_state[cpuindex].redirect_size = sizeof(IA32_IDT_REDIRECT_TABLE) * 256;
        core_state[cpuindex].vep_max = 0; //unused remove
        core_state[cpuindex].vep_tos = 0; //unused remove
			
#endif
        KdPrint(PRINT_ALWAYS, ("core state %d = 0x%x 0x%x 0x%lx 0x%llx\n", cpuindex,
                core_state[cpuindex].vep_max,
                core_state[cpuindex].vep_tos,
                core_state[cpuindex].redirect_size,
				core_state[cpuindex].redirect_addr));

        /* Note - must be done before IDT modified - copy per core state to 
         * non-pageable memory */
        memcpy( (char*)ptr_core_state, 
                (char*)core_state, 
                sizeof(PER_CORE_STATE) * MAX_CORES);


        //set up complete - now we can modify IDT via driver or hypervisor

#ifdef __x86_64__
        Idtr.base = (uint64)(vidt_start);
        Idtr.limit = sizeof(IA32e_IDT_GATE_DESCRIPTOR) * 256 - 1;
#else
        Idtr.base = (uint32)(vidt_start);
        Idtr.limit = sizeof(IA32_IDT_GATE_DESCRIPTOR) * 256 - 1;
#endif
//        smp_call_function_single(cpuindex, ia32_load_idtr, &Idtr, 1);

        // Test register vIDT with VIDT stub only...
        // TBD: Add other regions as well in this.
        ADD_REGION(region_map, per_cpu_vidt_stub, ((isr_size*256)>>12)+1,
                stub_patch, n_stub_patches, (isr_size*256));
        i = 0;
        while ((per_cpu_vidt_stub + i*0x1000) <
                (per_cpu_vidt_stub + (isr_size*256))) {
            set_page_exec((unsigned long)(per_cpu_vidt_stub + i*0x1000));
            i++;
        }

#ifdef __x86_64__
        vIDT_info.vIDT_base = (uint64_t)Idtr.base;
#else
        vIDT_info.vIDT_base = (uint32_t)Idtr.base;
#endif
        vIDT_info.vIDT_limit = Idtr.limit;
        vIDT_info.cpu       = cpuindex;
        vIDT_info.map = region_map;
        KdPrint(PRINT_ALWAYS, ("Registering vIDT for cpu %d\n", cpuindex));
        KdPrint(PRINT_ALWAYS, ("Number of patches = 0x%x\n", 
                    vIDT_info.map.region[0].n_patches));
        KdPrint(PRINT_ALWAYS, ("Creating XO mapping for 0x%llx, n_pages =  0x%x\n",
                    region_map.region[0].start_gva, region_map.region[0].n_pages));

        vIDT_info.r0stack_num_pages = (uint16_t)NUM_STACK_PAGES;
#ifdef __x86_64__
        vIDT_info.r0stack_gva_tos = (uint64_t)(per_cpu_r0stack_pages + (NUM_STACK_PAGES*0x1000) -1);
        vIDT_info.r0_enter_page = (uint64_t)(per_cpu_enter_page);
        vIDT_info.r0_exit_page = (uint64_t)(per_cpu_exit_page);
        vIDT_info.enter_eresume_code = (uint64_t) (per_cpu_enter_eresume_code);
        vIDT_info.exit_code = (uint64_t) (per_cpu_exit_code);
        vIDT_info.async_exit_code = (uint64_t) (per_cpu_test_code);
#else
	vIDT_info.r0stack_gva_tos = (uint32_t)(per_cpu_r0stack_pages + (NUM_STACK_PAGES*0x1000) -1);
        vIDT_info.r0_enter_page = (uint32_t)(per_cpu_enter_page);
        vIDT_info.r0_exit_page = (uint32_t)(per_cpu_exit_page);
        vIDT_info.enter_eresume_code = (uint32_t) (per_cpu_enter_eresume_code);
        vIDT_info.exit_code = (uint32_t) (per_cpu_exit_code);
        vIDT_info.async_exit_code = (uint32_t) (per_cpu_test_code);
#endif

	KdPrint(PRINT_ALWAYS, ("num r0 stack pages are 0x%x\n", vIDT_info.r0stack_num_pages));
	KdPrint(PRINT_ALWAYS, ("r0 stack tos is at 0x%llx\n", vIDT_info.r0stack_gva_tos));

	// WARNING - Here, we are calling reg_vIDT on the cpu for which
	// we want to change the IDT. This reduces crashes seen in MP
        // environment. If possible, we should try to remove this restriction 
        // and make hypervisor change IDT on the desired cpu.
        // Note that last argument to the smp_single* function is 1 here as
        // we want the reg_VIDT to finish before proceeding. The reason is 
        // that the hypercall makes some pages XO and if we don't wait here,
        // there is a race where somebody could write to the pages after they
        // have been marked XO by the hypersim but before the EPTs are disabled.
        // That can cause EPT write violation.
        smp_call_function_single(cpuindex, reg_vIDT, (void*)&vIDT_info, 1);

        KdPrint(PRINT_ALWAYS, ("finished registering vidt for cpu  %u, running on cpu %d\n", cpuindex, smp_processor_id()));
        //verify
#ifdef __x86_64__
        smp_call_function_single(cpuindex, ia32e_read_idtr, &idt_desc, 1);
#else
        smp_call_function_single(cpuindex, ia32_read_idtr, &idt_desc, 1);
#endif
   //     smp_call_function_single(cpuindex, print_gdt, NULL, 0);
    //    smp_call_function_single(cpuindex, print_ldt, NULL, 0);
        
        KdPrint(PRINT_ALWAYS, ("CPU %x IDT lim is 0x%hx\n", cpuindex, idt_desc.limit));
        KdPrint(PRINT_ALWAYS, ("CPU %x IDT base is 0x%llx\n", cpuindex, idt_desc.base));
		KdPrint(PRINT_ALWAYS, ("==============\n"));

#ifdef __x86_64__
        idt_start = (IA32e_IDT_GATE_DESCRIPTOR*)idt_desc.base;
#else
        idt_start = (IA32_IDT_GATE_DESCRIPTOR*)idt_desc.base;
#endif
        //print_idt(idt_start);
		KdPrint(PRINT_DEBUG, ("==============\n"));

        core_vidt_state[cpuindex].per_cpu_vidt_start = 
            (unsigned long)vidt;
        core_vidt_state[cpuindex].per_cpu_vidt_end = 
            (unsigned long)vidt + 0x1000 - 1;
        core_vidt_state[cpuindex].per_cpu_vidt_stub_start = 
            (unsigned long)per_cpu_vidt_stub;
        core_vidt_state[cpuindex].per_cpu_vidt_stub_end = 
            (unsigned long)per_cpu_vidt_stub + (isr_size * 256) - 1;
        core_vidt_state[cpuindex].per_cpu_test_code_start = 
            (unsigned long)per_cpu_test_code;
        core_vidt_state[cpuindex].per_cpu_test_code_end = 
            (unsigned long)per_cpu_test_code + ((unsigned long)test_code_end - 
                (unsigned long)test_code) - 1;

        // For test only - after XO permission, enabling this code should cause
        // write protection violation in the hypersim.
#if 0
        memcpy( (char*)((unsigned long)per_cpu_vidt_stub + EENTER_VECTOR * isr_size + 0xF), 
                        (char*)(unsigned long)&per_cpu_enter_eresume_code, 
                        sizeof(unsigned long));
#endif

        //////////////////////////////////////////////////////////
        //for UP debug uncomment break
        //break;
        //////////////////////////////////////////////////////////
    }
    put_online_cpus();
	goto end_success;

fail_msg:
    put_online_cpus();
	KdPrint(PRINT_ALWAYS, ("some allocation failed\n"));
    SAFE_FREE(per_cpu_vidt_stub);
    SAFE_FREE(per_cpu_test_code);
    SAFE_FREE(per_cpu_exit_code);
    SAFE_FREE(per_cpu_enter_eresume_code);
    SAFE_FREE(per_cpu_r0stack_pages);
    SAFE_FREE(per_cpu_enter_page);
    SAFE_FREE(per_cpu_exit_page);
    SAFE_FREE(vidt);
    SAFE_FREE(secs_ptr);
    SAFE_FREE(redirect_ptr);
    SAFE_FREE(ptr_core_state);
	return SL_EUNKNOWN;

end_success:
    return SL_SUCCESS;
}
#endif // end of !APP_MODE

