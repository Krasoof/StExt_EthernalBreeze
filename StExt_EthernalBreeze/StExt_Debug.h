#pragma once

namespace Gothic_II_Addon
{
	//-----------------------------------------------------------------
	//							DEBUG
	//-----------------------------------------------------------------
	class zSTRING;
	class oCNpc;

	struct EXCEPTION_DESCR_STRUCT
	{
		char* Description[20];
		int	NumDecs;
	};

	#define DebugEnabled false
	#define DebugItemInstanceNameEnabled false

	#if DebugEnabled
		#define DEBUG_MSG(message) do { DebugMessage(message); } while(0)
		#define DEBUG_MSG_FUNC(funcName, message) do { DebugFuncMessage(funcName, message); } while(0)
		#define DEBUG_MSG_DAM(funcName, message, atk, target) do { DebugDamageMessage(funcName, message, atk, target); } while(0)
		#define DEBUG_MSG_IF(condition, message) do { if (condition) { DebugMessage(message); } } while(0)
		#define DEBUG_MSG_IFELSE(condition, message_true, message_false) do { if (condition) { DebugMessage(message_true); } else { DebugMessage(message_false); } } while(0)
		#define DEBUG_MSG_SCRIPTCALLS do { PrintDebugScriptCallStack(); } while(0)
	#else
		#define DEBUG_MSG(message) do { (void)sizeof(message); } while(0)
		#define DEBUG_MSG_FUNC(funcName, message) do { (void)sizeof(funcName); (void)sizeof(message); } while(0)
		#define DEBUG_MSG_DAM(funcName, message, atk, target) do { (void)sizeof(funcName); (void)sizeof(message); (void)sizeof(atk); (void)sizeof(target); } while(0)
		#define DEBUG_MSG_IF(condition, message) do { (void)(condition); } while(0)
		#define DEBUG_MSG_IFELSE(condition, message_true, message_false) do { (void)(condition); } while(0)
		#define DEBUG_MSG_SCRIPTCALLS do { } while(0)
	#endif

	constexpr int SCRIPT_CALL_STACK_SIZE = 256;
	extern int ScriptCallStack[SCRIPT_CALL_STACK_SIZE];
	extern int ScriptCallStackIndex;

	void CreateDebugFile();
	void ExceptionHandlerCallback(EXCEPTION_DESCR_STRUCT* desc);

	extern void PrintDebug(zSTRING message);
	extern void DebugMessage(zSTRING message);
	extern void DebugFuncMessage(zSTRING funcName, zSTRING message);
	extern void DebugDamageMessage(zSTRING funcName, zSTRING message, oCNpc* atk, oCNpc* target);

	extern void PushScriptCallStackPos(const int pos);
	extern void PrintDebugScriptCallStack();
}