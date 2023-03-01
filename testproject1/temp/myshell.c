#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define MAX_BUFFER 1024    // max line buffer
#define MAX_ARGS 64        // max # args
#define SEPARATORS " \t\n" // token sparators
#define README "/readme"   // help file name
#define MYSHELL "/myshell" // shell path

struct shellstatus_st
{
    int foreground;  // foreground execution flag
    char *infile;    // input redirection flag & file
    char *outfile;   // output redirection flag & file
    char *outmode;   // output redirection mode
    char *shellpath; // full pathname of shell
};
typedef struct shellstatus_st shellstatus;

extern char **environ;

void check4redirection(char **, shellstatus *); // check command line for i/o redirection
void errmsg(char *, char *);                    // error message printout
void execute(char **, shellstatus);             // execute command from arg array
char *getcwdstr(char *, int);                   // get current work directory string
FILE *redirected_op(shellstatus);               // return required o/p stream
char *stripath(char *);                         // strip path from filename
void syserrmsg(char *, char *);                 // system error message printout

/*******************************************************************/

int main(int argc, char **argv)
{
    FILE *ostream = stdout;   // (redirected) o/p stream
    FILE *instream = stdin;   // batch/keyboard input
    char linebuf[MAX_BUFFER]; // line buffer
    char cwdbuf[MAX_BUFFER];  // cwd buffer
    char *args[MAX_ARGS];     // pointers to arg strings
    char **arg;               // working pointer thru args
    char *prompt = "==>";     // shell prompt
    char *readmepath;         // readme pathname
    shellstatus status;       // status structure

    // parse command line for batch input
    switch (argc)
    {
    case 1:
    {
        // keyboard input
        // just "myshell",continue
        break;
    }

    case 2:
    {
        // possible batch/script
        instream = fopen(argv[1], "r");
        // open failed
        if(NULL == instream)
        {
            syserrmsg(argv[1], NULL);
            instream = stdin;
        }
        break;
    }
    default: // too many arguments
        fprintf(stderr, "%s command line error; max args exceeded\n"
                        "usage: %s [<scriptfile>]",
                stripath(argv[0]), stripath(argv[0]));
        exit(1);
    }

    // get starting cwd
    getcwdstr(cwdbuf, MAX_BUFFER);

    // add starting cwd to readme pathname
    readmepath = (char*) malloc(sizeof(char) * MAX_BUFFER); // allocate space
    strcpy(readmepath, cwdbuf); // the buffer will not overflow
    // absolute path
    strncat(readmepath, README, MAX_BUFFER-strlen(readmepath)-1); // make sure the buffer will not overflow

    // add starting cwd to shell pathname
    status.shellpath = (char*) malloc(sizeof(char) * MAX_BUFFER);
    strcpy(status.shellpath, cwdbuf);
    // absolute path
    strncat(status.shellpath, MYSHELL, MAX_BUFFER-strlen(status.shellpath)-1);

    // set SHELL = environment variable, malloc and store in environment
    setenv("SHELL", status.shellpath, 1);

    // prevent ctrl-C and zombie children
    signal(SIGINT, SIG_IGN);  // prevent ^C interrupt
    signal(SIGCHLD, SIG_IGN); // prevent Zombie children

    // keep reading input until "quit" command or eof of redirected input
    while (!feof(instream))
    {
        // (re)initialise status structure
        status.foreground = TRUE;

        // get cwd again after "cd" and set prompt
        getcwdstr(cwdbuf, MAX_BUFFER);
        prompt = strncat(cwdbuf, "==>", MAX_BUFFER-strlen(cwdbuf)-1);

        // only print prompt when argc == 1
        if (1 == argc) 
        {
            fprintf(stdout, "%s", prompt);
            // fflush stdout and output prompt
            fflush(stdout);
        }

        // get command line from input
        if (fgets(linebuf, MAX_BUFFER, instream))
        {
            // read a line
            // tokenize the input into args array
            arg = args;
            *arg++ = strtok(linebuf, SEPARATORS); // tokenize input
            while ((*arg++ = strtok(NULL, SEPARATORS)))
                ;
            // last entry will be NULL
            if (args[0])
            {
                // check for i/o redirection
                check4redirection(args, &status);

                // redirect output if necessary
                ostream = redirected_op(status);
                // redirection failed
                if (NULL == ostream)
                    continue;

                // check for internal/external commands
                // "cd" command
                if (!strcmp(args[0], "cd"))
                {
                    if (NULL == args[1])
                    {
                        // no arguments
                        getcwdstr(cwdbuf, MAX_BUFFER);
                        printf("%s\n", cwdbuf);
                    }
                    else{
                        if (!chdir(args[1]))
                        {
                            // chdir successfully
                            getcwdstr(cwdbuf, MAX_BUFFER);
                            setenv("PWD", cwdbuf, 1);  // change PWD
                        }
                        else
                            // chdir failed
                            syserrmsg(args[1], NULL);
                    }
                }
                // "clr" command
                else if (!strcmp(args[0], "clr"))
                {
                    // fork and execute
                    args[0] = "clear"; 
                    execute(args, status);
                }
                // "dir" command(consider redirection)
                else if (!strcmp(args[0], "dir"))
                {
                    // output in "-al" mode
                    int i = 1;
                    while (NULL != args[i])
                        i++;
                    // output in "-al" mode
                    args[i] = "-al";
                    args[i+1] = NULL;
                    // fork and execute
                    execute(args, status);
                }
                // "echo" command(consider redirection)
                else if (!strcmp(args[0], "echo"))
                {
                    // print arguments and add space
                    int i;
                    for(i = 1; NULL != args[i]; i++)
                    {
                        fprintf(ostream, "%s ", args[i]);
                    }
                    // print '\n'
                    fprintf(ostream, "%c", '\n');
                }
                // "environ" command(consider redirection)
                else if (!strcmp(args[0], "environ"))
                {
                    // print environment variables
                    int i;
                    for (i = 0; NULL != environ[i]; i++)
                        fprintf(ostream, "%s\n", environ[i]);
                }
                // "help" command(consider redirection)
                else if (!strcmp(args[0], "help"))
                {
                    args[0] = "more";
                    args[1] = readmepath;
                    args[2] = NULL;
                    // fork and execute
                    execute(args, status);
                }
                // "pause" command - note use of getpass - this is a made to measure way of turning off
                //  keyboard echo and returning when enter/return is pressed
                else if (!strcmp(args[0], "pause"))
                {
                    printf("Press enter/return to continue:\n");
                    while (getchar() != '\n')
                        continue;
                }
                // "quit" command
                else if (!strcmp(args[0], "quit"))
                {
                    break;
                }
                // else pass command on to OS shell
                else
                {
                    execute(args, status);
                }
            }
        }
        // turn off redirection otuput file
        if (stdout != ostream)
            fclose(ostream);
    }
    // close batch file
    if (2 == argc)
        fclose(instream);

    return 0;
}

