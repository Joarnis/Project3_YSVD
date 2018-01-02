#include "sort_file.h"

// Compares records by comparing a specific field (input fieldNo)
// Output is similar to strcmp (only with -2 output for input errors)
int record_cmp(int fieldNo, Record record1, Record record2) {
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

void record_swap(Record* a, Record* b) {
    Record t = *a;
    *a = *b;
    *b = t;
}