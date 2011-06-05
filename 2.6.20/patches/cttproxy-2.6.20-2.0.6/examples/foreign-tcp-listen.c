#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

#include <linux/netfilter_ipv4/ip_tproxy.h>

/* an IP address bound to one of the local interfaces */
#define LOCAL_IP "1.2.3.4"

/* the IP address to listen on (non-local) */
#define FOREIGN_IP "10.0.0.1"

int sigint_received = 0;

void 
sigint(int signo)
{
  sigint_received = 1;
}
                                
int 
main()
{
  int sock;
  struct sockaddr_in sin;
  struct in_tproxy itp;
  char ch;
  int i;

  signal(SIGINT, sigint);
    
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1)
    {
      perror("socket");
      return -1;
    }
  
  i = 1;
  if (setsockopt(sock, SOL_IP, SO_REUSEADDR, &i, sizeof(i)) == -1)
    {
      perror("SO_REUSEADDR");
      return -1;
    }
  
  /* check tproxy version */
  itp.op = TPROXY_VERSION;
  itp.v.version = 0x02000000;
  if (setsockopt(sock, SOL_IP, IP_TPROXY, &itp, sizeof(itp)) == -1)
    {
      perror("setsockopt(SOL_IP, IP_TPROXY, TPROXY_VERSION)");
      return -1;
    }

  /* bind to local address */
  sin.sin_family = AF_INET;
  inet_aton(LOCAL_IP, &sin.sin_addr);
  sin.sin_port = htons(9999);
  
  if (bind(sock, (struct sockaddr *) &sin, sizeof(sin)) == -1)
    {
      perror("bind");
      return -1;
    }
    
  /* assign foreign address */
  itp.op = TPROXY_ASSIGN;
  inet_aton(FOREIGN_IP, (struct in_addr *) &itp.v.addr.faddr);
  itp.v.addr.fport = htons(2000);
  
  if (setsockopt(sock, SOL_IP, IP_TPROXY, &itp, sizeof(itp)) == -1)
    {
      perror("setsockopt(SOL_IP, IP_TPROXY, TPROXY_ASSIGN)");
      return -1;
    }
  
  /* set listen flag */
  itp.op = TPROXY_FLAGS;
  itp.v.flags = ITP_LISTEN;
  if (setsockopt(sock, SOL_IP, IP_TPROXY, &itp, sizeof(itp)) == -1)
    {
      perror("setsockopt(SOL_IP, IP_TPROXY, TPROXY_FLAGS)");
      return -1;
    }
  
  /* listen */
  if (listen(sock, 255) == -1)
    {
      perror("listen");
      return -1;
    }

  while (!sigint_received)
    {  
      int len = sizeof(sin);
      int newsock;
      
      if ((newsock = accept(sock, (struct sockaddr *) &sin, &len)) == -1)
        {
          perror("accept");
          return -1;
        }
      printf("accepted a connection: %08x:%04x\n", sin.sin_addr.s_addr, sin.sin_port);
      
      ch = 'A';
      for (i = 0; i < 1000; i++)
        {
          write(newsock, &ch, 1);
        }
      close(newsock);
    }

  close(sock);
  return 0;
}

