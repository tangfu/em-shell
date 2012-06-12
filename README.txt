em-shell
========

a shell which can be embedded in software code.
一个嵌入软件中的shell环境

========
1. 【特性】
    * 实现了tab自动对齐，匹配能够识别|和>，能够实现最大匹配，相对比较智能
    * 匹配时的汉字显示采用的时zh_CN.utf8对齐，其他编码可能会导致匹配时的不规范(待修正)
    * 实现了多级管道和重定向
    * 实现了历史记录的上下翻页(现阶段还有些bug)
    * shell执行外部命令,例如ping时，必须以ctrl C结束，这经常与外部程序冲突,因此只要外部命令屏蔽Ctrl C,就可以保证在shell关闭外部命令是不受影响,使用该库的tx请屏蔽SIGINT信号，除非你的程序只有一个进程
    * shell一旦运行，可以stop也可以close，stop之后可以继续start，close之后就必须init
    * shell使用了信号63，因此，外部程序不应该再使用该信号值
    * 实现内部命令时，尽量不要阻塞，即便遇到加锁之类也应该使用trylock这种判断，如果获取不到琐就返回，因为内部命令没有使用创建新进程去实现，因此会阻塞掉当前shell的流程
    * 系统使用了SIGRTMAX-1，外部程序不应该再使用该信号值
    * 使用的线程读写锁设置了pshared属性，可以用于多线程和多进程
    * 由于一个程序只能有一个控制前端或终端，因此程序中只允许建立一个shell
    * 内部已经集成了少量shell内建命令，例如cd，version，help，pwd等

=======
2. 【使用方法】

    CMD_OBJ cmd[num] = {{"version","cmd_info",handler1, NULL},{...}};
    SHELL *sh = create_shell();
    sh->init(sh,cmd, get_cmd_len(cmd), "CDN_SHELL");
    sh->start(sh, 1);
    destroy_shell(sh);

