#! /usr/bin/python
# -*- coding: utf-8 -*-

# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

import sys, os
import re
import ConfigParser
import xml.dom.minidom

from ModuleObj import ModuleObj
from data.PmicData import PmicData

from utility.util import log
from utility.util import LogLevel
from utility.util import compare
from utility.util import sorted_key


class PmicObj(ModuleObj):
    def __init__(self):
        ModuleObj.__init__(self, 'pmic_drv.h', 'cust_pmic.dtsi')
        self.__fileName = 'pmic_drv.c'
        self.__chipName = ''
        self.__defLdo = ''
        self.__appCount = -1
        self.__func = ''
        self.__paraList = []
        self.__headerList = []


    def get_cfgInfo(self):
        cp = ConfigParser.ConfigParser(allow_no_value=True)
        cp.read(ModuleObj.get_cmpPath())

        PmicData._var_list = cp.options('APPLICATION')

        if self.__chipName == '':
            return
        #parse the pmic config file
        cmpPath = os.path.join(sys.path[0], 'config', self.__chipName + '.cmp')
        if not os.path.exists(cmpPath) or not os.path.isfile(cmpPath):
            log(LogLevel.error, 'Can not find %s pmic config file!' %(self.__chipName))
            sys.exit(-1)
        cp.read(cmpPath)
        self.__defLdo = cp.get('PMIC_TABLE', 'LDO_APPNAME_DEFAULT')
        self.__headerList = cp.get('PMIC_TABLE', 'INCLUDE_HEADER').split(':')
        self.__func = cp.get('PMIC_TABLE', 'FUNCTION')

        for i in range(1, cp.getint('PMIC_TABLE', 'NUM_LDO')+1):
            key = 'LDO_NAME%d' %(i)
            self.__paraList.append(cp.get(key, 'PARAMETER_NAME'))

        #parse app count in fig file
        cp.read(ModuleObj.get_chipId() + '.fig')

        cp.read(ModuleObj.get_figPath())
        self.__appCount = cp.getint('Chip Type', 'PMIC_APP_COUNT')

    def read(self, node):
        nodes = node.childNodes
        for node in nodes:
            if node.nodeType == xml.dom.Node.ELEMENT_NODE:
                if cmp(node.nodeName, 'chip') == 0:
                    if len(node.childNodes) == 0:
                       break
                    self.__chipName = node.childNodes[0].nodeValue
                    continue
                if cmp(node.nodeName, 'count') == 0:
                    continue
                ldoNode = node.getElementsByTagName('ldoVar')
                defNode = node.getElementsByTagName('defEn')

                data = PmicData()
                if len(ldoNode):
                    data.set_ldoName(ldoNode[0].childNodes[0].nodeValue)

                if len(defNode):
                    number = -1
                    if cmp(defNode[0].childNodes[0].nodeValue, 'SKIP') == 0:
                        number = 0
                    elif cmp(defNode[0].childNodes[0].nodeValue, 'OFF') == 0:
                        number = 1
                    else:
                        number = 2
                    data.set_defEnable(number)

                name_list = []
                for i in range(0, 6):
                    key = 'varName%d' %(i)
                    nameNode = node.getElementsByTagName(key)
                    if len(nameNode):
                        name_list.append(nameNode[0].childNodes[0].nodeValue)

                data.set_nameList(name_list)

                ModuleObj.set_data(self, node.nodeName, data)

        return True

    def parse(self, node):
        self.read(node)
        self.get_cfgInfo()

    def gen_files(self):
        ModuleObj.gen_files(self)
        self.gen_cFile()

    def gen_cFile(self):
        fp = open(os.path.join(ModuleObj.get_genPath(), self.__fileName), 'w')
        gen_str = ''
        gen_str += ModuleObj.writeComment()
        gen_str += self.fill_cFile()
        fp.write(gen_str)
        fp.close()


    def fill_hFile(self):
        gen_str = ''
        used = []
        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            for name in value.get_nameList():
                if name.strip() != '':
                    used.append(name)
                    gen_str += '''#define PMIC_APP_%s\t\t\t%s_POWER_LDO_%s\n''' %(name, self.__chipName[5:11], value.get_ldoName())


        gen_str += '''\n'''

        gen_str += '''/**********Output default name********************/\n'''

        for varName in PmicData._var_list:
            if not varName.upper() in used:
                gen_str += '''#define PMIC_APP_%s\t\t\t%s\n''' %(varName.upper(), self.__defLdo)

        return gen_str


    def fill_dtsiFile(self):
        gen_str = ''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            gen_str += '''&mt_pmic_%s_ldo_reg {\n''' %(value.get_ldoName().lower())
            gen_str += '''\tregulator-name = \"%s\";\n''' %((value.get_ldoName().replace('_', '')).lower())
            gen_str += '''\tregulator-default-on = <%d>; /* 0:skip, 1: off, 2:on */\n''' %(value.get_defEnable())
            gen_str += '''\tstatus = \"okay\";\n'''
            gen_str += '''};\n'''

        gen_str += '''\n'''
        gen_str += '''&kd_camera_hw1 {\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            for varName in value.get_nameList():
            #for i in range(0, self.__appCount):
                bExisted = False
                postFix = ''
                #varName = value.get_nameList()[i]
                if varName.find('CAMERA') != -1:
                    postFix = varName[varName.rfind('_')+1:]
                    bExisted = True

                if varName.find('MAIN_CAMERA') != -1:
                    gen_str += '''\tvcam%s-supply = <&mt_pmic_%s_ldo_reg>;\n''' %(postFix.lower(), value.get_ldoName().lower())

                if varName.find('SUB_CAMERA') != -1:
                    gen_str += '''\tvcam%s_main2-supply = <&mt_pmic_%s_ldo_reg>;\n''' %(postFix.lower(), value.get_ldoName().lower())
                    gen_str += '''\tvcam%s_sub-supply = <&mt_pmic_%s_ldo_reg>;\n''' %(postFix.lower(), value.get_ldoName().lower())

            #if bExisted == True:
                #gen_str += '''\n'''

        gen_str += '''\tstatus = \"okay\";\n'''
        gen_str += '''};\n\n'''
        gen_str += '''&touch {\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            for name in value.get_nameList():
                if name.find('TOUCH') != -1:
                    gen_str += '''\tvtouch-supply = <&mt_pmic_%s_ldo_reg>;\n''' %(value.get_ldoName().lower())

        gen_str += '''\tstatus = \"okay\";\n'''
        gen_str += '''};\n'''

        return gen_str

    def fill_cFile(self):
        gen_str = ''
        for header in self.__headerList:
            gen_str += '''#include <%s>\n''' %(header)

        gen_str += '''\n'''
        gen_str += '''void pmu_drv_tool_customization_init(void)\n'''
        gen_str += '''{\n'''
        idx = 0

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if value.get_defEnable() != 0:
                gen_str += '''\t%s(%s,%d);\n''' %(self.__func, self.__paraList[idx], value.get_defEnable()-1)
            idx += 1
        gen_str += '''}\n'''

        return gen_str

