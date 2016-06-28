//
//  module: CVR tunnel daemon
//      tunnel.c
//      provide tunnel service from devices 
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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "tunnel.h"
#include "tunnmsg.h"
#include "link.h"
#include "cJSON.h"
#include "tunnel_ext.h"




//
// const definition
//


const char *TunnelStateMag[MAX_TUNNEL_STATE]={"Idle","Connecting","Authentication","Service","Closing"};

//
// external function declear
//
extern void free_link(TUNNEL_CTRL *tc, int id);
static void tunnel_update_counter(TUNNEL_CTRL *tc);
//
//  function: do_sent_message
//      sent control messsage or data to device
//      parameters
//          Tunnel control block
//          type: 0 data, 1 message
//          encrypt: 1 encrypt, 0 not encrypt
//          key: encrypt key
//          start of encrypt data
//          size of data
//      return
//          0       success
//          other   fail
//
#ifdef RAW_TUNN_TEST
int do_sent_raw(TUNNEL_CTRL *tc, char *data, int dlen)
{
    int ret =0,len,offset;
    
	// if no data in queue, write to socket immediate    
	if (queue_data_size(&tc->txqu)==0)
	{    
#ifdef _OPEN_SECURED_CONN
        if (tc->ssl)
            len = ssl_server_send(tc->ssl, tc->sock, data, dlen);
        else
#endif	    
    	    len = send(tc->sock, data, dlen, MSG_NOSIGNAL);    
	    if (len<0)    
	    {
	        ret = len;
	        goto end_do_sent_message;
	    }
    	dlen -= len;
    	offset = len;
    	
    	tc->tx_pcnt += 1;
	    tc->tx_bcnt += len; 
        //p2p_log_msg("%s():tunnel socket=%d write %d!\n", __FUNCTION__,tc->sock,ret);
	}
	else
	    offset = 0;
    
	// put reminder into in tx queue
	if (dlen)
	{    
    	ret = queue_put(&tc->txqu, &data[offset], dlen);
	    if (ret<0)
            goto end_do_sent_message;	    
	    // enable write select
	    FD_SET(tc->sock, tc->wfds);
        p2p_log_msg("%s():turn on write select, socket=%d! queued %u bytes\n", __FUNCTION__,tc->sock, queue_data_size(&tc->txqu));
	}
end_do_sent_message:	
	return ret;
}
#endif
// RAW_TUNN_TEST

int do_sent_message(TUNNEL_CTRL *tc, unsigned char tag, char *data, int dlen)
{
	int ret =0,len, offset;
	char buf[SIZE_OF_MSGBUF+16];
	
	// encode message
    ret = encode_msg(tc->session_key, tag, data, dlen, buf, SIZE_OF_MSGBUF+16);
    if (ret<0)
        goto end_do_sent_message;
    len = ret;    
    // send resposne to devcie
	//dump_frame("sent message",buf,(len>32?32:len));
	    
	// if no data in queue, write to socket immediate    
	if (queue_data_size(&tc->txqu)==0)
	{    
#ifdef _OPEN_SECURED_CONN
        if (tc->ssl)
            ret = ssl_server_send(tc->ssl, tc->sock, buf, len);
        else
#endif	    
    	    ret = send(tc->sock, buf, len, MSG_NOSIGNAL);    
	    if (ret<0)  
	    {      
	        if (errno != EAGAIN)
	        {    
	            tc->state = TUNNEL_CLOSEING;
	            goto end_do_sent_message;
	        }
	        ret = 0;
	    }    
	    tc->tx_pcnt += 1;
	    tc->tx_bcnt += ret;
    	tc->time = get_time_milisec() + 10000;  // extend 10 seconds
    	len -= ret;
    	offset = ret;
	}
	else
	    offset = 0;
	// put reminder into in tx queue
	if (len)
	{    
    	ret = queue_put(&tc->txqu, &buf[offset], len);
	    if (ret<0)
            goto end_do_sent_message;	    
	    // enable write select
	    FD_SET(tc->sock, tc->wfds);
	}
end_do_sent_message:
    if (ret<0)
        p2p_log_msg("%s(): sent out data fail %d, tag %x!", __FUNCTION__, ret, tag);
	return ret;
}

