#ifndef disk_operations_H
#define disk_operations_H

#include "indexing/b_plus_tree.h"

#define FREE_SLOTS_COUNT 510
#define FREE_PAGES_COUNT 1020
#define TABLE_NAME_SIZE 100

typedef struct DB_Header {
	char table_file_name[TABLE_NAME_SIZE]; // Name of current tables file where the data is stored
	Page root_page; // The root page of the b_tree
	Page next; // The pointer pointing to the next free page to allocate, only increases
	Page page_tracker; // The page id of the most recently used page from a function
	Page free_pages_head; // The page id of the head of the FreePages list
	Page free_pages_tail; // The page id of the tail of the FreePages list
	Page spare_free_page; // A page id used to store a spare FreePage
	Page free_slots_pages_head; // The page id of the head of the FreeSlotsPages list
	Page free_slots_pages_tail; // The page id of the tail of the FreeSlotsPages list
	Page current_record_page; // Last sessions record page being used
	int32_t max; // Maximum value in the b+tree
	int32_t min; // Minimum value in the b+tree
} DB_Header;

extern DB_Header* db_information;

Page allocate_page(PageType type); // Allocates a new page, or old freed page to be used, and returns page that was allocated
void free_page(Page p); // Frees a page, and adds it to the free page list of a FreePage struct
void* get_page(Page p, int type); // Returns the node thats at p
void write_page(Page p, void* n); // Writes a page back to disk

Page get_page_tracker(); // Returns page tracker
Location allocate_location(int32_t key, char* name, char* email); // Allocates a location for a given entry
Slot* get_slot(Page record_page, uint8_t slot); // Returns a pointer to the specified slot
void delete_slot_from_record(Page record_page, uint8_t slot); // Deletes a slot from the record

void start_database_page(char* file_name);
void save_database(char* file_name);

#endif
