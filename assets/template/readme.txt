template_gen.c是模板生成工具，先独立sway编译成可执行程序，然后运行加上配置参数生成点阵模板，以上一系列过程都是独立于sway编译的，得事先进行
gcc -o template_gen template_gen.c -lm
# 运行（参数：设备ID 时间戳 输出路径）
./template_gen 0x1A2B3C4D 1718000000 ./watermark_template.bin