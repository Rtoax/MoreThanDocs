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

配置你的应用程序
==================

以下各节介绍了在不同体系结构上配置DPDK应用程序的方法。


X86
-----

英特尔处理器提供性能计数器来监视事件
英特尔提供的某些工具（如VTune）可用于对应用程序进行配置和基准测试。
欲了解更多信息，请参阅英特尔 *VTune Performance Analyzer Essentials* 。

对于DPDK应用程序，这只能在Linux应用程序环境中完成。

应通过事件计数器监测的主要情况是：

*   Cache misses

*   Branch mis-predicts

*   DTLB misses

*   Long latency instructions and exceptions

请参考 `Intel Performance Analysis Guide <http://software.intel.com/sites/products/collateral/hpc/vtune/performance_analysis_guide.pdf>`_ 获取更多的信息。


ARM64
-------

使用 Linux perf
~~~~~~~~~~~~~~~~

ARM64体系结构提供性能计数器来监视事件。
Linux ``perf`` 工具可以用来分析和测试应用程序。
除了标准事件之外，还可以使用 ``perf`` 通过原始事件（ ``-e`` ``-rXX`` ）来分析arm64特定的PMU（性能监视单元）事件。

更多详细信息请参考 `ARM64 specific PMU events enumeration <http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.100095_0002_04_en/way1382543438508.html>`_.


高分辨率的cycle计数器
~~~~~~~~~~~~~~~~~~~~~~~~

基于 ``rte_rdtsc()`` 的默认 ``cntvct_el0`` 提供了一种便携的方式来获取用户空间中的时钟计数器。通常它运行在<= 100MHz下。

为高分辨时钟计数器启用 ``rte_rdtsc()`` 的替代方法是通过armv8 PMU子系统。
PMU周期计数器以CPU频率运行。但是，在arm64 linux内核中，默认情况下，不能从用户空间访问PMU周期计数器。
通过从特权模式（内核空间）配置PMU，可以为用户空间访问启用循环计数器。

默认情况下，``rte_rdtsc()`` 实现使用一个可移植的 ``cntvct_el0`` 方案。
应用程序可以使用“CONFIG_RTE_ARM_EAL_RDTSC_USE_PMU”选择基于PMU的实现。

下面的示例显示了在armv8机器上配置基于PMU的循环计数器的步骤。

.. code-block:: console

    git clone https://github.com/jerinjacobk/armv8_pmu_cycle_counter_el0
    cd armv8_pmu_cycle_counter_el0
    make
    sudo insmod pmu_el0_cycle_counter.ko
    cd $DPDK_DIR
    make config T=arm64-armv8a-linuxapp-gcc
    echo "CONFIG_RTE_ARM_EAL_RDTSC_USE_PMU=y" >> build/.config
    make

.. warning::

   The PMU based scheme is useful for high accuracy performance profiling with
   ``rte_rdtsc()``. However, this method can not be used in conjunction with
   Linux userspace profiling tools like ``perf`` as this scheme alters the PMU
   registers state.
