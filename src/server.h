
struct task_with_memory
{
    b32 BeingUsed;
    memory_arena Arena;
    temporary_memory TempMemory;
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
    task_with_memory Tasks[NUMBER_OF_THREADS-1];
    // TODO(vincent): NUMBER_OF_THREADS if it turns out later that the main thread can also do
    // work from the queue?
    platform_work_queue *Queue;
    platform_add_entry *PlatformAddEntry;
};

