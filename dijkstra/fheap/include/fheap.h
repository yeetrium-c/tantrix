#ifndef __FHEAP__
#define __FHEAP__

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

/*
 * true <=> ok
 * false <=> failure
 */
typedef bool callback_ret;


typedef double fheap_key_t;
typedef uint64_t val_t;

#define VAL_OF_INT(x) ((val_t) x)
#define INT_OF_VAL(v) (v)
#define VAL_OF_DOUBLE(x) (*(val_t*)&x)
#define DOUBLE_OF_VAL(v) (*(double*)&v)

typedef struct key_val_pair_s
{
  fheap_key_t key;
  val_t val;
} key_val_pair;

// 56 bytes
typedef struct fheap_node_s
{
  key_val_pair pair;
  struct fheap_node_s *prev, *next;
  // dynamic array of pointers to childrens
  size_t count, capacity;
  struct fheap_node_s **items;
} fheap_node;

typedef struct fheap_s
{
  fheap_node* root, *min;
  uint64_t n;
} fheap;

#define PAIR_OF_NODE(n) (n.pair)

typedef callback_ret (*callback)(fheap_node*);


/*
 * `it` is the iterator
 * `__chase__` is only there to not break on the first loop
 * (this is to emulate some kind of do-while loop)
 * __chase__ starts at h->root and increments **AFTER** each test
 * thus __chase__ == h->root only the first time (or after an integer overflow)
 * because of this last caveat, h cannot have more than the pointer size roots (which will not append)
 */
#define foreachroot_ex(h, it, extra) \
  for (fheap_node* it = (h)->root,* __chase__ = (h)->root; it != (h)->root || (__chase__)++ == (h)->root; it = it->next, extra)

#define foreachroot(h, it) foreachroot_ex((h), it, (void)it)

void fheap_init(fheap* h);
uint64_t fheap_size(fheap* h);
bool fheap_is_empty(fheap* h);
void fheap_clear(fheap* h);
fheap_node* find_min(fheap* h);
void fheap_duplicate(fheap* rop, fheap* op);
callback_ret fheap_merge(fheap* rop, fheap* op);
void fheap_insert(fheap *rop, double key, uint64_t val);
key_val_pair fheap_extract_min(fheap* h);

#endif // __FHEAP__
