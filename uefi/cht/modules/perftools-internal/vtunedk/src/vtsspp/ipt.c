/*COPYRIGHT**
// -------------------------------------------------------------------------
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of the accompanying license
//  agreement or nondisclosure agreement with Intel Corporation and may not
//  be copied or disclosed except in accordance with the terms of that
//  agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
// -------------------------------------------------------------------------
**COPYRIGHT*/

/*
//  File  : ipt.c 
//  Author: Stanislav Bratanov
*/

#include <linux/dma-mapping.h>
#include "vtss_config.h"
#include "globals.h"
#include "time.h"
#include "ipt.h"
/**
// Intel Processor Trace functionality
*/

int vtss_ipt_init(void)
{
    int i;
    int res = 0;
    dma_addr_t dma_addr;
    /// for each CPU:
    ///   allocate ToPA page
    ///   allocate output buffer
    ///   free all buffers in case of error
    for(i = 0; i < hardcfg.cpu_no; i++)
    {
        if((pcb(i).topa_virt = dma_alloc_coherent(NULL, IPT_BUF_SIZE, &dma_addr, GFP_KERNEL)))
        {
            pcb(i).topa_phys = (unsigned long long)dma_addr;
        }
        if((pcb(i).iptbuf_virt = dma_alloc_coherent(NULL, IPT_BUF_SIZE, &dma_addr, GFP_KERNEL)))
        {
            pcb(i).iptbuf_phys = (unsigned long long)dma_addr;
        }
    }
    /// check for errors and free all buffers if any
    for(i = 0; i < hardcfg.cpu_no; i++)
    {
        if(!pcb(i).topa_virt)
        {
            dma_free_coherent(NULL, IPT_BUF_SIZE, pcb(i).topa_virt, (dma_addr_t)pcb(i).topa_phys);
            pcb(i).topa_virt = 0;
            pcb(i).topa_phys = 0;
            res = VTSS_ERR_NOMEMORY;
        }
        if(!pcb(i).iptbuf_virt)
        {
            dma_free_coherent(NULL, IPT_BUF_SIZE, pcb(i).iptbuf_virt, (dma_addr_t)pcb(i).iptbuf_phys);
            pcb(i).iptbuf_virt = 0;
            pcb(i).iptbuf_phys = 0;
            res = VTSS_ERR_NOMEMORY;
        }
    }
    return res;
}

static inline long long read_msr(int idx)
{
    long long val;
    rdmsrl(idx, val);
    return val;
}
static void vtss_init_ipt(void)
{
    long long tmp = read_msr(IPT_CONTROL_MSR);

    wrmsrl(IPT_CONTROL_MSR, tmp & ~1L);

    wrmsrl(IPT_CONTROL_MSR, 0);
    wrmsrl(IPT_STATUS_MSR, 0);
    wrmsrl(IPT_OUT_BASE_MSR, 0);
    wrmsrl(IPT_OUT_MASK_MSR, 0);
}

void vtss_ipt_fini(void)
{
    int i;

    for(i = 0; i < hardcfg.cpu_no; i++)
    {
        if(pcb(i).topa_virt)
        {
//            MmFreeNonCachedMemory(pcb(i).topa_virt, IPT_BUF_SIZE);
            dma_free_coherent(NULL, IPT_BUF_SIZE, pcb(i).topa_virt, (dma_addr_t)pcb(i).topa_phys);
            pcb(i).topa_virt = 0;
            pcb(i).topa_phys = 0;
        }
        if(pcb(i).iptbuf_virt)
        {
//            MmFreeNonCachedMemory(pcb[i].iptbuf_virt, IPT_BUF_SIZE);
            dma_free_coherent(NULL, IPT_BUF_SIZE, pcb(i).iptbuf_virt, (dma_addr_t)pcb(i).iptbuf_phys);
            pcb(i).iptbuf_virt = 0;
            pcb(i).iptbuf_phys = 0;
        }
    }
}

int vtss_has_ipt_overflowed(void)
{
    return 0;
}

extern int vtss_lbr_no;
extern int vtss_lbr_msr_ctl;
extern int vtss_lbr_msr_from;
extern int vtss_lbr_msr_to;
extern int vtss_lbr_msr_tos;
extern int vtss_lbr_msr_sel;

