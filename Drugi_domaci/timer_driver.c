#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/device.h>

#include <linux/io.h> //iowrite ioread
#include <linux/slab.h>//kmalloc kfree
#include <linux/platform_device.h>//platform driver
#include <linux/of.h>//of_match_table
#include <linux/ioport.h>//ioremap

#include <linux/interrupt.h> //irqreturn_t, request_irq

// REGISTER CONSTANTS
#define XIL_AXI_TIMER_TCSR0_OFFSET		0x0
#define XIL_AXI_TIMER_TLR0_OFFSET		0x4
#define XIL_AXI_TIMER_TCR0_OFFSET		0x8
#define XIL_AXI_TIMER_TCSR1_OFFSET		0x10
#define XIL_AXI_TIMER_TLR1_OFFSET		0x14
#define XIL_AXI_TIMER_TCR1_OFFSET		0x18

#define XIL_AXI_TIMER_CSR_CASC_MASK	0x00000800
#define XIL_AXI_TIMER_CSR_ENABLE_ALL_MASK	0x00000400
#define XIL_AXI_TIMER_CSR_ENABLE_PWM_MASK	0x00000200
#define XIL_AXI_TIMER_CSR_INT_OCCURED_MASK 0x00000100
#define XIL_AXI_TIMER_CSR_ENABLE_TMR_MASK 0x00000080
#define XIL_AXI_TIMER_CSR_ENABLE_INT_MASK 0x00000040
#define XIL_AXI_TIMER_CSR_LOAD_MASK 0x00000020
#define XIL_AXI_TIMER_CSR_AUTO_RELOAD_MASK 0x00000010
#define XIL_AXI_TIMER_CSR_EXT_CAPTURE_MASK 0x00000008
#define XIL_AXI_TIMER_CSR_EXT_GENERATE_MASK 0x00000004
#define XIL_AXI_TIMER_CSR_DOWN_COUNT_MASK 0x00000002
#define XIL_AXI_TIMER_CSR_CAPTURE_MODE_MASK 0x00000001

#define BUFF_SIZE 20
#define DRIVER_NAME "timer"
#define DEVICE_NAME "xilaxitimer"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR ("Xilinx");
MODULE_DESCRIPTION("Test Driver for Zynq PL AXI Timer.");
MODULE_ALIAS("custom:xilaxitimer");

struct timer_info {
	unsigned long mem_start;
	unsigned long mem_end;
	void __iomem *base_addr;
	int irq_num;
};

dev_t my_dev_id;
static struct class *my_class;
static struct device *my_device;
static struct cdev *my_cdev;
static struct timer_info *tp = NULL;

// Promenljive
unsigned long long time = 0;
unsigned int endRead = 0, start = 1, time_set = 0;

static irqreturn_t xilaxitimer_isr(int irq,void*dev_id);
static void setup_and_start_timer(unsigned int milliseconds);
static int timer_probe(struct platform_device *pdev);
static int timer_remove(struct platform_device *pdev);
int timer_open(struct inode *pinode, struct file *pfile);
int timer_close(struct inode *pinode, struct file *pfile);
ssize_t timer_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset);
ssize_t timer_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset);
static int __init timer_init(void);
static void __exit timer_exit(void);

struct file_operations my_fops =
{
	.owner = THIS_MODULE,
	.open = timer_open,
	.read = timer_read,
	.write = timer_write,
	.release = timer_close,
};

static struct of_device_id timer_of_match[] = {
	{ .compatible = "xlnx,xps-timer-1.00.a", },
	{ /* end of list */ },
};

static struct platform_driver timer_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= timer_of_match,
	},
	.probe		= timer_probe,
	.remove		= timer_remove,
};


MODULE_DEVICE_TABLE(of, timer_of_match);


// Moja funkcija za konvertovanje broja u string
void int2str(unsigned int num, char* str)
{
	char* temp[3];
	
	temp[2] = 0;
	if(num < 10)
	{
		temp[0] = num + 48;
		temp[1] = 48;
	}
	else
	{
		temp[0] = num % 10 + 48;
		temp[1] = num / 10 + 48;
	}
	
	str = temp;
}

//***************************************************
// INTERRUPT SERVICE ROUTINE (HANDLER)

