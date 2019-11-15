#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<signal.h>
#include<fcntl.h>
#include<openssl/md5.h>
#include"httpd.h"

#define MAX_CONNECTION 1000
#define BYTES 1024

#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m"

struct {
    char *ext;
    char *filetype;
} extensions [] = {
    {"gif", "image/gif" },
    {"jpg", "image/jpeg"},
    {"jpeg","image/jpeg"},
    {"png", "image/png" },
    {"zip", "image/zip" },
    {"gz",  "image/gz"  },
    {"tar", "image/tar" },
    {"htm", "text/html" },
    {"html","text/html" },
    {"exe","text/plain" },
    {0,0} };



static char *buf;
char * fstr;
typedef struct { char *name, *value; } header_t;
static header_t reqhdr[17] = { {"\0", "\0"} };

int file_fd, nBytes;
char *ROOT, *pathname, *PATH;
int listenfd, clients[MAX_CONNECTION];
void error(char *);
void startServer(char *);
void respond(int);

int main(int argc, char* argv[])
{
	struct sockaddr_in clientaddr;
	socklen_t addrlen;
	char c;    
	
	//Default Values PATH = ~/ and PORT=10000
	char PORT[6];
	ROOT = getenv("PWD");
	strcpy(PORT,"10000");

	int slot=0;

	//Parsing the command line arguments
	while ((c = getopt (argc, argv, "p:r:")) != -1)
		switch (c)
		{
			case 'r':
				ROOT = (void *)malloc(strlen(optarg));
				strcpy(ROOT,optarg);
				break;
			case 'p':
				strcpy(PORT,optarg);
				break;
			case '?':
				fprintf(stderr,"Wrong arguments given!!!\n");
				exit(1);
			default:
				exit(1);
		}
	
	printf("Server started at port no. %s%s%s with root directory as %s%s%s\n"
			,GREEN,PORT,RESET,GREEN,ROOT,RESET);
	// Setting all elements to -1: signifies there is no client connected
	int i;
	for (i=0; i<MAX_CONNECTION; i++)
		clients[i]=-1;
	startServer(PORT);

	// ACCEPT connections
	while (1)
	{
		addrlen = sizeof(clientaddr);
		clients[slot] = accept (listenfd, (struct sockaddr *) &clientaddr, &addrlen);

		if (clients[slot]<0)
			error ("accept() error");
		else
		{
			if ( fork()==0 )
			{
				respond(slot);
				exit(0);
			}
		}

		while (clients[slot]!=-1) slot = (slot+1)%MAX_CONNECTION;
	}

	return 0;
}

//start server
void startServer(char *port)
{
	struct addrinfo hints, *res, *p;

	// getaddrinfo for host
	memset (&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo( NULL, port, &hints, &res) != 0)
	{
		perror ("getaddrinfo() error");
		exit(1);
	}
	// socket and bind
	for (p = res; p!=NULL; p=p->ai_next)
	{
		listenfd = socket (p->ai_family, p->ai_socktype, 0);
		if (listenfd == -1) continue;
		if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) break;
	}
	if (p==NULL)
	{
		perror ("socket() or bind()");
		exit(1);
	}

	freeaddrinfo(res);

	// listen for incoming connections
	if ( listen (listenfd, 1000000) != 0 )
	{
		perror("listen() error");
		exit(1);
	}
}

// get request header
char *request_header(const char* name)
{
    header_t *h = reqhdr;
    while(h->name) {
        if (strcmp(h->name, name) == 0) return h->value;
        h++;
    }
    return NULL;
}

