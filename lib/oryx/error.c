#include "oryx.h"

const char *
oryx_safe_strerror
(
	IN int err
)
{
  const char *s = strerror(err);
  return (s != NULL) ? s : "Unknown error";
}

