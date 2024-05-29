/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <error.h>
#include <pthread.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include "dlb.h"

static dlb_dev_cap_t cap;
static int dev_id;
static uint64_t num_events;
static int num_workers;
static int num_credit_combined;
static int num_credit_ldb;
static int num_credit_dir;
static bool use_max_credit_combined = true;
static bool use_max_credit_ldb = true;
static bool use_max_credit_dir = true;
static int partial_resources = 100;
static dlb_resources_t rsrcs;
/* Temporary variable for LM test. Will remove later */
static int live_migration_pause = 0;

#define NUM_EVENTS_PER_LOOP 4
#define RETRY_LIMIT 1000000000

#define EPOLL_SIZE 256
#define EPOLL_RETRY 10

static bool epoll_enabled = false;
static uint64_t ticks = 2000; /* 2 sec*/

enum wait_mode_t {
    POLL,
    INTERRUPT,
} wait_mode = INTERRUPT;

static int print_resources(
    dlb_resources_t *rsrcs)
{
    printf("DLB's available resources:\n");
    printf("\tDomains:           %d\n", rsrcs->num_sched_domains);
    printf("\tLDB queues:        %d\n", rsrcs->num_ldb_queues);
    printf("\tLDB ports:         %d\n", rsrcs->num_ldb_ports);
    printf("\tDIR ports:         %d\n", rsrcs->num_dir_ports);
    printf("\tSN slots:          %d,%d\n", rsrcs->num_sn_slots[0],
	   rsrcs->num_sn_slots[1]);
    printf("\tES entries:        %d\n", rsrcs->num_ldb_event_state_entries);
    printf("\tContig ES entries: %d\n",
           rsrcs->max_contiguous_ldb_event_state_entries);
    if (!cap.combined_credits) {
        printf("\tLDB credits:       %d\n", rsrcs->num_ldb_credits);
        printf("\tContig LDB cred:   %d\n", rsrcs->max_contiguous_ldb_credits);
        printf("\tDIR credits:       %d\n", rsrcs->num_dir_credits);
        printf("\tContig DIR cred:   %d\n", rsrcs->max_contiguous_dir_credits);
        printf("\tLDB credit pls:    %d\n", rsrcs->num_ldb_credit_pools);
        printf("\tDIR credit pls:    %d\n", rsrcs->num_dir_credit_pools);
    } else {
        printf("\tCredits:           %d\n", rsrcs->num_credits);
        printf("\tCredit pools:      %d\n", rsrcs->num_credit_pools);
    }
    printf("\n");

    return 0;
}

#define CQ_DEPTH 8

static int create_sched_domain(
    dlb_hdl_t dlb)
{
    int p_rsrsc = partial_resources;
    dlb_create_sched_domain_t args;

    args.num_ldb_queues = 1 + (num_workers > 0);
    args.num_ldb_ports = 2 + num_workers;
    args.num_dir_ports = 0;
    args.num_ldb_event_state_entries = 2 * args.num_ldb_ports * CQ_DEPTH;
    if (!cap.combined_credits) {
        args.num_ldb_credits = rsrcs.max_contiguous_ldb_credits * p_rsrsc / 100;
        args.num_dir_credits = rsrcs.max_contiguous_dir_credits * p_rsrsc / 100;
        args.num_ldb_credit_pools = 1;
        args.num_dir_credit_pools = 1;
    } else {
        args.num_credits = rsrcs.num_credits * p_rsrsc / 100;
        args.num_credit_pools = 1;
    }

    args.num_sn_slots[0] = rsrcs.num_sn_slots[0] * p_rsrsc / 100;
    args.num_sn_slots[1] = rsrcs.num_sn_slots[1] * p_rsrsc / 100;

    return dlb_create_sched_domain(dlb, &args);
}

static int create_ldb_queue(
    dlb_domain_hdl_t domain,
    int num_seq_numbers)
{
    dlb_create_ldb_queue_t args = {0};

    args.num_sequence_numbers = num_seq_numbers;

    return dlb_create_ldb_queue(domain, &args);
}

