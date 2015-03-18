/*
 * The file implements vIDT ISR flows.
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

#ifndef APP_MODE
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
#endif
//#include <linux/notifier.h>
#define ACTIVATE_ACR3
#define APP_MODE 1

__asm__ ( \
	".intel_syntax noprefix\n"  \
	"patch_str_secs_ptr = 0xAABBCCDDAABBCCDD\n" \
	"patch_str_core_id =  0xBBCCDDAABBCCDDAA\n" \
	"patch_str_core_state_ptr = 0xCCDDAABBCCDDAABB\n" \
	"patch_str_secs_scv = 0xDDCCAABBDDCCAABB\n" \
	"patch_str_region_addr = 0xAACCBBDDAACCBBDD\n" \
	"patch_str_idt_vector = 0xAADDBBCCAADDBBCC\n"	\
	"patch_str_enter_page = 0xBBDDAACCBBDDAACC\n" \
	"patch_str_exit_page = 0xCCAADDBBCCAADDBB\n" \
	"patch_str_secs_scv_un = 0xDDBBAACCDDBBAACC\n" \
	".att_syntax\n" \
	);

// ***********************************************
//!!!!!!!!!!!!!!!!!! NOTE WELL !!!!!!!!!!!!!!!!!!!
//if this code is changed, the entrypoint 
//offset added to the MyHandler base in read_idt 
//must be updated to ensure the correct entrypoints
//are registered in the vIDT
//Also NOTE - mov for val is used to ensure opcode 
//does not change with operand size, so that size 
//of this code block does not change
// ************************************************
void vidt_stub_patch_val0(void);
void begin_vidt_stub0(void);
void vidt_stub_patch_callee0(void);
#define VIDT_STUB(errcode,val) \
    __asm__ ( \
            ".intel_syntax noprefix\n" \
            ".globl begin_vidt_stub0\n" \
            ".globl vidt_stub_patch_val0\n" \
            ".globl vidt_stub_patch_callee0\n" \
            \
            "begin_vidt_stub"val":\n"  \
            "push r8;\n"    \
            "push r9;\n"    \
            "push r10;\n"   \
            "push r11;\n"   \
            "push r12;\n"   \
            "push r13;\n"   \
            "push r14;\n"   \
            "push r15;\n"   \
            "push rdi;\n" \
            "push rsi;\n" \
            "push rbx;\n" \
            "push rdx;\n" \
            "push rcx;\n" \
            "push rax;\n" \
            \
            "xor rdi, rdi;\n"   \
            "vidt_stub_patch_val"val":\n"   \
            "mov rdi, patch_str_idt_vector;\n" \
            "xor rsi, rsi;\n"   \
            "mov esi, "errcode";\n" \
            "vidt_stub_patch_callee"val":\n"   \
            "mov rax, patch_str_region_addr;\n" \
            "call rax;\n" \
            \
            "cmp eax, 0;\n" \
            "je vidt_stub_use_iret"val";\n" \
            "pop rax;\n" \
            "pop rcx;\n" \
            "pop rdx;\n" \
            "pop rbx;\n" \
            "pop rsi;\n" \
            "pop rdi;\n"    \
            "pop r15;\n" \
            "pop r14;\n" \
            "pop r13;\n" \
            "pop r12;\n" \
            "pop r11;\n" \
            "pop r10;\n" \
            "pop r9;\n" \
            "xchg r8, [rsp];\n" \
            "ret;\n" \
            \
            "vidt_stub_use_iret"val":\n" \
            "pop rax;\n" \
            "pop rcx;\n" \
            "pop rdx;\n" \
            "pop rbx;\n" \
            "pop rsi;\n" \
            "pop rdi;\n" \
            "pop r15;\n" \
            "pop r14;\n" \
            "pop r13;\n" \
            "pop r12;\n" \
            "pop r11;\n" \
            "pop r10;\n" \
            "pop r9;\n" \
            "pop r8;\n" \
            "iretq;\n" \
            ".att_syntax\n" \
            );

void MyHandler(void);

__asm__ (
	".intel_syntax noprefix\n"
	".globl MyHandler\n"

	"MyHandler:\n"
	/* #DE */
	"vidt_entry_0:\n"
	);
VIDT_STUB("0","0");
__asm__ (
        //#DB
        "vidt_entry_1:\n"
        );
VIDT_STUB("0","1");
__asm__ (
        //#NMI
        "vidt_entry_2:\n"
        );
VIDT_STUB("0","2");
__asm__ (
        //#BP
        "vidt_entry_3:\n"
        );
VIDT_STUB("0","3");
__asm__ (
        //#OF
        "vidt_entry_4:\n"
        );
VIDT_STUB("0","4");
__asm__ (
        //#BR
        "vidt_entry_5:\n"
        );
VIDT_STUB("0","5");
__asm__ (
        //#UD
        "vidt_entry_6:\n"
        );
VIDT_STUB("0","6");
__asm__ (
        //#NM
        "vidt_entry_7:\n"
        );
VIDT_STUB("0","7");
__asm__ (
        //#DF 
        "vidt_entry_8:\n"
        );
VIDT_STUB("1","8"); //errcode=1
__asm__ (
        //CSO Abort
        "vidt_entry_9:\n"
        );
VIDT_STUB("0","9");
__asm__ (
        //#TS
        "vidt_entry_10:\n"
        );
VIDT_STUB("1","10"); //errcode=1
__asm__ (
        //#NP
        "vidt_entry_11:\n"
        );
VIDT_STUB("1","11"); //errcode=1
__asm__ (
        //#SS
        "vidt_entry_12:\n"
        );
VIDT_STUB("1","12"); //errcode=1
__asm__ (
        //#GP
        "vidt_entry_13:\n"
        );
VIDT_STUB("1","13"); //errcode=1
__asm__ (
        //#PF
        "vidt_entry_14:\n"
        );
VIDT_STUB("1","14"); //errcode=1
__asm__ (
        //RESV
        "vidt_entry_15:\n"
        );
VIDT_STUB("0","15");
__asm__ (
        //#MF
        "vidt_entry_16:\n"
        );
VIDT_STUB("0","16");
__asm__ (
        //#AC
        "vidt_entry_17:\n"
        );
VIDT_STUB("1","17"); //errcode=1
__asm__ (
        //#MC
        "vidt_entry_18:\n"
        );
VIDT_STUB("0","18");
__asm__ (
        //#XM
        "vidt_entry_19:\n"
        );
VIDT_STUB("0","19");
__asm__ (
        //#VE
        "vidt_entry_20:\n"
        );
VIDT_STUB("0","20");
///////////////////////////////// arch handlers end
//TBD NEED TO FILL IN OTHER ISRS uptil index 255
//THOSE ARE COPIED into memory in the code
//that sets up the per-core vIDT code pages
__asm__ (
        ".att_syntax\n"
        );
void MyHandler_arch_end(void);
__asm__ (
    ".globl MyHandler_arch_end\n"
    "MyHandler_arch_end:"
        );
VIDT_STUB("0","99"); //dummy ISR for size calculation
void MyHandler_non_arch_end(void);
__asm__ (
    ".globl MyHandler_non_arch_end\n"
    "MyHandler_non_arch_end:"
	"nop;\n"
    );

