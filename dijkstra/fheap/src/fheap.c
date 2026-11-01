#include <stdlib.h>
#include <assert.h>

#define NOB_DA_INIT_CAP (8)
#include "nob.h"

#include "fheap.h"

void fheap_init(fheap *h)
{
  h->min = NULL;
  h->root = NULL;
  h->n = 0;
  return;
}

uint64_t fheap_size(fheap* h)
{
  return h->n;
}

bool fheap_is_empty(fheap* h)
{
  return fheap_size(h) == 0;
}

callback_ret iterate_on_branch_postfix(fheap_node* root, callback f)
{
  if (root == NULL)
    return true;

  callback_ret ret = true;

  nob_da_foreach(fheap_node*, node, root)
  {
    ret &= iterate_on_branch_postfix(*node, f);
    if (!ret) return ret;
  }

  ret &= (*f)(root);

  return ret;
}

callback_ret iterate_on_fheap_postfix(fheap* h, callback f)
{
  callback_ret ret = true;

  foreachroot(h, node)
  {
    ret &= iterate_on_branch_postfix(node, f);
    if (!ret) return ret;
  }

  return ret;
}

bool clear_node(fheap_node* node)
{
  da_foreach(fheap_node*, child, node)
    free(*child);
  // free the children array
  da_free(*node);
  return true;
}

/*
 * clear is complicated because the roots are allocated by the heap but all others are childrens and responsability of their parents, in dynamic arrays
 */
void fheap_clear(fheap* h)
{
  if (h->root == NULL)
    return;

  // free the nodes at the end of each loop, AFTER the `node = node->next`
  // it will thus happend to each node, even the first and last
  foreachroot_ex(h, node, free(node->prev))
    (void)iterate_on_branch_postfix(node, clear_node);

  h->root = NULL;
  h->min  = NULL;

  return;
}

fheap_node* find_min(fheap* h)
{
  return h->min;
}

void fheap_duplicate(fheap* rop, fheap* op)
{
  rop->root = op->root;
  rop->min = op->min;
  rop->n = op->n;
  return;
}

callback_ret fheap_merge(fheap* rop, fheap* op)
{
  if (op->root == NULL)
    // nothing to be done
    return true;

  if (rop->root == NULL)
  {
    fheap_duplicate(rop, op);
    return true;
  }

  fheap_node* end1 = rop->root->prev;
  fheap_node* end2 =  op->root->prev;

  /*
   * rop->root    end1 op->root end2
   *     ^               ^
   *     # -># -># ->#   # -># ->#
   *     |----<------|   |-<-----|
   */
  end2->next = rop->root;
  /*
   * rop->root    end1 op->root end2
   *     ^               ^
   *     # -># -># ->#   # -># ->#
   *     |----<------|   |- - - -|
   *     |- - - - - - -<- - - - -|
   */
  end1->next = op->root;
  /*
   * rop->root    end1 op->root end2
   *     ^               ^
   *     # -># -># ->#  ># -># ->#
   *     |- - - - - -|   |- - - -|
   *     |- - - - - - -<- - - - -|
   */
  op->root->prev = end1;
  /*
   * rop->root    end1 op->root end2
   *     ^               ^       ^
   *     # -># -># -># -># -># ->#
   *     |- - - - - -|
   *     |- - - - - - -<- - - - -|
   */
  rop->root->prev = end2;
  /*
   * rop->root    end1 op->root end2
   *     ^               ^
   *     # -># -># -># -># -># ->#
   *     |-------------<---------|
   */

  // choose the correct min
  if (op->min->pair.key < rop->min->pair.key)
    rop->min = op->min;

  rop->n += op->n;

  return true;
}

void fheap_insert(fheap *rop, double key, uint64_t val)
{
  fheap_node *node = malloc(sizeof(fheap_node));
  node->prev = node;
  node->next = node;
  node->count = 0;
  node->capacity = 0;
  node->items = NULL;
  node->pair = (key_val_pair){.key = key, .val = val};
  fheap op = {.root = node, .min = node, .n = 1};
  fheap_merge(rop, &op);

  return;
}

/*
 * Remove root from the circular doubly-linked list of roots of h
 * Ensures h->root still points to a valid root in h
 * does not deallocate anything, just patch up around the root
 * it could be grafted onto another tree
 * DOES NOT update h->min!!
 */
