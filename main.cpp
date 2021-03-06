#include <unistd.h>
#include <sys/inotify.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include "json/json.h"
#include <set>  
#include <mysql/mysql.h>
#include <pthread.h>  
#include <sys/types.h>  
#include <sys/ipc.h>  
#include <sys/msg.h>  

#include <algorithm>
#include<vector>
#include<string>

#define MSGKEY 1024  
#define MAX_BUF_SIZE 1024 // 缓冲区最大字节数

using namespace std;

set<string> mm, nn;
set<string>::iterator it_w, it_b;

set<string> mm_md5, nn_md5;
set<string>::iterator it_w_md5, it_b_md5;

set<string> mm_url, nn_url;
set<string>::iterator it_w_url, it_b_url;

set<string> mm_dns, nn_dns;
set<string>::iterator it_w_dns, it_b_dns;

pthread_mutex_t mutex;

MYSQL_RES 	*g_res; // mysql 记录集
MYSQL		*g_conn; // mysql 连接
MYSQL_ROW 	g_row; // 字符串数组，mysql 记录行
const char	*g_host_name = "192.168.10.112";
const char	*g_user_name = "root";
const char	*g_password = "123456";
const char	*g_db_name = "test_c";
const unsigned int g_db_port = 3306;


//该结构是状态节点,只负责记录当前节点相关的信息
class Node{
public:
 char whichCauseToThisStatus;//哪一个动作导致了当前节点的出现
 int NodeStatus;//当前节点状态
 int previewNodeStatus;//前一节点状态
 bool isEndStatus;//是否是终止状态
 string maxChildString;//该节点串中前面除去第一个字符的最大子串
 string outputStringOfThisStatus;//如果当前节点是终止节点的话，当前节点应该要输出的内容
 int whereToGoWhenFailed;//当失配的时候的跳转状态
public:
 Node(){//构造函数
  setNode(0, -1, false, "", "", -1);
 }
 Node(int NS, int PRS, bool IES, string MCS, string OSOTS, int WTGWF){//构造函数
  setNode(NS,PRS,IES,MCS,OSOTS,WTGWF);
 }
 void setNode(int NS, int PRS, bool IES, string MCS, string OSOTS, int WTGWF){
  NodeStatus = NS;//默认值为根节点
  previewNodeStatus = PRS;//默认前一个节点的状态是根节点
  isEndStatus = IES;//默认节点不是终止状态
  maxChildString = MCS;//默认最大子串为空
  outputStringOfThisStatus = OSOTS;//当前是输出节点的输出结果
  whereToGoWhenFailed = WTGWF;//默认跳转是为根节点
 }
 friend ostream& operator<<(ostream&cout, Node&n){

 // cout << "哪一个字符导致了当前的节点:" << n.whichCauseToThisStatus << endl;
 // cout << "当前节点状态:" << n.NodeStatus << endl;
 // cout << "前一个节点状态:" << n.previewNodeStatus << endl;
 // cout << "当前节点是否是终止状态:" << n.isEndStatus << endl;
 // cout << "最大子串:" << n.maxChildString << endl;
 // cout << "如果是终止状态的输出字符串:" << n.outputStringOfThisStatus << endl;
 // cout << "失配的时候应该要回溯到哪里:" << n.whereToGoWhenFailed << endl;
  return cout;
 }
};

//该结构是每个当前状态indexNode的一次操作结果，可以认为是领域关系, 
//重要的是要判断该状态下是否存在某操作的结果，如果不存在就添加进去，存在的话就切换当前状态为下一个状态
class indexNode{
public:
 int indexStatus;//当前状态
 char ch;//一次操作
 int nextStatusOfIndexStatus;//操作结果转移到的状态
public:
 indexNode(){
  indexStatus = 0;
  ch = '~';
  nextStatusOfIndexStatus = 0;
 }
 indexNode(int a, char b, int c){
  indexStatus = a;
  ch = b;
  nextStatusOfIndexStatus = c;
 }
 void set(int a, char b, int c){
  indexStatus = a;
  ch = b;
  nextStatusOfIndexStatus = c;
 }
 bool operator==(const indexNode&iN){
  return indexStatus == iN.indexStatus&&ch == iN.ch;
 }
 bool operator!=(const indexNode&iN){
  return indexStatus != iN.indexStatus&&ch != iN.ch;
 }
};

