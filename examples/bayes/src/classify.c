/**
	Classifier by Naive Bayes
 */

#include "oryx.h"
#if 0
#define NB_PROPERTY_VECS	10000
#define NB_CHARS_PER_VOC	100

struct nb_classfier_t {
	int	_count;
	char	property_vector[NB_VOCS][NB_CHARS_PER_VOC];	
};

int nb_split_word (struct nb_classfier_t *nbc, char *text)
{
	
	/** split tokens */
	char *split = ".,\n";
	char *substring = NULL;

	do {
		substring = strtok (text, split);
		if (substring) {
			int size = strlen(substring);
			if (size >= NB_CHARS_PER_VOC)
				size = (NB_CHARS_PER_VOC - 1);	
			strcpy (nbc->voc_vector[nbc->voc_count], size);
		}

#ifdef NB_DEBUG
		if (substring) fprintf (stdout, "%s\n", nbc->voc_vector[nbc->voc_count]);
#endif

		substring = (NULL, split);
	} while (substring);

	return nbc->voc_count;
}
#endif

//#define NB_DEBUG

enum nb_category_t {
	NB_NO,
	NB_YES,
	NB_CATE_SIZE
};

struct indent_t {
	int col;
	char * section;
	int id;
	char * ci;
	int count;	/** Unused */
	float nb_post_proba[NB_CATE_SIZE];

};

const struct indent_t outlook[4] = {
	{1,	"Sunny",	1,	"yes",	0,	{0.00, 0.00}},
	{1,	"Overcast", 	2,	"yes",	0,	{0.00, 0.00}},
	{1,	"Rain",		3,	"yes",	0,	{0.00, 0.00}},
	{1,	NULL, 		0,	NULL,	0,	{0.00, 0.00}}
};

const struct indent_t temperature[4] = {
	{2,	"Hot",		1,	"yes",	0,	{0.00, 0.00}},
	{2,	"Mild", 	2,	"yes",	0,	{0.00, 0.00}},
	{2,	"Cool",		3,	"yes",	0,	{0.00, 0.00}},
	{2,	NULL,		0,	NULL,	0,	{0.00, 0.00}}
};

const struct indent_t humidity[3] = {
	{3,	"High",		1,	"yes",	0,	{0.00, 0.00}},
	{3,	"Normal",	2,	"yes",	0,	{0.00, 0.00}},
	{3,	NULL,		0,	NULL,	0,	{0.00, 0.00}}
};

const struct indent_t wind[3] = {
	{4,	"Strong",	1,	"yes",	0,	{0.00, 0.00}},
	{4,	"Weak",		2,	"yes",	0,	{0.00, 0.00}},
	{4,	NULL,		0,	NULL,	0,	{0.00, 0.00}}
};

/** Restor count. */
int **_outlook;
int **_temperature;
int **_humidity;
int **_wind;

struct nb_entry_t {
	int outlook;
	int temperature;
	int humidity;
	int wind;
	int yn;	/* Yes or No */
};

#define NB_MAX_TRAINED_ENTRIES	10000

struct nb_classfier_t {

	struct nb_entry_t entries[NB_MAX_TRAINED_ENTRIES];
	int nb_count;
	int nb_cat[NB_CATE_SIZE];

	float nb_prior_proba[NB_CATE_SIZE];
};

struct nb_classfier_t *nb_ptr, nb = {
	.nb_count = 0,
	.nb_cat = {0, 0},
	.nb_prior_proba = {0.00, 0.00}
};

static __oryx_always_inline__ int
outlook_index(const char *section)
{

	int i;
	
	for (i = 0; i < (int)(sizeof (outlook)/sizeof (struct indent_t) - 1); i ++) {
		struct indent_t *t;
		t = (struct indent_t *)&outlook[i];
		if (strcasecmp (t->section, section) == 0)
			return t->id;
	}

	return 0;
}

static __oryx_always_inline__ int
temperature_index(const char *section)
{

	int i;
	
	for (i = 0; i < (int)(sizeof (temperature)/sizeof (struct indent_t) - 1); i ++) {
		struct indent_t *t;
		t = (struct indent_t *)&temperature[i];
		if (strcasecmp (t->section, section) == 0)
			return t->id;
	}

	return 0;
}

