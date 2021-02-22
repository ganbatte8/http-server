#include "server_config_loader.h"


internal void
AddToken(read_file_result Source, scanner_location *Scanner, parsed_config_tokens *T,
         token_hint Hint)
{
    config_token Token;
    Token.Type = Hint.Type;
    Token.Row = Scanner->Row;
    Token.Column = Scanner->Column;
    Token.Lexeme.Base = Source.Content + Scanner->Start;
    Token.Lexeme.Length = Scanner->Current - Scanner->Start + 1;
    
    switch (Hint.Type)
    {
        case ConfigTokenType_String: Token.Lexeme = Hint.String; break;
        case ConfigTokenType_Integer: Token.Value = Hint.Value; break;
    }
    
    if (T->Count < ArrayCount(T->Tokens))
    {
        T->Tokens[T->Count] = Token;
        T->Count++;
    }
    else
    {
        fprintf(stderr, "Scanning error: too many tokens. (Max is %llu)\n", ArrayCount(T->Tokens));
        Scanner->ErrorCount++;
    }
}

internal b32 
ScannerMatch(read_file_result Source, scanner_location *Scanner, char Expected)
{
    if (Scanner->Current >= Source.Size)
        return false;
    if (Source.Content[Scanner->Current] != Expected)
        return false;
    Scanner->Current++;
    return true;
}

internal b32
IsDigit(char C)
{
    b32 Result = ('0' <= C && C <= '9');
    return Result;
}

internal b32
IsAlpha(char C)
{
    b32 Result = ('a' <= C && C <= 'z') || ('A' <= C && C <= 'Z') || (C == '_');
    return Result;
}

internal b32
IsAlphaNumeric(char C)
{
    b32 Result = IsAlpha(C) || IsDigit(C);
    return Result;
}

internal char
ScannerPeek(read_file_result Source, scanner_location *Scanner)
{
    if (Scanner->Current < Source.Size)
        return Source.Content[Scanner->Current];
    return 0;
}

internal void
ScanString(read_file_result Source, scanner_location *Scanner, parsed_config_tokens *Tokens)
{
    printf("ScanString: (%u, %u)\n", Scanner->Row, Scanner->Column);
    while (ScannerPeek(Source, Scanner) != '"' && Scanner->Current < Source.Size)
    {
        if (ScannerPeek(Source, Scanner) == '\n') 
            // TODO(vincent): we probably don't want to support multiline strings
        {
            Scanner->Current++;
            Scanner->Row++;
            Scanner->Column = 1;
            fprintf(stderr, "Scanning error: newline before end of string (%u, %u)\n", 
                    Scanner->Row, Scanner->Column);
            Scanner->ErrorCount++;
        }
        else
        {
            Scanner->Current++;
            Scanner->Column++;
        }
    }
    if (Scanner->Current >= Source.Size)
    {
        fprintf(stderr, "Scanning error: unterminated string: Row %u, Column %u\n", Scanner->Row, Scanner->Column);
        Scanner->ErrorCount++;
        return;
    }
    
    
    string String;
    String.Base = Source.Content + Scanner->Start + 1;
    String.Length = Scanner->Current - Scanner->Start - 1;
    
    // Advancing over the unquote symbol:
    Scanner->Current++;
    Scanner->Column++;
    
    AddToken(Source, Scanner, Tokens, TokenHint(ConfigTokenType_String, String));
}

internal void 
ScanNumber(read_file_result Source, scanner_location *Scanner, parsed_config_tokens *Tokens)
{
    u32 Value = Source.Content[Scanner->Start] - '0';
    b32 OverflowSixteen = false;
    for (;;)
    {
        char C = ScannerPeek(Source, Scanner);
        if (IsDigit(C))
        {
            Value = Value * 10 + (C - '0');
            if (Value >= (1<<16))
                OverflowSixteen = true;
            Scanner->Current++;
            Scanner->Column++;
        }
        else
            break;
    }
    
    if (OverflowSixteen)
    {
        fprintf(stderr, "Number literal overflows 16-bit: Row %u Column %u\n", 
                Scanner->Row, Scanner->Column);
        Scanner->ErrorCount++;
    }
    
    AddToken(Source, Scanner, Tokens, TokenHint(ConfigTokenType_Integer, Value)); 
}

