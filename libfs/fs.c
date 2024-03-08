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
  if (block_disk_open(diskname) == -1) {
    return -1;
  }
  if (block_read(0, &sb) == -1) {
    return -1;
  }
  if(strncmp(sb.signature, "ECS150FS",8)!=0){
    return -1;
  }
  if(block_disk_count()!=sb.numTotalBlocks){
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
    strcpy(openFiles[openFileIterator].fileName,"\0");
  }
  return 0;
  /* TODO: Phase 1 */
}

int fs_umount(void) {
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
  for(int openFileIterator=0;openFileIterator<FS_OPEN_MAX_COUNT;openFileIterator++){
     openFiles[openFileIterator].fd=-1;
    openFiles[openFileIterator].offset=0;
    strcpy(openFiles[openFileIterator].fileName,"\0");
    close(openFiles[openFileIterator].fd);
  }
  return 0;
  /* TODO: Phase 1 */
}

int fs_info(void) {
  // Printing info into terminal
  printf("FS Info:\n");
  printf("total_blk_count=%d\n", sb.numTotalBlocks);
  printf("fat_blk_count=%d\n", sb.numFATBlocks);
  printf("rdir_blk=%d\n", sb.rootDirIndex);
  printf("data_blk=%d\n", sb.dataBlockIndex);
  printf("data_blk_count=%d\n", sb.numDataBlocks);


  // calculate the number of full FAT blocks as well as the extra fat blocks
  uint16_t fullFatBlocks = sb.numDataBlocks/2048;
  uint16_t extraFatEntries = sb.numDataBlocks - (fullFatBlocks*2048);

  //iterate through all the "full" FAT blocks
  int freeFAT = 0;
  for (int i = 0; i < fullFatBlocks; i++) {
    for (int j = 0; j < 2048; j++) { 
      if(i==0 && j==0){
        continue;
      }
      else if (FATarray[i].entries[j] == 0) {
        freeFAT++;
      }
    }
  }
  //iterate through all the "extra" FAT entries
  for(int free=0; free< extraFatEntries; free++){
    if(fullFatBlocks==0 && free==0){
      continue;
      }
      if(FATarray[fullFatBlocks].entries[free]==0){
        freeFAT++;
        }
    }

  //iterate through all the file entries
  int freeRoot = 0;
  for (int k = 0; k < FS_FILE_MAX_COUNT; k++) {
    if (rd[k].fileName[0] == '\0') {
      freeRoot++;
    }
  }
  printf("fat_free_ratio=%d/%d\n", freeFAT, sb.numDataBlocks);
  printf("rdir_free_ratio=%d/%d\n", freeRoot, 128);
  /* TODO: Phase 1 */
  return 0;
}

int fs_create(const char *filename) {
  //check invalid file
  if (filename == NULL || strlen(filename) >= FS_FILENAME_LEN) {
    return -1;
  }
  //check if the filename is null terminated
  if (strchr(filename, '\0') == NULL) {
        return -1;
    }

  int find =0;
  // if file is already in the root directory
  for (find = 0; find < FS_FILE_MAX_COUNT; find++) {
    if (strcmp(filename, rd[find].fileName) == 0) {
  //    printf("file is already here\n");
      return -1;
    }
  }

  //finds an empty spot in the root directory
  int emptySpot = -1;
  for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
    if (rd[i].fileName[0]=='\0') {
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
  return 0;
}

int fs_delete(const char *filename) {
  // Look for file in the root directory 
  int fileIndex = -1;
  for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
    if (strcmp(rd[i].fileName, filename) == 0) { 
      fileIndex = i;
      break;
    }
  }
  // error check if we found the file
  if (fileIndex > FS_FILE_MAX_COUNT || fileIndex < 0) {
    return -1;
  }
  
  //check is the files is opened
  for(int opened=0;opened<FS_OPEN_MAX_COUNT;opened++){
    if(strcmp(openFiles[opened].fileName, filename)==0){
      return -1;
    }
  }

  //reset file-chain in fat 
  int block_in_fat = rd[fileIndex].firstBlockIndex/2048;
  int entry_no = rd[fileIndex].firstBlockIndex%2048;
  while(FATarray[block_in_fat].entries[entry_no]!=FAT_EOC){
      //find the next FAT entry to go to 
      int next_block_in_fat = FATarray[block_in_fat].entries[entry_no]/2048;
      int next_entry_no = FATarray[block_in_fat].entries[entry_no]%2048;
      //make the current entry "free"
      FATarray[block_in_fat].entries[entry_no] = 0;
      block_in_fat = next_block_in_fat;
      entry_no = next_entry_no;
  }
  //make the last entry in the link "free"
  FATarray[block_in_fat].entries[entry_no]=0;

  //take out the file in the root directory
  strcpy(rd[fileIndex].fileName, "\0");
  rd[fileIndex].fileSize = 0;
  rd[fileIndex].firstBlockIndex = FAT_EOC;
  return 0;
}

