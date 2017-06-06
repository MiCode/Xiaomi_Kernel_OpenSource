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
    "core.c:144",
    "inet_connection_sock.c:430",
    "inet_connection_sock.c:467",
    "inet6_connection_sock.c:89",
    "cpu.c:633",
    "ftrace.c:140",
    "page_alloc.c:524",
    "page-writeback.c:2436",
    "page_alloc.c:1323",
    "core.c:928",
    "statfs.c:318",
    "statfs.c:319",
    "page-writeback.c:2458",
    "osq_lock.c:24",
    "wakelock.c:80",
    "statfs.c:320",
    "page-writeback.c:2715",
    "hooks.c:403",
    "timer.c:699",
    "ring_buffer.c:1804",
    "topology.c:26",
    "statfs.c:321",
    "page-writeback.c:2762",
    "hrtimer.c:408",
    "statfs.c:322",
    "page-writeback.c:2817",
    "rng.c:36",
    "vmscan.c:2058",
    "keystore.c:1166",
    "cfq-iosched.c:455",
    "keystore.c:1185",
    "vmscan.c:2745",
    "vmscan.c:2970",
    "cfq-iosched.c:594",
    "cfq-iosched.c:595",
    "zram_drv.c:330",
    "regcache-rbtree.c:129",
    "regcache-rbtree.c:130",
    "cfq-iosched.c:591",
    "workingset.c:292",
    "workingset.c:296",
    "workingset.c:478",
    "cfq-iosched.c:985",
    "regcache-rbtree.c:242",
    "control_compat.c:111",
    "rmap.c:1161",
    "rmap.c:1201",
    "regcache-rbtree.c:243",
    "control_compat.c:128",
    "slub.c:1316",
    "br_if.c:436",
    "net_namespace.c:293",
    "psci.c:95",
    "slub.c:1945",
    "psci.c:104",
    "control_compat.c:133",
    "dmi_scan.c:396",
    "uhid.c:240",
    "control_compat.c:134",
    "slub.c:1950",
    "net_namespace.c:307",
    "br_if.c:450",
    "uhid.c:284",
    "secureboot.c:19",
    "control_compat.c:135",
    "net_namespace.c:311",
    "secureboot.c:22",
    "zsmalloc.c:756",
    "zsmalloc.c:784",
    "control_compat.c:403",
    "v4l2-compat-ioctl32.c:719",
    "media-entity.c:25",
    "zsmalloc.c:472",
    "control_compat.c:404",
    "timer_compat.c:53",
    "v4l2-compat-ioctl32.c:724",
    "bond_main.c:1031",
    "zsmalloc.c:483",
    "control_compat.c:409",
    "timer_compat.c:54",
    "v4l2-compat-ioctl32.c:727",
    "main.c:764",
    "control_compat.c:410",
    "net_namespace.c:482",
    "main.c:765",
    "v4l2-compat-ioctl32.c:764",
    "control_compat.c:411",
    "net_namespace.c:486",
    "main.c:766",
    "inffast.c:31",
    "timer_compat.c:55",
    "main.c:767",
    "rtnetlink.c:306",
    "exthdrs.c:207",
    "devinet.c:1256",
    "pcm_compat.c:227",
    "rtnetlink.c:477",
    "main.c:768",
    "nf_nat_h323.c:553",
    "pcm_compat.c:235",
    "main.c:789",
    "exthdrs.c:209",
    "pcm_compat.c:236",
    "exthdrs.c:214",
    "main.c:790",
    "pcm_compat.c:237",
    "main.c:798",
    "nfnetlink_queue.c:466",
    "mip6.c:234",
    "pcm_compat.c:238",
    "mip6.c:256",
    "main.c:800",
    "nf_conntrack_netlink.c:2550",
    "pcm_compat.c:239",
    "main.c:801",
    "nf_conntrack_netlink.c:2787",
    "rawmidi_compat.c:41",
    "pcm_compat.c:240",
    "main.c:802",
    "nf_conntrack_netlink.c:2844",
    "cls_api.c:48",
    "pcm_compat.c:241",
    "cls_api.c:736",
    "nf_conntrack_netlink.c:2847",
    "pcie_bus.c:359",
    "rawmidi_compat.c:42",
    "pcm_compat.c:242",
    "nf_conntrack_netlink.c:2914",
    "txrx.c:64",
    "pcm_compat.c:243",
    "nf_conntrack_netlink.c:3031",
    "radiotap.c:119",
    "devio.c:306",
    "pcm_compat.c:244",
    "nf_conntrack_netlink.c:3122",
    "devio.c:307",
    "pcm_compat.c:245",
    "nf_conntrack_netlink.c:3126",
    "devio.c:308",
    "composite.c:626",
    "pcm_compat.c:246",
    "nf_conntrack_netlink.c:3130",
    "composite.c:638",
    "devio.c:309",
    "pcm_compat.c:247",
    "nf_conntrack_netlink.c:3203",
    "composite.c:671",
    "devio.c:1494",
    "pcm_compat.c:248",
    "nf_nat_core.c:232",
    "devio.c:1499",
    "core.c:143",
    "pcm_compat.c:502",
    "devio.c:1502",
    "x_tables.c:326",
    "pcm_compat.c:503",
    "x_tables.c:346",
    "devio.c:1515",
    "pcm_compat.c:504",
    "devio.c:1516",
    "pcm_compat.c:532",
    "devio.c:1517",
    "rawmidi_compat.c:43",
    "pcm_compat.c:533",
    "pcm_compat.c:534",
    "rawmidi_compat.c:44",
    "pcm_compat.c:535",
    "pcm_compat.c:537",
    "pcm_compat.c:538",
    "pcm_compat.c:539",
    "rawmidi_compat.c:72",
    "rawmidi_compat.c:88",
    "rawmidi_compat.c:89",
    "rawmidi_compat.c:90",
    "user-offsets.c:18",
    "ubd_kern.c:867",
    "time.c:21",
    "process.c:156",
    "process.c:579",
    "task_work.c:103",
    "slab.c:384",
    "process.c:306",
    "slab.c:653",
    "hrtimer.c:1372",
    "ntp.c:243",
    "cputime.c:258",
    "rt.c:1060",
    "fair.c:2592",
    "slab.c:659",
    "fair.c:2596",
    "select.c:599",
    "fair.c:3474",
    "eventpoll.c:1626",
    "rt.c:1062",
    "fair.c:4683",
    "eventpoll.c:1627",
    "fair.c:4704",
    "ethtool.c:2528",
    "dev.c:3928",
    "fair.c:4721",
    "dev.c:4064",
    "fair.c:4722",
    "slub.c:279",
    "slub.c:1303",
    "slub.c:1328",
    "media-entity.c:41",
    "net1080.c:381",
    "net1080.c:382",
    "net1080.c:420",
    "net1080.c:487",
    "net1080.c:271",
    "range.h:25",
    "kdev_t.h:64",
    "mmu.c:623",
    "sys.c:2193",
    "shmem.c:1769",
    "core.c:6813",
    "shmem.c:4145",
    "fcntl.c:548",
    "fcntl.c:550",
    "percpu.c:2310",
    "libfs.c:1043",
    "ioctl.c:571",
    "random.c:61",
    "i2c-core.c:2958",
    "compat_ioctl.c:733",
    "irq-gic-v3-its.c:1279",
    "super.c:788",
    "mballoc.c:692",
    "dm-ioctl.c:942",
    "super.c:2608",
    "dm-bufio.c:1876",
    "compat_ioctl.c:1532",
    "ndisc.c:1328",
    "binfmt_elf.c:2226",
    "mc.c:80",
    "xfrm6_tunnel.c:143",
    "mmc.c:80",
    "fdt.c:1178",
    "mmc.c:81",
    "act_api.c:456",
    "fdt_strerror.c:94",
    "mmc.c:82",
    "mmc.c:83",
    "mmc.c:84",
    "fdt.c:1184",
    "mmc.c:85",
    "sd.c:83",
    "mmc.c:86",
    "sd.c:84",
    "sd.c:85",
    "mmc.c:87",
    "mmc.c:88",
    "sd.c:86",
    "mmc.c:89",
    "mmc.c:90",
    "sd.c:87",
    "mmc.c:91",
    "sd.c:88",
    "mmc.c:92",
    "sd.c:89",
    "mmc.c:98",
    "mmc.c:99",
    "sd.c:90",
    "mmc.c:100",
    "mmc.c:101",
    "sd.c:91",
    "mmc.c:102",
    "sd.c:93",
    "mmc.c:103",
    "sd.c:94",
    "mmc.c:104",
    "sd.c:108",
    "mmc.c:105",
    "sd.c:112",
    "mmc.c:106",
    "sd.c:113",
    "mmc.c:108",
    "mmc.c:109",
    "sd.c:115",
    "mmc.c:145",
    "sd.c:117",
    "mmc.c:152",
    "sd.c:118",
    "mmc.c:153",
    "sd.c:120",
    "mmc.c:154",
    "sd.c:122",
    "sd.c:123",
    "mmc.c:156",
    "helpers.c:125",
    "sd.c:734",
    "mmc.c:158",
    "mmc.c:159",
    "mmc.c:161",
    "mmc.c:163",
    "sd.c:126",
    "mmc.c:164",
    "sd.c:127",
    "mmc.c:167",
    "sd.c:128",
    "mmc.c:168",
    "sd.c:129",
    "helpers.c:145",
    "mmc.c:169",
    "mmc.c:170",
    "mmc.c:171",
    "sd.c:130",
    "sd.c:733",
    "mmc.c:172",
    "sd.c:131",
    "mmc.c:173",
    "sd.c:132",
    "mmc.c:174",
    "mmc.c:177",
    "sd.c:133",
    "sd.c:135",
    "mmc.c:178",
    "sd.c:767",
    "spmi-pmic-arb.c:382",
    "sd.c:138",
    "sd.c:768",
    "sysrq.c:146",
    "sd.c:154",
    "sd.c:797",
    "sd.c:155",
    "sd.c:157",
    "sd.c:158",
    "sd.c:164",
    "sd.c:798",
    "sd.c:199",
    "sd.c:206",
    "sd.c:207",
    "sd.c:210",
    "sd.c:212",
    "sd.c:218",
    "sd.c:257",
    "sd.c:261",
    "sd.c:262",
    "sd.c:264",
    "sd.c:828",
    "f_hid.c:913",
    "sd.c:829",
    "f_hid.c:914",
    "sd.c:930",
    "f_hid.c:915",
    "sd.h:187",
    "sd.h:182",
    "sd.h:172",
    "page_alloc.c:7248",
    "atomic.h:156",
    "atomic.h:176",
    "atomic.h:181",
    "atomic.h:197",
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
