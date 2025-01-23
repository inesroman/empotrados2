################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Add inputs and outputs from these tool invocations to the build variables 
CFG_SRCS += \
../app.cfg 

CMD_SRCS += \
../TM4C123GH6PM.cmd 

LIB_SRCS += \
../driverlib.lib 

C_SRCS += \
../computos.c \
../indep.c 

GEN_CMDS += \
./configPkg/linker.cmd 

GEN_FILES += \
./configPkg/linker.cmd \
./configPkg/compiler.opt 

GEN_MISC_DIRS += \
./configPkg/ 

C_DEPS += \
./computos.d \
./indep.d 

GEN_OPTS += \
./configPkg/compiler.opt 

OBJS += \
./computos.obj \
./indep.obj 

GEN_MISC_DIRS__QUOTED += \
"configPkg\" 

OBJS__QUOTED += \
"computos.obj" \
"indep.obj" 

C_DEPS__QUOTED += \
"computos.d" \
"indep.d" 

GEN_FILES__QUOTED += \
"configPkg\linker.cmd" \
"configPkg\compiler.opt" 

C_SRCS__QUOTED += \
"../computos.c" \
"../indep.c" 


