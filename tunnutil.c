//
//  module: device tunnel module
//      tunnutil.h
//      active to builde up tunnel with specified server, multiplex/demutiplex traffice 
//      from server to local service or vice vera
//
#include <stdio.h>      /* printf, scanf, NULL */
#include <stdlib.h>     /* malloc, free, rand */
#include <string.h>
#include <stdarg.h> 
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "tunnel.h"
#include "link.h"
#include "tunnutil.h"
#include "tunnmsg.h"

//
// tunnel control block allocate/free
//
struct list_head tunnel_free_list, tunnel_active_list;
pthread_mutex_t lock;

//
// function declearation
//
extern unsigned int get_time_milisec(void);
//
//  function: remove_active_tunnel
//      frmove a tunnel control block from active list
//      parameters
//          tunnel block pointer
//      return
//          1 success
//          0 not found;
//
int remove_active_tunnel(TUNNEL_CTRL *tc)
{
    TUNNEL_CTRL *p = NULL;
    struct list_head *head;
    
    if (tc==NULL)
        return 0;
    pthread_mutex_lock(&lock);
    head = &tunnel_active_list;    
    list_for_each_entry(p, head, list)
    {
        if (p != tc)
            continue;
        list_del(&p->list);
        pthread_mutex_unlock(&lock);
        return 1;
    }
    pthread_mutex_unlock(&lock);
    return 0;
}

//
//  function: get_free_tunnel_block
//      get a tunnel control block in free list
//      parameters
//          list head pointer
//      return
//          pointer of tunnel conrol block
//          null    not free block
//
TUNNEL_CTRL *get_free_tunnel_block()
{
    TUNNEL_CTRL *p = NULL;
    struct list_head *head;
    
    pthread_mutex_lock(&lock);
    head = &tunnel_free_list;
    if (!list_empty(head))
    {
        /*remove node from head of queue*/
        p = list_entry(head->next, TUNNEL_CTRL, list);
        if (p)
            list_del(&p->list);
    }
    pthread_mutex_unlock(&lock);
    return p;
}


//
//  function: put_tunnel_block
//      get a tunnel control block in free/active list
//      parameters
//          tc pointer,
//          list head
//      return
//           none
//
void put_tunnel_block(struct list_head *head, TUNNEL_CTRL *p)
{
    pthread_mutex_lock(&lock);
    list_add_tail(&p->list, head);
    pthread_mutex_unlock(&lock);
}

//
//  function: find_tunnel_block_by_sid
//      find active tunnel list by socket id
//      parameters
//          socket id
//      return
//          pointer of tunnel conrol block
//          null    not found
//
TUNNEL_CTRL *find_tunnel_block_by_sid(int sid)
{
    TUNNEL_CTRL *p;
    struct list_head *head;
    
    pthread_mutex_lock(&lock); 
    head = &tunnel_active_list;
    list_for_each_entry(p, head, list)
    {
        if (p->sock == sid)
        {    
            pthread_mutex_unlock(&lock);
            return p;
        }
    }
    pthread_mutex_unlock(&lock);
    return 0; /*not found*/
}


//
//  function: find_tunnel_block_by_addr
//      find active tunnel list by client address and port
//      parameters
//          client ip
//          client port
//      return
//          pointer of tunnel conrol block
//          null    not found
//
TUNNEL_CTRL *find_tunnel_block_by_addr(char *ip, unsigned short port)
{
    TUNNEL_CTRL *p;
    struct list_head *head;
    
    pthread_mutex_lock(&lock);            
    head = &tunnel_active_list;
    list_for_each_entry(p, head, list)
    {
        if (*(unsigned int *)p->ip == *(unsigned int *)ip &&
            p->port == port                                 )
        {
            pthread_mutex_unlock(&lock);
            return p;
        }
    }
    pthread_mutex_unlock(&lock);
    return 0; /*not found*/
}

//
//  function: find_tunnel_block_by_uid
//      find active tunnel list by device's uid
//      parameters
//          devcie uid
//      return
//          pointer of tunnel conrol block
//          null    not found
//
TUNNEL_CTRL *find_tunnel_block_by_uid(char *uid)
{
    TUNNEL_CTRL *p;
    struct list_head *head;
    
    pthread_mutex_lock(&lock);
    head = &tunnel_active_list;
    list_for_each_entry(p, head, list)
    {
        if (memcmp(p->uid, uid, sizeof(*p->uid))==0)
        {    
            pthread_mutex_unlock(&lock);
            return p;
        }
    }
    pthread_mutex_unlock(&lock);
    return 0; /*not found*/
}



