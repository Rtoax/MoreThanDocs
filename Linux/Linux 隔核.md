<center><font size='6'>Linux 如何隔离CPU核心 isolcpus=0-2</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年5月8日</font></center>
<br/>


# 1. tuned调优方式隔核
## 1.1. 首先查看当前调优方式

```
# tuned-adm active 
Current active profile: realtime-virtual-host
```

## 1.2. tuned查看可用的调优方式

```
# tuned-adm profile 
Available profiles:
- balanced                    - General non-specialized tuned profile
- desktop                     - Optimize for the desktop use-case
- latency-performance         - Optimize for deterministic performance at the cost of increased po
wer consumption- network-latency             - Optimize for deterministic performance at the cost of increased po
wer consumption, focused on low latency network performance- network-throughput          - Optimize for streaming network throughput, generally only necessar
y on older CPUs or 40G+ networks- powersave                   - Optimize for low power consumption
- realtime                    - Optimize for realtime workloads
- realtime-virtual-guest      - Optimize for realtime workloads running within a KVM guest
- realtime-virtual-host       - Optimize for KVM guests running realtime workloads
- throughput-performance      - Broadly applicable tuning that provides excellent performance acro
ss a variety of common server workloads- virtual-guest               - Optimize for running inside a virtual guest
- virtual-host                - Optimize for running KVM guests
Current active profile: realtime-virtual-host
```

## 1.3. 查看系统存在调优方式的配置文件

```
# ls /etc/tuned/
active_profile  profile_mode             realtime-virtual-guest-variables.conf  recommend.d
bootcmdline     realtime-variables.conf  realtime-virtual-host-variables.conf   tuned-main.conf
```

## 1.4. 修改tuned 某一种调优方式的配置文件profile


```
# vim realtime-virtual-host-variables.conf

  1 # Examples:
  2 # isolated_cores=2,4-7
  3 # isolated_cores=2-23
  4 #
  5 isolated_cores=0

```
通过修改isolated_cores的数字来隔离CPU

## 1.5. tuned 生效一种调优方式

```
tuned-adm profile realtime-virtual-host
```

## 1.6. 重启生效配置

```
reboot
```

# 2. 修改grub配置文件

隔核调优方式也可以直接在`/etc/default/grub`中修改。

```
vim /etc/default/grub

GRUB_CMDLINE_LINUX="crashkernel=auto rd.lvm.lv=centos/root rd.lvm.lv=centos/swap no_timer_check clocksource=tsc tsc=perfect intel_pstate=disable selinux=0 enforcing=0 nmi_watchdog=0 softlockup_panic=0 isolcpus=1-39 nohz_full=0-39 idle=poll default_hugepagesz=1G hugepagesz=1G hugepages=16 rcu_nocbs=1-39 kthread_cpus=0 irqaffinity=0 rcu_nocb_poll rhgb quiet"
```

修改grub配置文件后需要更新grub配置文件，运行一下命令
```
grub2-mkconfig -o /boot/grub2/grub.cfg
```

然后重启生效
```
reboot
```


# 3. 查看隔核情况

```
$ more /proc/cmdline 
BOOT_IMAGE=/vmlinuz-3.10.0-1062.el7.x86_64 root=/dev/mapper/centos-root ro crashkernel=auto rd.lvm.lv=centos/root r
d.lvm.lv=centos/swap rhgb quiet skew_tick=1 isolcpus=2-3 intel_pstate=disable nosoftlockup
```
其中`isolcpus=2-3`即为隔离的CPU。

