/*
  Copyright (C) 2010-2014 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
#include "vtss_config.h"
#include "record.h"
#include "globals.h"
#include "time.h"
#include "cpuevents.h"

#include <linux/sched.h>
#include <linux/math64.h>
#include <linux/delay.h> // for msleep_interruptible()

#define VTSS_PROCESS_NAME_LEN   0x10

int vtss_record_magic(struct vtss_transport_data* trnd, int is_safe)
{
#ifdef VTSS_USE_UEC
    static unsigned int marker[2] = { UEC_MAGIC, UEC_MAGICVALUE };
    return vtss_transport_record_write(trnd, marker, sizeof(marker), NULL, 0, is_safe);
#else
    int rc = -EFAULT;
    void* entry;
    unsigned int* p = (unsigned int*)vtss_transport_record_reserve(trnd, &entry, 2*sizeof(unsigned int));
    if (likely(p)) {
        *p++ = UEC_MAGIC;
        *p++ = UEC_MAGICVALUE;
        rc = vtss_transport_record_commit(trnd, entry, is_safe);
    }
    return rc;
#endif
}

int vtss_record_debug_info(struct vtss_transport_data* trnd, const char* message, int is_safe)
{
    int rc = 0;

    if (likely(message != NULL)) {
        size_t msglen = strlen(message) + 1;
#ifdef VTSS_USE_UEC
        debug_info_record_t dbgrec;
        dbgrec.flagword = UEC_LEAF1 | UECL1_USERTRACE;
        dbgrec.size = msglen + sizeof(dbgrec.size) + sizeof(dbgrec.type);
        dbgrec.type = UECSYSTRACE_DEBUG;
        rc = vtss_transport_record_write(trnd, &dbgrec, sizeof(dbgrec), (void*)message, msglen, is_safe);
#else
        void* entry;
        debug_info_record_t* dbgrec = (debug_info_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(debug_info_record_t) + msglen);
        if (likely(dbgrec)) {
            dbgrec->flagword = UEC_LEAF1 | UECL1_USERTRACE;
            dbgrec->size = msglen + sizeof(dbgrec->size) + sizeof(dbgrec->type);
            dbgrec->type = UECSYSTRACE_DEBUG;
            memcpy(++dbgrec, message, msglen);
            rc = vtss_transport_record_commit(trnd, entry, is_safe);
        }
#endif
    }
    return rc;
}

int vtss_record_process_exec(struct vtss_transport_data* trnd, pid_t tid, pid_t pid, int cpu, const char *filename, int is_safe)
{
    size_t namelen = filename ? strlen(filename) + 1 : 0;
#ifdef VTSS_USE_UEC
    prc_trace_record_t procrec;
    procrec.flagword = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_CPUIDX | UECL1_USRLVLID | UECL1_CPUTSC | UECL1_REALTSC | UECL1_SYSTRACE;
    procrec.activity = UECACT_NEWTASK;
    procrec.cpuidx   = cpu;
    procrec.pid      = pid;
    procrec.tid      = tid;
    vtss_time_get_sync(&procrec.cputsc, &procrec.realtsc);
    procrec.size     = namelen + sizeof(procrec.size) + sizeof(procrec.type);
    procrec.type     = UECSYSTRACE_PROCESS_NAME;
    return vtss_transport_record_write(trnd, &procrec, sizeof(procrec), (void*)filename, namelen, is_safe);
#else
    int rc = -EFAULT;
    void* entry;
    prc_trace_record_t* procrec = (prc_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(prc_trace_record_t) + namelen);
    if (likely(procrec)) {
        procrec->flagword = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_CPUIDX | UECL1_USRLVLID | UECL1_CPUTSC | UECL1_REALTSC | UECL1_SYSTRACE;
        procrec->activity = UECACT_NEWTASK;
        procrec->cpuidx   = cpu;
        procrec->pid      = pid;
        procrec->tid      = tid;
        vtss_time_get_sync(&procrec->cputsc, &procrec->realtsc);
        procrec->size     = namelen + sizeof(procrec->size) + sizeof(procrec->type);
        procrec->type     = UECSYSTRACE_PROCESS_NAME;
        if (likely(namelen))
            memcpy(++procrec, filename, namelen);
        rc = vtss_transport_record_commit(trnd, entry, is_safe);
    }
    return rc;
#endif
}

int vtss_record_process_exit(struct vtss_transport_data* trnd, pid_t tid, pid_t pid, int cpu, const char *filename, int is_safe)
{
    size_t namelen = filename ? strlen(filename) + 1 : 0;

#ifdef VTSS_USE_UEC
    prc_trace_record_t procrec;
    procrec.flagword = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_CPUIDX | UECL1_USRLVLID | UECL1_CPUTSC | UECL1_REALTSC | UECL1_SYSTRACE;
    procrec.activity = UECACT_OLDTASK;
    procrec.cpuidx   = cpu;
    procrec.pid      = pid;
    procrec.tid      = tid;
    vtss_time_get_sync(&procrec.cputsc, &procrec.realtsc);
    procrec.size     = namelen + sizeof(procrec.size) + sizeof(procrec.type);
    procrec.type     = UECSYSTRACE_PROCESS_NAME;
    return vtss_transport_record_write(trnd, &procrec, sizeof(procrec), (void*)filename, namelen, is_safe);
#else
    int rc = -EFAULT;
    void* entry;
    prc_trace_record_t* procrec = (prc_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(prc_trace_record_t) + namelen);
    if (likely(procrec)) {
        procrec->flagword = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_CPUIDX | UECL1_USRLVLID | UECL1_CPUTSC | UECL1_REALTSC | UECL1_SYSTRACE;
        procrec->activity = UECACT_OLDTASK;
        procrec->cpuidx   = cpu;
        procrec->pid      = pid;
        procrec->tid      = tid;
        vtss_time_get_sync(&procrec->cputsc, &procrec->realtsc);
        procrec->size     = namelen + sizeof(procrec->size) + sizeof(procrec->type);
        procrec->type     = UECSYSTRACE_PROCESS_NAME;
        if (likely(namelen))
            memcpy(++procrec, filename, namelen);
        rc = vtss_transport_record_commit(trnd, entry, is_safe);
    }
    return rc;
#endif
}

int vtss_record_thread_create(struct vtss_transport_data* trnd, pid_t tid, pid_t pid, int cpu, int is_safe)
{
#ifdef VTSS_USE_UEC
    nth_trace_record_t threc;
    threc.flagword = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_USRLVLID | UECL1_CPUTSC | UECL1_REALTSC;
    threc.activity = UECACT_NEWTASK;
    threc.residx   = tid;
    threc.cpuidx   = cpu;
    threc.pid      = pid;
    threc.tid      = tid;
    vtss_time_get_sync(&threc.cputsc, &threc.realtsc);
    return vtss_transport_record_write(trnd, &threc, sizeof(threc), NULL, 0, is_safe);
#else
    int rc = -EFAULT;
    void* entry;
    nth_trace_record_t* threc = (nth_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(nth_trace_record_t));
    if (likely(threc)) {
        threc->flagword = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_USRLVLID | UECL1_CPUTSC | UECL1_REALTSC;
        threc->activity = UECACT_NEWTASK;
        threc->residx   = tid;
        threc->cpuidx   = cpu;
        threc->pid      = pid;
        threc->tid      = tid;
        vtss_time_get_sync(&threc->cputsc, &threc->realtsc);
        rc = vtss_transport_record_commit(trnd, entry, is_safe);
    }
    return rc;
#endif
}

int vtss_record_thread_stop(struct vtss_transport_data* trnd, pid_t tid, pid_t pid, int cpu, int is_safe)
{
#ifdef VTSS_USE_UEC
    nth_trace_record_t threc;
    threc.flagword = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_USRLVLID | UECL1_CPUTSC | UECL1_REALTSC;
    threc.activity = UECACT_OLDTASK;
    threc.residx   = tid;
    threc.cpuidx   = cpu;
    threc.pid      = pid;
    threc.tid      = tid;
    vtss_time_get_sync(&threc.cputsc, &threc.realtsc);
    return vtss_transport_record_write(trnd, &threc, sizeof(threc), NULL, 0, is_safe);
#else
    int rc = -EFAULT;
    void* entry;
    nth_trace_record_t* threc = (nth_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(nth_trace_record_t));
    if (likely(threc)) {
        threc->flagword = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_USRLVLID | UECL1_CPUTSC | UECL1_REALTSC;
        threc->activity = UECACT_OLDTASK;
        threc->residx   = tid;
        threc->cpuidx   = cpu;
        threc->pid      = pid;
        threc->tid      = tid;
        vtss_time_get_sync(&threc->cputsc, &threc->realtsc);
        rc = vtss_transport_record_commit(trnd, entry, is_safe);
    }
    return rc;
#endif
}

int vtss_record_switch_from(struct vtss_transport_data* trnd, int cpu, int is_preempt, int is_safe)
{
#ifdef VTSS_USE_UEC
    cto_trace_record_t ctxrec;
    ctxrec.sysout.flagword = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_REALTSC;
    ctxrec.sysout.activity = (is_preempt ? 0 : UECACT_SYNCHRO) | UECACT_SWITCHFROM;
    ctxrec.sysout.cpuidx   = cpu;
    vtss_time_get_sync(&ctxrec.sysout.cputsc, &ctxrec.sysout.realtsc);
    return vtss_transport_record_write(trnd, &ctxrec, sizeof(ctxrec.sysout), NULL, 0, is_safe);
#else
    int rc = -EFAULT;
    void* entry;
    cto_trace_record_t* ctxrec = (cto_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(ctxrec->sysout));
    if (likely(ctxrec)) {
        ctxrec->sysout.flagword = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_REALTSC;
        ctxrec->sysout.activity = (is_preempt ? 0 : UECACT_SYNCHRO) | UECACT_SWITCHFROM;
        ctxrec->sysout.cpuidx   = cpu;
        vtss_time_get_sync(&ctxrec->sysout.cputsc, &ctxrec->sysout.realtsc);
        rc = vtss_transport_record_commit(trnd, entry, is_safe);
    }
    return rc;
#endif
}

int vtss_record_switch_to(struct vtss_transport_data* trnd, pid_t tid, int cpu, void* ip, int is_safe)
{
#ifdef VTSS_USE_UEC
    cti_trace_record_t ctxrec;
    ctxrec.procina.flagword = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_REALTSC | UECL1_EXECADDR;
    ctxrec.procina.activity = UECACT_SWITCHTO;
    ctxrec.procina.residx   = tid;
    ctxrec.procina.cpuidx   = cpu;
    vtss_time_get_sync(&ctxrec.procina.cputsc, &ctxrec.procina.realtsc);
    ctxrec.procina.execaddr = (unsigned long long)(size_t)ip;
    return vtss_transport_record_write(trnd, &ctxrec, sizeof(ctxrec.procina), NULL, 0, is_safe);
#else
    int rc = -EFAULT;
    void* entry;
    cti_trace_record_t* ctxrec = (cti_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(ctxrec->procina));
    if (likely(ctxrec)) {
        ctxrec->procina.flagword = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_REALTSC | UECL1_EXECADDR;
        ctxrec->procina.activity = UECACT_SWITCHTO;
        ctxrec->procina.residx   = tid;
        ctxrec->procina.cpuidx   = cpu;
        vtss_time_get_sync(&ctxrec->procina.cputsc, &ctxrec->procina.realtsc);
        ctxrec->procina.execaddr = (unsigned long long)(size_t)ip;
        rc = vtss_transport_record_commit(trnd, entry, is_safe);
    }
    return rc;
#endif
}

#define VTSS_MAX_ACTIVE_CPUEVENTS VTSS_CFG_CHAIN_SIZE/10

int vtss_record_sample(struct vtss_transport_data* trnd, pid_t tid, int cpu, cpuevent_t* cpuevent_chain, void* ip, int is_safe)
{
    int i, j, n, rc = -EFAULT;
    event_trace_record_t eventrec;

#ifdef VTSS_USE_UEC
    unsigned long flags;
    unsigned long long* scratch;

    local_irq_save(flags);
    scratch = (unsigned long long*)pcb_cpu.scratch_ptr;
    for (i = 0, j = 0; i < VTSS_CFG_CHAIN_SIZE; i++) {
        if (!cpuevent_chain[i].valid) {
            break;
        }
        if (cpuevent_chain[i].mux_grp != cpuevent_chain[i].mux_idx) {
            continue;
        }
        scratch[j++] = cpuevent_chain[i].count;
        if (j >= VTSS_MAX_ACTIVE_CPUEVENTS) {
            ERROR("MAX active cpuevents is reached");
            break;
        }
    }
    scratch[j] = (unsigned long long)(size_t)ip;
    if (ip != NULL) {
        eventrec.sperec.flagword = UEC_VECTORED | UEC_LEAF1 | UECL1_ACTIVITY | UECL1_VRESIDX |
            UECL1_CPUIDX | UECL1_CPUTSC | UECL1_MUXGROUP | UECL1_CPUEVENT | UECL1_EXECADDR;
        eventrec.sperec.vectored = UECL1_CPUEVENT;
        eventrec.sperec.activity = UECACT_SAMPLED;
        eventrec.sperec.residx   = tid;
        eventrec.sperec.cpuidx   = cpu;
        eventrec.sperec.cputsc   = vtss_time_cpu();
        eventrec.sperec.muxgroup = cpuevent_chain[0].mux_idx;
        eventrec.sperec.event_no = j;
        rc = vtss_transport_record_write(trnd, &eventrec.sperec, sizeof(eventrec.sperec), scratch, (j+1)*sizeof(unsigned long long), is_safe);
    } else {
        eventrec.gperec.flagword = UEC_VECTORED | UEC_LEAF1 | UECL1_VRESIDX |
            UECL1_CPUIDX | UECL1_CPUTSC | UECL1_MUXGROUP | UECL1_CPUEVENT;
        eventrec.gperec.vectored = UECL1_CPUEVENT;
        eventrec.gperec.residx   = tid;
        eventrec.gperec.cpuidx   = cpu;
        eventrec.gperec.cputsc   = vtss_time_cpu();
        eventrec.gperec.muxgroup = cpuevent_chain[0].mux_idx;
        eventrec.gperec.event_no = j;
        rc = vtss_transport_record_write(trnd, &eventrec.gperec, sizeof(eventrec.gperec), scratch, j*sizeof(unsigned long long), is_safe);
    }
    local_irq_restore(flags);
#else
    unsigned long long* counters;

    for (i = 0, n = 0; i < VTSS_CFG_CHAIN_SIZE; i++) {
        if (!cpuevent_chain[i].valid) {
            break;
        }
        if (cpuevent_chain[i].mux_grp != cpuevent_chain[i].mux_idx) {
            continue;
        }
        if (++n >= VTSS_MAX_ACTIVE_CPUEVENTS) {
            ERROR("MAX active cpuevents is reached");
            break;
        }
    }
    if (ip != NULL) {
        void* entry;
        event_trace_record_t* eventrec = (event_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(eventrec->sperec) + (n+1)*sizeof(unsigned long long));
        if (likely(eventrec)) {
            eventrec->sperec.flagword = UEC_VECTORED | UEC_LEAF1 | UECL1_ACTIVITY | UECL1_VRESIDX |
                UECL1_CPUIDX | UECL1_CPUTSC | UECL1_MUXGROUP | UECL1_CPUEVENT | UECL1_EXECADDR;
            eventrec->sperec.vectored = UECL1_CPUEVENT;
            eventrec->sperec.activity = UECACT_SAMPLED;
            eventrec->sperec.residx   = tid;
            eventrec->sperec.cpuidx   = cpu;
            eventrec->sperec.cputsc   = vtss_time_cpu();
            eventrec->sperec.muxgroup = cpuevent_chain[0].mux_idx;
            eventrec->sperec.event_no = n;

            counters = (unsigned long long*)((char*)eventrec+sizeof(eventrec->sperec));
            for (i = 0, j = 0; i < VTSS_CFG_CHAIN_SIZE; i++) {
                if (!cpuevent_chain[i].valid) {
                    break;
                }
                if (cpuevent_chain[i].mux_grp != cpuevent_chain[i].mux_idx) {
                    continue;
                }
                counters[j++] = cpuevent_chain[i].count;
                if (j >= VTSS_MAX_ACTIVE_CPUEVENTS) {
                    break;
                }
            }
            counters[j] = (unsigned long long)(size_t)ip;
            rc = vtss_transport_record_commit(trnd, entry, is_safe);
        }
    } else {
        void* entry;
        event_trace_record_t* eventrec = (event_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(eventrec->gperec) + n*sizeof(unsigned long long));
        if (likely(eventrec)) {
            eventrec->gperec.flagword = UEC_VECTORED | UEC_LEAF1 | UECL1_VRESIDX |
                UECL1_CPUIDX | UECL1_CPUTSC | UECL1_MUXGROUP | UECL1_CPUEVENT;
            eventrec->gperec.vectored = UECL1_CPUEVENT;
            eventrec->gperec.residx   = tid;
            eventrec->gperec.cpuidx   = cpu;
            eventrec->gperec.cputsc   = vtss_time_cpu();
            eventrec->gperec.muxgroup = cpuevent_chain[0].mux_idx;
            eventrec->gperec.event_no = n;

            counters = (unsigned long long*)((char*)eventrec+sizeof(eventrec->gperec));
            for (i = 0, j = 0; i < VTSS_CFG_CHAIN_SIZE; i++) {
                if (!cpuevent_chain[i].valid) {
                    break;
                }
                if (cpuevent_chain[i].mux_grp != cpuevent_chain[i].mux_idx) {
                    continue;
                }
                counters[j++] = cpuevent_chain[i].count;
                if (j >= VTSS_MAX_ACTIVE_CPUEVENTS) {
                    break;
                }
            }
            rc = vtss_transport_record_commit(trnd, entry, is_safe);
        }
    }
#endif
    return rc;
}

int vtss_record_bts(struct vtss_transport_data* trnd, pid_t tid, int cpu, void* bts_buff, size_t bts_size, int is_safe)
{
#ifdef VTSS_USE_UEC
    bts_trace_record_t btsrec;

    if (bts_size >= ((unsigned short)~0)-4)
        return -1;
    /// generate branch trace record
    /// [flagword][residx][cpuidx][tsc][systrace(bts)]
    btsrec.flagword = UEC_LEAF1 | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_SYSTRACE;
    btsrec.residx   = tid;
    btsrec.cpuidx   = cpu;
    btsrec.cputsc   = vtss_time_cpu();
    btsrec.size     = (unsigned short)(bts_size + sizeof(btsrec.size) + sizeof(btsrec.type));
    btsrec.type     = UECSYSTRACE_BRANCH_V0;
    return vtss_transport_record_write(trnd, &btsrec, sizeof(bts_trace_record_t), bts_buff, bts_size, is_safe);
#else
    int rc = -EFAULT;
    void* entry;
    bts_trace_record_t* btsrec;

    if (bts_size >= ((unsigned short)~0)-4)
        return rc;
    btsrec = (bts_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(bts_trace_record_t) + bts_size);
    if (likely(btsrec)) {
        /// generate branch trace record
        /// [flagword][residx][cpuidx][tsc][systrace(bts)]
        btsrec->flagword = UEC_LEAF1 | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_SYSTRACE;
        btsrec->residx   = tid;
        btsrec->cpuidx   = cpu;
        btsrec->cputsc   = vtss_time_cpu();
        btsrec->size     = (unsigned short)(bts_size + sizeof(btsrec->size) + sizeof(btsrec->type));
        btsrec->type     = UECSYSTRACE_BRANCH_V0;
        memcpy(++btsrec, bts_buff, bts_size);
        rc = vtss_transport_record_commit(trnd, entry, is_safe);
    }
    return rc;
#endif
}

int vtss_record_module(struct vtss_transport_data* trnd, int m32, unsigned long addr, unsigned long size, const char *pname, unsigned long pgoff, long long cputsc, long long realtsc, int is_safe)
{
#ifdef VTSS_USE_UEC
    dlm_trace_record_t modrec;

    modrec.flagword = UEC_LEAF1 | UECL1_USRLVLID | UECL1_CPUTSC | UECL1_REALTSC | UECL1_SYSTRACE;
    modrec.pid      = 0;
    modrec.tid      = 0;
    //vtss_time_get_sync(&modrec.cputsc, &modrec.realtsc);
    modrec.cputsc = cputsc;
    modrec.realtsc = realtsc;
    modrec.start  = addr;
    modrec.end    = addr + size;
    modrec.offset = pgoff << PAGE_SHIFT;
    modrec.bin    = (unsigned char)MODTYPE_ELF;
    modrec.len    = strlen(pname) + 1;
    modrec.size   = (unsigned short)(sizeof(modrec) - (size_t)((char*)&modrec.size - (char*)&modrec.flagword) + modrec.len);

#ifdef CONFIG_X86_64
    modrec.type = m32 ? UECSYSTRACE_MODULE_MAP32 : UECSYSTRACE_MODULE_MAP64;
    /* convert the structure from 64 to 32 bits */
    if (modrec.type == UECSYSTRACE_MODULE_MAP32) {
        dlm_trace_record_32_t modrec32;

        modrec32.flagword= modrec.flagword;
        modrec32.pid     = modrec.pid;
        modrec32.tid     = modrec.tid;
        modrec32.cputsc  = modrec.cputsc;
        modrec32.realtsc = modrec.realtsc;
        modrec32.type    = modrec.type;
        modrec32.start   = (unsigned int)modrec.start;
        modrec32.end     = (unsigned int)modrec.end;
        modrec32.offset  = (unsigned int)modrec.offset;
        modrec32.bin     = modrec.bin;
        modrec32.len     = modrec.len;
        modrec32.size    = (unsigned short)(sizeof(modrec32) - (size_t)((char*)&modrec32.size - (char*)&modrec32.flagword) + modrec32.len);
        return vtss_transport_record_write(trnd, &modrec32, sizeof(modrec32), (void*)pname, modrec32.len, is_safe);
    }
