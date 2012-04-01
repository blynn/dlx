#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "darray.h"

#define F(i, n) for(int i=0; i<n; i++)

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
  int i[8], j[8], k[8], n;
};
typedef struct hint_s *hint_ptr;

darray_ptr hints;

int main() {
  int dim = 0, m = 0;
  char *sym[16][16];
  hints = darray_new();

  for(char *s = 0; (s = mallocgets()) && strcmp(s, "%%"); free(s)) {
    int i = 0;
    void f(char *s) {
      F(i, dim) F(j, m) if (!strcmp(sym[i][j], s)) die("duplicate symbol: %s", s);
      F(j, i) if (!strcmp(sym[dim][j], s)) die("duplicate symbol: %s", s);
      sym[dim][i++] = strdup(s);
    }
    fword(s, f);
    if (!dim) m = i; else if (m != i) die("line %d: wrong number of fields", dim + 1);
    dim++;
  }
  for(char *s = 0; (s = mallocgets()); free(s)) {
    hint_ptr h = 0;
    void f(char *s) {
      if (!h) {
        h = malloc(sizeof(*h));
        h->cmd = *s;
        h->n = 0;
        darray_append(hints, h);
        return;
      }
      h->k[h->n] = 0;
      char *d = strchr(s, '.');
      if (d) {
        *d = 0;
        h->k[h->n] = atoi(d+1);
      }
      F(i, dim) F(j, m) if (!strcmp(sym[i][j], s)) {
        h->i[h->n] = i;
        h->j[h->n] = j;
        h->n++;
        return;
      }
      die("invalid symbol: %s", s);
    }
    fword(s, f);
  }

  int n[dim-1], p[dim-1][m], a[m][dim-1];

  int get(int k, int m) { return m ? a[k][m-1] : k; }
  int has(int k, int m, int i) { return get(k, m) == i; }

  int match(hint_ptr p) {
    F(k, m) {
      int t = 0;
      F(n, p->n) if ((t += has(k, p->i[n], p->j[n])) > 1) return 1;
    }
    return 0;
  }
  int matchcount(hint_ptr p) {
    int count = 0;
    F(k, m) {
      int t = 0;
      F(n, p->n) t += has(k, p->i[n], p->j[n]);
      count += (t > 1);
    }
    return count;
  }
  int attr(hint_ptr p, int n) {
    F(k, m) if (get(k, p->i[n]) == p->j[n]) return get(k, p->k[n]);
    die("unreachable");
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
    int anon(void *data) {
      hint_ptr p = data;
      switch(p->cmd) {
        case '=': return !match(p);
        case '!': return match(p);
        case '^': return matchcount(p) >= 2;
        case '<': return attr(p, 0) >= attr(p, 1);
        case '>': return attr(p, 0) <= attr(p, 1);
        case '1': return attr(p, 0) + 1 != attr(p, 1);
        case 'A': return abs(attr(p, 0) - attr(p, 1)) != 1;
      }
      return 0;
    }
    if (darray_until(hints, anon)) return;
    F(k, m) {
      fputs(sym[0][k], stdout);
      F(i, dim-1) printf(" %s", sym[i+1][a[k][i]]);
      puts("");
    }
  }
  f();
  return 0;
}
