#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "disk_operations.h"
#include "cache.h"

/* This struct is used as a way to store a list
   of pages that were freed, and not being used 
	because they would be before the next pointer, so we store
	them in disk on a page, there can be multiple of these pages in disk */
typedef struct FreePage {
	Page pages[FREE_PAGES_COUNT]; 
	Page next_free_page;
	Page prev_free_page;
	uint32_t count;
	bool dirty; // Bool used to check if needs to be written to disk or not
} FreePage;

// Same concept as the FreePage struct, but for slots in records
typedef struct FreeSlotsPage {
	Location locations[FREE_SLOTS_COUNT];
	Page next_free_slots_pages;
	Page prev_free_slots_page;
	uint32_t count;
	bool dirty;
} FreeSlotsPage;

static FreeSlotsPage* current_free_slots_page = NULL;
static FreePage* current_free_page = NULL; // Points to the current free_page thats being used
DB_Header* db_information = NULL;
static Record* current_record = NULL;
static int global_flag = 0;

static void start_global_states(char* file_name); 

/* FUNCTIONS FOR CREATING, DELETING, AND WRITING STRUCTS */

static FreeSlotsPage* create_free_slots_page_struct() {
	FreeSlotsPage* fs = malloc(PAGE_SIZE);
	if (fs == NULL) {
		perror("\033[1;31mError on malloc to freeslots \033[0m");
		return NULL;
	}

	fs->count = 0;
	fs->next_free_slots_pages = INVALID_PAGE;
	fs->dirty = true;

	Page p = db_information->next;
	db_information->next++;

	if (db_information->free_slots_pages_head == INVALID_PAGE) {
		db_information->free_slots_pages_head = p;
		db_information->free_slots_pages_tail = p;
		fs->prev_free_slots_page = INVALID_PAGE;
	}
	else {
		current_free_slots_page->next_free_slots_pages = p;
		fs->prev_free_slots_page = db_information->free_slots_pages_tail;
		db_information->free_slots_pages_tail = p;
	}

	return fs;
}

static void delete_free_slots_page_struct(FreeSlotsPage* fs) {
	if (fs == NULL) { return; }
	free(fs);
}

static void write_free_slots_page_struct(FreeSlotsPage* fs, Page p) {
	if (p == INVALID_PAGE) { return; }

	FILE* file = fopen(db_information->table_file_name, "rb+");
	if (file == NULL) {
		perror("\033[1;31mError on opening file when writing free page \033[0m");
		return;
	}

	long offset = (long)p * PAGE_SIZE;
	fseek(file, offset, SEEK_SET);

	size_t write_check = fwrite(fs, PAGE_SIZE, 1, file);
	if (write_check != 1) {
		perror("\033[1;31mError on writing free page \033[0m");
		fclose(file);
		return;
	}

	fclose(file);
}

static FreePage* create_free_page_struct() {
	// Create a new struct which represents a page that contains a list of other free pages
	FreePage* fr = malloc(PAGE_SIZE);
	if (fr == NULL) {
		perror("\033[1;31mError on malloc to freepages \033[0m");
		return NULL;
	}

	fr->count = 0;
	fr->next_free_page = INVALID_PAGE;
	fr->dirty = true;

	Page p = db_information->next;
	db_information->next++;

	// If creating for first time, head == tail
	if (db_information->free_pages_head == INVALID_PAGE) {
		db_information->free_pages_head = p;
		db_information->free_pages_tail = p;
		fr->prev_free_page = INVALID_PAGE;
	}
	else {
		current_free_page->next_free_page = p;
		fr->prev_free_page = db_information->free_pages_tail;
		db_information->free_pages_tail = p;
	}

	return fr;
}

static void delete_free_page_struct(FreePage* fr) {
	if (fr == NULL) { return; }
	free(fr);
}

static void write_free_page_struct(FreePage* fr, Page p) {
	if (p == INVALID_PAGE) { return; }

	FILE* file = fopen(db_information->table_file_name, "rb+");
	if (file == NULL) {
		perror("\033[1;31mError on opening file when writing free page \033[0m");
		return;
	}

	long offset = (long)p * PAGE_SIZE;
	fseek(file, offset, SEEK_SET);

	size_t write_check = fwrite(fr, PAGE_SIZE, 1, file);
	if (write_check != 1) {
		perror("\033[1;31mError on writing free page \033[0m");
		fclose(file);
		return;
	}

	fclose(file);
}

