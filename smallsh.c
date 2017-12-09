#include "smallsh.h" /* include file for example */
#include <stdlib.h>
#include <signal.h>
#define redOutput 6
#define redInput 7
#define symPipe 8
/* program buffers and work pointers */
static char inpbuf[MAXBUF], tokbuf[2*MAXBUF], *ptr = inpbuf, *tok = tokbuf;
int intr_p = 0;
char *prompt = "Command>"; /* prompt */
int fg_pid = 0;
int isRedirectOutput = 0;
char* outFile = NULL;
int isRedirectInput = 0;
char* inFile = NULL;
int inPipe = 0;
int outPipe = 0;


int fd[2];
int fd2[2];
int prevFd;
void parentChdir()
{
	char temp[BUFSIZE];
	read(fd2[READ], temp, BUFSIZE);
	//printf("dir path : %s\n", temp);
	chdir(temp);
}
void cmdIsCd(char* input)
{
	//printf("%s\n", input);
	if((strlen(input)-1)==2) //cmd is 'cd'
	{
		write(fd2[WRITE], "/", BUFSIZE);
		kill(getppid(), SIGUSR1);
	}
	else if((strlen(input)-1) == 3) //cmd is 'cd_'
	{
		if(input[2] == ' ') // cmd is 'cd '
		{
			write(fd2[WRITE], "/", BUFSIZE);
			kill(getppid(), SIGUSR1);
		}
	}
	else //cmd is 'cd dir_path'
	{
		char *piece = strtok(input, " "); //str split -> ' '
		piece = strtok(NULL, " "); //get dir path
		char *index = strchr(piece, '\n'); //fine \n
		*index = '\0'; // \n to \0
		//printf("piece : %s\n", piece);
		if(!chdir(piece))
		{
			write(fd2[WRITE], piece, BUFSIZE);
			kill(getppid(), SIGUSR1);
		}
	}
}
int count;
int userin(char *p) /* print prompt and read a line */
{
	int c;
	/* initialization for later routines */
	ptr = inpbuf;
	tok = tokbuf;

	/* display prompt */
	printf("%s ", p);

	for(count = 0;;){
		c = getchar(); //user input //getchar == scanf
		if(c == EOF)
			return(EOF);

		if(count < MAXBUF)
			inpbuf[count++] = c;

		if(c == '\n' && count < MAXBUF){
			inpbuf[count] = '\0';
			return(count);
		}

		/* if line too long restart */
		if(c == '\n'){
			printf("smallsh: input line too long\n");
			count = 0;
			printf("%s ", p);
		}
	}
}

static char special[] = {' ', '\t', '&', ';', '<', '>', '|', '\n', '\0'};

int inarg(char c) /* are we in an ordinary argument */
{
	char *wrk;
	for(wrk = special; *wrk != '\0'; wrk++)
		if(c == *wrk)
			return(0); //break in while

	return(1); // continue...
}

int gettok(char **outptr) /* get token and place into tokbuf */
{
	int type;
	*outptr = tok; // tok = tokbuf (type :char)
	/* strip white space */
	for(;*ptr == ' ' || *ptr == '\t'; ptr++)
		;

	*tok++ = *ptr;

	switch(*ptr++){ 
		case '\n':
			type = EOL; break;
		case '&':
			type = AMPERSAND; break;
		case ';':
			type = SEMICOLON; break;
		case '>':
			type = redOutput; break;
		case '<':
			type = redInput; break;
		case '|':
			type = symPipe; break;
		default:
			type = ARG;
			while(inarg(*ptr))
				*tok++ = *ptr++;
	}

	*tok++ = '\0';
	return(type);
}

