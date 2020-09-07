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




# 1. 路由

<br/>

## 1.1. 要学习的技能
在本练习中，您将学习以下新技能：

* 将路由添加到Linux主机路由表
* 将路由添加到FD.io VPP路由表

并重温旧的：

* 检查FD.io VPP路由表
* 在vpp1和vpp2上启用跟踪
* 从主机ping到FD.io VPP
* 检查并清除vpp1和vpp2上的跟踪
* 从FD.io VPP ping到主机
* 检查并清除vpp1和vpp2上的跟踪

<br/>

## 1.2. 在本练习中学习了VPP命令
[ip路由添加](https://docs.fd.io/vpp/17.04/clicmd_src_vnet_ip.html#clicmd_ip_route)

<br/>

## 1.3. 拓扑结构

连接两个FD.io VPP拓扑
![拓扑结构](_v_images/20200907105915271_9141.png)

## 1.4. 初始状态
假定此初始状态是[连接两个FD.io VPP实例](https://fd.io/docs/vpp/master/gettingstarted/progressivevpp/VPP/Progressive_VPP_Tutorial#Connecting_two_vpp_instances)的练习的最终状态

## 1.5. 设置主机路由
```
$ sudo ip route add 10.10.2.0/24 via 10.10.1.2
$ ip route
default via 10.0.2.2 dev enp0s3
10.0.2.0/24 dev enp0s3  proto kernel  scope link  src 10.0.2.15
10.10.1.0/24 dev vpp1host  proto kernel  scope link  src 10.10.1.1
10.10.2.0/24 via 10.10.1.2 dev vpp1host
```

## 1.6. 在vpp2上设置返回路线
```
$ sudo vppctl -s /run/vpp/cli-vpp2.sock
 vpp# ip route add 10.10.1.0/24  via 10.10.2.1
```
## 1.7. 从主机通过vpp1 ping至vpp2
从vpp1到vpp2的连接使用memif驱动程序，到主机的连接使用af-packet驱动程序。要跟踪来自主机的数据包，我们使用从vpp1到vpp2的af-packet-input，我们使用memif-input。

* 在vpp1和vpp2上设置跟踪
* 从主机Ping 10.10.2.2
* 检查vpp1和vpp2上的跟踪
* 清除vpp1和vpp2上的跟踪




