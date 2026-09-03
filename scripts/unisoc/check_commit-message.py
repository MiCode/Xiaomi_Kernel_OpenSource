#!/usr/bin/python
# -*- coding: UTF-8 -*-
# SPDX-License-Identifier: GPL-2.0

import os
import re
import sys
import commands

MAIN_PATH = ''
TAGS_FILE_NAME = 'Documentation/unisoc/patch-tags.txt'
AGREEMENT_FILE_NAME = 'Documentation/unisoc/Open-Source-Agreement'

GET_PATCH_INFO_COMMANDS = 'git log -1'
GET_PATCH_MODIFY_FILE_INFO = 'git log -1 --pretty="format:" --name-only'

ATTRIBUTE_TAGS  = []
SUBSYSTEM1_TAGS = []
SUBSYSTEM2_TAGS = []
SUBSYSTEM3_TAGS = []
SUBSYSTEM1_TAGS_NOCHECK = []
SPECIAL_CHECK_TAGS = ['Documentation', 'dts']
PATCH_SIGNATURE = []
check_tags_flag = 1
check_signature_flag = 1
check_perm_flag = 0

def read_line(path,file_name):
    read_file = path + '/' + file_name
    f = open(read_file, 'rb')
    lines = f.readlines()
    f.close()
    return lines

def get_tags():
    global ATTRIBUTE_TAGS
    global SUBSYSTEM1_TAGS
    global SUBSYSTEM2_TAGS
    global SUBSYSTEM3_TAGS
    global SUBSYSTEM1_TAGS_NOCHECK
    get_tags_flag = 0

#print "main path: %s" % MAIN_PATH

    read_tags_list = read_line(KERNEL_DIR,TAGS_FILE_NAME)

    for x in read_tags_list:
        if "\n" in x:
            x = x.strip("\n")

        if "[info]" in x and get_tags_flag == 0:
            get_tags_flag = 1
            continue
        elif get_tags_flag == 0:
            continue

        if ":" in x and get_tags_flag == 1:
            subsystem1_tags = ''
            subsystem2_tags = ''
            subsystem3_tags = ''

            subsystem_list = x.split(":")
#print "subsystem tags:%s %d" % (subsystem_list,len(subsystem_list))
            if '*' in x:
                SUBSYSTEM1_TAGS_NOCHECK.append(subsystem_list[0].replace(' ',''))
                continue

            subsystem1_tags = subsystem_list[0].replace(' ','')
            if len(subsystem_list) >= 4:
                subsystem2_tags = subsystem_list[1].replace(' ','')
                subsystem3_tags = subsystem_list[2].replace(' ','')
            elif len(subsystem_list) >= 3:
                subsystem2_tags = subsystem_list[1].replace(' ','')

#print "subsystem tags: 1:%s,2:%s,3:%s" % (subsystem1_tags,subsystem2_tags,subsystem3_tags)

            if subsystem1_tags not in SUBSYSTEM1_TAGS:
                SUBSYSTEM1_TAGS.append(subsystem1_tags)
                SUBSYSTEM2_TAGS.append([subsystem2_tags])
                SUBSYSTEM3_TAGS.append([subsystem3_tags])
            else:
                if len(subsystem_list) >= 4:
                    if subsystem2_tags not in SUBSYSTEM2_TAGS[SUBSYSTEM1_TAGS.index(subsystem1_tags)]:
                        SUBSYSTEM2_TAGS[SUBSYSTEM1_TAGS.index(subsystem1_tags)].append(subsystem2_tags)
                    if subsystem3_tags not in SUBSYSTEM3_TAGS[SUBSYSTEM1_TAGS.index(subsystem1_tags)]:
                        SUBSYSTEM3_TAGS[SUBSYSTEM1_TAGS.index(subsystem1_tags)].append(subsystem3_tags)
                elif len(subsystem_list) >= 3:
                    if subsystem2_tags not in SUBSYSTEM2_TAGS[SUBSYSTEM1_TAGS.index(subsystem1_tags)]:
                        SUBSYSTEM2_TAGS[SUBSYSTEM1_TAGS.index(subsystem1_tags)].append(subsystem2_tags)

#print "SUBSYSTEM1_TAGS: %s" % SUBSYSTEM1_TAGS
#print "SUBSYSTEM2_TAGS: %s" % SUBSYSTEM2_TAGS
#print "SUBSYSTEM3_TAGS: %s" % SUBSYSTEM3_TAGS

        elif "," in x and get_tags_flag == 1:
            ATTRIBUTE_TAGS = x.split(",")
#           print "attribute tags:%s" % ATTRIBUTE_TAGS

