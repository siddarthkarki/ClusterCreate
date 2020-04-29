#include "ClusterCreate.h"
#include <unistd.h>

int main(){
    sleep(2);
    char addr[25] = "127.0.0.1";
    runclient(addr ,8080);
    return 0;
}
