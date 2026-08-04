#ifndef show_H
#define show_H

#include "indexing/b_plus_tree.h"

// Maximum sized name and email which will be printed to the terminal
#define PRINT_NAME_SIZE 20
#define PRINT_EMAIL_SIZE 40

void print_header();
void print_row(Slot* s);

#endif
