#include <stdio.h>
#include <string.h>
#include "interaction/lexer.h"

#define MAX_INPUT_SIZE 1000

int main(void) {
	char input[MAX_INPUT_SIZE];

	while(1) {
		printf("\033[1;33mdb> \033[0m");

		if (fgets(input, sizeof(input), stdin) == NULL) {
			break;
		}

		// If inputs is over the maximum size
		if (strchr(input, '\n') == NULL) {
        	printf("Command too long\n");

        	int c;
        	while ((c = getchar()) != '\n' && c != EOF);
        	continue;
    	}

		// If user types ":q", we exist out of the REPl, regaurdless if there was more input after
		if (input[0] == ':' && input[1] == 'q') {
			printf("\033[1;33mQuitting \033[0m\n");
			break; 
		}

		// Skip lexing if its just a new line
		if (input[0] == '\n') {
			continue;
		}

		input[strcspn(input, "\n")] = '\0'; // Remove the new line after an input with the null char
		lex_input(input);
	}

   return 0;
}
