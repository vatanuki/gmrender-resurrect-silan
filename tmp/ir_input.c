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

//#define DEBUG_BUF

DECLARE_WAIT_QUEUE_HEAD(ir_wq);

#define CODENUM 2 //48

static const struct {
   int ircode;
   int keycode;
} ir_key_table[] = {
    { 0x08F7, KEY_0 },
    { 0x8877, KEY_1 },
    { 0x48B7, KEY_2 },
    { 0xC837, KEY_3 },
    { 0x28D7, KEY_4 },
    { 0xA857, KEY_5 },
    { 0x6897, KEY_6 },
    { 0xE817, KEY_7 },
    { 0x18E7, KEY_8 },
    { 0x9867, KEY_9 },
    { 0x14EB, KEY_BACK },
    { 0x10EF, KEY_POWER },
    { 0x906F, KEY_MUTE },
    { 0x40BF, KEY_VOLUMEUP },
    { 0xC03F, KEY_VOLUMEDOWN },
    { 0x0DF2, KEY_PLAY },
    { 0x5DA2, KEY_PAUSE },
    { 0x8D72, KEY_STOP },
    { 0xA25D, KEY_MENU },
    { 0x02FD, KEY_UP },
    { 0, KEY_DOWN },
    { 0xE01F, KEY_LEFT },
    { 0x609F, KEY_RIGHT },
    { 0x22DD, KEY_ENTER },

/*    
          KEY_TV                   0x0FF0
          KEY_SUBTITLE             0x9C63
          KEY_A                    0x8976	# Was:	AD
          KEY_R                    0x9E61	# Was:	RATIO
          KEY_AUX                  0xD02F	# Was:	INPUT
          KEY_LIST                 0xCA35
          KEY_V                    0x58A7	# Was:	Q.VIEW
          KEY_FAVORITES            0x7887
          KEY_G                    0xD52A	# Was:	GUIDE

          KEY_CHANNELUP            0x00FF
          KEY_CHANNELDOWN          0x807F
          KEY_TEXT                 0x04FB
          KEY_INFO                 0x55AA
          KEY_OPTION               0x847B
          KEY_SETUP                0xC23D	# Was:	SETTINGS
          KEY_MENU                 	# Was:	Q.MENU
          KEY_RIGHT                0x609F
          KEY_EXIT                 0xDA25
          KEY_MODE                 0x0CF3	# Was:	AV MODE
          KEY_RECORD               0xBD42	# Was:	REC
          KEY_REWIND               0xF10E
          KEY_FORWARD              0x718E
          KEY_RED                  0x4EB1
          KEY_GREEN                0x8E71
          KEY_YELLOW               0xC639
          KEY_BLUE                 0x8679 */

};

/* Private data structure */
struct ir_private {
    unsigned long *key_table;      /* Table for repetition keys */
    spinlock_t lock;               /* Spin lock */
    unsigned long last_jiffies;    /* Timestamp for last reception */
    unsigned char *ir_devname;
    unsigned int width;
    struct timeval     base_time;
    int previous_data;
};

static int idx;
static int state;
static int data;
static int data2;
static int repeat;
static unsigned int width;

#ifdef DEBUG_BUF
static int buf[255];
#endif

struct input_dev *ir_dev;
struct ir_private ir_priv;
struct delayed_work ir_work;
static int ir_irq = 12;
static int gpiobase = 96;

static void ir_produce(void);
static irqreturn_t ir_isr(int irq, void *dev_id);
static int __init ir_init(void);
static void __exit ir_exit(void);

