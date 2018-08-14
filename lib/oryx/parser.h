#ifndef ORYX_PARSER_H
#define ORYX_PARSER_H

ORYX_DECLARE(void oryx_pcre_initialize(void));
ORYX_DECLARE(void oryx_pcre_deinitialize(void));

#if defined(HAVE_PCRE)
ORYX_DECLARE(int oryx_str2_u8(const char *size, uint8_t *res));
ORYX_DECLARE(int oryx_str2_u16(const char *size, uint16_t *res));
ORYX_DECLARE(int oryx_str2_u32(const char *size, uint32_t *res));
ORYX_DECLARE(int oryx_str2_u64(const char *size, uint64_t *res));
#endif

#endif
