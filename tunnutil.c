//
//  module: device tunnel module
//      tunnutil.h
//      active to builde up tunnel with specified server, multiplex/demutiplex traffice 
//      from server to local service or vice vera
//
#include <stdio.h>      /* printf, scanf, NULL */
#include <stdlib.h>     /* malloc, free, rand */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "tunnel.h"

//
// tunnel control block allocate/free
//
struct list_head tunnel_free_list, tunnel_active_list;

//
//  function: get_tunnel_block
//      get a tunnel control block in free list
//      parameters
//          none
//      return
//          pointer of tunnel conrol block
//          null    not free block
//
TUNNEL_CTRL *get_tunnel_block()
{
    TUNNEL_CTRL *p = NULL;
    struct list_head *head = &tunnel_free_list;
    
    if (!list_empty(head))
    {
        /*remove node from head of queue*/
        p = list_entry(head->next, TUNNEL_CTRL, list);
        if (p)
            list_del(&p->list);
    }
    return p;
}

//
//  function: free_tunnel_block
//      get a tunnel control block in free list
//      parameters
//          none
//      return
//          pointer of tunnel conrol block
//          null    not free block
//
void free_tunnel_block(TUNNEL_CTRL *p)
{
    list_add_tail(&p->list, &tunnel_free_list);
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
    struct list_head *head = &tunnel_active_list;
    
    list_for_each_entry(p, head, list)
    {
        if (p->sock == sid)
            return p;
    }
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
    struct list_head *head = &tunnel_active_list;
    
    list_for_each_entry(p, head, list)
    {
        if (*(unsigned int *)p->ip == *(unsigned int *)ip &&
            p->port == port                                 )
            return p;
    }
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
    struct list_head *head = &tunnel_active_list;
    
    list_for_each_entry(p, head, list)
    {
        if (memcmp(p->uid, uid, sizeof(*p->uid))==0)
            return p;
    }
    return 0; /*not found*/
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

    INIT_LIST_HEAD(&tunnel_active_list);
    
    INIT_LIST_HEAD(&tunnel_free_list);
    
    for (i = 0; i < (MAX_TUNNEL_NUM); i++)
    {
        p = malloc(sizeof(TUNNEL_CTRL));
        if (!p)
        {
            printf("%s: alloc memory fail\n",__FUNCTION__);
            ret = -1;
            break;
        }
        list_add_tail(&p->list, &tunnel_free_list);
    }
    
    if (ret<0)
    {
        // free_list
        while(1)
        {
            p = get_tunnel_block();
            if (p)
                free(p);
            else
                break;
        }
    }
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
			fprintf(stderr, "%s full\n",__FUNCTION__);	// output debug message
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
//  function: read_from_socket
//      read from socket and put in queue
//      parameters
//          socket
//          queue pointer,
//      return
//          0       success
//          other   fail
//
int read_from_sock(int sock, struct _queue_ *q)
{
	return 0;
}