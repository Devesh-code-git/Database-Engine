#ifndef engine_H
#define engine_H

#include "indexing/b_plus_tree.h"
#include "disk_design/disk_operations.h"

// Struct represents a user database table
// include important information for the table
typedef struct Table {
	B_Tree* b_tree;
	char path[TABLE_NAME_SIZE];
} Table;

Table* create_table(char* name);
Table* load_table(Page p, char* name);

// Refined API'S to perform operations on a table
void table_insert(Table* t, int32_t key, char* name, char* email);
void table_remove(Table* t, int32_t key);
void table_update(Table* t, int32_t key, char* name, char* email);
void table_search(Table* t, int32_t key);
void table_range_search(Table* t, int32_t lower_bound, int32_t upper_bound, int type, int flag);

#endif