//
//  function: do_device_info
//      set device on/off line to SC
//      parameters
//          TC control
//          action: 1 online, 0 offline
//          *stat: 200 OK, other fail
//      return
//          0       success
//          other   fail
//
int do_device_info(TUNNEL_CTRL *tc, int action, int *stat)
{
    char fq, lq, *out, data[256]={0},resp[1024]={0}, uid[MAX_UID_LEN+1]={0};
    int ret = TUNN_AUTH_FAIL;
    cJSON *root, *obj;
	struct in_addr addr;
    // to do authentication with SC, curl?
    snprintf(data,255,_sc_auth_msg_, tc->uid, tc->session_id, action, tc->http_port, tc->rtsp_port);
    
    p2p_log_msg("%s(): do registry to SC\n%s\n",__FUNCTION__, data);

    ret = clouds_service(sc_auth_url,data,resp,1023);
	if (ret>0)
	{
	    int retval = -1;
	    // process and get token
	    root = cJSON_Parse(resp);
#ifdef DEBUG	    
	    if (root)
	    {	
		    out = cJSON_Print(root);
		    p2p_log_msg("%s\n",out);
		    free(out);
        }
#endif  
        // get uid
    	obj = cJSON_GetObjectItem(root,"uid");
	    if (obj)
    	{    
	        out = cJSON_Print(obj);
	        if (out)
    	    {    
	            sscanf(out,"%c%[^\"]%c", &fq, data, &lq);
	            strncpy(uid, data, MAX_UID_LEN);
	            free(out);    
	        }
	        //uid not match?
	        if (strncmp(tc->uid, uid, MAX_UID_LEN)!=0)
	        {
	            retval = -2;
	            goto end_parse_resp;
	        }
        }
        // get session id
	    obj = cJSON_GetObjectItem(root,"sessionKey");
	    if (obj)
	    {    
	        out = cJSON_Print(obj);
	        if (out)
	        {    
	            sscanf(out,"%c%[^\"]%c", &fq, data, &lq);
	            strncpy(tc->session_key, data, MAX_SESSION_KEY_LEN);
	            free(out);    
	        }
        }
        
        // get server IP (wan IP)
	    obj = cJSON_GetObjectItem(root,"ip");
	    if (obj)
	    {    
	        out = cJSON_Print(obj);
    	    if (out)
	        {    
	            sscanf(out,"%c%[^\"]%c", &fq, data, &lq);
	            inet_aton(data, &addr);
    	        memcpy(tc->cvr_ip, &addr.s_addr, sizeof(int));
	            free(out);    
	            p2p_log_msg("CVR wan ip %u.%u.%u.%u\n", tc->cvr_ip[0], tc->cvr_ip[1], tc->cvr_ip[2], tc->cvr_ip[3]);
	        }
        }
    
        // valid?        
        obj = cJSON_GetObjectItem(root,"valid");
	    if (obj)
	    {	
		    out = cJSON_Print(obj);
		    retval = atoi(out);
		    free(out);
        }
end_parse_resp:        
	    if (stat)
	    {    
            if (retval ==1)
            {    
                *stat = 200;    // success
                ret = TUNN_OK;
            }
            else if (retval == -2)
                *stat = 404;    // band UID not found
            else
                *stat = 400;    // fail bad request
        }
        ret = TUNN_OK;
		p2p_log_msg("%s(): sent %s %s info retval=%d\n",__FUNCTION__, tc->uid, (action?"on line":"off line"), retval);
    }    
	else    
	{
		p2p_log_msg("%s(): sent %s %s info fail %d\n",__FUNCTION__, tc->uid,(action?"on line":"off line"), ret);
        if (stat)
    		*stat = 401; // unauth
	}
	return ret;
}


//
//  function: do_device_registry
//      do registry to CVR and cloud database
//      parameters
//          TC control
//          action 0: offline, 1 online
//      return
//          0       success
//          other   fail
//
int do_device_registry(TUNNEL_CTRL *tc, int action)
{
    char *out, data[512]={0},resp[1024]={0}, addr[33]={0};
    char liveview_url[256],playback_url[256], key[256],*cp;
    int ret = TUNN_OK, retval = 0, stat;
    cJSON *root, *obj;
        
    // to do registry with CVR, curl?
    snprintf(addr,32,"%u.%u.%u.%u",tc->cvr_ip[0],tc->cvr_ip[1],tc->cvr_ip[2],tc->cvr_ip[3]);
    snprintf(data,511,_dev_registry_msg_, tc->uid, addr, 443, tc->rtsp_port, tc->http_port, tc->session_key, action);

    p2p_log_msg("%s(): do registry to CVR\n%s\n",__FUNCTION__, data);

    ret = clouds_service(dev_registry_cvr_url,data,resp,1023);
	if (ret>0)
	{
//#ifdef DEBUG	    
	    // process and get token
	    root = cJSON_Parse(resp);
	    if (root)
	    {	
		    out = cJSON_Print(root);
		    p2p_log_msg("%s\n",out);
		    free(out);
        }
//#endif  
        /*
        int retval = -1;
        obj = cJSON_GetObjectItem(root,"valid");
	    if (obj)
	    {	
		    out = cJSON_Print(obj);
		    retval = atoi(out);
		    free(out);
        }
	    if (stat)
	    {    
            if (retval ==1)
                *stat = 200;    // success
            else
                *stat = 400;    // fail bad request
        }
		p2p_log_msg("%s(): sent %s %s info retval=%d\n",__FUNCTION__, tc->uid, (action?"on line":"off line"), retval);
        ret = TUNN_OK;
        */
       
    	snprintf(liveview_url,255,"http://%s/%s/liveview.m38u",addr, tc->uid);
    	snprintf(playback_url,255,"http://%s/%s/playback.m38u",addr, tc->uid);
    	
    	cp = tc->session_key;
    	snprintf(key,255,"%c%c%c%c%c%c%c%c-%c%c%c%c-%c%c%c%c-%c%c%c%c-%s",cp[0],cp[1],cp[2],cp[3],cp[4],cp[5],cp[6],cp[7],
    	    cp[8],cp[9],cp[10],cp[11],cp[12],cp[13],cp[14],cp[15],cp[16],cp[17],cp[18],cp[19],&cp[20]);
    	
        //snprintf(data,511,_dev_registry_db_msg_, tc->uid, (action?"true":"false"), liveview_url, playback_url, addr, 443, tc->rtsp_port, tc->http_port, tc->session_key);
        snprintf(data,511,_dev_registry_db_msg_, tc->uid, action, liveview_url, playback_url, addr, 443, tc->rtsp_port, tc->http_port, key);

        p2p_log_msg("%s(): do registry database\n%s\n",__FUNCTION__, data);
        
        ret = clouds_put_service(dev_registry_db_url,data,resp,1023);
       if (ret>0)
        {    
//#ifdef DEBUG	    
	        // process and get token
    	    root = cJSON_Parse(resp);
	        if (root)
	        {	
	            out = cJSON_Print(root);
        		p2p_log_msg("%s\n",out);
	        	free(out);
            }
//#endif
        }
        // update default service plan
        snprintf(data,511,_dev_service_plan_msg_, tc->uid, 0, 0, 0, 0, 1);
        p2p_log_msg("%s(): update CVR service plan, default\n%s\n",__FUNCTION__, data);
        
        ret = clouds_service(dev_cvr_service_plan_url,data,resp,1023);
        if (ret>0)
        {    
//#ifdef DEBUG	    
	        // process and get token
    	    root = cJSON_Parse(resp);
	        if (root)
	        {	
	            out = cJSON_Print(root);
        		p2p_log_msg("%s\n",out);
	        	free(out);
            }
//#endif
        }
        
        ret = TUNN_OK;
    }    
end_do_device_registry:   
    p2p_log_msg("%s(): sent %s %s info %s\n",__FUNCTION__, tc->uid,(action?"on line":"off line"),(ret!=TUNN_OK?"fail":"success"));
	return ret;
}


