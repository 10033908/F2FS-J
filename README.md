## How to run F2FS-J
F2FS-J is implemented based on clearlinux desktop with Linux kernel v5.15.39. Considering the kernel compatibility and the issues of toolchain dependencies, we provide two ways to build and run the platform.

## Ubuntu VM
- We provide a virtual machine environment of Ubuntu. Specifically, based on the Ubuntu 20.04.3 release version, we have replaced the kernel with version 5.15.39 and also completed the installation of the relevant toolchain. This virtual machine environment can be used for simple testing.
- Please refer ubuntu_guide.pdf for using.

## ClearLinux
If you have a Clear Linux environment that is consistent with the experimental environment of the thesis, you need to pay attention to the following matters.

### Ensure filebench support a large number of files
- Download filebench from: wget https://phoenixnap.dl.sourceforge.net/project/filebench/1.5-alpha3/filebench-1.5-alpha3.tar.gz
- uzip filebench: sudo tar -zxf filebench-1.5-alpha3.tar.gz -C /usr/local
- cd /usr/local
- modify FILEBENCH_NFILESETENTRIES (1024 * 1024) to (1024 * 1024 * 10) in ipc.h
- sudo ./configure; sudo make; sudo make install

### Download F2FS-J repo
- cd [your-path]
- Download repo
	- For example, using command 'git clone https://github.com/10033908/F2FS-J.git' or Download .zip and unzip it to [your-path]

### Create a virtual storage device for testing
- cd [your-path]/F2FS-J
- mkdir test_dir && cd test_dir
- dd if=/dev/zero of=dev.img bs=1M count=16384

### Config compilation path 
- cd [your-path]/F2FS-J/f2fsj, then you can see a Makefile
- Check the default compilation path
 	- Run "uname -r" in shell, if the output is not 5.15.39
  	- Modify the value of KDIR in Makefile to '[your-5.15.39-linux-install-path]/build' , e.g., KDIR ?= /lib/modules/5.15.39/build
- cd [your-path]/F2FS-J/f2fsj/script, then you can see a build.sh
	- Again, if the output of 'uname -r' is not 5.15.39
		-  Modify '/lib/modules/$(shell uname -r)/f2fsj.ko' to '[your-5.15.39-linux-install-path]/f2fsj.ko'
			- E.g., '/lib/modules/5.15.39/f2fsj.ko'
		- This step ensure that Linux can find f2fsj.ko when loading f2fsj filesystem.

### Config filebench working path
- cd /
 - sudo mkdir j_f2fs_mount_point
 - sudo mkdir f2fs_mount_point
 - sudo mkdir ext4_mount_point
 - sudo mkdir xfs_mount_point


### Config filebench file paths in testing scripts
- For f2fsj filesystem
	- cd [your-path]/F2FS-J/filebench/script/j_f2fs_fb.sh
	- Modify following paths
		- dev_path=[your-path]/F2FS-J/test_dir/dev.img
		- meta_bench_path=[your-path]/F2FS-J/filebench/meta_data_only/j_f2fs_fb
		- meta_data_bench_path=[your-path]/F2FS-J/filebench/data_and_meta_data/j_f2fs_fb
		- realwork_bench_path=[your-path]/F2FS-J/filebench/real_workloads/j_f2fs_fb
- For f2fs filesystem
	- cd [your-path]/F2FS-J/filebench/script/f2fs_fb.sh
	- Modify following paths
		- dev_path=[your-path]/F2FS-J/test_dir/dev.img
		- meta_bench_path=[your-path]/F2FS-J/filebench/meta_data_only/f2fs_fb
		- meta_data_bench_path=[your-path]/F2FS-J/filebench/data_and_meta_data/f2fs_fb
		- realwork_bench_path=[your-path]/F2FS-J/filebench/real_workloads/f2fs_fb
- For ext4 filesystem
	- cd [your-path]/F2FS-J/filebench/script/ext4_fb.sh
	- Modify following paths
		- dev_path=[your-path]/F2FS-J/test_dir/dev.img
		- meta_bench_path=[your-path]/F2FS-J/filebench/meta_data_only/ext4_fb
		- meta_data_bench_path=[your-path]/F2FS-J/filebench/data_and_meta_data/ext4_fb
		- realwork_bench_path=[your-path]/F2FS-J/filebench/real_workloads/ext4_fb
- For xfs filesystem
	- cd [your-path]/F2FS-J/filebench/script/xfs_fb.sh
	- Modify following paths
		- dev_path=[your-path]/F2FS-J/test_dir/dev.img
		- meta_bench_path=[your-path]/F2FS-J/filebench/meta_data_only/xfs_fb
		- meta_data_bench_path=[your-path]/F2FS-J/filebench/data_and_meta_data/xfs_fb
		- realwork_bench_path=[your-path]/F2FS-J/filebench/real_workloads/xfs_fb

### Check if f2fs is insmod
- run 'cat /proc/filesystems | grep f2fs', if the output has 'f2fs'
	- cd [your-path]/F2FS-J/filebench/script/f2fs_fb.sh
	- Comment out the line `sudo modprobe f2fs`.  
- if the output does not have 'f2fs'
	- cd /lib/modules/5.15.39/kernel/fs/f2fs
	- sudo cp f2fs.ko /lib/modules/5.15.39/
		- This step ensure that Linux can find f2fs.ko when loading f2fs filesystem. 

### Check if xfs is insmod
- run 'cat /proc/filesystems | grep xfs', if the output has 'xfs'
	- cd [your-path]/F2FS-J/filebench/script/xfs_fb.sh
	- Comment out the line `sudo modprobe xfs`.  
- if the output does not have 'xfs'
	- cd /lib/modules/5.15.39/kernel/fs/xfs
	- sudo cp xfs.ko /lib/modules/5.15.39/
		- This step ensure that Linux can find xfs.ko when loading xfs filesystem. 

### Compile f2fs.ko
- Before compilation, please ensure that the kernel configuration, CONFIG_F2FS_IOSTAT is disabled.
- cd [your-path]/F2FS-J/f2fsj
- sudo ./script/build.sh f2fsj

### Run f2fsj filesystem by filebench scripts
- cd [your-path]/F2FS-J/filebench/script
- sudo ./setup.sh 
- sudo ./j_f2fs_fb.sh create_4k (using -h to check other benchmark commands)

### Potential runtime conflicts
- run 'mount | grep f2fs' to check if f2fs is already mounted, if yes, you need to modify some trace functions in [your-5.15.39-kernel-path]/trace/event/f2fs.h by error outputs (sudo dmesg -w in terminal)
- This is due to F2FSJ is implemented on top of F2FS, their kernel trace functions are identical. However, the Linux kernel does not support loading two modules with duplicate trace functions at runtime.
