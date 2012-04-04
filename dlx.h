struct dlx_s;
typedef struct dlx_s *dlx_t;
dlx_t dlx_new();
void dlx_add_col(dlx_t);
void dlx_add_row(dlx_t);
void dlx_set_last_row(dlx_t, int col);
void dlx_search(dlx_t p);
void dlx_set_col_id(dlx_t, char *(*cb)(int));
void dlx_mark_optional(dlx_t, int col);
