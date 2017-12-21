/* tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"

#define debug(str, ...) \
	printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)

/* added by Sean 10/14/2017 */
/* parameters */
spinlock_t buttonsLock;
int ackStatus = 1;						/* 0 waiting ack, 1 available */
int resetStatus = 0;
unsigned long buttonStatus;
unsigned long ledStatus = 0;		/* used to store value of leds in order to restore them later */
const char data_7seg [16] = {0xE7, 0x06, 0xCB, 0x8F, 0x2E, 0xAD, 0xED, 0x86, 0xEF, 0xAF, 0xEE, 0x6D, 0xE1, 0x4F, 0xE9, 0xE8};

/* added by Sean 10/14/2017 */
/* functions */
int tuxctl_ioctl_init(struct tty_struct* tty);
int tuxctl_ioctl_buttons(struct tty_struct* tty, unsigned long arg);
int tuxctl_ioctl_set_led (struct tty_struct* tty, unsigned long arg);
int tuxctl_ioctl_led_request(struct tty_struct* tty);
int tuxctl_ioctl_read_led (struct tty_struct* tty, unsigned long arg);
int tuxtl_handle_get_button(unsigned b, unsigned c);

/************************ Protocol Implementation *************************/

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in 
 * tuxctl-ld.c. It calls this function, so all warnings there apply 
 * here as well.
 */
void tuxctl_handle_packet (struct tty_struct* tty, unsigned char* packet)
{
	unsigned a, b, c;
	unsigned long flag;

    a = packet[0]; /* Avoid printk() sign extending the 8-bit */
    b = packet[1]; /* values when printing them. */
    c = packet[2];

    //printk("packet : %x %x %x\n", a, b, c);
    switch(a)
    {
    	case MTCP_ERROR:
    		ackStatus = 1;
			break;
			
     	case MTCP_ACK:
     		ackStatus = 1;
			
			if(resetStatus)
			{
				/* tty cannot be null */
				if(tty == NULL)	break;
				/* restore the before status */
				tuxctl_ioctl_set_led(tty, ledStatus);
				resetStatus = 0;
			}
     		break;
     		
     	case MTCP_BIOC_EVENT:
     		/* prevent button r/w at the same time */
			spin_lock_irqsave(&buttonsLock, flag);
			/*
			;	byte 1  +-7-----4-+-3-+-2-+-1-+---0---+
			;			| 1 X X X | C | B | A | START |
			;			+---------+---+---+---+-------+
			;	byte 2  +-7-----4-+---3---+--2---+--1---+-0--+
			;			| 1 X X X | right | down | left | up |
			;			+---------+-------+------+------+----+
			 */
			buttonStatus = (b&0xf) + ((c&0xf) << 4);
			spin_unlock_irqrestore(&buttonsLock, flag);
     		break;
     		
     	case MTCP_RESET:
			/* tty cannot be null */
				if(tty == NULL)	break;
     		tuxctl_ioctl_init(tty);	
			
			/* set resetStatus to 1 to restore later */
			resetStatus = 1;
			break; 
			
		 default:
		 	break;
     }
}

/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/
int tuxctl_ioctl (struct tty_struct* tty, struct file* file, unsigned cmd, unsigned long arg)
{
    switch (cmd) 
    {
		case TUX_INIT:
			/* tty cannot be null */
			if(tty == NULL)	return -EINVAL;
			return  tuxctl_ioctl_init(tty);
			
		case TUX_BUTTONS:
			/* arg cannot be null */
			if(arg == (long)NULL)	return -EINVAL;
			return  tuxctl_ioctl_buttons(tty, arg);
			
		case TUX_SET_LED:
			/* if waiting for ack, can't do anything */
			if(!ackStatus)	return -EINVAL;
			/* tty cannot be null */
			if(tty == NULL)	return -EINVAL;
			return  tuxctl_ioctl_set_led(tty, arg);
			
		case TUX_LED_ACK:
			return 0;
		case TUX_LED_REQUEST:
			return 0;
		case TUX_READ_LED:
			return 0;
		default:
	    	return -EINVAL;
    }
}


