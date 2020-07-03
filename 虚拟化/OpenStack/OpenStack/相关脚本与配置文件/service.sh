/etc/init.d/networking restart  #重启网络
service ntp restart		#重启NTP服务
service mysql restart		#重启mysql服务
service keystone restart	#重启keystone服务
service glance-api restart 	#重启glance-api服务 
service glance-registry restart	#重启glance-registry服务

#停止和重启nova相关服务
service libvirt-bin restart
service nova-network restart
service nova-compute restart
service nova-api restart      	
service nova-objectstore restart
service nova-scheduler restart
service novnc restart 
service nova-volume restart
service nova-consoleauth restart


service apache2 restart		#重启apache2服务
