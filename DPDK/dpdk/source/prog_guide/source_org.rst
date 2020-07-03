..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

**Part 2: Development Environment**

源码组织
==========

本节介绍DPDK框架中的源码组织结构。

Makefiles 和 Config
--------------------

.. note::

    在本文的描述中 ``RTE_SDK`` 作为环境变量指向DPDK源码包解压出来的文件根目录。
    参看 :ref:`Useful_Variables_Provided_by_the_Build_System` 获取更多的变量描述。

由DPDK库和应用程序提供的 Makefiles 位于 ``$(RTE_SDK)/mk`` 中。

配置模板位于 ``$(RTE_SDK)/config`` 。这些模板描述了为每个目标启用的选项。
配置文件许多可以为DPDK库启用或禁用的选项，包括调试选项。
用户应该查看配置文件并熟悉这些选项。
配置文件同样也用于创建头文件，创建的头文件将位于新生成的目录中。

库
---

库文件源码位于目录 ``$(RTE_SDK)/lib`` 中。
按照惯例，库指的是为应用程序提供API的任何代码。
通常，它会生成一个 (``.a``) 文件，这个目录中可能也保存一些内核模块。

Lib目标包含如下项目 ::

    lib
    +-- librte_cmdline      # 命令行接口
    +-- librte_distributor  # 报文分发器
    +-- librte_eal          # 环境抽象层
    +-- librte_ether        # PMD通用接口
    +-- librte_hash         # 哈希库
    +-- librte_ip_frag      # IP分片库
    +-- librte_kni          # 内核NIC接口
    +-- librte_kvargs       # 参数解析库
    +-- librte_lpm          # 最长前缀匹配库
    +-- librte_mbuf         # 报文及控制缓冲区操作库
    +-- librte_mempool      # 内存池管理器
    +-- librte_meter        # QoS metering 库
    +-- librte_net          # IP相关的一些头部
    +-- librte_power        # 电源管理库
    +-- librte_ring         # 软件无锁环形缓冲区
    +-- librte_sched        # QoS调度器和丢包器库
    +-- librte_timer        # 定时器库

驱动
------

驱动程序是为设备（硬件设备或者虚拟设备）提供轮询模式驱动程序实现的特殊库。
他们包含在 *drivers* 子目录中，按照类型分类，各自编译成一个库，其格式为 ``librte_pmd_X.a`` ，其中 ``X`` 是驱动程序的名称。

驱动程序目录下有个 *net* 子目录，包括如下项目::

    drivers/net
    +-- af_packet          # 基于Linux af_packet的pmd
    +-- bonding            # 绑定pmd驱动
    +-- cxgbe              # Chelsio Terminator 10GbE/40GbE pmd
    +-- e1000              # 1GbE pmd (igb and em)
    +-- enic               # Cisco VIC Ethernet NIC Poll-mode Driver
    +-- fm10k              # Host interface PMD driver for FM10000 Series
    +-- i40e               # 40GbE poll mode driver
    +-- ixgbe              # 10GbE poll mode driver
    +-- mlx4               # Mellanox ConnectX-3 poll mode driver
    +-- null               # NULL poll mode driver for testing
    +-- pcap               # PCAP poll mode driver
    +-- ring               # Ring poll mode driver
    +-- szedata2           # SZEDATA2 poll mode driver
    +-- virtio             # Virtio poll mode driver
    +-- vmxnet3            # VMXNET3 poll mode driver
    +-- xenvirt            # Xen virtio poll mode driver

.. note::

   部分 ``driver/net`` 目录包含一个 ``base`` 子目录，这个目录通常包含用户不能直接修改的代码。
   任何修订或增强都应该 ``X_osdep.c`` 或 ``X_osdep.h`` 文件完成。
   请参阅base目录中本地的自述文件以获取更多的信息。

应用程序
----------

应用程序是包含 ``main()`` 函数的源文件。
他们位于 ``$(RTE_SDK)/app`` 和 ``$(RTE_SDK)/examples`` 目录中。

应用程序目录包含用于测试DPPDK（如自动测试）或轮询模式驱动程序（test-pmd）的实例应用程序::

    app
    +-- chkincs            # Test program to check include dependencies
    +-- cmdline_test       # Test the commandline library
    +-- test               # Autotests to validate DPDK features
    +-- test-acl           # Test the ACL library
    +-- test-pipeline      # Test the IP Pipeline framework
    +-- test-pmd           # Test and benchmark poll mode drivers

Example目录包含示例应用程序，显示了如何使用库::

    examples
    +-- cmdline            # Example of using the cmdline library
    +-- exception_path     # Sending packets to and from Linux TAP device
    +-- helloworld         # Basic Hello World example
    +-- ip_reassembly      # Example showing IP reassembly
    +-- ip_fragmentation   # Example showing IPv4 fragmentation
    +-- ipv4_multicast     # Example showing IPv4 multicast
    +-- kni                # Kernel NIC Interface (KNI) example
    +-- l2fwd              # L2 forwarding with and without SR-IOV
    +-- l3fwd              # L3 forwarding example
    +-- l3fwd-power        # L3 forwarding example with power management
    +-- l3fwd-vf           # L3 forwarding example with SR-IOV
    +-- link_status_interrupt # Link status change interrupt example
    +-- load_balancer      # Load balancing across multiple cores/sockets
    +-- multi_process      # Example apps using multiple DPDK processes
    +-- qos_meter          # QoS metering example
    +-- qos_sched          # QoS scheduler and dropper example
    +-- timer              # Example of using librte_timer library
    +-- vmdq_dcb           # Example of VMDQ and DCB receiving
    +-- vmdq               # Example of VMDQ receiving
    +-- vhost              # Example of userspace vhost and switch

.. note::

    实际的实例目录可能与上面显示的有所出入。
    相关详细信息，请参考最新的DPDK代码。