//
//  function: do_device_auth
//      read first authentication message from socke and 
//		thne formward to SC clent to do authentication and authorize.
//
//      parameters
//          data biffer
//          length of data
//      return
//          0       success
//          other   fail
//
int do_device_auth(TUNNEL_CTRL *tc, char *buf, int bufsize)
{
	int ret =0, len, dlen,stat=400;
	struct _tunnel_msg_ *msg;
	cJSON *root=NULL, *obj;
	char msgbuf[256], data[512]={0}, action[33];
	char fq, lq, *out;
	
    tc->state = TUNNEL_AUTH;
	// receive the data from socke and do authentication
#ifdef _OPEN_SECURED_CONN
    if (tc->ssl)
        len = ssl_server_recv(tc->ssl, tc->sock, buf, bufsize);
    else
#endif    
        len = recv(tc->sock, buf, bufsize, 0);
    if (len<0)
    {
        ret = len;
        p2p_log_msg("%s(): read socket %d error %d!", __FUNCTION__, ret, errno);
        goto end_do_device_auth;
    }
	//dump_frame("auth resuest",buf,len);
    msg = (struct _tunnel_msg_ *)buf;
    //printf("%s, tag = %x, len =%d\n",__FUNCTION__, *(unsigned short *)msg->tag, msg->len);
	
	root = cJSON_Parse(msg->data);
    if(root == NULL)
    {    
        p2p_log_msg("%s(): fail tag = %x, %s\n",__FUNCTION__, *(unsigned short *)msg->tag, msg->data);
        ret = TUNN_ERR_PARAMETER;
        goto end_do_device_auth;
	}
	// get uid
	obj = cJSON_GetObjectItem(root,"uid");
	if (obj)
	{    
	    out = cJSON_Print(obj);
	    if (out)
	    {    
	        sscanf(out,"%c%[^\"]%c", &fq, data, &lq);
	        strncpy(tc->uid, data, MAX_UID_LEN);
	        free(out);    
	    }
    }
    // get session id
	obj = cJSON_GetObjectItem(root,"session_id");
	if (obj)
	{    
	    out = cJSON_Print(obj);
	    if (out)
	    {    
	        sscanf(out,"%c%[^\"]%c", &fq, data, &lq);
	        strncpy(tc->session_id, data, MAX_SESSION_ID_LEN);
	        free(out);    
	    }
    }
        
    // get action
	obj = cJSON_GetObjectItem(root,"action");
	if (obj)
	{    
	    char *out = cJSON_Print(obj);
	    if (out)
	    {    
	        p2p_log_msg("action = %s\n",out);
	        sscanf(out,"%c%[^\"]%c", &fq, action, &lq);
	        free(out);    
	    }
    }
#ifndef TEST_TUNNEL        
    // to do info device online 
    if (strcmp(action,"open")==0)
    {    
        ret = do_device_info(tc, 1, &stat);   
        if (ret<0)
             goto end_do_device_auth;
	}    
#else
    stat = 200;	
#endif
	
	// get response, encode response message
	len = snprintf(msgbuf, 255,_tunnel_resp_msg_, action, stat);
	// encode message
    ret = encode_msg(tc->session_key, MSG_TAG+ENCRYPT_TAG, msgbuf, len, data, 512);
    if (ret<0)
        goto end_do_device_auth;
    dlen = ret;    
    
	//dump_frame("response message",data,dlen);
    // send resposne to devcie
#ifdef _OPEN_SECURED_CONN
    if (tc->ssl)
        len = ssl_server_send(tc->ssl, tc->sock, data, dlen);
    else
#endif 
    	len = send(tc->sock, data, dlen, MSG_NOSIGNAL);
	if (len != dlen)
	{
	    p2p_log_msg("%s write error %d\n",__FUNCTION__, len);
        ret = TUNN_WRITE_ERR;
        goto end_do_device_auth;
    }    
    if (stat == 200)
        tc->state = TUNNEL_SERVICE;
    else
        ret = TUNN_AUTH_FAIL;    
end_do_device_auth:
    if (root) 
        cJSON_Delete(root);
	return ret;
}