/* Produce data */
static void ir_produce()
{
    unsigned int i, key = data2;

#ifdef DEBUG_BUF
    disable_irq(gpiobase+ir_irq);
    for(i = 0; i < 255; i++)
        printk("%d = %d\n", i, buf[i]);
    enable_irq(gpiobase+ir_irq);
    return;
#endif

//    disable_irq(gpiobase+ir_irq);

    printk("data: 0x%08x\n", key);

    if((key >> DATA_BITS) == DATA_PRE){
        key&= ~(-1 << DATA_BITS);
        for(i = 0; i < sizeof(ir_key_table)/8; i++){
            if(key == ir_key_table[i].ircode){

                printk("key: 0x%08x, code: %d\n", key, ir_key_table[i].keycode);

                input_report_key(ir_dev, ir_key_table[i].keycode, 1);
                input_report_key(ir_dev, ir_key_table[i].keycode, 0);
                input_sync(ir_dev);
            }
        }
    }

//    enable_irq(gpiobase+ir_irq);

    return ;
}

static irqreturn_t ir_isr(int irq, void *dev_id)
{
    struct ir_private *priv = (struct ir_private *) dev_id;
    struct timeval tv;

    do_gettimeofday(&tv);
    width = 1000000 * (tv.tv_sec - priv->base_time.tv_sec)
            + tv.tv_usec - priv->base_time.tv_usec;

    //store current time
    priv->base_time.tv_sec = tv.tv_sec;
    priv->base_time.tv_usec = tv.tv_usec;

#ifdef DEBUG_BUF
    if(idx < 0 || idx > 255){
        idx = 0;
        schedule_delayed_work(&ir_work, 0);
    }
    buf[idx++] = width;
    return IRQ_HANDLED;
#endif

	if(width > CLK_GAP+CLK_ERR){
		state = STATE_HEADER_PULSE;
		return IRQ_HANDLED;
	}

	switch(state){
	case STATE_HEADER_PULSE:
		if(width >= CLK_HEADER_PULSE-CLK_ERR && width <= CLK_HEADER_PULSE+CLK_ERR)
			state = STATE_HEADER_SPACE;
		else{
			state = 0;
			printk("[ERR2] p: %d, w: %d, i: %d\n", gpio_get_value(ir_irq), width, idx);
		}
		break;

	case STATE_HEADER_SPACE:
		if(width >= CLK_HEADER_SPACE-CLK_ERR && width <= CLK_HEADER_SPACE+CLK_ERR){
			idx = 0;
			data = 0;
			repeat = 0;
			state = STATE_DATA_PULSE;
		}else{
			if(width >= CLK_REPEAT_SPACE-CLK_ERR && width <= CLK_REPEAT_SPACE+CLK_ERR){
				state = STATE_REPEAT_PULSE;
			}else{
				state = 0;
				printk("[ERR3] p: %d, w: %d, i: %d\n", gpio_get_value(ir_irq), width, idx);
			}
		}
		break;

	case STATE_REPEAT_PULSE:
		if(width >= CLK_DATA_PULSE-CLK_ERR && width <= CLK_DATA_PULSE+CLK_ERR){
			state = STATE_REPEAT_SPACE;
		}else{
			state = 0;
			printk("[ERR7] p: %d, w: %d, i: %d\n", gpio_get_value(ir_irq), width, idx);
		}
		break;

	case STATE_REPEAT_SPACE:
		if(width > CLK_HEADER_PULSE+CLK_ERR && width <= CLK_GAP+CLK_ERR){
			state = STATE_HEADER_PULSE;
			if(++repeat > SKIP_REPEAT){
				schedule_delayed_work(&ir_work, 0);
			}
		}else{
			state = 0;
			printk("[ERR8] p: %d, w: %d, i: %d\n", gpio_get_value(ir_irq), width, idx);
		}
		break;

	case STATE_DATA_PULSE:
		if(width >= CLK_DATA_PULSE-CLK_ERR && width <= CLK_DATA_PULSE+CLK_ERR){
			if(idx >= DATA_PRE_BITS + DATA_BITS){
				state = STATE_REPEAT_SPACE;
				data2 = data;
				schedule_delayed_work(&ir_work, 0);
			}else{
				state = STATE_DATA_SPACE;
			}
		}else{
			state = 0;
			printk("[ERR4] p: %d, w: %d, i: %d\n", gpio_get_value(ir_irq), width, idx);
		}
		break;

	case STATE_DATA_SPACE:
		if(width >= CLK_DATA_1_SPACE-CLK_ERR && width <= CLK_DATA_1_SPACE+CLK_ERR){
			state = STATE_DATA_PULSE;
			data<<= 1;
			data|= 1;
			idx++;
		}else{
			if(width >= CLK_DATA_0_SPACE-CLK_ERR && width <= CLK_DATA_0_SPACE+CLK_ERR){
				state = STATE_DATA_PULSE;
				data<<= 1;
				idx++;
			}else{
				state = 0;
				printk("[ERR5] p: %d, w: %d, i: %d\n", gpio_get_value(ir_irq), width, idx);
			}
		}
		break;

	default:
		printk("[ERR1] w: %d, i: %d, 0x%08x\n", width, idx, data);
	}

	return IRQ_HANDLED;
}

