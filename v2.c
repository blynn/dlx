#include <stdio.h>
#include <stdlib.h>
#include "cbt.h"
#include "darray.h"

#define F(i,n) for(int i = 0; i < n; i++)

struct col_s;
typedef struct col_s *col_ptr;
struct cell_s;
typedef struct cell_s *cell_ptr;

struct cell_s {
  cell_ptr U, D, L, R;
  col_ptr c;
};
typedef struct cell_s *cell_ptr;

struct col_s {
  // These 4 pointers must come first, and in this order.
  cell_ptr U, D;
  col_ptr L, R;
  int s, n;
  cbt_it it;
};

col_ptr col_new() {
  col_ptr c = malloc(sizeof(*c));
  c->s = 0;
  c->U = c->D = (cell_ptr) c;
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
  h->L = h->R = h;
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
  c->U = c->D = (cell_ptr) c;
  col_ptr h = p->root, last = h->L;
  h->L = last->R = c, c->L = last, c->R = h;
  cbt_put(it, c);
  c->it = it;
  c->n = cbt_size(p->ctab) - 1;
}

void dlx_dump_col(dlx_t p) {
  for(col_ptr c = p->root->R; c != p->root; c = c->R) {
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
  cell_ptr last = c->U;
  c->U = last->D = n;
  n->U = last;
  n->D = (cell_ptr) c;
  n->L = n->R = n;
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
  cell_ptr last = c->U;
  last->D = c->U = n;
  n->U = last;
  n->D = (cell_ptr) c;
  cell_ptr next = p->cur->R;
  next->L = p->cur->R = n; 
  n->L = p->cur;
  n->R = next;
  n->c = c;
  c->s++;
  p->cur = n;
}

void cover_col(col_ptr c) {
//printf("cover %s\n", cbt_key(c->it));
  c->R->L = c->L;
  c->L->R = c->R;
  for(cell_ptr i = c->D; i != (cell_ptr) c; i = i->D) {
    for(cell_ptr j = i->R; j != i; j = j->R) {
      j->U->D = j->D;
      j->D->U = j->U;
      j->c->s--;
    }
  }
}

void uncover_col(col_ptr c) {
//printf("uncover %s\n", cbt_key(c->it));
  for(cell_ptr i = c->U; i != (cell_ptr) c; i = i->U) {
    for(cell_ptr j = i->L; j != i; j = j->L) {
      j->c->s++;
      j->U->D = j->D->U = j;
    }
  }
  c->R->L = c->L->R = c;
}

static void cover_row(cell_ptr r) {
  cover_col(r->c);
  for(cell_ptr j = r->R; j != r; j = j->R) {
    cover_col(j->c);
  }
}

void dlx_search(dlx_t p) {
  col_ptr h = p->root;
  darray_ptr sol = darray_new();
  void search(int k) {
//printf("search %d\n", k);
    col_ptr c = h->R;
    if (c == h) {
      // Print solution.
      printf("solution:\n");
      void pr(void *data) {
        cell_ptr r = data;
        while(r->L->c->n < r->c->n) r = r->L;
        do {
          printf(" %s", cbt_key(r->c->it));
          r = r->R;
        } while (r->R->c->n > r->c->n);
        printf("\n");
      }
      darray_forall(sol, pr);
      return;
    }
    int s = c->s;
    for(col_ptr cc = c->R; cc != h; cc = cc->R) {
      if (cc->s < s) c = cc, s = c->s;
    }
printf("lvl %d: %s(%d)\n", k, cbt_key(c->it), s);
    if (!s) return;
    cover_col(c);
    for(cell_ptr r = c->D; r != (cell_ptr) c; r = r->D) {
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
