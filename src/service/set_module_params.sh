#!/usr/bin/env bash

DEV_NAME='module_messy'
m=0
s=0

usage() {
  cat <<EOF >&1
    Usage: $0 [options]
    Allow to set all CoreMess module parameters.
    Options:
      -m max_message_size   Set message size limit. Default = 128B
      -s max_storage_size   Set total storage size limit. Default = 1MB
      -h help               Print all the CoreMess opts.
EOF
  exit 1
}

exit_with_shame(){
  echo 'Could not cd. Sorry'
  sleep 1
  exit 1
}

find_module(){
	cd /sys/module || exit_with_shame
	find ./ -name $DEV_NAME -print
}

while getopts "m:s:h:" o;do
	case $o in
	  m) m=$OPTARG;;
	  s) s=$OPTARG;;
	  h) usage;;
	  *) usage
	esac
done
shift "$((OPTIND - 1))"
echo 'm' $m 's' $s 

#max_message_size
if [ ! $m -eq 0 ]
then
  echo "Setting new value of max_message_size"
  cd /sys/module/$DEV_NAME/parameters/ || exit_with_shame
  ls
  echo $m > ./max_message_size
  cat ./max_message_size
fi

#max_storage_size
if [ ! $s -eq 0 ]
then
  echo "Setting new value of max_storage_size"
  cd /sys/module/$DEV_NAME/parameters/ || exit_with_shame
  ls
  echo $s > ./max_storage_size
  cat ./max_storage_size
fi
