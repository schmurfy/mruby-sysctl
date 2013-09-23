
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
// #include <error.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <net/route.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include <strings.h>

#include "mruby.h"
#include "mruby/data.h"
#include "mruby/array.h"
#include "mruby/string.h"
#include "mruby/hash.h"

// struct state {
//   int s;
// };


// static void state_free(mrb_state *mrb, void *ptr)
// {
//   struct state *st = (struct state *)ptr;
//   if( s != 0 ){
//     close(s);
//     s = 0;
//   }
// }

// static struct mrb_data_type state_type = { "State", state_free };


// static mrb_value sysctl_initialize(mrb_state *mrb, mrb_value self)
// {
//   struct state *st = mrb_malloc(mrb, sizeof(struct state));
//   const char *s;
//   mrb_int opt;
    
//   mrb_get_args(mrb, "z|i", &s, &opt);
  
//   s = socket(AF_LOCAL, SOCK_DGRAM, 0);
    
//   DATA_PTR(self)  = (void*)st;
//   DATA_TYPE(self) = &state_type;
    
// ret:
//   return self;
// }



static int sockaddr_to_string(struct sockaddr *sa, char *buffer, size_t buffer_size)
{
  void *addr_data;
  
  if( sa->sa_family == AF_INET ){
    addr_data = &((struct sockaddr_in *)sa)->sin_addr;
  }
  else {
    addr_data = &((struct sockaddr_in6 *)sa)->sin6_addr;
  }
  
  if( inet_ntop(sa->sa_family, addr_data, buffer, buffer_size) == NULL){
    return -1;
  }
  
  return 0;
}

static mrb_value sysctl_net_list(mrb_state *mrb, mrb_value self)
{
  const char *ifname = NULL;
  struct ifaddrs *addrs, *curr;
  mrb_value r_ret;
  struct RClass *if_class = mrb_class_get(mrb, "NetworkAddress");
  
  mrb_get_args(mrb, "|z", &ifname);
  
  if( getifaddrs(&addrs) == -1 ){
    mrb_raisef(mrb, E_RUNTIME_ERROR, "getifaddrs: %S", mrb_str_new_cstr(mrb, strerror(errno)));
    goto ret;
  }
  
  if( ifname ){
    r_ret = mrb_ary_new(mrb);
  }
  else {
    r_ret = mrb_hash_new(mrb);
  }
  
  curr = addrs;
  
  while( curr != NULL ){
    char addr[INET6_ADDRSTRLEN];
    char netmask[INET6_ADDRSTRLEN];
    
    if( curr->ifa_netmask != NULL ){
      mrb_value r_key, r_addr, r_list;
      
      // only return wanted infos
      if( !ifname || !strcmp(ifname, curr->ifa_name) ){
        if( sockaddr_to_string(curr->ifa_addr, addr, sizeof(addr)) == -1)
          continue;
        
        if( sockaddr_to_string(curr->ifa_netmask, netmask, sizeof(netmask)) == -1)
          continue;
        
        r_key = mrb_str_new_cstr(mrb, curr->ifa_name);
        r_addr = mrb_funcall(mrb, mrb_obj_value(if_class), "new", 2,
            mrb_str_new_cstr(mrb, addr),
            mrb_str_new_cstr(mrb, netmask)
          );
        
        if( ifname ){
          mrb_ary_push(mrb, r_ret, r_addr);
        }
        else {
          // check if key exists
          r_list = mrb_hash_get(mrb, r_ret, r_key);
          if( mrb_nil_p(r_list) ){
            r_list = mrb_ary_new(mrb);
          }
          
          mrb_ary_push(mrb, r_list, r_addr);
          
          mrb_hash_set(mrb, r_ret, r_key, r_list);
        }
      }
    }
    
    curr = curr->ifa_next;
  }

ret:
  return r_ret;
}


static void set_addr4(struct sockaddr *sa, const char *addr)
{
  struct sockaddr_in *s = (struct sockaddr_in*)sa;
  
  bzero(s, sizeof(struct sockaddr_in));
  
  s->sin_len    = sizeof(*s);
  s->sin_family = AF_INET;
  s->sin_addr.s_addr = inet_addr(addr);
}

