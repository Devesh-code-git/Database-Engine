#ifndef cursor_H
#define cursor_H

#include "indexing/b_plus_tree.h"
#include <stdint.h>

// Flags related to knowing what type of iteration is needed
#define ASC 0
#define DESC 1
#define LIMIT 0 // If there is a bound when using (BETWEEN _ AND _)
#define NO_LIMIT 1 // when using comparators like <, >, <=, >=, there is no stopping bound

typedef struct Cursor Cursor;

/* The cursor struct is only used to speed up ranged queries by the user
   its main purpose is to save the first leaf node it visits and traverse 
	the other leaf nodes from there until hitting a limit */
void set_cursor(Page p, int index); // This sets the cursor to the current leaf node given
void iterate_cursor_ascending(int32_t upper_bound, int flag); // For trvaeling through the leaf nodes in ascending order
void iterate_cursor_descending(int32_t lower_bound, int flag); // For traveling through the leaf nodes in descending order

#endif
