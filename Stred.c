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
#include <linux/wait.h>
#include <linux/semaphore.h>
#define BUFF_SIZE 120 // max velicina buffera
#define STR_SIZE 101
MODULE_LICENSE("Dual BSD/GPL");

DECLARE_WAIT_QUEUE_HEAD(writeQ);	
DECLARE_WAIT_QUEUE_HEAD(readQ);
struct semaphore sem;

dev_t my_dev_id;
static struct class *my_class;
static struct device *my_device;
static struct cdev *my_cdev;

unsigned char endRead = 0;
int l = 0;
char string[STR_SIZE]; // jedan karakter za '\0'

int stred_open(struct inode *pinode, struct file *pfile);
int stred_close(struct inode *pinode, struct file *pfile);
ssize_t stred_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset);
ssize_t stred_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset);

void remove_seq(char* str, char* occ, int occ_len)
{

	if(str == occ) // obrazac koji se uklanja je na pocetku stringa
		str += occ_len;
	else
	{
		*occ = 0;
		// occ++; Aleksa_voli_jaja, occ = v
		// Aleksa_0oli_jaja
		// printk(KERN_INFO "%s, %s \n", str, occ + occ_len);
		strcat(str, occ + occ_len);
	}
}

struct file_operations my_fops =
{
	.owner = THIS_MODULE,
	.open = stred_open,
	.read = stred_read,
	.write = stred_write,
	.release = stred_close,
};

int stred_open(struct inode *pinode, struct file *pfile) 
{
		printk(KERN_INFO "Succesfully opened stred\n");
		return 0;
}

int stred_close(struct inode *pinode, struct file *pfile) 
{
		printk(KERN_INFO "Succesfully closed stred\n");
		return 0;
}

// Dovde sam stigao
ssize_t stred_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset) 
{
	int ret; // ovde cemo sacuvati info da li je doslo do problema
		 // prilikom preuzimanja stringa od korisnika
	char buff[BUFF_SIZE]; // mesto gde skaldistimo string
	long int len = 0;
	
	if(endRead)
	{
		endRead = 0;
		return 0;
	}

	endRead = 1;
	

	
	if(down_interruptible(&sem))
		return -ERESTARTSYS;
	while(l <= 0)
	{
		up(&sem);
		if(wait_event_interruptible(readQ, (l > 0)))
			return -ERESTARTSYS;
		if(down_interruptible(&sem))
			return -ERESTARTSYS;
	}
	

	len = scnprintf(buff, BUFF_SIZE, "%s", string);
	if(len == 0)
	{
		printk(KERN_WARNING "String is empty\n");
	}
	else
	{
		ret = copy_to_user(buffer, buff, len);
		printk(KERN_INFO "String je %s, a njegova duzina %d \n", string, strlen(string));
		if(ret)	
			return -EFAULT;

		printk(KERN_INFO "Succesfully read\n");
	}

	
	up(&sem);
	wake_up_interruptible(&writeQ);	
	

	return len;
}

