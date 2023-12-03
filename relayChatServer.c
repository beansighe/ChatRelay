/* * * *
* "relayChatServer.c"
* RelayChat by Micah Lorenz and Tierney McBride
* sources used in designing socket management: 
* <beej.us/guide/bgnet>,
* <https://www.ibm.com/docs/en/i/7.1?topic=designs-using-poll-instead-select>
* * * * */

#include "relayChat.h" 

#define BACKLOG 10
#define POLL_MINS 5

//user records
struct user *users[MAX_USERS];
int user_ct = 0;

//sockets to poll
struct pollfd *fds_poll;
int fds_ct, current_ct = 0;

void process_packet(struct irc_packet_generic *incoming, int sock, int index);

int main(void) {
    //create db


	int ret, listen_fd, connect_fd; //catch on ret, listen on listen, new connect on connect
    int check; //for error checks
    int yes = 1; // for use in setsockopt()
    struct addrinfo *server_info, *valid_sock;
	struct addrinfo pre_info;
    struct sockaddr_storage connecter_addr;
    socklen_t sin_size; //for identifying whether incoming connection is ipv4 or ipv6 
    char s[INET6_ADDRSTRLEN];
   // struct sigaction sig_action;


    //indicators
   int disconnect = FALSE;
   int close_connection;

   //thread stuff
   pthread_t thread;
   int thread_ret = 1;

   //poll stuff
   fds_poll = malloc(sizeof *fds_poll * MAX_USERS); //could change to be variable size for storage efficiency
   int timeout = POLL_MINS * 60 * 1000; // mins to millisecs

   // incoming data mgmt
   char buffer[BUFSIZ];



    //find and bind socket
	memset(&pre_info, 0, sizeof pre_info);
	pre_info.ai_family = AF_UNSPEC;
	pre_info.ai_socktype = SOCK_STREAM;
	pre_info.ai_flags = AI_PASSIVE;

	// error check
	if ((ret = getaddrinfo(NULL, PORT, &pre_info, &server_info)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		return 1;						//how to communicate error back
	}

	for (valid_sock = server_info; valid_sock != NULL; valid_sock = valid_sock->ai_next) {
		if ((listen_fd = socket(valid_sock->ai_family, valid_sock->ai_socktype,
				valid_sock->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		// allow reuse of socket descriptor
        if (check = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
            perror("setsockopt() failed");
            close(listen_fd);
            exit(-1);
        }

        // make non-blocking: inherited socks will share this attribute
        if ((check = ioctl(listen_fd, FIONBIO, &yes)) < 0) {
            perror("ioctl() failed");
            close(listen_fd);
            exit(-1);
        }

    
        // bind socket
		if (bind(listen_fd, valid_sock->ai_addr, valid_sock->ai_addrlen) == -1) {
			close(listen_fd);
			perror("server: bind");
			continue;
		}

		break;
	}

    // free result holder 
    freeaddrinfo(server_info);

    // prevent seg fault if uncaught error above
    if (valid_sock == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    // listen
    if (listen(listen_fd, BACKLOG) == -1) {
        perror("listen");
        close(listen_fd);
        exit(1);
    }
    
    printf("server: waiting for connections. . .\n");
    
    // set up for poll()
    // initialize pollfd storage
    memset(fds_poll, 0, sizeof(fds_poll));

    //add listener
    fds_poll[0].fd = listen_fd;
    fds_poll[0].events = POLLIN;

    fds_ct = 1;

    do {
        // call poll
        printf("Polling sockets. . . \n");
        if ((check = poll(fds_poll, fds_ct, timeout)) < 0) {
            perror(" poll() failed");
            break;
        }

        // check for timeout
        if (check == 0) {
            printf(" poll() timeout. Server shutting down. Goodbye.\n"); //why no disconnect set here?
            break;
        }

        // Identify readable sockets
        current_ct = fds_ct;
        for (int i = 0; i < current_ct; i++) {

            //no event
            if (fds_poll[i].revents == 0)
                continue;
            
            // nonzero and not equal to POLLIN is unexpected, shut down
            if (fds_poll[i].revents != POLLIN) {
                printf(" Error! revents = %d; expected POLLIN\n", fds_poll[i].revents);
                disconnect = TRUE;
                break;
            }

            // if listener is readable, initialize new connection
            if (fds_poll[i].fd == listen_fd) {

                printf("Reading from listening socket fd %d . . . \n", listen_fd);

                //loop to accept all valid incoming connections
                do {

                    // disconnect if error is not EWOULDBLOCK (indicates no more incoming)
                    if ((connect_fd = accept(listen_fd, NULL, NULL)) < 0) {
                        if (errno != EWOULDBLOCK) {
                            perror(" accept() failed");
                            disconnect = TRUE;
                        }
                        break;
                    }

                    // add new sock to pollfd at next open slot, prepare for next poll
                    printf(" New incoming client - using fd %d\n", connect_fd);
                    fds_poll[fds_ct].fd = connect_fd;
                    fds_poll[fds_ct].events = POLLIN;
                    ++fds_ct;

                    
                } while (connect_fd != -1);


            
            }
            else {
                printf(" fd %d is readable\n", fds_poll[i].fd);
                close_connection = FALSE;

                //read in data on this socket
                //EWOULDBLOCK indicates end of successful rcv()
                // other errors will close connection
                if ((check = recv(fds_poll[i].fd, buffer, sizeof(buffer), 0)) < 0) {
                    if (errno != EWOULDBLOCK) {
                        perror(" recv() failed. closing connection.");
                        disconnect_client(fds_poll[i].fd, i);
                        close_connection = TRUE;
                        break;
                    }
                }

                int rcv_sock = fds_poll[i].fd;


                // check for disconnect from client
                if (check == 0) {
                    printf(" Connection closed by client\n");
                    disconnect_client(rcv_sock, i);
                    close_connection = TRUE;
                    disconnect = TRUE;
                    break;
                }

                char *to_process = malloc(check);
                strncpy(to_process, buffer, check);
                process_packet(to_process, rcv_sock, i);


                // send data
                /*for (int j = 0; j < fds_ct; j++) {
                    int dest_fd = fds_poll[j].fd;

                    // send to all but server and sender
                    if (dest_fd != listen_fd && dest_fd != sender_fd) {
                        if(send(dest_fd, buffer, check, 0) == -1) {
                            //perror("send");
                            fprintf(stderr, "send error, socket fd %d", dest_fd);
                        }
                    }

                }
                    */

            }
        }


    } while (disconnect == FALSE);

/*
    //accept() loop
    while(1) {
        sin_size = sizeof connecter_addr;
        connect_fd = accept(listen_fd, (struct sockaddr *)&connecter_addr, &sin_size);
        if (connect_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(connecter_addr.ss_family,
            get_ip_addr((struct sockaddr *)&connecter_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        thread_ret = pthread_create(&thread, NULL, manage_client, (void*)connect_fd);
    }
    */

   //close open sockets
   for (int i = 0; i < fds_ct; ++i) {
    if(fds_poll[i].fd >=0)
        close(fds_poll[i].fd);
   }

    exit(0);
}

/*
storeNewUser(char un, uint_? ip_addr) {
    //check if name taken
    //if not:
        //add un and ip_addr to user list;
}

isThisUserOnline(un) {
    //is un in db?
}

join_room(char un, char room_name) {
    // if !(room_name)
        // create
    //find user_data un
    //update_roster(*room, user_data)
    //send_updated_roster(*room)
}

void send_updated_roster()

void unpack_packet() {
    //if . . . sort by type of packet to requisite series of actions
}


OBJECTS???
typedef struct user_data {
    char username;
    uint_? ip_addr;
}

typedef struct chat_room {
    char room_name[name_size];
    struct user_data *members[room_size];
    int curr_size;
}

ROOM MANAGEMENT FUNCTIONS
int update_roster (struct chat_room *this) {
        // ??
        //update and return this->curr_size?
    };

void send_updated_roster (struct chat_room *this) {
        //for all users in this->members:
            //send new roster to ip_addr
    };

void send_room_msg (struct chat_room *this, struct irc_packet_tell_msg *to_send)
        //for all users in this->members:
            // send to_send to ip_addr
    };


*/
//REALLY WORKING ON IT NOW
void process_packet(struct irc_packet_generic *incoming, int sock, int index) {

    switch(incoming->header.opcode) {
        case IRC_OPCODE_ERR:
            manage_incoming_error(incoming, sock, index);
            break;
        case IRC_OPCODE_HEARTBEAT:
            heartbeat(incoming, sock, index);
            break;
        case IRC_OPCODE_HELLO:
            hello(incoming, sock, index);
            break;

       /*case IRC_OPCODE_LIST_ROOMS:
            break;
        case IRC_OPCODE_JOIN_ROOM:
            break;
        case IRC_OPCODE_LEAVE_ROOM:
            break;
        case IRC_OPCODE_SEND_MSG:
            break;
            */
        case IRC_OPCODE_SEND_PRIV_MSG:
            private_message(incoming, sock, index);
            break;
        /*default:

            some kind of error management
            */
    }

}

void manage_incoming_error(struct irc_packet_generic *error_msg, int sock, int index) {
    return;
}

void hello(struct irc_packet_generic *incoming, int sock, int index) {
    struct irc_packet_hello *packet = incoming;
    //validate username?
    add_user(&packet->username, sock, index);
}

void add_user(char* username, int sock, int index) {
    if (user_ct == MAX_USERS) {
        fprintf(stderr, "server full. user could not be added.");
        manage_error(sock, index, IRC_ERR_TOO_MANY_USERS);
    }
    if (is_user(username) == 1) {
            frpintf(stderr, "name already in use.");
            manage_error(sock, index, IRC_ERR_NAME_EXISTS);
            return;
    }
    users[user_ct] = malloc(sizeof(struct user));
    if (users[user_ct] != NULL) {
        strncpy(users[user_ct]->username, username, strlen(username));
        users[user_ct]->sock = sock; 
        users[user_ct]->index = index;
        for (int i = 0; i < 10; ++i)
            users[user_ct]->rooms[i] = NULL;
        ++user_ct;
    }
    //deal with consequences of malloc fail
}

void manage_error(int sock, int index, uint32_t error_no) {
    struct irc_packet_error curr_error;
    curr_error.header.opcode = IRC_OPCODE_ERR;
    curr_error.header.length = 4;
    curr_error.error_code = error_no;
    if(send(sock, &curr_error, sizeof(struct irc_packet_error), 0) == -1)
        perror("send");
    disconnect_client(sock, index);
}

int is_user(char *username) {
    //compare against current names
    return 0;
}


void disconnect_client(int sock, int index) {
    fds_poll[index] = fds_poll[fds_ct - 1];
    if (users[index - 1] != NULL)
        users[fds_ct - 2]->index = index;
    close(sock);
    --user_ct;
    --fds_ct;
}

void private_message(struct irc_packet_generic *incoming, int socket, int index) {
    struct irc_packet_send_msg *packet = incoming;
    struct irc_packet_tell_msg outgoing;
    int new_length = packet->header.length;
    int len = new_length - 20;
    strncpy(&outgoing.target_name, packet->target_name, strlen(packet->target_name));
    strncpy(&outgoing.msg, packet->msg, len);
    outgoing.header.opcode = IRC_OPCODE_TELL_MSG;
    outgoing.header.length = new_length;
    strncpy(&outgoing.sending_user, users[index -1]->username, strlen(users[index -1]->username));
    if(send(users[index-1]->sock, &outgoing, new_length + 8, 0) == -1)
        perror("send");

}
