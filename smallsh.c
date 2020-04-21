/********************************************************************************
** Description:  This program will run command line instructions and return the
**				 results similar to other shells.
*********************************************************************************/
#define _BSD_SOURCE 1
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_LENGTH 2048
#define MAX_ARGUMENT 512
int isBackground = 1;

/************************************************************************************
** Function Name : checkStatus
** Description : check and display the exit status
************************************************************************************/
void checkStatus(int childExitMethod) {
	if (WIFEXITED(childExitMethod)) {
		// exited by status
		printf("exit value %d\n", WEXITSTATUS(childExitMethod));
	}
	else {
		// terminated by signal
		printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
	}
}


/************************************************************************************
** Function Name : catchSIGINT/catchSIGSTOP
** Description : catch and display message for signals, switch between foreground and 
**			     background modecode modified based on lecture from professor
************************************************************************************/
void catchSIGINT(int signo) {
	char* message = "\nCaught SIGINT.\n";
	write(STDOUT_FILENO, message, 16);
}

void catchSIGTSTP(int signo) {
	// If backgound mode is on, display message and switch mode to 0
	if (isBackground == 1) {
		char* message = "\nEntering foreground-only mode (& is now ignored)\n";
		write(1, message, 49);
		fflush(stdout);
		isBackground = 0;
	}	
	// else display message and switch mode to 1
	else {
		char* message = "\nExiting foreground-only mode\n";
		write(1, message, 29);
		fflush(stdout);
		isBackground = 1;
	}
}



// main function
int main() {
	// set up catchSIGINT()
	struct sigaction sa_sigint = { 0 };
	sa_sigint.sa_handler = SIG_IGN;
	sigfillset(&sa_sigint.sa_mask);
	sa_sigint.sa_flags = 0;
	sigaction(SIGINT, &sa_sigint, NULL);

	// set up catchSIGTSTP() to switch background/foreground mode
	struct sigaction sa_sigtstp = { 0 };
	sa_sigtstp.sa_handler = catchSIGTSTP;
	sigfillset(&sa_sigtstp.sa_mask);
	sa_sigtstp.sa_flags = 0;
	sigaction(SIGTSTP, &sa_sigtstp, NULL);

	int pid = getpid();
	pid_t spawnPid = -5;
	int statusCode = 0;
	int backgroundOn = 0;

	// declare array to store arguments from user input
	char* arguments[MAX_ARGUMENT];
	int inputFile, outputFile;
	char inFileName[255] = "";
	char outFileName[255] = "";

	int i;
	do {
		// initialize array and background at beginning of each loop
		for (i = 0; i < MAX_ARGUMENT; i++) {
			arguments[i] = NULL;
		}
		backgroundOn = 0;

		// Get userInput, ignore comment or empty input
		char userInput[MAX_LENGTH];
		do {
			printf(": ");
			fflush(stdout);
			fgets(userInput, MAX_LENGTH, stdin);
		} while (userInput[0] == '#' || userInput[0] == '\n');

		// eat newline at the end
		for (i = 0; i < MAX_LENGTH; i++) {
			if (userInput[i] == '\n') {
				userInput[i] = '\0';
				break;
			}
		}
		
		// convert $$ to pid
		char* temp = strdup(userInput);
		for (i = 0; i < MAX_LENGTH; i++) {
			if ((temp[i] == '$') && (temp[i + 1] == '$' && (i + 1 < strlen(temp)))) {
				temp[i] = '%';
				temp[i + 1] = 'd';
			}
		}
		sprintf(userInput, temp, getpid());
		free(temp);

		// convert user input into the list of arguments
		char *token = strtok(userInput, " ");
		// check if user command is exit
		if (strcmp(token, "exit") == 0) {
			exit(0);
		}

		int i;
		for (i = 0; token; i++) {
			// Check if user specified & background process
			if (!strcmp(token, "&")) {
				backgroundOn = 1;
			}

			// Check if user specified input file
			else if (!strcmp(token, "<")) {
				token = strtok(NULL, " ");
				strcpy(inFileName, token);
			}

			// Check if user specified output file
			else if (!strcmp(token, ">")) {
				token = strtok(NULL, " ");
				strcpy(outFileName, token);
			}

			else {
				arguments[i] = strdup(token);				
			}
			token = strtok(NULL, " ");
		}
		
		// check if user wants to see status
		if (strcmp(arguments[0], "status") == 0) {
			checkStatus(statusCode);
		}

		// check is user command is change directory
		else if (strcmp(arguments[0], "cd") == 0) {
			// // If there are no other arguments other than cd, then go to home directory
			if (arguments[1] == NULL)
				chdir(getenv("HOME"));
			else
				chdir(arguments[1]);
		}

		// run commands
		else {
			//Fork a new process and run switch on pid (code from lecture)
			spawnPid = fork();
			switch (spawnPid) {
			case -1:
				perror("Hull Breach!\n");
				exit(1);
				break;

			case 0:
				sa_sigint.sa_handler = SIG_DFL;
				sigaction(SIGINT, &sa_sigint, NULL);

				// handle input file, code provided by professor
				if (strcmp(inFileName, "")) {
					inputFile = open(inFileName, O_RDONLY);
					if (inputFile == -1) {
						fprintf(stderr, "Cannot open %s for input\n", inFileName);
						exit(1);
					}
					if (dup2(inputFile, 0) == -1) {
						perror("dup2");
						exit(2);
					}
					// close input file
					close(inputFile);
				}

				// Handle output, code provided by professor
				if (strcmp(outFileName, "")) {
					outputFile = open(outFileName, O_WRONLY | O_CREAT | O_TRUNC, 0666);
					if (outputFile == -1) {
						fprintf(stderr, "Cannot open %s for output\n", outFileName);
						exit(1);
					}

					if (dup2(outputFile, 1) == -1) {
						perror("dup2");
						exit(2);
					}

					// close output file
					close(outputFile);
				}

				// Execute commands and check for invalid name
				if (execvp(arguments[0], (char* const*)arguments)) {
					printf("%s: no such file or directory\n", arguments[0]);
					fflush(stdout);
					exit(2);
				}
				break;

			default:
				// if background mode is on, run process in background
				if (backgroundOn && isBackground) {
					//pid_t actualPid = waitpid(spawnPid, statusCode, WNOHANG);
					printf("background pid is %d\n", spawnPid);
					fflush(stdout);
				}

				// else run in foreground mode
				else {
					waitpid(spawnPid, &statusCode, 0);
				}				
			}
		}
		
		inFileName[0] = '\0';
		outFileName[0] = '\0';
		// Check if process has completed
		spawnPid = waitpid(-1, &statusCode, WNOHANG);
		while (spawnPid > 0) {
			printf("background pid %i is done: ", spawnPid);
			checkStatus(statusCode);
			fflush(stdout);
			spawnPid = waitpid(-1, &statusCode, WNOHANG);
		}
	} while (1);
	
	return 0;

}