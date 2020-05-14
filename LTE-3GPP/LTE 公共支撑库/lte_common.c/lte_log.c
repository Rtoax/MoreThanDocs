/*******************************************************************************
 **
 ** Copyright (c) 2005-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_log.c
 ** Description: output information for debugging
 **
 ** Current Version: 0.0
 ** Revision: 0.0.0.0
 ** Author: Jia Baolei jiabaolei@ict.ac.cn
 ** Date: 2010-12-20
 **
 ******************************************************************************/
/* Dependencies ------------------------------------------------------------- */


#include <stdarg.h>
#include <libgen.h>

#include "lte_log.h"
#include "lte_sema.h"
#include "lte_file.h"
#include <sys/socket.h>
#include <arpa/inet.h>

/* Constants ---------------------------------------------------------------- */
#define LOG_FILE_NAME "log_message.txt"
#define LOG_SOCKET_PORT 9000
#define LOGPROT_PK_MAX		65536

enum Foreground {
    FG_BLACK = 30,
    FG_RED,
    FG_GREEN,
    FG_YELLOW,
    FG_BLUE,
    FG_PURPLE,
    FG_CYAN,
    FG_WHITE
};

struct logprot_hdr {
	uint32_t	time;
	uint16_t	sys;
	uint16_t	sub;
	uint32_t	lineno;
	uint8_t		level;
	uint8_t		mode_size;
	uint16_t	file_size;
	uint16_t	func_size;
	uint16_t	body_size;	
} __attribute__((packed));


/* Types -------------------------------------------------------------------- */


/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */
static UINT8 g_log_level; /* lowest level that the module will excute*/
static UINT8 g_log_format; /* output simple debug info or complete debug info*/
static FILE *g_log_file = NULL; /* output file pointor */
/* semaphore used to make sure only one thread use g_log_file at a time*/
pthread_mutex_t g_log_mutexlock = PTHREAD_MUTEX_INITIALIZER;

INT32 trace_sock = -1;
UINT8	trace_addr[20] = "10.21.1.67";
INT32	trace_port = 12345;

char rrc_log_addr[20];
UINT32 rrc_log_port;
char si_log_addr[20];
UINT32 si_log_port;



/* Indicate whose log message will be print*/
/* one bit indicates one module*/
static UINT64 g_module_switch = 0;
UINT32 g_log_socket;
struct sockaddr_in	g_send_addr;
extern UINT16 get_sysframe();
extern UINT16 get_subframe();

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
* init the log module 
* Input:
*		style: WRITE2FILE or WRITE2STDOUT
*		format: LOG_SIMPLE or LOG_COMPLETE
*		current_level: LOG_ERR, LOG_WARRNING or LOG_INFO, indicating the lowest
*					level that the module will excute
* Output:
*		None;
* Return:
*		0:success;
*		-1:failed
 ******************************************************************************/
INT32 log_init(UINT8 style, UINT8 format, UINT8 current_level, 
	INT8 *log_ip, UINT32 log_port)
{
	UINT32 i = 0;
	
	if (current_level <= LOG_INFO) {	
		g_log_level = current_level;
	} else {
		printf("LOG module: current_level error\n");
		return -1;
	}
	if (format <= LOG_COMPLETE) {
		g_log_format = format;
	} else {
		printf("LOG module: format error");
		return -1;
	}
	if (style == WRITE2STDOUT) {
		g_log_file = stdout;
	}else if(style == WRITE2WIRESHARK){	
		g_log_file = WRITE2WIRESHARK;
		g_log_socket = socket(AF_INET, SOCK_DGRAM, 0);
		g_send_addr.sin_family = AF_INET;
		g_send_addr.sin_addr.s_addr = inet_addr(log_ip);
		g_send_addr.sin_port = htons(log_port);		
	}else {
		if (!(g_log_file = open_file(LOG_FILE_NAME, "w"))) {
			printf("LOG module: open file error\n");
			return -1;
		}
	}

	for (i = 0; i < LOG_MAX_MODULE_NAME; ++i) {
		log_open(i);
	}


	/*init rrc si and l1api socket info */
	/*init rrc si and l1api socket info */
	if (strlen((const char*)log_ip) >= 20) {
		log_msg(LOG_ERR, MAIN, "Start rrc si and l1api log error,\n");
	} else {
		strcpy(rrc_log_addr,log_ip);
		rrc_log_port = log_port + 1;
		
		strcpy(si_log_addr,log_ip);
		si_log_port = log_port + 2;

		strcpy(trace_addr, log_ip);
		trace_port = log_port + 3;
	}

	trace_sock = create_udp_socket(trace_addr, trace_port, 32564, NULL, 0);
	log_msg(LOG_SUMMARY, TRACE, "Success to create udp socket, which send addr=%s, send port=%d",
		trace_addr, trace_port);

	return 0;
	
}
/*******************************************************************************
* cleanup the log module ,release the resource
* Input:
*		None
* Output:
*		None;
* Return:
*		0:success;
*		-1:failed
 ******************************************************************************/
INT32 log_cleanup()
{
	if (g_log_file) {
		if (g_log_file != stdout) {
			if (g_log_file == WRITE2WIRESHARK){
				close(g_log_socket);
			}else{
				close_file(g_log_file);
				g_log_file = NULL;
			}
		}
	}
	return 0;
}
/*******************************************************************************
* turn on the log print of one module 
* Input:
*		module :name of module
* Output:
*		None;
* Return:
*		None;
 ******************************************************************************/
