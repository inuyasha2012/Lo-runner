#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int a, b;
    sleep(2);
    scanf("%d %d", &a, &b);
    printf("%d\n", a + b);
    // open("asdf", O_CREAT);
    /*scanf("%d", &a);*/
    
    return 0;
}
