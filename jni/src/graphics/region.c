#define LOG_TAG "ymagine::region"

#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

int
VrectComputeIntersection(const Vrect *rect1, const Vrect *rect2, Vrect *output)
{
  if (output == NULL) {
    return YMAGINE_OK;
  }

  /* NULL means full space, so intersection of a rectangle with NULL is this rectangle */
  if (rect1 == NULL && rect2 == NULL) {
    return YMAGINE_ERROR;
  }
  if (rect1 == NULL) {
    output->x = rect2->x;
    output->y = rect2->y;
    output->width = rect2->width;
    output->height = rect2->height;
    return YMAGINE_OK;
  }
  if (rect2 == NULL) {
    output->x = rect1->x;
    output->y = rect1->y;
    output->width = rect1->width;
    output->height = rect1->height;
    return YMAGINE_OK;
  }

  // Intersect is symetric, so pre-condition rectangles so r1 is left of r2
  if (rect1->x > rect2->x) {
    const Vrect *tmp = rect1;
    rect1 = rect2;
    rect2 = tmp;
  }

  if (rect1->x + rect1->width <= rect2->x) {
    // Rectangle 1 is left of rectangle 2
    output->x = 0;
    output->y = 0;
    output->width = 0;
    output->height = 0;
    return YMAGINE_OK;
  }
  if (rect1->y + rect1->height <= rect2->y) {
    // Rectangle 1 is above rectangle 2
    output->x = 0;
    output->y = 0;
    output->width = 0;
    output->height = 0;
    return YMAGINE_OK;
  }
  if (rect1->y >= rect2->y + rect2->height) {
    // Rectangle 1 is below rectangle 2
    output->x = 0;
    output->y = 0;
    output->width = 0;
    output->height = 0;
    return YMAGINE_OK;
  }

  output->x = rect2->x;
  output->width = rect1->width - (output->x - rect1->x);
  if (output->width > rect2->width) {
    output->width = rect2->width;
  }

  // Intersect is symetric, so pre-condition rectangles so r1 is above r2
  if (rect1->y > rect2->y) {
    const Vrect *tmp = rect1;
    rect1 = rect2;
    rect2 = tmp;
  }

  output->y = rect2->y;
  output->height = rect1->height - (output->y - rect1->y);
  if (output->height > rect2->height) {
    output->height = rect2->height;
  }
  return YMAGINE_OK;
}

