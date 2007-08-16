#########################################################
# SNS vxWorks startup file
#########################################################

#*******************************************************
# Project Name: LeCroy Test IOC Application
# Boards --- List the boards in your crate here
# IOC Team Leader: Sheng Peng
# EPICS System Support: Ernest L. Williams Jr.
# Computing and Network Systems Support: Greg Lawson
# Host Platform: Linux 2.4.X
# Target Platform: PowerPC 603e (vxWorks 5.4.2)
#**************************************************

# Allow some command line editing please
# "Delete"
tyBackspaceSet(0177)

# ******************************************************
# **** Let's create a bootLog ***************************
# *******************************************************
# Note that open() can only create files on NFS drives

#fd = creat("bootLogFile.txt", 0x2)

# Set stdout to bootLogFile.txt
#ioGlobalStdSet(1, fd)

# Now everything below will be logged to the file "bootLogFile.txt"
# -------------------------------------------------------------------

## The following is needed if your board support package doesn't at boot time
## automatically cd to the directory containing its startup script
## cd "/ade/epics/supTop/share/R3.14.6/LeCroy_ENET/Development/iocBoot/iocLeCroy_ENETTest"

# Change var tothe IOC-specific directory.
# That directory should include the autosave directory
# var=""
# < ../nfsCommands
# putenv ("EPICS_CA_MAX_ARRAY_BYTES=4000000")
< cdCommands

## You may have to change iocLeCroy_ENETTest to something else
## everywhere it appears in this file

# Let's load our main munch file.  This munch file should
# include EPICS BASE, vxWorks and components common to all IOCs
cd topbin
ld < LeCroy_ENETTest.munch

## Register all support components
cd top
dbLoadDatabase("dbd/LeCroy_ENETTest.dbd",0,0)
LeCroy_ENETTest_registerRecordDeviceDriver(pdbbase)


#***********************************************************************
###############  Hardware/Software Initialization ######################
#***********************************************************************
# ----------------------------------------------------------------------
# Initialize LeCroy_ENET Support
# ----------------------------------------------------------------------
LECROY_DRV_DEBUG=1
#install and init LT364 scope driver
init_LT364(0, "160.91.229.165")
#init_LT364(1, "130.199.2.165")
#init_LT364(2, "130.199.2.166")
#init_LT364(3, "130.199.2.48")
#init_LT364(4, "130.199.2.147")
#init_LT364(5, "130.199.2.112")

#---------------- LeCroy_ENET initialization complete -----------------  

########################################################
#####        Load databases               #####
#######################################################
cd top
## Load record instances

cd startup
# Load databases for the LeCroy scopes
dbLoadTemplate("lecroy.instance")

######################################################
### ---------- Database loading complete ----------###
#######################################################

# -----------------------------------------
# Channel Access Security
# -----------------------------------------
# Remember it is necessary to cd back to top
# before calling iocInit.
# since iocInit will use the relative path name
# that is specified by asSetFilename.
# So, don't forget.

cd top
#asSetFilename("db/Mysecurity.acf")
#asSetSubstitutions("P=IOCLeCroy_ENETTest")

cd startup
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
# ------- Start EPICS KERNEL Release ------
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
iocInit()

# --------------------------------------
# ----- Set Shell prompt for IOC -------
# --------------------------------------
shellPromptSet "IOCLeCroy_ENETTest> "

# Enable ioc log
iocLogInit()

## Start any sequence programs
#seq &sncExample,"user=pengs"

cd "/var"
dbl("","IOCLeCroy_ENETTest.pvlist")

## If finished logging info, don't forget
# to close the file descriptor
# Reset stdout to console
##ioGlobalStdSet(1, 3)
##close(fd)

