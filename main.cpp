#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <cstring>
#include <sstream>
#include <vector>

using namespace std;

/* -------------------------------------------------------------------------------------------------
see my_print.asm for nasm_print()
------------------------------------------------------------------------------------------------- */
extern "C" {
    void nasm_print(const char *, const int);
}

typedef unsigned char u8; // 1B
typedef unsigned short u16; // 2B
typedef unsigned int u32; // 4B

/* -------------------------------------------------------------------------------------------------
Error Warning Hint
------------------------------------------------------------------------------------------------- */
const char *ERR_PARAMETER_WRONG = "ERROR: parameter of ls is wrong!\n";
const char *ERR_COMMAND_WRONG = "ERROR: command not found!\n";
const char *ERR_NO_DIR = "ERROR: cannot find the directory!\n";
const char *ERR_NO_FILE = "ERROR: cannot find the file!\n";
const char *ERR_CANT_OPEN = "ERROR: file cannot be opened!\n";

/* -------------------------------------------------------------------------------------------------
frequently used variables
------------------------------------------------------------------------------------------------- */
int BytesPerSec; // Bytes per sector
int SecPerClus; // Sectors per cluster
int RsvdSecCnt; // Sectors of boot-sector
int NumFATs; // FAT table counts
int RootEntCnt; // Max file counts of root entry
int FATSz; // FAT sector counts

/* -------------------------------------------------------------------------------------------------
specially used for assembleString(vector<string> &sv)
------------------------------------------------------------------------------------------------- */
vector<string> strList;

/* -------------------------------------------------------------------------------------------------
specially used for myPrint() info
------------------------------------------------------------------------------------------------- */
string str_print;

#pragma pack (1)

/* -------------------------------------------------------------------------------------------------
BPB 25B
 already skipped first 11B (BPB_jmpBOOT, BPB_OEMName)
------------------------------------------------------------------------------------------------- */
struct BPB {
    u16  BPB_BytesPerSec; // Bytes per sector [ default : 512B(0x200h) ]
    u8   BPB_SecPerClus; // Sectors per cluster [ default : FAT12 1 sector per cluster ]
    u16  BPB_RsvdSecCnt; // Sectors of boot-sector [ default : 1 sector ]
    u8   BPB_NumFATs; // FAT table counts [ default : 2(FAT1, FAT2) ]
    u16  BPB_RootEntCnt; // Max counts of root entry [ default : 224 ]
    u16  BPB_TotSec16; // Total sectors [ default : 2880(0xB40) ]
    u8   BPB_Media; // Media descriptor
    u16  BPB_FATSz16; // FAT sector counts [ default : 9, FAT1: sec1~sec9, FAT2: sec10~sec18 ]
    u16  BPB_SecPerTrk; // Sectors per track [ default : 18 ]
    u16  BPB_NumHeads; // Head numbers  [ default : 2 ]
    u32  BPB_HiddSec; // Hidden sectors
    u32  BPB_TotSec32; // if BPB_TotSec16 = 0, use this field
};

/* -------------------------------------------------------------------------------------------------
root entry structure, 32B
------------------------------------------------------------------------------------------------- */
struct RootEntry {
    u8 DIR_Name[11];
    u8 DIR_Attr;
    u8 reserved[10];
    u16 DIR_WrtTime;
    u16 DIR_WrtDate;
    u16  DIR_FstClus; // start cluster index
    u32  DIR_FileSize;
};

#pragma pack ()

/* -------------------------------------------------------------------------------------------------
Node class that composes list
------------------------------------------------------------------------------------------------- */
class Node {
public:
    string name; // name of node
    vector<Node *> next; // Node vector of next directory
    string path; // record path, convenient for output
    u32 FileSize; // file size(B)
    bool isfile = false; // stamp of file or directory
    bool isval = true; // stamp of ./..
    int dir_count = 0; // record next level directories
    int file_count = 0; // record next level files
    char *content = new char[10000]{ 0 }; // store file content
};