static irqreturn_t xilaxitimer_isr(int irq, void*dev_id)		
{      
	unsigned int data = 0;
	
	// Omoguci novi upis u tajmerski registar
	start = 1;
	
	// Check Timer Counter Value
	data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCR0_OFFSET);
	//printk(KERN_INFO "xilaxitimer_isr: Interrupt occurred !\n");

	// Clear Interrupt
	data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
	iowrite32(data | XIL_AXI_TIMER_CSR_INT_OCCURED_MASK,
			tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
	
	// Disable Timer
	printk(KERN_NOTICE "Times's up!\n");
	data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
	iowrite32(data & ~(XIL_AXI_TIMER_CSR_ENABLE_TMR_MASK), tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);

	return IRQ_HANDLED;
}
//***************************************************
//HELPER FUNCTION THAT RESETS AND STARTS TIMER WITH PERIOD IN MILISECONDS

static void setup_and_start_timer(unsigned long long time)
{
	// Disable Timer Counter
	unsigned int data = 0;

	// Disable timer/counter 0 while configuration is in progress
	data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
	iowrite32(data & ~(XIL_AXI_TIMER_CSR_ENABLE_TMR_MASK),
			tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
			
	// Disable timer/counter 1 while configuration is in progress
	data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCSR1_OFFSET);
	iowrite32(data & ~(XIL_AXI_TIMER_CSR_ENABLE_TMR_MASK),
			tp->base_addr + XIL_AXI_TIMER_TCSR1_OFFSET);

	// Set initial value in load registers
	iowrite32(time & 0x00000000ffffffff, tp->base_addr + XIL_AXI_TIMER_TLR0_OFFSET); // pisanje donjih 32 bita
	iowrite32(time & 0xffffffff00000000, tp->base_addr + XIL_AXI_TIMER_TLR1_OFFSET); // pisanje gornjih 32 bita
	
	// Set the CASC bit in TCSR0
	data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
	iowrite32(data | XIL_AXI_TIMER_CSR_CASC_MASK,
			tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
			
	// Load initial value into counter from load register 0
	data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
	iowrite32(data | XIL_AXI_TIMER_CSR_LOAD_MASK,
			tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);

	data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCSR1_OFFSET);
	iowrite32(data & ~(XIL_AXI_TIMER_CSR_LOAD_MASK),
			tp->base_addr + XIL_AXI_TIMER_TCSR1_OFFSET);
	
	/*	
	//** da li moram oba tajmera ili je dovoljno da ucitam samo za prvi, jer je vec setovan casc bit?
	// Load initial value into counter from load register 1
	data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCSR1_OFFSET);
	iowrite32(data | XIL_AXI_TIMER_CSR_LOAD_MASK,
			tp->base_addr + XIL_AXI_TIMER_TCSR1_OFFSET);

	data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCSR1_OFFSET);
	iowrite32(data & ~(XIL_AXI_TIMER_CSR_LOAD_MASK),
			tp->base_addr + XIL_AXI_TIMER_TCSR1_OFFSET);
	*/
	
	
	// Enable interrupts
	iowrite32(XIL_AXI_TIMER_CSR_ENABLE_INT_MASK,
			tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
	/*
	// Start Timer bz setting enable signal
	data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
	iowrite32(data | XIL_AXI_TIMER_CSR_ENABLE_TMR_MASK,
			tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
	*/
}

//***************************************************
// PROBE AND REMOVE
static int timer_probe(struct platform_device *pdev)
{
	struct resource *r_mem;
	int rc = 0;

	// Get phisical register adress space from device tree
	r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r_mem) {
		printk(KERN_ALERT "xilaxitimer_probe: Failed to get reg resource\n");
		return -ENODEV;
	}

	// Get memory for structure timer_info
	tp = (struct timer_info *) kmalloc(sizeof(struct timer_info), GFP_KERNEL);
	if (!tp) {
		printk(KERN_ALERT "xilaxitimer_probe: Could not allocate timer device\n");
		return -ENOMEM;
	}

	// Put phisical adresses in timer_info structure
	tp->mem_start = r_mem->start;
	tp->mem_end = r_mem->end;

	// Reserve that memory space for this driver
	if (!request_mem_region(tp->mem_start,tp->mem_end - tp->mem_start + 1,	DEVICE_NAME))
	{
		printk(KERN_ALERT "xilaxitimer_probe: Could not lock memory region at %p\n",(void *)tp->mem_start);
		rc = -EBUSY;
		goto error1;
	}

	// Remap phisical to virtual adresses
	tp->base_addr = ioremap(tp->mem_start, tp->mem_end - tp->mem_start + 1);
	if (!tp->base_addr) {
		printk(KERN_ALERT "xilaxitimer_probe: Could not allocate memory\n");
		rc = -EIO;
		goto error2;
	}

	// Get interrupt number from device tree
	tp->irq_num = platform_get_irq(pdev, 0);
	if (!tp->irq_num) {
		printk(KERN_ALERT "xilaxitimer_probe: Failed to get irq resource\n");
		rc = -ENODEV;
		goto error2;
	}

	// Reserve interrupt number for this driver
	if (request_irq(tp->irq_num, xilaxitimer_isr, 0, DEVICE_NAME, NULL)) {
		printk(KERN_ERR "xilaxitimer_probe: Cannot register IRQ %d\n", tp->irq_num);
		rc = -EIO;
		goto error3;
	
	}
	else {
		printk(KERN_INFO "xilaxitimer_probe: Registered IRQ %d\n", tp->irq_num);
	}

	printk(KERN_NOTICE "xilaxitimer_probe: Timer platform driver registered\n");
	return 0;//ALL OK

error3:
	iounmap(tp->base_addr);
error2:
	release_mem_region(tp->mem_start, tp->mem_end - tp->mem_start + 1);
	kfree(tp);
error1:
	return rc;
}




