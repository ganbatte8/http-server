#include <stdint.h>
#include <stdio.h>
#include <malloc.h>



#define DEFAULT_SERVER_PORT "80"  // the port users will be connecting to
// TODO(vincent): make sure this works on all platforms. I think there's a string/integer duality


#define internal static

#define Kilobytes(Value) ((Value) * 1000LL)
#define Megabytes(Value) (Kilobytes(Value) * 1000LL)
#define Gigabytes(Value) (Megabytes(Value) * 1000LL)
#define Terabytes(Value) (Gigabytes(Value) * 1000LL)

#if DEBUG
#define Assert(Expression) if (!(Expression)) {*(int *)0 = 0;}
#define InvalidCodePath {*(int *)0 = 0;}
#else
#define Assert(Expression)
#define InvalidCodePath
#endif

#define ArrayCount(Array) sizeof(Array) / sizeof((Array)[0])

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
};

struct string
{
    char *Base;
    u32 Length;
};

inline void
InitializeArena(memory_arena *Arena, u32 Size, void *Base)
{
    Arena->Size = Size;
    Arena->Base = (u8 *)Base;
    Arena->Used = 0;
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


internal void
WriteStringLiteral(char *Buffer, const char *Literal)
{
    // NOTE(vincent): Buffer is assumed to be big enough.
    while (*Literal)
        *Buffer++ = *Literal++;
    *Buffer = 0;
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

internal void 
ReverseBytes(char *Buffer, u32 Size)
{
    for (u32 Index = 0; Index < Size/2; Index++)
    {
        Assert(Size-1-Index < Size);
        Buffer[Index] = Buffer[Size-1-Index];
    }
}

internal char*
BinaryToHexadecimal(char *Source, char *Dest, size_t SourceLength)
{
    // NOTE(vincent): Dest buffer is assumed to have room for SourceLength*3+1 bytes
    const char *Hexits = "0123456789ABCDEF";
    
    for (size_t SourceIndex = 0; SourceIndex < SourceLength; SourceIndex++)
    {
        Dest[SourceIndex*3]     = Hexits[Source[SourceIndex] >> 4];
        Dest[(SourceIndex*3)+1] = Hexits[Source[SourceIndex] & 0x0F];
        Dest[(SourceIndex*3)+2] = ' ';
    }
    Dest[SourceLength*3] = 0;
    return Dest;
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

internal b32
StringsAreEqual(string A, char *B)
{
    u32 Count = 0;
    while (Count < A.Length && *B)
    {
        if (A.Base[Count] != *B)
            return false;
        B++;
        Count++;
    }
    return (*B == 0 && Count == A.Length);
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

#include "file_api.cpp"
