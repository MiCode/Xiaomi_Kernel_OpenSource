#!/usr/bin/python

import os
import sys
import commands
import argparse

clang_version = commands.getoutput("cat ./build.config.constants |grep 'CLANG_VERSION' |awk -F'=' '{print $(NF)}'")
clang_path = os.path.abspath(".././prebuilts/clang/host/linux-x86/" + "clang-" + clang_version + "/bin")
gki_defconfig_path = "arch/arm64/configs/gki_defconfig"
fragment_gki_path = "arch/arm64/configs/sprd_gki.fragment"
fragment_defconfig = "arch/arm64/configs/fragment_gki_defconfig"
tmp_path = "./tmp_fregment_check/"
gki_config_info = {}
fragment_gki_config_info = {}
fragment_soc_config_info = {}
fragment_soc_without_m_config_info = {}
ret = 0
check_flag = 0

config_whitelist=["CONFIG_ARCH_SPRD","CONFIG_WCN_BOOT","CONFIG_SC23XX","CONFIG_DYNAMIC_DEBUG","CONFIG_SPRD_HANG_DEBUG","CONFIG_ZRAM_WRITEBACK","CONFIG_ZRAM_DEDUP"]
config_noeffect_gki=["CONFIG_MODULE_SIG_ALL"]

def read_file(file_path, list_info):

    f = open(file_path, 'r')
    lines = f.read().splitlines()

    for i in range(len(lines)):
        if "# CONFIG_" in lines[i] and 'is not set' in lines[i]:
            config_name = lines[i].split(' ')[1]
            list_info[config_name] = "n"
        elif "CONFIG_" in lines[i] and '=y' in lines[i]:
            config_name = lines[i].split('=')[0]
            list_info[config_name] = "y"
        elif "CONFIG_" in lines[i] and '=' in lines[i]:
            config_name = lines[i].split('=')[0]
            val = lines[i].split('=')[1]
            list_info[config_name] = val

def create_fragment_config_dirc(fragment_path, fragment_config_info):
    global ret

    os.system("rm -rf " + fragment_defconfig)
    ret += os.system("KCONFIG_CONFIG=" + fragment_defconfig + " scripts/kconfig/merge_config.sh -m -r " + " " + gki_defconfig_path + " " + fragment_path)
    ret += os.system("make LLVM=1 ARCH=arm64 O=" + tmp_path + " fragment_gki_defconfig")
    if (ret):
        exit(1)

    read_file(tmp_path + "/.config", fragment_config_info)

def craete_gki_config_dirc(gki_config_info):
    global ret

    ret += os.system("make LLVM=1 ARCH=arm64 O=" + tmp_path + " gki_defconfig")
    if (ret):
        exit(1)

    read_file(tmp_path + "/.config", gki_config_info)

def check_fragment_gki_config_consistency(fragment_gki_info, fragment_soc_info,check_modify_gki):
    global check_flag

    for soc in fragment_soc_info:
        print >> sys.stderr, "=========================== %s =========================" % soc
        for config in fragment_soc_info[soc]:
            if  check_modify_gki == 1:
                if fragment_soc_info[soc][config] == "y":
                    if ( config in gki_config_info and gki_config_info[config] != "y" and config not in config_whitelist):
                        print >> sys.stderr, "ERROR: " + config + " should not be modify(gki_defconfig is defined " + gki_config_info[config] + " )!!!"
                        check_flag = 1
            else:
                if fragment_soc_info[soc][config] == "y":
                    if ( config not in fragment_gki_info or fragment_gki_info[config] != "y" ):
                        print >> sys.stderr, "ERROR: " + config + " isn't the subset of sprd_gki.fragment"
                        check_flag = 1
                elif fragment_soc_info[soc][config] == "m":
                    if ( config in gki_config_info and gki_config_info[config] != "m" ) and (config in gki_config_info and gki_config_info[config] != "n" ):
                        print >> sys.stderr, "ERROR: " + config + " should not be modify(gki_defconfig is defined " + gki_config_info[config] + " )!!!"
                        check_flag = 1
                    if ( config not in fragment_gki_info or fragment_gki_info[config] != "m" ):
                        print >> sys.stderr, "ERROR: " + config + " isn't the subset of sprd_gki.fragment soc"
                        check_flag = 1
                elif fragment_soc_info[soc][config] == "n":
                    if ( config in gki_config_info and gki_config_info[config] != "n" ) and ( config in gki_config_info and gki_config_info[config] != "m" ) and ( config not in config_noeffect_gki ) :
                        print >> sys.stderr, "ERROR: " + config + " should not be modify(gki_defconfig is defined " + gki_config_info[config] + " )!!!"
                        check_flag = 1
        print >> sys.stderr, "=========================== %s =========================" % soc

def do_clean():
    os.system("rm -rf " + fragment_defconfig)
    os.system("rm -rf " + tmp_path)
    (status, output)=commands.getstatusoutput("find ./arch/arm64 -name 'sprd_gki_*.fragment_without_module'")
    for fragment_without_m_path in output.split("\n"):
        os.system("rm -rf " + fragment_without_m_path)

def main():
    parser = argparse.ArgumentParser(description="The GKI Check Scripts Can Add Those Paremeter")
    parser.add_argument(
        "--clang",
        default=clang_path,
        help = "Set to the directory of clang compile tools")
    args = parser.parse_args()

    if not os.path.exists(clang_path):
        print >> sys.stderr, "ERROR: Clang not find!!"
        exit(1)

    os.environ['PATH'] = clang_path + ":" + os.environ['PATH']

    do_clean()
    craete_gki_config_dirc(gki_config_info)
    create_fragment_config_dirc(fragment_gki_path, fragment_gki_config_info)

    (status, output)=commands.getstatusoutput("find ./arch/arm64 -name 'sprd_gki_*.fragment'|grep -v 'debug'")
    for fragment_path in output.split("\n"):
        fragment_without_m_path=fragment_path+"_without_module"
        os.system("grep -v =m %s > %s" % (fragment_path,fragment_without_m_path))
        soc = fragment_path.split("/").pop().split("_").pop(2).split(".").pop(0)
        fragment_soc_config_info[soc] = {}
        fragment_soc_without_m_config_info[soc] = {}
        create_fragment_config_dirc(fragment_path, fragment_soc_config_info[soc])
        create_fragment_config_dirc(fragment_without_m_path, fragment_soc_without_m_config_info[soc])

    check_fragment_gki_config_consistency(fragment_gki_config_info, fragment_soc_config_info,0)
    check_fragment_gki_config_consistency(fragment_gki_config_info, fragment_soc_without_m_config_info,1)
    do_clean()
    exit(check_flag)

if __name__ == '__main__':
    main()
