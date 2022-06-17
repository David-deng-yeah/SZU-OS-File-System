typedef struct
{
    unsigned short year;
    unsigned short month;
    unsigned short day;
    unsigned short hour;
    unsigned short minute;
    unsigned short second;
} Datetime;

typedef struct
{
    // 32 Bytes
    char name[11];
    char ext[3]; // extension name
    Datetime datetime; // created time
    short block_number; // number of first block
    unsigned short size; // length
    char is_directory; // indicate whether is file or directory
    char is_existed; // indicate catogory item exist or not
} Fcb;