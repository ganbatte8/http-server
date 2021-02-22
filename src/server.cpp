#include "server.h"
#include "server_config_loader.cpp"

internal http_request
ParseHTTPRequest(char *ReceivedRequest, u32 RequestLength)
{
    http_request Result;
    char *S = ReceivedRequest;
    u32 MinimumLength = 10; // all the work we're really doing is parse the GET and the path; 
    // beyond that I'm assuming there should always be more stuff, but I don't exactly know how much.
    // TODO(vincent): how big is an HTTP request ?
    
    if (RequestLength >= MinimumLength && S[0] == 'G' && S[1] == 'E' && S[2] == 'T' && S[3] == ' ')
    {
        Result.Method = HttpMethod_Get;
        Result.Path.Base = S + 4;
        
        u32 PathLength = 0;
        while (PathLength + 4 < RequestLength && Result.Path.Base[PathLength] != ' ')
            PathLength++;
        Result.Path.Length = PathLength;
    }
    
    else
        Result.Method = HttpMethod_Other;
    
    return Result;
}

internal b32
FourBytesCRLF(char *Base)
{
    b32 Result = (Base[0] == '\r' && Base[1] == '\n' && Base[2] == '\r' && Base[3] == '\n'); 
    return Result;
}

internal u32
StringLineLength(char *String)
{
    u32 Count;
    while (*String && *String != '\r' && *String != '\n')
    {
        String++;
        Count++;
    }
    return Count;
}


internal u32
InitializeServerMemory()
{
    size_t BlockMemorySize = Megabytes(64);
    void *Memory = malloc(BlockMemorySize);
    if (!Memory)
    {
        fprintf(stderr, "Couldn't allocate server memory\n");
        return 0;
    }
    
    // TODO(vincent): idk if we even want a memory arena,
    // but we do want some kind of place for memory, probably. 
    // Need to think about our use cases.
    
    /*
    server_state *State = Memory;
    InitializeArena(State->Arena, BlockMemorySize - sizeof(server_state), 
                    Memory + sizeof(server_state));
    parse_config_file_result *Config = PushStruct(&State->Arena, parse_config_file_result);
    */
    parsed_config_file_result Config;
    ParseConfigFile(&Config);
    return 1;
}

internal u32
SprintfUntilDelimiter(char *Dest, char *Source, char Delimiter)
{
    u32 Count = 0;
    while (*Source && *Source != Delimiter)
    {
        *Dest++ = *Source++;
        Count++;
    }
    return Count;
}

internal int
HandleConnection(struct sockaddr *IncomingAddress, SOCKET ClientSocket,
                 char *ReceiveBuffer, u32 ReceiveBufferLength,
                 char *ReceiveBufferHex)
{
    //int SizeTheirAddress = sizeof(*IncomingAddress);
    char AddressString[INET6_ADDRSTRLEN];
    inet_ntop(IncomingAddress->sa_family, GetInternetAddress(IncomingAddress),
              AddressString, sizeof(AddressString));
    
    char StringBuffer[1024];
    string ToPrint;
    ToPrint.Base = StringBuffer;
    ToPrint.Length = 0;
    
    ToPrint.Length += 
        sprintf(StringBuffer + ToPrint.Length, "Server: got connection from %s ", AddressString);
    
    int BytesReceived;
    for (;;)
    {
        BytesReceived = recv(ClientSocket, ReceiveBuffer, ReceiveBufferLength, 0);
        if (BytesReceived <= 0 || BytesReceived == INVALID_SOCKET)
            break;
        
#if 0
        ToPrint.Length += 
            sprintf(StringBuffer + ToPrint.Length, "BytesReceived: %d\n", BytesReceived);
        BinaryToHexadecimal(ReceiveBuffer, ReceiveBufferHex, BytesReceived);
        ToPrint.Length +=
            sprintf(StringBuffer + ToPrint.Length, "\n%s\n\n%s", ReceiveBufferHex, ReceiveBuffer);
        // TODO(vincent): check for null-termination since we are printing ReceiveBuffer?
        // TODO(vincent): what if someone sends us a giant thing
        ToPrint.Length +=
            sprintf(StringBuffer + ToPrint.Length, "Bytes received: %d\n", BytesReceived);
#endif
        
        if (BytesReceived >= 10 && FourBytesCRLF(ReceiveBuffer + BytesReceived - 4))
        {
            // NOTE(vincent): if we receive some text that ends with \r\n\r\n, 
            // it may be an HTTP request. We want to parse the method and the path of the HTTP request.
            http_request Request = ParseHTTPRequest(ReceiveBuffer, BytesReceived-4);
            ToPrint.Length += 
                SprintfUntilDelimiter(StringBuffer + ToPrint.Length, ReceiveBuffer, '\r');
            break;
        }
        
        // TODO(vincent): should we reset the buffer to zeroes?
    }
    
    puts(ToPrint.Base);
    //printf("%u\n", ToPrint.Length);
    //printf("hello\n");
    
    return BytesReceived;
}