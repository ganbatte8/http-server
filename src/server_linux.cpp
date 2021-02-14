#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "common.h"

#define BACKLOG 10   // how many pending connections queue will hold

#define INVALID_SOCKET -1  // this helps for platform-independent code compatibility with Windows
typedef int SOCKET // same

internal void
sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}


int main(void)
{
    int Socket, NewSocket;  // listen on Socket, new connection on NewSocket
    struct addrinfo Hints, *ServerInfo, *P;
    struct sockaddr_storage TheirAddress; // connector's address information
    socklen_t SinSize;
    struct sigaction SignalAction;
    int One = 1;
    char AddressString[INET6_ADDRSTRLEN];
    int AddressInfoResult;
    char ReceiveBuffer[2048];
    char ReceiveBufferHex[3*ArrayCount(ReceiveBuffer)+1];
    memset(&Hints, 0, sizeof Hints);
    Hints.ai_family = AF_UNSPEC;
    Hints.ai_socktype = SOCK_STREAM;
    Hints.ai_flags = AI_PASSIVE; // use my IP
    
    if ((AddressInfoResult = getaddrinfo(NULL, SERVER_PORT, &Hints, &ServerInfo)) != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(AddressInfoResult));
        return 1;
    }
    
    // loop through all the results and bind to the first we can
    for(P = ServerInfo; P != NULL; P = P->ai_next) {
        if ((Socket = socket(P->ai_family, P->ai_socktype,
                             P->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        
        if (setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR, &One, sizeof(int)) == -1) 
        {
            perror("setsockopt");
            exit(1);
        }
        
        if (bind(Socket, P->ai_addr, P->ai_addrlen) == -1) 
        {
            close(Socket);
            perror("server: bind");
            continue;
        }
        
        break;
    }
    
    freeaddrinfo(ServerInfo); // all done with this structure
    
    if (P == 0)  
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
    
    if (listen(Socket, BACKLOG) == -1) 
    {
        perror("listen");
        exit(1);
    }
    
    SignalAction.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&SignalAction.sa_mask);
    SignalAction.sa_flags = SA_RESTART;
    
    if (sigaction(SIGCHLD, &SignalAction, NULL) == -1) 
    {
        perror("sigaction");
        exit(1);
    }
    
    char SendBuffer[512];
    WriteStringLiteral(SendBuffer, "HTTP/1.0 200 OK\r\n\r\nHello");
    int SendBufferLength = StringLength(SendBuffer);
    
    while(1) 
    {  
        // main accept() loop
        printf("server: waiting for a connection on port %s\n", SERVER_PORT);
        
        SinSize = sizeof TheirAddress;
        NewSocket = accept(Socket, (struct sockaddr *)&TheirAddress, &SinSize);
        if (Socket == -1) 
        {
            perror("accept");
            continue;
        }
        
        int BytesReceived = HandleConnection((struct sockaddr *)&TheirAddress,
                                             ReceiveBuffer, ArrayCount(ReceiveBuffer),
                                             ReceiveBufferHex, ArrayCount(ReceiveBufferHex));
        
        if (BytesReceived < 0)
        {
            perror("recv");
            exit(1);
        }
        
        int BytesSent;
        if ((BytesSent = send(NewSocket, SendBuffer, SendBufferLength, 0)) == -1)
            perror("send");
        printf("BytesSent: %d\n", BytesSent);
        /*
        if (!fork()) 
        { 
            // this is the child process
            close(Socket); // child doesn't need the listener
            close(NewSocket);
            exit(0);
        }*/
        close(NewSocket);  // parent doesn't need this
    }
    
    return 0;
}