/* -------------------------------------------------------------------------------------------------
function declarations
------------------------------------------------------------------------------------------------- */
void fillBPB(FILE * fat12, struct BPB* bpb_ptr); // load BPB
void ReadFiles(FILE * fat12, struct RootEntry* rootEntry_ptr, Node *father); // read root entry
void readChildren(FILE * fat12, int startClus, Node *father); // read son entry
int  getFATValue(FILE * fat12, int num); // read FAT table and get next cluster index
void printLS(Node *root); // ls cmd print
void createNode(Node *p, Node *father); // create ./.. node
void printLS_L(Node *root); // ls-l cmd print
void printLSWithPath(Node *root, string path, int & exist, bool hasL); // ls-d [addr] cmd print
void getContent(FILE * fat12, int startClus, Node* son); // get file content when initialize Node list
void printCat(Node *root, string path, int & exist); // cat cmd print
void myPrint(const char* p); // nasm print
void split(const string &s, vector<string> &sv, char flag);
void pathDeal(string &s);
string assembleStringRED(vector<string> &sv);
string assembleString(vector<string> &sv);

/* -------------------------------------------------------------------------------------------------
main.cpp entry
------------------------------------------------------------------------------------------------- */
int main() {
    FILE* fat12;
    fat12 = fopen("a.img", "rb");

    struct BPB bpb;
    struct BPB* bpb_ptr = &bpb;

    fillBPB(fat12, bpb_ptr);

    BytesPerSec = bpb_ptr->BPB_BytesPerSec;
    SecPerClus = bpb_ptr->BPB_SecPerClus;
    RsvdSecCnt = bpb_ptr->BPB_RsvdSecCnt;
    NumFATs = bpb_ptr->BPB_NumFATs;
    RootEntCnt = bpb_ptr->BPB_RootEntCnt;

    if (bpb_ptr->BPB_BytesPerSec != 0) {
        FATSz = bpb_ptr->BPB_FATSz16;
    }
    else {
        FATSz = bpb_ptr->BPB_TotSec32;
    }

    struct RootEntry rootEntry;
    struct RootEntry* rootEntry_ptr = &rootEntry;

    // root Node
    Node* root = new Node();
    root->name = "";
    root->path = "/";

    // create file list
    ReadFiles(fat12, rootEntry_ptr, root);

    // analyze input commands
    while (true) {
        myPrint(">");
        string cmd;
        getline(cin, cmd);
	if(cmd == "") {
	    continue;
	}
	bool isAllSpace = true;
        for(int i = 0; i < cmd.length(); i++) {
	    if(cmd.at(i) != ' ') {
		isAllSpace = false;
            }
	}
	if(isAllSpace) continue;
        vector<string> cmd_list;
        split(cmd, cmd_list, ' ');
        for (vector<string>::iterator itr = cmd_list.begin(); itr != cmd_list.end();) { // delete space index in cmd_list
            if (*itr == "") {
                itr = cmd_list.erase(itr);
            }
            else {
                itr++;
            }
        }

        // exit cmd
        if (cmd_list[0] == "exit") {
            myPrint("Bye!\n");
            fclose(fat12);
            return 0;
        }
        // ls related cmd
        else if (cmd_list[0] == "ls") {
            // cond1 : ls
            if (cmd_list.size() == 1) {
                printLS(root);
            }
            else {
                bool hasL = false;
                bool hasPath = false;
                bool error = false;
                string *path = NULL;
                for (int i = 1; i < cmd_list.size(); i++) { // go through all params
                    string param = cmd_list[i];
                    if (param[0] != '-') {
                        // path
                        if (hasPath) {
                            myPrint(ERR_PARAMETER_WRONG);
                            error = true;
                            break;
                        }
                        else {
                            hasPath = true;
                            pathDeal(cmd_list[i]);
                            path = &cmd_list[i];
                        }
                    }
                    else {
                        // -[param]
                        if (param.length() == 1) {
                            // -
                            myPrint(ERR_PARAMETER_WRONG);
                            error = true;
                            break;
                        }
                        for (int j = 1; j < param.length(); j++) {
                            if (param[j] != 'l') {
                                myPrint(ERR_PARAMETER_WRONG);
                                error = true;
                                break;
                            }
                        }
                        hasL = true;
                    }
                }
                if (error) {
                    continue;
                }
                // cond2 : ls -l
                int exist = 0;
                if (hasL && !hasPath) {
                    exist = 1;
                    printLS_L(root);
                }
                else if (!hasL && hasPath) {
                    // cond3 : ls /NJU
                    printLSWithPath(root, *path, exist, false);
                }
                else if (hasL && hasPath) {
                    // cond4 : ls /NJU -l
                    printLSWithPath(root, *path, exist, true);
                }
                else {
                    printLS(root);
                    continue;
                }
                if (exist == 0) {
                    myPrint(ERR_NO_DIR);
                    continue;
                }
                else if (exist == 2) {
                    myPrint(ERR_CANT_OPEN);
                    continue;
                }
            }
        }
        // cat cmd
        else if (cmd_list[0] == "cat") {
            if (cmd_list.size() == 2 && cmd_list[1][0] != '-') {
                // cat [path]
                int exist = 0;
                pathDeal(cmd_list[1]);
                printCat(root, cmd_list[1], exist);
                if (exist == 0) {
                    myPrint(ERR_NO_FILE);
                    break;
                }
                else if (exist == 2) {
                    myPrint(ERR_CANT_OPEN);
                    break;
                }
            }
            else {
                // cat cmd without param or too many param
                myPrint(ERR_PARAMETER_WRONG);
                continue;
            }
        }
        // other invalid cmd input
        else {
            myPrint(ERR_COMMAND_WRONG);
            continue;
        }
    }
}

