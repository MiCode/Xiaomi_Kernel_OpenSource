#! /usr/bin/env python2
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2011-2017, 2018 The Linux Foundation. All rights reserved.

# -*- coding: utf-8 -*-

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
    "seqiv.c:76",
    "echainiv.c:50",
    "crypto.c:67",
    "shash.c:91",
    "shash.c:127",
    "smp.c:176",
    "xcbc.c:68",
    "hmac.c:56",
    "cbc.c:14",
    "cbc.h:116",
    "cryptd.c:487",
    "cryptd.c:515",
    "ablkcipher.c:215",
    "authenc.c:188",
    "authenc.c:186",
    "authencesn.c:186",
    "libcrc32c.c:45",
    "ppp_mppe.c:158",
    "ppp_mppe.c:369",
    "ppp_mppe.c:483",
    "dm-verity-fec.c:218",
    "umid.c:138",
    "umid.c:213",
    "umid.c:388",
    "shash.c:92",
    "shash.c:128",
    "smp.c:177",
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
