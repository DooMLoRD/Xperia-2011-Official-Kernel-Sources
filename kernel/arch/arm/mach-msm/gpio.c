/* linux/arch/arm/mach-msm/gpio.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <mach/gpio.h>
#include "gpio_hw.h"
#include "proc_comm.h"
#include "smd_private.h"

enum {
	GPIO_DEBUG_SLEEP = 1U << 0,
};
static int msm_gpio_debug_mask;
module_param_named(debug_mask, msm_gpio_debug_mask, int,
		   S_IRUGO | S_IWUSR | S_IWGRP);

#define MSM_GPIO_BROKEN_INT_CLEAR 1

struct msm_gpio_regs {
	void __iomem *out;
	void __iomem *in;
	void __iomem *int_status;
	void __iomem *int_clear;
	void __iomem *int_en;
	void __iomem *int_edge;
	void __iomem *int_pos;
	void __iomem *oe;
};

struct msm_gpio_chip {
	spinlock_t		lock;
	struct gpio_chip	chip;
	struct msm_gpio_regs	regs;
#if MSM_GPIO_BROKEN_INT_CLEAR
	unsigned                int_status_copy;
#endif
	unsigned int            both_edge_detect;
	unsigned int            int_enable[2]; /* 0: awake, 1: sleep */
};

static int msm_gpio_write(struct msm_gpio_chip *msm_chip,
			  unsigned offset, unsigned on)
{
	unsigned b = 1U << offset;
	unsigned v;

	v = readl(msm_chip->regs.out);
	if (on)
		writel(v | b, msm_chip->regs.out);
	else
		writel(v & (~b), msm_chip->regs.out);
	return 0;
}

static void msm_gpio_update_both_edge_detect(struct msm_gpio_chip *msm_chip)
{
	int loop_limit = 100;
	unsigned pol, val, val2, intstat;
	do {
		val = readl(msm_chip->regs.in);
		pol = readl(msm_chip->regs.int_pos);
		pol = (pol & ~msm_chip->both_edge_detect) |
		      (~val & msm_chip->both_edge_detect);
		writel(pol, msm_chip->regs.int_pos);
		intstat = readl(msm_chip->regs.int_status);
		val2 = readl(msm_chip->regs.in);
		if (((val ^ val2) & msm_chip->both_edge_detect & ~intstat) == 0)
			return;
	} while (loop_limit-- > 0);
	printk(KERN_ERR "msm_gpio_update_both_edge_detect, "
	       "failed to reach stable state %x != %x\n", val, val2);
}

static int msm_gpio_clear_detect_status(struct msm_gpio_chip *msm_chip,
					unsigned offset)
{
	unsigned b = BIT(offset);

#if MSM_GPIO_BROKEN_INT_CLEAR
	/* Save interrupts that already triggered before we loose them. */
	/* Any interrupt that triggers between the read of int_status */
	/* and the write to int_clear will still be lost though. */
	msm_chip->int_status_copy |= readl(msm_chip->regs.int_status);
	msm_chip->int_status_copy &= ~b;
#endif
	writel(b, msm_chip->regs.int_clear);
	msm_gpio_update_both_edge_detect(msm_chip);
	return 0;
}

static int msm_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct msm_gpio_chip *msm_chip;
	unsigned long irq_flags;

	msm_chip = container_of(chip, struct msm_gpio_chip, chip);
	spin_lock_irqsave(&msm_chip->lock, irq_flags);
	writel(readl(msm_chip->regs.oe) & ~BIT(offset), msm_chip->regs.oe);
	spin_unlock_irqrestore(&msm_chip->lock, irq_flags);
	return 0;
}

