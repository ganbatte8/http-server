// MD5 references:
// https://en.wikipedia.org/wiki/MD5
// https://www.ietf.org/rfc/rfc1321.txt

/*
MD5: variable-length message -> 128bit output.
 The input message is broken up into 512bit blocks.

The message is padded such that its length is a multiple of 512 bits.
The padding works like this: first append a 1 (bit), then append some 0s 
until you are 64 bits away from the end of a 512-bit chunk,
 then fill up the remaining 64 bits representing the length in bits of the original message.

MD5 operates on a 128-bit state divided in four 32-bit parts: A, B, C, D.
These are initialized to certain fixed constants.
 Then MD5 uses each 512-bit message block in turn to modify the state.

All variables are unsigned 32 bit and wrap modulo 2^32 when calculating.
*/

struct md5_result
{
    u32 a;
    u32 b;
    u32 c;
    u32 d;
};

internal md5_result
MD5(u8 *Source, u32 MessageLength)
{
    // NOTE(vincent): MessageLength is in bytes.
    // We assume that the Source buffer is big enough to hold some additional padding at the end.
    // The space required for padding is 1 + 511 + 64 bits = 576 bits = 72 bytes.
    // The padding is not required to be initialized to 0.
    
    Assert(Source[MessageLength + 71] || !Source[MessageLength + 71]);
    
    // We also assume that the message length in bits actually fits in a 32-bit unsigned integer.
    // In other words,
    Assert( (((u32)0xFFFFFFFF) / 8) >= MessageLength);
    
    u32 PerRoundShifts[64] = { 
        7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
        5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
        4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
        6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
    };
    
    u32 K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
        0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
        0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 
    };
    
    md5_result State;
    State.a = 0x67452301; // 01 23 45 67 in memory order (how you read the bytes when address increases)
    State.b = 0xefcdab89; // 89 ab cd ef
    State.c = 0x98badcfe; // fe dc ba 98
    State.d = 0x10325476; // 76 54 32 10
    
    // padding
    Source[MessageLength] = 0x80; // bits: 1000 0000
    u32 ChunkOffset = MessageLength & 63;
    u32 ZeroBytesCount = (ChunkOffset <= 55 ? 55 - ChunkOffset : 63-(ChunkOffset-56));
    for (u32 Byte = 0; Byte < ZeroBytesCount; Byte++)
    {
        Source[MessageLength + Byte + 1] = 0;
    }
    u32 PaddedLength = MessageLength + 1 + ZeroBytesCount + 8;
    Assert(PaddedLength % 64 == 0);
    u32 OriginalSizeInBits = MessageLength * 8;
    u32 *WriteSizePtr = (u32 *)(Source + PaddedLength - 8);
    *WriteSizePtr = OriginalSizeInBits;
    
    
    u32 ChunksCount = PaddedLength / 64;
    
    // Process the message in successive 512-bit chunks:
    for (u32 ChunkIndex = 0; ChunkIndex < ChunksCount; ChunkIndex++)
    {
        // Break chunk into sixteen 32-bit words M[j], 0 ≤ j ≤ 15;
        u32 *M = (u32*)Source + ChunkIndex*16;
        
        u32 A = State.a;
        u32 B = State.b;
        u32 C = State.c;
        u32 D = State.d;
        for (int i = 0; i < 64; i++)
        {
            u32 F,g;
            if (i <= 15)
            {
                F = (B & C) | ((~B) & D);
                g = i;
            }
            else if (i <= 31)
            {
                F = (D & B) | ((~D) & C);
                g = (5*i + 1) & 15;
            }
            else if (i <= 47)
            {
                F = B ^ C ^ D;
                g = (3*i + 5) & 15;
            }
            else
            {
                F = C ^ (B | (~D));
                g = (7*i) & 15;
            }
            F = F + A + K[i] + M[g];
            A = D;
            D = C;
            C = B;
            B = B + ((F << PerRoundShifts[i]) | (F >> (32-PerRoundShifts[i]))); // left rotate
        }
        State.a += A;
        State.b += B;
        State.c += C;
        State.d += D;
    }
    
    return State;
}


