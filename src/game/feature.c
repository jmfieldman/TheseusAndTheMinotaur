#include "feature.h"
#include <stdlib.h>
#include <string.h>

Feature* feature_create(const FeatureVTable* vt, int col, int row) {
    Feature* f = calloc(1, sizeof(Feature));
    if (!f) return NULL;
    f->vt   = vt;
    f->col  = col;
    f->row  = row;
    f->data = NULL;
    return f;
}

void feature_free(Feature* f) {
    if (!f) return;
    if (f->vt && f->vt->destroy) {
        f->vt->destroy(f);
    }
    free(f);
}
