#ifndef HOST_DEFS_H
#define HOST_DEFS_H


#define QUEUE_ID_TO_FLOW_ID(Q) Q - 1
#define FLOW_ID_TO_Q_ID(F) F + 1
#define NVME_COMP_Q_MEMORY_REGION 40
#define DATA_MEMORY_REGION 0xFF0000000000

const uint64_t SUBMISSION_QUEUE_MEMORY_0 = 0x000000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_0 = 0x00F000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_1 = 0x010000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_1 = 0x01F000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_2 = 0x020000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_2 = 0x02F000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_3 = 0x030000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_3 = 0x03F000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_4 = 0x040000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_4 = 0x04F000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_5 = 0x050000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_5 = 0x05F000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_6 = 0x060000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_6 = 0x06F000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_7 = 0x070000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_7 = 0x07F000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_8 = 0x080000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_8 = 0x08F000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_9 = 0x090000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_9 = 0x09F000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_10 = 0x0A0000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_10 = 0x0AF000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_11 = 0x0B0000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_11 = 0x0BF000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_12 = 0x0C0000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_12 = 0x0CF000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_13 = 0x0D0000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_13 = 0x0DF000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_14 = 0x0E0000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_14 = 0x0EF000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_15 = 0x0F0000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_15 = 0x0FF000000000;
const uint64_t SUBMISSION_QUEUE_MEMORY_16 = 0x100000000000;
const uint64_t COMPLETION_QUEUE_MEMORY_16 = 0x10F000000000;

/* 
注意添加新的元素后需要在src/host/IO_Flow_Base.cpp文件内的IO_Flow_Base::IO_Flow_Base函数和
src/ssd/Host_Interface_NVMe.cpp的Request_Fetch_Unit_NVMe::Process_pcie_write_message函数中添加相对应的赋值和匹配项 
注意添加更多的负载可能会导致盘设备空间不足，从而遇到包括mapping中的意外错误 
*/

#endif //!HOST_DEFS_H