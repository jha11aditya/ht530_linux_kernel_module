/**
 * @file   ht530.c
 * @author Aditya  Jha
 * @date   5 Dec 2020
 * @version 0
 * @brief   Accessing a kernel hash table via devicefile interface
 */

#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <linux/uaccess.h>          // Required for the copy to user function
#include <linux/mutex.h>	         /// Required for the mutex functionality
#include <linux/hashtable.h>        /// hash table api in linux
#include <linux/types.h>            // u32  and other d.types etc.
#include <linux/wait.h>             // wait funcs for mutex locks

#define  DEVICE_NAME "ht530"    ///< The device will appear at /dev/ht530 using this value
#define  CLASS_NAME  "ht"        ///< The device class -- this is a character device driver
#define  bits  8                 /// 2^8 = 256 buckets in the hash table created below
#define DUMP _IOWR('d','d',int32_t*)   /// ioctl number for implementining dump cmd via the same


MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Aditya  Jha");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A simple Linux char driver for Hash Table");  ///< The description -- see modinfo
MODULE_VERSION("0.1");            ///< A version number to inform users



static int    majorNumber;                  ///< Stores the device number -- determined automatically
static char   message[256] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message;              ///< Used to remember the size of the string stored
static int    numberOpens = 0;              ///< Counts the number of times the device is opened
static struct class*  ht530Class  = NULL; ///< The device-driver class struct pointer
static struct device* ht530Device = NULL; ///< The device-driver device struct pointer


struct ht {    // hash table entry struct for user space 
int key;
int data;
};

struct ht_entry {    // hash table entry struct for kernel inplementation
int key;
int data;
struct hlist_node node;
};

struct dump_arg      // dump argument struct for ioctl dump
{
   int n;// the n-th bucket(in) or n objects retrieved (out)
   struct ht object_array[8];// to retrieve at most 8 objects from the n-th bucket
};

static DEFINE_HASHTABLE(ht530_tbl, bits);    //Define new hash table

static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static long dev_ioctl(struct file *, unsigned int , unsigned long );    // special i/o func 

static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
   .unlocked_ioctl	= dev_ioctl,
};




static DEFINE_MUTEX(ht530_mutex);  /// mutex lock to serialize and isolate multiple device accesses by user space processes/threads
                                    

DECLARE_WAIT_QUEUE_HEAD(ht530_wait_q);    /// wait queue for user processes trying to access this device while busy
wait_queue_head_t ht530_qhead;   /// head pointer of the wait queue


static int __init ht530_init(void){
   printk(KERN_INFO "ht530: Initializing the ht530 LKM\n");

   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "ht530 failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "ht530: registered correctly with major number %d\n", majorNumber);

   // Register the device class
   ht530Class = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(ht530Class)){                // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(ht530Class);          // Correct way to return an error on a pointer
   }
   printk(KERN_INFO "ht530: device class registered correctly\n");

   // Register the device driver
   ht530Device = device_create(ht530Class, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(ht530Device)){               // Clean up if there is an error
      class_destroy(ht530Class);           // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to create the device\n");
      return PTR_ERR(ht530Device);
   }
   printk(KERN_INFO "ht530: device class created correctly\n"); // Made it! device was initialized
   mutex_init(&ht530_mutex);

   
   hash_init(ht530_tbl); // Initialize hash table
   printk(KERN_INFO "ht530: ht530_tbl hash table created correctly\n"); //  table  initialized

init_waitqueue_head(&ht530_qhead);  // Initialize the wait queue


   return 0;
}


static void __exit ht530_exit(void){
   mutex_destroy(&ht530_mutex);        /// destroy the dynamically-allocated mutex
   
   device_destroy(ht530Class, MKDEV(majorNumber, 0));     // remove the device
   class_unregister(ht530Class);                          // unregister the device class
   class_destroy(ht530Class);                             // remove the device class
   unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
   
   //Deleting the FULL Hash Table
   struct ht_entry * curr;
   int bkt=0;
   hash_for_each(ht530_tbl, bkt, curr,  node){
      hash_del(&curr->node); 
      printk(KERN_INFO "ht530: DELETE ht530_tbl key=[%d]  data=[%d] is in bucket\n", curr->key , curr->data);
   }
   
   printk(KERN_INFO "ht530: Goodbye from the ht530 LKM!\n");
}


