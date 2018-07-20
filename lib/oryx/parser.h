#ifndef ORYX_PARSER_H
#define ORYX_PARSER_H

void oryx_pcre_initialize(void);
void oryx_pcre_deinitialize(void);

#if defined(HAVE_PCRE)
int oryx_str2_u8(const char *size, uint8_t *res);
int oryx_str2_u16(const char *size, uint16_t *res);
int oryx_str2_u32(const char *size, uint32_t *res);
int oryx_str2_u64(const char *size, uint64_t *res);
#endif




#endif
