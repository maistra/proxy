/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2022 Intel Corporation
 */

/* This tool displays the monitoring data for libdlb applications.
 *  It obtains data from the dlb device file periodically.
 *  -i can be used to pass the device_id
 *  -z can be used to skip zero values
 *  -w can be used to display the data continuously.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <error.h>
#include <pthread.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <inttypes.h>
#include "dlb.h"
#include "dlb_priv.h"
#include "dump_dlb_regs.h"

#ifndef SYS_gettid
#error "SYS_gettid unavailable on this system"
#endif
#define gettid() ((pid_t)syscall(SYS_gettid))

#define CSR_BAR_SIZE  (4ULL * 1024ULL * 1024ULL * 1024ULL)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define US_PER_S 1000000
#define RTE_DIM(a) (sizeof(a) / sizeof((a)[0]))
#define NUM_EVENTS_PER_LOOP 32
#define RETRY_LIMIT 10000000
#define NUM_LDB_PORTS 20
#define BUF_LEN 100
#define PATH_STR1 "/sys/class/dlb2/dlb"
#define PATH_STR2 "/device/resource2"
#define DLB_DEV_XSTATS_NAME_SIZE 64
#define MAX_PORTS_QUEUES 256
#define CQ_DEPTH 8
#define MAX_DEPTH_STRING_LEN 62

static dlb_dev_cap_t cap;
static int dev_id;
static uint64_t num_events;
static unsigned int sns_per_queue;
static int num_credit_combined;
static int num_credit_ldb;
static int num_credit_dir;
static int num_workers = 1;
static bool use_max_credit_combined = true;
static bool use_max_credit_ldb = true;
static bool use_max_credit_dir = true;

static dlb_resources_t rsrcs;

/* Default is false for all flags. Enable them from command line */
static bool do_reset = false;
static bool do_watch = false;  /* Watch mode */
static bool skip_zero = false; /* Skip zero values */
static bool prt_ldb = false;   /* Print LDB queues */
static bool prt_dir = false;   /* Print DIR queues */
static bool prt_cq = false;    /* Print CQ statstics */
static bool prt_glb = false;   /* Print Global Statistics */

static void get_device_xstats(void);
static void collect_config(void);
static void display_config(void);
static void collect_stats(void);
static void display_stats(void);
static void display_queue_stats(void);
static void display_device_config(void);
static uint8_t *base;
static uint64_t num_ldb_ports;
static uint64_t num_ldb_queues;
static uint64_t num_dir_ports;
static uint64_t num_dir_queues;

struct dlb_dev_xstats_name {
	char name[DLB_DEV_XSTATS_NAME_SIZE];
};

/* Rate calculations */
static uint32_t measure_time_us = 1 * US_PER_S;
static double time_elapsed;
static struct timespec start_time, stop_time;
static uint64_t hcw_nalb_prev;
static uint64_t hcw_atm_prev;
static uint64_t hcw_dir_prev;

static const char * const dev_xstat_strs[] = {
	"cfg_cq_ldb_tot_inflight_count",
	"cfg_cq_ldb_tot_inflight_limit",
	"cfg_cmp_pp_nq_hptr_ldb_credit",
	"dlb_dm.cfg_cmp_pp_nq_hptr_dir_credit",
	"dev_pool_size",
	"cfg_counter_dequeue_hcw_atm",
	"cfg_counter_enqueue_hcw_atm",
	"cfg_counter_dequeue_hcw_dir",
	"cfg_counter_enqueue_hcw_dir",
	"cfg_counter_dequeue_hcw_nalb",
	"cfg_counter_enqueue_hcw_nalb",
	"cfg_aqed_tot_enqueue_count",
	"cfg_aqed_tot_enqueue_limit",
};

enum dlb_dev_xstats {
	DEV_INFL_EVENTS,
	DEV_NB_EVENTS_LIMIT,
	DEV_LDB_POOL_SIZE,
	DEV_DIR_POOL_SIZE,
	DEV_POOL_SIZE,
	CFG_COUNTER_DEQUEUE_HCW_ATM,
	CFG_COUNTER_ENQUEUE_HCW_ATM,
	CFG_COUNTER_DEQUEUE_HCW_DIR,
	CFG_COUNTER_ENQUEUE_HCW_DIR,
	CFG_COUNTER_DEQUEUE_HCW_NALB,
	CFG_COUNTER_ENQUEUE_HCW_NALB,
	DEV_AQED_ENQ_CNT,
	DEV_AQED_ENQ_LIMIT,
};

