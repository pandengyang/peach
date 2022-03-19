# 关于

桃花源（英文名为 peach）是一个迷你虚拟机，用于学习 Intel 硬件虚拟化技术。学习该项目可使读者对 CPU 虚拟化、内存虚拟化技术有个感性、直观的认识，为学习 KVM 打下坚实的基础。peach 实现了如下功能：

* 使用 Intel VT-x 技术实现 CPU 虚拟化
* 使用 EPT 技术实现内存虚拟化
* 支持虚拟 x86 实模式运行环境
* 支持虚拟 CPUID 指令
* 支持虚拟 HLT 指令，Guest 利用 HLT 指令关机

关于 peach 的详细讲解，请阅读微信公众号 ScratchLab 文章《自己动手写虚拟机（一）》：

![微信搜一搜 ScratchLab](scratchlab.jpg)