#else  /* CONFIG_X86_64 */
    modrec.type = UECSYSTRACE_MODULE_MAP32;
#endif /* CONFIG_X86_64 */
    return vtss_transport_record_write(trnd, &modrec, sizeof(modrec), (void*)pname, modrec.len, is_safe);
#else  /* VTSS_USE_UEC */
    int rc = -EFAULT;
    void* entry;
    size_t namelen = pname ? strlen(pname) + 1 : 0;

#ifdef CONFIG_X86_64
    if (m32) {
        dlm_trace_record_32_t* modrec = (dlm_trace_record_32_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(dlm_trace_record_32_t) + namelen);
        if (unlikely(!modrec)){
            //If transport is not ready we are in "cmd_start => target_new".
            //During attach the transport will not attach till client get response from driver,
            //so, it's no sense to wait for the case
            if (vtss_transport_is_ready(trnd) && (!irqs_disabled())){
                //try again
                int cnt = 20;
                while ((!modrec) && cnt > 0){
                    msleep_interruptible(100);
                    cnt--;
                    modrec = (dlm_trace_record_32_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(dlm_trace_record_32_t) + namelen);
                }
            }
        }
        if (likely(modrec)) {
            modrec->flagword = UEC_LEAF1 | UECL1_USRLVLID | UECL1_CPUTSC | UECL1_REALTSC | UECL1_SYSTRACE;
            modrec->pid      = 0;
            modrec->tid      = 0;
            //vtss_time_get_sync(&modrec->cputsc, &modrec->realtsc);
            modrec->cputsc = cputsc;
            modrec->realtsc = realtsc;

            modrec->start  = (unsigned int)addr;
            modrec->end    = (unsigned int)(addr + size);
            modrec->offset = (unsigned int)(pgoff << PAGE_SHIFT);
            modrec->bin    = (unsigned char)MODTYPE_ELF;
            modrec->len    = namelen;
            modrec->size   = (unsigned short)(sizeof(dlm_trace_record_32_t) - (size_t)((char*)&modrec->size - (char*)&modrec->flagword) + namelen);
            modrec->type   = UECSYSTRACE_MODULE_MAP32;

            if (namelen)
                memcpy(++modrec, pname, namelen);
            rc = vtss_transport_record_commit(trnd, entry, is_safe);
        }
    } else {
        dlm_trace_record_t* modrec = (dlm_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(dlm_trace_record_t) + namelen);
        if (unlikely(!modrec)){
            if (vtss_transport_is_ready(trnd) && (!irqs_disabled())){
                //try again
                int cnt = 20;
                while ((!modrec) && cnt > 0){
                    TRACE("record awaiting transport ready");
                    msleep_interruptible(100);
                    cnt--;
                    modrec = (dlm_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(dlm_trace_record_t) + namelen);
                }
            }
        }
        if (likely(modrec)) {
            modrec->flagword = UEC_LEAF1 | UECL1_USRLVLID | UECL1_CPUTSC | UECL1_REALTSC | UECL1_SYSTRACE;
            modrec->pid      = 0;
            modrec->tid      = 0;
//            vtss_time_get_sync(&modrec->cputsc, &modrec->realtsc);

            modrec->cputsc = cputsc;
            modrec->realtsc = realtsc;

            modrec->start  = addr;
            modrec->end    = addr + size;
            modrec->offset = pgoff << PAGE_SHIFT;
            modrec->bin    = (unsigned char)MODTYPE_ELF;
            modrec->len    = namelen;
            modrec->size   = (unsigned short)(sizeof(dlm_trace_record_t) - (size_t)((char*)&modrec->size - (char*)&modrec->flagword) + namelen);
            modrec->type   = UECSYSTRACE_MODULE_MAP64;

            if (namelen)
                memcpy(++modrec, pname, namelen);
            rc = vtss_transport_record_commit(trnd, entry, is_safe);
        }
    }
