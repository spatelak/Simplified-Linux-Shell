
/*
 *
 * CS-252 Fall 2013
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "y.tab.h"

extern int loading;
extern "C" char *read_line();

int mygetc(FILE *f) {
  static char *p;
  char ch;

  if (!isatty(0)) {
    // stdin is not a tty. Call real getc
    return getc(f);
  }

  // stdin is a tty. Call our read_line.
  if (p == NULL || *p == 0) {
    char *s = read_line();
    p = s;
  }

  ch = *p;
  p++;
  return ch;
}

#undef getc
#define getc(f) mygetc(f)

static  void yyunput (int c,char *buf_ptr);

void myunputc(int c) {
    unput(c);
}

%}
%x incl
%%
"source"      BEGIN(incl);      /* dealing with import */

<incl>[ \t]*      /* eat the whitespace */
<incl>[^ \t\n]+   { /* got the include file name */
  yyin = fopen(yytext, "r");

  if (!yyin) {
    //error("Unable to open file %s.\n", yytext);
  }
  loading = 1;        /* Disable printing prompts while reading contents from the file. */ 
  yypush_buffer_state(yy_create_buffer(yyin, YY_BUF_SIZE));  /* Put current buffer on stack, and use the file */
  BEGIN(INITIAL);
}

<<EOF>> {
  yypop_buffer_state();
  loading = 0;    /* Enable printing prompts again.  This assumes that we only use import for one level. */ 
  if (!YY_CURRENT_BUFFER) {
    yyterminate();
  }
}  /* End of code supporting source. */



\n 	{
		return NEWLINE;
	}

[ \t] 	{
		/* Discard spaces and tabs */
     	}	

">" 	{
		return GREAT;
	}

"<"     {
                return LESS;
        }

"|"     {
                return PIPE;
        }

"&"     {
                return AMPERSAND;
        }

">&"    {
                return GREATAMPERSAND;
        }

">>"    {
                return GREATGREAT;
        }

">>&"   {
                return GREATGREATAMPERSAND;
        }

\`[^\n\`]*\` {
                yytext[strlen(yytext++) - 1] = '\0';
                yylval.string_val = strdup(yytext);
                char *arg = (char *) malloc ((strlen(yytext) + 7) * sizeof(char *));
                strcpy(arg, yytext);
                strcat(arg, "\nexit\n");

                // set up the pipes
                int pipe1[2];
                int pipe2[2];
                int defaultin  = dup(0);
                int defaultout = dup(1);
                pipe(pipe1);

                // write the command inside backtics to the pipe
                write(pipe1[1], arg, strlen(arg));
                close(pipe1[1]);
                pipe(pipe2);

                int pid = fork();
                if (pid == 0) {
                  //redirect stdin
                  dup2(pipe1[0], 0);
                  close(pipe1[0]);

                  //redirect stdout and close file desciptors
                  dup2(pipe2[1], 1);
                  close(pipe2[1]);
                  close(pipe2[0]);

                  // create subshell
                  char *args[2];
                  args[0] = (char *) "/proc/self/exe";
                  args[1] = NULL;
                  execvp("/proc/self/exe", args);      
                }
       
                waitpid(pid, NULL, 0);
                close(pipe1[0]);
                close(pipe2[1]);

                // buffer to read output of subshell
                char *buffer = (char *) calloc(sizeof(char), 5000);
                char *temp = buffer;
                int i = 0;

                // read the output
                while (read(pipe2[0], temp, 1) != 0) {
                  if (*temp == '\n') *temp = ' ';
                  temp++;
                  i++;
                }

               close(pipe2[0]);
               buffer[i--] = '\0';

                // close file descriptors and send the output from subshell back to lex input buffer in reverse order
                dup2(defaultout, 1);
                dup2(defaultin, 0);
                close(defaultin);
                close(defaultout);
                while (i >= 0) yyunput(buffer[i--], yytext);
                free(arg);
         }

[^ \t\n<>&\|]*(\\.[^ \t\n<>&\|]*)* { 
                yylval.string_val = strdup(yytext);
                int i = 0, j = 0;

                for (i = 0; i < strlen(yytext); i ++) {
		  if (yytext[i] == '\\') yylval.string_val[j++] = yytext[++i];
		  else                   yylval.string_val[j++] = yytext[i];     
	        }

		yylval.string_val[j] = '\0';
		return WORD;
        }

\"[^\n\"]*\" {
                yytext[strlen(yytext++) - 1] = '\0';
                yylval.string_val = strdup(yytext);
                return WORD;
        }
