1. ctrl + a, c: 进入qemu
info mem: 查看内存


2. uservec对应的虚拟地址是 0x3ffffff000
所以在uservec 打断点可以用 b *0x3ffffff000


3. syscall: 0x80001fe2

4. userret: 0x0000003ffffff09c

4. userret的最后一条指令(sret): 0x0000003ffffff11c