//ac算法类
class AC{
public:
 vector<Node>vecNode;//vecNode向量存储了各个状态的信息
 vector<string>vecString;//vecString存储了模式串集合
 vector<indexNode>vecIndexNode;//存储了状态机的vector,现在的问题是要能够保证能够访问到该vector里面的每一个重复状态，可以使用find函数进行查找
public:
 AC( int tag){

	vecNode.clear();
 	vecString.clear();
 	vecIndexNode.clear();


  //完成模式串集合的输入
  vecNode.reserve(100);
  vecString.reserve(20);
  //string s = "";
  //cout << "请输入模式串集合:";
  //while (1){
  // cin >> s;
  // if (s == "end" || s == "END"){
  //  break;
  // }
  // vecString.push_back(s);
  //}

//mysql


	string line;
    char sql[MAX_BUF_SIZE];


//	mm_url.clear();
//	nn_url.clear();

    // init the database connection
    g_conn = mysql_init(NULL);

    /* connect the database */
    if(!mysql_real_connect(g_conn, g_host_name, g_user_name, g_password, g_db_name, g_db_port, NULL, 0)) // 如果失败
    {
		perror("mysql connect: ");
		//return -1;
	}

    // 是否连接已经可用
	
    sprintf(sql, "set names utf8");
    if (mysql_real_query(g_conn, sql, strlen(sql))) // 如果失败
    {
		perror("test error: ");
		//return -1;
	}

	if(tag == 1)
    	sprintf(sql, "select * from `d_URL` where type = 1 order by id");
	else if(tag == 0)
    	sprintf(sql, "select * from `d_URL` where type = 0 order by id");
		
    if (mysql_real_query(g_conn, sql, strlen(sql))) // 如果失败
    {
		perror("execute error: ");
		//return -1;
	}


    g_res = mysql_store_result(g_conn); // 从服务器传送结果集至本地，mysql_use_result直接使用服务器上的记录集

    int iNum_rows = mysql_num_rows(g_res); // 得到记录的行数
    int iNum_fields = mysql_num_fields(g_res); // 得到记录的列数
	
    while ((g_row=mysql_fetch_row(g_res))) // 打印结果集
	{
		line=g_row[2];
		if(strcmp(g_row[1],"1"))
		{
        //	mm_url.insert(line);
   			vecString.push_back(line);
		}
		else if(!strcmp(g_row[1],"1"))
		{
        //	nn_url.insert(line);
   			vecString.push_back(line);
		}
	}

    mysql_free_result(g_res); // 释放结果集
    mysql_close(g_conn); // 关闭链接

	//show alll white list and black list

//	it_w_url=mm_url.begin();                                                                             
//	printf("-----------white-----------\n");
//	printf("---------------------------\n");
//	while(it_w_url!=mm_url.end())
//	{
//      cout<< *it_w_url <<endl;
//      ++it_w_url;
//	}
//  
//	it_b_url=nn_url.begin();                                                                             
//	printf("-----------black-----------\n");
//	printf("---------------------------\n");
//	while(it_b_url!=nn_url.end())
//	{
//      cout<< *it_b_url <<endl;
//      ++it_b_url;
//	}

//mysql

  // //输出结果
   int k = 0;
   for (vector<string>::iterator i = vecString.begin(); i != vecString.end(); i++)
	{
    cout<<vecString[k]<<endl;
    k++;
   }

  //AC算法需要处理一下，每个模式串，从而确定下状态树
  Node*p = NULL;//p用来新建一个状态
  string temp;//临时string
  int status = 0;//当前状态为0,status只能加加,该状态是新建的状态
  int tempchar = 0;//对应于一个模式串处理到了哪一个位置
  int jumpStatus = 0;//跳转状态
  vector<indexNode>::iterator VINI;
  //存储0状态
  vecNode.push_back(Node());
  for (vector<string>::iterator i = vecString.begin(); i != vecString.end(); i++){//对于每一个模式串

   temp = *i;
   jumpStatus = 0;//对于每一个模式串的处理都是从状态0开始跳转
   tempchar = 0;//当前处理的位置是0
   for (string::iterator j = temp.begin(); j != temp.end(); j++,tempchar++){//对于每一个模式串开始处理其字符

    //如果在已经建立好了的表里面查找到了相应的字符
    if ((VINI=find(vecIndexNode.begin(), vecIndexNode.end(), indexNode(jumpStatus, *j, 0)))!=vecIndexNode.end()){

     jumpStatus = VINI->nextStatusOfIndexStatus;
     if (j + 1 == temp.end()){//如果当前字符是最后一个字符的话而且被在串中查找到了的话,就要更改此节点的信息
      vecNode[jumpStatus].isEndStatus = true;
      vecNode[jumpStatus].outputStringOfThisStatus = temp;
     }
    }
    else{

     //否则的话
     status++;//第一次的时候，新建的一个状态为1
     //新建出一个节点状态
     p = new Node();
     p->whichCauseToThisStatus = *j;//导致当前节点的字符的赋值
     if (j + 1 == temp.end()){//如果当前字符是最后一个字符的话，就需要设置输出字符串
      p->setNode(status, jumpStatus, true, temp.substr(1, tempchar), temp, 0);
     }
     else{//否则的话不需要设置输出字符串
      p->setNode(status, jumpStatus, false, temp.substr(1, tempchar), "", 0);
     }
     //节点设置好了之后就添加进去
     vecNode.push_back(*p);

     //并且在vecIndexNode里面注册一个信息
     vecIndexNode.push_back(indexNode(jumpStatus,*j,p->NodeStatus));

     //添加完毕的话就修改当前的跳转状态
     jumpStatus = p->NodeStatus;
    }
   }
  }


  //状态树确定正确
  //AC算法需要处理一下fail状态下的wheretogowhenfail
  //for (vector<indexNode>::iterator i = vecIndexNode.begin(); i != vecIndexNode.end(); i++){
  //
  // if (i->indexStatus == 0){
  //  vecNode[i->nextStatusOfIndexStatus].whereToGoWhenFailed = 0;
  // }
  //}

  int toUseAsTmpStatus = 0;
  vector<indexNode>::iterator VI;
  for (int i = 1; i < vecNode.size(); i++){

   if (vecNode[i].previewNodeStatus != 0){
    if (vecNode[vecNode[i].previewNodeStatus].whereToGoWhenFailed == 0){//如果当前关系的前一个节点是最靠近0节点的节点的话
     VI = find(vecIndexNode.begin(), vecIndexNode.end(), indexNode(0, vecNode[i].whichCauseToThisStatus, 0));
     if (VI != vecIndexNode.end()){//说明找到了这样一个节点
      vecNode[i].whereToGoWhenFailed = VI->nextStatusOfIndexStatus;
     }
     else{//如果没有找到的话
      vecNode[i].whereToGoWhenFailed = 0;
     }
    }
    else{//如果不是最靠近0节点的话
     toUseAsTmpStatus = vecNode[vecNode[i].previewNodeStatus].whereToGoWhenFailed;
     VI = find(vecIndexNode.begin(), vecIndexNode.end(), indexNode(toUseAsTmpStatus, vecNode[i].whichCauseToThisStatus, 0));
     if (VI != vecIndexNode.end()){//如果存在

      vecNode[i].whereToGoWhenFailed = VI->nextStatusOfIndexStatus;
     }
     else{//如果不存在就要从0状态去查找
      VI = find(vecIndexNode.begin(), vecIndexNode.end(), indexNode(0, vecNode[i].whichCauseToThisStatus, 0));
      if (VI != vecIndexNode.end()){//找到了
       vecNode[i].whereToGoWhenFailed = VI->nextStatusOfIndexStatus;
      }
      else{//没找到
       vecNode[i].whereToGoWhenFailed = 0;
      }
     }
    }
   }
  }

  //含有跳转信息
  //输出状态树
  //cout << "=====================================================================\n状态树结果:" << endl;
  //for (vector<indexNode>::iterator i = vecIndexNode.begin(); i != vecIndexNode.end(); i++){
  //
  // cout << "(" << i->indexStatus << "," << i->ch << "," << i->nextStatusOfIndexStatus << ")" << endl;
  //}
  //cout << "=====================================================================\n状态树结果:" << endl;
  //for (vector<Node>::iterator i = vecNode.begin(); i != vecNode.end(); i++){
  //
  // cout << *i << endl;
  //}
   cout <<"Load Over"<<endl;
 }


void update( int tag){
  //完成模式串集合的输入
	vecNode.clear();//vecNode向量存储了各个状态的信息
 	vecString.clear();//vecString存储了模式串集合
 	vecIndexNode.clear();//存储了状态机的vector,现在的问题是要能够保证能够访问到该vector里面的每一个重复状态，可以使用find函数进行查找

  vecNode.reserve(100);
  vecString.reserve(20);
  //string s = "";
  //cout << "请输入模式串集合:";
  //while (1){
  // cin >> s;
  // if (s == "end" || s == "END"){
  //  break;
  // }
  // vecString.push_back(s);
  //}

//mysql


	string line;
    char sql[MAX_BUF_SIZE];


//	mm_url.clear();
//	nn_url.clear();

    // init the database connection
    g_conn = mysql_init(NULL);

    /* connect the database */
    if(!mysql_real_connect(g_conn, g_host_name, g_user_name, g_password, g_db_name, g_db_port, NULL, 0)) // 如果失败
    {
		perror("mysql connect: ");
		//return -1;
	}

    // 是否连接已经可用
	
    sprintf(sql, "set names utf8");
    if (mysql_real_query(g_conn, sql, strlen(sql))) // 如果失败
    {
		perror("test error: ");
		//return -1;
	}

	if(tag == 1)
    	sprintf(sql, "select * from `d_URL` where type = 1 order by id");
	else if(tag == 0)
    	sprintf(sql, "select * from `d_URL` where type = 0 order by id");
		
    if (mysql_real_query(g_conn, sql, strlen(sql))) // 如果失败
    {
		perror("execute error: ");
		//return -1;
	}


    g_res = mysql_store_result(g_conn); // 从服务器传送结果集至本地，mysql_use_result直接使用服务器上的记录集

    int iNum_rows = mysql_num_rows(g_res); // 得到记录的行数
    int iNum_fields = mysql_num_fields(g_res); // 得到记录的列数
	
    while ((g_row=mysql_fetch_row(g_res))) // 打印结果集
	{
		line=g_row[2];
		if(strcmp(g_row[1],"1"))
		{
        //	mm_url.insert(line);
   			vecString.push_back(line);
		}
		else if(!strcmp(g_row[1],"1"))
		{
        //	nn_url.insert(line);
   			vecString.push_back(line);
		}
	}

    mysql_free_result(g_res); // 释放结果集
    mysql_close(g_conn); // 关闭链接

	//show alll white list and black list

//	it_w_url=mm_url.begin();                                                                             
//	printf("-----------white-----------\n");
//	printf("---------------------------\n");
//	while(it_w_url!=mm_url.end())
//	{
//      cout<< *it_w_url <<endl;
//      ++it_w_url;
//	}
//  
//	it_b_url=nn_url.begin();                                                                             
//	printf("-----------black-----------\n");
//	printf("---------------------------\n");
//	while(it_b_url!=nn_url.end())
//	{
//      cout<< *it_b_url <<endl;
//      ++it_b_url;
//	}

//mysql

  // //输出结果
   int k = 0;
   for (vector<string>::iterator i = vecString.begin(); i != vecString.end(); i++)
	{
    cout<<vecString[k]<<endl;
    k++;
   }

  //AC算法需要处理一下，每个模式串，从而确定下状态树
  Node*p = NULL;//p用来新建一个状态
  string temp;//临时string
  int status = 0;//当前状态为0,status只能加加,该状态是新建的状态
  int tempchar = 0;//对应于一个模式串处理到了哪一个位置
  int jumpStatus = 0;//跳转状态
  vector<indexNode>::iterator VINI;
  //存储0状态
  vecNode.push_back(Node());
  for (vector<string>::iterator i = vecString.begin(); i != vecString.end(); i++){//对于每一个模式串

   temp = *i;
   jumpStatus = 0;//对于每一个模式串的处理都是从状态0开始跳转
   tempchar = 0;//当前处理的位置是0
   for (string::iterator j = temp.begin(); j != temp.end(); j++,tempchar++){//对于每一个模式串开始处理其字符

    //如果在已经建立好了的表里面查找到了相应的字符
    if ((VINI=find(vecIndexNode.begin(), vecIndexNode.end(), indexNode(jumpStatus, *j, 0)))!=vecIndexNode.end()){

     jumpStatus = VINI->nextStatusOfIndexStatus;
     if (j + 1 == temp.end()){//如果当前字符是最后一个字符的话而且被在串中查找到了的话,就要更改此节点的信息
      vecNode[jumpStatus].isEndStatus = true;
      vecNode[jumpStatus].outputStringOfThisStatus = temp;
     }
    }
    else{

     //否则的话
     status++;//第一次的时候，新建的一个状态为1
     //新建出一个节点状态
     p = new Node();
     p->whichCauseToThisStatus = *j;//导致当前节点的字符的赋值
     if (j + 1 == temp.end()){//如果当前字符是最后一个字符的话，就需要设置输出字符串
      p->setNode(status, jumpStatus, true, temp.substr(1, tempchar), temp, 0);
     }
     else{//否则的话不需要设置输出字符串
      p->setNode(status, jumpStatus, false, temp.substr(1, tempchar), "", 0);
     }
     //节点设置好了之后就添加进去
     vecNode.push_back(*p);

     //并且在vecIndexNode里面注册一个信息
     vecIndexNode.push_back(indexNode(jumpStatus,*j,p->NodeStatus));

     //添加完毕的话就修改当前的跳转状态
     jumpStatus = p->NodeStatus;
    }
   }
  }


  //状态树确定正确
  //AC算法需要处理一下fail状态下的wheretogowhenfail
  //for (vector<indexNode>::iterator i = vecIndexNode.begin(); i != vecIndexNode.end(); i++){
  //
  // if (i->indexStatus == 0){
  //  vecNode[i->nextStatusOfIndexStatus].whereToGoWhenFailed = 0;
  // }
  //}

  int toUseAsTmpStatus = 0;
  vector<indexNode>::iterator VI;
  for (int i = 1; i < vecNode.size(); i++){

   if (vecNode[i].previewNodeStatus != 0){
    if (vecNode[vecNode[i].previewNodeStatus].whereToGoWhenFailed == 0){//如果当前关系的前一个节点是最靠近0节点的节点的话
     VI = find(vecIndexNode.begin(), vecIndexNode.end(), indexNode(0, vecNode[i].whichCauseToThisStatus, 0));
     if (VI != vecIndexNode.end()){//说明找到了这样一个节点
      vecNode[i].whereToGoWhenFailed = VI->nextStatusOfIndexStatus;
     }
     else{//如果没有找到的话
      vecNode[i].whereToGoWhenFailed = 0;
     }
    }
    else{//如果不是最靠近0节点的话
     toUseAsTmpStatus = vecNode[vecNode[i].previewNodeStatus].whereToGoWhenFailed;
     VI = find(vecIndexNode.begin(), vecIndexNode.end(), indexNode(toUseAsTmpStatus, vecNode[i].whichCauseToThisStatus, 0));
     if (VI != vecIndexNode.end()){//如果存在

      vecNode[i].whereToGoWhenFailed = VI->nextStatusOfIndexStatus;
     }
     else{//如果不存在就要从0状态去查找
      VI = find(vecIndexNode.begin(), vecIndexNode.end(), indexNode(0, vecNode[i].whichCauseToThisStatus, 0));
      if (VI != vecIndexNode.end()){//找到了
       vecNode[i].whereToGoWhenFailed = VI->nextStatusOfIndexStatus;
      }
      else{//没找到
       vecNode[i].whereToGoWhenFailed = 0;
      }
     }
    }
   }
  }

  //含有跳转信息
  //输出状态树
  //cout << "=====================================================================\n状态树结果:" << endl;
  //for (vector<indexNode>::iterator i = vecIndexNode.begin(); i != vecIndexNode.end(); i++){
  //
  // cout << "(" << i->indexStatus << "," << i->ch << "," << i->nextStatusOfIndexStatus << ")" << endl;
  //}
  //cout << "=====================================================================\n状态树结果:" << endl;
  //for (vector<Node>::iterator i = vecNode.begin(); i != vecNode.end(); i++){
  //
  // cout << *i << endl;
  //}
   cout <<"-=  UPDATE OVER  =-"<<endl;
 }


