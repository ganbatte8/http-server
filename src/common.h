#include <stdint.h>
#include <stdio.h>
#include <malloc.h>  // TODO(vincent): maybe get rid of this



#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_GCC)
#define COMPILER_GCC 0
#endif

#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_GCC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
// TODO(vincent): can we detect whether the current compiler is LLVM / GCC ?
#endif
#endif

#define internal static

#define Kilobytes(Value) ((Value) * 1000LL)
#define Megabytes(Value) (Kilobytes(Value) * 1000LL)
#define Gigabytes(Value) (Megabytes(Value) * 1000LL)
#define Terabytes(Value) (Gigabytes(Value) * 1000LL)

#define SERVER_STORAGE_SIZE Megabytes(64)

#define NUMBER_OF_THREADS 4
// NOTE(vincent): Needs to be at least 1, ideally <= the number of cores on the machine.
// TODO(vincent): maybe we can ask the OS to query how many cores we have

#define DEFAULT_SERVER_PORT "80"  // the port users will be connecting to


#if DEBUG
#define Assert(Expression) if (!(Expression)) {*(int *)0 = 0;}
#define InvalidCodePath {*(int *)0 = 0;}
#define InvalidDefaultCase default: {InvalidCodePath;}
#else
#define Assert(Expression)
#define InvalidCodePath
#define InvalidDefaultCase default: {}
#endif

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int b32;
typedef float f32;
typedef double f64;


struct memory_arena
{
    u32 Size;
    u8 *Base;
    u32 Used;
    s32 TempCount;
};

inline void
InitializeArena(memory_arena *Arena, u32 Size, void *Base)
{
    Arena->Size = Size;
    Arena->Base = (u8 *)Base;
    Arena->Used = 0;
    Arena->TempCount = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))

internal b32
BytesAreZero(char *Buffer, u32 BytesCount)
{
    for (u32 Byte = 0; Byte < BytesCount; Byte++)
    {
        if (Buffer[Byte])
            return 0;
    }
    return 1;
}

internal void
ZeroBytes(char *Buffer, u32 BytesCount)
{
    for (u32 Byte = 0; Byte < BytesCount; Byte++)
    {
        Buffer[Byte] = 0;
    }
}

inline void *
PushSize_(memory_arena *Arena, u32 Size)
{
    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used;
    Arena->Used += Size;
    return Result;
}


struct temporary_memory
{
    memory_arena *Arena;
    u32 Used;
};

internal temporary_memory
BeginTemporaryMemory(memory_arena *Arena)
{
    temporary_memory Result;
    Result.Arena = Arena;
    Result.Used = Arena->Used;
    ++Arena->TempCount;
    
    return Result;
}

internal void
EndTemporaryMemory(temporary_memory TempMemory)
{
    memory_arena *Arena = TempMemory.Arena;
    Assert(Arena->Used >= TempMemory.Used);
    Arena->Used = TempMemory.Used;
    --Arena->TempCount;
    Assert(Arena->TempCount >= 0);
}

internal void
CheckArena(memory_arena *Arena)
{
    Assert(Arena->TempCount == 0);
}

internal void
SubArena(memory_arena *Result, memory_arena *Arena, u32 Size)
{
    Result->Size = Size;
    Result->Base = (u8 *)PushSize_(Arena, Size);
    Result->Used = 0;
    Result->TempCount = 0;
}

// NOTE(vincent): forward declaring three functions that the server code needs 
// and that the platform layer has to implement:
#if COMPILER_GCC
typedef int SOCKET;        // This is to make the Linux platform "understand" this Windows type
#define INVALID_SOCKET -1  // Same
#endif
internal b32 HandleReceiveError(int BytesReceived, SOCKET ClientSocket);
internal b32 HandleSendError(int BytesSent, SOCKET ClientSocket);
internal void ShutdownConnection(SOCKET ClientSocket);

struct platform_work_queue;
#define PLATFORM_WORK_QUEUE_CALLBACK(name) void name(platform_work_queue *Queue, void *Data)
typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);

typedef void platform_add_entry(platform_work_queue *Queue, 
                                platform_work_queue_callback *Callback, void *Data);


struct platform_work_queue_entry
{
    platform_work_queue_callback *Callback;
    void *Data;
};

#define PLATFORM_DO_NEXT_WORK_ENTRY(name) b32 name(platform_work_queue *Queue)
typedef PLATFORM_DO_NEXT_WORK_ENTRY(platform_do_next_work_entry);

