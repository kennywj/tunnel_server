//
//  module: device tunnel daemon
//      tunnel.h
//      active to builde up tunnel with specified server, multiplex/demutiplex traffice 
//      from server to local service or vice vera
//
#include <stdarg.h>

//
// tunnel auth message
//
#define _tunnel_req_msg_    \
"{\"uid\":\"%s\",\"session_id\":\"%s\",\"action\":\"%s\"}\r\n"
#define _tunnel_resp_msg_    \
"{\"action\":\"%s\",\"stat\": %d}\r\n"
// registry device tunnel
//#define sc_auth_url "http://52.196.179.234:3601/OnLineStatus"
#define sc_auth_url "http://127.0.0.1:3601/OnLineStatus"
#define  _sc_auth_msg_  \
"{\"uid\":\"%s\",\"sessionId\":\"%s\",\"onLineStatus\":%d,\"httpPort\":%u,\"rtspPort\":%u}\r\n"
//#define sc_report_url "http://52.196.179.234:3601/TunnelReport"
#define sc_report_url "http://127.0.0.1:3601/TunnelReport"
#define _tunnel_dev_msg_ \
    "{\"uid\":\"%s\",\"ip\":\"%u.%u.%u.%u\",\"port\":%u,\"sock\":%u,\"state\":\"%s\",\"session_id\":\"%s\",\"session_key\":\"%s\",\"cvr_ip\":\"%u.%u.%u.%u\","   \
    "\"http_port\":%u,\"http_sock\":%u,\"rtsp_port\":%u,\"rtsp_sock\":%u,\"tx_pcnt\":%u,\"tx_bcnt\":%u,\"rx_pcnt\":%u,\"rx_bcnt\":%u,\"echo\":%u,"  \
    "\"tx queued\":%u,\"rx queued\":%u,\"links\":[%s]}"
#define _link_blk_msg_    \
    "{\"id\":%u,\"state\":\"%s\",\"addr\":\"%s\",\"port\":%u,\"sock\":%u,\"tx_pcnt\":%u,\"tx_bcnt\":%u,\"rx_pcnt\":%u,\"rx_bcnt\":%u,\"queued\":%u}"
    
#define _tunnel_dev_status_ \
    "{\"uid\":\"%s\",\"ip\":\"%u.%u.%u.%u\",\"port\":%u,\"sock\":%u,\"state\":\"%s\",\"session_id\":\"%s\",\"session_key\":\"%s\",\"cvr_ip\":\"%u.%u.%u.%u\","   \
    "\"http_port\":%u,\"http_sock\":%u,\"rtsp_port\":%u,\"rtsp_sock\":%u,\"msg\":\"%s\"}" 
    
//    
// registry CVR to SC && CVR
//    
//#define dev_registry_cvr_url "http://52.26.130.53:8080/record/config_camera_info"
#define dev_registry_cvr_url "http://127.0.0.1:80/record/config_camera_info"

#define  _dev_registry_msg_  \
"{\"uid\":\"%s\",\"cvrIp\":\"%s\",\"cvrPort\":%u,\"cameraRtspPort\":%u,\"cameraHttpPort\":%u,\"sessionKey\":\"%s\",\"onlineStatus\":%d}\r\n"

//#define dev_registry_clouds_url "http://127.0.0.1:3601/OnLineStatus"


//
// CVR service plan API
//
#define dev_cvr_service_plan_url "http://127.0.0.1:80/record/config_info"

#define  _dev_service_plan_msg_ \
"{\"uid\":\"%s\",\"starttime\":\"%u\",\"expiretime\":\"%u\",\"keepdays\":\"%u\",\"record\":\"%u\",\"liveview\":\"%u\"}\r\n"

//
// lanteam intferface API
//
#define dev_registry_db_url "http://lanetteam.com:8554/camerastreamurl"

#define  _dev_registry_db_msg_ \
    "{\"UID\":\"%s\",\"CameraStatus\":%d,\"LiveUrl\":\"%s\",\"PlayBackUrl\":\"%s\",\"CVRIP\":\"%s\",\"CVRPort\":%u,\"CameraRtspPort\":%u,\"CameraHttpPort\":%u,\"SessionKey\":\"%s\"}"

extern int tunnel_ctrl_init(void);
extern void tunnel_ctrl_free(void);
extern int free_device_tunnel(void *tc);
extern int clouds_service(char *url, char *cmd, char *resp, int bufsize);
extern int clouds_put_service(char *url, char *cmd, char *resp, int bufsize);

#ifdef _OPEN_SECURED_CONN
extern int ssl_server_init(char *cacert, char *keyfile);
extern int ssl_server_accept(int server_sid, SSL **);
extern int ssl_server_close(SSL *ssl);
extern int ssl_server_recv(SSL *ssl,int sockid, char *buf, int size);
extern int ssl_server_send(SSL *ssl,int sockid, char *buf, int size);
extern int new_device_tunnel(int sid,struct sockaddr_in *addr, SSL *);
#else
extern int new_device_tunnel(int sid,struct sockaddr_in *addr);
#endif

//#define p2p_log_msg printf
extern int p2p_log_msg(char *fmt, ...);

