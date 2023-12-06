
/* * * *
 * "relayChatClient.c"
 * RelayChat by Micah Lorenz and Tierney McBride
 * * * * */

// gcc relayChatClient.c common.c -o realyChatClient

#include "relayChat.h"
#include <string.h>

struct room;
typedef struct room
{
    char roomName[20];
    char members[MAXUSERS][20];
    int count;
    struct room *next;
} room_t;

int keepalive(int sock, time_t *lastSent, time_t *lastRecvd);
int receiver(int sock, time_t *lastRecvd, void *recvBuf);
int sender(int sock, time_t *lastSent, char *inputBuf);
void updateRoomMembership(struct irc_packet_list_resp *input);

// pthread_mutex_t timestamp_mutex;
char username[30] = "";

// pthread_mutex_t rooms_mutex;
room_t *roomHead = NULL;

// CONNECTING TO NETWORK
int get_socket_list(char *hostname, struct addrinfo *first_link)
{
    int ret, socket_fd;
    struct addrinfo pre_info;

    if ((ret = getaddrinfo(hostname, PORT, &pre_info, &first_link)) != 0)
    {
        fprintf(stderr, "\033[31mgetaddrinfo: %s\033[0m\n", gai_strerror(ret));
        return -1;
    }

    return ret;
}

