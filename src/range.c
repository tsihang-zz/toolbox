#include "oryx.h"
#include "et1500.h"


static int isalldigit_skip(const char *str, u8 skip_char)
{
    int i;

	if (!str) return 0;

	for (i = 0; i < (int)strlen(str); i++)
    {
    	if (str[i] == skip_char)
		    continue;

		if (!isdigit(str[i]))
            return 0;
    }
	
    return 1;
}

void range_get (char *keyword_range_str, struct range_t *r)
{
	r->ul_start = r->ul_end = r->uc_ready = 0;
	
	if (isalldigit(keyword_range_str)) {
		sscanf(keyword_range_str, "%d", &r->ul_start);
		r->ul_end = r->ul_start;
		r->uc_ready = 1;
	} else {
		if (isalldigit_skip(keyword_range_str, '-')) {
			sscanf(keyword_range_str, "%d-%d", &r->ul_start, &r->ul_end);
			r->uc_ready = 1;
		}
	}

}

