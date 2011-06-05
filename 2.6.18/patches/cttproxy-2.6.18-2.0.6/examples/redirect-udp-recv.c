#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <arpa/inet.h>

#include <linux/netfilter_ipv4/ip_tproxy.h>

/* Local IP address to bind to */
#define DEST_IP  "192.168.1.1"


int main()
{
  int sock;
  struct msghdr msg;
  struct cmsghdr *cmsg;
  char buf[1024];
  char msgbuf[3000];
  struct iovec iov;
  size_t len = sizeof(buf);
  struct sockaddr_in sa;
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
  sa.sin_family = AF_INET;
  inet_aton("192.168.131.124", &sa.sin_addr);
  sa.sin_port = htons(9999);
  
  if (bind(sock, (struct sockaddr *) &sa, sizeof(sa)) == -1)
    {
      perror("bind");
      return -1;
    }
  
  /* set RECVORIGADDRS socket option, to be able to get original destination address
   * through recvmsg() */
  memset(&msg, 0, sizeof(msg));
  msg.msg_controllen = sizeof(buf);
  msg.msg_control = buf;
  msg.msg_iovlen = 1;
  msg.msg_iov = &iov;
  iov.iov_base = msgbuf;
  iov.iov_len = sizeof(msgbuf);

  if (setsockopt(sock, SOL_IP, IP_RECVORIGADDRS, &len, sizeof(len)) == -1)
    {
      perror("setsockopt(SOL_IP, IP_RECVORIGADDRS)");
    }
    
  while (1)
    {
      recvmsg(sock, &msg, 0);
  
      for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg,cmsg))
        {
      
          printf("level=%d, type=%d\n", cmsg->cmsg_level, cmsg->cmsg_type);
          if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_PKTINFO)
            {
              struct in_pktinfo *info = (struct in_pktinfo *) CMSG_DATA(cmsg);
              
              printf("addr=%08x, peer=%08x, ifi=%d\n", info->ipi_addr.s_addr, info->ipi_spec_dst.s_addr, info->ipi_ifindex);
            }
          else if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_ORIGADDRS)
            {
              struct in_origaddrs *ioa = (struct in_origaddrs *) CMSG_DATA(cmsg);
              char addr1[32], addr2[32];
              
              strcpy(addr1, inet_ntoa(ioa->ioa_srcaddr));
              strcpy(addr2, inet_ntoa(ioa->ioa_dstaddr));
              
              printf("src=%s:%d, dst=%s:%d\n", 
                     addr1, ntohs(ioa->ioa_srcport), 
                     addr2, ntohs(ioa->ioa_dstport));
            }
        }
    }
}

