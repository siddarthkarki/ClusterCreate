#include "ClusterCreate.h"
#include <unistd.h>

int main(){
    // this sleep is not necessarily required, but helps in debugging on the server side
    sleep(2);
    char addr[25] = "127.0.0.1";
    runclient(addr ,8080);
    return 0;
}
