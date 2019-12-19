/*******************************************************************************
 **
 ** Copyright (c) 2007-2012 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_socket.c
 ** Description: Sourse file for TCP/UDP transmission.
 **
 ** Current Version: 1.2
 ** $Revision: 4.1 $
 ** Author: Wang Guoqiang (wangguoqiang@ict.ac.cn)
 ** Date: 2012.09.26
 ******************************************************************************/
/* Dependencies ------------------------------------------------------------- */
#include "lte_socket.h"
#include "lte_malloc.h"
#include "lte_log.h"
#include <fcntl.h>

/* Constants ---------------------------------------------------------------- */
#define BUFF_SIZE 10
/* Types -------------------------------------------------------------------- */
/* Macros ------------------------------------------------------------------- */
/* Globals ------------------------------------------------------------------ */
UdpSocket g_udp_socket[MAX_UDP_NUM]; /*store all the udp socket.*/

static TcpServerSocket g_tcp_server_socket[MAX_TCP_SERVER]; /*store all the tcp server socket.*/

static TcpClientSocket g_tcp_client_socket[MAX_TCP_CLIENT]; /*store all the tcp client socket.*/

static UINT8 *tcp_buff[BUFF_SIZE] = {NULL}; /*the buffer used to receive tcp message.*/

static UINT8 *udp_buff[BUFF_SIZE] = {NULL}; /*the buffer used to receive udp message.*/

static fd_set read_set; /*the data set stored all the socket and used to receive message in the form of non-blocking.*/

static INT32 max_socket = 0; /*the max number in all the socket. It is used to select for linux.*/

static INT32 tcp_buff_num = 0; /*the id of tcp buffer.*/
/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
 * init the socket mode include malloc the space and initialize the some global variable.
 *
 * Input:
 *		none.
 * Output:
 *		none.
 * return:
 *		success: 0.
 *		fail: others.
*******************************************************************************/
INT32 init_socket()
{
	INT32 i = 0 ;

	/*init the socket*/
//	memset(g_udp_socket, 0, MAX_UDP_NUM * sizeof(UdpSocket));
	for (i = 0; i < MAX_UDP_NUM; i++) {
		g_udp_socket[i].udp_index = i;
		g_udp_socket[i].socket_conn = -1;
	}

//	memset(g_tcp_server_socket, 0, MAX_TCP_SERVER * sizeof(TcpServerSocket));
	for (i = 0; i < MAX_TCP_SERVER; i++) {
		g_tcp_server_socket[i].server_index = i;
	}

//	memset(g_tcp_client_socket, 0, MAX_TCP_CLIENT * sizeof(TcpClientSocket));
	for (i = 0; i < MAX_TCP_CLIENT; i++) {
		g_tcp_client_socket[i].client_index = i;
	}

	/*init the rx buffer*/
	for (i = 0; i < BUFF_SIZE; i++) {
		tcp_buff[i] = (UINT8 *)lte_malloc(MAX_TCP_DATA_LEN);
		if (NULL == tcp_buff[i]) {
			perror("init socket malloc:");
			return -1;
		}
		memset(tcp_buff[i], 0, sizeof(MAX_TCP_DATA_LEN));

		udp_buff[i] = (UINT8 *)lte_malloc(MAX_UDP_DATA_LEN);
		if (NULL == udp_buff[i]) {
			perror("init socket malloc:");
			return -1;
		}
		memset(udp_buff[i], 0, sizeof(MAX_UDP_DATA_LEN));
	}

	FD_ZERO(&read_set);
	max_socket = 0;
	return 0;
}

/*******************************************************************************
 * cleanup the socket mode include close all sockets and free the space.
 *
 * Input:
 *		none.
 * Output:
 *		none.
 * return:
 *		success: 0.
 *		fail: others.
*******************************************************************************/
INT32 clean_socket()
{
	INT32 i = 0, j = 0;

	FD_ZERO(&read_set);

	/*clean all the udp socket.*/
	for (i = 0; i < MAX_UDP_NUM; i++) {
		if (0 != g_udp_socket[i].socket_conn){
			close(g_udp_socket[i].socket_conn);
		}
	}

	/*clean all the tcp server socket.*/
	for (i = 0; i < MAX_TCP_SERVER; i++) {
		/*clean the a server's clients socket*/
		for (j = 0; j < MAX_TCP_USER; j++) {
			if (0 != g_tcp_server_socket[i].client_socket[j]) {
				close(g_tcp_server_socket[i].client_socket[j]);
			}
		}

		/*clean the server socket.*/
		if (0 != g_tcp_server_socket[i].server_socket) {
			close(g_tcp_server_socket[i].server_socket);
		}
	}

	/*clean all the tcp client socket.*/
	for (i = 0; i < MAX_TCP_CLIENT; i++) {
		if (0 != g_tcp_client_socket[i].server_socket) {
			close(g_tcp_client_socket[i].server_socket);
		}
	}

	/*clean all the socket buffer.*/
	for (i = 0; i < BUFF_SIZE; i++) {
		if (NULL != tcp_buff[i]) {
			lte_free(tcp_buff[i]);
			tcp_buff[i] = NULL;
		}

		if (NULL != udp_buff[i]) {
			lte_free(udp_buff[i]);
			udp_buff[i] = NULL;
		}
	}

	return 0;
}

