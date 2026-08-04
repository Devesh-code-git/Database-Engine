#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "tokenize.h"
#include "tools/engine.h"
#include "tools/cursor.h"

#define MAX_TOKENS_LENGTH 100

typedef enum {
	LESS,
	GREATER,
	LESS_EQUAL,
	GREATER_EQUAL,
	EQUAL
} OperationType;

typedef struct {
	TokenType type; // The type of token
	char* pattern; // Is the whole input line         
	int start; // Start of pattern in the input line
	int end; // End of pattern in the input line, exclusive not 0th index
} Token;

// A list of the tokens for the current statement
typedef struct {
	Token tokens[MAX_TOKENS_LENGTH];
	int count;
} TokenList;

/* Since my SQL type language is very limited
   instead of writing a whole recurive descent parser
	I have hard coded the different valid statements to match with instead. 
	The first element is the length of the statment, for quick comparisons 
	the second element is the index starting from 0 of which token holds the file name */
static int s1[] = {4, 3, SELECT, STAR, FROM, IDENTIFIER};
static int s2[] = {8, 3, SELECT, STAR, FROM, IDENTIFIER, WHERE, IDENTIFIER, OPERATION, NUMBER};
static int s3[] = {10, 3, SELECT, STAR, FROM, IDENTIFIER, WHERE, IDENTIFIER, BETWEEN, NUMBER, AND, NUMBER};

static int s4[] = {11, 2, INSERT, INTO, IDENTIFIER, VALUES, LEFT_PAREN, NUMBER, COMMA, STRING, COMMA, STRING, RIGHT_PAREN};

static int s5[] = {10, 1, UPDATE, IDENTIFIER, SET, IDENTIFIER, OPERATION, STRING, WHERE, IDENTIFIER, OPERATION, NUMBER};
static int s6[] = {14, 1, UPDATE, IDENTIFIER, SET, IDENTIFIER, OPERATION, STRING, COMMA, IDENTIFIER, OPERATION, STRING, WHERE, IDENTIFIER, OPERATION, NUMBER};

static int s7[] = {7, 2, DELETE, FROM, IDENTIFIER, WHERE, IDENTIFIER, OPERATION, NUMBER};

static int s8[] = {3, 2, CREATE, TABLE, IDENTIFIER};
static int s9[] = {3, 2, DROP, TABLE, IDENTIFIER};
//------------------------------------------------------------------

static int* statements_list[10] = {s1, s2, s3, s4, s5, s6, s7, s8, s9};
static TokenList* List = NULL;

static void print_type(TokenType type);
static void print_pattern(char* input, int start, int end);

void add_token(TokenType type, char* input, int start, int end) {
	if (List == NULL) {
		List = malloc(sizeof(TokenList));
		if (List == NULL) {
			perror("\033[1;31mError on malloc to TokenList \033[0m\n");
			return;
		}

		List->count = 0;
	}

	if (List->count == MAX_TOKENS_LENGTH) {
		printf("\033[1;31mStatement is too long, has too many tokens \033[0m\n");
		return;
	}

	Token t = {type, input, start, end};
	List->tokens[List->count] = t;
	List->count++;
}

void clear_tokens() {
	if (List == NULL) { return; }

	free(List);
	List = NULL;
}

static int compare_token_list() {
	for (int i = 0; i < 9; i++) {
		/* If a tokenLists count is not equal to a statements count
		   then it skips comparing with it */
		if (List->count != statements_list[i][0]) {
			continue;
		}

		int* statement = statements_list[i];
		int index = 0;
		bool flag = true; // Assume it matches from the start

		for (int j = 2; j < statement[0] + 2; j++) { // First 2 elements are meta_data
			if (List->tokens[index].type != statement[j]) {
				flag = false; // Dosent match
				break;
			}

			index++;
		}

		if (flag) { return i; }
	}

	return -1;
}

// Checks if two strings are equal
static bool match(char* s1, char* s2, int s1_start, int s1_end, int s2_length) {
	if ((s1_end - s1_start) != s2_length) {
		return false;
	}

	int j = 0;
	for (int i = s1_start; i < s1_end; i++) {
		if (s1[i] != s2[j]) { return false; }
		j++;
	}

	return true;
}

