#ifndef _SWA_GPIO_API_H_
#define _SWA_GPIO_API_H_

#define DLNA_GPIO_NAME  "/dev/dlna_gpio"

#define GPIO_MAGIC 'd'

#define GPIO_IOCTL_READ      _IO(GPIO_MAGIC, 1)
#define GPIO_IOCTL_WRITE     _IO(GPIO_MAGIC, 2)
#define GPIO_IOCTL_SET_OUT   _IO(GPIO_MAGIC, 3)
#define GPIO_IOCTL_SET_IN    _IO(GPIO_MAGIC, 4)
#define GPIO_IOCTL_SET_PULL  _IO(GPIO_MAGIC, 5)

#define GPIO_BTN1 GPIO1_12
#define GPIO_BTN2 GPIO1_13

#define GPIO_MUTE GPIO2_17

#define GPIO_LED1 GPIO2_18
#define GPIO_LED2 GPIO2_19
#define GPIO_LED3 GPIO2_20

#define GPIO_AMP_POWER GPIO_LED1
#define GPIO_AMP_MUTE  GPIO_LED2

#define GPIO_SPDIF_IN0 GPIO1_9

typedef enum _PROT{
	GPIO1_0,  GPIO1_1,  GPIO1_2,  GPIO1_3,  GPIO1_4,  GPIO1_5,  GPIO1_6,  GPIO1_7,
	GPIO1_8,  GPIO1_9,  GPIO1_10, GPIO1_11, GPIO1_12, GPIO1_13, GPIO1_14, GPIO1_15,
	GPIO1_16, GPIO1_17, GPIO1_18, GPIO1_19, GPIO1_20, GPIO1_21, GPIO1_22, GPIO1_23,
	GPIO1_24, GPIO1_25, GPIO1_26, GPIO1_27, GPIO1_28, GPIO1_29,
	GPIO2_0=32,  GPIO2_1,  GPIO2_2,  GPIO2_3,  GPIO2_4,  GPIO2_5,  GPIO2_6,  GPIO2_7,
	GPIO2_8,  GPIO2_9,  GPIO2_10, GPIO2_11, GPIO2_12, GPIO2_13, GPIO2_14, GPIO2_15,
	GPIO2_16, GPIO2_17, GPIO2_18, GPIO2_19, GPIO2_20,
}PORT;

enum pull_mode
{
    PULLING_HIGH,
    PULLING_NONE
};

struct gpio_data_t
{
    int num;
    int value;
};

#endif

