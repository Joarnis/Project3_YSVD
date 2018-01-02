#include "sort_file.h"
#include "sr_utils.h"

/* 
 * Standard quicksort algorthm implemented for sorting records
 * of an input buffer block array
 * Buffer_data is the buffer array, low and high are the starting
 * and ending indexes respectively and fieldNo is the element we want
 * to sort the buffers by
 * 
 * In this implementation the last element is always picked as pivot
 */

void block_quicksort(char* buffer_data[], int fieldNo, int low, int high) {
    if (low < high) {
        int pivot_location = partition(buffer_data, fieldNo, low, high);
        // Call recursively for before and after pivot location
        quicksort(buffer_data, fieldNo, low, pivot_location - 1);
        quicksort(buffer_data, fieldNo, pivot_location + 1, high);
    }
}

void block_partition(char* buffer_data[], int fieldNo, int low, int high) {
    Record* pivot = get_nth_record(buffer_data[], high); 
     
    int leftwall = low;
    Record* leftwall_rec = get_nth_record(buffer_data[], leftwall);

    for (int i = low; i < high; i++) {
        Record curr_rec = get_nth_record(buffer_data[], i);
        if (A[i] < pivot) {
            record_swap(A[i], A[leftwall])
            leftwall = leftwall + 1
        }
    }
    record_swap(pivot,A[leftwall]);

    return leftwall;
}

// Function that returns the nth (input) record from an array of blocks (buffers) 
// If n is greater than the number of records in a block, then go to the next one in the array
// N will never be out of bounds (cause quicksort)
Record* get_nth_record(char* buffer_data[], int n) {
    int buffer_i = 0;
    bool found = false;
    // Find the block where the record is, while making n in-bounds for that buffer
    while (!found) {
        int rec_number = 0; // H MPORW KAI = (int*)*buffer_data[buffer_i] ISWS KALITERO
        memcpy(&rec_number, buffer_data[buffer_i], sizeof(int));
        if (n > rec_number) {
            n = n - rec_number;
            buffer_i++;
        }
        else
            found = true;
    }
    Record* data = buffer_data[buffer_i] + sizeof(int);
    return &data[n];
}
