#include "sort_file.h"

#define CHK_BF_ERR(call)      \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      return SR_ERROR;        \
    }                         \
  }

SR_ErrorCode SR_Init() {
  // Your code goes here

  return SR_OK;
}

SR_ErrorCode SR_CreateFile(const char *fileName) {
  // Create and open file
  CHK_BF_ERR(BF_CreateFile(filename));
  int fileDesc = 0;
  CHK_BF_ERR(BF_OpenFile(filename, &fileDesc));

  // Allocate the file's first block
  BF_Block* block;
  BF_Block_Init(&block);
  CHK_BF_ERR(BF_AllocateBlock(fileDesc, block));
  // Initialize it with metadata needed to know its a heap file (.sf)
  char* block_data = BF_Block_GetData(block);
  char sf_id[4] = ".sf";
  memcpy(block_data, sf_id, strlen(sf_id) + 1);

  // Dirty and unpin
  BF_Block_SetDirty(block);
  CHK_BF_ERR(BF_UnpinBlock(block));
  // Destroy block and close file
  BF_Block_Destroy(&block);
  CHK_BF_ERR(BF_CloseFile(fileDesc));

  return SR_OK;
}



SR_ErrorCode SR_OpenFile(const char *fileName, int *fileDesc) {
  // Open file
  int tmp_fd = 0;
  CHK_BF_ERR(BF_OpenFile(fileName, &tmp_fd));
  // Check if there is a block in the file
  int block_num;
  CHK_BF_ERR(BF_GetBlockCounter(tmp_fd, &block_num));
  if (block_num == 0) {
    printf("Error: File %s is not a sort file\n", fileName);
    return SR_ERROR;
  }

  // Else check if its a sort file
  BF_Block* block;
  BF_Block_Init(&block);
  // There should be an ".sf" at the start of the first block
  CHK_BF_ERR(BF_GetBlock(tmp_fd, 0, block));
  char* block_data = BF_Block_GetData(block);
  if (strcmp(block_data, ".sf") != 0) {
    printf("Error: File %s is not a sort file\n", fileName);
    return SR_ERROR;
  }

  // Assign the fileDesc value
  *fileDesc = tmp_fd;

  // Unpin and destroy block
  CHK_BF_ERR(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  return SR_OK;
}



SR_ErrorCode SR_CloseFile(int fileDesc) {
  // Just call BF_CloseFile and check for errors
  CHK_BF_ERR(BF_CloseFile(fileDesc));

  return SR_OK;
}



SR_ErrorCode SR_InsertEntry(int fileDesc,	Record record) {
  BF_Block* block;
  BF_Block_Init(&block);
  // Get number of blocks
  int block_num;
  CHK_BF_ERR(BF_GetBlockCounter(fileDesc, &block_num));

  // If the only block allocated is the metadata block
  if (block_num == 1) {
    // Allocate another block
    CHK_BF_ERR(BF_AllocateBlock(fileDesc, block));
    // Initialize with metadata (1 record in the block)
    char* block_data = BF_Block_GetData(block);
    int rec_num = 1;
    memcpy(block_data, &rec_num, sizeof(int));

    // Insert record
    memcpy(block_data + sizeof(int), &record, sizeof(Record));

    // Dirty and unpin
    BF_Block_SetDirty(block);
    CHK_BF_ERR(BF_UnpinBlock(block));
  }
  else {
    // Get the number of records in the current block
    CHK_BF_ERR(BF_GetBlock(fileDesc, block_num - 1, block));
    char* block_data = BF_Block_GetData(block);
    int rec_num;
    memcpy(&rec_num, block_data, sizeof(int));

    // Check if there is enough space for a new record in
    // the current block
    int used_space = sizeof(int) + rec_num*sizeof(Record);
    if (BF_BLOCK_SIZE - used_space >= sizeof(Record)) {
      // Increment and update rec_num metadata
      rec_num++;
      memcpy(block_data, &rec_num, sizeof(int));

      // Insert record
      memcpy(block_data + used_space, &record, sizeof(Record));

      // Dirty and unpin
      BF_Block_SetDirty(block);
      CHK_BF_ERR(BF_UnpinBlock(block));
    }
    // Else allocate a new block
    else {
      // First unpin last block
      CHK_BF_ERR(BF_UnpinBlock(block));

      // Allocate another block
      CHK_BF_ERR(BF_AllocateBlock(fileDesc, block));
      // Initialize with metadata (1 record in the block)
      char* block_data = BF_Block_GetData(block);
      int rec_num = 1;
      memcpy(block_data, &rec_num, sizeof(int));

      // Insert record
      memcpy(block_data + sizeof(int), &record, sizeof(Record));

      // Dirty and unpin
      BF_Block_SetDirty(block);
      CHK_BF_ERR(BF_UnpinBlock(block));
    }
  }

  // Destroy block
  BF_Block_Destroy(&block);
  return SR_OK;
}

SR_ErrorCode SR_SortedFile(
  const char* input_filename,
  const char* output_filename,
  int fieldNo,
  int bufferSize
) {

  // Check for invalid bufferSize and fieldNo
  if (bufferSize < 3 || bufferSize > BF_BUFFER_SIZE)
    return SR_WRONG_BUFFER_SIZE;
  if (fieldNo < 0 || fieldNo > 3)
    return SR_WRONG_FIELD_NO;

  // Use SR_OpenFile to open the input sort file (only uses 1 block, unpins and destroys it after)
  int input_fileDesc = -1;
  SR_OpenFile(input_fileName, &input_fileDesc);
  // Get the number of blocks in the input file
  int input_file_block_number;
  CHK_BF_ERR(BF_GetBlockCounter(tmp_fd, &input_file_block_number));

  // Create and open a temp file
  char* temp_filename = "temp";
  CHK_BF_ERR(BF_CreateFile(temp_filename));
  int temp_fileDesk = -1;
  CHK_BF_ERR(BF_OpenFile(temp_filename, &temp_fileDesk));


  // AUTO?? PREPEI NA XEIRIZOMASTE MONO TA BUFFER BLOCKS (PAIZEI NA TO XA VALEI KEGW?)
  BF_Block* temp_block;


  // Buffers and initialization
  BF_Block* buff_blocks[bufferSize];
  char* buffer_data[bufferSize];
  for (int i = 0; i < bufferSize; i++)
    BF_Block_Init(&buff_blocks[i]); // <- ELPIZW NA MIN THELEI PARENTHESEIS

  // Then copy each block of the input file into the temporary one (only uses 2 buffer blocks)
  for (int i = 0; i < input_file_block_number; i++) {
    // Get block of input file
    CHK_BF_ERR(BF_GetBlock(input_fileDesc, i, buff_blocks[0]));
    buff_data[0] = BF_Block_GetData(buff_blocks[0]);
    // Create block into the temp file
    CHK_BF_ERR(BF_AllocateBlock(temp_fileDesk, buff_blocks[1]));
    buff_data[1] = BF_Block_GetData(buff_blocks[1]);
    // Copy data
    memcpy(block_data[1], block_data[0], BF_BLOCK_SIZE);
    // Dirty and unpin
    BF_Block_SetDirty(block_data[1]);
    CHK_BF_ERR(BF_UnpinBlock(block_data[0]));
    CHK_BF_ERR(BF_UnpinBlock(block_data[1]));
  }

  // Main while loop (for step 1, quicksort)
  // Sort blocks in groups of bufferSize
  int curr_group = 0;
  while (curr_group*bufferSize < input_file_block_number) {
    // Load blocks into buffers
    for (int i = 0; i < bufferSize; i++) {
      CHK_BF_ERR(BF_GetBlock(input_fileDesc, curr_group*bufferSize + i, buff_blocks[i]));
      buff_data[i] = BF_Block_GetData(buff_blocks[i]);
    }
    // Call quicksort
    


    // Move on to the next group
    curr_group++;
  }

  // Create the sorted, output file
  CHK_BF_ERR(BF_CreateFile(output_filename));
  // Check if bufferSize is greater than the number of blocks in the input file
  // in that case, step 2 is not needed





  ////////////////part 2//////////////////
// STO TEMP FILE TA PRWTA N EINAI TA BLOCKS POU EXEI TO INPUT FILE OPOU TA
// SINEXOMENA N/BUFFERSIZE EINAI IDI TAKSINOMIMENA METAKSI TOUS
// PX AN TA BLOCKS TOU INPUT EINAI 9 ME BUFFERSIZE 3
// TA 1,2,3 EINAI TAKSINOMIMENA METAKSI TOUS
// OPWS KAI TA 4,5,6 KAI TA 7,8,9
// AN DEN DIAIREITAI AKRIVWS APLA MENOUN KAPOIA STO TELOS
// TA XW ME MEGALA GRAMMATA GIA NA THIMITHOUME NA TA SVISOUME AYTA OK?

// SIMANTIKO
// STIN ARXI TOU KATHE BLOCK IPARXEI ENAS INT POU LEEI POSA RECORDS IPARXOUN MESA STO BLOCK
// DEN KSERW AN PREPEI NA XRISIMOPOIISOUME ALLA METADATA
// AFOU TA DEDOMENA KATHE BLOCK EINAI STANDARD
// EPISIS MPOROUME MONO NA XRISIMOPOIISOUME TA BUFFER BLOCKS, OTAN FERNOUME KATI TO FORTWNOUME EKEI

// PARE AUTO EDW KANE OTI THES
//OTAN ARXISEI TO PART 2 STO TEMP FILE THA IPARXOUN ALLOCATED BLOCKS ISA ME TON ARITHMO TWN BLOCKS STO ARXIKO ARXEIO
// APO KEI KAI PERA KANE OTI THES

  //the temp file will have two times the Blocks of the original file
  //we know that so we will allocate them now
  //we wont need to create anymore
  //and beacuse of the way we use them this will simplify the process
  for(int i=0;i<input_file_block_number;i++){
    CHK_BF_ERR(BF_AllocateBlock(temp_fileDesk, temp_block));
    CHK_BF_ERR(BF_UnpinBlock(temp_block));
  }

  int fieldNo_pass;//depending on fieldNo we need to pass at the start fieldNo_pass bits
  int fieldNo_size;//size of what we are sorting

  if(fieldNo==0){
    fieldNo_pass=0;
    fieldNo_size=sizeof(int);
  }
  else if(fieldNo==1){
    fieldNo_pass=sizeof(int);
    fieldNo_size=15*sizeof(char);
  }
  else if(fieldNo==2){
    fieldNo_pass=sizeof(int)+15*sizeof(char);
    fieldNo_size=20*sizeof(char);
  }
  else{
    fieldNo_pass=sizeof(int)+15*sizeof(char)+20*sizeof(char);
    fieldNo_size=20*sizeof(char);
  }

  int j=0;//helps as pass the groups we saw
  int num_of_blocks=bufferSize; //the next block will be that far and the block groups will have that many blocks
  int num_of_block_groups=input_file_block_number/bufferSize;//number of groups

  if(input_file_block_number/bufferSize==0)
    num_of_block_groups++;                  //if there is an incomplete group count it as a whole

  int groups_remain=num_of_block_groups;
  int fl=0;//depending on the number we see at the begining or at the middle
  int blocknum[bufferSize]; //the number of the block we have at the buffer
  int new_group_num;
  int last_record; //the last block might not be full
  int smaller_record_location; //smaller record in a buffer block
  char* smaller
//MAX RECORDS
  char *info[bufferSize-1];
  int blocks_passed[bufferSize];
  int records_passed[bufferSize];
  char *records_in_block[bufferSize-1];

  for(int i=0;i<bufferSize;i++){
    int blocks_passed[i]=0;
    int records_passed[i]=0;
  }

  while(1){
    for(int i=0;i<bufferSize-1;i++){//take the first block from the first bufferSize-1 block groups
      if(i==num_of_block_groups)
        break;
      CHK_BF_ERR(BF_GetBlock(temp_fileDesk,input_file_block_number*fl+ num_of_blocks*(i+j), buff_blocks[i]));
      buff_data[i] = BF_Block_GetData(buff_blocks[i]);
      blocknum[i]=input_file_block_number*fl+ num_of_blocks*(i+j);
    }

      if(groups_remain==num_of_block_groups){//starting block for sorting
        if(fl==0){
          CHK_BF_ERR(BF_GetBlock(temp_fileDesk , input_file_block_number , buff_blocks[bufferSize-1]));
          buff_data[bufferSize-1]=BF_Block_GetData(buff_blocks[bufferSize-1]);
          blocknum[bufferSize-1]=input_file_block_number;
        }
        else{
          CHK_BF_ERR(BF_GetBlock(temp_fileDesk , 0 , buff_blocks[bufferSize-1]));
          buff_data[bufferSize-1]=BF_Block_GetData(buff_blocks[bufferSize-1]);
          blocknum[bufferSize-1]=0;
        }
      }

      if(num_of_block_groups<=bufferSize-1){
        break;
      }

      for(int i=0;i<bufferSize-1;i++){
        memcpy(records_in_block[i],buff_data[i],sizeof(int));
        memcpy(info[i],buff_data[i] + fieldNo_pass,fieldNo_size);//pedio analoga me to fieldNo apo to prwto record
      }

      while(1){
        strpcy(smaller_record,info[0]);
        for(int i=1;i<bufferSize-1;i++){
          if(strcmp(smaller_record,info[i])<0){
            smaller_record_loc=i;
            strpcy(smaller_record,info[i]);
          }
          //CHANGE THE RECORDS NUMBER
        }

        if(records_passed[bufferSize-1]<max_records){
          memcpy(buff_data[bufferSize-1],sizeof(int) + buff_data[smaller_record_loc] +records_passed[bufferSize-1]*sizeof(Record) ,fieldNo_size);
          records_passed[bufferSize-1]++;
          //SETDIRTY
        }
        else{
          //NEXT BLOCK
        }
      }

      groups_remain-=bufferSize-1;





    if(groups_remain==0){
      num_of_blocks*=bufferSize-1; //we merged bufferSize-1 groups with the same number of blocks
      new_group_num=num_of_block_groups/(bufferSize-1);
      if(num_of_block_groups/(bufferSize-1)==0)
        new_group_num++;
      num_of_block_groups=new_group_num;  //the same here
      groups_remain=num_of_block_groups; //we havent started merging the new groups yet
      j=0;                               //we passed 0 so far
      fl=(fl+1)%2;                       //changes between 0 and 1
    }
    else
      j=j+bufferSize-1;
      //j increases by bufferSize-1 in order to ignore the block groups that we saw before


  }

for (int i = 0; i < bufferSize; i++)
  BF_Block_Destroy(&buff_blocks[i]); // <- ELPIZW NA MIN THELEI PARENTHESEIS

  // Use SR_closeFile to close the input file (SR_OpenFile was used to open it)
  SR_CloseFile(input_fileDesc);


  CHK_BF_ERR(BF_CloseFile(output_filename));
  CHK_BF_ERR(BF_CloseFile(temp_file));


  return SR_OK;
}

SR_ErrorCode SR_PrintAllEntries(int fileDesc) {
  // Your code goes here

  return SR_OK;
}
