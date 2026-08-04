#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cache.h"

typedef struct Frame {
	Page page_id; // Page id of node in disk
	void* n; // Can be a record or internal or leaf page
	int bit; // This cache uses the clock algoritham, bit indicates if frame has been acessed recently
	int dirty; // If node is updated, frame becomes dirty
} Frame;

typedef struct BufferPool {
	int hand; // The hand pointing to last replaced frame, starts at 0 when struct is created
	int count;
	Frame* frames[BUFFER_POOL_SIZE]; // Frames in RAM
} BufferPool;

static bool in_cache(Page p);

static BufferPool* cache = NULL;

static Frame* create_frame(bool new, Page p, void* n) {
	Frame* fr = malloc(sizeof(Frame));
	if (fr == NULL) {
		perror("\033[1;31mError on malloc for frame \033[0m");
		return NULL;
	}

	fr->page_id = p;
	fr->n = n;
	fr->bit = 1; // Automatically set to one when creating new frame
	fr->dirty = new;

	return fr;
}

static void delete_frame(Frame* fr) {
	if (fr == NULL) {
		return;
	}

	free(fr->n); // n's a void*, so we free it as well
	free(fr);
}

static BufferPool* create_buffer_pool() {
	BufferPool* buf = malloc(sizeof(BufferPool));
	if (buf == NULL) {
		perror("\033[1;31mError on malloc for buffer pool \033[0m");
		return NULL;
	}

	buf->hand = 0;
	buf->count = 0;

	for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
		buf->frames[i] = NULL;
	}

	return buf;
}

/* The new parameter tells us if we are adding a competly new Page to cache
   or if we are adding an existing Page into cache 
	if its new we have to mark it dirty, because it was never written 
	while an existing Page only gets marked as dirty if we write to it */
void add_to_cache(bool new, Page page, void* n) {
	if (cache == NULL) { cache = create_buffer_pool(); }
	if (in_cache(page)) { 
		free(n); 
		return; 
	} // Dont insert duplicate cache

	if (cache->count + 1 <= BUFFER_POOL_SIZE) {
		cache->frames[cache->count] = create_frame(new, page, n);
		if (cache->frames[cache->count] == NULL) {
			return;
		}

		cache->count++;
		return;
	}

	/* If adding a page makes overflow occur,
	   we start at where the hand left off, and loop until we find an unused page */
	Frame* fr;
	while(1) {
		// If we reach the end of the list, wrap back to the beggining
		if (cache->hand == BUFFER_POOL_SIZE) {
			cache->hand = 0;
		}

		fr = cache->frames[cache->hand];

		if (fr->bit > 0) { // If page was recently used, we halve the bit and skip it
			fr->bit >>= 1;
			cache->hand++;
		}
		else {
			break;
		}
	}

	Frame* new_frame = create_frame(new, page, n);
	if (new_frame == NULL) {
		perror("\033[1;31mError on creating new frame when adding to cache\033[0m");
		free(n);
		return;
	}

	// If frame has been updated, we write it back before letting it go
	if (fr->dirty == true) {
		write_page(fr->page_id, fr->n);
	}
	delete_frame(fr);

	cache->frames[cache->hand] = new_frame;

	cache->hand++;
	if (cache->hand == BUFFER_POOL_SIZE) {
		cache->hand = 0;
	}
}

void delete_from_cache(Page p) {
	if (cache == NULL) { cache = create_buffer_pool(); }

	for (int i = 0; i < cache->count; i++) {
		Frame* fr = cache->frames[i];

		if (fr->page_id == p) {
			delete_frame(fr);

			// Shift over remaning pages left after removing p
			for (int j = i; j < cache->count - 1; j++) {
				Frame* temp = cache->frames[j];
				cache->frames[j] = cache->frames[j + 1];
				cache->frames[j + 1] = temp;
			}

			cache->frames[cache->count - 1] = NULL;

			cache->count--;
			break;
		}
	}
}

// Checks if page is already in cache
static bool in_cache(Page p) {
	for (int i = 0; i < cache->count; i++) {
		Frame* fr = cache->frames[i];

		if (fr->page_id == p) {
			return true;
		}
	}

	return false;
} 

void* get_from_cache(Page p, int type) {
	if (cache == NULL) { cache = create_buffer_pool(); }	

	for (int i = 0; i < cache->count; i++) {
		Frame* fr = cache->frames[i];

		if (fr->page_id == p) {
			// If writing to page, mark it as dirty
			if (type == WRITE) {
				fr->dirty = true;
			}

			fr->bit++; // Setting the bit
			return fr->n;
		}
	}

	// If page not in cache, have to get from disk and add to cache
	void* n = malloc(PAGE_SIZE);
	if (n == NULL) {
		perror("\033[1;31mError on malloc for n when getting from cache");
		return NULL;
	}

	FILE* file = fopen(db_information->table_file_name, "rb+");
	if (file == NULL) {
		perror("\033[1;31mError on opening file when getting from cache \033[0m");
		free(n);
		return NULL;
	}

	long offset = (long)p * PAGE_SIZE;
	fseek(file, offset, SEEK_SET);

	size_t read_check = fread(n, PAGE_SIZE, 1, file);
	if (read_check != 1) {
		perror("\033[1;31mError on reading node when getting from cache \033[0m");
		free(n);
		fclose(file);
		return NULL;
	}

	add_to_cache(false, p, n);
	fclose(file);

	for (int i = 0; i < cache->count; i++) {
		Frame* fr = cache->frames[i];

		if(cache->frames[i]->page_id == p) {
			if (type == WRITE) {
				cache->frames[i]->dirty = true;
			}

			cache->frames[i]->bit = 1;
        	return cache->frames[i]->n;
		}
	}

	return NULL;
}

void write_cache_to_disk() {
	if (cache == NULL) { return; }

	FILE* file = fopen(db_information->table_file_name, "rb+");
	if (file == NULL) {
		perror("\033[1;31mError on writing whole cache back to disk \033[0m");
		return;
	}

	for (int i = 0; i < cache->count; i++) {
		Frame* fr = cache->frames[i];

		if (fr->dirty != true) {
            delete_frame(fr);
			cache->frames[i] = NULL;
			continue;
		}

		Page p = fr->page_id;
		void* n = fr->n;
		long offset = (long)p * PAGE_SIZE;

		fseek(file, offset, SEEK_SET);
		size_t write_check = fwrite(n, PAGE_SIZE, 1, file);
		if (write_check != 1) {
			perror("\033[1;31mError on writing node from whole cache \033[0m");
			fclose(file);
			return;
		}

        delete_frame(fr);
		cache->frames[i] = NULL;
	}

	cache->count = 0;
	cache->hand = 0;

	if (cache != NULL) {
        free(cache);
        cache = NULL;
    }
    
	fclose(file);
}
