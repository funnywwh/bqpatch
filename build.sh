
case $1 in
    -d)
        echo build debug bqpatch
        g++ -DDEBUG=1 -g main.cpp -o bqpatch
    ;;
    *)
        echo build release bqpatch
        g++ main.cpp -o bqpatch
    ;;
esac
