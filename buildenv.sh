#!/bin/bash

# this script can be ran on Debian/Ubuntu series Linux

# Put corresponding BerkeleyDB source code under current dir first
# since we cannot download BDB code from Oracle directly.
# Damned Oracle!

gputs()
{
    printf "\033[01;32m$1\033[00m\n"
}

cputs()
{
    printf "\033[01;36;40m$1\033[00m\n"
}

warn_puts()
{
    printf "\033[01;33;41m$1\033[00m\n"
}

print_help()
{
    echo "Usage:"
    echo "	-c		Clean environment including datanode.log, data/ and generated reports"
    echo "	--rebuild	Clean environment and make"
    echo "	--cleanall	Delete all unused files and directories including tmp/ and utils/"
    echo "	-d		Display necessary env"
    echo "	--install	Download and install third-party softwares"
    echo "	-i		Print machine hard info"
}

run_cmd()
{
    printf "\033[01;32m$1\033[00m\n"
    printf "\033[01;30;40m"
    $1
    if [ $? != 0 ]
    then
        printf "\033[01;33;41mFailed: $1\033[00m\n"
        exit 1
    fi
    printf "\033[00m"
}

print_hardinfo()
{
    printf "HOSTNAME\n"
    printf "CPUINFO\n"
    printf "FREE MEM\n"
    printf "HARD DISK INFO(sda)\n"
    printf "MOUNT INFO\n"
    printf "KERNEL CONFIG\n\n"

    printf "HOSTNAME\n"
    echo "--------"
    hostname
    echo ""

    printf "CPUINFO\n"
    echo "-------"
    cat /proc/cpuinfo
    echo ""

    printf "FREE MEM\n"
    echo "--------"
    free -m
    echo ""

    printf "HARD DISK INFO(sda)\n"
    echo "------------------"
    sudo hdparm -i /dev/sda
    sudo hdparm -I /dev/sda
    echo ""

    printf "MOUNT INFO\n"
    echo   "----------"
    mount
    echo ""

    printf "KERNEL CONFIG\n"
    echo "-------------"
    cat /sys/block/sda/queue/scheduler
    cat /sys/block/sda/queue/nr_requests
    cat /sys/block/sda/queue/read_ahead_kb
}

DEPLOY_ROOT_DIR=`pwd`

if [ -z $1 ]
then
    print_help
fi

if [ ! -z $1 ]
then
    if [ $1 = '-c' ]
    then
        gputs "--- CLEANUP ENV ---"
        run_cmd "rm -rf data"
        run_cmd "make clean"
        run_cmd "rm -f datanode.log"
        run_cmd "rm -f core"
        run_cmd "rm -f cscope.out"
        run_cmd "rm -f tags"
        run_cmd "rm -f report/dist.png"
        run_cmd "rm -f report/timeline.png"
        run_cmd "rm -f report/gnuplot.dat"
        run_cmd "rm -f test/integration/log.*"
    elif [ $1 = '--rebuild' ]
    then
        run_cmd "rm -rf data"
        run_cmd "make clean"
        run_cmd "rm -f datanode.log"
        run_cmd "rm -f core"
        run_cmd "rm -f cscope.out"
        run_cmd "rm -f tags"
        run_cmd "rm -f report/dist.png"
        run_cmd "rm -f report/timeline.png"
        run_cmd "rm -f report/gnuplot.dat"
        run_cmd "rm -f test/integration/log.*"
        run_cmd "make"
    elif [ $1 = '--cleanall' ]
    then
        gputs "--- CLEAN ALL FILES ---"
        run_cmd "rm -rf tmp"
        run_cmd "rm -rf utils"
        run_cmd "rm -rf data"
        run_cmd "make clean"
        run_cmd "rm -f datanode.log"
        run_cmd "rm -f core"
        run_cmd "rm -f cscope.out"
        run_cmd "rm -f tags"
    elif [ $1 == '--help' ]
    then
        print_help
    elif [ $1 == '-i' ]
    then
        print_hardinfo
    fi
fi

