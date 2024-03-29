/* arch/arm/mach-msm/board-liberty.c
 * Copyright (C) 2009 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/i2c-msm.h>
#include <linux/irq.h>
#include <linux/leds.h>
#include <linux/switch.h>
#include <linux/synaptics_i2c_rmi.h>
#include <linux/bma150.h>
#include <linux/capella_cm3602.h>
#include <linux/sysdev.h>
#include <linux/android_pmem.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/gpio_event.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/flash.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/setup.h>
#include <asm/mach/mmc.h>
#ifdef CONFIG_CACHE_L2X0
#include <asm/hardware/cache-l2x0.h>
#endif

#include <mach/hardware.h>
#include <mach/system.h>
#include <mach/vreg.h>
/* #include <mach/gpio_chip.h> */
#include <mach/board.h>
#include <mach/board_htc.h>
#include <mach/msm_serial_hs.h>
#include <mach/atmega_microp.h>
/* #include <mach/msm_tssc.h> */
#ifdef CONFIG_MSM_SMEM_BATTCHG
#include <mach/htc_battery_smem_def.h>
#include "proc_comm_wince.h"
#else
#include <mach/htc_battery.h>
#endif
#include <mach/htc_pwrsink.h>
#include <mach/perflock.h>
#include <mach/drv_callback.h>
#include <mach/camera.h>
#include <mach/msm_serial_debugger.h>
#include <mach/msm_iomap.h>
#include <mach/msm_hsusb.h>
#include <mach/htc_usb.h>
#include <mach/htc_headset_mgr.h>
#include <mach/htc_headset_gpio.h>
#include <mach/htc_headset_microp.h>
#include <mach/htc_acoustic_wince.h>

#include "devices.h"
#include "board-photon.h"
#include "proc_comm.h"
#include "../../../drivers/staging/android/timed_gpio.h"


void msm_init_irq(void);
void msm_init_gpio(void);
void config_liberty_camera_on_gpios(void);
void config_liberty_camera_off_gpios(void);

void config_liberty_proximity_gpios(int on);
void kgsl_boot_reset(void);

#ifdef CONFIG_MICROP_COMMON
void __init liberty_microp_init(void);
#endif

///////////////////////////////////////////////////////////////////
// Cotulla: debug function
static int io_already_inited;

void photon_check_debug(void)
{
	if (!io_already_inited) return;

	if (!(*(volatile uint32_t*)(MSM_GPIO2_BASE + 0xC20) & 0x10))
	{            
	    while (1) 
	    {
		*(volatile uint32_t*)(MSM_GPIO2E_BASE + 0x504) |=  0x200; // AARM
		*(volatile uint32_t*)(MSM_GPIO2_BASE  + 0xC08) |=  0x200; // OUT
		*(volatile uint32_t*)(MSM_GPIO2_BASE  + 0xC00) &= ~0x200; // LOW
            }
	}		
}

static void sdc1_boot_reset(void)
{
	volatile uint32_t* sdc1_clk = MSM_CLK_CTL_BASE + 0x214;

	*sdc1_clk |= (1 << 9);
   	mdelay(1);
	*sdc1_clk &= ~(1 << 9);
}


// stub function for now
int photon_is_nand_boot(void)
{
	return 0;
}

///////////////////////////////////////////////////////////////////

/* HTC_HEADSET_GPIO Driver */
static struct htc_headset_gpio_platform_data htc_headset_gpio_data = {
	.hpin_gpio		= LIBERTY_GPIO_35MM_HEADSET_DET,
	.key_enable_gpio	= 0,
	.mic_select_gpio	= 0,
};

static struct platform_device htc_headset_gpio = {
	.name	= "HTC_HEADSET_GPIO",
	.id	= -1,
	.dev	= {
		.platform_data	= &htc_headset_gpio_data,
	},
};

/* HTC_HEADSET_MICROP Driver */
static struct htc_headset_microp_platform_data htc_headset_microp_data = {
	.remote_int		= 1 << 5,
	.remote_irq		= MSM_uP_TO_INT(5),
	.remote_enable_pin	= 0,
	.adc_channel		= 0x01,
	.adc_remote		= {0, 33, 38, 82, 95, 167},
};

static struct platform_device htc_headset_microp = {
	.name	= "HTC_HEADSET_MICROP",
	.id	= -1,
	.dev	= {
		.platform_data	= &htc_headset_microp_data,
	},
};

/* HTC_HEADSET_MGR Driver */
static struct platform_device *headset_devices[] = {
	&htc_headset_microp,
	&htc_headset_gpio,
	/* Please put the headset detection driver on the last */
};

