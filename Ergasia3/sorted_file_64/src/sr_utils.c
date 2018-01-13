#include <string.h>

#include "sort_file.h"

// Compares records by comparing a specific field (input fieldNo)
// Output is similar to strcmp (only with -2 output for input errors)
int record_cmp(int fieldNo, Record record1, Record record2) {
  printf("\n\nFDFDGDFGF\n" );
  // Only for comparing the id of the records
  if (fieldNo == 0) {
    if (record1.id < record2.id)
      return -1;
    else if (record1.id > record2.id)
      return 1;
    else
      return 0;
  }
  else if (fieldNo <= 3) {
    char* string_field1;
    char* string_field2;
    // Get the string that we want to compare (depends on fieldNo)
    if (fieldNo == 1) {
        string_field1 = record1.name;
        string_field2 = record2.name;
    }
    else if (fieldNo == 2) {
        string_field1 = record1.surname;
        string_field2 = record2.surname;
    }
    else if (fieldNo == 3) {
        string_field1 = record1.city;
        string_field2 = record2.city;
    }
    // Then compare the two strings
    if (strcmp(string_field1, string_field2) < 0)
      return -1;
    else if (strcmp(string_field1, string_field1) > 0)
      return 1;
    else
      return 0;
  }
  // In case of wrong fieldNo input
  else
    return -2;
}

// Function that returns the nth (input) record from an array of blocks (buffers)
// If n is greater than the number of records in a block, then go to the next one in the array
// N will never be out of bounds (cause quicksort)
Record* get_nth_record(char** buffer_data, int n) {
    printf("getting record with n %d\n", n);
    int buffer_i = 0;
    int found = 0;
    // Find the block where the record is, while making n in-bounds for that buffer
    while (!found) {
        int rec_number = 0; // H MPORW KAI = (int*)*buffer_data[buffer_i] ISWS KALITERO
        memcpy(&rec_number, buffer_data[buffer_i], sizeof(int));
        printf("rec number is %d \n", rec_number);
        if (n >= rec_number) {
            n = n - rec_number;
            buffer_i++;
        }
        else
            found = 1;
    }
    Record* data = buffer_data[buffer_i] + sizeof(int);
    printf("got record with id %d\n",data[n].id);
    return &data[n];
}

void record_swap(Record* a, Record* b) {
    Record t = *a;
    *a = *b;
    *b = t;
}