internal void
PrintMD5NoNull(char *Dest, md5_result Hash)
{
    char *Source = (char *)&Hash;
    for (u32 ByteIndex = 0; ByteIndex < 16; ByteIndex++)
    {
        char C = Source[ByteIndex];
        
        // Convert C into two hex digits in ascii
        char Low = C & 0x0F;           // That value is in [0,15]
        char High = (C >> 4) & 0x0F;
        
        char LowChar = (Low < 10 ? Low + '0' : Low + 'a' - 10);
        char HighChar = (High < 10 ? High + '0' : High + 'a' - 10);
        
        *Dest++ = HighChar;
        *Dest++ = LowChar;
    }
}

internal void
TestMD5()
{
    char Buffer[1000] = {};
    
    string String[4];
    string ExpectedHash[4];
    md5_result Hash[4];
    b32 Success[4] = {};
    
    String[0]       = StringFromLiteral("user");
    ExpectedHash[0] = StringFromLiteral("ee11cbb19052e40b07aac0ca060c23ee");
    String[1]       = StringFromLiteral("The quick brown fox jumps over the lazy dog");
    ExpectedHash[1] = StringFromLiteral("9e107d9d372bb6826bd81d3542a419d6");
    String[2]       = StringFromLiteral("The quick brown fox jumps over the lazy dog.");
    ExpectedHash[2] = StringFromLiteral("e4d909c290d0fb1ca068ffaddf22cbd0");
    String[3]       = {};
    ExpectedHash[3] = StringFromLiteral("d41d8cd98f00b204e9800998ecf8427e");
    
    for (u32 StringIndex = 0; StringIndex < ArrayCount(String); StringIndex++)
    {
        Sprint(Buffer, String[StringIndex]);
        Hash[StringIndex] = MD5((u8 *)Buffer, String[StringIndex].Length);
        PrintMD5NoNull(Buffer, Hash[StringIndex]);
        string HashString = StringBaseLength(Buffer, 32);
        if (StringsAreEqual(HashString, ExpectedHash[StringIndex]))
            Success[StringIndex] = true;
    }
    
    for (u32 SuccessIndex = 0; SuccessIndex < ArrayCount(Success); SuccessIndex++)
        Assert(Success[SuccessIndex]);
    
}

internal char
SextetToAscii(char Sextet)
{
    // NOTE(vincent): Converts a sextet into an ascii character, following Base64 encoding rules.
    // https://en.wikipedia.org/wikipedia.org/wiki/Base64#Base64_table
    char Output = 0;
    
    if      (Sextet <= 25)  Output = 'A' + Sextet;
    else if (Sextet <= 51)  Output = 'a' + Sextet;
    else if (Sextet <= 61)  Output = '0' + Sextet;
    else if (Sextet == 62)  Output = '+';
    else                    Output = '/';
    
    Assert(Output);
    return Output;
}

internal char
AsciiToSextet(char C)
{
    // NOTE(vincent): Takes an incoming base 64 ascii character and decodes it back.
    // In case of an unexpected input, we make sure the answer is in [0,63].
    char Result;
    if      ('A' <= C && C <= 'Z')   Result = C - 'A';
    else if ('a' <= C && C <= 'z')   Result = C - 'a' + 26;
    else if ('0' <= C && C <= '9')   Result = C - '0' + 52;
    else if (C == '+')               Result = 62;
    else                             Result = 63;
    
    Assert(0 <= Result && Result <= 63);
    return Result;
}



