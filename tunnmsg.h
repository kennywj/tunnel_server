//
//  module: device tunnel module
//      tunnmsg.h
//      active to builde up tunnel with specified server, multiplex/demutiplex traffice 
//      from server to local service or vice vera
//
#ifndef _TUNNMSG_
#define _TUNNMSG_

//
// define start of message tag, first byte is 0xFF
//  second byte : 0x00: bi-direction traffic, 0x01 control message
//                0x80 encryption, 0x00, not encryption 
#define START_TAG       0xFF
#define DATA_TAG        0x00
#define MSG_TAG         0x01
#define ENCRYPT_TAG     0x80

// maximum data in a message is 1460 bytes, max TCP payload size in a ethernet packet
#define MIN_TUNNEL_DATALEN   4                      // only signal message
#define MAX_TUNNEL_DATALEN   1420                   // avoid tcp fragment

#define MIN_TUNNEL_MSGHEAD   4                      // tag(2B) + len(2B)
#define MIN_TUNNEL_MSGLEN    (4+MIN_TUNNEL_DATALEN) 
#define MAX_TUNNEL_MSGLEN    (4+MAX_TUNNEL_DATALEN)                  

#define SYN 0x01
#define FIN 0x02
#define ACK 0x80
#define RST 0x40

#pragma pack(1)
// traffic of links 
struct _link_head_
{
    char            signal;                     // signal of link, NONE, SYN, SYNACK, SYNNAK, FIN, FINACK, 
    char            id;                         // link id
    unsigned short  dlen;                       // real data length without padding
    char            data[0];                    // real data without padding
};


struct _tunnel_msg_
{
    unsigned char tag[2];       // message TAG
    unsigned short len;         // length of data in this message include padding.
    // following field may or may not encrypt
    char data[0];               // start address of encryption message    
};
#pragma pack()
   // signal + id, + dlen
#define TUNNEL_LINK_HEAD sizeof(struct _link_head_)
   // tag + len
#define TUNNEL_MSG_HEAD sizeof(struct _tunnel_msg_)

#define TUNNEL_HEAD_LEN (TUNNEL_MSG_HEAD+TUNNEL_LINK_HEAD)


int check_msghead(struct _queue_ *q, char *type, char *encrypt);
int encode_msg(char *key, unsigned char tag, char *data, int dlen, char *buf, int bufsize);


#endif