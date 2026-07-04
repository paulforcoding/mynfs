# MyNFS 内核模块开发计划

## 项目概述

将 Linux 5.14.0-710.el9 内核的 NFS 客户端代码提取出来，重命名为 `mynfs`，编译为可加载的内核模块（.ko 文件）。

**目标**：通过 `insmod mynfs.ko` 加载独立的 NFS 客户端模块。

**开发环境**：
- 本地编辑：Mac (M5, 24GB)
- 编译运行：UTM VM (CentOS Stream 9, 3.5GB)
- 内核版本：5.14.0-719.el9.aarch64
- 同步方式：`vm-sync` 命令（已配置 alias）

## 已确定的决策

| 决策项 | 选择 | 说明 |
|--------|------|------|
| 模块结构 | **单模块** | mynfs.ko，包含 NFSv3 和 NFSv4 支持 |
| NFS 版本 | **NFSv3 + NFSv4** | 不包含 NFSv2，不包含 pNFS |
| 依赖处理 | **链接系统模块** | 使用系统已加载的 sunrpc.ko、lockd.ko，不复制代码 |
| 文件系统名 | **mynfs** | 挂载时使用 `mount -t mynfs` |
| 符号重命名 | **nfs_* → mynfs_*** | 所有导出符号重命名，避免冲突 |

---

## 一、源码结构

### 1.1 需要复制的文件

来源目录：`/Users/zp001/Documents/MyGitHub/kernsrc/srpm_extracted/linux-5.14.0-710.el9/fs/nfs/`

**NFS 核心文件**（必须）：
```
client.c       - NFS 客户端核心逻辑
dir.c          - 目录操作
file.c         - 文件操作
getroot.c      - 获取根文件系统
inode.c        - inode 管理
super.c        - 超级块操作
io.c           - I/O 操作
direct.c       - 直接 I/O
pagelist.c     - 页列表管理
read.c         - 读操作
symlink.c      - 符号链接
unlink.c       - 删除操作
write.c        - 写操作
namespace.c    - 命名空间管理
mount_clnt.c   - 挂载客户端
nfstrace.o     - 跟踪支持
export.c       - 导出支持
sysfs.c        - sysfs 接口
fs_context.c   - 文件系统上下文
```

**NFSv3 文件**（必须）：
```
nfs3super.c    - NFSv3 超级块
nfs3client.c   - NFSv3 客户端
nfs3proc.c     - NFSv3 过程
nfs3xdr.c      - NFSv3 XDR
nfs3acl.c      - NFSv3 ACL（可选）
```

**NFSv4 文件**（必须）：
```
nfs4proc.c         - NFSv4 过程
nfs4xdr.c          - NFSv4 XDR
nfs4state.c        - NFSv4 状态管理
nfs4renewd.c       - NFSv4 续期守护
nfs4super.c        - NFSv4 超级块
nfs4file.c         - NFSv4 文件操作
delegation.c       - 委派支持
nfs4idmap.c        - NFSv4 ID 映射
callback.c         - 回调处理
callback_xdr.c     - 回调 XDR
callback_proc.c    - 回调过程
nfs4namespace.c    - NFSv4 命名空间
nfs4getroot.c      - NFSv4 获取根
nfs4client.c       - NFSv4 客户端
nfs4session.c      - NFSv4 会话
dns_resolve.c      - DNS 解析
nfs4trace.c        - NFSv4 跟踪
cache_lib.c        - 缓存库
nfs4sysctl.c       - NFSv4 sysctl
```

**头文件**：
```
internal.h       - 内部头文件
nfs.h            - NFS 公共头文件
nfs3_fs.h        - NFSv3 内部头文件
callback.h       - 回调头文件
delegation.h     - 委派头文件
dns_resolve.h    - DNS 解析头文件
fscache.h        - fscache 头文件
iostat.h         - I/O 统计头文件
nfstrace.h       - 跟踪头文件
netns.h          - 网络命名空间头文件
```

### 1.2 外部依赖（链接系统模块）

| 依赖模块 | 系统模块文件 | 说明 |
|---------|--------------|------|
| sunrpc | /lib/modules/$(uname -r)/kernel/net/sunrpc/sunrpc.ko | RPC 层，必须 |
| lockd | /lib/modules/$(uname -r)/kernel/fs/lockd/lockd.ko | 文件锁，必须 |
| nfs_common | /lib/modules/$(uname -r)/kernel/fs/nfs_common/nfs_common.ko | NFS 公共代码 |
| crc32 | 内核内置 | 数据校验 |
| keys | 内核内置 | NFSv4 密钥管理 |
| dns_resolver | /lib/modules/$(uname -r)/kernel/net/dns_resolver/dns_resolver.ko | DNS 解析 |

**注意**：这些模块在目标系统上已存在，mynfs.ko 直接链接它们的导出符号，不需要复制源码。

---

## 二、重命名策略

### 2.1 符号重命名规则