void fillBPB(FILE * fat12, struct BPB* bpb_ptr) { // read BPB and load into bpb_ptr
    int check;

    // BPB need to start from offset 11B
    check = fseek(fat12, 11, SEEK_SET);
    if (check == -1) {
        myPrint("fseek in fillPBP failed!\n");
    }

    // read 25B
    check = fread(bpb_ptr, 1, 25, fat12);
    if (check != 25) {
        myPrint("fread in fillPBP failed!\n");
    }
}

void ReadFiles(FILE * fat12, struct RootEntry* rootEntry_ptr, Node *father) {
    int base = (RsvdSecCnt + NumFATs * FATSz) * BytesPerSec; // first Byte of root directory offset
    int check;
    char realName[12]; // temp store file name

    // process each entry of root directory
    for (int i = 0; i < RootEntCnt; i++) {
        check = fseek(fat12, base, SEEK_SET);
        if (check == -1) {
            myPrint("fseek in ReadFiles failed!\n");
        }

        // each entry occupies 32B
        check = fread(rootEntry_ptr, 1, 32, fat12);
        if (check != 32) {
            myPrint("fread in ReadFiles failed!\n");
        }
        base += 32;

        if (rootEntry_ptr->DIR_Name[0] == '\0') { // empty entry
            continue;
        }

        // filter illegal files
        int flag = 0;
        for (int i = 0; i < 11; i++) {
            if (!(((rootEntry_ptr->DIR_Name[i] >= 48) && (rootEntry_ptr->DIR_Name[i] <= 57)) ||
                  ((rootEntry_ptr->DIR_Name[i] >= 65) && (rootEntry_ptr->DIR_Name[i] <= 90)) ||
                  ((rootEntry_ptr->DIR_Name[i] >= 97) && (rootEntry_ptr->DIR_Name[i] <= 122)) ||
                  (rootEntry_ptr->DIR_Name[i] == ' '))) {
                flag = 1; // not English, digits, space
                break;
            }
        }
        if (flag == 1) { // illegal files
            continue;
        }

        if ((rootEntry_ptr->DIR_Attr & 0x10) == 0) {
            // file
            int temp = -1;
            for (int i = 0; i < 11; i++) {
                if (rootEntry_ptr->DIR_Name[i] != ' ') {
                    temp++;
                    realName[temp] = rootEntry_ptr->DIR_Name[i];
                }
                else {
                    temp++;
                    realName[temp] = '.';
                    while (rootEntry_ptr->DIR_Name[i] == ' ') {
                        i++;
                    }
                    i--;
                }
            }
            temp++;
            realName[temp] = '\0'; // done process, realName = fileName
            Node *son = new Node(); // newly create file Node
            father->next.push_back(son); // store into father Node's next vector
            son->name = realName;
            son->FileSize = rootEntry_ptr->DIR_FileSize;
            son->isfile = true;
            strList.push_back(father->path);
            strList.push_back(realName);
            strList.push_back("/");
            son->path = assembleString(strList);
            // son->path = father->path + realName + "/";
            father->file_count++;
            getContent(fat12, rootEntry_ptr->DIR_FstClus, son); // read file's content
        }
        else {
            // directory
            int temp = -1;
            for (int i = 0; i < 11; i++) {
                if (rootEntry_ptr->DIR_Name[i] != ' ') {
                    temp++;
                    realName[temp] = rootEntry_ptr->DIR_Name[i];
                }
                else {
                    temp++;
                    realName[temp] = '\0';
                    break;
                }
            } // done process, realName = dirName

            Node* son = new Node();
            father->next.push_back(son);
            son->name = realName;
            strList.push_back(father->path);
            strList.push_back(realName);
            strList.push_back("/");
            son->path = assembleString(strList);
            // son->path = father->path + realName + "/";
            father->dir_count++;
            createNode(son, father);
            // output directory and son files
            readChildren(fat12, rootEntry_ptr->DIR_FstClus, son);
        }
    }
}

