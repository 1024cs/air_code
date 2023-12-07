#ifndef HOST_IO_REQUEST_H
#define HOST_IO_REQUEST_H

#include "../ssd/SSD_Defs.h"

namespace Host_Components
{
	enum class Host_IO_Request_Type { READ, WRITE };
	class Host_IO_Request
	{
	public:
		/* 在src/host/IO_Flow_Trace_Based.cpp的Execute_simulator_event中先初始化Arrival_time 再初始化Enqueue_time */
		sim_time_type Arrival_time;//The time that the request has been generated  /* src/host/IO_Flow_Trace_Based.cpp的Generate_next_request中被赋值 */
		sim_time_type Enqueue_time;//The time that the request enqueued into the I/O queue  /* src/host/IO_Flow_Base.cpp的Submit_io_request中被赋值 (其实在NVMe_consume_io_request也有赋值 只不过基本上不会被调用 暂时不会) */
		LHA_type Start_LBA;      /* ASCIITraceAddressColumn / 512 */
		unsigned int LBA_count;  /* ASCIITraceSizeColumn / 512 */
		Host_IO_Request_Type Type;
		uint16_t IO_queue_info;
		uint16_t Source_flow_id;//Only used in SATA host interface
	};
}

#endif // !HOST_IO_REQUEST_H
