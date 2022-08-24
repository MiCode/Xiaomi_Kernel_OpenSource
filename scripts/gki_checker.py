# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2021 Mediatek Inc.

#!/usr/bin/env python3

###################################################################################################################
#                                                                                                                 #
#   This program is used for dump different files/Symbols/config between MTK kernel tree and Google Kernel tree   #
#                                                                                                                 #
###################################################################################################################

import datetime
import difflib
import os
import re
import shutil
import string
import sys
import subprocess

from optparse import OptionParser
from pprint import pprint

date_path = datetime.date.today()
config_wlist = []
symbol_wlist = []
file_wlist = []
g_config_pass = True
g_symbol_pass = True
g_file_pass = True
options = None
wiki = "https://wiki.mediatek.inc/display/KernelStandardization/GKI+checker"
checker_version = "v2.0"
default_google_sha = "41ff3fa8fff9"

def create_compare_file(target, input_file, output_file_name):
    if target == "Config":
        # print "create "+ input_file +" config list"
        # get config from vmlinux
        content = subprocess.check_output(options.config_tool + " "+input_file, shell=True)
        with open(output_file_name, "wb") as f:
            f.write(content)
    elif target == "Symbol":
        # print "create " + input_file + " symbol list"
        # get System.map from vmlinux
        cmd = options.tool_chain + "llvm-nm -n --print-size " + input_file + " | grep -v '\( [aNUw] \)\|\(__crc_\)\|\( \$[adt]\)\|\( \.L\)' > " + output_file_name
        os.system(cmd)

def write_dict_file(dict_cont, with_size, filename):
    with open(options.checker_out + filename, "w") as f:
        if with_size:
            for x, c_list in dict_cont.items():
                str_cont = x + " " + "".join(item for item in c_list)
                f.write(str_cont +'\n')
        else:
            for x in dict_cont.keys():
                f.write(x +'\n')

def compare_config(g_config_file, m_config_file):
    global g_config_pass
    nonset_config_info = re.compile("^(-|\+)# (CONFIG_\w*) is not set$")
    set_config_info = re.compile("^(-|\+)(CONFIG_\w*)=(.*)$")
    mt_info = re.compile("MT\d+|MTK|MEDIATEK")

    cmd = "diff --strip-trailing-cr -EbwB --unified=0 " + g_config_file + " " + m_config_file
    output = os.popen(cmd)
    diff_cont = output.read()
    output.close()
    g_config = {}
    m_config = {}
    for line in diff_cont.splitlines():
        if line[0:3] in ("+++", "---", "@@ "):
            continue
        if line[0] in ("+", "-") and len(line) == 1:
            continue
        if line[0:2] in ("+#", "-#") and len(line) == 2:
            continue

        nset_info = nonset_config_info.search(line)
        set_info = set_config_info.search(line)
        if nset_info:
            if nset_info.group(2) in config_wlist:
                continue
            if nset_info.group(1)=="-":
                g_config[nset_info.group(2)] = "n"
                # print(nset_info.group(1), "nset:", nset_info.group(2))
            elif nset_info.group(1)=="+" and nset_info.group(2) in g_config:
                if g_config[nset_info.group(2)] != "m" and g_config[nset_info.group(2)] != "y":
                    g_config.pop(nset_info.group(2))
                else:
                    m_config[nset_info.group(2)] = "n"
                    # print("Worning!!", nset_info.group(1), "nset:", nset_info.group(2))
        if set_info:
            if set_info.group(2) in config_wlist:
                continue
            if set_info.group(1)=="-":
                g_config[set_info.group(2)] = set_info.group(3)
                # print(set_info.group(1), "set:", set_info.group(2), ", value:", set_info.group(3))
            elif set_info.group(1)=="+":
                if set_info.group(3)=="m" and set_info.group(2) in g_config:
                    if g_config[set_info.group(2)] != "y":
                        g_config.pop(set_info.group(2))
                    else:
                        m_config[set_info.group(2)] = set_info.group(3)
                        # print("Worning!!", set_info.group(1), "set:", set_info.group(2), ", value:", set_info.group(3))
                elif set_info.group(3)=="y" and set_info.group(2) in g_config:
                    if g_config[set_info.group(2)] != "n" and g_config[set_info.group(2)] != "m":
                        g_config.pop(set_info.group(2))
                    else:
                        m_config[set_info.group(2)] = set_info.group(3)
                        # print("Worning!!", set_info.group(1), "set:", set_info.group(2), ", value:", set_info.group(3))
                elif not mt_info.search(set_info.group(2)) and set_info.group(3) != "m":
                    m_config[set_info.group(2)] = set_info.group(3)
                    # print("Worning!!", set_info.group(1), "set:", set_info.group(2), ", value:", set_info.group(3))
    if g_config:
        print("Error: Addition Google config\n{ ", end="")
        g_config_pass = False
        # pprint(g_config)
        for x in g_config.keys():
            print(x, end=", ")
        print("}")
        write_dict_file(g_config, True, "config/gki_cfg.txt")
    if m_config:
        print("Error: Redundant MTK config\n{ ", end="")
        g_config_pass = False
        # pprint(m_config)
        for x in m_config.keys():
            print(x, end=", ")
        print("}")
        write_dict_file(m_config, True, "config/mtk_cfg.txt")