 //AC算法的处理已经完成
 //等待被处理字符串进行处理

void  checkMaxChildString(string s){//s是待检测模式串
														//若有匹配当即退出 返回1 ，若无匹配则 返回2

  vector<string>::iterator VII;
  for (int i = 1; i < s.size();i++){

   VII = find(vecString.begin(), vecString.end(), s.substr(i));
   if (VII != vecString.end()){//如果找到了的话，就输出
    cout << *VII << endl;
   }
  }
 }

 int  checkStrings(string text){//进行模式匹配,text是要被用来匹配的模式


cout <<"-------------DEBUG----0--------"<<endl;
   int k = 0;
   for (vector<string>::iterator i = vecString.begin(); i != vecString.end(); i++)
	{
    cout<<vecString[k]<<endl;
    k++;
   }

cout <<"-------------DEBUG----1--------"<<endl;

  vector<indexNode>::iterator VI;
  int jumpStatus = 0;
  for (string::iterator i = text.begin(); i != text.end();){

   VI = find(vecIndexNode.begin(), vecIndexNode.end(), indexNode(jumpStatus, *i, 0));//查找是否存在当前关系

   if (VI == vecIndexNode.end()){//如果不存在当前关系

    if (jumpStatus != 0){
     jumpStatus = vecNode[jumpStatus].whereToGoWhenFailed;//如果当前关系不存在的话，就转到当前状态的下一个跳转状态
    }
    else{
     i++;
    }
   }
   else{//如果存在当前关系,就要判断当前jumpStatus是否是终止状态如果是的话，就要输出该终止态应该要输出的结果
    
    jumpStatus = VI->nextStatusOfIndexStatus;
    if (vecNode[jumpStatus].isEndStatus){//如果该操作的下一个状态是一个终止状态，就要输出其结果

     cout << vecNode[jumpStatus].outputStringOfThisStatus << endl;
	 cout << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"<<endl;
	 return 1;
     //输出完毕之后还需要对该节点的最大子串进行检查,看最大子串是不是一个模式串,而不是对最大子串进行匹配处理
     checkMaxChildString(vecNode[jumpStatus].outputStringOfThisStatus);
    }
    //不管是不是终止状态都要字符指针向下移动
    i++;
   }
  }
  //cout << text << endl;
	return 2;
 }

};

