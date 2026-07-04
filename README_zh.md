# MyNFS

可加载 NFS 客户端内核模块。从 Linux 5.14.0-710 内核提取 NFS 客户端代码，重命名符号（`nfs_*` → `mynfs_*`），编译为独立的 `mynfs.ko`。

- **目标内核**: CentOS Stream 9 — 5.14.0-719.el9.aarch64
- **支持版本**: NFSv3 + NFSv4（含 v4.1/v4.2）
- **挂载命令**: `mount -t mynfs server:/export /mnt`

---

## 安装

### 1. 编译模块

```bash
# 在目标机器上（CentOS Stream 9 aarch64）
cd mynfs
make
```

> 需要安装内核开发包：`dnf install kernel-devel kernel-headers make gcc`

### 2. 前置依赖

`mynfs.ko` 依赖以下内核模块：

| 模块 | 来源 | 用途 |
|------|------|------|
| `sunrpc` | 内核自带 | RPC 通信层 |
| `lockd` | 内核自带 | 文件锁 |
| `grace` | 内核自带 | 锁宽限期 |
| `nfs_acl` | 内核自带 | ACL 支持 |
| `fscache` | 内核自带 | 缓存层 |
| `netfs` | 内核自带 | 网络文件系统辅助 |
| `dns_resolver` | 内核自带 | DNS 解析 |
| `auth_rpcgss` | 内核自带 | RPC SEC GSS 认证 |

### 3. 加入 modules.dep（推荐）

`insmod` 不会自动加载依赖。建议将模块注册到内核模块数据库，这样 `modprobe mynfs` 会自动加载所有依赖：

```bash
# 创建模块目录
mkdir -p /lib/modules/$(uname -r)/extra/mynfs

# 复制模块
cp mynfs/mynfs.ko /lib/modules/$(uname -r)/extra/mynfs/

# 更新模块依赖数据库
depmod -a

# 现在可以用 modprobe 自动加载
modprobe mynfs

# 验证
lsmod | grep mynfs
```

如果不想修改系统目录，也可以手动逐个加载依赖：

```bash
modprobe sunrpc lockd grace nfs_acl fscache netfs dns_resolver auth_rpcgss
insmod mynfs/mynfs.ko
```

### 4. 安装挂载工具

```bash
# 编译并安装 mount.mynfs / umount.mynfs
cd mynfs_utils
make
make install    # 安装到 /sbin，仅在目标机器上执行
```

### 5. 挂载

```bash
mount -t mynfs 120.77.9.153:/export /mnt
```

---

## 目录结构

```
mynfs/           内核模块源码
mynfs_utils/     用户态挂载/卸载工具
```

## GitHub

https://github.com/paulforcoding/mynfs
