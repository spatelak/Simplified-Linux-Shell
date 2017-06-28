/*
 * CS354: Operating Systems. 
 * Purdue University
 * Example that shows how to read one line with simple editing
 * using raw terminal.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#define MAX_BUFFER_LINE 2048

// Buffer where line is stored
int line_length;
char line_buffer[MAX_BUFFER_LINE];

// position of cursor on the line
int cursor;

// Array storing the previous commands
int history_length = 0;
int history_index  = 0;
char **history;

// Prototypes
void backspace(char ch);
void leftArrow(char ch);
void rightArrow(char ch);
void eraseLine(char ch);

// Print the usage/options
void read_line_print_usage()
{
  char * usage = "\n"
    " ctrl-?              Print usage\n"
    " left arrow key      Moves cursor to the left and allows insertion\n"
    " right arrow key     Moves cursor to the right and allows insertion\n"
    " delete              Deletes character at the cursor\n"
    " backspace(ctrl-H)   Deletes character before the cursor\n"
    " home key(ctrl-A)    Moves cursor to the beginning of the line\n"
    " end key(ctrl-E)     Moves cursor to the end of the line\n"
    " up arrow            See the next command in the history\n"
    " down arrow          See the previous command in the history\n";

  write(1, usage, strlen(usage));
}

/* 
 * Input a line with some basic editing.
 */
char *read_line() {

  // To reset tty mode before exiting
  struct termios terminal_attr;
  tcgetattr(0,&terminal_attr);
  
  // Set terminal in raw mode
  tty_raw_mode();

  line_length = 0;
  cursor      = 0;
  
  // Read one line until enter is typed
  while (1) {
    // Read one character in raw mode.
    char ch;
    read(0, &ch, 1);

    // It is a printable character 
    if (ch >= 32 && ch != 127) {
      // Print the char and shift the chars after cursor to the right
      char backspace = 8;
      int  oldCursor = cursor;
      while (cursor <= line_length) {
	char tempCh = line_buffer[cursor];
	write(1, &ch ,1);
	line_buffer[cursor] = ch;
	ch = tempCh;
        cursor++;
      }
      
      // Bring back the cursor to the appropriate position
      while (cursor > oldCursor + 1) {
	write(1, &backspace ,1);
	cursor--;
      }
      
      // If max number of character reached return.
      if (line_length == MAX_BUFFER_LINE-2) break; 

      // Add char to buffer
      line_length++;
    }

    // <ctrl-A> was typed. Move cursor to the beggining of line
    else if (ch == 1) {
      while (cursor > 0) {
	leftArrow(ch);
      }
    }

    // <ctrl-E> was typed. Move cursor to the end of the line
    else if (ch == 5) {
      while (cursor < line_length) {
	rightArrow(ch);
      }
      
      ch = ' ';
      write(1, &ch ,1);
      ch = 8;
      write(1, &ch ,1);
    }
    
    // <Enter> was typed. Return line
    else if (ch == 10) {
      // Add the command to the history table
      if (line_length > 0) {
	// Allocate the memory for the new command and update the history table
	history                 = (char **) realloc(history, (history_length + 1) * sizeof(char *));
	history[history_length] = (char *)  malloc(line_length * sizeof(char *));
	strncpy(history[history_length], line_buffer, line_length);
	history[history_length][line_length] = '\0';
	history_length++;
	history_index = history_length;
      }
      
      // Print newline
      write(1, &ch ,1);
      break;
    }

    // ctrl-?
    else if (ch == 31) {
      read_line_print_usage();
      line_buffer[0] = 0;
      break;
    }

    // <backspace> was typed. Remove character before the cursor
    else if (ch == 8) {
      if (cursor > 0 && line_length > 0)
	backspace(ch);
    }

    // <delete> was type. Remove the character at the cursor
    else if (ch == 127) {
      if (cursor < line_length) {
	rightArrow(ch);
	ch = 8;
	backspace(ch);
      }
    }

    // <arrow> was used
    else if (ch == 27) {
      // Escape sequence. Read two chars more
      char ch1; 
      char ch2;
      read(0, &ch1, 1);
      read(0, &ch2, 1);

      // <up arrow> was used. Give the previous commmand from the history
      if (ch1 == 91 && ch2 == 65) {	
	if (history_index > 0) {
	  // Erase old line
	  eraseLine(ch);

	  // Copy line from history
	  strcpy(line_buffer, history[--history_index]);
	  line_length = strlen(line_buffer);
	  cursor = line_length;
	  
	  // echo line
	  write(1, line_buffer, line_length);
	}
      }

      // <down arrow> was used. Give the next command from the history
      else if (ch1 == 91 && ch2 == 66) {
	if (history_index < history_length - 1) {
	  // Erase old line
	  eraseLine(ch);
	  
	  // Copy line from history
	  strcpy(line_buffer, history[++history_index]);
	  line_length = strlen(line_buffer);
	  cursor = line_length;
	  
	  // echo line
	  write(1, line_buffer, line_length);
	}

	else if (history_index == history_length - 1) {
	  // Erase old line
	  eraseLine(ch);

	  // Clear the line buffer
	  int i;
	  for (i = 0; i < line_length; i++) line_buffer[i] = 0;
	  line_length = 0;
	  cursor = 0;

	  // Adjust index
	  history_index++;
	}
      }
      
      // <right arrow> was used. Move cursor to the right
      else if (ch1 == 91 && ch2 == 67) {
	if (cursor < line_length)
	  rightArrow(ch);
      }

      // <left arrow> was used. Move cursor to the left
      else if (ch1 == 91 && ch2 == 68) {
	if (cursor > 0)
	  leftArrow(ch);
      }      
    }
  }

  // Add eol and null char at the end of string
  line_buffer[line_length++] = 10;
  line_buffer[line_length]   = 0;

  tcsetattr(0,TCSANOW, &terminal_attr);
  return line_buffer;
}

// Removes the previous character
void backspace(char ch) {
  // Shift the characters in the array to the left
  int i;
  for (i = cursor; i < line_length; i++) line_buffer[i-1] = line_buffer[i];
  
  // Do echo
  ch = 8;
  write(1, &ch ,1);
  for (i = cursor - 1; i < line_length - 1; i++) {
    ch = line_buffer[i];
    write(1, &ch ,1);
  }
  
  // Bring back the cursor to the appropriate position
  ch = ' ';
  write(1, &ch ,1);
  i = cursor - 1;
  while (i < line_length) {
    ch = 8;
    write(1, &ch ,1);
    i++;
  }

  // Remove one character from buffer
  line_length--;
  cursor--;
}

// Moves the cursor to the right
void rightArrow(char ch) {
  ch = line_buffer[cursor];
  write(1, &ch ,1);
  cursor++;
}

// Moves the cursor to the left
void leftArrow(char ch) {
  ch = 8;
  write(1, &ch ,1);
  cursor--;
}

// Earses the current command on command-line
void eraseLine(char ch) {
  // Print backspaces
  int i = 0;
  for (i = 0; i < line_length; i++) {
    ch = 8;
    write(1, &ch ,1);
  }

  // Print spaces on top
  for (i = 0; i < line_length; i++) {
    ch = ' ';
    write(1, &ch ,1);
  }

  // Print backspaces
  for (i = 0; i < line_length; i++) {
    ch = 8;
    write(1, &ch ,1);
  }
  
}
