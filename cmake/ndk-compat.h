/* AmigaPorts/newlib headers before the NDK's network declarations. */
#include <sys/types.h>
#include <sys/socket.h>
#if !defined(_SS_MAXSIZE) && !defined(_SOCKADDR_STORAGE_DEFINED)
struct sockaddr_storage {
    unsigned char ss_len;
    unsigned char ss_family;
    char ss_pad[126];
};
#endif