// load msg url
AC	ac_w(0);
AC	ac_b(1);


AC load_msg_url_w()
{
	
	//printf("---------------------------\n");
	//printf("-     LOAD BLACK URL END  -\n");
	//printf("---------------------------\n");
	
	return ac_w;
}
	
AC load_msg_url_b()
{

	//printf("---------------------------\n");
	//printf("-     LOAD WRITE URL END  -\n");
	//printf("---------------------------\n");

	return ac_b;
}
	
int load_msg_md5()
{
	string line;
    char sql[MAX_BUF_SIZE];

	printf("---------------------------\n");
	printf("-        LOAD   MD5       -\n");
	printf("---------------------------\n");

	mm_md5.clear();
	nn_md5.clear();
    // init the database connection
    g_conn = mysql_init(NULL);

    /* connect the database */
    if(!mysql_real_connect(g_conn, g_host_name, g_user_name, g_password, g_db_name, g_db_port, NULL, 0)) // 如果失败
    {
		perror("mysql connect: ");
		return -1;
	}

    // 是否连接已经可用

    sprintf(sql, "set names utf8");
    if (mysql_real_query(g_conn, sql, strlen(sql))) // 如果失败
    {
		perror("test error: ");
		return -1;
	}

    sprintf(sql, "select * from `d_MD5` order by id");
    if (mysql_real_query(g_conn, sql, strlen(sql))) // 如果失败
    {
		perror("execute error: ");
		return -1;
	}


    g_res = mysql_store_result(g_conn); // 从服务器传送结果集至本地，mysql_use_result直接使用服务器上的记录集

    int iNum_rows = mysql_num_rows(g_res); // 得到记录的行数
    int iNum_fields = mysql_num_fields(g_res); // 得到记录的列数
	
    while ((g_row=mysql_fetch_row(g_res))) // 打印结果集
	{
		line=g_row[2];
		if(strcmp(g_row[1],"1"))
		{
        	mm_md5.insert(line);
		}
		else if(!strcmp(g_row[1],"1"))
		{
        	nn_md5.insert(line);
		}
	}

    mysql_free_result(g_res); // 释放结果集
    mysql_close(g_conn); // 关闭链接

	//show alll white list and black list
	it_w_md5=mm_md5.begin();                                                                             
	printf("-----------white-----------\n");
	printf("---------------------------\n");
	while(it_w_md5!=mm_md5.end())
	{
      cout<< *it_w_md5 <<endl;
      ++it_w_md5;
	}
  
	it_b_md5=nn_md5.begin();                                                                             
	printf("-----------black-----------\n");
	printf("---------------------------\n");
	while(it_b_md5!=nn_md5.end())
	{
      cout<< *it_b_md5 <<endl;
      ++it_b_md5;
	}

	printf("---------------------------\n");
	printf("-     LOAD  MD5  END      -\n");
	printf("---------------------------\n");

	return 0;
}



