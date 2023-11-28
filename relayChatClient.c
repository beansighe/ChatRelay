/* * * *
* "relayChatClient.c"
* RelayChat by Micah Lorenz and Tierney McBride
* 
* Client side program for an internet relay chat
* application. Takes server hostname as command
* line argument.
* * * * */

#include "relayChat.h"

#define MAXDATASIZE 100 // buffer size

int main(int argc, char *argv[]) {
    int socket_fd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo pre_info, *server_info, *valid_sock;
    int ret;
    char s[INET6_ADDRSTRLEN];


    if (argc != 2) {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    memset(&pre_info, 0, sizeof pre_info);
    pre_info.ai_family = AF_UNSPEC;
    pre_info.ai_socktype = SOCK_STREAM;


    //connect to socket
    if ((ret = getaddrinfo(argv[1], PORT, &pre_info, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return 1;
    }
    //REPLACE WITH:
    /*
    if ((ret = getSocketList(argv[1], &server_info)) == -1) {
        return -1;
    }
    */

    //loop thru, connect to first viable
    for (valid_sock = server_info; valid_sock != NULL; valid_sock = valid_sock->ai_next) {
        if ((socket_fd = socket(valid_sock->ai_family, valid_sock->ai_socktype, 
                valid_sock->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(socket_fd, valid_sock->ai_addr, valid_sock->ai_addrlen) == -1) {
            close(socket_fd);
            perror("client: connect");
            continue;
        }

        break;
    }
    //REPLACE WITH:
    /*
    socket_fd = findAndBindSocket(&server_info, valid_sock);
    */
   //error checking?

    if (valid_sock == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
    

    inet_ntop(valid_sock->ai_family, get_ip_addr((struct sockaddr *)valid_sock->ai_addr),
        s, sizeof s);
    printf("client: connecting to %s\n", s);
    
    freeaddrinfo(server_info); //done with this


    if ((numbytes = recv(socket_fd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }

    buf[numbytes] = '\0';

    printf("client: received '%s'\n", buf);

    close(socket_fd);

    exit(0);
}


//CONNECTING TO NETWORK
int get_socket_list(char *hostname, struct addrinfo *first_link) {
    int ret, socket_fd;
    struct addrinfo pre_info; 
    
    if ((ret = getaddrinfo(hostname, PORT, &pre_info, &first_link)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }

    return ret;
}

int find_and_bind_socket(struct addrinfo **server_info, struct addrinfo *valid_sock) {
    int socket_fd;

    for (valid_sock = server_info; valid_sock != NULL; valid_sock = valid_sock->ai_next) {
        if ((socket_fd = socket(valid_sock->ai_family, valid_sock->ai_socktype, 
                valid_sock->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(socket_fd, valid_sock->ai_addr, valid_sock->ai_addrlen) == -1) {
            close(socket_fd);
            perror("client: connect");
            continue;
        }

        break;
    }

    //error checking?
    return socket_fd;
}
//
/*
void send_user_data(char un, uint ip_addr) {
    //create and fill struct
    //create and fill packet
    //send on to socket
}

void send_msg(uint ip_addr) {
    //create and fill packet
    //send to socket
}

void heartbeat_mgmt(){
    //for send_time = 5 seconds:
        //send_heartbeat()
    //if time elapsed > 5 seconds and no received:
        //disconnect
};

FUNCTIONS FOR BOTH??
void send_heartbeat(socket, port) {
    //create packet
    //send to socket
}
void disconnect(){}; 
*/


/*
//BASIC UI

//read in username

//read in message

//display messages

//display room

//login

*/