/***********************************************************************

void check4redirection(char ** args, shellstatus *sstatus);

check command line args for i/o redirection & background symbols
set flags etc in *sstatus as appropriate

***********************************************************************/

void check4redirection(char **args, shellstatus *sstatus)
{
    sstatus->infile = NULL; // set defaults
    sstatus->outfile = NULL;
    sstatus->outmode = NULL;

    // prevent cross-border error
    int i = 0;

    // search for "<",">",">>" and "&"
    while (args[i] && i < MAX_ARGS)
    {
        // input redirection
        if (!strcmp(args[i], "<"))
        {
            // remove "<"
            args[i] = NULL;
            i++;
            // set redirection input file
            sstatus->infile = args[i];
            // remvoe <directory> and move other arguments forward
            int k = i-2;  // let i++ = the position of "<"
            while (args[i] && i < MAX_ARGS - 1)
            {
                args[i-1] = args[i+1];
                i++;
            }
            i = k;
        }
        // output direction
        else if (!strcmp(args[i], ">") || !strcmp(args[i], ">>"))
        {
            // set outmode
            if (!strcmp(args[i], ">"))
                sstatus->outmode = "w";
            else
                sstatus->outmode = "a";
            // remove ">" or "<<"
            args[i] = NULL;
            i++;
            // set redirect output file
            sstatus->outfile = args[i];
            // remove <directory>
            args[i] = NULL;
        }
        // background execution
        else if (!strcmp(args[i], "&"))
        {
            // set background execution
            sstatus->foreground = FALSE;
            // remove "&" and move other arguments forward
            int j = i-1;  // let i++ = the position of "&"
            while (args[i] && i < MAX_ARGS - 1)
            {
                args[i] = args[i+1];
                i++;
            }
            i = j;
        }
        i++;
    }
}

