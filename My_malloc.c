#include  <My_malloc.h>
 
#define ALIGN_SIZE(X,Align_Byte) ((X + Align_Byte - 1) & ~(Align_Byte - 1))
 
static  const unsigned int BLOCK_Min_SIZE = BIT_SIZE_CONFIG < sizeof(List_Node) ? sizeof(List_Node) : BIT_SIZE_CONFIG;
 
#if defined(__GNUC__) || defined(__clang__)
 
static inline int Get_highest_bit_position(Base_Type x)
{
	static	const int CLZ_MAX = CLZ(0x01);
	return x ? (CLZ_MAX - CLZ(x)) : -1;
}
 
static int Find_Block(Memory_Head* Head, Base_Type Index_len, Base_Type x)
{
	TPED_BIT Temp_bit = 0;
	const Base_Type Bit_mask = (Base_Type)0x01 << Head->Map_unit_size;
	const Base_Type Bit_position = Head->Map_unit_size;
 
  if ((x >> Bit_position) >= Index_len) return -1;
	if (Head->Map_bit[x >> Bit_position] < ((Base_Type)0x01 << (x & (Bit_mask - 1))))
	{
		Base_Type Index_Bit = Head->Index_bit & (~((((Base_Type)0x01 << (x >> Bit_position)) << 1) - 1));
		if (Index_Bit) x = (FFS(Index_Bit) - 1) << Bit_position; else return -1;
	}
	Temp_bit = Head->Map_bit[x >> Bit_position] & (~(((Base_Type)0x01 << (x & (Bit_mask - 1))) - 1));
	return (x & ~(Bit_mask - 1)) | (FFS(Temp_bit) - 1);
}
#else
 
static int Get_highest_bit_position(Base_Type x)
{//此函数是软件实现的二分查找算法，用于找到一个无符号整数x中最高位1所在的位置。如果硬件支持该功能最好，可以直接使用内置函数来实现这个功能，例如GCC中的__builtin_clz函数。
	int high_bit = sizeof(x) << 3, low_bit = 0;
	int position = (high_bit + low_bit) >> 1;
	if (x == 0)	return -1;
	while ((x >> position) ^ (Base_Type)0x01)
	{
		if (x >> position)	low_bit = position; else high_bit = position;
		position = (high_bit + low_bit) >> 1;
	}
	return position;
}
 
static int Find_Block(Memory_Head* Head, Base_Type Index_len, Base_Type x)
{//此函数用于在内存管理器的位图中查找第一个不小于x的块。如果能用硬件指令实现最好，可以直接使用内置函数来实现这个功能，例如GCC中的__builtin_ffs函数。
	TPED_BIT Temp_bit = 0;
	Base_Type Bit_mask = (Base_Type)0x01  << Head->Map_unit_size;
	Base_Type Bit_position = Head->Map_unit_size;
 
  if ((x >> Bit_position) >= Index_len) return -1;
	if (Head->Map_bit[x >> Bit_position] < ((Base_Type)0x01  << (x & (Bit_mask - 1))))
	{
		x += Bit_mask - (x & (Bit_mask - 1));
		while ((x >> Bit_position) < Index_len && !Head->Map_bit[x >> Bit_position]) x += Bit_mask;
	}
	if((x >> Bit_position) >= Index_len) return -1;
	Temp_bit = Head->Map_bit[x >> Bit_position];
	for (Base_Type i = (x & (Bit_mask - 1)); i < Bit_mask; i++, x++)
	{
		if (Temp_bit & ((Base_Type)0x01 << i)) return x;
	}
	return -1;
}
#endif
 
//将数值分解成不小于16的块，块的大小为2的幂次方，并返回块在数组中的位置
static inline Base_Type Change_Index(Base_Type size, int Position_bit, Base_Type alignment)
{
	return (Position_bit << alignment) | ((((Base_Type)0x01 << alignment) - 1) & (size >> (Position_bit - alignment)));
}
//将数值分解成不小于16的块，块的大小为2的幂次方，并返回第一个不小于x的块在数组中的位置
static inline Base_Type Align_Index(Base_Type size, Base_Type alignment)
{
	int Position_bit = Get_highest_bit_position(size);
	return (Change_Index(size, Position_bit, alignment) + ((((Base_Type)0x01 << (Position_bit - alignment)) - 1) & size ? 1 : 0));
}
 
