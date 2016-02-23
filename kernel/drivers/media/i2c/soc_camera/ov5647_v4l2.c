/*
 * ov5647.c - ov5647 sensor driver
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/videodev2.h>

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <mach/io_dpd.h>

#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-chip-ident.h>
#include <media/soc_camera.h>
#include <media/ov5647.h>
#include "ov5647_v4l2.h"

//#define OV5647_MAX_RETRIES	3
#define OV5647_WAIT_MS		5

#define CAM_RSTN	219	// TEGRA_GPIO_PBB3
#define CAM1_PWDN	221	// TEGRA_GPIO_PBB5
#define CAM_AF_PWDN	223	// TEGRA_GPIO_PBB7
#define CAM1_GPIO	225	// TEGRA_GPIO_PCC1

static struct tegra_io_dpd csia_io = {
	.name			= "CSIA",
	.io_dpd_reg_index	= 0,
	.io_dpd_bit		= 0,
};

struct ov5647_datafmt {
	enum v4l2_mbus_pixelcode	code;
	enum v4l2_colorspace		colorspace;
};

#define STATUS_NEED_INIT	0x01
#define STATUS_MODE_CHANGED	0x02

struct ov5647_priv {
	struct v4l2_subdev		subdev;
	struct v4l2_ctrl_handler	hdl;

	const struct ov5647_datafmt	*fmt;
	struct ov5647_power_rail	power;
	//struct ov5647_sensordata	sensor_data;
	struct ov5647_platform_data	*pdata;
	struct i2c_client		*i2c_client;
	int				mode;
	unsigned int			status;
	u16				reg_addr;
	//struct clk			*mclk;
	//struct mutex			ov5647_camera_lock;
	//struct dentry			*debugdir;
};

struct ov5647_reg {
	u16 addr;
	u8 val;
};

static const struct ov5647_datafmt ov5647_colour_fmts[] = {
	{V4L2_MBUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_SRGB},
	//{V4L2_MBUS_FMT_SBGGR8_1X8, V4L2_COLORSPACE_SRGB},
};

#define OV5647_TABLE_WAIT_MS	0
#define OV5647_TABLE_SEQ	1
#define OV5647_TABLE_END	2

enum {
	OV5647_SEQ_2592X1944 = 0,
	OV5647_SEQ_1920X1080,
	OV5647_SEQ_1280X960,
	OV5647_SEQ_1280X720,
	OV5647_SEQ_640X480,
	OV5647_SEQ_NO_BINNING,
};

static struct ov5647_reg default_regs[] = {
	{0x0103, 0x01},	// Reset
	{0x0100, 0x00},	// Standby
	{OV5647_TABLE_WAIT_MS, OV5647_WAIT_MS},
	{0x0100, 0x00},	// Standby
	{0x0103, 0x01},	// Reset

	// MIPI settings
	{0x3016, 0x08},	// mipi_pad_enable
	{0x3017, 0xe0},	// MIPI PHY
	{0x3018, 0x44},	// MIPI enable
	//{0x4800, 0x04},
	{0x4800, 0x24},	// Gate clock lane
	//{0x4837, 0x20},	// MIPI PCLK divider
	//{0x4838, 0x30},	// MIPI wakeup delay
	//{0x4843, 0x01},

	{0x3034, 0x1A},	// pll_charge_pump, mipi_bit_mode
	{0x3035, 0x21},	// system_pll_div, scale_divider_mipi
	{0x3036, 0x62},	// PLL_multiplier
	{0x303C, 0x11},	// pll_root_div, pll_prediv
	{0x3106, 0xF5},	// PLL clock divider(pll_sclk)
	//{0x3821, 0x00},
	//{0x3820, 0x00},
	//{0x3827, 0xEC},	// ?
	{0x370C, 0x03},	// ANALOG_CONTROL 37
	{0x3612, 0x5B},	// ANALOG_CONTROL 36
	{0x3618, 0x04},	// ANALOG_CONTROL 36
	{0x5000, 0x06},	// ISP(bc, wc)
#if 0
#if 1
	{0x5001, 0x00},	// ISP
#else
	{0x5001, 0x01},	// ISP(awb)
#endif
#endif
#if 0
	{0x5002, 0x40},	// ISP(win_en)
#else
	{0x5002, 0x41},	// ISP(win_en, AWB gain)
	//{0x5005, 0x34},	// ISP(awb_bias, lenc_bias)
	//{0x5180, 0x40},	// AWB
#endif
	//{0x5002, 0x43},
	{0x5003, 0x08},	// ISP(buf_en)
	//{0x5003, 0x0a},
	//{0x501f, 0x33},
	//{0x5025, 0x01},	// ISP(AVG from AWB)
	//{0x5A00, 0x08},
	{0x5A00, 0x00},
	{0x3000, 0x00},	// IO diection input
	{0x3001, 0x00},	// IO diection input
	{0x3002, 0x00},	// IO diection input
	{0x301C, 0xF8},	// ?
	{0x301D, 0xF0},	// ?
	{0x3A18, 0x00},	// AEC gain ceiling
	{0x3A19, 0xF8},	// AEC gain ceiling
	{0x3C01, 0x80},	// 50/60Hz
	{0x3B07, 0x0C},	// STROBE_FREX
	{0x3708, 0x64},	// ANALOG_CONTROL 37
	{0x3709, 0x12},	// ANALOG_CONTROL 37
	{0x3630, 0x2E},	// ANALOG_CONTROL 36
	{0x3632, 0xE2},	// ANALOG_CONTROL 36
	{0x3633, 0x23},	// ANALOG_CONTROL 36
	{0x3634, 0x44},	// ANALOG_CONTROL 36
	{0x3636, 0x06},	// A
	{0x3620, 0x64},	// A
	{0x3621, 0xE0},	// A/
	{0x3600, 0x37},	// A
	{0x3704, 0xA0},	// A
	{0x3703, 0x5A},	// A
	{0x3715, 0x78},	// A
	{0x3717, 0x01},	// A
	{0x3731, 0x02},	// A
	{0x370B, 0x60},	// A
	{0x3705, 0x1A},	// A
	{0x3F05, 0x02},	// ?
	{0x3F06, 0x10},	// ?
	{0x3F01, 0x0A},	// ?
#if 1
	{0x3A08, 0x01},
	{0x3A09, 0x28},
	{0x3A0A, 0x00},
	{0x3A0B, 0xf6},
	{0x3A0D, 0x08},
	{0x3A0E, 0x06},
	{0x3A0F, 0x58},
	{0x3A10, 0x50},
	{0x3A1B, 0x58},
	{0x3A1E, 0x50},
	{0x3A11, 0x60},
	{0x3A1F, 0x28},
#else
	{0x3A08, 0x01},
	{0x3A09, 0x4B},
	{0x3A0A, 0x01},
	{0x3A0B, 0x13},
	{0x3A0D, 0x04},
	{0x3A0E, 0x03},
	{0x3A0F, 0x58},
	{0x3A10, 0x50},
	{0x3A1B, 0x58},
	{0x3A1E, 0x50},
	{0x3A11, 0x60},
	{0x3A1F, 0x28},
#endif
	{0x4001, 0x02},
	{0x4004, 0x04},
	{0x4000, 0x09},
#if 0
	{0x3503, 0x03},
#else
	{0x3503, 0x00},
#endif
	//{0x3820, 0x00},
	//{0x3821, 0x02},
	{0x350A, 0x00},
	{0x350B, 0x10},
	{0x3212, 0x00},
	{0x3500, 0x00},
	{0x3501, 0x0b},
	{0x3502, 0x40},
	{0x3212, 0x10},
	{0x3212, 0xA0},
	{0x350A, 0x00},
	{0x350B, 0x10},
	{0x350A, 0x00},
	{0x350B, 0x10},
	{0x3212, 0x00},
	{0x3500, 0x00},
	{0x3501, 0x0b},
	{0x3502, 0x40},
	{0x3212, 0x10},
	{0x3212, 0xA0},
	//{0x3820, 0x42},
	//{0x3821, 0x00},

	//{OV5647_TABLE_WAIT_MS, OV5647_WAIT_MS},
	{OV5647_TABLE_END, 0x00}
};

static u8 ov5647_seq_no_binning[] = {
	0x38, 0x20,	// Register start address
	0x46,		// Vertical binning/flip
	0x00,		// Horizontal binning/mirror
};

static u8 ov5647_seq_2592x1944[] = {	// ~14.6 FPS
	0x38, 0x00,	// Register start address
	0x00, 0x0c,	// x_addr_start
	0x00, 0x04,	// y_addr_start
	0x0a, 0x33,	// x_addr_end
	0x07, 0xa3,	// y_addr_end
	0x0a, 0x20,	// DVP out horizontal
	0x07, 0x98,	// DVP out vertical
	0x0b, 0x1c,	// Horizontal total size
	0x07, 0xb0,	// Vertical total size
	0x00, 0x00,	// ISP horizontal offset
	0x00, 0x00,	// ISP vertical offset
	0x11,		// X_INC
	0x11,		// Y_INC
};

static struct ov5647_reg mode_2592x1944[] = {
	{OV5647_TABLE_SEQ, OV5647_SEQ_2592X1944},
	{OV5647_TABLE_SEQ, OV5647_SEQ_NO_BINNING},
	{0x3612, 0x5b},
	{0x3618, 0x04},
	{OV5647_TABLE_END, 0x00}
};

static u8 ov5647_seq_1920x1080[] = {	// ~30 FPS
	0x38, 0x00,	// Register start address
	0x01, 0x60,	// x_addr_start
	0x01, 0xb6,	// y_addr_start
	0x08, 0xe8,	// x_addr_end
	0x05, 0xf6,	// y_addr_end
	0x07, 0x80,	// DVP out horizontal
	0x04, 0x38,	// DVP out vertical
	0x09, 0x70,	// Horizontal total size
	0x04, 0x50,	// Vertical total size
	0x00, 0x00,	// ISP horizontal offset
	0x00, 0x08,	// ISP vertical offset
	0x11,		// X_INC
	0x11,		// Y_INC
};

static struct ov5647_reg mode_1920x1080[] = {
	{OV5647_TABLE_SEQ, OV5647_SEQ_1920X1080},
	{0x3820, 0x42},	// Vertical binning/flip
	{0x3821, 0x00},	// Horizontal binning/mirror
	{OV5647_TABLE_END, 0x00}
};

static struct ov5647_reg mode_1280x960[] = {	// ~43 FPS
	{0x3800, 0x00},	// x_addr_start
	{0x3801, 0x18},
	{0x3802, 0x00},	// y_addr_start
	{0x3803, 0x0e},
	{0x3804, 0x0a},	// x_addr_end
	{0x3805, 0x27},
	{0x3806, 0x07},	// y_addr_end
	{0x3807, 0x95},

	{0x3808, 0x05},	// DVP out horizontal
	{0x3809, 0x00},
	{0x380a, 0x03},	// DVP out vertical
	{0x380b, 0xc0},

	{0x380c, 0x07},	// Horizontal total size
	{0x380d, 0x68},
	{0x380e, 0x03},	// Vertical total size
	{0x380f, 0xd8},
#if 0
	{0x3810, 0x00},	// ISP horizontal offset
	{0x3811, 0x10},
	{0x3812, 0x00},	// ISP vertical offset
	{0x3813, 0x06},
#endif
	{0x3814, 0x31},	// X_INC
	{0x3815, 0x31},	// Y_INC
	{0x3820, 0x47},	// Vertical binning/flip
	{0x3821, 0x01},	// Horizontal binning/mirror

	{0x3612, 0x59},
	{0x3618, 0x00},
	{OV5647_TABLE_END, 0x00}
};

static struct ov5647_reg mode_1280x720[] = {	// ~61 FPS
	{0x3800, 0x00},	// x_addr_start
	{0x3801, 0x18},
	{0x3802, 0x00},	// y_addr_start
	{0x3803, 0xf8},
	{0x3804, 0x0a},	// x_addr_end
	{0x3805, 0x27},
	{0x3806, 0x06},	// y_addr_end
	{0x3807, 0xa7},

	{0x3808, 0x05},	// DVP out horizontal
	{0x3809, 0x00},
	{0x380a, 0x02},	// DVP out vertical
	{0x380b, 0xd0},

	{0x380c, 0x07},	// Horizontal total size
	{0x380d, 0x00},
	{0x380e, 0x02},	// Vertical total size
	{0x380f, 0xe8},
#if 0
	{0x3810, 0x00},	// ISP horizontal offset
	{0x3811, 0x10},
	{0x3812, 0x00},	// ISP vertical offset
	{0x3813, 0x06},
#endif
	{0x3814, 0x31},	// X_INC
	{0x3815, 0x31},	// Y_INC
	{0x3820, 0x47},	// Vertical binning/flip
	{0x3821, 0x01},	// Horizontal binning/mirror

	{0x3612, 0x59},
	{0x3618, 0x00},
	{OV5647_TABLE_END, 0x00}
};

static struct ov5647_reg mode_640x480[] = {	// ~87 FPS
	{0x3800, 0x00},	// x_addr_start
	{0x3801, 0x10},
	{0x3802, 0x00},	// y_addr_start
	{0x3803, 0x00},
	{0x3804, 0x0a},	// x_addr_end
	{0x3805, 0x2f},
	{0x3806, 0x07},	// y_addr_end
	{0x3807, 0x9f},

	{0x3808, 0x02},	// DVP out horizontal
	{0x3809, 0x80},
	{0x380a, 0x01},	// DVP out vertical
	{0x380b, 0xe0},

	{0x380c, 0x07},	// Horizontal total size
	{0x380d, 0x3c},
	{0x380e, 0x01},	// Vertical total size
	{0x380f, 0xf8},
#if 0
	{0x3810, 0x00},	// ISP horizontal offset
	{0x3811, 0x00},
	{0x3812, 0x00},	// ISP vertical offset
	{0x3813, 0x00},
#endif
	{0x3814, 0x71},	// X_INC
	{0x3815, 0x71},	// Y_INC
	{0x3820, 0x47},	// Vertical binning/flip
	{0x3821, 0x01},	// Horizontal binning/mirror

	{0x3612, 0x59},
	{0x3618, 0x00},
	{OV5647_TABLE_END, 0x00}
};

#define OV5647_WIDTH	2592
#define OV5647_HEIGHT	1944

static const struct v4l2_frmsize_discrete ov5647_frmsizes[] = {
	{2592, 1944},
	{1920, 1080},
	{1280, 960},
	{1280, 720},
	{640, 480},
};

enum {
	OV5647_MODE_2592X1944,
	OV5647_MODE_1920X1080,
	OV5647_MODE_1280X960,
	OV5647_MODE_1280X720,
	OV5647_MODE_640X480,
	OV5647_MODE_INVALID
};

static const struct ov5647_reg *mode_table[] = {
	[OV5647_MODE_2592X1944] = mode_2592x1944,
	[OV5647_MODE_1920X1080] = mode_1920x1080,
	[OV5647_MODE_1280X960] = mode_1280x960,
	[OV5647_MODE_1280X720] = mode_1280x720,
	[OV5647_MODE_640X480] = mode_640x480,
};

struct ov5647_seq {
	u8 *data;
	u16 size;
};
#define OV5647_SEQ(s)	{s, sizeof(s)}

static const struct ov5647_seq ov5647_seq_table[] = {
	[OV5647_SEQ_2592X1944] = OV5647_SEQ(ov5647_seq_2592x1944),
	[OV5647_SEQ_1920X1080] = OV5647_SEQ(ov5647_seq_1920x1080),
	[OV5647_SEQ_NO_BINNING] = OV5647_SEQ(ov5647_seq_no_binning),
};

#define OV5647_MODE_DEF	OV5647_MODE_2592X1944

static inline void msleep_range(unsigned int delay_base)
{
	usleep_range(delay_base * 1000, delay_base * 1000 + 500);
}

static int ov5647_read_reg(struct i2c_client *client, u16 addr, u8 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[3];

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	*val = data[2];
	return 0;
}

static int ov5647_write_reg(struct i2c_client *client, u16 addr, u8 val)
{
	int err;
	struct i2c_msg msg;
	unsigned char data[3];

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err == 1)
		return 0;

	pr_err("%s: i2c write failed, %x = %x\n",
			__func__, addr, val);

	return err;
}

static int ov5647_write_seq(struct i2c_client *client, u8 *seq, u16 cnt)
{
	int err;
	struct i2c_msg msg;

	if (!client->adapter)
		return -ENODEV;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = cnt;
	msg.buf = seq;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err == 1)
		return 0;

	pr_err("%s: i2c write failed, seq of size %x\n",
			__func__, cnt);

	return err;
}

static int ov5647_write_table(struct i2c_client *client, const struct ov5647_reg table[])
{
	int err;
	const struct ov5647_reg *next;
	u16 val;

	for (next = table; next->addr != OV5647_TABLE_END; next++) {
		if (next->addr == OV5647_TABLE_WAIT_MS) {
			msleep_range(next->val);
			continue;
		} else if (next->addr == OV5647_TABLE_SEQ) {
			ov5647_write_seq(client,
					ov5647_seq_table[next->val].data,
					ov5647_seq_table[next->val].size);
			continue;
		}

		val = next->val;

		err = ov5647_write_reg(client, next->addr, val);
		if (err) {
			pr_err("%s:ov5647_write_table:%d", __func__, err);
			return err;
		}
	}
	return 0;
}

static inline struct ov5647_priv *to_ov5647(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct ov5647_priv, subdev);
}

static int test_mode;
module_param(test_mode, int, 0644);

static int ov5647_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5647_priv *priv = to_ov5647(client);
	int err = 0;

	if (!enable) {
		pr_info("%s: disable streaming\n", __func__);
		ov5647_write_reg(priv->i2c_client, 0x0100, 0x00);
		//msleep_range(OV5647_WAIT_MS);
		//ov5647_write_table(priv->i2c_client, ov5647_disable);
		return 0;
	}

	pr_info("%s: mode %d, pixel fmt 0x%04x, pixel cs %d\n", __func__, priv->mode, priv->fmt->code, priv->fmt->colorspace);

	if (priv->status & STATUS_NEED_INIT) {
		err = ov5647_write_table(priv->i2c_client, default_regs);
		if (err)
			return err;

		ov5647_write_reg(priv->i2c_client, 0x503d, test_mode);
		priv->status = ~STATUS_NEED_INIT;
	}

	if (priv->status & STATUS_MODE_CHANGED) {
		err = ov5647_write_table(priv->i2c_client, mode_table[priv->mode]);
		if (err)
			return err;
		priv->status &= ~STATUS_MODE_CHANGED;
	}

#if 0
	err = ov5647_write_table(priv->i2c_client, mode_quality);
	if (err)
		return err;
#endif
	// Enable streaming
	err = ov5647_write_reg(priv->i2c_client, 0x0100, 0x01);
	//err = ov5647_write_table(priv->i2c_client, data_oe);
	if (err)
		return err;

	return err;
}

/* Find a data format by a pixel code in an array */
static const struct ov5647_datafmt *ov5647_find_datafmt(
		enum v4l2_mbus_pixelcode code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ov5647_colour_fmts); i++)
		if (ov5647_colour_fmts[i].code == code)
			return ov5647_colour_fmts + i;

	return NULL;
}