static int dev_open(struct inode *inodep, struct file *filep){

   // if(!mutex_trylock(&ht530_mutex)){    /// Try to acquire the mutex (i.e., put the lock on/down)
   //                                        /// returns 1 if successful and 0 if there is contention
   //    printk(KERN_ALERT "ht530: Device in use by another process");
   //    return -EBUSY;
   // }
   // while(!mutex_trylock(&ht530_mutex));

   wait_event_interruptible(ht530_qhead,mutex_trylock(&ht530_mutex)==1); /// put user processes in wait queue if the device is busy (mutex_lock == 0)

   numberOpens++;
   printk(KERN_INFO "ht530: Device has been opened %d time(s)\n", numberOpens);
   return 0;
}












static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int error_count = 0;
   char* msg;
   struct ht_entry * curr;
   struct hlist_node * tmp; 

   int bkt=0;
   struct ht ht_msg;

   // Get and Cast back ht struct from buffer pntr
   copy_from_user(message, buffer, sizeof(struct ht));
   struct ht* t = (struct ht*) message;
   printk(KERN_INFO "ht530: This is the key to search: [%d]\n", t->key );

   bool chk_fnd = 0;
   // Search hash table by key of passed ht pntr
   hash_for_each_possible(ht530_tbl, curr,  node, t->key){
   printk(KERN_INFO "ht530: FOUND-SRCH ht530_tbl key=[%d]  data=[%d] is in bucket\n", curr->key , curr->data);
   ht_msg.key = curr->key;
   ht_msg.data = curr->data;
   chk_fnd = 1; // if entry found
   }
   if(chk_fnd == 0){
      msg = "-1";
   } else {
   // cast found ht object
   msg = (char*) &ht_msg;
   }
   // printk(KERN_INFO "ht530: ZZ-ht ht530_tbl key=[%d]  data=[%d] is in bucket\n", ht_msg.key , ht_msg.data);

   
//    hash_for_each(ht530_tbl, bkt, curr,  node){
//    printk(KERN_INFO "ht530: RD-allSRCH ht530_tbl key=[%d]  data=[%d] is in bucket=[%d]\n", curr->key , curr->data, bkt);
//    }

//   hlist_for_each_entry_safe(curr, tmp, &ht530_tbl[251], node){
//       printk(KERN_INFO "ht530: [251]RD-allSRCH ht530_tbl key=[%d]  data=[%d] is in bucket \n", curr->key , curr->data);  
//   }
  
   error_count = copy_to_user(buffer, msg, sizeof(struct ht)); // copy back the result in the same ht struct pointed by buffer

   if (error_count==0){            // if true then have success
      printk(KERN_INFO "ht530: Sent %ld characters to the user\n", sizeof(struct ht));
      if( chk_fnd == 0){return EINVAL;} // return EINVAL if not found
      return (size_of_message=0);  // clear the position to the start and return 0

   }
   else {
      printk(KERN_INFO "ht530: Failed to send %d characters to the user\n", error_count);
      return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
   }
}
















