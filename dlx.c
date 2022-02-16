// See http://en.wikipedia.org/wiki/Dancing_Links.
#include <limits.h>
#include <stdlib.h>
#include "dlx.h"

#define F(i,n) for(int i = 0; i < n; i++)

#define C(i,n,dir) for(cell_ptr i = (n)->dir; i != n; i = i->dir)

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

void free_row(cell_ptr r) {
  cell_ptr next;
  for(cell_ptr j = r->R; j != r; j = next) {
    next = j->R;
    free(j);
  }
  free(r);
}

void dlx_clear(dlx_t p) {
  // Elements in the LR list for each row are never covered, thus all cells
  // can be accessed from the 'rtab' LR lists.
  F(i, p->rtabn) {
    cell_ptr r = p->rtab[i];
    if (r) free_row(r);
  }
  // Columns may be covered, but they are always accessible from 'ctab'.
  F(i, p->ctabn) free(p->ctab[i]);
  free(p->rtab);
  free(p->ctab);
  free(p->root);
  free(p);
}

int dlx_rows(dlx_t dlx) { return dlx->rtabn; }
int dlx_cols(dlx_t dlx) { return dlx->ctabn; }

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
  if (p->rtabn == p->rtab_alloc) {
    p->rtab = realloc(p->rtab, sizeof(cell_ptr) * (p->rtab_alloc *= 2));
  }
  p->rtab[p->rtabn++] = 0;
}

static void alloc_col(dlx_t p, int n) { while(p->ctabn <= n) dlx_add_col(p); }
static void alloc_row(dlx_t p, int n) { while(p->rtabn <= n) dlx_add_row(p); }

void dlx_mark_optional(dlx_t p, int col) {
  alloc_col(p, col);
  cell_ptr c = p->ctab[col];
  // Prevent undeletion by self-linking.
  LR_self(LR_delete(c));
}

void dlx_set(dlx_t p, int row, int col) {
  // We don't bother sorting. DLX works fine with jumbled rows and columns.
  // We just have to watch out for duplicates. (Actually, I think the DLX code
  // works even with duplicates, though it would be inefficient.)
  //
  // For a given column, the UD list is ordered in the order that dlx_set()
  // is called, not by row number. Similarly for a given row and its LR list.
  alloc_row(p, row);
  alloc_col(p, col);
  cell_ptr c = p->ctab[col];
  cell_ptr new1() {
    cell_ptr n = malloc(sizeof(*n));
    n->n = row;
    n->c = c;
    c->s++;
    UD_insert(n, c);
    return n;
  }
  cell_ptr *rp = p->rtab + row;
  if (!*rp) {
    *rp = LR_self(new1());
    return;
  }
  // Ignore duplicates.
  if ((*rp)->c->n == col) return;
  C(r, *rp, R) if (r->c->n == col) return;
  // Otherwise insert at end of LR list.
  LR_insert(new1(), *rp);
}

static void cover_col(cell_ptr c) {
  LR_delete(c);
  C(i, c, D) C(j, i, R) UD_delete(j)->c->s--;
}

static void uncover_col(cell_ptr c) {
  C(i, c, U) C(j, i, L) UD_restore(j)->c->s++;
  LR_restore(c);
}

int dlx_pick_row(dlx_t p, int i) {
  if (i < 0 || i >= p->rtabn) return -1;
  cell_ptr r = p->rtab[i];
  if (!r) return 0;  // Empty row.
  cover_col(r->c);
  C(j, r, R) cover_col(j->c);
  return 0;
}

int dlx_remove_row(dlx_t p, int i) {
  if (i < 0 || i >= p->rtabn) return -1;
  cell_ptr r = p->rtab[i];
  if (!r) return 0;  // Empty row.
  UD_delete(r)->c->s--;
  C(j, r, R){
    UD_delete(j)->c->s--;
  }
  p->rtab[i] = 0;
  free_row(r);
  return 0;
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
      if (stuck_cb) stuck_cb(c->n);
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