def get_gki_denyfile():
    # remove old denyfiles.txt
    cmd = "rm -f " + options.checker_out + "file/tmp/gki_denyfiles.txt"
    print(cmd)
    os.system(cmd)
    # get redount strings in file path ex. out_abi/android12-5.10/common and replace "/" as "\/" cause sed
    cmd = options.tool_chain + "llvm-dwarfdump --debug-info "+ options.google_vmlinux + " | grep -m 1 \"DW_AT_comp_dir\" | awk 'BEGIN {FS=\"\\\"\"} {print $2}'"
    #print(cmd)
    output = os.popen(cmd)
    restr = output.read().splitlines()[0].replace("/","\/")
    output.close()
    # get denyfile
    cmd = options.tool_chain + "llvm-dwarfdump --debug-info "+ options.google_vmlinux \
    + " | grep \"DW_AT_decl_file\" | awk 'BEGIN {FS=\"\\\"\"} {print $2}' | sed 's/" \
    + restr + "\///' | sort | uniq > " + options.checker_out + "file/tmp/tmp_gki_denyfiles.txt"
    print(cmd)
    os.system(cmd)
    tmp_list = []
    with open(options.checker_out + "file/tmp/tmp_gki_denyfiles.txt", "r") as t:
        for x in t:
            tmp_list.append(os.path.normpath(x))
    tmp_list.sort()
    f = open(options.checker_out + "file/tmp/gki_denyfiles.txt", "w")
    for x in tmp_list:
        f.write(x)
    f.close()

'''
# TODO report info, current not use
def verify_violation(diff_cont):
    ret = False
    start_synx_info = re.compile('^\+(#if IS_ENABLED\(|#ifdef |#if defined\()(\w*)\)?$')
    single_line_star = re.compile('^-\t*(else|if *\( *.*\)|else if *\( *.*\)) *$')
    multi_line_star = re.compile('^\+\t*\}? *(else|if *\( *.*\)|else if *\( *.*\)) *\{.*$')
    close_star = re.compile('^\+\t*\} *(else *|else if *\(.*\) *)?\{?$')
    config_trace = False
    # special_case = False
    for line in diff_cont.splitlines():
        # bypass unnecessary line info
        if line[0:3] in ("+++", "---", "@@ "):
            print("b:"+line)
            continue
        elif line[0] in ("-", "+", " ") and len(line) == 1:
            print("b:"+line)
            continue
        elif line[0:4] == "diff":
            print("b:"+line)
            continue
        elif line[0:5] == "index":
            print("b:"+line)
            continue

        start_synx = start_synx_info.search(line)
        if start_synx:
            # print(start_synx.group(2))
            config_trace = True
            print("c:"+line)
            continue
        elif line == "+#else" or line == "+#endif":
            config_trace = False
            print("c:"+line)
            continue

        if config_trace:
            if line[0] != "+":
                print("v:"+line)
                ret = True
                # break
            else:
                print("c:"+line)
        else:
            # if single_line_star.search(line):
            #     # print("n:", line)
            #     special_case = True
            # elif line[0] != " ":
            if line[0] != " ":
                # if special_case:
                #     if multi_line_star.search(line):
                #         print("n:", line)
                #         continue
                #     elif close_star.search(line):
                #         print("n:", line)
                #         special_case = False
                #         continue
                print("v:"+line)
                ret = True
                # break
    return ret
'''

