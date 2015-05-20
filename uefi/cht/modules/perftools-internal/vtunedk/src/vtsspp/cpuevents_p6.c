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

/// P6 event control virtual functions
static void vf_p6_start(cpuevent_t* this)
{
}

static void vf_p6_stop(cpuevent_t* this)
{
    int i;

    for (i = 0; i < pmu_counter_no; i++) {
        wrmsrl(IA32_PERFEVTSEL0 + i, 0ULL);
        wrmsrl(IA32_PMC0        + i, 0ULL);
    }
    if (hardcfg.model >= 0x0f)
        wrmsrl(IA32_FIXED_CTR_CTRL, 0ULL);
}

static void vf_p6_read(cpuevent_t* this)
{
}

static void vf_p6_freeze(cpuevent_t* this)
{
    int i;

    for (i = 0; i < pmu_counter_no; i++) {
        wrmsrl(IA32_PERFEVTSEL0 + i, 0ULL);
    }
    if (hardcfg.model >= 0x0f)
        wrmsrl(IA32_FIXED_CTR_CTRL, 0ULL);
}

/// with continuous counting mode
static void vf_p6_restart(cpuevent_t* this)
{
    long long interval = this->frozen_count;
    unsigned long long tmp;
    unsigned long long msk;

    if (!this->interval && !this->slave_interval)    /// no sampling
    {
        /// wrap the counters around
        this->frozen_count &= CPU_EVTCNT_THRESHOLD - 1;
        interval = -(interval & (CPU_EVTCNT_THRESHOLD - 1));
    } else {
        if (interval >= this->slave_interval)      /// overflowed
        {
            /// use the programmed interval
            this->frozen_count = interval = this->interval;
        } else                  /// underflowed
        {
            /// use the residual count
            this->frozen_count = interval = -interval;

            if (this->slave_interval)
            {
                this->frozen_count = -interval;
            }
        }
    }
    /// ensure we do not count backwards
    if (interval > this->interval) {
        interval = this->interval;
    }
    /// set up counters
    if (this->selmsr == IA32_FIXED_CTR_CTRL) {
        /// set up the counter
        wrmsrl(this->cntmsr, -interval & pmu_fixed_counter_width_mask);

        /// set up the control register 
        rdmsrl(IA32_FIXED_CTR_CTRL, tmp);

        msk = (((this->modifier & VTSS_EVMOD_ALL) >> 16) | 8) << (4 * ((event_modifier_t*)&this->modifier)->cnto);

        wrmsrl(IA32_FIXED_CTR_CTRL, tmp | msk);
    } else {
        /// set up the counter
        wrmsrl(this->cntmsr, -interval & pmu_counter_width_mask);
        /// set up the control register 
        /// TODO: use other modifier fields
        wrmsrl(this->selmsr, (this->selmsk & ~VTSS_EVMOD_ALL) | (this->modifier & VTSS_EVMOD_ALL));
    }
    if (this->extmsr) {
        wrmsrl(this->extmsr, this->extmsk);
    }
#if 0
    /// save timestamp to enable computation of event distribution function
    this->time[this->state_idx][0] = vtss_time_cpu();
    this->flux[this->state_idx] = 0;
#endif
}