为避免与内核内置 NFS 模块冲突，对所有符号进行重命名：

```
nfs_*      → mynfs_*
nfsv3_*    → mynfsv3_*
nfsv4_*    → mynfsv4_*
NFS_*      → MYNFS_*  (宏定义)
```

**重命名范围**：
- 函数名：`nfs_read_super` → `mynfs_read_super`
- 全局变量：`nfs_callback_info` → `mynfs_callback_info`
- 结构体：`struct nfs_server` → `struct mynfs_server`
- 宏定义：`NFS_FSBITS` → `MYNFS_FSBITS`

**工具**：使用 `sed` 批量替换脚本

### 2.2 文件系统类型重命名

```c
// 原始代码
static struct file_system_type nfs_fs_type = {
    .name = "nfs",
    ...
};

// 重命名后
static struct file_system_type mynfs_fs_type = {
    .name = "mynfs",  // 改为 "mynfs"
    ...
};
```

**挂载命令**：`mount -t mynfs server:/export /mnt`

---

## 三、编译系统

### 3.1 目录结构

```
MyNfs/
├── mynfs/                    # 模块源码目录
│   ├── Makefile             # Kbuild Makefile
│   ├── client.c             # 核心文件（重命名后）
│   ├── dir.c
│   ├── file.c
│   ├── ...                  # 其他 NFS 源文件
│   ├── internal.h           # 内部头文件（重命名后）
│   └── mynfs_main.c         # 模块入口点（新增）
├── include/                  # 头文件目录
│   └── linux/
│       ├── nfs.h           # NFS 协议定义
│       ├── nfs3.h          # NFSv3 定义
│       ├── nfs4.h          # NFSv4 定义
│       └── nfs_xdr.h       # XDR 定义
└── .aireports/             # 文档目录
    └── mynfs_dev_plan.md   # 本文档
```

### 3.2 Kbuild Makefile

```makefile
# MyNfs Kernel Module Makefile

# 目标模块名称
obj-m := mynfs.o

# 模块组成文件
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
           nfstrace.o \
           export.o \
           sysfs.o \
           fs_context.o \
           nfs3super.o \
           nfs3client.o \
           nfs3proc.o \
           nfs3xdr.o \
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
           nfs4trace.o \
           cache_lib.o \
           nfs4sysctl.o \
           mynfs_main.o

# 编译标志
ccflags-y := -I$(src)/../include
ccflags-y += -DCONFIG_NFS_V3
ccflags-y += -DCONFIG_NFS_V4
ccflags-y += -DCONFIG_NFS_V4_1
ccflags-y += -DCONFIG_NFS_V4_2

# 内核源码路径
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# 默认目标
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

.PHONY: all clean install
```

### 3.3 模块入口点

新增 `mynfs_main.c`：

```c
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

// 前向声明
extern int mynfs_init(void);
extern void mynfs_exit(void);

static int __init mynfs_module_init(void)
{
    pr_info("mynfs: loading NFS client module\n");
    return mynfs_init();
}

static void __exit mynfs_module_exit(void)
{
    pr_info("mynfs: unloading NFS client module\n");
    mynfs_exit();
}

module_init(mynfs_module_init);
module_exit(mynfs_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MyNFS Project");
MODULE_DESCRIPTION("Custom NFS Client Module");
MODULE_VERSION("1.0");
```

---

## 四、开发任务分解

### 阶段 1：环境准备（1 天）

**任务 1.1**：创建项目目录结构
```bash
mkdir -p mynfs include/linux
```

**任务 1.2**：复制内核源码
```bash
KERN_SRC=/Users/zp001/Documents/MyGitHub/kernsrc/srpm_extracted/linux-5.14.0-710.el9

# 复制 NFS 源文件（不包含 NFSv2 和 pNFS）
cp $KERN_SRC/fs/nfs/{client,dir,file,getroot,inode,super}.c mynfs/
cp $KERN_SRC/fs/nfs/{io,direct,pagelist,read,symlink,unlink,write}.c mynfs/
cp $KERN_SRC/fs/nfs/{namespace,mount_clnt,export,sysfs,fs_context}.c mynfs/
cp $KERN_SRC/fs/nfs/nfstrace.{c,h} mynfs/

# 复制 NFSv3 文件
cp $KERN_SRC/fs/nfs/nfs3{super,client,proc,xdr,acl}.c mynfs/
cp $KERN_SRC/fs/nfs/nfs3_fs.h mynfs/

# 复制 NFSv4 文件
cp $KERN_SRC/fs/nfs/nfs4{proc,xdr,state,renewd,super,file}.c mynfs/
cp $KERN_SRC/fs/nfs/{delegation,nfs4idmap,callback,callback_xdr,callback_proc}.c mynfs/
cp $KERN_SRC/fs/nfs/{nfs4namespace,nfs4getroot,nfs4client,nfs4session}.c mynfs/
cp $KERN_SRC/fs/nfs/{dns_resolve,nfs4trace,cache_lib,nfs4sysctl}.c mynfs/

# 复制头文件
cp $KERN_SRC/fs/nfs/{internal.h,nfs.h,callback.h,delegation.h}.h mynfs/
cp $KERN_SRC/fs/nfs/{dns_resolve.h,fscache.h,iostat.h,netns.h}.h mynfs/

# 复制 include 头文件
cp $KERN_SRC/include/linux/nfs*.h include/linux/
```

