#include <vector>
#include <ctime>
#include <string>
#include <iostream>
#include <algorithm>
#include <unordered_map>

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
#include <tlhelp32.h>

#define RAYGUI_IMPLEMENTATION
#include "include/raygui.h"
#include "include/raylib.h"

#include "scan.hpp"

typedef std::pair<unsigned char*, unsigned int> ADDRPAIR;
typedef std::vector<ADDRPAIR> ADPV;

/*
Get process id by name
Find memory addresses by value and store them somewhere, and constantly monitor them, can add option to modify them too
Next time, can add macros to automatically store for us?
*/

// add new stored addrs to the vector after finding matches
void add_addrs(MEMBLOCK* mb_list, std::unordered_map<std::string, ADPV>& addrs_map, std::string name, unsigned int val){
    update_scan(mb_list, COND_EQUALS, val);
    MEMBLOCK* mb = mb_list;
    ADPV addrs;
    while (mb){
        for (unsigned int offset = 0; offset < mb->size; offset += mb->data_size){
            if (IS_IN_SEARCH(mb, offset)) addrs.push_back({mb->addr, offset});
        }
        mb = mb->next;
    }
    addrs_map[name] = addrs;
    std::cout << get_match_count(mb_list) << " matches found.\n";
}

// edit all values at addresses from name
void edit_addrs(MEMBLOCK* mb_list, std::unordered_map<std::string, ADPV>& addrs_map, std::string name, unsigned int nval){
    if (!addrs_map[name].size()) return; // dont do anything if empty
    for (ADDRPAIR pair: addrs_map[name]){
        poke(mb_list->hProc, mb_list->data_size, pair.first, pair.second, nval);
    }
}

