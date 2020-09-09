<div align=center>
	<img src="_v_images/20200904171558212_22234.png" width="300"> 
</div>

<br/>
<br/>
<br/>

<center><font size='20'>FD.io VPP：用户文档 iperf3灌包测试</font></center>
<br/>
<br/>
<center><font size='5'>荣涛</font></center>
<center><font size='5'>2020年9月</font></center>
<br/>
<br/>
<br/>
<br/>


# 1. VPP环境配置与启动
<br/>

## 1.1. 安装VPP环境

`略`

<br/>

## 1.2. VPP配置文件
启动配置文件startup-iperf3.conf
```config
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
}
```

<br/>

## 1.3. VPP的初始化命令行脚本

为VPP自动化配置考虑，需要在VPP启动配置文件中配置`startup-config`参数，在上小节中的`startup-config`配置为`/usr/share/vpp/scripts/startup.txt`。
`/usr/share/vpp/scripts/startup.txt`文件如下：

```config
comment {
	文件描述：该文件用于配置VPP网络，具体配置根据实际情况而定
	作者：荣涛
	时间：2020年9月2日
	网址：https://docs.fd.io/vpp/20.05/d5/d57/clicmd.html
}

comment { 设置网卡IP地址 }
set interface ip address dpdk0 10.170.7.169/24 
comment {
set interface ip address dpdk1 10.170.6.169/24
}

comment { 设置网卡MAC地址 }
comment {
set interface mac address dpdk0 xx:xx:xx:xx:xx:xx
}

comment { 设置MTU-最大传输单元 }
comment {
set interface mtu [packet|ip4|ip6|mpls] value interface
}

comment { 设置接收模式 }
comment {
set interface rx-mode interface [queue n] [polling | interrupt | adaptive]
}
show interface rx-placement

comment { 设置队列worker }
comment {
set interface rx-placement interface queue 1 worker 0
}

comment { 启动网卡 [up|down|punt|enable] }
set interface state dpdk0 up
set interface state dpdk1 up

comment { 设置路由，根据实际情况决定 }
ip route add 10.170.6.0/24 via 10.170.7.254

comment { 开启混杂模式  }
set interface promiscuous on dpdk0

comment { 启动local0口 }
comment {
set interface state local0 up
}

comment { 创建环回地址 }
comment {
create loopback interface
set interface ip address loop0 127.0.0.1/8
set interface state loop0 up
}

comment { DPDK相关配置、查询 }
comment {
show dpdk buffer
show dpdk physmem
show dpdk version
}

comment { DPDK内存测试 }
comment {
test dpdk buffer allocate 1024
test dpdk buffer
test dpdk buffer free 1024
}

comment { 显示有用信息 }
comment { show version }
comment { 查看所有插件 }
comment {
show plugins
}

comment { 抓包 }
comment {
pcap trace status
pcap trace tx max 35 intfc dpdk0 file dpdk0.pcap
pcap trace status
pcap trace off
}

```

<br/>

## 1.4. VPP启动
启动很简单，我们给vpp传入配置文件`startup-iperf3.conf`

```
# vpp -c startup-iperf3.conf

一下内容为输出打印，报错部分先忽略。
vpp[19861]: vlib_sort_init_exit_functions:160: order constraint fcn 'dns_init' not f
oundvpp[19861]: vnet_feature_arc_init:271: feature node 'nat44-in2out-output' not found 
(before 'ip4-dvr-reinject', arc 'ip4-output')vpp[19861]: dpdk: EAL init args: -c e -n 4 --in-memory --log-level debug --file-pref
ix vpp -w 0000:3b:00.0 -w 0000:3b:00.1 --master-lcore 1 Thread 1 (vpp_wk_0):
  node dpdk-input:
    dpdk0 queue 0 (polling)
Thread 2 (vpp_wk_1):
  node dpdk-input:
    dpdk1 queue 0 (polling)
```

<br/>

