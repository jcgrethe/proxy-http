#ifndef httpNIO_H_whVj9DjZzFKtzEUtC0Ma2Ae45Hm
#define httpNIO_H_whVj9DjZzFKtzEUtC0Ma2Ae45Hm

#include <netdb.h>
#include "selector.h"

/** handler del socket pasivo que atiende conexiones socksv5 */
void
http_passive_accept(struct selector_key *key);


/** libera pools internos */
void
http_pool_destroy(void);

#endif
