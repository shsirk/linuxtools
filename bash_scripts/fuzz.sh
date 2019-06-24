
REMOTE_HOST=192.168.4.171
PORT=80

ping -c 1 $REMOTE_HOST > /dev/null 2>&1
if [ $? -ne 0 ]
then
   echo "[-] remote host $REMOTE_HOST seems to be down"
   exit; 
fi

ulimit -c unlimited

export ASAN_OPTIONS=detect_leaks=0

while true
do
    wget_output=$(wget -q "http://$REMOTE_HOST:$PORT/" -O testcase.js)
    if [ $? -ne 0 ]; then
        echo "[-] [ERROR] failed to download testcase from remote host!"
        exit 1
    fi
    
    printf "[*] fuzz [`md5sum testcase.js`] ... "
    JS_ENGINES=( "./Linux/jsc/jsc --validateOptions=true --useConcurrentJIT=false --useConcurrentGC=false --thresholdForJITSoon=10 --thresholdForJITAfterWarmUp=10 --thresholdForOptimizeAfterWarmUp=100 --thresholdForOptimizeAfterLongWarmUp=100 --thresholdForOptimizeAfterLongWarmUp=100 --thresholdForFTLOptimizeAfterWarmUp=1000 --thresholdForFTLOptimizeSoon=1000 --gcAtEnd=true" )
    for i in "${JS_ENGINES[@]}"
    do
        $i testcase.js > ./error 2>&1
	    if cat ./error | grep -q "AddressSanitizer"; then
	        printf "[CRASH]\n"
	        save="CRASH_"`md5sum testcase.js | awk '{ print $1 }'`".zip"
	        zip -q $save testcase.js ./error
        elif cat ./error | grep -q "Error"; then
            printf "[ERROR]\n"
        else
            printf "[OK!]\n"
	    fi
    done
    
    #SLEEP FOR SOME TIME NOW
    sleep 1
done
