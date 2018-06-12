#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include "media_types.h"


/* Validation */
/* 1 match / -1 no match / 0 malformed line */
int validate_media_type(const char * line, struct media_types * s){
    regmatch_t * match = malloc(LINEMAXLENGTH * sizeof(regmatch_t));
    if(checkMalformed(line)) {
        free(match);
        return 0;
    }
    for (int i = 0; i < s->rangesi; ++i) {
        if(regexec(&s->regexCompilersArray[i], line, 1, match, REG_EXTENDED) == 0) {
            free(match);
            return 1;   //Match
        }
    }
    free(match);
    return -1;
}


/* Parses the media types ranges and prepare for validation in structure */
struct media_types * parse_media_types(char * arg){
    const char delim = ',';
    char * firstToken;
    char ** ranges = malloc(MAXQRANGES* sizeof(char *));
    int rangesi = 0;
    char * aux = malloc(RANGEMAXLENGTH*MAXQRANGES);
    strcpy(aux, arg);

    firstToken = strtok(aux, &delim);
    while(firstToken != NULL){
        ranges[rangesi] = malloc(RANGEMAXLENGTH* sizeof(char));
        strcpy(ranges[rangesi++], firstToken);
        firstToken = strtok(NULL, &delim);
    }
    regex_t * regexCompilersArray = createRegexCompilersArray(ranges, rangesi);
    struct media_types * s = malloc(sizeof(struct media_types));
    s->ranges = ranges;
    s->rangesi = rangesi;
    s->regexCompilersArray = regexCompilersArray;
    return s;
}


void createRegex(const char * type, const char * subtype, const char * ows, const char * ows_parameter, char * regex){
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
    if(ows != NULL && ows_parameter != NULL){
        strcat(regex, ";");
        strcat(regex, ows);
        strcat(regex, "=");
        strcat(regex, ows_parameter);
    }
}
int validateRanges(char ** ranges){
    // TODO: Validate ranges
    return 1;
}

regex_t * createRegexCompilersArray(char ** ranges, const int rangesi){
    char * type , * subtype, * ows, * ows_parameter;
    char * regex = malloc(REGEXMAXLENGTH * sizeof(char));
    const char * delim = "/",  * ows_delim = ";", * ows_parameter_delim = "=" ;
    regex_t * compiledExp = malloc((rangesi)* sizeof(regex_t));
    int compileFlags;
    for (int i = 0; i < rangesi ; i++) {
        type = strtok(ranges[i], delim);
        subtype = strtok(NULL, ows_delim);
        ows = strtok(NULL, ows_parameter_delim);
        ows_parameter = strtok(NULL, ows_parameter_delim);
        createRegex(type, subtype, ows, ows_parameter, regex);
        compileFlags = regcomp(&compiledExp[i], regex, REG_EXTENDED);
        if(compileFlags != 0)
            printf("Error creating regex comp.");
    }
    free(regex);
    return compiledExp;
}

//char * getLineFromStdin(char * buff){
////    char * buff = malloc(LINEMAXLENGTH * sizeof(char));
//    if (fgets(buff, LINEMAXLENGTH, stdin) == NULL)
//        return NULL; //No input
//    if (buff[strlen(buff)-1] != '\n') {
//        return NULL; //Too long
//    }
//    return buff;
//}
int checkMalformed(const char * line){
    int compileFlags;
    regex_t * compiled = malloc(sizeof(regex_t));
    regmatch_t * match = malloc(LINEMAXLENGTH * sizeof(regmatch_t));

    compileFlags = regcomp(compiled, "[a-zA-Z\\*]+\\/[a-zA-Z\\*]+(; [a-zA-Z]+=[a-zA-Z0-9]+)?", REG_EXTENDED);
    if(!regexec(compiled, line, 1, match, REG_EXTENDED) == 0) {
        free(compiled); free(match);
        return 1;   //Malformed
    }else {
        free(compiled); free(match);
        return 0;
    }
}


//int main(int argc, char ** argv){
//    const char delim = ',';
//    char * firstToken, * line = malloc(LINEMAXLENGTH * sizeof(char));
//    char ** ranges = malloc(MAXQRANGES* sizeof(char *));
//    int rangesi = 0;
//
//    firstToken = strtok(argv[1], &delim);
//    while(firstToken != NULL){
//        ranges[rangesi] = malloc(RANGEMAXLENGTH* sizeof(char));
//        strcpy(ranges[rangesi++], firstToken);
//        firstToken = strtok(NULL, &delim);
//    } //Ranges get okay
//    if(!validateRanges(ranges)){
//        fputs("Invalid Ranges - Quiting program.", stdout);
//        exit(0);
//    }
//    regex_t * regexCompilersArray = createRegexCompilersArray(ranges, rangesi);
//    while(strcmp((line = getLineFromStdin(line)),"\n") != 0){
//        switch (validateLine(line, regexCompilersArray, rangesi)){
//            case 1:     fputs("true\n", stdout);break;
//            case -1:    fputs("false\n", stdout);break;
//            default:    fputs("null\n", stdout);break;
//        }
//    }
//    for (int i = 0; i <rangesi; ++i) {
//        free(ranges[i]);
//    }
//    free(firstToken); free(line); free(ranges); free(regexCompilersArray);
//    return 1;
//}
