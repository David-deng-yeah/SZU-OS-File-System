#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "Disk.h"
#include "File.h"

#define READ_MAX 256

/*disk*/
Block* disk; // point to disk
void* disk_space = (void*)0; // disk space
int shmid; // share memory id
char* fat; // point to fat(file allocation table)

/*path ds*/
Fcb* open_path[16]; // fcb array for currently opened directory
char* open_name[16]; // name for currently opened directory
short current; // depth for cur opened dir
const int FCB_LIST_LEN = sizeof(Block) / sizeof(Fcb); // num of fcb that one block can contain

/*semaphore*/
sem_t *sem_read, *sem_write;

// declaration of some function

/*function for Disk*/
Block* getDisk();
void releaseDisk();
void initDisk();
void initBootBlock();
void initDataBlock();
void initFat();
void initDir(Fcb* fcb, short block_number, short parent_number);

/**/
Block* getBlock(int block_number);
void setCurrentTime(Datetime* datetime);
int getFreeBlock(int size);
int getBlockNum(int size);
Fcb* searchFcb(char* path, Fcb* root);
Fcb* getFreeFcb(Fcb* fcb);
Fcb* initFcb(Fcb* fcb, char* name, char is_dir, int size);
Fcb* getParent(char* path);
char* getPathLastName(char* path);
char* getAbsPath(char* path, char* abs_path);

/*functions for instructions*/
int doMkdir(char* path);
int doRmdir(char* path, Fcb* root);
int doRename(char* src, char* dst);
int doOpen(char* path);
int doWrite(char* path);
int doRm(char* path, Fcb* root);
void doLs();
void doLls();
int doCd(char* path);
int doHelp();

/*some untils*/
void printPathInfo();
char* getArg(char* str);
char* doWhat(char* cmd);
int LoopInput();
int split(char** arr, char* str, const char* delims);

int main()
{
    getDisk();
    initDisk();
    int ret = LoopInput();
    releaseDisk();
    return ret;
}

// deal with input
int LoopInput()
{
    char buffer[64];
    char tmp_buffer[64];
    while (1) {
        printPathInfo();
        doWhat(buffer);
        if (strcmp(buffer, "mkdir") == 0) {
            doMkdir(getArg(buffer));
        } else if (strcmp(buffer, "rmdir") == 0) {
            doRmdir(getArg(buffer), open_path[current]);
        } else if (strcmp(buffer, "rename") == 0) {
            getArg(buffer);
            getArg(tmp_buffer);
            doRename(buffer, tmp_buffer);
        } else if (strcmp(buffer, "open") == 0) {
            doOpen(getArg(buffer));
        } else if (strcmp(buffer, "write") == 0) {
            doWrite(getArg(buffer));
        } else if (strcmp(buffer, "rm") == 0) {
            doRm(getArg(buffer), open_path[current]);
        } else if (strcmp(buffer, "ls") == 0) {
            doLs();
        } else if (strcmp(buffer, "lls") == 0) {
            doLls();
        } else if (strcmp(buffer, "cd") == 0) {
            doCd(getArg(buffer));
        } else if (strcmp(buffer, "exit") == 0) {
            return 0;
        } else if (strcmp(buffer, "help") == 0) {
            doHelp();
        } else if (strlen(buffer) != 0) {
            printf("[LoopInput] Unsupported command\n");
        }
        fflush(stdin);
    }
    return -1;
}

/*initialization*/

// request for disk space, return the pointer point to the disk speace
Block* getDisk()
{
    // request for the share memory
    shmid = shmget((key_t)1127, sizeof(Disk), 0666 | IPC_CREAT);
    if (shmid == -1) {
        fprintf(stderr, "[getDisk] Shmget failed\n");
        exit(EXIT_FAILURE);
    }
    /*attaches the shared memory segment associated with the shared memory identifier, 
    shmid, to the address space of the calling process*/
    disk_space = shmat(shmid, (void*)0, 0);
    if (disk_space == (void*)-1) {
        fprintf(stderr, "[getDisk] Shmat failed\n");
        exit(EXIT_FAILURE);
    }
    disk = (Block*)disk_space;
    return disk;
}