static const char * const port_xstat_strs[] = {
	"tx_ok",
	"tx_new",
	"tx_fwd",
	"tx_rel",
	"tx_sched_ordered",
	"tx_sched_unordered",
	"tx_sched_atomic",
	"tx_sched_directed",
	"tx_invalid",
	"tx_nospc_ldb_hw_credits",
	"tx_nospc_dir_hw_credits",
	"tx_nospc_hw_credits",
	"tx_nospc_inflight_max",
	"tx_nospc_new_event_limit",
	"tx_nospc_inflight_credits",
	"outstanding_releases",
	"max_outstanding_releases",
	"total_polls",
	"zero_polls",
	"rx_ok",
	"rx_sched_ordered",
	"rx_sched_unordered",
	"rx_sched_atomic",
	"rx_sched_directed",
	"rx_sched_invalid",
	"is_configured",
	"is_load_balanced",
};

enum dlb_port_xstats {
	TX_OK,
	TX_NEW,
	TX_FWD,
	TX_REL,
	TX_SCHED_ORDERED,
	TX_SCHED_UNORDERED,
	TX_SCHED_ATOMIC,
	TX_SCHED_DIRECTED,
	TX_SCHED_INVALID,
	TX_NOSPC_LDB_HW_CREDITS,
	TX_NOSPC_DIR_HW_CREDITS,
	TX_NOSPC_HW_CREDITS,
	TX_NOSPC_INFL_MAX,
	TX_NOSPC_NEW_EVENT_LIM,
	TX_NOSPC_INFL_CREDITS,
	OUTSTANDING_RELEASES,
	MAX_OUTSTANDING_RELEASES,
	TOTAL_POLLS,
	ZERO_POLLS,
	RX_OK,
	RX_SCHED_ORDERED,
	RX_SCHED_UNORDERED,
	RX_SCHED_ATOMIC,
	RX_SCHED_DIRECTED,
	RX_SCHED_INVALID,
	IS_CONFIGURED,
	PORT_IS_LOAD_BALANCED,
};

static const char * const queue_xstat_strs[] = {
	"current_depth",
	"is_load_balanced",
	"cfg_qid_ldb_inflight_count",
	"cfg_qid_ldb_inflight_limit",
	"cfg_qid_aqed_active_count",
	"cfg_atm_qid_dpth_thrsh",
	"cfg_nalb_qid_dpth_thrsh",
	"cfg_ldb_cq_depth",
	"cfg_cq_ldb_token_count",
	"cfg_cq_ldb_token_depth_select",
	"cfg_dir_cq_depth",
	"cfg_dir_qid_dpth_thrsh",
	"cfg_qid_atq_enqueue_count",
	"cfg_qid_ldb_enqueue_count",
};

enum dlb_queue_xstats {
	CURRENT_DEPTH=0,
	QUEUE_IS_LOAD_BALANCED, 
	CFG_QID_LDB_INFLIGHT_COUNT,
	CFG_QID_LDB_INFLIGHT_LIMIT,
	CFG_QID_ATM_ACTIVE,
	CFG_ATM_QID_DPTH_THRSH,
	CFG_NALB_QID_DPTH_THRSH,
	CFG_LDB_CQ_DEPTH,
	CFG_CQ_LDB_TOKEN_COUNT,
	CFG_CQ_LDB_TOKEN_DEPTH_SELECT,
	CFG_DIR_CQ_DEPTH,
	CFG_DIR_QID_DPTH_THRSH,
	CFG_QID_ATQ_ENQ_CNT,
	CFG_QID_LDB_ENQ_CNT,
};

unsigned int dev_xstat_ids[RTE_DIM(dev_xstat_strs)];
unsigned int queue_xstat_ids[MAX_PORTS_QUEUES][RTE_DIM(queue_xstat_strs)];
uint64_t dev_xstat_vals[RTE_DIM(dev_xstat_strs)];
uint64_t queue_xstat_vals[MAX_PORTS_QUEUES][RTE_DIM(queue_xstat_strs)] = {0};

