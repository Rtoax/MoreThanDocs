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


# 1. 使用trace命令

## 1.1. 要学习的技能

* 设置“trace”
* 查看“trace”
* 清除“trace”
* 使用主机上的ping进行验证
* 从vpp ping
* 检查ARP表
* 检查ip fib

<br/>

## 1.2. 基本跟踪命令

显示跟踪缓冲区[最大COUNT]。~~*下面的trace命令，我在执行过程中，好像没有什么返回。*~~

```
vpp# show trace
```

清除跟踪缓冲区和可用内存。

```
vpp# clear trace
```

过滤器跟踪输出-包含NODE COUNT | 排除NODE COUNT | 没有。

```
vpp# trace filter <include NODE COUNT | exclude NODE COUNT | none>
```

<br/>

## 1.3. 添加跟踪

```
vpp# trace add af-packet-input 10
```

**跟踪添加支持以下节点列表：**

* AF数据包输入
* avf输入
* 键合过程
* dpdk-crypto-input
* dpdk输入
* 切换轨迹
* ixge输入
* 记忆输入
* mrvl-pp2-输入
* 网图输入
* p2p-以太网输入
* pg输入
* Punt-socket-rx
* rdma输入
* 会话队列
* tuntap-rx
* 虚拟主机用户输入
* 虚拟输入
* vmxnet3-输入

<br/>

## 1.4. 从主机到VPP ping

```
vpp# q
$ ping -c 1 10.10.1.2
PING 10.10.1.2 (10.10.1.2) 56(84) bytes of data.
64 bytes from 10.10.1.2: icmp_seq=1 ttl=64 time=0.283 ms

--- 10.10.1.2 ping statistics ---
1 packets transmitted, 1 received, 0% packet loss, time 0ms
rtt min/avg/max/mdev = 0.283/0.283/0.283/0.000 ms
```

<br/>

## 1.5. 检查从主机到VPP的ping跟踪

```
$ sudo vppctl -s /run/vpp/cli-vpp1.sock
vpp# show trace
------------------- Start of thread 0 vpp_main -------------------
Packet 1

00:17:04:099260: af-packet-input
af_packet: hw_if_index 1 next-index 4
  tpacket2_hdr:
    status 0x20000001 len 98 snaplen 98 mac 66 net 80
    sec 0x5b60e370 nsec 0x3af2736f vlan 0 vlan_tpid 0
00:17:04:099269: ethernet-input
IP4: e2:0f:1e:59:ec:f7 -> 02:fe:d9:75:d5:b4
00:17:04:099285: ip4-input
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3f7c
  fragment id 0xe516, flags DONT_FRAGMENT
ICMP echo_request checksum 0xc043
00:17:04:099290: ip4-lookup
fib 0 dpo-idx 5 flow hash: 0x00000000
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3f7c
  fragment id 0xe516, flags DONT_FRAGMENT
ICMP echo_request checksum 0xc043
00:17:04:099296: ip4-local
  ICMP: 10.10.1.1 -> 10.10.1.2
    tos 0x00, ttl 64, length 84, checksum 0x3f7c
    fragment id 0xe516, flags DONT_FRAGMENT
  ICMP echo_request checksum 0xc043
00:17:04:099300: ip4-icmp-input
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3f7c
  fragment id 0xe516, flags DONT_FRAGMENT
ICMP echo_request checksum 0xc043
00:17:04:099301: ip4-icmp-echo-request
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3f7c
  fragment id 0xe516, flags DONT_FRAGMENT
ICMP echo_request checksum 0xc043
00:17:04:099303: ip4-load-balance
fib 0 dpo-idx 13 flow hash: 0x00000000
ICMP: 10.10.1.2 -> 10.10.1.1
  tos 0x00, ttl 64, length 84, checksum 0x4437
  fragment id 0xe05b, flags DONT_FRAGMENT
ICMP echo_reply checksum 0xc843
00:17:04:099305: ip4-rewrite
tx_sw_if_index 1 dpo-idx 1 : ipv4 via 10.10.1.1 host-vpp1out: mtu:9000 e20f1e59ecf702fed975d5b40800 flow hash: 0x00000000
00000000: e20f1e59ecf702fed975d5b4080045000054e05b4000400144370a0a01020a0a
00000020: 01010000c8437c92000170e3605b000000001c170f00000000001011
00:17:04:099307: host-vpp1out-output
host-vpp1out
IP4: 02:fe:d9:75:d5:b4 -> e2:0f:1e:59:ec:f7
ICMP: 10.10.1.2 -> 10.10.1.1
  tos 0x00, ttl 64, length 84, checksum 0x4437
  fragment id 0xe05b, flags DONT_FRAGMENT
ICMP echo_reply checksum 0xc843
```

