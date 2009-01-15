/*
 * Copyright (C) 2008 Search Solution Corporation. All rights reserved by Search Solution. 
 *
 *   This program is free software; you can redistribute it and/or modify 
 *   it under the terms of the GNU General Public License as published by 
 *   the Free Software Foundation; version 2 of the License. 
 *
 *  This program is distributed in the hope that it will be useful, 
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 *  GNU General Public License for more details. 
 *
 *  You should have received a copy of the GNU General Public License 
 *  along with this program; if not, write to the Free Software 
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
 *
 */


/*
 * server_util.c -
 */

#ident "$Id$"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>		/* umask()          */
#include <sys/stat.h>		/* umask(), stat()  */
#include <time.h>
#include <ctype.h>		/* isalpha()        */
#include <errno.h>
#include <fcntl.h>

#ifdef WIN32
#include <process.h>
#include <io.h>
#include <direct.h>
#include <winsock2.h>
#include <sys/locking.h>
#include <Tlhelp32.h>
#else
#include <unistd.h>
#include <dirent.h>		/* opendir() ...    */
#include <pwd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/statvfs.h>
#if !defined(HPUX) && !defined(AIX)
#include <sys/procfs.h>
#endif
#include <sys/file.h>
#endif

#include "cm_porting.h"
#include "cm_server_util.h"
#include "cm_nameval.h"
#include "cm_dstring.h"
#include "cm_config.h"
#include "cm_job_task.h"
#include "cm_command_execute.h"
#include <assert.h>

#ifdef FSERVER_SLAVE
#define DEF_TASK_FUNC(TASK_FUNC_PTR)	TASK_FUNC_PTR
#else
#define DEF_TASK_FUNC(TASK_FUNC_PTR)	NULL
#endif

static int _uReadDBtxtFile (char *dn, int idx, char *outbuf);

