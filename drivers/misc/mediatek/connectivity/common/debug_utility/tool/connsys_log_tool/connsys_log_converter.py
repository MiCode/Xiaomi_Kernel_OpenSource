
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

import sys
import re
import time
import os
import datetime
import codecs
import struct

LogHeader = b'\x00\x00\x62'
InfoHeader = b'\x00\x25\x62'
TimeSyncLog = b'\x74\x69\x6D\x65'
LostLog = b'\x6C\x6F\x73\x74'
UTC_DIFF = 0
UtcTime = 0
systemTime = 0

# write time log
# ex: 01-01 08:34:46.372470 (   79.231659)
def write_time_title(fh_out, utc_time, system_time):
    utcTimeInt = int(utc_time)
    utcTimeFrac = utc_time - utcTimeInt
    systemTimeInt = int(system_time)
    systemTimeFrac = system_time - systemTimeInt
    fh_out.write(time.strftime("%m-%d %H:%M:%S", time.gmtime(utcTimeInt)) + ("%.6f " % utcTimeFrac)[1:] + "(%5d" % systemTimeInt + ("%.6f) " % systemTimeFrac)[1:])

# check time sync log
# update UTC_DIFF if utc log is chaged
def update_time_sync(fh, fh_out):
    global UTC_DIFF
    global UtcTime
    global systemTime
    fh.read(8) # skip 24 byte in header
    systemTime = struct.unpack("<I",fh.read(4))[0]
    systemTime /= 1.0
    systemTime /= 32768
    UtcTime = struct.unpack("<I",fh.read(4))[0]
    usec = struct.unpack("<I",fh.read(4))[0]
    usec /= 1.0
    usec /= 1000000
    UtcTime += usec
    tmpDiff = UtcTime - systemTime
    # write timesync only if UTC time changed
    if(tmpDiff != UTC_DIFF):
        UTC_DIFF = tmpDiff
        write_time_title(fh_out, UtcTime, systemTime);
        fh_out.write("timesync\n")

def show_lost_counter(fh, fh_out):
    fh_out.write("lost" + fh.read(20) + "\n")

def find_1st_timesync_from_here(pos, fh, fh_out):
    fh.seek(pos)
    while True:
        byte = fh.read(1)
        if byte == '':
            if UTC_DIFF == 0:
                fh_out.write("No found timesync log in this file.\n")
            break;
        if byte == b'\x55':
            bytes = fh.read(3)
            if bytes == InfoHeader:
                # skip 12 bytes header data
                fh.read(12)
                bytes = fh.read(4)
                if bytes == TimeSyncLog:
                    update_time_sync(fh, fh_out)
                    break

# parse all file in sub folder
def walk_all_file(folderName):
    # print "walk all file - ", folderName
    for path, subdirs, files in os.walk(folderName):
        for name in files:
            # print os.path.join(path, name)
            # print "convert_file: ", os.path.abspath(name)
            convert_file(os.path.join(path, name))

def convert_file(srcFile):
    global UTC_DIFF
    global UtcTime
    global systemTime

    name = os.path.basename(srcFile)
    # only support file name starts and ends with
    # WIFI_FW_2010_0101_001720.clog or WIFI_FW_2010_0101_001720.clog.curl
    # GPS_FW_2010_0101_003427.clog or GPS_FW_2010_0101_003427.clog.curl
    if(not((name.startswith('GPS_FW_') or name.startswith('WIFI_FW_') or name.startswith('MCU_FW_')) and (name.endswith('.clog') or name.endswith('.curf')))):
        return

    # create converted file name
    fileSize = os.path.getsize(srcFile)
    outputFile = os.path.abspath(srcFile) + ".txt"
    print "convert_file: ", outputFile
    try:
        fh = open(srcFile, "rb")
        fhout = open(outputFile, "w")
    except IOError as err:
        print err
        sys.exit()

    # find 1st time sync log
    find_1st_timesync_from_here(0, fh, fhout)

    # parse file
    fh.seek(0)
    while True:
        byte = fh.read(1)
        if byte == '':
            break;

        if byte == b'\x55':
            bytes = fh.read(3)
            # parse normal log
            if bytes == LogHeader:
                fh.read(4); # skip 4 byte REM NUM
                timeStamp = struct.unpack("<I",fh.read(4))[0]
                timeStamp /= 1.0
                timeStamp /= 32768
                utc_time = UTC_DIFF + timeStamp
                write_time_title(fhout, utc_time, timeStamp)
                fh.read(2); # skip 2 byte TASK ID
                bufLen = struct.unpack("<h",fh.read(2))[0]
                log_buf = fh.read(bufLen)
                fhout.write(log_buf)
                # if log end has no \n, append \n
                if len(log_buf) != 0 and log_buf[-1] != b'\x0a':
                    fhout.write('\n')
                continue

            # Parse time sync log
            elif bytes == InfoHeader:
                # skip 12 bytes header data
                fh.read(12)
                bytes = fh.read(4)
                if bytes == TimeSyncLog:
                    update_time_sync(fh, fhout)
                elif bytes == LostLog:
                    show_lost_counter(fh, fhout)
                    # since lost some log, to find last time sync
                    find_1st_timesync_from_here(fh.tell(), fh, fhout)
                    fh.seek(fh.tell())
                continue

    try:
        fh.close()
        fhout.close()
    except IOError as err:
        print err
        sys.exit()

if __name__ == '__main__':
    inputArg = os.path.abspath(sys.argv[1])
    if os.path.exists(inputArg):
        if os.path.isdir(inputArg):
            walk_all_file(inputArg)
        else:
            convert_file(inputArg)
        sys.exit()
    else:
        print "file/dir not exist"
