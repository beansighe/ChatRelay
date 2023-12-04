
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

struct room_data;//if doing a linked list need circular declaration
typedef struct room_data
{
    char roomName[20];
    char users[MAXUSERS][20];//the names of users in the room, sorted by name
    int userIDs[MAXUSERS];//the index where the user lives in the userData and file descriptor parallel arrays
    int countUsers;
    // room_data * next; //if a linked list
}room_data_t;

typedef struct client_data
{
    char name[20];
    int roomIDs[MAXROOMS];//if room data is stored in an array, else use an array of pointers
    int countRooms;
    time_t lastSent;
    time_t lastRecvd;

}client_data_t;

int userCount = 0;
client_data_t userData[MAXUSERS + 1];//to keep it parallel with the fds_poll array

void *manage_client(void *data);

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
   struct pollfd fds_poll[MAX_USERS];
   int fds_ct = 1, current_ct = 0, i, j;
   int timeout = 2500;//interrupt polling to check heartbeat
   //POLL_MINS * 60 * 1000; // mins to millisecs

   // incoming data mgmt
   char buffer[BUFSIZ];


    //perror("test");
    //printf("other test\n");

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

    do {
        // call poll
        printf("Polling sockets. . . \n");
        if ((check = poll(fds_poll, fds_ct, 100 * timeout)) < 0) {
            perror(" poll() failed");
            break;
        }

        // check for timeout
        if (check == 0) {
            //Instead of shutting down, this is where we should handle heartbeat logic for the clients
            //Loop over each client and check there timestamps.
            //If last received is older than 5*timeoutWindow, kick them
            //If last sent is old, send heartbeat

            //After looping over clients and sending heartbeats, the poll loop will continue to next iteration 
            //printf(" poll() timeout. Server shutting down. Goodbye.\n"); //why no disconnect set here?
            continue;
            //break;
        }

        // Identify readable sockets
        current_ct = fds_ct;
        for (i = 0; i < current_ct; i++) {

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

                printf("Reading from listening socket . . . \n");

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
                //A packet has been received from a client
                printf(" fd %d is readable\n", fds_poll[i].fd);
                close_connection = FALSE;

                //read in data on this socket
                //EWOULDBLOCK indicates end of successful rcv()
                // other errors will close connection
                if ((check = recv(fds_poll[i].fd, buffer, sizeof(buffer), 0)) < 0) {
                    if (errno != EWOULDBLOCK) {
                        perror(" recv() failed. closing connection.");
                        close_connection = TRUE;
                        break;
                    }
                }

                // check for disconnect from client
                if (check == 0) {
                    printf(" Connection closed by client\n");
                    //remove the client from all data structures
                    //remove the client from rooms they were in and update rmeaining room members accordingly
                    
                    //remove client from fds_poll

                    //close the socket with the client

                    close_connection = TRUE;
                    break;
                }

                int sender_fd = fds_poll[i].fd;


                // send data
                for (int j = 0; j < fds_ct; j++) {
                    int dest_fd = fds_poll[j].fd;

                    // send to all but server and sender
                    if (dest_fd != listen_fd && dest_fd != sender_fd) {
                        if(send(dest_fd, buffer, check, 0) == -1) {
                            perror("send");
                        }
                    }

                }

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
   for (i = 0; i < fds_ct; ++i) {
    if(fds_poll[i].fd >=0)
        close(fds_poll[i].fd);
   }

    exit(0);
}

/*
storeNewUser(char un, uint_? ip_addr) {
    //add un and ip_addr to user list;
}

acceptLogin(char un, uint_? ip_addr) {
    //isThisUserOnline() ?, if not call storeNewUser()
    //else ????
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


void *manage_client(void *data) {
    int socket_fd = (int) data;

    if (send(socket_fd, "Hello, world!", 13, 0) == -1)
        perror("send");
    close(socket_fd);
    pthread_detach(pthread_self());
    pthread_exit(NULL);
}
/* * * *
* "relayChatServer.c"
* RelayChat by Micah Lorenz and Tierney McBride
* * * * 

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
*/