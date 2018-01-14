#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "sort_file.h"
#include "sr_utils.h"
#include "block_quicksort.h"

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
  CHK_BF_ERR(BF_CreateFile(fileName));
  int fileDesc = 0;
  CHK_BF_ERR(BF_OpenFile(fileName, &fileDesc));

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
    return SR_ERROR;
  if (fieldNo < 0 || fieldNo > 3)
    return SR_ERROR;

  // Use SR_OpenFile to open the input sort file (only uses 1 block, unpins and destroys it after)
  int input_fileDesc = -1;
  SR_OpenFile(input_filename, &input_fileDesc);
  // Get the number of blocks in the input file
  int input_file_block_number;
  CHK_BF_ERR(BF_GetBlockCounter(input_fileDesc, &input_file_block_number));
  // Create and open a temp file
  char* temp_filename = "temp";
  CHK_BF_ERR(BF_CreateFile(temp_filename));
  int temp_fileDesc = -1;
  CHK_BF_ERR(BF_OpenFile(temp_filename, &temp_fileDesc));

  // Buffers and initialization
  BF_Block* buff_blocks[bufferSize];
  char* buff_data[bufferSize];
  for (int i = 0; i < bufferSize; i++)
    BF_Block_Init(&buff_blocks[i]);

  // Then copy each block of the input file into the temporary one (only uses 2 buffer blocks)
  for (int i = 1; i < input_file_block_number; i++) {
    // Get block of input file
    CHK_BF_ERR(BF_GetBlock(input_fileDesc, i, buff_blocks[0]));
    buff_data[0] = BF_Block_GetData(buff_blocks[0]);
    // Create block into the temp file
    CHK_BF_ERR(BF_AllocateBlock(temp_fileDesc, buff_blocks[1]));
    buff_data[1] = BF_Block_GetData(buff_blocks[1]);
    // Copy data
    memcpy(buff_data[1], buff_data[0], BF_BLOCK_SIZE);
    // Dirty and unpin
    BF_Block_SetDirty(buff_blocks[1]);
    CHK_BF_ERR(BF_UnpinBlock(buff_blocks[0]));
    CHK_BF_ERR(BF_UnpinBlock(buff_blocks[1]));
  }

  const int temp_block_num = input_file_block_number - 1;
  // Main while loop (for step 1, quicksort)
  // Sort blocks in groups of bufferSize
  int curr_group = 0;
  while (bufferSize + curr_group*bufferSize < temp_block_num) {
    // Load blocks into buffers

    for (int i = 0; i < bufferSize; i++) {
      CHK_BF_ERR(BF_GetBlock(temp_fileDesc, curr_group*bufferSize + i, buff_blocks[i]));
      buff_data[i] = BF_Block_GetData(buff_blocks[i]);
    }

    // Get total number of records in the buffers
    int tot_records = 0;
    for (int i = 0; i < bufferSize; i++) {
      int buff_recs = 0;
      memcpy(&buff_recs, buff_data[i], sizeof(int));
      tot_records += buff_recs;
    }
    // Call quicksort
    int low = 0;
    int high = tot_records - 1;
    block_quicksort(buff_data, fieldNo, low, high);
    // Dirty and unpin
    for (int i = 0; i < bufferSize; i++) {
      BF_Block_SetDirty(buff_blocks[i]);
      CHK_BF_ERR(BF_UnpinBlock(buff_blocks[i]));
    }

    // Move on to the next group
    curr_group++;
  }

  // Now call quicksort for the remaining blocks
  int rem_block_num = temp_block_num % bufferSize;
  // May also be the last bufferSize number of blocks 
  if (rem_block_num == 0)
    rem_block_num = bufferSize;

  // Load remaining blocks into buffers
  for (int i = 0; i < rem_block_num; i++) {
    CHK_BF_ERR(BF_GetBlock(temp_fileDesc, curr_group*bufferSize + i, buff_blocks[i]));
    buff_data[i] = BF_Block_GetData(buff_blocks[i]);
  }
  // Load blocks into buffers
  for (int i = 0; i < rem_block_num; i++) {
    CHK_BF_ERR(BF_GetBlock(temp_fileDesc, curr_group*bufferSize + i, buff_blocks[i]));
    buff_data[i] = BF_Block_GetData(buff_blocks[i]);
  }
  // Get total number of records in the buffers
  int tot_records = 0;
  for (int i = 0; i < rem_block_num; i++) {
    int buff_recs = 0;
    memcpy(&buff_recs, buff_data[i], sizeof(int));
    tot_records += buff_recs;
  }
  // Call quicksort
  int low = 0;
  int high = tot_records - 1;
  block_quicksort(buff_data, fieldNo, low, high);
  // Dirty and unpin
  for (int i = 0; i < rem_block_num; i++) {
    BF_Block_SetDirty(buff_blocks[i]);
    CHK_BF_ERR(BF_UnpinBlock(buff_blocks[i]));
  }

  // Check if bufferSize is greater than the number of blocks in the input file
  // in that case, step 2 is not needed
  if (bufferSize >= temp_block_num) {
    // Create the sorted, output file
    SR_CreateFile(output_filename);
    int output_fileDesc = 0;
    // Open it and copy temp_file contents
    SR_OpenFile(output_filename, &output_fileDesc);
    for (int i = 0; i < temp_block_num; i++) {
      // Get block of temp file
      CHK_BF_ERR(BF_GetBlock(temp_fileDesc, i, buff_blocks[0]));
      buff_data[0] = BF_Block_GetData(buff_blocks[0]);

      // Create a symmetric block into the output file
      CHK_BF_ERR(BF_AllocateBlock(output_fileDesc, buff_blocks[1]));
      buff_data[1] = BF_Block_GetData(buff_blocks[1]);

      // Copy data
      memcpy(buff_data[1], buff_data[0], sizeof(int));

      // Dirty and unpin
      BF_Block_SetDirty(buff_blocks[1]);
      CHK_BF_ERR(BF_UnpinBlock(buff_blocks[0]));
      CHK_BF_ERR(BF_UnpinBlock(buff_blocks[1]));
    }

    // End program
    // Destroy blocks
    for (int i=0; i < bufferSize; i++)
      BF_Block_Destroy(&buff_blocks[i]);
    // Close files
    SR_CloseFile(input_fileDesc);
    CHK_BF_ERR(BF_CloseFile(temp_fileDesc));
    SR_CloseFile(output_fileDesc);
    // Delete temp file
    remove(temp_filename);
    return SR_OK;

  }



  ////////////////Part 2//////////////////


  //the temp file will have two times the Blocks of the original file
  //we know that so we will allocate them now
  //we wont need to create anymore
  //and beacuse of the way we use them this will simplify the process

  // Create another temp_block_num number of blocks in the temp file with the same
  // number of records in them
  for (int i=0; i < temp_block_num; i++) {
    // Get block of temp file
    CHK_BF_ERR(BF_GetBlock(temp_fileDesc, i, buff_blocks[0]));
    buff_data[0] = BF_Block_GetData(buff_blocks[0]);

    // Create a symmetric block into the temp file
    CHK_BF_ERR(BF_AllocateBlock(temp_fileDesc, buff_blocks[1]));
    buff_data[1] = BF_Block_GetData(buff_blocks[1]);

    // Copy data
    memcpy(buff_data[1], buff_data[0], sizeof(int));

    // Dirty and unpin
    BF_Block_SetDirty(buff_blocks[1]);
    CHK_BF_ERR(BF_UnpinBlock(buff_blocks[0]));
    CHK_BF_ERR(BF_UnpinBlock(buff_blocks[1]));
  }


  // THIMISOU GROUPS EINAI TA TAKSINOMIMENA GROUP APO BLOCKS
  // MPOREI NA MENOUN 3, 4 KLP STO TELOS ANALOGA ME TO BUFFERSIZE AB TO BUFFERSIZE EINAI 5 MPOREI STO TELOS NA EXOUME 3 GROUP, DILADI NA MPREPEI NA XRISIMOPOIISOUME MONO 3 BUFFERS


  const int blocks_to_output = temp_block_num;
  int j=0;//helps as pass the groups we saw
  int blocks_in_group=bufferSize; //the next block group will be that far and the block groups will have that many blocks

  int num_of_block_groups = blocks_to_output/bufferSize;//number of groups
  if(blocks_to_output % bufferSize != 0)
    num_of_block_groups = num_of_block_groups + (blocks_to_output%bufferSize);     //if there is an incomplete group count it as a whole ME BUFFERSIZE 3 MPOREI NA PERISEUOUN 2 GROUP

  int number_of_merges = num_of_block_groups/(bufferSize-1);
  if (num_of_block_groups/(bufferSize-1) != 0)
    number_of_merges++;

  int groups_remain = num_of_block_groups;
  int fl=0;//depending on the number we see at the begining or at the middle
  int block_index[bufferSize]; //the number (index) of the block we have at the buffer 
  int new_group_num;

  int block_index_in_group[bufferSize];  //how many we passed in group
  int rec_index_in_block[bufferSize];  //how many records we passed in block
  int tot_recs_in_block[bufferSize]; //how many records does a block have


  int tot_blocks_in_group[bufferSize-1]; // POSA BLOCKS EXEI TO GROUP (MONO TO TELEUTAIO THA EXEI LIGOTERA)
  int buffers_needed_for_merge[number_of_merges]; // POSOUS BUFFERS THELOUME GIA TO WHILE (MONO GIA TO TELEUTAIO PRIN MIDENISTEI MPOREI NA THELOUME LIGOTEROUS APO 1-BUFFERSIZE-2)
  Record* record_data[bufferSize];
  int current_merge = 0;  // POSES FORES EXEI TREKSEI I WHILE MEXRI NA "MIDENISTEI" (PAEI APO TIN ARXI)
  
  // GIA DEBUG
  int counter = 0;
  

  // I TELEUTAIA OMADA APO BLOCK GROUPS MPOREI NA EINAI APO 0 EWS BUFFERSIZE-2 GROUPS (GIA TO MERGE POU XRISIMOPOIOUME TA BUFFERS)
  for (int i = 0; i < number_of_merges; i++) {
    if ((i == number_of_merges-1) && (number_of_merges % (bufferSize-1) != 0))
      buffers_needed_for_merge[i] = number_of_merges % (bufferSize-1);
    else
      buffers_needed_for_merge[i] = bufferSize-1;
  }

  // ENDIAMESI TAKSINOMISI
  while (num_of_block_groups > bufferSize-1) {
    printf("buffers_needed = %d\n", buffers_needed_for_merge[current_merge]);

    // Take the first block from the first bufferSize-1 block groups USING ONLY THE BUFFERS NEEDED FOR MERGE
    for (int i = 0; i < buffers_needed_for_merge[current_merge]; i++) {
      block_index[i] = blocks_to_output*fl + blocks_in_group*(i+j);
      CHK_BF_ERR(BF_GetBlock(temp_fileDesc, block_index[i], buff_blocks[i]));
      buff_data[i] = BF_Block_GetData(buff_blocks[i]);
    }

    if(groups_remain == num_of_block_groups) {//starting block for sorting
      if(fl == 0) {
        block_index[bufferSize-1] = blocks_to_output;
        CHK_BF_ERR(BF_GetBlock(temp_fileDesc, block_index[bufferSize-1], buff_blocks[bufferSize-1]));
        buff_data[bufferSize-1] = BF_Block_GetData(buff_blocks[bufferSize-1]);
        
      }
      else {
        block_index[bufferSize-1] = 0;
        CHK_BF_ERR(BF_GetBlock(temp_fileDesc , block_index[bufferSize-1], buff_blocks[bufferSize-1]));
        buff_data[bufferSize-1] = BF_Block_GetData(buff_blocks[bufferSize-1]);
      }
    }

    // TO TELEUTAIO GROUP THA EXEI ALLON ARITHMO BLOCKS
    int r_block_counter = blocks_to_output;
    for (int i = 0; i < buffers_needed_for_merge[current_merge]; i++) {
      if (r_block_counter > blocks_in_group) {
        tot_blocks_in_group[i] = blocks_in_group;
        r_block_counter = r_block_counter - blocks_in_group;
      }
      else
        tot_blocks_in_group[i] = r_block_counter;
    }

    /////////////////////////////////
    //EDW GINETAI TAKSINOMHSH////
    // Initialization for normal buffers may be fewer than bufferSize-1!!!!
    for (int i = 0; i < buffers_needed_for_merge[current_merge]; i++) {
      memcpy(&tot_recs_in_block[i], buff_data[i], sizeof(int));
      block_index_in_group[i] = 0;
      rec_index_in_block[i] = 0;
      record_data[i] = buff_data[i] + sizeof(int);
    }

    // For output buffer
    memcpy(&tot_recs_in_block[bufferSize-1], buff_data[bufferSize-1], sizeof(int));
    block_index_in_group[bufferSize-1] = 0;
    rec_index_in_block[bufferSize-1] = 0;
    record_data[bufferSize-1] = buff_data[bufferSize-1] + sizeof(int);



    // AUTO MIN TO PEIRAZEIS GT TO MIN_RECORD ME TA IF MPOREI NA MEINEI 0 XWRIS LOGO
    int no_more_recs = 0;
    while(!no_more_recs) {
      // Find min record value
      int min_record_i = -1;
      for (int buff_i = 0; buff_i < buffers_needed_for_merge[current_merge]; buff_i++) {
        if (block_index_in_group[buff_i] < tot_blocks_in_group[buff_i]) {
          
          if(min_record_i == -1 ||
              record_cmp(fieldNo, record_data[buff_i][rec_index_in_block[buff_i]], 
                        record_data[min_record_i][rec_index_in_block[min_record_i]]) < 0){
            
            min_record_i = buff_i;
          }
        }
      }


      printf("den trww seg prin apo to min block buffer=%d rec_index=%d\n", min_record_i, rec_index_in_block[min_record_i]);

      // Copy the whole record to bufferSize-1 (output block)
      record_data[bufferSize-1][rec_index_in_block[bufferSize-1]]
        = record_data[min_record_i][rec_index_in_block[min_record_i]];
      printf("COPIED RECORD!! %d\n", counter++);
        
      

      // We passed one record from one buffer
      rec_index_in_block[min_record_i]++;
      // If there are no other records in the block of the min record, move to the next block
      if (rec_index_in_block[min_record_i] == tot_recs_in_block[min_record_i]) {
        // Unpin previous block
        CHK_BF_ERR(BF_UnpinBlock(buff_blocks[min_record_i]));
        // Get next block
        block_index[min_record_i]++;
        block_index_in_group[min_record_i]++;

        // IF NOT DONT GET NEXT BLOCK (WE SEARCH FOR THE OTHER BUFFERS)
        if (block_index_in_group[min_record_i] < tot_blocks_in_group[min_record_i]) {
          CHK_BF_ERR(BF_GetBlock(temp_fileDesc, block_index[min_record_i], buff_blocks[min_record_i]));

          rec_index_in_block[min_record_i] = 0;

          buff_data[min_record_i] = BF_Block_GetData(buff_blocks[min_record_i]);
          memcpy(&tot_recs_in_block[min_record_i], buff_data[min_record_i], sizeof(int));
          record_data[min_record_i] = buff_data[min_record_i] + sizeof(int);
        }
      }

      // Now preform check to see if there are more records to copy
      no_more_recs = 1;
      for (int buff_i = 0; buff_i < buffers_needed_for_merge[current_merge]; buff_i++)
        if (block_index_in_group[buff_i] < tot_blocks_in_group[buff_i])
          no_more_recs = 0;

      if (!no_more_recs) { 
        // We passed one record in the output block
        rec_index_in_block[bufferSize-1]++;
        // Check if output buffer is full (get next block)
        if (rec_index_in_block[bufferSize-1] == tot_recs_in_block[bufferSize-1]) {
          // Dirty and Unpin previous block
          BF_Block_SetDirty(buff_blocks[bufferSize-1]);
          CHK_BF_ERR(BF_UnpinBlock(buff_blocks[bufferSize-1]));
        
          // Get next block
          block_index[bufferSize-1]++;
          block_index_in_group[bufferSize-1]++;//we passed one block
      
          CHK_BF_ERR(BF_GetBlock(temp_fileDesc, block_index[bufferSize-1], buff_blocks[bufferSize-1]));

          rec_index_in_block[bufferSize-1] = 0;

          buff_data[bufferSize-1] = BF_Block_GetData(buff_blocks[bufferSize-1]);
          memcpy(&tot_recs_in_block[bufferSize-1], buff_data[bufferSize-1], sizeof(int));
          record_data[bufferSize-1] = buff_data[bufferSize-1] + sizeof(int);
        }
      }
      // Else exit, Dirty and Unpin
      else {
        // Dirty and Unpin previous block
        BF_Block_SetDirty(buff_blocks[bufferSize-1]);
        CHK_BF_ERR(BF_UnpinBlock(buff_blocks[bufferSize-1]));
      }

    }

    ///////////////////////////////

    groups_remain = groups_remain - buffers_needed_for_merge[current_merge];
    printf("no new beggg groups_rem=%d\n", groups_remain);  
    if (groups_remain == 0) {
      printf("new beggg\n");  
      blocks_in_group *= bufferSize-1; //we merged bufferSize-1 groups with the same number of blocks AUTO EINAI TO GENIKO!! TO EIDIKO EINAI TO ARRAY int tot_blocks_in_group[bufferSize-1];
      new_group_num = num_of_block_groups / (bufferSize-1);
      
      // MI KSEXNAS TA GROUPS EINAI OI OMADES POU EINAI TAKSINOMIMENES STO PRWTO MEROS MPORTEI NA EINAI PERISSOTERA APO ENA AUTA POU PERISSEUOUN ANALOGA ME TO BUFFERSIZE
      if(num_of_block_groups%(bufferSize-1)!=0)
        new_group_num = new_group_num + num_of_block_groups%(bufferSize-1);
      
      num_of_block_groups = new_group_num;  //the same here
      groups_remain = num_of_block_groups; //we havent started merging the new groups yet
      
      j = 0;                               //we passed 0 so far
      fl= (fl+1) % 2;                       //changes between 0 and 1

      current_merge = 0;

      // TO NUMBER OF MERGES EINAI POSO THA TREKSEI I WHILE MEXRI NA "MIDENISTEI"
      // TO NUMBER OF BLOCK GROUPS EINAI DIAFORETIKO, SE ENA MERGE XRISIMOPOIOUNTAI POLLA BLOCK GROUPS ( TA BLOCK GROUPS EINAI AUTA POU SOU DINW APO PRWTO MEROS)
      int new_number_of_merges = number_of_merges / (bufferSize-1);
      if (number_of_merges / (bufferSize-1) != 0)
        new_number_of_merges++;

      number_of_merges = new_number_of_merges;

      // I TELEUTAIA OMADA APO BLOCK GROUPS MPOREI NA EINAI APO 0 EWS BUFFERSIZE-2 GROUPS (GIA TO MERGE POU XRISIMOPOIOUME TA BUFFERS)
      for (int i = 0; i < number_of_merges; i++) {
        if ((i == number_of_merges-1) && (number_of_merges % (bufferSize-1) != 0))
          buffers_needed_for_merge[i] = number_of_merges % (bufferSize-1);
        else
          buffers_needed_for_merge[i] = bufferSize-1;
      }

    }
    // Else j increases by bufferSize-1 in order to ignore the block groups that we saw before
    else {
      j = j + bufferSize-1;
      current_merge++;
    }
      
  }































/* TELEUTAIA TAKSINOMISI SXEDON IDIA ME PANW XWRIS TO MEGALO WHILE */




/*
// TELIKO GIA COPY STO OUTPUT KANEI TO TELIKO SORTARISMA EXOUME GROUPS = num_of_block_groups
  for (int i = 0; i < num_of_block_groups; i++) {
      block_index[i] = blocks_to_output*fl + blocks_in_group*(i+j);
      CHK_BF_ERR(BF_GetBlock(temp_fileDesc, block_index[i], buff_blocks[i]));
      buff_data[i] = BF_Block_GetData(buff_blocks[i]);
    }

    if(groups_remain==num_of_block_groups) {//starting block for sorting
      if(fl == 0) {
        block_index[bufferSize-1] = blocks_to_output;
        CHK_BF_ERR(BF_GetBlock(temp_fileDesc, block_index[bufferSize-1], buff_blocks[bufferSize-1]));
        buff_data[bufferSize-1] = BF_Block_GetData(buff_blocks[bufferSize-1]);
        
      }
      else {
        block_index[bufferSize-1] = 0;
        CHK_BF_ERR(BF_GetBlock(temp_fileDesc , block_index[bufferSize-1], buff_blocks[bufferSize-1]));
        buff_data[bufferSize-1] = BF_Block_GetData(buff_blocks[bufferSize-1]);
      }
    }


    /////////////////////////////////
    //EDW GINETAI TAKSINOMHSH////
    // Get number of records in each block
    Record* record_data[bufferSize];
    // Initialization
    for (int i = 0; i < bufferSize; i++) {
      memcpy(&tot_recs_in_block[i], buff_data[i], sizeof(int));
      block_index_in_group[i] = 0;
      rec_index_in_block[i] = 0;
      record_data[i] = buff_data[i] + sizeof(int);
    }

    int min_record_i = -1;
    while(block_index_in_group[bufferSize-1] < blocks_to_output) {
      
      // Find min record value
      for (int buff_i = 0; buff_i < bufferSize-1; buff_i++) {
        if ()EDWWWW EIMAIIIIIIIIIII
          if(record_cmp(fieldNo, record_data[buff_i][rec_index_in_block[buff_i]],
                        record_data[min_record_i][rec_index_in_block[min_record_i]]) < 0){
          
            min_record_i = buff_i;
        }
      }

      //copy the whole record to bufferSize-1
      record_data[bufferSize-1][rec_index_in_block[bufferSize-1]]
          = record_data[min_record_i][rec_index_in_block[min_record_i]];
      
      // We passed one in the min_block and one in the block we copy it
      rec_index_in_block[bufferSize-1]++;
      // Check if output buffer is full (get next block)
      if (rec_index_in_block[bufferSize-1] > tot_recs_in_block[bufferSize-1]){
        // Dirty and Unpin previous block
        BF_Block_SetDirty(buff_blocks[bufferSize-1]);
        CHK_BF_ERR(BF_UnpinBlock(buff_blocks[bufferSize-1]));
        // Get next block
        block_index[bufferSize-1]++;
        CHK_BF_ERR(BF_GetBlock(temp_fileDesc, block_index[bufferSize-1], buff_blocks[bufferSize-1]));
        block_index_in_group[bufferSize-1]++;//we passed one block
        buff_data[bufferSize-1] = BF_Block_GetData(buff_blocks[bufferSize-1]);
        memcpy(&tot_recs_in_block[bufferSize-1], buff_data[bufferSize-1], sizeof(int));
        record_data[bufferSize-1] = buff_data[bufferSize-1] + sizeof(int);
      }

      rec_index_in_block[min_record_i]++;
      // If there are no other records in the block of the min record, move to the next block
      if (rec_index_in_block[min_record_i] > tot_recs_in_block[min_record_i]) {
        // Unpin previous block
        CHK_BF_ERR(BF_UnpinBlock(buff_blocks[min_record_i]));
        // Get next block
        block_index[min_record_i]++;
        CHK_BF_ERR(BF_GetBlock(temp_fileDesc, block_index[min_record_i], buff_blocks[min_record_i]));
        block_index_in_group[min_record_i]++;
        rec_index_in_block[min_record_i] = 0;
        buff_data[min_record_i] = BF_Block_GetData(buff_blocks[min_record_i]);
        memcpy(&tot_recs_in_block[min_record_i], buff_data[min_record_i], sizeof(int));
        record_data[min_record_i] = buff_data[min_record_i] + sizeof(int);
      }

    }



  int output_fileDesc;
  //Create the sorted, output file
  SR_CreateFile(output_filename);
  SR_OpenFile(output_filename, &output_fileDesc);
  
  
  
  
  
  
  
  
  
  
  for (int i=0; i < bufferSize; i++)
    BF_Block_Destroy(&buff_blocks[i]); // <- ELPIZW NA MIN THELEI PARENTHESEIS

  // Use SR_closeFile to close the input file (SR_OpenFile was used to open it)
//  SR_CloseFile(input_fileDesc);

//  SR_CloseFile(output_fileDesc);
//  CHK_BF_ERR(BF_CloseFile(temp_fileDesc));*/
  remove("temp");

  return SR_OK;
}
























SR_ErrorCode SR_PrintAllEntries(int fileDesc) {
  BF_Block *block;
  BF_Block_Init(&block);
  Record record;
  // Get number of blocks
  int block_num;
  CHK_BF_ERR(BF_GetBlockCounter(fileDesc, &block_num));

  // File has been opened, so no need to check for errors

  // For each block
  for (int i = 1; i < block_num; i++) {
    CHK_BF_ERR(BF_GetBlock(fileDesc, i, block));
    char* block_data = BF_Block_GetData(block);
    // Get number of records in current block
    int rec_num = 0;
    memcpy(&rec_num, block_data, sizeof(int));
    // For each record in the block, get and print record
    for (int j = 0; j < rec_num; j++) {
      memcpy(&record, block_data + sizeof(int) + j*sizeof(Record),
          sizeof(Record));
      printf("%d,\"%s\",\"%s\",\"%s\"\n",
          record.id, record.name, record.surname, record.city);
    }
    // Unpin block
    CHK_BF_ERR(BF_UnpinBlock(block));
  }

  // Destroy block
  BF_Block_Destroy(&block);
  return SR_OK;
}
