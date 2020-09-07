<div align=center>
	<img src="_v_images/20200904171558212_22234.png" width="300"> 
</div>

<br/>
<br/>
<br/>

<center><font size='20'>FD.io VPP：用户文档</font></center>
<br/>
<br/>
<center><font size='5'>荣涛</font></center>
<center><font size='5'>2020年9月</font></center>
<br/>
<br/>
<br/>
<br/>



# 1. 配置大页内存

VPP需要巨大的页面才能在VPP操作期间运行，以管理大容量的内存。在VPP安装过程中，VPP将覆盖现有的大页面设置。默认情况下，VPP将系统上的大页面数设置为1024 2M个大页面。这是系统上不仅仅由VPP使用的大页面数。

安装VPP后，以下配置文件将复制到系统中。大页面设置适用于VPP安装和系统重新引导。要设置大页面设置，请执行以下命令：

```bash
$ cat /etc/sysctl.d/80-vpp.conf
# 4. Number of 2MB hugepages desired
vm.nr_hugepages=1024

# 5. Must be greater than or equal to (2 * vm.nr_hugepages).
vm.max_map_count=3096

# 6. All groups allowed to access hugepages
vm.hugetlb_shm_group=0

# 7. Shared Memory Max must be greater or equal to the total size of hugepages.
# 8. For 2MB pages, TotalHugepageSize = vm.nr_hugepages * 2 * 1024 * 1024
# 9. If the existing kernel.shmmax setting  (cat /sys/proc/kernel/shmmax)
# 10. is greater than the calculated TotalHugepageSize then set this parameter
# 11. to current shmmax value.
kernel.shmmax=2147483648
```

这部分，我在`10.170.7.166`上已经配置好了大页内存。

根据系统的使用方式，可以更新此配置文件以调整系统上保留的大页面数。以下是一些可能的设置示例。

**对于工作量最少的小型VM：**
```
vm.nr_hugepages=512
vm.max_map_count=2048
kernel.shmmax=1073741824
```
对于运行多个虚拟机的大型系统，每个虚拟机都需要自己的一组大页面：
```
vm.nr_hugepages=32768
vm.max_map_count=66560
kernel.shmmax=68719476736
```

* 注意：如果`VPP在虚拟机（VM）中运行`，则该VM必须具有巨大的页面支持。安装VPP后，它将尝试覆盖现有的大页面设置。如果虚拟机没有巨大的页面支持，则安装将失败，但是失败可能不会引起注意。重新引导VM后，在系统启动时， 将重新应用'vm.nr_hugepages'，并且将失败，并且VM将中止内核引导，从而锁定VM。为了避免这种情况，请确保VM具有足够的大页面支持。


# 2. 设置环境

***PS：Vagrant是一个基于Ruby的工具，用于创建和部署虚拟化开发环境.***

