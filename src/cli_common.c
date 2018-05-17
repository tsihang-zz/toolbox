#include "oryx.h"
#include "vty.h"
#include "command.h"

#include "event.h"
#include "vars_private.h"
#include "queue_private.h"
#include "threadvars.h"
#include "dpdk.h"

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
	ThreadVars *tv = &g_tv[lcore];
	DecodeThreadVars *dtv = &g_dtv[lcore];
	PacketQueue *pq = &g_pq[lcore];

#define format_buf_size 4096
	char buf[format_buf_size] = {0};
	size_t step = 0;

	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "pkts:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_pkts), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "bytes:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_bytes), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "eth:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_eth), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "arp:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_arp), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "ipv4:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_ipv4), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "ipv6:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_ipv6), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "udp:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_udp), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "tcp:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_tcp), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "sctp:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_sctp), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "icmpv4:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_icmpv4), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "icmpv6:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_icmpv6), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "flows.memcap:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_flow_memcap), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "flows.tcp:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_flow_tcp), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "flows.udp:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_flow_udp), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "flows.icmpv4:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_flow_icmp4), VTY_NEWLINE);
	step += snprintf (buf + step, format_buf_size - step, 
		  "\"%15s\" 	  %llu%s", "flows.icmpv6:", oryx_counter_get(&tv->perf_private_ctx0, dtv->counter_flow_icmp6), VTY_NEWLINE);

	vty_out(vty, "%s%s%s", VTY_NEWLINE, buf, VTY_NEWLINE);

	return CMD_SUCCESS;
}

void common_cli(void)
{
	ThreadVars *tv;
	
	install_element (CONFIG_NODE, &set_log_level_cmd);
	install_element (CONFIG_NODE, &show_dp_stats_cmd);
}
