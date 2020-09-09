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



# 1. VPP配置-`CLI`和`'startup.conf'`

成功安装后，VPP将在`/ etc / vpp /`目录中安装名为`startup.conf`的启动配置文件 。可以定制此文件以使VPP根据需要运行，但包含典型安装的默认值。

以下是有关此文件及其包含的一些参数和值的更多详细信息。

## 1.1. 命令行参数
在我们描述启动配置文件（startup.conf）的详细信息之前，应该提到可以在没有启动配置文件的情况下启动VPP。

参数按节名称分组。当为一个节提供多个参数时，该节的所有参数都必须用花括号括起来。例如，要通过命令行使用节名称为'unix'的配置数据启动VPP ：

```bash
$ sudo /usr/bin/vpp unix { interactive cli-listen 127.0.0.1:5002 }
```
命令行可以显示为单个字符串或多个字符串。在解析之前，命令行上给出的所有内容都用空格连接成单个字符串。VPP应用程序必须能够找到其自己的可执行映像。确保其工作的最简单方法是通过提供其绝对路径来调用VPP应用程序。例如：` '/ usr / bin / vpp <options>' `在启动时，VPP应用程序首先解析它们自己的ELF节，以创建init，配置和退出处理程序的列表。

使用VPP开发时，在gdb中通常足以启动这样的应用程序：

```
(gdb) run unix interactive
```

## 1.2. 启动配置文件（startup.conf）

将启动配置指定为VPP的更典型方法是使用启动配置文件（startup.conf）。

该文件的路径在命令行上提供给VPP应用程序。通常在`/etc/vpp/startup.conf`中。如果VPP作为软件包安装，则在此位置提供默认的startup.conf文件。

配置文件的格式是一个简单的文本文件，其内容与命令行相同。

一个非常简单的startup.conf文件：

```
$ cat /etc/vpp/startup.conf
unix {
  nodaemon
  log /var/log/vpp/vpp.log
  full-coredump
  cli-listen localhost:5002
}

api-trace {
  on
}

dpdk {
  dev 0000:03:00.0
}
```

指示VPP使用-c选项加载该文件。例如：
```
$ sudo /usr/bin/vpp -c /etc/vpp/startup.conf
```

## 1.3. 配置参数

以下是一些节名称及其相关参数的列表。这不是一个详尽的列表，但是应该使您了解如何配置VPP。

对于所有配置参数，请在源代码中搜索`VLIB_CONFIG_FUNCTION`和`VLIB_EARLY_CONFIG_FUNCTION`的实例 。

例如，调用“ `VLIB_CONFIG_FUNCTION（foo_config，“ foo”）`”将使函数“ `foo_config`”接收名为“ foo”的参数块中给出的所有参数：“ `foo {arg1 arg2 arg3…}`”。

## 1.4. Unix部分
配置VPP启动和行为类型属性，以及所有基于OS的属性。

```json
unix {
  nodaemon
  log /var/log/vpp/vpp.log
  full-coredump
  cli-listen /run/vpp/cli.sock
  gid vpp
}
```

## 1.5. Nodaemon
不要派生/后台vpp进程。从流程监视器调用VPP应用程序时，通常会出现这种情况。默认在默认的 “ `startup.conf`”文件中设置。

```
nodaemon
```

## 1.6. nosyslog
禁用syslog并将错误记录到stderr。从流程监视器（如runit或daemontools）调用VPP应用程序时，通常将其服务输出传递到专用日志服务，这通常会附加时间戳并根据需要旋转日志。

```
nosyslog
```

## 1.7. 互动
将CLI附加到`stdin / out`并提供调试命令行界面。

```
interactive
```

## 1.8. 日志<文件名>
以文件名记录启动配置和所有后续CLI命令。在人们不记得或不愿意在错误报告中包含CLI命令的情况下非常有用。默认的“ startup.conf”文件是写入“` /var/log/vpp/vpp.log`”的文件。

