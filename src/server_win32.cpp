// TODO(vincent): exploiter une requête GET en HTTP/1.x (en ignorant les entêtes non gérées)
// TODO(vincent): use port 80 ? I believe that's what web browsers will try to find.
// Apparently the OS might not let you do that.

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include "common.h"
#include "server.cpp"
#pragma comment(lib, "Ws2_32.lib")


int main() 
{
    // NOTE(vincent): This is basically following the instructions on MSDN to set up a TCP server:
    // https://docs.microsoft.com/en-us/windows/win32/winsock/winsock-server-applicationup
    
    //void *ServerMemory = malloc(Megabytes(64));
    //InitializeServerMemory();
    parsed_config_file_result ParsedConfig = {};
    sprintf(ParsedConfig.PortString, DEFAULT_SERVER_PORT);
    ParseConfigFile(&ParsedConfig);
    
    
    
    // NOTE(vincent): initialize the Windows Sockets DLL
    WSADATA WSAData;
    int Result = WSAStartup(MAKEWORD(2,2), &WSAData);
    if (Result != 0)
    {
        printf("WSAStartup failed: %d\n", Result);
        return 1;
    }
    
    struct addrinfo *AddressInfo = 0; 
    //struct addrinfo *Pointer = 0;        // TODO(vincent): when do we actually use this ??? 
    struct addrinfo Hints;
    ZeroBytes((char *)&Hints, sizeof(Hints));
    Hints.ai_family = AF_INET;           // IPv4. TODO(vincent): handle IPv6 as well
    Hints.ai_socktype = SOCK_STREAM;
    Hints.ai_protocol = IPPROTO_TCP;
    Hints.ai_flags = AI_PASSIVE;
    
    // Resolve the local address and port to be used by the server
    int GetaddrinfoResult = getaddrinfo(0, ParsedConfig.PortString, &Hints, &AddressInfo);
    if (GetaddrinfoResult != 0) 
    {
        printf("getaddrinfo failed: %d\n", GetaddrinfoResult);
        WSACleanup();
        return 1;
    }
    
    SOCKET ListenSocket = INVALID_SOCKET;
    
    ListenSocket = socket(AddressInfo->ai_family, AddressInfo->ai_socktype, AddressInfo->ai_protocol);
    
    if (ListenSocket == INVALID_SOCKET)
    {
        printf("Error at socket(): %ld\n", WSAGetLastError());
        freeaddrinfo(AddressInfo);
        WSACleanup();
        return 1;
    }
    
    
    // Setup the TCP listening socket
    int BindResult = bind(ListenSocket, AddressInfo->ai_addr, (int)AddressInfo->ai_addrlen);
    if (BindResult == SOCKET_ERROR) 
    {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(AddressInfo);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    
    freeaddrinfo(AddressInfo);
    
    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) 
    {
        printf( "Listen failed with error: %ld\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    
    
#define DEFAULT_BUFFER_LENGTH 2048
    char ReceiveBuffer[DEFAULT_BUFFER_LENGTH];
    char ReceiveBufferHex[3*DEFAULT_BUFFER_LENGTH+1];
    
    char SendBuffer[512];
    WriteStringLiteral(SendBuffer, "HTTP/1.0 200 OK\r\n\r\nHello");
    int SendBufferLength = StringLength(SendBuffer);
    
    // Receive until the peer shuts down the connection
    b32 ServerIsRunning = true;
    struct sockaddr_storage TheirAddress; // connector's address information
    int SizeTheirAddress = sizeof(TheirAddress);
    printf("Server: waiting for a connection on port %s\n", ParsedConfig.PortString);
    while (ServerIsRunning)
    {
        // main accept() loop
        
        // Accept a client socket
        
        SOCKET ClientSocket = INVALID_SOCKET;
        ClientSocket = accept(ListenSocket, (struct sockaddr *)&TheirAddress, &SizeTheirAddress);
        
        if (ClientSocket == INVALID_SOCKET) 
        {
            printf("accept failed: %d\n", WSAGetLastError());
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }
        
        int BytesReceived = HandleConnection((struct sockaddr *)&TheirAddress, ClientSocket,
                                             ReceiveBuffer, ArrayCount(ReceiveBuffer),
                                             ReceiveBufferHex);
        
        if (BytesReceived < 0)
        {
            printf("recv failed: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
            return 1;
        }
        
        int BytesSent;
        // TODO(vincent): should send() be inside of a loop as well?
        if ((BytesSent = send(ClientSocket, SendBuffer, SendBufferLength, 0)) == SOCKET_ERROR)
        {
            printf("send failed: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
            return 1;
        }
        
#if 0
        printf("BytesSent: %d\n", BytesSent);
#endif
        // shutdown the send half of the connection since no more data will be sent
        int ShutdownResult = shutdown(ClientSocket, SD_SEND);
        if (ShutdownResult == SOCKET_ERROR) 
        {
            printf("shutdown failed: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
            return 1;
        }
        
        // cleanup
        closesocket(ClientSocket);
    }
    
    
    // NOTE(vincent): the server loop is not really supposed to end, so we are probably
    // not gonna run code here. But it's good form to try to clean things up anyway,
    // just to raise awareness of the resources that are being allocated.
    WSACleanup();
    
    return 0;
}