static T_FSERVER_TASK_INFO task_info[] = {
  {"startinfo", TS_STARTINFO, 0, DEF_TASK_FUNC (ts_startinfo), FSVR_SA},
  {"userinfo", TS_USERINFO, 0, DEF_TASK_FUNC (ts_userinfo), FSVR_CS},
  {"createuser", TS_CREATEUSER, 1, DEF_TASK_FUNC (ts_create_user), FSVR_CS},
  {"deleteuser", TS_DELETEUSER, 1, DEF_TASK_FUNC (ts_delete_user), FSVR_CS},
  {"updateuser", TS_UPDATEUSER, 1, DEF_TASK_FUNC (ts_update_user), FSVR_CS},
  {"createdb", TS_CREATEDB, 1, DEF_TASK_FUNC (tsCreateDB), FSVR_SA},
  {"deletedb", TS_DELETEDB, 1, DEF_TASK_FUNC (tsDeleteDB), FSVR_SA},
  {"renamedb", TS_RENAMEDB, 1, DEF_TASK_FUNC (tsRenameDB), FSVR_SA},
  {"startdb", TS_STARTDB, 0, DEF_TASK_FUNC (tsStartDB), FSVR_NONE},
  {"stopdb", TS_STOPDB, 0, DEF_TASK_FUNC (tsStopDB), FSVR_CS},
  {"dbspaceinfo", TS_DBSPACEINFO, 0, DEF_TASK_FUNC (tsDbspaceInfo),
   FSVR_SA_CS},
  {"classinfo", TS_CLASSINFO, 0, DEF_TASK_FUNC (ts_class_info), FSVR_SA_CS},
  {"class", TS_CLASS, 0, DEF_TASK_FUNC (ts_class), FSVR_CS},
  {"renameclass", TS_RENAMECLASS, 1, DEF_TASK_FUNC (ts_rename_class),
   FSVR_CS},
  {"dropclass", TS_DROPCLASS, 1, DEF_TASK_FUNC (ts_drop_class), FSVR_CS},
  {"setsysparam", TS_SETSYSPARAM, 1, DEF_TASK_FUNC (ts_set_sysparam),
   FSVR_NONE},
  {"getallsysparam", TS_GETALLSYSPARAM, 0,
   DEF_TASK_FUNC (ts_get_all_sysparam), FSVR_NONE},
  {"addvoldb", TS_ADDVOLDB, 1, DEF_TASK_FUNC (tsRunAddvoldb), FSVR_SA_CS},
  {"createclass", TS_CREATECLASS, 1, DEF_TASK_FUNC (ts_create_class),
   FSVR_CS},
  {"createvclass", TS_CREATEVCLASS, 1, DEF_TASK_FUNC (ts_create_vclass),
   FSVR_CS},
  {"getloginfo", TS_GETLOGINFO, 0, DEF_TASK_FUNC (ts_get_log_info),
   FSVR_NONE},
  {"viewlog", TS_VIEWLOG, 0, DEF_TASK_FUNC (ts_view_log), FSVR_NONE},
  {"viewlog2", TS_VIEWLOG, 0, DEF_TASK_FUNC (ts_view_log), FSVR_NONE},
  {"resetlog", TS_RESETLOG, 0, DEF_TASK_FUNC (ts_reset_log), FSVR_NONE},
  {"getenv", TS_GETENV, 0, DEF_TASK_FUNC (tsGetEnvironment), FSVR_SA_UC},
  {"checkaccessright", TS_GETACCESSRIGHT, 0,
   DEF_TASK_FUNC (ts_get_access_right), FSVR_CS},
  {"addattribute", TS_ADDATTRIBUTE, 1, DEF_TASK_FUNC (ts_add_attribute),
   FSVR_CS},
  {"dropattribute", TS_DROPATTRIBUTE, 1, DEF_TASK_FUNC (ts_drop_attribute),
   FSVR_CS},
  {"updateattribute", TS_UPDATEATTRIBUTE, 1,
   DEF_TASK_FUNC (ts_update_attribute), FSVR_CS},
  {"addconstraint", TS_ADDCONSTRAINT, 1, DEF_TASK_FUNC (ts_add_constraint),
   FSVR_CS},
  {"dropconstraint", TS_DROPCONSTRAINT, 1, DEF_TASK_FUNC (ts_drop_constraint),
   FSVR_CS},
  {"addsuper", TS_ADDSUPER, 1, DEF_TASK_FUNC (ts_add_super), FSVR_CS},
  {"dropsuper", TS_DROPSUPER, 1, DEF_TASK_FUNC (ts_drop_super), FSVR_CS},
  {"getsuperclassesinfo", TS_GETSUPERCLASSESINFO, 0,
   DEF_TASK_FUNC (ts_get_superclasses_info), FSVR_CS},
  {"addresolution", TS_ADDRESOLUTION, 1, DEF_TASK_FUNC (ts_add_resolution),
   FSVR_CS},
  {"dropresolution", TS_DROPRESOLUTION, 1, DEF_TASK_FUNC (ts_drop_resolution),
   FSVR_CS},
  {"addmethod", TS_ADDMETHOD, 1, DEF_TASK_FUNC (ts_add_method), FSVR_CS},
  {"dropmethod", TS_DROPMETHOD, 1, DEF_TASK_FUNC (ts_drop_method), FSVR_CS},
  {"updatemethod", TS_UPDATEMETHOD, 1, DEF_TASK_FUNC (ts_update_method),
   FSVR_CS},
  {"addmethodfile", TS_ADDMETHODFILE, 1, DEF_TASK_FUNC (ts_add_method_file),
   FSVR_CS},
  {"dropmethodfile", TS_DROPMETHODFILE, 1,
   DEF_TASK_FUNC (ts_drop_method_file), FSVR_CS},
  {"addqueryspec", TS_ADDQUERYSPEC, 1, DEF_TASK_FUNC (ts_add_query_spec),
   FSVR_CS},
  {"dropqueryspec", TS_DROPQUERYSPEC, 1, DEF_TASK_FUNC (ts_drop_query_spec),
   FSVR_CS},
  {"changequeryspec", TS_CHANGEQUERYSPEC, 1,
   DEF_TASK_FUNC (ts_change_query_spec), FSVR_CS},
  {"validatequeryspec", TS_VALIDATEQUERYSPEC, 0,
   DEF_TASK_FUNC (ts_validate_query_spec), FSVR_CS},
  {"validatevclass", TS_VALIDATEVCLASS, 0, DEF_TASK_FUNC (ts_validate_vclass),
   FSVR_CS},
  {"kill_process", TS_KILL_PROCESS, 1, DEF_TASK_FUNC (ts_kill_process),
   FSVR_NONE},
  {"gethistory", TS_GETHISTORY, 0, DEF_TASK_FUNC (tsGetHistory), FSVR_NONE},
  {"sethistory", TS_SETHISTORY, 1, DEF_TASK_FUNC (tsSetHistory), FSVR_NONE},
  {"gethistorylist", TS_GETHISTORYFILELIST, 0,
   DEF_TASK_FUNC (tsGetHistoryFileList), FSVR_NONE},
  {"viewhistorylog", TS_READHISTORYFILE, 0, DEF_TASK_FUNC (tsReadHistoryFile),
   FSVR_NONE},
  {"copydb", TS_COPYDB, 1, DEF_TASK_FUNC (ts_copydb), FSVR_SA},
  {"optimizedb", TS_OPTIMIZEDB, 1, DEF_TASK_FUNC (ts_optimizedb), FSVR_SA_CS},
  {"checkdb", TS_CHECKDB, 0, DEF_TASK_FUNC (ts_checkdb), FSVR_SA_CS},
  {"compactdb", TS_COMPACTDB, 1, DEF_TASK_FUNC (ts_compactdb), FSVR_SA},
  {"backupdbinfo", TS_BACKUPDBINFO, 0, DEF_TASK_FUNC (ts_backupdb_info),
   FSVR_NONE},
  {"backupdb", TS_BACKUPDB, 0, DEF_TASK_FUNC (ts_backupdb), FSVR_SA_CS},
  {"unloaddb", TS_UNLOADDB, 1, DEF_TASK_FUNC (ts_unloaddb), FSVR_SA},
  {"unloadinfo", TS_UNLOADDBINFO, 0, DEF_TASK_FUNC (ts_unloaddb_info),
   FSVR_NONE},
  {"loaddb", TS_LOADDB, 1, DEF_TASK_FUNC (ts_loaddb), FSVR_SA},
  {"gettransactioninfo", TS_GETTRANINFO, 0, DEF_TASK_FUNC (ts_get_tran_info),
   FSVR_CS},
  {"killtransaction", TS_KILLTRAN, 1, DEF_TASK_FUNC (ts_killtran), FSVR_CS},
  {"lockdb", TS_LOCKDB, 0, DEF_TASK_FUNC (ts_lockdb), FSVR_CS},
  {"getbackuplist", TS_GETBACKUPLIST, 0, DEF_TASK_FUNC (ts_get_backup_list),
   FSVR_NONE},
  {"restoredb", TS_RESTOREDB, 1, DEF_TASK_FUNC (ts_restoredb), FSVR_SA},
  {"backupvolinfo", TS_BACKUPVOLINFO, 0, DEF_TASK_FUNC (ts_backup_vol_info),
   FSVR_SA},
  {"getdbsize", TS_GETDBSIZE, 0, DEF_TASK_FUNC (ts_get_dbsize), FSVR_SA_CS},
  {"getbackupinfo", TS_GETBACKUPINFO, 0, DEF_TASK_FUNC (ts_get_backup_info),
   FSVR_NONE},
  {"addbackupinfo", TS_ADDBACKUPINFO, 1, DEF_TASK_FUNC (ts_add_backup_info),
   FSVR_NONE},
  {"deletebackupinfo", TS_DELETEBACKUPINFO, 1,
   DEF_TASK_FUNC (ts_delete_backup_info), FSVR_NONE},
  {"setbackupinfo", TS_SETBACKUPINFO, 0, DEF_TASK_FUNC (ts_set_backup_info),
   FSVR_NONE},
  {"getdberror", TS_GETDBERROR, 0, DEF_TASK_FUNC (ts_get_db_error),
   FSVR_NONE},
  {"getautoaddvol", TS_GETAUTOADDVOL, 0, DEF_TASK_FUNC (ts_get_auto_add_vol),
   FSVR_NONE},
  {"setautoaddvol", TS_SETAUTOADDVOL, 1, DEF_TASK_FUNC (ts_set_auto_add_vol),
   FSVR_NONE},
  {"generaldbinfo", TS_GENERALDBINFO, 0, DEF_TASK_FUNC (ts_general_db_info),
   FSVR_SA_CS},
  {"loadaccesslog", TS_LOADACCESSLOG, 0, DEF_TASK_FUNC (ts_load_access_log),
   FSVR_NONE},
  {"deleteaccesslog", TS_DELACCESSLOG, 1,
   DEF_TASK_FUNC (ts_delete_access_log), FSVR_NONE},
  {"deleteerrorlog", TS_DELERRORLOG, 1, DEF_TASK_FUNC (ts_delete_error_log),
   FSVR_NONE},
  {"checkdir", TS_CHECKDIR, 0, DEF_TASK_FUNC (ts_check_dir), FSVR_NONE},
  {"getautobackupdberrlog", TS_AUTOBACKUPDBERRLOG, 0,
   DEF_TASK_FUNC (ts_get_autobackupdb_error_log), FSVR_NONE},
  {"getdbmtuserinfo", TS_GETDBMTUSERINFO, 0,
   DEF_TASK_FUNC (tsGetDBMTUserInfo), FSVR_NONE},
  {"deletedbmtuser", TS_DELETEDBMTUSER, 1, DEF_TASK_FUNC (tsDeleteDBMTUser),
   FSVR_NONE},
  {"updatedbmtuser", TS_UPDATEDBMTUSER, 1, DEF_TASK_FUNC (tsUpdateDBMTUser),
   FSVR_NONE},
  {"setdbmtpasswd", TS_SETDBMTPASSWD, 1,
   DEF_TASK_FUNC (tsChangeDBMTUserPasswd), FSVR_NONE},
  {"adddbmtuser", TS_ADDDBMTUSER, 1, DEF_TASK_FUNC (tsCreateDBMTUser),
   FSVR_NONE},
  {"getaddvolstatus", TS_GETADDVOLSTATUS, 0,
   DEF_TASK_FUNC (ts_get_addvol_status), FSVR_NONE},
  {"checkauthority", TS_CHECKAUTHORITY, 0, DEF_TASK_FUNC (ts_check_authority),
   FSVR_CS},
  {"getautoaddvollog", TS_GETAUTOADDVOLLOG, 0,
   DEF_TASK_FUNC (tsGetAutoaddvolLog), FSVR_UC},
  {"getinitbrokersinfo", TS2_GETINITUNICASINFO, 0,
   DEF_TASK_FUNC (ts2_get_unicas_info), FSVR_UC},
  {"getbrokersinfo", TS2_GETUNICASINFO, 0,
   DEF_TASK_FUNC (ts2_get_unicas_info),
   FSVR_UC},
  {"startbroker", TS2_STARTUNICAS, 0, DEF_TASK_FUNC (ts2_start_unicas),
   FSVR_UC},
  {"stopbroker", TS2_STOPUNICAS, 0, DEF_TASK_FUNC (ts2_stop_unicas), FSVR_UC},
  {"getadminloginfo", TS2_GETADMINLOGINFO, 0,
   DEF_TASK_FUNC (ts2_get_admin_log_info), FSVR_UC},
  {"getlogfileinfo", TS2_GETLOGFILEINFO, 0,
   DEF_TASK_FUNC (ts2_get_logfile_info), FSVR_UC},
  {"addbroker", TS2_ADDBROKER, 1, DEF_TASK_FUNC (ts2_add_broker), FSVR_UC},
  {"getaddbrokerinfo", TS2_GETADDBROKERINFO, 0,
   DEF_TASK_FUNC (ts2_get_add_broker_info), FSVR_UC},
  {"deletebroker", TS2_DELETEBROKER, 1, DEF_TASK_FUNC (ts2_delete_broker),
   FSVR_UC},
  {"renamebroker", TS2_RENAMEBROKER, 1, DEF_TASK_FUNC (ts2_rename_broker),
   FSVR_UC},
  {"getbrokerstatus", TS2_GETBROKERSTATUS, 0,
   DEF_TASK_FUNC (ts2_get_broker_status), FSVR_UC},
  {"getbrokerinfo", TS2_GETBROKERCONF, 0, DEF_TASK_FUNC (ts2_get_broker_conf),
   FSVR_UC},
  {"getbrokeronconf", TS2_GETBROKERONCONF, 0,
   DEF_TASK_FUNC (ts2_get_broker_on_conf), FSVR_UC},
  {"broker_setparam", TS2_SETBROKERCONF, 1,
   DEF_TASK_FUNC (ts2_set_broker_conf), FSVR_UC},
  {"setbrokeronconf", TS2_SETBROKERONCONF, 1,
   DEF_TASK_FUNC (ts2_set_broker_on_conf), FSVR_UC},
  {"broker_start", TS2_STARTBROKER, 0, DEF_TASK_FUNC (ts2_start_broker),
   FSVR_UC},
  {"broker_stop", TS2_STOPBROKER, 0, DEF_TASK_FUNC (ts2_stop_broker),
   FSVR_UC},
  {"broker_suspend", TS2_SUSPENDBROKER, 0, DEF_TASK_FUNC (ts2_suspend_broker),
   FSVR_UC},
  {"broker_resume", TS2_RESUMEBROKER, 0, DEF_TASK_FUNC (ts2_resume_broker),
   FSVR_UC},
  {"broker_job_first", TS2_BROKERJOBFIRST, 0,
   DEF_TASK_FUNC (ts2_broker_job_first), FSVR_UC},
  {"broker_job_info", TS2_BROKERJOBINFO, 0,
   DEF_TASK_FUNC (ts2_broker_job_info), FSVR_UC},
  {"broker_add", TS2_ADDBROKERAS, 1, DEF_TASK_FUNC (ts2_add_broker_as),
   FSVR_UC},
  {"broker_drop", TS2_DROPBROKERAS, 1, DEF_TASK_FUNC (ts2_drop_broker_as),
   FSVR_UC},
  {"broker_restart", TS2_RESTARTBROKERAS, 0,
   DEF_TASK_FUNC (ts2_restart_broker_as), FSVR_UC},
  {"broker_getstatuslog", TS2_GETBROKERSTATUSLOG, 0,
   DEF_TASK_FUNC (ts2_get_broker_status_log), FSVR_UC},
  {"broker_getmonitorconf", TS2_GETBROKERMCONF, 0,
   DEF_TASK_FUNC (ts2_get_broker_m_conf), FSVR_UC},
  {"broker_setmonitoconf", TS2_SETBROKERMCONF, 1,
   DEF_TASK_FUNC (ts2_set_broker_m_conf), FSVR_UC},
  {"getaslimit", TS2_GETBROKERASLIMIT, 0,
   DEF_TASK_FUNC (ts2_get_broker_as_limit), FSVR_UC},
  {"getbrokerenvinfo", TS2_GETBROKERENVINFO, 0,
   DEF_TASK_FUNC (ts2_get_broker_env_info), FSVR_UC},
  {"setbrokerenvinfo", TS2_SETBROKERENVINFO, 1,
   DEF_TASK_FUNC (ts2_set_broker_env_info), FSVR_UC},
  {"access_list_addip", TS2_ACCESSLISTADDIP, 0,
   DEF_TASK_FUNC (ts2_access_list_add_ip), FSVR_UC},
  {"access_list_deleteip", TS2_ACCESSLISTDELETEIP, 0,
   DEF_TASK_FUNC (ts2_access_list_delete_ip), FSVR_UC},
  {"access_list_info", TS2_ACCESSLISTINFO, 0,
   DEF_TASK_FUNC (ts2_access_list_info), FSVR_UC},
  {"checkfile", TS_CHECKFILE, 0, DEF_TASK_FUNC (ts_check_file), FSVR_NONE},
  {"registerlocaldb", TS_REGISTERLOCALDB, 1,
   DEF_TASK_FUNC (ts_localdb_operation), FSVR_SA_CS},
  {"removelocaldb", TS_REMOVELOCALDB, 1, DEF_TASK_FUNC (ts_localdb_operation),
   FSVR_SA_CS},
  {"addtrigger", TS_ADDNEWTRIGGER, 1, DEF_TASK_FUNC (ts_trigger_operation),
   FSVR_SA_CS},
  {"altertrigger", TS_ALTERTRIGGER, 1, DEF_TASK_FUNC (ts_trigger_operation),
   FSVR_SA_CS},
  {"droptrigger", TS_DROPTRIGGER, 1, DEF_TASK_FUNC (ts_trigger_operation),
   FSVR_SA_CS},
  {"gettriggerinfo", TS_GETTRIGGERINFO, 0, DEF_TASK_FUNC (ts_get_triggerinfo),
   FSVR_SA_CS},
  {"getfile", TS_GETFILE, 0, DEF_TASK_FUNC (ts_get_file), FSVR_SA_CS},
  {"getautoexecquery", TS_GETAUTOEXECQUERY, 0,
   DEF_TASK_FUNC (ts_get_autoexec_query), FSVR_SA_CS},
  {"setautoexecquery", TS_SETAUTOEXECQUERY, 1,
   DEF_TASK_FUNC (ts_set_autoexec_query), FSVR_SA_CS},
#ifdef DIAG_DEVEL
  {"getdiaginfo", TS_GETDIAGINFO, 0, DEF_TASK_FUNC (ts_get_diaginfo),
   FSVR_NONE},
  {"getdiagdata", TS_GET_DIAGDATA, 0, DEF_TASK_FUNC (ts_get_diagdata),
   FSVR_NONE},
  {"addstatustemplate", TS_ADDSTATUSTEMPLATE, 0,
   DEF_TASK_FUNC (ts_addstatustemplate), FSVR_NONE},
  {"updatestatustemplate", TS_UPDATESTATUSTEMPLATE, 0,
   DEF_TASK_FUNC (ts_updatestatustemplate), FSVR_NONE},
  {"removestatustemplate", TS_REMOVESTATUSTEMPLATE, 0,
   DEF_TASK_FUNC (ts_removestatustemplate), FSVR_NONE},
  {"getstatustemplate", TS_GETSTATUSTEMPLATE, 0,
   DEF_TASK_FUNC (ts_getstatustemplate), FSVR_NONE},
#if 0				/* ACTIVITY_PROFILE */
  {"addactivitytemplate", TS_ADDACTIVITYTEMPLATE, 0,
   DEF_TASK_FUNC (ts_addactivitytemplate), FSVR_NONE},
  {"updateactivitytemplate", TS_UPDATEACTIVITYTEMPLATE, 0,
   DEF_TASK_FUNC (ts_updateactivitytemplate), FSVR_NONE},
  {"removeactivitytemplate", TS_REMOVEACTIVITYTEMPLATE, 0,
   DEF_TASK_FUNC (ts_removeactivitytemplate), FSVR_NONE},
  {"getactivitytemplate", TS_GETACTIVITYTEMPLATE, 0,
   DEF_TASK_FUNC (ts_getactivitytemplate), FSVR_NONE},
#endif
  {"getcaslogfilelist", TS_GETCASLOGFILELIST, 0,
   DEF_TASK_FUNC (ts_getcaslogfilelist), FSVR_NONE},
  {"analyzecaslog", TS_ANALYZECASLOG, 0, DEF_TASK_FUNC (ts_analyzecaslog),
   FSVR_NONE},
  {"executecasrunner", TS_EXECUTECASRUNNER, 0,
   DEF_TASK_FUNC (ts_executecasrunner), FSVR_NONE},
  {"removecasrunnertmpfile", TS_REMOVECASRUNNERTMPFILE, 0,
   DEF_TASK_FUNC (ts_removecasrunnertmpfile), FSVR_NONE},
  {"getcaslogtopresult", TS_GETCASLOGTOPRESULT, 0,
   DEF_TASK_FUNC (ts_getcaslogtopresult), FSVR_NONE},
#if 0				/* ACTIVITY_PROFILE */
  {"addactivitylog", TS_ADDACTIVITYLOG, 0, DEF_TASK_FUNC (ts_addactivitylog),
   FSVR_NONE},
  {"updateactivitylog", TS_UPDATEACTIVITYLOG, 0,
   DEF_TASK_FUNC (ts_updateactivitylog), FSVR_NONE},
  {"removeactivitylog", TS_REMOVEACTIVITYLOG, 0,
   DEF_TASK_FUNC (ts_removeactivitylog), FSVR_NONE},
  {"getactivitylog", TS_GETACTIVITYLOG, 0, DEF_TASK_FUNC (ts_getactivitylog),
   FSVR_NONE},
#endif
#endif
#if 0
  {"broker_alarmmsg", TS2_BROKERALARMMSG, 0,
   DEF_TASK_FUNC (ts2_broker_alarm_msg)},
  {"broker_getparam", TS2_GETBROKERPARAM, 0,
   DEF_TASK_FUNC (ts2_get_broker_param)},
  {"getdbdir", TS_GETDBDIR, 0, DEF_TASK_FUNC (tsGetDBDirectory)},
  {"getsysparam", TS_GETSYSPARAM, 0, DEF_TASK_FUNC (ts_get_sysparam)},
  {"getunicasdconf", TS2_GETUNICASDCONF, 0,
   DEF_TASK_FUNC (ts2_get_unicasd_conf)},
  {"getunicasdinfo", TS2_GETUNICASDINFO, 0,
   DEF_TASK_FUNC (ts2_get_unicasd_info)},
  {"getuserinfo", TS_GETUSERINFO, 0, DEF_TASK_FUNC (ts_get_user_info)},
  {"popupspaceinfo", TS_POPSPACEINFO, 0, DEF_TASK_FUNC (ts_popup_space_info)},
  {"setsysparam2", TS_SETSYSPARAM2, 1, DEF_TASK_FUNC (ts_set_sysparam2)},
  {"setunicasdconf", TS2_SETUNICASDCONF, 1,
   DEF_TASK_FUNC (ts2_set_unicasd_conf)},
  {"startunicasd", TS2_STARTUNICASD, 0, DEF_TASK_FUNC (ts2_start_unicasd)},
  {"stopunicasd", TS2_STOPUNICASD, 0, DEF_TASK_FUNC (ts2_stop_unicasd)},
#endif
  {"dbmtuserlogin", TS_DBMTUSERLOGIN, 0, DEF_TASK_FUNC (tsDBMTUserLogin),
   FSVR_NONE},
  {"changeowner", TS_CHANGEOWNER, 1, DEF_TASK_FUNC (ts_change_owner),
   FSVR_CS},
  {"removelog", TS_REMOVE_LOG, 0, DEF_TASK_FUNC (ts_remove_log), FSVR_NONE},
  {NULL, TS_UNDEFINED, 0, NULL, FSVR_NONE}
};