//AEX - ASYNCHRONOUS EXIT VIEW FLOW
//NOTE - expected to be called only from MyHandler code
//Expects to get a flag param on stack to skip error code 
//based on IDT vector - some entries below 20 have error code
void test_code_cpuindex(void);
void test_code_ptr_core_state_patch(void);
void test_code_secs_patch1(void);
void test_code_secs_patch2(void);
void test_code_cmp_patch(void);
void test_code_exit_page_patch(void);
void test_code(void);
__asm__ (
        ".intel_syntax noprefix\n"
        ".globl test_code_cpuindex\n"
        ".globl test_code_ptr_core_state_patch\n"
        ".globl test_code_secs_patch1\n"
        ".globl test_code_secs_patch2\n"
        ".globl test_code_cmp_patch\n"
        ".globl test_code_exit_page_patch\n"
        ".globl test_code\n"

        "params_size           =0\n"
        //stack params offsets
        "flag                  =4\n"
        "vector                =8\n"

        "gpr_frame_size        =112\n"
        //offset to gprs on stack
        "start_gprs            =params_size + 8\n"
        //offsets for gprs with base at start_gprs
        "gprs.eax               =0\n"
        "gprs.ecx               =8\n"
        "gprs.edx               =16\n"
        "gprs.ebx               =24\n"
        "gprs.esi               =32\n"
        "gprs.edi               =40\n"
        "gprs.r15               =48\n"
        "gprs.r14               =56\n"
        "gprs.r13               =64\n"
        "gprs.r12               =72\n"
        "gprs.r11               =80\n"
        "gprs.r10               =88\n"
        "gprs.r9                =96\n"
        "gprs.r8                =104\n"

        //intr_frame offsets
        "if_skip_ec            =start_gprs + gpr_frame_size + 8\n" //skips errcode
        "if_noskip_ec          =start_gprs + gpr_frame_size\n"   //does not skip errcode
        "if.errcode            =0\n" //used with if_noskip_ec offset
        //these offsets are with reference base if_skip_ec and if_noskip_ec:
        "if.eip                =0\n"
        "if.cs                 =8\n"
        "if.eflags             =16\n"
        "if.esp                =24\n"
        "if.ss                 =32\n"

        //secs offsets from arch72.h
        "secs.size             =0\n"
        "secs.base             =8\n"
        "secs.ssa_frame_size   =16\n"
        "secs.attrib           =48\n"
        "secs.eid              =260\n"
        "secs.ept_enabled      =3559\n"
        "secs.acr3             =3560\n"
        "secs.scv              =3568\n"
        "secs.os_cr3           =3576\n"
        "secs.pcd              =3584\n" //per core data
        //secs offsets for per core data
        "secs.pcd.tcs          =0\n"
        "secs.pcd.idtr         =8\n"
        "secs.pcd.gdtr         =16\n"
        "secs.pcd.ldtr         =24\n"
        "secs.pcd.tr           =32\n"
        "secs.pcd.r0sp         =40\n"

        "shift_size_secs_pcd   =6\n" //pcd size is 64 currently
        "shift_size_4k         =12\n" //ssa frame size is 4k

        //tcs offsets from arch72.h
        "tcs.state             =0\n"
        "tcs.flags             =8\n"
        "tcs.ossa              =16\n"
        "tcs.cssa              =24\n"
        "tcs.nssa              =28\n"
        "tcs.oentry            =32\n"
        "tcs.aep               =40\n"
        "tcs.ofs_base          =48\n"
        "tcs.ogs_base          =56\n"
        "tcs.ofs_limit         =64\n"
        "tcs.ogs_limit         =68\n"
        "tcs.save_fs_selector  =72\n"
        "tcs.save_fs_desc_low  =74\n"
        "tcs.save_fs_desc_high =78\n"
        "tcs.save_gs_selector  =82\n"
        "tcs.save_gs_desc_low  =84\n"
        "tcs.save_gs_desc_high =88\n"
        "tcs.ssa               =92\n"
        "tcs.eflags            =96\n"
        "tcs.os_cr3            =100\n"
        "tcs.ur0sp             =108\n"

        "tcs_state_expect_active =1\n"
        "tcs_state_set_inactive  =0\n"

        //note ssa gpr area at end of page
        "ssa_gpr_size          =168\n"
        //ssa offsets from arch72.h
        "ssa.ax                =0\n"
        "ssa.cx                =8\n"
        "ssa.dx                =16\n"
        "ssa.bx                =24\n"
        "ssa.sp                =32\n"
        "ssa.bp                =40\n"
        "ssa.si                =48\n"
        "ssa.di                =56\n"
        "ssa.r8                =64\n"
        "ssa.r9                =72\n"
        "ssa.r10               =80\n"
        "ssa.r11               =88\n"
        "ssa.r12               =96\n"
        "ssa.r13               =104\n"
        "ssa.r14               =112\n"
        "ssa.r15               =120\n"
        "ssa.flags             =128\n"
        "ssa.ip                =136\n"
        "ssa.sp_u              =144\n"
        "ssa.bp_u              =152\n"

        "cr0.ts                =3\n"
        "seg_granularity       =23\n"

        "vmfunc_view_sw_ctrl   =0\n"
        "vmfunc_return_success =0\n"

        "untrusted_view_id     =0\n"

        "vmcall_assert         =42\n"
        "vmcall_ta_exception   =0x9f\n"

        "size_core_state       =30\n" // look at PER_CORE_STATE data structure
        "redirect_table        =12\n"
        "sizeof_redirect_entry =16\n"

        "eresume_leaf          =3\n"
        "eenter_opsize         =2\n"
        
        "eenter_vector         =29\n"
        "eresume_vector        =30\n"
        "eexit_vector          =31\n"
        
        "ss_rpl_mask           =0x0003\n"
        "ss_rpl_ring3          =0x0003\n"
        "ss_ti_mask            =0x0004\n"
        "ss_ti_gdt             =0x0000\n"
        "ss_ti_ldt             =0x0004\n"
        "enclave_crashed       =0x1006\n" //error code
        "ecall_not_allowed     =0x1007\n" //error code

        "exception_pf          =14\n"
        "exception_last        =31\n"
        "exception_nmi         =2\n"
        "exception_mc          =18\n"
        "view_0_chk_fail       =0x2\n"
        "secs_scv_chk_fail     =0x3\n"
        "trusted_view_chk_fail =0x4\n"
        "trusted_view_init_fail =0x5\n"
        "tcs_pg_align_chk_fail =0x6\n"
        "ssa_frame_avl_chk1_fail =0x7\n"
        "ssa_frame_avl_chk2_fail =0x8\n"
        "aep_chk_fail            =0x9\n"
        "target_chk_fail         =0xa\n"
        "ss_db_chk_fail          =0xb\n"
        "ds_expand_chk_fail      =0xc\n"
        "ossa_page_align_chk_fail=0xd\n"
        "ofsbase_page_align_chk_fail=0xe\n"
        "ogsbase_page_align_chk_fail=0xf\n"
        "no_fs_wrap_around_chk_fail=0x10\n"
        "fs_within_ds_chk_fail     =0x11\n"
        "no_gs_wrap_around_chk_fail=0x12\n"
        "gs_within_ds_chk_fail     =0x13\n"
        "tcs_reserved_chk_fail     =0x14\n"
        "tcs_lock_acquire_fail     =0x15\n"


        "test_code:\n"

        //itp_loop for debug
        "nop;\n"
        "itp_loop_exit_code:\n"
		"mov eax, 4;\n"
		"nop;\n"
		"nop;\n"
		"nop;\n"
		"nop;\n"
		"cmp eax, 5;\n"
        "jz itp_loop_exit_code;\n"

        "test_code_cpuindex:\n"
        "mov r13, patch_str_core_id;\n" //populate core id statically
        "mov r10, rdi;\n" //r10 == vector
        "mov r11, rsi;\n" //r11 == errorcode_flag
        //r12 == os_cr3 (initialized later)
        //rcx == view_handle
        //rdx == secs
        //check static ring-0 virtual address for SECS for current view
        //verify secret value on page to ensure it is not malicious 
        "test_code_secs_patch1:\n"
        "mov rdx, patch_str_secs_ptr;\n"
        "mov rax, [rdx+secs.scv];\n" 
        "test_code_cmp_patch:\n"
        "mov r9, patch_str_secs_scv;\n"
        "cmp rax, r9;\n"
        "jz exit_code_cookie_check_ok;\n"

	/* For view0, cookie value check will fail when running
	 * in EPT environment because we do not reveal the secure
	 * cookie value to the untrusted view.
	 */
	"mov rbx, [rdx+secs.eid];\n"
	"cmp rbx, 0;\n"
	"jnz assert_invalid_secs;\n"

	/* For unstrusted view, we compare the secs->scv with untrusted
	 * scv
	 */
	"mov r9, patch_str_secs_scv_un;\n"
	"cmp rax, r9;\n"
	"jz exit_code_cookie_check_ok;\n"

	"assert_invalid_secs:\n"
	/* VMCALL here to assert only for trusted view */
	"mov eax, vmcall_assert;\n"
	"vmcall;\n"

        "exit_code_cookie_check_ok:\n"
        //if 0 do not switch views, pass control to os handler
        "mov rax, [rdx+secs.eid];\n"
        "cmp rax, 0;\n"
        "jz no_view_switch;\n"

        //we are in a trusted view

        //****************debug only start***************
        /*
        "mov ebx, edx;\n"
        "add ebx, secs.vid;\n"
        "mov ax, 1;\n" //expect view 1
        "mov cx, 0;\n" //switch vid to 0
        ".byte 0xF0;\n"
        "cmpxchg [ebx], cx;\n"
        */
        //****************debug only end***************
        "mov eax, r10d;\n"
        "cmp eax, exception_last;\n"
        "jg continue_aex_flow;\n"
        // Do not crash TA for nmi and mc exceptions
        "cmp eax, exception_nmi;\n"
        "je continue_aex_flow;\n"
        // This is an exception in TA - mark SECS.attributes.inited = 0
        "mov eax, [rdx+secs.attrib];\n"
        "and eax, ~0x1;\n" //Clear secs.attributes.inited
        "mov [rdx+secs.attrib], eax;\n"
        
        "continue_aex_flow:\n"
        //check boolean param whether to expect error code 
        //on interrupt frame or not
		"mov eax, r11d;\n"
		"cmp eax, 0;\n"
		"jz no_error_code;\n"

		//lookup CS for ring3 dpl and eip from intr frame
        //if not r3 CS - this should not happen since we should not be in 
        //non-0 view and in ring-0 CS, unless, we have ring-0 views in which
        //case we will need to save state for ring-0 code via r0 TCS

        //skip top 36 bytes of stack frame since we get called from MyHandler
		//which pushes eax, ebx, ecx, edx, esi, edi, vector, 
        //errflag, retaddr (due to call)
        "mov eax, [rsp+if_skip_ec+if.cs];\n" //cs
        "mov rbx, [rsp+if_skip_ec+if.eip];\n" //interrupted eip
		"jmp continue_test;\n"

        "no_error_code:\n"
        "mov eax, [rsp+if_noskip_ec+if.cs];\n" //cs
		"mov rbx, [rsp+if_noskip_ec+if.eip];\n" //interrupted eip

        //we have cs and eip cached in eax and ebx resp.
        "continue_test:\n"
        //BUG: "mov eax, [esp+if_noskip_ec+if.cs];\n" //cs
        "and eax, ss_rpl_mask;\n"
		//BUG: "mov ebx, [esp+if_noskip_ec+if.eip];\n" //interrupted eip
		"cmp eax, ss_rpl_ring3;\n" //cmp with user/expected CS 
		"jnz save_ret_noreplace;\n" //if not r3 CS dont touch intr frame

        /*
        //we have eip cached in ebx
        "mov eax, [edx+secs.base];\n" //get start of protected gla from secs
		"cmp ebx, eax;\n" //ensure EIP >= gla_start
		"jnae save_ret_noreplace;\n" //out_of_range if ebx !>= gla_start

        "add eax, [edx+secs.size];\n" //get start of protected gla from secs
        "cmp ebx, eax;\n" //ensure EIP <= gla_end
        "ja save_ret_noreplace;\n" //out of range if ebx > gla_end
        */

        //we have to replace EIP on stack with AEP
        //and replace RSP on stack with external stack from TCS
        "jmp test_code_get_ssa;\n"

        "save_ret_noreplace:\n"
        "mov eax, vmcall_assert;\n"
        "vmcall;\n"

        "test_code_get_ssa:\n"
        //save original EIP (intr point) into SSA with RSP and other GPRS
        //all required gprs, esp, eip values should be on trusted stack

        //Establish ptr to TCS
        "xor rcx, rcx;\n"
        "mov rbx, rdx;\n"
        "add rbx, secs.pcd;\n"
        "mov ecx, r13d;\n" //core id cached in r13
        "shl ecx, shift_size_secs_pcd;\n"
        "add rcx, rbx;\n" //ptr to core-specific pcd
        "mov ebx, [rcx+secs.pcd.tcs];\n"

        //Establish ptr to SSA frame (from TCS)
        //TCS is in ebx, OSSA, CSSA is a field in TCS struct
        //FIXME MAYBE - this could be done in EENTER, ERESUME
        //in which case this will require only one read from TCS
        "mov rcx, [ebx+tcs.ossa];\n"   //ossa
        "mov rax, [rdx+secs.base];\n"  //base
        "add rcx, rax;\n" //ssa_va = ossa+base
        "xor rax, rax;\n" // clear rax for new production
        "mov eax, [ebx+tcs.cssa];\n"   //cssa
        "mov rsi, [rdx+secs.ssa_frame_size];\n" //ssa frame size
        "shl rsi, shift_size_4k;\n" //ssa frame size is in pages
        "mul rsi;\n" //size*cssa - result in rdx:rax
        "add rax, rcx;\n" //add base FIXME LATER higher 32 bits rdx ignored

        // XSAVE and clean
        //xsave registers on ssa starting from
        //the beginning.
        "fxsave64 [rax];\n"
        //Clear the fx registers by restoring value in them from
        //ssa  +512 byte offset.
        "fxrstor64 [rax+512];\n"
        //gpr area at end of ssa page
        "add rax, 4096;\n"
        "sub rax, ssa_gpr_size;\n"

        //ensure order accessed on stack is same as saved from MyHandler
        "mov rcx, [rsp+start_gprs+gprs.eax];\n" //save eax
        "mov [rax+ssa.ax], rcx;\n"
        "mov rcx, [rsp+start_gprs+gprs.ecx];\n" //save ecx
        "mov [rax+ssa.cx], ecx;\n"
        "mov rcx, [rsp+start_gprs+gprs.edx];\n" //save edx
        "mov [rax+ssa.dx], rcx;\n"
        "mov rcx, [rsp+start_gprs+gprs.ebx];\n" //save ebx
        "mov [rax+ssa.bx], rcx;\n"
        "mov rcx, [rsp+start_gprs+gprs.esi];\n" //save esi
        "mov [rax+ssa.si], rcx;\n"
        "mov rcx, [rsp+start_gprs+gprs.edi];\n" //save edi
        "mov [rax+ssa.di], rcx;\n"

        "mov rcx, [rsp+start_gprs+gprs.r8];\n" //save r8
        "mov [rax+ssa.r8], rcx;\n"
        "mov rcx, [rsp+start_gprs+gprs.r9];\n" //save r9
        "mov [rax+ssa.r9], rcx;\n"
        "mov rcx, [rsp+start_gprs+gprs.r10];\n" //save r10
        "mov [rax+ssa.r10], rcx;\n"
        "mov rcx, [rsp+start_gprs+gprs.r11];\n" //save r11
        "mov [rax+ssa.r11], rcx;\n"
        "mov rcx, [rsp+start_gprs+gprs.r12];\n" //save r12
        "mov [rax+ssa.r12], rcx;\n"
        "mov rcx, [rsp+start_gprs+gprs.r13];\n" //save r13
        "mov [rax+ssa.r13], rcx;\n"
        "mov rcx, [rsp+start_gprs+gprs.r14];\n" //save r14
        "mov [rax+ssa.r14], rcx;\n"
        "mov rcx, [rsp+start_gprs+gprs.r15];\n" //save r15
        "mov [rax+ssa.r15], rcx;\n"

        "mov [rax+ssa.bp], rbp;\n"    //save ebp directly since we dont clobber

        //Again, check boolean flag param whether to expect error code 
        //on interrupt frame or not
        "mov ecx, r11d ;\n"
        "cmp ecx, 0;\n"
        "jz save_eflags_no_errcode;\n"

        "mov ecx, [rsp+if_skip_ec+if.eflags];\n" //save eflags after skipping error code
        "mov [rax+ssa.flags], ecx;\n"     //FIXME LATER need to scrub eflags right

        "mov rcx, [rsp+if_skip_ec+if.eip];\n" //save eip
        "mov [rax+ssa.ip], rcx;\n"
        "mov rcx, [ebx+tcs.aep];\n"
        "mov [rsp+if_skip_ec+if.eip], rcx;\n" //replace eip with aep

        "mov rcx, [rsp+if_skip_ec+if.esp];\n" //save esp
        "mov [rax+ssa.sp], rcx;\n"
        "mov rcx, [eax+ssa.sp_u];\n" 
        "mov [rsp+if_skip_ec+if.esp], rcx;\n" //replace esp with u_esp
        "jmp continue_ssa_save;\n"

        //TODO: remove the duplication and give skip/noskip by macro parameter
        "save_eflags_no_errcode:\n"
        "mov ecx, [rsp+if_noskip_ec+if.eflags];\n" //save eflags - no error code
        "mov [rax+ssa.flags], ecx;\n"

        "mov rcx, [rsp+if_noskip_ec+if.eip];\n" //save eip
        "mov [rax+ssa.ip], rcx;\n"
        "mov rcx, [ebx+tcs.aep];\n"
        "mov [rsp+if_noskip_ec+if.eip], rcx;\n" //replace eip with aep

        "mov rcx, [rsp+if_noskip_ec+if.esp];\n" //save esp 
        "mov [rax+ssa.sp], rcx;\n"
        "mov rcx, [eax+ssa.sp_u];\n"
        "mov [rsp+if_noskip_ec+if.esp], rcx;\n" //replace esp with u_esp

        "continue_ssa_save:\n"
        //FIXME LATER - update exit_info struct at offset 160 

        //update CSSA in TCS
        "mov ecx, [ebx+tcs.cssa];\n" //cssa
        "inc ecx;\n"
        "mov [ebx+tcs.cssa], ecx;\n"


        //put synthetic state on stack for ESP=ESP_U, EBP=EBP_U and EIP=AEP_U
        //SWITCH stacks to RSP_U and copy interupt frame on RSP_u from RSP_t
        //add synthetic state for GPRs and add 8 for vector and flag so that 
        //RSP_u is setup such that whether view switch happens or not
        //code following no_view_switch label always does the same thing:
        //pops passed params, and transfers control to MyHandler which
        //pops synthetic (or real values of GPRs) and rets to OS ISR via edx

        /*
        //bug no need to modify r0 stack - we have already modified esp on stack
        //we still need to scrub registers as done below...
        "mov esi, esp;\n"
        "mov esp, [eax+ssa.sp_u];\n" //[eax+144] references esp_u
        */ 
        "mov rbp, [rax+ssa.bp_u];\n" //[eax+152] references ebp_u
        
        //-----No More trusted stack accesses after this point---------

        //need to setup u_stack frame the same way as non_vs exit
        //push interrupt frame - aep and rsp have already been updated on frame
        //push synthetic gprs which will be popped by MyHandler code
        //push dummy vector, flag, MyHandler offset after call to this function
        //ebx has ptr to trusted esp
        "mov ecx, eresume_leaf;\n"
        "mov [rsp+start_gprs+gprs.eax], ecx;\n" //eax=eresume leaf
        "mov ecx, [ebx+tcs.aep];\n"
        "mov [rsp+start_gprs+gprs.ecx], ecx;\n" //ecx=AEP
        "mov ecx, 0;\n"
        "mov [rsp+start_gprs+gprs.edx], ecx;\n" //copy 0 edx
        "mov ecx, ebx;\n"
        "mov [rsp+start_gprs+gprs.ebx], ecx;\n" //ebx=TCS
        "mov rcx, 0;\n"
        "mov [rsp+start_gprs+gprs.r8], rcx;\n" //copy 0 r8
        "mov [rsp+start_gprs+gprs.r9], rcx;\n" //copy 0 r9
        "mov [rsp+start_gprs+gprs.r10], rcx;\n" //copy 0 r10
        "mov [rsp+start_gprs+gprs.r11], rcx;\n" //copy 0 r11
        "mov [rsp+start_gprs+gprs.r12], rcx;\n" //copy 0 r12
        "mov [rsp+start_gprs+gprs.r13], rcx;\n" //copy 0 r13
        "mov [rsp+start_gprs+gprs.r14], rcx;\n" //copy 0 r14
        "mov [rsp+start_gprs+gprs.r15], rcx;\n" //copy 0 r15
        // Check if the vector indicates an exception.
        // if (vector == exception)
        // Copy error code in gprs.esi
        // Copy faulting IP in gprs.edi
        // else
        // copy 0 to esi and 0 to edi
        "mov [rsp+start_gprs+gprs.esi], rcx;\n" //copy 0 esi
        "mov [rsp+start_gprs+gprs.edi], rcx;\n" //copy 0 edi
        "mov esi, r10d;\n"
        "cmp esi, exception_last;\n"
        "jg no_trusted_view_exception;\n"

        "cmp esi, exception_nmi;\n"
        "je no_trusted_view_exception;\n"

        "mov ecx, enclave_crashed;\n"
        "mov [rsp+start_gprs+gprs.esi], ecx;\n" //copy error code to  esi
        //TODO: Make sure that faulting ip register size is consistent with expected
        //value by the sdk
        "mov ecx, [eax+ssa.ip];\n"
        "mov [rsp+start_gprs+gprs.edi], ecx;\n" //copy faulting address to edi 
        "mov [rsp+start_gprs+gprs.edx], esi;\n" // Copy error code to  edx
        "no_trusted_view_exception:\n"
       
	//STACKFIX Ravi - at this point the stack frame is ready to be used
	//### get address of exit page
        "test_code_exit_page_patch:\n"
	"mov rax, patch_str_exit_page;\n"
	"mov r15, rax;\n"
	//### bulk copy stack frame from trusted r0 stack to exit-page <rw>
	"pop rax;\n" //caller rip
	"mov [r15], rax;\n"
	"pop rax;\n" //rax
	"mov [r15+start_gprs+gprs.eax], rax;\n"
	"pop rax;\n" //rcx
	"mov [r15+start_gprs+gprs.ecx], rax;\n"
	"pop rax;\n" //rdx
	"mov [r15+start_gprs+gprs.edx], rax;\n"
	"pop rax;\n" //rbx
	"mov [r15+start_gprs+gprs.ebx], rax;\n"
	"pop rax;\n" //rsi
	"mov [r15+start_gprs+gprs.esi], rax;\n"
	"pop rax;\n" //rdi
	"mov [r15+start_gprs+gprs.edi], rax;\n"
	"pop rax;\n" //r15
	"mov [r15+start_gprs+gprs.r15], rax;\n"
	"pop rax;\n" //r14
	"mov [r15+start_gprs+gprs.r14], rax;\n"
	"pop rax;\n" //r13
	"mov [r15+start_gprs+gprs.r13], rax;\n"
	"pop rax;\n" //r12
	"mov [r15+start_gprs+gprs.r12], rax;\n"
	"pop rax;\n" //r11
	"mov [r15+start_gprs+gprs.r11], rax;\n"
	"pop rax;\n" //r10
	"mov [r15+start_gprs+gprs.r10], rax;\n"
	"pop rax;\n" //r9
	"mov [r15+start_gprs+gprs.r9], rax;\n"
	"pop rax;\n" //r8
	"mov [r15+start_gprs+gprs.r8], rax;\n"
        //### check for err code
	"mov eax, r11d;\n"
	"cmp eax, 0;\n"
	"jz no_err_code_to_copy;\n"

        "pop rax;\n" //errcode
        "mov [r15+if_noskip_ec+if.errcode], rax;\n"
        "pop rax;\n"//eip
	"mov [r15+if_skip_ec+if.eip], rax;\n" //eip
	"pop rax;\n" //cs
	"mov [r15+if_skip_ec+if.cs], rax;\n"
	"pop rax;\n" //rflags
	"mov [r15+if_skip_ec+if.eflags], rax;\n"
	"pop rax;\n" //rsp
	"mov [r15+if_skip_ec+if.esp], rax;\n"
	"pop rax;\n" //ss
	"mov [r15+if_skip_ec+if.ss], rax;\n"
	"jmp continue_with_copy;\n"
	
	"no_err_code_to_copy:\n"
	"pop rax;\n" //eip
	"mov [r15+if_noskip_ec+if.eip], rax;\n"
	"pop rax;\n" //cs
	"mov [r15+if_noskip_ec+if.cs], rax;\n"
	"pop rax;\n" //rflags
	"mov [r15+if_noskip_ec+if.eflags], rax;\n"
	"pop rax;\n" //rsp
	"mov [r15+if_noskip_ec+if.esp], rax;\n"
	"pop rax;\n" //ss
	"mov [r15+if_noskip_ec+if.ss], rax;\n"
	
	"continue_with_copy:\n"	
	//#### adjust untrusted rsp to remove frame - already done above in pops
	//#### (do we need to - will always be used from tss->rsp0)
	//#### cache untrusted r0 stack ptr from TCS?? into r14
	"mov r14, [ebx+tcs.ur0sp];\n" 
 
	//FIXME? how come we are accessing TCS after cr3 switched back??
	//cr3 switch should be right before vmfunc out

        //Make TCS state inactive
        //lock cmpxchg tcs.state from expect_active to  set_inactive
        "mov eax, tcs_state_expect_active;\n"
        "mov ecx, tcs_state_set_inactive;\n"
        ".byte 0xF0;\n"
        "cmpxchg [ebx], ecx;\n"
        "je tcs_state_inactive_ok;\n"

        "mov eax, vmcall_assert;\n"
        "vmcall;\n" 
        
        "tcs_state_inactive_ok:\n"
        //we need to save the FS, GS base and limit on EENTER into TCS
        //we access gdt and ldt from a SECS cached location cached during creation
        //so that we dont have to perform SGDT again (also exit will occur)
        //FIXME LATER - assumes FS, GS is always pointing to GDT and not LDT

        //r13d has core id cached, esi is available
        "test_code_secs_patch2:\n"
        "mov rdx, patch_str_secs_ptr;\n" //since edx was clobbered in the mul above
        "mov rsi, rdx;\n"
        "add rsi, secs.pcd;\n"
        "mov rcx, r13;\n" //we need to cache core id in r13 
        "shl ecx, shift_size_secs_pcd;\n"
        "add rsi, rcx;\n" //get ptr to core-specific pcd

	/* swap fs */
	"mov rax, [rsi+secs.pcd.gdtr];\n"
	"xor rcx, rcx;\n"
	"mov cx, [ebx+tcs.save_fs_selector];\n"
	"mov edx, ecx;\n"
	"and ecx, 0xFFF8;\n" /* shift TI and RPL fields out and mul by 8 */
	"and edx, 4;\n" /* fs.TI */
	"cmp edx, 0;\n"
	"je  cont_fs_restore_aex;\n"
	"mov rax, [rsi+secs.pcd.ldtr];\n" /* restore in LDT */

	"cont_fs_restore_aex:\n"
	"add rax, rcx;\n"
	"mov ecx, [ebx+tcs.save_fs_desc_low];\n"
	"mov [rax], ecx;\n"
	"mov ecx, [ebx+tcs.save_fs_desc_high];\n"
	"mov [rax+4], ecx;\n"

	"xor rcx, rcx;\n"
	"mov cx, [ebx+tcs.save_fs_selector];\n"
	"mov fs, ecx;\n"

	/* swap gs */
	"mov rax, [rsi+secs.pcd.gdtr];\n"
	"xor rcx, rcx;\n"
	"mov cx, [ebx+tcs.save_gs_selector];\n"
	"mov edx, ecx;\n"
	"shr ecx, 3;\n" /* shift TI and RPL fields out */
	"shl ecx, 3;\n" /* selector x 8 (bytes) */
	"and edx, 4;\n" /* gs.TI */
	"cmp edx, 0;\n"
	"je  cont_gs_restore_aex;\n"
	"mov rax, [rsi+secs.pcd.ldtr];\n" /* restore GS in LDT */

	"cont_gs_restore_aex:\n"
	"add rax, rcx;\n"
	"mov ecx, [ebx+tcs.save_gs_desc_low];\n"
	"mov [rax], ecx;\n"
	"mov ecx, [ebx+tcs.save_gs_desc_high];\n"
	"mov [rax+4], ecx;\n"

        "xor rcx, rcx;\n"
        "mov cx, [ebx+tcs.save_gs_selector];\n"
        "mov gs, ecx;\n"
        //-----No More TCS accesses after this point---------
#ifdef ACTIVATE_ACR3
        //switch CR3 to CR3_u from TCS
        "mov rax, [ebx+tcs.os_cr3];\n"
        "mov cr3, rax;\n"
#endif

        //vmfunc to view 0 always (exit) - this could be on a seperate page

        "mov rax, vmfunc_view_sw_ctrl;\n"
        "mov rcx, untrusted_view_id;\n"
        //"vmcall;\n" //VMCALL only for debug
#if 1
        //Assembler on our dev machine does not support
        //vmfunc instruction - so writing byte code sequence
        ".byte 0x0f;\n"
        ".byte 0x01;\n"
        ".byte 0xd4;\n"
#else
        "vmfunc;\n"
#endif

	//STACKFIX Ravi - at this point stack frame is copied to exit-page r15 <ro>
	//### copy stack frame from exit-page to untrusted r0 stack (r14)
	//### alternately tss has remapped to os original so we could also read tss->esp0 to use	
	//### pivot stack to untrusted stack cached in r14
	"mov rsp, r14;\n"	
        //### check for err code
	"mov eax, r11d;\n"
	"cmp eax, 0;\n"
	"jz no_err_code_to_copy_2;\n"

	"mov rax, [r15+if_skip_ec+if.ss];\n"
	"push rax;\n" //ss
	"mov rax, [r15+if_skip_ec+if.esp];\n"
	"push rax;\n" //rsp
	"mov rax, [r15+if_skip_ec+if.eflags];\n"
	"push rax;\n" //rflags
	"mov rax, [r15+if_skip_ec+if.cs];\n"
	"push rax;\n" //cs
	"mov rax, [r15+if_skip_ec+if.eip];\n" //eip
        "push rax;\n"//eip
	"mov rax, [r15+if_noskip_ec+if.errcode];\n"
	"push rax;\n" //errcode
        
	"jmp continue_with_copy_2;\n"
	"no_err_code_to_copy_2:\n"
	"mov rax, [r15+if_noskip_ec+if.ss];\n"
	"push rax;\n" //ss
	"mov rax, [r15+if_noskip_ec+if.esp];\n"
	"push rax;\n" //rsp
	"mov rax, [r15+if_noskip_ec+if.eflags];\n"
	"push rax;\n" //rflags
	"mov rax, [r15+if_noskip_ec+if.cs];\n"
	"push rax;\n" //cs
	"mov rax, [r15+if_noskip_ec+if.eip];\n" //eip
        "push rax;\n"//eip

	"continue_with_copy_2:\n"
	"mov rax, [r15+start_gprs+gprs.r8];\n"
	"push rax;\n" //r8
	"mov rax, [r15+start_gprs+gprs.r9];\n"
	"push rax;\n" //r9
	"mov rax, [r15+start_gprs+gprs.r10];\n"
	"push rax;\n" //r10
	"mov rax, [r15+start_gprs+gprs.r11];\n"
	"push rax;\n" //r11
	"mov rax, [r15+start_gprs+gprs.r12];\n"
	"push rax;\n" //r12
	"mov rax, [r15+start_gprs+gprs.r13];\n"
	"push rax;\n" //r13
	"mov rax, [r15+start_gprs+gprs.r14];\n"
	"push rax;\n" //r14
	"mov rax, [r15+start_gprs+gprs.r15];\n"
	"push rax;\n" //r15
	"mov rax, [r15+start_gprs+gprs.edi];\n"
	"push rax;\n" //rdi
	"mov rax, [r15+start_gprs+gprs.esi];\n"
	"push rax;\n" //rsi
	"mov rax, [r15+start_gprs+gprs.ebx];\n"
	"push rax;\n" //rbx
	"mov rax, [r15+start_gprs+gprs.edx];\n"
	"push rax;\n" //rdx
	"mov rax, [r15+start_gprs+gprs.ecx];\n"
	"push rax;\n" //rcx
	"mov rax, [r15+start_gprs+gprs.eax];\n"
	"push rax;\n" //rax
	"mov rax, [r15];\n"
	"push rax;\n" //caller rip

        //if (vector == to_be_handled_exception)
        //else, jmp no_view_switch
        "mov esi, r10d;\n"
        "cmp esi, exception_last;\n"
        "jg after_view_switch;\n"

        // No TA exception handling
        "cmp esi, exception_nmi;\n"
        "je no_view_switch;\n"
        "cmp esi, exception_mc;\n"
        "je after_view_switch;\n"

        "jmp exit_ta_exception;\n"
        
        "after_view_switch:\n"
        "no_view_switch:\n" //if no view switch, no cr3 or stack switch either

        //get redirect base address for this core
        "test_code_ptr_core_state_patch:\n"
        "mov rbx, patch_str_core_state_ptr;\n"
        "xor rax, rax;\n"
        "xor rcx, rcx;\n"
        "mov eax, r13d;\n"   //patch core-id during vidt setup
		"mov  ecx, size_core_state;\n"
        "mul ecx;\n" // *** edx <- high order bits, but multiplication is small. ***
        "add rbx, rax;\n" //get ptr to per core state - retain in ebx
        //fetch OS ISR address and cache in r8
        "mov r9, [rbx + redirect_table];\n" //get redirect base address for this core
        "mov ecx, r10d;\n" //ecx = vector
        "mov eax, sizeof_redirect_entry;\n"
        "mul ecx;\n" // eax contains the multiplied output, edx is ignored as multiplication is small
        "mov r8, [rax+r9];\n" //r8 = os isr address

        "mov eax, 1;\n" //to indicate to MyHandler to use ret
		"ret;\n" 
        //pop errflag and vector passed by MyHandler entry code
		//NOTE edi has the os original handler address 
        //MyHandler will xchg edi with TOS and ret to kernel ISR cleanly

        //Ravi - keep this block - it us unused for now
        //Ravi - it is used to check result after pf to add page to acr3
        //special exit to not touch MyHandler ISR - to be able
        //to conditionally pop error code for events like PF!!
        "exit_ta_exception:\n"
        "cmp r11d, 0;\n"
        "je exit_no_error_code_pop;\n"
        "pop rax;\n" // pop vidt stub rip
        "pop rax;\n"
        "pop rcx;\n"
        "pop rdx;\n"
        "pop rbx;\n"
        "pop rsi;\n"
        "pop rdi;\n"
        "pop r15;\n"
        "pop r14;\n"
        "pop r13;\n"
        "pop r12;\n"
        "pop r11;\n"
        "pop r10;\n"
        "pop r9;\n"
        "pop r8;\n"
        "add rsp, 8;\n" //pop error code without killing gprs!
        "iretq;\n"

        "exit_no_error_code_pop:\n"
        "pop rax;" //pop vidt stub rip
        "pop rax;\n"
        "pop rcx;\n"
        "pop rdx;\n"
        "pop rbx;\n"
        "pop rsi;\n"
        "pop rdi;\n"
        "pop r15;\n"
        "pop r14;\n"
        "pop r13;\n"
        "pop r12;\n"
        "pop r11;\n"
        "pop r10;\n"
        "pop r9;\n"
        "pop r8;\n"

        "iretq;\n"
        ".att_syntax\n"
        );
