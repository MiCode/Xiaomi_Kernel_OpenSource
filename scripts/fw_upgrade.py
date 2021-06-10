#!/usr/bin/env python3
# Author：songmuchun <smcdef@163.com>
import os
import time
import datetime
import platform
import sys
import threading
import subprocess

DEVPATH  = "/sys/class/touchscreen/"
PROCPATH = "/proc/touchscreen/"

# if Linux, print log with color, else none
def print_log(msg, end = False):
    system = platform.system()
    if system == "Linux":
        if end == False:
            print("\033[1;33m" + msg + "\033[0m")
        else:
            print("\033[1;33m" + msg + "\033[0m", end = "", flush = True)
    else:
        if end == False:
            print(msg)
        else:
            print(msg, end = "", flush = True)

# print progress bar and total time of upgrade firmware
def show_progress_bar():
    print("\n",  end = "")
    print_log("Firmware Upgrading...", True)
    i = 0
    time_start = datetime.datetime.now()
    shell_cmd = "adb shell \"cat " + DEVPATH + "flashprog\""
    while True:
        status,out = subprocess.getstatusoutput(shell_cmd)
        if out == "1":
            if i == 0:
                print("\n    ", end = "", flush = True)
            print_log("*", True)
            i += 1
            if i == 50:
                i = 0
            time.sleep(0.001)
        else:
            time_end = datetime.datetime.now()
            print("\n",  end = "")
            print_log("Total Time: " + str((time_end - time_start).seconds) + " seconds\n")
            break;

def cat_file(node):
    shell_cmd = shell_cmd = "adb shell \"cat " + DEVPATH + node + "\""
    status,out = subprocess.getstatusoutput(shell_cmd)
    return out

# adb shell ls /sys/class/touchscreen
# adb shell cat /sys/class/touchscreen/touchpanel/path
def set_dev_path():
    global DEVPATH
    shell_cmd = "adb shell \"ls " + DEVPATH + "\""
    status,out = subprocess.getstatusoutput(shell_cmd)
    shell_cmd = "adb shell \"cat " + DEVPATH + out + "/path\""
    status,out = subprocess.getstatusoutput(shell_cmd)
    DEVPATH = "/sys" + out + "/"

# adb wait-for-device
# adb root
# adb wait-for-device
# adb remount
def wait_device():
    shell_cmd = "adb wait-for-device;" + \
        "adb root;" + "adb wait-for-device;" + "adb remount;"
    subprocess.getstatusoutput(shell_cmd)

# adb shell cat DEVPATH/poweron
def power_on(is_up = True):
    # just for keeping screen lit
    out = cat_file("poweron")
    if out == "0":
        os.system("adb shell input keyevent 26")
    if is_up:
        os.system("adb shell input keyevent 82")

# adb shell cat DEVPATH/flashprog
def is_fw_upgrading():
    out = cat_file("flashprog")
    if out == "1":
        return True
    else:
        return False

# adb shell "mount -o remount,rw /"
# adb shell "mkdir /lib/firmware -p"
# adb push file /lib/firmware
def push_firmware(file):
    shell_cmd = "adb shell mount -o remount,rw /"
    status,out = subprocess.getstatusoutput(shell_cmd)
    if status == 0:
        shell_cmd = "adb shell mkdir /lib/firmware -p"
        status,out = subprocess.getstatusoutput(shell_cmd)
        if status == 0:
            shell_cmd = "adb push " + file + " /lib/firmware/"
            os.system(shell_cmd)
            return

    shell_cmd = "adb shell \"echo -n /data > /sys/module/firmware_class/parameters/path\""
    os.system(shell_cmd)
    shell_cmd = "adb push " + file + " /data"
    os.system(shell_cmd)

def change_open_short_file(file):
    basename = os.path.basename(file)
    shell_cmd = "adb shell \"echo " + basename + " > " + DEVPATH + "ini_file_name" + "\""
    os.system(shell_cmd)

