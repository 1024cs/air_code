#ifndef USER_REQUEST_H
#define USER_REQUEST_H

#include <string>
#include <list>
#include "SSD_Defs.h"
#include "../sim/Sim_Defs.h"
#include "Host_Interface_Defs.h"
#include "NVM_Transaction.h"

namespace SSD_Components
{
	enum class UserRequestType { READ, WRITE };
	class NVM_Transaction;
	class User_Request
	{
	public:
		User_Request();
		IO_Flow_Priority_Class::Priority Priority_class;
		io_request_id_type ID;
		LHA_type Start_LBA;   /* 按照sector的大小计数 */

		sim_time_type STAT_InitiationTime;  /* 该时间与对应请求的发射时间有区别 */
		sim_time_type STAT_ResponseTime;
		std::list<NVM_Transaction*> Transaction_list;
		unsigned int Sectors_serviced_from_cache;

		unsigned int Size_in_byte;
		unsigned int SizeInSectors;  /* 按照sector的大小计数 */
		UserRequestType Type;
		stream_id_type Stream_id;
		bool ToBeIgnored;
		void* IO_command_info;//used to store host I/O command info
		void* Data;

		/* zy1024cs */
		long int cache_time = -1;
		// bool bypass = false;
		int fuzzy_priority = -1; /* 0 ~ (stream_count-1) */
		sim_time_type Arrival_time;
		double band_rar[4] = {-1, -1, -1, -1};
		bool ifcache = false;


	private:
		static unsigned int lastId;
	};
}

#endif // !USER_REQUEST_H