static void ir_work_fn(struct work_struct *work)
{
    return ir_produce();
}

static int __init ir_init(void)
{
    int error, i;

    gpio_request(ir_irq, "silan-gpio-ir");
    gpio_direction_input(ir_irq);

    //hook up isr
    if(request_irq(gpiobase+ir_irq, ir_isr, IRQF_DISABLED, "sl_ir", &ir_priv) != 0)
    {
        printk(KERN_ERR "ir_input.c: Can't allocate irq %d\n", ir_irq);
        return (-EIO);
    }

    INIT_DELAYED_WORK(&ir_work, ir_work_fn);

    ir_dev = input_allocate_device();    //allocate space for ir_dev
    if (!ir_dev)
    {
        printk(KERN_ERR "ir_input.c: Not enough memory\n");
        error = -ENOMEM;
        goto err_free_irq;
    }

    ir_dev->evbit[0] = BIT_MASK(EV_KEY);        //set key type

    ir_dev->name = "sl-ir";
    ir_dev->dev.init_name = "ir_input";

    //set key code
    for(i = 0; i < CODENUM; i++)
    {
        ir_dev->keybit[BIT_WORD(ir_key_table[i].keycode)] |= BIT_MASK(ir_key_table[i].keycode);
    }

    error = input_register_device(ir_dev);        //register the ir_input device
    if (error)
    {
        printk(KERN_ERR "ir_input.c: Failed to register device\n");
        goto err_free_dev;
    }
    return 0;

err_free_dev:
    input_free_device(ir_dev);
err_free_irq:
    free_irq(gpiobase+ir_irq, ir_isr);
    return error;
}

static void __exit ir_exit(void)
{

    input_unregister_device(ir_dev);    //remove device
    free_irq(gpiobase+ir_irq, ir_isr);  //free ir_irq
}

module_init(ir_init);
module_exit(ir_exit);
MODULE_LICENSE("GPL");

