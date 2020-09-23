<div align=center>
	<img src="_v_images/20200915124435478_1737.jpg" width="600"> 
</div>
<br/>
<br/>

<center><font size='20'>Linux 监视和调整Linux网络协议栈：发送数据</font></center>
<br/>
<br/>
<center><font size='5'>RToax</font></center>
<center><font size='5'>2020年9月</font></center>
<br/>
<br/>
<br/>
<br/>

[Linux 监视和调整Linux网络协议栈：发送数据](https://blog.packagecloud.io/eng/2017/02/06/monitoring-tuning-linux-networking-stack-sending-data/)


这篇博客文章解释了运行Linux内核的计算机如何发送数据包，以及如何在数据包从用户程序流到网络硬件时监视和调整网络堆栈的每个组件。

这篇文章与我们之前的文章《监视和调整Linux网络堆栈：接收数据》形成了一对。
如果不阅读内核的源代码并且对正在发生的事情有深刻的了解，就不可能调整或监视Linux网络堆栈。

希望该博客文章可以为希望这样做的任何人提供参考。

# 有关监视和调整Linux网络堆栈的一般建议
正如我们在上一篇文章中提到的，Linux网络堆栈很复杂，没有一种适合所有解决方案的监视或调整解决方案。如果您确实想调整网络堆栈，则别无选择，只能花费大量的时间，精力和金钱来了解网络系统各部分之间的交互方式。

本博客文章中提供的许多示例设置仅用于说明目的，不建议或反对某些配置或默认设置。在调整任何设置之前，您应该围绕需要监视的内容建立参考框架，以注意到有意义的更改。

通过网络连接到机器时调整网络设置很危险；您可以轻松地将自己锁定在外，或者完全断开网络连接。不要在生产机器上调整这些设置；取而代之的是，对新机器进行调整，然后将其轮流投入生产。








<br/>
<div align=right>以上内容由RTOAX翻译整理自网络。
	<img src="_v_images/20200915124533834_25268.jpg" width="40"> 
</div>