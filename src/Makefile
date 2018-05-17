#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

include $(RTE_SDK)/mk/rte.vars.mk

ifeq ($(CONFIG_RTE_LIBRTE_CMDLINE),y)

#
# library name
#
APP = et1500

SRCS-cli += \
	cli_iface.c cli_map.c cli_udp.c cli_appl.c\
	cli_common.c

SRCS-dpdk += \
	dpdk.c dpdk_init.c

SRCS-decode += \
	dp_decode_gre.c dp_decode_eth.c dp_decode_mpls.c dp_decode_ppp.c dp_decode_pppoe.c \
	dp_decode_arp.c dp_decode_ipv4.c dp_decode_ipv6.c dp_decode_vlan.c \
	dp_decode_icmpv4.c dp_decode_icmpv6.c dp_decode_udp.c dp_decode_tcp.c dp_decode_sctp.c
#
# all sources are stored in SRCS-y
#
SRCS-y += \
	main.c rc4.c threadvars.c event.c\
	dp_main.c dp_decode.c dp_flow.c dp_flow_queue.c\
	$(SRCS-dpdk) \
	$(SRCS-cli) \
	$(SRCS-decode) \
	dp_storage.c

#CFLAGS += $(WERROR_FLAGS)
CFLAGS_SRC := -W -Wstrict-prototypes -Wmissing-prototypes
CFLAGS_SRC += -Wmissing-declarations -Wpointer-arith
CFLAGS_SRC += -Wcast-align -Wnested-externs
CFLAGS_SRC += -Wformat-nonliteral -Wformat-security
CFLAGS_SRC += -Wundef -Wwrite-strings
#CFLAGS_SRC += -Wall -Wold-style-definition -Wcast-qual
CFLAGS += -g -O3 --std=gnu99 $(CFLAGS_SRC)
CFLAGS += -D_GNU_SOURCE -D__USE_GNU -D__STDC_FORMAT_MACROS\
		  -DHAVE_BACKTRACE\
		  -DHAVE_PCAP -DHAVE_ADVANCED_TMR\
		  -DHAVE_DPDK
CFLAGS += -I$(SRCDIR)\
		  -I$(SRCDIR)/include\
		  -I$(SRCDIR)/../lib\
		  -I$(SRCDIR)/../lib/quagga\
		  -I$(SRCDIR)/../lib/oryx

LDFLAGS += -lstdc++ -ldl -lpthread -lm -lrt\
		   -lhs -lpcre -lyaml -lpcap


LDLIBS += -L$(subst src,lib/oryx,$(RTE_OUTPUT))/lib
LDLIBS += -loryx

LDLIBS += -L$(subst src,lib/quagga,$(RTE_OUTPUT))/lib
LDLIBS += -lquagga

#LDLIBS += -L$(subst src,lib/suricata/src,$(RTE_OUTPUT))/lib
#LDLIBS += -lsuricata

include $(RTE_SDK)/mk/rte.extapp.mk

endif
