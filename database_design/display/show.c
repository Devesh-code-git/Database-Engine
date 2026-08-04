#include <stdio.h>
#include "show.h"

static void print_divider();
static void print_id_divider();
static void print_name_divider();
static void print_email_divider();

static void print_id_header();
static void print_name_header();
static void print_email_header();

static void print_id(int32_t key);
static void print_name(char* name);
static void print_email(char* email);

void print_header() {
	print_divider();
	printf("\n");

	print_id_header();
	print_name_header();
	print_email_header();
	printf("\n");

	print_divider();
	printf("\n");
}

/* Functions relates to printing the actual row data */
void print_row(Slot* s) {
	if (s == NULL) { return; }
	putchar('|');

	print_id(s->id);
	print_name(s->name);
	print_email(s->email);
	printf("\n");

	print_divider();
	printf("\n");
}

static void print_id(int32_t key) {
	printf("%12d|", key);
}

static void print_name(char* name) {	
	int size = 0;

	while(name[size] != '\0') {
		size++;
	}

	/* If names size exceeds printing size, 
	   we truncate it and put ... to let the user know */
	if (size > PRINT_NAME_SIZE) {
		for (int i = 0; i < PRINT_NAME_SIZE - 3; i++) {
			char c = name[i];
			putchar(c);
		}
		printf("...|");
	}
	else {
		printf("%-20s|", name);
	}
}

static void print_email(char* email) {
	int size = 0;

	while(email[size] != '\0') {
		size++;
	}

	// Do the same thing for email as names if too long
	if (size > PRINT_EMAIL_SIZE) {
		for (int i = 0; i < PRINT_EMAIL_SIZE - 3; i++) {
			char c = email[i];
			putchar(c);
		}
		printf("...|");
	}
	else {
		printf("%-40s|", email);
	}
}

/* Functions which prints dividers between each rows */
static void print_divider() {
	// Helper functions corrisponding to the dividers to be printed
	print_id_divider();
	print_name_divider();
	print_email_divider();
}

static void print_id_divider() {
	putchar('+');

	for (int i = 0; i < 12; i++) {
		putchar('-');
	}

	putchar('+');
}

static void print_name_divider() {
	for (int i = 0; i < PRINT_NAME_SIZE; i++) {
		putchar('-');
	}
	putchar('+');
}

static void print_email_divider() {
	for (int i = 0; i < PRINT_EMAIL_SIZE; i++) {
		putchar('-');
	}
	putchar('+');
}

/* Functions related to printing the tables rows headers */
static void print_id_header() {
	printf("|\033[1;93m%-12s\033[0m|", "ID"); // Assume that id wont be higher than 12 digits,
															// as that is more than enough for int32_t
}

static void print_name_header() {
	fputs(" \033[1;93mName\033[0m", stdout);

	for (int i = 5; i < PRINT_NAME_SIZE; i++) {
		putchar(' ');
	}
	putchar('|');
}

static void print_email_header() {
	fputs(" \033[1;93mEmail\033[0m", stdout);

	for (int i = 6; i < PRINT_EMAIL_SIZE; i++) {
		putchar(' ');
	}
	putchar('|');
}
