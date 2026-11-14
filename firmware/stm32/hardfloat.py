# Передаёт компоновщику те же флаги FPU, что и компилятору (аппаратная плавающая точка Cortex-M4F)
Import("env")
env.Append(LINKFLAGS=["-mfloat-abi=hard", "-mfpu=fpv4-sp-d16"])