static int
msm_gpio_direction_output(struct gpio_chip *chip, unsigned offset, int value)
{
	struct msm_gpio_chip *msm_chip;
	unsigned long irq_flags;

	msm_chip = container_of(chip, struct msm_gpio_chip, chip);
	spin_lock_irqsave(&msm_chip->lock, irq_flags);
	msm_gpio_write(msm_chip, offset, value);
	writel(readl(msm_chip->regs.oe) | BIT(offset), msm_chip->regs.oe);
	spin_unlock_irqrestore(&msm_chip->lock, irq_flags);
	return 0;
}

static int msm_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct msm_gpio_chip *msm_chip;

	msm_chip = container_of(chip, struct msm_gpio_chip, chip);
	return (readl(msm_chip->regs.in) & (1U << offset)) ? 1 : 0;
}

static void msm_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct msm_gpio_chip *msm_chip;
	unsigned long irq_flags;

	msm_chip = container_of(chip, struct msm_gpio_chip, chip);
	spin_lock_irqsave(&msm_chip->lock, irq_flags);
	msm_gpio_write(msm_chip, offset, value);
	spin_unlock_irqrestore(&msm_chip->lock, irq_flags);
}

static int msm_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	return MSM_GPIO_TO_INT(chip->base + offset);
}

