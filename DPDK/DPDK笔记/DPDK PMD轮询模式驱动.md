<div align=center>
	<img src="_v_images/20200910110325796_584.png" width="600"> 
</div>
<br/>
<br/>

<center><font size='20'>DPDK笔记 PMD轮训模式驱动</font></center>
<br/>
<br/>
<center><font size='5'>RToax</font></center>
<center><font size='5'>2020年9月</font></center>
<br/>
<br/>
<br/>
<br/>


[dpdk的Poll Model Driver机制简介](https://my.oschina.net/u/2539854/blog/735332)

Linux内核的网络协议栈是基于中断的，网卡收到数据包后发出中断信号，CPU停止当前的处理工作，进入中断环境，执行相应网卡驱动的中断处理程序。 这种处理方式适合通用的处理平台，使CPU可以执行多样化的任务。而对于一些专门处理网络事务的设备，很多都配有1G, 10G, 甚至40G网卡，高负载时，网卡中断非常频繁，每次处理网卡中断都要涉及进程上下文的切换，花费很多额外的时间，这时便有了轮询的处理方法，由CPU主动收包，更适合大流量的处理。

这里使用的是dpdk-16.07,  intell 82599网卡，ixgbe驱动。

# 1. DMA环形缓冲区

intell 82599网卡最多支持128的Rx队列，设备初始化时可以设置启用的队列数量，对每一个队列都会分配一个DMA缓冲区队列，大小为256。队列的每一个元素是一个结构体，存放一块预分配的内存的地址等信息。 

下面是intell 官方文档中关于接收队列的描述。Head指向下一个空闲的元素，Tail指向已使用的元素，Head和Tail之间蓝色区域为空闲的元素。收到一个数据包后Head逆时针移动一格， Head等于Tail时没有空闲的元素。

![](_v_images/20200910144633189_16183.png)

# 2. 网卡接收网络包流程
网口收到数据包 -> 通过设定的规则确定发送到哪个队列 -> 从DMA环形缓冲区寻找空闲的预分配内存--> 通过DMA复制数据包到内存。

如果已经设定好了DMA环形缓冲区，那么从收到网络包到复制到内存，不需要CPU的参与，网卡可以自动完成。 数据包复制到内存后便可以被CPU访问，如果采用中断的方式，这时会由设备产生中断信号，CPU执行中断处理程序，进入内核的网络协议栈处理。这里采用PMD轮询，不会产生中断信号。


# 3. PMD机制
进程中主动调用`rte_eth_rx_burst`函数收取数据包，最多收取nb_pkts个包，每个数据包都放在ret_mbuf的结构体中，这时数据包已经在内存中了，这里的收包只是把相应的rte_mbuf的指针放在rx_pkts中，返回收到包的个数。

```c
static inline uint16_t
rte_eth_rx_burst(uint8_t port_id, uint16_t queue_id,
		 struct rte_mbuf **rx_pkts, const uint16_t nb_pkts)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];

    /*****此处省略****/

	int16_t nb_rx = (*dev->rx_pkt_burst)(dev->data->rx_queues[queue_id],
			rx_pkts, nb_pkts);

    /*****此处省略****/

	return nb_rx;
}
```

实际调用的收包函数是dev->rx_pkt_burst，每种网卡都不相同，即使同一种网卡也会因为intell针对CPU的优化而采用不同的处理。这里为了简单以rx_recv_pkts为例。

* 调用ixgbe_rx_scan_hw_ring， 把需要收取的包放在rxq的rx_stage中。
* 如果rxq->rx_tail > rxq_rx_free_trigger，表明收取了数据包，再分配nb_rx个rte_mbuf结构补充进DMA环形缓冲区中。
* 调用ixgbe_rx_fill_from_stage将rxq的rx_stage中保存的收取的包放在rx_pkts中返回。


```c
static inline uint16_t
rx_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	     uint16_t nb_pkts)
{
	struct ixgbe_rx_queue *rxq = (struct ixgbe_rx_queue *)rx_queue;
	uint16_t nb_rx = 0;

	/* Any previously recv'd pkts will be returned from the Rx stage */
	if (rxq->rx_nb_avail)
		return ixgbe_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	/* Scan the H/W ring for packets to receive */
	nb_rx = (uint16_t)ixgbe_rx_scan_hw_ring(rxq);

	/* update internal queue state */
	rxq->rx_next_avail = 0;
	rxq->rx_nb_avail = nb_rx;
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_rx);

	/* if required, allocate new buffers to replenish descriptors */
	if (rxq->rx_tail > rxq->rx_free_trigger) {
		uint16_t cur_free_trigger = rxq->rx_free_trigger;

		if (ixgbe_rx_alloc_bufs(rxq, true) != 0) {
			int i, j;
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u", (unsigned) rxq->port_id,
				   (unsigned) rxq->queue_id);

			rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
				rxq->rx_free_thresh;

			/*
			 * Need to rewind any previous receives if we cannot
			 * allocate new buffers to replenish the old ones.
			 */
			rxq->rx_nb_avail = 0;
			rxq->rx_tail = (uint16_t)(rxq->rx_tail - nb_rx);
			for (i = 0, j = rxq->rx_tail; i < nb_rx; ++i, ++j)
				rxq->sw_ring[j].mbuf = rxq->rx_stage[i];

			return 0;
		}

		/* update tail pointer */
		rte_wmb();
		IXGBE_PCI_REG_WRITE(rxq->rdt_reg_addr, cur_free_trigger);
	}

	if (rxq->rx_tail >= rxq->nb_rx_desc)
		rxq->rx_tail = 0;

	/* received any packets this loop? */
	if (rxq->rx_nb_avail)
		return ixgbe_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	return 0;
}
```
这样CPU与网卡分工合作， CPU不断接收数据包然后处理， 网卡不断接收网络包然后复制到内存，构成一个生产者-消费者的模型，共同完成处理任务。


<br/>
<div align=right>以上内容由RTOAX翻译整理自网络。
	<img src="_v_images/20200910110657842_12395.jpg" width="40"> 
</div>