//
// function: tunnel_serv_process
//      handle the service messages from server, 
// parameters:
//      pointer to tunnel block
//  return:
//      200:    success, statsu code      
//      0:      data not enough
//      other:  fail
//
void tunnel_serv_process(TUNNEL_CTRL *tc, char type, char *data, int dlen)
{
    int stat = 200;
    char *out, fq, lq, param[128];
    cJSON *root, *obj;
    
    if (type==MSG_TAG)
    {    
        // parsing response
        root = cJSON_Parse(data);
        if (root)
        {   
            // check uid
/*            obj = cJSON_GetObjectItem(root,"uid");
            if (obj)
            {
                out = cJSON_Print(obj);
                sscanf(out,"%c%[^\"]%c", &fq, param, &lq);
                free(out);
                if (strncmp(tc->uid, param, MAX_UID_LEN)!=0)
                {
                    ret = TUNN_ERR_PARAMETER;
                    stat = 400;
                }                    
            }    
            // check session id
            obj = cJSON_GetObjectItem(root,"session_id");
            if (obj)
            {
                out = cJSON_Print(obj);
                sscanf(out,"%c%[^\"]%c", &fq, param, &lq);
                free(out);
                if (strncmp(tc->session_id, param, MAX_SESSION_ID_LEN)!=0)
                {
                    ret = TUNN_ERR_PARAMETER;
                    stat = 400;
                }
            }
*/            
            // got action
            obj = cJSON_GetObjectItem(root,"action");
            if (obj)
            {
                out = cJSON_Print(obj);
                sscanf(out,"%c%[^\"]%c", &fq, param, &lq);
                free(out);
            }
            
            // got action
            obj = cJSON_GetObjectItem(root,"stat");
            if (obj)
            {
                out = cJSON_Print(obj);
                stat = atoi(out);
                free(out);
            }
            cJSON_Delete(root);
    
            // process request and response message
            if (strcmp(param,"echo")==0 && stat == 200)
            {   
                //printf("echo success\n");
                tc->echo_count ++;
            }
        }
    }
    else
        link_recv_process(tc, data, dlen);
//end_tunnel_serv_process:        
    return;
}


//
//  function: tunnel_recv_process
//      handle the message from device
//  parameters:
//      pointer to tunnel  
//  return:
//      0<=      error  or socket close
//      >0       process data length
//
int tunnel_recv_process(TUNNEL_CTRL *tc, char *buf, int bufsize)
{
    int dlen, len, ret=TUNN_OK;
    char type=0,encrypt=0;
    struct _queue_ *q = &tc->rxqu;
#ifdef RAW_TUNN_TEST
    TUNN_LINK *link = &tc->link[0];
    
    if (link->state == LINK_CONNECTING)
    {    
        dlen = queue_get(q, buf, bufsize);
        if (dlen>0)
        {
            dump_frame("",buf, dlen);
            link->state = LINK_CONNECTED;
            link->time = get_time_milisec() + 10000;     // wait 10 seconds
        }
        return 0;    
    }                
    
    if (link->state != LINK_CONNECTED)
        return 0;
        
    link->time = get_time_milisec() + 10000;     // wait 10 seconds
    // direct write to link
    while(queue_data_size(q))
    {
        int offset =0;
        dlen = queue_get(q, buf, bufsize);
        // no data in queue?, write it immediately
        if (queue_data_size(&link->txqu)==0)
	    {    
 	        //len = write(link->sock, buf, dlen);    
            len = send(link->sock, buf, dlen, MSG_NOSIGNAL );  
	        if (len<0)  
	        {      
                p2p_log_msg("%s(): warning! link %d write sock=%d err %d, errno=%d \n",
                     __FUNCTION__, link->id, link->sock, ret, errno);
	            break;
	        }
    	    dlen -= len;
    	    offset = len;
    	    //update counter
    	    link->tx_pcnt+=1;
       	    link->tx_bcnt+=ret;
	    }
	    else
	        offset = 0;
                    
        if (dlen)
        {
            //dump_frame("sent to local", msg->data,msg->dlen); 
            len = queue_put(&link->txqu, &buf[offset], dlen);
            if (len<0)
                p2p_log_msg("%s(): warning! put data in link queue fail %d,%d \n",__FUNCTION__, dlen, len);
            // enable write select
	        FD_SET(link->sock, tc->wfds);
	    }
    }   // end while
#else    
    // process received data, by state
    while(queue_data_size(q)>MIN_TUNNEL_MSGLEN)
    {
        // check if message header is response? 
        if ((len = check_msghead(q,&type,&encrypt))<0)
        {
            p2p_log_msg("%s! head tag error %d\n",__FUNCTION__, len);
            continue;        
        }    
        if (len==0) // may be not enough, return
            break;      // exit loop
        if (len>bufsize)
        {
            queue_move(q,len);  // consume message
            p2p_log_msg("%s! too large %d ignore message\n",__FUNCTION__, len);
            continue;
        }
        // get the message
        dlen = queue_get(q,buf,len);
        //p2p_log_msg("%s(): sock=%d type=%d receive %d\n",__FUNCTION__, tc->sock, type, dlen);
        //dump_frame("tunnel_recv_process",buf,(dlen>32?32:dlen));
        tc->rx_pcnt += 1;
	    tc->rx_bcnt += dlen;
        // process message by state
        switch(tc->state)
        {
            case TUNNEL_SERVICE:
                tunnel_serv_process(tc, type, buf, dlen);
            break;    
            case TUNNEL_CLOSEING:    
                //ret = tunnel_closing_process(tc);
            //break;    
            default:
                p2p_log_msg("%s, state %s! reset queue\n",__FUNCTION__,TunnelStateMag[tc->state]);
                queue_reset(q);
            break;
        }
    }   // end of while
//end_tunnel_recv_process:  
#endif  
    return ret;
}   // end of tunnel process