/* 
 * added by Sean 10/14/2017
 * tuxctl_ioctl_init
 * DESCRIPTION: initialize tux controller
 *   INPUTS: tty pointer
 *   OUTPUTS: none
 *   RETURN VALUE: 0
 *   SIDE EFFECTS: setup tux controller
 */
 int tuxctl_ioctl_init(struct tty_struct* tty)
 {
 	/* initialization commands */
	char command[2] = {MTCP_BIOC_ON, MTCP_LED_USR};

	/* initialization for ack */
	ackStatus = 0;

	/* send the packet to tux controller */
	tuxctl_ldisc_put(tty, command, 2);
 	
 	/* initialize buttonStatus */
 	buttonStatus = 0x0;

 	/* initialize the lock */
 	buttonsLock = SPIN_LOCK_UNLOCKED;
	
 	return 0;
 }


/* 
 * added by Sean 10/13/2017
 * tuxctl_ioctl_buttons
 * DESCRIPTION: Takes a pointer to a 32-bit integer.
 *				sets the bits of the low byte corresponding to the currently pressed buttons,
 *				the order will be
 *				right > left > down > up > c > b > a > start
 *   INPUTS: arg, the address which the information is copied to
 *   OUTPUTS: none
 *   RETURN VALUE: 0: sucess, -EINVAL: error
 *   SIDE EFFECTS: none
 */
int tuxctl_ioctl_buttons(struct tty_struct* tty, unsigned long arg)
{
	unsigned long flag;
	unsigned long *buttonsPtr = &buttonStatus;
	
	/* prevent button r/w at the same time */
	spin_lock_irqsave(&buttonsLock, flag);

	/* copy to user space */
	if (copy_to_user((void*)arg, (void *)buttonsPtr, sizeof(buttonStatus)) != 0)
	{
		spin_unlock_irqrestore(&buttonsLock, flag);
		return -EINVAL;
	}
	
	spin_unlock_irqrestore(&buttonsLock, flag);
	return 0;
}

/* 
 * added by Sean 10/13/2017
 * tuxctl_ioctl_set_led
 * DESCRIPTION: copy the 8 buttons' status into user, the order will be
 *			right > left > down > up > c > b > a > start
 *   INPUTS: tty pointer
 *			arg: a 32-bit integer of the following form: The low 16-bits specify a number whose
 *			hexadecimal value is to be displayed on the 7-segment displays. The low 4 bits of the third byte
 *			specifies which LED’s should be turned on. The low 4 bits of the highest byte (bits 27:24) specify
 *			whether the corresponding decimal points should be turned on. This ioctl should return 0.
 *   OUTPUTS: none
 *   RETURN VALUE: 0: sucess, -EINVAL: error
 *   SIDE EFFECTS: none
 */
 int tuxctl_ioctl_set_led (struct tty_struct* tty, unsigned long arg)
 {
 	int i;
	char userMode = MTCP_LED_USR;
	char led_on_off = (arg & 0xf0000)>>16;
	char dec_where_on = (arg & 0xf000000)>>24;
	char digit_on7seg[4];
	char command[6] = {MTCP_LED_SET, 0xf};
	/*		for command[1]
	 ;		 __7___6___5___4____3______2______1______0___
	 ; 		| X | X | X | X | LED3 | LED2 | LED1 | LED0 | 
	 ; 		----+---+---+---+------+------+------+------+
	 */
 	
 	/* waiting for ack */
 	ackStatus = 0;
 	
 	/* change to user mode first */
	tuxctl_ldisc_put(tty, &userMode, 1);
	
	/* retrieve the data from arg and assign them into digit_on7seg */
	for(i=0;i<4;i++)
	{
		digit_on7seg[i] = data_7seg[(arg & (0xf<<i*4)) >> (i*4)];
		
		/* if this digit is the same as dec, add dec into the corresponding digit */
		if(dec_where_on & (0x1<<i))
			digit_on7seg[i] |= 0x10;
	}

 	/* put the information into command */
	for(i=0;i<4;i++)
	{
		/* if the corresponding digit should be on, assign digit_on7seg to it */
		if(led_on_off & (0x1<<i))
			command[i+2] = digit_on7seg[i];
		/* if not, assign 0x0(dim) to it */
		else
			command[i+2] = 0x0;
	}
	
 	/* store led status now in order to restore later once reset */
 	ledStatus = arg;

	/* send the packet to tux controller */
	tuxctl_ldisc_put(tty, command, 6);

	return 0;
 }