def get_sha(vmlinux):
    sha_default = True
    sha = default_google_sha
    sha_re = re.compile('^Linux version.*-g(\w*)-.*$')
    # Get Goolge commit sha from google vmlinux
    g_content = subprocess.check_output(options.tool_chain+"llvm-strings "+(vmlinux)+" | grep \"Linux version\"", shell=True).decode("utf-8")
    for ctx in g_content.split("\n"):
        sha_info = sha_re.search(ctx)
        if sha_info:
            sha_default = False
            sha = sha_info.group(1)
            print("Google SHA:" + sha)
            break
    # Get MTK commit tag from git commit message
    try:
        tag_re = re.compile('^.* ACK: Merge (.*)( \w*|-\w*) into .*$')
        content = subprocess.check_output("git log --grep=" + sha + " | grep \"ACK: Merge\"", shell=True).decode("utf-8")
        for ctx in content.split("\n"):
            tag_info = tag_re.search(ctx)
            if tag_info:
                tag = tag_info.group(1)
                try:
                    subprocess.check_output("git remote remove ack", shell=True).decode("utf-8")
                except subprocess.CalledProcessError as warn:
                    print("No Need to remote remove ack!")
                    #print(warn)
                try:
                    subprocess.check_output("git remote add ack --no-tags --mirror=fetch https://gerrit.mediatek.inc/kernel/common && git fetch ack " + tag, shell=True).decode("utf-8")
                except subprocess.CalledProcessError as warn:
                    print("No Need to fetch ack info!")
                    #print(warn)
                break
    except subprocess.CalledProcessError as warn:
        print("Not MP stage")
        #print(warn)

    if sha_default:
        print("Warning: Can't get ACK_SHA!, Use default SHA:" + sha)
    return sha

def filecompare(fileName):
    # DO NOT change the input file order, or the verify() will be wrong
    # return true when file context are different
    # googlefile = google_path + fileName
    # mtkfile = mtk_path + fileName
    # cmd = "diff --strip-trailing-cr -EbwBu " + googlefile + " " + mtkfile
    cmd = "git diff --ignore-all-space --ignore-blank-lines " + options.ACK_SHA + " -- " + fileName
    output = os.popen(cmd)
    diff_cont = output.read()
    output.close()
    # if diff_cont and verify_violation(diff_cont):
    if diff_cont:
        print("\nError: You should Not modify " + fileName)
        gcmd = "git show " + options.ACK_SHA + ":" + fileName
        mcmd = "git show HEAD:" + fileName
        goutput = os.popen(gcmd)
        gdiff_cont = goutput.read()
        google_file = open(options.checker_out + "file/google/" + fileName.replace("/","_"),"w")
        google_file.write(gdiff_cont)
        google_file.close()

        moutput = os.popen(mcmd)
        mdiff_cont = moutput.read()
        mtk_file = open(options.checker_out + "file/mtk/" + fileName.replace("/","_"),"w")
        mtk_file.write(mdiff_cont)
        mtk_file.close()
        return True
    else:
        return False

