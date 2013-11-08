// Solves logic grid puzzles using the DLX algorithm.
//
// We view a logic grid puzzle as follows. Given a MxN table of objects and
// some constraints, for each row except the first, we are to permute its
// entries so that the table satisfies the constraints.
//
// The simplest constraints specify that 2 particular objects must lie in
// the same column, or lie in different columns. More complex ones involve
// ordering.
//
// DLX also involves rows and columns. To avoid confusion, we'll use the
// terms DLX-rows and DLX-columns for these.
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "blt.h"
#include "dlx.h"

#define F(i, n) for(int i=0; i<n; i++)

#define NEW_ARRAY(A, MAX) malloc(sizeof(*A) * MAX)
#define GROW(A, N, MAX) if (N == MAX) A = realloc(A, sizeof(*A) * (MAX *= 2))

#define NORETURN __attribute__((__noreturn__))
void die(const char *err, ...) NORETURN __attribute__((format (printf, 1, 2)));
void die(const char *err, ...) {
  va_list params;
  va_start(params, err);
  vfprintf(stderr, err, params);
  fputc('\n', stderr);
  va_end(params);
  exit(1);
}

void forall_word(char *s, void f(char *)) {
  for(;;) {
    char *e = strchr(s, ' ');
    if (e) *e = 0;
    f(s);
    if (!e) break;
    s = e + 1;
  }
}

char *mallocgets() {
  char *s = 0;
  size_t len = 0;
  if (-1 == getline(&s, &len, stdin)) {
    free(s);
    return 0;
  }
  // Assumes newline before EOF.
  s[strlen(s) - 1] = 0;
  return s;
}

struct hint_s {
  char cmd;  // Type of clue.
  int (*coord)[2], n, coord_max;  // Arguments of clue.
  int dlx_col;  // If nonzero, base of DLX-columns representing this clue.
};
typedef struct hint_s *hint_ptr;

