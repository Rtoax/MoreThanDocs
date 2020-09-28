
<center><font size='20'>FD.io VPP环境下运行用户应用程序教程</font></center>
<br/>
<br/>
<center><font size='5'>荣涛</font></center>
<center><font size='5'>2020年9月</font></center>
<br/>


# 1. VPP简介

VPP是思科矢量数据包处理（VPP）技术的开源版本：一种高性能的数据包处理协议栈。具体介绍请阅读相关文档，此处不做VPP介绍相关内容。


# 2. VPP协议栈架构

DPDK用户态驱动接管10G网卡，VPP内嵌DPDK插件（plugin）组成一套完整的用户态网络协议栈。通过使用VCL库，VPP可以重定向应用程序的接口函数，当然，要通过LD_PRELOAD。

![](_v_images/20200922091936015_28143.png =600x)


# 3. DPDK配置
VPP我们选择的20.05版本，DPDK也选择了响应的20.05兼容版本。

## 3.1. 编译
>编译步骤一般在部署环境过程中已经完成，不用再次编译。

进入dpdk目录：
```bash
cd /opt/dpdk-20.05/
```
执行脚本：
```bash
./usertools/dpdk-setup.sh 
```
DPDK支持的架构越来越丰富，这里，我们选择[38]:
```
[34] x86_64-native-bsdapp-gcc
[35] x86_64-native-freebsd-clang
[36] x86_64-native-freebsd-gcc
[37] x86_64-native-linuxapp-clang
[38] x86_64-native-linuxapp-gcc
[39] x86_64-native-linuxapp-icc
[40] x86_64-native-linux-clang
[41] x86_64-native-linux-gcc
[42] x86_64-native-linux-icc
```

## 3.2. 设置环境变量

进入dpdk目录：
```bash
cd /opt/dpdk-20.05/
```

```bash
cat DPDKGNUenv.sh
```

内容如下：
```bash
#!/bin/bash
# <*! 添加 DPDK-20.05 环境变量> rongtao@sylincom.com

export RTE_SDK=/opt/dpdk-20.05
export RTE_TARGET=x86_64-native-linuxapp-gcc
```
环境变量我编写了对应20.05版本的脚本，直接执行source使其生效。

```bash
source DPDKGNUenv.sh
```
或者为了方便，将其加入`/etc/profile`中，使其登录生效，在`/etc/profile`（或者只在当前用户生效的`~/.bashrc`）中添加如下内容：
```bash
# 添加DPDK-20.05环境变量
source /opt/dpdk-20.05/DPDKGNUenv.sh
```


## 3.3. 加载驱动
进入dpdk目录：
```bash
cd /opt/dpdk-20.05/
```
执行脚本：
```bash
./usertools/dpdk-setup.sh 
```
有如下内容：
```
----------------------------------------------------------
 Step 2: Setup linux environment
----------------------------------------------------------
[45] Insert IGB UIO module
[46] Insert VFIO module
[47] Insert KNI module
[48] Setup hugepage mappings for non-NUMA systems
[49] Setup hugepage mappings for NUMA systems
[50] Display current Ethernet/Baseband/Crypto device settings
[51] Bind Ethernet/Baseband/Crypto device to IGB UIO module
[52] Bind Ethernet/Baseband/Crypto device to VFIO module
[53] Setup VFIO permissions
```
分别执行[45]、[46]、[47]，以[45]为例：
```
Option: 45

Unloading any existing DPDK UIO module
Loading uio module
Loading DPDK UIO module

Press enter to continue ...
```

## 3.4. 大页内存的配置
关于大页内存的配置（[48]、[49]），在部署环境过程中已经完成，此处不做介绍。