/*
# modprobe ir_input
kn gpio_pull_mode 12 0
input: sl-ir as /devices/virtual/input/ir_input
# 0, w: -1157483102
1, w: 8995
2, w: -4535
3, w: 520
4, w: -612
5, w: 513
6, w: -599
7, w: 524
8, w: -1748
9, w: 499
10, w: -600
11, w: 524
12, w: -600
13, w: 523
14, w: -604
15, w: 532
16, w: -590
17, w: 521
18, w: -604
19, w: 520
20, w: -1752
21, w: 496
22, w: -1753
23, w: 493
24, w: -603
25, w: 522
26, w: -1750
27, w: 496
28, w: -1751
29, w: 496
30, w: -1752
31, w: 495
32, w: -1749
33, w: 498
34, w: -1740
35, w: 506
36, w: -599
37, w: 525
38, w: -598
39, w: 524
40, w: -1759
41, w: 487
42, w: -601
43, w: 523
44, w: -601
45, w: 523
46, w: -601
47, w: 522
48, w: -1749
49, w: 506
50, w: -605
51, w: 511
52, w: -1750
53, w: 506
54, w: -1740
55, w: 495
56, w: -603
57, w: 522
58, w: -1746
59, w: 501
60, w: -1749
61, w: 497
62, w: -1749
63, w: 499
64, w: -599
65, w: 523
66, w: -1751
67, w: 496
68, w: -40229
69, w: 9023
70, w: -2305
71, w: 502
72, w: -96405
73, w: 8999
74, w: -2308
75, w: 500
76, w: -96413
77, w: 8995
78, w: -2316
79, w: 495
80, w: -96414
81, w: 9008
82, w: -2311
83, w: 509
84, w: -96409
85, w: 8999
86, w: -2306
87, w: 502
88, w: -96389
89, w: 9024
90, w: -2308
91, w: 500
92, w: -96390
93, w: 9025
94, w: -2310
95, w: 499
96, w: -96387
97, w: 9031
98, w: -2299
99, w: 500
100, w: -96422
101, w: 8997
102, w: -2314
103, w: 495
104, w: -96404
105, w: 8997
106, w: -2307
107, w: 500
108, w: -96398
109, w: 9008
110, w: -2312
111, w: 498
112, w: -96400
113, w: 9004
114, w: -2303
115, w: 502
116, w: -96398
117, w: 9004
118, w: -2311
119, w: 497
120, w: -96402
121, w: 8996
122, w: -2312
123, w: 499
124, w: -96407
125, w: 8997
126, w: -2321
127, w: 487
128, w: -96399
129, w: 9006
130, w: -2307
131, w: 502
132, w: -96413
133, w: 8994
134, w: -2311
135, w: 498
136, w: -96417
137, w: 9005
138, w: -2307
139, w: 496
140, w: -96400
141, w: 9017
142, w: -2309
143, w: 499
144, w: -96398
145, w: 9016
146, w: -2315
147, w: 495
148, w: -96410
149, w: 9001
150, w: -2306
151, w: 511
152, w: -96406
153, w: 8998
154, w: -2318
155, w: 490
156, w: -96411
157, w: 9000
158, w: -2308
159, w: 499
160, w: -96420
161, w: 8999
162, w: -2309
163, w: 498
164, w: -96406
165, w: 9008
166, w: -2313
167, w: 495
168, w: -96391
169, w: 9020
170, w: -2309
171, w: 500
172, w: -96403
173, w: 9013
174, w: -2307
175, w: 502
176, w: -96418
177, w: 8998
178, w: -2306
179, w: 502
180, w: -96374
181, w: 9046
182, w: -2308
183, w: 500
184, w: -96409
185, w: 8997
186, w: -2309
187, w: 499
188, w: -96415
189, w: 8999
190, w: -2321
191, w: 486
192, w: -96418
193, w: 9002
194, w: -2323
195, w: 487
196, w: -96400
197, w: 9009
198, w: -2310
199, w: 497
200, w: -96413
201, w: 9002
202, w: -2310
203, w: 498
204, w: -96388
205, w: 9022
206, w: -2312
207, w: 496
208, w: -96421
209, w: 8999
210, w: -2310
211, w: 498
212, w: -96412
213, w: 9003
214, w: -2313
215, w: 495
216, w: -96370
217, w: 9051
218, w: -2316
219, w: 493
220, w: -96422
221, w: 9000
222, w: -2310
223, w: 500
224, w: -96413
225, w: 9029
226, w: -2310
227, w: 500
228, w: -96396
229, w: 9023
230, w: -2305
231, w: 502
232, w: -96407
233, w: 9007
234, w: -2304
235, w: 497
236, w: -96388
237, w: 9022
238, w: -2310
239, w: 499
240, w: -96414
241, w: 9006
242, w: -2306
243, w: 499
244, w: -96393
245, w: 9026
246, w: -2312
247, w: 496
248, w: -96408
249, w: 8999
*/