#else  /* CONFIG_X86_64 */
    dlm_trace_record_t* modrec = (dlm_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(dlm_trace_record_t) + namelen);
    if (likely(modrec)) {
        modrec->flagword = UEC_LEAF1 | UECL1_USRLVLID | UECL1_CPUTSC | UECL1_REALTSC | UECL1_SYSTRACE;
        modrec->pid      = 0;
        modrec->tid      = 0;
  //      vtss_time_get_sync(&modrec->cputsc, &modrec->realtsc);
        modrec->cputsc = cputsc;
        modrec->realtsc = realtsc;


        modrec->start  = addr;
        modrec->end    = addr + size;
        modrec->offset = pgoff << PAGE_SHIFT;
        modrec->bin    = (unsigned char)MODTYPE_ELF;
        modrec->len    = namelen;
        modrec->size   = (unsigned short)(sizeof(dlm_trace_record_t) - (size_t)((char*)&modrec->size - (char*)&modrec->flagword) + namelen);
        modrec->type   = UECSYSTRACE_MODULE_MAP32;

        if (namelen)
            memcpy(++modrec, pname, namelen);
        rc = vtss_transport_record_commit(trnd, entry, is_safe);
    }
#endif /* CONFIG_X86_64 */
    return rc;
#endif /* VTSS_USE_UEC */
}

