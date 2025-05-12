#!/bin/bash
make_command=$1

function make_vfs_ko(){
    #make $MODULE_NAME LLVM=y
    make $MODULE_NAME
    if [ $? -eq 0 ]; then
        echo -e "\e[32mko file compile success\e[0m"
        #cp ./build/lib/modules/5.15.39/extra/f2fsj.ko ../f2fsj_build/f2fsj.ko
        #cp ./build/lib/modules/5.15.39/extra/f2fsj.ko /home/ytcui22/f2fsj/f2fsj/filebench/script/f2fsj.ko
        #cp ./build/lib/modules/5.15.39/extra/f2fsj.ko ../crash_test/f2fsj.ko
        sudo cp ./build/lib/modules/5.15.39/extra/f2fsj.ko /lib/modules/$(shell uname -r)/f2fsj.ko
    return 0

    else
        echo "make f2fsj.ko fail"
        return 1
    fi
}

function make_clean(){
    make clean
    if [ $? -eq 0 ]; then
        echo -e "\e[32mmake clean over \e[0m"
        return 0
    else
        echo "make clean happen exception"
        return 0
    fi
}

##############################
if [ $make_command == "f2fsj" ]; then
    make_vfs_ko
    if [ $? -ne 0 ]; then
        exit
    fi
    make_clean
    if [ $? -ne 0 ]; then
        exit
    fi
    echo -e "\e[32m***************************************\e[0m"
    echo -e "\e[32m*       make f2fsj succuss            *\e[0m"
    echo -e "\e[32m*       find in ../../f2fsj_build     *\e[0m"
    echo -e "\e[32m***************************************\e[0m"
    echo -e $(date)
elif [ $make_command == "-h" ]; then
    echo "./script/build.sh f2fsj, make f2fsj.ko"
fi
