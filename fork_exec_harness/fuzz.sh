echo "--------------------------------------------------------------------"
echo "      Fuzzing with ForkHarness linux                                "
echo "      @shsirk                                                       "
echo
EXE_PATH=/bin/ls
FORK_HARNESS_PATH=./fork_harness
FORK_HARNESS_STREAM_OUT=./child_err_out_redir

echo "using fork_harness. default timeout is 30 seconds for each sample."
echo "harness path: $FORK_HARNESS_PATH"
echo "executable path: $EXE_PATH"

CRASH_DIR=`pwd`"/CRASHES"
echo "creating crash directory -> $CRASH_DIR"
mkdir $CRASH_DIR >/dev/null 2>&1

SAMPLES_DIR=`pwd`"/SAMPLES"
echo "creating samples directory -> $SAMPLES_DIR"
mkdir $SAMPLES_DIR >/dev/null 2>&1

echo "--------------------------------------------------------------------"
echo

while true
do
    echo "(*) generating fuzz sample batch..."
    python generate_fuzz_samples.py -n 10 $SAMPLES_DIR >/dev/null 2>&1

    echo "(*) fuzzing samples..."
    for sample in $SAMPLES_DIR/*
    do 
        echo "  (+) running sample..$sample"
        
        $FORK_HARNESS_PATH $EXE_PATH $sample
        grep -q -P "(AddressSanitizer|Assertion|SegFaultCrash)" $FORK_HARNESS_STREAM_OUT >/dev/null 2>&1
        if [ $? -eq 0 ]
        then
            hash_name=`md5sum $sample | awk '{print $1}'` 
            echo "      (!) CRASH found. saving PoC to $CRASH_DIR/$hash_name"
            cat $FORK_HARNESS_STREAM_OUT
            cp $sample $CRASH_DIR/$hash_name.pdf
        fi

        rm $sample
        rm $FORK_HARNESS_STREAM_OUT >/dev/null 2>&1
    done
done