void readChildren(FILE * fat12, int startClus, Node *father) {
    // first Byte of dataBase offset
    int dataBase = BytesPerSec * (RsvdSecCnt + FATSz * NumFATs + (RootEntCnt * 32 + BytesPerSec - 1) / BytesPerSec);

    int currentClus = startClus;

    // value if used to check if there are clusters > 1 (check FAT table)
    int value = 0;
    while (value < 0xFF8) { // value >= 0xFF8, means cluster is already last cluster of file
        value = getFATValue(fat12, currentClus); // check FAT table for next cluster index
        if (value == 0xFF7) {
            myPrint("Bad cluster, read failed!\n");
            break;
        }

        int startByte = dataBase + (currentClus - 2) * SecPerClus * BytesPerSec; // FAT entry0,1 is never used
        int check;
        int length = SecPerClus * BytesPerSec; // Bytes per cluster
        int loop = 0;
        while (loop < length) {
            RootEntry sonEntry; // read directory entry
            RootEntry *sonEntryP = &sonEntry;
            check = fseek(fat12, startByte + loop, SEEK_SET);
            if (check == -1) {
                myPrint("fseek in readChildren failed!\n");
            }
            check = fread(sonEntryP, 1, 32, fat12);
            if (check != 32) {
                myPrint("fread in readChildren failed!\n");
            } // finished read

            loop += 32;

            if (sonEntryP->DIR_Name[0] == '\0') { // empty entry
                continue;
            }

            // filter illegal files
            int flag = 0;
            for (int i = 0; i < 11; i++) {
                if (!(((sonEntryP->DIR_Name[i] >= 48) && (sonEntryP->DIR_Name[i] <= 57)) ||
                      ((sonEntryP->DIR_Name[i] >= 65) && (sonEntryP->DIR_Name[i] <= 90)) ||
                      ((sonEntryP->DIR_Name[i] >= 97) && (sonEntryP->DIR_Name[i] <= 122)) ||
                      (sonEntryP->DIR_Name[i] == ' '))) {
                    flag = 1; // not English, digits, space
                    break;
                }
            }
            if (flag == 1) { // illegal files
                continue;
            }

            if ((sonEntryP->DIR_Attr & 0x10) == 0) {
                // file
                char tempName[12];
                int temp = -1;
                for (int i = 0; i < 11; i++) {
                    if (sonEntryP->DIR_Name[i] != ' ') {
                        temp++;
                        tempName[temp] = sonEntryP->DIR_Name[i];
                    }
                    else {
                        temp++;
                        tempName[temp] = '.';
                        while (sonEntryP->DIR_Name[i] == ' ') {
                            i++;
                        }
                        i--;
                    }
                }
                temp++;
                tempName[temp] = '\0'; // done process, tempName = fileName
                Node *son = new Node();
                father->next.push_back(son);
                son->name = tempName;
                son->FileSize = sonEntryP->DIR_FileSize;
                son->isfile = true;
                strList.push_back(father->path);
                strList.push_back(tempName);
                strList.push_back("/");
                son->path = assembleString(strList);
                // son->path = father->path + tempName + '/';
                father->file_count++;
                getContent(fat12, sonEntryP->DIR_FstClus, son);
            }
            else {
                // directory
                char tempName[12];
                int temp = -1;
                for (int i = 0; i < 11; i++) {
                    if (sonEntryP->DIR_Name[i] != ' ') {
                        temp++;
                        tempName[temp] = sonEntryP->DIR_Name[i];
                    }
                    else {
                        temp++;
                        tempName[temp] = '\0';
                    }
                }

                Node* son = new Node();
                father->next.push_back(son);
                son->name = tempName;
                strList.push_back(father->path);
                strList.push_back(tempName);
                strList.push_back("/");
                son->path = assembleString(strList);
                // son->path = father->path + tempName + "/";
                father->dir_count++;
                createNode(son, father);
                // output directory and son files
                readChildren(fat12, sonEntryP->DIR_FstClus, son);
            }
        }
        currentClus = value; // next cluster
    }
}

