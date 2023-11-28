/* * * *
* "relayChatServer.c"
* RelayChat by Micah Lorenz and Tierney McBride
* * * * */

#include "relayChat.h" 
#include <string.h>


struct addrinfo *serverInfo;
struct addrinfo hints;

int main(void) {
    char ipstr[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    //hints.ai_flags = AI_PASSIVE;
    int status = getaddrinfo(NULL, "3490", &hints, &serverInfo);
    if(status != 0)
    {
        fprintf(stderr, "getarrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    printf("IP addresses for %s:\n\n", "localhost");

    for(struct addrinfo * temp = serverInfo; temp != NULL; temp = temp->ai_next)
    {
        void * addr;
        char * ipver;

        if(temp->ai_family == AF_INET)
        {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)temp->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        }
        else
        {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)temp->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }
        inet_ntop(temp->ai_family, addr, ipstr, sizeof ipstr);
        printf(" %s: %s\n", ipver, ipstr);

    }

    freeaddrinfo(serverInfo);
    exit(0);
}