struct server_memory
{
    u32 StorageSize;
    void *Storage;
    
    platform_work_queue *Queue;
    platform_add_entry *PlatformAddEntry;
    platform_do_next_work_entry *PlatformDoNextWorkEntry;  // NOTE(vincent): for the main thread
};






// ---------------------- Math, bytes and string manipulation -----------------------

internal void
WriteStringLiteral(char *Buffer, const char *Literal)
{
    // NOTE(vincent): Buffer is assumed to be big enough.
    while (*Literal)
        *Buffer++ = *Literal++;
    *Buffer = 0;
}

struct string
{
    char *Base;
    u32 Length;
};

internal string
StringBaseLength(char *Base, u32 Length)
{
    string Result;
    Result.Base = Base;
    Result.Length = Length;
    return Result;
}

internal string
StringBaseEnder(char *Base, char Ender)
{
    string Result;
    Result.Base = Base;
    char *C = Base;
    while (*C && *C != Ender)
    {
        C++;
    }
    Result.Length = (u32)(C - Base);
    return Result;
}

internal string
StringFromLiteral(const char *Base)
{
    string Result;
    Result.Base = (char *)Base;
    while (*Base)
        Base++;
    Result.Length = (u32)(Base - Result.Base);
    return Result;
}

internal string
StringFromOffset(string String, u32 Offset)
{
    string Result;
    Result.Base = String.Base + Offset;
    Result.Length = String.Length >= Offset ? String.Length - Offset : 0;
    return Result;
}

internal string
StringPrefixUntil(string String, char Ender)
{
    string Result;
    Result.Base = String.Base;
    u32 CharIndex = 0;
    while (CharIndex < String.Length && String.Base[CharIndex] != Ender)
    {
        CharIndex++;
    }
    Result.Length = CharIndex;
    return Result;
}

internal string
StringSuffixAfter(string String, char Opener)
{
    string Result;
    Result.Base = String.Base + String.Length;
    Result.Length = 0;
    
    u32 CharIndex = 0;
    while (CharIndex < String.Length)
    {
        if (String.Base[CharIndex] == Opener)
        {
            Result.Base = String.Base + CharIndex + 1;
            Assert(CharIndex + 1 <= String.Length);
            Result.Length = String.Length - CharIndex - 1;
            break;
        }
        CharIndex++;
    }
    
    return Result;
}

internal void
AppendStringLiteral(string *Prefix, const char *Literal)
{
    char *C = Prefix->Base + Prefix->Length;
    while (*Literal)
    {
        *C++ = *Literal++;
        Prefix->Length++;
    }
}

internal void
AppendStringLiteralAndNull(string *Prefix, const char *Literal)
{
    char *C = Prefix->Base + Prefix->Length;
    while (*Literal)
    {
        *C++ = *Literal++;
        Prefix->Length++;
    }
    *C = 0;
}


internal void
AppendString(string *Dest, string Source)
{
    char *C = Dest->Base + Dest->Length;
    for (u32 Byte = 0; Byte < Source.Length; Byte++)
        *C++ = Source.Base[Byte];
    Dest->Length += Source.Length;
}

internal u32
TruncateStringUntil(string *S, char NewEnd)
{
    // NOTE(vincent): If the string has positive length, it is guaranteed to be truncated by some amount.
    if (S->Length > 0)
    {
        do
        {
            S->Length--;
        } while(S->Length > 0 && S->Base[S->Length-1] != '/');
    }
    
    return S->Length;  // not zero iff ended at a slash
}

internal u32
StringLength(char *String)
{
    u32 Count = 0;
    while (*String)
    {
        String++;
        Count++;
    }
    return Count;
}


internal b32
StringsAreEqual(string A, const char *B)
{
    u32 Count = 0;
    while (Count < A.Length && *B)
    {
        if (A.Base[Count] != *B)
            break;
        B++;
        Count++;
    }
    return (*B == 0 && Count == A.Length);
}

internal b32
StringsAreEqual(string A, string B)
{
    b32 Result = (A.Length == B.Length);
    if (Result)
    {
        for (u32 Byte = 0; Byte < A.Length; Byte++)
        {
            if (A.Base[Byte] != B.Base[Byte])
            {
                Result = false;
                break;
            }
        }
    }
    return Result;
}
internal b32
StringBeginsWith(string A, const char *B)
{
    u32 Count = 0;
    while (Count < A.Length && *B)
    {
        if (A.Base[Count] != *B)
            break;
        B++;
        Count++;
    }
    return *B == 0;
}

