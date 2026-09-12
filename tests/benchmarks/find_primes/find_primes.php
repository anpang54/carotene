<?php

$n = 10000;

function is_prime($n) {
    if($n == 0 || $n == 1) {
        return false;
    }
    for($i = 2; $i < $n; ++$i) {
        if($n % $i == 0) {
            return false;
        }
    }
    return true;
}

for($i = 0; $i < $n; ++$i) {
    if(is_prime($i)) {
        // echo $i . "\n";
    }
}