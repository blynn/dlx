#include <stdio.h>
#include <stdlib.h>
#include "def.h"

#define REP(i, n) for(int i=0; i<n; i++)
#define NE(m, n) if (eq(m, n)) return 0;

int main() {
  char *s[3][6] = { { "Bob", "Chuck",  "Dave", "Ed", "Frank", "Gary"},
                    { "Hall", "King", "Noyes", "Pinza", "Veery", "White"},
                    { "AL", "AR", "AS", "ML", "MR", "MS" } };
  void x(int *i, int *j) {
    int t = *i;
    *i = *j, *j = t;
  }
  int a[6][2];
  int get(int m, int i, int n) {
    REP(k, 6) if ((m ? a[k][m-1] : k) == i) return a[k][n-1];
    fprintf(stderr, "unreachable!\n"), exit(1);
  }
  int eq(int m, int i, int n, int j) { return get(m, i, n) == j; }
  int g() {
    // 1. Every order was different.
    //
    // 2. Bob, King, and a man who ordered ravioli...
    //    Chuck, Hall, and a man who ordered spaghetti...
    if (get(Bob, 2)/3 != get(King, 2)/3) return 0;
    if (get(Chuck, 2)/3 != get(Hall, 2)/3) return 0;
    NE(Bob, King)
    NE(Bob, Hall);
    NE(Bob, AR);
    NE(Bob, MR);
    NE(King, AR);
    NE(King, MR);
    NE(Chuck, Hall);
    NE(Chuck, King);
    NE(Chuck, AS);
    NE(Chuck, MS);
    NE(Hall, AS);
    NE(Hall, MS);
    // 3. Gary and White both ordered lasagna; Hall did not.
    NE(Gary, AR);
    NE(Gary, AS);
    NE(Gary, MR);
    NE(Gary, MS);
    NE(White, AR);
    NE(White, AS);
    NE(White, MR);
    NE(White, MS);
    NE(Hall, AL);
    NE(Hall, ML);
    NE(Gary, White);
    NE(Gary, Hall);
    // 4. Frank didn't order minestrone; neither he nor Pinza ordered ravioli.
    NE(Frank, ML);
    NE(Frank, MR);
    NE(Frank, MS);
    NE(Frank, AR);
    NE(Pinza, AR);
    NE(Pinza, MR);
    NE(Frank, Pinza);
    // 5. Neither Ed nor Frank are Veery.
    NE(Ed, Veery);
    NE(Frank, Veery);
    return 1;
  }
  int n[2] = { 6, 6 }, p[2][6];
  REP(i, 2) REP(j, 6) p[i][j] = j;
  void f() {
    int i = 0;
    while(i < 2 && !n[i]) i++;
    if (i == 2) {
      if (g()) REP(k, 6) printf("%s, %s, %s\n", s[0][k], s[1][a[k][0]], s[2][a[k][1]]);
      return;
    }
    int *q = p[i];
    REP(k, n[i]) a[--n[i]][i] = q[k], x(q+k, q+n[i]), f(), x(q+k, q+n[i]++);
  }
  f();
  return 0;
}
