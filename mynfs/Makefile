# MyNFS Kernel Module Makefile
# Builds a single loadable NFS client module (mynfs.ko)
# containing NFSv3 + NFSv4 support.

obj-m := mynfs.o

mynfs-y := client.o \
	   dir.o \
	   file.o \
	   getroot.o \
	   inode.o \
	   super.o \
	   io.o \
	   direct.o \
	   pagelist.o \
	   read.o \
	   symlink.o \
	   unlink.o \
	   write.o \
	   namespace.o \
	   mount_clnt.o \
	   export.o \
	   sysfs.o \
	   fs_context.o \
	   sysctl.o \
	   nfs3super.o \
	   nfs3client.o \
	   nfs3proc.o \
	   nfs3xdr.o \
	   nfs3acl.o \
	   nfs4proc.o \
	   nfs4xdr.o \
	   nfs4state.o \
	   nfs4renewd.o \
	   nfs4super.o \
	   nfs4file.o \
	   delegation.o \
	   nfs4idmap.o \
	   callback.o \
	   callback_xdr.o \
	   callback_proc.o \
	   nfs4namespace.o \
	   nfs4getroot.o \
	   nfs4client.o \
	   nfs4session.o \
	   dns_resolve.o \
	   cache_lib.o \
	   nfs4sysctl.o \
	   nfs42proc.o \
	   nfs42xattr.o \
	   fscache.o \
	   pnfs.o \
	   pnfs_dev.o \
	   pnfs_nfs.o \
	   mynfs_main.o

# Trace objects need -I$(src) so define_trace.h can re-include our headers
mynfs-y += nfstrace.o
mynfs-y += nfs4trace.o

ccflags-y := -DCONFIG_NFS_V3
ccflags-y += -DCONFIG_NFS_V4
ccflags-y += -DCONFIG_NFS_V4_1
ccflags-y += -DCONFIG_NFS_V4_2
ccflags-y += -DCONFIG_NFS_FSCACHE
CFLAGS_nfstrace.o  += -I$(src)
CFLAGS_nfs4trace.o += -I$(src)

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean
