#ifndef BLOCK_QUICKSORT
#define BLOCK_QUICKSORT

// ME TA [] TI PAIZEI??
void block_quicksort(char* buffer_data[], int fieldNo, int low, int high);
void block_partition(char* buffer_data[], int fieldNo, int low, int high);
Record* get_nth_record(char* buffer_data[], int n);



#endif /* BLOCK_QUICKSORT */