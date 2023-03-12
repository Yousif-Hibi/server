
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include "threadpool.h"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
int   handleRes(void*);
int   isPermitions(char*);
char* get_mime_type(char*);
char* getHtml(char*, char*);
char* errorDir(char*,char*,int);
void succsesDir(char*,char*,int,int);
char * htmlres(char*,char*,int);

int main(int argc, char **argv)
{
    char *port=argv[1];
    char* pool=argv[2];
    char* res=argv[3];
    threadpool *thread = create_threadpool(atoi(pool));
    if(thread == NULL)
    {
        printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
        exit(-1);
    }
    if((isdigit(port) == -1 || isdigit(pool) == -1 || isdigit(res) == -1)||argc!=4)
    {
        printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
        exit(-1);
    }
    struct sockaddr_in serv_addr;
    struct sockaddr cli_addr;
    int socketClient = sizeof(serv_addr);
    int socketServer;
    int newSocket[atoi(res)];
    if ((socketServer = socket(AF_INET,SOCK_STREAM,0)) < 0)  //creating the main socket
    {
        perror("error making socket");
        exit(-1);
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(atoi(port));

    if (bind(socketServer, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("error binding");
        exit(-1);
    }

    if(listen(socketServer, 5) < 0)
    {
        perror("error tring to listen");
        close(socketServer);
        exit(-1);
    }
    int i;
    for(i = 0 ; i < atoi(res) ; ++i)
    {
        if((newSocket[i] = accept(socketServer, &cli_addr, (socklen_t *)&socketClient)) < 0)
        {
            perror("error in accept");
            close(socketServer);
            exit(-1);
        }
        dispatch(thread,handleRes,&newSocket[i]);
    }
    close(socketServer);
    destroy_threadpool(thread);
    return 0;















}
int handleRes(void* sock){
    char method[1000];
    bzero(method,1000);
    char path[1000];
    bzero(path,1000);
    char protocol[1000];
    bzero(protocol,1000);
    char buff[4000];
    bzero(buff,4000);
    int socket = *(int*)sock;
    int size;
    char* html=NULL;
    while(1) {
        if ((size = read(socket, buff, sizeof(buff))) < 0) //read request
        {
            perror("error reading");
            errorDir("500 Internal Server Error",path,socket);
            return 0;
        }
        if(strchr(buff,'\r')!=NULL||size==0)
            break;
    }
    char* tokken;
    char cutStr [strlen(buff)+1];
    strncpy(cutStr,buff,strlen(buff)+1);
    if(strstr(buff,"\r\n")) {
        tokken = strtok(cutStr, "\r\n");
        strcpy(method,strtok(cutStr, " "));
        strcpy(  path , strtok(NULL, " "));
        strcpy(protocol , strtok(NULL, " "));
    }
    if(strcmp(buff,"")==0 || method==NULL || path==NULL || protocol==NULL  || tokken==NULL){
        errorDir("400 Bad Request",path,socket);
        return 0;
    }
    if(!(strlen(path)==1)&& !(strcmp(path,"/")==0)) {

        memmove(path, path+1, strlen(path));

    }else{
        char privateCase[3]="./";
        strncpy(path,privateCase,strlen(privateCase));
    }
    if(strcmp(method,"GET")!=0)
    {
        errorDir("401",path,socket);
        return 0;
    }

    else if( (strcmp(protocol,"HTTP/1.0")!=0) && (strcmp(protocol,"HTTP/1.1")!=0)){
       errorDir("400 Bad Request",path,socket);
        return 0;
    }
    struct stat sb;
     if(( stat(path, &sb ) != 0)){
        errorDir("404",path,socket);
        return 0;
    }
     else if((sb.st_mode & S_IFDIR)) {
         if (path[strlen(path) - 1] != '/') {
             errorDir("302 Found", path, socket);
             return 0;
         } else if (path[strlen(path) - 1] == '/') {
             char Url[sizeof(path) + 11];
             bzero(Url, sizeof(path) + 11);
             strncat(Url, path, strlen(path));
             strcat(Url, "index.html");

             if (stat(Url, &sb) >= 0) {
                 if (isPermitions(Url) == 1) {
                     html = getHtml(path, "index.html");
                     if (html != NULL) {
                         htmlres(html, path, socket);
                         return 0;
                     } else if (strcmp(html, "ERROR")) {
                         errorDir("500 Internal Server Error", path, socket);
                         return 1;
                     }
                 } else {
                     errorDir("403 Forbidden", path, socket);
                     return 0;
                 }
             } else if (stat(Url, &sb) < 0) {
                 if (isPermitions(path) == 1) {
                     succsesDir("200 Ok",path,socket,1);
                     return 0;
                 } else {
                     errorDir("403 Forbidden",path, socket);
                     return 0;
                 }
             }
         }
     }
    else if(!(sb.st_mode & S_IFDIR)){
        if(!S_ISREG(sb.st_mode))
        {
            errorDir("403 Forbidden",path,socket);
            return 0;
        }
        else if((sb.st_mode & S_IRUSR) &&(sb.st_mode & S_IRGRP) &&(sb.st_mode & S_IROTH)){
            if(isPermitions(path)==1){
               succsesDir("200 Ok",path,socket,0);
                return 0;
            }
            else {
               errorDir("403 Forbidden",path,socket);
                return 0;
            }
        }
        else{
            errorDir("403 Forbidden",path,socket);
            return 0;

        }
    }
    return 0;
}
char* errorDir(char*numError,char*path,int socket) {

    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    int size = 50000;
    char str[size];
    bzero(str, size);
    strcat(str, "HTTP/1.0 ");
    strcat(str, numError);
    strcat(str, "\r\n");
    strcat(str, "Server: webserver/1.0\r\n");
    strcat(str, "Date: ");
    strcat(str, timebuf);
    if (numError == "302 Found") {
        strcat(str, "\r\nLocation: ");
        strcat(str, path);
        strcat(str, "/\r\nContent-Type: text/html\r\n");
    }
     else{
        strcat(str, "\r\nContent-Type: text/html\r\n");
     }
    strcat(str, "Content-Length: ");
    if (numError == "400 Bad Request") {
        strcat(str, "121");
        strcat(str, "\r\nConnection: close\r\n");
        strcat(str,
               "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n<BODY><H4>400 Bad Request</H4>\r\nBad Request.\r\n</BODY></HTML>\r\n");
    } else if (numError == "302 Found") {
        strcat(str, "131");
        strcat(str, "\r\nConnection: close\r\n\r\n");
        strcat(str,"<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\r\n<BODY><H4>302 Found</H4>\r\nDirectories must end with a slash.\r\n</BODY></HTML>\r\n");
    } else if (numError == "403 Forbidden") {
        strcat(str, "118");
        strcat(str, "\r\nConnection: close\r\n\r\n");
        strcat(str,"<HTML><HEAD><TITLE>403 Forbidden/TITLE></HEAD>\r\n<BODY><H4>403 Forbidden</H4>\r\nAccess denied.\r\n</BODY></HTML>\r\n");
    } else if (numError == "404 Not Found") {
        strcat(str, "119");
        strcat(str, "\r\nConnection: close\r\n\r\n");
        strcat(str,"<HTML><HEAD><TITLE>404 Not Found/TITLE></HEAD>\r\n<BODY><H4>404 Not Found</H4>\r\nFile not found.\r\n</BODY></HTML>\r\n");
    } else if (numError == "500 Internal Server Error") {
        strcat(str, "151");
        strcat(str, "\r\nConnection: close\r\n\r\n");
        strcat(str,"<HTML><HEAD><TITLE>500 Internal Server Error/TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>\r\n");
    } else if (numError == "501 Not supported") {
        strcat(str, "138");
        strcat(str, "\r\nConnection: close\r\n\r\n");
        strcat(str,"<HTML><HEAD><TITLE>501 Not supported/TITLE></HEAD>\r\n<BODY><H4>501 Not supported</H4>\r\n\"Method is not supported.\r\n</BODY></HTML>\r\n");
    }


    str[strlen(str)] = '\0';
    int n = 0, rescounter = 0;

    while (1) {
        n = write(socket, str, strlen((str)));
        rescounter += n;
        if (rescounter == strlen(str)) { break; }
        if (n < 0) {
            perror("error in write");
            return NULL;
        }
    }
    close(socket);
    return "SUCCESS";
}
char* getHtml(char * path, char * html) {
    struct dirent **list;
    struct stat sb;
    int n;
    char *url = (char *) malloc(sizeof(path) + sizeof(html) + 1);
    bzero(url,(int)sizeof(url));
    strcat(url, path);
    strcat(url, html);
    if(stat(url, &sb)!=0){
        perror("stat Error\n");
        return "ERROR";
    }


    char *buffer = (char *) malloc((long long) sb.st_size +1);
    if(buffer==NULL)
    {
        printf("malloc Error\n");
        return "ERROR";
    }
    bzero(buffer, (int)sizeof(buffer));


    n = scandir(path, &list, NULL, alphasort);
    if (n == -1) {
        perror("scandir");
        return "ERROR";
    }
    for (int i = 0; i < n; i++) {

        if (strcmp(list[i]->d_name, html) == 0) {



            if (stat(url, &sb) == -1) {
                perror("stat");
                exit(EXIT_FAILURE);
            }

            int nbytes=0;
            FILE *f;


            f = fopen(url, "rb");
            while ((nbytes = fread(buffer, 1,1024, f)) > 0);
            fclose(f);
            while (n--) {

                free(list[n]);
            }

            free(url);
            free(list);
            buffer[sb.st_size]='\0';

            return buffer;

        }
    }

    while (n--) {

        free(list[n]);
    }

    free(url);
    free(list);
    return NULL;
}

char * htmlres(char * file,char* path,int socket){

    time_t now;
    char timebuf[128];
    char timecur[128];
    now = time(NULL);
    strftime(timecur, sizeof(timecur), RFC1123FMT, gmtime(&now));
   struct stat sb;
    stat(path,&sb);
    strftime(timebuf,sizeof(timebuf),RFC1123FMT,gmtime(&sb.st_mtime));

    char orignalTextSize[100];
    sprintf(orignalTextSize,"%d",(int)strlen(file));

    char str [6000];
    bzero(str,6000);

    strcat(str,"HTTP/1.0 ");
    strcat(str,"200 OK\r\n");
    strcat(str,"Server: webserver/1.0\r\n");
    strcat(str,"Date: ");
    strcat(str, timecur);
    strcat(str,"\r\n");
    strcat(str,"Content-Type: ");
    strcat(str,"text/html");
    strcat(str, "\r\nContent-Length: ");
    char conLen[100];
    sprintf(conLen, "%d", sb.st_size);
    strcat(str, conLen);
    strcat(str, "Last-Modified: ");
    strcat(str,timebuf);
    strcat(str ,"\r\nConnection: close\r\n\r\n");
    strcat(str, file);
    str[strlen(str)+2]='\0';
    int n = 0,res= 0;

    while (1) {
        n = write(socket, str, strlen((str)));
        res += n;
        if (res == strlen(str)) { break; }
        if (n < 0) {
            perror("ERROR writing to socket");
            return NULL;
        }
    }
    close(socket);
    free(file);
    return "SUCCESS";
}

void succsesDir(char*num,char*path,int socket,int check) {

    time_t now;
    struct stat sb;
    stat(path,&sb);
    char timecur[128];
    now = time(NULL);
    strftime(timecur, sizeof(timecur), RFC1123FMT, gmtime(&now));
    char timebuff[128];
    strftime(timebuff,sizeof(timebuff),RFC1123FMT,gmtime(&sb.st_mtime));
    char *type = get_mime_type(path);
    int size=6000;
    char  str [size];
    bzero(str, size);
    strcat(str, "HTTP/1.0 ");
    strcat(str, "200 OK\r\n");
    strcat(str, "Server: webserver/1.0\r\n");
    strcat(str, "Date: ");
    strcat(str, timecur);
    if(type){
        strcat(str, "\r\n");
        strcat(str, "Content-Type: ");
        strcat(str, type);}
    strcat(str, "\r\nContent-Length: ");
    char conLen[100];
    sprintf(conLen, "%d", sb.st_size);
    strcat(str, conLen);
    strcat(str, "\r\nLast-Modified: ");
    strcat(str, timebuff);
    strcat(str, "\r\nConnection: close\r\n\r\n");
    if(check==1){
        struct dirent **list;
        struct stat sb;
        int size=6000;
        int n = scandir(path, &list, NULL, alphasort);
        if (n == -1) {
            perror("error in scandir");
            return ;
        }
        char * makeHTML = (char*)malloc(n*500 +size);
        if(makeHTML==NULL)
        {
            printf("malloc Error");
            return ;
        }
        bzero(makeHTML,500*n+size);
        strcat(makeHTML,"<HTML>\r\n<HEAD><TITLE>");
        strcat(makeHTML,path);
        strcat(makeHTML,"</TITLE></HEAD>\r\n");
        strcat(makeHTML,"<BODY>\r\n<H4>Index of ");
        strcat(makeHTML,path);
        strcat(makeHTML,"</H4>\r\n");
        strcat(makeHTML,"<table CELLSPACING=8>\r\n");
        strcat(makeHTML,"<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n");
        for(int i =0 ; i< n; i++){
            strcat(makeHTML,"<tr><td><A HREF=");
            strcat(makeHTML,list[i]->d_name);
            strcat(makeHTML,">");
            strcat(makeHTML,list[i]->d_name);
            strcat(makeHTML,"</A></td><td>");
            char nameList[strlen(path)+strlen(list[i]->d_name)+1];
            bzero(nameList,strlen(path)+strlen(list[i]->d_name)+1);
            strcat(nameList,path);
            strcat(nameList, list[i]->d_name);

            strcat(makeHTML,timecur);
            strcat(makeHTML,"</td><td>");
            stat(nameList, &sb);
            if(S_ISREG(sb.st_mode))
            {
                char conLen[100];
                sprintf(conLen, "%d", sb.st_size);
                strcat(makeHTML,conLen);
            }
            strcat(makeHTML,"</td></tr>");
            if(n!=0)
            {
                strcat(makeHTML,"\r\n");
            }
        }
        strcat(makeHTML,"</table>\r\n<HR>\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n</BODY></HTML>");

        while (n--) {

            printf("%s\n", list[n]->d_name);
            free(list[n]);
        }
        free(list);
        strcat(str,makeHTML);
        int num = 0, C = 0;

        while (1) {
            num = write(socket, str, strlen(str));
            C+= num;
            if (C== strlen(str)) { break; }
            if (num < 0) {
                perror("ERROR writing to socket");
                return ;
            }

        }
        close(socket);
        free(makeHTML);
    }
    else if(check==0){
        printf("asdsdasd");
    str[strlen(str)+1] = '\0';

    int n = 0, rescounter = 0;
    while (1) {
        n = write(socket, str, (int)strlen((const char*)str));
        rescounter += n;
        if (rescounter == (int)strlen((const char*)str)) { break; }
        if (n < 0) {
            perror("error in write");
            return;
        }
    }
     char buff[1024];
    int nbytes;
    FILE * html = fopen (path, "rb");
    fseek (html, 0, SEEK_SET);
    while((nbytes = fread (buff,1,size, html)) > 0){
        write(socket,buff,nbytes);}
    fclose(html);
    }
}
char *get_mime_type(char *name){
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}


int isPermitions(char *path)
{
    if(strcmp(path,"") == 0)
        return 1;
    char *tokken, *str, temp[strlen(path)];
    struct stat sb;
    str = strdup(path);

    stat(str, &sb);

    if((!(sb.st_mode & S_IFDIR))&&!(sb.st_mode & S_IRUSR) || !(sb.st_mode & S_IRGRP) || !(sb.st_mode & S_IROTH)){
            free(str);
            str = NULL;
            return 0;
        }

    tokken = strtok(str, "/");
    strcpy(temp,tokken);
    while( tokken != NULL ){
        stat(temp, &sb);
        if(sb.st_mode & S_IFDIR)
            if((sb.st_mode & S_IRUSR) && (sb.st_mode & S_IRGRP) &&(sb.st_mode & S_IROTH)){
                tokken = strtok(NULL, "/");
                if(tokken != NULL)
                {
                    strcat(temp,"/");
                    strcat(temp,tokken);
                }

            }else{
                free(str);
                str = NULL;
                return 0;
            }

    }
    free(str);
    str = NULL;
    return 1;
}