// Sudoku solver.
//
// Input: nonzero digits represent themselves, and '0' or '.' represents an
// unknown digit. All other characters are ignored. Examples:
//
// .......1.
// 4........
// .2.......
// ....5.4.7
// ..8...3..
// ..1.9....
// 3..4..2..
// .5.1.....
// ...8.6...
//
//  . . . | . . . | . 1 2  
//  . . . | . . . | . . 3  
//  . . 2 | 3 . . | 4 . .  
// -------+-------+------ 
//  . . 1 | 8 . . | . . 5  
//  . 6 . | . 7 . | 8 . .  
//  . . . | . . 9 | . . .  
// -------+-------+------ 
//  . . 8 | 5 . . | . . .  
//  9 . . | . 4 . | 5 . .  
//  4 7 . | . . 6 | . . .  
//
// Shows step-by-step reasoning when run with -v option.
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dlx.h"

#define F(i,n) for(int i = 0; i < n; i++)
#define C(i,n,dir) for(cell_t i = n->dir; i != n; i = i->dir)

int main(int argc, char *argv[]) {
  int verbose = 0, opt;
  while ((opt = getopt(argc, argv, "v")) != -1) {
    if (opt == 'v') verbose++; else {
      fprintf(stderr, "Usage: %s [-v]\n", *argv);
      exit(1);
    }
  }
  int a[9][9] = {{0}}, c;
  F(i, 9) F(j, 9) do if (EOF == (c = getchar())) exit(1); while(
      isdigit(c) ? a[i][j] = c - '0', 0 : c != '.');

  dlx_t dlx = dlx_new();
  int nine(int a, int b, int c) { return 9*9*a + 9*b + c; }
  F(d, 9) F(r, 9) F(c, 9) {
    int i = 0;
    void con(int x, int y) { dlx_set(dlx, nine(d, r, c), nine(i++, x, y)); }
    con(r, c);            // One digit per cell.
    con(r, d);            // One digit per row.
    con(c, d);            // One digit per column.
    con(r/3*3 + c/3, d);  // One digit per 3x3 region.
  }
  // Fill in the given digits.
  F(r, 9) F(c, 9) if (a[r][c]) dlx_pick_row(dlx, nine(a[r][c]-1, r, c));

  // Print all solutions.
  void print_solution(int row[], int n) {
    F(i, n) a[row[i]/9%9][row[i]%9] = row[i]/9/9 + 1;
    F(r, 9) F(c, 9 || (putchar('\n'), 0)) putchar('0'+a[r][c]);
  }
  dlx_forall_cover(dlx, print_solution);
  if (verbose) {
    // Print reasoning.
    int kid[9*9], n = 0, tried[9*9] = { 0 }, indent = 0;
    void tabs() { F(i, indent) fputs("  ", stdout); }
    void con(int c) {
      int k = c%(9*9);
      switch(c/9/9) {
        case 0: printf("! %d %d", k/9+1, k%9+1); break;
        case 1: printf("%d r %d", k/9+1, k%9+1); break;
        case 2: printf("%d c %d", k/9+1, k%9+1); break;
        case 3: printf("%d x %d %d", k/9+1, k%9/3+1, k%9%3+1); break;
      }
    }
    void cover(int c, int s, int r) {
      if (!tried[n]) {
        kid[n] = s;
        indent += s > 1;
      }
      tabs(), con(c);
      if (s == 1) printf(" =>"); else printf(" guess [%d/%d]:", tried[n]+1, s);
      printf(" %d @ %d %d\n", r/9/9+1, r/9%9+1, r%9+1);
      n++;
    }
    void uncover() {
      n--;
      tried[n]++;
      if (tried[n] == kid[n]) {
        indent -= kid[n] > 1;
        tried[n] = 0;
      }
    }
    void found() { tabs(), puts("solved!"); }
    void stuck(int c) { tabs(), con(c), puts(" => stuck! backtracking..."); }
    dlx_solve(dlx, cover, uncover, found, stuck);
  }
  dlx_clear(dlx);
  return 0;
}
