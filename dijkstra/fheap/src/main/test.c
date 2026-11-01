#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <time.h>

#include "nob.h"

#include "fheap.h"

#define MAX_INSERTS (1ULL << 9)

// returns true if the test passed
bool fuzz(fheap* h);

int main(int argc, char** argv)
{
  // program name
  (void) shift(argv, argc);

  uint64_t loop_count = 1;
  if (argc > 0)
    loop_count = atoi(shift(argv, argc));

  srand(time(NULL));
  // srand(69);

  fheap h = {0};
  fheap_init(&h);
  nob_log(NOB_INFO, "Successfully initted heap");

  for (uint64_t i = 0; i < loop_count; ++i)
    fuzz(&h);

  nob_log(NOB_WARNING, "Press any key to continue...");
  getchar();

  uint64_t acc = 0;
  for (; !fheap_is_empty(&h); ++acc)
    (void)fheap_extract_min(&h);

  nob_log(NOB_INFO, "Emptied the heap by performing %"PRIu64" deletes", acc);

  fheap_clear(&h);
  nob_log(NOB_INFO, "Successfully cleared heap");

  return 0;
}

double rand_double()
{
  return ((double) rand()) / RAND_MAX;
}

bool fuzz(fheap* h)
{
  const uint64_t inserts = rand() % MAX_INSERTS;

  for (uint64_t i = 0; i < inserts; ++i)
    fheap_insert(h, rand_double(), 0);

  const uint64_t deletes = rand() % (inserts + 1);

  for (uint64_t i = 0; i < deletes; ++i)
    (void)fheap_extract_min(h);

  nob_log(NOB_INFO, "Performed %"PRIu64" inserts and %"PRIu64" deletes (current heap size = %"PRIu64")", inserts, deletes, fheap_size(h));

  return true;
}

#define NOB_IMPLEMENTATION
#include "nob.h"
