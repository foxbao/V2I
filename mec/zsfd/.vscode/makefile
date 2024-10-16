MK		= make

all:
	$(MK) -f .vscode/utils_mkdep
	$(MK) -f .vscode/utils_mk
	$(MK) -f .vscode/hwgrph_mkdep
	$(MK) -f .vscode/hwgrph_mk
	$(MK) -f .vscode/uicore_mkdep
	$(MK) -f .vscode/uicore_mk
	$(MK) -f .vscode/mware_mkdep
	$(MK) -f .vscode/mware_mk
	$(MK) -f .vscode/syssvr_mkdep
	$(MK) -f .vscode/syssvr_mk

.PHONY: clean

clean:
	$(MK) -f .vscode/utils_mk clean
	$(MK) -f .vscode/hwgrph_mk clean
	$(MK) -f .vscode/uicore_mk clean
	$(MK) -f .vscode/mware_mk clean
	$(MK) -f .vscode/syssvr_mk clean