static int ov5647_find_mode(struct v4l2_subdev *sd, u32 width, u32 height)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ov5647_frmsizes); i++) {
		if (width == ov5647_frmsizes[i].width &&
		    height == ov5647_frmsizes[i].height)
			return i;
	}

	dev_err(sd->v4l2_dev->dev, "%dx%d is not supported\n", width, height);
	return OV5647_MODE_2592X1944;
}

static int ov5647_try_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5647_priv *priv = to_ov5647(client);
	int mode = ov5647_find_mode(sd, mf->width, mf->height);

	mf->width = ov5647_frmsizes[mode].width;
	mf->height = ov5647_frmsizes[mode].height;

	if (mf->code != V4L2_MBUS_FMT_SBGGR8_1X8 &&
	    mf->code != V4L2_MBUS_FMT_SBGGR10_1X10)
		mf->code = V4L2_MBUS_FMT_SBGGR10_1X10;

	mf->field = V4L2_FIELD_NONE;
	mf->colorspace = V4L2_COLORSPACE_SRGB;

	priv->mode = mode;
	priv->status |= STATUS_MODE_CHANGED;

	return 0;
}

static int ov5647_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5647_priv *priv = to_ov5647(client);

	dev_dbg(sd->v4l2_dev->dev, "%s(%u)\n", __func__, mf->code);

	/* MIPI CSI could have changed the format, double-check */
	if (!ov5647_find_datafmt(mf->code))
		return -EINVAL;

	ov5647_try_fmt(sd, mf);

	priv->fmt = ov5647_find_datafmt(mf->code);

	return 0;
}

