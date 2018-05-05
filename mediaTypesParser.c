#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#define MAXQRANGES 5
#define RANGEMAXLENGTH 15
#define RANGETYPEMAXLENGTH 10
#define LINEMAXLENGTH 50
#define REGEXMAXLENGTH 20


void createRegex(const char * type, const char * subtype, char * regex){
    if(strcmp(type, "*") == 0)
        strcpy(regex,"[a-zA-Z]+\\/[a-zA-Z]+");
    else if(strcmp(subtype, "*") == 0){
        strcpy(regex, type);
        strcat(regex, "\\/[a-zA-Z]+");
    }
    else{
        strcpy(regex, type);
        strcat(regex, "\\/");
        strcat(regex, subtype);
    }
}
int validateRanges(char ** ranges){
    // TODO: Validate ranges
    return 1;
}

regex_t * createRegexCompilersArray(char ** ranges, const int rangesi){
    char * type , * subtype;
    char * regex = malloc(REGEXMAXLENGTH * sizeof(char));
    const char * delim = "/";
    regex_t * compiledExp = malloc((rangesi)* sizeof(regex_t));
    int compileFlags;
    for (int i = 0; i < rangesi ; i++) {
        type = strtok(ranges[i], delim);
        subtype = strtok(NULL, delim);
        createRegex(type, subtype, regex);
        compileFlags = regcomp(&compiledExp[i], regex, REG_EXTENDED);
        if(compileFlags != 0)
            fputs("Error during creating regex compiler: ", stdout);
    }
    free(regex);
    return compiledExp;
}

char * getLineFromStdin(char * buff){
//    char * buff = malloc(LINEMAXLENGTH * sizeof(char));
    if (fgets(buff, LINEMAXLENGTH, stdin) == NULL)
        return NULL; //No input
    if (buff[strlen(buff)-1] != '\n') {
        return NULL; //Too long
    }
    return buff;
}
int checkMalformed(const char * line){
    int compileFlags;
    regex_t * compiled = malloc(sizeof(regex_t));
    regmatch_t * match = malloc(LINEMAXLENGTH * sizeof(regmatch_t));
    compileFlags = regcomp(compiled, "[a-zA-Z]+\\/[a-zA-Z]+(; [a-zA-Z]+=[a-zA-Z0-9]+)?", REG_EXTENDED);
    if(!regexec(compiled, line, 1, match, REG_EXTENDED) == 0) {
        free(compiled); free(match);
        return 1;   //Malformed
    }else {
        free(compiled); free(match);
        return 0;
    }
}
int validateLine(const char * line, const regex_t * regexCompilersArray, const int rangesi){
    // 1 match / -1 no match / 0 malformed line
    regmatch_t * match = malloc(LINEMAXLENGTH * sizeof(regmatch_t));
    if(checkMalformed(line)) {
        free(match);
        return 0;
    }
    for (int i = 0; i < rangesi; ++i) {
        if(regexec(&regexCompilersArray[i], line, 1, match, REG_EXTENDED) == 0) {
            free(match);
            return 1;   //Match
        }
    }
    free(match);
    return -1;
}
int main(int argc, char ** argv){
    const char delim = ',';
    char * firstToken, * line = malloc(LINEMAXLENGTH * sizeof(char));
    char ** ranges = malloc(MAXQRANGES* sizeof(char *));
    int rangesi = 0;

    firstToken = strtok(argv[1], &delim);
    while(firstToken != NULL){
        ranges[rangesi] = malloc(RANGEMAXLENGTH* sizeof(char));
        strcpy(ranges[rangesi++], firstToken);
        firstToken = strtok(NULL, &delim);
    } //Ranges get okay
    if(!validateRanges(ranges)){
        fputs("Invalid Ranges - Quiting program.", stdout);
        exit(0);
    }
    regex_t * regexCompilersArray = createRegexCompilersArray(ranges, rangesi);
    while(strcmp((line = getLineFromStdin(line)),"\n") != 0){
        switch (validateLine(line, regexCompilersArray, rangesi)){
            case 1:     fputs("true\n", stdout);break;
            case -1:    fputs("false\n", stdout);break;
            default:    fputs("null\n", stdout);break;
        }
    }
    for (int i = 0; i <rangesi; ++i) {
        free(ranges[i]);
    }
    free(firstToken); free(line); free(ranges); free(regexCompilersArray);
    return 1;
}
