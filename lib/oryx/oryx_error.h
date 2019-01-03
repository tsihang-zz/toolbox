/*!
 * @file oryx_error.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __ORYX_ERROR_H__
#define __ORYX_ERROR_H__

#include "error.h"

ORYX_DECLARE (
	const char *oryx_safe_strerror (
		IN int err
	)
);


#endif
