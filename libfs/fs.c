#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "disk.h"
#include "fs.h"


#define FAT_EOC 0xFFFF
#define Root_Dir_Entries 128

struct __attribute__((__packed__)) superblock {
  char signature[8];
  uint16_t numTotalBlocks;
  uint16_t rootDirIndex;
  uint16_t dataBlockIndex;
  uint16_t numDataBlocks;
  uint8_t numFATBlocks;
  uint8_t padding[4079];
};

struct __attribute__((__packed__)) FAT {
  uint16_t entries[2048];
};

struct __attribute__((__packed__)) rootDir {
  char fileName[16];
  uint32_t fileSize;
  uint16_t firstBlockIndex;  //first index of block OR entry in the FAT table (same thing)
  uint8_t padding2[10];
};

struct __attribute__((__packed__)) file {
  char fileName[FS_FILENAME_LEN];
  int fd;
  size_t offset;
};

struct __attribute__((__packed__)) files {
  struct file Files[FS_OPEN_MAX_COUNT];
};

/* TODO: Phase 1 */

struct superblock sb;
struct FAT *FATarray;
struct rootDir rd[128];
struct file openFiles[FS_FILE_MAX_COUNT];

void printFAT(){
  printf("printing FAT\n");
   int freeFAT = 0;
  for (int i = 0; i < sb.numFATBlocks; i++) {
    for (int j = 0; j < 2048; j++) { 
       if (FATarray[i].entries[j] == 0) {
        freeFAT++;
      }
      else if(FATarray[i].entries[j] == FAT_EOC){
        printf("FATarray at entry %d (block %d index %d) is end of file (or has smaller things in it)\n",(i*2048)+j, i,j);
      }
      else{
        printf("Stuff in datablockindex %d! next block that has this files data is %d\n",(i*2048)+j,FATarray[i].entries[j]);
      }
    }
  }
  printf("\n");
}

void printRoot(){
  printf("printing root directory\n");
  for(int i=0;i<Root_Dir_Entries;i++){
    if(rd[i].fileName[0]!='\0'){
      printf("root dir index: %d\n",i);
      printf(" filename: %s",rd[i].fileName);
       printf(" file size: %d",rd[i].fileSize);
        printf(" first block index: %d\n",rd[i].firstBlockIndex);

    }
  }
  printf("\n");
}

void printOpenFiles(){
  printf("printing all open files\n");
  for(int i=0;i<FS_OPEN_MAX_COUNT;i++){
    if(openFiles[i].fileName[0]!='\0'){
      printf("file in openfiles %d: %s\n", i, openFiles[i].fileName);
       printf("fd in openfiles %d: %d\n", i, openFiles[i].fd);
       printf("offset in openfiles %d: %ld\n", i, openFiles[i].offset);
      
    }
  }
  printf("\n");
}

void clearFAT(){
  for(int i=0;i < sb.numFATBlocks;i++){
    for(int j=0;j<2048;j++){
      if(i==0 && j==0){
        FATarray[i].entries[j]=FAT_EOC;
      }
      else{
         FATarray[i].entries[j]=0;
      }
    }
  }
}

void clearOpen(){
  for(int i=0;i<FS_OPEN_MAX_COUNT;i++){
    openFiles[i].fd=-1;
    openFiles[i].offset=0;
    strcpy(openFiles[i].fileName, "\0");
  }
}

void clearRoot(){
  for(int i=0;i<128;i++){
    strcpy(rd[i].fileName, "\0");
    rd[i].fileSize=0;
    rd[i].firstBlockIndex=0;
  }
}


int fs_mount(const char *diskname) {
 // printf("mount is called\n");
  if (block_disk_open(diskname) == -1) {
    return -1;
  }
  if (block_read(0, &sb) == -1) {
    return -1;
  }
  FATarray = (struct FAT *)malloc(sizeof(struct FAT) * sb.numFATBlocks);
  for (int i = 1; i < sb.numFATBlocks + 1; i++) {
    if (block_read(i, &FATarray[i - 1]) == -1) {
      return -1;
    }
  }
  if (block_read(sb.rootDirIndex, &rd) == -1) {
    return -1;
  }

  //does it keep the open file fd when you unmount it and mount it again?
  for(int openFileIterator=0;openFileIterator<FS_OPEN_MAX_COUNT;openFileIterator++){
    openFiles[openFileIterator].fd=-1;
    openFiles[openFileIterator].offset=0;
  }
  return 0;
  /* TODO: Phase 1 */
}