// Add_Node函数用于将一个内存块节点添加到内存管理器的自由列表中。它首先计算出块大小对应的自由列表索引，然后将节点插入到该索引的链表头部，并更新相应的位图以标记该索引有可用块。最后，它将节点的使用标志设置为0，表示该块是空闲的。
static void Add_Node(Memory_Head* Head, List_Node* Node)
{
	if (Node == NULL || Head == NULL) return;
	Base_Type Index = Change_Index(Node->Base.Block_Size, Get_highest_bit_position(Node->Base.Block_Size), Head->Map_unit_size);
	List_Node** Current_Head = &Head->Free_List_Head[Index >> (Head->Map_unit_size)][Index & (((Base_Type)0x01 << (Head->Map_unit_size)) - 1)];
 
	Node->Base.Used_flag = 0;
	Node->Prev = NULL;
	Node->Next = *Current_Head;
	if (*Current_Head) (*Current_Head)->Prev = Node;
 
	Head->Index_bit |= ((Base_Type)0x01 << (Index >> (Head->Map_unit_size)));
	Head->Map_bit[Index >> (Head->Map_unit_size)] |= ((Base_Type)0x01 << (Index & (((Base_Type)0x01 << (Head->Map_unit_size)) - 1)));
	*Current_Head = Node;
	Head->Available_Size += Node->Base.Block_Size;
}
 
// Take_Out_Node函数用于从内存管理器的自由列表中取出一个满足条件的内存块节点。它首先计算出块大小对应的自由列表索引，然后从该索引的链表头部取出一个节点，并更新相应的位图以标记该索引是否还有可用块。最后，它将节点的使用标志设置为1，表示该块已被占用，并返回该节点。
static List_Node* Take_Out_Node(Memory_Head* Head, Base_Type Index)
{
	if (Head == NULL) return NULL;
	List_Node** Current_Head = &Head->Free_List_Head[Index >> (Head->Map_unit_size)][Index & (((Base_Type)0x01 << (Head->Map_unit_size)) - 1)];
	List_Node* Node = *Current_Head;
 
	*Current_Head = Node->Next;
	if (*Current_Head) (*Current_Head)->Prev = NULL; else Head->Map_bit[Index >> (Head->Map_unit_size)] &= ~((Base_Type)0x01 << (Index & (((Base_Type)0x01 << (Head->Map_unit_size)) - 1)));
	if (!Head->Map_bit[Index >> (Head->Map_unit_size)]) Head->Index_bit &= ~((Base_Type)0x01 << (Index >> (Head->Map_unit_size)));
	Node->Base.Used_flag = 1;
	Head->Available_Size -= Node->Base.Block_Size;
 
	return Node;
}
 
// Delete_Node函数用于将一个内存块节点从内存管理器的自由列表中删除。它首先获取节点的前一个节点和下一个节点，然后根据节点在链表中的位置更新前一个节点和下一个节点的指针。最后，它将节点的使用标志设置为1，表示该块已被占用，并更新相应的位图以标记该索引是否还有可用块。
static void Delete_Node(Memory_Head* Head, List_Node* Node)
{
	if (Head == NULL || Node == NULL)	return;
	Base_Type Index = Change_Index(Node->Base.Block_Size, Get_highest_bit_position(Node->Base.Block_Size), Head->Map_unit_size);
	List_Node* Prev_Node = Node->Prev, * Next_Node = Node->Next;
	List_Node** Current_Head = &Head->Free_List_Head[Index >> (Head->Map_unit_size)][Index & (((Base_Type)0x01 << (Head->Map_unit_size)) - 1)];
	if (*Current_Head == Node) *Current_Head = Node->Next;
	else Prev_Node->Next = Node->Next;
 
	if (Next_Node) Next_Node->Prev = Node->Prev;
	if (*Current_Head == NULL) Head->Map_bit[Index >> (Head->Map_unit_size)] &= ~((Base_Type)0x01 << (Index & (((Base_Type)0x01 << (Head->Map_unit_size)) - 1)));
	if (!Head->Map_bit[Index >> (Head->Map_unit_size)]) Head->Index_bit &= ~((Base_Type)0x01 << (Index >> (Head->Map_unit_size)));
	Node->Base.Used_flag = 1;
 
	Head->Available_Size -= Node->Base.Block_Size;
}
 
