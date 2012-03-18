// Pipe output to def.h.
#include <stdio.h>
#define REP(i, n) for(int i=0; i<n; i++)
int main() {
  char *s[3][6] = { { "Bob", "Chuck",  "Dave", "Ed", "Frank", "Gary"},
                    { "Hall", "King", "Noyes", "Pinza", "Veery", "White"},
                    { "AL", "AR", "AS", "ML", "MR", "MS" } };
  REP(i, 3) REP(j, 6) printf("#define %s %d, %d\n", s[i][j], i, j);
  return 0;
}
