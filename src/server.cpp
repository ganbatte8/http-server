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