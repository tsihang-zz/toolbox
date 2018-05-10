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

#
# all sources are stored in SRCS-y
#
SRCS-y += \
	main.c dpdk.c \
	init.c

SRCS-y += version.c

CFLAGS += -O3
CFLAGS += -DHAVE_DPDK -DHAVE_QUAGGA -DHAVE_SURICATA
CFLAGS += -D_GNU_SOURCE -D__USE_GNU -D__STDC_FORMAT_MACROS
CFLAGS += -I$(SRCDIR)/../lib -I$(SRCDIR)/../deps/quagga -I$(SRCDIR)/../deps/suricata/src
CFLAGS += $(WERROR_FLAGS)

LDLIBS += -L$(subst src,lib,$(RTE_OUTPUT))/lib
LDLIBS += -loryx

LDLIBS += -L$(subst src,deps/quagga,$(RTE_OUTPUT))/lib
LDLIBS += -lquagga

LDLIBS += -L$(subst src,deps/suricata/src,$(RTE_OUTPUT))/lib
LDLIBS += -lsuricata

include $(RTE_SDK)/mk/rte.extapp.mk

endif