def dump_diff_file():
    global g_file_pass
    options.ACK_SHA = get_sha(options.google_vmlinux)
    gki54_file = open(options.checker_out + "file/tmp/gki_denyfiles.txt","r")
    summry_file = open(options.checker_out + "file/summary.log","w")
    print("Checking", end="")
    for idx, line in enumerate(gki54_file):
        if idx%100 == 99:
            print(".", end="", flush=True)
        file_name = line.strip()
        if file_name in file_wlist:
            continue
        if filecompare(file_name):
            g_file_pass = False
            summry_file.write(file_name+'\n')
    print("")
    gki54_file.close()
    summry_file.close()

def is_hex_str(s):
    return set(s).issubset(string.hexdigits)

def count_dec_char(input_str):                  # return # of [0-9] char
    return sum(c.isdigit() for c in input_str)

def contain_offset(input_str_list):             # TODO
    ret = []
    for idx, item in enumerate(input_str_list):
        if item.isdigit():
            ret.append(idx)
    return ret

def clean_str(redundant_str):
    if len(redundant_str.split('.')) > 1 and redundant_str.split('.')[-1].isdigit():
        str_obfs = '.'.join(item for item in redundant_str.split('.')[:-1])
        return str_obfs, redundant_str.split('.')[-1]
    elif len(redundant_str.split('_'))>1:
        sp_list = redundant_str.split('_')
        if len(sp_list[-1]) == 16 and is_hex_str(sp_list[-1]):       # contain hex
            str_obfs = '_'.join(item for item in sp_list[:-1])
            return str_obfs, sp_list[-1]
        else:
            rd_value_list = []
            if count_dec_char(sp_list[-1])>=2:       # dec number on the tail of symbol
                rd_value_list.append(sp_list[-1])
                sp_list[-1] = re.sub(r'\d+', '', sp_list[-1])
            co = contain_offset(sp_list[:-1])
            for idx in co:    # dec number in the middle of symbol
                rd_value_list.append(sp_list[idx])
                sp_list[idx] = ""
            str_obfs = '_'.join(item for item in sp_list)
            rd_value = '_'.join(item for item in rd_value_list)
            return str_obfs, rd_value
    return redundant_str, ""

def parsing_System_map(file_name):
    symbols = []
    unsize_symbols = []
    with open(file_name, "r") as f:
        for line in f.readlines():
            sb = line.split()
            sb[3] , rd_value = clean_str(sb[3])
            tmp = sb[3] + " " + str(int(sb[1], 16))
            if sb[3] not in symbol_wlist:
                symbols.append(tmp)
                unsize_symbols.append(sb[3])
    return unsize_symbols, symbols

def compare_symbols(g_symbols, m_symbols, with_size):
    sym_size_info = re.compile("^(-|\+)(\w*) ?(\d*)?$")
    diff = difflib.unified_diff(g_symbols, m_symbols, n=0)
    sdiff = sorted(diff, reverse=True)
    g_symbol = {}
    m_symbol = {}
    for line in sdiff:
        if line[0:3] in ("+++", "---", "@@ ") or len(line) == 0:
            continue
        if line.find("__Cortex") > -1:
            continue
        info = sym_size_info.search(line)
        if info:
            size = '0'
            symb = info.group(2)
            if with_size:
                size = info.group(3)
            if info.group(1) == "-":
                if symb in g_symbol:
                    g_symbol[symb].append(size)
                else:
                    g_symbol[symb] = [size]
            if info.group(1) == "+":
                if symb in g_symbol:
                    if size in g_symbol[symb]:
                        if len(g_symbol[symb]) == 1:
                            g_symbol.pop(symb)
                        else:
                            g_symbol[symb].remove(size)
                    else:
                        if symb in m_symbol:
                            m_symbol[symb].append(size)
                        else:
                            m_symbol[symb] = [size]
                else:
                    if symb in m_symbol:
                        m_symbol[symb].append(size)
                    else:
                        m_symbol[symb] = [size]
    return g_symbol, m_symbol

