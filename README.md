直接在这个项目文件夹http-sever的终端里输入./httpd就可以在浏览器中找到localhost：4000

     httpd文件在终端中make生成
     
     make查找makefile里的命令
     
     如果makefile出现“分隔符缺失 (你大概想用 TAB，而不是八个空格)” 则把第四行删除并重新输入Tab键，后面加上原来第四行的内容

     makefile文件中第四行必须要有-lpthread，否则会出现undefined reference to `pthread_create'

     在vscode中直接使用f5或f6快捷键同样会出现undefined reference to `pthread_create'，因此要用makefile文件，并在终端中使用make命令编译httpd.c文件