int getFATValue(FILE * fat12, int num) {
    // FAT1 offset
    int fatBase = RsvdSecCnt * BytesPerSec;
    // FAT entry position (each FAT entry is 12b, 1.5B)
    int fatPos = fatBase + num * 3 / 2;
    // process even/odd FAT entry with different approach, classify them
    int type = 0;
    if (num % 2 != 0) {
        type = 1;
    }

    // read the first 2 Bytes out, because FAT entry0,1 never used
    u16 bytes;
    u16* bytes_ptr = &bytes;
    int check;
    check = fseek(fat12, fatPos, SEEK_SET);
    if (check == -1) {
        myPrint("fseek in getFATValue failed!\n");
    }
    check = fread(bytes_ptr, 1, 2, fat12);
    if (check != 2) {
        myPrint("fread in getFATValue failed!\n");
    }

    // type = 0 | byte2[low4] + byte1
    // type = 1 | byte2 + byte1[high4]
    if (type == 0) {
        bytes = bytes << 4;
        return bytes >> 4;
    }
    else {
        return bytes >> 4;
    }
}

void printLS(Node *root) {
    // go through all nodes starting from Node root
    Node *p = root;
    if (p->isfile) {
        return;
    }
    else {
        strList.push_back(p->path);
        strList.push_back(":");
        strList.push_back("\n");
        str_print = assembleString(strList);
        // str_print = p->path + ":" + "\n";
        myPrint(str_print.c_str());
        str_print.clear();
        // print every next
        Node *q;
        int len = p->next.size();
        for (int i = 0; i < len; i++) {
            q = p->next[i];
            if (!q->isfile) {
                // folder
                // cout << "\033[31m" << q->name << "\033[0m" << "  ";
                // str_print = "\033[31m" + q->name + "\033[0m" + "  ";
                strList.push_back(q->name);
                strList.push_back("  ");
                str_print = assembleStringRED(strList);
                // str_print = q->name + "  ";
                myPrint(str_print.c_str());
                str_print.clear();
            }
            else {
                // file
                // cout << q->name << "  ";
                strList.push_back(q->name);
                strList.push_back("  ");
                str_print = assembleString(strList);
                // str_print = q->name + "  ";
                myPrint(str_print.c_str());
                str_print.clear();
            }
        }
        // cout << endl;
        myPrint("\n");

        // recursive printLS
        for (int i = 0; i < len; i++) {
            if (p->next[i]->isval) {
                printLS(p->next[i]);
            }
        }
    }
}

void createNode(Node *p, Node *father) {
    Node* q = new Node();
    q->name = ".";
    q->isval = false;
    p->next.push_back(q);
    q = new Node();
    q->name = "..";
    q->isval = false;
    p->next.push_back(q);
}

void printLS_L(Node *root) {
    Node *p = root;

    if (p->isfile) {
        return;
    }
    else {
        // cout << p->path <<" "<<p->dir_count<<" "<<p->file_count<< ":" << endl;
        strList.push_back(p->path);
        strList.push_back(" ");
        strList.push_back(to_string(p->dir_count));
        strList.push_back(" ");
        strList.push_back(to_string(p->file_count));
        strList.push_back(":\n");
        str_print = assembleString(strList);
        // str_print = p->path + " " + to_string(p->dir_count) + " " + to_string(p->file_count) + ":\n";
        myPrint(str_print.c_str());
        str_print.clear();
        // print every next
        Node *q;
        int len = p->next.size();
        for (int i = 0; i < len; i++) {
            q = p->next[i];
            if (!q->isfile) {
                // folder
                if (q->isval) {
                    // str_print = "\033[31m" + q->name + "\033[0m" + "  " + to_string(q->dir_count) + " " + to_string(q->file_count) + "\n";
                    strList.push_back(q->name);
                    strList.push_back("  ");
                    strList.push_back(to_string(q->dir_count));
                    strList.push_back(" ");
                    strList.push_back(to_string(q->file_count));
                    strList.push_back("\n");
                    str_print = assembleStringRED(strList);
                    // str_print = q->name + "  " + to_string(q->dir_count) + " " + to_string(q->file_count) + "\n";
                    myPrint(str_print.c_str());
                    str_print.clear();
                }
                else {
                    // deal with . ..
                    // cout << q->name << "  "<<endl;
                    // str_print = "\033[31m" + q->name + "\033[0m" + "  \n";
                    strList.push_back(q->name);
                    strList.push_back("  \n");
                    str_print = assembleStringRED(strList);
                    // str_print = q->name + "  \n";
                    myPrint(str_print.c_str());
                    str_print.clear();
                }
            }
            else {
                // file
                // cout << q->name << "  " << q->FileSize << endl;
                strList.push_back(q->name);
                strList.push_back("  ");
                strList.push_back(to_string(q->FileSize));
                strList.push_back("\n");
                str_print = assembleString(strList);
                // str_print = q->name + "  " + to_string(q->FileSize) + "\n";
                myPrint(str_print.c_str());
                str_print.clear();
            }
        }
        // cout << endl;
        myPrint("\n");
        // recursion
        for (int i = 0; i < len; i++) {
            if (p->next[i]->isval) {
                printLS_L(p->next[i]);
            }
        }
    }
}

