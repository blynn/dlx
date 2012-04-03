#include <stdio.h>
#include <stdlib.h>
#include "cbt.h"
#include "darray.h"

#define F(i,n) for(int i = 0; i < n; i++)

struct col_s;
typedef struct col_s *col_ptr;

struct cell_s {
  struct cell_s *ud[2], *lr[2];
  col_ptr c;
};
typedef struct cell_s *cell_ptr;

struct col_s {
  // These 4 pointers must come first, and in this order.
  cell_ptr ud[2];
  col_ptr lr[2];
  int s, n;
  cbt_it it;
};

col_ptr col_new() {
  col_ptr c = malloc(sizeof(*c));
  c->s = 0;
  c->ud[0] = c->ud[1] = (cell_ptr) c;
  return c;
}

struct dlx_s {
  cbt_t ctab, rtab;
  col_ptr root;
  cell_ptr cur;
};
typedef struct dlx_s *dlx_t;

dlx_t dlx_new() {
  dlx_t p = malloc(sizeof(*p));
  p->ctab = cbt_new();
  p->rtab = cbt_new();
  col_ptr h = col_new();
  h->lr[0] = h->lr[1] = h;
  p->root = h;
  p->cur = 0;
  return p;
}

void dlx_add_col(dlx_t p, char *key) {
  cbt_it it;
  void *f(void *data) { free(data); return 0; }
  if (!cbt_insert_with(&it, p->ctab, f, key)) {
    fprintf(stderr, "duplicate column key: %s\n", key);
    return;
  }
  col_ptr c = col_new();
  c->ud[0] = c->ud[1] = (cell_ptr) c;
  col_ptr h = p->root, last = h->lr[0];
  h->lr[0] = last->lr[1] = c, c->lr[0] = last, c->lr[1] = h;
  cbt_put(it, c);
  c->it = it;
  c->n = cbt_size(p->ctab) - 1;
}

void dlx_dump_col(dlx_t p) {
  for(col_ptr c = p->root->lr[1]; c != p->root; c = c->lr[1]) {
    printf("%s(%d)\n", cbt_key(c->it), c->n);
  }
}

void dlx_add_row(dlx_t p, char *rkey, char *ckey) {
  cbt_it it;
  void *f(void *data) { free(data); return 0; }
  if (!cbt_insert_with(&it, p->rtab, f, rkey)) {
    fprintf(stderr, "duplicate row key: %s\n", rkey);
    return;
  }
  cell_ptr n = malloc(sizeof(*n));
  cbt_put(it, n);
  it = cbt_at(p->ctab, ckey);
  if (cbt_is_off(it)) {
    fprintf(stderr, "column key not found: %s\n", ckey);
    return;
  }
  col_ptr c = cbt_get(it);
  cell_ptr last = c->ud[0];
  c->ud[0] = last->ud[1] = n;
  n->ud[0] = last;
  n->ud[1] = (cell_ptr) c;
  n->lr[0] = n->lr[1] = n;
  n->c = c;
  c->s++;
  p->cur = n;
}

void dlx_add_1(dlx_t p, char *key) {
  if (!p->cur) {
    fprintf(stderr, "no rows yet");
    return;
  }
  cbt_it it = cbt_at(p->ctab, key);
  if (cbt_is_off(it)) {
    fprintf(stderr, "column key not found: %s\n", key);
    return;
  }
  col_ptr c = cbt_get(it);
  if (c->n <= p->cur->c->n) {
    // We enforce this to avoid duplicate columns in a row, and for simplicity,
    // though the algorithm works even if the columns were jumbled.
    fprintf(stderr, "must add columns in incresaing order: %s\n", key);
    return;
  }
  cell_ptr n = malloc(sizeof(*n));
  cell_ptr last = c->ud[0];
  last->ud[1] = c->ud[0] = n;
  n->ud[0] = last;
  n->ud[1] = (cell_ptr) c;
  cell_ptr next = p->cur->lr[1];
  next->lr[0] = p->cur->lr[1] = n; 
  n->lr[0] = p->cur;
  n->lr[1] = next;
  n->c = c;
  c->s++;
  p->cur = n;
}

void cover_col(col_ptr c) {
//printf("cover %s\n", cbt_key(c->it));
  c->lr[1]->lr[0] = c->lr[0];
  c->lr[0]->lr[1] = c->lr[1];
  for(cell_ptr i = c->ud[1]; i != (cell_ptr) c; i = i->ud[1]) {
    for(cell_ptr j = i->lr[1]; j != i; j = j->lr[1]) {
      j->ud[0]->ud[1] = j->ud[1];
      j->ud[1]->ud[0] = j->ud[0];
      j->c->s--;
    }
  }
}

