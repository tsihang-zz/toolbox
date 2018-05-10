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
	detect-ssh-proto-version.c util-profiling-rules.c util-storage.c detect-tls-cert-issuer.c util-checksum.c \
	output-json-stats.c util-cuda-vars.c detect-flowvar.c decode-pppoe.c detect-app-layer-protocol.c \
	util-streaming-buffer.c app-layer-htp-file.c util-rohash.c detect-engine-hrhhd.c detect-http-start.c \
	detect-http-raw-header.c tm-modules.c log-pcap.c util-bloomfilter-counting.c log-file.c \
	detect-content.c detect-nfs-procedure.c detect-engine-iponly.c respond-reject-libnet11.c detect-dnp3.c \
	detect-engine-uri.c output-json-dnp3-objects.c app-layer-ntp.c packet-queue.c util-profiling-keywords.c \
	detect-target.c app-layer-dnp3-objects.c output-json.c detect-seq.c output-json-dnp3.c \
	detect-template.c detect-engine-hua.c detect-engine-dns.c detect-http-stat-msg.c detect-flowbits.c \
	util-classification-config.c util-unittest.c counters.c detect-tls-sni.c util-device.c \
	decode-afl.c flow-worker.c util-base64.c detect-http-request-line.c detect-iprep.c \
	detect-tos.c suricata.c runmode-netmap.c detect-engine-hcd.c detect-flow.c \
	decode-icmpv6.c detect-http-cookie.c output-tx.c decode-gre.c detect-engine-hrud.c \
	detect-gid.c stream-tcp-sack.c util-lua-dns.c source-ipfw.c detect-lua-extensions.c \
	util-radix-tree.c detect-dns-query.c detect-engine-prefilter-common.c detect-filesha256.c detect-template-buffer.c \
	detect-noalert.c alert-debuglog.c source-netmap.c decode-ppp.c detect-engine-filedata-smtp.c \
	detect-filemagic.c detect-stream_size.c detect-engine-address-ipv6.c detect-engine-state.c tm-threads.c \
	app-layer-parser.c util-decode-asn1.c output.c detect-filename.c detect-tls-version.c \
	host.c detect-dce-stub-data.c app-layer-events.c detect-engine-hcbd.c runmode-unittests.c \
	util-memrchr.c data-queue.c defrag-config.c tmqh-simple.c util-daemon.c \
	app-layer-ssl.c util-cuda.c output-json-ssh.c detect-http-referer.c output-file.c \
	win32-misc.c app-layer-ftp.c util-cuda-buffer.c detect-http-headers.c app-layer-protos.c \
	detect-engine-profile.c util-lua-http.c util-enum.c ippair-bit.c detect-uricontent.c \
	detect-engine-modbus.c detect-urilen.c util-napatech.c detect-lua.c detect-http-accept-lang.c \
	detect-nocase.c log-httplog.c detect-fast-pattern.c detect-mark.c util-cpu.c \
	detect-http-hh.c app-layer-tls-handshake.c detect-ssh-software.c app-layer-detect-proto.c source-erf-file.c \
	log-droplog.c unix-manager.c flow-hash.c detect-engine-hsmd.c detect-nfs-version.c \
	util-decode-der-get.c detect-tls-cert-validity.c app-layer-dns-tcp.c detect-dce-iface.c decode.c \
	util-time.c detect-ack.c detect-fragoffset.c detect-engine-alert.c detect-engine-threshold.c \
	detect-detection-filter.c log-dnslog.c stream-tcp-util.c util-path.c detect-priority.c \
	detect-engine-enip.c decode-raw.c detect-ssh-software-version.c host-queue.c detect-engine-siggroup.c \
	app-layer-nfs-udp.c detect-http-stat-code.c detect-asn1.c util-cidr.c detect-depth.c \
	util-var.c flow-util.c util-profiling-locks.c threads.c ippair-timeout.c \
	util-lua.c detect-engine-address-ipv4.c decode-ipv6.c host-storage.c detect-within.c \
	app-layer-htp.c detect-engine-hscd.c detect-classtype.c app-layer-dcerpc-udp.c util-decode-mime.c \
	app-layer.c util-fix_checksum.c decode-tcp.c detect-http-content-len.c flow-bit.c \
	app-layer-htp-body.c util-host-os-info.c detect-base64-decode.c util-lua-tls.c detect-http-method.c \
	output-streaming.c stream-tcp-list.c util-bloomfilter.c decode-icmpv4.c detect-ssh-proto.c \
	log-filestore.c detect-fragbits.c detect-ipopts.c detect-icmp-seq.c util-random.c \
	util-pool.c decode-teredo.c detect-http-ua.c flow-var.c app-layer-smb2.c \
	util-spm-bs.c app-layer-dnp3.c flow.c source-af-packet.c detect-flags.c \
	detect-prefilter.c tmqh-flow.c app-layer-htp-libhtp.c app-layer-dns-udp.c detect-http-content-type.c \
	app-layer-enip-common.c util-conf.c util-proto-name.c reputation.c decode-ipv4.c \
	decode-udp.c util-byte.c output-json-drop.c decode-ethernet.c runmode-napatech.c \
	app-layer-enip.c util-mpm-ac.c util-spm-hs.c detect-ipproto.c app-layer-dns-common.c \
	output-json-tls.c util-hash.c util-crypt.c detect-http-uri.c detect-msg.c \
	detect-modbus.c ippair.c detect-cipservice.c source-nfq.c flow-storage.c \
	output-json-alert.c conf-yaml-loader.c detect-base64-data.c runmodes.c app-layer-dns-tcp-rust.c \
	runmode-unix-socket.c decode-sll.c util-magic.c util-coredump-config.c decode-mpls.c \
	util-mpm-ac-bs.c runmode-nfq.c util-running-modes.c runmode-tile.c util-unittest-helper.c \
	util-strlcatu.c detect-distance.c detect-metadata.c util-action.c detect-engine-mpm.c \
	util-print.c detect-tag.c detect-parse.c defrag.c alert-syslog.c \
	util-lua-common.c detect-http-raw-uri.c util-runmodes.c detect-reference.c win32-service.c \
	util-signal.c util-cuda-handlers.c runmode-erf-file.c source-pfring.c stream.c \
	detect-engine-analyzer.c detect-geoip.c util-hash-lookup3.c util-memcmp.c app-layer-smtp.c \
	detect-sid.c util-lua-smtp.c util-hyperscan.c detect-fileext.c util-fmemopen.c \
	detect-hostbits.c detect-app-layer-event.c util-file.c tm-queues.c util-spm-bm.c \
	output-json-http.c detect-bypass.c stream-tcp.c util-pool-thread.c detect-engine-tls.c \
	host-timeout.c output-flow.c log-stats.c app-layer-dns-udp-rust.c \
	util-debug-filters.c ippair-storage.c detect-engine.c output-json-smtp.c util-mpm-ac-tile.c \
	util-ioctl.c output-json-email-common.c app-layer-htp-xff.c detect-rev.c detect-http-header.c \
	tmqh-packetpool.c detect-http-protocol.c detect-ssl-state.c detect-pcre.c detect-engine-prefilter.c \
	detect-itype.c detect-http-accept.c app-layer-dcerpc.c host-bit.c detect-http-header-common.c \
	util-syslog.c util-lua-ssh.c source-mpipe.c util-strlcpyu.c detect-byte-extract.c \
	detect-engine-address.c util-logopenfile.c detect-dce-opnum.c util-misc.c detect-engine-hsbd.c \
	detect-filesha1.c detect-pktvar.c output-json-flow.c util-mpm.c util-pages.c \
	util-threshold-config.c ippair-queue.c detect-engine-event.c util-mpm-ac-tile-small.c detect-flowint.c \
	detect-engine-dcepayload.c detect-isdataat.c decode-null.c detect-engine-hrhd.c detect-icmp-id.c \
	pkt-var.c detect-engine-proto.c alert-unified2-alert.c util-log-redis.c output-json-netflow.c \
	util-host-info.c log-tcp-data.c flow-timeout.c util-var-name.c log-tlslog.c \
	detect-engine-content-inspection.c util-rule-vars.c respond-reject.c output-stats.c detect-threshold.c \
	detect-http-accept-enc.c detect-engine-payload.c decode-template.c util-affinity.c util-decode-der.c \
	detect-http-response-line.c detect-tls-cert-subject.c detect-tls.c runmode-pfring.c detect-filesize.c \
	detect-http-connection.c runmode-ipfw.c util-ip.c util-lua-dnp3.c detect-http-header-names.c \
	detect-engine-port.c detect-tls-cert-serial.c tmqh-nfq.c source-napatech.c detect-replace.c \
	flow-manager.c output-json-vars.c log-cf-common.c util-hashlist.c detect-file-data.c \
	util-mpm-hs.c util-spm.c stream-tcp-inline.c detect-filestore.c decode-erspan.c


CFLAGS += -O3 --std=gnu99
CFLAGS += -D_GNU_SOURCE -D__USE_GNU -D__STDC_FORMAT_MACROS
CFLAGS += $(WERROR_FLAGS)

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
ifeq ($(CONFIG_RTE_LIBRTE_IXGBE_PMD),y)
LDLIBS += -lrte_pmd_ixgbe
endif
endif

LDLIBS += -lstdc++ -ldl -lpthread -lm -lrt
LDLIBS += -lhs -lpcre -lhtp  -lapr-1 -laprutil-1 -lyaml

include $(RTE_SDK)/mk/rte.extlib.mk
