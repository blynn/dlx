// = Grizzly =
//
// Solves logic grid puzzles. By default, uses the DLX agorithm, but
// uses brute force if --alg=brute is given on the command-line.
//
// We view a logic grid puzzle as follows. Given a MxN table of distinct
// symbols and some constraints, for each row except the first, we are to
// permute its entries so that the table satisfies the constraints.
//
// DLX also involves rows and columns. To avoid confusion, we call them
// DLX-rows and DLX-columns.

#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
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

void swap_int(int *x, int *y) { int tmp = *x; *x = *y, *y = tmp; }

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

// Solves using brute force.
void brute(int M, int N, char *sym[M][N], int hint_n, hint_ptr *hint) {
  // For each row except the first, generate all permutations.
  int perm[M-1][N];
  F(m, M-1) F(n, N) perm[m][n] = n;
  void f(int m) {
    if (m == M-1) {
      // Base case: see if solution works.
      int check(hint_ptr h) {
        int get(int m, int n) { return m ? perm[m-1][n] : n; }
        int has(int i, int n) { return get(h->coord[i][0], n) == h->coord[i][1]; }
        int matchmax() {
          int count = 0;
          F(n, N) {
            int t = 0;
            F(i, h->n) t += has(i, n);
            if (count < t) count = t;
          }
          return count;
        }
        int col(int i) {
          F(n, N) if (has(i, n)) return n;
          die("unreachable");
        }

        switch(h->cmd) {
          case '=': return matchmax() < h->n;
          case '!': return matchmax() > 1;
          case '^': {
            int count = 0;
            F(n, N) {
              int t = 0;
              F(i, h->n) t += has(i, n);
              count += t >= 2;
            }
            return count > 1;
          }
          case '<': return col(0) >= col(1);
          case '>': return col(0) <= col(1);
          case '1': return col(0) + 1 != col(1);
          case 'A': return abs(col(0) - col(1)) != 1;
          case 'i':
            F(n, N) if (has(0, n)) {
              for (int i = 1; i < h->n; i++) if (has(i, n)) return 0;
              return 1;
            }
          case 'p':
            F(n, N) {
              if (has(0, n) && has(1, n)) return 1;
              if (has(2, n) && has(3, n)) return 1;
              int t = 0;
              F(i, 4) t += has(i, n);
              if ((t | 2) != 2) return 1;
            }
            return 0;
          case 'X': {
            int count = 0;
            F(n, N) F(i, h->n/2) count += has(2*i, n) && has(2*i + 1, n);
            return count > 1;
          }
        }
        return 0;
      }
      F(i, hint_n) if (check(hint[i])) return;
      F(n, N) {
        printf("%s", sym[0][n]);
        F(m, M-1) printf(" %s", sym[m+1][perm[m][n]]);
        puts("");
      }
      return;
    }
    // Generate all permutations of row m.
    void g(int k) {
      if (k == N) {
        // Base case: recurse to next row.
        f(m + 1);
        return;
      }
      for(int i = k; i < N; i++) {
        swap_int(perm[m] + k, perm[m] + i);
        g(k + 1);
        swap_int(perm[m] + k, perm[m] + i);
      }
    }
    g(0);
  }
  f(0);
}

// Solves using DLX where each possible column corresponds to a subset in
// the collection.
void per_col_dlx(int M, int N, char *sym[M][N], int hint_n, hint_ptr *hint) {
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
          case '^':
            if (!h->dlx_col) {
              h->dlx_col = dlxN;
              dlx_mark_optional(dlx, dlxN++);
            }
            int count = 0;
            F(k, h->n) count += has(h, k);
            if (count >= 2) {
              dlx_set(dlx, dlxM, h->dlx_col);
            }
            break;
          case 'X':
            if (!h->dlx_col) {
              h->dlx_col = dlxN;
              dlx_mark_optional(dlx, dlxN++);
            }
            F(k, h->n/2) if (has(h, 2*k) && has(h, 2*k + 1)) dlx_set(dlx, dlxM, h->dlx_col);
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
      F(k, M) {
        if (k) putchar(' ');
        printf("%s", sym[k][dlx_a[row[i]][k]]);
      }
      putchar('\n');
    }
  }
  dlx_forall_cover(dlx, pr);
  dlx_clear(dlx);
  free(dlx_a);
}