//
//  function: do_local_service
//      get two unused ports for http and rtsp and listen on it
//		it will get the connetction form local/remote and formwared to device
//      parameters
//          tunnel control
//          give two ports, http & rtsp
//          update the nfds,
//      return
//          0       success
//          other   fail
//
int do_local_service(TUNNEL_CTRL *tc, int *nfds)
{
    int servfd, i, port, ret =TUNN_OK;
    struct sockaddr_in serveraddr; /* client addr */
    
    for(i=0;i<2;i++)
    {
        //
        // get aviailable port
        //
        port = get_free_port();
        if (port<0)
        {
            p2p_log_msg("%s: no avaliable port %d\n",__FUNCTION__, port);
            ret = TUNN_NOT_AVAILABLE;
            goto end_do_local_service;    
        }            
        //
        // socket: create the parent socket 
        //
    
        servfd = socket(AF_INET, SOCK_STREAM, 0);
        if (servfd < 0) 
        {    
            p2p_log_msg( "%s: ERROR opening socket\n",__FUNCTION__);
            ret = TUNN_SOCK_ERR;
            goto end_do_local_service;
        }
    
        // setsockopt: Handy debugging trick that lets 
        // us rerun the server immediately after we kill it; 
        // otherwise we have to wait about 20 secs. 
        // Eliminates "ERROR on binding: Address already in use" error. 
        //
        int optval = 1;
        setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (const void *)&optval , sizeof(int));

        //
        // build the server's Internet address
        //
        bzero((char *) &serveraddr, sizeof(serveraddr));

        // this is an Internet address
        serveraddr.sin_family = AF_INET;

        // let the system figure out our IP address */
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

        // this is the port we will listen on */
        serveraddr.sin_port = htons(port);

        // 
        // bind: associate the parent socket with a port 
        //
        if ((ret=bind(servfd, (struct sockaddr *) &serveraddr, sizeof(struct sockaddr_in))) < 0) 
    	{   
            p2p_log_msg("%s: ERROR on binding %d, errno=%d\n",__FUNCTION__, ret, errno);
            ret = TUNN_SOCK_ERR;
            goto end_do_local_service;
        }
        // 
        // listen: make this socket ready to accept connection requests 
        //
        if (listen(servfd, 5) < 0) // allow 5 requests to queue up
        {    
            p2p_log_msg("%s: ERROR on listen",__FUNCTION__);
            ret = TUNN_SOCK_ERR;
            goto end_do_local_service;
        }
        FD_SET(servfd, tc->rfds);
        *nfds = max(*nfds, servfd);
            
        if (i==0)
        {
            tc->http_port = port;
            tc->http_sock = servfd;
        }        
        else
        {
            tc->rtsp_port = port;
            tc->rtsp_sock = servfd;
        }
        p2p_log_msg("%s %s listen on %u port\n",__FUNCTION__,(i?"RTSP":"HTTP"),port);
    }   // end for             
    
end_do_local_service:
    return ret;    
}


//
//  function: do_tunnel_report
//      execute registry procedure with SC client
//		curl http request
//      parameters
//          message
//      return
//          0       success
//          other   fail
//
int do_tunnel_report(char *msg, TUNNEL_CTRL *tc)
{   
    int ret = 0;
    char *cp, *end, buf[1024], resp[1024];
    
    p2p_log_msg("%s(): %s\n",__FUNCTION__, msg);
    
    cp = buf, end =&buf[1024];
    cp += snprintf(cp, end-cp,_tunnel_dev_status_, tc->uid,tc->ip[0],tc->ip[1],tc->ip[2],tc->ip[3],tc->port, 
            tc->sock, TunnelStateMag[tc->state],tc->session_id, tc->session_key, tc->cvr_ip[0],tc->cvr_ip[1],tc->cvr_ip[2],tc->cvr_ip[3],
            tc->http_port, tc->http_sock, tc->rtsp_port, tc->rtsp_sock, msg);
    ret = clouds_service(sc_report_url, buf, resp, 1024);
    return ret;
}


