#include <stdio.h>
#include <stdlib.h>

int main(int ac, char **av) {
  if (ac < 2) {
    fprintf(stderr, "Usage\n  ft_ping [options] <destination>");
    return EXIT_FAILURE;
  }
  (void)av;
  return 0;
}
