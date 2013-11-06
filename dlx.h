struct dlx_s;
typedef struct dlx_s *dlx_t;

dlx_t dlx_new();
int dlx_rows(dlx_t dlx);
int dlx_cols(dlx_t dlx);
void dlx_set(dlx_t dlx, int row, int col);
void dlx_mark_optional(dlx_t dlx, int col);
void dlx_solve(dlx_t dlx, int (*cb)(int row[], int n));
