#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <setjmp.h>

int desc[2];
static sigjmp_buf go_prompt;
static volatile sig_atomic_t jumpable = 0;

int makeargv(const char *s, const char *delimiters, char ***argvp) 
{
        int error;
        int i;
        int num_argv;
        const char *snew;
        char *t;
        if((s==NULL) || (delimiters==NULL) || (argvp==NULL)) 
	{
                errno = EINVAL;
                return -1;
        }
        *argvp = NULL;
        snew = s+strspn(s, delimiters);
        if((t = (char *)malloc(strlen(snew) + 1)) == NULL)
                return -1;
        strcpy(t, snew);
        num_argv = 0;

        if(strtok(t, delimiters) != NULL)
                for(num_argv = 1; strtok(NULL, delimiters) != NULL; num_argv++);
        if((*argvp = malloc((num_argv +1)*sizeof(char *))) == NULL) 
	{
                error = errno;
                free(t);
                errno = error;
                return -1;
        }
        if(num_argv == 0)
                free(t);
        else 
	{
                strcpy(t, snew);
                **argvp = strtok(t, delimiters);
                for(i=1; i<num_argv; i++)
                        *((*argvp) + i) = strtok(NULL, delimiters);
        }
        *((*argvp) + num_argv) = NULL;

        return num_argv;
}



int make_redirect_in(char *cmd) 
{
        int error;
        int infd;
        char *infile;
        if((infile = strchr(cmd, '<')) == NULL)
                return 0;
        *infile = 0;
        infile = strtok(infile+1, " \t");
        if(infile == NULL)
                return 0;
        if((infd = open(infile, O_RDONLY)) == -1)
                return -1;
        if(dup2(infd, STDIN_FILENO) == -1) 
	{
                error = errno;
                close(infd);
                errno = error;
                return -1;
        }
        return close(infd);
}

int make_redirect_out(char *cmd) 
{
        int error;
        int outfd;
        char *outfile;
        if((outfile = strchr(cmd, '>')) == NULL)
                return 0;
        *outfile = 0;
        outfile = strtok(outfile+1, " \t");

        if(outfile == NULL)
                return 0;

        if((outfd = open(outfile, O_WRONLY)) == -1)
                return -1;

        if(dup2(outfd, STDOUT_FILENO) == -1) 
	{
                error = errno;
                close(outfd);
                errno = error;
                return -1;
        }
        return close(outfd);
}

void executecmd(char *cmds) 
{
        int child;
        int count;
        int fds[2];
        int i;
        char **pipelist;

        count = makeargv(cmds, "|", &pipelist);
        if(count <=0 ) 
	{
                fprintf(stderr, "Failed\n");
                exit(1);
        }
        for(i=0; i<count-1; i++) 
	{
                if(pipe(fds) == -1)
                {
                		perror("Failed to create pipes");
        				exit(1);
       		}
                else if((child = fork()) == -1)                        
                {
                        perror("Failed to create process to run command");
                        exit(1);
                }
                else if(child) 
		{
                        if(dup2(fds[1], STDOUT_FILENO) == -1)                                
                     	{
			       	perror("Failed to connect pipeline");
                                exit(1);
                        }
                        if(close(fds[0]) || close(fds[1]))                               
			{					 
				perror("Failed to close needed files");
                                exit(1);                
                        }
                        executeredirect(pipelist[i], i==0, 0);
                        exit(1);
                }
                if(dup2(fds[0], STDIN_FILENO) == -1)                        
                {
                        perror("Failed to connect last component");
                        exit(1);     
                }            
                if(close(fds[0]) || close(fds[1]))                        
                {
                        perror("Failed to do final close");
                        exit(1);     
                }
        }
        executeredirect(pipelist[i], i==0, 1);
        exit(1);
}

void executeredirect(char *s, int in, int out) 
{
        char **chargv;
        char *pin;
        char *pout;
        int i, j;
        if(in && ((pin = strchr(s, '<')) != NULL) && out && ((pout=strchr(s,'>')) !=NULL) && (pin>pout))
	{ 
                if(make_redirect_in(s) == -1) 
		{
                        perror("Failed to redirect input");
                        return;
                }
        }

        if(out && make_redirect_out(s) == -1)
                perror("Failed to redirect output");
        else if(in && make_redirect_in(s) == -1)
                perror("failed to redirect input");
        else if(makeargv(s, " \t", &chargv) <= 0)
                fprintf(stderr, "failed to parse command line\n");
        else 
	{
                for(i=0; chargv[i] != 0; i++) 
		{
                        for(j=0; chargv[i][j] != 0; j++)  
			{
                                write(desc[1], &chargv[i][j], sizeof(char));
                        }
                        write(desc[1], " ", sizeof(char));
                }
                execvp(chargv[0], chargv);
                perror("failed to execute command");
                write(desc[1], "/5999", sizeof("/5999"));
        }
        exit(1);
}


