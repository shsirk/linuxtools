REMOTE_HOST=192.168.4.171
PORT=80
RED='\033[0;31m'
NC='\033[0m' # No Color

ping -c 1 $REMOTE_HOST > /dev/null 2>&1
if [ $? -ne 0 ]
then
   echo "[-] remote host $REMOTE_HOST seems to be down"
   exit; 
fi

ulimit -c unlimited

export ASAN_OPTIONS=detect_leaks=0

echo -e "${RED}[DOWNLOADING]${NC} --------------------------------------------------------"
wget_output=$(wget -q "http://$REMOTE_HOST:$PORT/" -O testcase.js)
if [ $? -ne 0 ]; then
    echo "[-] failed to download testcase from remote host!"
    exit 1
fi
tail -n 10 testcase.js
echo -e "${RED}[TESTING] ${NC}--------------------------------------------------------"
./Linux/jsc/jsc testcase.js

rm drcov.jsc.*.log

echo -e "${RED}[COVERAGE] ${NC}--------------------------------------------------------"
~/DynamoRIO-Linux-7.1.0-1/bin64/drrun -t drcov -- Linux/jsc/jsc testcase.js

echo -e "${RED}[FLT|DFG] ${NC}----------------------------------------------------"
python drcov.py drcov.jsc.* | egrep -i "(FLT|DFG|OSR)"
echo -e "${RED}[DONE] ${NC}--------------------------------------------------------"