int load_msg_ip()
{
	string line;
    char sql[MAX_BUF_SIZE];

	printf("---------------------------\n");
	printf("-        LOAD  IP         -\n");
	printf("---------------------------\n");

	mm.clear();
	nn.clear();
    // init the database connection
    g_conn = mysql_init(NULL);

    /* connect the database */
    if(!mysql_real_connect(g_conn, g_host_name, g_user_name, g_password, g_db_name, g_db_port, NULL, 0)) // 如果失败
    {
		perror("mysql connect: ");
		return -1;
	}

    // 是否连接已经可用

    sprintf(sql, "set names utf8");
    if (mysql_real_query(g_conn, sql, strlen(sql))) // 如果失败
    {
		perror("test error: ");
		return -1;
	}

    sprintf(sql, "select * from `d_IP` order by id");
    if (mysql_real_query(g_conn, sql, strlen(sql))) // 如果失败
    {
		perror("execute error: ");
		return -1;
	}


    g_res = mysql_store_result(g_conn); // 从服务器传送结果集至本地，mysql_use_result直接使用服务器上的记录集

    int iNum_rows = mysql_num_rows(g_res); // 得到记录的行数
    int iNum_fields = mysql_num_fields(g_res); // 得到记录的列数
	
    while ((g_row=mysql_fetch_row(g_res))) // 打印结果集
	{
		line=g_row[2];
		if(strcmp(g_row[1],"1"))
		{
        	mm.insert(line);
		}
		else if(!strcmp(g_row[1],"1"))
		{
        	nn.insert(line);
		}
	}

    mysql_free_result(g_res); // 释放结果集
    mysql_close(g_conn); // 关闭链接

	//show alll white list and black list
	it_w=mm.begin();                                                                             
	printf("-----------white-----------\n");
	printf("---------------------------\n");
	while(it_w!=mm.end())
	{
      cout<< *it_w <<endl;
      ++it_w;
	}
  
	it_b=nn.begin();                                                                             
	printf("-----------black-----------\n");
	printf("---------------------------\n");
	while(it_b!=nn.end())
	{
      cout<< *it_b <<endl;
      ++it_b;
	}

	printf("---------------------------\n");
	printf("-       LOAD IP END       -\n");
	printf("---------------------------\n");

	return 0;
}


//load url from  database and rebuilt the AC tree;


int load_msg_dns()
{
	string line;
    char sql[MAX_BUF_SIZE];

	printf("---------------------------\n");
	printf("-        LOAD   DNS       -\n");
	printf("---------------------------\n");

	mm_dns.clear();
	nn_dns.clear();
    // init the database connection
    g_conn = mysql_init(NULL);

    /* connect the database */
    if(!mysql_real_connect(g_conn, g_host_name, g_user_name, g_password, g_db_name, g_db_port, NULL, 0)) // 如果失败
    {
		perror("mysql connect: ");
		return -1;
	}

    // 是否连接已经可用

    sprintf(sql, "set names utf8");
    if (mysql_real_query(g_conn, sql, strlen(sql))) // 如果失败
    {
		perror("test error: ");
		return -1;
	}

    sprintf(sql, "select * from `d_DNS` order by id");
    if (mysql_real_query(g_conn, sql, strlen(sql))) // 如果失败
    {
		perror("execute error: ");
		return -1;
	}


    g_res = mysql_store_result(g_conn); // 从服务器传送结果集至本地，mysql_use_result直接使用服务器上的记录集

    int iNum_rows = mysql_num_rows(g_res); // 得到记录的行数
    int iNum_fields = mysql_num_fields(g_res); // 得到记录的列数
	
    while ((g_row=mysql_fetch_row(g_res))) // 打印结果集
	{
		line=g_row[2];
		if(strcmp(g_row[1],"1"))
		{
        	mm_dns.insert(line);
		}
		else if(!strcmp(g_row[1],"1"))
		{
        	nn_dns.insert(line);
		}
	}

    mysql_free_result(g_res); // 释放结果集
    mysql_close(g_conn); // 关闭链接

	//show alll white list and black list
	it_w_dns=mm_dns.begin();                                                                             
	printf("-----------white-----------\n");
	printf("---------------------------\n");
	while(it_w_dns!=mm_dns.end())
	{
      cout<< *it_w_dns <<endl;
      ++it_w_dns;
	}
  
	it_b_dns=nn_dns.begin();                                                                             
	printf("-----------black-----------\n");
	printf("---------------------------\n");
	while(it_b_dns!=nn_dns.end())
	{
      cout<< *it_b_dns <<endl;
      ++it_b_dns;
	}

	printf("---------------------------\n");
	printf("-       LOAD DNS END      -\n");
	printf("---------------------------\n");

	return 0;
}


