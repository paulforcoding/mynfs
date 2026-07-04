# UTM + CentOS Stream 9 (aarch64) qcow2 — 本机内核模块开发环境方案

> 状态:**待你审阅,确认前不动手、不下载、不安装。**
> 目标:在 Mac(Apple M5 / macOS 26)上用 UTM 起一台 CentOS Stream 9 arm64 VM,装好内核模块开发环境,把 Mac 目录实时共享进 VM;模块先在本地编 + insmod 跑通,同一份源码再到远端 `120.24.168.168`(5.14.0-474.el9.x86_64)重编 insmod。

---

## 0. 前置与资源预算

- 本机:macOS 26.5.1,Apple M5,24 GiB,10 核;可快速回收内存 ~9 GiB。给 VM 分 **4 GiB / 2–4 vCPU**,绰绰有余。
- 远端 `120.24.168.168`:CentOS Stream 9,内核 `5.14.0-474.el9.x86_64`,kernel-devel 已在 `/usr/src/kernels/5.14.0-474.el9.x86_64`。最终在它上面编 + 载,版本天然匹配。
- kABI:VM 内核会是当前 `~5.14.0-7xx.el9.aarch64`,与远端 474 同属 RHEL9 5.14 的 Z-stream、同 kABI 类——**源码跨边零改动**,各编各的、各载各的(原理见 §9)。
- 本地全量源码 `/Users/zp001/Documents/MyGitHub/kernsrc/srpm_extracted/linux-5.14.0-710.el9`:**仅作阅读/查代码**,模块编译不直接用它(认 `kernel-devel` 即可)。

## 1. 下载清单

| 文件 | URL | 大小 | 用途 |
|---|---|---|---|
| UTM 安装包 | https://github.com/utmapp/UTM/releases/latest/download/UTM.dmg | ~250MB | 虚拟机软件(开源免费) |
| CentOS Stream 9 qcow2 | https://cloud.centos.org/centos/9-stream/aarch64/images/CentOS-Stream-GenericCloud-9-latest.aarch64.qcow2 | 1.5G | 预装磁盘镜像(开机即用) |
| qcow2 校验 | …/CentOS-Stream-GenericCloud-9-latest.aarch64.qcow2.SHA256SUM | — | 完整性校验 |

> qcow2 是云镜像,首次启动靠 cloud-init 配置;§4 用种子 ISO 注入 root 密码 + SSH key + 预装包。

## 2. 装 UTM

1. 双击 `UTM.dmg`,把 `UTM.app` 拖进 `/Applications`。
2. 首次启动若提示"无法验证开发者":系统设置 → 隐私与安全性 → 点"仍要打开"。
3. 授予 UTM 网络/卷访问权限。不装任何额外驱动(用 macOS 自带 Hypervisor.framework);卸载即拖删,无残留。

## 3. 下载 qcow2 + 校验(Mac 终端)

```bash
mkdir -p ~/VMs/centos9 && cd ~/VMs/centos9
curl -fLO https://cloud.centos.org/centos/9-stream/aarch64/images/CentOS-Stream-GenericCloud-9-latest.aarch64.qcow2
curl -fLO https://cloud.centos.org/centos/9-stream/aarch64/images/CentOS-Stream-GenericCloud-9-latest.aarch64.qcow2.SHA256SUM
shasum -a 256 -c CentOS-Stream-GenericCloud-9-latest.aarch64.qcow2.SHA256SUM
```

## 4. 做 cloud-init 种子 ISO(命令已在本机实测通过 ✅)

NoCloud 数据源要求卷标为 `CIDATA`、内含 `user-data` + `meta-data`。用 macOS 自带 `hdiutil`:

`~/VMs/centos9/seed/user-data`:
```
#cloud-config
password: <改成你的root密码>
chpasswd: { expire: false }
ssh_pwauth: true
ssh_authorized_keys:
  - <粘你 ~/.ssh/id_ed25519.pub 的内容,没有就 ssh-keygen -t ed25519 生成>
package_update: true
packages:
  - make
  - gcc
  - kernel-devel
  - elfutils-libelf-devel
runcmd:
  - hostnamectl set-hostname cz-dev
```

`~/VMs/centos9/seed/meta-data`:
```
instance-id: cz-dev-01
local-hostname: cz-dev
```

打 ISO(实测命令,卷标 CIDATA 已验证):
```bash
hdiutil makehybrid -o ~/VMs/centos9/seed.iso \
  -iso -joliet -default-volume-name CIDATA ~/VMs/centos9/seed/
```
→ 产出 `~/VMs/centos9/seed.iso`。

## 5. 在 UTM 创建 VM

新建虚拟机,关键设置:

- **引擎:QEMU**(不是 Apple Virtualization)——VirtFS/9p 目录共享在 QEMU 引擎下有官方文档支持。
- **架构:ARM64 (aarch64)**,硬件加速(HVF)开启 → arm64-on-arm64 接近原生。
- **内存/CPU:4 GiB / 2–4 核。**
- **磁盘**:把下载的 qcow2 作为主磁盘导入(Import existing disk);建议先 `cp` 一份做副本再导入,原图留底。
- **CD/DVD**:挂载 `~/VMs/centos9/seed.iso`(NoCloud 种子)。
- **网络**:VirtIO-NAT;在 Network 高级里加端口转发 `host 2222 → guest 22`(方便 SSH)。
- **显示**:VGA/SPICE,留控制台窗口。

## 6. 首次启动 + 登录