在VPP 18.04中，默认日志文件位置已从'`/tmp/vpp.log`'移至'`/var/log/vpp/vpp.log`'。VPP代码与文件位置无关。但是，如果启用了SELinux，则需要新位置才能正确标记文件。检查本地“ startup.conf”文件中系统上日志文件的位置。

```
log /var/log/vpp/vpp-debug.log
```

## 1.9. 执行| 启动配置<文件名>
从文件名读取启动操作配置。文件的内容将像在CLI上输入的那样执行。这两个关键字是同一功能的别名。如果两者都指定，则只有最后一个才会生效。

CLI命令文件可能类似于：
```
$ cat /usr/share/vpp/scripts/interface-up.txt
set interface state TenGigabitEthernet1/0/0 up
set interface state TenGigabitEthernet1/0/1 up
```

参数示例：
```
startup-config /usr/share/vpp/scripts/interface-up.txt
```

## 1.10. gid <number | 名称>
将有效组ID设置为呼叫进程的输入组ID或组名。

```
gid vpp
```

## 1.11. 全核心转储
要求Linux内核转储所有内存映射的地址区域，而不仅仅是text + data + bss。

```
full-coredump
```

## 1.12. 核心转储大小无限制| \<n> G | \<n> M | \<n> K | \<n>
设置coredump文件的最大大小。输入值可以以GB，MB，KB或字节设置，也可以设置为'unlimited'。

```
coredump-size unlimited
```

## 1.13. cli-listen <ipaddress：port> | <套接字路径>
绑定CLI以侦听TCP端口5002上的地址localhost。这将接受ipaddress：port对或文件系统路径；在后一种情况下，将打开本地Unix套接字。默认的“ startup.conf”文件是打开套接字“ `/run/vpp/cli.sock`”的文件。

```
cli-listen localhost:5002
cli-listen /run/vpp/cli.sock
```

## 1.14. 斜线模式
在stdin上禁用逐字符I / O。与emacs Mx gud-gdb结合使用时很有用。

```
cli-line-mode
```

## 1.15. cli-提示\<string>
将CLI提示符配置为字符串，就是命令行提示符。

```
cli-prompt vpp-2
```

## 1.16. cli-history-limit \<n>
将命令历史记录限制为<n>行。值为0将禁用命令历史记录。默认值：50

```
cli-history-limit 100
```
## 1.17. cli-banner
在stdin和Telnet连接上禁用登录横幅。

```
cli-no-banner
```


## 1.18. cli-pager
禁用输出寻呼机。

```
cli-no-pager
```

## 1.19. cli-pager-buffer-limit \<n>
将寻呼机缓冲区限制为输出的<n>行。值为0将禁用寻呼机。默认值：100000

```
cli-pager-buffer-limit 5000
```

## 1.20. runtime-dir <目录>
设置运行时目录，这是某些文件（例如套接字文件）的默认位置。默认值基于用于启动VPP的用户ID。通常是'root'，默认为'/ run / vpp /'。否则，默认为'/ run / user / <uid> / vpp /'。

```
runtime-dir /tmp/vpp
```

## 1.21. poll-sleep-usec \<n>
在主循环轮询之间添加固定睡眠。默认值为0，即不休眠。

```
poll-sleep-usec 100
```

## 1.22. pidfile <文件名>
将主线程的pid写入给定的文件名。

```
pidfile /run/vpp/vpp1.pid
```

## 1.23. api-trace部分
尝试了解控制平面已试图要求转发平面执行的操作时，**跟踪，转储和重播控制平面API跟踪的功能**使VPP用起来大为不同。

通常，只需启用API消息跟踪方案即可：

```
api-trace {
  api-trace on
}
```

### 1.23.1. on | enable
从开始就启用API跟踪捕获，并在应用程序异常终止时安排API跟踪的事后转储。默认情况下，（循环）跟踪缓冲区将配置为捕获256K跟踪。默认的“ startup.conf”文件默认情况下启用了跟踪，除非有很强的理由，否则应保持启用状态。