static void write_meta_data() {
	FILE* file = fopen(db_information->table_file_name, "rb+");
	if (file == NULL) {
		perror("\033[1;31mError on writing meta data to file, file dosent exist \033[0m");
		return;
	}

	fseek(file, 0, SEEK_SET); // Go to page 0
	size_t write_check = fwrite(db_information, sizeof(DB_Header), 1, file);
	if (write_check != 1) {
		perror("\033[1;31mError on writing metadata to file \033[0m");
		fclose(file);
		return;
	}

	fclose(file);
}	

static DB_Header* create_meta_data(char* file_name) {
	DB_Header* db = malloc(sizeof(DB_Header));
	if (db == NULL) {
		perror("\033[1;31mError on creating db_header struct \033[0m");
		return NULL;
	}

	snprintf(db->table_file_name, TABLE_NAME_SIZE, "%s", file_name);
	db->root_page = INVALID_PAGE;
	db->next = 1;
	db->page_tracker = 1;
	db->free_pages_head = INVALID_PAGE;
	db->free_pages_tail = INVALID_PAGE;
	db->spare_free_page = INVALID_PAGE;
	db->free_slots_pages_head = INVALID_PAGE;
	db->free_slots_pages_tail = INVALID_PAGE;
	db->current_record_page = INVALID_PAGE;
	db->max = 0;
	db->min = 0;
	return db;
}

static void free_meta_data_struct(DB_Header* db) {
	if (db == NULL) { return; }

	write_meta_data(); // Write to disk just incase
	free(db);
}

/* FUNCTIONS RELATED TO DISK OPERATIONS */

void start_database_page(char* file_name) {
	// If file exists, we read back the previous sessions states
	FILE* file = fopen(file_name, "rb+");
	if (file != NULL) {
		start_global_states(file_name);
		fclose(file);
		return;
	}

	// If file dosent exist, we start the global states for the first time

	file = fopen(file_name, "wb+");
	if (file == NULL) {
		perror("\033[1;31mError creating database file \033[0m");
		return;
	}

	db_information = create_meta_data(file_name);
	if (db_information == NULL) {
		fclose(file);
		return;
	}

	current_free_page = create_free_page_struct();
	if (current_free_page == NULL) {
		free(db_information);
		fclose(file);
		return;
	}
	write_free_page_struct(current_free_page, db_information->free_pages_head);

	current_free_slots_page = create_free_slots_page_struct();
	if (current_free_slots_page == NULL) {
		free(db_information);
		free(current_free_page);
		fclose(file);
		return;
	}
	write_free_slots_page_struct(current_free_slots_page, db_information->free_slots_pages_head);

	current_record = calloc(1, PAGE_SIZE);
	if (current_record == NULL) {
		free(db_information);
		free(current_free_page);
		free(current_free_slots_page);
		fclose(file);
		return;
	}
	current_record->page_id = db_information->next;
	db_information->next++;
	current_record->slot_count = 0;
	db_information->current_record_page = current_record->page_id;

	add_to_cache(true, current_record->page_id, (void *)current_record);

	fseek(file, 0, SEEK_SET);
	write_meta_data();

	fclose(file);
}