//
//  function: tunnel_close
//      close the tunnel socket and related local connections
//  parameters:
//      pointer to tunnel  
//  return:
//      none
//
void tunnel_close(TUNNEL_CTRL *tc)
{
    int i;
    
    if (!tc)
        return;
    p2p_log_msg("%s\n",__FUNCTION__);
    
    for (i=0;i<MAX_LINK_NUM;i++)
        free_link(tc, tc->link[i].id);
    //reset queue    
    queue_reset(&tc->rxqu);
    queue_reset(&tc->txqu);
    // close socket
#ifdef _OPEN_SECURED_CONN
    if (tc->ssl)
        ssl_server_close(tc->ssl);
    else    
#endif 
    if (tc->sock>=0)
        close(tc->sock);
    // close local service
    if (tc->http_sock>=0)
        close(tc->http_sock);
    if (tc->rtsp_sock>=0)
        close(tc->rtsp_sock);
    tc->http_sock = -1;    
    tc->rtsp_sock = -1;    
    tc->sock = -1;
    tc->state = TUNNEL_IDLE;
    // close all links
}


//
//  function: tunnel_device_handler
//      opne a service port and process the service request from devices
//      parameters
//          service port number
//      return
//          0       success
//          other   fail
//
void *tunnel_device_handler(void *pdata)
{
    TUNNEL_CTRL *tc1, *tc = (TUNNEL_CTRL *)pdata;
    fd_set readfds, writefds, rfds, wfds;
    int nfds=-1, sd=-1, ret=-1, dlen, len, secs=0, err;
    struct timeval tv;
    socklen_t clientlen;
    struct sockaddr_in clientaddr; /* client addr */
    char data[SIZE_OF_MSGBUF+16];
    unsigned int next_time, current_time;
    socklen_t addrlen ;
    struct sockaddr_in peeraddr;
            
    p2p_log_msg("%s():start tunnel from %u.%u.%u.%u:%u!\n",__FUNCTION__,tc->ip[0],tc->ip[1],tc->ip[2],tc->ip[3],tc->port);
    
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    tc->rfds = &readfds;
    tc->wfds = &writefds;
    tc->nfds = &nfds;
    //
    // listen on 2 local sockets, rtsp and http 
    //
    ret = do_local_service(tc, &nfds);
    if (ret != TUNN_OK)
    {
        p2p_log_msg("s%():local listen port error %d!",__FUNCTION__,ret);
        goto end_tunnel_device_handler;
    }    
    
#ifdef RAW_TUNN_TEST   
    tc->state = TUNNEL_SERVICE;
#else
    // sent online info and to do authentication
    ret = do_device_auth(tc, data, SIZE_OF_DATABUF);
    if (ret <0)
   	{
        p2p_log_msg("%s():auth fail %d!\n",__FUNCTION__,ret);
        goto end_tunnel_device_handler;
    }
#endif    
    // looking for old devices
    tc1 = find_tunnel_block_by_uid(tc->session_id);
    if (tc1)
    {
        p2p_log_msg("%s():old device exist, close it!\n",__FUNCTION__);
        free_device_tunnel(tc1);
    }
    put_tunnel_block(&tunnel_active_list, tc);
    
    //
    // set tcp no delay
    //
    int nodelay_flag = 1;
    setsockopt(tc->sock, IPPROTO_TCP, TCP_NODELAY, (void*) &nodelay_flag, sizeof(int));
    
	// set socket none-block
	fd_set_blocking(tc->sock,0);   
    FD_SET(tc->sock, &readfds);
    nfds = max(tc->sock,nfds);
    
    //
    // do registry to CVR system and clouds database
    //
#ifndef TEST_TUNNEL    
/* registry to SC and Database will move to SC
    ret = do_device_registry(tc,1);
    if (ret != TUNN_OK)
    {
        p2p_log_msg("%s():registry to CVR or cloud database fail %d!\n",__FUNCTION__,ret);
        goto end_tunnel_device_handler;
    }
*/    
    //
    // new device on line
    //
    do_tunnel_report("on line",tc);
#endif   
    //
    // main loop: wait for a connection request
    //
    current_time = get_time_milisec();
    next_time  = current_time + 1000;   // next seconds
    tc->time = current_time + 10000;    // extend 10 seconds
    while (1) 
    {
        current_time = get_time_milisec();
        if (current_time > next_time)
        {    
            tunnel_update_counter(tc);
            next_time = current_time + 1000;
            // if overflow?
            //if (next_time < current_time)
            //    next_time = 0;
        }
        
        if (current_time>tc->time)
        {
            p2p_log_msg("%s(): tunnel expired!\n", __FUNCTION__ );
            break;
        }
        memcpy(&rfds,&readfds,sizeof(fd_set));
        memcpy(&wfds,&writefds,sizeof(fd_set));
         // reset timeout value
        tv.tv_sec = 1;  // 1 seconds
        tv.tv_usec = 0;
        
        if((ret=select(nfds+1, &rfds, &wfds, NULL, &tv)) == -1)
        {
            p2p_log_msg("tunnel select() error lol!");
            goto end_tunnel_device_handler;
        }
        
        if (tc->do_exit)
        {
            p2p_log_msg("do exit!");
            goto end_tunnel_device_handler;
        }    
        // if timer expired, it means no traffic
#ifndef RAW_TUNN_TEST        
        if (ret==0)
        {
            secs++;
            if (secs>5)
            {
                int len = snprintf(data, 255,_tunnel_req_msg_, tc->uid, tc->session_id, "echo");
            	ret = do_sent_message(tc, MSG_TAG+ENCRYPT_TAG, data, len);
            	if (ret<0)
        	    {
        	        p2p_log_msg("sent echo error %d!",ret);
       		        goto end_tunnel_device_handler; 
                }    
                secs = 0;    
            }    
        }  
#endif    	
        // enable write data to device
        if (FD_ISSET(tc->sock, &wfds))
       	{
       	    do {
       	        len=-1;
       	        dlen = queue_peek(&tc->txqu, data, SIZE_OF_MSGBUF);
       	        // sent out
       	        if (dlen>0)
       	        {    
#ifdef _OPEN_SECURED_CONN
                    if (tc->ssl)
                        len = ssl_server_send(tc->ssl, tc->sock, data, dlen);
                    else
#endif       	            
       	                len = send(tc->sock, data, dlen, MSG_NOSIGNAL);
       	        }
       	        else
       	            len = dlen;
       	        // update queue pointer    
       	        if (len>0)
       	        {    
       	            queue_move(&tc->txqu,len);
       	            tc->tx_pcnt += 1;
	                tc->tx_bcnt += len;
                   	tc->time = get_time_milisec() + 10000;  // extend 10 seconds
                }
       	        else
       	        {   // len <=0
       	            err = errno;
       	            if (len < 0 && err == EAGAIN)
           	        {    
       		            p2p_log_msg("%s():warning write socket=%d err=%d errno=%d!\n", __FUNCTION__,tc->sock, len, err);
           	            break;
           	        }
       		        FD_CLR(tc->sock,&writefds);
       		        if (len<0)
       		        {
       		            ret = len;
       		            p2p_log_msg("%s():tunnel socket=%d write err %d errno=%d!", __FUNCTION__,tc->sock,ret, err);
       		            goto end_tunnel_device_handler; 
       		        }    
       	        }
       	    }while(len>0);
    	}   // end of write data to peer
    	
    	// new data incoming from peer
        if (FD_ISSET(tc->sock, &rfds))
       	{
            // select will always has data in socket
#ifdef _OPEN_SECURED_CONN
            if (tc->ssl)
                dlen = ssl_server_recv(tc->ssl, tc->sock, (unsigned char *)data, SIZE_OF_MSGBUF+16);
            else
#endif            
                dlen = recv(tc->sock, (void *)data, SIZE_OF_MSGBUF+16, 0);
            if (dlen<=0)
            {   
                ret = dlen;
                p2p_log_msg("%s(): sock %d read error %d, errno=%d\n",__FUNCTION__, tc->sock, dlen, errno);
                break;
            }    
            
           	tc->time = get_time_milisec() + 10000;  // extend 10 seconds
            // put received data in queue
            len = queue_put(&tc->rxqu,data,dlen);
            if (len != dlen)
            {
                p2p_log_msg("%s(): warning! put data in queue fail %d,%d \n",__FUNCTION__, dlen, len);
            }    
            // process received data from peer
            ret=tunnel_recv_process(tc, data, SIZE_OF_MSGBUF+16);
	        if (ret!= TUNN_OK) // got error
	        {
  	            p2p_log_msg("%s(): tunnel process error %d!",__FUNCTION__, ret);
	            break; 
            }
    	} 	
    	
       	// local http connection 	
        if (FD_ISSET(tc->http_sock, &rfds))
        {
            // 
            //  accept: wait for a connection request 
            //
            clientlen = sizeof(clientaddr);
#ifdef _OPEN_SECURED_CONN
            SSL *ssl;
            
            sd = ssl_server_accept(tc->http_sock,&ssl);
#else            
            sd = accept(tc->http_sock, (struct sockaddr *) &clientaddr, &clientlen);
#endif            
            if (sd >= 0)
            {    
            	addrlen = sizeof peeraddr;
            	getpeername(sd, (struct sockaddr*)&peeraddr, &addrlen);
#ifdef _OPEN_SECURED_CONN
                ret = new_link_process_local(tc, sd, HTTP_PORT, &peeraddr, ssl);
#else            	
            	// accetp new link from local
                ret = new_link_process_local(tc, sd, HTTP_PORT, &peeraddr);
#endif                
                if (ret<0)
                {
                    p2p_log_msg("%s():reject http connection from %s\n",__FUNCTION__, inet_ntoa (clientaddr.sin_addr));
                    close(sd);
                }    
            }    
            else     
                p2p_log_msg("ERROR HTTP on accept\n");
        }
        
        // local http connection 	
        if (FD_ISSET(tc->rtsp_sock, &rfds))
        {
            // 
            //  accept: wait for a connection request 
            //
            clientlen = sizeof(clientaddr);
            sd = accept(tc->rtsp_sock, (struct sockaddr *) &clientaddr, &clientlen);
            if (sd >= 0)
            {    
                addrlen = sizeof peeraddr;
            	getpeername(sd, (struct sockaddr*)&peeraddr, &addrlen);
            	// accetp new link from local
#ifdef _OPEN_SECURED_CONN
                ret = new_link_process_local(tc, sd, RTSP_PORT, &peeraddr, NULL);
#else            	
                ret = new_link_process_local(tc, sd, RTSP_PORT, &peeraddr);
#endif                
                if (ret<0)
                {
                    p2p_log_msg("%s():reject rtsp connection from %s\n",__FUNCTION__, inet_ntoa (clientaddr.sin_addr));
                    close(sd);
                }    
            }    
            else     
                p2p_log_msg("ERROR RTSP on accept\n");
        }
        // servcie tunnels processing
        tunnel_link_process(tc, &rfds, &wfds);
        // time process
        tunnel_time_process(tc);
        
        // error handler if rfd not be cleared?
        
    }   // end of while
    
end_tunnel_device_handler:  
    // if on line, then it should do offline?
#ifndef TEST_TUNNEL        
    if (tc->state == TUNNEL_SERVICE)
    {   
        // unregistry database and CVR system will move to SC
        /*ret = do_device_registry(tc,0);
        if (ret < 0)
        {
            p2p_log_msg("unregistry to CVR or cloud database fail %d!\n",ret);
        }*/
        // unregistry SC 
        ret = do_device_info(tc, 0, NULL);
        if (ret < 0)
        {
            p2p_log_msg("unregistry to SC fail %d!\n",ret);
        }
    }    
    do_tunnel_report("off line",tc);
#endif        

    tunnel_close(tc); 
    // remove from active list
    remove_active_tunnel(tc);
	put_tunnel_block(&tunnel_free_list, tc);
	
    p2p_log_msg("tunnel exit\n");
    pthread_exit(NULL);
    return NULL;
}   // endo fo tunnel server 

