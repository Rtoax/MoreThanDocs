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


[What is the Vector Packet Processor](https://fd.io/docs/vpp/master/#)



<br/>


# 1. VPP入门

* [下载并安装VPP](https://fd.io/docs/vpp/master/gettingstarted/installing/index.html)
* [渐进式VPP教程](https://fd.io/docs/vpp/master/gettingstarted/progressivevpp/index.html)
* [对于用户](https://fd.io/docs/vpp/master/gettingstarted/users/index.html)
* [对于开发人员](https://fd.io/docs/vpp/master/gettingstarted/developers/index.html)
* [撰写文件](https://fd.io/docs/vpp/master/gettingstarted/writingdocs/index.html)

<br/>


## 1.1. 渐进式VPP教程

了解如何使用Vagrant在单个Ubuntu 16.04 VM上运行FD.io VPP，本演练涵盖了基本FD.io VPP方案。将使用有用的FD.io VPP命令，并将讨论基本操作以及系统上正在运行的FD.io VPP的状态。

*注意：这并非旨在作为“如何在生产环境中运行”的一组说明。*

详情请见具体章节文档。

<br/>

## 1.2. 对用户

**“用户”部分描述了基本的VPP安装和配置操作**。VPP的安装和配置可以手动完成，也可以使用配置实用程序完成。

本节涵盖以下领域：

用户部分涵盖了基本的VPP安装和配置操作。本节涵盖以下领域：

* 描述了不同类型的VPP软件包
* 介绍如何在不同的OS平台（Ubuntu，Centos，openSUSE）上手动安装VPP Binaries
* 说明如何手动配置，然后运行VPP
* 介绍如何安装，然后使用配置实用程序配置VPP

* [配置VPP](https://fd.io/docs/vpp/master/gettingstarted/users/configuring/index.html)
    [大页内存](https://fd.io/docs/vpp/master/gettingstarted/users/configuring/hugepages.html)
    [VPP配置-CLI和'startup.conf'](https://fd.io/docs/vpp/master/gettingstarted/users/configuring/startup.html)
* [运行VPP](https://fd.io/docs/vpp/master/gettingstarted/users/running/index.html)
   ['vpp'用户组](https://fd.io/docs/vpp/master/gettingstarted/users/running/index.html#vpp-usergroup)
    [VPP系统文件-'vpp.service'](https://fd.io/docs/vpp/master/gettingstarted/users/running/index.html#vpp-systemd-file-vpp-service)




<br/>


## 1.3. 对开发人员

开发人员部分涵盖以下领域：

* 描述如何构建不同类型的VPP图像
* 通过一些GDB示例，说明如何在有和没有GDB的情况下运行VPP
* 描述了审核和合并补丁所需的步骤
* 描述VPP软件体系结构并标识关联的四个VPP层
* 描述与每个VPP层相关的不同组件
* 介绍如何创建，添加，启用/禁用不同的ARC功能
* 讨论了绑定索引可扩展哈希（bihash）的不同方面，以及如何在数据库查找中使用它
* 描述了不同类型的API支持以及如何集成插件

**`TODO`**

<br/>

# 2. VPP Wiki，Doxygen和其他链接

* [FD.io主站点](https://fd.io/docs/vpp/master/links/index.html#fd-io-main-site)
* [VPP Wiki](https://fd.io/docs/vpp/master/links/index.html#vpp-wiki)
* [源代码文档（doxygen）](https://fd.io/docs/vpp/master/links/index.html#source-code-documents-doxygen)

**`TODO`**


<br/>

# 3. 用例

* [带容器的VPP](https://fd.io/docs/vpp/master/usecases/containers.html)
* [具有Iperf3和TRex的VPP](https://fd.io/docs/vpp/master/usecases/simpleperf/index.html)
* [带有虚拟机的FD.io VPP](https://fd.io/docs/vpp/master/usecases/vhost/index.html)
* [带有VMware / Vmxnet3的VPP](https://fd.io/docs/vpp/master/usecases/vmxnet3.html)
* [带有FD.io VPP的访问控制列表（ACL）](https://fd.io/docs/vpp/master/usecases/acls.html)
* [云中的VPP](https://fd.io/docs/vpp/master/usecases/vppcloud.html)
* [使用VPP作为家庭网关](https://fd.io/docs/vpp/master/usecases/homegateway.html)
* [Contiv / VPP](https://fd.io/docs/vpp/master/usecases/contiv/index.html)
* [网络模拟器插件](https://fd.io/docs/vpp/master/usecases/networksim.html)
* [构建VPP Web应用程序](https://fd.io/docs/vpp/master/usecases/webapp.html)
* [基于容器的网络仿真](https://fd.io/docs/vpp/master/usecases/container_test.html)

**`TODO`**

<br/>


# 4. 发行功能

网站发行版最高为`19.08.`，我在`10.170.7.166`上安装的是`20.05`.

* [VPP版本19.08的功能](https://fd.io/docs/vpp/master/featuresbyrelease/vpp1908.html)
* VPP版本19.04的功能
* VPP版本19.01的功能


**`TODO`**

<br/>

# 5. 故障排除

[故障排除](https://fd.io/docs/vpp/master/troubleshooting/index.html)
本章介绍了用于解决和诊断FD.io VPP实现问题的许多技术中的一些。

**`TODO`**

<br/>

# 6. 参考

* [有用的调试CLI](https://fd.io/docs/vpp/master/reference/cmdreference/index.html)
* [虚拟机与游民](https://fd.io/docs/vpp/master/reference/vppvagrant/index.html)
* [阅读文档](https://fd.io/docs/vpp/master/reference/readthedocs/index.html)
* [Github仓库](https://fd.io/docs/vpp/master/reference/github/index.html)


**`TODO`**

<br/>