static int create_ldb_port(
    dlb_domain_hdl_t domain,
    int ldb_pool,
    int dir_pool)
{
    dlb_create_port_t args;

    if (!cap.combined_credits) {
        args.ldb_credit_pool_id = ldb_pool;
        args.dir_credit_pool_id = dir_pool;
    } else {
        args.credit_pool_id = ldb_pool;
    }
    args.cq_depth = CQ_DEPTH;
    args.num_ldb_event_state_entries = CQ_DEPTH*2;
    args.cos_id = DLB_PORT_COS_ID_ANY;

    return dlb_create_ldb_port(domain, &args);
}

typedef struct {
    dlb_port_hdl_t port;
    int queue_id;
    int efd;
} thread_args_t;

static void *tx_traffic(void *__args)
{
    thread_args_t *args = (thread_args_t *) __args;
    dlb_event_t events[NUM_EVENTS_PER_LOOP];
    int64_t num_loops, i;
    int ret;

    num_loops = (num_events == 0) ? -1 : num_events / NUM_EVENTS_PER_LOOP;

    /* Initialize the static fields in the send events */
    for (i = 0; i < NUM_EVENTS_PER_LOOP; i++) {
        events[i].send.queue_id = args->queue_id;
        if (num_workers)
            events[i].send.sched_type = SCHED_ORDERED;
        else
            events[i].send.sched_type = SCHED_ATOMIC;
        events[i].send.priority = 0;
        events[i].send.flow_id = 0xABCD;
    }

    for (i = 0; i < num_loops || num_loops == -1; i++) {
        int j, num;

        /* Initialize the dynamic fields in the send events */
        for (j = 0; j < NUM_EVENTS_PER_LOOP; j++) {
            events[j].adv_send.udata64 = i*100 + j;
            events[j].adv_send.udata16 = ~(i + j);
        }

        /* Send the events */
        ret = 0;
        num = 0;
        for (j = 0; num != NUM_EVENTS_PER_LOOP && j < RETRY_LIMIT; j++) {
            ret = dlb_send(args->port, NUM_EVENTS_PER_LOOP-num, &events[num]);

            if (ret == -1)
                break;

            num += ret;
            if ((j != 0) && (j % 10000000 == 0))
                printf("[%s()] TIMEOUT: Tx blocked for %d iterations\n", __func__, j);
        }

        if (num != NUM_EVENTS_PER_LOOP) {
            printf("[%s()] FAILED: Sent %d/%d events on iteration %ld!\n",
                   __func__, num, NUM_EVENTS_PER_LOOP, i);
            exit(-1);
        }

    }

    printf("[%s()] Sent %ld events\n",
           __func__, num_loops * NUM_EVENTS_PER_LOOP);

    return NULL;
}

static volatile bool worker_done;

/* Create eventfd per port and map the eventfd to the port
 * using dlb_enable_cq_epoll() API. Create epoll instance
 * and register the eventfd to monitor for events.
 */
static int setup_epoll(thread_args_t *args) {
    struct epoll_event ev;
    int epoll_fd;

    args->efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (args->efd < 0)
        error(1, errno, "eventfd error");
    if (dlb_enable_cq_epoll(args->port, true, args->efd))
        error(1, errno, "dlb_enable_cq_epoll");

    epoll_fd = epoll_create(EPOLL_SIZE);
    if( epoll_fd < 0 )
        error(1, errno, "epoll_create failed");

    ev.data.fd = args->efd;
    ev.events  = EPOLLIN;
    if( epoll_ctl(epoll_fd, EPOLL_CTL_ADD, args->efd, &ev ) ) {
        close(epoll_fd);
        error(1, errno, "Failed to add file descriptor to epoll");
    }
    return epoll_fd;
}