static void start_global_states(char* file_name) {
	FILE* file = fopen(file_name, "rb+");
	if (file == NULL) {
		perror("\033[1;31mError opening database file when starting global states \033[0m");
		return;
	}

	// If file exists have to read in last sessions meta data into struct
	if (db_information != NULL) {
		free(db_information);
		db_information = NULL;
	}

	if (db_information == NULL) {
		db_information = create_meta_data(file_name);
		if (db_information == NULL) {
			fclose(file);
			return;
		}

		fseek(file, 0, SEEK_SET);
		size_t read_check = fread(db_information, sizeof(DB_Header), 1, file);
		if (read_check != 1) {
			perror("\033[1;31mError on reading existing meta data \033[0m");
			fclose(file);
			free(db_information);
			return;
		}
	}

	// If file exists but current_free_pages is NULL, have to read in last sessions into struct
	// we use the tails, because we want to clear the furthest free page, before getting to head
	if (current_free_page == NULL && db_information->free_pages_tail != INVALID_PAGE) {
		current_free_page = malloc(PAGE_SIZE);
		if (current_free_page == NULL) {
			fclose(file);
			free(db_information);
			return;
		}

		Page p = db_information->free_pages_tail;
		long offset = (long)p * PAGE_SIZE;

		fseek(file, offset, SEEK_SET);
		size_t read_check = fread(current_free_page, PAGE_SIZE, 1, file);
		if (read_check != 1) {
			perror("\033[1;31mError on reading existing free page data \033[0m");
			fclose(file);
			free(db_information);
			free(current_free_page);
			return;
		}
	}

	// If file exists but current_free_slots_page is NULL, have to read in last sessions into struct
	// we use the tails, because we want to clear the furthest free slot, before getting to head
	if (current_free_slots_page == NULL && db_information->free_slots_pages_tail != INVALID_PAGE) {
		current_free_slots_page = malloc(PAGE_SIZE);
		if (current_free_slots_page == NULL) {
			fclose(file);
			free(db_information);
			free(current_free_page);
			return;
		}

		Page p = db_information->free_slots_pages_tail;
		long offset = (long)p * PAGE_SIZE;

		fseek(file, offset, SEEK_SET);
		size_t read_check = fread(current_free_slots_page, PAGE_SIZE, 1, file);
		if (read_check != 1) {
			perror("\033[1;31mError on reading existing free page data \033[0m");
			free(db_information);
			free(current_free_page);
			free(current_free_slots_page);
			fclose(file);
			return;
		}
	}

	// If current record, have to read in last sessiongs record page that was left off
	if (current_record == NULL) {
		current_record = calloc(1, PAGE_SIZE);
		if (current_record == NULL) {
			fclose(file);
			free(db_information);
			free(current_free_page);
			free(current_free_slots_page);
			return;
		}

		Page p = db_information->current_record_page;
		long offset = (long)p * PAGE_SIZE;

		fseek(file, offset, SEEK_SET);
		size_t read_check = fread(current_record, PAGE_SIZE, 1, file);
		if (read_check != 1) {
			fclose(file);
			free(db_information);
			free(current_free_page);
			free(current_free_slots_page);
			free(current_record);
			return;
		}

		add_to_cache(true, p, (void *)current_record);
	}

	fclose(file);
}

void save_database(char* file_name) {
	FILE* file = fopen(file_name, "rb+");
	Page free_tail = INVALID_PAGE;
	Page slots_tail = INVALID_PAGE;

	if (file != NULL) {
		db_information->current_record_page = current_record->page_id;
		free_tail = db_information->free_pages_tail;
		slots_tail = db_information->free_slots_pages_tail;

		if (current_free_page != NULL) {
			write_free_page_struct(current_free_page, free_tail);
			free(current_free_page);
			current_free_page = NULL;
		}

		if (current_free_slots_page != NULL) {
			write_free_slots_page_struct(current_free_slots_page, slots_tail);
			free(current_free_slots_page);
			current_free_slots_page = NULL;
		}

		write_cache_to_disk();
		write_meta_data();
		free(db_information);
		db_information = NULL;

		/* Dont have to free current_record because it would be in cache
		   and since its in cache, will get written and removed in the write_to_cache() function */
		current_record = NULL;

		fclose(file);
	}
}