void
uRemoveCRLF (char *str)
{
  int i;
  if (str == NULL)
    return;
  for (i = strlen (str) - 1; (i >= 0) && (str[i] == 10 || str[i] == 13); i--)
    {
      str[i] = '\0';
    }
}

char *
time_to_str (time_t t, char *fmt, char *buf, int type)
{
  struct tm ltm;

  ltm = *localtime (&t);
  if (type == TIME_STR_FMT_DATE)
    sprintf (buf, fmt, ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday);
  else if (type == TIME_STR_FMT_TIME)
    sprintf (buf, fmt, ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
  else				/* TIME_STR_FMT_DATE_TIME */
    sprintf (buf, fmt, ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday,
	     ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
  return buf;
}

int
uStringEqual (const char *str1, const char *str2)
{
  if (str1 == NULL || str2 == NULL)
    return 0;
  if (strcmp (str1, str2) == 0)
    return 1;
  return 0;
}

int
uStringEqualIgnoreCase (const char *str1, const char *str2)
{
  if (str1 == NULL || str2 == NULL)
    return 0;
  if (strcasecmp (str1, str2) == 0)
    return 1;
  return 0;
}

int
ut_error_log (nvplist * req, char *errmsg)
{
  char *id, *addr, *task, *stype;
  FILE *logf;
  char strbuf[512];
  int lock_fd;
  T_DBMT_FILE_ID log_fid;

  stype = nv_get_val (req, "_SVRTYPE");
  if ((task = nv_get_val (req, "task")) == NULL)
    task = "-";
  if ((addr = nv_get_val (req, "_CLIENTIP")) == NULL)
    addr = "-";
  if ((id = nv_get_val (req, "_ID")) == NULL)
    id = "-";
  if (errmsg == NULL)
    errmsg = "-";

  if (uStringEqual (stype, "psvr"))
    log_fid = FID_PSERVER_ERROR_LOG;
  else if (uStringEqual (stype, "fsvr"))
    log_fid = FID_FSERVER_ERROR_LOG;
  else
    return -1;

  lock_fd = uCreateLockFile (conf_get_dbmt_file (FID_LOCK_SVR_LOG, strbuf));
  if (lock_fd < 0)
    return -1;

  logf = fopen (conf_get_dbmt_file (log_fid, strbuf), "a");
  if (logf != NULL)
    {
      time_to_str (time (NULL), "%04d/%02d/%02d %02d:%02d:%02d", strbuf,
		   TIME_STR_FMT_DATE_TIME);
      fprintf (logf, "%s %s %s %s %s\n", strbuf, id, addr, task, errmsg);
      fflush (logf);
      fclose (logf);
    }

  uRemoveLockFile (lock_fd);
  return 1;
}

int
ut_access_log (nvplist * req, char *msg)
{
  char *id, *cli_addr, *task, *stype;
  FILE *logf;
  char strbuf[512];
  int lock_fd;
  T_DBMT_FILE_ID log_fid;

  if (req == NULL)
    return 1;

  stype = nv_get_val (req, "_SVRTYPE");
  if ((task = nv_get_val (req, "task")) == NULL)
    task = "-";
  if ((cli_addr = nv_get_val (req, "_CLIENTIP")) == NULL)
    cli_addr = "-";
  if ((id = nv_get_val (req, "_ID")) == NULL)
    id = "-";
  if (msg == NULL)
    msg = "";

  if (uStringEqual (stype, "psvr"))
    log_fid = FID_PSERVER_ACCESS_LOG;
  else if (uStringEqual (stype, "fsvr"))
    log_fid = FID_FSERVER_ACCESS_LOG;
  else
    return -1;

  lock_fd = uCreateLockFile (conf_get_dbmt_file (FID_LOCK_SVR_LOG, strbuf));
  if (lock_fd < 0)
    return -1;

  logf = fopen (conf_get_dbmt_file (log_fid, strbuf), "a");
  if (logf != NULL)
    {
      time_to_str (time (NULL), "%04d/%02d/%02d %02d:%02d:%02d", strbuf,
		   TIME_STR_FMT_DATE_TIME);
      fprintf (logf, "%s %s %s %s %s\n", strbuf, id, cli_addr, task, msg);

      fflush (logf);
      fclose (logf);
    }

  uRemoveLockFile (lock_fd);
  return 1;
}

int
ut_get_task_info (char *task, char *access_log_flag, T_TASK_FUNC * task_func)
{
  int i;

  for (i = 0; task_info[i].task_str != NULL; i++)
    {
      if (uStringEqual (task, task_info[i].task_str))
	{
	  if (access_log_flag)
	    *access_log_flag = task_info[i].access_log_flag;
	  if (task_func)
	    *task_func = task_info[i].task_func;
	  return task_info[i].task_code;
	}
    }

  return TS_UNDEFINED;
}

#ifndef FSERVER_SLAVE
int
ut_send_response (int fd, nvplist * res)
{
  int i;

  if (res == NULL)
    return -1;

  for (i = 0; i < res->nvplist_size; ++i)
    {
      if (res->nvpairs[i] == NULL)
	continue;
      write_to_socket (fd, dst_buffer (res->nvpairs[i]->name),
		       dst_length (res->nvpairs[i]->name));
      write_to_socket (fd, dst_buffer (res->delimiter), res->delimiter->dlen);
      write_to_socket (fd, dst_buffer (res->nvpairs[i]->value),
		       dst_length (res->nvpairs[i]->value));
      write_to_socket (fd, dst_buffer (res->endmarker), res->endmarker->dlen);
    }
  write_to_socket (fd, dst_buffer (res->listcloser), res->listcloser->dlen);

  return 0;
}

/*
 *  read incoming data and construct name-value pair list of request
 */
int
ut_receive_request (int fd, nvplist * req)
{
  int rc;
  char c;
  dstring *linebuf = NULL;
  char *p;

  linebuf = dst_create ();

  while ((rc = read_from_socket (fd, &c, 1)) == 1)
    {
      if (c == '\n')
	{
	  /* if null string, stop parsing */
	  if (dst_length (linebuf) == 0)
	    {
	      dst_destroy (linebuf);
	      return 1;
	    }

	  p = strchr (dst_buffer (linebuf), ':');
	  if (p)
	    {
	      *p = '\0';
	      p++;
	    }
	  nv_add_nvp (req, dst_buffer (linebuf), p);
	  dst_reset (linebuf);
	}
      else
	{
	  if (c != '\r')
	    dst_append (linebuf, &c, 1);
	}
    }

  dst_destroy (linebuf);
  return 0;
}


void
send_msg_with_file (int sock_fd, char *filename)
{
  FILE *res_file;
  char *file_name[10];
  char buf[1024];
  int file_num;
  long *file_size;
  int index, file_send_flag;
  int *del_flag;
#ifdef	_DEBUG_
  FILE *log_file;
  char log_filepath[1024];
#endif

  if (sock_fd < 0)
    return;

#ifdef	_DEBUG_
  sprintf (log_filepath, "%s/tmp/getfile.log", sco.szCubrid);
  log_file = fopen (log_filepath, "w+");
#endif

  memset (buf, '\0', sizeof (buf));


  file_send_flag = 1;
  /* point to file whether transfer or no */

  res_file = fopen (filename, "r");
  index = 0;
  if (res_file)
    {
      while (1)
	{
	  fgets (buf, 1024, res_file);
	  if (write_to_socket (sock_fd, buf, strlen (buf)) < 0)
	    return;

	  if (strncmp (buf, "status:failure", 14) == 0)
	    file_send_flag = 0;
	  /* nothing to send any file */

	  if (strncmp (buf, "file_num", 8) == 0)
	    {
	      file_num = atoi (buf + 9);
	      file_size = (long *) malloc (sizeof (long) * file_num);
	      del_flag = (int *) malloc (sizeof (int) * file_num);
	    }

	  if (strncmp (buf, "open:file", 9) == 0)
	    {
	      fgets (buf, 1024, res_file);	/* filename */
	      if (write_to_socket (sock_fd, buf, strlen (buf)) < 0)
		return;

	      file_name[index] = strdup (buf + 9);
	      *(file_name[index] + strlen (file_name[index]) - 1) = '\0';

	      fgets (buf, 1024, res_file);	/* filestatus */
	      if (write_to_socket (sock_fd, buf, strlen (buf)) < 0)
		return;

	      fgets (buf, 1024, res_file);	/* file size */
	      if (write_to_socket (sock_fd, buf, strlen (buf)) < 0)
		return;
	      file_size[index] = atoi (buf + 9);

	      fgets (buf, 1024, res_file);	/* delflag */
	      if (write_to_socket (sock_fd, buf, strlen (buf)) < 0)
		return;

	      del_flag[index] = (*(buf + 8) == 'y') ? 1 : 0;
	      fgets (buf, 1024, res_file);	/* close:file */
	      if (write_to_socket (sock_fd, buf, strlen (buf)) < 0)
		return;

	      index++;
	      if (file_num == index)
		break;
	    }
	}
      write_to_socket (sock_fd, "\n", 1);
      /* send \n to indicate last data because used fgets */
      fclose (res_file);
    }

  /* file send */
  if (file_send_flag == 1)
    {
      /* stay client's response */
      char recv_buf[100];
      int total_send;
      memset (recv_buf, '\0', sizeof (recv_buf));

      for (index = 0; index < file_num; index++)
	{
	  total_send = 0;

	  if (recv (sock_fd, recv_buf, sizeof (recv_buf), 0) < 0)
	    {
#ifndef	WIN32
	      if (errno == EINTR)
		{
		  break;
		}
	      else if (errno == EBADF)
		{
		  break;
		}
	      else if (errno == ENOTSOCK)
		{
		  break;
		}
	      else
		{
		  break;
		}
#else
	      break;
#endif
	    }
	  else if (strncmp (recv_buf, "ACK", 3) != 0)
	    {
	      break;
	    }
#ifdef	_DEBUG_
	  if (log_file)
	    {
	      fprintf (log_file, "filename : %s\n", file_name[index]);
	      fprintf (log_file, "filesize : %d bytes\n", file_size[index]);
	    }
	  total_send =
	    send_file_to_client (sock_fd, file_name[index], log_file);
#else
	  total_send = send_file_to_client (sock_fd, file_name[index], NULL);
#endif



	  if (total_send != file_size[index])
	    {
#ifdef	_DEBUG_
	      fprintf (log_file,
		       "file send error totalsend:%d file_size:%d\n",
		       total_send, file_size[index]);
#endif
	      break;
	    }
#ifdef	_DEBUG_
	  fprintf (log_file, "file_name : %s\n", file_name[index]);
#endif
	}
    }

  for (index = 0; index < file_num; index++)
    {
      if (del_flag[index] == 1)
	unlink (file_name[index]);
      /* remove compress file */

      FREE_MEM (file_name[index]);
    }

  FREE_MEM (file_size);
  FREE_MEM (del_flag);
#ifdef	_DEBUG_
  if (log_file)
    fclose (log_file);
#endif
}

int
send_file_to_client (int sock_fd, char *file_name, FILE * log_file)
{
  int send_file;
  int read_len;
  long total_send;
  char buf[5120];

  memset (buf, '\0', sizeof (buf));
#ifdef	WIN32
  send_file = open (file_name, O_RDONLY | O_BINARY);
#else
  send_file = open (file_name, O_RDONLY);
#endif

  total_send = 0;

  if (send_file < 0)
    {
      return 0;
    }

  while ((read_len = read (send_file, buf, sizeof (buf))) > 0)
    {
      if (write_to_socket (sock_fd, buf, read_len) != -1)
	{
	  total_send += read_len;
	}
      else
	{
	  CLOSE_SOCKET (sock_fd);
#ifdef	_DEBUG_
	  fprintf (log_file, "\nCan't Send to socket...\n");
#endif
	  break;
	}
    }
  close (send_file);
#ifdef	_DEBUG_
  if (log_file)
    {
      fprintf (log_file, "\nSend to client(in func) => %d byte\n",
	       total_send);
    }
#endif
  return total_send;
}

#endif

void
ut_daemon_start ()
{
#ifdef WIN32
  return;
#else
  int childpid;

  /* Ignore the terminal stop signals */
  signal (SIGTTOU, SIG_IGN);
  signal (SIGTTIN, SIG_IGN);
  signal (SIGTSTP, SIG_IGN);

#if 0
  /* to make it run in background */
  signal (SIGHUP, SIG_IGN);
  childpid = PROC_FORK ();
  if (childpid > 0)
    exit (0);			/* kill parent */
#endif

  /* setpgrp(); */
  setsid ();			/* become process group leader and  */
  /* disconnect from control terminal */

  signal (SIGHUP, SIG_IGN);
  childpid = PROC_FORK ();	/* to prevent from reaquiring control terminal */
  if (childpid > 0)
    exit (0);			/* kill parent */

#if 0
  /* change current working directory */
  chdir ("/");
  /* clear umask */
  umask (0);
#endif
#endif /* ifndef WIN32 */
}

int
ut_write_pid (char *pid_file)
{
  FILE *pidfp;

  pidfp = fopen (pid_file, "w");
  if (pidfp == NULL)
    {
      perror ("fopen");
      return -1;
    }
  fprintf (pidfp, "%d\n", (int) getpid ());
  fclose (pidfp);
  return 0;
}

void
server_fd_clear (int srv_fd)
{
#if !defined(WINDOWS)
  int i;
  int fd;

  for (i = 3; i < 1024; i++)
    {
      if (i != srv_fd)
	close (i);
    }

  fd = open ("/dev/null", O_RDWR);
#ifndef DIAG_DEBUG
  dup2 (fd, 1);
#endif
  dup2 (fd, 2);
#endif /* !WINDOWS */
}

int
uRetrieveDBDirectory (char *dbname, char *target)
{
  int ret_val;
#ifdef	WIN32
  char temp_name[512];
#endif

  ret_val = _uReadDBtxtFile (dbname, 1, target);
#ifdef	WIN32
  if (ret_val == ERR_NO_ERROR)
    {
      strcpy (temp_name, target);
      memset (target, '\0', strlen (target));
      if (GetLongPathName (temp_name, target, MAX_PATH) == 0)
	{
	  strcpy (target, temp_name);
	}
    }
#endif

  return ret_val;
}

int
uRetrieveDBLogDirectory (char *dbname, char *target)
{
  int ret_val;
#ifdef	WIN32
  char temp_name[512];
#endif

  ret_val = _uReadDBtxtFile (dbname, 3, target);
#ifdef	WIN32
  if (ret_val == ERR_NO_ERROR)
    {
      strcpy (temp_name, target);
      memset (target, '\0', strlen (target));
      if (GetLongPathName (temp_name, target, MAX_PATH) == 0)
	{
	  strcpy (target, temp_name);
	}
    }
#endif
  return ret_val;
}

int
uIsDatabaseActive (char *dbn)
{
  T_COMMDB_RESULT *cmd_res;
  int retval = 0;

  if (dbn == NULL)
    return 0;
  cmd_res = cmd_commdb ();
  if (cmd_res != NULL)
    {
      retval = uIsDatabaseActive2 (cmd_res, dbn);
      cmd_commdb_result_free (cmd_res);
    }
  return retval;
}

int
uIsDatabaseActive2 (T_COMMDB_RESULT * cmd_res, char *dbn)
{
  T_COMMDB_INFO *info;
  int i;

  if (cmd_res == NULL)
    return 0;

  info = (T_COMMDB_INFO *) cmd_res->result;
  for (i = 0; i < cmd_res->num_result; i++)
    {
      if (strcmp (info[i].db_name, dbn) == 0)
	{
	  return 1;
	}
    }
  return 0;
}

T_DB_SERVICE_MODE
uDatabaseMode (char *dbname)
{
  int pid;

  if (uIsDatabaseActive (dbname))
    return DB_SERVICE_MODE_CS;

  pid = get_db_server_pid (dbname);
  if (pid > 0)
    {
      if (kill (pid, 0) >= 0)
	{
	  return DB_SERVICE_MODE_SA;
	}
    }
  return DB_SERVICE_MODE_NONE;
}

int
_isRegisteredDB (char *dn)
{
  if (_uReadDBtxtFile (dn, 0, NULL) == ERR_NO_ERROR)
    return 1;

  return 0;
}

/* dblist should have enough space  */
/* db names are delimited by '\0' for ease of using it */
int
uReadDBnfo (char *dblist)
{
  int retval = 0;
  char strbuf[512];
  char *p;
  FILE *infp;
  int lock_fd;

  lock_fd =
    uCreateLockFile (conf_get_dbmt_file (FID_LOCK_PSVR_DBINFO, strbuf));
  if (lock_fd < 0)
    return -1;

  infp = fopen (conf_get_dbmt_file (FID_PSVR_DBINFO_TEMP, strbuf), "r");
  if (infp == NULL)
    {
      retval = -1;
    }
  else
    {
      fgets (strbuf, sizeof (strbuf), infp);
      retval = atoi (strbuf);

      p = dblist;
      while (fgets (strbuf, sizeof (strbuf), infp))
	{
	  ut_trim (strbuf);
	  strcpy (p, strbuf);
	  p += (strlen (p) + 1);
	}
      fclose (infp);
    }

  uRemoveLockFile (lock_fd);

  return retval;
}

void
uWriteDBnfo (void)
{
  T_COMMDB_RESULT *cmd_res;

  cmd_res = cmd_commdb ();
  uWriteDBnfo2 (cmd_res);
  cmd_commdb_result_free (cmd_res);
}

void
uWriteDBnfo2 (T_COMMDB_RESULT * cmd_res)
{
  int i;
  int dbcnt;
  char strbuf[1024];
  int dbvect[MAX_INSTALLED_DB];
  int lock_fd;
  FILE *outfp;
  T_COMMDB_INFO *info;

  lock_fd =
    uCreateLockFile (conf_get_dbmt_file (FID_LOCK_PSVR_DBINFO, strbuf));
  if (lock_fd < 0)
    return;

  outfp = fopen (conf_get_dbmt_file (FID_PSVR_DBINFO_TEMP, strbuf), "w");
  if (outfp != NULL)
    {
      dbcnt = 0;
      if (cmd_res == NULL)
	{
	  fprintf (outfp, "%d\n", dbcnt);
	}
      else
	{
	  info = (T_COMMDB_INFO *) cmd_res->result;
	  for (i = 0; i < cmd_res->num_result; i++)
	    {
	      if (_isRegisteredDB (info[i].db_name))
		{
		  dbvect[dbcnt] = i;
		  ++dbcnt;
		}
	    }
	  fprintf (outfp, "%d\n", dbcnt);
	  info = (T_COMMDB_INFO *) cmd_res->result;
	  for (i = 0; i < dbcnt; i++)
	    fprintf (outfp, "%s\n", info[dbvect[i]].db_name);
	}
      fclose (outfp);
    }

  uRemoveLockFile (lock_fd);
}

int
uCreateLockFile (char *lockfile)
{
  int outfd;
#ifndef WIN32
  struct flock lock;
#endif

  outfd = open (lockfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);

  if (outfd < 0)
    return outfd;

#ifdef WIN32
  while (_locking (outfd, _LK_NBLCK, 1) < 0)
    Sleep (100);
#else
  lock.l_type = F_WRLCK;
  lock.l_start = 0;
  lock.l_whence = SEEK_SET;
  lock.l_len = 0;

  while (fcntl (outfd, F_SETLK, &lock) < 0)
    SLEEP_MILISEC (0, 10);
#endif

  return outfd;
}

void
uRemoveLockFile (int outfd)
{
#ifndef WIN32
  struct flock lock;

  lock.l_type = F_UNLCK;
  lock.l_start = 0;
  lock.l_whence = SEEK_SET;
  lock.l_len = 0;
#endif

#ifdef WIN32
  _locking (outfd, _LK_UNLCK, 1);
#else
  fcntl (outfd, F_SETLK, &lock);
#endif
  close (outfd);
}

int
uRemoveDir (char *dir, int remove_file_in_dir)
{
  char path[1024];
  char command[2048];

  if (dir == NULL)
    return ERR_DIR_REMOVE_FAIL;

  strcpy (path, dir);
  memset (command, '\0', sizeof (command));
  ut_trim (path);

#ifdef WIN32
  unix_style_path (path);
#endif

  if (access (path, F_OK) == 0)
    {
      if (remove_file_in_dir == REMOVE_DIR_FORCED)
	{
	  sprintf (command, "%s %s %s", DEL_DIR, DEL_DIR_OPT, path);
	  if (system (command) == -1)
	    return ERR_DIR_REMOVE_FAIL;
	}
      else
	{
	  if (rmdir (path) == -1)
	    return ERR_DIR_REMOVE_FAIL;
	}
    }
  else
    return ERR_DIR_REMOVE_FAIL;

  return ERR_NO_ERROR;
}



int
uCreateDir (char *new_dir)
{
  char *p, path[1024];

  if (new_dir == NULL)
    return ERR_DIR_CREATE_FAIL;

  strcpy (path, new_dir);
  ut_trim (path);

#ifdef WIN32
  unix_style_path (path);
#endif

#ifdef WIN32
  if (path[0] == '/')
    p = path + 1;
  else if (strlen (path) > 3 && path[2] == '/')
    p = path + 3;
  else
    return ERR_DIR_CREATE_FAIL;
#else
  if (path[0] != '/')
    return ERR_DIR_CREATE_FAIL;
  p = path + 1;
#endif

  while (p != NULL)
    {
      p = strchr (p, '/');
      if (p != NULL)
	*p = '\0';
      if (access (path, F_OK) < 0)
	{
	  if (mkdir (path, 0700) < 0)
	    {
	      return ERR_DIR_CREATE_FAIL;
	    }
	}
      if (p != NULL)
	{
	  *p = '/';
	  p++;
	}
    }
  return ERR_NO_ERROR;
}

void
close_all_fds (int init_fd)
{
  int i;

  for (i = init_fd; i < 1024; i++)
    {
      close (i);
    }
}

char *
ut_trim (char *str)
{
  char *p;
  char *s;

  if (str == NULL)
    return (str);

  for (s = str;
       *s != '\0' && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r');
       s++)
    ;
  if (*s == '\0')
    {
      *str = '\0';
      return (str);
    }

  /* *s must be a non-white char */
  for (p = s; *p != '\0'; p++)
    ;
  for (p--; *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'; p--)
    ;
  *++p = '\0';

  if (s != str)
    memcpy (str, s, strlen (s) + 1);

  return (str);
}

#ifdef WIN32
int
ut_disk_free_space (char *path)
{
  char buf[1024];
  DWORD a, b, c, d;

  strcpy (buf, path);
  ut_trim (buf);
  if (buf[1] == ':')
    strcpy (buf + 2, "/");
  else
    strcpy (buf, "c:/");

  if (GetDiskFreeSpace (buf, &a, &b, &c, &d) == 0)
    return 0;

  return ((c >> 10) * ((b * a) >> 10));
}
#else
int
ut_disk_free_space (char *path)
{
  struct statvfs sv;

  if (statvfs (path, &sv) < 0)
    return 0;

  return ((((sv.f_bavail) >> 10) * sv.f_frsize) >> 10);
}
#endif

char *
ip2str (unsigned char *ip, char *ip_str)
{
  sprintf (ip_str, "%d.%d.%d.%d", (unsigned char) ip[0],
	   (unsigned char) ip[1],
	   (unsigned char) ip[2], (unsigned char) ip[3]);
  return ip_str;
}

int
string_tokenize_accept_laststring_space (char *str, char *tok[], int num_tok)
{
  int i;
  char *p;

  tok[0] = str;
  for (i = 1; i < num_tok; i++)
    {
      tok[i] = strpbrk (tok[i - 1], " \t");
      if (tok[i] == NULL)
	return -1;
      *(tok[i]) = '\0';
      p = (tok[i]) + 1;
      for (; *p && (*p == ' ' || *p == '\t'); p++)
	;
      if (*p == '\0')
	return -1;
      tok[i] = p;
    }

  return 0;
}

int
string_tokenize (char *str, char *tok[], int num_tok)
{
  int i;
  char *p;

  tok[0] = str;
  for (i = 1; i < num_tok; i++)
    {
      tok[i] = strpbrk (tok[i - 1], " \t");
      if (tok[i] == NULL)
	return -1;
      *(tok[i]) = '\0';
      p = (tok[i]) + 1;
      for (; *p && (*p == ' ' || *p == '\t'); p++)
	;
      if (*p == '\0')
	return -1;
      tok[i] = p;
    }
  p = strpbrk (tok[num_tok - 1], " \t");
  if (p)
    *p = '\0';

  return 0;
}

int
string_tokenize2 (char *str, char *tok[], int num_tok, int c)
{
  int i;
  char *p;

  for (i = 0; i < num_tok; i++)
    tok[i] = NULL;

  if (str == NULL)
    return -1;

  tok[0] = str;
  for (i = 1; i < num_tok; i++)
    {
      tok[i] = strchr (tok[i - 1], c);
      if (tok[i] == NULL)
	return -1;
      *(tok[i]) = '\0';
      (tok[i])++;
    }
  p = strchr (tok[num_tok - 1], c);
  if (p)
    *p = '\0';

  return 0;
}

int
get_db_server_pid (char *dbname)
{
  char srv_lock_file[1024];
  FILE *fp;
  int pid = -1;

  if (uRetrieveDBLogDirectory (dbname, srv_lock_file) != ERR_NO_ERROR)
    {
      return -1;
    }
  sprintf (srv_lock_file + strlen (srv_lock_file), "/%s%s", dbname,
	   CUBRID_SERVER_LOCK_EXT);
  if ((fp = fopen (srv_lock_file, "r")) != NULL)
    {
      if (fscanf (fp, "%*s %d", &pid) < 1)
	pid = -1;
      fclose (fp);
    }
  return pid;
}

static int
_uReadDBtxtFile (char *dn, int idx, char *outbuf)
{
  char strbuf[1024];
  FILE *dbf;
  char *value_p[4];
  int retval = ERR_DBDIRNAME_NULL;

  if (outbuf)
    outbuf[0] = '\0';

  sprintf (strbuf, "%s/%s", sco.szCubrid_databases, CUBRID_DATABASE_TXT);
  dbf = fopen (strbuf, "r");
  if (dbf == NULL)
    return ERR_DBDIRNAME_NULL;

  while (fgets (strbuf, sizeof (strbuf), dbf))
    {
      ut_trim (strbuf);
      if (string_tokenize (strbuf, value_p, 4) < 0)
	continue;
      if (strcmp (value_p[0], dn) == 0)
	{
	  if (outbuf)
	    {
	      strcpy (outbuf, value_p[idx]);
#ifdef WIN32
	      unix_style_path (outbuf);
#endif
	    }
	  retval = ERR_NO_ERROR;
	  break;
	}
    }
  fclose (dbf);

  return retval;
}

#ifndef FSERVER_SLAVE
int
read_from_socket (SOCKET sock_fd, char *buf, int size)
{
  int read_len;
  fd_set read_mask;
  int nfound;
  int maxfd;
  struct timeval timeout_val = { 5, 0 };

  timeout_val.tv_sec = 5;
  timeout_val.tv_usec = 0;

  FD_ZERO (&read_mask);
  FD_SET (sock_fd, (fd_set *) & read_mask);
  maxfd = sock_fd + 1;
  nfound =
    select (maxfd, &read_mask, (fd_set *) 0, (fd_set *) 0, &timeout_val);
  if (nfound < 0)
    {
      return -1;
    }

  if (FD_ISSET (sock_fd, (fd_set *) & read_mask))
    {
      read_len = recv (sock_fd, buf, size, 0);
    }
  else
    {
      return -1;
    }

  return read_len;
}

int
write_to_socket (SOCKET sock_fd, char *buf, int size)
{
  int write_len;
  fd_set write_mask;
  int nfound;
  int maxfd;
  struct timeval timeout_val;

  timeout_val.tv_sec = 5;
  timeout_val.tv_usec = 0;

  if (sock_fd < 0)
    {
      return -1;
    }

  FD_ZERO (&write_mask);
  FD_SET (sock_fd, (fd_set *) & write_mask);
  maxfd = sock_fd + 1;
  nfound =
    select (maxfd, (fd_set *) 0, &write_mask, (fd_set *) 0, &timeout_val);
  if (nfound < 0)
    {
      return -1;
    }

  if (FD_ISSET (sock_fd, (fd_set *) & write_mask))
    {
      write_len = send (sock_fd, buf, size, 0);
    }
  else
    {
      return -1;
    }

  return write_len;
}
#endif

#ifdef WIN32
int
kill (int pid, int signo)
{
  HANDLE phandle;

  phandle = OpenProcess (PROCESS_TERMINATE, FALSE, pid);
  if (phandle == NULL)
    {
      int error = GetLastError ();
      if (error == ERROR_ACCESS_DENIED)
	errno = EPERM;
      else
	errno = ESRCH;
      return -1;
    }

  if (signo == SIGTERM)
    TerminateProcess (phandle, 0);

  CloseHandle (phandle);
  return 0;
}
#endif

#ifdef WIN32
int
run_child (char *argv[], int wait_flag, char *stdin_file, char *stdout_file,
	   char *stderr_file, int *exit_status)
{
  int new_pid;
  STARTUPINFO start_info;
  PROCESS_INFORMATION proc_info;
  BOOL res;
  int i, cmd_arg_len;
  char cmd_arg[1024];
  FILE *fp;
  int saved_0_fd, saved_1_fd, saved_2_fd;
  BOOL inherit_flag = FALSE;

  if (exit_status != NULL)
    *exit_status = 0;

  for (i = 0, cmd_arg_len = 0; argv[i]; i++)
    {
      cmd_arg_len += sprintf (cmd_arg + cmd_arg_len, "\"%s\" ", argv[i]);
    }

  GetStartupInfo (&start_info);
  start_info.wShowWindow = SW_HIDE;

  if (stdin_file)
    {
      saved_0_fd = dup (0);
      fp = fopen (stdin_file, "r");
      if (fp)
	{
	  dup2 (fileno (fp), 0);
	  fclose (fp);
	}
      start_info.dwFlags = STARTF_USESTDHANDLES;
      start_info.hStdInput = GetStdHandle (STD_INPUT_HANDLE);
      inherit_flag = TRUE;
    }
  if (stdout_file)
    {
      saved_1_fd = dup (1);
      unlink (stdout_file);
      fp = fopen (stdout_file, "w");
      if (fp)
	{
	  dup2 (fileno (fp), 1);
	  fclose (fp);
	}
      start_info.dwFlags = STARTF_USESTDHANDLES;
      start_info.hStdOutput = GetStdHandle (STD_OUTPUT_HANDLE);
      inherit_flag = TRUE;
    }
  if (stderr_file)
    {
      saved_2_fd = dup (2);
      unlink (stderr_file);
      fp = fopen (stderr_file, "w");
      if (fp)
	{
	  dup2 (fileno (fp), 2);
	  fclose (fp);
	}
      start_info.dwFlags = STARTF_USESTDHANDLES;
      start_info.hStdError = GetStdHandle (STD_ERROR_HANDLE);
      inherit_flag = TRUE;
    }

  res = CreateProcess (argv[0], cmd_arg, NULL, NULL, inherit_flag,
		       0, NULL, NULL, &start_info, &proc_info);

  if (stdin_file)
    {
      dup2 (saved_0_fd, 0);
      close (saved_0_fd);
    }
  if (stdout_file)
    {
      dup2 (saved_1_fd, 1);
      close (saved_1_fd);
    }
  if (stderr_file)
    {
      dup2 (saved_2_fd, 2);
      close (saved_2_fd);
    }

  if (res == FALSE)
    {
      return -1;
    }

  new_pid = proc_info.dwProcessId;

  if (wait_flag)
    {
      DWORD status = 0;
      WaitForSingleObject (proc_info.hProcess, INFINITE);
      GetExitCodeProcess (proc_info.hProcess, &status);
      if (exit_status != NULL)
	*exit_status = status;
      CloseHandle (proc_info.hProcess);
      CloseHandle (proc_info.hThread);
      return 0;
    }
  else
    {
      CloseHandle (proc_info.hProcess);
      CloseHandle (proc_info.hThread);
      return new_pid;
    }
}
#else
int
run_child (char *argv[], int wait_flag, char *stdin_file, char *stdout_file,
	   char *stderr_file, int *exit_status)
{
  int pid;

  if (exit_status != NULL)
    *exit_status = 0;

  if (wait_flag)
    signal (SIGCHLD, SIG_DFL);
  else
    signal (SIGCHLD, SIG_IGN);
  pid = PROC_FORK ();
  if (pid == 0)
    {
      FILE *fp;

      close_all_fds (3);

      if (stdin_file)
	{
	  fp = fopen (stdin_file, "r");
	  if (fp)
	    {
	      dup2 (fileno (fp), 0);
	      fclose (fp);
	    }
	}
      if (stdout_file)
	{
	  unlink (stdout_file);
	  fp = fopen (stdout_file, "w");
	  if (fp)
	    {
	      dup2 (fileno (fp), 1);
	      fclose (fp);
	    }
	}
      if (stderr_file)
	{
	  unlink (stderr_file);
	  fp = fopen (stderr_file, "w");
	  if (fp)
	    {
	      dup2 (fileno (fp), 2);
	      fclose (fp);
	    }
	}

      execv (argv[0], argv);
      exit (0);
    }

  if (pid < 0)
    return -1;

  if (wait_flag)
    {
      int status = 0;
      waitpid (pid, &status, 0);
      if (exit_status != NULL)
	*exit_status = status;
      return 0;
    }
  else
    {
      return pid;
    }
}
#endif

#ifdef WIN32
void
unix_style_path (char *path)
{
  char *p;
  for (p = path; *p; p++)
    {
      if (*p == '\\')
	*p = '/';
    }
}

char *
nt_style_path (char *path, char *new_path_buf)
{
  char *p;
  char *q;
  char tmp_path_buf[1024];

  if (path == new_path_buf)
    {
      strcpy (tmp_path_buf, path);
      q = tmp_path_buf;
    }
  else
    {
      q = new_path_buf;
    }
  if (strlen (path) < 2 || path[1] != ':')
    {
      *q++ = _getdrive () + 'A' - 1;
      *q++ = ':';
      *q++ = '\\';
    }

  for (p = path; *p; p++, q++)
    {
      if (*p == '/')
	*q = '\\';
      else
	*q = *p;
    }
  *q = '\0';
  for (q--; q != new_path_buf; q--)
    {
      if (*q == '\\')
	*q = '\0';
      else
	break;
    }
  if (*q == ':')
    {
      q++;
      *q++ = '\\';
      *q++ = '\\';
      *q = '\0';
    }
  if (path == new_path_buf)
    {
      strcpy (new_path_buf, tmp_path_buf);
    }
  return new_path_buf;
}
#endif

void
wait_proc (int pid)
{
#ifdef WIN32
  HANDLE h;
#endif

  if (pid <= 0)
    return;

#ifdef WIN32
  h = OpenProcess (SYNCHRONIZE, FALSE, pid);
  if (h)
    {
      WaitForSingleObject (h, INFINITE);
      CloseHandle (h);
    }
#else
  while (1)
    {
      if (kill (pid, 0) < 0)
	break;
      SLEEP_MILISEC (0, 100);
    }
#endif
}

int
make_version_info (char *cli_ver, int *major_ver, int *minor_ver)
{
  char *p;
  if (cli_ver == NULL)
    return 0;

  p = cli_ver;
  *major_ver = atoi (p);
  p = strchr (p, '.');
  if (p != NULL)
    *minor_ver = atoi (p + 1);
  else
    *minor_ver = 0;

  return 1;
}

int
file_copy (char *src_file, char *dest_file)
{
  char strbuf[1024];
  int src_fd, dest_fd;
  int read_size, write_size;

  if ((src_fd = open (src_file, O_RDONLY)) == -1)
    return -1;

  if ((dest_fd = open (dest_file, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1)
    {
      close (src_fd);
      return -1;
    }

#ifdef WIN32
  if (setmode (src_fd, O_BINARY) == -1 || setmode (dest_fd, O_BINARY) == -1)
    {
      close (src_fd);
      close (dest_fd);
      return -1;
    }
#endif

  while ((read_size = read (src_fd, strbuf, sizeof (strbuf))) > 0)
    {
      if ((write_size = write (dest_fd, strbuf, read_size)) < read_size)
	{
	  close (src_fd);
	  close (dest_fd);
	  unlink (dest_file);
	  return -1;
	}
    }

  close (src_fd);
  close (dest_fd);

  return 0;
}

int
move_file (char *src_file, char *dest_file)
{
  if (file_copy (src_file, dest_file) < 0)
    {
      return -1;
    }
  else
    unlink (src_file);

  return 0;
}

#ifdef WIN32
void
remove_end_of_dir_ch (char *path)
{
  if (path && path[strlen (path) - 1] == '\\')
    path[strlen (path) - 1] = '\0';
}
#endif

int
is_cmserver_process (int pid, char *module_name)
{
#ifdef WIN32
  HANDLE hModuleSnap = NULL;
  MODULEENTRY32 me32 = { 0 };

  hModuleSnap = CreateToolhelp32Snapshot (TH32CS_SNAPMODULE, pid);
  if (hModuleSnap == (HANDLE) - 1)
    return -1;

  me32.dwSize = sizeof (MODULEENTRY32);

  if (Module32First (hModuleSnap, &me32))
    {
      do
	{
	  if (strcasecmp (me32.szModule, module_name) == 0)
	    {
	      CloseHandle (hModuleSnap);
	      return 1;
	    }
	}
      while (Module32Next (hModuleSnap, &me32));
    }
  CloseHandle (hModuleSnap);
  return 0;
#else
  char *argv[10];
  int argc = 0;
  char cmjs_pid[10];
  char result_file[1024];
  char buf[1024], cur_pid[10], prog_name[32];
  FILE *fRes;

  sprintf (cmjs_pid, "%d", pid);
  sprintf (result_file, "%s/DBMT_js.%d", sco.dbmt_tmp_dir, (int) getpid ());
  argv[argc++] = "/bin/ps";
  argv[argc++] = "-e";
  argv[argc] = NULL;

  if (run_child (argv, 1, NULL, result_file, NULL, NULL) < 0)
    {				/* ps */
      return -1;
    }

  fRes = fopen (result_file, "r");
  if (fRes)
    {
      while (fgets (buf, 1024, fRes))
	{
	  if (sscanf (buf, "%s %*s %*s %s", cur_pid, prog_name) != 2)
	    {
	      continue;
	    }

	  if (!strcmp (cur_pid, cmjs_pid) && !strcmp (prog_name, module_name))
	    {
	      fclose (fRes);
	      unlink (result_file);
	      return 1;
	    }
	}

      fclose (fRes);
    }

  unlink (result_file);
  return 0;
#endif
}

int
make_default_env ()
{
  int retval = ERR_NO_ERROR;
  char strbuf[512];
  FILE *fd;

  /* create log/manager directory */
  sprintf (strbuf, "%s/%s", sco.szCubrid, DBMT_LOG_DIR);
  if ((retval = uCreateDir (strbuf)) != ERR_NO_ERROR)
    return retval;

  /* create var/manager direcory */
  sprintf (strbuf, "%s/%s", sco.szCubrid, DBMT_PID_DIR);
  if ((retval = uCreateDir (strbuf)) != ERR_NO_ERROR)
    return retval;

  /* if databases.txt file doesn't exist, create 0 byte file. */
  sprintf (strbuf, "%s/%s", sco.szCubrid_databases, CUBRID_DATABASE_TXT);
  if (access (strbuf, F_OK) < 0)
    {
      if ((fd = fopen (strbuf, "a")) == NULL)
	return ERR_FILE_CREATE_FAIL;
      fclose (fd);
    }
  return retval;
}
