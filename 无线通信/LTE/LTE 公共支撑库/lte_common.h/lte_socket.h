/*******************************************************************************
 **
 ** Copyright (c) 2007-2012 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_socket.h
 ** Description: Header file for TCP/UDP transmission..
 **
 ** Current Version: 1.2
 ** $Revision: 4.1 $
 ** Author: Wang Guoqiang (wangguoqiang@ict.ac.cn)
 ** Date: 2012.09.26
 ******************************************************************************/

#ifndef LTE_SOCKET_H_
#define LTE_SOCKET_H_
/*******************************************************************************
 **
 Declaration for all
 **
 ******************************************************************************/

/* Dependencies ------------------------------------------------------------- */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "lte_type.h"
/* Constants ---------------------------------------------------------------- */
typedef INT32 (*SOCK_FUNC)(INT32 index, UINT8 *msg_p, INT32 msg_len);

#define MAX_UDP_NUM 10

#define MAX_UDP_DATA_LEN 10000

#define MAX_TCP_DATA_LEN 30000

#define MAX_TCP_USER 5 /*the max user number that one tcp server has.*/

#define MAX_TCP_SERVER 3 /*the max tcp server number that one can create. */

#define MAX_TCP_CLIENT 5 /*the max tcp client number that one can create. */

#define MTU      1500     /* MTU */

/* mac->rlc socket */
#define UL_DATA_RX_SOCKET_PORT 4000

/* dlfapi -> ulfapi socket */
#define UL_DATA_FAPI_SOCKET_PORT 4001

/* param set command  */
#define PARAM_SET_CMD_SOCKET_PORT 4003

/* Types -------------------------------------------------------------------- */
typedef enum {
	SOCKET_SINGLE = 0, // user uses rx_msg by himself.
	SOCKET_TOGETHER = 1 // system deal all the socket and call the function according the data structure.
} SocketRxType;

typedef struct {
	INT32 udp_index; // form 0 to MAX_UDP SOCKET-1
	INT32 socket_conn; // the socket indication of system.
	struct sockaddr_in send_addr;
	SOCK_FUNC rx_deal_func;
	SocketRxType rx_type;
} UdpSocket;


typedef struct {
	INT32 server_index; // form 0 to MAX_TCP_SERVER-1
	INT32 client_socket[MAX_TCP_USER]; // store the server's clients.
	INT32 server_socket;
	INT32 current_num; // the server has current_num+1 clients now.
	SOCK_FUNC rx_deal_func;
} TcpServerSocket;


typedef struct {
	INT32 client_index; // form 0 to MAX_TCP_CLIENT-1
	INT32 server_socket;
	SOCK_FUNC rx_deal_func;
	SocketRxType rx_type;
} TcpClientSocket;

typedef struct {
	INT32 len;
	UINT8 buf[MAX_TCP_DATA_LEN - 4];
} DataBuf;

typedef enum {
	TCP_SERVER= 0,
	TCP_CLIENT = 1
} TcpType;
/* Macros ------------------------------------------------------------------- */
#ifdef SOCKET_TCP_SERVER  /*when the connection type is TCP server between enodeb and ue.*/

#define create_lte_tcp(server_addr, server_port, rx_deal_fun) \
	create_tcp_socket(server_addr, server_port, rx_deal_fun, SOCKET_TOGETHER, TCP_SERVER)

#define tx_msg_between_lte(socket_index, msg_p, msg_len) \
	tx_tcp_server_msg(0, socket_index, msg_p, msg_len)

#define rx_msg_between_lte(socket_index, msg_p_p, msg_len_p) \
	rx_tcp_server_msg(0, socket_index, msg_p_p, msg_len_p)

#define close_socket_between_lte() \
	close_tcp_server_socket(0, -1)

#elif SOCKET_TCP_CLIENT  /*when the connection type is TCP client between enodeb and ue.*/

#define create_lte_tcp(server_addr, server_port, rx_deal_fun) \
	create_tcp_socket(server_addr, server_port, rx_deal_fun, SOCKET_TOGETHER, TCP_CLIENT)

#define tx_msg_between_lte(socket_index, msg_p, msg_len) \
	tx_tcp_client_msg(socket_index, msg_p, msg_len)

#define rx_msg_between_lte(socket_index, msg_p_p, msg_len_p) \
	rx_tcp_client_msg(socket_index, msg_p_p, msg_len_p)

#define close_socket_between_lte() \
	close_tcp_client_socket(0);

#else  /*when the connection type is UDP between enodeb and ue.*/

#define tx_msg_between_lte(socket_index, msg_p, msg_len) \
	tx_udp_msg(socket_index, msg_p, msg_len)

#define rx_msg_between_lte(socket_index, msg_p_p, msg_len_p) \
	rx_udp_msg(socket_index, msg_p_p, msg_len_p)

#define close_socket_between_lte() \
	close_udp_socket(-1)

#endif

/*packet other udp socket.*/
#define tx_msg_to_kernel(socket_index, msg_p, msg_len) \
	tx_udp_msg(socket_index, msg_p, msg_len)

#define rx_msg_from_kernel(socket_index, msg_p, msg_len) \
	rx_udp_msg(socket_index, msg_p, msg_len)

#define close_kernel_socket(socket_index) \
	close_udp_socket(socket_index)

#define create_lte_udp(send_addr, send_port, recv_port, rx_deal_fun) \
	create_udp_socket(send_addr, send_port, recv_port, rx_deal_fun, SOCKET_TOGETHER)

#define create_kernel_socket(send_addr, send_port, recv_port, rx_deal_fun) \
	create_udp_socket(send_addr, send_port, recv_port, rx_deal_fun, SOCKET_TOGETHER)

/* Globals ------------------------------------------------------------------ */
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
INT32 init_socket();

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
INT32 clean_socket();

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
INT32 create_udp_socket(UINT8 *send_addr, INT32 send_port, INT32 recv_port, SOCK_FUNC rx_deal_func, SocketRxType rx_type);

/*******************************************************************************
 * alter the send address of udp socket. alter：改变，更改
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
INT32 alter_udp_addr(INT32 socket_index, UINT8 *send_addr, INT32 send_port);
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
INT32 create_tcp_socket(UINT8 *server_addr, INT32 server_port, SOCK_FUNC rx_deal_func, SocketRxType rx_type, TcpType tcp_type);

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
INT32 close_udp_socket(INT32 socket_index);

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
INT32 close_tcp_server_socket(INT32 tcp_index, INT32 socket_index);

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
INT32 close_tcp_client_socket(INT32 socket_index);

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
INT32 tx_udp_msg(INT32 socket_index, UINT8 *msg_p, INT32 msg_len);

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
INT32 rx_udp_msg(INT32 socket_index, UINT8 **msg_p_p, INT32 *msg_len_p);

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
INT32 tx_tcp_server_msg(INT32 tcp_index, INT32 socket_index, UINT8 *msg_p, INT32 msg_len);

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
INT32 rx_tcp_server_msg(INT32 tcp_index, INT32 socket_index, UINT8 **msg_p_p, INT32 *msg_len_p);

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
INT32 tx_tcp_client_msg(INT32 socket_index, UINT8 *msg_p, INT32 msg_len);

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
INT32 rx_tcp_client_msg(INT32 socket_index, UINT8 **msg_p_p, INT32 *msg_len_p);

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
INT32 rx_socket_msg_thread();


#endif /* LTE_SOCKET_H_ */
