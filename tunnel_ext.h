//
//  module: device tunnel daemon
//      tunnel.h
//      active to builde up tunnel with specified server, multiplex/demutiplex traffice 
//      from server to local service or vice vera
//


extern int tunnel_ctrl_init(void);
extern int free_device_tunnel(void *tc);
extern int new_device_tunnel(int sid,struct sockaddr_in *addr);