static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
   copy_from_user(message, buffer, sizeof(struct ht));   // appending received string with its length 
   // size_of_message = strlen(message);                 // store the length of the stored message
   struct ht* hep = (struct ht*) message;
   int bkt=0;

   
   printk(KERN_INFO "ht530: Received key: %d data: %d from the user\n", hep->key, hep->data );
   
   // struct ht_entry * htea = (struct ht_entry *)vmalloc(sizeof(struct ht_entry));
   // htea->key = 123;
   // htea->data = 118;
   // hash_add(ht530_tbl, &htea->node, htea->key);
  
   // struct ht_entry * hteb = (struct ht_entry *)vmalloc(sizeof(struct ht_entry));
   // hteb->key = 123;
   // hteb->data = 119;
   // hash_add(ht530_tbl, &hteb->node, hteb->key);

   // struct ht_entry * htec = (struct ht_entry *)vmalloc(sizeof(struct ht_entry));
   // htec->key = 124;
   // htec->data = 120;
   // hash_add(ht530_tbl, &htec->node, htec->key);

   // struct ht_entry hte2;
   // hte2.key = hep->key+1;
   // hte2.data = 789;
   // hash_add(ht530_tbl, &hte2.node, hte2.key);

   struct ht_entry * curr;
   struct hlist_node * tmp; 
   bool chk_replace=0;


   if(hep->data == 0){
      hash_for_each_possible_safe(ht530_tbl, curr, tmp,  node, hep->key){
      printk(KERN_INFO "ht530: DELETE key=[%d]  data=[%d]  \n", curr->key , curr->data);
      hash_del(&curr->node);
      }

   } else { //Non-Zero Data Field
      chk_replace = 0;
      // If there exist any entry with the same key than data is replaced
      hash_for_each_possible_safe(ht530_tbl, curr, tmp,  node, hep->key){
      if(curr->key == hep->key){
         curr->data = hep->data;
         chk_replace = 1;
         printk(KERN_INFO "ht530: REPLACE ht530_tbl key=[%d]  data=[%d] \n", curr->key , curr->data);

      }
      }
      // Else a new entry is created dynamically and chained to one of the hash table bucket
      if(chk_replace == 0){
         struct ht_entry * hte = (struct ht_entry *)vmalloc(sizeof(struct ht_entry));
         hte->key = hep->key;
         hte->data = hep->data;
         hash_add(ht530_tbl, &hte->node, hte->key);
         printk(KERN_INFO "ht530: ADD ht530_tbl key=[%d]  data=[%d] \n", hte->key , hte->data);

      }
      

   }


   // hash_for_each(ht530_tbl, bkt, curr,  node){
   //    printk(KERN_INFO "ht530: AFTER_wr_ops ht530_tbl key=[%d]  data=[%d] is in bucket=[%d]\n", curr->key , curr->data, bkt);
   // }
   
   return len;
}







static long dev_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param){
   int error_count=0;
   char* mp = (char*)ioctl_param;
   char msg[70];
   copy_from_user(msg, mp, sizeof(struct dump_arg));
   struct dump_arg* db = (struct dump_arg*) msg;

   struct ht_entry * curr;
   struct hlist_node * tmp; 
   char* pdb;
   bool out_ran = 0;

   int ind;
   for( ind=0;ind<8;ind++){
      db->object_array[ind].key = -1;
      db->object_array[ind].data = -1;
     
   }

   if( ioctl_num == DUMP){
      printk(KERN_INFO "ht530: IOCTL-DUMP function ioctl_num=[%u]", ioctl_num);
      printk(KERN_INFO "ht530: IOCTL-DUMP this bucket n=[%d]", db->n);

      int htind = 0;
      if(db->n >=0 && db->n <= 255){
         hlist_for_each_entry_safe(curr, tmp, &ht530_tbl[db->n], node){
               
               printk(KERN_INFO "ht530: IOCTL-DUMP bucket=[%d] key=[%d] data=[%d] \n", db->n ,curr->key , curr->data);
               if(htind <= 7)
                  {
                     db->object_array[htind].key = curr->key;
                     db->object_array[htind].data = curr->data;
                     htind++;
                  }  
                  
               hash_del(&curr->node);
               
        }
       pdb = (char*)db;

      } else { // n is OUT of range
       pdb = "-1";   
       out_ran = 1;
      }

   
   // char* objs8 = (char*) db->object_array;
   // struct ht * dmp8 = (struct ht *) objs8;
   // for( i=0;i<8;i++){
   //    printk(KERN_INFO "ht530: IOCTL-DUMP 888 key=[%d] data=[%d] \n", dmp8[i].key ,dmp8[i].data);
   // }

   
   error_count = copy_to_user(mp, pdb, sizeof(struct dump_arg));



   if (error_count==0){            // if true then have success
      printk(KERN_INFO "ht530: IOCTL-DUMP Copied %ld characters to the user\n", sizeof(struct dump_arg));
      if(out_ran == 1){return EINVAL;}
      return (size_of_message=0);  // clear the position to the start and return 0
   }
   else {
      printk(KERN_INFO "ht530: IOCTL-DUMP Failed to send %d characters to the user\n", error_count);
      return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
   }

         
   }


return 0;
}














static int dev_release(struct inode *inodep, struct file *filep){
   mutex_unlock(&ht530_mutex);          /// Releases the mutex (i.e., the lock goes up)
   printk(KERN_INFO "ht530: Device successfully closed\n");
   wake_up_interruptible(&ht530_qhead);  ///Wake up the waiting processes in the  ht530_wait_q wait queue
   return 0;
}


module_init(ht530_init);
module_exit(ht530_exit);