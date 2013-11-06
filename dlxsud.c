// Reads a sudoku on standard input and solves it using the DLX library.
// Prints all solutions and also explains each step of the search.
#define _GNU_SOURCE
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "dlx.h"

#define F(i,n) for(int i = 0; i < n; i++)

int main() {
  static int grid[9][9];
  F(i, 9) {
    char *s = 0;
    size_t len = 0;
    if (-1 == getline(&s, &len, stdin)) {
      fprintf(stderr, "input error\n");
      return 1;
    }
    F(j, 9) {
      if (s[j] == '\n') break;
      grid[i][j] = isdigit(s[j]) ? s[j] - '0' : 0;
    }
    free(s);
  }

  dlx_t dlx = dlx_new();

  int nine(int a, int b, int c) { return ((a * 9) + b) * 9 + c; }

  // Add constraints.
  F(n, 9) F(r, 9) F(c, 9) {
    int row = nine(n, r, c);
    dlx_set(dlx, row, nine(0, r, c));
    dlx_set(dlx, row, nine(1, n, r));
    dlx_set(dlx, row, nine(2, n, c));
    dlx_set(dlx, row, nine(3, n, r / 3 * 3 + c / 3));
  }

  // Choose rows corresponding to given digits.
  F(r, 9) F(c, 9) if (grid[r][c]) dlx_pick_row(dlx, nine(grid[r][c] - 1, r, c));

  void pr(int row[], int n) {
    F(i, n) {
      int k = row[i];
      grid[k/9%9][k%9] = 1 + k/9/9;
    }
    F(r, 9) {
      F(c, 9) putchar(grid[r][c] + '0');
      putchar('\n');
    }
  }
  dlx_forall_cover(dlx, pr);

  // Show how the search works, step by step.
  {
    char *spr(const char *fmt, ...) {
      va_list params;
      va_start(params, fmt);
      char *s;
      vasprintf(&s, fmt, params);
      va_end(params);
      return s;
    }

    char *con[4*9*9];  // Description of constraints.
    char **p = con;
    void add_con(char *id) { *p++ = id; }
    // Exactly one digit per box.
    F(r, 9) F(c, 9) add_con(spr("! %d %d", r+1, c+1));
    // Each digit appears exactly once per row.
    F(n, 9) F(r, 9) add_con(spr("%d r %d", n+1, r+1));
    // ...per column.
    F(n, 9) F(c, 9) add_con(spr("%d c %d", n+1, c+1));
    // ... and per 3x3 region.
    F(n, 9) F(r, 3) F(c, 3) add_con(spr("%d x %d %d", n+1, r+1, c+1));

    void cover(int c, int s, int r) {
      if (s == 1) {
        printf("%s forces %d @ %d %d\n", con[c], r/9/9+1, r/9%9+1, r%9+1);
      } else {
        printf("%s: %d choices\n", con[c], s);
        printf("try %d @ %d %d\n", r/9/9+1, r/9%9+1, r%9+1);
      }
    }
    void found() { puts("solved!"); }
    void stuck(int c) { printf("%s: stuck! backtracking...\n", con[c]); }
    dlx_solve(dlx, cover, NULL, found, stuck);
    F(i, 4*9*9) free(con[i]);
  }
  dlx_clear(dlx);
  return 0;
}
