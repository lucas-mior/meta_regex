oct  = [0-9]{1,3};
dot  = [.];
ipv4 = @p1 oct dot @p2 oct dot @p3 oct dot @p4 oct [\n];
