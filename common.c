#include "relayChat.h"

int validate_string(char * text, int length)
{
    if(length <= 0) return -1;
    if(text == NULL) return -2;

    int nullencountered = 0;
    for(int i = 0; i < length; ++i)
    {
        if(text[i] == '\0')
        {
            nullencountered = 1;
        }
        else if(nullencountered)
        {//after the first \0 there is a non-null character
            return -3;
        }
        else if((text[i] < 0x20 || text[i] > 0x7E) && text[i] != '\n')
        {//non printable character in the string
            return -4;
        }
    }
    if(nullencountered == 0)
    {
        if(length != 20 && length != MAXMSGLENGTH)
        {
            return 0;
        }
    }
    return 1;
}

// store ip of socket connection ??????
// source: beej.us/guide/bgnet
void *get_ip_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int find_sock_struct(struct addrinfo *server_info, struct addrinfo *valid_sock) {
	int ret, socket_fd;
	struct addrinfo pre_info;


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
		if ((socket_fd = socket(valid_sock->ai_family, valid_sock->ai_socktype,
				valid_sock->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		// something about setsockopt so sockets can be reused??

		if (bind(socket_fd, valid_sock->ai_addr, valid_sock->ai_addrlen) == -1) {
			close(socket_fd);
			perror("server: bind");
			continue;
		}

		break;
	}

	return socket_fd;
}