static struct htc_headset_mgr_platform_data htc_headset_mgr_data = {
	.headset_devices_num	= ARRAY_SIZE(headset_devices),
	.headset_devices	= headset_devices,
};

//////////////////////////////////////////////
// BATTERY 
//////////////////////////////////////////////
#if defined(CONFIG_MSM_SMEM_BATTCHG)
struct platform_device msm_device_htc_battery_smem = {
	.name = "htc_battery",
	.id = -1,
};

static smem_batt_t htcphoton_htc_battery_smem_pdata = {
	//r0bin: no need battery detection
	.gpio_battery_detect = 0, 
	//r0bin: photon dont have AC charging cable
	.gpio_ac_detect = 0,
	.gpio_charger_enable = PHOTON_GPIO_CHARGE_EN_N,
	.gpio_charger_fast_dis = PHOTON_FAST_CHARGER_DIS, 
	.gpio_charger_fast_en = PHOTON_FAST_CHARGER_EN,
	.smem_offset = 0xfc110,
	.smem_field_size = 2,
};
#else
static struct htc_battery_platform_data htc_battery_pdev_data = {
	.guage_driver = GUAGE_MODEM,
	.charger = LINEAR_CHARGER,
	.m2a_cable_detect = 1,
};

static struct platform_device htc_battery_pdev = {
	.name = "htc_battery",
	.id = -1,
	.dev	= {
		.platform_data = &htc_battery_pdev_data,
	},
};
#endif
//////////////////////////////////////////////

static int capella_cm3602_power(int pwr_device, uint8_t enable);

static struct microp_function_config microp_functions[] = {
	{
		.name   = "microp_intrrupt",
		.category = MICROP_FUNCTION_INTR,
	},
	{
		.name   = "reset-int",
		.category = MICROP_FUNCTION_RESET_INT,
		.int_pin = 1 << 8,
	},
};

static struct microp_function_config microp_lightsensor = {
	.name = "light_sensor",
	.category = MICROP_FUNCTION_LSENSOR,
	//cardsharing-x levels defined in lightsensor driver!
	.levels = { 0, 0x21, 0x4D, 0xDC, 0x134, 0x18D, 0x1E5, 0x2BA, 0x35C, 0x3FF },
	//.levels = { 0, 0x21, 0x4D, 0xDC, 0x134, 0x18D, 0x1E5, 0x3FF, 0x3FF, 0x3FF },
	.channel = 3,
	.int_pin = 1 << 9,
	.golden_adc = 0xC0,
	.ls_power = capella_cm3602_power,
};

static struct lightsensor_platform_data lightsensor_data = {
	.config = &microp_lightsensor,
	.irq = MSM_uP_TO_INT(9),
};

static struct microp_led_config led_config[] = {
	{
		.name   = "amber",
		.type = LED_RGB,
	},
	{
		.name   = "green",
		.type = LED_RGB,
	},
	{
		.name = "button-backlight",
		.type = LED_PWM,
		.led_pin = 1 << 5,
		.init_value = 1,
		.fade_time = 5,
	},
};

static struct microp_led_platform_data microp_leds_data = {
	.num_leds	= ARRAY_SIZE(led_config),
	.led_config	= led_config,
};


static struct bma150_platform_data liberty_g_sensor_pdata = {
	.microp_new_cmd = 1,
	.chip_layout = 1,
};

static struct platform_device microp_devices[] = {
	{
		.name = "lightsensor_microp",
		.dev		= {
			.platform_data	= &lightsensor_data,
		},
	},
	{
		.name		= "leds-microp",
		.id		= -1,
		.dev		= {
			.platform_data	= &microp_leds_data,
		},
	},
	{
		.name = BMA150_G_SENSOR_NAME,
		.dev = {
			.platform_data = &liberty_g_sensor_pdata,
		},
	},
	{
		.name	= "HTC_HEADSET_MGR",
		.id	= -1,
		.dev	= {
			.platform_data	= &htc_headset_mgr_data,
		},
	},
};

static struct microp_i2c_platform_data microp_data = {
	.num_functions   = ARRAY_SIZE(microp_functions),
	.microp_function = microp_functions,
	.num_devices = ARRAY_SIZE(microp_devices),
	.microp_devices = microp_devices,
	.gpio_reset = LIBERTY_GPIO_UP_RESET_N,
	.spi_devices = SPI_GSENSOR,
};

static struct synaptics_i2c_rmi_platform_data liberty_ts_t1021_data[] = {
	{
		.version = 0x0100,
		.inactive_left = -1 * 0x10000 / 320,
		.inactive_right = -1 * 0x10000 / 320,
		.inactive_top = -1 * 0x10000 / 480,
		.inactive_bottom = -72 * 0x10000 / 480,
	}
};

