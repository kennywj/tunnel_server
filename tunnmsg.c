//
//  module: device tunnel module
//      tunnmsg.h
//      active to builde up tunnel with specified server, multiplex/demutiplex traffice 
//      from server to local service or vice vera
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include "tunnel.h"
#include "tunnmsg.h"

//
// function: check_msghead
//      check first 2 bytes in queue, it it is a tag, the get following 2 bytes 
//      as length field and return length of data. consume first 4 bytes
// parameters:
//      pointer to rx queue
//      expext tag
//  return:
//      >=0      length of following message
//      other:  fail, consume the tage and length field.
//
int check_msghead(struct _queue_ *q,const unsigned short tag)
{
    char buf[MIN_TUNNEL_MSGLEN];
    int len=0, dlen=0;
    
    len = queue_peek(q, buf, MIN_TUNNEL_MSGHEAD);
    if (len==MIN_TUNNEL_MSGHEAD)
    {
        // correct tag?
        if (*(unsigned short *)buf == tag)
        {
            fprintf(stderr,"%s! head tag error %x %x\n",__FUNCTION__, buf[0], buf[1]);
            queue_move(q,MIN_TUNNEL_MSGHEAD);
            dlen = -1;
            goto end_check_msghead;
        }    
        dlen = *(unsigned short *)&buf[2];
        len = queue_data_size(q);
        if (dlen > len)
        {
            fprintf(stderr,"%s! data not enough, expect %d, real %d\n",__FUNCTION__, dlen, len);
            dlen = 0;
            goto end_check_msghead;
        }    
        queue_move(q,MIN_TUNNEL_MSGHEAD);
        // return data length
    }    
end_check_msghead:    
    return dlen;
}

//
// function: encode_msg
//      encode the message befor tx to server
//      it will also do encryption if need
//      the message format:
//      [TAG][len][encrypted message, align to 16 bytes boundary], 'len' is the total length of encrypt message
// parameters:
//      
//      
//  return:
//      >=0      length of encoded message
//      other:  fail
//
int encode_msg(TUNNEL_CTRL *tc, unsigned short tag, char *data, int dlen, char *buf, int bufsize)
{
    struct _tunnel_msg_ *msg = (struct _tunnel_msg_ *)buf;
    int padding=0;
    
    if ((dlen+MIN_TUNNEL_MSGHEAD)>bufsize)
    {
        fprintf(stderr,"%s! buffer not enough, data %d, bufsize %d\n",__FUNCTION__, dlen+MIN_TUNNEL_MSGHEAD, bufsize);
        return -1;
    }
    msg->tag = tag;
    memcpy(msg->data,data,dlen);
    padding = (((dlen-1)|0xf)+1)-dlen;
    if ((dlen+padding)>bufsize)
    {
        fprintf(stderr,"%s! buffer not enough, dlen+padding %d, bufsize %d\n",__FUNCTION__, dlen+padding, bufsize);
        return -2;
    }    
    memset(&msg->data[dlen],(char)padding,padding);
    msg->len = dlen+padding;
    // do encryption in here
    
    return msg->len;
}
