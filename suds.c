#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define F(i,n) for(int i = 0; i < n; i++)
#define C(i,n,dir) for(cell_t i = n->dir; i != n; i = i->dir)

typedef struct cell_s *cell_t;
struct cell_s {
  cell_t U, D, L, R, c;
  int s, n;
};

cell_t cell_new() { return malloc(sizeof(struct cell_s)); }
cell_t *cell_array(int n) { return malloc(sizeof(struct cell_s) * n); }
// Dance steps.
cell_t UD_remove (cell_t j) { return j->U->D = j->D, j->D->U = j->U, j; }
cell_t UD_restore(cell_t j) { return j->U->D = j->D->U = j; }
cell_t UD_self   (cell_t j) { return j->U = j->D = j; }
cell_t UD_insert (cell_t j, cell_t k) {
  return j->U = k->U, j->D = k, k->U = k->U->D = j;
}
cell_t LR_remove (cell_t j) { return j->L->R = j->R, j->R->L = j->L, j; }
cell_t LR_restore(cell_t j) { return j->L->R = j->R->L = j; }
cell_t LR_self   (cell_t j) { return j->L = j->R = j; }
cell_t LR_insert (cell_t j, cell_t k) {
  return j->L = k->L, j->R = k, k->L = k->L->R = j;
}

typedef struct dlx_s {
  cell_t *rtab, *ctab, *out, root;
  void (*sol_cb)(int);
} *dlx_t;

dlx_t dlx_new(int rows, int cols, int levels, void (*sol_cb)(int)) {
  dlx_t p = malloc(sizeof(*p));
  *p = (struct dlx_s) { cell_array(rows), cell_array(cols), cell_array(levels),
                        LR_self(cell_new()), sol_cb };
  F(i, cols) {
    cell_t c = cell_new();
    c->s = 0;
    p->ctab[c->n = i] = LR_insert(UD_self(c), p->root);
  }
  F(i, rows) p->rtab[i] = 0;
  return p;
}

void dlx_set(dlx_t p, int row, int col) {
  cell_t c = p->ctab[col], r = p->rtab[row];
  cell_t n = UD_insert(cell_new(), c);
  (n->c = c)->s++;
  (r ? LR_insert(n, r) : LR_self(p->rtab[row] = n))->n = row;
}

static void cover(cell_t c) {
  LR_remove(c);
  C(i, c, D) C(j, i, R) UD_remove(j)->c->s--;
}

static void uncover(cell_t c) {
  C(i, c, U) C(j, i, L) UD_restore(j)->c->s++;
  LR_restore(c);
}

void dlx_cover_row(dlx_t p, int row) {
  cell_t r = p->rtab[row];
  cover(r->c);  // Assumes row is nonempty.
  C(j, r, R) cover(j->c);
}

void dlx_search(dlx_t p) {
  void search(int k) {
    cell_t c = p->root->R;
    if (c == p->root) {  // Solution: send output to callback followed by -1.
      F(i, k) p->sol_cb(p->out[i]->n);
      p->sol_cb(-1);
      return;
    }
    int s = INT_MAX;  // S-heuristic: choose first most-constrained column.
    C(i, p->root, R) if (i->s < s) s = (c = i)->s;
    // Let's dance! Tiny optimization: if (!s) return;
    cover(c);
    C(r, c, D) {
      p->out[k] = r;
      C(j, r, R) cover(j->c);
      search(k+1);
      C(j, r, L) uncover(j->c);
    }
    uncover(c);
  }
  search(0);
}

int main() {
  int a[9][9] = {{0}}, c;
  F(i, 9) F(j, 9) do if (EOF == (c = getchar())) return 1; while(
      isdigit(c) ? a[i][j] = c - '0', 0 : c != '.');
  void print_solution(int r) {
    if (r >= 0) a[r/9%9][r%9] = r/9/9 + 1;
    else F(r, 9) F(c, 9 || (putchar('\n'), 0)) putchar('0'+a[r][c]);
  }
  dlx_t dlx = dlx_new(9*9*9, 4*9*9, 9*9, print_solution);
  int nine(int a, int b, int c) { return 9*9*a + 9*b + c; }
  F(d, 9) F(r, 9) F(c, 9) {
    int cond[4][2] = {  // Constraints:
        {r, c},                // One digit per box.
        {r, d},                // One digit per row.
        {c, d},                // One digit per column.
        {(r/3*3 + c/3), d} };  // One digit per 3x3 region.
    F(i, 4) dlx_set(dlx, nine(d, r, c), nine(i, cond[i][0], cond[i][1]));
  }
  // Fill in the given digits, and go!
  F(r, 9) F(c, 9) if (a[r][c]) dlx_cover_row(dlx, nine(a[r][c]-1, r, c));
  dlx_search(dlx);
  return 0;
}