static __oryx_always_inline__ int
humidity_index(const char *section)
{

	int i;
	
	for (i = 0; i < (int)(sizeof (humidity)/sizeof (struct indent_t) - 1); i ++) {
		struct indent_t *t;
		t = (struct indent_t *)&humidity[i];
		if (strcasecmp (t->section, section) == 0)
			return t->id;
	}
	return 0;
}

static __oryx_always_inline__ int
wind_index(const char *section)
{

	int i;
	
	for (i = 0; i < (int)(sizeof (wind)/sizeof (struct indent_t) - 1); i ++) {
		struct indent_t *t;
		t = (struct indent_t *)&wind[i];
		if (strcasecmp (t->section, section) == 0)
			return t->id;
	}
	return 0;
}

#define INDEX_OF(index)	(index-1)

int nb_load_sample(char *file)
{

	char record[1024] = {0};
	FILE *fp;
	int id;
	char col_outlook[64] = {0};	
	char col_temperature[64] = {0};
	char col_humidity[64] = {0};
	char col_wind[64] = {0};
	char col_yn[64] = {0};
	int i = 0;
	struct nb_classfier_t *nbptr = &nb;
	struct nb_entry_t *nb_entry;
	
#ifdef NB_DEBUG
	fprintf (stdout, "Openning %s, %d, %d, %d, %d\n", 
			file,
			(int)(sizeof (outlook)/sizeof (struct indent_t) - 1),
			(int)(sizeof (temperature)/sizeof (struct indent_t) - 1),
			(int)(sizeof (humidity)/sizeof (struct indent_t) - 1),
			(int)(sizeof (wind)/sizeof (struct indent_t) - 1));
#endif
	fp = fopen (file, "r");

	if (unlikely(!fp))  {
		fprintf (stdout, "No such file\n");
		return 0;
	}

	for (i = 0; i < NB_MAX_TRAINED_ENTRIES; i ++) {
		struct nb_entry_t *nb_entry = &nbptr->entries[i];
		memset (nb_entry, 0, sizeof (struct nb_entry_t));
	}

	while (fgets (record, 1024, fp)) {

		if (record[0] == '#')
			continue;
		
		sscanf (record, "%d%s%s%s%s%s",
			&id,
			&col_outlook[0],
			&col_temperature[0],
			&col_humidity[0],
			&col_wind[0],
			&col_yn[0]);

		if ((col_outlook[0] && col_temperature[0] && col_humidity[0] && col_wind[0])) {

			nb_entry = &nbptr->entries[nbptr->nb_count ++];
			
			if (col_outlook[0])
				nb_entry->outlook = outlook_index (col_outlook);
			
			if (col_temperature[0])
				nb_entry->temperature = temperature_index (col_temperature);
			
			if (col_humidity[0])
				nb_entry->humidity = humidity_index(col_humidity);
			
			if (col_wind[0])
				nb_entry->wind = wind_index(col_wind);

			if (col_yn[0]) {
				if (strcasecmp (col_yn, "Yes") == 0) {
					nb_entry->yn = NB_YES;
					nbptr->nb_cat[NB_YES] ++;
				}
				if (strcasecmp (col_yn, "No") == 0) {
					nb_entry->yn = NB_NO;
					nbptr->nb_cat[NB_NO] ++;
				}
			}
#ifdef NB_DEBUG
			fprintf (stdout, "%s(%d)  %s(%d)  %s(%d)  %s(%d) %s(%d) \n", 
					col_outlook, nb_entry->outlook,
					col_temperature, nb_entry->temperature,
					col_humidity, nb_entry->humidity,
					col_wind, nb_entry->wind,
					col_yn, nb_entry->yn);
#endif				
		}

		memset (col_outlook, 0, 64);
		memset (col_temperature, 0, 64);
		memset (col_humidity, 0, 64);
		memset (col_wind, 0, 64);
		memset (col_yn, 0, 64);
	}
#ifdef NB_DEBUG
	for (i = 0; i < nbptr->nb_count; i ++) {
		struct nb_entry_t *nb_entry = &nbptr->entries[i];
		fprintf (stdout, "%d ->    %d  %d  %d  %d \n", 
					(i + 1),
					nb_entry->outlook,
					nb_entry->temperature,
					nb_entry->humidity,
					nb_entry->wind);
	}
#endif
	fprintf (stdout, "yes =%d, no =%d, total =%d\n", 
			nbptr->nb_cat[NB_NO], 
			nbptr->nb_cat[NB_YES],
			nbptr->nb_count);

	nbptr->nb_prior_proba[NB_NO] = (float)((float)nbptr->nb_cat[NB_NO]/(float)nbptr->nb_count);
	nbptr->nb_prior_proba[NB_YES] = (float)((float)nbptr->nb_cat[NB_YES]/(float)nbptr->nb_count);
	fprintf (stdout, "%15s%6.6f\n", "nb_prior_proba[N] =   ", nbptr->nb_prior_proba[NB_NO]);
	fprintf (stdout, "%15s%6.6f\n", "nb_prior_proba[Y] =   ", nbptr->nb_prior_proba[NB_YES]);
	
	return 0;
}