//
//  function: new_device_tunnel
//      handle the new device connection, add to list 
//      fork a new process to handle the device's traffic and then start do auth
//      parameters
//          socket id,
//          address of connection
//      return
//          0       success
//          other   fail
//
#ifdef _OPEN_SECURED_CONN
int new_device_tunnel(int sid,struct sockaddr_in *addr, SSL *ssl)
#else
int new_device_tunnel(int sid,struct sockaddr_in *addr)
#endif
{
    pthread_attr_t attr;
    TUNNEL_CTRL *tc = get_free_tunnel_block();
    
    printf("%s\n",__FUNCTION__);
    
    if (!tc)
        return TUNN_NOT_AVAILABLE;
    tc->sock = sid;
    tc->state = TUNNEL_CONNECTING;					 // do connect to TC and sent information
    *(unsigned int *)tc->ip = addr->sin_addr.s_addr; // device's the IP
    tc->port = ntohs(addr->sin_port);
    queue_init(&tc->rxqu, XMT_QU_SIZE);				// buffer size 32KB
    queue_init(&tc->txqu, TUNN_QU_SIZE);            // buffer size 512KB
    tc->do_exit = 0;
#ifdef _OPEN_SECURED_CONN
    tc->ssl = ssl;
#endif    
    tc->http_sock = -1;
    tc->rtsp_sock = -1;
    tc->http_port = 0;
    tc->rtsp_port = 0;
    memset(tc->link, 0, sizeof(TUNN_LINK)*MAX_LINK_NUM);
    // start a thread running    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	// start the device service thread
    pthread_create(&tc->tunnel_thread, &attr, tunnel_device_handler, (void *)tc);
    return TUNN_OK;
}


