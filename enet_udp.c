#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <enet/enet.h>


#define BUFF_LEN 4096
#define ENET
// #define UDP

#ifdef ENET
ENetHost * gclient;
ENetPeer * gsendpeer;

ENetHost * gserver;
ENetPeer * grecvpeer;

char *heartbeat = "heartbeat";
#endif

char *peer_sip = NULL;
short peer_sport = 0;
char *local_sip = NULL;
short local_sport = 0;

int send_totalnumber = 0;
int recv_totalnumber = 0;
int lostnumber = 0;
int outordernumber = 0;
int wrongnumber = 0;
int send_flag = 0;
int recv_flag = 0;
int sstoped = 0;
int rstoped = 0;

void data_dump(char *buf, int len)
{
	int i = 0;
	for ( i=0; i<len; i++ )
	{
		printf("0x%02x ", (unsigned char)buf[i]);
		if ( 0 == (i+1)%16)
		{
			printf("\n");
		}
	}
	printf("-----------------------------------\n\n\n");
}

// 丢包检查函数,返回值,0:未丢包,其他:丢包
int data_lostcheck(char *buf, int len, int seq)
{
	
	int currentseq = *((unsigned int *)buf);
	if ( currentseq != (seq+1) )
	{
		return -1;
	}
	return 0;
}

// 乱序检查函数,返回值,0:未乱序,其他:乱序
int data_ordercheck(char *buf, int len, int seq)
{
	int currentseq = *((unsigned int *)buf);
	if ( currentseq < seq+1 )
	{
		return -1;
	}
	return 0;
}

// 误码检查
int data_contentcheck(char *buf, int len)
{
    int i = 0;
	int con = 0;
	int seq = *((unsigned int *)buf);
    for ( i=4; i<len; i+=4 )
    {
		seq++;
        con = *((unsigned int *)(buf+i));
		if ( seq != con )
		{
			return -1;
		}
    }

	return 0;
}

void data_check(char *buf, int len, int seq)
{
	int ret = 0;

	// 丢包检查函数,返回值,0:未丢包,其他:丢包
	ret = data_lostcheck(buf, len, seq);
	if ( 0 != ret )
	{
		lostnumber++;
	}

	// 乱序检查函数,返回值,0:未乱序,其他:乱序
	ret = data_ordercheck(buf, len, seq);
	if ( 0 != ret )
	{
		outordernumber++;
	}

	// 误码检查
	ret = data_contentcheck(buf, len);
	if ( 0 != ret )
	{
		wrongnumber++;
	}
}

// 数据填充函数
void data_fill(char *buf, int len, int seq)
{
    memset(buf, 0, len);			//memset 将buf后面所指的len都变成0所指定的ASCII码
    int i = 0;
    for ( i=0; i<len; i+=4 )
    {
        *((unsigned int *)(buf+i)) = seq++;
		 
		/**((unsigned int *)(buffer+i)) = ret;
		buffer是一个指针，buffer+i也是一个指针（地址），
		然后加了一个(unsigned int *)强制转化了这个指针的类型成unsigned int *,
		然后在用了一个*得到该地址指向的空间，然后把ret赋值给它。
		
		将s所指向的某一块内存中的每个字节的内容全部设置为ch指定的ASCII值,
　　	块的大小由第三个参数指定,这个函数通常为新申请的内存做初始化工作,
　　	其返回值为指向S的指针。
		*/															
    }
}
	
int udp_client_create(char *ip, short port)
{
	int client_fd;


    client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(client_fd < 0)
    {
        printf("create socket fail!\n");
        return -1;
    }
	return client_fd;
}

int enet_client_create(char *ip, short port)
{
    ENetHost * client;
    client = enet_host_create (NULL /*  create a client host */,
            1 /*  only allow 1 outgoing connection */,
            1 /*  allow up 2 channels to be used, 0 and 1 */,
            0 /*  56K modem with 56 Kbps downstream bandwidth */,
            0 /*  56K modem with 14 Kbps upstream bandwidth */);
    if (client == NULL)
    {
        fprintf (stderr, 
                "An error occurred while trying to create an ENet client host.\n");
        return -1;
    }

    ENetAddress address;
    ENetEvent event;
    ENetPeer *peer;

    /*  Connect to some.server.net:1234. */
    enet_address_set_host (&address, ip);
    address.port = port;

    while(1)
    {
        /*  Initiate the connection, allocating the two channels 0 and 1. */
        peer = enet_host_connect (client, &address, 1, 0);    
        if (peer == NULL)
        {
            fprintf (stderr, 
                    "No available peers for initiating an ENet connection.\n");
            sleep(1);
            continue;
        }
        /*  Wait up to 5 seconds for the connection attempt to succeed. */
        if (enet_host_service (client, & event, 5000) > 0 &&
                event.type == ENET_EVENT_TYPE_CONNECT)
        {
            puts ("Connection to server succeeded.");
            break;
        }
        else
        {
            /*  Either the 5 seconds are up or a disconnect event was */
            /*  received. Reset the peer in the event the 5 seconds   */
            /*  had run out without any significant event.            */
            enet_peer_reset (peer);
            puts ("Connection to server failed.");
            sleep(1);
            continue;
        }
    }
    enet_host_flush (client);

    gclient = client;
    gsendpeer = peer;
    return 0;
}

