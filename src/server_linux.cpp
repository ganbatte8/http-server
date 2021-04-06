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
#include <pthread.h>  // NOTE(vincent):  Compile and link with -pthread. semaphore.h also needs it.
#include <semaphore.h>
#include <sys/mman.h>
#include "common.h"
#define BACKLOG 10         // how many pending connections the queue will hold

#define INVALID_SOCKET -1  // this helps for platform-independent code compatibility with Windows
typedef int SOCKET;        // same
#include "server.cpp"


struct platform_work_queue
{
    u32 volatile NextEntryToWrite;
    u32 volatile NextEntryToRead;
    sem_t SemaphoreHandle;
    platform_work_queue_entry Entries[256];
};

internal void
LinuxAddEntry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data)
{
    u32 NewNextEntryToWrite = (Queue->NextEntryToWrite + 1) % ArrayCount(Queue->Entries);
    Assert(NewNextEntryToWrite != Queue->NextEntryToWrite);
    
    if (NewNextEntryToWrite != Queue->NextEntryToRead)
    {
        platform_work_queue_entry *Entry = Queue->Entries + Queue->NextEntryToWrite;
        Entry->Callback = Callback;
        Entry->Data = Data;
        // TODO(vincent): compiler write barrier here?
        Queue->NextEntryToWrite = NewNextEntryToWrite;
        // increase semaphore count so that a thread blocked by sem_wait() can wake up
        sem_post(&Queue->SemaphoreHandle);
    }
}

internal b32
LinuxDoNextWorkQueueEntry(platform_work_queue *Queue)
{
    b32 WeShouldSleep = false;
    u32 OriginalNextEntryToRead = Queue->NextEntryToRead;
    u32 NewNextEntryToRead = (OriginalNextEntryToRead + 1) % ArrayCount(Queue->Entries);
    if (OriginalNextEntryToRead != Queue->NextEntryToWrite)
    {
        // That __sync function is a GCC thing. Might also work on LLVM.
        u32 Index = __sync_val_compare_and_swap(&Queue->NextEntryToRead, OriginalNextEntryToRead, 
                                                NewNextEntryToRead);
        if (Index == OriginalNextEntryToRead)
        {
            platform_work_queue_entry Entry = Queue->Entries[Index];
            Entry.Callback(Queue, Entry.Data);
        }
    }
    else
    {
        WeShouldSleep = true;
    }
    
    return WeShouldSleep;
}

internal void *
ThreadProc(void *Arg)
{
    platform_work_queue *Queue = (platform_work_queue *)Arg;
    for (;;)
    {
        if (LinuxDoNextWorkQueueEntry(Queue))
        {
            sem_wait(&Queue->SemaphoreHandle);  // decrement
        }
    }
}

internal void
LinuxMakeQueue(platform_work_queue *Queue, u32 ThreadCount)
{
    Queue->NextEntryToWrite = 0;
    Queue->NextEntryToRead = 0;
    u32 InitialCount = 0;
    sem_init(&Queue->SemaphoreHandle, 0, InitialCount); 
    
    for (u32 ThreadIndex = 0; ThreadIndex < ThreadCount; ThreadIndex++)
    {
        pthread_t ThreadID;
        pthread_create(&ThreadID,
                       0, // const pthread_attr_t *restrict attr,
                       ThreadProc,
                       Queue);
    }
}

internal b32
HandleReceiveError(int BytesReceived, SOCKET ClientSocket)
{
    b32 Success = true;
    if (BytesReceived < 0)
    {
        perror("recv failed");
        // TODO(vincent): Now that I think about it, perror() might not be thread-safe,
        // because it uses some global variable iirc.
        // not sure if we care very much though.
        Success = false;
    }
    return Success;
}

internal b32
HandleSendError(int BytesSent, SOCKET ClientSocket)
{
    b32 Success = true;
    if (BytesSent == -1)
    {
        perror("send failed");
        Success = false;
    }
    return Success;
}

internal void
ShutdownConnection(SOCKET ClientSocket)
{
    close(ClientSocket);
}

int main(void)
{
    // NOTE(vincent): Initialize threads and work queue
    platform_work_queue Queue = {};
    LinuxMakeQueue(&Queue, NUMBER_OF_THREADS - 1);
    
    // NOTE(vincent): Initializing server memory
    server_memory ServerMemory = {};
    void *BaseAddress = 0;
    ServerMemory.StorageSize = SERVER_STORAGE_SIZE;
    ServerMemory.Storage = mmap(BaseAddress, ServerMemory.StorageSize, PROT_READ | PROT_WRITE,
                                MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (ServerMemory.Storage == MAP_FAILED)
    {
        perror("mmap failed");
        return 1;
    }
    initialize_server_memory_result InitResult = 
        InitializeServerMemory(&ServerMemory, &Queue, LinuxAddEntry, LinuxDoNextWorkQueueEntry);
    
    if (InitResult.ParsingErrorCount == 0)
    {
        struct addrinfo *AddressInfo = 0;
        struct addrinfo Hints;
        ZeroBytes((char *)&Hints, sizeof(Hints));
        Hints.ai_family = AF_UNSPEC;
        Hints.ai_socktype = SOCK_STREAM;
        Hints.ai_protocol = IPPROTO_TCP;
        Hints.ai_flags = AI_PASSIVE;      // "use my IP"
        
        // Resolve the local address and port to be used by the server
        int AddressInfoResult = getaddrinfo(0, InitResult.PortString, &Hints, &AddressInfo);
        if (AddressInfoResult != 0) 
        {
            fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(AddressInfoResult));
            return 1;
        }
        
        SOCKET ListenSocket = INVALID_SOCKET;
        
        // loop through all the results and bind to the first we can
        struct addrinfo *P;
        int One = 1;
        for(P = AddressInfo; 
            P; 
            P = P->ai_next) 
        {
            if ((ListenSocket = socket(P->ai_family, P->ai_socktype, P->ai_protocol)) == -1) 
            {
                perror("socket() failed");
                continue;
            }
            
            if (setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEADDR, &One, sizeof(int)) == -1) 
            {
                perror("setsockopt() failed");
                exit(1);
            }
            
            if (bind(ListenSocket, P->ai_addr, P->ai_addrlen) == -1) 
            {
                close(ListenSocket);
                perror("bind() failed");
                continue;
            }
            break;  // we break here when the three calls were successful
        }
        
        freeaddrinfo(AddressInfo); // all done with this structure
        
        if (P == 0)  
        {
            fprintf(stderr, "failed to bind\n");
            exit(1);
        }
        
        if (listen(ListenSocket, BACKLOG) == -1) 
        {
            perror("listen");
            exit(1);
        }
        
        struct sockaddr_storage TheirAddress; // connector's address information
        socklen_t SizeTheirAddress = sizeof(TheirAddress);
        printf("Server: waiting for a connection on port %s\n", InitResult.PortString);
        
        for (;;)
        {  
            // Accept a client socket
            SOCKET ClientSocket = 
                accept(ListenSocket, (struct sockaddr *)&TheirAddress, &SizeTheirAddress);
            if (ClientSocket == -1) 
            {
                perror("accept failed");
                continue;
            }
            
            PrepareHandshaking(&ServerMemory, (struct sockaddr *)&TheirAddress, ClientSocket, &Queue);
        }
    }
    
    return 0;
}
