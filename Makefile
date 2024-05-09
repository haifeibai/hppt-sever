all: httpd
  
httpd: httpd.c
        gcc -g -W -Wall -pthread -o httpd httpd.c

clean:
        rm httpd