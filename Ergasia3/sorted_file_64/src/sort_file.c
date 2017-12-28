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
  BF_Block *block;
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
  BF_Block *block;
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
  BF_Block *block;
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

  // Create and open a temp file
  char* temp_filename = "temp";
  CHK_BF_ERR(BF_CreateFile(temp_file));

  // Use SR_OpenFile to open the input sort file (only uses 1 block and destroys it after)
  int input_fileDesc = -1;
  SR_OpenFile(input_filename, &input_fileDesc);

  // Allocate buffer blocks (BUFF_DATA PINAKAS ME CHAR* GIA TA BUFFERS)
  BF_Block **buff_blocks;
  buff_blocks = malloc(bufferSize * sizeof(BF_Block*));
  char **buff_data;
  buff_data = malloc(bufferSize * sizeof(char*));

  // Initialize the buffer blocks
  for (int i = 0; i < bufferSize; i++)
    BF_Block_Init(&buff_blocks[i]); // <- ELPIZW NA MIN THELEI PARENTHESEIS

  // Get number of blocks of the input sort file
  int block_num = -1;
  CHK_BF_ERR(BF_GetBlockCounter(input_fileDesc, &block_num));

  // WHILE FOR QUICKSORT
  CHK_BF_ERR(BF_AllocateBlock(fileDesc, block));
  // Initialize with metadata (1 record in the block)
  char* block_data = BF_Block_GetData(block);




  //create the sorted file
  CHK_BF_ERR(BF_CreateFile(output_filename));





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


  //YPARXEI KAI PIO PANW
  //for (int i = 0; i < bufferSize; i++)
  //  BF_Block_Destroy(&buff_blocks[i]); // <- ELPIZW NA MIN THELEI PARENTHESEIS

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