```
on
```


### 1.23.2. nitems \<n>
配置循环跟踪缓冲区以包含最后的<n>条目。默认情况下，跟踪缓冲区捕获最后收到的256K API消息。

```
nitems 524288
```

### 1.23.3. save-api-table <文件名>
将API消息表转储到/ tmp / <文件名>。

```
save-api-table apiTrace-07-04.txt
```

## 1.24. api段部分
这些值控制与VPP的二进制API接口的各个方面。

默认值如下所示：

```
api-segment {
  gid vpp
}
```

### 1.24.1. prefix \<path>
将前缀设置为用于共享内存（SHM）段的名称的前缀。默认值为空，这意味着共享内存段直接在SHM目录'/ dev / shm'中创建。值得注意的是，在许多系统上，“ / dev / shm”是指向文件系统中其他位置的符号链接。Ubuntu将其链接到'/ run / shm'。

```
prefix /run/shm
```
### 1.24.2. uid <number | name>
设置用于设置共享内存段所有权的用户ID或名称。默认为启动VPP的用户（可能是root）。

```
uid root
```
### 1.24.3. gid <number | name>
设置应用于设置共享内存段所有权的组ID或名称。默认为启动VPP的同一组，可能是root。

```
gid vpp
```

**以下参数仅应由熟悉VPP相互作用的人员设置。**

### 1.24.4. baseva \<x>
设置SVM全局区域的基地址。如果未设置，则在AArch64上，代码将尝试确定基址。所有其他默认值为0x30000000。

```
baseva 0x20000000
```
### 1.24.5. global-size \<n>G | \<n>M | \<n>
设置全局内存大小，所有路由器实例之间共享的内存，数据包缓冲区等。如果未设置，则默认为64M。输入值可以GB，MB或字节设置。

```
global-size 2G
```
### 1.24.6. global-pvt-heap-size \<n>M | size \<n>
设置全局VM专用堆的大小。如果未设置，则默认为128k。输入值可以以MB或字节为单位设置。

```
global-pvt-heap-size size 262144
```
### 1.24.7. api-pvt-heap-size \<n>M | size \<n>
设置api私有堆的大小。如果未设置，则默认为128k。输入值可以以MB或字节为单位设置。

```
api-pvt-heap-size 1M
```
### 1.24.8. api-size \<n>M | \<n>G | \<n>
设置API区域的大小。如果未设置，则默认为16M。输入值可以GB，MB或字节设置。

```
api-size 64M
```
## 1.25. socksvr部分
启用处理二进制API消息的Unix域套接字。参见`…/ vlibmemory / socket_api.c`。如果未设置此参数，则vpp不会通过套接字处理二进制API消息。

```
socksvr {
   # Explicitly name a socket file
   socket-name /run/vpp/api.sock
   or
   # Use defaults as described below
   default
}
```
“ default”关键字指示vpp在以root用户身份运行时使用`/run/vpp/api.sock`，否则将使用`/run/user/<uid>/api.sock`。

## 1.26. cpu部分
在VPP中，有一个主线程，并且用户可以选择创建工作线程。主线程和工作线程可以手动或自动固定到CPU内核。

```
cpu {
   main-core 1
   corelist-workers 2-3,18-19
}
```

**手动将线程固定到CPU内核**

### 1.26.1. main-core
设置运行主线程的逻辑CPU内核，如果未设置主内核，则VPP将使用内核1（如果有）

```
main-core 1
```
### 1.26.2. corelist-workers
设置运行工作线程的逻辑CPU内核

```
corelist-workers 2-3,18-19
```

**自动将线程固定到CPU内核**

### 1.26.3. skip-cores number
设置要跳过的CPU内核数（1…N-1），跳过的CPU内核不用于固定主线程和工作线程。

