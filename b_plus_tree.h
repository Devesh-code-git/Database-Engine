#ifndef b_plus_tree_H
#define b_plus_tree_H

#include <stdint.h>

#define PAGE_SIZE 4096 // 4K block on disk
#define NAME_SIZE 64 // A name can only be this long in bytes
#define EMAIL_SIZE 128 // A email can only be this long in bytes

#define INTERNAL_MAX 508 // Maximum amount of keys an internal node can have
#define INTERNAL_MIN 254
#define LEAF_MAX 339 // Maximum amount of keys a leaf node can have
#define LEAF_MIN 169

#define INVALID_PAGE 0

#define READ 0
#define WRITE 1

#define MAX_SLOTS 20 // The amount of slots a record page can hold without being > PAGE_SIZE

typedef uint32_t Page;

typedef enum {
	INTERNAL_PAGE,
	LEAF_PAGE
} PageType;

// Struct at the top of the page to tell us what page type it is
typedef struct {
	PageType type;
} Header;

typedef struct {
	Page record_page;
	uint8_t slot;
} Location;

// An entry tells us the location of the row in disk
typedef struct {
	int32_t key;
	Page record_page;
	uint8_t slot;
} Entry;

typedef struct {
	Header header;

	Page parent;
	uint32_t count;

	Entry entries[LEAF_MAX + 1];

	Page next_leaf; // Points to leaf node thats to the right of it
} LeafNode;

typedef struct {
	Header header;

	Page parent;
	uint32_t count;

	int32_t keys[INTERNAL_MAX + 1];
	Page pages[INTERNAL_MAX + 2]; // The children of the internal node
} InternalNode;

// Slot struct holds actuall row data for the database
typedef struct {
	int32_t id;
	char name[NAME_SIZE];
	char email[EMAIL_SIZE];
} Slot;

// A type of page in disk which holds multiple rows(i.e slots)
typedef struct {
	Page page_id;
	uint8_t slot_count;
	Slot slots[20];
} Record;

typedef struct {
	Page root_page;
} B_Tree;

InternalNode* make_internal_node();
LeafNode* make_leaf_node();
B_Tree* btree_create_tree();
B_Tree* start_btree(Page p);
void btree_delete_tree(B_Tree* b);

bool btree_insert_key(B_Tree* b, int32_t key, Location l);
Slot* btree_search_entry(B_Tree* b, int32_t key); // search returns slot, so we can use the data right away
bool btree_delete_key(B_Tree* b, int32_t key);

#endif