int vtss_record_configs(struct vtss_transport_data* trnd, int m32, int is_safe)
{
    int rc = 0;
    unsigned short size;

    /* Fixup maxusr_address */
#ifdef CONFIG_X86_64
    hardcfg.maxusr_address = m32 ? IA32_PAGE_OFFSET : PAGE_OFFSET;
#else
    hardcfg.maxusr_address = PAGE_OFFSET;
#endif
    TRACE("hardcfg.maxusr_address=0x%llx", hardcfg.maxusr_address);

    /* generate forward compatibility format record (always) */
    {
        fcf_trace_record_t fcfrec;
        /// [flagword][systrace(fmtcfg)]
        fcfrec.flagword = UEC_LEAF1 | UECL1_SYSTRACE;
        fcfrec.size     = sizeof(fmtcfg) + sizeof(fcfrec.size) + sizeof(fcfrec.type);
        fcfrec.type     = UECSYSTRACE_FMTCFG;
        rc |= vtss_transport_record_write(trnd, &fcfrec, sizeof(fcfrec), (void*)fmtcfg, sizeof(fmtcfg), is_safe);
    }

    /* generate collector configuration record */
    {
        static const char colname[] = "vtss++ (" VTSS_TO_STR(VTSS_VERSION_STRING) ")";
        col_trace_record_t colrec;
        /// [flagword][systrace(colcfg)]
        colrec.flagword = UEC_LEAF1 | UECL1_SYSTRACE;
        colrec.size     = sizeof(col_trace_record_t) - sizeof(colrec.flagword) + sizeof(colname);
        colrec.type     = UECSYSTRACE_COLCFG;

        colrec.version  = 1;
        colrec.major    = VTSS_VERSION_MAJOR;
        colrec.minor    = VTSS_VERSION_MINOR;
        colrec.revision = VTSS_VERSION_REVISION;
        colrec.features =
            VTSS_CFGTRACE_CPUEV  | VTSS_CFGTRACE_SWCFG  |
            VTSS_CFGTRACE_HWCFG  | VTSS_CFGTRACE_SAMPLE  | VTSS_CFGTRACE_TP     |
            VTSS_CFGTRACE_MODULE | VTSS_CFGTRACE_PROCTHR |
            VTSS_CFGTRACE_BRANCH | VTSS_CFGTRACE_EXECTX  | VTSS_CFGTRACE_TBS    |
            VTSS_CFGTRACE_LASTBR | VTSS_CFGTRACE_TREE    | VTSS_CFGTRACE_SYNCARG;
        colrec.features |= reqcfg.trace_cfg.trace_flags;
        colrec.len = (unsigned char)sizeof(colname);
        rc |= vtss_transport_record_write(trnd, &colrec, sizeof(colrec), (void*)colname, sizeof(colname), is_safe);
    }

    /* generate system configuration record */
    {
        sys_trace_record_t sysrec;
        size = syscfg.record_size;
        /// [flagword][systrace(sysinfo)]
        sysrec.flagword = UEC_LEAF1 | UECL1_SYSTRACE;
        sysrec.size     = size + sizeof(sysrec.size) + sizeof(sysrec.type);
        sysrec.type     = UECSYSTRACE_SYSINFO;
        rc |= vtss_transport_record_write(trnd, &sysrec, sizeof(sysrec), (void*)&syscfg, size, is_safe);
    }

    /* generate hardware configuration record */
    {
        hcf_trace_record_t hcfrec;
        size = (unsigned short)(sizeof(hardcfg) - (NR_CPUS - hardcfg.cpu_no) * sizeof(hardcfg.cpu_map[0]));
        /// [flagword][systrace(hwcfg)]
        hcfrec.flagword = UEC_LEAF1 | UECL1_SYSTRACE;
        hcfrec.size     = size + sizeof(hcfrec.size) + sizeof(hcfrec.type);
        hcfrec.type     = UECSYSTRACE_HWCFG;
        hardcfg.timer_freq = vtss_freq_real(); /* update real timer freq */
        rc |= vtss_transport_record_write(trnd, &hcfrec, sizeof(hcfrec), (void*)&hardcfg, size, is_safe);
    }

    /* generate time marker record */
    {
        struct timespec now;
        time_marker_record_t timark;
        timark.flagword = UEC_LEAF1 | UEC_VECTORED | UECL1_REALTSC;
        timark.vectored = UECL1_REALTSC;
        timark.vec_no   = 2;
        timark.tsc      = vtss_time_real();
        getnstimeofday(&now);
        /* convert global time to 100ns units */
        timark.utc      = div64_u64((u64)timespec_to_ns(&now), 100ULL);
        rc |= vtss_transport_record_write(trnd, &timark, sizeof(timark), NULL, 0, is_safe);
    }

    return rc;
}

