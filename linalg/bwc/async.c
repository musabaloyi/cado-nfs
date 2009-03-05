#define _POSIX_C_SOURCE 200112L

#include <signal.h>
#include <string.h>

#include "async.h"
#include "rusage.h"
#include "macros.h"
#include "bw-common.h"
#include "manu.h"

// int int_caught = 0;
int hup_caught = 0;

void timing_update_ticks(struct timing_data * t, int iter MAYBE_UNUSED)
{
    job_seconds(t->current->job);
    thread_seconds(t->current->thread);
    t->current->wct = walltime_seconds();
}

void timing_partial_init(struct timing_data * t, int iter)
{
    t->go_mark = iter;
    t->last_print = iter;
    t->next_print = iter + 1;
    t->next_async_check = iter + 1;
    t->async_check_period = 1;

    timing_update_ticks(t, iter);
    memcpy(t->go, t->current, sizeof(*t->go));
}

void timing_init(struct timing_data * t, int iter)
{
    timing_partial_init(t, iter);
    memcpy(t->beginning, t->go, sizeof(*t->go));
    t->begin_mark = iter;
}

void timing_clear(struct timing_data * t MAYBE_UNUSED)
{
}

void timing_rare_checks(pi_wiring_ptr wr, struct timing_data * t, int iter, int print)
{
    /* We've decided that it was time to check for asynchronous data.
     * Since it's an expensive operation, the whole point is to avoid
     * doing this check too often. */
    timing_update_ticks(t, iter);

    /* First, re-evaluate the async checking period */
    double av[2];

    av[0] = (t->current->thread[0] - t->go->thread[0])/(iter - t->go_mark);
    av[1] = (t->current->thread[1] - t->go->thread[1])/(iter - t->go_mark);

    double good_period = PREFERRED_ASYNC_LAG / av[0];
    int guess = 1 + (int) good_period;
    complete_broadcast(wr, &guess, sizeof(int), 0, 0);
    /* negative stuff is most probably caused by overflows in a fast
     * context */
    if (guess <= 0) guess = 10;
    if (iter == t->go_mark + 1 && print) {
        printf("Checking for asynchronous events every %d iterations\n", guess);
    }
    t->next_async_check += guess;
    t->async_check_period = guess;

    /* Now do some possibly expensive checks */

    /* This read is unsafe. We need to protect it. */
    unsigned int caught_something = hup_caught; // || int_caught;

    /* propagate the unsafe read. */
    if (wr->trank == 0) {
        int err = MPI_Allreduce(MPI_IN_PLACE, &caught_something, 1,
                MPI_UNSIGNED, MPI_MAX, wr->pals);
        BUG_ON(err);
    }

    /* reconcile threads */
    serialize_threads(wr);      // bug in thread_agreement
    void * ptr = &caught_something;
    thread_agreement(wr, &ptr, 0);
    caught_something = * (unsigned int *) ptr;

    /* ok, got it. Before we possibly leave, make sure everybody has read
     * the data from the pointer. */
    serialize(wr);

    if (!caught_something) {
        /* And the result of all this is... nothing :-(( */
        return;
    }

    /* We have something ! */
    if (print) {
        printf("Signal caught (iteration %d), restarting timings\n", iter);
    }
    serialize(wr);
    hup_caught = 0;
    // int_caught = 0;
    timing_partial_init(t, iter);
}


