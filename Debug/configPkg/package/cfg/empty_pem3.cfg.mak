# invoke SourceDir generated makefile for empty.pem3
empty.pem3: .libraries,empty.pem3
.libraries,empty.pem3: package/cfg/empty_pem3.xdl
	$(MAKE) -f C:\Users\stirl\CCS_workspace_v12\Projects\TKJ_group_project/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\stirl\CCS_workspace_v12\Projects\TKJ_group_project/src/makefile.libs clean