// Partition_Node函数用于将一个内存块节点分割成一个满足请求大小的块和一个剩余的空闲块。它首先检查节点的块大小是否足够大以容纳请求的大小加上最小块大小，如果是，则创建一个新的节点来表示剩余的空闲块，并将其添加到自由列表中。最后，它将原始节点的使用标志设置为1，表示该块已被占用，并返回该节点。
static void* Partition_Node(Memory_Head* Head, void* addr, Base_Type size)
{
	List_Node* Node = (List_Node*)addr;
	if (Node->Base.Block_Size >= (size + BLOCK_Min_SIZE))
	{
		List_Node* Divide_Node = (List_Node*)((BYTE*)addr + size);
		List_Node* Next_Node = (List_Node*)((BYTE*)addr + Node->Base.Block_Size);
		Divide_Node->Base.Block_Size = Node->Base.Block_Size - size, Divide_Node->Base.Pre_addr = addr;
		Next_Node->Base.Pre_addr = Divide_Node;
		Node->Base.Block_Size = size;
		Add_Node(Head, Divide_Node);
	}
	return Node;
}
 
// Merge_Node函数用于将一个内存块节点与其相邻的空闲块合并成一个更大的块。它首先获取节点的前一个节点和下一个节点，然后检查它们是否为空闲块。如果前一个节点是空闲块，则将其从自由列表中删除，并将其块大小加到当前节点的块大小上，同时更新下一个节点的前一个地址指针。如果下一个节点是空闲块，则将其从自由列表中删除，并将其块大小加到当前节点的块大小上，同时更新下一个节点的前一个地址指针。最后，它将合并后的块添加回自由列表中。
static void Merge_Node(Memory_Head* Head, void* addr)
{
	List_Node* Node = (List_Node*)addr;
	List_Node* Pre_Node = (List_Node*)Node->Base.Pre_addr;
	List_Node* Next_Node = (List_Node*)((BYTE*)Node + Node->Base.Block_Size);
 
	if (Pre_Node && !Pre_Node->Base.Used_flag)
	{
		Delete_Node(Head, Pre_Node);
		Pre_Node->Base.Block_Size += Node->Base.Block_Size;
		Next_Node->Base.Pre_addr = Pre_Node;
		Node = Pre_Node;
	}
 
	if (!Next_Node->Base.Used_flag)
	{
		Delete_Node(Head, Next_Node);
		Node->Base.Block_Size += Next_Node->Base.Block_Size;
		Next_Node = (List_Node*)((BYTE*)Node + Node->Base.Block_Size);
		Next_Node->Base.Pre_addr = Node;
	}
	Add_Node(Head, Node);
}
 