有关将`VPP与Virtual Box / Vagrant一​​起使用`的更多信息，请参阅[VM与Vagrant一​​起使用](https://fd.io/docs/vpp/master/reference/vppvagrant/index.html#vppvagrant)

<br/>

## 2.1. 安装Virtual Box和Vagrant

**`略`**

<br/>

## 2.2. 创建一个Vagrant目录

**`略`**

<br/>

## 2.3. 运行Vagrant

**`略`**

<br/>

## 2.4. 使用Vagrant设置VPP环境

**`略`**

<br/>

## 2.5. 安装VPP

这部分会单独出一份文档，这里简单介绍官网的安装方法，这种方法没有配置DPDK插件。
[CentOS7安装VPP](https://fd.io/docs/vpp/master/gettingstarted/installing/centos.html)
**`略`**

## 2.6. 创建一些启动文件

我们将创建一些启动文件供本教程使用。通常，您将修改`/etc/vpp/startup.conf`中的startup.conf文件。有关此文件的更多信息，请参考`VPP配置-CLI`和`'startup.conf'`。

在运行多个VPP实例时，每个实例都需要指定一个“名称”或“前缀”。在下面的示例中，“名称”或“前缀”为“ vpp1”。请注意，只有一个实例可以使用dpdk插件，因为该插件正在尝试获取文件的锁。我们创建的这些启动文件将禁用dpdk插件。

还要在我们的启动文件中注意`api-segment`。`api-segment {prefix vpp1}` 告诉FD.io VPP如何用不同于默认名称的方式为您的VPP实例命名`/ dev / shm /`中的文件。`unix {cli-listen /run/vpp/cli-vpp1.sock}` 告诉vpp在被`vppctl`寻址时使用非默认套接字文件。这里我也会单独做出配置文件`/etc/vpp/startup.conf`的详细说明（荣涛 [rongtao@sylincom.com](rongtao@sylincom.com)）。

现在，使用以下内容创建2个名为**startup1.conf**和**startup2.conf**的文件。这些文件可以位于任何地方。启动VPP时，我们指定位置。

**startup1.conf：**
```
unix {cli-listen /run/vpp/cli-vpp1.sock}
api-segment { prefix vpp1 }
plugins { plugin dpdk_plugin.so { disable } }
```

**startup2.conf：**

```
unix {cli-listen /run/vpp/cli-vpp2.sock}
api-segment { prefix vpp2 }
plugins { plugin dpdk_plugin.so { disable } }
```

<br/>


# 3. 运行VPP

使用我们在设置环境中创建的文件，我们现在将启动并运行VPP。

VPP在用户空间中运行。在生产环境中，通常会使用DPDK来运行它以连接到实际的NIC或使用vhost来连接到VM。在这种情况下，通常会运行一个VPP实例。

就本教程而言，运行多个VPP实例并将它们彼此连接以形成拓扑将非常有用。幸运的是，VPP支持这一点。

使用我们在安装程序中创建的文件，我们将启动VPP。

```
$ sudo /usr/bin/vpp -c startup1.conf
vlib_plugin_early_init:361: plugin path /usr/lib/vpp_plugins:/usr/lib/vpp_plugins
load_one_plugin:189: Loaded plugin: abf_plugin.so (ACL based Forwarding)
load_one_plugin:189: Loaded plugin: acl_plugin.so (Access Control Lists)
load_one_plugin:189: Loaded plugin: avf_plugin.so (Intel Adaptive Virtual Function (AVF) Device Plugin)
.........
$
```

**如果VPP无法启动**，则可以尝试将`nodaemon`添加到unix部分的`startup.conf`文件中 。

使用nodaemon的startup.conf示例：

```
unix {nodaemon cli-listen /run/vpp/cli-vpp1.sock}
api-segment { prefix vpp1 }
plugins { plugin dpdk_plugin.so { disable } }
```

命令`vppctl`将启动一个`VPP Shell`，您可以使用它以交互方式运行VPP命令。

现在，我们应该能够执行VPP Shell并显示版本了。

```
$ sudo vppctl -s /run/vpp/cli-vpp1.sock
    _______    _        _   _____  ___
 __/ __/ _ \  (_)__    | | / / _ \/ _ \
 _/ _// // / / / _ \   | |/ / ___/ ___/
 /_/ /____(_)_/\___/   |___/_/  /_/

vpp# show version
vpp v18.07-release built by root on c469eba2a593 at Mon Jul 30 23:27:03 UTC 2018
vpp#
```

![VPP的命令行截图](_v_images/20200904180458392_20546.png)

*注意：使用ctrl-d或q退出VPP Shell。*

**如果要运行多个VPP实例，请确保在完成后将其杀死**。

您可以使用如下所示的内容：

```bash
$ ps -eaf | grep vpp
root      2067     1  2 05:12 ?        00:00:00 /usr/bin/vpp -c startup1.conf
vagrant   2070   903  0 05:12 pts/0    00:00:00 grep --color=auto vpp
$ kill -9 2067
$ ps -eaf | grep vpp
vagrant   2074   903  0 05:13 pts/0    00:00:00 grep --color=auto vpp
```


<br/>




# 4. 运行VPP详述

## 4.1. 'vpp'用户组
安装VPP后，将创建一个新的用户组'vpp'。为避免以root用户身份运行VPP CLI（vppctl），请将需要与VPP进行交互的所有现有用户添加到新组中：

```
$ sudo usermod -a -G vpp user1
```
更新您的当前会话以使组更改生效：

```
$ newgrp vpp
```

## 4.2. VPP系统文件-'`vpp.service`'
安装VPP后，还将安装systemd服务文件。此文件vpp.service（Ubuntu：/lib/systemd/system/vpp.service和CentOS：/usr/lib/systemd/system/vpp.service）控制如何将VPP作为服务运行。例如，是否在发生故障时重新启动，如果有，则延迟多少时间。另外，应加载哪个UIO驱动程序以及“ startup.conf” 文件的位置。

```
$ cat /usr/lib/systemd/system/vpp.service
[Unit]
Description=Vector Packet Processing Process
After=syslog.target network.target auditd.service

[Service]
ExecStartPre=-/bin/rm -f /dev/shm/db /dev/shm/global_vm /dev/shm/vpe-api
ExecStartPre=-/sbin/modprobe uio_pci_generic
ExecStart=/usr/bin/vpp -c /etc/vpp/startup.conf
Type=simple
Restart=on-failure
RestartSec=5s

[Install]
WantedBy=multi-user.target
```

* 注意：某些“ `uio_pci_generic`”驱动程序的较旧版本无法正确绑定所有受支持的NIC，因此需要安装从`DPDK`构建的“ `igb_uio`”驱动程序。此文件控制在引导时加载哪个驱动程序。 'startup.conf'文件控制使用哪个驱动程序。

# 5. 运行VPP

构建VPP二进制文件后，您现在已构建了多个映像。当您需要运行VPP而不安装软件包时，这些映像很有用。例如，如果要与GDB一起运行VPP。

## 5.1. 在没有GDB的情况下运行
要运行没有GDB构建的VPP映像，请运行以下命令：

运行发布映像：

```
# 4. make run-release
#
```
运行调试映像：

```
# 4. make run
#
```

## 5.2. 与GDB一起运行
使用以下命令，您可以运行VPP，然后将其放入GDB提示符。

运行发布映像：

```
# 4. make debug-release
(gdb)
```
运行调试映像：

```
# 4. make debug
(gdb)
```

## 5.3. GDB范例
在本节中，我们有一些有用的gdb命令。

### 5.3.1. 启动GDB
在gdb提示符下，可以通过运行以下命令来启动VPP：

```
(gdb) run -c /etc/vpp/startup.conf
Starting program: /scratch/vpp-master/build-root/install-vpp_debug-native/vpp/bin/vpp -c /etc/vpp/startup.conf
 [Thread debugging using libthread_db enabled]
 Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
 vlib_plugin_early_init:361: plugin path /scratch/vpp-master/build-root/install-vpp_debug-native/vpp/lib/vpp_plugins:/scratch/vpp-master/build-root/install-vpp_debug-native/vpp/lib/vpp_plugins
 ....
```
### 5.3.2. 回溯
如果您在运行VPP时遇到错误，例如由于段错误或中止信号而导致VPP终止，则可以运行VPP调试二进制文件，然后执行backtrace或bt。

```
(gdb) bt
#0  ip4_icmp_input (vm=0x7ffff7b89a40 <vlib_global_main>, node=0x7fffb6bb6900, frame=0x7fffb6725ac0) at /scratch/vpp-master/build-data/../src/vnet/ip/icmp4.c:187
#1  0x00007ffff78da4be in dispatch_node (vm=0x7ffff7b89a40 <vlib_global_main>, node=0x7fffb6bb    6900, type=VLIB_NODE_TYPE_INTERNAL, dispatch_state=VLIB_NODE_STATE_POLLING, frame=0x7fffb6725ac0, last_time_stamp=10581236529    65565) at /scratch/vpp-master/build-data/../src/vlib/main.c:988
#2  0x00007ffff78daa77 in dispatch_pending_node (vm=0x7ffff7b89a40 <vlib_global_main>, pending_frame_index=6, last_time_stamp=1058123652965565) at /scratch/vpp-master/build-data/../src/vlib/main.c:1138
....
```

### 5.3.3. 进入GDB提示
当VPP运行时，您可以通过按CTRL + C进入命令提示符。

### 5.3.4. 断点
在GDB提示符下，通过运行以下命令来设置断点：

```
(gdb) break ip4_icmp_input
Breakpoint 4 at 0x7ffff6b9c00b: file /scratch/vpp-master/build-data/../src/vnet/ip/icmp4.c, line 142.
```
列出已经设置的断点：
```
(gdb) i b
Num     Type           Disp Enb Address            What
1       breakpoint     keep y   0x00007ffff6b9c00b in ip4_icmp_input at /scratch/vpp-master/build-data/../src/vnet/ip/icmp4.c:142
    breakpoint already hit 3 times
2       breakpoint     keep y   0x00007ffff6b9c00b in ip4_icmp_input at /scratch/vpp-master/build-data/../src/vnet/ip/icmp4.c:142
3       breakpoint     keep y   0x00007ffff640f646 in tw_timer_expire_timers_internal_1t_3w_1024sl_ov
    at /scratch/vpp-master/build-data/../src/vppinfra/tw_timer_template.c:775
```

### 5.3.5. 删除断点
```
(gdb) del 2
(gdb) i b
Num     Type           Disp Enb Address            What
1       breakpoint     keep y   0x00007ffff6b9c00b in ip4_icmp_input at /scratch/vpp-master/build-data/../src/vnet/ip/icmp4.c:142
    breakpoint already hit 3 times
3       breakpoint     keep y   0x00007ffff640f646 in tw_timer_expire_timers_internal_1t_3w_1024sl_ov
    at /scratch/vpp-master/build-data/../src/vppinfra/tw_timer_template.c:775
```

### 5.3.6. Step/Next/List
使用（s）tep into，（n）ext逐步遍历代码，并在list所在位置的前后列出一些行。

```
Thread 1 "vpp_main" hit Breakpoint 1, ip4_icmp_input (vm=0x7ffff7b89a40 <vlib_global_main>, node=0x7fffb6bb6900, frame=0x7fffb6709480)
    at /scratch/jdenisco/vpp-master/build-data/../src/vnet/ip/icmp4.c:142
142 {
(gdb) n
143   icmp4_main_t *im = &icmp4_main;
(
(gdb) list
202       vlib_put_next_frame (vm, node, next, n_left_to_next);
203     }
204
205   return frame->n_vectors;
206 }
207
208 /* *INDENT-OFF* */
209 VLIB_REGISTER_NODE (ip4_icmp_input_node,static) = {
210   .function = ip4_icmp_input,
211   .name = "ip4-icmp-input",
```

### 5.3.7. 检查数据和数据包
要查看数据和数据包，请使用e（x）胺或（p）rint。

例如，在此代码中查看ip数据包：

```
(gdb) p/x *ip0
$3 = {{ip_version_and_header_length = 0x45, tos = 0x0, length = 0x5400,
fragment_id = 0x7049, flags_and_fragment_offset = 0x40, ttl = 0x40, protocol = 0x1,
checksum = 0x2ddd, {{src_address = {data = {0xa, 0x0, 0x0, 0x2},
data_u32 = 0x200000a, as_u8 = {0xa, 0x0, 0x0, 0x2}, as_u16 = {0xa, 0x200},
as_u32 = 0x200000a}, dst_address = {data = {0xa, 0x0, 0x0, 0xa}, data_u32 = 0xa00000a,
as_u8 = {0xa, 0x0, 0x0, 0xa}, as_u16 = {0xa, 0xa00},  as_u32 = 0xa00000a}},
address_pair = {src = {data = {0xa, 0x0, 0x0, 0x2}, data_u32 = 0x200000a,
as_u8 = {0xa, 0x0, 0x0, 0x2}, as_u16 = {0xa, 0x200},  as_u32 = 0x200000a},
dst = {data = {0xa, 0x0, 0x0, 0xa}, data_u32 = 0xa00000a, as_u8 = {0xa, 0x0, 0x0, 0xa},
as_u16 = {0xa, 0xa00}, as_u32 = 0xa00000a}}}}, {checksum_data_64 =
{0x40704954000045, 0x200000a2ddd0140}, checksum_data_64_32 = {0xa00000a}},
{checksum_data_32 = {0x54000045, 0x407049, 0x2ddd0140, 0x200000a, 0xa00000a}}}
```
然后是icmp标头

```
(gdb) p/x *icmp0
$4 = {type = 0x8, code = 0x0, checksum = 0xf148}
```
然后查看实际字节：

```
(gdb) x/50w ip0
0x7fde9953510e:     0x54000045      0x00407049      0x2ddd0140      0x0200000a
0x7fde9953511e:     0x0a00000a      0xf1480008      0x03000554      0x5b6b2e8a
0x7fde9953512e:     0x00000000      0x000ca99a      0x00000000      0x13121110
0x7fde9953513e:     0x17161514      0x1b1a1918      0x1f1e1d1c    
```