void handle_int(int s) {
	int c;
	if(!fg_pid) {
		/* ctrl-c hit at shell prompt */
		return;
	}
	if(intr_p) {
		printf("\ngot it, signalling\n");
		kill(fg_pid, SIGTERM);
		fg_pid = 0;
	} else {
		printf("\nignoring, type ^C again to interrupt\n");
		intr_p = 1;
	}
}
/* execute a command with optional wait */
int runcommand(char **cline,int where)
{
	int pid, exitstat, ret;
	/* ignore signal (linux) */
	struct sigaction sa_ign, sa_conf;
	sa_ign.sa_handler = SIG_IGN;
	sigemptyset(&sa_ign.sa_mask);
	sa_ign.sa_flags = SA_RESTART;
	//sa_restart : 시그널 핸들러에 의해 중지된 시스템 호출을 자동적으로 재시작

	sa_conf.sa_handler = handle_int;
	sigemptyset(&sa_conf.sa_mask);
	sa_conf.sa_flags = SA_RESTART;


	if((pid = fork()) < 0){
		perror("smallsh");
		return(-1);
	}
	if(!strcmp(*cline, "exit")) //cmd is "exit"
	{
		//kill(getppid(), SIGKILL);
		exit(0); //terminated
	}
	
	if(pid == 0){ //child
		close(fd2[READ]);
		sigaction(SIGINT, &sa_ign, NULL);
		sigaction(SIGQUIT, &sa_ign, NULL);
		sigaction(SIGTSTP, &sa_ign, NULL);
		
		/*PIPE*/
		if(inPipe)
		{
			dup2(prevFd, 0);
			close(prevFd);
			//close(fd[WRITE]);
		}
		if(outPipe)
		{
			dup2(fd[WRITE], 1);
			close(fd[WRITE]);
		}

		//close(fd[READ]);

		/*REDIRECT*/
		if(isRedirectOutput)
		{
			int outfd;
			outfd = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if(outfd == -1)
			{
				printf("open err(out)\n");
				exit(-1);
			}
			dup2(outfd,1);
			close(outfd);
		}
		if(isRedirectInput)
		{
			int infd;
			infd = open(inFile, O_RDONLY);
			if(infd == -1)
			{
				printf("open err(in)\n");
				exit(-1);
			}
			dup2(infd, 0);
			close(infd);
		}
		/*CD*/
		if(!strncmp(*cline, "cd", 2)) // cd cmd
		{
			cmdIsCd(inpbuf);
			exit(0);
		}
		execvp(*cline, cline);
		perror(*cline);
		exit(127);
	} else { //parent
		fg_pid = pid;
	}
	/* code for parent */
	/* if background process print pid and exit */
	if(where == BACKGROUND){
		fg_pid = 0;
		printf("[Process id %d]\n", pid);
		return(0);
	}
	/* wait until process pid exits */
	sigaction(SIGINT, &sa_conf, NULL);
	sigaction(SIGQUIT, &sa_conf, NULL);
	sigaction(SIGTSTP, &sa_conf, NULL);

	while( (ret=wait(&exitstat)) != pid && ret != -1)
		;

	if(inPipe)
	{
		close(prevFd);
		inPipe = 0;
	}
	if(outPipe)
	{
		close(fd[WRITE]);
		outPipe = 0;
		inPipe = 1;
		prevFd = fd[READ];
	}

	fg_pid = 0;
	return(ret == -1 ? -1 : exitstat);
}

void procline() /* process input line */
{
	char *arg[MAXARG+1]; /* pointer array for runcommand */
	int toktype; /* type of token in command */
	int narg; /* numer of arguments so far */
	int type; /* FOREGROUND or BACKGROUND? */
	isRedirectOutput = 0;
	isRedirectInput = 0;
	inPipe = 0;
	outPipe = 0;
	/* reset intr flag */
	intr_p = 0;

	for(narg = 0;;){ /* loop forever */
		/* take action according to token type */
		switch(toktype = gettok(&arg[narg])){
			case ARG:
				if(narg < MAXARG)
					narg++;
				break;
			case redOutput:
				isRedirectOutput = 1;
				gettok(&outFile);
				break;
			case redInput:
				isRedirectInput = 1;
				gettok(&inFile);
				break;
			case symPipe:
				pipe(fd);
				outPipe = 1;
			case EOL: //end of line
			case SEMICOLON:
			case AMPERSAND:
				type = (toktype == AMPERSAND) ? BACKGROUND : FOREGROUND; //kind of type is binary ( BACKGROUND or FOREGROUND )

				if(narg != 0){ // narg is number of argument
					arg[narg] = NULL;
					runcommand(arg, type);
				}
				if(toktype == EOL)
					return;
				narg = 0;
				break;
		}
	}
}
void fpe(int signo){
	if(signo==2){
		printf("Cauht SIGINT\n");
	}
	else if(signo==3){
		printf("Caught SIGQUIT\n");
	}
	else if(signo==20)
	{
		printf("Caught SIGTSTP\n");
	}
	else
	{
		printf("signal err\n");
	}

}

int main()
{
	pipe(fd2);
	/* sigaction struct (linux) */
	struct sigaction sa, dirAct;
	sa.sa_handler = fpe;
	sigfillset(&sa.sa_mask);
	//sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	dirAct.sa_handler = parentChdir;
	sigemptyset(&dirAct.sa_mask);
	sigaction(SIGUSR1, &dirAct, NULL);

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTSTP, &sa, NULL);

	//printf("%d", userin(prompt)); //return cmd length & userinput value = inpbuf
	while(userin(prompt) != EOF)
		procline();
	return 0;
}

