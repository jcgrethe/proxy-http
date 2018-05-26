#ifndef SOCKS5NIO_H_whVj9DjZzFKtzEUtC0Ma2Ae45Hm
#define SOCKS5NIO_H_whVj9DjZzFKtzEUtC0Ma2Ae45Hm

#include <netdb.h>
#include "selector.h"

/** handler del socket pasivo que atiende conexiones socksv5 */
void
socksv5_passive_accept(struct selector_key *key);


/** libera pools internos */
void
socksv5_pool_destroy(void);

#endif
