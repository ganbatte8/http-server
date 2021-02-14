#include <stdio.h>
#include <malloc.h>

internal void
DEBUGFreeFileMemory(void *Content)
{
    if (Content)
        free(Content);
}

struct debug_read_file_result
{
    size_t Size;
    void *Content;
};

internal debug_read_file_result
DEBUGReadEntireFile(char *Filename)
{
    debug_read_file_result Result = {};
    FILE *File = fopen(Filename, "rb");
    if (File)
    {
        fseek(File, 0, SEEK_END);
        size_t FileSize = ftell(File);
        fseek(File, 0, SEEK_SET);
        Result.Content = malloc(FileSize);
        Result.Size = FileSize;
        if (Result.Content)
        {
            fread(Result.Content, FileSize, 1, File);
        }
        fclose(File);
    }
    
    // TODO(vincent): I'm pretty much not doing anything if an error happens
    // for malloc(), fopen(), fread()...
    
    return Result;
}

// NOTE(vincent): ReadEntireFileInto is the same as ReadEntireFile, except it doesn't malloc
// and writes into a pointer parameter instead.

internal debug_read_file_result
DEBUGReadEntireFileInto(char *Filename, char *Buffer)
{
    debug_read_file_result Result = {};
    FILE *File = fopen(Filename, "rb");
    if (File)
    {
        fseek(File, 0, SEEK_END);
        size_t FileSize = ftell(File);
        fseek(File, 0, SEEK_SET);
        Result.Content = Buffer;
        Result.Size = FileSize;
        if (Result.Content)
        {
            fread(Result.Content, FileSize, 1, File);
        }
        fclose(File);
    }
    
    // TODO(vincent): error checking ?
    return Result;
}


internal b32
DEBUGWriteEntireFile(char *Filename, u32 Size, void *Memory)
{
    b32 Result = false;
    
    FILE *File = fopen(Filename, "wb");
    if (File)
    {
        size_t BytesWritten = fwrite(Memory, 1, Size, File);
        fclose(File);
        if (BytesWritten == Size)
            Result = true;
    }
    
    // TODO(vincent): error checking ?
    return Result;
}