internal void 
ScanIdentifier(read_file_result Source, scanner_location *Scanner, 
               parsed_config_tokens *Tokens)
{
    while (IsAlphaNumeric(ScannerPeek(Source, Scanner)))
    {
        Scanner->Current++;
        Scanner->Column++;
    }
    
    string Identifier;
    Identifier.Base = Source.Content + Scanner->Start;
    Identifier.Length = Scanner->Current - Scanner->Start;
    
    if (StringsAreEqual(Identifier, "port"))
    {
        printf("ScanIdentifier port: (%u, %u)\n", Scanner->Row, Scanner->Column);
        AddToken(Source, Scanner, Tokens, TokenHint(ConfigTokenType_Port, 0));
    }
    else if (StringsAreEqual(Identifier, "root"))
    {
        printf("ScanIdentifier root: (%u, %u)\n", Scanner->Row, Scanner->Column);
        AddToken(Source, Scanner, Tokens, TokenHint(ConfigTokenType_Root, 0));
    }
    else
    {
        fprintf(stderr, "Unknown identifier (%u, %u)\n", Scanner->Row, Scanner->Column);
        Scanner->ErrorCount++;
    }
    
}
internal void
ScanToken(read_file_result Source, scanner_location *Scanner, parsed_config_tokens *Tokens)
{
    char C = Source.Content[Scanner->Current];
    Scanner->Current++;
    Scanner->Column++;
    
    switch(C)
    {
        case ':': AddToken(Source, Scanner, Tokens, TokenHint(ConfigTokenType_Colon, 0)); 
        break;
        
        case '/':
        if (ScannerMatch(Source, Scanner, '/'))      // found end-of-line comment
        {
            while (ScannerPeek(Source, Scanner) != '\n' && Scanner->Current < Source.Size)
            {
                Scanner->Current++;
            }
            printf("Scanning: Slash (%u, %u)\n", Scanner->Row, Scanner->Column);
        }
        break;
        
        case ' ':
        case '\r':
        case '\t':
        // ignore whitespace
        break;
        
        case '\n':
        Scanner->Row++;
        Scanner->Column = 1;
        printf("Scanning: Linefeed (%u, %u)\n", Scanner->Row, Scanner->Column);
        break;
        
        case '"':
        ScanString(Source, Scanner, Tokens);
        break;
        
        default: 
        if (IsDigit(C))
            ScanNumber(Source, Scanner, Tokens);
        else if (IsAlpha(C))
            ScanIdentifier(Source, Scanner, Tokens);
        else
        {
            fprintf(stderr, "Scanning Error: Unexpected character %c (%u, %u)\n", 
                    C, Scanner->Row, Scanner->Column);
            Scanner->ErrorCount++;
        }
        
    }
    
}

internal u32
ParseConfigFile(parsed_config_file_result *Result)
{
    read_file_result ReadFileResult = DEBUGReadEntireFile("config");
    
    if (!ReadFileResult.Content)
    {
        fprintf(stderr, "Couldn't load config file.\n");
        return 0;
    }
    
    scanner_location Scanner;
    Scanner.Start = 0;     // points to the first character in the lexeme being considered
    Scanner.Current = 0;   // points to the character being considered
    Scanner.Row = 1;
    Scanner.Column = 1;
    Scanner.ErrorCount = 0;
    parsed_config_tokens Tokens = {};
    
    // NOTE(vincent): This is the scanning loop! It transforms text into tokens.
    while (Scanner.Current < ReadFileResult.Size)
    {
        Scanner.Start = Scanner.Current;
        ScanToken(ReadFileResult, &Scanner, &Tokens);
    }
    
    printf("\nConfig file: scanned %u tokens.\n", Tokens.Count);
    for (u32 TokenIndex = 0; TokenIndex < Tokens.Count; TokenIndex++)
    {
        config_token T = Tokens.Tokens[TokenIndex];
        switch (T.Type)
        {
            case ConfigTokenType_Colon: printf("Colon (%u,%u)\n", T.Row, T.Column); break;
            case ConfigTokenType_String:
            printf("String (%u,%u): %.*s\n", T.Row, T.Column, T.Lexeme.Length, T.Lexeme.Base); break;
            case ConfigTokenType_Integer: printf("Integer (%u,%u): %u\n", T.Row, T.Column, T.Value); break;
            case ConfigTokenType_Port: printf("Port (%u,%u)\n", T.Row, T.Column); break;
            case ConfigTokenType_Root: printf("Root (%u,%u)\n", T.Row, T.Column); break;
            default: InvalidCodePath;
        }
    }
    
    if (Scanner.ErrorCount > 0)
    {
        printf("Found %u scanning errors. Tokens won't be processed.\n", 
               Scanner.ErrorCount);
    }
    else
    {
        printf("\nProcessing tokens...\n");
        // we probably don't care very much about having a robust token grammar here.
        // so we just loop through the tokens, and overwrite the port/root field whenever we
        // get a new value for that field.
        config_token_type LastType = ConfigTokenType_Invalid;
        for (u32 TokenIndex = 0; TokenIndex < Tokens.Count; TokenIndex++)
        {
            config_token T = Tokens.Tokens[TokenIndex];
            switch (T.Type)
            {
                case ConfigTokenType_Colon: break;
                
                case ConfigTokenType_String: 
                if (LastType == ConfigTokenType_Root)
                {
                    sprintf(Result->Root, "%.*s", 
                            Minimum(T.Lexeme.Length, ArrayCount(Result->Root)-1), T.Lexeme.Base); 
                    Result->RootSet = true;
                }
                break;
                
                case ConfigTokenType_Integer: 
                if (LastType == ConfigTokenType_Port)
                {
                    Result->Port = T.Value;
                    Result->PortSet = true;
                }
                break;
                
                case ConfigTokenType_Port:
                case ConfigTokenType_Root: LastType = T.Type; 
                break;
                
                default: InvalidCodePath;
            }
        }
        
        // TODO(vincent): count set/not set as token errors ?
        if (Result->PortSet)
        {
            IntegerToString(Result->Port, Result->PortString);
            printf("Parsed and set port: %s\n", Result->PortString);
        }
        else
            printf("Didn't set the port\n");
        if (Result->RootSet)
            printf("Parsed and set root: %s\n", Result->Root);
        else
            printf("Didn't set the root\n");
    }
    
    free(ReadFileResult.Content);
    return Scanner.ErrorCount;  // TODO(vincent): signal whether successful or not ?
}
