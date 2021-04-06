// TODO(vincent): use port 80 ? I believe that's what web browsers will try to find.
// Apparently the OS might not let you do that.


// TODO(vincent): When spamming F5 (refresh) in the browser, the winsock API crashes.
// we get these errors:
// shutdown failed: 10054
// accept failed: 10004
// return exit code 6
//
// why?

// TODO(vincent): If you launch the server from a shell, 
// do you actually have to be in the same folder as the executable?


#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include "common.h"
#include "server.cpp"
#pragma comment(lib, "Ws2_32.lib")


struct platform_work_queue
{
    //u32 volatile CompletionGoal;
    //u32 volatile CompletionCount;
    u32 volatile NextEntryToWrite;
    u32 volatile NextEntryToRead;
    HANDLE SemaphoreHandle;
    platform_work_queue_entry Entries[256];
};

internal void
Win32AddEntry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data)
{
    u32 NewNextEntryToWrite = (Queue->NextEntryToWrite + 1) % ArrayCount(Queue->Entries);
    Assert(NewNextEntryToWrite != Queue->NextEntryToWrite);
    
    if (NewNextEntryToWrite != Queue->NextEntryToRead)
    {
        platform_work_queue_entry *Entry = Queue->Entries + Queue->NextEntryToWrite;
        Entry->Callback = Callback;
        Entry->Data = Data;
        //++Queue->CompletionGoal;
        //_WriteBarrier(); // TODO(vincent): hmmmm... why would you use that?
        Queue->NextEntryToWrite = NewNextEntryToWrite;
        // increase semaphore count so a thread can wake up
        ReleaseSemaphore(Queue->SemaphoreHandle, 1, 0); 
    }
    
}

internal b32
Win32DoNextWorkQueueEntry(platform_work_queue *Queue)
{
    // Many threads may be executing this function simultaneously.
    // Queue is subject to concurrent modifications so we have to be careful.
    // Entries in the work queue are laid out in a circular buffer.
    b32 WeShouldSleep = false;
    u32 OriginalNextEntryToRead = Queue->NextEntryToRead; // read this once and try to work with that
    u32 NewNextEntryToRead = (OriginalNextEntryToRead + 1) % ArrayCount(Queue->Entries);
    if (OriginalNextEntryToRead != Queue->NextEntryToWrite) // if read pointer hasn't caught up 
    {
        // Atomically increment the queue's NextEntryToRead iff somebody else hasn't already done it.
        u32 Index = InterlockedCompareExchange((LONG volatile *)&Queue->NextEntryToRead, 
                                               NewNextEntryToRead, OriginalNextEntryToRead);
        // Index is the old value of Queue->NextEntryToRead, 
        // so we are successfully the ones to have done the incrementation 
        // iff Index == OriginalNextEntryToRead, 
        // and in that case we are the thread that should do that entry's work.
        if (Index == OriginalNextEntryToRead)
        {
            platform_work_queue_entry Entry = Queue->Entries[Index];
            Entry.Callback(Queue, Entry.Data);
            //InterlockedIncrement((LONG volatile *) &Queue->Completioncount);
            // Other people may be retrieving other entries simultaneously so this should also be atomic.
        }
    }
    else
    {
        WeShouldSleep = true; // this thread found that there is no work left to do
    }
    
    return WeShouldSleep;
}

#if 0 
internal void
Win32CompleteAllWork(platform_work_queue *Queue)
{
    while (Queue->CompletionGoal != Queue->CompletionCount)
    {
        Win32DoNextWorkQueueEntry(Queue);
    }
    Queue->CompletionGoal = 0;
    Queue->CompletionCount = 0;
}
#endif 

DWORD WINAPI
ThreadProc(LPVOID lpParameter)
{
    platform_work_queue *Queue = (platform_work_queue *)lpParameter;
    for (;;)
    {
        if (Win32DoNextWorkQueueEntry(Queue))
        {
            WaitForSingleObjectEx(Queue->SemaphoreHandle, INFINITE, FALSE);
        }
    }
}

// TODO(vincent): Call this. 4 threads ? Can the main thread do some queue work itself ? Should it ?
internal void
Win32MakeQueue(platform_work_queue *Queue, u32 ThreadCount)
{
    //Queue->CompletionGoal = 0;
    //Queue->CompletionCount = 0;
    Queue->NextEntryToWrite = 0;
    Queue->NextEntryToRead = 0;
    u32 InitialCount = 0;
    Queue->SemaphoreHandle = CreateSemaphoreEx(0, InitialCount, ThreadCount, 0, 0, SEMAPHORE_ALL_ACCESS);
    for (u32 ThreadIndex = 0; ThreadIndex < ThreadCount; ThreadIndex++)
    {
        DWORD ThreadID;
        HANDLE ThreadHandle = CreateThread(0, 0, ThreadProc, Queue, 0, &ThreadID);
        CloseHandle(ThreadHandle);
    }
}


