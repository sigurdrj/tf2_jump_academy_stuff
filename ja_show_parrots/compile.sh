g++-10 -std=c++2a -O2 -c generate_tf2_script.cpp -o generate_tf2_script.o
g++-10 -std=c++2a generate_tf2_script.o -Wall -O2 -lcurl -lcurlpp -o generate_tf2_script
