#include "relayChat.h"

#define MAXDATASIZE 2000
#define MAXMSGLENGTH 8000
#define POLL_MINS 5

void sender(int sock, char *inputBuf);
void receiver(int sock, char *buf);
int validate_string(char *text, int length);
void private_message(struct irc_packet_tell_msg *message);
void process_packet(struct irc_packet_generic *input);

char username[20] = "";

int main(int argc, char *argv[])
{

    int socket_fd, numbytes;
    int on = 1;
    // char buf[MAXDATASIZE];
    void *buf = NULL;
    char *inputBuf = NULL;
    struct addrinfo pre_info, *server_info, *valid_sock;
    int ret;
    char s[INET6_ADDRSTRLEN];

    // polling
    struct pollfd fds[2];
    int timeout = POLL_MINS * 60 * 1000;
    fds[0].fd = 0;
    fds[0].events = POLLIN;

    if (argc != 2)
    {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    memset(&pre_info, 0, sizeof pre_info);
    pre_info.ai_family = AF_UNSPEC;
    pre_info.ai_socktype = SOCK_STREAM;

    // connect to socket
    if ((ret = getaddrinfo(argv[1], PORT, &pre_info, &server_info)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return 1;
    }
    // REPLACE WITH:

    /*
    if ((ret = get_socket_list(argv[1], &server_info)) == -1) {
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

        // allow reuse
        /*  if ((setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))) < 0)
          {
              perror("setsockopt() failed");
              close(socket_fd);
              exit(-1);
          }
          */

        // set nonblocking
        /*if ((ioctl(socket_fd, FIONBIO, (char *)&on)) < 0)
        {
            perror("ioctl() failed");
            close(socket_fd);
            exit(-1);
        }
        */

        if (connect(socket_fd, valid_sock->ai_addr, valid_sock->ai_addrlen) == -1)
        {
            close(socket_fd);
            perror("client: connect");
            continue;
        }

        break;
    }
    /*
    //REPLACE WITH:
    socket_fd = find_and_bind_socket(&server_info, valid_sock);
    */
    // error checking?

    if (valid_sock == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(valid_sock->ai_family, get_ip_addr((struct sockaddr *)valid_sock->ai_addr),
              s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(server_info); // done with this

    fds[1].fd = socket_fd;
    fds[1].events = POLLIN;

    int close_connection = 0;
    // gather user info for hello
    memset(buf, '\0', 20 * sizeof(char));
    fgets(buf, 19, stdin);
    char *uname = strtok(NULL, "\n");
    memset(username, '\0', 20 * sizeof(char));
    strncpy(username, uname, strlen(uname));

    // construct and send hello packet
    struct irc_packet_hello *hello = malloc(sizeof(struct irc_packet_hello));
    memset(hello->username, '\0', 20);
    hello->header.opcode = IRC_OPCODE_HELLO;
    hello->header.length = 24;
    strncpy(hello->username, uname, strlen(uname));
    if (send(socket_fd, hello, sizeof(struct irc_packet_hello), 0) == -1)
        perror("send");

    do
    {

        if ((ret = poll(fds, 2, timeout)) < 0)
        {
            perror(" poll() failed");
            exit(1);
        }
        if (ret == 0)
        {
            printf("poll() timed out. End program.\n");
            break;
            close_connection = 1;
        }

        for (int i = 0; i < 2; ++i)
        {
            if (fds[i].revents == 0)
                continue;
            if (fds[i].revents != POLLIN)
            {
                printf("Error! revents =%d\n", fds[i].revents);
                exit(1);
            }
            if (i == 0)
                sender(socket_fd, inputBuf);
            else
                receiver(socket_fd, buf);
        }

        /*
        if (send(socket_fd, "Hello, world. I'm here.", 23, 0) == -1)
            perror("send");

        if ((numbytes = recv(socket_fd, buf, MAXDATASIZE - 1, 0)) == -1)
        {
            perror("recv");
            close_connection = 1;
        }

        if (numbytes == 0)
        {
            printf("Connection closed\n");
            close_connection = 1;
            break;
        }

        printf(buf);
        */

    } while (close_connection == 0);
    // check for user input

    close(socket_fd);

    return 0;
}

void sender(int sock, char *inputBuf)
{
    if (!inputBuf)
    {
        inputBuf = malloc(MAXDATASIZE * sizeof(char));
    }

    while (1)
    {
        // read a line from the user
        memset(inputBuf, '\0', MAXDATASIZE * sizeof(char)); // Clear out the input buffer so it can satisfy the string expectations
        fgets(inputBuf, MAXDATASIZE - 1, stdin);
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
                char *rname = strtok(NULL, " ");
                char *msgbody = strtok(NULL, " ");
                int msglength = strlen(msgbody) + 1;
                if (msglength <= 1)
                {
                    if (inputBuf[1] == 'r')
                    {
                        printf("usage \\r username msgbody\n");
                    }
                    else
                    {
                        printf("usage \\u username msgbody\n");
                    }
                }
                else
                {
                    struct irc_packet_send_msg *roommsg = malloc(sizeof(struct irc_packet_send_msg) + msglength);
                    if (inputBuf[1] == 'r')
                    {
                        roommsg->header.opcode = IRC_OPCODE_SEND_MSG;
                    }
                    else
                    {
                        roommsg->header.opcode = IRC_OPCODE_SEND_PRIV_MSG;
                    }
                    roommsg->header.length = 20 + msglength;
                    memset(roommsg->target_name, '\0', 20);
                    memset(roommsg->msg, '\0', msglength);
                    strncpy(roommsg->target_name, rname, msgbody - rname - 1);
                    send(sock, roommsg, sizeof(struct irc_packet_send_msg) + msglength, 0);
                    free(roommsg);
                }
                break;
            case 'j': // join (or create) a room
                // usage \j roomname
                if (strlen(inputBuf) <= 3)
                {
                    printf("usage \\j roomname\n");
                }
                else
                {
                    // prepare message to send
                    struct irc_packet_join joinmsg;
                    joinmsg.header.opcode = IRC_OPCODE_JOIN_ROOM;
                    joinmsg.header.length = 20;
                    memset(joinmsg.room_name, '\0', 20);
                    strncpy(joinmsg.room_name, inputBuf + 3, 20);

                    // first check we are not already in the room

                    // add room name to list of rooms

                    // now let the server know we want to join the room
                    send(sock, &joinmsg, sizeof(struct irc_packet_join), 0);
                }
                break;
            case 'l': // list rooms
                // usage \l
                // struct irc_packet_list_rooms outgoing;
                // outgoing.header.opcode = IRC_OPCODE_LIST_ROOMS;
                // outgoing.header.length = 0;
                // send(sock, &outgoing, sizeof(struct irc_packet_list_rooms), 0);
                break;
            case 'e': // exit a room
                // usage \e roomname
                break;
            case 'q': // quit the server
                // usage \q
                break;
            default:
                // unrecognized command
                break;
            }
        }
        else
        { // forward message to all joined rooms
        }

        // construct a packet struct according to the command and send it
    }
}

void receiver(int sock, char *buf)
{
    size_t capacity = sizeof(struct irc_packet_tell_msg) + MAXDATASIZE * sizeof(char);
    if (!buf)
    {
        buf = malloc(capacity);
    }

    size_t size = 0;
    while (1)
        ;
    {

        size = recv(sock, buf, capacity, 0);

        // pthread_mutex_lock(&timestamp_mutex);
        // lastRecvd = time(NULL);
        // pthread_mutex_unlock(&timestamp_mutex);
        char *to_process = malloc(size);
        strncpy(to_process, buf, size);
        process_packet(to_process);
    }
}

int validate_string(char *text, int length)
{
    if (length <= 0)
        return -1;
    if (text == NULL)
        return -2;

    int nullencountered = 0;
    for (int i = 0; i < length; ++i)
    {
        if (text[i] == '\0')
        {
            nullencountered = 1;
        }
        else if (nullencountered)
        { // after the first \0 there is a non-null character
            return -3;
        }
        else if ((text[i] < 0x20 || text[i] > 0x7E) && text[i] != '\n')
        { // non printable character in the string
            return -4;
        }
    }
    if (nullencountered == 0)
    {
        if (length != 20 && length != MAXMSGLENGTH)
        {
        }
    }
    return 1;
}

void process_packet(struct irc_packet_generic *input)
{
    switch (input->header.opcode)
    {
    case IRC_OPCODE_ERR:
        // disconnect from server and terminate
        // pthread_cancel(sendThrd);
        // pthread_cancel(kaThrd);
        // printf("Recieved inalid data, disconnecting from server...\n");
        // timeout = 0;
        return;
        break;
    case IRC_OPCODE_HEARTBEAT:
        // no work necessary, timestamp already updated
        break;
    case IRC_OPCODE_LIST_ROOMS_RESP:
        // struct irc_packet_list_resp *roomlist = (struct irc_packet_list_resp *)input;
        printf("The rooms currently available are: \n");
        // for (int i = 0; i < roomlist->header.length; ++i)
        //{
        //     printf("\t%.20s\n", roomlist->item_names[i]);
        // }
        break;
    case IRC_OPCODE_LIST_USERS_RESP:
        // updateRoomMembership((struct irc_packet_list_resp *)input);
        break;
    case IRC_OPCODE_TELL_MSG:
    case IRC_OPCODE_TELL_PRIV_MSG:
        private_message(input);
        break;
    default:
        // invalid opcode
        // struct irc_packet_error output;
        // output.error_code = IRC_ERR_ILLEGAL_OPCODE;
        // output.header.opcode = IRC_OPCODE_ERR;
        // output.header.length = 4;
        // send(sock, &output, sizeof(struct irc_packet_error), 0);
        // disconnect from server and terminate
        // pthread_cancel(kaThrd);
        // pthread_cancel(sendThrd);
        // pthread_exit();
        // close(sock);
        break;
    }
}

void private_message(struct irc_packet_tell_msg *message)
{
    int result = validate_string(message->sending_user, 20);
    if (message->header.length > 40 + MAXMSGLENGTH || message->header.length < 40)
    {
        // err invalid length
    }
    if (result != 1)
    {
        // err bad string
    }
    result = validate_string(message->target_name, 20);
    if (result != 1)
    {
        // err bad string
    }
    result = validate_string(message->msg, message->header.length - 40);
    if (result != 1)
    {
        // err bad string
    }
    if (strncmp(username, message->target_name, 20) == 0)
    {
        fprintf(stdout, "%.20s to %.20s: %.*s\n", message->sending_user, message->target_name, MAXMSGLENGTH, message->msg);
    }
}