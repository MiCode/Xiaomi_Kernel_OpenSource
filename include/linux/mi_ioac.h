#ifndef _LINUX_MI_IOAC_H
#define _LINUX_MI_IOAC_H

#ifdef CONFIG_TASK_XACCT
#define SAMPLE_INTERVAL (1000 * NSEC_PER_MSEC)
enum ioac_mode {
	RCHAR,
	WCHAR,
	CR,
	CW,
	CS,
};

static inline void add_last_ioac(struct task_struct *tsk, enum ioac_mode mode)
{
	u64 delta_time;
	u64 this_time;
	u64 delta_data;

	if (RCHAR == mode) {
		if (0 == tsk->ioac.last_ioac.rtime) {
			tsk->ioac.last_ioac.rtime = ktime_get_ns();
			tsk->ioac.last_ioac.rchar = tsk->ioac.rchar;
		} else {
			this_time = ktime_get_ns();
			delta_time = this_time - tsk->ioac.last_ioac.rtime;
			if (delta_time > SAMPLE_INTERVAL) {
				delta_data = tsk->ioac.rchar - tsk->ioac.last_ioac.rchar;
				tsk->ioac.last_ioac.rbw_char = (delta_data / (delta_time / NSEC_PER_MSEC));
				/*hook point*/
				tsk->ioac.last_ioac.rtime = this_time;
				tsk->ioac.last_ioac.rchar = tsk->ioac.rchar;
			}
		}
	} else if (WCHAR == mode) {
		if (0 == tsk->ioac.last_ioac.wtime) {
			tsk->ioac.last_ioac.wtime = ktime_get_ns();
			tsk->ioac.last_ioac.wchar = tsk->ioac.wchar;
		} else {
			this_time = ktime_get_ns();
			delta_time = this_time - tsk->ioac.last_ioac.wtime;
			if (delta_time > SAMPLE_INTERVAL) {
				delta_data = tsk->ioac.wchar - tsk->ioac.last_ioac.wchar;
				tsk->ioac.last_ioac.wbw_char = (delta_data / (delta_time / NSEC_PER_MSEC));
				/*hook point*/
				tsk->ioac.last_ioac.wtime = this_time;
				tsk->ioac.last_ioac.wchar = tsk->ioac.wchar;
			}
		}
	} else {
		#ifdef MI_IOAC_EXTENTED
		if (CR == mode) {
			if (0 == tsk->ioac.last_ioac.crtime) {
				tsk->ioac.last_ioac.crtime = ktime_get_ns();
				tsk->ioac.last_ioac.syscr = tsk->ioac.syscr;
			} else {
				this_time = ktime_get_ns();
				delta_time = this_time - tsk->ioac.last_ioac.crtime;
				if (delta_time > SAMPLE_INTERVAL) {
					delta_data = tsk->ioac.syscr - tsk->ioac.last_ioac.syscr;
					tsk->ioac.last_ioac.crtime = this_time;
					tsk->ioac.last_ioac.syscr = tsk->ioac.syscr;
				}
			}
		} else if (CW == mode) {
			if (0 == tsk->ioac.last_ioac.cwtime) {
				tsk->ioac.last_ioac.cwtime = ktime_get_ns();
				tsk->ioac.last_ioac.syscw = tsk->ioac.syscw;
			} else {
				this_time = ktime_get_ns();
				delta_time = this_time - tsk->ioac.last_ioac.cwtime;
				if (delta_time > SAMPLE_INTERVAL) {
					delta_data = tsk->ioac.syscw - tsk->ioac.last_ioac.syscw;
					tsk->ioac.last_ioac.cwtime = this_time;
					tsk->ioac.last_ioac.syscw = tsk->ioac.syscw;
				}
			}
		} else if (CS == mode) {
			if (0 == tsk->ioac.last_ioac.cstime) {
				tsk->ioac.last_ioac.cstime = ktime_get_ns();
				tsk->ioac.last_ioac.syscfs = tsk->ioac.syscfs;
			} else {
				this_time = ktime_get_ns();
				delta_time = this_time - tsk->ioac.last_ioac.cstime;
				if (delta_time > SAMPLE_INTERVAL) {
					delta_data = tsk->ioac.syscfs - tsk->ioac.last_ioac.syscfs;
					tsk->ioac.last_ioac.cstime = this_time;
					tsk->ioac.last_ioac.syscfs = tsk->ioac.syscfs;
				}
			}
		}
		#endif
	}
}
#endif

#endif