//
//  function: get_tunntl_info
//      extract tunnel information and put in buffer
//      parameters
//          buffer start, size of buffer
//      return
//          number of data
//
int get_tunnel_info(char *buf, int bufsize)
{
    int  i, count, dev_num;
    char tbuf[2048],*cp1, *end1, *start = buf, *cp=buf, *end=&buf[bufsize-3];   // reserve last 3 bytes
    TUNNEL_CTRL *tc;
    TUNN_LINK *link;
    struct list_head *head;
    
    pthread_mutex_lock(&lock);
    head = &tunnel_active_list;
    cp += snprintf(cp, end-cp,"{\"devs\":[");
    dev_num = 0;
    list_for_each_entry(tc, head, list)
    {
        cp1 = tbuf, end1=&tbuf[2048];
        // get links
        count = 0;
        for(i=0;i<MAX_LINK_NUM && (cp1 < end1);i++)
        {
            link = &tc->link[i];
            if (link->state == LINK_IDLE)
                continue;
            if (count && cp1 < end1)
                *cp1++ =',';                
            cp1 += snprintf(cp1, end1-cp1, _link_blk_msg_, link->id, msg_link_state[link->state], inet_ntoa(link->addr.sin_addr), link->port, link->sock,
                link->tx_pcnt_ps, link->tx_bcnt_ps, link->rx_pcnt_ps, link->rx_bcnt_ps, queue_data_size(&link->txqu)); 
            count ++;    
        }
        // get device
        if (dev_num && cp < end)
            *cp++ =',';
        cp += snprintf(cp, end-cp,_tunnel_dev_msg_, tc->uid,tc->ip[0],tc->ip[1],tc->ip[2],tc->ip[3],tc->port, 
            tc->sock, TunnelStateMag[tc->state],tc->session_id, tc->session_key, tc->cvr_ip[0],tc->cvr_ip[1],tc->cvr_ip[2],tc->cvr_ip[3],
            tc->http_port, tc->http_sock, tc->rtsp_port, tc->rtsp_sock, tc->tx_pcnt_ps, tc->tx_bcnt_ps, tc->rx_pcnt_ps,tc->rx_bcnt_ps, 
            tc->echo_count, queue_data_size(&tc->txqu), queue_data_size(&tc->rxqu), tbuf);
        dev_num++;
    }
    // last 3 bytes
    *cp++ =']'; *cp++='}'; *cp++='\0';
    pthread_mutex_unlock(&lock);
    //printf("%s() %d:%s\n",__FUNCTION__, cp-start, buf);
    return cp-start;
}


//
//  function: tunnel_ctrl_free
//      free the tunnel block list, free all active list, free list
//      parameters
//          none
//      return
//          0       success
//          other   fail
//
void tunnel_ctrl_free()
{
     TUNNEL_CTRL *p, *prev;
     struct list_head *head;

     p2p_log_msg("%s(): free all tunnels\n",__FUNCTION__);
     pthread_mutex_lock(&lock);
     head = &tunnel_active_list;
      // release free_list
     list_for_each_entry(p, head, list)
     {
         prev = list_entry(p->list.prev, TUNNEL_CTRL, list);
         list_del(&p->list);
         free_device_tunnel(p);
         free(p);
         p = prev;
     }
     
     
     head = &tunnel_free_list;
     // release free_list
     list_for_each_entry(p, head, list)
     {
         prev = list_entry(p->list.prev, TUNNEL_CTRL, list);
         list_del(&p->list);
         free(p);
         p = prev;
     }
     pthread_mutex_unlock(&lock);
     pthread_mutex_destroy(&lock);
}