void releaseDisk()
{
    // stop link to share mem
    if (shmdt(disk_space) == -1) {
        fprintf(stderr, "[releaseDisk] Shmdt failed\n");
        exit(EXIT_FAILURE);
    }
    // delete share mem
    if (shmctl(shmid, IPC_RMID, 0) == -1) {
        fprintf(stderr, "[releaseDisk] Shmctl failed\n");
        exit(EXIT_FAILURE);
    }
}

void initDisk()
{
    initBootBlock();
    initFat();
    initDataBlock();
}

void initBootBlock()
{
    BootBlock* boot_block = (BootBlock*)disk;
    strcpy(boot_block->disk_name, "dhw's Disk");
    boot_block->disk_size = sizeof(Block) * BLOCK_NUM;// 100MB
    boot_block->fat_block = disk + FAT_BLOCK;// point to fat
    boot_block->data_block = disk + DATA_BLOCK;// point to data
}

// initialize fat
// fat[BLOCK_num(25600)]
void initFat()
{
    fat = (char*)(disk + FAT_BLOCK);// point to fat

    for (int i = 0; i < FAT_BLOCK; i++) {
        fat[i] = USED; // Full sign
    }
    for (int i = FAT_BLOCK; i < BLOCK_NUM; i++) {
        fat[i] = FREE; // Empty sign
    }
}

// intialize data block
void initDataBlock()
{
    // root point to start of a block
    Fcb* root = (Fcb*)getBlock(DATA_BLOCK);
    initDir(root, DATA_BLOCK, DATA_BLOCK);
    current = 0;
    open_path[current] = root;
    open_name[current] = "Root";
}

// initialize dir
void initDir(Fcb* fcb, short block_number, short parent_number)
{
    // di "."
    // mark this used block 
    fat[block_number] = USED;
    // dir name
    strcpy(fcb->name, ".");
    fcb->is_directory = 1;
    // time
    setCurrentTime(&fcb->datetime);
    // file
    fcb->block_number = block_number;
    fcb->size = 2 * sizeof(Fcb);
    fcb->is_existed = 1;

    // dir ".."
    Fcb* p = fcb + 1;
    memcpy(p, fcb, sizeof(Fcb));
    strcpy(p->name, "..");
    p->block_number = parent_number;
    p->size = -1;

    // initialize other dir
    // 0 for ".", 1 for "..", 2~end for others
    for (int i = 2; i < FCB_LIST_LEN; i++) {
        p++;
        strcpy(p->name, "");
        p->is_existed = 0;
    }
}

/*untils*/

// set datetime by current time
void setCurrentTime(Datetime* datetime)
{
    time_t rawtime;
    time(&rawtime);
    struct tm* time = localtime(&rawtime);
    datetime->year = (time->tm_year + 1900);
    datetime->month = (time->tm_mon + 1);
    datetime->day = time->tm_mday;
    datetime->hour = time->tm_hour;
    datetime->minute = time->tm_min;
    datetime->second = time->tm_sec;
}

// block number --> physical address of block(format as Fcb)
Block* getBlock(int block_number)
{
    return disk + block_number;
}

// get the num of block needed to fill acquired
int getBlockNum(int size)
{
    return (size - 1) / sizeof(Block) + 1;
}

// get gree block, if it equals to acquired size, return the start address "i"
// else return -1
int getFreeBlock(int size)
{
    int count = 0;
    // iterate disk from data_block, pace by block
    for (int i = DATA_BLOCK; i < DATA_NUM; i++) {
        if (fat[i] == FREE) {
            count++;
        } else {
            count = 0;
        }
        if (count == size) {
            for (int j = 0; j < size; j++) {
                fat[i - j] = USED;
            }
            return i;
        }
    }
    return -1;
}

// find FCB
Fcb* searchFcb(char* path, Fcb* root)
{
    char _path[64];
    strcpy(_path, path);
    char* name = strtok(_path, "/");
    char* next = strtok(NULL, "/");
    Fcb* p = root;
    for (int i = 0; i < FCB_LIST_LEN; i++) {
        if (p->is_existed == 1 && strcmp(p->name, name) == 0) {
            if (next == NULL) {
                return p;
            }
            return searchFcb(path + strlen(name) + 1, (Fcb*)getBlock(p->block_number));
        }
        p++;
    }
    return NULL;
}