int fs_umount(void) {
 // printf("unmount is called\n");
  if (block_write(0, &sb) == -1) {
    return -1;
  }
  for (int i = 1; i < sb.numFATBlocks + 1; i++) {
    if (block_write(i, &FATarray[i - 1]) == -1) {
      return -1;
    }
  }
  if (block_write(sb.rootDirIndex, &rd) == -1) {
    return -1;
  }
  if (block_disk_close() == -1) {
    return -1;
  }
  // for(int openFileIterator=0;openFileIterator<FS_OPEN_MAX_COUNT;openFileIterator++){
  //   close(openFiles[openFileIterator].fd);
  //   openFiles[openFileIterator].fd=-1;
  //   openFiles[openFileIterator].offset=0;
  // }

  return 0;
  /* TODO: Phase 1 */
}

int fs_info(void) {
  // printFAT();
  // printOpenFiles();
  // printRoot();
  // Printing info into terminal
  printf("FS Info:\n");
  printf("total_blk_count=%d\n", sb.numTotalBlocks);
  printf("fat_blk_count=%d\n", sb.numFATBlocks);
  printf("rdir_blk=%d\n", sb.rootDirIndex);
  printf("data_blk=%d\n", sb.dataBlockIndex);
  printf("data_blk_count=%d\n", sb.numDataBlocks);

  uint16_t DATABlocks = sb.numDataBlocks;
  uint16_t FATblocksize = 2048;
  if(DATABlocks<BLOCK_SIZE){
    FATblocksize = sb.numDataBlocks;
  }

  int freeFAT = 0;
  for (int i = 0; i < sb.numFATBlocks; i++) {
    for (int j = 0; j < FATblocksize; j++) { 
      if(i==0 && j==0){
        continue;
      //  printf("FATarray at entry 0 is always EOC\n");
      }
      else if (FATarray[i].entries[j] == 0) {
        freeFAT++;
      }
      // else if(FATarray[i].entries[j] == FAT_EOC){
      //   printf("FATarray at entry %d is end of file (or has smaller things in it)\n",(i*2048)+j);
      // }
      // else{
      //   printf("Stuff in datablockindex %d! next block that has this files data is %d\n",(i*2048)+j,FATarray[i].entries[j]);
      // }
    }
  }
  int freeRoot = 0;
  for (int k = 0; k < FS_FILE_MAX_COUNT; k++) {
    if (rd[k].fileName[0] == '\0') {
      freeRoot++;
    }
    // else{
    //   printf("rd[%d] is file '%s' and starts at datablockindex(fatblockindex) %d\n", k,rd[k].fileName, rd[k].firstBlockIndex);
    // }
  }
  printf("fat_free_ratio=%d/%d\n", freeFAT, sb.numDataBlocks);
  printf("rdir_free_ratio=%d/%d\n", freeRoot, 128);
  /* TODO: Phase 1 */
  return 0;
  /* TODO: Phase 1 */
}

int fs_create(const char *filename) {
 // printf("called create\n");

  if (filename == NULL || strlen(filename) >= FS_FILENAME_LEN) {
  //  printf("broke\n");
    return -1;
  }
  if (strchr(filename, '\0') == NULL) {
   //    printf("broke\n");
        return -1;
    }
  int find =0;
  for (find = 0; find < FS_FILE_MAX_COUNT; find++) {
    if (strcmp(filename, rd[find].fileName) == 0) {
  //    printf("file is already here\n");
      return -1;
    }
  }

  int emptySpot = -1;
  for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
    if (rd[i].fileName[0]=='\0') { // rootDirBlock[i].fileName[0]=='\0'
      emptySpot = i;
      break;
    }
  }

  // Check if the directory is full
  if (emptySpot == -1) {
    return -1;
  }

  // Add the new file
  strcpy(rd[emptySpot].fileName, filename);
  rd[emptySpot].fileSize = 0;
  rd[emptySpot].firstBlockIndex = FAT_EOC;
 
 // printRoot();
  return 0;
}

