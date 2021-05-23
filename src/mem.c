#include "mem.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdio.h>

static BYTE _ram[RAM_SIZE];

static struct {
	uint32_t proc;	// ID of process currently uses this page , PID đang dùng cái mem này
	int index;	// Index of the page in the list of pages allocated
			// to the process.
	int next;	// The next page in the list. -1 if it is the last
			// page.
} _mem_stat [NUM_PAGES];		//mem status

static pthread_mutex_t mem_lock;

void init_mem(void) {
	memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
	memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
	pthread_mutex_init(&mem_lock, NULL);
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr) {
	return addr & ~((~0U) << OFFSET_LEN);
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
	return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
	return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table table from the a segment table */
static struct page_table_t * get_page_table(
		addr_t index, 	// Segment level index
		struct seg_table_t * seg_table) { // first level table
	
	/*
	 * TODO: Given the Segment index [index], you must go through each
	 * row of the segment table [seg_table] and check if the v_index
	 * field of the row is equal to the index
	 *
	 * */

	int i;

	for (i = 0; i < seg_table->size; i++) {
		// Enter your code here
		if(seg_table->table[i].v_index == index)
			return seg_table->table[i].pages;
	}
	return NULL;
}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(																				//dịch từ bộ nhớ ảo sang thật
		addr_t virtual_addr, 	// Given virtual address											//truyền địa chỉ ảo
		addr_t * physical_addr, // Physical address to be returned									//truyền địa chỉ thưc
		struct pcb_t * proc) {  // Process uses given virtual address								// truyền proc xài mem

	/* Offset of the virtual address */
	addr_t offset = get_offset(virtual_addr);												    	 	//lấy 10 bits of set của địa chỉ 
	/* The first layer index */																			
	addr_t first_lv = get_first_lv(virtual_addr);														//lấy 5 bits segment của địa chỉ 			
	/* The second layer index */
	addr_t second_lv = get_second_lv(virtual_addr);														//lấy 5 bits page của địa chỉ
	
	/* Search in the first level */
	struct page_table_t * page_table = NULL;															// tạo một bảng phân trang 
	page_table = get_page_table(first_lv, proc->seg_table);												 //gán bảng đó = bẳng phân trang của của 5 bits seg so với segtable của proc 
	if (page_table == NULL) {																			// nếu kiếm khonng có thì trả về 0
		return 0;
	}

	int i;
	for (i = 0; i < page_table->size; i++) {
		if (page_table->table[i].v_index == second_lv) {												//nếu dò mà thấy v_index của bảng đó == 5 bits page thì
			/* TODO: Concatenate the offset of the virtual addess										//địa chỉ vâtj lý sẽ bằng p_index của bảng dó dịch trái 10 bits rồi ccộng 10 bits offset của nó
			 * to [p_index] field of page_table->table[i] to 
			 * produce the correct physical address and save it to
			 * [*physical_addr]  */
			*physical_addr = (page_table->table[i].p_index << OFFSET_LEN) + offset;
			return 1;
		}
	}
	return 0;	
}

addr_t alloc_mem(uint32_t size, struct pcb_t * proc) {
	pthread_mutex_lock(&mem_lock);															//mutex lock
	addr_t ret_mem = 0;																		//địa chỉ bắt đầu cấp phát
	/* TODO: Allocate [size] byte in the memory for the
	 * process [proc] and save the address of the first
	 * byte in the allocated memory region to [ret_mem].
	 * */

	uint32_t num_pages = ((size % PAGE_SIZE) == 0) ? size / PAGE_SIZE :
		size / PAGE_SIZE + 1; // Number of pages we will use

																							//nếu size/page_size vừa đủ thì lấy số đó còn không đủ (có dư) thì 1 thêm 1 page


	int mem_avail = 0; 																		// We could allocate new memory region or not? mem đang trống

	/* First we must check if the amount of free memory in
	 * virtual address space and physical address space is
	 * large enough to represent the amount of required 
	 * memory. If so, set 1 to [mem_avail].
	 * Hint: check [proc] bit in each page of _mem_stat
	 * to know whether this page has been used by a process.
	 * For virtual memory space, check bp (break pointer).
	 * */

	int numOfPage=0;// free space number
	for(int i=0;i<NUM_PAGES;i++)												//chạy i từ 0->số trang 
	{
		if(_mem_stat[i].proc==0)												// nếu cái trang đó đang trống khôgn có process nào xài thì 
		{
			numOfPage=numOfPage+1;												// số trang+1 cho đến khi nào mà 
			if(numOfPage==num_pages)											//số trang đang có == số trang cần
			{
				mem_avail=1;													// thì mem đó coi như available nhưng vẫn xét tiếp số trang cần và break point có vượt quá size của RAM không
				break;
			}
		}
	}
	if(proc->bp + num_pages * PAGE_SIZE > RAM_SIZE)								//nếu break point + số trang cần * size của 1 trang > kích thước của ram 
	{
		mem_avail=0;															// thì coi như mem đó không available
	}


	if (mem_avail) {
		/* We could allocate new memory region to the process */
		ret_mem = proc->bp;
		proc->bp += num_pages * PAGE_SIZE;										//break point += số trang * size của trang
		/* Update status of physical pages which will be allocated
		 * to [proc] in _mem_stat. Tasks to do:
		 * 	- Update [proc], [index], and [next] field
		 * 	- Add entries to segment table page tables of [proc]
		 * 	  to ensure accesses to allocated memory slot is
		 * 	  valid. */
		int num = 0;																			//num sẽ mang giá trị index trang đang cấp 
		int pre_index;	// index of the previous page in the list
		for(int i = 0; i < NUM_PAGES; i++){														//duyệt i từ 0 đén số trang 
			if(_mem_stat[i].proc) continue;														//nếu trang có proc rồi thì bỏ quá
			if(_mem_stat[i].proc == 0){															//nếu trang đó không có proc thì	
				_mem_stat[i].proc = proc->pid;													// gán PID cho trang đó để beiest trang đó có proc dùng rồi
				_mem_stat[i].index = num;//update index											// gán index 
				//update last page
				if(_mem_stat[i].index != 0)														//xét có phải trang đầu không nếu trang đầu tiên thì ko có tiền trang nên sẽ tạo một  
					_mem_stat[pre_index].next = i;												// tạo một biến đệm mang giá trị tiền trang(trang không đầu thì next của tiền trang là trang hiện tai)
				pre_index = i;																	
				//alloc
				addr_t virtual_address=ret_mem + num*PAGE_SIZE;									//  địa chỉ ảo: số trang hiện tại * size trang 
				addr_t virtual_segment=get_first_lv(virtual_address);							// dịch phải 15 bits để lấy địa chỉ segment

				struct page_table_t *virtual_page_table =										//con trỏ tới bảng phân trang
					get_page_table(virtual_segment, proc->seg_table);

				if(virtual_page_table != NULL){													// nếu đã có bảng thì thêm một trang mới cho 
					int index=virtual_page_table->size;
					virtual_page_table->table[index].v_index=get_second_lv(virtual_address);
					virtual_page_table->table[index].p_index=i;
					virtual_page_table->size++;
				}
				else{																			// chưa có thì tạo một cái bảng mới 
					//segment table size;
					int index=proc->seg_table->size;											// gán index = size của bảng seg của proc
					proc->seg_table->table[index].v_index=virtual_segment;						// index tại bảng tai index đó 
					//create new page table
					proc->seg_table->table[index].pages=(struct page_table_t *)malloc(sizeof(struct page_table_t));
					proc->seg_table->table[index].pages->table[0].v_index
						=get_second_lv(virtual_address);
					proc->seg_table->table[index].pages->table[0].p_index
						=i;
					proc->seg_table->table[index].pages->size=1;
					proc->seg_table->size++;
				}
				num++;// so luong page da cap phat
				if(num==num_pages){
					_mem_stat[i].next=-1;							
					break;
				}		
			}
		}
	}

	pthread_mutex_unlock(&mem_lock);
	return ret_mem;
}

int free_mem(addr_t address, struct pcb_t * proc) {
	pthread_mutex_lock(&mem_lock);
	addr_t virtual_address= address;															//địa chỉ bộ nhớ ảo truyền vào cần giải phóng
	addr_t physical_address=0;																	//process đang dùng nó
	//check address
	if(!translate(virtual_address, &physical_address, proc)) return 1;							// check xem đỉa chỉ ảo cso địa chỉ vậy lý không	
	//find physical page
	addr_t segment=physical_address>> OFFSET_LEN;												// 5 bits segment = đại chỉ vật lý dịch 
	int count_page=0;																			// đếm số trang
	//free physical
	for(int i=segment;i!=-1;i=_mem_stat[i].next){												//duyệt mấy trang trong bộ nhớ thực vật lý
		_mem_stat[i].proc=0;//gan process id=0													// gán bằng 0 hết để coi như chưa có proc dùng
		count_page=count_page+1;//count page													// đếm số trang 
	}

	
	for(int i=0;i<count_page;i++){					//clear virtual page table					// duyệt lại lân nữa trên bộ nhớ ảo
		addr_t virtual_page_address=virtual_address+i*PAGE_SIZE;								// tạo một biếnt đại chỉ trang ảo
		addr_t first_lv=get_first_lv(virtual_page_address);											// lấy 5 biến segment
		addr_t second_lv=get_second_lv(virtual_page_address);										// lấy 5 biếng page
		struct page_table_t * page_table=get_page_table(first_lv,proc->seg_table);					//lấy bảng phân trang của địa chỉ đó
		if(page_table!=NULL){																		//nếu bảng tồn tại thì 
			for(int j=0;j<page_table->size;j++){														// duyêt qua tất cả các trang trong bảng đó 
				if(page_table->table[j].v_index==second_lv){											// địa chỉ ảo của trang đó thì bằng 5 bits page
					page_table->size=page_table->size-1;												//size--
					page_table->table[j]=page_table->table[page_table->size];							//
					break;
				}
			}
		}
		else
		{
			continue;
		}
		if(page_table->size==0){
			//remove page_table
			for(int j=0;j<proc->seg_table->size;j++){
				if(proc->seg_table->table[i].v_index==first_lv){
					proc->seg_table->size=proc->seg_table->size-1;
					int index=proc->seg_table->size;
					proc->seg_table->table[i]=proc->seg_table->table[index];
					proc->seg_table->table[index].v_index=0;
					free(proc->seg_table->table[index].pages);
					break;
				}
			}
		}
	}
	proc->bp=proc->bp-count_page*PAGE_SIZE;//giam bp 
	pthread_mutex_unlock(&mem_lock);
	return 0;
}

int read_mem(addr_t address, struct pcb_t * proc, BYTE * data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		*data = _ram[physical_addr];
		return 0;
	}else{
		return 1;
	}
}

int write_mem(addr_t address, struct pcb_t * proc, BYTE data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		_ram[physical_addr] = data;
		return 0;
	}else{
		return 1;
	}
}

void dump(void) {
	int i;
	for (i = 0; i < NUM_PAGES; i++) {
		if (_mem_stat[i].proc != 0) {
			printf("%03d: ", i);
			printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
				i << OFFSET_LEN,
				((i + 1) << OFFSET_LEN) - 1,
				_mem_stat[i].proc,
				_mem_stat[i].index,
				_mem_stat[i].next
			);
			int j;
			for (	j = i << OFFSET_LEN;
				j < ((i+1) << OFFSET_LEN) - 1;
				j++) {
				
				if (_ram[j] != 0) {
					printf("\t%05x: %02x\n", j, _ram[j]);
				}
					
			}
		}
	}
}