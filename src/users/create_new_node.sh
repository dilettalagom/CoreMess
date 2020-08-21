#!/usr/bin/env bash

usage() {
  cat <<EOF >&1
    Usage: $0 [options]
    Create a new CoreMess node.
    Options:
      -n node_name        Specify the new_node device name.
      -M major number     Set the major number. Check its value in 'dmesg' command.
      -m minor number     Set the minor number. Default = 0.
      -h help             Print all opts.
EOF
  exit 1
}

exit_with_shame(){
  echo 'Could not cd. Sorry'
  sleep 1
  exit 1
}

start(){
  cd /dev/ || exit_with_shame
  sudo mknod $node_name c $major $minor
  ls ./ | grep $node_name
}

minor=0
major=0
node_name=""
while getopts "n:M:m:h:" o;do
	case $o in
	  n) node_name=$OPTARG;;
	  M) major=$OPTARG;;
	  m) minor=$OPTARG;;
	  h) usage;;
	  *) usage
	esac
done
shift "$((OPTIND - 1))"

if [ "$node_name" = "" ] || [ $major -eq 0 ]
then
  echo "Please, select a valid node_name and a valid major_number. Check this:\n"
  usage
  exit 1
else
  echo 'name:' $node_name '\nmajor:' $major '\nminor:' $minor
  start
  exit 0
fi