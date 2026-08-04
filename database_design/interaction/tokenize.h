#ifndef tokenize_H
#define tokenize_H

#include "tokens.h"

void add_token(TokenType type, char* input, int start, int end);
void clear_tokens();
void perform_token_actions();

#endif
