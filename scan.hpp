#include <iostream>

// prevent clashing with raylib
#if defined(_WIN32)
#define MMNOSOUND
#define NOGDICAPMASKS     // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES // VK_*
#define NOWINMESSAGES     // WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES       // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS      // SM_*
#define NOMENUS           // MF_*
#define NOICONS           // IDI_*
#define NOKEYSTATES       // MK_*
#define NOSYSCOMMANDS     // SC_*
#define NORASTEROPS       // Binary and Tertiary raster ops
#define NOSHOWWINDOW      // SW_*
#define OEMRESOURCE       // OEM Resource values
#define NOATOM            // Atom Manager routines
#define NOCLIPBOARD       // Clipboard routines
#define NOCOLOR           // Screen colors
#define NOCTLMGR          // Control and Dialog routines
#define NODRAWTEXT        // DrawText() and DT_*
#define NOGDI             // All GDI defines and routines
#define NOKERNEL          // All KERNEL defines and routines
#define NOUSER            // All USER defines and routines
//#define NONLS             // All NLS defines and routines
#define NOMB              // MB_* and MessageBox()
#define NOMEMMGR          // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE        // typedef METAFILEPICT
#define NOMSG             // typedef MSG and associated routines
#define NOOPENFILE        // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL          // SB_* and scrolling routines
#define NOSERVICE         // All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND           // Sound driver routines
#define NOTEXTMETRIC      // typedef TEXTMETRIC and associated routines
#define NOWH              // SetWindowsHook and WH_*
#define NOWINOFFSETS      // GWL_*, GCL_*, associated routines
#define NOCOMM            // COMM driver routines
#define NOKANJI           // Kanji support stuff.
#define NOHELP            // Help engine interface.
#define NOPROFILER        // Profiler interface.
#define NODEFERWINDOWPOS  // DeferWindowPos routines
#define NOMCX             // Modem Configuration Extensions

// Type required before windows.h inclusion
typedef struct tagMSG *LPMSG;

#include <windows.h>
#endif

/*
VirtualQueryEx - find out which regions of processe's memory are used
ReadProcessMemory - read the memory locally
WriteProcessMemory
*/

#pragma once

#define WRITABLE (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)
#define IS_IN_SEARCH(mb, offset) (mb->searchmask[(offset / 8)] & (1 << ((offset) % 8)))
#define REMOVE_FROM_SEARCH(mb, offset) (mb->searchmask[(offset / 8)] &= ~(1 << ((offset) % 8)))

// hold info about one memory block in remote process
typedef struct _MEMBLOCK{
    HANDLE hProc;
    unsigned char* addr; // base address in another process
    int size; // size for how much memory
    unsigned char *buffer; // copy data into this when reading

    unsigned char* searchmask; // flag for each byte in buffer, decide whether it should be included in search
    int matches;
    int data_size; // search comparison: byte or dword basis

    struct _MEMBLOCK *next; // linked list functionality
} MEMBLOCK;

typedef enum {
    COND_UNCONDITIONAL,
    COND_EQUALS,
    // COND_INCREASED,
    // COND_DECREASED,
} SEARCH_CONDITION;

MEMBLOCK* create_memblock(HANDLE hProc, MEMORY_BASIC_INFORMATION* meminfo, int data_size){
    MEMBLOCK* mb = (MEMBLOCK*) malloc(sizeof(MEMBLOCK));
    if (mb){
        mb->hProc = hProc;
        mb->addr = (unsigned char*) meminfo->BaseAddress;
        mb->size = meminfo->RegionSize;
        mb->searchmask = (unsigned char*) malloc(meminfo->RegionSize / 8);
        memset(mb->searchmask, 0xff, meminfo->RegionSize / 8);
        mb->matches = meminfo->RegionSize;
        mb->data_size = data_size;
        mb->buffer = (unsigned char*) malloc(meminfo->RegionSize); // create some space
    }
    return mb;
}

void free_memblock(MEMBLOCK* mb){
    if (mb){
        // need to free the buffer first
        if (mb->buffer){
            free(mb->buffer);
        }
        if (mb->searchmask){
            free(mb->searchmask);
        }
        free(mb);
    }
}

// read process memory
void update_memblock(MEMBLOCK* mb, SEARCH_CONDITION condition, unsigned int val){
    static unsigned char tempbuf[128 * 1024]; // local buffer 128KB
    size_t bytes_left;
    size_t total_read;
    size_t bytes_to_read;
    size_t bytes_read;

    bytes_left = mb->size;
    total_read = 0;

    if (mb->matches <= 0) return;

    mb->matches = 0; // ok reset the number of matches

    while (bytes_left){
        bytes_to_read = (bytes_left > sizeof(tempbuf) ? sizeof(tempbuf) : bytes_left);
        ReadProcessMemory(mb->hProc, mb->addr + total_read, tempbuf, bytes_to_read, &bytes_read);
        if (bytes_read != bytes_to_read) break; // just stop if there was a problem

        // check if match and set flags
        if (condition == COND_UNCONDITIONAL){
            memset(mb->searchmask + (total_read / 8), 0xff, bytes_read / 8);
            mb->matches += bytes_read;
        } else {
            for (unsigned int offset = 0; offset < bytes_read; offset += mb->data_size){
                if (IS_IN_SEARCH(mb, (total_read + offset))){
                    BOOL is_match = FALSE;
                    unsigned int temp_val;

                    switch (mb->data_size){
                        case 1: // byte size
                            temp_val = tempbuf[offset];
                            break;
                        case 2: // 2 bytes
                            temp_val = *((unsigned short*) &tempbuf[offset]);
                            break;
                        case 4: // 4 bytes
                        default:
                            temp_val = *((unsigned int*) &tempbuf[offset]);
                    }

                    switch (condition){
                        case COND_EQUALS:
                            is_match = (temp_val == val);
                            break;
                        default:
                            break;
                    }

                    if (is_match){
                        mb->matches++;
                    } else {
                        REMOVE_FROM_SEARCH(mb, (total_read + offset));
                    }
                }
            }
        }

        // copy from tempbuf to actual buffer
        memcpy(mb->buffer + total_read, tempbuf, bytes_read);

        bytes_left -= bytes_read;
        total_read += bytes_read;
    }

    mb->size = total_read;
}