int find_and_bind_socket(struct addrinfo **server_info, struct addrinfo *valid_sock)
{
    int socket_fd;

    for (valid_sock = server_info; valid_sock != NULL; valid_sock = valid_sock->ai_next)
    {
        if ((socket_fd = socket(valid_sock->ai_family, valid_sock->ai_socktype,
                                valid_sock->ai_protocol)) == -1)
        {
            perror("\033[31mclient: socket\033[0m\n");
            continue;
        }

        if (connect(socket_fd, valid_sock->ai_addr, valid_sock->ai_addrlen) == -1)
        {
            close(socket_fd);
            perror("\033[31mclient: connect\033[0m\n");
            continue;
        }

        break;
    }

    // error checking?
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
    //for send_time = 3 seconds:
        //send_heartbeat()
    //if time elapsed > 15 seconds and no received:
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

int keepalive(int sock, time_t *lastSent, time_t *lastRecvd)
{
    time_t now = time(NULL);
    int connected = 1;

    irc_packet_heartbeat_t pulse;
    pulse.header.opcode = IRC_OPCODE_HEARTBEAT;
    pulse.header.length = 0;
    now = time(NULL);
    //printf("Checking heartbeat %lu\n", now);

    if (now - *lastRecvd >= 15)
    { // server is not sending heartbeats, assume connection lost
        // kill sender and receiver threads, set timeout flag and return to main.
        printf("\033[31mstale heartbeat\033[0m\n");
        return 1;
        // Attempt to reconnect?
    }
    if (now - *lastSent >= 3)
    { // Outgoing heartbeat is stale
        // send heartbeat
        //printf("sending heartbeat\n");
        send(sock, &pulse, sizeof(irc_packet_heartbeat_t), 0);
        *lastSent = time(NULL);
    }
    return 0;
}

int receiver(int sock, time_t *lastRecvd, void *recvBuf)
{
    size_t capacity = sizeof(struct irc_packet_tell_msg) + MAXMSGLENGTH * sizeof(char);
    // if(!recvBuf)
    // {
    //     recvBuf = malloc(capacity);
    // }
    struct irc_packet_tell_msg *message = NULL;
    struct irc_packet_list_resp *roomlist = NULL;
    int result = 0;
    struct irc_packet_error output;
    irc_packet_error_t reply;

    size_t size = 0;
    // while(1);
    //{

    size = recv(sock, recvBuf, capacity, 0);
    
    *lastRecvd = time(NULL);
    
    struct irc_packet_generic *input = recvBuf;
    //printf("Packet opcode: %u\n", input->header.opcode);
    switch (input->header.opcode)
    {
    case IRC_OPCODE_ERR:
        // disconnect from server and terminate
        printf("\033[31mKicked by the server, disconnecting from server...\033[0m\n");
        return -1;
        break;
    case IRC_OPCODE_HEARTBEAT:
        // no work necessary, timestamp already updated
        break;
    case IRC_OPCODE_LIST_ROOMS_RESP:
        roomlist = (struct irc_packet_list_resp *)input;
        // struct irc_packet_list_resp *roomlist = (struct irc_packet_list_resp *)input;
        printf("\033[32mThe rooms currently available are: \n");
        for (int i = 0; i < (roomlist->header.length / 20 - 1); ++i)
        {
            printf("\t%.20s\n", roomlist->item_names[i]);
        }
        printf("\033[0m\n");
        break;
    case IRC_OPCODE_LIST_USERS_RESP:
        updateRoomMembership((struct irc_packet_list_resp *)input);
        break;
    case IRC_OPCODE_TELL_MSG:
    case IRC_OPCODE_TELL_PRIV_MSG:
        message = recvBuf;
        result = validate_string(message->sending_user, 20);
        if (message->header.length > 40 + MAXMSGLENGTH || message->header.length < 40)
        {
            // err invalid length
            //irc_packet_error_t reply;
            reply.header.opcode = IRC_OPCODE_ERR;
            reply.header.length = 4;
            reply.error_code = IRC_ERR_ILLEGAL_LENGTH;
            send(sock, &reply, sizeof(irc_packet_error_t), 0);
            return -1;
        }
        if (result != 1)
        {
            // err bad string
            //irc_packet_error_t reply;
            reply.header.opcode = IRC_OPCODE_ERR;
            reply.header.length = 4;
            reply.error_code = IRC_ERR_ILLEGAL_MESSAGE;
            send(sock, &reply, sizeof(irc_packet_error_t), 0);
            return -1;
        }
        result = validate_string(message->target_name, 20);
        if (result != 1)
        {
            // err bad string
            reply.header.opcode = IRC_OPCODE_ERR;
            reply.header.length = 4;
            reply.error_code = IRC_ERR_ILLEGAL_MESSAGE;
            send(sock, &reply, sizeof(irc_packet_error_t), 0);
            return -1;
        }
        result = validate_string(message->msg, message->header.length - 40);
        if (result != 1)
        {
            // err bad string
            //irc_packet_error_t reply;
            reply.header.opcode = IRC_OPCODE_ERR;
            reply.header.length = 4;
            reply.error_code = IRC_ERR_ILLEGAL_MESSAGE;
            send(sock, &reply, sizeof(irc_packet_error_t), 0);
            return -1;
        }
        //if (strncmp(username, message->target_name, 20) == 0)
        //{
            fprintf(stdout, "\033[36m%.20s to %.20s: %.*s\033[0m\n", message->sending_user, message->target_name, MAXMSGLENGTH, message->msg);
        //}
        break;
    default:
        // invalid opcode
        
        printf("\033[31mReceived invalid data, disconnecting from server...\033[0m\n");
        
        output.error_code = IRC_ERR_ILLEGAL_OPCODE;
        output.header.opcode = IRC_OPCODE_ERR;
        output.header.length = 4;
        send(sock, &output, sizeof(struct irc_packet_error), 0);
        // disconnect from server and terminate
        return -1;
        break;
    }

    //}
    return 0;
}

int sender(int sock, time_t *lastSent, char *inputBuf)
{
    char *rname = NULL;
    char *msgbody = NULL;
    int msglength = 0;
    struct irc_packet_send_msg *roommsg = NULL;
    struct irc_packet_list_rooms outgoing;
    irc_packet_leave_t leavemsg;
    room_t *current = NULL;
    room_t *previous = NULL;
    int notFound = 1;
    // while (1)
    //{
    // read a line from the user
    memset(inputBuf, '\0', MAXINPUTLENGTH * sizeof(char)); // Clear out the input buffer so it can satisfy the string expectations
    //read(stdin, inputBuf, MAXINPUTLENGTH - 1);//compatible with pthread_cancel
    fgets(inputBuf, MAXINPUTLENGTH - 1, stdin); // the version I would normally use
    inputBuf[strlen(inputBuf) - 1] = '\0';//trim off the '\n'
    //fflush(stdin);
    // parse the line to identify a command
    if (inputBuf[0] == '\\')
    {
        switch (inputBuf[1])
        {
        case 'r': // send message to only one room
                  // usage \r roomname msgbody
        case 'u': // send message to only one user
            // usage \u username msgbody
            
            strtok(inputBuf, " ");
            rname = strtok(NULL, " ");
            msgbody = strtok(NULL, "\0\n");
            if (msgbody == NULL)
            {
                if (inputBuf[1] == 'r')
                {
                    printf("usage \\r username msgbody\n");
                    return -2;
                }
                else
                {
                    printf("usage \\u username msgbody\n");
                    return -2;
                }
            }
            msglength = strlen(msgbody) + 1;
            //printf("Message length %d\n", msglength);
            
            roommsg = malloc(sizeof(struct irc_packet_send_msg) + msglength);
            if (inputBuf[1] == 'r')
            {
                roommsg->header.opcode = IRC_OPCODE_SEND_MSG;
                //printf("Public message\n");
            }
            else
            {
                roommsg->header.opcode = IRC_OPCODE_SEND_PRIV_MSG;
                //printf("Private message\n");
            }
            roommsg->header.length = 20 + msglength;
            memset(roommsg->target_name, '\0', 20);
            memset(roommsg->msg, '\0', msglength);
            strncpy(roommsg->target_name, rname, 20);
            //printf("Destination room: %.20s.\n", roommsg->target_name);
            strncpy(roommsg->msg, msgbody, msglength);
            send(sock, roommsg, sizeof(struct irc_packet_send_msg) + msglength, 0);
            free(roommsg);
            *lastSent = time(NULL);
            
            break;
        case 'j': // join (or create) a room
            // usage \j roomname
            if (strlen(inputBuf) <= 3)
            {
                printf("usage \\j roomname\n");
                return -2;
            }
            else
            {
                // prepare message to send
                struct irc_packet_join joinmsg;
                joinmsg.header.opcode = IRC_OPCODE_JOIN_ROOM;
                joinmsg.header.length = 20;
                memset(joinmsg.room_name, '\0', 20);
                memcpy(joinmsg.room_name, inputBuf + 3, 20);
                int inRoom = 0;

                // pthread_mutex_lock(&rooms_mutex);
                // first check we are not already in the room
                current = roomHead;
                while (current)
                {
                    if (strncmp(joinmsg.room_name, current->roomName, 20) == 0)
                    {
                        printf("You are already a member of that room\n");
                        inRoom = 1;
                        return -2;
                    }
                    current = current->next;
                }
                // add room name to list of rooms
                if (!inRoom)
                {
                    room_t *newRoom = malloc(sizeof(room_t));
                    memset(newRoom, '\0', sizeof(room_t));
                    newRoom->next = roomHead;
                    memcpy(newRoom->roomName, joinmsg.room_name, 20);
                    newRoom->count = 0;
                    // now let the server know we want to join the room
                    send(sock, &joinmsg, sizeof(struct irc_packet_join), 0);
                    *lastSent = time(NULL);

                    roomHead = newRoom;
                }
            }
            break;
        case 'l': // list rooms
            // usage \l
            //struct irc_packet_list_rooms outgoing;
            outgoing.header.opcode = IRC_OPCODE_LIST_ROOMS;
            outgoing.header.length = 0;
            send(sock, &outgoing, sizeof(struct irc_packet_list_rooms), 0);
            *lastSent = time(NULL);
            break;
        case 'e': // exit a room
            // usage \e roomname
            if(strlen(inputBuf) <= 3)
            {
                printf("usage \\e roomname\n");
                return -2;
            }
            //irc_packet_leave_t leavemsg;
            memset(&leavemsg, '\0', sizeof(irc_packet_leave_t));
            strncpy(leavemsg.room_name, inputBuf + 3, 20);
            leavemsg.header.opcode = IRC_OPCODE_LEAVE_ROOM;
            leavemsg.header.length = 20;

            // pthread_mutex_lock(&rooms_mutex);
            // loop through rooms to find matching one and remove it, send leaving packet
            current = roomHead;
            previous = NULL;
            notFound = 1;
            while (current && notFound)
            {
                if (strncmp(leavemsg.room_name, current->roomName, 20) == 0)
                {
                    notFound = 0;
                    send(sock, &leavemsg, sizeof(irc_packet_leave_t), 0);
                    *lastSent = time(NULL);
                    if (!previous)
                    {
                        roomHead = current->next;
                    }
                    else
                    {
                        previous->next = current->next;
                    }
                    free(current);
                }
                else
                {
                    previous = current;
                    current = current->next;
                }
            }
            if (notFound)
            {
                printf("You are not a member of that room.\n");
                return -2;
            }
            // pthread_mutex_unlock(&rooms_mutex);
            break;
        case 'q': // quit the server
            // usage \q
            // pthread_cancel(kaThrd);
            // pthread_cancel(recvThrd);
            // pthread_exit(0);
            return -1;
            break;
        default:
            // unrecognized command
            break;
        }
    }
    else
    { // forward message to all joined rooms
      // pthread_mutex_lock(&rooms_mutex);
        // loop through rooms, sent message packet to each
        // would be better to have a dedicated opcode to tell the server to broadcast to all joined rooms
        // because less total network traffic that way
        //char * rname = strtok(NULL, " ");
        char * msgbody = inputBuf;
        int msglength = strlen(msgbody) + 1;
        struct irc_packet_send_msg * broadcastmsg = malloc(sizeof(struct irc_packet_send_msg) + msglength);
        broadcastmsg->header.opcode = IRC_OPCODE_SEND_MSG;

        broadcastmsg->header.length = 20 + msglength;
        memset(broadcastmsg->msg, '\0', msglength);
        memcpy(broadcastmsg->msg, msgbody, msglength - 1);
        room_t * current = roomHead;
        while (current)
        {
            memset(broadcastmsg->target_name, '\0', 20);
            memcpy(broadcastmsg->target_name, current->roomName, 20);
            send(sock, broadcastmsg, sizeof(struct irc_packet_send_msg) + msglength, 0);
            current = current->next;
        }
        free(broadcastmsg);
        *lastSent = time(NULL);
    }
    return 0;
    // construct a packet struct according to the command and send it

    //}
}

void updateRoomMembership(struct irc_packet_list_resp *input)
{
    int inputCount = input->header.length / 20 - 1;
    room_t *current = roomHead;
    while (current && strncmp(current->roomName, input->identifier, 20) != 0)
    {
        current = current->next;
    }
    if (current) // matching room found
    {
        int myIndex = 0;
        int serverIndex = 0;
        int endSRC[MAXUSERS];
        int i = 0;
        while (serverIndex < inputCount && myIndex < current->count && i < MAXUSERS)
        {
            int result = strncmp(current->members[myIndex], input->item_names[serverIndex], 20);
            if (result == 0) // name matches
            {
                ++serverIndex;
                ++myIndex;
                endSRC[i] = serverIndex;
                ++i;
            }
            else if (result < 0)
            {
                // the user at current->members[myIndex] has left the room
                fprintf(stdout, "\033[31m%.20s left %.20s\033[m\n", current->members[myIndex], input->identifier);
                ++myIndex;
            }
            else
            {
                // new user has joined
                endSRC[i] = serverIndex;
                fprintf(stdout, "\033[32m%.20s joined %.20s\033[0m\n", input->item_names[serverIndex], input->identifier);
                ++serverIndex;
                ++i;
            }
        }
        while (serverIndex < inputCount && i < MAXUSERS)
        {
            // new user has joined
            fprintf(stdout, "\033[32m%.20s joined %.20s\033[0m\n", input->item_names[serverIndex], input->identifier);
            endSRC[i] = serverIndex;
            ++serverIndex;
            ++i;
        }

        for (int j = 0; j < i; ++j) // update the membership record of the room
        {
            memset(current->members[j], '\0', 20 * sizeof(char));
            strncpy(current->members[j], input->item_names[endSRC[j]], 20);
        }
        current->count = inputCount;
    }

    // else we are not a member of the room
}

int main(int argc, char *argv[])
{
    int socket_fd, numbytes;
    // char buf[MAXDATASIZE];
    struct addrinfo pre_info, *server_info, *valid_sock;
    int ret;
    char s[INET6_ADDRSTRLEN];
    int repeat = 1;
    int sock = 0;
    int timeout = 0;
    void *recvBuf = NULL;  // the buffer used by the receiver to hold incoming packets
    char *inputBuf = NULL; // the buffer used by the sender to hold user input

    time_t lastSent;
    time_t lastRecvd;

    if (argc != 2)
    {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    memset(&pre_info, 0, sizeof pre_info);
    pre_info.ai_family = AF_UNSPEC;
    pre_info.ai_socktype = SOCK_STREAM;

    // init behavior
    // Get username from user
    irc_packet_hello_t greeting;
    
    memset(greeting.username, '\0', 20);
    greeting.version = 1;
    printf("Welcome to the IRC client, please enter your desired username:\n");
    fgets(username, 30, stdin);
    username[strnlen(username, 25) - 1] = '\0';
    greeting.header.length = 24;
    strncpy(greeting.username, username, 20);
    greeting.header.opcode = IRC_OPCODE_HELLO;

    // connect to socket
    if ((ret = getaddrinfo(argv[1], PORT, &pre_info, &server_info)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return 1;
    }
    // REPLACE WITH:
    /*
    if ((ret = getSocketList(argv[1], &server_info)) == -1) {
        return -1;
    }
    */

    // loop thru, connect to first viable
    for (valid_sock = server_info; valid_sock != NULL; valid_sock = valid_sock->ai_next)
    {
        if ((socket_fd = socket(valid_sock->ai_family, valid_sock->ai_socktype,
                                valid_sock->ai_protocol)) == -1)
        {
            perror("client: socket");
            continue;
        }

        if (connect(socket_fd, valid_sock->ai_addr, valid_sock->ai_addrlen) == -1)
        {
            close(socket_fd);
            perror("client: connect");
            continue;
        }

        break;
    }
    // REPLACE WITH:
    /*
    socket_fd = findAndBindSocket(&server_info, valid_sock);
    */
    // error checking?

    if (valid_sock == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(valid_sock->ai_family, get_ip_addr((struct sockaddr *)valid_sock->ai_addr),
              s, sizeof s);
    printf("\033[32mclient: connecting to %s\033[0m\n", s);

    freeaddrinfo(server_info); // done with this

    /*
    if ((numbytes = recv(socket_fd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }

    buf[numbytes] = '\0';

    printf("client: received '%s'\n", buf);
    */
    // close(socket_fd);
    sock = socket_fd;

    greeting.header.opcode = IRC_OPCODE_HELLO;
    greeting.header.length = 24;
    greeting.version = 1;
    send(sock, &greeting, sizeof(irc_packet_hello_t), 0);

    ///////////////////////////////////////////////////////////////////////////////
    lastRecvd = time(NULL);
    lastSent = time(NULL);
    //memset(username, '\0', 20 * sizeof(char));

    struct pollfd streams[2];
    int activeCount = 0;
    memset(streams, 0, sizeof(streams));

    streams[0].fd = 0; // stdin
    streams[0].events = POLLIN;

    streams[1].fd = sock;
    streams[1].events = POLLIN;

    if (!inputBuf)
    {
        inputBuf = malloc(MAXINPUTLENGTH * sizeof(char));
    }

    size_t capacity = sizeof(struct irc_packet_tell_msg) + MAXMSGLENGTH * sizeof(char);
    if (!recvBuf)
    {
        recvBuf = malloc(capacity);
    }

    do
    {
        activeCount = poll(streams, 2, 2900);

        if (streams[0].revents & POLLIN)
        { // There is a line in stdin
            if (sender(sock, &lastSent, inputBuf) == -1)
            {
                //The user typed the quit command, so disconnect from the server
                repeat = 0;
            }
            streams[0].revents = 0;
        }
        

        if (streams[1].revents & POLLIN)
        { // there is an incoming packet waiting
            if (receiver(sock, &lastRecvd, recvBuf) == -1)
            {
                // receiver call returned an error
                // disconnect from server
                repeat = 0;
            }
            streams[1].revents = 0;
        }

        if(repeat)//no need to check heartbeat if we are planning to quit
        {
            //timeout = 0;
            timeout = keepalive(sock, &lastSent, &lastRecvd);
        }
        // timeout = 0;
        if (timeout)
        {

            // server disconnected due to timeout
            fprintf(stdout, "\033[31m server is unresponsive. Do you want to attempt to reconnect? (y/n)\033[0m\n");
            fgets(inputBuf, 100, stdin);
            fflush(stdin);
            if (inputBuf[0] == 'y') // if yes
            {
                close(sock); // disconnect from server
                repeat = 1;

                // attempt to reconnect

                memset(&pre_info, 0, sizeof pre_info);
                pre_info.ai_family = AF_UNSPEC;
                pre_info.ai_socktype = SOCK_STREAM;

                // connect to socket
                if ((ret = getaddrinfo(argv[1], PORT, &pre_info, &server_info)) != 0)
                {
                    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
                    return 1;
                }

                // loop thru, connect to first viable
                for (valid_sock = server_info; valid_sock != NULL; valid_sock = valid_sock->ai_next)
                {
                    if ((socket_fd = socket(valid_sock->ai_family, valid_sock->ai_socktype,
                                            valid_sock->ai_protocol)) == -1)
                    {
                        perror("client: socket");
                        continue;
                    }

                    if (connect(socket_fd, valid_sock->ai_addr, valid_sock->ai_addrlen) == -1)
                    {
                        close(socket_fd);
                        perror("client: connect");
                        continue;
                    }

                    break;
                }

                if (valid_sock == NULL)
                {
                    fprintf(stderr, "client: failed to connect\n");
                    return 2;
                }

                inet_ntop(valid_sock->ai_family, get_ip_addr((struct sockaddr *)valid_sock->ai_addr),
                          s, sizeof s);
                printf("\033[32mclient: connecting to %s\033[0m\n", s);

                freeaddrinfo(server_info); // done with this

                sock = socket_fd;
                streams[1].fd = sock;
                // send a fresh hello
                send(sock, &greeting, sizeof(irc_packet_hello_t), 0);
                lastSent = time(NULL);

                //receiver(sock, &lastRecvd, recvBuf); // wait to receive the server hello and process it
                // recv(sock, recvBuf, capacity, 0);

                // Rejoin rooms
                room_t *current = roomHead;
                irc_packet_join_t toJoin;
                toJoin.header.opcode = IRC_OPCODE_JOIN_ROOM;
                toJoin.header.length = 20;
                while (current)
                {
                    memset(toJoin.room_name, '\0', 20);
                    strncpy(toJoin.room_name, current->roomName, 20);
                    send(sock, &toJoin, sizeof(struct irc_packet_join), 0);
                    lastSent = time(NULL);
                }

            }
            else
            {
                repeat = 0;
                continue;
            }
        }
        // threads terminate on 3 conditions:
        // 1: The uesr quits
        //       No need to loop
        // 2: The server kicks the user
        //       Depending on error, client state may be incompatible with server, so terminate
        // 3: The heartbeat times out.
        //       Ask the user if they want to reconnect
    } while (repeat); //

    if (inputBuf)
    {
        free(inputBuf);
    }
    if (recvBuf)
    {
        free(recvBuf);
    }
    room_t *temp = NULL;
    while (roomHead)
    {
        temp = roomHead;
        roomHead = roomHead->next;
        free(temp);
    }

    // close the socket

    close(sock);


/*
    // nonsense tests start here
    struct irc_packet_send_msg *test = malloc(sizeof(struct irc_packet_send_msg) + 27 * sizeof(char));

    strcpy(test->msg, "abcdefghijklmnopqrstuwxyz");

    printf("%s\n", test->msg);

    fprintf(stdout, "%lu\n", sizeof(char));
    printf("%.7s\n", "0123456789");

    int count = 5;

    while (count >= 0)
    {
        printf("%s\n", "snore");
        sleep(10);
        --count;
    }
*/
    exit(0);
}
