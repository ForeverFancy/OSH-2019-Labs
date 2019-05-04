%{
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"
//#include "command.h"
#define YYSTYPE char *
typedef struct yy_buffer_state * YY_BUFFER_STATE;

int yyerror(char *msg);
int yylex();
extern YY_BUFFER_STATE yy_scan_string(char *str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern struct SimpleCommand simplecommand[100];
extern int command_num[100];
extern int i;
extern int j;
extern int total_command_num;
extern int last_command_num;
int temp_num=0;
%}

%token WORD 
%token GREAT GREATGREAT LESS LESSLESS LESSAMPERSAND GREATAMPERSAND GREATGREATAMPERSAND
%token PIPE
%token AMPERSAND
%token NEWLINE
%token NOTOKEN
%token SEMICOLON AMPERSANDAMPERSAND
%start command_line

%%

io_modifiers:  LESS WORD 
            { 
                //printf("Got < redirection: %s\n", $2);
                char *temp;
                temp=(char *)malloc(sizeof($2));
                temp=strdup($2);
                free($2);
                simplecommand[i].file_src_in=temp;
                simplecommand[i].redirect=1;
                simplecommand[i].redirect_in=1;
            }
            | LESSLESS WORD 
            { 
                //printf("Got << redirection: %s\n", $2);
                //TODO: add "<<". 
            }
            | GREAT WORD 
            { 
                //printf("Got > redirection: %s\n", $2);
                char *temp;
                temp=(char *)malloc(sizeof($2));
                temp=strdup($2);
                free($2);
                simplecommand[i].file_src_out=temp;
                simplecommand[i].redirect=1;
                simplecommand[i].redirect_out_cover=1;
            }
            | GREATGREAT WORD 
            { 
                //printf("Got >> redirection: %s\n", $2); 
                char *temp;
                temp=(char *)malloc(sizeof($2));
                temp=strdup($2);
                free($2);
                simplecommand[i].file_src_out=temp;
                simplecommand[i].redirect=1;
                simplecommand[i].redirect_out_append=1;
            }
            | GREATGREATAMPERSAND WORD 
            { 
                //printf("Got >>& redircection:%s\n", $2);
                //TODO:add ">>&".
            }
            | WORD GREATAMPERSAND WORD 
            { 
                //printf("Got >& redirection:%s >& %s\n", $1,$3);
                char *temp1;
                temp1=(char *)malloc(sizeof($1));
                temp1=strdup($1);
                free($1);
                simplecommand[i].combination_file1=temp1;
                char *temp2;
                temp2=(char *)malloc(sizeof($3));
                temp2=strdup($3);
                free($3);
                simplecommand[i].combination_file2=temp2;
                simplecommand[i].redirect_combination=1;
                simplecommand[i].redirect=1;
            }
            | WORD LESSAMPERSAND WORD 
            { 
                //printf("Got <& redircetion:%s <& %s\n", $1,$3);
                char *temp1;
                temp1=(char *)malloc(sizeof($1));
                temp1=strdup($1);
                free($1);
                simplecommand[i].combination_file1=temp1;
                char *temp2;
                temp2=(char *)malloc(sizeof($3));
                temp2=strdup($3);
                free($3);
                simplecommand[i].combination_file2=temp2;
                simplecommand[i].redirect_combination=1;
                simplecommand[i].redirect=1;
            }
            ;

cmd_and_args: WORD ;

simple_command:  simple_command cmd_and_args {
                     //printf("simple command 1:%s\n", $2);
                    // printf("i=%d j=%d\n",i,j);
                    char *temp;
                    temp=(char *)malloc(sizeof($2));
                    temp=strdup($2);
                    free($2);
                    simplecommand[i].arguments[j] = temp;
                    simplecommand[i].number_of_arguments++;
                    j++;
                    }
                | simple_command io_modifiers 
                | io_modifiers
                | cmd_and_args { 
                    // printf("simple command 2:%s\n", $1); 
                    // printf("i=%d j=%d\n",i,j);
                    char *temp;
                    temp=(char *)malloc(sizeof($1));
                    temp=strdup($1);
                    free($1);
                    simplecommand[i].arguments[j] = temp;
                    simplecommand[i].number_of_arguments++;
                    j++;
                    }
                ;
// To avoid ambiguity, combine the same side in the defination.

pipe_list:      pipe_list PIPE simple_command { 
                    // printf("Got cmd and args for pipe 1:%s\n", $3);
                    simplecommand[i].pipe=1;
                    temp_num++;
                    i++;
                    j=0;
                    //printf("i=%d j=%d\n",i,j);
                    }
                | simple_command { 
                    // printf("Got cmd and args for pipe 2:%s\n",$1); 
                    simplecommand[i].pipe=1;
                    temp_num++;
                    i++;
                    j=0;
                    //printf("i=%d j=%d\n",i,j);
                    };

//pipe_list is the combination of simple_command which is connected by PIPE.

background_option: AMPERSAND 
                { 
                    //printf("Got background &\n");
                    simplecommand[i-1].background=1; 
                }
                | /* empty */;


command_line:   command_line SEMICOLON pipe_list background_option
                {
                    command_num[total_command_num]=temp_num;
                    total_command_num++;
                    temp_num=0;
                }
                | command_line AMPERSANDAMPERSAND pipe_list background_option
                {
                    command_num[total_command_num]=temp_num;
                    total_command_num++;
                    temp_num=0;
                }
                | pipe_list background_option 
                {
                    command_num[total_command_num]=temp_num;
                    total_command_num++;
                    temp_num=0;
                }
                |command_line SEMICOLON background_option
                |/* empty */;


%%
