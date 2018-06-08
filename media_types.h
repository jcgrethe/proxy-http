#include <regex.h>

#define MAXQRANGES 5
#define RANGEMAXLENGTH 15
#define RANGETYPEMAXLENGTH 10
#define LINEMAXLENGTH 50
#define REGEXMAXLENGTH 20

struct media_types{
    char ** ranges;
    int rangesi;
    regex_t * regexCompilersArray;
};

int validate_media_type(const char * line, struct media_types * s);
struct media_types * parse_media_types(char * arg);
int validateRanges(char ** ranges);
regex_t * createRegexCompilersArray(char ** ranges, const int rangesi);
int checkMalformed(const char * line);
