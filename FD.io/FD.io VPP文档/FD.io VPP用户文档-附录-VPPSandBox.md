<div align=center>
	<img src="_v_images/20200904171558212_22234.png" width="300"> 
</div>

<br/>
<br/>
<br/>

<center><font size='20'>FD.io VPP：用户文档 SandBox</font></center>
<br/>
<br/>
<center><font size='5'>荣涛</font></center>
<center><font size='5'>2020年9月</font></center>
<br/>
<br/>
<br/>
<br/>

[vppsb](https://git.fd.io/vppsb/)

# 1. VPP SandBox存储库
VPP SandBox是一个临时托管场所，用于存放与VPP相关的小型扩展，插件，库或脚本。它旨在通过为VPP新手提供托管，可见性和质量代码审查来促进自举。

该存储库包含多个且大多是独立的工作，每个工作都有一个根目录。

要寻求新的尝试或获取更多信息，请访问项目Wiki页面。

```bash
$ git clone https://git.fd.io/vppsb.git
$ cd vppsb/
$ ls
flowtable  netlink  README.md  router  turbotap  vcl-ldpreload  vhost-test  vpp-bootstrap  vpp-userdemo
```
## 1.1. flowtable

提供一个流表节点来进行流分类，并关联一个流上下文，该上下文可以根据需要由另一个节点或外部应用程序丰富。 目的是要适应性强，以便用于任何有状态的用途，例如负载平衡，防火墙...

## 1.2. router

该存储库提供了一个插件，可通过Tap接口将数据平面接口连接到主机操作系统。 主机操作系统可以运行引用Tap接口的路由守护程序，就像它们是数据平面接口一样。 IPv4数据包转发决定在数据路径中进行，而控制流量则发送到主机并由主机处理。

## 1.3. turbotap

该存储库提供了一个实验性工作，它是一个使用套接字API系统调用“ sendmmsg”或“ recvmmsg”来使用Tap接口的插件，它允许使用一个系统调用来发送/接收多个数据包。 它替代了VPP中的tapcli驱动程序，后者每个数据包使用一个系统调用。 因此，节省了在用户空间和内核空间之间进行“上下文切换”的时间。

## 1.4. vcl-ldpreload
```
# vcl-ldpreload a LD_PRELOAD library that uses the VPP Communications Library (VCL).

========================== NOTE ==========================
The VCL-LDPRELOAD library has been moved to VPP project at

             .../vpp/extras/vcl-ldpreload

The code which resides here is no longer under development.
===========================================================

```
参见以下目录内容，服务器`10.170.7.166`
```
# 2. pwd
/opt/vpp-stable-2005/extras/vcl-ldpreload
```

<br/>

# 2. 联系
邮件列表：vppsb-dev@lists.fd.io [[订阅](https://lists.fd.io/mailman/listinfo/vppsb-dev)]。
IRC：＃fdio-vppsb @ freenode.com网络聊天







<br/>
<div align=right>	以上内容由荣涛翻译整理自网络。</div>