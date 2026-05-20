sp        = [ \t]+;
host      = @h1 [0-9.]+ @h2;
userid    = @i1 [-] @i2;
authuser  = @a1 [-] @a2;
date      = @d1 "[" [^\x00\n\]]+ "]" @d2;
request   = @r1 ["] [^\x00\n"]+ ["] @r2;
status    = @s1 [0-9]+ @s2;
size      = @z1 ([0-9]+ | '-') @z2;
url       = @u1 ["] [^\x00\n"]* ["] @u2;
useragent = @g1 ["] [^\x00\n"]* ["] @g2;
line    =
    host      sp
    userid    sp
    authuser  sp
    date      sp
    request   sp
    status    sp
    size      sp
    url       sp
    useragent [\n];
