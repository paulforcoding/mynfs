// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012 Netapp, Inc. All rights reserved.
 */
#include <linux/module.h>
#include <linux/nfs_fs.h>
#include "internal.h"
#include "nfs3_fs.h"
#include "nfs.h"

struct nfs_subversion mynfs_v3 = {
	.owner = THIS_MODULE,
	.nfs_fs   = &mynfs_fs_type,
	.rpc_vers = &nfs_version3,
	.rpc_ops  = &nfs_v3_clientops,
	.sops     = &mynfs_sops,
#ifdef CONFIG_NFS_V3_ACL
	.xattr    = nfs3_xattr_handlers,
#endif
};

int __init mynfs_init_nfs_v3(void)
{
	mynfs_register_nfs_version(&mynfs_v3);
	return 0;
}

void __exit mynfs_exit_nfs_v3(void)
{
	mynfs_unregister_nfs_version(&mynfs_v3);
}