# @is_force
#     True:  adb shell "echo file > DEVPATH/forcereflash"
#     False: adb shell "echo file > DEVPATH/doreflash"
def upgrade_firmware(file, is_force = False):
    push_firmware(file)
    print_log("Before upgrade version: " + get_version())
    # get base name, eg: /workspace/fw.bin ==> fw.bin
    fw = os.path.basename(file)
    is_upgrade = is_fw_upgrading()
    if is_upgrade:
        print_log("Is upgrading firmware, please wait...")
    while is_fw_upgrading():
        pass
    power_on()
    node = "forcereflash" if (is_force) else "doreflash"
    shell_cmd = "adb shell \"echo " + fw + \
        " > " + DEVPATH + node + "\""
    upgrade_fw_thread = threading.Thread(target = show_progress_bar)
    upgrade_fw_thread.start()
    status,out = subprocess.getstatusoutput(shell_cmd)
    upgrade_fw_thread.join()
    print_log("After upgrade version: " + get_version())
    if status != 0:
        print_log(out)
        print_log("Upgrade firmware fail! You can keep the screen lit and retry.")
        sys.exit(0)

# adb shell "cat DEVPATH/buildid"
def get_version():
    power_on(is_up = False)
    return cat_file("buildid")

# adb shell "cat DEVPATH/vendor"
def get_vendor():
    return cat_file("vendor")

# adb shell "cat DEVPATH/productinfo"
def get_productinfo():
    return cat_file("productinfo")

def get_all_info():
    vendor = get_vendor()
    productinfo = get_productinfo()
    version = get_version()
    return vendor + "-" + productinfo + "-" + version

# open-short test
def open_short_test(times = 1):
    i = 0
    shell_cmd = "adb shell \"cat " + PROCPATH + "ctp_openshort_test\""

    pass_count = 0
    fail_count = 0
    while i < times:
        i += 1
        # keep screen lit
        power_on()
        status,out = subprocess.getstatusoutput(shell_cmd)
        if status == 0:
            if out == "result=1":
                print("open-short test pass")
                pass_count += 1
            elif out == "result=0":
                print("open-short test fail: " + out)
                fail_count += 1
        else:
            print_log(out)
            break;
    if times > 1:
        print("\n", end='')
        print_log("Total times: " + str(times))
        print_log("Pass  times: " + str(pass_count))
        print_log("Fail  times: " + str(fail_count))

# print help message
def print_help():
    print_log(
        "Usage: fw_upgrade [file] [option]\n" + \
        " upgrade firmware with compare the version of firmware\n" + \
        "  -f, --force            Force upgrade firmware\n" + \
        "  -v, --version          Get version of the firmware\n" + \
        "  -t, --open-short-test  Open-short test\n" + \
        "  -a, --all              Display all information about touchpanel\n" + \
        "  -c, --cover            objcopy -I binary -O ihex <file> <file.ihex>\n" + \
        "  -r, --reverse          objcopy -I ihex -O binary <file.ihex> <file>\n" + \
        "  -h, --help             Display this information"
        )

# start here
def main():
    if len(sys.argv) == 1:
        print_help()
        sys.exit(0)
    else:
        file = sys.argv[1]
        if file == "-h":
            print_help()
            sys.exit(0)
        if len(sys.argv) == 3 and file != "-t":
            if os.path.exists(file) == False:
                print_log(file + "is not exist")
                sys.exit(0)
            if sys.argv[2] == "-c":
                os.system("objcopy -I binary -O ihex " + file + " " + file + ".ihex")
                sys.exit(0)
            if sys.argv[2] == "-r":
                fw = file.split(".ihex")[0]
                os.system("objcopy -I ihex -O binary " + file + " " + fw)
                sys.exit(0)
        wait_device()
        set_dev_path()
        if file == "-a":
            print_log(get_all_info())
            sys.exit(0)
        if file == "-v":
            print_log(get_version())
            sys.exit(0)
        if file == "-t":
            if len(sys.argv) == 3:
                open_short_test(times = int(sys.argv[2]))
            else:
                open_short_test()
            sys.exit(0)
        if os.path.exists(file) == False:
            print_log(file + "is not exist")
            sys.exit(0)
        if len(sys.argv) == 2:
            upgrade_firmware(file)
        elif len(sys.argv) == 3:
            if sys.argv[2] == "-f":
                upgrade_firmware(file, is_force = True)
            elif sys.argv[2] == "-t":
                if cat_file("ini_file_name") != os.path.basename(file):
                    push_firmware(file)
                change_open_short_file(file)
                open_short_test()

main()
