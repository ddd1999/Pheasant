#ifndef TUDOSA_PHEASANT_COMMONS_H
#define TUDOSA_PHEASANT_COMMONS_H

#include <stdlib.h>
#include <string.h>

#define Sock int
#define SockAddr struct sockaddr_in
#define SockAddrPtr struct sockaddr *

#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 50000

#define ADDRESS(addr) htonl(addr)
#define PORT(port) htons(port)
#define ADDRESS_IPV4 AF_INET

#define NEW(dataType) (dataType *) malloc ( sizeof ( dataType ) )
#define NEW_ARR(dataType, elementCount) (dataType *) malloc ( sizeof ( dataType ) * elementCount )

static int readDisconnectFlag = 0;

#define READ_STRING(descriptor, string)  \
{ \
    int length = 0;                        \
    if ( 0 >= read ( descriptor, & length, sizeof ( int ) ) ) \
        readDisconnectFlag = 1;\
    if ( 0 >= read ( descriptor, string, length ) )           \
        readDisconnectFlag = 1;\
}

#define WRITE_STRING(descriptor, string) \
{ \
    int length = strlen ( string ) + 1;     \
    write ( descriptor, & length, sizeof ( int ) ); \
    write ( descriptor, string, length );   \
}

#define READ_INT(descriptor, value)  { read(descriptor, & value, sizeof (int)); }
#define WRITE_INT(descriptor, value) { write(descriptor, & value, sizeof(int)); }

#define STR_EQ(str1, str2) ( strcmp ( str1, str2 ) == 0)

#endif //TUDOSA_PHEASANT_COMMONS_H
