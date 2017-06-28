/*
1;95;0c * CS252: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <regex.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>

#include "command.h"

#define MAXFILENAME 1024

char **array;

// variables to support env var expansion
char *path;
int loading;
int pid;
int exitStatus;
extern int history_length;
extern char **history;

SimpleCommand::SimpleCommand()
{
  // Create available space for 5 arguments
  _numOfAvailableArguments = 5;
  _numOfArguments = 0;
  _arguments = (char **) malloc( _numOfAvailableArguments * sizeof( char * ) );
}

void
SimpleCommand::insertArgument(char *argument)
{
  if ( _numOfAvailableArguments == _numOfArguments  + 1 ) {
    // Double the available space
    _numOfAvailableArguments *= 2;
    _arguments = (char **) realloc( _arguments, _numOfAvailableArguments *sizeof(char *));
  }

  _arguments[ _numOfArguments ] = argument;
  
  // Add NULL argument at the end
  _arguments[ _numOfArguments + 1] = NULL;
  
  _numOfArguments++;
}

char*
Command::expandEnvVar(char *arg) {
  regmatch_t match;
  regex_t re;
  const char *reg = "^.*${[^}][^}]*}.*$";
  int expbuf = regcomp(&re, reg, 0);

  if (expbuf != 0) {
    perror("regcomp");
    return arg;
  }

  if(regexec(&re, arg, 1, &match, 0) == 0) {
    char *newArg = (char *) calloc(1, MAXFILENAME * sizeof(char *));
    int i = 0, j = 0;
    while(arg[i]!='\0' && i < MAXFILENAME) {
      if (arg[i] != '$') {
	newArg[j++] = arg[i++];
      }
      else {
	char *open  = strchr((char *)(arg + i), '{');
	char *close = strchr((char *)(arg + i), '}');
	char *var   = strndup(open + 1, close - open);
	var[strlen(var) - 1] = '\0';

	// check the type of the enviroment variable and set the value accordingly 
	char *value = (char *) calloc(1, MAXFILENAME * sizeof(char *));	
	if      (strcmp(var, "$") == 0)     { sprintf(value, "%d", getpid());      }
	else if (strcmp(var, "?") == 0)     { sprintf(value, "%d", exitStatus);    }
	else if (strcmp(var, "!") == 0)     { sprintf(value, "%d", pid);           }
	else if (strcmp(var, "_") == 0)     { value = history[history_length - 2]; }
	else if (strcmp(var, "SHELL") == 0) { value = path;                        }
	else                                { value = getenv(var);                 }

	// concat the value of variable
	if (value != NULL) {
	  strcat(newArg, value);
	  i += strlen(var) + 3; // go from '$' to char after '}'
	}
	j += strlen(value);
	
	free(var);
      }
    }
    arg = strdup(newArg);
    free(newArg);
  }
  
  return arg;
}

char*
Command::expandTilde(char *arg) {
  if (arg[0] != '~') return arg;

  char *newArg = (char *) malloc((strlen(arg) + 8)*sizeof(char *));
  if (strcmp(arg, "~") == 0 || strcmp(arg, "~/") == 0) {
    strcpy(newArg, getpwnam(getenv("USER"))->pw_dir); 
    strcat(newArg, arg + 1);
  }
  
  else if (strstr(arg, "~/") != NULL) {
    strcpy(newArg, getenv("HOME"));
    strcat(newArg, arg+1);
  }
  else  {
    strcpy(newArg, "/homes/");
    strcat(newArg, arg+1);
  }

  return newArg;
}

void
Command::expandWildcardsIfNecessary(char *arg) {
  if (strchr(arg, '*') == NULL && strchr(arg, '?') == NULL) {
    Command::_currentSimpleCommand->insertArgument(arg);
    return;
  }
  
  char prefix[1];
  expandWildcards(prefix, arg);  
  free(array);
}

void
Command::expandWildcards(char *prefix, char *suffix) {
  // insert the expanded argument when suffix is empty
  if (suffix[0] == 0) return;

  // Obtain the next component in the suffix
  char *s = strchr(suffix, '/');;
  char component[MAXFILENAME];

  // get the next component from the suffix
  if (s != NULL) {
    if (s - suffix == 0) component[0] = '\0';
    else strncpy(component, suffix, s-suffix);
    suffix = s + 1;
  }
  
  else {
    strcpy(component, suffix);
    suffix += strlen(suffix);
  }

  // expand the component if necessary
  char newPrefix[MAXFILENAME];
  if (strchr(component, '*') == NULL && strchr(component, '?') == NULL) {
    if (strlen(prefix) == 0 && strcmp(component, ".") == 0) sprintf(newPrefix, "%s", component);
    else if (strcmp(prefix, "/") == 0)                      sprintf(newPrefix, "/%s", component);
    else                                                    sprintf(newPrefix, "%s/%s", prefix, component);    
    expandWildcards(newPrefix, suffix);
    return;
  }

  
  // convert wildcard into regex
  char *reg = (char *) malloc(2*strlen(component) + 10);
  char *a   = component;
  char *r   = reg;
  *r = '^';
  r++;

  while (*a) {
    if (*a == '*')      { *r = '.'; r++; *r = '*'; r++; }
    else if (*a == '?') { *r = '.'; r++; }
    else if (*a == '.') { *r = '\\'; r++; *r = '.'; r++; }
    else                { *r = *a; r++; }
    a++;
  }

  *r = '$';
  r++;
  *r = 0;

  // compile regex
  regex_t re;
  int expbuf = regcomp(&re, reg, REG_EXTENDED|REG_NOSUB);
  if (expbuf != 0) {
    perror("regcomp");
    return;
  }

  // list directories and add as arguments the entries that match he regular expression
  char *directory;
  if (prefix[0] == 0) {
    directory    = (char *) malloc(sizeof(char *)*2);
    directory[0] = '.';
    directory[1] = '\0';
  }

  else {
    directory = strdup(prefix);
  }
  
  DIR *dir = opendir(directory);
  if (dir == NULL) {
    if (errno != ENOTDIR) perror("opendir");
    return;
  }

  struct dirent *ent;
  int maxEntries = 20;
  int nEntries   = 0;
  array = (char **) malloc(maxEntries * sizeof(char *));
  
  while ((ent = readdir(dir)) != NULL) {
    regmatch_t match;
    if (regexec(&re, ent->d_name, 1, &match, 0) == 0) {
      if (strlen(prefix) == 0)           sprintf(newPrefix, "%s", ent->d_name);
      else if (strcmp(prefix, "/") == 0) sprintf(newPrefix, "/%s", ent->d_name);
      else                               sprintf(newPrefix, "%s/%s", prefix, ent->d_name);
      expandWildcards(newPrefix, suffix);
      
      if (nEntries == maxEntries) {
	maxEntries *= 2;
	array = (char **) realloc(array, maxEntries*sizeof(char *));
      }
      if (ent->d_name[0] == '.') {
	if (component[0] == '.') {
	  array[nEntries++] = strdup(ent->d_name);
	}
      }
      else if (strlen(suffix) == 0) array[nEntries++] = strdup(newPrefix);
    }
  }
  free(reg);
  closedir(dir);

  // sort the directories in the array 
  qsort((void *) &array[0], nEntries, sizeof (char *), compare);

  // insert the directories in the array as arguments
  for (int i = 0; i < nEntries; i++) {
    Command::_currentSimpleCommand->insertArgument(array[i]);
  }
}

// Compare funtion for sorting an array
int
Command::compare(const void *a, const void *b )
{
  const char **s1 = (const char **)a;
  const char **s2 = (const char **)b;

  return strcmp(*s1, *s2);
}

Command::Command()
{
  // Create available space for one simple command
  _numOfAvailableSimpleCommands = 1;
  _simpleCommands = (SimpleCommand **) malloc( _numOfSimpleCommands *sizeof(SimpleCommand *));
	
  _numOfSimpleCommands = 0;
  _outFile = 0;
  _inFile = 0;
  _errFile = 0;
  _background = 0;
}

void
Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
  if ( _numOfAvailableSimpleCommands == _numOfSimpleCommands ) {
    _numOfAvailableSimpleCommands *= 2;
    _simpleCommands = (SimpleCommand **) realloc( _simpleCommands, _numOfAvailableSimpleCommands *sizeof(SimpleCommand *));
  }

   _simpleCommands[_numOfSimpleCommands] = simpleCommand;
   _numOfSimpleCommands++;
}

void
Command:: clear()
{
  for ( int i = 0; i < _numOfSimpleCommands; i++ ) {
    for ( int j = 0; j < _simpleCommands[ i ]->_numOfArguments; j ++ ) {
      free ( _simpleCommands[ i ]->_arguments[ j ] );
    }
		
    free ( _simpleCommands[ i ]->_arguments );
    free ( _simpleCommands[ i ] );
  }
  
  if ( _outFile ) {
    free( _outFile );
  }
	
  if ( _inFile ) {
    free( _inFile );
  }
  
  if ( _errFile && !_outFile ) {
    free( _errFile );
  }
  
  _numOfSimpleCommands = 0;
  _outFile = 0;
  _inFile = 0;
  _errFile = 0;
  _background = 0;
}

void
Command::print()
{
  printf("\n\n");
  printf("              COMMAND TABLE                \n");
  printf("\n");
  printf("  #   Simple Commands\n");
  printf("  --- ----------------------------------------------------------\n");
  
  for ( int i = 0; i < _numOfSimpleCommands; i++ ) {
    printf("  %-3d ", i );
    for ( int j = 0; j < _simpleCommands[i]->_numOfArguments; j++ ) {
      printf("\"%s\" \t", _simpleCommands[i]->_arguments[ j ] );
    }
  }
  
  printf( "\n\n" );
  printf( "  Output       Input        Error        Background\n" );
  printf( "  ------------ ------------ ------------ ------------\n" );
  printf( "  %-12s %-12s %-12s %-12s\n", _outFile?_outFile:"default",
	  _inFile?_inFile:"default", _errFile?_errFile:"default",
	  _background?"YES":"NO");
  printf( "\n\n" );
  
}

void
Command::execute()
{
  // Don't do anything if there are no simple commands
  if ( _numOfSimpleCommands == 0 ) {
    prompt();
    return;
  }

  // exit command: end the shell
  if (strcmp(_simpleCommands[0]->_arguments[0], "exit") == 0) {
    if (isatty(0)) printf("\n Good bye!!\n\n");
    exit(0);
  }

  // Print contents of Command data structure
  //print()
  
  else{
    // Execution
    int defaultin  = dup(0);
    int defaultout = dup(1);
    int defaulterr = dup(2);
    int ret, fdin, fdout, fderr;
        
    // set the initial input
    if (_inFile) {
      fdin = open(_inFile, O_RDONLY);
    }
    else fdin = dup(defaultin);
  
    for (int i = 0; i < _numOfSimpleCommands; i++) {
      dup2(fdin, 0);
      close(fdin);
      
      // i/o redirection for last command
      if (i == _numOfSimpleCommands - 1) {
	if (_errFile) {
	  if(_append != 1) fderr = open(_errFile, O_CREAT | O_WRONLY | O_TRUNC, 0664);
	  else             fderr = open(_errFile, O_CREAT | O_WRONLY | O_APPEND, 0664);
	  dup2(fderr, 2);
	  close(fderr);
	}
      
	if (_outFile) {
	  if (_append != 1) fdout = open(_outFile, O_CREAT | O_WRONLY | O_TRUNC, 0664);
	  else              fdout = open(_outFile, O_CREAT | O_WRONLY | O_APPEND, 0664);
	  dup2(fdout, 1);
	}
	else fdout = dup(defaultout);
	
      }
      
      // redirect output
      else {
	int fdpipe[2];
	pipe(fdpipe);
	fdin  = fdpipe[0];
	fdout = fdpipe[1];     
      }
      
      dup2(fdout, 1);
      close(fdout);

      // print the environment variables
      if (strcmp(_simpleCommands[i]->_arguments[0], "printenv") == 0) {
	extern char **environ;
	int j = 0;
	while (environ[j] != NULL) {
	  printf("%s\n", environ[j++]);
	}
	fflush(stdout);
      }

      // set an environment variable
      else if (strcmp(_simpleCommands[i]->_arguments[0], "setenv") == 0) {
	ret = setenv(_simpleCommands[i]->_arguments[1], _simpleCommands[i]->_arguments[2], 1);
	if (ret < 0) perror("setenv");
      }

      // unset an environmwnt variable
      else if (strcmp(_simpleCommands[i]->_arguments[0], "unsetenv") == 0) {
	ret = unsetenv(_simpleCommands[i]->_arguments[1]);
	if (ret < 0) perror("unsetenv");
      }

      // change the directory path
      else if (strcmp(_simpleCommands[i]->_arguments[0], "cd") == 0) {
	if (_simpleCommands[i]->_arguments[1] == NULL) chdir(getenv("HOME"));
	else if (chdir(_simpleCommands[i]->_arguments[1]) == -1) perror("chdir");
      }
      
     else {
       // create child process
       ret = fork();
       if (ret == 0) {
	 signal(SIGINT, SIG_DFL);
	 execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
	 perror("execvp failed");
	 _exit(1);
       }
	
       else if (ret < 0) {
	 perror("fork failed");
	 return;
       }
     }
    }
  
    // restore stdin and stdout
    dup2(defaultin, 0);
    dup2(defaultout, 1);
    dup2(defaulterr, 2);
    
    // close the descriptors that are no longer needed
    close(defaultin);
    close(defaultout);
    close(defaulterr);
  
    // parent process
    if (_background == 0)  {
      int status;
      waitpid(ret, &status, 0);
      exitStatus = WEXITSTATUS(status);
    }
    else pid = ret;
  }
  
  // Clear to prepare for next command
  clear();
  
  // Print new prompt
  prompt();
}

extern "C" void
zombieHandler(int sig)
{
  while(waitpid(-1, NULL, WNOHANG) > 0);
}

extern "C" void
ctrlCHandler(int sig)
{
    printf("\n");
    Command::_currentCommand.prompt();
    Command::_currentCommand.clear();
}

// Shell implementation
void
Command::prompt()
{
  if (isatty(0) && loading == 0) printf("myshell> ");
  fflush(stdout);
}

Command Command::_currentCommand;
SimpleCommand * Command::_currentSimpleCommand;

int yyparse(void);

main(int argc, char **argv)
{
  path = argv[0];
  signal(SIGINT, ctrlCHandler);
  signal(SIGCHLD, zombieHandler);
  Command::_currentCommand.prompt();
  yyparse();
}