struct msm_gpio_chip msm_gpio_chips[] = {
	{
		.regs = {
			.out =         GPIO_OUT_0,
			.in =          GPIO_IN_0,
			.int_status =  GPIO_INT_STATUS_0,
			.int_clear =   GPIO_INT_CLEAR_0,
			.int_en =      GPIO_INT_EN_0,
			.int_edge =    GPIO_INT_EDGE_0,
			.int_pos =     GPIO_INT_POS_0,
			.oe =          GPIO_OE_0,
		},
		.chip = {
			.base = 0,
			.ngpio = 16,
			.get = msm_gpio_get,
			.set = msm_gpio_set,
			.direction_input = msm_gpio_direction_input,
			.direction_output = msm_gpio_direction_output,
			.to_irq = msm_gpio_to_irq,
		}
	},
	{
		.regs = {
			.out =         GPIO_OUT_1,
			.in =          GPIO_IN_1,
			.int_status =  GPIO_INT_STATUS_1,
			.int_clear =   GPIO_INT_CLEAR_1,
			.int_en =      GPIO_INT_EN_1,
			.int_edge =    GPIO_INT_EDGE_1,
			.int_pos =     GPIO_INT_POS_1,
			.oe =          GPIO_OE_1,
		},
		.chip = {
			.base = 16,
#if defined(CONFIG_ARCH_MSM7X30)
			.ngpio = 28,
#else
			.ngpio = 27,
#endif
			.get = msm_gpio_get,
			.set = msm_gpio_set,
			.direction_input = msm_gpio_direction_input,
			.direction_output = msm_gpio_direction_output,
			.to_irq = msm_gpio_to_irq,
		}
	},
	{
		.regs = {
			.out =         GPIO_OUT_2,
			.in =          GPIO_IN_2,
			.int_status =  GPIO_INT_STATUS_2,
			.int_clear =   GPIO_INT_CLEAR_2,
			.int_en =      GPIO_INT_EN_2,
			.int_edge =    GPIO_INT_EDGE_2,
			.int_pos =     GPIO_INT_POS_2,
			.oe =          GPIO_OE_2,
		},
		.chip = {
#if defined(CONFIG_ARCH_MSM7X30)
			.base = 44,
			.ngpio = 24,
#else
			.base = 43,
			.ngpio = 25,
#endif
			.get = msm_gpio_get,
			.set = msm_gpio_set,
			.direction_input = msm_gpio_direction_input,
			.direction_output = msm_gpio_direction_output,
			.to_irq = msm_gpio_to_irq,
		}
	},
	{
		.regs = {
			.out =         GPIO_OUT_3,
			.in =          GPIO_IN_3,
			.int_status =  GPIO_INT_STATUS_3,
			.int_clear =   GPIO_INT_CLEAR_3,
			.int_en =      GPIO_INT_EN_3,
			.int_edge =    GPIO_INT_EDGE_3,
			.int_pos =     GPIO_INT_POS_3,
			.oe =          GPIO_OE_3,
		},
		.chip = {
			.base = 68,
			.ngpio = 27,
			.get = msm_gpio_get,
			.set = msm_gpio_set,
			.direction_input = msm_gpio_direction_input,
			.direction_output = msm_gpio_direction_output,
			.to_irq = msm_gpio_to_irq,
		}
	},
	{
		.regs = {
			.out =         GPIO_OUT_4,
			.in =          GPIO_IN_4,
			.int_status =  GPIO_INT_STATUS_4,
			.int_clear =   GPIO_INT_CLEAR_4,
			.int_en =      GPIO_INT_EN_4,
			.int_edge =    GPIO_INT_EDGE_4,
			.int_pos =     GPIO_INT_POS_4,
			.oe =          GPIO_OE_4,
		},
		.chip = {
			.base = 95,
#if defined(CONFIG_ARCH_QSD8X50)
			.ngpio = 9,
#else
			.ngpio = 12,
#endif
			.get = msm_gpio_get,
			.set = msm_gpio_set,
			.direction_input = msm_gpio_direction_input,
			.direction_output = msm_gpio_direction_output,
			.to_irq = msm_gpio_to_irq,
		}
	},
	{
		.regs = {
			.out =         GPIO_OUT_5,
			.in =          GPIO_IN_5,
			.int_status =  GPIO_INT_STATUS_5,
			.int_clear =   GPIO_INT_CLEAR_5,
			.int_en =      GPIO_INT_EN_5,
			.int_edge =    GPIO_INT_EDGE_5,
			.int_pos =     GPIO_INT_POS_5,
			.oe =          GPIO_OE_5,
		},
		.chip = {
#if defined(CONFIG_ARCH_QSD8X50)
			.base = 104,
			.ngpio = 18,
#elif defined(CONFIG_ARCH_MSM7X30)
			.base = 107,
			.ngpio = 27,
#elif defined(CONFIG_ARCH_MSM7X27) || defined(CONFIG_ARCH_MSM7X25)
			.base = 107,
			.ngpio = 26,
#else
			.base = 107,
			.ngpio = 15,
#endif
			.get = msm_gpio_get,
			.set = msm_gpio_set,
			.direction_input = msm_gpio_direction_input,
			.direction_output = msm_gpio_direction_output,
			.to_irq = msm_gpio_to_irq,
		}
	},
#if defined(CONFIG_ARCH_QSD8X50) || defined(CONFIG_ARCH_MSM7X30)
	{
		.regs = {
			.out =         GPIO_OUT_6,
			.in =          GPIO_IN_6,
			.int_status =  GPIO_INT_STATUS_6,
			.int_clear =   GPIO_INT_CLEAR_6,
			.int_en =      GPIO_INT_EN_6,
			.int_edge =    GPIO_INT_EDGE_6,
			.int_pos =     GPIO_INT_POS_6,
			.oe =          GPIO_OE_6,
		},
		.chip = {
#if defined(CONFIG_ARCH_MSM7X30)
			.base = 134,
			.ngpio = 17,
#else
			.base = 122,
			.ngpio = 31,
#endif
			.get = msm_gpio_get,
			.set = msm_gpio_set,
			.direction_input = msm_gpio_direction_input,
			.direction_output = msm_gpio_direction_output,
			.to_irq = msm_gpio_to_irq,
		}
	},
	{
		.regs = {
			.out =         GPIO_OUT_7,
			.in =          GPIO_IN_7,
			.int_status =  GPIO_INT_STATUS_7,
			.int_clear =   GPIO_INT_CLEAR_7,
			.int_en =      GPIO_INT_EN_7,
			.int_edge =    GPIO_INT_EDGE_7,
			.int_pos =     GPIO_INT_POS_7,
			.oe =          GPIO_OE_7,
		},
		.chip = {
#if defined(CONFIG_ARCH_MSM7X30)
			.base = 151,
			.ngpio = 31,
#else
			.base = 153,
			.ngpio = 12,
#endif
			.get = msm_gpio_get,
			.set = msm_gpio_set,
			.direction_input = msm_gpio_direction_input,
			.direction_output = msm_gpio_direction_output,
			.to_irq = msm_gpio_to_irq,
		}
	},
#endif
};