int vtss_record_softcfg(struct vtss_transport_data* trnd, pid_t tid, int is_safe)
{
    int i, rc = 0;
    int evtsize;
    vtss_softcfg_t* softcfg;
    unsigned short size;
    unsigned long flags;
    unsigned short exectx_len;
    scf_trace_record_t scfrec;

    if (reqcfg.cpuevent_count_v1 > 0) {
        local_irq_save(flags);
        softcfg = (vtss_softcfg_t*)pcb_cpu.scratch_ptr;

        softcfg->version = 2;
        size = sizeof(int) + sizeof(short); // version + cpu_chain_len

        for (i = 0; i < reqcfg.cpuevent_count_v1; i++) {
            evtsize = reqcfg.cpuevent_cfg_v1[i].reqsize;

            if (size + evtsize >= VTSS_DYNSIZE_SCRATCH) {
                break;
            }
            /// copy event configuration
            memcpy((char*)softcfg + size, &reqcfg.cpuevent_cfg_v1[i], sizeof(cpuevent_cfg_v1_t));
            /// adjust event name and description offsets
            ((cpuevent_cfg_v1_t*)((char*)softcfg + size))->name_off =
                sizeof(cpuevent_cfg_v1_t);
            ((cpuevent_cfg_v1_t*)((char*)softcfg + size))->desc_off =
                sizeof(cpuevent_cfg_v1_t) + reqcfg.cpuevent_cfg_v1[i].name_len;
            /// copy event name and description
            size += sizeof(cpuevent_cfg_v1_t);
            memcpy((char*)softcfg + size,
                   (char*)&reqcfg.cpuevent_cfg_v1[i] + reqcfg.cpuevent_cfg_v1[i].name_off,
                   reqcfg.cpuevent_cfg_v1[i].name_len);
            size += reqcfg.cpuevent_cfg_v1[i].name_len;
            memcpy((char*)softcfg + size,
                   (char*)&reqcfg.cpuevent_cfg_v1[i] + reqcfg.cpuevent_cfg_v1[i].desc_off,
                   reqcfg.cpuevent_cfg_v1[i].desc_len);
            size += reqcfg.cpuevent_cfg_v1[i].desc_len;
        }
        softcfg->cpu_chain_len = i;
        /// generate software configuration record
        /// [flagword][systrace(swcfg)]
        scfrec.flagword = UEC_LEAF1 | UECL1_VRESIDX | UECL1_SYSTRACE;
        scfrec.vresidx = (unsigned)tid;
        scfrec.size = size + sizeof(scfrec.size) + sizeof(scfrec.type);
        scfrec.type = UECSYSTRACE_SWCFG;
        rc |= vtss_transport_record_write(trnd, &scfrec, sizeof(scfrec), (void*)softcfg, size, is_safe);
        local_irq_restore(flags);
    }
    return rc;
}

