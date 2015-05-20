/*
  Copyright (C) 2014 Intel Corporation.  All Rights Reserved.

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

/// KNX event control virtual functions
static void vf_knx_start(cpuevent_t* this)
{
}

static void vf_knx_stop(cpuevent_t* this)
{
    int i;

    for (i = 0; i < pmu_counter_no; i++) {
        wrmsrl(KNX_CORE_PERFEVTSEL0 + i, 0ULL);
        wrmsrl(KNX_CORE_PMC0        + i, 0ULL);
    }
}

static void vf_knx_read(cpuevent_t* this)
{
}

static void vf_knx_freeze(cpuevent_t* this)
{
    int i;

    for (i = 0; i < pmu_counter_no; i++) {
        wrmsrl(KNX_CORE_PERFEVTSEL0 + i, 0ULL);
    }
}

/// with continuous counting mode
static void vf_knx_restart(cpuevent_t* this)
{
    long long interval = this->frozen_count;
    unsigned long long tmp;
    unsigned long long msk;

    if (!this->interval)    /// no sampling
    {
        /// wrap the counters around
        this->frozen_count &= CPU_EVTCNT_THRESHOLD - 1;
        interval = -(interval & (CPU_EVTCNT_THRESHOLD - 1));
    } else {
        if (interval >= 0)      /// overflowed
        {
            /// use the programmed interval
            this->frozen_count = interval = this->interval;
        } else                  /// underflowed
        {
            /// use the residual count
            this->frozen_count = interval = -interval;
        }
    }
    /// ensure we do not count backwards
    if (interval > this->interval) {
        interval = this->interval;
    }
    /// set up the counter
    TRACE("MSR(0x%x)<=0x%llx", this->cntmsr, -interval & pmu_counter_width_mask);
    wrmsrl(this->cntmsr, -interval & pmu_counter_width_mask);
    /// set up the control register 
    /// TODO: use other modifier fields
    TRACE("MSR(0x%x)<=0x%x", this->selmsr, (this->selmsk & ~VTSS_EVMOD_ALL) | (this->modifier & VTSS_EVMOD_ALL));
    wrmsrl(this->selmsr, (this->selmsk & ~VTSS_EVMOD_ALL) | (this->modifier & VTSS_EVMOD_ALL));
    if (this->extmsr) {
        TRACE("MSR(0x%x)<=0x%llx", this->extmsr, this->extmsk);
        wrmsrl(this->extmsr, this->extmsk);
    }
}

static void vf_knx_freeze_read(cpuevent_t* this)
{
    long long interval = (this->frozen_count > 0) ? this->frozen_count : (long long)this->interval;
    int shift = 64 - pmu_counter_width;

    TRACE("MSR(0x%x)<=0x%llx", this->selmsr, 0LL);
    wrmsrl(this->selmsr, 0ULL);
    rdmsrl(this->cntmsr, this->frozen_count);
    TRACE("MSR(0x%x)=>0x%llx, interval=0x%llx", this->cntmsr, this->frozen_count, (long long)this->interval);

    /// convert the count to 64 bits
    this->frozen_count = (this->frozen_count << shift) >> shift;

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
    } else {
        /// update the accrued count by adding the signed values of current count and sampling interval
        this->sampled_count += interval + this->frozen_count;
    }
    /// separately preserve counts of overflowed counters, and
    /// uncomment to always save fixed counters (to show performance impact of synchronization on call tree)
    if (this->frozen_count >= 0) {
        this->count = this->sampled_count;
    }
}

static void vf_knx_restart_ro(cpuevent_t* this)
{
    rdmsrl(this->cntmsr, this->frozen_count);
}

static void vf_knx_freeze_read_ro(cpuevent_t* this)
{
    long long oldcnt = this->frozen_count;
    long long newcnt;

    rdmsrl(this->cntmsr, newcnt);

    if (newcnt < oldcnt) {
        this->count += CPU_EVTCNT_THRESHOLD;
    }
    this->count += newcnt - oldcnt;
}

static int vf_knx_overflowed(cpuevent_t* this)
{
    if (this->frozen_count >= 0) {
        return 1;               /// always signal overflow for no sampling mode and in case of real overflow
    }
    return 0;
}

static long long vf_knx_convert(cpuevent_t* this)
{
    return 0LL;
}

static void vf_knx_overflow_update(cpuevent_t* this)
{
}

static void vf_knx_update_restart(cpuevent_t* this)
{
}

static int vf_knx_select_muxgroup(cpuevent_t* this)
{
    return -1;
}

static int vf_knx_select_muxgroup_ro(cpuevent_t* this)
{
    return -1;
}

/// KNX virtual function tables
static cpuevent_i vft_knx = {
    vf_knx_start,
    vf_knx_stop,
    vf_knx_read,
    vf_knx_freeze,
    vf_knx_restart,
    vf_knx_freeze_read,
    vf_knx_overflowed,
    vf_knx_convert,
    vf_knx_overflow_update,
    vf_knx_update_restart,
    vf_knx_select_muxgroup
};
