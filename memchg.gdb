# 设置一个监视点，监控指定地址的值变化
watch *0x0000000087f38028

# 为监视点设置一个条件，仅当内存地址的值变成42时触发
condition $bpnum *0x0000000087f38028 == 0x21FD00D7

# 输出提示信息
commands
    printf "Here\n"
end