int vtss_record_probe(struct vtss_transport_data* trnd, int cpu, int fid, int is_safe)
{
    prb_trace_record_t proberec;

    proberec.flagword  = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_USERTRACE;
    proberec.activity  = UECACT_PROBED;
    /* it's a global probe and TID isn't important here */
    proberec.residx    = 0;
    proberec.cpuidx    = cpu;
    /* NOTE: it's real TSC to be consistent with TPSS */
    proberec.cputsc    = vtss_time_real();
    proberec.size      = sizeof(prb_trace_record_t) - offsetof(prb_trace_record_t, size);
    /* arch isn't important here but make it as native arch */
#ifdef CONFIG_X86_64
    proberec.type      = URT_APIWRAP64_V1;
#else
    proberec.type      = URT_APIWRAP32_V1;
#endif
    proberec.entry_tsc = proberec.cputsc;
    proberec.entry_cpu = proberec.cpuidx;
    proberec.fid       = fid;
    return vtss_transport_record_write(trnd, &proberec, sizeof(proberec), NULL, 0, is_safe);
}

int vtss_record_probe_all(int cpu, int fid, int is_safe)
{
    prb_trace_record_t proberec;

    proberec.flagword  = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_USERTRACE;
    proberec.activity  = UECACT_PROBED;
    /* it's a global probe and TID isn't important here */
    proberec.residx    = 0;
    proberec.cpuidx    = cpu;
    /* NOTE: it's real TSC to be consistent with TPSS */
    proberec.cputsc    = vtss_time_real();
    proberec.size      = sizeof(prb_trace_record_t) - offsetof(prb_trace_record_t, size);
    /* arch isn't important here but make it as native arch */
#ifdef CONFIG_X86_64
    proberec.type      = URT_APIWRAP64_V1;
#else
    proberec.type      = URT_APIWRAP32_V1;
#endif
    proberec.entry_tsc = proberec.cputsc;
    proberec.entry_cpu = proberec.cpuidx;
    proberec.fid       = fid;
    return vtss_transport_record_write_all(&proberec, sizeof(proberec), NULL, 0, is_safe);
}