// Init_TLSF函数用于初始化内存管理器。它首先检查输入的内存管理器头指针、内存地址和内存大小是否有效，如果无效则直接返回。然后，它将内存大小对齐到最近的对齐大小的倍数，并设置内存管理器头中的总大小、可用大小和位图单位大小等信息。接下来，它将整个内存区域划分为一个大的空闲块，并将其添加到自由列表中。最后，它在内存区域的末尾设置一个占用块，以标记内存区域的结束。
extern Base_Type Init_TLSF(Memory_Head* Head, void* Memory_addr, Base_Type Memory_size, Base_Type Align_Byte, void (*Lock)(void), void (*Unlock)(void))
{
  Memory_size &= ~(Align_Byte - 1);
  if((Base_Type)Memory_addr & (Align_Byte - 1)) return 0;
	if (Head == NULL || Memory_addr == NULL || Align_Byte == 0 || (Align_Byte & (Align_Byte - 1))) return 0;
	if (Memory_size < (BLOCK_Min_SIZE << 2)|| BLOCK_Min_SIZE < sizeof(List_Node) || (BLOCK_Min_SIZE & (Align_Byte - 1))) return 0;
 
	Head->Total_Size = Memory_size;
	Head->Available_Size = 0;
	Head->Align_Byte = Align_Byte;
	Head->Map_unit_size = Get_highest_bit_position(sizeof(Head->Map_bit[0]) << 3);
	if (Get_highest_bit_position(BLOCK_Min_SIZE) < Head->Map_unit_size) return 0;
 
	Head->Index_bit = 0;
	for (Base_Type i = 0; i < FREE_LIST_SIZE; i++)
	{
		for (Base_Type j = 0; j < (sizeof(Head->Map_bit[0]) << 3); j++)
		{
			Head->Free_List_Head[i][j] = NULL;
		}
		Head->Map_bit[i] = 0;
	}
 
	List_Node* Node = (List_Node*)Memory_addr;
	Node->Base.Pre_addr = NULL, Node->Base.Used_flag = 1, Node->Base.Block_Size = Memory_size - BLOCK_Min_SIZE;
	Add_Node(Head, Node);
 
	Node = (List_Node*)((BYTE*)Memory_addr + (Memory_size - BLOCK_Min_SIZE));
	Node->Base.Pre_addr = Memory_addr, Node->Base.Used_flag = 1, Node->Base.Block_Size = BLOCK_Min_SIZE;
	Head->End_Addr_Node = Node;
 
	Head->Lock = Lock;
	Head->Unlock = Unlock;
 
	return Memory_size;
}
 
// TLSF_malloc函数用于在内存管理器中分配一块指定大小的内存。它首先检查输入的大小是否为零或大于可用大小，如果是，则返回NULL，表示无法进行分配。否则，它将请求的大小对齐到最近的对齐大小的倍数，并加上基本节点结构的大小，以考虑每个分配块将存储的元数据。然后，它确保总大小至少等于BLOCK_Min_SIZE定义的最小块大小。
extern void* TLSF_malloc(Memory_Head* Head, Base_Type size)
{
	if (Head == NULL || size == 0 || size > Head->Available_Size) return NULL;
	size += ALIGN_SIZE(sizeof(Base_Node), Head->Align_Byte);
	size = (size < BLOCK_Min_SIZE) ? ALIGN_SIZE(BLOCK_Min_SIZE, Head->Align_Byte) : ALIGN_SIZE(size, Head->Align_Byte);
    if (size > ((Base_Type)0x01 << ((sizeof(Base_Type) << 3) - 1))) return NULL;
	Base_Type Index = Align_Index(size, Head->Map_unit_size);
	int Node_index = Find_Block(Head, FREE_LIST_SIZE, Index);
	if (Node_index == -1) return NULL;
	List_Node* Node = Take_Out_Node(Head, Node_index);
	return (void*)((BYTE*)Partition_Node(Head, Node, size) + ALIGN_SIZE(sizeof(Base_Node), Head->Align_Byte));
}
 
// TLSF_free函数用于释放之前分配的内存块。它首先检查输入的内存管理器头指针和要释放的地址是否为NULL，如果是，则直接返回。否则，它调用Merge_Node函数将要释放的内存块与相邻的空闲块合并，并将合并后的块添加回自由列表中。
extern void TLSF_free(Memory_Head* Head, void* addr)
{
	if (Head == NULL || addr == NULL) return;
	Merge_Node(Head, (BYTE*)addr - ALIGN_SIZE(sizeof(Base_Node), Head->Align_Byte));
}
 