static void dump_regs(uint8_t *base)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dlb_regs); i++) {
		printf("%s 0x%08x 0x%08x\n",
				dlb_regs[i].name,
				dlb_regs[i].offset,
				*(uint32_t *)(base + dlb_regs[i].offset));
	}
}

static void get_xstats(uint8_t *base, uint64_t *val, const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dlb_regs); i++) {
		if (strstr(dlb_regs[i].name, name))
			*val = *(uint32_t *)(base + dlb_regs[i].offset);
	}
}

static int print_resources(dlb_resources_t *rsrcs)
{
	printf("DLB's available resources:\n");
	printf("\tDomains:           %d\n", rsrcs->num_sched_domains);
	printf("\tLDB queues:        %d\n", rsrcs->num_ldb_queues);
	printf("\tLDB ports:         %d\n", rsrcs->num_ldb_ports);
	printf("\tDIR ports:         %d\n", rsrcs->num_dir_ports);
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

/* Prints Usage*/
static void
usage(void)
{
	const char *usage_str =
		"Usage: dlb_monitor_sec [options]\n"
		"Options:\n"
		" -i <dev_id>   DLB Device id (default: 0)\n"
		" -r            Reset stats after displaying them\n"
		" -t <duration> Measurement duration (seconds) (min: 1s, default: 1s)\n"
		" -w            Repeatedly print stats\n"
		" -z            Don't print ports or queues with 0 enqueue/dequeue/depth stats\n"
		" -l            Print LDB queue statstics\n"
		" -d            Print DIR queue statstics\n"
		" -c            Print CQ  queue statstics\n"
		" -a            Equivalent to setting 'ldcg' flags\n"
		"\n";

	printf("%s\n", usage_str);
	printf("Acronyms\n");
	printf("\t ldb_infl: Per-QID count of the number of load balanced QEs {ATM, UNO, ORD} waiting for a completion.\n");
	printf("\t inf_limit: Per-QID maximum number of {ATM, UNO, ORD} QE permitted to wait for a completion.\n");
	printf("\t atm_active: Atomic QID Active Count\n");
	printf("\t atm_th: Atomic QID Depth Threshold\n");
	printf("\t naldb_th: Nonatomic Load Balanced QID Depth Threshold\n");
	printf("\t depth_th: DIR QID Depth Threshold\n");
	printf("\t ldb_cq_depth: Per LDB CQ count of the number of tokens owned by the consumer port.\n");
	printf("\t dir_cq_depth: Per DIR CQ Depth. Number of tokens held by the consumer port.\n");
	printf("\t cq_ldb_token: Count of the number of tokens owned by the LDB CQ.\n");
	printf("\n");
	exit(1);
}

/* Parses the input args*/
static int
parse_app_args(int argc, char **argv)
{
	int option_index, c;

	opterr = 0;

	for (;;) {
		c = getopt_long(argc, argv, "i:rt:wzldcga", NULL, &option_index); /* :=has value */
		if (c == -1)
			break;

		switch (c) {
		case 'i':
			dev_id = atoi(optarg);
			break;
		case 'r':
			do_reset = true;
			break;
		case 't':
			if (atoi(optarg) < 1)
				usage();
			measure_time_us = atoi(optarg) * US_PER_S;
			break;
		case 'w':
			do_watch = true;
			break;
		case 'z':
			skip_zero = true;
			break;
		case 'l':
			prt_ldb = true;
			break;
		case 'd':
			prt_dir = true;
			break;
		case 'c':
			prt_cq = true;
			break;
		case 'g':
			prt_glb = true;
			break;
		case 'a': /* set ldcg */
			prt_glb = true;
			prt_ldb = true;
			prt_dir = true;
			prt_cq = true;
			break;
		default:
			usage();
			break;
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	int worker_port_id[NUM_LDB_PORTS];
	int dir_queue_id, ldb_queue_id;
	dlb_resources_t rsrcs;
	char path[BUF_LEN];
	int total = 0, cnt;
	dlb_hdl_t dlb;
	int fd;
	int i;

	if (parse_app_args(argc, argv))
		return -1;

	if (dlb_open(dev_id, &dlb) == -1)
		error(1, errno, "dlb_open");

	printf("dev_id: %d\n", dev_id);

	if (dlb_get_num_resources(dlb, &rsrcs)) {
		error(1, errno, "dlb_get_num_resources");
		exit(-1);
	}

	if (print_resources(&rsrcs)) {
		error(1, errno, "print_resources");
		exit(-1);
	}

	sprintf(path, "%s%d%s", PATH_STR1, dev_id, PATH_STR2);
	printf("PATH: %s\n", path);

	fd = open(path, O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("open");
		exit(-1);
	}

	base = mmap(0, CSR_BAR_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	if (base == (void *) -1) {
		perror("mmap");
		exit(-1);
	}

	close(fd);

	printf("\n");

	/* Get and output any stats requested on the command line */
	collect_config();

	display_config();
	cnt = 0;

	do {
		collect_stats();
		if (do_watch)
			printf("Sample #%d\n", cnt++);

		if (skip_zero)
			printf("Skipping ports and queues with zero stats\n");

		display_stats();
	} while (do_watch);

	if (dlb_close(dlb) == -1)
		error(1, errno, "dlb_close");

	return 0;
}

/* Display device configuration params like dev id, pool size etc*/
static void
display_device_config(void)
{
	printf("\n");
	printf("          Device Configuration\n");
	printf("-----------------------------------------------------------\n");
	printf("      |  LDB pool size |  DIR pool size |  COMB pool size |\n");
	printf("Device|    (DLB 2.0)   |    (DLB 2.0)   |     (DLB 2.5)   |\n");
	printf("------|----------------|----------------|-----------------|\n");

	printf("  %2u  |     %5"PRIu64"      |      %4"PRIu64"      |",
			dev_id,
			dev_xstat_vals[DEV_LDB_POOL_SIZE],
			dev_xstat_vals[DEV_DIR_POOL_SIZE]);
	printf("      %5"PRIu64"      |\n",
			dev_xstat_vals[DEV_POOL_SIZE]);
	printf("-----------------------------------------------------------\n");
	printf("\n");
}

static void
display_config(void)
{
	display_device_config();

	/* TODO */
	/*display_port_config();*/

	/* TODO */
	/*display_queue_config();*/
}

static void
collect_config(void)
{
	uint32_t attr_id;
	unsigned int i;
	int ret;

	get_device_xstats();
}

/* get Configuration from dlb device registers*/
static void
get_device_xstats()
{

	get_xstats(base, &dev_xstat_vals[DEV_LDB_POOL_SIZE], dev_xstat_strs[DEV_LDB_POOL_SIZE]);
	get_xstats(base, &dev_xstat_vals[DEV_DIR_POOL_SIZE], dev_xstat_strs[DEV_DIR_POOL_SIZE]);
	get_xstats(base, &dev_xstat_vals[DEV_POOL_SIZE], dev_xstat_strs[DEV_POOL_SIZE]);
	get_xstats(base, &dev_xstat_vals[DEV_INFL_EVENTS], dev_xstat_strs[DEV_INFL_EVENTS]);
	get_xstats(base, &num_ldb_ports, "total_ldb_ports");
	get_xstats(base, &num_ldb_queues, "total_ldb_qid");
	get_xstats(base, &num_dir_ports, "total_dir_ports");
	get_xstats(base, &num_dir_queues, "total_dir_qid");

}

/* display device stats*/
static void
display_device_stats(void)
{
	float ldb_sched_throughput, dir_sched_throughput;
	uint64_t events_inflight, nb_events_limit, aqed_enq_cnt, aqed_enq_limit;
	uint64_t total = 0;
	unsigned int i;

	events_inflight = dev_xstat_vals[DEV_INFL_EVENTS];
	nb_events_limit = dev_xstat_vals[DEV_NB_EVENTS_LIMIT];
	aqed_enq_cnt = dev_xstat_vals[DEV_AQED_ENQ_CNT];
	aqed_enq_limit = dev_xstat_vals[DEV_AQED_ENQ_LIMIT];

	ldb_sched_throughput = 0.0f;
	dir_sched_throughput = 0.0f;

	/* Throughput is displayed in millions of events per second, so no need
	 * to convert microseconds to seconds.
	 */
	ldb_sched_throughput = ldb_sched_throughput / measure_time_us;
	dir_sched_throughput = dir_sched_throughput / measure_time_us;
	ldb_sched_throughput = ldb_sched_throughput / measure_time_us;
	dir_sched_throughput = dir_sched_throughput / measure_time_us;

	printf("                        Device stats\n");
	printf("-----------------------------------------------------------\n");
	printf("Inflight events: %"PRIu64"/%"PRIu64"\n", events_inflight, nb_events_limit);
	printf("AQED storage events: %"PRIu64"/%"PRIu64"\n", events_inflight, nb_events_limit);
	printf("Non-atomic LDB QE Rate: %3.2f  mpps\n",
		(dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_NALB] - hcw_nalb_prev)
		/ (time_elapsed * 1000000));
	hcw_nalb_prev = dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_NALB];
	printf("Atomic LDB QE Rate: %3.2f  mpps\n",
		(dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_ATM] - hcw_atm_prev)
		/ (time_elapsed * 1000000));
	hcw_atm_prev = dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_ATM];
	printf("Directed QE Rate: %3.2f  mpps\n",
		(dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_DIR] - hcw_dir_prev)
		/ (time_elapsed * 1000000));
	hcw_dir_prev = dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_DIR];
	printf("\n-----------------------------------------------------------\n");
}

