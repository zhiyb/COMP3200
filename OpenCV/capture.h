#ifndef CAPTURE_H
#define CAPTURE_H

int captureInit(const char *devfile, int width, int height);
cv::Mat captureQuery();
void captureClose();

#endif
