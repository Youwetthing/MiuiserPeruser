#include "ipc_globals.h"
#include <unistd.h>
extern int miuiserperuser_ipc_init(void);
int main() { miuiserperuser_ipc_init(); while(g_running){ pause(); } return 0; }