我们用telnet命令进入该vpp实例的命令行
```
# telnet 0 5002
Trying 0.0.0.0...
Connected to 0.
Escape character is '^]'.
    _______    _        _   _____  ___ 
 __/ __/ _ \  (_)__    | | / / _ \/ _ \
 _/ _// // / / / _ \   | |/ / ___/ ___/
 /_/ /____(_)_/\___/   |___/_/  /_/    

vpp# 
```

<br/>

查看网口信息

```
vpp# show interface 
              Name               Idx    State  MTU (L3/IP4/IP6/MPLS)     Counter          Count     
dpdk0                             1      up          9000/0/0/0     rx packets                   342
                                                                    rx bytes                   35543
                                                                    tx packets                     3
                                                                    tx bytes                     126
                                                                    drops                        282
                                                                    punt                          60
                                                                    ip4                           18
                                                                    ip6                           24
dpdk1                             2      up          9000/0/0/0     rx packets                   339
                                                                    rx bytes                   35295
                                                                    drops                        280
                                                                    punt                          59
                                                                    ip4                           16
                                                                    ip6                           24
local0                            0     down          0/0/0/0       
vpp# 
```

查看网口IP地址

```
vpp# show interface address
dpdk0 (up):
  L3 10.170.7.169/24
dpdk1 (up):
local0 (dn):
```

这样我们就确定了，`10.170.7.169/24`即为我们配置过的VPP网卡。

<br/>

# 2. UDP灌包测试

<br/>

## 2.1. ACL库预加载

<br/>

在得知`10.170.7.169/24`即为我们配置过的VPP网卡后，首先配置`LD_PRELOAD`环境变量，将系统的posix替换为vpp的acl库。

* `LD_PRELOAD`的作用：系统在运行过程中，会首先加载该环境变量指定的函数库（在libc.so之前加载），如果函数库内包含了程序中执行的函数名，该可执行文件的函数将被重定向到LD_PRELOAD指向的函数中。

```bash
export LD_PRELOAD=$VPP_ROOT/build-root/build-vpp-native/vpp/lib/libvcl_ldpreload.so
```
在10.170.7.166服务器上，我们的环境变量如下：
```bash
export LD_PRELOAD=/opt/vpp-stable-2005/build-root/build-vpp-native/vpp/lib/libvcl_ldpreload.so
echo $LD_PRELOAD
/opt/vpp-stable-2005/build-root/build-vpp-native/vpp/lib/libvcl_ldpreload.so
```

注意，当设定环境变量`LD_PRELOAD`后，该终端/bash下的所有指令，如有使用posix相关函数的，都将会被中定向到VPP的ACL库函数中。此时的socket相关系统指令可能执行失败，如下面的ping指令

```bash
ping 10.170.6.69
WARNING: your kernel is veeery old. No problems.
WARNING: setsockopt(IP_RECVTTL): 不支持的操作
WARNING: setsockopt(IP_RETOPTS): 不支持的操作
PING 10.170.6.69 (10.170.6.69) 56(84) bytes of data.
Warning: no SO_TIMESTAMP support, falling back to SIOCGSTAMP
ping: sendmsg: 传输端点尚未连接
ping: recvmsg: 函数未实现
^C
--- 10.170.6.69 ping statistics ---
1 packets transmitted, 0 received, 100% packet loss, time 0ms

vl_client_disconnect:323: queue drain: 531

```


<br/>

## 2.2. 启动iperf3服务端
在任一台服务器或许你中启动iperf3服务端，监听5201端口。端口可变
这里我进行了绑核操作，可以不绑。

```bash
iperf3 -s --bind 10.170.6.66 --affinity 1
-----------------------------------------------------------
Server listening on 5201
-----------------------------------------------------------
```

<br/>

## 2.3. 在VPP下启动iperf3 UDP客户端

启动iperf3客户端
```
# iperf3 -c 10.170.6.66 --bind 10.170.7.169 -u
```

输出如下：

