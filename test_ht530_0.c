/**
 * @file   test_ht530.c
 * @author Aditya  Jha
 * @date   5 Dec 2020
 * @version 0
 * @brief   User Space Test Accessing akernel hash table via devicefile interface
 */




#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<sys/ioctl.h>
#include<sys/types.h>
#include <setjmp.h>
#include <signal.h>
#include <setjmp.h>

#define BUFFER_LENGTH 256               ///< The buffer length (crude but fine)
static char receive[BUFFER_LENGTH];     ///< The receive buffer from the LKM
 
#define DUMP _IOWR('d','d',int32_t*)

sigjmp_buf point;



struct ht {
int key;
int data;
};

struct dump_arg 
{
   int n;// the n-th bucket(in) or n objects retrieved (out)
   struct ht object_array[8];// to retrieve at most 8 objects from the n-th bucket
};



void handler(int sig, siginfo_t *dont_care, void *dont_care_either) { 
   printf("The interrupt signal is (%d)\n",sig);
   longjmp(point, 1);
//    exit(signal_num);   
}



int main(){
   struct sigaction sa;

   memset(&sa, 0, sizeof(struct sigaction));
   sigemptyset(&sa.sa_mask);

   sa.sa_flags     = SA_NODEFER;
   sa.sa_sigaction = handler;

   sigaction(026, &sa, NULL);

   srand(1);
printf("[%d]",-EBUSY);
   int ret;
   int fd=0;
   printf("Starting device test code example...\n");
   // if(setjmp(point)==0){
      fd = open("/dev/ht530", O_RDWR); // Open the device with read/write access
   // }
   printf("fd[%d]\n",fd);
   if (fd < 0){
      perror("Failed to open the device...");
      return errno;
   }
   printf("Press Enter to send to the kernel module:\n");
   getchar();   
   struct ht he1;
   he1.key = 123;
   he1.data = 123321;
   char* stp = (char*) &he1;
   printf("Writing message to the device [%s].\n", stp);

   ret = write(fd, stp, strlen(stp)); // Send the string to the LKM
   
   // he1.key = 380;
   // he1.data = 3790;
   // ret = write(fd, stp, strlen(stp)); // Send the string to the LKM

   // he1.key = 909;
   // he1.data = 9908;
   // ret = write(fd, stp, strlen(stp)); // Send the string to the LKM


   if (ret < 0){
      perror("Failed to write the message to the device.");
      return errno;
   }

   printf("Press ENTER to read back from the device...\n");
   getchar();

   printf("Reading from the device...\n");
   struct ht t1;
   t1.key = 123;
   char * rcv = (char*) &t1;
   ret = read(fd, rcv, BUFFER_LENGTH);        // Read the response from the LKM
   if (ret < 0){
      perror("Failed to read the message from the device.");
      return errno;
   }
   struct ht* rcvht = (struct ht*) rcv; 
   printf("The received message is: key[%d] data[%d]\n", rcvht->key, rcvht->data);
   printf("End of the program\n");

   printf("ioctl DUMP [%ld]\n", DUMP);


   struct dump_arg d;
   d.n = 251;
   unsigned long iomsg = (unsigned long) &d;
   ioctl(fd, DUMP, iomsg);
   struct ht* dmp8 = (struct ht*) &iomsg;
   for(int i=0;i<8;i++){
      printf("key=[%d] data=[%d]\n",d.object_array[i].key, d.object_array[i].data);
   }

   return 0;
}