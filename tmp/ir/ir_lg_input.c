#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/input.h>
#include <linux/init.h>

//#define DEBUG_IR_LG_BUF
//#define DEBUG_IR_LG_CODE

#define IR_LG_DEV_NAME     "sl-ir-lg"

#define CLK_ERR            100
#define CLK_HEADER_PULSE   9000
#define CLK_HEADER_SPACE   4500
#define CLK_REPEAT_SPACE   2300
#define CLK_DATA_PULSE     510
#define CLK_DATA_1_SPACE   1750
#define CLK_DATA_0_SPACE   600
#define CLK_GAP            96400

#define STATE_HEADER_PULSE 1
#define STATE_HEADER_SPACE 2
#define STATE_REPEAT_PULSE 3
#define STATE_REPEAT_SPACE 4
#define STATE_DATA_PULSE   5
#define STATE_DATA_SPACE   6
#define STATE_GAP          7

#define DATA_BITS          16
#define DATA_PRE_BITS      16
#define DATA_PRE           0x20DF

#define SKIP_REPEAT        2

static const struct {
   int ircode;
   int keycode;
} ir_key_table[] = {
    { 0x10EF, KEY_POWER },
    { 0x0FF0, KEY_RADIO },

    { 0xD02F, KEY_AUX },
    { 0xC23D, KEY_SETUP },
    { 0x9E61, KEY_SCALE },
    { 0x9C63, KEY_SUBTITLE },

    { 0x8877, KEY_NUMERIC_1 },
    { 0x48B7, KEY_NUMERIC_2 },
    { 0xC837, KEY_NUMERIC_3 },

    { 0x28D7, KEY_NUMERIC_4 },
    { 0xA857, KEY_NUMERIC_5 },
    { 0x6897, KEY_NUMERIC_6 },

    { 0xE817, KEY_NUMERIC_7 },
    { 0x18E7, KEY_NUMERIC_8 },
    { 0x9867, KEY_NUMERIC_9 },

    { 0xD52A, KEY_PROGRAM },
    { 0x08F7, KEY_NUMERIC_0 },
    { 0x58A7, KEY_PREVIOUS },

    { 0x40BF, KEY_VOLUMEUP },
    { 0x7887, KEY_FAVORITES },
    { 0x00FF, KEY_CHANNELUP },

    { 0x55AA, KEY_INFO },

    { 0xC03F, KEY_VOLUMEDOWN },
    { 0x906F, KEY_MUTE },
    { 0x807F, KEY_CHANNELDOWN },

    { 0xF906, KEY_PROG1 },
    { 0x3EC1, KEY_HOME },
    { 0x42BD, KEY_DASHBOARD },

    { 0x04FB, KEY_TEXT },
    { 0x02FD, KEY_UP },
    { 0x847B, KEY_OPTION },

    { 0xE01F, KEY_LEFT },
    { 0x22DD, KEY_OK },
    { 0x609F, KEY_RIGHT },

    { 0x14EB, KEY_BACK },
    { 0x827D, KEY_DOWN },
    { 0xDA25, KEY_EXIT },

    { 0x8976, KEY_ADDRESSBOOK },
    { 0xBD42, KEY_RECORD },
    { 0x8D72, KEY_STOP },

    { 0xF10E, KEY_REWIND },
    { 0x0DF2, KEY_PLAY },
    { 0x718E, KEY_FORWARD },
    { 0x5DA2, KEY_PAUSE },

    { 0x4EB1, KEY_RED },
    { 0x8E71, KEY_GREEN },
    { 0xC639, KEY_YELLOW },
    { 0x8679, KEY_BLUE },
};

#define CODENUM (sizeof(ir_key_table) / (sizeof(int) * 2))

struct ir_private {
    int idx;
    int state;
    int data;
    int data_ok;
    int repeat;
    struct timeval base_time;
};

#ifdef DEBUG_IR_LG_BUF
static int buf[255];
#endif

struct input_dev *ir_dev;
struct ir_private ir_priv;
struct delayed_work ir_work;
static int ir_irq = 12;
static int gpiobase = 96;

