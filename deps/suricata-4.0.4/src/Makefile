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
	alert-debuglog.c alert-fastlog.c alert-prelude.c alert-syslog.c alert-unified2-alert.c \
	app-layer.c app-layer-dnp3-objects.c app-layer-dns-udp-rust.c app-layer-htp-body.c app-layer-htp-xff.c \
	app-layer-parser.c app-layer-ssh.c app-layer-dcerpc.c app-layer-dns-common.c app-layer-enip.c app-layer-htp.c \
	app-layer-modbus.c app-layer-protos.c app-layer-ssl.c app-layer-dcerpc-udp.c app-layer-dns-tcp.c app-layer-enip-common.c \
	app-layer-htp-file.c app-layer-nfs-tcp.c app-layer-smb2.c app-layer-template.c app-layer-detect-proto.c app-layer-dns-tcp-rust.c \
	app-layer-events.c app-layer-htp-libhtp.c app-layer-nfs-udp.c app-layer-smb.c app-layer-tls-handshake.c \
	app-layer-dnp3.c app-layer-dns-udp.c app-layer-ftp.c app-layer-htp-mem.c app-layer-ntp.c app-layer-smtp.c \
	conf.c  conf-yaml-loader.c  counters.c \
	data-queue.c \
	defrag.c defrag-config.c defrag-hash.c defrag-queue.c defrag-timeout.c \
	decode-afl.c decode-erspan.c decode-events.c decode-icmpv4.c decode-ipv4.c decode-mpls.c decode-ppp.c \
	decode-raw.c decode-sll.c  decode-template.c  decode-udp.c decode.c decode-ethernet.c decode-gre.c \
	decode-icmpv6.c decode-ipv6.c decode-null.c decode-pppoe.c decode-sctp.c decode-tcp.c decode-teredo.c decode-vlan.c \
	detect.c \
	detect-ssh-proto-version.c detect-tls-cert-issuer.c detect-flowvar.c detect-app-layer-protocol.c detect-engine-hrhhd.c \
	detect-http-raw-header.c detect-content.c detect-nfs-procedure.c detect-engine-iponly.c detect-dnp3.c \
	detect-engine-uri.c detect-rawbytes.c detect-target.c detect-seq.c detect-l3proto.c \
	detect-engine-hua.c detect-engine-dns.c detect-http-stat-msg.c detect-flowbits.c detect-engine-hmd.c \
	detect-http-request-line.c detect-iprep.c detect-tos.c detect-engine-hcd.c detect-flow.c \
	detect-engine-hrud.c detect-gid.c detect-lua-extensions.c detect-ttl.c detect-dns-query.c \
	detect-filesha256.c detect-template-buffer.c detect-noalert.c detect-engine-filedata-smtp.c detect-filemagic.c \
	detect-engine-address-ipv6.c detect-engine-state.c detect-engine-file.c detect-filename.c detect-filemd5.c detect-tls-version.c detect-dce-stub-data.c \
	detect-offset.c detect-http-referer.c detect-engine-hhhd.c detect-http-headers.c detect-engine-profile.c \
	detect-engine-modbus.c detect-urilen.c detect-lua.c detect-http-accept-lang.c detect-nocase.c \
	detect-mark.c detect-http-hh.c detect-ssh-software.c app-layer-detect-proto.c detect-engine-hsmd.c \
	detect-engine-sigorder.c detect-tls-cert-validity.c detect-dce-iface.c detect-engine-tag.c detect-ack.c \
	detect-engine-alert.c detect-engine-threshold.c detect-window.c detect-detection-filter.c detect-priority.c \
	detect-ssh-software-version.c detect-engine-siggroup.c detect-file-hash-common.c detect-http-stat-code.c detect-asn1.c \
	detect-id.c detect-engine-loader.c detect-engine-address-ipv4.c detect-within.c detect-engine-hscd.c \
	detect-http-content-len.c detect-icode.c detect-base64-decode.c detect-http-method.c detect-ssh-proto.c \
	detect-ipopts.c detect-icmp-seq.c detect-http-ua.c detect-flags.c detect-prefilter.c \
	detect-rpc.c detect-ssl-version.c detect-ipproto.c detect-http-uri.c detect-msg.c \
	detect-modbus.c detect-cipservice.c detect-pkt-data.c detect-base64-data.c detect-distance.c \
	detect-engine-mpm.c detect-tag.c detect-parse.c detect-http-raw-uri.c detect-reference.c \
	detect-engine-analyzer.c detect-geoip.c detect-sid.c detect-fileext.c detect-hostbits.c \
	detect-http-client-body.c detect-bypass.c detect-engine-tls.c detect-csum.c detect-http-server-body.c \
	detect-rev.c detect-http-header.c detect-http-protocol.c detect-ssl-state.c detect-pcre.c \
	detect-sameip.c detect-itype.c detect-http-accept.c detect-http-header-common.c detect-byte-extract.c detect-bytejump.c \
	detect-dce-opnum.c detect-engine-hsbd.c detect-bytetest.c detect-filesha1.c detect-pktvar.c \
	detect-engine-event.c detect-flowint.c detect-engine-dcepayload.c detect-isdataat.c detect-engine-hrhd.c \
	detect-engine-proto.c detect-dsize.c detect-engine-content-inspection.c detect-threshold.c detect-http-accept-enc.c \
	detect-xbits.c detect-http-response-line.c detect-tls-cert-subject.c detect-tls.c detect-filesize.c detect-ftpbounce.c\
	detect-http-connection.c detect-http-hrh.c detect-http-header-names.c detect-engine-port.c detect-tls-cert-serial.c detect-replace.c \
	flow-bit.c flow-hash.c flow-manager.c flow-queue.c flow-storage.c flow-timeout.c flow-util.c flow-var.c flow-worker.c \
	host-bit.c host.c host-queue.c host-storage.c host-timeout.c \
	ippair-bit.c ippair.c ippair-queue.c ippair-storage.c ippair-timeout.c \
	log-cf-common.c log-dnslog.c log-droplog.c log-file.c log-filestore.c log-httplog.c log-pcap.c log-stats.c \
	log-tcp-data.c log-tlslog.c log-tlsstore.c \
	output.c output-json-alert.c output-json-dns.c output-json-flow.c output-json-smtp.c output-json-tls.c \
	output-stats.c output-file.c output-json.c output-json-drop.c output-json-http.c output-json-ssh.c \
	output-json-vars.c output-streaming.c output-filedata.c output-json-dnp3.c output-json-email-common.c \
	output-json-netflow.c output-json-stats.c output-lua.c output-tx.c output-flow.c output-json-dnp3-objects.c \
	output-json-file.c output-json-nfs.c output-json-template.c output-packet.c \
	packet-queue.c pkt-var.c \
	reputation.c respond-reject-libnet11.c runmode-erf-dag.c runmode-ipfw.c runmode-netmap.c runmode-nfq.c \
	runmode-pcap-file.c runmodes.c runmode-unittests.c respond-reject.c runmode-af-packet.c runmode-erf-file.c \
	runmode-napatech.c runmode-nflog.c runmode-pcap.c runmode-pfring.c runmode-tile.c runmode-unix-socket.c \
	source-af-packet.c source-ipfw.c source-netmap.c source-pcap.c stream.c	stream-tcp-list.c stream-tcp-util.c \
	source-erf-dag.c source-mpipe.c	source-nflog.c source-pcap-file.c stream-tcp.c stream-tcp-reassemble.c  suricata.c \
	source-erf-file.c source-napatech.c source-nfq.c source-pfring.c stream-tcp-inline.c  stream-tcp-sack.c \
	threads.c tm-modules.c  tmqh-flow.c tmqh-nfq.c tmqh-packetpool.c tmqh-simple.c tm-queuehandlers.c tm-queues.c tm-threads.c \
	unix-manager.c \
	util-profiling-rules.c util-storage.c util-checksum.c util-cuda-vars.c util-streaming-buffer.c \
	util-bloomfilter-counting.c util-profiling-keywords.c util-classification-config.c util-unittest.c util-device.c \
	util-error.c util-lua-dns.c util-radix-tree.c util-prefilter.c util-decode-asn1.c \
	util-daemon.c util-cuda.c util-cuda-buffer.c util-buffer.c util-lua-http.c \
	util-napatech.c util-cpu.c util-decode-der-get.c util-time.c util-path.c \
	util-var.c util-profiling-locks.c util-lua.c util-decode-mime.c util-fix_checksum.c \
	util-lua-tls.c util-atomic.c util-bloomfilter.c util-random.c util-profiling-rulegroups.c \
	util-spm-bs.c util-conf.c util-proto-name.c util-byte.c util-mpm-ac.c \
	util-privs.c util-hash.c util-crypt.c util-lua-dnp3-objects.c util-magic.c \
	util-profiling.c util-mpm-ac-bs.c util-running-modes.c util-unittest-helper.c util-strlcatu.c \
	util-print.c util-spm-bs2bm.c util-lua-common.c util-runmodes.c util-signal.c \
	util-luajit.c util-hash-lookup3.c util-memcmp.c util-lua-smtp.c util-hyperscan.c \
	util-file.c util-spm-bm.c util-pool-thread.c util-debug-filters.c \
	util-ioctl.c util-pidfile.c util-syslog.c util-lua-ssh.c util-strlcpyu.c \
	util-misc.c util-mpm.c util-pages.c util-threshold-config.c util-mpm-ac-tile-small.c \
	util-log-redis.c util-debug.c util-host-info.c util-var-name.c util-rule-vars.c \
	util-decode-der.c util-ip.c util-lua-dnp3.c util-hashlist.c util-mpm-hs.c \
	util-reference-config.c \
	win32-misc.c  win32-service.c

CFLAGS += -O3 --std=gnu99
CFLAGS += -D_GNU_SOURCE -D__USE_GNU -D__STDC_FORMAT_MACROS
CFLAGS += $(WERROR_FLAGS)

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
ifeq ($(CONFIG_RTE_LIBRTE_IXGBE_PMD),y)
LDLIBS += -lrte_pmd_ixgbe
endif
endif

include $(RTE_SDK)/mk/rte.extlib.mk