int fs_ls(void) {
  printf("FS Ls:\n");
  //iterate through all the files in the root directory
  for (int currentFile = 0; currentFile < Root_Dir_Entries; currentFile++) {
    if (rd[currentFile].fileName[0] == '\0') {
      break;
    } else {
      //print metadata
      printf("file: %s, size: %d, data_blk: %d\n", rd[currentFile].fileName,
             rd[currentFile].fileSize, rd[currentFile].firstBlockIndex);
    }
  }
  return 0;
}

int fs_open(const char *filename) {  
  // First check if the file is mounted

  // Check if the file is NULL or invalid
  if (filename == NULL || strlen(filename)>FS_FILENAME_LEN) {
    return -1;
  }
  //check if the filename is null terminated
  if (strchr(filename, '\0') == NULL) {
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
    return -1;
  }
  //opens the filename
  int fd = open(filename, O_RDWR | O_CREAT, 0644);
  if(fd<0){
    return -1;
  }
  //checks if the file is already opened or if there's a space to open it
  for(int openFileIndex=0;openFileIndex<FS_OPEN_MAX_COUNT;openFileIndex++){
    //replace the current fd
    if(strcmp(openFiles[openFileIndex].fileName, filename)==0){
      dup2(openFiles[openFileIndex].fd, fd);
      openFiles[openFileIndex].fd=fd;
      return fd;
    }
    //checks if an entry is empty, and we can occupy (open) the file
    else if(openFiles[openFileIndex].fd==-1){
      dup2(openFiles[openFileIndex].fd, fd);
      openFiles[openFileIndex].fd = fd;
      openFiles[openFileIndex].offset=0;
      strcpy(openFiles[openFileIndex].fileName,filename);
      //error check
      if(openFiles[openFileIndex].fd<0){
        return -1;
      }
      return fd;
    }
  }
  //if no spaces are open in the array
  return -1;
}

int fs_close(int fd) {
  int fdFind = 0;
  int found = -1;
  //checks if the file is opened
  for (fdFind = 0; fdFind < FS_FILE_MAX_COUNT; fdFind++) {
    if (openFiles[fdFind].fd == fd) {
      found=1;
      break;
    }
  }
  //if the file is not even opened
  if (found==-1) {
    return -1;
  }
  //empties the entry in the openFiles array
  openFiles[fdFind].fd = -1;
  openFiles[fdFind].offset = 0;
  strcpy(openFiles[fdFind].fileName, "\0");
  //closes file
  close(openFiles[fdFind].fd);
  return 0;
}

int fs_stat(int fd) {
  char nameOfFile[FS_FILENAME_LEN];
  int openIndex = 0;
  int found = 0;
  //iterates through the openfiles
  for (openIndex = 0; openIndex < FS_OPEN_MAX_COUNT; openIndex++) {
    if (openFiles[openIndex].fd == fd) {
      strcpy(nameOfFile, openFiles[openIndex].fileName);
      found = 1;
      break;
    }
  }
  // if the file is not open
  if (found == 0) {
    return -1;
  }
  //iterates through the root directory
  for(int i=0;i<Root_Dir_Entries;i++){
    if(strcmp(rd[i].fileName, nameOfFile)==0){
      return rd[i].fileSize;
    }
  }
  return -1;
}

int fs_lseek(int fd, size_t offset) {
  int openIndex = 0;
  int found = 0;
  //check if file is opened and get the filesize
  for (openIndex = 0; openIndex < FS_OPEN_MAX_COUNT; openIndex++) {
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
      //sets the offset of the file
      openFiles[openIndex].offset = offset;
      found = 1;
      break;
    }
  }
  // if the file is not open
  if (found == 0) {
    return -1;
  }
  return 0;
}

