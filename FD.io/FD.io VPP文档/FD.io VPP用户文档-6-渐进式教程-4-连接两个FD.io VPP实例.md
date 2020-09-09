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




# 1. 连接两个FD.io VPP实例

[连接两个FD.io VPP实例](https://fd.io/docs/vpp/master/gettingstarted/progressivevpp/twovppinstances.html)

`memif`是一种非常高性能的直接内存接口类型，可以在FD.io VPP实例之间使用。它使用文件套接字作为控制通道来设置**共享内存**。

这里，我把他理解成，VPP支持的两个进程间通信，采用memif进行通信，memif的实现机制是共享内存。

<br/>

## 1.1. 要学习的技能

您将在此练习中学习以下新技能：

* 在两个FD.io VPP实例之间创建一个`memif接口`

您应该能够使用先前练习中学到的以下技能来执行此练习：

* 运行第二个FD.io VPP实例
* 将IP地址添加到FD.io VPP接口
* 来自FD.io VPP的Ping

<br/>

## 1.2. 拓扑结构

![连接两个FD.io VPP拓扑](_v_images/20200907102204795_1499.png)

<br/>

## 1.3. 初始状态

假定此处的初始状态是练习中的最终状态。

<br/>

## 1.4. 运行第二个FD.io VPP实例

您应该已经有一个运行的FD.io VPP实例，名为：vpp1。

运行另一个名为vpp2的FD.io VPP实例。

```
$ sudo /usr/bin/vpp -c startup2.conf
....
$ sudo vppctl -s /run/vpp/cli-vpp2.sock
    _______    _        _   _____  ___
 __/ __/ _ \  (_)__    | | / / _ \/ _ \
 _/ _// // / / / _ \   | |/ / ___/ ___/
 /_/ /____(_)_/\___/   |___/_/  /_/

vpp# show version
vpp v18.07-release built by root on c469eba2a593 at Mon Jul 30 23:27:03 UTC 2018
vpp# quit
```

<br/>

## 1.5. 在vpp1上创建memif接口
在vpp1上创建一个memif接口。要连接到实例vpp1，请使用套接字/run/vpp/cli-vpp1.sock

```
$ sudo vppctl -s /run/vpp/cli-vpp1.sock
vpp# create interface memif id 0 master
```

这将使用`/ run / vpp / memif`作为其套接字文件在`vpp1 memif0 / 0`上创建一个接口。此memif接口的vpp1角色是“主”。

通过所学的知识：

* 将memif0 / 0状态设置为up。
* 将IP地址10.10.2.1/24分配给memif0 / 0
* 通过show命令检查memif0 / 0

<br/>

## 1.6. 在vpp2上创建memif接口
我们希望vpp2使用相同的run / vpp / memif-vpp1vpp2套接字文件来承担“从属”角色

```
vpp# create interface memif id 0 slave
```

这将使用/ run / vpp / memif作为其套接字文件在vpp2 memif0 / 0上创建一个接口。此memif接口的vpp1角色是“从属”。

使用您以前使用的技能来：

* 将memif0 / 0状态设置为up。
* 将IP地址10.10.2.2/24分配给memif0 / 0
* 通过show命令检查memif0 / 0

<br/>

## 1.7. 从vpp1 ping到vpp2

从vpp1 Ping 10.10.2.2

```
$ ping 10.10.2.2
```

从vpp2 Ping 10.10.2.1

```
$ ping 10.10.2.1
```

<br/>

<br/>
<div align=right>	以上内容由荣涛翻译整理自网络。</div>