
#ifndef command_h
#define command_h

// Command Data Structure
struct SimpleCommand {
  // Available space for arguments currently preallocated
  int _numOfAvailableArguments;
  
  // Number of arguments
  int _numOfArguments;
  char ** _arguments;
  
  SimpleCommand();
  void insertArgument( char * argument );
};

struct Command {
  int _numOfAvailableSimpleCommands;
  int _numOfSimpleCommands;
  SimpleCommand ** _simpleCommands;
  char * _outFile;
  char * _inFile;
  char * _errFile;
  int _background;
  int _append;

  void prompt();
  void print();
  void execute();
  void clear();
  
  Command();
  void insertSimpleCommand(SimpleCommand *simpleCommand );
  char *expandEnvVar(char *arg);
  char *expandTilde(char *arg);
  void expandWildcardsIfNecessary(char *arg);
  void expandWildcards(char *prefix, char *suffix);
  static int compare(const void *a, const void *b);
  
  static Command _currentCommand;
  static SimpleCommand *_currentSimpleCommand;
};

#endif