static void msm_gpio_irq_ack(unsigned int irq)
{
	unsigned long irq_flags;
	struct msm_gpio_chip *msm_chip = get_irq_chip_data(irq);
	spin_lock_irqsave(&msm_chip->lock, irq_flags);
	msm_gpio_clear_detect_status(msm_chip,
				     irq - gpio_to_irq(msm_chip->chip.base));
	spin_unlock_irqrestore(&msm_chip->lock, irq_flags);
}

static void msm_gpio_irq_mask(unsigned int irq)
{
	unsigned long irq_flags;
	struct msm_gpio_chip *msm_chip = get_irq_chip_data(irq);
	unsigned offset = irq - gpio_to_irq(msm_chip->chip.base);

	spin_lock_irqsave(&msm_chip->lock, irq_flags);
	/* level triggered interrupts are also latched */
	if (!(readl(msm_chip->regs.int_edge) & BIT(offset)))
		msm_gpio_clear_detect_status(msm_chip, offset);
	msm_chip->int_enable[0] &= ~BIT(offset);
	writel(msm_chip->int_enable[0], msm_chip->regs.int_en);
	spin_unlock_irqrestore(&msm_chip->lock, irq_flags);
}

static void msm_gpio_irq_unmask(unsigned int irq)
{
	unsigned long irq_flags;
	struct msm_gpio_chip *msm_chip = get_irq_chip_data(irq);
	unsigned offset = irq - gpio_to_irq(msm_chip->chip.base);

	spin_lock_irqsave(&msm_chip->lock, irq_flags);
	/* level triggered interrupts are also latched */
	if (!(readl(msm_chip->regs.int_edge) & BIT(offset)))
		msm_gpio_clear_detect_status(msm_chip, offset);
	msm_chip->int_enable[0] |= BIT(offset);
	writel(msm_chip->int_enable[0], msm_chip->regs.int_en);
	spin_unlock_irqrestore(&msm_chip->lock, irq_flags);
}

static int msm_gpio_irq_set_wake(unsigned int irq, unsigned int on)
{
	unsigned long irq_flags;
	struct msm_gpio_chip *msm_chip = get_irq_chip_data(irq);
	unsigned offset = irq - gpio_to_irq(msm_chip->chip.base);

	spin_lock_irqsave(&msm_chip->lock, irq_flags);

	if (on)
		msm_chip->int_enable[1] |= BIT(offset);
	else
		msm_chip->int_enable[1] &= ~BIT(offset);

	spin_unlock_irqrestore(&msm_chip->lock, irq_flags);
	return 0;
}

static int msm_gpio_irq_set_type(unsigned int irq, unsigned int flow_type)
{
	unsigned long irq_flags;
	struct msm_gpio_chip *msm_chip = get_irq_chip_data(irq);
	unsigned offset = irq - gpio_to_irq(msm_chip->chip.base);
	unsigned v, b = BIT(offset);

	spin_lock_irqsave(&msm_chip->lock, irq_flags);
	v = readl(msm_chip->regs.int_edge);
	if (flow_type & IRQ_TYPE_EDGE_BOTH) {
		writel(v | b, msm_chip->regs.int_edge);
		irq_desc[irq].handle_irq = handle_edge_irq;
	} else {
		writel(v & ~b, msm_chip->regs.int_edge);
		irq_desc[irq].handle_irq = handle_level_irq;
	}
	if ((flow_type & IRQ_TYPE_EDGE_BOTH) == IRQ_TYPE_EDGE_BOTH) {
		msm_chip->both_edge_detect |= b;
		msm_gpio_update_both_edge_detect(msm_chip);
	} else {
		msm_chip->both_edge_detect &= ~b;
		v = readl(msm_chip->regs.int_pos);
		if (flow_type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_HIGH))
			writel(v | b, msm_chip->regs.int_pos);
		else
			writel(v & ~b, msm_chip->regs.int_pos);
	}
	spin_unlock_irqrestore(&msm_chip->lock, irq_flags);
	return 0;
}

