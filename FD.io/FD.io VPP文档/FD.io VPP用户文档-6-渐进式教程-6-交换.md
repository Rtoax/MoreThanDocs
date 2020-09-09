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



# 1. 交换
## 1.1. 要学习的技能

* 将接口与网桥域相关联
* 创建回送接口
* 为网桥域创建BVI（网桥虚拟接口）
* 检查桥域

## 1.2. 在本练习中学习了FD.io VPP命令

* [桥接](https://docs.fd.io/vpp/17.04/clicmd_src_vnet_l2.html#clicmd_show_bridge-domain)
* [显示桥接细节](https://docs.fd.io/vpp/17.04/clicmd_src_vnet_l2.html#clicmd_show_bridge-domain)
* [设置int l2桥](https://docs.fd.io/vpp/17.04/clicmd_src_vnet_l2.html#clicmd_set_interface_l2_bridge)
* [显示详细的l2fib](https://docs.fd.io/vpp/17.04/clicmd_src_vnet_l2.html#clicmd_show_l2fib)

## 1.3. 拓扑结构

![拓扑结构](_v_images/20200907110359654_1018.jpg)

## 1.4. 初始状态

与以前的练习不同，对于此练习，您要开始进行表格绘制。

注意：您将丢失FD.io VPP实例中的所有现有配置！

要从以前的练习中清除现有配置，请运行：
```
$ ps -ef | grep vpp | awk '{print $2}'| xargs sudo kill
$ sudo ip link del dev vpp1host
$ # do the next command if you are cleaning up from this example
$ sudo ip link del dev vpp1vpp2
```

## 1.5. 运行FD.io VPP实例

1.运行名为vpp1的vpp实例
2.运行名为vpp2的vpp实例

## 1.6. 将vpp1连接到主机

* 创建一个veth，一端名为vpp1host，另一端名为vpp1out。
* 将vpp1out连接到vpp1
* 在vpp1host上添加IP地址10.10.1.1/24

## 1.7. 将vpp1连接到vpp2

* 创建一端名为vpp1vpp2且另一端名为vpp2vpp1的veth。
* 将vpp1vpp2连接到vpp1。
* 将vpp2vpp1连接到vpp2。

## 1.8. 在vpp1上配置网桥域
检查以查看已经存在哪些桥接域，然后选择第一个未使用的桥接域号：

```
vpp# show bridge-domain
 ID   Index   Learning   U-Forwrd   UU-Flood   Flooding   ARP-Term     BVI-Intf
 0      0        off        off        off        off        off        local0
```
在上面的示例中，网桥域ID已经为“ 0”。即使有时我们可能会收到如下反馈：
```
no bridge-domains in use
```
网桥域ID“ 0”仍然存在，不支持任何操作。例如，如果我们尝试将host-vpp1out和host-vpp1vpp2添加到网桥域ID 0，则将无法进行任何设置。
```
vpp# set int l2 bridge host-vpp1out 0
vpp# set int l2 bridge host-vpp1vpp2 0
vpp# show bridge-domain 0 detail
show bridge-domain: No operations on the default bridge domain are supported
```
因此，我们将创建桥域1而不是使用默认桥域ID 0进行播放。

将host-vpp1out添加到网桥域ID 1
```
vpp# set int l2 bridge host-vpp1out 1
```
将host-vpp1vpp2添加到网桥域ID1
```
vpp# set int l2 bridge host-vpp1vpp2  1
```
检查网桥域1：
```
vpp# show bridge-domain 1 detail
BD-ID   Index   BSN  Age(min)  Learning  U-Forwrd  UU-Flood  Flooding  ARP-Term  BVI-Intf
1       1      0     off        on        on        on        on       off       N/A

        Interface           If-idx ISN  SHG  BVI  TxFlood        VLAN-Tag-Rewrite
    host-vpp1out            1     1    0    -      *                 none
    host-vpp1vpp2           2     1    0    -      *                 none
```

## 1.9. 在vpp2上配置环回接口
```
vpp# create loopback interface
loop0
```
将IP地址10.10.1.2/24添加到vpp2接口loop0。将vpp2上的接口loop0的状态设置为“ up”

## 1.10. 在vpp2上配置网桥域
检查以查看第一个可用的网桥域ID（在这种情况下为1）

将接口loop0作为桥接虚拟接口（bvi）添加到桥接域1
```
vpp# set int l2 bridge loop0 1 bvi
```
将接口vpp2vpp1添加到网桥域1
```
vpp# set int l2 bridge host-vpp2vpp1  1
```
检查网桥域和接口。

## 1.11. 从主机ping到vpp，从vpp到主机

* 1.在vpp1和vpp2上添加跟踪
* 2.从主机ping到10.10.1.2
* 3.检查并清除vpp1和vpp2上的跟踪
* 4.从vpp2 ping到10.10.1.1
* 5.检查并清除vpp1和vpp2上的跟踪

## 1.12. 检查l2 fib

```
vpp# show l2fib verbose
Mac Address     BD Idx           Interface           Index  static  filter  bvi   Mac Age (min)
de:ad:00:00:00:00    1            host-vpp1vpp2           2       0       0     0      disabled
c2:f6:88:31:7b:8e    1            host-vpp1out            1       0       0     0      disabled
2 l2fib entries
```

```
vpp# show l2fib verbose
Mac Address     BD Idx           Interface           Index  static  filter  bvi   Mac Age (min)
de:ad:00:00:00:00    1                loop0               2       1       0     1      disabled
c2:f6:88:31:7b:8e    1            host-vpp2vpp1           1       0       0     0      disabled
2 l2fib entries
```

<br/>
<div align=right>	以上内容由荣涛翻译整理自网络。</div>