#    print "SUBSYSTEM1_TAGS_NOCHECK: %s" % SUBSYSTEM1_TAGS_NOCHECK
#    print "SUBSYSTEM1_TAGS: %s" % SUBSYSTEM1_TAGS
#    print "SUBSYSTEM1_TAGS num = %d" % len(SUBSYSTEM1_TAGS)

def find_last_char(string, p):
    index = 0
    i = 0
    for x in string:
        if p == x:
            index = i
        i += 1

    return index

def get_osa_list():
    osa_file_list = read_line(KERNEL_DIR,AGREEMENT_FILE_NAME)
    tags_list_tmp = [x.strip() for x in osa_file_list]
    osa_start= tags_list_tmp.index("- Your Name <your-email-address@unisoc.com>")
    osa_list=tags_list_tmp[osa_start+1:]
    return osa_list

def check_patch_signature(patch_info_list):
    global PATCH_SIGNATURE
    global check_signature_flag
    current_signature = ""
    RET = 0

    read_message_lines = read_line(KERNEL_DIR,AGREEMENT_FILE_NAME)

    sob_list=[]
    osa_list=get_osa_list()

    for i in patch_info_list:
        if "Author: " in i:
            author=i[len("Author: "):]
        if "    Signed-off-by: " in i:
            sob_list.append(i[len("    Signed-off-by: "):])
    if len(sob_list):
        if author not in sob_list:
            print >> sys.stderr, "\nERROR: Signature not contain the Author!!!"
            print >> sys.stderr, "Please add the Signature in commit message:Signed-off-by: %s\n" % author
            RET = 1
        for sob in sob_list:
            if "- " + sob in osa_list:
                print >> sys.stdout, "Oneof Sob %s is the OSA Documentation" % sob
                check_signature_flag=0
                break
        if check_signature_flag == 1:
                print >> sys.stderr, "\nERROR: None of following signatures not in Documentation/unisoc/Open-Source-Agreement"
                print >> sys.stderr, "%s\n" % sob_list
                RET = 1
    else:
        print >> sys.stderr, "\nERROR: Signature(Signed-off-by) doesnot exist for commit message!!!"
        print >> sys.stderr, "Please add the Singature by run command: git commit --amend -s\n"
        RET = 1
    return RET

def check_tags_commit_id(patch_info_list):
    global check_tags_flag
    check_issue_num = 0
    check_title_flag = 1
    check_commit_id_flag = 0
    tags_list_start_num = 0
    ret_hit_tags_list = []
    attribute_temp = []

    get_tags()

    for x in patch_info_list:
        if check_title_flag == 1 and "Bug #" in x:
            print >> sys.stdout, "Patch title:\n%s" % x
            check_title_flag = 0

            if "  " in x[x.index("Bug #"):]:
                return (1, "More than two consecutive spaces in the title")
            if "：" in x:
                return (1, "The patch title contains : of chinese")
            if ":" not in x:
                return (1, "The patch donot contains tag")
            if not x[x.index('#') + 1:x.index(':')].replace(' ','').isalnum():
                characters_temp_list = x[x.index('#') + 1:x.index(':')].split(' ')
                if not ((characters_temp_list[1] in SUBSYSTEM1_TAGS_NOCHECK or \
                        characters_temp_list[1] in SUBSYSTEM1_TAGS) \
                        and characters_temp_list[0].isalnum()):
                    return (1, "Title contains special characters between bug id and tags")

            if len(x.split(":")) != len(x.split(": ")):
                return (1, "expected ' ' after ':'")

            tags_list = x[x.index("Bug #") + len("Bug #"):find_last_char(x, ":")].split(' ')[1:]
