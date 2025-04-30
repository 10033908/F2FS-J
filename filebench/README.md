file benchmark
===============

in ./script, there are test scripts for file benchmarks.
In these scripts, Firstly we need to run ./set_env.sh to insmod fsfs and btrfs, and close limitation for randomize_va_space
and then, run different scripts for file bench test, e.g: ./ext4_fb.sh -[param], param stands for different test case
Every test will be executed in a clean filesystem, so in test scripts, it will format device
================


in meta_data_only, support file benchmark for mkdir, rmdir, create empty file, unlink empty file, read dir

in data_and_meta_data, support file benchmark for create-4k, unlink-4k, copy-4k

ext4_dev is for file bench testing
================


some solutions for issues:

#set randomize_va_space, solution from https://github.com/filebench/filebench/issues/112
sudo -i
echo 0 > /proc/sys/kernel/randomize_va_space

#support very big filesets(more than 1M), solution from https://github.com/filebench/filebench/issues/90
modify FILEBENCH_NFILESETENTRIES to (1024 * 1024 * 10) and re-compile

#fsfs.ko and btrfs.ko is in /lib/moudle/(shell uname -r)/kernel/fs/, use modprobe f2fs / modprobe btrfs to load
#https://blog.csdn.net/JAZZSOLDIER/article/details/70053495

#get mkfs.f2fs, https://github.com/RiweiPan/F2FS-NOTES/blob/master/F2FS-Experiment/%E5%AE%9E%E9%AA%8C%E7%8E%AF%E5%A2%83%E6%90%AD%E5%BB%BA.md
sudo apt-get install f2fs-tools

#get mkfs.btrfs on ubuntu20.04, https://linuxhint.com/install-and-use-btrfs-on-ubuntu-lts/
sudo apt install btrfs-progs -y