// Checks what type of operation the user picked and returns it
static OperationType get_operation_type(Token t) {
	int length = t.end - t.start;

	if (length == 1) {
		if (t.pattern[t.start] == '=') {
			return EQUAL;
		}
		else if (t.pattern[t.start] == '<') {
			return LESS;
		}

		return GREATER;
	}

	if (t.pattern[t.start] == '<' && t.pattern[t.start + 1] == '=') {
		return LESS_EQUAL;
	}

	return GREATER_EQUAL;
}

/* Returns the number the user entered 
   very basic function, dosent saftey check for a number being bigger
	or smaller than a int32_t can repsent, assume user knows the limits
	and they wont need more than around (+,-) 2.1 billion ids */
static int32_t get_number(Token t) {
	char* start = t.pattern + t.start;
	char* end;

	int32_t final_value = (int32_t)strtol(start, &end, 10);
	return final_value;
}

// Saves the contents of the table and quits
static void exit_table(Table* t) {
	save_database(t->path);
	free(t->b_tree);
	free(t);
}

/* These functions are helpers to use
   engine.h functions to perform table operations
	based on what the user entered */
static void selection(int statement_number, char* file_name) {
	start_database_page(file_name);
	Table* t = load_table(db_information->root_page, file_name);

	if (statement_number == 0) { // No WHERE clause
		table_range_search(t, db_information->min, 0, ASC, NO_LIMIT);
	}
	else if (statement_number == 1) { // WHERE OPERATION statement
		Token id = List->tokens[5];
		if (match(id.pattern, "id", id.start, id.end, 2) == false) {
			printf("\033[1;31mColumn name should be id\033[0m\n");
			exit_table(t);
			return;
		}

		OperationType o = get_operation_type(List->tokens[6]);
		int32_t num = get_number(List->tokens[7]);

		switch(o) {
			case EQUAL:
				table_search(t, num);
				break;
			case LESS_EQUAL:
				table_range_search(t, db_information->min, num, ASC, LIMIT);
				break;
			case GREATER_EQUAL:
				table_range_search(t, num, db_information->max, ASC, LIMIT);
				break;
			case LESS:	
				table_range_search(t, db_information->min, num - 1, ASC, LIMIT);
				break;
			case GREATER:
				table_range_search(t, num + 1, db_information->max, ASC, LIMIT);
				break;
		}
	}
	else { // WHERE BETWEEN AND statement
		Token id = List->tokens[5];
		if (match(id.pattern, "id", id.start, id.end, 2) == false) {
			printf("\033[1;31mColumn name should be id\033[0m\n");
			exit_table(t);
			return;
		}

		int32_t n1 = get_number(List->tokens[7]);
		int32_t n2 = get_number(List->tokens[9]);

		// If higher number entered first, then descending range search is used
		if (n1 <= n2) {
			table_range_search(t, n1, n2, ASC, LIMIT);
		}
		else {
			table_range_search(t, n2, n1, DESC, LIMIT);
		}
	}

	exit_table(t);
}

static void insertion(char* file_name) {
	Token name_token = List->tokens[7];
	Token email_token = List->tokens[9];

	start_database_page(file_name);
	Table* t = load_table(db_information->root_page, file_name);

	int32_t key = get_number(List->tokens[5]);
	char name[NAME_SIZE];
	char email[EMAIL_SIZE];

	// If name or email entered is too long, will stop at char that reaches max value
	int j = 0;
	for (int i = name_token.start + 1; i < name_token.end - 1; i++) {
		name[j] = name_token.pattern[i];
		j++;

		if (j == NAME_SIZE - 1) { break; }
	}
	name[j] = '\0';

	j = 0;
	for (int i = email_token.start + 1; i < email_token.end - 1; i++) {
		email[j] = email_token.pattern[i];
		j++;

		if (j == EMAIL_SIZE - 1) { break; }
	}
	email[j] = '\0';

	table_insert(t, key, name, email);

	exit_table(t);
}