static void
display_stats(void)
{
	/*TODO*/
	/*display_port_dequeue_stats();*/

	/*TODO*/
	/*display_port_enqueue_stats();*/

	display_queue_stats();

	display_device_stats();

	printf("Note: scheduling throughput measured over a duration of %us. All other stats are instantaneous samples.\n",
			measure_time_us / US_PER_S);
	printf("\n");
}

/* Sleep function to wait for the time interval between data display*/
void
dlb_delay_us_sleep(unsigned int us)
{
	struct timespec wait[2] = {0};
	int ind = 0;

	wait[0].tv_sec = 0;
	if (us >= US_PER_S) {
		wait[0].tv_sec = us / US_PER_S;
		us -= wait[0].tv_sec * US_PER_S;
	}
	wait[0].tv_nsec = 1000 * us;

	while (nanosleep(&wait[ind], &wait[1 - ind]) && errno == EINTR) {
		/*
		 * Sleep was interrupted. Flip the index, so the 'remainder'
		 * will become the 'request' for a next call.
		 */
		ind = 1 - ind;
	}

	return;
}

/* Collect stats periodically from the DLB device registers*/
static void
collect_stats(void)
{
	char xstatname_buf[DLB_DEV_XSTATS_NAME_SIZE];
	unsigned int i;
	int ret;

	/* Wait while the eventdev application executes */
	dlb_delay_us_sleep(measure_time_us);

	get_xstats(base, &dev_xstat_vals[DEV_INFL_EVENTS], dev_xstat_strs[DEV_INFL_EVENTS]);
	get_xstats(base, &dev_xstat_vals[DEV_NB_EVENTS_LIMIT], dev_xstat_strs[DEV_NB_EVENTS_LIMIT]);
	get_xstats(base, &dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_ATM], dev_xstat_strs[CFG_COUNTER_DEQUEUE_HCW_ATM]);
	get_xstats(base, &dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_ATM], dev_xstat_strs[CFG_COUNTER_ENQUEUE_HCW_ATM]);
	get_xstats(base, &dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_DIR], dev_xstat_strs[CFG_COUNTER_DEQUEUE_HCW_DIR]);
	get_xstats(base, &dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_DIR], dev_xstat_strs[CFG_COUNTER_ENQUEUE_HCW_DIR]);
	get_xstats(base, &dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_NALB], dev_xstat_strs[CFG_COUNTER_DEQUEUE_HCW_NALB]);
	get_xstats(base, &dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_NALB], dev_xstat_strs[CFG_COUNTER_ENQUEUE_HCW_NALB]);

	if (clock_gettime(CLOCK_REALTIME, &stop_time) == -1) {
		perror("ERROR: clock_gettime()"); exit(-1);
	}
	time_elapsed = (stop_time.tv_sec - start_time.tv_sec) +
		(stop_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;
	if (clock_gettime(CLOCK_REALTIME, &start_time) == -1) {
		perror("ERROR: clock_gettime()"); exit(-1);
	}

	/* Collect Queue xstats */
	for (i = 0; i < num_ldb_queues; i++) {
		sprintf(xstatname_buf, "%s[%d]", queue_xstat_strs[CFG_QID_LDB_INFLIGHT_COUNT], i);
		get_xstats(base, &queue_xstat_vals[i][CFG_QID_LDB_INFLIGHT_COUNT], xstatname_buf);
		sprintf(xstatname_buf, "%s[%d]", queue_xstat_strs[CFG_QID_LDB_INFLIGHT_LIMIT], i);
		get_xstats(base, &queue_xstat_vals[i][CFG_QID_LDB_INFLIGHT_LIMIT], xstatname_buf);
		sprintf(xstatname_buf, "%s[%d]", queue_xstat_strs[CFG_QID_ATM_ACTIVE], i);
		get_xstats(base, &queue_xstat_vals[i][CFG_QID_ATM_ACTIVE], xstatname_buf);
		sprintf(xstatname_buf, "%s[%d]", queue_xstat_strs[CFG_ATM_QID_DPTH_THRSH], i);
		get_xstats(base, &queue_xstat_vals[i][CFG_ATM_QID_DPTH_THRSH], xstatname_buf);
		sprintf(xstatname_buf, "%s[%d]", queue_xstat_strs[CFG_NALB_QID_DPTH_THRSH], i);
		get_xstats(base, &queue_xstat_vals[i][CFG_NALB_QID_DPTH_THRSH], xstatname_buf);
		sprintf(xstatname_buf, "%s[%d]", queue_xstat_strs[CFG_QID_ATQ_ENQ_CNT], i);
		get_xstats(base, &queue_xstat_vals[i][CFG_QID_ATQ_ENQ_CNT], xstatname_buf);
		sprintf(xstatname_buf, "%s[%d]", queue_xstat_strs[CFG_QID_LDB_ENQ_CNT], i);
		get_xstats(base, &queue_xstat_vals[i][CFG_QID_LDB_ENQ_CNT], xstatname_buf);
	}

	for (i = 0; i < num_dir_queues; i++) {
		sprintf(xstatname_buf, "%s[%d]", queue_xstat_strs[CFG_LDB_CQ_DEPTH], i);
		get_xstats(base, &queue_xstat_vals[num_ldb_queues+i][CFG_LDB_CQ_DEPTH], xstatname_buf);
		sprintf(xstatname_buf, "%s[%d]", queue_xstat_strs[CFG_CQ_LDB_TOKEN_COUNT], i);
		get_xstats(base, &queue_xstat_vals[num_ldb_queues+i][CFG_CQ_LDB_TOKEN_COUNT], xstatname_buf);
		sprintf(xstatname_buf, "%s[%d]", queue_xstat_strs[CFG_CQ_LDB_TOKEN_DEPTH_SELECT], i);
		get_xstats(base, &queue_xstat_vals[num_ldb_queues+i][CFG_CQ_LDB_TOKEN_DEPTH_SELECT], xstatname_buf);
		sprintf(xstatname_buf, "%s[%d]", queue_xstat_strs[CFG_DIR_CQ_DEPTH], i);
		get_xstats(base, &queue_xstat_vals[num_ldb_queues+i][CFG_DIR_CQ_DEPTH], xstatname_buf);
		sprintf(xstatname_buf, "%s[%d]", queue_xstat_strs[CFG_DIR_QID_DPTH_THRSH], i);
		get_xstats(base, &queue_xstat_vals[num_ldb_queues+i][CFG_DIR_QID_DPTH_THRSH], xstatname_buf);
	}
}