// require short abspath
char* getAbsPath(char* path, char* abs_path)
{
    char abs_path_arr[16][16];
    int len;
    for (len = 0; len <= current; len++) {
        strcpy(abs_path_arr[len], open_name[len]);
    }

    char _path[64];
    strcpy(_path, path);
    char* name = strtok(_path, "/");
    char* next = name;
    while (next != NULL) {
        name = next;
        next = strtok(NULL, "/");
        if (strcmp(name, ".") == 0) {
            continue;
        } else if (strcmp(name, "..") == 0) {
            len--;
        } else {
            strcpy(abs_path_arr[len++], name);
        }
    }
    char* p = abs_path;
    for (int i = 0; i < len; i++) {
        for (int j = 0; j < strlen(abs_path_arr[i]); j++) {
            *p++ = abs_path_arr[i][j];
        }
        *p++ = '-';
    }
    *(p - 1) = 0;
    return abs_path;
}

// return the first free place of current fcb_table
Fcb* getFreeFcb(Fcb* fcb)
{
    for (int i = 0; i < FCB_LIST_LEN; i++) {
        if (fcb->is_existed == 0) {
            return fcb;
        }
        fcb++;
    }
    return NULL;
}

// create new fcb
Fcb* initFcb(Fcb* fcb, char* name, char is_dir, int size)
{
    // dir name
    strcpy(fcb->name, name);
    fcb->is_directory = is_dir;
    // time
    setCurrentTime(&fcb->datetime);
    // file
    int block_num = getFreeBlock(getBlockNum(size));
    if (block_num == -1) {
        printf("[initFcb] Disk has Fulled\n");
        exit(EXIT_FAILURE);
    }
    fcb->block_number = block_num;
    fcb->size = 0;
    fcb->is_existed = 1;
    return fcb;
}

// return parent dir
Fcb* getParent(char* path)
{
    // get the parent path string in parent_path
    char parent_path[64];
    strcpy(parent_path, path);
    for (int i = strlen(parent_path); i >= 0; i--) {
        if (parent_path[i] == '/') {
            parent_path[i] = 0;
            break;
        }
        parent_path[i] = 0;
    }
    // find parent's FCB
    Fcb* parent;
    if (strlen(parent_path) != 0) {
        Fcb* parent_pcb = searchFcb(parent_path, open_path[current]);
        if (parent_pcb == NULL) {
            return NULL;
        }
        parent = (Fcb*)getBlock(parent_pcb->block_number);
    } else {
        parent = open_path[current];
    }
    return parent;
}

// return the last name of a path
char* getPathLastName(char* path)
{
    char _path[64];
    strcpy(_path, path);
    char* name = strtok(_path, "/");
    char* next = name;
    while (next != NULL) {
        name = next;
        next = strtok(NULL, "/");
    }
    return name;
}

/*functions for filesys operation*/

// mkdir 
int doMkdir(char* path)
{
    // find whether path is existed or not
    Fcb* res = searchFcb(path, open_path[current]);
    if (res) {
        printf("[doMkdir] %s is existed\n", path);
        return -1;
    }
    // find parent fcb
    Fcb* parent = getParent(path);
    if (parent == NULL) {
        printf("[doMkdir] Not found %s\n", path);
        return -1;
    }
    // insert the new dir's fcb into current fcb table
    Fcb* fcb = getFreeFcb(parent);
    char* name = getPathLastName(path);
    initFcb(fcb, name, 1, sizeof(Block));
    fcb->size = sizeof(Fcb) * 2;
    parent->size += sizeof(Fcb);

    // initialize new dir's fcb
    Fcb* new_dir = (Fcb*)getBlock(fcb->block_number);
    initDir(new_dir, fcb->block_number, parent->block_number);
    return 0;
}

