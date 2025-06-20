cd /Arcs/Repos/smalls/enswap/cpp/
# g++ -std=c++20 -o enswap main.cpp
#  -F 1024
# -std=c++20 -std=c++2c
# g++ -Wall -Ofast -flto -fno-rtti -mtune=native -s -lpthread -std=c++20 -o enswap main.cpp
g++ -Wall -Os -flto -fno-rtti -mtune=native -s -lpthread -std=c++20 -o enswap main.cpp
