#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

#define STDIN_READ_SIZE 8192

int verbose = 0;

void print_help() {
  printf("A netcat like utility for ZMQ\n");
  printf("Usage: zc [OPTION]... TYPE ENDPOINT\n");
  printf("OPTION: one of the following options:\n");
  printf(" -h --help : print this message\n");
  printf(" -b --bind : bind instead of connect\n");
  printf(" -n --nbiter : number of iterations (0 for infinite loop)\n");
  printf(" -v --verbose : print some messages in stderr\n");
  printf("TYPE: set ZMQ socket type in req/rep/pub/sub/push/pull\n");
  printf("ENDPOINT: a string consisting of two parts as follows: transport://address (see zmq documentation)\n");
}

void exit_with_zmq_error(const char* where) {
    fprintf(stderr,"%s error %d: %s\n",where,errno,zmq_strerror(errno));
    exit(-1);
}

void free_buffer(void* data,void* hint) {
  free(data);
}

size_t send_from_stdin(void* socket) {
  size_t total = 0;
  // read from stdin
  char* buffer = malloc(STDIN_READ_SIZE);
  while(1) {
    size_t read = fread(buffer+total, 1, STDIN_READ_SIZE, stdin);
    total += read;
    if(ferror(stdin)) {
      fprintf(stderr,"fread error %d: %s\n",errno,strerror(errno));
      exit(-2);
    }
    if(feof(stdin)) break;
    buffer = realloc(buffer,total+STDIN_READ_SIZE);
  }
  // prepare and send zmq message
  int err;
  zmq_msg_t msg;
  err = zmq_msg_init_data(&msg, buffer, total, free_buffer, NULL);
  if(err) exit_with_zmq_error("zmq_msg_init_data");

  if(verbose) fprintf(stderr, "sending %d bytes\n", total);

  err = zmq_sendmsg(socket, &msg, 0);
  if(err==-1) exit_with_zmq_error("zmq_sendmsg");

  return total;
}

size_t recv_to_stdout(void* socket) {
  size_t total = 0;
  int err;
  int64_t more = 1;
  size_t more_size = sizeof(more);
  while(more) {
    zmq_msg_t msg;

    zmq_msg_init(&msg);
    if(err) exit_with_zmq_error("zmq_msg_init");
    
    err = zmq_recvmsg(socket, &msg, 0);
    if(err==-1) exit_with_zmq_error("zmq_recvmsg");

    // print message to stdout
    size_t size = zmq_msg_size(&msg);
    total += size;
    if(verbose) fprintf(stderr, "receiving %d bytes\n", size);
    fwrite(zmq_msg_data(&msg), 1, size, stdout);

    err = zmq_msg_close(&msg);
    if(err) exit_with_zmq_error("zmq_msg_close");

    // check for multipart messages
    zmq_getsockopt(socket, ZMQ_RCVMORE, &more, &more_size);
  }
  return total;
}

int main(int argc, char** argv) {

  static struct option options[] = {
    {"help",    no_argument,       0, 'h'},
    {"bind",    no_argument,       0, 'b'},
    {"nbiter",  required_argument, 0, 'n'},
    {"verbose", no_argument,       0, 'v'},
    {0, 0, 0, 0}
  };

  int bind = 0;
  int type = 0;
  char* endpoint = 0;
  int nbiter = 0;

  int iopt = 0;
  while(1) {
     int c = getopt_long(argc,argv,"hbn:v",options, &iopt);
     if(c==-1)
       break;
     switch(c) {
       case 'h':
         print_help();
         exit(0);
         break;
       case 'b':
         bind = 1;
         break;
       case 'n':
         nbiter = atoi(optarg);
         break;
       case 'v':
         verbose = 1;
         break;
     }
  }
  if(optind<argc) {
    char* str_type = argv[optind++];
    if (!strcasecmp(str_type,"req"))
      type = ZMQ_REQ;
    else if (!strcasecmp(str_type,"rep"))
      type = ZMQ_REP;
    else if (!strcasecmp(str_type,"pub"))
      type = ZMQ_PUB;
    else if (!strcasecmp(str_type,"sub"))
      type = ZMQ_SUB;
    else if (!strcasecmp(str_type,"push"))
      type = ZMQ_PUSH;
    else if (!strcasecmp(str_type,"pull"))
      type = ZMQ_PULL;
  }
  if(optind<argc) {
    endpoint = argv[optind++];
  }

  if(!type || !endpoint) {
    print_help();
    return -1;
  }

  void *ctx = zmq_init(1);
  if(ctx == NULL) exit_with_zmq_error("zmq_init");

  void *socket = zmq_socket(ctx, type);
  if(socket == NULL) exit_with_zmq_error("zmq_socket");

  int err;
  if(bind) {
    err = zmq_bind(socket,endpoint);
    if(err) exit_with_zmq_error("zmq_bind");
    if(verbose) fprintf(stderr,"bound to %s\n",endpoint);
  }
  else {
    err = zmq_connect(socket,endpoint);
    if(err) exit_with_zmq_error("zmq_connect");
    if(verbose) fprintf(stderr,"connected to %s\n",endpoint);
  }

  if(type==ZMQ_SUB) {
    zmq_setsockopt(socket,ZMQ_SUBSCRIBE,"",0); 
  }

  if(type==ZMQ_PUB) {
    usleep(10000); // let a chance for the connection to be established before sending a message
  }

  int i = 0;
  while(nbiter==0 || i++<nbiter) {
    switch(type) {
      case ZMQ_REQ:
      case ZMQ_PUB:
      case ZMQ_PUSH:
        send_from_stdin(socket);
    }
    switch(type) {
      case ZMQ_REQ:
      case ZMQ_REP:
      case ZMQ_SUB:
      case ZMQ_PULL:
        recv_to_stdout(socket);
    }
    switch(type) {
      case ZMQ_REP:
        send_from_stdin(socket);
    }
  }

  err = zmq_close(socket);
  if(err) exit_with_zmq_error("zmq_close");

  err = zmq_term(ctx);
  if(err) exit_with_zmq_error("zmq_term");

  return 0;
}