static void *rx_traffic(void *__args)
{
    thread_args_t *args = (thread_args_t *) __args;
    dlb_event_t events[NUM_EVENTS_PER_LOOP];
    struct epoll_event epoll_events;
    int epoll_retry = EPOLL_RETRY;
    int ret, nfds = 0, epoll_fd;
    int64_t num_loops, i;

    if (epoll_enabled)
        epoll_fd = setup_epoll(args);

    num_loops = (num_events == 0) ? -1 : num_events / NUM_EVENTS_PER_LOOP;

    for (i = 0; i < num_loops || num_loops == -1; i++) {
        int j, num = 0;

	if (epoll_enabled) {
	    while (nfds == 0 && epoll_retry-- > 0) {
	        nfds = epoll_wait(epoll_fd, &epoll_events, 1, ticks);
                if (nfds < 0) {
                    printf("[%s()] FAILED: epoll_wait", __func__);
                    goto end;
                }
            }
            if (nfds == 0 && epoll_retry == 0) {
                printf("[%s()] TIMEOUT: No eventfd ready in %ld msec. Exiting.\n",
                           __func__, ticks * EPOLL_RETRY);
                goto end;
            }
        }

	/* Receive the events */
        for (j = 0; (num != NUM_EVENTS_PER_LOOP && j < RETRY_LIMIT); j++) {
            ret = dlb_recv(args->port,
                           NUM_EVENTS_PER_LOOP-num,
                           (wait_mode == INTERRUPT),
                           &events[num]);

            if (ret == -1)
                break;

            num += ret;
            if ((j != 0) && (j % 10000000 == 0))
                printf("[%s()] TIMEOUT: Rx blocked for %d iterations\n", __func__, j);
        }

        if (num != NUM_EVENTS_PER_LOOP) {
            printf("[%s()] FAILED: Recv'ed %d events (iter %ld)!\n", __func__, num, i);
            exit(-1);
        }

        /* Validate the events */
        for (j = 0; j < NUM_EVENTS_PER_LOOP; j++) {
            if (events[j].recv.udata64 != (i*100 + j) ||
                events[j].recv.queue_id != args->queue_id ||
                events[j].recv.udata16 != (uint16_t) ~(i + j) ||
                (cap.ldb_deq_event_fid && events[j].recv.flow_id != 0xABCD) ||

                events[j].recv.error) {
                printf("[%s()] FAILED: Bug in received event %ld,%d\n", __func__, i,j);
                exit(-1);
            }
        }

        if (dlb_release(args->port, num) != num) {
            printf("[%s()] FAILED: Release of all %d events (iter %ld)!\n",
                   __func__, num, i);
            exit(-1);
        }
    }

    printf("[%s()] Received %ld events\n",
           __func__, num_loops * NUM_EVENTS_PER_LOOP);

    worker_done = true;

end:
    if (epoll_enabled) {
        close(epoll_fd);
	close(args->efd);
    }

    return NULL;
}

