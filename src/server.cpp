enum http_method
{
    HttpMethod_Get,
    HttpMethod_Other
};

struct string
{
    char *Base;
    u32 Length;
};

struct http_request
{
    http_method Method;
    string Path;
};

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


internal int
HandleConnection(struct sockaddr *IncomingAddress, SOCKET ClientSocket,
                 char *ReceiveBuffer, u32 ReceiveBufferLength,
                 char *ReceiveBufferHex)
{
    //int SizeTheirAddress = sizeof(*IncomingAddress);
    char AddressString[INET6_ADDRSTRLEN];
    inet_ntop(IncomingAddress->sa_family, GetInternetAddress(IncomingAddress),
              AddressString, sizeof(AddressString));
    
    printf("Server: got connection from %s\n", AddressString);
    
    int BytesReceived;
    for (;;)
    {
        BytesReceived = recv(ClientSocket, ReceiveBuffer, ReceiveBufferLength, 0);
        if (BytesReceived <= 0 || BytesReceived == INVALID_SOCKET)
            break;
        
        printf("BytesReceived: %d\n", BytesReceived);
        BinaryToHexadecimal(ReceiveBuffer, ReceiveBufferHex, BytesReceived);
        printf("\n%s\n\n%s", ReceiveBufferHex, ReceiveBuffer);
        // TODO(vincent): check for null-termination?
        // TODO(vincent): parse HTTP requests here ? \r\n thing
        if (ReceiveBuffer[BytesReceived-1] == '\n')
        {
            break;
        }
        // TODO(vincent): should we reset the buffer to zeroes?
    }
    
    
    //ParseHTTPRequest(ReceiveBuffer);
    
    
    
    return BytesReceived;
}