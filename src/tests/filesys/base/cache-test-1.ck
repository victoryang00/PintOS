# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(cache-test-1) begin
(cache-test-1) mkdir "a"
(cache-test-1) cache number <= 64
(cache-test-1) create "a/b"
(cache-test-1) cache number <= 64
(cache-test-1) end
EOF
pass;