static void *worker_fn(void *__args)
{
    thread_args_t *args = (thread_args_t *) __args;
    int epoll_fd, epoll_retry = EPOLL_RETRY;
    struct epoll_event epoll_events;
    int i, j, ret, total = 0;
    int nfds = 0;

    if (epoll_enabled)
        epoll_fd = setup_epoll(args);

    for (i = 0; !worker_done; i++) {
        dlb_event_t events[NUM_EVENTS_PER_LOOP];
        int j, num_tx, num_rx;

	if (epoll_enabled) {
	    while (nfds == 0 && epoll_retry-- > 0) {
                nfds = epoll_wait(epoll_fd, &epoll_events, 1, ticks);
	        if (worker_done)
                    goto end;
                else if (nfds < 0) {
                    printf("[%s()] FAILED: epoll_wait", __func__);
                    goto end;
                }
            }
            if (nfds == 0 && epoll_retry == 0) {
	        printf("[%s()] TIMEOUT: No eventfd ready in %ld msec. Exiting.\n",
                           __func__, ticks * EPOLL_RETRY);
                goto end;
	    }
	}

        /* Receive the events */
        for (j = 0, num_rx = 0; num_rx == 0 && j < RETRY_LIMIT; j++) {
            num_rx = dlb_recv(args->port,
                              NUM_EVENTS_PER_LOOP,
                              (wait_mode == INTERRUPT),
                              events);

            if ((j != 0) && (j % 10000000 == 0))
                printf("[%s()] TIMEOUT: Worker blocked for %d iterations\n", __func__, j);
        }

        /* The port was disabled, indicating the thread should return */
        if (num_rx == -1 && errno == EACCES)
            break;

        total += num_rx;

        /* Validate the events */
        for (j = 0; j < num_rx; j++) {
            if (events[j].recv.error) {
                printf("[%s()] FAILED: Bug in received event %d,%d\n", __func__, i, j);
                exit(-1);
            }
        }

        for (j = 0; j < num_rx; j++) {
            events[j].send.queue_id = args->queue_id;
            events[j].send.sched_type = SCHED_ATOMIC;
            events[j].send.flow_id = 0xABCD;
        }

        for (j = 0, num_tx = 0; num_tx < num_rx && j < RETRY_LIMIT; j++) {
            ret = dlb_forward(args->port, num_rx-num_tx, &events[num_tx]);

            if (ret == -1)
                break;

            num_tx += ret;
        }

        if (num_tx != num_rx) {
            printf("[%s()] Forwarded %d/%d events on iteration %d!\n",
                   __func__, num_tx, num_rx, i);
            exit(-1);
        }
    }
    printf("[%s()] Received %d events\n",
           __func__, total);

end:
    if (epoll_enabled) {
        close(epoll_fd);
        close(args->efd);
    }
    return NULL;
}

static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},	
    {"num-events", required_argument, 0, 'n'},
    {"dev-id", required_argument, 0, 'd'},
    {"wait-mode", required_argument, 0, 'w'},
    {"num-workers", required_argument, 0, 'f'},
    {"partial_resources", required_argument, 0, 'p'},
    {"num-credit-combined", required_argument, 0, 'c'},
    {"num-credit-ldb", required_argument, 0, 'l'},
    {"num-credit-dir", required_argument, 0, 'e'},
    {"live-migration-pause", required_argument, 0, 'm'},
    {0, 0, 0, 0}
};

static void
usage(void)
{
    const char *usage_str =
        "  Usage: traffic [options]\n"
        "  Options:\n"
	"  -h, --help             Prints all the available options\n"
        "  -n, --num-events=N     Number of looped events (0: infinite) (default: 0)\n"
        "  -d, --dev-id=N         Device ID (default: 0)\n"
        "  -w, --wait-mode=<str>  Options: 'poll', 'interrupt', 'epoll' (default: interrupt)\n"
        "  -f, --num-workers=N    Number of 'worker' threads that forward events (default: 0)\n"
        "  -p, --partial_resources=N    Partial HW resources in percentage (default: 100)\n"
        "  -c, --num-credit-combined=N   Number of combined SW credits (default: combined HW credits\n"
        "  -l, --num-credit-ldb=N    Number of ldb SW credits (default: HW ldb credits)\n"
        "  -e, --num-credit-dir=N    Number of dir SW credits (default: HW dir credits)\n"
        "  -m, --live-migration-pause=N Add N seconds pause before traffic for live migration test\n"
        "\n";
    fprintf(stderr, "%s", usage_str);
    exit(1);
}

