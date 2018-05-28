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
extern ThreadVars g_tv[];
extern DecodeThreadVars g_dtv[];
extern PacketQueue g_pq[];

int
loglevel_unformat(const char *level_str);

const char *
loglevel_format(u32 loglevel);

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
	u32 loglevel0 = UINT_MAX; 
	u32 loglevel1 = UINT_MAX;

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
	"show dp_stats",
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	int lcore = 0;
	ThreadVars *tv;
	DecodeThreadVars *dtv;
	vlib_main_t *vm = &vlib_main;
	struct oryx_fmt_buff_t fb = FMT_BUFF_INITIALIZATION;
	u64 counter_eth[MAX_LCORES] = {0};
	u64 counter_ipv4[MAX_LCORES] = {0};
	u64 counter_ipv6[MAX_LCORES] = {0};
	u64 counter_tcp[MAX_LCORES] = {0};
	u64 counter_udp[MAX_LCORES] = {0};
	u64 counter_sctp[MAX_LCORES] = {0};
	u64 counter_arp[MAX_LCORES] = {0};
	u64 counter_icmpv4[MAX_LCORES] = {0};
	u64 counter_icmpv6[MAX_LCORES] = {0};
	u64 counter_pkts[MAX_LCORES] = {0};
	u64 counter_bytes[MAX_LCORES] = {0};
	u64 counter_pkts_invalid[MAX_LCORES] = {0};
	u64 counter_eth_total = 0;
	u64 counter_ipv4_total = 0;
	u64 counter_ipv6_total = 0;
	u64 counter_tcp_total = 0;
	u64 counter_udp_total = 0;
	u64 counter_icmpv4_total = 0;
	u64 counter_icmpv6_total = 0;
	u64 counter_pkts_total = 0;
	u64 counter_bytes_total = 0;
	u64 counter_arp_total = 0;
	u64 counter_sctp_total = 0;
	u64 counter_pkts_invalid_total = 0;

	if (!(vm->ul_flags & VLIB_DP_INITIALIZED)) {
		vty_out(vty, "Dataplane is not ready%s", VTY_NEWLINE);
		return CMD_SUCCESS;
	}

	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		tv = &g_tv[lcore % vm->nb_lcores];
		dtv = &g_dtv[lcore % vm->nb_lcores];

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

	}

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

	oryx_format(&fb, "%12s", "pkts");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		oryx_format(&fb, "%20llu", counter_pkts[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "bytes");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		oryx_format(&fb, "%20llu", counter_bytes[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "ethernet");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		oryx_format(&fb, "%20llu", counter_eth[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "ipv4");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		oryx_format(&fb, "%20llu", counter_ipv4[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "ipv6");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		oryx_format(&fb, "%20llu", counter_ipv6[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "tcp");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		oryx_format(&fb, "%20llu", counter_tcp[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "udp");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		oryx_format(&fb, "%20llu", counter_udp[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "sctp");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		oryx_format(&fb, "%20llu", counter_sctp[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "arp");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		oryx_format(&fb, "%20llu", counter_arp[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "icmpv4");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		oryx_format(&fb, "%20llu", counter_icmpv4[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	oryx_format(&fb, "%12s", "icmpv6");
	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		oryx_format(&fb, "%20llu", counter_icmpv6[lcore]);
	}
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);	

	oryx_format_free(&fb);

	return CMD_SUCCESS;
}

DEFUN(clear_dp_stats,
	clear_dp_stats_cmd,
	"clear dp_stats",
	KEEP_QUITE_STR KEEP_QUITE_CSTR
	KEEP_QUITE_STR KEEP_QUITE_CSTR)
{
	int lcore = 0;
	ThreadVars *tv;
	DecodeThreadVars *dtv;
	vlib_main_t *vm = &vlib_main;

	for (lcore = 0; lcore < vm->nb_lcores; lcore ++) {
		tv = &g_tv[lcore % vm->nb_lcores];
		dtv = &g_dtv[lcore % vm->nb_lcores];

		tv->n_rx_bytes = 0;
		tv->n_rx_packets = 0;
		tv->n_tx_bytes = 0;
		tv->n_tx_packets = 0;
		
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

	}

	return CMD_SUCCESS;
}

void common_cli(void)
{
	ThreadVars *tv;
	
	install_element (CONFIG_NODE, &set_log_level_cmd);
	install_element (CONFIG_NODE, &show_dp_stats_cmd);
	install_element (CONFIG_NODE, &clear_dp_stats_cmd);
}
