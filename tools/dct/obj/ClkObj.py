#! /usr/bin/python
# -*- coding: utf-8 -*-

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

import os
import re
import string
import ConfigParser

import xml.dom.minidom

from ModuleObj import ModuleObj
from data.ClkData import ClkData
from utility.util import log
from utility.util import LogLevel
from utility.util import sorted_key

DEFAULT_AUTOK = 'AutoK'
class ClkObj(ModuleObj):
    def __init__(self):
        ModuleObj.__init__(self, 'cust_clk_buf.h', 'cust_clk_buf.dtsi')
        #self.__prefix_cfg = 'driving_current_pmic_clk_buf'
        self.__suffix = '_BUF'
        self.__count = -1

    def read(self, node):
        nodes = node.childNodes
        for node in nodes:
            if node.nodeType == xml.dom.Node.ELEMENT_NODE:
                if node.nodeName == 'count':
                    continue

                varNode = node.getElementsByTagName('varName')
                curNode = node.getElementsByTagName('current')

                key = re.findall(r'\D+', node.nodeName)[0].upper() + self.__suffix + '%s' %(re.findall(r'\d+', node.nodeName)[0])

                if key not in ModuleObj.get_data(self):
	                continue;

                data = ModuleObj.get_data(self)[key]

                if len(varNode):
                    data.set_varName(varNode[0].childNodes[0].nodeValue)

                if len(curNode):
                    data.set_current(curNode[0].childNodes[0].nodeValue)

                ModuleObj.set_data(self, key, data)

        return True

    def get_cfgInfo(self):
        cp = ConfigParser.ConfigParser(allow_no_value=True)
        cp.read(ModuleObj.get_figPath())

        count = string.atoi(cp.get('CLK_BUF', 'CLK_BUF_COUNT'))
        self.__count = count

        ops = cp.options('CLK_BUF')
        for op in ops:
            if op == 'clk_buf_count':
                self.__count = string.atoi(cp.get('CLK_BUF', op))
                ClkData._count = string.atoi(cp.get('CLK_BUF', op))
                continue

            value = cp.get('CLK_BUF', op)
            var_list = value.split(':')

            data = ClkData()
            data.set_curList(var_list[2:])
            data.set_defVarName(string.atoi(var_list[0]))
            data.set_defCurrent(string.atoi(var_list[1]))

            key = op[16:].upper()
            ModuleObj.set_data(self, key, data)

    def parse(self, node):
        self.get_cfgInfo()
        self.read(node)

    def gen_files(self):
        ModuleObj.gen_files(self)

    def fill_hFile(self):
        gen_str = '''typedef enum {\n'''
        gen_str += '''\tCLOCK_BUFFER_DISABLE,\n'''
        gen_str += '''\tCLOCK_BUFFER_SW_CONTROL,\n'''
        gen_str += '''\tCLOCK_BUFFER_HW_CONTROL\n'''
        gen_str += '''} MTK_CLK_BUF_STATUS;\n'''
        gen_str += '''\n'''

        gen_str += '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_AUTO_K = -1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_0,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_2,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_3\n'''
        gen_str += '''} MTK_CLK_BUF_DRIVING_CURR;\n'''
        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            gen_str += '''#define %s_STATUS_PMIC\t\tCLOCK_BUFFER_%s\n''' %(key[5:], value.get_varName().upper())

        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            idx = value.get_curList().index(value.get_current())
            if cmp(value.get_curList()[0], DEFAULT_AUTOK) == 0:
                idx -= 1

            if idx >= 0:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_%d\n''' %(key, idx)
            else:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_AUTO_K\n''' %(key)

        gen_str += '''\n'''

        return gen_str

    def fill_dtsiFile(self):
        gen_str = '''&pmic_clock_buffer_ctrl {\n'''
        gen_str += '''\tmediatek,clkbuf-quantity = <%d>;\n''' %(self.__count)
        gen_str += '''\tmediatek,clkbuf-config = <'''

        #sorted_list = sorted(ModuleObj.get_data(self).keys())
        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('PMIC') == -1:
                continue
            value = ModuleObj.get_data(self)[key]
            gen_str += '''%d ''' %(ClkData._varList.index(value.get_varName()))

        gen_str = gen_str.rstrip()
        gen_str += '''>;\n'''

        gen_str += '''\tmediatek,clkbuf-driving-current = <'''

        #sorted_list = sorted(ModuleObj.get_data(self).keys())
        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('PMIC') == -1:
                continue
            value = ModuleObj.get_data(self)[key]
            idx = value.get_curList().index(value.get_current())
            if cmp(value.get_curList()[0], DEFAULT_AUTOK) == 0:
                idx -= 1
            if idx < 0:
                gen_str += '''(%d) ''' %(-1)
            else:
                gen_str += '''%d ''' %(idx)

        gen_str = gen_str.rstrip()
        gen_str += '''>;\n'''

        gen_str += '''\tstatus = \"okay\";\n'''
        gen_str += '''};\n'''

        return gen_str


