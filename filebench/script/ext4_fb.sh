#!/bin/bash
dev_path=/home/test/f2fsj/test_dir/dev.img
mount_path=/ext4_mount_point

meta_bench_path=/home/test/f2fsj/filebench/meta_data_only/ext4_fb
meta_data_bench_path=/home/test/f2fsj/filebench/data_and_meta_data/ext4_fb
realwork_bench_path=/home/test/f2fsj/filebench/real_workloads/ext4_fb

test_case=$1

function format_fs(){
    sudo mkfs.ext4 $dev_path
    if [ $? -eq 0 ]; then
        echo -e "\e[32m format device with ext4 success\e[0m"
        return 0
    else
        echo "format device with ext4 fail"
        return 1
    fi
}

function mount_fs(){
    sudo mount -t ext4 -o loop $dev_path $mount_path
    if [ $? -eq 0 ]; then
        echo -e "\e[32m mount ext4 in $mount_path\e[0m"
        mount | grep ext4_mount
        return 0
    else
        echo "mount ext4 in $mount_path fail"
        return 1
    fi
}

function fs_test(){
    if [ $test_case == "create_empty" ]; then
        sudo filebench -f $meta_bench_path/micro_createfiles_empty.f
    elif [ $test_case == "unlink_empty" ]; then
        sudo filebench -f $meta_bench_path/micro_delete_empty.f
    elif [ $test_case == "mkdir" ]; then
        sudo filebench -f $meta_bench_path/micro_makedirs.f
    elif [ $test_case == "readdir" ]; then
        sudo filebench -f $meta_bench_path/micro_listdirs_empty.f
    elif [ $test_case == "rmdir" ]; then
        sudo filebench -f $meta_bench_path/micro_removedirs.f
    elif [ $test_case == "create_4k" ]; then
        sudo filebench -f $meta_data_bench_path/micro_createfiles.f
    elif [ $test_case == "create_32k" ]; then
        sudo filebench -f $meta_data_bench_path/micro_createfiles_32k.f
    elif [ $test_case == "copy_4k" ]; then
        sudo filebench -f $meta_data_bench_path/micro_copyfiles.f
    elif [ $test_case == "create_1g" ]; then
        sudo filebench -f $meta_data_bench_path/micro_createfiles_1g.f
    elif [ $test_case == "delete_4k" ]; then
        sudo filebench -f $meta_data_bench_path/micro_delete.f
    elif [ $test_case == "rread_64g" ]; then
        sudo filebench -f $meta_data_bench_path/filemicro_rread.f
    elif [ $test_case == "rwrite_64g" ]; then
        sudo filebench -f $meta_data_bench_path/filemicro_rwrite.f
    elif [ $test_case == "seqread_64g" ]; then
        sudo filebench -f $meta_data_bench_path/filemicro_seqread.f
    elif [ $test_case == "seqwrite_64g" ]; then
        sudo filebench -f $meta_data_bench_path/filemicro_seqwrite.f
    elif [ $test_case == "create_64g" ]; then
        sudo filebench -f $meta_data_bench_path/micro_createfiles_64g.f
    elif [ $test_case == "read_small" ]; then
        sudo filebench -f $meta_data_bench_path/filemicro_read_small_files.f
    elif [ $test_case == "varmail" ]; then # realistic workload
        sudo filebench -f $realwork_bench_path/real_varmail.f
    elif [ $test_case == "oltp" ]; then
        sudo filebench -f $realwork_bench_path/real_oltp.f
    elif [ $test_case == "fileserver" ]; then
        sudo filebench -f $realwork_bench_path/real_fileserver.f
    elif [ $test_case == "webproxy" ]; then
        sudo filebench -f $realwork_bench_path/real_webproxy.f
    elif [ $test_case == "webserver" ]; then
        sudo filebench -f $realwork_bench_path/real_webserver.f
    else
        echo "invalid test param, plz check"
    fi
}

function reset(){
    sudo umount $mount_path
    if [ $? -eq 0 ]; then
        echo -e "\e[32m umount ext4 in $mount_path\e[0m"
        return 0
    else
        echo "umount ext4 in $mount_path fail"
        return 1
    fi
}

function help_cmd(){
if [ $test_case == "-h" ]; then
    echo "---------------------------------------------"
    echo "META Only test cases:"
    echo "./xx_fb.sh create_empty, create empty files"
    echo "./xx_fb.sh unlink_empty, delete empty files"
    echo "./xx_fb.sh mkdir,        make directory"
    echo "./xx_fb.sh rmdir,        remove directory"
    echo "./xx_fb.sh readdir,      read directory"
    echo "---------------------------------------------"
    echo "DATA_And_META test cases:"
    echo "./xx_fb.sh create_4k,    create files with 4kb size"
    echo "./xx_fb.sh read_small    read 4M files(4k for each)"
    echo "./xx_fb.sh copy_4k,      copy files with 4kb size"
    echo "./xx_fb.sh create_1g,    create files with 1gb size"
    echo "./xx_fb.sh delete_4k,    delete files with 4kb size"
    echo "./xx_fb.sh rread_64g     random read files with 64g size"
    echo "./xx_fb.sh rwrite_64g    random write files with 64g size"
    echo "./xx_fb.sh seqread_64g   sequence read files with 64g size"
    echo "./xx_fb.sh seqwrite_64g  sequence write files with 64g size"
    echo "./xx_fb.sh create_64g    create a file with 64g size"
    echo "---------------------------------------------"
    echo "./xx_fb.sh varmail,      realistic workloads for varmail"
    echo "./xx_fb.sh oltp,         realistic workloads for oltp"
    echo "./xx_fb.sh fileserver,   realistic workloads for fileserver"
    echo "./xx_fb.sh webproxy,     realistic workloads for webproxy"
    echo "./xx_fb.sh webserver,    realistic workloads for webserver"
    echo "---------------------------------------------"
    return 0
else
    return 1
fi
}

## test steps
help_cmd
if [ $? -eq 0 ]; then
    exit
fi
echo -e "\e[32m Begin to filebench test\e[0m"

format_fs
mount_fs
echo ""

fs_test
reset

echo -e "\e[32m filebench test $test_case Over\e[0m"