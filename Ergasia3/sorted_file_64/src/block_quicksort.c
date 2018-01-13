#include <stdio.h>

#include "block_quicksort.h"
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

void block_quicksort(char** buffer_data, int fieldNo, int low, int high) {
    if (low < high) {
        printf("before pivot\n");
        int pivot_location = block_partition(buffer_data, fieldNo, low, high);
        printf("after pivot\n");
        // Call recursively for before and after pivot location
        block_quicksort(buffer_data, fieldNo, low, pivot_location - 1);
        block_quicksort(buffer_data, fieldNo, pivot_location + 1, high);
    }
}

int block_partition(char** buffer_data, int fieldNo, int low, int high) {
    Record* pivot = get_nth_record(buffer_data, high);
    int leftwall = low - 1;

    for (int i = low; i < high - 1; i++) {
        Record* curr_rec = get_nth_record(buffer_data, i);
        if (record_cmp(fieldNo, *curr_rec, *pivot) < 1) {
            leftwall++;
            Record* curr_leftwall_rec = get_nth_record(buffer_data, leftwall);
            record_swap(curr_rec, curr_leftwall_rec);
        }
    }
    leftwall++;
    Record* leftwall_rec = get_nth_record(buffer_data, leftwall);
    record_swap(pivot,leftwall_rec);

    return leftwall;
}
