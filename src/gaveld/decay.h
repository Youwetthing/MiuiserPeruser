#ifndef GAVELD_DECAY_H
#define GAVELD_DECAY_H

/*
 * decay.h — score decay ticker
 * Ported from scored.c decay_tick() and decay_thread().
 * Runs as an independent pthread, fires every DECAY_INTERVAL_SEC.
 * Skips sources with active signals in the SIGNAL_WINDOW_SEC window.
 * Respects internal_affairs.lock — skips entire tick if lock present.
 */

/* Start the decay thread — call once from gaveld.c after db_open() */
int  decay_start(void);

/* Signal the decay thread to stop and join it */
void decay_stop(void);

/* Run one decay tick immediately — for testing / manual trigger */
void decay_tick(void);

#endif /* GAVELD_DECAY_H */
