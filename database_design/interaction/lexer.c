#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "tokenize.h"

#define NUMBER_OF_KEYWORDS 14

typedef struct {
	char* pattern;
	TokenType type;
	int length;
} KeyWord;

// A list of keywords for string comparison
static const KeyWord keywords[NUMBER_OF_KEYWORDS] = {
	{"SELECT", SELECT, 6}, {"FROM", FROM, 4},
	{"WHERE", WHERE, 5}, {"BETWEEN", BETWEEN, 7},
	{"AND", AND, 3}, {"INSERT", INSERT, 6},
	{"INTO", INTO, 4}, {"VALUES", VALUES, 6},
	{"UPDATE", UPDATE, 6}, {"SET", SET, 3},
	{"DELETE", DELETE, 6}, {"CREATE", CREATE, 6},
	{"TABLE", TABLE, 5}, {"DROP", DROP, 4}
};

// If the second final char is ';' and the final char is '\0', then were at the end
static bool at_end(char* input, int index) {
	return input[index] == ';' && input[index + 1] == '\0';
}

// Checks if a char is a number
static bool is_number(char c) {
	return c >= '0' && c <= '9';
}

// Checks if char is a alphabetical number
static bool is_alpha(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_alpha_numeric(char c) {
	return is_number(c) || is_alpha(c) || c == '_';
}

// Checks if a string is properly closed
static bool string(char* input, int* index) {
	int start = *index;

	while (1) {
		/* If null char is hit, end of the statement has been reached
		   meaning that the string was never closed */
		if (input[*index] == '\0') {
			printf("\033[1;31mString never closed properly. String started at column %d\033[0m\n", start);
			return false;
		}

		/* String is closed when reaching the end quotations, 
		   this means the string cant have inner quotations inside of it*/
		if (input[*index] == '"') {
			add_token(STRING, input, start - 1, ((*index) + 1));
			return true;
		}

		(*index)++;
	}
}

/* Function consumes numbers until either reaching a punctuation to stop
   so either, white_space or a comma, errors when reaching a char thats not part of the number pattern */
static bool number(char* input, int* index) {
	int start = *index;

	// We check if its a negative number
	if (input[start] == '-') {
		(*index)++; // increment index to start after the '-', so loop runs
	}

	while(is_number(input[*index])) {
		(*index)++;

		if (is_alpha(input[*index])) {
			printf("\033[1;31mInvalid number, remove this: %c \033[0m\n", input[*index]);
			return false;
		}
	}

	add_token(NUMBER, input, start, (*index));
	(*index)--; // If loop exits sucesfully, want to consume punctuation on next iteration
	return true;
}

// Compares if the identifier is equal to the keyword
static bool compare_strings(char* s1, char* s2, int s1_start, int s1_end, int s2_length) {
	int s1_length = s1_end - s1_start;
	if (s1_length != s2_length) { return false; }

	for (int i = 0; i < s1_length; i++) {
		if (s1[s1_start + i] != s2[i]) { return false; }
	}

	return true;
}

static bool identifier(char* input, int* index) {
	// Find end of key word, after reaching white_space
	int start = *index;
	while(is_alpha_numeric(input[*index])) {
		(*index)++;
	}

	for (int i = 0; i < NUMBER_OF_KEYWORDS; i++) {
		KeyWord k = keywords[i];

		if (compare_strings(input, k.pattern, start, (*index), k.length)) {
			add_token(k.type, input, start, (*index));
			(*index)--;
			return true;
		}
	}

	// If no key words match, its just an identifier;
	add_token(IDENTIFIER, input, start, (*index));
	(*index)--;
	return true;
}

void lex_input(char* input) {
	int i = 0;
	bool exit_loop = false;
	bool no_semi_colon = false;

	// Loops until reaching end of statement
	while(1) {
		if (exit_loop) { break; }

		if (at_end(input, i)) { break; }

		char c = input[i];
		/*Since a statment is ended with a semi-colon, it is an error to reach a null char
		  because it means the statement was never ended with a semi-colon */
		if (c == '\0') {
			printf("\033[1;31mStatment never closed with a semi-colon \033[0m\n");
			no_semi_colon = true;
			break;
		}

		switch(c) {
			// Ignore white_space
			case ' ':
			case '\t':
			case '\r':
				break;

			// Single char tokens
			case '*':
				add_token(STAR, input, i, (i + 1));
				break;
			case '(':
				add_token(LEFT_PAREN, input, i, (i + 1));
				break;
			case ')':
				add_token(RIGHT_PAREN, input, i, (i + 1));
				break;
			case '=':
				add_token(OPERATION, input, i, (i + 1));
				break;
			case '>':                      // Both < and > could be <= or >=, so have to check
				if (input[i + 1] == '=') {
					add_token(OPERATION, input, i, (i + 2));
					i++; // Consume the '='
				}
				else {
					add_token(OPERATION, input, i, (i + 1));
				}

				break;
			case '<':
				if (input[i + 1] == '=') {
					add_token(OPERATION, input, i, (i + 2));
					i++;
				}
				else {
					add_token(OPERATION, input, i, (i + 1));
				}

				break;
			case ',':
				add_token(COMMA, input, i, (i + 1));
				break;

			// Multi char tokens
			default:
				bool check = true;

				if (is_number(c) || c == '-') {
					check = number(input, &i);
				}
				else if (is_alpha(c) || c == '_') {
					check = identifier(input, &i);
				}
				else if (c == '"') {
					i++;
					check = string(input, &i);
				}
				else if (c == ';') {
					printf("\033[1;31mSemi-colon in middle of statement, should be at the end only\033[0m\n");
					exit_loop = true;
				}
				else {
					printf("\033[1;31mUnkown character, not valid for statements:\033[0m %c\n", c);
					exit_loop = true;
				}

				if (!check) { exit_loop = true; }

				break;
		}

		i++;
	}

	if (!exit_loop && !no_semi_colon) {
		perform_token_actions();
	}

	clear_tokens();
}

