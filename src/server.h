
struct task_with_memory
{
    b32 BeingUsed;
    memory_arena Arena;
    temporary_memory TempMemory;
    u32 Index;
};

struct server_state
{
    memory_arena Arena;
    string ToSend;
    parsed_config_file_result Config;
    
    char *StringOK;
    char *StringBR;
    char *StringNF;
    char *StringUN;
    char *StringFB;
    task_with_memory Tasks[NUMBER_OF_THREADS];
    platform_work_queue *Queue;
    platform_add_entry *PlatformAddEntry;
};