// TLSF_realloc函数用于重新分配之前分配的内存块。它首先检查输入的内存管理器头指针和要重新分配的地址是否为NULL，如果是，则直接调用TLSF_malloc函数进行分配。然后，它检查请求的大小是否为零，如果是，则调用TLSF_free函数释放内存并返回NULL。否则，它获取当前内存块的大小，并将请求的大小对齐到最近的对齐大小的倍数，并加上基本节点结构的大小。接下来，它比较当前块的大小和请求的大小，如果当前块足够大，则调用Partition_Node函数进行分割。如果当前块不够大，则检查下一个相邻块是否为空闲且足够大，如果是，则将其合并到当前块中，并根据需要进行分割。如果下一个块也不满足条件，则调用TLSF_malloc函数分配一个新的块，并将原始数据复制到新块中，然后释放原始块。最后，返回指向重新分配内存的新地址。
extern void* TLSF_realloc(Memory_Head* Head, void* addr, Base_Type size)
{
	if (Head == NULL) return NULL;
	if (addr == NULL) return TLSF_malloc(Head, size);
	if (size == 0) { TLSF_free(Head, addr); return NULL; }
 
	List_Node* Node = (List_Node*)((BYTE*)addr - ALIGN_SIZE(sizeof(Base_Node), Head->Align_Byte));
	Base_Type Old_size = Node->Base.Block_Size;
	List_Node* Next_Node = (List_Node*)((BYTE*)Node + Old_size);

    if (size > ((Base_Type)0x01 << ((sizeof(Base_Type) << 3) - 1))) return NULL;
	size = ALIGN_SIZE(size, Head->Align_Byte) + ALIGN_SIZE(sizeof(Base_Node), Head->Align_Byte);
	size = (size < BLOCK_Min_SIZE) ? ALIGN_SIZE(BLOCK_Min_SIZE, Head->Align_Byte) : ALIGN_SIZE(size, Head->Align_Byte);

	if (Old_size >= size)
	{//如果当前块足够大，则调用Partition_Node函数进行分割。
		if (Old_size >= (size + BLOCK_Min_SIZE))
		{//如果当前块足够大以容纳请求的大小加上最小块大小，则调用Partition_Node函数进行分割。
			List_Node* Free_Node = (List_Node*)((BYTE*)Node + size);
			Node->Base.Block_Size = size;
			Free_Node->Base.Block_Size = Old_size - size, Free_Node->Base.Pre_addr = Node;
			if (Next_Node->Base.Used_flag == 0)
			{//如果下一个块是空闲的，则将其从自由列表中删除，并将其块大小加到当前块的块大小上，同时更新下一个节点的前一个地址指针。
				List_Node* Next_Next_Node = (List_Node*)((BYTE*)Next_Node + Next_Node->Base.Block_Size);
				Delete_Node(Head, Next_Node);
				Free_Node->Base.Block_Size += Next_Node->Base.Block_Size;
				Next_Next_Node->Base.Pre_addr = Free_Node;
			}
			else//如果下一个块不是空闲的，则直接将分割后的空闲块添加到自由列表中。
				Next_Node->Base.Pre_addr = Free_Node;
			Add_Node(Head, Free_Node);
		}
	}
	else
	{//如果当前块不够大，则检查下一个相邻块是否为空闲且足够大，如果是，则将其合并到当前块中，并根据需要进行分割。如果下一个块也不满足条件，则调用TLSF_malloc函数分配一个新的块，并将原始数据复制到新块中，然后释放原始块。最后，返回指向重新分配内存的新地址。
		if (Next_Node->Base.Used_flag == 0 && (Next_Node->Base.Block_Size + Old_size) >= size)
		{//如果下一个块是空闲的且足够大，则将其合并到当前块中，并根据需要进行分割。
			List_Node* Next_Next_Node = (List_Node*)((BYTE*)Next_Node + Next_Node->Base.Block_Size);
			Delete_Node(Head, Next_Node);
			if (Next_Node->Base.Block_Size + Old_size >= (size + BLOCK_Min_SIZE))
			{//如果合并后的块足够大，则调用Partition_Node函数进行分割。
				List_Node* Divide_Node = (List_Node*)((BYTE*)Node + size);
				Divide_Node->Base.Block_Size = Next_Node->Base.Block_Size - (size - Old_size), Divide_Node->Base.Pre_addr = Node;
				Next_Next_Node->Base.Pre_addr = Divide_Node;
				Node->Base.Block_Size = size;
				Add_Node(Head, Divide_Node);
			}
			else
			{//如果合并后的块不满足分割条件，则直接将其合并到当前块中。
				Node->Base.Block_Size += Next_Node->Base.Block_Size;
				Next_Next_Node->Base.Pre_addr = Node;
			}
		}
		else
		{//如果下一个块也不满足条件，则调用TLSF_malloc函数分配一个新的块，并将原始数据复制到新块中，然后释放原始块。最后，返回指向重新分配内存的新地址。
			void* New_Node = TLSF_malloc(Head, size - ALIGN_SIZE(sizeof(Base_Node), Head->Align_Byte));
			if (New_Node == NULL) return NULL;
			Memcpy_s(New_Node, addr, Old_size - ALIGN_SIZE(sizeof(Base_Node), Head->Align_Byte));
			TLSF_free(Head, addr);
			addr = New_Node;
		}
	}
	return addr;
}
 
