#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <linux/netfilter_ipv4/ip_tproxy.h>

/* an IP address bound to one of the local interfaces */
#define LOCAL_IP "1.2.3.4"

/* the IP address to use as source address */
#define FOREIGN_IP "10.0.0.1"

/* IP address to connect to */
#define DEST_IP  "192.168.1.1"

int main()
{
  int sock;
  struct sockaddr_in sin;
  struct in_tproxy itp;
  
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1)
    {
      perror("socket");
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
  
  /* assign foreign address, specify exact foreign port */
  itp.op = TPROXY_ASSIGN;
  inet_aton(FOREIGN_IP, (struct in_addr *) &itp.v.addr.faddr);
  itp.v.addr.fport = htons(2000);
  
  if (setsockopt(sock, SOL_IP, IP_TPROXY, &itp, sizeof(itp)) == -1)
    {
      perror("setsockopt(SOL_IP, IP_TPROXY, TPROXY_ASSIGN)");
      return -1;
    }
  
  /* set connect flag on socket */
  itp.op = TPROXY_FLAGS;
  itp.v.flags = ITP_CONNECT;
  if (setsockopt(sock, SOL_IP, IP_TPROXY, &itp, sizeof(itp)) == -1)
    {
      perror("setsockopt(SOL_IP, IP_TPROXY, TPROXY_FLAGS)");
      return -1;
    }
  
  /* connect */
  inet_aton(DEST_IP, &sin.sin_addr);
  sin.sin_port = htons(10000);
  if (connect(sock, (struct sockaddr *) &sin, sizeof(sin)) == -1)
    {
      perror("connect");
      return -1;
    }
  
  write(sock, "1234\n", 5);
  
  close(sock);
  return 0;
}

