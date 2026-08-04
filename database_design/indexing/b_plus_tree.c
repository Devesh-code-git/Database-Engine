#include <stdlib.h>
#include "b_plus_tree.h"
#include "disk_design/disk_operations.h"
#include "tools/cursor.h"

/* FUNCTION DECLARATIONS */

static bool inserting_first_time(B_Tree* b);
static void leaf_key_insert_sort(Entry* entries, Entry e, uint32_t* size);
static void internal_key_insert_sort(int32_t* keys, int32_t key, uint32_t* size);
static void btree_leaf_split(B_Tree* b, LeafNode* n, Page n_page);
static void btree_internal_split(B_Tree* b, InternalNode* n, Page n_page);
static void btree_leaf_remove_helper(B_Tree* b, LeafNode* n, Page n_page);
static void btree_internal_remove_helper(B_Tree* b, InternalNode* n, Page n_page);

//-----------------------------------------------------------------------------

/* FUNCTIONS RELATED TO CREATION AND DELETION */

InternalNode* make_internal_node() {
	InternalNode* n = calloc(1, PAGE_SIZE);
	if (n == NULL) {
		perror("\033[1;31mError on malloc to internal node \033[0m");
		return NULL;
	}

	n->header.type = INTERNAL_PAGE;
	n->parent = INVALID_PAGE;
	n->count = 0;

	for (int i = 0; i < INTERNAL_MAX + 2; i++) {
		n->pages[i] = INVALID_PAGE;
	}

	return n;
}

LeafNode* make_leaf_node() {
	LeafNode* n = calloc(1, PAGE_SIZE);
	if (n == NULL) {
		perror("\033[1;31mError on malloc to leaf node \033[0m");
		return NULL;
	}

	n->header.type = LEAF_PAGE;
	n->parent = INVALID_PAGE;
	n->count = 0;
	n->next_leaf = INVALID_PAGE;
	n->prev_leaf = INVALID_PAGE;

	return n;
}

static void delete_leaf_node(LeafNode* n, Page n_page) {
	if (n == NULL) { return; }
	free_page(n_page);
}

/* The reason we dont delete any of the internal nodes children, is because
   in this code base, we only call delete node on a node thats been merged
	so its children have been moved over as well, so we only free the base of this node */
static void delete_internal_node(InternalNode* n, Page n_page) {
	if (n == NULL) { return; }
	free_page(n_page);
}

Entry create_entry(int32_t key, Page p, uint32_t slot) {
	Entry e = {key, p, slot};
	return e;
}

B_Tree* btree_create_tree() {
	B_Tree* b = malloc(sizeof(B_Tree));
	if (b == NULL) {
		perror("\033[1;31mError on malloc to b-tree \033[0m");
		return NULL;
	}

	Page p = allocate_page(LEAF_PAGE);
	if (p == INVALID_PAGE) {
		free(b);
		return NULL;
	}

	b->root_page = p;
	db_information->root_page = p;
	return b;
}

B_Tree* start_btree(Page p) {
	B_Tree* b = malloc(sizeof(B_Tree));
	if (b == NULL) {
		perror("\033[1;31mError on malloc to b-tree \033[0m");
		return NULL;
	}

	b->root_page = p;
	return b;
}

void btree_delete_tree(B_Tree* b) {
	if (b == NULL) { return; }
	free(b);
}

//-----------------------------------------------------------------------------

/* API'S TO USE THE B+TREE */

