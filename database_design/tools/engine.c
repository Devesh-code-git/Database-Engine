#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "cursor.h"
#include "display/show.h"
#include "engine.h"

Table* create_table(char* name) {
	int i = 0;
	while (name[i] != '\0') {
		i++;

		if (i >= TABLE_NAME_SIZE - 4) {
			printf("\033[1;31mTable name too long \033[0m");
			return NULL;
		}
	}

	Table* t = malloc(sizeof(Table));
	if (t == NULL) {
		perror("\033[1;31mError on malloc for table \033[0m");
		return NULL;
	}

	t->b_tree = btree_create_tree();
	if (t->b_tree == NULL) {
		free(t);
		return NULL;
	}

	snprintf(t->path, TABLE_NAME_SIZE, "%s", name); // Add the tables file name

	printf("\033[1;32mTable created successfully\033[0m\n");
	return t;
}

Table* load_table(Page p, char* name) {
	int i = 0;
	while (name[i] != '\0') {
		i++;

		if (i >= TABLE_NAME_SIZE - 4) {
			printf("\033[1;31mTable name too long \033[0m");
			return NULL;
		}
	}

	Table* t = malloc(sizeof(Table));
	if (t == NULL) {
		perror("\033[1;31mError on malloc for table \033[0m");
		return NULL;
	}

	t->b_tree = start_btree(p);
	if (t->b_tree == NULL) {
		free(t);
		return NULL;
	}

	snprintf(t->path, TABLE_NAME_SIZE, "%s", name); // Add the tables file name
	return t;
}

void table_insert(Table* t, int32_t key, char* name, char* email) {
	// Find a location in disk to put data
	Location l = allocate_location(key, name, email);
	if (l.record_page == INVALID_PAGE) { return; }

	bool verify = btree_insert_key(t->b_tree, key, l);
	if (!verify) {
		printf("\033[1;31mCould not insert, ID might already exist \033[0m\n");
	}
}

void table_update(Table* t, int32_t key, char* name, char* email) {
	Slot* s = btree_search_entry(t->b_tree, key);
	if (s == NULL) {
		printf("\033[1;31mCould not find entry to update \033[0m\n");
		return;
	}

	if (name != NULL) {
		snprintf(s->name, NAME_SIZE, "%s", name);
	}

	if (email != NULL) {
		snprintf(s->email, EMAIL_SIZE, "%s", email);
	}
}

void table_remove(Table* t, int32_t key) {
	bool verify = btree_delete_key(t->b_tree, key);
	if (!verify) {
		printf("\033[1;31mCould not remove, ID might not exist\033[0m\n");
	}
}

void table_search(Table* t, int32_t key) {
	Slot* s = btree_search_entry(t->b_tree, key);
	print_header();
	print_row(s);
}

void table_range_search(Table* t, int32_t lower_bound, int32_t upper_bound, int type, int flag) {	
	Slot* s = NULL;
	int32_t i = 0;

	if (upper_bound < lower_bound) {
		print_header();
		return;
	}

	void* page = get_page(t->b_tree->root_page, READ);
	Header* header = (Header *)page;
	// First check that the root node is not an empty leaf
	if (header->type == LEAF_PAGE) {
		LeafNode* n = (LeafNode *)page;

		if (n->count == 0) {
			print_header();
			return;
		}
	}

	/* Lower or upper bound key might not exist, so we loop while increasing or decreasing the
	   key value by 1 until we find a leaf node which we then set the cursors node to */
	while(1) {
		if (type == ASC) { // Ascending
			s = btree_search_entry(t->b_tree, lower_bound + i);
		}
		else if (type == DESC) { // Descending
			s = btree_search_entry(t->b_tree, upper_bound - i);
		}

		if (s != NULL) { break; }

		i++;

		// If while looping we go past the bounds, no slot exists between them
		if (type == ASC && lower_bound + i > upper_bound && flag == LIMIT) {
			s = NULL;
			break;
		}
		else if (type == DESC && upper_bound - i < lower_bound && flag == LIMIT) {
			s = NULL;
			break;
		}
	}

	print_header();

	// If no values in the bound, we return after printing the header
	if (s == NULL) {
		return;
	}

	print_row(s);

	if (type == ASC) {
		iterate_cursor_ascending(upper_bound, flag);
	}
	else if (type == DESC) {
		iterate_cursor_descending(lower_bound, flag);
	}
}
