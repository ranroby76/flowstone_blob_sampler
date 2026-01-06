#ifndef H_FlowStoneSdk_H
#define H_FlowStoneSdk_H

#define FS_SDK_VERSION      1

#pragma pack(push,8)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Prototype @FsSdkMain function
typedef bool (*FsSdkMainFunc)(unsigned /*op*/, void* /*arg*/);

// op for @FsSdkMain
enum FsSdkRequest
{
	fsrInit					= 0,	// passes READ NULL
									//	called after the DLL is attached
	fsrDeInit				= 1,	// passes READ NULL
									//	called before the DLL is detached
	fsrDescribeModule		= 2,	// passes WRITE @FsSdkDescribeModule
									//	called to get general information about a Module
	fsrDescribeIO			= 3,	// passes WRITE @FsSdkDescribeIO
									//	called to get information about a single I/O connector
	fsrCreateInstance		= 4,	// passes READ/WRITE @FsSdkCreateInstance
									//	called when an instance of the module is created
	fsrDestroyInstance		= 5,	// passes READ pInstance for de-allocation from earlier @fsrCreateInstance
									//	called when an instance of a module gets destroyed
	fsrSaveInstanceSize		= 6,	// passes READ/WRITE @FsSdkSaveInstanceSize
									//	called to get information on how much memory is required for saving module instance data
									//	FS will allocate this size of memory and pass it into @fsrSaveInstanceData call
									//	for the module to write into
	fsrSaveInstanceData		= 7,	// passes READ/WRITE @FsSdkSaveInstanceData
									//	called to save module instance data to disk or clipboard
	fsrTrigger				= 8,	// passes READ @FsSdkTrigger
									//	called when a trigger is received on a green/yellow input or timer/schematic event
	fsrStream				= 9,	// passes READ/WRITE @FsSdkStream
									//	called when the module has at least one stream input or output to process
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// arg for @fsrDescribeModule
struct FsSdkDescribeModule
{
	unsigned sdkVersion;			// WRITE This is for future compatibility, should be set to FS_SDK_VERSION
	enum Embed
	{
		embedMemory   = 0,			// Default, DLL is mapped into memory and loaded from there
		embedTempFile = 1,			// DLL is extracted into %temp% folder and loaded through Windows
		embedDisabled = 2,			// DLL is not allowed to be embedded
	};

	unsigned embed;					// WRITE Use @FsSdkDescribeModule::Embed to describes how the embedding is handled when exporting VST
	char name[128];					// WRITE Name that appears in the toolbox
	char desc[256];					// WRITE Description that appears in tooltip/statusbar
	char caption[32];				// WRITE Short name at the top of the module
	unsigned nrInputs;				// WRITE Number of inputs
	unsigned nrOutputs;				// WRITE Number or outputs
	unsigned timerMS;				// WRITE If the module need a timer, set it to the time in milliseconds, otherwise 0
	char future[256];				// Don't touch, reserved for future additions
};

// arg for @fsrDescribeIO
struct FsSdkDescribeIO
{
	enum Type
	{
		iotNone        =  0,		// Should never occur, indicates invalid connector
		iotTrigger     =  1,		// Trigger (green connector)
		iotBoolean     =  2,		// Boolean (green connector)
		iotInt         =  3,		// Integer (green connector)
		iotFloat       =  4,		// Float (green connector)
		iotString      =  5,		// String (yellow connector)
		iotPoint       =  6,		// Point (yellow connector)
		iotColor       =  7,		// Color (yellow connector)
		iotArea        =  8,		// Area (yellow connector)

		iotFloatArray  =  9,		// Float Array (yellow connector)
		iotIntArray    = 10,		// Integer Array (yellow connector)
		iotStringArray = 11,		// String Array (yellow connector)
		iotPointArray  = 12,		// Point Array (yellow connector)

		iotBitmap      = 13,		// Bitmap (yellow connector)

		iotMono        = 20,		// Mono Stream
		iotStereo      = 21,		// Stereo Stream
		iotMono4       = 22,		// Mono4 Stream
		iotStereo4     = 23,		// Stereo4 Stream
	};

	struct IoInfo
	{
		Type type;					// READ/WRITE type of connector
		char name[32];				// READ/WRITE name of the connector
		char desc[64];				// READ/WRITE description of the connector
		bool autoTrigger;			// READ/WRITE automatically triggers when it changes (only for iotBoolean/int/float/string)
	};

