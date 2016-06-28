/* ------------------------------------------------------------ *
 * file:        sslconnect.c                                    *
 * purpose:     Example code for building a SSL connection and  *
 *              retrieving the server certificate               *
 * author:      06/12/2012 Frank4DD                             *
 *                                                              *
 * gcc -lssl -lcrypto -o sslconnect sslconnect.c                *
 * ------------------------------------------------------------ */



#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <errno.h>
#include <ctype.h>
#include <sched.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <sys/mount.h>

#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <netdb.h>
#ifdef _OPEN_SECURED_CONN
#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/pem.h"
#include "openssl/x509.h"
#include "openssl/x509_vfy.h"
#endif
#include "tunnel_ext.h"

//#include "rtsp.h"
#ifdef _OPEN_SECURED_CONN

SSL_CTX *tls_ctx = NULL;
//
// load certification file
//
static int load_certs(SSL_CTX* ctx, char* CertFile, char* KeyFile)
{
    int ret = -1;
    /* set the local certificate from CertFile */
    if(SSL_CTX_use_certificate_file(ctx, CertFile, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        goto exit;
    }

    /* set the private key from KeyFile (may be the same as CertFile) */
    if(SSL_CTX_use_PrivateKey_file(ctx, KeyFile, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        goto exit;
    }

    /* verify private key */
    if(!SSL_CTX_check_private_key(ctx))
    {
        p2p_log_msg("%s():Private key does not match the public certificate\n",__FUNCTION__);
        goto exit;
    }

    ret = 0;

exit:
    return ret;
}

//
// initial SSL server size
//
int ssl_server_init(char *cacert, char *privkey)
{
    int ret = -1;

    OpenSSL_add_all_algorithms();
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();
    SSL_load_error_strings();

    if (SSL_library_init() < 0)
        goto exit;

    if ((tls_ctx = SSL_CTX_new(SSLv23_server_method())) == NULL)
    {
        p2p_log_msg("%s error %d\n", __FUNCTION__, __LINE__);
        goto exit;
    }

    if (load_certs(tls_ctx, cacert, privkey)<0)
        goto exit;
        
    ret = 0;

exit:
    return ret;
}

//
// close ssl 
//
int ssl_server_close(SSL *ssl)
{
    int sd;
   
    if (!ssl)
        return -1;
   
    sd = SSL_get_fd(ssl);
    if (sd >=0)
    {    
        p2p_log_msg("%s(): free socket %d\n", __FUNCTION__, sd);
        close(sd);
    }
    SSL_shutdown(ssl);
    SSL_free(ssl);
    return 0;
}

//
// do accept new device connection
//
int ssl_server_accept(int server_sid, SSL **ssl)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int sockid = -1;

    sockid = accept(server_sid, &addr, &len);
    if(sockid < 0) goto exit;

    *ssl = SSL_new(tls_ctx);
    if (*ssl==NULL)
    {
        close(sockid);
        sockid = -1;
        goto exit;
    }    
    SSL_set_fd(*ssl, sockid);

    int ret = SSL_accept(*ssl);
    if(ret < 0)
    {
        int q = SSL_get_error(*ssl, ret);

        unsigned int e = ERR_get_error();
        char errmsg[512] = {0};
        ERR_error_string(e, errmsg);
        p2p_log_msg("%s(): %d error %u %s\n", __FUNCTION__, q, e, errmsg);
        ssl_server_close(*ssl);
        sockid = -1;
    }

exit:
    return sockid;
}

//
//  receive from SSL
//
int ssl_server_recv(SSL *ssl, int sockid, char *buf, int size)
{
    int ret; //retry=0, ret;
    
    if(ssl)
    {
do_retry_rx:        
        ret = SSL_read(ssl, buf, size);
        if(ret < 0)
        {
            int q = SSL_get_error(ssl, ret);
            // data in processing
            if (q==SSL_ERROR_WANT_READ)
            {   
                //if (++retry<10)
                {    
                    usleep(100000);    //delay 100ms?
                    goto do_retry_rx;    
                }
            }
            unsigned int e = ERR_get_error();
            char errmsg[512] = {0};
            ERR_error_string(e, errmsg);
            p2p_log_msg("%s(): ret =%d, %d error %u %s\n", __FUNCTION__, ret,  q, e, errmsg);
        }

        return ret;
    }
    else
        return -1;
}

//
// ssl send function
//
int ssl_server_send(SSL *ssl,  int sockid, char *buf, int size)
{
    int ret;//retry=0;
    if(ssl)
    {
do_retry_tx:         
        ret = SSL_write(ssl, buf, size);
        //printf("%s write %d\n", __FUNCTION__, ret);
        if(ret < 0)
        {
            int q = SSL_get_error(ssl, ret);
            if (q==SSL_ERROR_WANT_WRITE)
            {   
                //if (++retry<3)
                {    
                    usleep(100000);    //delay 100ms?
                    goto do_retry_tx;    
                }
            }
            unsigned int e = ERR_get_error();
            char errmsg[512] = {0};
            ERR_error_string(e, errmsg);
            p2p_log_msg("%s(): %d error %u %s\n", __FUNCTION__, q, e, errmsg);
        }

        return ret;
    }
    else
        return -1;
}

#endif
//_OPEN_SECURED_CONN