/**
	P(Ci|R)
*/

float nb_post_proba()
{

	struct nb_classfier_t *nbptr = &nb;
	int i, j;

	_outlook = (int **)malloc(sizeof(int)*(sizeof (outlook)/sizeof (struct indent_t) - 1));
	for (i = 0; i < (int)(sizeof (outlook)/sizeof (struct indent_t) - 1); i ++) 
		_outlook[i] = (int *)malloc (sizeof (int) * NB_CATE_SIZE);
	
	_temperature = (int **)malloc(sizeof(int)*(sizeof (temperature)/sizeof (struct indent_t) - 1) * NB_CATE_SIZE);
	for (i = 0; i < (int)(sizeof (temperature)/sizeof (struct indent_t) - 1); i ++) 
		_temperature[i] = (int *)malloc (sizeof (int) * NB_CATE_SIZE);
	
	_humidity = (int **)malloc(sizeof(int)*(sizeof (humidity)/sizeof (struct indent_t) - 1) * NB_CATE_SIZE);
	for (i = 0; i < (int)(sizeof (humidity)/sizeof (struct indent_t) - 1); i ++) 
		_humidity[i] = (int *)malloc (sizeof (int) * NB_CATE_SIZE);
	
	_wind = (int **)malloc(sizeof(int)*(sizeof (wind)/sizeof (struct indent_t) - 1) * NB_CATE_SIZE);
	for (i = 0; i < (int)(sizeof (wind)/sizeof (struct indent_t) - 1); i ++) 
		_wind[i] = (int *)malloc (sizeof (int) * NB_CATE_SIZE);

	for (j = 0; j < nbptr->nb_count; j ++) {
		struct nb_entry_t *nb_entry = &nbptr->entries[j];

		/** 
		  * P(Outlook|R=No) 
		  * P(Outlook|R=YES)
		  */	
		switch (nb_entry->outlook) {
			case 1:	/** Sunny */
			case 2:	/** Overcast */
			case 3:	/** Rain */
				//fprintf (stdout, "%d\n", nb_entry->outlook);
				_outlook[INDEX_OF(nb_entry->outlook)][nb_entry->yn] ++;
				break;
			default:
				break;
		}

		/** 
		  * P(Temperature|R=No) 
		  * P(Temperature|R=YES)
		  */	
		switch (nb_entry->temperature) {
			case 1:	/** Hot */
			case 2:	/** Mild */
			case 3:	/** Cool */
				_temperature[INDEX_OF(nb_entry->temperature)][nb_entry->yn] ++;
				break;
			default:
				break;
		}

		/** 
		  * P(Humidity|R=No) 
		  * P(Humidity|R=YES)
		  */	
		switch (nb_entry->humidity) {
			case 1:	/** High */
			case 2:	/** Normal */
				_humidity[INDEX_OF(nb_entry->humidity)][nb_entry->yn] ++;
				break;
			default:
				break;
		}

		/** 
		  * P(Wind|R=No) 
		  * P(Wind|R=YES)
		  */	
		switch (nb_entry->wind) {
			case 1:	/** Strong */
			case 2:	/** Weak */
				_wind[INDEX_OF(nb_entry->wind)][nb_entry->yn] ++;
				break;
			default:
				break;
		}
		
	}
#ifdef NB_DEBUG
	for (i = 0; i < (int)(sizeof (outlook)/sizeof (struct indent_t) - 1); i ++) 
		fprintf (stdout, "%s, %d(yes), %d(no)\n",  outlook[i].section, _outlook[i][NB_YES], _outlook[i][NB_NO]);
	
	for (i = 0; i < (int)(sizeof (temperature)/sizeof (struct indent_t) - 1); i ++) 
		fprintf (stdout, "%s, %d(yes), %d(no)\n",  temperature[i].section, _temperature[i][NB_YES], _temperature[i][NB_NO]);
	
	for (i = 0; i < (int)(sizeof (humidity)/sizeof (struct indent_t) - 1); i ++) 
		fprintf (stdout, "%s, %d(yes), %d(no)\n",  humidity[i].section, _humidity[i][NB_YES], _humidity[i][NB_NO]);
	
	for (i = 0; i < (int)(sizeof (wind)/sizeof (struct indent_t) - 1); i ++) 
		fprintf (stdout, "%s, %d(yes), %d(no)\n",  wind[i].section, _wind[i][NB_YES], _wind[i][NB_NO]);
#endif	
	
#ifdef NB_DEBUG
	for (i = 0; i < (int)(sizeof (outlook)/sizeof (struct indent_t) - 1); i ++) {
		struct indent_t *t;
		t = (struct indent_t *)&outlook[i];
		
		fprintf (stdout, "%s, %.3f(%d/%d, yes), %.3f(%d/%d, no)\n", 
			t->section,
			(float)((float)_outlook[i][NB_YES] /(float)(nbptr->nb_cat[NB_YES])),
			_outlook[i][NB_YES], nbptr->nb_cat[NB_YES],
			(float)((float)_outlook[i][NB_NO] /(float)(nbptr->nb_cat[NB_NO])),
			_outlook[i][NB_NO], nbptr->nb_cat[NB_NO]);

	}

	for (i = 0; i < (int)(sizeof (temperature)/sizeof (struct indent_t) - 1); i ++) {
		struct indent_t *t;
		t = (struct indent_t *)&temperature[i];
		
		fprintf (stdout, "%s, %.3f(%d/%d, yes), %.3f(%d/%d, no)\n", 
			t->section,
			(float)((float)_temperature[i][NB_YES] /(float)(nbptr->nb_cat[NB_YES])),
			_temperature[i][NB_YES], nbptr->nb_cat[NB_YES],
			(float)((float)_temperature[i][NB_NO] /(float)(nbptr->nb_cat[NB_NO])),
			_temperature[i][NB_NO], nbptr->nb_cat[NB_NO]);

	}

	for (i = 0; i < (int)(sizeof (humidity)/sizeof (struct indent_t) - 1); i ++) {
		struct indent_t *t;
		t = (struct indent_t *)&humidity[i];
		
		fprintf (stdout, "%s, %.3f(%d/%d, yes), %.3f(%d/%d, no)\n", 
			t->section,
			(float)((float)_humidity[i][NB_YES] /(float)(nbptr->nb_cat[NB_YES])),
			_humidity[i][NB_YES], nbptr->nb_cat[NB_YES],
			(float)((float)_humidity[i][NB_NO] /(float)(nbptr->nb_cat[NB_NO])),
			_humidity[i][NB_NO], nbptr->nb_cat[NB_NO]);

	}

	for (i = 0; i < (int)(sizeof (wind)/sizeof (struct indent_t) - 1); i ++) {
		struct indent_t *t;
		t = (struct indent_t *)&wind[i];
		
		fprintf (stdout, "%s, %.3f(%d/%d, yes), %.3f(%d/%d, no)\n", 
			t->section,
			(float)((float)_wind[i][NB_YES] /(float)(nbptr->nb_cat[NB_YES])),
			_wind[i][NB_YES], nbptr->nb_cat[NB_YES],
			(float)((float)_wind[i][NB_NO] /(float)(nbptr->nb_cat[NB_NO])),
			_wind[i][NB_NO], nbptr->nb_cat[NB_NO]);

	}
	
#endif

	return 0;
}

