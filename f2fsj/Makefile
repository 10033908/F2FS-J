## Compile f2fsj from linux kernel 5.15.39

KBUILD_CFLAGS   += -w
MODULE_NAME = f2fsj
obj-m += $(MODULE_NAME).o

KDIR ?= /lib/modules/$(shell uname -r)/build

$(MODULE_NAME)-y		:= dir.o file.o inode.o namei.o hash.o super.o inline.o
$(MODULE_NAME)-y		+= checkpoint.o gc.o data.o node.o segment.o recovery.o
$(MODULE_NAME)-y		+= shrinker.o extent_cache.o sysfs.o
$(MODULE_NAME)-y        += j_log_operate.o j_epoch_commit.o j_checkpoint.o j_epoch.o j_journal_file.o j_recovery.o
$(MODULE_NAME)-y        += j_epoch_process.o
$(MODULE_NAME)-$(CONFIG_F2FS_STAT_FS) += debug.o
$(MODULE_NAME)-$(CONFIG_F2FS_FS_XATTR) += xattr.o
$(MODULE_NAME)-$(CONFIG_F2FS_FS_POSIX_ACL) += acl.o
$(MODULE_NAME)-$(CONFIG_FS_VERITY) += verity.o
$(MODULE_NAME)-$(CONFIG_F2FS_FS_COMPRESSION) += compress.o
$(MODULE_NAME)-$(CONFIG_F2FS_IOSTAT) += iostat.o

all:
	make -C $(KDIR) M=$(PWD) modules
	make -C $(KDIR) M=$(PWD) modules_install INSTALL_MOD_PATH=$(PWD)/build
clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f *~ $(PWD)/*.ur-safe

#f2fsj is in the ./build/lib/modules/5.15.39/extra/fsfsj.ko