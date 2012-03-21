#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define F(i, n) for(int i=0; i<n; i++)

void fword(char *s, void f(char *)) {
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
    if (-1 == getline(&s, &len, stdin)) return 0;
    // Assumes newline before EOF.
    s[strlen(s) - 1] = 0;
    return s;
}

struct hint_s {
  char cmd;
  int i[16], j[16], n;
} hint[16];
typedef struct hint_s *hint_ptr;

int main() {
  int dim = 0, m = 0;
  char *sym[8][8];

  for(char *s = 0; (s = mallocgets()) && strcmp(s, "%%"); free(s)) {
    int i = 0;
    void f(char *s) { sym[dim][i++] = strdup(s); }
    fword(s, f);
    if (!dim) {
      m = i;
    } else if (m != i) {
      fprintf(stderr, "line %d: wrong number of fields\n", dim + 1);
      exit(1);
    }
    dim++;
  }
  hint_ptr hinte = hint;
  for(char *s = 0; (s = mallocgets()); free(s)) {
    hinte->n = -1;
    void f(char *s) {
      if (hinte->n == -1) {
        hinte->cmd = *s;
        hinte->n++;
        return;
      }
      F(i, dim) F(j, m) if (!strcmp(sym[i][j], s)) {
        hinte->i[hinte->n] = i;
        hinte->j[hinte->n] = j;
        hinte->n++;
        return;
      }
      fprintf(stderr, "invalid symbol: %s\n", s);
      exit(1);
    }
    fword(s, f);
    hinte++;
  }

  int n[dim-1], p[dim-1][m], a[m][dim-1];

  int has(int k, int m, int i) { return (m ? a[k][m-1] : k) == i; }

  int match(hint_ptr p) {
    F(k, m) {
      int t = 0;
      F(n, p->n) if ((t += has(k, p->i[n], p->j[n])) > 1) return 1;
    }
    return 0;
  }

  F(i, dim-1) {
    n[i] = m;
    F(j, m) p[i][j] = j;
  }
  void f() {
    F(i, dim-1) if (n[i]) {
      int *q = p[i];
      F(k, n[i]) a[--n[i]][i] = q[k], q[k] = q[n[i]], f(), q[k] = a[n[i]++][i];
      return;
    }
    for(hint_ptr p = hint; p != hinte; p++) {
      switch(p->cmd) {
        case 'E':
          if (!match(p)) return;
          break;
        case 'N':
          if (match(p)) return;
          break;
      }
    }
    F(k, m) {
      fputs(sym[0][k], stdout);
      F(i, dim-1) printf(" %s", sym[i+1][a[k][i]]);
      puts("");
    }
  }
  f();
  return 0;
}
