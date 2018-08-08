# enet_demo
学习了几天的enet，写了简单的demo。


demo中使用的是enet 1.3.13，根据官方教程编写而成，其中有几处和官方不太一致的地方：

1）心跳机制，编写时，测试发现如果暂停10s左右，server端就会收到disconnect事件，然后连接断开，尝试了enet_peer_ping函数来维持链接，但是发现无效果，没办法只能自己手动增加心跳机制；

2）发送时，总是出现最后的几十个包发不出去，server端接收不到，没办法就开始瞎试，最终改成了以下这样：

  // 发送前后各调用一次enet_host_service
  enet_host_service (gclient, &event, 0);
	enet_peer_send (gsendpeer, 0, packet);
  enet_host_service (gclient, &event, 0);