void fheap_remove_root(fheap* h, fheap_node* root)
{
  /*
   *     prev   root   next
   *     ^      ^      ^
   *     # ----># ---->#
   */
  fheap_node* prev = root->prev;
  fheap_node* next = root->next;

  /*
   *     prev   root   next
   *     ^      ^      ^
   *     # - - -# ---->#
   *     |- - - > - - -|
   */
  prev->next = next;

  /*
   *     prev   root   next
   *     ^      ^      ^
   *     # - - -# - - >#
   *     |------>------|
   */
  next->prev = prev;

  /*
   *     prev   root   next
   *     ^      ^      ^
   *     #      #      #
   *     |------>------|
   */

  // if root was the only node of the heap
  if (h->n == 1)
  {
    h->root = NULL;
    return;
  }

  if (h->root == root)
    // need to find another root to attach it to
  {
    if (root->next != root)
      // there are other roots
      h->root = root->next;
    else
    {
      // root was the only root of h
      // the first of its children will become the root of the tree
      // root MUST have children because h->n > 1 and there are not other roots
      h->root = root->items[0];
      // make it a proper root
      h->root->next = h->root;
      h->root->prev = h->root;
    }
  }

  root->next = NULL;
  root->prev = NULL;

  return;
}

// appends root's children to h as roots
void fheap_append_children(fheap *h, fheap_node* root)
{
  // nothing to be done
  if (root->count == 0)
    return;

  /*
   *     h->root     end  root's children
   *     ^           ^     /-----|-----\
   *     #---->#-...>#     #     #     #
   *     |-----<-----|
   */

  fheap_node* end = h->root->prev;

  // the first child
  end->next = root->items[0];

  /*
   *     h->root     end  root's children
   *     ^           ^     /-----|-----\
   *     #---->#-...>#- - >#     # ... #
   *     |-------------<---------------|
   */

  root->items[0]->prev = end;

  /*
   *     h->root     end  root's children
   *     ^           ^     /-----|-----\
   *     #---->#-...>#---->#     # ... #
   *     |-------------<---------------|
   */

  // the last child
  h->root->prev = root->items[root->count - 1];

  /*
   *     h->root     end  root's children
   *     ^           ^     /-----|-----\
   *     #---->#-...>#     #     # ... #
   *     |- - -<- - -|                 |
   *     |- - - - - - - - - - - - - - -|
   */

  root->items[root->count - 1]->next = h->root;

  /*
   *     h->root     end  root's children
   *     ^           ^     /-----|-----\
   *     #---->#-...>#     #     # ... #
   *     |- - -<- - -|                 |
   *     |-------------<---------------|
   */

  // circularize the children of min
  da_foreach(fheap_node*, node, root)
  {
    if (node == root->items)
      continue;

    /*
     * node - 1    node
     * #           #
     */

    (*node)->prev = *(node - 1);

    /*
     * node - 1    node
     * #- - - - - -#
     */

    (*(node - 1))->next = *node;

    /*
     * node - 1    node
     * #-----------#
     */
  }

  /*
   *     h->root     end  root's children
   *     ^           ^     /-----|-----\
   *     #---->#-...>#---->#---->#-...>#
   *     |-------------<---------------|
   */
  return;
}

uint64_t root_count(fheap* h)
{
  if (h->n == 0)
    return 0;

  uint64_t acc = 0;
  foreachroot(h, node)
    ++acc ;
  return acc;
}

key_val_pair fheap_extract_min(fheap* h)
{
  if (h->min == NULL)
  {
    nob_log(NOB_ERROR, "Delete min from empty heap !!\n");
    exit(1);
  }

  // remove min and keep its children as roots

  fheap_node min = *h->min;

  fheap_remove_root(h, h->min);
  --h->n;

  fheap_append_children(h, &min);

  da_free(*h->min);
  h->min->items = NULL;
  free(h->min);

  if (h->n == 0)
    // no consolidating to be done
    return min.pair;

  // consolidate the heap

  // array of pointers
  fheap_node** A = calloc(h->n, sizeof(fheap_node*));

#define VISITED_MASK (1ULL << 63)
#define SET_VISITED(n) ((n)->capacity |= VISITED_MASK)
#define IS_VISITED(n)  ((n)->capacity & VISITED_MASK)
#define CLEAR_VISITED(n) ((n)->capacity &= ~VISITED_MASK)

  foreachroot(h, node)
  {
    if (IS_VISITED(node))
      break;
    SET_VISITED(node);
    /*
     * while we find another with the same degree, make one the child of the other
     */
    for (fheap_node *y = A[node->count]; y != NULL; y = A[node->count])
    {
      // ensure node->key <= y->key
      if (node->pair.key > y->pair.key)
      {
        fheap_node *t = y;
        y = node;
        node = t;
      }

      if (y == h->root)
        // y is no longer a root, node which becomes it parent is now the root
        h->root = node;

      fheap_remove_root(h, y);

      // empty A
      A[node->count] = NULL;

      // but store y in node
      CLEAR_VISITED(node);
      nob_da_append(node, y);
      SET_VISITED(node);

      assert(node->count <= h->n);
    }

    // We have solved the cascade of collisions

    A[node->count] = node;
  }

  free(A);

  // find the new min of h
  h->min = h->root;
  foreachroot(h, node)
  {
    CLEAR_VISITED(node);
    if (node->pair.key < h->min->pair.key)
      h->min = node;
  }

  return min.pair;
}
