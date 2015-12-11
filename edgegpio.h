#ifndef ANDGPIO_IOCTL_H

#define ANDGPIO_IOCTL_H

#include <linux/ioctl.h>

typedef struct
{
	int type, value;
} andgpio_arg_t;

#define EDGE_MAGIC 'l'

#define EDGE_SETPORT_GPIO_OUTPUT _IOW(EDGE_MAGIC, 1, andgpio_arg_t *)
#define EDGE_SETPORT_GPIO_INPUT _IOW(EDGE_MAGIC, 2, andgpio_arg_t *)
#define EDGE_SETPORT_ADC _IOW(EDGE_MAGIC, 3, andgpio_arg_t *)
#define EDGE_SETPORT_PWM _IOW(EDGE_MAGIC, 4, andgpio_arg_t *)

#define GPIO_OUTPUT_PORT	1
#define GPIO_INPUT_PORT		2
#define GPIO_ADC_PORT		3
#define GPIO_PWM_PORT		4

#endif
