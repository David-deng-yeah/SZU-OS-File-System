#include <fcntl.h>
#include <sys/stat.h>

#define USED -1
#define FREE 0

#define FAT_BLOCK 1
#define DATA_BLOCK 8

const unsigned int BLOCK_NUM = 25600; // 25MB
const unsigned int DATA_NUM = 25600 - DATA_BLOCK;
const unsigned int BLOCK_SIZE = 4096;// 4MB
const unsigned int DISK_SIZE = 104857600; // 100MB

typedef struct{
    char space[4096];
} Block;

typedef struct{
    Block data[25600];// 25600 * Block(4096) = 104857600(100MB)
} Disk;

typedef struct{
    char disk_name[32]; 
    short disk_size;   
    Block *fat_block;   // start of fat block
    Block *data_block;  // start of data block
} BootBlock;

typedef struct{
    char id;
} Fat;