主线程自动固定到第一个可用的CPU内核，工作线程固定在分配给主线程的内核之后的下一个空闲CPU内核

```
skip-cores 4
```
### 1.26.4. workers number
指定要创建的工作线程数将工作线程固定到N个连续的CPU内核，同时跳过“跳过内核” CPU内核和主线程的CPU内核

```
workers 2
```
### 1.26.5. scheduler-policy other | batch | idle | fifo | rr
设置主线程和工作线程的调度策略和优先级

调度策略选项包括：other （`SCHED_OTHER`），batch （`SCHED_BATCH`）idle（`SCHED_IDLE`），fifo（`SCHED_FIFO`），rr（`SCHED_RR`）

```
scheduler-policy fifo
```
### 1.26.6. scheduler-priority number
调度优先级仅用于“实时策略（fifo和rr）”，并且必须在特定策略支持的优先级范围内

```
scheduler-priority 50
```

## 1.27. 缓冲区部分
```
buffers {
   buffers-per-numa 128000
   default data-size 2048
}
```


### 1.27.1. buffers-per-numa number
增加分配的缓冲区数，只有在具有大量接口和辅助线程的情况下才需要。值是每个numa节点。默认值为16384（如果未特权运行则为8192）

```
buffers-per-numa 128000
```
### 1.27.2. default data-size number
缓冲区数据区的大小，默认为2048

```
default data-size 2048
```


## 1.28. dpdk部分
```
dpdk {
   dev default {
      num-rx-desc 512
      num-tx-desc 512
   }

   dev 0000:02:00.1 {
      num-rx-queues 2
      name eth0
   }
}
```

### 1.28.1. dev \<pci-dev\> | default { .. }
将特定的PCI设备列入白名单（例如尝试驱动）。PCI-dev是形式为“ `DDDD：BB：SS.F`”的字符串，其中：

* DDDD =网域
* BB =总线号码
* SS =插槽号
* F =功能

如果使用关键字default，则这些值将应用于所有设备。

这与Linux sysfs树（即，`/ sys / bus / pci / devices`）中用于PCI设备目录名称的格式相同。

```
dpdk {
   dev default {
      num-rx-desc 512
      num-tx-desc 512
   }
```
### 1.28.2. dev \<pci-dev\> { .. }
通过指定PCI地址将特定接口列入白名单。通过指定PCI地址将特定接口列入白名单时，还可以指定其他自定义参数。有效选项包括：

```
dev 0000:02:00.0
dev 0000:03:00.0
```
### 1.28.3. blacklist \<pci-dev\>
通过指定PCI供应商将特定设备类型列入黑名单：设备白名单条目优先

```
blacklist 8086:10fb
```
### 1.28.4. name interface-name
设置接口名称

```
dev 0000:02:00.1 {
   name eth0
}
```
### 1.28.5. num-rx-queues \<n\>
接收队列数。还启用RSS。预设值为1。

```
dev 0000:02:00.1 {
   num-rx-queues <n>
}
```
### 1.28.6. num-tx-queues \<n\>
传输队列数。默认值等于工作线程数，如果没有工作线程，则默认为1。

```
dev 000:02:00.1 {
   num-tx-queues <n>
}
```
### 1.28.7. num-rx-desc \<n\>
接收环中描述符的数量。增加或减少数量可能会影响性能。默认值为1024。

```
dev 000:02:00.1 {
   num-rx-desc <n>
}
```
### 1.28.8. vlan-strip-offload on | off
接口的VLAN Strip卸载模式。默认情况下，使用ENIC驱动程序对除VIC以外的所有NIC的VLAN剥离均关闭，该驱动程序默认情况下已启用VLAN剥离。

```
dev 000:02:00.1 {
   vlan-strip-offload on|off
}
```
### 1.28.9. uio-driver driver-name
更改VPP使用的UIO驱动程序，选项为：`igb_uio`，`vfio-pci`，`uio_pci_generic`或`auto`（默认）

