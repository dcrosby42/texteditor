/*** includes ***/
// http://viewsourcecode.org/snaptoken/kilo/index.html
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)


/*** data ***/

struct editorConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;
};

struct editorConfig E;

/*** predeclares ***/
void editorClearScreen();

/*** terminal ***/
// http://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

void die(const char *s) {
  editorClearScreen();

  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = E.orig_termios; // copy
  raw.c_iflag &= ~(ICRNL | IXON);  // ICRNL makes C-m and Enter 13, and IXON disables C-q and C-s behavior
  raw.c_oflag &= ~(OPOST);  // turn off "\n"-to-"\r\n" translation
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // turn of echo, lineread, disable C-v and C-o, and C-c and C-z
  raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP);  // more misc input flags, obsolete
  raw.c_cflag |= (CS8);  // more misc input flags, obsolete
  // "cc" means "control characters":
  raw.c_cc[VMIN] = 0;  // min num bytes to read before returning
  raw.c_cc[VTIME] = 1; // max block time for read(), in 0.1 second intervals (100ms)

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws; // ioctl.h

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** output ***/

void editorDrawRows() {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    write(STDOUT_FILENO, "~\r\n",3);
  }
}

void editorClearScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4); // J=Erase In Display, 2=entire screen
  write(STDOUT_FILENO, "\x1b[H", 3); // H=Cursor Position (default upper left, same as 1;1H
}
void editorRefreshScreen() {
  editorClearScreen();

  editorDrawRows();

  write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** input ***/

void editorProcessKeypress() {
  char c = editorReadKey();
  switch (c) {
    case CTRL_KEY('q'):
      editorClearScreen();
      exit(0);
      break;
  }
}

/*** init ***/

void initEditor() {
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
