#ifndef cache_H
#define cache_H

#include "disk_operations.h"

/* The cache stores 128MB of pages, and each page is 4KB
   so 128MB / 4096B = 32,768 pages in cache */
#define BUFFER_POOL_SIZE 32768

// Adds to cache, if theres no room, performs the clock algoritham
void add_to_cache(bool new, Page p, void* n);
void delete_from_cache(Page p); // Removes a frame from cache, specfically used when freeing a page, not used with the clock algoritham
void* get_from_cache(Page p, int type); // Gets a frame from cache, if not in cache, gets it from disk and add page to cache
void write_cache_to_disk(); // Writes any dirty frames back to disk, then deletes the cache and all of its contents

#endif