void enet_sendto(char *buf, int len)
{
    int ret = 0;
    ENetEvent event;
    ENetPacket *packet = enet_packet_create (buf, 
			len,
			ENET_PACKET_FLAG_RELIABLE);

    enet_host_service (gclient, &event, 0);
	enet_peer_send (gsendpeer, 0, packet);
#if 1
    ret = enet_host_service (gclient, &event, 0);
    if ( 0 != ret )
    {
        printf("send result: %d\n", ret);
        printf("event type: %d\n", event.type);
    }
#endif
	// enet_host_flush (gclient);
}

void enet_heartbeat()
{
    ENetEvent event;
    ENetPacket *packet = enet_packet_create (heartbeat, 
			strlen(heartbeat),
			ENET_PACKET_FLAG_RELIABLE);

    enet_host_service (gclient, &event, 0);
	enet_peer_send (gsendpeer, 0, packet);
    enet_host_service (gclient, &event, 0);
}

void *thread_send(void *param)
{
    int seq = 1;
    char buf[BUFF_LEN] = {0};
#ifdef ENET
    int ret = 0;
    ret = enet_client_create(peer_sip, peer_sport);
    if ( 0 != ret )
    {
        fprintf(stderr, "create enet client error\n");
        return NULL;
    }
#endif

#ifdef UDP
    struct sockaddr_in dst;
    int socklen = sizeof(dst);
    int udpsock = 0;
    // 初始化一个UDP client
    udpsock = udp_client_create(peer_sip, peer_sport);
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = inet_addr(peer_sip);
    dst.sin_port = htons(peer_sport);
#endif	 

    while(!sstoped)
    {
        if ( 1 == send_flag )
        {
            // 构造数据
            memset(buf, 0, sizeof(buf));
            data_fill(buf, sizeof(buf), seq);
            // 发送出去
#ifdef ENET
            enet_sendto(buf, sizeof(buf));
#endif

#ifdef UDP
            sendto(udpsock, buf, sizeof(buf), 0, (struct sockaddr*)&dst, socklen);
#endif
            seq++;
            send_totalnumber++;
        }
        else
        {
#ifdef ENET
            // 发送心跳
            enet_heartbeat();
#endif
            // 空转,不做实际业务
            // printf("thread send loop none\n");
            sleep(1);
        }
    } 
}

