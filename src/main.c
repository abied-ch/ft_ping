#include <stdio.h>
#include <stdlib.h>

int main(int ac, char **av) {
    if (ac < 2) {
        fprintf(stderr, "ft_ping: usage error: Destination address required\n");
        return EXIT_FAILURE;
    }
    (void)av;
    return 0;
}
