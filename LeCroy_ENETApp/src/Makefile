TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# Let's Debug
USR_CFLAGS += -g -O0

#==================================================
# The following will built and instaled:
# dbd/LeCroy_ENET.dbd
# include/LeCroy_ENET.h
# bin/<arch>/LeCroy_ENETLib.munch
# lib/<arch>/libLeCroy_ENET.a
#=============================
# build an ioc application

# This is what we will be building from here:
# create library for OSI type driver
LIBRARY_IOC += LeCroy_ENET

# This is for vxWorks specific drivers.
# Don't forget to substitute your driver name.
# create library for linking will install in <lib>
#LIBRARY_vxWorks += LeCroy_ENET

# create munch file for dynamic loading will install in <bin>
#PROD_IOC_vxWorks += LeCroy_ENETLib

# The following line makes the LeCroy_ENET.h file
# This is needed for Record support. So uncomment
# the following line if you are making record support.
##DBDINC += LeCroy_ENET
# LeCroy_ENET.dbd will be created from LeCroy_ENETInclude.dbd
DBD += LeCroy_ENET.dbd

# Add locally compiled object code
LeCroy_ENET_SRCS += LeCroy_drv.c
LeCroy_ENET_SRCS += LeCroy_dev.c

# The following builds sncExample as a component of LeCroy_ENET
# Also in LeCroy_ENETInclude.dbd uncomment #registrar(sncExampleRegistrar)
#LeCroy_ENET_SRCS += sncExample.stt
#LeCroy_ENET_LIBS += seq pv

# The PROD_IOC_vxWorks is built from the same srcs and must include
# the registry routines.
LeCroy_ENETLib_SRCS += $(LeCroy_ENET_SRCS)
LeCroy_ENETLib_SRCS += LeCroy_ENET_registerRecordDeviceDriver.cpp

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