## 3.5. 网卡绑定
进入dpdk目录：
```bash
cd /opt/dpdk-20.05/
```
执行脚本：
```bash
./usertools/dpdk-setup.sh 
```
有如下内容：
```
----------------------------------------------------------
 Step 2: Setup linux environment
----------------------------------------------------------
[45] Insert IGB UIO module
[46] Insert VFIO module
[47] Insert KNI module
[48] Setup hugepage mappings for non-NUMA systems
[49] Setup hugepage mappings for NUMA systems
[50] Display current Ethernet/Baseband/Crypto device settings
[51] Bind Ethernet/Baseband/Crypto device to IGB UIO module
[52] Bind Ethernet/Baseband/Crypto device to VFIO module
[53] Setup VFIO permissions
```
这里我们使用igb_uio驱动[51]选项：
```
Option: 51


Network devices using kernel driver
===================================
0000:18:00.0 'NetXtreme BCM5720 Gigabit Ethernet PCIe 165f' if=em1 drv=tg3 unused=igb_uio *Active*
0000:18:00.1 'NetXtreme BCM5720 Gigabit Ethernet PCIe 165f' if=em2 drv=tg3 unused=igb_uio 
0000:19:00.0 'NetXtreme BCM5720 Gigabit Ethernet PCIe 165f' if=em3 drv=tg3 unused=igb_uio 
0000:19:00.1 'NetXtreme BCM5720 Gigabit Ethernet PCIe 165f' if=em4 drv=tg3 unused=igb_uio 

Other Network devices
=====================
0000:3b:00.0 'Ethernet 10G 2P X520 Adapter 154d' unused=ixgbe,igb_uio
0000:3b:00.1 'Ethernet 10G 2P X520 Adapter 154d' unused=ixgbe,igb_uio

No 'Baseband' devices detected
==============================

No 'Crypto' devices detected
============================

No 'Eventdev' devices detected
==============================

No 'Mempool' devices detected
=============================

No 'Compress' devices detected
==============================

No 'Misc (rawdev)' devices detected
===================================

Enter PCI address of device to bind to IGB UIO driver: 

```

根据网卡型号，服务器中安装了`Ethernet 10G 2P X520 Adapter 154d`网卡，所以绑定这两个网口即可：
```
Enter PCI address of device to bind to IGB UIO driver: 0000:3b:00.0
OK

Press enter to continue ...
```
同样的方式绑定另一个网口。

然后，再次输入选项[51]，如下：
```
Option: 51

Network devices using DPDK-compatible driver
============================================
0000:3b:00.0 'Ethernet 10G 2P X520 Adapter 154d' drv=igb_uio unused=ixgbe
0000:3b:00.1 'Ethernet 10G 2P X520 Adapter 154d' drv=igb_uio unused=ixgbe
```

证明网卡绑定成功。

>当然也可以使用`./usertools/dpdk-devbind.py`，查询、绑定、取消绑定网卡。
>To display current device status:
>       dpdk-devbind.py --status
>To display current network device status:
>        dpdk-devbind.py --status-dev net
>To bind eth1 from the current driver and move to use igb_uio
>        dpdk-devbind.py --bind=igb_uio eth1
>To unbind 0000:01:00.0 from using any driver
>        dpdk-devbind.py -u 0000:01:00.0
>To bind 0000:02:00.0 and 0000:02:00.1 to the ixgbe kernel driver
>        dpdk-devbind.py -b ixgbe 02:00.0 02:00.1

截至目前，VPP环境的DPDK设置已经完成。下面进行VPP的设置。

# 4. VPP的配置
因为我们采用VCL的方式使用VPP，并不是采用编写插件、GraphNode的形式，我们不需要添加修改VPP源代码。By the way，我们的VPP源码环境目录位于`/opt/vpp-stable-2005`目录下，对于VPP的编译安装，这里不做说明解释。

# 5. VPP启动配置文件startup.conf
>启动配置文件在部署环境过程中会作出对应的修改，基本上，不需要作出修改，但这里有必要对关键参数进行说明。
vpp的默认配置启动文件为`/etc/vpp/startup.conf`，当然，VPP也支持传入配置文件名，使用`-c`参数传入，例如：`vpp -c startup-myconf.conf`。下面就启动配置文件作出一些说明。

## 5.1. unix{}参数
先给出示例配置：
```
unix {
  nodaemon
  log /var/log/vpp/vpp.log
  full-coredump
  # cli-listen /run/vpp/cli.sock
  gid vpp
  coredump-size unlimited
  cli-listen localhost:5002
  cli-prompt RToax-VPP#
  cli-history-limit 100
  cli-pager-buffer-limit 5000
  pidfile /run/vpp/vpp.pid
  startup-config /usr/share/vpp/scripts/startup.txt
}
```

