#! /usr/bin/env python
# -*- coding: utf-8 -*-

# Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Code Aurora nor
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
    "alignment.c:298",
    "alignment.c:720",
    "async.c:122",
    "async.c:270",
    "block.c:885",
    "block.c:886",
    "dir.c:43",
    "dm.c:1053",
    "dm.c:1080",
    "dm-table.c:1120",
    "dm-table.c:1126",
    "drm_edid.c:1303",
    "eventpoll.c:1143",
    "f_mass_storage.c:3368",
    "inode.c:72",
    "inode.c:73",
    "inode.c:74",
    "msm_sdcc.c:126",
    "msm_sdcc.c:128",
    "nf_conntrack_core.c:1579",
    "nf_conntrack_core.c:1580",
    "nf_conntrack_netlink.c:790",
    "nf_conntrack_proto.c:210",
    "nf_conntrack_proto.c:345",
    "nf_conntrack_proto.c:370",
    "nf_nat_core.c:528",
    "nf_nat_core.c:739",
    "nf_nat_core.c:740",
    "nf_nat_core.c:741",
    "nf_nat_core.c:742",
    "nf_nat_core.c:751",
    "nf_nat_core.c:753",
    "nf_nat_core.c:756",
    "nf_nat_ftp.c:123",
    "nf_nat_pptp.c:285",
    "nf_nat_pptp.c:288",
    "nf_nat_pptp.c:291",
    "nf_nat_pptp.c:294",
    "nf_nat_sip.c:550",
    "nf_nat_sip.c:551",
    "nf_nat_sip.c:552",
    "nf_nat_sip.c:553",
    "nf_nat_sip.c:555",
    "nf_nat_sip.c:556",
    "nf_nat_sip.c:554",
    "nf_nat_standalone.c:118",
    "nf_nat_tftp.c:46",
    "return_address.c:62",
    "sch_generic.c:678",
    "soc-core.c:1719",
    "xt_log.h:50",
    "vx6953.c:3124",
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
