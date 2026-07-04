# mynfs 模块加载依赖问题记录

## 现象

VM reboot 后，直接 `insmod mynfs.ko` 失败，报 18 个 "Unknown symbol"：

```
mynfs: Unknown symbol nlmclnt_proc (err -2)
mynfs: Unknown symbol __fscache_acquire_volume (err -2)
mynfs: Unknown symbol __fscache_begin_read_operation (err -2)
mynfs: Unknown symbol __fscache_unuse_cookie (err -2)
mynfs: Unknown symbol __fscache_relinquish_cookie (err -2)
mynfs: Unknown symbol nfsacl_decode (err -2)
mynfs: Unknown symbol netfs_readahead (err -2)
mynfs: Unknown symbol __fscache_relinquish_volume (err -2)
mynfs: Unknown symbol nfsacl_encode (err -2)
mynfs: Unknown symbol dns_query (err -2)
mynfs: Unknown symbol nlmclnt_rpc_clnt (err -2)
mynfs: Unknown symbol netfs_read_folio (err -2)
mynfs: Unknown symbol __fscache_invalidate (err -2)
mynfs: Unknown symbol __fscache_use_cookie (err -2)
mynfs: Unknown symbol nlmclnt_init (err -2)
mynfs: Unknown symbol nlmclnt_done (err -2)
mynfs: Unknown symbol netfs_subreq_terminated (err -2)
mynfs: Unknown symbol __fscache_acquire_cookie (err -2)
```

## 缺失的依赖模块

| 符号来源 | 模块 | 符号数 |
|----------|------|--------|
| nlmclnt_* | lockd | 4 |
| __fscache_* | fscache | 8 |
| nfsacl_* | nfs_acl | 2 |
| netfs_* | netfs | 3 |
| dns_query | dns_resolver | 1 |

## 临时解决办法

reboot 后需要逐个加载依赖：

```bash
modprobe grace
modprobe auth_rpcgss
modprobe lockd
modprobe nfs_acl
modprobe dns_resolver
modprobe netfs
modprobe fscache
insmod /root/MyNfs/mynfs/mynfs.ko
```

注意：`modprobe sunrpc lockd nfs_acl ...` 一次传多个模块名不生效，需逐个执行。

## 待调查

为什么 CentOS 内核自带的 `nfs` 模块不需要手动前置加载这些依赖？
可能原因：
1. 内核 nfs.ko 的 modinfo 里有 `depends:` 声明，modprobe 自动解析
2. 内核模块有 softdep 声明
3. mynfs.ko 缺少正确的依赖声明

## 复现环境

- 内核：5.14.0-719.el9.aarch64
- 模块：mynfs.ko v1.0（commit 2902556）
- 日期：2026-07-04
