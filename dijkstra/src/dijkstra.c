#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "fheap.h"
#include "dijkstra.h"

/*
 * rop MUST be an array of doubles of size at least G.n
 */
void dijkstra(int64_t *pred, double *d, graph G, uint64_t x0)
{
  // assosiates to the key of "distance from x to x0" as a double, the value x
  fheap h = {0};
  fheap_init(&h);
  fheap_insert(&h, 0, x0);

  // BOOL_ARRAY_INIT(visited, G.n);
  uint8_t *visited = calloc(G.n, sizeof(uint8_t));

  for (size_t i = 0; i < G.n; ++i)
    d[i] = INFINITY;
  d[x0] = 0;

  memset(pred, -1, G.n * sizeof(int64_t));
  pred[x0] = x0;

  key_val_pair node;
  while (h.n > 0)
  {
    node = fheap_extract_min(&h);
    const uint64_t x = node.val;
    const double dx = node.key;
    if (visited[x])
      continue;

    for (uint32_t i = 0; i < G.neighbour[x].neighbour_count; ++i)
    {
      const uint64_t y = G.neighbour[x].neighbour[i];
      const double dxy = G.neighbour[x].weight[i];
      if (dx + dxy < d[y])
      {
        d[y] = dx + dxy;
        pred[y] = x;
        fheap_insert(&h, d[y], y);
      }
    }

    visited[x] = 1;
  }

  free(visited);

  fheap_clear(&h);

  return;
}

void graph_free(graph *G)
{
  for (uint64_t i = 0; i < G->n; ++i)
  {
    free(G->neighbour[i].neighbour);
    free(G->neighbour[i].weight);
  }

  free(G->neighbour);
  G->n = 0;
}
