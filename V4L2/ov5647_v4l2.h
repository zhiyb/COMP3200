#ifndef OV5647_H
#define OV5647_H

#define OV5647_MAX_W	2592
#define OV5647_MAX_H	1944

#define OV5647_CID_NUM		2
#define OV5647_CID_BASE		(V4L2_CID_USER_BASE + 0x1500)
#define OV5647_CID_LED		V4L2_CID_ILLUMINATORS_1
// Write to register
#define OV5647_CID_REG_W	OV5647_CID_BASE
// Read from register (use s_ctrl)
#define OV5647_CID_REG_R	(OV5647_CID_BASE + 1)
// Word operation mask
#define OV5647_CID_REG_WMASK	0x80000000UL

#endif
