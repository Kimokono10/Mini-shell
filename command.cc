/*
 * CS354: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <wait.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "command.h"

#define DEBUG 0
#define LOGFILE "logs.txt"

SimpleCommand::SimpleCommand()
{
	// Creat available space for 5 arguments
	_numberOfAvailableArguments = 5;
	_numberOfArguments = 0;
	_arguments = (char **) malloc( _numberOfAvailableArguments * sizeof( char * ) );
	_pipe = false;
}

void
SimpleCommand::insertArgument( char * argument )
{
	// realocate space if necessary
	if ( _numberOfAvailableArguments == _numberOfArguments  + 1 ) {
		// Double the available space
		_numberOfAvailableArguments *= 2;
		_arguments = (char **) realloc( _arguments,
				  _numberOfAvailableArguments * sizeof( char * ) );
	}
	
	// Insert argument
	_arguments[ _numberOfArguments ] = argument;

	// Add NULL argument at the end
	_arguments[ _numberOfArguments + 1] = NULL;
	
	_numberOfArguments++;
}

// added functions
void SimpleCommand::pipe(){
	_pipe = true;
}


Command::Command()
{
	// Create available space for one simple command
	_numberOfAvailableSimpleCommands = 1;
	_simpleCommands = (SimpleCommand **)
		malloc( _numberOfAvailableSimpleCommands * sizeof( SimpleCommand * ) );

	_numberOfSimpleCommands = 0;
	_outFile = 0;
	_inputFile = 0;
	_errFile = 0;
	_background = 0;
	_append = false;
}


void
Command::insertSimpleCommand( SimpleCommand * simpleCommand )
{
	if ( _numberOfAvailableSimpleCommands == _numberOfSimpleCommands ) {
		_numberOfAvailableSimpleCommands *= 2;
		_simpleCommands = (SimpleCommand **) realloc( _simpleCommands,
			 _numberOfAvailableSimpleCommands * sizeof( SimpleCommand * ) );
	}
	
	_simpleCommands[ _numberOfSimpleCommands ] = simpleCommand;
	_numberOfSimpleCommands++;
}

void
Command:: clear()
{
	for ( int i = 0; i < _numberOfSimpleCommands; i++ ) {
		for ( int j = 0; j < _simpleCommands[ i ]->_numberOfArguments; j ++ ) {
			free ( _simpleCommands[ i ]->_arguments[ j ] );
		}
		
		free ( _simpleCommands[ i ]->_arguments );
		free ( _simpleCommands[ i ] );
	}

	if ( _outFile ) {
		free( _outFile );
	}

	if ( _inputFile ) {
		free( _inputFile );
	}

	if ( _errFile ) {
		free( _errFile );
	}

	_numberOfSimpleCommands = 0;
	_outFile = 0;
	_inputFile = 0;
	_errFile = 0;
	_background = 0;
	_append = false;
	
}

void
Command::print()
{
	printf("\n\n");
	printf("              COMMAND TABLE                \n");
	printf("\n");
	printf("  #   Simple Commands\n");
	printf("  --- ----------------------------------------------------------\n");
	
	for ( int i = 0; i < _numberOfSimpleCommands; i++ ) {
		printf("  %-3d ", i );
		for ( int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++ ) {
			printf("\"%s\" \t", _simpleCommands[i]->_arguments[ j ] );
		}
		printf("\n");
	}

	printf( "\n\n" );
	printf( "  Output       Input        Error        Background\n" );
	printf( "  ------------ ------------ ------------ ------------\n" );
	printf( "  %-12s %-12s %-12s %-12s\n", _outFile?_outFile:"default",
		_inputFile?_inputFile:"default", _errFile?_errFile:"default",
		_background?"YES":"NO");
	printf( "\n\n" );
	
}

bool children_reaped = false;

void sigchld_handler(int sig) {

	// open logs
	FILE *logfile = fopen(LOGFILE, "a+");
	if (logfile == NULL) {
		perror("Error opening log file");
		return;
	}

	// reap all children
	pid_t pid;
    int   status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0){
		fprintf(logfile, "Child process with PID %d terminated with status %d\n", pid, status);
	}

	// close logs
	fclose(logfile);

	// set flag
	children_reaped = true;

}

void
Command::execute()
{
    // Return if no commands were provided
    if (_numberOfSimpleCommands == 0) {
        prompt();
        return;
    }

    // Display command table
    print();

    // Initialize pipe for first command
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe creation failed");
        return;
    }

    // Store original file descriptors to restore them later
    int defaultin = dup(0);   
    int defaultout = dup(1);  
    int defaulterr = dup(2); 

    // Process each command in the pipeline
    for (int i = 0; i < _numberOfSimpleCommands; i++) {
        // Reset to default file descriptors at the start of each iteration
        dup2(defaultin, 0);
        dup2(defaultout, 1);
        dup2(defaulterr, 2);

        SimpleCommand *current = _simpleCommands[i];
        bool isLastCommand = (i == _numberOfSimpleCommands - 1);

        //=============== INPUT HANDLING ===============//
        if (i == 0 && _inputFile) {
            int fd = open(_inputFile, O_RDONLY);
            if (fd < 0) {
                perror("input file open failed");
                continue;
            }
            dup2(fd, 0);  // Redirect stdin to input file
            close(fd);  
        } else if (i > 0 && _simpleCommands[i-1]->_pipe) {
            // Not first command and previous command has pipe
            // Read from pipe instead of stdin
            dup2(pipefd[0], 0);
            close(pipefd[0]);
        }

        //=============== OUTPUT HANDLING ===============//
        if (isLastCommand && _outFile) {
            int flags = O_WRONLY | O_CREAT;
            flags |= _append ? O_APPEND : O_TRUNC;  // Append or overwrite mode
            int fd = open(_outFile, flags, 0644);  
            if (fd < 0) {
                perror("output file open failed");
                continue;
            }
            dup2(fd, 1);  // Redirect stdout to output file
            close(fd);
        } else if (current->_pipe) {
            // Current command pipes to next command
            // Create new pipe for next command
            if (pipe(pipefd) == -1) {
                perror("pipe creation failed");
                continue;
            }
            dup2(pipefd[1], 1);
            close(pipefd[1]); 
        }

        //=============== ERROR HANDLING ===============//
        if (_errFile) {
            // Error redirection specified
            int fd = open(_errFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
                dup2(fd, 2); 
                close(fd);   
            }
        }

        //=============== COMMAND EXECUTION ===============//
        pid_t pid = fork();
        if (pid == 0) {
            // Child process: execute the command
            execvp(current->_arguments[0], current->_arguments);
            perror("execvp failed");
            exit(1);  
        } else if (pid < 0) {
            // Fork failed
            perror("fork failed");
            continue;
        }

        // Parent process
        if (!_background) {
            // Foreground process: wait for completion
            while (!children_reaped) {
                // Wait for SIGCHLD handler to reap child
                usleep(1000);
            }
            children_reaped = false;
        }
        // Background process: continue without waiting
    }

    //=============== CLEANUP ===============//
    // Restore original file descriptors
    dup2(defaultin, 0);
    dup2(defaultout, 1);
    dup2(defaulterr, 2);

    // Close all saved descriptors
    close(defaultin);
    close(defaultout);
    close(defaulterr);
    close(pipefd[0]);
    close(pipefd[1]);
    clear();
    prompt();
}


void
Command::prompt()
{
	printf("\nKimokono>");
	fflush(stdout);
}

Command Command::_currentCommand;
SimpleCommand * Command::_currentSimpleCommand;

int yyparse(void);


int 
main()
{
	// ignore control C
	signal(SIGINT, SIG_IGN);
	signal(SIGCHLD, sigchld_handler);

	FILE *logfile = fopen(LOGFILE, "a+");
    if (logfile == NULL) {
        perror("Error opening log file");
        return 1;
    }
	fprintf(logfile, "\nShell session starting:\n");
    fclose(logfile);

	Command::_currentCommand.prompt();
	yyparse();
	return 0;
}
