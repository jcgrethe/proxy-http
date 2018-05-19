//  ----------- Font Colors -------------
// Use colors like printf("This is " ECOLOR "red" RESET);
#define ECOLOR   "\x1B[31m"    // For errors
#define SCOLOR   "\x1B[32m"    // For success
#define WCOLOR   "\x1B[33m"    // For warnings
#define ICOLOR   "\x1B[35m"    // For informations
#define TCOLOR   "\x1B[34m"    // For titles
#define STCOLOR   "\x1B[36m"   // For subtitles
#define RESETCOLOR "\x1B[0m"   // Return to default color

// ----------- Titles Templates -----------
// Use colors like printf(TPREFIX " my-title " TSUFIX);

#define EPREFIX "[E: "      // For errors
#define ESUFIX " ]"
#define SPREFIX "[S: "      // For success
#define SSUFIX " ]"
#define WPREFIX "[W: "      // For warnings
#define WSUFIX " ]"
#define IPREFIX "[I: "      // For informations
#define ISUFIX " ]"
#define TPREFIX "<{---- "   // For titles
#define TSUFIX " ----}>"
#define STPREFIX "-_-_-_-"  // For subtitles
#define STSUFIX "-_-_-_-"

// Examples:
//    printf(SCOLOR SPREFIX " Succes using styles! " SSUFIX RESETCOLOR"\n");
//    printf(ECOLOR EPREFIX " Error using styles! " ESUFIX RESETCOLOR"\n");
//    printf(ICOLOR IPREFIX " Info using styles! " ISUFIX RESETCOLOR"\n");
//    printf(WCOLOR WPREFIX " Warning using styles! " WSUFIX RESETCOLOR"\n");
//    printf(TCOLOR TPREFIX " Title using styles! " TSUFIX RESETCOLOR"\n");
//    printf(STCOLOR STPREFIX " Subtitle using styles! " STSUFIX RESETCOLOR"\n");