int fs_delete(const char *filename) {
 // printf("delete is called\n");
  // printOpenFiles();
  // printFAT();
  // printRoot();
  // Look for file
  int fileIndex = -1;
  for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
    if (strcmp(rd[i].fileName, filename) ==
        0) { // rootDirBlock[i].fileName[0]=='\0'
      fileIndex = i;
      break;
    }
  }
  // error check if we found the file
  if (fileIndex > FS_FILE_MAX_COUNT || fileIndex < 0) {
    return -1;
  }
  
  //check is the files is opened
  // for(int opened=0;opened<FS_OPEN_MAX_COUNT;opened++){
  //   if(strcmp(openFiles[opened].fileName, filename)){
  //     return -1;
  //   }
  // }

  //reset file-chain in fat 
  int block_in_fat = rd[fileIndex].firstBlockIndex/2048;
  int entry_no = rd[fileIndex].firstBlockIndex%2048;
  while(FATarray[block_in_fat].entries[entry_no]!=FAT_EOC){
    if(FATarray[block_in_fat].entries[entry_no]==0){
 //     printf("this entry is free but it suppose to have a file occupy it\n");
      return -1;
    }
    else{
      int next_block_in_fat = FATarray[block_in_fat].entries[entry_no]/2048;
      int next_entry_no = FATarray[block_in_fat].entries[entry_no]%2048;
      FATarray[block_in_fat].entries[entry_no] = 0;
      block_in_fat = next_block_in_fat;
      entry_no = next_entry_no;
    }
  }
  FATarray[block_in_fat].entries[entry_no]=0;

  // reset rootDirBlock[em]
  strcpy(rd[fileIndex].fileName, "\0");
  rd[fileIndex].fileSize = 0;
  rd[fileIndex].firstBlockIndex = FAT_EOC;
  // printFAT();
  // printRoot();
  // printOpenFiles();


  
  
  //this is temporary

  /* TODO: Phase 2 */
  return 0;
}

int fs_ls(void) {
  printf("FS Ls:\n");
  for (int currentFile = 0; currentFile < Root_Dir_Entries; currentFile++) {
    if (rd[currentFile].fileName[0] == '\0') {
      break;
    } else {
      printf("file: %s, size: %d, data_blk: %d\n", rd[currentFile].fileName,
             rd[currentFile].fileSize, rd[currentFile].firstBlockIndex);
    }
  }
  /* TODO: Phase 2 */
  return 0;
}

int fs_open(const char *filename) {  
 //  printf("open is called\n");
  /* TODO: Phase 3 */
  // First check if the file is mounted
  // Check if the file is NULL
  if (filename == NULL || strlen(filename)>FS_FILENAME_LEN) {
    return -1;
  }
  int found=-1;
  //checks if the filename is in the root directory
  for(int checkrd=0;checkrd<FS_FILE_MAX_COUNT;checkrd++){
    if(strcmp(rd[checkrd].fileName, filename)==0){
      found=1;
      break;
    }
  }
  //return -1 if the filename is not in the root directory
  if(found==-1){
 //   printf("broke\n");
    return -1;
  }
  int fd = open(filename, O_RDWR | O_CREAT, 0644);
  if(fd<0){
 //   printf("broke2\n");
    return -1;
  }

  //checks if the file is already opened or if there's a space to open it
  for(int openFileIndex=0;openFileIndex<FS_OPEN_MAX_COUNT;openFileIndex++){
    //checks if it is already opened, might need to get rid of
    if(strcmp(openFiles[openFileIndex].fileName, filename)==0){
  //    printf("file is opened. switching fd\n");
      dup2(openFiles[openFileIndex].fd, fd);
      openFiles[openFileIndex].fd=fd;
      return fd;
    }
    //checks if an entry is empty, and we can occupy (open) the file
    else if(openFiles[openFileIndex].fd==-1){
      //occupies the open file entry with the fd, offset, and filename
      openFiles[openFileIndex].fd = fd;
      openFiles[openFileIndex].offset=0;
      strcpy(openFiles[openFileIndex].fileName,filename);

      if(openFiles[openFileIndex].fd<0){
 //       printf("problem openening the file\n");
        return -1;
      }
  //    printf("found empty space\n");
      dup2(openFiles[openFileIndex].fd, fd);
      openFiles[openFileIndex].fd=fd;
  //    printf("new fd: %d for :%s\n", openFiles[openFileIndex].fd, openFiles[openFileIndex].fileName);
      return fd;
    }
  }

  //if no spaces are open in the array
//  printf("no spaces left\n");
  return -1;
}