void timing_check(parallelizing_info pi, struct timing_data * timing, int iter, int print)
{
    pi_wiring_ptr wr = pi->m;

    // printf("timing_check %d timing%d\n", iter, wr->trank);

    if (iter == timing->next_async_check) {
        // printf("timing_check async %d timing%d\n", iter, wr->trank);
        timing_rare_checks(wr, timing, iter, print);
    }

    if (iter < timing->next_print)
        return;

    const int period[] = {1,5,20,100,1000,10000,100000,1000000,0};
    int i;
    for(i = 0 ; ; i++) {
        if (period[i] == 0 || iter % period[i])
            break;
    }
    i--;
    ASSERT(iter % period[i] == 0);
    timing->next_print = iter + period[i];

    timing_update_ticks(timing, iter);

    char buf[20];

    double dt[2];
    double av[2];
    double pcpu[2];

    dt[0] = timing->current->thread[0] - timing->go->thread[0];
    dt[1] = timing->current->thread[1] - timing->go->thread[1];

    av[0] = dt[0] / (iter - timing->go_mark);
    av[1] = dt[1] / (iter - timing->go_mark);

    pcpu[0] = 100.0 * dt[0] / (timing->current->wct - timing->go->wct);
    pcpu[1] = 100.0 * dt[1] / (timing->current->wct - timing->go->wct);

    snprintf(buf, sizeof(buf), "%.2f %.2f %2d%%+%2d%%",
            dt[0], av[0], (int) pcpu[0], (int) pcpu[1]);

    if (print)
        printf("iteration %d\n", iter);

    grid_print(pi, buf, sizeof(buf), print);
}

void timing_disp_collective_oneline(parallelizing_info pi, struct timing_data * timing, int iter, int print)
{
    double oncpu[2];
    double dt[2];
    double av[2];
    double pcpu[2];

    timing_update_ticks(timing, iter);

    oncpu[0] = timing->current->job[0] - timing->beginning->job[0];
    oncpu[1] = timing->current->job[1] - timing->beginning->job[1];

    double wct = timing->current->wct - timing->beginning->wct;

    dt[0] = timing->current->job[0]-timing->go->job[0];
    dt[1] = timing->current->job[1]-timing->go->job[1];
    // dt must be collected.
    int err;
    err = MPI_Allreduce(MPI_IN_PLACE, oncpu, 2, MPI_DOUBLE, MPI_SUM, pi->m->pals);
    BUG_ON(err);
    err = MPI_Allreduce(MPI_IN_PLACE, dt, 2, MPI_DOUBLE, MPI_SUM, pi->m->pals);
    BUG_ON(err);
    double di = iter - timing->go_mark;

    av[0] = dt[0] / di;
    av[1] = dt[1] / di;
    pcpu[0] = 100.0 * dt[0] / (double) (timing->current->wct - timing->go->wct);
    pcpu[1] = 100.0 * dt[1] / (double) (timing->current->wct - timing->go->wct);

    double avsum = av[0] + av[1];
    char * what = "s";
    if (avsum < 0.1) {
        what = "ms";
        avsum *= 1000.0;
    }

    if (print) {
        printf("N=%d ; %.2f cpu, %.2f sys, %.2f wct"
                " %.2f %s/iter, %.2f%% cpu + %.2f%% sys\n",
                iter, oncpu[0], oncpu[1], wct, avsum, what, pcpu[0], pcpu[1]);
    }
}


void block_control_signals()
{
    /* Only the master thread receives control signals */
    sigset_t sset[1];
    sigemptyset(sset);
    sigaddset(sset, SIGHUP);
    // sigaddset(sset, SIGINT);
    pthread_sigmask(SIG_BLOCK, sset, NULL);
}

void sighandler(int sig)
{
    int caught = 0;
    if (sig == SIGHUP) hup_caught = caught = 1;
    // if (sig == SIGINT) int_caught = caught = 1;
    /* Of course, everybody is allowed to print this. */
    if (caught) printf("Signal caught, wait before it is acknowledged\n");
}

void catch_control_signals()
{
    struct sigaction sa[1];
    memset(sa, 0, sizeof(sa));
    sa->sa_handler = sighandler;
    sigaction(SIGHUP, sa, NULL);
    // sigaction(SIGINT, sa, NULL);

    sigset_t sset[1];
    sigemptyset(sset);
    sigaddset(sset, SIGHUP);
    // sigaddset(sset, SIGINT);
    pthread_sigmask(SIG_UNBLOCK, sset, NULL);
}
