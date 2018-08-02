
#include "oryx.h"

#if defined(HAVE_PCRE)
#define PARSE_REGEX "^\\s*(\\d+(?:.\\d+)?)\\s*([a-zA-Z]{2})?\\s*$"
static pcre *parse_regex = NULL;
static pcre_extra *parse_regex_study = NULL;
#endif

void oryx_pcre_initialize(void)
{
#if defined(HAVE_PCRE)
    const char *eb = NULL;
    int eo;
    int opts = 0;

    parse_regex = pcre_compile(PARSE_REGEX, opts, &eb, &eo, NULL);
    if (parse_regex == NULL) {
		oryx_panic(0,
			"Compile of \"%s\" failed at offset "
                   "%" PRId32 ": %s", PARSE_REGEX, eo, eb);
    }
    parse_regex_study = pcre_study(parse_regex, 0, &eb);
    if (eb != NULL) {
		oryx_panic(0,
			"pcre study failed: %s", eb);
    }
#endif
}

void oryx_pcre_deinitialize(void)
{
#if defined(HAVE_PCRE)
    if (parse_regex != NULL)
        pcre_free(parse_regex);
    if (parse_regex_study != NULL)
        pcre_free_study(parse_regex_study);
#endif
}

#if defined(HAVE_PCRE)
/* size string parsing API */
static int ParseSizeString(const char *size, double *res)
{
#define MAX_SUBSTRINGS 30
    int pcre_exec_ret;
    int r;
    int ov[MAX_SUBSTRINGS];
    int retval = 0;
    char str[128];
    char str2[128];

    *res = 0;

    if (size == NULL) {
        oryx_loge(0,
			"invalid size argument - NULL. Valid size "
                   "argument should be in the format - \n"
                   "xxx <- indicates it is just bytes\n"
                   "xxxkb or xxxKb or xxxKB or xxxkB <- indicates kilobytes\n"
                   "xxxmb or xxxMb or xxxMB or xxxmB <- indicates megabytes\n"
                   "xxxgb or xxxGb or xxxGB or xxxgB <- indicates gigabytes.\n"
			    );
        retval = -2;
        goto end;
    }

    pcre_exec_ret = pcre_exec(parse_regex, parse_regex_study, size, strlen(size), 0, 0,
                    ov, MAX_SUBSTRINGS);
    if (!(pcre_exec_ret == 2 || pcre_exec_ret == 3)) {
        oryx_loge(0,
			"invalid size argument - %s. Valid size "
                   "argument should be in the format - \n"
                   "xxx <- indicates it is just bytes\n"
                   "xxxkb or xxxKb or xxxKB or xxxkB <- indicates kilobytes\n"
                   "xxxmb or xxxMb or xxxMB or xxxmB <- indicates megabytes\n"
                   "xxxgb or xxxGb or xxxGB or xxxgB <- indicates gigabytes.\n",
                   size);
        retval = -2;
        goto end;
    }

    r = pcre_copy_substring((char *)size, ov, MAX_SUBSTRINGS, 1,
                             str, sizeof(str));
    if (r < 0) {
        oryx_loge(0,
			"pcre_copy_substring failed");
        retval = -2;
        goto end;
    }

    char *endptr, *str_ptr = str;
    errno = 0;
    *res = strtod(str_ptr, &endptr);
    if (errno == ERANGE) {
        oryx_loge(0,
			"Numeric value out of range");
        retval = -1;
        goto end;
    } else if (endptr == str_ptr) {
        oryx_loge(0,
			"Invalid numeric value");
        retval = -1;
        goto end;
    }

    if (pcre_exec_ret == 3) {
        r = pcre_copy_substring((char *)size, ov, MAX_SUBSTRINGS, 2,
                                 str2, sizeof(str2));
        if (r < 0) {
            oryx_loge(0,
				"pcre_copy_substring failed");
            retval = -2;
            goto end;
        }

        if (strcasecmp(str2, "kb") == 0) {
            *res *= 1024;
        } else if (strcasecmp(str2, "mb") == 0) {
            *res *= 1024 * 1024;
        } else if (strcasecmp(str2, "gb") == 0) {
            *res *= 1024 * 1024 * 1024;
        } else {
            /* Bad unit. */
            retval = -1;
            goto end;
        }
    }

    retval = 0;
end:
    return retval;
}

int oryx_str2_u8(const char *size, uint8_t *res)
{
    double temp_res = 0;

    *res = 0;
    int r = ParseSizeString(size, &temp_res);
    if (r < 0)
        return r;

    if (temp_res > UINT8_MAX)
        return -1;

    *res = temp_res;

    return 0;
}

int oryx_str2_u16(const char *size, uint16_t *res)
{
    double temp_res = 0;

    *res = 0;
    int r = ParseSizeString(size, &temp_res);
    if (r < 0)
        return r;

    if (temp_res > UINT16_MAX)
        return -1;

    *res = temp_res;

    return 0;
}

int oryx_str2_u32(const char *size, uint32_t *res)
{
    double temp_res = 0;

    *res = 0;
    int r = ParseSizeString(size, &temp_res);
    if (r < 0)
        return r;

    if (temp_res > UINT32_MAX)
        return -1;

    *res = temp_res;

    return 0;
}

int oryx_str2_u64(const char *size, uint64_t *res)
{
    double temp_res = 0;

    *res = 0;
    int r = ParseSizeString(size, &temp_res);
    if (r < 0)
        return r;

    if (temp_res > UINT64_MAX)
        return -1;

    *res = temp_res;

    return 0;
}
#endif