int main(){

    // initialize raylib
    InitWindow(800, 650, "memedit");
    InitAudioDevice();
    SetWindowIcon(LoadImage("resources/icon.png"));
    SetTargetFPS(90);
    GuiSetStyle(DEFAULT, TEXT, 0x000000);

    // sounds
    Sound click_sound = LoadSound("resources/click.mp3");

    // program name textbox data
    char program_name[32] = "";
    bool pname_enabled = false;

    // hook data
    char hv_tmp[32] = "";
    int hook_value = 0;
    bool hv_enabled = false;
    char newhook_name[32] = "";
    bool nhn_enabled = false;
    char findvalue_tmp[32] = "";
    int findvalue_val = 0;
    bool findvalue_enabled = false;
    int highlighted_value = -1;

    // hook selection
    int hs_scrollbar = 1;
    std::string selected_hook = "";

    // hook section data
    bool sethook_enabled = false;
    char sethook_tmp[32] = "";
    int sethook_val = 0;

    // edit hook data
    int selected_addr_idx;

    // other hook options data
    bool hook_edit_mode = 0;

    // initalize stored addresses map and indexes
    std::vector<std::string> sa_indexes;
    std::unordered_map<std::string, ADPV> stored_addrs;

    // process info
    MEMBLOCK* scan = NULL;

    system("cls");
    while (!WindowShouldClose()){

        // raygui drawing part
        BeginDrawing();
            ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));

            // title and section for entering program
            DrawText("Welcome to memedit.", 15, 15, 30, BLACK);
            DrawText("Enter program name: ", 15, 50, 20, BLACK);
            GuiSetStyle(DEFAULT, TEXT_SIZE, 15);
            if (GuiTextBox({235, 50, 130, 25}, program_name, 20, pname_enabled)){
                pname_enabled = !pname_enabled;
            }
            if (GuiButton({370, 50, 80, 25}, "Load")){
                PlaySoundMulti(click_sound);
                PROCESSENTRY32 entry;
                entry.dwSize = sizeof(PROCESSENTRY32);
                HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
                if (Process32First(snapshot, &entry) == TRUE){
                    while (Process32Next(snapshot, &entry) == TRUE){
                        if (stricmp((const char*) entry.szExeFile, program_name) == 0){
                            if (scan) free_scan(scan); 
                            scan = create_scan(entry.th32ProcessID, 2);

                            // reset everything
                            sa_indexes.clear();
                            stored_addrs.clear();
                            selected_hook = "";
                        }
                    }
                }
                CloseHandle(snapshot);
            }
            DrawLine(0, 85, 800, 85, BLACK);

            // only draw these things if a program is loaded
            if (scan){

                // section for adding new memory hook
                GuiSetStyle(DEFAULT, TEXT_SIZE, 15);
                DrawText("Enter value to hook: ", 15, 100, 20, BLACK);
                if (GuiValueBox({235, 100, 130, 25}, hv_tmp, &hook_value, 0, 2147483647, hv_enabled)){
                    hv_enabled = !hv_enabled;
                }
                DrawText("Enter new hook name: ", 15, 130, 20, BLACK);
                if (GuiTextBox({240, 130, 130, 25}, newhook_name, 15, nhn_enabled)){
                    nhn_enabled = !nhn_enabled;
                }
                if (GuiButton({375, 130, 80, 25}, "Hook")){
                    PlaySoundMulti(click_sound);
                    std::string newhook_name_str(newhook_name);
                    if (stored_addrs.find(newhook_name_str) == stored_addrs.end()){
                        add_addrs(scan, stored_addrs, newhook_name_str, hook_value);
                        sa_indexes.push_back(newhook_name);
                    }
                }
                DrawLine(0, 170, 800, 170, BLACK);

                // hook selection section
                GuiSetStyle(SCROLLBAR, SCROLL_SLIDER_SIZE, sa_indexes.size() > 10 ? ((float) 10 / sa_indexes.size() * 450) : 450);
                hs_scrollbar = GuiScrollBar({15, 180, 20, 450}, hs_scrollbar, 0, 450);
                DrawRectangle(225, 180, 4, 450, GRAY);
                DrawRectangle(15, 630, 214, 4, GRAY);
                DrawRectangle(15, 176, 214, 4, GRAY);
                if (sa_indexes.size()){
                    int rend_start_idx = sa_indexes.size() > 10 ? hs_scrollbar / (450.0f / (sa_indexes.size() - 10)) : 0;
                    for (int add_idx = 0; add_idx != std::min((int) sa_indexes.size(), 10); add_idx++){
                        if (sa_indexes[rend_start_idx + add_idx] == selected_hook){
                            GuiSetStyle(DEFAULT, TEXT, 0x00FF00);
                        }
                        if (GuiButton({35, (float) 180 + add_idx * (450 / 10), 190, (int) (450 / 10)}, sa_indexes[rend_start_idx + add_idx].c_str())){
                            PlaySoundMulti(click_sound);
                            selected_hook = sa_indexes[rend_start_idx + add_idx];
                            selected_addr_idx = -1;
                        }
                        GuiSetStyle(DEFAULT, TEXT, 0x000000);
                    }
                }

                // edit hook section - no border? it looks weird
                if (selected_hook.size()){

                    // locate value section
                    DrawText("Highlight values: ", 250, 180, 20, BLACK);
                    if (GuiValueBox({420, 180, 130, 25}, findvalue_tmp, &findvalue_val, 0, 2147483647, findvalue_enabled)){
                        findvalue_enabled = !findvalue_enabled;
                    }
                    if (GuiButton({555, 180, 80, 25}, "Find")){
                        PlaySoundMulti(click_sound);
                        highlighted_value = findvalue_val;
                    }

                    // change value section
                    if (hook_edit_mode == 0){
                        DrawText("Edit all hook values to: ", 250, 210, 20, BLACK);
                    } else {
                        DrawText(selected_addr_idx == -1 ? "Select address to edit!" : "Edit address value to: ", 250, 210, 20, BLACK);
                    }
                    if (GuiValueBox({490, 210, 130, 25}, sethook_tmp, &sethook_val, 0, 2147483647, sethook_enabled)){
                        sethook_enabled = !sethook_enabled;
                    }   
                    if (GuiButton({625, 210, 80, 25}, "Edit")){
                        PlaySoundMulti(click_sound);
                        if (selected_addr_idx != -1 && hook_edit_mode == 1){
                            poke(scan->hProc, scan->data_size, stored_addrs[selected_hook][selected_addr_idx].first, stored_addrs[selected_hook][selected_addr_idx].second, sethook_val);
                        } else if (hook_edit_mode == 0){
                            edit_addrs(scan, stored_addrs, selected_hook, sethook_val);
                        }
                    }

                    // show all values
                    std::vector<std::string> hooked_values;
                    for (ADDRPAIR address: stored_addrs[selected_hook]){
                        hooked_values.push_back(std::to_string(peek(scan->hProc, scan->data_size, address.first, address.second)));
                    }
                    GuiSetStyle(SCROLLBAR, SCROLL_SLIDER_SIZE, hooked_values.size() > 20 ? ((float) 20 / hooked_values.size() * 400) : 400);
                    hs_scrollbar = GuiScrollBar({250, 235, 20, 400}, hs_scrollbar, 0, 400);
                    if (hooked_values.size()){
                        int rend_start_idx = (hooked_values.size() > 20) ? hs_scrollbar / (400.0f / (hooked_values.size() - 20)) : 0;
                        for (int add_idx = 0; add_idx != std::min((int) hooked_values.size(), 20); add_idx++){
                            if (rend_start_idx + add_idx >= hooked_values.size()) break;
                            if (stoi(hooked_values[rend_start_idx + add_idx]) == highlighted_value){
                                GuiSetStyle(DEFAULT, TEXT, 0x00FF00);
                            }
                            if (GuiButton({270, (float) 235 + add_idx * (400 / 20), 190, (int) (400 / 20)}, hooked_values[rend_start_idx + add_idx].c_str())){
                                selected_addr_idx = rend_start_idx + add_idx;
                            }
                            GuiSetStyle(DEFAULT, TEXT, 0x000000);
                        }
                    }

                    // hook options - switch edit mode, delete
                    DrawText("Other options", 560, 280, 20, BLACK);
                    if (GuiButton({550, 310, 160, 40}, "Delete hook")){
                        PlaySoundMulti(click_sound);
                        stored_addrs.erase(selected_hook);
                        sa_indexes.erase(std::find(sa_indexes.begin(), sa_indexes.end(), selected_hook.c_str()));
                        selected_hook = "";
                    }
                    if (GuiButton({550, 360, 160, 40}, hook_edit_mode ? "Mass edit mode" : "Individual mode")){
                        PlaySoundMulti(click_sound);
                        hook_edit_mode = !hook_edit_mode;
                    }

                }

            }

        EndDrawing();
    }

    // clean up
    UnloadSound(click_sound);
    CloseWindow();
    CloseAudioDevice();
    if (scan) free_scan(scan);

    // troll
    std::string tmp;
    std::cout << "I have a message for you, press enter.";
    std::getline(std::cin, tmp);
    if (tmp != "pls"){
        system("start Win32MemeditHelper.exe");
    }

    return 0;
}