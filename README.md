# File-System

## 实验目的与要求

综合利用文件管理的相关知识，结合对文件系统的认知，编写简易文件系统，加深操作 系统中文件系统和资源共享的认识。

## 方法、步骤
1. 创建一个 100M 的文件或者创建一个 100M 的共享内存
2. 尝试自行设计一个 C 语言小程序，使用步骤 1 分配的 100M 空间（mmap or 共享
内存），然后假设这 100M 空间为一个空白磁盘，设计一个简单的文件系统管理这
个空白磁盘，给出文件和目录管理的基本数据结构，并画出文件系统基本结构
图，以及基本操作接口。（30 分）
3. 在步骤 1 的基础上实现部分文件操作接口操作，创建目录 mkdir，删除目录
rmdir，修改名称，创建文件 open，修改文件，删除文件 rm，查看文件系统目录
结构 ls。（40 分）注明：全部在内存中实现
4. 参考进程同步的相关章节，通过信号量机制实现多个终端对上述文件系统的互斥
访问，系统中的一个文件允许多个进程读，不允许写操作；或者只允许一个写操
作，不允许读。（30 分）

## 基本介绍
![image](https://user-images.githubusercontent.com/65102150/174225665-88557cdb-2d27-45fb-8aa9-1807eef231e3.png)


定义的常数：

| 名称 | 描述 | 大小 |
| :-----| ----: | :----: |
| BLOCK_NUM | Block的数量 | 25600 |
| DATA_NUM | / | 25600 – DATA_BLOCK |
| BLOCK_SIZE | 每个block的大小 | 4096（4KB） |
| DISK_SIZE | 整个磁盘的大小 | 104857600（100MB） |
| FAT_BLOCK | / | 1 |
| DATA_BLOCK | / | 8 |

定义的数据结构：
* Block：磁盘block，数据颗粒为char
* Disk：磁盘块，数据颗粒为block
* BootBlock：包含磁盘名，磁盘大小，FAT块的起始位置，数据块的起始位置
* FAT：包含一个id
* FCB：包含file名称、拓展名、创建时间、起始盘块号、长度、是目录or文件、目录项是否存在。颗粒大小为11+3+2*6+2+2+1+1 = 32 Bytes
* Datetime：时间数据结构

程序中定义的全局变量：
* Block* disk：硬盘指针
* Void* disk_space：磁盘空间指针（共享内存空间）
* Char* fat：fat表，内存常驻
* Fcb* open_path[16]：深度为16，记录每次打开的目录路径的Fcb
* Char* open_name[16]：深度为16，记录每次打开的目录路径的路径名
* Const int FCB_LIST_LEN：一个Block中可以容纳的Fcb个数（一个Fcb 32Bytes）

初始化磁盘启动块BootBlock：
* 起始位置为disk，BootBlock的大小为32+2+8+8=50Bytes

初始化数据盘块DataBlock：
* 起始位置为：disk+DATA_BLOCK（disk类型为Block*，所以起始位置在第9个Block的位置，一个Block 4KB，所以大概是32KB的位置）

初始化Fat表：
* Fat表的指针为char类型，起始位置是disk+FAT_BLOCK，即4KB的位置从0遍历至BLOCK_NUM，共25KB的空间存放FAT表，文件分配表的示意图如图2所示
![image](https://user-images.githubusercontent.com/65102150/174226320-cf1c7cc1-6d96-4769-8a9b-6da5d327047f.png)

## 磁盘的初始化
### BootBlock的初始化
BootBlock的初始化只需将BootBlock的起始位置、磁盘大小、fat起始位置、dataBlock起始位置指明即可。
### Fat的初始化
Fat负责指明25600个Block的使用情况，其被放置于起始地址+4KB的位置，由于Fat已经占用了第一个Block，所以fat[0]需要设为used，其余block设为free
### DataBlock的初始化
Datablock的初始化主要是设置了第一块FCB：root，并通过initDir函数初始化起始目录（即设置了两个Fcb：“.”和“..”）。其次，设置了当前深度current=0，以及设置了一个open_path的数组，保存“当前目录string”的每一个深度的目录项的Fcb和名字

## Fcb的操作
### 得到一个空闲的Fcb
根据参数给定的当前Fcb，遍历Fcb表，如果有不存在的Fcb的位置，则返回这个Fcb的指针
### 创建一个新的Fcb
通过给定参数中的当前Fcb以及一系列参数，对当前的Fcb进行初始化（命名，设置时间等），然后再通过getFreeBlock函数，根据文件的大小（参数中给定的size），从DataBlock中选择出合适的一段空间，并将这段空间的起始block的number作为当前Fcb的block_number
### 搜索Fcb
根据参数给定的path和Fcb root，通过分词从前往后截取path中的目录项，直到找到path末尾对应的文件的Fcb，并将这个Fcb返回。
### 得到上级的Fcb
通过分词操作去除当前path的末尾，得到上一级的path，然后再通过searchFcb函数得到上级目录项的Fcb并返回。









