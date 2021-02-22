enum http_method
{
    HttpMethod_Other,
    HttpMethod_Get,
};

struct http_request
{
    http_method Method;
    string Path;
};

struct server_state
{
    memory_arena *ServerArena;
    
};