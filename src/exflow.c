#include "suricata-common.h"
#include "suricata.h"
#include "decode.h"
#include "conf.h"
#include "threadvars.h"
#include "tm-threads.h"
#include "runmodes.h"

#include "util-random.h"
#include "util-time.h"

#include "flow.h"
#include "flow-queue.h"
#include "flow-hash.h"
#include "flow-util.h"
#include "flow-var.h"
#include "flow-private.h"
#include "flow-timeout.h"
#include "flow-manager.h"
#include "flow-storage.h"

#include "stream-tcp-private.h"
#include "stream-tcp-reassemble.h"
#include "stream-tcp.h"

#include "util-unittest.h"
#include "util-unittest-helper.h"
#include "util-byte.h"
#include "util-misc.h"

#include "util-debug.h"
#include "util-privs.h"

#include "detect.h"
#include "detect-engine-state.h"

#include "app-layer-parser.h"

void read_settings()
{
	FlowConfig fc;
	int retval;
	
    /* Check if we have memcap and hash_size defined at config */
    const char *conf_val;
	
    if ((retval = ConfGet("flow.memcap", &conf_val)) == 1)
    {
    	SCLogNotice("conf_val0= %s, retval= %d", conf_val, retval);
        if ((retval = ParseSizeStringU64(conf_val, &fc.memcap)) < 0) {
	        SCLogError(SC_ERR_SIZE_PARSE, "Error parsing flow.memcap "
	                   "from conf file - %s[%d].  Killing engine",
	                   conf_val, retval);
	        exit(EXIT_FAILURE);
	    }
    }
}

#if defined(HAVE_FLOW_MGR)

#endif