#ifndef tokens_H
#define tokens_h

typedef enum {
	// Keywords
	SELECT, FROM, WHERE, BETWEEN, AND, INSERT, INTO, VALUES, UPDATE, SET, DELETE,
	CREATE, TABLE, DROP,

	// Punctuation and logic
	LEFT_PAREN, RIGHT_PAREN, COMMA, OPERATION, STAR,

	//Literals
	NUMBER, STRING, IDENTIFIER
} TokenType;

#endif