internal b32
HandleReceiveError(int BytesReceived, SOCKET ClientSocket)
{
    b32 Success = true;
    if (BytesReceived < 0)
    {
        printf("recv failed: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        //WSACleanup();
        Success = false;
    }
    return Success;
}

internal b32
HandleSendError(int BytesSent, SOCKET ClientSocket)
{
    b32 Success = true;
    if (BytesSent == SOCKET_ERROR)
    {
        printf("send failed: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        //WSACleanup();
        Success = false;
    }
    return Success;
}

internal void 
ShutdownConnection(SOCKET ClientSocket)
{
    // shutdown the send half of the connection since no more data will be sent
    int ShutdownResult = shutdown(ClientSocket, SD_SEND);
    
    if (ShutdownResult == SOCKET_ERROR) 
    {
        //printf("shutdown failed: %d\n", WSAGetLastError());
        
        // NOTE(vincent): Here is a real scenario where we could branch here:
        // if you spam F5 (refresh) in your navigator, the client may forcibly close the connection early
        // by themself, in which case shutdown() will return error 10054.
        // The server should keep running.
        
        //WSACleanup();
    }
    closesocket(ClientSocket);
}

int main() 
{
    // NOTE(vincent): Initialize threads and work queue
    platform_work_queue Queue = {};
    Win32MakeQueue(&Queue, NUMBER_OF_THREADS - 1);
    
    // NOTE(vincent): Initializing server memory
    server_memory ServerMemory = {};
    LPVOID BaseAddress = 0;//(LPVOID) Terabytes(2);
    ServerMemory.StorageSize = SERVER_STORAGE_SIZE; 
    ServerMemory.Storage = VirtualAlloc(BaseAddress, ServerMemory.StorageSize,
                                        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    initialize_server_memory_result InitResult =
        InitializeServerMemory(&ServerMemory, &Queue, Win32AddEntry, Win32DoNextWorkQueueEntry);
    
    
    if (InitResult.ParsingErrorCount == 0)
    {
        // NOTE(vincent): The rest of this is basically following the instructions on MSDN 
        // to set up a TCP server:
        // https://docs.microsoft.com/en-us/windows/win32/winsock/winsock-server-applicationup
        
        // NOTE(vincent): initialize the Windows Sockets DLL
        WSADATA WSAData;
        int Result = WSAStartup(MAKEWORD(2,2), &WSAData);
        if (Result != 0)
        {
            printf("WSAStartup failed: %d\n", Result);
            return 1;
        }
        
        struct addrinfo *AddressInfo = 0; 
        struct addrinfo Hints;
        ZeroBytes((char *)&Hints, sizeof(Hints));
        Hints.ai_family = AF_UNSPEC;           // AF_INET IPv4, AF_INET6 Ipv6, AF_UNSPEC agnostic
        Hints.ai_socktype = SOCK_STREAM;
        Hints.ai_protocol = IPPROTO_TCP;
        Hints.ai_flags = AI_PASSIVE;
        
        // Resolve the local address and port to be used by the server
        int GetaddrinfoResult = getaddrinfo(0, InitResult.PortString, &Hints, &AddressInfo);
        if (GetaddrinfoResult != 0) 
        {
            printf("getaddrinfo failed: %d\n", GetaddrinfoResult);
            //WSACleanup();
            return 2;
        }
        
        SOCKET ListenSocket = INVALID_SOCKET;
        
        ListenSocket = socket(AddressInfo->ai_family, AddressInfo->ai_socktype, AddressInfo->ai_protocol);
        
        if (ListenSocket == INVALID_SOCKET)
        {
            printf("Error at socket(): %ld\n", WSAGetLastError());
            freeaddrinfo(AddressInfo);
            //WSACleanup();
            return 3;
        }
        
        
        // Setup the TCP listening socket
        int BindResult = bind(ListenSocket, AddressInfo->ai_addr, (int)AddressInfo->ai_addrlen);
        if (BindResult == SOCKET_ERROR) 
        {
            printf("bind failed with error: %d\n", WSAGetLastError());
            freeaddrinfo(AddressInfo);
            closesocket(ListenSocket);
            //WSACleanup();
            return 4;
        }
        
        freeaddrinfo(AddressInfo);
        
        if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) 
        {
            printf( "Listen failed with error: %ld\n", WSAGetLastError());
            closesocket(ListenSocket);
            //WSACleanup();
            return 5;
        }
        
        struct sockaddr_storage TheirAddress; // connector's address information
        int SizeTheirAddress = sizeof(TheirAddress);
        printf("\nServer: waiting for a connection on port %s\n", InitResult.PortString);
        u32 RequestsCount = 0;
        for (;;)
        {
            // Accept a client socket
            SOCKET ClientSocket = INVALID_SOCKET;
            ClientSocket = accept(ListenSocket, (struct sockaddr *)&TheirAddress, &SizeTheirAddress);
            
            if (ClientSocket == INVALID_SOCKET) 
            {
                printf("accept failed: %d\n", WSAGetLastError());
                closesocket(ListenSocket);
                //WSACleanup();
                return 6;
            }
            else
                PrepareHandshaking(&ServerMemory, (struct sockaddr *)&TheirAddress, ClientSocket, &Queue);
            
            RequestsCount++;
            //printf("%u requests\n", RequestsCount); 
        }
        
        //WSACleanup(); 
        // NOTE(vincent): I think we don't need to ever call WSACleanup() anywhere.
        // when we call WSACleanup(), the server can't really run anymore, so you might as well
        // just close the program, and any modern OS should free the memory when the process disappears.
    }
    return 0;
}

