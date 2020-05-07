#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

const char CONTENTDIR[]="./contentdir" ; // this is the directory where keep all the files for requests


pthread_mutex_t mutex;
pthread_mutexattr_t mattr;



void error(const char *msg)
{
    perror(msg);
    exit(1);
}



struct user_input
     {
         int sockfd;      
         int thread_NO; 
         int buffer_size;
         pthread_t *thread_pool;
     }input;

struct newsocket
{
    int newsockfd;
};


void httpWorker(int *);// This function will handle request
char * fType(char *);
char * responseHeader(int, char *);// function that builds response header
void *thread_pool(void *); //This function is called in main() function to create thread pool and create scheduling thread
void *sched_thread(void *); //This function is called in main_thread() function to call worker threads by using for loop




int main(int argc, char *argv[])
{
    pthread_t main_thread;
    int sockfd, newsockfd, portno;
    
    struct user_input *input = (struct user_input *)malloc(sizeof(struct user_input));
    input->thread_NO = atoi(argv[2]);//gathering user input of thread number
    input->buffer_size = atoi(argv[3]);//same for buffer size
    
     
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
     
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
     
    if (sockfd < 0) 
       error("ERROR opening socket");
     
    bzero((char *) &serv_addr, sizeof(serv_addr));
     
    portno = atoi(argv[1]);
     
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
     
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
            error("ERROR on binding");

    listen(sockfd, input->buffer_size);
    


    input->sockfd = sockfd;
    pthread_create(&main_thread, NULL, (void *)thread_pool, (void *)input);
    pthread_join(main_thread, NULL);
    
 

    pthread_exit(NULL);
    close(sockfd);
    
    return 0; 
}



void *thread_pool(void *input) 
{
    int worker_id;//index for creating thread pool

    //Casting info from struct argument passed from main()
    struct user_input *input_args = (struct user_input *)input;
    int thread_NO = ((struct user_input *)input)->thread_NO;


    //Getting ready to create thread pool and scheduling thread
    pthread_t thread_pool[thread_NO];
    pthread_t main_thread2;
    input_args->thread_pool = &thread_pool[thread_NO];
    
    
     //thread pool creating
    for(worker_id = 0; worker_id < sizeof(thread_NO); worker_id++)
    {   
        input_args->thread_pool[worker_id] = worker_id;
    }
      
    
    //creating scheduling thread
    pthread_create(&main_thread2, NULL, sched_thread, (void *)input_args);
    pthread_join(main_thread2, NULL);

    pthread_exit(0);
}



void *sched_thread(void *input)
{
    struct newsocket *new_sock = (struct newsocket *)malloc(sizeof(struct newsocket));

    //gathering info from struct passed from main_thread()
    struct user_input *input_args = (struct user_input *)input;
    int sockfd = ((struct user_input *)input)->sockfd;
    input_args->buffer_size = ((struct user_input *)input)->buffer_size;

    int newsockfd;


    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int i;

    
    pthread_t *worker_thread = input_args->thread_pool;
    

    clilen = sizeof(cli_addr);
    new_sock->newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

        
    if (new_sock->newsockfd < 0) 
        error("ERROR on accept");
        
     
    while(1)
    {
            for(int i = 0; i < input_args->buffer_size; i++)
            {  
                close(sockfd);
                pthread_create(&worker_thread[i], NULL, (void *)httpWorker, &new_sock->newsockfd); 
                pthread_join(worker_thread[i], NULL);  
            }

    }

    pthread_exit(0);
    pthread_mutex_destroy(&mutex);
}



void httpWorker(int *sockfd)//sockfd contains all the information
{
    int newsockfd = *sockfd;// create a local variable for sockfd 
    char buffer[256];// we will read the data in this buffer
    char *token;// local variable to split the request to get the filename 
    bzero(buffer,256);// intialize the buffer data to zero
    char fileName[50];
    char homedir[50];
    char * type;
    strcpy(homedir,CONTENTDIR);// directory where files are stored.
    char *respHeader; //response header
    // start reading the message from incoming conenction

    pthread_detach(pthread_self());
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_TIMED_NP);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&mutex, &mattr);
    pthread_mutex_lock(&mutex);

    

    if (read(newsockfd,buffer,255) < 0) 
      error("ERROR reading from socket");
    //get the requested file part of the request
    token = strtok(buffer, " ");// split string into token seperated by " "
    token = strtok(NULL, " ");// in this go we read the file name that needs to be sent
    strcpy(fileName,token);
     
    // get the complete filename 
    if(strcmp(fileName,"/")==0) // if filename is not provided then we will send index.html
        strcpy(fileName,strcat(homedir,"/index.html"));
    else
        strcpy(fileName,strcat(homedir,fileName));    
    type = fType(fileName);// get file type
    //open file and ready to send 
    FILE *fp;
    int file_exist=1;
    fp=fopen(fileName, "r"); 
    if (fp==NULL) file_exist=0; 
    respHeader = responseHeader(file_exist,type);
    if ((send(newsockfd, respHeader,strlen(respHeader), 0) == -1) || (send(newsockfd,"\r\n", strlen("\r\n"), 0) == -1))
      perror("Failed to send bytes to client");   

    free(respHeader);// free the allocated memory (note: the memory is allocated in responseheader function)

    if (file_exist)
    {
      char filechar[1];
      while((filechar[0]=fgetc(fp))!=EOF)
      {    
        if(send(newsockfd,filechar,sizeof(char),0) == -1) perror("Failed to send bytes to client");       
      } 
    }
    else
  {
    if (send(newsockfd,"<html> <HEAD><TITLE>404 Not Found</TITLE></HEAD><BODY>Not Found</BODY></html> \r\n", 100, 0) == -1)
      perror("Failed to send bytes to client");          
  }

    close(newsockfd);
    pthread_mutex_unlock(&mutex);
}


// function below find the file type of the file requested
char * fType(char * fileName){
     char * type; 
      char * filetype = strrchr(fileName,'.');// This returns a pointer to the first occurrence of some character in the string 
      if((strcmp(filetype,".htm"))==0 || (strcmp(filetype,".html"))==0)
            type="text/html";
      else if((strcmp(filetype,".jpg"))==0)
            type="image/jpeg";
      else if(strcmp(filetype,".gif")==0)
            type="image/gif";
      else if(strcmp(filetype,".txt")==0)
            type="text/plain";
      else
            type="application/octet-stream";
     
return type;
}



//buildresponseheader
char * responseHeader(int filestatus, char * type){
   char statuscontent[256] = "HTTP/1.0";
   if(filestatus==1){
            strcat(statuscontent," 200 OK\r\n");
            strcat(statuscontent,"Content-Type: ");
            strcat(statuscontent,type);
            strcat(statuscontent,"\r\n");
        }
   else {
            strcat(statuscontent,"404 Not Found\r\n");
            //send a blank line to indicate the end of the header lines   
            strcat(statuscontent,"Content-Type: ");
            strcat(statuscontent,"NONE\r\n");
        } 
   char * returnheader =malloc(strlen(statuscontent)+1);
   strcpy(returnheader,statuscontent);
   return returnheader;
}