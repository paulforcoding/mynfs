// SPDX-License-Identifier: GPL-2.0
/*
 * mynfs_main.c - MyNFS module entry point
 *
 * Registers the combined NFSv3 + NFSv4 client as a single loadable module.
 * Filesystem type: "mynfs" (mount with: mount -t mynfs ...)
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

/* Forward declarations - defined in inode.c, nfs4super.c, nfs3super.c */
extern int  __init init_nfs_fs(void);
extern void __exit exit_nfs_fs(void);
extern int  __init init_nfs_v4(void);
extern void __exit exit_nfs_v4(void);
extern int  __init init_nfs_v3(void);
extern void __exit exit_nfs_v3(void);

static int __init mynfs_module_init(void)
{
	int err;

	pr_info("mynfs: loading NFS client module (v3+v4)\n");

	err = init_nfs_fs();
	if (err)
		return err;

	err = init_nfs_v4();
	if (err)
		goto err_v4;

	err = init_nfs_v3();
	if (err)
		goto err_v3;

	pr_info("mynfs: module loaded\n");
	return 0;

err_v3:
	exit_nfs_v4();
err_v4:
	exit_nfs_fs();
	return err;
}

static void __exit mynfs_module_exit(void)
{
	pr_info("mynfs: unloading NFS client module\n");
	exit_nfs_v3();
	exit_nfs_v4();
	exit_nfs_fs();
}

module_init(mynfs_module_init);
module_exit(mynfs_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MyNFS Project");
MODULE_DESCRIPTION("Custom NFS Client Module (NFSv3 + NFSv4)");
MODULE_VERSION("1.0");
