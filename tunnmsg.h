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
//  second byte : 0x00: bi-direction traffic, 0x01 request message, 0x02 response message
//                0x80 encryption, 0x00, not encryption 
#define DATA_TAG        0xFF00
#define REQ_TAG         0xFF01
#define RSP_TAG         0xFF02
#define ENCRYPT_TAG     0x0080

// maximum data in a message is 1460 bytes, max TCP payload size in a ethernet packet
#define MIN_TUNNEL_DATALEN   4                      // only signal message
#define MAX_TUNNEL_DATALEN   1460

#define MIN_TUNNEL_MSGHEAD   4                      // tag(2B) + len(2B)
#define MIN_TUNNEL_MSGLEN    (4+MIN_TUNNEL_DATALEN) 
#define MAX_TUNNEL_MSGLEN    (4+MAX_TUNNEL_DATALEN)                  

#pragma pack(1)

// traffic of links 
struct _link_head_
{
    char            signal;                     // signal of link, NONE, SYN, SYNACK, SYNNAK, FIN, FINACK, 
    char            id;                         // link id
    unsigned short  dlen;                       // real data length without padding
    char            data[MAX_TUNNEL_DATALEN];   // real data without padding
};

struct _tunnel_msg_
{
    unsigned short tag;     // message TAG
    unsigned short len;     // length of data in this message include padding.
    // following field may or may not encrypt
    char data[sizeof(struct _link_head_)];   // start address of encryption message    
    char padding[0];        // start address of padding field
};

int check_msghead(struct _queue_ *q,const unsigned short tag);
int encode_msg(TUNNEL_CTRL *tc, unsigned short tag, char *data, int dlen, char *buf, int bufsize);


#pragma pack()
#endif