class ClkObj_Everest(ClkObj):
    def __init__(self):
        ClkObj.__init__(self)
        self.__rf = 'RF'
        self.__pmic = 'PMIC'

    def parse(self, node):
        ClkObj.parse(self, node)

    def gen_files(self):
        ClkObj.gen_files(self)

    def fill_hFile(self):
        gen_str = '''typedef enum {\n'''
        gen_str += '''\tCLOCK_BUFFER_DISABLE,\n'''
        gen_str += '''\tCLOCK_BUFFER_SW_CONTROL,\n'''
        gen_str += '''\tCLOCK_BUFFER_HW_CONTROL\n'''
        gen_str += '''} MTK_CLK_BUF_STATUS;\n'''
        gen_str += '''\n'''

        gen_str += '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_0_4MA,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_0_9MA,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_1_4MA,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_1_9MA\n'''
        gen_str += '''} MTK_CLK_BUF_DRIVING_CURR;\n'''
        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find(self.__pmic) != -1:
                gen_str += '''#define %s_STATUS_PMIC\t\t\t\tCLOCK_BUFFER_%s\n''' %(key[5:], value.get_varName())

        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find(self.__pmic) != -1:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_%sMA\n''' %(key, value.get_current().replace('.', '_'))

        gen_str += '''\n'''


        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find(self.__rf) != -1:
                gen_str += '''#define %s_STATUS\t\tCLOCK_BUFFER_%s\n''' %(key[3:], value.get_varName())

        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find(self.__rf) != -1:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_%sMA\n''' %(key, value.get_current().replace('.', '_'))

        gen_str += '''\n'''



        return gen_str

    def fill_dtsiFile(self):
        gen_str = ClkObj.fill_dtsiFile(self)

        gen_str += '''\n'''

        gen_str += '''&rf_clock_buffer_ctrl {\n'''
        gen_str += '''\tmediatek,clkbuf-quantity = <%d>;\n''' %(len(ModuleObj.get_data(self))-ClkData._count)
        msg = 'rf clk buff count : %d' %(len(ModuleObj.get_data(self))-ClkData._count)
        log(LogLevel.info, msg)
        gen_str += '''\tmediatek,clkbuf-config = <'''

        #sorted_list = sorted(ModuleObj.get_data(self).keys())

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]

            if key.find(self.__rf) != -1:
                gen_str += '''%d ''' %(ClkData._varList.index(value.get_varName()))
        gen_str = gen_str.rstrip()
        gen_str += '''>;\n'''

        gen_str += '''\tmediatek,clkbuf-driving-current = <'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find(self.__rf) != -1:
                idx = value.get_curList().index(value.get_current())
                if cmp(value.get_curList()[0], DEFAULT_AUTOK) == 0:
                    idx -= 1
                gen_str += '''%d ''' %(idx)

        gen_str.rstrip()
        gen_str += '''>;\n'''

        gen_str += '''\tstatus = \"okay\";\n'''
        gen_str += '''};\n'''

        return gen_str

class ClkObj_Olympus(ClkObj_Everest):

    def __init__(self):
        ClkObj_Everest.__init__(self)

    def get_cfgInfo(self):
        ClkObj_Everest.get_cfgInfo(self)

    def parse(self, node):
        ClkObj_Everest.parse(self, node)

    def gen_files(self):
        ClkObj_Everest.gen_files(self)

    def fill_hFile(self):
        gen_str = '''typedef enum {\n'''
        gen_str += '''\tCLOCK_BUFFER_DISABLE,\n'''
        gen_str += '''\tCLOCK_BUFFER_SW_CONTROL,\n'''
        gen_str += '''\tCLOCK_BUFFER_HW_CONTROL\n'''
        gen_str += '''} MTK_CLK_BUF_STATUS;\n'''
        gen_str += '''\n'''

        gen_str += '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_AUTO_K = -1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_0,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_2,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_3\n'''
        gen_str += '''} MTK_CLK_BUF_DRIVING_CURR;\n'''
        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find('PMIC') != -1:
                gen_str += '''#define %s_STATUS_PMIC\t\tCLOCK_BUFFER_%s\n''' %(key[5:], value.get_varName())

        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find('RF') != -1:
                gen_str += '''#define %s_STATUS\t\t\t\tCLOCK_BUFFER_%s\n''' %(key[3:], value.get_varName())

        gen_str += '''\n'''


        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('PMIC') != -1:
                continue
            value = ModuleObj.get_data(self)[key]
            idx = value.get_curList().index(value.get_current())
            if cmp(value.get_curList()[0], DEFAULT_AUTOK) == 0:
                idx -= 1

            if idx >= 0:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_%d\n''' %(key, idx)
            else:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_AUTO_K\n''' %(key)

        gen_str += '''\n'''


        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('RF') != -1:
                continue
            value = ModuleObj.get_data(self)[key]
            idx = value.get_curList().index(value.get_current())
            if cmp(value.get_curList()[0], DEFAULT_AUTOK) == 0:
                idx -= 1

            if idx >= 0:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_%d\n''' %(key, idx)
            else:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_AUTO_K\n''' %(key)

        gen_str += '''\n'''

        return gen_str

class ClkObj_Rushmore(ClkObj):

    def __init__(self):
        ClkObj.__init__(self)
        self.__suffix = '_BUF'

    def parse(self, node):
        ClkObj.parse(self, node)

    def get_cfgInfo(self):
        cp = ConfigParser.ConfigParser(allow_no_value=True)
        cp.read(ModuleObj.get_figPath())

        count = string.atoi(cp.get('CLK_BUF', 'CLK_BUF_COUNT'))
        self.__count = count

    def read(self, node):
        nodes = node.childNodes
        for node in nodes:
            if node.nodeType == xml.dom.Node.ELEMENT_NODE:
                if node.nodeName == 'count':
                    continue

                varNode = node.getElementsByTagName('varName')
                curNode = node.getElementsByTagName('current')

                key = re.findall(r'\D+', node.nodeName)[0].upper() + self.__suffix + '%s' %(re.findall(r'\d+', node.nodeName)[0])
                data = ClkData()
                if len(varNode):
                    data.set_varName(varNode[0].childNodes[0].nodeValue)

                #if len(curNode):
                    #data.set_current(curNode[0].childNodes[0].nodeValue)

                ModuleObj.set_data(self, key, data)

        return True

    def fill_hFile(self):
        gen_str = '''typedef enum {\n'''
        gen_str += '''\tCLOCK_BUFFER_DISABLE,\n'''
        gen_str += '''\tCLOCK_BUFFER_SW_CONTROL,\n'''
        gen_str += '''\tCLOCK_BUFFER_HW_CONTROL\n'''
        gen_str += '''} MTK_CLK_BUF_STATUS;\n'''
        gen_str += '''\n'''

        gen_str += '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_AUTO_K = -1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_0,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_2,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_3\n'''
        gen_str += '''} MTK_CLK_BUF_DRIVING_CURR;\n'''
        gen_str += '''\n'''


        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find('RF') != -1:
                gen_str += '''#define %s_STATUS\t\t\t\tCLOCK_BUFFER_%s\n''' %(key[3:], value.get_varName())

        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('RF') != -1:
                continue
            value = ModuleObj.get_data(self)[key]
            idx = value.get_curList().index(value.get_current())
            if cmp(value.get_curList()[0], DEFAULT_AUTOK) == 0:
                idx -= 1

            if idx >= 0:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_%d\n''' %(key, idx)
            else:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_AUTO_K\n''' %(key)

        gen_str += '''\n'''

        return gen_str

    def fill_dtsiFile(self):
        gen_str = '''&rf_clock_buffer_ctrl {\n'''
        gen_str += '''\tmediatek,clkbuf-quantity = <%d>;\n''' %(self.__count)
        gen_str += '''\tmediatek,clkbuf-config = <'''

        #sorted_list = sorted(ModuleObj.get_data(self).keys())
        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('RF') == -1:
                continue
            value = ModuleObj.get_data(self)[key]
            gen_str += '''%d ''' %(ClkData._varList.index(value.get_varName()))

        gen_str = gen_str.rstrip()
        gen_str += '''>;\n'''

        gen_str += '''\tstatus = \"okay\";\n'''
        gen_str += '''};\n'''

        return gen_str



