# 开发环境

## 架构

```
Mac (M5, 24GB) ──SSH port 2222──→ UTM VM (CentOS Stream 9, 3.5GB)
     本地编辑代码                     rsync 到 VM 编译运行
```

## 规则

**本目录(`MyNfs/`)是唯一代码真相来源。** 所有编辑在本地完成,rsync 到 VM 编译测试。

## 同步

```bash
# Mac → VM (推送)
rsync -avz --delete -e "ssh -p 2222" ./ root@localhost:/root/MyNfs/

# VM → Mac (拉取,如需)
rsync -avz -e "ssh -p 2222" root@localhost:/root/MyNfs/ ./
```

建议加 alias:
```bash
alias vm-sync='rsync -avz --delete -e "ssh -p 2222" ./ root@localhost:/root/MyNfs/'
```

## VM 连接

```bash
ssh -p 2222 root@localhost
```

免密已配,端口转发走 UTM Emulated VLAN。

## 项目目标

开发 `mynfs` loadable kernel module。

- 源码:本地 `MyNfs/` 目录
- 编译:VM 内 `/root/MyNfs/`
- 内核版本:5.14.0-719.el9.aarch64
- 内核头:`/lib/modules/$(uname -r)/build/`

## 关键路径

| 本地 | VM |
|---|---|
| `~/Documents/MyRepos/MyNfs/` | `/root/MyNfs/` |
| 编辑 | 编译 + insmod/rmmod |

## 其他

- 剪贴板:已通(spice-vdagent)
- 文件共享:UTM VirtFS/SPICE WebDAV 均不可用(CentOS 9 内核无 9p 模块,无 spice-webdavd 包)
- GUI:未装,按需再搞
