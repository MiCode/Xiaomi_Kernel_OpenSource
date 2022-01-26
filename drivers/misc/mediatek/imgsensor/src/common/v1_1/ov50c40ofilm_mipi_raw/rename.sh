#!/bin/bash
echo 请输入目录或文件名，当前目录直接回车
read fileordirectory
if [ -n fileordirectory ]
then fileordirectory=`pwd`
fi
echo 请输入改变前的字符串
read oldstr
echo 请输入改变后的字符串
read newstr
if [ -s fileordirectory ]
then
echo 文件或目录为空
exit 8
fi
recursive(){

    for fil in `ls $1`
    do
    file=$1'/'$fil
    if [ -d $file ]
    then
        newfile=`echo $file|sed "s/$oldstr/$newstr/g"`
        if [ $file != $newfile ]
        then  mv $file $newfile
        fi
        recursive $newfile
    else
        sed -i "s/$oldstr/$newstr/g" $file
        newfile=`echo $file|sed "s/$oldstr/$newstr/g"`
        if [ $file != $newfile ]
        then  mv $file $newfile
        fi
    fi
    done
}
recursive $fileordirectory
echo 替换完毕
