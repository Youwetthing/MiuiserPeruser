#ifndef PORT_PATHWAY_H
#define PORT_PATHWAY_H

#include <stdbool.h>

#define SENSEI_PORT_BRIDGE 6789

/* Probe the port bridge and return true if it responds. */
bool probe_port_bridge(void);

/* Optional: perform a simple info request over the bridge. */
bool port_bridge_request_basic_info(void);

#endif