bool btree_insert_key(B_Tree* b, int32_t key, Location l) {
	if (b == NULL) { return false; }

	Entry e = create_entry(key, l.record_page, l.slot);

	Page p = b->root_page;

	void* page = get_page(b->root_page, READ);
	Header* header = (Header *)page;
	int exit_loop = 0;

	while(1) {
		if (exit_loop) { break; }

		switch (header->type) {
			case LEAF_PAGE: // If its a leaf node, we insert the key
			{
				LeafNode* n = (LeafNode* )get_page(p, WRITE);

				// No duplicates
				for (int i = 0; i < n->count; i++) {
					int32_t k = n->entries[i].key;
					if (key == k) {
						return false;
					}
				}
 
				leaf_key_insert_sort(n->entries, e, &n->count);
				if (n->count > LEAF_MAX) {
					btree_leaf_split(b, n, p);
				}

				// MIN - MAX operations
				if (inserting_first_time(b)) {
					db_information->max = key;
					db_information->min = key;
				}
				else if (key > db_information->max) {
					db_information->max = key;
				}
				else if (key < db_information->min) {
					db_information->min = key;
				}

				exit_loop = 1;
				break;
			}

			case INTERNAL_PAGE: // If its a internal node, we continue travesing throught the tree
			{
				InternalNode* n_internal = (InternalNode *)page;
				int flag = 0;

				for (int i = 0; i < n_internal->count; i++) {
					int32_t k = 0; int32_t prev = 0;
					k = n_internal->keys[i];

					if (i != 0) { prev = n_internal->keys[i - 1]; }

					if (i == 0 && key < k) {
						p = n_internal->pages[0];
						flag = 1;
					}

					if (i == n_internal->count - 1 && key >= k) { // Greater then all the keys or equal to greatest
						p = n_internal->pages[n_internal->count];
						flag = 1;
					}

					if (i != 0 && key >= prev && key < k) { // In between or equal to sepreator key
						p = n_internal->pages[i];
						flag = 1;
					}

					if (flag) {
						page = get_page(p, READ);
						header = (Header *)page;
						break;
					}
				}

				break;
			}
		}
	}

	return true;
}

Slot* btree_search_entry(B_Tree* b, int32_t key) {
	if (b == NULL) { return NULL; }

	Page p = b->root_page;

	void* page = get_page(p, READ);
	Header* header = (Header *)page;

	while(1) {
		switch(header->type) {
			case LEAF_PAGE:
			{
				LeafNode* n = (LeafNode *)get_page(p, WRITE);

				for (int i = 0; i < n->count; i++) {
					if (n->entries[i].key == key) {
						set_cursor(p, i);
						return get_slot(n->entries[i].record_page, n->entries[i].slot);
					}
				}

				return NULL; // If loop finishes, key wasent found, return NULL
				break;
			}

			case INTERNAL_PAGE:
			{
				InternalNode* n_internal = (InternalNode *)page;
				int flag = 0;

				for (int i = 0; i < n_internal->count; i++) {
					int32_t k = 0; int32_t prev = 0;
					k = n_internal->keys[i];

					if (i != 0) { prev = n_internal->keys[i - 1]; }

					if (i == 0 && key < k) {
						p = n_internal->pages[0];
						flag = 1;
					}

					if (i == n_internal->count - 1 && key >= k) { // Greater then all the keys or equal to greatest
						p = n_internal->pages[n_internal->count];
						flag = 1;
					}

					if (i != 0 && key >= prev && key < k) { // In between or equal to sepreator key
						p = n_internal->pages[i];
						flag = 1;
					}

					if (flag) {
						page = get_page(p, READ);
						header = (Header *)page;
						break;
					}
				}

				break;
			}
		}
	}

	// Wont reach, just to satisfy the compiler
	return NULL;
}

