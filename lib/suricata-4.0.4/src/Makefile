#   BSD LICENSE
#
#   Copyright(c) 2015 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overwritten by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(CONFIG_RTE_EXEC_ENV),"linuxapp")
$(error This application can only operate in a linuxapp environment, \
please change the definition of the RTE_TARGET environment variable)
endif

# library name
LIB = libsuricata.a

LIBABIVER := 1

# all source are stored in SRC-Y
SRCS-y :=\
	alert-debuglog.c alert-fastlog.c alert-prelude.c  alert-syslog.c alert-unified2-alert.c app-layer.c\
	app-layer-dcerpc.c app-layer-dcerpc-udp.c app-layer-detect-proto.c app-layer-dnp3.c app-layer-dnp3-objects.c\
	app-layer-dns-common.c app-layer-dns-tcp.c app-layer-dns-tcp-rust.c app-layer-dns-udp.c\
	app-layer-dns-udp-rust.c app-layer-enip.c app-layer-enip-common.c app-layer-events.c app-layer-ftp.c\
	app-layer-htp-body.c app-layer-htp.c app-layer-htp-file.c app-layer-htp-libhtp.c app-layer-htp-mem.c\
	app-layer-htp-xff.c  app-layer-modbus.c app-layer-nfs-tcp.c app-layer-nfs-udp.c app-layer-ntp.c\
	app-layer-parser.c app-layer-protos.c app-layer-smb2.c app-layer-smb.c app-layer-smtp.c\
	app-layer-ssh.c app-layer-ssl.c app-layer-template.c app-layer-tls-handshake.c\
	conf.c conf-yaml-loader.c counters.c\
	data-queue.c decode-afl.c\
	decode.c decode-erspan.c decode-ethernet.c decode-events.c decode-gre.c decode-icmpv4.c decode-icmpv6.c\
	decode-ipv4.c decode-ipv6.c decode-mpls.c decode-null.c decode-ppp.c decode-pppoe.c decode-raw.c\
	decode-sctp.c decode-sll.c decode-tcp.c decode-template.c decode-teredo.c decode-udp.c decode-vlan.c\
	defrag.c defrag-config.c defrag-hash.c defrag-queue.c defrag-timeout.c detect-ack.c detect-app-layer-event.c\
	detect-app-layer-protocol.c detect-asn1.c detect-base64-data.c detect-base64-decode.c detect-bypass.c\
	detect-byte-extract.c detect-bytejump.c detect-bytetest.c detect.c detect-cipservice.c detect-classtype.c\
	detect-content.c detect-csum.c detect-dce-iface.c detect-dce-opnum.c detect-dce-stub-data.c detect-depth.c\
	detect-detection-filter.c detect-distance.c detect-dnp3.c detect-dns-query.c detect-dsize.c\
	detect-engine-address.c detect-engine-address-ipv4.c detect-engine-address-ipv6.c detect-engine-alert.c\
	detect-engine-analyzer.c detect-engine.c detect-engine-content-inspection.c detect-engine-dcepayload.c\
	detect-engine-dns.c detect-engine-enip.c detect-engine-event.c detect-engine-file.c\
	detect-engine-filedata-smtp.c detect-engine-hcbd.c detect-engine-hcd.c detect-engine-hhhd.c\
	detect-engine-hmd.c detect-engine-hrhd.c detect-engine-hrhhd.c detect-engine-hrud.c detect-engine-hsbd.c\
	detect-engine-hscd.c detect-engine-hsmd.c detect-engine-hua.c detect-engine-iponly.c detect-engine-loader.c\
	detect-engine-modbus.c detect-engine-mpm.c detect-engine-payload.c detect-engine-port.c\
	detect-engine-prefilter.c detect-engine-prefilter-common.c detect-engine-profile.c detect-engine-proto.c\
	detect-engine-siggroup.c detect-engine-sigorder.c detect-engine-state.c detect-engine-tag.c\
	detect-engine-threshold.c detect-engine-tls.c detect-engine-uri.c detect-fast-pattern.c\
	detect-file-data.c detect-fileext.c detect-file-hash-common.c detect-filemagic.c detect-filemd5.c\
	detect-filename.c detect-filesha1.c detect-filesha256.c detect-filesize.c detect-filestore.c\
	detect-flags.c detect-flowbits.c detect-flow.c detect-flowint.c detect-flowvar.c detect-fragbits.c\
	detect-fragoffset.c detect-ftpbounce.c detect-geoip.c detect-gid.c detect-hostbits.c\
	detect-http-accept.c detect-http-accept-enc.c detect-http-accept-lang.c\
	detect-http-client-body.c detect-http-connection.c detect-http-content-len.c detect-http-content-type.c\
	detect-http-cookie.c detect-http-header.c detect-http-header-common.c detect-http-header-names.c\
	detect-http-headers.c detect-http-hh.c detect-http-hrh.c detect-http-method.c detect-http-protocol.c\
	detect-http-raw-header.c detect-http-raw-uri.c detect-http-referer.c\
	detect-http-request-line.c detect-http-response-line.c detect-http-server-body.c detect-http-start.c detect-http-stat-code.c\
	detect-http-stat-msg.c detect-http-ua.c detect-http-uri.c detect-icmp-id.c detect-icmp-seq.c detect-icode.c detect-id.c\
	detect-ipopts.c detect-ipproto.c detect-iprep.c detect-isdataat.c detect-itype.c detect-l3proto.c detect-lua.c detect-lua-extensions.c\
	detect-mark.c detect-metadata.c detect-modbus.c detect-msg.c detect-nfs-procedure.c detect-nfs-version.c detect-noalert.c detect-nocase.c\
	detect-offset.c detect-parse.c detect-pcre.c detect-pkt-data.c detect-pktvar.c detect-prefilter.c detect-priority.c detect-rawbytes.c\
	detect-reference.c detect-replace.c detect-rev.c detect-rpc.c detect-sameip.c detect-seq.c detect-sid.c detect-ssh-proto.c\
	detect-ssh-proto-version.c detect-ssh-software.c detect-ssh-software-version.c detect-ssl-state.c detect-ssl-version.c\
	detect-stream_size.c detect-tag.c detect-target.c detect-template-buffer.c detect-template.c detect-threshold.c\
	detect-tls.c detect-tls-cert-issuer.c detect-tls-cert-serial.c detect-tls-cert-subject.c detect-tls-cert-validity.c detect-tls-sni.c\
	detect-tls-version.c detect-tos.c detect-ttl.c detect-uricontent.c detect-urilen.c detect-window.c detect-within.c detect-xbits.c\
	flow-bit.c flow.c flow-hash.c flow-manager.c  flow-queue.c flow-storage.c flow-timeout.c flow-util.c flow-var.c flow-worker.c\
	host-bit.c host.c host-queue.c host-storage.c host-timeout.c\
	ippair-bit.c ippair.c ippair-queue.c ippair-storage.c ippair-timeout.c log-cf-common.c log-dnslog.c\
	log-droplog.c log-file.c log-filestore.c log-httplog.c log-pcap.c log-stats.c log-tcp-data.c log-tlslog.c log-tlsstore.c\
	output.c output-file.c output-filedata.c output-flow.c output-json-alert.c output-json.c output-json-dnp3.c output-json-dnp3-objects.c\
	output-json-dns.c output-json-drop.c output-json-email-common.c output-json-file.c output-json-flow.c\
	output-json-http.c output-json-netflow.c output-json-nfs.c output-json-smtp.c output-json-ssh.c output-json-stats.c output-json-template.c\
	output-json-tls.c output-json-vars.c output-lua.c output-packet.c output-stats.c output-streaming.c output-tx.c\
	packet-queue.c pkt-var.c\
	reputation.c respond-reject.c respond-reject-libnet11.c\
	runmode-af-packet.c runmode-erf-dag.c runmode-erf-file.c runmode-ipfw.c runmode-napatech.c runmode-netmap.c runmode-nflog.c\
	runmode-nfq.c runmode-pcap.c runmode-pcap-file.c runmode-pfring.c runmodes.c runmode-tile.c runmode-unittests.c runmode-unix-socket.c\
	source-af-packet.c source-erf-dag.c source-erf-file.c source-ipfw.c source-mpipe.c source-napatech.c source-netmap.c source-nflog.c\
	source-nfq.c source-pcap.c source-pcap-file.c source-pfring.c\
	stream.c stream-tcp.c stream-tcp-inline.c stream-tcp-list.c stream-tcp-reassemble.c stream-tcp-sack.c stream-tcp-util.c\
	suricata.c\
	threads.c\
	tm-modules.c tmqh-flow.c tmqh-nfq.c tmqh-packetpool.c tmqh-simple.c tm-queuehandlers.c tm-queues.c tm-threads.c\
	unix-manager.c\
	util-action.c util-affinity.c util-atomic.c util-base64.c util-bloomfilter.c util-bloomfilter-counting.c util-buffer.c\
	util-byte.c util-checksum.c util-cidr.c util-classification-config.c util-conf.c util-coredump-config.c\
	util-cpu.c util-crypt.c util-cuda-buffer.c util-cuda.c util-cuda-handlers.c util-cuda-vars.c util-daemon.c\
	util-debug.c util-debug-filters.c util-decode-asn1.c util-decode-der.c util-decode-der-get.c\
	util-decode-mime.c util-device.c util-enum.c util-error.c util-file.c util-fix_checksum.c util-fmemopen.c util-hash.c\
	util-hashlist.c util-hash-lookup3.c util-host-info.c util-host-os-info.c util-hyperscan.c util-ioctl.c\
	util-ip.c util-logopenfile.c util-logopenfile-tile.c util-log-redis.c util-lua.c util-lua-common.c\
	util-lua-dnp3.c util-lua-dnp3-objects.c util-lua-dns.c util-lua-http.c util-luajit.c\
	util-lua-smtp.c util-lua-ssh.c util-lua-tls.c util-magic.c util-memcmp.c util-memrchr.c\
	util-misc.c util-mpm-ac-bs.c util-mpm-ac.c util-mpm-ac-tile.c util-mpm-ac-tile-small.c util-mpm.c\
	util-mpm-hs.c util-napatech.c util-pages.c util-path.c util-pidfile.c util-pool.c util-pool-thread.c\
	util-prefilter.c util-print.c util-privs.c util-profiling.c util-profiling-keywords.c util-profiling-locks.c\
	util-profiling-rulegroups.c util-profiling-rules.c util-proto-name.c util-radix-tree.c util-random.c\
	util-reference-config.c util-rohash.c util-rule-vars.c util-runmodes.c util-running-modes.c\
	util-signal.c util-spm-bm.c util-spm-bs2bm.c util-spm-bs.c util-spm.c util-spm-hs.c\
	util-storage.c util-streaming-buffer.c util-strlcatu.c util-strlcpyu.c util-syslog.c util-threshold-config.c\
	util-time.c util-unittest.c util-unittest-helper.c util-var.c util-var-name.c win32-misc.c win32-service.c


#CFLAGS += $(WERROR_FLAGS)
CFLAGS_SURICATA := -W -Wstrict-prototypes -Wmissing-prototypes
CFLAGS_SURICATA += -Wmissing-declarations
CFLAGS_SURICATA += -Wcast-align -Wnested-externs
CFLAGS_SURICATA += -Wformat-nonliteral -Wformat-security
CFLAGS_SURICATA += -Wundef -Wwrite-strings
#CFLAGS_SURICATA += -Wold-style-definition -Wcast-qual -Wpointer-arith

CFLAGS += -g -O3 --std=gnu99 $(CFLAGS_SURICATA)
CFLAGS += -D_GNU_SOURCE -D__USE_GNU -D__STDC_FORMAT_MACROS

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
ifeq ($(CONFIG_RTE_LIBRTE_IXGBE_PMD),y)
LDLIBS += -lrte_pmd_ixgbe
endif
endif

include $(RTE_SDK)/mk/rte.extlib.mk