static void msm_gpio_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	int i, j, m;
	unsigned v;

	for (i = 0; i < ARRAY_SIZE(msm_gpio_chips); i++) {
		struct msm_gpio_chip *msm_chip = &msm_gpio_chips[i];
		v = readl(msm_chip->regs.int_status);
		v &= msm_chip->int_enable[0];
		while (v) {
			m = v & -v;
			j = fls(m) - 1;
			/* printk("%s %08x %08x bit %d gpio %d irq %d\n",
				__func__, v, m, j, msm_chip->chip.start + j,
				FIRST_GPIO_IRQ + msm_chip->chip.start + j); */
			v &= ~m;
			generic_handle_irq(FIRST_GPIO_IRQ +
					   msm_chip->chip.base + j);
		}
	}
	desc->chip->ack(irq);
}

static struct irq_chip msm_gpio_irq_chip = {
	.name      = "msmgpio",
	.ack       = msm_gpio_irq_ack,
	.mask      = msm_gpio_irq_mask,
	.unmask    = msm_gpio_irq_unmask,
	.set_wake  = msm_gpio_irq_set_wake,
	.set_type  = msm_gpio_irq_set_type,
};

#define NUM_GPIO_SMEM_BANKS 6
#define GPIO_SMEM_NUM_GROUPS 2
#define GPIO_SMEM_MAX_PC_INTERRUPTS 8
struct tramp_gpio_smem {
	uint16_t num_fired[GPIO_SMEM_NUM_GROUPS];
	uint16_t fired[GPIO_SMEM_NUM_GROUPS][GPIO_SMEM_MAX_PC_INTERRUPTS];
	uint32_t enabled[NUM_GPIO_SMEM_BANKS];
	uint32_t detection[NUM_GPIO_SMEM_BANKS];
	uint32_t polarity[NUM_GPIO_SMEM_BANKS];
};

static void msm_gpio_sleep_int(unsigned long arg)
{
	int i, j;
	struct tramp_gpio_smem *smem_gpio;

	BUILD_BUG_ON(NR_GPIO_IRQS > NUM_GPIO_SMEM_BANKS * 32);

	smem_gpio = smem_alloc(SMEM_GPIO_INT, sizeof(*smem_gpio));
	if (smem_gpio == NULL)
		return;

	local_irq_disable();
	for (i = 0; i < GPIO_SMEM_NUM_GROUPS; i++) {
		int count = smem_gpio->num_fired[i];
		for (j = 0; j < count; j++) {
			/* TODO: Check mask */
			generic_handle_irq(
				MSM_GPIO_TO_INT(smem_gpio->fired[i][j]));
		}
	}
	local_irq_enable();
}

static DECLARE_TASKLET(msm_gpio_sleep_int_tasklet, msm_gpio_sleep_int, 0);

