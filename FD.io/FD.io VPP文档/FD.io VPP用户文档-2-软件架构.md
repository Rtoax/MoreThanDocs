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



# 1. 软件架构

fd.io vpp实现是第三代矢量数据包处理实现，具体涉及美国专利7,961,636和早期的工作。请注意，Apache-2许可证专门授予非专有的专利许可证；我们将这项专利作为历史关注点。

为了提高性能，vpp数据平面由转发节点的有向图组成，该转发图每次调用处理多个数据包。这种模式可实现多种微处理器优化：流水线和预取以覆盖相关的读取延迟，固有的I缓存阶段行为，矢量指令。除了硬件输入和硬件输出节点之外，整个转发图都是可移植的代码。

根据当前的情况，我们经常启动多个工作线程，这些工作线程使用相同的转发图副本处理来自多个队列的进入哈希数据包

# 2. VPP层分类法
![VPP层-实施分类法](_v_images/20200907152226718_3254.png =1139x)

* VPP Infra-VPP基础结构层，其中包含核心库源代码。该层执行存储功能，与向量和环配合使用，在哈希表中执行键查找，并与用于调度图形节点的计时器一起使用。
* VLIB-向量处理库。vlib层还处理各种应用程序管理功能：缓冲区，内存和图形节点管理，维护和导出计数器，线程管理，数据包跟踪。Vlib实现了调试CLI（命令行界面）。
* VNET-与VPP的网络接口（第2层，第3层和第4层）配合使用，执行会话和流量管理，并与设备和数据控制平面配合使用。
* Plugins-包含越来越丰富的数据平面插件集，如上图所示。
* VPP-与以上所有内容链接的容器应用程序。
重要的是要对每一层都有一定的了解。最好在API级别处理大多数实现，否则就不去管它了。

## 2.1. [VPPINFRA（基础设施）](https://fd.io/docs/vpp/master/gettingstarted/developers/infrastructure.html#vppinfra-infrastructure)

与VPP基础结构层关联的文件位于./src/vppinfra文件夹中。

VPPinfra是基本c库服务的集合，足以建立直接在裸机上运行的独立程序。它还提供高性能的动态数组，哈希，位图，高精度的实时时钟支持，细粒度的事件记录和数据结构序列化。

关于vppinfra的一个合理的评论/合理的警告：您不能总是仅通过名称来从普通函数中的内联函数中告诉宏。宏通常用于避免函数调用，并引起（有意的）副作用。

详细介绍请参见：[VPPINFRA](https://fd.io/docs/vpp/master/gettingstarted/developers/infrastructure.html)

**`TODO`**

## 2.2. [VLIB（矢量处理库）](https://fd.io/docs/vpp/master/gettingstarted/developers/vlib.html#vlib-vector-processing-library)

与vlib关联的文件位于./src/{vlib，vlibapi，vlibmemory}文件夹中。这些库提供矢量处理支持，包括图形节点调度，可靠的多播支持，超轻量协作多任务线程，CLI，插件.DLL支持，物理内存和Linux epoll支持。

详细介绍请参见：[VLIB](https://fd.io/docs/vpp/master/gettingstarted/developers/vlib.html)

**`TODO`**


## 2.3. [VNET（VPP网络堆栈）](https://fd.io/docs/vpp/master/gettingstarted/developers/vnet.html#vnet-vpp-network-stack)

与VPP网络堆栈层关联的文件位于 ./src/vnet文件夹中。网络堆栈层基本上是其他层中代码的实例化。该层具有vnet库，该库提供矢量化的第2层和3个网络图节点，数据包生成器和数据包跟踪器。

在构建数据包处理应用程序方面，vnet提供了独立于平台的子图，一个子图连接了两个设备驱动程序节点。

典型的RX连接包括“以太网输入”（完整的软件分类，提供ipv4-input，ipv6-input，arp-input等）和“ ipv4-input-no-checksum” [如果硬件可以分类，请执行ipv4标头校验和] 。

详细介绍请参见：[VNET](https://fd.io/docs/vpp/master/gettingstarted/developers/vnet.html)

**`TODO`**
