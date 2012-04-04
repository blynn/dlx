#define _GNU_SOURCE
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define F(i,n) for(int i = 0; i < n; i++)
#define C(i,n,dir) for(cell_ptr i = n->dir; i != n; i = i->dir)

static void die(const char *err, ...) {
  va_list params;
  va_start(params, err);
  vfprintf(stderr, err, params);
  va_end(params);
  fputc('\n', stderr);
  exit(1);
}

struct cell_s;
typedef struct cell_s *cell_ptr;
struct cell_s {
  cell_ptr U, D, L, R, c;
  int s, n;
};

// Dance steps.
cell_ptr UD_remove (cell_ptr j) { return j->U->D = j->D, j->D->U = j->U, j; }
cell_ptr UD_restore(cell_ptr j) { return j->U->D = j->D->U = j; }
cell_ptr UD_self   (cell_ptr j) { return j->U = j->D = j; }
cell_ptr UD_insert(cell_ptr j, cell_ptr k) {
  return j->U = k->U, j->D = k, k->U = k->U->D = j;
}
// Identical except UD is translated to LR.
cell_ptr LR_remove (cell_ptr j) { return j->L->R = j->R, j->R->L = j->L, j; }
cell_ptr LR_restore(cell_ptr j) { return j->L->R = j->R->L = j; }
cell_ptr LR_self   (cell_ptr j) { return j->L = j->R = j; }
cell_ptr LR_insert(cell_ptr j, cell_ptr k) {
  return j->L = k->L, j->R = k, k->L = k->L->R = j;
}

struct dlx_s {
  cell_ptr *rtab, *ctab, root;
  int rmax, cmax, rsz, csz;
  void (*sol_cb)(int);
};
typedef struct dlx_s *dlx_t;

dlx_t dlx_new() {
  dlx_t p = malloc(sizeof(*p));
  p->rsz = p->csz = 0;
  p->rmax = p->cmax = 1024;
  p->ctab = malloc(sizeof(cell_ptr) * p->cmax);
  p->rtab = malloc(sizeof(cell_ptr) * p->rmax);
  p->root = LR_self(malloc(sizeof(*p->root)));
  return p;
}

void dlx_add_col(dlx_t p) {
  cell_ptr c = malloc(sizeof(*c));
  UD_self(c)->s = 0;
  p->ctab[c->n = p->csz++] = LR_insert(c, p->root);
}

void dlx_add_row(dlx_t p) { p->rtab[p->rsz++] = 0; }

void dlx_alloc_col(dlx_t p, int n) {
  if (p->cmax < n) p->ctab = realloc(p->ctab, sizeof(cell_ptr)*(p->cmax = 2*n));
  while(p->csz < n) dlx_add_col(p);
}

void dlx_alloc_row(dlx_t p, int n) {
  if (p->rmax < n) p->rtab = realloc(p->rtab, sizeof(cell_ptr)*(p->rmax = 2*n));
  while (p->rsz < n) dlx_add_row(p);
}

static cell_ptr row_new(cell_ptr c) {
  cell_ptr n = UD_insert(malloc(sizeof(*n)), c);
  (n->c = c)->s++;
  return n;
}

void dlx_set(dlx_t p, int row, int col) {
  dlx_alloc_row(p, row + 1);
  dlx_alloc_col(p, col + 1);
  cell_ptr c = p->ctab[col], r = p->rtab[row];
  if (!r) {
    LR_self(p->rtab[row] = row_new(c))->n = row;
    return;
  }
  // We enforce this to avoid duplicate columns in a row, and for simplicity,
  // though the algorithm works even if the columns were jumbled.
  if (c->n <= r->L->c->n) die("columns must increase: %d");
  LR_insert(row_new(c), r);
}

void dlx_set_last_row(dlx_t p, int col) {
  if (!p->rsz) die("no rows yet");
  dlx_set(p, p->rsz - 1, col);
}

static void cover(cell_ptr c) {
  LR_remove(c);
  C(i, c, D) C(j, i, R) UD_remove(j)->c->s--;
}

static void uncover(cell_ptr c) {
  C(i, c, U) C(j, i, L) UD_restore(j)->c->s++;
  LR_restore(c);
}

void dlx_cover_row(dlx_t p, int i) {
  if (i >= p->rsz) die("%d out of range", i);
  cell_ptr r = p->rtab[i];
  if (!r) return;
  cover(r->c);
  C(j, r, R) cover(j->c);
}

void dlx_search(dlx_t p) {
  cell_ptr sol[9*9], h = p->root;
  void search(int k) {
    cell_ptr c = h->R;
    if (c == h) {  // Solution found.
      F(i, k) {
        cell_ptr r = sol[i];
        while(r->L->c->n < r->c->n) r = r->L;
        p->sol_cb(r->n);
      }
      p->sol_cb(-1);
      return;
    }
    // Choose the first most-constrained column.
    int s = INT_MAX;
    C(i, h, R) if (i->s < s) c = i, s = c->s;
    if (!s) return;
    // Let's dance!
    cover(c);
    C(r, c, D) {
      sol[k] = r;
      C(j, r, R) cover(j->c);
      search(k+1);
      C(j, r, L) uncover(j->c);
    }
    uncover(c);
  }
  search(0);
}

int main() {
  char a[9][9];
  // Read Sudoku. Assumes input is valid.
  memset(a, 0, sizeof(a));
  F(i, 9) {
    char *s = 0;
    size_t len = 0;
    if (-1 == getline(&s, &len, stdin)) die("input error");
    F(j, 9 && s[j] != '\n') a[i][j] = isdigit(s[j]) ? s[j] - '0' : 0;
  }

  dlx_t dlx = dlx_new();
  void print_solution(int r) {
    if (r < 0) {
      F(r, 9) {
        F(c, 9) printf("%d", a[r][c]);
        puts("");
      }
      return;
    }
    a[r/9%9][r%9] = r/9/9 + 1;
  }
  dlx->sol_cb = print_solution;

  F(d, 9) F(r, 9) F(c, 9) {
    dlx_add_row(dlx);
    int cond[4][2] = {  // Constraints.
       {r, c},  // One digit per box.
       {r, d},  // One digit per row.
       {c, d},  // One digit per column.
       {(r/3*3 + c/3), d} };   // One digit per 3x3 region.
    F(i, 4) dlx_set_last_row(dlx, 9*9*i + 9*cond[i][0] + cond[i][1]);
  }
  F(r, 9) F(c, 9) if (a[r][c]) dlx_cover_row(dlx, 9*9*(a[r][c]-1) + 9*r + c);

  dlx_search(dlx);
  return 0;
}