int  udp_server_create(char *ip, short port)
{
	int server_fd, ret;
    struct sockaddr_in ser_addr; 

    server_fd = socket(AF_INET, SOCK_DGRAM, 0); //AF_INET:IPV4;SOCK_DGRAM:UDP
    if(server_fd < 0)
    {
        printf("create socket fail!\n");
        return -1;
    }

    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = inet_addr(ip); //IP地址，需要进行网络序转换，INADDR_ANY：本地地址
    ser_addr.sin_port = htons(port);  //端口号，需要网络序转换

    ret = bind(server_fd, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
    if(ret < 0)
    {
        printf("socket bind fail!\n");
        return -1;
    }

	return server_fd;
}

int enet_server_create(char *ip, short port)
{
    int ret = 0;

	ENetAddress address;
	ENetHost * server;
	ENetEvent event;

	enet_address_set_host(&address, ip);
	address.port = port;
	server = enet_host_create (&address, 1, 1, 0, 0);
	if (server == NULL)
	{
		fprintf (stderr, 
				"An error occurred while trying to create an ENet server host.\n");
        return -1;
	}

    /*  Wait up to 1000 milliseconds for an event. */
    while (1)
    {
        ret = enet_host_service (server, &event, 1000);
        if ( ENET_EVENT_TYPE_CONNECT == event.type )
        {
            printf ("A new client connected from %x:%u.\n", 
                    event.peer -> address.host,
                    event.peer -> address.port);
            /*  Store any relevant client information here. */
            event.peer -> data = "Client information";
            grecvpeer = event.peer;
            break;
        }
    }

    gserver = server;
    return 0;
}

int enet_recvfrom(char *buf, int len)
{
    int packetlen = 0;
    int ret = 0;
	ENetHost * server = gserver;
	ENetEvent event;

    /*  Wait up to 1000 milliseconds for an event. */
    while (1)
    {
        ret = enet_host_service (server, &event, 100);
        if ( ENET_EVENT_TYPE_RECEIVE == event.type )
        {
#if 0
            printf ("A packet of length %u containing %s was received from %s on channel %u.\n",
                    event.packet -> dataLength,
                    event.packet -> data,
                    event.peer -> data,
                    event.channelID);
#endif
            // 心跳处理
            if ( strlen(heartbeat) == event.packet->dataLength )
            {
                continue;
            }

            memcpy(buf, event.packet->data, event.packet->dataLength);
            packetlen = event.packet->dataLength;
            /*  Clean up the packet now that we're done using it. */
            enet_packet_destroy (event.packet);
            break;
        }
        else if ( ENET_EVENT_TYPE_DISCONNECT == event.type )
        {
            printf ("%s disconnected.\n", event.peer -> data);
            /*  Reset the peer's client information. */
            event.peer -> data = NULL;
            break;
        }
    }

    return packetlen;
}

void *thread_recv(void *param)
{
    int seq = 0;
    char buf[BUFF_LEN] = {0};
    int recvlen = 0;

#ifdef ENET
    int ret = 0;
    ret = enet_server_create(local_sip, local_sport);
    if ( 0 != ret )
    {
        fprintf(stderr, "error to create enet server \n");
        return NULL;
    }
#endif

#ifdef UDP
    int udpsock = 0;
    // 初始化一个UDP Server
    udpsock = udp_server_create(local_sip, local_sport);
#endif

	while(!rstoped)
	{
		if ( 1 == recv_flag )
		{
			// 接收数据
#ifdef ENET
            recvlen = enet_recvfrom(buf, sizeof(buf));
#else
			recvlen = recvfrom(udpsock, buf, sizeof(buf), 0,  NULL, NULL);  //recvfrom是拥塞函数，没有数据就一直拥塞
#endif
			recv_totalnumber++;
			//data_dump(buf, sizeof(buf));
			//printf("recv message %d\n",seq);
			//printf("recv message %d\n",*((unsigned int *)(buf)));
			//printf("thread recv message\n");
			// 校验数据
			data_check(buf, sizeof(buf), seq);
            seq = *((unsigned int *)buf);
			//printf("recv message1 %d\n", seq);
		}
		else
		{
			// 空转,不做实际业务
			// printf("thread recv loop none\n");
			sleep(1);
		}
	}
}
 
// 运行示例: ./Test  192.168.2.10 8080 192.168.2.11 8090
int main(int argc, char **argv)
{
    if (enet_initialize () != 0)
	{
		fprintf (stderr, "An error occurred while initializing ENet.\n");
		return EXIT_FAILURE;
	}

	// argc:参数的总个数,从0开始
	if ( 5 != argc )
	{
		printf("%s, peer_sip peer_sport local_sip loca_sport \n", argv[0]);
		exit(0);
	}
	peer_sip = argv[1];
	peer_sport = atoi(argv[2]);
	local_sip = argv[3];
	local_sport = atoi(argv[4]);

    // 创建线程A
	pthread_t t0;
    pthread_t t1;

    if(pthread_create(&t0, NULL, thread_send, NULL) == -1){
        puts("fail to create pthread t0");
        exit(1);
    }

    if(pthread_create(&t1, NULL, thread_recv, NULL) == -1){
        puts("fail to create pthread t1");
        exit(1);
    }
	
	int mstoped = 0;
	while(!mstoped)
	{
		char c = getchar();
		
		switch(c)
		{
			case 's':
			{
				// 启动发送
				send_flag = 1;
			}
			break;
			case 'r':
			{
				// 启动接收
				recv_flag = 1;
			}
			break;
			case 'a':
			{
				send_flag = 0;
			}
			break;
			case 'b':
			{
				recv_flag = 0;
			}
			break;
			case 'e':
			{
				mstoped = 1;
			}
			break;
			case 'p':
			{
				printf("info: \n");
				printf("send_totalnumber:%d\n",send_totalnumber);
				printf("recv_totalnumber:%d\n",recv_totalnumber);
				printf("wrongnumber:%d\n",wrongnumber);
				printf("outordernumber: %d\n",outordernumber);
				printf("lostnumber: %d\n",lostnumber);

			}
			break;
		}
		sleep(1);
    }	
    
	sstoped = 1;
	rstoped = 1;

    // 等待线程结束
    void * result;
    if(pthread_join(t0, &result) == -1){
        puts("fail to recollect t0");
    }

    if(pthread_join(t1, &result) == -1){
        puts("fail to recollect t1");
    }

	enet_deinitialize();
}
