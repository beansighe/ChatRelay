/* * * *
 * "relayChatServer.c"
 * RelayChat by Micah Lorenz and Tierney McBride
 * sources used in designing socket management:
 * <beej.us/guide/bgnet>,
 * <https://www.ibm.com/docs/en/i/7.1?topic=designs-using-poll-instead-select>
 * * * * */

// gcc relayChatServer.c common.c -o realyChatServer

#include "relayChat.h"

#define BACKLOG 10
#define POLL_MINS 5

// struct room_data; // if doing a linked list need advance declaration
typedef struct room_data
{
    char roomName[20];
    char users[MAXUSERS][20]; // the names of users in the room, sorted by name
    // this array is conveniently formatted for a memcopy into the outgoing list rooms packets
    int userIDs[MAXUSERS]; // the index where the user lives in the userData and file descriptor parallel arrays
    int countUsers;
    // room_data * next; //if a linked list
} room_data_t;

typedef struct client_data
{
    char name[20];
    int roomIDs[MAXROOMS]; // if room data is stored in an array, else use an array of pointers
    int countRooms;
    time_t lastSent;
    time_t lastRecvd;

} client_data_t;

int userCount = 0;
client_data_t userData[MAXUSERS + 1]; // + 1 to keep it parallel with the fds_poll array
room_data_t roomList[MAXROOMS];
int roomCount = 0;

// sockets to poll
struct pollfd *fds_poll;
int fds_ct, current_ct = 0;

int process_packet(struct irc_packet_generic *incoming, int sock, int index);
void manage_incoming_error(struct irc_packet_generic *error_msg, int sock, int index);
int hello(struct irc_packet_generic *incoming, int sock, int index);
int add_user(char *username, int sock, int index);
void manage_error(int sock, int index, uint32_t error_no);
int is_user(char *username);
void disconnect_client(int sock, int index);
int send_room_msg(irc_packet_send_msg_t *incoming, int sourceUser);
int private_message(struct irc_packet_generic *incoming, int socket, int index);
int keepalive(int userID);
void room_list(struct irc_packet_generic *incoming, int sock, int index);
int join_room(struct irc_packet_generic *incoming, int sock, int index);
void send_room_roster(room_data_t *room);
int create_room(char room_name[20], int user_index);
void remove_from_room(int room_index, int user_index);
int leave_room(struct irc_packet_generic *incoming, int sock, int index);
int add_to_room(int user_index, int room_index);
void update_user_data(room_data_t *to_update, int new_user_index);
int find_user(char *username);