void per_cell_dlx(int M, int N, char *sym[M][N], int hint_n, hint_ptr *hint) {
  dlx_t dlx = dlx_new();
  // It's easier to add all rows then subtract forbidden rows at the end than
  // to attempt a purely additive construction of the DLX-table.
  int remove_me[(M-1)*N*N];

  // DLX-rows: row (m*N + n)*N + k = sym[m+1][n] in the kth column.
  F(m, M-1) F(n, N) F(k, N) {
    int r = (m*N + n)*N + k;
    // sym[m+1][n] must be used exactly once.
    dlx_set(dlx, r, m*N + n);
    // solution[m+1][k] must contain exactly one symbol.
    dlx_set(dlx, r, (M-1)*N + m*N + k);
    remove_me[r] = 0;
  }

  // The first row of the puzzle is a special case, complicating our code.
  int sol[M-1][N];
  F(m, M-1) F(n, N) sol[m][n] = -1;
  int base = 2*(M-1)*N;
  F(i, hint_n) {
    hint_ptr h = hint[i];
    int row_of(int x, int k) {
      return ((h->coord[x][0] - 1)*N + h->coord[x][1])*N + k;
    }
    switch(h->cmd) {
      case '=': {
        int firstrow = 0;
        F(x, h->n) if (!h->coord[x][0]) {
          F(y, h->n) if (x != y) {
            sol[h->coord[y][0] - 1][h->coord[x][1]] = h->coord[y][1];
          }
          firstrow = 1;
          break;
        }
        if (firstrow) break;

        F(x, h->n) F(k, N) {
          dlx_set(dlx, row_of(x, k), base + k);
          F(y, h->n) if (x != y) F(n, N) if (n != k) {
            dlx_set(dlx, row_of(y, n), base + k);
          }
          F(k, N) dlx_mark_optional(dlx, base++);
        }
        break;
      }
      case '!':
        F(x, h->n) {
          if (!h->coord[x][0]) {
            F(y, h->n) if (x != y && h->coord[y][0]) {
              remove_me[row_of(y, h->coord[x][1])] = 1;
            }
          } else F(k, N) {
            dlx_set(dlx, row_of(x, k), base + k);
            F(y, h->n) if (x != y && h->coord[y][0]) {
              dlx_set(dlx, row_of(y, k), base + k);
            }
            F(k, N) dlx_mark_optional(dlx, base++);
          }
        }
        break;
      case '<':
        if (!h->coord[0][0]) {
          F(k, h->coord[0][1]+1) remove_me[row_of(1, k)] = 1;
          break;
        }
        if (!h->coord[1][0]) {
          for (int k = h->coord[1][1]; k < N; k++) {
            remove_me[row_of(0, k)] = 1;
          }
          break;
        }
        F(k, N) {
          dlx_set(dlx, row_of(0, k), base + k);
          F(n, k+1) dlx_set(dlx, row_of(1, n), base + k);
        }
        F(k, N) dlx_mark_optional(dlx, base++);
        break;
      case '1': {
        int x = !h->coord[1][0];
        if (!h->coord[x][0]) {
          F(k, N) if (h->coord[x][1] - k != 1) remove_me[row_of(!x, k)] = 1; 
          break;
        }
        F(k, N) {
          dlx_set(dlx, row_of(0, k), base + k);
          F(n, N) if (n - k != 1) dlx_set(dlx, row_of(1, n), base + k);
        }
        F(k, N) dlx_mark_optional(dlx, base++);
        break;
      }
      case 'A': {
        int x = !h->coord[1][0];
        if (!h->coord[x][0]) {
          F(k, N) if (abs(h->coord[x][1] - k) != 1) remove_me[row_of(!x, k)] = 1; 
          break;
        }
        F(k, N) {
          dlx_set(dlx, row_of(0, k), base + k);
          F(n, N) if (abs(n - k) != 1) dlx_set(dlx, row_of(1, n), base + k);
        }
        F(k, N) dlx_mark_optional(dlx, base++);
        break;
      }
    }
  }
  F(r, (M-1)*N*N) if (remove_me[r]) dlx_remove_row(dlx, r);
  F(m, M-1) F(n, N) if (sol[m][n] >= 0) dlx_pick_row(dlx, (m*N + sol[m][n])*N + n);
  // Solve!
  void f(int row[], int row_n) {
    F(i, row_n) sol[row[i]/N/N][row[i]%N] = row[i]/N%N;
    F(n, N) {
      printf("%s", sym[0][n]);
      F(m, M-1) printf(" %s", sym[m+1][sol[m][n]]);
      putchar('\n');
    }
  }
  dlx_forall_cover(dlx, f);
  dlx_clear(dlx);
}

int main(int argc, char *argv[]) {
  void (*alg)(int M, int N, char *sym[M][N], int hint_n, hint_ptr *hint)
      = per_col_dlx;
  for (;;) {
    static struct option longopts[] = {
        {"alg", required_argument, 0, 'a'},
        {0, 0, 0, 0},
    };
    int c = getopt_long(argc, argv, "", longopts, 0);
    if (c == -1) break;
    switch(c) {
      case 'a':
        if (!strcmp(optarg, "brute")) {
          alg = brute;
        } else if (!strcmp(optarg, "per_col_dlx")) {
          alg = per_col_dlx;
        } else if (!strcmp(optarg, "per_cell_dlx")) {
          alg = per_cell_dlx;
        } else {
          printf("Unknown algorithm\n");
          exit(0);
        }
        break;
      case '?':
        exit(0);
      default: die("unreachable!");
    }
  }
  BLT *blt = blt_new();
  int M = 0, N = 0;
  // Read M lines of N space-delimited fields, terminated by "%%" on a
  // single line by itself.
  for(;;) {
    char *s = mallocgets();
    if (!s) die("expected %%%%");
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
  void add_sym(BLT_IT *it) {
    int *coord = it->data;
    sym[coord[0]][coord[1]] = it->key;
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
      BLT_IT *it = blt_get(blt, s);
      if (!it) die("invalid symbol: %s", s);
      int *coord = it->data;
      GROW(h->coord, h->n, h->coord_max);
      F(k, 2) h->coord[h->n][k] = coord[k];
      h->n++;
    }
    forall_word(s, f);
    if (h->cmd == '>') {
      if (h->n != 2) die("inequality must have exactly 2 fields");
      h->cmd = '<';
      F(k, 2) swap_int(h->coord[0] + k, h->coord[1] + k);
    }
  }

  alg(M, N, sym, hint_n, hint);
  F(i, hint_n) free(hint[i]->coord), free(hint[i]);
  free(hint);
  {
    void f(BLT_IT *it) { free(it->data); }
    blt_forall(blt, f);
    blt_clear(blt);
  }
  return 0;
}