static int timer_remove(struct platform_device *pdev)
{
	// Disable timer
	unsigned int data=0;
	
	//*** Sta ovde raditi, samo tajmer 0 ili i tajmer 1?
	
	data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
	iowrite32(data & ~(XIL_AXI_TIMER_CSR_ENABLE_TMR_MASK),
			tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
	// Free resources taken in probe
	free_irq(tp->irq_num, NULL);
	iowrite32(0, tp->base_addr);
	iounmap(tp->base_addr);
	release_mem_region(tp->mem_start, tp->mem_end - tp->mem_start + 1);
	kfree(tp);
	printk(KERN_WARNING "xilaxitimer_remove: Timer driver removed\n");
	return 0;
}





//***************************************************
// FILE OPERATION functions

int timer_open(struct inode *pinode, struct file *pfile) 
{
	//printk(KERN_INFO "Succesfully opened timer\n");
	return 0;
}

int timer_close(struct inode *pinode, struct file *pfile) 
{
	//printk(KERN_INFO "Succesfully closed timer\n");
	return 0;
}

// sa cut se cita trenutno vreme

ssize_t timer_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset) 
{
	
	char buff[BUFF_SIZE];
	unsigned long long curr_time = 0;
	unsigned int data = 0;
	unsigned int len = 0;
	int ret;
	unsigned int day, hour, min, sec;
	char* day_s;
	char* hour_s;
	char* min_s;
	char* sec_s;
	
	if (endRead){
		endRead = 0;
		return 0;
	}
	
	// Procitati vrednosti iz brojackih registara
	// Sacuvati ih u neku promenljivu
	// Konvertovati ih u dati format
	
	data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCR1_OFFSET);
	curr_time += data;
	data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCR0_OFFSET);
	curr_time = (curr_time << 32) + data;
	
	day = curr_time / 24*60*60;
	curr_time -= day*24*60*60;
	hour = curr_time / 60*60;
	curr_time -= hour*60*60;
	min = curr_time / 60;
	curr_time -= min*60;
	sec = curr_time;
	
	// pravljenje stringova
	int2str(day, day_s);
	int2str(hour, hour_s);
	int2str(min, min_s);
	int2str(sec, sec_s);
	
	// Spajanje u jedan jedinstven string
	buffer = strcat(day_s, ':');
	buffer = strcat(buff, hour_s);
	buffer = strcat(buff, ':');
	buffer = strcat(buff, min_S);
	buffer = strcat(buff, ':');
	buffer = strcat(buff, sec_s);
	buff[11] = 0; // 12:11:38:02
	
	len = 12;
	ret = copy_to_user(buffer, buff, len);
	if(ret)
		return -EFAULT;
	
	endRead = 1;

	return len;
}



/*
	echo “dd:hh:mm:ss” > /dev/timer - upisuje se vreme, ali se ne pokrece tajmer
	echo “start” > /dev/timer - tajmer se pokrece
	echo “stop” > /dev/timer - tajmer se zaustavlja, ali se moze nastaviti sa start komandom

*/


