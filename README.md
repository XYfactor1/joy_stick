下载SDL2库
sudo apt-get install libsdl2-dev

git clone 项目
git clone https://github.com/XYfactor1/joy_stick.git

##进入项目
cd joy_stick

创建编译目录
mkdir build 
cd build

编译
cmake .. 
make

运行
./simple_joystick
