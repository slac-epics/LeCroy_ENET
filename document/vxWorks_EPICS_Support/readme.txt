Q: How to add new command or new function?
A: Search "new_command" and "new_function" in source, then you can find the
   right place to insert your new code.

Q: How to interpret the scope response?
A: Because the response has always '\x0A' at the end, so if you use strcmp or
   strncmp to analyze it, you must be very careful. We think strstr is better.
   The actually format of the response is depend on the INIT_STRING, make sure
   they are consistent.

Q: What is the structure of document?
A: There are three directories: vxWorks_EPICS_Support, WavePro and WaveRunner.
   WavePro and WaveRunner has the manuals from vendor.
   vxWorks_EPICS_Support includes this readme and some other description about
   vxWorks driver and EPICS support for LeCroy scope. They are:
           template.txt: The actually template we read back from real scope
           readme.txt:   This file
           Manual.ppt:   API reference of vxWorks driver
           LeCroy-Debug.doc: The bug fix history when the vxWorks driver and
                             EPICS support originally developed in BNL, it's
                             here just for memory.

Q: What is the model of LeCroy scope this software could support?
A: The most correct answer is the LeCroy scopes which are using the same
   template as template.txt in this directory are supported.
   Actually we found since 1980, LeCroy started to use this template, and so
   far they are still using it. So all LeCroy scopes with ethernet interface
   which are made after 1980, we can support them.