int fs_close(int fd) {
 // printf("closed is called\n");
  int fdFind = 0;
  int found = 0;
  //checks if the file is opened
  for (fdFind = 0; fdFind < FS_FILE_MAX_COUNT; fdFind++) {
    if (openFiles[fdFind].fd == fd) {
      found = 1;
      break;
    }
  }
  //if the file is not even opened
  if (found != 1) {
    return -1;
  }
  //find this file in the directory, get the startingdatablockindex, which is the same as fatIndex where the file starts
  // int rdFind=0;
  // for(rdFind=0;rdFind<Root_Dir_Entries;rdFind++){
  //   if( strcmp(rd[rdFind].fileName,openFiles.Files[fdFind].fileName)){
  //     break;
  //   }
  // }

  // uint16_t firstDataBlockIndex= rd[rdFind].firstBlockIndex;
  // uint16_t FATEntryBlock = firstDataBlockIndex/2048;
  // uint16_t FATEntry = firstDataBlockIndex%2048;

  // //clear space in the fat
  // while(FATarray[FATEntryBlock].entries[FATEntry]!=FAT_EOC){

  // }

  //closes the file and restores the entry in openFiles so that another file can be opened 
  
  close(openFiles[fdFind].fd);
  openFiles[fdFind].fd = -1;
  openFiles[fdFind].offset = 0;
  strcpy(openFiles[fdFind].fileName, "\0");
  /* TODO: Phase 3 */
  return 0;
}

int fs_stat(int fd) {
  //printf("stat called\n");
  char nameOfFile[FS_FILENAME_LEN];
  int openIndex = 0;
  int found = 0;
 // struct stat fileStat;
  for (openIndex = 0; openIndex < FS_OPEN_MAX_COUNT; openIndex++) {
    if (openFiles[openIndex].fd == fd) {
      strcpy(nameOfFile, openFiles[openIndex].fileName);
    //  printf("file name to stat: %s\n",nameOfFile);
      found = 1;
      break;
    }
  }
  // if the file is not open
  if (found == 0) {
    return -1;
  }
  for(int i=0;i<Root_Dir_Entries;i++){
    if(strcmp(rd[i].fileName, nameOfFile)==0){
    //  printf("file size: %d\n", rd[i].fileSize);
      return rd[i].fileSize;
    }
  }
  return -1;
  //stat(nameOfFile, &fileStat);
  //return fileStat.st_size;
  /* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset) {
  int openIndex = 0;
  int found = 0;
  for (openIndex = 0; openIndex < FS_OPEN_MAX_COUNT; openIndex++) {
    //if file is opened
    if (openFiles[openIndex].fd == fd) {
      int fileSize = fs_stat(fd);
      if (fileSize < 0) {
                return -1; // File not found or another error
            }
            // Ensure offset does not exceed file size
            // Here we assume offset can safely be compared to fileSize
      if (offset > (size_t)fileSize) {
                return -1;
            }
      openFiles[openIndex].offset = offset;
      found = 1;
      break;
    }
  }
  // if the file is not open
  if (found == 0) {
    return -1;
  }
  /* TODO: Phase 3 */
  return 0;
}

//from the firstblock index, it is the same as the entry point to the fat block
uint16_t fs_block_index_from_current_offset(uint16_t startBlock, size_t currFileOffset){

  //calculate how many entries to traverse the fat array from the starting block
  uint16_t count = currFileOffset/BLOCK_SIZE;
  uint16_t returnIndex = startBlock;
  
  for(int i=0;i<count;i++){
    uint16_t nextBlock = returnIndex/2048;
    uint16_t nextIndex = returnIndex%2048;
    returnIndex = FATarray[nextBlock].entries[nextIndex];
  }
  return returnIndex;
}

int add_FAT_Entry(uint16_t FATBlock, uint16_t FATEntry){
  for(int i=0;i<sb.numFATBlocks;i++){
    for(int j=0;j<2048;j++){
      if(j==0 && i==0){
        continue;
      }
      if(FATarray[i].entries[j]==0){
        FATarray[FATBlock].entries[FATEntry]= FATarray[i].entries[j];
        FATarray[i].entries[j]=FAT_EOC;
        return 1;
      }
    }
  }
  return -1;
}

uint16_t find_empty_FAT(){
   for(int i=0;i<sb.numFATBlocks;i++){
    for(int j=0;j<2048;j++){
      if(j==0 && i==0){
        continue;
      }
      if(FATarray[i].entries[j]==0){
        return (i*2048 + j);
      }
    }
  }
  return 0;
}