nodaemon：不以守护进程的形式运行VPP；
cli-listen：VPP命令行，`localhost:5002`为监听5002端口，用户可以使用`telnet  [host address] 5002`进行命令行接入；也可以设置成`cli-listen /run/vpp/cli.sock`，这是，可以使用`vppctl`接入命令行。

>VPP命令行在下文中会做简单的介绍。

值得注意的是`startup-config`选项，该选项会在VPP初始化结束后实行对应文件中的VPP命令行指令。*比如说，我们在`startup.conf`中指定了dpdk网卡、队列等，可以使用`startup-config`选项，在初始化后，执行网卡启动指令将网卡up。*
执行的vpp命令函脚本`/usr/share/vpp/scripts/startup.txt`内容简介。

设置VPP网卡IP
```
set interface ip address dpdk0 10.170.7.169/24
```
上面的网卡名`dpdk0`在启动配置文件`startup.conf`中命名。
启动VPP网卡
```
set interface state dpdk0 up
```
设置路由
```
ip route add 10.170.7.0/24 via 10.170.7.254
ip route add 10.170.6.0/24 via 10.170.7.254
ip route add 10.37.8.0/24 via 10.170.7.254
```
开启网卡混杂模式
```
set interface promiscuous on dpdk0
```

## 5.2. cpu{}参数
只介绍几个关键参数，详情请见相关文档。
```
cpu {
	main-core 1
	workers 2
	scheduler-policy fifo
	scheduler-priority 50
	thread-prefix vpp
}

```
设置主CPU，注意，这个CPU不能是其他线程绑定的，需要被VPP独占
```
main-core 1
```
设置从CPU，同样需要被VPP独占
```
workers 2
```

## 5.3. dpdk{}参数

```
dpdk {
	log-level debug
	dev 0000:3b:00.0 {
		name dpdk0
	}
	dev 0000:3b:00.1 {
		name dpdk1
	}
	no-multi-seg 
	dev default {
		num-rx-queues 2
		num-tx-queues 2
	}

	# igb_uio, vfio-pci 
	uio-driver igb_uio
}
```

设置总线地址为`0000:3b:00.0`的网卡名为`dpdk0`
```
name dpdk0
```

设置默认的接收发送队列个数
```
num-rx-queues 2
num-tx-queues 2
```
设置网卡的驱动程序类型，这里需要与上一章节中的`网卡绑定：igb_uio驱动[51]选项`相对应，
```
uio-driver igb_uio
```

# 6. VPP VCL配置文件vcl.conf

VPP VCL配置文件vcl.conf的默认路径为`/etc/vpp/vcl.conf`，这了当然也可以执行，但是我们不做处理。

>这部分内容不需要做出修改。

# 7. VPP的启动

## 7.1. systemctl的启动方式（不采用）
若vpp的启动配置文件均采用默认配置，使用`/etc/vpp/startup.conf`，VCL的配置文件使用`/etc/vpp/vcl.conf`命名，那么，我们可以使用`systemctl`来控制vpp的启动、重启、停止。当然，如果修改`vpp.service`文件的话，也可以修改使用systemctl启动vpp的默认配置文件，但为了保证尽可能少的修改vpp原始配置，这里我们不使用。

>如果采用systemctl启动的话，vpp遵从status、start、restart、stop、enable、disable等操作，但目前我们处于调试阶段，尚不再用这种方法，采用手动启动vpp软件。

## 7.2. 手动启动VPP（采用）

这里的配置文件名称我们命名为`startup-iperf3.conf`，其内容如下（`10.170.7.166`）：
```
## Filename:		startup-iperf3.conf
## Introduction:	VPP start up configure file
## File History:	
##	2020.09	rongtao		New
## 
unix { 
	nodaemon
	gid vpp 
	cli-listen localhost:5002
	startup-config /usr/share/vpp/scripts/startup.txt
}
session { evt_qs_memfd_seg  }
socksvr { socket-name /tmp/vpp-api.sock}
api-trace { on }
cpu {
	main-core 1
	workers 2
	scheduler-policy fifo
	scheduler-priority 50
	thread-prefix vpp
}

buffers {
	buffers-per-numa 128000
}

dpdk {
	log-level debug

	dev 0000:3b:00.0 {
		name dpdk0
	}
	dev 0000:3b:00.1 {
		name dpdk1
	}
	no-multi-seg 
	dev default {
		num-rx-queues 1
		num-tx-queues 1
		num-rx-desc 1024
	}
	# igb_uio, vfio-pci 
	uio-driver igb_uio

	# socket-mem 2048,2048

	no-tx-checksum-offload
}

plugins {
	## Adjusting the plugin path depending on where the VPP plugins are
	path /opt/vpp-stable-2005/build-root/install-vpp-native/vpp/lib/vpp_plugins

	## Disable all plugins by default and then selectively enable specific plugins
	 plugin default { disable }
	 plugin dpdk_plugin.so { enable }
	 plugin acl_plugin.so { enable }

	## Enable all plugins by default and then selectively disable specific plugins
	# plugin dpdk_plugin.so { disable }
	# plugin acl_plugin.so { disable }
 }
```
>文件`startup-iperf3.conf`不需要做出修改。

