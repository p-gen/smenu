#include <stdio.h>

#include "index.h"

int
main(int argc, char **argv)
{
  tst_node_t *root = NULL;
  int         i;

  if (argc < 2)
  {
    fprintf(stderr, "usage: %s WORD...\n", argv[0]);
    return 1;
  }

  for (i = 1; i < argc; i++)
    root = tst_insert(root, argv[i], argv[i]);

  tst_pretty_print(root);

  for (i = 1; i < argc; i++)
    printf("search %s: %s\n",
           argv[i],
           tst_search(root, argv[i]) != NULL ? "found" : "not found");

  tst_cleanup(root);

  return 0;
}