int main(void)
{

    int ret, listen_fd, connect_fd; // catch on ret, listen on listen, new connect on connect
    int check;                      // for error checks
    int yes = 1;                    // for use in setsockopt()
    struct addrinfo *server_info, *valid_sock;
    struct addrinfo pre_info;
    struct sockaddr_storage connecter_addr;
    socklen_t sin_size; // for identifying whether incoming connection is ipv4 or ipv6
    char s[INET6_ADDRSTRLEN];
    // struct sigaction sig_action;

    int shutdown = FALSE;

    // thread stuff
    /*pthread_t thread;
    int thread_ret = 1;
    */

    // poll stuff
    fds_poll = malloc(sizeof(struct pollfd) * (1 + MAX_USERS)); // could change to be variable size for storage efficiency or to remove the user limit
    int timeout = 2500;//heartbeat frequency
    //POLL_MINS * 60 * 1000;             // mins to millisecs

    // incoming data mgmt
    char buffer[BUFSIZ];

    // find and bind socket
    memset(&pre_info, 0, sizeof pre_info);
    pre_info.ai_family = AF_UNSPEC;
    pre_info.ai_socktype = SOCK_STREAM;
    pre_info.ai_flags = AI_PASSIVE;

    // error check
    if ((ret = getaddrinfo(NULL, PORT, &pre_info, &server_info)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return 1; // how to communicate error back
    }

    for (valid_sock = server_info; valid_sock != NULL; valid_sock = valid_sock->ai_next)
    {
        if ((listen_fd = socket(valid_sock->ai_family, valid_sock->ai_socktype,
                                valid_sock->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        // allow reuse of socket descriptor
        if (check = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0)
        {
            perror("setsockopt() failed");
            close(listen_fd);
            exit(-1);
        }

        // make non-blocking: inherited socks will share this attribute
        /**/
        if ((check = ioctl(listen_fd, FIONBIO, &yes)) < 0)
        {
            perror("ioctl() failed");
            close(listen_fd);
            exit(-1);
        }
        /**/
        // bind socket
        if (bind(listen_fd, valid_sock->ai_addr, valid_sock->ai_addrlen) == -1)
        {
            close(listen_fd);
            perror("server: bind");
            continue;
        }

        break;
    }

    // free result holder
    freeaddrinfo(server_info);

    // prevent seg fault if uncaught error above
    if (valid_sock == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    // listen
    if (listen(listen_fd, BACKLOG) == -1)
    {
        perror("listen");
        close(listen_fd);
        exit(1);
    }

    printf("server: waiting for connections. . .\n");

    // set up for poll()
    // initialize pollfd storage
    memset(fds_poll, 0, sizeof(fds_poll));

    // add listener
    fds_poll[0].fd = listen_fd;
    fds_poll[0].events = POLLIN;

    fds_ct = 1;
    userCount = 0;
    do
    {

        // call poll
        printf("Polling sockets. . . \n");
        if ((check = poll(fds_poll, fds_ct, timeout)) < 0)
        {
            perror(" poll() failed");
            break;
        }

        // Identify readable sockets
        current_ct = fds_ct;
        for (int i = 0; i < current_ct; i++)
        {

            // no event
            if (fds_poll[i].revents == 0)
                continue;

            // nonzero and not equal to POLLIN is unexpected, shut down
            //if (fds_poll[i].revents & POLLIN == 0)
            //{
                //printf(" Error! revents = %d; expected POLLIN\n", fds_poll[i].revents);
                //shutdown = TRUE;
                //break;
            //}

            // if listener is readable, initialize new connection
            if (fds_poll[i].fd == listen_fd)
            {

                printf("Reading from listening socket fd %d . . . \n", listen_fd);

                // loop to accept all valid incoming connections
                do
                {

                    // disconnect if error is not EWOULDBLOCK (indicates no more incoming)
                    if ((connect_fd = accept(listen_fd, NULL, NULL)) < 0)
                    {
                        if (errno != EWOULDBLOCK)
                        {
                            perror(" accept() failed");
                        }
                        break;
                    }
                    
                    // add new sock to pollfd at next open slot, prepare for next poll
                    printf(" New incoming client - using fd %d\n", connect_fd);
                    fds_poll[fds_ct].fd = connect_fd;
                    fds_poll[fds_ct].events = POLLIN;
                    fds_poll[fds_ct].revents = 0;
                    userData[fds_ct].name[0] = '\0';
                    userData[fds_ct].lastRecvd = time(NULL);
                    userData[fds_ct].countRooms = 0;
                    userData[fds_ct].lastSent = time(NULL);
                    ++fds_ct;
                    ++userCount;
                } while (connect_fd != -1);
            }
            else
            {
                printf(" fd %d is readable\n", fds_poll[i].fd);

                // read in data on this socket
                // EWOULDBLOCK indicates end of successful rcv()
                //  other errors will close connection
                if ((check = recv(fds_poll[i].fd, buffer, sizeof(buffer), 0)) < 0)
                {
                    if (errno != EWOULDBLOCK)
                    {
                        perror(" recv() failed. closing connection.");
                        disconnect_client(fds_poll[i].fd, i);
                        --i;
                        --current_ct;
                        break;
                    }
                }

                int rcv_sock = fds_poll[i].fd;

                // check for disconnect from client
                if (check == 0)
                {
                    printf(" Connection closed by client %.20s\n", userData[i].name);
                    disconnect_client(rcv_sock, i);
                    --i;
                    --current_ct;
                    continue;
                }
                else
                {
                    printf("Processing packet\n");
                    char *to_process = malloc(check);
                    memcpy(to_process, buffer, check);
                    if(process_packet((irc_packet_generic_t *)to_process, rcv_sock, i) == -1)
                    {
                        --i;
                        printf("An error occurred while processing, user has been kicked\n");
                        --current_ct;
                    }
                    free(to_process);
                }
            }
        }
        // check for timeout

        // Loop over each client and check their timestamps.
        // If last received is older than 5*timeoutWindow, kick them
        // If last sent is old, send heartbeat

        // After looping over clients and sending heartbeats, the poll loop will continue to next iteration
        // printf(" poll() timeout. Server shutting down. Goodbye.\n"); //why no disconnect set here?
        int val = 0;
        for (int i = 1; i <= userCount; ++i)
        {
            val = keepalive(i);
            if (val == -1)
            {
                //heartbeat from this client is stale, kick them
                manage_error(fds_poll[i].fd, i, IRC_ERR_NO_HEARTBEAT);
                //disconnect_client(fds_poll[i].fd, i);
                --i;
            }
        }
    } while (shutdown == FALSE);

    // close open sockets
    for (int i = 0; i < fds_ct; ++i)
    {
        if (fds_poll[i].fd >= 0)
            close(fds_poll[i].fd);
    }

    exit(0);
}

int send_room_msg(irc_packet_send_msg_t *incoming, int sourceUser)
{
    int i = 0;
    // call validate string on the room name and message body
    int valid = validate_string(incoming->target_name, 20);
    if (validate_string(incoming->target_name, 20) != 1 || validate_string(incoming->msg, incoming->header.length - 20) != 1)
    {
        // bad string
        manage_error(fds_poll[sourceUser].fd, sourceUser, IRC_ERR_ILLEGAL_MESSAGE);
        return -1;
    }
    irc_packet_tell_msg_t *reply = malloc(sizeof(irc_packet_tell_msg_t) + incoming->header.length + 20);
    for (i = 0; i < roomCount; ++i)
    {
        if (strncmp(incoming->target_name, roomList[i].roomName, 20) == 0)
        {
            memset(reply, '\0', sizeof(irc_packet_tell_msg_t) + incoming->header.length + 20);
            reply->header.opcode = IRC_OPCODE_TELL_MSG;
            reply->header.length = incoming->header.length + 20;
            memcpy(reply->target_name, incoming->target_name, 20);
            memcpy(reply->msg, incoming->msg, incoming->header.length - 20);
            memcpy(reply->sending_user, userData[sourceUser].name, 20);
            for (int j = 0; j < roomList[i].countUsers; ++j)
            {
                if (sourceUser != roomList[i].userIDs[j])
                {
                    if (send(fds_poll[roomList[i].userIDs[j]].fd, reply, sizeof(irc_packet_tell_msg_t) + incoming->header.length + 20, 0) == -1)
                    {
                        perror("send");
                    }
                    userData[roomList[i].userIDs[j]].lastSent = time(NULL);
                }
            }
            free(reply);
            return 0;
        }
    }
    if (i == roomCount)
    { // target room does not exist
        manage_error(fds_poll[sourceUser].fd, sourceUser, IRC_ERR_UNKNOWN);
        free(reply);
        return -1;
    }
}


int process_packet(struct irc_packet_generic *incoming, int sock, int index)
{
    userData[index].lastRecvd = time(NULL);
    printf("Packet opcode: %u\n", incoming->header.opcode);
    switch (incoming->header.opcode)
    {
    case IRC_OPCODE_ERR:
        manage_incoming_error(incoming, sock, index);
        return -1;
        break;
    case IRC_OPCODE_HEARTBEAT:
        //heartbeat(incoming, index);
        //already updated lastRecvd for user
        break;
    case IRC_OPCODE_HELLO:
        return hello(incoming, sock, index);
        break;
    case IRC_OPCODE_LIST_ROOMS:
        room_list(incoming, sock, index);
        break;
    case IRC_OPCODE_JOIN_ROOM:
        join_room(incoming, sock, index);
        break;
    case IRC_OPCODE_LEAVE_ROOM:
        leave_room(incoming, sock, index);
        break;
    case IRC_OPCODE_SEND_MSG:
        int result = send_room_msg((irc_packet_send_msg_t *)incoming, index);
        if(result == -1)
        { // send failed due to illegal string
            //manage_error(sock, index, IRC_ERR_ILLEGAL_MESSAGE);
            return -1;
        }
        break;
    case IRC_OPCODE_SEND_PRIV_MSG:
        if(private_message(incoming, sock, index) == -1)
            return -1;
        break;
    default:
        manage_error(sock, index, IRC_ERR_ILLEGAL_OPCODE);
        return -1;
    }
    return 0;
}



void manage_incoming_error(struct irc_packet_generic *error_msg, int sock, int index)
{
    disconnect_client(sock, index);
    return;
}

int hello(struct irc_packet_generic *incoming, int sock, int index)
{
    struct irc_packet_hello *packet = (struct irc_packet_hello *)incoming;
    // validate username?
    if (validate_string(packet->username, 20) == 1)
    {
        return add_user(packet->username, sock, index);
    }
    printf("Bad string\n");
    manage_error(sock, index, IRC_ERR_ILLEGAL_NAME);
    return -1;
}

int add_user(char *username, int sock, int index)
{
    if (userCount == MAX_USERS)
    {
        fprintf(stderr, "server full. user could not be added.");
        manage_error(sock, index, IRC_ERR_TOO_MANY_USERS);
        return -1;
    }
    if (is_user(username) == 1)
    {
        fprintf(stderr, "name already in use.");
        manage_error(sock, index, IRC_ERR_NAME_EXISTS);
        return -1;
    }
    memcpy(userData[index].name, username, 20);
    userData[index].countRooms = 0;
    //++userCount;
    printf("user %s added successfully\n", username); // debug
    return 0;
}

void manage_error(int sock, int index, uint32_t error_no)
{
    struct irc_packet_error curr_error;
    curr_error.header.opcode = IRC_OPCODE_ERR;
    curr_error.header.length = 4;
    curr_error.error_code = error_no;
    printf("kicking user with error code %u\n", error_no);
    if (send(sock, &curr_error, sizeof(struct irc_packet_error), 0) <= 0)
    {
        perror("send");
        printf("Could not send error message");
    }
        
    disconnect_client(sock, index);
}

int is_user(char *username)
{
    // compare against current names
    for(int i = 1; i <= userCount; ++i)
    {
        if(strncmp(username, userData[i].name, 20) == 0)
        {
            return 1;
        }
    }
    return 0;
}

void disconnect_client(int sock, int index)
{
    // remove from rooms
    for (int i = 0; i < userData[index].countRooms; ++i)
    {
        remove_from_room(userData[index].roomIDs[i], index);
    }
    fds_poll[index] = fds_poll[fds_ct - 1];
    memcpy(&(userData[index]), &(userData[userCount]), sizeof(client_data_t));
    //after the fds_poll and userData arrays are rearranged, every room needs to u
    /*
    for(int i = 0; i < roomCount; ++i)
    {
        for(int j = 0; j < roomList[i].countUsers; ++j)
        {
            if(roomList[i].userIDs[j] == fds_ct - 1)
            {
                roomList[i].userIDs[j] = index;
            }
        }
    }
    */
    // update room data for moved users
    for (int i = 0; i < userData[index].countRooms; ++i)//intead of looping over all rooms, this loops over rooms that had the repositioned user in them
        update_user_data(&roomList[userData[index].roomIDs[i]], index);
    --userCount;
    --fds_ct;
    close(sock);
}

void update_user_data(room_data_t *to_update, int new_user_index)
{
    for (int i = 0; i < to_update->countUsers; ++i)
    {
        if (strncmp(userData[new_user_index].name, to_update->users[i], 20) == 0)
        {
            to_update->userIDs[i] = new_user_index;
        }
    }
}

void room_list(struct irc_packet_generic *incoming, int socket, int index)
{
    int new_length = sizeof(struct irc_packet_list_resp) + sizeof(char) * 20 * roomCount;
    struct irc_packet_list_resp *outgoing = malloc(new_length);
    outgoing->header.opcode = IRC_OPCODE_LIST_ROOMS_RESP;
    outgoing->header.length = 20 * roomCount + 20;
    if (roomCount != 0)
    {
        // maybe make this memcpy
        for (int i = 0; i < roomCount; ++i)
            memcpy(outgoing->item_names[i], roomList[i].roomName, 20);
    }

    if (send(fds_poll[index].fd, outgoing, new_length, 0) == -1)
        perror("send");
    else
        userData[index].lastSent = time(NULL);
    free(outgoing);
}

int private_message(struct irc_packet_generic *incoming, int socket, int index)
{
    struct irc_packet_send_msg *packet = (struct irc_packet_send_msg *) incoming;
    struct irc_packet_tell_msg * outgoing = malloc(sizeof(irc_packet_tell_msg_t) + (incoming->header.length - 20) * sizeof(char));
    int new_length = packet->header.length;
    int len = new_length - 20;
    int target_index;
    // check if intended target is a user
    if (len > MAXMSGLENGTH)
    {
        manage_error(socket, index, IRC_ERR_ILLEGAL_LENGTH);
        free(outgoing);
        return -1;
    }
    if (validate_string(packet->msg, len) != 1)
    {
        manage_error(socket, index, IRC_ERR_ILLEGAL_MESSAGE);
        free(outgoing);
        return -1;
    }
    if (validate_string(packet->target_name, 20) != 1)// || packet->target_name) == 1)
    {
        manage_error(socket, index, IRC_ERR_ILLEGAL_NAME);
        free(outgoing);
        return -1;
    }
    memcpy(outgoing->target_name, packet->target_name, 20);
    memset(outgoing->msg, '\0', len * sizeof(char));
    strncpy(outgoing->msg, packet->msg, len);
    outgoing->header.opcode = IRC_OPCODE_TELL_MSG;
    outgoing->header.length = new_length + 20;
    target_index = find_user(packet->target_name);
    if (target_index == -1)
    {
        manage_error(socket, index, IRC_ERR_ILLEGAL_NAME);
        free(outgoing);
        return -1;
    }
    memcpy(outgoing->sending_user, userData[index].name, 20);//strlen(userData[index].name));
    if (send(fds_poll[target_index].fd, outgoing, sizeof(irc_packet_tell_msg_t) + (incoming->header.length - 20) * sizeof(char), 0) == -1)
        perror("send");
    else
    {
        userData[target_index].lastSent = time(NULL);
    }
    free(outgoing);
    return 0;
}

int find_user(char *username)
{
    for (int i = 1; i <= userCount; ++i)
    {
        if (strncmp(username, userData[i].name, 20) == 0)
        {
            return i;
        }
    }
    return -1;
}

int join_room(struct irc_packet_generic *incoming, int sock, int index)
{
    struct irc_packet_join *packet = (struct irc_packet_join *) incoming;
    //if (roomCount != 0)
    //{
        for (int i = 0; i < roomCount; ++i)
        {
            if (strncmp(packet->room_name, roomList[i].roomName, 20) != 0)
                continue;
            else
            {
                return add_to_room(index, i);
            }
        }
        //the loop finishes if the room did not yet exist, so make a new room
        if (roomCount == MAXROOMS)
        {
            manage_error(sock, index, IRC_ERR_TOO_MANY_ROOMS);
            return -1;
        }
        return create_room(packet->room_name, index);
    //}
}

int add_to_room(int user_index, int room_index)
{
    // check if user already member
    for (int i = 0; i < userData[user_index].countRooms; ++i)
    {
        if (room_index == userData[user_index].roomIDs[i])
        {
            //If the user is already in the room, just ignore
            //manage_error(fds_poll[user_index].fd, user_index, IRC_ERR_UNKNOWN);
            return 0;
        }
    }
    // check for space in room
    if (roomList[room_index].countUsers == MAXUSERS)
    {
        manage_error(fds_poll[user_index].fd, user_index, IRC_ERR_TOO_MANY_USERS);
        return -1;
    }
    
    int inserted = 0;
    /*
    for(int i = 0; i < roomList[room_index].countUsers; ++i)
    {
        if(strncmp(userData[user_index].name, roomList[room_index].users[i], 20) < 0)
        {
            memmove(roomList[room_index].users[i + i], roomList[room_index].users[i], sizeof(room_data_t) * (roomList[room_index].countUsers - i));
            memmove(&(roomList[room_index].userIDs[i + i]), &(roomList[room_index].userIDs[i]), sizeof(int) * (roomList[room_index].countUsers - i));
            inserted = 1;
            ++roomList[room_index].countUsers;
            break;
        }
    }
    */
    // add username to roster
    for (int i = 0; i < roomList[room_index].countUsers; ++i)
    {
        if (strncmp(userData[user_index].name, roomList[room_index].users[i], 20) < 0)
        {
            memmove(roomList[room_index].users[i + 1], roomList[room_index].users[i], sizeof(char) * 20 * (roomList[room_index].countUsers - i));
            //memcpy(roomList[room_index].users[i + i], roomList[room_index].users[i], sizeof(char) * 20 * (roomList[room_index].countUsers - i - 1));
            memcpy(roomList[room_index].users[i], userData[user_index].name, 20);
            memmove(&(roomList[room_index].userIDs[i + 1]), &(roomList[room_index].userIDs[i]), sizeof(int) * (roomList[room_index].countUsers - i));
            roomList[room_index].userIDs[i] = user_index;
            inserted = 1;
            // increment user ct
            ++(roomList[room_index].countUsers);
            // send update to room members
            send_room_roster(&roomList[room_index]);
            // add room to user's data and increment user's room count
            userData[user_index].roomIDs[(userData[user_index].countRooms)++] = room_index;
            return 0;
        }
    }
    if(inserted == 0)
    {
        // add username to roster
        memcpy(roomList[room_index].users[roomList[room_index].countUsers], userData[user_index].name, 20);
        // add new user index and increment user ct
        roomList[room_index].userIDs[(roomList[room_index].countUsers)] = user_index;
        ++roomList[room_index].countUsers;
    }
    
    // send update to room members
    send_room_roster(&roomList[room_index]);
    // add room to user's data and increment user's room count
    userData[user_index].roomIDs[(userData[user_index].countRooms)++] = room_index;
    return 0;
}

int leave_room(struct irc_packet_generic *incoming, int sock, int index)
{
    irc_packet_leave_t *packet = (irc_packet_leave_t *) incoming;
    if (validate_string(packet->room_name, 20) != 1)
    {
        manage_error(sock, index, IRC_ERR_ILLEGAL_NAME);
        return -1;
    }
    for (int i = 0; i < roomCount; ++i)
    {
        if (strncmp(packet->room_name, roomList[i].roomName, 20) == 0)
        {
            remove_from_room(i, index);
            return 0;
        }
    }
    manage_error(sock, index, IRC_ERR_UNKNOWN);
    return -1;
}

void remove_from_room(int room_index, int user_index)
{
    // capture user count and decrement
    int user_ct = (roomList[room_index].countUsers)--;
    for (int i = 0; i < user_ct; ++i)
    {
        // when name found, remove from roster, send out update, update user's data
        if (strncmp(roomList[room_index].users[i], userData[user_index].name, 20) == 0)
        {
            memmove(roomList[room_index].users[i], roomList[room_index].users[i + 1], sizeof(char) * 20 * (user_ct - i - 1));
            memmove(&(roomList[room_index].userIDs[i]), &(roomList[room_index].userIDs[i + 1]), sizeof(int) * (user_ct - i - 1));
            send_room_roster(&roomList[room_index]);
            // remove room index from user's data, decrement user's room count
            for (int i = 0; i < userData[user_index].countRooms; ++i)
            {
                if (userData[user_index].roomIDs[i] == room_index)
                {
                    memmove(&(userData[user_index].roomIDs[i]), &(userData[user_index].roomIDs[i + 1]), sizeof(int) * userData[user_index].countRooms - i - 1);
                    --userData[user_index].countRooms;
                    break;
                }
            }
            break;
        }
    }
}

int create_room(char room_name[20], int user_index)
{
    memcpy(roomList[roomCount].roomName, room_name, 20);
    memcpy(roomList[roomCount].users[0], userData[user_index].name, 20);
    roomList[roomCount].userIDs[0] = user_index;
    roomList[roomCount].countUsers = 1;
    send_room_roster(&roomList[roomCount]);
    ++roomCount;
    printf("Creating a new room named: %.20s\n", room_name);
    return 0;
}

void send_room_roster(room_data_t *room)
{
    int new_length = sizeof(irc_packet_list_resp_t) + sizeof(char) * 20 * (room->countUsers + 1);
    irc_packet_list_resp_t *roster = malloc(new_length);
    roster->header.opcode = IRC_OPCODE_LIST_USERS_RESP;
    roster->header.length = sizeof(char) * 20 * (1 + room->countUsers);//new_length - 8;
    memcpy(roster->identifier, room->roomName, 20);
    memcpy(roster->item_names, room->users, sizeof(char) * 20 * room->countUsers);
    for (int i = 0; i < room->countUsers; ++i)
    {
        int curr_sock = room->userIDs[i];
        printf("Sending roster of room %.20s to user %.20s\n", room->roomName, userData[curr_sock].name);
        if (send(fds_poll[curr_sock].fd, roster, new_length, 0) == -1)
            perror("send");
        else
            userData[room->userIDs[i]].lastSent = time(NULL);
    }
    free(roster);
}

int keepalive(int userID)
{
    time_t now = time(NULL);
    int connected = 1;

    irc_packet_heartbeat_t pulse;
    pulse.header.opcode = IRC_OPCODE_HEARTBEAT;
    pulse.header.length = 0;
    now = time(NULL);
    printf("Checking heartbeat status of user %d named %.20s %lu\n", userID, userData[userID].name, now);

    if (now - userData[userID].lastRecvd >= 15)
    { // client is not sending heartbeats, assume connection lost
        printf("stale heartbeat from user %d named %.20s\n", userID, userData[userID].name);
        return -1;
    }
    if (now - userData[userID].lastSent >= 3)
    { // Outgoing heartbeat is stale
        // send heartbeat
        printf("sending heartbeat to user %d named %.20s\n", userID, userData[userID].name);
        if (send(fds_poll[userID].fd, &pulse, sizeof(irc_packet_heartbeat_t), 0) == -1)
            perror("send");
        else
            userData[userID].lastSent = time(NULL);
    }
    return 0;
}