def compare_System_map(g_sysmap, m_sysmap):
    global g_symbol_pass
    g_ns_symbols, g_symbols = parsing_System_map(g_sysmap)
    m_ns_symbols, m_symbols = parsing_System_map(m_sysmap)
    g_symbols.sort(key=lambda x: x[0])
    m_symbols.sort(key=lambda x: x[0])
    g_ns_symbols.sort(key=lambda x: x[0])
    m_ns_symbols.sort(key=lambda x: x[0])
    # Compare two System.map symbols
    #print("Compare symbols")
    g_s_report, m_s_report = compare_symbols(g_symbols, m_symbols, True)
    if g_s_report:
        print("Error: Symbol size different\n{ ", end="")
        g_symbol_pass = False
        for x in g_s_report.keys():
            print(x, end=", ")
        print("}")
        write_dict_file(g_s_report, True, "symbol/gki_size_diff.txt")
    if m_s_report:
        g_symbol_pass = False
        # pprint(m_s_report)
        write_dict_file(m_s_report, True, "symbol/mtk_size_diff.txt")

    # print("Remove symbols with different size but same name ")
    g_ns_report, m_ns_report = compare_symbols(g_ns_symbols, m_ns_symbols, False)

    if g_ns_report:
        print("Error: Addition Google Symbols:\n{ ", end="")
        g_symbol_pass = False
        for x in g_ns_report.keys():
            print(x, end=", ")
        print("}")
        write_dict_file(g_ns_report, True, "symbol/gki_panic.txt")
    if m_ns_report:
        print("Error: Redundant MTK Symbols:\n{ ", end="")
        g_symbol_pass = False
        for x in m_ns_report.keys():
            print(x, end=", ")
        print("}")
        write_dict_file(m_ns_report, True, "symbol/mtk_panic.txt")

def remove_tmp(opt_type):
    try:
        shutil.rmtree(options.checker_out + opt_type + "tmp/")
    except OSError as e:
        print("Error: %s - %s." % (e.filename, e.strerror))

def checker_verify():
    if g_config_pass and g_symbol_pass and g_file_pass:
        print("Good Job!")
        return 0
    if not g_config_pass:
        print("Error: Config check failed!")
    if not g_symbol_pass:
        print("Error: Symbol check failed!")
    if not g_file_pass:
        print("Error: File check failed!")
    print("For more detail, please check wiki:"+ wiki)
    return -1

def read_white_list(filename):
    cfg=[]
    sbl=[]
    fil=[]
    with open(filename, "r") as f:
        for line in f.readlines():
            line = line.replace('\r', '')
            line = line.replace('\n', '')
            sb = line.split(" ")
            if sb[0] == "f":
                fil.append(sb[1])
            elif sb[0] == "c":
                cfg.append(sb[1])
            elif sb[0] == "s":
                sbl.append(sb[1])
    return cfg, sbl, fil

