#! /usr/bin/python
# -*- coding: utf-8 -*-

# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

import re
import string
import ConfigParser
import xml.dom.minidom

from ModuleObj import ModuleObj
from utility.util import LogLevel
from utility.util import log
from data.KpdData import KpdData

class KpdObj(ModuleObj):

    def __init__(self):
        ModuleObj.__init__(self, 'cust_kpd.h', 'cust_kpd.dtsi')


    def get_cfgInfo(self):
        cp = ConfigParser.ConfigParser(allow_no_value=True)
        cp.read(ModuleObj.get_cmpPath())

        ops = cp.options('Key_definition')
        for op in ops:
            KpdData._keyValueMap[op.upper()] = string.atoi(cp.get('Key_definition', op))

        KpdData._keyValueMap['NC'] = 0

        cp.read(ModuleObj.get_figPath())
        if cp.has_option('KEYPAD_EXTEND_TYPE', 'KEY_ROW'):
            KpdData.set_row_ext(string.atoi(cp.get('KEYPAD_EXTEND_TYPE', 'KEY_ROW')))
        if cp.has_option('KEYPAD_EXTEND_TYPE', 'KEY_COLUMN'):
            KpdData.set_col_ext(string.atoi(cp.get('KEYPAD_EXTEND_TYPE', 'KEY_COLUMN')))

        return True

    def read(self, node):
        nodes = node.childNodes
        for node in nodes:
            if node.nodeType == xml.dom.Node.ELEMENT_NODE:
                if node.nodeName == 'row':
                    row = string.atoi(node.childNodes[0].nodeValue)
                    KpdData.set_row(row)

                if node.nodeName == 'column':
                    col = string.atoi(node.childNodes[0].nodeValue)
                    KpdData.set_col(col)

                if node.nodeName == 'keyMatrix':
                    content = node.childNodes[0].nodeValue
                    content = content.replace('\t', '')
                    rows = content.split('''\n''')
                    matrix = []
                    for row in rows:
                        for item in row.split(' '):
                            matrix.append(item)
                    KpdData.set_matrix(matrix)
                    for item in matrix:
                        if cmp(item, 'NC') != 0:
                            KpdData._usedKeys.append(item)
                    KpdData._usedKeys.append('POWER')

                if node.nodeName == "keyMatrix_ext" and node.childNodes:
                    content = node.childNodes[0].nodeValue
                    content = content.replace('\t', '')
                    rows = content.split('''\n''')
                    matrix = []
                    for row in rows:
                        for item in row.split(' '):
                            matrix.append(item)
                    KpdData.set_matrix_ext(matrix)

                if node.nodeName == 'downloadKey':
                    keys = node.childNodes[0].nodeValue
                    KpdData.set_downloadKeys(keys.split(' '))

                if node.nodeName == 'modeKey':
                    value = node.childNodes[0].nodeValue
                    keys = value.split(' ')
                    KpdData._modeKeys['META'] = keys[0]
                    KpdData._modeKeys['RECOVERY'] = keys[1]
                    KpdData._modeKeys['FACTORY'] = keys[2]

                if node.nodeName == 'pwrKeyEint_gpioNum':
                    num = string.atoi(node.childNodes[0].nodeValue)
                    KpdData.set_gpioNum(num)

                if node.nodeName == 'pwrKeyUtility':
                    util = node.childNodes[0].nodeValue
                    KpdData.set_utility(util)

                if node.nodeName == 'home_key':
                    if len(node.childNodes) != 0:
                        home = node.childNodes[0].nodeValue
                    else:
                        home = ''
                    KpdData.set_homeKey(home)

                if node.nodeName == 'bPwrKeyUseEint':
                    flag = False
                    if node.childNodes[0].nodeValue == 'false':
                        flag = False
                    else:
                        flag = True

                    KpdData.set_useEint(flag)

                if node.nodeName == 'bPwrKeyGpioDinHigh':
                    flag = False
                    if node.childNodes[0].nodeValue == 'false':
                        flag = False
                    else:
                        flag = True

                    KpdData.set_gpioDinHigh(flag)

                if node.nodeName == 'pressPeriod':
                    time = string.atoi(node.childNodes[0].nodeValue)
                    KpdData.set_pressTime(time)

                if node.nodeName == 'keyType':
                    keyType = node.childNodes[0].nodeValue
                    KpdData.set_keyType(keyType)

        return True

    def parse(self, node):
        self.get_cfgInfo()
        self.read(node)

    def gen_files(self):
        ModuleObj.gen_files(self)

    def gen_spec(self, para):
        ModuleObj.gen_spec(self, para)


    def fill_hFile(self):
        gen_str = '''#include <linux/input.h>\n'''
        gen_str += '''#include <cust_eint.h>\n'''
        gen_str += '''\n'''
        gen_str += '''#define KPD_YES\t\t1\n'''
        gen_str += '''#define KPD_NO\t\t0\n'''
        gen_str += '''\n'''
        gen_str += '''/* available keys (Linux keycodes) */\n'''
        gen_str += '''#define KEY_CALL\t\tKEY_SEND\n'''
        gen_str += '''#define KEY_ENDCALL\tKEY_END\n'''
        gen_str += '''#undef KEY_OK\n'''
        gen_str += '''#define KEY_OK\t\tKEY_REPLY    /* DPAD_CENTER */\n'''
        gen_str += '''#define KEY_FOCUS\tKEY_HP\n'''
        gen_str += '''#define KEY_AT\t\tKEY_EMAIL\n'''
        gen_str += '''#define KEY_POUND\t228\t//KEY_KBDILLUMTOGGLE\n'''
        gen_str += '''#define KEY_STAR\t227\t//KEY_SWITCHVIDEOMODE\n'''
        gen_str += '''#define KEY_DEL\t\tKEY_BACKSPACE\n'''
        gen_str += '''#define KEY_SYM\t\tKEY_COMPOSE\n'''
        gen_str += '''\n'''
        gen_str += '''#define KPD_KEY_DEBOUNCE\t%d\n''' %(KpdData.get_pressTime())
        gen_str += '''#define KPD_PWRKEY_MAP\tKEY_%s\n''' %(KpdData.get_utility())
        # do not gen this macro if the home key is null
        if KpdData.get_homeKey() != '':
            gen_str += '''#define KPD_PMIC_RSTKEY_MAP\tKEY_%s\n''' %(KpdData.get_homeKey())
        if cmp(KpdData.get_keyType(), 'EXTEND_TYPE') != 0:
            gen_str += '''#define MTK_PMIC_PWR_KEY\t%d\n''' %(KpdData.get_col() - 1)
            if KpdData.get_homeKey() != '':
                gen_str += '''#define MTK_PMIC_RST_KEY\t\t%d\n''' %(2*KpdData.get_col() - 1)
            gen_str += '''\n'''
            gen_str += '''#define KPD_USE_EXTEND_TYPE\tKPD_NO\n'''
        else:
            gen_str += '''#define MTK_PMIC_PWR_KEY\t%d\n''' %(KpdData.get_col_ext() - 1)
            if KpdData.get_keyType() != '':
                gen_str += '''#define MTK_PMIC_RST_KEY\t\t%d\n''' %(2*KpdData.get_col_ext() - 1)
            gen_str += '''\n'''
            gen_str += '''#define KPD_USE_EXTEND_TYPE\tKPD_YES\n'''
        gen_str += '''\n'''
        gen_str += '''/* HW keycode [0 ~ 71] -> Linux keycode */\n'''
        gen_str += '''#define KPD_INIT_KEYMAP()\t\\\n'''
        gen_str += '''{\t\\\n'''


        if KpdData.get_keyType() == 'NORMAL_TYPE':
            for key in KpdData.get_matrix():
                if cmp(key, 'NC') != 0:
                    gen_str += '''\t[%d] = KEY_%s,\t\\\n''' %(KpdData.get_matrix().index(key), key)
        else:
            for key in KpdData.get_matrix_ext():
                if cmp(key, 'NC') != 0:
                    gen_str += '''\t[%d] = KEY_%s,\t\\\n''' %(KpdData.get_matrix_ext().index(key), key)

        gen_str += '''}\n'''
        gen_str += '''\n'''

        gen_str += '''/***********************************************************/\n'''
        gen_str += '''/****************Preload Customation************************/\n'''
        gen_str += '''/***********************************************************/\n'''
        gen_str += '''#define KPD_PWRKEY_EINT_GPIO\tGPIO%d\n''' %(KpdData.get_gpioNum())
        gen_str += '''#define KPD_PWRKEY_GPIO_DIN\t%d\n''' %(int(KpdData.get_gpioDinHigh()))
        gen_str += '''\n'''

        for key in KpdData.get_downloadKeys():
            if cmp(key, 'NC') != 0:
                dlIdx = KpdData.get_downloadKeys().index(key)
                mtxIdx = self.get_matrixIdx(key)
                gen_str += '''#define KPD_DL_KEY%d\t%d\t/* KEY_%s */\n''' %(dlIdx+1, mtxIdx, key)
        gen_str += '''\n'''

        gen_str += '''/***********************************************************/\n'''
        gen_str += '''/****************Uboot Customation**************************/\n'''
        gen_str += '''/***********************************************************/\n'''

        for (key, value) in KpdData.get_modeKeys().items():
            if cmp(value, 'NC') != 0:
                idx = self.get_matrixIdx(value)
                #idx = KpdData.get_matrix().index(value)
                gen_str += '''#define MT65XX_%s_KEY\t%d\t/* KEY_%s */\n''' %(key, idx, value)

        gen_str += '''\n'''

        return gen_str

    def get_matrixIdx(self, value):
        if KpdData.get_keyType() == 'NORMAL_TYPE':
            if cmp(value, 'POWER') == 0:
                return KpdData.get_col() - 1
            elif cmp(value, KpdData.get_homeKey()) == 0:
                return 2 * KpdData.get_col() - 1
            else:
                return KpdData.get_matrix().index(value)
        elif KpdData.get_keyType() == 'EXTEND_TYPE':
            if cmp(value, 'POWER') == 0:
                return KpdData.get_col_ext() - 1
            elif cmp(value, KpdData.get_homeKey()) == 0:
                return 2 * KpdData.get_col_ext() - 1
            else:
                return KpdData.get_matrix_ext().index(value)

    def fill_dtsiFile(self):
        gen_str = '''&keypad {\n'''
        gen_str += '''\tmediatek,kpd-key-debounce = <%d>;\n''' %(KpdData.get_pressTime())
        gen_str += '''\tmediatek,kpd-sw-pwrkey = <%d>;\n''' %(KpdData._keyValueMap[KpdData.get_utility()])
        if KpdData.get_keyType() == 'NORMAL_TYPE':
            gen_str += '''\tmediatek,kpd-hw-pwrkey = <%d>;\n''' %(KpdData.get_col()-1)
        else:
            gen_str += '''\tmediatek,kpd-hw-pwrkey = <%d>;\n''' %(KpdData.get_col_ext()-1)

        #gen_str += '''\tmediatek,kpd-sw-rstkey  = <%d>;\n''' %(KpdData._keyValueMap[KpdData.get_homeKey()])
        if KpdData.get_homeKey() != '':
            gen_str += '''\tmediatek,kpd-sw-rstkey  = <%d>;\n''' %(KpdData.get_keyVal(KpdData.get_homeKey()))
        if KpdData.get_keyType() == 'NORMAL_TYPE':
            if KpdData.get_homeKey() != '':
                gen_str += '''\tmediatek,kpd-hw-rstkey = <%d>;\n''' %(2*KpdData.get_col() - 1)
            gen_str += '''\tmediatek,kpd-use-extend-type = <0>;\n'''
        else:
            if KpdData.get_homeKey() != '':
                gen_str += '''\tmediatek,kpd-hw-rstkey = <%d>;\n''' %(2*KpdData.get_col_ext() - 1)
            gen_str += '''\tmediatek,kpd-use-extend-type = <1>;\n'''

        #gen_str += '''\tmediatek,kpd-use-extend-type = <0>;\n'''
        gen_str += '''\t/*HW Keycode [0~%d] -> Linux Keycode*/\n''' %(KpdData.get_row() * KpdData.get_col() - 1)
        gen_str += '''\tmediatek,kpd-hw-map-num = <%d>;\n''' %(KpdData.get_row() * KpdData.get_col())
        gen_str += '''\tmediatek,kpd-hw-init-map = <'''

        if KpdData.get_keyType() == 'NORMAL_TYPE':
            for key in KpdData.get_matrix():
                idx = KpdData._keyValueMap[key]
                gen_str += '''%d ''' %(idx)
        else:
            for key in KpdData.get_matrix_ext():
                idx = KpdData._keyValueMap[key]
                gen_str += '''%d ''' %(idx)

        gen_str.rstrip()
        gen_str += '''>;\n'''
        gen_str += '''\tmediatek,kpd-pwrkey-eint-gpio = <%d>;\n''' %(KpdData.get_gpioNum())
        gen_str += '''\tmediatek,kpd-pwkey-gpio-din  = <%d>;\n''' %(int(KpdData.get_gpioDinHigh()))
        for key in KpdData.get_downloadKeys():
            if cmp(key, 'NC') == 0:
                continue
            gen_str += '''\tmediatek,kpd-hw-dl-key%d = <%s>;\n''' %(KpdData.get_downloadKeys().index(key), self.get_matrixIdx(key))

        for (key, value) in KpdData.get_modeKeys().items():
            if cmp(value, 'NC') == 0:
                continue
            gen_str += '''\tmediatek,kpd-hw-%s-key = <%d>;\n''' %(key.lower(), self.get_matrixIdx(value))

        gen_str += '''\tstatus = \"okay\";\n'''
        gen_str += '''};\n'''

        return gen_str






