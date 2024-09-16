
#
# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

'''
Created on 2012/11/14

Convert kernel time stamp in each line of a kernel log,
prepend the converted local time in the beginning of each line.
The time stamp reference is from the log of the following pattern:

...[<kernel time>] ... android time <local time>

such as:
<4>[12175.130847] (0)[62:wdtk-0][cpu-0:thread:62] 2012-08-02 03:41:18.94083 UTC; android time 2012-08-02 11:41:18.94083
<5>[25676.236787] (0)[313:InputReader][request_suspend_state]: wakeup (0->0) at 25665623103493 (2012-11-19 08:03:43.654473155 UTC)
<6>[  344.427890] (0)[41:binder_watchdog]binder: 54296 exec 1354:1380 to 110:333 over 4.011 sec () dex_code 4 start_at 581.260 2012-01-02 00:06:18.728
<6>[  160.699284] (0)[44:binder_watchdog]binder: 15889 read 563:1223 to 1076:0 over 2.257 sec () dex_code 44 start_at 158.372 android 2013-04-26 21:57:28.624
<6>[26064.469194].(4)[6320:kworker/u:1]PM: suspend exit 2013-10-12 06:18:48.551577654 UTC
[    5.219499] .(2)[1:swapper/0]mt-rtc mt-rtc: setting system clock to 2018-01-25 01:24:36 UTC (1516843476)
[   67.101501] .(1)[249:wdtk-1][name:wd_common_drv&][thread:249][RT:67101493390] 2018-01-25 01:25:38.382875 UTC;android time 2018-01-25 01:25:38.382875

'''

import sys
import re
import time
import os
import datetime
from bisect import bisect_right

def kernel_to_utc(klog, llog):
    # if llog == '-', output to stdout
    line = 0
    line_timestamp = []
    diff_timestamp = []
    debug_enable = False
    _stdout = True if llog == '-' else False

    # first pass
    try:
        fh = open(klog, "rb")
        if _stdout:
            fhout = sys.stdout
        else:
            fhout = open(llog, "w")
    except IOError as err:
        print err
        sys.exit()

    _re_utc_time = re.compile(r"[^\[]*?\[[ ]*(\d+?\.\d+)\].*?(\d+-\d+-\d+ \d+:\d+:\d+)(\.\d+) UTC")

    for linebuf in fh:
        line = line + 1

        if "UTC" not in linebuf:
           continue

        m = _re_utc_time.match(linebuf)
        if m:
            (ktime, atime, usec) = m.group(1,2,3)
            time_sec_int = time.mktime(time.strptime(atime, "%Y-%m-%d %H:%M:%S"))
            time_sec = time_sec_int + float(usec) - float(ktime)

            if (debug_enable):
                print "*"
                print linebuf
                print ktime, atime, usec

            line_timestamp.append(line)
            diff_timestamp.append(time_sec)


    # second pass
    fh.seek(0, os.SEEK_SET)

    if debug_enable:
        print len(line_timestamp),",",len(diff_timestamp)
        print line_timestamp
        print diff_timestamp

    line_ts_ref = -1
    diff_ts_ref = 0

    # in case no timestamp exist
    if line_timestamp:
        line_ts_ref = line_timestamp[0]
        diff_ts_ref = diff_timestamp[0]

    # _re_ktime = re.compile(r"^[ ]*?<[^>]+>\[[ ]*?(\d+?\.\d+?)\]")
    _re_ktime = re.compile(r"^[^\[]*?\[[ ]*?(\d+?\.\d+?)\]")

    line = 0
    for linebuf in fh:
        diff_ts = -1
        line = line + 1

        m = _re_ktime.match(linebuf)
        if m:
            diff_ts = float(m.group(1))

        if line == line_ts_ref:
            line_timestamp.pop(0)
            line_ts_ref = line_timestamp[0] if line_timestamp else sys.maxint
            diff_ts_ref = diff_timestamp.pop(0)

            if debug_enable: print "diff_ts_ref: ", diff_ts_ref

        if diff_ts != -1:
            if line_ts_ref != -1:
                android_time = diff_ts + diff_ts_ref
                android_time_int = int(android_time)
                android_time_frac = android_time - android_time_int

                ts = time.localtime(android_time_int)

                fhout.write(time.strftime("%m-%d %H:%M:%S", ts) + ("%.6f " % android_time_frac)[1:])
            else:
                fhout.write("??-?? ??:??:??.?????? ")

        fhout.write(linebuf)

        # for debug
        # print linebuf,

    try:
        fhout.close()
        if not _stdout:
            fh.close()
    except IOError as err:
        print err
        sys.exit()


if __name__ == "__main__":
    if (len(sys.argv) not in [2, 3]):
        print "Usage: ", sys.argv[0], " <kernel log> [-]"
        sys.exit()

    _stdout = False
    if len(sys.argv) == 3 and sys.argv[2] == '-':
        _stdout = True

    if _stdout:
        kernel_to_utc(sys.argv[1],'-')
    else:
        kernel_to_utc(sys.argv[1],sys.argv[1]+".utc")
    sys.exit()