/* Display queue Stats */
static void
display_queue_stats(void)
{
	unsigned int i;

	if(prt_ldb) {
	printf("\n");
	printf("               Per QID Configuration and stats\n");
	printf("--------------------------------------------------------\n");
	printf("\n");
	printf("   LDB QUEUE stats\n");
	printf("--------------------\n");
	printf("Queue|Type|ldb_inf|inf_limit|atm_active|atm_th |naldb_th|atq_enq|naldb_enq|\n");
	printf("-----|----|-------|---------|----------|-------|--------|-------|---------|\n");
	for (i = 0; i < num_ldb_queues; i++) {
		if (!queue_xstat_vals[i][CFG_QID_LDB_INFLIGHT_LIMIT] && skip_zero)
			continue;
		printf(" %3u |%s|%7"PRIu64"|%9"PRIu64"|%10"PRIu64"|%7"PRIu64"|%8"PRIu64"|%7"PRIu64"|%9"PRIu64"|\n",
				i,
				" LDB",
				queue_xstat_vals[i][CFG_QID_LDB_INFLIGHT_COUNT],
				queue_xstat_vals[i][CFG_QID_LDB_INFLIGHT_LIMIT],
				queue_xstat_vals[i][CFG_QID_ATM_ACTIVE],
				queue_xstat_vals[i][CFG_ATM_QID_DPTH_THRSH],
				queue_xstat_vals[i][CFG_NALB_QID_DPTH_THRSH],
				queue_xstat_vals[i][CFG_QID_ATQ_ENQ_CNT],
				queue_xstat_vals[i][CFG_QID_LDB_ENQ_CNT]);
	}

	printf("-------------------------------------------------------------------------------\n");
	}

	if(prt_dir) {
	printf("\n");
	printf("   DIR QUEUE stats\n");
	printf("--------------------\n");
	printf("Queue|Type|depth_th|\n");
	printf("-----|----|--------|\n");
	for (i = 0; i < num_dir_queues; i++) {
		if (!queue_xstat_vals[num_ldb_queues+i][CFG_DIR_QID_DPTH_THRSH] && skip_zero)
			continue;
		printf(" %3u |%s|%8"PRIu64"|\n",
				i,
				" DIR",
				queue_xstat_vals[num_ldb_queues+i][CFG_DIR_QID_DPTH_THRSH]);
	}
	printf("-------------------------------------------------------------------------------\n");
	}

	if(prt_cq) {
	printf("\n");
	printf(" Per Port CQ stats\n");
	printf("-------------------------------------------------\n");
	printf("  CQ |type|size|ldb_cq_depth|dir_cq_depth|cq_ldb_token|\n");
	printf("-----|----|----|------------|------------|------------|\n");
	for (i = 0; i < num_dir_queues; i++) {
		if ((!queue_xstat_vals[num_ldb_queues+i][CFG_LDB_CQ_DEPTH] &&
					!queue_xstat_vals[num_ldb_queues+i][CFG_DIR_CQ_DEPTH]) && skip_zero)
			continue;
		printf(" %3u |%s|%4"PRIu32"|%12"PRIu64"|%12"PRIu64"|%12"PRIu64"|\n",
				i,
				queue_xstat_vals[num_ldb_queues+i][CFG_DIR_CQ_DEPTH] ? " DIR" : " LDB",
				2 << (queue_xstat_vals[num_ldb_queues+i][CFG_CQ_LDB_TOKEN_DEPTH_SELECT]+1),
				queue_xstat_vals[num_ldb_queues+i][CFG_LDB_CQ_DEPTH],
				queue_xstat_vals[num_ldb_queues+i][CFG_DIR_CQ_DEPTH],
				queue_xstat_vals[num_ldb_queues+i][CFG_CQ_LDB_TOKEN_COUNT]);
	}
	printf("-------------------------------------------------------------------------------\n");
	}

	if( prt_glb ) {
	printf("\n");
	printf("-------------------------------------------------------------------------------\n");
	printf("           DLB Global Stats\n");
	printf("-------------------------------------------------------------------------------\n");
	printf("cfg_counter_enqueue_hcw_atm        %12"PRIu64"  Total Atomic HCW enqueued\n",
			dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_ATM]);
	printf("cfg_counter_dequeue_hcw_atm        %12"PRIu64"  Total Atomic HCW dequeued\n",
			dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_ATM]);
	printf("cfg_counter_enqueue_hcw_dir        %12"PRIu64"  Total DIR HCW enqueued\n",
			dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_DIR]);
	printf("cfg_counter_dequeue_hcw_dir        %12"PRIu64"  Total DIR HCW dequeued\n",
			dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_DIR]);
	printf("cfg_counter_enqueue_hcw_nalb       %12"PRIu64"  Total LDB HCW enqueued\n",
			dev_xstat_vals[CFG_COUNTER_ENQUEUE_HCW_NALB]);
	printf("cfg_counter_dequeue_hcw_nalb       %12"PRIu64"  Total LDB HCW dequeued\n",
			dev_xstat_vals[CFG_COUNTER_DEQUEUE_HCW_NALB]);

	printf("-------------------------------------------------------------------------------\n");
	printf("\n");
	}
}
