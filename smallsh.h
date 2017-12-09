#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <wait.h>

#define EOL 1     /* end of line */
#define ARG 2     /* normal argument */
#define AMPERSAND 3
#define SEMICOLON 4
#define INTERRUPT 5

#define MAXARG 512  /* max. no. command args */
#define MAXBUF 512  /* max. length input line */
#define BUFSIZE 512
#define READ 0
#define WRITE 1
#define FOREGROUND 0
#define BACKGROUND 1