void msm_gpio_enter_sleep(int from_idle)
{
	int i;
	struct tramp_gpio_smem *smem_gpio;

	smem_gpio = smem_alloc(SMEM_GPIO_INT, sizeof(*smem_gpio));

	if (smem_gpio) {
		for (i = 0; i < ARRAY_SIZE(smem_gpio->enabled); i++) {
			smem_gpio->enabled[i] = 0;
			smem_gpio->detection[i] = 0;
			smem_gpio->polarity[i] = 0;
		}
	}

	for (i = 0; i < ARRAY_SIZE(msm_gpio_chips); i++) {
		writel(msm_gpio_chips[i].int_enable[!from_idle],
		       msm_gpio_chips[i].regs.int_en);
		if (smem_gpio) {
			uint32_t tmp;
			int start, index, shiftl, shiftr;
			start = msm_gpio_chips[i].chip.base;
			index = start / 32;
			shiftl = start % 32;
			shiftr = 32 - shiftl;
			tmp = msm_gpio_chips[i].int_enable[!from_idle];
			smem_gpio->enabled[index] |= tmp << shiftl;
			smem_gpio->enabled[index+1] |= tmp >> shiftr;
			smem_gpio->detection[index] |=
				readl(msm_gpio_chips[i].regs.int_edge) <<
				shiftl;
			smem_gpio->detection[index+1] |=
				readl(msm_gpio_chips[i].regs.int_edge) >>
				shiftr;
			smem_gpio->polarity[index] |=
				readl(msm_gpio_chips[i].regs.int_pos) << shiftl;
			smem_gpio->polarity[index+1] |=
				readl(msm_gpio_chips[i].regs.int_pos) >> shiftr;
		}
	}

	if (smem_gpio) {
		if (msm_gpio_debug_mask & GPIO_DEBUG_SLEEP)
			for (i = 0; i < ARRAY_SIZE(smem_gpio->enabled); i++) {
				printk("msm_gpio_enter_sleep gpio %d-%d: enable"
				       " %08x, edge %08x, polarity %08x\n",
				       i * 32, i * 32 + 31,
				       smem_gpio->enabled[i],
				       smem_gpio->detection[i],
				       smem_gpio->polarity[i]);
			}
		for (i = 0; i < GPIO_SMEM_NUM_GROUPS; i++)
			smem_gpio->num_fired[i] = 0;
	}
}

void msm_gpio_exit_sleep(void)
{
	int i;
	struct tramp_gpio_smem *smem_gpio;

	smem_gpio = smem_alloc(SMEM_GPIO_INT, sizeof(*smem_gpio));

	for (i = 0; i < ARRAY_SIZE(msm_gpio_chips); i++) {
		writel(msm_gpio_chips[i].int_enable[0],
		       msm_gpio_chips[i].regs.int_en);
	}

	if (smem_gpio && (smem_gpio->num_fired[0] || smem_gpio->num_fired[1])) {
		if (msm_gpio_debug_mask & GPIO_DEBUG_SLEEP)
			printk(KERN_INFO "gpio: fired %x %x\n",
			      smem_gpio->num_fired[0], smem_gpio->num_fired[1]);
		tasklet_schedule(&msm_gpio_sleep_int_tasklet);
	}
}

static int __init msm_init_gpio(void)
{
	int i, j = 0;

	for (i = FIRST_GPIO_IRQ; i < FIRST_GPIO_IRQ + NR_GPIO_IRQS; i++) {
		if (i - FIRST_GPIO_IRQ >=
			msm_gpio_chips[j].chip.base +
			msm_gpio_chips[j].chip.ngpio)
			j++;
		set_irq_chip_data(i, &msm_gpio_chips[j]);
		set_irq_chip(i, &msm_gpio_irq_chip);
		set_irq_handler(i, handle_edge_irq);
		set_irq_flags(i, IRQF_VALID);
	}

	for (i = 0; i < ARRAY_SIZE(msm_gpio_chips); i++) {
		spin_lock_init(&msm_gpio_chips[i].lock);
		writel(0, msm_gpio_chips[i].regs.int_en);
		gpiochip_add(&msm_gpio_chips[i].chip);
	}

	set_irq_chained_handler(INT_GPIO_GROUP1, msm_gpio_irq_handler);
	set_irq_chained_handler(INT_GPIO_GROUP2, msm_gpio_irq_handler);
	set_irq_wake(INT_GPIO_GROUP1, 1);
	set_irq_wake(INT_GPIO_GROUP2, 2);
	return 0;
}

