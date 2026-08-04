#include <stdlib.h>
#include <stdbool.h>
#include "cursor.h"
#include "display/show.h"
#include "indexing/b_plus_tree.h"
#include "disk_design/disk_operations.h"

typedef struct Cursor {
	LeafNode* n;
	Page page_id;
	int32_t key;
	int index;
} Cursor;

Cursor* global_cursor = NULL;

static bool cursor_valid();
static void cursor_next();
static void cursor_prev(); 

static Cursor* make_cursor() {
	Cursor* c = malloc(sizeof(Cursor));
	if (c == NULL) {
		perror("\033[1;31mError on malloc to cursor \033[0m");
		return NULL;
	}

	c->n = NULL;
	c->page_id = INVALID_PAGE;
	c->key = 0;
	c->index = 0;
}

void set_cursor(Page p, int index) {
	if (global_cursor == NULL) {
		global_cursor = make_cursor();

		if (global_cursor == NULL) {
			return;
		}
	}

	global_cursor->n = (LeafNode *)get_page(p, READ);
	global_cursor->page_id = p;
	global_cursor->index = index;
	global_cursor->key = global_cursor->n->entries[global_cursor->index].key;
}

void iterate_cursor_ascending(int32_t upper_bound, int flag) {
	while(1) {
		cursor_next();
		if (!cursor_valid()) { break; }

		if (global_cursor->key > upper_bound && flag == LIMIT) {	
			break;
		}

		int index = global_cursor->index;
		Slot* s = get_slot(global_cursor->n->entries[index].record_page, global_cursor->n->entries[index].slot);
		print_row(s);
	}
}

void iterate_cursor_descending(int32_t lower_bound, int flag) {
	while(1) {
		cursor_prev();
		if (!cursor_valid()) { break; }

		if (global_cursor->key < lower_bound && flag == LIMIT) {
			break;
		}

		int index = global_cursor->index;
		Slot* s = get_slot(global_cursor->n->entries[index].record_page, global_cursor->n->entries[index].slot);
		print_row(s);
	}
}

static void cursor_next() {
	/* If were at the end of a current nodes entry list
	   move on to the next node */
	if (global_cursor->index + 1 == global_cursor->n->count) {
		Page p = global_cursor->n->next_leaf;
		global_cursor->n = (LeafNode *)get_page(p, READ);
		global_cursor->page_id = p;

		if (global_cursor->page_id != INVALID_PAGE) {
			global_cursor->index = 0; // Start of node
			global_cursor->key = global_cursor->n->entries[global_cursor->index].key;
		}
	}
	else {
		global_cursor->index++;
		global_cursor->key = global_cursor->n->entries[global_cursor->index].key;
	}
}

static void cursor_prev() {
	/* If were at the end of a current nodes entry list
	   move on to the next node */
	if (global_cursor->index - 1 < 0) {
		Page p = global_cursor->n->prev_leaf;
		global_cursor->n = (LeafNode *)get_page(p, READ);
		global_cursor->page_id = p;

		if (global_cursor->page_id != INVALID_PAGE) {
			global_cursor->index = global_cursor->n->count - 1; // End of node
			global_cursor->key = global_cursor->n->entries[global_cursor->index].key;
		}
	}
	else {
		global_cursor->index--;
		global_cursor->key = global_cursor->n->entries[global_cursor->index].key;
	}
}

// If cursor ever goes to an invalid node, this returns null
static bool cursor_valid() {
	return !(global_cursor->n == NULL) && !(global_cursor->page_id == INVALID_PAGE);
}