// TLSF_calloc函数用于分配一块指定大小的内存，并将其初始化为零。它首先计算出总的内存大小，即请求的元素数量乘以每个元素的大小。然后，它调用TLSF_malloc函数进行分配，如果分配成功，则调用Memory_Reset函数将分配的内存块初始化为零。最后，返回指向分配内存的指针。
extern void* TLSF_calloc(Memory_Head* Head, Base_Type num, Base_Type size)
{
	if (Head == NULL || num == 0 || size == 0) return NULL;
	if (num > (0x0UL - 1) / size) return NULL; // 在乘法前检查溢出
	Base_Type total_size = num * size;
	void* ptr = TLSF_malloc(Head, total_size);
	if (ptr) Memory_Reset(ptr, total_size);
	return ptr;
}
 
// TLSF_malloc_Safe、TLSF_free_Safe、TLSF_realloc_Safe和TLSF_calloc_Safe函数是线程安全版本的内存分配函数。它们在调用相应的内存分配函数之前，首先检查内存管理器头中的Lock函数指针是否存在，如果存在，则调用Lock函数来获取锁，以确保在多线程环境下对内存管理器的访问是互斥的。然后，它调用相应的内存分配函数进行操作，并在操作完成后检查Unlock函数指针是否存在，如果存在，则调用Unlock函数来释放锁。最后，返回相应的结果。
extern void* TLSF_malloc_Safe(Memory_Head* Head, Base_Type size)
{
	if(Head == NULL || Head->Lock == NULL || Head->Unlock == NULL) return NULL;
	Head->Lock();
	void* ptr = TLSF_malloc(Head, size);
	Head->Unlock();
	return ptr;
}
 
extern void TLSF_free_Safe(Memory_Head* Head, void* addr)
{
	if (Head == NULL || addr == NULL || Head->Lock == NULL || Head->Unlock == NULL) return;
	Head->Lock();
	TLSF_free(Head, addr);
	Head->Unlock();
}
 
extern void* TLSF_realloc_Safe(Memory_Head* Head, void* addr, Base_Type size)
{
	if (Head == NULL || Head->Lock == NULL || Head->Unlock == NULL) return NULL;
	Head->Lock();
	void* ptr = TLSF_realloc(Head, addr, size);
	Head->Unlock();
	return ptr;
}
 
extern void* TLSF_calloc_Safe(Memory_Head* Head, Base_Type num, Base_Type size)
{
	if (Head == NULL || num == 0 || size == 0) return NULL;
	if (Head->Lock == NULL || Head->Unlock == NULL) return NULL;
	Head->Lock();
	void* ptr = TLSF_calloc(Head, num, size);
	Head->Unlock();
	return ptr;
}
 