void test_code_end(void);
__asm__ (
    ".globl test_code_end\n"
    "test_code_end:"

	"nop;\n"
    );

//EEXIT VIEW FLOW
//NOTE - expected to be called only from MyHandler code
//Expects to get a flag param on stack to skip error code 
//based on IDT vector - some entries below 20 have error codei
//This exit flow is performed through a ring-0 trampoline memory pages 
//referenced via the asserted page table setup for the Trusted View by 
//the initialization flow. This flow is initiated by software executing 
//inside the TV using an INT opcode that transfers control to the 
//trusted ring-0 trampoline code.
//Pre-conditions:
//Executing inside in trusted view
//GPRs passed in are the same as EEXIT (RAX (in) 04h, RBX (in) holds 
//the address to branch outside the trusted view, RCX (out) returns AEP)
//Note - Responsibility of tRTS software to clear out GPR state and swap 
//stack to OS-external stack from TCS and then invoke this flow via INT 

void exit_code_cpuindex(void);
void exit_code_cmp_patch(void);
void exit_code_secs_patch1(void);
void exit_code_exit_page_patch(void);
void exit_code(void);
__asm__ (
        ".intel_syntax noprefix\n"
        ".globl exit_code_cpuindex\n"
        ".globl exit_code\n"
        ".globl exit_code_cmp_patch\n"
        ".globl exit_code_secs_patch1\n"
        ".globl exit_code_exit_page_patch\n"

        //NOTE WELL - this routine uses all the defines from
        //test_code (async exit flow)

        "exit_code:\n"

        //itp_loop for debug
        "nop\n"
        "exit_code_itp_loop:\n"
		"mov eax, 3;\n"
		"nop;\n"
		"nop;\n"
		"nop;\n"
		"nop;\n"
		"cmp eax, 5;\n"
        "jz exit_code_itp_loop;\n"

        "exit_code_step_1:\n"
        //check static ring-0 virtual address for SECS for current view
        //verify secret value on page to ensure it is not malicious 
        "exit_code_secs_patch1:\n"
        "mov rdx, patch_str_secs_ptr;\n"
        "mov r10, rdi;\n" //r10 == vector
        "mov r11, rsi;\n" //r11 == errorcode_flag
        "mov rax, [rdx+secs.scv];\n" 
        "exit_code_cmp_patch:\n"
        "mov r9, patch_str_secs_scv;\n"
        "cmp rax, r9;\n"
        "jz exit_code_step_2;\n"

        //VMCALL here to assert
        "mov eax, vmcall_assert;\n"
        "vmcall;\n"

        "exit_code_step_2:\n"
        //if 0 then assert
        "mov rax, [rdx+secs.eid];\n"
        "cmp rax, 0;\n"
        "jne exit_code_step_3;\n"

        //VMCALL here to assert
        "mov eax, vmcall_assert;\n"
        "vmcall;\n"

        //we are in a trusted view

        "exit_code_step_3:\n"
        //****************debug only start***************
        /*
        "mov ebx, edx;\n"
        "add ebx, secs.vid;\n"
        "mov ax, 1;\n" //expect view 1
        "mov cx, 0;\n" //switch vid to 0
        ".byte 0xF0;\n"
        "cmpxchg [ebx], cx;\n"
        */
        //****************debug only end***************

        //ignore boolean param whether to expect error code 
        //on interrupt frame or not - since we know this is a sw int flow

        //lookup CS for ring3 dpl and eip from intr frame
        //if not r3 CS - this should not happen since we should not be in 
        //non-0 view and in ring-0 CS, unless, we have ring-0 views

        "mov rax, [rsp+if_noskip_ec+if.cs];\n" //cs
        "and rax, ss_rpl_mask;\n"
        "mov rbx, [rsp+if_noskip_ec+if.eip];\n" //interrupted eip
        "cmp rax, ss_rpl_ring3;\n" //cmp with user/expected CS 
        "jz exit_code_step_4;\n" //if not r3 CS dont touch intr frame

        "mov eax, vmcall_assert;\n"
        "vmcall;\n" 

        "exit_code_step_4:\n"
        //we have eip cached in ebx
        "mov rax, [rdx+secs.base];\n" //get start of protected gla from secs
        "cmp rbx, rax;\n" //ensure EIP >= gla_start
        "jae exit_code_step_4b;\n" 
        
        "mov eax, vmcall_assert;\n"
        "vmcall;\n" 

        "exit_code_step_4b:\n"
        "add rax, [rdx+secs.size];\n" //get start of protected gla from secs
        "cmp rbx, rax;\n" //ensure EIP <= gla_end
        "jbe exit_code_step_5;\n" 

        "mov eax, vmcall_assert;\n"
        "vmcall;\n" 
        
        "exit_code_step_5:\n"
        //we have to replace EIP on stack with RBX (desired target)

        //Establish ptr to TCS to read AEP (we need to pass it back in RCX)
        "mov rbx, rdx;\n"
        "add rbx, secs.pcd;\n"
        "exit_code_cpuindex:\n"
        "mov rdi, patch_str_core_id;\n" //populate core id statically
        "shl rdi, shift_size_secs_pcd;\n"
        "add rdi, rbx;\n" //ptr to core-specific pcd
        "mov rbx, [rdi+secs.pcd.tcs];\n"

        //setup RCX to contain AEP, fills ecx on untrusted stack setup
        //by MyHandler
        "mov rcx, [rbx+tcs.aep];\n"
        "mov [rsp+start_gprs+gprs.ecx], rcx;\n" //ecx=AEP

        //cache os-cr3 from tcs into gpr FIXME LATER
        "mov rsi, [rbx+tcs.os_cr3];\n"

        //Make TCS state inactive
        //lock cmpxchg tcs.state from expect_active to  set_inactive
        "mov rax, tcs_state_expect_active;\n"
        "mov rcx, tcs_state_set_inactive;\n"
        ".byte 0xF0;\n"
        "cmpxchg [rbx], rcx;\n"
        "je exit_code_step_6;\n"

        "mov eax, vmcall_assert;\n"
        "vmcall;\n"

        "exit_code_step_6:\n"
        //we need to save the FS, GS base and limit on EENTER into TCS
        //we access gdt and ldt from a SECS cached location cached during creation
        //so that we dont have to perform SGDT again (also exit will occur)
        //FIXME LATER - assumes FS, GS is always pointing to GDT and not LDT
        
        //swap fs
        //edi has core-specific pcd
	"mov rax, [rdi+secs.pcd.gdtr];\n" /* gdtr in eax */
	"xor rcx, rcx;\n"
	"mov cx, [rbx+tcs.save_fs_selector];\n"
	"mov edx, ecx;\n"
	"and rcx, 0xFFF8;\n" /* shift TI and RPL fields out and mul by 8 */
	"and edx, 4;\n" /* fs.TI */
	"cmp edx, 0;\n"
	"je cont_fs_restore_exit;\n"
	"mov rax,[rdi+secs.pcd.ldtr];\n" /* restore in LDT */

	"cont_fs_restore_exit:\n"
	"add rax, rcx;\n"
	"mov rcx, [rbx+tcs.save_fs_desc_low];\n"
	"mov [rax], rcx;\n"
	"mov rcx, [rbx+tcs.save_fs_desc_high];\n"
	"mov [rax+4], rcx;\n"

        "xor rcx, rcx;\n"
        "mov cx, [rbx+tcs.save_fs_selector];\n"
        "mov fs, cx;\n"

	/* swap gs */
	"mov rax, [rdi+secs.pcd.gdtr];\n" /* gdtr in eax */
	"xor rcx, rcx;\n"
	"mov cx, [rbx+tcs.save_gs_selector];\n"
	"mov edx, ecx;\n"
	"shr rcx, 3;\n" /* shift TI and RPL fields out */
	"shl rcx, 3;\n" /* selector x 8 (bytes) */
	"and edx, 4;\n" /* gs.TI */
	"cmp edx, 0;\n"
	"je cont_gs_restore_exit;\n"
	"mov rax,[rdi+secs.pcd.ldtr];\n" /* restore gs in LDT */

	"cont_gs_restore_exit:\n"
        "add rax, rcx;\n"
        "mov rcx, [rbx+tcs.save_gs_desc_low];\n"
        "mov [rax], rcx;\n"
        "mov rcx, [rbx+tcs.save_gs_desc_high];\n"
        "mov [rax+4], rcx;\n"

        "xor ecx, ecx;\n"
        "mov cx, [ebx+tcs.save_gs_selector];\n"
        "mov gs, ecx;\n"

        //set desired target for iret
        "mov rcx, [rsp+start_gprs+gprs.ebx];\n" //tRTS passed desired target in ebx
        "mov [rsp+if_noskip_ec+if.eip], rcx;\n" //replace eip with desired target
        //rsp on stack should already by untrusted stack (switched by tRTS)

	//STACKFIX Ravi - we need to move the bulk frame to exit-page
	//### get location of exit-page <rw>
        "exit_code_exit_page_patch:\n"
	"mov rax, patch_str_exit_page;\n"
	"mov r15, rax;\n"
	//### bulk copy stack frame from trusted r0 stack to exit-page <rw>
	"pop rax;\n" //caller rip
	"mov [r15], rax;\n"
	"pop rax;\n" //rax
	"mov [r15+start_gprs+gprs.eax], rax;\n"
	"pop rax;\n" //rcx
	"mov [r15+start_gprs+gprs.ecx], rax;\n"
	"pop rax;\n" //rdx
	"mov [r15+start_gprs+gprs.edx], rax;\n"
	"pop rax;\n" //rbx
	"mov [r15+start_gprs+gprs.ebx], rax;\n"
	"pop rax;\n" //rsi
	"mov [r15+start_gprs+gprs.esi], rax;\n"
	"pop rax;\n" //rdi
	"mov [r15+start_gprs+gprs.edi], rax;\n"
	"pop rax;\n" //r15
	"mov [r15+start_gprs+gprs.r15], rax;\n"
	"pop rax;\n" //r14
	"mov [r15+start_gprs+gprs.r14], rax;\n"
	"pop rax;\n" //r13
	"mov [r15+start_gprs+gprs.r13], rax;\n"
	"pop rax;\n" //r12
	"mov [r15+start_gprs+gprs.r12], rax;\n"
	"pop rax;\n" //r11
	"mov [r15+start_gprs+gprs.r11], rax;\n"
	"pop rax;\n" //r10
	"mov [r15+start_gprs+gprs.r10], rax;\n"
	"pop rax;\n" //r9
	"mov [r15+start_gprs+gprs.r9], rax;\n"
	"pop rax;\n" //r8
	"mov [r15+start_gprs+gprs.r8], rax;\n"
	"pop rax;\n" //eip
	"mov [r15+if_noskip_ec+if.eip], rax;\n"
	"pop rax;\n" //cs
	"mov [r15+if_noskip_ec+if.cs], rax;\n"
	"pop rax;\n" //rflags
	"mov [r15+if_noskip_ec+if.eflags], rax;\n"
	"pop rax;\n" //rsp
	"mov [r15+if_noskip_ec+if.esp], rax;\n"
	"pop rax;\n" //ss
	"mov [r15+if_noskip_ec+if.ss], rax;\n"
	//### adjust trusted r0 stack ptr - done with pops
	//### (not needed since will be used from tss automatically)
	//### cache untrusted r0 stack ptr from TCS into r14
	"mov r14, [rbx+tcs.ur0sp];\n" 

	//FIXME noticed that tcs is accessed with a rbx base in this routine
	//and with a ebx base in other routines - should fix

	//vmfunc to view 0 always (exit) - this could be on a seperate page
	"mov rax, vmfunc_view_sw_ctrl;\n"
	"mov rcx, untrusted_view_id;\n"
#ifdef ACTIVATE_ACR3
	//switch cr3 to value cached in gpr - note - imp to do this before
        //view switched - since acr3 is only mapped in trusted view so
        //if you switch cr3 afterwards, that code will never run after vmfunc
        //to untrusted view (since your cr3 is invalid!)
        "mov cr3, rsi;\n"
#endif

#if 1
        ".byte 0x0f;\n"
        ".byte 0x01;\n"
        ".byte 0xd4;\n"
#else
        "vmfunc;\n" //VMCALL only for debug
#endif
        //FIXME TODO Should add a check here that ecx has untrusted view id (0)
	//to disallow malicious code from jmping to the vmfunc and misusing trampoline page

	//STACKFIX Ravi - at this point stack frame is copied to exit-page r15 <ro>
	//### copy stack frame from exit-page to untrusted r0 stack (r14)
	//### alternately tss has remapped to os original so we could also read tss->esp0 to use	
	//### pivot stack to untrusted stack cached in r14
	"mov rsp, r14;\n"	

	"mov rax, [r15+if_noskip_ec+if.ss];\n"
	"push rax;\n" //ss
	"mov rax, [r15+if_noskip_ec+if.esp];\n"
	"push rax;\n" //rsp
	"mov rax, [r15+if_noskip_ec+if.eflags];\n"
	"push rax;\n" //rflags
	"mov rax, [r15+if_noskip_ec+if.cs];\n"
	"push rax;\n" //cs
	"mov rax, [r15+if_noskip_ec+if.eip];\n" //eip
        "push rax;\n"//eip
	"mov rax, [r15+start_gprs+gprs.r8];\n"
	"push rax;\n" //r8
	"mov rax, [r15+start_gprs+gprs.r9];\n"
	"push rax;\n" //r9
	"mov rax, [r15+start_gprs+gprs.r10];\n"
	"push rax;\n" //r10
	"mov rax, [r15+start_gprs+gprs.r11];\n"
	"push rax;\n" //r11
	"mov rax, [r15+start_gprs+gprs.r12];\n"
	"push rax;\n" //r12
	"mov rax, [r15+start_gprs+gprs.r13];\n"
	"push rax;\n" //r13
	"mov rax, [r15+start_gprs+gprs.r14];\n"
	"push rax;\n" //r14
	"mov rax, [r15+start_gprs+gprs.r15];\n"
	"push rax;\n" //r15
	"mov rax, [r15+start_gprs+gprs.edi];\n"
	"push rax;\n" //rdi
	"mov rax, [r15+start_gprs+gprs.esi];\n"
	"push rax;\n" //rsi
	"mov rax, [r15+start_gprs+gprs.ebx];\n"
	"push rax;\n" //rbx
	"mov rax, [r15+start_gprs+gprs.edx];\n"
	"push rax;\n" //rdx
	"mov rax, [r15+start_gprs+gprs.ecx];\n"
	"push rax;\n" //rcx
	"mov rax, [r15+start_gprs+gprs.eax];\n"
	"push rax;\n" //rax
	"mov rax, [r15];\n"
	"push rax;\n" //caller rip

	"mov rax, 0;\n" //return flag to indicate use IRET not RET
	"ret;\n"
        //We return a flag in eax to MyHandler so that it checks that 
        //and either does a RET to the OS handler (like AEX) to address in edi OR
        //IRETs for our INT flows, in either case, MyHandler leaves only the 
        //required intr frame on the appropriate stack before issueing IRET or RET
        //IRET cleans up the intr frame, and RET does not (as intended)
        ".att_syntax\n"
        );
