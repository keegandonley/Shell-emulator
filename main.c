/*
 * Keegan Donley
 * CS 450 Homework 2
 * March 2018
 * 
 * This is a shell-like program that is able to execute commands using pipes
 * and input/output redirection. 
 * 
 * Commands can be entered with the following guidelines:
 * 		- commands cannot be bash builtins
 * 		- there will be 1024 or fewer characters per line
 * 		- there will be 100 or fewer words per line
 * 		- there will be whitespace around <, >, and | operators
 * 		- in a command with an arbitrary number of pipes, the first command may use redirected
 * 		  input and the second command may use redirected output
 * 
 * To exit the program press ^D
 * 
 * `make run` will build and execute the program
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "constants.h"
#include "parsetools.h"

/*
 * Struct: command_data
 * ----------------------
 * 
 * name: the command being executed (ls, wc, etc)
 * 
 * args: an array of char * items that will hold the arguments to the command.
 *       The first argument of this array should always be the name of the command.
 * 
 * argc: the number of arguments for the command
 * 
 * is_file_in: a flag that should be set to 0 if the command does not read input from a file,
 *             and set to 1 when it does. It should never be 1 if is_file_out is 1, and
 *             vice-versa
 * 
 * is_file_out: a flag that should be set to 0 if the command does not write output to a file,
 *              and set to 1 when it does. It should never be 1 if is_file_in is 1, and
 *              vice-versa
 * 
 * file_name_in: The name of the file that is being read from by the command. Is required if
 *               is_file_in is set.
 * 
 * file_name_out: The name of the file that is being written to by the command. Is required if
 *                is_file_out is set.
 */
typedef struct command_data {
	char * name;
	char ** args;
	int argc;
	int is_file_in;
	int is_file_out;
	char * file_name_in;
	char * file_name_out;
} command;

void syserror( const char * );
int what_operator(const char* line_word);
int get_args(char ** line_words, int i, int num_words, char ** arguments);
int spawn_two_processes(char command[2][1024], char *** args_list, int * num_arguments, int action_code);
int spawn_one_process(char command[2][1024], char *** args, int * argc, int input_output);
int count_pipes(char ** line_words, int num_words);
int get_commands(char ** line_words, int num_words, command * commands, int num_commands);
void process_commands(command * commands, int num_commands, int num_pipes);
int remove_quotes(char ** args, int argc);

int main() {
    // Buffer for reading one line of input
    char line[MAX_LINE_CHARS];
    char* line_words[MAX_LINE_WORDS + 1];

    // Loop until user hits Ctrl-D (end of input)
    // or some other input error occurs
	while(fgets(line, MAX_LINE_CHARS, stdin)) {		
		int num_words = split_cmd_line(line, line_words);
		int num_pipes = count_pipes(line_words, num_words);
		command * commands = malloc((num_pipes + 1) * sizeof(command));
		int num_commands = get_commands(line_words, num_words, commands, num_pipes + 1);
		process_commands(commands, num_commands, num_pipes);
    }

    return 0;
}

/*
 * Function: process_commands
 * --------------------------
 *  Takes all the commands for a line and executes them with proper input and
 *  output direction. Each command that is processed has an array of arguments.
 *  This array is null-terminated by this function before being passed to exec.
 *
 *  commands: An array containing command_data items representing each command to
 * 		      be executed.
 * 
 *  num_commands: An integer representing how many commands are being passed.
 *
 *  num_pipes: An integer representing how many pipes will need to be created.
 */