void uncover_col(col_ptr c) {
//printf("uncover %s\n", cbt_key(c->it));
  for(cell_ptr i = c->ud[0]; i != (cell_ptr) c; i = i->ud[0]) {
    for(cell_ptr j = i->lr[0]; j != i; j = j->lr[0]) {
      j->c->s++;
      j->ud[0]->ud[1] = j->ud[1]->ud[0] = j;
    }
  }
  c->lr[1]->lr[0] = c->lr[0]->lr[1] = c;
}

static void cover_row(cell_ptr r) {
  cover_col(r->c);
  for(cell_ptr j = r->lr[1]; j != r; j = j->lr[1]) {
    cover_col(j->c);
  }
}

void dlx_search(dlx_t p) {
  col_ptr h = p->root;
  darray_ptr sol = darray_new();
  void search(int k) {
//printf("search %d\n", k);
    col_ptr c = h->lr[1];
    if (c == h) {
      // Print solution.
      printf("solution:\n");
      void pr(void *data) {
        cell_ptr r = data;
        while(r->lr[0]->c->n < r->c->n) r = r->lr[0];
        do {
          printf(" %s", cbt_key(r->c->it));
          r = r->lr[1];
        } while (r->lr[1]->c->n > r->c->n);
        printf("\n");
      }
      darray_forall(sol, pr);
      return;
    }
    int s = c->s;
    for(col_ptr cc = c->lr[1]; cc != h; cc = cc->lr[1]) {
      if (cc->s < s) c = cc, s = c->s;
    }
printf("lvl %d: %s(%d)\n", k, cbt_key(c->it), s);
    if (!s) return;
    cover_col(c);
    for(cell_ptr r = c->ud[1]; r != (cell_ptr) c; r = r->ud[1]) {
      darray_append(sol, r);
      for(cell_ptr j = r->lr[1]; j != r; j = j->lr[1]) {
        cover_col(j->c);
      }
      search(k+1);
      darray_remove_last(sol);
      for(cell_ptr j = r->lr[0]; j != r; j = j->lr[0]) {
        uncover_col(j->c);
      }
    }
    uncover_col(c);
  }
  search(0);
}

int main() {
  static int grid[9][9];
  char s[80];
  F(i, 9) {
    // TODO: malloc getline
    if (EOF == scanf("%s\n", s)) {
      fprintf(stderr, "input error\n");
      exit(128);
    }
    F(j, 9) {
      if (s[j] == '\n') break;
      grid[i][j] = s[j] >= '1' && s[j] <= '9' ? s[j] - '0' : 0;
    }
  }

  dlx_t dlx = dlx_new();
  // One digit per box.
  F(r, 9) F(c, 9) {
    sprintf(s, "%d,%d", r, c);
    dlx_add_col(dlx, s);
  }
  // Once per row.
  F(r, 9) F(n, 9) {
    sprintf(s, "%dr%d", n, r);
    dlx_add_col(dlx, s);
  }
  // Once per column.
  F(n, 9) F(c, 9) {
    sprintf(s, "%dc%d", n, c);
    dlx_add_col(dlx, s);
  }
  // Once per 3x3 region.
  F(n, 9) F(r, 3) F(c, 3) {
    sprintf(s, "%d %d,%d", n, r, c);
    dlx_add_col(dlx, s);
  }
  // Add rows:
  F(n, 9) F(r, 9) F(c, 9) {
    char key[16];
    sprintf(s, "%d %d %d", n, r, c);
    sprintf(key, "%d,%d", r, c);
    dlx_add_row(dlx, s, key);
    sprintf(key, "%dr%d", n, r);
    dlx_add_1(dlx, key);
    sprintf(key, "%dc%d", n, c);
    dlx_add_1(dlx, key);
    sprintf(key, "%d %d,%d", n, r/3, c/3);
    dlx_add_1(dlx, key);
  }

  for (int i = 0; i < 9; i++) {
    for (int j = 0; j < 9; j++) {
      if (grid[i][j]) {
        sprintf(s, "%d %d %d", grid[i][j] - 1, i, j);
        cover_row((cell_ptr) cbt_get(cbt_at(dlx->rtab, s)));
      }
    }
  }

  dlx_search(dlx);
  return 0;
}