<br/>

## 1.6. 清除跟踪缓冲区

```
vpp# clear trace
```

<br/>

## 1.7. 从VPP ping主机

**这一步，我们可以完成。-荣涛**

```
vpp# ping 10.10.1.1
64 bytes from 10.10.1.1: icmp_seq=1 ttl=64 time=.0789 ms
64 bytes from 10.10.1.1: icmp_seq=2 ttl=64 time=.0619 ms
64 bytes from 10.10.1.1: icmp_seq=3 ttl=64 time=.0519 ms
64 bytes from 10.10.1.1: icmp_seq=4 ttl=64 time=.0514 ms
64 bytes from 10.10.1.1: icmp_seq=5 ttl=64 time=.0526 ms

Statistics: 5 sent, 5 received, 0% packet loss
```

<br/>

## 1.8. 检查从VPP到主机的ping跟踪

输出将演示FD.io VPP对所有数据包执行ping操作的跟踪。

```
vpp# show trace
------------------- Start of thread 0 vpp_main -------------------
Packet 1

00:17:04:099260: af-packet-input
af_packet: hw_if_index 1 next-index 4
  tpacket2_hdr:
    status 0x20000001 len 98 snaplen 98 mac 66 net 80
    sec 0x5b60e370 nsec 0x3af2736f vlan 0 vlan_tpid 0
00:17:04:099269: ethernet-input
IP4: e2:0f:1e:59:ec:f7 -> 02:fe:d9:75:d5:b4
00:17:04:099285: ip4-input
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3f7c
  fragment id 0xe516, flags DONT_FRAGMENT
ICMP echo_request checksum 0xc043
00:17:04:099290: ip4-lookup
fib 0 dpo-idx 5 flow hash: 0x00000000
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3f7c
  fragment id 0xe516, flags DONT_FRAGMENT
ICMP echo_request checksum 0xc043
00:17:04:099296: ip4-local
  ICMP: 10.10.1.1 -> 10.10.1.2
    tos 0x00, ttl 64, length 84, checksum 0x3f7c
    fragment id 0xe516, flags DONT_FRAGMENT
  ICMP echo_request checksum 0xc043
00:17:04:099300: ip4-icmp-input
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3f7c
  fragment id 0xe516, flags DONT_FRAGMENT
ICMP echo_request checksum 0xc043
00:17:04:099301: ip4-icmp-echo-request
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3f7c
  fragment id 0xe516, flags DONT_FRAGMENT
ICMP echo_request checksum 0xc043
00:17:04:099303: ip4-load-balance
fib 0 dpo-idx 13 flow hash: 0x00000000
ICMP: 10.10.1.2 -> 10.10.1.1
  tos 0x00, ttl 64, length 84, checksum 0x4437
  fragment id 0xe05b, flags DONT_FRAGMENT
ICMP echo_reply checksum 0xc843
00:17:04:099305: ip4-rewrite
tx_sw_if_index 1 dpo-idx 1 : ipv4 via 10.10.1.1 host-vpp1out: mtu:9000 e20f1e59ecf702fed975d5b40800 flow hash: 0x00000000
00000000: e20f1e59ecf702fed975d5b4080045000054e05b4000400144370a0a01020a0a
00000020: 01010000c8437c92000170e3605b000000001c170f00000000001011
00:17:04:099307: host-vpp1out-output
host-vpp1out
IP4: 02:fe:d9:75:d5:b4 -> e2:0f:1e:59:ec:f7
ICMP: 10.10.1.2 -> 10.10.1.1
  tos 0x00, ttl 64, length 84, checksum 0x4437
  fragment id 0xe05b, flags DONT_FRAGMENT
ICMP echo_reply checksum 0xc843

Packet 2

00:17:09:113964: af-packet-input
af_packet: hw_if_index 1 next-index 4
  tpacket2_hdr:
    status 0x20000001 len 42 snaplen 42 mac 66 net 80
    sec 0x5b60e375 nsec 0x3b3bd57d vlan 0 vlan_tpid 0
00:17:09:113974: ethernet-input
ARP: e2:0f:1e:59:ec:f7 -> 02:fe:d9:75:d5:b4
00:17:09:113986: arp-input
request, type ethernet/IP4, address size 6/4
e2:0f:1e:59:ec:f7/10.10.1.1 -> 00:00:00:00:00:00/10.10.1.2
00:17:09:114003: host-vpp1out-output
host-vpp1out
ARP: 02:fe:d9:75:d5:b4 -> e2:0f:1e:59:ec:f7
reply, type ethernet/IP4, address size 6/4
02:fe:d9:75:d5:b4/10.10.1.2 -> e2:0f:1e:59:ec:f7/10.10.1.1

Packet 3

00:18:16:407079: af-packet-input
af_packet: hw_if_index 1 next-index 4
  tpacket2_hdr:
    status 0x20000001 len 98 snaplen 98 mac 66 net 80
    sec 0x5b60e3b9 nsec 0x90b7566 vlan 0 vlan_tpid 0
00:18:16:407085: ethernet-input
IP4: e2:0f:1e:59:ec:f7 -> 02:fe:d9:75:d5:b4
00:18:16:407090: ip4-input
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3fe8
  fragment id 0x24ab
ICMP echo_reply checksum 0x37eb
00:18:16:407094: ip4-lookup
fib 0 dpo-idx 5 flow hash: 0x00000000
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3fe8
  fragment id 0x24ab
ICMP echo_reply checksum 0x37eb
00:18:16:407097: ip4-local
  ICMP: 10.10.1.1 -> 10.10.1.2
    tos 0x00, ttl 64, length 84, checksum 0x3fe8
    fragment id 0x24ab
  ICMP echo_reply checksum 0x37eb
00:18:16:407101: ip4-icmp-input
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3fe8
  fragment id 0x24ab
ICMP echo_reply checksum 0x37eb
00:18:16:407104: ip4-icmp-echo-reply
ICMP echo id 7531 seq 1
00:18:16:407108: ip4-drop
  ICMP: 10.10.1.1 -> 10.10.1.2
    tos 0x00, ttl 64, length 84, checksum 0x3fe8
    fragment id 0x24ab
  ICMP echo_reply checksum 0x37eb
00:18:16:407111: error-drop
ip4-icmp-input: unknown type

Packet 4

00:18:17:409084: af-packet-input
af_packet: hw_if_index 1 next-index 4
  tpacket2_hdr:
    status 0x20000001 len 98 snaplen 98 mac 66 net 80
    sec 0x5b60e3ba nsec 0x90b539f vlan 0 vlan_tpid 0
00:18:17:409088: ethernet-input
IP4: e2:0f:1e:59:ec:f7 -> 02:fe:d9:75:d5:b4
00:18:17:409092: ip4-input
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3f40
  fragment id 0x2553
ICMP echo_reply checksum 0xcc6d
00:18:17:409095: ip4-lookup
fib 0 dpo-idx 5 flow hash: 0x00000000
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3f40
  fragment id 0x2553
ICMP echo_reply checksum 0xcc6d
00:18:17:409097: ip4-local
  ICMP: 10.10.1.1 -> 10.10.1.2
    tos 0x00, ttl 64, length 84, checksum 0x3f40
    fragment id 0x2553
  ICMP echo_reply checksum 0xcc6d
00:18:17:409099: ip4-icmp-input
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3f40
  fragment id 0x2553
ICMP echo_reply checksum 0xcc6d
00:18:17:409101: ip4-icmp-echo-reply
ICMP echo id 7531 seq 2
00:18:17:409104: ip4-drop
  ICMP: 10.10.1.1 -> 10.10.1.2
    tos 0x00, ttl 64, length 84, checksum 0x3f40
    fragment id 0x2553
  ICMP echo_reply checksum 0xcc6d
00:18:17:409104: error-drop
ip4-icmp-input: unknown type

Packet 5

00:18:18:409082: af-packet-input
af_packet: hw_if_index 1 next-index 4
  tpacket2_hdr:
    status 0x20000001 len 98 snaplen 98 mac 66 net 80
    sec 0x5b60e3bb nsec 0x8ecad24 vlan 0 vlan_tpid 0
00:18:18:409087: ethernet-input
IP4: e2:0f:1e:59:ec:f7 -> 02:fe:d9:75:d5:b4
00:18:18:409091: ip4-input
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3e66
  fragment id 0x262d
ICMP echo_reply checksum 0x8e59
00:18:18:409093: ip4-lookup
fib 0 dpo-idx 5 flow hash: 0x00000000
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3e66
  fragment id 0x262d
ICMP echo_reply checksum 0x8e59
00:18:18:409096: ip4-local
  ICMP: 10.10.1.1 -> 10.10.1.2
    tos 0x00, ttl 64, length 84, checksum 0x3e66
    fragment id 0x262d
  ICMP echo_reply checksum 0x8e59
00:18:18:409098: ip4-icmp-input
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3e66
  fragment id 0x262d
ICMP echo_reply checksum 0x8e59
00:18:18:409099: ip4-icmp-echo-reply
ICMP echo id 7531 seq 3
00:18:18:409102: ip4-drop
  ICMP: 10.10.1.1 -> 10.10.1.2
    tos 0x00, ttl 64, length 84, checksum 0x3e66
    fragment id 0x262d
  ICMP echo_reply checksum 0x8e59
00:18:18:409102: error-drop
ip4-icmp-input: unknown type

Packet 6

00:18:19:414750: af-packet-input
af_packet: hw_if_index 1 next-index 4
  tpacket2_hdr:
    status 0x20000001 len 98 snaplen 98 mac 66 net 80
    sec 0x5b60e3bc nsec 0x92450f2 vlan 0 vlan_tpid 0
00:18:19:414754: ethernet-input
IP4: e2:0f:1e:59:ec:f7 -> 02:fe:d9:75:d5:b4
00:18:19:414757: ip4-input
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3e52
  fragment id 0x2641
ICMP echo_reply checksum 0x9888
00:18:19:414760: ip4-lookup
fib 0 dpo-idx 5 flow hash: 0x00000000
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3e52
  fragment id 0x2641
ICMP echo_reply checksum 0x9888
00:18:19:414762: ip4-local
  ICMP: 10.10.1.1 -> 10.10.1.2
    tos 0x00, ttl 64, length 84, checksum 0x3e52
    fragment id 0x2641
  ICMP echo_reply checksum 0x9888
00:18:19:414764: ip4-icmp-input
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3e52
  fragment id 0x2641
ICMP echo_reply checksum 0x9888
00:18:19:414765: ip4-icmp-echo-reply
ICMP echo id 7531 seq 4
00:18:19:414768: ip4-drop
  ICMP: 10.10.1.1 -> 10.10.1.2
    tos 0x00, ttl 64, length 84, checksum 0x3e52
    fragment id 0x2641
  ICMP echo_reply checksum 0x9888
00:18:19:414769: error-drop
ip4-icmp-input: unknown type

Packet 7

00:18:20:418038: af-packet-input
af_packet: hw_if_index 1 next-index 4
  tpacket2_hdr:
    status 0x20000001 len 98 snaplen 98 mac 66 net 80
    sec 0x5b60e3bd nsec 0x937bcc2 vlan 0 vlan_tpid 0
00:18:20:418042: ethernet-input
IP4: e2:0f:1e:59:ec:f7 -> 02:fe:d9:75:d5:b4
00:18:20:418045: ip4-input
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3e47
  fragment id 0x264c
ICMP echo_reply checksum 0xc0e8
00:18:20:418048: ip4-lookup
fib 0 dpo-idx 5 flow hash: 0x00000000
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3e47
  fragment id 0x264c
ICMP echo_reply checksum 0xc0e8
00:18:20:418049: ip4-local
  ICMP: 10.10.1.1 -> 10.10.1.2
    tos 0x00, ttl 64, length 84, checksum 0x3e47
    fragment id 0x264c
  ICMP echo_reply checksum 0xc0e8
00:18:20:418054: ip4-icmp-input
ICMP: 10.10.1.1 -> 10.10.1.2
  tos 0x00, ttl 64, length 84, checksum 0x3e47
  fragment id 0x264c
ICMP echo_reply checksum 0xc0e8
00:18:20:418054: ip4-icmp-echo-reply
ICMP echo id 7531 seq 5
00:18:20:418057: ip4-drop
  ICMP: 10.10.1.1 -> 10.10.1.2
    tos 0x00, ttl 64, length 84, checksum 0x3e47
    fragment id 0x264c
  ICMP echo_reply checksum 0xc0e8
00:18:20:418058: error-drop
ip4-icmp-input: unknown type

Packet 8

00:18:21:419208: af-packet-input
af_packet: hw_if_index 1 next-index 4
  tpacket2_hdr:
    status 0x20000001 len 42 snaplen 42 mac 66 net 80
    sec 0x5b60e3be nsec 0x92a9429 vlan 0 vlan_tpid 0
00:18:21:419876: ethernet-input
ARP: e2:0f:1e:59:ec:f7 -> 02:fe:d9:75:d5:b4
00:18:21:419881: arp-input
request, type ethernet/IP4, address size 6/4
e2:0f:1e:59:ec:f7/10.10.1.1 -> 00:00:00:00:00:00/10.10.1.2
00:18:21:419896: host-vpp1out-output
host-vpp1out
ARP: 02:fe:d9:75:d5:b4 -> e2:0f:1e:59:ec:f7
reply, type ethernet/IP4, address size 6/4
02:fe:d9:75:d5:b4/10.10.1.2 -> e2:0f:1e:59:ec:f7/10.10.1.1
```

