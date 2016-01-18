#include <stdio.h>  
#include <sys/types.h>  
#include <sys/ipc.h>  
#include <sys/msg.h>  
#include <errno.h>  
#include<stdlib.h>
#include<string.h>
  

#include <stdio.h>                                                                                            
#include<stdlib.h>
  
#define MSGKEY 1024  
  
//struct msgstru  
//{  
//   long msgtype;  
//   char msgtext[2048];   
//  
//};  
  
struct msgstru  
{  
   int  categary;  
   char version[2048];  
  
};  

main()  
{  
  int imsgID;  
  struct msgstru slQMsg,slRcvMsg;  

  int ilrc;  

  int  cat;
  char str[2048];  
 while(1){ 
  printf("enter the categary: 1 2 3 4 5\n");  
  scanf ("%d", &cat);  

  printf("enter the version: 1.0.0\n");  
  scanf ("%s", str);  
 
  //msgctl(imsgID,IPC_RMID,0);

 
  imsgID=msgget(MSGKEY,IPC_EXCL );  //检查消息队列是否存在  
  if(imsgID < 0){  
    imsgID=msgget(MSGKEY,IPC_CREAT|0666);//创建消息队列  
    if(imsgID <0 ){  
    printf("创建消息失败! errno=%d [%s]\n",errno,strerror(errno));  
    exit(-1);  
    }  
  }else{
	printf("msg quene has already exsit\n");	
	}    
  
  slQMsg.categary = cat;  
  strcpy(slQMsg.version, str);  
//  ilrc = msgsnd(imsgID,&slQMsg,sizeof(struct msgstru),IPC_NOWAIT); //发送消息队列  
  ilrc = msgsnd(imsgID,&slQMsg,sizeof(struct msgstru),IPC_NOWAIT); //发送消息队列  
  if ( ilrc < 0 ) {  
       printf("msgsnd()写消息队列失败,errno=%d[%s]\n",errno,strerror(errno));  
       exit(-1);  
  
  }else{
	   printf("消息队列发送成功\n");
  }
}
 //free(MSG);
 return 0; 
}  