void vtss_enable_ipt(void)
{
    int i;
    vtss_pcb_t* pcbp = &pcb_cpu;

    long long tmp = read_msr(IPT_CONTROL_MSR);

    TRACE("enable IPT");
    wrmsrl(IPT_CONTROL_MSR, tmp & ~1L);

    /// disable LBRs and BTS
    wrmsrl(VTSS_DEBUGCTL_MSR, 0);
    
    for(i = 0; i < vtss_lbr_no; i++)
    {
        wrmsrl(vtss_lbr_msr_from + i, 0);
        wrmsrl(vtss_lbr_msr_to + i, 0);
    }
    wrmsrl(vtss_lbr_msr_tos, 0);

    /// form ToPA, and initialize status, base and mask pointers and control MSR
    *((unsigned long long*)pcbp->topa_virt) = pcbp->iptbuf_phys | 0x10;
    *(((unsigned long long*)pcbp->topa_virt) + 1) = pcbp->topa_phys | 0x1;

    wrmsrl(IPT_OUT_MASK_MSR, 0x7f);
    wrmsrl(IPT_OUT_BASE_MSR, pcbp->topa_phys);
    wrmsrl(IPT_STATUS_MSR, 0);

    ///write_msr(IPT_CONTROL_MSR, 0x210c);
    ///write_msr(IPT_CONTROL_MSR, 0x210d);
    wrmsrl(IPT_CONTROL_MSR, 0x2108);  /// user-mode only
    wrmsrl(IPT_CONTROL_MSR, 0x2109);  /// user-mode only
}

void vtss_disable_ipt(void)
{
    long long tmp = read_msr(IPT_CONTROL_MSR);

    wrmsrl(IPT_CONTROL_MSR, tmp & ~1L);
    /// clear control MSR
    wrmsrl(IPT_CONTROL_MSR, 0);
}

void vtss_dump_ipt(struct vtss_transport_data* trnd, int tidx, int cpu, int is_safe)
{
    unsigned short size;

    vtss_pcb_t* pcbp = &pcb_cpu;

    /// form IPT record and save the contents of the output buffer (from base to current mask pointer)

    if((reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_IPT) &&
        hardcfg.family == 0x06 && (hardcfg.model == 0x3d /* BDW */ || hardcfg.model == 0x4e /* SKL */))
    {
#ifdef VTSS_USE_UEC
        ipt_trace_record_t iptrec;
#else
        ipt_trace_record_t* iptrec;
        void* entry;
#endif
        TRACE("IPT before reset: Control = %llX; Status = %llX; Base = %llX; Mask = %llX",
                read_msr(IPT_CONTROL_MSR), read_msr(IPT_STATUS_MSR), read_msr(IPT_OUT_BASE_MSR), read_msr(IPT_OUT_MASK_MSR));
        //vtss_disable_ipt();
        size = (unsigned short)(((unsigned long long)read_msr(IPT_OUT_MASK_MSR) >> 32) & 0xffff);
#ifdef VTSS_USE_UEC
        /// [flagword][residx][cpuidx][tsc][systrace(bts)]
        iptrec.flagword = UEC_LEAF1 | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_SYSTRACE;
        iptrec.residx = (unsigned int)tidx;
        iptrec.cpuidx = (unsigned int)smp_processor_id();
        iptrec.cputsc = vtss_time_cpu();
        iptrec.type = UECSYSTRACE_IPT;

        ///size = 0x100;

        iptrec.size = size + 4;

        if (vtss_transport_record_write(trnd, &iptrec, sizeof(ipt_trace_record_t), pcb_cpu.iptbuf_virt, size, UECMODE_SAFE)) {
            ERROR("vtss_transport_record_write() FAIL");
            return;
        }


#else
        iptrec = (ipt_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(ipt_trace_record_t) + size);
        if (unlikely(!iptrec)) {
            ERROR("vtss_transport_record_reserve() FAIL");
            return;
        }
        /// [flagword][residx][cpuidx][tsc][systrace(bts)]
        iptrec->flagword = UEC_LEAF1 | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_SYSTRACE;
        iptrec->residx = (unsigned int)tidx;
        iptrec->cpuidx = (unsigned int)smp_processor_id();
        iptrec->cputsc = vtss_time_cpu();
        iptrec->size = (unsigned short)(size + sizeof(iptrec->size) + sizeof(iptrec->type));
        iptrec->type = UECSYSTRACE_IPT;
        memcpy(++iptrec, pcb_cpu.iptbuf_virt, size);
        if (vtss_transport_record_commit(trnd, entry, is_safe)){
            ERROR("vtss_transport_record_write() FAIL");
            return;
        }
#endif
        vtss_init_ipt();
        TRACE("IPT after reset: Control = %llX; Status = %llX; Base = %llX; Mask = %llX",
            read_msr(IPT_CONTROL_MSR), read_msr(IPT_STATUS_MSR), read_msr(IPT_OUT_BASE_MSR), read_msr(IPT_OUT_MASK_MSR));
    }
}