检查跟踪之后，请使用`vpp＃clear trace`再次将其清除。

<br/>

## 1.9. 检查ARP表

**这个我运行也查不到啊。-荣涛**

```
vpp# show ip arp
Time           IP4       Flags      Ethernet              Interface
1101.5636    10.10.1.1      D    e2:0f:1e:59:ec:f7 host-vpp1out
```

没有这个命令，难道说arp这个也是个插件，我没有加再进来？

```
RToax-VPP# show ip help
  container                      show ip container <address> <interface>
  fib                            show ip fib [summary] [table <table-id>] [index <fib-id>] [<ip4-addr>[/<mask>]] [m
trie] [detail]  local                          show ip local
  mfib                           show ip mfib [summary] [table <table-id>] [index <fib-id>] [<grp-addr>[/<mask>]] [
<grp-addr>] [<src-addr> <grp-addr>]  neighbors                      show ip neighbors [interface]
  neighbor                       show ip neighbor [interface]
  neighbor-config                show ip neighbor-config
  neighbor-watcher               show ip neighbors-watcher
  punt                           show ip punt commands
  source-and-port-range-check    show ip source-and-port-range-check vrf <table-id> <ip-addr> [port <n>]
```

<br/>


## 1.10. 检查路由表

```
RToax-VPP# show ip fib  
ipv4-VRF:0, fib_index:0, flow hash:[src dst sport dport proto ] epoch:0 flags:none locks:[adjacency:1, recursive-re
solution:1, default-route:1, nat-hi:2, ]0.0.0.0/0
  unicast-ip4-chain
  [@0]: dpo-load-balance: [proto:ip4 index:1 buckets:1 uRPF:0 to:[7523169:7733662576]]
    [0] [@0]: dpo-drop ip4
0.0.0.0/32
  unicast-ip4-chain
  [@0]: dpo-load-balance: [proto:ip4 index:2 buckets:1 uRPF:1 to:[0:0]]
    [0] [@0]: dpo-drop ip4
10.170.6.0/24
  unicast-ip4-chain
  [@0]: dpo-load-balance: [proto:ip4 index:14 buckets:1 uRPF:16 to:[650:330002]]
    [0] [@12]: dpo-load-balance: [proto:ip4 index:13 buckets:1 uRPF:10 to:[0:0] via:[650:330002]]
          [0] [@5]: ipv4 via 10.170.7.254 dpdk0: mtu:9000 next:3 346b5bf046c1b496916db4cc0800
10.170.7.0/32
  unicast-ip4-chain
  [@0]: dpo-load-balance: [proto:ip4 index:10 buckets:1 uRPF:11 to:[0:0]]
    [0] [@0]: dpo-drop ip4
10.170.7.2/32
  unicast-ip4-chain
  [@0]: dpo-load-balance: [proto:ip4 index:38 buckets:1 uRPF:42 to:[67:3410]]
    [0] [@5]: ipv4 via 10.170.7.2 dpdk0: mtu:9000 next:3 b82a72d0067eb496916db4cc0800
10.170.7.3/32
  unicast-ip4-chain
  [@0]: dpo-load-balance: [proto:ip4 index:24 buckets:1 uRPF:28 to:[368:63324]]
    [0] [@5]: ipv4 via 10.170.7.3 dpdk0: mtu:9000 next:3 eca86b30f687b496916db4cc0800
10.170.7.7/32
  unicast-ip4-chain
  [@0]: dpo-load-balance: [proto:ip4 index:46 buckets:1 uRPF:50 to:[6:426]]
    [0] [@5]: ipv4 via 10.170.7.7 dpdk0: mtu:9000 next:3 0023247dc5eab496916db4cc0800
10.170.7.30/32
  unicast-ip4-chain
  [@0]: dpo-load-balance: [proto:ip4 index:32 buckets:1 uRPF:36 to:[518:291334]]
    [0] [@5]: ipv4 via 10.170.7.30 dpdk0: mtu:9000 next:3 b82a72d03edab496916db4cc0800
10.170.7.32/32
  unicast-ip4-chain
  [@0]: dpo-load-balance: [proto:ip4 index:44 buckets:1 uRPF:48 to:[1:104]]
    [0] [@5]: ipv4 via 10.170.7.32 dpdk0: mtu:9000 next:3 000c29ed4141b496916db4cc0800
10.170.7.35/32
  unicast-ip4-chain
  [@0]: dpo-load-balance: [proto:ip4 index:21 buckets:1 uRPF:25 to:[505:41886]]
    [0] [@5]: ipv4 via 10.170.7.35 dpdk0: mtu:9000 next:3 a4bf01041deab496916db4cc0800
10.170.7.65/32
  unicast-ip4-chain
  [@0]: dpo-load-balance: [proto:ip4 index:23 buckets:1 uRPF:27 to:[641:38620]]
    [0] [@5]: ipv4 via 10.170.7.65 dpdk0: mtu:9000 next:3 e4434b3088d4b496916db4cc0800
10.170.7.79/32
  unicast-ip4-chain
  [@0]: dpo-load-balance: [proto:ip4 index:39 buckets:1 uRPF:43 to:[42:4566]]
    [0] [@5]: ipv4 via 10.170.7.79 dpdk0: mtu:9000 next:3 000c2907ec7cb496916db4cc0800

```

<br/>

<br/>
<div align=right>	以上内容由荣涛翻译整理自网络。</div>