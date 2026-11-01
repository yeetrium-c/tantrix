#ifndef __DIJKSTRA__
#define __DIJKSTRA__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "fheap.h"

typedef struct
{
  uint64_t *neighbour;
  double *weight;
  uint64_t neighbour_count;
} neighbour_array;

typedef struct
{
  neighbour_array *neighbour;
  uint64_t n;
} graph;

#define BOOL_ARRAY_INIT(name, len) uint8_t *name = calloc((len) / __CHAR_BIT__, sizeof(uint8_t))
#define BOOL_ARRAY_GET(xs, i) (((xs)[(i) / __CHAR_BIT__] >> ((i) % __CHAR_BIT__)) & 0x1)
#define BOOl_ARRAY_SET(xs, i) ((xs)[(i) / __CHAR_BIT__] |= (0x1 << ((i) % __CHAR_BIT__)))
#define BOOl_ARRAY_CLS(xs, i) ((xs)[(i) / __CHAR_BIT__] &= ~(0x1 << ((i) % __CHAR_BIT__)))

void dijkstra(int64_t *pred, double *d, graph G, uint64_t x0);
void graph_free(graph *G);

#endif // __DIJKSTRA__