**任务 1.3**：创建基础 Makefile
```bash
# 使用上面 3.2 节的 Makefile 内容
```

### 阶段 2：符号重命名（2 天）

**任务 2.1**：编写重命名脚本
```bash
#!/bin/bash
# rename_symbols.sh

cd mynfs

# 重命名 nfs_* → mynfs_*
find . -name "*.c" -o -name "*.h" | xargs sed -i '' 's/\<nfs_/mynfs_/g'
find . -name "*.c" -o -name "*.h" | xargs sed -i '' 's/\<NFS_/MYNFS_/g'

# 重命名 nfsv3_* → mynfsv3_*
find . -name "*.c" -o -name "*.h" | xargs sed -i '' 's/\<nfsv3_/mynfsv3_/g'

# 重命名 nfsv4_* → mynfsv4_*
find . -name "*.c" -o -name "*.h" | xargs sed -i '' 's/\<nfsv4_/mynfsv4_/g'

# 特殊处理：文件系统类型名
sed -i '' 's/\.name = "nfs"/\.name = "mynfs"/g' super.c
```

**任务 2.2**：执行重命名
```bash
chmod +x rename_symbols.sh
./rename_symbols.sh
```

**任务 2.3**：手动修正
- 检查并修正注释中的符号名
- 修正字符串字面量
- 检查重命名冲突

### 阶段 3：模块入口点（1 天）

**任务 3.1**：创建 `mynfs_main.c`
```bash
# 使用上面 3.3 节的代码
```

**任务 3.2**：修改初始化函数
- 将 `init_nfs_fs()` 重命名为 `mynfs_init()`
- 将 `exit_nfs_fs()` 重命名为 `mynfs_exit()`
- 确保在 `super.c` 中导出这两个函数

### 阶段 4：编译测试（2-3 天）

**任务 4.1**：同步代码到 VM
```bash
vm-sync
```

**任务 4.2**：在 VM 中编译
```bash
ssh -p 2222 root@localhost
cd /root/MyNfs/mynfs
make
```

**任务 4.3**：修复编译错误
- 处理头文件缺失
- 修复 API 变化
- 解决符号冲突

**任务 4.4**：生成 mynfs.ko
```bash
modinfo mynfs.ko
```

### 阶段 5：功能测试（2-3 天）

**任务 5.1**：加载模块
```bash
insmod mynfs.ko
dmesg | tail
lsmod | grep mynfs
```

**任务 5.2**：挂载测试
```bash
# 需要先启动 NFS 服务器或使用现有服务器
mount -t mynfs server:/export /mnt
ls /mnt
```

**任务 5.3**：功能测试
```bash
# 文件读写
echo "test" > /mnt/test.txt
cat /mnt/test.txt

# 目录操作
mkdir /mnt/testdir
rmdir /mnt/testdir
```

**任务 5.4**：卸载测试
```bash
umount /mnt
rmmod mynfs
dmesg | tail
```

---

## 五、时间线

| 阶段 | 任务 | 时间 | 交付物 |
|------|------|------|--------|
| 1 | 环境准备 | 1 天 | 项目目录、源码、Makefile |
| 2 | 符号重命名 | 2 天 | 重命名后的源码 |
| 3 | 模块入口点 | 1 天 | mynfs_main.c |
| 4 | 编译测试 | 2-3 天 | mynfs.ko |
| 5 | 功能测试 | 2-3 天 | 测试报告 |

**总计**：8-10 天

---

## 六、常用命令

```bash
# 同步代码到 VM
vm-sync

# SSH 到 VM
ssh -p 2222 root@localhost

# 在 VM 中编译
cd /root/MyNfs/mynfs && make

# 加载模块
insmod mynfs.ko

# 卸载模块
rmmod mynfs

# 查看模块信息
modinfo mynfs.ko

# 查看内核日志
dmesg | tail

# 挂载 NFS
mount -t mynfs server:/export /mnt

# 卸载 NFS
umount /mnt
```

---

## 七、下一步行动

1. **立即开始阶段 1**：创建目录结构，复制源码
2. **执行阶段 2**：批量重命名符号
3. **完成阶段 3**：创建模块入口点
4. **进入阶段 4**：同步到 VM，编译测试
5. **第一个里程碑**：成功编译出 mynfs.ko
6. **第二个里程碑**：成功加载并挂载 NFS

---

**文档版本**：1.1
**创建日期**：2026-07-04
**最后更新**：2026-07-04
**状态**：可执行