Page allocate_page(PageType type) {
	Page p;

	// if we have free pages that are not the next counter, use them instead
	if (current_free_page != NULL && current_free_page->count > 0) {
		p = current_free_page->pages[current_free_page->count - 1];

		// If we have a spare free page, insert into allocated pages index
		if (db_information->spare_free_page != INVALID_PAGE) {
			current_free_page->pages[current_free_page->count - 1] = db_information->spare_free_page;
			db_information->spare_free_page = INVALID_PAGE;
		}
		else {
			current_free_page->count--;
		}

		current_free_page->dirty = true;

		if (current_free_page->count == 0) {
			// We get the previous free page to make the current free page
			Page prev = current_free_page->prev_free_page;
			long offset = (long)prev * PAGE_SIZE;

			FILE* file = fopen(db_information->table_file_name, "rb+");
			if (file == NULL) {
				perror("\033[1;31mError on opening file when allocating page \033[0m");
				return INVALID_PAGE;
			}

			fseek(file, offset, SEEK_SET);
			size_t read_check = fread(current_free_page, sizeof(FreePage), 1, file);
			if (read_check != 1) {
				perror("\033[1;31mError on reading file when removing free page \033[0m");
				fclose(file);
				return INVALID_PAGE;
			}

			/* We set the old free page, which is also the current free pages next page
			   to be the spare page, and make the next page pointer invalid */
			db_information->spare_free_page = current_free_page->next_free_page;
			current_free_page->next_free_page = INVALID_PAGE;
			fclose(file);
		}
	}
	else {
		p = db_information->next;
		db_information->next++;
	}

    // allocating a new page based on the type of page we need to allocate
    switch(type) {
        case LEAF_PAGE:
            LeafNode* n = make_leaf_node();
            if (n == NULL) { return INVALID_PAGE; }

            add_to_cache(true, p, (void *)n);
            break;

        case INTERNAL_PAGE:
            InternalNode* n_internal = make_internal_node();
            if (n_internal == NULL) { return INVALID_PAGE; }

            add_to_cache(true, p, (void *)n_internal);
            break;
    }

	db_information->page_tracker = p;
	return p;
}

void free_page(Page p) {
	if (p == INVALID_PAGE) { return; }
	/* If the page were freeing is just next - 1, 
	   then we can just move the pointer back one page instead */
	if (p == db_information->next - 1) {
		db_information->next--;
		delete_from_cache(p);
		return;
	}

	/* 1020 is the maximum amount of elements the free pages list can hold
	   otherwise the sturct will be bigger than a page size */
	if (current_free_page->count + 1 == FREE_PAGES_COUNT) {
		// Save old free page, since we have to write it back
		FreePage* old_page = current_free_page;

		// If we have a spare page available, use it instead
		if (db_information->spare_free_page != INVALID_PAGE) {
			FILE* file = fopen(db_information->table_file_name, "rb+");
			if (file == NULL) {
				perror("\033[1;31mError on opening file for spare page \033[0m");
				return;
			}

			long offset = (long)(db_information->spare_free_page) * PAGE_SIZE;
			fseek(file, offset, SEEK_SET);

			size_t read_check = fread(current_free_page, sizeof(FreePage), 1, file);
			if (read_check != 1) {
				perror("\033[1;31mError on reading file for spare page \033[0m");
				fclose(file);
				return;
			}

			db_information->spare_free_page = INVALID_PAGE;
			fclose(file);
		}
		else {
			current_free_page = create_free_page_struct(); // Set to newly allocated free page
		}

		// Use new free page to insert page that is being freed
		current_free_page->pages[0] = p;
		current_free_page->count++;

		write_free_page_struct(old_page, current_free_page->prev_free_page);
		delete_from_cache(p);
		return;
	}

	// Using current free page, since it wont go over count
	current_free_page->pages[current_free_page->count] = p;
	current_free_page->count++;
	current_free_page->dirty = true;
	delete_from_cache(p);
}

void write_page(Page p, void* n) {
	FILE* file = fopen(db_information->table_file_name, "rb+");
	if (file == NULL) {
		perror("\033[1;31mError on opening file for writing a page \033[0m");
		return;
	}

	long offset = (long)p * PAGE_SIZE;
	fseek(file, offset, SEEK_SET);

	size_t write_check = fwrite(n, PAGE_SIZE, 1, file);
	if (write_check != 1) {
		perror("\033[1;31mError on writing page back to disk \033[0m");
	}

	fclose(file);
}

void* get_page(Page p, int type) {
	if (p == INVALID_PAGE) {
		return NULL;
	}

	db_information->page_tracker = p;
	return get_from_cache(p, type);
}

Page get_page_tracker() {
	return db_information->page_tracker;
}