//初始化内存块
extern void Memory_Reset(void* addr, Base_Type Length)
{
	BYTE* addr_byte = (BYTE*)addr;
	long long* addr_int = NULL;
	Base_Type Len = 0;
	if (addr == NULL || Length == 0) return;
	// 地址处理非对齐部分
	for (; Length && ((Base_Type)addr_byte & (sizeof(long long) - 1)); Length--)
	{
		*addr_byte++ = 0;
	}
	addr_int = (long long*)addr_byte;
	Len = Length / sizeof(long long);
	for (; Len; Len--)
	{
		*addr_int++ = 0;
	}
	//剩余部分处理
	Len = Length % sizeof(long long);
	addr_byte = (BYTE*)addr_int;
	for (; Len; Len--)
	{
		*addr_byte++ = 0;
	}
}
 
//内存拷贝
extern void* Memcpy_s(void* destinction, void* orignal, Base_Type Length)
{
	BYTE* dest = (BYTE*)destinction;
	BYTE* orign = (BYTE*)orignal;
	Base_Type Len = 0;
	if (!destinction || !orignal || Length == 0)  return NULL;

	if ((dest > orign) && ((orign + Length) > dest))
	{// 目的地址更高且区域重叠:从尾部向头部拷贝
		Len = Length % sizeof(long long);
		dest += Length;
		orign += Length;
		for (; Len; Len--) *(--dest) = *(--orign);       // 尾部余数字节
		Len = Length / sizeof(long long);
		if ((((UINT_64)dest | (UINT_64)orign) & (sizeof(long long) - 1)) == 0)
		{// 快路径:与原始实现完全相同
			long long* dest_int = (long long*)dest;
			long long* orign_int = (long long*)orign;
			for (; Len; Len--) *(--dest_int) = *(--orign_int);
		}
		else
		{// 未对齐路径:memcpy 内建按 8 字节搬移
			for (; Len; Len--)
			{
				dest -= sizeof(long long); orign -= sizeof(long long);
				__builtin_memcpy(dest, orign, sizeof(long long));
			}
		}
	}
	else
	{
		Len = Length % sizeof(long long);
		for (; Len; Len--) *dest++ = *orign++;           // 头部余数字节
		Len = Length / sizeof(long long);
		if ((((UINT_64)dest | (UINT_64)orign) & (sizeof(long long) - 1)) == 0)
		{
			long long* dest_int = (long long*)dest;
			long long* orign_int = (long long*)orign;
			for (; Len; Len--) *dest_int++ = *orign_int++;
		}
		else
		{
			for (; Len; Len--)
			{
				__builtin_memcpy(dest, orign, sizeof(long long));
				dest += sizeof(long long); orign += sizeof(long long);
			}
		}
	}
	return destinction;
}
//原地扩展内存的大小
extern Base_Type Extend_Memory_Space(Memory_Head* Head, Base_Type size)
{
	if (Head == NULL) return 0;
    size &= ~(Head->Align_Byte - 1);
    if(size == 0 || size < BLOCK_Min_SIZE) return 0;
	List_Node* End_addr = Head->End_Addr_Node;
	List_Node* End_Pre_addr = End_addr->Base.Pre_addr;
	List_Node* Extend_Node = NULL;
	//整理尾空间合并内存大小
	if (End_Pre_addr && !End_Pre_addr->Base.Used_flag)
	{//如果靠近尾部最后一块内存是空闲状态
		Extend_Node = End_Pre_addr;
		Delete_Node(Head, End_Pre_addr);
		Extend_Node->Base.Block_Size += End_addr->Base.Block_Size + size;
	}
	else
	{//否则，就把尾部哨兵块合并整理
		Extend_Node = End_addr;
		Extend_Node->Base.Block_Size += size;
	}
	//重新标记好每个块的信息
	End_addr = (List_Node*)((BYTE*)Extend_Node + (Extend_Node->Base.Block_Size - BLOCK_Min_SIZE));
	End_addr->Base.Used_flag = 1, End_addr->Base.Block_Size = BLOCK_Min_SIZE, End_addr->Base.Pre_addr = Extend_Node;
	Extend_Node->Base.Block_Size -= BLOCK_Min_SIZE;
	Head->Total_Size += size;//更新管理块大小
	Add_Node(Head, Extend_Node);
	Head->End_Addr_Node = End_addr;
    return size;
}
//尝试原地缩减管理的内存，返回值是缩减的内存字节
extern Base_Type Shrinkage_Memory(Memory_Head* Head, Base_Type Block_num, Base_Type Block_size)
{
    if (Head == NULL || Block_num == 0) return 0;
	Block_size &= ~(Head->Align_Byte - 1);//保持缩减后内存对齐
	if (Block_size == 0) return 0;
	List_Node* End_addr = Head->End_Addr_Node;
	List_Node* End_Pre_addr = End_addr->Base.Pre_addr;
 
	if (End_Pre_addr == NULL || End_Pre_addr->Base.Used_flag) return 0;//判断是否可以缩减内存
	Base_Type Shrink_num = End_Pre_addr->Base.Block_Size / Block_size;//获取缩减后的余量
	Shrink_num = Shrink_num < Block_num ? Shrink_num : Block_num;
	Base_Type Remain = End_Pre_addr->Base.Block_Size - (Shrink_num * Block_size);
	//如果余量过小,不足以支撑一个基础块的大小,则扩大余量，使剩余的余量支撑一个基础节点的大小
	if (Remain && Remain < BLOCK_Min_SIZE)
	{//如果余量过小
		if (Shrink_num * Block_size < (BLOCK_Min_SIZE - Remain)) return 0;//如果余量过小,不足以支撑一个基础块的大小
		Shrink_num = (End_Pre_addr->Base.Block_Size - BLOCK_Min_SIZE) / Block_size;//扩大余量，使剩余的余量支撑一个基础节点的大小
		Remain = End_Pre_addr->Base.Block_Size - (Shrink_num * Block_size);
	}
	if (Shrink_num == 0) return 0;//如果没有缩减的内存块数量,则直接返回
 
	Base_Type Reduct_byte = Shrink_num * Block_size;//计算缩减的内存字节数
	Delete_Node(Head, End_Pre_addr);//取出内存块
	End_Pre_addr->Base.Block_Size -= Reduct_byte;//缩减内存块的大小
	End_addr = (List_Node*)((BYTE*)End_Pre_addr + End_Pre_addr->Base.Block_Size);//重新计算尾部哨兵块的位置
	End_addr->Base.Used_flag = 1, End_addr->Base.Block_Size = BLOCK_Min_SIZE;//重新标记好每个块的信息
	if (Remain)	End_addr->Base.Pre_addr = End_Pre_addr, Add_Node(Head, End_Pre_addr);//如果余量不为0,则把缩减后的内存块添加回自由列表中
	Head->End_Addr_Node = End_addr;
	Head->Total_Size -= Reduct_byte;//更新管理块大小
 
	return Reduct_byte;
}
//获取当前管理的大小
extern Base_Type Get_Head_Total(Memory_Head* Head)
{
	return Head == NULL ? 0 : Head->Total_Size;
}
// Get_Current_Available函数用于获取当前内存管理器中可用的内存大小。它直接返回内存管理器头中的Available_Size字段，该字段表示当前可用的内存大小。
extern Base_Type Get_Current_Available(Memory_Head* Head)
{
	return Head == NULL ? 0 : Head->Available_Size;
}
// Get_Node_Size函数用于获取一个内存块节点的大小。它首先检查输入的地址是否为NULL，如果是，则返回0。否则，它通过将输入地址减去基本节点结构的大小来获取对应的List_Node结构，并返回该节点的块大小减去基本节点结构的大小，以得到实际可用的内存大小。
extern Base_Type Get_Node_Size(Memory_Head* Head, void* addr)
{
	if (Head == NULL || addr == NULL) return 0;
	List_Node* Node = (List_Node*)((BYTE*)addr - ALIGN_SIZE(sizeof(Base_Node), Head->Align_Byte));
	return Node->Base.Block_Size - ALIGN_SIZE(sizeof(Base_Node), Head->Align_Byte);
}