static int ov5647_g_fmt(struct v4l2_subdev *sd,	struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5647_priv *priv = to_ov5647(client);

	const struct ov5647_datafmt *fmt = priv->fmt;

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->width	= OV5647_WIDTH;
	mf->height	= OV5647_HEIGHT;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int ov5647_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = V4L2_MBUS_CSI2_2_LANE |
		V4L2_MBUS_CSI2_CHANNEL_0 |
		V4L2_MBUS_CSI2_NONCONTINUOUS_CLOCK;

	return 0;
}

static int ov5647_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	if ((unsigned int)index >= ARRAY_SIZE(ov5647_colour_fmts))
		return -EINVAL;

	*code = ov5647_colour_fmts[index].code;
	return 0;
}

static int ov5647_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct v4l2_rect *rect = &a->c;

	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	rect->top	= 0;
	rect->left	= 0;
	rect->width	= OV5647_WIDTH;
	rect->height	= OV5647_HEIGHT;

	return 0;
}

static int ov5647_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= OV5647_WIDTH;
	a->bounds.height		= OV5647_HEIGHT;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static struct v4l2_subdev_video_ops ov5647_subdev_video_ops = {
	.enum_mbus_fmt	= ov5647_enum_fmt,
	.s_stream	= ov5647_s_stream,
	.s_mbus_fmt	= ov5647_s_fmt,
	.g_mbus_fmt	= ov5647_g_fmt,
	.try_mbus_fmt	= ov5647_try_fmt,
	.g_crop		= ov5647_g_crop,
	.cropcap	= ov5647_cropcap,
	.g_mbus_config	= ov5647_g_mbus_config,
};