/* FUNCTIONS RELATED TO RECORDS AND SLOTS */

Location allocate_location(int32_t key, char* name, char* email) {
	Location l;

	FILE* file = fopen(db_information->table_file_name, "rb+");
	if (file == NULL) {
		perror("\033[1;31mError on opening file when allocating page \033[0m");
		l.record_page = 0;
		l.slot = 0;
		return l;
	}

	// Use an unsued free slot
	if (current_free_slots_page != NULL && current_free_slots_page->count > 0) {
		l = current_free_slots_page->locations[current_free_slots_page->count - 1];
		current_free_slots_page->count--;

		// If emtpy and not at head, we go to the previous page in the list
		if (current_free_slots_page->count == 0 && current_free_slots_page->prev_free_slots_page != INVALID_PAGE) {
			db_information->free_slots_pages_tail = current_free_slots_page->prev_free_slots_page; // Update tail to previous page

			Page p = db_information->free_slots_pages_tail;
			long offset = (long)p * PAGE_SIZE;
			fseek(file, offset, SEEK_SET);

			size_t read_check = fread(current_free_slots_page, PAGE_SIZE, 1, file);
			if (read_check != 1) {
				perror("\033[1;31mError on reading free slots when allocating location \033[0m");
				l.record_page = 0;
				l.slot = 0;
				return l;
			}

			free_page(current_free_slots_page->next_free_slots_pages); // Free the page that has 0 count now

			current_free_slots_page->next_free_slots_pages = INVALID_PAGE;
		}

		// Putting new slot at free location
		Record* r = (Record *)get_page(l.record_page, WRITE);
		Slot s;
		s.id = key;
		snprintf(s.name, NAME_SIZE, "%s", name);
		snprintf(s.email, EMAIL_SIZE, "%s", email);
		r->slots[l.slot] = s;

		fclose(file);
		return l;
	}

	// If no unused free slot, we use the current record pages slots

	if (current_record->slot_count == MAX_SLOTS) {
		// Write back old record_page to disk
		long offset = (long)(current_record->page_id) * PAGE_SIZE;
		fseek(file, offset, SEEK_SET);
		size_t write_check = fwrite(current_record, PAGE_SIZE, 1, file);
		if (write_check != 1) {
			perror("\033[1;31mError on writing record page back to disk \033[0m");
			l.record_page = 0;
			l.slot = 0;
			fclose(file);
			return l;
		}

		delete_from_cache(current_record->page_id);

		// Allocate new record_page
		current_record = calloc(1, PAGE_SIZE);
		current_record->page_id = db_information->next;
		db_information->next++;
		current_record->slot_count = 0;
		db_information->current_record_page = current_record->page_id;

		add_to_cache(true, current_record->page_id, (void *)current_record);
	}

	uint8_t slot_number = current_record->slot_count;
	l.record_page = current_record->page_id;
	l.slot = current_record->slot_count;
	Slot s;
	s.id = key;
	snprintf(s.name, NAME_SIZE, "%s", name);
	snprintf(s.email, EMAIL_SIZE, "%s", email);
	current_record->slots[l.slot] = s;
	current_record->slot_count++;

	fclose(file);
	return l;
}

Slot* get_slot(Page record_page, uint8_t slot) {
	Record* r = (Record *)get_page(record_page, READ);

	if (r == NULL) {
		return NULL;
	}

	return &r->slots[slot];
}

void delete_slot_from_record(Page record_page, uint8_t slot) {
	Location l = {record_page, slot}; // Create a location struct
	uint32_t* count = &current_free_slots_page->count;

	// If adding goes over count, have to allocate a new page for list
	if ((*count) + 1 > FREE_SLOTS_COUNT) {
		FreeSlotsPage* old_page = current_free_slots_page;

		current_free_slots_page = create_free_slots_page_struct();
		current_free_slots_page->locations[0] = l;
		current_free_slots_page->count++;

		write_free_slots_page_struct(old_page, current_free_slots_page->prev_free_slots_page); // Write back previous page to disk

		return;
	}

	// Use current if it wont go over count
	current_free_slots_page->locations[*count] = l;
	(*count)++;
}
