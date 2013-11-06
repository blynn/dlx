#define _GNU_SOURCE
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dlx.h"

#define F(i,n) for(int i = 0; i < n; i++)

#define C(i,n,dir) for(cell_ptr i = n->dir; i != n; i = i->dir)

static void die(const char *err, ...) {
  va_list params;

  va_start(params, err);
  vfprintf(stderr, err, params);
  fputc('\n', stderr);
  va_end(params);
  exit(1);
}

struct cell_s;
typedef struct cell_s *cell_ptr;
struct cell_s {
  cell_ptr U, D, L, R;
  int n;
  union {
    cell_ptr c;
    int s;
  };
};

// Some link dance moves.
static cell_ptr LR_self(cell_ptr c) { return c->L = c->R = c; }
static cell_ptr UD_self(cell_ptr c) { return c->U = c->D = c; }

// Undeletable deletes.
static cell_ptr LR_delete(cell_ptr c) {
  return c->L->R = c->R, c->R->L = c->L, c;
}
static cell_ptr UD_delete(cell_ptr c) {
  return c->U->D = c->D, c->D->U = c->U, c;
}

// Undelete.
static cell_ptr UD_restore(cell_ptr c) { return c->U->D = c->D->U = c; }
static cell_ptr LR_restore(cell_ptr c) { return c->L->R = c->R->L = c; }

// Insert cell j to the left of cell k.
static cell_ptr LR_insert(cell_ptr j, cell_ptr k) {
  return j->L = k->L, j->R = k, k->L = k->L->R = j;
}

// Insert cell j above cell k.
static cell_ptr UD_insert (cell_ptr j, cell_ptr k) {
  return j->U = k->U, j->D = k, k->U = k->U->D = j;
}

cell_ptr col_new() {
  cell_ptr c = malloc(sizeof(*c));
  UD_self(c)->s = 0;
  return c;
}

struct dlx_s {
  int ctabn, rtabn, ctab_alloc, rtab_alloc;
  cell_ptr *ctab, *rtab;
  cell_ptr root;
};
typedef struct dlx_s *dlx_t;

dlx_t dlx_new() {
  dlx_t p = malloc(sizeof(*p));
  p->ctabn = p->rtabn = 0;
  p->ctab_alloc = p->rtab_alloc = 8;
  p->ctab = malloc(sizeof(cell_ptr) * p->ctab_alloc);
  p->rtab = malloc(sizeof(cell_ptr) * p->rtab_alloc);
  p->root = LR_self(col_new());
  return p;
}

void dlx_add_col(dlx_t p) {
  cell_ptr c = col_new();
  LR_insert(c, p->root);
  c->n = p->ctabn++;
  if (p->ctabn == p->ctab_alloc) {
    p->ctab = realloc(p->ctab, sizeof(cell_ptr) * (p->ctab_alloc *= 2));
  }
  p->ctab[c->n] = c;
}

void dlx_add_row(dlx_t p) {
  if (++p->rtabn == p->rtab_alloc) {
    p->rtab = realloc(p->rtab, sizeof(cell_ptr) * (p->rtab_alloc *= 2));
  }
  p->rtab[p->rtabn] = 0;
}

static void alloc_col(dlx_t p, int n) {
  while(p->ctabn <= n) dlx_add_col(p);
}

static void alloc_row(dlx_t p, int n) {
  while(p->rtabn <= n) dlx_add_row(p);
}

void dlx_mark_optional(dlx_t p, int col) {
  alloc_col(p, col);
  cell_ptr c = p->ctab[col];
  // Prevent undeletion by self-linking.
  LR_self(LR_delete(c));
}

void dlx_set(dlx_t p, int row, int col) {
  alloc_row(p, row);
  alloc_col(p, col);
  cell_ptr c = p->ctab[col];
  cell_ptr *rp = p->rtab + row;
  if (!*rp) {
    cell_ptr n = malloc(sizeof(*n));
    LR_self(UD_insert(n, c));
    n->n = row;
    n->c = c;
    c->s++;
    *rp = n;
    return;
  }
  cell_ptr r =(*rp)->L;
  if (c->n <= r->c->n) {
    // We enforce this to avoid duplicate columns in a row, and for simplicity,
    // though the algorithm works even if the columns were jumbled.
    // TODO: Remove this restriction.
    die("must add columns in increasing order: %d");
  }
  cell_ptr n = malloc(sizeof(*n));
  UD_insert(n, c);
  LR_insert(n, r);
  n->c = c;
  c->s++;
  n->n = row;
}

static void cover_col(cell_ptr c) {
  LR_delete(c);
  C(i, c, D) C(j, i, R) UD_delete(j)->c->s--;
}

static void uncover_col(cell_ptr c) {
  C(i, c, U) C(j, i, L) UD_restore(j)->c->s++;
  LR_restore(c);
}

void dlx_pick_row(dlx_t p, int i) {
  if (i >= p->rtabn) die("%d out of range", i);
  cell_ptr r = p->rtab[i];
  if (!r) return;
  cover_col(r->c);
  C(j, r, R) cover_col(j->c);
}

void dlx_solve(dlx_t p,
               void (*try_cb)(int, int, int),
               void (*undo_cb)(void),
               void (*found_cb)(),
               void (*stuck_cb)()) {
  void recurse() {
    cell_ptr c = p->root->R;
    if (c == p->root) {
      if (found_cb) found_cb();
      return;
    }
    int s = INT_MAX;  // S-heuristic: choose first most-constrained column.
    C(i, p->root, R) if (i->s < s) s = (c = i)->s;
    if (!s) {
      if (stuck_cb) stuck_cb();
      return;
    }
    cover_col(c);
    C(r, c, D) {
      if (try_cb) try_cb(c->n, s, r->n);
      C(j, r, R) cover_col(j->c);
      recurse();
      if (undo_cb) undo_cb();
      C(j, r, L) uncover_col(j->c);
    }
    uncover_col(c);
  }
  recurse();
}

void dlx_forall_cover(dlx_t p, void (*cb)(int[], int)) {
  int sol[p->rtabn], soln = 0;
  void cover(int c, int s, int r) { sol[soln++] = r; }
  void uncover() { soln--; }
  void found() { cb(sol, soln); }
  dlx_solve(p, cover, uncover, found, NULL);
}

int dlx_rows(dlx_t dlx) { return dlx->rtabn; }
int dlx_cols(dlx_t dlx) { return dlx->ctabn; }

int main() {
  static int grid[9][9];
  F(i, 9) {
    char *s = 0;
    size_t len = 0;
    if (-1 == getline(&s, &len, stdin)) die("input error");
    F(j, 9) {
      if (s[j] == '\n') break;
      grid[i][j] = isdigit(s[j]) ? s[j] - '0' : 0;
    }
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
    F(i, n) grid[row[i] / 9 % 9][row[i] % 9] = 1 + row[i] / 9 / 9;
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

    char *con[729];  // Description of constraints.
    int ncon = 0;
    void add_con(char *id) {
      con[ncon++] = id;
    }
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
    void found() {
      puts("solved!");
    }
    void stuck() {
      puts("stuck! backtracking...");
    }
    dlx_solve(dlx, cover, NULL, found, stuck);
  }
  return 0;
}