bool btree_delete_key(B_Tree* b, int32_t key) {
	if (b == NULL) { return false; }

	Page p = b->root_page;
	void* page = get_page(p, READ);
	Header* header = (Header*)page;
	int exit_loop = 0;

	while(1) {
		if (exit_loop) { break; }

		switch (header->type) {
			case LEAF_PAGE:
			{
				LeafNode* n = (LeafNode *)get_page(p, WRITE);
				int found = 0;
				int i = 0;

				for (; i < n->count; i++) {
					if (n->entries[i].key != key) { continue; }

					// delete entry and shift over evreything left by one from where entry was deleted
					for (int j = i; j < n->count - 1; j++) {
						Entry temp = n->entries[j];
						n->entries[j] = n->entries[j + 1];
						n->entries[j + 1] = temp;
					}

					found = 1;
					break;
				}

				if (!found) { return false; }

				// Have to delete record from disk
				Entry e = n->entries[n->count - 1]; // Use n->count - 1 because thats where the deleted entrie was moved to
				delete_slot_from_record(e.record_page, e.slot);

				n->count--;

				/* If deleted key was either the max or min value, 
				   we change the value respectively, we check for count not being 0
					because if the root node is a leaf, it can have less than MIN keys
					so if we delete the final key, we cant assign a new max or min */
				if (key == db_information->max && n->count != 0) {
					db_information->max = n->entries[n->count - 1].key;
				}
				else if (key == db_information->min && n->count != 0) {
					db_information->min = n->entries[0].key;
				}

				if (n->count < LEAF_MIN && p != b->root_page) {
					btree_leaf_remove_helper(b, n, p);
				}

				exit_loop = 1;
				break;
			}

			case INTERNAL_PAGE:
			{
				InternalNode* n_internal = (InternalNode *)page;
				int flag = 0;

				for (int i = 0; i < n_internal->count; i++) {
					int32_t k = 0; int32_t prev = 0;
					k = n_internal->keys[i];

					if (i != 0) { prev = n_internal->keys[i - 1]; }

					if (i == 0 && key < k) {
						p = n_internal->pages[0];
						flag = 1;
					}

					if (i == n_internal->count - 1 && key >= k) { // Greater then all the keys or equal to greatest
						p = n_internal->pages[n_internal->count];
						flag = 1;
					}

					if (i != 0 && key >= prev && key < k) { // In between or equal to sepreator key
						p = n_internal->pages[i];
						flag = 1;
					}

					if (flag) {
						page = get_page(p, READ);
						header = (Header *)page;
						break;
					}
				}

				break;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------

/* PRIVATE HELPER FUNCTIONS */

// Algoritham to deal with leaf nodes when their count is < than MIN
static void btree_leaf_remove_helper(B_Tree* b, LeafNode* n, Page n_page) {
	int child_idx = 0; // Index in parents pages list that is n's page
	int32_t seperator_key; // Key that seperates sibilings
	int seperator_idx;
	Entry sibiling_entry; // Entry that will be taken from sibiling
	InternalNode* parent = (InternalNode *)get_page(n->parent, WRITE);
	LeafNode* sibiling;
	Page sibiling_page;

	for (int i = 0; i < parent->count + 1; i++) {
		if (parent->pages[i] == n_page) {
			child_idx = i;
			break;
		}
	}

	int right_idx = child_idx + 1; // right sibilings index
	int left_idx = child_idx - 1; // left sibilings index

	// Taking from right sibiling
	if ((right_idx < INTERNAL_MAX + 2) && parent->pages[right_idx] != INVALID_PAGE && 
	    ((LeafNode *)get_page(parent->pages[right_idx], READ))->count > LEAF_MIN) 
	{
		sibiling = (LeafNode *)get_page(parent->pages[right_idx], WRITE);
		sibiling_entry = sibiling->entries[0];

		// Putting siblings entry in node
		leaf_key_insert_sort(n->entries, sibiling_entry, &n->count);

		// clearing gap thats at the start of sibilings entry list
		for (int i = 0; i < sibiling->count - 1; i++) {
			sibiling->entries[i] = sibiling->entries[i + 1];
		}

		sibiling->count--;

		parent->keys[child_idx] = sibiling->entries[0].key; // Right sibilings new lowest entry key becomes seperator key
		return;
	}
	// Taking from left sibiling
	else if ((left_idx >= 0) && parent->pages[left_idx] != INVALID_PAGE && 
	         ((LeafNode *)get_page(parent->pages[left_idx], READ))->count > LEAF_MIN) 
	{
		sibiling = (LeafNode *)get_page(parent->pages[left_idx], WRITE);
		sibiling_entry = sibiling->entries[sibiling->count - 1];

		// Putting sibilings entry in node
		leaf_key_insert_sort(n->entries, sibiling_entry, &n->count);

		sibiling->count--;

		parent->keys[child_idx - 1] = n->entries[0].key; // Left sibilings old highest entry key becomes the seperator key
		return;
	}

	// If above conditions not met, we merge sibilings
	
	// Right merge
	if ((right_idx < INTERNAL_MAX + 2) && parent->pages[right_idx] != INVALID_PAGE) {
		sibiling = (LeafNode *)get_page(parent->pages[right_idx], WRITE);
		sibiling_page = parent->pages[right_idx];
		seperator_idx = child_idx;

		/* Since were merging with right, left sibilings next leaf pointer updates to pointing to right sibiling
		   and right sibilings prev leaf pointer updates to pointing to left sibiling */
		if (left_idx >= 0 && parent->pages[left_idx] != INVALID_PAGE) {
			LeafNode* left_sibiling = (LeafNode *)get_page(parent->pages[left_idx], WRITE);
			left_sibiling->next_leaf = n->next_leaf;
		}
		sibiling->prev_leaf = n->prev_leaf;
	}
	// Left merge
	else {
		sibiling = (LeafNode *)get_page(parent->pages[left_idx], WRITE);
		sibiling_page = parent->pages[left_idx];
		seperator_idx = child_idx - 1;

		// Same pointer updates done as in right merge
		if (right_idx < INTERNAL_MAX + 2 && parent->pages[right_idx] != INVALID_PAGE) {
			LeafNode* right_sibiling = (LeafNode *)get_page(parent->pages[right_idx], WRITE);
			right_sibiling->prev_leaf = n->prev_leaf;
		}
		sibiling->next_leaf = n->next_leaf;
	}

	// Getting rid of gap thats at parents keys list
	for (int i = seperator_idx; i < parent->count - 1; i++) {
		parent->keys[i] = parent->keys[i + 1];
	}

	// Adding n's entries to sibiling
	for (int i = 0; i < n->count; i++) {
		leaf_key_insert_sort(sibiling->entries, n->entries[i], &sibiling->count);
	}

	// Getting rid of the gap there will be after deleting n
	for (int i = child_idx; i < parent->count; i++) {
		parent->pages[i] = parent->pages[i + 1];
	}

	parent->pages[parent->count] = INVALID_PAGE;
	parent->count--;

	delete_leaf_node(n, n_page);

	/* If parent is root and has no keys, that means it only has one children
	   so we promote last child to root */
	if (parent->parent == INVALID_PAGE && parent->count == 0) {
		b->root_page = sibiling_page;
		db_information->root_page = sibiling_page;
		parent->pages[0] = INVALID_PAGE;
		delete_internal_node(parent, sibiling->parent);
		sibiling->parent = INVALID_PAGE;
		return;
	}

	if (parent->count < INTERNAL_MIN && parent->parent != INVALID_PAGE) {
		btree_internal_remove_helper(b, parent, sibiling->parent);
	}
}

// The internal algoritham is the same as leaf, except that when taking from a sibiling we have to move children as well
static void btree_internal_remove_helper(B_Tree* b, InternalNode* n, Page n_page) {
	int child_idx = 0; // n's child index in parent
	int32_t seperator_key = 0;
	int seperator_idx;
	int32_t sibiling_key = 0;
	InternalNode* sibiling; // Node pointer for sibiling being used
	Page sibiling_page;
	InternalNode* parent = (InternalNode *)get_page(n->parent, WRITE);

	for (int i = 0; i < parent->count + 1; i++) {
		if (parent->pages[i] == n_page) {
			child_idx = i;
			break;
		}
	}

	int right_idx = child_idx + 1;
	int left_idx = child_idx - 1;

	if ((right_idx < INTERNAL_MAX + 2) && parent->pages[right_idx] != INVALID_PAGE && 
	     ((InternalNode *)get_page(parent->pages[right_idx], READ))->count > INTERNAL_MIN) 
	{
		sibiling = get_page(parent->pages[right_idx], WRITE);
		seperator_key = parent->keys[child_idx];
		sibiling_key = sibiling->keys[0];

		parent->keys[child_idx] = sibiling_key; // Seperator key replaced by sibiling key
		internal_key_insert_sort(n->keys, seperator_key, &n->count); // Seperator key inserted into n's keys
		Page sibiling_child = sibiling->pages[0];
		n->pages[n->count] = sibiling_child; // right sibilings lowest child is n's largest

		for (int i = 0; i < sibiling->count - 1; i++) {
			sibiling->keys[i] = sibiling->keys[i + 1];
		}

		// taking the first child of sibiling, have to shift evreything left by one
		for (int i = 0; i < sibiling->count; i++) {
			sibiling->pages[i] = sibiling->pages[i + 1];
		}

		sibiling->pages[sibiling->count] = INVALID_PAGE;
		
		sibiling->count--;
		return;
	}
	// Left sibiling
	else if ((left_idx >= 0) && parent->pages[left_idx] != INVALID_PAGE && 
	         ((InternalNode *)get_page(parent->pages[left_idx], READ))->count > INTERNAL_MIN) 
	{
		sibiling = get_page(parent->pages[left_idx], WRITE);
		seperator_key = parent->keys[child_idx - 1];
		sibiling_key = sibiling->keys[sibiling->count - 1];

		parent->keys[child_idx - 1] = sibiling_key; // Seperator key replaced by sibiling key
		internal_key_insert_sort(n->keys, seperator_key, &n->count); // Seperator key inserted into n's keys
		
		// Since putting left sibilings highest child into n, have to make space for new child in n	
		for (int i = n->count; i >= 0; i--) {
			n->pages[i + 1] = n->pages[i];
		}

		n->pages[0] = sibiling->pages[sibiling->count];
		sibiling->pages[sibiling->count] = INVALID_PAGE;

		sibiling->count--;
		return;
	}

	// If above conditions not met, then merge nodes
	
	// Merge with right
	if ((right_idx < INTERNAL_MAX + 2) && parent->pages[right_idx] != INVALID_PAGE) {
		sibiling = get_page(parent->pages[right_idx], WRITE);
		sibiling_page = parent->pages[right_idx];
		seperator_key = parent->keys[child_idx];
		seperator_idx = child_idx;

		// Have to bring over n's children to right-sibiling
		for (int i = 0; i < n->count + 1; i++) {
			for (int j = sibiling->count + i; j >= 0; j--) {
				sibiling->pages[j + 1] = sibiling->pages[j];
			}

			sibiling->pages[0] = n->pages[n->count - i];
			n->pages[n->count - i] = INVALID_PAGE;
		}
	}
	// Merge with left
	else {
		sibiling = get_page(parent->pages[left_idx], WRITE);
		sibiling_page = parent->pages[left_idx];
		seperator_key = parent->keys[child_idx - 1];
		seperator_idx = child_idx - 1;

		// Appending n's children to left sibiling, if n is an internal node
		int i = 0;
		for (int j = sibiling->count + 1; j < (sibiling->count + 1 + n->count + 1); j++) {
			sibiling->pages[j] = n->pages[i];
			n->pages[i] = INVALID_PAGE;
			i++;
		}
	}

	for (int i = seperator_idx; i < parent->count - 1; i++) {
		parent->keys[i] = parent->keys[i + 1];
	}

	// Merging seperator key, and all keys in n with whichever sibiling was picked
	internal_key_insert_sort(sibiling->keys, seperator_key, &sibiling->count);
	for (int i = 0; i < n->count; i++) {
		internal_key_insert_sort(sibiling->keys, n->keys[i], &sibiling->count);
	}

	// Getting rid of the gap that is now at parents children list after removing n
	for (int i = child_idx; i < parent->count; i++) {
		parent->pages[i] = parent->pages[i + 1];
	}

	parent->pages[child_idx] = INVALID_PAGE;
	parent->count--;

	delete_internal_node(n, n_page);

	if (parent->parent == INVALID_PAGE && parent->count == 0) {
		b->root_page = sibiling_page;
		db_information->root_page = sibiling_page;
		parent->pages[0] = INVALID_PAGE;
		delete_internal_node(parent, sibiling->parent);
		sibiling->parent = INVALID_PAGE;
		return;
	}

	if (parent->count < INTERNAL_MIN && parent->parent != INVALID_PAGE) {
		btree_internal_remove_helper(b, parent, sibiling->parent);
	}
}

// Splitting algoritham for leaf nodes
static void btree_leaf_split(B_Tree* b, LeafNode* n, Page n_page) {
	int middle = (n->count) / 2;
	int32_t middle_value = n->entries[middle].key;

	// Create new right node, and allocate upper-half of n to right child
	Page right_page = allocate_page(LEAF_PAGE);
	LeafNode* right_child = (LeafNode *)get_page(right_page, WRITE);

	int i = 0;
	for (int j = middle; j < n->count; j++) {
		right_child->entries[i] = n->entries[j]; // Dont need to use key sort, becase data coming in is sorted
		i++;
	}

	right_child->parent = n->parent;

	right_child->count = i;
	n->count = n->count - right_child->count;

	right_child->next_leaf = n->next_leaf;
	n->next_leaf = right_page;
	right_child->prev_leaf = n_page;

	// First time splitting, have to make a new parent
	if (n->parent == INVALID_PAGE) {
		Page new_root_page = allocate_page(INTERNAL_PAGE);
		InternalNode* new_root = (InternalNode *)get_page(new_root_page, WRITE);

		new_root->keys[0] = middle_value;
		new_root->count++;

		new_root->pages[0] = n_page;
		new_root->pages[1] = right_page;

		n->parent = new_root_page;
		right_child->parent = new_root_page;

		b->root_page = new_root_page;
		db_information->root_page = new_root_page;
		return;
	}

	/* If not at root have to push middle value up
	   and shift over parent pages to make space for right child */
	InternalNode* parent = (InternalNode *)get_page(n->parent, WRITE);
	internal_key_insert_sort(parent->keys, middle_value, &parent->count);

	Page prev = right_page;
	for (int i = 0; i < parent->count + 2; i++) {
		if (parent->pages[i] != n_page) { continue; }

		for (int j = i + 1; j < INTERNAL_MAX + 2; j++) {
			Page temp = parent->pages[j];
			parent->pages[j] = prev;
			prev = temp;
		}

		break;
	}

	if (parent->count > INTERNAL_MAX) {
		btree_internal_split(b, parent, n->parent);
	}
}

/* Splitting algoritham for internal nodes is the same as leaf nodes, except we have to also
   split the children to their respective nodes when splitting internal nodes */
static void btree_internal_split(B_Tree* b, InternalNode* n, Page n_page) {
	int middle = (n->count) / 2;
	int32_t middle_value = n->keys[middle];

	Page right_page = allocate_page(INTERNAL_PAGE);
	InternalNode* right_child = (InternalNode *)get_page(right_page, WRITE);
	void* page;
	Header* header;

	int i = 0;
	for (int j = middle + 1; j < n->count; j++) {
		right_child->keys[i] = n->keys[j];
		right_child->pages[i] = n->pages[j];

		page = get_page(n->pages[j], WRITE);
		header = (Header *)page;

		// Switching moving nodes parent to right_child, children could be leaf nodes or internal nodes
		switch (header->type) {
			case LEAF_PAGE:
				LeafNode* n = (LeafNode *)page;
				n->parent = right_page;
				break;

			case INTERNAL_PAGE:
				InternalNode* n_internal = (InternalNode *)page;
				n_internal->parent = right_page;
				break;
		}

		n->pages[j] = INVALID_PAGE;
		i++;
	}

	right_child->pages[i] = n->pages[n->count];
	page = get_page(n->pages[n->count], WRITE);
	header = (Header *)page;

	switch (header->type) {
		case LEAF_PAGE:
			LeafNode* n = (LeafNode *)page;
			n->parent = right_page;
			break;

		case INTERNAL_PAGE:
			InternalNode* n_internal = (InternalNode *)page;
			n_internal->parent = right_page;
			break;
	}
	n->pages[n->count] = INVALID_PAGE;

	right_child->count = i;
	n->count = n->count - right_child->count - 1;

	right_child->parent = n->parent;

	// Follow the same steps as the leaf split algoritham

	if (n->parent == INVALID_PAGE) {
		Page new_root_page = allocate_page(INTERNAL_PAGE);
		InternalNode* new_root = (InternalNode *)get_page(new_root_page, WRITE);

		new_root->keys[0] = middle_value;
		new_root->count++;

		new_root->pages[0] = n_page;
		new_root->pages[1] = right_page;

		n->parent = new_root_page;
		right_child->parent = new_root_page;

		b->root_page = new_root_page;
		db_information->root_page = new_root_page;
		return;
	}

	InternalNode* parent = (InternalNode *)get_page(n->parent, WRITE);
	internal_key_insert_sort(parent->keys, middle_value, &parent->count);

	Page prev = right_page;
	for (int i = 0; i < parent->count + 2; i++) {
		if (parent->pages[i] != n_page) { continue; }

		for (int j = i + 1; j < INTERNAL_MAX + 2; j++) {
			Page temp = parent->pages[j];
			parent->pages[j] = prev;
			prev = temp;
		}

		break;
	}

	if (parent->count > INTERNAL_MAX) {
		btree_internal_split(b, parent, n->parent);
	}
}

/* Checks if were inserting a value for the first time into the tree,
   not just first time ever, but when the tree is empty and were inserting a first value,
   this functions is used for setting the min and max values up again after emptying the tree
   or if it truly is the first insertion into the tree */
static bool inserting_first_time(B_Tree* b) {
	if (b == NULL) { return false; }

	void* page = get_page(b->root_page, READ);
	Header* header = (Header *)page;

	if (header->type == INTERNAL_PAGE) {
		return false;
	}

	LeafNode* n = (LeafNode *)page;
	if (n->count > 0) { return false; }

	return true;
}

//-----------------------------------------------------------------------------

/* INSERTION FUNCTIONS */

// Inserts a entry in sorted order, based on entry's keys
static void leaf_key_insert_sort(Entry* entries, Entry e, uint32_t* size) {
	int i = *size - 1;

	while (i >= 0 && entries[i].key > e.key) {
		entries[i + 1] = entries[i];
		i--;
	}

	entries[i + 1] = e;
	*size += 1;
}

// Inserts a key in sorted order
static void internal_key_insert_sort(int32_t* keys, int32_t key, uint32_t* size) {
	int i = *size - 1;

	while (i >= 0 && keys[i] > key) {
		keys[i + 1] = keys[i];
		i--;
	}

	keys[i + 1] = key;
	*size += 1;
}
