#ifndef PLANE_H
#define PLANE_H

#include "../NVM_Types.h"
#include "FlashTypes.h"
#include "Block.h"
#include "Flash_Command.h"

namespace NVM
{
	namespace FlashMemory
	{
		class Plane
		{
		public:
			Plane(unsigned int BlocksNoPerPlane, unsigned int PagesNoPerBlock);
			~Plane();
			Block** Blocks;  /* 第二个'*'代表的是指针 会赋值一个new对象的地址 */
			unsigned int Healthy_block_no;
			unsigned long Read_count;                     //how many read count in the process of workload
			unsigned long Progam_count;
			unsigned long Erase_count;
			stream_id_type* Allocated_streams;
		};
	}
}
#endif // !PLANE_H
