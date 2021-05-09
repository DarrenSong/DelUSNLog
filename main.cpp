#include<iostream>
#include<stdio.h>
#include<string>
#include<tchar.h>
#include<Windows.h>

using namespace std;

bool isNTFS(string path);
HANDLE getHandle(string volName);
bool createUSN(HANDLE hVol, CREATE_USN_JOURNAL_DATA& cujd);
bool getUSNInfo(HANDLE hVol, USN_JOURNAL_DATA& ujd);
bool getUSNJournal(HANDLE hVol, USN_JOURNAL_DATA& ujd);
bool deleteUSN(HANDLE hVol, USN_JOURNAL_DATA& ujd);

int main(){
    //isNTFS("C:/");
    CREATE_USN_JOURNAL_DATA* cujd = new CREATE_USN_JOURNAL_DATA;
    USN_JOURNAL_DATA* ujd = new USN_JOURNAL_DATA;
    HANDLE hVol = getHandle("C:");
    createUSN(hVol, *cujd);
    getUSNInfo(hVol, *ujd);
    getUSNJournal(hVol, *ujd);
    deleteUSN(hVol, *ujd);
    system("pause");
    return 0;
}

//判断是否是NTFS盘
bool isNTFS(string path){//"C:/"
    char sysNameBuf[MAX_PATH];
    int status = GetVolumeInformationA(path.c_str(),
        NULL,
        0,
        NULL,
        NULL,
        NULL,
        sysNameBuf,
        MAX_PATH);

    if (0 != status){
        if (0 == strcmp(sysNameBuf, "NTFS")){
            //printf(" 文件系统名 : %s\n", sysNameBuf);
            cout << "盘符：" << path << "\n文件系统名：" << sysNameBuf << endl;
            return true;
        }
        else {
            printf(" 该驱动盘非 NTFS 格式 \n");
            return false;
        }

    }
    return false;
}

/**
* step 02. 获取驱动盘句柄
*/
HANDLE getHandle(string volName){

    char fileName[MAX_PATH];
    fileName[0] = '\0';

    // 传入的文件名必须为\\.\C:的形式  
    strcpy_s(fileName, "\\\\.\\");
    strcat_s(fileName, volName.c_str());
    // 为了方便操作，这里转为string进行去尾  
    string fileNameStr = (string)fileName;
    fileNameStr.erase(fileNameStr.find_last_of(":") + 1);

    printf("驱动盘地址: %s\n", fileNameStr.data());

    // 调用该函数需要管理员权限  
    HANDLE hVol = CreateFileA(fileNameStr.data(),
        GENERIC_READ | GENERIC_WRITE, // 可以为0  
        FILE_SHARE_READ | FILE_SHARE_WRITE, // 必须包含有FILE_SHARE_WRITE  
        NULL, // 这里不需要  
        OPEN_EXISTING, // 必须包含OPEN_EXISTING, CREATE_ALWAYS可能会导致错误  
        FILE_ATTRIBUTE_READONLY, // FILE_ATTRIBUTE_NORMAL可能会导致错误  
        NULL); // 这里不需要  

    if (INVALID_HANDLE_VALUE != hVol){
        //getHandleSuccess = true;
        cout << "获取驱动盘句柄成功！\n";
        return hVol;
    }
    else{
        printf("获取驱动盘句柄失败 —— handle:%x error:%d\n", hVol, GetLastError());
        return 0;
    }
    return 0;
}

/**
* step 03. 初始化USN日志文件
*/
bool createUSN(HANDLE hVol, CREATE_USN_JOURNAL_DATA& cujd){
    DWORD br;    
    cujd.MaximumSize = 0; // 0表示使用默认值  
    cujd.AllocationDelta = 0; // 0表示使用默认值  
    bool status = DeviceIoControl(hVol,
        FSCTL_CREATE_USN_JOURNAL,
        &cujd,
        sizeof(cujd),
        NULL,
        0,
        &br,
        NULL);

    if (0 != status){
        //initUsnJournalSuccess = true;
        return true;
    }
    else{
        printf("初始化USN日志文件失败 —— status:%x error:%d\n", status, GetLastError());
        return false;
    }
    return false;
}