1. 启动 VM,看控制台:cloud-init 读种子 ISO → 设 root 密码、注入 SSH key、装包(make/gcc/kernel-devel/elfutils-libelf-devel)。
2. 从 Mac 登录:
   ```bash
   ssh -p 2222 root@localhost
   # 或在 UTM 控制台直接 root + 你设的密码登录
   ```
3. 验证开发环境就绪:
   ```bash
   uname -r                                # 5.14.0-7xx.el9.aarch64
   ls -ld /lib/modules/$(uname -r)/build   # 应指向 kernel-devel 目录
   gcc --version; make --version
   ```

> **回退**:若 cloud-init 没认到种子(登不进),用 `rd.break` 重置 root 密码——开机在 GRUB 按 `e`,内核行末加 `rd.break`,启动后:
> ```bash
> mount -o remount,rw /sysroot && chroot /sysroot && passwd root && touch /.autorelabel && exit; exit
> ```
> 重启后 SELinux 自动重打标签再进(会多一次重启)。

## 7. 配置 VirtFS 共享 Mac 目录

1. UTM → 该 VM 设置 → **Sharing** → Directory Share Mode = **VirtFS**,Path 选你的模块源码目录(建议 `~/Documents/MyRepos/MyNfs/kmod`,先建好)。
2. VM 内挂载(CentOS9 内核自带 `9pnet_virtio`):
   ```bash
   modprobe 9pnet_virtio      # 一般已加载
   mkdir -p /mnt/share
   mount -t 9p -o trans=virtio,version=9p2000.L share /mnt/share
   ls /mnt/share              # 应看到 Mac 侧文件
   ```
   持久化(`/etc/fstab`):
   ```
   share /mnt/share 9p trans=virtio,version=9p2000.L,rw 0 0
   ```
> 注意:VirtFS 共享路径**开机后不能改**,要改先关机在设置里改。

## 8. 跑通一个内核模块(本地验证)

在 Mac 上 `/mnt/share` 对应目录建 `hello.c` + `Makefile`(VM 里 `/mnt/share` 同步可见):

`hello.c`:
```c
#include <linux/module.h>
#include <linux/init.h>
MODULE_LICENSE("GPL");
static int __init hello_init(void){ pr_info("hello: loaded\n"); return 0; }
static void __exit hello_exit(void){ pr_info("hello: unloaded\n"); }
module_init(hello_init);
module_exit(hello_exit);
```

`Makefile`(注意 Makefile 缩进用 Tab):
```make
obj-m := hello.o
KDIR ?= /lib/modules/$(shell uname -r)/build
all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
```

VM 内:
```bash
cd /mnt/share && make
insmod ./hello.ko && dmesg | tail -5 && lsmod | head; rmmod hello; dmesg | tail -3
```
看到 `hello: loaded` / `hello: unloaded` 即本地闭环跑通。

## 9. 拿到远端 120.24.168.168 重编 + 载

同一份源码(`hello.c` + `Makefile`),scp 到远端,用远端 474 的 kernel-devel 编、在远端载:
```bash
# 从 Mac
scp -r ~/Documents/MyRepos/MyNfs/kmod root@120.24.168.168:/root/kmod
ssh root@120.24.168.168 'cd /root/kmod && make && insmod ./hello.ko && dmesg | tail -3 && rmmod hello'
```
远端 `5.14.0-474.el9.x86_64` 的 kernel-devel 已在 `/usr/src/kernels/5.14.0-474.el9.x86_64`,vermagic 天然匹配,无需 `--force`。

> **kABI 原理**:本地 VM 内核 `~7xx.aarch64`、远端 `474.x86_64`,同 RHEL9 5.14 的 Z-stream、同 kABI 类。
> - **源码层**:同一份模块源码,两边都零改动可编(Z-stream 不改 API,kABI 锁了 ABI)——这就是本地 VM 内核不必非得是 474 的根据。
> - **二进制层**:`.ko` 的 `vermagic` 写死精确版本,只对本机内核有效,所以各编各载。你"本机跑通→远端重编能跑"正是此理,完全正确。

## 10. 资源与清理

- VM 只在启动时占内存;关掉即释放,UTM 本身**不跑常驻 daemon**。
- 不用时:UTM 里关 VM 即可。
- 彻底卸载:删 `~/VMs/centos9` + UTM 里删该 VM + 拖 `UTM.app` 到废纸篓。**无残留 daemon、无网桥、无 PATH 污染**。

## 11. 风险与回退

| 风险 | 回退 |
|---|---|
| cloud-init 没认种子、登不进 | §6 末尾 `rd.break` 重置 root 密码 |
| VirtFS 内核模块缺失 | `dnf install kernel-modules` 或临时改用 SPICE WebDAV(UTM Sharing 里换模式) |
| 不想搞 cloud-init 种子 ISO | 改下 `CentOS-Stream-9-latest-aarch64-boot.iso` 跑安装器(文本安装时设 root 密码),其余步骤不变 |
| qcow2 内核(~7xx)与本地 710 全量源码不符 | 模块开发只认 `kernel-devel`(自动匹配运行内核),不直接用全量源码树;710 全量源码仅作阅读 |

---

### 附录:核心结论速览

- **Apple container 走不通**:其 VM 内核 `# CONFIG_MODULES is not set`(官方 config 第 729 行),不能 insmod;且内核是主线 6.18.5,与 5.14 不同族。
- **UTM 是对的选**:干净(无 daemon/网桥)、可 VirtFS 实时共享 Mac 目录、arm64 原生速;VM 挂了免费重启。
- **内核版本不必追 474**:官方镜像只存最新 rolling build,474 那批已不存档;但 Z-stream 同 kABI,源码跨边零改动,各编各载即可。