def update_white_list(filename):
    cfg=[]
    sbl=[]
    fil=[]
    curr_path = os.path.dirname(os.path.abspath(__file__))
    if os.path.exists(options.checker_out + "file/summary.log"):
        os.remove(options.checker_out + "file/summary.log")
    if os.path.exists(options.checker_out + "symbol/gki_size_diff.txt"):
        os.remove(options.checker_out + "symbol/gki_size_diff.txt")
    if os.path.exists(options.checker_out + "symbol/gki_panic.txt"):
        os.remove(options.checker_out + "symbol/gki_panic.txt")
    if os.path.exists(options.checker_out + "symbol/mtk_panic.txt"):
        os.remove(options.checker_out + "symbol/mtk_panic.txt")
    if os.path.exists(options.checker_out + "config/mtk_cfg.txt"):
        os.remove(options.checker_out + "config/mtk_cfg.txt")
    print("Update File white list")
    get_gki_denyfile()
    os.chdir(options.kernel_path)
    dump_diff_file()
    os.chdir(curr_path)
    with open(options.checker_out + "file/summary.log","r") as f:
        for line in f.readlines():
            fil.append(line)
    print("Update Symbol white list")
    create_compare_file("Symbol", options.google_vmlinux, options.checker_out + "symbol/tmp/g_System.map")
    create_compare_file("Symbol", options.mtk_vmlinux, options.checker_out + "symbol/tmp/m_System.map")
    compare_System_map(options.checker_out + "symbol/tmp/g_System.map", options.checker_out + "symbol/tmp/m_System.map")
    if os.path.exists(options.checker_out + "symbol/gki_size_diff.txt"):
        with open(options.checker_out + "symbol/gki_size_diff.txt","r") as f:
            for line in f.readlines():
                sbl.append(line.split()[0]+'\n')
    if os.path.exists(options.checker_out + "symbol/gki_panic.txt"):
        with open(options.checker_out + "symbol/gki_panic.txt","r") as f:
            for line in f.readlines():
                sbl.append(line.split()[0]+'\n')
    if os.path.exists(options.checker_out + "symbol/mtk_panic.txt"):
        with open(options.checker_out + "symbol/mtk_panic.txt","r") as f:
            for line in f.readlines():
                sbl.append(line.split()[0]+'\n')
    print("Update Config white list")
    create_compare_file("Config", options.google_vmlinux, options.checker_out + "config/tmp/google_kconfig")
    create_compare_file("Config", options.mtk_vmlinux, options.checker_out + "config/tmp/mtk_kconfig")
    compare_config(options.checker_out + "config/tmp/google_kconfig", options.checker_out + "config/tmp/mtk_kconfig")
    if os.path.exists(options.checker_out + "config/mtk_cfg.txt"):
        with open(options.checker_out + "config/mtk_cfg.txt","r") as f:
            for line in f.readlines():
                cfg.append(line.split()[0]+'\n')
    with open(filename, "w") as w:
        for f in fil:
            w.write("f " + f)
        for s in sbl:
            w.write("s " + s)
        for c in cfg:
            w.write("c " + c)

def getExecuteOptions(self, args=[]):
    parser = OptionParser()
    croot = ""
    if os.getenv('TOP') == None:
        croot=os.path.dirname(os.path.abspath(__file__))+"/../.."
    else:
        croot=os.getenv('TOP')

    parser.add_option("-O", "--out", nargs=1, dest="checker_out", default=os.path.dirname(os.path.abspath(__file__))+"/checker_out/",
                      help="Checker output folder")
    parser.add_option("-k", "--kpath", nargs=1, dest="kernel_path", default=croot+"/kernel-5.10/",
                      help="Kernel path")
    parser.add_option("-t", "--tcpath", nargs=1, dest="tool_chain", default=croot+"/kernel/prebuilts-master/clang/host/linux-x86/clang-r416183b/bin/",
                      help="Extract tool chain")
    parser.add_option("-c", "--ct", nargs=1, dest="config_tool", default=croot+"/kernel-5.10/scripts/extract-ikconfig",
                      help="Config extract script")
    parser.add_option("-g", "--gv", nargs=1, dest="google_vmlinux", default=croot+"/vendor/aosp_gki/kernel-5.10/aarch64/vmlinux-userdebug",
                      help="Google vmlinux location")
    parser.add_option("-m", "--mv", nargs=1, dest="mtk_vmlinux", default=croot+"/out_krn/target/product/mgk_64_k510/obj/KERNEL_OBJ/vmlinux",
                      help="MTK vmlinux location")
    parser.add_option("-a", "--ack", nargs=1, dest="ACK_SHA", default="",
                      help="SHA align Google ack")
    parser.add_option("-w", "--white", nargs=1, dest="white_list", default=os.path.dirname(os.path.abspath(__file__))+"/gki_checker_white_list.txt",
                      help="white list location")
    parser.add_option("-o", "--opt", nargs=1, dest="opt", default="all",
                      help="check option: 'config', 'file', 'symbol' or all(default)\n'update' for update white list.")
    (options, args) = parser.parse_args()
    return options