void respond(int n)
{
    char mesg[999999],msg[999999], *reqline[200], *resline[200], data_to_send[BYTES], path[99999];
	int rcvd, fd, bytes_read;

	memset( (void*)mesg, (int)'\0', 999999 );

	rcvd=recv(clients[n], mesg, 999999, 0);
	
	sprintf(msg, "%s",mesg);

	strcpy(msg, mesg);

	if (rcvd<0)    // receive error
		fprintf(stderr,("recv() error\n"));
	else if (rcvd==0)    // receive socket closed
		fprintf(stderr,"Client disconnected upexpectedly.\n");
	else    // message received
	{
		printf("%s", mesg);
		reqline[0] = strtok (mesg, " \t\n");
		if ( strncmp(reqline[0], "GET\0", 4)==0 )
		{
			reqline[1] = strtok (NULL, " \t");
			reqline[2] = strtok (NULL, " \t\n");
			if ( strncmp( reqline[2], "HTTP/1.0", 8)!=0 && strncmp( reqline[2], "HTTP/1.1", 8)!=0 )
			{
				write(clients[n], "HTTP/1.0 400 Bad Request\n", 25);
			}
			else
			{
				if ( strncmp(reqline[1], "/\0", 2)==0 )
					reqline[1] = "/index.html";        //Because if no file is specified, index.html will be opened by default (like it happens in APACHE...

				strcpy(path, ROOT);
				strcpy(&path[strlen(ROOT)], reqline[1]);
				printf("file: %s\n", path);

				if ( (fd=open(path, O_RDONLY))!=-1 )    //FILE FOUND
				{
					send(clients[n], "HTTP/1.0 200 OK\n\n", 17, 0);
					while ( (bytes_read=read(fd, data_to_send, BYTES))>0 )
						write (clients[n], data_to_send, bytes_read);
				}
				else    {
					perror(path);
					write(clients[n], "HTTP/1.0 404 Not Found\n", 23); //FILE NOT FOUND
				}
			}
		}else if (strncmp(reqline[0], "POST\0", 5)==0){	
			int i=1;
			do{
				reqline[i] = strtok(NULL, " \t\n\r\"");
			}while(strncmp(reqline[i++],"filename=",9));
			reqline[i] = strtok(NULL, " \t\n\r\"");
			char *filename = malloc(strlen(reqline[i]));
			strcpy(filename, reqline[i]);
			strcat(path, "views/src/");
			strcat(path, filename);
			
            char buff[999999];
			int j=0, cnt=0, change=1, rnd=0;
			for(i=0; i<rcvd; i++){
				if(msg[i]=='\n'){
				//	printf(GREEN"cnt = %d"RESET, cnt);
					if(cnt==2){
						change++;
						change%=2;
						rnd++;
					}
					cnt=0;
				}
				if('A'<=msg[i]<='z') cnt++;	
				if(change){
					printf(YELLOW);
					if(rnd>=2)
						buff[j++] = msg[i];
				}else{
					printf(BLUE);
				}

                switch (msg[i])
				{
				case '\r':
				//	printf("\\r");
					break;
				case '\n':
				//	printf("\\n\n");
					break;
				case '\t':
				//	printf("\\t");
					break;
				default:
				//	printf("%c", msg[i]);
					cnt++;
					break;
				}
				printf(RESET);
			}

			buff[j]='\0';
			
			if ( (file_fd=open(path,O_WRONLY|O_CREAT|O_TRUNC, __S_IREAD| __S_IWRITE))!=-1 )    //FILE FOUND
			{
				write(file_fd, buff, j);
				close(file_fd);

				strcpy(path, "src/");
				strcat(path, filename);
				sprintf(data_to_send, "HTTP/1.0 200 OK\n\n\
				<a href=\"%s\">%s</a>", path, path);

				send(clients[n], data_to_send, strlen(data_to_send), 0);
			}
			else    {
				perror(path);
				write(clients[n], "HTTP/1.0 404 Not Found\n", 23); //FILE NOT FOUND	
			}
		
			free(filename);
		}
	}

	//Closing SOCKET
	shutdown (clients[n], SHUT_RDWR);         //All further send and recieve operations are DISABLED...
	close(clients[n]);
	clients[n]=-1;
}

