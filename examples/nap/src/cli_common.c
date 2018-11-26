#include "oryx.h"
#include "vty.h"
#include "command.h"

#include "event.h"
#include "vars_private.h"
#include "queue_private.h"
#include "threadvars.h"

#if defined(HAVE_DPDK)
#include "dpdk.h"
#endif

extern vlib_main_t vlib_main;
extern threadvar_ctx_t g_tv[];
extern decode_threadvar_ctx_t g_dtv[];
extern pq_t g_pq[];

int
loglevel_unformat(const char *level_str);

const char *
loglevel_format(uint32_t loglevel);

DEFUN(set_log_level,
      set_log_level_cmd,
      "log_level (debug|info|notice|warning|error|critical|alert|emerg)",
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR
      KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	uint32_t loglevel0 = UINT_MAX; 
	uint32_t loglevel1 = UINT_MAX;

	loglevel0 = oryx_log_global_log_level;
	loglevel1 = loglevel_unformat(argv[0]);
	
	oryx_log_global_log_level = \
		loglevel1 != UINT_MAX ? loglevel1 : oryx_log_global_log_level;
	
	vty_out(vty, "loglevel changed %s -> %s%s", 
		loglevel_format(loglevel0),  loglevel_format(loglevel1), VTY_NEWLINE);

	return CMD_SUCCESS;
}


