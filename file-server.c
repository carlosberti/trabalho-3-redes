#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_REQ_LEN 1000
#define FBLOCK_SIZE 4000
#define APPEND_CONTENT_SIZE 2000

#define REQ_GET "GET"
#define REQ_CREATE "CREATE"
#define REQ_REMOVE "REMOVE"
#define REQ_APPEND "APPEND"

#define COD_OK0_GET "OK-0 Method GET OK\n"
#define COD_OK1_FILE "OK-1 File open OK\n"
#define COD_OK2_CREATE "OK-2 Method CREATE OK\n"
#define COD_OK3_FILE_CREATE "OK-3 File create OK\n"
#define COD_OK4_FILE_REMOVE "OK-4 File remove OK\n"
#define COD_OK5_APPEND_TEXT "OK-5 Text append OK\n"
#define COD_OK6_REMOVE "OK-6 Method REMOVE OK\n"
#define COD_OK6_APPEND "OK-7 Method APPEND OK\n"

#define COD_ERROR_0_METHOD "ERROR-0 Method not supported\n"
#define COD_ERROR_1_FILE "ERROR-1 File could not be open\n"
#define COD_ERROR_2_FILE_EXISTS "ERROR-2 File already exists\n"
#define COD_ERROR_3_FILE_CREATE "ERROR-3 File could not be created\n"
#define COD_ERROR_4_FILE_REMOVE "ERROR-4 File could not be removed\n"
#define COD_ERROR_5_FILE_NOT_EXISTS "ERROR-5 File does not exists\n"
#define COD_ERROR_6_FILE_APPEND_CONTENT "ERROR-6 Content could not be appended to file\n"
#define COD_ERROR_7_INVALID_INPUT "ERROR-7 Invalid input\n"

struct req_t {
  char method[128];
  char content[APPEND_CONTENT_SIZE];
  char filename[256];
};
  
typedef struct req_t req_t;

int words(const char *sentence){
  int count=0, i, len, quote_count=0;
  char lastC;

  len=strlen(sentence);

  if(len > 0){
    lastC = sentence[0];
  }

  for(i=0; i<=len; i++) {
    if(((sentence[i]==' ' || sentence[i]=='\0') && lastC != ' ') || lastC == 34) {
      printf("char: %c", lastC);
      if(lastC == 34){
        quote_count++;
        printf("quotes: %d\n", quote_count);
      }
      if(quote_count != 1)
        count++;
    }
    lastC = sentence[i];
  }

  return count;
}

int get_request(req_t *r, char *rstr, int sockfd) {
  int nof_words;

  bzero(r, sizeof(req_t));
  nof_words = words(rstr);

  if(nof_words == 3 && (strchr(rstr, 34)) == NULL) {
    send(sockfd, COD_ERROR_0_METHOD, strlen(COD_ERROR_0_METHOD), 0);
    return -1;
  }

  printf("entrou: %d\n", nof_words);

  switch (nof_words) {
    case 2:
      sscanf(rstr, "%s %s", r->method, r->filename);
      return 0;
    case 3:
      sscanf(rstr, "%s \"%49[^\"]\" %s", r->method, r->content, r->filename);
      return 0;
    default:
      send(sockfd, COD_ERROR_7_INVALID_INPUT, strlen(COD_ERROR_7_INVALID_INPUT), 0);
      return -1;
  }
};

void send_file(int sockfd, int nr, int fd) {
  unsigned char fbuff[FBLOCK_SIZE];

  bzero(fbuff, FBLOCK_SIZE);
  nr = read(fd, fbuff, FBLOCK_SIZE);
  
  if(nr > 0){
    send(sockfd, fbuff, nr, 0);
    send_file(sockfd, nr, fd);
  }

  send(sockfd, "\n", strlen("\n"), 0);
  return;
};

void open_file(int sockfd, req_t r) {
  int fd, nr;

  fd = open(r.filename, O_RDONLY, S_IRUSR);

  if(fd == -1) {
    perror("open");
    send(sockfd, COD_ERROR_1_FILE, strlen(COD_ERROR_1_FILE), 0);
    return;
  }

  send(sockfd, COD_OK1_FILE, strlen(COD_OK1_FILE), 0);
  send_file(sockfd, nr, fd);
  close(fd);
  return;
}

void create_file(int sockfd, req_t r) {
  int fd;

  if(access(r.filename, F_OK) == 0) {
    send(sockfd, COD_ERROR_2_FILE_EXISTS, strlen(COD_ERROR_2_FILE_EXISTS), 0);
    return;
  }

  fd = creat(r.filename, S_IRUSR | S_IWUSR);


  if(fd == -1){
    perror("open");
    send(sockfd, COD_ERROR_3_FILE_CREATE, strlen(COD_ERROR_3_FILE_CREATE), 0);
    return;
  }

  send(sockfd, COD_OK3_FILE_CREATE, strlen(COD_OK3_FILE_CREATE), 0);
  close(fd);
  return;
}

