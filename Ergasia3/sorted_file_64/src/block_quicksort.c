#include "sort_file.h"

Quicksort(A as array, low as int, high as int){
    if (low < high){
        pivot_location = Partition(A,low,high)
        Quicksort(A,low, pivot_location)
        Quicksort(A, pivot_location + 1, high)
    }
}

quicksort(char **buffer_data, int low, int high) {
    if (low < high) {
        int pivot_location = partition(buffer_data, low, high);
        Quicksort(buffer_data, low, pivot_location);
        Quicksort(buffer_data, pivot_location + 1, high);
    }
}

Partition(A as array, low as int, high as int){
     pivot = A[low]
     leftwall = low

     for i = low + 1 to high{
         if (A[i] < pivot) then{
             swap(A[i], A[leftwall])
             leftwall = leftwall + 1
         }
     }
     swap(pivot,A[leftwall])

    return (leftwall)
}

partition(char **buffer_data, int low, int high) {
     int pivot = A[low];
     int leftwall = low;

     for (int i = low + 1; i < to high) {
         if (A[i] < pivot) {
             record_swap(A[i], A[leftwall])
             leftwall = leftwall + 1
         }
     }
     record_swap(pivot,A[leftwall]);

    return leftwall;
}

void record_swap(Record* a, Record* b) {
    Record t = *a;
    *a = *b;
    *b = t;
}