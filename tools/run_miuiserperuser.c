#include "daemon_core.h"
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    return miuiserperuser_main_loop(true);
}