void process_commands(command * commands, int num_commands, int num_pipes) {
	int pipes[num_pipes * 2];

	for (int i = 0; i < num_pipes; i ++) {
		if (pipe(pipes + i * 2) < 0) {
			syserror( "Could not create pipe" );
		}
	}

	int command_index = 0; // represents which command in the array is being executed
	int max_commands = num_commands;

	for (int i = command_index; i < max_commands; i++) {
		commands[i].args[commands[i].argc] = NULL;
		int num_changes = remove_quotes(commands[i].args, commands[i].argc);
		pid_t pid;

		switch (pid = fork()) {
			case 0:
				// Open output file if necessary
				if (commands[i].is_file_out) {
					if (close(1) == -1) syserror("Could not close stdout");
					int fd = open(commands[i].file_name_out, O_WRONLY | O_CREAT | O_TRUNC, 0777);
					if (fd < 0) {
						syserror("Could not open output file");
					}
					if (dup(fd) < 0) syserror("Could not dup file descriptor");
				}
				
				// Open input file if necessary
				if (commands[i].is_file_in) {
					if (close(0) == -1) syserror("Could not close stdin");
					int fd = open(commands[i].file_name_in, O_RDONLY);
					if (fd < 0) {
						syserror("Could not open input file");
					}
					if (dup(fd) < 0) syserror("Could not dup file descriptor");
				}

				// As long as we are not looking at the first command in the string
				// we can get the input from the pipe.
				if (i != 0) {
					if (dup2(pipes[(i - 1) * 2], 0) < 0) {
						syserror( "Could not open input pipe" );
					}
				}

				// As long as we are not looking at the last command in the string
				// we can send the input to the pipe.
				if (i != max_commands - 1) {
					if (dup2(pipes[i * 2 + 1], 1) < 0) syserror( "Could not open output pipe" );
				} 

				// Close all pipes
				for (int i = 0; i < num_pipes * 2; i ++)
					if (close(pipes[i]) < 0) {
						syserror( "Could not close pipe from child" );
					}

				// Perform execution and error if exevp fails
				execvp(commands[i].name, commands[i].args);
				syserror( "Could not exec" );
			case -1:
				syserror( "Could not fork" );
		}
	}

	// Close parent's pipes
	for (int i = 0; i < num_pipes * 2; i ++)
		if (close(pipes[i]) < 0) {
			syserror( "Could not close pipe from parent" );
		}

	// Wait for all forks to complete
	for (int i = 0; i < num_pipes * 2; i ++)
		wait(0);

}

/*
 * Function: remove_quotes
 * --------------------------
 *  Strips quotes from around a file. Assumes that if an argument starts with a quote it also ends with one.
 *
 *  args: An array containing char * words, one for each argument of a single command
 * 
 *  argc: An integer to represent the number of arguments being passed
 * 
 *  returns: An integer to represent the number of arguments modified.
 */
int remove_quotes(char ** args, int argc) {
	int count = 0;
	for (int i = 0; i < argc; i++) {
		if (args[i][0] == '\"' || args[i][0] == '\'') {
			int length = strlen(args[i]);
			args[i]++;
			args[i][length - 2] = 0;
			count++;
		}
	}
	return count;
}

/*
 * Function: get_commands
 * --------------------------
 *  Parses a line entered by the user and breaks it up into its separate,
 * 	pipe-connected commands and their arguments.
 *
 *  line_words: An array containing char * words, one for each line entered.
 * 
 *  num_words: An integer representing how many words are being passed.
 *
 *  commands: An un-allocated array of command_data items to be allocated and filled.
 * 
 *  num_commands: An integer representing how many commands are to be extracted from
 *  the line.
 * 
 *  returns: An integer to represent the number of commands parsed.
 */
