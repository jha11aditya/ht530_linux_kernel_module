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
#include <pthread.h> 

#define BUFFER_LENGTH 256               ///< The buffer length (crude but fine)
static char receive[BUFFER_LENGTH];     ///< The receive buffer from the LKM
 
#define DUMP _IOWR('d','d',int32_t*)





struct ht {
int key;
int data;
};

struct dump_arg 
{
   int n;// the n-th bucket(in) or n objects retrieved (out)
   struct ht object_array[8];// to retrieve at most 8 objects from the n-th bucket
};




void * htbl_actions(void* fdp){
   int fd = *(int*)fdp;
   int ret = 0;
   printf("thread\n");

for(int i=0;i<200;i++){ // Insert loop for hash table
   // printf("[%d]\n",rand());
   struct ht he1;
   he1.key = rand()%100;
   he1.data = rand()%100;
   char* stp = (char*) &he1;
   printf("Writing message to the device [%s].\n", stp);
   ret = write(fd, stp, strlen(stp)); // Send the string to the ht530 LKM

   if (ret < 0){
      perror("Failed to write the message to the device.");
      // return errno;
   }

}

for(int i=0;i<20000;i++){ // Search loop for hash table

 printf("Reading from the device...\n");
   struct ht t1;
   t1.key = rand()%100;
   char * rcv = (char*) &t1;
   ret = read(fd, rcv, BUFFER_LENGTH);        // Read the response from the LKM
   if (ret < 0){
      perror("Failed to read the message from the device.");
      // return errno;
   }
   else if(ret == EINVAL){
      perror("EINVAL Invalid Argument.");
   }
   else   {
      struct ht* rcvht = (struct ht*) rcv; 
      printf("The received message is: key[%d] data[%d]\n", rcvht->key, rcvht->data);
      printf("End of the program\n");
   }
}

for(int i=0;i<200;i++){
   int dmp_bckt = rand()%512;

   printf("ioctl DUMP bucket [%d]\n", dmp_bckt);
   struct dump_arg d;
   d.n = dmp_bckt;
   unsigned long iomsg = (unsigned long) &d;
   ret = ioctl(fd, DUMP, iomsg);
   if (ret < 0){
      perror("Failed to read the message from the device.");
      // return errno;
   }  else if(ret == EINVAL){
      perror("EINVAL Invalid Argument.");
   } else {

      struct ht* dmp8 = (struct ht*) &iomsg;
      for(int i=0;i<8;i++){
         printf("key=[%d] data=[%d]\n",d.object_array[i].key, d.object_array[i].data);
      }
   }

}

}




int main(){
  
   srand(1);

  


   int ret;
   int fd=0;
   printf("Starting ht530 device test code example...\n");
   fd = open("/dev/ht530", O_RDWR); // Open the device with read/write access
   if (fd < 0){
      perror("Failed to open the device...");
      return errno;
   }


   pthread_t th1,th2,th3,th4;

   pthread_create(&th1, NULL, htbl_actions, (void*)&fd);
   pthread_create(&th2, NULL, htbl_actions, (void*)&fd);
   pthread_create(&th3, NULL, htbl_actions, (void*)&fd);
   pthread_create(&th4, NULL, htbl_actions, (void*)&fd);

   pthread_join(th1, NULL);
   pthread_join(th2, NULL);
   pthread_join(th3, NULL);
   pthread_join(th4, NULL);


   // printf("Press Enter to send to the kernel module:\n");
   // getchar();   
   // struct ht he1;
   // he1.key = 123;
   // he1.data = 123321;
   // char* stp = (char*) &he1;
   // printf("Writing message to the device [%s].\n", stp);

   // ret = write(fd, stp, strlen(stp)); // Send the string to the LKM
   
   // // he1.key = 380;
   // // he1.data = 3790;
   // // ret = write(fd, stp, strlen(stp)); // Send the string to the LKM

   // // he1.key = 909;
   // // he1.data = 9908;
   // // ret = write(fd, stp, strlen(stp)); // Send the string to the LKM


   // if (ret < 0){
   //    perror("Failed to write the message to the device.");
   //    return errno;
   // }

   // printf("Press ENTER to read back from the device...\n");
   // getchar();

   // printf("Reading from the device...\n");
   // struct ht t1;
   // t1.key = 123;
   // char * rcv = (char*) &t1;
   // ret = read(fd, rcv, BUFFER_LENGTH);        // Read the response from the LKM
   // if (ret < 0){
   //    perror("Failed to read the message from the device.");
   //    return errno;
   // }
   // struct ht* rcvht = (struct ht*) rcv; 
   // printf("The received message is: key[%d] data[%d]\n", rcvht->key, rcvht->data);
   // printf("End of the program\n");

   // printf("ioctl DUMP [%ld]\n", DUMP);


   // struct dump_arg d;
   // d.n = 251;
   // unsigned long iomsg = (unsigned long) &d;
   // ioctl(fd, DUMP, iomsg);
   // struct ht* dmp8 = (struct ht*) &iomsg;
   // for(int i=0;i<8;i++){
   //    printf("key=[%d] data=[%d]\n",d.object_array[i].key, d.object_array[i].data);
   // }

   return 0;
}