internal void
ToBase64(char *Source, char *Dest, u32 SourceLength)
{
    // NOTE(vincent): This is untested, probably buggy and not actually useful for our use case.
    // We are only using base 64 decoding so far, we don't need to encode in base64.
    // I just wrote this as a way to learn how base 64 works. 
    
    u32 ByteOffset = 0;
    u32 DestOffset = 0;
    while (ByteOffset <= SourceLength-3)
    {
        u8 *Block = (u8 *)Source + ByteOffset;
        u8 Sextets[4]; 
        Sextets[0] = Block[0] >> 2;
        Sextets[1] = ((Block[0] << 4) | (Block[1] >> 4)) & 63;
        Sextets[2] = ((Block[1] << 2) | (Block[2] >> 6)) & 63;
        Sextets[3] = Block[2] & 63;
        
        for (u32 SextetIndex = 0; SextetIndex < 4; SextetIndex++)
        {
            Dest[DestOffset] = SextetToAscii(Sextets[SextetIndex]);
            DestOffset++;
        }
        
        ByteOffset += 3;
    }
    
    if (ByteOffset < SourceLength)
    {
        u8 *Block = (u8 *)Source + ByteOffset;
        
        if (ByteOffset == SourceLength - 2)
        {
            // NOTE(vincent): Read two bytes from the source, get three sextets, add one '=' of padding.
            char Sextets[3];
            Sextets[0] = Block[0] >> 2;
            Sextets[1] = ((Block[0] << 4) | (Block[1] >> 4)) & 63,
            Sextets[2] = (Block[1] << 2) & 63;
            
            for (u32 SextetIndex = 0; SextetIndex < 3; SextetIndex++)
            {
                Dest[DestOffset] = SextetToAscii(Sextets[SextetIndex]);
                DestOffset++;
            }
            Dest[DestOffset] = '=';
            DestOffset++;
        }
        else 
        {
            Assert(ByteOffset == SourceLength - 1);
            // NOTE(vincent): Read one byte from the source, get two sextets, add two '='.
            char Sextets[2];
            Sextets[0] = (Block[0] >> 2),
            Sextets[1] = (Block[0] << 4) & 63;
            
            for (u32 SextetIndex = 0; SextetIndex < 2; SextetIndex++)
            {
                Dest[DestOffset] = SextetToAscii(Sextets[SextetIndex]);
                DestOffset++;
            }
            Dest[DestOffset] = '=';
            DestOffset++;
            Dest[DestOffset] = '=';
            DestOffset++;
        }
        
    }
    
}


internal string
FromBase64(string Source, char *Dest)
{
    string Result;
    Result.Base = Dest;
    Result.Length = 0;
    
    u32 ReadCount = 0;
    while (ReadCount < Source.Length)
    {
        u32 ChunkSize = Minimum(4, Source.Length - ReadCount);
        u8 *Chunk = (u8 *)Source.Base + ReadCount;
        char *DestPtr = Result.Base + Result.Length;
        
        u32 PadFreeSize = 0;
        char Decoded[4];
        while (PadFreeSize < ChunkSize && Chunk[PadFreeSize] != '=')
        {
            Decoded[PadFreeSize] = AsciiToSextet(Chunk[PadFreeSize]);
            PadFreeSize++;
        }
        // '=' padding normally never happens in the first byte of a chunk.
        // However, the program should at least not crash if it is being fed something dumb.
        // Basically, PadFreeSize could be 0 or 1, and at that point it probably doesn't matter
        // what we write to the dest buffer, but we shouldn't crash.
        
        switch (PadFreeSize)
        {
            case 4:
            {
                u8 OutputA = (Decoded[0] << 2) | (Decoded[1] >> 4);
                u8 OutputB = (Decoded[1] << 4) | (Decoded[2] >> 2);
                u8 OutputC = (Decoded[2] << 6) | Decoded[3];
                DestPtr[0] = OutputA;
                DestPtr[1] = OutputB;
                DestPtr[2] = OutputC;
                Result.Length += 3;
            } break;
            case 3:
            {
                u8 OutputA = (Decoded[0] << 2) | (Decoded[1] >> 4);
                u8 OutputB = (Decoded[1] << 4) | (Decoded[2] >> 2);
                DestPtr[0] = OutputA;
                DestPtr[1] = OutputB;
                Result.Length += 2;
            } break;
            case 2:
            {
                u8 OutputA = (Decoded[0] << 2) | (Decoded[1] >> 4);
                DestPtr[0] = OutputA;
                Result.Length += 1;
            } break;
            default:
            {
            }
        }
        ReadCount += ChunkSize;
    }
    
    return Result;
}


internal void
TestFromBase64()
{
    char Dest[1000] = {};
    
    string UserUser = StringFromLiteral("user:user");
    string Encoded = StringFromLiteral("dXNlcjp1c2Vy");
    string Decoded = FromBase64(Encoded, Dest);
    
    Assert(StringsAreEqual(UserUser, Decoded));
    
    string Jojo = StringFromLiteral("jojo no kimyouna bouken");
    string Encoded2 = StringFromLiteral("am9qbyBubyBraW15b3VuYSBib3VrZW4=");
    string Decoded2 = FromBase64(Encoded2, Dest);
    
    Assert(StringsAreEqual(Jojo, Decoded2));
    
    
}