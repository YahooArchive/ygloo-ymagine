#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

#include "ccv.h"

#include <string.h>

#if HAVE_CLASSIFIER
static void*
CcvMalloc(size_t l)
{
    void *p = ccmalloc(l);
    if (p != NULL) {
	memset(p, 0, l);
    }

    return p;
}

static int32_t
unzigzag32(uint32_t n)
{
    return (int32_t) ((n >> 1) ^ (-(n & 1)));
}

static int64_t
unzigzag64(uint64_t n)
{
    return (int64_t) ((n >> 1) ^ (-(n & 1)));
}

static uint32_t
getVarint32(const char *s, int *l)
{
    uint32_t res = 0;
    int shift = 0;
    int length = 0;
    const unsigned char *c = (const unsigned char*) s;

    while (1) {
	res |= ((uint32_t) (*c & 0x7f)) << shift;
	shift += 7;
	length++;

	if (!(*c & 0x80)) {
	    break;
	}
	if (shift >= 32) {
	    /* Error */
	    break;
	}

	c++;
    }

    if (l != NULL) {
	*l = length;
    }

    return res;
}

static uint64_t
getVarint64(const char *s, int *l)
{
    uint64_t res = 0;
    int shift = 0;
    int length = 0;
    const unsigned char *c = (const unsigned char*) s;

    while (1) {
	res |= ((uint32_t) (*c & 0x7f)) << shift;
	shift += 7;
	length++;

	if (!(*c & 0x80)) {
	    break;
	}
	if (shift >= 64) {
	    /* Error */
	    break;
	}

	c++;
    }

    if (l != NULL) {
	*l = length;
    }

    return res;
}

static uint32_t
getSint32(const char *s, int *l) {
    return unzigzag32(getVarint32(s, l));
}

static uint32_t
getSint64(const char *s, int *l) {
    return unzigzag64(getVarint64(s, l));
}

static float
getFloat(const char *s, int *l) {
    float f;
    int length = 0;

    memcpy(&f, s, sizeof(float));
    length += sizeof(float);

    if (l != NULL) {
	*l = length;
    }

    return f;
}

ccv_bbf_classifier_cascade_t*
ccv_bbf_classifier_cascade_read_compact_binary(const char* s)
{
    ccv_bbf_classifier_cascade_t* cascade;
    ccv_bbf_stage_classifier_t* classifier;
    const char *next;
    int i, j, k;
    int fcount;
    int l;

    next = (const char*) s;

    cascade = (ccv_bbf_classifier_cascade_t*) CcvMalloc(sizeof(ccv_bbf_classifier_cascade_t));
    if (cascade == NULL) {
	return NULL;
    }

    cascade->count = getVarint32(next, &l);
    next += l;
    cascade->size.width = getVarint32(next, &l);
    next += l;
    cascade->size.height = getVarint32(next, &l);
    next += l;

    cascade->stage_classifier = (ccv_bbf_stage_classifier_t*)CcvMalloc(cascade->count * sizeof(ccv_bbf_stage_classifier_t));
    if (cascade->stage_classifier == NULL) {
	ccfree(cascade);
	return NULL;
    }

    /* Initialize all members of cascade, so we can clean it up in case of failure */
    classifier = cascade->stage_classifier;
    for (i = 0; i < cascade->count; i++, classifier++) {	
	classifier->count = 0;
	classifier->threshold = 0.0f;
	classifier->feature = NULL;
	classifier->alpha = NULL;
    }

    classifier = cascade->stage_classifier;
    for (i = 0; i < cascade->count; i++, classifier++) {
	classifier->count = getVarint32(next, &l);
	next += l;
	classifier->threshold = getFloat(next, &l);
	next += l;

#if 0
	int32_t *it = (int*) &(classifier->threshold);
	printf("// Loading %d/%d %dx%d\n", i+1, cascade->count, cascade->size.width, cascade->size.height);
	printf("%d\n", classifier->count);
	printf("%d\n", *it);
#endif

	classifier->feature = (ccv_bbf_feature_t*)CcvMalloc(classifier->count * sizeof(ccv_bbf_feature_t));
	classifier->alpha = (float*)CcvMalloc(classifier->count * 2 * sizeof(float));

	if (classifier->feature == NULL || classifier->alpha == NULL) {
	    for (j = 0; j <= cascade->count; i++, classifier++) {
		if (classifier->feature != NULL) {
		    ccfree(classifier->feature);
		    classifier->feature = NULL;
		}
		if (classifier->alpha != NULL) {
		    ccfree(classifier->alpha);
		    classifier->alpha = NULL;
		}
	    }
	    ccfree(cascade->stage_classifier);
	    ccfree(cascade);
	    return NULL;
	}

	for (j = 0; j < classifier->count; j++) {
	    fcount = getVarint32(next, &l);
	    next += l;

#if 0
	    printf("%d\n", fcount);
#endif
	    classifier->feature[j].size = fcount;

	    for (k = 0; k < fcount; k++) {
		classifier->feature[j].px[k] = getSint32(next, &l);
		next += l;
		classifier->feature[j].py[k] = getSint32(next, &l);
		next += l;
		classifier->feature[j].pz[k] = getSint32(next, &l);
		next += l;
		classifier->feature[j].nx[k] = getSint32(next, &l);
		next += l;
		classifier->feature[j].ny[k] = getSint32(next, &l);
		next += l;
		classifier->feature[j].nz[k] = getSint32(next, &l);
		next += l;

#if 0
		printf("%d %d %d\n",
		       classifier->feature[j].px[k],
		       classifier->feature[j].py[k],
		       classifier->feature[j].pz[k]);
		printf("%d %d %d\n",
		       classifier->feature[j].nx[k],
		       classifier->feature[j].ny[k],
		       classifier->feature[j].nz[k]);
#endif
	    }

	    classifier->alpha[2*j] = getFloat(next, &l);
	    next += l;
	    classifier->alpha[2*j+1] = getFloat(next, &l);
	    next += l;

#if 0
	    int32_t *i1 = (int*) &(classifier->alpha[2*j]);
	    int32_t *i2 = (int*) &(classifier->alpha[2*j+1]);
	    printf("%d %d\n", *i1, *i2);
#endif
	}
    }

    return cascade;
}
#endif
