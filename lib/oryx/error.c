/*!
 * @file error.c
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#include "oryx.h"

__oryx_always_extern__
const char * oryx_safe_strerror
(
	IN int err
)
{
	const char *s = strerror(err);
	return (s != NULL) ? s : "Unknown error";
}

