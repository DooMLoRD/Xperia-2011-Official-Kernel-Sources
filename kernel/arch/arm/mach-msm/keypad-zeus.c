#include <linux/platform_device.h>
#include <linux/gpio_event.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include "keypad-zeus.h"

static struct gpio_event_input_info keypad_gpio_info = {
	.info.func = gpio_event_input_func,
	.flags = 0,
	.type = EV_KEY,
	.keymap = keypad_zeus_gpio_map,
	.keymap_size = ARRAY_SIZE(keypad_zeus_gpio_map),
	.debounce_time.tv.nsec = 10 * NSEC_PER_MSEC,
};

static struct gpio_event_input_info switch_gpio_info = {
	.info.func = gpio_event_input_func,
	.info.no_suspend = 1,
	.flags = 0,
	.type = EV_SW,
	.keymap = switch_zeus_gpio_map,
	.keymap_size = ARRAY_SIZE(switch_zeus_gpio_map),
};

static struct gpio_event_input_info lidswitch_gpio_info = {
	.info.func = gpio_event_input_func,
	.info.no_suspend = 1,
	.flags = GPIOEDF_ACTIVE_HIGH,
	.type = EV_MSC,
	.keymap = lidswitch_zeus_gpio_map,
	.keymap_size = ARRAY_SIZE(lidswitch_zeus_gpio_map),
};
#if defined(CONFIG_PHF_TESTER)

static struct gpio_event_input_info phf_gpio_info = {
	.info.func = gpio_event_input_func,
	.flags = 0,
	.type = EV_KEY,
	.keymap = phf_gpio_map,
	.keymap_size = ARRAY_SIZE(phf_gpio_map),
};

#endif

static struct gpio_event_info *keypad_info[] = {
	&keypad_gpio_info.info,
	&switch_gpio_info.info,
	&lidswitch_gpio_info.info,
#if defined(CONFIG_PHF_TESTER)
	&phf_gpio_info.info,
#endif

};

static int keypad_power(const struct gpio_event_platform_data *pdata, bool on)
{
	int i;
	int gpio;
	int mgp;
	int rc;

	if (!on) {
		for (i = 0; i < ARRAY_SIZE(keypad_zeus_gpio_map); ++i) {
			gpio = keypad_zeus_gpio_map[i].gpio;

			mgp = GPIO_CFG(gpio,
				       0,
				       GPIO_CFG_INPUT,
				       GPIO_CFG_PULL_DOWN,
				       GPIO_CFG_2MA);
			rc = gpio_tlmm_config(mgp, GPIO_CFG_DISABLE);
			if (rc)
				pr_err("%s: gpio_tlmm_config (gpio=%d), failed\n",
				       __func__, keypad_zeus_gpio_map[i].gpio);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(keypad_zeus_gpio_map); ++i) {
			gpio = keypad_zeus_gpio_map[i].gpio;
			mgp = GPIO_CFG(gpio,
				       0,
				       GPIO_CFG_INPUT,
				       GPIO_CFG_PULL_UP,
				       GPIO_CFG_2MA);
			rc = gpio_tlmm_config(mgp, GPIO_CFG_ENABLE);
			if (rc)
				pr_err("%s: gpio_tlmm_config (gpio=%d), failed\n",
				       __func__, keypad_zeus_gpio_map[i].gpio);
		}
	}

	return 0;
}

static struct gpio_event_platform_data keypad_data = {
	.name		= "keypad-zeus",
	.info		= keypad_info,
	.info_count	= ARRAY_SIZE(keypad_info),
	.power          = keypad_power,
};

struct platform_device keypad_device_zeus = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &keypad_data,
	},
};

static int __init keypad_device_init(void)
{
	int i;
	int rc;
	int gpio;
	int mgp;

	keypad_power(&keypad_data,1);

	for (i = 0; i < ARRAY_SIZE(switch_zeus_gpio_map); ++i) {
		gpio = switch_zeus_gpio_map[i].gpio;
		mgp = GPIO_CFG(gpio,
			       0,
			       GPIO_CFG_INPUT,
			       GPIO_CFG_NO_PULL,
			       GPIO_CFG_2MA);
		rc = gpio_tlmm_config(mgp, GPIO_CFG_ENABLE);
		if (rc)
			pr_err("%s: gpio_tlmm_config (gpio=%d), failed\n",
					__func__, switch_zeus_gpio_map[i].gpio);
	}

#ifdef CONFIG_PHF_TESTER
	for (i = 0; i < ARRAY_SIZE(phf_gpio_map); ++i) {
		gpio = phf_gpio_map[i].gpio;
		mgp = GPIO_CFG(gpio,
			       0,
			       GPIO_CFG_INPUT,
			       GPIO_CFG_PULL_UP,
			       GPIO_CFG_2MA);
		rc = gpio_tlmm_config(mgp, GPIO_CFG_ENABLE);
		if (rc)
			pr_err("%s: gpio_tlmm_config (gpio=%d), failed\n",
					__func__, phf_gpio_map[i].gpio);
	}
#endif

	return platform_device_register(&keypad_device_zeus);
}

static void __exit keypad_device_exit(void)
{
	platform_device_unregister(&keypad_device_zeus);
}

module_init(keypad_device_init);
module_exit(keypad_device_exit);
