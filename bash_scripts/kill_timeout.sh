while true
do
    JS_ENGINES=( js d8 ch jsc)
    for i in "${JS_ENGINES[@]}"
    do
        killall --older-than 30s $i 2>/dev/null
        if [ $? -eq 0 ]; then
            echo "[KILLED] $i" 
        fi
    done
    sleep 30s
done
