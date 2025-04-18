/*
 * CS-413 Spring 98
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 */

%union	{
		char   *string_val;
	}

%token <string_val> WORD

%token  PIPE GREATGREAT LESS AMP CD 
%token 	NOTOKEN GREAT NEWLINE 
%token GREATGREATAMP

%{
extern "C" 
{
	int yylex();
	void yyerror (char const *s);
}

#define yylex yylex
#include <stdio.h>
#include <unistd.h> // for chdir
#include <string.h>
#include "command.h"
%}

%%

goal: commands
	;

commands: 
	command
	| commands command 
	;

command : simple_command
        | simple_command command // handling PIPEs
		| CD NEWLINE {
			printf("   Yacc: Change directory to home\n");
			const char* home = getenv("HOME");
			
			// Ensure HOME environment variable exists
			if (!home) {
				fprintf(stderr, "Error: HOME environment variable not set\n");
			} 
			// Attempt to change directory 
			else if (chdir(home) != 0) {
				perror("cd");  
			} 
			else {
				Command::_currentCommand.prompt();
			}
		}
		| CD WORD NEWLINE {
			printf("   Yacc: Change directory to \"%s\"\n", $2);
			// 1. Check for NULL pointer
			// 2. Validate path length
			// 3. Verify directory exists and is accessible
			if (!$2) {
				fprintf(stderr, "Error: Invalid directory path\n");
			} 
			else if (strlen($2) > 1024) {  
				fprintf(stderr, "Error: Directory path exceeds maximum length (1024 characters)\n");
			} 
			else if (chdir($2) != 0) {
				perror("cd");  
			} 
			else {
				Command::_currentCommand.prompt();
			}
		}
 		;

simple_command:
	// handling piped commands
    command_and_args PIPE {
		printf("   Yacc: Pipe command\n");
		Command::_currentSimpleCommand->pipe();
	}
	| command_and_args iomodifier command_end {
		printf("   Yacc: Execute command\n");
		Command::_currentCommand.execute();
	}
	| NEWLINE 
	| error NEWLINE { yyerrok; }
	;

command_end:
	NEWLINE
	| AMP NEWLINE {
		printf("   Yacc: Execute in background\n");
		Command::_currentCommand._background = true;
	}
	;

command_and_args:
	command_word arg_list {
		Command::_currentCommand.
			insertSimpleCommand( Command::_currentSimpleCommand );
	}
	;

arg_list:
	arg_list argument
	| /* can be empty */
	;

argument:
	WORD {
		/* Process and validate command arguments */
		printf("   Yacc: insert argument \"%s\"\n", $1);
		if ($1 && strlen($1) < 4096) {
			Command::_currentSimpleCommand->insertArgument($1);
		} else {
			fprintf(stderr, "Error: Argument invalid or exceeds maximum length (4096 characters)\n");
		}
	}
	
	
	;

command_word:
	WORD {
               printf("   Yacc: insert command \"%s\"\n", $1);
	       
	       Command::_currentSimpleCommand = new SimpleCommand();
	       Command::_currentSimpleCommand->insertArgument( $1 );
	}
	;
iomodifier:
	iomodifier_opt
	| iomodifier_opt iomodifier_opt
	;

iomodifier_opt:
	iomodifier_opt iomodifier_opt
	| GREAT WORD {
		/* Handle output redirection (>) with validation */
		printf("   Yacc: insert output \"%s\"\n", $2);
		// - Ensure path is not NULL
		// - Check path length is within reasonable limits
		if ($2 && strlen($2) < 1024) {
			Command::_currentCommand._outFile = $2;
		} else {
			fprintf(stderr, "Error: Output file path invalid or too long (max 1024 characters)\n");
		}
	}
	| GREATGREAT WORD {
		/* Handle append redirection (>>) with validation */
		printf("   Yacc: insert append \"%s\"\n", $2);
		if ($2 && strlen($2) < 1024) {
			Command::_currentCommand._outFile = $2;
			Command::_currentCommand._append = true;
		} else {
			fprintf(stderr, "Error: Append file path invalid or too long (max 1024 characters)\n");
		}
	}
	| GREATGREATAMP WORD {
		/* Handle append with background (>>&) with validation */
		printf("   Yacc: insert append with background \"%s\"\n", $2);
		if ($2 && strlen($2) < 1024) {
			Command::_currentCommand._outFile = $2;
			Command::_currentCommand._append = true;
			Command::_currentCommand._background = true;
		} else {
			fprintf(stderr, "Error: Background append file path invalid or too long (max 1024 characters)\n");
		}
	}
	| LESS WORD {
		/* Handle input redirection (<) with validation */
		printf("   Yacc: insert input \"%s\"\n", $2);
		
		// Validate input file path
		if ($2 && strlen($2) < 1024) {
			Command::_currentCommand._inputFile = $2;
		} else {
			fprintf(stderr, "Error: Input file path invalid or too long (max 1024 characters)\n");
		}
	}
	| /* can be empty */ 
	;

%%



void
yyerror(const char * s)
{
	fprintf(stderr,"%s", s);
}

#if 0
main()
{
	yyparse();
}
#endif
