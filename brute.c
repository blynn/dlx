#include <stdio.h>
#include <stdlib.h>
#include "def.h"

#define F(i, n) for(int i=0; i<n; i++)
#define N(...) if (match((int[]){__VA_ARGS__, -1})) return 0;

int main() {
  char *s[3][6] = { { "Bob", "Chuck",  "Dave", "Ed", "Frank", "Gary"},
                    { "Hall", "King", "Noyes", "Pinza", "Veery", "White"},
                    { "AL", "AR", "AS", "ML", "MR", "MS" } };
  int a[6][2], swp;
  void x(int *i, int *j) { swp = *i, *i = *j, *j = swp; }
  int has(int k, int m, int i) { return (m ? a[k][m-1] : k) == i; }
  int starter(int m, int i) {
    F(k, 6) if (has(k, m, i)) return a[k][1]/3;
    fprintf(stderr, "unreachable!\n"), exit(1);
  }
  int match(int *p) {
    F(k, 6) for(int n=0, *q=p; *q>=0; q+=2) if ((n += has(k, *q, q[1])) > 1) return 1;
    return 0;
  }
  int g() {
    // 1. Every order was different.
    //
    // 2. Bob, King, and a man who ordered ravioli...
    //    Chuck, Hall, and a man who ordered spaghetti...
    if (starter(Bob) != starter(King)) return 0;
    if (starter(Chuck) != starter(Hall)) return 0;
    N(Bob, King, AR, MR);
    N(Chuck, Hall, AS, MS);
    N(Bob, Chuck, Hall, King);
    // 3. Gary and White both ordered lasagna; Hall did not.
    N(Gary, White, AR, AS, MR, MS);
    N(Hall, AL, ML);
    N(Gary, Hall);
    // 4. Frank didn't order minestrone; neither he nor Pinza ordered ravioli.
    N(Frank, ML, MR, MS);
    N(Frank, Pinza, AR, MR);
    // 5. Neither Ed nor Frank are Veery.
    N(Ed, Frank, Veery);
    return 1;
  }
  int n[2] = { 6, 6 }, p[2][6];
  F(i, 2) F(j, 6) p[i][j] = j;
  void f() {
    F(i, 2) if (n[i]) {
      int *q = p[i];
      F(k, n[i]) a[--n[i]][i] = q[k], x(q+k, q+n[i]), f(), x(q+k, q+n[i]++);
      return;
    }
    if (g()) F(k, 6) printf("%s, %s, %s\n", s[0][k], s[1][*a[k]], s[2][a[k][1]]);
  }
  f();
  return 0;
}