int signalsetup(struct sigaction *def, sigset_t *mask, void (*handler)(int)) 
{
        struct sigaction catch;
        catch.sa_handler = handler;
        def->sa_handler = SIG_DFL;
        catch.sa_flags = 0;
        def->sa_flags = 0;
        if((sigemptyset(&(def->sa_mask)) == -1) || (sigemptyset(&(catch.sa_mask)) == -1) 
		|| (sigaddset(&(catch.sa_mask), SIGINT) == -1) || (sigaddset(&(catch.sa_mask), SIGQUIT) == -1) 
		|| (sigaction(SIGINT, &catch, NULL) == -1) || (sigaction(SIGQUIT, &catch, NULL) == -1) 
		|| (sigemptyset(mask) == -1) || (sigaddset(mask, SIGINT) == -1) || (sigaddset(mask, SIGQUIT) == -1))
                return -1;

	return 0;
}


static void jhandling(int signalnum) 
{
        if(!jumpable) return;
        jumpable = 0;
        siglongjmp(go_prompt, 1);
}



int main(void) 
{
        pid_t childpid;
        char inbuf[256];
        int len;
        sigset_t blockmask;
        struct sigaction defhandler;
        char *backp;
        int inbackground;
        char **str;
        int tcnt=0;
        char pipebuf[101];
        int j, k;
        int str_len;
        pipe(desc);
        
        if(signalsetup(&defhandler, &blockmask, jhandling) == -1) 
	{
                perror("Failed to set up shell signal handling");
                return 1;
        }
        if(sigprocmask(SIG_BLOCK, &blockmask, NULL) == -1 ) 
	{
                perror("Failed to block signals");
                return 1;
        }
        while(1) 
	{
                if((sigsetjmp(go_prompt, 1)) && (fputs("\n", stdout) ==EOF)) 
                        continue;
      			jumpable = 1;            
                if(fputs("SHELL >> ", stdout) == EOF)
                        continue;
                if(fgets(inbuf, 256, stdin) == NULL)
                        continue;
                len = strlen(inbuf);
                if(inbuf[len-1] == '\n')
                        inbuf[len-1] = 0;
                if(strcmp(inbuf, "exit") == 0)
                        break;
                if((backp = strchr(inbuf, '&')) == NULL)
                        inbackground = 0;
                else 
		{
                        inbackground = 1;
                        *backp = 0;
                }
                if(sigprocmask(SIG_BLOCK, &blockmask, NULL) == -1)
                        perror("Failed to block signals");
                
		makeargv(inbuf, " \t", &str);
				
		if(str[0]=='\0') 
			continue;
				
                if(strcmp(str[0],"cd") == 0) 
		{
                        chdir(str[1]);
                        system("pwd");
                        continue;
                }
                
		for(j=0; j<100; j++)
                        pipebuf[j] = '\0';

                write(desc[1], " ", sizeof(char));
                str_len = read(desc[0], pipebuf, 100);
                pipebuf[str_len]=0;
            
                if((childpid = fork()) == -1)
		{ 
                        perror("Failed to fork child to execute command");
                }
                else if(childpid == 0)
		{ 
                        if(inbackground && (setpgid(0,0) == -1))
                                return 1;
                        if((sigaction(SIGINT, &defhandler, NULL) == -1) 
				|| (sigaction(SIGQUIT, &defhandler, NULL) == -1) 
				|| (sigprocmask(SIG_UNBLOCK, &blockmask, NULL) == -1)) 
			{
                                perror("Failed to set signal handling for command ");
                        	return 1;
                        }
                    	executecmd(inbuf);
                    	return 1;
                }
                if(sigprocmask(SIG_UNBLOCK, &blockmask, NULL) == -1)
                        perror("Failed to unblock signals");
                if(!inbackground)
                        waitpid(childpid, NULL, 0); 
                while(waitpid(-1, NULL, WNOHANG) > 0);
        }
        return 0;
}