#            print "tags list:%s" % tags_list

            if "BACKPORT" == tags_list[tags_list_start_num]:
                tags_list_start_num += 1

            if tags_list[tags_list_start_num].strip(":") in ATTRIBUTE_TAGS:
                if tags_list[tags_list_start_num].strip(":") in ATTRIBUTE_TAGS[0:ATTRIBUTE_TAGS.index("ANDROID") + 1]:
                    for line in patch_info_list:
                        if re.search('Bug: \d{9,}',line):
                            check_issue_num=1
                    if check_issue_num != 1:
                        return(1, "the patch is not from aosp kernel/common branch")
                if tags_list[tags_list_start_num].strip(":") in ATTRIBUTE_TAGS[0:ATTRIBUTE_TAGS.index("FROMLIST") + 1]:
                    check_tags_flag = 0
                    check_commit_id_flag = 0
                elif tags_list[tags_list_start_num].strip(":") in ATTRIBUTE_TAGS[ATTRIBUTE_TAGS.index("WORKAROUND"):]:
                    check_tags_flag = 1
                else:
                    check_tags_flag = 0
                    check_commit_id_flag = 1
                tags_list_start_num += 1

            if check_tags_flag == 1:
                if tags_list_start_num < len(tags_list):
                    if tags_list[tags_list_start_num].strip(":") in SUBSYSTEM1_TAGS_NOCHECK:
                        ret_hit_tags_list.append(tags_list[tags_list_start_num].strip(":"))
                    elif tags_list[tags_list_start_num].strip(":") in SUBSYSTEM1_TAGS:
                        ret_hit_tags_list.append(tags_list[tags_list_start_num].strip(":"))
                        tags_list_start_num += 1
                        if tags_list_start_num < len(tags_list):
                            if tags_list[tags_list_start_num].strip(":") in SUBSYSTEM2_TAGS[SUBSYSTEM1_TAGS.index(tags_list[tags_list_start_num - 1].strip(":"))]:
                                ret_hit_tags_list.append(tags_list[tags_list_start_num].strip(":"))
                                tags_list_start_num += 1
                                if tags_list_start_num < len(tags_list):
                                    if tags_list[tags_list_start_num].strip(":") in SUBSYSTEM3_TAGS[SUBSYSTEM1_TAGS.index(tags_list[tags_list_start_num - 2].strip(":"))]:
                                        ret_hit_tags_list.append(tags_list[tags_list_start_num].strip(":"))
                                        continue
                                    else:
                                        return (1, "The subsystem 3 tag is error")
                                else:
                                    continue
                            else:
                                return (1, "The subsystem 2 tag is error")
                        else:
                            continue
                    else:
                        return (1, "The subsystem 1 tag is error")
                else:
                    return (1, "The title donot contains subsystem tag")
        elif check_commit_id_flag == 1 and "commit" in x:
            # check commit id ok
            print >> sys.stdout, "check commit id ok"
            return (0, ret_hit_tags_list)

    if check_commit_id_flag == 1:
        return (1, "The patch donot contains commit id")

    return (0, ret_hit_tags_list)

def check_tags_file(modify_file_list, tags_list):
    file_name_list_temp = []
    inconsistent_file_list = []
    file_add_inconsistent_flag = 0
    file_and_tag_consistent_flag = 0
    special_inconsistent_flag = 0

    if "asoc" in tags_list:
        tags_list[tags_list.index("asoc")] = 'sound'
    if "arm/arm64" in tags_list:
        if ','.join(modify_file_list).find('arch') > 0 and \
        (','.join(modify_file_list).find('arm') < 0  or \
        ','.join(modify_file_list).find('arm64') < 0):
            file_add_inconsistent_flag += 1
        del tags_list[tags_list.index("arm/arm64")]

    for x in modify_file_list:

        if len(x) < 2:
            continue
        #elif "." in x:
        #    file_name_list_temp = x.split(".")[0].split("/")
        else:
            file_name_list_temp = x.split("/")

        print >> sys.stdout, "Modified file name: %s" % x

        for y in tags_list:
            if y not in file_name_list_temp and 'include' not in file_name_list_temp \
            and 'configs'not in file_name_list_temp:
                file_add_inconsistent_flag += 1
                for z in file_name_list_temp:
                    if z in y:
                        break
                    elif y in z:
                        break
                if file_add_inconsistent_flag == 1:
                    inconsistent_file_list.append(x)
                    for special_tag in SPECIAL_CHECK_TAGS:
                        if special_tag in tags_list:
                            special_inconsistent_flag = 1
                            break
                        if special_tag in file_name_list_temp:
                            #针对sprd-configs文件做特殊处理，允许Documentation目录下sprd-configs文件和其他文件一起修改
                            if special_inconsistent_flag == 0 and "sprd-configs" in file_name_list_temp:
                                continue
                            else:
                                special_inconsistent_flag = 1
                            break
                    break

        if file_add_inconsistent_flag == 0 and file_and_tag_consistent_flag == 0:
            file_and_tag_consistent_flag = 1

    if file_and_tag_consistent_flag == 1 and special_inconsistent_flag == 0:
        return (0, inconsistent_file_list)

    return (-1, inconsistent_file_list)

def check_osa_ifsorted():
    is_sorted_flag=0
    status,output=commands.getstatusoutput(GET_PATCH_MODIFY_FILE_INFO)
    get_patch_modify_file_list = output.split('\n')
    if AGREEMENT_FILE_NAME in get_patch_modify_file_list:
        osa_list=get_osa_list()
        if (sorted(osa_list) == osa_list or sorted(osa_list,reverse=True) == osa_list):
            print >> sys.stdout,"\nAdd OSA sorted check pass\n"
        else:
            print >> sys.stderr, "\nYour OSA is not sorted alphabetically\n"
            is_sorted_flag=1
    return is_sorted_flag