int check_md5(string md5) 
{
	cout<<">>>   MD5 CHECK   <<<"<<endl;
	string strin = md5;

	it_w_md5 = mm_md5.find(strin);
	if(mm_md5.end() != it_w_md5)  // in white list
	{
		cout<<"match white"<<endl;
		cout<<">>>  MD5 CHECK END   <<<"<<endl;
		return 1;
	}
	else if(mm_md5.end() == it_w_md5)  // not in white list
	{
		cout << "not in white"<<endl;
		
		it_b_md5 = nn_md5.find(strin);
		if(nn_md5.end() != it_b_md5) // in black list 
		{
			cout<<"match black"<<endl;
			cout<<">>>  MD5 CHECK END   <<<"<<endl;
			return 2;
		}
		else if(nn_md5.end() == it_b_md5) // not in black list
		{
			cout << "not in black yet" << endl;
			cout<<">>>  MD5 CHECK END   <<<"<<endl;
			return -1;
		}
	}
}


int check_ip(string ip) 
{
	cout<<">>>   IP  CHECK   <<<"<<endl;
	string strin = ip;

	it_w = mm.find(strin);
	if(mm.end() != it_w)  // in white list
	{
		cout<<"match white"<<endl;
		cout<<">>>   IP CHECK END   <<<"<<endl;
		return 1;
	}
	else if(mm.end() == it_w)  // not in white list
	{
		cout << "not in white"<<endl;
		
		it_b = nn.find(strin);
		if(nn.end() != it_b) // in black list 
		{
			cout<<"match black"<<endl;
			cout<<">>>   IP CHECK END   <<<"<<endl;
			return 2;
		}
		else if(nn.end() == it_b) // not in black list
		{
			cout << "not in black yet" << endl;
			cout<<">>>   IP CHECK END   <<<"<<endl;
			return -1;
		}
	}
}

int check_url( string t)
{
	cout<<">>>   URL CHECK   <<<"<<endl;
 //cout<<"enter the strings text"<<endl;
 string url = t;
 //cin>>url;
 int ret_w =  ac_w.checkStrings(url);
 int ret_b =  ac_b.checkStrings(url);
 
 if(ret_w == 1 && ret_b == 2)
{
	cout<<"match white"<<endl;
 	cout<<">>>  URL CHECK END   <<<"<<endl;
	return 1;
}

 if(ret_w == 1 && ret_b == 1)
 {
 	cout<<"match black"<<endl;
 	cout<<">>>  URL CHECK END   <<<"<<endl;
 	return 2;
 }

 if(ret_w == 2 && ret_b == 1)
 {
 	cout<<"match black"<<endl;
 	cout<<">>>  URL CHECK END   <<<"<<endl;
 	return 2;
 }

 if(ret_w == 2 && ret_b == 2)
 {
 	cout << "not in black yet" << endl;
 	cout<<">>>  URL CHECK END   <<<"<<endl;
 	return -1;
 }

}


int check_dns(string dns) 
{
	cout<<">>>   DNS CHECK   <<<"<<endl;
	string strin = dns;

	it_w_dns = mm_dns.find(strin);
	if(mm_dns.end() != it_w_dns)  // in white list
	{
		cout<<"match white"<<endl;
		cout<<">>>  DNS CHECK END   <<<"<<endl;
		return 1;
	}
	else if(mm_dns.end() == it_w_dns)  // not in white list
	{
		cout << "not in white"<<endl;
		
		it_b_dns = nn_dns.find(strin);
		if(nn_dns.end() != it_b_dns) // in black list 
		{
			cout<<"match black"<<endl;
			cout<<">>>  DNS CHECK END   <<<"<<endl;
			return 2;
		}
		else if(nn_dns.end() == it_b_dns) // not in black list
		{
			cout << "not in black yet" << endl;
			cout<<">>>  DNS CHECK END   <<<"<<endl;
			return -1;
		}
	}
}

int check_av(const char *file, string &v)		// v - name of virus if have
{
	cout<<">>>   AV  CHECK   <<<"<<endl;

    FILE *fstream=NULL;     
    char buff[1024];   
	char *p;
	char *q;
	char *t;
    memset(buff,0,sizeof(buff));   
	//printf("%s", file);
	char cmd[1024]={0};

	strcpy(cmd, "/usr/bin/clamscan ");
	strcat(cmd, file);
	printf("cmd is => [%s]\n", cmd);
    //if(NULL==(fstream=popen("/usr/bin/clamscan /tmp/a.out","r")))     
    if(NULL==(fstream=popen(cmd,"r")))     
    {    
        fprintf(stderr,"execute command failed:");     
        return -1;     
    }    
    fgets(buff, sizeof(buff), fstream); 
	p=buff;
	t=strchr(p, '\n');
	if(t)
		*t='\0';
    printf("[%s]\n",buff);   
	q=strtok(p," ");
    q=strtok(NULL," ");
    //printf("[%s]---\n",q);
	if(!strcmp(q,"OK"))
	{
    	pclose(fstream);   
		v=q;
		cout<<">>>   AV CHECK END   <<<"<<endl;
		return 2;
	}else{
    	pclose(fstream);   
		v=q;
		cout<<">>>   AV CHECK END   <<<"<<endl;
		return 1;
	}
}

