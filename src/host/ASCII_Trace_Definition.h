#ifndef ASCII_TRACE_DEFINITION_H
#define ASCII_TRACE_DEFINITION_H

enum class Trace_Time_Unit { PICOSECOND, NANOSECOND, MICROSECOND, MILLISECOND, SECOND};//The unit of arrival times in the input file
#define PicoSecondCoeff  1000000000000	//the coefficient to convert picoseconds to second
#define NanoSecondCoeff  1000000000	//the coefficient to convert nanoseconds to second
#define MicroSecondCoeff  1000000	//the coefficient to convert microseconds to second
#define MilliSecondCoeff  1000	//the coefficient to convert milliseconds to second
#define SecondCoeff  1	//the coefficient to convert seconds to second

#define ASCIITraceTimeColumn 4
#define ASCIITraceDeviceColumn 0
#define ASCIITraceAddressColumn 2
#define ASCIITraceSizeColumn 3
#define ASCIITraceTypeColumn 1
#define ASCIITraceWriteCode "W"
#define ASCIITraceReadCode "R"
#define ASCIITraceWriteCodeInteger 0
#define ASCIITraceReadCodeInteger 1
#define ASCIILineDelimiter ','
#define ASCIIItemsPerLine 5

#define ASCIITraceTimeColumn_TenCloud 0
#define ASCIITraceDeviceColumn_TenCloud 4
#define ASCIITraceAddressColumn_TenCloud 1
#define ASCIITraceSizeColumn_TenCloud 2
#define ASCIITraceTypeColumn_TenCloud 3
#define ASCIITraceWriteCode_TenCloud "1"
#define ASCIITraceReadCode_TenCloud "0"
#define ASCIITraceWriteCodeInteger_TenCloud 0
#define ASCIITraceReadCodeInteger_TenCloud 1
#define ASCIILineDelimiter_TenCloud ','
#define ASCIIItemsPerLine_TenCloud 5

#define ASCIITraceTimeColumn_MSR 0
#define ASCIITraceDeviceColumn_MSR 1
#define ASCIITraceAddressColumn_MSR 4
#define ASCIITraceSizeColumn_MSR 5
#define ASCIITraceTypeColumn_MSR 3
#define ASCIITraceWriteCode_MSR "Write"
#define ASCIITraceReadCode_MSR "Read"
#define ASCIITraceWriteCodeInteger_MSR 0
#define ASCIITraceReadCodeInteger_MSR 1
#define ASCIILineDelimiter_MSR ','
#define ASCIIItemsPerLine_MSR 7

#define PicoSec2SimTimeCoeff 1000
#define NanoSec2SimTimeCoeff 1
#define MicroSec2SimTimeCoeff 1000
#define MilliSec2SimTimeCoeff 1000000
#define Sec2SimTimeCoeff 1000000000

#endif // !ASCII_TRACE_DEFINITION_H