static void updates(int statement_number, char* file_name) {
	start_database_page(file_name);
	Table* t = load_table(db_information->root_page, file_name);

	Token id;
	OperationType where_operation;

	/* Regardless of what type of statement
	   the WHERE clause should be "id" and "=" */
	if (statement_number == 4) {
		id = List->tokens[7];
		where_operation = get_operation_type(List->tokens[8]);
	}
	else {
		id = List->tokens[11];
		where_operation = get_operation_type(List->tokens[12]);
	}

	if (match(id.pattern, "id", id.start, id.end, 2) == false) {
		printf("\033[1;31mColumn name for WHERE clause should be id\033[0m\n");
		exit_table(t);
		return;
	}

	if (where_operation != EQUAL) {
		printf("\033[1;31mUpdate only works on operation of '='\033[0m\n");
		exit_table(t);
		return;
	}

	// Only one SET clause
	if (statement_number == 4) {
		Token i = List->tokens[3];
		OperationType o = get_operation_type(List->tokens[4]);

		if (o != EQUAL) {
			printf("\033[1;31mUpdate only works on operation of '='\033[0m\n");
			exit_table(t);
			return;
		}

		// Column refrenced in SET clause has to be either name or email
		bool name_check = match(i.pattern, "name", i.start, i.end, 4);
		bool email_check = match(i.pattern, "email", i.start, i.end, 5);

		if (name_check == false && email_check == false) {
			printf("\033[1;31mColumn name for SET clause should be either \"name\" or \"email\"\033[0m\n");
			exit_table(t);
			return;
		}

		char string[EMAIL_SIZE];

		Token value = List->tokens[5];

		int j = 0;
		for (int i = value.start + 1; i < value.end - 1; i++) {
			string[j] = value.pattern[i];
			j++;

			if (j == NAME_SIZE - 1 && name_check == true) { break; }
			if (j == EMAIL_SIZE - 1 && email_check == true) { break; }
		}
		string[j] = '\0';

		int32_t num = get_number(List->tokens[9]);
		
		if (name_check == true) {
			table_update(t, num, string, NULL);
		}
		else {
			table_update(t, num, NULL, string);
		}
	}
	// Two SET clauses
	else {
		OperationType o1 = get_operation_type(List->tokens[4]);
		OperationType o2 = get_operation_type(List->tokens[8]);

		if (o1 != EQUAL || o2 != EQUAL) {
			printf("\033[1;31mUpdate only works on operation of '='\033[0m\n");
			exit_table(t);
			return;
		}

		Token i1 = List->tokens[3];
		Token i2 = List->tokens[7];

		bool name_check = match(i1.pattern, "name", i1.start, i1.end, 4);
		bool email_check = match(i1.pattern, "email", i1.start, i1.end, 5);
		bool i2_is_name;

		if (name_check == false && email_check == false) {
			printf("\033[1;31mColumn name for SET clauses should be either \"name\" or \"email\"\033[0m\n");
			exit_table(t);
			return;
		}

		// Cant change name twice in one statement and other SET clause has to be email
		if (name_check == true && match(i2.pattern, "name", i2.start, i2.end, 4) == true) {
			printf("\033[1;31mCan only change name once in statement\033[0m\n");
			exit_table(t);
			return;
		}
		else if (name_check == true && match(i2.pattern, "email", i2.start, i2.end, 5) == false) {
			printf("\033[1;31mColumn name for SET clauses should be either \"name\" or \"email\"\033[0m\n");
			exit_table(t);
			return;
		}
		else if (name_check == true) {
			i2_is_name = false;
		}

		// Cant change email twice in one statement and other SET clause has to be name
		if (email_check == true && match(i2.pattern, "email", i2.start, i2.end, 5) == true) {
			printf("\033[1;31mCan only change email once in statement\033[0m\n");
			exit_table(t);
			return;
		}
		else if (email_check == true && match(i2.pattern, "name", i2.start, i2.end, 4) == false) {
			printf("\033[1;31mColumn name for SET clauses should be either \"name\" or \"email\"\033[0m\n");
			exit_table(t);
			return;
		}
		else if (email_check == true) {
			i2_is_name = true;
		}

		// Getting the string from the first SET clause
		char string_one[EMAIL_SIZE];
		Token value_one = List->tokens[5];

		int j = 0;
		for (int i = value_one.start + 1; i < value_one.end - 1; i++) {
			string_one[j] = value_one.pattern[i];
			j++;

			if (!i2_is_name && j == NAME_SIZE - 1) { break; }
			if (i2_is_name && j == EMAIL_SIZE - 1) { break; }
		}
		string_one[j] = '\0';

		//Getting the value from the second SET clause
		char string_two[EMAIL_SIZE];
		Token value_two = List->tokens[9];

		j = 0;
		for (int i = value_two.start + 1; i < value_two.end - 1; i++) {
			string_two[j] = value_two.pattern[i];
			j++;

			if (i2_is_name && j == NAME_SIZE - 1) { break; }
			if (!i2_is_name && j == EMAIL_SIZE - 1) { break; }
		}
		string_two[j] = '\0';

		int32_t num = get_number(List->tokens[13]);

		if (i2_is_name) {	
			table_update(t, num, string_two, string_one);
		}
		else {
			table_update(t, num, string_one, string_two);
		}
	}
	
	exit_table(t);
}

