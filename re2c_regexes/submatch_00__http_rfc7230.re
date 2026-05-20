eol            = "\n";
crlf           = eol;
sp             = " ";
htab           = "\t";
ows            = (sp | htab)*;
digit          = [0-9];
alpha          = [a-zA-Z];
hexdigit       = [0-9a-fA-F];
unreserved     = alpha | digit | [-._~];
pct_encoded    = "%" hexdigit{2};
sub_delims     = [!$&'()*+,;=];
pchar          = unreserved | pct_encoded | sub_delims | [:@];
vchar          = [\x1f-\x7e];
tchar          = [-!#$%&'*+.^_`|~] | digit | alpha;
obs_fold       = crlf (sp | htab)+;
obs_text       = [\x80-\xff];
field_name     = tchar+;
field_vchar    = vchar | obs_text;
field_content  = field_vchar ((sp | htab)+ field_vchar)?;
field_value    = (field_content | obs_fold)*;
header_field   = #h1 field_name #h2 ":" ows #h3 field_value #h4 ows;
scheme         = alpha (alpha | digit | [-+.])*;
userinfo       = (unreserved | pct_encoded | sub_delims | ":")*;
dec_octet
    = digit
    | [\x31-\x39] digit
    | "1" digit{2}
    | "2" [\x30-\x34] digit
    | "25" [\x30-\x35];
ipv4address    = dec_octet "." dec_octet "." dec_octet "." dec_octet;
h16            = hexdigit{1,4};
ls32           = h16 ":" h16 | ipv4address;
ipv6address
    =                            (h16 ":"){6} ls32
    |                       "::" (h16 ":"){5} ls32
    | (               h16)? "::" (h16 ":"){4} ls32
    | ((h16 ":"){0,1} h16)? "::" (h16 ":"){3} ls32
    | ((h16 ":"){0,2} h16)? "::" (h16 ":"){2} ls32
    | ((h16 ":"){0,3} h16)? "::"  h16 ":"     ls32
    | ((h16 ":"){0,4} h16)? "::"              ls32
    | ((h16 ":"){0,5} h16)? "::"              h16
    | ((h16 ":"){0,6} h16)? "::";
ipvfuture      = "v" hexdigit+ "." (unreserved | sub_delims | ":" )+;
ip_literal     = "[" ( ipv6address | ipvfuture ) "]";
reg_name       = (unreserved | pct_encoded | sub_delims)*;
path_abempty   = ("/" pchar*)*;
path_absolute  = "/" (pchar+ ("/" pchar*)*)?;
path_rootless  = pchar+ ("/" pchar*)*;
path_empty     = "";
host           = ip_literal | ipv4address | reg_name;
port           = digit*;
query          = (pchar | [/?])*;
absolute_uri   = @s1 scheme @s2 ":"
    ( "//" (@u1 userinfo @u2 "@")? @hs1 host @hs2 (":" @r1 port @r2)? @p1 path_abempty @p2
    | @p3 (path_absolute | path_rootless | path_empty) @p4
    ) ("?" @q1 query @q2)?;
authority      = (@u3 userinfo @u4 "@")? @hs3 host @hs4 (":" @r3 port @r4)?;
origin_form    = @p5 path_abempty @p6 ("?" @q3 query @q4)?;
http_name      = "HTTP";
http_version   = http_name "/" digit "." digit;
request_target
    = @at authority
    | @au absolute_uri
    | @of origin_form
    | "*";
method         = tchar+;
request_line   = @m1 method @m2 sp request_target sp @v3 http_version @v4 crlf;
status_code    = digit{3};
reason_phrase  = (htab | sp | vchar | obs_text)*;
status_line    = @v1 http_version @v2 sp @st1 status_code @st2 sp @rp1 reason_phrase @rp2 crlf;
start_line     = (request_line | status_line);
message_head   = start_line (header_field crlf)* crlf;
