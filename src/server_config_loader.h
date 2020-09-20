
struct parsed_config_file_result
{
    u32 Port;
    char PortString[6];   // the actual port used by Windows and Linux, it looks like
    char Root[65535];
    b32 PortSet;
    b32 RootSet;
};

enum config_token_type
{
    ConfigTokenType_Colon,
    ConfigTokenType_String,
    ConfigTokenType_Integer,
    ConfigTokenType_Port,
    ConfigTokenType_Root,
    ConfigTokenType_Invalid,
};

struct config_token
{
    config_token_type Type;
    string Lexeme;
    u32 Value;
    u32 Row;
    u32 Column;
};

struct parsed_config_tokens
{
    config_token Tokens[32];
    u32 Count;
};

struct scanner_location
{
    u32 Start;
    u32 Current;
    u32 Row;
    u32 Column;
    //char *Source;
    u32 ErrorCount;
};

struct token_hint
{
    config_token_type Type;
    union
    {
        string String;
        u32 Value;
    };
};

internal token_hint
TokenHint(config_token_type Type, string String)
{
    token_hint Result;
    Result.Type = Type;
    Result.String = String;
    return Result;
}

internal token_hint
TokenHint(config_token_type Type, u32 Value)
{
    token_hint Result;
    Result.Type = Type;
    Result.Value = Value;
    return Result;
}