void remove_file(int sockfd, req_t r) {
  int fd;

  if(access(r.filename, F_OK) == 0) {
    fd = remove(r.filename);

    if(fd == -1){
      perror("remove");
      send(sockfd, COD_ERROR_4_FILE_REMOVE, strlen(COD_ERROR_4_FILE_REMOVE), 0);
      return;
    }

    send(sockfd, COD_OK4_FILE_REMOVE, strlen(COD_OK4_FILE_REMOVE), 0);
    return;
  }

  send(sockfd, COD_ERROR_5_FILE_NOT_EXISTS, strlen(COD_ERROR_5_FILE_NOT_EXISTS), 0);
  return;
}

void append_file(int sockfd, req_t r) {
  int fd, fw;

  if(access(r.filename, W_OK) == 0){
    fd = open(r.filename, O_WRONLY | O_APPEND);

    if(fd == -1) {
      perror("open");
      send(sockfd, COD_ERROR_1_FILE, strlen(COD_ERROR_1_FILE), 0);
      return;
    }

    fw = write(fd, r.content, strlen(r.content));

    if(fw == -1){
      perror("write");
      send(sockfd, COD_ERROR_6_FILE_APPEND_CONTENT, strlen(COD_ERROR_6_FILE_APPEND_CONTENT), 0);
      return;
    }

    send(sockfd, COD_OK5_APPEND_TEXT, strlen(COD_OK5_APPEND_TEXT), 0);
    open_file(sockfd, r);
    return;
  };
  
  send(sockfd, COD_ERROR_1_FILE, strlen(COD_ERROR_1_FILE), 0);
  return;
}

void proc_request(int sockfd, req_t r) {
  if(strcmp(r.method, REQ_GET) == 0) {
    send(sockfd, COD_OK0_GET, strlen(COD_OK0_GET), 0);
    open_file(sockfd, r);
  } else if(strcmp(r.method, REQ_CREATE) == 0) {
    send(sockfd, COD_OK2_CREATE, strlen(COD_OK2_CREATE), 0);
    create_file(sockfd, r);
  } else if(strcmp(r.method, REQ_REMOVE) == 0) {
    send(sockfd, COD_OK6_REMOVE, strlen(COD_OK6_REMOVE), 0);
    remove_file(sockfd, r);
  } else if(strcmp(r.method, REQ_APPEND) == 0) {
    send(sockfd, COD_OK6_APPEND, strlen(COD_OK6_APPEND), 0);
    append_file(sockfd, r);
  } else {
    send(sockfd, COD_ERROR_0_METHOD, strlen(COD_ERROR_0_METHOD), 0);
  };
  return;
}

char request[MAX_REQ_LEN];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void * socketThread(void *arg) {
  int sc = *((int *)arg);
  int nr, is_valid_request;
  req_t req;

  nr = recv(sc, request, MAX_REQ_LEN, 0);
  pthread_mutex_lock(&lock);

  if(nr > 0) {
    is_valid_request = get_request(&req, request, sc);

    if(is_valid_request == 0){
      printf(" method: %s\n filename: %s\n", req.method, req.filename);
      proc_request(sc, req);
    };
  };

  pthread_mutex_unlock(&lock);
  close(sc);  
  pthread_exit(NULL);
};

int main(int argc, char **argv){
  if(argc != 2) {
    printf("Uso: %s <porta>\n", argv[0]);
    return 0;
  };

  int sl;

  sl = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  
  if(sl == -1) {
    perror("socket");
    return 0;
  };

  struct sockaddr_in saddr;

  saddr.sin_port = htons(atoi(argv[1]));
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr =  INADDR_ANY;

  if(bind(sl, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in)) == -1) {
    perror("bind");
    return 0;
  };

  if(listen(sl, 1000) == -1) {
    perror("listen");
    return 0;
  };

  pthread_t tid[60];
  struct sockaddr_in caddr;
  int addr_len, sc, nr, i;

  while(1){
    addr_len = sizeof(struct sockaddr_in);
    sc = accept(sl, (struct sockaddr *)&caddr, (socklen_t *)&addr_len);

    if(sc == -1) {
      perror("accept");
      continue;
    }

    printf("Conectado com o cliente %s:%d\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
    bzero(request, MAX_REQ_LEN);

    if(pthread_create(&tid[i++], NULL, socketThread, &sc) != 0){
      perror("thread");
    }

    if(i >= 30) {
      i = 0;

      while(i < 30) {
        pthread_join(tid[i++], NULL);
      }

      i = 0;
    }
  };
  close(sl);

  return 0;
};