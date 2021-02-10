<div align=center>
	<img src="_v_images/20200904171558212_22234.png" width="300"> 
</div>

<br/>
<br/>
<br/>

<center><font size='6'>研华服务器万兆网口UDP灌包速率测试对比报告</font></center>
<br/>
<br/>
<center><font size='5'>荣涛</font></center>
<center><font size='5'>2021年1月</font></center>
<br/>
<br/>


# 1. 测试结果

详细的测试结果请参见下面的章节，总体的速率为：

* **戴尔服务器**：`10G 2P X520`网卡，峰值速率为：`17.1 Gbits/sec`；（2020年9月测试的结果）
* **研华服务器**：`X722 for 10GbE SFP+`网卡，峰值速率为：`7.85 Gbits/sec`；

<br/>

# 2. 戴尔服务器UDP灌包峰值速率

在历史邮件中“FD.io VPP环境下运行应用程序”已经给出了戴尔服务器的iperf3测试速率，这套环境若没有重新测试，下面是历史的UDP灌包速率：

戴尔服务器网卡型号
```c
3b:00.0 Ethernet controller: Intel Corporation Ethernet 10G 2P X520 Adapter (rev 01)
3b:00.1 Ethernet controller: Intel Corporation Ethernet 10G 2P X520 Adapter (rev 01)
```

<br/>

## 2.1. 启动iperf3服务端（内核协议端）
在任一台服务器或许你中启动iperf3服务端，监听5201端口。端口可变
这里我进行了绑核操作，可以不绑。

```bash
iperf3 -s --bind 10.170.6.66 --affinity 1
-----------------------------------------------------------
Server listening on 5201
-----------------------------------------------------------
```

<br/>

## 2.2. 启动iperf3客户端（DPDK端）
带宽使用`40000 Mbps`，显然这个数值永远不会达到，只是为了测试峰值：
```c
# iperf3 -c 10.170.6.66 --bind 10.170.7.169 -b 40000M -u
```
测试结果如下：
```c
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

<br/>

# 3. 研华服务器UDP灌包峰值速率

研华服务器网卡型号
```c
3d:00.0 Ethernet controller: Intel Corporation Ethernet Connection X722 for 10GbE SFP+ (rev 09)
3d:00.1 Ethernet controller: Intel Corporation Ethernet Connection X722 for 10GbE SFP+ (rev 09)
```

<br/>


## 3.1. 启动iperf3服务端（内核协议端）
```c
# iperf3 -s --bind 10.37.8.74 --affinity 1
-----------------------------------------------------------
Server listening on 5201
-----------------------------------------------------------
```

<br/>

## 3.2. 启动iperf3客户端（DPDK端）

由于服务端是普通的1G口，服务端的iperf3日志信息为：
```c
Accepted connection from 10.37.8.111, port 59525
[  5] local 10.37.8.74 port 5201 connected to 10.37.8.111 port 49184
[ ID] Interval           Transfer     Bandwidth       Jitter    Lost/Total Datagrams
[  5]   0.00-1.00   sec   114 MBytes   956 Mbits/sec  0.023 ms  582704/664561 (88%)  
[  5]   1.00-2.00   sec   114 MBytes   957 Mbits/sec  0.014 ms  589801/671721 (88%)  
[  5]   2.00-3.00   sec   114 MBytes   957 Mbits/sec  0.017 ms  589209/671109 (88%)  
[  5]   3.00-4.00   sec   114 MBytes   957 Mbits/sec  0.015 ms  587307/669229 (88%)  
[  5]   4.00-5.00   sec   114 MBytes   957 Mbits/sec  0.013 ms  590310/672222 (88%)  
[  5]   5.00-6.00   sec   114 MBytes   956 Mbits/sec  0.036 ms  585949/667843 (88%)  
[  5]   6.00-7.00   sec   114 MBytes   957 Mbits/sec  0.013 ms  587469/669394 (88%)  
[  5]   7.00-8.00   sec   114 MBytes   957 Mbits/sec  0.021 ms  587750/669661 (88%)  
[  5]   8.00-9.00   sec   114 MBytes   957 Mbits/sec  0.021 ms  590926/672835 (88%)  
[  5]   9.00-10.00  sec   114 MBytes   957 Mbits/sec  0.014 ms  588054/669978 (88%)  
[  5]  10.00-10.26  sec   174 KBytes  5.59 Mbits/sec  0.015 ms  848/970 (87%)  
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bandwidth       Jitter    Lost/Total Datagrams
[  5]   0.00-10.26  sec  0.00 Bytes  0.00 bits/sec  0.015 ms  5880327/6699523 (88%) 
```

此时，客户端(DPDK端)的iperf3日志信息为：
```c
# iperf3 -c 10.37.8.74 --bind 10.37.8.111 -b 40000M -u
warning: Warning:  UDP block size 1460 exceeds TCP MSS 0, may result in fragmentation / drops
Connecting to host 10.37.8.74, port 5201
[ 33] local 10.37.8.111 port 49184 connected to 10.37.8.74 port 5201
[ ID] Interval           Transfer     Bandwidth       Total Datagrams
[ 33]   0.00-1.00   sec   926 MBytes  7.77 Gbits/sec  665274  
[ 33]   1.00-2.00   sec   935 MBytes  7.85 Gbits/sec  671696  
[ 33]   2.00-3.00   sec   935 MBytes  7.84 Gbits/sec  671163  
[ 33]   3.00-4.00   sec   932 MBytes  7.82 Gbits/sec  669210  
[ 33]   4.00-5.00   sec   936 MBytes  7.85 Gbits/sec  672321  
[ 33]   5.00-6.00   sec   930 MBytes  7.80 Gbits/sec  667888  
[ 33]   6.00-7.00   sec   932 MBytes  7.82 Gbits/sec  669351  
[ 33]   7.00-8.00   sec   933 MBytes  7.82 Gbits/sec  669756  
[ 33]   8.00-9.00   sec   937 MBytes  7.86 Gbits/sec  672943  
[ 33]   9.00-10.00  sec   933 MBytes  7.82 Gbits/sec  669924  
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bandwidth       Jitter    Lost/Total Datagrams
[ 33]   0.00-10.00  sec  9.11 GBytes  7.83 Gbits/sec  0.015 ms  5880327/6699523 (88%)  
[ 33] Sent 6699523 datagrams

iperf Done.
```