int vtss_record_thread_name(struct vtss_transport_data* trnd, pid_t tid, const char *taskname, int is_safe)
{
    size_t namelen = taskname ? strlen(taskname) + 1 : 0;
#ifdef VTSS_USE_UEC
    thname_trace_record_t namerec;
    namerec.probe.flagword  = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_USERTRACE;
    namerec.probe.activity  = UECACT_PROBED;
    namerec.probe.residx    = tid;
    namerec.probe.cpuidx    = 0;
    namerec.probe.cputsc    = vtss_time_real();

    namerec.probe.size      = sizeof(thname_trace_record_t) - offsetof(prb_trace_record_t, size) + namelen;
#ifdef CONFIG_X86_64
    namerec.probe.type      = URT_APIWRAP64_V1;
#else
    proberec.probe.type     = URT_APIWRAP32_V1;
#endif
    namerec.probe.entry_tsc = namerec.probe.cputsc;
    namerec.probe.entry_cpu = namerec.probe.cpuidx;
    namerec.probe.fid       = FID_THREAD_NAME;

    namerec.version  = 1;
    namerec.length   = namelen;

    return vtss_transport_record_write(trnd, &namerec, sizeof(namerec), (void*)taskname, namelen, is_safe);
#else
    int rc = -EFAULT;
    void* entry;
    thname_trace_record_t* namerec = (thname_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(thname_trace_record_t) + namelen);
    if (likely(namerec)) {
        namerec->probe.flagword  = UEC_LEAF1 | UECL1_ACTIVITY | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_USERTRACE;
        namerec->probe.activity  = UECACT_PROBED;
        namerec->probe.residx    = tid;
        namerec->probe.cpuidx    = 0;
        namerec->probe.cputsc    = vtss_time_real();

        namerec->probe.size      = sizeof(thname_trace_record_t) - offsetof(prb_trace_record_t, size) + namelen;
#ifdef CONFIG_X86_64
        namerec->probe.type      = URT_APIWRAP64_V1;
#else
        namerec->probe.type      = URT_APIWRAP32_V1;
#endif
        namerec->probe.entry_tsc = namerec->probe.cputsc;
        namerec->probe.entry_cpu = namerec->probe.cpuidx;
        namerec->probe.fid       = FID_THREAD_NAME;
        
        namerec->version  = 1;
        namerec->length   = namelen;
        memcpy(++namerec, taskname, namelen);
        rc = vtss_transport_record_commit(trnd, entry, is_safe);
    }
    return rc;
#endif
}