static int ov5647_power_on(struct ov5647_power_rail *pw)
{
	int err;
	//struct ov5647_priv *priv = container_of(pw, struct ov5647_priv, power);

	if (unlikely(WARN_ON(!pw || !pw->iovdd || !pw->avdd || !pw->dvdd)))
		return -EFAULT;

	/* disable CSIA/B IOs DPD mode to turn on camera for ardbeg */
	tegra_io_dpd_disable(&csia_io);

	gpio_set_value(CAM1_GPIO, 0);
	gpio_set_value(CAM1_PWDN, 0);
	usleep_range(100, 200);

	err = regulator_enable(pw->iovdd);
	if (err)
		goto ov5647_iovdd_fail;

	err = regulator_enable(pw->avdd);
	if (err)
		goto ov5647_avdd_fail;

	usleep_range(5000, 6000);
	//gpio_set_value(CAM1_GPIO, 1);
	gpio_set_value(CAM1_PWDN, 1);
	usleep_range(20000, 25000);

	return 1;

//ov5647_reg_fail:
ov5647_iovdd_fail:
	regulator_disable(pw->avdd);

ov5647_avdd_fail:
	tegra_io_dpd_enable(&csia_io);
	pr_err("%s failed.\n", __func__);
	return -ENODEV;
}