#main function
if __name__ == "__main__":
    start_time = datetime.datetime.now()
    curr_path = os.path.dirname(os.path.abspath(__file__))
    options = getExecuteOptions(sys.argv[1:])
    options.checker_out = os.path.abspath(options.checker_out)+'/'
    #options.ACK_SHA = get_sha(options.google_vmlinux)

    if options.opt == "update":
        os.makedirs(options.checker_out+"file/google/", exist_ok=True)
        os.makedirs(options.checker_out+"file/mtk/", exist_ok=True)
        os.makedirs(options.checker_out+"file/tmp/", exist_ok=True)
        os.makedirs(options.checker_out+"symbol/tmp/", exist_ok=True)
        os.makedirs(options.checker_out+"config/tmp/", exist_ok=True)
        update_white_list(options.white_list)
        remove_tmp("symbol/")
        remove_tmp("file/")
        remove_tmp("config/")
    else:
        config_wlist ,symbol_wlist, file_wlist = read_white_list(options.white_list)

    if options.opt == "file":
        os.makedirs(options.checker_out+"file/google/", exist_ok=True)
        os.makedirs(options.checker_out+"file/mtk/", exist_ok=True)
        os.makedirs(options.checker_out+"file/tmp/", exist_ok=True)
        print("Check file")
        get_gki_denyfile()
        os.chdir(options.kernel_path)
        dump_diff_file()
        os.chdir(curr_path)
        remove_tmp("file/")
    elif options.opt == "config":
        os.makedirs(options.checker_out+"config/tmp/", exist_ok=True)
        print("Check Config")
        create_compare_file("Config", options.google_vmlinux, options.checker_out + "config/tmp/google_kconfig")
        create_compare_file("Config", options.mtk_vmlinux, options.checker_out + "config/tmp/mtk_kconfig")
        compare_config(options.checker_out + "config/tmp/google_kconfig", options.checker_out + "config/tmp/mtk_kconfig")
        remove_tmp("config/")
    elif options.opt == "symbol":
        os.makedirs(options.checker_out+"symbol/tmp/", exist_ok=True)
        print("Check Symbol")
        create_compare_file("Symbol", options.google_vmlinux, options.checker_out + "symbol/tmp/g_System.map")
        create_compare_file("Symbol", options.mtk_vmlinux, options.checker_out + "symbol/tmp/m_System.map")
        compare_System_map(options.checker_out + "symbol/tmp/g_System.map", options.checker_out + "symbol/tmp/m_System.map")
        remove_tmp("symbol/")
    elif options.opt == "all":
        os.makedirs(options.checker_out+"file/google/", exist_ok=True)
        os.makedirs(options.checker_out+"file/mtk/", exist_ok=True)
        os.makedirs(options.checker_out+"file/tmp/", exist_ok=True)
        os.makedirs(options.checker_out+"symbol/tmp/", exist_ok=True)
        os.makedirs(options.checker_out+"config/tmp/", exist_ok=True)
        print("Check file")
        get_gki_denyfile()
        os.chdir(options.kernel_path)
        dump_diff_file()
        os.chdir(curr_path)
        print("Check Symbol")
        create_compare_file("Symbol", options.google_vmlinux, options.checker_out + "symbol/tmp/g_System.map")
        create_compare_file("Symbol", options.mtk_vmlinux, options.checker_out + "symbol/tmp/m_System.map")
        compare_System_map(options.checker_out + "symbol/tmp/g_System.map", options.checker_out + "symbol/tmp/m_System.map")
        print("Check Config")
        create_compare_file("Config", options.google_vmlinux, options.checker_out + "config/tmp/google_kconfig")
        create_compare_file("Config", options.mtk_vmlinux, options.checker_out + "config/tmp/mtk_kconfig")
        compare_config(options.checker_out + "config/tmp/google_kconfig", options.checker_out + "config/tmp/mtk_kconfig")
        remove_tmp("symbol/")
        remove_tmp("file/")
        remove_tmp("config/")

    end_time = datetime.datetime.now()
    elapsedTime = end_time - start_time
    mins, secs = divmod(elapsedTime.total_seconds(), 60)
    print("Total time elapsed: "+str(int(mins))+" mins, "+str(int(secs))+" secs")

    if options.opt != "update" and checker_verify():
        sys.exit("Failed")
    else:
        sys.exit(0)
