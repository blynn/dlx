#define _GNU_SOURCE
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "darray.h"
#include "dlx.h"

#define F(i,n) for(int i = 0; i < n; i++)

#define VAPRINT(s, fmt) \
    va_list params; \
    va_start(params, fmt); \
    char *s; \
    vasprintf(&s, fmt, params); \
    va_end(params);

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

cell_ptr col_new() {
  cell_ptr c = malloc(sizeof(*c));
  c->s = 0;
  c->U = c->D = c;
  return c;
}

struct dlx_s {
  darray_ptr ctab, rtab;
  cell_ptr root;
  char *(*cid)(int);
};
typedef struct dlx_s *dlx_t;

char *default_cid(int n) {
  static char buf[16];
  sprintf(buf, "%d", n);
  return buf;
}

dlx_t dlx_new() {
  dlx_t p = malloc(sizeof(*p));
  p->ctab = darray_new();
  p->rtab = darray_new();
  cell_ptr h = col_new();
  h->L = h->R = h;
  p->root = h;
  p->cid = default_cid;
  return p;
}

void dlx_add_col(dlx_t p) {
  cell_ptr c = col_new();
  c->U = c->D = c;
  cell_ptr h = p->root;
  c->L = h->L; h->L = h->L->R = c, c->R = h;
  c->n = darray_count(p->ctab);
  darray_append(p->ctab, c);
}

void dlx_add_row(dlx_t p) {
  darray_append(p->rtab, 0);
}

void dlx_alloc_col(dlx_t p, int n) {
  while(darray_count(p->ctab) < n) dlx_add_col(p);
}

void dlx_alloc_row(dlx_t p, int n) {
  while(darray_count(p->rtab) < n) dlx_add_row(p);
}

void dlx_mark_optional(dlx_t p, int col) {
  dlx_alloc_col(p, col + 1);
  cell_ptr c = (cell_ptr) darray_at(p->ctab, col);
  c->L->R = c->R;
  c->R->L = c->L;
  c->L = c->R = c;
}

// Called by dlx_set the first time we add a 1 to a row.
static cell_ptr row_first(dlx_t p, cell_ptr c, int row) {
  cell_ptr n = malloc(sizeof(*n));
  n->U = c->U;
  c->U = c->U->D = n;
  n->D = n->c = c;
  n->L = n->R = n;
  c->s++;
  n->n = row;
  return n;
}

void dlx_set(dlx_t p, int row, int col) {
  dlx_alloc_row(p, row + 1);
  dlx_alloc_col(p, col + 1);
  cell_ptr c = (cell_ptr) darray_at(p->ctab, col);
  cell_ptr *rp = (cell_ptr *) p->rtab->item + row;
  if (!*rp) {
    *rp = row_first(p, c, row);
    return;
  }
  cell_ptr r =(*rp)->L;
  if (c->n <= r->c->n) {
    // We enforce this to avoid duplicate columns in a row, and for simplicity,
    // though the algorithm works even if the columns were jumbled.
    die("must add columns in increasing order: %d");
  }
  cell_ptr n = malloc(sizeof(*n));
  n->U = c->U;
  c->U = c->U->D = n;
  n->D = n->c = c;
  n->R = r->R;
  r->R = r->R->L = n; 
  n->L = r;
  c->s++;
  r = n;
}

void dlx_set_last_row(dlx_t p, int col) {
  dlx_set(p, darray_count(p->rtab) - 1, col);
}

void cover_col(cell_ptr c) {
//printf("cover %d\n", cbt_key(c->it));
  c->R->L = c->L;
  c->L->R = c->R;
  for(cell_ptr i = c->D; i != c; i = i->D) {
    for(cell_ptr j = i->R; j != i; j = j->R) {
      j->U->D = j->D;
      j->D->U = j->U;
      j->c->s--;
    }
  }
}