下面启动VPP软件：

```
vpp -c startup-iperf3.conf
```
输出内容如下：
```
[root@localhost vpp]# vpp -c startup-iperf3.conf 
vpp[33021]: vlib_sort_init_exit_functions:160: order constraint fcn 'dns_init' not found
vpp[33021]: vnet_feature_arc_init:271: feature node 'nat44-in2out-output' not found (before 'ip4-dvr-reinject', arc 
'ip4-output')vpp[33021]: dpdk: EAL init args: -c e -n 4 --in-memory --log-level debug --file-prefix vpp -w 0000:3b:00.0 -w 0000:3
b:00.1 --master-lcore 1 Thread 1 (vpp_wk_0):
  node dpdk-input:
    dpdk0 queue 0 (polling)
Thread 2 (vpp_wk_1):
  node dpdk-input:
    dpdk1 queue 0 (polling)
```

这时候可以进入VPP的命令行，打开一个终端，使用telnet命令连接命令行：
```
[root@localhost ~]# telnet 0 5002
Trying 0.0.0.0...
Connected to 0.
Escape character is '^]'.
    _______    _        _   _____  ___ 
 __/ __/ _ \  (_)__    | | / / _ \/ _ \
 _/ _// // / / / _ \   | |/ / ___/ ___/
 /_/ /____(_)_/\___/   |___/_/  /_/    

vpp# 
```
查看配置的dpdk网口信息状态
```
vpp# show interface address
dpdk0 (up):
  L3 10.170.7.169/24
dpdk1 (up):
local0 (dn):
```
查看网卡收发包情况
```
vpp# show interface 
              Name               Idx    State  MTU (L3/IP4/IP6/MPLS)     Counter          Count     
dpdk0                             1      up          9000/0/0/0     rx packets                   539
                                                                    rx bytes                   46170
                                                                    tx packets                     3
                                                                    tx bytes                     126
                                                                    drops                        485
                                                                    punt                          54
                                                                    ip4                           96
                                                                    ip6                           12
dpdk1                             2      up          9000/0/0/0     rx packets                   534
                                                                    rx bytes                   45754
                                                                    drops                        480
                                                                    punt                          54
                                                                    ip4                           93
                                                                    ip6                           12
local0                            0     down          0/0/0/0  
```

这时，VPP启动成功。

# 8. 基于VCL库运行用户应用程序

## 8.1. VPP VCL库简介

VCL库提供一种将可执行程序中的函数接口重定向至VPP中，整体架构如下：

![](_v_images/20200922110340388_3820.png =632x)

如上图可知，我们可以在不修改VPP代码和应用程序代码的基础上，在VPP环境下运行应用程序，目前已经测试成功的为iperf3应用程序在VPP下运行。

>基于VPP的iperf3灌包测试见下文。

## 8.2. 设置环境变量
进入VPP目录
```
cd /opt/vpp-stable-2005/
```
这里我为大家写好了一个配置环境变量的脚本，执行下面的命令：

```
source ld_preload.sh
```
这时，在此终端下运行的指令，只要有vcl支持的接口函数都会被重定向到VPP的VCL库中。

>接下来，你在这个终端（bash）下运行的所有指令，指令中相关的函数接口都将被重定向到VCL库中去。

# 9. 在VPP环境下的iperf3灌包测试

在以上的配置下来后，开启一个终端，最好是另一台服务器，并且与运行VPP的服务器网络通畅。

>在需要配置VPP路由表时，请配置路由表。