//
//  function: tunnel_ctrl_init
//      initial the tunnel block list, free and active list
//      parameters
//          none
//      return
//          0       success
//          other   fail
//
int tunnel_ctrl_init()
{
    int i,ret=0;
    TUNNEL_CTRL *p;


    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        p2p_log_msg("%s(): mutex init failed\n",__FUNCTION__);
        return TUNN_MUTEX_FAIL;
    }
    
    INIT_LIST_HEAD(&tunnel_active_list);
    
    INIT_LIST_HEAD(&tunnel_free_list);
    
    for (i = 0; i < (MAX_TUNNEL_NUM); i++)
    {
        p = malloc(sizeof(TUNNEL_CTRL));
        if (!p)
        {
            p2p_log_msg("%s(): alloc memory fail\n",__FUNCTION__);
            ret = TUNN_NO_MEM;
            break;
        }
        put_tunnel_block(&tunnel_free_list, p);
    }
    
    if (ret<0)
        tunnel_ctrl_free();
    return ret;
}





//
// utility of queue
//

//
//  function: queue_init
//      create a queue buffer
//  parameters:
//      give structure of queue pointer
//      size of buffer
//  return:
//      TUNN_OK: success
//      TUNN_NO_MEM: fail , no memory
//
int queue_init(struct _queue_ *q, int size)
{
	memset(q,0,sizeof(struct _queue_));
	q->buf = malloc(size);
	if (q->buf)
	{	
		q->size = size;
		q->put=q->get=q->full=0;
		//q->sem = xSemaphoreCreateMutex();
		return TUNN_OK;
	}
	else
		return TUNN_NO_MEM;
}

//
//  function: queue_exit
//      free the queue
//  parameters:
//      give structure of queue pointer
//      size of buffer
//  return:
//      none
//
void queue_exit(struct _queue_ *q)
{
	if (q && q->buf)
	{	
		free(q->buf);
		q->buf = NULL;
		q->size = 0;
		//vSemaphoreDelete(q->sem);
		//q->sem = NULL;
	}
}

//
//  function: queue_put
//      put data into fifo, return success or queue full
//  parameters:
//      give structure of queue pointer
//      start data pointer
//      size of data
//  return:
//      >0 number of put data
//      TUNN_QUEUE_FULL: queue full
//      TUNN_NOT_AVAILABLE: servic not available 
//  
int queue_put(struct _queue_ *q, char *data, int size)
{
	int i;
	
	if (!q || !q->buf)
		return TUNN_NOT_AVAILABLE;
		
	//if (xSemaphoreTake( q->sem, ( TickType_t ) 10 ) != pdTRUE)
	//	return TRI_ERR_SEM_TAKE;
		
	for(i=0;i<size;i++)
	{
		if (q->full)
		{	
			p2p_log_msg( "%s full\n",__FUNCTION__);	// output debug message
			return TUNN_QUEUE_FULL;
		}
		q->buf[q->put++]=data[i];
		if (q->put>=q->size)
			q->put = 0;
		if (q->put == q->get)
			q->full = 1;
	}
	//xSemaphoreGive( q->sem );
	return i;	
}

//
//  function: queue_get
//      get data from fifo, return get size
//  parameters:
//      give structure of queue pointer
//      start buffer pointer
//      size of buffer
//  return:
//      size of got data
//      TUNN_NOT_AVAILABLE: queue not available
//
int queue_get(struct _queue_ *q,char *buf, int bufsize)
{
	int i;
	
	if (!q || !q->buf)
		return TUNN_NOT_AVAILABLE;
		
	//if (xSemaphoreTake( q->sem, ( TickType_t ) 10 ) != pdTRUE)
	//	return TRI_ERR_SEM_TAKE;
		
	for(i=0;i<bufsize;i++)
	{
		if (q->get == q->put && !q->full)	// empty?
			break;
		buf[i]=q->buf[q->get++];
		if (q->get >= q->size)
			q->get  = 0;
		q->full = 0;
	}
	//xSemaphoreGive( q->sem );
	return i;
}

//
//  function: queue_peek
//      get data from fifo, but not update the internal read pointer, return get size
//  parameters:
//      give structure of queue pointer
//      start buffer pointer
//      size of buffer
//  return:
//      size of got data
//      TUNN_NOT_AVAILABLE: queue not available
//
int queue_peek(struct _queue_ *q,char *buf, int bufsize)
{
	int i;

	if (!q || !q->buf)
		return TUNN_NOT_AVAILABLE;

	//if (xSemaphoreTake( q->sem, ( TickType_t ) 10 ) != pdTRUE)
	//	return TRI_ERR_SEM_TAKE;

	int get = q->get;
	int full = q->full;
	for(i=0;i<bufsize;i++)
	{
		if (get == q->put && !full)	// empty?
			break;

		buf[i]=q->buf[get++];
		if (get >= q->size)
			get = 0;

		full = 0;
	}
	//xSemaphoreGive( q->sem );
	return i;
}

