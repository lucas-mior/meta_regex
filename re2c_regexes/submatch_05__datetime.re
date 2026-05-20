year     = @y1 (([1-9][0-9]*)? [0-9]{4}) @y2;
month    = @m1 ([1][0-2] | [0][1-9]) @m2;
day      = @d1 ([3][0-1] | [0][1-9] | [1-2][0-9]) @d2;
hours    = @h1 ([2][0-3] | [0-1][0-9]) @h2;
minutes  = @M1 [0-5][0-9] @M2;
seconds  = @s1 [0-5][0-9] @s2;
timezone = @z1 ([Z] | [+-]([2][0-3] | [0-1][0-9])[:][0-5][0-9]) @z2;
datetime = year [-] month [-] day [T] hours [:] minutes [:] seconds timezone [\n];