// rmdir 
int doRmdir(char* path, Fcb* root)
{
    Fcb* fcb = searchFcb(path, root);
    if (fcb && fcb->is_directory == 1) {
        if (strcmp(fcb->name, ".") == 0 || strcmp(fcb->name, "..") == 0) {
            printf("[doRmdir] You can't delete %s\n", fcb->name);
            return -1;
        }
        // rm file&dir recrusively
        Fcb* p = (Fcb*)getBlock(fcb->block_number) + 2;// p point to the 3rd fcb
        for (int i = 2; i < FCB_LIST_LEN; i++) {
            if (p->is_existed == 0) {
                continue;
            } else if (p->is_directory) {
                doRmdir(p->name, p);
            } else {
                doRm(p->name, p);
            }
            p++;
        }
        // release fat mark
        for (int i = 0; i < getBlockNum(fcb->size); i++) {
            fat[fcb->block_number + i] = FREE;
        }
        fcb->is_existed = 0;
        // alter the size recorded in root
        root->size -= sizeof(Fcb);
    } else {
        printf("[doRmdir] Not found %s\n", path);
        return -1;
    }
    return 0;
}

// rename
int doRename(char* src, char* dst)
{
    Fcb* fcb = searchFcb(src, open_path[current]);
    if (fcb) {
        if (strcmp(fcb->name, ".") == 0 || strcmp(fcb->name, "..") == 0) {
            printf("[doRename] You can't rename %s\n", src);
            return -1;
        }
        strcpy(fcb->name, dst);
    } else {
        printf("[doRename] Not found %s\n", src);
        return -1;
    }
    return 0;
}

int doOpen(char* path)
{
    Fcb* fcb = searchFcb(path, open_path[current]);
    if (fcb) {
        if (fcb->is_directory != 0) {
            printf("[doOpen] %s is not readable file\n", fcb->name);
            return -1;
        }
        char mutex_name[256];
        getAbsPath(path, mutex_name);
        char* suffix = mutex_name + strlen(mutex_name);
        
        strcpy(suffix, "-write");
        sem_write = sem_open(mutex_name, O_CREAT, 0666, 1);
        int sval;
        sem_getvalue(sem_write, &sval);
        if (sval < 1) {
            printf("[doOpen] %s is busy\n", fcb->name);
            return -1;
        }
        
        strcpy(suffix, "-read");
        sem_read = sem_open(mutex_name, O_CREAT, 0666, READ_MAX);
        sem_wait(sem_read);

        char* p = (char*)getBlock(fcb->block_number);
        for (int i = 0; i < fcb->size; i++) {
            printf("%c", *p);
            p++;
        }
        printf("\n");
        getchar();
        printf("[doOpen] Input any key to return...");
        getchar();

        sem_post(sem_read);
    } else {
        Fcb* parent = getParent(path);
        if (parent == NULL) {
            printf("[doOpen] Not found %s\n", path);
            return -1;
        }
        Fcb* fcb = getFreeFcb(parent);
        char* name = getPathLastName(path);
        initFcb(fcb, name, 0, sizeof(Block));
        fcb->size = 0;
        parent->size += sizeof(Fcb);
    }
    return 0;
}

// write file
int doWrite(char* path)
{
    Fcb* fcb = searchFcb(path, open_path[current]);
    if (fcb) {
        if (fcb->is_directory != 0) {
            printf("[doWrite] %s is not writable file\n", fcb->name);
            return -1;
        }
        
        char mutex_name[256];
        getAbsPath(path, mutex_name);
        char* suffix = mutex_name + strlen(mutex_name);
        
        strcpy(suffix, "-read");
        sem_read = sem_open(mutex_name, O_CREAT, 0666, READ_MAX);
        int sval;
        sem_getvalue(sem_read, &sval);
        if (sval < READ_MAX) {
            printf("[doWrite] %s is busy\n", fcb->name);
            return -1;
        }

        strcpy(suffix, "-write");
        sem_write = sem_open(mutex_name, O_CREAT, 0666, 1);
        
    sem_getvalue(sem_write, &sval);
    if (sval < 1) {
        printf("[doWrite] Waiting for idle...\n");
    }
    sem_wait(sem_write);
    printf("[doWrite] You can write now\n");

        char* head = (char*)(disk + fcb->block_number);
        char* p = head;
        getchar();
        while ((*p = getchar()) != 27 && *p != EOF) {
            p++;
        }
        *p = 0;
        fcb->size = strlen(head);
        
        sem_post(sem_write);
    } else {
        printf("[doWrite] Not found %s\n", path);
    }
    return 0;
}