static int ov5647_power_off(struct ov5647_power_rail *pw)
{
	//return 0;

	if (unlikely(WARN_ON(!pw || !pw->iovdd || !pw->avdd))) {
		tegra_io_dpd_enable(&csia_io);
		return -EFAULT;
	}

	gpio_set_value(CAM1_GPIO, 0);
	gpio_set_value(CAM1_PWDN, 0);

	regulator_disable(pw->iovdd);
	regulator_disable(pw->avdd);

	/* put CSIA/B IOs into DPD mode to save additional power for ardbeg */
	tegra_io_dpd_enable(&csia_io);
	return 0;
}

static int ov5647_g_chip_ident(struct v4l2_subdev *sd,
			       struct v4l2_dbg_chip_ident *id)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (id->match.type != V4L2_CHIP_MATCH_I2C_ADDR)
		return -EINVAL;

	if (id->match.addr != client->addr)
		return -ENODEV;

	id->ident	= V4L2_IDENT_OV5647;
	id->revision	= 0;

	return 0;
}

static int ov5647_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5647_priv *priv = to_ov5647(client);
	int err;

	pr_info("%s: on: %d\n", __func__, on);

	if (on) {
		priv->status = STATUS_NEED_INIT;
		//err = ov5647_mclk_enable(priv);
		//if (!err)
			err = ov5647_power_on(&priv->power);
		if (err)
			return err;
		// Hard reset
		//err = ov5647_write_reg(priv->i2c_client, 0x0103, 0x01);
		return 0;
		//if (err < 0)
			//ov5647_mclk_disable(priv);
		//return err;
	} else if (!on) {
		ov5647_power_off(&priv->power);
		//ov5647_mclk_disable(priv);
		return 0;
	} else
		return -EINVAL;
}