int check(vector<string> t, vector<string> & r)
{

	cout<<"---------------------"<<endl;
	cout<<"-   DO CHECK HERE   -"<<endl;
	cout<<"---------------------"<<endl;

	string check_dir="/home/SD/Data/DataDIR/"; //the path for clamAV check
	string virus;
	int i=0;	
	for(i=0; i<15; i++)		
	{
		r.push_back(t[i]);
	}
	r.push_back("0");
	r.push_back("0");
	r.push_back("0");

	int ret;
	ret=check_md5(t[13]);	
	if(ret == 1){
		r[15] = "md5";
		r[16] = "0";       // r[18]  infer leve of thread   0-non  1-low 2-mid 3-hig
		return 1;
	}
	else if(ret == 2){
		r[15] = "md5";
		r[16] = "0";
		return 2;
	}

	ret=check_ip(t[0]);
	if(ret == 1){
		r[15] = "ip";
		r[16] = "0";
		return 1;
	}
	else if(ret == 2){
		r[15] = "ip";
		r[16] = "0";
		return 2;
	}

	ret=check_url(t[11]);
	if(ret == 1){
		r[15] = "url";
		r[16] = "0";
		return 1;
	}
	else if(ret == 2){
		r[15] = "url";
		r[16] = "0";
		return 2;
	}

	ret=check_dns(t[12]);
	if(ret == 1){
		r[15] = "dns";
		r[16] = "0";
		return 1;
	}
	else if(ret == 2){
		r[15] = "dns";
		r[16] = "0";
		return 2;
	}

	string check_file = t[6];
	string check_path = check_dir + check_file;
	ret=check_av(check_path.c_str(), virus);
	if(ret == 1){
		printf("the file is infected!\n");
		r[17] = virus;
		r[15] = "av";
		r[16] = "0";
		return 3;
	}
	else if(ret == 2){
		printf("the file is not sure !\n");
		r[17] = virus;
		r[15] = "av";
		r[16] = "0";
		return 4;
	}

}

void writejsonfile(int tag, const char * file, vector<string>  p)
{
	char write_path[1024]={0};
	if(tag==1)
	{
		if(p[7] == "M")
		strcpy(write_path, "/home/SD/Data/ResDIR/DIR-1/");
		if(p[7] == "F")
		strcpy(write_path, "/home/SD/Data/ResDIR/DIR-2/");
		if(p[7] == "N")
		strcpy(write_path, "/home/SD/Data/ResDIR/DIR-3/");
	}
	if(tag == 2)
		strcpy(write_path, "/home/SD/Data/DetectDIR/");
	
	strcat(write_path, file);
	//根节点
    Json::Value root; 
    //根节点属性

	root["sip"]			=	Json::Value(p[0]);
	root["sport"]		=	Json::Value(p[1]);
	root["dip"]			=	Json::Value(p[2]);
	root["dport"]		=	Json::Value(p[3]);
	root["protocol"]	=	Json::Value(p[4]);
	root["app_type"]	=	Json::Value(p[5]);
	root["name"]		=	Json::Value(p[6]);
	root["from"]		=	Json::Value(p[7]);
	root["session_id"]	=	Json::Value(p[8]);
	root["sender"]		=	Json::Value(p[9]);
 	root["receiver"]	=	Json::Value(p[10]);
 	root["url"]			=	Json::Value(p[11]);
 	root["dns"]			=	Json::Value(p[12]);
 	root["md5"]			=	Json::Value(p[13]);
 	root["c_time"]		=	Json::Value(p[14]);
 	root["detect_type"]	=	Json::Value(p[15]);		//[ md5, ip, url, dns , av ]
 	root["level"]		=	Json::Value(p[16]);		//[ 0-non, 1-low, 2-mid, 3-hig ]
 	root["virus_name"]	=	Json::Value(p[17]);		//[ ok, virname] 

    //缩进输出
    cout << "StyledWriter:" << endl;
    Json::StyledWriter sw;
    cout << sw.write(root) << endl << endl;
     
    //输出到文件

    ofstream os;
    os.open(write_path);
    os << sw.write(root);
    os.close();
 
    return ;
}

int readjsonfile(const char *dir, const char *file, vector <string> & p)
{	

	printf("---------------------------\n");
	printf("-     READ JSON FILE      -\n");
	printf("---------------------------\n");

	string member[15];
	Json::Reader reader;
	Json::Value root;
	char tmp_path[1024]={0};
	char cmd[1024]={0};

	strcpy(tmp_path, dir);
	printf("---------------------------\n");
	printf("- json file dir is [%s]\n", tmp_path);
	printf("- json file name is [%s]\n", file);
	strcat(tmp_path,file);
	printf("- absolute path is [%s]\n", tmp_path);
	printf("---------------------------\n\n");
	 
    //从文件中读取
    ifstream is;
    //is.open("/home/SD/Data/InfoDIR/a.json", ios::binary);
    is.open(tmp_path, ios::binary);
	if(!is.is_open())
	{
		cout<<"open error!"<<endl;
		is.close();
		return -1;
	}
 
    if(reader.parse(is,root))
    {
        //读取根节点信息
		member[0] = 	root["sip"].asString();
		member[1] = 	root["sport"].asString();
		member[2] = 	root["dip"].asString();
		member[3] = 	root["dport"].asString();
		member[4] = 	root["protocol"].asString();
		member[5] = 	root["app_type"].asString();
		member[6] = 	root["name"].asString();
		member[7] = 	root["from"].asString();
		member[8] = 	root["session_id"].asString();
		member[9] = 	root["sender"].asString();
		member[10] = 	root["receiver"].asString();
		member[11] = 	root["url"].asString();
		member[12] = 	root["dns"].asString();
		member[13] = 	root["md5"].asString();
		member[14] = 	root["c_time"].asString();
	//	member[13] = 	root["ip"].asString();
	//	member[14] = 	root["md5"].asString();
	//	member[15] = 	root["c_time"].asString();
    }
 
    is.close();
	
		
	int i=0;

	for(i=0; i<15; i++)		
	{
		p.push_back(member[i]);
	}

	printf("---------------------------\n");
	printf("-     DEL JSON FILE       -\n");
	printf("---------------------------\n");

	strcpy(cmd, "rm -rf ");
	strcat(cmd, tmp_path);
	printf("- absolute del cmd is [%s]\n", cmd);
	system(cmd);

	printf("---------------------------\n");
	printf("-     READ JSON END       -\n");
	printf("---------------------------\n");
	

    return 0;
}

