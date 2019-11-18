# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(cache-test-2) begin
(cache-test-2) mkdir "a"
(cache-test-2) create "a/b"
(cache-test-2) fist flush sucess
(cache-test-2) second flush nothing
(cache-test-2) end
EOF
pass;