static void ir_produce(struct work_struct *work);
static irqreturn_t ir_isr(int irq, void *dev_id);
static int __init ir_init(void);
static void __exit ir_exit(void);

static void ir_produce(struct work_struct *work)
{
    unsigned int i, key = ir_priv.data_ok;

#ifdef DEBUG_IR_LG_BUF
    disable_irq(gpiobase+ir_irq);
    for(i = 0; i < 255; i++)
        printk("%d = %d\n", i, buf[i]);
    enable_irq(gpiobase+ir_irq);
    return;
#endif

#ifdef DEBUG_IR_LG_CODE
    printk(IR_LG_DEV_NAME ": key: 0x%08X\n", key);
#endif

    if((key >> DATA_BITS) == DATA_PRE){
        key&= ~(-1 << DATA_BITS);
        for(i = 0; i < CODENUM; i++){
            if(key == ir_key_table[i].ircode){

#ifdef DEBUG_IR_LG_CODE
                printk(IR_LG_DEV_NAME ": key: 0x%08X, code: %d\n", key, ir_key_table[i].keycode);
#endif

                input_report_key(ir_dev, ir_key_table[i].keycode, 1);
                input_report_key(ir_dev, ir_key_table[i].keycode, 0);
                input_sync(ir_dev);
                break;
            }
        }
    }

    return ;
}

static irqreturn_t ir_isr(int irq, void *dev_id)
{
    struct ir_private *priv = (struct ir_private *) dev_id;
    struct timeval tv;
    unsigned int width;

    do_gettimeofday(&tv);
    width = 1000000 * (tv.tv_sec - priv->base_time.tv_sec)
            + tv.tv_usec - priv->base_time.tv_usec;

    //store current time
    priv->base_time.tv_sec = tv.tv_sec;
    priv->base_time.tv_usec = tv.tv_usec;

#ifdef DEBUG_IR_LG_BUF
    if(priv->idx < 0 || priv->idx > 255){
        priv->idx = 0;
        schedule_delayed_work(&ir_work, 0);
    }
    buf[priv->idx++] = gpio_get_value(ir_irq) ? -width : width;
    return IRQ_HANDLED;
#endif

	if(width > CLK_GAP+CLK_ERR){
		priv->state = STATE_HEADER_PULSE;
		return IRQ_HANDLED;
	}

	switch(priv->state){
	case STATE_HEADER_PULSE:
		if(width >= CLK_HEADER_PULSE-CLK_ERR && width <= CLK_HEADER_PULSE+CLK_ERR)
			priv->state = STATE_HEADER_SPACE;
		else{
			priv->state = 0;
#ifdef DEBUG_IR_LG_CODE
			printk(IR_LG_DEV_NAME ": [STATE_HEADER_PULSE] w: %d, i: %d\n", width, priv->idx);
#endif
		}
		break;

	case STATE_HEADER_SPACE:
		if(width >= CLK_HEADER_SPACE-CLK_ERR && width <= CLK_HEADER_SPACE+CLK_ERR){
			priv->idx = 0;
			priv->data = 0;
			priv->repeat = 0;
			priv->state = STATE_DATA_PULSE;
		}else{
			if(width >= CLK_REPEAT_SPACE-CLK_ERR && width <= CLK_REPEAT_SPACE+CLK_ERR){
				priv->state = STATE_REPEAT_PULSE;
			}else{
				priv->state = 0;
#ifdef DEBUG_IR_LG_CODE
				printk(IR_LG_DEV_NAME ": [STATE_HEADER_SPACE] w: %d, i: %d\n", width, priv->idx);
#endif
			}
		}
		break;

	case STATE_REPEAT_PULSE:
		if(width >= CLK_DATA_PULSE-CLK_ERR && width <= CLK_DATA_PULSE+CLK_ERR){
			priv->state = STATE_REPEAT_SPACE;
		}else{
			priv->state = 0;
#ifdef DEBUG_IR_LG_CODE
			printk(IR_LG_DEV_NAME ": [STATE_REPEAT_PULSE] w: %d, i: %d\n", width, priv->idx);
#endif
		}
		break;

	case STATE_REPEAT_SPACE:
		if(width > CLK_HEADER_PULSE+CLK_ERR && width <= CLK_GAP+CLK_ERR){
			priv->state = STATE_HEADER_PULSE;
			if(++priv->repeat > SKIP_REPEAT){
				schedule_delayed_work(&ir_work, 0);
			}
		}else{
			priv->state = 0;
#ifdef DEBUG_IR_LG_CODE
			printk(IR_LG_DEV_NAME ": [STATE_REPEAT_SPACE] w: %d, i: %d\n", width, priv->idx);
#endif
		}
		break;

	case STATE_DATA_PULSE:
		if(width >= CLK_DATA_PULSE-CLK_ERR && width <= CLK_DATA_PULSE+CLK_ERR){
			if(priv->idx >= DATA_PRE_BITS + DATA_BITS){
				priv->state = STATE_REPEAT_SPACE;
				priv->data_ok = priv->data;
				schedule_delayed_work(&ir_work, 0);
			}else{
				priv->state = STATE_DATA_SPACE;
			}
		}else{
			priv->state = 0;
#ifdef DEBUG_IR_LG_CODE
			printk(IR_LG_DEV_NAME ": [STATE_DATA_PULSE] w: %d, i: %d\n", width, priv->idx);
#endif
		}
		break;

	case STATE_DATA_SPACE:
		if(width >= CLK_DATA_1_SPACE-CLK_ERR && width <= CLK_DATA_1_SPACE+CLK_ERR){
			priv->state = STATE_DATA_PULSE;
			priv->data<<= 1;
			priv->data|= 1;
			priv->idx++;
		}else{
			if(width >= CLK_DATA_0_SPACE-CLK_ERR && width <= CLK_DATA_0_SPACE+CLK_ERR){
				priv->state = STATE_DATA_PULSE;
				priv->data<<= 1;
				priv->idx++;
			}else{
				priv->state = 0;
#ifdef DEBUG_IR_LG_CODE
				printk(IR_LG_DEV_NAME ": [STATE_DATA_SPACE] w: %d, i: %d\n", width, priv->idx);
#endif
			}
		}
		break;

	default:
#ifdef DEBUG_IR_LG_CODE
		printk(IR_LG_DEV_NAME ": [DEFAULT] w: %d, i: %d, 0x%08X\n", width, priv->idx, priv->data);
#endif
		break;
	}

	return IRQ_HANDLED;
}