//
//  function: free_device_tunnel
//      find all active device thread and cforce to close it
//      parameters
//          none	free all active device process
//			tc		free specified tc
//      return
//          0       success
//          other   fail
//
int free_device_tunnel(void *p)
{
    char *retstr;
    TUNNEL_CTRL *tc = (TUNNEL_CTRL *)p;
    
    printf("%s\n",__FUNCTION__);
    if (!tc)
        return TUNN_NOT_AVAILABLE;
    tc->do_exit =1;
    // wait thread complete
    pthread_join(tc->tunnel_thread, (void **)&retstr);
    p2p_log_msg("%s(): exit %s!\n", __FUNCTION__, retstr);
    return TUNN_OK;
}
   
//
//  function: tunnel_update_counter
//      calculate a statistic counters
//      parameters
//          none
//      return
//          none
//   
void tunnel_update_counter(TUNNEL_CTRL *tc)
{
    tc->rx_pcnt_ps = tc->rx_pcnt;
    tc->rx_pcnt = 0;
    tc->rx_bcnt_ps = tc->rx_bcnt;
    tc->rx_bcnt = 0;
    tc->tx_pcnt_ps = tc->tx_pcnt;
    tc->tx_pcnt = 0;
    tc->tx_bcnt_ps = tc->tx_bcnt;
    tc->tx_bcnt = 0;
    
    //p2p_log_msg("%s():\ntx: %u p/s, %u B/s, rx: %u p/s, %u B/s, echo %u, tx queued %u, rx queue %u\n", __FUNCTION__,
    //    tc->tx_pcnt_ps, tc->tx_bcnt_ps, tc->rx_pcnt_ps,tc->rx_bcnt_ps, tc->echo_count, 
    //    queue_data_size(&tc->txqu), queue_data_size(&tc->rxqu));
}
