#!/usr/bin/env bash

if [ ! -d "find_testbed/testbed" ]; then
  if [ ! -f "find_testbed.tar.bz2"]; then
    echo "downloading testbed..."
    curl -o find_testbed.tar.bz2 https://os.inf.tu-dresden.de/Studium/SysProg/Tasks/Find/find_testbed.tar.bz2 || exit 1
  fi
  echo "unpacking testbed..."
  tar xvf find_testbed.tar.bz2 || exit 1

  echo "creating testbed..."
  find_testbed/create_testbed.sh || exit 1
fi

if mount | grep $(realpath find_testbed/testbed/basic/MNT); then
    echo "found xdev mountpoint"
else
  read -p "mount image to test -xdev? (y/n) [n]: " yn
  case $yn in
      [Yy]* ) sudo mount find_testbed/xdev.img find_testbed/testbed/basic/MNT;;
      * ) echo "xdev results won't be accurate!";;
  esac
fi

echo "--- Testscript will print diff of mydiff vs. gnu find ---"
echo "empty output => pass"

echo "==================[ Test basic ]=================="
./test.sh find_testbed/testbed

echo "==================[ Test trailing slash ]=================="
./test.sh find_testbed/testbed/

echo "==================[ Test nonexistent ]=================="
./test.sh find_testbed/nonexistent

echo "==================[ Test nonexistent absolute ]=================="
./test.sh /nonexistent

echo "==================[ Test tilde ]=================="
mkdir ~/test
mkdir ~/test/a1
mkdir ~/test/a2
./test.sh ~/test
rmdir ~/test/a1
rmdir ~/test/a2
rmdir ~/test

echo "==================[ Test \$HOME ]=================="
mkdir $HOME/test
mkdir $HOME/test/a1
mkdir $HOME/test/a2
./test.sh $HOME/test
rmdir $HOME/test/a1
rmdir $HOME/test/a2
rmdir $HOME/test

echo "==================[ Test name ]=================="
./test.sh find_testbed/testbed -name bb1

echo "==================[ Test pattern * ]=================="
./test.sh find_testbed/testbed -name b*

echo "==================[ Test pattern ? ]=================="
./test.sh find_testbed/testbed -name b??

echo "==================[ Test pattern [] ]=================="
./test.sh find_testbed/testbed -name BBB[12]

echo "==================[ Test follow ]=================="
./test.sh find_testbed/testbed -follow

echo "==================[ Test follow+name ]=================="
./test.sh find_testbed/testbed -follow -name bb1

echo "==================[ Test follow+pattern * ]=================="
./test.sh find_testbed/testbed -follow -name b*

echo "==================[ Test follow+pattern ? ]=================="
./test.sh find_testbed/testbed -follow -name b??

echo "==================[ Test follow+pattern [] ]=================="
./test.sh find_testbed/testbed -follow -name BBB[12]

echo "==================[ Test xdev ]=================="
./test.sh find_testbed/testbed -xdev

echo "==================[ Test xdev+name incorrect ]=================="
./test.sh find_testbed/testbed -xdev -name bb1

echo "==================[ Test xdev+name incorrect ]=================="
./test.sh find_testbed/testbed -name bb1 -xdev

echo "==================[ Test xdev+pattern * ]=================="
./test.sh find_testbed/testbed -xdev -name b*

echo "==================[ Test xdev+pattern ? ]=================="
./test.sh find_testbed/testbed -xdev -name b??

echo "==================[ Test xdev+pattern [] ]=================="
./test.sh find_testbed/testbed -xdev -name BBB[12]

echo "==================[ Test follow+xdev ]=================="
./test.sh find_testbed/testbed -follow -xdev

echo "==================[ Test follow+xdev+name ]=================="
./test.sh find_testbed/testbed -xdev -follow -name bb1

echo "==================[ Test follow+xdev+pattern * ]=================="
./test.sh find_testbed/testbed -xdev -follow -name b*

echo "==================[ Test follow+xdev+pattern ? ]=================="
./test.sh find_testbed/testbed -xdev -follow -name b??

echo "==================[ Test follow+xdev+pattern [] ]=================="
./test.sh find_testbed/testbed -xdev -follow -name BBB[12]

echo "==================[ Test stderr nonexistent ]=================="
./test-streams.sh find_testbed/nonexistent

echo "==================[ Test stderr nonexistent absolute ]=================="
./test-streams.sh /nonexistent

echo "==================[ Test stderr xdev position ]=================="
./test-streams.sh find_testbed/testbed -name bb1 -xdev

echo "==================[ Test permission ]=================="
mkdir permtest
chmod 000 permtest
./test.sh permtest
rmdir permtest

echo "==================[ Test permission with subdir ]=================="
mkdir permtest
mkdir permtest/foo
chmod 000 permtest
./test.sh permtest
chmod 777 permtest
rmdir permtest/foo
rmdir permtest

echo "==================[ Test subdir permission ]=================="
mkdir permtest
mkdir permtest/foo
chmod 000 permtest/foo
./test.sh permtest
rmdir permtest/foo
rmdir permtest

echo "==================[ Test home permission ]=================="
mkdir ~/permtest
chmod 000 ~/permtest
./test.sh ~/permtest
rmdir ~/permtest

echo "==================[ Test home permission with subdir ]=================="
mkdir ~/permtest
mkdir ~/permtest/foo
chmod 000 ~/permtest
./test.sh ~/permtest
chmod 777 ~/permtest
rmdir ~/permtest/foo
rmdir ~/permtest

echo "==================[ Test home subdir permission ]=================="
mkdir ~/permtest
mkdir ~/permtest/foo
chmod 000 ~/permtest/foo
./test.sh ~/permtest
rmdir ~/permtest/foo
rmdir ~/permtest

rm find.txt find2.txt me.txt me2.txt 2>/dev/null

if mount | grep $(realpath find_testbed/testbed/basic/MNT); then
  echo "-------------------------"
  echo "to unmount xdev image run:"
  echo "sudo umount $(realpath find_testbed/testbed/basic/MNT)"
fi