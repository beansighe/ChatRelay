#include "relayChat.h"

int validate_string(char * text, int length)
{
    if(length <= 0) return -1;
    if(NULL == text) return -2;

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

        }
    }
    return 1;
}