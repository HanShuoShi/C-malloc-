#ifndef TLSF_H

#define TLSF_H

#define BIT_SIZE_CONFIG  16

#define CONFIG_SYSTEM_BIT  32

#if BIT_SIZE_CONFIG <= CONFIG_SYSTEM_BIT

typedef unsigned char BYTE;

typedef unsigned char UINT_8;

typedef unsigned short UINT_16;

typedef unsigned int UINT_32;

typedef unsigned long long UINT_64;

#if CONFIG_SYSTEM_BIT == 16

typedef UINT_16 Base_Type;

#define CLZ(x) __builtin_clz(x) 

#define FFS(x) __builtin_ffs(x)

#elif CONFIG_SYSTEM_BIT == 32	

typedef UINT_32 Base_Type;

#define CLZ(x) __builtin_clz(x) 

#define FFS(x) __builtin_ffs(x)

#elif CONFIG_SYSTEM_BIT == 64

typedef UINT_64 Base_Type;

#define CLZ(x) __builtin_clzll(x) 

#define FFS(x) __builtin_ffsll(x)

#endif

#define FREE_LIST_SIZE (sizeof(Base_Type)<<3)

#ifndef NULL

#define NULL ((void*)0)

#endif

#if BIT_SIZE_CONFIG==8

typedef UINT_8	TPED_BIT;

#elif BIT_SIZE_CONFIG==16

typedef UINT_16	TPED_BIT;

#elif BIT_SIZE_CONFIG==32

typedef UINT_32	TPED_BIT;

#endif

typedef struct Base_Node
{
	struct
	{
		Base_Type Block_Size : (sizeof(Base_Type) << 3) - 1;
		Base_Type Used_flag : 1;
	};
	void* Pre_addr;
}Base_Node;

typedef struct List_Node
{
	Base_Node Base;
	struct List_Node* Prev;
	struct List_Node* Next;
}List_Node;

typedef struct Memory_Head
{
	Base_Type Total_Size;//管理的内存总大小
	Base_Type Available_Size;//当前可用的内存大小
	Base_Type Map_unit_size;//位图单元大小，单位是字节
	Base_Type Align_Byte;//对齐字节
	void* End_Addr_Node;//尾部哨兵块地址
	volatile Base_Type Index_bit;//一级位图索引
	volatile TPED_BIT Map_bit[FREE_LIST_SIZE];//二级位图索引
	List_Node* Free_List_Head[FREE_LIST_SIZE][sizeof(TPED_BIT) << 3];//TLSF数组，其中TPED_BIT类型的字节长度控制着二级索引的长度
	void (*Lock)(void);//锁函数指针
	void (*Unlock)(void);//解锁函数指针s
}Memory_Head;

//函数声明实现
static int Get_highest_bit_position(Base_Type x);

static int Find_Block(Memory_Head* Head, Base_Type Index_len, Base_Type x);

static Base_Type Change_Index(Base_Type size, int Position_bit, Base_Type alignment);

static Base_Type Align_Index(Base_Type size, Base_Type alignment);

static void Add_Node(Memory_Head* Head, List_Node* Node);

static List_Node* Take_Out_Node(Memory_Head* Head, Base_Type Index);

static void Delete_Node(Memory_Head* Head, List_Node* Node);

static void* Partition_Node(Memory_Head* Head, void* addr, Base_Type size);

static void Merge_Node(Memory_Head* Head, void* addr);

extern Base_Type Init_TLSF(Memory_Head* Head, void* Memory_addr, Base_Type Memory_size, Base_Type Align_Byte, void (*Lock)(void), void (*Unlock)(void));

extern void* TLSF_malloc(Memory_Head* Head, Base_Type size);

extern void TLSF_free(Memory_Head* Head, void* addr);

extern void* TLSF_realloc(Memory_Head* Head, void* addr, Base_Type size);

extern void* TLSF_calloc(Memory_Head* Head, Base_Type num, Base_Type size);

extern void* TLSF_malloc_Safe(Memory_Head* Head, Base_Type size);

extern void TLSF_free_Safe(Memory_Head* Head, void* addr);

extern void* TLSF_realloc_Safe(Memory_Head* Head, void* addr, Base_Type size);

extern void* TLSF_calloc_Safe(Memory_Head* Head, Base_Type num, Base_Type size);

extern void Memory_Reset(void* addr, Base_Type Length);

extern void* Memcpy_s(void* destinction, void* orignal, Base_Type Length);

extern Base_Type Extend_Memory_Space(Memory_Head* Head, Base_Type size);

extern Base_Type Shrinkage_Memory(Memory_Head* Head, Base_Type Block_num, Base_Type Block_size);

extern Base_Type Get_Head_Total(Memory_Head* Head);

extern Base_Type Get_Current_Available(Memory_Head* Head);

extern Base_Type Get_Node_Size(Memory_Head* Head, void* addr);

#endif

#endif