//from the firstblock index, (the same as the entry point to the fat chain)
uint16_t fs_block_index_from_current_offset(uint16_t startBlock, size_t currFileOffset){

  //calculate how many entries to traverse the fat array from the starting block
  uint16_t count = currFileOffset/BLOCK_SIZE;
  //keeps track of which block to return
  uint16_t returnBlock = startBlock;
  
  for(int i=0;i<count;i++){
    //determine which FatBlock and FatIndex to go to (correspond to a single data block)
    uint16_t nextBlock = returnBlock/2048;
    uint16_t nextIndex = returnBlock%2048;
    returnBlock = FATarray[nextBlock].entries[nextIndex];
  }
  return returnBlock;
}

//adds a fat entry to the end of the FAT chain, return -1 if there's no more fat entries
int add_FAT_Entry(uint16_t FATBlock, uint16_t FATEntry){
   // calculate the number of full FAT blocks as well as the extra fat entries
  uint16_t fullFatBlocks = sb.numDataBlocks/2048;
  uint16_t extraFatEntries = sb.numDataBlocks - (fullFatBlocks*2048);
  //iterates through entire fatblocks
  for(int i=0;i<fullFatBlocks;i++){
    for(int j=0;j<2048;j++){
      //skips the very first entry
      if(j==0 && i==0){
        continue;
      }
      //checks if the entry is free
      if(FATarray[i].entries[j]==0){
        //fill the current entry with the "free" entry
        FATarray[FATBlock].entries[FATEntry]= FATarray[i].entries[j];
        FATarray[i].entries[j]=FAT_EOC;
        return 1;
      }
    }
  }
  // iterates through the extra FAT entries (if any)
  for(int extra = 0; extra< extraFatEntries; extra++){
    //skips the very first entry
    if(fullFatBlocks==0 && extra ==0){
      continue;
    }
     //checks if the entry is free
    if(FATarray[fullFatBlocks].entries[extra]==0){
       //fill the current entry with the "free" entry
       FATarray[FATBlock].entries[FATEntry]= FATarray[fullFatBlocks].entries[extra];
       FATarray[fullFatBlocks].entries[extra]=FAT_EOC;
      return 1;
    }
  }
  return -1;
}

//finds an empty FAT entry
uint16_t find_empty_FAT(){
   // calculate the number of full FAT blocks as well as the extra fat entries
  uint16_t fullFatBlocks = sb.numDataBlocks/2048;
  uint16_t extraFatEntries = sb.numDataBlocks - (fullFatBlocks*2048);
  //iterates through the the entire FAT block
   for(int i=0;i<fullFatBlocks;i++){
    for(int j=0;j<2048;j++){
      //skips very first entry
      if(j==0 && i==0){
        continue;
      }
      //return the DATABLOCK index if the fat entry is free
      if(FATarray[i].entries[j]==0){
        return (i*2048 + j);
      }
    }
  }
  //iterates through any extra entries in the FAT
  for(int extra =0; extra< extraFatEntries;extra++){
    //skips very first entry
      if(fullFatBlocks==0 && extra ==0){
      continue;
    }
    //return the DATABLOCK index if the fat entry is free
    if(FATarray[fullFatBlocks].entries[extra]==0){
      return (fullFatBlocks*2048)+ extra;
    }
  }
  return 0;
}