/***********************************************************************

  void execute(char ** args, shellstatus sstatus);

  fork and exec the program and command line arguments in args
  if foreground flag is TRUE, wait until pgm completes before
    returning

***********************************************************************/

void execute(char **args, shellstatus sstatus)
{
    int status;
    pid_t child_pid;

    switch (child_pid = fork())
    {
    case -1:
        syserrmsg("fork", NULL);
        break;
    case 0:
        // execution in child process
        // reset ctrl-C and child process signal trap
        signal(SIGINT, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        // output redirection 
        if (NULL != sstatus.outfile)
        {
            if (NULL == freopen(sstatus.outfile, sstatus.outmode, stdout))
            {
                // redirection failed
                syserrmsg(sstatus.outfile, NULL);
                exit(1);
            }
        }
        // input redirection
        if (NULL != sstatus.infile)
        {
            if (NULL == freopen(sstatus.infile, "r", stdin))
            {
                // redirection failed
                syserrmsg(sstatus.infile, NULL);
                exit(1);
            }
        }

        // set PARENT = environment variable, malloc and store in environment
        setenv("PARENT", sstatus.shellpath, 1);

        // final exec of program
        execvp(args[0], args);
        // execute failed
        syserrmsg("exec failed -", args[0]);
        exit(127);
    }

    // continued execution in parent process
    // background execution
    if (!sstatus.foreground)
    {
        waitpid(child_pid, &status, WNOHANG);
        printf("child_process id %d\n", child_pid);
    }
    // foreground execution
    else
        waitpid(child_pid, &status, 0);
}

/***********************************************************************

 char * getcwdstr(char * buffer, int size);

return start of buffer containing current working directory pathname

***********************************************************************/

char *getcwdstr(char *buffer, int size)
{
    getcwd(buffer, size);
    return buffer;
}

/***********************************************************************

FILE * redirected_op(shellstatus ststus)

  return o/p stream (redirected if necessary)

***********************************************************************/

FILE *redirected_op(shellstatus status)
{
    FILE *ostream = stdout;

    if (NULL != status.outfile)
    {
        ostream = fopen(status.outfile, status.outmode);
        if (NULL == ostream)
        {
            syserrmsg(status.outfile, NULL);
            ostream = NULL;
        }
    }

    return ostream;
}

/*******************************************************************

  char * stripath(char * pathname);

  strip path from file name

  pathname - file name, with or without leading path

  returns pointer to file name part of pathname
            if NULL or pathname is a directory ending in a '/'
                returns NULL
*******************************************************************/

char *stripath(char *pathname)
{
    char *filename = pathname;

    if (filename && *filename)
    {                                      // non-zero length string
        filename = strrchr(filename, '/'); // look for last '/'
        if (filename)                      // found it
            if (*(++filename))             // AND file name exists
                return filename;
            else
                return NULL;
        else
            return pathname; // no '/' but non-zero length string
    }                        // original must be file name only
    return NULL;
}

/********************************************************************

void errmsg(char * msg1, char * msg2);

print an error message (or two) on stderr

if msg2 is NULL only msg1 is printed
if msg1 is NULL only "ERROR: " is printed
*******************************************************************/

void errmsg(char *msg1, char *msg2)
{
    fprintf(stderr, "ERROR: ");
    if (msg1)
        fprintf(stderr, "%s; ", msg1);
    if (msg2)
        fprintf(stderr, "%s; ", msg2);
    return;
    fprintf(stderr, "\n");
}

/********************************************************************

  void syserrmsg(char * msg1, char * msg2);

  print an error message (or two) on stderr followed by system error
  message.

  if msg2 is NULL only msg1 and system message is printed
  if msg1 is NULL only the system message is printed
 *******************************************************************/

void syserrmsg(char *msg1, char *msg2)
{
    errmsg(msg1, msg2);
    perror(NULL);
    return;
}