int get_commands(char ** line_words, int num_words, command * commands, int num_commands) {
	int is_new = 0;
	int argc = 1;
	int commandc = 0;
	commands[commandc].args = malloc(num_words * sizeof(char*));
	int noargs = 0;
	int previous = 0;
	for (int i = 0; i < num_words; i++) {
		if (is_new == 0) {
			switch (what_operator(line_words[i])) {
				case 3: // this is a command
					is_new = 1;
					// Add the command to the struct
					commands[commandc].name = malloc(strlen(line_words[i]) + 1);
					strcpy(commands[commandc].name, line_words[i]);
					argc = 1;
					commands[commandc].args[0] = malloc(strlen(line_words[i]) + 1);
					strcpy(commands[commandc].args[0], line_words[i]);
					commands[commandc].argc = argc;
					commands[commandc].is_file_in = 0;
					commands[commandc].is_file_out = 0;
					noargs = 0;
					break;
				case 0: // a new pipe
					commandc++;
					commands[commandc].args = malloc(sizeof(char*));
					is_new = 0;
					break;
				default:
					return num_commands;
			}
		} else {
			switch (what_operator(line_words[i])) {
				case 3: // this is an argument
					if (noargs) {
						if (previous == 1) {
							commands[commandc].file_name_out = malloc(strlen(line_words[i]) + 1);
							strcpy(commands[commandc].file_name_out, line_words[i]);
						} else {
							commands[commandc].file_name_in = malloc(strlen(line_words[i]) + 1);
							strcpy(commands[commandc].file_name_in, line_words[i]);
						}

					} else {
						commands[commandc].args[argc] = malloc(strlen(line_words[i]) + 1);
						strcpy(commands[commandc].args[argc], line_words[i]);
						commands[commandc].argc++;
						argc++;
					}
					break;
				case 0: // a new pipe
					commandc++;
					commands[commandc].args = malloc(num_words * sizeof(char*));
					is_new = 0;
					break;
				case 1: // Ouput redirection
					commands[commandc].is_file_out = 1;
					noargs = 1;
					previous = 1;
					break;
				case 2: //input redirection
					commands[commandc].is_file_in = 1;
					noargs = 1;
					previous = 2;
					break;
				default:
					return num_commands;
			}
		}
	}

	return num_commands;
}

/*
 * Function: count_pipes
 * --------------------------
 *  Spawns one process and handles either input redirection, output redirection, 
 *  or standard execution.
 *
 *  line_words: An array containing char * words, one for each line entered.
 * 
 *  num_words: An integer representing how many words were passed in the line.
 * 
 *  returns: An integer representing the number of pipes found in the line.
 */
int count_pipes(char ** line_words, int num_words) {
	// Count pipes
	int count = 0;
	for (int i = 0; i < num_words; i++) {
		if (what_operator(line_words[i]) == 0) count++;
	}
	return count;
}

/*
 * Function: get_args
 * --------------------------
 *  Retreives the arguments for a command and builds an array of arguments.
 *
 *  line_words: An array of char * words represending an entire line of input
 * 
 *  current_word_index: An integer representing the index of the word in the line
 * 						that triggered argument extraction. 
 *
 *  argument_count: The number of total arguments to be retrieved.
 * 
 *  arguments: An array of char * items to be filled by the arguments.
 * 
 *  returns: An integer to represent the last item that was taken from the input
 */
int get_args(char ** line_words, int current_word_index, int argument_count, char ** arguments) {
	
	int last_word_index = current_word_index + argument_count;

	int i;
	for (i = current_word_index; i <= last_word_index; i++) {
		char * word = line_words[i];
		arguments[i - current_word_index] = word;
	}

	return i - 2;

}

/*
 * Function: what_operator
 * --------------------------
 *  Determines what a word's type is
 *
 *  line_word: The word to be checked
 *
 *  returns: An integer 1 to represent the output redirection operator, a 2 to represent the input
 *  		 redirection operator, a 0 to represent the pipe, and a 3 if the word was not an operator
 */
int what_operator(const char* line_word) {
	if (strcmp(line_word, "<") == 0) // input redirection
		return 2;
	else if (strcmp(line_word, ">") == 0) // output redirection
		return 1;
	else if (strcmp(line_word, "|") == 0) 
		return 0;
	return 3;
}

/*
 * Function: syserror
 * --------------------------
 *  Outputs an error message and terminates execution
 *
 *  s: The char * string to be displayed before exiting
 *
 *  returns: An integer 1 to represent the output redirection operator, a 2 to represent the input
 *  		 redirection operator, a 0 to represent the pipe, and a 3 if the word was not an operator
 */
void syserror(const char *s)
{
    extern int errno;

    fprintf( stderr, "%s\n", s );
    fprintf( stderr, " (%s)\n", strerror(errno) );
    exit( 1 );
}
