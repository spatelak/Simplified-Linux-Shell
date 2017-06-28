
/*
 * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 */

%token	<string_val> WORD

%token 	NOTOKEN PIPE GREAT LESS GREATAMPERSAND GREATGREAT GREATGREATAMPERSAND AMPERSAND NEWLINE

%union	{
		char   *string_val;
	}

%{
//#define yylex yylex
#include <stdio.h>
#include <string.h>
#include "command.h"
void yyerror(const char * s);
int yylex();

%}

%%

goal:	
  commands
;

commands: 
  command  
 |  
  commands command
;

command:
   simple_command
;

simple_command:	
  pipe_list iomodifier_list background NEWLINE {
    //printf("   Yacc: Execute command\n");
    Command::_currentCommand.execute();
  }
 | 
  NEWLINE {
    Command::_currentCommand.clear();
    Command::_currentCommand.prompt();
  }
 |
  error NEWLINE { yyerrok; }
;

pipe_list:
  pipe_list PIPE command_and_args {
  }
 |
  command_and_args  
;

command_and_args:
  command_word argument_list {
    Command::_currentCommand.insertSimpleCommand( Command::_currentSimpleCommand );
  }
;

argument_list:
  argument_list argument  
 | /* can be empty */  
;

argument:
  WORD {
    $1 = Command::_currentCommand.expandEnvVar($1);
    
    if (strchr($1, '~') != NULL)
      $1 = Command::_currentCommand.expandTilde($1);
       
    Command::_currentCommand.expandWildcardsIfNecessary($1);
  }
;

command_word:
  WORD {
    //printf("   Yacc: insert command \"%s\"\n", $1);	       
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument( $1 );
  }
;

background:
  AMPERSAND {
    Command::_currentCommand._background = 1;
  }
 |
;

iomodifier_list:
  iomodifier_list iomodifier_opt
  |
;

iomodifier_opt:
  GREAT WORD {
    //printf("   Yacc: insert output \"%s\"\n", $2);
    if (Command::_currentCommand._outFile != NULL) yyerror("Ambiguous output redirect\n");
    Command::_currentCommand._outFile = $2;
  }
 |
  LESS WORD {
    //printf("   Yacc: get input \"%s\"\n", $2);
    Command::_currentCommand._inFile = $2;
  }
 |
  GREATAMPERSAND WORD {
    if (Command::_currentCommand._outFile != NULL) yyerror("Ambiguous output redirect\n");
    Command::_currentCommand._outFile = $2;
    Command::_currentCommand._errFile = $2;
  }

 |
  GREATGREAT WORD {
    if (Command::_currentCommand._outFile != NULL) yyerror("Ambiguous output redirect\n");
    Command::_currentCommand._append = 1;
    Command::_currentCommand._outFile = $2;
  }
 |
  GREATGREATAMPERSAND WORD {
    if (Command::_currentCommand._outFile != NULL) yyerror("Ambiguous output redirect\n");
    Command::_currentCommand._append = 1;
    Command::_currentCommand._outFile = $2;
    Command::_currentCommand._errFile = $2;
  } 
;

%%

void
yyerror(const char * s)
{
	fprintf(stderr,"%s", s);
}

#if 0
main()
{
	yyparse();
}
#endif
