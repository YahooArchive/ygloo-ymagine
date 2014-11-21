#ifndef _YMAGINE_GRAPHICS_REGION_H
#define _YMAGINE_GRAPHICS_REGION_H 1

#include "ymagine/ymagine.h"

#ifdef __cplusplus
extern "C" {
#endif

struct VrectStruct {
    int x;
    int y;
    int width;
    int height;
};

typedef struct VrectStruct Vrect;

int
VrectComputeIntersection(const Vrect *rect1, const Vrect *rect2, Vrect *output);

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_GRAPHICS_REGION_H */
