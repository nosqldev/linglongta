<?php
$m = new Memcached();
$m->addServer('localhost', 5000);
$v = $m->get("hello");
if ($v == 'world')
{
    echo "\033[01;32mPASSED\033[00m\n";
}
else
{
    echo "\033[01;33;41mFAILED\033[00m\n";
}

$m = new Memcached();
$m->addServer('localhost', 5000);
$v = $m->get("1234567890");
if ($v == 'abcabc')
{
    echo "\033[01;32mPASSED\033[00m\n";
}
else
{
    echo "\033[01;33;41mFAILED\033[00m\n";
}

$m = new Memcached();
$m->addServer('localhost', 5000);
$v = $m->get("1234567890123");
if ($v == 'xyz')
{
    echo "\033[01;32mPASSED\033[00m\n";
}
else
{
    echo "\033[01;33;41mFAILED\033[00m\n";
}
?>