def check_duplicate():
    check_duplicate_flag = 0
    status,output=commands.getstatusoutput(GET_PATCH_MODIFY_FILE_INFO)
    get_patch_modify_file_list = output.split('\n')
    if TAGS_FILE_NAME in get_patch_modify_file_list:
        print >> sys.stdout,"modify patch-tags.txt,check duplicate tag."
        tags_file_list = read_line(KERNEL_DIR,TAGS_FILE_NAME)
        # remove '\n' for each element
        tags_list_tmp = [x.strip() for x in tags_file_list]
        # change list to str and split ',' to list
        str_tags_file = ','.join(tags_list_tmp)
        tags_list_tmp = str_tags_file.split(',')
        # remove the empty element
        tags_list = [i for i in tags_list_tmp if i != "" ]
        info_start = tags_list.index("[info]")
        list_all_tag = tags_list[info_start+1:]
        lower_list_all_tags = [tags.lower().strip(':') for tags in list_all_tag]
        for tag in list_all_tag:
            num = lower_list_all_tags.count(tag.lower().strip(':'))
            if num > 1 :
                print >> sys.stderr, "%s duplicate" % tag
                check_duplicate_flag=1
    if check_duplicate_flag == 0 :
         print >> sys.stdout, "\nCheck duplicate tag ok\n"
    return check_duplicate_flag

def check_tags_consistent(ret_info):
    RET = 0
    if ret_info[0] != 0:
        print >> sys.stderr, "\nERROR: %s" %  ret_info[1]
        print >> sys.stderr, "Please read 'Documentation/unisoc/patch-tags.txt' file."
        RET = 1
    else:
        print >> sys.stdout, "\nCheck tags OK, tags list: %s\n" % ret_info[1]

        if check_tags_flag == 1:
            status,output=commands.getstatusoutput(GET_PATCH_MODIFY_FILE_INFO)
            get_patch_modify_file_list = output.split('\n')

            ret_check_file = check_tags_file(get_patch_modify_file_list, ret_info[1])

            if ret_check_file[0] != 0:
                print >> sys.stderr, "\nERROR: Tags and modified files are inconsistent!"
                print >> sys.stderr, "Please read 'Documentation/unisoc/patch-tags.txt' file.\n\ninconsistent file list:"
                for x in ret_check_file[1]:
                    print >> sys.stderr, "%s" % x
                RET = 1
            elif len(ret_check_file[1]) > 0:
                print >> sys.stdout, "\nWARNING: Tags and modified files are inconsistent.\n\ninconsistent file list:"
                for x in ret_check_file[1]:
                    print >> sys.stderr, "%s" % x
            else:
                print >> sys.stdout, "\nTags and modified files are consistent!"
        else:
            print >> sys.stdout, "\nINFO:Ignore check of tags and modified files!"
    return RET

def check_modify_file_perm():
    check_perm_flag=0
    status,output=commands.getstatusoutput(GET_PATCH_MODIFY_FILE_INFO)
    get_patch_modify_file_list = output.split('\n')
    for f in get_patch_modify_file_list:
        if "scripts/unisoc" not in f:
            if (os.access(f,os.R_OK|os.W_OK|os.X_OK)):
                print >> sys.stderr, "\nDO NOT ADD execute permission for %s\n" % f
                check_perm_flag=1
    return check_perm_flag

def main(argv=None):
    return_val = 0
    ret_info = []
    ret_check_file = []
    get_patch_info_list = []
    get_patch_modify_file_list = []

    get_patch_info_list = output.split('\n')

#print "get patch info:"
#for x in get_patch_info_list:
#print "%s" % x

    return_val += check_patch_signature(get_patch_info_list)
    ret_info = check_tags_commit_id(get_patch_info_list)
    return_val += ret_info[0]

    return_val += check_duplicate()
    return_val += check_osa_ifsorted()
    return_val += check_tags_consistent(ret_info)
    return_val += check_modify_file_perm()
    return return_val


if __name__ == '__main__':

    MAIN_PATH = os.path.dirname(os.path.abspath(sys.argv[0]))
    KERNEL_DIR = os.path.abspath(os.path.dirname(MAIN_PATH)+os.path.sep+"..")
    status,output=commands.getstatusoutput(GET_PATCH_INFO_COMMANDS)

    sys.exit(main())