postcore_initcall(msm_init_gpio);

int gpio_tlmm_config(unsigned config, unsigned disable)
{
	return msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, &disable);
}
EXPORT_SYMBOL(gpio_tlmm_config);

int msm_gpios_request_enable(const struct msm_gpio *table, int size)
{
	int rc = msm_gpios_request(table, size);
	if (rc)
		return rc;
	rc = msm_gpios_enable(table, size);
	if (rc)
		msm_gpios_free(table, size);
	return rc;
}
EXPORT_SYMBOL(msm_gpios_request_enable);

void msm_gpios_disable_free(const struct msm_gpio *table, int size)
{
	msm_gpios_disable(table, size);
	msm_gpios_free(table, size);
}
EXPORT_SYMBOL(msm_gpios_disable_free);

int msm_gpios_request(const struct msm_gpio *table, int size)
{
	int rc;
	int i;
	const struct msm_gpio *g;
	for (i = 0; i < size; i++) {
		g = table + i;
		rc = gpio_request(GPIO_PIN(g->gpio_cfg), g->label);
		if (rc) {
			pr_err("gpio_request(%d) <%s> failed: %d\n",
			       GPIO_PIN(g->gpio_cfg), g->label ?: "?", rc);
			goto err;
		}
	}
	return 0;
err:
	msm_gpios_free(table, i);
	return rc;
}
EXPORT_SYMBOL(msm_gpios_request);

void msm_gpios_free(const struct msm_gpio *table, int size)
{
	int i;
	const struct msm_gpio *g;
	for (i = size-1; i >= 0; i--) {
		g = table + i;
		gpio_free(GPIO_PIN(g->gpio_cfg));
	}
}
EXPORT_SYMBOL(msm_gpios_free);

int msm_gpios_enable(const struct msm_gpio *table, int size)
{
	int rc;
	int i;
	const struct msm_gpio *g;
	for (i = 0; i < size; i++) {
		g = table + i;
		rc = gpio_tlmm_config(g->gpio_cfg, GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("gpio_tlmm_config(0x%08x, GPIO_CFG_ENABLE)"
			       " <%s> failed: %d\n",
			       g->gpio_cfg, g->label ?: "?", rc);
			pr_err("pin %d func %d dir %d pull %d drvstr %d\n",
			       GPIO_PIN(g->gpio_cfg), GPIO_FUNC(g->gpio_cfg),
			       GPIO_DIR(g->gpio_cfg), GPIO_PULL(g->gpio_cfg),
			       GPIO_DRVSTR(g->gpio_cfg));
			goto err;
		}
	}
	return 0;
err:
	msm_gpios_disable(table, i);
	return rc;
}
EXPORT_SYMBOL(msm_gpios_enable);

int msm_gpios_disable(const struct msm_gpio *table, int size)
{
	int rc = 0;
	int i;
	const struct msm_gpio *g;
	for (i = size-1; i >= 0; i--) {
		int tmp;
		g = table + i;
		tmp = gpio_tlmm_config(g->gpio_cfg, GPIO_CFG_DISABLE);
		if (tmp) {
			pr_err("gpio_tlmm_config(0x%08x, GPIO_CFG_DISABLE)"
			       " <%s> failed: %d\n",
			       g->gpio_cfg, g->label ?: "?", rc);
			pr_err("pin %d func %d dir %d pull %d drvstr %d\n",
			       GPIO_PIN(g->gpio_cfg), GPIO_FUNC(g->gpio_cfg),
			       GPIO_DIR(g->gpio_cfg), GPIO_PULL(g->gpio_cfg),
			       GPIO_DRVSTR(g->gpio_cfg));
			if (!rc)
				rc = tmp;
		}
	}

	return rc;
}
EXPORT_SYMBOL(msm_gpios_disable);
