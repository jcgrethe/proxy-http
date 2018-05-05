#include <stdio.h>
#include <stdlib.h>

#include "include/http_parser.h"

int main(int argc, char **argv)
{
    struct http_parser *parser;
    parser = http_parser_init();

    FILE *fp = argc > 1 ? fopen(argv[1], "r") : stdin;

    if (!fp)
    {
        fprintf(stderr, "Error: file open failed for '%s'.\n", argv[1]);
        return -1;
    }

    int error = http_parser_parse(parser, fp);

    if (error != 0)
    {
        return error;
    }

    http_parser_print_information(parser);

    http_parser_free(parser);

    return 0;
}
