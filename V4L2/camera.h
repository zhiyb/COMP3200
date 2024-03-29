#ifndef CAMERA_H
#define CAMERA_H

// Minimum frame interval (ms)
#define CAM_ITVL	(10.21917f / 1000.f)
// Output lines when frame interval is minimum
#define CAM_LINES	495
// Time to transmit 1 line data (us)
#define CAM_LINEITVL	(20.645f / 1000.f / 1000.f)
// Maximum frame lines change rate
#define CAM_CHANGE	0x60

#define FPS_MAX		30.f
#define FPS_MIN		5.2f
#define FPS_CHANGE	1500.f

#endif