```
uio-driver vfio-pci
```
### 1.28.10. no-multi-seg
禁用多段缓冲区，提高性能，但禁用巨型MTU支持

```
no-multi-seg
```
### 1.28.11. socket-mem \<n\>
更改每个socket的大页内存分配，仅在需要大量mbuf时才需要。每个检测到的CPU插槽的默认值为256M

```
socket-mem 2048,2048
```
### 1.28.12. no-tx-checksum-offload
禁用UDP / TCP TX校验和卸载。通常需要使用更快的矢量PMD（以及非多段）

```
no-tx-checksum-offload
```
### 1.28.13. enable-tcp-udp-checksum
启用UDP / TCP TX校验和卸载这是'no-tx-checksum-offload'的反向选项

```
enable-tcp-udp-checksum
```
## 1.29. 插件部分
配置VPP插件。

```
plugins {
   path /ws/vpp/build-root/install-vpp-native/vpp/lib/vpp_plugins
   plugin dpdk_plugin.so enable
}
```
### 1.29.1. path pathname
根据VPP插件的位置调整插件路径。

```
path /ws/vpp/build-root/install-vpp-native/vpp/lib/vpp_plugins
```
### 1.29.2. plugin plugin-name | default enable | disable
默认情况下禁用所有插件，然后有选择地启用特定插件

```
plugin default disable
plugin dpdk_plugin.so enable
plugin acl_plugin.so enable
```
默认情况下启用所有插件，然后有选择地禁用特定插件

```
plugin dpdk_plugin.so disable
plugin acl_plugin.so disable
```


## 1.30. 统计部分
```
statseg {
   per-node-counters on
 }
```


## 1.31. socket-name \<filename>
统计段套接字的名称默认为`/run/vpp/stats.sock`。

```
socket-name /run/vpp/stats.sock
```
## 1.32. size <nnn>[KMG]
统计数据段的大小，默认为32mb

```
size 1024M
```
## 1.33. per-node-counters on | off
默认为无

`per-node-counters on`
## 1.34. update-interval \<f64-seconds>
设置片段抓取/更新间隔

```
update-interval 300
```

**一些高级参数：**

## 1.35. acl插件部分
这些参数更改**ACL（访问控制列表）**插件的配置，例如ACL双哈希表的初始化方式。

仅应由熟悉VPP和ACL插件相互作用的人员设置。

前三个参数（连接哈希存储桶，连接哈希内存和连接计数最大值）设置每个接口的连接表参数， 以修改IPv6的两个有界索引可扩展哈希表（40 * 8位密钥和8 * 8位）值对）和IPv4（16 * 8位密钥和8 * 8位值对）的ACL插件FA接口会话 被初始化。


### 1.35.1. connection hash buckets \<n>
设置两个双向哈希表中的每个哈希桶的数量（四舍五入为2的幂）。默认为64 * 1024（65536）哈希存储桶。

```
connection hash buckets 65536
```
### 1.35.2. connection hash memory \<n>
为两个双向哈希表中的每一个设置分配的内存大小（以字节为单位）。默认为1073741824字节

```
connection hash memory 1073741824
```
### 1.35.3. connection count max \<n>
为两个双哈希表分配每个工作人员的会话池时，设置池元素的最大数量。每个池中默认为500000个元素。

```
connection count max 500000
```
### 1.35.4. main heap size \<n>G | \<n>M | \<n>K | \<n>
设置保存所有ACL模块相关分配的主内存堆的大小（散列除外）。默认大小为0，但在ACL堆期间初始化等于 `per_worker_size_with_slack * tm-> n_vlib_mains + bihash_size + main_slack`。请注意，这些变量部分基于上面提到的 连接表每个接口参数。