static void fill_broadcast4(struct ifaliasreq *r)
{
  struct sockaddr_in *addr, *netmask, *bcast;
  
  addr = (struct sockaddr_in *)&r->ifra_addr;
  netmask = (struct sockaddr_in *)&r->ifra_mask;
  bcast = (struct sockaddr_in *)&r->ifra_broadaddr;
  
  // printf("%x %x\n", bcast->sin_addr.s_addr,
  //     addr->sin_addr.s_addr | ~netmask->sin_addr.s_addr
  //   );
  bcast->sin_addr.s_addr = addr->sin_addr.s_addr | ~netmask->sin_addr.s_addr;
}

static mrb_value sysctl_net_add_addr4(mrb_state *mrb, mrb_value self)
{
  int s;
  struct ifaliasreq newaddr;
  const char *ifname, *addr, *broadcast = NULL, *netmask;
  
  mrb_get_args(mrb, "zzz|z", &ifname, &addr, &netmask, &broadcast);
  
  strlcpy(newaddr.ifra_name, ifname, sizeof(newaddr.ifra_name));
  set_addr4(&newaddr.ifra_addr,       addr);
  set_addr4(&newaddr.ifra_mask,       netmask);
  
  if( broadcast ){
    set_addr4(&newaddr.ifra_broadaddr,  broadcast);
  }
  else {
    fill_broadcast4(&newaddr);
  }
    
  s = socket(AF_INET, SOCK_DGRAM, 0);
  
  // see SIOCAIFADDR_IN6 for ipv6
  if( ioctl(s, SIOCAIFADDR, &newaddr) == -1){
    mrb_raisef(mrb, E_RUNTIME_ERROR, "ioctl: %S", mrb_str_new_cstr(mrb, strerror(errno)));
  }
  
  close(s);
  return mrb_nil_value();
}

static mrb_value sysctl_net_del_addr4(mrb_state *mrb, mrb_value self)
{
  int s;
  struct ifaliasreq addr;
  const char *ifname, *address, *broadcast = NULL, *netmask;
  
  mrb_get_args(mrb, "zzz|z", &ifname, &address, &netmask, &broadcast);
  
  strlcpy(addr.ifra_name, ifname, sizeof(addr.ifra_name));
  set_addr4(&addr.ifra_addr,       address);
  set_addr4(&addr.ifra_mask,       netmask);
  
  if( broadcast ){
    set_addr4(&addr.ifra_broadaddr,  broadcast);
  }
  else {
    fill_broadcast4(&addr);
  }
  
  
  s = socket(AF_INET, SOCK_DGRAM, 0);
  
  // see SIOCAIFADDR_IN6 for ipv6
  if( ioctl(s, SIOCDIFADDR, &addr) == -1){
    mrb_raisef(mrb, E_RUNTIME_ERROR, "ioctl: %S", mrb_str_new_cstr(mrb, strerror(errno)));
  }
  
  close(s);
  return mrb_nil_value();
}


void mrb_mruby_sysctl_gem_init(mrb_state *mrb)
{
  // struct RClass *mod = mrb_define_module(mrb, "SysCtl");
  // struct RClass *class = mrb_define_class_under(mrb, mod, "NetworkInterfaces", NULL);
  struct RClass *class = mrb_define_class(mrb, "NetworkInterfaces", NULL);
  
  int ai = mrb_gc_arena_save(mrb);
  
  // mrb_define_method(mrb, class, "initialize", sysctl_initialize, ARGS_REQ(1));
  // mrb_define_method(mrb, class, "list", sysctl_list, ARGS_REQ(1));
  // mrb_define_method(mrb, class, "capture", pcap_capture, ARGS_NONE());
  
  mrb_define_class_method(mrb, class, "list", sysctl_net_list, ARGS_NONE());
  mrb_define_class_method(mrb, class, "add_addr4", sysctl_net_add_addr4, ARGS_REQ(3));
  mrb_define_class_method(mrb, class, "del_addr4", sysctl_net_del_addr4, ARGS_REQ(3));
      
  mrb_gc_arena_restore(mrb, ai);

}

void mrb_mruby_sysctl_gem_final(mrb_state* mrb)
{
  
}