// rm file
int doRm(char* path, Fcb* root)
{
    Fcb* fcb = searchFcb(path, root);
    if (fcb) {
        if (fcb->is_directory != 0) {
            printf("[doRm] %s is not file\n", fcb->name);
            return -1;
        }
        // release fat mark
        for (int i = 0; i < getBlockNum(fcb->size); i++) {
            fat[fcb->block_number + i] = FREE;
        }
        fcb->is_existed = 0;
        // alter size recorded in parent 
        getParent(path)->size -= sizeof(Fcb);
    } else {
        printf("[doRm] Not found %s\n", path);
        return -1;
    }
    return 0;
}

// ls
void doLs()
{
    Fcb* fcb = open_path[current];
    int num = open_path[current]->size / sizeof(Fcb);
    for (int i = 0; i < FCB_LIST_LEN; i++) {
        if (fcb->is_existed) {
            printf("%s\t", fcb->name);
        }
        fcb++;
    }
    printf("\n");
}

// ls -l
void doLls()
{
    Fcb* fcb = open_path[current];
    int num = open_path[current]->size / sizeof(Fcb);
    for (int i = 0; i < 2; i++) {
        if (fcb->is_existed) {
            printf("%s\n", fcb->name);
        }
        fcb++;
    }
    for (int i = 2; i < FCB_LIST_LEN; i++) {
        if (fcb->is_existed) {
            printf("%hu-%hu-%hu %hu:%hu:%hu\t", fcb->datetime.year, fcb->datetime.month, fcb->datetime.day, fcb->datetime.hour, fcb->datetime.minute, fcb->datetime.second);
            printf("Block %hd  \t", fcb->block_number);
            printf("%hu B\t", fcb->size);
            printf("%s\t", fcb->is_directory ? "Dir" : "File");
            printf("%s\n", fcb->name);
        }
        fcb++;
    }
}

// cd跳转指令
int doCd(char* path)
{
    char* names[16];
    int len = split(names, path, "/");
    for (int i = 0; i < len; i++) {
        if (strcmp(names[i], ".") == 0) {
            continue;
        }
        if (strcmp(names[i], "..") == 0) {
            if (current == 0) {
                printf("[doCd] Depth of the directory has reached the lower limit\n");
                return -1;
            }
            current--;
            continue;
        }
        if (current == 15) {
            printf("[doCd] Depth of the directory has reached the upper limit\n");
            return -1;
        }
        // find whether this dir exist or not
        Fcb* fcb = searchFcb(names[i], open_path[current]);
        if (fcb) {
            if (fcb->is_directory != 1) {
                printf("[doCd] %s is not directory\n", names[i]);
                return -1;
            }
            current++;
            open_name[current] = fcb->name;
            open_path[current] = (Fcb*)getBlock(fcb->block_number);
        } else {
            printf("[doCd] %s is not existed\n", names[i]);
            return -1;
        }
    }
    return 0;
}

int doHelp()
{
    printf("\n");
    printf("help\toutput help\n");
    printf("ls\toutput dir information\n");
    printf("lls\toutput dir in long format\n");
    printf("cd\tmove to other path \n");
    printf("mkdir\tcreate dir\n");
    printf("rmdir\trm dir recursively\n");
    printf("open\topen file, if it not exit, create a new one\n");
    printf("rm\trm file\n");
    printf("rename\talter name\n");
    printf("exit\texit file-sys\n");
    printf("\n");
}

// print current path indicator
void printPathInfo()
{
    printf("DHW@FileSystem:");
    for (int i = 0; i <= current; i++) {
        printf("/%s", open_name[i]);
    }
    printf("> ");
}

/*trival utils*/

// split string by delims into several pieces in arr and return count
int split(char** arr, char* str, const char* delims)
{
    int count = 0;
    char _str[64];
    strcpy(_str, str);
    char* s = strtok(_str, delims);
    while (s != NULL) {
        count++;
        *arr++ = s;
        s = strtok(NULL, delims);
    }
    return count;
}

// receive parameters
char* getArg(char* str)
{
    scanf("%s", str);
    return str;
}

// receive instructions
char* doWhat(char* cmd)
{
    scanf("%s", cmd);
    return cmd;
}
