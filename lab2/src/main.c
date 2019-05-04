#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "main.h"
#include "y.tab.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <fcntl.h>
#include <ctype.h>

#define MAX_LENGTH 4096
#define BUILDIN_COMMAND_NUM 5

typedef struct yy_buffer_state * YY_BUFFER_STATE;

int yyparse();
int yyerror(char *msg);
int yylex();
extern YY_BUFFER_STATE yy_scan_string(char *str);
extern  void yy_delete_buffer(YY_BUFFER_STATE buffer);

struct SimpleCommand simplecommand[MAX_LENGTH];
int last_command_num=0;
int total_command_num=0;
int command_num[MAX_LENGTH];
int i=0;
int j=0;

char *buildin[]={"cd","pwd","exit","env","export"};
extern char **__environ;

int yyerror(char *msg) 
{
    printf("%s\n", msg);
    return 1;
}

int yywrap()
{
    return 1;
}

void getName(char name[])
{
    uid_t id=getuid();
    struct passwd *pwd;
    pwd = getpwuid(id);
    strcpy(name,pwd->pw_name);
}

void getHostName(char *hostname, int length)
{
    int k=gethostname(hostname,length);
    if(k)
        perror("Hostname calling error");
    return ;
}

int notRoot()
{
    uid_t euid=geteuid();
    return euid;
}

void getPrompt(char prompt[])
{
    char name[MAX_LENGTH]; 
    getName(name);
    
    char hostname[MAX_LENGTH];
    char wd[MAX_LENGTH];
    char hint[2];
    getHostName(hostname,sizeof(hostname));
    getcwd(wd,sizeof(wd));
    if(notRoot())
        hint[0]='$';
    else
        hint[0]='#';
    hint[1]='\0';
    strcpy(prompt,name);
    strcat(prompt,"@\0");
    strcat(prompt,hostname);
    strcat(prompt,":\0");
    strcat(prompt,wd);
    strcat(prompt,hint);
    strcat(prompt," \0");
}

void init(struct SimpleCommand sc[], int length)
{
    memset(sc,0,length*sizeof(struct SimpleCommand));
    last_command_num=0;
    total_command_num=0;
    i=0;
    j=0;
    memset(command_num,0,MAX_LENGTH*sizeof(int));
}

void handle_error(char *msg, int child_process)
{
    perror(msg);
    if(child_process)
    {
        _exit(1);
    }
    else
    {
        exit(1);
    }


}

int check_background(struct SimpleCommand sc[], int num)
{
    for(int k=0;k<num;k++)
    {
        if(sc[k].background)
            return 1;
    }
    return 0;
}

int is_buildin(struct SimpleCommand sc)
{
    for(int k=0;k<BUILDIN_COMMAND_NUM;k++)
    {
        if(strcmp(sc.arguments[0],buildin[k])==0)
            return 1;
    }
    return 0;
}

int exec_buildin(struct SimpleCommand sc)
{
    if (strcmp(sc.arguments[0], "cd") == 0) 
    {
        if (sc.arguments[1])
            if(chdir(sc.arguments[1])==-1)
                perror("Error during changing directory");
        return 1;
    }

    if (strcmp(sc.arguments[0], "pwd") == 0) 
    {
        char wd[4096];
        puts(getcwd(wd, sizeof(wd)));
        return 1;
    }
   
    if (strcmp(sc.arguments[0], "exit") == 0)
    {
        exit(0);
    }
        
    
    if (strcmp(sc.arguments[0], "env") == 0)
    {
        char **var;
        for (var = __environ;*var !=NULL;++var)
            printf ("%s\n ",*var);
        printf("\n");
        return 1;
    }

    if (strcmp(sc.arguments[0], "export") == 0)
    {
        char temp[MAX_LENGTH];
        strcpy(temp,sc.arguments[1]);
        char *q=strchr(temp,'=');
        if(q==NULL)
            return 1;
        else
            *q='\0';
        setenv(temp,q+1,1);
        return 1;
    }

    return 0;
}