void exit_code_end(void);
__asm__ (
    ".globl exit_code_end\n"
    "exit_code_end:"
	"nop;\n"
    );


// this is a "function" that tests if the base of segment whose
// selector is in eax is 0; it assumes gdt is in rsi and ldt in rdi
// it leaves copies of the segment.lo in edx and hi in ecxi
#define CHECK_SEGMENT_BASE(seg) \
    "mov edx, eax;\n" /*make a copy*/ \
    "shr eax, 3;\n" /*eax has index*/ \
    "and edx, 4;\n" /*edx has table type*/ \
    \
    "cmp edx, 0;\n" /*if GDT*/ \
    "jne use_ldt_"seg";\n" \
    "mov ecx, [rsi+rax*8];\n" \
    "mov eax, [rsi+rax*8+4];\n" \
    "jmp dt_used_"seg";\n" \
    "use_ldt_"seg":\n" \
    "mov ecx, [rdi+rax*8];\n" \
    "mov eax, [rdi+rax*8+4];\n" \
    "dt_used_"seg":\n" \
    \
    "mov edx, ecx;\n" /*make a copy we need it later*/ \
    "and ecx, 0xFFFF0000;\n" /*now check the 3 base fields*/ \
    "cmp ecx, 0;\n" /*ecx has lo 32 bits*/ \
    "jz base_address_15_00_check_ok_"seg";\n" \
    "jmp gp_vmcall;\n" \
    \
    "base_address_15_00_check_ok_"seg":\n" \
    "mov ecx, eax;\n" /*eax has hi 32 bits*/ \
    "and eax, 0xFF0000FF;\n" \
    "cmp eax, 0;\n" \
    "jz base_address_31_16_check_ok_"seg";\n" \
    \
    "jmp gp_vmcall;\n" \
    \
    "base_address_31_16_check_ok_"seg":\n" \
    "nop;\n" \
    "nop;\n"