/**

* step 04. 获取USN日志基本信息(用于后续操作)

* msdn:http://msdn.microsoft.com/en-us/library/aa364583%28v=VS.85%29.aspx

*/
bool getUSNInfo(HANDLE hVol, USN_JOURNAL_DATA& ujd){
    bool getBasicInfoSuccess = false;
    DWORD br;
    bool status = DeviceIoControl(hVol,
        FSCTL_QUERY_USN_JOURNAL,
        NULL,
        0,
        &ujd,
        sizeof(ujd),
        &br,
        NULL);
    if (0 != status){
        //getBasicInfoSuccess = true;
        printf("获取USN日志基本信息成功\n");
        return true;
    }
    else{
        printf("获取USN日志基本信息失败 —— status:%x error:%d\n", status, GetLastError());
        return false;
    }
    return false;
}

bool getUSNJournal(HANDLE hVol, USN_JOURNAL_DATA& ujd){
    MFT_ENUM_DATA_V0 med;
    med.StartFileReferenceNumber = 0;
    med.LowUsn = ujd.FirstUsn;
    med.HighUsn = ujd.NextUsn;
#define BUF_LEN 4096  
    CHAR buffer[BUF_LEN]; // 用于储存记录的缓冲 , 尽量足够地大  
    DWORD usnDataSize = 0;
    PUSN_RECORD UsnRecord;
    while (0 != DeviceIoControl(hVol,
        FSCTL_ENUM_USN_DATA,
        &med,
        sizeof (med),
        buffer,
        BUF_LEN,
        &usnDataSize,
        NULL))
    {
        DWORD dwRetBytes = usnDataSize - sizeof (USN);
        // 找到第一个 USN 记录  
        // from MSDN(http://msdn.microsoft.com/en-us/library/aa365736%28v=VS.85%29.aspx ):  
        // return a USN followed by zero or more change journal records, each in a USN_RECORD structure.  
        UsnRecord = (PUSN_RECORD)(((PCHAR)buffer) + sizeof (USN));
        printf(" ********************************** \n");
        while (dwRetBytes>0){
            // 打印获取到的信息  
            const int strLen = UsnRecord->FileNameLength;
            char fileName[MAX_PATH] = { 0 };
            WideCharToMultiByte(CP_OEMCP, NULL, UsnRecord->FileName, strLen / 2, fileName, strLen, NULL, FALSE);
            printf("FileName: %s\n", fileName);
            // 下面两个 file reference number 可以用来获取文件的路径信息  
            printf("FileReferenceNumber: %xI64\n", UsnRecord->FileReferenceNumber);
            printf("ParentFileReferenceNumber: %xI64\n", UsnRecord->ParentFileReferenceNumber);
            printf("\n");
            // 获取下一个记录  
            DWORD recordLen = UsnRecord->RecordLength;
            dwRetBytes -= recordLen;
            UsnRecord = (PUSN_RECORD)(((PCHAR)UsnRecord) + recordLen);
        }
        // 获取下一页数据， MTF 大概是分多页来储存的吧？  
        // from MSDN(http://msdn.microsoft.com/en-us/library/aa365736%28v=VS.85%29.aspx ):  
        // The USN returned as the first item in the output buffer is the USN of the next record number to be retrieved.  
        // Use this value to continue reading records from the end boundary forward.  
        med.StartFileReferenceNumber = *(USN *)&buffer;
    }
    return true;
}

/**
* step 06. 删除 USN 日志文件 ( 当然也可以不删除 )
*/
bool deleteUSN(HANDLE hVol, USN_JOURNAL_DATA& ujd){
    DELETE_USN_JOURNAL_DATA dujd;
    dujd.UsnJournalID = ujd.UsnJournalID;
    dujd.DeleteFlags = USN_DELETE_FLAG_DELETE;
    DWORD br;
    int status = DeviceIoControl(hVol,
        FSCTL_DELETE_USN_JOURNAL,
        &dujd,
        sizeof (dujd),
        NULL,
        0,
        &br,
        NULL);
    if (0 != status){
        CloseHandle(hVol);
        printf(" 成功删除 USN 日志文件 !\n");
        return true;
    }
    else {
        CloseHandle(hVol);
        printf(" 删除 USN 日志文件失败 —— status:%x error:%d\n", status, GetLastError());
        return false;
    }
    return false;
}