if [ ! -z $1 ]
then
    if [ $1 = '-d' ]
    then
        gputs "--- UPDATE YOUR ENV MANUALLY ---"
        gputs "1. For bash"
        cputs "export C_INCLUDE_PATH='$DEPLOY_ROOT_DIR/utils/include'"
        cputs "export LIBRARY_PATH='$DEPLOY_ROOT_DIR/utils/lib'"
        cputs "export LD_LIBRARY_PATH='$DEPLOY_ROOT_DIR/utils/lib'"

        gputs "2. For tcsh"
        cputs "setenv C_INCLUDE_PATH $DEPLOY_ROOT_DIR/utils/include"
        cputs "setenv LIBRARY_PATH $DEPLOY_ROOT_DIR/utils/lib"
        cputs "setenv LD_LIBRARY_PATH $DEPLOY_ROOT_DIR/utils/lib"
    fi
fi

if [ ! -z $1 ]
then
    if [ $1 = '--install' ]
    then
        gputs "--- INSTALL GNU Parallel ---"
        run_cmd "mkdir -p tmp"
        run_cmd 'wget -P tmp http://mirrors.ispros.com.bd/gnu/parallel/parallel-20111222.tar.bz2'
        run_cmd "mkdir -p utils"
        run_cmd "cd tmp"
        run_cmd "tar xfvj parallel-20111222.tar.bz2"
        run_cmd "cd parallel-20111222"
        run_cmd "./configure --prefix=$DEPLOY_ROOT_DIR/utils"
        run_cmd "make"
        run_cmd "make install"

        gputs "--- INSTALL UTHASH ---"
        run_cmd "cd $DEPLOY_ROOT_DIR"
        run_cmd "wget -O tmp/uthash.tar.bz2 http://downloads.sourceforge.net/project/uthash/uthash/uthash-1.9.5/uthash-1.9.5.tar.bz2?r=http%3A%2F%2Futhash.sourceforge.net%2F&ts=1325312087&use_mirror=cdnetworks-kr-2"
        run_cmd "cd tmp"
        run_cmd "tar xfvj uthash.tar.bz2"

        gputs "--- INSTALL BerkeleyDB ---"
        run_cmd "cd $DEPLOY_ROOT_DIR"
        if [ -e db-5.2.36.NC.tar.gz ]
        then
            run_cmd "mv db-5.2.36.NC.tar.gz tmp/"
        elif [ -e $HOME/software/db-5.2.36.NC.tar.gz ]
        then
            run_cmd "cp $HOME/software/db-5.2.36.NC.tar.gz tmp/"
        else
            warn_puts "cannot find BerkeleyDB source code"
            exit -1
        fi
        run_cmd "cd tmp"
        run_cmd "tar xfvz db-5.2.36.NC.tar.gz"
        run_cmd "cd db-5.2.36.NC/build_unix"
        run_cmd "../dist/configure --prefix=$DEPLOY_ROOT_DIR/utils/"
        run_cmd "make -j4"
        run_cmd "make install"

        gputs "--- INSTALL CUnit ---"
        run_cmd "cd $DEPLOY_ROOT_DIR"
        run_cmd "cd tmp"
        run_cmd "wget -O Cunit.tar.bz2 http://downloads.sourceforge.net/project/cunit/CUnit/2.1-2/CUnit-2.1-2-src.tar.bz2?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fcunit%2F&ts=1324818822&use_mirror=cdnetworks-kr-1"
        run_cmd "tar xfvj Cunit.tar.bz2"
        run_cmd "cd CUnit-2.1-2"
        run_cmd "./configure --prefix=$DEPLOY_ROOT_DIR/utils"
        run_cmd "make"
        run_cmd "make install"

        gputs "If you want to run PHP integration test scripts, please run the following command(on Debian/Ubuntu) first"
        gputs "sudo apt-get -y install php5-memcached"

        gputs "--- UPDATE YOUR ENV MANUALLY ---"
        gputs "1. For bash"
        cputs "export C_INCLUDE_PATH='$DEPLOY_ROOT_DIR/utils/include'"
        cputs "export LIBRARY_PATH='$DEPLOY_ROOT_DIR/utils/lib'"
        cputs "export LD_LIBRARY_PATH='$DEPLOY_ROOT_DIR/utils/lib'"

        gputs "2. For tcsh"
        cputs "setenv C_INCLUDE_PATH $DEPLOY_ROOT_DIR/utils/include"
        cputs "setenv LIBRARY_PATH $DEPLOY_ROOT_DIR/utils/lib"
        cputs "setenv LD_LIBRARY_PATH $DEPLOY_ROOT_DIR/utils/lib"
    fi
fi
