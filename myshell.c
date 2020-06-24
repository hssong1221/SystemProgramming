#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#define ENDOFLINE	1 
#define ARG			2 
#define	AMPERSAND	3
#define SEMICOLON	4
#define PIPE		5 
#define EXCLAMATION	6

#define MAXARG		512 
#define	MAXBUF		1024
#define MAXPIPE		5

#define	FOREGROUND	0 
#define	BACKGROUND	1 

char inpbuf[MAXBUF];
char tokbuf[MAXBUF*2]; 
char *ptr = inpbuf; 
char *tok = tokbuf; 
char tag[] = { ' ', '\t', '&', ';', '\n', '\0', '!'}; 
struct sigaction act;
char *baseline = "minishell> ";
int save = 0;
char historybuffer[MAXBUF][MAXBUF];

int input(char*);
int classify(char**);
int check(char);
int work(void);
int run(char**, int, int);
int linkpipe(char**, char**);
void separate(char**, char**, char**);

int fatal(char *err) 
{ 
	perror(err);
	exit(1); 
}

int input(char *base) 
{ 
	int word, count;
	
	ptr = inpbuf;
	tok = tokbuf;
	
	printf("%s", base); 
	count = 0; 

	while (true) 
	{
		if ((word = getchar()) == EOF) 
			return EOF;

		if (count < MAXBUF)
			inpbuf[count++] = word; 

		if (word == '\n' && count < MAXBUF) 
		{
			inpbuf[count] = '\0'; 
			strcpy(historybuffer[save++], inpbuf);
			historybuffer[save + 1][0] = '\0';
			return count;
		}
	}
}

int classify(char **outptr) 
{  
	int type;
	*outptr = tok;

	while (*ptr == ' ' || *ptr == '\t')
		ptr++;

	*tok++ = *ptr;

	switch (*ptr++) 
	{ 
		case '\n':
			type = ENDOFLINE;	
			break;
		case '&':
			type = AMPERSAND;
			break;
		case ';':
			type = SEMICOLON;
			break;
		case '|':
			type = PIPE;
			break;
		case '!':
			type = EXCLAMATION;
			break;
		default:
			type = ARG;
			while (check(*ptr)) 
				*tok++ = *ptr++;
	}
	*tok++ = '\0';
	return type;
}

int check(char input) 
{ 
	char *checking;
	for (checking = tag; *checking; checking++) 
	{
		if (input == *checking)
			return 0;
	}
	return 1;
}

int work(void) 
{
	char *arg[MAXARG + 1]; 
	int tokkentype; 
	int argnum = 0; 
	int type1;  
	int type2 = 0;
	while (true) 
	{ 
		tokkentype = classify(&arg[argnum]);
		if(tokkentype == ARG)
		{
			if (argnum < MAXARG)
				argnum++;
		}
		else if(tokkentype == ENDOFLINE || tokkentype == SEMICOLON || tokkentype == EXCLAMATION || tokkentype == AMPERSAND)
		{
			if (tokkentype == AMPERSAND) 
				type1 = BACKGROUND;
			else
				type1 = FOREGROUND;

			if (argnum != 0) 
			{
				arg[argnum] = NULL;
				if (strcmp(arg[0], "quit") == 0) 
					return -1;
				run(arg, type1, type2); 
			}

			if (tokkentype == ENDOFLINE)
				return 0;
			argnum = 0;
		}
		else if(tokkentype == PIPE)
		{
			type2 = tokkentype;
			if (argnum < MAXARG) 
				argnum++;
		}
	}
}

