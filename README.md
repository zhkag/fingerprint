
# fingerprint软件包

#### 1、介绍

基于RT-Thread的指纹模块软件包
目前支持ATK-301指纹模块，之后会支持更多的指纹模块

##### 1.1 许可证

fingerprint package 遵循 GPL-3.0 许可，详见 LICENSE 文件。

#### 2、如何使用fingerprint

使用 fingerprint package 需要先使用 `pkgs --upgrade` 更新包列表

然后在 RT-Thread 的包管理器中选择它，具体路径如下：

```c
RT-Thread online packages
    peripheral libraries and drivers --->
    	sensors drivers --->
        	[*] fingerprint module driver  --->
    			fingerprint uart name
            	[*]   Enable fingerprint SAMPLE

```

fingerprint uart name的值要和指纹模块所接的串口保持一直

#### 3、示例说明

一定要看https://gitee.com/aurorazk/fingerprint/blob/master/doc/sample.md

#### 4、联系方式 & 感谢

维护：aurorazk

主页：https://gitee.com/aurorazk

邮箱：pk-ing@nyist.edu.cn

