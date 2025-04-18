/*
 *
 * CS-413 Spring 98
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
*/

%{

#include <string.h>
#include "y.tab.h"

%}

%%

\n 					{
						return NEWLINE;
					}

[ \t] 				{
						/* Discard spaces and tabs */
					}	

(?i:"EXIT") 		{	
						printf("\nExiting terminal\n\n");
						exit(0);
					}



"cd"				{
						return CD;
					}
					
">>&"				{
						return GREATGREATAMP;
					}

">>" 				{
						return GREATGREAT;
					}

"<"					{
						return LESS;
					}

">"					{
						return GREAT;
					}

"|"					{
						return PIPE;
					}

"&"					{
						return AMP;
					}

[^ \t\n"<"">""&""|"]+ {
						/* Assume that file names have only alpha chars */
						yylval.string_val = strdup(yytext);
						return WORD;
					}

. 					{
						printf("Illegal character %c\n", *yytext);
					}

%%
	
