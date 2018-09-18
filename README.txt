#启用
1. 把下面的宏定义写入rtconfig.h
   #define RT_USING_DFS
   #define RT_USING_MTD
   #define MTD_USING_NAND
   #define PKG_USING_DFS_YAFFS 
 2. 注册nand驱动
 3. 调用yaffs_start_up注册yaffs驱动到'nand1'的MTD设备上,
    或者参照这个函数自己实现注册yaffs驱动的方法。
 4. 挂载yaffs文件系统
 