```
main heap size 3G
```
接下来的三个参数，即哈希查找堆大小，哈希查找哈希桶和哈希查找哈希内存，将修改ACL插件使用的双向哈希查找表的初始化。尝试将ACL应用于在数据包处理期间查找的ACL的现有向量时，会初始化此表（但发现该表不存在/尚未初始化。）
### 1.35.5. hash lookup heap size \<n>G | \<n>M | \<n> K | \<n>
设置内存堆的大小，该内存堆保存与基于散列的查找有关的所有其他分配。默认大小为67108864字节。

```
hash lookup heap size 70M
```
### 1.35.6. hash lookup hash buckets \<n>
设置双向哈希表中的哈希桶数（四舍五入为2的幂）。默认为65536个哈希存储桶。

```
hash lookup hash buckets 65536
```
### 1.35.7. hash lookup hash memory \<n>
设置为双哈希查找表分配的内存大小（以字节为单位）。默认为67108864字节

```
hash lookup hash memory 67108864
```
### 1.35.8. use tuple merge \<n>
设置一个布尔值，该布尔值指示是否将TupleMerge用于哈希ACL。默认值为1（true），这意味着哈希ACL的默认实现确实使用TupleMerge。

```
use tuple merge 1
```
### 1.35.9. tuple merge split threshold \<n>
设置在将表拆分为两个新表之前，可以在双向哈希表中冲突的最大规则（ACE）。拆分可通过基于它们的公共元组（通常是它们的最大公共元组）对冲突规则进行散列来确保较少的规则冲突。当冲突规则向量的长度大于此阈值量时，就会发生拆分 。默认情况下，每个表最多39个规则冲突。

```
tuple merge split threshold 30
```
### 1.35.10. reclassify sessions \<n>
设置一个布尔值，该值指示在处理重新应用ACL或更改已应用的ACL时是否考虑会话的时期。默认值为0（false），这意味着默认实现不考虑会话的时期。

```
reclassify sessions 1
```
## 1.36. api-queue部分

### 1.36.1. length \<n>
设置api队列的长度。有效队列的最小长度为1024，这也是默认值。

```
length 2048
```

## 1.37. [cj Section](https://fd.io/docs/vpp/master/gettingstarted/users/configuring/startup.html#cj-section)

**`TODO`**

## 1.38. [dns Section](https://fd.io/docs/vpp/master/gettingstarted/users/configuring/startup.html#dns-section)

**`TODO`**


## 1.39. [ethernet Section](https://fd.io/docs/vpp/master/gettingstarted/users/configuring/startup.html#ethernet-section)

**`TODO`**

### 1.39.1. default-mtu \<n>
指定以太网接口的默认MTU大小。必须在64-9000范围内。默认值为9000。

```
default-mtu 1500
```

## 1.40. [heapsize Section](https://fd.io/docs/vpp/master/gettingstarted/users/configuring/startup.html#heapsize-section)

**`TODO`**

## 1.41. [ip Section](https://fd.io/docs/vpp/master/gettingstarted/users/configuring/startup.html#ip-section)

**`TODO`**

## 1.42. [ip6 Section](https://fd.io/docs/vpp/master/gettingstarted/users/configuring/startup.html#ip6-section)

**`TODO`**

## 1.43. vlib Section
这些参数配置VLIB，例如允许您选择是启用内存跟踪还是事后日志转储。

### 1.43.1. memory-trace
启用内存跟踪（mheap追溯）。默认值为0，表示禁用内存跟踪。

```
memory-trace
```
### 1.43.2. elog-events \<n>
设置事件环（事件的循环缓冲区）的元素/事件数（大小）。此数字舍入为2的幂。默认为131072（128 << 10）个元素。

```
elog-events 4096
```
### 1.43.3. elog-post-mortem-dump
启用尝试将事后日志转储到 / tmp / elog_post_mortem。如果调用了os_panic或os_exit，则<PID_OF_CALLING_PROCESS>。

```
elog-post-mortem-dump
```

## 1.44. **`TODO`**



<br/>
<div align=right>	以上内容由荣涛翻译整理自网络。</div>