ssize_t timer_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset) 
{
	unsigned int data;
	char buff[BUFF_SIZE];
	int ret = 0;
	char *command;
	char *h;
	char *m;
	char *s;
	
	ret = copy_from_user(buff, buffer, length);
	if(ret)
		return -EFAULT;
	buff[length] = '\0';
	
	ret = sscanf(buff,"%s", command);
	
	if(ret == 1)
	{
		h = strchr(command, ':'); // 12:10:03:01 
		if(h != NULL)
		{
			*h = 0; // command sadrzi dane
			h++;
			m = strchr(h, ':'); // 10:03:01
			if(h != NULL)
			{
				*h = 0; // command sadrzi dane
				h++;
				s = strchr(m, ':'); // 03:01
				if(s != NULL)
				{
					*s = 0;
					s++;
					
					// Najveci moguci upis je 99:23:59:59
					if((atoi(command) < 100) && (atoi(h) < 24) && (atoi(m) < 60) && (atoi(s) < 60))
					{
						if((!atoi(command)) && (!atoi(h)) && (!atoi(m)) && (!atoi(s)))
						{
							time_set = 1;
							start = 1; // mozes da upises ovo vreme dok prethodno nije isteklo
							time = atoi(command)*24*60*60 + atoi(h)*60*60 + atoi(m)*60 + atoi(s);
							printk(KERN_INFO "Time is set!\n");
						}	
						else
							printk(KERN_WARNING "Time is zero!\n");
					}
					else
					{
						printk(KERN_WARNING "Values are over the limits!\n");
					}
					
				}
				else
					printk(KERN_WARNING "xilaxitimer_write: Invalid command format\n");
			}
			else
				printk(KERN_WARNING "xilaxitimer_write: Invalid command format\n");
		}
		else
		{
			if(!strcmp(command, "stop"))
			{
				// Resetujem enable bit da bi se tajmer zaustavio
				data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
				iowrite32(data & ~(XIL_AXI_TIMER_CSR_ENABLE_TMR_MASK), tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
			}
			else if(!strcmp(command, "start"))
			{
				// *** Ovde treba pokrenuti onu funkciju za inicijalizaciju kada se prvi put poziva start
				// *** Mozda dodati flag za prekid, kako bi sve radilo za naredni pokusaj
				
				if(start)
				{
					start = 0;
					if(time_set)
					{
						time_set = 0;
						setup_and_start_timer(time);
					}
					else
					{
						printk(KERN_WARNING "Timer is not set!");
						start = 1; // mozes ponovo da upises, jer nije ovog puta bilo dobro
					}
				}
				
				data = ioread32(tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
				iowrite32(data | XIL_AXI_TIMER_CSR_ENABLE_TMR_MASK, tp->base_addr + XIL_AXI_TIMER_TCSR0_OFFSET);
			}
			else
				printk(KERN_WARNING "xilaxitimer_write: Invalid command format\n");
		}
	}
	else
		printk(KERN_WARNING "xilaxitimer_write: Wrong format, expected only one parameter\n");
	
	
	return length;
}





//***************************************************
// MODULE_INIT & MODULE_EXIT functions

static int __init timer_init(void)
{
	int ret = 0;


	ret = alloc_chrdev_region(&my_dev_id, 0, 1, DRIVER_NAME);
	if (ret){
		printk(KERN_ERR "xilaxitimer_init: Failed to register char device\n");
		return ret;
	}
	printk(KERN_INFO "xilaxitimer_init: Char device region allocated\n");

	my_class = class_create(THIS_MODULE, "timer_class");
	if (my_class == NULL){
		printk(KERN_ERR "xilaxitimer_init: Failed to create class\n");
		goto fail_0;
	}
	printk(KERN_INFO "xilaxitimer_init: Class created\n");

	my_device = device_create(my_class, NULL, my_dev_id, NULL, DRIVER_NAME);
	if (my_device == NULL){
		printk(KERN_ERR "xilaxitimer_init: Failed to create device\n");
		goto fail_1;
	}
	printk(KERN_INFO "xilaxitimer_init: Device created\n");

	my_cdev = cdev_alloc();	
	my_cdev->ops = &my_fops;
	my_cdev->owner = THIS_MODULE;
	ret = cdev_add(my_cdev, my_dev_id, 1);
	if (ret)
	{
		printk(KERN_ERR "xilaxitimer_init: Failed to add cdev\n");
		goto fail_2;
	}
	printk(KERN_INFO "xilaxitimer_init: Cdev added\n");
	printk(KERN_NOTICE "xilaxitimer_init: Hello world\n");

	return platform_driver_register(&timer_driver);

fail_2:
	device_destroy(my_class, my_dev_id);
fail_1:
	class_destroy(my_class);
fail_0:
	unregister_chrdev_region(my_dev_id, 1);
	return -1;
}

static void __exit timer_exit(void)
{
	platform_driver_unregister(&timer_driver);
	cdev_del(my_cdev);
	device_destroy(my_class, my_dev_id);
	class_destroy(my_class);
	unregister_chrdev_region(my_dev_id,1);
	printk(KERN_INFO "xilaxitimer_exit: Goodbye, cruel world\n");
}


module_init(timer_init);
module_exit(timer_exit);