/**
  * How is the P(R|C={Outlook=Sunny, Temperature=Cool, Humidity=High, Wind=Strong}
  * Let's start.
  */
void nb_test ()
{

	int index;
	struct nb_classfier_t *nbptr = &nb;
	float outlook_yn[NB_CATE_SIZE];
	float temperature_yn[NB_CATE_SIZE];
	float humidity_yn[NB_CATE_SIZE];
	float wind_yn[NB_CATE_SIZE];
	float summary_yn[NB_CATE_SIZE];
	
	index = NB_YES;
	
	/** P(sunny|yes)*/
	outlook_yn[index] = (float)((float)_outlook[INDEX_OF(outlook_index("Sunny"))][index]/(float)nbptr->nb_cat[index]);

	/** P(cool|yes)*/
	temperature_yn[index] = (float)((float)_temperature[INDEX_OF(temperature_index("Cool"))][index]/(float)nbptr->nb_cat[index]);

	/** P(high|yes)*/
	humidity_yn[index] = (float)((float)_humidity[INDEX_OF(humidity_index("High"))][index]/(float)nbptr->nb_cat[index]);

	/** P(high|yes)*/
	wind_yn[index] = (float)((float)_wind[INDEX_OF(wind_index("Strong"))][index]/(float)nbptr->nb_cat[index]);

	/**
	  *	    P(R=Yes|Ci) = P(Outlook=Sunny|R=Yes) *
	  *              	P(Temperature=Cool|R=Yes) *
	  *			P(Humidity=High|R=Yes) *
	  *			P(Wind=Strong|R=Yes)  * 
	  *			P(R=Yes)/P(C)
	  */
	summary_yn[index] = (outlook_yn[index] * temperature_yn[index] * humidity_yn[index] * wind_yn[index]) * nbptr->nb_prior_proba[index];



	index = NB_NO;
	
	/** P(sunny|no)*/
	outlook_yn[index] = (float)((float)_outlook[INDEX_OF(outlook_index("Sunny"))][index]/(float)nbptr->nb_cat[index]);

	/** P(cool|no)*/
	temperature_yn[index] = (float)((float)_temperature[INDEX_OF(temperature_index("Cool"))][index]/(float)nbptr->nb_cat[index]);

	/** P(high|no)*/
	humidity_yn[index] = (float)((float)_humidity[INDEX_OF(humidity_index("High"))][index]/(float)nbptr->nb_cat[index]);

	/** P(high|no)*/
	wind_yn[index] = (float)((float)_wind[INDEX_OF(wind_index("Strong"))][index]/(float)nbptr->nb_cat[index]);


	/**
	  *	    P(R=No|Ci) = P(Outlook=Sunny|R=No) *
	  *              	P(Temperature=Cool|R=No) *
	  *			P(Humidity=High|R=No) *
	  *			P(Wind=Strong|R=No)  * 
	  *			P(R=No)/P(C)
	  */
	summary_yn[index] = (outlook_yn[index] * temperature_yn[index] * humidity_yn[index] * wind_yn[index]) * nbptr->nb_prior_proba[index];

	fprintf (stdout, "%16s%6.6f\n", "nb_post_proba[N] =   ", summary_yn[NB_NO]);
	fprintf (stdout, "%16s%6.6f\n", "nb_post_proba[Y] =   ", summary_yn[NB_YES]);
	if (summary_yn[NB_YES] > summary_yn[NB_NO])
		fprintf (stdout, "You should goto tennis\n");
	else
		fprintf (stdout, "You should not goto tennis\n");

	
	
}

void nb_free ()
{

	int i;

	for (i = 0; i < (int)(sizeof (outlook)/sizeof (struct indent_t) - 1); i ++) 
		free (_outlook[i]);
	free (_outlook);

	for (i = 0; i < (int)(sizeof (temperature)/sizeof (struct indent_t) - 1); i ++) 
		free (_temperature[i]);
	free (_temperature);
	
	for (i = 0; i < (int)(sizeof (humidity)/sizeof (struct indent_t) - 1); i ++) 
		free (_humidity[i]);
	free (_humidity);

	for (i = 0; i < (int)(sizeof (wind)/sizeof (struct indent_t) - 1); i ++) 
		free (_wind[i]);
	free (_wind);
}

