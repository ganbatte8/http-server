
enum http_method
{
    HttpMethod_Other,
    HttpMethod_Get,
};

enum http_version
{
    HttpVersion_10,
    HttpVersion_11,
    HttpVersion_20
};

struct http_request
{
    http_method Method;
    string RequestPath;
    http_version HttpVersion;
    string Host;
    string AuthString;
    b32 IsValid;
};

internal http_request
ParseHTTPRequest(char *ReceiveBuffer, int BytesReceived)
{
    http_request Result = {}; // IsValid is false until proven otherwise.
    
    // Parse the lines before parsing further:
    string RequestLines[512] = {};
    u32 RequestLinesCount = 0;
    u32 BOL = 0;
    
    // NOTE(vincent): This function would be cleaner if we could jump to a goto label
    // across initialization statements :( instead we have to deal with this FoundError mess.
    b32 FoundError = false;
    
    for (int ByteIndex = 0; ByteIndex < BytesReceived; ByteIndex++)
    {
        char C = ReceiveBuffer[ByteIndex];
        if (C == '\r')
        {
            u32 LineLength = ByteIndex - BOL;
            ByteIndex++;
            if (ByteIndex < BytesReceived)
            {
                C = ReceiveBuffer[ByteIndex];
                if (C == '\n')
                {
                    ByteIndex++;
                }
                else
                {
                    FoundError = true;
                    break;
                }
            }
            else
            {
                FoundError = true;
                break;
            }
            RequestLines[RequestLinesCount] = StringBaseLength(ReceiveBuffer + BOL, LineLength);
            RequestLinesCount++;
            if (LineLength == 0)
            {
                break; // reached CRLFCRLF
            }
            if (RequestLinesCount == ArrayCount(RequestLines))
            {
                // probably don't want to truncate the request and pretend it's valid
                FoundError = true;
                break;
            }
            BOL = ByteIndex;
        }
    }
    
    if (RequestLinesCount <= 1) // we want at least two lines: the first one and the Host field
        FoundError = true;
    
    if (!FoundError)
    {
        // Parse the first line. We are expecting three parts separated by individual spaces:
        // the HTTP method, the HTTP request path, and the HTTP version.
        string FirstLineWords[3];
        string FirstLine = RequestLines[0];
        b32 InWord = false;
        u32 WordIndex = 0;
        for (u32 CharIndex = 0; CharIndex < FirstLine.Length; CharIndex++)
        {
            char *C = FirstLine.Base + CharIndex;
            if (!InWord && *C != ' ')
            {
                InWord = true;
                FirstLineWords[WordIndex].Base = C;
            }
            if (InWord && *C == ' ')
            {
                InWord = false;
                FirstLineWords[WordIndex].Length = (u32)(C - FirstLineWords[WordIndex].Base);
                WordIndex++;
                if (WordIndex == 3)
                {
                    FoundError = true;
                    break;
                }
            }
        }
        
        if (!FoundError && WordIndex == 2)
        {
            FirstLineWords[WordIndex].Length = 
                (u32)(FirstLine.Base + FirstLine.Length - FirstLineWords[WordIndex].Base);
            
            // Successfully found three words. Figure out the method, path and version.
            
            if (StringsAreEqual(FirstLineWords[0], "GET"))
            {
                Result.Method = HttpMethod_Get;
            }
            else
                goto Goto_EndHttpParsing;
            
            Result.RequestPath = FirstLineWords[1];
            
            if (StringBeginsWith(FirstLineWords[2], "HTTP/"))
            {
                string NumberPart = StringFromOffset(FirstLineWords[2], 5);
                if (StringsAreEqual(NumberPart, "1.0"))
                    Result.HttpVersion = HttpVersion_10;
                else if (StringsAreEqual(NumberPart, "2.0"))
                    Result.HttpVersion = HttpVersion_20;
                else
                    Result.HttpVersion = HttpVersion_11;
            }
            else
                goto Goto_EndHttpParsing;
            
            // Parse other lines
            for (u32 LineIndex = 1; LineIndex < RequestLinesCount; LineIndex++)
            {
                string Line = RequestLines[LineIndex];
                string Field = StringPrefixUntil(Line, ':');
                
                // A few notes about this loop:
                // - This could be inefficient if we threw a bunch of field strings to test here.
                //   If we have to read many headers, maybe hash the Field so each loop iteration happens
                //   in O(n) string reads instead of O(n^2).
                //   If that seems worth the trouble, profile it first.
                // - If you're thinking about designing a file format or a protocol like HTTP, 
                //   consider speccing the headers/fields to always be in the same order 
                //   so we don't have to do all this work.
                // - Host is mandatory for a valid request.
                
                // TODO(vincent): maybe figure out a way to break out early 
                // when we read all the headers we wanted.
                if (StringsAreEqual(Field, "Host"))
                {
                    Result.Host = StringBaseEnder(Field.Base + Field.Length + 2, '\r');
                    Result.IsValid = true;
                }
                else if (StringsAreEqual(Field, "Authorization"))
                {
                    string AuthString = StringBaseEnder(Field.Base + Field.Length + 2, '\r');
                    string AuthTypeString = StringBaseEnder(AuthString.Base, ' ');
                    if (StringsAreEqual(AuthTypeString, "Basic"))
                    {
                        Result.AuthString = 
                            StringBaseEnder(AuthTypeString.Base + AuthTypeString.Length + 1, '\r');
                    }
                }
            }
        } // END if (!FoundError && WordIndex == 2)
    }
    
    Goto_EndHttpParsing:
    return Result;   // NOTE(vincent): Function always exits here.
}

#if 0
GET / HTTP/1.1
Host: localhost:3490
Connection: keep-alive
Cache-Control: max-age=0
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/89.0.4389.105 Safari/537.36
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9
Sec-GPC: 1
Sec-Fetch-Site: none
Sec-Fetch-Mode: navigate
Sec-Fetch-User: ?1
Sec-Fetch-Dest: document
Accept-Encoding: gzip, deflate, br
Accept-Language: ja,en-US;q=0.9,en;q=0.8

*/
#endif 