static int __init ir_init(void)
{
    int error, i;

    gpio_request(ir_irq, "silan-gpio-ir-lg");
    gpio_direction_input(ir_irq);

    if(request_irq(gpiobase+ir_irq, ir_isr, 0, "sl_ir_lg", &ir_priv) != 0)
    {
        printk(KERN_ERR "ir_lg_input.c: Can't allocate irq %d\n", ir_irq);
        return (-EIO);
    }

    INIT_DELAYED_WORK(&ir_work, ir_produce);

    ir_dev = input_allocate_device();
    if (!ir_dev)
    {
        printk(KERN_ERR "ir_lg_input.c: Not enough memory\n");
        error = -ENOMEM;
        goto err_free_irq;
    }

    ir_dev->evbit[0] = BIT_MASK(EV_KEY);

    ir_dev->name = IR_LG_DEV_NAME;
    ir_dev->dev.init_name = "ir_lg_input";

    //set key code
    for(i = 0; i < CODENUM; i++)
    {
        ir_dev->keybit[BIT_WORD(ir_key_table[i].keycode)] |= BIT_MASK(ir_key_table[i].keycode);
    }

    error = input_register_device(ir_dev);
    if (error)
    {
        printk(KERN_ERR "ir_lg_input.c: Failed to register device\n");
        goto err_free_dev;
    }
    return 0;

err_free_dev:
    input_free_device(ir_dev);
err_free_irq:
    free_irq(gpiobase+ir_irq, NULL);
    return error;
}

static void __exit ir_exit(void)
{
    input_unregister_device(ir_dev);     //remove device
    free_irq(gpiobase+ir_irq, &ir_priv); //free ir_irq
}

module_init(ir_init);
module_exit(ir_exit);
MODULE_LICENSE("GPL");