void printLSWithPath(Node *root, string path, int & exist, bool hasL) {
    if (path == root->path) { // path is exactly same
        // found
        if (root->isfile) { // if is a file, cannot be opened
            exist = 2;
            return;
        }
        else {
            exist = 1;
            if (hasL) {
                printLS_L(root);
            }
            else {
                printLS(root);
            }
        }
        return;
    }
    if (path.length() <= root->path.length()) {
        return;
    }
    // cut path substring, compare with current path
    // if same, means target is level under current node
    string temp = path.substr(0, root->path.length());
    if (temp == root->path) {
        // path matched, start recursion
        for (Node *q : root->next) {
            printLSWithPath(q, path, exist, hasL);
        }
    }
}

void getContent(FILE * fat12, int startClus, Node* son) {
    int dataBase = BytesPerSec * (RsvdSecCnt + FATSz * NumFATs + (RootEntCnt * 32 + BytesPerSec - 1) / BytesPerSec);
    int currentClus = startClus;
    int value = 0; // used to get more clusters (> 512B)
    char *p = son->content; // final pointer that points to content

    if (startClus == 0) {
        return;
    }

    while (value < 0xFF8) {
        value = getFATValue(fat12, currentClus);
        if (value == 0xFF7) {
            myPrint("Bad cluster, read failed!\n");
            break;
        }

        char* str = (char*)malloc(SecPerClus*BytesPerSec); // temp storage of read content
        char *content = str;

        int startByte = dataBase + (currentClus - 2) * SecPerClus * BytesPerSec;
        int check;

        check = fseek(fat12, startByte, SEEK_SET);
        if (check == -1) {
            myPrint("fseek in getContent failed!\n");
        }

        check = fread(content, 1, SecPerClus*BytesPerSec, fat12);
        if (check != SecPerClus * BytesPerSec) {
            myPrint("fread in getContent failed!\n");
        }

        int length = SecPerClus * BytesPerSec;
        for (int i = 0; i < length; i++) {
            *p = content[i];
            p++;
        }

        free(str);
        currentClus = value;
    }
}

void printCat(Node *root, string path, int & exist) {
    if (path == root->path) {
        if (root->isfile) {
            exist = 1;
            if (root->content[0] != 0) {
                // cout << root->content << endl;
                myPrint(root->content);
                myPrint("\n");
            }
            return;
        }
        else {
            exist = 2;
            return;
        }
    }
    if (path.length() <= root->path.length()) {
        return;
    }
    string temp = path.substr(0, root->path.length());
    if (temp == root->path) {
        // path matched, start recursion
        for (Node *q : root->next) {
            printCat(q, path, exist);
        }
    }
}

void myPrint(const char* p) {
    nasm_print(p, strlen(p));
    // cout << p;
}

void split(const string &s, vector<string> &sv, const char flag = ' ') {
    sv.clear();
    istringstream iss(s);
    string temp;
    while (getline(iss, temp, flag)) {
        sv.push_back(temp);
    }
}

void pathDeal(string &s) {
    if (s[0] != '/') {
        s = "/" + s;
    }
    if (s[s.length() - 1] != '/') {
        s += "/";
    }
}

string assembleStringRED(vector<string> &sv) {
    string res;
    string redFront = "\033[31m";
    string redBack = "\033[0m";
    res.append(redFront);
    res.append(sv.at(0));
    res.append(redBack);
    for(int i = 1; i < sv.size(); i++) {
        res.append(sv.at(i));
    }
    sv.clear();
    return res;
}

string assembleString(vector<string> &sv) {
    string res;
    for(int i = 0; i < sv.size(); i++) {
        res.append(sv.at(i));
    }
    sv.clear();
    return res;
}