```
warning: Warning:  UDP block size 1460 exceeds TCP MSS 0, may result in fragmentatio
n / dropsConnecting to host 10.170.6.66, port 5201
[ 33] local 10.170.7.169 port 58322 connected to 10.170.6.66 port 5201
[ ID] Interval           Transfer     Bandwidth       Total Datagrams
[ 33]   0.00-1.00   sec   115 KBytes   946 Kbits/sec  81  
[ 33]   1.00-2.00   sec   128 KBytes  1.05 Mbits/sec  90  
[ 33]   2.00-3.00   sec   128 KBytes  1.05 Mbits/sec  90  
[ 33]   3.00-4.00   sec   128 KBytes  1.05 Mbits/sec  90  
[ 33]   4.00-5.00   sec   127 KBytes  1.04 Mbits/sec  89  
[ 33]   5.00-6.00   sec   128 KBytes  1.05 Mbits/sec  90  
[ 33]   6.00-7.00   sec   128 KBytes  1.05 Mbits/sec  90  
[ 33]   7.00-8.00   sec   128 KBytes  1.05 Mbits/sec  90  
[ 33]   8.00-9.00   sec   128 KBytes  1.05 Mbits/sec  90  
[ 33]   9.00-10.00  sec   127 KBytes  1.04 Mbits/sec  89  
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bandwidth       Jitter    Lost/Total Datagrams
[ 33]   0.00-10.00  sec  1.24 MBytes  1.04 Mbits/sec  0.469 ms  0/889 (0%)  
[ 33] Sent 889 datagrams

iperf Done.
vl_client_disconnect:323: queue drain: 531
```

此时的服务端打印如下：