ssize_t stred_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset) 
{
	char buff[BUFF_SIZE];
	char command[BUFF_SIZE];
	int ret;
	char* ptr; // ptr ce pokazivati na '='
	int i;

	ret = copy_from_user(buff, buffer, length);

	if(ret)
		return -EFAULT;

	buff[length-1] = '\0';

	
	if(down_interruptible(&sem))
		return -ERESTARTSYS;
	while(l >= STR_SIZE)
	{
		up(&sem);
		if(wait_event_interruptible(writeQ, (l < STR_SIZE)))
			return -ERESTARTSYS;
		if(down_interruptible(&sem))
			return -ERESTARTSYS;
	}
	

	strcpy(command, buff);

	ret = sscanf(buff,"%s", buff); // iz buff se prepisuje vrednost u command
	
	ptr = strchr(command, '=');
	if(ptr != NULL)
	{
		*ptr = 0;
		ptr++;
	}


	printk(KERN_INFO "         Komanda je: %s, a argument: %s \n", command, ptr);
	if(ret==1)//treba samo jedan parametar da se prosledi
	{
		
		if(!strcmp(command, "string"))
		{
			if(strlen(ptr) > STR_SIZE - 1)
				printk(KERN_WARNING "Length is over the limit\n");
			else
			{	
				l = strlen(ptr);
				strcpy(string, ptr); // kopiraj string desno od '=' u str
				printk(KERN_INFO "Successfully executed %s command\n", command);
			}				
		}

		else if(!strcmp(command, "append"))
		{
			if(strlen(string) + strlen(ptr) > STR_SIZE - 1)
				printk(KERN_WARNING "Length is over the limit\n");
			else
			{
				l = strlen(string) + strlen(ptr);
				strcat(string, ptr);
				printk(KERN_INFO "Successfully executed %s command\n", command);
			}
		}

		else if(!strcmp(command, "truncate"))
		{	
			int n_t;
			kstrtoint(ptr, 10, &n_t); // paziti na osnovu 10
			if(strlen(string) < n_t)
			{
				printk(KERN_WARNING "Not enough characters in string\n");
			}
			else
			{
				l = strlen(string) - n_t;
				string[strlen(string) - n_t] = 0;
				printk(KERN_INFO "Successfully executed %s command\n", command);
			}
		}

		else if(!strcmp(command, "remove"))
		{
			if(strlen(string) < strlen(ptr))
				printk(KERN_WARNING "Not enough characters in string\n");
			else
			{
				int ptr_len = strlen(ptr);
				char* occ = strstr(string, ptr); // pokazivac na prvu pojavu
				while(occ != NULL)
				{	
					l = strlen(string) - ptr_len;
					remove_seq(string, occ, ptr_len);
					occ = strstr(string, ptr);
				}
				printk(KERN_INFO "Successfully executed %s command\n", command);
			}
		}

		else if(!strcmp(command, "clear"))
		{	
			l = 0;
			for(i = 0; i < STR_SIZE; i++)
			{
				string[i] = 0;
			}
			printk(KERN_INFO "Successfully executed %s command\n", command);
		}

		else if(!strcmp(command, "shrink"))
		{
			char temp[STR_SIZE];
			int j = 0;
			for(i = 0; i < STR_SIZE; i++)
				temp[i] = 0;
			for(i = 0; i < strlen(string); i++)
			{
				if(string[i] != ' ')
				{
					for(j = i; j < strlen(string); j++)
						temp[j - i] = string[j];
					break;
				}
				l--;
				
			}
			for(i = strlen(temp) - 1; i >= 0; i--)
			{
				if(temp[i] == ' ')
				{
					temp[i] = 0;
					l--;
				}
				else
					break;
			}
			strcpy(string, temp);
			printk(KERN_INFO "Successfully executed %s command\n", command);
		}

		else
			printk(KERN_WARNING "Wrong command format\n");

	}
	else
	{
		printk(KERN_WARNING "Wrong command format\n");
	}

	
	up(&sem);
	wake_up_interruptible(&readQ);


	return length;
}

static int __init stred_init(void)
{
   int ret = 0;
	int i=0;
	
   sema_init(&sem,1);

	//Initialize array
	for (i=0; i< STR_SIZE; i++)
		string[i] = 0;

   ret = alloc_chrdev_region(&my_dev_id, 0, 1, "stred");
   if (ret){
      printk(KERN_ERR "failed to register char device\n");
      return ret;
   }
   printk(KERN_INFO "char device region allocated\n");

   my_class = class_create(THIS_MODULE, "stred_class");
   if (my_class == NULL){
      printk(KERN_ERR "failed to create class\n");
      goto fail_0;
   }
   printk(KERN_INFO "class created\n");
   
   my_device = device_create(my_class, NULL, my_dev_id, NULL, "stred");
   if (my_device == NULL){
      printk(KERN_ERR "failed to create device\n");
      goto fail_1;
   }
   printk(KERN_INFO "device created\n");

	my_cdev = cdev_alloc();	
	my_cdev->ops = &my_fops;
	my_cdev->owner = THIS_MODULE;
	ret = cdev_add(my_cdev, my_dev_id, 1);
	if (ret)
	{
      printk(KERN_ERR "failed to add cdev\n");
		goto fail_2;
	}
   printk(KERN_INFO "cdev added\n");
   printk(KERN_INFO "Hello world\n");

   return 0;

   fail_2:
      device_destroy(my_class, my_dev_id);
   fail_1:
      class_destroy(my_class);
   fail_0:
      unregister_chrdev_region(my_dev_id, 1);
   return -1;
}

static void __exit stred_exit(void)
{
   cdev_del(my_cdev);
   device_destroy(my_class, my_dev_id);
   class_destroy(my_class);
   unregister_chrdev_region(my_dev_id,1);
   printk(KERN_INFO "Goodbye, cruel world\n");
}


module_init(stred_init);
module_exit(stred_exit);