/*******************************************************************************
 * create a udp socket.
 *
 * Input:
 *		send_addr : the target address of sending message.
 *		send_port : the target port of sending message.
 *		recv_port : the source port of receiving message.
 *		rx_deal_func : the deal function when the socket receive a message.
 *		rx_type : the operation of receiving message is done together or not. you can type SOCKET_SINGLE or SOCKET_TOGETHER
 * Output:
 *		socket_index : the id of udp socket.
 * return:
 *		success: >0.
 *		fail: -1.
*******************************************************************************/
INT32 create_udp_socket(UINT8 *send_addr, INT32 send_port, INT32 recv_port, SOCK_FUNC rx_deal_func, SocketRxType rx_type)
{
	INT32 option = 1, bindResult = 0, ret = 0;
	static INT32 current_udp_num = 0;
	struct sockaddr_in recv_udp_addr;
	UINT32 flags;

	ret = current_udp_num;
	while (g_udp_socket[current_udp_num].socket_conn != -1){
		current_udp_num = (current_udp_num + 1) % MAX_UDP_NUM;
		if (current_udp_num == ret){ //System is having the max udp sockets.
			log_msg(LOG_ERR, 0, "Have no free space to create a udp socket.\n");
			return -1;
		}
	}
	ret = current_udp_num;

	g_udp_socket[current_udp_num].socket_conn = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == g_udp_socket[current_udp_num].socket_conn) {
		perror("socket:");
		exit(-1);
	}
	
	flags = fcntl(g_udp_socket[current_udp_num].socket_conn, F_GETFL, 0);		
	fcntl(g_udp_socket[current_udp_num].socket_conn, F_SETFL, flags|O_NONBLOCK);	

	setsockopt(g_udp_socket[current_udp_num].socket_conn, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

	/*init recieve addresses.*/
	memset(&recv_udp_addr, 0, sizeof(struct sockaddr_in));
	recv_udp_addr.sin_family = AF_INET;
	recv_udp_addr.sin_addr.s_addr = htonl(INADDR_ANY); //local address.
	recv_udp_addr.sin_port = htons(recv_port);

	bindResult = bind(g_udp_socket[current_udp_num].socket_conn, (struct sockaddr *)&recv_udp_addr, sizeof(recv_udp_addr));
	if (-1 == bindResult) {
		perror("udp bind:");
		close(g_udp_socket[current_udp_num].socket_conn);
		g_udp_socket[current_udp_num].socket_conn = 0;
		exit(-1);
	}

	/*init sned addresses.*/
//	memset(&g_udp_socket[current_udp_num].send_addr, 0, sizeof(struct sockaddr_in));
	g_udp_socket[current_udp_num].send_addr.sin_family = AF_INET;
	g_udp_socket[current_udp_num].send_addr.sin_addr.s_addr = inet_addr(send_addr);//target address.
	g_udp_socket[current_udp_num].send_addr.sin_port = htons(send_port);

	/*the deal type when socket receive a data.*/
	g_udp_socket[current_udp_num].rx_deal_func = rx_deal_func;
	g_udp_socket[current_udp_num].rx_type = rx_type;

	if (SOCKET_TOGETHER == rx_type) {
		if (g_udp_socket[current_udp_num].socket_conn > max_socket) {
			max_socket = g_udp_socket[current_udp_num].socket_conn;
		}
		FD_SET(g_udp_socket[current_udp_num].socket_conn, &read_set);
	}

	current_udp_num = (current_udp_num + 1) % MAX_UDP_NUM;
	log_msg(LOG_INFO, TRACE, "Success to create udp socket, which send addr=%s, send port=%d, "
			"recv port = %d \n", send_addr, send_port, recv_port);
	return ret;
}

INT32 create_udp_socket_simple(INT32 bind_port)
{
	INT32 socket_fd = -1;
	INT32 option = 1, bindResult = 0;
	struct sockaddr_in recv_udp_addr;
	UINT32 flags;

	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == socket_fd) {
		perror("socket_fd:");
		exit(-1);
	}
	
	flags = fcntl(socket_fd, F_GETFL, 0);		
	fcntl(socket_fd, F_SETFL, flags|O_NONBLOCK);	

	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

	/*init recieve addresses.*/
	memset(&recv_udp_addr, 0, sizeof(struct sockaddr_in));
	recv_udp_addr.sin_family = AF_INET;
	recv_udp_addr.sin_addr.s_addr = htonl(INADDR_ANY); //local address.
	recv_udp_addr.sin_port = htons(bind_port);

	bindResult = bind(socket_fd, (struct sockaddr *)&recv_udp_addr, sizeof(recv_udp_addr));
	if (-1 == bindResult) {
		perror("udp bind:");
		close(socket_fd);
		socket_fd = -1;
		exit(-1);
	}
	return socket_fd;
}



