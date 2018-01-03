#ifndef SR_UTILS
#define SR_UTILS

//#include "sort_file.h"

int record_cmp(int, Record, Record);
void record_swap(Record*, Record*);
Record* get_nth_record(char* buffer_data[], int n);

#endif /* SR_UTILS */
