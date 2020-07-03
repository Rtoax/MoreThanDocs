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

术语
======


ACL
   访问控制列表

API
   应用程序编程接口

ASLR
   Linux内核地址空间布局
BSD
   伯克利软件

Clr
   Clear

CIDR
   无类别域间路由

Control Plane
   控制面

Core
   如果处理器支持超线程，则内核可能包含多个内核或线程。

Core Components
   DPDK提供的一组库，包括 eal, ring, mempool, mbuf, timers等扥。

CPU
   Central Processing Unit

CRC
   Cyclic Redundancy Check

ctrlmbuf
   携带控制数据的 *mbuf* 。

Data Plane
   数据面，在网络架构层中对应于控制面，负责转发报文。这层必须具有很高的性能。

DIMM
   Dual In-line Memory Module

Doxygen
   DPDK中用于生成API参考的文档生成器。

DPDK
   Data Plane Development Kit

DRAM
   Dynamic Random Access Memory

EAL
   The Environment Abstraction Layer (EAL) provides a generic interface that
   hides the environment specifics from the applications and libraries.  The
   services expected from the EAL are: development kit loading and launching,
   core affinity/ assignment procedures, system memory allocation/description,
   PCI bus access, inter-partition communication.

FIFO
   First In First Out

FPGA
   Field Programmable Gate Array

GbE
   Gigabit Ethernet

HW
   Hardware

HPET
   High Precision Event Timer; a hardware timer that provides a precise time
   reference on x86 platforms.

ID
   Identifier

IOCTL
   Input/Output Control

I/O
   Input/Output

IP
   Internet Protocol

IPv4
   Internet Protocol version 4

IPv6
   Internet Protocol version 6

lcore
   A logical execution unit of the processor, sometimes called a *hardware
   thread*.

KNI
   Kernel Network Interface

L1
   Layer 1

L2
   Layer 2

L3
   Layer 3

L4
   Layer 4

LAN
   Local Area Network

LPM
   Longest Prefix Match

master lcore
   The execution unit that executes the main() function and that launches
   other lcores.

mbuf
   An mbuf is a data structure used internally to carry messages (mainly
   network packets).  The name is derived from BSD stacks.  To understand the
   concepts of packet buffers or mbuf, refer to *TCP/IP Illustrated, Volume 2:
   The Implementation*.

MESI
   Modified Exclusive Shared Invalid (CPU cache coherency protocol)

MTU
   Maximum Transfer Unit

NIC
   Network Interface Card

OOO
   Out Of Order (execution of instructions within the CPU pipeline)

NUMA
   Non-uniform Memory Access

PCI
   Peripheral Connect Interface

PHY
   An abbreviation for the physical layer of the OSI model.

pktmbuf
   An *mbuf* carrying a network packet.

PMD
   Poll Mode Driver

QoS
   Quality of Service

RCU
   Read-Copy-Update algorithm, an alternative to simple rwlocks.

Rd
   Read

RED
   Random Early Detection

RSS
   Receive Side Scaling

RTE
   Run Time Environment. Provides a fast and simple framework for fast packet
   processing, in a lightweight environment as a Linux* application and using
   Poll Mode Drivers (PMDs) to increase speed.

Rx
   Reception

Slave lcore
   Any *lcore* that is not the *master lcore*.

Socket
   A physical CPU, that includes several *cores*.

SLA
   Service Level Agreement

srTCM
   Single Rate Three Color Marking

SRTD
   Scheduler Round Trip Delay

SW
   Software

Target
   In the DPDK, the target is a combination of architecture, machine,
   executive environment and toolchain.  For example:
   i686-native-linuxapp-gcc.

TCP
   Transmission Control Protocol

TC
   Traffic Class

TLB
   Translation Lookaside Buffer

TLS
   Thread Local Storage

trTCM
   Two Rate Three Color Marking

TSC
   Time Stamp Counter

Tx
   Transmission

TUN/TAP
   TUN 和 TAP 是虚拟的网络内核设备。

VLAN
   Virtual Local Area Network

Wr
   Write

WRED
   加权随机早丢弃

WRR
   加权轮询
