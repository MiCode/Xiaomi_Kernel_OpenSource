#!/usr/bin/env python3

import re
import os
import sys

script_path=os.path.split(os.path.realpath(__file__))[0]
kernel_path=os.path.abspath(os.path.join(script_path,"../.."))
abi_whitelist_file=os.path.join(kernel_path,"android/abi_gki_aarch64_unisoc")
new_abi_whitelist_file="/tmp/new_abi"

def list_print(_list):
    for i in range(len(_list)):
        print(_list[i])

def get_abi_list(_file):
    list_file=[]
    with open(_file) as f:
        lines=f.readlines()
    for i in range(len(lines)):
        list_file.append(lines[i].strip())
    return list_file

def get_sorted_modules_list():
    modules_list=[]
    for i in range(len(list_all_symbols)):
        if "# required by" in list_all_symbols[i]:
            modules_list.append(list_all_symbols[i])
    return sorted(list(set(sorted(modules_list,key=str.lower))))

def get_list_index(lst=None, item=''):
    return [index for (index,value) in enumerate(lst) if value == item]

def sort_symbol(_list):
    def __key(a):
        assert (a)
        if not a:
            return a
        tmp = a.lower()
        for idx,c in enumerate(tmp):
            if c != "_":
                break
        return (tmp.replace("_", "") + (5 - idx) * "_", a)

    return sorted(set(_list),key=__key)

def get_sorted_symbols_for_module(module):
    global list_all_symbols
    module_index=get_list_index(list_all_symbols,module)
    symbol=[]
    for i in range(len(module_index)):
        tmp_symbols=[]
        tmp_symbols=list_all_symbols[module_index[i]+1:]
        for j in range(len(tmp_symbols)):
            if len(tmp_symbols[j]) == 0 or "# " in tmp_symbols[j] :
                break
            else:
                symbol.append(tmp_symbols[j])
    return sort_symbol(symbol)

list_all_symbols=get_abi_list(abi_whitelist_file)
list_all_symbols.pop(0)
all_index=["# commonly used symbols"]
new_abi_list=["[abi_symbol_list]"]

def  main(update_flag):
    sorted_modules_list=get_sorted_modules_list()
    for i in range(len(sorted_modules_list)):
        all_index.append(sorted_modules_list[i])

    for i in range(len(all_index)):
        new_abi_list.append(all_index[i])

        symbol_index_list = get_list_index(list_all_symbols,all_index[i])

        tmp_module_symbol=[]
        tmp_module_symbol=get_sorted_symbols_for_module(all_index[i])

        for j in range(len(tmp_module_symbol)):
            new_abi_list.append("  "+tmp_module_symbol[j])
        new_abi_list.append("")

    f=open(new_abi_whitelist_file,"w")

    for line in new_abi_list:
        f.write(line+'\n')
    f.close()

    if get_abi_list(new_abi_whitelist_file) == get_abi_list(abi_whitelist_file):
        check_flag=0
        print("==== abi whitlist order correct, check pass====",file=sys.stdout)
        if update_flag == 1:
            print("==== no need to update the abi whitelist ====",file=sys.stderr)
    else:
        check_flag=1
        print("==== abi whitlist is not sorted by order,check fail====",file=sys.stderr)
        if update_flag == 1:
            print("==== update the abi whitelist ====",file=sys.stdout)
            cmd="cp " + new_abi_whitelist_file + " " +abi_whitelist_file
            os.system(cmd)

    return check_flag

if __name__ == '__main__':
    if len(sys.argv) >1 and sys.argv[1] == "update":
        update_flag=1
    else:
        update_flag=0
    sys.exit(main(update_flag))