static int liberty_phy_init_seq[] = { 0x1D, 0x0D, 0x1D, 0x10, -1 };
static void liberty_phy_reset(void)
{
	int ret;
	printk(KERN_INFO "msm_hsusb_phy_reset\n");
	ret = msm_proc_comm(PCOM_MSM_HSUSB_PHY_RESET,
			NULL, NULL);
	if (ret)
		printk(KERN_INFO "%s failed\n", __func__);
}

#ifdef CONFIG_USB_ANDROID
static void usb_connected(int on) {/*printk("OLD%s notifier, online=%d\n",__func__,on);*/}

static struct msm_hsusb_platform_data msm_hsusb_pdata = {
	.phy_init_seq		= liberty_phy_init_seq,
	.phy_reset		= liberty_phy_reset,
	.usb_id_pin_gpio =  LIBERTY_GPIO_USB_ID_PIN,
	.usb_connected = usb_connected,
};

static struct usb_mass_storage_platform_data mass_storage_pdata = {
	.nluns		= 1,
	.vendor		= "HTC",
	.product	= "Android Phone",
	.release	= 0x0100,
};

static struct platform_device usb_mass_storage_device = {
	.name	= "usb_mass_storage",
	.id	= -1,
	.dev	= {
		.platform_data = &mass_storage_pdata,
	},
};

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id	= 0x0bb4,
	.product_id	= 0x0c92,
	.version	= 0x0100,
	.product_name		= "Android Phone",
	.manufacturer_name	= "HTC",
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_all),
	.functions = usb_functions_all,
};

static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id		= -1,
	.dev		= {
		.platform_data = &android_usb_pdata,
	},
};
#endif


static struct i2c_board_info i2c_devices[] = {
	{
		I2C_BOARD_INFO(SYNAPTICS_T1021_NAME, 0x21),
		.platform_data = &liberty_ts_t1021_data,
		.irq = LIBERTY_GPIO_TO_INT(LIBERTY_GPIO_TP_ATT_N)
	},
	{
		I2C_BOARD_INFO(MICROP_I2C_NAME, 0xCC >> 1),
		.platform_data = &microp_data,
		.irq = LIBERTY_GPIO_TO_INT(LIBERTY_GPIO_UP_INT_N)
	},
};

static struct pwr_sink liberty_pwrsink_table[] = {
	{
		.id     = PWRSINK_AUDIO,
		.ua_max = 100000,
	},
	{
		.id     = PWRSINK_BACKLIGHT,
		.ua_max = 125000,
	},
	{
		.id     = PWRSINK_LED_BUTTON,
		.ua_max = 0,
	},
	{
		.id     = PWRSINK_LED_KEYBOARD,
		.ua_max = 0,
	},
	{
		.id     = PWRSINK_GP_CLK,
		.ua_max = 0,
	},
	{
		.id     = PWRSINK_BLUETOOTH,
		.ua_max = 15000,
	},
	{
		.id     = PWRSINK_CAMERA,
		.ua_max = 0,
	},
	{
		.id     = PWRSINK_SDCARD,
		.ua_max = 0,
	},
	{
		.id     = PWRSINK_VIDEO,
		.ua_max = 0,
	},
	{
		.id     = PWRSINK_WIFI,
		.ua_max = 200000,
	},
	{
		.id     = PWRSINK_SYSTEM_LOAD,
		.ua_max = 100000,
		.percent_util = 38,
	},
};


#define SND(num, desc) { .name = desc, .id = num }
static struct snd_endpoint snd_endpoints_list[] = {
	SND(0, "HANDSET"),
	SND(1, "SPEAKER"),
	SND(2, "HEADSET"),
	SND(2, "NO_MIC_HEADSET"),
	SND(3, "BT"),
	SND(3, "BT_EC_OFF"),

	SND(13, "SPEAKER_MIC"),

	SND(0x11, "IDLE"),
	SND(256, "CURRENT"),
};
#undef SND

static struct msm_snd_endpoints photon_snd_endpoints = {
        .endpoints = snd_endpoints_list,
        .num = ARRAY_SIZE(snd_endpoints_list),
};

static struct platform_device photon_snd = {
	.name = "msm_snd",
	.id = -1,
	.dev	= {
		.platform_data = &photon_snd_endpoints,
	},
};


static int liberty_pwrsink_resume_early(struct platform_device *pdev)
{
	htc_pwrsink_set(PWRSINK_SYSTEM_LOAD, 7);
	return 0;
}

static void liberty_pwrsink_resume_late(struct early_suspend *h)
{
	htc_pwrsink_set(PWRSINK_SYSTEM_LOAD, 38);
}