//
//  function: queue_move
//      move get pointer, drop first n bytes of queue data
//  parameters:
//      give structure of queue pointer
//      start buffer pointer
//      number of moved bytes
//  return:
//      number of moved bytes
//      TUNN_NOT_AVAILABLE: queue not available
//
int queue_move(struct _queue_ *q, int n)
{
	if (!q || !q->buf)
		return TUNN_NOT_AVAILABLE;

	//if (xSemaphoreTake( q->sem, ( TickType_t ) 10 ) != pdTRUE)
	//	return TRI_ERR_SEM_TAKE;

	int data_size = 0;
	int ret = 0;
	if (q->full)
		data_size = q->size;
	else
		data_size = (q->put >= q->get ? (q->put - q->get) : (q->size + q->put - q->get));

	ret = (data_size > n ? n : data_size);

	q->get += ret;
	if (q->get >= q->size)
	{
		q->get -= q->size;
	}

	q->full = 0;

	//xSemaphoreGive( q->sem );
	return ret;
}

//
//  function: queue_data_size
//      get data size in fifo
//  parameters:
//      give structure of queue pointer
//  return:
//      number of moved bytes 
// 
int queue_data_size(struct _queue_ *q)
{
	int ret = 0;
	
	if (!q || !q->buf)
	    return 0;
	    
	//if (xSemaphoreTake( q->sem, ( TickType_t ) 10 ) != pdTRUE)
	//	return 0;
	if (q->full)
		ret = q->size;
	else if (q->put >= q->get)
		ret = (q->put - q->get);
	else
		ret = (q->size + q->put - q->get);
	//xSemaphoreGive( q->sem );
	return ret;
}

//
//  function: queue_space
//      get free space in queue
//  parameters:
//      give structure of queue pointer
//  return:
//      number of bytes of buffer size 
// 
int queue_space(struct _queue_ *q)
{
    if (!q || !q->buf)
	    return 0;
    return q->size - queue_data_size(q);
}

//
//  function: queue_reset
//      reset queue, ignore data in queue
//  parameters:
//      give structure of queue pointer
//  return:
//      TUNN_OK: success             
//      TUNN_NOT_AVAILABLE: queue not available
// 
int queue_reset(struct _queue_ *q)
{
    if (!q || !q->buf)
	    return TUNN_NOT_AVAILABLE;
	    
	//if (xSemaphoreTake( q->sem, ( TickType_t ) 10 ) != pdTRUE)
	//	return TRI_ERR_SEM_TAKE;
		
	q->put=q->get=q->full=0;	
	
	//xSemaphoreGive( q->sem );
	return TUNN_OK;	
}

//
//  function: fd_set_blocking
//      set block or none-block for socket
//  parameters:
//      socket id,
//		blocking: 1: block, 0 none block
//  return:
//      TUNN_OK: success             
//      TUNN_NOT_AVAILABLE: queue not available
// 
int fd_set_blocking(int fd, int blocking) {
    /* Save the current flags */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return 0;

    if (blocking)
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags) != -1;
}

//
//  function: get_free_port
//      get a port and check it is available
//      parameters
//          none
//      return
//          >0      port number
//          <=0     fail or not available
//
unsigned int link_port = MIN_LINK_SERVICE_PORT;
int get_free_port()
{
    unsigned int port = link_port++;
    
    if (link_port>MAX_LINK_SERVICE_PORT)
        link_port = MIN_LINK_SERVICE_PORT;
    
/*    
    #define _cmd_ = "netstat -tulnp | grep %u";    
    char buf[256];
    int count = 0;
    FILE *fp;

    // check port available?
    
    while (count < (MAX_LINK_SERVICE_PORT-MIN_LINK_SERVICE_PORT))
    {
        sprintf(buf,_cmd_,link_port);
        
        if ((fp = popen(cmd, "r")) == NULL) {
            printf("Error opening pipe!\n");
            return -1;
        }

        while (fgets(buf, BUFSIZE, fp) != NULL) {
            // Do whatever you want here...
            printf("OUTPUT: %s", buf);
        }

        if (pclose(fp))  
        {
            printf("Command not found or exited with error status\n");
            return -1;
        }
    }
*/    
	return port;
}