int parse_args(int argc, char **argv)
{
    int option_index, c;

    for (;;) {
        c = getopt_long(argc, argv, "n:d:w:f:hc:l:e:p:m:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'n':
                num_events = (uint64_t) atol(optarg);
                break;
            case 'd':
                dev_id = atoi(optarg);
                break;
            case 'w':
                if (strncmp(optarg, "poll", sizeof("poll")) == 0)
                    wait_mode = POLL;
                else if (strncmp(optarg, "interrupt", sizeof("interrupt")) == 0)
                    wait_mode = INTERRUPT;
		else if (strncmp(optarg, "epoll", sizeof("epoll")) == 0) {
		    epoll_enabled = true;
		    wait_mode = POLL;
		}
                else
                    usage();
                break;
            case 'f':
                num_workers = atoi(optarg);
                break;
            case 'c':
		num_credit_combined = atoi(optarg);
		use_max_credit_combined = false;
                break;
            case 'p':
		partial_resources = atoi(optarg);
                break;
            case 'l':
		num_credit_ldb = atoi(optarg);
		use_max_credit_ldb = false;
                break;
            case 'e':
		num_credit_dir = atoi(optarg);
		use_max_credit_dir = false;
                break;
            case 'm':
                live_migration_pause = atoi(optarg);
                break;
	    case 'h':
		usage();
		break;
            default:
                usage();
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    pthread_t rx_thread, tx_thread, *worker_threads;
    thread_args_t rx_args, tx_args, *worker_args;
    int ldb_pool_id, dir_pool_id;
    int rx_port_id, tx_port_id;
    dlb_domain_hdl_t domain;
    int num_seq_numbers;
    int worker_queue_id;
    int domain_id, i;
    dlb_hdl_t dlb;

    if (parse_args(argc, argv))
        return -1;

    worker_threads = (pthread_t *)malloc(sizeof(pthread_t) * num_workers);
    if (!worker_threads)
        return -ENOMEM;

    worker_args = (thread_args_t *)malloc(sizeof(thread_args_t) * num_workers);
    if (!worker_args) {
        free(worker_threads);
        return -ENOMEM;
    }

    if (dlb_open(dev_id, &dlb) == -1)
        error(1, errno, "dlb_open");

    if (dlb_get_dev_capabilities(dlb, &cap))
        error(1, errno, "dlb_get_dev_capabilities");

    if (dlb_get_num_resources(dlb, &rsrcs))
        error(1, errno, "dlb_get_num_resources");

    if (print_resources(&rsrcs))
        error(1, errno, "print_resources");

    if (dlb_get_ldb_sequence_number_allocation(dlb, 0, &num_seq_numbers))
        error(1, errno, "dlb_get_ldb_sequence_number_allocation");

    domain_id = create_sched_domain(dlb);
    if (domain_id == -1)
        error(1, errno, "dlb_create_sched_domain");

    domain = dlb_attach_sched_domain(dlb, domain_id);
    if (domain == NULL)
        error(1, errno, "dlb_attach_sched_domain");

    if (!cap.combined_credits) {
	int max_ldb_credits = rsrcs.num_ldb_credits * partial_resources / 100;
	int max_dir_credits = rsrcs.num_dir_credits * partial_resources / 100;

        if (use_max_credit_ldb == true)
            ldb_pool_id = dlb_create_ldb_credit_pool(domain, max_ldb_credits);
        else
            if (num_credit_ldb <= max_ldb_credits)
                ldb_pool_id = dlb_create_ldb_credit_pool(domain,
                                                         num_credit_ldb);
            else
                error(1, EINVAL, "Requested ldb credits are unavailable!");

        if (ldb_pool_id == -1)
            error(1, errno, "dlb_create_ldb_credit_pool");

        if (use_max_credit_dir == true)
            dir_pool_id = dlb_create_dir_credit_pool(domain, max_dir_credits);
        else
            if (num_credit_dir <= max_dir_credits)
                dir_pool_id = dlb_create_dir_credit_pool(domain,
                                                         num_credit_dir);
            else
                error(1, EINVAL, "Requested dir credits are unavailable!");

        if (dir_pool_id == -1)
            error(1, errno, "dlb_create_dir_credit_pool");
    } else {
	int max_credits = rsrcs.num_credits * partial_resources / 100;

        if (use_max_credit_combined == true)
            ldb_pool_id = dlb_create_credit_pool(domain, max_credits);
        else
            if (num_credit_combined <= max_credits)
                ldb_pool_id = dlb_create_credit_pool(domain,
                                                     num_credit_combined);
            else
                error(1, EINVAL, "Requested combined credits are unavailable!");

        if (ldb_pool_id == -1)
            error(1, errno, "dlb_create_credit_pool");
    }

    tx_args.queue_id = create_ldb_queue(domain, num_seq_numbers);
    if (tx_args.queue_id == -1)
        error(1, errno, "dlb_create_ldb_queue");

    tx_port_id = create_ldb_port(domain, ldb_pool_id, dir_pool_id);
    if (tx_port_id == -1)
        error(1, errno, "dlb_create_ldb_port");

    tx_args.port = dlb_attach_ldb_port(domain, tx_port_id);
    if (tx_args.port == NULL)
        error(1, errno, "dlb_attach_ldb_port");

    rx_port_id = create_ldb_port(domain, ldb_pool_id, dir_pool_id);
    if (rx_port_id == -1)
        error(1, errno, "dlb_create_ldb_port");

    rx_args.port = dlb_attach_ldb_port(domain, rx_port_id);
    if (rx_args.port == NULL)
        error(1, errno, "dlb_attach_ldb_port");

    /* Create worker queue */
    if (num_workers) {
        worker_queue_id = create_ldb_queue(domain, 0);
        if (worker_queue_id == -1)
            error(1, errno, "dlb_create_ldb_queue");
    }

    /* Create worker ports */
    for (i = 0; i < num_workers; i++) {
        int port_id;

        port_id = create_ldb_port(domain, ldb_pool_id, dir_pool_id);
        if (port_id == -1)
            error(1, errno, "dlb_create_ldb_port");

        worker_args[i].port = dlb_attach_ldb_port(domain, port_id);
        if (worker_args[i].port == NULL)
            error(1, errno, "dlb_attach_ldb_port");

        worker_args[i].queue_id = worker_queue_id;

        if (dlb_link_queue(worker_args[i].port, tx_args.queue_id, 0) == -1)
            error(1, errno, "dlb_link_queue");
    }

    /* Link the worker queue if there any workers, else link the tx queue. */
    rx_args.queue_id = (num_workers > 0) ? worker_queue_id : tx_args.queue_id;

    if (dlb_link_queue(rx_args.port, rx_args.queue_id, 0) == -1)
        error(1, errno, "dlb_link_queue");

    if (dlb_launch_domain_alert_thread(domain, NULL, NULL))
        error(1, errno, "dlb_launch_domain_alert_thread");

    if (dlb_start_sched_domain(domain))
        error(1, errno, "dlb_start_sched_domain");

    if (live_migration_pause) {
        printf("%s: pausing %d sec for live migration.....\n", __func__, live_migration_pause);
        sleep(live_migration_pause);
        printf("%s: resuming after live migration.....\n", __func__);
    }

    /* Launch threads */
    for (i = 0; i < num_workers; i++)
        pthread_create(&worker_threads[i], NULL, worker_fn, &worker_args[i]);
    pthread_create(&rx_thread, NULL, rx_traffic, &rx_args);

    /* Add sleep here to make sure the rx_thread is staretd before tx_thread */
    usleep(1000);

    pthread_create(&tx_thread, NULL, tx_traffic, &tx_args);

    /* Wait for threads to complete */
    pthread_join(tx_thread, NULL);
    pthread_join(rx_thread, NULL);
    /* The worker threads may be blocked on the CQ interrupt wait queue, so
     * disable their ports in order to wake them before joining the thread.
     */
    for (i = 0; i < num_workers; i++) {
        if (dlb_disable_port(worker_args[i].port))
            error(1, errno, "dlb_disable_port");
        pthread_join(worker_threads[i], NULL);
    }

    for (i = 0; i < num_workers; i++) {
        if (dlb_detach_port(worker_args[i].port) == -1)
            error(1, errno, "dlb_detach_port");
    }

    if (dlb_detach_port(rx_args.port) == -1)
        error(1, errno, "dlb_detach_port");

    if (dlb_detach_port(tx_args.port) == -1)
        error(1, errno, "dlb_detach_port");

    if (dlb_detach_sched_domain(domain) == -1)
        error(1, errno, "dlb_detach_sched_domain");

    if (dlb_reset_sched_domain(dlb, domain_id) == -1)
        error(1, errno, "dlb_reset_sched_domain");

    if (dlb_close(dlb) == -1)
        error(1, errno, "dlb_close");

    free(worker_threads);
    free(worker_args);

    return 0;
}