static void liberty_pwrsink_suspend_early(struct early_suspend *h)
{
	htc_pwrsink_set(PWRSINK_SYSTEM_LOAD, 7);
}

static int liberty_pwrsink_suspend_late(struct platform_device *pdev, pm_message_t state)
{
	htc_pwrsink_set(PWRSINK_SYSTEM_LOAD, 1);
	return 0;
}

static struct pwr_sink_platform_data liberty_pwrsink_data = {
	.num_sinks      = ARRAY_SIZE(liberty_pwrsink_table),
	.sinks          = liberty_pwrsink_table,
	.suspend_late	= liberty_pwrsink_suspend_late,
	.resume_early	= liberty_pwrsink_resume_early,
	.suspend_early	= liberty_pwrsink_suspend_early,
	.resume_late	= liberty_pwrsink_resume_late,
};

static struct platform_device liberty_pwr_sink = {
	.name = "htc_pwrsink",
	.id = -1,
	.dev    = {
		.platform_data = &liberty_pwrsink_data,
	},
};

static struct msm_pmem_setting pmem_setting = {
	.pmem_start = MSM_PMEM_MDP_BASE,
	.pmem_size = MSM_PMEM_MDP_SIZE,
	.pmem_adsp_start = MSM_PMEM_ADSP_BASE,
	.pmem_adsp_size = MSM_PMEM_ADSP_SIZE,
	.pmem_camera_start = MSM_PMEM_CAMERA_BASE,
	.pmem_camera_size = MSM_PMEM_CAMERA_SIZE,
	.ram_console_start = MSM_RAM_CONSOLE_BASE,
	.ram_console_size = MSM_RAM_CONSOLE_SIZE,
        .kgsl_start = MSM_GPU_MEM_BASE,
        .kgsl_size = MSM_GPU_MEM_SIZE,
};