void log_open(UINT8 module) 
{
	if (module < LOG_MAX_MODULE_NAME) {

		g_module_switch |= 1 << module;
	} else {
		printf("LOG module , module name invalid\n");
	}
}
/*******************************************************************************
* turn off the log print of one module 
* Input:
*		module :name of module
* Output:
*		None;
* Return:
*		None;
 ******************************************************************************/
void log_close(UINT8 module) 
{
	if (module < LOG_MAX_MODULE_NAME) {

		g_module_switch &= ~(1 << module);
	} else {
		printf("LOG module , module name invalid\n");
	}
}

/*******************************************************************************
* output the debug information, this function won't be used directly by users
* Input:
*		level: the output level
*		*fmt: varable argument
*		...: optional argument
* Output
*		None;
* Return:
*		0:success;
*		-1:failed
 ******************************************************************************/
INT32 log_msg_i(UINT8 level, UINT8 module, const char *mdstr, const char *file, INT32 lineno,
					const char *func, const char *fmt,...)
{
	char *title = NULL;
	int color = FG_BLACK;
	int port_index,length = 0;
	uint8_t logbuf[LOGPROT_PK_MAX];
	uint16_t fnm_sz = 0;
	uint16_t func_sz = 0;
	uint16_t body_sz = 0;
	uint8_t mode_size = 0;
	va_list arg;
	struct logprot_hdr *hdr;
	uint8_t *lpp;
	if (level <= g_log_level ) { 
		if (g_log_file != NULL) {
			if(g_log_file == WRITE2WIRESHARK){			
				if (g_module_switch & ((UINT64)1 << module)) {
					switch (level){
	     			   case LOG_ERR:
						   port_index = 0 ;
				           break;
				
				       case LOG_WARNING:
						   port_index = 1;
				           break;
						
					   case LOG_SUMMARY:
						   port_index = 2;
						   break;
				
				       case LOG_INFO:
					   	   port_index = 3;
				           break;			
					}

					/* lua Proto identity , 03:log, 02: rrc, 01:s1ap */
					logbuf[0]= 0x00;
					logbuf[1]= 0x03;
					
					hdr = (struct logprot_hdr *)(&logbuf[2]);

					lpp = (uint8_t *)(hdr + 1);

					hdr->time = htonl(time(NULL));
					hdr->level = (uint8_t)port_index;
					hdr->lineno = htonl(lineno);
					hdr->sys = htons(get_sysframe());
					hdr->sub = htons(get_subframe());
					mode_size = strlen(mdstr);					
					hdr->mode_size = mode_size;				
					fnm_sz = strlen(file);
					hdr->file_size = htons(fnm_sz);
					func_sz = strlen(func);
					hdr->func_size = htons(func_sz);
					memcpy(lpp, mdstr, mode_size);
					lpp += mode_size;
					memcpy(lpp, file, fnm_sz);
					lpp += fnm_sz;
					memcpy(lpp, func, func_sz);
					lpp += func_sz;
					va_start(arg, fmt);
					body_sz = vsnprintf((char *)lpp, LOGPROT_PK_MAX - sizeof(*hdr) - mode_size - fnm_sz - func_sz,fmt, arg);
					va_end(arg);
					if (body_sz < 0)
						return;
					lpp += body_sz;
					hdr->body_size = htons(body_sz);
					pthread_mutex_lock(&g_log_mutexlock);					
					//length += sprintf(logbuf+length,"%d%s[%s]%s:%d in %s:\n  ",t,title, mdstr, file,lineno,func);
				//	length += vsprintf(logbuf+length, fmt, arg);
					sendto(g_log_socket, 
						   logbuf,
						   lpp - logbuf,
						   0,
						   (struct sockaddr*)&g_send_addr, 
						   sizeof(struct sockaddr));
					pthread_mutex_unlock(&g_log_mutexlock);
				}

			}else{
				if (g_module_switch & ((UINT64)1 << module)) {
					switch (level)
	 				   {
		     			   case LOG_ERR:
		          			  title = "ERROR:";
		          			  color = FG_RED;
					            break;
					
					        case LOG_WARNING:
					            title = "WARNING:";
					            color = FG_PURPLE;
					            break;
							
							case LOG_SUMMARY:
								title = "SUMMARY:";
								color = FG_GREEN;
								break;
					
					        case LOG_INFO:
					            title = "INFO:";
					            color = FG_WHITE;
					            break;
		
						
						}
					pthread_mutex_lock(&g_log_mutexlock);
					if (g_log_format == LOG_COMPLETE) {
#if defined(HAVE_COLOR)
					if (g_log_file == stdout || g_log_file == stderr)
						fprintf(g_log_file, "\033[0;%dm", color);
#endif
#if defined(LOG_SHORT_FILENAME)
					fprintf(g_log_file, "\n%s[%s]%s:%d in %s: \n",title, mdstr, basename(file),lineno,func);
#else
					fprintf(g_log_file, "\n%s[%s]%s:%d in %s: \n",title, mdstr, file,lineno,func);
#endif
				}
				va_list ap;
				va_start(ap, fmt);
				vfprintf(g_log_file, fmt, ap);
#if defined(HAVE_COLOR)
				if (g_log_file == stdout || g_log_file == stderr)
					fprintf(g_log_file, "\033[0m");
#endif
					va_end(ap);
					fflush(g_log_file);
					pthread_mutex_unlock(&g_log_mutexlock);
				}// else module not open
			}
		} else {
			printf("LOG module not initiated:output file is not opened\n");
			return -1;
		}
	} //else level not open
	return 0;
}

void log_data(UINT8 *buf, UINT32 len)
{
	sendto(g_log_socket, buf, len, 0, (struct sockaddr*)&g_send_addr, sizeof(struct sockaddr));
}


UINT8 get_log_level(){
	return g_log_level;
}


