#ifndef NVME_DEFINITIONS_H
#define NVME_DEFINITIONS_H

#include <cstdint>
#include <string>
#include "Host_Interface_NVMe_Priorities.h"

enum class HostInterface_Types { SATA, NVME };

#define NVME_FLUSH_OPCODE 0x0000
#define NVME_WRITE_OPCODE 0x0001
#define NVME_READ_OPCODE 0x0002

#define SATA_WRITE_OPCODE 0x0001
#define SATA_READ_OPCODE 0x0002

const uint64_t NCQ_SUBMISSION_REGISTER = 0x1000;
const uint64_t NCQ_COMPLETION_REGISTER = 0x1003;
const uint64_t SUBMISSION_QUEUE_REGISTER_0 = 0x1000;
const uint64_t COMPLETION_QUEUE_REGISTER_0 = 0x1003;
const uint64_t SUBMISSION_QUEUE_REGISTER_1 = 0x1010;
const uint64_t COMPLETION_QUEUE_REGISTER_1 = 0x1013;
const uint64_t SUBMISSION_QUEUE_REGISTER_2 = 0x1020;
const uint64_t COMPLETION_QUEUE_REGISTER_2 = 0x1023;
const uint64_t SUBMISSION_QUEUE_REGISTER_3 = 0x1030;
const uint64_t COMPLETION_QUEUE_REGISTER_3 = 0x1033;
const uint64_t SUBMISSION_QUEUE_REGISTER_4 = 0x1040;
const uint64_t COMPLETION_QUEUE_REGISTER_4 = 0x1043;
const uint64_t SUBMISSION_QUEUE_REGISTER_5 = 0x1050;
const uint64_t COMPLETION_QUEUE_REGISTER_5 = 0x1053;
const uint64_t SUBMISSION_QUEUE_REGISTER_6 = 0x1060;
const uint64_t COMPLETION_QUEUE_REGISTER_6 = 0x1063;
const uint64_t SUBMISSION_QUEUE_REGISTER_7 = 0x1070;
const uint64_t COMPLETION_QUEUE_REGISTER_7 = 0x1073;
const uint64_t SUBMISSION_QUEUE_REGISTER_8 = 0x1080;
const uint64_t COMPLETION_QUEUE_REGISTER_8 = 0x1083;
const uint64_t SUBMISSION_QUEUE_REGISTER_9 = 0x1090;
const uint64_t COMPLETION_QUEUE_REGISTER_9 = 0x1093;
const uint64_t SUBMISSION_QUEUE_REGISTER_10 = 0x10A0;
const uint64_t COMPLETION_QUEUE_REGISTER_10 = 0x10A3;
const uint64_t SUBMISSION_QUEUE_REGISTER_11 = 0x10B0;
const uint64_t COMPLETION_QUEUE_REGISTER_11 = 0x10B3;
const uint64_t SUBMISSION_QUEUE_REGISTER_12 = 0x10C0;
const uint64_t COMPLETION_QUEUE_REGISTER_12 = 0x10C3;
const uint64_t SUBMISSION_QUEUE_REGISTER_13 = 0x10D0;
const uint64_t COMPLETION_QUEUE_REGISTER_13 = 0x10D3;
const uint64_t SUBMISSION_QUEUE_REGISTER_14 = 0x10E0;
const uint64_t COMPLETION_QUEUE_REGISTER_14 = 0x10E3;
const uint64_t SUBMISSION_QUEUE_REGISTER_15 = 0x10F0;
const uint64_t COMPLETION_QUEUE_REGISTER_15 = 0x10F3;
const uint64_t SUBMISSION_QUEUE_REGISTER_16 = 0x1100;
const uint64_t COMPLETION_QUEUE_REGISTER_16 = 0x1103;

/* 
注意添加新的元素后需要在src/host/IO_Flow_Base.cpp文件内的IO_Flow_Base::IO_Flow_Base函数和
src/ssd/Host_Interface_NVMe.cpp的Request_Fetch_Unit_NVMe::Process_pcie_write_message函数中添加相对应的赋值和匹配项 
注意添加更多的负载可能会导致盘设备空间不足，从而遇到包括mapping中的意外错误 
*/


struct Completion_Queue_Entry
{
	uint32_t Command_specific;
	uint32_t Reserved;
	uint16_t SQ_Head; //SQ Head Pointer, Indicates the current Submission Queue Head pointer for the Submission Queue indicated in the SQ Identifier field
	uint16_t SQ_ID;//SQ Identifier, Indicates the Submission Queue to which the associated command was issued to.
	uint16_t Command_Identifier;//Command Identifier, Indicates the identifier of the command that is being completed
	uint16_t SF_P; //Status Field (SF)+ Phase Tag(P)  /* 一开始CQ中每条命令完成条目中的”P” bit初始化为0，SSD在往CQ中写入命令完成条目时，会把”P”写成1。 */
				   //SF: Indicates status for the command that is being completed
				   //P: Identifies whether a Completion Queue entry is new
};

struct Submission_Queue_Entry
{
	uint8_t Opcode;//Is it a read or write request
	uint8_t PRP_FUSE;
	uint16_t Command_Identifier;//The id of the command in the I/O submission queue
	uint64_t Namespace_identifier;
	uint64_t Reserved;  /* 目前被用来拿去标记用户请求的发射时间了 在src/host/IO_Flow_Base.cpp内的NVMe_read_sqe函数中初始化 */
	uint64_t Metadata_pointer_1;
	uint64_t PRP_entry_1;
	uint64_t PRP_entry_2;
	uint32_t Command_specific[6];
};

#endif // !NVME_DEFINISIONS_H
