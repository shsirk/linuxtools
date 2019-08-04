#
# fuzz script to assist with forkserver template
# krishs.patil@gmail.com (twitter @shsirk)
#
# apply patch on target server and compile program
# this script downloads samples from generator/mutator hosted on network and save it to corpus  
# then runs harnessed program over it. 
#
REMOTE_HOST=localhost
PORT=8000
PROGRAM=./program/to/fuzz

ping -c 1 $REMOTE_HOST > /dev/null 2>&1
if [ $? -ne 0 ]
then
   echo "[-] remote host $REMOTE_HOST seems to be down"
   exit; 
fi

ulimit -c unlimited

export ASAN_OPTIONS=detect_leaks=0
export FORKSERVER_FUZZ=1

while true
do
    echo "fuzz.sh downloading samples..."
    for ii in {1..100}
    do 
        wget_output=$(wget -q "http://$REMOTE_HOST:$PORT/" -O testcase)
        if [ $? -ne 0 ]; then
            echo "[-] [ERROR] failed to download testcase from remote host!"
            exit 1
        fi
        mv ./testcase ./corpus/`md5sum testcase.js|awk '{print $1}'`
    done

    $PROGRAM ./testcase
    
    #SLEEP FOR SOME TIME NOW
    echo "fuzz.sh sleeping for 10 seconds before next iterations..."
    sleep 10
done
