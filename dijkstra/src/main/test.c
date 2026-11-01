#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>

#define NOB_STRIP_PREFIX
#include <nob.h>

#include "timer.h"

#include "dijkstra.h"

void graph_from_path(graph *G, const char *const fpath);

int main(int argc, char **argv)
{
  (void)argc, (void)argv;

  timer time;
  timer_start(&time);

  graph G = {0};
  graph_from_path(&G, "inputs/NA.txt");

  printf("%"PRIu64"\n", G.n);
  printf("loading the file took: %lfs\n", timer_stop(&time));

  timer_start(&time);
  double *d = calloc(G.n, sizeof(double));
  int64_t *pred = calloc(G.n, sizeof(int64_t));

  const uint64_t x0 = 42012;
  const uint64_t xf = 9;
  dijkstra(pred, d, G, x0);

  printf("%lf\n", d[xf]);
  printf("dijkstra took: %lfs\n", timer_stop(&time));

  uint64_t x = xf;
  do
    printf("%"PRIu64" <- ", x);
  while ((x = pred[x]) != x0);
  printf("%"PRIu64"\n", x0);

  free(d);
  free(pred);

  graph_free(&G);

  return 0;
}

void graph_from_path(graph *G, const char *const fpath)
{
  (void)G;
  FILE *f = fopen(fpath, "r");

  if (f == NULL)
  {
    nob_log(NOB_ERROR, "Could not open file %s: %s", fpath, strerror(errno));
    exit(1);
  }

  G->n = 0;

  uint64_t x = 0, y = 0;
  while (fscanf(f, "[%"PRIu64",%"PRIu64",%*f]\n", &x, &y) != EOF)
  {
    if (x > G->n)
      G->n = x;
    if (y > G->n)
      G->n = y;
  }

  ++G->n; // neighbour[max] must be valid

  G->neighbour = calloc(G->n, sizeof(*G->neighbour));

  fseek(f, 0, SEEK_SET);

  double dxy = 0;
  while (fscanf(f, "[%"PRIu64",%"PRIu64",%lf]\n", &x, &y, &dxy) != EOF)
  {
    // non-oriented graph
    ++G->neighbour[x].neighbour_count;
    ++G->neighbour[y].neighbour_count;
  }

  for (uint64_t i = 0; i < G->n; ++i)
  {
    neighbour_array *j = G->neighbour + i;
    j->neighbour = calloc(j->neighbour_count, sizeof(*j->neighbour));
    j->weight = calloc(j->neighbour_count, sizeof(*j->weight));

    // will be used as counter for the next loop, will be reset to the same value
    j->neighbour_count = 0;
  }

  fseek(f, 0, SEEK_SET);

  while (fscanf(f, "[%"PRIu64",%"PRIu64",%lf]\n", &x, &y, &dxy) != EOF)
  {
    // non-oriented graph
    G->neighbour[x].neighbour[G->neighbour[x].neighbour_count] = y;
    G->neighbour[x].weight[G->neighbour[x].neighbour_count++] = dxy;

    G->neighbour[y].neighbour[G->neighbour[y].neighbour_count] = x;
    G->neighbour[y].weight[G->neighbour[y].neighbour_count++] = dxy;
  }

  fclose(f);

  return;
}

#define __TIMER_IMPLEMENTATION__
#include "timer.h"

#define NOB_IMPLEMENTATION
#include "nob.h"