MEMBLOCK* create_scan(unsigned int pid, int data_size){
    MEMBLOCK* mb_list = NULL; // head of (reverse) linked list
    MEMORY_BASIC_INFORMATION meminfo; // struct to use with virtualqueryex
    unsigned char* addr = 0; // keep track of address that is passed to virtualqueryex
    // addr is 0: start with the lowest address first

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

    if (hProc){
        // loop through process memory with virtualqueryex
        while (true){
            if (VirtualQueryEx(hProc, addr, &meminfo, sizeof(meminfo)) == 0){ // start looking from this address, if 0 is too high
                break;
            }

            // not readable or writable, skip
            if ((meminfo.State & MEM_COMMIT) && (meminfo.Protect & WRITABLE)){

                // got block, create struct and add to linkedlist
                MEMBLOCK* mb = create_memblock(hProc, &meminfo, data_size);
                if (mb){
                    mb->next = mb_list;
                    mb_list = mb;
                }
            }

            // regionsize is size of the block, so go to next address
            addr = (unsigned char*) meminfo.BaseAddress + meminfo.RegionSize; // update the address
        }
    }

    return mb_list;
}

void free_scan(MEMBLOCK* mb_list){
    CloseHandle(mb_list->hProc);
    while (mb_list){
        MEMBLOCK* mb = mb_list;
        mb_list = mb_list->next;
        free_memblock(mb);
    }
}

void update_scan(MEMBLOCK* mb_list, SEARCH_CONDITION condition, unsigned int val){
    MEMBLOCK* mb = mb_list;
    while (mb){
        update_memblock(mb, condition, val);
        mb = mb->next;
    }
}

// print all found information (sizes)
void dump_scan_info(MEMBLOCK* mb_list){
    MEMBLOCK* mb = mb_list;
    while (mb){
        printf("0x%08x size %d\n", mb->addr, mb->size);
        for (int i = 0; i < mb->size; i++){
            printf("%02x", mb->buffer[i]);
        }
        printf("\n");
        mb = mb->next;
    }
}

void poke(HANDLE hProc, size_t data_size, unsigned char* addr, unsigned int offset, unsigned int val){
    if (WriteProcessMemory(hProc, addr + offset, &val, data_size, NULL) == 0){
        // std::cout << "poke failed\n";
    }
}

unsigned int peek(HANDLE hProc, size_t data_size, unsigned char* addr, unsigned int offset){
    unsigned int val = 0;
    if (ReadProcessMemory(hProc, addr + offset, (void*) &val, data_size, NULL) == 0){
        // std::cout << "peek failed\n";
    }
    return val;
}

void peek_matches(MEMBLOCK* mb_list){
    MEMBLOCK* mb = mb_list;
    while (mb){
        for (unsigned int offset = 0; offset < mb->size; offset += mb->data_size){
            if (IS_IN_SEARCH(mb, offset)){
                unsigned int val = peek(mb->hProc, mb->data_size, mb->addr, offset);
                printf("08x%08x (%d)\n", mb->addr + offset, val);
            }
        }
        mb = mb->next;
    }
}

void poke_matches(MEMBLOCK* mb_list, unsigned int nval, bool silent = true){
    MEMBLOCK* mb = mb_list;
    while (mb){
        for (unsigned int offset = 0; offset < mb->size; offset += mb->data_size){
            if (IS_IN_SEARCH(mb, offset)){
                unsigned int val = peek(mb->hProc, mb->data_size, mb->addr, offset);
                if (!silent) printf("08x%08x (%d) -> (%d)\n", mb->addr + offset, val, nval);
                poke(mb->hProc, mb->data_size, mb->addr, offset, nval);
            }
        }
        mb = mb->next;
    }
}

int get_match_count(MEMBLOCK* mb_list){
    MEMBLOCK* mb = mb_list;
    int count = 0;
    while (mb){
        count += mb->matches;
        mb = mb->next;
    }
    return count;
}

int SCAN_main(){
    unsigned int pid;
    std::cin >> pid;
    MEMBLOCK* scan = create_scan(pid, 4);
    update_scan(scan, COND_EQUALS, 12345);
    if (scan){
        peek_matches(scan);
        poke_matches(scan, 688);
        // dump_scan_info(scan);
    }
    std::cout << get_match_count(scan) << " matches found.\n";
    free_scan(scan);
    return 0;
}