```
Accepted connection from 10.170.7.169, port 36991
[  5] local 10.170.6.66 port 5201 connected to 10.170.7.169 port 58322
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-1.00   sec   117 KBytes   956 Kbits/sec  127.396 ms  0/82 (0%)  
[  5]   1.00-2.00   sec   128 KBytes  1.05 Mbits/sec  0.845 ms  0/90 (0%)  
[  5]   2.00-3.01   sec   128 KBytes  1.05 Mbits/sec  0.195 ms  0/90 (0%)  
[  5]   3.01-4.01   sec   128 KBytes  1.05 Mbits/sec  0.321 ms  0/90 (0%)  
[  5]   4.01-5.00   sec   125 KBytes  1.03 Mbits/sec  0.332 ms  0/88 (0%)  
[  5]   5.00-6.00   sec   130 KBytes  1.06 Mbits/sec  0.499 ms  0/91 (0%)  
[  5]   6.00-7.01   sec   128 KBytes  1.04 Mbits/sec  0.538 ms  0/90 (0%)  
[  5]   7.01-8.01   sec   128 KBytes  1.06 Mbits/sec  0.463 ms  0/90 (0%)  
[  5]   8.01-9.00   sec   128 KBytes  1.06 Mbits/sec  0.434 ms  0/90 (0%)  
[  5]   9.00-10.00  sec   125 KBytes  1.03 Mbits/sec  0.469 ms  0/88 (0%)  
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-10.00  sec  1.24 MBytes  1.04 Mbits/sec  0.469 ms  0/889 (0%)  receiver

```
发现速率根本不对应，通过在网上查找资料，发现需要添加参数`-b`。相关链接：[Iperf results on UDP Bandwidth](https://networkengineering.stackexchange.com/questions/44304/iperf-results-on-udp-bandwidth)

参数`-b`的说明如下

```
  -b, --bandwidth #[KMG][/#] target bandwidth in bits/sec (0 for unlimited)
                            (default 1 Mbit/sec for UDP, unlimited for TCP)
                            (optional slash and packet count for burst mode)
```

再次测试：

```
# iperf3 -c 10.170.6.66 --bind 10.170.7.169 -b 1000M -u
```
输出如下：

```
warning: Warning:  UDP block size 1460 exceeds TCP MSS 0, may result in fragmentatio
n / dropsConnecting to host 10.170.6.66, port 5201
[ 33] local 10.170.7.169 port 1748 connected to 10.170.6.66 port 5201
[ ID] Interval           Transfer     Bandwidth       Total Datagrams
[ 33]   0.00-1.00   sec   109 MBytes   913 Mbits/sec  78148  
[ 33]   1.00-2.00   sec   119 MBytes  1000 Mbits/sec  85588  
[ 33]   2.00-3.00   sec   119 MBytes  1000 Mbits/sec  85601  
[ 33]   3.00-4.00   sec   119 MBytes  1.00 Gbits/sec  85660  
[ 33]   4.00-5.00   sec   119 MBytes  1.00 Gbits/sec  85620  
[ 33]   5.00-6.00   sec   119 MBytes   999 Mbits/sec  85567  
[ 33]   6.00-7.00   sec   119 MBytes  1.00 Gbits/sec  85669  
[ 33]   7.00-8.00   sec   119 MBytes  1000 Mbits/sec  85574  
[ 33]   8.00-9.00   sec   119 MBytes  1000 Mbits/sec  85603  
[ 33]   9.00-10.00  sec   119 MBytes  1.00 Gbits/sec  85683  
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bandwidth       Jitter    Lost/Total Datagrams
[ 33]   0.00-10.00  sec  1.15 GBytes   991 Mbits/sec  0.060 ms  818675/846746 (97%) 
 [ 33] Sent 846746 datagrams

iperf Done.
vl_client_disconnect:323: queue drain: 531
```

加大带宽再次测试：

```
# iperf3 -c 10.170.6.66 --bind 10.170.7.169 -b 10000M -u

```
```
warning: Warning:  UDP block size 1460 exceeds TCP MSS 0, may result in fragmentatio
n / dropsConnecting to host 10.170.6.66, port 5201
[ 33] local 10.170.7.169 port 32072 connected to 10.170.6.66 port 5201
[ ID] Interval           Transfer     Bandwidth       Total Datagrams
[ 33]   0.00-1.00   sec  1.13 GBytes  9.71 Gbits/sec  831282  
[ 33]   1.00-2.00   sec  1.18 GBytes  10.1 Gbits/sec  865276  
[ 33]   2.00-3.00   sec  1.17 GBytes  10.0 Gbits/sec  859818  
[ 33]   3.00-4.00   sec  1.16 GBytes  10.0 Gbits/sec  856510  
[ 33]   4.00-5.00   sec  1.16 GBytes  9.96 Gbits/sec  852834  
[ 33]   5.00-6.00   sec  1.15 GBytes  9.88 Gbits/sec  845942  
[ 33]   6.00-7.00   sec  1.17 GBytes  10.1 Gbits/sec  860781  
[ 33]   7.00-8.00   sec  1.16 GBytes  9.95 Gbits/sec  851835  
[ 33]   8.00-9.00   sec  1.18 GBytes  10.1 Gbits/sec  865302  
[ 33]   9.00-10.00  sec  1.17 GBytes  10.1 Gbits/sec  864098  
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bandwidth       Jitter    Lost/Total Datagrams
[ 33]   0.00-10.00  sec  11.6 GBytes  9.99 Gbits/sec  0.075 ms  8499899/8551442 (99%
)  [ 33] Sent 8551442 datagrams

iperf Done.
vl_client_disconnect:323: queue drain: 531

```
索性将带宽增加至`40000 Mbps`：
```
# iperf3 -c 10.170.6.66 --bind 10.170.7.169 -b 40000M -u
```
测试结果如下：
```
warning: Warning:  UDP block size 1460 exceeds TCP MSS 0, may result in fragmentatio
n / dropsConnecting to host 10.170.6.66, port 5201
[ 33] local 10.170.7.169 port 43051 connected to 10.170.6.66 port 5201
[ ID] Interval           Transfer     Bandwidth       Total Datagrams
[ 33]   0.00-1.00   sec  1.99 GBytes  17.1 Gbits/sec  1460305  
[ 33]   1.00-2.00   sec  1.99 GBytes  17.1 Gbits/sec  1461188  
[ 33]   2.00-3.00   sec  1.99 GBytes  17.1 Gbits/sec  1461091  
[ 33]   3.00-4.00   sec  1.99 GBytes  17.1 Gbits/sec  1461270  
[ 33]   4.00-5.00   sec  1.99 GBytes  17.1 Gbits/sec  1461472  
[ 33]   5.00-6.00   sec  1.99 GBytes  17.1 Gbits/sec  1463297  
[ 33]   6.00-7.00   sec  1.99 GBytes  17.1 Gbits/sec  1463611  
[ 33]   7.00-8.00   sec  1.99 GBytes  17.1 Gbits/sec  1463020  
[ 33]   8.00-9.00   sec  1.99 GBytes  17.1 Gbits/sec  1463278  
[ 33]   9.00-10.00  sec  1.99 GBytes  17.1 Gbits/sec  1462751  
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bandwidth       Jitter    Lost/Total Datagrams
[ 33]   0.00-10.00  sec  19.9 GBytes  17.1 Gbits/sec  0.061 ms  14553747/14612709 (1e+02%)  [ 33] Sent 14612709 datagrams

iperf Done.
vl_client_disconnect:323: queue drain: 531
```
此时服务端的丢包率非常大（还好我不用关心）：
```
Accepted connection from 10.170.7.169, port 34288
[  5] local 10.170.6.66 port 5201 connected to 10.170.7.169 port 24719
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-1.00   sec  10.3 MBytes  86.3 Mbits/sec  1.032 ms  1432532/1439957 (99%)  
[  5]   1.00-2.01   sec  9.67 MBytes  80.7 Mbits/sec  0.948 ms  1478665/1485608 (1e+02%)  
[  5]   2.01-3.00   sec  9.59 MBytes  81.0 Mbits/sec  0.747 ms  1465574/1472460 (1e+02%)  
[  5]   3.00-4.01   sec  9.29 MBytes  77.3 Mbits/sec  1.048 ms  1476442/1483116 (1e+02%)  
[  5]   4.01-5.00   sec  9.54 MBytes  80.7 Mbits/sec  0.996 ms  1457845/1464696 (1e+02%)  
[  5]   5.00-6.01   sec  9.69 MBytes  80.9 Mbits/sec  0.895 ms  1478246/1485203 (1e+02%)  
[  5]   6.01-7.01   sec  8.66 MBytes  72.5 Mbits/sec  0.924 ms  1474369/1480588 (1e+02%)  
[  5]   7.01-8.01   sec  8.72 MBytes  73.3 Mbits/sec  0.771 ms  1467575/1473841 (1e+02%)  
[  5]   8.01-9.00   sec  8.89 MBytes  74.8 Mbits/sec  0.767 ms  1467219/1473604 (1e+02%)  
[  5]   9.00-10.01  sec  9.31 MBytes  77.9 Mbits/sec  0.980 ms  1472938/1479624 (1e+02%)  
[  5]  10.01-10.25  sec   301 KBytes  10.1 Mbits/sec  0.225 ms  37991/38202 (99%)  
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-10.25  sec  94.0 MBytes  76.9 Mbits/sec  0.225 ms  14709396/14776899 (1e+02%)  receiver
-----------------------------------------------------------
Server listening on 5201
-----------------------------------------------------------

```
<br/>

# 3. 结论

我们采用VPP方案，使用英特尔X520网卡
```
# lspci | grep 10G
3b:00.0 Ethernet controller: Intel Corporation Ethernet 10G 2P X520 Adapter (rev 01)
3b:00.1 Ethernet controller: Intel Corporation Ethernet 10G 2P X520 Adapter (rev 01)
```
测出的发包峰值速率为`17.1 Gbits/sec`。



<br/>
<div align=right>	以上内容由荣涛调试、测试整理。</div>