internal b32
IsWhitespace(char C)
{
    b32 Result = false;
    if (C == ' ' || C == '\t' || C == '\r' || C == '\n' || C == '\v' || C == '\f')
        Result = true;
    return Result;
}


internal u32
Minimum(u32 A, u32 B)
{
    if (A < B)
        return A;
    return B;
}

internal void
IntegerToString(u32 Integer, char *Buffer)
{
    // NOTE(vincent): Buffer is assumed to be big enough to contain the integer, 
    // plus a null-terminating character.
    
    // writing the bytes:
    char *C = Buffer;
    
    do {                               // notice how this handles the case Integer == 0
        *C++ = (Integer % 10) + '0';
        Integer /= 10;
    } while (Integer > 0);
    *C = 0;
    
    // reversing the bytes:
    C--;
    while (C > Buffer)
    {
        char Temp = *C;
        *C = *Buffer;
        *Buffer = Temp;
        
        Buffer++;
        C--;
    }
}

inline u32
Sprint(char *Dest, char *Source)
{
    u32 PrintCount = 0;
    while (*Source)
    {
        *Dest++ = *Source++;
        PrintCount++;
    }
    *Dest = 0;
    return PrintCount;
}

inline u32
Sprint(char *Dest, string Source)
{
    char *C = Source.Base;
    for (u32 Count = 0; Count < Source.Length; Count++)
        *Dest++ = *C++;
    *Dest = 0;
    return Source.Length;
}

inline u32
SprintNoNull(char *Dest, char *Source)
{
    u32 PrintCount = 0;
    while (*Source)
    {
        *Dest++ = *Source++;
        PrintCount++;
    }
    return PrintCount;
}

inline u32
SprintBounded(char *Dest, char *Source, u32 MaxBytes)
{
    u32 PrintCount = 0;
    while (*Source && PrintCount < MaxBytes)
    {
        *Dest++ = *Source++;
        PrintCount++;
    }
    return PrintCount;
}

inline u32
SprintInt(char *Dest, int Integer)
{
    // Integer is assumed to be >= 0.
    char *C = Dest;
    
    // print digits in memory order from least significant to most significant:
    do {                               // notice how this handles the case Integer == 0
        *C++ = (Integer % 10) + '0';
        Integer /= 10;
    } while (Integer > 0);
    *C = 0;
    
    u32 DigitCount = (u32)(C - Dest);
    
    // reverse the bytes:
    C--;
    while (C > Dest)
    {
        char Temp = *C;
        *C = *Dest;
        *Dest = Temp;
        
        Dest++;
        C--;
    }
    
    return DigitCount;
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
SprintUntilDelimiter(char *Dest, char *Source, char Delimiter)
{
    u32 Count = 0;
    while (*Source && *Source != Delimiter)
    {
        *Dest++ = *Source++;
        Count++;
    }
    return Count;
}

internal void
PrintString(string S)
{
    char *C = S.Base;
    for (u32 Byte = 0; Byte < S.Length; Byte++)
    {
        putchar(*C);
        C++;
    }
}

internal void 
ReverseBytes(char *Buffer, u32 Size)
{
    for (u32 Index = 0; Index < Size/2; Index++)
    {
        Assert(Size-1-Index < Size);
        Buffer[Index] = Buffer[Size-1-Index];
    }
}

internal u32
ReverseBytesU32(u32 Source)
{
    u32 Result;
    Result = (((Source >> 24) & 0xFF) |
              ((Source >>  8) & 0xFF00) |
              ((Source <<  8) & 0xFF0000) |
              ((Source << 24) & 0xFF000000));
    return Result;
}

internal u32
BinaryToHexadecimal(char *Dest, char *Source, u32 SourceLength)
{
    const char *Hexits = "0123456789ABCDEF";
    
    for (u32 SourceIndex = 0; SourceIndex < SourceLength; SourceIndex++)
    {
        Dest[SourceIndex*3]     = Hexits[Source[SourceIndex] >> 4];
        Dest[(SourceIndex*3)+1] = Hexits[Source[SourceIndex] & 0x0F];
        Dest[(SourceIndex*3)+2] = ' ';
    }
    Dest[SourceLength*3] = 0;
    u32 PrintedCount = SourceLength*3+1;
    return PrintedCount;
}

// get sockaddr, IPv4 or IPv6:
internal void*
GetInternetAddress(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// -------------------------------------------------------------

struct initialize_server_memory_result
{
    u32 ParsingErrorCount;
    char *PortString;
};

#include "file_api.cpp"
