nl        = "\n";
char      = [-._~%!$&'()*+,;=a-zA-Z0-9];
scheme    = @s1 [-+.a-zA-Z0-9]+ @s2;
userinfo  = @u1 (char | [:])+ @u2;
host      = @h1 (char | "[" (char | [:])* "]")+ @h2;
port      = @r1 [0-9]* @r2;
path      = @p1 (char | [:@/])* @p2;
query     = @q1 (char | [:@?/])* @q2;
fragment  = @f1 (char | [:@?/])* @f2;
uri       = scheme ":"
            ("//" (userinfo "@")? host (":" port)?)?
            path ("?" query)? ("#" fragment)? nl;
