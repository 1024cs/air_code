#ifndef PCIE_MESSAGE_H
#define PCIE_MESSAGE_H

#include<cstdint>

namespace Host_Components
{
	enum class PCIe_Destination_Type {HOST, DEVICE};
	enum class PCIe_Message_Type {READ_REQ, WRITE_REQ, READ_COMP};
	
	class PCIe_Message
	{
	public:
		PCIe_Destination_Type Destination;
		PCIe_Message_Type Type;
		void* Payload;    /* 专门存放提交实体和完成实体 */
		unsigned int Payload_size;
		uint64_t Address;  /* 可以认为就是不同用户的一个标识，对应于不同的提交队列或者完成队列 但是值大小不为0 1 2这些 具体在src/ssd/Host_Interface_Defs.h */
	};
}

#endif //!PCIE_MESSAGE_H
