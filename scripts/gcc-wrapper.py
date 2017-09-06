#! /usr/bin/env python2
# -*- coding: utf-8 -*-

# Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of The Linux Foundation nor
#       the names of its contributors may be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Invoke gcc, looking for warnings, and causing a failure if there are
# non-whitelisted warnings.

import errno
import re
import os
import sys
import subprocess

# Note that gcc uses unicode, which may depend on the locale.  TODO:
# force LANG to be set to en_US.UTF-8 to get consistent warnings.

allowed_warnings = set([
    "range.h:25",
    "kdev_t.h:64",
    "core.c:144",
    "fdt_strerror.c:94",
    "mmu.c:628",
    "page_alloc.c:4462",
    "sys.c:2436",
    "swap.c:594",
    "vmscan.c:1297",
    "util.c:636",
    "percpu.c:2319",
    "libfs.c:1047",
    "page_alloc.c:4461",
    "vmscan.c:1327",
    "rt.c:2687",
    "zsmalloc.c:753",
    "super.c:788",
    "mballoc.c:690",
    "ioctl.c:571",
    "meminfo.c:109",
    "compat_ioctl.c:390",
    "compat_ioctl.c:1525",
    "random.c:61",
    "i2c-core-base.c:2042",
    "ndisc.c:1328",
    "irq-gic-v3-its.c:1281",
    "dm-ioctl.c:954",
    "dm-bufio.c:1877",
    "mmc.c:81",
    "sd.c:83",
    "xfrm6_tunnel.c:143",
    "pcie_bus.c:383",
    "nf_nat_h323.c:553",
    "fdt.c:1208",
    "act_api.c:483",
    "helpers.c:125",
    "sd.c:742",
    "sysrq.c:146",
    "f_hid.c:913",
    "pcie_bus.c:383",
    "page_alloc.c:4605",
    "meminfo.c:110",
    "vmscan.c:2102",
    "zsmalloc.c:781",
    "mmc.c:82",
    "sd.c:84",
    "act_api.c:495",
    "fdt.c:1202",
    "helpers.c:145",
    "sd.c:741",
    "f_hid.c:914",
    "meminfo.c:113",
    "page_alloc.c:4606",
    "vmscan.c:2798",
    "mmc.c:83",
    "sd.c:85",
    "act_api.c:689",
    "sd.c:775",
    "f_hid.c:915",
    "meminfo.c:115",
    "mmc.c:84",
    "sd.c:86",
    "sd.c:776",
    "mmc.c:85",
    "sd.c:87",
    "sd.c:805",
    "mmc.c:86",
    "sd.c:88",
    "sd.c:806",
    "mmc.c:87",
    "sd.c:89",
    "sd.c:836",
    "mmc.c:88",
    "sd.c:90",
    "sd.c:837",
    "mmc.c:89",
    "sd.c:91",
    "sd.c:938",
    "mmc.c:90",
    "sd.c:93",
    "sd.h:189",
    "mmc.c:91",
    "sd.c:94",
    "sd.h:184",
    "mmc.c:92",
    "sd.c:108",
    "sd.h:174",
    "mmc.c:93",
    "sd.c:112",
    "mmc.c:99",
    "sd.c:113",
    "mmc.c:100",
    "sd.c:115",
    "mmc.c:101",
    "sd.c:117",
    "mmc.c:102",
    "sd.c:118",
    "mmc.c:103",
    "sd.c:120",
    "mmc.c:104",
    "sd.c:122",
    "mmc.c:105",
    "sd.c:123",
    "mmc.c:106",
    "sd.c:126",
    "mmc.c:107",
    "sd.c:127",
    "mmc.c:109",
    "sd.c:128",
    "mmc.c:110",
    "sd.c:129",
    "mmc.c:146",
    "sd.c:130",
    "mmc.c:153",
    "sd.c:131",
    "mmc.c:154",
    "sd.c:132",
    "mmc.c:155",
    "sd.c:133",
    "mmc.c:157",
    "sd.c:135",
    "mmc.c:159",
    "sd.c:138",
    "mmc.c:160",
    "sd.c:154",
    "mmc.c:162",
    "sd.c:155",
    "mmc.c:164",
    "sd.c:157",
    "mmc.c:165",
    "sd.c:158",
    "mmc.c:168",
    "sd.c:164",
    "mmc.c:169",
    "sd.c:199",
    "mmc.c:170",
    "sd.c:206",
    "mmc.c:171",
    "sd.c:207",
    "mmc.c:172",
    "sd.c:210",
    "mmc.c:173",
    "sd.c:212",
    "mmc.c:174",
    "sd.c:218",
    "sd.c:257",
    "mmc.c:175",
    "sd.c:261",
    "mmc.c:178",
    "mmc.c:179",
    "sd.c:262",
    "sd.c:264",
    "process.c:614",
    "shmem.c:1773",
    "super.c:2628",
    "binfmt_elf.c:2266",
    "shmem.c:4156",
    "i2c-core-base.c:2043",
    "page_alloc.c:7373",
    "cls_api.c:333",
 ])

# Capture the name of the object file, can find it.
ofile = None

warning_re = re.compile(r'''(.*/|)([^/]+\.[a-z]+:\d+):(\d+:)? warning:''')
def interpret_warning(line):
    """Decode the message from gcc.  The messages we care about have a filename, and a warning"""
    line = line.rstrip('\n')
    m = warning_re.match(line)
    if m and m.group(2) not in allowed_warnings:
        print "error, forbidden warning:", m.group(2)

        # If there is a warning, remove any object if it exists.
        if ofile:
            try:
                os.remove(ofile)
            except OSError:
                pass
        sys.exit(1)

def run_gcc():
    args = sys.argv[1:]
    # Look for -o
    try:
        i = args.index('-o')
        global ofile
        ofile = args[i+1]
    except (ValueError, IndexError):
        pass

    compiler = sys.argv[0]

    try:
        proc = subprocess.Popen(args, stderr=subprocess.PIPE)
        for line in proc.stderr:
            print line,
            interpret_warning(line)

        result = proc.wait()
    except OSError as e:
        result = e.errno
        if result == errno.ENOENT:
            print args[0] + ':',e.strerror
            print 'Is your PATH set correctly?'
        else:
            print ' '.join(args), str(e)

    return result

if __name__ == '__main__':
    status = run_gcc()
    sys.exit(status)
