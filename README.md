## How to run F2FS-J
F2FS-J is implemented based on clearlinux desktop with Linux kernel v5.15.39. Considering the kernel compatibility and the issues of toolchain dependencies, we provide two ways to build and run the platform.

### Ubuntu VM
- We provide a virtual machine environment of Ubuntu. Specifically, based on the Ubuntu 20.04.3 release version, we have replaced the kernel with version 5.15.39 and also completed the installation of the relevant toolchain. This virtual machine environment can be used for simple testing.
- Download Ubuntu VM from:
- Import VM with VMware workstation 16 pro
- Login with password: dji
- For testing, please refer ubuntu_guide.pdf

### ClearLinux
If you have a Clear Linux environment that is consistent with the experimental environment of the thesis, you need to pay attention to the following matters.
- Download filebench from: wget https://phoenixnap.dl.sourceforge.net/project/filebench/1.5-alpha3/filebench-1.5-alpha3.tar.gz
- uzip filebench: sudo tar -zxf filebench-1.5-alpha3.tar.gz -C /usr/local
- cd /usr/local
- modify FILEBENCH_NFILESETENTRIES (1024 * 1024) to (1024 * 1024 * 10) in ipc.h to make filebench support a large number of files.
- sudo ./configure; sudo make; sudo make install
Then, modify the paths of kernel source code and .o files involved in the compilation within a Makefile.
- For example, KDIR ?= /lib/modules/$(shell uname -r)/build
Compile
- cd ~/f2fsj/f2fsj; ./script/build f2fsj/f2fsj
Config storage device path and mount path at ~/f2fs/filebench/script/j_f2fs_fb.sh; (also need to modify ext4_fb.sh/f2fs_fb.sh/xfs_fb.sh)
Run
- cd ~/f2fs/filebench/script; sudo ./setup.sh; sudo ./j_f2fs_fb.sh create_4k (using -h to check other benchmarks)
