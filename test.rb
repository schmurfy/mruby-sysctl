puts "it worked !"

NetworkInterfaces.list("en0").each do |a|
  NetworkInterfaces.del_addr4("en0", a.address, a.netmask)
end

# NetworkInterfaces.add_addr4("en0", "192.168.1.1", "255.255.255.0")
# NetworkInterfaces.del_addr4("en0", "192.168.1.1", "255.255.255.0")
