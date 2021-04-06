
internal void
DEBUGFreeFileMemory(void *Content)
{
    if (Content)
        free(Content);
}

struct read_file_result
{
    size_t Size;
    char *Content;
};

internal read_file_result
DEBUGReadEntireFile(const char *Filename)
{
    read_file_result Result = {};
    FILE *File = fopen(Filename, "rb");
    if (File)
    {
        fseek(File, 0, SEEK_END);
        size_t FileSize = ftell(File);
        fseek(File, 0, SEEK_SET);
        Result.Content = (char *)malloc(FileSize);
        Result.Size = FileSize;
        if (Result.Content)
        {
            fread(Result.Content, FileSize, 1, File);
        }
        fclose(File);
    }
    
    return Result;
}

// NOTE(vincent): ReadEntireFileInto is the same as ReadEntireFile, except it doesn't malloc
// and writes into a pointer parameter instead.

internal read_file_result
DEBUGReadEntireFileInto(char *Filename, char *Buffer)
{
    read_file_result Result = {};
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
    
    return Result;
}


struct safer_read_file_result
{
    size_t Size;
    b32 Success;
};

internal safer_read_file_result
SaferReadEntireFileInto(char *Dest, char *Filename, u32 AvailableSize)
{
    safer_read_file_result Result = {};
    FILE *File = fopen(Filename, "rb");
    if (File)
    {
        fseek(File, 0, SEEK_END);
        size_t FileSize = ftell(File);
        fseek(File, 0, SEEK_SET);
        if (FileSize <= AvailableSize)
        {
            size_t BytesWritten = fread(Dest, 1, FileSize, File);
            if (BytesWritten == FileSize)
            {
                Result.Size = FileSize;
                Result.Success = true;
            }
        }
        fclose(File);
    }
    
    return Result;
}

struct push_read_entire_file
{
    char *Memory;
    size_t Size;
    b32 Success;
};
internal push_read_entire_file
PushReadEntireFile(memory_arena *Arena, char *Filename)
{
    push_read_entire_file Result = {};
    
    FILE *File = fopen(Filename, "rb");
    if (File)
    {
        fseek(File, 0, SEEK_END);
        Result.Size = ftell(File);
        fseek(File, 0, SEEK_SET);
        u32 AvailableSize = Arena->Size - Arena->Used;
        if (Result.Size <= AvailableSize)
        {
            Result.Memory = PushArray(Arena, (u32)Result.Size, char);
            size_t BytesWritten = fread(Result.Memory, 1, Result.Size, File);
            if (BytesWritten == Result.Size)
                Result.Success = true;
        }
        fclose(File);
        // NOTE(vincent): Fun fact: I had forgotten to put fclose() here. The result was that
        // when you keep reloading the same page after a certain number of times,
        // the server would return 404 errors exclusively.
    }
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
    
    return Result;
}