void enter_eresume_code_cpuindex(void);
void enter_eresume_code_cmp_patch1(void);
void enter_eresume_code_cmp_patch2(void);
void enter_eresume_code_secs_patch1(void);
void enter_eresume_code_secs_patch2(void);
void enter_eresume_code_secs_patch3(void);
void enter_eresume_code_secs_patch4(void);
void enter_eresume_code_secs_patch5(void);
void enter_eresume_code_secs_patch6(void);
void enter_eresume_code_enter_page_patch(void);
void enter_eresume_code(void);
__asm__ (
        ".intel_syntax noprefix\n"
        ".globl enter_eresume_code_cpuindex\n"
        ".globl enter_eresume_code\n"
        ".globl enter_eresume_code_secs_patch1\n"
        ".globl enter_eresume_code_secs_patch2\n"
        ".globl enter_eresume_code_secs_patch1\n"
        ".globl enter_eresume_code_secs_patch2\n"
        ".globl enter_eresume_code_secs_patch3\n"
        ".globl enter_eresume_code_secs_patch4\n"
        ".globl enter_eresume_code_secs_patch5\n"
        ".globl enter_eresume_code_secs_patch6\n"
        ".globl enter_eresume_code_cmp_patch1\n"
        ".globl enter_eresume_code_cmp_patch2\n"
        ".globl enter_eresume_code_enter_page_patch\n"

        //NOTE WELL - this routine uses all the defines from
        //test_code (async exit flow)

//#if 0
        "enter_eresume_code:\n"
	    "enter_eresume_code_itp_loop:\n"
        "mov eax, 2;\n"
		"nop;\n"
		"nop;\n"
		"nop;\n"
		"nop;\n"
		"cmp eax, 3;\n"
        "jz enter_eresume_code_itp_loop;\n"

        "enter_eresume_code_step_1:\n"
        //We are entering this code through an 
        //interrupt gate then we dont need to disable interrupts

	//STACKFIX Ravi - copy stack contents from untrusted (unknown) stack
	//to pre known (untrusted) enter page buffer
	//#### get enter-page for cpu
        "enter_eresume_code_enter_page_patch:\n"
	"mov rax, patch_str_enter_page;\n"
	//#### cache enter-page address in r15
	"mov r15, rax;\n" //r15 == enter_page
	//#### COPY STACK contents in bulk (whole stack frame) to enter-page<rw>
	"pop rax;\n" //rip
	"mov [r15], rax;\n"
	"pop rax;\n" //rax
	"mov [r15+start_gprs+gprs.eax], rax;\n"
	"pop rax;\n" //rcx
	"mov [r15+start_gprs+gprs.ecx], rax;\n"
	"pop rax;\n" //rdx
	"mov [r15+start_gprs+gprs.edx], rax;\n"
	"pop rax;\n" //rbx
	"mov [r15+start_gprs+gprs.ebx], rax;\n"
	"pop rax;\n" //rsi
	"mov [r15+start_gprs+gprs.esi], rax;\n"
	"pop rax;\n" //rdi
	"mov [r15+start_gprs+gprs.edi], rax;\n"
	"pop rax;\n" //r15
	"mov [r15+start_gprs+gprs.r15], rax;\n"
	"pop rax;\n" //r14
	"mov [r15+start_gprs+gprs.r14], rax;\n"
	"pop rax;\n" //r13
	"mov [r15+start_gprs+gprs.r13], rax;\n"
	"pop rax;\n" //r12
	"mov [r15+start_gprs+gprs.r12], rax;\n"
	"pop rax;\n" //r11
	"mov [r15+start_gprs+gprs.r11], rax;\n"
	"pop rax;\n" //r10
	"mov [r15+start_gprs+gprs.r10], rax;\n"
	"pop rax;\n" //r9
	"mov [r15+start_gprs+gprs.r9], rax;\n"
	"pop rax;\n" //r8
	"mov [r15+start_gprs+gprs.r8], rax;\n"
	"pop rax;\n" //rip
	"mov [r15+if_noskip_ec+if.eip], rax;\n"
	"pop rax;\n" //cs
	"mov [r15+if_noskip_ec+if.cs], rax;\n"
	"pop rax;\n" //rflags
	"mov [r15+if_noskip_ec+if.eflags], rax;\n"
	"pop rax;\n" //rsp
	"mov [r15+if_noskip_ec+if.esp], rax;\n"
	"pop rax;\n" //ss
	"mov [r15+if_noskip_ec+if.ss], rax;\n"
	//#### adjust untrusted rsp to remove frame - already done above in pops
	//#### cache untrusted r0 stack in r14 since after we switch tss will be
	//remapped and we will need to save this - should go into secs pcd OR TCS??
	"mov r14, rsp;\n"

        //View handle passed in via RDX, load into RCX

        "enter_eresume_code_cpuindex:\n"
        "mov rax, patch_str_core_id;\n"
        "mov r13, rax;\n" //r13 == core_id
        "mov r10, rdi;\n" //r10 == vector
        "mov r11, rsi;\n" //r11 == errorcode_flag
        //r12 == os_cr3 (initialized later)
        //rcx == view_handle
        //rdx == secs

        //this stack frame is copied over to enter-page at this point:
		//+------------+
		//| SS         | 152
		//+------------+
		//| RSP        | 144
		//+------------+
		//| RFLAGS     | 136
		//+------------+
		//| CS         | 128
		//+------------+
		//| RIP        | 120
		//+------------+
		//| R8         | 112
		//+------------+
		//| R9         | 104
		//+------------+
		//| R10        | 96
		//+------------+
		//| R11        | 88
		//+------------+
		//| R12        | 80
		//+------------+
		//| R13        | 72
		//+------------+
		//| R14        | 64
		//+------------+
		//| R15        | 56
		//+------------+
		//| RDI        | 48
		//+------------+
		//| RSI        | 40
		//+------------+
		//| RBX        | 32
		//+------------+
		//| RDX        | 24
		//+------------+
		//| RCX        | 16
		//+------------+
		//| RAX        | 8
		//+------------+
		//| RIP (CALL) | 0
		//+------------+
		// my assumption is that eenter/eresume flow is called from myhandler
		// if that's the case, I alrady have esp

		//Save GPRS (RCX specifically) as passed in by EENTER since RCX is used to switch views

		//View handle passed in via RDX, load into RCX for EPTP switching
		"mov rcx, rdx;\n"

		// edx is now available

        //Check that the Current View is zero (entry should be 
        //invoked by untrusted code)
        "enter_eresume_code_secs_patch1:\n"
        "mov rdx, patch_str_secs_ptr;\n"
        "mov rax, [rdx+secs.scv];\n"
        "enter_eresume_code_cmp_patch1:\n"
	"mov r9, patch_str_secs_scv_un;\n"
        "cmp rax, r9;\n"
        "jz view_0_secs_check_ok;\n"
        "mov ecx, 0x1;\n"
		"jmp gp_vmcall;\n"

        "view_0_secs_check_ok:\n"
#if 0
        "mov r9, 0xDEADBEEFDEADBEEF;\n"
        "mov [rdx+secs.scv], r9;\n"
#endif
        //KCZ must be 0; otherwise, EENTER is illegal
        "mov rax, [rdx+secs.eid];\n"
        "cmp rax, 0;\n"
        "jz view_0_check_ok;\n"
        "mov ecx, view_0_chk_fail;\n"
		"jmp gp_vmcall;\n"
        
        "view_0_check_ok:\n"
        //VMM sets/resets the ept_enabled flag based on the
        //presence or absence of active views. If EPT is disabled.
        //then we should not try to do vmfunc.
        "mov rax, [rdx+secs.ept_enabled];\n"
        "bt rax, 0;\n"
        "jnc cancel_enter_no_view_switch;\n"
        // Raise NM# exception before
        // view switch, so that we never get "Device
        // not available" exception in trusted code.
        // fwait causes an NM# if CR0.TS flag is set.
        // The flag is set by the OS to delay the restoring
        // of FPU context on a context switch. Only when the
        // FPU instruction (like fwait, or wait) is execute,
        // the NM# exception in raised on which the OS resets
        // CR0.ts, thereby preventing further NM#
        "mov rax, cr0;\n"
        "bt rax, cr0.ts;\n"
        "jnc do_vmfunc;\n"
        "swapgs;\n" //Switch to kernel GS, as fwait will raise an exception
        "fwait;\n" // Will raise NM# exception
        "swapgs;\n" //Switch back to user GS, as the enter flow assumes that
        // gs points to user gs
        "do_vmfunc:\n"
//#endif

        //****************debug only start***************
        /*
        //VIVEK: update secs_ptr->base with address passed in eax
		"mov eax, [esp+12];\n" //get vector
        "cmp eax, eresume_vector;\n"
        "je eresume_flow;\n"

        //do this only for enter flow (since for resume it should
        //already be set correctly)
        "mov eax, [esp+start_gprs+gprs.eax +4];\n" // eax in stack contains the SEC base
        "mov [edx+secs.base], eax; \n" // secs->base = SEC base passed through eenter

        "eresume_flow:\n"

        "mov ebx, edx;\n"
        "add ebx, secs.vid;\n"
        "mov ax, 0;\n" //expect view 0
        "mov cx, 1;\n" //switch vid to 1
        ".byte 0xF0;\n"
        "cmpxchg [ebx], cx;\n"
        */
        //****************debug only end***************

        //Perform vmfunc to transition to required EPT trusted view
        //KCZ ecx has the view number already
        "mov rax, vmfunc_view_sw_ctrl;\n"
#if 1
        ".byte 0x0f;\n"
        ".byte 0x01;\n"
        ".byte 0xd4;\n"
#else
        "vmfunc;\n"
#endif
        "cmp rax, vmfunc_return_success;\n"
        "jne cancel_enter_eresume;\n"

        //"vmcall;\n" //VMCALL only for debug
        //VMFUNC opcode emitted below
       // ".byte 0x0f;\n"
       // ".byte 0x01;\n"
       // ".byte 0xd4;\n"

        //KCZ views are switched; should load cr3 but not doing that for now...
        //KCZ but before the switch, we have to store OS-cr3 for now before
        //KCZ we can save it in SECS
        //KCZ esi should be available
        "mov r12, cr3;\n" //r12 == os cr3. This will be saved in TCS later on.

        "mov rax, [rdx+secs.os_cr3];\n"
        "cmp r12, rax;\n"
        "je check_scv_trusted_view;\n"
        "cancel_enter_eresume:\n"

        "mov rax, vmfunc_view_sw_ctrl;\n"
        "mov rcx, untrusted_view_id;\n"
#if 1
        ".byte 0x0f;\n"
        ".byte 0x01;\n"
        ".byte 0xd4;\n"
#else
        "vmfunc;\n" //Switch back to view 0
#endif
        "cancel_enter_no_view_switch:"
        //Recreate the untrusted stack again as it was emptied
        //at the beginning with an intention to switch to the
        //trusted view
        "mov rax, [r15+if_noskip_ec+if.ss];\n"
        "push rax;\n"
        "mov rax, [r15+if_noskip_ec+if.esp];\n"
        "push rax;\n"
        "mov rax, [r15+if_noskip_ec+if.eflags];\n"
        "push rax;\n"
        "mov rax, [r15+if_noskip_ec+if.cs];\n"
        "push rax;\n"
        "mov rax, [r15+if_noskip_ec+if.eip];\n"
        "push rax;\n"
        "mov rax, [r15+start_gprs+gprs.r8];\n"
        "push rax;\n"
        "mov rax, [r15+start_gprs+gprs.r9];\n"
        "push rax;\n"
        "mov rax, [r15+start_gprs+gprs.r10];\n"
        "push rax;\n"
        "mov rax, [r15+start_gprs+gprs.r11];\n"
        "push rax;\n"
        "mov rax, [r15+start_gprs+gprs.r12];\n"
        "push rax;\n"
        "mov rax, [r15+start_gprs+gprs.r13];\n"
        "push rax;\n"
        "mov rax, [r15+start_gprs+gprs.r14];\n"
        "push rax;\n"
        "mov rax, [r15+start_gprs+gprs.r15];\n"
        "push rax;\n"
        "mov rax, [r15+start_gprs+gprs.edi];\n"
        "push rax;\n"
        "mov rax, [r15+start_gprs+gprs.esi];\n"
        "push rax;\n"
        "mov rax, [r15+start_gprs+gprs.ebx];\n"
        "push rax;\n"
        "mov rax, [r15+start_gprs+gprs.edx];\n"
        "push rax;\n"
        "mov rax, [r15+start_gprs+gprs.ecx];\n"
        "push rax;\n"
        "mov rax, [r15+start_gprs+gprs.eax];\n"
        "push rax;\n"
        "mov rax, [r15];\n" //rip in VIDT_STUB
        "push rax;\n"
        //enter_enclave.S expects %edi as -1 for normal
        //exit, any other value of %edi is treated as
        //ocall return
        "mov edi, -1;\n"
        "mov [rsp+start_gprs+gprs.edi], edi;\n"
        //esi expects the return status from ecall.
        //return nonzero value
        "mov esi, ecall_not_allowed;\n"
        "mov [rsp+start_gprs+gprs.esi], esi;\n"

        "jmp out_enter_eresume;\n"

        "check_scv_trusted_view:\n"
        //Read SECS security cookie from SECS GVA (RDI)
        //Compare against immediate value on XO trampoline page
        "mov rax, [rdx+secs.scv];\n"
        "enter_eresume_code_cmp_patch2:\n"
        "mov r9, patch_str_secs_scv;\n"
        "cmp rax, r9;\n"
        "jz view_x_secs_check_ok;\n"

        "mov ecx, secs_scv_chk_fail;\n"
        "jmp gp_vmcall;\n"

        "view_x_secs_check_ok:\n"
        //Read view-handle expected from SECS
        //Verify view handle requested post view switch 
        //and branch to fail if mis-match 
        //(Fail vmcalls and injects #GP after restoring OS-CR3 and RCX)
        //KCZ ecx still has view id from the original edx
        "mov rax, [rdx+secs.eid];\n"
        "cmp eax, ecx;\n"
        "jz view_x_check_ok;\n"

        //Handle cases where some other thread destroys
        //the view while this thread is trying to enter
        //the view. Rather than causing panic in the
        //VMM, we exit out to the guest with reason of
        //ecall not allowed
        "jmp cancel_enter_eresume;\n"

        "view_x_check_ok:\n"
        //Verify SECS state to ensure View initialized
        //KCZ secs->attributes->init flag?
        //KCZ bit 0 of secs->attributes has to be set
        "mov rax, [rdx+secs.attrib];\n"
        "bt rax, 0;\n"
        "jc secs_inited_check_ok;\n"

        "jmp cancel_enter_eresume;\n"

        "secs_inited_check_ok:\n"
        //Cache OS-CR3 in GPR
        //KCZ shouldn't this have been done above?
        //KCZ in any case, OS-cr3 is in esi
        //load ACR3 from SECS
        //FIXME KCZ again, shouldn't this be done above before
        //KCZ we access secs_la?
#ifdef ACTIVATE_ACR3
        "mov rax, [rdx+secs.acr3];\n"
        "mov cr3, rax;\n"
#endif

	//STACKFIX Ravi 
	//cpu tss points to our tss which refs our r0 stack
	//and we can now access our r0 stack, but we have to set it up and pivot first
	//###save untrusted r0 rsp from r14 into TCS -> done later when TCS ready
	//###copy bulk info from enter-page<ro> r15 to new rsp from secs pcd
	//###pivot stack to trusted r0 stack
        "xor rcx, rcx;\n"
        "mov rbx, rdx;\n" //secs ptr
        "add rbx, secs.pcd;\n"
        "mov ecx, r13d;\n" //core id cached in r13
        "shl ecx, shift_size_secs_pcd;\n"
        "add rcx, rbx;\n" //ptr to core-specific pcd
        "mov rsp, [rcx+secs.pcd.r0sp];\n"

	"mov rax, [r15+if_noskip_ec+if.ss];\n"
	"push rax;\n"
	"mov rax, [r15+if_noskip_ec+if.esp];\n"
	"push rax;\n"
	"mov rax, [r15+if_noskip_ec+if.eflags];\n"
	"push rax;\n"
	"mov rax, [r15+if_noskip_ec+if.cs];\n"
	"push rax;\n"
	"mov rax, [r15+if_noskip_ec+if.eip];\n"
	"push rax;\n"
	"mov rax, [r15+start_gprs+gprs.r8];\n"
	"push rax;\n"
	"mov rax, [r15+start_gprs+gprs.r9];\n"
	"push rax;\n"
	"mov rax, [r15+start_gprs+gprs.r10];\n"
	"push rax;\n"
	"mov rax, [r15+start_gprs+gprs.r11];\n"
	"push rax;\n"
	"mov rax, [r15+start_gprs+gprs.r12];\n"
	"push rax;\n"
	"mov rax, [r15+start_gprs+gprs.r13];\n"
	"push rax;\n"
	"mov rax, [r15+start_gprs+gprs.r14];\n"
	"push rax;\n"
	"mov rax, [r15+start_gprs+gprs.r15];\n"
	"push rax;\n"
	"mov rax, [r15+start_gprs+gprs.edi];\n"
	"push rax;\n"
	"mov rax, [r15+start_gprs+gprs.esi];\n"
	"push rax;\n"
	"mov rax, [r15+start_gprs+gprs.ebx];\n"
	"push rax;\n"
	"mov rax, [r15+start_gprs+gprs.edx];\n"
	"push rax;\n"
	"mov rax, [r15+start_gprs+gprs.ecx];\n"
	"push rax;\n"
	"mov rax, [r15+start_gprs+gprs.eax];\n"
	"push rax;\n"
	"mov rax, [r15];\n"
	"push rax;\n"
    "xor rbx, rbx;\n"
	//following code *should* work unchanged since it uses rsp as before

	//Verify RBX is page-aligned (for TCS)
	//KCZ if we are supposed to save OS-cr3 in TCS
	//KCZ shouldn't this check be done earlier?
        //KCZ !(rbx & 4095) is OK
        //VT: TODO: change ebx->rbx for 64 bit user space
        "mov ebx, [rsp+start_gprs+gprs.ebx];\n"
        "mov eax, ebx;\n"
        "and eax, 4095;\n"
        "cmp eax, 0;\n"
        "jz tcs_page_aligned_check_ok;\n"
        "mov ecx, tcs_pg_align_chk_fail;\n"
		"jmp gp_vmcall;\n"
        "tcs_page_aligned_check_ok:\n"

		// save OS-CR3 until we can write to TCS
        // VT: OS-CR3 is saved in r12
//		"push rsi;\n"

		//Make sure SSA contains at least one frame
		// cssa
		"mov eax, [ebx+tcs.cssa];\n"
		// if this is eresume, jmp to a different check
        "cmp r10d, eresume_vector;\n"
		"jz check_ssa_for_eresume;\n"

		// compare cssa to nssa
		"mov esi, [ebx+tcs.nssa];\n"
        "cmp eax, esi;\n"
		// cssa must be < nssa
		"jb ssa_frame_avail_check_ok;\n"

        "mov ecx, ssa_frame_avl_chk1_fail;\n"
		"jmp gp_vmcall;\n"

		"check_ssa_for_eresume:\n"
		// there must be at least one active frame
		"cmp eax, 0;\n"
		"jnz ssa_frame_avail_check_ok;\n"
//#endif
        "mov ecx, ssa_frame_avl_chk2_fail;\n"
		"jmp gp_vmcall;\n"

		"ssa_frame_avail_check_ok:\n"

		// calculate ssa
        //uint64_t ssa_start = tcs->arch.ossa + secs->baseaddr 
        //+ SSA_FRAME_SIZE(secs) * tcs->arch.cssa;

		// assume ssa frame size is 4096
        // eax has cssa already
        "push rdx;\n" //since mul clobbers eax, edx
        "mov ecx, 4096;\n"
        "mul ecx;\n" //RAVI modified to use ecx - confirm ok
		// KCZ yes, it is OK; I forgot mul must use r/m32
        "pop rdx;\n" //since mul clobbers eax, edx - restore secs 

        // add ossa
        "add eax, [ebx+tcs.ossa];\n"

        // add base address from SECS
        "add eax, [rdx+secs.base];\n"

		// if this is eresume, subtract one frame
		"mov rcx, r10;\n" //get vector
        "cmp rcx, eresume_vector;\n"
		"jnz ssa_address_done;\n"
		// for now it is hardcoded to one page
		"sub eax, 4096;\n"
		"ssa_address_done:\n"
		// push on the stack, it will be needed later
		"push rax;\n"

		// the stack looks like this
		//+------------+
		//| ssa        |  0
		//+------------+
		
		//Perform segment table validation
        //Verify that CS, SS, DS, ES.base is 0
        // KCZ GDT and LDT bases are cached in SECS
        // KCZ because desc table exiting is on
        "xor rax, rax;\n"
        "mov eax, r13d;\n" //core id 
        "shl eax, shift_size_secs_pcd;\n"
        "add rax, rdx;\n"
        "add rax, secs.pcd;\n"

        "mov rsi, [rax+secs.pcd.gdtr];\n"
        "mov rdi, [rax+secs.pcd.ldtr];\n"
		// GP_IF (cs.base)
		"xor eax, eax;\n"
        "mov eax, [rsp+if_noskip_ec+if.cs+8];\n" //+8 as there is a temporary additional push on stacks (for ssa)... similar change for SS below
        CHECK_SEGMENT_BASE("cs")

        //    GP_IF (aep > cs.limit)         ;
        // now edx has lo; ecx has hi
        // limit is 16 lsbs from edx and bits 16-19 from ecx
        // this is cs so no expanding down
		"and edx, 0xFFFF;\n"

        //check granularity of segment
        "bt ecx, seg_granularity;\n"
        "jc seg_g_4k_inc;\n"

        //byte granularity
        "and ecx, 0xF0000;\n"
        "or ecx, edx;\n"

        "jmp perf_aep_check;\n"

        "seg_g_4k_inc:\n"
        "and ecx, 0xF0000;\n"
        "or ecx, edx;\n"
        "shl ecx, shift_size_4k;\n" 

        "perf_aep_check:\n"
		"mov edx, [ebx+tcs.aep];\n"
		"cmp ecx, edx;\n"
		"ja aep_check_ok;\n"

        "mov ecx, aep_chk_fail;\n"
		"jmp gp_vmcall;\n"

		"aep_check_ok:\n"

		// if eenter
		//    target = tcs->arch.oentry + secs->baseaddr;
		// if eresume
		//    target = gpr->rip
		//    GP_IF (target > cs.limit);

		"mov eax, r10d;\n" //vector is saved in r10 
        "cmp eax, eresume_vector;\n"
		"jz eresume_target;\n"

		// oentry
		"mov eax, [ebx+tcs.oentry];\n"
		// add base address from SECS
        "enter_eresume_code_secs_patch2:\n"
		"mov rdx, patch_str_secs_ptr;\n"
		"add eax, [rdx+secs.base];\n"
		"jmp target_check;\n"

		"eresume_target:\n"
		// ssa is on the stack
		"mov edx, [rsp];\n"
        //but gprs are at the end
        "add edx, 4096;\n"
        "sub edx, ssa_gpr_size;\n"
		"mov eax, [edx+ssa.ip];\n" //TODO: For 64-bit userspace it must be copied in rax

		"target_check:\n"
		"cmp ecx, eax;\n"
		"ja target_check_ok;\n"

        "mov ecx, target_chk_fail;\n"
		"jmp gp_vmcall;\n"

		"target_check_ok:\n"

        // KCZ temp fix
        // moved ds checks after ss checks
        // KCZ temp fix

		// the problem is that USABLE macro in PAS uses a null bit in AR
		// I don't know if such a bit exists here bt if base is not 0
		// but segement is somehow marked unusable, we will inject #GP
		//    if (USABLE(es)) GP_IF (es.base); 
		//    if (USABLE(ss)) GP_IF (ss.base || !ss.ar.db); 

		"xor eax, eax;\n"
		"mov ax, es;\n"
		CHECK_SEGMENT_BASE("es") 

		"xor eax, eax;\n"
        "mov eax, [rsp+if_noskip_ec+if.ss+8];\n"
		CHECK_SEGMENT_BASE("ss") 

		// ecx has ss.hi, check db bit
		"bt ecx, 22;\n"
		"jc ss_db_check_ok;\n"

        "mov ecx, ss_db_chk_fail;\n"
		"jmp gp_vmcall;\n"
		"ss_db_check_ok:\n"
		
        // KCZ temp fix
        //    GP_IF (ds.base)    
		"xor eax, eax;\n"
		"mov ax, ds;\n"
		CHECK_SEGMENT_BASE("ds") 
        "push rcx;\n"

		// this check is in the PAS; ucode tests this by testing ebx against ds.limit
		// seems easier to do it the PAS way
		"bt ecx, 10;\n"
		"jnc ds_expand_check_ok;\n"

        "mov ecx, ds_expand_chk_fail;\n"
		"jmp gp_vmcall;\n"

		"ds_expand_check_ok:\n"
        // KCZ temp fix

		// check SSA and FS/GS base	alignment
		"mov eax, [ebx+tcs.ossa];\n"
		"and eax, 4095;\n"
		"cmp eax, 0;\n"
		"jz ossa_page_aligned_check_ok;\n"

        "mov ecx, 0xd;\n"
		"jmp gp_vmcall;\n"

		"ossa_page_aligned_check_ok:\n"
		// tcs.ofs_base
		"mov eax, [ebx+48];\n"
		"and eax, 4095;\n"
		"cmp eax, 0;\n"
		"jz ofsbase_page_aligned_check_ok;\n"

        "mov ecx, ofsbase_page_align_chk_fail;\n"
		"jmp gp_vmcall;\n"

		"ofsbase_page_aligned_check_ok:\n"
		// tcs.ogs_base
		"mov eax, [ebx+56];\n"
		"and eax, 4095;\n"
		"cmp eax, 0;\n"
		"jz ogsbase_page_aligned_check_ok;\n"

        "mov ecx, 0xf;\n"
		"jmp gp_vmcall;\n"

		"ogsbase_page_aligned_check_ok:\n"

		// check that proposed FS/GS segments fall within DS
		// edx has ds.lo; ecx has ds.hi
		"and edx, 0xFFFF;\n"

        //check granularity of segment
        "bt ecx, 23;\n"
        "jc ds_g_4k_inc;\n"

        //byte granularity
        "and ecx, 0xF0000;\n"
        "or ecx, edx;\n"

        "jmp perf_fsgs_check;\n"

        "ds_g_4k_inc:\n"
        "and ecx, 0xF0000;\n"
        "or ecx, edx;\n"
        "shl ecx, shift_size_4k;\n" 

        "perf_fsgs_check:\n"
		// ecx has granularity adjusted ds.limit now
        "enter_eresume_code_secs_patch3:\n"
		"mov rdx, patch_str_secs_ptr;\n"
		"mov eax, [rdx+secs.base];\n"
		"add eax, [ebx+48];\n"
		"add eax, [ebx+64];\n"
		// eax has enclave_base + ofs_base + ofs_limit
		// first, check for overflow (wrap-around)
		// if overflow, ds.limit must be more than 4GB, which is not possible
		// otherwise eax must be less than or equal to ds.limit
		"jnc no_fs_wrap_around_check_ok;\n"

        "mov ecx, no_fs_wrap_around_chk_fail;\n"
		"jmp gp_vmcall;\n"

		"no_fs_wrap_around_check_ok:\n"

		"cmp ecx, eax;\n"
		"ja fs_within_ds_check_ok;\n"

        "mov ecx, fs_within_ds_chk_fail;\n"
		"jmp gp_vmcall;\n"

		"fs_within_ds_check_ok:\n"
		// now check gs
		"mov eax, [rdx+secs.base];\n"
		"add eax, [ebx+56];\n"
		"add eax, [ebx+68];\n"
		"jnc no_gs_wrap_around_check_ok;\n"

        "mov ecx, no_gs_wrap_around_chk_fail;\n"
		"jmp gp_vmcall;\n"

		"no_gs_wrap_around_check_ok:\n"

		"cmp ecx, eax;\n"
		"ja gs_within_ds_check_ok;\n"

        "mov ecx, gs_within_ds_chk_fail;\n"
		"jmp gp_vmcall;\n"

		"gs_within_ds_check_ok:\n"
		// check if TCS.flags.reserved is all 0s
		"mov rax, [ebx+tcs.flags];\n"
	//	"and eax, 0xFFFFFFFE;\n"
		"and rax, ~0x1;\n"
//		"jz tcs_reserved_0_3_check_ok;\n"
		"jz tcs_reserved_check_ok;\n"

        "mov ecx, tcs_reserved_chk_fail;\n"
		"jmp gp_vmcall;\n"
#if 0
		"tcs_reserved_0_3_check_ok:\n"

		"mov eax, [ebx+tcs.flags+4];\n"
		"and eax, 0xFFFFFFFF;\n"
		"jz tcs_reserved_4_7_check_ok;\n"

        "mov ecx, 0x15;\n"
		"jmp gp_vmcall;\n"
		"tcs_reserved_4_7_check_ok:\n"
#endif
        "tcs_reserved_check_ok:\n"

		// Transition to next state: INACTIVE -> ACTIVE
        // Make sure we started in the INACTIVE state and are the only thread using
        // the TCS. Otherwise, the instruction fails w/ #GP
        // TCS must not be active
        "mov eax, 0;\n"
        "mov ecx, 1;\n" //RAVI added
        // add lock
        ".byte 0xF0;\n"
        // set to active
        //cmpxchg [ebx], 1;\n"
        "cmpxchg [ebx], ecx;\n" //RAVI modified
        "je tcs_lock_acquired_ok;\n"

        "mov ecx, tcs_lock_acquire_fail;\n"
        "jmp gp_vmcall;\n"
#if 0
        //VMCALL here to assert
        "mov eax, vmcall_assert;\n"
        "vmcall;\n"
#endif
        "tcs_lock_acquired_ok:\n"
        // KCZ temp fix
        "pop rcx;\n" // ds.hi
        // KCZ temp fix
        ////////// EENTER/ERESUME can't fail beyond this point /////////////////

		// we can now write to TCS
		// eax is available

		// tcs->ssa_featsave_ppa = ssa;
		"pop rax;\n"
		// save ssa in tcs
		"mov [ebx+tcs.ssa], eax;\n"

		// take are care of OS-CR3 and AEP
		//  tcs->arch.aep = aep; it's in ecx on the stack
		"mov eax, [rsp+start_gprs+gprs.ecx];\n"
		"mov [ebx+tcs.aep], eax;\n"
		// OS-CR3
        // VT: No longer saving OS CR3 on stack
		"mov [ebx+tcs.os_cr3], r12;\n"

		//STACKFIX Ravi
		//save untrusted r0stack we entered with
		"mov [ebx+tcs.ur0sp], r14;\n"

		// ecx has ds.hi
		// but we need to reuse it so save on stack
		// (could have pushed earlier instead of making copies)
		"push rcx;\n"

		// KCZ this comment is wrong; FS/GS are saved in TCS
		//Do the Guest FS/GS Swap from TCS  Save OS FS/GS to SSA
		"xor rax, rax;\n"
        "mov ax, fs;\n" //VT: Kernel FS and user fs are same
		// save in tcs
		"mov [ebx+tcs.save_fs_selector], ax;\n"
		"mov edx, eax;\n"
		// since we have to do *8 later
		// instead of shifting right by 3 and mult by 8
		// just do and 0xFFF8
		//shr eax, 3
		"and eax, 0xFFF8;\n"
		"and edx, 4;\n" //fs.TI
		"cmp edx, 0;\n"
		"jne fs_use_ldt;\n"
		"add rax, rsi;\n"
		// address of fs descriptor
		"push rax;\n"
		"mov ecx, [rax];\n"
		"mov eax, [rax+4];\n"
		"jmp fs_done;\n"
		"fs_use_ldt:\n"
		"add rax, rdi;\n"
		// address of fs descriptor
		"push rax;\n"
		"mov ecx, [rax];\n"
		"mov eax, [rax+4];\n"
		"fs_done:\n"
		// save in tcs
		"mov [ebx+tcs.save_fs_desc_low], ecx;\n"
		"mov [ebx+tcs.save_fs_desc_high], eax;\n"

		"xor rax, rax;\n"
		"mov ax, gs;\n"
		"mov [ebx+tcs.save_gs_selector], ax;\n"
		"mov edx, eax;\n"
		//shr eax, 3
		"and eax, 0xFFF8;\n"
		"and edx, 4;\n"
		"cmp edx, 0;\n"
		"jne gs_use_ldt;\n"
		"add rax, rsi;\n"
		// address of gs descriptor
		"push rax;\n"
		"mov ecx, [rax];\n"
		"mov eax, [rax+4];\n"
		"jmp gs_done;\n"
		"gs_use_ldt:\n"
		"add rax, rdi;\n" 
		// address of gs descriptor
		"push rax;\n"
		"mov ecx, [rax];\n"
		"mov eax, [rax+4];\n"
		"gs_done:\n"
		"mov [ebx+tcs.save_gs_desc_low], ecx;\n"
		"mov [ebx+tcs.save_gs_desc_high], eax;\n"

        // do the swap
		// stack looks like this now
		//+------------+
		//| ds.hi      |  16
		//+------------+
		//| &fs        |  8
		//+------------+
		//| &gs        |  0
		//+------------+

		// we start with ds descriptor
		"mov ecx, [rsp+16];\n"

		// type &= 0x3
		// clear bits 10 and 11 in hi
		"and ecx, 0xFFFFF3FF;\n"
		// type |= 0x1
		// set bit 8
		// s = 1;
		// set bit 12
		// p = 1;
		// set bit 15
		// db = 1;
		// set bit 22
		// g = 1;
		// set bit 23
		"or ecx, 0xC09100;\n"

		// at this point FS/GS are the same
		// so copy back to ds.hi
		"mov [rsp+16], ecx;\n"

		// write limit_15_00 and base_15_00
        "enter_eresume_code_secs_patch4:\n"
		"mov rdx, patch_str_secs_ptr;\n"
		"mov rax, [rdx+secs.base];\n"
		// ofs_base
		"add rax, [ebx+48];\n"
		// eax has the whole fs base; push for later
		"push rax;\n"
		// stack looks like this now
		//+------------+
		//| ds.hi      |  24
		//+------------+
		//| &fs        |  16
		//+------------+
		//| &gs        |  8
		//+------------+
		//| fs.b       |  0
		//+------------+

		// now build fs.lo
		// base_15_0 goes into msbs
		"shl eax, 16;\n"
		// limit_15_0 goes into lsbs
		// ofs_limit
		"mov ax, [ebx+64];\n"

		// eax has fs.lo now so write back
		// to the fs descriptor address on the stack
		"mov rcx, [rsp+16];\n"
		"mov [rcx], eax;\n"

		// now gs.lo
		"mov rax, [rdx+secs.base];\n"
		// ogs_base
		"add rax, [ebx+56];\n"
		// eax has the whole gs base; push for later
		"push rax;\n"
		// stack looks like this now
		//+------------+
		//| ds.hi      | 32
		//+------------+
		//| &fs        | 24
		//+------------+
		//| &gs        | 16
		//+------------+
		//| fs.b       |  8
		//+------------+
		//| gs.b       |  0
		//+------------+

		"shl eax, 16;\n"
		// ogs_limit
		"mov ax, [ebx+68];\n"
		"mov rcx, [rsp+16];\n"
		"mov [rcx], eax;\n"

		// done with lo parts

		// read ds.hi
		"mov eax, [rsp+32];\n"
		// we only need the bits in the middle
		"and eax, 0x00FFFF00;\n"
		// read fs.b
		"mov ecx, [rsp+8];\n"
		// we only need 16 msbs
		"shr ecx, 16;\n"
		// copy 8 lsbs
		"or al, cl;\n"
        // now we need 8 msbs to be at the very top
        "shl ecx, 16;\n" //RAVI changed typo to shl
        // but don't need anything else
        "and ecx, 0xFF000000;\n"
        "or eax, ecx;\n"
        // we have the whole fs.hi now
        "mov rcx, [rsp+24];\n"
        // so write back to the correct place
        "mov [rcx+4], eax;\n"
		
        // same thing for gs
        // read ds.hi
        "mov eax, [rsp+32];\n"
        // we only need the bits in the middle
        "and eax, 0x00FFFF00;\n"
        // read gs.b
        "mov ecx, [rsp];\n"
        // we only need 16 msbs
        "shr ecx, 16;\n"
        // copy 8 lsbs
		"or al, cl;\n"
        // now we need 8 msbs to be at the very top
        "shl ecx, 16;\n" //RAVI modified typo srl to shl - confirm
		// KCZ yes, shl is correct
        // but don't need anything else
        "and ecx, 0xFF000000;\n"
        "or eax, ecx;\n"
        // we have the whole fs.hi now
        "mov rcx, [rsp+16];\n"
        // so write back to the correct place
        "mov [rcx+4], eax;\n"

        // finished swapping FS/GS
        // pop the stack
        "add rsp, 40;\n"

        // shouldn't we force reload of FS/GS here?
        //RAVI - yes we should
        "mov fs, [ebx+tcs.save_fs_selector];\n" 
        "mov gs, [ebx+tcs.save_gs_selector];\n" 
        
        // if fs selector is 0 use gs selector
        // NOTE this only works if proposed fsbase == gsbase
        // and proposed fslim == gslim
        "xor rax, rax;\n"
        "mov ax, fs;\n"
        "cmp ax, 0;\n"
        "jne continue_enter_eresume;\n"
        "mov fs, [ebx+tcs.save_gs_selector];\n"
        "continue_enter_eresume:\n"

		// get stored ssa from tcs
		"mov eax, [ebx+tcs.ssa];\n"
        //gpr area is at the end of ssa page
        //  gpr = ssa + SE_PAGE_SIZE - sizeof(gpr_t);   
        "add eax, 4096;\n"
        "sub eax, ssa_gpr_size;\n"

        // rflags from the stack
        "mov rdx, [rsp+if_noskip_ec+if.eflags];\n"

        // if this is eresume, restore values stored by AEX
        "mov ecx, r10d;\n"
        "cmp ecx, eresume_vector;\n"
        "jz eresume_restore_state;\n"

        //Save state for possible asynchronous exits 
        //Save the outside RSP and RBP so they can be restored 
        //on next asynch or synch exit
        //Setup stack to be switched to trusted view stack by 
        //setting RSP on interrupt frame

        // save u_rsp and u_rbp
        "mov rcx, [rsp+if_noskip_ec+if.esp];\n"
        "mov [eax+ssa.sp_u], rcx;\n"
        "mov [eax+ssa.bp_u], rbp;\n" //VT: Since we are not writing
        // ISRs in C, the rbp is still the user rbp. A 'C' code
        // requires rbp to be saved on the stack and the new rbp
        // be created from kernel rsp

		// save EFLAGS.TF - needed only for OPTOUT enclave 

		//  tcs->rflags = rflags.raw;
        //  VT: low 32 bits are good enough since bits 22:63
        //  for rflags are reserved.
		"mov [ebx+tcs.eflags], edx;\n"
		// clear TF
		"and edx, 0xFFFFFEFF;\n"
#if 0
        //clear IF - for debuggin purpose only
        "and edx, 0xFFFFFDFF;\n"
#endif
        "jmp prepare_for_enter;\n"

		"eresume_restore_state:\n"

		"mov eax, [ebx+tcs.ssa];\n"
        "fxrstor64 [eax];\n"

        "add eax, 4096;\n"
        "sub eax, ssa_gpr_size;\n"

		// rax
		"mov rcx, [eax+ssa.ax];\n"
		"mov [rsp+start_gprs+gprs.eax], rcx;\n"
		// rcx
		"mov rcx, [eax+ssa.cx];\n"
		"mov [rsp+start_gprs+gprs.ecx], rcx;\n"
		// rdx
		"mov rcx, [eax+ssa.dx];\n"
		"mov [rsp+start_gprs+gprs.edx], rcx;\n"
		// rbx
		"mov rcx, [eax+ssa.bx];\n"
		"mov [rsp+start_gprs+gprs.ebx], rcx;\n"
		// rsi
		"mov rcx, [eax+ssa.si];\n"
		"mov [rsp+start_gprs+gprs.esi], rcx;\n"
		// rdi
		"mov rcx, [eax+ssa.di];\n"
		"mov [rsp+start_gprs+gprs.edi], rcx;\n"

		// r8
		"mov rcx, [eax+ssa.r8];\n"
		"mov [rsp+start_gprs+gprs.r8], rcx;\n"
		// r9
		"mov rcx, [eax+ssa.r9];\n"
		"mov [rsp+start_gprs+gprs.r9], rcx;\n"
		// r10
		"mov rcx, [eax+ssa.r10];\n"
		"mov [rsp+start_gprs+gprs.r10], rcx;\n"
		// r11
		"mov rcx, [eax+ssa.r11];\n"
		"mov [rsp+start_gprs+gprs.r11], rcx;\n"
		// r12
		"mov rcx, [eax+ssa.r12];\n"
		"mov [rsp+start_gprs+gprs.r12], rcx;\n"
		// r13
		"mov rcx, [eax+ssa.r13];\n"
		"mov [rsp+start_gprs+gprs.r13], rcx;\n"
		// r14
		"mov rcx, [eax+ssa.r14];\n"
		"mov [rsp+start_gprs+gprs.r14], rcx;\n"
		// r15
		"mov rcx, [eax+ssa.r15];\n"
		"mov [rsp+start_gprs+gprs.r15], rcx;\n"
		// rbp
		"mov rcx, [eax+ssa.bp];\n"
		"mov rbp, rcx;\n"
		// rsp
		"mov rcx, [eax+ssa.sp];\n"
		"mov [rsp+if_noskip_ec+if.esp], rcx;\n"

		// rflags [start]
		"mov rdx, [eax+ssa.flags];\n"
		"mov ecx, [rsp+if_noskip_ec+if.eflags];\n"
		// restore tf
		"and ecx, 0x100;\n"
		"or edx, ecx;\n"
		"mov [rsp+if_noskip_ec+if.eflags], rdx;\n"
        // eflags [end]

        //if resume flow should resume to ssa.ip
		"mov rsi, [eax+ssa.ip];\n"

        // this is still eresume flow
		// must decrement cssa
		"mov eax, [ebx+24];\n"
        "dec eax;\n"
        "mov [ebx+24], eax;\n"

        "jmp eresume_restore_eip;\n"

		"prepare_for_enter:\n"
        // only eenter flow goes here.
		// eflags back on the stack
        //************debug only start************
        //"and edx, 0xFFFFFDFF;\n" //set IF=0 for test
        //************debug only end**************
		"mov [rsp+if_noskip_ec+if.eflags], rdx;\n"

		//if enter then
        //Setup entrypoint RIP to TCS.OENTRY entrypoint by setting 
        //RIP on interrupt frame

		// int saves correct address 
        "mov rax, [rsp+if_noskip_ec+if.eip];\n"
		// ecx
		"mov [rsp+start_gprs+gprs.ecx], rax;\n"
		// cssa
		"mov eax, [ebx+tcs.cssa];\n"
		// eax
		"mov [rsp+start_gprs+gprs.eax], eax;\n"

        //IRET to continue execution at entrypoint point inside TV
		//if enter: entry point secs->base + tcs->oentry
        "enter_eresume_code_secs_patch5:\n"
		"mov rdx, patch_str_secs_ptr;\n"
		"mov rax, [rdx+secs.base];\n"
		"add rax, [ebx+tcs.oentry];\n"

        //Entry point of trusted is changed on stack
		"mov [rsp+if_noskip_ec+if.eip], rax;\n"
        "jmp enter_resume_last_step;\n"

		"eresume_restore_eip:\n"
		//if resume: entry point is from ssa.ip
        //which is stored in esi
		"mov [rsp+if_noskip_ec+if.eip], esi;\n"

        "enter_resume_last_step:\n"
        // last step: write tcs address to secs[core#]
        "xor rax, rax;\n"
        "mov eax, r13d;\n" //cpu id
		// size of per-core-data is shift_size_secs_pcd 
		"shl eax, shift_size_secs_pcd;\n"
		"add eax, secs.pcd;\n"
		"add eax, secs.pcd.tcs;\n"
        "enter_eresume_code_secs_patch6:\n"
		"mov rdx, patch_str_secs_ptr;\n" // Putting edx closer to its consumption
		"mov [rdx+rax], ebx;\n"

        "out_enter_eresume:\n"
		"mov rax, 0;\n" //return flag to indicate use IRET not RET
        "ret;\n"
        //We return a flag in eax to MyHandler so that it checks that 
        //and either does a RET to the OS handler (like AEX) to address in edi OR
        //IRETs for our INT flows, in either case, MyHandler leaves only the 
        //required intr frame on the appropriate stack before issueing IRET or RET
        //IRET cleans up the intr frame, and RET does not (as intended)
		"gp_vmcall:\n"

		"mov eax, vmcall_assert;\n"
		"vmcall;\n"

        ".att_syntax\n"
        );
void enter_eresume_code_end(void);
__asm__ (
    ".globl enter_eresume_code_end\n"
    "enter_eresume_code_end:"
	"nop;\n"
    );