int fs_write(int fd, void *buf, size_t count) {
 // printf("write called\n");
  //printf("called write function\n");
  // printFAT();
  // printOpenFiles();
  // printRoot();
   if(count==0){
    return 0;
  }
  if (buf == NULL) {
    return -1;
  }
  //check if the file is open
  int found=-1;
  int openIndex =0;
  char fileName[FS_FILENAME_LEN];
  for(openIndex=0;openIndex<FS_OPEN_MAX_COUNT;openIndex++){
    if(fd==openFiles[openIndex].fd){
      strcpy(fileName, openFiles[openIndex].fileName);
   //   printf("file: %s is in openfiles\n", openFiles[openIndex].fileName);
      found=1;
      break;
    }
  }
  if(found==-1){
    return -1;
  }
  //find file in root directory
  int fileInDirectory=0;
  int found2=-1;
  for( fileInDirectory=0;fileInDirectory<Root_Dir_Entries;fileInDirectory++){
    if(strcmp(rd[fileInDirectory].fileName,fileName)==0){
   //   printf("file is open at rootdirectory index %d\n", fileInDirectory);
      found2=1;
      break;
    }
  }

    if(rd[fileInDirectory].firstBlockIndex == FAT_EOC){
      rd[fileInDirectory].firstBlockIndex = find_empty_FAT();
      if(rd[fileInDirectory].firstBlockIndex==0){
     //   printf("ran out of space in fat\n");
        return 0;
      }
      uint16_t FATBlock2 = rd[fileInDirectory].firstBlockIndex/2048;
     uint16_t FATIndex2 = rd[fileInDirectory].firstBlockIndex%2048;
     FATarray[FATBlock2].entries[FATIndex2]=FAT_EOC;
    }
  //   printFAT();
  // printOpenFiles();
  // printRoot();
  //error check 
  if(found2==-1){
    return -1;
  }
 

  //function that finds the end block index of the file
  size_t bytesWrite=0;
  uint8_t bounceBuffer[BLOCK_SIZE];

  while(bytesWrite<count){
    if(rd[fileInDirectory].firstBlockIndex == FAT_EOC){
      rd[fileInDirectory].firstBlockIndex = find_empty_FAT();
      if(rd[fileInDirectory].firstBlockIndex==0){
    //    printf("ran out of space in fat\n");
        return 0;
      }
      uint16_t FATBlock2 = rd[fileInDirectory].firstBlockIndex/2048;
     uint16_t FATIndex2 = rd[fileInDirectory].firstBlockIndex%2048;
     FATarray[FATBlock2].entries[FATIndex2]=FAT_EOC;
    }
     uint16_t currBlock=fs_block_index_from_current_offset(rd[fileInDirectory].firstBlockIndex, openFiles[openIndex].offset);
     uint16_t FATBlock = currBlock/2048;
     uint16_t FATIndex = currBlock%2048;
   
    // printf("current block: %d\n",rd[fileInDirectory].firstBlockIndex);
    //  printf("current offset: %ld\n", openFiles[openIndex].offset);
    //  printf("current datablock for file: %d\n", currBlock);
     uint16_t currentOffset = openFiles[openIndex].offset % BLOCK_SIZE;
      if(block_read(sb.dataBlockIndex+currBlock,&bounceBuffer)==-1){
   //   printf("could not read\n");
      return -1;
    }
   // printf("block to write/modify \n %s \n", bounceBuffer);
    
    uint16_t bytesLeftInBlock = BLOCK_SIZE-currentOffset;
    uint16_t bytesToCopy = (bytesLeftInBlock < count - bytesWrite) ? bytesLeftInBlock : count - bytesWrite;

    memcpy(bounceBuffer+currentOffset, buf+bytesWrite,bytesToCopy);
    bytesWrite+=bytesToCopy;


    if(block_write(currBlock+sb.dataBlockIndex, &bounceBuffer)==-1){
   //   printf("could not write\n");
      return -1;
    }
    openFiles[openIndex].offset+=bytesToCopy;
    rd[fileInDirectory].fileSize+=bytesToCopy;

     if(FATarray[FATBlock].entries[FATIndex]==FAT_EOC && count-bytesWrite>bytesLeftInBlock){
      int nextBlock = add_FAT_Entry(FATBlock,FATIndex);
      if(nextBlock==-1){
        return bytesWrite;
      }
   }
  }
  // printFAT();
  // printOpenFiles();
  // printRoot();
  return bytesWrite;

  /* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count) {
 // printf("read called\n");
  // printFAT();
  // printRoot();
  // printOpenFiles();
  
  
  //find file in open files
  if (buf == NULL) {
    return -1;
  }
  if(count==0){
    return 0;
  }
  //check if the file is open
  int found=-1;
  int openIndex =0;
  char fileName[FS_FILENAME_LEN];
  for(openIndex=0;openIndex<FS_OPEN_MAX_COUNT;openIndex++){
    if(fd==openFiles[openIndex].fd){
      strcpy(fileName, openFiles[openIndex].fileName);
     // printf("file: %s is in openfiles\n", openFiles[openIndex].fileName);
      found=1;
      break;
    }
  }
  if(found==-1){
    return -1;
  }
 // printf("filename to copy: %s\n", fileName);
  //find file in root directory
  int fileInDirectory=0;
  int found2=-1;
  for( fileInDirectory=0;fileInDirectory<Root_Dir_Entries;fileInDirectory++){
    if(strcmp(rd[fileInDirectory].fileName,fileName)==0){
    //  printf("file is open at rootdirectory index %d\n", fileInDirectory);
      found2=1;
      break;
    }
  }
  //error check 
  if(found2==-1){
    return -1;
  }
  if(count==0){
    return 0;
  }
 // printf("first data block index for file: %d\n", rd[fileInDirectory].firstBlockIndex);
  
  size_t bytesRead=0;
  uint8_t bounceBuffer[BLOCK_SIZE];
  while(bytesRead<count){
   uint16_t currentDataBlockIndex = fs_block_index_from_current_offset(rd[fileInDirectory].firstBlockIndex, openFiles[openIndex].offset);

   uint16_t FATBlock = currentDataBlockIndex/2048;
   uint16_t FATIndex = currentDataBlockIndex%2048;
  
  // printf("current data block index %d\n",currentDataBlockIndex);
    uint16_t currentOffset = openFiles[openIndex].offset % BLOCK_SIZE;
   // printf("current offset %d\n",currentOffset);
    if(block_read(sb.dataBlockIndex+currentDataBlockIndex,&bounceBuffer)==-1){
    //  printf("could not read\n");
      return -1;
    }
   // printf("block read: \n %s \n", bounceBuffer);
    
    
    uint16_t bytesLeftInBlock = BLOCK_SIZE-currentOffset;
    uint16_t bytesToCopy = (bytesLeftInBlock < count - bytesRead) ? bytesLeftInBlock : count - bytesRead;
   
    memcpy(buf+bytesRead, bounceBuffer+currentOffset,bytesToCopy);
    bytesRead+=bytesToCopy;
    openFiles[openIndex].offset+=bytesToCopy;


   // uint16_t nextDataBlockIndex = fs_block_index_from_current_offset(rd//[fileInDirectory].firstBlockIndex, openFiles[openIndex].offset);
    // printf("nextDataBlockIndex: %d\n", nextDataBlockIndex);
    // printf("next offset: %ld\n",openFiles[openIndex].offset);

    if(FATarray[FATBlock].entries[FATIndex]==FAT_EOC){
      break;
   }

  }


  // while(bytesRead<count){
  //   uint16_t currentDataBlockIndex = fs_block_index_from_current_offset(rd[fileInDirectory].firstBlockIndex, openFiles.Files[openIndex].offset);
  //   uint16_t currentOffset = openFiles.Files[openIndex].offset % BLOCK_SIZE;
  //   if(block_read(currentDataBlockIndex+sb.dataBlockIndex, &bounceBuffer)==-1){
  //     return -1;
  //   }
  //   uint16_t bytesLeftInBlock = BLOCK_SIZE-currentOffset;
  //   uint16_t bytesToCopy = (bytesLeftInBlock < count - bytesRead) ? bytesLeftInBlock : count - bytesRead;
   
  //   memcpy(buf+bytesRead, bounceBuffer+currentOffset,bytesToCopy);
  //   bytesRead+=bytesLeftInBlock;
  //   openFiles.Files[openIndex].offset+=bytesToCopy;
  // }
  return bytesRead;

  // uint16_t blockIndexBeforeRead = fs_block_index_from_current_offset(rd[fileInDirectory].firstBlockIndex, openFiles.Files[openIndex].offset);
  // uint16_t blockIndexAftRead =fs_block_index_from_current_offset(blockIndexBeforeRead, count);
  // uint8_t bounceBuffer[BLOCK_SIZE];
  //if our read only takes 1 block






  /* TODO: Phase 4 */
}