static void deletion(char* file_name) {
	start_database_page(file_name);
	Table* t = load_table(db_information->root_page, file_name);

	Token id = List->tokens[4];
	if (match(id.pattern, "id", id.start, id.end, 2) == false) {
		printf("\033[1;31mColumn name should be id\033[0m\n");
		exit_table(t);
		return;
	}

	if (get_operation_type(List->tokens[5]) != EQUAL) { 
		printf("\033[1;31mDeletion only works on operation of '='\033[0m\n");
		exit_table(t);
		return;
	}

	int32_t num = get_number(List->tokens[6]);

	table_remove(t, num);

	exit_table(t);
}

static void table(int statement_number, char* file_name) {
	// Table creation
	if (statement_number == 7) {
		MKDIR("tables");
		start_database_page(file_name);
		Table* t = create_table(file_name);
		exit_table(t);
	}
	// Table deletion
	else {
		if (remove(file_name) != 0) {
			printf("\033[1;31mCould not delete Table\033[0m\n");
			return;
		}

		printf("\033[1;32mTable deleted successfully\033[0m\n");
	}
}
//-------------------------------------------------------------------

static bool does_table_exist(char* file_name) {
	FILE* file = fopen(file_name, "rb");
	if (file == NULL) {
		return false;
	}

	fclose(file);
	return true;
}

void perform_token_actions() {
	MKDIR("tables");
	int check = compare_token_list();

	if (check == -1) {
		printf("\033[1;31mValid statement not entered\033[0m\n");
		return;
	}

	int* statement = statements_list[check];
	Token t = List->tokens[statement[1]];

	int start = t.start;
	int end = t.end;
	char* input = t.pattern;

	// 100 is the maximum length a tables name can be
	if (end - start >= 100) {
		printf("\033[1;31mTable's name is too long\033[0m\n");
		return;
	}

	/* Getting the tables name from the users input, 
	   as thats also the name of the file containing data for the table */
	char path_name[109] = "tables/";
	int j = 7;
	for (int i = start; i <= end; i++) {
		if (i == end) {
			path_name[j] = '\0';
		}
		else {
			path_name[j] = input[i];
		}

		j++;
	}

	/* if check returns 7, that means statement 8 was entered
	   where the user is creating a table, so the table entered 
		wouldnt exist */
	if (!does_table_exist(path_name) && check != 7) {
		printf("\033[1;31mTable does not exist\033[0m\n");
		return;
	}
	// If table already exists, cant create it again
	else if (does_table_exist(path_name) && check == 7) {
		printf("\033[1;31mTable already exists\033[0m\n");
		return;
	}

	/* Calling a function based on the type of statments
	   to perform the required actions */
	if (check >= 7 && check <= 8) {
		table(check, path_name);
	}
	else if (check >= 0 && check <= 2) {
		selection(check, path_name);
	}
	else if (check == 3) {
		insertion(path_name);
	}
	else if (check >= 4 && check <= 5) {
		updates(check, path_name);
	}
	else if (check == 6) {
		deletion(path_name);
	}
}