在另一台服务器上于小宁iperf3服务端（10.170.6.66）：
```
[root@localhost ~]# iperf3 -s 
-----------------------------------------------------------
Server listening on 5201
-----------------------------------------------------------
```

在部署、启动并设置了VCL环境变量的VPP的环境下（10.170.7.166），运行iperf3 UDP客户端进行灌包测试：

```
[root@localhost ~]# iperf3 --bind 10.170.7.169 -c 10.170.6.66 -b 30G -u
```
在服务端（10.170.6.66），速率为：
```
Accepted connection from 10.170.7.169, port 45251
[  5] local 10.170.6.66 port 5201 connected to 10.170.7.169 port 40774
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-1.01   sec  11.5 MBytes  95.2 Mbits/sec  0.875 ms  1409578/1417807 (99%)  
[  5]   1.01-2.01   sec  9.22 MBytes  77.7 Mbits/sec  0.768 ms  1434748/1441372 (1e+02%)  
[  5]   2.01-3.01   sec  9.15 MBytes  76.6 Mbits/sec  0.805 ms  1444889/1451464 (1e+02%)  
[  5]   3.01-4.00   sec  13.1 MBytes   110 Mbits/sec  0.644 ms  1433329/1442738 (99%)  
[  5]   4.00-5.01   sec  10.2 MBytes  85.1 Mbits/sec  0.898 ms  1445632/1452944 (99%)  
[  5]   5.01-6.01   sec  9.01 MBytes  75.6 Mbits/sec  0.687 ms  1446563/1453033 (1e+02%)  
[  5]   6.01-7.01   sec  11.1 MBytes  93.6 Mbits/sec  0.630 ms  1436834/1444831 (99%)  
[  5]   7.01-8.00   sec  11.6 MBytes  97.5 Mbits/sec  0.787 ms  1431028/1439352 (99%)  
[  5]   8.00-9.00   sec  10.7 MBytes  89.8 Mbits/sec  0.717 ms  1437719/1445388 (99%)  
[  5]   9.00-10.00  sec  15.3 MBytes   128 Mbits/sec  0.515 ms  1450464/1461435 (99%)  
[  5]  10.00-10.27  sec  1.34 MBytes  43.1 Mbits/sec  0.040 ms  22743/23707 (96%)  
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-10.27  sec   112 MBytes  91.6 Mbits/sec  0.040 ms  14393527/14474071 (99%)  receiver
```

在客户端（10.170.7.166），灌包速率为：
```
Connecting to host 10.170.6.66, port 5201
[ 33] local 10.170.7.169 port 40774 connected to 10.170.6.66 port 5201
[ ID] Interval           Transfer     Bandwidth       Total Datagrams
[ 33]   0.00-1.00   sec  1.97 GBytes  16.9 Gbits/sec  1447697  
[ 33]   1.00-2.00   sec  1.97 GBytes  16.9 Gbits/sec  1447854  
[ 33]   2.00-3.00   sec  1.97 GBytes  16.9 Gbits/sec  1448529  
[ 33]   3.00-4.00   sec  1.97 GBytes  16.9 Gbits/sec  1447631  
[ 33]   4.00-5.00   sec  1.97 GBytes  16.9 Gbits/sec  1448475  
[ 33]   5.00-6.00   sec  1.97 GBytes  16.9 Gbits/sec  1448486  
[ 33]   6.00-7.00   sec  1.97 GBytes  16.9 Gbits/sec  1447945  
[ 33]   7.00-8.00   sec  1.97 GBytes  16.9 Gbits/sec  1448170  
[ 33]   8.00-9.00   sec  1.97 GBytes  16.9 Gbits/sec  1447847  
[ 33]   9.00-10.00  sec  1.97 GBytes  16.9 Gbits/sec  1447870  
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bandwidth       Jitter    Lost/Total Datagrams
[ 33]   0.00-10.00  sec  19.7 GBytes  16.9 Gbits/sec  0.040 ms  14393527/14474071 (99%)  
[ 33] Sent 14474071 datagrams
```

后续，我又部署了一套vpp环境（10.37.8.37），进行联合测试的结果如下图：

![](_v_images/20200922135336624_7728.png =632x)


# 10. 在VPP环境下运行基站程序

这与上一节一样，将iperf3换成我们的应用程序即可。