/*******************************************************************************
 * alter the send address of udp socket.
 *
 * Input:
 * 	socket_index : the id of udp socket.
 *		send_addr : the target address of sending message.
 *		send_port : the target port of sending message.
 * Output:
 *		none
 * return:
 *		success: >0.
 *		fail: -1.
*******************************************************************************/
INT32 alter_udp_addr(INT32 socket_index, UINT8 *send_addr, INT32 send_port)
{
	if (socket_index < -1 || socket_index >= MAX_UDP_NUM) {
		log_msg(LOG_ERR, HENB_TNL, "socket_index error!\n");
		return -1;
	}

	if (g_udp_socket[socket_index].socket_conn == 0) {
		log_msg(LOG_ERR, HENB_TNL, "Has no this udp socket.\n");
		return -1;
	}
	g_udp_socket[socket_index].send_addr.sin_family = AF_INET;
	g_udp_socket[socket_index].send_addr.sin_addr.s_addr = inet_addr(send_addr);//target address.
	g_udp_socket[socket_index].send_addr.sin_port = htons(send_port);

	return 0;
}

/*******************************************************************************
 * create a tcp socket.
 *
 * Input:
 *		server_addr : the tcp server address.
 *		server_port : the tcp server port.
 *		rx_deal_func : the deal function when the socket receive a message.
 *		rx_type : the operation of receiving message is done together or not. you can type SOCKET_SINGLE or SOCKET_TOGETHER
 *		tcp_type : the type of tcp ,you can type TCP_SERVER or TCP_CLIENT.
 * Output:
 *		socket_index : the id of TCP socket.
 * return:
 *		success: >0.
 *		fail: -1.
*******************************************************************************/
INT32 create_tcp_socket(UINT8 *server_addr, INT32 server_port, SOCK_FUNC rx_deal_func, SocketRxType rx_type, TcpType tcp_type)
{
	struct sockaddr_in server_sock_addr;
	static INT32 tcp_server_num = 0, tcp_client_num;
	INT32 ret = 0, option = 1;

	memset(&server_sock_addr, 0, sizeof(server_sock_addr));
	server_sock_addr.sin_addr.s_addr = inet_addr(server_addr);
	server_sock_addr.sin_family = AF_INET;
	server_sock_addr.sin_port = htons(server_port);

	if (TCP_CLIENT == tcp_type) {
		/*find a free tcp client socket*/
		ret = tcp_client_num;
		while (g_tcp_client_socket[tcp_client_num].server_socket != 0) {
			tcp_client_num = (tcp_client_num + 1) % MAX_TCP_CLIENT;
			if (tcp_client_num == ret) {
				log_msg(LOG_ERR, 0, "Have no free space to create a tcp socket.\n");
				return -1;
			}
		}
		ret = tcp_client_num;
		g_tcp_client_socket[tcp_client_num].server_socket = socket(AF_INET, SOCK_STREAM, 0);
		setsockopt(g_tcp_client_socket[tcp_client_num].server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
		if( -1 == connect(g_tcp_client_socket[tcp_client_num].server_socket,(struct sockaddr *)&server_sock_addr,
							sizeof(struct sockaddr_in))) {
				perror("connect server error!");
				close(g_tcp_client_socket[tcp_client_num].server_socket);
				exit(-1);
		 }
		log_msg(LOG_SUMMARY, HENB_TNL, "Have connect a client server!\n");

		/*config the deal function when receive a msg.*/
		g_tcp_client_socket[tcp_client_num].rx_deal_func = rx_deal_func;
		g_tcp_client_socket[tcp_client_num].rx_type = rx_type;

		if (SOCKET_TOGETHER == rx_type) {
			/*system can deal all the socket together when configured this.*/
			if (g_tcp_client_socket[tcp_client_num].server_socket > max_socket) {
				max_socket = g_tcp_client_socket[tcp_client_num].server_socket;
			}
			FD_SET(g_tcp_client_socket[tcp_client_num].server_socket, &read_set);
		}

		tcp_client_num = (tcp_client_num + 1) % MAX_TCP_CLIENT;
	} else if(TCP_SERVER == tcp_type) {
		ret = tcp_server_num;
		while (g_tcp_server_socket[tcp_server_num].server_socket != 0) {
			tcp_server_num = (tcp_server_num + 1) % MAX_TCP_SERVER;
			if (tcp_server_num == ret) {
				log_msg(LOG_ERR, 0, "Have no free space to create a tcp socket.\n");
				return -1;
			}
		}
		ret = tcp_server_num;
		g_tcp_server_socket[tcp_server_num].server_socket = socket(AF_INET, SOCK_STREAM, 0);
		setsockopt(g_tcp_server_socket[tcp_server_num].server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

		if( -1 == bind(g_tcp_server_socket[tcp_server_num].server_socket, (struct sockaddr*)&server_sock_addr,
						sizeof(server_sock_addr))) {
			perror("tcp server bind error:");
			close(g_tcp_server_socket[tcp_server_num].server_socket);
			exit(-1);
		}
		g_tcp_server_socket[tcp_server_num].rx_deal_func = rx_deal_func;
		FD_SET(g_tcp_server_socket[tcp_server_num].server_socket, &read_set);
		if (g_tcp_server_socket[tcp_server_num].server_socket > max_socket) {
			max_socket = g_tcp_server_socket[tcp_server_num].server_socket;
		}
		listen(g_tcp_server_socket[tcp_server_num].server_socket, MAX_TCP_USER);
		log_msg(LOG_SUMMARY, HENB_TNL,"Henb listening... addr=%s,port=%d\n",server_addr,server_port);
		/*system deal all the socket together when environment is tcp server.*/
		tcp_server_num = (tcp_server_num + 1) % MAX_TCP_CLIENT;
	}

	return ret;
}

/*******************************************************************************
 * close a udp socket.
 *
 * Input:
 *		socket_index : the id of the udp socket. -1 is all the udp socket.
 * Output:
 *		none
 * return:
 *		success: 0.
 *		fail: -1.
*******************************************************************************/
INT32 close_udp_socket(INT32 socket_index)
{
	INT32 i = 0;

	if (socket_index < -1 || socket_index >= MAX_UDP_NUM) {
		log_msg(LOG_ERR, HENB_TNL, "socket_index error!\n");
		return -1;
	}

	if (socket_index == -1) { // clean all the udp socket.
		for (i = 0; i < MAX_UDP_NUM; i++) {
			if (0 != g_udp_socket[i].socket_conn) {
				if (SOCKET_TOGETHER == g_udp_socket[i].rx_type) {
					FD_CLR(g_udp_socket[i].socket_conn, &read_set);
				}
				close(g_udp_socket[i].socket_conn);
				g_udp_socket[i].socket_conn = 0;
			}
		}
	} else {
		if (0 != g_udp_socket[socket_index].socket_conn) {
			if (SOCKET_TOGETHER == g_udp_socket[socket_index].rx_type) {
				FD_CLR(g_udp_socket[socket_index].socket_conn, &read_set);
			}
			close(g_udp_socket[socket_index].socket_conn);
			g_udp_socket[socket_index].socket_conn = 0;
		} else {
			log_msg(LOG_ERR, HENB_TNL, "Has no this udp socket.\n");
			return -1;
		}
	}
	return 0;
}

/*******************************************************************************
 * close a tcp server socket.
 *
 * Input:
 * 	tcp_index : the id of the tcp server. -1 is all the tcp server socket.
 *		socket_index : the id of the tcp server's client. -1 is all the tcp server's clients.
 * Output:
 *		none
 * return:
 *		success: 0.
 *		fail: -1.
*******************************************************************************/
INT32 close_tcp_server_socket(INT32 tcp_index, INT32 socket_index)
{
	INT32 i = 0, j = 0;
	if (tcp_index < -1 || tcp_index >= MAX_TCP_SERVER) {
		log_msg(LOG_ERR, HENB_TNL, "TCP server index error.\n");
		return -1;
	}
	if (socket_index < -1 || socket_index >= MAX_TCP_USER) {
		log_msg(LOG_ERR, HENB_TNL, "TCP server socket_index error.\n");
		return -1;
	}

	if (tcp_index == -1) { //delete all the tcp server
		for (i = 0; i < MAX_TCP_SERVER; i++) {
			if (0 != g_tcp_server_socket[i].server_socket) {
				for (j = 0; j < MAX_TCP_USER; j++) {
					if (0 != g_tcp_server_socket[i].client_socket[j]) {
						FD_CLR(g_tcp_server_socket[i].client_socket[j], &read_set);
						close(g_tcp_server_socket[i].client_socket[j]);
						g_tcp_server_socket[i].client_socket[j] = 0;
					}
				}
				FD_CLR(g_tcp_server_socket[i].server_socket, &read_set);
				close(g_tcp_server_socket[i].server_socket);
				g_tcp_server_socket[i].server_socket = 0;
			}
		}
	} else { //delete a tcp server
		if (0 != g_tcp_server_socket[tcp_index].server_socket) {
			if (socket_index == -1) { //delete all of a tcp server's clients
				for (j = 0; j < MAX_TCP_USER; j++) {
					if (0 != g_tcp_server_socket[tcp_index].client_socket[j]) {
						FD_CLR(g_tcp_server_socket[tcp_index].client_socket[j], &read_set);
						close(g_tcp_server_socket[tcp_index].client_socket[j]);
						g_tcp_server_socket[tcp_index].client_socket[j] = 0;
					}
				}
			} else { // delete a tcp server's client.
				if (0 != g_tcp_server_socket[tcp_index].client_socket[socket_index]) {
					FD_CLR(g_tcp_server_socket[tcp_index].client_socket[socket_index], &read_set);
					close(g_tcp_server_socket[tcp_index].client_socket[socket_index]);
					g_tcp_server_socket[tcp_index].client_socket[socket_index] = 0;
				} else {
					log_msg(LOG_ERR, HENB_TNL, "Has no this tcp socket.\n");
					return -1;
				}
			}
		} else {
			log_msg(LOG_ERR, HENB_TNL, "Has no this tcp socket.\n");
			return -1;
		}
	}

	return 0;
}

/*******************************************************************************
 * close a tcp client socket.
 *
 * Input:
 *		socket_index : the id of the tcp client socket. -1 is all the tcp client socket.
 * Output:
 *		none
 * return:
 *		success: 0.
 *		fail: -1.
*******************************************************************************/
INT32 close_tcp_client_socket(INT32 socket_index)
{
	INT32 i = 0;

	if (socket_index < -1 || socket_index >= MAX_TCP_CLIENT){
		log_msg(LOG_ERR, HENB_TNL, "TCP client socket_index error.\n");
		return -1;
	}

	if (socket_index == -1) {
		for (i = 0; i < MAX_TCP_CLIENT; i++) {
			if (0 != g_tcp_client_socket[i].server_socket) {
				if (SOCKET_TOGETHER == g_tcp_client_socket[i].rx_type) {
					FD_CLR(g_tcp_client_socket[i].server_socket, &read_set);
				}
				close(g_tcp_client_socket[i].server_socket);
				g_tcp_client_socket[i].server_socket = 0;
			}
		}
	} else {
		if (0 != g_tcp_client_socket[socket_index].server_socket) {
			if (SOCKET_TOGETHER == g_tcp_client_socket[socket_index].rx_type) {
				FD_CLR(g_tcp_client_socket[socket_index].server_socket, &read_set);
			}
			close(g_tcp_client_socket[socket_index].server_socket);
			g_tcp_client_socket[socket_index].server_socket = 0;
		} else {
			log_msg(LOG_ERR, HENB_TNL, "Has no this tcp client socket.\n");
			return -1;
		}
	}

	return 0;
}

/*******************************************************************************
 * send data through a udp socket.
 *
 * Input:
 * 	socket_index : the id of the udp socket.
 *		msg_p: the data should be send.
 *		msg_len: the length of data.
 * Output:
 *		none.
 * return:
 *		success: 0.
 *		fail: -1.
*******************************************************************************/
INT32 tx_udp_msg(INT32 socket_index, UINT8 *msg_p, INT32 msg_len)
{
	if (socket_index < 0 || socket_index >= MAX_UDP_NUM){
		log_msg(LOG_ERR, HENB_TNL, "UDP socket_index=%d error.\n",socket_index);
		return -1;
	}
	sendto(g_udp_socket[socket_index].socket_conn, msg_p, msg_len, 0,
			(struct sockaddr*)&g_udp_socket[socket_index].send_addr, sizeof(struct sockaddr));
	return 0;
}

/* mac tx ul data to 127.0.0.1 socket */
INT32 tx_ul_data2socket(INT32 socket, UINT32 port, UINT8 *msg_p, INT32 msg_len)
{
	struct sockaddr_in send_addr;

	if (socket < 0){
		log_msg(LOG_ERR, HENB_TNL, "UDP socket =%d error.\n",socket);
		return -1;
	}
	
	send_addr.sin_family = AF_INET;
	send_addr.sin_addr.s_addr = inet_addr("127.0.0.1");//target address.
	send_addr.sin_port = htons(port);

	sendto(socket, msg_p, msg_len, 0, (struct sockaddr*)&send_addr, sizeof(struct sockaddr));
	return 0;
}

/*******************************************************************************
 * receive data through a udp socket.
 *
 * Input:
 * 	socket_index : the id of the udp socket.
 * Output:
 *		msg_p_p: the data will be received.
 *		msg_len_p: the length of data.
 * return:
 *		success: 0.
 *		fail: -1.
*******************************************************************************/
INT32 rx_udp_msg(INT32 socket_index, UINT8 **msg_p_p, INT32 *msg_len_p)
{
	static INT32 udp_buff_num = 0;
	INT32 result = 0;
	if (socket_index < 0 || socket_index >= MAX_UDP_NUM){
		log_msg(LOG_ERR, HENB_TNL, "UDP socket_index error.\n");
		return -1;
	}

	result = recvfrom(g_udp_socket[socket_index].socket_conn, udp_buff[udp_buff_num], MAX_UDP_DATA_LEN,
						0, NULL,  NULL);
	if (result > 0) {
		*msg_p_p = (UINT8 *)lte_malloc(result * sizeof(UINT8));
		memcpy(*msg_p_p, udp_buff[udp_buff_num], result);
		*msg_len_p = result;
		udp_buff_num = (udp_buff_num + 1) % BUFF_SIZE;
	} else {
		log_msg(LOG_ERR, HENB_TNL, "UDP receive error.udp socket_index=%d\n", socket_index);
		perror("receive udp message:");
		return -1;
	}
	return 0;
}

/*******************************************************************************
 * receive data through a udp socket.
 *
 * Input:
 * 	socket_index : the id of the udp socket.
 * Output:
 *		msg_p_p: the data will be received.
 *		msg_len_p: the length of data.
 * return:
 *		success: 0.
 *		fail: -1.
*******************************************************************************/
inline INT32 rx_udp_buff(INT32 socket_fd, UINT8 *buff_p)
{
	INT32 result = 0;
	result = recvfrom(socket_fd, buff_p, 10000,
						0, NULL,  NULL);
	if (result > 0) {
		return result;
	} else {
		log_msg(LOG_ERR, HENB_TNL, "UDP receive error.udp socket_index=%d\n", socket_fd);
		perror("receive udp message:");
		
		if (close_udp_socket(socket_fd)) {
			perror("Error close HeNB socket failure!");
			return -1;
		}
		log_msg(LOG_SUMMARY, HENB_TNL, "close udp socket[%d]\n",socket_fd);
	}
	return -1;
}


/*******************************************************************************
 * send data through a udp socket.
 *
 * Input:
 * 	tcp_index : the id of the tcp server. -1 is all the tcp server socket.
 *		socket_index : the id of the tcp server's client. -1 is all the tcp server's clients.
 *		msg_p: the data should be send.
 *		msg_len: the length of data.
 * Output:
 *		none.
 * return:
 *		success: 0.
 *		fail: -1.
*******************************************************************************/
INT32 tx_tcp_server_msg(INT32 tcp_index, INT32 socket_index, UINT8 *msg_p, INT32 msg_len)
{
   DataBuf data_buf;

	if (tcp_index < 0 || tcp_index >= MAX_TCP_SERVER) {
		log_msg(LOG_ERR, HENB_TNL, "TCP server index error.\n");
		return -1;
	}
	if (socket_index < 0 || socket_index >= MAX_TCP_USER) {
		log_msg(LOG_ERR, HENB_TNL, "TCP server socket_index error.\n");
		return -1;
	}
//   memset(&data_buf, 0, MAX_TCP_DATA_LEN);
   data_buf.len = msg_len;
   memcpy(data_buf.buf, msg_p, msg_len);
   send(g_tcp_server_socket[tcp_index].client_socket[socket_index], &data_buf, msg_len + sizeof(INT32), 0);

   return 0;
}

/*******************************************************************************
 * receive data through a tcp server.
 *
 * Input:
 * 	tcp_index : the id of the tcp server. -1 is all the tcp server socket.
 *		socket_index : the id of the tcp server's client. -1 is all the tcp server's clients.
 * Output:
 *		msg_p_p: the data will be received.
 *		msg_len_p: the length of data.
 * return:
 *		success: 0.
 *		fail: -1.
*******************************************************************************/
INT32 rx_tcp_server_msg(INT32 tcp_index, INT32 socket_index, UINT8 **msg_p_p, INT32 *msg_len_p)
{
	INT32 recv_flag = 0, ret = 0, recv_len = 0;
	UINT8 *recv_buff = NULL;

	if (tcp_index < 0 || tcp_index >= MAX_TCP_SERVER) {
		log_msg(LOG_ERR, HENB_TNL, "TCP server index error.\n");
		return -1;
	}
	if (socket_index < 0 || socket_index >= MAX_TCP_USER) {
		log_msg(LOG_ERR, HENB_TNL, "TCP server socket_index error.\n");
		return -1;
	}

	while (1){
		if (recv_flag == 0){
			ret = recv(g_tcp_server_socket[tcp_index].client_socket[socket_index], &recv_len, 4, 0);
			if (ret <= 0){
            	log_msg(LOG_ERR, HENB_TNL, "The tcp receive data error .\n");
            	return -1;
       		}
			*msg_len_p = recv_len;
			if (recv_len > MAX_TCP_DATA_LEN) {
				log_msg(LOG_ERR, HENB_TNL, "The tcp receive data size has expired the max tcp data size .\n");
				return -1;
			} else if (recv_len <= 0) {
				log_msg(LOG_ERR, HENB_TNL, "The tcp receive data error .\n");
				return -1;
			}
			recv_buff = (UINT8 *)lte_malloc(recv_len);
			recv_flag = 1;
		}
		ret = recv(g_tcp_server_socket[tcp_index].client_socket[socket_index], tcp_buff[tcp_buff_num], recv_len, 0);
		if (ret <= 0){
			log_msg(LOG_ERR, HENB_TNL, "The tcp receive data error .\n");
			return -1;
		}
		memcpy(recv_buff + *msg_len_p - recv_len, tcp_buff[tcp_buff_num], ret);
		recv_len -= ret;
		tcp_buff_num = (tcp_buff_num + 1) % BUFF_SIZE;
		if (recv_len == 0)
			break;
	}

	*msg_p_p = recv_buff;
	return 0;
}

/*******************************************************************************
 * send data through a tcp client.
 *
 * Input:
 * 	socket_index : the id of the tcp client.
 *		msg_p: the data should be send.
 *		msg_len: the length of data.
 * Output:
 *		none.
 * return:
 *		success: 0.
 *		fail: -1.
*******************************************************************************/
INT32 tx_tcp_client_msg(INT32 socket_index, UINT8 *msg_p, INT32 msg_len)
{
	DataBuf data_buf;
	if (socket_index < 0 || socket_index >= MAX_TCP_CLIENT){
		log_msg(LOG_ERR, HENB_TNL, "TCP client socket_index error.\n");
		return -1;
	}

//	memset(&data_buf, 0, MAX_TCP_DATA_LEN);
	data_buf.len = msg_len;
	memcpy(data_buf.buf, msg_p, msg_len);
	send(g_tcp_client_socket[socket_index].server_socket, &data_buf, msg_len + sizeof(INT32), 0);

	return 0;
}

/*******************************************************************************
 * receive data through a tcp client.
 *
 * Input:
 * 	socket_index : the id of the udp socket.
 * Output:
 *		msg_p_p: the data will be received.
 *		msg_len_p: the length of data.
 * return:
 *		success: 0.
 *		fail: -1.
*******************************************************************************/
INT32 rx_tcp_client_msg(INT32 socket_index, UINT8 **msg_p_p, INT32 *msg_len_p)
{
	INT32 recv_flag = 0, ret = 0, recv_len = 0;
	UINT8 *recv_buff = NULL;

	if (socket_index < 0 || socket_index >= MAX_TCP_CLIENT){
		log_msg(LOG_ERR, HENB_TNL, "TCP client socket_index error.\n");
		return -1;
	}

	while (1){
		if (recv_flag == 0){
			ret = recv(g_tcp_client_socket[socket_index].server_socket, &recv_len, 4, 0);	
			if (ret <= 0){
            	log_msg(LOG_ERR, HENB_TNL, "The tcp receive data error .\n");
            	return -1;
       		}
			*msg_len_p = recv_len;
			if (recv_len > MAX_TCP_DATA_LEN) {
				log_msg(LOG_ERR, HENB_TNL, "The tcp receive data size has expired the max tcp data size .\n");
				return -1;
			} else if (recv_len <= 0){
				log_msg(LOG_ERR, HENB_TNL, "The tcp receive data error .\n");
				return -1;
			}
			recv_buff = (UINT8 *)lte_malloc(recv_len);
			recv_flag = 1;
		}
		ret = recv(g_tcp_client_socket[socket_index].server_socket, tcp_buff[tcp_buff_num], recv_len, 0);
		if (ret <= 0){
			log_msg(LOG_ERR, HENB_TNL, "The tcp receive data error .\n");
			return -1;
		}
		memcpy(recv_buff + *msg_len_p - recv_len, tcp_buff[tcp_buff_num], ret);
		recv_len -= ret;
		tcp_buff_num = (tcp_buff_num + 1) % BUFF_SIZE;
		if (recv_len == 0)
			break;
	}

	*msg_p_p = recv_buff;

	return 0;
}

/*******************************************************************************
 * start a thread to receive socket message in the form of non-blocking.
 *
 * Input:
 * 	none.
 * Output:
 *		none.
 * return:
 *		success: 0.
 *		fail: -1.
*******************************************************************************/
INT32 rx_socket_msg_thread()
{
	INT32 msg_len = 0, ret = 0, i = 0, j = 0, user_index = 0, user_flag = 0;
	struct sockaddr_in ue_conn_addr;
	UINT32 addr_len =  sizeof(struct sockaddr_in);
	fd_set ready_set;
	struct timeval timeout = {0, 0};
	UINT8 *msg_p = NULL;

	while (1) {
		ready_set = read_set;

		if (select(max_socket+1, &ready_set, NULL, NULL, &timeout) > 0) {

			/*receive tcp server socket.*/
			for (i = 0; i < MAX_TCP_SERVER; i++) {
				if (g_tcp_server_socket[i].server_socket == 0)
					continue;
				if (FD_ISSET(g_tcp_server_socket[i].server_socket, &ready_set)) {
					user_index = g_tcp_server_socket[i].current_num;
					user_flag = 1;
					while (g_tcp_server_socket[i].client_socket[g_tcp_server_socket[i].current_num] != 0) {
						g_tcp_server_socket[i].current_num = (g_tcp_server_socket[i].current_num + 1) % MAX_TCP_USER;
						if (g_tcp_server_socket[i].current_num == user_index) {
							log_msg(LOG_ERR, HENB_TNL, "Has reached the max client number.\n");
							user_flag = 0;
							break;
						}
					}
					if (user_flag == 1) {
						g_tcp_server_socket[i].client_socket[user_index] = accept(g_tcp_server_socket[i].server_socket,
								(struct sockaddr_in *)&ue_conn_addr, &addr_len);
						if (-1 == g_tcp_server_socket[i].client_socket[user_index]) {
							log_msg(LOG_ERR, HENB_TNL, "Error accetp UE connect:%s", strerror(errno));
							exit(1);
						}else{
							log_msg(LOG_SUMMARY, HENB_TNL,"Welcome ue[%s] idx:%d connecting to HeNB\n",
									inet_ntoa(ue_conn_addr.sin_addr), user_index);
						}
						FD_SET(g_tcp_server_socket[i].client_socket[user_index], &read_set);
						if (g_tcp_server_socket[i].client_socket[user_index] > max_socket)
							max_socket = g_tcp_server_socket[i].client_socket[user_index];

						g_tcp_server_socket[i].current_num = (g_tcp_server_socket[i].current_num + 1) % MAX_TCP_USER;
					}
				}
				for (j = 0; j < MAX_TCP_USER; j++) {
					if (g_tcp_server_socket[i].client_socket[j]!= 0 && FD_ISSET(g_tcp_server_socket[i].client_socket[j], &ready_set)) {
						msg_p = NULL;
						msg_len = 0;
						ret = rx_tcp_server_msg(i, j, &msg_p, &msg_len);
						if (ret != 0) {
							if (close_tcp_server_socket(i, j)) {
								perror("Error close HeNB socket failure!");
								exit(1);
							}
							log_msg(LOG_SUMMARY, HENB_TNL, "close tcp server client socket[%d][%d]\n",i,j);
							continue;
						} // if(rece_len)
						g_tcp_server_socket[i].rx_deal_func(j, msg_p, msg_len);
					} //if FD_ISSET
				} //for the tcp server's client
			} // for the tcp server

			/*receive tcp client*/
			for (i = 0; i < MAX_TCP_CLIENT; i++) {
				if (g_tcp_client_socket[i].server_socket !=0 && g_tcp_client_socket[i].rx_type == SOCKET_TOGETHER &&
						FD_ISSET(g_tcp_client_socket[i].server_socket, &ready_set)) {
					msg_p = NULL;
					msg_len = 0;
					ret = rx_tcp_client_msg(i,&msg_p, &msg_len);
					if (ret != 0) {
						if (close_tcp_client_socket(i)) {
							perror("Error close HeNB socket failure!");
							exit(1);
						}
						log_msg(LOG_SUMMARY, HENB_TNL, "close tcp client socket[%d]\n",i);
						continue;
					} // if(rece_len)
					g_tcp_client_socket[i].rx_deal_func(i, msg_p, msg_len);
				} //if FD_ISSET
			} //for the tcp client

			/*receive udp client*/
			for (i = 0; i < MAX_UDP_NUM; i++) {
				if (g_udp_socket[i].socket_conn !=0 && g_udp_socket[i].rx_type == SOCKET_TOGETHER &&
						FD_ISSET(g_udp_socket[i].socket_conn, &ready_set)) {
					msg_p = NULL;
					msg_len = 0;
					ret = rx_udp_msg(i, &msg_p, &msg_len);
					if (ret != 0) {
						if (close_udp_socket(i)) {
							perror("Error close HeNB socket failure!");
							exit(1);
						}
						log_msg(LOG_SUMMARY, HENB_TNL, "close udp socket[%d]\n",i);
						continue;
					} // if(rece_len)
					if(NULL != g_udp_socket[i].rx_deal_func) {
						g_udp_socket[i].rx_deal_func(i, msg_p, msg_len);
					}
				} //if FD_ISSET
			} //for the tcp client

		} //if select
	}

	return 0;
}