static int ov5647_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov5647_priv *priv = container_of(ctrl->handler, struct ov5647_priv, hdl);
	u16 addr;
	u8 val[2] = {0};
	int err;
	switch (ctrl->id) {
	case OV5647_CID_REG_R:
		addr = priv->reg_addr & ~(OV5647_CID_REG_WMASK >> 16);
		if ((err = ov5647_read_reg(priv->i2c_client, addr, &val[1])) != 0)
			return err;
		if (priv->reg_addr & (OV5647_CID_REG_WMASK >> 16)) {
			if ((err = ov5647_read_reg(priv->i2c_client, addr + 1, &val[0])) != 0)
				return err;
			ctrl->val = ((u16)val[1] << 8) | val[0];
			//pr_info("%s: 0x%04x=0x%04x\n", __func__, addr, ctrl->val & 0xffff);
		} else {
			ctrl->val = val[1];
			//pr_info("%s: 0x%04x=0x%02x\n", __func__, addr, ctrl->val & 0xff);
		}
		return 0;
	}
	return -EINVAL;
}

static int ov5647_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov5647_priv *priv = container_of(ctrl->handler, struct ov5647_priv, hdl);
	unsigned int addr;
	int err;
	if (priv->status & STATUS_NEED_INIT)
		return 0;
	switch (ctrl->id) {
	case OV5647_CID_LED:
		gpio_set_value(CAM1_GPIO, ctrl->val ? 1 : 0);
		ctrl->val = ctrl->val ? 0x55aa : 0;
		return 0;
	case OV5647_CID_REG_W:
		addr = (ctrl->val & ~OV5647_CID_REG_WMASK) >> 16;
		if (ctrl->val & OV5647_CID_REG_WMASK) {
			u8 data[] = {addr >> 8, addr, ctrl->val >> 8, ctrl->val};
			if ((err = ov5647_write_seq(priv->i2c_client, data, sizeof(data))) != 0)
				return err;
			//pr_info("%s: 0x%04x=0x%04x\n", __func__, addr, ctrl->val & 0xffff);
		} else {
			if ((err = ov5647_write_reg(priv->i2c_client, addr, ctrl->val)) != 0)
				return err;
			//pr_info("%s: 0x%04x=0x%02x\n", __func__, addr, ctrl->val & 0xff);
		}
		return 0;
	case OV5647_CID_REG_R:
		priv->reg_addr = ctrl->val >> 16;
		return 0;
	}
	return -EINVAL;
}