void uncover_col(cell_ptr c) {
//printf("uncover %s\n", cbt_key(c->it));
  for(cell_ptr i = c->U; i != c; i = i->U) {
    for(cell_ptr j = i->L; j != i; j = j->L) {
      j->c->s++;
      j->U->D = j->D->U = j;
    }
  }
  c->R->L = c->L->R = c;
}

void dlx_cover_row(dlx_t p, int i) {
  if (i >= darray_count(p->rtab)) die("%d out of range", i);
  cell_ptr r = p->rtab->item[i];
  if (!r) return;
  cover_col(r->c);
  for(cell_ptr j = r->R; j != r; j = j->R) {
    cover_col(j->c);
  }
}

void dlx_search(dlx_t p) {
  cell_ptr h = p->root;
  darray_ptr sol = darray_new();
  void search(int k) {
    cell_ptr c = h->R;
    if (c == h) {
      // Print solution.
      printf("solution:\n");
      void pr(void *data) {
        cell_ptr r = data;
        while(r->L->c->n < r->c->n) r = r->L;
        printf("(row %d)", r->n);
        cell_ptr first = r;
        do {
          printf(" %s", p->cid(r->c->n));
          r = r->R;
        } while (r != first);
        printf("\n");
      }
      darray_forall(sol, pr);
      return;
    }
    int s = c->s;
    for(cell_ptr cc = c->R; cc != h; cc = cc->R) {
      if (cc->s < s) c = cc, s = c->s;
    }
printf("lvl %d(%d): %s\n", k, s, p->cid(c->n));
    if (!s) return;
    cover_col(c);
    for(cell_ptr r = c->D; r != c; r = r->D) {
      darray_append(sol, r);
      for(cell_ptr j = r->R; j != r; j = j->R) {
        cover_col(j->c);
      }
      search(k+1);
      darray_remove_last(sol);
      for(cell_ptr j = r->L; j != r; j = j->L) {
        uncover_col(j->c);
      }
    }
    uncover_col(c);
  }
  search(0);
}

void dlx_set_col_id(dlx_t p, char *(*cb)(int)) {
  p->cid = cb;
}

int sudoku_main() {
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

  darray_ptr con = darray_new();
  darray_ptr choice = darray_new();

  void constrain(const char *id, ...) {
    VAPRINT(s, id);
    darray_append(con, s);
    dlx_add_col(dlx);
  }

  void new_row(const char *id, ...) {
    VAPRINT(s, id);
    darray_append(choice, s);
    dlx_add_row(dlx);
  }

  void add_1(const char *id, ...) {
    VAPRINT(s, id);
    F(c, darray_count(con)) if (!strcmp(s, con->item[c])) {
      dlx_set_last_row(dlx, c);
      free(s);
      return;
    }
    die("no such contraint %s", s);
  }

  void choose(const char *id, ...) {
    VAPRINT(s, id);
    F(i, darray_count(choice)) if (!strcmp(s, choice->item[i])) {
      dlx_cover_row(dlx, i);
      free(s);
      return;
    }
    die("no such choice %s", s);
  }

  char *constraint_print(int n) { return con->item[n]; }
  dlx->cid = constraint_print;

  // Setup human-friendly names for columns.
  // Exactly one digit per box.
  F(r, 9) F(c, 9) constrain("! %d %d", r, c);
  // Each digit appears exactly once per row.
  F(n, 9) F(r, 9) constrain("%d r %d", n, r);
  // ...per column.
  F(n, 9) F(c, 9) constrain("%d c %d", n, c);
  // ... and per 3x3 region.
  F(n, 9) F(r, 3) F(c, 3) constrain("%d x %d %d", n, r, c);

  // Add rows:
  F(n, 9) F(r, 9) F(c, 9) {
    new_row("%d @ %d %d", n, r, c);
    add_1("! %d %d", r, c);
    add_1("%d r %d", n, r);
    add_1("%d c %d", n, c);
    add_1("%d x %d %d", n, r/3, c/3);
  }

  F(r, 9) F(c, 9) if (grid[r][c]) choose("%d @ %d %d", grid[r][c] - 1, r, c);

  dlx_search(dlx);
  return 0;
}
