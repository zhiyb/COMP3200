#ifndef __OV5647_H__
#define __OV5647_H__

#include <linux/ioctl.h>  /* For IOCTL macros */
#include <media/nvc.h>
#include <media/nvc_image.h>

#if 0
struct ov5647_sensordata {
	__u32 fuse_id_size;
	__u8  fuse_id[16];
};
#endif

#ifdef __KERNEL__
struct ov5647_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *iovdd;
	struct regulator *vcmvdd;
};

struct ov5647_platform_data {
	const char *mclk_name; /* NULL for default default_mclk */
	int (*power_on)(struct ov5647_power_rail *pw);
	int (*power_off)(struct ov5647_power_rail *pw);
};
#endif /* __KERNEL__ */

#endif  /* __OV5647_H__ */
