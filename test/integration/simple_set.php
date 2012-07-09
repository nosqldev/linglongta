<?php
$m = new Memcached();
$m->addServer('localhost', 5000);

if ($m->set("hello", "world"))
{
    echo "\033[01;32mPASSED\033[00m\n";
}
else
{
    echo "\033[01;33;41mFAILED\033[00m\n";
}

$m = new Memcached();
$m->addServer('localhost', 5000);
if ($m->set("1234567890", "abcabc"))
{
    echo "\033[01;32mPASSED\033[00m\n";
}
else
{
    echo "\033[01;33;41mFAILED\033[00m\n";
}

$m = new Memcached();
$m->addServer('localhost', 5000);
if ($m->set("1234567890123", "xyz"))
{
    echo "\033[01;32mPASSED\033[00m\n";
}
else
{
    echo "\033[01;33;41mFAILED\033[00m\n";
}
?>