static void vf_p6_freeze_read(cpuevent_t* this)
{
#if 0
    int state_idx = this->state_idx;
#endif
    long long interval = (this->frozen_count > 0) ? this->frozen_count : (long long)this->interval;
    int shift = 64 - pmu_counter_width;
    int fixed_shift = 64 - pmu_fixed_counter_width;
    unsigned long long mask = (((1ULL << pmu_fixed_counter_no) - 1) << 32) | ((1ULL << pmu_counter_no) - 1);
    unsigned long long ovf;

    rdmsrl(IA32_PERF_GLOBAL_STATUS, ovf);
    ovf &= mask;

    wrmsrl(this->selmsr, 0ULL);
    rdmsrl(this->cntmsr, this->frozen_count);
    TRACE("MSR(0x%x)=>0x%llx, interval=0x%llx", this->cntmsr, this->frozen_count, (long long)this->interval);

    /// CPU BUG: Correction for broken fixed counters on some Meroms and Penryns
    if (hardcfg.family == 0x06) {
        if (hardcfg.model == 0x0f && hardcfg.stepping < 0x0b) {
            if (this->selmsr == IA32_FIXED_CTR_CTRL) {
                this->frozen_count = -interval;
            }
        } else if (hardcfg.model == 0x17) {
            if (this->cntmsr == 0x30b) {
                this->frozen_count = -interval;
            }
        }
    }
    /// convert the count to 64 bits
    if (this->selmsr == IA32_FIXED_CTR_CTRL) {
        this->frozen_count = (this->frozen_count << fixed_shift) >> fixed_shift;
    } else {
        this->frozen_count = (this->frozen_count << shift) >> shift;
    }

    /// ensure we do not count backwards
    if (this->frozen_count < -interval) {
        this->frozen_count = -this->interval;
        interval = (long long)this->interval;
    }
    if (!this->interval)    /// no sampling
    {
        if (this->frozen_count < interval)  /// HW and VM sanity check
        {
            interval = this->frozen_count;
        }
        this->sampled_count += this->frozen_count - interval;
#if 0
        /// save event count to enable computation of event distribution function
        this->flux[state_idx] += this->frozen_count - interval;
#endif
    } else {
        /// update the accrued count by adding the signed values of current count and sampling interval
        this->sampled_count += interval + this->frozen_count;
#if 0
        /// save event count to enable computation of event distribution function
        this->flux[state_idx] = interval + this->frozen_count;
#endif
    }
    /// separately preserve counts of overflowed counters, and
    /// uncomment to always save fixed counters (to show performance impact of synchronization on call tree)
    if ((this->frozen_count >= 0 && this->frozen_count >= this->slave_interval) || (this->selmsr == IA32_FIXED_CTR_CTRL && ovf)) {
        this->count = this->sampled_count;
    }
#if 0
    /// save timestamp & event count to enable computation of event distribution function
    this->time[state_idx][1] = vtss_time_cpu();
    /// toggle the state index
    this->state_idx = state_idx ^ 1;
#endif
}

static void vf_p6_restart_ro(cpuevent_t* this)
{
    rdmsrl(this->cntmsr, this->frozen_count);
}

static void vf_p6_freeze_read_ro(cpuevent_t* this)
{
    long long oldcnt = this->frozen_count;
    long long newcnt;

    rdmsrl(this->cntmsr, newcnt);

    if (newcnt < oldcnt) {
        this->count += CPU_EVTCNT_THRESHOLD;
    }
    this->count += newcnt - oldcnt;
}

static int vf_p6_overflowed(cpuevent_t* this)
{
    if (this->frozen_count >= 0) {
        return 1;               // always signal overflow for no sampling mode and in case of real overflow
    }
    return 0;
}

static long long vf_p6_convert(cpuevent_t* this)
{
    return 0;
}

static void vf_p6_overflow_update(cpuevent_t* this)
{
}

static void vf_p6_update_restart(cpuevent_t* this)
{
}

static int vf_p6_select_muxgroup(cpuevent_t* this)
{
    return -1;
}

static int vf_p6_select_muxgroup_ro(cpuevent_t* this)
{
    ///TODO: check if the PMC configuration matches the HW registers and return the mux_grp
    ///TODO: or simply return the control register value
    ///TODO: or use chain_idx field to search for and update an appropriate counter
    ///TODO: this functionality may be implemented in restart_ro/freeze_read_ro
    return -1;
}

/// P6 virtual function tables
static cpuevent_i vft_p6 = {
    vf_p6_start,
    vf_p6_stop,
    vf_p6_read,
    vf_p6_freeze,
    vf_p6_restart,
    vf_p6_freeze_read,
    vf_p6_overflowed,
    vf_p6_convert,
    vf_p6_overflow_update,
    vf_p6_update_restart,
    vf_p6_select_muxgroup
};
