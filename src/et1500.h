#ifndef __HB_H__
#define __HB_H__

#define OS_DEBUG
#ifdef OS_DEBUG
#define stdout_(fmt, ...) printf(""fmt"", ##__VA_ARGS__)
#else
#define stdout_(fmt, ...)
#endif

typedef struct vlib_main_t
{
	int argc;
	char **argv;
	/* Name for e.g. syslog. */
	const char *prgname;
	int log_level;
	char *log_path;

	void (*sighandler)(int signum);
	
	/** equal with sizeof (Packet) */
	u32 extra_priv_size;

#define VLIB_DPDK_EAL_INITIALIZED		(1 << 0)
#define VLIB_DPDK_MEMPOOL_INITIALIZED	(1 << 1)
#define VLIB_QUIT						(1 << 2)
	u32 ul_flags;
} vlib_main_t;

/** Quadrant */
enum {
	QUA_DROP,
	QUA_PASS,
	QUAS,
};

/** This also means the priotity.  */
#define foreach_criteria\
  _(DROP, 0, "Drop if hit.") \
  _(PASS, 1, "Forward current frame to output path if hit.")\
  _(TIMESTAMP, 2, "Enable or disable tag a frame with timestamp.")\
  _(SLICING, 3, "Enable or disable frame slicing.")				\
  _(DESENSITIZATION, 4, "Enable or disable packet desensitization.")	\
  _(DEDUPLICATION, 5, "Enable or disable packet duplication checking.")\
  _(RESV, 6, "Resv.")
  

enum criteria_flags_t {
#define _(f,o,s) CRITERIA_FLAGS_##f = (1<<o),
	foreach_criteria
#undef _
	CRITERIA_FLAGS,
};

struct stats_trunk_t {
	u64 bytes;
	u64 packets;
};

#include "rc4.h"

#if defined(HAVE_QUAGGA)
#include "vector.h"
#include "prefix.h"
#endif

#if defined(HAVE_SURICATA)
#include "suricata-common.h"
#include "config.h"

#if HAVE_GETOPT_H
#include <getopt.h>
#endif

#if HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_NSS
#include <prinit.h>
#include <nss.h>
#endif

#include "suricata.h"
#include "decode.h"
#include "detect.h"
#include "packet-queue.h"
#include "threads.h"
#include "threadvars.h"
#include "flow-worker.h"

#include "util-atomic.h"
#include "util-spm.h"
#include "util-cpu.h"
#include "util-action.h"
#include "util-pidfile.h"
#include "util-ioctl.h"
#include "util-device.h"
#include "util-misc.h"
#include "util-running-modes.h"

#include "tm-queuehandlers.h"
#include "tm-queues.h"
#include "tm-threads.h"

#include "tmqh-flow.h"

#include "conf.h"
#include "conf-yaml-loader.h"

#include "source-nfq.h"
#include "source-nfq-prototypes.h"

#include "source-nflog.h"

#include "source-ipfw.h"

#include "source-pcap.h"
#include "source-pcap-file.h"

#include "source-pfring.h"

#include "source-erf-file.h"
#include "source-erf-dag.h"
#include "source-napatech.h"

#include "source-af-packet.h"
#include "source-netmap.h"
#include "source-mpipe.h"

#include "respond-reject.h"

#include "flow.h"
#include "flow-timeout.h"
#include "flow-manager.h"
#include "flow-var.h"
#include "flow-bit.h"
#include "pkt-var.h"
#include "host-bit.h"

#include "ippair.h"
#include "ippair-bit.h"

#include "host.h"
#include "unix-manager.h"

#include "util-decode-der.h"
#include "util-radix-tree.h"
#include "util-host-os-info.h"
#include "util-cidr.h"
#include "util-unittest.h"
#include "util-unittest-helper.h"
#include "util-time.h"
#include "util-rule-vars.h"
#include "util-classification-config.h"
#include "util-threshold-config.h"
#include "util-reference-config.h"
#include "util-profiling.h"
#include "util-magic.h"
#include "util-signal.h"
#include "util-coredump-config.h"

#include "defrag.h"

#include "runmodes.h"
#include "runmode-unittests.h"

#include "util-cuda.h"
#include "util-decode-asn1.h"
#include "util-debug.h"
#include "util-error.h"
#include "util-daemon.h"
#include "reputation.h"

#include "output.h"

#include "util-privs.h"

#include "tmqh-packetpool.h"

#include "util-proto-name.h"
#ifdef __SC_CUDA_SUPPORT__
#include "util-cuda-buffer.h"
#include "util-mpm-ac.h"
#endif
#include "util-mpm-hs.h"
#include "util-storage.h"
#include "host-storage.h"

#include "util-lua.h"
#include "log-filestore.h"

#ifdef HAVE_RUST
#include "rust.h"
#include "rust-core-gen.h"
#endif
#endif

#if defined(HAVE_DPDK)
#include "dpdk.h"
#endif


#define CONFIG_PATH	"conf"
#define CONFIG_PATH_YAML CONFIG_PATH"/settings.yaml"
#define ET1500_N_XE_PORTS 2
#define ET1500_N_GE_PORTS 8

#include "tables.h"
#include "range.h"
#include "udp.h"
#include "appl.h"
#include "map.h"
#include "intf.h"


#endif