// function: get_time_milisec
//      get system time ticks
//  parameters:
//      none
//  return:
//      current minisecs
//
unsigned int get_time_milisec(void)
{
    struct timespec ts;
    unsigned int tm;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    tm = (ts.tv_sec * 1000) + (unsigned int)(ts.tv_nsec / 1000000);
    return tm;
}

//
//  function: dump_frame
//      dumpe memory 
//      parameters
//          message string
//          start of memory
//          length of memory
//      return
//          0       success
//          other   fail
//
void dump_frame(char *msg, char *frame, int len)
{
    unsigned short  i;
    unsigned char *p=(unsigned char *)frame;
    char ch[16+1]= {0};

    if (len<=0)
    {
        fprintf(stderr,"size overrun %u\n",len);
        len &= 0x7fff;
    }
    if (msg)
        fprintf(stderr,"%s, size %d\n",msg, len);
    while(len>16)
    {
        for (i=0; i<16; i++)
            ch[i] = (p[i]<0x20 || p[i]>=0x7f) ? '.' : p[i];

        fprintf(stderr,"%04x: ", (p-(unsigned char *)frame));
        fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X-%02X %02X %02X %02X %02X %02X %02X %02X | ",
                p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
        fprintf(stderr,"%s \n", ch);

        p+=16;
        len-=16;
    }/* End of for */
    if (len)
    {
        fprintf(stderr,"%04x: ", (p-(unsigned char *)frame));
        for(i=0; i<16; i++)
        {
            if (i<len)
            {
                ch[i] = (p[i]<0x20 || p[i]>=0x7f) ? '.' : p[i];
                fprintf(stderr,"%02X ",p[i]);
            }
            else
            {
                ch[i]='\0';
                fprintf(stderr,"   ");
            }
        }
        fprintf(stderr,"| %s \n", ch);
    }
}

//
// display log message
//
int p2p_log_msg(char *fmt, ...)
{
    struct tm TM = {0};
    time_t t = time(NULL);
    char buffer[1024] = {0};
    char format[256] = {0};

    localtime_r (&t, &TM);

    sprintf(format, "[%04u/%02u/%02u-%02u:%02u:%02u] %s", TM.tm_year + 1900, TM.tm_mon + 1, TM.tm_mday, TM.tm_hour, TM.tm_min, TM.tm_sec, fmt);

    va_list arg_list;

    va_start(arg_list, fmt);
    vsnprintf(buffer, 1024, format, arg_list);
    va_end(arg_list);

    fprintf(stderr, "%s", buffer);

    return 0;
}

//
// function: get_server_ip
//  to use system call to get default route interface IP address
//  parameter:
//      buffer start
//      buffer size
//  return
//      0: success
//      other fail
//
int get_server_ip(char *buf, int bufsize)
{
    #define _cmd1_str_  "route | grep default | awk '{print $8}' | tr -d '\n'"
    #define _cmd2_str_  "ifconfig %s | awk '/inet addr/{print substr($2,6)}' | tr -d '\n'" 
    
    char msg[256];
    int ret = TUNN_OK;
    FILE *fp;
    
     // get interface
    if ((fp = popen(_cmd1_str_, "r")) == NULL) 
    {
        p2p_log_msg("%s(): Error opening pipe!\n", __FUNCTION__);
        return -1;
    }

    if (fgets(buf, bufsize, fp))
        // Do whatever you want here...
        p2p_log_msg("%s(): interface: %s\n", __FUNCTION__, buf);
        
    ret= pclose(fp);
    if (ret)  
    {
        p2p_log_msg("%s(): Command not found or exited with error status, %d\n", __FUNCTION__, ret);
        return ret;
    }
    // get ip
    snprintf(msg,255,_cmd2_str_,buf);
    //p2p_log_msg("%s!\n",msg);
    if ((fp = popen(msg, "r")) == NULL) 
    {
        p2p_log_msg("%s(): Error opening pipe!\n", __FUNCTION__);
        return -1;
    }

    if (fgets(buf, bufsize, fp))
        // Do whatever you want here...
        p2p_log_msg("%s(): IP: %s\n", __FUNCTION__, buf);
        
    ret= pclose(fp);
    if (ret)  
    {
        p2p_log_msg("%s():Command not found or exited with error status, %d\n", __FUNCTION__, ret);
        return ret;
    }
    
    return ret;
}