static const struct v4l2_ctrl_ops ov5647_ctrl_ops = {
	.g_volatile_ctrl = ov5647_g_volatile_ctrl,
	.s_ctrl = ov5647_s_ctrl,
};

static const struct v4l2_subdev_core_ops ov5647_subdev_core_ops = {
	.g_chip_ident	= ov5647_g_chip_ident,
	.s_power	= ov5647_s_power,
};

static const struct v4l2_subdev_ops ov5647_subdev_ops = {
	.core	= &ov5647_subdev_core_ops,
	.video	= &ov5647_subdev_video_ops,
};

static void ov5647_add_reg_ctrl(struct ov5647_priv *priv)
{
	struct v4l2_ctrl_config ctrl = {
		.ops = &ov5647_ctrl_ops,
		.id = OV5647_CID_REG_W,
		.name = "Register write (debug)",
		.type = V4L2_CTRL_TYPE_BITMASK,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.min = 0,
		.max = 0xffffffff,
		.def = 0,
	};
	v4l2_ctrl_new_custom(&priv->hdl, &ctrl, NULL);
	ctrl.name = "Register read (debug)";
	ctrl.id = OV5647_CID_REG_R;
	v4l2_ctrl_new_custom(&priv->hdl, &ctrl, NULL);
}

static int ov5647_check_sensor_id(struct ov5647_priv *priv)
{
	int err;
	u8 id[2];

	//ov5647_mclk_enable(priv);
	err = ov5647_power_on(&priv->power);
	if (err < 0)
		return err;

	err = ov5647_read_reg(priv->i2c_client, 0x300a, &id[0]);
	if (err)
		goto failed;
	err = ov5647_read_reg(priv->i2c_client, 0x300b, &id[1]);
	if (err)
		goto failed;
	if (id[0] != 0x56 || id[1] != 0x47) {
		pr_err("%s: sensor id checking failed: 0x%02x%02x\n", __func__, id[0], id[1]);
		err = -ENODEV;
	}

failed:
	ov5647_power_off(&priv->power);
	//ov5647_mclk_disable(priv);
	return err;
}

static int ov5647_power_get(struct ov5647_priv *priv)
{
	struct ov5647_power_rail *pw = &priv->power;
	int err;

	/* ananlog 2.7v */
	pw->avdd = devm_regulator_get(&priv->i2c_client->dev, "vana");
	if (IS_ERR(pw->avdd)) {
		err = PTR_ERR(pw->avdd);
		pw->avdd = NULL;
		dev_err(&priv->i2c_client->dev, "Failed to get regulator vana: %d\n", err);
		return err;
	}

	/* digital 1.2v */
	pw->dvdd = devm_regulator_get(&priv->i2c_client->dev, "vdig");
	if (IS_ERR(pw->dvdd)) {
		err = PTR_ERR(pw->dvdd);
		pw->dvdd = NULL;
		dev_err(&priv->i2c_client->dev, "Failed to get regulator vdig: %d\n", err);
		return err;
	}

	/* IO 1.8v */
	//pw->iovdd = devm_regulator_get(&priv->i2c_client->dev, "vif");
	pw->iovdd = devm_regulator_get(&priv->i2c_client->dev, "vdd_cam_1v8_cam");
	if (IS_ERR(pw->iovdd)) {
		err = PTR_ERR(pw->iovdd);
		pw->iovdd = NULL;
		//dev_err(&priv->i2c_client->dev, "Failed to get regulator vif: %d\n", err);
		dev_err(&priv->i2c_client->dev, "Failed to get regulator vdd_cam_1v8_cam: %d\n", err);
		return err;
	}

	return 0;
}