int fs_write(int fd, void *buf, size_t count) {
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
      found2=1;
      break;
    }
  }
  if(found2==-1){
    return -1;
  }
  //if we are writing to an initiall empty file, occupy the firstBlockIndex to write the data
    if(rd[fileInDirectory].firstBlockIndex == FAT_EOC){
      rd[fileInDirectory].firstBlockIndex = find_empty_FAT();
      if(rd[fileInDirectory].firstBlockIndex==0){
        return 0;
      }
     uint16_t FATBlock2 = rd[fileInDirectory].firstBlockIndex/2048;
     uint16_t FATIndex2 = rd[fileInDirectory].firstBlockIndex%2048;
     FATarray[FATBlock2].entries[FATIndex2]=FAT_EOC;
    }


  //bounce buffer and how many bytes we have currently written
  size_t bytesWrite=0;
  uint8_t bounceBuffer[BLOCK_SIZE];

  //while we have written less than our desires amount, write the appropriate amount of bytes 
  while(bytesWrite<count){
    //find current data block you are on
     uint16_t currBlock=fs_block_index_from_current_offset(rd[fileInDirectory].firstBlockIndex, openFiles[openIndex].offset);

     //find the corresponding entry in the FATarray
     uint16_t FATBlock = currBlock/2048;
     uint16_t FATIndex = currBlock%2048;

     //find the current offset in the file
     uint16_t currentOffset = openFiles[openIndex].offset % BLOCK_SIZE;

     //read an entire block into the bounce buffer
      if(block_read(sb.dataBlockIndex+currBlock,&bounceBuffer)==-1){
      return -1;
    }

    //determine how many bytes are left in the current block
    uint16_t bytesLeftInBlock = BLOCK_SIZE-currentOffset;
    //determine how many bytes to copy from the bouncebuffer
    uint16_t bytesToCopy = (bytesLeftInBlock < count - bytesWrite) ? bytesLeftInBlock : count - bytesWrite;
    bytesToCopy = (bytesToCopy< count) ? bytesToCopy: count;

    //copy the contents from the buf (starting at how many bytes we've already written) into the bounce buffer(starting from the current offset)
    memcpy(bounceBuffer+currentOffset, buf+bytesWrite,bytesToCopy);
    //update the amount of bytes youve written
    bytesWrite+=bytesToCopy;

    //write to the datablock buffer
    if(block_write(currBlock+sb.dataBlockIndex, &bounceBuffer)==-1){
      return -1;
    }
    //update the offset of the file
    openFiles[openIndex].offset+=bytesToCopy;
    //the filesize is now at our new offset
    rd[fileInDirectory].fileSize=openFiles[openIndex].offset;

    //update how many bytes are left in the block
    bytesLeftInBlock = BLOCK_SIZE - (openFiles[openIndex].offset % BLOCK_SIZE);

    //if the current block we wrote to contains the end of the file AND we need to write in another block, find the next free FAT entry
     if(FATarray[FATBlock].entries[FATIndex]==FAT_EOC && count-bytesWrite>bytesLeftInBlock){
      int nextBlock = add_FAT_Entry(FATBlock,FATIndex);
      //if we can't, the FAT array must be full
      if(nextBlock==-1){
        return bytesWrite;
      }
   }
  }
  return bytesWrite;
}

int fs_read(int fd, void *buf, size_t count) {
  //input validation
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

  //keeps track of the amount bytes read, as well as bytes left in the file to read
  size_t bytesRead=0;
  uint8_t bounceBuffer[BLOCK_SIZE];
  uint16_t bytesLeftInFile = rd[fileInDirectory].fileSize - openFiles[openIndex].offset;

  //keep boing until you read the correct amount(count)
  while(bytesRead<count){
    //update bytes left in the file
    bytesLeftInFile = rd[fileInDirectory].fileSize - openFiles[openIndex].offset;
    //update the current data block you are on
   uint16_t currentDataBlockIndex = fs_block_index_from_current_offset(rd[fileInDirectory].firstBlockIndex, openFiles[openIndex].offset);

   //calculate correcponding data block in the FAT
   uint16_t FATBlock = currentDataBlockIndex/2048;
   uint16_t FATIndex = currentDataBlockIndex%2048;
   
   //find the current offset in the block
   uint16_t currentOffset = openFiles[openIndex].offset % BLOCK_SIZE;

   //read the entire block from the datablock to the bouncebuffer
    if(block_read(sb.dataBlockIndex+currentDataBlockIndex,&bounceBuffer)==-1){
      return -1;
    }
    //calculate bytes left in the block
    uint16_t bytesLeftInBlock = BLOCK_SIZE-currentOffset;
    //determine how many bytes to copy
    uint16_t bytesToCopy = (bytesLeftInBlock < bytesLeftInFile) ? bytesLeftInBlock : bytesLeftInFile;
    bytesToCopy = (bytesToCopy < count) ? bytesToCopy: count;

    //copy the contents of the bouncebuffer (starting from the current offset) to the buf
    memcpy(buf+bytesRead, bounceBuffer+currentOffset,bytesToCopy);

    //update the amount of bytes read as well as the offset of the file
    bytesRead+=bytesToCopy;
    openFiles[openIndex].offset+=bytesToCopy;

    //at this point, we would assume we read until the end of the file, so we can break
    if(FATarray[FATBlock].entries[FATIndex]==FAT_EOC || bytesLeftInFile==0){
      break;
   }
  }
  return bytesRead;

  /* TODO: Phase 4 */
}