DEFUN(show_dp_stats,
	show_dp_stats_cmd,
	"show dp_stats [clear]",
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	int lcore = 0;
	threadvar_ctx_t *tv;
	decode_threadvar_ctx_t *dtv;
	vlib_main_t *vm = &vlib_main;
	dpdk_main_t *dm = &dpdk_main;
	dpdk_config_main_t *conf = dm->conf;
	uint64_t nr_mbufs_delta = 0;
	struct timeval start, end;
	static uint64_t	cur_tsc[MAX_LCORES];
	struct oryx_fmt_buff_t fb = FMT_BUFF_INITIALIZATION;
	uint64_t counter_eth[MAX_LCORES] = {0};
	uint64_t counter_ipv4[MAX_LCORES] = {0};
	uint64_t counter_ipv6[MAX_LCORES] = {0};
	uint64_t counter_tcp[MAX_LCORES] = {0};
	uint64_t counter_udp[MAX_LCORES] = {0};
	uint64_t counter_sctp[MAX_LCORES] = {0};
	uint64_t counter_arp[MAX_LCORES] = {0};
	uint64_t counter_icmpv4[MAX_LCORES] = {0};
	uint64_t counter_icmpv6[MAX_LCORES] = {0};
	uint64_t counter_pkts[MAX_LCORES] = {0};
	uint64_t counter_bytes[MAX_LCORES] = {0};
	uint64_t counter_pkts_invalid[MAX_LCORES] = {0};
	uint64_t counter_drop[MAX_LCORES] = {0};
	uint64_t counter_http[MAX_LCORES] = {0};
	uint64_t counter_eth_total = 0;
	uint64_t counter_ipv4_total = 0;
	uint64_t counter_ipv6_total = 0;
	uint64_t counter_tcp_total = 0;
	uint64_t counter_udp_total = 0;
	uint64_t counter_icmpv4_total = 0;
	uint64_t counter_icmpv6_total = 0;
	uint64_t counter_pkts_total = 0;
	uint64_t counter_bytes_total = 0;
	uint64_t counter_arp_total = 0;
	uint64_t counter_sctp_total = 0;
	uint64_t counter_pkts_invalid_total = 0;
	uint64_t counter_drop_total = 0;
	uint64_t counter_http_total = 0;
	uint64_t nr_mbufs_refcnt = 0;
	uint64_t nr_mbufs_feedback = 0;

	if (!(vm->ul_flags & VLIB_DP_INITIALIZED)) {
		vty_out(vty, "Dataplane is not ready%s", VTY_NEWLINE);
		return CMD_SUCCESS;
	}

	gettimeofday(&start, NULL);

	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		tv = &g_tv[lcore % vm->nb_lcores];
		dtv = &g_dtv[lcore % vm->nb_lcores];

		cur_tsc[lcore] 		=  tv->cur_tsc;
		nr_mbufs_refcnt		+= tv->nr_mbufs_refcnt;
		nr_mbufs_feedback	+= tv->nr_mbufs_feedback;
		
		counter_pkts_total += 
			counter_pkts[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_pkts);
		counter_bytes_total += 
			counter_bytes[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_bytes);
		counter_eth_total += 
			counter_eth[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_eth);
		counter_ipv4_total += 
			counter_ipv4[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_ipv4);
		counter_ipv6_total += 
			counter_ipv6[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_ipv6);
		counter_tcp_total += 
			counter_tcp[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_tcp);
		counter_udp_total += 
			counter_udp[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_udp);
		counter_icmpv4_total += 
			counter_icmpv4[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_icmpv4);
		counter_icmpv6_total +=
			counter_icmpv6[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_icmpv6);
		counter_arp_total +=
			counter_arp[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_arp);
		counter_sctp_total +=
			counter_sctp[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_sctp);
		counter_pkts_invalid_total +=
			counter_pkts_invalid[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_invalid);
		counter_http_total +=
			counter_http[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_http);
		counter_drop_total +=
			counter_drop[lcore] = oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_drop);
	}

	if (argc == 1 && !strncmp (argv[0], "c", 1)) {
		for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
			tv	= &g_tv[lcore % vm->nb_lcores];
			dtv	= &g_dtv[lcore % vm->nb_lcores];

			tv->nr_mbufs_feedback	= 0;
			tv->nr_mbufs_refcnt		= 0;
			
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_pkts, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_bytes, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_eth, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_ipv4, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_ipv6, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_tcp, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_udp, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_icmpv4, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_icmpv6, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_arp, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_sctp, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_invalid, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_drop, 0);
			oryx_counter_set(&tv->perf_private_ctx0, dtv->counter_http, 0);
		}
	}

	nr_mbufs_delta = (nr_mbufs_refcnt - nr_mbufs_feedback);

	vty_newline(vty);
	vty_out(vty, "==== Global Graph%s", VTY_NEWLINE);	
	vty_out(vty, "%12s%16lu%s", "Total_Pkts:", counter_pkts_total, VTY_NEWLINE);
	vty_out(vty, "%12s%16lu (%lu, %.2f%%)%s", "MBUF Usage:", nr_mbufs_delta, conf->nr_mbufs, ratio_of(nr_mbufs_delta, conf->nr_mbufs), VTY_NEWLINE);
	vty_out(vty, "%12s%16lu (%.2f%%)%s", "IPv4:", counter_ipv4_total, ratio_of(counter_ipv4_total, counter_pkts_total), VTY_NEWLINE);
	vty_out(vty, "%12s%16lu (%.2f%%)%s", "IPv6:", counter_ipv6_total, ratio_of(counter_ipv6_total, counter_pkts_total), VTY_NEWLINE);
	vty_out(vty, "%12s%16lu (%.2f%%)%s", "UDP:", counter_udp_total, ratio_of(counter_udp_total, counter_pkts_total), VTY_NEWLINE);
	vty_out(vty, "%12s%16lu (%.2f%%)%s", "TCP:", counter_tcp_total, ratio_of(counter_tcp_total, counter_pkts_total), VTY_NEWLINE);
	vty_out(vty, "%12s%16lu (%.2f%%)%s", "SCTP:", counter_sctp_total, ratio_of(counter_sctp_total, counter_pkts_total), VTY_NEWLINE);
	vty_out(vty, "%12s%16lu (%.2f%%)%s", "Drop:", counter_drop_total, ratio_of(counter_drop_total, counter_pkts_total), VTY_NEWLINE);
	vty_newline(vty);
	
	oryx_format_reset(&fb);
	oryx_format(&fb, "%12s", " ");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		char lcore_str[32];
		sprintf(lcore_str, "lcore%d", lcore);
		oryx_format(&fb, "%20s", lcore_str);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "core_ratio");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		char format_pkts[20] = {0};
		sprintf (format_pkts, "%.2f%s", ratio_of(counter_pkts[lcore], counter_pkts_total), "%");
		oryx_format(&fb, "%20s", format_pkts);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "alive");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		oryx_format(&fb, "%20s", cur_tsc[lcore] == tv->cur_tsc ? "N" : "-");
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);
	

	oryx_format(&fb, "%12s", "pkts");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_pkts[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_pkts[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "ethernet");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_eth[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_eth[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "ipv4");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_ipv4[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_ipv4[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "ipv6");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_ipv6[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_ipv6[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "tcp");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_tcp[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_tcp[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "udp");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_udp[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_udp[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "sctp");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_sctp[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_sctp[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "http");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_http[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_http[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "arp");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_arp[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_arp[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "icmpv4");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_icmpv4[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_icmpv4[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "icmpv6");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_icmpv6[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_icmpv6[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);	

	oryx_format(&fb, "%12s", "drop");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		if(counter_drop[lcore] == 0)
			oryx_format(&fb, "%20s", "-");
		else
			oryx_format(&fb, "%20llu", counter_drop[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);	

	oryx_format_free(&fb);

	gettimeofday(&end, NULL);
	vty_out(vty, ", cost %lu us%s", oryx_elapsed_us(&start, &end), VTY_NEWLINE);

	return CMD_SUCCESS;
}

void common_cli(vlib_main_t *vm)
{
	threadvar_ctx_t *tv;
	
	install_element (CONFIG_NODE, &set_log_level_cmd);
	install_element (CONFIG_NODE, &show_dp_stats_cmd);
}