int run(char **cline, int back, int pipe) 
{ 
	pid_t pid;
	int status;
	char temp[MAXBUF][MAXBUF];

	for(int i = 0; i < MAXBUF; i++)
		strcpy(temp[i], historybuffer[i]);

	if(isdigit(cline[0][0]))
	{
		int index= 0;
		int length = 0;
		index = atoi(cline[0]);
		cline[0] = temp[index-1];
		length = strlen(cline[0]);
		cline[0][length-1] = '\0';

		if (strstr(cline[0], "cd") != NULL) 
		{ 
			char *p = strtok(cline[0], " ");
			p = strtok(NULL, " ");
			if (chdir(p) == -1)
				fatal("change directory fail");

			return 0;
		}
		if(strstr(cline[0], "history") != NULL)
		{
			int hid = 0;
			while(true)
			{
				if(historybuffer[hid][0] == '\0')
					break;
				printf("%d : %s", hid+1, historybuffer[hid]);
				hid++;
			}
			return 0;
		}
	}
	if (strcmp(cline[0], "cd") == 0) 
	{ 
		if (chdir(cline[1]) == -1)
			fatal("change directory fail");

		return 0;
	}
	if(strcmp(cline[0], "history") == 0)
	{
		int hid = 0;
		while(true)
		{
			if(historybuffer[hid][0] == '\0')
				break;
			printf("%d : %s", hid+1, historybuffer[hid]);
			hid++;
		}
		return 0;
	}

	switch(pid = fork()){
	case -1:
		fatal("fork error");
		return -1;
	case 0: 
		if (back == BACKGROUND) 
		{ 
			act.sa_handler = SIG_IGN;
			sigaction(SIGINT, &act, NULL);	
		} 
		else 
		{
			act.sa_handler = SIG_DFL;
			sigaction(SIGINT, &act, NULL);
		}

		if (pipe == PIPE) 
		{ 
			char* tmp[MAXPIPE] = { 0 };
			char* tmp2[MAXPIPE] = { 0 };
			separate(tmp, tmp2, cline); 
			linkpipe(tmp, tmp2); 

			exit(1);
		}
		execvp(*cline, cline); 
		fatal(*cline);
		exit(1);
	}

	if (back == BACKGROUND) 
	{ 
		printf("[Process ID %d]\n", pid);
		return 0;
	}

	if (waitpid(pid, &status, 0) == -1)
		return -1;
	else
		return status;
}

int linkpipe(char *command1[], char *command2[]) 
{ 
	int files[2], status;
	char* tmp[MAXPIPE] = { 0 };
	char* tmp2[MAXPIPE] = { 0 };
	pid_t pid;

	if (pipe(files) == -1)
		fatal("pipe call in linkpipe");

	pid = fork();
	if(pid == -1)
		fatal("2nd fork error");
	else if(pid == 0)
	{
		close(1);
		dup(files[1]);
		close(files[0]);
		close(files[1]);
		execvp(command1[0], command1);
		fatal("1st execvp call in linkpipe");
	}
	else
	{
		close(0);
		dup(files[0]); 
		close(files[0]);
		close(files[1]);
		separate(tmp, tmp2, command2); 
		if (tmp2[0] == 0) 
		{
			execvp(command2[0], command2);
			fatal("2nd execvp call in linkpipe");
		} 
		else 
			linkpipe(tmp, tmp2);
	}
}
void separate(char *command1[], char *command2[], char *str[]) 
{
	int j = 0;
	for (int i = 0; str[i] != 0; i++) 
	{ 
		char *tmp = str[i]; 
		while (true) 
		{
			if (*tmp == '|') 
			{ 
				j = i + 1;
				while (str[j]) 
				{
					command2[j - i - 1] = str[j];
					j++;
				}
				command2[j - i - 1] = '\0'; 
				return;
			}
			if (*tmp++ == '\0') 
				break;
		}
		command1[i] = str[i];
		command1[i + 1] = 0;
	}
}

int main(void) 
{
	act.sa_handler = SIG_IGN; 
	sigaction(SIGINT, &act, NULL);
	while(input(baseline) != EOF) 
	{ 
		act.sa_handler = SIG_IGN;
		sigaction(SIGINT, &act, NULL);
		if (work() == -1)
			break;
	}
	return 0;
}