class PmicObj_MT6758(PmicObj):
    def __init__(self):
        PmicObj.__init__(self)

    def parse(self, node):
        PmicObj.parse(self, node)

    def gen_files(self):
        PmicObj.gen_files(self)

    def gen_spec(self, para):
        PmicObj.gen_spec(self, para)

    def fill_dtsiFile(self):
        gen_str = ''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            gen_str += '''&mt_pmic_%s_ldo_reg {\n''' %(value.get_ldoName().lower())
            gen_str += '''\tregulator-name = \"%s\";\n''' %((value.get_ldoName().replace('_', '')).lower())
            gen_str += '''\tregulator-default-on = <%d>; /* 0:skip, 1: off, 2:on */\n''' %(value.get_defEnable())
            gen_str += '''\tstatus = \"okay\";\n'''
            gen_str += '''};\n'''

        gen_str += '''\n'''
        gen_str += '''&kd_camera_hw1 {\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            for varName in value.get_nameList():
            #for i in range(0, self.__appCount):
                bExisted = False
                postFix = ''
                #varName = value.get_nameList()[i]
                if varName.find('CAMERA') != -1:
                    postFix = varName[varName.rfind('_')+1:]
                    bExisted = True

                if varName.find('MAIN_CAMERA_3') != -1:
                    gen_str += '''\tvcam%s_main3-supply = <&mt_pmic_%s_ldo_reg>;\n''' %(postFix.lower(), value.get_ldoName().lower())
                elif varName.find('MAIN_CAMERA_2') != -1:
                    gen_str += '''\tvcam%s_main2-supply = <&mt_pmic_%s_ldo_reg>;\n''' %(postFix.lower(), value.get_ldoName().lower())
                elif varName.find('MAIN_CAMERA') != -1:
                    gen_str += '''\tvcam%s-supply = <&mt_pmic_%s_ldo_reg>;\n''' %(postFix.lower(), value.get_ldoName().lower())
                elif varName.find('SUB_CAMERA_2') != -1:
                    gen_str += '''\tvcam%s_sub2-supply = <&mt_pmic_%s_ldo_reg>;\n''' %(postFix.lower(), value.get_ldoName().lower())
                elif varName.find('SUB_CAMERA') != -1:
                    #gen_str += '''\tvcam%s_main2-supply = <&mt_pmic_%s_ldo_reg>;\n''' %(postFix.lower(), value.get_ldoName().lower())
                    gen_str += '''\tvcam%s_sub-supply = <&mt_pmic_%s_ldo_reg>;\n''' %(postFix.lower(), value.get_ldoName().lower())

            #if bExisted == True:
                #gen_str += '''\n'''

        gen_str += '''\tstatus = \"okay\";\n'''
        gen_str += '''};\n\n'''
        gen_str += '''&touch {\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            for name in value.get_nameList():
                if name.find('TOUCH') != -1:
                    gen_str += '''\tvtouch-supply = <&mt_pmic_%s_ldo_reg>;\n''' %(value.get_ldoName().lower())

        gen_str += '''\tstatus = \"okay\";\n'''
        gen_str += '''};\n'''

        return gen_str
