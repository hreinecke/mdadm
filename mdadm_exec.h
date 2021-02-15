#ifndef _MDADM_EXEC_H
#define _MDADM_EXEC_H

#define SYSLOG_FACILITY LOG_DAEMON

#define DEFAULT_BITMAP_DELAY 5
#define DEFAULT_MAX_WRITE_BEHIND 256

extern char DefaultConfFile[];

enum mode {
	ASSEMBLE=1,
	BUILD,
	CREATE,
	MANAGE,
	MISC,
	MONITOR,
	GROW,
	INCREMENTAL,
	AUTODETECT,
	mode_count
};

extern char short_options[];
extern char short_bitmap_options[];
extern char short_bitmap_auto_options[];
extern struct option long_options[];
extern char Usage[], Help[], OptionHelp[],
	*mode_help[],
	Help_create[], Help_build[], Help_assemble[], Help_grow[],
	Help_incr[],
	Help_manage[], Help_misc[], Help_monitor[], Help_config[];

/* for option that don't have short equivilents, we assign arbitrary
 * numbers later than any 'short' character option.
 */
enum special_options {
	AssumeClean = 300,
	BitmapChunk,
	WriteBehind,
	ReAdd,
	NoDegraded,
	Sparc22,
	BackupFile,
	HomeHost,
	AutoHomeHost,
	Symlinks,
	AutoDetect,
	Waitclean,
	DetailPlatform,
	KillSubarray,
	UpdateSubarray,
	IncrementalPath,
	NoSharing,
	HelpOptions,
	Brief,
	NoDevices,
	ManageOpt,
	Add,
	AddSpare,
	AddJournal,
	Remove,
	Fail,
	Replace,
	With,
	MiscOpt,
	WaitOpt,
	ConfigFile,
	ChunkSize,
	WriteMostly,
	FailFast,
	NoFailFast,
	Layout,
	Auto,
	Force,
	SuperMinor,
	EMail,
	ProgramOpt,
	Increment,
	Fork,
	Bitmap,
	RebuildMapOpt,
	InvalidBackup,
	UdevRules,
	FreezeReshape,
	Continue,
	OffRootOpt,
	Prefer,
	KillOpt,
	DataOffset,
	ExamineBB,
	Dump,
	Restore,
	Action,
	Nodes,
	ClusterName,
	ClusterConfirm,
	WriteJournal,
	ConsistencyPolicy,
};

extern int open_mddev(char *dev, int report_errors);
int Write_rules(char *rule_name);
#endif /* _MDADM_EXEC_H */