	unsigned index;					// READ which connector is being requested
	IoInfo inputs[64];				// WRITE all inputs, fill what is used (depending on index and module size)
	IoInfo outputs[64];				// WRITE all outputs, fill what is used (depending on index and module size)
	char future[256];				// Don't touch, reserved for future additions
};

// arg for @fsrCreateInstance
struct FsSdkCreateInstance
{
	void* pInstance;				// WRITE your instance pointer
};

// arg for @fsrSaveInstanceSize
struct FsSdkSaveInstanceSize
{
	void* pInstance;				// READ your instance pointer
	unsigned size;					// WRITE size in bytes needed
};

// arg for @fsrSaveInstanceData
struct FsSdkSaveInstanceData
{
	void* pInstance;				// READ your instance pointer
	void* pData;					// READ/WRITE pointer to buffer of size returned from @fsrSaveInstanceSize
};

struct FsSdkTiming
{
	float sampleRate;
	float bpm;
	float timeSigNum;
	float timeSigDen;
	bool playing;
	bool offline;
	char future[256];				// Don't touch, reserved for future additions
};

// part of @fsrStream
struct FsSdkPlaybackPos
{
	float ppq;
	float barStartPos;
	char future[256];				// Don't touch, reserved for future additions
};

// arg for @fsrStream
struct FsSdkStream
{
	void* pInstance;				// READ/WRITE The instance data from earlier @fsrCreateInstance
	unsigned nrSamples;				// READ Nr. of samples in each buffer (is multiple of 4, as each stream has 4 SSE channels)
	float** pInputs;				// READ Array of buffers, each buffer contains @nrSamples of floats
									//	The nr. of buffers depends on how many input connectors are of stream type
	float** pOutputs;				// WRITE Array of buffers, each buffer contains @nrSamples of floats
									//	The nr. of buffers depends on how many output connectors are of stream type
	FsSdkTiming* pTiming;			// READ Information about the playback timing
	FsSdkPlaybackPos* pPlaybackPos;	// READ Information about the current playback position
	char future[256];				// Don't touch, reserved for future additions
};

struct FsSdkTrigger
{
	enum TriggerIndex
	{
		tiTimer      = 0,			// timer event
		tiSchematic  = 1,			// schematic event
		tiFirstInput = 2			// first input index
	};

	void* pOwner;					// READ
	void* pInstance;				// READ/WRITE
	unsigned index;					// READ trigger index (timer/schematic/input index)

	void* (*input)(void* pOwner, int idx);				// READ
	void  (*output)(void* pOwner, int idx, void* pData);	// READ
	FsSdkTiming* (*timing)(void* pOwner);				// READ
};

struct FsSdkPoint
{
	float x;
	float y;
};

struct FsSdkArea
{
	float x;
	float y;
	float w;
	float h;
};

struct FsSdkFloatArray
{
	unsigned len;
	float* pData;
};

struct FsSdkIntArray
{
	unsigned len;
	int* pData;
};

struct FsSdkStringArray
{
	unsigned len;
	char** pData;
};

struct FsSdkPointArray
{
	unsigned len;
	FsSdkPoint* pData;
};

typedef unsigned long FsSdkColor;

struct FsSdkBitmap
{
	unsigned width;
	unsigned height;
	unsigned stride;
	unsigned char* pData;			// ARGB
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Some macros to simplify some things
#include <string.h>
#define FS_SDK_STRING(dst, src)		strcpy_s(dst, sizeof(dst), src)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Some helper functions
inline void fsSdkDescribeInput(const FsSdkDescribeIO* p, unsigned i, FsSdkDescribeIO::Type type, const char* name = "", const char* desc = "", bool autoTrigger = false)
{
	p->inputs[i].type = type;
	FS_SDK_STRING(p->inputs[i].name, name);
	FS_SDK_STRING(p->inputs[i].desc, desc);
	p->inputs[i].autoTrigger = autoTrigger;
}

inline void fsSdkDescribeOutput(const FsSdkDescribeIO* p, unsigned i, FsSdkDescribeIO::Type type, const char* name = "", const char* desc = "")
{
	p->outputs[i].type = type;
	FS_SDK_STRING(p->outputs[i].name, name);
	FS_SDK_STRING(p->outputs[i].desc, desc);
}

inline void* fsSdkInput(const FsSdkTrigger* p, int idx) { return p->input(p->pOwner, idx); }
inline bool fsSdkBoolInput(const FsSdkTrigger* p, int idx) { const auto* r = fsSdkInput(p, idx); return r ? *(const bool*)r : false; }
inline float fsSdkFloatInput(const FsSdkTrigger* p, int idx) { const auto* r = fsSdkInput(p, idx); return r ? *(const float*)r : 0; }
inline int fsSdkIntInput(const FsSdkTrigger* p, int idx) { const auto* r = fsSdkInput(p, idx); return r ? *(const int*)r : 0; }
inline const char* fsSdkStringInput(const FsSdkTrigger* p, int idx) { const auto* r = fsSdkInput(p, idx); return (const char*)r; }
inline FsSdkPoint fsSdkPointInput(const FsSdkTrigger* p, int idx) { const auto* r = fsSdkInput(p, idx); return r ? *(const FsSdkPoint*)r : FsSdkPoint{0,0}; }
inline FsSdkColor fsSdkColorInput(const FsSdkTrigger* p, int idx) { const auto* r = fsSdkInput(p, idx); return r ? *(const FsSdkColor*)r : 0xff000000; }
inline FsSdkArea fsSdkAreaInput(const FsSdkTrigger* p, int idx) { const auto* r = fsSdkInput(p, idx); return r ? *(const FsSdkArea*)r : FsSdkArea{0,0,0,0}; }

inline void fsSdkOutput(const FsSdkTrigger* p, int idx, void* pData) { p->output(p->pOwner, idx, pData); }
inline void fsSdkTriggerOutput(const FsSdkTrigger* p, int idx) { fsSdkOutput(p, idx, NULL); }

#pragma pack(pop)

#endif // H_FlowStoneSdk_H