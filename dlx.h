// The 'dlx_t' type is an opaque pointer that holds an instance of the exact
// cover problem represented as a sparse matrix of 0s and 1s. The library
// provides a function that solves the instance using
// http://en.wikipedia.org/wiki/Dancing_Links[the DLX algorithm].
//
// Row and column numbers are 0-indexed.

struct dlx_s;
typedef struct dlx_s *dlx_t;

// Returns new empty exact cover problem. No rows. No columns.
dlx_t dlx_new();

// Frees exact cover problem.
void dlx_clear();

// Returns number of rows.
int dlx_rows(dlx_t dlx);

// Returns number of columns.
int dlx_cols(dlx_t dlx);

// Places a 1 in the given row and column.
// Increases the number of rows and columns if necessary.
void dlx_set(dlx_t dlx, int row, int col);

// Marks a column as optional: a solution need not cover the given column,
// but it still must respect the constraints it entails.
void dlx_mark_optional(dlx_t dlx, int col);

// Removes a row from consideration. Returns 0 on success, -1 otherwise.
// Should only be called after all dlx_set() calls.
int dlx_remove_row(dlx_t p, int row);

// Picks a row to be part of the solution. Returns 0 on success, -1 otherwise.
// Should only be called after all dlx_set() calls and dlx_remove_row() calls.
// TODO: Check the row can be legally chosen.
int dlx_pick_row(dlx_t dlx, int row);

// Runs the DLX algorithm, and for every exact cover, calls the given callback
// with an array containing all the row numbers of the solution and the size of
// said array.
void dlx_forall_cover(dlx_t dlx, void (*cb)(int rows[], int n));

// Runs the DLX algorithm, calling the appropriate callback when:
//
//  * a column is covered by selectng a row (cover_cb)
//  * a column is uncovered because of backtracking (uncover_cb)
//  * a solution is found (found_cb)
//  * there is no solution with the rows chosen so far (stuck_cb)
//
// The callback cover_cb is given the column covered, the number of rows
// that can legally cover the column, and the selected row.
//
// The callback stuck_cb is given the first uncoverable column.
void dlx_solve(dlx_t dlx,
               void (*cover_cb)(int col, int s, int row),
               void (*uncover_cb)(),
               void (*found_cb)(),
               void (*stuck_cb)(int col));
