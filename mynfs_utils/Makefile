# Makefile for mynfs-utils
# ️ 仅在 VM(CentOS 9) 内执行 make install，禁止在 macOS 上运行 install 目标

CC = gcc
CFLAGS = -Wall -O2 -D_GNU_SOURCE
PREFIX = /sbin

all: mount.mynfs umount.mynfs

mount.mynfs: mount.mynfs.c nfs4_mount.h nfs_mount.h
	$(CC) $(CFLAGS) -o $@ $<

umount.mynfs: umount.mynfs.c
	$(CC) $(CFLAGS) -o $@ $<

# ⚠️ 仅在 VM(CentOS 9) 内执行，禁止在 macOS 上运行
install: mount.mynfs umount.mynfs
	install -m 4511 mount.mynfs $(PREFIX)/mount.mynfs
	ln -sf mount.mynfs $(PREFIX)/mount.mynfs4
	install -m 755 umount.mynfs $(PREFIX)/umount.mynfs
	ln -sf umount.mynfs $(PREFIX)/umount.mynfs4
	# Override umount.nfs4 to prevent conflict with real mount.nfs
	ln -sf umount.mynfs $(PREFIX)/umount.nfs4

clean:
	rm -f mount.mynfs umount.mynfs

.PHONY: all install clean