static int ov5647_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ov5647_priv *priv;
	//const char *mclk_name;
	int err;

	if (test_mode)
		pr_info("%s: test_mode: 0x%02x\n", __func__, test_mode);

	pr_info("[ov5647] probing sensor.\n");
	priv = devm_kzalloc(&client->dev,
			sizeof(struct ov5647_priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev, "unable to allocate memory!\n");
		return -ENOMEM;
	}

#if 0
	if (client->dev.of_node)
		priv->pdata = ov5647_parse_dt(client);
	else
#endif
		priv->pdata = client->dev.platform_data;

	if (!priv->pdata) {
		dev_err(&client->dev, "unable to get platform data\n");
		return -EFAULT;
	}

	priv->i2c_client = client;
	priv->mode = OV5647_MODE_DEF;
	priv->status = STATUS_NEED_INIT;
	priv->fmt = &ov5647_colour_fmts[0];

#if 0
	mclk_name = priv->pdata->mclk_name ?
		    priv->pdata->mclk_name : "default_mclk";
	priv->mclk = devm_clk_get(&client->dev, mclk_name);
	if (IS_ERR(priv->mclk)) {
		dev_err(&client->dev, "unable to get clock %s\n", mclk_name);
		return PTR_ERR(priv->mclk);
	}
#endif

	err = ov5647_power_get(priv);
	if (err)
		return err;

	i2c_set_clientdata(client, priv);

	if (gpio_request(CAM1_GPIO, "CAM1_GPIO")) {
		gpio_free(CAM1_GPIO);
		gpio_request(CAM1_GPIO, "CAM1_GPIO");
	}
	if (gpio_request(CAM1_PWDN, "CAM1_PWDN")) {
		gpio_free(CAM1_PWDN);
		gpio_request(CAM1_PWDN, "CAM1_PWDN");
	}

	gpio_direction_output(CAM1_GPIO, 0);
	gpio_direction_output(CAM1_PWDN, 0);

	err = ov5647_check_sensor_id(priv);
	if (err) {
		gpio_free(CAM1_GPIO);
		gpio_free(CAM1_PWDN);
		return err;
	}

	v4l2_i2c_subdev_init(&priv->subdev, client, &ov5647_subdev_ops);
	v4l2_ctrl_handler_init(&priv->hdl, OV5647_CID_NUM);
	// ops, id, min, max, step, def
	v4l2_ctrl_new_std(&priv->hdl, &ov5647_ctrl_ops,
			OV5647_CID_LED, 0, 1, 1, 0);
	ov5647_add_reg_ctrl(priv);
	priv->subdev.ctrl_handler = &priv->hdl;
	v4l2_ctrl_handler_setup(&priv->hdl);
	if (priv->hdl.error) {
		gpio_free(CAM1_GPIO);
		gpio_free(CAM1_PWDN);
		return priv->hdl.error;
	}

	return 0;
}

static int ov5647_power_put(struct ov5647_power_rail *pw)
{
	if (unlikely(!pw))
		return -EFAULT;

	pw->avdd = NULL;
	pw->iovdd = NULL;
	pw->dvdd = NULL;

	return 0;
}

static int ov5647_remove(struct i2c_client *client)
{
	struct soc_camera_subdev_desc *ssdd;
	struct ov5647_priv *priv;

	pr_info("[ov5647] removing sensor.\n");
	if (!client) {
		pr_info("[ov5647] Error: no i2c_client.\n");
		return 0;
	}

	ssdd = soc_camera_i2c_to_desc(client);
	if (!ssdd)
		pr_info("[ov5647] Error: no soc_camera_subdev_desc.\n");
	else if (ssdd->free_bus)
		ssdd->free_bus(ssdd);

	priv = i2c_get_clientdata(client);
	if (!priv)
		pr_info("[ov5647] Error: no clientdata.\n");
	else
		ov5647_power_put(&priv->power);

	v4l2_device_unregister_subdev(&priv->subdev);
	v4l2_ctrl_handler_free(&priv->hdl);

	gpio_free(CAM1_GPIO);
	gpio_free(CAM1_PWDN);

	return 0;
}

static const struct i2c_device_id ov5647_id[] = {
	{ "ov5647_v4l2", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ov5647_id);

static struct i2c_driver ov5647_i2c_driver = {
	.driver = {
		.name = "ov5647_v4l2",
		.owner = THIS_MODULE,
	},
	.probe = ov5647_probe,
	.remove = ov5647_remove,
	.id_table = ov5647_id,
};

module_i2c_driver(ov5647_i2c_driver);

MODULE_DESCRIPTION("SoC Camera driver for OmniVision OV5647");
MODULE_LICENSE("GPL v2");