int exec_redirect(struct SimpleCommand sc)
{
    if (sc.redirect_combination && sc.redirect_out_cover)
    {
        int cmb1 = atoi(sc.combination_file1);
        int cmb2 = atoi(sc.combination_file2);

        int fdout = open(sc.file_src_out, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
        if (dup2(fdout, cmb1) == -1)
            handle_error("Error during redirecting out file.", 1);
        if (dup2(fdout, cmb2) == -1)
            handle_error("Error during redirecting out file.", 1);
        if (close(fdout) == -1)
            handle_error("Error during closing file.", 1);
        if (close(fdout) == -1)
            handle_error("Error during closing file.", 1);
    }
    if (sc.redirect_combination && sc.redirect_out_append)
    {
        int cmb1 = atoi(sc.combination_file1);
        int cmb2 = atoi(sc.combination_file2);

        int fdout = open(sc.file_src_out, O_CREAT | O_WRONLY | O_APPEND, S_IRWXU);
        if (dup2(fdout, cmb1) == -1)
            handle_error("Error during redirecting out file.", 1);
        if (dup2(fdout, cmb2) == -1)
            handle_error("Error during redirecting out file.", 1);
        if (close(fdout) == -1)
            handle_error("Error during closing file.", 1);
        if (close(fdout) == -1)
            handle_error("Error during closing file.", 1);
    }
    if (sc.redirect_in)
    {
        int fdin = open(sc.file_src_in, O_RDONLY);
        if (fdin <= -1)
            handle_error("Error during opening file", 1);
        if (dup2(fdin, STDIN_FILENO) == -1)
            handle_error("Error during redirecting in file", 1);
        if (close(fdin) == -1)
            handle_error("Error during closing file", 1);
    }
    if (!sc.redirect_combination && sc.redirect_out_cover)
    {
        int fdout = open(sc.file_src_out, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
        if (dup2(fdout, STDOUT_FILENO) == -1)
            handle_error("Error during redirecting in file", 1);
        if (close(fdout) == -1)
            handle_error("Error during closing file", 1);
    }
    if (!sc.redirect_combination && sc.redirect_out_append)
    {
        int fdout = open(sc.file_src_out, O_CREAT | O_WRONLY | O_APPEND, S_IRWXU);
        if (dup2(fdout, STDOUT_FILENO) == -1)
            handle_error("Error during redirecting in file", 1);
        if (close(fdout) == -1)
            handle_error("Error during closing file", 1);
    }

    execvp(sc.arguments[0], sc.arguments);
    perror("Error during executing");

    return 0;
}

int exec_pipeline(struct SimpleCommand sc[], int index, int num, int fdin)
{
    int fin;
    fin = fdin;
    if (num >= 2 && sc[index + 0].redirect_in)
    {
        int f_redirect = open(sc[index + 0].file_src_in, O_RDONLY);
        if (f_redirect <= -1)
            handle_error("No such file", 1);
        fin = f_redirect;
    }

    for (int k = 0; k < num - 1; k++)
    {
        int fd[2];
        if (pipe(fd) == -1)
        {
            handle_error("Error during building pipe", 0);
            return -1;
        }
        pid_t pid = fork();
        if (pid == 0)
        {
            if (dup2(fin, STDIN_FILENO) == -1)
                handle_error("Error during redirecting in file", 1);

            if (close(fin) == -1)
                handle_error("Error during closing file", 1);

            if (close(fd[0]) == -1)
                handle_error("Error during closing file", 1);

            if (dup2(fd[1], STDOUT_FILENO) == -1)
                handle_error("Error during redirecting out file", 1);

            if (close(fd[1]) == -1)
                handle_error("Error during closing file", 1);

            execvp(sc[k+index].arguments[0], sc[k+index].arguments);
            perror("Error during executing");
        }
        else
        {
            if (close(fd[1]))
                handle_error("Error during closing file", 1);
            wait(NULL);
            fin = fd[0];
        }
    }
    if (fin != STDIN_FILENO)
    {
        dup2(fin, STDIN_FILENO);
        close(fin);
    }
    if (sc[index + num - 1].redirect)
    {
        exec_redirect(sc[index + num - 1]);
    }
    else
    {
        execvp(sc[index + num - 1].arguments[0], sc[index + num - 1].arguments);
        perror("Error during exectuing");
    }
    return 1;
}

int check_bg_done(int *jobs, int pid_bg)
{
    // if (*jobs >= 1)
    // {
    //     int pr;
    //     int status;
    //     pr = waitpid(pid_bg, &status, WNOHANG);
    //     if (pr != 0)
    //     {
    //         printf("[%d] + %d done %d\n", *jobs, pid_bg, status);
            
    //         (*jobs)--;
    //         return 1;
    //     }
    // }
    return 0;
}

int main(void)
{
    // int jobs=0;
    // pid_t pid_bg;
    init(simplecommand, MAX_LENGTH);
    for(;;)
    {
        //check_bg_done(&jobs,pid_bg);
        init(simplecommand, MAX_LENGTH);
        fflush(stdin);
        fflush(stdout);

        char *cmd;
        char prompt[MAX_LENGTH];

        getPrompt(prompt);

        if ((cmd = readline(prompt)) != NULL)
        {
            if (*cmd)
                add_history(cmd);
        }

        YY_BUFFER_STATE buffer = yy_scan_string(cmd);
        yyparse();
        yy_delete_buffer(buffer);
        
        for (int k = 0; k < total_command_num; k++)
        {
            int index;
            if (k == 0)
                index = 0;
            else
                index += command_num[k - 1];
            
            if (command_num[k] == 0)
                continue;
            if (command_num[k] == 1 && exec_buildin(simplecommand[index]))
                continue;

            else
            {
                if (check_background(simplecommand, command_num[k]) == 0)
                {
                    pid_t pid = fork();
                    if (pid == 0)
                    {
                        exec_pipeline(simplecommand, index, command_num[k], STDIN_FILENO);
                        _exit(EXIT_FAILURE);
                    }
                    int status;
                    waitpid(pid, &status, 0);
                    continue;
                }
                else
                {
                    pid_t pid = fork();
                    if (pid == 0)
                    {
                        setpgid(0, 0);
                        exec_pipeline(simplecommand, index, command_num[k], STDIN_FILENO);
                        printf("Command not found\n");
                        _exit(1);
                    }
                    //jobs++;
                    //printf("[%d] %d\n",jobs,pid_bg);
                    continue;
                }
            }
        }
    }
    return 0;
}