int main() {
  BLT *blt = blt_new();
  int M = 0, N = 0;
  // Read M lines of N space-delimited fields, terminated by "%%" on a
  // single line by itself.
  for(;;) {
    char *s = mallocgets();
    if (!s) die("expected %%");
    if (!strcmp(s, "%%")) {
      free(s);
      break;
    }
    int n = 0;
    void f(char *s) {
      int *coord = NEW_ARRAY(coord, 2);
      coord[0] = M;
      coord[1] = n++;
      if (blt_put_if_absent(blt, s, coord)) die("duplicate symbol: %s", s);
    }
    forall_word(s, f);
    if (!M) N = n; else if (N != n) die("line %d: wrong number of fields", M+1);
    M++;
    free(s);
  }
  char *sym[M][N];
  int add_sym(BLT_IT *it) {
    int *coord = it->data;
    sym[coord[0]][coord[1]] = it->key;
    return 1;
  }
  blt_forall(blt, add_sym);

  // Expect a list of constraints, one per line.
  int hint_n = 0, hint_max = 64;
  hint_ptr *hint = NEW_ARRAY(hint, hint_max);
  for(char *s = 0; (s = mallocgets()); free(s)) {
    hint_ptr h = 0;
    void f(char *s) {
      if (!h) {
        h = malloc(sizeof(*h));
        h->cmd = *s;
        h->n = 0;
        h->dlx_col = 0;
        h->coord_max = 2;
        h->coord = NEW_ARRAY(h->coord, h->coord_max);
        GROW(hint, hint_n, hint_max);
        hint[hint_n++] = h;
        return;
      }
      int *coord = blt_get(blt, s)->data;
      if (!coord) die("invalid symbol: %s", s);
      GROW(h->coord, h->n, h->coord_max);
      F(k, 2) h->coord[h->n][k] = coord[k];
      h->n++;
    }
    forall_word(s, f);
    if (h->cmd == '>') {
      if (h->n != 2) die("inequality must have exactly 2 fields");
      h->cmd = '<';
      void swap_int(int *x, int *y) { int tmp = *x; *x = *y, *y = tmp; }
      F(k, 2) swap_int(h->coord[0] + k, h->coord[1] + k);
    }
  }

  dlx_t dlx = dlx_new();

  // Generate all possible columns: an M-digit counter in base N.
  // Columns that pass initial checks become the DLX-rows.
  int a[M];  // Holds current column.
  // The first MN DLX-columns represent the symbols. These must be covered;
  // the others are optional.
  // The symbol at row r and column c corresponds to DLX-column N*r + c.
  int dlxM = 0, dlxN = M * N;
  // The array dlx_a records the columns that pass the initial checks and
  // hence added as a DLX-row.
  int dlx_max = 32, (*dlx_a)[M] = NEW_ARRAY(dlx_a, dlx_max);
  void f(int i) {
    int has(hint_ptr h, int i) { return a[h->coord[i][0]] == h->coord[i][1]; }
    int match(hint_ptr h) {
      int t = 0;
      F(i, h->n) t += has(h, i);
      return t;
    }
    if (i == M) {
      // Base case: finished generating a single column. If contraints allow
      // it, add a DLX-row representing this column, otherwise skip it.
      // The DLX-row has a 1 in the DLX-columns corresponding to the symbols
      // in the column.
      int anon(hint_ptr h) {
        switch(h->cmd) {
          case 'p': return (has(h, 0) && has(h, 1)) ||
              (has(h, 2) && has(h, 3)) || (match(h) | 2) != 2;
          case '=': return match(h) == 1;
          case '<':
          case '1':
          case 'A':
          case '!': return match(h) > 1;
          case 'i': return has(h, 0) && (match(h) | 2) != 2;
          // TODO: case '^': return matchcount(h) >= 2;
        }
        return 0;
      }
      F(i, hint_n) if (anon(hint[i])) return;
      // No constraints immediately disqualify this column.
      // Add a new DLX-row to represent it.
      GROW(dlx_a, dlxM, dlx_max);
      F(i, M) dlx_a[dlxM][i] = a[i];
      // Set the DLX-column coresponding to each symbol.
      F(k, M) dlx_set(dlx, dlxM, N*k + a[k]);
      // Add optional columns for constraints that need it.
      void assign_dlx_col(hint_ptr h) {
        if (!h->dlx_col) {
          h->dlx_col = dlxN;
          F(i, N) dlx_mark_optional(dlx, dlxN++);
        }
      }
      void opthints(hint_ptr h) {
        switch(h->cmd) {
          case '1':
            assign_dlx_col(h);
            if (has(h, 0)) {
              F(k, N) {
                if (k == a[0] + 1) continue;
                dlx_set(dlx, dlxM, h->dlx_col + k);
              }
            }
            if (has(h, 1)) {
              dlx_set(dlx, dlxM, h->dlx_col + a[0]);
            }
            break;
          case 'A':
            assign_dlx_col(h);
            if (has(h, 0)) {
              F(k, N) {
                if (abs(k - a[0]) == 1) continue;
                dlx_set(dlx, dlxM, h->dlx_col + k);
              }
            }
            if (has(h, 1)) {
              dlx_set(dlx, dlxM, h->dlx_col + a[0]);
            }
            break;
          case '<':
            assign_dlx_col(h);
            if (has(h, 0)) {
              for(int k = 0; k <= a[0]; k++) {
                dlx_set(dlx, dlxM, h->dlx_col + k);
              }
            }
            if (has(h, 1)) {
              for(int k = a[0]; k < N; k++) {
                dlx_set(dlx, dlxM, h->dlx_col + k);
              }
            }
            break;
        }
      }
      F(i, hint_n) opthints(hint[i]);
      dlxM++;
      return;
    }
    F(k, N) {
      a[i] = k;
      f(i+1);
    }
  }
  f(0);

  // Solve!
  void pr(int row[], int n) {
    F(i, n) {
      F(k, M) printf(" %s", sym[k][dlx_a[row[i]][k]]);
      putchar('\n');
    }
  }
  dlx_forall_cover(dlx, pr);

  dlx_clear(dlx);
  free(dlx_a);
  F(i, hint_n) free(hint[i]->coord), free(hint[i]);
  free(hint);
  {
    int f(BLT_IT *it) { free(it->data); return 1; }
    blt_forall(blt, f);
    blt_clear(blt);
  }
  return 0;
}
