/* * * *
* "relayChatClient.c"
* RelayChat by Micah Lorenz and Tierney McBride
* * * * */

#include "relayChat.h" 
#include <string.h>

int main(int argc, char *argv[]) {

    char username[20];

    struct irc_packet_send_msg * test = malloc(2 * sizeof(struct irc_packet_header) + 20 * sizeof(char) + 27 * sizeof(char));

    strcpy(test->msg, "abcdefghijklmnopqrstuwxyz");

    printf("%s\n", test->msg);


    exit(0);
}
