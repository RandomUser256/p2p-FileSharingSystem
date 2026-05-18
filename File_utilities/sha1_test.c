#include "sha1.h"
//#include "CUnit/Basic.h"
#include "stdio.h"
#include <string.h>
#include <stdio.h>

#define SUCCESS 0

//gcc sha1_test.c sha1.c -o sha1_test -lm

int main( void ) {
    char const string[] = "10.11.20.37:8080";
    char result[64];
    char hexresult[41];
    size_t offset;

    SHA1(result, string, strlen(string));

    for( offset = 0; offset < 20; offset++) {
        sprintf( ( hexresult + (2*offset)), "%02x", result[offset]&0xff);
    }

    printf("%s", result);
}