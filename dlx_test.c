#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "dlx.h"

#define F(i,n) for(int i = 0; i < n; i++)

#define EXPECT(X) if (!(X)) die("FAIL: line %d: %s", __LINE__, #X)

static void die(const char *err, ...) {
  va_list params;
  va_start(params, err);
  vfprintf(stderr, err, params);
  fputc('\n', stderr);
  va_end(params);
  exit(1);
}

// From http://school.maths.uwa.edu.au/~gordon/sudokumin.php
static char sudoku17_1[] =
    ".......1."
    "4........"
    ".2......."
    "....5.4.7"
    "..8...3.."
    "..1.9...."
    "3..4..2.."
    ".5.1....."
    "...8.6..."
    "";

static char sudoku17_1_solved[] =
    "693784512"
    "487512936"
    "125963874"
    "932651487"
    "568247391"
    "741398625"
    "319475268"
    "856129743"
    "274836159"
    "";

void parse_sudoku(int grid[9][9], char *s) {
  F(r, 9) F(c, 9) {
    grid[r][c] = isdigit(*s) ? *s - '0' : 0;
    s++;
  }
}

static void test_sudoku() {
  int grid[9][9];
  parse_sudoku(grid, sudoku17_1);
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

  int count = 0;
  void f(int row[], int n) {
    F(i, n) {
      int k = row[i];
      grid[k/9%9][k%9] = 1 + k/9/9;
    }
    count++;
  }
  dlx_forall_cover(dlx, f);
  EXPECT(count == 1);
  int sol[9][9];
  parse_sudoku(sol, sudoku17_1_solved);
  F(r, 9) F(c, 9) EXPECT(grid[r][c] == sol[r][c]);
  dlx_clear(dlx);
}

static void test_sudoku_duplicate_constraints() {
  int grid[9][9];
  parse_sudoku(grid, sudoku17_1);
  dlx_t dlx = dlx_new();

  int nine(int a, int b, int c) { return ((a * 9) + b) * 9 + c; }

  // Add constraints multiple times.
  F(n, 9) F(r, 9) F(c, 9) {
    int row = nine(n, r, c);
    dlx_set(dlx, row, nine(0, r, c));
    dlx_set(dlx, row, nine(1, n, r));
    dlx_set(dlx, row, nine(2, n, c));
    dlx_set(dlx, row, nine(3, n, r / 3 * 3 + c / 3));
  }
  F(n, 9) F(r, 9) F(c, 9) {
    int row = nine(n, r, c);
    dlx_set(dlx, row, nine(0, r, c));
    dlx_set(dlx, row, nine(0, r, c));
    dlx_set(dlx, row, nine(1, n, r));
    dlx_set(dlx, row, nine(2, n, c));
    dlx_set(dlx, row, nine(3, n, r / 3 * 3 + c / 3));
    dlx_set(dlx, row, nine(3, n, r / 3 * 3 + c / 3));
    dlx_set(dlx, row, nine(2, n, c));
    dlx_set(dlx, row, nine(1, n, r));
  }

  F(r, 9) F(c, 9) if (grid[r][c]) dlx_pick_row(dlx, nine(grid[r][c] - 1, r, c));

  int count = 0;
  void f(int row[], int n) {
    F(i, n) {
      int k = row[i];
      grid[k/9%9][k%9] = 1 + k/9/9;
    }
    count++;
  }
  dlx_forall_cover(dlx, f);
  EXPECT(count == 1);
  int sol[9][9];
  parse_sudoku(sol, sudoku17_1_solved);
  F(r, 9) F(c, 9) EXPECT(grid[r][c] == sol[r][c]);
  dlx_clear(dlx);
}

static void test_sudoku_random_order() {
  int grid[9][9];
  parse_sudoku(grid, sudoku17_1);
  dlx_t dlx = dlx_new();

  srandom(time(NULL));
  const int con = 4*9*9*9;
  int shuf[con];
  F(i, con) shuf[i] = i;
  F(i, con) {
    int j = i + random() % (con - i), tmp = shuf[i];
    shuf[i] = shuf[j];
    shuf[j] = tmp;
  }

  int nine(int a, int b, int c) { return ((a * 9) + b) * 9 + c; }

  // Add rows and constraints in random order.
  F(i, con) {
    int m = shuf[i];
    int k = m/9/9/9;
    int n = m/9/9%9;
    int r = m/9%9;
    int c = m%9;
    int row = nine(n, r, c);
    switch (k) {
    case 0:
      dlx_set(dlx, row, nine(0, r, c));
      break;
    case 1:
      dlx_set(dlx, row, nine(1, n, r));
      break;
    case 2:
      dlx_set(dlx, row, nine(2, n, c));
      break;
    case 3:
      dlx_set(dlx, row, nine(3, n, r / 3 * 3 + c / 3));
      break;
    }
  }

  F(r, 9) F(c, 9) if (grid[r][c]) dlx_pick_row(dlx, nine(grid[r][c] - 1, r, c));

  int count = 0;
  void f(int row[], int n) {
    F(i, n) {
      int k = row[i];
      grid[k/9%9][k%9] = 1 + k/9/9;
    }
    count++;
  }
  dlx_forall_cover(dlx, f);
  EXPECT(count == 1);
  int sol[9][9];
  parse_sudoku(sol, sudoku17_1_solved);
  F(r, 9) F(c, 9) EXPECT(grid[r][c] == sol[r][c]);
  dlx_clear(dlx);
}

void test_counter() {
  dlx_t dlx = dlx_new();
  // Set entry (i, 0) for i in [0..9].
  // and entry (i, 1) for i in [10..13].
  F(i, 10) dlx_set(dlx, i, 0);
  F(i, 3) dlx_set(dlx, 10 + i, 1);
  // The exact covers are pairs of rows r0, r1 where r0 lies in [10..13] and r1
  // in [0..9].
  // DLX tries the most constrained column first, and then each row in the
  // order they were added, so we check r1 + 10*(r0 - 10) goes from 0 to 29.
  int counter = 0;
  void f(int r[], int n) {
    EXPECT(n == 2);
    EXPECT(r[1] + 10*(r[0] - 10) == counter++);
  }
  dlx_forall_cover(dlx, f);
  EXPECT(30 == counter);
  dlx_clear(dlx);
}

void test_perm() {
  dlx_t dlx = dlx_new();
  F(i, 4) F(j, 4) {
    // Consder a string of length 4 and the characters {a, b, c, d}.
    // Row 4*i + j represents putting the jth character the ith position
    // of the string. We set column i so that exactly one character will go
    // into each position, and we set column 4 + j so that each character is
    // used exactly once. Thus the exact covers map to all the permutations of
    // "abcd".
    dlx_set(dlx, 4*i + j, i);
    dlx_set(dlx, 4*i + j, 4 + j);
  }
  int counter = 0;
  void f(int r[], int n) {
    char s[5];
    s[4] = 0;
    F(i, n) s[r[i]/4] = r[i]%4 + 'a';
    // Check that the index of s in the list of all sorted permutations
    // matches the counter.
    int present[4] = {1,1,1,1}, index = 0;
    F(i, 4) {
      index *= 4-i;
      int k = s[i] - 'a';
      present[k] = 0;
      while(--k >= 0) index += present[k];
    }
    EXPECT(index == counter++);
  }
  dlx_forall_cover(dlx, f);
  EXPECT(4*3*2*1 == counter);
  dlx_clear(dlx);
}

int main() {
  test_sudoku();
  test_sudoku_duplicate_constraints();
  test_sudoku_random_order();
  test_counter();
  test_perm();
  return 0;
}