int freadsome(void *dest, size_t remain, FILE *file)  
{
    char *offset = (char*)dest;
    while (remain) {
        int n = fread(offset, 1, remain, file);
        if (n==0) {
            return -1;
        }
  
        remain -= n;
        offset += n;
    }
    return 1;
}

int callback(const char* dir, const char* file)
{
	const char* ch = strrchr(file, '.'), *suffix;
	if (!ch) {
		return -1;
	}

	printf("\n");
	printf("---------------------------\n");
	printf("-          BEGIN          -\n");
	printf("---------------------------\n");
	printf("- DIR %s\n", dir);
	printf("- FIL %s\n", file);

	suffix = ch+1;
	printf("- SUF %s\n", suffix);
	
	if (!strcmp(suffix, "json")) {
		printf("- suffix is json !\n");
	}
	else{
		printf("- suffix not json !\n");
		return 0;
	}
	
	printf("---------------------------\n");
	return 1;
}

void monitor()
{
	string path="/home/SD/Data/InfoDIR/";		//path store incoming file information
	vector<string> p; 							//save tmp json data for writing into json file
	vector<string> result; //save json data;

    int monitor = inotify_init();
    if ( -1 == monitor ) {
        perror("monitor: ");
    }

    int watcher = inotify_add_watch(monitor, path.c_str(), IN_CLOSE_WRITE);
    if ( -1 == watcher  ) {
        perror("inotify_add_watch: ");
    }

    FILE *monitor_file = fdopen(monitor, "r");
    char name[1024];

    while (1) {

		p.clear();
		result.clear();

        struct inotify_event event;
        if ( -1 == freadsome(&event, sizeof(event), monitor_file) )
		{
            perror("fread monitor");
        }

        if (event.len)
		{
            freadsome(name, event.len, monitor_file);
        }
		else{
            sprintf(name, "FD: %d\n", event.wd);
        }

        if (!callback(path.c_str(), name)) {
			printf("callback func false!\n");
        }

		readjsonfile(path.c_str(), name, p);

		int ret=check(p, result);
		if(ret==1)
			writejsonfile(1, name, result);
		if(ret==2)
			writejsonfile(1, name, result);
		if(ret==3)
			writejsonfile(1, name, result);
		if(ret==4)
			writejsonfile(2, name, result);
    }
    return ;
}


void * func1(void* args)  
{  

	struct msgstru  
	{  
	   int  categary; 
	   char version[2048];  
	  
	};  
	
	int imsgID;
	struct msgstru slQMsg,slRcvMsg;
	int ilrc;
	
	while(1)
	{	
		//pthread_mutex_lock(&mutex);  

		imsgID=msgget(MSGKEY, IPC_EXCL);//检查消息队列是否存在  

		if(imsgID < 0)
		{
		      printf("消息队列不存在! errno=%d [%s]\n",errno,strerror(errno));  
		      exit(-1);  
		}

		ilrc = msgrcv(imsgID,&slRcvMsg,sizeof(struct msgstru),0,0);/*接收消息队列*/  
		
		if ( ilrc < 0 )
		{  
			printf("msgsnd()读消息队列失败,errno=%d[%s]\n",errno,strerror(errno));  
			exit(-1);  
		}
		else
		{
			printf("消息队列read成功\n");
		}
		//free(MSG_R);

		printf("TYPE	=	[%d]\n", slRcvMsg.categary);  
		printf("VERSION	=	[%s]\n", slRcvMsg.version);  
		
		//	 msgctl(imsgID,IPC_RMID,0); //删除消息队列  
		
		printf("---------------------------\n");
		printf("-          UPDATE          \n");
		printf("---------------------------\n");
		
		switch(slRcvMsg.categary)
		{
			case 1:
				load_msg_ip();
				break;
			case 2:
//				load_msg_url_w();
//				load_msg_url_b();
				ac_w.update(0);
				ac_b.update(1);
				break;
			case 3:
				load_msg_dns();
				break;
			case 4:
				load_msg_md5();
				break;
			default:
				printf("The categary type not 1 2 3 4\n");
		}
		
		printf("---------------------------\n");
		printf("-       UPDATE END         \n");
		printf("---------------------------\n");
		
		//pthread_mutex_unlock(&mutex);  
	}
}  

void * func2(void* args)  
{  
    while(1)  
    {
        	pthread_mutex_lock(&mutex);  
			monitor();
        	pthread_mutex_unlock(&mutex);  
    }  
}  

int main(void)
{

    pthread_t pid1, pid2;  
    pthread_mutex_init(&mutex, NULL);  


	printf("---------------------------\n");
	printf("-          START          -\n");
	printf("---------------------------\n");

	load_msg_md5();
	load_msg_ip();
//	load_msg_url_w();
//	load_msg_url_b();
	ac_w.update(0);
	ac_b.update(1);
	load_msg_dns();


    if(pthread_create(&pid1, NULL, func1, NULL))  
    {  
        return -1;  
    }  
  
    if(pthread_create(&pid2, NULL, func2, NULL))  
    {  
        return -1;  
    }  
	
	while(1)
		sleep(0);

	return 0;
	
}
