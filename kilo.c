/*** includes ***/
// http://viewsourcecode.org/snaptoken/kilo/index.html
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO,"\x1b[6n",4) != 4) return -1;

  while (i < sizeof(buf) -1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws; // ioctl.h

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    // TIOCGWINSZ didn't work, let's move the cursor to see where it goes:
    if (write(STDOUT_FILENO,"\x1b[999C\x1b[999B",12) != 12) return -1; // C=Cursor Forward, B=Cursor Down
    return getCursorPosition(rows,cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** append buffer ***/
struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL,0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

/*** output ***/

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    abAppend(ab, "~",1);
    abAppend(ab, "\x1b[K",3);
    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n",2);
    }
  }
}

void editorClearScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4); // J=Erase In Display, 2=entire screen
  write(STDOUT_FILENO, "\x1b[H", 3); // H=Cursor Position (default upper left, same as 1;1H
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6); // l=Reset Mode, ?25=display cursor
  abAppend(&ab, "\x1b[H", 3); // H=Cursor Position (default upper left, same as 1;1H

  editorDrawRows(&ab);

  abAppend(&ab, "\x1b[H", 3);
  abAppend(&ab, "\x1b[?25h", 6); // h=Set Mode, ?25=display cursor

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
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