#ifdef CONFIG_MSM_CAMERA
static struct resource msm_camera_resources[] = {
	{
		.start	= MSM_VFE_PHYS,
		.end	= MSM_VFE_PHYS + MSM_VFE_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_VFE,
		.end	= INT_VFE,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct msm_camera_device_platform_data msm_camera_device_data = {
	.camera_gpio_on  = config_liberty_camera_on_gpios,
	.camera_gpio_off = config_liberty_camera_off_gpios,
	.ioext.mdcphy = MSM_MDC_PHYS,
	.ioext.mdcsz  = MSM_MDC_SIZE,
	.ioext.appphy = MSM_CLK_CTL_PHYS,
	.ioext.appsz  = MSM_CLK_CTL_SIZE,
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k4e1gx_data = {
	.sensor_name    = "s5k4e1gx",
	.sensor_reset   = LIBERTY_GPIO_CAM1_RST_N,
	.vcm_pwd        = LIBERTY_GPIO_VCM_PWDN,
	.pdata          = &msm_camera_device_data,
	.flash_type     = MSM_CAMERA_FLASH_LED,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources)
};

static struct platform_device msm_camera_sensor_s5k4e1gx = {
	.name      = "msm_camera_s5k4e1gx",
	.dev       = {
		.platform_data = &msm_camera_sensor_s5k4e1gx_data,
	},
};
#endif

static struct platform_device liberty_rfkill = {
	.name = "liberty_rfkill",
	.id = -1,
};

/* Proximity Sensor (Capella_CM3602)*/
static int __capella_cm3602_power(int on)
{
	int rc;
	struct vreg *vreg = vreg_get(0, "wlan");
	if (!vreg) {
		printk(KERN_ERR "%s: vreg error\n", __func__);
		return -EIO;
	}
	rc = vreg_set_level(vreg, 2800);

	printk(KERN_DEBUG "%s: Turn the capella_cm3602 power %s\n",
		__func__, (on) ? "on" : "off");
	if (on) {
		config_liberty_proximity_gpios(1);
		gpio_direction_output(LIBERTY_GPIO_PROXIMITY_EN, 1);
		rc = vreg_enable(vreg);
		if (rc < 0)
			printk(KERN_ERR "%s: vreg enable failed\n", __func__);
	} else {
		rc = vreg_disable(vreg);
		if (rc < 0)
			printk(KERN_ERR "%s: vreg disable failed\n", __func__);
		gpio_direction_output(LIBERTY_GPIO_PROXIMITY_EN, 0);
		config_liberty_proximity_gpios(0);
	}

	return rc;
}

static DEFINE_MUTEX(capella_cm3602_lock);
static unsigned int als_power_control;

static int capella_cm3602_power(int pwr_device, uint8_t enable)
{
	unsigned int old_status = 0;
	int ret = 0, on = 0;
	mutex_lock(&capella_cm3602_lock);

	old_status = als_power_control;
	if (enable)
		als_power_control |= pwr_device;
	else
		als_power_control &= ~pwr_device;

	on = als_power_control ? 1 : 0;
	if (old_status == 0 && on)
		ret = __capella_cm3602_power(1);
	else if (!on)
		ret = __capella_cm3602_power(0);

	mutex_unlock(&capella_cm3602_lock);
	return ret;
}

static struct capella_cm3602_platform_data capella_cm3602_pdata = {
	.p_out = LIBERTY_GPIO_PROXIMITY_INT,
	.p_en = LIBERTY_GPIO_PROXIMITY_EN,
	.power = capella_cm3602_power,
	.irq = MSM_GPIO_TO_INT(LIBERTY_GPIO_PROXIMITY_INT),
};

static struct platform_device capella_cm3602 = {
	.name = CAPELLA_CM3602,
	.dev = {
		.platform_data = &capella_cm3602_pdata
	}
};
/* End Proximity Sensor (Capella_CM3602)*/

#define LIB_MICROP_VER	0x02

static struct msm_i2c_device_platform_data msm_i2c_pdata = {
	.i2c_clock = 400000,
	.clock_strength = GPIO_8MA,
	.data_strength = GPIO_4MA,
};

static void __init msm_device_i2c_init(void)
{
	msm_i2c_gpio_init();
	msm_device_i2c.dev.platform_data = &msm_i2c_pdata;
}

static struct timed_gpio liberty_timed_gpios_str[] = {
	{
		.name = "vibrator",
		.gpio = LIBERTY_GPIO_VIB_3V_EN,
		.max_timeout = 15000,
	},
};

static struct timed_gpio_platform_data liberty_timed_gpio_data = {
	.num_gpios	= ARRAY_SIZE(liberty_timed_gpios_str),
	.gpios		= liberty_timed_gpios_str,
};

static struct platform_device liberty_timed_gpios = {
	.name		= TIMED_GPIO_NAME,
	.id		= -1,
	.dev		= {
		.platform_data	= &liberty_timed_gpio_data,
	},
};

/* acoustic stuf for photon */
extern void photon_headset_amp(int enabled);
static struct htc_acoustic_wce_board_data htcphoton_acoustic_data = {
    .set_headset_amp = photon_headset_amp,
};

static struct platform_device *devices[] __initdata = {
	&msm_device_i2c,
#ifdef CONFIG_MSM_SMEM_BATTCHG
	&msm_device_htc_battery_smem,
#else
	&htc_battery_pdev,
#endif
    &photon_snd,
	&msm_camera_sensor_s5k4e1gx,
	&liberty_rfkill,
#ifdef CONFIG_HTC_PWRSINK
	&liberty_pwr_sink,
#endif
	&capella_cm3602,
	&liberty_timed_gpios,
	&acoustic_device,
};

extern struct sys_timer msm_timer;

static void __init photon_init_irq(void)
{
	printk("photon_init_irq()\n");
	msm_init_irq();
}

static uint cpld_iset;
static uint cpld_charger_en;
static uint cpld_usb_h2w_sw;
static uint opt_disable_uart3;
static char *keycaps = "";

module_param_named(iset, cpld_iset, uint, 0);
module_param_named(charger_en, cpld_charger_en, uint, 0);
module_param_named(usb_h2w_sw, cpld_usb_h2w_sw, uint, 0);
module_param_named(disable_uart3, opt_disable_uart3, uint, 0);
module_param_named(keycaps, keycaps, charp, 0);

static char bt_chip_id[10] = "brfxxxx";
module_param_string(bt_chip_id, bt_chip_id, sizeof(bt_chip_id), S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(bt_chip_id, "BT's chip id");

static char bt_fw_version[10] = "v2.0.38";
module_param_string(bt_fw_version, bt_fw_version, sizeof(bt_fw_version), S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(bt_fw_version, "BT's fw version");

static void liberty_reset(void)
{
	while (1)
	{
	  *(volatile uint32_t*)(MSM_GPIO2E_BASE + 0x504) |=  0x200; // AARM
	  *(volatile uint32_t*)(MSM_GPIO2_BASE  + 0xC08) |=  0x200; // OUT
	  *(volatile uint32_t*)(MSM_GPIO2_BASE  + 0xC00) &= ~0x200; // LOW
	}

//	gpio_set_value(LIBERTY_GPIO_PS_HOLD, 0);
}

static uint32_t proximity_on_gpio_table[] = {
	PCOM_GPIO_CFG(LIBERTY_GPIO_PROXIMITY_INT,
		0, GPIO_INPUT, GPIO_NO_PULL, 0), /* PS_VOUT */
};

static uint32_t proximity_off_gpio_table[] = {
	PCOM_GPIO_CFG(LIBERTY_GPIO_PROXIMITY_INT,
		0, GPIO_INPUT, GPIO_PULL_DOWN, 0) /* PS_VOUT */
};

static struct i2c_board_info i2c_camera_devices[] = {
	{
		I2C_BOARD_INFO("s5k4e1gx", 0x20 >> 1),/*5M samsung bayer sensor driver*/
	},
};

static uint32_t camera_off_gpio_table[] = {
	/* CAMERA */
	PCOM_GPIO_CFG(0, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT0 */
	PCOM_GPIO_CFG(1, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT1 */

	PCOM_GPIO_CFG(2, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT2 */
	PCOM_GPIO_CFG(3, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT3 */
	PCOM_GPIO_CFG(4, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT4 */
	PCOM_GPIO_CFG(5, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT5 */
	PCOM_GPIO_CFG(6, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT6 */
	PCOM_GPIO_CFG(7, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT7 */
	PCOM_GPIO_CFG(8, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT8 */
	PCOM_GPIO_CFG(9, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT9 */
	PCOM_GPIO_CFG(10, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT10 */
	PCOM_GPIO_CFG(11, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* DAT11 */
	PCOM_GPIO_CFG(12, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* PCLK */
	PCOM_GPIO_CFG(13, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* HSYNC_IN */
	PCOM_GPIO_CFG(14, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_4MA), /* VSYNC_IN */
	PCOM_GPIO_CFG(15, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_4MA), /* MCLK */

/* PCOM_GPIO_CFG(27, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),//CAM_I2C_SDA */
/* PCOM_GPIO_CFG(49, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),//CAM_I2C_SCL */
};

static uint32_t camera_on_gpio_table[] = {
	/* CAMERA */
	PCOM_GPIO_CFG(2, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT2 */
	PCOM_GPIO_CFG(3, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT3 */
	PCOM_GPIO_CFG(4, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT4 */
	PCOM_GPIO_CFG(5, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT5 */
	PCOM_GPIO_CFG(6, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT6 */
	PCOM_GPIO_CFG(7, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT7 */
	PCOM_GPIO_CFG(8, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT8 */
	PCOM_GPIO_CFG(9, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT9 */
	PCOM_GPIO_CFG(10, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT10 */
	PCOM_GPIO_CFG(11, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT11 */
	PCOM_GPIO_CFG(12, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_16MA), /* PCLK */
	PCOM_GPIO_CFG(13, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* HSYNC_IN */
	PCOM_GPIO_CFG(14, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* VSYNC_IN */
	PCOM_GPIO_CFG(15, 1, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_16MA), /* MCLK */
};

void config_liberty_camera_on_gpios(void)
{
	config_gpio_table(camera_on_gpio_table,
		ARRAY_SIZE(camera_on_gpio_table));
}

void config_liberty_camera_off_gpios(void)
{
	config_gpio_table(camera_off_gpio_table,
		ARRAY_SIZE(camera_off_gpio_table));
}

void config_liberty_proximity_gpios(int on)
{
	if (on)
		config_gpio_table(proximity_on_gpio_table,
			ARRAY_SIZE(proximity_on_gpio_table));
	else
		config_gpio_table(proximity_off_gpio_table,
			ARRAY_SIZE(proximity_off_gpio_table));
}

/* for bcm */
static char bdaddress[20];
extern unsigned char *get_bt_bd_ram(void);

static void bt_export_bd_address(void)
{
	unsigned char cTemp[6];

	memcpy(cTemp, get_bt_bd_ram(), 6);
	sprintf(bdaddress, "%02x:%02x:%02x:%02x:%02x:%02x",
		cTemp[0], cTemp[1], cTemp[2], cTemp[3], cTemp[4], cTemp[5]);
	printk(KERN_INFO "YoYo--BD_ADDRESS=%s\n", bdaddress);
}

module_param_string(bdaddress, bdaddress, sizeof(bdaddress), S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(bdaddress, "BT MAC ADDRESS");

static uint32_t liberty_serial_debug_table[] = {
	/* config as serial debug uart */
	PCOM_GPIO_CFG(LIBERTY_GPIO_UART3_RX, 1,
			GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),	/* UART3 RX */
	PCOM_GPIO_CFG(LIBERTY_GPIO_UART3_TX, 1,
			GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),	/* UART3 TX */
};

static void liberty_config_serial_debug_gpios(void)
{
	config_gpio_table(liberty_serial_debug_table,
			ARRAY_SIZE(liberty_serial_debug_table));
}

static void __init config_gpios(void)
{
	liberty_config_serial_debug_gpios();
	config_liberty_camera_off_gpios();
}

static struct msm_acpu_clock_platform_data liberty_clock_data = {
	.acpu_switch_time_us = 50,
	.max_speed_delta_khz = 256000,
	.vdd_switch_time_us = 62,
	.power_collapse_khz = 19200,
	.wait_for_irq_khz = 200000,
};

static unsigned liberty_perf_acpu_table[] = {
	245760000,
	480000000,
	600000000,
};

static struct perflock_platform_data liberty_perflock_data = {
	.perf_acpu_table = liberty_perf_acpu_table,
	.table_size = ARRAY_SIZE(liberty_perf_acpu_table),
};

#ifdef CONFIG_SERIAL_MSM_HS
static struct msm_serial_hs_platform_data msm_uart_dm1_pdata = {
	.rx_wakeup_irq = MSM_GPIO_TO_INT(LIBERTY_GPIO_BT_HOST_WAKE),
	.inject_rx_on_wakeup = 0,
	.cpu_lock_supported = 1,

	/* for bcm */
	.bt_wakeup_pin_supported = 1,
	.bt_wakeup_pin = LIBERTY_GPIO_BT_CHIP_WAKE,
	.host_wakeup_pin = LIBERTY_GPIO_BT_HOST_WAKE,

};
#endif

static ssize_t liberty_virtual_keys_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
		/* center: x: call: 32, home: 96, menu: 160, back: 224, endcall: 288, y: 550 */
		return sprintf(buf,
			__stringify(EV_KEY) ":" __stringify(KEY_PHONE)       ":32:550:64:55"
			":" __stringify(EV_KEY) ":" __stringify(KEY_HOME)    ":96:550:64:55"
			":" __stringify(EV_KEY) ":" __stringify(KEY_MENU)    ":160:550:64:55"
			":" __stringify(EV_KEY) ":" __stringify(KEY_BACK)    ":224:550:64:55"
			":" __stringify(EV_KEY) ":" __stringify(KEY_SEARCH)     ":288:550:64:55"
			"\n");
}

static struct kobj_attribute liberty_synaptics_virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.synaptics-rmi-touchscreen",
		.mode = S_IRUGO,
	},
	.show = &liberty_virtual_keys_show,
};

static struct attribute *liberty_properties_attrs[] = {
	&liberty_synaptics_virtual_keys_attr.attr,
	NULL
};

static struct attribute_group liberty_properties_attr_group = {
	.attrs = liberty_properties_attrs,
};

static void __init photon_init(void)
{
	int rc;
	char *cid = NULL;
	struct kobject *properties_kobj;

	printk("photon_init() revision = 0x%X\n", system_rev);

	// Cotulla: vibro test
	//*(volatile unsigned int*)(MSM_GPIO1_BASE + 0x808) |= 0x1000000;
	//*(volatile unsigned int*)(MSM_GPIO1_BASE + 0x808) |= 0x1000000;		

/*        	
	while (1)
	{
		photon_check_debug();		
	}
*/        


// Cotulla: fix for KGSL run from haret
	kgsl_boot_reset();
	sdc1_boot_reset();

	board_get_cid_tag(&cid);

	/* for bcm */
	bt_export_bd_address();

	/*
	 * Setup common MSM GPIOS
	 */
	config_gpios();

	/* We need to set this pin to 0 only once on power-up; we will
	 * not actually enable the chip until we apply power to it via
	 * vreg.
	 */
	gpio_request(LIBERTY_GPIO_LS_EN, "ls_en");
	gpio_direction_output(LIBERTY_GPIO_LS_EN, 0);
	/* disable power for cm3602 chip */
	/* __capella_cm3602_power(0); */

	msm_hw_reset_hook = liberty_reset;

	msm_acpu_clock_init(&liberty_clock_data);
#ifdef CONFIG_MSM_SMEM_BATTCHG
	msm_proc_comm_wince_init();
	msm_device_htc_battery_smem.dev.platform_data = &htcphoton_htc_battery_smem_pdata;
#endif
	perflock_init(&liberty_perflock_data);

#if defined(CONFIG_MSM_SERIAL_DEBUGGER)
	if (!opt_disable_uart3)
		msm_serial_debug_init(MSM_UART3_PHYS, INT_UART3,
			&msm_device_uart3.dev, 1,
				MSM_GPIO_TO_INT(LIBERTY_GPIO_UART3_RX));
#endif

	// Set acoustic device specific parameters
    htc_acoustic_wce_board_data = &htcphoton_acoustic_data;
	
	msm_add_devices();
	printk("after msm_add_devices()\n");

#ifdef CONFIG_SERIAL_MSM_HS
	msm_device_uart_dm1.dev.platform_data = &msm_uart_dm1_pdata;
	msm_device_uart_dm1.name = "msm_serial_hs_bcm";	/* for bcm */
	msm_add_serial_devices(3);
#else
	msm_add_serial_devices(0);
#endif

	msm_add_serial_devices(2);
	/*
	msm_change_usb_id(0x0bb4, 0x0c10);
	*/
#ifdef CONFIG_USB_FUNCTION
	msm_add_usb_id_pin_gpio(LIBERTY_GPIO_USB_ID_PIN);
	msm_add_usb_devices(liberty_phy_reset, NULL);
#endif

#ifdef CONFIG_USB_ANDROID
	android_usb_pdata.products[0].product_id =
		android_usb_pdata.product_id;
	android_usb_pdata.serial_number = board_serialno();
	msm_hsusb_pdata.serial_number = board_serialno();
	msm_device_hsusb.dev.platform_data = &msm_hsusb_pdata;
	platform_device_register(&msm_device_hsusb);
	platform_device_register(&usb_mass_storage_device);
	platform_device_register(&android_usb_device);
#endif
	msm_add_mem_devices(&pmem_setting);

#ifdef CONFIG_MICROP_COMMON
	liberty_microp_init();
#endif

	rc = photon_init_mmc(system_rev);
	if (rc)
		printk(KERN_CRIT "%s: MMC init failure (%d)\n", __func__, rc);

	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		rc = sysfs_create_group(properties_kobj,
						&liberty_properties_attr_group);
	if (!properties_kobj || rc)
		pr_err("failed to create board_properties\n");

	/* probe camera driver */
	i2c_register_board_info(0, i2c_camera_devices, ARRAY_SIZE(i2c_camera_devices));

	msm_device_i2c_init();
	platform_add_devices(devices, ARRAY_SIZE(devices));
	i2c_register_board_info(0, i2c_devices, ARRAY_SIZE(i2c_devices));

//	liberty_init_panel();

	liberty_init_keypad();
// Cotulla: hack
	msm_hsusb_set_vbus_state(1);
}

static void __init photon_fixup(struct machine_desc *desc, struct tag *tags,
                               char **cmdline, struct meminfo *mi)
{
	mi->nr_banks=1;
	mi->bank[0].start = MSM_PHOTON_LINUX1_BASE;
	mi->bank[0].node = PHYS_TO_NID(mi->bank[0].start);
	mi->bank[0].size = MSM_PHOTON_LINUX1_SIZE;

	mi->nr_banks++;
	mi->bank[1].start = MSM_PHOTON_LINUX2_BASE;
	mi->bank[1].node = PHYS_TO_NID(mi->bank[1].start);
	mi->bank[1].size = MSM_PHOTON_LINUX2_SIZE;

printk(KERN_INFO "fixup: nr_banks = %d\n", mi->nr_banks);
printk(KERN_INFO "fixup: bank0 start=%08lx, node=%08x, size=%08lx\n", mi->bank[0].start, mi->bank[0].node, mi->bank[0].size);
printk(KERN_INFO "fixup: bank1 start=%08lx, node=%08x, size=%08lx\n", mi->bank[1].start, mi->bank[1].node, mi->bank[1].size);
}

int __init htc_fb_console_init(void);
int __init ram_console_early_init(void);

static void __init photon_map_io(void)
{
	printk("liberty_init_map_io()\n");
	msm_map_common_io();
//	*(volatile unsigned int*)(MSM_GPIO1_BASE + 0x808) |= 0x1000000;
	msm_clock_init();
#ifdef CONFIG_CACHE_L2X0
	/* 7x27 has 256KB L2 cache:
	* 64Kb/Way and 4-Way Associativity;
	* R/W latency: 3 cycles;
	* evmon/parity/share disabled.
	*/
	l2x0_init(MSM_L2CC_BASE, 0x00068012, 0xfe000000);
#endif
	io_already_inited = 1;
//	ram_console_early_init();
//	htc_fb_console_init();
//	*(volatile unsigned int*)(0xF8003000 + 0x808) |= 0x1000000;
//	*(volatile unsigned int*)(MSM_GPIO1_BASE + 0x808) |= 0x1000000;
}

MACHINE_START(PHOTON, "photon")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params    = 0x00200100,
	.fixup          = photon_fixup,
	.map_io         = photon_map_io,
	.init_irq       = photon_init_irq,
	.init_machine   = photon_init,
	.timer          = &msm_timer,
MACHINE_END
