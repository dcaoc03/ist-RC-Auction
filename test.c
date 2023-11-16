#include <stdio.h>
#include <string.h>

int main() {
    char* hello = "Hello world";

    printf("%ld %ld\n", sizeof(hello), strlen(hello));
}