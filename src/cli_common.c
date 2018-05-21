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

int
loglevel_unformat(const char *level_str);

const char *
loglevel_format(u32 loglevel);

extern	ThreadVars g_tv[];
extern 	DecodeThreadVars g_dtv[];
extern	PacketQueue g_pq[];

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

	for (lcore = 0; lcore < vm->max_lcores; lcore ++) {
		tv = &g_tv[lcore % vm->max_lcores];
		dtv = &g_dtv[lcore % vm->max_lcores];

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

	oryx_format(&fb, "%8s%20s%8s%8s%8s%8s%8s%8s%8s%8s%8s%8s%8s", 
		"Lcore", "Pkts", "Bytes", "eth", "arp", "ipv4", "ipv6", "udp", "tcp", "sctp", "icmpv4", "icmpv6", "invalid");
	vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
	oryx_format_reset(&fb);

	for (lcore = 0; lcore < vm->max_lcores; lcore ++) {

		char format_pkts[20] = {0};
		sprintf (format_pkts, "%llu(%.2f%)", counter_pkts[lcore], ratio_of(counter_pkts[lcore], counter_pkts_total));
		oryx_format(&fb, "%8u%20s%8llu%8llu%8llu%8llu%8llu%8llu%8llu%8llu%8llu%8llu%8llu",
			lcore, 
			format_pkts,
			counter_bytes[lcore], counter_eth[lcore], counter_arp[lcore],
			counter_ipv4[lcore], counter_ipv6[lcore], counter_udp[lcore], counter_tcp[lcore],
			counter_sctp[lcore], counter_icmpv4[lcore], counter_icmpv6[lcore], counter_pkts_invalid[lcore]);
		vty_out(vty, "%s%s", FMT_DATA(fb), VTY_NEWLINE);
		oryx_format_reset(&fb);
	}

	oryx_format_free(&fb);
	return CMD_SUCCESS;
}

void common_cli(void)
{
	ThreadVars *tv;
	
	install_element (CONFIG_NODE, &set_log_level_cmd);
	install_element (CONFIG_NODE, &show_dp_stats_cmd);
}
